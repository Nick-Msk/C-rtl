/********************************************************************
             TYPED BASE VALUE64 ARRAY MODULE IMPLEMENTATION
********************************************************************/

#include "value64_tarray.h"

// ------------------------------------ Utilities ------------------------------------------

/**
 * @brief  simple formatter to VALUE64_UNKNOWN
 *
 * @param vtarr  typed value64 pointer
 * @param cnt    count to fromat
*/
static void                             format(value64_typed *vtarr, int cnt) {
    for (int i = 0; i < cnt; i++)
        vtarr[i] = value64_typedunk();
}

/**
 * @brief Изменяет размер динамического массива.
 *
 * - При расширении (newsz > sz) выделяет новый буфер нужного размера.
 *   Если strict == true, размер буфера будет ровно newsz.
 *   Если strict == false, размер может быть увеличен до ближайшей степени двойки.
 * - При сжатии (newsz < sz) размер буфера становится ровно newsz.
 *   При этом newsz НЕ должен быть меньше cnt – это ошибка.
 * - Если newsz == sz, ничего не делает.
 *
 * @param varr   динамический массив
 * @param newsz  новый размер (количество элементов)
 * @param strict управляет оптимизацией расширения
 * @return varr или NULL при ошибке
 */
static                  value64_tarray *resize(value64_tarray *varr, int newsz, bool strict) {
    invraisecode(ERR_NULLABLE_PTR, varr != NULL && newsz >= 0,
        "Null varr %p or incorrent input %d", varr, newsz);

    // Ничего не делаем, если размер не меняется
    if (newsz == varr->sz) {
        return logsimpleret(varr, "Noting to do, sz %d", newsz);
    }

    value64_typed *new_v = varr->v;
    if (newsz < varr->sz) {
        // Сжатие: строгая проверка
        if (newsz < varr->cnt) {
            // Ошибка: нельзя сжать массив так, чтобы потерять существующие элементы
            return logsimpleerr(NULL, "Unable to shrink %d < cnt %d", newsz, varr->cnt);
        }
        // Сжатие всегда strict (не округляем)
        new_v = realloc(varr->v, newsz * sizeof(value64_typed));
        if (!new_v && newsz > 0) {
            userraiseint(ERR_UNABLE_ALLOCATE, "Unable to acclocate %d elems", newsz);  // realloc failed
        }
    } else {
        // Расширение
        if (!strict)
            newsz = calcnewsize(SIZE_POWER2, newsz);
        new_v = realloc(varr->v, newsz * sizeof(value64_typed));
        if (!new_v) {
            userraiseint(ERR_UNABLE_ALLOCATE, "Unable to acclocate %d elems", newsz);  // ошибка выделения памяти
        }

        format(new_v + varr->sz, newsz - varr->sz);
    }
    varr->v = new_v;
    varr->sz = newsz;
    return logsimpleret(varr, "Resized to %d", varr->sz);
}

/**
 * @brief  simple checker for increase
 *
 * @param varr value64_tarray pointer
*/
static bool                             check_increase(value64_tarray *varr) {
    if (varr->cnt >= varr->sz) {
        int newsz = varr->sz;
        if (!resize(varr, newsz, true)) {
            userraiseint(ERR_UNABLE_ALLOCATE,
                         "Unable to grow vt array from %d to %d", varr->sz, newsz);
        }
        return true;    // increased
    } else
        return false;
}

/**
 * @brief  technical printer, wrapper over value64_techfprint
 *
 * @param out FILE *, opened for write
 * @param v value64_typed structure
 */
int                                     value64_typed_techfprint(FILE *out, value64_typed v) {
    int cnt = 0;
    cnt += value64_techfprint(out, v.val, v.typ, "");
    return cnt;
}

// ------------------------------------- API -----------------------------------------------

/**
 * @brief Allocate an empty dynamic value64_tarray.
 * @param cap initial capacity (may be 0).
 * @return initialized array (owns memory, must be freed with value64_tarray_free).
 */
