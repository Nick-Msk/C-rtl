/**
 * @file fs2_api.c
 * @brief Non-inline implementations for fs2_api.h:
 *        reverse, partialsort, and N-occurrence search functions.
 */

#define FS2_INTERNAL_UTILS

#include "fs2_api.h"
#include <locale.h>

// Utilities

static inline size_t
fs2_fill_table(bool *restrict tbl,  size_t cnt, const char *restrict chars) {
    size_t total = 0UL;
    for (size_t i = 0; chars[i]; i++) {
        size_t  ind = (unsigned char)chars[i];
        if (ind < cnt && !tbl[ind])
            tbl[ind] = true, total++;
    }
    return total;
}

/**
 * @brief Reverse the string in place.
 *
 * Swaps bytes from both ends toward the center.
 *
 * @pre    str is not FS2_STATIC; str->len <= str->sz.
 *
 * @param str  Non-NULL fs2 (ALLOC or LOCAL).
 * @return     @p str (for chaining).
 */
fs2*             
fs2_flip(fs2 *str) {
    if (fs2_cant_mutate(str))
        return str;

    size_t i = 0UL, j = str->len - 1;
    while (i < j)
        char_exch(str->v + i++, str->v + j--);
    return str;
}

/**
 * @brief Sort a sub-range [from, to) of the string in place.
 *
 * Uses qsort with byte-wise comparison (ascending or descending).
 * Bound-clamps: if to > len, to is set to len; if from >= to, no-op.
 *
 * @pre    str is not FS2_STATIC.
 *
 * @param str   Non-NULL fs2 (ALLOC or LOCAL).
 * @param from  Start offset (inclusive).
 * @param to    End offset (exclusive); clamped to str->len.
 * @param asc   true = ascending, false = descending.
 */
void             
fs2_partialsort(fs2 *str, size_t from, size_t to, bool asc) {
    if (fs2_cant_mutate(str))
        return;

    if (to > str->len)
        to = str->len;
    if (from < to)
        qsort(str->v + from, to - from, 1, asc ? pchar_cmp: pchar_revcmp);
}

// ------------------------------------------ ONE SYMBOL, N occurence SEARCH  --------------------------------------------------------

/* forward, sensitive */

/**
 * @brief Find the k-th (1-based) forward occurrence of byte @p c.
 *
 * Delegates to fs2_nchr_nth with lim = str->len.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param k    1-based occurrence number (0 → -1).
 * @return     0-based offset, or -1 if fewer than k occurrences.
 */
ptrdiff_t   
fs2_chr_nth   (const fs2 *str, int c, size_t k) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null pointer");

    return fs2_nchr_nth(str, c, k, str->len);
} 
/**
 * @brief Find the k-th (1-based) forward occurrence of @p c within [0, lim).
 *
 * Iteratively skips past each found occurrence until the k-th is located.
 * @p k is 1-based: k=1 finds the first occurrence.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-sensitive).
 * @param k    1-based occurrence number (0 → -1).
 * @param lim  Maximum bytes to examine (clamped to str->len).
 * @return     0-based offset from string start, or -1.
 */
ptrdiff_t
fs2_nchr_nth  (const fs2 *str, int c, size_t k, size_t lim) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null pointer");

    if (k == 0 || str->v == NULL) 
        return -1;                        // 1-based

    const unsigned char *base = (const unsigned char *)str->v;
    const unsigned char *orig = base;
    size_t avail = (lim < str->len) ? lim : str->len;

    for (size_t i = 0; i < k; i++) {
        ptrdiff_t hit = __fs2_chr_raw(base, avail, c);
        if (hit < 0)
            return -1;
        if (i == k - 1) 
            return (ptrdiff_t)(base - orig) + hit;
        avail -= (size_t)hit + 1;
        base  += hit + 1;
    }
    return -1;
}

/* forward, insensitive */

/**
 * @brief Find the k-th (1-based) forward case-insensitive occurrence of @p c.
 *
 * Delegates to fs2_nchr_ci_nth with lim = str->len.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based occurrence number (0 → -1).
 * @return     0-based offset, or -1.
 */
ptrdiff_t
fs2_chr_ci_nth  (const fs2 *str, int c, size_t k) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null pointer");

    return fs2_nchr_ci_nth(str, c, k, str->len);
}

/**
 * @brief Find the k-th (1-based) forward case-insensitive occurrence within [0, lim).
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based occurrence number (0 → -1).
 * @param lim  Maximum bytes to examine (clamped to str->len).
 * @return     0-based offset from string start, or -1.
 */
ptrdiff_t
fs2_nchr_ci_nth (const fs2 *str, int c, size_t k, size_t lim) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null pointer");
    
    if (k == 0 || str->v == NULL)
        return -1;

    const unsigned char *base = (const unsigned char *)str->v;
    const unsigned char *orig = base;
    size_t avail = (lim < str->len) ? lim : str->len;

    for (size_t i = 0; i < k; i++) {
        ptrdiff_t hit = __fs2_chr_raw_ci(base, avail, c);
        if (hit < 0)
            return -1;
        if (i == k - 1)
            return (ptrdiff_t)(base - orig) + hit;
        avail -= (size_t)hit + 1;
        base  += hit + 1;
    }
    return -1;
}

// ------------------------------------------ ONE SYMBOL, N occurence REVERSE SEARCH  --------------------------------------------------------

/* reverse, sensitive */

/**
 * @brief Find the k-th (1-based) reverse case-sensitive occurrence of @p c.
 *
 * Scans from the end: k=1 is the last occurrence, k=2 the second-to-last, etc.
 * Delegates to fs2_nrchr_nth with lim = str->len.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param k    1-based reverse occurrence number (0 → -1).
 * @return     0-based offset, or -1.
 */
ptrdiff_t
fs2_rchr_nth  (const fs2 *str, int c, size_t k) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null pointer");

    return fs2_nrchr_nth(str, c, k, str->len);
}

/**
 * @brief Find the k-th (1-based) reverse case-sensitive occurrence within the last @p lim bytes.
 *
 * Scans from the end of the window [len-lim, len) toward the start.
 * The returned offset is relative to the string start.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find.
 * @param k    1-based reverse occurrence number (0 → -1).
 * @param lim  Number of trailing bytes to examine (clamped to str->len).
 * @return     0-based offset from string start, or -1.
 */
