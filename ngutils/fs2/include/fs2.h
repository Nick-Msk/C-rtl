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
    FS2_STATIC    = 0x02,   /**< v is external, read-only; not owned, not freed, not growable. */
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
 * Comparators
 * ───────────────────────────────────────────────────────────────────────── */
typedef int (*fs2_cmp_t)(const char *a, const char *b);

/* ─────────────────────────────────────────────────────────────────────────
 * Value macros
 * ───────────────────────────────────────────────────────────────────────── */

/** Empty growable value (stack body, ALLOC, no capacity yet). */
#define FS2()          (fs2){ .len = 0, .sz = 0, \
                                .flags = FS2_ALLOC,  .v = NULL }

/** Empty read-only value. Safe to share; fs2_free is a no-op. */
#define FS2EMPTY        (fs2){ .len = 0, .sz = 1, \
                                .flags = FS2_STATIC, .v = (char *)"" }

/** Literal: wraps a compile-time string. sizeof-1 gives strlen for free. */
#define FS2LITERAL(s)   (fs2){ .len = sizeof(s) - 1, .sz = sizeof(s), \
                                .flags = FS2_STATIC, .v = (char *)(s) }                      

/* ─────────────────────────────────────────────────────────────────────────
 * Getters (all const-safe, no allocation)
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Logical length of the string (excluding the trailing NUL).
 *
 * @param s  Non-NULL pointer to an fs2.
 * @return   Number of significant bytes.
 */
static inline size_t      
fs2_len (const fs2 *s) { return s->len; }

/**
 * @brief Total buffer capacity in bytes (including room for NUL).
 *
 * @param s  Non-NULL pointer to an fs2.
 * @return   Allocated capacity.
 */
static inline size_t      
fs2_sz  (const fs2 *s) { return s->sz;  }

/**
 * @brief Mutable pointer to the underlying NUL-terminated buffer.
 *
 * Use for in-place mutation. Only valid when the buffer is writable
 * (FS2_ALLOC or FS2LOCAL). UB to write to a FS2_STATIC buffer.
 *
 * @param s  Non-NULL pointer to an fs2.
 * @return   `char*` to the buffer; `NULL` if the buffer has been released.
 */
static inline char       *
fs2_str (fs2 *s)       { return s->v;   }

/**
 * @brief Const pointer to the underlying NUL-terminated buffer.
 *
 * Safe for all string kinds (STATIC / LOCAL / ALLOC).
 *
 * @param s  Non-NULL pointer to an fs2.
 * @return   `const char*` to the buffer; `NULL` if the buffer has been released.
 */
static inline const char *
fs2_cstr(const fs2 *s) { return s->v;   }

/* ─────────────────────────────────────────────────────────────────────────
 * Predicates
 * ───────────────────────────────────────────────────────────────────────── */

/** @brief Check if the buffer is an external read-only literal (FS2_STATIC). */
static inline bool          
fs2_is_static   (const fs2 *s) { return (s->flags & FS2_STATIC)    != 0; }

/** @brief Check if the buffer is a caller-owned fixed-size buffer (FS2LOCAL). */
static inline bool          
fs2_is_local    (const fs2 *s) { return (s->flags & FS2_LOCAL)     != 0; }

/** @brief Check if the buffer is heap-allocated and growable (FS2_ALLOC). */
static inline bool          
fs2_is_alloc    (const fs2 *s) { return (s->flags & FS2_ALLOC)     != 0; }

/** @brief Check if the fs2 struct itself is heap-allocated (FS2_BODYALLOC). */
static inline bool          
fs2_is_bodyalloc(const fs2 *s) { return (s->flags & FS2_BODYALLOC) != 0; }

/** @brief Logical emptiness: len == 0. */
static inline bool          
fs2_is_empty(const fs2 *s) { 
    return s == NULL || s->len == 0;
}

/**
 * @brief Null-check: either @p s itself is NULL or its buffer is NULL.
 *
 * Defunct values have @c v == NULL and therefore also report as null.
 */
