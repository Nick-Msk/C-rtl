/**
 * @file fs2_api.h
 * @brief Higher-level operations on fs2: element access, reverse/sort,
 *        single-symbol search (forward/reverse, sensitive/insensitive,
 *        limited/unlimited, Nth-occurrence), sprintf.
 *
 * This is the **unprotected** layer — no runtime bounds checking.
 * `invraise` is a debug-only macro (compiled out in release).
 * For checked/guarded access, use the pfs2 wrapper.
 *
 * All search functions return `ptrdiff_t`: 0-based offset, or -1 if not found.
 */

#ifndef FS2_API_H
#define FS2_API_H

#include <ctype.h>

#include "fs2.h"

// -------------------------------- Internal helpers --------------------------------------

/**
 * @brief Forward search for byte @p c in @p avail bytes (internal, case-sensitive).
 *
 * Uses `memchr` — hardware-accelerated on most platforms.
 *
 * @param p      Buffer to search.
 * @param avail  Number of bytes to examine (0 → always -1).
 * @param c      Byte to find.
 * @return       0-based offset of first occurrence, or -1.
 */
static inline ptrdiff_t
__fs2_chr_raw(const unsigned char *p, size_t avail, int c) {
    const unsigned char *hit = memchr(p, (unsigned char)c, avail);
    return hit ? (ptrdiff_t)(hit - p) : -1;
}

/**
 * @brief Forward case-insensitive search for byte @p c (internal).
 *
 * Byte-wise `tolower` comparison — NOT Unicode-aware.
 *
 * @param p      Buffer to search.
 * @param avail  Number of bytes to examine.
 * @param c      Byte to find (compared after tolower on both sides).
 * @return       0-based offset of first match, or -1.
 */
static inline ptrdiff_t
__fs2_ichr_raw(const unsigned char *p, size_t avail, int c) {
    const int lc = tolower((unsigned char)c);
    for (size_t i = 0; i < avail; i++)
        if (tolower(p[i]) == lc) 
            return (ptrdiff_t)i;
    return -1;
}

/**
 * @brief Reverse search for byte @p c in the last @p avail bytes (internal, case-sensitive).
 *
 * Scans from position @p avail-1 down to 0.
 *
 * @param p      Buffer.
 * @param avail  Number of trailing bytes to examine.
 * @param c      Byte to find.
 * @return       0-based offset of the last occurrence (within @p avail), or -1.
 */
static inline ptrdiff_t
__fs2_rchr_raw(const unsigned char *p, size_t avail, int c) {
    const unsigned char uc = (unsigned char)c;
    for (ptrdiff_t i = (ptrdiff_t) avail - 1; i >= 0; i--)
        if (p[i] == uc)
            return i;
    return -1;
}

/**
 * @brief Reverse case-insensitive search for byte @p c (internal).
 *
 * @param p      Buffer.
 * @param avail  Number of trailing bytes to examine.
 * @param c      Byte to find (case-insensitive).
 * @return       0-based offset of the last match, or -1.
 */
static inline ptrdiff_t
__fs2_irchr_raw(const unsigned char *p, size_t avail, int c) {
    const int lc = tolower((unsigned char)c);
    for (ptrdiff_t i = (ptrdiff_t) avail - 1; i >= 0; i--)
        if (tolower(p[i]) == lc)
            return i;
    return -1;
}

// --------------------------------------- API ---------------------------------------

/**
 * @brief Direct byte access without modifying len or sz.
 *
 * @pre    @p pos < s->sz. No bounds check (unprotected layer).
 *
 * @param s    Non-NULL fs2.
 * @param pos  0-based position within the buffer.
 * @return     Pointer to the byte at @p pos.
 */
static inline char *
fs2_get(const fs2 *s, size_t pos){
    return s->v + pos;
}

/**
 * @brief Access a byte, auto-allocating if needed; does NOT update len.
 *
 * If @p pos >= s->sz, the buffer is grown via `fs2_reserve`.
 * The logical length (`s->len`) is **not** modified — only capacity.
 *
 * @pre    s is FS2_ALLOC (grow requires heap).
 *
 * @param s    Non-NULL fs2 (ALLOC).
 * @param pos  0-based position.
 * @return     Pointer to the byte at @p pos.
 */
static inline char *
fs2_elem(fs2 *s, size_t pos){
    if (pos >= s->sz){
        fs2_reserve(s, pos + 1);   // len remains the same here! sz is changed
    }
    return fs2_get(s, pos);
}

/**
 * @brief Access a byte, auto-allocating and extending len to include it.
 *
 * If @p pos >= s->len, the buffer is grown (if needed) and len is
 * set to pos+1. Use for building strings one byte at a time.
 *
 * @pre    s is FS2_ALLOC (grow requires heap).
 *
 * @param s    Non-NULL fs2 (ALLOC).
 * @param pos  0-based position.
 * @return     Pointer to the byte at @p pos.
 */
