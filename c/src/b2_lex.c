/* b2_lex.c - program loading, variable slots and the tokenizer */

static int ch_at(const char *t, long n, long j)
{
    if (j < 1 || j > n) return 0;
    return (unsigned char)t[j - 1];
}

static void init_words(void)
{
    strcpy(kws, " PRINT LET IF THEN ELSE ELSEIF ENDIF FOR TO STEP NEXT WHILE WEND DO LOOP UNTIL GOTO GOSUB");
    strcat(kws, " RETURN ON ONERROR END STOP SYSTEM DIM REDIM ERASE DATA READ RESTORE DEF SWAP RANDOMIZE");
    strcat(kws, " CLS LOCATE COLOR BEEP SLEEP OPEN CLOSE INPUT LINEINPUT WRITE CONST SUB FUNCTION ENDSUB");
    strcat(kws, " ENDFUNCTION EXITFOR EXITDO EXITWHILE EXITSUB EXITFUNCTION EXITSELECT CONTFOR CONTDO");
    strcat(kws, " CONTWHILE SELECTCASE CASE CASEELSE ENDSELECT CALL DECLARE SHARED AS BYVAL BYREF USING");
    strcat(kws, " OPTION RESUME ERROR WIDTH SCREEN STATIC COMMON LOCAL SHELL KILL IS PRESERVE VIEW PALETTE");
    strcat(kws, " DEFINT DEFSNG DEFDBL DEFSTR DEFLNG CLEAR LSET RSET ");
    strcpy(fnl, " ABS ASC ATN CDBL CHR CINT CLNG COS CSNG EXP FIX HEX INT LEFT LEN LOG LTRIM MID OCT RIGHT");
    strcat(fnl, " RND RTRIM SGN SIN SPACE SQR STR STRING TAN UCASE LCASE VAL INSTR TIMER DATE TIME INKEY");
    strcat(fnl, " INPUT EOF LOF FREEFILE ERR ERL FRE POS CSRLIN LBOUND UBOUND ASIN ACOS ATAN2 SINH COSH");
    strcat(fnl, " TANH LOG10 LOG2 TRIM INSTRREV IIF VALINT VALLNG COMMAND ENVIRON CURDIR FRAC CBOOL PEEK");
    strcat(fnl, " TAB SPC FORMAT BIN SQRT CINT ENVIRON MKI CVI LOC SEEK ATAN POW MAX MIN ISNUMERIC ");
}

static void init_state(void)
{
    long i;
    init_words();
    memset(&lmap, 0, sizeof(lmap));
    memset(&nm, 0, sizeof(nm));
    for (i = 0; i <= NFILES; i++) {
        fl_fp[i] = NULL;
        fl_mode[i] = 0;
        fl_eof[i] = 0;
        fl_has[i] = 0;
    }
    dptr = 1;
    pc = 1;
    stmt_pc = 1;
    mstmt = 1;
    res_pc = 1;
    rnd_state = 1013904223.0;
    rnd_last = 0.5;
    out_col = 0;
    for (i = 0; i <= MAXPREC; i++) prec[i] = 0;
    prec[1] = 9;
    prec[2] = 9;
    prec[3] = 13;
    prec[4] = 13;
    prec[5] = 12;
    prec[6] = 15;
    prec[12] = 7;
    prec[13] = 7;
    prec[14] = 7;
    prec[15] = 7;
    prec[16] = 7;
    prec[17] = 7;
    prec[19] = 8;
    prec[31] = 5;
    prec[32] = 4;
    prec[33] = 3;
    prec[34] = 2;
    prec[35] = 1;
    prec[36] = 11;
    prec[37] = 10;
    prec[38] = 10;
}

static void reset_program(void)
{
    long i;
    ntk = 0;
    nlines = 0;
    hm_clear(&lmap);
    hm_clear(&nm);
    for (i = 1; i <= nvars; i++) {
        free(vars[i].name);
        vars[i].name = NULL;
    }
    nvars = 0;
    npn = 0;
    nps = 0;
    np = 0;
    npp = 0;
    cur_proc = 0;
    nfn = 0;
    nd = 0;
    dptr = 1;
}

