/**
 * @file fs2_api.c
 * @brief Non-inline implementations for fs2_api.h:
 *        reverse, partialsort, and N-occurrence search functions.
 */

#include "fs2_api.h"

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
 * Delegates to fs2_nichr_nth with lim = str->len.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based occurrence number (0 → -1).
 * @return     0-based offset, or -1.
 */
ptrdiff_t
fs2_ichr_nth  (const fs2 *str, int c, size_t k) {
    invraise(str != NULL, "Null pointer");

    return fs2_nichr_nth(str, c, k, str->len);
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
fs2_nichr_nth (const fs2 *str, int c, size_t k, size_t lim) {
    invraise(str != NULL, "Null pointer");
    
    if (k == 0)
        return -1;

    const unsigned char *base = (const unsigned char *)str->v;
    const unsigned char *orig = base;
    size_t avail = (lim < str->len) ? lim : str->len;

    for (size_t i = 0; i < k; i++) {
        ptrdiff_t hit = __fs2_ichr_raw(base, avail, c);
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
 * Scans from the end. Delegates to fs2_nirchr_nth with lim = str->len.
 *
 * @param str  Non-NULL fs2.
 * @param c    Byte to find (case-insensitive).
 * @param k    1-based reverse occurrence number (0 → -1).
 * @return     0-based offset, or -1.
 */
ptrdiff_t
fs2_irchr_nth (const fs2 *str, int c, size_t k) {
    invraise(str != NULL, "Null pointer");

    return fs2_nirchr_nth(str, c, k, str->len);
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
fs2_nirchr_nth(const fs2 *str, int c, size_t k, size_t lim) {
    invraise(str != NULL, "Null pointer");

    if (k == 0)
        return -1;

    size_t avail = (lim < str->len) ? lim : str->len;
    size_t off   = str->len - avail;
    const unsigned char *base = (const unsigned char *)str->v + off;
    size_t end   = avail;

    for (size_t i = 0; i < k; i++) {
        ptrdiff_t hit = __fs2_irchr_raw(base, end, c);
        if (hit < 0)
            return -1;
        if (i == k - 1)
            return (ptrdiff_t)off + hit;
        end = (size_t)hit;
    }
    return -1;
}

// ------------------------------------ CAT/CPY/ETC -----------------------------------------
fs2 *
fs2_cat(fs2 *restrict dst, const fs2 *restrict src) {
    invraise(dst != NULL && src != NULL, "Null pointers %p %p", dst, src);

    size_t  len = dst->len + src->len;
    fs2_reserve(dst, len + 1);
    if (src->len > 0)
        memcpy(dst->v + dst->len, src->v, src->len + 1);
    fs2_setlen(dst, len);
    return dst;
}

// beginner-null safe version
fs2 *
fs2_rev_catstr(fs2 *restrict dst, const char *restrict beginner) {
    invraise(dst != NULL, "Null pointer %p", dst);

    if (!beginner)
        return dst;
    size_t  len = strlen(beginner);
    if (len == 0)       // no any allocations if null or empty 
        return dst;
    fs2_reserve(dst, dst->len + len + 1);
    memmove(dst->v + len, dst->v, dst->len + 1);
    memcpy(dst->v, beginner, len);

    return fs2_setlen(dst, dst->len + len);
}

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

fs2 *
fs2_heap_substr(const fs2 *s, size_t from, size_t len) {
    invraise(s != NULL, "Null pointer");

    fs2 *res = fs2_heap();
    fs2_substr_bounds(s, &from, &len);        
    
    if (len == 0)
        return res;

    fs2_memcpy(res, 0, s, from, len);
    fs2_setlen(res, len);
    return res;
}
