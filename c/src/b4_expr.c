/* b4_expr.c - errors, arrays, expression evaluation, calls and builtin functions */

/* ---------- errors ---------- */
static void rt_error(long code, const char *msg)
{
    long li;
    if (err_flag == 1) return;
    err_flag = 1;
    err_code = code;
    strncpy(last_error_text, msg, sizeof(last_error_text) - 1);
    last_error_text[sizeof(last_error_text) - 1] = '\0';
    li = tk[stmt_pc].l;
    if (li >= 1) {
        if (ln_num[li] >= 0) err_line = ln_num[li];
        else err_line = li;
    } else err_line = 0;
}

static void syn(void)
{
    rt_error(2, "Syntax error");
}

/* ---------- output primitives ---------- */
static void outs(const char *txt, long n)
{
    long k;
    FILE *fh;
    if (n <= 0) return;
    if (out_file == 0) fh = stdout;
    else fh = fl_fp[out_file];
    if (fh != NULL) fwrite(txt, 1, (size_t)n, fh);
    k = n;
    while (k > 0 && txt[k - 1] != '\n') k--;
    if (k > 0) out_col = n - k;
    else out_col += n;
}

static void outz(const char *z)
{
    outs(z, (long)strlen(z));
}

static void flush_out(void)
{
    fflush(stdout);
}

static void errs(const char *txt)
{
    fflush(stdout);
    fputs(txt, stderr);
    fflush(stderr);
}

/* ---------- arrays ---------- */
static void dim_array(long slot, int nd0, long l1, long h1, long l2, long h2, long l3, long h3)
{
    double tot;
    long i, cnt;
    Var *v;
    if (h1 < l1) {
        rt_error(9, "Subscript out of range");
        return;
    }
    if (nd0 >= 2 && h2 < l2) {
        rt_error(9, "Subscript out of range");
        return;
    }
    if (nd0 >= 3 && h3 < l3) {
        rt_error(9, "Subscript out of range");
        return;
    }
    tot = (double)h1 - (double)l1 + 1.0;
    if (nd0 >= 2) tot = tot * ((double)h2 - (double)l2 + 1.0);
    if (nd0 >= 3) tot = tot * ((double)h3 - (double)l3 + 1.0);
    if (tot > 30000000.0) {
        rt_error(7, "Out of memory");
        return;
    }
    cnt = (long)tot;
    v = &vars[slot];
    v->lo[0] = l1;
    v->hi[0] = h1;
    v->lo[1] = l2;
    v->hi[1] = h2;
    v->lo[2] = l3;
    v->hi[2] = h3;
    v->nd = nd0;
    v->isarr = 1;
    if (v->kind == 2) {
        v->base = nps + 1;
        NEED(pool_s, pool_s_cap, nps + cnt + 2);
        for (i = 1; i <= cnt; i++) {
            nps++;
            s_clr(&pool_s[nps]);
        }
    } else {
        v->base = npn + 1;
        NEED(pool_n, pool_n_cap, npn + cnt + 2);
        for (i = 1; i <= cnt; i++) {
            npn++;
            pool_n[npn] = 0.0;
        }
    }
}

static void auto_dim(long slot, int nsub)
{
    if (nsub == 1) dim_array(slot, 1, optbase, 10, 0, 0, 0, 0);
    else if (nsub == 2) dim_array(slot, 2, optbase, 10, optbase, 10, 0, 0);
    else dim_array(slot, 3, optbase, 10, optbase, 10, optbase, 10);
}

static void need_rparen(void)
{
    if (tk[pc].t == 5 && tk[pc].k == 8) pc++;
    else rt_error(2, "Syntax error: ')' expected");
}

static void need_op(long k)
{
    if (tk[pc].t == 5 && tk[pc].k == k) pc++;
    else rt_error(2, "Syntax error");
}

/* parse (i, j, k) at pc and return the pool index of the element */
static long arr_index(long slot)
{
    int nsub;
    long i1, i2, i3, idx;
    Var *v;
    nsub = 0;
    i1 = 0;
    i2 = 0;
    i3 = 0;
    pc++;
    for (;;) {
        ex_prec(1);
        if (rt == 1) {
            rt_error(13, "Type mismatch");
            return 0;
        }
        nsub++;
        if (nsub == 1) i1 = tol(rn);
        else if (nsub == 2) i2 = tol(rn);
        else if (nsub == 3) i3 = tol(rn);
        else {
            syn();
            return 0;
        }
        if (tk[pc].t == 5 && tk[pc].k == 9) pc++;
        else break;
    }
    need_rparen();
    if (err_flag == 1) return 0;
    if (vars[slot].isarr == 0) auto_dim(slot, nsub);
    if (err_flag == 1) return 0;
    v = &vars[slot];
    if (nsub != v->nd) {
        rt_error(9, "Subscript out of range");
        return 0;
    }
    if (i1 < v->lo[0] || i1 > v->hi[0]) {
        rt_error(9, "Subscript out of range");
        return 0;
    }
    idx = i1 - v->lo[0];
    if (nsub >= 2) {
        if (i2 < v->lo[1] || i2 > v->hi[1]) {
            rt_error(9, "Subscript out of range");
            return 0;
        }
        idx = idx * (v->hi[1] - v->lo[1] + 1) + (i2 - v->lo[1]);
    }
    if (nsub >= 3) {
        if (i3 < v->lo[2] || i3 > v->hi[2]) {
            rt_error(9, "Subscript out of range");
            return 0;
        }
        idx = idx * (v->hi[2] - v->lo[2] + 1) + (i3 - v->lo[2]);
    }
    return v->base + idx;
}

/* ---------- expression evaluation (precedence climbing); result in rn / rs / rt ---------- */
static void s_prepend(Str *d, const Str *a)
{
    if (a->n <= 0) return;
    s_reserve(d, d->n + a->n);
    memmove(d->p + a->n, SP(*d), (size_t)d->n + 1);
    memcpy(d->p, a->p, (size_t)a->n);
    d->n += a->n;
}