static inline bool          
fs2_is_null(const fs2 *s) { 
    return s == NULL || s->v == NULL; 
}

/**
 * @brief Skip-or-raise guard for in-place mutators.
 *
 * @return true  — empty (NULL or len==0), caller should return `s` untouched.
 *         false — mutable, caller may proceed.
 * @raise       — non-empty STATIC (caller tries to mutate .rodata).
 */
static inline bool
fs2_cant_mutate(const fs2 *s) {
    if (fs2_is_empty(s))
        return true;
    if (fs2_is_static(s))
        return userraise(0, ERR_UNSUPPORTED_TYPE, "STATIC fs2 violation!");
    return false;
}

/**
 * @brief Guard for grow/write functions (cat, catstr, cpy, cpystr, ...).
 *
 * @return true — empty or mutable, caller may proceed (empty auto-promotes via reserve).
 *         userraise — non-empty STATIC (cannot write into .rodata while preserving content).
 */
static inline bool
fs2_cant_growth(fs2 *s) {
    if (!s)
        return userraise(0, ERR_NULL_INPUT, 
                  "Cannot grouth for null destination!");
    if (fs2_is_empty(s))
        return false;               /* empty → OK, will grow as FSEMPTY */
    if (fs2_is_static(s))
        return userraise(0, ERR_UNSUPPORTED_TYPE, 
                  "Cannot grouth of non-empty STATIC fs2");
    return false;                                    /* ALLOC/LOCAL → proceed */
}

/**
 * @brief Precondition guard for memcpy/memmove-style operations.
 *
 * Trivial case (len == 0): no src read, no dst write — returns true
 * even for NULL dst/src or STATIC dst ("фиктивная операция").
 *
 * Non-trivial: dst and src must be non-NULL; src[srcpos..srcpos+len)
 * must fit within src->len; dst must be writable (empty auto-promotes,
 * non-empty STATIC raises).
 *
 * @return true  — caller should return dst unchanged (len == 0).
 *         false — caller may proceed with the copy.
 * @raise userraise — NULL dst/src, non-empty STATIC dst.
 * @raise sysraise  — src over-read.
 */
static inline bool
fs2_cant_copy(fs2 *dst, size_t dstpos, const fs2 *src, size_t srcpos, size_t len) {
    (void) dstpos;
    /* Trivial: nothing copied → no-op, no validation needed */
    if (len == 0)
        return true;
    
    /* NULL pointers — hard error for non-trivial copy */
    if (!dst)
        userraise(0, ERR_NULL_INPUT, "Null dst pointer");
    if (!src)
        userraise(0, ERR_NULL_INPUT, "Null src pointer");
    
    /* Over-read: src[srcpos .. srcpos+len) must fit within src */
    if (srcpos > src->len || len > src->len - srcpos)
        sysraise(ERR_OUT_OF_RANGE,
                 "src[%zu..%zu] exceeds src->len=%zu",
                 srcpos, srcpos + len, src->len);
    
    /* dst must be writable — empty auto-promotes via reserve,
     * non-empty STATIC cannot (memcpy into .rodata) */
    if (!fs2_is_empty(dst) && fs2_is_static(dst))
        userraise(0, ERR_UNSUPPORTED_TYPE,
                  "Cannot write into non-empty STATIC fs2");
    
    return false;
}

/**
 * @brief Declare a stack-allocated fixed-capacity fs2 (FS2LOCAL).
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
 * @brief Ensure capacity for at least @p need bytes (including NUL).
 *
 * No-op for STATIC/LOCAL (cannot grow external buffers).
 *
 * @param s     Non-NULL fs2.
 * @param need  Desired minimum capacity.
 *
 * @warning     `invraise` if @p s is NULL.
 */
extern fs2 *       
fs2_reserve(fs2 *s, size_t need);

