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
    invraise(dst != NULL, "Null pointer");

    if (dst == src)
        return dst; // self-copy is a no-op
    fs2_clear(dst);
    return fs2_cat(dst, src);
}

static inline fs2* 
fs2_cpystr(fs2 *restrict dst, const char *restrict src){
    fs2 l = fs2_wrap(src);
    return fs2_cpy(dst, &l); 
}

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
 *
 * @warning Do NOT pass pointers into s->v as %s / %.*s arguments:
 *          internal realloc may invalidate them before vsnprintf reads them.
 *          Clone the source first: fs2 tmp = fs2_clone(&s);
 */
static inline long           
fs2_sprintf(fs2 *restrict s, const char *restrict fmt, ...) 
    __attribute__ (( format (printf, 2, 3) ) );

/**
 * @brief Append formatted text to the current content of an fs2.
 *
 * Unlike fs2_sprintf, this concatenates: the existing content is kept
 * and the formatted result is appended at the end.
 *
 * @param s    Non-NULL fs2 (ALLOC). Content is preserved, new text appended.
 * @param fmt  Format string.
 * @param ...  Format arguments.
 * @return     Number of characters appended (excluding NUL), or -1 on error.
 * 
 * @warning Do NOT pass pointers into s->v as %s / %.*s arguments:
 *          internal realloc may invalidate them before vsnprintf reads them.
 *          Clone the source first: fs2 tmp = fs2_clone(&s);
 */
static inline long           
fs2_sprintf_concat(fs2 *restrict s, const char *restrict fmt, ...)
    __attribute__ (( format (printf, 2, 3) ) );

// internal
extern long                  
fs2_vsprintf_position(fs2 *restrict s, size_t pos, const char *restrict fmt, va_list ap);

static inline long           
fs2_sprintf(fs2 *restrict s, const char *restrict fmt, ...) {
    invraise(s != 0 && fmt != 0, "Null pointers %p - %p", s, fmt);

    va_list ap; 
    va_start(ap, fmt);
    long res = fs2_vsprintf_position(s, 0, fmt, ap);
    va_end(ap);

    if (res < 0)
        return logsimpleerr(res, "fs2_vsprintf_position failed");
    
    fs2_setlen(s, res); // fix the length!!!
    return res;
}

static inline long           
fs2_sprintf_concat(fs2 *restrict s, const char *restrict fmt, ...) {
    invraise(s != 0 && fmt != 0, "Null pointers %p - %p", s, fmt);

    va_list ap; 
    va_start(ap, fmt);
    long res = fs2_vsprintf_position(s, s->len, fmt, ap);
    va_end(ap);

    if (res < 0)
        return logsimpleerr(res, "fs2_vsprintf_position failed");

    fs2_setlen(s, s->len + res); // fix the length!!!

    return res;
}

// substring
extern fs2 *
fs2_substr(fs2 *s, size_t from, size_t len);

// note - stacked-constructor
extern fs2 
fs2_substr_new(const fs2 *s, size_t from, size_t len);

// note - heap-constructor
extern fs2 *
fs2_heap_substr(const fs2 *s, size_t from, size_t len);

// left/right (postgres func)
// left
static inline fs2 *
fs2_left(fs2 *s, size_t len) {
    return fs2_substr(s, 0, len);
}
static inline fs2
fs2_left_new(const fs2 *s, size_t len) {
    return fs2_substr_new(s, 0, len);
}
static inline fs2 *
fs2_heap_left(fs2 *s, size_t len) {
    return fs2_heap_substr(s, 0, len);
}
// right
static inline fs2 *
fs2_right(fs2 *s, size_t len) {
    if (!s || len >= s->len)
        return s;

    return fs2_substr(s, s->len - len, len);
}
static inline fs2
fs2_right_new(const fs2 *s, size_t len) {
    if (!s)
        return FS2();
    if (len >= s->len)
        return fs2_clone(s);

    return fs2_substr_new(s, s->len - len, len);
}
static inline fs2 *
fs2_heap_right(const fs2 *s, size_t len) {
    if (!s)
        return fs2_heap();
    if (len >= s->len)
        return fs2_heap_clone(s);

    return fs2_heap_substr(s, s->len - len, len);
}

// ------------------------------- Upper / Lower case ---------------------------------------
// upper
extern fs2 *
fs2_toupper(fs2 *str);

