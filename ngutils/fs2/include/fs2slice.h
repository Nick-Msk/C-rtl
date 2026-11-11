/** @file
 * @brief Zero-copy views into an @ref fs2 buffer: single slice and array of slices.
 *
 * A #fs2slice is a non-owning, **move-only** handle that references a
 * contiguous byte range [from, to) of a parent @ref fs2 buffer.
 * It shares the parent's memory (no copy) but holds a borrow reference
 * to keep the parent alive while the slice is in use.
 *
 * A #fs2arrslice holds a growable array of #fs2sliceview ranges, all
 * pointing into the same parent. It owns exactly **one** borrow.
 *
 * @section thread Safety
 *
 * - **Same parent**: NOT thread-safe. `parent->borrow` is a plain
 *   `uint32_t`; concurrent `fs2slice_new` / `fs2slice_free` on one
 *   parent require external locking.
 * - **Different parents**: safe.
 * - **Read-only** access to independent slices: safe.
 *
 * @section lifecycle Move-only RAII handle
 *
 * @warning fs2slice is a **move-only** handle. Copying
 *          (`fs2slice b = a`) does **NOT** increment `parent->borrow`.
 *          Freeing either copy leaves the other dangling
 *          (`parent->borrow == 0`) or causes **UNDERFLOW** (double free).
 *          Pass by pointer (`const fs2slice *`) or wrap in a container
 *          that manages lifetime; never shallow-copy.
 *
 * @section extract Extraction (deep copy)
 *
 * `fs2slice_substr_new`, `fs2slice_to_owned`, `fs2slice_left_new`,
 * `fs2slice_right_new`, and `fs2slice_clone` return a **new owned**
 * #fs2 — the slice itself is left untouched.
 */

#ifndef FS2SLICE_H
#define FS2SLICE_H

#include "fs2.h"
#include "fs2_api.h"
/**
 * @brief Half-open byte range [from, to) within a parent @ref fs2 buffer.
 *
 * A value type embedded in #fs2slice and #fs2arrslice.
 * `from` and `to` are absolute offsets into `parent->v`.
 */
typedef struct fs2sliceview {
    size_t from;   /**< Start offset (inclusive) into parent->v. */
    size_t to;     /**< End offset (exclusive) into parent->v.   */
} fs2sliceview;

/** Zero-initialised sliceview — represents "empty / no range". */
#define FS2SLICEVIEW() (fs2sliceview) {.from = 0, .to = 0}

/**
 * @brief Array of slice views, all pointing into the same parent @ref fs2.
 *
 * The array owns a **single** borrow on `parent`. Individual elements
 * retrieved via `fs2arrslice_get` each create their own temporary borrow.
 *
 * Invariants (checked by `fs2arrslice_validate`):
 *  - `count <= hwm <= size`
 *  - `count > 0` implies `ranges != NULL`
 *  - if `parent == NULL`, all of `ranges/hwm/size/count` must be zero
 *    (canonical empty)
 */
typedef struct fs2arrslice {
    fs2          *parent;   /**< Parent fs2 (single borrow). NULL = canonical empty. */
    fs2sliceview *ranges;   /**< Heap array of ranges, allocated length `size`.       */
    size_t        hwm;      /**< High-water mark: max `count` ever reached.           */
    size_t        size;     /**< Allocated capacity of `ranges`.                       */
    size_t        count;    /**< Number of valid entries in `ranges`.                  */
} fs2arrslice;

/**
 * @brief Canonical empty / zero #fs2arrslice.
 *
 * All fields are NULL/zero — represents "no parent, no ranges".
 * `fs2arrslice_free` is a no-op on this value.
 */
#define FS2ARRSLICE() (fs2arrslice){ .parent = NULL, .ranges = NULL, \
                                     .hwm = 0, .size = 0, .count = 0 }

/**
 * @brief Move-only handle to a single contiguous sub-range of a parent @ref fs2.
 *
 * Reads `parent->v` on every access, so it is automatically
 * realloc-safe: if the parent buffer is relocated (e.g. via `fs2_reserve`),
 * the slice continues to see the new buffer through the pointer.
 *
 * **Move-only**: do not copy. See file-level @warning above.
 */
