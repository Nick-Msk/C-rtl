
#include "fs2.h"
#include "fs2array.h"
#include "error.h"

// utilities

// formatting and setup length, assumption - [0 - from) already formatted
static void 
fs2arr_initempty(fs2array *a, size_t from, size_t to) {
    if (to > a->size)
        userraise(false, ERR_OUT_OF_RANGE, "Position %zu exceed %zu",
            to, a->size);
    for (size_t i = from; i < to; i++)
        fs2arr_set_hole(&a->arr[i]);   // we CAN'T free here!! User MUST exec preperly!
} 

/**
 * @brief Ensure capacity for at least @p need elements
 *
 * Grows capacity by power-of-2 (floor 16), like fs2_reserve. No-op if
 * current capacity is already >= need. New slots initialized to FS2EMPTY.
 *
 * @param a     Non-NULL fs2array.
 * @param need  Target capacity in elements.
 * @return      a->arr (for chaining).
 * @warning    `invfatal` (SIGABRT) if a is NULL.
 * @warning    `sysraise` (OOM / overflow) if realloc fails.
 */
fs2 *
fs2arr_reserve(fs2array *a, size_t need) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null fs2array");

    if (need <= a->size)
        return logsimpleret(a->arr, 
            "No need to realloc %zu > %zu", a->size, need);

    size_t newsz = need < FS2_MIN_ALLOC? FS2_MIN_ALLOC: need;

    // just for check
    if (newsz > SIZE_MAX / sizeof(fs2))
        sysraise(NULL, "fs2arr_reserve: %zu bytes overflow", newsz);

    newsz = calcnewsize(SIZE_POWER2, newsz);
    size_t oldsz = a->size;
    fs2 *p = realloc(a->arr, newsz * sizeof(fs2));
    if (!p)
        sysraise(NULL, "fs2arr_reserve: cannot grow to %zu elements", 
            newsz);

    a->arr  = p;
    a->size = newsz; // newsz >= need!!!!!
    fs2arr_initempty(a, oldsz, newsz); // ALL new physical slots, not [len, need)
    
    return a->arr;
}

fs2array
fs2arr_init(size_t cnt) {
    fs2array a = FS2ARR();
    if (cnt == 0)
        return a;

    fs2arr_reserve(&a, cnt);    // allocation
     
    logsimple("Allocated %zu/%zu", a.hwm, a.size);
    return a;
}

fs2array
fs2arr_clone(const fs2array *src) {
    invfatal(ERR_NULL_INPUT, src != NULL, "Null fs2array");

    fs2array a = fs2arr_init(src->count);  // physical count MAY BE differ!!! Logical - the same.          

    for (size_t i = 0; i < src->hwm; i++) {
        if (fs2arr_is_hole(&src->arr[i]))
            continue;
        fs2 v = fs2_clone(&src->arr[i]);    /* deep copy of one element */
        fs2arr_push(&a, &v);                /* v → FS2EMPTY, copy in slot */
    }
    return a;
}

// exact clone
fs2array
fs2arr_cloneexact(const fs2array *src) {
    invfatal(ERR_NULL_INPUT, src != NULL, "Null fs2array");

    fs2array a = fs2arr_init(src->hwm);  // total copy    
    a.hwm = src->hwm;       // to save trailer nulls    

    for (size_t i = 0; i < src->hwm; i++) {
        if (fs2arr_is_hole(&src->arr[i]))
            continue;
        fs2 v = fs2_clone(&src->arr[i]);    /* deep copy of one element */
        fs2arr_attach_sp(&a, &v, i);                /* v → FS2EMPTY, copy in slot */
    }
    return a;
}

size_t 
fs2arr_synchwm(fs2array *a) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null fs2array");

    while (a->hwm > 0 && fs2arr_is_hole(&a->arr[a->hwm - 1]))
        a->hwm--;
    return logsimpleret(a->hwm, "Hwn synced to %zu", a->hwm);
}

size_t
fs2arr_clean(fs2array *a, size_t from, size_t to) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null fs2array");

    if (!(from <= to && to <= a->size))
        userraise (0, ERR_OUT_OF_RANGE, 
             "fs2arr_clean: bad range [%zu, %zu), size %zu", from, to, a->size);

    size_t freed = 0;
    for (size_t i = from; i < to; i++) {
        if (fs2arr_is_hole(&a->arr[i]))
            continue;
        fs2_free(&a->arr[i]);
        fs2arr_set_hole(&a->arr[i]);
        freed++;
    }
     /* Trim trailing holes only if the cleaned range reached the logical tail.
     * `hwm` is unchanged during the loop, so `to >= hwm` is checked after.
     * Middle clean (to < hwm): slots [to, hwm) may be live — don't trim. */
    if (to >= a->hwm)
        fs2arr_synchwm(a);

    a->count -= freed;
    logsimple("fs2arr_clean [%zu, %zu): %zu freed, hwm=%zu count=%zu", 
        from, to, freed, a->hwm, a->count);
    return freed;
}