static void ex_prec(int minp)
{
    double ln;
    Str ls;
    int lt;
    long k, p;
    ls.p = NULL;
    ls.n = 0;
    ls.cap = 0;
    ex_unary();
    for (;;) {
        if (tk[pc].t != 5) break;
        k = tk[pc].k;
        p = prec[k];
        if (p == 0 || p < minp) break;
        pc++;
        ln = rn;
        lt = rt;
        if (lt == 1) s_cpy(&ls, &rs);
        ex_prec((int)p + 1);
        binop(k, ln, &ls, lt);
    }
    s_free(&ls);
}

static void ex_unary(void)
{
    int t;
    long k;
    I64 a;
    t = tk[pc].t;
    if (t == 1) {
        rn = tk[pc].num;
        rt = 0;
        pc++;
        return;
    }
    if (t == 2) {
        s_set(&rs, tk[pc].s, tk[pc].sl);
        rt = 1;
        pc++;
        return;
    }
    if (t == 3) {
        if (tk[pc].y == BIGKIND && tk[pc].x == 0 && tk[pc].f == 0) {
            k = tk[pc].k;
            if (vars[k].kind == 2) {
                s_cpy(&rs, &vars[k].s);
                rt = 1;
            } else {
                rn = vars[k].n;
                rt = 0;
            }
            pc++;
            return;
        }
        ex_id();
        return;
    }
    if (t == 5) {
        k = tk[pc].k;
        if (k == 7) {
            pc++;
            ex_prec(1);
            need_rparen();
            return;
        }
        if (k == 2) {
            pc++;
            ex_prec(15);
            if (rt == 1) rt_error(13, "Type mismatch");
            else rn = -rn;
            return;
        }
        if (k == 1) {
            pc++;
            ex_prec(15);
            if (rt == 1) rt_error(13, "Type mismatch");
            return;
        }
        if (k == 39) {
            pc++;
            ex_prec(7);
            if (rt == 1) rt_error(13, "Type mismatch");
            else {
                a = i64_from(rint_d(rn));
                rn = i64_to(i64_not(a));
            }
            return;
        }
    }
    syn();
    rn = 0.0;
    rt = 0;
}

static void binop(long k, double ln, const Str *ls, int lt)
{
    int tf;
    I64 a, b;
    double x, y;
    char nb[64];
    long sh;

    if (k == 1) {
        if (lt == 0 && rt == 0) rn = ln + rn;
        else if (lt == 1 && rt == 1) s_prepend(&rs, ls);
        else rt_error(13, "Type mismatch");
        return;
    }
    if (k == 19) {
        if (rt == 0) {
            fmtnum(rn, nb);
            s_set(&rs, nb, (long)strlen(nb));
        }
        if (lt == 0) {
            Str t;
            memset(&t, 0, sizeof(t));
            fmtnum(ln, nb);
            s_set(&t, nb, (long)strlen(nb));
            s_prepend(&rs, &t);
            s_free(&t);
        } else s_prepend(&rs, ls);
        rt = 1;
        return;
    }
    if (k >= 12 && k <= 17) {
        tf = 0;
        if (lt == 0 && rt == 0) {
            switch (k) {
            case 12: tf = ln == rn; break;
            case 13: tf = ln < rn; break;
            case 14: tf = ln > rn; break;
            case 15: tf = ln <= rn; break;
            case 16: tf = ln >= rn; break;
            default: tf = ln != rn; break;
            }
        } else if (lt == 1 && rt == 1) {
            int c;
            c = cmp_str(ls, &rs);
            switch (k) {
            case 12: tf = c == 0; break;
            case 13: tf = c < 0; break;
            case 14: tf = c > 0; break;
            case 15: tf = c <= 0; break;
            case 16: tf = c >= 0; break;
            default: tf = c != 0; break;
            }
        } else {
            rt_error(13, "Type mismatch");
            return;
        }
        if (tf == 1) rn = -1.0;
        else rn = 0.0;
        rt = 0;
        return;
    }
    if (lt == 1 || rt == 1) {
        rt_error(13, "Type mismatch");
        return;
    }
    switch (k) {
    case 2:
        rn = ln - rn;
        break;
    case 3:
        rn = ln * rn;
        break;
    case 4:
        if (rn == 0.0) {
            rt_error(11, "Division by zero");
            rn = 0.0;
        } else rn = ln / rn;
        break;
    case 5:
        x = fix_d(ln);
        y = fix_d(rn);
        if (y == 0.0) {
            rt_error(11, "Division by zero");
            rn = 0.0;
        } else rn = (x - fmod(x, y)) / y;
        break;
    case 36:
        x = fix_d(ln);
        y = fix_d(rn);
        if (y == 0.0) {
            rt_error(11, "Division by zero");
            rn = 0.0;
        } else rn = fmod(x, y);
        break;
    case 6:
        if (ln == 0.0 && rn < 0.0) {
            rt_error(11, "Division by zero");
            rn = 0.0;
        } else if (ln < 0.0 && floor(rn) != rn) {
            rt_error(5, "Illegal function call");
            rn = 0.0;
        } else rn = pow(ln, rn);
        break;
    case 31:
        rn = i64_to(i64_and(i64_from(rint_d(ln)), i64_from(rint_d(rn))));
        break;
    case 32:
        rn = i64_to(i64_or(i64_from(rint_d(ln)), i64_from(rint_d(rn))));
        break;
    case 33:
        rn = i64_to(i64_xor(i64_from(rint_d(ln)), i64_from(rint_d(rn))));
        break;
    case 34:
        rn = i64_to(i64_not(i64_xor(i64_from(rint_d(ln)), i64_from(rint_d(rn)))));
        break;
    case 35:
        rn = i64_to(i64_or(i64_not(i64_from(rint_d(ln))), i64_from(rint_d(rn))));
        break;
    case 37:
    case 38:
        x = rint_d(ln);
        y = rint_d(rn);
        if (y < 0.0 || y > 63.0) {
            if (k == 37) rn = 0.0;
            else if (x < 0.0) rn = -1.0;
            else rn = 0.0;
        } else {
            a = i64_from(x);
            b = a;
            for (sh = 0; sh < (long)y; sh++) b = (k == 37) ? i64_shl1(b) : i64_sar1(b);
            rn = i64_to(b);
        }
        break;
    default:
        syn();
        break;
    }
    rt = 0;
}

