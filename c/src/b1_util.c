/* b1_util.c - memory, strings, hash maps, number formatting, 64-bit helpers */

static void die_mem(void)
{
    fflush(stdout);
    fputs("basic: out of memory\n", stderr);
    exit(2);
}

static void *xalloc(long n)
{
    void *p;
    p = malloc((size_t)(n > 0 ? n : 1));
    if (p == NULL) die_mem();
    return p;
}

static char *xstrdup(const char *s, long n)
{
    char *p;
    p = (char *)xalloc(n + 1);
    if (n > 0) memcpy(p, s, (size_t)n);
    p[n] = '\0';
    return p;
}

/* grow a zero-filled array so that index need-1 is valid; *cap is in elements */
static void *grow_arr(void *arr, long *cap, long need, long esz)
{
    long nc;
    void *p;
    if (need <= *cap) return arr;
    nc = *cap < 16 ? 16 : *cap;
    while (nc < need) nc = nc + nc / 2 + 8;
    p = realloc(arr, (size_t)nc * (size_t)esz);
    if (p == NULL) die_mem();
    memset((char *)p + (size_t)*cap * (size_t)esz, 0, (size_t)(nc - *cap) * (size_t)esz);
    *cap = nc;
    return p;
}

#define NEED(arr, cap, idx) \
    do { if ((idx) >= (cap)) (arr) = grow_arr((arr), &(cap), (idx) + 1, (long)sizeof(*(arr))); } while (0)

/* ---------- strings ---------- */
static void s_reserve(Str *d, long n)
{
    long nc;
    char *p;
    if (n + 1 <= d->cap) return;
    nc = d->cap < 32 ? 32 : d->cap;
    while (nc < n + 1) nc = nc + nc / 2 + 16;
    p = (char *)realloc(d->p, (size_t)nc);
    if (p == NULL) die_mem();
    d->p = p;
    d->cap = nc;
}

static void s_set(Str *d, const char *p, long n)
{
    if (n < 0) n = 0;
    if (d->p != NULL && p >= d->p && p < d->p + d->cap && n + 1 <= d->cap) {
        memmove(d->p, p, (size_t)n);
        d->p[n] = '\0';
        d->n = n;
        return;
    }
    s_reserve(d, n);
    if (n > 0) memcpy(d->p, p, (size_t)n);
    d->p[n] = '\0';
    d->n = n;
}

static void s_cpy(Str *d, const Str *s)
{
    if (d == s) return;
    s_set(d, SP(*s), s->n);
}

static void s_cat(Str *d, const char *p, long n)
{
    if (n <= 0) return;
    s_reserve(d, d->n + n);
    memcpy(d->p + d->n, p, (size_t)n);
    d->n += n;
    d->p[d->n] = '\0';
}

static void s_cats(Str *d, const char *z)
{
    s_cat(d, z, (long)strlen(z));
}

static void s_catc(Str *d, int c)
{
    char ch;
    ch = (char)c;
    s_cat(d, &ch, 1);
}

static void s_clr(Str *d)
{
    s_set(d, "", 0);
}

static void s_free(Str *d)
{
    if (d->p != NULL) free(d->p);
    d->p = NULL;
    d->n = 0;
    d->cap = 0;
}

static int s_eq(const Str *a, const char *z)
{
    long n;
    n = (long)strlen(z);
    return a->n == n && (n == 0 || memcmp(a->p, z, (size_t)n) == 0);
}

/* strict comparison: bytewise, a proper prefix is smaller */
static int cmp_mem(const char *a, long an, const char *b, long bn)
{
    long m;
    int c;
    m = an < bn ? an : bn;
    if (m > 0) {
        c = memcmp(a, b, (size_t)m);
        if (c != 0) return c < 0 ? -1 : 1;
    }
    if (an < bn) return -1;
    if (an > bn) return 1;
    return 0;
}

static int cmp_str(const Str *a, const Str *b)
{
    return cmp_mem(SP(*a), a->n, SP(*b), b->n);
}

static int in_set(int c, const char *set)
{
    return c != 0 && strchr(set, c) != NULL;
}

