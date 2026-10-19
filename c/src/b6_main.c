/* b6_main.c - running a program, token dump, interactive mode, main */

static void run_program(void)
{
    halted = 0;
    err_flag = 0;
    pc = 1;
    gsp = 0;
    fsp = 0;
    ssp = 0;
    svp = 0;
    cfp = 0;
    ab_cnt = 0;
    dptr = 1;
    on_err = 0;
    redo_flag = 0;
    retflag = 0;
    out_file = 0;
    out_col = 0;
    running = 1;
    run_loop();
    close_all();
    flush_out();
    running = 0;
}

static void dump_tokens(void)
{
    long p0;
    char nb[64];
    for (p0 = 1; p0 <= ntk; p0++) {
        printf("%ld L%ld T%d K%ld F%ld X%ld Y%ld ", p0, tk[p0].l, tk[p0].t, tk[p0].k, tk[p0].f, tk[p0].x, tk[p0].y);
        if (tk[p0].t == 1) {
            fmtnum(tk[p0].num, nb);
            printf("%s\n", nb);
        } else printf("[%s]\n", tk[p0].s);
    }
    printf("lines %ld tokens %ld vars %ld procs %ld data %ld\n", nlines, ntk, nvars, np, nd);
    for (p0 = 1; p0 <= nvars; p0++)
        printf("var %ld %s kind %d proc %ld\n", p0, vars[p0].name, vars[p0].kind, vars[p0].proc);
}

/* ---------- interactive mode ---------- */
static void repl_store(long num, const Str *txt, const Str *rest)
{
    long i, j;
    Str t;
    int blank;
    memset(&t, 0, sizeof(t));
    strip_into(&t, SP(*rest), rest->n, 'B', ' ');
    blank = t.n == 0;
    s_free(&t);
    i = 1;
    while (i <= rp_cnt) {
        if (rp[i].num >= num) break;
        i++;
    }
    if (i <= rp_cnt && rp[i].num == num) {
        if (blank) {
            s_free(&rp[i].txt);
            for (j = i; j < rp_cnt; j++) rp[j] = rp[j + 1];
            memset(&rp[rp_cnt], 0, sizeof(RepLine));
            rp_cnt--;
        } else s_cpy(&rp[i].txt, txt);
        return;
    }
    if (blank) return;
    NEED(rp, rp_cap, rp_cnt + 3);
    for (j = rp_cnt; j >= i; j--) rp[j + 1] = rp[j];
    memset(&rp[i], 0, sizeof(RepLine));
    rp[i].num = num;
    s_cpy(&rp[i].txt, txt);
    rp_cnt++;
}

static void repl_load(const char *fn)
{
    FILE *f;
    Str line;
    int e;
    memset(&line, 0, sizeof(line));
    f = fopen(fn, "r");
    if (f == NULL) {
        printf("File not found\n");
        return;
    }
    rp_cnt = 0;
    for (;;) {
        e = read_line(f, &line);
        if (e > 0 && line.n == 0) break;
        rp_cnt++;
        NEED(rp, rp_cap, rp_cnt + 3);
        rp[rp_cnt].num = rp_cnt * 10;
        if (line.n > 0 && line.p[line.n - 1] == '\r') line.n--, line.p[line.n] = '\0';
        s_cpy(&rp[rp_cnt].txt, &line);
    }
    fclose(f);
    s_free(&line);
}

static void repl_run(void)
{
    long i;
    int ok;
    reset_program();
    for (i = 1; i <= rp_cnt; i++) add_line(SP(rp[i].txt), rp[i].txt.n);
    ok = link_program();
    if (ok == 1) {
        run_program();
        pc = 1;
    }
}