static inline fs2
fs2_toupper_new(const fs2 *str) {
    fs2 res = fs2_clone(str);
    fs2_toupper(&res);
    return res;
}

static inline fs2 *
fs2_heap_toupper(const fs2 *str) {
    return fs2_toupper(fs2_heap_clone(str));
}

//lower
extern fs2 *
fs2_tolower(fs2 *str);

static inline fs2
fs2_tolower_new(const fs2 *str) {
    fs2 res = fs2_clone(str);
    fs2_tolower(&res);
    return res;
}

static inline  fs2 *
fs2_heap_tolower(const fs2 *str) {
    return fs2_tolower(fs2_heap_clone(str));
}

// ------------------------------------- Padding -------------------------------------------
// right
extern fs2 *                    
fs2_rpad(fs2 *restrict str, size_t len, const fs2 *restrict pad);

static inline fs2 *
fs2_rpadstr(fs2 *restrict str, size_t len, const char *restrict padstr) {

    fs2 tmp = fs2_wrap(padstr);
    return fs2_rpad(str, len, &tmp);
}
// stack-constructor
static inline fs2
fs2_rpad_new(const fs2 *restrict str, size_t len, const fs2 *restrict pad) {
    invraise(str != NULL, "Null pointer");

    fs2 res = fs2_clone(str);  
    if (!pad)
        return res;
     
    fs2_rpad(&res, len, pad);
    return res;
}
// heap-constructor
static inline fs2
fs2_rpadstr_new(const fs2 *restrict str, size_t len, const char *restrict padstr) {
    
    fs2 tmp = fs2_wrap(padstr);
    return fs2_rpad_new(str, len, &tmp);
}
// left
extern fs2 *                    
fs2_lpad(fs2 *restrict str, size_t len, const fs2 *restrict pad);

static inline fs2 *
fs2_lpadstr(fs2 *restrict str, size_t len, const char *restrict padstr) {

    fs2 tmp = fs2_wrap(padstr);
    return fs2_lpad(str, len, &tmp);
}
// stack-constructor
static inline fs2
fs2_lpad_new(const fs2 *restrict str, size_t len, const fs2 *restrict pad) {
    invraise(str != NULL, "Null pointer");

    fs2 res = fs2_clone(str);  
    if (!pad)
        return res;
     
    fs2_lpad(&res, len, pad);
    return res;
}
// heap-constructor
static inline fs2
fs2_lpadstr_new(const fs2 *restrict str, size_t len, const char *restrict padstr) {
    
    fs2 tmp = fs2_wrap(padstr);
    return fs2_lpad_new(str, len, &tmp);
}

// --------------------------------- Random string generators ------------------------------
extern fs2 *           
fs2_genrnd(fs2 *s, size_t len, char type);

static inline fs2
fs2_genrnd_new(size_t len, char type) {
    fs2     res = fs2_init(len + 1);
    fs2_genrnd(&res, len, type);
    return res;
}

