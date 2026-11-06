
#include "fs2.h"
#include "fs2array.h"
#include "error.h"

// utilities

// formatting and setup length, assumption - [0 - from) already formatted
static inline void fs2arr_initempty(fs2array *a, size_t from, size_t to) {
    if (to > a->sz)
        userraise(false, ERR_OUT_OF_RANGE, "Position %zu exceed %zu",
            to, a->sz);
    for (size_t i = from; i < to; i++)
        a->arr[i] = FS2EMPTY;    // we CAN'T free here!! User MUST exec preperly!
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

    if (need <= a->sz)
        return logsimpleret(a->arr, 
            "No need to realloc %zu > %zu", a->sz, need);

    size_t newsz = need < FS2_MIN_ALLOC? FS2_MIN_ALLOC: need;

    // just for check
    if (newsz > SIZE_MAX / sizeof(fs2))
        sysraise(NULL, "fs2arr_reserve: %zu bytes overflow", newsz);

    newsz = calcnewsize(SIZE_POWER2, newsz);
    size_t oldsz = a->sz;
    fs2 *p = realloc(a->arr, newsz * sizeof(fs2));
    if (!p)
        sysraise(NULL, "fs2arr_reserve: cannot grow to %zu elements", 
            newsz);

    a->arr = p;
    a->sz  = newsz; // newsz >= need!!!!!
    fs2arr_initempty(a, oldsz, newsz); // ALL new physical slots, not [len, need)
    
    return a->arr;
}

// fs2 * fs2arr_shrink(fs2array *a);    SKIP FOR NOW

fs2array
fs2arr_init(size_t cnt) {
    fs2array a = FS2ARR();
    if (cnt == 0)
        return a;

    fs2arr_reserve(&a, cnt);    // allocation
     
    logsimple("Allocated %zu/%zu", a.len, a.sz);
    return a;
}

void 
fs2arr_clean(fs2array *a) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null fs2array");

    for (size_t i = 0; i < a->len; i++) {
        fs2_free(a->arr + i);
        a->arr[i] = FS2EMPTY;
    }
    logsimple("cleaned %zu elements", a->len);
    a->len = 0UL;
}

// NULL safe method!
void
fs2arr_free(fs2array *a) {
    if (!a)
        return;
    fs2arr_clean(a);
    free(a->arr);
    *a = FS2ARR();        // note - can't free(arr), can be in stack
}
