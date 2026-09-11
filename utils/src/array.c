// only array.h
#include "array.h"
#include "ds.h"

/********************************************************************
                 ARRAY MODULE IMPLEMENTATION
********************************************************************/

//  globals, can be changed by app

// TODO: context must be used for that
int                      g_array_rec_line        = 20;  // TODO: rework that to normal (in Array structure)

// not possible to format v64 that way!
static const int        g_array_acs_rndinc      = 5;
static const int        g_array_desc_rndinc     = 5;

// -------------------------- Factories ----------------------------

// -------------------------- 1 API method per 1 filler type! --------------------------------

typedef long                    (*ArrayFillRangeFunc)(Array *parr, size_t from, size_t to);

static long                      arrayFillRangeASC(Array *parr, size_t from, size_t to);
static long                      arrayFillRangeDESC(Array *parr, size_t from, size_t to);
static long                      arrayFillRangeZERO(Array *parr, size_t from, size_t to);
static long                      arrayFillRangeSAFEEMPTY(Array *parr, size_t from, size_t to);
static long                      arrayFillRangeRND(Array *parr, size_t from, size_t to);
static long                      arrayFillRangeASCSERIES(Array *parr, size_t from, size_t to);
static long                      arrayFillRangeDESCSERIES(Array *parr, size_t from, size_t to);

// general interface
typedef struct {
    ArrayFillRangeFunc      typefiller;     // fill part of array
    // others
} FilledInterface;

static const FilledInterface         FILLERINTERFACE[] = {
    [ARRAY_FILLTYPE_ASC]         = { .typefiller = arrayFillRangeASC },
    [ARRAY_FILLTYPE_DESC]        = { .typefiller = arrayFillRangeDESC },
    [ARRAY_FILLTYPE_ZERO]        = { .typefiller = arrayFillRangeZERO },
    [ARRAY_FILLTYPE_SAFE_EMPTY]  = { .typefiller = arrayFillRangeSAFEEMPTY },
    [ARRAY_FILLTYPE_RND]         = { .typefiller = arrayFillRangeRND },
    [ARRAY_FILLTYPE_ASC_SERIES]  = { .typefiller = arrayFillRangeASCSERIES },
    [ARRAY_FILLTYPE_DESC_SERIES] = { .typefiller = arrayFillRangeDESCSERIES },
};

// ----------------------------- Basic Filler Interfaces -------------------------------------
static const FilledInterface      *getFilledInterface(ArrayFillType filltyp) {
    if (filltyp < 0 || filltyp >= COUNT(FILLERINTERFACE) ) 
        return userraise(NULL, ERR_UNSUPPORTED_INTERFACE, 
                    "Unable to find  filler interface for %d/%s", 
                        filltyp, arrayFillTypeGetName(filltyp));
    return &FILLERINTERFACE[filltyp];
}