/* read the program text; returns 1 on success */
static int load_program(const char *fn)
{
    FILE *f;
    Str line;
    int c, eof;
    memset(&line, 0, sizeof(line));
    f = fopen(fn, "r");
    if (f == NULL) {
        printf("basic: cannot open %s\n", fn);
        return 0;
    }
    reset_program();
    for (;;) {
        s_clr(&line);
        eof = 0;
        for (;;) {
            c = getc(f);
            if (c == EOF) {
                eof = 1;
                break;
            }
            if (c == '\n') break;
            s_catc(&line, c);
        }
        if (eof && line.n == 0) break;
        add_line(SP(line), line.n);
    }
    fclose(f);
    s_free(&line);
    return 1;
}

static void add_line(const char *txt, long n)
{
    if (n > 0 && txt[n - 1] == '\r') n--;
    nlines++;
    NEED(ln_num, ln_cap, nlines + 1);
    NEED(ln_start, ln_scap, nlines + 1);
    tokenize_line(txt, n, nlines);
}

/* next alphabetic word at or after position p0 (blanks skipped), upper case; sets pw_end */
static void peekword(const char *txt, long n, long p0, Str *out)
{
    long j, k;
    j = p0;
    while (j <= n && (ch_at(txt, n, j) == ' ' || ch_at(txt, n, j) == '\t')) j++;
    k = j;
    while (k <= n && is_letter(ch_at(txt, n, k))) k++;
    pw_end = k;
    s_set(out, txt + j - 1, k - j);
    up_str(out);
}

static int peekword_is(const char *txt, long n, long p0, const char *w)
{
    Str t;
    int r;
    memset(&t, 0, sizeof(t));
    peekword(txt, n, p0, &t);
    r = s_eq(&t, w);
    s_free(&t);
    return r;
}

/* next non-blank character at or after p0 */
static int peekchar(const char *txt, long n, long p0)
{
    long j;
    j = p0;
    while (j <= n && (ch_at(txt, n, j) == ' ' || ch_at(txt, n, j) == '\t')) j++;
    return ch_at(txt, n, j);
}

/* compound keywords: END IF, EXIT FOR, ...; returns a static string or "" */
static const char *compound(const char *u, const char *txt, long n, long p0)
{
    Str nw;
    const char *r;
    memset(&nw, 0, sizeof(nw));
    peekword(txt, n, p0, &nw);
    r = "";
    if (nw.n == 0) {
        s_free(&nw);
        return "";
    }
    if (strcmp(u, "END") == 0) {
        if (s_eq(&nw, "IF")) r = "ENDIF";
        else if (s_eq(&nw, "SUB")) r = "ENDSUB";
        else if (s_eq(&nw, "FUNCTION")) r = "ENDFUNCTION";
        else if (s_eq(&nw, "SELECT")) r = "ENDSELECT";
    } else if (strcmp(u, "EXIT") == 0) {
        if (s_eq(&nw, "FOR")) r = "EXITFOR";
        else if (s_eq(&nw, "DO")) r = "EXITDO";
        else if (s_eq(&nw, "WHILE")) r = "EXITWHILE";
        else if (s_eq(&nw, "SUB")) r = "EXITSUB";
        else if (s_eq(&nw, "FUNCTION")) r = "EXITFUNCTION";
        else if (s_eq(&nw, "SELECT")) r = "EXITSELECT";
    } else if (strcmp(u, "CONTINUE") == 0) {
        if (s_eq(&nw, "FOR")) r = "CONTFOR";
        else if (s_eq(&nw, "DO")) r = "CONTDO";
        else if (s_eq(&nw, "WHILE")) r = "CONTWHILE";
    } else if (strcmp(u, "SELECT") == 0) {
        if (s_eq(&nw, "CASE")) r = "SELECTCASE";
    } else if (strcmp(u, "CASE") == 0) {
        if (s_eq(&nw, "ELSE")) r = "CASEELSE";
    } else if (strcmp(u, "LINE") == 0) {
        if (s_eq(&nw, "INPUT")) r = "LINEINPUT";
    } else if (strcmp(u, "GO") == 0) {
        if (s_eq(&nw, "TO")) r = "GOTO";
        else if (s_eq(&nw, "SUB")) r = "GOSUB";
    } else if (strcmp(u, "ON") == 0) {
        if (s_eq(&nw, "ERROR")) r = "ONERROR";
    }
    if (r[0] == '\0') pw_end = p0;
    s_free(&nw);
    return r;
}

