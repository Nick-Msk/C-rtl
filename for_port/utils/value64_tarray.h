#ifndef _VALUE64_TARRAY_H
#define _VALUE64_TARRAY_H

#include "value64.h"

// ------------------------------- TYPES ---------------------------------

/** @brief Типизированное значение value64 (значение + тип) */
typedef struct {
    value64      val;   ///< само значение
    value64_type typ;   ///< его тип (VALUE64_INT, VALUE64_FS, ...)
} value64_typed;

/** @brief Динамический массив типизированных value64 */
typedef struct {
    value64_typed *v;   ///< элементы
    int            cnt; ///< занято
    int            sz;  ///< выделено
} value64_tarray;

// for static init (no free)
typedef struct {
    value64_tarray base;
} value64_static_tarray;

// TODO: value64_tarray4 {value64_tarray v[4];}

// ------------------------- CONSTRUCTORS / DESTRUCTORS -------------------

/** @brief Создать пустой массив с начальной ёмкостью cap */
extern value64_tarray                  value64_tarray_init(int cap);

/** @brief Освободить массив и все владеющие памятью элементы (FS/STR) */
extern void                            value64_tarray_free(value64_tarray *arr);

/**
 * @brief Create a dynamic value64_tarray from an array of ints.
 * @param vals pointer to the first element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
extern value64_tarray                  value64_tarray_int_from_arr(const int *vals, int n);
/**
 * @brief Create a dynamic value64_tarray from an array of longs.
 * @param vals pointer to the first element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
extern value64_tarray                  value64_tarray_long_from_arr(const long *vals, int n);
/**
 * @brief Create a dynamic value64_tarray from an array of doubles.
 * @param vals pointer to the first element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
extern value64_tarray                  value64_tarray_dbl_from_arr(const double *vals, int n);
/**
 * @brief Create a dynamic value64_tarray from an array of C‑strings.
 * @param vals pointer to the first string pointer
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
extern value64_tarray                  value64_tarray_str_from_arr(const char **vals, int n);
/**
 * @brief Create a dynamic value64_tarray from an array of fs pointers.
 * @param vals pointer to the first fs* element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
extern value64_tarray                  value64_tarray_fs_from_arr(fs **vals, int n);
/**
 * @brief Create a dynamic value64_tarray from an array of c-str pointers.
 * @param vals pointer to the first fs* element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
extern value64_tarray                  value64_tarray_fs_from_strarr(const char **vals, int n);
// ------------------------- ДОБАВЛЕНИЕ -----------------------------------

/** @brief Добавить элемент в конец (автоматически расширяет массив) */
extern value64_tarray                 *value64_tarray_push(value64_tarray *arr, value64_typed elem);

extern value64_tarray                 *value64_tarray_move(value64_tarray *restrict arr, value64_typed *restrict elem);

// ------------------------- ДОСТУП ---------------------------------------

/** @brief Получить указатель на i-й элемент */
static inline value64_typed          *value64_tarray_getptr(value64_tarray *arr, int i) {
    invraisecode(ERR_NULLABLE_PTR, arr != NULL, "Null arr");
    invraisecode(ERR_OUT_OF_RANGE, i < arr->sz && i >= 0, "%d is out of range (0-%d)", i, arr->sz);
    return arr->v + i;
}
/** @brief Получить i-й элемент (возвращает копию value64_typed) */
static inline value64_typed           value64_tarray_get(value64_tarray *arr, int i) {
    return *value64_tarray_getptr(arr, i);
}
#define                              value64_tarray_elem(arr, pos) (*value64_tarray_getptr(arr, pos) )

// ------------------------ Printers ---------------------------------------

/**
 * @brief  technical printer
 *
 * @param out FILE *, opened for write
 * @param arr  value64_tarray pointer
 * @param name   name of value64 tarray
*/
extern int                             value64_tarray_techfprinf(FILE *restrict out, value64_tarray *restrict arr, const char *restrict name);
/**
 * @brief  technical printer to stdout
 *
 * @param arr  value64_tarray pointer
 * @param name   name of value64 tarray
*/
static inline int                      value64_tarray_techprinf(value64_tarray *restrict arr, const char *restrict name) {
    return value64_tarray_techfprinf(stdout, arr, name);
}