typedef struct fs2slice {
    fs2            *parent;  /**< Non-NULL while alive; NULL after `fs2slice_free`. */
    fs2sliceview    range;   /**< [from, to) in parent byte offsets.                 */
} fs2slice;                      

/**
 * @brief Canonical empty / zombie #fs2slice (no parent).
 *
 * `fs2slice_free` is a no-op on this value. All accessors
 * (`fs2slice_len`, `fs2slice_data`, …) `invfatal` on it.
 */
#define FS2SLICE() (fs2slice){ .parent = NULL, .range = FS2SLICEVIEW() }

/* =========================================================================
 * Leak-checking counters (process-wide, thread-safe)
 * ========================================================================= */

/**
 * @brief Atomically increment the global slice-borrow counter.
 *
 * Called internally by `fs2slice_parentinc()`. Tracks the net number of
 * active slice borrows across **all** #fs2 objects in the process.
 *
 * @return The value of the counter **before** the increment.
 * @note Thread-safe (C11 atomic). Exposed for test harnesses / debug builds.
 */
extern long
fs2slice_inc(void);

/**
 * @brief Atomically decrement the global slice-borrow counter.
 *
 * Called internally by `fs2slice_parentdec()`.
 *
 * @return The value of the counter **before** the decrement.
 * @note Thread-safe (C11 atomic).
 */
extern long
fs2slice_dec(void);

/**
 * @brief Return the current value of the global slice-borrow counter.
 *
 * A non-zero value at program exit indicates leaked (unfreed) slices.
 *
 * @return Current net borrow count (may be negative on underflow).
 * @note Thread-safe (C11 atomic).
 */
extern long
fs2slice_sum(void);

/* =========================================================================
 * Accessors
 * ========================================================================= */

/**
 * @brief Length (in bytes) of the slice content.
 *
 * @param s  Non-NULL, non-zombie slice.
 * @return   `s->range.to - s->range.from`.
 * @warning  `invfatal` (SIGABRT) if @p s is NULL or `s->parent` is NULL.
 */
static inline size_t
fs2slice_len(const fs2slice *s) { 
    invfatal(ERR_FS2SLICE_ZOMBIE, s != NULL && s->parent != NULL,
        "Null or Zombie %p %p", s, s ? s->parent: NULL);
    return s->range.to - s->range.from; 
}

/**
 * @brief Pointer to the first byte of the slice content.
 *
 * Points into `s->parent->v`; the parent is **not** modified.
 *
 * @param s  Non-NULL, non-zombie slice.
 * @return   `s->parent->v + s->range.from`.
 * @warning  `invfatal` (SIGABRT) if @p s is NULL or `s->parent` is NULL.
 */
static inline const char *
fs2slice_data(const fs2slice *s) { 
    invfatal(ERR_FS2SLICE_ZOMBIE, s != NULL && s->parent != NULL,
        "Null or Zombie %p %p", s, s ? s->parent: NULL);
    return s->parent->v + s->range.from; 
}

/* =========================================================================
 * Internal — borrow bookkeeping (not part of the public API)
 * ========================================================================= */

/**
 * @internal Increment `parent->borrow` and the global slice-borrow counter.
 *
 * Called by `fs2slice_new` and `fs2arrslice_get`.
 *
 * @param parent  Non-NULL parent #fs2 whose borrow count is incremented.
 * @return        New borrow count after the increment.
 * @warning Not thread-safe (`fs2.borrow` is a plain `uint32_t`).
 *          External locking is required for concurrent access to the same parent.
 */
static inline int32_t
fs2slice_parentinc(fs2 *parent) {
    fs2slice_inc();
    return ++parent->borrow;       // not atomic: external lock required (see header)
}

