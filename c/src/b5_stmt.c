/* b5_stmt.c - files, output, input, and the statement executor */

/* seconds since local midnight (fractional with BASIC_POSIX) */
static double clock_seconds(void)
{
    time_t tt;
    struct tm *lt;
    double r;
    tt = time(NULL);
    lt = localtime(&tt);
    r = 0.0;
    if (lt != NULL) r = (double)(lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec);
#ifdef BASIC_POSIX
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        r = r + (double)tv.tv_usec / 1000000.0;
    }
#endif
    return r;
}

/* read one line (without the newline); returns 1 when the end of the file was hit */
static int read_line(FILE *f, Str *line)
{
    int c;
    s_clr(line);
    for (;;) {
        c = getc(f);
        if (c == EOF) return 1;
        if (c == '\n') return 0;
        s_catc(line, c);
    }
}

/* EOF(n): -1 when at end of file, else 0 (reads one line ahead) */
static long file_eof(long n)
{
    int e;
    Str line;
    if (n < 1 || n > NFILES) {
        rt_error(52, "Bad file number");
        return 0;
    }
    if (fl_mode[n] != 1) {
        rt_error(54, "Bad file mode");
        return 0;
    }
    if (fl_has[n] == 1) return 0;
    if (fl_eof[n] == 1) return -1;
    memset(&line, 0, sizeof(line));
    e = read_line(fl_fp[n], &line);
    if (e > 0 && line.n == 0) {
        fl_eof[n] = 1;
        s_free(&line);
        return -1;
    }
    if (line.n > 0 && line.p[line.n - 1] == '\r') line.n--, line.p[line.n] = '\0';
    s_cpy(&fl_pend[n], &line);
    fl_has[n] = 1;
    s_free(&line);
    return 0;
}

/* ---------- statement helpers ---------- */
static int at_end(void)
{
    int t;
    t = tk[pc].t;
    if (t == 6 || t == 7) return 1;
    if (t == 5 && tk[pc].k == 11) return 1;
    if (t == 4 && tks(pc, "ELSE")) return 1;
    return 0;
}

static double num_expr(void)
{
    ex_prec(1);
    if (rt == 1) {
        rt_error(13, "Type mismatch");
        return 0.0;
    }
    return rn;
}

static long int_expr(void)
{
    double x;
    x = num_expr();
    if (err_flag == 1) return 0;
    return tol(x);
}

/* evaluate a string expression into d; returns 0 after an error */
static int str_expr(Str *d)
{
    ex_prec(1);
    if (rt != 1) {
        rt_error(13, "Type mismatch");
        s_clr(d);
        return 0;
    }
    s_cpy(d, &rs);
    return 1;
}

static int is_op(long k)
{
    return tk[pc].t == 5 && tk[pc].k == k;
}

static int is_kw(const char *w)
{
    return tk[pc].t == 4 && tks(pc, w);
}

static const char *errmsg(long c)
{
    switch (c) {
    case 1: return "NEXT without FOR";
    case 2: return "Syntax error";
    case 3: return "RETURN without GOSUB";
    case 4: return "Out of DATA";
    case 5: return "Illegal function call";
    case 6: return "Overflow";
    case 7: return "Out of memory";
    case 8: return "Undefined line number";
    case 9: return "Subscript out of range";
    case 10: return "Duplicate definition";
    case 11: return "Division by zero";
    case 13: return "Type mismatch";
    case 20: return "RESUME without error";
    case 52: return "Bad file name or number";
    case 53: return "File not found";
    case 54: return "Bad file mode";
    case 55: return "File already open";
    case 62: return "Input past end of file";
    case 64: return "Bad file name";
    case 70: return "Permission denied";
    default: break;
    }
    return "Unprintable error";
}

/* store rn/rs/rt into a variable (idx > 0: array element in the pool) */
static void store_val(long slot, long idx)
{
    double x;
    if (vars[slot].kind == 2) {
        if (rt != 1) {
            rt_error(13, "Type mismatch");
            return;
        }
        if (idx > 0) s_cpy(&pool_s[idx], &rs);
        else s_cpy(&vars[slot].s, &rs);
        return;
    }
    if (rt != 0) {
        rt_error(13, "Type mismatch");
        return;
    }
    x = rn;
    if (vars[slot].kind == 1) x = rint_d(x);
    if (idx > 0) pool_n[idx] = x;
    else vars[slot].n = x;
}

static void load_val(long slot, long idx)
{
    if (vars[slot].kind == 2) {
        if (idx > 0) s_cpy(&rs, &pool_s[idx]);
        else s_cpy(&rs, &vars[slot].s);
        rt = 1;
    } else {
        if (idx > 0) rn = pool_n[idx];
        else rn = vars[slot].n;
        rt = 0;
    }
}

/* parse a variable reference at pc: sets lv_slot / lv_idx */
static void lval(void)
{
    long s;
    lv_slot = 0;
    lv_idx = 0;
    if (tk[pc].t != 3 || tk[pc].f > 0) {
        syn();
        return;
    }
    s = tk[pc].k;
    lv_slot = s;
    if (tk[pc].x == 1) {
        pc++;
        lv_idx = arr_index(s);
    } else pc++;
}

static long resolve_label(long tp)
{
    char kb[64];
    long v;
    if (tk[tp].y > 0) return tk[tp].y;
    if (tk[tp].t == 1) sprintf(kb, "%.0f", rint_d(tk[tp].num));
    else {
        strncpy(kb, tk[tp].s, 60);
        kb[60] = '\0';
    }
    if (tk[tp].t != 1 && tk[tp].sl > 60) {
        Str k2;
        memset(&k2, 0, sizeof(k2));
        s_set(&k2, tk[tp].s, tk[tp].sl);
        v = hm_get(&lmap, SP(k2));
        s_free(&k2);
    } else v = hm_get(&lmap, kb);
    if (v == 0) return 0;
    tk[tp].y = ln_start[v];
    return tk[tp].y;
}

/* ---------- output ---------- */
static void num_str(double x, char *out)
{
    char s[64];
    fmtnum(x, s);
    if (x >= 0) {
        if (dial == 0) sprintf(out, " %s ", s);
        else sprintf(out, " %s", s);
        return;
    }
    if (dial == 0) sprintf(out, "%s ", s);
    else strcpy(out, s);
}

static void set_outfile(long n)
{
    if (n < 1 || n > NFILES) {
        rt_error(52, "Bad file name or number");
        return;
    }
    if (fl_mode[n] != 2 && fl_mode[n] != 3) {
        rt_error(54, "Bad file mode");
        return;
    }
    out_file = n;
}

/* optional '#n,' at pc; returns the file number (0 = none) */
static long file_arg(void)
{
    long n;
    n = 0;
    if (is_op(18)) {
        pc++;
        n = int_expr();
        if (is_op(9) || is_op(10)) pc++;
    }
    return n;
}

