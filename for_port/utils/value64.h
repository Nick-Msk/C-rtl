/**
 * @file value64.h
 * @brief Core data types and utility functions for a 64-bit discriminated union system.
 */

#ifndef _VALUE64_H
#define _VALUE64_H

// ---------------------------------------------------------------------------------
// ------------------------------ Public value64 API -------------------------------
// ---------------------------------------------------------------------------------

// ----------------------------- Includes ------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
//
#include "bool.h"
#include "log.h"
#include "common.h"
#include "error.h"
#include "checker.h"
#include "fs.h"
#include "fs_iter.h"
#include "numeric_ops.h"
#include "getword.h"
#include "fileutils.h"

// --------------------------------- CONSTANTS AND GLOBALS --------------------------

// ---------------------------------- TYPES -----------------------------------------

/**
 * @brief A 64-bit discriminated union representing various data types.
 * 
 * This union is designed to be used in conjunction with {@ref value64_type}.
 * Note that this union does not store the type tag itself; the type must 
 * be managed externally. ANY type must be inside uint64_t
 */
typedef union value64 {
        int                 ival;       /**< int */
        long                lval;       /**< long */
        unsigned long      ulval;       /**< unsigned long */
        double              dval;       /**< double */
        char               *sval;       /**< c-string */
        char                cval;       /**< single char! For Array compatibility */ 
        bool                bval;       /**< single char! For conf file */
        fs                 *fsval;      /**< fs string */
        FILE               *FILEval;    /**< FILE * - NO ANY ACTIONS for now, only storage the pointer! */
        // lwset              sw;       // TODO:
        void               *pval;       /**< Generic pointer representation */
        uint64_t            u64;        /**< Raw 64-bit representation (used for hashing) */
} value64;

/**
 * @brief Compile-time check to ensure the union is exactly 64 bits.
 */
_Static_assert(sizeof(value64) == sizeof(uint64_t),
               "value64 must be exactly as uint64_t");

 /**
 * @brief Enumeration of supported types for value64.
 */
typedef enum value64_type {
    VALUE64_UNKNOWN = 0,
    VALUE64_INT = 1,
    VALUE64_LONG,
    VALUE64_ULONG,
    VALUE64_DBL,
    VALUE64_PTR,
    VALUE64_CHR,
    VALUE64_BOOL,
    VALUE64_FS = 0x10,  // for memory-alloc types
    VALUE64_STR,
    VALUE64_FILE,
    VALUE64_TYPE_COUNT
} value64_type;

// not used for now
/* typedef enum value64_serialize_type {
    VALUE64_2STR,
    VALUE64_2JSON       // not implemented
} value64_serialize_type; not used for now */

/**
 * @brief Metadata container for a value64_type.
 */
typedef struct {
    const char  *name;
    size_t       size;
    bool         is_valid;
    const char  *type_desc;
} value64_typeinfo;

/**
 * @brief Retrieves the metadata associated with a specific value64_type.
 * @param typ The type to query.
 * @return Pointer to the corresponding value64_typeinfo structure.
 */
extern const                        value64_typeinfo* value64_info_get(value64_type typ);

/**
 * @brief Checks if a given type is registered and valid.
 * @param typ The type to check.
 * @return true if the type is valid, false otherwise.
 */
static inline bool                  value64_checktype(value64_type typ) {
    return value64_info_get(typ) != NULL;
}

/**
 * @brief Returns the string name of a given value64_type.
 * @param t The type to query.
 * @return The string name of the type.
 * @throws ERR_UNSUPPORTED_TYPE if the type is unknown.
 */
static inline const char            *value64_typename(value64_type t) {
    const value64_typeinfo* info = value64_info_get(t);
    if (!info)
        return userraise(NULL, ERR_UNSUPPORTED_TYPE, "Type %d not supported", t);
    return info->name;
}

/**
 * @brief Returns the detailed description of a value64_type.
 * @param t The type to query.
 * @return A human-readable description.
 * @throws ERR_UNSUPPORTED_TYPE if the type is unknown.
 */
static inline const char            *value64_typedesc(value64_type t) {
    const value64_typeinfo* info = value64_info_get(t);
    if (!info)
        return userraise(NULL, ERR_UNSUPPORTED_TYPE, "Type %d not supported", t);
    return info->type_desc;
}

/**
 * @brief Converts a string to a value64_type.
 * @param str The input string to parse.
 * @return The parsed value64_type.
 */
extern  value64_type                 value64_gettype(const char *str);
// only zero for now

/* --- Function Pointer Typedefs for Dispatch Tables --- */

/**
 * @brief Function pointer for value conversion (Copy Semantics).
 * @param v The source value.
 * @return The converted value.
 */
typedef value64                     (*value64_ConverterFunc)(value64 v);

/**
 * @brief Function pointer for conversion checker.
 * @param v The source value.
 * @return The true if conversion if possible
 */
typedef bool                        (*value64_ValidatorFunc)(value64);

/**
 * @brief Function pointer for value movement (Move Semantics).
 * @param v Pointer to the source value (will be modified/cleared).
 * @return The moved value.
 */
typedef value64                     (*value64_ConverterMoveFunc)(value64 *v);

/**
 * @brief Function pointer for comparing two value64 objects.
 * @param v1 First value.
 * @param v2 Second value.
 * @return Comparison result (standard comparator logic).
 */
typedef int                         (*value64_Comparator)(value64, value64);

/**
 * @brief Function pointer for pointer-based comparison (for qsort/bsearch).
 * @param v1 Pointer to the first value.
 * @param v2 Pointer to the second value.
 * @return Comparison result.
 */
typedef int                         (*value64_PComparator)(const void *restrict, const void *restrict);

/**
 * @brief Returns a pointer-based comparator for use in generic algorithms.
 * @param typ The type of the data.
 * @return Pointer to a value64_PComparator.
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized or invalid.
 */
