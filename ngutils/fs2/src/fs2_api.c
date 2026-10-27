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

    if (src->len == 0) return dst;

    fs2_memcpy(dst, dst->len, src, 0, src->len);
    
    fs2_setlen(dst, dst->len + src->len);
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
    fs2_selfmove(dst, len, 0, dst->len);

    fs2_memcpy_str(dst, 0, beginner, len);

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
    *res = fs2_substr_new(s, from, len);
    res->flags |= FS2_BODYALLOC;
    return res;
}

// ------------------------------- Upper / Lower case ---------------------------------------
// upper
fs2 *
fs2_toupper(fs2 *str) {
    if (!str)
        return str;
    for (size_t i = 0; i < str->len; i++)
        str->v[i] = toupper( (unsigned char) str->v[i]);
    return str;
}

//lower
fs2 *
fs2_tolower(fs2 *str) {
    if (!str)
        return str;
    for (size_t i = 0; i < str->len; i++)
        str->v[i] = tolower( (unsigned char) str->v[i]);
    return str;
}

// ------------------------------------- Padding -------------------------------------------
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
// left
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
// generators
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

bool 
fs2_pos_parse_long(const fs2 *restrict s, size_t pos, size_t *restrict newpos, long *restrict out) {
    if (!s || !s->v || pos >= s->len) 
        return logsimpleerr(false, "Null input or pos %zu out of length", pos);

    const char *base = s->v + pos;
    
    // Пропускаем ведущие пробелы и табуляции
    const char *start = skip_spaces(base);
    
    // Если строка состояла только из пробелов до конца
    if (*start == '\0') 
        return logsimpleerr(false, "Only spaces detected");
    
    if (!isdigit((unsigned char)*start) && *start != '+' && *start != '-')
        return logsimpleerr(false, "incorrent sym detected [%c]", *start);

    char *endptr = NULL;
    errno = 0;
    long val = strtol(start, &endptr, 10);

    // Overflow/Underflow или нет цифр
    if (errno == ERANGE || endptr == start)
        return logsimpleerr(false, "Overflow or no digits %d, %zu", errno, endptr - start);

    if (out)
        *out = val;
        
    // Вычисляем newpos через разницу указателей
    if (newpos)
        *newpos = pos + (size_t)(endptr - base);

    return true;
}

bool 
fs2_pos_parse_int(const fs2 *restrict s, size_t pos, size_t *restrict newpos, int *restrict out) {
    long lval = 0;
    size_t npos = 0;

    // Сначала парсим как long
    if (!fs2_pos_parse_long(s, pos, &npos, &lval))
        return false;

    // Проверяем, влезает ли в int
    if (lval < INT_MIN || lval > INT_MAX)
        return logsimpleerr(false, "Parsed int is out of bound %d <= %ld <= %d", INT_MIN, lval, INT_MAX);

    if (out)
        *out = (int)lval;
    if (newpos)
        *newpos = npos;

    return true;
}