/* identifier: variable, array element, builtin or user function */
static void ex_id(void)
{
    long slot, idx, j, base, na, r;
    slot = tk[pc].k;
    if (tk[pc].f > 0) {
        if (tk[pc].y == 0) tk[pc].y = find_proc(tk[pc].s) + 1;
        if (tk[pc].y == 1) {
            do_builtin();
            return;
        }
        j = tk[pc].y - 1;
        pc++;
        base = ab_cnt;
        na = 0;
        if (tk[pc].t == 5 && tk[pc].k == 7) na = eval_args(j);
        if (err_flag == 1) {
            ab_cnt = base;
            return;
        }
        invoke(j, base, na);
        return;
    }
    if (tk[pc].x == 0) {
        /* a user function with no argument list? */
        j = tk[pc].y;
        if (j == 0) {
            if (tk[pc].sl > 2 && tk[pc].s[0] == 'F' && tk[pc].s[1] == 'N') j = 0 - find_fn(tk[pc].s);
            if (j == 0) j = BIGKIND;
            tk[pc].y = j;
        }
        if (j < 0) {
            call_fn(0 - j);
            return;
        }
        if (vars[slot].kind == 2) {
            s_cpy(&rs, &vars[slot].s);
            rt = 1;
        } else {
            rn = vars[slot].n;
            rt = 0;
        }
        pc++;
        return;
    }
    /* followed by '(' */
    if (vars[slot].isarr == 1) {
        pc++;
        idx = arr_index(slot);
        if (err_flag == 1) return;
        if (vars[slot].kind == 2) {
            s_cpy(&rs, &pool_s[idx]);
            rt = 1;
        } else {
            rn = pool_n[idx];
            rt = 0;
        }
        return;
    }
    r = tk[pc].y;
    if (r == 0) {
        r = find_proc(tk[pc].s);
        if (r == 0) {
            j = find_fn(tk[pc].s);
            if (j > 0) r = -j;
        }
        if (r == 0) r = BIGKIND;
        tk[pc].y = r;
    }
    if (r > 0 && r < BIGKIND) {
        pc++;
        base = ab_cnt;
        na = eval_args(r);
        if (err_flag == 1) {
            ab_cnt = base;
            return;
        }
        invoke(r, base, na);
        return;
    }
    if (r < 0) {
        call_fn(-r);
        return;
    }
    /* implicit array */
    pc++;
    idx = arr_index(slot);
    if (err_flag == 1) return;
    if (vars[slot].kind == 2) {
        s_cpy(&rs, &pool_s[idx]);
        rt = 1;
    } else {
        rn = pool_n[idx];
        rt = 0;
    }
}

/* ---------- argument lists and procedure calls ---------- */
/* pc is at '('; evaluate the arguments onto the argument stack; returns count */
static long eval_args(long p)
{
    long n0, sv, pc2, idx;
    ArgEnt *a;
    n0 = 0;
    if (bare_flag == 0) {
        pc++;
        if (tk[pc].t == 5 && tk[pc].k == 8) {
            pc++;
            return 0;
        }
    }
    for (;;) {
        if (tk[pc].t == 5 && tk[pc].k == 18) pc++;
        sv = pc;
        if (tk[sv].t == 3 && tk[sv].f == 0 && tk[sv].x == 1 && tk[sv + 1].t == 5 && tk[sv + 1].k == 7 &&
            tk[sv + 2].t == 5 && tk[sv + 2].k == 8) {
            /* whole array: name() */
            ab_cnt++;
            NEED(argstk, ab_cap, ab_cnt + 2);
            a = &argstk[ab_cnt];
            a->n = 0.0;
            s_clr(&a->s);
            a->t = 2;
            a->slot = tk[sv].k;
            a->idx = 0;
            pc = sv + 3;
        } else {
            ex_prec(1);
            if (err_flag == 1) return n0;
            ab_cnt++;
            NEED(argstk, ab_cap, ab_cnt + 2);
            a = &argstk[ab_cnt];
            a->n = rn;
            s_cpy(&a->s, &rs);
            a->t = rt;
            a->slot = 0;
            a->idx = 0;
            if (p > 0 && n0 < procs[p].pcnt) {
                if (pparms[procs[p].pfirst + n0].ref == 1 && tk[sv].t == 3 && tk[sv].f == 0) {
                    if (tk[sv].x == 0 && pc == sv + 1) argstk[ab_cnt].slot = tk[sv].k;
                    else if (tk[sv].x == 1 && vars[tk[sv].k].isarr == 1) {
                        long cur;
                        cur = ab_cnt;
                        pc2 = pc;
                        pc = sv + 1;
                        idx = arr_index(tk[sv].k);
                        if (pc == pc2 && err_flag == 0) {
                            argstk[cur].slot = 0 - tk[sv].k;
                            argstk[cur].idx = idx;
                        }
                        pc = pc2;
                    }
                }
            }
        }
        n0++;
        if (tk[pc].t == 5 && tk[pc].k == 9) pc++;
        else break;
    }
    if (bare_flag == 0) need_rparen();
    return n0;
}

static void swap_str(Str *a, Str *b)
{
    Str t;
    t = *a;
    *a = *b;
    *b = t;
}