static int is_digit(int c)
{
    return in_set(c, DIGITS);
}

static int is_letter(int c)
{
    if (c == '_') return 1;
    return in_set(c, LETTERS) || in_set(c, LOWERS);
}

static int is_alnum(int c)
{
    return is_letter(c) || is_digit(c) || c == '.';
}

static int to_up(int c)
{
    const char *q;
    q = c != 0 ? strchr(LOWERS, c) : NULL;
    if (q != NULL) return LETTERS[q - LOWERS];
    return c;
}

static int to_low(int c)
{
    const char *q;
    q = c != 0 ? strchr(LETTERS, c) : NULL;
    if (q != NULL) return LOWERS[q - LETTERS];
    return c;
}

/* 1..26 for a letter (either case), else 0 */
static int letter_index(int c)
{
    const char *q;
    c = to_up(c);
    q = c != 0 ? strchr(LETTERS, c) : NULL;
    if (q != NULL) return (int)(q - LETTERS) + 1;
    return 0;
}

static void up_str(Str *d)
{
    long i;
    for (i = 0; i < d->n; i++) d->p[i] = (char)to_up((unsigned char)d->p[i]);
}

/* strip blanks: mode 'L', 'T' or 'B'; ch is the character to remove */
static void strip_into(Str *d, const char *p, long n, int mode, int ch)
{
    long a, b;
    a = 0;
    b = n;
    if (mode == 'L' || mode == 'B')
        while (a < b && (unsigned char)p[a] == (unsigned char)ch) a++;
    if (mode == 'T' || mode == 'B')
        while (b > a && (unsigned char)p[b - 1] == (unsigned char)ch) b--;
    s_set(d, p + a, b - a);
}

/* 1-based position of needle in hay starting at start, or 0 */
static long rx_pos(const char *ndl, long nl, const char *hay, long hl, long start)
{
    long i;
    if (nl <= 0) return 0;
    if (start < 1) start = 1;
    for (i = start - 1; i + nl <= hl; i++)
        if (hay[i] == ndl[0] && memcmp(hay + i, ndl, (size_t)nl) == 0) return i + 1;
    return 0;
}

/* last position of needle within the first lim characters of hay */
static long rx_lastpos(const char *ndl, long nl, const char *hay, long hl, long lim)
{
    long i;
    if (nl <= 0) return 0;
    if (lim > hl) lim = hl;
    for (i = lim - nl; i >= 0; i--)
        if (hay[i] == ndl[0] && memcmp(hay + i, ndl, (size_t)nl) == 0) return i + 1;
    return 0;
}

/* word position of w in a blank separated list, 0 when absent */
static long wordpos(const char *w, const char *list)
{
    long wl, idx;
    const char *p;
    const char *q;
    wl = (long)strlen(w);
    if (wl == 0) return 0;
    idx = 0;
    p = list;
    for (;;) {
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        q = p;
        while (*q != ' ' && *q != '\0') q++;
        idx++;
        if (q - p == wl && memcmp(p, w, (size_t)wl) == 0) return idx;
        p = q;
    }
}

/* n-th blank separated word of s into d */
static void rx_word(Str *d, const char *s, long sl, long n)
{
    long i, a, cnt;
    i = 0;
    cnt = 0;
    while (i < sl) {
        while (i < sl && s[i] == ' ') i++;
        if (i >= sl) break;
        a = i;
        while (i < sl && s[i] != ' ') i++;
        cnt++;
        if (cnt == n) {
            s_set(d, s + a, i - a);
            return;
        }
    }
    s_clr(d);
}

/* ---------- hash map: string -> positive long (0 = absent) ---------- */
static unsigned long hash_str(const char *s)
{
    unsigned long h;
    h = 5381UL;
    while (*s != '\0') {
        h = (h * 33UL + (unsigned long)(unsigned char)*s) & M32;
        s++;
    }
    return h;
}

static void hm_clear(HMap *m)
{
    long i;
    for (i = 0; i < m->cap; i++)
        if (m->tab[i].key != NULL) free(m->tab[i].key);
    if (m->tab != NULL) free(m->tab);
    m->tab = NULL;
    m->cap = 0;
    m->cnt = 0;
}