/**
 * @internal Decrement `parent->borrow` and the global slice-borrow counter;
 *          NULL the parent pointer.
 *
 * Called by `fs2slice_free` and `fs2arrslice_free`.
 *
 * @param[out] parent  Pointer to the `fs2 *` to release; set to NULL on return.
 * @return             New borrow count after the decrement.
 * @warning  `userraise` (ERR_FS2SLICE_UNDERFLOW) if `borrow` is already 0.
 * @warning Not thread-safe (`fs2.borrow` is a plain `uint32_t`).
 *          External locking is required for concurrent access to the same parent.
 */
static inline int32_t
fs2slice_parentdec(fs2 **parent) {
    fs2 *s = *parent;
    if (s->borrow == 0)
        userraise(false, ERR_FS2SLICE_UNDERFLOW, 
            "borrow underflow");
    int32_t res = --s->borrow;       // not atomic: external lock required (see header)
    *parent = NULL;
    fs2slice_dec();
    return res;
}

/**
 * @brief Create a new slice handle over `parent` covering [from, to).
 *
 * Increments `parent->borrow` and returns a move-only #fs2slice.
 * The caller is responsible for calling `fs2slice_free` exactly once.
 *
 * @param parent  Non-NULL parent #fs2.
 * @param from    Start offset (inclusive) into `parent->v`; must be < `to`.
 * @param to      End offset (exclusive) into `parent->v`; must be ≤ `parent->len`.
 * @return        A new #fs2slice referencing `parent`.
 * @warning `invfatal` (SIGABRT) if `parent` is NULL.
 * @warning `userraise` (ERR_OUT_OF_RANGE) if `from >= to` or `to > parent->len`.
 *
 * @note **Move-only**: `fs2slice b = a` does **NOT** increment borrow.
 *       Freeing both → underflow; freeing one → orphaned parent.
 *       Pass `const fs2slice *` for read-only access.
 */
static inline fs2slice
fs2slice_new(fs2 *parent, size_t from, size_t to) {
    invfatal(ERR_NULL_INPUT, parent != NULL, "Null parent");
    if (from >= to || to > parent->len)
        userraise(0, ERR_OUT_OF_RANGE,
             "Out of range [%zu, %zu) not in [0, %zu]",
             from, to, parent->len);
    fs2slice_parentinc(parent);
    return (fs2slice){ .parent = parent, .range = {.from = from, .to = to} };
}

/**
 * @brief Retrieve a new slice handle for element @p i of the array.
 *
 * Creates a new #fs2slice over `a->parent` covering `a->ranges[i]`,
 * incrementing `a->parent->borrow`. The caller must free the returned
 * slice with `fs2slice_free`.
 *
 * @param a  Non-NULL #fs2arrslice.
 * @param i  Index in [0, count).
 * @return   A new #fs2slice (temporary borrow on the parent).
 * @warning  `invfatal` if @p a is NULL.
 * @warning  `userraise` (ERR_OUT_OF_RANGE) if `i >= a->count`.
 */
static inline fs2slice
fs2arrslice_get(const fs2arrslice *a, size_t i) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null arslice");

    if (i >= a->count)
        userraise(0, ERR_OUT_OF_RANGE,
            "idx %zu >= count %zu", i, a->count);
    return fs2slice_new(a->parent, a->ranges[i].from, a->ranges[i].to);
}

/**
 * @brief Release the borrow held by a slice; reset it to canonical empty.
 *
 * Decrements `parent->borrow` and sets `s->parent = NULL`.
 * Safe to call on a NULL or already-freed (zombie) slice (no-op).
 *
 * @param[out] s  Slice to free. A NULL pointer is silently ignored.
 * @warning  `userraise` (ERR_FS2SLICE_UNDERFLOW) if `parent->borrow` is 0
 *           (double-free / underflow).
 */
static inline void
fs2slice_free(fs2slice *s) {
    if (!s || !s->parent)
        return;

    fs2slice_parentdec(&s->parent);
    s->range  = FS2SLICEVIEW();
}

/**
 * @brief Release the borrow held by an array-slice; free its range buffer;
 *        reset to canonical empty.
 *
 * Decrements `parent->borrow` by 1 (the array's own single borrow),
 * frees the `ranges` heap buffer, and resets all fields to zero/NULL.
 * Safe to call on a NULL or already-freed (zombie) value (no-op).
 *
 * @param[out] s  Array-slice to free. A NULL pointer is silently ignored.
 * @warning  `userraise` (ERR_FS2SLICE_UNDERFLOW) if `parent->borrow` is 0.
 */
