#ifndef FS2_API_H
#define FS2_API_H

#include "fs2.h"

// -------------------------------- Internal helpers --------------------------------------

static inline ptrdiff_t
__fs2_chr_raw(const unsigned char *p, size_t avail, int c) {
    const unsigned char *hit = memchr(p, (unsigned char)c, avail);
    return hit ? (ptrdiff_t)(hit - p) : -1;
}

static inline ptrdiff_t
__fs2_ichr_raw(const unsigned char *p, size_t avail, int c) {
    const int lc = tolower((unsigned char)c);
    for (size_t i = 0; i < avail; i++)
        if (tolower(p[i]) == lc) 
            return (ptrdiff_t)i;
    return -1;
}

static inline ptrdiff_t
__fs2_rchr_raw(const unsigned char *p, size_t avail, int c) {
    const unsigned char uc = (unsigned char)c;
    for (ptrdiff_t i = (ptrdiff_t) avail - 1; i >= 0; i--)
        if (p[i] == uc)
            return i;
    return -1;
}

static inline ptrdiff_t
__fs2_irchr_raw(const unsigned char *p, size_t avail, int c) {
    const int lc = tolower((unsigned char)c);
    for (ptrdiff_t i = (ptrdiff_t) avail - 1; i >= 0; i--)
        if (tolower(p[i]) == lc)
            return i;
    return -1;
}

// --------------------------------------- API ---------------------------------------
// direct access, NO change len or sz, position MUST be < sz
static inline char *
fs2_get(const fs2 *s, size_t pos){
    return s->v + pos;
}

// autoallocation!!! any position, but NO control the len
static inline char *
fs2_elem(fs2 *s, size_t pos){
    if (pos >= s->sz){
        fs2_reserve(s, pos + 1);   // len remains the same here! sz is changed
    }
    return fs2_get(s, pos);
}

// autoallocation + 1!!! any position, but with setting the len
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

extern fs2*             fs2_reverse(fs2 *str);

// THINK: fs2_slice
extern void             fs2_partialsort(fs2 *s, size_t from, size_t to, bool asc);

static inline void      fs2_sort(fs2 *str, bool asc) {
    invraise(str != NULL, "Null pointer");
    fs2_partialsort(str, 0, str->len, asc);
}

// ------------------------------------ CAT/CPY/ETC -----------------------------------------

// TODO:

// ----------------------------- One Symbol Search ------------------------------------------
// pointer, sensitive, unlim (library call)
static inline ptrdiff_t
fs2_chr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");
    return __fs2_chr_raw((const unsigned char *) str->v, str->len, c);
}

// pointer, insensitive, unlim
static inline ptrdiff_t
fs2_ichr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");
    return __fs2_ichr_raw((const unsigned char *) str->v, str->len, c);
}

// pointer, sensitive, lim
static inline ptrdiff_t
fs2_nchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");
    size_t avail = (lim < str->len) ? lim : str->len;
    return __fs2_chr_raw((const unsigned char *) str->v, avail, c);
}

// pointer, insensitive, lim
static inline ptrdiff_t
fs2_nichr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");
    size_t avail = (lim < str->len) ? lim : str->len;
    return __fs2_ichr_raw((const unsigned char *) str->v, avail, c);
}

// ------------------------------------------ ONE SYMBOL REVERSE SEARCH  --------------------------------------------------------
// pointer, sensitive, unlim
static inline ptrdiff_t
fs2_rchr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");
    return __fs2_rchr_raw((const unsigned char *) str->v, str->len, c);
}

// pointer, insensitive, unlim
static inline ptrdiff_t
fs2_irchr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");
    return __fs2_irchr_raw((const unsigned char *)str->v, str->len, c);
}

// pointer, sensitive, lim, reverse
static inline ptrdiff_t fs2_nrchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");
    size_t avail = (lim < str->len) ? lim : str->len;
    size_t off   = str->len - avail;
    ptrdiff_t hit = __fs2_rchr_raw((const unsigned char *)str->v + off, avail, c);
    return hit < 0 ? -1 : hit + (ptrdiff_t)off;
}

// pointer, insensitive, lim, reverse
static inline ptrdiff_t fs2_nirchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");
    size_t avail = (lim < str->len) ? lim : str->len;
    size_t off   = str->len - avail;
    ptrdiff_t hit = __fs2_irchr_raw((const unsigned char *)str->v + off, avail, c);
    return hit < 0 ? -1 : hit + (ptrdiff_t)off;
}

// ------------------------------------------ ONE SYMBOL, N occurence SEARCH  --------------------------------------------------------

/* forward, sensitive */
ptrdiff_t   fs2_chr_nth   (const fs2 *str, int c, size_t k);
ptrdiff_t   fs2_nchr_nth  (const fs2 *str, int c, size_t k, size_t lim);

/* forward, insensitive */
ptrdiff_t   fs2_ichr_nth  (const fs2 *str, int c, size_t k);
ptrdiff_t   fs2_nichr_nth (const fs2 *str, int c, size_t k, size_t lim);

// ------------------------------------------ ONE SYMBOL, N occurence REVERSE SEARCH  --------------------------------------------------------

/* reverse, sensitive */
ptrdiff_t   fs2_rchr_nth  (const fs2 *str, int c, size_t k);
ptrdiff_t   fs2_nrchr_nth (const fs2 *str, int c, size_t k, size_t lim);

/* reverse, insensitive */
ptrdiff_t   fs2_irchr_nth (const fs2 *str, int c, size_t k);
ptrdiff_t   fs2_nirchr_nth(const fs2 *str, int c, size_t k, size_t lim);

// ------------------------------------------------------------------------------------------
extern long           
fs_sprintf(fs2 *restrict s, const char *restrict fmt, ...) 
    __attribute__ (( format (printf, 2, 3) ) );

extern long           
fs_sprintf_concat(fs2 *restrict s, const char *restrict fmt, ...)
    __attribute__ (( format (printf, 2, 3) ) );



#endif /* !FS2_API_H */