/**
 * @brief Ensure dst can hold @p len bytes at offset @p pos, plus NUL.
 *
 * Safe against pos + len + 1 overflow. Use this from memcpy/memmove
 * wrappers and any API that writes at a known offset.
 *
 * @raise ERR_OUT_OF_RANGE if pos + len + 1 overflows size_t.
 */
static inline fs2 *
fs2_reserve_span(fs2 *s, size_t pos, size_t len) {
    if (pos >= SIZE_MAX - len)
        sysraise(ERR_OUT_OF_RANGE, 
                 "fs2_reserve_span: pos=%zu + len=%zu + 1 overflows", 
                 pos, len);
    return fs2_reserve(s, pos + len + 1);
}

/**
 * @brief Set capacity to exactly @p need bytes (no rounding).
 *
 * Unlike fs2_reserve (which rounds up to the next power of two and
 * floors at FS2_MIN_ALLOC = 16), fs2_setsize performs an exact
 * realloc. Useful when the caller knows the final size and wants to
 * avoid over-allocation, or wants to shrink a buffer after a temporary
 * spike.
 *
 * ALLOC buffers are reallocated, including when shrinking. STATIC and
 * LOCAL are no-ops (cannot resize external storage).
 *
 * @param s     Non-NULL fs2. Resize only happens when fs2_is_alloc(s).
 * @param need  Exact target capacity in bytes.
 *
 * @pre  need >= s->len + 1
 *       The string data must fit. Passing a smaller value truncates
 *       the buffer while @c s->len stays unchanged — the next
 *       @c strlen(s->v) or @c fs2_validate will read past the end
 *       of the allocation (heap over-read). This is a hard caller
 *       error, not checked here.
 *
 * @pre  need > 0
 *       Implied by the above (len >= 0 ⇒ need >= 1). @c realloc(v, 0)
 *       is implementation-defined and may free @c v, leaving
 *       @c s->v == NULL with @c len > 0 (corrupt state).
 *
 * @note realloc() may internally allocate more than @p need for
 *       alignment or allocator bookkeeping (typically up to the next
 *       malloc bucket size). @c s->sz always reflects exactly
 *       @p need — the allocator's extra bytes are not visible to
 *       fs2 and are not usable by the caller.
 *
 * @note When @p need < s->sz, this is a shrink — the same allocation
 *       is returned to the allocator, but the internal block may not
 *       move (implementation-defined by realloc). Use it to release
 *       memory after a large temporary spike.
 *
 * @note When @p need == s->sz, realloc is still called (see the
 *       `!init || newsz > s->sz` condition in increasesize). Harmless
 *       but wasteful; callers can skip the call themselves if
 *       @c fs2_sz(s) == need.
 *
 * @warning No precondition checking. Truncation, `need == 0`, and
 *          `need < s->len + 1` are UB / corrupt state. Use
 *          pfs2_setsize (planned) for the guarded variant.
 *
 * @see fs2_reserve
 */
extern fs2 * 
fs2_setsize(fs2 *s, size_t need);

extern fs2 *      
fs_shrink(fs2 *s);

// internal, clamps from/len to valid range
static inline void 
fs2_substr_bounds(const fs2 *s, size_t *from, size_t *len) {
    // Если буфер не выделен или начало за пределами строки — возвращаем пустоту
    if (!s->v || *from >= s->len) {
        *from = s->len;
        if (len)
            *len  = 0;
        return;
    }
    // Защита от underflow: *len не может быть больше, чем осталось до конца
    if (len && *len > s->len - *from)
        *len = s->len - *from;
}

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
 * @brief Wrap a C-string as an fs2 view (STATIC, zero-copy).
 *
 * @param str  NUL-terminated string, or NULL.
 *             NULL is treated as the empty string.
 * @return     fs2 view aliasing @p str (or the empty view if NULL).
 *             @p str must outlive every use of the result.
 * @note       No allocation. `fs2_free` on the result is a no-op.
 *             Do not pass the result as both `dst` and `src` to fs2_cat
 *             (restrict violation).
 */