#define VALUE64_TARRAY_TECHFPRINF(out, arr) value64_tarray_techfprinf(out, arr, #arr)
#define VALUE64_TARRAY_TECHPRINF(arr) value64_tarray_techfprinf(arr, #arr)

/**
 * @brief  technical printer, wrapper over value64_techfprint
 *
 * @param out FILE *, opened for write
 * @param v value64_typed structure
 */
int                                    value64_typed_techfprint(FILE *out, value64_typed v);

// ------------------------- ИНИЦИАЛИЗАЦИЯ ЛИТЕРАЛАМИ --------------------

/** @brief Создать статический массив из литералов */
#define VALUE64_TSTATIC_ARRAY(...) \
    (value64_static_tarray){ .base = { \
                      .v = (value64_typed[]){__VA_ARGS__}, \
                      .cnt = COUNT((value64_typed[]){__VA_ARGS__}), \
                      .sz = -1 } \
                           }

#define V64TYP_INT(val)    (value64_typed){ .val = LITERAL64_INT(val), .typ = VALUE64_INT }
#define V64TYP_LONG(val)   (value64_typed){ .val = LITERAL64_LONG(val), .typ = VALUE64_LONG }
#define V64TYP_DBL(val)    (value64_typed){ .val = LITERAL64_DBL(val), .typ = VALUE64_DBL }
#define V64TYP_PTR(val)    (value64_typed){ .val = LITERAL64_PTR(val), .typ = VALUE64_PTR }
#define V64TYP_STR(val)    (value64_typed){ .val = LITERAL64_STR(val), .typ = VALUE64_STR }  // для литералов
#define V64TYP_FS(val)     (value64_typed){ .val = LITERAL64_PFS(val), .typ = VALUE64_FS }

/** @brief Выбрать макрос по количеству аргументов (до 4) */
#define GET_MACRO(_1,_2,_3,_4,NAME,...) NAME

// ==================== V64TYP – static GENERATED array helpers (up to 4 elements) ====================

// ==================== INT ====================
#define V64TYP_INTLIST(...) \
    GET_MACRO(__VA_ARGS__, _V64TYP_INTLIST4, _V64TYP_INTLIST3, _V64TYP_INTLIST2, _V64TYP_INTLIST1)(__VA_ARGS__)

#define _V64TYP_INTLIST1(v1) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_INT(v1), .typ = VALUE64_INT } }, \
        .cnt = 1, .sz = -1 } }
#define _V64TYP_INTLIST2(v1, v2) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_INT(v1), .typ = VALUE64_INT }, \
                                (value64_typed){ .val = LITERAL64_INT(v2), .typ = VALUE64_INT } }, \
        .cnt = 2, .sz = -1 } }
#define _V64TYP_INTLIST3(v1, v2, v3) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_INT(v1), .typ = VALUE64_INT }, \
                                (value64_typed){ .val = LITERAL64_INT(v2), .typ = VALUE64_INT }, \
                                (value64_typed){ .val = LITERAL64_INT(v3), .typ = VALUE64_INT } }, \
        .cnt = 3, .sz = -1 } }
#define _V64TYP_INTLIST4(v1, v2, v3, v4) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_INT(v1), .typ = VALUE64_INT }, \
                                (value64_typed){ .val = LITERAL64_INT(v2), .typ = VALUE64_INT }, \
                                (value64_typed){ .val = LITERAL64_INT(v3), .typ = VALUE64_INT }, \
                                (value64_typed){ .val = LITERAL64_INT(v4), .typ = VALUE64_INT } }, \
        .cnt = 4, .sz = -1 } }