static inline void
fs2arrslice_free(fs2arrslice *s) {
    if (!s || !s->parent)
        return;
    
    fs2slice_parentdec(&s->parent);
    free(s->ranges);
    *s = FS2ARRSLICE();
}

enum { FS2SLICE_TECH_PRINT_COUNT = 8 };   /* ranges shown in arrslice dump */

//
extern bool
fs2slice_validate(FILE *restrict out, const fs2slice *restrict s);

extern bool
fs2arrslice_validate(FILE *restrict out, const fs2arrslice *restrict a);

extern long
fs2slice_techfprint(FILE *restrict out, const fs2slice *restrict s, const char *restrict name);

extern long
fs2arrslice_techfprint(FILE *restrict out, const fs2arrslice *restrict a, const char *restrict name);

// macro-support
#define FS2SLICETECHFPRINT(out, s)    fs2slice_techfprint((out),    (s), #s)
#define FS2SLICETECHPRINT(s)          fs2slice_techfprint(stdout,   (s), #s)
#define FS2SLICETECHLOG(s)            fs2slice_techfprint(logfile,  (s), #s)

#define FS2ARRSLICETECHFPRINT(out, a) fs2arrslice_techfprint((out), (a), #a)
#define FS2ARRSLICETECHPRINT(a)       fs2arrslice_techfprint(stdout,(a), #a)
#define FS2ARRSLICETECHLOG(a)         fs2arrslice_techfprint(logfile,(a),#a)

// ----------------------------------- Comparators ------------------------------

/* fs2slice.h */

/* Comparator signature: length-bounded (memcmp-style).
 * Compares at most `n` bytes; return <0 / 0 / >0.
 * MUST NOT read past min(n, min-content-length) — no NUL lookahead. */
typedef int (*fs2slicecmp_t)(const char *a, const char *b, size_t n);

/* Common core: memcmp-style first, length tiebreak after.
 * `cmp` must be NUL-agnostic (see above). */
static inline int
fs2slice_slice_cmp_core(const char *a, size_t la,
                 const char *b, size_t lb,
                 fs2slicecmp_t cmp) {
    size_t n = la < lb ? la : lb;
    int    r = cmp(a, b, n);
    if (r != 0)
        return r;
    return (la > lb) - (la < lb);   /* shorter < longer */
}

/* Comparators (wrap stdlib to be proper function-pointer types;
 * memcmp has void* args, so wrap to keep strict signature match) */
static inline int
fs2slice_slice_cmp_cs(const char *a, const char *b, size_t n) { 
    return memcmp(a, b, n); 
}

static inline int
fs2slice_slice_cmp_ci(const char *a, const char *b, size_t n) { 
    return strncasecmp(a, b, n); 
}

/* --- CS --- */
static inline int
fs2slice_cmp(const fs2slice *a, const fs2slice *b) {
    invfatal(ERR_NULL_INPUT, a != NULL && b != NULL, 
        "Null slices %p %p", a, b);

    return fs2slice_slice_cmp_core(fs2slice_data(a), fs2slice_len(a),
                            fs2slice_data(b), fs2slice_len(b),
                            fs2slice_slice_cmp_cs);
}

static inline int
fs2slice_cmpfs2(const fs2slice *sl, const fs2 *s) {
    invfatal(ERR_NULL_INPUT, sl != NULL && s != NULL,
             "Null slice or fs2 %p %p", sl, s);

    return fs2slice_slice_cmp_core(fs2slice_data(sl), fs2slice_len(sl),
                            s->v ? s->v : "", s->len,
                            fs2slice_slice_cmp_cs);
}

static inline int
fs2slice_cmpstr(const fs2slice *sl, const char *str) {
    invfatal(ERR_NULL_INPUT, sl != NULL && str != NULL,
             "Null slice or c-str %p %p", sl, str);

    return fs2slice_slice_cmp_core(fs2slice_data(sl), fs2slice_len(sl),
                            str, strlen(str),
                            fs2slice_slice_cmp_cs);
}