/* Full clean — frees all live elements, resets hwm/count, keeps capacity. */
void
fs2arr_cleanall(fs2array *a) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null fs2array");

    if (!a->arr)
        return;
    fs2arr_clean(a, 0, a->hwm);
    a->hwm   = 0;
    a->count = 0;
}

bool
fs2arr_shrink(fs2array *a) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null fs2array");
    if (!a->arr)
        return logsimpleret(true, "Null arr, nothing to do");

    if (a->hwm == a->size)
        return logsimpleret(true, 
            "hwn equal size %zu, nothing to free", a->size);                        /* already tight */

    /* hwm == 0 (no live region, or synchwm already trimmed).
     * realloc(arr, 0) is impl-defined — free explicitly to get canonical. */
    if (a->hwm == 0) {
        fs2arr_free(a);
        return logsimpleret(true, "hwm is 0, freed totally");
    }

    /* Internal holes preserved — only tail is trimmed. Shrink to exact hwm. */
    fs2 *p = realloc(a->arr, a->hwm * sizeof(fs2));
    if (!p)
        return userraise(false, ERR_UNABLE_ALLOCATE, 
            "%zu items", a->hwm);                       /* original block valid, hwm trimmed */

    a->arr  = p;
    a->size = a->hwm;
    return logsimpleret(true, "Shrinked to %zu", a->size);
}

// NULL safe method!
void
fs2arr_free(fs2array *a) {
    if (!a)
        return;
    fs2arr_cleanall(a);
    free(a->arr);
    *a = FS2ARR();        // note - can't free(arr), can be in stack
}

// search till HWM!!!
ptrdiff_t
fs2arr_find_tillhwm(const fs2array *a) {
    invfatal(ERR_NULL_INPUT, a != NULL, 
        "fs2array is null %p", a);

    for (size_t i = 0; i < a->hwm; i++)
        if (fs2arr_is_hole(&a->arr[i]))
            return i;
    return -1;
}

// internal plugger, no checking at all 
// a != NULL, src != NULL, idx < a->size, slot[idx] — hole.
static ptrdiff_t
fs2arr_plug(fs2array *a, fs2 *src, size_t idx) {
    if (idx >= a->hwm)
        a->hwm = idx + 1;

    a->count++;
    fs2_move(&a->arr[idx], src);             /* value in slot, p = FS2EMPTY|(body) */
    return idx;
}

ptrdiff_t
fs2arr_attach_sp(fs2array *a, fs2 *src, size_t idx) {
    invfatal(ERR_NULL_INPUT, a != NULL && src != NULL, 
        "Null pointers %p %p", a, src);

    if (idx >= a->size)
        userraise(-1, ERR_OUT_OF_RANGE, 
            "Index %zu >= size %zu", idx, a->size);
    if (!fs2arr_is_hole(&a->arr[idx]))
        userraise(-1, ERR_OUT_OF_RANGE, 
            " slot[%zu] not a null marker", idx);

    return fs2arr_plug(a, src, idx);
}

ptrdiff_t
fs2arr_attach_dp(fs2array *a, fs2 **src, size_t idx) {
    invfatal(ERR_NULL_INPUT, a != NULL && src != NULL, "Null pointers");

    if (*src == NULL)
        return -1;

    /* Pre-validate so null-first is safe: no raise AFTER nulling. */
    if (idx >= a->size)
        userraise(-1, ERR_OUT_OF_RANGE, "idx %zu >= size %zu", idx, a->size);
    if (!fs2arr_is_hole(&a->arr[idx]))
        userraise(-1, ERR_OUT_OF_RANGE, "slot[%zu] not a hole", idx);

    fs2 *p = *src;
    *src = NULL;                     /* All validation passed — no raise below, safe to null and consume. */

    ptrdiff_t slot = fs2arr_plug(a, p, idx);
    fs2_free(p);                     /* ← no-op for stack; frees struct for heap */
    return slot;
}

bool
fs2arr_detachto(fs2array *a, size_t idx, fs2 *dst) {
    invfatal(ERR_NULL_INPUT, a != NULL && dst != NULL, 
             "Null pointers %p %p", a, dst);

    if (idx >= a->hwm)
        return logsimpleerr(false,
            "fs2arr_detachto: idx %zu >= hwm %zu", idx, a->hwm);

    if (fs2arr_is_hole(&a->arr[idx]))
        return logsimpleerr(false, 
            "fs2arr_detachto: slot[%zu] is already empty", idx);

    fs2_move(dst, &a->arr[idx]);        // slot → FS2EMPTY, dst gets value 
    fs2arr_set_hole(&a->arr[idx]);
    a->count--;                          // hwm unchanged, hole stays
    return true;
}