/* call a SUB / FUNCTION: arguments are on the argument stack from base+1 */
static void invoke(long p, long base, long na)
{
    long first, cnt, npar, pf, i, d, s, a, r, src, save_pc, sv0;
    Proc *pr;
    SvEnt *sv;
    Var *v;
    if (cfp > 400) {
        rt_error(7, "Out of memory (recursion too deep)");
        ab_cnt = base;
        return;
    }
    pr = &procs[p];
    first = pr->first;
    cnt = pr->cnt;
    npar = pr->pcnt;
    pf = pr->pfirst;
    save_pc = pc;
    sv0 = svp;
    /* save and reset the locals */
    for (i = 0; i < cnt; i++) {
        s = pl_slots[first + i];
        svp++;
        NEED(svstack, svstack_cap, svp + 2);
        sv = &svstack[svp];
        v = &vars[s];
        sv->n = v->n;
        swap_str(&sv->s, &v->s);
        sv->base = v->base;
        sv->isarr = v->isarr;
        sv->nd = v->nd;
        for (d = 0; d < 3; d++) {
            sv->lo[d] = v->lo[d];
            sv->hi[d] = v->hi[d];
        }
        v->n = 0.0;
        s_clr(&v->s);
        v->isarr = 0;
        v->nd = 0;
        v->base = 0;
    }
    /* bind the parameters */
    for (i = 1; i <= npar; i++) {
        s = pparms[pf + i - 1].slot;
        if (i <= na) {
            a = base + i;
            if (pparms[pf + i - 1].arr == 1) {
                if (argstk[a].t == 2) {
                    src = argstk[a].slot;
                    vars[s].base = vars[src].base;
                    vars[s].isarr = vars[src].isarr;
                    vars[s].nd = vars[src].nd;
                    for (d = 0; d < 3; d++) {
                        vars[s].lo[d] = vars[src].lo[d];
                        vars[s].hi[d] = vars[src].hi[d];
                    }
                } else rt_error(13, "Type mismatch");
            } else if (vars[s].kind == 2) {
                if (argstk[a].t == 1) s_cpy(&vars[s].s, &argstk[a].s);
                else rt_error(13, "Type mismatch");
            } else {
                if (argstk[a].t == 0) {
                    if (vars[s].kind == 1) vars[s].n = rint_d(argstk[a].n);
                    else vars[s].n = argstk[a].n;
                } else rt_error(13, "Type mismatch");
            }
        }
    }
    if (err_flag == 0) {
        cfp++;
        NEED(cf_gsp, cf_cap, cfp + 2);
        NEED(cf_proc, cf_pcap, cfp + 2);
        cf_gsp[cfp] = gsp;
        cf_proc[cfp] = p;
        retflag = 0;
        pc = pr->body;
        run_loop();
        cfp--;
        /* result of a FUNCTION */
        if (pr->isfn == 1) {
            r = pr->ret;
            if (vars[r].kind == 2) {
                s_cpy(&rs, &vars[r].s);
                rt = 1;
            } else {
                rn = vars[r].n;
                rt = 0;
            }
        } else {
            rn = 0.0;
            rt = 0;
        }
        /* copy BYREF parameters back */
        for (i = 1; i <= npar; i++) {
            if (i <= na) {
                a = base + i;
                s = pparms[pf + i - 1].slot;
                if (pparms[pf + i - 1].ref == 1 && pparms[pf + i - 1].arr == 0) {
                    if (argstk[a].slot > 0) {
                        if (vars[argstk[a].slot].kind == 2) s_cpy(&vars[argstk[a].slot].s, &vars[s].s);
                        else vars[argstk[a].slot].n = vars[s].n;
                    } else if (argstk[a].slot < 0 && argstk[a].idx > 0) {
                        if (vars[0 - argstk[a].slot].kind == 2) s_cpy(&pool_s[argstk[a].idx], &vars[s].s);
                        else pool_n[argstk[a].idx] = vars[s].n;
                    }
                }
            }
        }
    }
    /* restore the locals */
    for (i = cnt - 1; i >= 0; i--) {
        s = pl_slots[first + i];
        sv = &svstack[svp];
        v = &vars[s];
        v->n = sv->n;
        swap_str(&sv->s, &v->s);
        v->base = sv->base;
        v->isarr = sv->isarr;
        v->nd = sv->nd;
        for (d = 0; d < 3; d++) {
            v->lo[d] = sv->lo[d];
            v->hi[d] = sv->hi[d];
        }
        svp--;
    }
    svp = sv0;
    ab_cnt = base;
    pc = save_pc;
    retflag = 0;
}

/* DEF FN user function: pc is at the FN name token */
static void call_fn(long j)
{
    long q, base, na, i, np0, s, e0, save_pc, sv0;
    long pcs[256];
    q = fns_def[j].pc;
    np0 = 0;
    if (tk[q].t == 5 && tk[q].k == 7) {
        q++;
        while (tk[q].t != 6 && tk[q].t != 7) {
            if (tk[q].t == 5 && tk[q].k == 8) break;
            if (tk[q].t == 3 && np0 < 255) {
                np0++;
                pcs[np0] = tk[q].k;
            }
            q++;
        }
        q++;
    }
    e0 = q + 1;
    pc++;
    base = ab_cnt;
    na = 0;
    if (tk[pc].t == 5 && tk[pc].k == 7) na = eval_args(0);
    if (err_flag == 1) {
        ab_cnt = base;
        return;
    }
    if (na != np0) {
        rt_error(5, "Illegal function call");
        ab_cnt = base;
        return;
    }
    save_pc = pc;
    sv0 = svp;
    for (i = 1; i <= np0; i++) {
        s = pcs[i];
        svp++;
        NEED(svstack, svstack_cap, svp + 2);
        svstack[svp].n = vars[s].n;
        swap_str(&svstack[svp].s, &vars[s].s);
        s_clr(&vars[s].s);
        if (vars[s].kind == 2) {
            if (argstk[base + i].t == 1) s_cpy(&vars[s].s, &argstk[base + i].s);
            else rt_error(13, "Type mismatch");
        } else {
            if (argstk[base + i].t == 0) vars[s].n = argstk[base + i].n;
            else rt_error(13, "Type mismatch");
        }
    }
    ab_cnt = base;
    pc = e0;
    ex_prec(1);
    for (i = np0; i >= 1; i--) {
        s = pcs[i];
        vars[s].n = svstack[svp].n;
        swap_str(&svstack[svp].s, &vars[s].s);
        svp--;
    }
    svp = sv0;
    pc = save_pc;
}