value64_tarray                      value64_tarray_init(int cap) {
    invraisecode(ERR_WRONG_PARAMETER, cap > 0, "Must be positive %d", cap);

    logenter("%d", cap);
    value64_tarray arr = { .v = NULL, .cnt = 0, .sz = 0 };
    if (!resize(&arr, cap, false) )
            userraiseint(ERR_UNABLE_ALLOCATE, "Unable to create vt array %d", cap);

    return logret(arr, "Created with %d", arr.sz);
}

/**
 * @brief Append a typed element to a dynamic array.
 *
 * @param arr  non‑NULL, non‑static value64_tarray
 * @param elem element to append
 * @return pointer to the (possibly resized) array
 * @throws ERR_NULLABLE_PTR if arr is NULL
 * @throws ERR_UNSUPPORTED_TYPE if arr is static (sz < 0)
 * @throws ERR_UNABLE_ALLOCATE if realloc fails
 */
value64_tarray                     *value64_tarray_push(value64_tarray *arr, value64_typed elem) {
    invraisecode(ERR_NULLABLE_PTR, arr != NULL,
                "Null pointer");
    invraisecode(ERR_UNSUPPORTED_TYPE, arr->sz >= 0,
                "Cannot push to static array (sz=%d)", arr->sz);

    logenter("elem typ=%d:%s", elem.typ, value64_typename(elem.typ) );

    check_increase(arr);
    arr->v[arr->cnt++] = value64_typed_clone(elem);
    return logret(arr, "Pushed, cnt=%d", arr->cnt);
}

/**
 * @brief Move a typed element into a dynamic array, taking ownership.
 *
 * The source element *elem is moved into the array via value64_move();
 * after the call *elem is reset to a safe empty state
 * (LITERAL64_ZERO / VALUE64_INT).  This avoids a deep copy for
 * FS/STR values that own heap memory.
 *
 * @param arr  non‑NULL, non‑static value64_tarray
 * @param elem pointer to the element to move
 * @return pointer to the (possibly resized) array
 * @throws ERR_NULLABLE_PTR if arr or elem is NULL
 * @throws ERR_UNSUPPORTED_TYPE if arr is static (sz < 0)
 * @throws ERR_UNABLE_ALLOCATE if realloc fails
 */
value64_tarray *value64_tarray_move(value64_tarray *restrict arr, value64_typed *restrict elem) {
    invraisecode(ERR_NULLABLE_PTR, arr != NULL && elem != NULL,
                 "Null pointer arr=%p elem=%p", arr, elem);
    invraisecode(ERR_UNSUPPORTED_TYPE, arr->sz >= 0,
                 "Cannot move into static array (sz=%d)", arr->sz);

    logenter("elem typ=%d:%s", elem->typ, value64_typename(elem->typ));

    check_increase(arr);
    arr->v[arr->cnt++] = value64_typed_move(elem);
    return logret(arr, "Moved, cnt=%d", arr->cnt);
}

// ------------------------------------- Constructor/destructor -----------------------------------------------

/**
 * @brief Create a dynamic value64_tarray from an array of ints.
 * @param vals pointer to the first element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
value64_tarray                  value64_tarray_int_from_arr(const int *vals, int n) {
    value64_tarray arr = value64_tarray_init(n);
    for (int i = 0; i < n; i++) {
        value64_tarray_push(&arr, value64_typedint(vals[i]) );
    }
    return arr;
}

/**
 * @brief Create a dynamic value64_tarray from an array of longs.
 * @param vals pointer to the first element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
value64_tarray                  value64_tarray_long_from_arr(const long *vals, int n) {
    value64_tarray arr = value64_tarray_init(n);
    for (int i = 0; i < n; i++) {
        value64_tarray_push(&arr, value64_typedlong(vals[i]) );
    }
    return arr;
}
/**
 * @brief Create a dynamic value64_tarray from an array of doubles.
 * @param vals pointer to the first element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
value64_tarray                  value64_tarray_dbl_from_arr(const double *vals, int n) {
    value64_tarray arr = value64_tarray_init(n);
    for (int i = 0; i < n; i++) {
        value64_tarray_push(&arr, value64_typeddbl(vals[i]) );
    }
    return arr;
}
/**
 * @brief Create a dynamic value64_tarray from an array of C‑strings.
 * @param vals pointer to the first string pointer
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
value64_tarray                  value64_tarray_str_from_arr(const char **vals, int n) {
    value64_tarray arr = value64_tarray_init(n);
    for (int i = 0; i < n; i++) {
        value64_tarray_push(&arr, value64_typedstr( vals[i]));
    }
    return arr;
}

/**
 * @brief Create a dynamic value64_tarray from an array of fs pointers.
 * @param vals pointer to the first fs* element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
value64_tarray                  value64_tarray_fs_from_arr(fs **vals, int n) {
    value64_tarray arr = value64_tarray_init(n);
    for (int i = 0; i < n; i++) {
        value64_tarray_push(&arr, value64_typedfs(vals[i] ) );
    }
    return arr;
}
/**
 * @brief Create a dynamic value64_tarray from an array of c-str pointers.
 * @param vals pointer to the first fs* element
 * @param n    number of elements
 * @return initialized dynamic array (must be freed with value64_tarray_free)
 */