static int typekind(const Str *w)
{
    if (s_eq(w, "STRING") || s_eq(w, "ZSTRING") || s_eq(w, "WSTRING")) return 2;
    if (s_eq(w, "INTEGER") || s_eq(w, "LONG") || s_eq(w, "SHORT") || s_eq(w, "BYTE") || s_eq(w, "UBYTE")) return 1;
    if (s_eq(w, "USHORT") || s_eq(w, "UINTEGER") || s_eq(w, "ULONG") || s_eq(w, "LONGINT") ||
        s_eq(w, "ULONGINT") || s_eq(w, "BOOLEAN")) return 1;
    return 0;
}

static long opcode(const char *t)
{
    if (strcmp(t, "+") == 0) return 1;
    if (strcmp(t, "-") == 0) return 2;
    if (strcmp(t, "*") == 0) return 3;
    if (strcmp(t, "/") == 0) return 4;
    if (strcmp(t, "\\") == 0) return 5;
    if (strcmp(t, "^") == 0) return 6;
    if (strcmp(t, "(") == 0) return 7;
    if (strcmp(t, ")") == 0) return 8;
    if (strcmp(t, ",") == 0) return 9;
    if (strcmp(t, ";") == 0) return 10;
    if (strcmp(t, ":") == 0) return 11;
    if (strcmp(t, "=") == 0) return 12;
    if (strcmp(t, "<") == 0) return 13;
    if (strcmp(t, ">") == 0) return 14;
    if (strcmp(t, "<=") == 0) return 15;
    if (strcmp(t, ">=") == 0) return 16;
    if (strcmp(t, "<>") == 0) return 17;
    if (strcmp(t, "#") == 0) return 18;
    if (strcmp(t, "&") == 0) return 19;
    if (strcmp(t, "AND") == 0) return 31;
    if (strcmp(t, "OR") == 0) return 32;
    if (strcmp(t, "XOR") == 0) return 33;
    if (strcmp(t, "EQV") == 0) return 34;
    if (strcmp(t, "IMP") == 0) return 35;
    if (strcmp(t, "MOD") == 0) return 36;
    if (strcmp(t, "SHL") == 0) return 37;
    if (strcmp(t, "SHR") == 0) return 38;
    if (strcmp(t, "NOT") == 0) return 39;
    return 0;
}

/* ---------- variable slots ---------- */
static int namekind(const char *nme)
{
    long l;
    int sx, li;
    l = (long)strlen(nme);
    if (l > 0 && nme[l - 1] == '(') l--;
    sx = l > 0 ? (unsigned char)nme[l - 1] : 0;
    if (sx == '$') return 2;
    if (sx == '%' || sx == '&') return 1;
    if (sx == '!' || sx == '#') return 0;
    li = l > 0 ? letter_index((unsigned char)nme[0]) : 0;
    if (li >= 1 && li <= 26) return defkind[li];
    return 0;
}

static long newslot(const char *key, const char *nme, long owner)
{
    long s0, j;
    Var *v;
    nvars++;
    NEED(vars, vars_cap, nvars + 1);
    s0 = nvars;
    v = &vars[s0];
    v->name = xstrdup(nme, (long)strlen(nme));
    v->kind = namekind(nme);
    v->n = 0.0;
    s_clr(&v->s);
    v->isarr = 0;
    v->nd = 0;
    v->base = 0;
    v->proc = owner;
    v->cnst = 0;
    v->shared = 0;
    v->stat = 0;
    for (j = 0; j < 3; j++) {
        v->lo[j] = 0;
        v->hi[j] = 0;
    }
    hm_set(&nm, key, s0);
    return s0;
}

/* sm: 0 normal, 1 shared, 2 static;  ak: forced kind or -1 */
static long getslot(const char *key, int sm, int ak)
{
    Str lk;
    long v, s0, g0;
    memset(&lk, 0, sizeof(lk));
    if (cur_proc > 0) {
        s_cats(&lk, procs[cur_proc].name);
        s_catc(&lk, ':');
        s_cats(&lk, key);
        v = hm_get(&nm, SP(lk));
        if (v != 0) {
            s0 = v;
            if (ak >= 0) vars[s0].kind = ak;
            s_free(&lk);
            return s0;
        }
        v = hm_get(&nm, key);
        if (v != 0) {
            g0 = v;
            if (vars[g0].shared == 1) {
                s_free(&lk);
                return g0;
            }
        }
        if (sm == 1) {
            if (v == 0) g0 = newslot(key, key, 0);
            else g0 = v;
            vars[g0].shared = 1;
            s_free(&lk);
            return g0;
        }
        s0 = newslot(SP(lk), key, cur_proc);
        if (sm == 2) vars[s0].stat = 1;
        if (ak >= 0) vars[s0].kind = ak;
        s_free(&lk);
        return s0;
    }
    v = hm_get(&nm, key);
    if (v != 0) s0 = v;
    else s0 = newslot(key, key, 0);
    if (sm == 1) vars[s0].shared = 1;
    if (ak >= 0) vars[s0].kind = ak;
    return s0;
}

