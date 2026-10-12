/**
 * @file common.h
 * @brief Small cross-utility helpers.
 *
 * Contains only the symbols required by fs.h / fs.c. The original
 * monolithic common.h had 600+ lines of unrelated utilities; this
 * version is deliberately minimal and grows on demand.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <ctype.h>
#include <stdlib.h>


#define COMMON_VERSION_MAJOR 0
#define COMMON_VERSION_MINOR 1
#define COMMON_VERSION_PATCH 0

#define COMMON_STRINGIFY_(x) #x
#define COMMON_STRINGIFY(x)  COMMON_STRINGIFY_(x)

/**
 * @brief Current version as a string literal, e.g. @c "0.1.0" .
 */
#define COMMON_VERSION \
    COMMON_STRINGIFY(COMMON_VERSION_MAJOR) "." \
    COMMON_STRINGIFY(COMMON_VERSION_MINOR) "." \
    COMMON_STRINGIFY(COMMON_VERSION_PATCH)

/**
 * @brief Current version string, e.g. @c "0.1.0" .
 *
 * Equivalent to @c *common_versions() .
 *
 * @return Pointer to a static string literal; do not free.
 */
extern const char *common_version(void);

/**
 * @brief Published versions, newest first, terminated by @c NULL .
 *
 * @c versions[0] is always the current version. Array and elements are
 * read-only; do not free.
 *
 * @return Pointer to a static, NULL-terminated array of strings.
 */
extern const char *const *common_versions(void);


/* -------------------------------------------------------------------------
 * Generic macros
 * ------------------------------------------------------------------------- */

/**
 * @brief Return the smaller of two values.
 *
 * Both operands are evaluated once. Types must be comparable.
 */
#define MIN(x, y)  ({ typeof(x) _x = (x); typeof(y) _y = (y); \
                      _x < _y ? _x : _y; })

/**
 * @brief Return the larger of two values.
 */
#define MAX(x, y)  ({ typeof(x) _x = (x); typeof(y) _y = (y); \
                      _x > _y ? _x : _y; })

/**
 * @brief Switch-case helper: stringify the enumerator.
 *
 * @code
 *   switch (flag) {
 *       CASE_RETURN(FS_FLAG_STATIC);
 *       CASE_RETURN(FS_FLAG_ALLOC);
 *       default: return "Unknown";
 *   }
 * @endcode
 */
#define CASE_RETURN(x)  case x: return #x

/* -------------------------------------------------------------------------
 * "Endless loop" sentinel
 *
 * Used by fs_fprint_arr / fs_fsave_arr to cap iteration over a
 * NULL-terminated array of structures that are not pointers (and so
 * cannot be NULL themselves).
 * ------------------------------------------------------------------------- */

static const int G_GLOB_AVERAGE = INT_MAX;

/* -------------------------------------------------------------------------
 * Size growth strategy
 * ------------------------------------------------------------------------- */

/**
 * @brief Allocation growth policy used by containers.
 */
typedef enum {
    SIZE_NONE    = 0,   /**< Use the requested size as-is. */
    SIZE_POWER2,        /**< Round up to the next power of two. */
    SIZE_MIN10          /**< Enforce a floor of 10. */
} Tincrease;

/**
 * @brief Smallest power of two strictly greater than @p val.
 * @return 0 if @p val is 0.
 */
static inline unsigned long
round_up_2(unsigned long val)
{
    unsigned long prev = 0;
    while (val)
        val &= ((prev = val) - 1);
    return prev << 1;
}

/**
 * @brief Compute the target allocation size for a given growth policy.
 *
 * @param t  Growth strategy.
 * @param n  Requested minimum size.
 * @return   Target size, or -1 for an unknown strategy.
 */
long calcnewsize(Tincrease t, long n);

/* -------------------------------------------------------------------------
 * Character helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Conditionally lowercase a character.
 * @param lower  When true, apply @c tolower ; otherwise return @p c .
 */
static inline int clower(int c, bool lower) { return lower ? tolower(c) : c; }

/**
 * @brief Conditionally uppercase a character.
 */
static inline int cupper(int c, bool upper) { return upper ? toupper(c) : c; }

/* -------------------------------------------------------------------------
 * Random helpers
 *
 * The caller is responsible for calling srand() before use. These
 * helpers cover the ranges used in test-data generation.
 * ------------------------------------------------------------------------- */

/** @brief Uniform random int in @c [0, max] (inclusive). */
static inline int
rndint(int max)
{
    if (max <= 0)
        return 0;
    int range = max + 1;
    int limit = RAND_MAX - (RAND_MAX % range);
    int r;
    do { r = rand(); } while (r >= limit);
    return r % range;
}

/** @brief Random lowercase letter. */
static inline char rndlowchar(void)   { return (char)('a' + rndint(25)); }

/** @brief Random uppercase letter. */
static inline char rndupperchar(void) { return (char)('A' + rndint(25)); }

/** @brief Random decimal digit. */
static inline char rnddigitchar(void) { return (char)('0' + rndint(9));  }

/* -------------------------------------------------------------------------
 * C-string helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Reverse @p len characters of @p s in place.
 * @return @p s (for chaining).
 */
char *reverse(char *s, int len);

/* -------------------------------------------------------------------------
 * String-to-value parsing
 *
 * Each function returns true on success, false on any of:
 *   - NULL input/output pointer,
 *   - empty string,
 *   - trailing garbage after the number,
 *   - overflow / underflow.
 * On failure the output is untouched.
 * ------------------------------------------------------------------------- */

bool try_parse_int   (const char *restrict str, int          *restrict res);
bool try_parse_long  (const char *restrict str, long         *restrict res);
bool try_parse_ulong (const char *restrict str, unsigned long*restrict res);
bool try_parse_double(const char *restrict str, double       *restrict res);

/* -------------------------------------------------------------------------
 * Comparators for qsort / bsearch (byte-wise)
 * ------------------------------------------------------------------------- */

/** @brief Ascending byte comparator. */
int pchar_cmp   (const void *restrict s1, const void *restrict s2);

/** @brief Descending byte comparator. */
int pchar_revcmp(const void *restrict s1, const void *restrict s2);

#endif /* !COMMON_H */
