#ifndef _ARRAY_H
#define _ARRAY_H

/**
 * @file array.h
 * @brief Utility functions and metadata for Array type management.
 */

#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <stdlib.h>
#include <float.h>

#include "common.h"
#include "log.h"
#include "checker.h"
#include "value64.h"    // for Value64
#include "error.h"
#include "v64gen.h"

#include "ds.h"

// ---------------------------------------------------------------------------------
// --------------------------- Public Array API ------------------------------------
// ---------------------------------------------------------------------------------

// ----------- CONSTANTS AND GLOBALS ---------------

/**
 * @enum ArrayFillType
 * @brief Defines strategies for initializing array elements.
 */
typedef enum ArrayFillType{
    ARRAY_FILLTYPE_SAFE_EMPTY      = 0,
    ARRAY_FILLTYPE_DESC,
    ARRAY_FILLTYPE_ASC,
    ARRAY_FILLTYPE_RND,
    ARRAY_FILLTYPE_ZERO,
    ARRAY_FILLTYPE_ASC_SERIES,
    ARRAY_FILLTYPE_DESC_SERIES,
    //
    ARRAY_FILLTYPE_MAX          // not a real value
} ArrayFillType;

typedef enum ArraySortType{
    ARRAY_SORTTYPE_DESC = ARRAY_FILLTYPE_DESC,
    ARRAY_SORTTYPE_ASC  = ARRAY_FILLTYPE_ASC
} ArraySortType;

/**
 * @struct ArrayFillTypeInfo
 * @brief Internal metadata containing properties of an ArrayFillType.
 */
typedef struct {
    ArrayFillType    type;
    const char      *name;
} ArrayFillTypeInfo;

/**
 * @enum ArrayType
 * @brief Identifies the primitive data type stored within an Array.
 */
typedef enum ArrayType{
    ARRAY_UNKNOWN   = 0x0,
    ARRAY_DOUBLE    = 0x1,
    ARRAY_INT       = 0x2,
    ARRAY_LONG      = 0x3,
    ARRAY_POINTER   = 0x4,
    ARRAY_CHAR      = 0x5,       // to supply c-str/fs => Array conversion
    ARRAY_V64       = 0x11      // value64 port => change to 17
    //ARRAY_ERROR     = 0x100     // TODO: delete that
} ArrayType;

/**
 * @struct ArrayTypeInfo
 * @brief Internal metadata containing all properties of an ArrayType.
 */
typedef struct {
    ArrayType       type;           /**< The base enum identifier */
    value64_type    vt64;           /**< value64 map (if exists) */
    const char     *name;           /**< Pretty name (e.g., "DOUBLE") */
    const char     *name_raw;       /**< Parsing name (e.g., "ARRAY_DOUBLE") */
    size_t          elem_size;       /**< Size of the element in bytes */
} ArrayTypeInfo;

static const ArrayTypeInfo          ARRAY_TYPE_TABLE[] = {
    /* Базовый тип | Подтип (v64) | Имя (pretty) | Имя (raw) | Размер */
    { ARRAY_INT,     VALUE64_INT,   "INT",        "ARRAY_INT",     sizeof(int) },
    { ARRAY_LONG,    VALUE64_LONG,  "LONG",       "ARRAY_LONG",    sizeof(long) },
    { ARRAY_DOUBLE,  VALUE64_DBL,   "DOUBLE",     "ARRAY_DOUBLE",  sizeof(double) },
    { ARRAY_POINTER, VALUE64_PTR,   "POINTER",    "ARRAY_POINTER", sizeof(void*) },
    { ARRAY_CHAR,    VALUE64_CHR,   "CHAR",       "ARRAY_CHAR",    sizeof(char) },
    //    V64 container
    { ARRAY_V64,     VALUE64_UNKNOWN, "V64",        "ARRAY_V64",     sizeof(value64) },
    // 
    { ARRAY_UNKNOWN, VALUE64_UNKNOWN, "UNKNOWN",    "ARRAY_UNKNOWN", 0 }
};