static long hm_get(HMap *m, const char *key)
{
    unsigned long h;
    if (m->cap == 0) return 0;
    h = hash_str(key) % (unsigned long)m->cap;
    while (m->tab[h].key != NULL) {
        if (strcmp(m->tab[h].key, key) == 0) return m->tab[h].val;
        h = (h + 1) % (unsigned long)m->cap;
    }
    return 0;
}

static void hm_set(HMap *m, const char *key, long val);

static void hm_rehash(HMap *m)
{
    HEnt *old;
    long oc, i;
    old = m->tab;
    oc = m->cap;
    m->cap = oc == 0 ? 64 : oc * 2;
    m->tab = (HEnt *)xalloc(m->cap * (long)sizeof(HEnt));
    memset(m->tab, 0, (size_t)m->cap * sizeof(HEnt));
    m->cnt = 0;
    for (i = 0; i < oc; i++) {
        if (old[i].key != NULL) {
            hm_set(m, old[i].key, old[i].val);
            free(old[i].key);
        }
    }
    if (old != NULL) free(old);
}

static void hm_set(HMap *m, const char *key, long val)
{
    unsigned long h;
    if (m->cnt * 2 >= m->cap) hm_rehash(m);
    h = hash_str(key) % (unsigned long)m->cap;
    while (m->tab[h].key != NULL) {
        if (strcmp(m->tab[h].key, key) == 0) {
            m->tab[h].val = val;
            return;
        }
        h = (h + 1) % (unsigned long)m->cap;
    }
    m->tab[h].key = xstrdup(key, (long)strlen(key));
    m->tab[h].val = val;
    m->cnt++;
}

/* ---------- numbers ---------- */
static double d_trunc(double x)
{
    return x < 0 ? ceil(x) : floor(x);
}

/* round half to even, clamped to +-9.2e18 (NaN gives 0): CREXX toint as a double */
static double rint_d(double x)
{
    double r, d;
    if (x != x) return 0.0;
    if (x > 9.2e18) return 9.2e18;
    if (x < -9.2e18) return -9.2e18;
    r = floor(x);
    d = x - r;
    if (d > 0.5) r = r + 1.0;
    else if (d == 0.5) {
        if (fmod(r, 2.0) != 0.0) r = r + 1.0;
    }
    return r;
}

static double fix_d(double x)
{
    if (x != x) return 0.0;
    if (x > 9.2e18) return 9.2e18;
    if (x < -9.2e18) return -9.2e18;
    return d_trunc(x);
}

/* integer value for counts and indexes, limited to +-1e9 (fits a 32-bit long) */
static long tol(double x)
{
    double r;
    r = rint_d(x);
    if (r > 1e9) return 1000000000L;
    if (r < -1e9) return -1000000000L;
    return (long)r;
}

static double log2_d(double x)
{
    int e;
    double m;
    m = frexp(x, &e);
    if (m == 0.5) return (double)(e - 1);
    return log(x) / log(2.0);
}