/* ---------- builtin functions ---------- */
enum {
    B_NONE = 0,
    B_ABS, B_SGN, B_INT, B_FIX, B_FRAC, B_CINT, B_CLNG, B_CSNG, B_CDBL,
    B_CBOOL, B_SQR, B_SQRT, B_SIN, B_COS, B_TAN, B_ATN, B_ATAN, B_ASIN,
    B_ACOS, B_ATAN2, B_SINH, B_COSH, B_TANH, B_EXP, B_LOG, B_LOG10, B_LOG2,
    B_RND, B_TIMER, B_LEN, B_LEFT, B_RIGHT, B_MID, B_STR, B_VAL, B_VALINT,
    B_VALLNG, B_CHR, B_ASC, B_SPACE, B_STRING, B_UCASE, B_LCASE, B_LTRIM,
    B_RTRIM, B_TRIM, B_INSTR, B_INSTRREV, B_HEX, B_OCT, B_BIN, B_SPC,
    B_TAB, B_POW, B_MAX, B_MIN, B_ISNUMERIC, B_IIF, B_DATE, B_TIME, B_ERR,
    B_ERL, B_FRE, B_PEEK, B_CSRLIN, B_POS, B_COMMAND, B_ENVIRON, B_CURDIR,
    B_FREEFILE, B_EOF, B_LOF, B_LOC, B_SEEK, B_INKEY, B_INPUT, B_LBOUND,
    B_UBOUND,
    B_LAST
};

static const char *bnames[] = {
    "",
    "ABS", "SGN", "INT", "FIX", "FRAC", "CINT", "CLNG", "CSNG", "CDBL",
    "CBOOL", "SQR", "SQRT", "SIN", "COS", "TAN", "ATN", "ATAN", "ASIN",
    "ACOS", "ATAN2", "SINH", "COSH", "TANH", "EXP", "LOG", "LOG10", "LOG2",
    "RND", "TIMER", "LEN", "LEFT", "RIGHT", "MID", "STR", "VAL", "VALINT",
    "VALLNG", "CHR", "ASC", "SPACE", "STRING", "UCASE", "LCASE", "LTRIM",
    "RTRIM", "TRIM", "INSTR", "INSTRREV", "HEX", "OCT", "BIN", "SPC",
    "TAB", "POW", "MAX", "MIN", "ISNUMERIC", "IIF", "DATE", "TIME", "ERR",
    "ERL", "FRE", "PEEK", "CSRLIN", "POS", "COMMAND", "ENVIRON", "CURDIR",
    "FREEFILE", "EOF", "LOF", "LOC", "SEEK", "INKEY", "INPUT", "LBOUND",
    "UBOUND",
    ""
};

static long builtin_id(const char *nme)
{
    long i;
    for (i = 1; i < (long)B_LAST; i++)
        if (strcmp(bnames[i], nme) == 0) return i;
    return 0;
}

static int argok(const char *spec, int lo, int hi, long base, long na)
{
    long i;
    int c;
    if (na < lo || na > hi) {
        rt_error(5, "Illegal function call");
        return 0;
    }
    for (i = 1; i <= na; i++) {
        c = (i <= (long)strlen(spec)) ? spec[i - 1] : ' ';
        if (c == 'n' && argstk[base + i].t != 0) {
            rt_error(13, "Type mismatch");
            return 0;
        }
        if (c == 's' && argstk[base + i].t != 1) {
            rt_error(13, "Type mismatch");
            return 0;
        }
    }
    return 1;
}

static int hexval(int c)
{
    const char *q;
    c = to_up(c);
    q = c != 0 ? strchr("0123456789ABCDEF", c) : NULL;
    if (q == NULL) return 99;
    return (int)(q - "0123456789ABCDEF");
}

static double valparse(const Str *src)
{
    Str t, body;
    long n, p, j, k, idg, bs;
    int h, c, sg;
    double v;
    memset(&t, 0, sizeof(t));
    memset(&body, 0, sizeof(body));
    strip_into(&t, SP(*src), src->n, 'B', ' ');
    n = t.n;
    val_ok = 0;
    v = 0.0;
    if (n == 0) {
        s_free(&t);
        return 0.0;
    }
    if (t.p[0] == '&' && n >= 2) {
        h = to_up((unsigned char)t.p[1]);
        if (h == 'H' || h == 'O' || h == 'B') {
            bs = h == 'H' ? 16 : (h == 'O' ? 8 : 2);
            j = 3;
            while (j <= n && hexval((unsigned char)t.p[j - 1]) < bs) {
                v = v * (double)bs + (double)hexval((unsigned char)t.p[j - 1]);
                j++;
            }
            if (j == 3) {
                s_free(&t);
                return 0.0;
            }
            if (j > n) val_ok = 1;
            s_free(&t);
            return v;
        }
    }
    p = 1;
    sg = 0;
    c = (unsigned char)t.p[0];
    if (c == '+' || c == '-') {
        sg = c;
        p = 2;
    }
    j = p;
    while (j <= n && is_digit(ch_at(t.p, n, j))) j++;
    idg = j - p;
    if (j <= n && ch_at(t.p, n, j) == '.') {
        j++;
        k = j;
        while (j <= n && is_digit(ch_at(t.p, n, j))) j++;
        idg += (j - k);
    }
    if (idg == 0) {
        s_free(&t);
        return 0.0;
    }
    if (j <= n) {
        c = to_up(ch_at(t.p, n, j));
        if (c == 'E' || c == 'D') {
            k = j + 1;
            if (k <= n && (ch_at(t.p, n, k) == '+' || ch_at(t.p, n, k) == '-')) k++;
            if (k <= n && is_digit(ch_at(t.p, n, k))) {
                j = k;
                while (j <= n && is_digit(ch_at(t.p, n, j))) j++;
            }
        }
    }
    s_set(&body, t.p + p - 1, j - p);
    up_str(&body);
    for (k = 0; k < body.n; k++)
        if (body.p[k] == 'D') body.p[k] = 'E';
    if (j > n) val_ok = 1;
    v = numtofloat(SP(body));
    if (sg == '-') v = -v;
    s_free(&t);
    s_free(&body);
    return v;
}

static double rnd_next(void)
{
    rnd_state = fmod(rnd_state * 48271.0, 2147483647.0);
    if (rnd_state == 0.0) rnd_state = 1.0;
    rnd_last = rnd_state / 2147483647.0;
    return rnd_last;
}

static long lookup_array(const char *nme)
{
    Str key;
    long v;
    memset(&key, 0, sizeof(key));
    s_cats(&key, nme);
    s_catc(&key, '(');
    if (cfp > 0) {
        Str lk;
        memset(&lk, 0, sizeof(lk));
        s_cats(&lk, procs[cf_proc[cfp]].name);
        s_catc(&lk, ':');
        s_cat(&lk, key.p, key.n);
        v = hm_get(&nm, SP(lk));
        s_free(&lk);
        if (v != 0) {
            s_free(&key);
            return v;
        }
    }
    v = hm_get(&nm, SP(key));
    s_free(&key);
    return v;
}