extern value64_tarray                  value64_tarray_fs_from_strarr(const char **vals, int n){
    value64_tarray arr = value64_tarray_init(n);
    for (int i = 0; i < n; i++) {
        //value64_typed tmp = value64_typedfs(value64_fs(value64_createfs_asstr(vals[i] ) ) );
        value64_typed tmp = value64_typedfs(fs_heapcopy(vals[i] ) );
        value64_tarray_move(&arr, &tmp);              // перемещаем владение в массив, elem.val обнуляется
    }
    return arr;
}
/**
 * @brief Free a dynamic value64_tarray and all owned elements.
 *
 * Static arrays (sz < 0) are silently ignored.
 * After return the array is zeroed and safe to reuse.
 *
 * @param arr non‑NULL pointer to a dynamic array
 * @throws ERR_NULLABLE_PTR if arr is NULL
 * @throws ERR_UNSUPPORTED_TYPE is static
 */
void                            value64_tarray_free(value64_tarray *arr) {
    invraisecode(ERR_NULLABLE_PTR, arr != NULL,
                "Null pointer");
    //invraisecode(ERR_UNSUPPORTED_TYPE, arr->sz >= 0,
    //            "Cannot free static array (sz=%d)", arr->sz);
    if (arr->sz < 0)
        logsimple("Static - do nothing");
    else {
        for (int i = 0; i < arr->cnt; i++)
            value64_free(&arr->v[i].val, arr->v[i].typ);
        free(arr->v);
        arr->cnt = arr->sz = 0;
        arr->v = NULL;
        logsimple("freed tarray");
    }
};

// ------------------------ Printers ---------------------------------------
/**
 * @brief  technical printer
 *
 * @param out FILE *, opened for write
 * @param arr  value64_tarray pointer
 * @param name   name of value64 tarray
 * @throws ERR_NULLABLE_PTR if arr is NULL
 */
int                             value64_tarray_techfprinf(FILE *restrict out, value64_tarray *restrict arr, const char *restrict name) {
    invraisecode(ERR_NULLABLE_PTR, arr != NULL, "Null pointer");
    int cnt = 0;
    if (out) {
        cnt += fprintf(out, "VALUE64_TARRAY: cnt [%d], sz [%d] %s[\n", arr->cnt, arr->sz, name);
        for (int i = 0; i < arr->cnt; i++)
            cnt += value64_typed_techfprint(out, arr->v[i]);
        cnt += fprintf(out, "]\n");
    }
    return cnt;
}

// ---------------------------------------- Testing ------------------------------------------
#ifdef VALUE64_TARRAY_TESTING

#include "test.h"

