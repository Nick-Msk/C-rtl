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

// ----------------------------- One Symbol Search ------------------------------------------
// pointer, sensitive, unlim (library call)
static inline long           fs2_chr(const fs2 *str, char c) {
    invraise(str != NULL, "Null pointer");

    const char *p = strchr(str->v, c);
    return p ? p - str->v : -1;
}
// pointer, sensitive, lim
static inline long           fs2_nchr(const fs2 *str, char c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    const char *p = str->v;
    while (lim-- > 0 && *p != '\0') {
        if (*p == c)
            return p - str->v;
        p++;
    }
    return -1;
}
// pointer, insensitive, lim
static inline long           fs2_nichr(const fs2 *str, char c, size_t lim) {
    invraise(str != NULL, "Null pointer");

    const char *p = str->v;
    c = tolower(c);
    while (lim-- > 0 && *p != '\0') {
        if (*p == c)
            return p - str->v;
        p++;
    }
    return -1;
}
// pointer, insensitive, unlim
static inline long           fs2_ichr(const fs2 *str, char c) {
    invraise(str != NULL, "Null pointer");

    return fs2_nichr(str, c, str->len);
}

// ------------------------------------------------------------------------------------------
extern long           
fs_sprintf(fs2 *restrict s, const char *restrict fmt, ...) 
    __attribute__ (( format (printf, 2, 3) ) );

extern long           
fs_sprintf_concat(fs2 *restrict s, const char *restrict fmt, ...)
    __attribute__ (( format (printf, 2, 3) ) );



#endif /* !FS2_API_H */