ptrdiff_t
fs2_nrchr_nth (const fs2 *str, int c, size_t k, size_t lim) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null pointer");

    if (k == 0 || str->v == NULL)
        return -1;

    size_t avail = (lim < str->len) ? lim : str->len;
    size_t off   = str->len - avail;
    const unsigned char *base = (const unsigned char *)str->v + off;
    size_t end   = avail;                        // exclusive end within window

    for (size_t i = 0; i < k; i++) {
        ptrdiff_t hit = __fs2_rchr_raw(base, end, c);
        if (hit < 0)
            return -1;
        if (i == k - 1)
            return (ptrdiff_t)off + hit;
        end = (size_t)hit;                       // search [0, hit) next
    }
    return -1;
}

/* reverse, insensitive */

/**
 * @brief Find the k-th (1-based) reverse case-insensitive occurrence of @p c.
 *
 * Scans from the end. Delegates to fs2_nrchr_ci_nth with lim = str->len.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based reverse occurrence number (0 → -1).
 * @return     0-based offset, or -1.
 */
ptrdiff_t
fs2_rchr_ci_nth (const fs2 *str, int c, size_t k) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null pointer");

    return fs2_nrchr_ci_nth(str, c, k, str->len);
}

/**
 * @brief Find the k-th (1-based) reverse case-insensitive occurrence within the last @p lim bytes.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based reverse occurrence number (0 → -1).
 * @param lim  Number of trailing bytes to examine (clamped to str->len).
 * @return     0-based offset from string start, or -1.
 */
ptrdiff_t
fs2_nrchr_ci_nth(const fs2 *str, int c, size_t k, size_t lim) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null pointer");

    if (k == 0 || str->v == NULL)
        return -1;

    size_t avail = (lim < str->len) ? lim : str->len;
    size_t off   = str->len - avail;
    const unsigned char *base = (const unsigned char *)str->v + off;
    size_t end   = avail;

    for (size_t i = 0; i < k; i++) {
        ptrdiff_t hit = __fs2_rchr_raw_ci(base, end, c);
        if (hit < 0)
            return -1;
        if (i == k - 1)
            return (ptrdiff_t)off + hit;
        end = (size_t)hit;
    }
    return -1;
}

// ------------------------------------ CAT/CPY/ETC -----------------------------------------

/**
 * @brief Append @p src to @p dst in place.
 *
 * Grows the buffer of @p dst via `fs2_reserve`.
 * Empty @p src is a no-op.
 *
 * @param dst  Non-NULL fs2 (ALLOC or LOCAL).
 * @param src  Non-NULL fs2 (any flavor). Not modified.
 * @return     @p dst.
 */
fs2 *
fs2_cat(fs2 *restrict dst, const fs2 *restrict src) {
    // guargs
    invfatal(ERR_NULL_INPUT, src != NULL, "fs2_cat: NULL src");
    if(fs2_cant_realloc(dst) || src->len == 0)  // was is_empty(src)
        return dst;
            
    fs2_memcpy(dst, dst->len, src, 0, src->len);
    
    fs2_setlen(dst, dst->len + src->len);
    return dst;
}

/**
 * @brief Prepend a C-string @p beginner to @p dst in place.
 *
 * Moves existing content forward and copies @p beginner at position 0.
 * Empty @p beginner is a no-op.
 *
 * @param dst       Non-NULL fs2 (ALLOC or LOCAL).
 * @param beginner  Non-NULL NUL-terminated string.
 * @return          @p dst.
 *
 * @warning `invfatal` (SIGABRT) if @p dst or @p beginner is NULL,
 *          or if @p dst is non-empty STATIC.
 */
fs2 *
fs2_rev_catstr(fs2 *restrict dst, const char *restrict beginner) {
    invfatal(ERR_NULL_INPUT, dst != NULL && beginner != NULL, 
        "Null pointers %p %p", dst, beginner);

    size_t  len = strlen(beginner);
    if (len == 0)       // no any allocations if null or empty 
        return dst;

    fs2_selfmove(dst, len, 0, dst->len);
    fs2_memcpy_str(dst, 0, beginner, len);
    return fs2_setlen(dst, dst->len + len);
}

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
long                  
_fs2_vsprintf_position(fs2 *restrict s, size_t pos, const char *restrict fmt, va_list ap) {
    // no checking, internal
    va_list ap2; 
    va_copy(ap2, ap); 
    long    needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);

    if (needed < 0)
        return logsimpleerr(-1L, "vsnprintf length verify failed");
    
    size_t  lastpos = pos + 1 + needed;
    if (lastpos > s->sz || fs2_is_static(s))
        fs2_reserve(s, lastpos > 1 ? lastpos : 2);

    long     cnt = vsnprintf(s->v + pos, needed + 1, fmt, ap);

    return cnt;    
}

/**
 * @brief Format into an fs2, replacing its current content.
 *
 * @param s    Non-NULL fs2 (ALLOC). Content is overwritten.
 * @param fmt  Non-NULL format string.
 * @param ...  Format arguments.
 * @return     Number of characters written (excluding NUL), or -1 on error.
 */
long           
fs2_sprintf(fs2 *restrict s, const char *restrict fmt, ...) {
    invfatal(ERR_NULL_INPUT, fmt != 0, 
        "Null fmt string %p", fmt);
    if (fs2_cant_realloc(s))
        return -1L; // never here!

    va_list ap;
    va_start(ap, fmt);
    long res = _fs2_vsprintf_position(s, 0, fmt, ap);
    va_end(ap);

    if (res < 0)
        return logsimpleerr(res, "_fs2_vsprintf_position failed");  // just a log, no userraise
    
    fs2_setlen(s, res); // fix the length!!!
    return res;
}

/**
 * @brief Append formatted text to the current content of an fs2.
 *
 * Unlike fs2_sprintf, this concatenates: the existing content is kept
 * and the formatted result is appended at the end.
 *
 * @param s    Non-NULL fs2 (ALLOC). Content is preserved, new text appended.
 * @param fmt  Non-NULL format string.
 * @param ...  Format arguments.
 * @return     Number of characters appended (excluding NUL), or -1 on error.
 */
