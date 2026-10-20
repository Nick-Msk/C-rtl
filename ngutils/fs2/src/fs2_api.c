#include "fs2_api.h"

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

void             
fs2_partialsort(fs2 *str, size_t from, size_t to, bool asc) {
    invraise(str != NULL, "Null pointer");

    if (fs2_is_static(str))
        userraise(0, ERR_UNSUPPORTED_TYPE, "Cannot sort STATIC fs2 in-place");
    qsort(str->v + from, to - from, 1, asc ? pchar_cmp: pchar_revcmp);
}

// ------------------------------------------ ONE SYMBOL, N occurence SEARCH  --------------------------------------------------------

/* forward, sensitive */
ptrdiff_t   
fs2_chr_nth   (const fs2 *str, int c, size_t k) {
    invraise(str != NULL, "Null pointer");

    return fs2_nchr_nth(str, c, k, str->len);
} 
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
ptrdiff_t
fs2_ichr_nth  (const fs2 *str, int c, size_t k) {
    invraise(str != NULL, "Null pointer");

    return fs2_nichr_nth(str, c, k, str->len);
}

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
ptrdiff_t
fs2_rchr_nth  (const fs2 *str, int c, size_t k) {
    invraise(str != NULL, "Null pointer");

    return fs2_nrchr_nth(str, c, k, str->len);
}

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
ptrdiff_t
fs2_irchr_nth (const fs2 *str, int c, size_t k) {
    invraise(str != NULL, "Null pointer");

    return fs2_nirchr_nth(str, c, k, str->len);
}

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