static void do_print(void)
{
    int nl;
    long n, k, sp, saved;
    char nb[96];
    saved = out_file;
    pc++;
    n = file_arg();
    if (err_flag == 1) return;
    if (n > 0) set_outfile(n);
    if (err_flag == 1) {
        out_file = saved;
        return;
    }
    nl = 1;
    if (is_kw("USING")) {
        print_using();
        out_file = saved;
        return;
    }
    while (at_end() == 0 && err_flag == 0) {
        k = tk[pc].k;
        if (tk[pc].t == 5 && k == 10) {
            pc++;
            nl = 0;
        } else if (tk[pc].t == 5 && k == 9) {
            pc++;
            sp = 14 - (out_col % 14);
            memset(nb, ' ', (size_t)sp);
            outs(nb, sp);
            nl = 0;
        } else {
            ex_prec(1);
            if (err_flag == 1) break;
            if (rt == 1) outs(SP(rs), rs.n);
            else {
                num_str(rn, nb);
                outz(nb);
            }
            nl = 1;
        }
    }
    if (nl == 1 && err_flag == 0) outs("\n", 1);
    out_file = saved;
}

/* ---------- PRINT USING ---------- */
static void fixed_str(double x, int dec, char *out)
{
    double m, v;
    char s[420];
    long l, z;
    m = pow(10.0, (double)dec);
    v = floor(x * m + 0.5);
    if (v >= 9.0e18) {
        fmtnum(x, out);
        return;
    }
    int_digits(v, s);
    l = (long)strlen(s);
    if (dec > 0) {
        if (l <= dec) {
            z = dec - l + 1;
            memmove(s + z, s, (size_t)l + 1);
            memset(s, '0', (size_t)z);
            l += z;
        }
        memcpy(out, s, (size_t)(l - dec));
        out[l - dec] = '.';
        memcpy(out + l - dec + 1, s + l - dec, (size_t)dec + 1);
        return;
    }
    strcpy(out, s);
}

static void group3(const char *ip, char *out)
{
    long n, first, i;
    char *o;
    n = (long)strlen(ip);
    o = out;
    first = n % 3;
    if (first == 0) first = 3;
    if (n <= 3) {
        strcpy(out, ip);
        return;
    }
    for (i = 0; i < n; i++) {
        if (i == first || (i > first && (i - first) % 3 == 0)) *o++ = ',';
        *o++ = ip[i];
    }
    *o = '\0';
}

/* right-justify txt in width w with fill character f */
static void right_fill(Str *d, const char *txt, long tl, long w, int f)
{
    long i;
    s_clr(d);
    if (tl >= w) {
        s_set(d, txt + tl - w, w);
        return;
    }
    for (i = 0; i < w - tl; i++) s_catc(d, f);
    s_cat(d, txt, tl);
}

/* format x according to a numeric field spec */
static void using_num(const char *spec, long n, double x, Str *outp)
{
    long i, nint, ndec, hasdot, lead_plus, dollar, star, comma, expo, neg, width, ex10, p0, k;
    int c;
    char trail;
    char body[420], ip[460], fp[420], txt[900], tmp[460];
    double mant, ax;
    int fill;

    i = 1;
    nint = 0;
    ndec = 0;
    hasdot = 0;
    lead_plus = 0;
    dollar = 0;
    star = 0;
    comma = 0;
    expo = 0;
    trail = '\0';
    if (ch_at(spec, n, i) == '+') {
        lead_plus = 1;
        i++;
    }
    if (ch_at(spec, n, i) == '*' && ch_at(spec, n, i + 1) == '*') {
        star = 1;
        i += 2;
        if (ch_at(spec, n, i) == '$') {
            dollar = 1;
            i++;
        }
    } else if (ch_at(spec, n, i) == '$' && ch_at(spec, n, i + 1) == '$') {
        dollar = 1;
        i += 2;
        nint++;
    }
    while (i <= n) {
        c = ch_at(spec, n, i);
        if (c == '#') nint++;
        else if (c == ',') comma = 1;
        else break;
        i++;
    }
    if (i <= n && ch_at(spec, n, i) == '.') {
        hasdot = 1;
        i++;
        while (i <= n && ch_at(spec, n, i) == '#') {
            ndec++;
            i++;
        }
    }
    if (ch_at(spec, n, i) == '^' && ch_at(spec, n, i + 1) == '^' && ch_at(spec, n, i + 2) == '^' &&
        ch_at(spec, n, i + 3) == '^') {
        expo = 1;
        i += 4;
    }
    if (i <= n) {
        c = ch_at(spec, n, i);
        if (c == '+' || c == '-') trail = (char)c;
    }
    neg = 0;
    if (x < 0) neg = 1;
    ax = fabs(x);
    txt[0] = '\0';
    if (expo == 1) {
        char es[40];
        ex10 = 0;
        if (ax != 0.0) ex10 = (long)floor(log10(ax));
        mant = ax / pow(10.0, (double)ex10);
        fixed_str(mant, (int)ndec, body);
        if (body[0] == '1' && body[1] == '0') {
            ex10++;
            fixed_str(mant / 10.0, (int)ndec, body);
        }
        strcpy(txt, body);
        if (ex10 < 0) {
            sprintf(es, "%ld", -ex10);
            strcat(txt, "E-");
        } else {
            sprintf(es, "%ld", ex10);
            strcat(txt, "E+");
        }
        k = (long)strlen(es);
        if (k == 1) strcat(txt, "0");
        else if (k > 2) memmove(es, es + k - 2, 3);
        strcat(txt, es);
    } else {
        fixed_str(ax, (int)ndec, body);
        p0 = rx_pos(".", 1, body, (long)strlen(body), 1);
        if (p0 > 0) {
            memcpy(ip, body, (size_t)(p0 - 1));
            ip[p0 - 1] = '\0';
            strcpy(fp, body + p0 - 1);
        } else {
            strcpy(ip, body);
            fp[0] = '\0';
        }
        if (comma == 1) {
            group3(ip, tmp);
            strcpy(ip, tmp);
        }
        if (strcmp(ip, "0") == 0 && nint == 0) ip[0] = '\0';
        strcpy(txt, ip);
        strcat(txt, fp);
        if (hasdot == 1 && ndec == 0) strcat(txt, ".");
    }
    tmp[0] = '\0';
    if (lead_plus == 1) {
        if (neg == 1) strcpy(tmp, "-");
        else strcpy(tmp, "+");
    } else if (neg == 1 && trail == '\0') strcpy(tmp, "-");
    if (dollar) strcat(tmp, "$");
    strcat(tmp, txt);
    strcpy(txt, tmp);
    if (trail != '\0') {
        if (neg == 1) strcat(txt, "-");
        else if (trail == '+') strcat(txt, "+");
        else strcat(txt, " ");
    }
    width = n;
    if ((long)strlen(txt) > width) {
        s_set(outp, "%", 1);
        s_cats(outp, txt);
        return;
    }
    fill = ' ';
    if (star == 1) fill = '*';
    right_fill(outp, txt, (long)strlen(txt), width, fill);
}