long           
fs2_sprintf_concat(fs2 *restrict s, const char *restrict fmt, ...) {
    invfatal(ERR_NULL_INPUT,fmt != 0, 
        "Null fmt string %p", fmt);
    if (fs2_cant_realloc(s))
        return -1L; // never here!

    va_list ap; 
    va_start(ap, fmt);
    long res = _fs2_vsprintf_position(s, s->len, fmt, ap);
    va_end(ap);

    if (res < 0)
        return logsimpleerr(res, "_fs2_vsprintf_position failed");

    fs2_setlen(s, s->len + res); // required: _fs2_vsprintf_position never touches len 

    return res;
}

/**
 * @brief Replace content of @p s with the substring [from, from+len).
 *
 * In-place: moves the selected bytes to the beginning and truncates.
 * Bounds are clamped via `fs2_substr_bounds`.
 *
 * @param s     Non-NULL fs2 (ALLOC or LOCAL). Modified in place.
 * @param from  Start offset (clamped to s->len).
 * @param len   Number of bytes to keep (clamped to remaining tail).
 * @return      @p s.
 */
fs2 *
fs2_substr(fs2 *s, size_t from, size_t len) {
    if (fs2_cant_mutate(s))
        return s;

    fs2_substr_bounds(s, &from, &len);      // normalization

    fs2_selfmove(s, 0, from, len);
    fs2_setlen(s, len);
    return s;
}

/**
 * @brief Return a stack-allocated fs2 containing the substring [from, from+len).
 *
 * Does NOT modify @p s (works with STATIC fs2 as well).
 *
 * @param s     Non-NULL fs2 (any flavor). Not modified.
 * @param from  Start offset (clamped).
 * @param len   Number of bytes to extract (clamped).
 * @return      A new fs2 (stack, FS2_BODYALLOC body).
 */
fs2 
fs2_substr_new(const fs2 *s, size_t from, size_t len) {
    invfatal(ERR_NULL_INPUT, s != NULL, "Null pointer");

    fs2_substr_bounds(s, &from, &len);        
    
    if (len == 0)
        return FS2EMPTY;   /* constructor contract: empty → FS2EMPTY */

    fs2 res = FS2();
    fs2_memcpy(&res, 0, s, from, len);
    fs2_setlen(&res, len);
    return res;
}

/**
 * @brief Return a heap-allocated fs2 containing the substring [from, from+len).
 *
 * @param s     Non-NULL fs2 (any flavor). Not modified.
 * @param from  Start offset (clamped).
 * @param len   Number of bytes to extract (clamped).
 * @return      A new heap-allocated fs2.
 */
fs2 *
fs2_heap_substr(const fs2 *s, size_t from, size_t len) {
    fs2 *res = fs2_heap();
    *res = fs2_substr_new(s, from, len);
    res->flags |= FS2_BODYALLOC;
    return res;
}

// ------------------------------- Upper / Lower case ---------------------------------------
/**
 * @brief Convert all bytes to upper case in place (byte-wise, NOT Unicode-aware).
 *
 * NULL-safe: returns NULL unchanged.
 *
 * @param str  fs2 (ALLOC or LOCAL).
 * @return     @p str.
 */
fs2 *
fs2_toupper(fs2 *str) {
    if (fs2_cant_mutate(str))
        return str;

    for (size_t i = 0; i < str->len; i++)
        str->v[i] = toupper( (unsigned char) str->v[i]);
    return str;
}

/**
 * @brief Convert all bytes to lower case in place (byte-wise, NOT Unicode-aware).
 *
 * NULL-safe: returns NULL unchanged.
 *
 * @param str  fs2 (ALLOC or LOCAL).
 * @return     @p str.
 */
fs2 *
fs2_tolower(fs2 *str) {
    if (fs2_cant_mutate(str))
        return str;

    for (size_t i = 0; i < str->len; i++)
        str->v[i] = tolower( (unsigned char) str->v[i]);
    return str;
}

// ------------------------------------- Padding -------------------------------------------

/**
 * @brief Right-pad @p str to total length @p len using bytes from @p pad (in-place).
 *
 * If `len < str->len`, the string is truncated via `fs2_left`.
 * If `pad->len == 0` or `len == str->len`, no change.
 *
 * @param str  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param len  Target total length.
 * @param pad  Non-NULL non-empty fs2 used as the repeating pad pattern.
 * @return     @p str.
 */