static void do_builtin(void)
{
    long base, na, sl, d0, id;
    const char *nme;
    nme = tk[pc].s;
    id = tk[pc].k;
    if (id == 0) {
        id = builtin_id(tk[pc].s);
        tk[pc].k = id == 0 ? -1 : id;
    }
    if (id < 0) id = 0;
    if (id == B_LBOUND || id == B_UBOUND) {
        /* UBOUND(array [, dimension]) */
        pc++;
        need_op(7);
        sl = 0;
        if (tk[pc].t == 3) {
            sl = lookup_array(tk[pc].s);
            pc++;
            if (tk[pc].t == 5 && tk[pc].k == 7) {
                pc++;
                need_op(8);
            }
        }
        d0 = 1;
        if (tk[pc].t == 5 && tk[pc].k == 9) {
            pc++;
            ex_prec(1);
            d0 = tol(rn);
        }
        need_rparen();
        rt = 0;
        rn = 0.0;
        if (sl == 0 || err_flag == 1) {
            rt_error(5, "Illegal function call");
            return;
        }
        if (vars[sl].isarr == 0 || d0 < 1 || d0 > vars[sl].nd) {
            rt_error(9, "Subscript out of range");
            return;
        }
        if (id == B_LBOUND) rn = (double)vars[sl].lo[d0 - 1];
        else rn = (double)vars[sl].hi[d0 - 1];
        return;
    }
    pc++;
    base = ab_cnt;
    na = 0;
    if (tk[pc].t == 5 && tk[pc].k == 7) na = eval_args(0);
    rn = 0.0;
    s_clr(&rs);
    rt = 0;
    if (err_flag == 1) {
        ab_cnt = base;
        return;
    }
    builtin(id, base, na, nme);
    ab_cnt = base;
}

static void set_chr(Str *d, long n)
{
    char ch;
    ch = (char)(n & 255);
    s_set(d, &ch, 1);
}

static void rs_text(const char *p, long n)
{
    s_set(&rs, p, n);
    rt = 1;
}