static void print_using(void)
{
    Str fmt, spec, tmp;
    long n, i, j, k, nfields, w;
    int c, c2, fl, nl, done, c3, c4;
    pc++;
    memset(&fmt, 0, sizeof(fmt));
    memset(&spec, 0, sizeof(spec));
    memset(&tmp, 0, sizeof(tmp));
    j = 0;
    if (!str_expr(&fmt)) {
        s_free(&fmt);
        return;
    }
    if (err_flag == 1) {
        s_free(&fmt);
        return;
    }
    if (is_op(10) || is_op(9)) pc++;
    n = fmt.n;
    nl = 1;
    done = 0;
    nfields = 0;
    while (done == 0 && err_flag == 0) {
        i = 1;
        nfields = 0;
        while (i <= n && done == 0 && err_flag == 0) {
            c = ch_at(fmt.p, n, i);
            c2 = ch_at(fmt.p, n, i + 1);
            c3 = ch_at(fmt.p, n, i + 2);
            c4 = ch_at(fmt.p, n, i + 3);
            (void)c4;
            fl = 0;
            if (c == '_') {
                outs(fmt.p + i, c2 != 0 ? 1 : 0);
                i += 2;
                continue;
            }
            if (c == '!') fl = 1;
            else if (c == '&') fl = 2;
            else if (c == '\\') {
                j = rx_pos("\\", 1, fmt.p, n, i + 1);
                if (j > 0) fl = 3;
            } else if (c == '#' || (c == '.' && c2 == '#')) fl = 4;
            else if ((c == '+' || c == '-') && (c2 == '#' || c2 == '.' || (c2 == '$' && c3 == '$') || (c2 == '*' && c3 == '*')) && c == '+') fl = 4;
            else if ((c == '*' && c2 == '*') || (c == '$' && c2 == '$')) fl = 4;
            if (fl == 0) {
                outs(fmt.p + i - 1, 1);
                i++;
                continue;
            }
            nfields++;
            if (at_end() == 1) {
                done = 1;
                break;
            }
            ex_prec(1);
            if (err_flag == 1) break;
            if (is_op(10) || is_op(9)) {
                pc++;
                if (at_end() == 1) nl = 0;
            }
            if (fl == 4) {
                if (rt == 1) {
                    rt_error(13, "Type mismatch");
                    break;
                }
                k = i;
                if (ch_at(fmt.p, n, k) == '+') k++;
                if (ch_at(fmt.p, n, k) == '*' && ch_at(fmt.p, n, k + 1) == '*') {
                    k += 2;
                    if (ch_at(fmt.p, n, k) == '$') k++;
                } else if (ch_at(fmt.p, n, k) == '$' && ch_at(fmt.p, n, k + 1) == '$') k += 2;
                while (k <= n && (ch_at(fmt.p, n, k) == '#' || ch_at(fmt.p, n, k) == ',')) k++;
                if (k <= n && ch_at(fmt.p, n, k) == '.') {
                    k++;
                    while (k <= n && ch_at(fmt.p, n, k) == '#') k++;
                }
                if (ch_at(fmt.p, n, k) == '^' && ch_at(fmt.p, n, k + 1) == '^' && ch_at(fmt.p, n, k + 2) == '^' &&
                    ch_at(fmt.p, n, k + 3) == '^') k += 4;
                if (k <= n && (ch_at(fmt.p, n, k) == '+' || ch_at(fmt.p, n, k) == '-') &&
                    ch_at(fmt.p, n, i) != ch_at(fmt.p, n, k)) k++;
                else if (k <= n && ch_at(fmt.p, n, k) == '-') k++;
                using_num(fmt.p + i - 1, k - i, rn, &spec);
                outs(SP(spec), spec.n);
                i = k;
            } else {
                if (rt != 1) {
                    rt_error(13, "Type mismatch");
                    break;
                }
                if (fl == 1) {
                    if (rs.n > 0) outs(SP(rs), 1);
                    else outs(" ", 1);
                    i++;
                } else if (fl == 2) {
                    outs(SP(rs), rs.n);
                    i++;
                } else {
                    w = j - i + 1;
                    s_cpy(&tmp, &rs);
                    while (tmp.n < w) s_catc(&tmp, ' ');
                    outs(SP(tmp), w);
                    i = j + 1;
                }
            }
            if (at_end() == 1) done = 1;
        }
        if (nfields == 0 && err_flag == 0) {
            rt_error(5, "Illegal function call");
            break;
        }
        if (at_end() == 1) done = 1;
    }
    if (nl == 1 && err_flag == 0) outs("\n", 1);
    s_free(&fmt);
    s_free(&spec);
    s_free(&tmp);
}

/* ---------- WRITE ---------- */
static void do_write(void)
{
    long n, saved;
    int first;
    char nb[96];
    saved = out_file;
    pc++;
    n = file_arg();
    if (n > 0) set_outfile(n);
    first = 1;
    while (at_end() == 0 && err_flag == 0) {
        ex_prec(1);
        if (err_flag == 1) break;
        if (first == 0) outs(",", 1);
        first = 0;
        if (rt == 1) {
            outs("\"", 1);
            outs(SP(rs), rs.n);
            outs("\"", 1);
        } else {
            fmtnum(rn, nb);
            outz(nb);
        }
        if (is_op(9) || is_op(10)) pc++;
    }
    if (err_flag == 0) outs("\n", 1);
    out_file = saved;
}

/* ---------- input ---------- */
/* reads a line into out; on an error err_flag is set */
static void read_text_line(long n, Str *out)
{
    FILE *fh;
    int e;
    s_clr(out);
    if (n == 0) {
        flush_out();
        fh = stdin;
    } else {
        if (n < 1 || n > NFILES) {
            rt_error(52, "Bad file name or number");
            return;
        }
        if (fl_mode[n] != 1) {
            rt_error(54, "Bad file mode");
            return;
        }
        if (fl_has[n] == 1) {
            fl_has[n] = 0;
            s_cpy(out, &fl_pend[n]);
            return;
        }
        if (fl_eof[n] == 1) {
            rt_error(62, "Input past end of file");
            return;
        }
        fh = fl_fp[n];
    }
    e = read_line(fh, out);
    if (e > 0 && out->n == 0) {
        if (n > 0) fl_eof[n] = 1;
        rt_error(62, "Input past end of file");
        return;
    }
    if (out->n > 0 && out->p[out->n - 1] == '\r') {
        out->n--;
        out->p[out->n] = '\0';
    }
}

static void split_fields(const Str *line)
{
    long n, i;
    int c, q, inq;
    Str w;
    memset(&w, 0, sizeof(w));
    n = line->n;
    inp_n = 0;
    q = 0;
    inq = 0;
    for (i = 0; i < n; i++) {
        c = (unsigned char)line->p[i];
        if (inq == 1) {
            if (c == '"') inq = 0;
            else s_catc(&w, c);
        } else if (c == '"') {
            /* a quote starts a quoted field only when nothing but blanks precedes it */
            long a;
            a = 0;
            while (a < w.n && w.p[a] == ' ') a++;
            if (a == w.n) {
                inq = 1;
                q = 1;
                s_clr(&w);
            } else s_catc(&w, c);
        } else if (c == ',') {
            inp_n++;
            NEED(inp_f, inp_cap, inp_n + 2);
            if (q == 1) s_cpy(&inp_f[inp_n], &w);
            else strip_into(&inp_f[inp_n], SP(w), w.n, 'B', ' ');
            s_clr(&w);
            q = 0;
        } else s_catc(&w, c);
    }
    inp_n++;
    NEED(inp_f, inp_cap, inp_n + 2);
    if (q == 1) s_cpy(&inp_f[inp_n], &w);
    else strip_into(&inp_f[inp_n], SP(w), w.n, 'B', ' ');
    s_free(&w);
}

