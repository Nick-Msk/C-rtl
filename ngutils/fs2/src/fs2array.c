
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
static fs2 *
fs2arr_plug(fs2array *a, fs2 *src, size_t idx) {
    if (idx >= a->hwm)
        a->hwm = idx + 1;

    a->count++;
    fs2_move(&a->arr[idx], src);             /* value in slot, p = FS2EMPTY|(body) */
    return a->arr + idx;
}

fs2 *
fs2arr_attach_sp(fs2array *a, fs2 *src, size_t idx) {
    invfatal(ERR_NULL_INPUT, a != NULL && src != NULL, 
        "Null pointers %p %p", a, src);

    if (idx >= a->size)
        userraise(NULL, ERR_OUT_OF_RANGE, 
            "Index %zu >= size %zu", idx, a->size);
    if (!fs2arr_is_hole(&a->arr[idx]))
        userraise(NULL, ERR_OUT_OF_RANGE, 
            " slot[%zu] not a null marker", idx);

    return fs2arr_plug(a, src, idx);
}

fs2 *
fs2arr_attach_dp(fs2array *a, fs2 **src, size_t idx) {
    invfatal(ERR_NULL_INPUT, a != NULL && src != NULL, "Null pointers");

    if (*src == NULL)
        return NULL;

    /* Pre-validate so null-first is safe: no raise AFTER nulling. */
    if (idx >= a->size)
        userraise(NULL, ERR_OUT_OF_RANGE, "idx %zu >= size %zu", idx, a->size);
    if (!fs2arr_is_hole(&a->arr[idx]))
        userraise(NULL, ERR_OUT_OF_RANGE, "slot[%zu] not a hole", idx);

    fs2 *p = *src;
    *src = NULL;                     /* All validation passed — no raise below, safe to null and consume. */

    fs2 *slot = fs2arr_plug(a, p, idx);
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
