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

/* Content-of-slot truncation uses FS2_TECH_PRINT_COUNT (same as fs2_techfprint).
 * Number of slots shown in array dump: FS2ARR_TECH_PRINT_COUNT. */
enum { FS2ARR_TECH_PRINT_COUNT = 8 };

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

/**
 * @brief Free live elements in [from, to), reset them to holes.
 *
 * Range is over physical slots, bounded by `a->size` (not `hwm`) — caller
 * may clean padding / trimmed-tail regions defensively. Live elements in
 * the range are `fs2_free`d, `count` is decremented per freed element,
 * `hwm`/`size` unchanged.
 *
 * @param a     Non-NULL fs2array.
 * @param from  Start index (inclusive).
 * @param to    End index (exclusive), 0 <= from <= to <= a->size.
 * @return      Number of live elements actually freed (holes skipped).
 * @warning     `invfatal` (SIGABRT) on NULL / bad range / count underflow.
 */
size_t
fs2arr_clean(fs2array *a, size_t from, size_t to);

extern void 
fs2arr_cleanall(fs2array *a);

/**
 * @brief Find the first FS2EMPTY slot in [0, len).
 *
 * holes in [0, len) can appear when a slot is manually
 * cleared or (in a future non-shifting detach) after an element is removed
 * without compacting. Returns SIZE_MAX if no hole.
 *
 * @note Holes within [len, sz) are the padding region and are excluded —
 *       that's push territory, not reuse.
 */
extern ptrdiff_t
fs2arr_find_tillhwm(const fs2array *a);

size_t 
fs2arr_synchwm(fs2array *a);

bool
fs2arr_shrink(fs2array *a);

/* fs2array.h */
/**
 * @brief Attach stack/field source into slot @p idx.
 * @return Index of the slot (= idx on success), or (ptrdiff_t)-1 on raise
 *         (but raises abort — never returns -1; the sentinel exists only
 *         for symmetry with dp variant).
 */
extern ptrdiff_t
fs2arr_attach_sp(fs2array *a, fs2 *src, size_t idx);

/**
 * @brief Attach with auto-null (heap source, validate-first).
 * @return Index of the slot (== idx on success), or -1 if *src == NULL.
 */
extern ptrdiff_t
fs2arr_attach_dp(fs2array *a, fs2 **src, size_t idx);

/**
 * @brief Generic attach — dispatches on `_Generic` static type of @p src.
 *
 *   fs2 s = ...;            fs2arr_attach(&a, &s, idx);   // → attach_sp
 *   fs2 *p = fs2_heap_…();  fs2arr_attach(&a, &p, idx);   // → attach_dp
 *
 * Const-qualified sources are rejected at compile time (can't move from
 * const).
 */
#define fs2arr_attach(a, src, idx)                                        \
    _Generic((src),                                                       \
        fs2  *:  fs2arr_attach_sp,                                        \
        fs2 **:  fs2arr_attach_dp                                         \
    )((a), (src), (idx))


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
 *       moved into the array). Use `fs2arr_attach_sp(&a, &p)` for automatic
 *       shell cleanup + pointer nulling.
 * @note Do not pass `&a->arr[i]` — if `push` triggers realloc, `val`
 *       becomes dangling.
 */
static inline ptrdiff_t
fs2arr_push_sp(fs2array *a, fs2 *src) {
    invfatal(ERR_NULL_INPUT, a != NULL && src != NULL, 
        "Null pointers %p %p", a, src);

    if (a->hwm == a->size)
        fs2arr_reserve(a, a->hwm + 1);
    return fs2arr_attach_sp(a, src, a->hwm);
}

static inline ptrdiff_t
fs2arr_push_dp(fs2array *a, fs2 **src) {     /* heap source, auto-null */
    invfatal(ERR_NULL_INPUT, a != NULL && src != NULL, "Null pointers");
    if (a->hwm == a->size)
        fs2arr_reserve(a, a->hwm + 1);
    return fs2arr_attach_dp(a, src, a->hwm);
}

#define fs2arr_push(a, src)                                               \
    _Generic((src),                                                       \
        fs2  *:  fs2arr_push_sp,                                          \
        fs2 **:  fs2arr_push_dp                                           \
    )((a), (src))

/**
 * @brief Move element @p idx out into @p dst, leaving a hole.
 *
 * Slot @p idx becomes FS2EMPTY (hole in [0, hwm)). No shift, no compaction.
 * `hwm` unchanged, `count` decremented. The hole can be reused later by
 * `fs2arr_attach_sp(a, val, idx)` or found by `fs2arr_find_tillhwm`.
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

/**
 * @brief Print a one-slot-per-line technical dump of an fs2array.
 *
 * Format:
 *   FS2ARR: <name>: hwm [N], size [M], count [K], arr(<ptr>)[
 *     [0] len [..], sz [..], flags [0x..], v(<ptr>)[content...]
 *     [1] <hole>
 *     ...
 *     ... (X more slots)
 *     padding [hwm, size) = P holes
 *   ]
 *
 * Slots [0, min(hwm, FS2ARR_TECH_PRINT_COUNT)) shown; the rest summarised.
 * Holes rendered as "<hole>". Padding [hwm, size) always all holes — noted.
 *
 * @param out   Output stream (NULL → return 0, no-op).
 * @param a     fs2array to dump (NULL → prints "<NULL>").
 * @param name  Label (NULL → "(unnamed)").
 * @return      Total bytes written, or -1 on I/O error.
 */
extern long
fs2arr_techfprint(FILE *restrict out, const fs2array *restrict a, const char *restrict name);

#define FS2ARRTECHFPRINT(out, a)   fs2arr_techfprint((out), (a), #a)
#define FS2ARRTECHPRINT(a)         fs2arr_techfprint(stdout, (a), #a)
#define FS2ARRTECHLOG(a)           fs2arr_techfprint(logfile, (a), #a)

/**
 * @brief Validate all invariants of an fs2array.
 *
 * Checks:
 * 1. count <= hwm <= size.
 * 2. arr == NULL only in canonical empty (hwm == size == count == 0).
 * 3. [0, hwm): each slot is either a hole (FS2_NULL_ARRAY_ELEMENT) or a
 *    valid fs2 (fs2_validate passes); count of live slots == count.
 * 4. [hwm, size): every slot is a hole (padding invariant).
 *
 * On failure, a diagnostic is written to @p out (if non-NULL) and to the
 * internal error log. For a slot-level failure, fs2_validate's own
 * diagnostic is emitted first (specific reason), then a context line
 * ("slot[N] invalid fs2").
 *
 * @param out  Optional output stream; NULL suppresses diagnostic to file
 *             (still logs via logsimpleerr).
 * @param a    fs2array to validate; NULL → reports and returns false.
 * @return     true iff all invariants hold.
 */
extern bool
fs2arr_validate(FILE *restrict out, const fs2array *restrict a);

#endif /* !FS2ARRAY_H */