static inline fs2 
fs2_wrap(const char *str) {
    if (!str)
        return FS2EMPTY;
    size_t  len = strlen(str), sz = len + 1;
    fs2 res = (fs2) {.len = len, .sz = sz, .v = (char *) str, .flags = FS2_STATIC};
    return res;
}

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
static inline fs2
fs2_init(size_t sz) {
    fs2         res = FS2();     // FS2_ALLOC flag
    fs2_reserve(&res, sz ? sz: 1);
    return res;
}

/**
 * @brief Create a growable fs2 as a deep copy of a C string.
 *
 * @param src  NUL-terminated string, or NULL (treated as "" → FS2EMPTY).
 * @return     fs2 value (ALLOC) with a heap copy of @p src.
 * @note NULL is not an error — consistent with fs2_wrap.
 */
fs2         fs2_fromstr(const char *src);

/**
 * @brief Create a growable fs2 via printf-style formatting.
 *
 * @param fmt  Format string (must not be NULL).
 * @param ...  Format arguments.
 * @return     fs2 value (ALLOC) containing the formatted result.
 * 
 */
fs2         fs2_fromstrf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

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
fs2         fs2_clone   (const fs2 *src);

/**
 * @brief Steal the buffer from @p src into a new stack fs2.
 *
 * After the call, @p src is in the canonical empty state (FS2(),
 * preserving its BODYALLOC if it was heap-allocated). Works for
 * ALLOC and STATIC sources. **Forbidden for LOCAL** (invraise).
 *
 * @param src  Non-NULL source fs2 (ALLOC or STATIC).
 * @return     fs2 value that now owns the former src buffer.
 * @warning    `invraise` if @p src is NULL or FS2LOCAL.
 */
fs2         fs2_movefrom(fs2 *src);

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
fs2         fs2_movefromstr(const char **from, fs2_flags fl);

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
fs2         fs2_adoptn(char **str, size_t saved_len);

/**
 * @brief Adopt a heap-allocated C string (length computed via strlen).
 *
 * Convenience wrapper around fs2_adoptn. If `str` or `*str` is NULL,
 * returns FS2().
 *
 * @param str  Pointer to the heap char* to adopt; set to NULL on success.
 * @return     fs2 value (ALLOC) wrapping the adopted buffer.
 */
fs2         fs2_adopt(char **str);

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
fs2        *fs2_heap          (void);

/**
 * @brief Allocate a growable fs2 on the heap with given capacity.
 *
 * Equivalent to fs2_init but the struct itself is heap-allocated.
 *
 * @param sz  Initial buffer capacity (including NUL slot).
 * @return    Heap-allocated fs2* (BODYALLOC | ALLOC), or raises on OOM.
 */
fs2        *fs2_heap_init     (size_t sz);

/**
 * @brief Heap-allocate an fs2 as a deep copy of a C string.
 *
 * @param src  Non-NULL NUL-terminated source.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC) with a heap copy.
 */
fs2        *fs2_heap_fromstr  (const char *src);

/**
 * @brief Heap-allocate an fs2 via printf-style formatting.
 *
 * @param fmt  Format string (must not be NULL).
 * @param ...  Format arguments.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC).
 */
fs2        *fs2_heap_fromstrf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Heap-allocate an fs2 as a deep copy of another fs2.
 *
 * The result is always BODYALLOC | ALLOC regardless of source kind.
 *
 * @param src  Non-NULL source fs2.
 * @return     Heap-allocated fs2* (BODYALLOC | ALLOC) with a heap copy.
 */
fs2        *fs2_heap_clone    (const fs2 *src);

/**
 * @brief Deprecated: heap-allocate an fs2 by stealing from @p src.
 *
 * Always raises ERR_NOT_IMPLEMENTED. Use fs2_stacktoheap instead.
 *
 * @param src  Source fs2 (unused).
 * @deprecated Use fs2_stacktoheap.
 */
