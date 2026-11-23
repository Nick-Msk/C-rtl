#ifndef _ITERATOR_H
#define _ITERATOR_H

#include <limits.h>

#define foreachint(var, ...) \
    for (int *_iter = (int[]){__VA_ARGS__, INT_MAX}, var = *_iter; \
         var != INT_MAX; \
         var = *++_iter)

#define foreachuint(var, ...) \
    for (unsigned *_iter = (unsigned[]){__VA_ARGS__, UINT_MAX}, var = *_iter; \
         var != UINT_MAX; \
         var = *++_iter)

#define foreachlong(var, ...) \
    for (long *_iter = (long[]){__VA_ARGS__, LONG_MAX}, var = *_iter; \
         var != LONG_MAX; \
         var = *++_iter)

#define foreachulong(var, ...) \
    for (unsigned long *_iter = (unsigned long[]){__VA_ARGS__, ULONG_MAX}, var = *_iter; \
         var != ULONG_MAX; \
         var = *++_iter)

#define foreachstring(var, ...) \
    for (const char **_iter = (const char *[]){__VA_ARGS__, 0x0}, var = *_iter; \
         var != 0x0; \
         var = *++_iter)

// ----------------------------------------- COMMON EXPAND MACRO -------------------------------------

#define                     CONCAT_EXPAND(x, y) x ## y
#define                     CONCAT(x, y)        CONCAT_EXPAND(x, y)
#define                     UNIQUE_ID(prefix)   CONCAT(prefix, __COUNTER__)

// ----------------------------- ARRAY OF POINTERS, RETURNS POINTER TO ELEMENT -----------------------
#define                     _pforeach_arrptr_impl(pi, parr, iter_name) \
        _Static_assert(__builtin_classify_type(*parr) == 5, \
                       "foreach_pointer: массив должен состоять из указателей"); \
        for (typeof_unqual(*parr)* iter_name = (parr), pi = *iter_name; \
                  *iter_name != 0;\
                  pi = *++iter_name)

#define                     _pforeach_arrptr_named(pi, parr, parr_name) \
    typeof_unqual(parr) parr_name = (parr); \
    _pforeach_arrptr_impl(pi, parr_name, UNIQUE_ID(_iter_))

// iterate via pointer to element
#define                     pforeach_arrptr(pi, parr)\
    _pforeach_arrptr_named(pi, parr, UNIQUE_ID(_parr_))

// ----------------------------- ARRAY OF POINTERS, RETURNS INDEX -------------------------------------
// iterate via int value
#define                     foreach_arrptr(i, parr)\
    _Static_assert(__builtin_classify_type(*(parr)) == 5, "Array of pointers is required");\
    for (int i = 0; parr[i] != 0; i++)

// -------------------------------- ANY ARRAY, RETURN INDEX, BUT CNT MUST BE SUPPLIED -----------------
/*#define                     foreach_arr(i, cnt)\
    for (int i = 0; i < (cnt); i++)  TODO: DISABLED FOR NOW
*/
// ---------------- ARRAY OF POINTERS, RETURNS ELEMENT, BUT CNT MUST BE SUPPLIED  --------------------

// Внутренний макрос: получает уже готовое arr_name и iter_name
#define                             _foreach_arr_impl(iter, arr_name, cnt) \
    for (typeof_unqual(*(arr_name)) *iter_name = (arr_name), iter = *iter_name; \
         iter_name - arr_name <cnt; \
         iter = *++iter_name)

// Промежуточный макрос: сохраняет arr в arr_name (полученном снаружи) и генерирует iter_name
#define                             _foreach_arr_named(iter, arr, cnt, arr_name) \
    typeof_unqual(*arr) *arr_name = arr; \
    _foreach_arr_impl(iter, arr_name, cnt)

// Внешний макрос: генерирует уникальное имя для arr ОДИН раз и передаёт его вниз
#define                             foreach_arr(iter, arr, cnt) \
    _foreach_arr_named(iter, (arr), (cnt), UNIQUE_ID(_arr_))

// ----------- ARRAY OF POINTERS, RETURNS POINTER TO ELEMENT, BUT CNT MUST BE SUPPLIED  ---------------

// Внутренний макрос: получает уже готовое arr_name и iter_name
#define                             _pforeach_arr_impl(iter, arr_name, cnt) \
    for (typeof_unqual(*(arr_name)) *iter_name = (arr_name), *iter = iter_name; \
         iter_name - arr_name <cnt; \
         iter = ++iter_name)

// Промежуточный макрос: сохраняет arr в arr_name (полученном снаружи) и генерирует iter_name
#define                             _pforeach_arr_named(iter, arr, cnt, arr_name) \
    typeof_unqual(*arr) *arr_name = arr; \
    _pforeach_arr_impl(iter, arr_name, cnt)

// Внешний макрос: генерирует уникальное имя для arr ОДИН раз и передаёт его вниз
#define                             pforeach_arr(iter, arr, cnt) \
    _pforeach_arr_named(iter, (arr), (cnt), UNIQUE_ID(_arr_))

#endif /* !_ITERATOR_H */
