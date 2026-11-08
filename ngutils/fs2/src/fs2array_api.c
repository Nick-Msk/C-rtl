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

/**
 * @brief Shared implementation: build fs2array from cnt elements.
 *
 * @param src      Non-NULL array of mutable fs2 pointers. When @p do_move
 *                 is false, `src` is only read (never written); when true,
 *                 each `*src[i]` is moved out (reset to FS2EMPTY).
 * @param cnt      Number of elements.
 * @param do_move  true → move each (`fs2_move`); false → deep copy (`fs2_clone`).
 *
 * Internal: cast-safety relies on caller (do_move=false never mutates).
 */
static fs2array
fs2arr_fromarr_common(fs2 **src, size_t cnt, bool do_move) {
    invfatal(ERR_NULL_INPUT, src != NULL, "Null src array");

    fs2array a = FS2ARR();
    if (cnt == 0)
        return a;

    fs2arr_reserve(&a, cnt);
    for (size_t i = 0; i < cnt; i++) {
        if (src[i] == NULL)
            userraise(false, ERR_NULL_INPUT, "src[%zu] is NULL", i);
        if (do_move) {
            fs2arr_push(&a, src[i]);       /* push_sp: *src[i] → FS2EMPTY */
        } else {
            fs2 v = fs2_clone(src[i]);     /* deep copy, src[i] untouched */
            fs2arr_push(&a, &v);
        }
    }
    return a;
}

fs2array
fs2arr_fromarr_new(const fs2 *const *src, size_t cnt) {
    /* Const dropped only to match the shared helper signature.
     * do_move=false → we never write through src. */
    return fs2arr_fromarr_common((fs2 **)src, cnt, false);
}

fs2array
fs2arr_fromarr_move(fs2 **src, size_t cnt) {
    return fs2arr_fromarr_common(src, cnt, true);
}

fs2array *
fs2arr_unique(fs2array *a, pointer_comparator_t cmp) {
    invfatal(ERR_NULL_INPUT, a != NULL && cmp != NULL,
             "Null pointers %p %p", a, cmp);

    if (a->hwm != a->count)
        userraise(NULL, ERR_UNSUPPORTED_TYPE,
             "not packed (hwm=%zu count=%zu); "
             "call fs2arr_compact() first", a->hwm, a->count);

    if (a->count < 2)
        return a;                             /* 0 or 1 — already unique */

    /* write is the next free output slot; write-1 is "last kept".
     * read walks the packed input; write <= read at every step. */
    size_t write = 1;                         /* arr[0] always kept */
    for (size_t read = 1; read < a->count; read++) {
        if (cmp(&a->arr[write - 1], &a->arr[read]) == 0) {
            /* duplicate of last kept — free and skip */
            fs2_free(&a->arr[read]);
            fs2arr_set_hole(&a->arr[read]);
        } else {
            /* distinct — keep it: transfer ownership to arr[write] */
            if (write != read) {
                a->arr[write] = a->arr[read];
                fs2arr_set_hole(&a->arr[read]);
            }
            write++;
        }
    }
    a->count = write;
    a->hwm   = write;                          /* [write, old_hwm) now holes */
    return a;
}

fs2array
fs2arr_unique_new(const fs2array *restrict src, pointer_comparator_t cmp) {
    fs2array r = fs2arr_clone(src);            /* packed deep copy */
    fs2arr_unique(&r, cmp);
    return r;
}

fs2 *
fs2arr_join_delim(fs2 *restrict dst, const fs2array *restrict src, const fs2 *delim) {
    invfatal(ERR_NULL_INPUT, dst != NULL && src != NULL,
             "Null pointers %p %p", dst, src);

    if (!src->arr || src->count == 0)
        return dst;                     /* nothing to append */

    bool delim_ok  = !fs2_is_empty(delim);
    bool wrote_any = (dst->len > 0);    /* dst acts as first element */

    for (size_t i = 0; i < src->hwm; i++) {
        if (fs2arr_is_hole(&src->arr[i]))
            continue;
        if (delim_ok && wrote_any)
            fs2_cat(dst, delim);
        fs2_cat(dst, &src->arr[i]);
        wrote_any = true;
    }
    return dst;
}

