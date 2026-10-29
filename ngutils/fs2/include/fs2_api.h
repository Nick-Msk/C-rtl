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
__fs2_chr_raw_ci(const unsigned char *p, size_t avail, int c) {
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
__fs2_rchr_raw_ci(const unsigned char *p, size_t avail, int c) {
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

/**
 * @brief Append @p src to @p dst in place.
 *
 * The buffer of @p dst is grown as needed via `fs2_reserve`.
 * A NUL terminator is guaranteed at the end.
 *
 * @param dst  Non-NULL fs2 (ALLOC or LOCAL). Grows.
 * @param src  Non-NULL fs2 (any flavor). Not modified.
 * @return     @p dst (for chaining).
 */
extern fs2 *
fs2_cat(fs2 *restrict dst, const fs2 *restrict src);

/**
 * @brief Append a C-string @p src to @p dst in place.
 *
 * Convenience wrapper: wraps @p src with `fs2_wrap` and calls `fs2_cat`.
 *
 * @param dst  Non-NULL fs2 (ALLOC or LOCAL).
 * @param src  NUL-terminated string (may be NULL → no-op).
 * @return     @p dst.
 */
static inline fs2 *
fs2_catstr(fs2 *restrict dst, const char *restrict src) {
    fs2 s = fs2_wrap(src);
    return fs2_cat(dst, &s);
}

/**
 * @brief Prepend a C-string @p beginner to @p dst in place.
 *
 * Moves the existing content forward and copies @p beginner at position 0.
 *
 * @param dst       Non-NULL fs2 (ALLOC or LOCAL).
 * @param beginner  NUL-terminated string (NULL → no-op).
 * @return          @p dst.
 */
extern fs2 *
fs2_rev_catstr(fs2 *restrict dst, const char *restrict beginner);

/**
 * @brief Copy @p src into @p dst, replacing all current content.
 *
 * Clears @p dst first, then concatenates @p src.
 * Self-copy (dst == src) is a no-op.
 *
 * @param dst  Non-NULL fs2 (ALLOC or LOCAL). Content replaced.
 * @param src  Non-NULL fs2 (any flavor). Not modified.
 * @return     @p dst.
 */
static inline fs2 *
fs2_cpy(fs2 *restrict dst, const fs2* restrict src) {
    invraise(dst != NULL, "Null pointer");

    if (dst == src)
        return dst; // self-copy is a no-op
    fs2_clear(dst);
    return fs2_cat(dst, src);
}

/**
 * @brief Copy a C-string @p src into @p dst, replacing all content.
 *
 * @param dst  Non-NULL fs2 (ALLOC or LOCAL).
 * @param src  NUL-terminated string (may be NULL → clears dst).
 * @return     @p dst.
 */
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

/**
 * @brief Internal: format into @p s starting at @p pos using a va_list.
 *
 * Grows the buffer if needed. Does NOT update `s->len` — the caller
 * is responsible for calling `fs2_setlen` with the result.
 *
 * @param s    Non-NULL fs2 (ALLOC).
 * @param pos  0-based offset to start writing.
 * @param fmt  Format string (non-NULL).
 * @param ap   Pre-initialized argument list.
 * @return     Number of characters written (excluding NUL), or -1 on error.
 */
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

/**
 * @brief Replace content of @p s with the substring [from, from+len).
 *
 * In-place: moves the selected bytes to the beginning and truncates.
 * Bounds are clamped: `from` is clamped to `s->len`, `len` to the
 * remaining tail. A NULL buffer (`!s->v`) leaves the fs2 untouched.
 *
 * @param s     Non-NULL fs2 (ALLOC or LOCAL). Modified in place.
 * @param from  Start offset (clamped).
 * @param len   Number of bytes to keep (clamped).
 * @return      @p s.
 */
extern fs2 *
fs2_substr(fs2 *s, size_t from, size_t len);

/**
 * @brief Return a stack-allocated fs2 containing the substring.
 *
 * Does NOT modify @p s (works with STATIC fs2 as well).
 *
 * @param s     Non-NULL fs2 (any flavor).
 * @param from  Start offset (clamped).
 * @param len   Number of bytes to extract (clamped).
 * @return      A new fs2 (stack, FS2_BODYALLOC body).
 */
extern fs2 
fs2_substr_new(const fs2 *s, size_t from, size_t len);

/**
 * @brief Return a heap-allocated fs2 containing the substring.
 *
 * @param s     Non-NULL fs2 (any flavor).
 * @param from  Start offset (clamped).
 * @param len   Number of bytes to extract (clamped).
 * @return      A new heap-allocated fs2 (FS2_BODYALLOC + FS2_BODYALLOC).
 */
extern fs2 *
fs2_heap_substr(const fs2 *s, size_t from, size_t len);

/**
 * @brief Keep only the first @p len characters (in-place, like PostgreSQL `left()`).
 *
 * @param s    Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param len  Number of leading bytes to keep (clamped).
 * @return     @p s.
 */
static inline fs2 *
fs2_left(fs2 *s, size_t len) {
    return fs2_substr(s, 0, len);
}

/**
 * @brief Return a stack-allocated copy containing the first @p len characters.
 *
 * @param s    Non-NULL fs2 (any flavor). Not modified.
 * @param len  Number of leading bytes to keep (clamped).
 * @return     New stack fs2.
 */
static inline fs2
fs2_left_new(const fs2 *s, size_t len) {
    return fs2_substr_new(s, 0, len);
}

/**
 * @brief Return a heap-allocated copy containing the first @p len characters.
 *
 * @param s    Non-NULL fs2 (any flavor). Not modified.
 * @param len  Number of leading bytes to keep (clamped).
 * @return     New heap fs2.
 */
static inline fs2 *
fs2_heap_left(fs2 *s, size_t len) {
    return fs2_heap_substr(s, 0, len);
}

/**
 * @brief Keep only the last @p len characters (in-place, like PostgreSQL `right()`).
 *
 * If `len >= s->len`, the string is unchanged.
 *
 * @param s    Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param len  Number of trailing bytes to keep.
 * @return     @p s.
 */
static inline fs2 *
fs2_right(fs2 *s, size_t len) {
    if (!s || len >= s->len)
        return s;

    return fs2_substr(s, s->len - len, len);
}

/**
 * @brief Return a stack-allocated copy containing the last @p len characters.
 *
 * @param s    Non-NULL fs2 (any flavor). Not modified.
 * @param len  Number of trailing bytes to keep.
 * @return     New stack fs2.
 */
static inline fs2
fs2_right_new(const fs2 *s, size_t len) {
    if (!s)
        return FS2();
    if (len >= s->len)
        return fs2_clone(s);

    return fs2_substr_new(s, s->len - len, len);
}

/**
 * @brief Return a heap-allocated copy containing the last @p len characters.
 *
 * @param s    Non-NULL fs2 (any flavor). Not modified.
 * @param len  Number of trailing bytes to keep.
 * @return     New heap fs2.
 */
static inline fs2 *
fs2_heap_right(const fs2 *s, size_t len) {
    if (!s)
        return fs2_heap();
    if (len >= s->len)
        return fs2_heap_clone(s);

    return fs2_heap_substr(s, s->len - len, len);
}

// ------------------------------- Upper / Lower case ---------------------------------------

/**
 * @brief Convert all bytes to upper case in place (byte-wise, NOT Unicode-aware).
 *
 * @pre    str is not FS2_STATIC.
 *
 * @param str  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @return     @p str.
 */
extern fs2 *
fs2_toupper(fs2 *str);

/**
 * @brief Return a stack-allocated copy with all bytes in upper case.
 *
 * @param str  Non-NULL fs2 (any flavor). Not modified.
 * @return     New stack fs2 (cloned, then upper-cased).
 */
static inline fs2
fs2_toupper_new(const fs2 *str) {
    fs2 res = fs2_clone(str);
    fs2_toupper(&res);
    return res;
}

/**
 * @brief Return a heap-allocated copy with all bytes in upper case.
 *
 * @param str  Non-NULL fs2 (any flavor). Not modified.
 * @return     New heap fs2.
 */
static inline fs2 *
fs2_heap_toupper(const fs2 *str) {
    return fs2_toupper(fs2_heap_clone(str));
}

/**
 * @brief Convert all bytes to lower case in place (byte-wise, NOT Unicode-aware).
 *
 * @pre    str is not FS2_STATIC.
 *
 * @param str  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @return     @p str.
 */
extern fs2 *
fs2_tolower(fs2 *str);

/**
 * @brief Return a stack-allocated copy with all bytes in lower case.
 *
 * @param str  Non-NULL fs2 (any flavor). Not modified.
 * @return     New stack fs2 (cloned, then lower-cased).
 */
static inline fs2
fs2_tolower_new(const fs2 *str) {
    fs2 res = fs2_clone(str);
    fs2_tolower(&res);
    return res;
}

/**
 * @brief Return a heap-allocated copy with all bytes in lower case.
 *
 * @param str  Non-NULL fs2 (any flavor). Not modified.
 * @return     New heap fs2.
 */
static inline  fs2 *
fs2_heap_tolower(const fs2 *str) {
    return fs2_tolower(fs2_heap_clone(str));
}

// ------------------------------------- Padding -------------------------------------------

/**
 * @brief Right-pad @p str to total length @p len using bytes from @p pad (in-place).
 *
 * If `len < str->len`, the string is truncated to `len` via `fs2_left`.
 * If `len == str->len` or `pad->len == 0`, no change.
 * Otherwise the pad pattern is repeated until the target length is reached.
 *
 * @param str  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param len  Target total length.
 * @param pad  Non-NULL non-empty fs2 used as the repeating pad pattern.
 * @return     @p str.
 * 
 * @pre str != pad  (restrict: str and pad must not alias)
 *
 * @note For self-padding — pad the string with itself — use
 *       fs2_repeat(). It's semantically different (n copies
 *       vs pad to length) and is UB-free.
 *
 *       fs2_rpad("ab", 8, "ab")  → "abababab"   (pad to length 8)
 *       fs2_repeat("ab", 4)      → "abababab"   (4 copies, also length 8)
 *
 *       The first form (self-pad) is UB. Use the second.
 */
extern fs2 *                    
fs2_rpad(fs2 *restrict str, size_t len, const fs2 *restrict pad);

/**
 * @brief Right-pad @p str to @p len using a C-string pad (in-place).
 *
 * @param str     Non-NULL fs2 (ALLOC or LOCAL).
 * @param len     Target total length.
 * @param padstr  Non-NULL non-empty C-string pad pattern.
 * @return        @p str.
 */
static inline fs2 *
fs2_rpadstr(fs2 *restrict str, size_t len, const char *restrict padstr) {

    fs2 tmp = fs2_wrap(padstr);
    return fs2_rpad(str, len, &tmp);
}

/**
 * @brief Return a stack-allocated copy of @p str right-padded to @p len.
 *
 * @param str  Non-NULL fs2 (any flavor). Not modified.
 * @param len  Target total length.
 * @param pad  Non-NULL non-empty fs2 pad (NULL → unmodified clone).
 * @return     New stack fs2.
 */
static inline fs2
fs2_rpad_new(const fs2 *restrict str, size_t len, const fs2 *restrict pad) {
    invraise(str != NULL, "Null pointer");

    fs2 res = fs2_clone(str);  
    if (!pad)
        return res;
     
    fs2_rpad(&res, len, pad);
    return res;
}

/**
 * @brief Return a stack-allocated copy of @p str right-padded to @p len (C-string pad).
 *
 * @param str     Non-NULL fs2 (any flavor).
 * @param len     Target total length.
 * @param padstr  Non-NULL non-empty C-string pad.
 * @return        New stack fs2.
 */
static inline fs2
fs2_rpadstr_new(const fs2 *restrict str, size_t len, const char *restrict padstr) {
    
    fs2 tmp = fs2_wrap(padstr);
    return fs2_rpad_new(str, len, &tmp);
}

/**
 * @brief Left-pad @p str to total length @p len using bytes from @p pad (in-place).
 *
 * The existing content is shifted right; the pad pattern fills the gap on the left.
 * If `len < str->len`, the string is truncated via `fs2_left`.
 *
 * @param str  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param len  Target total length.
 * @param pad  Non-NULL non-empty fs2 used as the repeating pad pattern.
 * @return     @p str.
 *
 * @pre str != pad  (restrict: str and pad must not alias)
 *
 * @note For self-padding — pad the string with itself — use
 *       fs2_repeat(). It's semantically different (n copies
 *       vs pad to length) and is UB-free.
 *
 *       fs2_rpad("ab", 8, "ab")  → "abababab"   (pad to length 8)
 *       fs2_repeat("ab", 4)      → "abababab"   (4 copies, also length 8)
 *
 *       The first form (self-pad) is UB. Use the second.
 */
extern fs2 *                    
fs2_lpad(fs2 *restrict str, size_t len, const fs2 *restrict pad);

/**
 * @brief Left-pad @p str to @p len using a C-string pad (in-place).
 *
 * @param str     Non-NULL fs2 (ALLOC or LOCAL).
 * @param len     Target total length.
 * @param padstr  Non-NULL non-empty C-string pad pattern.
 * @return        @p str.
 */
static inline fs2 *
fs2_lpadstr(fs2 *restrict str, size_t len, const char *restrict padstr) {

    fs2 tmp = fs2_wrap(padstr);
    return fs2_lpad(str, len, &tmp);
}

/**
 * @brief Return a stack-allocated copy of @p str left-padded to @p len.
 *
 * @param str  Non-NULL fs2 (any flavor). Not modified.
 * @param len  Target total length.
 * @param pad  Non-NULL non-empty fs2 pad (NULL → unmodified clone).
 * @return     New stack fs2.
 */
static inline fs2
fs2_lpad_new(const fs2 *restrict str, size_t len, const fs2 *restrict pad) {
    invraise(str != NULL, "Null pointer");

    fs2 res = fs2_clone(str);  
    if (!pad)
        return res;
     
    fs2_lpad(&res, len, pad);
    return res;
}

/**
 * @brief Return a stack-allocated copy of @p str left-padded to @p len (C-string pad).
 *
 * @param str     Non-NULL fs2 (any flavor).
 * @param len     Target total length.
 * @param padstr  Non-NULL non-empty C-string pad.
 * @return        New stack fs2.
 */
static inline fs2
fs2_lpadstr_new(const fs2 *restrict str, size_t len, const char *restrict padstr) {
    
    fs2 tmp = fs2_wrap(padstr);
    return fs2_lpad_new(str, len, &tmp);
}

// --------------------------------- Random string generators ------------------------------

/**
 * @brief Fill @p s with @p len random characters in place.
 *
 * Supported @p type values:
 *  - `'a'` — lowercase letters a-z
 *  - `'A'` — uppercase letters A-Z
 *  - `'0'` — digits 0-9
 *  - any other — fills with `'?'`
 *
 * @param s     Non-NULL fs2 (ALLOC or LOCAL). Buffer is grown as needed.
 * @param len   Number of random characters to generate.
 * @param type  Character set selector (see above).
 * @return      @p s.
 */
extern fs2 *           
fs2_genrnd(fs2 *s, size_t len, char type);

/**
 * @brief Return a stack-allocated fs2 containing @p len random characters.
 *
 * @param len   Number of random characters.
 * @param type  Character set selector ('a', 'A', '0', etc.).
 * @return      New stack fs2.
 */
static inline fs2
fs2_genrnd_new(size_t len, char type) {
    fs2     res = fs2_init(len + 1);
    fs2_genrnd(&res, len, type);
    return res;
}

/**
 * @brief Return a heap-allocated fs2 containing @p len random characters.
 *
 * @param len   Number of random characters.
 * @param type  Character set selector ('a', 'A', '0', etc.).
 * @return      New heap fs2.
 */
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
fs2_chr_ci(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");

    return __fs2_chr_raw_ci((const unsigned char *) str->v, str->len, c);
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
fs2_nchr_ci(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    size_t avail = (lim < str->len) ? lim : str->len;
    return __fs2_chr_raw_ci((const unsigned char *) str->v, avail, c);
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
fs2_rchr_ci(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");

    return __fs2_rchr_raw_ci((const unsigned char *)str->v, str->len, c);
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
    ptrdiff_t hit = __fs2_rchr_raw((const unsigned char *) str->v + off, avail, c);
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
fs2_nrchr_ci(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    size_t avail = (lim < str->len) ? lim : str->len;
    size_t off   = str->len - avail;
    ptrdiff_t hit = __fs2_rchr_raw_ci((const unsigned char *) str->v + off, avail, c);
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
ptrdiff_t   fs2_chr_ci_nth  (const fs2 *str, int c, size_t k);

/**
 * @brief Find the k-th forward case-insensitive occurrence within [0, lim).
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based occurrence number (0 → -1).
 * @param lim  Maximum bytes to examine (clamped to s->len).
 * @return     0-based offset, or -1.
 */
ptrdiff_t   fs2_nchr_ci_nth (const fs2 *str, int c, size_t k, size_t lim);

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
ptrdiff_t   fs2_rchr_ci_nth (const fs2 *str, int c, size_t k);

/**
 * @brief Find the k-th reverse case-insensitive occurrence within the last @p lim bytes.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based reverse occurrence number (0 → -1).
 * @param lim  Number of trailing bytes to examine (clamped to s->len).
 * @return     0-based offset from string start, or -1.
 */
ptrdiff_t   fs2_nrchr_ci_nth(const fs2 *str, int c, size_t k, size_t lim);

// ----------------------------------- Finders -----------------------------------

/**
 * @brief Linear search: find @p s in a NULL-terminated array of C-strings.
 *
 * Full scan using a custom comparator (e.g. `strcmp` or `strcasecmp`).
 *
 * @param s     Non-NULL fs2 (any flavor; NULL `v` is treated as "").
 * @param strs  NULL-terminated array of C-strings.
 * @param cmp   Comparison function (`int(const char *, const char *)`).
 * @return      0-based index of the first match, or -1.
 */
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

/**
 * @brief Case-sensitive check: is @p s present in a NULL-terminated C-string array?
 *
 * @param s     Non-NULL fs2.
 * @param strs  NULL-terminated array of C-strings.
 * @return      true if found (using `strcmp`).
 */
static inline bool           
fs2_in(const fs2 *restrict s, const char *restrict strs[]) {
    return fs2_anyof(s, strs, strcmp) >= 0;
}

/**
 * @brief Case-insensitive check: is @p s present in a NULL-terminated C-string array?
 *
 * @param s     Non-NULL fs2.
 * @param strs  NULL-terminated array of C-strings.
 * @return      true if found (using `strcasecmp`).
 */
static inline bool           
fs2_in_ci(const fs2 *restrict s, const char *restrict strs[]) {
    return fs2_anyof(s, strs, strcasecmp) >= 0;
}

/**
 * @brief Case-sensitive check: is @p s NOT in a NULL-terminated C-string array?
 *
 * @param s     Non-NULL fs2.
 * @param strs  NULL-terminated array of C-strings.
 * @return      true if NOT found (using `strcmp`).
 */
static inline bool           
fs2_not_in(const fs2 *restrict s, const char *restrict strs[]) {
    return fs2_anyof(s, strs, strcmp) < 0;
}

/**
 * @brief Case-insensitive check: is @p s NOT in a NULL-terminated C-string array?
 *
 * @param s     Non-NULL fs2.
 * @param strs  NULL-terminated array of C-strings.
 * @return      true if NOT found (using `strcasecmp`).
 */
static inline bool           
fs2_not_in_ci(const fs2 *restrict s, const char *restrict strs[]) {
    return fs2_anyof(s, strs, strcasecmp) < 0;
}

/**
 * @brief Macro: case-sensitive "NOT IN" — is @p s absent from a list of C-strings?
 *
 * Expands to `fs2_not_in(s, (const char*[]){ __VA_ARGS__, NULL })`.
 *
 * @code
 *   FS2_NOT_IN(name, "alice", "bob", "carol")
 * @endcode
 *
 * @param s      Non-NULL fs2 to check.
 * @param ...    One or more C-string literals (NULL terminator is appended).
 * @return       true if @p s does NOT match any of the listed strings.
 */
#define                      FS2_NOT_IN(s, ...) \
    fs2_not_in ( (s), (const char *[]) { __VA_ARGS__, NULL } )

/**
 * @brief Macro: case-insensitive "NOT IN" — is @p s absent from a list of C-strings?
 *
 * @param s      Non-NULL fs2 to check.
 * @param ...    One or more C-string literals.
 * @return       true if @p s does NOT match any (using `strcasecmp`).
 */
#define                      FS2_NOT_IN_CI(s, ...) \
    fs2_not_in_ci( (s), (const char *[]) { __VA_ARGS__, NULL } )

/**
 * @brief Macro: case-sensitive "IN" — is @p s present in a list of C-strings?
 *
 * @code
 *   FS2_IN(name, "alice", "bob")
 * @endcode
 *
 * @param s      Non-NULL fs2 to check.
 * @param ...    One or more C-string literals.
 * @return       true if @p s matches one of the listed strings.
 */
#define                      FS2_IN(s, ...) \
    fs2_in ( (s), (const char *[]) { __VA_ARGS__, NULL } )

/**
 * @brief Macro: case-insensitive "IN" — is @p s present in a list of C-strings?
 *
 * @param s      Non-NULL fs2 to check.
 * @param ...    One or more C-string literals.
 * @return       true if @p s matches one of the listed strings (using `strcasecmp`).
 */
#define                      FS2_IN_CI(s, ...) \
    fs2_in_ci( (s), (const char *[]) { __VA_ARGS__, NULL } )

// =========================================================================
// Binary Search (index-returning)
// =========================================================================

/**
 * @brief Binary search a sorted array of fs2 (case-sensitive).
 *
 * The array must be pre-sorted in ascending lexicographic order
 * (`strcmp` semantics). Uses `bsearch` under the hood.
 *
 * @param key  Non-NULL fs2 to look for.
 * @param arr  Array of n sorted fs2 elements.
 * @param n    Number of elements in the array.
 * @return     0-based index, or -1 if not found.
 */
static inline ptrdiff_t
fs2_bsearch(const fs2 *restrict key, const fs2 *restrict arr, size_t n) {
    logauto(n);
    logauto((void *) key);
    logauto((void *) arr );
    
    invraise(key != NULL, "Null pointers %p %p", key, arr);

    if (n == 0) return -1;
    
    const fs2 *res = bsearch(key, arr, n, sizeof(fs2), fs2_cmp_wrap);
    return res ? (ptrdiff_t)(res - arr) : -1;
}

/**
 * @brief Binary search a sorted array of fs2 (case-insensitive).
 *
 * The array must be pre-sorted in ascending order (`strcasecmp` semantics).
 *
 * @param key  Non-NULL fs2 to look for.
 * @param arr  Array of n sorted fs2 elements.
 * @param n    Number of elements.
 * @return     0-based index, or -1.
 */
static inline ptrdiff_t
fs2_bsearch_ci(const fs2 *restrict key, const fs2 *restrict arr, size_t n) {
    invraise(key != NULL, "Null pointers %p %p", key, arr);
    if (n == 0) return -1;

    const fs2 *res = bsearch(key, arr, n, sizeof(fs2), fs2_cmp_wrap_ci);
    return res ? (ptrdiff_t)(res - arr) : -1;
}

/**
 * @brief Binary search a sorted array of C-strings (case-sensitive).
 *
 * @param key  Non-NULL fs2 to look for.
 * @param arr  Array of n sorted `const char *` pointers.
 * @param n    Number of elements.
 * @return     0-based index, or -1.
 */
static inline ptrdiff_t
fs2_bsearch_str(const fs2 *restrict key, const char *const *restrict arr, size_t n) {
    invraise(key != NULL, "Null pointers %p %p", key, arr);
    if (n == 0) return -1;
    
    const char *k = key->v ? key->v : "";
    const char *const *res = bsearch(&k, arr, n, sizeof(const char *), pcstr_cmp);
    return res ? (ptrdiff_t)(res - arr) : -1;
}

/**
 * @brief Binary search a sorted array of C-strings (case-insensitive).
 *
 * @param key  Non-NULL fs2 to look for.
 * @param arr  Array of n sorted `const char *` pointers.
 * @param n    Number of elements.
 * @return     0-based index, or -1.
 */
static inline ptrdiff_t
fs2_bsearch_ci_str(const fs2 *restrict key, const char *const *restrict arr, size_t n) {
    invraise(key != NULL, "Null pointers %p %p", key, arr);
    if (n == 0) return -1;

    const char *k = key->v ? key->v : "";
    const char *const *res = bsearch(&k, arr, n, sizeof(const char *), pcstr_icmp);
    return res ? (ptrdiff_t)(res - arr) : -1;
}

/* ─────────────────────────────────────────────────────────────────
 * Conversion (parse / format primitives)
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief Parse a long long integer starting at @p pos (streaming).
 *
 * Skips leading spaces. Accepts an optional sign. Uses `strtoll`.
 *
 * @param s       Non-NULL fs2 with v != NULL.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position AFTER the parsed number.
 * @param out     Optional. If NULL, the parsed value is discarded.
 * @return        true on success, false on invalid format or overflow.
 */
extern bool 
fs2_pos_parse_llong(const fs2 *restrict s, size_t pos, size_t *restrict newpos, long long *restrict out);

/**
 * @brief Parse a long integer starting at @p pos (streaming).
 *
 * Delegates to `fs2_pos_parse_llong` then range-checks to `long`.
 *
 * @param s       Non-NULL fs2.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format, overflow, or range.
 */
extern bool 
fs2_pos_parse_long(const fs2 *restrict s, size_t pos, size_t *restrict newpos, long *restrict out);

/**
 * @brief Parse an int starting at @p pos (streaming).
 *
 * Delegates to `fs2_pos_parse_long` then range-checks to `int`.
 *
 * @param s       Non-NULL fs2.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format, overflow, or range.
 */
extern bool 
fs2_pos_parse_int(const fs2 *restrict s, size_t pos, size_t *restrict newpos, int *restrict out);

/**
 * @brief Parse an unsigned long long starting at @p pos (streaming).
 *
 * Skips leading spaces. Rejects a leading minus sign.
 * Uses `strtoull`.
 *
 * @param s       Non-NULL fs2 with v != NULL.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format or overflow.
 */
extern bool 
fs2_pos_parse_ullong(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned long long *restrict out);

/**
 * @brief Parse an unsigned long starting at @p pos (streaming).
 *
 * Delegates to `fs2_pos_parse_ullong` then range-checks.
 *
 * @param s       Non-NULL fs2.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format, overflow, or range.
 */
extern bool 
fs2_pos_parse_ulong(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned long *restrict out);

/**
 * @brief Parse an unsigned int starting at @p pos (streaming).
 *
 * Delegates to `fs2_pos_parse_ulong` then range-checks.
 *
 * @param s       Non-NULL fs2.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format, overflow, or range.
 */
extern bool 
fs2_pos_parse_uint(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned *restrict out);

/**
 * @brief Parse a double starting at @p pos (streaming).
 *
 * Accepts a digit, dot, sign, and letters for nan/inf. Uses `strtod`.
 *
 * @param s       Non-NULL fs2 with v != NULL.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format or overflow.
 */
extern bool 
fs2_pos_parse_double(const fs2 *restrict s, size_t pos, size_t *restrict newpos, double *restrict out);

/**
 * @brief Parse a single character at @p pos (streaming).
 *
 * Advances the position by 1.
 *
 * @param s       Non-NULL fs2 with v != NULL.
 * @param pos     Start offset (must be < s->len).
 * @param newpos  Optional. Set to pos + 1.
 * @param out     Optional. If NULL, the character is discarded.
 * @return        true on success, false if pos is out of bounds.
 */
extern bool 
fs2_pos_parse_char(const fs2 *restrict s, size_t pos, size_t *restrict newpos, char *restrict out);

/**
 * @brief Parse a short integer starting at @p pos (streaming).
 *
 * Delegates to `fs2_pos_parse_llong` then range-checks to `short`.
 *
 * @param s       Non-NULL fs2.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format, overflow, or range.
 */
extern bool 
fs2_pos_parse_short(const fs2 *restrict s, size_t pos, size_t *restrict newpos, short *restrict out);

/**
 * @brief Parse an unsigned short starting at @p pos (streaming).
 *
 * Delegates to `fs2_pos_parse_ullong` then range-checks.
 *
 * @param s       Non-NULL fs2.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format, overflow, or range.
 */
extern bool 
fs2_pos_parse_ushort(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned short *restrict out);

// ------------------------------------------------------------------
// TryParse: parse the entire string (pos 0 → s->len)
//
// All tryparse_* functions call the corresponding pos_parse_* with
// pos = 0. In @p strict mode the parsed value must consume the
// entire string (newpos == s->len); otherwise trailing garbage is
// tolerated.
// ------------------------------------------------------------------

/**
 * @brief Parse the whole string into an unsigned long long.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
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

/**
 * @brief Parse the whole string into a long long.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
static inline bool 
fs2_tryparse_llong(const fs2 *restrict s, long long *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_llong(s, 0, &newpos, out))
        return false;
    
    // Strict mode: there must be NOTHING after the number
    if (strict && newpos != s->len)
        return logsimpleerr(false, 
            "String mode failed to parse %zu but len %zu", newpos, s->len);
        
    return true;
}

/**
 * @brief Parse the whole string into an unsigned long.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
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

/**
 * @brief Parse the whole string into a long.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
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

/**
 * @brief Parse the whole string into an unsigned int.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
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

/**
 * @brief Parse the whole string into an int.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
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

/**
 * @brief Parse the whole string into a double.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
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

/**
 * @brief Parse the whole string into a single character.
 *
 * @param s       Non-NULL fs2 (must be non-empty).
 * @param out     Output character (may be NULL to discard).
 * @param strict  If true, rejects leading whitespace.
 * @return        true on success.
 */
static inline bool 
fs2_tryparse_char(const fs2 *restrict s, char *restrict out, bool strict) {
    size_t newpos = 0;
    if (!fs2_pos_parse_char(s, 0, &newpos, out))
        return false;
    
    /* strict = true: reject whitespace characters (space, \t, \n, \r, \v, \f) */
    if (strict && isspace((unsigned char)s->v[0]))
        return logsimpleerr(false, 
            "Strict char parse: leading char '%c' (0x%02x) is whitespace", 
            s->v[0], (unsigned char)s->v[0]);
    
    return true;
}

/**
 * @brief Parse the whole string into a short.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
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

/**
 * @brief Parse the whole string into an unsigned short.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
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

/**
 * @brief Parse the whole string into a long long, raising on failure.
 *
 * Non-strict: trailing characters are tolerated.
 * On failure raises a user error (ERR_UNABLE_PARSE_DATA).
 *
 * @param s  Non-NULL fs2.
 * @return   Parsed value (or raises).
 */
static inline long long
fs2_parse_llong(const fs2 *restrict s) {
    long long out = 0;
    if (!fs2_tryparse_llong(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse long long from string: '%.15s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

/**
 * @brief Parse the whole string into an unsigned long, raising on failure.
 *
 * Non-strict: trailing characters are tolerated.
 *
 * @param s  Non-NULL fs2.
 * @return   Parsed value (or raises).
 */
static inline unsigned long
fs2_parse_ulong(const fs2 *restrict s) {
    unsigned long out = 0;
    if (!fs2_tryparse_ulong(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse unsigned long from string: '%.40s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

/**
 * @brief Parse the whole string into a long, raising on failure.
 *
 * Non-strict: trailing characters are tolerated.
 *
 * @param s  Non-NULL fs2.
 * @return   Parsed value (or raises).
 */
static inline long 
fs2_parse_long(const fs2 *restrict s) {
    long out = 0;
    if (!fs2_tryparse_long(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse long from string: '%.15s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

/**
 * @brief Parse the whole string into an unsigned int, raising on failure.
 *
 * Non-strict: trailing characters are tolerated.
 *
 * @param s  Non-NULL fs2.
 * @return   Parsed value (or raises).
 */
static inline unsigned 
fs2_parse_uint(const fs2 *restrict s) {
    unsigned out = 0;
    if (!fs2_tryparse_uint(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse int from string: '%.15s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}


/**
 * @brief Parse the whole string into an int, raising on failure.
 *
 * Non-strict: trailing characters are tolerated.
 *
 * @param s  Non-NULL fs2.
 * @return   Parsed value (or raises).
 */
static inline int 
fs2_parse_int(const fs2 *restrict s) {
    int out = 0;
    if (!fs2_tryparse_int(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse int from string: '%.15s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

/**
 * @brief Parse the whole string into a double, raising on failure.
 *
 * Non-strict: trailing characters are tolerated.
 *
 * @param s  Non-NULL fs2.
 * @return   Parsed value (or raises).
 */
static inline double
fs2_parse_double(const fs2 *restrict s) {
    double out = 0.0;
    if (!fs2_tryparse_double(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse double from string: '%.40s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

/**
 * @brief Parse the whole string into a single character, raising on failure.
 *
 * Non-strict: a single character at position 0.
 *
 * @param s  Non-NULL fs2 (must be non-empty).
 * @return   The character (or raises).
 */
static inline char 
fs2_parse_char(const fs2 *restrict s) {
    char out = '\0';
    if (!fs2_tryparse_char(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
            "Failed to parse char from empty string");
    return out;
}

/**
 * @brief Parse the whole string into a short, raising on failure.
 *
 * Non-strict: trailing characters are tolerated.
 *
 * @param s  Non-NULL fs2.
 * @return   Parsed value (or raises).
 */
static inline short 
fs2_parse_short(const fs2 *restrict s) {
    short out = 0;
    if (!fs2_tryparse_short(s, &out, false))
        userraise(-1, ERR_UNABLE_PARSE_DATA, 
              "Failed to parse short from string: '%.40s'", 
              (s && s->v) ? s->v : "(null)");
    return out;
}

/**
 * @brief Parse the whole string into an unsigned short, raising on failure.
 *
 * Non-strict: trailing characters are tolerated.
 *
 * @param s  Non-NULL fs2.
 * @return   Parsed value (or raises).
 */
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

/**
 * @brief Select the correct `printf` format specifier for a primitive type.
 *
 * Uses C11 `_Generic` to map the static type of @p v to its
 * corresponding format string (`%d`, `%lu`, `%f`, …).
 */
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

/**
 * @brief Select a human-readable error message for a serialization failure.
 *
 * Maps the static type of @p v to a formatted string that includes
 * the offending value (e.g. `"Failed to serialize int 42"`).
 */
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

/**
 * @brief Serialize a primitive value into an existing fs2 (replaces content).
 *
 * Uses `fs2_sprintf` with the appropriate format specifier selected
 * automatically via `_Generic`.
 */
#define fs2_serialize(dst, val) \
    fs2_sprintf((dst), FS2_FMT(val), (val))

/**
 * @brief Serialize a primitive value into a new stack-allocated fs2.
 *
 * Returns the fs2 by value. Raises on serialization failure.
 */
#define fs2_from_new(val) \
    ({ \
        fs2 _fs2_res = FS2(); \
        if (fs2_sprintf(&_fs2_res, FS2_FMT(val), (val)) <= 0) \
            userraise(-1, ERR_UNABLE_SERIALIZE_DATA, \
                FS2_SER_ERRMSG(val), (val)); \
        _fs2_res; \
    })

/**
 * @brief Serialize a primitive value into a new heap-allocated fs2.
 *
 * Serializes to a stack fs2 first, then moves the body to the heap.
 * The caller must free the result.
 */
#define fs2_heap_from(val) \
    ({ \
        fs2 tmp = fs2_from_new(val); \
        fs2_stacktoheap(&tmp); \
    })

/**
 * @brief Append a serialized primitive value to an existing fs2.
 *
 * Uses `fs2_sprintf_concat` with the appropriate format specifier.
 */
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

/* ─────────────────────────────────────────────────────────────────
 * String search API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief Find the nth occurrence of @p pattern starting at @p pos.
 *
 * @param str      Haystack (must not be NULL, must have v != NULL).
 * @param pattern  NUL-terminated needle (must not be NULL).
 * @param pos      Start position (search begins here).
 * @param nth      1-based occurrence number (1 = first, 0 = invalid).
 * @return         Byte offset in @p str, or -1 if not found / invalid.
 *
 * @note Overlapping matches count separately: "aaa" / "aa" with nth=2
 *       returns 1 (not -1), because "aa" occurs at positions 0 and 1.
 * @note Empty pattern is invalid: returns -1.
 */
ptrdiff_t
fs2_instr_posn(const fs2 *restrict str, const fs2 *restrict pattern,
               size_t pos, size_t nth);

 /**
 * @brief Case Insesitive Find the nth occurrence of @p pattern starting at @p pos.
 *
 * @param str      Haystack (must not be NULL, must have v != NULL).
 * @param pattern  NUL-terminated needle (must not be NULL).
 * @param pos      Start position (search begins here).
 * @param nth      1-based occurrence number (1 = first, 0 = invalid).
 * @return         Byte offset in @p str, or -1 if not found / invalid.
 *
 * @note Overlapping matches count separately: "aaa" / "aa" with nth=2
 *       returns 1 (not -1), because "aa" occurs at positions 0 and 1.
 * @note Empty pattern is invalid: returns -1.
 */              
ptrdiff_t
fs2_instr_posn_ci(const fs2 *restrict str, const fs2 *restrict pattern,
                  size_t pos, size_t nth);               

/**
 * @brief Case-sensitive: find the first occurrence of @p pattern from @p pos.
 *
 * @param str      Non-NULL fs2.
 * @param pattern  Non-NULL fs2.
 * @param pos      Start position.
 * @return         Byte offset, or -1.
 */
static inline ptrdiff_t
fs2_instr_from(const fs2 *restrict str, const fs2 *restrict pattern, size_t pos) {
    return fs2_instr_posn(str, pattern, pos, 1);
}

/**
 * @brief Case-sensitive: find the nth occurrence of @p pattern from the start.
 *
 * @param str      Non-NULL fs2.
 * @param pattern  Non-NULL fs2.
 * @param nth      1-based occurrence number.
 * @return         Byte offset, or -1.
 */
static inline ptrdiff_t
fs2_instr_nth(const fs2 *restrict str, const fs2 *restrict pattern, size_t nth) {
    return fs2_instr_posn(str, pattern, 0, nth);
}

/**
 * @brief Case-sensitive: find the first occurrence of @p pattern.
 *
 * @param str      Non-NULL fs2.
 * @param pattern  Non-NULL fs2.
 * @return         Byte offset, or -1.
 */
static inline ptrdiff_t
fs2_instr(const fs2 *restrict str, const fs2 *restrict pattern) {
    return fs2_instr_posn(str, pattern, 0, 1);
}

/**
 * @brief Case-insensitive: find the first occurrence of @p pattern from @p pos.
 *
 * @param str      Non-NULL fs2.
 * @param pattern  Non-NULL fs2.
 * @param pos      Start position.
 * @return         Byte offset, or -1.
 */
static inline ptrdiff_t
fs2_instr_from_ci(const fs2 *restrict str, const fs2 *restrict pattern, size_t pos) {
    return fs2_instr_posn_ci(str, pattern, pos, 1);
}

/**
 * @brief Case-insensitive: find the nth occurrence of @p pattern from the start.
 *
 * @param str      Non-NULL fs2.
 * @param pattern  Non-NULL fs2.
 * @param nth      1-based occurrence number.
 * @return         Byte offset, or -1.
 */
static inline ptrdiff_t
fs2_instr_nth_ci(const fs2 *restrict str, const fs2 *restrict pattern, size_t nth) {
    return fs2_instr_posn_ci(str, pattern, 0, nth);
}

/**
 * @brief Case-insensitive: find the first occurrence of @p pattern.
 *
 * @param str      Non-NULL fs2.
 * @param pattern  Non-NULL fs2.
 * @return         Byte offset, or -1.
 */
static inline ptrdiff_t
fs2_instr_ci(const fs2 *restrict str, const fs2 *restrict pattern) {
    return fs2_instr_posn_ci(str, pattern, 0, 1);
}

// -------------------------- C-str wrappers -----------------------------------

static inline ptrdiff_t
fs2_instr_posn_str(const fs2 *restrict str, const char *restrict pattern, size_t pos, size_t nth) {
    fs2 wrapped = fs2_wrap(pattern);
    return fs2_instr_posn(str, &wrapped, pos, nth);
}

static inline ptrdiff_t
fs2_instr_posn_str_ci(const fs2 *restrict str, const char *restrict pattern, size_t pos, size_t nth) {
    fs2 wrapped = fs2_wrap(pattern);
    return fs2_instr_posn_ci(str, &wrapped, pos, nth);
}
// wrapper over fs2_instr_posn_str!!, non-ci
static inline ptrdiff_t
fs2_instr_from_str(const fs2 *restrict str, const char *restrict pattern, size_t pos) {
    return fs2_instr_posn_str(str, pattern, pos, 1);
}

static inline ptrdiff_t
fs2_instr_nth_str(const fs2 *restrict str, const char *restrict pattern, size_t nth) {
    return fs2_instr_posn_str(str, pattern, 0, nth);
}

static inline ptrdiff_t
fs2_instr_str(const fs2 *restrict str, const char *restrict pattern) {
    return fs2_instr_posn_str(str, pattern, 0, 1);
}
//wrappers over fs2_instr_posn_str_ci! ci
static inline ptrdiff_t
fs2_instr_from_str_ci(const fs2 *restrict str, const char *restrict pattern, size_t pos) {
    return fs2_instr_posn_str_ci(str, pattern, pos, 1);
}
static inline ptrdiff_t
fs2_instr_nth_str_ci(const fs2 *restrict str, const char *restrict pattern, size_t nth) {
    return fs2_instr_posn_str_ci(str, pattern, 0, nth);
}
static inline ptrdiff_t
fs2_instr_str_ci(const fs2 *restrict str, const char *restrict pattern) {
    return fs2_instr_posn_str_ci(str, pattern, 0, 1);
}

/**
 * @brief Replace all non-overlapping occurrences of @p pat with @p repl.
 *
 * Non-overlapping like Python's str.replace: "aaa" with pat="aa", repl="b"
 * gives "ba" (one replacement at position 0, next search starts after it).
 *
 * @pre  fs2_is_alloc(s)    (LOCAL/STATIC cannot grow)
 * @pre  s, pat, repl are distinct objects (restrict)
 * @pre  !fs2_is_empty(pat) OR no-op (empty pattern returns s unchanged)
 *
 * @return s (for chaining).
 */
extern fs2 *
fs2_replace(fs2 *restrict s, const fs2 *restrict pat, const fs2 *restrict repl);
// wrapper, c-str ports
// fs2 -> c-str
static inline fs2 *
fs2_replace_fromstr(fs2 *restrict s, const fs2 *restrict pat, const char *restrict repl) {
    fs2 tmp = fs2_wrap(repl);
    return fs2_replace(s, pat, &tmp);
}
// c-str -> c-str
/**
 * @brief Replace all occurrences of @p pat (c-string) with @p repl (c-string).
 *
 * Convenience wrapper for the most common case: both pattern and
 * replacement are plain C-strings. For mixed fs2/c-string arguments
 * use fs2_replace (fs2/fs2) or fs2_replace_fromstr (fs2/c-string).
 *
 * NULL pat → no-op. NULL/empty repl → delete.
 */
static inline fs2 *
fs2_replace_cstr(fs2 *restrict s, const char *restrict pat, const char *restrict repl) {
    fs2 tmp = fs2_wrap(pat);
    return fs2_replace_fromstr(s, &tmp, repl);
}

/**
 * @brief Trim characters from the set @p pattern at the edges of @p s.
 *
 * @param left   trim from the start
 * @param right  trim from the end
 *
 * Pattern is a character set (like strspn's accept), NOT a substring.
 * `fs2_trim("  hello  ", " ")` → "hello". 
 * `fs2_trim("xxxhello", "x")` → "hello".
 *
 * @return s (for chaining).
 */
extern fs2 *
fs2_trim_common(fs2 *restrict s, const fs2 *restrict pattern, bool left, bool right);

static inline fs2 *
fs2_trim(fs2 *restrict s, const fs2 *restrict pattern) {
    return fs2_trim_common(s, pattern, true, true);
}

static inline fs2 *
fs2_ltrim(fs2 *restrict s, const fs2 *restrict pattern) {
    return fs2_trim_common(s, pattern, true, false);
}

static inline fs2 *
fs2_rtrim(fs2 *restrict s, const fs2 *restrict pattern) {
    return fs2_trim_common(s, pattern, false, true);
}

/* c-string pattern wrappers */
static inline fs2 *
fs2_trim_str(fs2 *restrict s, const char *restrict pattern) {
    fs2 tmp = fs2_wrap(pattern);
    return fs2_trim(s, &tmp);
}

static inline fs2 *
fs2_ltrim_str(fs2 *restrict s, const char *restrict pattern) {
    fs2 tmp = fs2_wrap(pattern);
    return fs2_ltrim(s, &tmp);
}

static inline fs2 *
fs2_rtrim_str(fs2 *restrict s, const char *restrict pattern) {
    fs2 tmp = fs2_wrap(pattern);
    return fs2_rtrim(s, &tmp);
}

static inline bool
fs2_starts_with(const fs2 *restrict s, const fs2 *restrict prefix) {
    invraise(s != NULL && prefix != NULL, "Null pointers %p %p", s, prefix);
    if (prefix->len == 0)        
        return true;
    if (prefix->len > s->len)    
        return false;
    return memcmp(s->v, prefix->v, prefix->len) == 0;
}

static inline bool
fs2_ends_with(const fs2 *restrict s, const fs2 *restrict suffix) {
    invraise(s != NULL && suffix != NULL, "Null pointers %p %p", s, suffix);
    if (suffix->len == 0)        
        return true;
    if (suffix->len > s->len)    
        return false;
    return memcmp(s->v + s->len - suffix->len, suffix->v, suffix->len) == 0;
}

/* c-string wrappers */
static inline bool
fs2_starts_with_str(const fs2 *restrict s, const char *restrict prefix) {
    fs2 tmp = fs2_wrap(prefix);
    return fs2_starts_with(s, &tmp);
}

static inline bool
fs2_ends_with_str(const fs2 *restrict s, const char *restrict suffix) {
    fs2 tmp = fs2_wrap(suffix);
    return fs2_ends_with(s, &tmp);
}
// 
static inline bool
fs2_contains(const fs2 *restrict s, const fs2 *restrict pat) {
    return fs2_instr_posn(s, pat, 0, 1) >= 0;
}
// c-str wrap
static inline bool
fs2_contains_str(const fs2 *restrict s, const char *restrict pat) {
    fs2 tmp = fs2_wrap(pat);
    return fs2_contains(s, &tmp);
}
// count
/**
 * @return Number of non-overlapping occurrences of @p pat.
 *         Returns 0 if @p pat is empty (unlike Python's len+1, which
 *         has no practical use and would require special-casing).
 */
static inline size_t
fs2_count(const fs2 *restrict s, const fs2 *restrict pat) {
    invraise(s != NULL && pat != NULL, "Null pointers %p %p", s, pat);
    if (fs2_is_empty(pat)) return 0;   /* no non-overlapping occurrences of "" */
    
    size_t    count = 0;
    size_t    pos   = 0;
    ptrdiff_t hit;
    
    while ((hit = fs2_instr_posn(s, pat, pos, 1)) >= 0) {
        count++;
        pos = (size_t)hit + pat->len;   /* non-overlapping */
    }
    return count;
}
// c-str wrap
static inline size_t
fs2_count_str(const fs2 *restrict s, const char *restrict pat) {
    fs2 tmp = fs2_wrap(pat);
    return fs2_count(s, &tmp);
}

// Repeat  Stack from fs2
static inline fs2
fs2_repeat_new(const fs2 *restrict s, size_t n) {
    invraise(s != NULL, "Null pointer");
    if (fs2_is_empty(s) || n == 0)  
        return FS2();
    
    /* Overflow: n * s->len (и +1 для NUL) */
    if (n > (SIZE_MAX - 1) / s->len) 
        userraise(-1, ERR_OUT_OF_RANGE, 
                 "fs2_repeat_new: n * len overflows size_t");
    
    return fs2_rpad_new(&FS2EMPTY, n * s->len, s);
}

// in-place
extern fs2 *
fs2_repeat(fs2 *restrict s, size_t n);

/* ========== Heap from fs2 ========== */

static inline fs2 *
fs2_heap_repeat(const fs2 *restrict s, size_t n) {
    fs2 tmp = fs2_repeat_new(s, n);
    return fs2_stacktoheap(&tmp);
}

/* ========== Stack from c-string ========== */

static inline fs2
fs2_repeat_str_new(const char *restrict s, size_t n) {
    fs2 tmp = fs2_wrap(s);
    return fs2_repeat_new(&tmp, n);
}

/* ========== Heap from c-string ========== */

static inline fs2 *
fs2_heap_repeat_str(const char *restrict s, size_t n) {
    fs2 tmp = fs2_repeat_str_new(s, n);
    return fs2_stacktoheap(&tmp);
}

// prefix/suffix removers

static inline fs2 *
fs2_strip_prefix(fs2 *restrict s, const fs2 *restrict prefix) {
    invraise(s != NULL && prefix != NULL, "Null pointers %p %p", s, prefix);
    if (fs2_is_empty(prefix))         
        return s;
    if (!fs2_starts_with(s, prefix))  
        return s;
    
    /* Shift remainder left: [prefix->len, s->len) → [0, s->len - prefix->len) */
    fs2_selfmove(s, 0, prefix->len, s->len - prefix->len);
    return fs2_setlen(s, s->len - prefix->len);
}

static inline fs2 *
fs2_strip_suffix(fs2 *restrict s, const fs2 *restrict suffix) {
    invraise(s != NULL && suffix != NULL, "Null pointers %p %p", s, suffix);
    if (fs2_is_empty(suffix))         
        return s;
    if (!fs2_ends_with(s, suffix))    
        return s;
    
    /* Only shrink — no shift needed */
    return fs2_setlen(s, s->len - suffix->len);
}

/* c-string wrappers */
static inline fs2 *
fs2_strip_prefix_str(fs2 *restrict s, const char *restrict prefix) {
    fs2 tmp = fs2_wrap(prefix);
    return fs2_strip_prefix(s, &tmp);
}

static inline fs2 *
fs2_strip_suffix_str(fs2 *restrict s, const char *restrict suffix) {
    fs2 tmp = fs2_wrap(suffix);
    return fs2_strip_suffix(s, &tmp);
}

/* ==================== Stack constructor ==================== */

static inline fs2
fs2_strip_prefix_new(const fs2 *restrict src, const fs2 *restrict prefix) {
    invraise(src != NULL && prefix != NULL, "Null pointers %p %p", src, prefix);
    fs2 r = fs2_clone(src);
    fs2_strip_prefix(&r, prefix);
    return r;
}

static inline fs2
fs2_strip_suffix_new(const fs2 *restrict src, const fs2 *restrict suffix) {
    invraise(src != NULL && suffix != NULL, "Null pointers %p %p", src, suffix);
    fs2 r = fs2_clone(src);
    fs2_strip_suffix(&r, suffix);
    return r;
}

static inline fs2
fs2_strip_prefix_str_new(const fs2 *restrict src, const char *restrict prefix) {
    fs2 tmp = fs2_wrap(prefix);
    return fs2_strip_prefix_new(src, &tmp);
}

static inline fs2
fs2_strip_suffix_str_new(const fs2 *restrict src, const char *restrict suffix) {
    fs2 tmp = fs2_wrap(suffix);
    return fs2_strip_suffix_new(src, &tmp);
}

/* ==================== Heap constructor ==================== */

static inline fs2 *
fs2_heap_strip_prefix(const fs2 *restrict src, const fs2 *restrict prefix) {
    fs2 tmp = fs2_strip_prefix_new(src, prefix);
    return fs2_stacktoheap(&tmp);
}

static inline fs2 *
fs2_heap_strip_suffix(const fs2 *restrict src, const fs2 *restrict suffix) {
    fs2 tmp = fs2_strip_suffix_new(src, suffix);
    return fs2_stacktoheap(&tmp);
}

/**
 * @brief Capitalize first letter of each word, lowercase the rest.
 *
 * Words are delimited by non-alphanumeric characters, matching
 * PostgreSQL initcap('hi THOMAS') → 'Hi Thomas'.
 *
 * @note ASCII-only in C locale. Bytes >= 128 are treated as
 *       non-alphanumeric (word separators), like PostgreSQL's
 *       default behavior without ICU collation.
 */
extern fs2 *
fs2_initcap(fs2 *restrict s);

#endif /* !FS2_API_H */
