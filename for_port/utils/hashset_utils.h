#ifndef _HASHSET_UTILS_H
#define _HASHSET_UTILS_H

// -------------------------------------------------------------------------------------------
// --------------------------- Public Hashset utils API --------------------------------------
// -------------------------------------------------------------------------------------------

// --------------- Includes -----------------------

#include "hashset.h"
#include "value64_params.h"
#include "value64_tarray.h"

// ----------- CONSTANTS AND GLOBALS ---------------

// ------------------- TYPES -----------------------

// filter types
typedef bool                (*valur64_predicate_t)(value64 v, /* value64 data */ const value64_tarray *params);
//typedef bool                (*hset_predicate2_t)(value64 v, value64 data1, value64 data2); // between etc...

typedef struct              hset_accum {
    value64         value;    // накопленное значение (сумма, максимум и т.п.)
    int             count;    // количество элементов, участвовавших в накоплении
    value64_type    typ;      // type of value(s)
    const char     *sep;      // separator for fsagg
} hset_accum;

typedef                     void (*hset_reduce_func)(hset_accum *acc, value64 v);

// filter types
typedef value64              (*hset_map_t)(value64 v, const value64_params_t* data);

// generic code
// just intersect with construct
extern hset                 hset_init_intersect(const hset *restrict se1, const hset *restrict se2);
// minus with construct with construct
extern hset                 hset_init_minus(const hset *restrict se1, const hset *restrict se2);
// simm diff with construct
extern hset                 hset_init_symmdiff(const hset *restrict a, const hset *restrict b);
// union with construct
extern hset                 hset_init_union(const hset *restrict a, const hset *restrict b);

extern bool                 hset_eq(const hset *restrict se1, const hset *restrict se2);

extern bool                 hset_noteq(const hset *restrict se1, const hset *restrict se2);

// check if all of se2 in se1 strictly or not
extern bool                 hset_subset_check(const hset *restrict se1, const hset *restrict se2, bool strict);
// check if all of se2 in se1
static inline bool          hset_in(const hset *restrict se1, const hset *restrict se2){
    return hset_subset_check(se1, se2, false);
}
// check if all of se2 in se1  but se2 not equal se1
static inline bool          hset_strictin(const hset *restrict se1, const hset *restrict se2){
    return hset_subset_check(se1, se2, true);
}
// if not exists
extern bool                 hset_notexists(const hset *restrict se1, const hset *restrict se2);
// if exists any of se2 in se1
extern bool                 hset_any(const hset *restrict se1, const hset *restrict se2);
// se1 -= se2 as SET
extern hset                *hset_minus(hset *restrict se1, const hset *restrict se2);
// se1 insersect= se2 as SET
extern hset                *hset_intersect(hset *restrict se1, const hset *restrict se2);
// se1 symmdiff= se2 as SET
extern hset                *hset_symmdiff(hset *restrict a, const hset *restrict b);
// union= as SET
extern hset                *hset_union(hset *restrict a, const hset *restrict b);

// ----------------------------------------- iterators --------------------------------------

extern void                 hset_const_foreach(const hset *se, hset_const_proc_t proc);
//extern void               hset_foreach(hset *se, hset_proc_t proc);
//extern void               hset_modify_foreach(hset *se, hset_modify_proc_t proc);

// ----------------------------------------- REDUCE -----------------------------------------

// TODO: api for extracting from hset_accum
static inline value64       hset_accum_get(hset_accum *c){
    return c->value;
}
#define                     hset_accum_int(ha) (*hset_accum_getint(ha) )
static inline int          *hset_accum_getint(hset_accum *c){
    return &c->value.ival;
}
#define                     hset_accum_long(ha) (*hset_accum_getlong(ha) )
static inline long         *hset_accum_getlong(hset_accum *c) {
    return &c->value.lval;
}
#define                     hset_accum_dbl(ha) (*hset_accum_getdbl(ha) )
static inline double       *hset_accum_getdbl(hset_accum *c) {
    return &c->value.dval;
}
#define                     hset_accum_fs(ha)  (*hset_accum_getfs(ha) )
static inline fs          **hset_accum_getfs(hset_accum *c) {
    return &c->value.fsval;
}

#define                     HSET_ACCUM(type, ...) \
(hset_accum) { \
    .value = LITERAL64_ZERO, \
    .count = 0, \
    .typ = (type), \
    ##__VA_ARGS__ \
}
#define                     HSET_ACCUM_INT_ZERO HSET_ACCUM(VALUE64_INT)