static const ArrayFillTypeInfo      ARRAY_FILLTYPE_TABLE[] = {
    { ARRAY_FILLTYPE_SAFE_EMPTY,     "ARRAY_FILLTYPE_SAFE_EMPTY" },
    { ARRAY_FILLTYPE_DESC,           "FILLTYPE_DESC" },
    { ARRAY_FILLTYPE_ASC,            "FILLTYPE_ASC" },
    { ARRAY_FILLTYPE_RND,            "FILLTYPE_RND" },
    { ARRAY_FILLTYPE_ZERO,           "FILLTYPE_ZERO" },
    { ARRAY_FILLTYPE_ASC_SERIES,     "FILLTYPE_ASC_SERIES" },
    { ARRAY_FILLTYPE_DESC_SERIES,    "FILLTYPE_DESC_SERIES" }
};

extern int              g_array_rec_line;
extern const char      *g_custom_print_line;
// ------------------- TYPES -----------------------

typedef struct ArraySlice ArraySlice;

typedef struct {
    size_t          len;
    size_t          sz; // total size, > len + 1
    int             flags; // ARRAY_DOUBLE || ARRAY_INT || ARRAY_POINTER || ARRAY_V64 etc...
    union {
        void    *v;
        int     *iv;
        long    *lv;
        double  *dv;
        void   **pv;    // pointer array
        char    *cv;    // char array, NOT a char * array!
        struct {
            value64        *v64;   // container type
            value64_type    v64type;        // applicable only for ARRAY_V64
        };
    };
    // Slice API support (not implemented yet)
    size_t           max_slice_end_pos;
    ArraySlice      *first_child;
} Array;
// condition func
typedef         bool (*ArrayCond)(Array *arr, size_t pos);
// prosessing func
typedef         void (*ArrayProc)(Array *arr, size_t pos);

typedef struct ArraySlice {
    Array       *parent;       // Ссылка на владельца
    size_t       offset;        // Смещение относительно начала родителя
    size_t       len;           // Длина среза
    union {
        void    *v;
        int     *iv;
        long    *lv;
        double  *dv;
        void   **pv;    // pointer array
        char    *cv;    // char array, NOT a char * array!
        //struct {
        value64 *v64;   // container type
        //    value64_type    v64type;        // applicable only for ARRAY_V64
        //};
    };            // Указатель на начало данных среза (parent->v + offset)
    ArraySlice  *next_sibling; // Для связного списка в родителе NOT CLEAR WHAT FOR
} ArraySlice;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

/**
 * @brief Retrieves the metadata pointer for a given ArrayType.
 * 
 * @details This is the core function for all type-related queries. 
 *          It searches the master metadata table for a matching type.
 * 
 * @param t The ArrayType enum value to look up.
 * @return const ArrayTypeInfo* Pointer to the metadata structure. 
 *         Returns a pointer to the ARRAY_UNKNOWN entry if no match is found.
 */
static inline const ArrayTypeInfo   *arrayTypeGetInfo(ArrayType t) {
    size_t      i;
    for (i = 0; i < COUNT(ARRAY_TYPE_TABLE); i++) {
        if (ARRAY_TYPE_TABLE[i].type == t)
            return ARRAY_TYPE_TABLE + i;
    }
    return ARRAY_TYPE_TABLE + i - 1;    // ARRAY_UNKNOWN
 }

// mapper ARRAY_INT -> VALUE64_INT... etc
static inline value64_type          arrayTypeV64map(ArrayType typ, value64_type vt) {
    value64_type vt64 = arrayTypeGetInfo(typ)->vt64;
    if (vt64 == VALUE64_UNKNOWN)
        vt64 = vt;
    return vt64;
}

/**
 * @brief Gets the human-readable name of the array's type.
 * 
 * @param a The array instance.
 * @return const char* The pretty name (e.g., "INT").
 */
static inline const char            *arrayTypeGetName(ArrayType t) {
    return arrayTypeGetInfo(t)->name;
}

/**
 * @brief Gets the real name of the array's type.
 * 
 * @param a The array instance.
 * @return const char* The pretty name (e.g., "INT").
 */
static inline const char            *arrayTypeGetRealName(ArrayType t) {
    return arrayTypeGetInfo(t)->name_raw;
}

/**
 * @brief Retrieves the size of a single element for a given ArrayType.
 * 
 * @details This function is essential for calculating pointer offsets 
 *          during array slicing and resizing operations.
 * 
 * @param t The ArrayType enum value.
 * @return size_t The size of the element in bytes. Returns 0 if type is unknown.
 */
