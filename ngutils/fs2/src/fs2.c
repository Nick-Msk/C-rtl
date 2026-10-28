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

/**
 * @brief Return a NULL-terminated array of version strings.
 *
 * Implementation detail: backed by a static array with a single entry
 * (the current version) followed by NULL.
 *
 * @return Pointer to the static `const char*` array.
 */
const char *const              *fs2_versions(void) { return versions; }

/**
 * @brief Return the current library version string.
 *
 * @return Pointer to `versions[0]`, valid for the lifetime of the program.
 */
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
 * @brief Extract the BODYALLOC bit from a flag set (internal).
 *
 * @param s  Non-NULL fs2.
 * @return   FS2_BODYALLOC or 0.
 */
static inline fs2_flags     fs2_getbodyalloc(const fs2 *s) {
    return s->flags & FS2_BODYALLOC;
}

/**
 * @brief Set or clear the BODYALLOC bit, leaving other bits untouched (internal).
 *
 * @param s            Non-NULL fs2.
 * @param bodyallocflag  Pass FS2_BODYALLOC to set, 0 to clear.
 */
static inline void          fs2_setbodyalloc(fs2 *s, fs2_flags bodyallocflag) {
    bodyallocflag &= FS2_BODYALLOC;
    s->flags = ( (s->flags & ~FS2_BODYALLOC) | bodyallocflag);
}

/**
 * @brief Grow (or shrink) the heap buffer of an fs2 to a new capacity (internal).
 *
 * Only operates on FS2_ALLOC buffers; STATIC/LOCAL are silently skipped.
 *
 * @param s      Non-NULL fs2.
 * @param newsz  Target capacity in bytes (including NUL slot).
 * @param init   Growth strategy:
 *               - true:  exponential growth via calcnewsize (amortized O(1)).
 *               - false: exact resize to @p newsz.
 *
 * In both modes the result is clamped to at least FS_MIN_ALLOC (16) bytes.
 * Realloc is called only if the target exceeds current capacity (init mode)
 * or unconditionally (exact mode).
 *
 * @return @p s (for chaining). If FS2_TRACE is defined, returns a log object.
 *
 * @warning `sysraise` (OOM) if `realloc` fails.
 * @note    After a successful realloc, any previously obtained pointer into
 *          the buffer is invalidated.
 */