#define                     HSET_ACCUM_LNG_ZERO HSET_ACCUM(VALUE64_INT)

#define                     HSET_ACCUM_DBL_ZERO \
(hset_accum) { \
    .value = LITERAL64_DBL(0.0), \
    .count = 0, \
    .typ = VALUE64_DBL \
}
#define                     HSET_ACCUM_FS_ZERO \
(hset_accum) { \
    .value = (value64) \
        { .u64 = 0, \
          .fsval = fs_create() \
        }, \
    .count = 0, \
    .typ = VALUE64_FS \
}
#define                     HSET_ACCUM_FS_AGG(sepa) \
(hset_accum) { \
    .value     = (value64) \
        { .u64 = 0, \
          .fsval = fs_create() \
        }, \
    .count     = 0, \
    .typ       = VALUE64_FS, \
    .sep       = (sepa) \
}
// ---------------------------------------------------------------------------
//                              REDUCE ENGINE
// ---------------------------------------------------------------------------

/**
 * @brief Обычная свёртка без фильтрации (движок).
 * @param se   исходное множество
 * @param init начальное значение аккумулятора
 * @param func функция-редьюсер (вызывается для каждого элемента)
 * @return заполненный аккумулятор
 */
extern hset_accum hset_reduce(const hset *se, hset_accum init, hset_reduce_func func);

/**
 * @brief Свёртка с фильтрацией (движок).
 *
 * Если pred == NULL, обрабатываются все элементы (эквивалент hset_reduce).
 * Иначе обрабатываются только те элементы, для которых pred(v, data) == true.
 *
 * @param se   исходное множество
 * @param init начальное значение аккумулятора
 * @param func функция-редьюсер
 * @param pred предикат-фильтр (может быть NULL)
 * @param data данные для предиката
 * @return заполненный аккумулятор
 */
extern hset_accum hset_reduce_filtered(const hset *restrict se, hset_accum init,
                                       hset_reduce_func func,
                                       valur64_predicate_t pred, /* value64 data*/
                                       const value64_tarray *restrict params);

// ---------------------------------------------------------------------------
//                    TYPED REDUCE (without filtering)
// ---------------------------------------------------------------------------

/**
 * @brief Свёртка для VALUE64_INT (без фильтрации).
 * @param se   исходное множество
 * @param func функция-редьюсер
 * @return заполненный аккумулятор с типом VALUE64_INT
 */
static inline hset_accum hset_reduce_int(const hset *se, hset_reduce_func func) {
    return hset_reduce(se, HSET_ACCUM_INT_ZERO, func);
}

/**
 * @brief Свёртка для VALUE64_LONG (без фильтрации).
 * @param se   исходное множество
 * @param func функция-редьюсер
 * @return заполненный аккумулятор с типом VALUE64_LONG
 */
static inline hset_accum hset_reduce_lng(const hset *se, hset_reduce_func func) {
    return hset_reduce(se, HSET_ACCUM_LNG_ZERO, func);
}

/**
 * @brief Свёртка для VALUE64_DBL (без фильтрации).
 * @param se   исходное множество
 * @param func функция-редьюсер
 * @return заполненный аккумулятор с типом VALUE64_DBL
 */
static inline hset_accum hset_reduce_dbl(const hset *se, hset_reduce_func func) {
    return hset_reduce(se, HSET_ACCUM_DBL_ZERO, func);
}

/**
 * @brief Свёртка для VALUE64_FS (без фильтрации).
 * @param se   исходное множество
 * @param func функция-редьюсер
 * @return заполненный аккумулятор с типом VALUE64_FS
 */
static inline hset_accum hset_reduce_fs(const hset *se, hset_reduce_func func) {
    return hset_reduce(se, HSET_ACCUM_FS_ZERO, func);
}

/**
 * @brief Агрегирующая свёртка строк с разделителем (без фильтрации).
 * @param se  исходное множество
 * @param func функция-редьюсер (обычно hset_agg_fs)
 * @param sep строка-разделитель (может быть NULL)
 * @return заполненный аккумулятор с типом VALUE64_FS
 */
static inline hset_accum hset_reduce_fsagg(const hset *restrict se,
                                           hset_reduce_func func,
                                           const char *restrict sep) {
    return hset_reduce(se, HSET_ACCUM_FS_AGG(sep), func);
}