static inline fs2 *
fs2_heap_genrnd(size_t len, char type) {
    fs2     *res = fs2_heap_init(len + 1);
    fs2_genrnd(res, len, type);
    return res;
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

// ----------------------------------- Finders -----------------------------------

// full scan version
static inline ptrdiff_t
fs2_anyof(const fs2 *restrict s, const char *restrict strs[], fs2_cmp_t cmp) {
    invraise(s != NULL && strs != NULL, "Null pointers %p %p", s, strs);
    
    const char   *sv = s->v ? s->v : "";  // normalization FS2() -> ""
    ptrdiff_t     ind = 0;
    while (*strs) {
        if (cmp(sv, *strs) == 0)
            return ind; // Early exit
        strs++, ind++;
    }
    return -1;
}

static inline bool           
fs2_in(const fs2 *restrict s, const char *restrict strs[]) {
    return fs2_anyof(s, strs, strcmp) >= 0;
}

static inline bool           
fs2_iin(const fs2 *restrict s, const char *restrict strs[]) {
    return fs2_anyof(s, strs, strcasecmp) >= 0;
}

static inline bool           
fs2_not_in(const fs2 *restrict s, const char *restrict strs[]) {
    return fs2_anyof(s, strs, strcmp) < 0;
}

static inline bool           
fs2_not_iin(const fs2 *restrict s, const char *restrict strs[]) {
    return fs2_anyof(s, strs, strcasecmp) < 0;
}

#define                      FS2_NOT_IN(s, ...) \
    fs2_not_in ( (s), (const char *[]) { __VA_ARGS__, NULL } )
#define                      FS2_NOT_IIN(s, ...) \
    fs2_not_iin( (s), (const char *[]) { __VA_ARGS__, NULL } )
#define                      FS2_IN(s, ...) \
    fs2_in ( (s), (const char *[]) { __VA_ARGS__, NULL } )
#define                      FS2_IIN(s, ...) \
    fs2_iin( (s), (const char *[]) { __VA_ARGS__, NULL } )

// =========================================================================
// Binary Search (index-returning)
// =========================================================================

// Array of fs2, case-sensitive
static inline ptrdiff_t
fs2_in_bsearch(const fs2 *restrict key, const fs2 *restrict arr, size_t n) {
    logauto(n);
    logauto((void *) key);
    logauto((void *) arr );
    
    invraise(key != NULL, "Null pointers %p %p", key, arr);

    if (n == 0) return -1;
    
    const fs2 *res = bsearch(key, arr, n, sizeof(fs2), fs2_cmp_wrap);
    return res ? (ptrdiff_t)(res - arr) : -1;
}

// Array of fs2, case-insensitive
static inline ptrdiff_t
fs2_iin_bsearch(const fs2 *restrict key, const fs2 *restrict arr, size_t n) {
    invraise(key != NULL, "Null pointers %p %p", key, arr);
    if (n == 0) return -1;

    const fs2 *res = bsearch(key, arr, n, sizeof(fs2), fs2_icmp_wrap);
    return res ? (ptrdiff_t)(res - arr) : -1;
}

// Array of C-strings, case-sensitive
static inline ptrdiff_t
fs2_in_bsearch_str(const fs2 *restrict key, const char *const *restrict arr, size_t n) {
    invraise(key != NULL, "Null pointers %p %p", key, arr);
    if (n == 0) return -1;
    
    const char *k = key->v ? key->v : "";
    const char *const *res = bsearch(&k, arr, n, sizeof(const char *), pcstr_cmp);
    return res ? (ptrdiff_t)(res - arr) : -1;
}

// Array of C-strings, case-insensitive
static inline ptrdiff_t
fs2_iin_bsearch_str(const fs2 *restrict key, const char *const *restrict arr, size_t n) {
    invraise(key != NULL, "Null pointers %p %p", key, arr);
    if (n == 0) return -1;

    const char *k = key->v ? key->v : "";
    const char *const *res = bsearch(&k, arr, n, sizeof(const char *), pcstr_icmp);
    return res ? (ptrdiff_t)(res - arr) : -1;
}

/* ─────────────────────────────────────────────────────────────────
 * Conversion (parse / format primitives)
 * ───────────────────────────────────────────────────────────────── */

extern bool 
fs2_pos_parse_llong(const fs2 *restrict s, size_t pos, size_t *restrict newpos, long long *restrict out);

/**
 * @brief Parse a long integer starting at @p pos.
 * @param newpos  Optional. Set to the position AFTER the parsed number.
 * @param out     Optional. If NULL, the parsed value is discarded.
 * @return true on success, false on invalid format or overflow.
 * @note streaming version
 */
extern bool 
fs2_pos_parse_long(const fs2 *restrict s, size_t pos, size_t *restrict newpos, long *restrict out);

extern bool 
fs2_pos_parse_int(const fs2 *restrict s, size_t pos, size_t *restrict newpos, int *restrict out);

extern bool 
fs2_pos_parse_ullong(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned long long *restrict out);

extern bool 
fs2_pos_parse_ulong(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned long *restrict out);

extern bool 
fs2_pos_parse_uint(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned *restrict out);

extern bool 
fs2_pos_parse_double(const fs2 *restrict s, size_t pos, size_t *restrict newpos, double *restrict out);

extern bool 
fs2_pos_parse_char(const fs2 *restrict s, size_t pos, size_t *restrict newpos, char *restrict out);

extern bool 
fs2_pos_parse_short(const fs2 *restrict s, size_t pos, size_t *restrict newpos, short *restrict out);

extern bool 
fs2_pos_parse_ushort(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned short *restrict out);

/**
 * @brief Parse the whole string (or prefix) into an int/long.
 * @param strict  If true, `newpos` MUST exactly equal `s->len` (no trailing spaces/garbage).
 */

 static inline bool 
fs2_tryparse_ullong(const fs2 *restrict s, unsigned long long *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_ullong(s, 0, &newpos, out))
        return false;
    
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "Strict parse failed: consumed %zu but len is %zu", 
            newpos, s->len);
        
    return true;
}

