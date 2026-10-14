/**
 * @file fs2.c
 * @brief Core faststring primitives.
 */

#include "fs2.h"
#include <stdatomic.h>

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
 #endif
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
#endif
}

/* ─────────────────────────────────────────────────────────────────────────
 * Value constructors
 * ───────────────────────────────────────────────────────────────────────── */

 fs2            fs2_init(size_t sz) {
    fs2      res = FS2();     // fsalloc flag
    increasesize(&res, sz, true);   // init mode
    fs2_setlen(&res, 0);
    return res;
}


 /* ─────────────────────────────────────────────────────────────────────────
 * Value destructor
 * ───────────────────────────────────────────────────────────────────────── */

void                                    fs2_free(fs2 *s){
    if (!s) {
#ifdef FS2_TRACE 
        if (s)
            logsimple("trying to free NON allocated %d", s->flags);
#endif        
        return;
    }
    bool  body = fs2_is_bodyalloc(s);  // flags based
    if (fs2_is_alloc(s) && s->v) { // s->v in heap!
        atomic_fetch_add(&g_free_cnt, 1);
#ifdef FS2_TRACE 
        logsimple("freed... %p", s->v);   // WOW, logsimpleact? 
#endif
        free(s->v);
        s->sz = s->len = 0;
        s->v = NULL; 
    } 
    if (body){  // s in heap!
        atomic_fetch_add(&g_free_body_cnt, 1);
#ifdef FS2_TRACE 
        logsimple("fs body %p freed (g_free_body_cnt %d)", s, g_free_body_cnt);
#endif
        atomic_load_explicit(&g_free_body_cnt, memory_order_relaxed);
        free(s);
    }
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

bool                                    fs2_validate(FILE *restrict out, const fs2 *restrict s){
    if (!s)
        return LOGNPRINTF(out, "null pointer");
    if (!s->v)
        return LOGNPRINTF(out, "nullable string");

    // depents on which iterator engine is active
    if (s->len >= s->sz)
        return LOGNPRINTF(out, "len [%zu] must be < sz [%zu]", s->len, s->sz);

    size_t len = strlen(s->v);
    if (len != s->len)
        return LOGNPRINTF(out, "srtlen [%zu] can't be more than len [%zu]", len, s->len);
    
    return true;
}