// ==================== LONG ====================
#define V64TYP_LONGLIST(...) \
    GET_MACRO(__VA_ARGS__, _V64TYP_LONGLIST4, _V64TYP_LONGLIST3, _V64TYP_LONGLIST2, _V64TYP_LONGLIST1)(__VA_ARGS__)

#define _V64TYP_LONGLIST1(v1) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_LONG(v1), .typ = VALUE64_LONG } }, \
        .cnt = 1, .sz = -1 } }
#define _V64TYP_LONGLIST2(v1, v2) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_LONG(v1), .typ = VALUE64_LONG }, \
                                (value64_typed){ .val = LITERAL64_LONG(v2), .typ = VALUE64_LONG } }, \
        .cnt = 2, .sz = -1 } }
#define _V64TYP_LONGLIST3(v1, v2, v3) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_LONG(v1), .typ = VALUE64_LONG }, \
                                (value64_typed){ .val = LITERAL64_LONG(v2), .typ = VALUE64_LONG }, \
                                (value64_typed){ .val = LITERAL64_LONG(v3), .typ = VALUE64_LONG } }, \
        .cnt = 3, .sz = -1 } }
#define _V64TYP_LONGLIST4(v1, v2, v3, v4) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_LONG(v1), .typ = VALUE64_LONG }, \
                                (value64_typed){ .val = LITERAL64_LONG(v2), .typ = VALUE64_LONG }, \
                                (value64_typed){ .val = LITERAL64_LONG(v3), .typ = VALUE64_LONG }, \
                                (value64_typed){ .val = LITERAL64_LONG(v4), .typ = VALUE64_LONG } }, \
        .cnt = 4, .sz = -1 } }

// ==================== DBL ====================
#define V64TYP_DBLLIST(...) \
    GET_MACRO(__VA_ARGS__, _V64TYP_DBLLIST4, _V64TYP_DBLLIST3, _V64TYP_DBLLIST2, _V64TYP_DBLLIST1)(__VA_ARGS__)

#define _V64TYP_DBLLIST1(v1) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_DBL(v1), .typ = VALUE64_DBL } }, \
        .cnt = 1, .sz = -1 } }
#define _V64TYP_DBLLIST2(v1, v2) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_DBL(v1), .typ = VALUE64_DBL }, \
                                (value64_typed){ .val = LITERAL64_DBL(v2), .typ = VALUE64_DBL } }, \
        .cnt = 2, .sz = -1 } }
#define _V64TYP_DBLLIST3(v1, v2, v3) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_DBL(v1), .typ = VALUE64_DBL }, \
                                (value64_typed){ .val = LITERAL64_DBL(v2), .typ = VALUE64_DBL }, \
                                (value64_typed){ .val = LITERAL64_DBL(v3), .typ = VALUE64_DBL } }, \
        .cnt = 3, .sz = -1 } }
#define _V64TYP_DBLLIST4(v1, v2, v3, v4) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_DBL(v1), .typ = VALUE64_DBL }, \
                                (value64_typed){ .val = LITERAL64_DBL(v2), .typ = VALUE64_DBL }, \
                                (value64_typed){ .val = LITERAL64_DBL(v3), .typ = VALUE64_DBL }, \
                                (value64_typed){ .val = LITERAL64_DBL(v4), .typ = VALUE64_DBL } }, \
        .cnt = 4, .sz = -1 } }

// ==================== PTR ====================
#define V64TYP_PTRLIST(...) \
    GET_MACRO(__VA_ARGS__, _V64TYP_PTRLIST4, _V64TYP_PTRLIST3, _V64TYP_PTRLIST2, _V64TYP_PTRLIST1)(__VA_ARGS__)

#define _V64TYP_PTRLIST1(v1) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_PTR(v1), .typ = VALUE64_PTR } }, \
        .cnt = 1, .sz = -1 } }
#define _V64TYP_PTRLIST2(v1, v2) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_PTR(v1), .typ = VALUE64_PTR }, \
                                (value64_typed){ .val = LITERAL64_PTR(v2), .typ = VALUE64_PTR } }, \
        .cnt = 2, .sz = -1 } }