static void repl(void)
{
    Str line, u, rest;
    long num, i, j;
    int ok;
    memset(&line, 0, sizeof(line));
    memset(&u, 0, sizeof(u));
    memset(&rest, 0, sizeof(rest));
    rp_cnt = 0;
    in_repl = 1;
    reset_program();
    outz("CREXX BASIC.  Commands: RUN LIST NEW LOAD \"file\" SYSTEM\n");
    for (;;) {
        outz("Ok\n");
        err_flag = 0;
        read_text_line(0, &rest);
        if (err_flag == 1) break;
        strip_into(&line, SP(rest), rest.n, 'B', ' ');
        if (line.n == 0) continue;
        s_cpy(&u, &line);
        up_str(&u);
        if (is_digit((unsigned char)line.p[0])) {
            double dv;
            char nb[64];
            j = 1;
            while (j <= line.n && is_digit((unsigned char)line.p[j - 1])) j++;
            i = j - 1 > 40 ? 40 : j - 1;
            memcpy(nb, line.p, (size_t)i);
            nb[i] = '\0';
            dv = strtod(nb, NULL);
            num = dv > 2000000000.0 ? 2000000000L : (long)dv;
            s_set(&rest, line.p + j - 1, line.n - j + 1);
            repl_store(num, &line, &rest);
            continue;
        }
        if (s_eq(&u, "SYSTEM") || s_eq(&u, "QUIT") || s_eq(&u, "EXIT")) break;
        if (s_eq(&u, "NEW")) {
            rp_cnt = 0;
            reset_program();
            continue;
        }
        if (s_eq(&u, "RUN")) {
            repl_run();
            continue;
        }
        if (s_eq(&u, "LIST")) {
            for (i = 1; i <= rp_cnt; i++) {
                outs(SP(rp[i].txt), rp[i].txt.n);
                outs("\n", 1);
            }
            continue;
        }
        if (u.n >= 5 && memcmp(u.p, "LOAD ", 5) == 0) {
            Str fn;
            long a, b;
            memset(&fn, 0, sizeof(fn));
            a = 5;
            b = line.n;
            while (a < b && line.p[a] == '"') a++;
            while (b > a && line.p[b - 1] == '"') b--;
            s_set(&fn, line.p + 5, 0);
            s_set(&fn, line.p + a, b - a);
            repl_load(SP(fn));
            s_free(&fn);
            continue;
        }
        /* immediate statement */
        add_line(SP(line), line.n);
        ok = link_program();
        if (ok == 1) {
            halted = 0;
            err_flag = 0;
            retflag = 0;
            pc = ln_start[nlines];
            run_loop();
            flush_out();
        }
    }
    close_all();
    flush_out();
    s_free(&line);
    s_free(&u);
    s_free(&rest);
}

static void usage(void)
{
    printf("BASIC interpreter in C89 (port of basic.crexx)\n");
    printf("usage: basic [-fb|-ms] [-ansi] [-dump] program.bas [args...]\n");
    printf("       basic            (interactive)\n");
    printf("  -ms    MS BASIC output style (default): numbers print as ' 5 '\n");
    printf("  -fb    FreeBASIC output style: numbers print as ' 5'\n");
    printf("  -ansi  CLS / LOCATE / COLOR emit ANSI terminal escape codes\n");
    printf("  -dump  list the token stream instead of running\n");
}

int main(int argc, char **argv)
{
    int ia, ok;
    const char *fname;
    Str uo;
    memset(&uo, 0, sizeof(uo));
    init_state();
    NEED(tk, tk_cap, 16);
    NEED(ln_num, ln_cap, 16);
    NEED(ln_start, ln_scap, 16);
    NEED(vars, vars_cap, 16);
    NEED(procs, np_cap, 16);
    NEED(pparms, npp_cap, 16);
    NEED(fns_def, nfn_cap, 16);
    NEED(dat, nd_cap, 16);
    NEED(pl_slots, pl_slots_cap, 16);
    NEED(fstack, fstack_cap, 16);
    NEED(g_ret, g_ret_cap, 16);
    NEED(sstack, sstack_cap, 16);
    NEED(svstack, svstack_cap, 16);
    NEED(cf_gsp, cf_cap, 16);
    NEED(cf_proc, cf_pcap, 16);
    NEED(argstk, ab_cap, 16);
    NEED(pool_n, pool_n_cap, 16);
    NEED(pool_s, pool_s_cap, 16);
    NEED(inp_f, inp_cap, 16);
    NEED(rp, rp_cap, 16);
    fname = NULL;
    prog_name = "";
    for (ia = 1; ia < argc; ia++) {
        const char *oa;
        oa = argv[ia];
        if (fname == NULL && oa[0] == '-' && oa[1] != '\0') {
            s_set(&uo, oa, (long)strlen(oa));
            up_str(&uo);
            if (s_eq(&uo, "-FB")) dial = 1;
            else if (s_eq(&uo, "-MS") || s_eq(&uo, "-QB")) dial = 0;
            else if (s_eq(&uo, "-DUMP")) dbg = 1;
            else if (s_eq(&uo, "-ANSI")) ansi = 1;
            else if (s_eq(&uo, "-H") || s_eq(&uo, "-HELP") || s_eq(&uo, "-?")) {
                usage();
                return 0;
            } else {
                printf("basic: unknown option %s\n", oa);
                usage();
                return 1;
            }
        } else if (fname == NULL) fname = oa;
        else {
            if (cmdline.n > 0) s_catc(&cmdline, ' ');
            s_cats(&cmdline, oa);
        }
    }
    if (fname == NULL) repl();
    else {
        prog_name = (char *)fname;
        ok = load_program(fname);
        if (ok == 1) ok = link_program();
        if (ok == 1) {
            if (dbg == 1) dump_tokens();
            else run_program();
        } else {
            fflush(stdout);
            return 1;
        }
    }
    fflush(stdout);
    return (int)rc_exit;
}