/* --- CI --- */
static inline int
fs2slice_cmp_ci(const fs2slice *a, const fs2slice *b) {
    invfatal(ERR_NULL_INPUT, a != NULL && b != NULL, 
        "Null slices %p %p", a, b);

    return fs2slice_slice_cmp_core(fs2slice_data(a), fs2slice_len(a),
                            fs2slice_data(b), fs2slice_len(b),
                            fs2slice_slice_cmp_ci);
}

static inline int
fs2slice_cmpfs2_ci(const fs2slice *sl, const fs2 *s) {
    invfatal(ERR_NULL_INPUT, sl != NULL && s != NULL,
             "Null slice or fs2 %p %p", sl, s);

    return fs2slice_slice_cmp_core(fs2slice_data(sl), fs2slice_len(sl),
                            s->v ? s->v : "", s->len,
                            fs2slice_slice_cmp_ci);
}

static inline int
fs2slice_cmpstr_ci(const fs2slice *sl, const char *str) {
    invfatal(ERR_NULL_INPUT, sl != NULL && str != NULL,
             "Null slice or c-str %p %p", sl, str);

    return fs2slice_slice_cmp_core(fs2slice_data(sl), fs2slice_len(sl),
                            str, strlen(str),
                            fs2slice_slice_cmp_ci);
}

/**
 * @brief Pointer to byte at offset @p pos within the slice.
 *
 * @p pos is a 0-based offset **from @c slice.range.from**, not from
 * parent. Bounds are checked against the slice length (range.to - range.from),
 * not the parent. This is the whole point — the slice is a "window",
 * reading past `slice.to` would silently expose parent content.
 *
 * @param s    Non-NULL, non-zombie slice.
 * @param pos  Offset in [0, slice_len).
 * @return     @c parent->v + range.from + pos. Dereferencing when
 *             @c pos < slice_len is safe (byte is in parent buffer).
 * @warning `invfatal` (SIGABRT) if s is NULL/zombie, or pos >= slice_len.
 */
static inline char *
fs2slice_get(const fs2slice *s, size_t pos) {
    invfatal(ERR_NULL_INPUT, s != NULL, "Null slice");

    if (s->parent == NULL)
        userraise(NULL, ERR_FS2SLICE_ZOMBIE,
            "Zombie slice, null parent");

    if (pos >= fs2slice_len(s))
        userraise(NULL, ERR_OUT_OF_RANGE,
             "fs2slice_get: pos %zu >= slice_len %zu",
             pos, fs2slice_len(s));

    return s->parent->v + s->range.from + pos;
}

#define fs2sliceget(s, pos) *fs2slice_get((&s), (pos))

/* =========================================================================
 * Extraction — return NEW owned fs2 from slice content.
 * Slice is read-only; there is no in-place variant.
 * ========================================================================= */

/* Clamp [from, len) against slice_len (not parent->len). */
/* Reads ONLY s->len. Do not add checks/reads of s->v, s->flags, s->sz,
 * s->borrow without auditing fs2slice_substr_bounds (which passes
 * a fake fs2 with only .len set). */
static inline void
fs2slice_substr_bounds(const fs2slice *sl, size_t *from, size_t *len) {
    if (sl->parent == NULL)
        userraise(0, ERR_FS2SLICE_ZOMBIE, "Zombie, unable to calc bounds");
    fs2_substr_bounds(&( (fs2) { .len = fs2slice_len(sl) } ), from, len);
}

