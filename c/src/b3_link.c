/* b3_link.c - the link pass: blocks, procedures, DEF FN, parameters */

static int tks(long p, const char *z)
{
    return tk[p].s != NULL && strcmp(tk[p].s, z) == 0;
}

static long stmt_end(long q0)
{
    long q;
    q = q0;
    while (tk[q].t != 6 && tk[q].t != 7) {
        if (tk[q].t == 5 && tk[q].k == 11) break;
        q++;
    }
    return q;
}

static long line_end(long q0)
{
    long q;
    q = q0;
    while (tk[q].t != 6 && tk[q].t != 7) q++;
    return q;
}

static void line_label(long tp, char *out)
{
    long li;
    li = tk[tp].l;
    if (li < 1) {
        strcpy(out, "?");
        return;
    }
    if (ln_num[li] >= 0) sprintf(out, "line %ld", ln_num[li]);
    else sprintf(out, "source line %ld", li);
}

static void link_error(const char *msg, long tp)
{
    char lb[64];
    line_label(tp, lb);
    printf("basic: %s at %s\n", msg, lb);
    link_errs++;
}

static void link_error2(const char *kw, const char *msg, long tp)
{
    char b[200];
    strncpy(b, kw, 100);
    b[100] = '\0';
    strcat(b, msg);
    link_error(b, tp);
}

static long find_proc(const char *nme)
{
    long j;
    for (j = 1; j <= np; j++)
        if (strcmp(procs[j].name, nme) == 0) return j;
    return 0;
}

static long find_fn(const char *nme)
{
    long j;
    for (j = 1; j <= nfn; j++)
        if (strcmp(fns_def[j].name, nme) == 0) return j;
    return 0;
}

