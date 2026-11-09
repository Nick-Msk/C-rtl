#ifndef FS2ARRAY_API_H
#define FS2ARRAY_API_H

#include "fs2array.h"
#include "fs2_internal_utils.h"

/**
 * @brief Split @p s into an array of parts, using @p delim.
 *
 * Python/PG-style non-overlapping split:
 *   - Each occurrence of @p delim is a separator (skipped, not included).
 *   - Empty parts between consecutive delimiters are kept: "a,,b" → ["a","","b"].
 *   - Leading/trailing delimiters produce empty first/last parts:
 *     ",a," → ["","a",""].
 *   - Empty @p delim → no split: whole @p s becomes a single part ["..."],
 *     even when @p s is empty → [""].
 *   - Empty @p s with non-empty @p delim → [""] (one empty part, Python
 *     semantics; consistent with fs2_split_part("",",",1) == "").
 *
 * Result is packed (all slots [0, count) live, no holes) and tight
 * (size == count after final shrink, if any).
 *
 * @param s      Non-NULL fs2 to split.
 * @param delim  Non-NULL fs2 delimiter (may be empty → single part).
 * @return       New fs2array (packed, tight); free with fs2arr_free.
 * @warning     `invfatal` (SIGABRT) if s or delim is NULL.
 * @warning     `sysraise` (OOM) on allocation failure.
 *
 * @note Complexity O(|s|) amortised; no per-part allocation beyond the
 *       part buffers themselves (each fs2_substr_new allocates its own).
 * @note This method violate the rule _new, because of historical reason.
 */
extern fs2array
fs2_split_toarray(const fs2 *restrict s, const fs2 *restrict delim);

static inline fs2array
fs2_split_toarray_str(const fs2 *restrict s, const char *restrict delim) {
    fs2 d = _fs2_wrap(delim);
    return fs2_split_toarray(s, &d);
}

/**
 * @brief Sort slots [0, hwm) in place.
 *
 * Sorts ALL slots in [0, hwm), including holes. The comparator determines
 * hole placement:
 *
 *   fs2_cmp_wrap / fs2_cmp_desc_wrap
 *       — plain lexicographic; holes have v==NULL → SEGV. Caller must
 *         ensure hwm == count (no holes). Use after fs2arr_compact.
 *
 *   fs2_cmp_nullbefore_wrap / fs2_cmp_desc_nullbefore_wrap
 *       — "null < everything"; holes sorted to logical position. In asc,
 *         holes go to the front; in desc, to the end (== packed).
 *
 *   fs2_cmp_nullafter_wrap / fs2_cmp_desc_nullafter_wrap
 *       — "null > everything"; in asc, holes go to the end (== packed);
 *         in desc, to the front.
 *
 * "Packed" variants (holes at [count, hwm)) — sortasc_nullafter and
 * sortdesc_nullbefore — act as compact + sort in one call.
 *
 * count, hwm, size are unchanged by sort (only positions inside [0, hwm)
 * change). For packed-in-one-step semantics, call:
 *
 *   fs2arr_sortasc_nullafter(&a);   // asc order, holes at end
 *   fs2arr_sortdesc_nullbefore(&a); // desc order, holes at end
 *
 * @pre cmp != NULL. If cmp is fs2_cmp_wrap/fs2_cmp_desc_wrap, additionally
 *      hwm == count (no holes in [0, hwm)).
 */
extern void
fs2arr_sort(fs2array *a, pointer_comparator_t cmp);

static inline void 
fs2arr_sortasc(fs2array *a) { 
    fs2arr_sort(a, fs2_cmp_wrap);           
}
static inline void 
fs2arr_sortdesc(fs2array *a) { 
    fs2arr_sort(a, fs2_cmp_desc_wrap);      
}
/**
 * @note Only needed if you build the array MANUALLY, bypassing push/attach
 *       (which promote canonical FS2() → FS2EMPTY via fs2_move). Under the
 *       normal API, live slots always have v != NULL, so fs2_cmp is safe.
 *       These comparators exist for defense against corrupted/foreign arrays.
 */
