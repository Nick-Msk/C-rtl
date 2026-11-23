#ifndef _HASHSET_H
#define _HASHSET_H

// -------------------------------------------------------------------------------------
// --------------------------- Public HashSet API --------------------------------------
// -------------------------------------------------------------------------------------

// --------------- Includes -----------------------

#include <stdio.h>
#include <stdlib.h>

#include "numeric_ops.h"
#include "log.h"
#include "bool.h"
#include "common.h"
#include "error.h"
#include "checker.h"
#include "guard.h"
#include "fileutils.h"
#include "fs.h"
#include "value64.h"

// ----------- CONSTANTS AND GLOBALS ---------------

// ------------------- TYPES -----------------------


enum    { HSET_UNIFIED_CNT  = 0x10,
          HSET_HEAP_ALLOC   = 0x101,      // hset is allocated by malloc
          HSET_FS_LOAD_STR  = 0x102,       // for array loaded for const char *
          HSET_FS_LOAD_FS   = 0x103
};
typedef struct hset_elem {
    value64             v;
    struct hset_elem   *next;
} hset_elem;

typedef struct hset {
    int             sz;     // init size!
    int             flags;  // empty for now
    int             count;
    hset_elem     **table;
} hset;

// --------------------------------------- hset ----------------------------------------------
#define                 HSET(size, typ) (hset) {.sz = (size), .flags = (typ), .table = 0 }
#define                 HSET_NONINIT        HSET(0, VALUE64_UNKNOWN)

// create value from pointer
extern value64              hset_createval(const void *p, value64_type typ);

//  check if in non-init state
static inline bool          hset_isnoninit(const hset *se){
    return se->flags == 0 && se->sz == 0;
}

static inline value64_type  hset_getype(const hset *se){
    return se->flags & 0xFF;
}

static inline bool          hset_heap_alloc(const hset *se){
    return se->flags & HSET_HEAP_ALLOC;
}

static inline void          hset_set_heap_alloc(hset *se){
    se->flags |= HSET_HEAP_ALLOC;
}

// ------------- CONSTRUCTOTS/DESTRUCTORS ----------

extern hset                 hset_init(int sz, value64_type typ);      // #define will be for particular type
static inline hset          hset_init_int(int sz){
    return hset_init(sz, VALUE64_INT);
}
static inline hset          hset_init_long(int sz){
    return hset_init(sz, VALUE64_LONG);
}
static inline hset          hset_init_dbl(int sz){
    return hset_init(sz, VALUE64_DBL);
}
static inline hset          hset_init_fs(int sz){
    return hset_init(sz, VALUE64_FS);
}
static inline hset          hset_init_ptr(int sz){
    return hset_init(sz, VALUE64_PTR);
}

extern hset                 hset_init_resize(hset *se, int newsz);
extern hset                 hset_normalize(hset *se);
//
extern void                 hset_free(hset *se);
//
extern hset                 hset_clone(const hset *se);

extern hset                 hset_cloneas(const hset *se, value64_type typ);

// universale loader
extern hset                 hset_from_anyarr(const void *arr, int sz, value64_type typ);
// typed!!! not generic
static inline hset          hset_from_intarr(const int *iarr, int sz){
    return hset_from_anyarr(iarr, sz, VALUE64_INT);
}
static inline hset          hset_from_fssarr(const fs *fsarr, int sz){
    return hset_from_anyarr(fsarr, sz, VALUE64_FS);
}
static inline hset          hset_from_longarr(const long *larr, int sz){
    return hset_from_anyarr(larr, sz, VALUE64_LONG);
}
static inline hset          hset_from_dblarr(const double *darr, int sz){
    return hset_from_anyarr(darr, sz, VALUE64_DBL);
}
static inline hset          hset_from_ptrarr(const void **parr, int sz){
    return hset_from_anyarr(parr, sz, VALUE64_PTR);
}

// Simplified macroses
#define HSET_CREATE_INT(...)\
    ({ \
        int sz = sizeof((int []){__VA_ARGS__}) / sizeof(int); \
        hset _tmp = hset_from_intarr( (int []) {__VA_ARGS__},  sz > 10 ? sz : 10); \
        _tmp; \
    })
