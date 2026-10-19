/**
 * @file fs2.h
 * @brief Core primitives for a faststring type.
 *
 * This is the primitive layer: struct, flags, constructors, move/copy,
 * adopt, free. Higher-level operations (append, printf, substr, reserve,
 * comparison helpers) belong in fs2_api.h — this file deliberately stays
 * minimal so the invariant surface is small and testable.
 *
 * Two orthogonal axes:
 *   1. Where does the string live?   STATIC | LOCAL | ALLOC  (exactly one)
 *   2. Where does the struct live?   BODYALLOC or not         (independent)
 *
 * Plus two state flags, orthogonal to both:
 *   CONST    — reserved
 *
 * Preconditions (NOT checked here — see cfs2.h for a guarded wrapper):
 *   - No NULL pointers where a valid fs2 is expected.
 *   - len < sz.
 *   - No arithmetic overflow in size_t computations.
 *   - No use after fs2_free / fs2_movefrom on a heap value.
 */

#ifndef FS2_H
#define FS2_H

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bool.h"
#include "error.h"
#include "common.h"
#include "checker.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────── */

#define FS2_VERSION_MAJOR 0
#define FS2_VERSION_MINOR 1
#define FS2_VERSION_PATCH 0

#define FS2_STRINGIFY_(x) #x
#define FS2_STRINGIFY(x)  FS2_STRINGIFY_(x)

#define FS2_VERSION \
    FS2_STRINGIFY(FS2_VERSION_MAJOR) "." \
    FS2_STRINGIFY(FS2_VERSION_MINOR) "." \
    FS2_STRINGIFY(FS2_VERSION_PATCH)

/**
 * @brief Return the current library version string (e.g. `"0.1.0"`).
 *
 * @return Non-NULL static string, valid for the lifetime of the program.
 */
extern const char           *fs2_version(void);

/**
 * @brief Return a NULL-terminated array of version strings.
 *
 * `fs2_versions()[0]` is the current version; the array may be extended
 * in future with historical entries.
 *
 * @return Pointer to a `const char*` array terminated by `NULL`.
 */
extern const char    *const *fs2_versions(void);

/* ─────────────────────────────────────────────────────────────────────────
 * Flags
 * ───────────────────────────────────────────────────────────────────────── */

typedef enum {
    FS2_CONST     = 0x01,   /**< Reserved.                              */
    FS2_STATIC    = 0x02,   /**< v is external, read-only (literal).    */
    FS2_LOCAL     = 0x04,   /**< v is caller-owned writable buffer.     */
    FS2_ALLOC     = 0x08,   /**< v is heap-owned; growable via realloc. */
    FS2_BODYALLOC = 0x10,   /**< the struct itself is heap-allocated.   */
} fs2_flags;

/**
 * @brief Convert a flag value to a human-readable string.
 *
 * @param fl  A single flag value (exactly one bit set).
 * @return   Static string like `"FS2_ALLOC"`; `"UNKNOWN"` for compound/zero values.
 */
static inline const char *
fs2_flag_name(fs2_flags fl) {
    switch (fl) {
        case FS2_STATIC:    return "FS2_STATIC";
        case FS2_CONST:     return "FS2_CONST";
        case FS2_LOCAL:     return "FS2_LOCAL";
        case FS2_ALLOC:     return "FS2_ALLOC";
        case FS2_BODYALLOC: return "FS2_BODYALLOC";
        default:            return "UNKNOWN";
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * Struct
 * ───────────────────────────────────────────────────────────────────────── */

typedef struct fs2 {
    size_t      len;        /**< Logical length, excluding NUL.             */
    size_t      sz;         /**< Capacity in bytes, including NUL.          */
    fs2_flags   flags;      /**< state flags                                */
    char       *v;          /**< NUL-terminated;                            */
} fs2;

/* ─────────────────────────────────────────────────────────────────────────
 * Value macros
 * ───────────────────────────────────────────────────────────────────────── */

/** Empty growable value (stack body, ALLOC, no capacity yet). */
#define FS2()         (fs2){ .len = 0, .sz = 0, \
                                .flags = FS2_ALLOC,  .v = NULL }

/** Empty read-only value. Safe to share; fs2_free is a no-op. */
#define FS2_EMPTY        (fs2){ .len = 0, .sz = 1, \
                                .flags = FS2_STATIC, .v = (char *)"" }