static inline void 
fs2arr_sortasc_nullbefore(fs2array *a) { 
    fs2arr_sort(a, fs2_cmp_nullbefore_wrap);      
}
static inline void 
fs2arr_sortdesc_nullbefore(fs2array *a){ 
    fs2arr_sort(a, fs2_cmp_desc_nullbefore_wrap); 
}
static inline void 
fs2arr_sortasc_nullafter(fs2array *a) { 
    fs2arr_sort(a, fs2_cmp_nullafter_wrap);      
}
static inline void 
fs2arr_sortdesc_nullafter(fs2array *a){ 
    fs2arr_sort(a, fs2_cmp_desc_nullafter_wrap); 
}

/**
 * @brief Merge two sorted arrays into a new one (two-pointer merge).
 *
 * Stable: elements from @p a come before equal elements from @p b.
 * Elements are deep-copied; inputs untouched, result independent.
 *
 * @pre  Both @p a and @p b are packed (hwm == count) and sorted by @p cmp.
 * @pre  @p cmp is qsort-compatible.
 *
 * @param a    Non-NULL fs2array, packed, sorted.
 * @param b    Non-NULL fs2array, packed, sorted.
 * @param cmp  Non-NULL comparator.
 * @return     New fs2array, tight (hwm == size == count == a->count + b->count).
 *
 * @warning `invfatal` (SIGABRT) on NULL a/b/cmp.
 * @warning `userraise` (SIGINT) on not-packed input.
 * @warning `sysraise`  (SIGINT)  on OOM.
 */
extern fs2array
fs2arr_mergesorted_new(const fs2array *restrict a,
                       const fs2array *restrict b,
                       pointer_comparator_t cmp);

/**
 * @brief Check whether @p a is sorted according to @p cmp.
 *
 * @pre  @p a is packed (hwm == count).
 * @return true if sorted (or count < 2).
 * @warning `invfatal` on NULL a/cmp; `invraise` on not-packed.
 */
extern bool
fs2arr_issorted(const fs2array *restrict a, pointer_comparator_t cmp);

/**
 * @brief Append all live elements of @p src to @p dst (dst += src).
 *
 * Elements are deep-copied (src is const, untouched). Holes in @p src are
 * skipped — only live slots are appended. Holes in @p dst are preserved
 * (append goes to hwm, not into holes): if dst is [A, hole, B] and
 * src = [X], result is [A, hole, B, X].
 *
 * @pre  @p dst != @p src (restrict: self-concat would require clone first).
 *
 * @param dst  Non-NULL fs2array (grows; may have holes).
 * @param src  Non-NULL fs2array (any state; holes skipped).
 * @warning `invfatal` (SIGABRT) on NULL dst/src or dst == src.
 * @warning `sysraise` (OOM) on allocation failure.
 *
 * @note No-op if src is canonical / empty (count == 0).
 *       For packed result after concat, call fs2arr_compact(&dst) then
 *       fs2arr_synchwm(&dst).
 */
extern fs2array *
fs2arr_concat(fs2array *dst, const fs2array *src);

/**
 * @brief New array = @p a ++ @p b (deep copy, packed, tight).
 *
 * Shortcut for clone(a) + concat(clone, b) + shrink.
 * Result is packed (hwm == count) and tight (size == count), regardless
 * of holes in @p a or @p b.
 *
 * @param a  Non-NULL fs2array (any state).
 * @param b  Non-NULL fs2array (any state).
 * @return   New fs2array; free with fs2arr_free.
 * @warning `invfatal` on NULL a/b; `sysraise` on OOM.
 */
static inline fs2array
fs2arr_concat_new(const fs2array *restrict a, const fs2array *restrict b) {
    fs2array res = fs2arr_clone(a);
    return *fs2arr_concat(&res, b);
}

