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


#endif /* !FS2_API_H */
