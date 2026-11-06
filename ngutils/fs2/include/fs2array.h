#ifndef FS2ARRAY_H
#define FS2ARRAY_H

#include "fs2.h"

typedef struct fs2array {
    fs2    *arr;   // heap array of fs2 values (never BODYALLOC inside)
    size_t  len;   // number of valid elements
    size_t  sz;    // capacity in elements
} fs2array;

/** Canonical empty fs2array (no allocation). */
#define FS2ARR()  (fs2array){ .arr = NULL, .len = 0, .sz = 0 }

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

extern fs2 *
fs2arr_reserve(fs2array *a, size_t need);

extern void
fs2arr_free(fs2array *arr);

extern void 
fs2arr_clean(fs2array *a);


#endif /* !FS2ARRAY_H */