static inline size_t                arrayTypeGetElemSize(ArrayType t) {
    return arrayTypeGetInfo(t)->elem_size;
}

/**
 * @brief Converts a raw string representation of an ArrayType into its enum value.
 * 
 * @param name The string to parse (e.g., "ARRAY_INT").
 * @return ArrayType The corresponding enum value, or ARRAY_UNKNOWN if no match is found.
 */
static inline ArrayType               arrayTypeFromName(const char *name) {
    if (!name)
        return ARRAY_UNKNOWN;
    for (size_t i = 0; i < COUNT(ARRAY_TYPE_TABLE); i++) {
        if (strcmp(name, ARRAY_TYPE_TABLE[i].name_raw) == 0) {
            return ARRAY_TYPE_TABLE[i].type;
        }
    }
    return ARRAY_UNKNOWN;
}

/**
 * @brief Retrieves the human-readable name of an ArrayFillType.
 * 
 * @param t The ArrayFillType enum value.
 * @return const char* A pointer to the name string. Returns an empty string if not found.
 */
static inline const char           *arrayFillTypeGetName(ArrayFillType t) {
    for (size_t i = 0; i < COUNT(ARRAY_FILLTYPE_TABLE); i++) {
        if (ARRAY_FILLTYPE_TABLE[i].type == t)
            return ARRAY_FILLTYPE_TABLE[i].name;
    }
    return "";
}

// ------------- CONSTRUCTOTS/DESTRUCTORS --------------

// init
#define                         IArrayInit(...) (Array){.len = 0, .sz = 0, .iv = 0, .flags = ARRAY_INT, __VA_ARGS__}
#define                         LArrayInit(...) (Array){.len = 0, .sz = 0, .lv = 0, .flags = ARRAY_LONG, __VA_ARGS__}
#define                         DArrayInit(...) (Array){.len = 0, .sz = 0, .dv = 0, .flags = ARRAY_DOUBLE, __VA_ARGS__}
#define                         PArrayInit(...) (Array){.len = 0, .sz = 0, .iv = 0, .flags = ARRAY_POINTER, __VA_ARGS__}
#define                         CArrayInit(...) (Array){.len = 0, .sz = 0, .iv = 0, .flags = ARRAY_CHAR, __VA_ARGS__}
// this iv V64 container
#define                         V64ArrayInit(...) (Array){.len = 0, .sz = 0, .iv = 0, .flags = ARRAY_V64, __VA_ARGS__}
// 
#define                         ArrayInit(...)  (Array){.len = 0, .sz = 0, .iv = 0, .flags = 0, __VA_ARGS__}
#define                         ARRAYFREE(x)({ arrayFree((x)); (x) = NULL; })

// --------------------------------- CREATE and FILL ------------------------------------


/**
 * @brief Creates a new Array with specified capacity, filling pattern, and data type.
 * 
 * @details This function allocates memory for the Array descriptor and the 
 *          underlying data buffer. If successful, the caller takes ownership 
 *          of the returned pointer and must eventually call @ref ArrayFree.
 * 
 * @param cnt        Number of elements to allocate.
 * @param fill_type  The initialization pattern (e.g., zero, random, ascending).
 * @param data_type  The primitive data type to be stored (e.g., INT, DOUBLE).
 * @param v64_type   The specific value64 type (only applicable if data_type is ARRAY_V64).
 * 
 * @return Array* A pointer to the newly allocated Array, or NULL if memory 
 *         allocation failed or parameters are invalid.
 */
extern Array                   *arrayCreate(size_t cnt, ArrayFillType filltyp, ArrayType typ, value64_type vt);

/**
 * @brief Creates an Array without any initial filling pattern (initialized to NONE).
 * 
 * @param cnt      Number of elements to allocate.
 * @param data_type The primitive data type to be stored.
 * @param v64_type The specific value64 type (only applicable if data_type is ARRAY_V64).
 * 
 * @return Array* A pointer to the newly allocated Array, or NULL on error.
 */
static inline Array            *arrayOnlyCreate(size_t cnt, ArrayType typ, value64_type vt) {
    return arrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY, typ, vt);
}