#define _V64TYP_PTRLIST3(v1, v2, v3) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_PTR(v1), .typ = VALUE64_PTR }, \
                                (value64_typed){ .val = LITERAL64_PTR(v2), .typ = VALUE64_PTR }, \
                                (value64_typed){ .val = LITERAL64_PTR(v3), .typ = VALUE64_PTR } }, \
        .cnt = 3, .sz = -1 } }
#define _V64TYP_PTRLIST4(v1, v2, v3, v4) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_PTR(v1), .typ = VALUE64_PTR }, \
                                (value64_typed){ .val = LITERAL64_PTR(v2), .typ = VALUE64_PTR }, \
                                (value64_typed){ .val = LITERAL64_PTR(v3), .typ = VALUE64_PTR }, \
                                (value64_typed){ .val = LITERAL64_PTR(v4), .typ = VALUE64_PTR } }, \
        .cnt = 4, .sz = -1 } }

// ==================== STR ====================
#define V64TYP_STRLIST(...) \
    GET_MACRO(__VA_ARGS__, _V64TYP_STRLIST4, _V64TYP_STRLIST3, _V64TYP_STRLIST2, _V64TYP_STRLIST1)(__VA_ARGS__)

#define _V64TYP_STRLIST1(v1) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_STR(v1), .typ = VALUE64_STR } }, \
        .cnt = 1, .sz = -1 } }
#define _V64TYP_STRLIST2(v1, v2) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_STR(v1), .typ = VALUE64_STR }, \
                                (value64_typed){ .val = LITERAL64_STR(v2), .typ = VALUE64_STR } }, \
        .cnt = 2, .sz = -1 } }
#define _V64TYP_STRLIST3(v1, v2, v3) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_STR(v1), .typ = VALUE64_STR }, \
                                (value64_typed){ .val = LITERAL64_STR(v2), .typ = VALUE64_STR }, \
                                (value64_typed){ .val = LITERAL64_STR(v3), .typ = VALUE64_STR } }, \
        .cnt = 3, .sz = -1 } }
#define _V64TYP_STRLIST4(v1, v2, v3, v4) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_STR(v1), .typ = VALUE64_STR }, \
                                (value64_typed){ .val = LITERAL64_STR(v2), .typ = VALUE64_STR }, \
                                (value64_typed){ .val = LITERAL64_STR(v3), .typ = VALUE64_STR }, \
                                (value64_typed){ .val = LITERAL64_STR(v4), .typ = VALUE64_STR } }, \
        .cnt = 4, .sz = -1 } }

// ==================== FS ====================
#define V64TYP_FSLIST(...) \
    GET_MACRO(__VA_ARGS__, _V64TYP_FSLIST4, _V64TYP_FSLIST3, _V64TYP_FSLIST2, _V64TYP_FSLIST1)(__VA_ARGS__)

#define _V64TYP_FSLIST1(v1) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_PFS(v1), .typ = VALUE64_FS } }, \
        .cnt = 1, .sz = -1 } }
#define _V64TYP_FSLIST2(v1, v2) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_PFS(v1), .typ = VALUE64_FS }, \
                                (value64_typed){ .val = LITERAL64_PFS(v2), .typ = VALUE64_FS } }, \
        .cnt = 2, .sz = -1 } }
#define _V64TYP_FSLIST3(v1, v2, v3) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_PFS(v1), .typ = VALUE64_FS }, \
                                (value64_typed){ .val = LITERAL64_PFS(v2), .typ = VALUE64_FS }, \
                                (value64_typed){ .val = LITERAL64_PFS(v3), .typ = VALUE64_FS } }, \
        .cnt = 3, .sz = -1 } }