static fs2 *
increasesize(fs2 *s, size_t newsz, bool init) {
#ifdef FS2_TRACE 
    logenter("oldsz %zu, newsz %zu, init %s v %p, is alloc? %s", 
            s->sz, newsz, bool_str(init), s->v, bool_str(fs2_is_alloc(s)) );
#endif /* FS2_TRACE */
    if (fs2_is_alloc(s) ) {
        if (init)
            newsz = calcnewsize(SIZE_POWER2, newsz);    // with increase type
        if (init && newsz < FS_MIN_ALLOC)              // if !init -> mostly shrink
            newsz = FS_MIN_ALLOC;
        if (!init || newsz > s->sz) {                   // !init -> realloc ANYWAY
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

/**
 * @brief Deep-copy @p len bytes from @p src into a new ALLOC fs2 (internal).
 *
 * Allocates len+1 via fs2_init, memcpys len+1 bytes (including NUL),
 * and sets the logical length.
 *
 * @param src  Source buffer; must have at least @p len+1 bytes.
 * @param len  Number of significant bytes (excluding NUL).
 * @return     New ALLOC fs2 with a heap copy.
 *
 * @warning    `sysraise` on OOM.
 */
static inline fs2 
copymem(const char *src, size_t len) {
    fs2         res = fs2_init(len + 1);
    memcpy(res.v, src, len + 1);
    res.len = len;       // NOT SURE: THINK:
    return res;
}

/**
 * @brief printf-format into a new ALLOC fs2 from a va_list (internal).
 *
 * Two-pass: first measures via `vsnprintf(NULL,0)`, then allocates and
 * formats. Handles the case where the buffer is exactly the right size
 * (vsnprintf returns the needed size even if truncated).
 *
 * @param fmt  Format string (must not be NULL).
 * @param ap   Initialised argument list.
 * @return     ALLOC fs2 containing the formatted string.
 *
 * @warning    `userraise` if vsnprintf returns negative (format error).
 * @warning    `sysraise` on OOM.
 */
static inline fs2
fs2_fromstrf_va(const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);

    int         sz = vsnprintf(NULL, 0, fmt, ap2);
    if (sz < 0)
        userraise(0, ERR_WRONG_INPUT_FORMAT, "fmt %.15s", fmt);        // SIGINT or siglongjmp
    va_end(ap2);

    fs2         res = fs2_init( (size_t) sz + 1);
    vsnprintf(res.v, res.sz, fmt, ap);
    fs2_setlen(&res, sz);
    return res;
}

/**
 * @brief Low-level constructor: build an fs2 from a raw pointer (internal).
 *
 * The ONLY place where an fs2 is assembled by hand. All public
 * constructors (fs2_init, fs2_fromstr, fs2_adopt, ...) go through here
 * or through the FS2()/FS2LITERAL/FS2LOCAL macros.
 *
 * Counter bookkeeping (g_alloc_cnt) happens here and nowhere else.
 *
 * @param buf   Buffer. Must not be NULL. For non-STATIC flags, buf[len]
 *              is explicitly set to NUL.
 * @param len   Logical length (excluding NUL).
 * @param flag  Exactly one of FS2_STATIC / FS2LOCAL / FS2_ALLOC,
 *              optionally OR'd with FS2_BODYALLOC.
 * @return      Constructed fs2 value.
 * @warning     invraise if buf is NULL.
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

 /**
 * @brief Create a growable fs2 with the given initial capacity.
 *
 * Allocates a heap buffer of at least @p sz bytes (minimum 16, power-of-2
 * rounding). Length is 0; buffer is zero-initialised.
 *
 * @param sz  Desired initial capacity (including NUL slot).
 * @return    fs2 value (ALLOC) with a fresh heap buffer.
 *
 * @warning   `sysraise` on OOM.
 */
fs2
fs2_init(size_t sz) {
    fs2         res = FS2();     // fsalloc flag
    increasesize(&res, sz, true);   // init mode
    fs2_clear(&res);    // THINK:
    return res;
}

/**
 * @brief Create a growable fs2 as a deep copy of a C string.
 *
 * Measures @p src via `strlen`, then delegates to `copymem`.
 *
 * @param src  Non-NULL NUL-terminated source.
 * @return     fs2 value (ALLOC) with a heap copy of @p src.
 *
 * @warning    `invraise` if @p src is NULL.
 */
fs2
fs2_fromstr (const char *src) {
    invraise(src != NULL, "Null pointer"); 

    size_t      len = strlen(src);
    return copymem(src, len);
}

/**
 * @brief Deep-copy any fs2 into a new growable one.
 *
 * The result is always FS2_ALLOC with a fresh heap buffer, regardless of
 * the source kind. If the source buffer is NULL (defunct), returns FS2().
 *
 * @param src  Non-NULL source fs2.
 * @return     fs2 value (ALLOC) with a heap copy of the source content.
 *
 * @warning    `invraise` if @p src is NULL.
 */
fs2
fs2_clone   (const fs2 *src) {
    invraise(src != NULL, "Null pointer"); 

    size_t      len = src->len;
    if (src->v)
        return copymem(src->v, len);
    else
        return FS2();
}

/**
 * @brief Create a growable fs2 via printf-style formatting.
 *
 * Two-pass: measures via `vsnprintf(NULL,0)`, then allocates and formats.
 *
 * @param fmt  Format string (must not be NULL).
 * @param ...  Format arguments.
 * @return     fs2 value (ALLOC) containing the formatted result.
 *
 * @warning    `invraise` if @p fmt is NULL; `userraise` on format error.
 */
fs2
fs2_fromstrf(const char *fmt, ...) {
    invraise(fmt != NULL, "Null format string"); 

    va_list ap;
    va_start(ap, fmt);
    fs2 res = fs2_fromstrf_va(fmt, ap);
    va_end(ap);
    return res;
}

/**
 * @brief Steal the buffer from @p src into a new stack fs2.
 *
 * After the call, @p src is in the canonical empty state (FS2(),
 * preserving its BODYALLOC if it was heap-allocated). Works for
 * ALLOC and STATIC sources. **Forbidden for LOCAL** (invraise).
 *
 * @param src  Non-NULL source fs2 (ALLOC or STATIC).
 * @return     fs2 value that now owns the former src buffer.
 *
 * @warning    `invraise` if @p src is NULL or FS2LOCAL.
 */
fs2
fs2_movefrom(fs2 *src) {
    fs2 res = FS2();
    // transfer
    fs2_move(&res, src);
    return res;
}

/**
 * @brief Steal a heap or static C string into an fs2.
 *
 * After the call, `*from` is set to NULL. The caller must guarantee
 * that the pointer is valid for the specified flag kind.
 *
 * @param from  Pointer to the char* to steal; must be non-NULL and
 *              `*from` must be non-NULL. Set to NULL on success.
 * @param flag  FS2_STATIC (read-only, e.g. from a literal table)
 *              or FS2_ALLOC (heap, will be freed by fs2_free).
 * @return      fs2 value wrapping the stolen pointer.
 *
 * @warning     `invraise` if pointers are NULL; `userraise` for unsupported flag.
 */
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

/**
 * @brief Adopt a heap-allocated C string with a known length.
 *
 * After the call, `*str` is set to NULL. If `str` or `*str` is NULL,
 * returns FS2() (empty growable).
 *
 * @param str       Pointer to the heap char* to adopt; set to NULL.
 * @param saved_len Known logical length (avoids a strlen call).
 * @return          fs2 value (ALLOC) wrapping the adopted buffer.
 */
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

/**
 * @brief Adopt a heap-allocated C string (length computed via strlen).
 *
 * Convenience wrapper around fs2_adoptn. If `str` or `*str` is NULL,
 * returns FS2() (empty growable).
 *
 * @param str  Pointer to the heap char* to adopt; set to NULL on success.
 * @return     fs2 value (ALLOC) wrapping the adopted buffer.
 */
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

/**
 * @brief Allocate an empty fs2 on the heap (BODYALLOC set).
 *
 * The returned struct is initialised to FS2() (ALLOC, v=NULL) with
 * BODYALLOC. Free with fs2_free.
 *
 * @return  Heap-allocated fs2*, or raises on OOM.
 */
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

/**
 * @brief Allocate a growable fs2 on the heap with given capacity.
 *
 * Equivalent to fs2_init but the struct itself is heap-allocated.
 *
 * @param sz  Initial buffer capacity (including NUL slot).
 * @return    Heap-allocated fs2* (BODYALLOC | ALLOC), or raises on OOM.
 */
fs2 *
fs2_heap_init     (size_t sz) {
    fs2 *res = fs2_heap();  // BODYALLOC only, not a valid fs2 yet
    // no checking here, excaption 'll occurs if OOM
    fs2 s = fs2_init(sz);   // ALLOC
    fs2_move(res, &s);
    return res;
}

/**
 * @brief Heap-allocate an fs2 as a deep copy of another fs2.
 *
 * The result is always BODYALLOC | ALLOC regardless of source kind.
 *
 * @param src  Non-NULL source fs2.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC) with a heap copy.
 *
 * @warning    `invraise` if @p src is NULL.
 */
fs2 *
fs2_heap_clone    (const fs2 *src) {
    invraise(src != NULL, "Null pointer");

    fs2     *res = fs2_heap();      // BODYALLOC only, not a valid fs2 yet
    // no checking here, excaption 'll occurs if OOM
    fs2      s = fs2_clone(src);    // ALLOC
    fs2_move(res, &s);              // BODYALLOC + ALLOC
    return res;
}

/**
 * @brief Heap-allocate an fs2 as a deep copy of a C string.
 *
 * @param src  Non-NULL NUL-terminated source.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC) with a heap copy.
 *
 * @warning    `invraise` if @p src is NULL.
 */
fs2 *
fs2_heap_fromstr  (const char *src) {
    fs2     *res = fs2_heap();
    fs2      s = fs2_fromstr(src);

    fs2_move(res, &s);
    return res;
}

/**
 * @brief Heap-allocate an fs2 via printf-style formatting.
 *
 * @param fmt  Format string (must not be NULL).
 * @param ...  Format arguments.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC).
 *
 * @warning    `invraise` if @p fmt is NULL; `userraise` on format error.
 */
fs2 *
fs2_heap_fromstrf (const char *fmt, ...) {
    invraise(fmt != NULL, "Null format");

    fs2     *res = fs2_heap();
    va_list ap;
    va_start(ap, fmt);
    fs2      s = fs2_fromstrf_va(fmt, ap);
    va_end(ap);

    fs2_move(res, &s);
    return res;
}

/**
 * @brief Heap-allocate an fs2 by stealing a C string pointer.
 *
 * Wraps fs2_movefromstr into a heap body. `*from` is set to NULL.
 *
 * @param from  Pointer to the char* to steal; set to NULL.
 * @param fl    FS2_STATIC or FS2_ALLOC.
 * @return      Heap-allocated fs2* (BODYALLOC + given flag).
 *
 * @warning     `invraise` if pointers are NULL; `userraise` for unsupported flag.
 */
fs2 *
fs2_heap_movefromstr(const char **from, fs2_flags fl) {
    fs2     *res = fs2_heap();
    fs2      s = fs2_movefromstr(from, fl);
    fs2_move(res, &s);
    return res;
}

/**
 * @brief Ensure capacity for at least @p need bytes (including NUL).
 *
 * No-op for STATIC/LOCAL (cannot grow external buffers).
 *
 * @param s     Non-NULL fs2.
 * @param need  Desired minimum capacity.
 *
 * @warning     `invraise` if @p s is NULL.
 */
void
fs2_reserve(fs2 *s, size_t need)
{
    invraise(s != NULL, "Null pointer");
    if (fs2_is_alloc(s))
        increasesize(s, need, true);
}

/**
 * @brief Shrink the heap buffer to fit the current content exactly.
 *
 * Reallocates to len+1 bytes (minimum FS_MIN_ALLOC). No-op for
 * STATIC/LOCAL.
 *
 * @param s  Non-NULL fs2.
 * @return   @p s for chaining.
 *
 * @warning  `invraise` if @p s is NULL.
 */
fs2 *      
fs_shrink(fs2 *s) {
    invraise(s != NULL, "Null pointer");
    s = increasesize(s, s->len + 1, false);
    return s;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Move assignment (not a constructor)
 *
 *   dst ← src, then src → empty. Frees whatever `to` held before.
 *
 * Implementation: rejects LOCAL on either side. Frees dst's ALLOC buffer.
 * Copies struct, restores each side's BODYALLOC. Resets src to FS2().
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Move-assign: @p dst ← @p src, then @p src → canonical empty.
 *
 * Frees whatever @p dst previously held (if ALLOC). After the call:
 * - @p dst   owns the former source buffer/pointer.
 * - @p src   is reset to FS2() (ALLOC, v=NULL), preserving its BODYALLOC.
 *
 * The BODYALLOC flag of each side is independent and preserved.
 *
 * **LOCAL is forbidden** on both sides — moving a caller-owned buffer
 * would create an alias. Use fs2_clone for sharing.
 *
 * @param dst   Destination; must be non-NULL.
 * @param src   Source; must be non-NULL and not LOCAL. Reset to FS2().
 *
 * @return      @p dst (for chaining).
 *
 * @note        fs2_move(x, x) is a no-op that returns @p dst.
 * @warning     `invraise` if either pointer is NULL or either side is FS2LOCAL.
 */
fs2 *
fs2_move(fs2 *dst, fs2 *src) {
    invraise(dst != NULL && src != NULL, 
        "Null pointers %p %p", dst, src);


    if (dst == src)
        return dst;    

    if (fs2_is_local(src) || fs2_is_local(dst))
        return userraise(NULL, ERR_UNSUPPORTED_TYPE,
                  "fs2_move does not support LOCAL (stack buffer)");

    fs2_flags dst_body = fs2_getbodyalloc(dst);
    fs2_flags src_body = fs2_getbodyalloc(src);

    if (fs2_is_alloc(dst) )
        fs2_freestr(dst);   // BODYALLOC remains the same

    *dst = *src;
    fs2_setbodyalloc(dst, dst_body);
    *src = FS2();
    fs2_setbodyalloc(src, src_body); 

    return dst; 
}

/**
 * @brief Exchange the string values of two fs2 objects.
 *
 * Both sides must be FS2_ALLOC — exchanging with STATIC or LOCAL would
 * leave the ALLOC side pointing at a buffer it does not own (and the
 * STATIC/LOCAL side pointing at a buffer fs2_free would skip, leaking
 * it). The BODYALLOC flag of each side is preserved.
 *
 * @param a  First fs2. Must not be NULL, must be FS2_ALLOC.
 * @param b  Second fs2. Same constraints.
 *
 * @note     fs2_exch(a, a) is a no-op.
 * @warning  `userraise`(SIGINT) if either side is not FS2_ALLOC.
 */
void
fs2_exch(fs2 *a, fs2 *b)
{
    invraise(a != NULL && b != NULL, "Null pointer %p %p", a, b);

    if (a == b)
        return;

    if (fs2_is_local(a) || fs2_is_local(b))
        userraise(0, ERR_UNSUPPORTED_TYPE, "fs2_exch does not support LOCAL");

    fs2 tmp = FS2();
    fs2_move(&tmp, a);
    fs2_move(a, b);
    fs2_move(b, &tmp);
}

// stack <-> heap converters

/**
 * @brief Convert a heap-allocated fs2 to a stack value.
 *
 * Moves the value out of the heap struct, frees the struct, and sets
 * `*psrc` to NULL. The result has BODYALLOC cleared.
 *
 * @param psrc  Pointer to a heap fs2* (must be non-NULL and BODYALLOC).
 *              Set to NULL on success.
 * @return      Stack fs2 value with the former heap value's content.
 *
 * @warning     `invraise` if pointers are NULL; `userraise` if not BODYALLOC
 *              or if the source is FS2LOCAL.
 */
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
/**
 * @brief Convert a stack fs2 to a heap-allocated fs2.
 *
 * Moves the value into a new heap struct (BODYALLOC set). The source is
 * reset to FS2(). The source must not be LOCAL or already BODYALLOC.
 *
 * @param src  Stack fs2 (non-NULL, not BODYALLOC, not LOCAL).
 * @return     New heap fs2* owning the value.
 *
 * @warning    `invraise` if NULL or BODYALLOC; `userraise` if LOCAL.
 */
fs2 *
fs2_stacktoheap(fs2 *src) {
    invraise(src != NULL,  "Null pointer %p", src);

    if (fs2_is_bodyalloc(src))
        userraise(0, ERR_UNSUPPORTED_TYPE, "Only NOT BODY ALLOC supported");
    if (fs2_is_local(src))
        userraise(0, ERR_UNSUPPORTED_TYPE, "Cannot move LOCAL fs2 to heap");

    fs2 *res = fs2_heap();
    fs2_move(res, src);  // FS2_BODYALLOC is already set, src is cleared
    return res;
}

 /* ─────────────────────────────────────────────────────────────────────────
 * Value destructor
 * ───────────────────────────────────────────────────────────────────────── */
/**
 * @brief Free the heap buffer of an ALLOC fs2 (internal).
 *
 * Caller guarantees the flag is ALLOC. The null-guard on `s->v` is
 * defensive: in canonical empty state (v=NULL) this is a no-op.
 *
 * After the call: v=NULL, len=0, sz=0. Flags are NOT modified.
 *
 * @param s  Non-NULL fs2 that is ALLOC.
 */
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

/**
 * @brief Free the heap-allocated fs2 struct itself (internal).
 *
 * Caller guarantees the flag is BODYALLOC. The pointer must be valid
 * heap memory; stack-allocated fs2 never carries this flag.
 *
 * @param s  Non-NULL pointer to a heap-allocated fs2.
 */
static inline void 
fs2_freebody(fs2 *s) {
    int cnt = atomic_fetch_add(&g_free_body_cnt, 1) + 1;
#ifdef FS2_TRACE 
    logsimple("fs body freed (g_free_body_cnt %d)", 
        cnt);
#endif /* FS2_TRACE */
    free(s);
}

/**
 * @brief Destructor: release all resources owned by an fs2.
 *
 * - If FS2_ALLOC: frees the buffer (`free(s->v)`).
 * - If FS2_BODYALLOC: frees the struct itself.
 * - STATIC / LOCAL: no-op (no owned memory).
 *
 * Safe to call on a NULL pointer (returns immediately). Idempotent for
 * non-BODYALLOC values: after freeing an ALLOC buffer the state is
 * canonical empty (v=NULL, len=0, sz=0), so a second call is harmless.
 *
 * @param s  Pointer to fs2, or NULL (no-op).
 */
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
 * Wrapper for comparators
 * ───────────────────────────────────────────────────────────────────────── */
int                  
fs2_cmp_wrap(const void *str1, const void *str2) {
    return fs2_cmp(str1, str2);
}

int                  
fs2_cmp_wrap_ci(const void *str1, const void *str2) {
    return fs2_cmp_ci(str1, str2);
}
// int                  
// fs2_cmpstr_wrap(const void *str1, const void *str2) {
//     return fs2_cmpstr(str1, str2);
// }

// int                  
// fs2_cmpstr_wrap_ci(const void *str1, const void *str2) {
//     return fs2_cmp_ci_str(str1, str2);
// }

 /* ─────────────────────────────────────────────────────────────────────────
 * Tech printer
 * ───────────────────────────────────────────────────────────────────────── */
/**
 * @brief Print a one-line technical dump of an fs2 to a stream.
 *
 * Outputs: len, capacity, flags (decimal), buffer pointer, and up to
 * 100 characters of content (truncated with `...` if longer).
 * If @p s is NULL, prints `<NULL>`.
 *
 * @param out   Output stream; if NULL, nothing is written.
 * @param s     fs2 to print (may be NULL).
 * @param name  Label printed before the values.
 * @return      Number of characters written (0 if @p out is NULL).
 */
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

/**
 * @brief Validate all invariants of an fs2.
 *
 * Checks:
 * 1. No unknown flag bits.
 * 2. Exactly one of STATIC / LOCAL / ALLOC is set.
 * 3. Buffer presence: STATIC/LOCAL require v; ALLOC allows v==NULL only
 *    in canonical empty state (len==0, sz==0).
 * 4. len < sz.
 * 5. strlen(v) == len.
 *
 * On failure, a diagnostic message is written to @p out (if non-NULL)
 * and to the internal error log.
 *
 * @param out  Optional output stream for the diagnostic; may be NULL.
 * @param s    fs2 to validate (may be NULL — reports and returns false).
 * @return     true if all invariants hold; false otherwise.
 */
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

/**
 * @brief Technical equality: len + flags + content must all match.
 *
 * Fast path: same pointer or both empty → true.
 * Slow path: compare len, flags, then memcmp up to len bytes.
 *
 * See fs2.h for the full contract.
 */
bool    
fs2_techcmp(const fs2 *s1, const fs2 *s2) {
    if (s1 == s2)
        return true;
    invraise(s1 != NULL && s2 != NULL,
        "Null pointers %p %p", s1, s2);
    if (s1->len != s2->len || s1->flags != s2->flags)
        return false;

    if (s1->len == 0)
        return true;

    return memcmp(s1->v, s2->v, s1->len) == 0;
}


 /* ─────────────────────────────────────────────────────────────────────────
 * Leak checking
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Check for internal memory leaks by comparing alloc/free counters.
 *
 * Compares the global allocation and deallocation counters. If any
 * counter pair is mismatched, the current values are written to the
 * corresponding output parameters (if non-NULL) and a diagnostic is
 * logged.
 *
 * All parameters are optional (pass NULL to skip output for a pair).
 *
 * @param alloc      Out: current buffer allocation count (if mismatch).
 * @param freed      Out: current buffer free count (if mismatch).
 * @param bodyalloc  Out: current struct allocation count (if mismatch).
 * @param bodyfreed  Out: current struct free count (if mismatch).
 * @return           true if no leaks detected; false otherwise.
 */
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
/**
 * @brief Reset all internal alloc/free counters to zero.
 *
 * Test-only. Must be called between test cases to get clean leak
 * reporting from fs2_checkleak.
 */
void   __fs2_checkreset(void) {
    logsimple("Reset allocation counters!!!");
    g_free_cnt = g_alloc_cnt = g_alloc_body_cnt = g_free_body_cnt;
}
#endif /* FS2TEST */