static inline bool 
fs2_tryparse_llong(const fs2 *restrict s, long long *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_llong(s, 0, &newpos, out))
        return false;
    
    // Строгий режим: после числа не должно быть НИЧЕГО
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "String mode failed to parse %zu but len %zu", newpos, s->len);
        
    return true;
}

static inline bool 
fs2_tryparse_ulong(const fs2 *restrict s, unsigned long *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_ulong(s, 0, &newpos, out))
        return false;
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "Strict parse failed: consumed %zu but len is %zu", 
            newpos, s->len);
    return true;
}

static inline bool 
fs2_tryparse_long(const fs2 *restrict s, long *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_long(s, 0, &newpos, out))
        return false;
    
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "String mode failed to parse %zu but len %zu", newpos, s->len);
        
    return true;
}

static inline bool 
fs2_tryparse_uint(const fs2 *restrict s, unsigned *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_uint(s, 0, &newpos, out))
        return false;
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "Strict parse failed: consumed %zu but len is %zu", 
            newpos, s->len);

    return true;
}

static inline bool 
fs2_tryparse_int(const fs2 *restrict s, int *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_int(s, 0, &newpos, out))
        return false;
        
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "String mode failed to parse %zu but len %zu", newpos, s->len);;
        
    return true;
}

static inline bool 
fs2_tryparse_double(const fs2 *restrict s, double *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_double(s, 0, &newpos, out))
        return false;
    
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "Strict parse failed: consumed %zu but len is %zu", 
            newpos, s->len);
        
    return true;
}

static inline bool 
fs2_tryparse_char(const fs2 *restrict s, char *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_char(s, 0, &newpos, out))
        return false;
    
    /* strict = true: отвергаем пробельные символы (space, \t, \n, \r, \v, \f) */
    if (strict && isspace((unsigned char)s->v[0]))
        return logsimpleerr(false, 
            "Strict char parse: leading char '%c' (0x%02x) is whitespace", 
            s->v[0], (unsigned char)s->v[0]);
    
    return true;
}

static inline bool 
fs2_tryparse_short(const fs2 *restrict s, short *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_short(s, 0, &newpos, out))
        return false;
    
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "Strict parse failed: consumed %zu but len is %zu", 
            newpos, s->len);
        
    return true;
}

static inline bool 
fs2_tryparse_ushort(const fs2 *restrict s, unsigned short *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_ushort(s, 0, &newpos, out))
        return false;
    
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "Strict parse failed: consumed %zu but len is %zu", 
            newpos, s->len);
        
    return true;
}

/**
 * @brief Parse int/long, raising a user error (SIGINT) on failure. Non-strict.
 */