/** Literal: wraps a compile-time string. sizeof-1 gives strlen for free. */
#define FS2_LITERAL(s)   (fs2){ .len = sizeof(s) - 1, .sz = sizeof(s), \
                                .flags = FS2_STATIC, .v = (char *)(s) }

/** Local: wraps a caller-owned writable buffer (fixed capacity). */
#define FS2_LOCAL(buf)   (fs2){ .len = 0, .sz = sizeof(buf), \
                                .flags = FS2_LOCAL,  .v = (buf) }

/* ─────────────────────────────────────────────────────────────────────────
 * Getters (all const-safe, no allocation)
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Logical length of the string (excluding the trailing NUL).
 *
 * @param s  Non-NULL pointer to an fs2.
 * @return   Number of significant bytes.
 */
static inline size_t      fs2_len (const fs2 *s) { return s->len; }

/**
 * @brief Total buffer capacity in bytes (including room for NUL).
 *
 * @param s  Non-NULL pointer to an fs2.
 * @return   Allocated capacity.
 */
static inline size_t      fs2_sz  (const fs2 *s) { return s->sz;  }

/**
 * @brief Mutable pointer to the underlying NUL-terminated buffer.
 *
 * Use for in-place mutation. Only valid when the buffer is writable
 * (FS2_ALLOC or FS2_LOCAL). UB to write to a FS2_STATIC buffer.
 *
 * @param s  Non-NULL pointer to an fs2.
 * @return   `char*` to the buffer; `NULL` if the buffer has been released.
 */
static inline char       *fs2_str (fs2 *s)       { return s->v;   }

/**
 * @brief Const pointer to the underlying NUL-terminated buffer.
 *
 * Safe for all string kinds (STATIC / LOCAL / ALLOC).
 *
 * @param s  Non-NULL pointer to an fs2.
 * @return   `const char*` to the buffer; `NULL` if the buffer has been released.
 */
static inline const char *fs2_cstr(const fs2 *s) { return s->v;   }

/* ─────────────────────────────────────────────────────────────────────────
 * Predicates
 * ───────────────────────────────────────────────────────────────────────── */

/** @brief Check if the buffer is an external read-only literal (FS2_STATIC). */
static inline bool          fs2_is_static   (const fs2 *s) { return (s->flags & FS2_STATIC)    != 0; }

/** @brief Check if the buffer is a caller-owned fixed-size buffer (FS2_LOCAL). */
static inline bool          fs2_is_local    (const fs2 *s) { return (s->flags & FS2_LOCAL)     != 0; }

/** @brief Check if the buffer is heap-allocated and growable (FS2_ALLOC). */
static inline bool          fs2_is_alloc    (const fs2 *s) { return (s->flags & FS2_ALLOC)     != 0; }

/** @brief Check if the fs2 struct itself is heap-allocated (FS2_BODYALLOC). */
static inline bool          fs2_is_bodyalloc(const fs2 *s) { return (s->flags & FS2_BODYALLOC) != 0; }

/** @brief Logical emptiness: len == 0. */
static inline bool          fs2_is_empty(const fs2 *s) { 
    return s == NULL || s->len == 0;
}

/**
 * @brief Null-check: either @p s itself is NULL or its buffer is NULL.
 *
 * Defunct values have @c v == NULL and therefore also report as null.
 */
static inline bool          fs2_is_null(const fs2 *s) { 
    return s == NULL || s->v == NULL; 
}

/**
 * @brief Declare a stack-allocated fixed-capacity fs2 (FS2_LOCAL).
 *
 * Declares a `char` buffer of @p n bytes alongside a named `fs2` that
 * wraps it. The buffer is zero-initialised. The caller owns the memory;
 * `fs2_free` is a no-op. Cannot be moved/exchanged — use `fs2_clone`
 * to copy content elsewhere.
 *
 * @param name  Identifier for the fs2 variable.
 * @param n     Buffer size in bytes (including NUL slot).
 *
 * @code
 *   FS2LOCAL(mybuf, 256);
 *   fs2_setlen(fs2_str(&mybuf), 0);  // write into mybuf
 * @endcode
 *
 * @note Function-scope only. Not usable in structs or file scope.
 */