fs2 *                    
fs2_rpad(fs2 *restrict str, size_t len, const fs2 *restrict pad) {
    if (fs2_cant_realloc(str))   // NULL / non-empty STATIC → abort; empty (FS2()/FS2EMPTY) → OK, will be promoted
        return str;
    invfatal(ERR_NULL_INPUT, pad != NULL, "Null pad - unable to rpad");

    if (len < str->len)
        return fs2_left(str, len);

    if (pad->len == 0 || len == str->len)  // empty - nothing to PAD
        return str;

    fs2_reserve(str, len + 1);  // make sure mem is allocated

    size_t i = str->len, j = 0;
    // a stupid iteration over pad (non-opt)
    while (i < len) {
        str->v[i++] = pad->v[j++];
        if (j == pad->len)
            j = 0;
    }

    return fs2_setlen(str, len);
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
 */
fs2 *                    
fs2_lpad(fs2 *restrict str, size_t len, const fs2 *restrict pad) {
    if (fs2_cant_realloc(str))   // NULL / non-empty STATIC → abort; empty (FS2()/FS2EMPTY) → OK, will be promoted
        return str;
    invfatal(ERR_NULL_INPUT, pad != NULL, "Null pad - unable to lpad");

    if (len < str->len)
        return fs2_left(str, len);

    if (pad->len == 0 || len == str->len)  // empty - nothing to PAD
        return str;

    fs2_reserve(str, len + 1);  // make sure mem is allocated

    size_t pad_len = len - str->len;
    fs2_selfmove(str, pad_len, 0, str->len);

    size_t  i = 0, j = 0;
    // a stupid iteration over pad (non-opt)
    while (i < pad_len) {
        str->v[i++] = pad->v[j++];
        if (j == pad->len)
            j = 0;
    }

    return fs2_setlen(str, len);
}
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
fs2 *           
fs2_genrnd(fs2 *s, size_t len, char type) {
    if(fs2_cant_realloc(s))
        return s;

    fs2_reserve(s, len + 1);

    for (size_t i = 0; i < len; i++) {
        int     c;
        switch(type) {
            case 'a':   c = 'a' + rndint(26 - 1);
                break;
            case 'A':   c = 'A' + rndint(26 - 1);
                break;
            case '0':   c = '0' + rndint(10 - 1);
                break;
            // THINK: next? alnum...
            default:    c = '?';
                break;
        }
        s->v[i] = c;
    }
    return fs2_setlen(s, len);
}

/* ─────────────────────────────────────────────────────────────────
 * Conversion (parse / format primitives), streaming
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
bool 
fs2_pos_parse_llong(const fs2 *restrict s, size_t pos, 
                    size_t *restrict newpos, long long *restrict out) {
    if (!s || !s->v || pos >= s->len) 
        return logsimpleerr(false, "Null input or pos %zu out of length", pos);

    const char *base  = s->v + pos;
    const char *start = skip_spaces(base);
    
    if (*start == '\0') 
        return logsimpleerr(false, "Only spaces detected");
    
    if (!isdigit((unsigned char)*start) && *start != '+' && *start != '-')
        return logsimpleerr(false, "Incorrect sym detected [%c]", *start);

    char *endptr = NULL;
    errno = 0;
    long long val = strtoll(start, &endptr, 10);

    if (errno == ERANGE || endptr == start)
        return logsimpleerr(false, 
            "Overflow or no digits (errno=%d, consumed=%td)", 
            errno, endptr - start);   /* ← use %td instead of %zu */

    if (out)
        *out = val;
    if (newpos)
        *newpos = pos + (size_t)(endptr - base);

    return true;
}

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
bool 
fs2_pos_parse_long(const fs2 *restrict s, size_t pos, 
                   size_t *restrict newpos, long *restrict out) {
    long long ll_val = 0;
    size_t    npos   = 0;

    if (!fs2_pos_parse_llong(s, pos, &npos, &ll_val))
        return false;   /* log was already emitted in the base function */

    if (ll_val < LONG_MIN || ll_val > LONG_MAX)
        return logsimpleerr(false, 
            "Value %lld out of long range [%ld, %ld]", 
            ll_val, LONG_MIN, LONG_MAX);

    if (out)    
        *out    = (long) ll_val;
    if (newpos) 
        *newpos = npos;

    return true;
}

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
bool 
fs2_pos_parse_int(const fs2 *restrict s, size_t pos, 
                  size_t *restrict newpos, int *restrict out) {
    long long   ll_val = 0;
    size_t      npos  = 0;

    if (!fs2_pos_parse_llong(s, pos, &npos, &ll_val))
        return false;

    if (ll_val < INT_MIN || ll_val > INT_MAX)
        return logsimpleerr(false, 
            "Value %lld out of int range [%d, %d]", 
            ll_val, INT_MIN, INT_MAX);

    if (out)    
        *out    = (int) ll_val;
    if (newpos) 
        *newpos = npos;

    return true;
}

/**
 * @brief Parse an unsigned long long starting at @p pos (streaming).
 *
 * Skips leading spaces. Rejects a leading minus sign. Uses `strtoull`.
 *
 * @param s       Non-NULL fs2 with v != NULL.
 * @param pos     Start offset.
 * @param newpos  Optional. Set to the position after the number.
 * @param out     Optional. If NULL, the value is discarded.
 * @return        true on success, false on invalid format or overflow.
 */
bool 
fs2_pos_parse_ullong(const fs2 *restrict s, size_t pos, size_t *restrict newpos, unsigned long long *restrict out) {
    if (!s || !s->v || pos >= s->len) 
        return logsimpleerr(false, "Null input or pos %zu out of length", pos);

    const char *base  = s->v + pos;
    const char *start = skip_spaces(base);
    
    if (*start == '\0') 
        return logsimpleerr(false, "Only spaces detected");

    /* Unsigned types do not accept a minus sign. strtoull("-1") = ULLONG_MAX —
     * that is not what the caller expects. */
    if (*start == '-')
        return logsimpleerr(false, "Negative sign detected");;

    /* Prevent strtoull from silently skipping \n, \r, \v, \f */
    if (!isdigit((unsigned char)*start) && *start != '+')
        return logsimpleerr(false, "incorrent sym detected [%c]", *start);

    char *endptr = NULL;
    errno = 0;
    unsigned long long val = strtoull(start, &endptr, 10);

    if (errno == ERANGE || endptr == start)
        return logsimpleerr(false, "Overflow or no digits %d, %zu", errno, endptr - start);

    if (out)
        *out = val;
    if (newpos)
        *newpos = pos + (size_t)(endptr - base);

    return true;
}

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
bool 
fs2_pos_parse_ulong(const fs2 *restrict s, size_t pos, 
                    size_t *restrict newpos, unsigned long *restrict out) {
    unsigned long long ull  = 0;
    size_t             npos = 0;

    if (!fs2_pos_parse_ullong(s, pos, &npos, &ull))
        return false;

    if (ull > ULONG_MAX)
        return logsimpleerr(false, "Parsed int is out of bound %llu <= %lu", ull, ULONG_MAX);

    if (out)
        *out = (unsigned long)ull;
    if (newpos)
        *newpos = npos;

    return true;
}

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
bool 
fs2_pos_parse_uint(const fs2 *restrict s, size_t pos, 
                   size_t *restrict newpos, unsigned *restrict out) {
    unsigned long long  ull   = 0;
    size_t              npos = 0;

    if (!fs2_pos_parse_ullong(s, pos, &npos, &ull))
        return false;

    if (ull > UINT_MAX)
        return logsimpleerr(false, "Parsed int is out of bound %llu <= %u", ull, UINT_MAX);

    if (out)
        *out = (unsigned) ull;
    if (newpos)
        *newpos = npos;

    return true;
}

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
bool 
fs2_pos_parse_double(const fs2 *restrict s, size_t pos, 
                     size_t *restrict newpos, double *restrict out) {
    if (!s || !s->v || pos >= s->len) 
        return logsimpleerr(false, "Null input or pos %zu out of length", pos);

    const char *base  = s->v + pos;
    const char *start = skip_spaces(base);
    
    if (*start == '\0') 
        return logsimpleerr(false, "Only spaces detected");

    /* Accept a digit, dot, sign, and also letters for nan/inf */
    unsigned char c = (unsigned char)*start;
    if (!isdigit(c) && c != '.' && c != '+' && c != '-' &&
        c != 'i' && c != 'I' && c != 'n' && c != 'N')
        return logsimpleerr(false, "Incorrect sym detected [%c]", c);

    char *endptr = NULL;
    errno = 0;
    
    double val = strtod(start, &endptr);

    if (errno == ERANGE || endptr == start)
        return logsimpleerr(false, 
            "Overflow or no digits (errno=%d, consumed=%td)", 
            errno, endptr - start);

    if (out)
        *out = val;
    if (newpos)
        *newpos = pos + (size_t)(endptr - base);

    return true;
}

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
bool 
fs2_pos_parse_char(const fs2 *restrict s, size_t pos, 
                   size_t *restrict newpos, char *restrict out) {
    if (!s || !s->v || pos >= s->len)
        return logsimpleerr(false, 
            "Null input or pos %zu out of length %zu", pos, s ? s->len : 0);
    
    if (out)    
        *out    = s->v[pos];
    if (newpos) 
        *newpos = pos + 1;
    
    return true;
}

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
bool 
fs2_pos_parse_short(const fs2 *restrict s, size_t pos, 
                    size_t *restrict newpos, short *restrict out) {
    long long ll = 0;                 /* ← long long, not long */
    size_t npos = 0;

    if (!fs2_pos_parse_llong(s, pos, &npos, &ll))
        return false;

    if (ll < SHRT_MIN || ll > SHRT_MAX)
        return logsimpleerr(false, 
            "Value %lld out of short range [%d, %d]", 
            ll, SHRT_MIN, SHRT_MAX);

    if (out)    
        *out    = (short)ll;
    if (newpos) 
        *newpos = npos;
    return true;
}

