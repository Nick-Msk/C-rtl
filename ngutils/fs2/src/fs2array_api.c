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

    a->hwm = write;   /* trim logical end to last live; [hwm, old_hwm) → padding */
}

bool
fs2arr_issorted(const fs2array *restrict a, pointer_comparator_t cmp) {
    invfatal(ERR_NULL_INPUT, a != NULL && cmp != NULL,
             "Null pointers %p %p", a, cmp);

    if (a->count < 2)
        return true;
    size_t prev = SIZE_MAX;
    for (size_t i = 0; i < a->hwm; i++) {
        if (fs2arr_is_hole(&a->arr[i]))
            continue;
        if (prev != SIZE_MAX &&  cmp(&a->arr[prev], &a->arr[i]) > 0)
            return false;
        prev = i;
    }
    return true;
}

fs2array
fs2arr_mergesorted_new(const fs2array *restrict a,
                       const fs2array *restrict b,
                       pointer_comparator_t cmp) {
    invfatal(ERR_NULL_INPUT, a != NULL && b != NULL && cmp != NULL,
             "Null pointers %p %p %p", a, b, cmp);

    if (a->hwm != a->count || b->hwm != b->count || a->count > SIZE_MAX - b->count)
        userraise(0, ERR_OUT_OF_RANGE, 
            "Not packed or total overflow over %zu: a.hwm %zu a.count %zu, b.hwm %zu b.count %zu",
            SIZE_MAX, a->hwm, a->count, b->hwm, b->count);

    fs2array r = fs2arr_init(a->count + b->count);

    size_t ia = 0, ib = 0;
    while (ia < a->count && ib < b->count) {
        if (cmp(&a->arr[ia], &b->arr[ib]) <= 0) {
            fs2 v = fs2_clone(&a->arr[ia++]);
            fs2arr_push(&r, &v);
        } else {
            fs2 v = fs2_clone(&b->arr[ib++]);
            fs2arr_push(&r, &v);
        }
    }
    while (ia < a->count) { fs2 v = fs2_clone(&a->arr[ia++]); fs2arr_push(&r, &v); }
    while (ib < b->count) { fs2 v = fs2_clone(&b->arr[ib++]); fs2arr_push(&r, &v); }

    return r;
}

fs2array *
fs2arr_concat(fs2array *dst, const fs2array *src) {
    invfatal(ERR_NULL_INPUT, dst != NULL && src != NULL,
             "Null pointers %p %p", dst, src);

    if (!src->arr || src->count == 0)
        return logsimpleret(dst, "Nothing to concat %p %zu", src->arr, src->count);

    /* Snapshot: if dst == src, dst->hwm grows as we push; we only want
     * to process the ORIGINAL [0, hwm) slots. */
    size_t src_hwm   = src->hwm;
    size_t src_count = src->count;

    if (src_count > SIZE_MAX - dst->hwm)
        userraise(0, ERR_OUT_OF_RANGE,
                  "fs2arr_concat: hwm overflow %zu + %zu",
                  dst->hwm, src_count);

    /* Reserve for the whole batch upfront. Guarantees no realloc during
     * the loop → &src->arr[i] stays valid even when dst == src. */
    fs2arr_reserve(dst, dst->hwm + src_count);

    for (size_t i = 0; i < src_hwm; i++) {
        if (fs2arr_is_hole(&src->arr[i]))
            continue;
        fs2 v = fs2_clone(&src->arr[i]);
        fs2arr_push(dst, &v);                  /* appends at current hwm */
    }
    return dst;
}

fs2array
fs2arr_fromstrarr_new(const char *const *src) {
    invfatal(ERR_NULL_INPUT, src != NULL, 
        "Null src array");

    fs2array a = FS2ARR();
    
    while (*src) {
        fs2 v = fs2_fromstr(*src++);
        fs2arr_push(&a, &v);                   /* v → FS2EMPTY, copy in slot */
    }
    return a;
}