fs2        *fs2_heap_movefrom (fs2 *src);

/**
 * @brief Heap-allocate an fs2 by stealing a C string pointer.
 *
 * Wraps fs2_movefromstr into a heap body. `*from` is set to NULL.
 *
 * @param from  Pointer to the char* to steal; set to NULL.
 * @param fl    FS2_STATIC or FS2_ALLOC.
 * @return      Heap-allocated fs2* (BODYALLOC + given flag).
 */
fs2        *fs2_heap_movefromstr(const char **from, fs2_flags fl);

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
 * @warning     `invraise` if either pointer is NULL or either side is FS2LOCAL.
 */
fs2         *fs2_move(fs2 *to, fs2 *from);

// internal, memcpy wrapper
// TODO: generally all API have to use that instead of memcpy call
static inline fs2 *
fs2_memcpy(fs2 *restrict dst, size_t dstpos, const fs2 *restrict src, size_t srcpos, size_t len) {
    if (fs2_cant_copy(dst, dstpos, src, srcpos, len))
        return dst;

    fs2_reserve_span(dst, dstpos, len);

    memcpy(dst->v + dstpos, src->v + srcpos, len);
    return dst;
}

// internal, memcpy wrapper
// TODO: generally all API have to use that instead of memcpy call
static inline fs2 *
fs2_memcpy_str(fs2 *restrict dst, size_t dstpos, const char *restrict src, size_t len) {
    // NOTE: that can be fs2_wrap, but it launch strlen, we don't need that
    fs2 tmp = (fs2) { .v = (char *) src, .flags = FS2_STATIC, .len = len, .sz = len + 1 };

    return fs2_memcpy(dst, dstpos, &tmp, 0, len);
}


// internal, memmove wrapper
// TODO: generally all API have to use that instead of memmove call
static inline fs2 *
fs2_memmove(fs2 *dst, size_t dstpos, const fs2 *src, size_t srcpos, size_t len) {

    if (src == NULL) 
        src = dst;   /* self-move: resolve before guard */
    
    if (fs2_cant_copy(dst, dstpos, src, srcpos, len))
        return dst;
    
    fs2_reserve_span(dst, dstpos, len);

    memmove(dst->v + dstpos, src->v + srcpos, len);
    return dst;
}

static inline fs2 *
fs2_selfmove(fs2 *dst, size_t to, size_t from, size_t len) {
    return fs2_memmove(dst, to, NULL, from, len);
}

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
 *              or if the source is FS2LOCAL.
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
 * If @p s has no buffer (`s->v == NULL`, canonical empty), @p pos must be 0.
 *
 * @param s    Non-NULL fs2.
 * @param pos  New logical length.
 * @return     @p s for chaining.
 */