// ------------------------- TEST value64_tarray ---------------------------------
static TestStatus
tf_value64_tarray(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Создание пустого динамического массива */
    test_sub("subtest %d: init empty dynamic array", ++subnum);
    {
        value64_tarray arr = value64_tarray_init(0);
        test_validatefree(
            arr.sz == 0 && arr.cnt == 0 && arr.v == NULL,
            value64_tarray_free(&arr),
            "Empty init: sz=%d, cnt=%d, v=%p", arr.sz, arr.cnt, arr.v
        );
        value64_tarray_free(&arr);
    }

    /* 2. Создание с начальной ёмкостью */
    test_sub("subtest %d: init with capacity", ++subnum);
    {
        value64_tarray arr = value64_tarray_init(3);
        test_validatefree(
            arr.sz >= 3 && arr.cnt == 0,
            value64_tarray_free(&arr),
            "Init cap=3: sz=%d, cnt=%d", arr.sz, arr.cnt
        );
        value64_tarray_free(&arr);
    }

    /* 3. Добавление элементов разных типов */
    test_sub("subtest %d: push int, str, fs", ++subnum);
    {
        value64_tarray arr = value64_tarray_init(2);
        // push int
        VALUE64_TARRAY_TECHFPRINF(logfile, &arr);
        value64_tarray_push(&arr, value64_typedint(42));
        VALUE64_TARRAY_TECHFPRINF(logfile, &arr);
        test_validatefree(
            arr.cnt == 1 && arr.v[0].typ == VALUE64_INT && arr.v[0].val.ival == 42,
            value64_tarray_free(&arr),
            "Push int: cnt=%d, val=%d", arr.cnt, arr.v[0].val.ival
        );
        // push str (literal, no ownership)
        value64_tarray_push(&arr, value64_typedstr("hello"));
        test_validatefree(
            arr.cnt == 2 && arr.v[1].typ == VALUE64_STR &&
            strcmp(arr.v[1].val.sval, "hello") == 0,
            value64_tarray_free(&arr),
            "Push str: cnt=%d, val=%s", arr.cnt, arr.v[1].val.sval
        );
        // push fs (heap allocated, ownership transferred)
        fs *f = fs_heapcopy("test");
        value64_tarray_push(&arr, value64_typedfs(f));
        // после push оригинальный f не должен освобождаться пользователем, массив владеет копией?
        // Так как мы клонируем в push, то f можно освободить отдельно
        fs_free(f);
        test_validatefree(
            arr.cnt == 3 && arr.v[2].typ == VALUE64_FS &&
            strcmp(fs_str(arr.v[2].val.fsval), "test") == 0,
            value64_tarray_free(&arr),
            "Push fs: cnt=%d, val=%s", arr.cnt, fs_str(arr.v[2].val.fsval)
        );
        value64_tarray_free(&arr);
    }
    fs_alloc_check(true);

    /* 4. Проверка клонирования: изменение оригинала не влияет на массив */
    test_sub("subtest %d: clone isolation (int)", ++subnum);
    {
        value64_tarray arr = value64_tarray_init(1);
        value64_typed orig = value64_typedint(100);
        value64_tarray_push(&arr, orig);
        // меняем оригинал
        orig.val.ival = 999;
        test_validatefree(
            arr.v[0].val.ival == 100,
            value64_tarray_free(&arr),
            "Clone isolation: array must keep original value 100, got %d",
            arr.v[0].val.ival
        );
        value64_tarray_free(&arr);
    }

    /* 5. Защита от push в статический массив */
    test_sub("subtest %d: push to static array must raise SIGINT", ++subnum);
    {
        value64_static_tarray st = VALUE64_TSTATIC_ARRAY(value64_typedint(1));
        if (!try()) {
            // пытаемся добавить в статический массив (приведение к value64_tarray*)
            value64_tarray_push((value64_tarray*)&st.base, value64_typedint(2));
            // если сюда дошли, то ошибки не было -> провал
            test_validate(false, "Push to static array should have raised SIGINT");
        } else {
            logsimple("Exception correctly raised on push to static");
        }
        // статический массив освобождать не нужно
    }

    /* 6. Освобождение пустого массива */
    test_sub("subtest %d: free empty array", ++subnum);
    {
        value64_tarray arr = {0};
        value64_tarray_free(&arr);
        // просто не должно упасть
        test_validate(true, "Free empty array succeeded");
    }

    /* 7. Освобождение статического массива (должно игнорироваться) */
    test_sub("subtest %d: free static array must be ignored", ++subnum);
    {
        value64_static_tarray st = VALUE64_TSTATIC_ARRAY(value64_typedint(1));
        value64_tarray_free(&st.base);   // sz=-1, функция выйдет
        test_validate(true, "Free static array ignored");
    }

    return logret(TEST_PASSED, "done");
}
// ------------------------- TEST value64_tarray accessors -------------------------
static TestStatus
tf_tarray_accessors(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. getptr / get / elem в нормальном диапазоне */
    test_sub("subtest %d: getptr/get/elem within bounds", ++subnum);
    {
        value64_tarray arr = value64_tarray_init(3);
        value64_tarray_push(&arr, value64_typedint(10));
        value64_tarray_push(&arr, value64_typedint(20));
        value64_tarray_push(&arr, value64_typedint(30));

        // getptr
        value64_typed *p0 = value64_tarray_getptr(&arr, 0);
        value64_typed *p1 = value64_tarray_getptr(&arr, 1);
        test_validatefree(
            p0 != NULL && p1 != NULL &&
            p0->val.ival == 10 && p1->val.ival == 20,
            value64_tarray_free(&arr),
            "getptr: p0->val=%d, p1->val=%d (expected 10, 20)",
            p0 ? p0->val.ival : -1, p1 ? p1->val.ival : -1
        );
        // get (копия)
        value64_typed v0 = value64_tarray_get(&arr, 2);
        test_validatefree(
            v0.val.ival == 30 && v0.typ == VALUE64_INT,
            value64_tarray_free(&arr),
            "get: v0.val=%d, typ=%d (expected 30, INT)", v0.val.ival, v0.typ
        );
        // elem (макрос)
        test_validatefree(
            value64_tarray_elem(&arr, 0).val.ival == 10,
            value64_tarray_free(&arr),
            "elem: arr[0].ival=%d (expected 10)", value64_tarray_elem(&arr, 0).val.ival
        );

        value64_tarray_free(&arr);
    }

    /* 2. доступ по индексу за пределами (должен вызвать SIGINT) */
    test_sub("subtest %d: out-of-bounds access raises SIGINT", ++subnum);
    {
        value64_tarray arr = value64_tarray_init(2);
        value64_tarray_push(&arr, value64_typedint(1));

        if (!try()) {
            value64_tarray_getptr(&arr, 5);   // явно за границей
            test_validatefree(
                false, value64_tarray_free(&arr),
                "Out-of-bounds access should have raised SIGINT"
            );
        } else {
            logsimple("Exception correctly raised on out-of-bounds access");
            value64_tarray_free(&arr);
        }
    }

    /* 3. работа с пустым массивом (sz==0, любой индекс невалиден) */
    test_sub("subtest %d: access on empty array must raise", ++subnum);
    {
        value64_tarray arr = value64_tarray_init(0);
        if (!try()) {
            value64_tarray_getptr(&arr, 0);
            test_validatefree(
                false, value64_tarray_free(&arr),
                "Access on empty array should have raised SIGINT"
            );
        } else {
            logsimple("Exception correctly raised on empty array access");
        }
        value64_tarray_free(&arr);
    }

    /* 4. модификация через getptr и проверка через elem */
    test_sub("subtest %d: modify via getptr", ++subnum);
    {
        value64_tarray arr = value64_tarray_init(1);
        value64_tarray_push(&arr, value64_typedint(100));
        value64_typed *p = value64_tarray_getptr(&arr, 0);
        p->val.ival = 200;
        test_validatefree(
            value64_tarray_elem(&arr, 0).val.ival == 200,
            value64_tarray_free(&arr),
            "After modify via getptr: arr[0].ival=%d (expected 200)",
            value64_tarray_elem(&arr, 0).val.ival
        );
        value64_tarray_free(&arr);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST VALUE64_TSTATIC_ARRAY and typed constructors -------------------------
static TestStatus
tf_tstatic_array(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ====================================================================
     * 1. Проверка статических массивов (все типы, 1..3 элемента)
     * ==================================================================== */

    // ---------- INT ----------
    test_sub("subtest %d: static INT list (1 elem)", ++subnum);
    {
        value64_static_tarray st = V64TYP_INTLIST(42);
        test_validate(
            st.base.cnt == 1 && st.base.sz == -1 &&
            st.base.v[0].val.ival == 42 && st.base.v[0].typ == VALUE64_INT,
            "INT1: cnt=%d, val=%d", st.base.cnt, st.base.v[0].val.ival
        );
        // value64_tarray_free должен безопасно игнорировать статический массив
        value64_tarray_free(&st.base);
        test_validate(st.base.sz == -1, "After free static sz must stay -1");
    }

    test_sub("subtest %d: static INT list (2 elem)", ++subnum);
    {
        value64_static_tarray st = V64TYP_INTLIST(10, 20);
        test_validate(
            st.base.cnt == 2 && st.base.sz == -1 &&
            st.base.v[0].val.ival == 10 && st.base.v[0].typ == VALUE64_INT &&
            st.base.v[1].val.ival == 20 && st.base.v[1].typ == VALUE64_INT,
            "INT2: cnt=%d, vals=%d,%d", st.base.cnt, st.base.v[0].val.ival, st.base.v[1].val.ival
        );
        value64_tarray_free(&st.base);
        test_validate(st.base.sz == -1, "After free static sz must stay -1");
    }

    test_sub("subtest %d: static INT list (3 elem)", ++subnum);
    {
        value64_static_tarray st = V64TYP_INTLIST(1, 6, 7);
        test_validate(
            st.base.cnt == 3 && st.base.sz == -1 &&
            st.base.v[0].val.ival == 1 && st.base.v[0].typ == VALUE64_INT &&
            st.base.v[1].val.ival == 6 && st.base.v[1].typ == VALUE64_INT &&
            st.base.v[2].val.ival == 7 && st.base.v[2].typ == VALUE64_INT,
            "INT3: cnt=%d, vals=%d,%d,%d", st.base.cnt,
            st.base.v[0].val.ival, st.base.v[1].val.ival, st.base.v[2].val.ival
        );
        value64_tarray_free(&st.base);
        test_validate(st.base.sz == -1, "After free static sz must stay -1");
    }

    // ---------- LONG ----------
    test_sub("subtest %d: static LONG list (2 elem)", ++subnum);
    {
        value64_static_tarray st = V64TYP_LONGLIST(100L, 200L);
        test_validate(
            st.base.cnt == 2 && st.base.sz == -1 &&
            st.base.v[0].val.lval == 100L && st.base.v[0].typ == VALUE64_LONG &&
            st.base.v[1].val.lval == 200L && st.base.v[1].typ == VALUE64_LONG,
            "LONG2: cnt=%d, vals=%ld,%ld", st.base.cnt,
            st.base.v[0].val.lval, st.base.v[1].val.lval
        );
        value64_tarray_free(&st.base);
        test_validate(st.base.sz == -1, "After free static sz must stay -1");
    }

    // ---------- DBL ----------
    test_sub("subtest %d: static DBL list (2 elem)", ++subnum);
    {
        value64_static_tarray st = V64TYP_DBLLIST(1.5, 2.718);
        test_validate(
            st.base.cnt == 2 && st.base.sz == -1 &&
            st.base.v[0].val.dval == 1.5 && st.base.v[0].typ == VALUE64_DBL &&
            st.base.v[1].val.dval == 2.718 && st.base.v[1].typ == VALUE64_DBL,
            "DBL2: cnt=%d, vals=%f,%f", st.base.cnt,
            st.base.v[0].val.dval, st.base.v[1].val.dval
        );
        value64_tarray_free(&st.base);
        test_validate(st.base.sz == -1, "After free static sz must stay -1");
    }

    // ---------- PTR ----------
    test_sub("subtest %d: static PTR list (1 elem)", ++subnum);
    {
        int dummy = 99;
        value64_static_tarray st = V64TYP_PTRLIST(&dummy);
        test_validate(
            st.base.cnt == 1 && st.base.sz == -1 &&
            st.base.v[0].val.pval == &dummy && st.base.v[0].typ == VALUE64_PTR,
            "PTR1: cnt=%d, ptr=%p", st.base.cnt, st.base.v[0].val.pval
        );
        value64_tarray_free(&st.base);
        test_validate(st.base.sz == -1, "After free static sz must stay -1");
    }

    // ---------- STR ----------
    test_sub("subtest %d: static STR list (2 elem)", ++subnum);
    {
        value64_static_tarray st = V64TYP_STRLIST("hello", "world");
        test_validate(
            st.base.cnt == 2 && st.base.sz == -1 &&
            strcmp(st.base.v[0].val.sval, "hello") == 0 && st.base.v[0].typ == VALUE64_STR &&
            strcmp(st.base.v[1].val.sval, "world") == 0 && st.base.v[1].typ == VALUE64_STR,
            "STR2: cnt=%d, vals=%s,%s", st.base.cnt,
            st.base.v[0].val.sval, st.base.v[1].val.sval
        );
        value64_tarray_free(&st.base);
        test_validate(st.base.sz == -1, "After free static sz must stay -1");
    }

    // ---------- FS ----------
    test_sub("subtest %d: static FS list (2 elem)", ++subnum);
    {
        fs *f1 = fs_heapcopy("/tmp/a");
        fs *f2 = fs_heapcopy("/tmp/b");
        value64_static_tarray st = V64TYP_FSLIST(f1, f2);
        test_validatefree(
            st.base.cnt == 2 && st.base.sz == -1 &&
            st.base.v[0].val.fsval == f1 && st.base.v[0].typ == VALUE64_FS &&
            st.base.v[1].val.fsval == f2 && st.base.v[1].typ == VALUE64_FS,
            (fs_free(f1), fs_free(f2)),
            "FS2: cnt=%d, ptrs=%p,%p", st.base.cnt, f1, f2
        );
        // Вызов value64_tarray_free на статическом массиве не должен трогать fs
        value64_tarray_free(&st.base);
        test_validatefree(
            st.base.sz == -1,
            (fs_free(f1), fs_free(f2)),
            "Static FS array must stay intact after free"
        );
        fs_free(f1);
        fs_free(f2);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: dynamic from LONG arr + free", ++subnum);
    {
        long vals[] = {100L, 200L, 300L};
        value64_tarray arr = value64_tarray_long_from_arr(vals, 3);
        test_validatefree(
            arr.cnt == 3 && arr.sz >= 3 &&
            arr.v[0].val.lval == 100L &&
            arr.v[1].val.lval == 200L &&
            arr.v[2].val.lval == 300L,
            value64_tarray_free(&arr),
            "Dynamic LONG: cnt=%d but must be 3, arr.sz=%d but must be >= 3, "
            "v[0]=%ld but must be 100, v[1]=%ld but must be 200, v[2]=%ld but must be 300",
            arr.cnt, arr.sz,
            arr.v[0].val.lval, arr.v[1].val.lval, arr.v[2].val.lval
        );
        value64_tarray_free(&arr);
    }

    test_sub("subtest %d: dynamic from DBL arr + free", ++subnum);
    {
        double vals[] = {1.5, 2.718, 3.14};
        value64_tarray arr = value64_tarray_dbl_from_arr(vals, 3);
        test_validatefree(
            arr.cnt == 3 && arr.sz >= 3 &&
            arr.v[0].val.dval == 1.5 &&
            arr.v[1].val.dval == 2.718 &&
            arr.v[2].val.dval == 3.14,
            value64_tarray_free(&arr),
            "Dynamic DBL: cnt=%d but must be 3, arr.sz=%d but must be >= 3, "
            "v[0]=%f but must be 1.5, v[1]=%f but must be 2.718, v[2]=%f but must be 3.14",
            arr.cnt, arr.sz,
            arr.v[0].val.dval, arr.v[1].val.dval, arr.v[2].val.dval
        );
        value64_tarray_free(&arr);
    }

    /* ====================================================================
     * 2. Динамические массивы + корректное освобождение
     * ==================================================================== */

    test_sub("subtest %d: dynamic from INT arr + free", ++subnum);
    {
        int vals[] = {1, 2, 3, 4};
        value64_tarray arr = value64_tarray_int_from_arr(vals, 4);
        test_validatefree(
            arr.cnt == 4 && arr.sz >= 4 &&
            arr.v[0].val.ival == 1 && arr.v[3].val.ival == 4,
            value64_tarray_free(&arr),
            "Dynamic INT: cnt=%d", arr.cnt
        );
        value64_tarray_free(&arr);
    }

    test_sub("subtest %d: dynamic from STR arr + free", ++subnum);
    {
        const char *vals[] = {"a", "b"};
        value64_tarray arr = value64_tarray_str_from_arr(vals, 2);
        test_validatefree(
            arr.cnt == 2 && arr.sz >= 2 &&
            strcmp(arr.v[0].val.sval, "a") == 0,
            value64_tarray_free(&arr),
            "Dynamic STR: cnt=%d", arr.cnt
        );
        value64_tarray_free(&arr);
    }

    test_sub("subtest %d: dynamic from FS arr + free", ++subnum);
    {
        fs *f1 = fs_heapcopy("/tmp/x");
        fs *f2 = fs_heapcopy("/tmp/y");
        fs *arr_fs[] = {f1, f2};
        value64_tarray arr = value64_tarray_fs_from_arr(arr_fs, 2);

        test_validatefree(
            arr.cnt == 2 && arr.sz >= 2 &&
            fs_cmp(arr.v[0].val.fsval, f1) == 0 && fs_cmp(arr.v[1].val.fsval, f2) == 0,
            value64_tarray_free(&arr),
            "Dynamic FS: cnt=%d but must be %d, arr.sz=%d but must be >= %d, arr.v[0].val.fsval = '%s' but mus be '%s', arr.v[1].val.fsval = '%s' but mus be '%s'",
                arr.cnt, 2, arr.sz, 2, fs_str(arr.v[0].val.fsval), fs_str(f1), fs_str(arr.v[1].val.fsval), fs_str(f2)
        );
        value64_tarray_free(&arr);
        fsfreeall(f1, f2);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: dynamic FS from string array (using value64)", ++subnum);
    {
        const char *strings[] = {"/tmp/a", "/tmp/b", "/tmp/c"};
        value64_tarray arr = value64_tarray_fs_from_strarr(strings, 3);

        // Создаём литералы для сравнения
        fs tmp1 = fsliteral("/tmp/a");
        fs tmp2 = fsliteral("/tmp/b");
        fs tmp3 = fsliteral("/tmp/c");

        test_validatefree(
            arr.cnt == 3 && arr.sz >= 3 &&
            fs_cmp(arr.v[0].val.fsval, &tmp1) == 0 &&
            fs_cmp(arr.v[1].val.fsval, &tmp2) == 0 &&
            fs_cmp(arr.v[2].val.fsval, &tmp3) == 0,
            value64_tarray_free(&arr),
            "Dynamic FS from str arr: cnt=%d but must be 3, sz=%d but must be >= 3, "
            "v[0]=%s but must be /tmp/a, v[1]=%s but must be /tmp/b, v[2]=%s but must be /tmp/c",
            arr.cnt, arr.sz,
            fs_str(arr.v[0].val.fsval), fs_str(arr.v[1].val.fsval), fs_str(arr.v[2].val.fsval)
        );
        value64_tarray_free(&arr);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------------------------------------------------------------------------------------------------------------
int
main(int argc, const char *argv[])
{
    logsimpleinit("Start");
    bool    runall = argc == 1;

    while (runall || *++argv){
        int     num = INT_MAX;    // INT_MAX for all test
        if (!runall){
            num = atoi(*argv);
            if (num < 0){
                fprintf(stderr,"Invalid test num %d\n", num);
                continue;
            }
        }
        testenginestd_run(num,
            testnew(.f2 =  tf_value64_tarray,       .num = 1, .name = "SImple init/free test"
                , .desc="", .mandatory=true)
          , testnew(.f2 =  tf_tarray_accessors,     .num = 2, .name = "Simple access test"
                , .desc="", .mandatory=true)
          , testnew(.f2 =  tf_tstatic_array,        .num = 3, .name = "Simple test for VALUE64_TSTATIC_ARRAY and typed constructors"
                , .desc="", .mandatory=true)
        );
        if (runall)
            break;
    }

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* VALUE64_TARRAY_TESTING */