extern value64_PComparator                 value64_getPComparator(value64_type typ);
/**
 * @brief Returns a reverse pointer-based comparator.
 * @param typ The type of the data.
 * @return Pointer to a reversed value64_PComparator.
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized or invalid.
 */
extern value64_PComparator                 value64_getPRevComparator(value64_type typ);
/**
 * @brief Returns a value-based comparator.
 * @param typ The type of the data.
 * @return Pointer to a value64_Comparator.
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized or invalid.
 */
 extern value64_Comparator                 value64_getComparator(value64_type typ);
 /**
 * @brief Returns a reverse value-based comparator.
 * @param typ The type of the data.
 * @return Pointer to a reversed value64_Comparator.
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized or invalid.
 */
extern value64_Comparator                  value64_getRevComparator(value64_type typ);

extern value64                             value64_convert_str_to_fs(value64 v);

/**
 * @name Initialization Macros
 * @brief Rapidly create value64 objects using compound literals.
 * 
 * These macros allow for fast initialization of value64 objects. 
 * Note that they perform no safety checks and assume the developer 
 * provides correct values.
 * @{
 */

/** 
 * @brief Returns a zero-initialized value64 object. 
 */
#define                             LITERAL64_ZERO      (value64) {.u64 = 0L }
/** @brief Creates a value64 object representing an integer. */
#define                             LITERAL64_INT(val)  (value64) {.u64 = 0L, .ival = val }
/** @brief Creates a value64 object representing a long. */
#define                             LITERAL64_LONG(val)  (value64) {.u64 = 0L, .lval = val }
/** @brief Creates a value64 object representing a unsigned long. */
#define                             LITERAL64_ULONG(val)  (value64) {.u64 = 0L, .ulval = val }
/** 
 * @brief Creates a value64 object representing a character. 
 */
#define                             LITERAL64_CHR(val)  (value64) {.u64 = 0L, .cval = val }
/** 
 * @brief Creates a value64 object representing a bool. 
 */
#define                             LITERAL64_BOOL(val)  (value64) {.u64 = 0L, .bval = val }
/** @brief Creates a value64 object representing a double. */
#define                             LITERAL64_DBL(val)  (value64) {.u64 = 0L, .dval = val }
/** 
 * @brief Creates a value64 object representing a pointer. 
 * @param val A pointer to be stored in the object.
 */
#define                             LITERAL64_PTR(val)  (value64) {.u64 = 0L, .pval = val }
/** 
 * @brief Creates a value64 object from a C-string pointer.
 * @warning This macro performs a pointer copy. It does NOT duplicate the string.
 * @param val A pointer to a null-terminated string.
 */
#define                             LITERAL64_STR(val)  (value64) {.u64 = 0L, .sval = (char *) (val) }
/** 
 * @warning This macro takes the address of the 
 *          passed argument, which is often a local stack variable. 
 *          Using the resulting value64 outside the scope of the argument 
 *          will lead to undefined behavior (dangling pointer).
 * @param val The address of a filesystem object.
 */
#define                             LITERAL64_FS(val)   (value64) {.u64 = 0L, .fsval = &(val) }
/** 
 * @brief Creates a value64 object containing a pointer to a filesystem object.
 * @param val A pointer to the filesystem object.
 */
#define                             LITERAL64_PFS(val)   (value64) {.u64 = 0L, .fsval = (val) }
/** 
 * @brief Creates a value64 object from a string literal by wrapping it in an `fs` object.
 * @param str The string to be wrapped.
 */
#define                             LITERAL64_FS_STR(str) (value64) {.u64 = 0L, .fsval = &FSLITERAL(str) }
/** 
 * @brief Creates a value64 object representing a FILE *. 
 * @param val A pointer to be stored in the object.
 * @note No any action except saving the pointer
 */
#define                             LITERAL64_FILE(val)  (value64) {.u64 = 0L, .FILEval = val }

/** @} */

/**
 * @brief Resets a value64 object to a zero state.
 * 
 * @note This is a low-level, high-speed reset. It does not perform type validation 
 *       and assumes the provided type is correct for the underlying bits.
 * 
 * @param pv Pointer to the value64 object to reset.
 * @param typ The type of the object, used to handle special cases like double.
 * @return The reset value64 object.
 */
static inline value64               value64_setzero(value64 *pv, value64_type typ){
    switch (typ){
        case VALUE64_DBL:
            return *pv = LITERAL64_DBL(0.0);
        default:
            return *pv = LITERAL64_ZERO;
    }
}

// ------------------------- CONSTRUCTOTS/DESTRUCTORS -------------------------------

/**
 * @name Constructors and Memory Management
 * @brief Functions for creating, cloning, and destroying value64 objects.
 * @{
 */

/**
 * @name Mass Creation API (Pointer-based)
 * @brief High-level constructors for creating objects from existing memory.
 * @{
 */

/**
 * @brief Creates a value64 object from a pointer, with optional move semantics. 
 *          The part of mass creation API
 * 
 * @param p    Pointer to the source data.
 * @param typ  The type of the source data.
 * @param move If true, transfers ownership (move semantics). If false, performs a copy.
 * @return A new value64 object.
 */
extern value64                      value64_pcopy_move(void *p, value64_type typ, bool move);

/**
 * @brief Performs a copy-initialization from a pointer.
 * @param p Pointer to the source data.
 * @param typ The type of the source data.
 * @return A new value64 object (copy).
 */
static inline value64               value64_pinit(const void *p, value64_type typ){
    return value64_pcopy_move( (void *) p, typ, false);
}
/**
 * @brief Performs a move-initialization from a pointer.
 * @param p Pointer to the source data.
 * @param typ The type of the source data.
 * @return A new value64 object (ownership transferred).
 */
static inline value64               value64_pmove(void *p, value64_type typ){
    return value64_pcopy_move(p, typ, true);
}
/** @} */

/**
 * @name Primitive Value Constructors
 * @brief Rapidly creates value64 objects from immediate values.
 * @{
 */