// ---------------------- TYPE FILLERS -----------------------------
// factory-wrappers -- ACS (rnd) series
static v64Gen                   f_v64_acs_int(long c, long s, int i) { 
    return v64GenCreatorAscRnd(v64typedCreateInt(s), c, i); 
}
static v64Gen                   f_v64_acs_long(long c, long s, int i) {
    return v64GenCreatorAscRnd(v64typedCreateLong(s), c, i); 
}
static v64Gen                   f_v64_acs_ulong(long c, long s, int i) {
    return v64GenCreatorAscRnd(v64typedCreateULong(s), c, i); 
}
static v64Gen                   f_v64_acs_dbl(long c, long s, int i)  { 
    return v64GenCreatorAscRnd(v64typedCreateDbl(s), c, i); 
}
static v64Gen                   f_v64_acs_chr(long c, long s, int i)  { 
    return v64GenCreatorAscRnd(v64typedCreateChar(ucharmax(s)), c, i); 
}
static v64Gen                   f_v64_acs_bool(long c, long s, int i) { 
    return v64GenCreatorAscRnd(v64typedCreateBool(s != 0), c, i); 
}
static v64Gen                   f_v64_acs_str(long c, long s, int i)  { 
    return v64GenCreatorAscStrRnd(c, s, "%d", i); 
}
static v64Gen                   f_v64_acs_fs(long c, long s, int i)   { 
    return v64GenCreatorAscFsRnd(c, s, "%d", i); 
}
// factory-wrappers -- DESC (rnd) series
static v64Gen                   f_v64_desc_int(long c, long s, int i) { 
    return v64GenCreatorDescRnd(v64typedCreateInt(s), c, i); 
}
static v64Gen                   f_v64_desc_long(long c, long s, int i) {
    return v64GenCreatorDescRnd(v64typedCreateLong(s), c, i); 
}
static v64Gen                   f_v64_desc_ulong(long c, long s, int i) {
    return v64GenCreatorDescRnd(v64typedCreateULong(s), c, i); 
}
static v64Gen                   f_v64_desc_dbl(long c, long s, int i)  { 
    return v64GenCreatorDescRnd(v64typedCreateDbl(s), c, i); 
}
static v64Gen                   f_v64_desc_chr(long c, long s, int i)  { 
    return v64GenCreatorDescRnd(v64typedCreateChar(ucharmax(s)), c, i); 
}
static v64Gen                   f_v64_desc_bool(long c, long s, int i) { 
    return v64GenCreatorDescRnd(v64typedCreateBool(s != 0), c, i); 
}
static v64Gen                   f_v64_desc_str(long c, long s, int i)  { 
    return v64GenCreatorDescStrRnd(c, s, "%d", i); 
}
static v64Gen                   f_v64_desc_fs(long c, long s, int i)   { 
    return v64GenCreatorDescFsRnd(c, s, "%d", i); 
}
// factory-wrappers -- ACS series (1, 2, 3 ... etc)
static v64Gen                   f_v64_acs_series_int(long c, long s, int i) {
    (void) i;
    return v64GenCreatorAscSeries(v64typedCreateInt(s), c); 
}
static v64Gen                   f_v64_acs_series_long(long c, long s, int i) {
    (void) i;
    return v64GenCreatorAscSeries(v64typedCreateLong(s), c); 
}
static v64Gen                   f_v64_acs_series_ulong(long c, long s, int i) {
    (void) i;
    return v64GenCreatorAscSeries(v64typedCreateULong(s), c); 
}
static v64Gen                   f_v64_acs_series_dbl(long c, long s, int i)  { 
    (void) i;
    return v64GenCreatorAscSeries(v64typedCreateDbl(s), c); 
}
static v64Gen                   f_v64_acs_series_chr(long c, long s, int i)  { 
    (void) i;
    return v64GenCreatorAscSeries(v64typedCreateChar(ucharmax(s)), c); 
}
static v64Gen                   f_v64_acs_series_bool(long c, long s, int i) { 
    (void) i;
    return v64GenCreatorAscSeries(v64typedCreateBool(s != 0), c); 
}
static v64Gen                   f_v64_acs_series_str(long c, long s, int i) { 
    (void) i;
    return v64GenCreatorAscStrSeries(c, s, "%d"); 
}
static v64Gen                   f_v64_acs_series_fs(long c, long s, int i) {
    (void) i; 
    return v64GenCreatorAscFsSeries(c, s, "%d"); 
}
// factory-wrappers -- DESC series (5, 4, 3 ... etc)
static v64Gen                   f_v64_desc_series_int(long c, long s, int i) {
    (void) i;
    return v64GenCreatorDescSeries(v64typedCreateInt(s), c); 
}
static v64Gen                   f_v64_desc_series_long(long c, long s, int i) {
    (void) i;
    return v64GenCreatorDescSeries(v64typedCreateLong(s), c); 
}
static v64Gen                   f_v64_desc_series_ulong(long c, long s, int i) {
    (void) i;
    return v64GenCreatorDescSeries(v64typedCreateULong(s), c); 
}
static v64Gen                   f_v64_desc_series_dbl(long c, long s, int i)  { 
    (void) i;
    return v64GenCreatorDescSeries(v64typedCreateDbl(s), c); 
}
static v64Gen                   f_v64_desc_series_chr(long c, long s, int i)  { 
    (void) i;
    return v64GenCreatorDescSeries(v64typedCreateChar(ucharmax(s)), c); 
}
static v64Gen                   f_v64_desc_series_bool(long c, long s, int i) { 
    (void) i;
    return v64GenCreatorDescSeries(v64typedCreateBool(s != 0), c); 
}
static v64Gen                   f_v64_desc_series_str(long c, long s, int i) { 
    (void) i;
    return v64GenCreatorDescStrSeries(c, s, "%d"); 
}
static v64Gen                   f_v64_desc_series_fs(long c, long s, int i) {
    (void) i; 
    return v64GenCreatorDescFsSeries(c, s, "%d"); 
}
// --- RND Master Factories (Internal) ---
// Параметры: (count, start, increment)
static v64Gen                   f_v64_rnd_int(long c, long s, int i)  { 
    (void) s;
    return v64GenCreatorRnd(VALUE64_INT, c, i); 
}
static v64Gen                   f_v64_rnd_long(long c, long s, int i) { 
    (void) s;
    return v64GenCreatorRnd(VALUE64_LONG, c, i); 
}
static v64Gen                   f_v64_rnd_ulong(long c, long s, int i) { 
    (void) s;
    return v64GenCreatorRnd(VALUE64_ULONG, c, i); 
}
static v64Gen                   f_v64_rnd_dbl(long c, long s, int i) { 
    (void) s;
    return v64GenCreatorRnd(VALUE64_DBL, c, i); 
}
static v64Gen                   f_v64_rnd_chr(long c, long s, int i) { 
    (void) s;
    return v64GenCreatorRnd(VALUE64_CHR, c, i); 
}
static v64Gen                   f_v64_rnd_bool(long c, long s, int i) { 
    (void) s;
    (void) i;
    return v64GenCreatorRnd(VALUE64_BOOL, c, 1); 
}
static v64Gen                   f_v64_rnd_str(long c, long s, int i) { 
    (void) s;
    return v64GenCreatorStrRnd(c, "%d", i); 
}
static v64Gen                   f_v64_rnd_fs(long c, long s, int i) { 
    (void) s;
    return v64GenCreatorFsRnd(c, "%d", i); 
}
// --- SAFE_EMPTY Master Factories (Internal) ---
static v64Gen                   f_v64_empty_provider_fs(long c, long s, int i) {
    (void) i;
    (void) s;
    return v64GenCreatorNull(VALUE64_FS, c);
}
static v64Gen                   f_v64_empty_provider_str(long c, long s, int i) {
    (void) i;
    (void) s;
    return v64GenCreatorNull(VALUE64_STR, c);
}
// --- ZERO Master Factories (Internal) ---
static v64Gen                   f_v64_zero_provider_int(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_INT, c);
}
static v64Gen                   f_v64_zero_provider_long(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_LONG, c);
}
static v64Gen                   f_v64_zero_provider_ulong(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_ULONG, c);
}
static v64Gen                   f_v64_zero_provider_dbl(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_DBL, c);
}
static v64Gen                   f_v64_zero_provider_chr(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_CHR, c);
}
static v64Gen                   f_v64_zero_provider_bool(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_BOOL, c);
}
static v64Gen                   f_v64_zero_provider_str(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_STR, c);
}
static v64Gen                   f_v64_zero_provider_fs(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_FS, c);
}
static v64Gen                   f_v64_zero_provider_ptr(long c, long s, int i) {
    (void) s; (void) i;
    return v64GenCreatorZero(VALUE64_PTR, c);
}
// 
static const v64GenTypedFactory         ARRAYTYPEFILLERINTERFACE[][ARRAY_FILLTYPE_MAX] = {
    [VALUE64_INT]   = { [ARRAY_FILLTYPE_ASC]         = f_v64_acs_int,   
                        [ARRAY_FILLTYPE_DESC]        = f_v64_desc_int,
                        [ARRAY_FILLTYPE_ASC_SERIES]  = f_v64_acs_series_int,   
                        [ARRAY_FILLTYPE_DESC_SERIES] = f_v64_desc_series_int,
                        [ARRAY_FILLTYPE_RND]         = f_v64_rnd_int,
                        [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_int
                    },
    [VALUE64_LONG]  = { [ARRAY_FILLTYPE_ASC]         = f_v64_acs_long,  
                        [ARRAY_FILLTYPE_DESC]        = f_v64_desc_long,
                        [ARRAY_FILLTYPE_ASC_SERIES]  = f_v64_acs_series_long,  
                        [ARRAY_FILLTYPE_DESC_SERIES] = f_v64_desc_series_long,
                        [ARRAY_FILLTYPE_RND]         = f_v64_rnd_long,
                        [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_long
                    },
    [VALUE64_ULONG] = { [ARRAY_FILLTYPE_ASC]         = f_v64_acs_ulong, 
                        [ARRAY_FILLTYPE_DESC]        = f_v64_desc_ulong,
                        [ARRAY_FILLTYPE_ASC_SERIES]  = f_v64_acs_series_ulong, 
                        [ARRAY_FILLTYPE_DESC_SERIES] = f_v64_desc_series_ulong,
                        [ARRAY_FILLTYPE_RND]         = f_v64_rnd_ulong,
                        [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_ulong
                    },
    [VALUE64_DBL]   = { [ARRAY_FILLTYPE_ASC]         = f_v64_acs_dbl,   
                        [ARRAY_FILLTYPE_DESC]        = f_v64_desc_dbl,
                        [ARRAY_FILLTYPE_ASC_SERIES]  = f_v64_acs_series_dbl,   
                        [ARRAY_FILLTYPE_DESC_SERIES] = f_v64_desc_series_dbl,
                        [ARRAY_FILLTYPE_RND]         = f_v64_rnd_dbl,
                        [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_dbl
                    },
    [VALUE64_CHR]   = { [ARRAY_FILLTYPE_ASC]         = f_v64_acs_chr,   
                        [ARRAY_FILLTYPE_DESC]        = f_v64_desc_chr,
                        [ARRAY_FILLTYPE_ASC_SERIES]  = f_v64_acs_series_chr,   
                        [ARRAY_FILLTYPE_DESC_SERIES] = f_v64_desc_series_chr,
                        [ARRAY_FILLTYPE_RND]         = f_v64_rnd_chr,
                        [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_chr
                    },
    [VALUE64_BOOL]  = { [ARRAY_FILLTYPE_ASC]         = f_v64_acs_bool,  
                        [ARRAY_FILLTYPE_DESC]        = f_v64_desc_bool,
                        [ARRAY_FILLTYPE_ASC_SERIES]  = f_v64_acs_series_bool,  
                        [ARRAY_FILLTYPE_DESC_SERIES] = f_v64_desc_series_bool,
                        [ARRAY_FILLTYPE_RND]         = f_v64_rnd_bool,
                        [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_bool
                    },
    [VALUE64_STR]   = { [ARRAY_FILLTYPE_ASC]         = f_v64_acs_str,   
                        [ARRAY_FILLTYPE_DESC]        = f_v64_desc_str,
                        [ARRAY_FILLTYPE_ASC_SERIES]  = f_v64_acs_series_str,   
                        [ARRAY_FILLTYPE_DESC_SERIES] = f_v64_desc_series_str,
                        [ARRAY_FILLTYPE_RND]         = f_v64_rnd_str,
                        [ARRAY_FILLTYPE_SAFE_EMPTY]  = f_v64_empty_provider_str,
                        [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_str
                    },
    [VALUE64_FS]    = { [ARRAY_FILLTYPE_ASC]         = f_v64_acs_fs,    
                        [ARRAY_FILLTYPE_DESC]        = f_v64_desc_fs,
                        [ARRAY_FILLTYPE_ASC_SERIES]  = f_v64_acs_series_fs,    
                        [ARRAY_FILLTYPE_DESC_SERIES] = f_v64_desc_series_fs,
                        [ARRAY_FILLTYPE_RND]         = f_v64_rnd_fs,
                        [ARRAY_FILLTYPE_SAFE_EMPTY]  = f_v64_empty_provider_fs,
                        [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_fs
                    },
    [VALUE64_PTR]   = { [ARRAY_FILLTYPE_ZERO]        = f_v64_zero_provider_ptr
                    },
    [VALUE64_UNKNOWN] = {0} 
};
// internal, type-filler constructor
static v64GenTypedFactory          getTypedFillFactory(value64_type vt64, ArrayFillType ft) {
    if (vt64 < 0 || vt64 >= COUNT(ARRAYTYPEFILLERINTERFACE) || ft < 0 || ft >= ARRAY_FILLTYPE_MAX)
        NULL;
    return ARRAYTYPEFILLERINTERFACE[vt64][ft];
}

// ----------------------------- Basic Array Interfaces -------------------------------------
// ----- Sorting, binary search ect... 1 API method per 1 ArrayType type!

typedef bool                    (*ArrayCompareFunc)(const Array *restrict, const Array *restrict);

static bool                     equal_int(const Array *p1, const Array *p2) {
    for (size_t i = 0; i < p1->len; i++)
        if (p1->iv[i] != p2->iv[i])
            return false;
    return true;
}
static bool                     equal_long(const Array *p1, const Array *p2) {
    for (size_t i = 0; i < p1->len; i++)
        if (p1->lv[i] != p2->lv[i])
            return false;
    return true;
}
static bool                     equal_double(const Array *p1, const Array *p2) {
    for (size_t i = 0; i < p1->len; i++)
        if (p1->dv[i] != p2->dv[i])
            return false;
    return true;
}
static bool                     equal_pointer(const Array *p1, const Array *p2) {
    for (size_t i = 0; i < p1->len; i++)
        if (p1->pv[i] != p2->pv[i])
            return false;
    return true;
}
static bool                     equal_char(const Array *p1, const Array *p2) {
    for (size_t i = 0; i < p1->len; i++)
        if (p1->cv[i] != p2->cv[i])
            return false;
    return true;
}
static bool                     equal_v64(const Array *p1, const Array *p2) {
    for (size_t i = 0; i < p1->len; i++)
        if (!value64_equal(p1->v64[i], p2->v64[i], p1->v64type))
            return false;
    return true;
}

//typedef                         int (*pointer_comparator)(const void *restrict i1, const void *restrict i2);
typedef                         pointer_comparator (*QsortGetcompFunc)(const Array *parr, ArraySortType ord);

#define DEFINE_SIMPLE_QSORT_FACTORY(name, asc_func, desc_func) \
static pointer_comparator       get_##name##_cmp(const Array *p, ArraySortType ord) { \
    (void) p; \
    return (ord == ARRAY_SORTTYPE_ASC) ? (pointer_comparator) asc_func : (pointer_comparator) desc_func; \
}

DEFINE_SIMPLE_QSORT_FACTORY(int,        pint_cmp,  pint_revcmp)
DEFINE_SIMPLE_QSORT_FACTORY(long,       plong_cmp, plong_revcmp)
DEFINE_SIMPLE_QSORT_FACTORY(double,     pdbl_cmp,  pdbl_revcmp)
DEFINE_SIMPLE_QSORT_FACTORY(pointer,    pptr_cmp,  pptr_revcmp)
DEFINE_SIMPLE_QSORT_FACTORY(char,       pchar_cmp, pchar_revcmp)

#undef DEFINE_SIMPLE_QSORT_FACTORY

static pointer_comparator               get_v64_cmp(const Array *parr, ArraySortType ord) {
    return (ord == ARRAY_SORTTYPE_ASC) 
            ? value64_getPComparator(parr->v64type) 
            : value64_getPRevComparator(parr->v64type);
}

typedef long                   (*ArrayPumpFunc)(const Array*, v64Gen*, size_t, size_t);

static long                     pump_v64(const Array *p, v64Gen *g, size_t from, size_t to) {
    int cnt = 0;
    for (size_t i = from; i < to && v64GenHasnext(g); i++) {
        p->v64[i] = v64GenNext(g);
        cnt++;
    }
    return cnt;
}
static long                     pump_int(const Array *p, v64Gen *g, size_t from, size_t to) {
    long cnt = 0;
    for (size_t i = from; i < to && v64GenHasnext(g); i++) {
        p->iv[i] = v64GenNext(g).ival;
        cnt++;
    }
    return cnt;
}
static long                     pump_long(const Array *p, v64Gen *g, size_t from, size_t to) {
    long cnt = 0;
    for (size_t i = from; i < to && v64GenHasnext(g); i++) {
        p->lv[i] = v64GenNext(g).lval;
        cnt++;
    }
    return cnt;
}
static long                     pump_double(const Array *p, v64Gen *g, size_t from, size_t to) {
    long cnt = 0;
    for (size_t i = from; i < to && v64GenHasnext(g); i++) {
        p->dv[i] = v64GenNext(g).dval;
        cnt++;
    }
    return cnt;
}
static long                     pump_char(const Array *p, v64Gen *g, size_t from, size_t to) {
    long cnt = 0;
    for (size_t i = from; i < to && v64GenHasnext(g); i++) {
        p->cv[i] = v64GenNext(g).cval;
        cnt++;
    }
    return cnt;
}
static long                     pump_pointer(const Array *p, v64Gen *g, size_t from, size_t to) {
    long cnt = 0;
    for (size_t i = from; i < to && v64GenHasnext(g); i++) {
        p->pv[i] = v64GenNext(g).pval;
        cnt++;
    }
    return cnt;
}


// general interface
typedef struct {
    ArrayCompareFunc            basic_comparator;     // fill part of array
    QsortGetcompFunc            get_comparator;       // per element
    ArrayPumpFunc               pump;
    // others
} ArrayInterface;

static const                    ArrayInterface ARRAYINTERFACE[] = {
    [ARRAY_INT]     = { .basic_comparator   = equal_int, 
                        .get_comparator     = get_int_cmp,
                        .pump               = pump_int
                      },
    [ARRAY_LONG]    = { .basic_comparator   = equal_long,
                        .get_comparator     = get_long_cmp,
                        .pump               = pump_long
                      },
    [ARRAY_DOUBLE]  = { .basic_comparator   = equal_double, 
                        .get_comparator     = get_double_cmp,
                        .pump               = pump_double
                      },
    [ARRAY_POINTER] = { .basic_comparator   = equal_pointer,
                        .get_comparator     = get_pointer_cmp,
                        .pump               = pump_pointer
                      },
    [ARRAY_CHAR]    = { .basic_comparator   = equal_char,
                        .get_comparator     = get_char_cmp,
                        .pump               = pump_char
                      },
    [ARRAY_V64]     = { .basic_comparator   = equal_v64,
                        .get_comparator     = get_v64_cmp,
                        .pump               = pump_v64
                      }
};

static const ArrayInterface       *getTypedInterface(ArrayType typ) {
    if (typ < 0 || typ >= COUNT(ARRAYINTERFACE) ) 
        return userraise(NULL, ERR_UNSUPPORTED_INTERFACE, 
                    "Unable to find  filler interface for %d/%s", 
                        typ, arrayTypeGetName(typ));
    return &ARRAYINTERFACE[typ];
}

// -------------------------- Utilities ------------------------

/**
 * @brief Allocates and initializes a new Array descriptor.
 * 
 * @details This is a low-level constructor that allocates memory for the 
 *          Array structure itself. It uses @ref ArrayInit to set the 
 *          initial state of the descriptor. 
 * 
 * @note This function does NOT allocate the data buffer (the `v` pointer). 
 *       It only prepares the container structure. The caller is responsible 
 *       for the lifetime of the returned pointer and must call @ref arrayFree 
 *       to prevent memory leaks.
 * 
 * @param typ The primary type assigned to the array (e.g., ARRAY_INT, ARRAY_V64).
 * @param vt  The specialized value64 subtype (only relevant if `typ` is ARRAY_V64).
 * 
 * @return Array* A pointer to the newly allocated Array structure. 
 *         Returns NULL (via @ref userraise) if memory allocation fails.
 */
static inline Array             *arraycreate(ArrayType typ, value64_type vt) {
    Array *arr = malloc(sizeof(Array));
    if (!arr)
        return userraise(NULL, ERR_UNABLE_ALLOCATE, "Unable create Array structure");
    *arr = ArrayInit(.flags = typ, .v64type = vt);
    return arr;
}

static inline void             fixbysz(Array *parr, size_t *pos) {
    if (*pos > arraysz(parr)) {
        logsimple("postion %zu is out of bound, cut to sz %zu", *pos, arraysz(parr));
        *pos = arraysz(parr);
    }
}
// static inline void             fixbylen(Array *parr, size_t *pos) {
//     if (*pos > arraysz(parr)) {
//         logsimple("postion %d is out of bound, cut to sz %d", *pos, arraysz(parr));
//         *pos = arraysz(parr);
//     }
// }
static inline void              fixrangesbysz(Array *parr, size_t *from, size_t *to) {
    fixbysz(parr, from);
    fixbysz(parr, to);
    // from > to isn't checker for now
}
// static inline void              fixrangesbylen(Array *parr, size_t *from, size_t *to) {
//     fixbylen(parr, from);
//     fixbylen(parr, to);
//     // from > to isn't checker for now
// }

/// @brief free value64 elements of array
/// @param arr pointer to array
/// @param from from
/// @param to to
static void                     freeV64elems(Array *parr, size_t from, size_t to) {
    invraisecode(ERR_NULLABLE_PTR, parr != NULL, "Null pointer");

    fixrangesbysz(parr, &from, &to);
    if (parr->v64type == VALUE64_STR || parr->v64type == VALUE64_FS) {   
        for (size_t i = from; i < to; i++) {
            value64free(parr->v64[i], parr->v64type);
        }
        logsimple("freed %s  %zu - %zu", value64_typename(parr->v64type), from, to);
    }
}

/// @brief increase or descrease size of array
/// @param arr pointer to array
/// @param newsz new size
/// @return 
static size_t                    increase(Array *parr, size_t newsz){
    invraisecode(parr != NULL, ERR_NULLABLE_PTR,
        "Null pointer %p", parr);

    if (newsz == parr->sz)
        return logsimpleret(parr->sz, "No change sz %zu", parr->sz);
    if (newsz > parr->sz)
        newsz = round_up_2(newsz);

    if (arrayGetelemsize(parr) == 0L)
        return userraise(0L, ERR_UNKNOWN_TYPE, "Unknown type, elem size == 0");

    size_t      bytes = newsz * arrayGetelemsize(parr);        

    if (newsz < parr->len)
        freeV64elems(parr, newsz, parr->len);    

    void       *p = NULL;  
    if (bytes > 0) {
        if ( (p = realloc(parr->v, bytes) ) == NULL)
            userraiseint(ERR_UNABLE_ALLOCATE, "Unable to allocate %zu", bytes);
    } else
        free(parr->v);

    parr->v = p; // iv/dv/pv... is the same
    if (parr->len > newsz)   // shrink case, 0 if newsz == 0 (free)
        parr->len = newsz;
    parr->sz = newsz;
    return logsimpleret(parr->sz, "New sz %zu, new v = %p", parr->sz, parr->v);
}

/**
 * @brief Internal utility to shift a block of elements within the array buffer.
 *
 * This function uses memmove to safely handle overlapping memory regions.
 * It is a low-level primitive used for both deletions and insertions.
 *
 * @param parr      Pointer to the array.
 * @param dest_idx  The target starting index for the block.
 * @param src_idx   The original starting index of the block.
 * @param cnt       The number of elements to move.
 * @return          The number of elements moved.
 */
static int                  moveelem(Array *parr, size_t dest_idx, size_t src_idx, size_t cnt) {
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Null array pointer");

    size_t es = arrayGetelemsize(parr);

    // memmove is used to safely handle overlapping memory regions
    memmove((char *)parr->v + (dest_idx * es), 
            (char *)parr->v + (src_idx * es), 
            cnt * es);
    
    return cnt;
}

// -------------------------- (Utility) printers -------------------

// --------------------------- API ---------------------------------
// ------------- CONSTRUCTOTS/DESTRUCTORS --------------

// CREATE  and fill with method
Array                          *arrayCreate(size_t cnt, ArrayFillType filltyp, ArrayType typ, value64_type vt){
    logenter("cnt %zu, filltyp %s typ %s", cnt, arrayFillTypeGetName(filltyp), arrayTypeGetName(typ) );
    // TODO: refactor via arrayIncrease
    Array   *res = arraycreate(typ, vt);      
    
    if (increase(res, cnt) < 0)
        return userraise(NULL, ERR_UNABLE_ALLOCATE, "Unable to allocate %zu elems", cnt);
    else {
        res->len = cnt;
        arrayFillAll(res, filltyp);
    }
    return logret(res, "sz = %zu, len = %zu", res->sz, res->len );
}

void                            arrayFreeBody(Array *val) {
    if (val)
        increase(val, 0);
}

/// @brief free array
/// @param val pointer to array
/// @note: arrayFree must not failed even if val == NULL
void                           arrayFree(Array *val){
    if (val) {      // arrayFree must not failed even if val == NULL
        arrayFreeBody(val);
        free(val);
        val = NULL;
    }
}

/// @brief        Array filler (formatter) using fill type
/// @param a  Array (by value now, will be reworked)
/// @param typ  Array type 
/// @param vt   V64 type, only for for V64
/// @return Count of formatter data
long                            arrayFillAll(Array *parr, ArrayFillType typ){
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Null input");
    return arrayFillRange(parr, typ, 0, parr->len);
}


/// @brief        ascending filler (random increase)
/// @param parr   array 
/// @param from   start index 
/// @param to     end index 
/// @return       count of filled elements
static long                     arrayFillRangeASC(Array *parr, size_t from, size_t to) {

    value64_type              vt64 = arrayGetV64mapType(parr);
    v64GenTypedFactory        ti = getTypedFillFactory(vt64, ARRAY_FILLTYPE_ASC);

    if (ti) {
        v64Gen gen = ti(to - from, from, g_array_acs_rndinc);

        long cnt = arrayGenPumprange(parr, &gen, from, to);
        v64GenFree(&gen);

        return cnt;
    } else 
        return userraise(-1, ERR_ACTION_NOT_APPLICABLE,
                            "Type mismatch: Type %d/%s (v64: %d/%s) does not support ASC fill",
                            arrayGettype(parr), arrayGetTypeName(parr),
                            parr->v64type, arrayGetV64typeName(parr));
}

/// @brief        descending filler
/// @param parr   array 
/// @param from   start index 
/// @param to     end index 
/// @return       count of filled elements
static long                     arrayFillRangeDESC(Array *parr, size_t from, size_t to) {

    value64_type              vt64 = arrayGetV64mapType(parr);
    const long                start_num = (to - from + 1) * g_array_desc_rndinc;
    v64GenTypedFactory        ti = getTypedFillFactory(vt64, ARRAY_FILLTYPE_DESC);

    if (ti) {
        v64Gen gen = ti(to - from, start_num, g_array_desc_rndinc);

        long cnt = arrayGenPumprange(parr, &gen, from, to);
        v64GenFree(&gen);
    
        return cnt;
    } else {
        return userraise(-1, ERR_ACTION_NOT_APPLICABLE,
                            "Type mismatch: Type %d/%s (v64: %d/%s) does not support DESC fill",
                            arrayGettype(parr), arrayGetTypeName(parr),
                            parr->v64type, arrayGetV64typeName(parr));
    }
}


/// @brief        zero filler
/// @param parr   array 
/// @param from   start index 
/// @param to     end index 
/// @return       count of filled elements
static long                     arrayFillRangeZERO(Array *parr, size_t from, size_t to){

    value64_type    vt64 = arrayGetV64mapType(parr);

    v64GenTypedFactory ti = getTypedFillFactory(vt64, ARRAY_FILLTYPE_ZERO);

    if (ti) {
        // first param - c [ count ]
        v64Gen gen = ti(to - from, 0, 0);
        long cnt = arrayGenPumprange(parr, &gen, from, to);
        v64GenFree(&gen);

        return cnt;
    } else
        return 0;
}

/// @brief        none filler (fs & str)
/// @param parr   array 
/// @param from   start index 
/// @param to     end index 
/// @return       count of filled elements
/// @note         no generator here!
static long                     arrayFillRangeSAFEEMPTY(Array *parr, size_t from, size_t to) {
    value64_type    vt64 = arrayGetV64mapType(parr);

    v64GenTypedFactory ti = getTypedFillFactory(vt64, ARRAY_FILLTYPE_SAFE_EMPTY);

    if (ti) {
        v64Gen gen = ti(to - from, 0, 0);
        long cnt = arrayGenPumprange(parr, &gen, from, to);
        v64GenFree(&gen);

        return cnt;
    } else
        return 0;
}

/// @brief        random value filler
/// @param parr   array 
/// @param from   start index 
/// @param to     end index 
/// @return       count of filled elements
static long                     arrayFillRangeRND(Array *parr, size_t from, size_t to) {
    const long      rnd_max = 10 * (to - from);
    const long      rndinc  = to - from;
    value64_type    vt64 = arrayGetV64mapType(parr);

    v64GenTypedFactory ti = getTypedFillFactory(vt64, ARRAY_FILLTYPE_RND);

    if (ti) {
        v64Gen gen = ti(rnd_max, 0 /* not used*/, rndinc);
        long cnt = arrayGenPumprange(parr, &gen, from, to);
        v64GenFree(&gen);

        return cnt;
    } else 
        return userraise(-1, ERR_ACTION_NOT_APPLICABLE,
                            "Unsupported v64 type for RND fill: %d/%s or  %d/%s",
                            arrayGettype(parr), arrayGetTypeName(parr),
                            parr->v64type, arrayGetV64typeName(parr));
}

/// @brief        ascending series filler
/// @param a      array 
/// @param from   start index 
/// @param to     end index 
/// @return       count of filled elements
static long                     arrayFillRangeASCSERIES(Array *parr, size_t from, size_t to){
    value64_type              vt64 = arrayGetV64mapType(parr);
    v64GenTypedFactory        ti = getTypedFillFactory(vt64,  ARRAY_FILLTYPE_ASC_SERIES);

    if (ti) {
        v64Gen gen = ti(to - from, from, 1);

        long cnt = arrayGenPumprange(parr, &gen, from, to);
        v64GenFree(&gen);

        return cnt;
    } else 
        return userraise(-1, ERR_ACTION_NOT_APPLICABLE,
                            "Type mismatch: Type %d/%s (v64: %d/%s) does not support ASC series fill",
                            arrayGettype(parr), arrayGetTypeName(parr),
                            parr->v64type, arrayGetV64typeName(parr));
}

/// @brief        descending series filler
/// @param a      array 
/// @param from   start index 
/// @param to     end index 
/// @return       count of filled elements
static long                     arrayFillRangeDESCSERIES(Array *parr, size_t from, size_t to) {
    value64_type              vt64 = arrayGetV64mapType(parr);
    v64GenTypedFactory        ti = getTypedFillFactory(vt64, ARRAY_FILLTYPE_DESC_SERIES);
    const long                start_num = (to - 1); // the same as for scalar types

    if (ti) {
        v64Gen gen = ti(to - from, start_num, 1);

        long cnt = arrayGenPumprange(parr, &gen, from, to);
        v64GenFree(&gen);

        return cnt;
    } else 
        return userraise(-1, ERR_ACTION_NOT_APPLICABLE,
                            "Type mismatch: Type %d/%s (v64: %d/%s) does not support ASC series fill",
                            arrayGettype(parr), arrayGetTypeName(parr),
                            parr->v64type, arrayGetV64typeName(parr));
}

/// @brief Array filler
/// @param a base array
/// @param typ Array type 
/// @param from from (will be normilized if out of range)
/// @param to  to (will be normilized if out of range)
/// @return Count of formatter data
long                            arrayFillRange(Array *parr, ArrayFillType filltyp, size_t from, size_t to) {
    if (!parr)
        return userraise(-1, ERR_NULLABLE_PTR, "Null parr");
    logenter("%zu - %zu, %s (%s/v64: %s)", 
            from, to, arrayFillTypeGetName(filltyp), arrayGetTypeName(parr), arrayGetV64typeName(parr) );
    
    const FilledInterface      *fi = getFilledInterface(filltyp);
    if (fi->typefiller) {
        fixrangesbysz(parr, &from, &to);
        long cnt = fi->typefiller(parr, from, to);

        return logret(cnt, "Filled by %s %ld", arrayFillTypeGetName(filltyp), cnt);
    } else
        return userraise(-1, ERR_ACTION_NOT_APPLICABLE, 
                "Not supported filltype %d/%s", filltyp, arrayFillTypeGetName(filltyp));
}

// -------------- ACCESS AND MODIFICATION --------------

Array                          *arrayIncrease(Array *parr, size_t newcnt){
    if (newcnt > arraysz(parr) )
        increase(parr, newcnt);
    arrayFillRange(parr, ARRAY_FILLTYPE_ZERO, parr->len, newcnt);
    parr->len = newcnt;
    return parr;
}

Array                          *arrayShrink(Array *parr, size_t newsz){
    fixbysz(parr, &newsz);
    increase(parr, newsz);

    return logsimpleret(parr, "shrinked to (len %zu == sz %zu)", parr->len, parr->sz);
}

/**
 * @brief Shuffle array elements using the Fisher–Yates algorithm.
 * @param arr array (by value)
 */
Array                          *arrayShuffle(Array *parr) {
    size_t elem_size = arrayGetelemsize(parr);
    if (elem_size <= 0)
        userraise(NULL, ERR_UNSUPPORTED_TYPE, 
                        "unsupported type for shuffle %d/%s", arrayGettype(parr), arrayGetTypeName(parr));
    // in case if arr.len < 2 that'll do nothing
    if (parr->len > 1) {
        char    *data = parr->v;  // raw byte pointer
        for (size_t i = parr->len - 1; i > 0; i--) {
            int j = rndint(i);
            // swap elements at indices i and j
            item_exch(data + i * elem_size, data + j * elem_size, elem_size);   
        }
    }
    return parr;
}

/**
 * @brief Removes elements from the array within the specified range.
 * 
 * @details This function removes @p cnt elements starting from index @p from.
 * If the requested range `[from, from + cnt)` exceeds the current array length, 
 * the function will automatically adjust @p cnt to remove only elements up to 
 * the end of the array (clamping).
 * 
 * @note The function ensures no memory leaks by cleaning up the content of 
 *       removed elements (e.g., for V64 or pointer types) before shifting.
 * 
 * @param arr   Pointer to the array.
 * @param from  The starting index of the removal range.
 * @param cnt   The number of elements to remove.
 * 
 * @return      Pointer to the modified array. Returns the original array 
 *              unchanged if `cnt <= 0` or `from` is out of bounds.
 * 
 * @warning The function performs a linear time O(n) shift operation.
 */
Array                        *arrayDel(Array *parr, size_t from, size_t cnt) {
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, 
        "Null array pointer %p", parr);

    // THINK: refactor is required
    if (from >= parr->len) {
        return logsimpleret(parr, "Nothing to del from %zu, cnt %zu, len %zu", from, cnt, parr->len);
    }
    if ( (from + cnt) > parr->len) {
        logsimple("since %zu + %zu > total len %zu cnt reduced to %zu", 
                from, cnt, parr->len, parr->len - from);
        cnt = parr->len - from;
    }

    // no checking slices for now TODO:
    // ArraySlice *psli;
    // if ( (psli = ArrayCheckslicesInterval(parr, from, cnt) ) != NULL )
    // return userraise(NULL, ERR_OUT_OF_RANGE, "Slice exists... %d - %d", psli->initpos, psli->len);

    // 2. Cleanup: Free the content of the elements being removed to prevent leaks for V64.
    freeV64elems(parr, from, cnt);

    // 3. Shift: Move the suffix (elements after the removed block) to the deletion point.
    // Number of elements to move = Total length - end of deleted block
    long elements_to_move = parr->len - (from + cnt);
    if (elements_to_move > 0) {
        moveelem(parr, from, from + cnt, elements_to_move);
    }

    // 4. Update length
    parr->len -= cnt;

    return parr;
}

/**
 * @brief Inserts @p cnt new elements into the array at the specified position.
 *
 * @details This function expands the array to accommodate the new elements and 
 * shifts existing elements to make room for them. The process follows these steps:
 * <ol>
 *   <li>Validates the insertion index and the count of elements to be added.</li>
 *   <li>Increases the array capacity (sz) to ensure enough space for new elements.</li>
 *   <li>Shifts the existing elements from the insertion point to the end of the 
 *       array to the right, creating a "gap".</li>
 *   <li>Fills the newly created gap with the specified @p ftyp pattern.</li>
 *   <li>Updates the array's logical length (@p len).</li>
 * </ol>
 *
 * @param parr  Pointer to the array where elements will be inserted.
 * @param from  The starting index for the new elements.
 * @param cnt   The number of new elements to be added.
 * @param ftyp  The fill pattern to use for the newly inserted elements.
 *
 * @return      Pointer to the modified array on success.
 * @return      Returns an error code/NULL if the operation fails (e.g., invalid 
 *              parameters or memory allocation failure).
 *
 * @note This operation has a time complexity of O(n), where n is the number 
 *       of elements shifted to the right.
 * @warning If an error occurs during the capacity expansion (reallocation), 
 *          the array remains unchanged and in its original valid state.
 */
Array                   *arrayAdd(Array *parr, size_t from, size_t cnt, ArrayFillType ftyp) {
    // TODO:
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, 
        "Null array pointer %p", parr);

    if (from > parr->len)
        return logsimpleret(parr, "Nothing to add: from %zu, cnt %zu, len %zu", from, cnt, parr->len);

    if (increase(parr, parr->len + cnt) < 0)
        return userraise(NULL, ERR_UNABLE_ALLOCATE, "Unable to fill range, from %zu, cnt %zu, len %zu", from, cnt, parr->len);
    
    long     elements_to_move = parr->len - from;

    moveelem(parr, from + cnt, from, elements_to_move);
    parr->len += cnt;   // cnt > 0!!!

    arrayFillRange(parr, ftyp, from, from + cnt);
        
    return parr;
}

/**
 * @brief Checks whether two arrays are *not* equal.
 *
 * Arrays are equal if they have the same type, the same length, and all
 * elements compare equal.  The comparison is type‑aware:
 * - Numeric types (INT, LONG, DOUBLE, CHAR) compare directly.
 * - POINTER arrays compare the pointers themselves.
 * - V64 arrays delegate to value64_equals().
 *
 * @param arr1 pointer to the first array
 * @param arr2 pointer to the second array
 * @return true if the arrays differ in type, length, or any element,
 *         false if they are equal
 *
 * @throws ERR_NULLABLE_PTR if any of the pointers is NULL
 * @throws ERR_UNSUPPORTED_TYPE if the type is not handled
 */
bool                            arrayNoteq(const Array *restrict parr1, const Array *restrict parr2) {
    invraisecode(ERR_NULLABLE_PTR, parr1 != NULL && parr2 != NULL,
                 "Null pointers %p %p", (void*) parr1, (void*) parr2);
    // raise exception if not equal
    ArrayType typ = arrayCheckComparable(parr1, parr2);

    if (parr1->len != parr2->len)
        return true;   // different lengths -> not equal

    const ArrayInterface *ti = getTypedInterface(typ);
    bool    res = true;

    if (ti && ti->basic_comparator) {
        res = !ti->basic_comparator(parr1, parr2);
    } else
        userraiseint(ERR_UNSUPPORTED_TYPE, "Unsupported type for comparison: %d/%s",
                        arrayGettype(parr1), arrayGetTypeName(parr1));
    return res;
}

/**
 * @brief Sorts an array in ascending or descending order.
 *
 * Uses quicksort (qsort) with type‑specific comparators.
 * For ARRAY_V64 the comparator is obtained via value64_getPComparator()
 * / value64_getPRevComparator() according to the stored v64type.
 *
 * @param arr array (by value)
 * @param ord sort order: ARRAY_FILLTYPE_ASC or ARRAY_FILLTYPE_DESC
 *
 * @throws ERR_UNSUPPORTED_TYPE if the array type cannot be sorted
 */
void                                arrayQsort(Array *parr, ArraySortType ord) {
    invraisecode(ERR_NULLABLE_PTR, parr != NULL,
                 "Null pointers %p", parr);
    size_t                 sz = arrayGetelemsize(parr);
    if (sz <= 0)
        userraiseint(ERR_UNSUPPORTED_TYPE, 
            "Unable to get type size %d/%s", arrayGettype(parr), arrayGetTypeName(parr));
    pointer_comparator  cmp = NULL;
   
    const ArrayInterface *ti = getTypedInterface(arrayGettype(parr));
    if (ti && ti->get_comparator)
        cmp = ti->get_comparator(parr, ord);

    if (cmp)
        qsort(parr->v, parr->len, sz, cmp);
    else
        userraiseint(ERR_UNSUPPORTED_TYPE, 
                "Unsupported type %d/%s", arrayGettype(parr), arrayGetTypeName(parr));
    
}
/**
 * @brief Performs a generic binary search on an array using its internal interface.
 *
 * This function provides a polymorphic way to search for a value in any supported 
 * array type. It uses the array's specific comparator (retrieved from the 
 * interface) to perform the search.
 *
 * @param[in] parr The array to search. Must not be NULL and must be sorted 
 *                 according to the direction specified by @p acs.
 * @param[in] val  The value to search for, wrapped in a @ref value64 union. 
 *                 @important The @p val must be initialized with the member 
 *                 corresponding to the array's type (e.g., if the array is 
 *                 ARRAY_INT, @p val.ival must be set).
 * @param[in] acs  The sorting direction. @c true for ascending, @c false 
 *                 for descending.
 *
 * @return The zero-based index of the found element; @c -1 if the element 
 *         is not found or if the array is empty.
 *
 * @note Complexity: O(log n).
 * @warning If the array is not sorted in the direction specified by @p acs, 
 *          the result is undefined.
 */
long                        arrayBsearchCommon(const Array *parr, value64 val, bool acs) {
    invraisecode(ERR_NULLABLE_PTR, parr != NULL,
                 "Null pointers %p", parr);

    if (parr->len == 0) // this is discussable
        return -1;
    pointer_comparator    cmp = NULL;
    const ArrayInterface *ti = getTypedInterface(arrayGettype(parr));
    size_t                   elemsize = arrayGetelemsize(parr);

    if (elemsize > 0 && ti && ti->get_comparator)
        cmp = ti->get_comparator(parr, acs ? ARRAY_SORTTYPE_ASC: ARRAY_SORTTYPE_DESC);

    if (!cmp) {
        return userraiseint(ERR_UNSUPPORTED_TYPE, "No comparator for type (size %zu) %d/%s, v64(%d/%s)", elemsize,
                     arrayGettype(parr), arrayGetTypeName(parr), 
                     parr->v64type, value64_typename(parr->v64type));
    }
    // parr->v as generic for parr
    const char *found = bsearch(&val.u64, parr->v, parr->len, elemsize, cmp);

    return found != NULL? (found - (const char *) parr->v) / elemsize: -1;
}

// -----------------------------------------------------------------------------------------------------
// if condition is 0-ptr == ALL
long                        arrayForeach(Array *restrict arr, ArrayCond cond, ArrayProc func){
    long    cnt = 0;
    Array_pforeach_idx(arr, i) {
        if (cond == NULL || cond(arr, i) ){
            if (func)
                func(arr, i);
            cnt++;
        }
    }
    return logsimpleret(cnt, "processed %ld", cnt);
}

/**
 * @brief Fills the entire Array using a generator.
 * 
 * @note **Safety Features:**
 * - If @p from or @p to are outside the actual bounds of the array, they are 
 *   automatically clamped to the array's size to prevent memory corruption.
 * - The operation stops if the generator runs out of elements before the 
 *   specified range is completed.
 *
 * @param[in] parr Pointer to the destination Array (must be of type ARRAY_V64).
 * @param[in] gen  Pointer to the v64 generator.
 * @param[in] from The starting index for the fill operation (inclusive).
 * @param[in] to   The ending index for the fill operation (exclusive).
 * 
 * @return The number of elements successfully written to the array.
 */
long                        arrayGenPumprange(Array *restrict parr, v64Gen *restrict gen, size_t from, size_t to) {
    invraisecode(parr != NULL && gen != NULL, ERR_NULLABLE_PTR, 
        "Null input %p %p", parr, gen);

    fixrangesbysz(parr, &from, &to);

    ArrayType typ = arrayGettype(parr);
    const ArrayInterface *ti = getTypedInterface(typ);

    if (!ti || !ti->pump)
        return userraise(-1, ERR_UNSUPPORTED_TYPE, 
            "No pump function for type %d/%s", arrayGettype(parr), arrayGetTypeName(parr));
    
    // Если это массив V64, проверяем соответствие типа в генераторе
    if (arrayIsV64(parr))
        if (arrayGetV64mapType(parr) != gen->type) {
            return userraise(-1, ERR_TYPES_MISMATCH, "%d/%s vs %d/%s", 
                             typ, arrayGetTypeName(parr), 
                             gen->type, value64_typename(gen->type));
        }

    long cnt = ti->pump(parr, gen, from, to);

    return logsimpleret(cnt, "Generated %ld", cnt);
}

// -------------------------------Testing --------------------------

#ifdef ARRAYTESTING

#include <float.h>
#include <math.h>
#include "test.h"
#include "checker.h"

// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: double", ++subnum);
    {
        Array *arr = DarrayCreate(100, ARRAY_FILLTYPE_ZERO);
        for (size_t i= 0; i < arr->len; i++)
            test_validatefree(
                arr->dv[i] == 0.0,
                ARRAYFREE(arr),
                "%zu: Element must be 0.0, but not %lf", i, arr->dv[i]
            );
        test_validatefree(
            arrayIsvalid(arr),
            ARRAYFREE(arr),
            "Validation is failed"
        );
        ARRAYFREE(arr);
        test_validate(arr == NULL, "Array isn't freed (pointer must be NULL)");
    }

    test_sub("subtest %d: int", ++subnum);
    {
        Array *arr = IarrayCreate(100, ARRAY_FILLTYPE_ZERO);
        for (size_t i= 0; i < arr->len; i++)
            test_validatefree(
                arr->iv[i] == 0,
                ARRAYFREE(arr),
                "%zu: Element must be 0 but not %d", i, arr->iv[i]
            );
        test_validatefree(
            arrayIsvalid(arr),
            ARRAYFREE(arr),
            "Validation is failed"
        );
        ARRAYFREE(arr);
        test_validate(arr == NULL, "Array isn't freed (pointer must be NULL)");
    }

    test_sub("subtest %d: long", ++subnum);
    {
        Array *arr = LarrayCreate(100, ARRAY_FILLTYPE_ZERO);
        for (size_t i= 0; i < arr->len; i++)
            test_validatefree(
                arr->lv[i] == 0L,
                ARRAYFREE(arr),
                "%zu: Element must be 0L but not %ld", i, arr->lv[i]
            );
        test_validatefree(
            arrayIsvalid(arr),
            ARRAYFREE(arr),
            "Validation is failed"
        );
        ARRAYFREE(arr);
        test_validate(arr == NULL, "Array isn't freed (pointer must be NULL)");
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 2 ---------------------------------
static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: double asc/desc", ++subnum);
    {
        Array *arr = DarrayCreate(100, ARRAY_FILLTYPE_ASC);
        // ASC check
        for (size_t i = 0; i < arr->len - 1; i++)
            test_validatefree(
                arr->dv[i] <= arr->dv[i + 1],
                ARRAYFREE(arr),
                "ASC violation: arr[%zu]=%f > arr[%zu]=%f",
                i, arr->dv[i], i + 1, arr->dv[i + 1]
            );
        // refill to DESC
        arrayFillAll(arr, ARRAY_FILLTYPE_DESC);
        // DESC check
        for (size_t i = 0; i < arr->len - 1; i++)
            test_validatefree(
                arr->dv[i] >= arr->dv[i + 1],
                ARRAYFREE(arr),
                "DESC violation: arr[%zu]=%f < arr[%zu]=%f",
                i, arr->dv[i], i + 1, arr->dv[i + 1]
            );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: int asc/desc", ++subnum);
    {
        Array *arr = IarrayCreate(100, ARRAY_FILLTYPE_ASC);
        for (size_t i = 0; i < arr->len - 1; i++)
            test_validatefree(
                arr->iv[i] <= arr->iv[i + 1],
                ARRAYFREE(arr),
                "ASC violation: arr[%zu]=%d > arr[%zu]=%d",
                i, arr->iv[i], i + 1, arr->iv[i + 1]
            );
        arrayFillAll(arr, ARRAY_FILLTYPE_DESC);
        for (size_t i = 0; i < arr->len - 1; i++)
            test_validatefree(
                arr->iv[i] >= arr->iv[i + 1],
                ARRAYFREE(arr),
                "DESC violation: arr[%zu]=%d < arr[%zu]=%d",
                i, arr->iv[i], i + 1, arr->iv[i + 1]
            );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: long asc/desc", ++subnum);
    {
        Array *arr = LarrayCreate(100, ARRAY_FILLTYPE_ASC);
        for (size_t i= 0; i < arr->len - 1; i++)
            test_validatefree(
                arr->lv[i] <= arr->lv[i + 1],
                ARRAYFREE(arr),
                "ASC violation: arr[%zu]=%ld > arr[%zu]=%ld",
                i, arr->lv[i], i + 1, arr->lv[i + 1]
            );
        arrayFillAll(arr, ARRAY_FILLTYPE_DESC);
        for (size_t i= 0; i < arr->len - 1; i++)
            test_validatefree(
                arr->lv[i] >= arr->lv[i + 1],
                ARRAYFREE(arr),
                "DESC violation: arr[%zu]=%ld < arr[%zu]=%ld",
                i, arr->lv[i], i + 1, arr->lv[i + 1]
            );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}
/**
 * @brief Test suite for Array Shrink and Validation.
 * 
 * @details This test ensures that shrinking an array works correctly, 
 *          the data remains valid, and the internal properties (len, sz) 
 *          are updated properly.
 * 
 * @return TestStatus TEST_PASSED if all subtests pass, otherwise TEST_FAILED.
 */
// ------------------------- TEST 3 ---------------------------------
static TestStatus
tf3(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: double", ++subnum);
    {
        Array *arr = DarrayCreate(100, ARRAY_FILLTYPE_ASC);
        arrayfprint(logfile, arr, 0);

        arr = arrayShrink(arr, 10);
        test_validatefree(
            arrayIsvalid(arr) && arr->len == 10 && arr->sz >= 10 && arr->dv != NULL,
            ARRAYFREE(arr),
            "Shrink failed: len=%zu, sz=%zu, v=%p", arr->len, arr->sz, (void*)arr->dv
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: int", ++subnum);
    {
        Array *arr = IarrayCreate(100, ARRAY_FILLTYPE_ASC);
        arrayfprint(logfile, arr, 0);

        arr = arrayShrink(arr, 10);
        test_validatefree(
            arrayIsvalid(arr) && arr->len == 10 && arr->sz >= 10 && arr->iv != NULL,
            ARRAYFREE(arr),
            "Shrink failed: len=%zu, sz=%zu, v=%p", arr->len, arr->sz, (void*)arr->iv
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: long", ++subnum);
    {
        Array *arr = LarrayCreate(100, ARRAY_FILLTYPE_ASC);
        arrayfprint(logfile, arr, 0);

        arr = arrayShrink(arr, 10);
        test_validatefree(
            arrayIsvalid(arr) && arr->len == 10 && arr->sz >= 10 && arr->lv != NULL,
            ARRAYFREE(arr),
            "Shrink failed: len=%zu, sz=%zu, v=%p", arr->len, arr->sz, (void*)arr->lv
        );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 4 ---------------------------------
static TestStatus
tf4(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: int save/load", ++subnum);
    {
        Array *arr = IarrayCreate(100, ARRAY_FILLTYPE_RND);

        test_validate(
            arr != NULL, 
            "Array not created"
        );

        const char *filename = "res/array/iarr.sv";

        long written = arraySaveFileByName(arr, filename);
        test_validatefree(
            written > 0, 
            ARRAYFREE(arr), 
            "Int save failed"
        );

        Array *loaded = arrayLoadFileByName(filename);
        test_validatefree(
            loaded != NULL && arrayIsvalid(loaded) &&
            arr->len == loaded->len && arr->flags == loaded->flags,
            (ARRAYFREE(arr), ARRAYFREE(loaded)),
            "Length or flags mismatch: len %zu vs %zu, flags %d vs %d",
            arr->len, loaded->len, arr->flags, loaded->flags
        );

        // поэлементное сравнение
        for (size_t i= 0; i < arr->len; i++)
            test_validatefree(
                arr->iv[i] == loaded->iv[i],
                (ARRAYFREE(arr), ARRAYFREE(loaded)),
                "arr[%zu] = %d != arr2[%zu] = %d",
                i, arr->iv[i], i, loaded->iv[i]
            );

        ARRAYFREE(arr);
        ARRAYFREE(loaded);
    }

    test_sub("subtest %d: long save/load", ++subnum);
    {
        Array *arr = LarrayCreate(100, ARRAY_FILLTYPE_RND);
        const char *filename = "res/array/larr.sv";

        long written = arraySaveFileByName(arr, filename);
        test_validatefree(written > 0, ARRAYFREE(arr), "Long save failed");

        Array *loaded = arrayLoadFileByName(filename);
        test_validatefree(
            loaded != NULL && arrayIsvalid(loaded) &&
            arr->len == loaded->len && arr->flags == loaded->flags,
            (ARRAYFREE(arr), ARRAYFREE(loaded)),
            "Length or flags mismatch: len %zu vs %zu, flags %d vs %d",
            arr->len, loaded->len, arr->flags, loaded->flags
        );

        for (size_t i= 0; i < arr->len; i++)
            test_validatefree(
                arr->lv[i] == loaded->lv[i],
                (ARRAYFREE(arr), ARRAYFREE(loaded)),
                "arr[%zu] = %ld != arr2[%zu] = %ld",
                i, arr->lv[i], i, loaded->lv[i]
            );

        ARRAYFREE(arr);
        ARRAYFREE(loaded);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 5 ---------------------------------
static TestStatus
tf5(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d", ++subnum);
    {
        Array *arr = DarrayCreate(100, ARRAY_FILLTYPE_RND);
        const char *filename = "res/array/darr.sv";

        arrayfprint(logfile, arr, 0);
        long written = arraySaveFileByName(arr, filename);
        test_validatefree(written > 0, ARRAYFREE(arr), "Double save failed");

        Array *loaded = arrayLoadFileByName(filename);
        test_validatefree(
            loaded != NULL && arrayIsvalid(loaded) &&
            arr->len == loaded->len && arr->flags == loaded->flags,
            (ARRAYFREE(arr), ARRAYFREE(loaded)),
            "Length or flags mismatch: len %zu vs %zu, flags %d vs %d",
            arr->len, loaded->len, arr->flags, loaded->flags
        );

        for (size_t i= 0; i < arr->len; i++)
            test_validatefree(
                fabs(arr->dv[i] - loaded->dv[i]) <= FLT_EPSILON / 100,
                (ARRAYFREE(arr), ARRAYFREE(loaded)),
                "arr[%zu] = %15.15lf != arr2[%zu] = %15.15lf",
                i, arr->dv[i], i, loaded->dv[i]
            );

        ARRAYFREE(arr);
        ARRAYFREE(loaded);
    }
    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 6 ---------------------------------
static TestStatus
tf6(const char *name)
{
    logenter("%s", name);
    int subnum = 0;
    // g_custom_print_line = NULL;   // сбрасываем формат печати

    /* ---------- double ---------- */
    test_sub("subtest %d: double", ++subnum);
    {
        Array *arr = DarrayCreate(50, ARRAY_FILLTYPE_ASC);
        arrayShuffle(arr);

        // проверяем, что порядок нарушен (не все элементы строго возрастают)
        bool ordered = true;
        for (size_t i= 0; i < arr->len - 1; i++) {
            if (arr->dv[i] > arr->dv[i + 1]) {
                ordered = false;
                break;
            }
        }
        test_validatefree(
            !ordered,
            ARRAYFREE(arr),
            "Double shuffle: array must not be perfectly ordered after shuffle"
        );
        ARRAYFREE(arr);
    }

    /* ---------- int ---------- */
    test_sub("subtest %d: int", ++subnum);
    {
        Array *arr = IarrayCreate(50, ARRAY_FILLTYPE_ASC);
        arrayShuffle(arr);

        bool ordered = true;
        for (size_t i= 0; i < arr->len - 1; i++) {
            if (arr->iv[i] > arr->iv[i + 1]) {
                ordered = false;
                break;
            }
        }
        test_validatefree(
            !ordered,
            ARRAYFREE(arr),
            "Int shuffle: array must not be perfectly ordered"
        );
        ARRAYFREE(arr);
    }

    /* ---------- long ---------- */
    test_sub("subtest %d: long", ++subnum);
    {
        Array *arr = LarrayCreate(50, ARRAY_FILLTYPE_ASC);
        arrayShuffle(arr);

        bool ordered = true;
        for (size_t i= 0; i < arr->len - 1; i++) {
            if (arr->lv[i] > arr->lv[i + 1]) {
                ordered = false;
                break;
            }
        }
        test_validatefree(
            !ordered,
            ARRAYFREE(arr),
            "Long shuffle: array must not be perfectly ordered"
        );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 7 ---------------------------------
static TestStatus
tf7(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- double ---------- */
    test_sub("subtest %d: double asc/desc", ++subnum);
    {
        Array *arr = DarrayCreate(10000, ARRAY_FILLTYPE_RND);
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        for (size_t i= 1; i < arr->len; i++)
            test_validatefree(
                arr->dv[i - 1] <= arr->dv[i],
                ARRAYFREE(arr),
                "ASC violation: arr[%zu]=%f > arr[%zu]=%f",
                i - 1, arr->dv[i - 1], i, arr->dv[i]
            );

        arrayQsort(arr, ARRAY_SORTTYPE_DESC);
        for (size_t i= 1; i < arr->len; i++)
            test_validatefree(
                arr->dv[i - 1] >= arr->dv[i],
                ARRAYFREE(arr),
                "DESC violation: arr[%zu]=%f < arr[%zu]=%f",
                i - 1, arr->dv[i - 1], i, arr->dv[i]
            );
        ARRAYFREE(arr);
    }

    /* ---------- int ---------- */
    test_sub("subtest %d: int asc/desc", ++subnum);
    {
        Array *arr = IarrayCreate(100000, ARRAY_FILLTYPE_RND);
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        for (size_t i= 1; i < arr->len; i++)
            test_validatefree(
                arr->iv[i - 1] <= arr->iv[i],
                ARRAYFREE(arr),
                "ASC violation: arr[%zu]=%d > arr[%zu]=%d",
                i - 1, arr->iv[i - 1], i, arr->iv[i]
            );

        arrayQsort(arr, ARRAY_SORTTYPE_DESC);
        for (size_t i= 1; i < arr->len; i++)
            test_validatefree(
                arr->iv[i - 1] >= arr->iv[i],
                ARRAYFREE(arr),
                "DESC violation: arr[%zu]=%d < arr[%zu]=%d",
                i - 1, arr->iv[i - 1], i, arr->iv[i]
            );
        ARRAYFREE(arr);
    }

    /* ---------- long ---------- */
    test_sub("subtest %d: long asc/desc", ++subnum);
    {
        Array *arr = LarrayCreate(100000, ARRAY_FILLTYPE_RND);
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        for (size_t i= 1; i < arr->len; i++)
            test_validatefree(
                arr->lv[i - 1] <= arr->lv[i],
                ARRAYFREE(arr),
                "ASC violation: arr[%zu]=%ld > arr[%zu]=%ld",
                i - 1, arr->lv[i - 1], i, arr->lv[i]
            );

        arrayQsort(arr, ARRAY_SORTTYPE_DESC);
        for (size_t i= 1; i < arr->len; i++)
            test_validatefree(
                arr->lv[i - 1] >= arr->lv[i],
                ARRAYFREE(arr),
                "DESC violation: arr[%zu]=%ld < arr[%zu]=%ld",
                i - 1, arr->lv[i - 1], i, arr->lv[i]
            );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 8 ---------------------------------
static TestStatus
tf8(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: increase int array", ++subnum);
    {
        size_t initsz = 25;
        Array *arr = IarrayCreate(initsz, ARRAY_FILLTYPE_RND);

        arr = arrayIncrease(arr, initsz * 3);

        test_validatefree(
            arr->len == initsz * 3,
            ARRAYFREE(arr),
            "Array length %zu must be %zu", arr->len, initsz * 3
        );
        for (size_t i= initsz; i < arr->len; i++)
            test_validatefree(
                arr->iv[i] == 0,
                ARRAYFREE(arr),
                "arr[%zu] must be zero, but not %d", i, arr->iv[i]
            );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: increase double array", ++subnum);
    {
        size_t   initsz = 25;
        Array   *arr = DarrayCreate(initsz, ARRAY_FILLTYPE_RND);

        arr = arrayIncrease(arr, initsz * 3);

        test_validatefree(
            arr->len == initsz * 3,
            ARRAYFREE(arr),
            "Array length %zu must be %zu", arr->len, initsz * 3
        );
        for (size_t i= initsz; i < arr->len; i++)
            test_validatefree(
                arr->dv[i] == 0.0,
                ARRAYFREE(arr),
                "arr[%zu] must be zero, but not %lf", i, arr->dv[i]
            );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: increase long array", ++subnum);
    {
        size_t  initsz = 25;
        Array  *arr = LarrayCreate(initsz, ARRAY_FILLTYPE_RND);

        arr = arrayIncrease(arr, initsz * 5);

        test_validatefree(
            arr->len == initsz * 5,
            ARRAYFREE(arr),
            "Array length %zu must be %zu", arr->len, initsz * 5
        );
        for (size_t i= initsz; i < arr->len; i++)
            test_validatefree(
                arr->lv[i] == 0L,
                ARRAYFREE(arr),
                "arr[%zu] must be zero, but not %ld", i, arr->lv[i]
            );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 9 ---------------------------------
static TestStatus
tf9(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: creating pointer array", ++subnum);
    {
        Array *parr = ParrayCreate(100, ARRAY_FILLTYPE_ZERO);
        for (size_t i= 0; i < parr->len; i++)
            test_validatefree(
                parr->pv[i] == NULL,
                ARRAYFREE(parr),
                "Element %zu must be NULL, but not %p", i, (void*)parr->pv[i]
            );
        test_validatefree(
            arrayIsvalid(parr),
            ARRAYFREE(parr),
            "Validation is failed"
        );
        ARRAYFREE(parr);
    }

    test_sub("subtest %d: shrinking", ++subnum);
    {
        Array  *parr = ParrayCreate(100, ARRAY_FILLTYPE_ZERO);
        size_t  cnt = 10;
        parr = arrayShrink(parr, cnt);
        test_validatefree(
            arrayIsvalid(parr),
            ARRAYFREE(parr),
            "Validation is failed"
        );
        test_validatefree(
            parr->len == cnt && parr->sz >= cnt && parr->pv != NULL,
            ARRAYFREE(parr),
            "Shrink failed: len=%zu, sz=%zu, v=%p", parr->len, parr->sz, (void*)parr->pv
        );
        ARRAYFREE(parr);
    }

    test_sub("subtest %d: pointer array save/load", ++subnum);
    {
        const char *filename = "res/array/parr.sv";
        Array *parr = ParrayCreate(100, ARRAY_FILLTYPE_ZERO);
        arraySaveFileByName(parr, filename);

        Array *loaded = arrayLoadFileByName(filename);
        test_validatefree(
            arrayIsvalid(loaded),
            (ARRAYFREE(parr), ARRAYFREE(loaded)),
            "Loaded array validation failed"
        );
        test_validatefree(
            parr->len == loaded->len && parr->flags == loaded->flags,
            (ARRAYFREE(parr), ARRAYFREE(loaded)),
            "Length or flags mismatch: len %zu vs %zu, flags %d vs %d",
            parr->len, loaded->len, parr->flags, loaded->flags
        );

        for (size_t i = 0; i < parr->len; i++)
            test_validatefree(
                parr->pv[i] == loaded->pv[i],
                (ARRAYFREE(parr), ARRAYFREE(loaded)),
                "arr[%zu] = %p != arr2[%zu] = %p",
                i, (void*)parr->pv[i], i, (void*)loaded->pv[i]
            );

        ARRAYFREE(parr);
        ARRAYFREE(loaded);
    }

    test_sub("subtest %d: pointer array sorting", ++subnum);
    {
        int cnt = 10000;
        Array *parr = ParrayCreate(cnt, ARRAY_FILLTYPE_ZERO);

        // fill array with descending addresses
        for (size_t i = 0; i < parr->len; i++)
            parr->pv[i] = parr->pv + cnt - 1 - i;

        arrayQsort(parr, ARRAY_SORTTYPE_ASC);
        for (size_t i = 1; i < parr->len; i++)
            test_validatefree(
                (uintptr_t)parr->pv[i - 1] <= (uintptr_t)parr->pv[i],
                ARRAYFREE(parr),
                "ASC violation: arr[%zu]=%p > arr[%zu]=%p",
                i - 1, (void*)parr->pv[i - 1], i, (void*)parr->pv[i]
            );

        arrayQsort(parr, ARRAY_SORTTYPE_DESC);
        for (size_t i = 1; i < parr->len; i++)
            test_validatefree(
                (uintptr_t)parr->pv[i - 1] >= (uintptr_t)parr->pv[i],
                ARRAYFREE(parr),
                "DESC violation: arr[%zu]=%p < arr[%zu]=%p",
                i - 1, (void*)parr->pv[i - 1], i, (void*)parr->pv[i]
            );
        ARRAYFREE(parr);
    }

    test_sub("subtest %d: increase pointer array", ++subnum);
    {
        size_t initsz = 25;
        Array *arr = ParrayCreate(initsz, ARRAY_FILLTYPE_ZERO);
        arr = arrayIncrease(arr, initsz * 3);

        test_validatefree(
            arr->len == initsz * 3,
            ARRAYFREE(arr),
            "Array length %zu must be %zu", arr->len, initsz * 3
        );
        for (size_t i = initsz; i < arr->len; i++)
            test_validatefree(
                arr->pv[i] == NULL,
                ARRAYFREE(arr),
                "arr[%zu] must be NULL, but not %p", i, (void*)arr->pv[i]
            );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 10 ---------------------------------
static TestStatus
tf10(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Int ascending series */
    test_sub("subtest %d: int asc series", ++subnum);
    {
        size_t   cnt = 100;
        Array   *arr = IarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);

        size_t   len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        for (size_t i = 0; i < cnt; i++) {
            test_validatefree(
                arr->iv[i] == (int) i,
                ARRAYFREE(arr),
                "Int asc series: arr[%zu] = %d, expected %zu", i, arr->iv[i], i
            );
        }
    }

    /* 2. Int descending series */
    test_sub("subtest %d: int desc series", ++subnum);
    {
        size_t     cnt = 50;
        Array   *arr = IarrayCreate(cnt, ARRAY_FILLTYPE_DESC_SERIES);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        for (size_t i = 0; i < cnt; i++) {
            size_t expected = cnt - 1 - i;
            test_validatefree(
                arr->iv[i] == (int) expected,
                ARRAYFREE(arr),
                "Int desc series: arr[%zu] = %d, expected %zu", i, arr->iv[i], expected
            );
        }
    }

    /* 3. Long ascending series */
    test_sub("subtest %d: long asc series", ++subnum);
    {
        size_t     cnt = 70;
        Array     *arr = LarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        for (size_t i = 0; i < cnt; i++) {
            test_validatefree(
                arr->lv[i] == (long)i,
                ARRAYFREE(arr),
                "Long asc series: arr[%zu] = %ld, expected %ld", i, arr->lv[i], (long)i
            );
        }
    }

    /* 4. Long descending series */
    test_sub("subtest %d: long desc series", ++subnum);
    {
        size_t     cnt = 40;
        Array   *arr = LarrayCreate(cnt, ARRAY_FILLTYPE_DESC_SERIES);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        for (size_t i = 0; i < cnt; i++) {
            long expected = (long)(cnt - 1 - i);
            test_validatefree(
                arr->lv[i] == expected,
                ARRAYFREE(arr),
                "Long desc series: arr[%zu] = %ld, expected %ld", i, arr->lv[i], expected
            );
        }
    }

    /* 5. Double ascending series */
    test_sub("subtest %d: double asc series", ++subnum);
    {
        size_t     cnt = 30;
        Array   *arr = DarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        for (size_t i = 0; i < cnt; i++) {
            test_validatefree(
                arr->dv[i] == (double)i,
                ARRAYFREE(arr),
                "Double asc series: arr[%zu] = %f, expected %f", i, arr->dv[i], (double)i
            );
        }
    }

    /* 6. Double descending series */
    test_sub("subtest %d: double desc series", ++subnum);
    {
        size_t     cnt = 25;
        Array   *arr = DarrayCreate(cnt, ARRAY_FILLTYPE_DESC_SERIES);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        for (size_t i = 0; i < cnt; i++) {
            double expected = (double)(cnt - 1 - i);
            test_validatefree(
                arr->dv[i] == expected,
                ARRAYFREE(arr),
                "Double desc series: arr[%zu] = %f, expected %f", i, arr->dv[i], expected
            );
        }
    }

    /* 7. Empty array */
    test_sub("subtest %d: empty series", ++subnum);
    {
        Array      *arr = IarrayCreate(0, ARRAY_FILLTYPE_ASC_SERIES);
        size_t     len = arraylen(arr);
        test_validatefree(
            len == 0,
            ARRAYFREE(arr),
            "Empty array length = %zu, expected 0", len
        );
    }

    /* 8. Unsupported type (pointers) – must raise error */
    test_sub("subtest %d: pointer series (unsupported)", ++subnum);
    {
        if (!try()) {
            Array *arr = ParrayCreate(10, ARRAY_FILLTYPE_ASC_SERIES);
            // We should not reach here
            test_validate(
                false,
                "Pointer series should have raised an error but didn't"
            );
            // to avoid unused variable warning, but it will never be used
            arrayprint(arr, 0);
        } else {
            // Signal caught – expected behavior
            test_validate(
                true,
                "Pointer series correctly raised error"
            );
        }
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 11 ---------------------------------
static TestStatus
tf11(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Fill middle of int array with ascending series */
    test_sub("subtest %d: fill middle with asc series", ++subnum);
    {
        size_t     cnt = 50;
        Array     *arr = IarrayCreate(cnt, ARRAY_FILLTYPE_ZERO);
        size_t     from = 10, to = 20;

        arrayFillRange(arr, ARRAY_FILLTYPE_ASC_SERIES, from, to);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        // Elements before 'from' and after 'to' must remain zero
        for (size_t i = 0; i < cnt; i++) {
            if (i >= from && i < to)
                continue;
            test_validatefree(
                arr->iv[i] == 0,
                ARRAYFREE(arr),
                "Element [%zu] = %d, expected 0 (outside range)", i, arr->iv[i]
            );
        }

        // Inside the range, values must equal the index
        for (size_t i = from; i < to; i++) {
            test_validatefree(
                arr->iv[i] == (int) i,
                ARRAYFREE(arr),
                "Element [%zu] = %d, expected %zu (inside range)", i, arr->iv[i], i
            );
        }
    }

    /* 2. Full fill of long array with descending series */
    test_sub("subtest %d: full fill with desc series (long)", ++subnum);
    {
        size_t     cnt = 30;
        Array     *arr = LarrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
        arrayFillRange(arr, ARRAY_FILLTYPE_DESC_SERIES, 0, cnt);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        for (size_t i = 0; i < cnt; i++) {
            long expected = (long)(cnt - 1 - i);
            test_validatefree(
                arr->lv[i] == expected,
                ARRAYFREE(arr),
                "Element [%zu] = %ld, expected %ld", i, arr->lv[i], expected
            );
        }
    }

    /* 3. Empty range (from == to) – array remains unchanged */
    test_sub("subtest %d: from == to leaves array unchanged", ++subnum);
    {
        size_t     cnt = 20;
        Array     *arr = IarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);  // [0..19]
        arrayFillRange(arr, ARRAY_FILLTYPE_RND, 5, 5);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        for (size_t i = 0; i < cnt; i++) {
            test_validatefree(
                arr->iv[i] == (int) i,
                ARRAYFREE(arr),
                "Element [%zu] = %d, expected %zu (unchanged after empty fill)", i, arr->iv[i], i
            );
        }
    }

    /* 4. Out-of-bounds – program must not crash */
    test_sub("subtest %d: out-of-bounds does not crash", ++subnum);
    {
        size_t     cnt = 10;
        Array     *arr = IarrayCreate(cnt, ARRAY_FILLTYPE_ZERO);
        arrayFillRange(arr, ARRAY_FILLTYPE_ASC_SERIES, -5, cnt + 5);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "After out-of-bounds fill, length = %zu, expected %zu", len, cnt
        );
        // Content is not checked, as behavior is undefined
    }

    /* 5. Double array, ascending series fill in a sub-range */
    test_sub("subtest %d: double asc series fill range", ++subnum);
    {
        size_t     cnt = 25, from = 5, to = 15;
        Array     *arr = DarrayCreate(cnt, ARRAY_FILLTYPE_ZERO);
        arrayFillRange(arr, ARRAY_FILLTYPE_ASC_SERIES, from, to);

        size_t     len = arraylen(arr);
        test_validatefree(
            len == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", len, cnt
        );

        // Elements outside the range must remain zero
        for (size_t i = 0; i < cnt; i++) {
            if (i >= from && i < to) continue;
            test_validatefree(
                arr->dv[i] == 0.0,
                ARRAYFREE(arr),
                "Element [%zu] = %f, expected 0.0 (outside range)", i, arr->dv[i]
            );
        }

        // Inside the range, values must equal the index
        for (size_t i = from; i < to; i++) {
            test_validatefree(
                arr->dv[i] == (double)i,
                ARRAYFREE(arr),
                "Element [%zu] = %f, expected %f", i, arr->dv[i], (double)i
            );
        }
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 12 ---------------------------------

static TestStatus
tf12(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. int array */
    test_sub("subtest %d: int array (even half, odd zero)", ++subnum);
    {
        size_t   cnt = 10;
        Array   *arr = IarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);   // 0..9

        IArray_foreach(arr, elem) {
            if (*elem % 2 == 0)
                *elem /= 2;
            else
                *elem = 0;
        }

        test_validatefree(arraylen(arr) == cnt, ARRAYFREE(arr), "Length mismatch");

        int expected[] = {0, 0, 1, 0, 2, 0, 3, 0, 4, 0};
        for (size_t i = 0; i < cnt; i++)
            test_validatefree(arr->iv[i] == expected[i], ARRAYFREE(arr),
                "int[%zu]=%d expected %d", i, arr->iv[i], expected[i]);

        ARRAYFREE(arr);
    }

    /* 2. long array */
    test_sub("subtest %d: long array", ++subnum);
    {
        size_t   cnt = 8;
        Array   *arr = LarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);

        LArray_foreach(arr, elem) {
            if (*elem % 2 == 0)
                *elem /= 2;
            else
                *elem = 0;
        }

        test_validatefree(arraylen(arr) == cnt, ARRAYFREE(arr), "Length mismatch");

        long expected[] = {0L, 0L, 1L, 0L, 2L, 0L, 3L, 0L};
        for (size_t i = 0; i < cnt; i++)
            test_validatefree(arr->lv[i] == expected[i], ARRAYFREE(arr),
                "long[%zu]=%ld expected %ld", i, arr->lv[i], expected[i]);

        ARRAYFREE(arr);
    }

    /* 3. double array */
    test_sub("subtest %d: double array", ++subnum);
    {
        size_t   cnt = 6;
        Array   *arr = DarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);

        DArray_foreach(arr, elem) {
            if (fmod(*elem, 2.0) == 0.0)
                *elem /= 2.0;
            else
                *elem = 0.0;
        }

        test_validatefree(
            arraylen(arr) == cnt, 
            ARRAYFREE(arr), 
            "Length mismatch"
        );

        double expected[] = {0.0, 0.0, 1.0, 0.0, 2.0, 0.0};
        for (size_t i = 0; i < cnt; i++)
            test_validatefree(arr->dv[i] == expected[i], ARRAYFREE(arr),
                "double[%zu]=%f expected %f", i, arr->dv[i], expected[i]);

        ARRAYFREE(arr);
    }

    /* 4. pointer array (no‑op) */
    test_sub("subtest %d: pointer array (no‑op)", ++subnum);
    {
        size_t   cnt = 3;
        Array   *arr = ParrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->pv[0] = (void*)1; arr->pv[1] = (void*)2; arr->pv[2] = (void*)3;

        PArray_foreach(arr, elem) {
            // nothing
        }

        test_validatefree(arraylen(arr) == cnt, ARRAYFREE(arr), "Length changed");
        test_validatefree(arr->pv[0] == (void*)1, ARRAYFREE(arr), "ptr[0] mismatch");
        test_validatefree(arr->pv[1] == (void*)2, ARRAYFREE(arr), "ptr[1] mismatch");
        test_validatefree(arr->pv[2] == (void*)3, ARRAYFREE(arr), "ptr[2] mismatch");

        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 13 ---------------------------------
static bool keep_if_index_not_multiple_of_3(Array *arr, size_t pos) {
    (void)arr;
    return (pos % 3) != 0;
}

static void square_int(Array *arr, size_t pos) {
    int val = arr->iv[pos];
    arr->iv[pos] = val * val;
}
static void square_long(Array *arr, size_t pos) {
    long val = arr->lv[pos];
    arr->lv[pos] = val * val;
}
static void mul_one_point_five_double(Array *arr, size_t pos) {
    arr->dv[pos] *= 1.5;
}

static TestStatus
tf13(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. int array */
    test_sub("subtest %d: int array (square non‑multiples of 3)", ++subnum);
    {
        size_t     cnt = 10;
        Array     *arr = IarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);

        arrayForeach(arr, keep_if_index_not_multiple_of_3, square_int);

        test_validatefree(
            arraylen(arr) == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", arraylen(arr), cnt
        );

        int expected[] = {0, 1, 4, 3, 16, 25, 6, 49, 64, 9};
        for (size_t i = 0; i < cnt; i++) {
            test_validatefree(
                arr->iv[i] == expected[i],
                ARRAYFREE(arr),
                "int proc: arr[%zu] = %d, expected %d", i, arr->iv[i], expected[i]
            );
        }

        ARRAYFREE(arr);
    }

    /* 2. long array */
    test_sub("subtest %d: long array", ++subnum);
    {
        size_t   cnt = 8;
        Array   *arr = LarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);

        arrayForeach(arr, keep_if_index_not_multiple_of_3, square_long);

        test_validatefree(
            arraylen(arr) == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", arraylen(arr), cnt
        );

        long expected[] = {0L, 1L, 4L, 3L, 16L, 25L, 6L, 49L};
        for (size_t i = 0; i < cnt; i++) {
            test_validatefree(
                arr->lv[i] == expected[i],
                ARRAYFREE(arr),
                "long proc: arr[%zu] = %ld, expected %ld", i, arr->lv[i], expected[i]
            );
        }

        ARRAYFREE(arr);
    }

    /* 3. double array */
    test_sub("subtest %d: double array", ++subnum);
    {
        size_t   cnt = 6;
        Array   *arr = DarrayCreate(cnt, ARRAY_FILLTYPE_ASC_SERIES);

        arrayForeach(arr, keep_if_index_not_multiple_of_3, mul_one_point_five_double);

        test_validatefree(
            arraylen(arr) == cnt,
            ARRAYFREE(arr),
            "Array length = %zu, expected %zu", arraylen(arr), cnt
        );

        double expected[] = {0.0, 1.5, 3.0, 3.0, 6.0, 7.5};
        for (size_t i = 0; i < cnt; i++) {
            test_validatefree(
                arr->dv[i] == expected[i],
                ARRAYFREE(arr),
                "double proc: arr[%zu] = %f, expected %f", i, arr->dv[i], expected[i]
            );
        }

        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST V64Array (STR / FS) -------------------------
static TestStatus
tf_v64array_str_fs(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- VALUE64_STR ---------- */
    test_sub("subtest %d: create empty STR array", ++subnum);
    {
        Array *arr = V64ArrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        test_validatefree(
            arr->len == 0 && arr->sz == 0 && arr->v64 == NULL,
            ARRAYFREE(arr),
            "Empty STR array: len=%zu, sz=%zu, v64=%p (expected 0,0,NULL)",
            arr->len, arr->sz, (void*)arr->v64
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: create ZERO‑filled STR array", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_STR);
        test_validatefree(
            arr->len == 5 && arr->sz >= 5,
            ARRAYFREE(arr),
            "ZERO STR array: len=%zu, sz=%zu (expected 5, >=5)", arr->len, arr->sz
        );
        for (size_t i = 0; i < 5; i++) {
            test_validatefree(
                value64_str(arr->v64[i]) != NULL &&
                strcmp(value64_str(arr->v64[i]), "") == 0,
                ARRAYFREE(arr),
                "STR[%zu] must be empty string, got '%s'",
                i, value64_str(arr->v64[i])
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);   // STR not related to FS, but kept for consistency

    test_sub("subtest %d: create NONE FS array", ++subnum);
    {
        Array *arrtmp = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        ARRAYFREE(arrtmp);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create ASC‑filled STR array", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_ASC, VALUE64_STR);
        test_validatefree(
            arr->len == 4,
            ARRAYFREE(arr),
            "ASC STR array: len=%zu (expected 4)", arr->len
        );
        for (size_t i = 1; i < 4; i++) {
            test_validatefree(
                strlen(value64_str(arr->v64[i])) >= strlen(value64_str(arr->v64[i-1])),
                ARRAYFREE(arr),
                "ASC STR: length must be non‑decreasing"
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /*test_sub("subtest %d: create DESC‑filled STR array", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_DESC, VALUE64_STR);
        test_validatefree(
            arr->len == 4,
            ARRAYFREE(arr),
            "DESC STR array: len=%d (expected 4)", arr->len
        );
        for (size_t i = 1; i < 4; i++) {
            test_validatefree(
                strlen(value64_str(arr->v64[i])) <= strlen(value64_str(arr->v64[i-1])),
                ARRAYFREE(arr),
                "DESC STR: length must be non‑increasing"
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true); */    // because new v64 generator

    /* ---------- VALUE64_FS ---------- */
    test_sub("subtest %d: create empty FS array", ++subnum);
    {
        Array *arr = V64ArrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        test_validatefree(
            arr->len == 0 && arr->sz == 0 && arr->v64 == NULL,
            ARRAYFREE(arr),
            "Empty FS array: len=%zu, sz=%zu, v64=%p (expected 0,0,NULL)",
            arr->len, arr->sz, (void*)arr->v64
        );
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create ZERO‑filled FS array", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_FS);
        test_validatefree(
            arr->len == 5 && arr->sz >= 5,
            ARRAYFREE(arr),
            "ZERO FS array: len=%zu, sz=%zu (expected 5, >=5)", arr->len, arr->sz
        );
        for (size_t i = 0; i < 5; i++) {
            fs *f = value64_fs(arr->v64[i]);
            test_validatefree(
                f != NULL && fs_len(f) == 0 && fs_str(f)[0] == '\0',
                ARRAYFREE(arr),
                "FS[%zu] must be empty fs", i
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create ASC‑filled FS array", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_ASC, VALUE64_FS);
        test_validatefree(
            arr->len == 3,
            ARRAYFREE(arr),
            "ASC FS array: len=%zu (expected 3)", arr->len
        );
        for (size_t i = 1; i < 3; i++) {
            test_validatefree(
                fs_len(value64_fs(arr->v64[i])) >= fs_len(value64_fs(arr->v64[i-1])),
                ARRAYFREE(arr),
                "ASC FS: length must be non‑decreasing"
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    
    /*test_sub("subtest %d: create DESC‑filled FS array", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_DESC, VALUE64_FS);
        test_validatefree(
            arr->len == 3,
            ARRAYFREE(arr),
            "DESC FS array: len=%d (expected 3)", arr->len
        );
        for (size_t i = 1; i < 3; i++) {
            test_validatefree(
                fs_len(value64_fs(arr->v64[i])) <= fs_len(value64_fs(arr->v64[i-1])),
                ARRAYFREE(arr),
                "DESC FS: length must be non‑increasing"
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true); */ // because new v64 generator

    /* ---------- RND fill ---------- */
    test_sub("subtest %d: create RND‑filled STR array", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_RND, VALUE64_STR);
        test_validatefree(
            arr->len == 5,
            ARRAYFREE(arr),
            "RND STR array: len=%zu (expected 5)", arr->len
        );
        for (size_t i = 0; i < 5; i++) {
            const char *s = value64_str(arr->v64[i]);
            logmsg("VALUE64_STR: ARRAY_FILLTYPE_RND: %s", s);
            test_validatefree(
                s != NULL && strlen(s) > 0,
                ARRAYFREE(arr),
                "RND STR[%zu] must be non‑empty, got '%s'", i, s ? s : "NULL"
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create RND‑filled FS array", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_RND, VALUE64_FS);
        test_validatefree(
            arr->len == 5,
            ARRAYFREE(arr),
            "RND FS array: len=%zu (expected 5)", arr->len
        );
        for (size_t i = 0; i < 5; i++) {
            fs *f = value64_fs(arr->v64[i]);
            logmsg("VALUE64_FS: ARRAY_FILLTYPE_RND: %s", fs_str(f));
            test_validatefree(
                f != NULL && fs_len(f) > 0,
                ARRAYFREE(arr),
                "RND FS[%zu] must be non‑empty, got len=%zu", i, f ? fs_len(f) : -1
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /* ---------- ASC / DESC with length check ---------- */
    test_sub("subtest %d: create ASC‑filled STR (lengths non‑decreasing)", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_ASC, VALUE64_STR);
        test_validatefree(arr->len == 4, ARRAYFREE(arr), "len check");
        for (size_t i = 1; i < 4; i++) {
            test_validatefree(
                strlen(value64_str(arr->v64[i])) >= strlen(value64_str(arr->v64[i-1])),
                ARRAYFREE(arr),
                "ASC STR: length must be non‑decreasing"
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /*test_sub("subtest %d: create DESC‑filled FS (lengths non‑increasing)", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_DESC, VALUE64_FS);
        test_validatefree(arr->len == 4, ARRAYFREE(arr), "len check");
        for (size_t i = 1; i < 4; i++) {
            test_validatefree(
                fs_len(value64_fs(arr->v64[i])) <= fs_len(value64_fs(arr->v64[i-1])),
                ARRAYFREE(arr),
                "DESC FS: length must be non‑increasing"
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true); */ // because new v64 generator

    /* ---------- ZERO (empty strings) ---------- */
    test_sub("subtest %d: ZERO STR array must have empty strings", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_ZERO, VALUE64_STR);
        for (size_t i = 0; i < 3; i++) {
            test_validatefree(
                strcmp(value64_str(arr->v64[i]), "") == 0,
                ARRAYFREE(arr),
                "ZERO STR[%zu] must be empty", i
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST V64Array (STR / FS) shrink / increase -------------------------
static TestStatus
tf_v64array_shrink_increase(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== STR array: increase then shrink ========== */
    test_sub("subtest %d: STR increase + shrink", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_ZERO, VALUE64_STR);
        // заполним явно, чтобы потом проверять
        arr->v64[0] = value64_createstr("first");
        arr->v64[1] = value64_createstr("second");
        arr->v64[2] = value64_createstr("third");

        // увеличиваем до 6
        arr = arrayIncrease(arr, 6);
        test_validatefree(
            arr->len == 6 && arr->sz >= 6,
            ARRAYFREE(arr),
            "After increase len=%zu (expected 6)", arr->len
        );
        // новые элементы должны быть пустыми строками
        for (size_t i = 3; i < 6; i++) {
            test_validatefree(
                value64_str(arr->v64[i]) != NULL && strcmp(value64_str(arr->v64[i]), "") == 0,
                ARRAYFREE(arr),
                "STR[%zu] must be empty after increase", i
            );
        }

        // уменьшаем обратно до 3
        arr = arrayShrink(arr, 3);
        test_validatefree(
            arr->len == 3 && arr->sz >= 3,
            ARRAYFREE(arr),
            "After shrink len=%zu (expected 3)", arr->len
        );
        // старые элементы должны сохраниться
        test_validatefree(
            strcmp(value64_str(arr->v64[0]), "first") == 0 &&
            strcmp(value64_str(arr->v64[1]), "second") == 0 &&
            strcmp(value64_str(arr->v64[2]), "third") == 0,
            ARRAYFREE(arr),
            "STR elements must survive shrink"
        );

        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /* ========== FS array: increase then shrink ========== */
    test_sub("subtest %d: FS increase + shrink", ++subnum);
    {
        Array *arr = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        // заполним явно
        arr->v64[0] = value64_createfs_asstr("/first");
        arr->v64[1] = value64_createfs_asstr("/second");

        // увеличиваем до 4
        arr = arrayIncrease(arr, 4);
        test_validatefree(
            arr->len == 4 && arr->sz >= 4,
            ARRAYFREE(arr),
            "After increase len=%zu (expected 4)", arr->len
        );
        // новые элементы – пустые fs
        for (size_t i = 2; i < 4; i++) {
            fs *f = value64_fs(arr->v64[i]);
            test_validatefree(
                f != NULL && fs_len(f) == 0,
                ARRAYFREE(arr),
                "FS[%zu] must be empty after increase", i
            );
        }

        // уменьшаем до 1
        arr = arrayShrink(arr, 1);
        test_validatefree(
            arr->len == 1 && arr->sz >= 1,
            ARRAYFREE(arr),
            "After shrink len=%zu (expected 1)", arr->len
        );
        // первый элемент должен сохраниться
        test_validatefree(
            strcmp(fs_str(value64_fs(arr->v64[0])), "/first") == 0,
            ARRAYFREE(arr),
            "FS[0] must survive shrink"
        );

        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /* ========== Shrink to zero ========== */
    test_sub("subtest %d: shrink to zero", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_ZERO, VALUE64_STR);
        arr->v64[0] = value64_createstr("a");
        arr->v64[1] = value64_createstr("b");
        arr->v64[2] = value64_createstr("c");

        arr = arrayShrink(arr, 0);
        test_validatefree(
            arr->len == 0 && arr->sz == 0,
            ARRAYFREE(arr),
            "Shrink to zero: len=%zu sz=%zu (expected 0,0)", arr->len, arr->sz
        );
        // v64 должен быть NULL (память освобождена)
        test_validatefree(
            arr->v64 == NULL,
            ARRAYFREE(arr),
            "v64 must be NULL after shrinking to zero"
        );
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST V64Array (STR / FS) sorting -------------------------
static TestStatus
tf_v64array_sort(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== STR sorting ========== */
    test_sub("subtest %d: STR sort ASC", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("delta");
        arr->v64[1] = value64_createstr("alpha");
        arr->v64[2] = value64_createstr("charlie");
        arr->v64[3] = value64_createstr("beta");

        arrayQsort(arr, ARRAY_SORTTYPE_ASC);

        const char *expected[] = {"alpha", "beta", "charlie", "delta"};
        for (size_t i = 0; i < 4; i++) {
            test_validatefree(
                strcmp(value64_str(arr->v64[i]), expected[i]) == 0,
                ARRAYFREE(arr),
                "STR ASC [%zu]: expected '%s', got '%s'",
                i, expected[i], value64_str(arr->v64[i])
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: STR sort DESC", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("delta");
        arr->v64[1] = value64_createstr("alpha");
        arr->v64[2] = value64_createstr("charlie");
        arr->v64[3] = value64_createstr("beta");

        arrayQsort(arr, ARRAY_SORTTYPE_DESC);

        const char *expected[] = {"delta", "charlie", "beta", "alpha"};
        for (size_t i = 0; i < 4; i++) {
            test_validatefree(
                strcmp(value64_str(arr->v64[i]), expected[i]) == 0,
                ARRAYFREE(arr),
                "STR DESC [%zu]: expected '%s', got '%s'",
                i, expected[i], value64_str(arr->v64[i])
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /* ========== FS sorting ========== */
    test_sub("subtest %d: FS sort ASC", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("/zzz");
        arr->v64[1] = value64_createfs_asstr("/aaa");
        arr->v64[2] = value64_createfs_asstr("/mmm");
        arr->v64[3] = value64_createfs_asstr("/bbb");

        arrayQsort(arr, ARRAY_SORTTYPE_ASC);

        const char *expected[] = {"/aaa", "/bbb", "/mmm", "/zzz"};
        for (size_t i = 0; i < 4; i++) {
            test_validatefree(
                strcmp(fs_str(value64_fs(arr->v64[i])), expected[i]) == 0,
                ARRAYFREE(arr),
                "FS ASC [%zu]: expected '%s', got '%s'",
                i, expected[i], fs_str(value64_fs(arr->v64[i]))
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS sort DESC", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("/zzz");
        arr->v64[1] = value64_createfs_asstr("/aaa");
        arr->v64[2] = value64_createfs_asstr("/mmm");
        arr->v64[3] = value64_createfs_asstr("/bbb");

        arrayQsort(arr, ARRAY_SORTTYPE_DESC);

        const char *expected[] = {"/zzz", "/mmm", "/bbb", "/aaa"};
        for (size_t i = 0; i < 4; i++) {
            test_validatefree(
                strcmp(fs_str(value64_fs(arr->v64[i])), expected[i]) == 0,
                ARRAYFREE(arr),
                "FS DESC [%zu]: expected '%s', got '%s'",
                i, expected[i], fs_str(value64_fs(arr->v64[i]))
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /* ========== edge cases ========== */

    test_sub("subtest %d: STR sort empty array", ++subnum);
    {
        Array *arr = V64ArrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);   // must not crash
        test_validatefree(
            arr->len == 0,
            ARRAYFREE(arr),
            "Empty STR array after sort must still be empty"
        );
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: STR sort single element", ++subnum);
    {
        Array *arr = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("single");
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        test_validatefree(
            strcmp(value64_str(arr->v64[0]), "single") == 0,
            ARRAYFREE(arr),
            "Single STR element must survive sorting"
        );
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: STR sort already sorted", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("a");
        arr->v64[1] = value64_createstr("b");
        arr->v64[2] = value64_createstr("c");
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        const char *exp[] = {"a", "b", "c"};
        for (size_t i = 0; i < 3; i++) {
            test_validatefree(
                strcmp(value64_str(arr->v64[i]), exp[i]) == 0,
                ARRAYFREE(arr),
                "Already sorted STR [%zu] must stay '%s'", i, exp[i]
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: STR sort with duplicates", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("a");
        arr->v64[1] = value64_createstr("b");
        arr->v64[2] = value64_createstr("a");
        arr->v64[3] = value64_createstr("c");
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        const char *exp[] = {"a", "a", "b", "c"};
        for (size_t i = 0; i < 4; i++) {
            test_validatefree(
                strcmp(value64_str(arr->v64[i]), exp[i]) == 0,
                ARRAYFREE(arr),
                "Duplicates STR [%zu] must be '%s'", i, exp[i]
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS sort empty array", ++subnum);
    {
        Array *arr = V64ArrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        test_validatefree(arr->len == 0, ARRAYFREE(arr), "Empty FS array after sort must still be empty");
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS sort single element", ++subnum);
    {
        Array *arr = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("/only");
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        test_validatefree(
            strcmp(fs_str(value64_fs(arr->v64[0])), "/only") == 0,
            ARRAYFREE(arr),
            "Single FS element must survive sorting"
        );
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS sort already sorted", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("/a");
        arr->v64[1] = value64_createfs_asstr("/b");
        arr->v64[2] = value64_createfs_asstr("/c");
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        const char *exp[] = {"/a", "/b", "/c"};
        for (size_t i = 0; i < 3; i++) {
            test_validatefree(
                strcmp(fs_str(value64_fs(arr->v64[i])), exp[i]) == 0,
                ARRAYFREE(arr),
                "Already sorted FS [%zu] must stay '%s'", i, exp[i]
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS sort with duplicates", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("/a");
        arr->v64[1] = value64_createfs_asstr("/b");
        arr->v64[2] = value64_createfs_asstr("/a");
        arr->v64[3] = value64_createfs_asstr("/c");
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        const char *exp[] = {"/a", "/a", "/b", "/c"};
        for (size_t i = 0; i < 4; i++) {
            test_validatefree(
                strcmp(fs_str(value64_fs(arr->v64[i])), exp[i]) == 0,
                ARRAYFREE(arr),
                "Duplicates FS [%zu] must be '%s'", i, exp[i]
            );
        }
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST V64Array (STR / FS) save/load -------------------------
static TestStatus
tf_v64arraySaveFile_load(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== STR save/load ========== */
    test_sub("subtest %d: STR save/load", ++subnum);
    {
        const char *fname = "res/array/v64str.sv";

        Array *orig = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        orig->v64[0] = value64_createstr("one");
        orig->v64[1] = value64_createstr("two");
        orig->v64[2] = value64_createstr("three");

        long written = arraySaveFileByName(orig, fname);
        test_validatefree(
            written > 0,
            ARRAYFREE(orig),
            "STR save failed"
        );

        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == orig->len,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "STR load: len=%zu, expected %zu", loaded->len, orig->len
        );

        for (size_t i = 0; i < orig->len; i++) {
            test_validatefree(
                strcmp(value64_str(orig->v64[i]), value64_str(loaded->v64[i])) == 0,
                (ARRAYFREE(orig), ARRAYFREE(loaded)),
                "STR[%zu]: orig='%s', loaded='%s'",
                i, value64_str(orig->v64[i]), value64_str(loaded->v64[i])
            );
        }

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }
    fs_alloc_check(true);

    /* ========== FS save/load ========== */
    test_sub("subtest %d: FS save/load", ++subnum);
    {
        const char *fname = "res/array/v64fs.sv";

        Array *orig = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        orig->v64[0] = value64_createfs_asstr("/alpha");
        orig->v64[1] = value64_createfs_asstr("/beta");
        orig->v64[2] = value64_createfs_asstr("/gamma");

        long written = arraySaveFileByName(orig, fname);
        test_validatefree(written > 0, ARRAYFREE(orig), "FS save failed");

        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == orig->len,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "FS load: len=%zu, expected %zu", loaded->len, orig->len
        );

        for (size_t i = 0; i < orig->len; i++) {
            fs *f_orig = value64_fs(orig->v64[i]);
            fs *f_load = value64_fs(loaded->v64[i]);
            test_validatefree(
                f_orig && f_load && strcmp(fs_str(f_orig), fs_str(f_load)) == 0,
                (ARRAYFREE(orig), ARRAYFREE(loaded)),
                "FS[%zu]: orig='%s', loaded='%s'",
                i, f_orig ? fs_str(f_orig) : "NULL",
                f_load ? fs_str(f_load) : "NULL"
            );
        }

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }
    fs_alloc_check(true);

    /* ========== STR save/load: edge cases ========== */

    test_sub("subtest %d: STR save/load empty array", ++subnum);
    {
        const char *fname = "res/array/v64str_empty.sv";

        Array *orig = V64ArrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        long written = arraySaveFileByName(orig, fname);
        test_validatefree(written > 0, ARRAYFREE(orig), "STR empty save failed");

        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == 0 && loaded->v64 == NULL,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "Loaded empty STR: len=%zu, v64=%p (expected 0, NULL)", loaded->len, (void*)loaded->v64
        );

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: STR save/load single element", ++subnum);
    {
        const char *fname = "res/array/v64str_single.sv";

        Array *orig = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        orig->v64[0] = value64_createstr("single");
        long written = arraySaveFileByName(orig, fname);
        test_validatefree(written > 0, ARRAYFREE(orig), "STR single save failed");

        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == 1,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "Loaded single STR: len=%zu, expected 1", loaded->len
        );
        test_validatefree(
            strcmp(value64_str(orig->v64[0]), value64_str(loaded->v64[0])) == 0,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "STR single: orig='%s', loaded='%s'",
            value64_str(orig->v64[0]), value64_str(loaded->v64[0])
        );

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }
    fs_alloc_check(true);

    /* ========== FS save/load: edge cases ========== */

    test_sub("subtest %d: FS save/load empty array", ++subnum);
    {
        const char *fname = "res/array/v64fs_empty.sv";

        Array *orig = V64ArrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        long written = arraySaveFileByName(orig, fname);
        test_validatefree(written > 0, ARRAYFREE(orig), "FS empty save failed");

        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == 0 && loaded->v64 == NULL,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "Loaded empty FS: len=%zu, v64=%p (expected 0, NULL)", loaded->len, (void*)loaded->v64
        );

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS save/load single element", ++subnum);
    {
        const char *fname = "res/array/v64fs_single.sv";

        Array *orig = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        orig->v64[0] = value64_createfs_asstr("/only");
        long written = arraySaveFileByName(orig, fname);
        test_validatefree(written > 0, ARRAYFREE(orig), "FS single save failed");

        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == 1,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "Loaded single FS: len=%zu, expected 1", loaded->len
        );
        fs *f_orig = value64_fs(orig->v64[0]);
        fs *f_load = value64_fs(loaded->v64[0]);
        test_validatefree(
            f_orig && f_load && strcmp(fs_str(f_orig), fs_str(f_load)) == 0,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "FS single: orig='%s', loaded='%s'",
            f_orig ? fs_str(f_orig) : "NULL", f_load ? fs_str(f_load) : "NULL"
        );

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayBsearch (INT / LONG / DBL / V64) -------------------------
static TestStatus
tf_array_bsearch(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== INT ========== */
    test_sub("subtest %d: INT find existing", ++subnum);
    {
        Array *arr = IarrayCreate(10, ARRAY_FILLTYPE_ASC_SERIES); // 0,1,2,...,9
        long idx;
        test_validatefree(
            (idx = arrayBsearchInt(arr, 5)) == 5,
            ARRAYFREE(arr),
            "INT asc: expected idx=5, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: INT find missing", ++subnum);
    {
        Array *arr = IarrayCreate(10, ARRAY_FILLTYPE_ASC_SERIES);
        long idx;
        test_validatefree(
            (idx = arrayBsearchInt(arr, 99)) == -1,
            ARRAYFREE(arr),
            "INT asc missing: expected -1, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: INT find first / last", ++subnum);
    {
        Array *arr = IarrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES);
        test_validatefree(
            arrayBsearchInt(arr, 0) == 0 && arrayBsearchInt(arr, 4) == 4,
            ARRAYFREE(arr),
            "INT first/last: must be 0 and 4"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: INT rev search", ++subnum);
    {
        Array *arr = IarrayCreate(10, ARRAY_FILLTYPE_DESC_SERIES); // 9,8,...,0
        long idx;
        test_validatefree(
            (idx = arrayBsearchIntrev(arr, 5)) == 4,   // 9(0),8(1),7(2),6(3),5(4)
            ARRAYFREE(arr),
            "INT desc: expected idx=4, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: INT empty array", ++subnum);
    {
        Array *arr = IarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        test_validatefree(
            arrayBsearchInt(arr, 5) == -1,
            ARRAYFREE(arr),
            "Empty INT: must return -1"
        );
        ARRAYFREE(arr);
    }

    /* ========== LONG ========== */
    test_sub("subtest %d: LONG find existing", ++subnum);
    {
        Array *arr = LarrayCreate(10, ARRAY_FILLTYPE_ASC_SERIES);
        long idx;
        test_validatefree(
            (idx = arrayBsearchLong(arr, 7L)) == 7,
            ARRAYFREE(arr),
            "LONG asc: expected idx=7, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: LONG rev missing", ++subnum);
    {
        Array *arr = LarrayCreate(10, ARRAY_FILLTYPE_DESC_SERIES);
        long idx;
        test_validatefree(
            (idx = arrayBsearchLongRev(arr, 100L)) == -1,
            ARRAYFREE(arr),
            "LONG desc missing: expected -1, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    /* ========== DBL ========== */
    test_sub("subtest %d: DBL find first / last", ++subnum);
    {
        Array *arr = DarrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES); // 0.0,1.0,...,4.0
        test_validatefree(
            arrayBsearchDbl(arr, 0.0) == 0 && arrayBsearchDbl(arr, 4.0) == 4,
            ARRAYFREE(arr),
            "DBL first/last: must be 0 and 4"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: DBL rev search", ++subnum);
    {
        Array *arr = DarrayCreate(5, ARRAY_FILLTYPE_DESC_SERIES); // 4.0,3.0,...,0.0
        long idx;
        test_validatefree(
            (idx = arrayBsearchDblRev(arr, 2.0)) == 2,   // 4(0),3(1),2(2)
            ARRAYFREE(arr),
            "DBL desc: expected idx=2, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    /* ========== V64 (STR) ========== */
    test_sub("subtest %d: V64 STR find existing", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("a");
        arr->v64[1] = value64_createstr("b");
        arr->v64[2] = value64_createstr("c");
        arr->v64[3] = value64_createstr("d");

        value64 key = LITERAL64_STR("c");
        long idx;
        test_validatefree(
            (idx = arrayBsearchV64(arr, key)) == 2,
            ARRAYFREE(arr),
            "STR asc: expected idx=2, got %ld", idx
        );
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: V64 STR rev missing", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("d");
        arr->v64[1] = value64_createstr("c");
        arr->v64[2] = value64_createstr("b");
        arr->v64[3] = value64_createstr("a");

        value64 key = LITERAL64_STR("x");
        long idx;
        test_validatefree(
            (idx = arrayBsearchV64Rev(arr, key)) == -1,
            ARRAYFREE(arr),
            "STR desc missing: expected -1, got %ld", idx
        );
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /* ========== V64 (FS) ========== */
    test_sub("subtest %d: V64 FS find existing", ++subnum);
    {
        Array *arr = V64ArrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("/alpha");
        arr->v64[1] = value64_createfs_asstr("/beta");
        arr->v64[2] = value64_createfs_asstr("/gamma");
        arr->v64[3] = value64_createfs_asstr("/delta");

        arrayQsort(arr, ARRAY_SORTTYPE_ASC);

        value64 key = value64_createfs_asstr("/beta");
        long idx;
        test_validatefree(
            (idx = arrayBsearchV64(arr, key)) == 1,
            (ARRAYFREE(arr), value64_freefs(&key)),
            "FS asc: expected idx=1, got %ld", idx
        );
        value64_freefs(&key);
        ARRAYFREE(arr);
    }
    fs_alloc_check(true);

    /* ========== Type mismatch (must raise error) ========== */
    test_sub("subtest %d: type mismatch raises SIGINT", ++subnum);
    {
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_ASC_SERIES);
        if (!try()) {
            arrayBsearchLong(arr, 5L);
            test_validatefree(false, ARRAYFREE(arr), "Should have raised SIGINT");
        } else {
            logsimple("Exception correctly raised on type mismatch");
        }
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST CARray (char) create/fill/free -------------------------
static TestStatus
tf_carray_create_fill_free(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Создание пустого CHAR массива */
    test_sub("subtest %d: create empty CHAR array", ++subnum);
    {
        Array *arr = CarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        test_validatefree(
            arr->len == 0 && arr->sz == 0 && arr->cv == NULL,
            ARRAYFREE(arr),
            "Empty CHAR array: len=%zu, sz=%zu, cv=%p (expected 0,0,NULL)",
            arr->len, arr->sz, (void*)arr->cv
        );
        ARRAYFREE(arr);
    }

    /* 2. Создание ZERO‑filled CHAR массива */
    test_sub("subtest %d: create ZERO‑filled CHAR array", ++subnum);
    {
        Array *arr = CarrayCreate(5, ARRAY_FILLTYPE_ZERO);
        test_validatefree(
            arr->len == 5 && arr->sz >= 5,
            ARRAYFREE(arr),
            "ZERO CHAR array: len=%zu, sz=%zu (expected 5, >=5)", arr->len, arr->sz
        );
        // Проверяем, что все элементы — '\0'
        for (size_t i = 0; i < 5; i++) {
            test_validatefree(
                arr->cv[i] == '\0',
                ARRAYFREE(arr),
                "CHAR[%zu] must be '\\0', got '%c'", i, arr->cv[i]
            );
        }
        ARRAYFREE(arr);
    }

    /* 3. Создание ASC‑filled CHAR массива (случайные буквы) */
    test_sub("subtest %d: create ASC‑filled CHAR array", ++subnum);
    {
        Array *arr = CarrayCreate(4, ARRAY_FILLTYPE_ASC);
        test_validatefree(
            arr->len == 4,
            ARRAYFREE(arr),
            "ASC CHAR array: len=%zu (expected 4)", arr->len
        );
        // Элементы не должны быть '\0' и должны следовать в алфавитном порядке
        for (size_t i = 1; i < 4; i++) {
            test_validatefree(
                arr->cv[i - 1] <= arr->cv[i],
                ARRAYFREE(arr),
                "ASC CHAR: must be non‑decreasing"
            );
        }
        ARRAYFREE(arr);
    }

    /* 4. Создание DESC‑filled CHAR массива (случайные буквы в обратном порядке) */
    test_sub("subtest %d: create DESC‑filled CHAR array", ++subnum);
    {
        Array *arr = CarrayCreate(4, ARRAY_FILLTYPE_DESC);
        test_validatefree(
            arr->len == 4,
            ARRAYFREE(arr),
            "DESC CHAR array: len=%zu (expected 4)", arr->len
        );
        // Элементы должны быть не возрастающими
        for (size_t i = 1; i < 4; i++) {
            test_validatefree(
                arr->cv[i - 1] >= arr->cv[i],
                ARRAYFREE(arr),
                "DESC CHAR: must be non‑increasing"
            );
        }
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST CARray (char) sort -------------------------
static TestStatus
tf_carray_sort(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Сортировка по возрастанию */
    test_sub("subtest %d: CHAR sort ASC", ++subnum);
    {
        Array *arr = CarrayCreate(6, ARRAY_FILLTYPE_RND);
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);

        for (size_t i = 1; i < arr->len; i++) {
            test_validatefree(
                arr->cv[i - 1] <= arr->cv[i],
                ARRAYFREE(arr),
                "ASC CHAR: cv[%zu]='%c' > cv[%zu]='%c'",
                i - 1, arr->cv[i - 1], i, arr->cv[i]
            );
        }
        ARRAYFREE(arr);
    }

    /* 2. Сортировка по убыванию */
    test_sub("subtest %d: CHAR sort DESC", ++subnum);
    {
        Array *arr = CarrayCreate(6, ARRAY_FILLTYPE_RND);
        arrayQsort(arr, ARRAY_SORTTYPE_DESC);

        for (size_t i = 1; i < arr->len; i++) {
            test_validatefree(
                arr->cv[i - 1] >= arr->cv[i],
                ARRAYFREE(arr),
                "DESC CHAR: cv[%zu]='%c' < cv[%zu]='%c'",
                i - 1, arr->cv[i - 1], i, arr->cv[i]
            );
        }
        ARRAYFREE(arr);
    }

    /* 3. Пустой массив */
    test_sub("subtest %d: CHAR sort empty", ++subnum);
    {
        Array *arr = CarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);   // не должно упасть
        test_validatefree(arr->len == 0, ARRAYFREE(arr), "Empty array must stay empty after sort");
        ARRAYFREE(arr);
    }

    /* 4. Один элемент */
    test_sub("subtest %d: CHAR sort single element", ++subnum);
    {
        Array *arr = CarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'x';
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        test_validatefree(
            arr->cv[0] == 'x',
            ARRAYFREE(arr),
            "Single element 'x' must stay 'x', got '%c'", arr->cv[0]
        );
        ARRAYFREE(arr);
    }

    /* 5. Уже отсортированный */
    test_sub("subtest %d: CHAR sort already sorted", ++subnum);
    {
        Array *arr = CarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'a'; arr->cv[1] = 'b'; arr->cv[2] = 'c';
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        test_validatefree(
            arr->cv[0] == 'a' && arr->cv[1] == 'b' && arr->cv[2] == 'c',
            ARRAYFREE(arr),
            "Already sorted array must stay 'a','b','c'"
        );
        ARRAYFREE(arr);
    }

    /* 6. Дубликаты */
    test_sub("subtest %d: CHAR sort duplicates", ++subnum);
    {
        Array *arr = CarrayCreate(4, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'b'; arr->cv[1] = 'a'; arr->cv[2] = 'b'; arr->cv[3] = 'c';
        arrayQsort(arr, ARRAY_SORTTYPE_ASC);
        test_validatefree(
            arr->cv[0] == 'a' && arr->cv[1] == 'b' && arr->cv[2] == 'b' && arr->cv[3] == 'c',
            ARRAYFREE(arr),
            "Duplicates must be sorted correctly"
        );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayBsearch for CHAR / V64 (STR / FS) -------------------------
static TestStatus
tf_array_bsearch_char(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== CHAR ========== */
    test_sub("subtest %d: CHAR find existing", ++subnum);
    {
        Array *arr = CarrayCreate(5, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'a'; arr->cv[1] = 'b'; arr->cv[2] = 'c'; arr->cv[3] = 'd'; arr->cv[4] = 'e';
        long idx = arrayBsearchChar(arr, 'c');
        test_validatefree(
            idx == 2,
            ARRAYFREE(arr),
            "CHAR asc: expected idx=2, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: CHAR find missing", ++subnum);
    {
        Array *arr = CarrayCreate(5, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'a'; arr->cv[1] = 'b'; arr->cv[2] = 'c'; arr->cv[3] = 'd'; arr->cv[4] = 'e';
        long idx = arrayBsearchChar(arr, 'z');
        test_validatefree(
            idx == -1,
            ARRAYFREE(arr),
            "CHAR asc missing: expected -1, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: CHAR rev search", ++subnum);
    {
        Array *arr = CarrayCreate(5, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'e'; arr->cv[1] = 'd'; arr->cv[2] = 'c'; arr->cv[3] = 'b'; arr->cv[4] = 'a';
        long idx = arrayBsearchCharRev(arr, 'b');
        test_validatefree(
            idx == 3,   // e(0),d(1),c(2),b(3),a(4)
            ARRAYFREE(arr),
            "CHAR desc: expected idx=3, got %ld", idx
        );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST Array save/load (CHAR) -------------------------
static TestStatus
tf_arraySaveFile_load_char(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== CHAR ========== */
    test_sub("subtest %d: CHAR save/load", ++subnum);
    {
        const char *fname = "res/array/carr.sv";

        // создаём массив и заполняем
        Array *orig = CarrayCreate(5, ARRAY_FILLTYPE_SAFE_EMPTY);
        orig->cv[0] = 'h'; orig->cv[1] = 'e'; orig->cv[2] = 'l';
        orig->cv[3] = 'l'; orig->cv[4] = 'o';

        // сохраняем
        long written = arraySaveFileByName(orig, fname);
        test_validatefree(written > 0, ARRAYFREE(orig), "CHAR save failed");

        // загружаем
        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == orig->len,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "CHAR load: len=%zu, expected %zu", loaded->len, orig->len
        );

        // сравниваем поэлементно
        for (size_t i = 0; i < orig->len; i++) {
            test_validatefree(
                orig->cv[i] == loaded->cv[i],
                (ARRAYFREE(orig), ARRAYFREE(loaded)),
                "CHAR[%zu]: orig='%c', loaded='%c'",
                i, orig->cv[i], loaded->cv[i]
            );
        }

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }

    /* ========== CHAR: пустой массив ========== */
    test_sub("subtest %d: CHAR save/load empty", ++subnum);
    {
        const char *fname = "res/array/carr_empty.sv";

        Array *orig = CarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        long written = arraySaveFileByName(orig, fname);
        test_validatefree(written > 0, ARRAYFREE(orig), "CHAR empty save failed");

        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == 0 && loaded->cv == NULL,
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "Loaded empty CHAR: len=%zu, cv=%p (expected 0, NULL)", loaded->len, (void*)loaded->cv
        );

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }

    /* ========== CHAR: один элемент ========== */
    test_sub("subtest %d: CHAR save/load single", ++subnum);
    {
        const char *fname = "res/array/carr_single.sv";

        Array *orig = CarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        orig->cv[0] = 'Z';
        long written = arraySaveFileByName(orig, fname);
        test_validatefree(written > 0, ARRAYFREE(orig), "CHAR single save failed");

        Array *loaded = arrayLoadFileByName(fname);
        test_validatefree(
            loaded->len == 1 && loaded->cv[0] == 'Z',
            (ARRAYFREE(orig), ARRAYFREE(loaded)),
            "Loaded single CHAR: expected 'Z', got '%c'", loaded->cv[0]
        );

        ARRAYFREE(orig);
        ARRAYFREE(loaded);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayEq / arrayNoteq (all types, edge cases) -------------------------
static TestStatus
tf_array_eq_noteq(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== INT ========== */
    test_sub("subtest %d: INT equal", ++subnum);
    {
        Array *a = IarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        Array *b = IarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        a->iv[0] = 1; a->iv[1] = 2; a->iv[2] = 3;
        b->iv[0] = 1; b->iv[1] = 2; b->iv[2] = 3;
        test_validatefree(
            arrayEq(a, b) && !arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Identical INT arrays must be equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }

    test_sub("subtest %d: INT not equal (different values)", ++subnum);
    {
        Array *a = IarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        Array *b = IarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        a->iv[0] = 10; a->iv[1] = 20;
        b->iv[0] = 10; b->iv[1] = 30;
        test_validatefree(
            !arrayEq(a, b) && arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Different values must be not equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }

    test_sub("subtest %d: INT not equal (different lengths)", ++subnum);
    {
        Array *a = IarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        Array *b = IarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        test_validatefree(
            !arrayEq(a, b) && arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Different lengths must be not equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }

    test_sub("subtest %d: INT empty arrays", ++subnum);
    {
        Array *a = IarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        Array *b = IarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        test_validatefree(
            arrayEq(a, b) && !arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Empty INT arrays must be equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }

    /* ========== CHAR ========== */
    test_sub("subtest %d: CHAR equal", ++subnum);
    {
        Array *a = CarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        Array *b = CarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        a->cv[0] = 'x'; a->cv[1] = 'y';
        b->cv[0] = 'x'; b->cv[1] = 'y';
        test_validatefree(
            arrayEq(a, b) && !arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Identical CHAR arrays must be equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }

    test_sub("subtest %d: CHAR not equal", ++subnum);
    {
        Array *a = CarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        Array *b = CarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        a->cv[0] = 'a';
        b->cv[0] = 'b';
        test_validatefree(
            !arrayEq(a, b) && arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Different CHAR must be not equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }

    /* ========== V64 STR ========== */
    test_sub("subtest %d: V64 STR equal", ++subnum);
    {
        Array *a = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        Array *b = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        a->v64[0] = value64_createstr("hello");
        a->v64[1] = value64_createstr("world");
        b->v64[0] = value64_createstr("hello");
        b->v64[1] = value64_createstr("world");
        test_validatefree(
            arrayEq(a, b) && !arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Identical STR arrays must be equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: V64 STR not equal", ++subnum);
    {
        Array *a = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        Array *b = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        a->v64[0] = value64_createstr("abc");
        b->v64[0] = value64_createstr("xyz");
        test_validatefree(
            !arrayEq(a, b) && arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Different STR must be not equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }
    fs_alloc_check(true);

    /* ========== V64 FS ========== */
    test_sub("subtest %d: V64 FS equal", ++subnum);
    {
        Array *a = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        Array *b = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        a->v64[0] = value64_createfs_asstr("/tmp/a");
        a->v64[1] = value64_createfs_asstr("/tmp/b");
        b->v64[0] = value64_createfs_asstr("/tmp/a");
        b->v64[1] = value64_createfs_asstr("/tmp/b");
        test_validatefree(
            arrayEq(a, b) && !arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Identical FS arrays must be equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: V64 FS not equal", ++subnum);
    {
        Array *a = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        Array *b = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        a->v64[0] = value64_createfs_asstr("/first");
        b->v64[0] = value64_createfs_asstr("/second");
        test_validatefree(
            !arrayEq(a, b) && arrayNoteq(a, b),
            (ARRAYFREE(a), ARRAYFREE(b)),
            "Different FS must be not equal"
        );
        ARRAYFREE(a); ARRAYFREE(b);
    }
    fs_alloc_check(true);

    /* ========== Type mismatch (must raise SIGINT) ========== */
    test_sub("subtest %d: type mismatch raises SIGINT", ++subnum);
    {
        Array *a = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        Array *b = CarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        if (!try()) {
            arrayNoteq(a, b);
            test_validatefree(false, (ARRAYFREE(a), ARRAYFREE(b)),
                             "Type mismatch should have raised SIGINT");
        } else {
            logsimple("Exception correctly raised on type mismatch");
        }
        ARRAYFREE(a); ARRAYFREE(b);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayDel simple test -------------------------
static TestStatus
tf_ArrayDel(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== arrayDel ========== */
    test_sub("subtest %d: arrayDel middle one element", ++subnum);
    {
        Array *arr = IarrayCreate(4, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30; arr->iv[3] = 40;
        arr->len = 4;

        arr = arrayDel(arr, 1, 1);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 10 && arr->iv[1] == 30 && arr->iv[2] == 40,
            ARRAYFREE(arr),
            "Del middle 1: expected [10,30,40] len=3, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayDel middle multiple elements", ++subnum);
    {
        Array *arr = IarrayCreate(4, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30; arr->iv[3] = 40;
        arr->len = 4;

        arr = arrayDel(arr, 1, 2);
        test_validatefree(
            arr != NULL && arraylen(arr) == 2 &&
            arr->iv[0] == 10 && arr->iv[1] == 40,
            ARRAYFREE(arr),
            "Del middle 2: expected [10,40] len=2, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayDel from start", ++subnum);
    {
        Array *arr = IarrayCreate(4, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30; arr->iv[3] = 40;
        arr->len = 4;

        arr = arrayDel(arr, 0, 1);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 20 && arr->iv[1] == 30 && arr->iv[2] == 40,
            ARRAYFREE(arr),
            "Del start: expected [20,30,40] len=3, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayDel from end", ++subnum);
    {
        Array *arr = IarrayCreate(4, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30; arr->iv[3] = 40;
        arr->len = 4;

        arr = arrayDel(arr, 3, 1);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 10 && arr->iv[1] == 20 && arr->iv[2] == 30,
            ARRAYFREE(arr),
            "Del end: expected [10,20,30] len=3, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayDel all elements", ++subnum);
    {
        Array *arr = IarrayCreate(4, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30; arr->iv[3] = 40;
        arr->len = 4;

        arr = arrayDel(arr, 0, 4);
        test_validatefree(
            arr != NULL && arraylen(arr) == 0,
            ARRAYFREE(arr),
            "Del all: expected empty array, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayDel zero count", ++subnum);
    {
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 1; arr->iv[1] = 2; arr->iv[2] = 3;
        arr->len = 3;

        arr = arrayDel(arr, 1, 0);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 1 && arr->iv[1] == 2 && arr->iv[2] == 3,
            ARRAYFREE(arr),
            "Del zero: array must remain unchanged, len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayDel from beyond length", ++subnum);
    {
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 1; arr->iv[1] = 2; arr->iv[2] = 3;
        arr->len = 3;

        arr = arrayDel(arr, 5, 1);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 1 && arr->iv[1] == 2 && arr->iv[2] == 3,
            ARRAYFREE(arr),
            "Del beyond: array must remain unchanged, len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayDel cnt not exceeds length", ++subnum);
    {
        Array *arr = IarrayCreate(4, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30; arr->iv[3] = 40;
        //arr->len = 4;

        arr = arrayDel(arr, 2, 2 /*10*/ );   // удалит 30,40
        test_validatefree(
            arr != NULL && arraylen(arr) == 2 &&
            arr->iv[0] == 10 && arr->iv[1] == 20,
            ARRAYFREE(arr),
            "Del over: expected [10,20] len=2, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayDel cnt exceeds length", ++subnum);
    {
        Array *arr = IarrayCreate(4, ARRAY_FILLTYPE_ASC_SERIES);
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30; arr->iv[3] = 40;
        //arr->len = 4;

        arr = arrayDel(arr, 2, 10);   // удалит 30,40
        test_validatefree(
            arr != NULL && arraylen(arr) == 2 &&
            arr->iv[0] == 10 && arr->iv[1] == 20,
            ARRAYFREE(arr),
            "Del over: expected [10,20] len=2, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayAdd simple test -------------------------
static TestStatus
tf_ArrayAdd(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== arrayAdd ========== */
    test_sub("subtest %d: arrayAdd one element in middle", ++subnum);
    {
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_ASC_SERIES); // [0,1,2]
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30;

        arr = arrayAdd(arr, 1, 1, ARRAY_FILLTYPE_ZERO);
        arr->iv[1] = 99;
        test_validatefree(
            arr != NULL && arraylen(arr) == 4 &&
            arr->iv[0] == 10 && arr->iv[1] == 99 && arr->iv[2] == 20 && arr->iv[3] == 30,
            ARRAYFREE(arr),
            "Add middle 1: expected [10,99,20,30] len=4, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayAdd multiple in middle", ++subnum);
    {
        Array *arr = IarrayCreate(2, ARRAY_FILLTYPE_ASC_SERIES); // [0,1]
        arr->iv[0] = 5; arr->iv[1] = 15;

        arr = arrayAdd(arr, 1, 2, ARRAY_FILLTYPE_ZERO);
        arr->iv[1] = 7;
        arr->iv[2] = 10;
        test_validatefree(
            arr != NULL && arraylen(arr) == 4 &&
            arr->iv[0] == 5 && arr->iv[1] == 7 && arr->iv[2] == 10 && arr->iv[3] == 15,
            ARRAYFREE(arr),
            "Add middle 2: expected [5,7,10,15] len=4, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayAdd at beginning", ++subnum);
    {
        Array *arr = IarrayCreate(2, ARRAY_FILLTYPE_ASC_SERIES); // [0,1]
        arr->iv[0] = 20; arr->iv[1] = 30;

        arr = arrayAdd(arr, 0, 1, ARRAY_FILLTYPE_ZERO);
        arr->iv[0] = 10;
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 10 && arr->iv[1] == 20 && arr->iv[2] == 30,
            ARRAYFREE(arr),
            "Add start: expected [10,20,30] len=3, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayAdd at end", ++subnum);
    {
        Array *arr = IarrayCreate(2, ARRAY_FILLTYPE_ASC_SERIES); // [0,1]
        arr->iv[0] = 1; arr->iv[1] = 2;

        arr = arrayAdd(arr, 2, 1, ARRAY_FILLTYPE_ZERO);  // from == len
        arr->iv[2] = 3;
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 1 && arr->iv[1] == 2 && arr->iv[2] == 3,
            ARRAYFREE(arr),
            "Add end: expected [1,2,3] len=3, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayAdd zero cnt (no-op)", ++subnum);
    {
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_ASC_SERIES); // [0,1,2]
        arr->iv[0] = 1; arr->iv[1] = 2; arr->iv[2] = 3;

        arr = arrayAdd(arr, 1, 0, ARRAY_FILLTYPE_ZERO);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 1 && arr->iv[1] == 2 && arr->iv[2] == 3,
            ARRAYFREE(arr),
            "Add zero: array unchanged, len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayAdd from out of bounds (no-op)", ++subnum);
    {
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_ASC_SERIES); // [0,1,2]
        arr->iv[0] = 1; arr->iv[1] = 2; arr->iv[2] = 3;

        arr = arrayAdd(arr, 5, 1, ARRAY_FILLTYPE_ZERO);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 1 && arr->iv[1] == 2 && arr->iv[2] == 3,
            ARRAYFREE(arr),
            "Add out of bounds: array unchanged, len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: arrayAdd to empty array", ++subnum);
    {
        // Создаем массив с нулевой длиной
        Array *arr = IarrayCreate(0, ARRAY_FILLTYPE_ASC_SERIES); 

        arr = arrayAdd(arr, 0, 3, ARRAY_FILLTYPE_ZERO);
        arr->iv[0] = 10; arr->iv[1] = 20; arr->iv[2] = 30;

        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            arr->iv[0] == 10 && arr->iv[1] == 20 && arr->iv[2] == 30,
            ARRAYFREE(arr),
            "Add to empty: expected [10,20,30] len=3, got len=%zu", arraylen(arr)
        );
        ARRAYFREE(arr);
    }

    /* ========== V64 arrayAdd (STR type) ========== */
    test_sub("subtest %d: V64(STR) arrayAdd one element middle", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("a");
        arr->v64[1] = value64_createstr("b");
        arr->v64[2] = value64_createstr("c");
        arr->len = 3;

        arr = arrayAdd(arr, 1, 1, ARRAY_FILLTYPE_ZERO);
        // ZERO для STR заполнит пустой строкой ""
        test_validatefree(
            arr != NULL && arraylen(arr) == 4 &&
            strcmp(value64_str(arr->v64[0]), "a") == 0 &&
            strcmp(value64_str(arr->v64[1]), "") == 0 &&   // заполнено ZERO
            strcmp(value64_str(arr->v64[2]), "b") == 0 &&
            strcmp(value64_str(arr->v64[3]), "c") == 0,
            ARRAYFREE(arr),
            "V64(STR) Add middle 1: expected [\"a\",\"\",\"b\",\"c\"]"
        );
        // перезапишем
        value64free(arr->v64[1], VALUE64_STR);
        arr->v64[1] = value64_createstr("X");
        test_validatefree(
            strcmp(value64_str(arr->v64[1]), "X") == 0,
            ARRAYFREE(arr),
            "V64(STR) Add middle 1: after set idx 1 = X"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(STR) arrayAdd multiple middle", ++subnum);
    {
        Array *arr = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("first");
        arr->v64[1] = value64_createstr("last");
        arr->len = 2;

        arr = arrayAdd(arr, 1, 2, ARRAY_FILLTYPE_ZERO);
        // после сдвига: [0]="first", [1]="", [2]="", [3]="last"
        value64free(arr->v64[1], VALUE64_STR);
        value64free(arr->v64[2], VALUE64_STR);
        arr->v64[1] = value64_createstr("middle1");
        arr->v64[2] = value64_createstr("middle2");

        test_validatefree(
            arr != NULL && arraylen(arr) == 4 &&
            strcmp(value64_str(arr->v64[0]), "first") == 0 &&
            strcmp(value64_str(arr->v64[1]), "middle1") == 0 &&
            strcmp(value64_str(arr->v64[2]), "middle2") == 0 &&
            strcmp(value64_str(arr->v64[3]), "last") == 0,
            ARRAYFREE(arr),
            "V64(STR) Add middle 2: expected [first,middle1,middle2,last]"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(STR) arrayAdd at beginning", ++subnum);
    {
        Array *arr = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("world");
        arr->v64[1] = value64_createstr("!");
        arr->len = 2;

        arr = arrayAdd(arr, 0, 1, ARRAY_FILLTYPE_ZERO);
        value64free(arr->v64[0], VALUE64_STR);
        arr->v64[0] = value64_createstr("Hello");
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            strcmp(value64_str(arr->v64[0]), "Hello") == 0 &&
            strcmp(value64_str(arr->v64[1]), "world") == 0 &&
            strcmp(value64_str(arr->v64[2]), "!") == 0,
            ARRAYFREE(arr),
            "V64(STR) Add start: expected [Hello,world,!]"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(STR) arrayAdd at end", ++subnum);
    {
        Array *arr = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("x");
        arr->v64[1] = value64_createstr("y");
        arr->len = 2;

        arr = arrayAdd(arr, 2, 1, ARRAY_FILLTYPE_ZERO);
        value64free(arr->v64[2], VALUE64_STR);
        arr->v64[2] = value64_createstr("z");
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            strcmp(value64_str(arr->v64[0]), "x") == 0 &&
            strcmp(value64_str(arr->v64[1]), "y") == 0 &&
            strcmp(value64_str(arr->v64[2]), "z") == 0,
            ARRAYFREE(arr),
            "V64(STR) Add end: expected [x,y,z]"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(STR) arrayAdd zero cnt (no-op)", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("1");
        arr->v64[1] = value64_createstr("2");
        arr->v64[2] = value64_createstr("3");
        arr->len = 3;

        arr = arrayAdd(arr, 1, 0, ARRAY_FILLTYPE_ZERO);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            strcmp(value64_str(arr->v64[0]), "1") == 0 &&
            strcmp(value64_str(arr->v64[1]), "2") == 0 &&
            strcmp(value64_str(arr->v64[2]), "3") == 0,
            ARRAYFREE(arr),
            "V64(STR) Add zero: array unchanged"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(STR) arrayAdd out of bounds (no-op)", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        arr->v64[0] = value64_createstr("a");
        arr->v64[1] = value64_createstr("b");
        arr->v64[2] = value64_createstr("c");
        arr->len = 3;

        arr = arrayAdd(arr, 5, 1, ARRAY_FILLTYPE_ZERO);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            strcmp(value64_str(arr->v64[0]), "a") == 0 &&
            strcmp(value64_str(arr->v64[1]), "b") == 0 &&
            strcmp(value64_str(arr->v64[2]), "c") == 0,
            ARRAYFREE(arr),
            "V64(STR) Add out of bounds: array unchanged"
        );
        ARRAYFREE(arr);
    }

    /* ========== V64(FS) arrayAdd ========== */
    test_sub("subtest %d: V64(FS) arrayAdd one element middle", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("a");
        arr->v64[1] = value64_createfs_asstr("b");
        arr->v64[2] = value64_createfs_asstr("c");
        arr->len = 3;

        arr = arrayAdd(arr, 1, 1, ARRAY_FILLTYPE_ZERO);
        // ZERO для FS создаёт пустую строку ""
        test_validatefree(
            arr != NULL && arraylen(arr) == 4 &&
            strcmp(fs_str(value64_fs(arr->v64[0])), "a") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[1])), "") == 0 &&  // ZERO
            strcmp(fs_str(value64_fs(arr->v64[2])), "b") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[3])), "c") == 0,
            ARRAYFREE(arr),
            "V64(FS) Add middle 1: expected [\"a\",\"\",\"b\",\"c\"]"
        );
        // заменяем пустую строку на "X"
        value64free(arr->v64[1], VALUE64_FS);
        arr->v64[1] = value64_createfs_asstr("X");
        test_validatefree(
            strcmp(fs_str(value64_fs(arr->v64[1])), "X") == 0,
            ARRAYFREE(arr),
            "V64(FS) Add middle 1: after set idx 1 = X"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(FS) arrayAdd multiple middle", ++subnum);
    {
        Array *arr = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("first");
        arr->v64[1] = value64_createfs_asstr("last");
        arr->len = 2;

        arr = arrayAdd(arr, 1, 2, ARRAY_FILLTYPE_ZERO);
        // после сдвига: [0]="first", [1]="", [2]="", [3]="last"
        value64free(arr->v64[1], VALUE64_FS);
        value64free(arr->v64[2], VALUE64_FS);
        arr->v64[1] = value64_createfs_asstr("middle1");
        arr->v64[2] = value64_createfs_asstr("middle2");

        test_validatefree(
            arr != NULL && arraylen(arr) == 4 &&
            strcmp(fs_str(value64_fs(arr->v64[0])), "first") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[1])), "middle1") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[2])), "middle2") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[3])), "last") == 0,
            ARRAYFREE(arr),
            "V64(FS) Add middle 2: expected [first,middle1,middle2,last]"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(FS) arrayAdd at beginning", ++subnum);
    {
        Array *arr = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("world");
        arr->v64[1] = value64_createfs_asstr("!");
        arr->len = 2;

        arr = arrayAdd(arr, 0, 1, ARRAY_FILLTYPE_ZERO);
        value64free(arr->v64[0], VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("Hello");
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            strcmp(fs_str(value64_fs(arr->v64[0])), "Hello") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[1])), "world") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[2])), "!") == 0,
            ARRAYFREE(arr),
            "V64(FS) Add start: expected [Hello,world,!]"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(FS) arrayAdd at end", ++subnum);
    {
        Array *arr = V64ArrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("x");
        arr->v64[1] = value64_createfs_asstr("y");
        arr->len = 2;

        arr = arrayAdd(arr, 2, 1, ARRAY_FILLTYPE_ZERO);
        value64free(arr->v64[2], VALUE64_FS);
        arr->v64[2] = value64_createfs_asstr("z");
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            strcmp(fs_str(value64_fs(arr->v64[0])), "x") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[1])), "y") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[2])), "z") == 0,
            ARRAYFREE(arr),
            "V64(FS) Add end: expected [x,y,z]"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(FS) arrayAdd zero cnt (no-op)", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("1");
        arr->v64[1] = value64_createfs_asstr("2");
        arr->v64[2] = value64_createfs_asstr("3");
        arr->len = 3;

        arr = arrayAdd(arr, 1, 0, ARRAY_FILLTYPE_ZERO);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            strcmp(fs_str(value64_fs(arr->v64[0])), "1") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[1])), "2") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[2])), "3") == 0,
            ARRAYFREE(arr),
            "V64(FS) Add zero: array unchanged"
        );
        ARRAYFREE(arr);
    }

    test_sub("subtest %d: V64(FS) arrayAdd out of bounds (no-op)", ++subnum);
    {
        Array *arr = V64ArrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        arr->v64[0] = value64_createfs_asstr("a");
        arr->v64[1] = value64_createfs_asstr("b");
        arr->v64[2] = value64_createfs_asstr("c");
        arr->len = 3;

        arr = arrayAdd(arr, 5, 1, ARRAY_FILLTYPE_ZERO);
        test_validatefree(
            arr != NULL && arraylen(arr) == 3 &&
            strcmp(fs_str(value64_fs(arr->v64[0])), "a") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[1])), "b") == 0 &&
            strcmp(fs_str(value64_fs(arr->v64[2])), "c") == 0,
            ARRAYFREE(arr),
            "V64(FS) Add out of bounds: array unchanged"
        );
        ARRAYFREE(arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayFillRangeZERO with V64 generator -------------------------
static TestStatus
tf26_array_v64_zero_fill_all(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT */
    test_sub("subtest %d: V64 ZERO fill for INT", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_INT);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(value64_int(arr->v64[i]) == 0,
                              ARRAYFREE(arr), "INT[%zu] must be 0", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 2. LONG */
    test_sub("subtest %d: V64 ZERO fill for LONG", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_LONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(value64_long(arr->v64[i]) == 0L,
                              ARRAYFREE(arr), "LONG[%zu] must be 0", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 3. ULONG */
    test_sub("subtest %d: V64 ZERO fill for ULONG", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_ULONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(value64_ulong(arr->v64[i]) == 0UL,
                              ARRAYFREE(arr), "ULONG[%zu] must be 0", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 4. DBL */
    test_sub("subtest %d: V64 ZERO fill for DBL", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_DBL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(fabs(value64_dbl(arr->v64[i]) - 0.0) < 1e-9,
                              ARRAYFREE(arr), "DBL[%zu] must be 0.0", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 5. CHR */
    test_sub("subtest %d: V64 ZERO fill for CHR", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_CHR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(value64_char(arr->v64[i]) == '\0',
                              ARRAYFREE(arr), "CHR[%zu] must be \\0", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 6. BOOL */
    test_sub("subtest %d: V64 ZERO fill for BOOL", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_BOOL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(value64_bool(arr->v64[i]) == false,
                              ARRAYFREE(arr), "BOOL[%zu] must be false", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 7. PTR (NULL) */
    test_sub("subtest %d: V64 ZERO fill for PTR", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_PTR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(value64_ptr(arr->v64[i]) == NULL,
                              ARRAYFREE(arr), "PTR[%zu] must be NULL", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 8. FS (пустой fs) */
    test_sub("subtest %d: V64 ZERO fill for FS", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_FS);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(
                value64_fs(arr->v64[i]) != NULL && fs_len(value64_fs(arr->v64[i])) == 0,
                ARRAYFREE(arr), "FS[%zu] must be empty", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 9. STR (пустая строка) */
    test_sub("subtest %d: V64 ZERO fill for STR", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ZERO, VALUE64_STR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < 5; i++) {
            test_validatefree(
                value64_str(arr->v64[i]) != NULL && strcmp(value64_str(arr->v64[i]), "") == 0,
                ARRAYFREE(arr), "STR[%zu] must be empty string", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayFillRangeASCSERIES with V64 generator -------------------------
static TestStatus
tf27_array_v64_asc_series_fill_all(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: V64 ASC fill for INT", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES, VALUE64_INT);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");
        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_int(arr->v64[i]) == (int) i,
                              ARRAYFREE(arr), "INT[%zu] must be %zu, got %d", i, i, value64_int(arr->v64[i]));
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: V64 ASC fill for LONG", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES, VALUE64_LONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");
        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_long(arr->v64[i]) == (long) i,
                              ARRAYFREE(arr), "LONG[%zu] must be %zu", i, i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: V64 ASC fill for ULONG", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES, VALUE64_ULONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");
        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_ulong(arr->v64[i]) == (unsigned long)i,
                              ARRAYFREE(arr), "ULONG[%zu] must be %zu", i, i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: V64 ASC fill for DBL", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES, VALUE64_DBL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");
        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(fabs(value64_dbl(arr->v64[i]) - i) < 1e-9,
                              ARRAYFREE(arr), "DBL[%zu] must be %zu", i, i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: V64 ASC fill for CHR", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES, VALUE64_CHR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");
        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_char(arr->v64[i]) == (char)i,
                              ARRAYFREE(arr), "CHR[%zu] must be %zu", i, i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: V64 ASC fill for BOOL", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES, VALUE64_BOOL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");
        for (size_t i = 0; i < arraylen(arr); i++) {
            bool expected = (i % 2 != 0);
            test_validatefree(value64_bool(arr->v64[i]) == expected,
                              ARRAYFREE(arr), "BOOL[%zu] must be %s", i, expected ? "true" : "false");
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: V64 ASC fill for STR", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES, VALUE64_STR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");
        for (size_t i = 0; i < arraylen(arr); i++) {
            char expected[16];
            snprintf(expected, sizeof(expected), "%zu", i);
            test_validatefree(strcmp(value64_str(arr->v64[i]), expected) == 0,
                              ARRAYFREE(arr), "STR[%zu] must be '%s'", i, expected);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: V64 ASC fill for FS", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES, VALUE64_FS);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");
        for (size_t i = 0; i < arraylen(arr); i++) {
            char expected[16];
            snprintf(expected, sizeof(expected), "%zu", i);
            test_validatefree(fs_cmpstr(value64_fs(arr->v64[i]), expected) == 0,
                              ARRAYFREE(arr), "FS[%zu] must be '%s'", i, expected);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayFillRangeASC with V64 generator (random increase) -------------------------
static TestStatus
tf28_array_v64_asc_fill_all_random(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT */
    test_sub("subtest %d: V64 ASC random fill for INT", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_ASC, VALUE64_INT);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            int diff = value64_int(arr->v64[i]) - value64_int(arr->v64[i-1]);
            test_validatefree(diff >= 1 && diff <= g_array_acs_rndinc,
                              ARRAYFREE(arr),
                              "INT difference at %zu must be 1..%d, got %d", 
                              i, g_array_acs_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 2. LONG */
    test_sub("subtest %d: V64 ASC random fill for LONG", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_ASC, VALUE64_LONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            long diff = value64_long(arr->v64[i]) - value64_long(arr->v64[i-1]);
            test_validatefree(diff >= 1 && diff <= g_array_acs_rndinc,
                              ARRAYFREE(arr),
                              "LONG difference at %zu must be 1..%d, got %ld", 
                              i, g_array_acs_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 3. ULONG */
    test_sub("subtest %d: V64 ASC random fill for ULONG", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_ASC, VALUE64_ULONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            unsigned long diff = value64_ulong(arr->v64[i]) - value64_ulong(arr->v64[i-1]);
            test_validatefree(diff >= 1 && diff <= g_array_acs_rndinc,
                              ARRAYFREE(arr),
                              "ULONG difference at %zu must be 1..%d, got %lu", 
                              i, g_array_acs_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 4. DBL */
    test_sub("subtest %d: V64 ASC random fill for DBL", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_ASC, VALUE64_DBL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            double diff = value64_dbl(arr->v64[i]) - value64_dbl(arr->v64[i-1]);
            test_validatefree(diff >= 1.0 && diff <= g_array_acs_rndinc + 0.0,
                              ARRAYFREE(arr),
                              "DBL difference at %zu must be 1.0..%g, got %f", 
                              i, g_array_acs_rndinc + 0.0, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 5. CHR */
    test_sub("subtest %d: V64 ASC random fill for CHR", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_ASC, VALUE64_CHR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            int diff = (unsigned char)value64_char(arr->v64[i]) - (unsigned char)value64_char(arr->v64[i-1]);
            test_validatefree(diff >= 1 && diff <= g_array_acs_rndinc,
                              ARRAYFREE(arr),
                              "CHR difference at %zu must be 1..%d, got %d", 
                              i, g_array_acs_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 6. BOOL – просто проверяем, что созданы допустимые значения */
    test_sub("subtest %d: V64 ASC random fill for BOOL", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_ASC, VALUE64_BOOL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_bool(arr->v64[i]) == true || value64_bool(arr->v64[i]) == false,
                              ARRAYFREE(arr),
                              "BOOL[%zu] must be true or false", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 7. STR */
    test_sub("subtest %d: V64 ASC random fill for STR", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_ASC, VALUE64_STR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            int prev, curr;
            test_validatefree(sscanf(value64_str(arr->v64[i-1]), "%d", &prev) == 1 &&
                              sscanf(value64_str(arr->v64[i]), "%d", &curr) == 1,
                              ARRAYFREE(arr),
                              "STR[%zu] parse error", i);
            int diff = curr - prev;
            test_validatefree(diff >= 1 && diff <= g_array_acs_rndinc,
                              ARRAYFREE(arr),
                              "STR difference at %zu must be 1..%d, got %d", 
                              i, g_array_acs_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 8. FS */
    test_sub("subtest %d: V64 ASC random fill for FS", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_ASC, VALUE64_FS);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            int prev, curr;
            test_validatefree(sscanf(fs_str(value64_fs(arr->v64[i-1])), "%d", &prev) == 1 &&
                              sscanf(fs_str(value64_fs(arr->v64[i])), "%d", &curr) == 1,
                              ARRAYFREE(arr),
                              "FS[%zu] parse error", i);
            int diff = curr - prev;
            test_validatefree(diff >= 1 && diff <= g_array_acs_rndinc,
                              ARRAYFREE(arr),
                              "FS difference at %zu must be 1..%d, got %d", 
                              i, g_array_acs_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayFillRangeDESC with V64 generator (random decrease) -------------------------
static TestStatus
tf29_array_v64_desc_fill_all_random(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT */
    test_sub("subtest %d: V64 DESC random fill for INT", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_DESC, VALUE64_INT);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            int diff = value64_int(arr->v64[i-1]) - value64_int(arr->v64[i]);
            test_validatefree(diff >= 1 && diff <= g_array_desc_rndinc,
                              ARRAYFREE(arr),
                              "INT difference at %zu must be 1..%d, got %d", 
                              i, g_array_desc_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 2. LONG */
    test_sub("subtest %d: V64 DESC random fill for LONG", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_DESC, VALUE64_LONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            long diff = value64_long(arr->v64[i-1]) - value64_long(arr->v64[i]);
            test_validatefree(diff >= 1 && diff <= g_array_desc_rndinc,
                              ARRAYFREE(arr),
                              "LONG difference at %zu must be 1..%d, got %ld", 
                              i, g_array_desc_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 3. ULONG */
    test_sub("subtest %d: V64 DESC random fill for ULONG", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_DESC, VALUE64_ULONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            unsigned long diff = value64_ulong(arr->v64[i-1]) - value64_ulong(arr->v64[i]);
            test_validatefree(diff >= 1 && diff <= g_array_desc_rndinc,
                              ARRAYFREE(arr),
                              "ULONG difference at %d must be 1..%zu, got %lu", 
                              g_array_desc_rndinc, i, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 4. DBL */
    test_sub("subtest %d: V64 DESC random fill for DBL", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_DESC, VALUE64_DBL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            double diff = value64_dbl(arr->v64[i-1]) - value64_dbl(arr->v64[i]);
            test_validatefree(diff >= 1.0 && diff <= g_array_desc_rndinc + 0.0,
                              ARRAYFREE(arr),
                              "DBL difference at %zu must be 1.0..%g, got %f", 
                              i, g_array_desc_rndinc + 0.0, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 5. CHR */
    test_sub("subtest %d: V64 DESC random fill for CHR", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_DESC, VALUE64_CHR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            int diff = (unsigned char)value64_char(arr->v64[i-1]) - (unsigned char)value64_char(arr->v64[i]);
            test_validatefree(diff >= 1 && diff <= g_array_desc_rndinc,
                              ARRAYFREE(arr),
                              "CHR difference at %zu must be 1..%d, got %d", 
                              i, g_array_desc_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 6. BOOL – просто проверяем, что созданы допустимые значения */
    test_sub("subtest %d: V64 DESC random fill for BOOL", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_DESC, VALUE64_BOOL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_bool(arr->v64[i]) == true || value64_bool(arr->v64[i]) == false,
                              ARRAYFREE(arr),
                              "BOOL[%zu] must be true or false", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 7. STR */
    test_sub("subtest %d: V64 DESC random fill for STR", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_DESC, VALUE64_STR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            int prev, curr;
            test_validatefree(sscanf(value64_str(arr->v64[i-1]), "%d", &prev) == 1 &&
                              sscanf(value64_str(arr->v64[i]), "%d", &curr) == 1,
                              ARRAYFREE(arr),
                              "STR[%zu] parse error", i);
            int diff = prev - curr;
            test_validatefree(diff >= 1 && diff <= g_array_desc_rndinc,
                              ARRAYFREE(arr),
                              "STR difference at %zu must be 1..%d, got %d", 
                              i, g_array_desc_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 8. FS */
    test_sub("subtest %d: V64 DESC random fill for FS", ++subnum);
    {
        Array *arr = V64ArrayCreate(8, ARRAY_FILLTYPE_DESC, VALUE64_FS);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 1; i < arraylen(arr); i++) {
            int prev, curr;
            test_validatefree(sscanf(fs_str(value64_fs(arr->v64[i-1])), "%d", &prev) == 1 &&
                              sscanf(fs_str(value64_fs(arr->v64[i])), "%d", &curr) == 1,
                              ARRAYFREE(arr),
                              "FS[%zu] parse error", i);
            int diff = prev - curr;
            test_validatefree(diff >= 1 && diff <= g_array_desc_rndinc,
                              ARRAYFREE(arr),
                              "FS difference at %zu must be 1..%d, got %d", 
                              i, g_array_desc_rndinc, diff);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayFillRangeDESCSERIES with V64 generator -------------------------
static TestStatus
tf30_array_v64_desc_series_fill_all(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT */
    test_sub("subtest %d: V64 DESC_SERIES fill for INT", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_DESC_SERIES, VALUE64_INT);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            int expected = N - 1 - i;
            test_validatefree(value64_int(arr->v64[i]) == expected,
                              ARRAYFREE(arr),
                              "INT[%zu] must be %d, got %d", i, expected, value64_int(arr->v64[i]));
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 2. LONG */
    test_sub("subtest %d: V64 DESC_SERIES fill for LONG", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_DESC_SERIES, VALUE64_LONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            long expected = N - 1 - i;
            test_validatefree(value64_long(arr->v64[i]) == expected,
                              ARRAYFREE(arr),
                              "LONG[%zu] must be %ld, got %ld", i, expected, value64_long(arr->v64[i]));
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 3. ULONG */
    test_sub("subtest %d: V64 DESC_SERIES fill for ULONG", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_DESC_SERIES, VALUE64_ULONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            unsigned long expected = N - 1 - i;
            test_validatefree(value64_ulong(arr->v64[i]) == expected,
                              ARRAYFREE(arr),
                              "ULONG[%zu] must be %lu, got %lu", i, expected, value64_ulong(arr->v64[i]));
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 4. DBL */
    test_sub("subtest %d: V64 DESC_SERIES fill for DBL", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_DESC_SERIES, VALUE64_DBL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            double expected = N - 1 - i;
            test_validatefree(fabs(value64_dbl(arr->v64[i]) - expected) < 1e-9,
                              ARRAYFREE(arr),
                              "DBL[%zu] must be %f, got %f", i, expected, value64_dbl(arr->v64[i]));
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 5. CHR */
    test_sub("subtest %d: V64 DESC_SERIES fill for CHR", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_DESC_SERIES, VALUE64_CHR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        char expected =  N - 1;   // 7, 6, ..., 0
        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_char(arr->v64[i]) == expected,
                              ARRAYFREE(arr),
                              "CHR[%zu] must be '%c'/%d, got '%c'/%d", 
                              i, expected, expected, value64_char(arr->v64[i]), value64_char(arr->v64[i]));
            expected--;
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 6. BOOL */
    test_sub("subtest %d: V64 DESC_SERIES fill for BOOL", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_DESC_SERIES, VALUE64_BOOL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_bool(arr->v64[i]) == true || value64_bool(arr->v64[i]) == false,
                              ARRAYFREE(arr),
                              "BOOL[%zu] must be true or false", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 7. STR */
    test_sub("subtest %d: V64 DESC_SERIES fill for STR", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_DESC_SERIES, VALUE64_STR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            char expected[16];
            snprintf(expected, sizeof(expected), "%zu", N - 1 - i);
            test_validatefree(strcmp(value64_str(arr->v64[i]), expected) == 0,
                              ARRAYFREE(arr),
                              "STR[%zu] must be '%s', got '%s'", i, expected, value64_str(arr->v64[i]));
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 8. FS */
    test_sub("subtest %d: V64 DESC_SERIES fill for FS", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_DESC_SERIES, VALUE64_FS);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        for (size_t i = 0; i < arraylen(arr); i++) {
            char expected[16];
            snprintf(expected, sizeof(expected), "%zu", N - 1 - i);
            test_validatefree(fs_cmpstr(value64_fs(arr->v64[i]), expected) == 0,
                              ARRAYFREE(arr),
                              "FS[%zu] must be '%s', got '%s'", i, expected, fs_str(value64_fs(arr->v64[i])));
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayFillRangeRND with V64 generator -------------------------
static TestStatus
tf31_array_v64_rnd_fill_all(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT */
    test_sub("subtest %d: V64 RND fill for INT", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_RND, VALUE64_INT);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        int prev = value64_int(arr->v64[0]);
        bool all_same = true;
        for (size_t i = 0; i < arraylen(arr); i++) {
            int val = value64_int(arr->v64[i]);
            test_validatefree(val >= 0 && val <= 10 * N,
                              ARRAYFREE(arr),
                              "INT[%zu] out of range: %d", i, val);
            if (i > 0 && val != prev) all_same = false;
            prev = val;
        }
        test_validatefree(!all_same, ARRAYFREE(arr), "INT values should not be all identical");
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 2. LONG */
    test_sub("subtest %d: V64 RND fill for LONG", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_RND, VALUE64_LONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        long prev = value64_long(arr->v64[0]);
        bool all_same = true;
        for (size_t i = 0; i < arraylen(arr); i++) {
            long val = value64_long(arr->v64[i]);
            test_validatefree(val >= 0 && val <= 10 * N,
                              ARRAYFREE(arr),
                              "LONG[%zu] out of range: %ld", i, val);
            if (i > 0 && val != prev) all_same = false;
            prev = val;
        }
        test_validatefree(!all_same, ARRAYFREE(arr), "LONG values should not be all identical");
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 3. ULONG */
    test_sub("subtest %d: V64 RND fill for ULONG", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_RND, VALUE64_ULONG);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        unsigned long prev = value64_ulong(arr->v64[0]);
        bool all_same = true;
        for (size_t i = 0; i < arraylen(arr); i++) {
            unsigned long val = value64_ulong(arr->v64[i]);
            test_validatefree(val >= 0 && val <= 10UL * N,
                              ARRAYFREE(arr),
                              "ULONG[%zu] out of range: %lu", i, val);
            if (i > 0 && val != prev) all_same = false;
            prev = val;
        }
        test_validatefree(!all_same, ARRAYFREE(arr), "ULONG values should not be all identical");
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 4. DBL */
    test_sub("subtest %d: V64 RND fill for DBL", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_RND, VALUE64_DBL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        double prev = value64_dbl(arr->v64[0]);
        bool all_same = true;
        for (size_t i = 0; i < arraylen(arr); i++) {
            double val = value64_dbl(arr->v64[i]);
            test_validatefree(val >= 0.0 && val <= 10.0 * N,
                              ARRAYFREE(arr),
                              "DBL[%zu] out of range: %f", i, val);
            if (i > 0 && fabs(val - prev) > 1e-9) all_same = false;
            prev = val;
        }
        test_validatefree(!all_same, ARRAYFREE(arr), "DBL values should not be all identical");
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 5. CHR */
    test_sub("subtest %d: V64 RND fill for CHR", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_RND, VALUE64_CHR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        unsigned char prev = (unsigned char)value64_char(arr->v64[0]);
        bool all_same = true;
        for (size_t i = 0; i < arraylen(arr); i++) {
            unsigned char val = (unsigned char)value64_char(arr->v64[i]);
            test_validatefree(val <= 10 * N,
                              ARRAYFREE(arr),
                              "CHR[%zu] out of range: %u", i, val);
            if (i > 0 && val != prev) all_same = false;
            prev = val;
        }
        test_validatefree(!all_same, ARRAYFREE(arr), "CHR values should not be all identical");
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 6. BOOL */
    test_sub("subtest %d: V64 RND fill for BOOL", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_RND, VALUE64_BOOL);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        bool saw_true = false, saw_false = false;
        for (size_t i = 0; i < arraylen(arr); i++) {
            bool b = value64_bool(arr->v64[i]);
            test_validatefree(b == true || b == false,
                              ARRAYFREE(arr),
                              "BOOL[%zu] must be true or false", i);
            if (b) saw_true = true;
            else saw_false = true;
        }
        test_validatefree(saw_true && saw_false,
                          ARRAYFREE(arr),
                          "BOOL values should include both true and false");
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 7. STR */
    test_sub("subtest %d: V64 RND fill for STR", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_RND, VALUE64_STR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        int prev = -1;
        bool all_same = true;
        for (size_t i = 0; i < arraylen(arr); i++) {
            const char *s = value64_str(arr->v64[i]);
            test_validatefree(s != NULL && s[0] != '\0',
                              ARRAYFREE(arr),
                              "STR[%zu] must be non-empty", i);
            int num;
            test_validatefree(sscanf(s, "%d", &num) == 1,
                              ARRAYFREE(arr),
                              "STR[%zu] must be numeric, got '%s'", i, s);
            test_validatefree(num >= 0 && num <= 10 * N,
                              ARRAYFREE(arr),
                              "STR[%zu] out of range: %d", i, num);
            if (i > 0 && num != prev) all_same = false;
            prev = num;
        }
        test_validatefree(!all_same, ARRAYFREE(arr), "STR values should not be all identical");
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 8. FS */
    test_sub("subtest %d: V64 RND fill for FS", ++subnum);
    {
        const int N = 8;
        Array *arr = V64ArrayCreate(N, ARRAY_FILLTYPE_RND, VALUE64_FS);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        int prev = -1;
        bool all_same = true;
        for (size_t i = 0; i < arraylen(arr); i++) {
            const char *s = fs_str(value64_fs(arr->v64[i]));
            test_validatefree(s != NULL && s[0] != '\0',
                              ARRAYFREE(arr),
                              "FS[%zu] must be non-empty", i);
            int num;
            test_validatefree(sscanf(s, "%d", &num) == 1,
                              ARRAYFREE(arr),
                              "FS[%zu] must be numeric, got '%s'", i, s);
            test_validatefree(num >= 0 && num <= 10 * N,
                              ARRAYFREE(arr),
                              "FS[%zu] out of range: %d", i, num);
            if (i > 0 && num != prev) all_same = false;
            prev = num;
        }
        test_validatefree(!all_same, ARRAYFREE(arr), "FS values should not be all identical");
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arrayFillRangeSAFEEMPTY -------------------------
static TestStatus
tf32_array_safe_empty(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. STR array: все элементы должны быть NULL-строками */
    test_sub("subtest %d: SAFE_EMPTY for STR", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_STR);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        long cnt = arrayFillRangeSAFEEMPTY(arr, 0, arraylen(arr));
        test_validatefree(
            cnt == (long) arraylen(arr), 
            ARRAYFREE(arr),
            "expected %zu filled, got %ld", arraylen(arr), cnt
        );

        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(value64_str(arr->v64[i]) == NULL,
                              ARRAYFREE(arr),
                              "STR[%zu] must be NULL", i);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 2. FS array: все элементы должны быть пустыми fs (не NULL) */
    test_sub("subtest %d: SAFE_EMPTY for FS", ++subnum);
    {
        Array *arr = V64ArrayCreate(5, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_FS);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "V64ArrayCreate failed");

        long cnt = arrayFillRangeSAFEEMPTY(arr, 0, arraylen(arr));
        test_validatefree(cnt == (long) arraylen(arr), ARRAYFREE(arr),
                          "expected %zu filled, got %ld", arraylen(arr), cnt);

        for (size_t i = 0; i < arraylen(arr); i++) {
            fs *f = value64_fs(arr->v64[i]);

            test_validatefree(f != NULL && fs_isempty(f),
                            ARRAYFREE(arr),
                            "FS[%zu] must be non-NULL %p and == FS() ", i, f
            );
            if (f)
                test_validatefree(fs_len(f) == 0, ARRAYFREE(arr),
                                  "FS[%zu] must be empty, len=%zu", i, fs_len(f));
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    /* 3. Скалярный массив: SAFE_EMPTY ничего не делает, возвращает 0 */
    test_sub("subtest %d: SAFE_EMPTY for scalar", ++subnum);
    {
        Array *arr = IarrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES);
        test_validatefree(arr != NULL, ARRAYFREE(arr), "IarrayCreate failed");

        long cnt = arrayFillRangeSAFEEMPTY(arr, 0, arraylen(arr));
        test_validatefree(cnt == 0, ARRAYFREE(arr),
                          "expected 0, got %ld", cnt);

        // Значения должны остаться прежними (заполнены ASC_SERIES)
        for (size_t i = 0; i < arraylen(arr); i++) {
            test_validatefree(arr->iv[i] == (int) i,
                              ARRAYFREE(arr),
                              "INT[%zu] must be %zu, got %d", i, i, arr->iv[i]);
        }
        ARRAYFREE(arr);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// -------------------------------------------------------------------
int
main( /*int argc, char *argv[] */ )
{
    logsimpleinit("Start");

    testenginestd(
        TESTADD(tf1,                                    "Int/double creation/descr test")
      , TESTADD(tf2,                                    "Int/double filling test")
      , TESTADD(tf3,                                    "Shrink test")
      , TESTADD(tf4,                                    "Save/load int test")
      , TESTADD(tf5,                                    "Save/load dbl test")
      , TESTADD(tf6,                                    "Shuffle array(dbl/int) simple test")
      , TESTADD(tf7,                                    "Sort array(dbl/int) simple test")
      , TESTADD(tf8,                                    "arrayIncrease simple test")
      , TESTADD(tf9,                                    "PArray simple test")
      , TESTADD(tf10,                                   "Creation with ARRAY_(DE)ASC_SERIES simple test")
      , TESTADD(tf11,                                   "arrayFillRange simple test")
      , TESTADD(tf12,                                   "Array_foreach macro simple test")
      , TESTADD(tf13,                                   "Array_foreach_prod simple test")
      , TESTADD(tf_v64array_str_fs,                     "V64Array (STR / FS) simple test")
      , TESTADD(tf_v64array_shrink_increase,            "V64Array (STR / FS) shrink / increase simple test")
      , TESTADD(tf_v64array_sort,                       "V64Array (STR / FS) sorting simple test")
      , TESTADD(tf_v64arraySaveFile_load,               "V64Array STR/FS save/load simple test")
      , TESTADD(tf_array_bsearch,                       "arrayBsearch (INT / LONG / DBL / V64) simple test")
      , TESTADD(tf_carray_create_fill_free,             "CHAR create/fill/free simple test")
      , TESTADD(tf_carray_sort,                         "CHAR sorting simple test")
      , TESTADD(tf_array_bsearch_char,                  "CHAR arrayBsearch simple test")
      , TESTADD(tf_arraySaveFile_load_char,             "CHAR Array save/load simple test")
      , TESTADD(tf_array_eq_noteq,                      "arrayEq / arrayNoteq (all types, edge cases)")
      , TESTADD(tf_ArrayDel,                            "arrayDel simple test")
      , TESTADD(tf_ArrayAdd,                            "arrayAdd simple test")
      , TESTADD(tf26_array_v64_zero_fill_all,           "arrayFillRangeZERO with V64 generator")
      , TESTADD(tf27_array_v64_asc_series_fill_all,     "arrayFillRangeASCSERIES with V64 generator")
      , TESTADD(tf28_array_v64_asc_fill_all_random,     "arrayFillRangeASC_RND with V64 generator (random increase)")
      , TESTADD(tf29_array_v64_desc_fill_all_random,    "ArrayFillRange_DESC_RND with V64 generators (random decrease)")
      , TESTADD(tf30_array_v64_desc_series_fill_all,    "arrayFillRangeDESCSERIES with V64 generator")
      , TESTADD(tf31_array_v64_rnd_fill_all,            "arrayFillRangeRND with V64 generators all types")
      , TESTADD(tf32_array_safe_empty,                  "arrayFillRangeSAFEEMPTY simple test")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* ARRAYTESTING */