static void emit(int ty, double nv, const char *sv, long sl, long kv, long fv, long li)
{
    Tok *t;
    ntk++;
    NEED(tk, tk_cap, ntk + 12);
    t = &tk[ntk];
    t->t = ty;
    t->num = nv;
    if (t->s != NULL) free(t->s);
    t->s = xstrdup(sv, sl);
    t->sl = sl;
    t->k = kv;
    t->f = fv;
    t->x = 0;
    t->y = 0;
    t->l = li;
}

static void emitz(int ty, double nv, const char *sv, long kv, long fv, long li)
{
    emit(ty, nv, sv, (long)strlen(sv), kv, fv, li);
}

/* ---------- the tokenizer: one source line -> tokens ---------- */
static void tokenize_line(const char *txt, long n, long li)
{
    long i, j, k, slot, ls, depth, lastkwpos, kd;
    int c, c2, e, sx, atstart, smode, asknd, want_proc, inhdr, isarr;
    Str w, u, bu, lastkw, nx;
    char cb[3];
    double nv;
    long p;
    const char *cw;

    memset(&w, 0, sizeof(w));
    memset(&u, 0, sizeof(u));
    memset(&bu, 0, sizeof(bu));
    memset(&lastkw, 0, sizeof(lastkw));
    memset(&nx, 0, sizeof(nx));
    i = 1;
    ln_start[li] = ntk + 1;
    ln_num[li] = -1;
    ls = 0;
    depth = 0;
    atstart = 1;
    smode = 0;
    asknd = -1;
    want_proc = 0;
    lastkwpos = 0;
    inhdr = 0;
    while (i <= n && (ch_at(txt, n, i) == ' ' || ch_at(txt, n, i) == '\t')) i++;
    j = i;
    while (j <= n && is_digit(ch_at(txt, n, j))) j++;
    if (j > i) {
        double dv;
        char nb[64];
        long nl;
        nl = j - i;
        if (nl > 40) nl = 40;
        memcpy(nb, txt + i - 1, (size_t)nl);
        nb[nl] = '\0';
        dv = strtod(nb, NULL);
        k = dv > 2000000000.0 ? 2000000000L : (long)dv;
        ln_num[li] = k;
        sprintf(nb, "%ld", k);
        hm_set(&lmap, nb, li);
        i = j;
        atstart = 1;
    } else if (ch_at(txt, n, i) == '#') {
        i = n + 1;
    }
    while (i <= n) {
        c = ch_at(txt, n, i);
        if (c == ' ' || c == '\t') {
            i++;
            continue;
        }
        if (c == '\'') break;
        c2 = ch_at(txt, n, i + 1);
        /* ---------- numbers ---------- */
        if (is_digit(c) || (c == '.' && is_digit(c2))) {
            j = i;
            while (j <= n && is_digit(ch_at(txt, n, j))) j++;
            if (j <= n && ch_at(txt, n, j) == '.') {
                j++;
                while (j <= n && is_digit(ch_at(txt, n, j))) j++;
            }
            if (j <= n) {
                e = to_up(ch_at(txt, n, j));
                if (e == 'E' || e == 'D') {
                    k = j + 1;
                    if (k <= n && (ch_at(txt, n, k) == '+' || ch_at(txt, n, k) == '-')) k++;
                    if (k <= n && is_digit(ch_at(txt, n, k))) {
                        j = k;
                        while (j <= n && is_digit(ch_at(txt, n, j))) j++;
                    }
                }
            }
            s_set(&w, txt + i - 1, j - i);
            up_str(&w);
            for (k = 0; k < w.n; k++)
                if (w.p[k] == 'D') w.p[k] = 'E';
            nv = numtofloat(SP(w));
            i = j;
            sx = ch_at(txt, n, i);
            if (sx == '%' || sx == '!' || sx == '#') i++;
            else if (sx == '&' && !is_alnum(ch_at(txt, n, i + 1))) i++;
            emit(1, nv, "", 0, 0, 0, li);
            atstart = 0;
            continue;
        }
        /* ---------- &H &O &B literals ---------- */
        if (c == '&') {
            sx = to_up(c2);
            if (sx == 'H' || sx == 'O' || sx == 'B') {
                int base;
                j = i + 2;
                while (j <= n && is_alnum(ch_at(txt, n, j))) j++;
                s_set(&w, txt + i + 1, j - i - 2);
                up_str(&w);
                if (w.n > 0 && w.p[w.n - 1] == '&') w.n--, w.p[w.n] = '\0';
                if (w.n == 0) s_set(&w, "0", 1);
                base = sx == 'H' ? 16 : (sx == 'B' ? 2 : 8);
                nv = 0.0;
                for (k = 0; k < w.n; k++) {
                    p = (long)(strchr("0123456789ABCDEF", w.p[k]) ? strchr("0123456789ABCDEF", w.p[k]) - "0123456789ABCDEF" : 99);
                    if (p >= base) break;
                    nv = nv * base + (double)p;
                }
                i = j;
                if (ch_at(txt, n, i) == '&') i++;
                emit(1, nv, "", 0, 0, 0, li);
                atstart = 0;
                continue;
            }
        }
        /* ---------- strings ---------- */
        if (c == '"') {
            s_clr(&w);
            j = i + 1;
            for (;;) {
                if (j > n) break;
                e = ch_at(txt, n, j);
                if (e == '"') {
                    if (ch_at(txt, n, j + 1) == '"') {
                        s_catc(&w, '"');
                        j += 2;
                        continue;
                    }
                    break;
                }
                s_catc(&w, e);
                j++;
            }
            i = j + 1;
            emit(2, 0.0, SP(w), w.n, 0, 0, li);
            atstart = 0;
            continue;
        }
        /* ---------- identifiers and keywords ---------- */
        if (is_letter(c)) {
            j = i;
            while (j <= n && is_alnum(ch_at(txt, n, j))) j++;
            s_set(&w, txt + i - 1, j - i);
            sx = ch_at(txt, n, j);
            if (sx == '$' || sx == '%' || sx == '!' || sx == '#') {
                s_catc(&w, sx);
                j++;
            } else if (sx == '&' && ch_at(txt, n, j + 1) != '&') {
                if (!is_alnum(peekchar(txt, n, j + 1)) && peekchar(txt, n, j + 1) != '"' &&
                    peekchar(txt, n, j + 1) != '(') {
                    s_catc(&w, sx);
                    j++;
                }
            }
            i = j;
            s_cpy(&u, &w);
            up_str(&u);
            if (s_eq(&u, "REM")) break;
            /* label? */
            if (atstart == 1 && ch_at(txt, n, i) == ':' && wordpos(SP(u), kws) == 0) {
                hm_set(&lmap, SP(u), li);
                i++;
                continue;
            }
            atstart = 0;
            /* compound keywords */
            if (s_eq(&u, "END") || s_eq(&u, "EXIT") || s_eq(&u, "CONTINUE") || s_eq(&u, "SELECT") ||
                s_eq(&u, "CASE") || s_eq(&u, "LINE") || s_eq(&u, "GO") || s_eq(&u, "ON")) {
                cw = compound(SP(u), txt, n, i);
                if (cw[0] != '\0') {
                    s_set(&u, cw, (long)strlen(cw));
                    i = pw_end;
                }
            }
            /* operator words */
            kd = opcode(SP(u));
            if (kd >= 31) {
                emit(5, 0.0, SP(u), u.n, kd, 0, li);
                continue;
            }
            /* statement keywords */
            if (wordpos(SP(u), kws) > 0) {
                if (s_eq(&u, "DECLARE")) {
                    i = n + 1;
                    continue;
                }
                if (s_eq(&u, "DEFINT") || s_eq(&u, "DEFSNG") || s_eq(&u, "DEFDBL") || s_eq(&u, "DEFSTR") ||
                    s_eq(&u, "DEFLNG")) {
                    do_defxxx(SP(u), txt + i - 1, n - i + 1);
                    i = n + 1;
                    continue;
                }
                if (s_eq(&u, "AS") && !s_eq(&lastkw, "OPEN")) {
                    peekword(txt, n, i, &nx);
                    if (s_eq(&nx, "UNSIGNED")) {
                        i = pw_end;
                        peekword(txt, n, i, &nx);
                    }
                    if (nx.n > 0) {
                        i = pw_end;
                        kd = typekind(&nx);
                        if (s_eq(&nx, "LONG") && peekword_is(txt, n, i, "LONG")) i = pw_end;
                        if ((s_eq(&nx, "STRING") || s_eq(&nx, "ZSTRING")) && peekchar(txt, n, i) == '*') {
                            j = i;
                            while (j <= n && ch_at(txt, n, j) != '*') j++;
                            j++;
                            while (j <= n && (ch_at(txt, n, j) == ' ' || is_digit(ch_at(txt, n, j)))) j++;
                            i = j;
                        }
                        if (peekword_is(txt, n, i, "PTR")) i = pw_end;
                        emitz(4, 0.0, "AS", wordpos("AS", kws), 0, li);
                        emit(8, 0.0, SP(nx), nx.n, kd, 0, li);
                        if (lastkwpos == ntk - 2 &&
                            (s_eq(&lastkw, "DIM") || s_eq(&lastkw, "REDIM") || s_eq(&lastkw, "SHARED") ||
                             s_eq(&lastkw, "STATIC") || s_eq(&lastkw, "COMMON") || s_eq(&lastkw, "LOCAL") ||
                             s_eq(&lastkw, "CONST")))
                            asknd = (int)kd;
                        else if (ls > 0) vars[ls].kind = (int)kd;
                        continue;
                    }
                }
                if (s_eq(&u, "SHARED")) smode = 1;
                if (s_eq(&u, "STATIC") && cur_proc > 0) smode = 2;
                if (s_eq(&u, "SUB") && cur_proc == 0) want_proc = 1;
                if (s_eq(&u, "CONST") && cur_proc == 0) smode = 1;
                if (s_eq(&u, "FUNCTION") && cur_proc == 0) want_proc = 2;
                if (s_eq(&u, "ENDSUB") || s_eq(&u, "ENDFUNCTION")) {
                    emit(4, 0.0, SP(u), u.n, wordpos(SP(u), kws), 0, li);
                    cur_proc = 0;
                    continue;
                }
                emit(4, 0.0, SP(u), u.n, wordpos(SP(u), kws), 0, li);
                s_cpy(&lastkw, &u);
                lastkwpos = ntk;
                if (s_eq(&u, "DATA")) {
                    i = capture_data(txt, n, i, li);
                    continue;
                }
                continue;
            }
            /* builtin functions */
            s_cpy(&bu, &u);
            if (bu.n > 0 && bu.p[bu.n - 1] == '$') {
                bu.n--;
                bu.p[bu.n] = '\0';
            }
            if (want_proc == 0) {
                long fi;
                fi = wordpos(SP(bu), fnl);
                if (fi > 0 && peekchar(txt, n, i) != '(' && wordpos(SP(bu), nopar) == 0) fi = 0;
                if (fi > 0) {
                    emit(3, 0.0, SP(bu), bu.n, 0, fi, li);
                    continue;
                }
            }
            /* identifier */
            isarr = peekchar(txt, n, i) == '(';
            if (want_proc > 0) {
                np++;
                NEED(procs, np_cap, np + 1);
                procs[np].name = xstrdup(SP(u), u.n);
                procs[np].isfn = want_proc - 1;
                procs[np].pc = ntk;
                procs[np].body = 0;
                procs[np].end = 0;
                procs[np].ret = 0;
                procs[np].pfirst = 0;
                procs[np].pcnt = 0;
                cur_proc = np;
                if (want_proc == 2) {
                    slot = getslot(SP(u), 0, -1);
                    procs[np].ret = slot;
                    ls = slot;
                } else slot = 0;
                want_proc = 0;
                inhdr = 1;
                emit(3, 0.0, SP(u), u.n, slot, 0, li);
                continue;
            }
            if (isarr) {
                Str kk;
                memset(&kk, 0, sizeof(kk));
                s_cpy(&kk, &u);
                s_catc(&kk, '(');
                slot = getslot(SP(kk), smode, asknd);
                s_free(&kk);
            } else slot = getslot(SP(u), smode, asknd);
            if (depth == 0 || (inhdr == 1 && depth == 1)) ls = slot;
            emit(3, 0.0, SP(u), u.n, slot, 0, li);
            tk[ntk].x = isarr;
            continue;
        }
        /* ---------- operators and punctuation ---------- */
        if ((c == '<' && c2 == '=') || (c == '=' && c2 == '<')) {
            emitz(5, 0.0, "<=", 15, 0, li);
            i += 2;
        } else if ((c == '>' && c2 == '=') || (c == '=' && c2 == '>')) {
            emitz(5, 0.0, ">=", 16, 0, li);
            i += 2;
        } else if ((c == '<' && c2 == '>') || (c == '>' && c2 == '<')) {
            emitz(5, 0.0, "<>", 17, 0, li);
            i += 2;
        } else if (c == '?') {
            emitz(4, 0.0, "PRINT", wordpos("PRINT", kws), 0, li);
            s_set(&lastkw, "PRINT", 5);
            i++;
        } else {
            cb[0] = (char)c;
            cb[1] = '\0';
            kd = opcode(cb);
            if (kd == 7) depth++;
            if (kd == 8) {
                depth--;
                if (depth == 0 && inhdr == 1) {
                    if (procs[cur_proc].isfn == 1) ls = procs[cur_proc].ret;
                    else ls = 0;
                }
            }
            if (kd == 11) {
                smode = 0;
                asknd = -1;
                s_clr(&lastkw);
            }
            emit(5, 0.0, cb, 1, kd, 0, li);
            i++;
        }
    }
    emitz(6, 0.0, "", 0, 0, li);
    s_free(&w);
    s_free(&u);
    s_free(&bu);
    s_free(&lastkw);
    s_free(&nx);
}