/* assign a text field to a variable; returns 0 when a number is malformed */
static int assign_text(long slot, long idx, const Str *txt)
{
    Str t;
    int r;
    if (vars[slot].kind == 2) {
        s_cpy(&rs, txt);
        rt = 1;
        store_val(slot, idx);
        return 1;
    }
    rn = valparse(txt);
    rt = 0;
    r = 1;
    if (val_ok == 0) {
        memset(&t, 0, sizeof(t));
        strip_into(&t, SP(*txt), txt->n, 'B', ' ');
        if (t.n != 0) r = 0;
        s_free(&t);
    }
    if (r == 0) return 0;
    store_val(slot, idx);
    return 1;
}

static void do_input(void)
{
    static long *sl, *ix;
    static long vcap, vcap2;
    long n, nv, i, j;
    Str prompt, line;
    int isl, qm, ok;
    memset(&prompt, 0, sizeof(prompt));
    memset(&line, 0, sizeof(line));
    isl = 0;
    pc++;
    if (tks(stmt_pc, "LINEINPUT")) isl = 1;
    if (is_op(10)) pc++;
    n = file_arg();
    qm = 1;
    if (n == 0 && tk[pc].t == 2) {
        s_set(&prompt, tk[pc].s, tk[pc].sl);
        pc++;
        if (is_op(10)) pc++;
        else if (is_op(9)) {
            pc++;
            qm = 0;
        } else syn();
    }
    nv = 0;
    for (;;) {
        if (err_flag == 1) goto done;
        lval();
        if (err_flag == 1) goto done;
        nv++;
        NEED(sl, vcap, nv + 2);
        NEED(ix, vcap2, nv + 2);
        sl[nv] = lv_slot;
        ix[nv] = lv_idx;
        if (is_op(9)) pc++;
        else break;
    }
    if (isl == 1) {
        if (n == 0) outs(SP(prompt), prompt.n);
        read_text_line(n, &line);
        if (err_flag == 1) goto done;
        s_cpy(&rs, &line);
        rt = 1;
        store_val(sl[1], ix[1]);
        goto done;
    }
    for (;;) {
        if (n == 0) {
            outs(SP(prompt), prompt.n);
            if (qm == 1) outs("? ", 2);
        }
        ok = 1;
        i = 1;
        while (i <= nv && ok == 1) {
            read_text_line(n, &line);
            if (err_flag == 1) goto done;
            split_fields(&line);
            j = 1;
            while (j <= inp_n && i <= nv) {
                if (assign_text(sl[i], ix[i], &inp_f[j]) == 0) {
                    ok = 0;
                    break;
                }
                i++;
                j++;
            }
            if (ok == 1 && i <= nv && n == 0) outs("?" "? ", 3);
        }
        if (ok == 1) break;
        if (n != 0) {
            rt_error(13, "Type mismatch");
            goto done;
        }
        outs("?Redo from start\n", 17);
    }
done:
    s_free(&prompt);
    s_free(&line);
}

/* ---------- DATA / READ / RESTORE ---------- */
static void do_read(void)
{
    Str t;
    memset(&t, 0, sizeof(t));
    pc++;
    for (;;) {
        lval();
        if (err_flag == 1) return;
        if (dptr > nd) {
            rt_error(4, "Out of DATA");
            return;
        }
        s_set(&t, dat[dptr].s, dat[dptr].sl);
        if (assign_text(lv_slot, lv_idx, &t) == 0) {
            rt_error(13, "Type mismatch");
            s_free(&t);
            return;
        }
        dptr++;
        if (is_op(9)) pc++;
        else break;
    }
    s_free(&t);
}

static void do_restore(void)
{
    long t, li, v;
    char kb[64];
    pc++;
    dptr = 1;
    if (at_end() == 0) {
        if (tk[pc].t == 1) {
            sprintf(kb, "%.0f", rint_d(tk[pc].num));
            v = hm_get(&lmap, kb);
        } else v = hm_get(&lmap, tk[pc].s);
        pc++;
        if (v == 0) {
            rt_error(8, "Undefined line number");
            return;
        }
        li = v;
        dptr = nd + 1;
        for (t = 1; t <= nd; t++) {
            if (dat[t].line >= li) {
                dptr = t;
                break;
            }
        }
    }
}

/* ---------- files ---------- */
static void do_open(void)
{
    Str fnm, mode, w;
    int md;
    long n;
    const char *rmode;
    FILE *f;
    memset(&fnm, 0, sizeof(fnm));
    memset(&mode, 0, sizeof(mode));
    memset(&w, 0, sizeof(w));
    n = 0;
    pc++;
    if (!str_expr(&fnm)) goto done;
    md = 0;
    if (is_op(9)) {
        /* OPEN "I", #1, "file" */
        pc++;
        s_set(&mode, SP(fnm), fnm.n > 0 ? 1 : 0);
        up_str(&mode);
        if (is_op(18)) pc++;
        n = int_expr();
        if (is_op(9)) pc++;
        str_expr(&fnm);
        if (s_eq(&mode, "I")) md = 1;
        else if (s_eq(&mode, "O")) md = 2;
        else if (s_eq(&mode, "A")) md = 3;
    } else {
        md = 3;
        if (is_kw("FOR")) {
            pc++;
            s_set(&w, tk[pc].s, tk[pc].sl);
            up_str(&w);
            pc++;
            if (s_eq(&w, "INPUT")) md = 1;
            else if (s_eq(&w, "OUTPUT")) md = 2;
            else if (s_eq(&w, "APPEND")) md = 3;
            else md = -1;
        } else md = 3;
        while (is_kw("AS") == 0 && at_end() == 0) pc++;
        if (is_kw("AS")) pc++;
        if (is_op(18)) pc++;
        n = int_expr();
        while (at_end() == 0) pc++;
    }
    if (err_flag == 1) goto done;
    if (md <= 0) {
        rt_error(54, "Bad file mode");
        goto done;
    }
    if (n < 1 || n > NFILES) {
        rt_error(52, "Bad file name or number");
        goto done;
    }
    if (fl_mode[n] != 0) {
        rt_error(55, "File already open");
        goto done;
    }
    if (md == 1) rmode = "r";
    else if (md == 2) rmode = "w";
    else rmode = "a";
    f = fopen(SP(fnm), rmode);
    if (f == NULL) {
        if (md == 1) rt_error(53, "File not found");
        else rt_error(70, "Permission denied");
        goto done;
    }
    fl_fp[n] = f;
    fl_mode[n] = md;
    fl_eof[n] = 0;
    fl_has[n] = 0;
done:
    s_free(&fnm);
    s_free(&mode);
    s_free(&w);
}

static void close_file(long n)
{
    if (n < 1 || n > NFILES) return;
    if (fl_mode[n] == 0) return;
    if (fl_fp[n] != NULL) fclose(fl_fp[n]);
    fl_fp[n] = NULL;
    fl_mode[n] = 0;
    fl_has[n] = 0;
    fl_eof[n] = 0;
}