/**
 * @brief New owned fs2 = slice content [@p from, @p from + @p len).
 *
 * Offsets are **relative to slice start** (`slice.range.from` in parent),
 * NOT from parent. Bounds are clamped to `slice_len`, not `parent->len`.
 * Parent is untouched — returns independent deep copy.
 *
 *   parent = "0123456789"
 *   slice  = [3, 8)  →  "34567"
 *   fs2slice_substr_new(&slice, 1, 3) → "456"     (parent[4..7))
 *   fs2slice_substr_new(&slice, 0, 5) → "34567"   (whole slice)
 *   fs2slice_substr_new(&slice, 0, 100) → "34567" (clamped)
 *   fs2slice_substr_new(&slice, 5, 1) → ""        (from == slice_len)
 *   fs2slice_substr_new(&slice, 100, 1) → ""      (from > slice_len)
 *
 * @param sl    Non-NULL, non-zombie slice.
 * @param from  Start offset within slice.
 * @param len   Length to extract (clamped).
 * @return      New fs2 (ALLOC); FS2EMPTY for zero-length result.
 * @warning `invfatal` (SIGABRT) if sl is NULL/zombie.
 */
static inline fs2
fs2slice_substr_new(const fs2slice *sl, size_t from, size_t len) {
    invfatal(ERR_NULL_INPUT, sl != NULL, "Null slice");

    if (sl->parent == NULL)
        userraise(0, ERR_NULL_INPUT, "Zombie slice");

    fs2slice_substr_bounds(sl, &from, &len);
    if (len == 0)
        return FS2EMPTY;

    fs2 res = FS2();
    fs2_memcpy_str(&res, 0,
                   sl->parent->v + sl->range.from + from, len);
    fs2_setlen(&res, len);
    return res;
}

/**
 * @brief New owned fs2 = whole slice content (deep copy).
 *
 * Rust-flavored alias for `.to_owned()`.
 */
static inline fs2
fs2slice_to_owned(const fs2slice *sl) {
    invfatal(ERR_NULL_INPUT, sl != NULL, "Null slice");

    return fs2slice_substr_new(sl, 0, fs2slice_len(sl));
}

/**
 * @brief New owned fs2 = first @p n bytes of slice (clamped to slice_len).
 *
 * @p n == 0 → FS2EMPTY. @p n >= slice_len → clone of whole slice.
 */
static inline fs2
fs2slice_left_new(const fs2slice *sl, size_t n) {
    invfatal(ERR_NULL_INPUT, sl != NULL, "Null slice");

    return fs2slice_substr_new(sl, 0, n);
}

/**
 * @brief New owned fs2 = last @p n bytes of slice (clamped to slice_len).
 *
 * @p n == 0 → FS2EMPTY. @p n >= slice_len → clone of whole slice.
 */
static inline fs2
fs2slice_right_new(const fs2slice *sl, size_t n) {
    invfatal(ERR_NULL_INPUT, sl != NULL, "Null slice");

    size_t sl_len = fs2slice_len(sl);
    if (n >= sl_len)
        return fs2slice_to_owned(sl);
    return fs2slice_substr_new(sl, sl_len - n, n);
}

/* Rust-style alias — same as fs2slice_to_owned */
static inline fs2
fs2slice_clone(const fs2slice *sl) {
    return fs2slice_to_owned(sl);
}

/* =========================================================================
 * Case-fold — owned copy with case transform. Slice/parent untouched.
 * ========================================================================= */

/**
 * @brief New owned fs2 = slice content in UPPER case.
 *
 * Equivalent to `fs2_toupper(fs2slice_to_owned(sl))`, but expressed as
 * one call. Parent and slice are NOT modified (read-only). Empty slice
 * (impossible via fs2slice_new — from < to enforced) would yield FS2EMPTY.
 *
 * @param sl  Non-NULL, non-zombie slice.
 * @return    New fs2 (ALLOC); caller frees with fs2_free.
 * @warning `invfatal` (SIGABRT) if sl is NULL/zombie.
 * @note ASCII-only in C locale (same as fs2_toupper).
 */
static inline fs2
fs2slice_toupper_new(const fs2slice *sl) {
    fs2 r = fs2slice_to_owned(sl);
    fs2_toupper(&r);
    return r;
}

/**
 * @brief New owned fs2 = slice content in lower case.
 *
 * Same contract as fs2slice_toupper_new, opposite direction.
 */
static inline fs2
fs2slice_tolower_new(const fs2slice *sl) {
    fs2 r = fs2slice_to_owned(sl);
    fs2_tolower(&r);
    return r;
}

 #endif /* !FS2SLICE_H */