/**
 * @brief Frees the memory associated with an Array and its contents.
 * 
 * @details This function must be called to prevent memory leaks. 
 *          It frees both the internal data buffer and the Array descriptor itself.
 *          Passing NULL to this function has no effect.
 * 
 * @param arr Pointer to the Array to be destroyed.
 */
extern void                    arrayFree(Array *val);

/* =========================================================
 * CONVENIENCE FACTORY FUNCTIONS (Typed Helpers)
 * ========================================================= */

/**
 * @brief Creates an integer array (ARRAY_INT).
 * @param cnt      Number of elements.
 * @param fill     Initialization pattern.
 * @return Array*  Pointer to the new Array, or NULL.
 */
static inline Array            *IarrayCreate(size_t cnt, ArrayFillType typ){
    return arrayCreate(cnt, typ, ARRAY_INT, VALUE64_UNKNOWN);
}

/**
 * @brief Creates a long array (ARRAY_LONG).
 * @param cnt      Number of elements.
 * @param fill     Initialization pattern.
 * @return Array*  Pointer to the new Array, or NULL.
 */
static inline Array            *LarrayCreate(size_t cnt, ArrayFillType typ){
    return arrayCreate(cnt, typ, ARRAY_LONG, VALUE64_UNKNOWN);
}

/**
 * @brief Creates a double array (ARRAY_DOUBLE).
 * @param cnt      Number of elements.
 * @param fill     Initialization pattern.
 * @return Array*  Pointer to the new Array, or NULL.
 */
static inline Array            *DarrayCreate(size_t cnt, ArrayFillType typ){
    return arrayCreate(cnt, typ, ARRAY_DOUBLE, VALUE64_UNKNOWN);
}

/**
 * @brief Creates a pointer array (ARRAY_POINTER).
 * @param cnt      Number of elements.
 * @param fill     Initialization pattern.
 * @return Array*  Pointer to the new Array, or NULL.
 */
static inline Array            *PArrayCreate(size_t cnt, ArrayFillType typ){
    return arrayCreate(cnt, typ, ARRAY_POINTER, VALUE64_UNKNOWN);
}

/**
 * @brief Creates a character array (ARRAY_CHAR).
 * @param cnt      Number of elements.
 * @param fill     Initialization pattern.
 * @return Array*  Pointer to the new Array, or NULL.
 */
static inline Array            *CarrayCreate(size_t cnt, ArrayFillType typ){
    return arrayCreate(cnt, typ, ARRAY_CHAR, VALUE64_UNKNOWN);
}

/**
 * @brief Creates a value64 container array (ARRAY_V64).
 * @param cnt        Number of elements.
 * @param fill       Initialization pattern.
 * @param v64_type   The specific v64 subtype.
 * @return Array*    Pointer to the new Array, or NULL.
 */
static inline Array             *V64ArrayCreate(size_t cnt, ArrayFillType typ, value64_type vt){
    return arrayCreate(cnt, typ, ARRAY_V64, vt);
}

// -------------- ACCESS AND MODIFICATION --------------

static inline bool                  arrayIsV64(const Array *a);
static inline bool                  arrayIschar(const Array *a);

static inline const ArrayTypeInfo   *arrayGetTypeInfo(const Array *parr) {
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "NUll input");
    return arrayTypeGetInfo(parr->flags);
}

static inline ArrayType             arrayGettype(const Array *parr) {
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Null input");

    return parr->flags & 0xFF;
}
// array wrapper over arrayTypeV64map
static inline value64_type          arrayGetV64mapType(const Array *parr) {
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Null array ptr");
    return arrayTypeV64map(arrayGettype(parr), parr->v64type);
}
// 
static inline const char           *arrayGetTypeName(const Array *parr) {
    return arrayTypeGetName(arrayGettype(parr));
}

static inline const char           *arrayGetTypeRealName(const Array *parr) {
    return arrayTypeGetRealName(arrayGettype(parr));
}

static inline const char           *arrayGetV64typeName(const Array *parr) {
    if (arrayIsV64(parr))
        return value64_typename(parr->v64type);    // query V64 API
    else
        return "NONV64_TYPE";
}