static inline unsigned long long
fs2_parse_ullong(const fs2 *restrict s) {
    unsigned long long out = 0;
    if (!fs2_tryparse_ullong(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse unsigned long long from string: '%.40s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

static inline long long
fs2_parse_llong(const fs2 *restrict s) {
    long long out = 0;
    if (!fs2_tryparse_llong(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse long long from string: '%.15s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

static inline unsigned long
fs2_parse_ulong(const fs2 *restrict s) {
    unsigned long out = 0;
    if (!fs2_tryparse_ulong(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse unsigned long from string: '%.40s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

static inline long 
fs2_parse_long(const fs2 *restrict s) {
    long out = 0;
    if (!fs2_tryparse_long(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse long from string: '%.15s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

static inline unsigned 
fs2_parse_uint(const fs2 *restrict s) {
    unsigned out = 0;
    if (!fs2_tryparse_uint(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse int from string: '%.15s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}


static inline int 
fs2_parse_int(const fs2 *restrict s) {
    int out = 0;
    if (!fs2_tryparse_int(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse int from string: '%.15s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

static inline double
fs2_parse_double(const fs2 *restrict s) {
    double out = 0.0;
    if (!fs2_tryparse_double(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse double from string: '%.40s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

static inline char 
fs2_parse_char(const fs2 *restrict s) {
    char out = '\0';
    if (!fs2_tryparse_char(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
            "Failed to parse char from empty string");
    return out;
}

static inline short 
fs2_parse_short(const fs2 *restrict s) {
    short out = 0;
    if (!fs2_tryparse_short(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse short from string: '%.40s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

static inline unsigned short 
fs2_parse_ushort(const fs2 *restrict s) {
    unsigned short out = 0;
    if (!fs2_tryparse_ushort(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse unsigned short from string: '%.40s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}



// Format (primitive → string)

// Выбор строки формата по типу
#define FS2_FMT(v) _Generic((v),\
    char:               "%c",   \
    signed char:        "%hhd", \
    unsigned char:      "%hhu", \
    short:              "%hd",  \
    int:                "%d",   \
    long:               "%ld",  \
    long long:          "%lld", \
    unsigned short:     "%hu",  \
    unsigned:           "%u",   \
    unsigned long:      "%lu",  \
    unsigned long long: "%llu", \
    float:              "%f",   \
    double:             "%lf",  \
    void *:             "%p",   \
    const void *:       "%p"    \
)

#define FS2_SER_ERRMSG(v) _Generic((v),                                     \
    char:               "Failed to serialize char %c",                      \
    signed char:        "Failed to serialize signed char %hhd",             \
    unsigned char:      "Failed to serialize unsigned char %hhu",           \
    short:              "Failed to serialize short %hd",                    \
    int:                "Failed to serialize int %d",                       \
    long:               "Failed to serialize long %ld",                     \
    long long:          "Failed to serialize long long %lld",               \
    unsigned short:     "Failed to serialize unsigned short %hu",           \
    unsigned:           "Failed to serialize unsigned %u",                  \
    unsigned long:      "Failed to serialize unsigned long %lu",            \
    unsigned long long: "Failed to serialize unsigned long long %llu",      \
    float:              "Failed to serialize float %f",                     \
    double:             "Failed to serialize double %lf",                   \
    void *:             "Failed to serialize pointer %p",                   \
    const void *:       "Failed to serialize pointer %p"                    \
)

// Запись в существующий fs2
#define fs2_serialize(dst, val) \
    fs2_sprintf((dst), FS2_FMT(val), (val))

// 3. Сериализация в стек
#define fs2_from_new(val) \
    ({ \
        fs2 _fs2_res = FS2(); \
        if (fs2_sprintf(&_fs2_res, FS2_FMT(val), (val)) <= 0) \
            userraise(-1, ERR_UNABLE_SERIALIZE_DATA, \
                FS2_SER_ERRMSG(val), (val)); \
        _fs2_res; \
    })

// 4. Сериализация в кучу
#define fs2_heap_from(val) \
    ({ \
        fs2 tmp = fs2_from_new(val); \
        fs2_stacktoheap(&tmp); \
    })

#define fs2_serialize_concat(dst, val) \
    fs2_sprintf_concat((dst), FS2_FMT(val), (val))

// string parsers

/**
 * @brief Length of initial run in @p s (from @p pos) of chars in @p accept.
 * @return 0 on null, empty, or pos >= s->len.
 */

static inline size_t
fs2_spn(const fs2 *restrict s, size_t pos, const char *restrict accept) {
    invraise(s != NULL && accept != NULL, "Null pointers %p %p", s, accept);
    if (pos >= s->len) return 0;
    return strspn(s->v + pos, accept);
}

/**
 * @brief Length of initial run in @p s (from @p pos) not in @p reject.
 */
static inline size_t
fs2_cspn(const fs2 *restrict s, size_t pos, const char *restrict reject) {
    invraise(s != NULL && reject != NULL, "Null pointers %p %p", s, reject);
    if (pos >= s->len) return 0;
    return strcspn(s->v + pos, reject);
}

/**
 * @brief Length of initial non-whitespace run.
 *
 * Whitespace = " \t\n\r\v\f" (C locale, locale-independent).
 */
static inline size_t
fs2_nonspace_span(const fs2 *restrict s, size_t pos) {
    return fs2_cspn(s, pos, " \t\n\r\v\f");
}

//
//fs2_verirystr(pattern )

#endif /* !FS2_API_H */