static void do_defxxx(const char *kw, const char *rest, long m)
{
    long i, a, b;
    int kd, c, li;
    if (strcmp(kw, "DEFSTR") == 0) kd = 2;
    else if (strcmp(kw, "DEFINT") == 0 || strcmp(kw, "DEFLNG") == 0) kd = 1;
    else kd = 0;
    i = 1;
    while (i <= m) {
        c = ch_at(rest, m, i);
        li = letter_index(c);
        if (li > 0 && !is_digit(c) && c != '_') {
            a = li;
            b = a;
            i++;
            if (ch_at(rest, m, i) == '-') {
                c = ch_at(rest, m, i + 1);
                if (letter_index(c) > 0) b = letter_index(c);
                i += 2;
            }
            while (a <= b) {
                defkind[a] = kd;
                a++;
            }
        } else i++;
    }
}

static void add_data(const char *w, long wl, long q, long li)
{
    nd++;
    NEED(dat, nd_cap, nd + 1);
    if (dat[nd].s != NULL) free(dat[nd].s);
    dat[nd].s = xstrdup(w, wl);
    dat[nd].sl = wl;
    dat[nd].q = q;
    dat[nd].line = li;
}

/* DATA items: captured at tokenize time; returns the position after the DATA text */
static long capture_data(const char *txt, long n, long p0, long li)
{
    long i, q;
    int c, inq;
    Str w, t;
    memset(&w, 0, sizeof(w));
    memset(&t, 0, sizeof(t));
    i = p0;
    while (i <= n && (ch_at(txt, n, i) == ' ' || ch_at(txt, n, i) == '\t')) i++;
    q = 0;
    inq = 0;
    while (i <= n) {
        c = ch_at(txt, n, i);
        if (inq == 1) {
            if (c == '"') inq = 0;
            else s_catc(&w, c);
            i++;
            continue;
        }
        if (c == '"') {
            inq = 1;
            q = 1;
            i++;
            continue;
        }
        if (c == ',') {
            if (q == 0) {
                strip_into(&t, SP(w), w.n, 'B', ' ');
                add_data(SP(t), t.n, q, li);
            } else add_data(SP(w), w.n, q, li);
            s_clr(&w);
            q = 0;
            i++;
            while (i <= n && (ch_at(txt, n, i) == ' ' || ch_at(txt, n, i) == '\t')) i++;
            continue;
        }
        if (c == ':') break;
        if (c == '\'' && q == 0) {
            i = n + 1;
            break;
        }
        s_catc(&w, c);
        i++;
    }
    if (q == 0) {
        strip_into(&t, SP(w), w.n, 'B', ' ');
        add_data(SP(t), t.n, q, li);
    } else add_data(SP(w), w.n, q, li);
    s_free(&w);
    s_free(&t);
    return i;
}