/// @brief check if array is INT
/// @param a array
/// @return true if INT
static inline bool                  arrayIsint(const Array *parr){
    return arrayGettype(parr) == ARRAY_INT;
}
/// @brief check if array is LONG
/// @param a array
/// @return true if LONG
static inline bool                  arrayIslong(const Array *parr){
    return arrayGettype(parr) == ARRAY_LONG;
} 
/// @brief check if array is DBL
/// @param a array
/// @return true if DBL
static inline bool                  arrayIsdouble(const Array *parr){
    return arrayGettype(parr) == ARRAY_DOUBLE;
}
/// @brief check if array is PTR
/// @param a array
/// @return true if PTR
static inline bool                  arrayIspointer(const Array *parr){
    return arrayGettype(parr) == ARRAY_POINTER;
}
/// @brief check if array is CHAR
/// @param a array
/// @return true if CHAR
static inline bool                  arrayIschar(const Array *parr){
    return arrayGettype(parr) == ARRAY_CHAR;
}
/// @brief check if array is VALUE64
/// @param a array
/// @return true if VALUE64
static inline bool                  arrayIsV64(const Array *parr){
    return arrayGettype(parr) == ARRAY_V64;
}

/// @brief check if array is valuuable
/// @param a array
/// @return true if ok 
static inline bool                  arrayIsvalid(const Array *parr){
    return ( parr != NULL && ( /*!(a.flags & ARRAY_ERROR) && */
             parr->flags &
            (ARRAY_INT | ARRAY_LONG | ARRAY_DOUBLE | ARRAY_POINTER | ARRAY_CHAR | ARRAY_V64
            ) ) > 0) && parr->sz >= parr->len && parr->v != 0;
}

/**
 * @brief Retrieves the size of a single element in bytes.
 * 
 * @details This function is a high-performance utility used for pointer 
 *          arithmetic and calculating memory offsets. It queries the 
 *          centralized metadata table to determine the element size 
 *          based on the array's type.
 * 
 * @param parr Pointer to the array instance.
 * @return size_t The size of one element in bytes.
 * 
 * @note This function assumes that the array type is valid and 
 *       matches a known entry in the metadata table.
 */
static inline size_t               arrayGetelemsize(const Array *parr) {
    invraisecode(ERR_NULLABLE_PTR, parr != NULL, "Null pointer");

    return arrayGetTypeInfo(parr)->elem_size;
}

/// @brief get array length (count of formatted values)
/// @param a array
/// @return count of formatted values
static inline size_t                arraylen(const Array *parr){
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Null input");

    return parr->len;
}

/// @brief get array min position (just v)
/// @param a array
/// @return count of formatted values
static inline const void           *arraymin(const Array *parr){
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Null input");

    return parr->v;
}
/// @brief get  v64 array max position (just v64)
/// @param a array
/// @return count of formatted values
static inline const value64        *arraymaxv64(const Array *parr){
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Null input");

    return parr->v64 + parr->len;
}
/// @brief get array size (total allocated values)
/// @param a array
/// @return total allocated values
static inline size_t               arraysz(const Array *parr){
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Null input");

    return parr->sz;
}
/// @brief get count of non-null pointers
/// @param a array
/// @return count of non-null pointers
static inline size_t               arrayGetPtrNotNull(const Array *parr){
    invraise(parr != NULL && arrayIspointer(parr), 
        "Applicable only for pointers ARRAY_POINTER %d", ARRAY_POINTER);
    size_t cnt = 0;
    for (size_t i = 0; i < parr->len; i++)
        cnt += parr->pv[i] != 0;
    return logsimpleret(cnt, "Total valuable elem %zu", cnt);
}
/**
 * @brief Checks whether two arrays are comparable (i.e. have the exact same type).
 *
 * For now the arrays must have exactly the same type (e.g. ARRAY_INT,
 * ARRAY_V64).  In the future a broader compatibility check may be introduced.
 *
 * @param arr1 pointer to the first array
 * @param arr2 pointer to the second array
 * @return true if the arrays are of the same type, false otherwise
 */
static inline bool                  arrayIsComparable(const Array *restrict arr1, const Array *restrict arr2) {
    // for now must be EXACTLy the same type
    return arrayGettype(arr1) == arrayGettype(arr2);
}
/**
 * @brief Checks whether two arrays are comparable (i.e. have the exact same type).
 *
 * For now the arrays must have exactly the same type (e.g. ARRAY_INT,
 * ARRAY_V64).  In the future a broader compatibility check may be introduced.
 *
 * @param arr1 pointer to the first array
 * @param arr2 pointer to the second array
 * @note raise ERR_TYPES_MISMATCH  if the arrays have diff types
 */