#define HSET_CREATE_LONG(...)\
    ({ \
        int sz = sizeof((long []){__VA_ARGS__}) / sizeof(long); \
        hset _tmp = hset_from_longarr( (long []) {__VA_ARGS__},  sz > 10 ? sz : 10); \
        _tmp; \
    })
#define HSET_CREATE_DBL(...)\
    ({ \
        int sz = sizeof((double []){__VA_ARGS__}) / sizeof(double); \
        hset _tmp = hset_from_dblarr( (double []) {__VA_ARGS__},  sz > 10 ? sz : 10); \
        _tmp; \
    })
#define HSET_CREATEFS_ASSTR(...) \
    ({ \
        int _n = sizeof((const char *[]){__VA_ARGS__, NULL}) / sizeof(const char *) - 1; \
        hset _tmp = hset_init(_n > 10 ? _n : 10, VALUE64_FS); \
        HSET_LOADFS_STR(_tmp, __VA_ARGS__); \
        _tmp; \
    })

// -------------------- ACCESS AND MODIFICATORS ------------------------

// -------------------- hset_elem API ----------------------------------
static inline fs           *hsetelem_getfs(const hset_elem *el){
    return value64_fs(el->v);
}
static inline int           hsetelem_getint(const hset_elem *el){
    return value64_int(el->v);
}
static inline long          hsetelem_getlong(const hset_elem *el){
    return value64_long(el->v);
}
static inline double        hsetelem_getdbl(const hset_elem *el){
    return value64_dbl(el->v);
}
static inline void         *hsetelem_getptr(const hset_elem *el){
    return value64_ptr(el->v);
}

// ------------------------ Element access -----------------------------
// move and fill with ZERO 0
extern bool                 hset_move(hset *se, value64 *val);
// true if new element is added, if exists - false
static inline bool          hset_set(hset *se, value64 val){
    invraisecode(ERR_NULLABLE_PTR, se != 0, "Null pointer");

    value64 copy = value64_clone(val, hset_getype(se));
    return hset_move(se, &copy);
}
// try to delete elemenet, true if deleted, false if not found
extern bool                 hset_del(hset *se, value64 val);

extern bool                 hset_get(const hset *se, value64 val);

static inline int           hset_cnt(const hset *se){
    return se->count;
}
static inline bool          hset_isempty(const hset *se) {
    return hset_cnt(se) == 0;
}

extern void                 hset_clean(hset *se);
// origin wll be cleaned TODO: not checked
static inline hset         *hset_moveall(hset *restrict target, hset *restrict origin){
    hset_free(target);
    *target = *origin;
    origin->table = 0;
    origin->flags = origin->sz = 0;
    return logsimpleret(target, "moved to %p, sz %d, cnt %d", target, target->sz, target->count);
}

extern bool                 hset_elem_move(hset *restrict se, hset_elem *restrict elem);

// load values from array, any type
extern int                  hset_loadanyarr(hset *restrict se, void *arr, int sz, value64_type typ);

static inline int           hset_loadiarr(hset *restrict se, const int *iarr, int sz){
    return hset_loadanyarr(se, (void *) iarr, sz, VALUE64_INT);
}
static inline int           hset_loadlarr(hset *restrict se, const long *larr, int sz){
    return hset_loadanyarr(se, (void *) larr, sz, VALUE64_LONG);
}
static inline int           hset_loaddarr(hset *restrict se, const double *darr, int sz){
    return hset_loadanyarr(se, (void *) darr, sz, VALUE64_DBL);
}
// not sure about const here
static inline int           hset_loadparr(hset *restrict se, const void * const *restrict parr, int sz){
    return hset_loadanyarr(se, (void *) parr, sz, VALUE64_PTR);
}
static inline int           hset_loadfsarr(hset *restrict se, fs *restrict fsarr, int sz){
    return hset_loadanyarr(se, fsarr, sz, VALUE64_FS);
}
// fs creation from c-str[]
extern int                  hset_loadfs_str(hset *restrict se, const char *strings[]);

#define                     HSET_LOADFS_STR(se,...) hset_loadfs_str(&(se), (const char *[]) {__VA_ARGS__, NULL} )

