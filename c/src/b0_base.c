/* b0_base.c - includes, types and global state (see README.md) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#ifdef BASIC_POSIX
#include <sys/time.h>
#include <unistd.h>
#endif

#define NFILES 255
#define MAXPREC 40
#define M32 0xFFFFFFFFUL
#define BIGKIND 100000000L

/* a growable byte string (always NUL terminated once allocated) */
typedef struct {
    char *p;
    long n;
    long cap;
} Str;

typedef struct {
    int t;              /* 1 NUM 2 STR 3 ID 4 KW 5 OP 6 EOL 7 END 8 TYP */
    double num;
    char *s;
    long sl;
    long k, f, x, y, l;
} Tok;

typedef struct {
    char *name;
    int kind;           /* 0 number, 1 integer, 2 string */
    double n;
    Str s;
    int isarr;
    int nd;
    long base;
    long lo[3];
    long hi[3];
    long proc;
    int cnst, shared, stat;
} Var;

typedef struct {
    char *name;
    long pc, body, end, isfn, ret, pfirst, pcnt, first, cnt;
} Proc;

typedef struct {
    long slot, ref, arr, kind;
} PParm;

typedef struct {
    char *name;
    long pc;
} FnDef;

typedef struct {
    char *s;
    long sl;
    long q;
    long line;
} DataItem;

typedef struct {
    long slot;
    double end, step;
    long pc, forp;
} ForEnt;

typedef struct {
    double n;
    Str s;
    int t;
} SelEnt;

typedef struct {
    double n;
    Str s;
    long base;
    int isarr, nd;
    long lo[3], hi[3];
} SvEnt;

typedef struct {
    double n;
    Str s;
    int t;
    long slot, idx;
} ArgEnt;

typedef struct {
    char *key;
    long val;
} HEnt;

typedef struct {
    HEnt *tab;
    long cap;
    long cnt;
} HMap;

typedef struct {
    unsigned long hi, lo;
} I64;

typedef struct {
    long num;
    Str txt;
} RepLine;

/* ---------- program text and tokens ---------- */
static Tok *tk;
static long ntk, tk_cap;
static long *ln_num, *ln_start;
static long nlines, ln_cap, ln_scap;
static HMap lmap, nm;
static Var *vars;
static long nvars, vars_cap;
static double *pool_n;
static Str *pool_s;
static long npn, nps, pool_n_cap, pool_s_cap;
static Proc *procs;
static long np, np_cap;
static PParm *pparms;
static long npp, npp_cap;
static long *pl_slots, pl_slots_cap;
static long cur_proc;
static FnDef *fns_def;
static long nfn, nfn_cap;
static DataItem *dat;
static long nd, nd_cap, dptr;

/* ---------- run time state ---------- */
static long pc, stmt_pc, mstmt, res_pc;
static int halted, retflag, err_flag, redo_flag, rc_exit;
static long err_code, err_line, on_err;
static char last_error_text[300];
static ForEnt *fstack;
static long fsp, fstack_cap;
static long *g_ret;
static long gsp, g_ret_cap;
static SelEnt *sstack;
static long ssp, sstack_cap;
static SvEnt *svstack;
static long svp, svstack_cap;
static long *cf_gsp, *cf_proc;
static long cfp, cf_cap, cf_pcap;
static ArgEnt *argstk;
static long ab_cnt, ab_cap;
static double rn;
static Str rs;
static int rt;
static long out_col, out_file;
static FILE *fl_fp[NFILES + 1];
static int fl_mode[NFILES + 1];
static int fl_eof[NFILES + 1];
static int fl_has[NFILES + 1];
static Str fl_pend[NFILES + 1];
static int dial;
static double rnd_state, rnd_last;
static long optbase;
static int dbg, ansi;
static int defkind[27];
static int prec[MAXPREC + 1];
static Str cmdline;
static char *prog_name;
static long pw_end;
static int val_ok;
static int bare_flag;
static long lv_slot, lv_idx;
static Str *inp_f;
static long inp_n, inp_cap;
static long link_errs;
static int running, in_repl;
static RepLine *rp;
static long rp_cnt, rp_cap;

static char kws[700];
static char fnl[600];
static const char *nopar =
 " TIMER DATE TIME RND INKEY ERR ERL FRE POS CSRLIN COMMAND CURDIR FREEFILE ";

#define LETTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define LOWERS "abcdefghijklmnopqrstuvwxyz"
#define DIGITS "0123456789"
#define SP(s) ((s).p ? (s).p : "")
