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
 *   DEFUNCT  — moved out; only free / assignment is valid
 *
 * Preconditions (NOT checked here — see cfs2.h for a guarded wrapper):
 *   - No NULL pointers where a valid fs2 is expected.
 *   - len < sz (except DEFUNCT, where all zero).
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

extern const char           *fs2_version(void);
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
    char       *v;          /**< NUL-terminated; NULL only when DEFUNCT.    */
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

static inline size_t      fs2_len (const fs2 *s) { return s->len; }
static inline size_t      fs2_sz  (const fs2 *s) { return s->sz;  }
static inline char       *fs2_str (fs2 *s)       { return s->v;   }
static inline const char *fs2_cstr(const fs2 *s) { return s->v;   }

/* ─────────────────────────────────────────────────────────────────────────
 * Predicates
 * ───────────────────────────────────────────────────────────────────────── */

static inline bool fs2_is_static   (const fs2 *s) { return (s->flags & FS2_STATIC)    != 0; }
static inline bool fs2_is_local    (const fs2 *s) { return (s->flags & FS2_LOCAL)     != 0; }
static inline bool fs2_is_alloc    (const fs2 *s) { return (s->flags & FS2_ALLOC)     != 0; }
static inline bool fs2_is_bodyalloc(const fs2 *s) { return (s->flags & FS2_BODYALLOC) != 0; }

/** @brief Logical emptiness: len == 0. */
static inline bool fs2_is_empty(const fs2 *s) { 
    invraise(s != NULL, "Null pointers %p %p", s, s ? s->v : NULL);

    return s->len == 0 || *s->v == '\0'; 
}

/**
 * @brief Null-check: either @p s itself is NULL or its buffer is NULL.
 *
 * Defunct values have @c v == NULL and therefore also report as null.
 */
static inline bool fs2_is_null(const fs2 *s) { return s == NULL || s->v == NULL; }

#define FS2LOCAL(name, n)                          \
    char _fs2_buf_##name[(n)];                     \
    _fs2_buf_##name[0] = '\0';                     \
    fs2 name = (fs2){ .len = 0, .sz = (n),         \
                      .flags = FS2_LOCAL,          \
                      .v = _fs2_buf_##name }

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

/* ─────────────────────────────────────────────────────────────────────────
 * Constructors — value (stack body)
 *
 *   fs2_init(sz)         growable, capacity sz
 *   fs2_fromstr(s)       copy of a C-string
 *   fs2_fromstrf(f, ...) printf-formatted copy
 *   fs2_clone(src)       deep copy of any fs2
 *   fs2_movefrom(src)    steal from a stack fs2 (src becomes DEFUNCT)
 *   fs2_movefromstr(p)   steal a heap C-string (p becomes NULL)
 * ───────────────────────────────────────────────────────────────────────── */
 
fs2   fs2_init    (size_t sz);
fs2   fs2_fromstr (const char *src);
fs2   fs2_fromstrf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
fs2   fs2_clone   (const fs2 *src);
fs2   fs2_movefrom(fs2 *src);
fs2   fs2_movefromstr(const char **from, fs2_flags fl);

fs2   fs2_adoptn(char **str, size_t saved_len);
fs2   fs2_adopt(char **str);

/* ─────────────────────────────────────────────────────────────────────────
 * Constructors — heap (BODYALLOC set)
 *
 * Same operations, but the fs2 struct itself is allocated. Free with
 * fs2_free (which frees the buffer, the struct, or both, per flags).
 * ───────────────────────────────────────────────────────────────────────── */

fs2     *fs2_heap          (void);
fs2     *fs2_heap_init     (size_t sz);
fs2     *fs2_heap_fromstr  (const char *src);
fs2     *fs2_heap_fromstrf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
fs2     *fs2_heap_clone    (const fs2 *src);
fs2     *fs2_heap_movefrom (fs2 *src);
fs2     *fs2_heap_movefromstr(const char **from);

/* ─────────────────────────────────────────────────────────────────────────
 * Move assignment (not a constructor)
 *
 *   to ← from, then from → empty. Frees whatever `to` held before.
 * ───────────────────────────────────────────────────────────────────────── */
 
fs2     *fs2_move(fs2 *to, fs2 *from);

// stack <-> heap convertes
fs2      fs2_heaptostack(fs2 **psrc);
fs2     *fs2_stacktoheap(fs2 *src);

/* ─────────────────────────────────────────────────────────────────────────
 * Destructor
 *
 * Frees the buffer if ALLOC, frees the struct if BODYALLOC. No-op on
 * STATIC, LOCAL, DEFUNCT. Idempotent for value (non-BODYALLOC) cases.
 * ───────────────────────────────────────────────────────────────────────── */

void    fs2_free(fs2 *s);

/* ─────────────────────────────────────────────────────────────────────────
 * Unilities
 * ───────────────────────────────────────────────────────────────────────── */
static inline fs2 *fs2_setlen(fs2 *s, size_t pos) {
    s->v[s->len = pos] = '\0';
    return s;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Tech printer
 * ───────────────────────────────────────────────────────────────────────── */
long    fs_techfprint(FILE *restrict out, const fs2 *restrict s, const char *restrict name);

 /* ─────────────────────────────────────────────────────────────────────────
 * Validator
 * ───────────────────────────────────────────────────────────────────────── */
bool    fs2_validate(FILE *restrict out, const fs2 *restrict s);

/* ─────────────────────────────────────────────────────────────────────────
 * Leak checking
 * ───────────────────────────────────────────────────────────────────────── */

bool   fs2_checkleak(int *restrict alloc, int *restrict freed, int * restrict bodyalloc, int *restrict bodyfreed);

#ifdef FS2TEST
// only for the test purpose
void   __fs2_checkreset(void);
#endif /* FS2TEST */

#endif /* !FS2_H */
