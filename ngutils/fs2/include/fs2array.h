#ifndef FS2ARRAY_H
#define FS2ARRAY_H

#include "fs2.h"
#include "fs2_internal_utils.h"

typedef struct fs2array {
    fs2         *arr;   // heap array of fs2 values (never BODYALLOC inside)
    size_t       hwm;   // high position
    size_t       size;    // capacity in elements
    size_t       count;    // total count of valid elements
} fs2array;

/**
 * @brief Canonical empty fs2array (no allocation).
 *
 * Equivalent to `(fs2array){0}` — all fields zero, `arr == NULL`.
 * `fs2arr_free` is a no-op; `fs2arr_reserve` will allocate on first use.
 *
 * @code
 *   fs2array a = FS2ARR();
 *   fs2arr_reserve(&a, 64);
 * @endcode
 */
#define FS2ARR()  (fs2array){ .arr = NULL, .hwm = 0, .size = 0, .count = 0 }

/**
 * @brief Mark a slot as a hole (FS2EMPTY + FS2_NULL_ARRAY_ELEMENT flag).
 *
 * The caller must have already `fs2_free`d the slot — this only resets
 * the shell to `FS2()` and sets the hole sentinel flag.
 *
 * @param slot  Non-NULL pointer to the fs2 slot to turn into a hole.
 */
static inline void
fs2arr_set_hole(fs2 *slot) {
    *slot = FS2();
    slot->flags = FS2_NULL_ARRAY_ELEMENT;
}

/* Content-of-slot truncation uses FS2_TECH_PRINT_COUNT (same as fs2_techfprint).
 * Number of slots shown in array dump: FS2ARR_TECH_PRINT_COUNT. */
enum { FS2ARR_TECH_PRINT_COUNT = 8 };

/**
 * @brief Check if the array is packed: no holes in [0, hwm).
 *
 * Packed ⟺ hwm == count (all slots in the logical range are live, holes
 * appear only in padding [hwm, size)). True after push-only construction,
 * clone, compact, sort, unique, merge. False after detach (until compact).
 *
 * Required precondition for: fs2arr_sort, fs2arr_unique, fs2arr_reverse,
 * fs2arr_issorted, fs2arr_mergesorted_new, fs2arr_bsearch.
 *
 * @param a  Non-NULL fs2array.
 * @return   a->hwm == a->count.
 * @warning  NO null check on @p a — NULL deref like fs2_is_hole.
 */