static void builtin(long id, long base, long na, const char *nme)
{
    double x, y;
    const Str *s;
    const Str *t;
    static Str empty;
    long n, m, p, k, i;
    char nb[96];
    Str tmp;

    x = 0.0;
    s = &empty;
    if (na >= 1) {
        if (argstk[base + 1].t == 0) x = argstk[base + 1].n;
        else s = &argstk[base + 1].s;
    }
    switch (id) {
    case B_ABS:
        if (argok("n", 1, 1, base, na)) rn = fabs(x);
        break;
    case B_SGN:
        if (argok("n", 1, 1, base, na)) {
            if (x > 0) rn = 1.0;
            else if (x < 0) rn = -1.0;
            else rn = 0.0;
        }
        break;
    case B_INT:
        if (argok("n", 1, 1, base, na)) rn = floor(x);
        break;
    case B_FIX:
        if (argok("n", 1, 1, base, na)) rn = d_trunc(x);
        break;
    case B_FRAC:
        if (argok("n", 1, 1, base, na)) rn = x - d_trunc(x);
        break;
    case B_CINT:
    case B_CLNG:
        if (argok("n", 1, 1, base, na)) rn = rint_d(x);
        break;
    case B_CSNG:
    case B_CDBL:
        if (argok("n", 1, 1, base, na)) rn = x;
        break;
    case B_CBOOL:
        if (argok("n", 1, 1, base, na)) {
            if (x != 0) rn = -1.0;
            else rn = 0.0;
        }
        break;
    case B_SQR:
    case B_SQRT:
        if (argok("n", 1, 1, base, na)) {
            if (x < 0) rt_error(5, "Illegal function call");
            else rn = sqrt(x);
        }
        break;
    case B_SIN:
        if (argok("n", 1, 1, base, na)) rn = sin(x);
        break;
    case B_COS:
        if (argok("n", 1, 1, base, na)) rn = cos(x);
        break;
    case B_TAN:
        if (argok("n", 1, 1, base, na)) rn = tan(x);
        break;
    case B_ATN:
    case B_ATAN:
        if (argok("n", 1, 1, base, na)) rn = atan(x);
        break;
    case B_ASIN:
        if (argok("n", 1, 1, base, na)) {
            if (x < -1 || x > 1) rt_error(5, "Illegal function call");
            else rn = asin(x);
        }
        break;
    case B_ACOS:
        if (argok("n", 1, 1, base, na)) {
            if (x < -1 || x > 1) rt_error(5, "Illegal function call");
            else rn = acos(x);
        }
        break;
    case B_ATAN2:
        if (argok("nn", 2, 2, base, na)) rn = atan2(x, argstk[base + 2].n);
        break;
    case B_SINH:
        if (argok("n", 1, 1, base, na)) rn = sinh(x);
        break;
    case B_COSH:
        if (argok("n", 1, 1, base, na)) rn = cosh(x);
        break;
    case B_TANH:
        if (argok("n", 1, 1, base, na)) rn = tanh(x);
        break;
    case B_EXP:
        if (argok("n", 1, 1, base, na)) {
            if (x > 709) rt_error(6, "Overflow");
            else rn = exp(x);
        }
        break;
    case B_LOG:
        if (argok("n", 1, 1, base, na)) {
            if (x <= 0) rt_error(5, "Illegal function call");
            else rn = log(x);
        }
        break;
    case B_LOG10:
        if (argok("n", 1, 1, base, na)) {
            if (x <= 0) rt_error(5, "Illegal function call");
            else rn = log10(x);
        }
        break;
    case B_LOG2:
        if (argok("n", 1, 1, base, na)) {
            if (x <= 0) rt_error(5, "Illegal function call");
            else rn = log2_d(x);
        }
        break;
    case B_RND:
        if (na == 0) rn = rnd_next();
        else if (argok("n", 1, 1, base, na)) {
            if (x < 0) {
                rnd_state = fmod(rint_d(fabs(x) * 1000.0), 2147483646.0) + 1.0;
                rn = rnd_next();
            } else if (x == 0) rn = rnd_last;
            else rn = rnd_next();
        }
        break;
    case B_TIMER:
        rn = clock_seconds();
        break;
    /* ---------- strings ---------- */
    case B_LEN:
        if (argok("s", 1, 1, base, na)) rn = (double)s->n;
        break;
    case B_LEFT:
        if (argok("sn", 2, 2, base, na)) {
            n = tol(argstk[base + 2].n);
            if (n < 0) rt_error(5, "Illegal function call");
            else {
                if (n > s->n) n = s->n;
                rs_text(SP(*s), n);
            }
        }
        break;
    case B_RIGHT:
        if (argok("sn", 2, 2, base, na)) {
            n = tol(argstk[base + 2].n);
            if (n < 0) rt_error(5, "Illegal function call");
            else {
                if (n > s->n) n = s->n;
                rs_text(SP(*s) + s->n - n, n);
            }
        }
        break;
    case B_MID:
        if (argok("snn", 2, 3, base, na)) {
            p = tol(argstk[base + 2].n);
            n = s->n - p + 1;
            if (na == 3) {
                m = tol(argstk[base + 3].n);
                if (m < n) n = m;
            }
            if (p < 1 || n < 0) rt_error(5, "Illegal function call");
            else {
                if (n == 0 || p > s->n) rs_text("", 0);
                else {
                    if (p - 1 + n > s->n) n = s->n - p + 1;
                    rs_text(SP(*s) + p - 1, n);
                }
            }
        }
        break;
    case B_STR:
        if (argok("n", 1, 1, base, na)) {
            fmtnum(x, nb);
            if (x >= 0 && dial == 0) {
                memmove(nb + 1, nb, strlen(nb) + 1);
                nb[0] = ' ';
            }
            rs_text(nb, (long)strlen(nb));
        }
        break;
    case B_VAL:
    case B_VALINT:
    case B_VALLNG:
        if (argok("s", 1, 1, base, na)) {
            rn = valparse(s);
            if (id != B_VAL) rn = d_trunc(rn);
        }
        break;
    case B_CHR:
        if (argok("n", 1, 1, base, na)) {
            n = tol(x);
            if (n < 0 || n > 1114111) rt_error(5, "Illegal function call");
            else {
                set_chr(&rs, n);
                rt = 1;
            }
        }
        break;
    case B_ASC:
        if (argok("s", 1, 1, base, na)) {
            if (s->n == 0) rt_error(5, "Illegal function call");
            else rn = (double)(unsigned char)s->p[0];
        }
        break;
    case B_SPACE:
        if (argok("n", 1, 1, base, na)) {
            n = tol(x);
            if (n < 0) rt_error(5, "Illegal function call");
            else if (n > 100000000L) rt_error(7, "Out of memory");
            else {
                s_reserve(&rs, n);
                memset(rs.p, ' ', (size_t)n);
                rs.p[n] = '\0';
                rs.n = n;
                rt = 1;
            }
        }
        break;
    case B_STRING:
        if (argok("n.", 2, 2, base, na)) {
            n = tol(x);
            if (n < 0) rt_error(5, "Illegal function call");
            else {
                char ch;
                ch = ' ';
                if (argstk[base + 2].t == 0) ch = (char)(tol(argstk[base + 2].n) & 255);
                else if (argstk[base + 2].s.n > 0) ch = argstk[base + 2].s.p[0];
                if (n == 0) rs_text("", 0);
                else if (n > 100000000L) rt_error(7, "Out of memory");
                else {
                    s_reserve(&rs, n);
                    memset(rs.p, ch, (size_t)n);
                    rs.p[n] = '\0';
                    rs.n = n;
                    rt = 1;
                }
            }
        }
        break;
    case B_UCASE:
        if (argok("s", 1, 1, base, na)) {
            rs_text(SP(*s), s->n);
            up_str(&rs);
        }
        break;
    case B_LCASE:
        if (argok("s", 1, 1, base, na)) {
            rs_text(SP(*s), s->n);
            for (i = 0; i < rs.n; i++) rs.p[i] = (char)to_low((unsigned char)rs.p[i]);
        }
        break;
    case B_LTRIM:
        if (argok("s.", 1, 2, base, na)) {
            strip_into(&rs, SP(*s), s->n, 'L', ' ');
            rt = 1;
        }
        break;
    case B_RTRIM:
        if (argok("s.", 1, 2, base, na)) {
            strip_into(&rs, SP(*s), s->n, 'T', ' ');
            rt = 1;
        }
        break;
    case B_TRIM:
        if (argok("s.", 1, 2, base, na)) {
            strip_into(&rs, SP(*s), s->n, 'B', ' ');
            rt = 1;
        }
        break;
    case B_INSTR:
        if (na == 2 && argok("ss", 2, 2, base, na)) {
            t = &argstk[base + 2].s;
            if (t->n == 0) rn = 1.0;
            else rn = (double)rx_pos(SP(*t), t->n, SP(*s), s->n, 1);
        } else if (na == 3 && argok("nss", 3, 3, base, na)) {
            t = &argstk[base + 2].s;
            s = &argstk[base + 3].s;
            n = tol(x);
            if (n < 1) rt_error(5, "Illegal function call");
            else if (t->n == 0) {
                if (n <= s->n + 1) rn = (double)n;
                else rn = 0.0;
            } else rn = (double)rx_pos(SP(*s), s->n, SP(*t), t->n, n);
        } else if (na < 2 || na > 3) rt_error(5, "Illegal function call");
        break;
    case B_INSTRREV:
        if (argok("ss.", 2, 3, base, na)) {
            t = &argstk[base + 2].s;
            if (t->n == 0) rn = (double)(s->n + 1);
            else if (na == 3) {
                n = tol(argstk[base + 3].n);
                if (n < 1) rn = 0.0;
                else rn = (double)rx_lastpos(SP(*t), t->n, SP(*s), s->n, n);
            } else rn = (double)rx_lastpos(SP(*t), t->n, SP(*s), s->n, s->n);
        }
        break;
    case B_HEX:
        if (argok("n", 1, 2, base, na)) {
            radixstr(rint_d(x), 4, 16, nb);
            rs_text(nb, (long)strlen(nb));
        }
        break;
    case B_OCT:
        if (argok("n", 1, 2, base, na)) {
            radixstr(rint_d(x), 3, 22, nb);
            rs_text(nb, (long)strlen(nb));
        }
        break;
    case B_BIN:
        if (argok("n", 1, 2, base, na)) {
            char hb[40];
            long hl;
            memset(&tmp, 0, sizeof(tmp));
            radixstr(rint_d(x), 4, 16, hb);
            hl = (long)strlen(hb);
            for (i = 0; i < hl; i++) {
                m = hexval((unsigned char)hb[i]);
                s_catc(&tmp, (m & 8) ? '1' : '0');
                s_catc(&tmp, (m & 4) ? '1' : '0');
                s_catc(&tmp, (m & 2) ? '1' : '0');
                s_catc(&tmp, (m & 1) ? '1' : '0');
            }
            strip_into(&rs, SP(tmp), tmp.n, 'L', '0');
            if (rs.n == 0) s_set(&rs, "0", 1);
            rt = 1;
            s_free(&tmp);
        }
        break;
    case B_SPC:
        if (argok("n", 1, 1, base, na)) {
            n = tol(x);
            if (n < 0) n = 0;
            if (n > 100000000L) rt_error(7, "Out of memory");
            else {
                s_reserve(&rs, n);
                memset(rs.p, ' ', (size_t)n);
                rs.p[n] = '\0';
                rs.n = n;
                rt = 1;
            }
        }
        break;
    case B_TAB:
        if (argok("n", 1, 1, base, na)) {
            n = tol(x) - 1 - out_col;
            if (n < 0) n = 0;
            if (n > 100000000L) rt_error(7, "Out of memory");
            else {
                s_reserve(&rs, n);
                memset(rs.p, ' ', (size_t)n);
                rs.p[n] = '\0';
                rs.n = n;
                rt = 1;
            }
        }
        break;
    case B_POW:
        if (argok("nn", 2, 2, base, na)) rn = pow(x, argstk[base + 2].n);
        break;
    case B_MAX:
        if (argok("nn", 2, 2, base, na)) {
            rn = x;
            if (argstk[base + 2].n > x) rn = argstk[base + 2].n;
        }
        break;
    case B_MIN:
        if (argok("nn", 2, 2, base, na)) {
            rn = x;
            if (argstk[base + 2].n < x) rn = argstk[base + 2].n;
        }
        break;
    case B_ISNUMERIC:
        if (argok("s", 1, 1, base, na)) {
            y = valparse(s);
            (void)y;
            if (val_ok == 1) rn = -1.0;
            else rn = 0.0;
        }
        break;
    case B_IIF:
        if (argok("n..", 3, 3, base, na)) {
            if (x != 0) k = base + 2;
            else k = base + 3;
            rt = argstk[k].t;
            rn = argstk[k].n;
            s_cpy(&rs, &argstk[k].s);
        }
        break;
    /* ---------- system ---------- */
    case B_DATE:
    case B_TIME: {
        time_t tt;
        struct tm *lt;
        tt = time(NULL);
        lt = localtime(&tt);
        if (lt == NULL) nb[0] = '\0';
        else if (id == B_DATE) strftime(nb, sizeof(nb), "%m-%d-%Y", lt);
        else strftime(nb, sizeof(nb), "%H:%M:%S", lt);
        rs_text(nb, (long)strlen(nb));
        break;
    }
    case B_ERR:
        rn = (double)err_code;
        break;
    case B_ERL:
        rn = (double)err_line;
        break;
    case B_FRE:
        rn = 1000000.0;
        break;
    case B_PEEK:
        rn = 0.0;
        break;
    case B_CSRLIN:
        rn = 1.0;
        break;
    case B_POS:
        rn = (double)(out_col + 1);
        break;
    case B_COMMAND:
        if (na == 0) s_cpy(&rs, &cmdline);
        else {
            n = tol(x);
            if (n < 1) s_set(&rs, prog_name, (long)strlen(prog_name));
            else rx_word(&rs, SP(cmdline), cmdline.n, n);
        }
        rt = 1;
        break;
    case B_ENVIRON:
        if (argok("s", 1, 1, base, na)) {
            const char *ev;
            ev = getenv(SP(*s));
            if (ev == NULL) ev = "";
            rs_text(ev, (long)strlen(ev));
        }
        break;
    case B_CURDIR:
        rs_text(".", 1);
        break;
    case B_FREEFILE:
        rn = 0.0;
        for (i = 1; i <= NFILES; i++) {
            if (fl_mode[i] == 0) {
                rn = (double)i;
                break;
            }
        }
        break;
    case B_EOF:
        if (argok("n", 1, 1, base, na)) rn = (double)file_eof(tol(x));
        break;
    case B_LOF:
    case B_LOC:
    case B_SEEK:
        rn = 0.0;
        break;
    case B_INKEY: {
        int c;
        flush_out();
        c = getc(stdin);
        if (c == EOF) rs_text("", 0);
        else {
            char ch;
            ch = (char)c;
            rs_text(&ch, 1);
        }
        break;
    }
    case B_INPUT:
        if (argok("n.", 1, 2, base, na)) {
            FILE *fh;
            int c;
            n = tol(x);
            fh = stdin;
            if (na == 2) {
                k = tol(argstk[base + 2].n);
                if (k < 1 || k > 255 || fl_mode[k] != 1) {
                    rt_error(52, "Bad file number");
                    return;
                }
                fh = fl_fp[k];
            } else flush_out();
            s_clr(&rs);
            for (i = 1; i <= n; i++) {
                c = getc(fh);
                if (c == EOF) {
                    rt_error(62, "Input past end of file");
                    break;
                }
                s_catc(&rs, c);
            }
            rt = 1;
        }
        break;
    default:
        {
            char eb[160];
            strcpy(eb, "Illegal function call (");
            strncat(eb, nme, 60);
            strcat(eb, ")");
            rt_error(5, eb);
        }
        break;
    }
}