/** @brief Creates an integer-typed value. */
static inline value64               value64_createint(int val){
    value64 tmp = LITERAL64_ZERO;
    tmp.ival = val;
    return tmp;
}
/** @brief Creates a long-typed value. */
static inline value64               value64_createlong(long lval){
    value64 tmp = LITERAL64_ZERO;
    tmp.lval = lval;
    return tmp;
}
/** @brief Creates a unsigned long-typed value. */
static inline value64               value64_createulong(unsigned long ulval){
    value64 tmp = LITERAL64_ZERO;
    tmp.lval = ulval;
    return tmp;
}
/** @brief Creates a character-typed value. */
static inline value64               value64_createchar(char cval){
    value64 tmp = LITERAL64_ZERO;
    tmp.cval = cval;
    return tmp;
}
/** @brief Creates a bool-typed value. */
static inline value64               value64_createbool(bool bval){
    value64 tmp = LITERAL64_ZERO;
    tmp.bval = bval;
    return tmp;
}
/** @brief Creates a double-typed value. */
static inline value64               value64_createdbl(double dval){
    value64 tmp = LITERAL64_ZERO;
    tmp.dval = dval;
    return tmp;
}
/** @brief Creates a pointer-typed value. */
static inline value64               value64_createptr(void *pval){
    value64 tmp = LITERAL64_ZERO;
    tmp.pval = pval;
    return tmp;
}
/** @brief Creates a FILE *-typed value. */
static inline value64               value64_createFILE(FILE *fileval){
    value64 tmp = LITERAL64_ZERO;
    tmp.FILEval = fileval;
    return tmp;
}

// TODO: movestr?
/**
 * @brief Creates a string-typed value via deep copy (strdup).
 * @warning This function allocates memory on the heap.
 * @param sval The source C-string.
 * @return A new value64 object containing a copy of the string.
 */
static inline value64               value64_createstr(const char *sval){
    if (!sval)
        userraiseint(ERR_NULLABLE_PTR, "Null pointer");
    value64 tmp = LITERAL64_ZERO;
    if ( (tmp.sval = strdup(sval) ) == NULL)
        userraiseint(ERR_UNABLE_ALLOCATE, "Unable to dup c-string (%.20s)", sval);
    return tmp;
}
/**
 * @brief Creates an fs value from a filesystem resource.
 * @param fsval Pointer to the source filesystem resource.
 * @return A new value64 object owning a copy of the FS resource.
 */
static inline value64               value64_createfs(const fs *fsval){
    if (!fsval)
        userraiseint(ERR_NULLABLE_PTR, "Null pointer fs %p or fs->v %p", fsval, fsval ? fsval->v: NULL);

    value64 tmp = LITERAL64_ZERO;
    if (fsval->v){
        if ( (tmp.fsval = fs_heapcreate(fsval) ) == NULL)
            userraiseint(ERR_UNABLE_ALLOCATE, "Unable to dup fs");
    }
    return tmp;
}
/**
 * @brief Convenience constructor: creates an FS object from a C-string.
 * @param str The source string.
 * @return A value64 object holding an FS resource containing the string.
 */
static inline value64               value64_createfs_asstr(const char *str) {
    if (!str)
        return LITERAL64_FS( FS()); // null fs^ but with heap alloc
    value64 v = value64_createstr(str);

    // forward decl
    extern value64                     value64_convert_move_str_to_fs(value64 *v);

    return value64_convert_move_str_to_fs(&v);  //return value64_convert_str_to_fs(v);
}

/** @} */

/**
 * @name Cloning and Destructors
 * @brief Functions for duplicating and deallocating value64 objects.
 * @{
 */

/**
 * @brief Performs a deep copy (clone) of a value64 object.
 * @param source The source value64 object.
 * @param typ    The type of the object to clone.
 * @return A new value64 object with the same data.
 */
static inline value64               value64_clone(value64 source, value64_type typ){
    switch (typ){
        case VALUE64_INT:
            return value64_createint(source.ival);
        case VALUE64_LONG:
            return value64_createlong(source.lval);
        case VALUE64_ULONG:
            return value64_createulong(source.ulval);
        case VALUE64_DBL:
            return value64_createdbl(source.dval);
        case VALUE64_PTR:
            return value64_createptr(source.pval);
        case VALUE64_CHR:
            return value64_createchar(source.cval);
        case VALUE64_BOOL:
            return value64_createbool(source.bval);
        case VALUE64_FS:
            return value64_createfs(source.fsval);
        case VALUE64_STR:
            return value64_createstr(source.sval);
        case VALUE64_FILE:
            userraiseint(ERR_UNSUPPORTED_TYPE, "VALUE64_FILE can't be cloned! Use FILE * specific API");
        default:
            return LITERAL64_ZERO;
    }
}

/**
 * @brief Move constructor for filesystem resources.
 * 
 * This function transfers ownership of an existing `fs` resource into a 
 * new `value64` object by moving the resource to the heap. 
 * 
 * @note This is a destructive operation for the input `fsval`. Once moved, 
 *       the resource is managed by the returned `value64` object.
 * 
 * @param fsval Pointer to the `fs` resource to be moved.
 * @return A `value64` object containing the moved filesystem resource.
 * @throws ERR_UNABLE_ALLOCATE if the heap allocation fails.
 */
static inline value64               value64_movefs(fs *fsval){
    value64 tmp = LITERAL64_ZERO;
    if ( (tmp.fsval = fs_moveto_heap( (fs *) fsval) ) == NULL)
        userraiseint(ERR_UNABLE_ALLOCATE, "Unable to dup fs");
    return tmp;
}
/**
 * @brief Destructor for filesystem resources.
 * @param v Pointer to the value64 object containing an FS resource.
 */
static inline void                  value64_freefs(value64 *v){
    fs_free(v->fsval);
    v->fsval = 0;
}
/**
 * @brief Destructor for string resources.
 * @param v Pointer to the value64 object containing a string.
 */