static inline bool
fs2arr_ispacked(const fs2array *a) {
    return a->hwm == a->count;
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

/**
 * @brief Deep copy of an fs2array (compacted, tight).
 *
 * Result has hwm == size == count (no padding, no holes), elements in
 * original relative order (holes skipped). All element buffers are
 * freshly allocated copies — mutating the clone does not affect @p src
 * and vice versa.
 *
 * @param src  fs2array to clone (NULL or canonical → returns FS2ARR()).
 * @return     New fs2array; free with fs2arr_free.
 * @warning    `sysraise` (OOM) if allocation fails.
 */
extern fs2array
fs2arr_clone(const fs2array *src);

/**
 * @brief Exact (position-preserving) deep copy of an fs2array.
 *
 * Unlike `fs2arr_clone` (which compacts and drops holes), this preserves
 * the full layout: `hwm`, slot-by-slot hole positions, and `size` are all
 * replicated. Result has `hwm == src->hwm`, `count == src->count`,
 * `size >= src->hwm`. Elements are deep-copied — mutating one array does
 * not affect the other.
 *
 * @param src  fs2array to clone (NULL → `invfatal`).
 * @return     New fs2array; free with `fs2arr_free`.
 * @warning    `invfatal` (SIGABRT) if @p src is NULL.
 * @warning    `sysraise` (OOM) if allocation fails.
 */
extern fs2array
fs2arr_cloneexact(const fs2array *src);

/**
 * @brief Ensure capacity for at least @p need elements.
 *
 * Grows `a->size` by power-of-2 (floor `FS2_MIN_ALLOC`) via `realloc`.
 * No-op if current `size >= need`. New slots (old `size` … new `size`)
 * are initialised to holes. `hwm` / `count` unchanged.
 *
 * @param a     Non-NULL fs2array.
 * @param need  Target minimum capacity in elements.
 * @return      `a->arr` (convenience for chaining).
 * @warning     `invfatal` (SIGABRT) if @p a is NULL.
 * @warning     `sysraise` (OOM / overflow) if `realloc` fails.
 */
extern fs2 *
fs2arr_reserve(fs2array *a, size_t need);

/**
 * @brief Free all resources held by an fs2array and reset to canonical.
 *
 * Frees every live element (via `fs2arr_cleanall`), then `free`s the
 * backing buffer, and resets the struct to `FS2ARR()`. The struct itself
 * is never freed — only its heap buffer — so it is safe to call on a
 * stack-allocated array or a canonical one.
 *
 * @param arr  fs2array to free (NULL → no-op, returns silently).
 */
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

/**
 * @brief Free all live elements and reset `hwm` / `count` to 0.
 *
 * Equivalent to `fs2arr_clean(a, 0, a->hwm)` followed by
 * `a->hwm = a->count = 0`. The backing buffer (`a->arr`, `a->size`) is
 * kept — capacity is preserved for future pushes. Canonical
 * (`arr == NULL`) → no-op.
 *
 * @param a  Non-NULL fs2array.
 * @warning  `invfatal` (SIGABRT) if @p a is NULL.
 */
extern void
fs2arr_cleanall(fs2array *a);

/**
 * @brief Find the first hole slot in [0, hwm).
 *
 * Holes in [0, hwm) appear after `fs2arr_detachto` or manual slot
 * clearing. Returns the index of the first hole, or **-1** if the
 * logical region is fully live.
 *
 * @note Holes in the padding [hwm, size) are excluded — that is push
 *       territory, not reuse.
 *
 * @param a  Non-NULL fs2array.
 * @return   Index of first hole in [0, hwm), or -1 if none.
 * @warning  `invfatal` (SIGABRT) if @p a is NULL.
 */
extern ptrdiff_t
fs2arr_find_tillhwm(const fs2array *a);

/**
 * @brief Trim trailing holes from `hwm` (logical shrink from the tail).
 *
 * While `a->arr[a->hwm - 1]` is a hole, `hwm` is decremented. No-op if
 * the last logical slot is live. `size` / `count` unchanged.
 *
 * @param a  Non-NULL fs2array.
 * @return   New value of `hwm`.
 * @warning  `invfatal` (SIGABRT) if @p a is NULL.
 */
size_t
fs2arr_synchwm(fs2array *a);

/**
 * @brief Shrink the backing buffer to exactly `hwm` elements.
 *
 * `realloc`s `a->arr` to `hwm * sizeof(fs2)`. Internal holes are
 * preserved (only the tail padding is trimmed). If `hwm == 0` the buffer
 * is freed entirely and the array becomes canonical. If `hwm == size`
 * this is a no-op.
 *
 * @param a  Non-NULL fs2array.
 * @return   true on success or no-op; false if `realloc` fails
 *           (original buffer remains valid).
 * @warning  `invfatal` (SIGABRT) if @p a is NULL.
 */
bool
fs2arr_shrink(fs2array *a);

/**
 * @brief Pack live slots to [0, count), holes to [count, size).
 *
 * Stable (relative order of live slots preserved). In-place, no realloc,
 * O(hwm). After:
 *   - hwm = count (logical end = last live)
 *   - [0, hwm)   — all live, in original relative order
 *   - [hwm, size) — all holes (padding)
 *
 * Equivalent to "trim trailing holes + drop mid holes" in one shot.
 * `fs2arr_synchwm` after compact is a no-op. `size` unchanged; follow
 * with `fs2arr_shrink` to reduce capacity if desired.
 *
 * Canonical (`arr == NULL`) — no-op.
 *
 * @warning `invfatal` (SIGABRT) if a is NULL, or if internal `count`
 *          doesn't match actual live slots (array corruption).
 */
extern void
fs2arr_compact(fs2array *a);

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

/**
 * @brief Append a heap-allocated fs2* to the end (auto-null source).
 *
 * Same as `fs2arr_push_sp` but takes `fs2 **`. After successful attach
 * `*src` is set to NULL and the shell is freed — safe for `fs2_heap_*`
 * / `fs2arr_heap_*` return values.
 *
 * @param a    Non-NULL fs2array.
 * @param src  Non-NULL pointer to a heap `fs2 *` (auto-nulled on success).
 * @return     Index of the appended element.
 * @warning    `invfatal` (SIGABRT) if @p a or @p src is NULL.
 * @note       Do not pass `&a->arr[i]` — if push triggers realloc,
 *             the pointer becomes dangling.
 */
static inline ptrdiff_t
fs2arr_push_dp(fs2array *a, fs2 **src) {     /* heap source, auto-null */
    invfatal(ERR_NULL_INPUT, a != NULL && src != NULL, "Null pointers");
    if (a->hwm == a->size)
        fs2arr_reserve(a, a->hwm + 1);
    return fs2arr_attach_dp(a, src, a->hwm);
}

/**
 * @brief Generic push — dispatches on `_Generic` static type of @p src.
 *
 *   fs2 s = ...;            fs2arr_push(&a, &s);    // → push_sp
 *   fs2 *p = fs2_heap_…();  fs2arr_push(&a, &p);   // → push_dp
 *
 * @param a    Non-NULL fs2array.
 * @param src  `fs2 *` or `fs2 **` (any flavor).
 * @return     Index of the appended element.
 */
#define fs2arr_push(a, src)                                               \
    _Generic((src),                                                       \
        fs2  *:  fs2arr_push_sp,                                          \
        fs2 **:  fs2arr_push_dp                                           \
    )((a), (src))

/**
 * @brief Append a C string to the array (convenience wrapper).
 *
 * Creates a temporary `fs2` from @p str via `fs2_fromstr`, pushes it,
 * and the temporary is auto-freed (moved into the slot).
 *
 * @param a    Non-NULL fs2array.
 * @param str  Non-NULL null-terminated string (NULL → `invfatal`).
 * @return     Index of the appended element.
 * @warning    `invfatal` (SIGABRT) if @p a or @p str is NULL.
 */
static inline ptrdiff_t
fs2arr_pushstr(fs2array *restrict a, const char *restrict str) {
    fs2 tmp = fs2_fromstr(str);
    return fs2arr_push(a, &tmp);
}

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
 * @brief Get pointer to slot @p idx (may be a hole).
 *
 * Returns `&a->arr[idx]` if `idx < a->hwm`, NULL otherwise. The slot may
 * be a hole (check `fs2_is_hole`) or a live element. For **compacted**
 * arrays (after push-only or `fs2arr_compact`), all slots `[0, count)`
 * are live — no hole-check needed.
 *
 * @param a    Non-NULL fs2array.
 * @param idx  0-based slot index.
 * @return     Pointer to slot, or NULL if idx >= hwm.
 * @warning    `invfatal` (SIGABRT) if a is NULL.
 */
static inline const fs2 *
fs2arr_get(const fs2array *a, size_t idx) {
    invfatal(ERR_NULL_INPUT, a != NULL,
        "Null pointers %p", a);
    if (idx >= a->hwm)
        return NULL;
    return &a->arr[idx];
}

/* fs2array.h */
/**
 * @brief Exact comparison: same hwm, same slots [0, hwm) position-by-position.
 *
 * Two slots match iff both are holes, or both are live and their contents
 * are equal (fs2_cmp == 0, semantic / strcmp-style). `size` (capacity) is
 * ignored — it's a capacity hint, not data.
 *
 * Empty arrays (hwm == 0) are equal regardless of arr / size (canonical
 * vs empty-with-capacity). NULL handling: both NULL → true, one NULL → false.
 *
 * @param a  First array (may be NULL).
 * @param b  Second array (may be NULL).
 * @return   true if arrays are "exactly equal up to hwm".
 */
extern bool
fs2arr_equalexact(const fs2array *a, const fs2array *b);

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

/** @brief Technical dump of @p a to stream @p out (macro shorthand). */
#define FS2ARRTECHFPRINT(out, a)   fs2arr_techfprint((out), (a), #a)
/** @brief Technical dump of @p a to `stdout`. */
#define FS2ARRTECHPRINT(a)         fs2arr_techfprint(stdout, (a), #a)
/** @brief Technical dump of @p a to the global `logfile` stream. */
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

