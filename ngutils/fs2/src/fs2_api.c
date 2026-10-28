/**
 * @file fs2_api.c
 * @brief Non-inline implementations for fs2_api.h:
 *        reverse, partialsort, and N-occurrence search functions.
 */

#include "fs2_api.h"
#include <locale.h>

// Utilities

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
fs2_reverse(fs2 *str) {
    invraise(str != NULL, "Null pointer");

    if (fs2_is_static(str))
        userraise(0, ERR_UNSUPPORTED_TYPE, "Cannot modify STATIC fs2 in-place");

    if (str->len > 0UL) {
        size_t i = 0UL, j = str->len - 1;
        while (i < j)
            char_exch(str->v + i++, str->v + j--);
    }
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
    invraise(str != NULL, "Null pointer");

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
    invraise(str != NULL, "Null pointer");

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
    invraise(str != NULL, "Null pointer");

    if (k == 0) 
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
    invraise(str != NULL, "Null pointer");

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
    invraise(str != NULL, "Null pointer");
    
    if (k == 0)
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
    invraise(str != NULL, "Null pointer");

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
    invraise(str != NULL, "Null pointer");

    if (k == 0)
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
    invraise(str != NULL, "Null pointer");

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
    invraise(str != NULL, "Null pointer");

    if (k == 0)
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
    invraise(dst != NULL && src != NULL, "Null pointers %p %p", dst, src);

    if (src->len == 0) return dst;

    fs2_memcpy(dst, dst->len, src, 0, src->len);
    
    fs2_setlen(dst, dst->len + src->len);
    return dst;
}

/**
 * @brief Prepend a C-string @p beginner to @p dst in place.
 *
 * Moves existing content forward and copies @p beginner at position 0.
 * NULL or empty @p beginner is a no-op.
 *
 * @param dst       Non-NULL fs2 (ALLOC or LOCAL).
 * @param beginner  NUL-terminated string (NULL is safe).
 * @return          @p dst.
 */
fs2 *
fs2_rev_catstr(fs2 *restrict dst, const char *restrict beginner) {
    invraise(dst != NULL, "Null pointer %p", dst);

    if (!beginner)
        return dst;
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
fs2_vsprintf_position(fs2 *restrict s, size_t pos, const char *restrict fmt, va_list ap) {
    invraise(s != NULL && fmt != NULL, "Null pointers %p %p", s, fmt);

    va_list ap2; 
    va_copy(ap2, ap); 
    long    needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);

    if (needed < 0)
        return logsimpleerr(-1L, "vsnprintf length failed");
    
    size_t  lastpos = pos + 1 + needed;
    if (lastpos > s->sz) // not sure, probaly > sz
        fs2_reserve(s, lastpos);

    long     cnt = vsnprintf(s->v + pos, needed + 1, fmt, ap);

    return cnt;    
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
    invraise(s != NULL, "Null pointer");

    if (!s->v)     // not allocated
        return s;

    // normalization
    fs2_substr_bounds(s, &from, &len);

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
    invraise(s != NULL, "Null pointer");

    // s CAN be even STATIC
    fs2  res = FS2();
    fs2_substr_bounds(s, &from, &len);        
    
    if (len == 0)
        return res;

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
    invraise(s != NULL, "Null pointer");

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
 * @param str  fs2 (ALLOC or LOCAL). May be NULL.
 * @return     @p str (or NULL).
 */
fs2 *
fs2_toupper(fs2 *str) {
    if (!str)
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
 * @param str  fs2 (ALLOC or LOCAL). May be NULL.
 * @return     @p str (or NULL).
 */
fs2 *
fs2_tolower(fs2 *str) {
    if (!str)
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
    invraise(str != NULL && pad != NULL, "Null pointers %p %p", str, pad);

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
    invraise(str != NULL && pad != NULL, "Null pointers %p %p", str, pad);

    if (len < str->len)
        return fs2_left(str, len);

    if (pad->len == 0 || len == str->len)  // empty - nothing to PAD
        return str;

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
    invraise(s != NULL, "Null pointer");

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
    unsigned long ul   = 0;
    size_t        npos = 0;

    if (!fs2_pos_parse_ulong(s, pos, &npos, &ul))
        return false;

    if (ul > UINT_MAX)
        return logsimpleerr(false, "Parsed int is out of bound %lu <= %u", ul, UINT_MAX);

    if (out)
        *out = (unsigned)ul;
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

/* ─────────────────────────────────────────────────────────────────
 * String search API
 * ───────────────────────────────────────────────────────────────── */
/* Search function type: haystack -> position of first occurrence of needle,
 * or NULL if not found. Exactly like strstr/strcasestr. */
typedef char *(*fs2_findfn_t)(const char *haystack,
                                    const char *needle);

static inline ptrdiff_t
_fs2_instr_posn_common(const fs2 *restrict str, const fs2 *restrict pattern,
                       size_t pos, size_t nth, fs2_findfn_t find) {
    if (fs2_is_empty(pattern))       return -1;
    if (nth == 0)                    return -1;
    if (pos >= str->len)             return -1;
    
    size_t pat_len = pattern->len;
    if (pat_len > str->len - pos)    return -1;
    
    const char *needle       = pattern->v;
    const char *haystack     = str->v + pos;
    const char *haystack_end = str->v + str->len - pat_len;
    size_t      count        = 0;
    
    while (haystack <= haystack_end) {
        const char *hit = find(haystack, needle);
        if (!hit) break;
        count++;
        if (count == nth) return (ptrdiff_t)(hit - str->v);
        haystack = hit + 1;
    }
    return -1;
}

/* Public wrappers pass their own finder */
ptrdiff_t
fs2_instr_posn(const fs2 *restrict str, const fs2 *restrict pattern,
               size_t pos, size_t nth) {
    invraise(str != NULL && pattern != NULL, "Null pointers %p %p", str, pattern);
    return _fs2_instr_posn_common(str, pattern, pos, nth, strstr);
}

ptrdiff_t
fs2_instr_posn_ci(const fs2 *restrict str, const fs2 *restrict pattern,
                  size_t pos, size_t nth) {
    invraise(str != NULL && pattern != NULL, "Null pointers %p %p", str, pattern);
    return _fs2_instr_posn_common(str, pattern, pos, nth, strcasestr);
}