// only static literals! NOT implemented yet
//extern int                  hset_loadfs_literal(hset *restrict se, const char *lits[]);

// ----------------------------------------- iterators --------------------------------------


// Общий внутренний макрос – проходит по всем элементам и на каждой итерации
// объявляет var заданного типа и присваивает ей значение из поля field.
#define _HSET_FOREACH_TYPE(se, var, type, field) \
    for (int _i_ = 0; _i_ < (se)->sz; _i_++) \
        for (const hset_elem *_el_ = (se)->table[_i_]; _el_; _el_ = _el_->next) \
            for (int _flag_ = 1; _flag_; _flag_ = 0) \
                for (type var = _el_->v.field; _flag_; _flag_ = 0)

            //_Pragma("GCC diagnostic push") \
            //_Pragma("GCC diagnostic ignored \"-Wfor-loop-analysis\"") \
           // _Pragma("GCC diagnostic pop")
// Публичные макросы
#define                     HSET_FOREACH_INT(se, var)  _HSET_FOREACH_TYPE(se, var, int, ival)
#define                     HSET_FOREACH_LONG(se, var) _HSET_FOREACH_TYPE(se, var, long, lval)
#define                     HSET_FOREACH_DBL(se, var)  _HSET_FOREACH_TYPE(se, var, double, dval)
#define                     HSET_FOREACH_PTR(se, var)  _HSET_FOREACH_TYPE(se, var, void*, pval)
#define                     HSET_FOREACH_FS(se, var)   _HSET_FOREACH_TYPE(se, var, fs*, fsval)
// any type, no mod!
#define                     HSET_FOREACH(se, var) \
        for (int _i_ = 0; _i_ < (se)->sz; _i_++) \
            for (const hset_elem *_el_ = (se)->table[_i_]; _el_; _el_ = _el_->next) \
                for (int _flag_ = 1; _flag_; _flag_ = 0) \
                    for (value64 var  = _el_->v; _flag_; _flag_ = 0)
// any type, allow del version
#define HSET_FOREACH_DEL(se, var) \
    for (int _i_ = 0; _i_ < (se)->sz; _i_++) \
        for (hset_elem *_el_ = (se)->table[_i_], *_next_ = NULL; _el_; _el_ = _next_) \
            for (int _flag_ = 1; _flag_; _flag_ = 0) \
                for (value64 var = _el_->v; _flag_; _flag_ = 0) \
                    for (int _once_ = (_next_ = _el_->next, 1); _once_; _once_ = 0)

// ------------------------------------- PRINTERS/CHECKERS ---------------------------------

extern int                  hset_techfprint(FILE *restrict out, const hset *se, int cnt, const char *restrict name);
static inline int           hset_techprint(const hset *restrict se, int cnt, const char *restrict name){
    return hset_techfprint(stdout, se, cnt, name);
}

#define                     HSET_TECH_FPRINT(out, se, cnt) hset_techfprint( (out), &(se), (cnt), #se)
#define                     HSET_TECH_FPRINTALL(out, se) hset_techfprint( (out), &(se), 0, #se)
#define                     HSET_TECH_PRINT(se, cnt) hset_techprint( &(se), (cnt), #se)
#define                     HSET_TECH_PRINTALL(se) hset_techprint( &(se), 0, #se)

// common validator
extern bool                 hset_validate(FILE *out, const hset *restrict se);

// --------------------------------- SERIALIZATION -----------------------------------------
// file
extern int                  hset_fsave(FILE  *restrict out, const hset *restrict se);
extern int                  hset_save(const char *restrict fname, const hset *restrict se);
extern int                  hset_fload(FILE *restrict in, hset *restrict se);
extern int                  hset_load(const char *restrict fname, hset *restrict se);
// db, TODO: to sqlite
extern int                  hset_dbsave(const char *restrict conn, const hset *restrict se, const char *st);
extern int                  hset_dbsave(const char *restrict conn, const hset *restrict se, const char *st);

// const
typedef                     void (*hset_const_proc_t)(value64 v);
// change //typedef                 void (*hset_proc_t)(value64 *v);
// modift structure typedef                 void (*hset_modify_proc_t)(hset *se, hset_elem *el);

#endif /* !_HASHSET_H */

