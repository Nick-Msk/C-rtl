#ifndef _FS_ITER_H
#define _FS_ITER_H

// ---------------------------------------------------------------------------------
// --------------------------- Public Faststring API -------------------------------
// ---------------------------------------------------------------------------------

// --------------- Includes -----------------------

#include <stdio.h>
#include <string.h>
//
#include "bool.h"
#include "log.h"
#include "common.h"
#include "error.h"
#include "fs.h"

// ----------- CONSTANTS AND GLOBALS ---------------

// ------------------- TYPES -----------------------

typedef struct fsiter {
    size_t     pos, from, to;
    char      *v;
} fsiter;

typedef struct fsiterrev {
    size_t     pos, from, to;
    char      *v;
} fsiterrev;

typedef struct fsnew {
    size_t     pos;
    fs        *s;
} fsnew;

// ------------- CONSTRUCTOTS/DESTRUCTORS ----------

// TODO: make a REV interator as fsreviter{ };

// forward iterator
static inline fsiter        fseach(fs s){
    return (fsiter){.pos = 0, .from = 0, .to = s.len, .v = s.v};
}

// foprward iterator
static inline fsiterrev     fseachrev(fs s){
    return (fsiterrev){.pos = s.len - 1, .from = s.len - 1, .to = 0, .v = s.v};
}

// new iterator, it's assumed str is initialized as heap!
static inline fsnew         fsinew(fs *str){
    // fs_init
    if (!fs_alloc(str) )
        userraiseint(11, "Can't create build iterator to non-heap fs");
    return (fsnew){.pos = 0, .s = str};
}

// new appent iterator, it's assumed str is initialized as heap!
static inline fsnew         fsiapp(fs *str){
    if (!fs_alloc(str) )
        userraiseint(11, "Can't create build iterator to non-heap fs");
    return (fsnew){.pos = str->len, .s = str};
}

// ++ slice, TODO: rev slice
static inline fsiter        fsslice(fs s, size_t from, size_t to){
    fsiter i = fseach(s);
    i.from = i.pos = from < s.len ? from : s.len;
    i.to = to < s.len ? to : s.len;
    return i; // (fsiter){.pos = MIN(from, s.len), .from = MIN(from, s.len), .to = MIN(to, s.len), .v = s.v};
}

// -------------------- ACCESS AND MODIFICATORS ------------------------

static inline bool          fshasnext(fsiter *i){
    return i->pos < i->to;
}

static inline char         *fsnext(fsiter *i){
    return i->v + i->pos++;
}

static inline char         *fscurr(fsiter *i){
    return i->v + i->pos;
}

static inline bool          fshasnextrev(fsiterrev *i){
    return i->pos >= i->to;
}

static inline char         *fsnextrev(fsiterrev *i){
    return i->v + i->pos--;
}

static inline char         *fscurrrev(fsiterrev *i){
    return i->v + i->pos;
}

// semi-constructor!
static inline char         *fselemnext(fsnew *i){
    return fs_elem(i->s, i->pos++);
}
// not sure if really need that
static inline char         *fselemcurr(fsnew *i){
    return fs_elem(i->s, i->pos);   // TODO: fs_curr() ???
}

// setup len and '\0'
static inline char         *fselemend(fsnew *i){
    return fs_setlen(i->s, i->pos);
}

// clear to 0 len and pos
static inline char         *fselemclear(fsnew *i){
    return fs_setlen(i->s, i-> pos = 0);
}

static inline int           fsnew_techfprint(FILE *restrict f, fsnew *restrict i, const char *restrict name){
    int cnt = 0;
    if (f)
        cnt = fprintf(f, "FSNEW iter %s: pos %zu, len %zu, sz %zu\n", name, i->pos, i->s ? i->s->len: -1, i->s ? i->s->sz: -1);
    return cnt;
}

static inline int           fsnew_techprint(fsnew *restrict i, const char *restrict name){
    return fsnew_techfprint(stdout, i, name);
}

// forward
#define                     hasnext(i)          fshasnext(&(i) )
#define                     next(i)            *(fsnext(&(i) ) )
#define                     curr(i)            *(fscurr(&(i) ) )
#define                     fsforeach(s, i)     for (fsiter i = fseach(s); hasnext(i); next(i) )

// reverse
#define                     hasnextrev(i)       fshasnextrev(&(i) )
#define                     nextrev(i)         *(fsnextrev(&(i) ) )
#define                     currrev(i)         *(fscurrrev(&(i) ) )
#define                     fsforeachrev(s, i) for (fsiterrev i = fseachrev(s); hasnextrev(i); nextrev(i) )

// fsnew
#define                     elemnext(i)        *fselemnext(&(i) )
#define                     elemcurr(i)        *fselemcurr(&(i) )
#define                     elemend(i)         *fselemend(&(i) )
#define                     elemclear(i)       *fselemclear(&(i) )

// in future
// #define lyamdba(s, compare) ...

// ------------------------ PRINTERS/CHECKERS --------------------------

#define                     fsnewtechfprint(f, i)       fsnew_techfprint((f), &i, #i)
#define                     fsnewtechprint(i)           fsnew_techfprint(&i, #i)
// --------------------------- ETC. -------------------------

#endif /* !_FS_ITER_H */