static inline char *
fs2_elem0(fs2 *s, size_t pos){
    if (pos + 1 >= s->sz){
        fs2_reserve(s, pos + 2);   // len remains the same here! sz is changed
    }
    if (s->len <= pos)   // check
        fs2_setlen(s, pos + 1);
    return fs2_get(s, pos);
}

// ------------------------------------ FS Comparators --------------------------------------

// ----------------------------------- EXCNAHGERS -------------------------------------------

/**
 * @brief Reverse the string in place.
 *
 * @pre    s must not be FS2_STATIC.
 *
 * @param str  Non-NULL fs2 (ALLOC or LOCAL).
 * @return     @p str (for chaining).
 */
extern fs2* 
fs2_reverse(fs2 *str);

/**
 * @brief Sort a sub-range [from, to) of the string in place.
 *
 * @pre    from <= to <= s->len; s must not be FS2_STATIC.
 *
 * @param s     Non-NULL fs2 (ALLOC or LOCAL).
 * @param from  Start offset (inclusive).
 * @param to    End offset (exclusive).
 * @param asc   true for ascending, false for descending.
 */
extern void  
fs2_partialsort(fs2 *s, size_t from, size_t to, bool asc);

/**
 * @brief Sort the entire string in place.
 *
 * Convenience wrapper: fs2_partialsort(str, 0, str->len, asc).
 *
 * @param str  Non-NULL fs2 (ALLOC or LOCAL).
 * @param asc  true for ascending, false for descending.
 */
static inline void 
fs2_sort(fs2 *str, bool asc) {
    invraise(str != NULL, "Null pointer");
    
    fs2_partialsort(str, 0, str->len, asc);
}

// ------------------------------------ CAT/CPY/ETC -----------------------------------------

extern fs2 *
fs2_cat(fs2 *restrict dst, const fs2 *restrict src);

static inline fs2 *
fs2_catstr(fs2 *restrict dst, const char *restrict src) {
    fs2 s = fs2_wrap(src);
    return fs2_cat(dst, &s);
}

extern fs2 *
fs2_rev_catstr(fs2 *restrict dst, const char *restrict beginner);

static inline fs2 *
fs2_cpy(fs2 *restrict dst, const fs2* restrict src) {
    dst->len = 0;
    return fs2_cat(dst, src);
}

static inline fs2* 
fs2_cpystr(fs2 *restrict dst, const char *restrict src){
    fs2 l = fs2_wrap(src);
    return fs2_cpy(dst, &l); 
}


// ----------------------------- One Symbol Search ------------------------------------------

/**
 * @brief Forward case-sensitive search for byte @p c in the full string.
 *
 * Equivalent to `strchr(s->v, c) - s->v`, but respects `s->len`
 * (does not rely on NUL termination).
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @return     0-based offset of first occurrence, or -1.
 */
static inline ptrdiff_t
fs2_chr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");

    return __fs2_chr_raw((const unsigned char *) str->v, str->len, c);
}

/**
 * @brief Forward case-insensitive search for byte @p c in the full string.
 *
 * Byte-wise tolower comparison — NOT Unicode-aware.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @return     0-based offset of first match, or -1.
 */
static inline ptrdiff_t
fs2_ichr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");

    return __fs2_ichr_raw((const unsigned char *) str->v, str->len, c);
}

/**
 * @brief Forward case-sensitive search for byte @p c, limited to first @p lim bytes.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param lim  Maximum number of bytes to examine (clamped to s->len).
 * @return     0-based offset of first occurrence within [0, lim), or -1.
 */
static inline ptrdiff_t
fs2_nchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    size_t avail = (lim < str->len) ? lim : str->len;
    return __fs2_chr_raw((const unsigned char *) str->v, avail, c);
}

/**
 * @brief Forward case-insensitive search for byte @p c, limited to first @p lim bytes.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param lim  Maximum number of bytes to examine (clamped to s->len).
 * @return     0-based offset of first match within [0, lim), or -1.
 */
static inline ptrdiff_t
fs2_nichr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    size_t avail = (lim < str->len) ? lim : str->len;
    return __fs2_ichr_raw((const unsigned char *) str->v, avail, c);
}

// ------------------------------------------ ONE SYMBOL REVERSE SEARCH  --------------------------------------------------------

/**
 * @brief Reverse case-sensitive search: last occurrence of @p c in the full string.
 *
 * Equivalent to `strrchr(s->v, c) - s->v`, but respects `s->len`.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @return     0-based offset of the last occurrence, or -1.
 */
static inline ptrdiff_t
fs2_rchr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");

    return __fs2_rchr_raw((const unsigned char *) str->v, str->len, c);
}

/**
 * @brief Reverse case-insensitive search: last occurrence of @p c in the full string.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @return     0-based offset of the last match, or -1.
 */
static inline ptrdiff_t
fs2_irchr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");

    return __fs2_irchr_raw((const unsigned char *)str->v, str->len, c);
}

/**
 * @brief Reverse case-sensitive search, limited to the last @p lim bytes.
 *
 * Searches the window [len-lim, len). The returned offset is relative
 * to the **start** of the string (not the window).
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param lim  Number of trailing bytes to examine (clamped to s->len).
 * @return     0-based offset from string start, or -1.
 */
