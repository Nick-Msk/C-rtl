#ifndef FS2_API_H
#define FS2_API_H

#include "fs2.h"

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
static inline ptrdiff_t           fs2_chr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");

    const char *p = strchr(str->v, c);
    return p ? (ptrdiff_t) (p - str->v) : -1;
}
// pointer, sensitive, lim
static inline ptrdiff_t           fs2_nchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    const unsigned char *p = (const unsigned char *) str->v;
    const unsigned char uc = (unsigned char) c;
    while (lim-- > 0 && *p != '\0') {
        if (*p == uc)
            return (ptrdiff_t) (p - (const unsigned char *) str->v);
        p++;
    }
    return -1;
}
// pointer, insensitive, lim
static inline ptrdiff_t           fs2_nichr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    const unsigned char *p = (const unsigned char *) str->v;
    const int lc = tolower((unsigned char) c);
    while (lim-- > 0 && *p != '\0') {
        if (tolower(*p) == lc)
            return (ptrdiff_t) (p - (const unsigned char *) str->v);
        p++;
    }
    return -1;
}
// pointer, insensitive, unlim
static inline ptrdiff_t           fs2_ichr(const fs2 *str, int c) {
    invraise(str != NULL, "Null pointer");

    return fs2_nichr(str, c, str->len);
}

// ------------------------------------------ ONE SYMBOL REVERSE SEARCH  --------------------------------------------------------
// pointer, sensitive, unlim
static inline ptrdiff_t           fs2_rchr(const fs2 *str, int c) { 
    invraise(str != NULL, "Null pointer");

    ptrdiff_t   pos = (ptrdiff_t) str->len - 1;
    while (pos >= 0 && str->v[pos] != c)
        pos--;
    return pos;
}
// pointer, insensitive, unlim
static inline ptrdiff_t           fs2_irchr(const fs2 *str, int c) { 
    invraise(str != NULL, "Null pointer");

    c = tolower(c);
    ptrdiff_t   pos = (ptrdiff_t) str->len - 1;
    while (pos >= 0 && tolower( (unsigned char) str->v[pos] ) != c)
        pos--;
    return pos;
}

// pointer, sensitive, lim, reverse
static inline ptrdiff_t             fs2_nrchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");
    //if (str->len == 0 || lim == 0) return -1;

    ptrdiff_t   lo  = (str->len > lim) ? (ptrdiff_t) (str->len - lim) : 0;
    ptrdiff_t   pos = (ptrdiff_t) str->len - 1;
    while (pos >= lo) {
        if (str->v[pos] == c)
            return pos;
        pos--;
    }
    return -1;
}

// pointer, insensitive, lim, reverse
static inline ptrdiff_t             fs2_nirchr(const fs2 *str, int c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    c = tolower(c);
    ptrdiff_t   lo  = (str->len > lim) ? (ptrdiff_t) (str->len - lim) : 0;
    ptrdiff_t   pos = (ptrdiff_t) str->len - 1;
    while (pos >= lo) {
        if (tolower((unsigned char) str->v[pos]) == c) 
            return pos;
        pos--;
    }
    return -1;
}

// ------------------------------------------------------------------------------------------
extern long           
fs_sprintf(fs2 *restrict s, const char *restrict fmt, ...) 
    __attribute__ (( format (printf, 2, 3) ) );

extern long           
fs_sprintf_concat(fs2 *restrict s, const char *restrict fmt, ...)
    __attribute__ (( format (printf, 2, 3) ) );



#endif /* !FS2_API_H */