// ---------------------------------------------------------------------------
//                 FILTERED REDUCE (reduce + predicate)
// ---------------------------------------------------------------------------

/**
 * @brief Свёртка с фильтрацией для VALUE64_INT.
 * @param se   исходное множество
 * @param func функция-редьюсер
 * @param pred предикат-фильтр
 * @param data данные для предиката
 * @return заполненный аккумулятор с типом VALUE64_INT
 */
static inline hset_accum hset_filtereduce_int(const hset *restrict se,
                                              hset_reduce_func func,
                                              valur64_predicate_t pred,
                                              const value64_tarray *restrict params) {
    return hset_reduce_filtered(se, HSET_ACCUM_INT_ZERO, func, pred, params);
}

/**
 * @brief Свёртка с фильтрацией для VALUE64_LONG.
 * @param se   исходное множество
 * @param func функция-редьюсер
 * @param pred предикат-фильтр
 * @param data данные для предиката
 * @return заполненный аккумулятор с типом VALUE64_LONG
 */
static inline hset_accum hset_filtereduce_lng(const hset *restrict se,
                                              hset_reduce_func func,
                                              valur64_predicate_t pred,
                                              const value64_tarray *restrict params) {
    return hset_reduce_filtered(se, HSET_ACCUM_LNG_ZERO, func, pred, params);
}

/**
 * @brief Свёртка с фильтрацией для VALUE64_DBL.
 * @param se   исходное множество
 * @param func функция-редьюсер
 * @param pred предикат-фильтр
 * @param data данные для предиката
 * @return заполненный аккумулятор с типом VALUE64_DBL
 */
static inline hset_accum hset_filtereduce_dbl(const hset *restrict se,
                                              hset_reduce_func func,
                                              valur64_predicate_t pred,
                                              const value64_tarray *restrict params) {
    return hset_reduce_filtered(se, HSET_ACCUM_DBL_ZERO, func, pred, params);
}

/**
 * @brief Свёртка с фильтрацией для VALUE64_FS.
 * @param se   исходное множество
 * @param func функция-редьюсер
 * @param pred предикат-фильтр
 * @param data данные для предиката
 * @return заполненный аккумулятор с типом VALUE64_FS
 */
static inline hset_accum hset_filtereduce_fs(const hset *restrict se,
                                              hset_reduce_func func,
                                              valur64_predicate_t pred,
                                              const value64_tarray *restrict params) {
    return hset_reduce_filtered(se, HSET_ACCUM_FS_ZERO, func, pred, params);
}

/**
 * @brief Агрегирующая свёртка строк с фильтрацией и разделителем.
 * @param se  исходное множество
 * @param func функция-редьюсер (обычно hset_agg_fs)
 * @param sep строка-разделитель (может быть NULL)
 * @param pred предикат-фильтр
 * @param data данные для предиката
 * @return заполненный аккумулятор с типом VALUE64_FS
 */
static inline hset_accum hset_filtereduce_fsagg(const hset *restrict se,
                                                hset_reduce_func func,
                                                const char *restrict sep,
                                                valur64_predicate_t pred,
                                                const value64_tarray *restrict params) {
    return hset_reduce_filtered(se, HSET_ACCUM_FS_AGG(sep), func, pred, params);
}

// destructor
static inline void          hset_accum_free(hset_accum *ha){
    invraisecode(ERR_NULLABLE_PTR, ha != NULL, "Null pointer");
    switch (ha->typ) {
        case VALUE64_FS:
            fs_free(hset_accum_fs(ha) );
        break;
        case VALUE64_STR:
            userraiseint(ERR_UNSUPPORTED_TYPE, "VALUE64_STR isn't supported now");
        break;
        default:    // no action here
        break;
    }
    *ha = HSET_ACCUM(ha->typ);  // clear (hset_accum){ .typ = ha->typ }
}

/*
// unified version! TODO:
typedef struct              hset_unified {
    value64  value[HSET_UNIFIED_CNT];    // unified values (int, double, fs etc...)
} hset_unified;
*/

// ------------------------------------- REDUCE  -----------------------------------------
extern void                 hset_sum_int    (hset_accum *acc, value64 v);
extern void                 hset_count_int  (hset_accum *acc, value64 v);
extern void                 hset_max_int    (hset_accum *acc, value64 v);
extern void                 hset_min_int    (hset_accum *acc, value64 v);
//extern void               hset_avg_int    (hset_accum *acc, value64 v);

