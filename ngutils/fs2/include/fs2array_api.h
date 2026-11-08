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
 * @brief Sort live elements ascending/in place; holes compacted to end.
 *
 * Pipeline: compact (holes → [count, hwm)) → qsort [0, count) with @p cmp.
 * After: [0, count) sorted, [count, hwm) all holes. hwm/size unchanged.
 *
 * @param a    Non-NULL fs2array.
 * @param cmp  qsort-compatible comparator (`fs2_cmp_wrap`, `fs2_cmp_desc_wrap`,
 *             `fs2_cmp_null_wrap`, `fs2_cmp_desc_null_wrap`, or user-defined).
 *             Null-safe variant required if the array contains a live slot
 *             with `v == NULL` (only possible via manual corruption).
 * @warning `invfatal` (SIGABRT) if a is NULL.
 * @warning `sysraise` (OOM) if internal compact needs realloc — should not
 *          happen on packed arrays.
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

#endif /* !FS2ARRAY_API_H */