bool 
fs2_pos_parse_ushort(const fs2 *restrict s, size_t pos, 
                     size_t *restrict newpos, unsigned short *restrict out) {
    unsigned long long  ull   = 0;
    size_t              npos = 0;

    if (!fs2_pos_parse_ullong(s, pos, &npos, &ull))
        return false;

    if (ull > USHRT_MAX)
        return logsimpleerr(false, 
            "Value %llu out of unsigned short range [0, %u]", 
            ull, USHRT_MAX);

    if (out)    
        *out    = (unsigned short) ull;
    if (newpos) 
        *newpos = npos;

    return true;
}

/**
 * @brief Parse the whole string into an unsigned long long.
 *
 * @param s       Non-NULL fs2.
 * @param out     Output value (may be NULL to discard).
 * @param strict  If true, the entire string must be consumed.
 * @return        true on success.
 */
bool 
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
bool 
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
bool 
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
bool 
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
bool 
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
bool 
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
bool 
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
bool 
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
bool 
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
bool 
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

/* ─────────────────────────────────────────────────────────────────
 * String search API
 * ───────────────────────────────────────────────────────────────── */
/* Search function type: haystack -> position of first occurrence of needle,
 * or NULL if not found. Exactly like strstr/strcasestr. */
typedef char *(*fs2_findfn_t)(const char *haystack,
                                    const char *needle);

static inline ptrdiff_t
fs2_instr_pos_common_nth(const fs2 *restrict str, const fs2 *restrict pattern,
                       size_t pos, size_t nth, fs2_findfn_t find) {
    invfatal(ERR_NULL_INPUT, str != NULL && pattern != NULL, 
        "Null pointers %p %p", str, pattern);

    if (nth == 0)                 
        return -1;

    /* Empty pattern: matches at every position [pos, str->len].
     * Python semantics: "abc".find("", 2) → 2; "" matches at pos+nth-1. works even for FS2() since we don't deref v. */
    if (fs2_is_empty(pattern)) {
        if (pos > str->len || nth > str->len - pos + 1)
            return -1;
        return (ptrdiff_t)(pos + nth - 1);
    }

    if (str->v == NULL || pos >= str->len)
        return -1;

    size_t pat_len = pattern->len;
    if (pat_len > str->len - pos)
        return -1;
    
    const char *needle       = pattern->v;
    const char *haystack     = str->v + pos;
    const char *haystack_end = str->v + str->len - pat_len;
    size_t      count        = 0;
    
    while (haystack <= haystack_end) {
        const char *hit = find(haystack, needle);
        if (!hit)
            break;
        count++;
        if (count == nth)
            return (ptrdiff_t)(hit - str->v);
        haystack = hit + 1;
    }
    return -1;
}

/* Public wrappers pass their own finder */
ptrdiff_t
fs2_instr_pos_nth(const fs2 *restrict str, const fs2 *restrict pattern,
               size_t pos, size_t nth) {
    return fs2_instr_pos_common_nth(str, pattern, pos, nth, strstr);
}

ptrdiff_t
fs2_instr_pos_ci_nth(const fs2 *restrict str, const fs2 *restrict pattern,
                  size_t pos, size_t nth) {
    return fs2_instr_pos_common_nth(str, pattern, pos, nth, strcasestr);
}

/**
 * @brief Replace all non-overlapping occurrences of @p pat with @p repl in place.
 *
 * Non-overlapping, like Python's str.replace.
 * Empty @p pat is a no-op. NULL @p repl is treated as empty (delete).
 *
 * @param s     Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param pat   Non-NULL fs2 pattern.
 * @param repl  Non-NULL fs2 replacement (may be empty).
 * @return      @p s.
 */
fs2 *
fs2_replace(fs2 *restrict s, const fs2 *restrict pat, const fs2 *restrict repl) {
    invfatal(ERR_NULL_INPUT, s != NULL && pat != NULL && repl != NULL, 
             "Null pointers %p %p %p", s, pat, repl);
    
    if (fs2_is_empty(pat))  
        return s;
    if (pat->len > s->len)  
        return s;
    
    size_t pos = 0;
    while (pos < s->len) {
        ptrdiff_t hit = fs2_instr_from(s, pat, pos);
        if (hit < 0)
            break;
        
        size_t p       = (size_t) hit;
        size_t tail    = s->len - (p + pat->len);
        size_t new_len = s->len - pat->len + repl->len;
        
        /* Move only the tail bytes (NOT the NUL). fs2_cant_copy bounds src by
        * src->len and rejects reading v[len]; fs2_setlen() writes the NUL at
        * the new tail position, so moving it is unnecessary.
        * fs2_selfmove reserves tail+1 bytes at dstpos, which covers v[new_len];
        * fs2_memcpy separately reserves repl->len bytes at p. */
        fs2_selfmove(s, p + repl->len, p + pat->len, tail);
        
        /* Copy replacement. Skip if empty — memcpy(dst, NULL, 0) is UB
         * (FS2EMPTY has v="", but FS2() has v=NULL). */
        if (repl->len > 0) {
            fs2_memcpy(s, p, repl, 0, repl->len);
        }
        
        fs2_setlen(s, new_len);
        pos = p + repl->len;   /* non-overlapping, Python-style */
    }
    
    return s;
}