/**
 * @brief Build an fs2array by copying a NULL-terminated array of C-strings.
 *
 * Each `src[i]` (non-NULL by loop condition) is copied via fs2_fromstr.
 * Empty strings `""` become live empty slots (FS2EMPTY), not holes.
 * Result is packed and tight (hwm == size == count).
 *
 * @param src  NULL-terminated `const char *const[]`. `src != NULL` required
 *             (NULL → SIGABRT). `src[0] == NULL` → canonical empty result.
 * @return     New fs2array; free with fs2arr_free.
 * @warning    `invfatal` (SIGABRT) if @p src is NULL.
 * @warning    `sysraise`  (OOM) on allocation failure.
 */
extern fs2array
fs2arr_fromstrarr_new(const char *const src[]);

/**
 * @brief Variadic sugar: build fs2array from C-string literals.
 *
 *   fs2array colors = FS2ARRFROMSTR("red", "green", "blue");
 *   // ≡ fs2arr_fromstrarr((const char*const[]){"red","green","blue", NULL})
 *
 * Requires at least one argument (for zero-element, use `FS2ARR()`).
 */
#define FS2ARRFROMSTR(...) \
    fs2arr_fromstrarr_new((const char *const[]){ __VA_ARGS__, NULL })

/**
 * @brief Build an fs2array by deep-copying @p cnt elements from @p src.
 *
 * Each `src[i]` is cloned into a fresh slot. Source fs2 values are
 * untouched (const). Result is packed (hwm == count == cnt), capacity
 * rounded (no shrink — caller may fs2arr_shrink if tightness is needed).
 *
 * @param src  Non-NULL array of `const fs2 *`; entries must be non-NULL
 *             (NULL → SIGABRT with index).
 * @param cnt  Number of elements (0 → canonical FS2ARR()).
 * @return     New fs2array; free with fs2arr_free.
 * @warning    `invfatal` (SIGABRT) if src is NULL, or any src[i] is NULL.
 * @warning    `sysraise`  (OOM) on allocation failure.
 */
extern fs2array
fs2arr_fromarr_new(const fs2 *const *src, size_t cnt);

/**
 * @brief Build an fs2array by MOVING @p cnt elements from @p src.
 *
 * Each `src[i]` is moved via fs2_move; after the call, `*src[i]` is
 * FS2EMPTY (stack source) or shell (heap source — caller must fs2_free).
 * Result is packed (hwm == count == cnt).
 *
 * @param src  Non-NULL array of `fs2 *`; entries must be non-NULL.
 * @param cnt  Number of elements (0 → canonical FS2ARR()).
 * @return     New fs2array; free with fs2arr_free.
 * @warning    `invfatal` (SIGABRT) if src is NULL, or any src[i] is NULL.
 * @warning    For heap (BODYALLOC) sources, caller retains the shell and
 *             must fs2_free(src[i]) after the call (the fs2 struct itself
 *             was not freed; only its buffer moved into the array).
 */
extern fs2array
fs2arr_fromarr_move(fs2 **src, size_t cnt);

/**
 * @brief Remove adjacent duplicates in place; keeps first of each run.
 *
 * @pre  @p a is packed (hwm == count) and sorted by @p cmp (so equal
 *       elements are adjacent). Caller must compact + sync if holes may
 *       exist — "adjacent" is meaningless with holes in the middle.
 *
 * @param a    Non-NULL fs2array, packed, sorted.
 * @param cmp  Comparator used for sorting. Adjacent elements equal
 *             (cmp == 0) are considered duplicates.
 * @return     a (for chaining).
 * @warning    `invfatal` (SIGABRT) on NULL a/cmp or not-packed input.
 *
 * @note Only the FIRST of each equal-run is kept. Since equal elements
 *       under @p cmp may differ bytewise (e.g. case-insensitive cmp
 *       on "a" vs "A"), the retained one is the first in sorted order
 *       — deterministic but not "the smallest of the run".
 * @note Result is packed: hwm = count = number of distinct runs. Capacity
 *       unchanged; call fs2arr_shrink if tightness is needed.
 */
extern fs2array *
fs2arr_unique(fs2array *a, pointer_comparator_t cmp);

