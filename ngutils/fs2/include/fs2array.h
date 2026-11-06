#ifndef FS2ARRAY_H
#define FS2ARRAY_H

#include "fs2.h"

typedef struct fs2array {
    fs2         *arr;   // heap array of fs2 values (never BODYALLOC inside)
    size_t       hwm;   // high position
    size_t       size;    // capacity in elements
    size_t       count;    // total count of valid elements
} fs2array;

/** Canonical empty fs2array (no allocation). */
#define FS2ARR()  (fs2array){ .arr = NULL, .hwm = 0, .size = 0, .count = 0 }

/* A hole slot has FS2_NULL_ARRAY_ELEMENT set and nothing else.
 * Never produced by fs2 constructors; only by fs2arr operations. */
static inline bool
fs2arr_is_hole(const fs2 *slot) {
    return slot->flags == FS2_NULL_ARRAY_ELEMENT;
}

static inline void
fs2arr_set_hole(fs2 *slot) {
    slot->len   = 0;
    slot->sz    = 0;
    slot->flags = FS2_NULL_ARRAY_ELEMENT;
    slot->v     = NULL;
}

/**
 * @brief Allocate an fs2array with capacity for @p cnt elements.
 *
 * Capacity is exact (`a.sz == cnt`), not rounded. `len` is 0 — no
 * elements are populated. New slots are initialized to FS2EMPTY (safe
 * to fs2_free / fs2_move).
 *
 * @param cnt  Element capacity (0 → canonical empty, arr == NULL).
 * @return     fs2array with arr != NULL if cnt > 0; free with fs2arr_free.
 * @warning    `sysraise` (OOM / overflow) if allocation fails.
 */
extern fs2array
fs2arr_init(size_t cnt);

extern fs2 *
fs2arr_reserve(fs2array *a, size_t need);

extern void
fs2arr_free(fs2array *arr);

extern void 
fs2arr_clean(fs2array *a);

/**
 * @brief Find the first FS2EMPTY slot in [0, len).
 *
 * Internal helper: holes in [0, len) can appear when a slot is manually
 * cleared or (in a future non-shifting detach) after an element is removed
 * without compacting. Returns SIZE_MAX if no hole.
 *
 * @note Holes within [len, sz) are the padding region and are excluded —
 *       that's push territory, not reuse.
 */
extern ptrdiff_t
fs2arr_find_tillhwm(const fs2array *a);

extern fs2 *
fs2arr_attach(fs2array *a, fs2 *src, size_t idx);


/**
 * @brief Append a value to the end of the array.
 *
 * Source may be stack, field, or heap (BODYALLOC). Value is moved into
 * the slot via fs2_move; the source keeps its BODYALLOC shell (if any).
 *
 * @param a    Non-NULL fs2array.
 * @param val  Non-NULL, any flavor.
 * @return     Index of the appended element.
 *
 * @note For BODYALLOC sources, the caller owns the shell afterwards and
 *       must call `fs2_free(val)` (frees just the struct; buffer already
 *       moved into the array). Use `fs2arr_attach(&a, &p)` for automatic
 *       shell cleanup + pointer nulling.
 * @note Do not pass `&a->arr[i]` — if `push` triggers realloc, `val`
 *       becomes dangling.
 */
static inline fs2 *
fs2arr_push(fs2array *a, fs2 *src) {
    invfatal(ERR_NULL_INPUT, a != NULL && src != NULL, 
        "Null pointers %p %p", a, src);

    if (a->hwm == a->size)
        fs2arr_reserve(a, a->hwm + 1);
    return fs2arr_attach(a, src, a->hwm);
}

/**
 * @brief Move element @p idx out into @p dst, leaving a hole.
 *
 * Slot @p idx becomes FS2EMPTY (hole in [0, hwm)). No shift, no compaction.
 * `hwm` unchanged, `count` decremented. The hole can be reused later by
 * `fs2arr_attach(a, val, idx)` or found by `fs2arr_find_tillhwm`.
 *
 * @param a    Non-NULL fs2array.
 * @param idx  Slot index, 0 <= idx < a->hwm.
 * @param dst  Non-NULL output. May be stack fs2 (no BODYALLOC) or heap
 *             fs2* (BODYALLOC preserved). Its previous buffer is freed
 *             via fs2_move if ALLOC.
 *
 * @warning `invfatal` (SIGABRT): a/dst NULL, idx >= hwm, slot already
 *          empty (cannot detach a hole).
 */
extern bool
fs2arr_detachto(fs2array *a, size_t idx, fs2 *dst);

/** Stack-return convenience. */
static inline fs2
fs2arr_detach_new(fs2array *a, size_t idx) {
    fs2 tmp = FS2();
    if (!fs2arr_detachto(a, idx, &tmp))
        userraise(NULL, ERR_ACTION_NOT_APPLICABLE, 
            "Unable to detach %zu", idx);
    return tmp;                          /* caller owns buffer, must fs2_free */
}

/** Heap-return convenience (BODYALLOC | ALLOC). */
static inline fs2 *
fs2arr_heap_detach(fs2array *a, size_t idx) {
    fs2 *p = fs2_heap();                // canonical FS2() | BODYALLOC 
    if (!fs2arr_detachto(a, idx, p)) {
        fs2_free(p);                  // frees struct (buffer was NULL anyway) 
        userraise(NULL, ERR_ACTION_NOT_APPLICABLE, 
            "Unable to detach %zu", idx);
    }
    return p;                           /* caller frees via fs2_free */
}

#endif /* !FS2ARRAY_H */

