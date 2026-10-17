/**
 * @file fs2.c
 * @brief Core faststring primitives.
 */

#include <stdatomic.h>
#include <stdarg.h>

#include "fs2.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────── */

static const char *const        versions[] = { 
    FS2_VERSION, 
    NULL 
};

static const size_t             FS_TECH_PRINT_COUNT     = 100; // symplos to print
static const size_t             FS_MIN_ALLOC            = 16;

const char *const              *fs2_versions(void) { return versions; }
const char                     *fs2_version (void) { return versions[0]; }

// mem leak checking
static atomic_int               g_alloc_cnt             = 0;
static atomic_int               g_free_cnt              = 0;
static atomic_int               g_alloc_body_cnt        = 0;
static atomic_int               g_free_body_cnt         = 0;

// ------------------------------ Utilities --------------------------------

#define FS2_TRACE

static inline void fs2_freestr(fs2 *s);
static inline void fs2_freebody(fs2 *s);

/**
 * @brief Manages the capacity of an fs object.
 *
 * This function handles memory expansion for buffers allocated on the heap.
 * If the buffer is a fixed-size/static buffer (does not have @c FS_FLAG_ALLOC),
 * this function performs no resizing operations.
 *
 * @param[in,out] s      Pointer to the fs structure.
 * @param[in]     newsz  The target capacity in bytes.
 * @param[in]     init   Growth strategy flag:
 *                      - If @c true: Uses exponential growth (via @ref calcnewsize)
 *                        to ensure amortized O(1) complexity for heap-allocated buffers.
 *                      - If @c false: Attempts to resize to the exact @p newsz.
 *
 * @return Returns a log/status object containing information about the
 *         result (e.g., the final size @c s->sz).
 *
 * @note **Conditional Behavior**: The resizing logic (reallocation) is only 
 *      executed if @ref fs_alloc(s) returns true (i.e., the buffer was 
 *      initialized with @c FS_FLAG_ALLOC).
 * 
 * @warning **Pointer Invalidation**: For heap-allocated buffers, a successful 
 *          reallocation invalidates all existing pointers previously obtained 
 *          from this buffer (e.g., via @ref elem0).
 * 
 * @warning **Exception Handling**: For heap-allocated buffers, this function 
 *          may raise a system exception via @ref userraiseint if memory 
 *          allocation fails.
 * 
 * @details 
 * The internal execution flow is as follows:
 * 1. Check if @ref fs_alloc(s) is true. If false, return immediately.
 * 2. Check if the current capacity `s->sz` is sufficient for `newsz`.
 * 3. If more space is needed:
 *    a. Calculate new target size (exponential if @p init is true).
 *    b. Attempt to allocate new memory using `realloc`.
 *    c. If successful, update `s->v` and `s->sz`.
 *    d. If allocation fails, raise exception via @ref userraiseint.
 */

static fs2                              *increasesize(fs2 *s, size_t newsz, bool init) {
#ifdef FS2_TRACE 
    logenter("oldsz %zu, newsz %zu, init %s v %p, is alloc? %s", 
            s->sz, newsz, bool_str(init), s->v, bool_str(fs2_is_alloc(s)) );
#endif /* FS2_TRACE */
    if (fs2_is_alloc(s) ) {
        if (init)
            newsz = calcnewsize(SIZE_POWER2, newsz);    // with increase type
        if (newsz < FS_MIN_ALLOC)
            newsz = FS_MIN_ALLOC;
        if (newsz > s->sz || !init) {
                char *v = realloc(s->v, newsz);
                if (!v)
                    sysraise(ERR_UNABLE_ALLOCATE, "Unable to allocate %zu bytes", newsz);
                // now it's ok
                if (s->v == 0)      // only in case of REALLY new allocation logauto(++g_alloc_cnt);
                    atomic_fetch_add(&g_alloc_cnt, 1); 
                s->v = v;
                s->sz = newsz;
         }
    }
#ifdef FS2_TRACE 
    return logret(s, "%s%zu", fs2_is_alloc(s) ? "increased to " :"No change because non-heap ", s->sz);
#else
    return s;
#endif /* FS2_TRACE */
}

// internal constructor
static inline fs2 
copymem(const char *src, size_t len) {
    fs2         res = fs2_init(len + 1);
    memcpy(res.v, src, len + 1);
    res.len = len;       // NOT SURE: THINK:
    return res;
}