static void close_all(void)
{
    long i;
    for (i = 1; i <= NFILES; i++)
        if (fl_mode[i] != 0) close_file(i);
}

static void do_close(void)
{
    long n;
    pc++;
    if (at_end() == 1) {
        close_all();
        return;
    }
    for (;;) {
        if (is_op(18)) pc++;
        n = int_expr();
        if (err_flag == 1) return;
        close_file(n);
        if (is_op(9)) pc++;
        else break;
    }
}

/* ---------- the statement executor ---------- */
static int cond_expr(void)
{
    ex_prec(1);
    if (err_flag == 1) return 0;
    if (rt == 1) {
        rt_error(13, "Type mismatch");
        return 0;
    }
    if (rn != 0.0) return 1;
    return 0;
}

static void goto_tok(long tp)
{
    long t;
    t = resolve_label(tp);
    if (t == 0) rt_error(8, "Undefined line number");
    else pc = t;
}

static void report_error(void)
{
    long li;
    char s[400];
    li = tk[stmt_pc].l;
    flush_out();
    if (li >= 1 && ln_num[li] >= 0) sprintf(s, "%s in %ld\n", last_error_text, ln_num[li]);
    else sprintf(s, "%s in line %ld\n", last_error_text, li);
    errs(s);
}

static int handle_error(void)
{
    if (cfp > 0) return 0;
    if (on_err != 0 && redo_flag == 0) {
        res_pc = mstmt;
        err_flag = 0;
        if (on_err < 0) {
            pc = stmt_end(mstmt);
            return 1;
        }
        redo_flag = 1;
        pc = on_err;
        return 1;
    }
    report_error();
    halted = 1;
    rc_exit = 1;
    return 0;
}

static void run_loop(void)
{
    long fsp0, ssp0;
    int t;
    fsp0 = fsp;
    ssp0 = ssp;
    for (;;) {
        if (halted == 1 || retflag == 1) break;
        t = tk[pc].t;
        if (t == 6) {
            pc++;
            continue;
        }
        if (t == 5 && tk[pc].k == 11) {
            pc++;
            continue;
        }
        if (t == 7) {
            halted = 1;
            break;
        }
        stmt_pc = pc;
        if (cfp == 0) mstmt = pc;
        if (t == 4) exec_kw();
        else if (t == 3) exec_assign();
        else syn();
        if (err_flag == 1) {
            if (handle_error() == 0) break;
        }
    }
    fsp = fsp0;
    ssp = ssp0;
}

static void call_sub(long r)
{
    long base, na;
    base = ab_cnt;
    na = 0;
    if (is_op(7)) na = eval_args(r);
    else if (at_end() == 0) {
        bare_flag = 1;
        na = eval_args(r);
        bare_flag = 0;
    }
    if (err_flag == 1) {
        ab_cnt = base;
        return;
    }
    invoke(r, base, na);
}

/* an assignment, or a SUB call written without CALL */
static void exec_assign(void)
{
    long r, k, s1, i1, at;
    double an;
    Str as0;
    if (tk[pc].f > 0) {
        r = find_proc(tk[pc].s);
        if (r > 0) {
            pc++;
            call_sub(r);
            return;
        }
        if (tks(pc, "MID")) {
            mid_assign();
            return;
        }
        syn();
        return;
    }
    r = tk[pc].y;
    if (r == 0) {
        r = find_proc(tk[pc].s);
        if (r == 0) r = BIGKIND;
        tk[pc].y = r;
    }
    if (r > 0 && r < BIGKIND && (tk[pc + 1].t != 5 || tk[pc + 1].k != 12)) {
        pc++;
        call_sub(r);
        return;
    }
    if (tk[pc].x == 0) {
        s1 = tk[pc].k;
        i1 = 0;
        pc++;
    } else {
        lval();
        if (err_flag == 1) return;
        s1 = lv_slot;
        i1 = lv_idx;
    }
    if (tk[pc].t == 5 && tk[pc].k == 12) {
        pc++;
        ex_prec(1);
        if (err_flag == 1) return;
        if (i1 == 0 && rt == 0 && vars[s1].kind == 0) vars[s1].n = rn;
        else store_val(s1, i1);
        return;
    }
    /* compound assignment: += -= *= /= \= ^= &= */
    if (tk[pc].t == 5 && tk[pc + 1].t == 5 && tk[pc + 1].k == 12) {
        k = tk[pc].k;
        if (k == 1 || k == 2 || k == 3 || k == 4 || k == 5 || k == 6 || k == 19) {
            pc += 2;
            load_val(s1, i1);
            an = rn;
            at = rt;
            memset(&as0, 0, sizeof(as0));
            if (at == 1) s_cpy(&as0, &rs);
            ex_prec(1);
            if (err_flag == 1) {
                s_free(&as0);
                return;
            }
            binop(k, an, &as0, (int)at);
            s_free(&as0);
            if (err_flag == 1) return;
            store_val(s1, i1);
            return;
        }
    }
    syn();
}

static void mid_assign(void)
{
    long s1, i1, st, ln;
    Str txt, cur;
    memset(&txt, 0, sizeof(txt));
    memset(&cur, 0, sizeof(cur));
    pc++;
    need_op(7);
    lval();
    if (err_flag == 1) return;
    s1 = lv_slot;
    i1 = lv_idx;
    need_op(9);
    st = int_expr();
    ln = 1000000;
    if (is_op(9)) {
        pc++;
        ln = int_expr();
    }
    need_rparen();
    need_op(12);
    if (!str_expr(&txt)) {
        s_free(&txt);
        return;
    }
    if (err_flag == 1) {
        s_free(&txt);
        return;
    }
    load_val(s1, i1);
    s_cpy(&cur, &rs);
    if (st < 1 || st > cur.n) {
        rt_error(5, "Illegal function call");
        goto done;
    }
    if (ln > txt.n) ln = txt.n;
    if (ln > cur.n - st + 1) ln = cur.n - st + 1;
    if (ln > 0) memcpy(cur.p + st - 1, txt.p, (size_t)ln);
    s_cpy(&rs, &cur);
    rt = 1;
    store_val(s1, i1);
done:
    s_free(&txt);
    s_free(&cur);
}

