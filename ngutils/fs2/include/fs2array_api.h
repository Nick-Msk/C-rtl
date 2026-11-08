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

#endif /* !FS2ARRAY_API_H */