static int link_program(void)
{
    static long *bk, *bp, *bc;
    static long bcap, bpcap, bccap;
    long sp, p, q, r, e, dd, j, jj, kwant, cnt, prev;
    const char *kw;

    link_errs = 0;
    sp = 0;
    if (tk[ntk].t != 7) {
        emitz(7, 0.0, "", 0, 0, nlines);
    }
    p = 1;
    while (p <= ntk) {
        if (tk[p].t != 4) {
            p++;
            continue;
        }
        kw = tk[p].s;
        NEED(bk, bcap, sp + 4);
        NEED(bp, bpcap, sp + 4);
        NEED(bc, bccap, sp + 4);
        if (strcmp(kw, "IF") == 0) {
            q = p + 1;
            while (tk[q].t != 6 && tk[q].t != 7) {
                if (tk[q].t == 4 && (tks(q, "THEN") || tks(q, "GOTO"))) break;
                q++;
            }
            if (tk[q].t != 4) link_error("IF without THEN", p);
            else if (tks(q, "THEN") && tk[q + 1].t == 6) {
                sp++;
                NEED(bk, bcap, sp + 4);
                NEED(bp, bpcap, sp + 4);
                NEED(bc, bccap, sp + 4);
                bk[sp] = 1;
                bp[sp] = p;
                bc[sp] = p;
                tk[p].x = 0;
            } else {
                tk[p].x = 1;
                dd = 0;
                e = 0;
                r = q + 1;
                while (tk[r].t != 6 && tk[r].t != 7) {
                    if (tk[r].t == 4) {
                        if (tks(r, "IF")) dd++;
                        else if (tks(r, "ELSE")) {
                            if (dd == 0) {
                                e = r;
                                break;
                            }
                            dd--;
                        }
                    }
                    r++;
                }
                if (e > 0) {
                    tk[p].y = e + 1;
                    tk[e].x = line_end(e);
                } else tk[p].y = r;
            }
        } else if (strcmp(kw, "ELSEIF") == 0 || strcmp(kw, "ELSE") == 0) {
            if (strcmp(kw, "ELSE") == 0 && tk[p].x > 0) {
                /* ELSE of a single-line IF */
            } else if (sp == 0) link_error2(kw, " without IF", p);
            else if (bk[sp] != 1) link_error2(kw, " without IF", p);
            else {
                tk[bc[sp]].y = p;
                bc[sp] = p;
            }
        } else if (strcmp(kw, "ENDIF") == 0) {
            if (sp == 0) link_error("END IF without IF", p);
            else if (bk[sp] != 1) link_error("END IF without IF", p);
            else {
                tk[bc[sp]].y = p;
                q = bp[sp];
                while (q != p) {
                    r = tk[q].y;
                    if (tk[r].t == 4 && (tks(r, "ELSEIF") || tks(r, "ELSE"))) tk[r].x = p;
                    q = r;
                }
                sp--;
            }
        } else if (strcmp(kw, "WHILE") == 0 && p > 1 && tk[p - 1].t == 4 &&
                   (tks(p - 1, "DO") || tks(p - 1, "LOOP"))) {
            /* DO WHILE / LOOP WHILE */
        } else if (strcmp(kw, "WHILE") == 0) {
            sp++;
            NEED(bk, bcap, sp + 4);
            NEED(bp, bpcap, sp + 4);
            NEED(bc, bccap, sp + 4);
            bk[sp] = 2;
            bp[sp] = p;
            bc[sp] = 0;
        } else if (strcmp(kw, "WEND") == 0) {
            if (sp == 0) link_error("WEND without WHILE", p);
            else if (bk[sp] != 2) link_error("WEND without WHILE", p);
            else {
                tk[bp[sp]].x = p + 1;
                tk[p].x = bp[sp];
                sp--;
            }
        } else if (strcmp(kw, "DO") == 0) {
            sp++;
            NEED(bk, bcap, sp + 4);
            NEED(bp, bpcap, sp + 4);
            NEED(bc, bccap, sp + 4);
            bk[sp] = 3;
            bp[sp] = p;
            bc[sp] = 0;
        } else if (strcmp(kw, "LOOP") == 0) {
            if (sp == 0) link_error("LOOP without DO", p);
            else if (bk[sp] != 3) link_error("LOOP without DO", p);
            else {
                tk[bp[sp]].x = stmt_end(p);
                tk[bp[sp]].y = p;
                tk[p].x = bp[sp];
                sp--;
            }
        } else if (strcmp(kw, "FOR") == 0 && p > 1 && tk[p - 1].t != 6 &&
                   (tk[p - 1].t != 5 || tk[p - 1].k != 11) &&
                   (tk[p - 1].t != 4 || (!tks(p - 1, "THEN") && !tks(p - 1, "ELSE")))) {
            /* OPEN ... FOR INPUT */
        } else if (strcmp(kw, "FOR") == 0) {
            sp++;
            NEED(bk, bcap, sp + 4);
            NEED(bp, bpcap, sp + 4);
            NEED(bc, bccap, sp + 4);
            bk[sp] = 4;
            bp[sp] = p;
            bc[sp] = 0;
        } else if (strcmp(kw, "NEXT") == 0 && p > 1 && tk[p - 1].t == 4 && tks(p - 1, "RESUME")) {
            /* RESUME NEXT */
        } else if (strcmp(kw, "NEXT") == 0) {
            cnt = 0;
            q = p + 1;
            dd = 0;
            while (tk[q].t != 6 && tk[q].t != 7) {
                if (tk[q].t == 5 && tk[q].k == 11) break;
                if (tk[q].t == 3 && dd == 0) cnt++;
                if (tk[q].t == 5 && tk[q].k == 7) dd++;
                if (tk[q].t == 5 && tk[q].k == 8) dd--;
                q++;
            }
            if (cnt < 1) cnt = 1;
            for (j = 1; j <= cnt; j++) {
                if (sp == 0) {
                    link_error("NEXT without FOR", p);
                    break;
                }
                if (bk[sp] != 4) {
                    link_error("NEXT without FOR", p);
                    break;
                }
                tk[bp[sp]].x = q;
                tk[bp[sp]].y = p;
                sp--;
            }
        } else if (strcmp(kw, "SELECTCASE") == 0) {
            sp++;
            NEED(bk, bcap, sp + 4);
            NEED(bp, bpcap, sp + 4);
            NEED(bc, bccap, sp + 4);
            bk[sp] = 5;
            bp[sp] = p;
            bc[sp] = p;
        } else if (strcmp(kw, "CASE") == 0 || strcmp(kw, "CASEELSE") == 0) {
            if (sp == 0) link_error("CASE without SELECT CASE", p);
            else if (bk[sp] != 5) link_error("CASE without SELECT CASE", p);
            else {
                tk[bc[sp]].y = p;
                bc[sp] = p;
            }
        } else if (strcmp(kw, "ENDSELECT") == 0) {
            if (sp == 0) link_error("END SELECT without SELECT CASE", p);
            else if (bk[sp] != 5) link_error("END SELECT without SELECT CASE", p);
            else {
                tk[bc[sp]].y = p;
                tk[bp[sp]].x = p;
                q = tk[bp[sp]].y;
                while (q != p) {
                    tk[q].x = p;
                    q = tk[q].y;
                }
                sp--;
            }
        } else if ((strcmp(kw, "SUB") == 0 || strcmp(kw, "FUNCTION") == 0) && tk[p + 1].t == 5 &&
                   tk[p + 1].k == 12) {
            /* FUNCTION = value */
        } else if (strcmp(kw, "SUB") == 0 || strcmp(kw, "FUNCTION") == 0) {
            sp++;
            NEED(bk, bcap, sp + 4);
            NEED(bp, bpcap, sp + 4);
            NEED(bc, bccap, sp + 4);
            if (strcmp(kw, "SUB") == 0) bk[sp] = 6;
            else bk[sp] = 7;
            bp[sp] = p;
            bc[sp] = 0;
            j = 0;
            for (jj = 1; jj <= np; jj++)
                if (procs[jj].pc == p) j = jj;
            if (j > 0) {
                procs[j].body = line_end(p) + 1;
                bc[sp] = j;
            }
        } else if (strcmp(kw, "ENDSUB") == 0 || strcmp(kw, "ENDFUNCTION") == 0) {
            if (sp == 0) link_error("END SUB/FUNCTION without SUB/FUNCTION", p);
            else if (bk[sp] < 6) link_error("END SUB/FUNCTION without SUB/FUNCTION", p);
            else {
                tk[bp[sp]].x = p + 1;
                if (bc[sp] > 0) procs[bc[sp]].end = p;
                sp--;
            }
        } else if (strcmp(kw, "EXITFOR") == 0 || strcmp(kw, "CONTFOR") == 0 || strcmp(kw, "EXITDO") == 0 ||
                   strcmp(kw, "CONTDO") == 0 || strcmp(kw, "EXITWHILE") == 0 ||
                   strcmp(kw, "CONTWHILE") == 0 || strcmp(kw, "EXITSELECT") == 0) {
            if (strcmp(kw, "EXITFOR") == 0 || strcmp(kw, "CONTFOR") == 0) kwant = 4;
            else if (strcmp(kw, "EXITDO") == 0 || strcmp(kw, "CONTDO") == 0) kwant = 3;
            else if (strcmp(kw, "EXITWHILE") == 0 || strcmp(kw, "CONTWHILE") == 0) kwant = 2;
            else kwant = 5;
            j = sp;
            while (j >= 1) {
                if (bk[j] == kwant) break;
                j--;
            }
            if (j == 0) link_error2(kw, " outside of a matching block", p);
            else tk[p].x = bp[j];
        } else if (strcmp(kw, "DEF") == 0) {
            if (tk[p + 1].t == 3) {
                nfn++;
                NEED(fns_def, nfn_cap, nfn + 1);
                if (fns_def[nfn].name != NULL) free(fns_def[nfn].name);
                fns_def[nfn].name = xstrdup(tk[p + 1].s, tk[p + 1].sl);
                fns_def[nfn].pc = p + 2;
                tk[p].x = stmt_end(p);
            }
        }
        p++;
    }
    if (sp > 0) {
        while (sp > 0) {
            char b[120];
            strcpy(b, "block not closed (");
            strncat(b, tk[bp[sp]].s, 60);
            strcat(b, ")");
            link_error(b, bp[sp]);
            sp--;
        }
    }
    /* parameter tables */
    npp = 0;
    for (j = 1; j <= np; j++) {
        procs[j].pfirst = npp + 1;
        procs[j].pcnt = 0;
        q = procs[j].pc + 2;
        if (tk[q].t == 5 && tk[q].k == 7) {
            q++;
            r = 1;
            while (tk[q].t != 6 && tk[q].t != 7) {
                if (tk[q].t == 5 && tk[q].k == 8) break;
                if (tk[q].t == 4 && tks(q, "BYVAL")) r = 0;
                else if (tk[q].t == 4 && tks(q, "BYREF")) r = 1;
                else if (tk[q].t == 3) {
                    npp++;
                    NEED(pparms, npp_cap, npp + 1);
                    procs[j].pcnt++;
                    pparms[npp].slot = tk[q].k;
                    pparms[npp].ref = r;
                    pparms[npp].arr = 0;
                    pparms[npp].kind = vars[tk[q].k].kind;
                    if (tk[q].x == 1) {
                        pparms[npp].arr = 1;
                        q += 2;
                    }
                    r = 1;
                }
                q++;
            }
        }
    }
    /* per-procedure lists of local slots (saved/restored around calls) */
    cnt = 0;
    prev = 0;
    for (j = 1; j <= np; j++) {
        procs[j].first = cnt + 1;
        procs[j].cnt = 0;
        for (q = 1; q <= nvars; q++) {
            if (vars[q].proc == j && vars[q].stat == 0) {
                cnt++;
                NEED(pl_slots, pl_slots_cap, cnt + 1);
                pl_slots[cnt] = q;
                procs[j].cnt++;
            }
        }
    }
    (void)prev;
    if (link_errs > 0) return 0;
    return 1;
}