static void exec_kw(void)
{
    long k;
    k = tk[pc].k;
    switch (k) {
    case 3: do_if(); break;
    case 8: do_for(); break;
    case 11: do_next(); break;
    case 1: do_print(); break;
    case 17:
        pc++;
        goto_tok(pc);
        break;
    case 2:
        pc++;
        exec_assign();
        break;
    case 18: do_gosub(); break;
    case 19: do_return(); break;
    case 12: do_while(); break;
    case 13: pc = tk[pc].x; break;
    case 14: do_do(); break;
    case 15: do_loop(); break;
    case 5:
    case 6: pc = tk[pc].x; break;
    case 7: pc++; break;
    case 58: do_select(); break;
    case 59:
    case 60: pc = tk[pc].x; break;
    case 61:
        if (ssp > 0) ssp--;
        pc++;
        break;
    case 25:
    case 26:
    case 74:
    case 76: do_dim(); break;
    case 41:
    case 42: do_input(); break;
    case 29: do_read(); break;
    case 30: do_restore(); break;
    case 28: pc++; break;
    case 32: do_swap(); break;
    case 62: do_call(); break;
    case 45:
    case 46: do_subdef(); break;
    case 47:
    case 48:
    case 52:
    case 53: retflag = 1; break;
    case 49:
    case 55: do_exitfor(); break;
    case 50: pc = tk[tk[pc].x].x; break;
    case 56: pc = tk[tk[pc].x].y; break;
    case 51: pc = tk[tk[pc].x].x; break;
    case 57: pc = tk[pc].x; break;
    case 54: pc = tk[tk[pc].x].x; break;
    case 20: do_on(); break;
    case 21: do_onerror(); break;
    case 70: do_resume(); break;
    case 71:
        pc++;
        k = int_expr();
        if (err_flag == 0) rt_error(k, errmsg(k));
        break;
    case 22:
    case 23:
    case 24: halted = 1; break;
    case 44: do_const(); break;
    case 31:
        if (tk[pc].x > 0) pc = tk[pc].x;
        else pc = stmt_end(pc);
        break;
    case 33: do_randomize(); break;
    case 34: do_cls(); break;
    case 35: do_locate(); break;
    case 36: do_color(); break;
    case 37:
        outs("\a", 1);
        pc++;
        break;
    case 38: do_sleep(); break;
    case 39: do_open(); break;
    case 40: do_close(); break;
    case 43: do_write(); break;
    case 69: do_option(); break;
    case 27: do_erase(); break;
    case 88: do_clear(); break;
    case 64:
    case 75:
    case 72:
    case 73:
    case 81:
    case 82:
    case 77:
    case 78:
    case 89:
    case 90:
    case 63: pc = stmt_end(pc); break;
    default: syn(); break;
    }
}

static void do_subdef(void)
{
    long r;
    /* FUNCTION = value  (FreeBASIC result assignment) or a definition to skip */
    if (tk[pc + 1].t == 5 && tk[pc + 1].k == 12) {
        if (cfp == 0) {
            syn();
            return;
        }
        r = procs[cf_proc[cfp]].ret;
        pc += 2;
        ex_prec(1);
        if (err_flag == 1) return;
        store_val(r, 0);
        return;
    }
    pc = tk[pc].x;
}

static void do_call(void)
{
    long r;
    pc++;
    if (tk[pc].t != 3 && tk[pc].f == 0) {
        syn();
        return;
    }
    r = find_proc(tk[pc].s);
    if (r == 0) {
        char b[200];
        strcpy(b, "Syntax error: unknown SUB ");
        strncat(b, tk[pc].s, 100);
        rt_error(2, b);
        return;
    }
    pc++;
    call_sub(r);
}

static void do_if(void)
{
    long p, t;
    int c;
    p = pc;
    pc++;
    c = cond_expr();
    if (err_flag == 1) return;
    if (is_kw("THEN")) pc++;
    else if (is_kw("GOTO") == 0) {
        syn();
        return;
    }
    if (tk[p].x == 1) {
        if (c == 0) pc = tk[p].y;
        if (tk[pc].t == 1) goto_tok(pc);
        return;
    }
    t = p;
    for (;;) {
        if (c == 1) break;
        t = tk[t].y;
        if (tks(t, "ELSEIF")) {
            pc = t + 1;
            c = cond_expr();
            if (err_flag == 1) return;
            if (is_kw("THEN")) pc++;
            continue;
        }
        if (tks(t, "ELSE")) pc = t + 1;
        else pc = t;
        break;
    }
}

static void do_gosub(void)
{
    pc++;
    if (gsp > 20000) {
        rt_error(7, "Out of memory");
        return;
    }
    gsp++;
    NEED(g_ret, g_ret_cap, gsp + 2);
    g_ret[gsp] = stmt_end(pc + 1);
    goto_tok(pc);
    if (err_flag == 1) gsp--;
}

static void do_return(void)
{
    long r;
    pc++;
    if (cfp > 0) {
        if (gsp == cf_gsp[cfp]) {
            r = cf_proc[cfp];
            if (procs[r].isfn == 1 && at_end() == 0) {
                ex_prec(1);
                if (err_flag == 1) return;
                store_val(procs[r].ret, 0);
            }
            retflag = 1;
            return;
        }
    }
    if (gsp > 0) {
        pc = g_ret[gsp];
        gsp--;
    } else rt_error(3, "RETURN without GOSUB");
}

static void do_while(void)
{
    long p;
    p = pc;
    pc++;
    if (cond_expr() == 0 && err_flag == 0) pc = tk[p].x;
}

static void do_do(void)
{
    long p;
    int c, isw;
    p = pc;
    pc++;
    if (is_kw("WHILE") || is_kw("UNTIL")) {
        isw = tks(pc, "WHILE");
        pc++;
        c = cond_expr();
        if (err_flag == 1) return;
        if (isw && c == 0) pc = tk[p].x;
        else if (!isw && c == 1) pc = tk[p].x;
    }
}

static void do_loop(void)
{
    long p;
    int c, isw;
    p = pc;
    pc++;
    if (is_kw("WHILE") || is_kw("UNTIL")) {
        isw = tks(pc, "WHILE");
        pc++;
        c = cond_expr();
        if (err_flag == 1) return;
        if (isw && c == 1) pc = tk[p].x;
        else if (!isw && c == 0) pc = tk[p].x;
    } else pc = tk[p].x;
}

static void do_for(void)
{
    long p, slot, j;
    int ok;
    double v, e, st;
    ForEnt *f;
    p = pc;
    pc++;
    lval();
    if (err_flag == 1) return;
    slot = lv_slot;
    if (lv_idx > 0 || vars[slot].kind == 2) {
        rt_error(13, "Type mismatch");
        return;
    }
    need_op(12);
    v = num_expr();
    if (is_kw("TO")) pc++;
    else {
        syn();
        return;
    }
    e = num_expr();
    st = 1.0;
    if (is_kw("STEP")) {
        pc++;
        st = num_expr();
    }
    if (err_flag == 1) return;
    if (vars[slot].kind == 1) v = rint_d(v);
    vars[slot].n = v;
    j = fsp;
    while (j >= 1) {
        if (fstack[j].slot == slot) {
            fsp = j - 1;
            break;
        }
        j--;
    }
    ok = 1;
    if (st >= 0 && v > e) ok = 0;
    if (st < 0 && v < e) ok = 0;
    if (ok == 0) {
        pc = tk[p].x;
        return;
    }
    fsp++;
    NEED(fstack, fstack_cap, fsp + 2);
    f = &fstack[fsp];
    f->slot = slot;
    f->end = e;
    f->step = st;
    f->pc = pc;
    f->forp = p;
}