/**
 * @brief Deep-copy @p src, then apply fs2arr_unique on the copy.
 *
 * @param src  Non-NULL fs2array (any state — clone packs first).
 * @param cmp  Comparator.
 * @return     New fs2array; free with fs2arr_free.
 */
extern fs2array
fs2arr_unique_new(const fs2array *restrict src, pointer_comparator_t cmp);

/**
 * @brief Append all live elements of @p src to @p dst, separated by @p delim.
 *
 * Semantics match `fs2_concatall`:
 *   - dst is treated as **first element** of the resulting sequence. If dst
 *     is non-empty, @p delim is inserted before the first src element too.
 *   - Holes in @p src are skipped; live empty strings (FS2EMPTY) are kept
 *     (contributing `""` between separators).
 *   - NULL or empty @p delim → no separator.
 *
 *   fs2 dst = FS2();              fs2arr_join_delim(&dst, {a,b}, "|") → "a|b"
 *   fs2 dst = fs2_fromstr("x");   fs2arr_join_delim(&dst, {a,b}, "|") → "x|a|b"
 *
 * @param dst    Non-NULL fs2 (ALLOC or LOCAL); grows as needed.
 * @param src    Non-NULL fs2array. No-op if src is canonical / count==0.
 * @param delim  Optional; NULL or empty → no separator.
 * @return       @p dst (for chaining).
 *
 * @pre dst must not point into src->arr[i] (restrict: fs2_cat(dst, dst)
 *      is UB). Use fs2_clone to detach first.
 * @warning `invfatal` (SIGABRT) if dst or src is NULL.
 * @warning `sysraise`  (OOM) on allocation failure.
 */
extern fs2 *
fs2arr_join_delim(fs2 *restrict dst, const fs2array *restrict src, const fs2 *delim);

static inline fs2 *
fs2arr_join(fs2 *restrict dst, const fs2array *restrict src) {
    return fs2arr_join_delim(dst, src, NULL);
}

static inline fs2 *
fs2arr_join_delim_str(fs2 *restrict dst, const fs2array *restrict src, const char *restrict delim) {
    fs2 d = _fs2_wrap(delim);           /* NULL → empty → no separator */
    return fs2arr_join_delim(dst, src, &d);
}

/**
 * @brief Reverse elements in place.
 *
 * @pre  @p a is packed (hwm == count). Caller must fs2arr_compact if the
 *       array may contain holes — with holes, "reverse" is ambiguous.
 * @param a  Non-NULL fs2array, packed.
 * @return   @p a (for chaining).
 * @warning `invfatal` (SIGABRT) on NULL a or not-packed input.
 */
extern fs2array *
fs2arr_reverse(fs2array *a);

/**
 * @brief Deep-copy @p src, then reverse the copy (packed result).
 *
 * @param src  Non-NULL fs2array (any state — clone packs first).
 * @return     New fs2array; free with fs2arr_free.
 */
static inline fs2array
fs2arr_reverse_new(const fs2array *restrict src) {
    invfatal(ERR_NULL_INPUT, src != NULL, "Null fs2array");
    fs2array r = fs2arr_clone(src);            /* packed deep copy */
    return *fs2arr_reverse(&r);
}

/**
 * @brief Binary search in a sorted packed fs2array.
 *
 * @pre  @p a is packed (hwm == count) and sorted by @p cmp (same comparator
 *       as used for sort). Caller must `fs2arr_compact(&a)` + `fs2arr_synchwm(&a)`
 *       (or use `fs2arr_clone`, which packs) if the array may contain holes.
 * @pre  @p cmp is `fs2_cmp_wrap` / `fs2_cmp_desc_wrap` / `fs2_cmp_ci_wrap`
 *       / `fs2_cmp_desc_ci_wrap` — or any `qsort`/`bsearch`-compatible
 *       function matching the sort order.
 *
 * @param a    Non-NULL fs2array, packed, sorted.
 * @param key  Non-NULL fs2 to search for. Canonical `FS2()` (v==NULL) is
 *             normalized to `FS2EMPTY` before comparison (so `strcmp`-based
 *             comparators don't SEGV).
 * @param cmp  Typed comparator used for sort.
 * @return     0-based index of some matching element, or -1 if not found.
 *             With duplicates, the returned index is implementation-defined
 *             (`qsort`/`bsearch` don't guarantee "first of equal").
 *
 * @warning `invfatal` (SIGABRT) on NULL a/key/cmp, or not-packed input.
 *
 * @note For consistency with a sort that used `fs2_cmp_wrap`, pass the same
 *       `fs2_cmp_wrap` here. Mixing asc-sorted array with desc comparator →
 *       garbage result (undefined; bsearch assumes matching order).
 */