extern void                 hset_sum_dbl    (hset_accum *acc, value64 v);
extern void                 hset_count_dbl  (hset_accum *acc, value64 v);
extern void                 hset_max_dbl    (hset_accum *acc, value64 v);
extern void                 hset_min_dbl    (hset_accum *acc, value64 v);
//extern void                 hset_avg_dbl    (hset_accum *acc, value64 v);

extern void                 hset_count_fs   (hset_accum *acc, value64 v);
extern void                 hset_max_fs     (hset_accum *acc, value64 v);
extern void                 hset_min_fs     (hset_accum *acc, value64 v);
extern void                 hset_maxlen_fs  (hset_accum *acc, value64 v);
extern void                 hset_minlen_fs  (hset_accum *acc, value64 v);
extern void                 hset_sumlen_fs  (hset_accum *acc, value64 v);
//
extern void                 hset_agg_fs     (hset_accum *acc, value64 v);

// ------------------------------------- FILTER -----------------------------------------

// ------------------------------- engine ------------------------------------------
extern hset                 *hset_filter(hset *restrict se, valur64_predicate_t pred, value64 data);
extern hset                  hset_init_filter(const hset *restrict src, valur64_predicate_t pred, value64 data);

/*
extern hset                 *hset_filter2(hset *restrict se, hset_predicate2_t pred2, value64 data1, value64 data2);
extern hset                  hset_init_filter2(const hset *restrict src, hset_predicate2_t pred2, value64 data1, value64 data2);
*/

// ------------------------ simplifiers predicate engine ---------------------------
// fs - str
// sql-like create as select:fs where predicate:str
extern hset                  hset_create_fs_str_filter(const hset *restrict se, valur64_predicate_t filter, const char *restrict pattern);
// sql-like apply :fs where predicate:str
extern hset                 *hset_apply_fs_str_filter(hset *restrict se, valur64_predicate_t filter, const char *restrict pattern);
// fs - int
// sql-like create as select:fs where predicate:int
extern hset                  hset_create_fs_int_filter(const hset *restrict se, valur64_predicate_t filter, int data);
// sql-like apply :fs where predicate:int
extern hset                 *hset_apply_fs_int_filter(hset *restrict se, valur64_predicate_t filter, int data);

// int - int
// sql-like create as select:int where predicate:int
extern hset                  hset_create_int_int_filter(const hset *restrict se, valur64_predicate_t filter, int data);
// sql-like apply :int where predicate:int
extern hset                 *hset_apply_int_int_filter(hset *restrict se, valur64_predicate_t filter, int data);
// int - (int, int)
// sql-like create as select:int where predicate:(int, int)
extern hset                  hset_create_int_int_filter2(const hset *restrict se,  valur64_predicate_t filter2, int value1, int value2);
// sql-like apply:int where predicate:(int, int)
extern hset                 *hset_apply_int_int_filter2(hset *restrict se, valur64_predicate_t filter2, int value1, int value2);

// --------- simplifyers over filters ---------
// sql-like create :fs as select where length >=  :int
static inline hset           hset_create_fsminlen_int(const hset *restrict se, int len){
    return hset_create_fs_int_filter(se, value64_filter_fsminlen_int, len);
}
// sql-like apply :fs where length >=  :int
static inline hset          *hset_apply_fsminlen_int(hset *restrict se, int len){
    return hset_apply_fs_int_filter(se, value64_filter_fsminlen_int, len);
}
// sql-like create :fs as select where length <=  :int
static inline hset           hset_create_fsmaxlen_int(const hset *restrict se, int len){
    return hset_create_fs_int_filter(se, value64_filter_fsmaxlen_int, len);
}
// sql-like apply :fs where length <=  :int
static inline hset          *hset_apply_fsmaxlen_int(hset *restrict se, int len){
    return hset_apply_fs_int_filter(se, value64_filter_fsmaxlen_int, len);
}
// sql-like create :fs as select where length == :int
static inline hset           hset_create_fslen_int(const hset *restrict se, int len){
    return hset_create_fs_int_filter(se, value64_filter_fslen_int, len);
}
// sql-like apply :fs where length == :int
static inline hset          *hset_apply_fslen_int(hset *restrict se, int len){
    return hset_apply_fs_int_filter(se, value64_filter_fslen_int, len);
}
// sql-like create :fs as select where prefix == :str
static inline hset           hset_create_fsprefix_str(const hset *restrict se, const char *restrict pattern){
    return hset_create_fs_str_filter(se, value64_filter_fsprefix_str, pattern);
}
// sql-like apply :fs where prefix == :str
static inline hset          *hset_apply_fsprefix_str(hset *restrict se, const char *restrict pattern){
    return hset_apply_fs_str_filter(se, value64_filter_fsprefix_str, pattern);
}