/**
 * @brief Trim characters from the set @p pattern at the edges of @p s.
 *
 * Pattern is a character set (like strspn's accept), NOT a substring.
 *
 * @param s        Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param pattern  Non-NULL fs2 — set of bytes to remove from edges.
 * @param left     Trim from the start.
 * @param right    Trim from the end.
 * @return         @p s.
 */
fs2 *
fs2_trim_common(fs2 *restrict s, const fs2 *restrict pattern,
                bool left, bool right) {
    invfatal(ERR_NULL_INPUT, s != NULL && pattern != NULL, 
        "Null pointers %p %p", s, pattern);
    
    if (fs2_cant_mutate(s) || fs2_is_empty(pattern))          //guards:  NULL / empty → no-op; non-empty STATIC → abort
        return s;
    
    size_t start = 0;
    size_t end   = s->len;
    
    if (left) {
        /* strspn: fast path, SIMD-accelerated in glibc. 
         * pattern->v has no embedded NUL (fs2 invariant), safe. */
        start = strspn(s->v, pattern->v);
        if (start == end) { fs2_setlen(s, 0); return s; }
    }
    
    if (right) {
        /* No reverse-strspn in stdlib — scan back with memchr per byte. */
        while (end > start) {
            unsigned char c = (unsigned char)s->v[end - 1];
            if (!memchr(pattern->v, c, pattern->len)) break;
            end--;
        }
    }
    
    /* Shift kept content to front (no-op if start == 0). */
    if (start > 0 && end > start) {
        fs2_selfmove(s, 0, start, end - start);
    }
    fs2_setlen(s, end - start);
    return s;
}

/**
 * @brief Return a stack-allocated fs2 containing @p n repetitions of @p s.
 *
 * Empty source or n == 0 returns an empty fs2.
 *
 * @param s  Non-NULL fs2 (any flavor). Not modified.
 * @param n  Number of repetitions.
 * @return   New stack fs2.
 *
 * @warning  `userraise` (SIGINT) if n * s->len overflows size_t.
 */
fs2
fs2_repeat_new(const fs2 *restrict s, size_t n) {
    invfatal(ERR_NULL_INPUT, s != NULL, 
        "Null pointer");
        
    if (fs2_is_empty(s) || n == 0)  
        return FS2();
    
    /* Overflow: n * s->len (and +1 for NUL) */
    if (n > (SIZE_MAX - 1) / s->len) 
        userraise(-1, ERR_OUT_OF_RANGE, 
                 "fs2_repeat_new: n * len overflows size_t");
    
    return fs2_rpad_new(&FS2EMPTY, n * s->len, s);
}

/**
 * @brief Repeat @p s in place @p n times.
 *
 * n == 0 clears the string. n == 1 is a no-op.
 *
 * @param s  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param n  Number of repetitions.
 * @return   @p s.
 *
 * @warning  `userraise` (SIGINT) if n * s->len overflows size_t.
 */
fs2 *
fs2_repeat(fs2 *restrict s, size_t n) {
    invfatal(ERR_NULL_INPUT, s != NULL, 
        "Null pointer");
    
    if (n == 0) { 
        fs2_setlen(s, 0); 
        return s; 
    }
    if (n == 1)
        return s;
    if (s->len == 0)
        return s;
    
    if (n > (SIZE_MAX - 1) / s->len)
        userraise(-1, ERR_OUT_OF_RANGE,
                 "fs2_repeat: %zu * %zu overflows size_t", n, s->len);
    
    size_t orig  = s->len;
    size_t total = orig * n;
    fs2_reserve(s, total + 1);
    
    /* Disjoint regions: [i*orig, (i+1)*orig) vs [0, orig). memcpy safe. */
    for (size_t i = 1; i < n; i++) {
        memcpy(s->v + i * orig, s->v, orig);
    }
    
    return fs2_setlen(s, total);
}

/**
 * @brief Capitalize the first letter of each word, lowercase the rest.
 *
 * Words are delimited by non-alphanumeric characters, matching
 * PostgreSQL initcap('hi THOMAS') → 'Hi Thomas'.
 *
 * @note Locale-dependent: uses isalnum/toupper/tolower from <ctype.h>.
 *       Uses the process-wide locale (C setlocale). Not safe if
 *       another thread calls setlocale concurrently.
 *
 * @param s  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @return   @p s.
 */
fs2 *
fs2_initcap(fs2 *restrict s) {
    if (fs2_cant_mutate(s))
        return s;
    
    bool at_word_start = true;
    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char) s->v[i];
        bool alnum = isalnum(c) != 0;
        
        if (at_word_start && alnum) {
            s->v[i] = (char) toupper(c);
        } else {
            s->v[i] = (char) tolower(c);
        }
        at_word_start = !alnum;
    }
    return s;
}

/**
 * @brief Capitalize the first alphabetic character of @p s, lowercase the rest.
 *
 * Unlike fs2_initcap (which capitalizes every word), this affects only the
 * very first alphanumeric byte; everything after is lowercased.
 *
 * "hELLO wORLD" → "Hello world"
 *
 * @param s  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @return   @p s.
 */
fs2 *
fs2_capitalize(fs2 *restrict s) {
    invfatal(ERR_NULL_INPUT, s != NULL, 
        "Null pointer");

    if (fs2_cant_mutate(s))
        return s;
    
    bool seen_first = false;
    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char) s->v[i];
        bool alnum = isalnum(c) != 0;
        
        if (!seen_first && alnum) {
            s->v[i] = (char) toupper(c);
            seen_first = true;
        } else {
            s->v[i] = (char) tolower(c);
        }
    }
    return s;
}

/**
 * @brief Zero-fill @p s to @p width in place.
 *
 * If the string begins with a sign ('+' or '-'), it is preserved at the
 * start and zeros are inserted after it. Width < len is a no-op.
 *
 * "42"  + width=6 → "000042"
 * "-7"  + width=6 → "-00007"
 *
 * @param s      Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param width  Target total length.
 * @return       @p s.
 */
