/** @file
 * THREAD SAFETY: fs2slice operations on the SAME parent fs2 are not
 * thread-safe. `parent->borrow` is a plain uint32_t; concurrent
 * fs2slice_new / fs2slice_free on one parent require external locking.
 * Different parents, or read-only use of independent slices — safe.
 * 
 * 
 * @warning fs2slice is a RAII-like handle. Copying (`fs2slice b = a`)
 *          does NOT increment `parent->borrow`. Freeing either copy
 *          leaves the other dangling (parent->borrow == 0) or causes
 *          UNDERFLOW (double free). Pass by pointer (`const fs2slice *`)
 *          or wrap in a container that manages lifetime; never shallow-copy.
 */

#ifndef FS2SLICE_H
#define FS2SLICE_H

#include "fs2.h"

typedef struct fs2sliceview {
    size_t from;
    size_t to;
} fs2sliceview;

#define FS2SLICEVIEW() (fs2sliceview) {.from = 0, .to = 0}

typedef struct fs2arrslice {
    fs2          *parent;    /* array owns the single borrow */
    fs2sliceview *ranges;
    size_t        hwm, size, count;
} fs2arrslice;

#define FS2ARRSLICE() (fs2arrslice){ .parent = NULL, .ranges = NULL, \
                                     .hwm = 0, .size = 0, .count = 0 }

typedef struct fs2slice {
    fs2             *parent;   /* reads parent->v each access — realloc-safe */
    fs2sliceview     range;
} fs2slice;                      

#define FS2SLICE() (fs2slice){ .parent = NULL, .range = FS2SLICEVIEW() }

// leak-checking facility
extern long
fs2slice_inc(void);

extern long
fs2slice_dec(void);

extern long 
fs2slice_sum(void);

// -----

static inline size_t
fs2slice_len(const fs2slice *s) { 
    invfatal(ERR_FS2SLICE_ZOMBIE, s != NULL && s->parent != NULL,
        "Null or Zombie %p %p", s, s ? s->parent: NULL);
    return s->range.to - s->range.from; 
}

static inline const char *
fs2slice_data(const fs2slice *s) { 
    invfatal(ERR_FS2SLICE_ZOMBIE, s != NULL && s->parent != NULL,
        "Null or Zombie %p %p", s, s ? s->parent: NULL);
    return s->parent->v + s->range.from; 
}

// internal
static inline int32_t
fs2slice_parentinc(fs2 *parent) {
    fs2slice_inc();
    return ++parent->borrow;       // not atomic: external lock required (see header)
}

// internal
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
 * @warning fs2slice is a move-only handle. `fs2slice b = a` does NOT
 *        increment borrow. Freeing both → underflow; freeing one →
 *        orphaned parent (borrow == 0). Pass `const fs2slice *` for
 *        read-only, or use fs2slice_copy() for a proper borrow copy.
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

static inline fs2slice
fs2arrslice_get(const fs2arrslice *a, size_t i) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null arslice");

    if (i >= a->count)
        userraise(0, ERR_OUT_OF_RANGE,
            "idx %zu >= count %zu", i, a->count);
    return fs2slice_new(a->parent, a->ranges[i].from, a->ranges[i].to);
}

static inline void
fs2slice_free(fs2slice *s) {
    if (!s || !s->parent)
        return;

    fs2slice_parentdec(&s->parent);
    s->range  = FS2SLICEVIEW();
}

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

 #endif /* !FS2SLICE_H */