static inline void                  value64_freestr(value64 *v){
    free(v->sval);
    v->sval = NULL;
}
/**
 * @brief Generic destructor for value64 objects.
 * @details This function only performs deallocation if the type is 
 *          a resource type (STR or FS). Primitives are ignored.
 * @param v Pointer to the value64 object to destroy.
 * @param typ The type of the object.
 */
static inline void                  value64_free(value64 *v, value64_type typ){
    switch (typ){
        case VALUE64_STR:
            value64_freestr(v);
            break;
        case VALUE64_FS:
            value64_freefs(v);   // even if NULL
            break;
        case VALUE64_FILE:
            // NOTING TO DO!!! for close file use FILE * API
            break;
        default:
            break;
    }
}

/** @} */

#define value64freefs(v)        value64_freefs(&(v))
#define value64freestr(v)       value64_freestr(&(v))
#define value64free(v, typ)     value64_free(&(v), typ)

// -------------------- ACCESS AND MODIFICATORS -------------------------------------
/**
 * @name Accessors (Getters)
 * @brief High-speed accessors to extract values from a value64 object.
 * 
 * These functions provide direct access to the underlying members of the 
 * value64 union. They are extremely fast (static inline) but assume that 
 * the caller knows the correct type of the object.
 * 
 * @warning Accessing a member that does not match the actual type of the 
 *          value64 object will result in logically incorrect data.
 * @{
 */

/**
 * @brief Returns the integer value.
 * @return The integer representation.
 */
static inline int                   value64_int(value64 v){
    return v.ival;
}
/**
 * @brief Returns the long integer value.
 * @return The long representation.
 */
static inline long                  value64_long(value64 v){
    return v.lval;
}
/**
 * @brief Returns the long integer value.
 * @return The long representation.
 */
static inline unsigned long         value64_ulong(value64 v){
    return v.ulval;
}
/**
 * @brief Returns the character value.
 * @return The character value.
 */
static inline char                  value64_char(value64 v){
    return v.cval;
}
/**
 * @brief Returns the bool value.
 * @return The character value.
 */
static inline bool                  value64_bool(value64 v){
    return v.bval;
}
/**
 * @brief Returns the double precision value.
 * @return The double representation.
 */
static inline double                value64_dbl(value64 v){
    return v.dval;
}
/**
 * @brief Returns the generic pointer value.
 * @return The pointer representation.
 */
static inline void                 *value64_ptr(value64 v){
    return v.pval;
}
/**
 * @brief Returns the pointer to the C-string.
 * @return Pointer to the string buffer.
 */
static inline char                 *value64_str(value64 v){
    return v.sval;
}
/**
 * @brief Returns the pointer to the fs.
 * @return Pointer to the fs.
 */
static inline fs                   *value64_fs(value64 v){
    return v.fsval;
}
/**
 * @brief Returns the pointer to the c-string in fs.
 * @return Pointer to the c-str.
 */
static inline const char           *value64_fsstr(value64 v){
    return v.fsval->v;
}
/**
 * @brief Returns the pointer to the FILE *
 * @return Pointer to the FILE object.
 */
static inline const FILE           *value64_FILE(value64 v){
    return v.FILEval;
}

// ------------------------------------------------------------------------------------------------

/// @brief  exchanger
/// @param v1 pointer to first v64
/// @param v2 pointer to second v64
/// @note   this is low level function, no NULL pointers check!
static inline void                 v64_exch(value64 *restrict v1, value64 *restrict v2) {
    value64 tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}

// move constructor!
static inline  value64              value64_move(value64 *restrict source, value64_type typ) {
    invraisecode(source,  ERR_NULLABLE_PTR, "Null pointer");

    value64 res = *source;      // just a move!!! For all types
    switch (typ) {
        case VALUE64_DBL:
            *source = LITERAL64_DBL(0.0);
        break;
        default:
            *source = LITERAL64_ZERO;
        break;
    }
    return res;
}

// move to EXISTING object, thart is NOT a constructor
// move switcher to EXISTING object
extern value64                     *value64_moveto(value64 *restrict target, value64 *restrict source, value64_type typ);
static inline value64              *value64_moveto_int(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_INT);
}
static inline value64              *value64_moveto_long(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_LONG);
}
static inline value64              *value64_moveto_ulong(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_ULONG);
}
static inline value64              *value64_moveto_dbl(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_DBL);
}
static inline value64              *value64_moveto_chr(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_CHR);
}
static inline value64              *value64_moveto_bool(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_BOOL);
}
static inline value64              *value64_moveto_ptr(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_PTR);
}
static inline value64              *value64_moveto_str(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_STR);
}
static inline value64              *value64_moveto_fs(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_FS);
}
static inline value64              *value64_moveto_FILE(value64 *restrict target, value64 *restrict source){
    return value64_moveto(target, source, VALUE64_FILE);
}

extern unsigned long                value64_lhash(value64 value, value64_type typ);

// exchanger
extern void                         value64_exch(value64 *v1, value64 *v2);
// -------------------------------- Sorting/searching ------------------------------------------------
extern void                         value64_sort(value64_type typ, value64 *arr, int sz);
extern void                         value64_revsort(value64_type typ, value64 *arr, int sz);

// Сортировка по возрастанию для конкретных типов, except VALUE64_FILE
static inline void                  value64_sort_int(value64 *arr, int sz) {
    value64_sort(VALUE64_INT, arr, sz);
}
static inline void                  value64_sort_long(value64 *arr, int sz) {
    value64_sort(VALUE64_LONG, arr, sz);
}
static inline void                  value64_sort_char(value64 *arr, int sz) {
    value64_sort(VALUE64_CHR, arr, sz);
}
static inline void                  value64_sort_bool(value64 *arr, int sz) {
    value64_sort(VALUE64_BOOL, arr, sz);
}
static inline void                  value64_sort_dbl(value64 *arr, int sz) {
    value64_sort(VALUE64_DBL, arr, sz);
}
static inline void                  value64_sort_ptr(value64 *arr, int sz) {
    value64_sort(VALUE64_PTR, arr, sz);
}
static inline void                  value64_sort_str(value64 *arr, int sz) {
    value64_sort(VALUE64_STR, arr, sz);
}
static inline void                  value64_sort_fs(value64 *arr, int sz) {
    value64_sort(VALUE64_FS, arr, sz);
}