fs2 *
fs2_zfill(fs2 *restrict s, size_t width) {
    invfatal(ERR_NULL_INPUT, s != NULL, 
        "Null pointer");

    if (width <= s->len)
        return s;
    fs2_cant_realloc(s);

    size_t old_len = s->len;
    size_t prefix  = (old_len > 0 && (s->v[0] == '-' || s->v[0] == '+')) ? 1 : 0;
    size_t pad_len = width - old_len;

    /* One reserve for the full target size; selfmove's own reserve is a
     * no-op afterwards. Using reserve explicitly (not just selfmove) is
     * required for the empty-body case: when old_len == prefix (e.g. "-"),
     * selfmove has len=0 → early return → no reserve; but we still need
     * to write pad_len '0' bytes. */
    fs2_reserve(s, width + 1);

    /* Shift content after the sign (if any) right by pad_len. */
    fs2_selfmove(s, prefix + pad_len, prefix, old_len - prefix);

    /* Fill the gap with '0'. */
    for (size_t i = 0; i < pad_len; i++)
        s->v[prefix + i] = '0';

    return fs2_setlen(s, width);
}

/**
 * @brief Map bytes of @p s: each byte in @p from is replaced by the byte
 *         at the same index in @p to; bytes present in @p from but with no
 *         corresponding position in @p to are deleted.
 *
 * Matches PostgreSQL translate() semantics: the FIRST occurrence of a byte
 * in @p from wins (reverse-scan of @p from builds the mapping table).
 *
 * @param s     Non-NULL fs2 (ALLOC or LOCAL). Modified (may shrink).
 * @param from  Non-NULL fs2 — source byte set.
 * @param to    Non-NULL fs2 — replacement byte set (may be shorter → delete).
 * @return      @p s.
 */
fs2 *
fs2_translate(fs2 *restrict s, const fs2 *restrict from, const fs2 *restrict to) {
    invfatal(ERR_NULL_INPUT, from != NULL && to != NULL, 
        "Null pointers %p %p", from, to);

    if (fs2_cant_mutate(s))
        return s;

    /* map[c] = 'to' for c in from, identity otherwise, '\0' = delete.
     * NUL-sentinel safe: fs2 has no embedded NUL (validate invariant). */
    char map[256];
    for (int i = 0; i < 256; i++)
        map[i] = (char)i;

    /* Reverse iteration → FIRST occurrence in `from` wins (PG semantics). */
    for (size_t i = from->len; i-- > 0; )
        map[(unsigned char) from->v[i]] = (i < to->len) ? to->v[i] : '\0';

    /* In-place compaction: out <= i always, no allocation, no growth. */
    size_t out = 0;
    for (size_t i = 0; i < s->len; i++) {
        char m = map[(unsigned char) s->v[i]];
        if (m) s->v[out++] = m;
    }
    return fs2_setlen(s, out);
}

/**
 * @brief Return the n-th (1-based) part of @p s split by @p delim.
 *
 * Positive n: 1-based from the start. Negative n: 1-based from the end
 * (n = -1 is the last part, n = -2 the second-to-last, etc.).
 * n = 0 returns an empty fs2.
 *
 * Matches PostgreSQL split_part() semantics.
 *
 * @param s      Non-NULL fs2 (any flavor). Not modified.
 * @param delim  Non-NULL fs2 delimiter (empty → no split).
 * @param n      1-based part number (negative = from the end, 0 → empty).
 * @return       New stack fs2 (FS2EMPTY if the part does not exist).
 */
fs2
fs2_split_part(const fs2 *restrict s, const fs2 *restrict delim, ptrdiff_t n) {
    invfatal(ERR_NULL_INPUT, s != NULL && delim != NULL,
             "Null pointers %p %p", s, delim);

    if (n == 0)
        return FS2EMPTY;

    if (fs2_is_empty(delim))
        return (n == 1 || n == -1) ? fs2_clone(s) : FS2EMPTY;

    if (n < 0) {
        n = (ptrdiff_t)(fs2_count(s, delim) + 1) + n + 1;
        if (n < 1)
            return FS2EMPTY;
    }

    /* start = byte after (n-1)-th delim (skip nth-scan when n == 1). */
    size_t start = 0;
    if (n > 1) {
        ptrdiff_t prev = fs2_instr_nth(s, delim, (size_t)(n - 1));
        if (prev < 0)
            return FS2EMPTY;
        start = (size_t)prev + delim->len;
    }

    /* First delim AT OR AFTER start -> end of our part.
     * No need to rescan the prefix; instr_from is O(remaining). */
    ptrdiff_t end = fs2_instr_from(s, delim, start);
    size_t    len = (end < 0) ? s->len - start : (size_t)end - start;

    return fs2_substr_new(s, start, len);
}

/**
 * @brief Remove all occurrences of any byte from @p chars in place.
 *
 * Uses a 256-byte membership table for O(n+m) single-pass compaction.
 *
 * @param s      Non-NULL fs2 (ALLOC or LOCAL). Non-empty STATIC aborts.
 * @param chars  Non-NULL fs2 — byte set to remove.
 * @return       @p s.
 *
 * @warning `invfatal` (SIGABRT) if @p s is non-empty STATIC,
 *          even when @p chars is empty.
 */
fs2 *
fs2_removeall(fs2 *restrict s, const fs2 *restrict chars) {
    invfatal(ERR_NULL_INPUT, chars != NULL,  "Null pointer");

    if (fs2_cant_mutate(s) || fs2_is_empty(chars))          //guards:  NULL / empty → no-op; non-empty STATIC → abort
        return s;

    /* Membership table: 256 bytes on stack, O(|chars|) to build.
     * For byte-level filtering, a direct lookup table beats any
     * comparison-based approach — one indexed load per input byte. */
    bool drop[256] = {0};
    fs2_fill_table(drop, sizeof(drop), chars->v);

    /* Single-pass compaction, O(|s|).
     * Invariant: out <= i at every step, so overwriting s->v[out]
     * never clobbers an unread byte. The tail [out, len) is left
     * stale and discarded by fs2_setlen. */
    size_t out = 0;
    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)s->v[i];
        if (!drop[c])
            s->v[out++] = (char)c;
    }
    return fs2_setlen(s, out);
}

/**
 * @brief Return a stack-allocated copy of @p s with all occurrences of any
 *         byte from @p chars removed.
 *
 * Does NOT modify @p s (works with STATIC fs2 as well).
 * Optimized: allocates only once for the exact result size.
 *
 * @param s      Non-NULL fs2 (any flavor). Not modified.
 * @param chars  Non-NULL fs2 — byte set to remove.
 * @return       New stack fs2.
 */