// sql-like create :fs as select where like :str
static inline hset           hset_create_fslike_str(const hset *restrict se, const char *restrict pattern){
    return hset_create_fs_str_filter(se, value64_filter_fslike_str, pattern);
}
// sql-like create :fs as select where ulike :str
static inline hset           hset_create_fsulike_str(const hset *restrict se, const char *restrict pattern){
    return hset_create_fs_str_filter(se, value64_filter_fsulike_str, pattern);
}
// sql-like apply :fs where like :str
static inline hset          *hset_apply_fslike_str(hset *restrict se, const char *restrict pattern){
    return hset_apply_fs_str_filter(se, value64_filter_fslike_str, pattern);
}
// sql-like apply :fs where ulike :str
static inline hset          *hset_apply_fsulike_str(hset *restrict se, const char *restrict pattern){
    return hset_apply_fs_str_filter(se, value64_filter_fsulike_str, pattern);
}

// int - int
// ---------- less than ----------
static inline hset          hset_create_intlt_int(const hset *restrict se, int v) {
    return hset_create_int_int_filter(se, value64_filter_intlt_int, v);
}
static inline hset         *hset_apply_intlt_int(hset *restrict se, int v) {
    return hset_apply_int_int_filter(se, value64_filter_intlt_int, v);
}

// ---------- less or equal ----------
static inline hset          hset_create_intle_int(const hset *restrict se, int v) {
    return hset_create_int_int_filter(se, value64_filter_intle_int, v);
}
static inline hset         *hset_apply_intle_int(hset *restrict se, int v) {
    return hset_apply_int_int_filter(se, value64_filter_intle_int, v);
}

// ---------- greater than ----------
static inline hset          hset_create_intgt_int(const hset *restrict se, int v) {
    return hset_create_int_int_filter(se, value64_filter_intgt_int, v);
}
static inline hset         *hset_apply_intgt_int(hset *restrict se, int v) {
    return hset_apply_int_int_filter(se, value64_filter_intgt_int, v);
}

// ---------- greater or equal ----------
static inline hset          hset_create_intge_int(const hset *restrict se, int v) {
    return hset_create_int_int_filter(se, value64_filter_intge_int, v);
}
static inline hset         *hset_apply_intge_int(hset *restrict se, int v) {
    return hset_apply_int_int_filter(se, value64_filter_intge_int, v);
}

// ---------- equal ----------
static inline hset          hset_create_inteq_int(const hset *restrict se, int v) {
    return hset_create_int_int_filter(se, value64_filter_inteq_int, v);
}
static inline hset         *hset_apply_inteq_int(hset *restrict se, int v) {
    return hset_apply_int_int_filter(se, value64_filter_inteq_int, v);
}

// ---------- not equal ----------
static inline hset          hset_create_intne_int(const hset *restrict se, int v) {
    return hset_create_int_int_filter(se, value64_filter_intne_int, v);
}
static inline hset         *hset_apply_intne_int(hset *restrict se, int v) {
    return hset_apply_int_int_filter(se, value64_filter_intne_int, v);
}

// ------------- between int : int ---------------
static inline hset         hset_create_intbetween_int_int(const hset *restrict se, int v1, int v2) {
    return hset_create_int_int_filter2(se, value64_filter2_intbetween_int_int, v1, v2);
}
static inline hset *hset_apply_intbetween_int_int(hset *restrict se, int v1, int v2) {
    return hset_apply_int_int_filter2(se,  value64_filter2_intbetween_int_int, int v1, int v2);
}

// ---------------------------------- MAPPERS ---------------------------------

// ------------------------------- engine ------------------------------------------
// constructor only! No apply engine
extern hset                  hset_init_map(const hset *restrict src, hset_map_t mapper, const value64_params_t *restrict data);

#endif /* !_HASHSET_UTILS_H */