// Сортировка по убыванию для конкретных типов except VALUE64_FILE
static inline void                  value64_revsort_int(value64 *arr, int sz) {
    value64_revsort(VALUE64_INT, arr, sz);
}
static inline void                  value64_revsort_long(value64 *arr, int sz) {
    value64_revsort(VALUE64_LONG, arr, sz);
}
static inline void                  value64_revsort_char(value64 *arr, int sz) {
    value64_revsort(VALUE64_CHR, arr, sz);
}
static inline void                  value64_revsort_bool(value64 *arr, int sz) {
    value64_revsort(VALUE64_BOOL, arr, sz);
}
static inline void                  value64_revsort_dbl(value64 *arr, int sz) {
    value64_revsort(VALUE64_DBL, arr, sz);
}
static inline void                  value64_revsort_ptr(value64 *arr, int sz) {
    value64_revsort(VALUE64_PTR, arr, sz);
}
static inline void                  value64_revsort_str(value64 *arr, int sz) {
    value64_revsort(VALUE64_STR, arr, sz);
}
static inline void                  value64_revsort_fs(value64 *arr, int sz) {
    value64_revsort(VALUE64_FS, arr, sz);
}

extern int                          value64_search(value64 val, value64_type typ, const value64 *arr, int sz);
extern int                          value64_revsearch(value64 val, value64_type typ, const value64 *arr, int sz);
// must be sorted acs
extern int                          value64_binsearch(value64 val, value64_type typ, const value64 *arr, int sz);
// order desc
extern int                          value64_rev_binsearch(value64 val, value64_type typ, const value64 *arr, int sz);
// SQL in low level
static inline bool                  value64_notin(value64 val, value64_type typ, const value64 *arr, int sz){
    return value64_search(val, typ, arr, sz) == -1;
}
static inline bool                  value64_in(value64 val, value64_type typ, const value64 *arr, int sz){
    return value64_search(val, typ, arr, sz) >= 0;
}

/**
 * @name Value Comparison API
 * @brief High-level and low-level comparison functions for value64 objects.
 * @{
 */

/**
 * @brief Generic comparison of two value64 objects.
 * @param v1 First value.
 * @param v2 Second value.
 * @param typ The type to use for comparison.
 * @return An integer ( <0 if v1 < v2, 0 if v1 == v2, >0 if v1 > v2 ).
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized.
 */
extern int                          value64_compare(value64 v1, value64 v2, value64_type typ);
/**
 * @brief Checks if two value64 objects are equal.
 * @param v1 First value.
 * @param v2 Second value.
 * @param typ The type to compare.
 * @return true if v1 == v2, false otherwise.
 */
static inline bool                  value64_equal(value64 v1, value64 v2, value64_type typ) {
    return value64_compare(v1, v2, typ) == 0;
}

/** @name Specialized Value Comparators (Direct)
 *  These functions perform direct comparison of the union members.
 *  @{ */
extern int                          value64_int_comp(value64 v1, value64 v2);
extern int                          value64_long_comp(value64 v1, value64 v2);
extern int                          value64_ulong_comp(value64 v1, value64 v2);
extern int                          value64_char_comp(value64 v1, value64 v2);
extern int                          value64_bool_comp(value64 v1, value64 v2);
extern int                          value64_dbl_comp(value64 v1, value64 v2);
extern int                          value64_ptr_comp(value64 v1, value64 v2);
// memory alloc types
extern int                          value64_fs_comp(value64 v1, value64 v2);
// compare fs vs provided c-str
extern int                          value64_fs_compstr(value64 v1, const char *str);
extern int                          value64_str_comp(value64 v1, value64 v2);
/** @} */

/**
 * @name Reverse Comparison API
 * @brief Functions for descending order comparison.
 * @{
 */

/** @name Reverse Value Comparators (Direct) */
/** @{ */
extern int                          value64_int_rev_comp(value64 v1, value64 v2);
extern int                          value64_long_rev_comp(value64 v1, value64 v2);
extern int                          value64_ulong_rev_comp(value64 v1, value64 v2);
extern int                          value64_char_rev_comp(value64 v1, value64 v2);
extern int                          value64_bool_rev_comp(value64 v1, value64 v2);
extern int                          value64_dbl_rev_comp(value64 v1, value64 v2);
extern int                          value64_ptr_rev_comp(value64 v1, value64 v2);
// memory alloc types
extern int                          value64_fs_rev_comp(value64 v1, value64 v2);
extern int                          value64_str_rev_comp(value64 v1, value64 v2);
// pointer comparator
extern int                          value64_pt_compare(const value64 *restrict v1, const value64 *restrict v2, value64_type typ);

/** @} */

/** @name Pointer-based Comparators (Standard Library Compatible)
 *  Designed for use with `qsort`, `bsearch`, etc.
 *  @{ */
extern int                          value64_pint_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_plong_comp(const void *restrict v1, const void *restrict v2);
extern int                          value64_pulong_comp(const void *restrict v1, const void *restrict v2);
extern int                          value64_pchar_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_pbool_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_pdbl_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_pptr_comp (const void *restrict v1, const void *restrict v2);
//
extern int                          value64_pstr_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_pfs_comp  (const void *restrict v1, const void *restrict v2);
/** @} */

/** @name Reverse Pointer Comparators (Standard Library Compatible) */
/** @{ */
extern int                          value64_pint_rev_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_plong_rev_comp(const void *restrict v1, const void *restrict v2);
extern int                          value64_pulong_rev_comp(const void *restrict v1, const void *restrict v2);
extern int                          value64_pchar_rev_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_pbool_rev_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_pdbl_rev_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_pptr_rev_comp (const void *restrict v1, const void *restrict v2);
//
extern int                          value64_pstr_rev_comp (const void *restrict v1, const void *restrict v2);
extern int                          value64_pfs_rev_comp  (const void *restrict v1, const void *restrict v2);
/** @} */

