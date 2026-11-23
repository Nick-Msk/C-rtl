#ifndef _FS_ARRAY_H
#define _FS_ARRAY_H

// ---------------------------------------------------------------------------------------
// --------------------------- Public Faststring Array API -------------------------------
// ---------------------------------------------------------------------------------------

// --------------- Includes -----------------------

#include <stdio.h>
#include <string.h>
//
#include "bool.h"
#include "fs.h"
#include "log.h"
#include "checker.h"

// ------------------- CONSTANTS AND GLOBALS ---------------------------

// TODO: that should be in Context
static const int                G_FSARRAY_DEFAULT_INIT  = 10;
static const int                G_FSARRAY_LOAD_INC      = 50;

// --------------------------------- TYPES -----------------------------

typedef struct fsarray {
    int         sz;     // total fs  allocated
    int         cnt;
    fs         *ar;
} fsarray;

typedef struct fsl {
    fsarray     *a;
    int         pos;
} fsl;

// ------------------ API Constructs/Destrucor  ----------------------------
// returns count of freed
extern int                  fsarr_free(fsarray *arr);

extern fsarray              fsarr_init(int cnt);

extern fsarray              fsarr_fromarr(const char * const *arr, int cnt);

static inline fsarray       fsarr_empty(){
    return fsarr_init(G_FSARRAY_DEFAULT_INIT);
}

#define fsarrfree(s)    fsarr_free(&(s))
#define FSARRAY(...)    (fsarray) {.sz = 0, .ar = 0, .cnt = 0, ##__VA_ARGS__};

// -------------------- ACCESS AND MODIFICATORS ------------------------

// low level deatch
static inline fs            fsarr_detach(fsarray *arr, int pos){
    if (pos > arr->cnt || pos < 0)
        userraiseint(ERR_OUT_OF_RANGE, "Invalid pos(%d) while array cnt(%d)", pos, arr->cnt);
    fs s = fsmove(arr->ar[pos]);    // move via macro, fs_move undelaying
    return s;       // probably it's possible to not return noting
}

static inline bool          fsarr_attach(fsarray *arr, int pos, fs *s){
    if (pos > arr->cnt || pos < 0)
        return logsimpleerr(false, "Invalid pos(%d) while array cnt(%d)", pos, arr->cnt);
    if ( !fsisnull(arr->ar[pos] ) )
        return logsimpleerr(false, "Can't move to not nullable fs");
    arr->ar[pos] = fs_move(s);
    return true;    // probably better to return fs, but not true/false
}

// check if position exists
static inline bool          fsarr_exists(const fsarray *arr, int pos){
    return fsisnull(arr->ar[pos]);
}

// return a fs
static inline fs           *fsarr_get(const fsarray *arr, int pos){
    return arr->ar + pos;        // even if fsnull
}

extern int                  fsarr_increase(fsarray *arr, int newcnt);

static inline int           fsarr_increaseby(fsarray *arr, int add){
    return fsarr_increase(arr, arr->cnt + add);
}

extern int                  fsarr_shrink(fsarray *arr, int newcnt);

static inline int           fsarr_cnt(const fsarray *arr){
    return arr->cnt;
}

static inline int           fsarr_sz(const fsarray *arr){
    return arr->sz;
}
// empty or free allocared fs
extern int                  fsarr_clean(fsarray *arr, bool free);

// ---------------------------------------- Comparator -------------------------------------------

extern bool                 fsarr_cmp(const fsarray *restrict arr1, const fsarray *restrict arr2, int *restrict pos);

// -------------------------------------  FSL API ------------------------------------------------
// TODO: move to separate module
// fsl f1 = getfsl(fs_array a);
// f1.elem() = ...
// f1.get() = ...

static inline fsl           fsarr_getfsl(fsarray *arr, int pos){
    return (fsl){.a = arr, .pos = pos};
}

static inline fs           *fsl_fs(fsl l){
    return l.a->ar + l.pos; // pointer, it's impossible to copy fs!
}

// port to fs get()
static inline char         *fsl_get(fsl l, int p){
    return fs_get(fsl_fs(l), p);
}
// port to fs elem()
static inline char         *fsl_elem(fsl l, int p){
    return fs_elem(fsl_fs(l), p);
}

static inline int           fsl_len(fsl l){
    return fs_len(fsl_fs(l));
}

static inline fsl           fsl_cpystr(fsl l, const char *str){
    fs_cpystr(fsl_fs(l), str);
    return l;
}

#define                 fslelem(l, pos) *fsl_elem( (l), (pos) )
#define                 fslget(l, pos) *fsl_get( (l), (pos) )
#define                 fsarrget(arr, pos) *fsarr_get(&(arr), (pos) )
#define                 fsarrattach(arr, pos, s) fsarr_attach(&(arr), (pos), &(s) )
#define                 fsarrdetach(arr, pos) fsarr_detach(&(arr), (pos) )

// ------------------------ PRINTERS/CHECKERS --------------------------

extern int                  fsarr_fprint(FILE *restrict f, const fsarray *restrict arr);
static inline int           fsarr_print(const fsarray* arr){
    return fsarr_fprint(stdout, arr);
}

extern int                  fsarr_techfprint(FILE *restrict f, const fsarray *restrict arr);
static inline int           fsarr_techprint(const fsarray *arr){
    return fsarr_techfprint(stdout, arr);
}

extern int                  fsarr_techfprintlim(FILE *restrict f, const fsarray *restrict arr, int lim);
static inline int           fsarr_techprintlim(const fsarray *arr, int lim){
    return fsarr_techfprintlim(stdout, arr, lim);
}

extern bool                 fsarr_validate(FILE *restrict out, const fsarray *restrict arr);
extern bool                 fsarr_alloc_check(bool raise);
// ------------------------------ ETC. ---------------------------------

extern int                  fsarr_save(const char *restrict fname, const fsarray *restrict arr);
extern int                  fsarr_fsave(FILE *restrict f, const fsarray *restrict arr);

extern fsarray              fsarr_load(const char *fname);
extern fsarray              fsarr_fload(FILE *f);

extern int                  fsarr_fsavelines(FILE *restrict out, const fsarray *restrict arr, int cnt);
static inline int           fsarr_savelines(const fsarray *arr, int cnt){
    return fsarr_fsavelines(stdout, arr, cnt);
}

extern int                  fsarr_floadlines(FILE *restrict out, fsarray *restrict arr, int cnt);
static inline int           fsarr_loadlines(fsarray *arr, int cnt){
    return fsarr_floadlines(stdin, arr, cnt);
}

#endif /* !_FS_ARRAY_H */

