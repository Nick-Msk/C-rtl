 
#include "fs2slice.h"
#include <stdatomic.h>

 // mem leak checking
 static atomic_long              g_borrow_sum            = 0;

long
fs2slice_inc(void) {
    return atomic_fetch_add(&g_borrow_sum, 1); 
}

long
fs2slice_dec(void) {
    return atomic_fetch_add(&g_borrow_sum, -1); 
}

long 
fs2slice_sum(void) { 
    return atomic_load(&g_borrow_sum); 
}

/* =========================================================================
 * Internal — shared between slice and arrslice
 * ========================================================================= */

enum { FS2SLICE_PREVIEW = 40 };     /* slice content bytes shown */

/* Validate parent invariants (both types need the same checks). */
static inline bool
__slice_validate_parent(FILE *out, const fs2 *parent, const char *ctx) {
    if (!parent)
        return LOGNPRINTF(out, "%s: parent == NULL (zombie)", ctx);
    if (!parent->v)
        return LOGNPRINTF(out, "%s: parent->v == NULL (canonical empty)", ctx);
    if (parent->borrow == 0)
        return LOGNPRINTF(out, "%s: parent->borrow == 0 (orphan)", ctx);
    return true;
}

/* Validate a single [from, to) range against parent->len. */
static inline bool
__slice_validate_range(FILE *out, const fs2 *parent,
                       size_t from, size_t to, size_t idx, const char *ctx) {
    if (from >= to)
        return LOGNPRINTF(out,
            "%s: range[%zu] [%zu, %zu) empty or reversed",
            ctx, idx, from, to);
    if (to > parent->len)
        return LOGNPRINTF(out,
            "%s: range[%zu].to %zu > parent->len %zu",
            ctx, idx, to, parent->len);
    return true;
}

/* Print "[idx] [from, to) [preview]" line. Shared by both. */
static inline int
__slice_fprint_range(FILE *out, const fs2 *parent,
                     size_t idx, size_t from, size_t to) {
    size_t slen = to - from;
    size_t show = slen < FS2SLICE_PREVIEW ? slen : FS2SLICE_PREVIEW;
    int    ell  = (slen > FS2SLICE_PREVIEW);
    return fprintf(out,
        "  [%zu] [%zu, %zu) [%.*s%s]\n",
        idx, from, to, (int)show,
        parent->v ? parent->v + from : "",
        ell ? "..." : "");
}

/* Header shared by both. `count`/`size` = 1/1 for slice, real counts for arrslice. */
static inline int
__slice_fprint_header(FILE *out, const char *kind, const char *name,
                      const fs2 *parent, size_t count, size_t size) {
    return fprintf(out,
        "%s: %s: parent(%p)[len=%zu, borrow=%u] count[%zu], size[%zu][\n",
        kind, name, (void *)parent,
        parent->len, parent->borrow, count, size);
}

/* =========================================================================
 * validate
 * ========================================================================= */

bool
fs2slice_validate(FILE *restrict out, const fs2slice *restrict s) {
    if (!s)
        return LOGNPRINTF(out, "null pointer");
    if (!__slice_validate_parent(out, s->parent, "fs2slice_validate"))
        return false;
    return __slice_validate_range(out, s->parent,
                                  s->range.from, s->range.to,
                                  0, "fs2slice_validate");
}

bool
fs2arrslice_validate(FILE *restrict out, const fs2arrslice *restrict a) {
    if (!a)
        return LOGNPRINTF(out, "null pointer");

    /* canonical empty — special case before parent check */
    if (!a->parent) {
        if (a->ranges || a->hwm || a->size || a->count)
            return LOGNPRINTF(out,
                "null parent but ranges=%p hwm=%zu size=%zu count=%zu",
                (void *)a->ranges, a->hwm, a->size, a->count);
        return true;
    }

    if (a->count > a->hwm)
        return LOGNPRINTF(out, "count %zu > hwm %zu", a->count, a->hwm);
    if (a->hwm > a->size)
        return LOGNPRINTF(out, "hwm %zu > size %zu", a->hwm, a->size);
    if (a->count > 0 && a->ranges == NULL)
        return LOGNPRINTF(out, "count %zu > 0 but ranges == NULL", a->count);

    if (!__slice_validate_parent(out, a->parent, "fs2arrslice_validate"))
        return false;

    for (size_t i = 0; i < a->count; i++)
        if (!__slice_validate_range(out, a->parent,
                                    a->ranges[i].from, a->ranges[i].to,
                                    i, "fs2arrslice_validate"))
            return false;
    return true;
}

long
fs2slice_techfprint(FILE *restrict out, const fs2slice *restrict s,
                    const char *restrict name) {
    if (!out) return 0;
    const char *n = name ? name : "(unnamed)";

    if (!s) {
        int w = fprintf(out, "FS2SLICE: %s: <NULL>\n", n);
        return w < 0 ? -1 : (long)w;
    }
    if (!s->parent) {
        int w = fprintf(out, "FS2SLICE: %s: <zombie>\n", n);
        return w < 0 ? -1 : (long)w;
    }

    int w = __slice_fprint_header(out, "FS2SLICE", n, s->parent, 1, 1);
    if (w < 0) return -1;
    long total = w;

    w = __slice_fprint_range(out, s->parent, 0, s->range.from, s->range.to);
    if (w < 0) return -1;
    total += w;

    w = fprintf(out, "]\n");
    if (w < 0) return -1;
    return total + w;
}

long
fs2arrslice_techfprint(FILE *restrict out, const fs2arrslice *restrict a,
                       const char *restrict name) {
    if (!out) return 0;
    const char *n = name ? name : "(unnamed)";

    if (!a) {
        int w = fprintf(out, "FS2ARRSLICE: %s: <NULL>\n", n);
        return w < 0 ? -1 : (long)w;
    }
    if (!a->parent) {
        int w = fprintf(out, "FS2ARRSLICE: %s: <empty>\n", n);
        return w < 0 ? -1 : (long)w;
    }

    int w = __slice_fprint_header(out, "FS2ARRSLICE", n,
                                   a->parent, a->count, a->size);
    if (w < 0) return -1;
    long total = w;

    size_t shown = a->count < FS2SLICE_TECH_PRINT_COUNT
                   ? a->count : FS2SLICE_TECH_PRINT_COUNT;
    for (size_t i = 0; i < shown; i++) {
        w = __slice_fprint_range(out, a->parent, i,
                                 a->ranges[i].from, a->ranges[i].to);
        if (w < 0) return -1;
        total += w;
    }
    if (shown < a->count) {
        w = fprintf(out, "  ... (%zu more)\n", a->count - shown);
        if (w < 0) return -1;
        total += w;
    }
    w = fprintf(out, "]\n");
    if (w < 0) return -1;
    return total + w;
}

