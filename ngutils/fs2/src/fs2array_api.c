#include "fs2array_api.h"
#include "fs2_api.h"

fs2array
fs2_split_toarray(const fs2 *restrict s, const fs2 *restrict delim) {
    invfatal(ERR_NULL_INPUT, s != NULL && delim != NULL,
             "Null pointers %p %p", s, delim);

    fs2array a = FS2ARR();

    /* Empty delimiter: no split — whole string is one part (even empty). */
    if (fs2_is_empty(delim)) {
        fs2 part = fs2_clone(s);              /* empty → FS2EMPTY, else copy */
        fs2arr_push(&a, &part);
        return a;
    }

    /* Non-empty delimiter: split at each occurrence, keep empty parts. */
    size_t    pos = 0;
    ptrdiff_t hit;
    while ((hit = fs2_instr_from(s, delim, pos)) >= 0) {
        fs2 part = fs2_substr_new(s, pos, (size_t) hit - pos);
        fs2arr_push(&a, &part);               /* part → FS2EMPTY */
        pos = (size_t) hit + delim->len;       /* skip delim: non-overlapping */
    }

    /* Final tail: [pos, s->len). Always pushed — even when empty.
     * For "a,b" → parts ["a","b"]. For "a," → ["a",""].
     * For empty s → [""]. This matches Python split() and fs2_split_part. */
    fs2 tail = fs2_substr_new(s, pos, s->len - pos);
    fs2arr_push(&a, &tail);
    
    return a;
}

void
fs2arr_sort(fs2array *a, pointer_comparator_t cmp) {
    invfatal(ERR_NULL_INPUT, a != NULL && cmp != NULL, 
        "Null fs2array or comparator (%p %p)", a, cmp);

    if (!a->arr || a->count < 2)
        return;                                /* 0 or 1 live — already sorted */
    
    /* Sort full logical range [0, hwm). If the array has holes, @p cmp must
     * handle v==NULL (use fs2_cmp_nullbefore_wrap / fs2_cmp_nullafter_wrap).
     * Plain fs2_cmp_wrap/fs2_cmp_desc_wrap assumes no holes in [0, hwm). */
    qsort(a->arr, a->hwm, sizeof(fs2), cmp);
}

void
fs2arr_compact(fs2array *a) {
    invfatal(ERR_NULL_INPUT, a != NULL, 
        "Null fs2array (%p)", a);

    if (!a->arr)
        return;

    size_t write = 0;
    for (size_t read = 0; read < a->hwm; read++) {
        if (fs2arr_is_hole(&a->arr[read]))
            continue;

        if (write != read) {
            a->arr[write] = a->arr[read];
            fs2arr_set_hole(&a->arr[read]);
        }
        write++;
    }

    /* write == live count by invariant. If count drifted (bug elsewhere),
     * fail-fast here rather than silently propagate. */
    invfatal(ERR_OUT_OF_RANGE, write == a->count,
             "fs2arr_compact: live slots %zu != count %zu",
             write, a->count);

    a->hwm = write;   /* trim logical end to last live; [hwm, old_hwm) → padding */
}