extern ptrdiff_t
fs2arr_bsearch(const fs2array *restrict a,
               const fs2 *restrict key,
               pointer_comparator_t cmp);

/**
 * @brief Linear search for @p key using @p cmp.
 *
 * Works on any array (packed or not) — holes in [0, hwm) are skipped.
 * First match in slot order wins (not "first in any sorted order" —
 * find doesn't require sorted).
 *
 * @param a    Non-NULL fs2array.
 * @param key  Non-NULL fs2 to search for. Canonical FS2() (v==NULL) is
 *             promoted to FS2EMPTY before comparison — same as fs2arr_bsearch.
 * @param cmp  Typed comparator (`fs2_cmp_wrap` / `fs2_cmp_ci_wrap` /
 *             `fs2_cmp_desc_wrap` / custom). Any comparator works — `find`
 *             only checks `cmp(...) == 0`.
 * @return     0-based slot index of first match, or -1 if not found.
 * @warning `invfatal` (SIGABRT) on NULL a/key/cmp.
 */
extern ptrdiff_t
fs2arr_find(const fs2array *restrict a,
            const fs2 *restrict key,
            pointer_comparator_t cmp);

/**
 * @brief Convenience: `fs2arr_find(...) >= 0`.
 *
 * @return true iff @p key is present in @p a.
 */
static inline bool
fs2arr_contains(const fs2array *restrict a,
                const fs2 *restrict key,
                pointer_comparator_t cmp) {
    return fs2arr_find(a, key, cmp) >= 0;
}

/**
 * @brief Count occurrences of @p key in @p a using @p cmp.
 *
 * Linear scan, holes skipped. O(hwm).
 *
 * @param a    Non-NULL fs2array.
 * @param key  Non-NULL fs2 to search for (canonical promoted, see fs2arr_find).
 * @param cmp  Typed comparator.
 * @return     Number of slots where cmp(&slot, key) == 0 (0 if none).
 * @warning `invfatal` (SIGABRT) on NULL a/key/cmp.
 */
extern size_t
fs2arr_count(const fs2array *restrict a,
             const fs2 *restrict key,
             pointer_comparator_t cmp);

/**
 * @brief `fs2arr_find` with C-string key (NULL → searches for "").
 *
 * Zero-allocation: uses a temporary STATIC view, discarded after the call.
 */
static inline ptrdiff_t
fs2arr_find_str(const fs2array *restrict a, const char *restrict key,
                pointer_comparator_t cmp) {
    fs2 k = _fs2_wrap(key);
    return fs2arr_find(a, &k, cmp);
}

/**
 * @brief `fs2arr_contains` with C-string key.
 */
static inline bool
fs2arr_contains_str(const fs2array *restrict a, const char *restrict key,
                    pointer_comparator_t cmp) {
    fs2 k = _fs2_wrap(key);
    return fs2arr_contains(a, &k, cmp);
}

/**
 * @brief `fs2arr_count` with C-string key.
 */
static inline size_t
fs2arr_count_str(const fs2array *restrict a, const char *restrict key,
                 pointer_comparator_t cmp) {
    fs2 k = _fs2_wrap(key);
    return fs2arr_count(a, &k, cmp);
}

#endif /* !FS2ARRAY_API_H */