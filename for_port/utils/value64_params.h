#ifndef _VALUE64_PARAMS_H
#define _VALUE64_PARAMS_H

// ---------------------------------------------------------------------------------
// --------------------------- Public value64 params API ---------------------------
// ---------------------------------------------------------------------------------

// ----------------------------- Includes ------------------------------------------

#include "value64.h"

// --------------------------------- CONSTANTS AND GLOBALS --------------------------

/** @brief Максимальное количество параметров, которое можно передать */
enum { VALUE64_PARAMS_MAX = 4 };

// ---------------------------------- TYPES -----------------------------------------
/**
 * @brief Контейнер для передачи нескольких параметров в предикаты/ map-функции.
 *
 * Используйте макросы VALUE64_PARAMS1..4 для инициализации.
 * Для передачи в функцию используйте LITERAL64_<typ>(&params).
 */
typedef struct {
    value64         v[VALUE64_PARAMS_MAX];
#ifdef VALUE64_PARAMS_FREE
    value64_type    typs[VALUE64_PARAMS_MAX];        // only for free_value64_params
    int             count;                          // only for free_value64_params()
#endif /* VALUE64_PARAMS_FREE */
} value64_params_t;

// ------------------------- CONSTRUCTOTS/DESTRUCTORS -------------------------------


#ifdef VALUE64_PARAMS_FREE
/**
 * @brief Освобождает все владеющие памятью параметры (FS/STR).
 *
 * Используйте только если при компиляции определён VALUE64_PARAMS_FREE.
 * После вызова контейнер обнуляется (count = 0).
 *
 * @param p указатель на контейнер параметров
 */
static inline void                free_value64_params(value64_params_t *p) {
    invraisecode(ERR_NULLABLE_PTR, p != NULL, "Null pointer");

    for (int i = 0; i < p->count; i++) {
        value64_free(&p->v[i], p->typs[i]);
    }
    p->count = 0;
}

/**
 * @brief Создаёт контейнер с одним параметром (с указанием типа для освобождения)
 * @param a   первый параметр
 * @param t1  тип первого параметра (VALUE64_INT, VALUE64_FS, ...)
 */
#define VALUE64_PARAMS1(a, t1) \
   (value64_params_t){ .v = {a}, .typs = {t1}, .count = 1 }

/**
 * @brief Создаёт контейнер с двумя параметрами (с указанием типов)
 */
#define VALUE64_PARAMS2(a, t1, b, t2) \
   (value64_params_t){ .v = {a, b}, .typs = {t1, t2}, .count = 2 }

/**
 * @brief Создаёт контейнер с тремя параметрами (с указанием типов)
 */
#define VALUE64_PARAMS3(a, t1, b, t2, c, t3) \
    (value64_params_t){ .v = {a, b, c}, .typs = {t1, t2, t3}, .count = 3 }

/**
 * @brief Создаёт контейнер с четырьмя параметрами (с указанием типов)
 */
#define VALUE64_PARAMS4(a, t1, b, t2, c, t3, d, t4) \
    (value64_params_t){ .v = {a, b, c, d}, .typs = {t1, t2, t3, t4}, .count = 4 }

#else

/**
 * @brief Создаёт контейнер с одним параметром (без информации о типе)
 */
#define VALUE64_PARAMS1(a)          (value64_params_t){ .v = {a} }

/**
 * @brief Создаёт контейнер с двумя параметрами
 */
#define VALUE64_PARAMS2(a, b)       (value64_params_t){ .v = {a, b} }

/**
 * @brief Создаёт контейнер с тремя параметрами
 */
#define VALUE64_PARAMS3(a, b, c)    (value64_params_t){ .v = {a, b, c} }

/**
 * @brief Создаёт контейнер с четырьмя параметрами
 */
#define VALUE64_PARAMS4(a, b, c, d) (value64_params_t){ .v = {a, b, c, d} }

#endif /* VALUE64_PARAMS_FREE */

// -------------------- ACCESS AND MODIFICATORS -------------------------------------

/**
 * @brief Возвращает i-й параметр (нумерация с 0).
 *
 * При выходе за границы возвращает LITERAL64_ZERO.
 *
 * @param p указатель на контейнер параметров
 * @param i индекс параметра (0..3)
 * @return значение параметра или LITERAL64_ZERO
 */
static inline value64           value64_getpar(const value64_params_t *p, int i) {
    if (!p || i < 0 || i >= VALUE64_PARAMS_MAX) return LITERAL64_ZERO;
    return p->v[i];
}

/** @brief Возвращает первый параметр */
static inline value64           value64_getpar1(const value64_params_t *p) {
    return value64_getpar(p, 0);
}
/** @brief Возвращает второй параметр */
static inline value64           value64_getpar2(const value64_params_t *p) {
    return value64_getpar(p, 1);
}
/** @brief Возвращает третий параметр */
static inline value64           value64_getpar3(const value64_params_t *p) {
    return value64_getpar(p, 2);
}
/** @brief Возвращает четвёртый параметр */
static inline value64           value64_getpar4(const value64_params_t *p) {
    return value64_getpar(p, 3);
}

// ------------------------------------ ETC. ----------------------------------------

#endif /* !_VALUE64_PARAMS_H */