static void do_next(void)
{
    long slot, j;
    double v;
    int c;
    pc++;
    for (;;) {
        slot = 0;
        if (tk[pc].t == 3) {
            slot = tk[pc].k;
            pc++;
        }
        if (fsp < 1) {
            rt_error(1, "NEXT without FOR");
            return;
        }
        if (slot > 0) {
            j = fsp;
            while (j >= 1) {
                if (fstack[j].slot == slot) break;
                j--;
            }
            if (j < 1) {
                rt_error(1, "NEXT without FOR");
                return;
            }
            fsp = j;
        }
        j = fstack[fsp].slot;
        v = vars[j].n + fstack[fsp].step;
        vars[j].n = v;
        c = 0;
        if (fstack[fsp].step >= 0) {
            if (v <= fstack[fsp].end) c = 1;
        } else {
            if (v >= fstack[fsp].end) c = 1;
        }
        if (c == 1) {
            pc = fstack[fsp].pc;
            return;
        }
        fsp--;
        if (is_op(9)) pc++;
        else break;
    }
}

/* EXIT FOR / CONTINUE FOR */
static void do_exitfor(void)
{
    long f, j;
    int isexit;
    isexit = tks(pc, "EXITFOR");
    f = tk[pc].x;
    j = fsp;
    while (j >= 1) {
        if (fstack[j].forp == f) break;
        j--;
    }
    if (isexit) {
        if (j >= 1) fsp = j - 1;
        pc = tk[f].x;
    } else {
        if (j >= 1) fsp = j;
        pc = tk[f].y;
    }
}

static int cmp3(double an, const Str *as0, int at, double bn, const Str *bs, int bt)
{
    if (at != bt) {
        rt_error(13, "Type mismatch");
        return 0;
    }
    if (at == 0) {
        if (an < bn) return -1;
        if (an > bn) return 1;
        return 0;
    }
    return cmp_str(as0, bs);
}

static int case_match(const Str *ss, double sn, int st)
{
    int m, r, c;
    long k;
    double ln;
    Str ls;
    int lt;
    m = 0;
    memset(&ls, 0, sizeof(ls));
    for (;;) {
        r = 0;
        if (is_kw("IS")) {
            pc++;
            k = tk[pc].k;
            if (tk[pc].t != 5 || k < 12 || k > 17) {
                syn();
                s_free(&ls);
                return 0;
            }
            pc++;
            ex_prec(1);
            if (err_flag == 1) {
                s_free(&ls);
                return 0;
            }
            c = cmp3(sn, ss, st, rn, &rs, rt);
            if (k == 12 && c == 0) r = 1;
            else if (k == 13 && c < 0) r = 1;
            else if (k == 14 && c > 0) r = 1;
            else if (k == 15 && c <= 0) r = 1;
            else if (k == 16 && c >= 0) r = 1;
            else if (k == 17 && c != 0) r = 1;
        } else {
            ex_prec(1);
            if (err_flag == 1) {
                s_free(&ls);
                return 0;
            }
            if (is_kw("TO")) {
                pc++;
                ln = rn;
                lt = rt;
                s_cpy(&ls, &rs);
                ex_prec(1);
                if (err_flag == 1) {
                    s_free(&ls);
                    return 0;
                }
                if (cmp3(sn, ss, st, ln, &ls, lt) >= 0 && cmp3(sn, ss, st, rn, &rs, rt) <= 0) r = 1;
            } else if (cmp3(sn, ss, st, rn, &rs, rt) == 0) r = 1;
        }
        if (err_flag == 1) {
            s_free(&ls);
            return 0;
        }
        if (r == 1) m = 1;
        if (is_op(9)) pc++;
        else break;
    }
    s_free(&ls);
    return m;
}

static void do_select(void)
{
    long c;
    Str ss;
    double sn;
    int st, m;
    pc++;
    ex_prec(1);
    if (err_flag == 1) return;
    ssp++;
    NEED(sstack, sstack_cap, ssp + 2);
    sstack[ssp].n = rn;
    s_cpy(&sstack[ssp].s, &rs);
    sstack[ssp].t = rt;
    memset(&ss, 0, sizeof(ss));
    s_cpy(&ss, &rs);
    sn = rn;
    st = rt;
    c = tk[stmt_pc].y;
    for (;;) {
        if (tks(c, "ENDSELECT")) {
            pc = c;
            break;
        }
        if (tks(c, "CASEELSE")) {
            pc = c + 1;
            break;
        }
        pc = c + 1;
        m = case_match(&ss, sn, st);
        if (m == 1) break;
        if (err_flag == 1) break;
        c = tk[c].y;
    }
    s_free(&ss);
}

static void do_swap(void)
{
    long s1, i1, s2, i2;
    double an;
    int at;
    Str as0;
    memset(&as0, 0, sizeof(as0));
    pc++;
    lval();
    s1 = lv_slot;
    i1 = lv_idx;
    need_op(9);
    lval();
    s2 = lv_slot;
    i2 = lv_idx;
    if (err_flag == 1) return;
    load_val(s1, i1);
    an = rn;
    at = rt;
    s_cpy(&as0, &rs);
    load_val(s2, i2);
    store_val(s1, i1);
    if (err_flag == 1) {
        s_free(&as0);
        return;
    }
    rn = an;
    s_cpy(&rs, &as0);
    rt = at;
    store_val(s2, i2);
    s_free(&as0);
}

static void do_on(void)
{
    static long *tp;
    static long tpcap;
    long n, m, e;
    Str w;
    memset(&w, 0, sizeof(w));
    pc++;
    n = int_expr();
    if (err_flag == 1) return;
    if (is_kw("GOTO") || is_kw("GOSUB")) {
        s_set(&w, tk[pc].s, tk[pc].sl);
        pc++;
    } else {
        syn();
        return;
    }
    m = 0;
    for (;;) {
        m++;
        NEED(tp, tpcap, m + 2);
        tp[m] = pc;
        pc++;
        if (is_op(9)) pc++;
        else break;
    }
    if (n < 1 || n > m) {
        s_free(&w);
        return;
    }
    e = pc;
    if (s_eq(&w, "GOSUB")) {
        gsp++;
        NEED(g_ret, g_ret_cap, gsp + 2);
        g_ret[gsp] = e;
    }
    goto_tok(tp[n]);
    if (err_flag == 1 && s_eq(&w, "GOSUB")) gsp--;
    s_free(&w);
}

static void do_onerror(void)
{
    long t;
    pc++;
    if (is_kw("GOTO")) {
        pc++;
        if (tk[pc].t == 1 && tk[pc].num == 0.0) {
            on_err = 0;
            pc++;
        } else {
            t = resolve_label(pc);
            if (t == 0) rt_error(8, "Undefined line number");
            else on_err = t;
            pc++;
        }
    } else if (is_kw("RESUME")) {
        pc++;
        if (tk[pc].t == 3 || tk[pc].t == 4) pc++;
        on_err = -1;
    } else syn();
}

static void do_resume(void)
{
    pc++;
    if (redo_flag == 0) {
        rt_error(20, "RESUME without error");
        return;
    }
    redo_flag = 0;
    if (at_end() == 1) pc = res_pc;
    else if (tk[pc].t == 4 && tks(pc, "NEXT")) pc = stmt_end(res_pc);
    else goto_tok(pc);
}