#define _V64TYP_FSLIST4(v1, v2, v3, v4) \
    (value64_static_tarray){ .base = { \
        .v = (value64_typed[]){ (value64_typed){ .val = LITERAL64_PFS(v1), .typ = VALUE64_FS }, \
                                (value64_typed){ .val = LITERAL64_PFS(v2), .typ = VALUE64_FS }, \
                                (value64_typed){ .val = LITERAL64_PFS(v3), .typ = VALUE64_FS }, \
                                (value64_typed){ .val = LITERAL64_PFS(v4), .typ = VALUE64_FS } }, \
        .cnt = 4, .sz = -1 } }

// TODO:
#define V64TYP_FSLIST_FROMSTR(...) ...

// SIMPLIFIERS
#define V64TYP_CNT(parr)       ((parr)->cnt)

#define V64TYP_INT1(parr)      value64_int((parr)->v[0].val)
#define V64TYP_INT2(parr)      value64_int((parr)->v[1].val)
#define V64TYP_INT3(parr)      value64_int((parr)->v[2].val)
#define V64TYP_INT4(parr)      value64_int((parr)->v[3].val)

#define V64TYP_LONG1(parr)     value64_long((parr)->v[0].val)
#define V64TYP_LONG2(parr)     value64_long((parr)->v[1].val)
#define V64TYP_LONG3(parr)     value64_long((parr)->v[2].val)
#define V64TYP_LONG4(parr)     value64_long((parr)->v[3].val)

#define V64TYP_DBL1(parr)      value64_dbl((parr)->v[0].val)
#define V64TYP_DBL2(parr)      value64_dbl((parr)->v[1].val)
#define V64TYP_DBL3(parr)      value64_dbl((parr)->v[2].val)
#define V64TYP_DBL4(parr)      value64_dbl((parr)->v[3].val)

#define V64TYP_STR1(parr)      value64_str((parr)->v[0].val)
#define V64TYP_STR2(parr)      value64_str((parr)->v[1].val)
#define V64TYP_STR3(parr)      value64_str((parr)->v[2].val)
#define V64TYP_STR4(parr)      value64_str((parr)->v[3].val)

#define V64TYP_FS1(parr)       value64_fs((parr)->v[0].val)
#define V64TYP_FS2(parr)       value64_fs((parr)->v[1].val)
#define V64TYP_FS3(parr)       value64_fs((parr)->v[2].val)
#define V64TYP_FS4(parr)       value64_fs((parr)->v[3].val)


// ------------------------- ВСПОМОГАТЕЛЬНЫЕ КОНСТРУКТОРЫ ----------------

static inline value64_typed     value64_typedint(int x) {
    return (value64_typed){ .val = LITERAL64_INT(x), .typ = VALUE64_INT };
}
static inline value64_typed     value64_typedlong(long x) {
    return (value64_typed){ .val = LITERAL64_LONG(x), .typ = VALUE64_LONG };
}
static inline value64_typed     value64_typeddbl(double x) {
    return (value64_typed){ .val = LITERAL64_DBL(x), .typ = VALUE64_DBL };
}
static inline value64_typed     value64_typedstr(const char *s) {
    // Внимание: строка не копируется, ожидается, что она существует всё время использования массива
    return (value64_typed){ .val = LITERAL64_STR(s), .typ = VALUE64_STR };
}
static inline value64_typed     value64_typedfs(fs *s) {
    return (value64_typed){ .val = LITERAL64_PFS(s), .typ = VALUE64_FS };
}
static inline value64_typed     value64_typedunk(void) {
    return (value64_typed){ .val = LITERAL64_ZERO, .typ = VALUE64_UNKNOWN };
}
static inline value64_typed     value64_typed_clone(value64_typed v) {
    return (value64_typed) { .val = value64_clone(v.val, v.typ), .typ = v.typ };
}
static inline value64_typed     value64_typed_move(value64_typed *v) {
    value64_typed   res = { .val = value64_move(&v->val, v->typ), .typ = v->typ};
    v->typ = VALUE64_UNKNOWN;
    return res;
}

#endif /* _VALUE64_TARRAY_H */