static inline fs2 *fs2_setlen(fs2 *s, size_t pos) {
    invraise(s != NULL, "Null pointer");

    if (s->v && !fs2_is_static(s))
        s->v[s->len = pos] = '\0';  // not for static! ex. FS2EMPTY
    else {
        if (pos > 0)
            userraise(NULL, ERR_ACTION_NOT_APPLICABLE, 
                "Trying to set %zu on non-initialized fs2", pos);
        else
            s->len = 0; // v == NULL, только pos == 0 допустим
    }
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
 * Comparators
 * ───────────────────────────────────────────────────────────────────────── */

// unlim, bare level
static inline int            
fs2_cmp(const fs2 *restrict str1, const fs2 *restrict str2) {
    invraise(str1 != NULL && str1->v != NULL && str2 != NULL && str2->v != NULL, 
        "Null pointers %p %p", str1, str2);

    return strcmp(str1->v, str2->v);
}
// limited
static inline int            
fs2_ncmp(const fs2 *restrict str1, const fs2 *restrict str2, size_t len) {
    invraise(str1 != NULL && str1->v != NULL && str2 != NULL && str2->v != NULL, 
        "Null pointers %p %p", str1, str2);

    return strncmp(str1->v, str2->v, len);
}
// unlim, insensitive
static inline int            
fs2_cmp_ci(const fs2* restrict str1, const fs2* restrict str2) {
    invraise(str1 != NULL && str1->v != NULL && str2 != NULL && str2->v != NULL, 
        "Null pointers %p %p", str1, str2);

    return strcasecmp(str1->v, str2->v);
}
// limited, insensitive, limited
static inline int            
fs2_ncmp_ci(const fs2* restrict str1, const fs2* restrict str2, size_t len) {
    invraise(str1 != NULL && str1->v != NULL && str2 != NULL && str2->v != NULL, 
        "Null pointers %p %p", str1, str2);

    return strncasecmp(str1->v, str2->v, len);
}
// c-str wrappers
// unlim, c-str wrap
static inline int            
fs2_cmpstr(const fs2 *restrict str1, const char *restrict str2) {
    invraise(str1 != NULL && str1->v != NULL && str2 != NULL, 
        "Null pointers %p %p", str1, str2);

    return strcmp(str1->v, str2);
}
// limited, c-str wrap
static inline int            
fs2_ncmpstr(const fs2 *restrict str1, const char *restrict str2, size_t len) {
    invraise(str1 != NULL && str1->v != NULL && str2 != NULL, 
        "Null pointers %p %p", str1, str2);

    return strncmp(str1->v, str2, len);
}
// unlim, insensitive, c-str wrap
static inline int            
fs2_cmp_ci_str(const fs2* restrict str1, const char *restrict str2) {
    invraise(str1 != NULL && str1->v != NULL && str2 != NULL, 
        "Null pointers %p %p", str1, str2);

    return strcasecmp(str1->v, str2);
}
// limited, insensitive, c-str wrap
static inline int            
fs2_ncmp_ci_str(const fs2* restrict str1, const char *restrict str2, size_t len) {
    invraise(str1 != NULL && str1->v != NULL && str2 != NULL, 
        "Null pointers %p %p", str1, str2);

    return strncasecmp(str1->v, str2, len);
}
// ------------------ const void *, const void * wrappers -------------------
extern int                  fs2_cmp_wrap(const void *str1, const void *str2);

extern int                  fs2_cmp_wrap_ci(const void *str1, const void *str2);

extern int                  fs2_cmpstr_wrap(const void *str1, const void *str2);

extern int                  fs2_cmpstr_wrap_ci(const void *str1, const void *str2);

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
long                fs_techfprint(FILE *restrict out, const fs2 *restrict s, const char *restrict name);

#define  FSTECHFPRINT(out, s) \
        fs_techfprint( (out), (s), #s)

#define  FSTECHPRINT(s) \
        fs_techfprint(stdout, (s), #s)        

#define  FSTECHLOG(s) \
        fs_techfprint(logfile, (s), #s)        

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
bool                fs2_validate(FILE *restrict out, const fs2 *restrict s);

/**
 * @brief Compare two fs2 objects for technical equality.
 *
 * Unlike logical comparison (strcmp-style), this also checks flags:
 * FS2_ALLOC vs FS2_STATIC vs FS2LOCAL, and BODYALLOC. Two fs2 that
 * print the same content but have different ownership are considered
 * different.
 *
 * @return true if len, flags and content are identical; false otherwise.
 * @note invraise(SIGABRT) if either pointer is NULL.
 */
bool                fs2_techcmp(const fs2 *s1, const fs2 *s2);

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
bool                fs2_checkleak(int *restrict alloc, int *restrict freed, int * restrict bodyalloc, int *restrict bodyfreed);

#ifdef FS2TEST
/**
 * @brief Reset all internal alloc/free counters to zero.
 *
 * Test-only. Must be called between test cases to get clean leak
 * reporting from fs2_checkleak.
 */
void        __fs2_checkreset(void);
#endif /* FS2TEST */

#endif /* !FS2_H */