/** @} */

// ----------------------------- CONVERTERS ----------------------------------------
/**
 * @name General Interface
 * @brief High-level functions for checking and performing conversions.
 * @{
 */

// check if we've copy transision method
extern value64_ConverterFunc       value64_is_copyconverted(value64_type from, value64_type to);
// check if we've transision MOVE method
extern value64_ConverterMoveFunc   value64_is_moveconverted(value64_type from, value64_type to); 

/**
 * @brief Generic conversion function (Copy Semantics).
 * 
 * Converts a value from the specified 'from' type to the 'to' type.
 * This function performs a copy, leaving the source value intact.
 * 
 * @param v    The source value64 object.
 * @param from The source type.
 * @param to   The target type.
 * @return The converted value64 object.
 */
extern value64                  value64_convert_common(value64 v, value64_type from, value64_type to, bool check);

static inline value64           value64_convert(value64 v, value64_type from, value64_type to) {
    return value64_convert_common(v, from, to, true);
}
static inline value64           value64_convert_nocheck(value64 v, value64_type from, value64_type to) {
    return value64_convert_common(v, from, to, false);
}

/**
 * @brief Checks if a conversion between two types is valid.
 * @param v    The source value64 object.
 * @param from The source type.
 * @param to   The target type.
 * @return true if conversion is supported, false otherwise.
 */
extern bool                      value64_is_convertable(value64 v, value64_type from, value64_type to);
/** @} */


// --- Group INT ---
/**
 * @name Conversion Matrix (Copy Semantics)
 * @brief Specialized functions for converting from a specific source type.
 * @details These functions are non-destructive.
 * @{
 */

/** @name Integer Conversions */
/** @{ */
extern value64                     value64_convert_int_to_int(value64 v);
extern value64                     value64_convert_int_to_lng(value64 v);
extern value64                     value64_convert_int_to_ulong(value64 v);
extern value64                     value64_convert_int_to_dbl(value64 v);
extern value64                     value64_convert_int_to_char(value64 v);
extern value64                     value64_convert_int_to_bool(value64 v);
extern value64                     value64_convert_int_to_fs(value64 v);
extern value64                     value64_convert_int_to_str(value64 v);
/** @} */

// --- Group LNG ---
/** @name Long Conversions */
/** @{ */
extern value64                     value64_convert_lng_to_int(value64 v);
extern value64                     value64_convert_lng_to_lng(value64 v);
extern value64                     value64_convert_lng_to_ulong(value64 v);
extern value64                     value64_convert_lng_to_dbl(value64 v);
extern value64                     value64_convert_lng_to_char(value64 v);
extern value64                     value64_convert_lng_to_bool(value64 v);
extern value64                     value64_convert_lng_to_fs(value64 v);
extern value64                     value64_convert_lng_to_str(value64 v);
/** @} */

// --- Group ULONG ---
/** @name Unsigned Long Conversions */
/** @{ */
extern value64                     value64_convert_ulong_to_int(value64 v);
extern value64                     value64_convert_ulong_to_lng(value64 v);
extern value64                     value64_convert_ulong_to_ulong(value64 v);
extern value64                     value64_convert_ulong_to_dbl(value64 v);
extern value64                     value64_convert_ulong_to_char(value64 v);
extern value64                     value64_convert_ulong_to_bool(value64 v);
extern value64                     value64_convert_ulong_to_fs(value64 v);
extern value64                     value64_convert_ulong_to_str(value64 v);
/** @} */

// --- Group DBL ---
/** @name Double Conversions */
/** @{ */
extern value64                     value64_convert_dbl_to_int(value64 v);
extern value64                     value64_convert_dbl_to_lng(value64 v);
extern value64                     value64_convert_dbl_to_ulong(value64 v);
extern value64                     value64_convert_dbl_to_dbl(value64 v);
extern value64                     value64_convert_dbl_to_fs(value64 v);
extern value64                     value64_convert_dbl_to_str(value64 v);
//dbl => char NO convert 
//dbl => bool NO convert 

/** @} */

// --- Group CHR ---
/** @name Character Conversions */
/** @{ */
extern value64                     value64_convert_char_to_int(value64 v);
extern value64                     value64_convert_char_to_lng(value64 v);
extern value64                     value64_convert_char_to_ulong(value64 v);
extern value64                     value64_convert_char_to_char(value64 v);
extern value64                     value64_convert_char_to_bool(value64 v);
extern value64                     value64_convert_char_to_fs(value64 v);
extern value64                     value64_convert_char_to_str(value64 v);
// no convert char to dbl
/** @} */

// --- Group BOOL ---
/** @name Boolean Conversions */
/** @{ */
extern value64                     value64_convert_bool_to_int(value64 v);
extern value64                     value64_convert_bool_to_lng(value64 v);
extern value64                     value64_convert_bool_to_ulong(value64 v);
extern value64                     value64_convert_bool_to_char(value64 v);
extern value64                     value64_convert_bool_to_bool(value64 v);
extern value64                     value64_convert_bool_to_fs(value64 v);
extern value64                     value64_convert_bool_to_str(value64 v);
/** @} */

/** @name fs Conversions */
/** @{ */
// --- Group FS ---
extern value64                     value64_convert_fs_to_int(value64 v);
extern value64                     value64_convert_fs_to_lng(value64 v);
extern value64                     value64_convert_fs_to_ulong(value64 v);
extern value64                     value64_convert_fs_to_char(value64 v);
extern value64                     value64_convert_fs_to_bool(value64 v);
extern value64                     value64_convert_fs_to_dbl(value64 v);
extern value64                     value64_convert_fs_to_str(value64 v);
extern value64                     value64_convert_fs_to_fs(value64 v);
/** @} */