static void do_dim(void)
{
    long slot, hasp, ob, ocnt, ncnt, i, lo, hi, e, l1, h1, l2, h2, l3, h3;
    int nd0, preserve, skip, isre;
    Var *v;
    isre = 0;
    if (tks(pc, "REDIM")) isre = 1;
    (void)isre;
    pc++;
    preserve = 0;
    if (is_kw("PRESERVE")) {
        preserve = 1;
        pc++;
    }
    if (is_kw("SHARED")) pc++;
    if (is_kw("AS")) {
        pc++;
        if (tk[pc].t == 8) pc++;
    }
    for (;;) {
        if (tk[pc].t != 3) {
            syn();
            return;
        }
        slot = tk[pc].k;
        hasp = tk[pc].x;
        pc++;
        l1 = optbase;
        h1 = 0;
        l2 = optbase;
        h2 = 0;
        l3 = optbase;
        h3 = 0;
        skip = 0;
        if (hasp == 1) {
            pc++;
            nd0 = 0;
            for (;;) {
                e = int_expr();
                if (err_flag == 1) return;
                if (is_kw("TO")) {
                    pc++;
                    lo = e;
                    hi = int_expr();
                } else {
                    lo = optbase;
                    hi = e;
                }
                nd0++;
                if (nd0 == 1) {
                    l1 = lo;
                    h1 = hi;
                } else if (nd0 == 2) {
                    l2 = lo;
                    h2 = hi;
                } else if (nd0 == 3) {
                    l3 = lo;
                    h3 = hi;
                } else {
                    syn();
                    return;
                }
                if (is_op(9)) pc++;
                else break;
            }
            need_rparen();
            if (err_flag == 1) return;
            if (vars[slot].stat == 1 && vars[slot].isarr == 1) skip = 1;
            if (skip == 0) {
                v = &vars[slot];
                ob = v->base;
                ocnt = 0;
                if (v->isarr == 1 && v->nd == 1) ocnt = v->hi[0] - v->lo[0] + 1;
                dim_array(slot, nd0, l1, h1, l2, h2, l3, h3);
                if (err_flag == 1) return;
                if (preserve == 1 && nd0 == 1 && ocnt > 0) {
                    ncnt = h1 - l1 + 1;
                    if (ocnt < ncnt) ncnt = ocnt;
                    for (i = 0; i < ncnt; i++) {
                        if (vars[slot].kind == 2) s_cpy(&pool_s[vars[slot].base + i], &pool_s[ob + i]);
                        else pool_n[vars[slot].base + i] = pool_n[ob + i];
                    }
                }
            }
        } else if (vars[slot].stat == 0) {
            vars[slot].n = 0.0;
            s_clr(&vars[slot].s);
        }
        if (is_kw("AS")) {
            pc++;
            if (tk[pc].t == 8) pc++;
        }
        if (is_op(12) && hasp == 0) {
            pc++;
            ex_prec(1);
            if (err_flag == 1) return;
            store_val(slot, 0);
        }
        if (is_op(9)) pc++;
        else break;
    }
}

static void do_const(void)
{
    long slot;
    pc++;
    for (;;) {
        if (tk[pc].t != 3) {
            syn();
            return;
        }
        slot = tk[pc].k;
        pc++;
        if (is_kw("AS")) {
            pc++;
            if (tk[pc].t == 8) pc++;
        }
        need_op(12);
        ex_prec(1);
        if (err_flag == 1) return;
        store_val(slot, 0);
        vars[slot].cnst = 1;
        if (is_op(9)) pc++;
        else break;
    }
}

static void do_erase(void)
{
    pc++;
    while (tk[pc].t == 3) {
        vars[tk[pc].k].isarr = 0;
        pc++;
        if (is_op(7)) {
            pc++;
            need_rparen();
        }
        if (is_op(9)) pc++;
    }
}

static void do_clear(void)
{
    long i;
    for (i = 1; i <= nvars; i++) {
        if (vars[i].cnst == 0) {
            vars[i].n = 0.0;
            s_clr(&vars[i].s);
            vars[i].isarr = 0;
        }
    }
    dptr = 1;
    pc = stmt_end(pc);
}

static void do_option(void)
{
    pc++;
    if (tk[pc].t == 3 && tks(pc, "BASE")) {
        pc++;
        optbase = int_expr();
    } else pc = stmt_end(pc);
}

static void do_randomize(void)
{
    double x;
    long i;
    pc++;
    if (at_end() == 1) x = floor(clock_seconds() * 1000000.0);
    else x = num_expr();
    if (err_flag == 1) return;
    rnd_state = fmod(rint_d(fmod(fabs(x), 2147483646.0)), 2147483646.0) + 1.0;
    for (i = 1; i <= 4; i++) rnd_next();
}

static void do_cls(void)
{
    pc++;
    if (ansi == 1) outz("\033[2J\033[H");
    out_col = 0;
}

static void do_locate(void)
{
    long r, c;
    char b[64];
    pc++;
    r = -1;
    c = -1;
    if (at_end() == 0 && is_op(9) == 0) r = int_expr();
    if (is_op(9)) {
        pc++;
        if (at_end() == 0 && is_op(9) == 0) c = int_expr();
    }
    pc = stmt_end(pc);
    if (err_flag == 1) return;
    if (ansi == 1) {
        if (r > 0 && c > 0) {
            sprintf(b, "\033[%ld;%ldH", r, c);
            outz(b);
        } else if (r > 0) {
            sprintf(b, "\033[%ld;1H", r);
            outz(b);
        } else if (c > 0) {
            sprintf(b, "\033[%ldG", c);
            outz(b);
        }
    }
    if (c > 0) out_col = c - 1;
}

static long ansi_color(long n, int bg)
{
    long base, m;
    m = n % 8;
    /* BASIC colour order is black blue green cyan red magenta yellow white */
    if (m == 1) base = 4;
    else if (m == 3) base = 6;
    else if (m == 4) base = 1;
    else if (m == 6) base = 3;
    else if (m == 2) base = 2;
    else if (m == 5) base = 5;
    else if (m == 0) base = 0;
    else base = 7;
    if (bg == 1) {
        if (n >= 8) return 100 + base;
        return 40 + base;
    }
    if (n >= 8) return 90 + base;
    return 30 + base;
}

static void do_color(void)
{
    long f, b;
    char nb[64];
    pc++;
    f = -1;
    b = -1;
    if (at_end() == 0 && is_op(9) == 0) f = int_expr();
    if (is_op(9)) {
        pc++;
        if (at_end() == 0 && is_op(9) == 0) b = int_expr();
    }
    pc = stmt_end(pc);
    if (err_flag == 1) return;
    if (ansi == 1) {
        if (f >= 0) {
            sprintf(nb, "\033[%ldm", ansi_color(f, 0));
            outz(nb);
        }
        if (b >= 0) {
            sprintf(nb, "\033[%ldm", ansi_color(b, 1));
            outz(nb);
        }
    }
}

static void do_sleep(void)
{
    double x, t0, t1;
    pc++;
    flush_out();
    if (at_end() == 1) {
        (void)getc(stdin);
        return;
    }
    x = num_expr();
    if (err_flag == 1) return;
    t0 = clock_seconds();
    for (;;) {
        t1 = clock_seconds();
        if (t1 < t0 || t1 - t0 >= x) break;
    }
}