static inline ArrayType             arrayCheckComparable(const Array *restrict arr1, const Array *restrict arr2) {
    if (!arrayIsComparable(arr1, arr2) )
        userraiseint(ERR_TYPES_MISMATCH, "Type of arr1 %s and arr2 %s are not compatiple (equal for now)", 
            arrayGetTypeName(arr1), arrayGetTypeName(arr2) );   // different types -> not equal
    return arrayGettype(arr1);
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
extern bool                         arrayNoteq(const Array *restrict arr1, const Array *restrict arr2);
/**
 * @brief Checks whether two arrays are equal.
 *
 * Convenience wrapper around arrayNoteq().
 *
 * @param arr1 pointer to the first array
 * @param arr2 pointer to the second array
 * @return true if the arrays are equal, false otherwise
 */
static inline bool                  arrayEq(const Array *restrict arr1, const Array *restrict arr2) {
    arrayCheckComparable(arr1, arr2);        
    return !arrayNoteq(arr1, arr2);
}

/**
 * @brief Fills the entire array with values according to a fill type.
 *
 * @param arr  array (by value)
 * @param typ  fill type (e.g. ARRAY_FILLTYPE_ZERO, ARRAY_FILLTYPE_RND, …)
 * @return     number of filled elements (arr.len)
 */
extern long                      arrayFillAll(Array *restrict parr, ArrayFillType typ);

/**
 * @brief Fills a portion of an array with values according to a fill type.
 *
 * The interval [from, to) is filled.  The bounds are normalized if they
 * are out of range.
 *
 * @param a    array (by value)
 * @param typ  fill type
 * @param from start index (inclusive)
 * @param to   end index (exclusive)
 * @return     number of filled elements (to - from)
 */
extern long                       arrayFillRange(Array *parr, ArrayFillType typ, size_t from, size_t to);

/**
 * @brief Shrinks an array to the given size.
 *
 * Elements beyond `newsz` are freed (for owning types like FS/STR).
 * The length is updated accordingly.
 *
 * @param arr   array (by value)
 * @param newsz new size (must be non‑negative and ≤ current length)
 * @return      the shrunk array
 */
extern Array                       *arrayShrink(Array *parr, size_t newsz);

/**
 * @brief Increases the capacity of an array to accommodate at least `newcnt` elements.
 *
 * The array is resized and the newly added area is zero‑filled
 * (or filled with type‑appropriate empty values).
 *
 * @param arr    array (by value)
 * @param newcnt new minimum capacity
 * @return       the array with increased capacity
 */
extern Array                       *arrayIncrease(Array *parr, size_t newcnt);

/**
 * @brief Randomly shuffles the elements of an array using the Fisher‑Yates algorithm.
 *
 * Works for all array types (INT, LONG, DOUBLE, POINTER, CHAR, V64).
 *
 * @param arr array (by value)
 */
extern Array                       *arrayShuffle(Array *parr);

extern Array                       *arrayDel(Array *parr, size_t from, size_t cnt);

extern Array                       *arrayAdd(Array *parr, size_t from, size_t cnt, ArrayFillType ftyp);
/**
 * @brief Sorts the array in ascending or descending order.
 *
 * Uses the standard qsort() with type‑specific comparators.
 * For ARRAY_V64 the comparator is chosen according to the stored v64type.
 *
 * @param arr array (by value)
 * @param ord sort order (ARRAY_FILLTYPE_ASC or ARRAY_FILLTYPE_DESC)
 */
extern void                         arrayQsort(Array *parr, ArraySortType ord);
// ---------------------------- binary searchers --------------------------------
// generallized
extern long                       arrayBsearchCommon(const Array *parr, value64 val, bool acs);
/**
 * @brief Binary search for an integer in a sorted INT array.
 *
 * The array must be of type ARRAY_INT and sorted in ascending order.
 *
 * @param arr array (by value)
 * @param val value to search for
 * @return index of the found element (>=0), or -1 if not found
 */
static inline long                arrayBsearchIntCommon(const Array *parr, size_t val, bool acs) {
    if (!arrayIsint(parr))
        userraiseint(ERR_UNSUPPORTED_TYPE, "arrayBsearchInt requires ARRAY_INT");
    value64 v = value64_createint(val);
    return arrayBsearchCommon(parr, v, acs);
}
static inline long                arrayBsearchInt(const Array *parr, size_t val) {
    return arrayBsearchIntCommon(parr, val, true);
}
static inline long                arrayBsearchIntrev(const Array *parr, size_t val) {
    return arrayBsearchIntCommon(parr, val, false);
}
/**
 * @brief Binary search for a long in a sorted LONG array.
 *
 * The array must be of type ARRAY_LONG and sorted in ascending order.
 *
 * @param arr array (by value)
 * @param val value to search for
 * @return index of the found element (>=0), or -1 if not found
 */
static inline long                arrayBsearchLongCommon(const Array *parr, long val, bool acs) {
    if (!arrayIslong(parr))
        userraiseint(ERR_UNSUPPORTED_TYPE, "arrayBsearchLong requires ARRAY_LONG");
    value64 v = value64_createlong(val);
    return arrayBsearchCommon(parr, v, acs);
}
static inline long                arrayBsearchLong(const Array *parr, long val) {
    return arrayBsearchLongCommon(parr, val, true);
}
static inline long                arrayBsearchLongRev(const Array *parr, long val) {
    return arrayBsearchLongCommon(parr, val, false);
}
/**
 * @brief Binary search for a double in a sorted DOUBLE array.
 *
 * The array must be of type ARRAY_DOUBLE and sorted in ascending order.
 *
 * @param arr array (by value)
 * @param val value to search for
 * @return index of the found element (>=0), or -1 if not found
 */
static inline long               arrayBsearchDblCommon(const Array *parr, double val, bool acs) {
    if (!arrayIsdouble(parr))
        userraiseint(ERR_UNSUPPORTED_TYPE, "arrayBsearchDbl requires ARRAY_DOUBLE");
    value64 v = value64_createdbl(val);
    return arrayBsearchCommon(parr, v, acs);
}
static inline long                arrayBsearchDbl(const Array *parr, double val) {
    return arrayBsearchDblCommon(parr, val, true);
}
static inline long                arrayBsearchDblRev(const Array *parr, double val) {
    return arrayBsearchDblCommon(parr, val, false);
}
/**
 * @brief Binary search for a double in a sorted DOUBLE array.
 *
 * The array must be of type ARRAY_DOUBLE and sorted in ascending order.
 *
 * @param arr array (by value)
 * @param val value to search for
 * @return index of the found element (>=0), or -1 if not found
 */
static inline long                arrayBsearchCharCommon(const Array *parr, char val, bool acs) {
    if (!arrayIschar(parr))
        userraiseint(ERR_UNSUPPORTED_TYPE, "arrayBsearchChar requires ARRAY_CHAR");
    value64 v = value64_createchar(val);
    return arrayBsearchCommon(parr, v, acs);
}
static inline long                arrayBsearchChar(const Array *parr, char val) {
    return arrayBsearchCharCommon(parr, val, true);
}
static inline long                arrayBsearchCharRev(const Array *parr, char val) {
    return arrayBsearchCharCommon(parr, val, false);
}
/**
 * @brief Binary search for a value64 in a sorted V64 array.
 *
 * The array must be of type ARRAY_V64 and sorted in ascending or descending
 * order according to its v64type.
 *
 * @param arr array (by value)
 * @param val value to search for
 * @param asc true if array is sorted ascending, false if descending
 * @return index of the found element (>=0), or -1 if not found
 */
static inline long                arrayBsearchV64Common(const Array *parr, value64 val, bool acs) {
    if (!arrayIsV64(parr))
        userraiseint(ERR_UNSUPPORTED_TYPE, "arrayBsearchV64 requires ARRAY_V64");

    return arrayBsearchCommon(parr, val, acs);
}
static inline long                arrayBsearchV64(const Array *parr, value64 val) {
    return arrayBsearchV64Common(parr, val, true);
}
static inline long                arrayBsearchV64Rev(const Array *parr, value64 val) {
    return arrayBsearchV64Common(parr, val, false);
}
// -------------------------------------- foreach ---------------------------------------
// if condition is 0-ptr == ALL
extern long                         arrayForeach(Array *restrict parr, ArrayCond cond, ArrayProc func);
// if condition is 0-ptr == ALL
// TODO:
extern long                         arrayForeachRev(Array *restrict parr, ArrayCond cond, ArrayProc func);

//#define                       Array_apply(arr, condition, action)

// Общий макрос: p is arr.iv, len is arr.len
#define                         _Array_foreach_gen(p, len, elem) \
    for (typeof_unqual(*(p)) *_begin_ = (p), *(elem) = _begin_; \
         (elem) < _begin_ + (len); \
         ++(elem))
// reverse
 #define                         _Array_foreach_rev_gen(p, len, elem) \
    for (typeof_unqual(*(p)) *_begin_ = (p) + (len) - 1, *(elem) = _begin_; \
         (elem) >= _begin_ ; \
         --(elem))  
#define Array_pforeach_idx(parr, i) \
    for (size_t i = 0; i < (parr)->len; ++i) 
#define Array_pforeach_idx_rev(parr, i) \
    for (ssize_t i = (parr)->len - 1; i >= 0; --i)  


// Публичные однобуквенные макросы
#define IArray_foreach(parr, elem)   _Array_foreach_gen((parr)->iv, (parr)->len, elem)
#define LArray_foreach(parr, elem)   _Array_foreach_gen((parr)->lv, (parr)->len, elem)
#define DArray_foreach(parr, elem)   _Array_foreach_gen((parr)->dv, (parr)->len, elem)
#define PArray_foreach(parr, elem)   _Array_foreach_gen((parr)->pv, (parr)->len, elem)
#define V64Array_foreach(parr, elem) _Array_foreach_gen((parr)->v64, (parr)->len, elem)

// pump entry point
extern long                         arrayGenPumprange(Array *restrict parr, v64Gen *restrict gen, size_t from, size_t to);

/**
 * @brief Fills the entire v64 Array using a generator.
 * 
 * A convenience wrapper for @ref arrayGenPumprangeV64 that fills the array 
 * from index 0 to the end of the array.
 *
 * @param[in] parr Pointer to the destination Array (must be of type ARRAY_V64).
 * @param[in] gen  Pointer to the v64 generator.
 * 
 * @return The number of elements successfully written to the array.
 */
static inline long                  arrayGenPumpall(Array *restrict parr, v64Gen *restrict gen) {
    return arrayGenPumprange(parr, gen, 0, arraylen(parr));
}

// ----------------- PRINTERS/SERIALYZATION ----------------------

// -------------------------------------------- DS ------------------------------------------
extern long                         arraySaveToDS(DS *restrict out, Array *restrict parr);

extern long                         arrayfprint(FILE *restrict out, const Array *restrict parr, size_t limit);

static inline long                  arrayprint(const Array *parr, size_t limit){
    return arrayfprint(stdout, parr, limit);
}

extern long                         arraySaveFileByName(const Array *restrict parr, const char *restrict fname);
extern long                         arraySaveFile(FILE *restrict out, const Array *restrict parr);
extern Array                       *arrayLoadFileByName(const char *fname);
extern Array                       *arrayLoadFile(FILE *in);

/** 
 * @brief   Value-only Saver by delim for file
 * @note    Deprecated
 */
extern long                         arraySaveFilevalues(const Array *parr, const char *restrict fname, char delim);


/**
 * @brief Serializes the whole array into a fast‑string (fs).
 *
 * The resulting string can later be deserialized with arrayLoadFromfs().
 *
 * @param s   target fast‑string (will be modified)
 * @param arr array to serialize
 * @return    number of characters written, or -1 if `s` failed
 */

extern long                         arraySaveTofs(fs *restrict s, const Array *restrict parr);
/**
 * @brief Deserializes an array from a fs previously created by ArraySerialyze().
 *
 * @param s   source fs (not modified)
 * @param arr pointer to the array to fill or NULL (dump read)
 * @return    number of characters consumed, or -1 on error
 */
extern long                         arrayLoadFromfs(const fs *restrict s, Array *restrict parr); 


// ------------------ ETC. -------------------------

#endif /* !_ARRAY_H */