/** @name c-String Conversions */
/** @{ */
// --- Group STR ---
extern value64                     value64_convert_str_to_int(value64 v);
extern value64                     value64_convert_str_to_lng(value64 v);
extern value64                     value64_convert_str_to_ulong(value64 v);
extern value64                     value64_convert_str_to_char(value64 v);
extern value64                     value64_convert_str_to_bool(value64 v);
extern value64                     value64_convert_str_to_dbl(value64 v);
extern value64                     value64_convert_str_to_str(value64 v);
extern value64                     value64_convert_str_to_fs(value64 v);
/** @} */
/** @} */

/**
 * @name Move Semantics (Destructive)
 * @brief High-performance conversions that transfer ownership.
 * 
 * @warning These functions are DESTRUCTIVE. The source object (passed by pointer) 
 *          will be modified or cleared to prevent double-frees and ensure 
 *          efficient resource transfer.
 * @{
 */

/**
 * @brief Generic move-conversion.
 * @param source Pointer to the source object (will be modified).
 * @param from   Source type.
 * @param to     Target type.
 * @return The moved value64 object.
 */
extern value64                      value64_convert_move(value64 *source, value64_type from, value64_type to);

/** @name Specialized Move Conversions */
/** @{ */

extern value64                     value64_convert_move_fs_to_str(value64 *v);
extern value64                     value64_convert_move_fs_to_fs(value64 *v);
extern value64                     value64_convert_move_str_to_fs(value64 *v);
extern value64                     value64_convert_move_str_to_str(value64 *v);
/** @} */

/** @} */

// ------------------------ PRINTERS/CHECKERS ---------------------------------------

extern int                          value64_fprint_msg(FILE *restrict out, const char *restrict msg, value64 val, value64_type typ);
// generic file serilization!!
static inline  int                  value64_fprint(FILE *restrict out, value64 val, value64_type typ) {
    return value64_fprint_msg(out, NULL, val, typ);
}
static inline void                  value64_log(value64 val, value64_type typ) {
    value64_fprint(logfile, val, typ);
}
static inline void                  value64_print(value64 val, value64_type typ) {
    value64_fprint(stdout, val, typ);
}
/**
 * @brief technical printer
 *
 * @param out stream, opened for write
 * @param val the value64
 * @param typ type of value64
 */
extern int                          value64_techfprint(FILE *restrict out, value64 val, value64_type typ, const char *restrict name);
static inline int                   value64_techprint(value64 val, value64_type typ, const char *restrict name) {
    return value64_techfprint(stdout, val, typ, name);
}
#define VALUE64_TECHFPRINT(out, val, typ)   value64_techfprint( (out), (val), (typ), #val)
#define VALUE64_TECHPRINT(val, typ)         value64_techprint((val), (typ), #val)

// Write adapters, into FILE *  => NEED TO BE REFACTORE TO USE DS (???)
extern int                          value64_fprint_int  (FILE *out, value64 val);
extern int                          value64_fprint_long  (FILE *out, value64 val);
extern int                          value64_fprint_ulong(FILE *out, value64 val);
extern int                          value64_fprint_char (FILE *out, value64 val);
extern int                          value64_fprint_bool (FILE *out, value64 val);
extern int                          value64_fprint_dbl  (FILE *out, value64 val);
extern int                          value64_fprint_ptr  (FILE *out, value64 val);
//
extern int                          value64_fprint_fs   (FILE *out, value64 val);
extern int                          value64_fprint_str  (FILE *out, value64 val);
extern int                          value64_fprint_FILE (FILE *out, value64 val);

// --------------------------------- SERIALIZATION ----------------------------------

// string readers!
// fs must be initialized, val can be NULL, it means just check
// Read adapteds (from fs) => NEED TO BE REFACTORE TO USE DS

extern bool                         value64_sreadval_int(value64 *restrict val, fs *restrict buf);
extern bool                         value64_sreadval_long(value64 *restrict val, fs *restrict buf);
extern bool                         value64_sreadval_ulong(value64 *restrict val, fs *restrict buf);
extern bool                         value64_sreadval_char(value64 *restrict val, fs *restrict buf);
extern bool                         value64_sreadval_bool(value64 *restrict val, fs *restrict buf);
extern bool                         value64_sreadval_dbl(value64 *restrict val, fs *restrict buf);
extern bool                         value64_sreadval_fs(value64 *restrict val, fs *restrict buf);
extern bool                         value64_sreadval_str(value64 *restrict val, fs *restrict buf);


// generic DS reader, NOTE: it calls value64_sreadval_<type>
extern bool                         value64_dsreadval(DS *restrict ds, value64_type typ, value64 *restrict val, fs *restrict buf);

// generic c-str reader, NOTE: it calls value64_sreadval_<type>
static inline bool                  value64_strreadval(const char *restrict str, value64_type typ, value64 *restrict val, fs *restrict buf) {
    DS ds = dsCreateconst(str);
    return value64_dsreadval(&ds, typ, val, buf);
}

// generic FILE  reader, NOTE: it calls value64_sreadval_<type>
static inline bool                  value64_freadval(FILE *restrict in, value64_type typ, value64 *restrict val, fs *restrict buf) {
    DS ds = dsCreatef(in);
    return value64_dsreadval(&ds, typ, val, buf);
}

// generic save for FILE *
extern int                          value64_tofile(FILE *out, value64 val, value64_type typ, bool savetypeinfo);

/**
 * @brief Loads a value64 from DS source
 *
 * The expected format is:
 *   VALUE64(<type>): <value>
 * where <type> is the type name (INT, LNG, DBL, STR, FS, …) and <value>
 * is the textual representation of the value (quoted for strings).
 *
 * @param in           input stream (already opened for reading)
 * @param val          pointer to the value64 to fill
 * @param typ          expected type; will be updated from the header if
 *                     `loadtypeinfo` is true
 * @param loadtypeinfo if true, the type is read from the header, otherwise
 *                     the value is read assuming the given `typ`
 * @param buf          optional fast‑string buffer (if NULL, a temporary one
 *                     is allocated and freed)
 * @return true on success, false on format error or EOF
 *
 * @throws ERR_NULLABLE_PTR if `in` is NULL
 * @throws ERR_UNSUPPORTED_TYPE on unknown or unsupported type
 */