/* -------------------------------------------------------------------------
 * Low-level constructor — the ONLY place where fs2 is built by hand.
 *
 * All public constructors (fs2_init, fs2_fromstr, fs2_adopt, …) go
 * through here or through FS2()/FS2_LITERAL/FS2_LOCAL macros. Counter
 * bookkeeping happens here and nowhere else.
 *
 * @param buf   Buffer. Must not be NULL.
 * @param len   Logical length (excluding NUL). buf[len] is assumed
 *              to be '\0'.
 * @param flag  Exactly one of FS2_STATIC / FS2_LOCAL / FS2_ALLOC.
 *              BODYALLOC and DEFUNCT are not valid here.
 */
static inline fs2
fs2_fromraw(char *buf, size_t len, fs2_flags flag)
{
    invraise(buf != NULL, "Null pointer"); 

    fs2 s = (fs2){
        .len   = len,
        .sz    = len + 1,
        .flags = flag,
        .v     = buf,
    };
    if (flag != FS2_STATIC)
        buf[len] = '\0';    
    if (flag == FS2_ALLOC)
        atomic_fetch_add(&g_alloc_cnt, 1);
    return s;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Value constructors
 * ───────────────────────────────────────────────────────────────────────── */

 fs2            
 fs2_init(size_t sz) {
    fs2         res = FS2();     // fsalloc flag
    increasesize(&res, sz, true);   // init mode
    fs2_setlen(&res, 0);    // THINK:
    return res;
}

// note - unsafe if src has no limit
fs2   
fs2_fromstr (const char *src) {
    invraise(src != NULL, "Null pointer"); 

    size_t      len = strlen(src);
    return copymem(src, len);
}

fs2   
fs2_clone   (const fs2 *src) {
    invraise(src != NULL, "Null pointer"); 

    size_t      len = src->len;
    if (src->v)
        return copymem(src->v, len);
    else
        return FS2();
}

fs2   
fs2_fromstrf(const char *fmt, ...) {
    invraise(fmt != NULL, "Null format string"); 

    va_list ap;
    va_start(ap, fmt);
    int         sz = vsnprintf(NULL, 0, fmt, ap);
    if (sz < 0)
        userraise(0, ERR_WRONG_INPUT_FORMAT, "fmt %.15s", fmt);        // SIGINT or siglongjmp
    va_end(ap);
    fs2         res = fs2_init( (size_t) sz + 1);
    va_start(ap, fmt);
    vsprintf(res.v, fmt, ap);
    va_end(ap);
    fs2_setlen(&res, sz);
    return res;
}

fs2   
fs2_movefrom(fs2 *src) {
    fs2 res = FS2();
    // transfer
    fs2_move(&res, src);
    return res;
}

// dangerous one, we'he no idea if *from heap-located or static
// so we TRUST user
fs2   
fs2_movefromstr(const char **from, fs2_flags flag) {
    invraise(from != NULL && *from != NULL, 
        "Null pointer char **");

    if (flag != FS2_STATIC && flag != FS2_ALLOC)
        userraise(0, ERR_UNSUPPORTED_TYPE, 
            "%d/%s no supported for %s", flag, fs2_flag_name(flag), __func__);

    size_t  len = strlen(*from);
    fs2     res = fs2_fromraw((char *) *from, len, flag);    
    *from = NULL;
    return res;
}

// works even if NULL, *str MUST be heap allocated
fs2
fs2_adoptn(char **str, size_t saved_len)
{
    if (!str || !*str) {
#ifdef FS2_TRACE
        logsimple("adoptn from NULL!");
#endif /* FS2_TRACE */       
        return FS2();
    }
    fs2 s = fs2_fromraw(*str, saved_len, FS2_ALLOC);
    *str = NULL;
    return s;
}
// works even if NULL, *str MUST be heap allocated
fs2
fs2_adopt(char **str)
{
    if (!str || !*str) {
#ifdef FS2_TRACE
        logsimple("adopt from NULL!");
#endif /* FS2_TRACE */       
        return FS2();
    }
    return fs2_adoptn(str, strlen(*str));
}

/* ─────────────────────────────────────────────────────────────────────────
 * Constructors — heap (BODYALLOC set)
 *
 * Same operations, but the fs2 struct itself is allocated. Free with
 * fs2_free (which frees the buffer, the struct, or both, per flags).
 * ───────────────────────────────────────────────────────────────────────── */

fs2 *
fs2_heap          (void) {
    fs2 *res = malloc(sizeof(fs2));
    if (!res)
        return userraise(NULL, ERR_UNABLE_ALLOCATE, "Unable allocate fs2");

    *res = FS2();
    res->flags |= FS2_BODYALLOC;
    atomic_fetch_add(&g_alloc_body_cnt, 1);
    return res;
}

fs2     *fs2_heap_init     (size_t sz) {
    fs2 *res = fs2_heap();  // BODYALLOC only, not a valid fs2 yet
    // no checking here, excaption 'll occurs if OOM
    fs2 s = fs2_init(sz);   // ALLOC
    fs2_move(res, &s);
    return res;
}

fs2     *fs2_heap_clone    (const fs2 *src) {
    fs2     *res = fs2_heap();      // BODYALLOC only, not a valid fs2 yet
    // no checking here, excaption 'll occurs if OOM
    fs2      s = fs2_clone(src);    // ALLOC
    fs2_move(res, &s);              // BODYALLOC + ALLOC
    return res;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Move assignment (not a constructor)
 *
 *   dst ← src, then src → empty. Frees whatever `to` held before.
 * ───────────────────────────────────────────────────────────────────────── */
fs2    *
fs2_move(fs2 *dst, fs2 *src) {
    invraise(src != NULL, 
         "Null pointer %p", src);

    if (dst == src)
        return dst;    
    if (!dst) {
        fs2_free(src);
 #ifdef FS2_TRACE 
        logsimple("Move to NULL -> src freed!");
 #endif /* FS2_TRACE */   
        return NULL;
    }
    if (fs2_is_alloc(dst) )
        fs2_freestr(dst);   // BODYALLOC remains the same

    fs2_flags dst_body = dst->flags & FS2_BODYALLOC;
    *dst = *src;
    dst->flags = (fs2_flags) ((dst->flags & ~FS2_BODYALLOC) | dst_body);
 
    /* Detach buffer from src only if we took ownership.
     * STATIC / LOCAL sources keep pointing at the external buffer —
     * same rule as fs2_free. */
    if (fs2_is_alloc(src)) {
        src->v   = NULL;
        src->len = 0;
        src->sz  = 0;
        /* src's BODYALLOC (if any) stays — src still owns its struct. */
    }

    return dst; 
}

// stack <-> heap convertes
fs2   
fs2_heaptostack(fs2 **psrc) {
    invraise(psrc != NULL && *psrc != NULL, 
        "Null pointer %p or %p", psrc, psrc ? *psrc : NULL);

    if (!fs2_is_bodyalloc(*psrc))
        userraise(0, ERR_UNSUPPORTED_TYPE, "Only BODY ALLOC supported");
    fs2 res = FS2();
    fs2_move(&res, *psrc);
    fs2_freebody(*psrc);
    *psrc = NULL;       // remove origin??? fs2 *s = ...; fs2 stacked = fs2_heaptostack(&s);
    res.flags &= ~FS2_BODYALLOC;  // probably no need, check
    return res;
}
fs2  *
fs2_stacktoheap(fs2 *src) {
    invraise(src != NULL,  "Null pointer %p", src);

    if (fs2_is_bodyalloc(src))
        userraise(0, ERR_UNSUPPORTED_TYPE, "Only NOT BODY ALLOC supported");

    fs2 *res = fs2_heap();
    fs2_move(res, src);  // FS2_BODYALLOC is already set, src is cleared
    return res;
}

 /* ─────────────────────────────────────────────────────────────────────────
 * Value destructor
 * ───────────────────────────────────────────────────────────────────────── */
// NO check here - MUST be ALLOC
static inline void 
fs2_freestr(fs2 *s) {
    if (s->v) { // s->v in heap!
        int cnt = atomic_fetch_add(&g_free_cnt, 1) + 1;
#ifdef FS2_TRACE 
        logsimple("freed... %p (g_free_cnt %d)", 
            s->v, cnt);
#endif /* FS2_TRACE */
        free(s->v);
        s->sz = s->len = 0;
        s->v = NULL; 
    } 
}

// NO check here - MUST be BODYALLOC
static inline void 
fs2_freebody(fs2 *s) {
    int cnt = atomic_fetch_add(&g_free_body_cnt, 1) + 1;
#ifdef FS2_TRACE 
    logsimple("fs body freed (g_free_body_cnt %d)", 
        cnt);
#endif /* FS2_TRACE */
    ;
    free(s);
}

void                                    
fs2_free(fs2 *s) {
    if (!s)
        return;

    bool body   = fs2_is_bodyalloc(s);
    bool str    = fs2_is_alloc(s);

    if (!str && !body) {
#ifdef FS2_TRACE 
        logsimple("trying to free NON allocated and NOT bodyallocate %d", 
            s->flags);
#endif /* FS2_TRACE */  
        return;
    }
    if (str)
        fs2_freestr(s);
    if (body) 
        fs2_freebody(s);
}   

 /* ─────────────────────────────────────────────────────────────────────────
 * Tech printer
 * ───────────────────────────────────────────────────────────────────────── */
long                                    
fs_techfprint(FILE *restrict out, const fs2 *restrict s, const char *restrict name) {
    // technical print, statis attributes for now
    long     cnt = 0;
    if (out){
        if (s){
            size_t     len = MIN(FS_TECH_PRINT_COUNT, s->len);
            // TODO: use guard here
            cnt += fprintf(out, "FS2: %s: len [%zu], sz [%zu], flags [%d], s(%p)[%.*s", 
                        name, s->len, s->sz, s->flags, s->v, (unsigned) len, s->v);
            if (FS_TECH_PRINT_COUNT < s->len)
                cnt += fprintf(out, "...");
            cnt += fprintf(out, "]\n");
        } else
            cnt += fprintf(out, "FS2: %s: <NULL>\n", name);
    }
    return cnt;
}


 /* ─────────────────────────────────────────────────────────────────────────
 * Validator
 * ───────────────────────────────────────────────────────────────────────── */

 // TODO: move that to common.h
#define LOGNPRINTF(out, msg, ...) \
({ if (out) \
    fprintf(out, (msg), ##__VA_ARGS__); \
  logsimpleerr(false, (msg), ##__VA_ARGS__); \
})

bool
fs2_validate(FILE *restrict out, const fs2 *restrict s)
{
    if (!s)
        return LOGNPRINTF(out, "null pointer");

    fs2_flags FS2_ALL_FLAGS = (FS2_CONST | FS2_STATIC | FS2_LOCAL | FS2_ALLOC | FS2_BODYALLOC);
    
    /* 1. No unknown bits. */
    if (s->flags & ~FS2_ALL_FLAGS)
        return LOGNPRINTF(out, "unknown flag bits in 0x%x", (unsigned)s->flags);

    /* 2. Exactly one of STATIC / LOCAL / ALLOC. */
    {
        fs2_flags kind = s->flags & (FS2_STATIC | FS2_LOCAL | FS2_ALLOC);
        if (kind != FS2_STATIC && kind != FS2_LOCAL && kind != FS2_ALLOC)
            return LOGNPRINTF(out,
                "exactly one of STATIC/LOCAL/ALLOC required, got 0x%x", kind);
    }

    /* 3. Buffer presence depends on the string kind.
     *    - STATIC / LOCAL: must have v.
     *    - ALLOC: v may be NULL only in the canonical empty state
     *      (len = 0, sz = 0) — a fresh FS2() before any allocation. */
    if (!s->v) {
        if (s->flags & FS2_ALLOC) {
            if (s->len == 0 && s->sz == 0)
                return true;    /* canonical empty FS2() */
            return LOGNPRINTF(out, "null buffer but len=%zu, sz=%zu",
                              s->len, s->sz);
        }
        return LOGNPRINTF(out, "null buffer for STATIC/LOCAL string");
    }

    /* 4. Capacity invariant. */
    if (s->len >= s->sz)
        return LOGNPRINTF(out, "len [%zu] must be < sz [%zu]", s->len, s->sz);

    /* 5. Consistency with C-string content. */
    size_t len = strlen(s->v);
    if (len != s->len)
        return LOGNPRINTF(out, "strlen [%zu] != len [%zu]", len, s->len);

    return true;
}

 /* ─────────────────────────────────────────────────────────────────────────
 * Leak checking
 * ───────────────────────────────────────────────────────────────────────── */

bool 
fs2_checkleak(int *restrict alloc, int *restrict freed, int * restrict bodyalloc, int *restrict bodyfreed) {
    int      lfreecnt = atomic_load_explicit(&g_free_cnt, memory_order_relaxed),
            lalloccnt = atomic_load_explicit(&g_alloc_cnt, memory_order_relaxed),
         lfreebodycnt = atomic_load_explicit(&g_free_body_cnt, memory_order_relaxed),
        lallocbodycnt = atomic_load_explicit(&g_alloc_body_cnt, memory_order_relaxed);

    if (lfreecnt != lalloccnt) {
        if (alloc)
            *alloc = lalloccnt;
        if (freed)
            *freed = lfreecnt;
        return logsimpleerr(false, "g_free_cnt %d != g_alloc_cnt %d", 
                lfreecnt, lalloccnt);
    }
    if (lfreebodycnt != lallocbodycnt) {
        if (bodyalloc)
            *bodyalloc = lallocbodycnt;
        if (bodyfreed)
            *bodyfreed = lfreebodycnt;
        return logsimpleerr(false, "g_free_body_cnt %d != g_alloc_body_cnt %d", 
                            lfreebodycnt, lallocbodycnt);
    }
    return true;
}

#ifdef FS2TEST
// only for the test purpose
void   __fs2_checkreset(void) {
    logsimple("Reset allocation counters!!!");
    g_free_cnt = g_alloc_cnt = g_alloc_body_cnt = g_free_body_cnt;
}
#endif /* FS2TEST */