fs2
fs2_removeall_new(const fs2 *restrict s, const fs2 *restrict chars) {
    invfatal(ERR_NULL_INPUT, s != NULL && chars != NULL, 
        "Null pointers %p %p", s, chars);

    if (fs2_is_empty(s))
        return FS2EMPTY;

    if (fs2_is_empty(chars))
        return fs2_clone(s);
    
    bool drop[256] = {0};
    fs2_fill_table(drop, sizeof(drop), chars->v);

    fs2         res = FS2EMPTY;
    size_t      out = 0;
    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char) s->v[i];
        if (!drop[c])
            elem(res, out++) = c;
    }

    fs2_setlen(&res, out);  // even if FS2EMPTY
    return res;
}

/**
 * @brief Append all fs2 strings from a NULL-terminated array to @p dst,
 *         joined by @p delim.
 *
 * The result is concatenated onto the existing content of @p dst.
 * If @p dst is already non-empty, a delimiter is inserted before the
 * first element.
 *
 * @param dst   Non-NULL fs2 (ALLOC or LOCAL). Appended to.
 * @param delim Non-NULL fs2 delimiter (NULL or empty → no delimiter).
 * @param src   NULL-terminated array of non-NULL fs2 pointers.
 * @return      @p dst.
 */
fs2 *
fs2_concatall(fs2 *restrict dst, const fs2 *delim, const fs2 * const *restrict src) {
    if (fs2_cant_realloc(dst) || src == NULL)
        return dst;     // unable to write, static non-empty or nothing to do (src is null)
    
    const fs2 * const *iter = src;
    size_t             total = 0UL, delimlenth = fs2_orempty(delim)->len;
    bool               wrote_any = (dst->len > 0);   // TODO: skeleton can be here!
    // cacl total length
    while (*iter) {   // TODO: rewite via fs2array function iterator
        total += (*iter)->len;
        if (delim && iter != src)
            total += delimlenth;
        iter++;
    }
    fs2_reserve(dst, total + dst->len + 1);
    iter = src;     // recharge
    while (*iter) {   // TODO: rewite via fs2array function iterator
        if (delim && wrote_any)
            fs2_cat(dst, delim);
        fs2_cat(dst, *iter);
        iter++, wrote_any = true;
    }
    return dst;
}

/**
 * @brief Append all C-strings from a NULL-terminated array to @p dst,
 *         joined by @p delim.
 *
 * @param dst   Non-NULL fs2 (ALLOC or LOCAL). Appended to.
 * @param delim Non-NULL C-string delimiter (NULL or empty → no delimiter).
 * @param src   NULL-terminated array of C-strings.
 * @return      @p dst.
 */
fs2 *
fs2_concatall_str(fs2 *restrict dst, const char *delim, const char * const *restrict src) {
    if (fs2_cant_realloc(dst) || src == NULL)
        return dst;     // unable to write, static non-empty or nothing to do (src is null)
    
    const char * const *iter = src;
    bool wrote_any = (dst->len > 0);    // TODO: skeleton can be here!

    while (*iter) {   // TODO: rewite via fs2array function iterator
        if (delim && wrote_any)
            fs2_catstr(dst, delim);
        fs2_catstr(dst, *iter);
        iter++, wrote_any = true;;
    }
    return dst;
}

/**
 * @brief Collapse all whitespace runs to a single space, trimming leading
 *         and trailing whitespace.
 *
 * Uses isspace() from <ctype.h>: locale-dependent. In C locale (recommended),
 * only " \t\n\r\v\f" count as whitespace. Matches Python's str.split() with
 * no separator argument (join semantics).
 *
 * "  hello   world  " → "hello world"
 *
 * @param s  Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @return   @p s.
 */
fs2 *
fs2_normalize_whitespace(fs2 *restrict s) {
    if (fs2_cant_mutate(s))
        return s;

    size_t out = 0;
    bool   pending_space = false;

    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char) s->v[i];
        if (isspace(c)) {
            pending_space = true;              /* not flushed yet — trailing-safe */
        } else {
            if (pending_space && out > 0)      /* out > 0 => not leading */
                s->v[out++] = ' ';
            s->v[out++] = (char)c;
            pending_space = false;
        }
    }
    return fs2_setlen(s, out);                 /* pending trailing dropped */
}

/**
 * @brief Center @p s within @p width, padding both sides with @p pad.
 *
 * If width < s->len, the string is truncated to width.
 * If width == s->len or pad is empty, no change.
 * Extra padding (when the gap is odd) goes to the left (left-biased).
 *
 * @param s      Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param width  Target total length.
 * @param pad    Non-NULL non-empty fs2 pad pattern.
 * @return       @p s.
 */
fs2 *
fs2_center(fs2 *restrict s, size_t width, const fs2 *restrict pad) {
    fs2_cant_realloc(s);                      /* NULL / non-empty STATIC abort */
    invfatal(ERR_NULL_INPUT, pad != NULL, "Null pad");

    if (width < s->len)
        return fs2_left(s, width);            /* truncate (lpad/rpad convention) */
    if (width == s->len || fs2_is_empty(pad))
        return s;

    /* Reserve once for width+1; lpad/rpad then won't double-realloc. */
    fs2_reserve_span(s, 0, width);
    fs2_lpad(s, s->len + (width - s->len) / 2, pad);
    fs2_rpad(s, width, pad);
    return s;
}

/**
 * @brief Collapse consecutive runs of @p ch to a single occurrence.
 *
 * "aaabbbcaa" + ch='a' → "abbbca"
 * "hello"     + ch='l' → "helo"
 *
 * @param s   Non-NULL fs2 (ALLOC or LOCAL). Modified.
 * @param ch  Character whose consecutive runs to collapse.
 * @return    @p s.
 */
fs2 *
fs2_squeeze(fs2 *restrict s, int ch) {
    if (fs2_cant_mutate(s))
        return s;

    char target = (char)(unsigned char)ch;
    size_t out = 0;

    for (size_t i = 0; i < s->len; i++) {
        if (s->v[i] == target && out > 0 && s->v[out - 1] == target)
            continue;                /* second+ of a run — skip */
        s->v[out++] = s->v[i];
    }
    return fs2_setlen(s, out);
}