extern int                          value64_loadds(DS *restrict ds, value64 *restrict val, value64_type typ, bool loadtypeinfo, fs *restrict buf);

/**
 * @brief Loads a value64 from a text stream (formatted).
 *
 * The expected format is:
 *   VALUE64(<type>): <value>
 * where <type> is the type name (INT, LNG, DBL, STR, FS, …) and <value>
 * is the textual representation of the value (quoted for strings).
 *
 * @param in           input stream (already opened for reading)
 * @param val          pointer to the value64 to fill
 * @param typ          expected type; will be updated from the header if
 *                     `loadtypeinfo` is true
 * @param loadtypeinfo if true, the type is read from the header, otherwise
 *                     the value is read assuming the given `typ`
 * @param buf          optional fast‑string buffer (if NULL, a temporary one
 *                     is allocated and freed)
 * @return value on success, -1 format error or EOF
 *
 * @throws ERR_NULLABLE_PTR if `in` is NULL
 * @throws ERR_UNSUPPORTED_TYPE on unknown or unsupported type
 */
static inline int           value64_loadfile(FILE *restrict in, value64 *restrict val, value64_type typ, bool loadtypeinfo, fs *restrict buf) {
    DS ds = dsCreatef(in);
    return value64_loadds(&ds, val, typ, loadtypeinfo, buf);
}
/**
 * @brief Deserializes a value64 from a text string.
 *
 * The expected format is:
 *   VALUE64(<type>): <value>
 * where <type> is the type name (INT, LNG, DBL, STR, FS, …) and <value>
 * is the textual representation of the value (quoted for strings).
 *
 * @param source input string (must be null-terminated)
 * @param val    pointer to the value64 to fill
 * @param typ    expected type of the value
 * @param loadtypeinfo if true, the type is read from the header, otherwise
 *                     the value is read assuming the given `typ`
 * @param buf          optional fast‑string buffer (if NULL, a temporary one
 *                     is allocated and freed)
 * @return number of characters consumed, or a negative value on error
 */
static inline int                   value64_loadstr(const char *restrict source, value64 *restrict val, value64_type typ, bool loadtypeinfo, fs *restrict buf) {
    DS ds = dsCreateconst(source);
    return value64_loadds(&ds, val, typ, loadtypeinfo, buf);
}


// generic to string: fs MUST be initialized
extern int                          value64_tostr(fs *target, value64 val, value64_type typ, bool savetypeinfo);

// type to string
extern int                          value64_tostr_int(fs *target, value64 val);
extern int                          value64_tostr_long(fs *target, value64 val);
extern int                          value64_tostr_ulong(fs *target, value64 val);
extern int                          value64_tostr_dbl(fs *target, value64 val);
extern int                          value64_tostr_char(fs *target, value64 val);
extern int                          value64_tostr_bool(fs *target, value64 val);
extern int                          value64_tostr_ptr(fs *target, value64 val);
//  memory-alloc types
extern int                          value64_tostr_fs(fs *target, value64 val);
extern int                          value64_tostr_str(fs *target, value64 val);

// -------------------------------------------FILTERS -----------------------------------------
// ---------------------- trivial filters ------------------------------
extern bool                         value64_filter_true(value64 v, value64 data);
extern bool                         value64_filter_false(value64 v, value64 data);
// ---------------- fs filters -----------------
// fs filters (assuming v as fs*), data as int, check len >= data
extern bool                         value64_filter_fsminlen_int(value64 v, value64 data);
// fs filters (assuming v as fs*), data as int, check len <= data
extern bool                         value64_filter_fsmaxlen_int(value64 v, value64 data);
// fs filters (assuming v as fs*), data as int, check len == data
extern bool                         value64_filter_fslen_int(value64 v, value64 data);
// Проверка префикса (data.sval – строка-префикс)
extern bool                         value64_filter_fsprefix_str(value64 v, value64 data);
//
extern bool                         value64_filter_fsequals_str(value64 v, value64 data);
// fs vs str
extern bool                         value64_filter_fslike_str(value64 v, value64 data);
extern bool                         value64_filter_fsulike_str(value64 v, value64 data);
// --------------- numeric filters -------------------------
// int vs int
extern bool                         value64_filter_intlt_int(value64 v, value64 data);
extern bool                         value64_filter_intle_int(value64 v, value64 data);
extern bool                         value64_filter_intgt_int(value64 v, value64 data);
extern bool                         value64_filter_intge_int(value64 v, value64 data);
extern bool                         value64_filter_inteq_int(value64 v, value64 data);
extern bool                         value64_filter_intne_int(value64 v, value64 data);
// long vs  long
extern bool                         value64_filter_lnglt_lng(value64 v, value64 data);
extern bool                         value64_filter_lngle_lng(value64 v, value64 data);
extern bool                         value64_filter_lnggt_lng(value64 v, value64 data);
extern bool                         value64_filter_lngge_lng(value64 v, value64 data);
extern bool                         value64_filter_lngeq_lng(value64 v, value64 data);
extern bool                         value64_filter_lngne_lng(value64 v, value64 data);
// double vs double
extern bool                         value64_filter_dbllt_dbl(value64 v, value64 data);
extern bool                         value64_filter_dblle_dbl(value64 v, value64 data);
extern bool                         value64_filter_dblgt_dbl(value64 v, value64 data);
extern bool                         value64_filter_dblge_dbl(value64 v, value64 data);
extern bool                         value64_filter_dbleq_dbl(value64 v, value64 data);
extern bool                         value64_filter_dblne_dbl(value64 v, value64 data);
// 2 value filters
extern bool                         value64_filter2_intbetween_int_int(value64 v, value64 data1, value64 data2);

// ------------------------------------ ETC. ----------------------------------------

extern bool                         value64_validate(FILE *out, value64 v, value64_type typ);  // TODO:

#endif /* !_VALUE64_H */