#define FS2LOCAL(name, n)                          \
    char _fs2_buf_##name[(n)];                     \
    _fs2_buf_##name[0] = '\0';                     \
    fs2 name = (fs2){ .len = 0, .sz = (n),         \
                      .flags = FS2_LOCAL,          \
                      .v = _fs2_buf_##name }

/**
 * @brief Declare a stack-allocated fs2 pre-filled with a formatted string.
 *
 * Uses a VLA sized to the formatted output + 1. Calls `snprintf` twice
 * (once to measure, once to write), so it is **not** suitable for hot
 * paths.
 *
 * @param name  Identifier for the fs2 variable.
 * @param fmt   printf-style format string.
 * @param ...   Format arguments.
 *
 * @code
 *   FS2LOCALFILL(msg, "count = %d", n);
 * @endcode
 *
 * @note C99 VLA — function-scope only, not C++-compatible, not usable
 *       in structs or file scope.
 */
#define FS2LOCALFILL(name, fmt, ...)                                          \
    char _fs2_internal_buf_##name[snprintf(NULL, 0, fmt, ##__VA_ARGS__) + 1]; \
    snprintf(_fs2_internal_buf_##name, sizeof(_fs2_internal_buf_##name),      \
             fmt, ##__VA_ARGS__);                                             \
    fs2 name = (fs2){                                                         \
        .len   = sizeof(_fs2_internal_buf_##name) - 1,                        \
        .sz    = sizeof(_fs2_internal_buf_##name),                            \
        .flags = FS2_LOCAL,                                                   \
        .v     = _fs2_internal_buf_##name                                     \
    }

/**
 * @brief Ensure capacity for at least `need` bytes (including NUL).
 * No-op for STATIC/LOCAL (cannot grow external buffers).
 */
void    fs2_reserve(fs2 *s, size_t need);

/* ─────────────────────────────────────────────────────────────────────────
 * Constructors — value (stack body)
 *
 *   fs2_init(sz)         growable, capacity sz
 *   fs2_fromstr(s)       copy of a C-string
 *   fs2_fromstrf(f, ...) printf-formatted copy
 *   fs2_clone(src)       deep copy of any fs2
 *   fs2_movefrom(src)    steal from a stack fs2
 *   fs2_movefromstr(p)   steal a heap C-string (p becomes NULL)
 * ───────────────────────────────────────────────────────────────────────── */
 
/**
 * @brief Create a growable fs2 with the given initial capacity.
 *
 * Returns a stack-body fs2 (no BODYALLOC) with FS2_ALLOC set. The buffer
 * is heap-allocated via `realloc`/`malloc` with at least @p sz bytes
 * (minimum 16). Length is 0; the buffer is zero-initialised.
 *
 * @param sz  Desired initial capacity in bytes (including NUL slot).
 * @return    fs2 value with a fresh heap buffer.
 * @warning   `sysraise` if allocation fails (OOM).
 */
fs2   fs2_init    (size_t sz);

/**
 * @brief Create a growable fs2 as a deep copy of a C string.
 *
 * @param src  Non-NULL NUL-terminated source.
 * @return     fs2 value (ALLOC) with a heap copy of @p src.
 * @warning    `invraise` if @p src is NULL.
 */
fs2   fs2_fromstr (const char *src);

/**
 * @brief Create a growable fs2 via printf-style formatting.
 *
 * @param fmt  Format string (must not be NULL).
 * @param ...  Format arguments.
 * @return     fs2 value (ALLOC) containing the formatted result.
 * @warning    `userraise` on format error; `invraise` if @p fmt is NULL.
 */
fs2   fs2_fromstrf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Deep-copy any fs2 into a new growable one.
 *
 * The result is always FS2_ALLOC with a fresh heap buffer, regardless of
 * the source kind. If the source buffer is NULL (defunct), returns FS2().
 *
 * @param src  Non-NULL source fs2.
 * @return     fs2 value (ALLOC) with a heap copy of the source content.
 * @warning    `invraise` if @p src is NULL.
 */
fs2   fs2_clone   (const fs2 *src);

/**
 * @brief Steal the buffer from @p src into a new stack fs2.
 *
 * After the call, @p src is in the canonical empty state (FS2(),
 * preserving its BODYALLOC if it was heap-allocated). Works for
 * ALLOC and STATIC sources. **Forbidden for LOCAL** (invraise).
 *
 * @param src  Non-NULL source fs2 (ALLOC or STATIC).
 * @return     fs2 value that now owns the former src buffer.
 * @warning    `invraise` if @p src is NULL or FS2_LOCAL.
 */
fs2   fs2_movefrom(fs2 *src);

/**
 * @brief Steal a heap or static C string into an fs2.
 *
 * After the call, `*from` is set to NULL. The caller must guarantee
 * that the pointer is valid for the specified flag kind.
 *
 * @param from  Pointer to the char* to steal; must be non-NULL and
 *              `*from` must be non-NULL. Set to NULL on success.
 * @param fl    FS2_STATIC (read-only, e.g. from a literal table)
 *              or FS2_ALLOC (heap, will be freed by fs2_free).
 * @return      fs2 value wrapping the stolen pointer.
 * @warning     `invraise` if pointers are NULL; `userraise` for unsupported flag.
 */
fs2   fs2_movefromstr(const char **from, fs2_flags fl);

/**
 * @brief Adopt a heap-allocated C string with a known length.
 *
 * 
 * 
 * After the call, `*str` is set to NULL. If `str` or `*str` is NULL,
 * returns FS2() (empty growable).
 *
 * @param str       Pointer to the heap char* to adopt; set to NULL.
 * @param saved_len Known logical length (avoids a strlen call).
 * @return          fs2 value (ALLOC) wrapping the adopted buffer.
 */
fs2   fs2_adoptn(char **str, size_t saved_len);

/**
 * @brief Adopt a heap-allocated C string (length computed via strlen).
 *
 * Convenience wrapper around fs2_adoptn. If `str` or `*str` is NULL,
 * returns FS2().
 *
 * @param str  Pointer to the heap char* to adopt; set to NULL on success.
 * @return     fs2 value (ALLOC) wrapping the adopted buffer.
 */
fs2   fs2_adopt(char **str);

/* ─────────────────────────────────────────────────────────────────────────
 * Constructors — heap (BODYALLOC set)
 *
 * Same operations, but the fs2 struct itself is allocated. Free with
 * fs2_free (which frees the buffer, the struct, or both, per flags).
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocate an empty fs2 on the heap (BODYALLOC set).
 *
 * The returned struct is initialised to FS2() (ALLOC, v=NULL) with
 * BODYALLOC. Free with fs2_free.
 *
 * @return  Heap-allocated fs2*, or raises on OOM.
 */
fs2     *fs2_heap          (void);

/**
 * @brief Allocate a growable fs2 on the heap with given capacity.
 *
 * Equivalent to fs2_init but the struct itself is heap-allocated.
 *
 * @param sz  Initial buffer capacity (including NUL slot).
 * @return    Heap-allocated fs2* (BODYALLOC | ALLOC), or raises on OOM.
 */
fs2     *fs2_heap_init     (size_t sz);

/**
 * @brief Heap-allocate an fs2 as a deep copy of a C string.
 *
 * @param src  Non-NULL NUL-terminated source.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC) with a heap copy.
 */
fs2     *fs2_heap_fromstr  (const char *src);

/**
 * @brief Heap-allocate an fs2 via printf-style formatting.
 *
 * @param fmt  Format string (must not be NULL).
 * @param ...  Format arguments.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC).
 */
fs2     *fs2_heap_fromstrf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Heap-allocate an fs2 as a deep copy of another fs2.
 *
 * The result is always BODYALLOC | ALLOC regardless of source kind.
 *
 * @param src  Non-NULL source fs2.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC) with a heap copy.
 */
fs2     *fs2_heap_clone    (const fs2 *src);

/**
 * @brief Deprecated: heap-allocate an fs2 by stealing from @p src.
 *
 * Always raises ERR_NOT_IMPLEMENTED. Use fs2_stacktoheap instead.
 *
 * @param src  Source fs2 (unused).
 * @deprecated Use fs2_stacktoheap.
 */
[[deprecated]]
fs2     *fs2_heap_movefrom (fs2 *src);

/**
 * @brief Heap-allocate an fs2 by stealing a C string pointer.
 *
 * Wraps fs2_movefromstr into a heap body. `*from` is set to NULL.
 *
 * @param from  Pointer to the char* to steal; set to NULL.
 * @param fl    FS2_STATIC or FS2_ALLOC.
 * @return      Heap-allocated fs2* (BODYALLOC + given flag).
 */
fs2     *fs2_heap_movefromstr(const char **from, fs2_flags fl);

/* ─────────────────────────────────────────────────────────────────────────
 * Move assignment (not a constructor)
 *
 *   to ← from, then from → empty. Frees whatever `to` held before.
 * ───────────────────────────────────────────────────────────────────────── */
 
/**
 * @brief Move-assign: @p to ← @p from, then @p from → canonical empty.
 *
 * Frees whatever @p to previously held (if ALLOC). After the call:
 * - @p to   owns the former source buffer/pointer.
 * - @p from is reset to FS2() (ALLOC, v=NULL), preserving its BODYALLOC.
 *
 * The BODYALLOC flag of each side is independent and preserved.
 *
 * **LOCAL is forbidden** on both sides — moving a caller-owned buffer
 * would create an alias. Use fs2_clone for sharing.
 *
 * @param to    Destination; must be non-NULL. Its previous ALLOC buffer
 *              is freed before the move.
 * @param from  Source; must be non-NULL and not LOCAL. Reset to FS2().
 *
 * @return      @p to (for chaining).
 *
 * @note        fs2_move(x, x) is a no-op that returns @p to.
 * @warning     `invraise` if either pointer is NULL or either side is FS2_LOCAL.
 */
fs2     *fs2_move(fs2 *to, fs2 *from);

/**
 * @brief Exchange the string values of two fs2 objects.
 *
 * Both sides must be FS2_ALLOC — exchanging with STATIC or LOCAL would
 * leave the ALLOC side pointing at a buffer it does not own (and the
 * STATIC/LOCAL side pointing at a buffer fs2_free would skip, leaking
 * it). The BODYALLOC flag of each side is preserved; it describes
 * where the struct lives, not the value being swapped.
 *
 * @param a  First fs2. Must not be NULL, must be FS2_ALLOC.
 * @param b  Second fs2. Same constraints.
 *
 * @note fs2_exch(a, a) is a no-op.
 * @warning userraise(SIGINT) if either side is not FS2_ALLOC.
 */
void        fs2_exch(fs2 *a, fs2 *b);

/**
 * @brief Convert a heap-allocated fs2 to a stack value.
 *
 * Moves the value out of the heap struct, frees the struct, and sets
 * `*psrc` to NULL. The result has BODYALLOC cleared.
 *
 * @param psrc  Pointer to a heap fs2* (must be non-NULL and BODYALLOC).
 *              Set to NULL on success.
 * @return      Stack fs2 value with the former heap value's content.
 *
 * @warning     `invraise` if pointers are NULL; `userraise` if not BODYALLOC
 *              or if the source is FS2_LOCAL.
 */
fs2         fs2_heaptostack(fs2 **psrc);

/**
 * @brief Convert a stack fs2 to a heap-allocated fs2.
 *
 * Moves the value into a new heap struct (BODYALLOC set). The source is
 * reset to FS2(). The source must not be LOCAL or already BODYALLOC.
 *
 * @param src  Stack fs2 (non-NULL, not BODYALLOC, not LOCAL).
 * @return     New heap fs2* owning the value.
 *
 * @warning    `invraise` if NULL or BODYALLOC; `userraise` if LOCAL.
 */
fs2        *fs2_stacktoheap(fs2 *src);

/* ─────────────────────────────────────────────────────────────────────────
 * Destructor
 *
 * Frees the buffer if ALLOC, frees the struct if BODYALLOC. No-op on
 * STATIC, LOCAL. Idempotent for value (non-BODYALLOC) cases.
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Destructor: release all resources owned by an fs2.
 *
 * - If FS2_ALLOC: frees the buffer (`free(s->v)`).
 * - If FS2_BODYALLOC: frees the struct itself.
 * - STATIC / LOCAL: no-op (no owned memory).
 *
 * Safe to call on a NULL pointer (returns immediately). Idempotent for
 * non-BODYALLOC values: after freeing an ALLOC buffer the state is
 * canonical empty (v=NULL, len=0, sz=0), so a second call is harmless.
 *
 * @param s  Pointer to fs2, or NULL (no-op).
 */
void        fs2_free(fs2 *s);

/* ─────────────────────────────────────────────────────────────────────────
 * Utilities
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Set the logical length and NUL-terminate at @p pos.
 *
 * Writes `s->v[pos] = '\0'` and sets `s->len = pos`.
 * The caller must guarantee `pos < s->sz`.
 *
 * @param s    Non-NULL fs2 with a non-NULL buffer.
 * @param pos  New logical length.
 * @return     @p s for chaining.
 *
 * @note       No bounds check is performed.
 */
static inline fs2 *fs2_setlen(fs2 *s, size_t pos) {
    s->v[s->len = pos] = '\0';
    return s;
}

/**
 * @brief Reset the logical length to zero (empty the string in place).
 *
 * Equivalent to `fs2_setlen(s, 0)`. The buffer is preserved with its
 * capacity; only the content is logically discarded.
 *
 * @param s  Non-NULL fs2 with a non-NULL buffer.
 * @return   @p s for chaining.
 */
static inline fs2 *fs2_clear(fs2 *s) {
    return fs2_setlen(s, 0);
}
/* ─────────────────────────────────────────────────────────────────────────
 * Tech printer
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Print a one-line technical dump of an fs2 to a stream.
 *
 * Outputs: len, capacity, flags (decimal), buffer pointer, and up to
 * 100 characters of content (truncated with `...` if longer).
 * If @p s is NULL, prints `<NULL>`.
 *
 * @param out   Output stream; if NULL, nothing is written.
 * @param s     fs2 to print (may be NULL).
 * @param name  Label printed before the values.
 * @return      Number of characters written (0 if @p out is NULL).
 */
long    fs_techfprint(FILE *restrict out, const fs2 *restrict s, const char *restrict name);

 /* ─────────────────────────────────────────────────────────────────────────
 * Validator
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Validate all invariants of an fs2.
 *
 * Checks:
 * 1. No unknown flag bits.
 * 2. Exactly one of STATIC / LOCAL / ALLOC is set.
 * 3. Buffer presence: STATIC/LOCAL require v; ALLOC allows v==NULL only
 *    in canonical empty state (len==0, sz==0).
 * 4. len < sz.
 * 5. strlen(v) == len.
 *
 * On failure, a diagnostic message is written to @p out (if non-NULL)
 * and to the internal error log.
 *
 * @param out  Optional output stream for the diagnostic; may be NULL.
 * @param s    fs2 to validate (may be NULL — reports and returns false).
 * @return     true if all invariants hold; false otherwise.
 */
bool    fs2_validate(FILE *restrict out, const fs2 *restrict s);

/**
 * @brief Compare two fs2 objects for technical equality.
 *
 * Unlike logical comparison (strcmp-style), this also checks flags:
 * FS2_ALLOC vs FS2_STATIC vs FS2_LOCAL, and BODYALLOC. Two fs2 that
 * print the same content but have different ownership are considered
 * different.
 *
 * @return true if len, flags and content are identical; false otherwise.
 * @note invraise(SIGABRT) if either pointer is NULL.
 */
bool    fs2_techcmp(const fs2 *s1, const fs2 *s2);

/* ─────────────────────────────────────────────────────────────────────────
 * Leak checking
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Check for internal memory leaks by comparing alloc/free counters.
 *
 * Compares the global allocation and deallocation counters. If any
 * counter pair is mismatched, the current values are written to the
 * corresponding output parameters (if non-NULL) and a diagnostic is
 * logged.
 *
 * All parameters are optional (pass NULL to skip output for a pair).
 *
 * @param alloc      Out: current buffer allocation count (if mismatch).
 * @param freed      Out: current buffer free count (if mismatch).
 * @param bodyalloc  Out: current struct allocation count (if mismatch).
 * @param bodyfreed  Out: current struct free count (if mismatch).
 * @return           true if no leaks detected; false otherwise.
 */
bool   fs2_checkleak(int *restrict alloc, int *restrict freed, int * restrict bodyalloc, int *restrict bodyfreed);

#ifdef FS2TEST
/**
 * @brief Reset all internal alloc/free counters to zero.
 *
 * Test-only. Must be called between test cases to get clean leak
 * reporting from fs2_checkleak.
 */
void   __fs2_checkreset(void);
#endif /* FS2TEST */

#endif /* !FS2_H */