static inline ptrdiff_t
fs2_nrchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    size_t avail = (lim < str->len) ? lim : str->len;
    size_t off   = str->len - avail;
    ptrdiff_t hit = __fs2_rchr_raw((const unsigned char *)str->v + off, avail, c);
    return hit < 0 ? -1 : hit + (ptrdiff_t)off;
}

/**
 * @brief Reverse case-insensitive search, limited to the last @p lim bytes.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param lim  Number of trailing bytes to examine (clamped to s->len).
 * @return     0-based offset from string start, or -1.
 */
static inline ptrdiff_t 
fs2_nirchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    size_t avail = (lim < str->len) ? lim : str->len;
    size_t off   = str->len - avail;
    ptrdiff_t hit = __fs2_irchr_raw((const unsigned char *)str->v + off, avail, c);
    return hit < 0 ? -1 : hit + (ptrdiff_t)off;
}

// ------------------------------------------ ONE SYMBOL, N occurence SEARCH  --------------------------------------------------------

/**
 * @brief Find the k-th (1-based) forward case-sensitive occurrence of @p c.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param k    1-based occurrence number (0 → -1).
 * @return     0-based offset, or -1 if fewer than k occurrences.
 */
ptrdiff_t   fs2_chr_nth   (const fs2 *str, int c, size_t k);

/**
 * @brief Find the k-th forward case-sensitive occurrence within [0, lim).
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param k    1-based occurrence number (0 → -1).
 * @param lim  Maximum bytes to examine (clamped to s->len).
 * @return     0-based offset, or -1.
 */
ptrdiff_t   fs2_nchr_nth  (const fs2 *str, int c, size_t k, size_t lim);

/**
 * @brief Find the k-th (1-based) forward case-insensitive occurrence of @p c.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based occurrence number (0 → -1).
 * @return     0-based offset, or -1.
 */
ptrdiff_t   fs2_ichr_nth  (const fs2 *str, int c, size_t k);

/**
 * @brief Find the k-th forward case-insensitive occurrence within [0, lim).
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based occurrence number (0 → -1).
 * @param lim  Maximum bytes to examine (clamped to s->len).
 * @return     0-based offset, or -1.
 */
ptrdiff_t   fs2_nichr_nth (const fs2 *str, int c, size_t k, size_t lim);

// ------------------------------------------ ONE SYMBOL, N occurence REVERSE SEARCH  --------------------------------------------------------

/**
 * @brief Find the k-th (1-based) reverse case-sensitive occurrence of @p c.
 *
 * Scans from the end: k=1 is the last occurrence, k=2 the second-to-last, etc.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param k    1-based reverse occurrence number (0 → -1).
 * @return     0-based offset, or -1.
 */
ptrdiff_t   fs2_rchr_nth  (const fs2 *str, int c, size_t k);

/**
 * @brief Find the k-th reverse case-sensitive occurrence within the last @p lim bytes.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param k    1-based reverse occurrence number (0 → -1).
 * @param lim  Number of trailing bytes to examine (clamped to s->len).
 * @return     0-based offset from string start, or -1.
 */
ptrdiff_t   fs2_nrchr_nth (const fs2 *str, int c, size_t k, size_t lim);

/**
 * @brief Find the k-th (1-based) reverse case-insensitive occurrence of @p c.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based reverse occurrence number (0 → -1).
 * @return     0-based offset, or -1.
 */
ptrdiff_t   fs2_irchr_nth (const fs2 *str, int c, size_t k);

/**
 * @brief Find the k-th reverse case-insensitive occurrence within the last @p lim bytes.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based reverse occurrence number (0 → -1).
 * @param lim  Number of trailing bytes to examine (clamped to s->len).
 * @return     0-based offset from string start, or -1.
 */
ptrdiff_t   fs2_nirchr_nth(const fs2 *str, int c, size_t k, size_t lim);

// ------------------------------------------------------------------------------------------

/**
 * @brief Format into an fs2, replacing its current content.
 *
 * The buffer is grown as needed. The logical length is set to the
 * number of characters formatted.
 *
 * @param s    Non-NULL fs2 (ALLOC). Content is overwritten.
 * @param fmt  Format string.
 * @param ...  Format arguments.
 * @return     Number of characters written (excluding NUL), or -1 on error.
 */
extern long           
fs_sprintf(fs2 *restrict s, const char *restrict fmt, ...) 
    __attribute__ (( format (printf, 2, 3) ) );

/**
 * @brief Append formatted text to the current content of an fs2.
 *
 * Unlike fs_sprintf, this concatenates: the existing content is kept
 * and the formatted result is appended at the end.
 *
 * @param s    Non-NULL fs2 (ALLOC). Content is preserved, new text appended.
 * @param fmt  Format string.
 * @param ...  Format arguments.
 * @return     Number of characters appended (excluding NUL), or -1 on error.
 */
extern long           
fs_sprintf_concat(fs2 *restrict s, const char *restrict fmt, ...)
    __attribute__ (( format (printf, 2, 3) ) );



#endif /* !FS2_API_H */