bool
fs2arr_equalexact(const fs2array *a, const fs2array *b) {
    if (a == b)
        return true;
    if (!a || !b)
        return false;

    if (a->hwm   != b->hwm)   return false;
    if (a->count != b->count) return false;    /* derived; defense-in-depth */

    /* hwm == 0 → no data to compare, both "empty up to hwm" */
    for (size_t i = 0; i < a->hwm; i++) {
        bool ha = fs2arr_is_hole(&a->arr[i]);
        bool hb = fs2arr_is_hole(&b->arr[i]);

        if (ha != hb) return false;            /* hole vs live */
        if (ha)       continue;                /* both holes */
        if (fs2_cmp(&a->arr[i], &b->arr[i]) != 0)
            return false;                       /* live, different content */
    }
    return true;
}

long
fs2arr_techfprint(FILE *restrict out, const fs2array *restrict a, const char *restrict name) {
    if (!out)
        return 0;

    const char *n = name ? name : "(unnamed)";
    int         w;
    long        total = 0;

    if (!a) {
        w = fprintf(out, "FS2ARR: %s: <NULL>\n", n);
        return w < 0 ? -1 : (long)w;
    }

    w = fprintf(out, "FS2ARR: %s: hwm [%zu], size [%zu], count [%zu], arr(%p)[\n",
                n, a->hwm, a->size, a->count, (void *)a->arr);
    if (w < 0) return -1;
    total += w;

    if (a->arr) {
        size_t shown = a->hwm < FS2ARR_TECH_PRINT_COUNT
                       ? a->hwm : FS2ARR_TECH_PRINT_COUNT;

        for (size_t i = 0; i < shown; i++) {
            const fs2 *slot = &a->arr[i];
            if (fs2arr_is_hole(slot)) {
                w = fprintf(out, "  [%zu] <hole>\n", i);
            } else {
                const char *v   = slot->v ? slot->v : "(nil)";
                size_t      len = slot->v ? MIN(FS2_TECH_PRINT_COUNT, slot->len) : 0;
                int         ell = (slot->v && FS2_TECH_PRINT_COUNT < slot->len) ? 1 : 0;
                w = fprintf(out,
                            "  [%zu] len [%zu], sz [%zu], flags [0x%x], v(%p)[%.*s%s]\n",
                            i, slot->len, slot->sz, (unsigned)slot->flags,
                            (void *)v, (int)len, v, ell ? "..." : "");
            }
            if (w < 0) return -1;
            total += w;
        }

        if (shown < a->hwm) {
            w = fprintf(out, "  ... (%zu more slots)\n", a->hwm - shown);
            if (w < 0) return -1;
            total += w;
        }
        if (a->hwm < a->size) {
            w = fprintf(out, "  padding [%zu, %zu) = %zu holes\n",
                        a->hwm, a->size, a->size - a->hwm);
            if (w < 0) return -1;
            total += w;
        }
    }

    w = fprintf(out, "]\n");
    if (w < 0) return -1;
    total += w;

    return total;
}

bool
fs2arr_validate(FILE *restrict out, const fs2array *restrict a)
{
    if (!a)
        return LOGNPRINTF(out, "null pointer");

    /* 1. Ordering. */
    if (a->count > a->hwm)
        return LOGNPRINTF(out, "count [%zu] > hwm [%zu]", a->count, a->hwm);
    if (a->hwm > a->size)
        return LOGNPRINTF(out, "hwm [%zu] > size [%zu]", a->hwm, a->size);

    /* 2. Canonical empty (arr == NULL). */
    if (!a->arr) {
        if (a->hwm != 0 || a->size != 0 || a->count != 0)
            return LOGNPRINTF(out,
                "null arr but hwm=%zu, size=%zu, count=%zu",
                a->hwm, a->size, a->count);
        return true;
    }

    /* 3. [0, hwm): hole or valid fs2; live count == count. */
    size_t live = 0;
    for (size_t i = 0; i < a->hwm; i++) {
        if (fs2arr_is_hole(&a->arr[i]))
            continue;
        if (!fs2_validate(out, &a->arr[i]))
            return LOGNPRINTF(out, "slot[%zu] invalid fs2", i);
        live++;
    }
    if (live != a->count)
        return LOGNPRINTF(out,
            "live [%zu] in [0, hwm=%zu) != count [%zu]",
            live, a->hwm, a->count);

    /* 4. [hwm, size): all holes (padding). */
    for (size_t i = a->hwm; i < a->size; i++)
        if (!fs2arr_is_hole(&a->arr[i]))
            return LOGNPRINTF(out,
                "slot[%zu] in padding [hwm=%zu, size=%zu) is not a hole",
                i, a->hwm, a->size);

    return true;
}