/* exact decimal digits of a non-negative integral double (any size) */
static void int_digits(double v, char *out)
{
    char tmp[400];
    long n, i;
    double d;
    if (v < 1e15) {
        n = 0;
        if (v == 0.0) tmp[n++] = '0';
        while (v > 0.0) {
            d = fmod(v, 10.0);
            tmp[n++] = DIGITS[(int)d];
            v = (v - d) / 10.0;
        }
    } else {
        /* v = m * 2^e with m an integer below 2^53: double a decimal digit array */
        int e;
        double m;
        unsigned char dig[400];
        long nd2, j, c, t;
        m = frexp(v, &e);
        m = ldexp(m, 53);
        e = e - 53;
        nd2 = 0;
        while (m > 0.0) {
            d = fmod(m, 10.0);
            dig[nd2++] = (unsigned char)d;
            m = (m - d) / 10.0;
        }
        while (e > 0 && nd2 < 390) {
            c = 0;
            for (j = 0; j < nd2; j++) {
                t = dig[j] * 2 + c;
                dig[j] = (unsigned char)(t % 10);
                c = t / 10;
            }
            if (c) dig[nd2++] = (unsigned char)c;
            e--;
        }
        n = 0;
        for (j = 0; j < nd2; j++) tmp[n++] = DIGITS[dig[j]];
    }
    for (i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
}

static void trim_zeros(char *s)
{
    char *dot, *end;
    dot = strchr(s, '.');
    if (dot == NULL) return;
    end = s + strlen(s) - 1;
    while (end > dot && *end == '0') {
        *end = '\0';
        end--;
    }
    if (end == dot) *end = '\0';
}

/* the CREXX float -> string conversion (15 significant digits, numeric digits 18) */
static void fmt_float(double x, char *out)
{
    char coef[64];
    char *a;
    const char *cs;
    int neg;
    long ex, dc, i, ne;
    double ax, c;

    neg = x < 0.0;
    ax = fabs(x);
    ex = (long)floor(log10(ax));
    c = ax / pow(10.0, (double)ex);
    if (c >= 10.0) {
        c /= 10.0;
        ex += 1;
    }
    if (c < 1.0) {
        c *= 10.0;
        ex -= 1;
    }
    sprintf(coef, neg ? "-%.14f" : "%.14f", c);
    a = neg ? coef + 1 : coef;
    if (strncmp(a, "10.", 3) == 0) {
        a[2] = a[1];
        a[1] = '.';
        ex++;
    } else if (strncmp(a, "0.", 2) == 0) {
        if (a[2] != '\0') {
            a[0] = a[2];
            memmove(a + 2, a + 3, strlen(a + 3) + 1);
            ex--;
        }
    }
    trim_zeros(coef);
    if (ex == 0) {
        strcpy(out, coef);
        return;
    }
    if (ex + 1 > 18 || ex < -6) {
        sprintf(out, "%sE%+ld", coef, ex);
        return;
    }
    cs = coef;
    if (cs[0] == '-') {
        *out++ = '-';
        cs++;
    }
    dc = (long)strlen(cs);
    if (dc > 1) dc--;
    if (ex > 0) {
        if (ex < dc) {
            *out++ = cs[0];
            if (cs[1] != '\0') {
                memcpy(out, cs + 2, (size_t)ex);
                out += ex;
            }
            if (cs[ex + 2] != '\0') {
                *out++ = '.';
                strcpy(out, cs + ex + 2);
            } else {
                *out = '\0';
            }
        } else {
            out[0] = cs[0];
            if (cs[1] != '\0') strcpy(out + 1, cs + 2);
            ne = ex - dc + 1;
            for (i = 0; i < ne; i++) out[i + dc] = '0';
            out[ne + dc] = '\0';
        }
    } else {
        strcpy(out, "0.");
        for (i = 0; i < -ex - 1; i++) out[i + 2] = '0';
        out[-ex + 1] = cs[0];
        for (i = 1; i < dc; i++) out[-ex + 1 + i] = cs[i + 1];
        out[-ex + 1 + dc] = '\0';
    }
}

/* number to text as BASIC prints it, without the sign space (CREXX fmtnum) */
static void fmtnum(double x, char *out)
{
    char *o;
    if (x == 0.0) {
        strcpy(out, "0");
        return;
    }
    if (x != x) {
        strcpy(out, "nan");
        return;
    }
    if (x > DBL_MAX) {
        strcpy(out, "inf");
        return;
    }
    if (x < -DBL_MAX) {
        strcpy(out, "-inf");
        return;
    }
    if (fabs(x) < 1e15 && floor(x) == x) {
        o = out;
        if (x < 0.0) *o++ = '-';
        int_digits(fabs(x), o);
        return;
    }
    fmt_float(x, out);
}

/* text to number; the text is digits [. digits] [E [sign] digits] */
static double numtofloat(const char *w)
{
    char t[400];
    char u[400];
    long n, p, tl;
    n = (long)strlen(w);
    if (n > 380) n = 380;
    t[0] = '\0';
    if (w[0] == '.') strcpy(t, "0");
    strncat(t, w, (size_t)n);
    tl = (long)strlen(t);
    p = rx_pos(".", 1, t, tl, 1);
    if (p > 0) {
        if (p == tl) strcat(t, "0");
        else if (t[p] == 'e' || t[p] == 'E') {
            memcpy(u, t, (size_t)p);
            u[p] = '\0';
            strcat(u, "0");
            strcat(u, t + p);
            strcpy(t, u);
        }
    }
    return strtod(t, NULL);
}

/* ---------- 64-bit two's complement on 32-bit halves ---------- */
static I64 i64_from(double d)
{
    I64 r;
    double m, h;
    int neg;
    neg = d < 0.0;
    m = fabs(d);
    h = floor(m / 4294967296.0);
    r.hi = (unsigned long)h;
    r.lo = (unsigned long)(m - h * 4294967296.0);
    r.hi &= M32;
    r.lo &= M32;
    if (neg) {
        r.lo = (~r.lo + 1UL) & M32;
        r.hi = (~r.hi + (r.lo == 0UL ? 1UL : 0UL)) & M32;
    }
    return r;
}

static double i64_to(I64 a)
{
    I64 b;
    if (a.hi & 0x80000000UL) {
        b.lo = (~a.lo + 1UL) & M32;
        b.hi = (~a.hi + (b.lo == 0UL ? 1UL : 0UL)) & M32;
        return -((double)b.hi * 4294967296.0 + (double)b.lo);
    }
    return (double)a.hi * 4294967296.0 + (double)a.lo;
}

static I64 i64_and(I64 a, I64 b)
{
    I64 r;
    r.hi = a.hi & b.hi;
    r.lo = a.lo & b.lo;
    return r;
}

static I64 i64_or(I64 a, I64 b)
{
    I64 r;
    r.hi = a.hi | b.hi;
    r.lo = a.lo | b.lo;
    return r;
}

static I64 i64_xor(I64 a, I64 b)
{
    I64 r;
    r.hi = a.hi ^ b.hi;
    r.lo = a.lo ^ b.lo;
    return r;
}

static I64 i64_not(I64 a)
{
    I64 r;
    r.hi = ~a.hi & M32;
    r.lo = ~a.lo & M32;
    return r;
}

static I64 i64_shl1(I64 a)
{
    I64 r;
    r.hi = ((a.hi << 1) | (a.lo >> 31)) & M32;
    r.lo = (a.lo << 1) & M32;
    return r;
}

static I64 i64_sar1(I64 a)
{
    I64 r;
    r.lo = ((a.lo >> 1) | ((a.hi & 1UL) << 31)) & M32;
    r.hi = ((a.hi >> 1) | (a.hi & 0x80000000UL)) & M32;
    return r;
}

static int i64_isneg(I64 a)
{
    return (a.hi & 0x80000000UL) != 0UL;
}

static int i64_iszero(I64 a)
{
    return a.hi == 0UL && a.lo == 0UL;
}

/* digits of n in base 2**bits (nd0 digits of the 64-bit two's complement for negatives) */
static void radixstr(double n, int bits, int nd0, char *out)
{
    I64 a;
    char tmp[80];
    long k, i, b;
    unsigned long msk, d;
    a = i64_from(n);
    msk = (1UL << bits) - 1UL;
    k = 0;
    if (!i64_isneg(a)) {
        if (i64_iszero(a)) {
            strcpy(out, "0");
            return;
        }
        while (!i64_iszero(a)) {
            d = a.lo & msk;
            tmp[k++] = "0123456789ABCDEF"[d];
            for (b = 0; b < bits; b++) a = i64_sar1(a);
        }
    } else {
        for (i = 0; i < nd0; i++) {
            d = a.lo & msk;
            tmp[k++] = "0123456789ABCDEF"[d];
            for (b = 0; b < bits; b++) a = i64_sar1(a);
        }
    }
    for (i = 0; i < k; i++) out[i] = tmp[k - 1 - i];
    out[k] = '\0';
}
