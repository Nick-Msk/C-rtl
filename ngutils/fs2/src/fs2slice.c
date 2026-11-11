
#include "fs2slice.h"
#include <stdatomic.h>

/*
 * Global leak-checking counter.
 * Tracks the net number of active slice borrows across all fs2 objects
 * in the process. Used by test harnesses / debug builds to detect
 * unbalanced fs2slice_new / fs2slice_free calls.
 *
 * Thread-safety: atomic — safe to call from any thread.
 */
static atomic_long g_borrow_sum = 0;

/**
 * @brief Atomically increment the global borrow counter.
 *
 * Called internally by `fs2slice_parentinc()`. Increments `g_borrow_sum`
 * and returns the value it had **before** the increment (i.e. the
 * previous count).
 *
 * @return The pre-increment value of the global borrow counter.
 */
long
fs2slice_inc(void) {
    return atomic_fetch_add(&g_borrow_sum, 1);
}

/**
 * @brief Atomically decrement the global borrow counter.
 *
 * Called internally by `fs2slice_parentdec()`. Decrements `g_borrow_sum`
 * and returns the value it had **before** the decrement (i.e. the
 * previous count).
 *
 * @return The pre-decrement value of the global borrow counter.
 */
long
fs2slice_dec(void) {
    return atomic_fetch_add(&g_borrow_sum, -1);
}

/**
 * @brief Return the current value of the global borrow counter.
 *
 * Useful in test teardown / diagnostic code: a non-zero return at
 * program exit indicates leaked (unfreed) slices.
 *
 * @return Current count of active slice borrows (may be negative if
 *         underflowed).
 */
long
fs2slice_sum(void) {
    return atomic_load(&g_borrow_sum);
}

/* =========================================================================
 * Internal — shared between slice and arrslice
 * ========================================================================= */

enum { FS2SLICE_PREVIEW = 40 };     /* max content bytes shown in a preview line */

/**
 * @internal Validate parent invariants shared by both slice and arrslice.
 *
 * Checks: non-NULL parent, non-NULL `parent->v`, `parent->borrow > 0`.
 * On failure, logs a diagnostic via `LOGNPRINTF` and returns false.
 *
 * @param out     Output stream for diagnostics.
 * @param parent  Parent fs2 to validate.
 * @param ctx     Context label for log messages (e.g. function name).
 * @return        true if all parent invariants hold.
 */
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

/**
 * @internal Validate a single [from, to) range against parent->len.
 *
 * Checks: `from < to` (non-empty, non-reversed) and `to <= parent->len`.
 * On failure, logs a diagnostic via `LOGNPRINTF` and returns false.
 *
 * @param out     Output stream for diagnostics.
 * @param parent  Parent fs2 providing the length bound.
 * @param from    Start offset (inclusive).
 * @param to      End offset (exclusive).
 * @param idx     Range index (used only in log messages).
 * @param ctx     Context label for log messages.
 * @return        true if the range is valid.
 */
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

/**
 * @internal Print one `  [idx] [from, to) [preview]` line.
 *
 * Shows up to FS2SLICE_PREVIEW bytes of content; appends `...` if truncated.
 * Shared by `fs2slice_techfprint` and `fs2arrslice_techfprint`.
 *
 * @param out     Output stream.
 * @param parent  Parent fs2 (content source).
 * @param idx     Index for display.
 * @param from    Start offset.
 * @param to      End offset.
 * @return        Number of characters written, or negative on I/O error.
 */
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

/**
 * @internal Print the common header line shared by both techfprint variants.
 *
 * For a single slice: `count`/`size` = 1/1.
 * For an arrslice: real `count`/`size` values.
 *
 * @param out     Output stream.
 * @param kind    Type label ("FS2SLICE" or "FS2ARRSLICE").
 * @param name    Display name of the object.
 * @param parent  Parent fs2 (for len/borrow display).
 * @param count   Number of ranges.
 * @param size    Allocated capacity.
 * @return        Number of characters written, or negative on I/O error.
 */
static inline int
__slice_fprint_header(FILE *out, const char *kind, const char *name,
                      const fs2 *parent, size_t count, size_t size) {
    return fprintf(out,
        "%s: %s: parent(%p)[len=%zu, borrow=%u] count[%zu], size[%zu][\n",
        kind, name, (void *)parent,
        parent->len, parent->borrow, count, size);
}

/* =========================================================================
 * Validate
 * ========================================================================= */

/**
 * @brief Validate a single slice; log diagnostics on failure.
 *
 * Checks: non-NULL pointer, non-NULL parent, non-NULL `parent->v`,
 * `parent->borrow > 0`, `from < to`, `to <= parent->len`.
 *
 * @param out  Output stream for error messages (may be NULL).
 * @param s    Slice to validate (may be NULL — will be reported).
 * @return     true if all invariants hold; false otherwise.
 */
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

/**
 * @brief Validate an array-slice; log diagnostics on failure.
 *
 * Checks: non-NULL pointer, canonical-empty consistency (if `parent == NULL`,
 * all other fields must be zero), `count <= hwm <= size`, `count > 0`
 * implies `ranges != NULL`, parent invariants, and per-range bounds.
 *
 * @param out  Output stream for error messages (may be NULL).
 * @param a    Array-slice to validate (may be NULL — will be reported).
 * @return     true if all invariants hold; false otherwise.
 */
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

/**
 * @brief Dump a human-readable technical representation of a single slice.
 *
 * Prints the parent pointer, len, borrow, and the [from, to) range with a
 * content preview (up to FS2SLICE_PREVIEW bytes). Handles NULL and zombie
 * (null-parent) slices gracefully.
 *
 * @param out   Output stream (NULL → returns 0).
 * @param s     Slice to dump (may be NULL or zombie).
 * @param name  Display name for the slice (NULL → "(unnamed)").
 * @return      Number of characters written, or -1 on I/O error.
 */
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

/**
 * @brief Dump a human-readable technical representation of an array-slice.
 *
 * Prints the parent pointer, len, borrow, count/size, and up to
 * FS2SLICE_TECH_PRINT_COUNT ranges with content previews. If there are
 * more ranges, a `... (N more)` line is emitted. Handles NULL and empty
 * (null-parent) values gracefully.
 *
 * @param out   Output stream (NULL → returns 0).
 * @param a     Array-slice to dump (may be NULL or empty).
 * @param name  Display name (NULL → "(unnamed)").
 * @return      Number of characters written, or -1 on I/O error.
 */
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

