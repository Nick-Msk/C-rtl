/*
 * THREAD SAFETY: fs2slice operations on the SAME parent fs2 are not
 * thread-safe. `parent->borrow` is a plain uint32_t; concurrent
 * fs2slice_new / fs2slice_free on one parent require external locking.
 * Different parents, or read-only use of independent slices — safe.
 */

#ifndef FS2SLICE_H
#define FS2SLICE_H

#include "fs2.h"

typedef struct fs2sliceview {
    size_t from;
    size_t to;
} fs2sliceview;

#define FS2SLICEVIEW() (fs2sliceview) {.from = 0, .to = 0}

typedef struct fs2arrslice {
    fs2          *parent;    /* array owns the single borrow */
    fs2sliceview *ranges;
    size_t        hwm, size, count;
} fs2arrslice;

#define FS2ARRSLICE() (fs2arrslice){ .parent = NULL, .ranges = NULL, \
                                     .hwm = 0, .size = 0, .count = 0 }

typedef struct fs2slice {
    fs2             *parent;   /* reads parent->v each access — realloc-safe */
    fs2sliceview     range;
} fs2slice;                      

#define FS2SLICE() (fs2slice){ .parent = NULL, .range = FS2SLICEVIEW() }

static inline size_t
fs2slice_len(const fs2slice *s) { return s->range.to - s->range.from; }

static inline const char *
fs2slice_data(const fs2slice *s) { return s->parent->v + s->range.from; }

// leak-checking facility
extern long
fs2slice_inc(void);

extern long
fs2slice_dec(void);

extern long 
fs2slice_sum(void);

// internal
static inline int32_t
fs2slice_parentinc(fs2 *parent) {
    fs2slice_inc();
    return ++parent->borrow;       // not atomic: external lock required (see header)
}

// internal
static inline int32_t
fs2slice_parentdec(fs2 **parent) {
    fs2 *s = *parent;
    if (s->borrow == 0)
        userraise(false, ERR_FS2SLICE_UNDERFLOW, 
            "borrow underflow");
    int32_t res = --s->borrow;       // not atomic: external lock required (see header)
    *parent = NULL;
    fs2slice_dec();
    return res;
}

static inline fs2slice
fs2slice_new(fs2 *parent, size_t from, size_t to) {
    invfatal(ERR_NULL_INPUT, parent != NULL, "Null parent");
    if (from >= to || to > parent->len)
        userraise(0, ERR_OUT_OF_RANGE,
             "Out of range [%zu, %zu) not in [0, %zu]",
             from, to, parent->len);
    fs2slice_parentinc(parent);
    return (fs2slice){ .parent = parent, .range = {.from = from, .to = to} };
}

static inline fs2slice
fs2arrslice_get(const fs2arrslice *a, size_t i) {
    invfatal(ERR_NULL_INPUT, a != NULL, "Null arslice");

    if (i >= a->count)
        userraise(0, ERR_OUT_OF_RANGE,
            "idx %zu >= count %zu", i, a->count);
    return fs2slice_new(a->parent, a->ranges[i].from, a->ranges[i].to);
}

static inline void
fs2slice_free(fs2slice *s) {
    if (!s || !s->parent)
        return;

    fs2slice_parentdec(&s->parent);
    s->range  = FS2SLICEVIEW();
}

static inline void
fs2arrslice_free(fs2arrslice *s) {
    if (!s || !s->parent)
        return;
    
    fs2slice_parentdec(&s->parent);
    free(s->ranges);
    *s = FS2ARRSLICE();
}

// API

 #endif /* !FS2SLICE_H */
