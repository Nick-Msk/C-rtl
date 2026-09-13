/********************************************************************
                    VALUE64(128) SET MODULE IMPLEMENTATION
********************************************************************/

// common include
#include "value64.h"

// -------------------------- TYPE SUPPORT API ------------------------

// All infos related to the types!
static const value64_typeinfo           value64_info[] = {
    [VALUE64_UNKNOWN]    = {"INVALID",     0,                       false    , "VALUE64_UNKNOWN"},
    [VALUE64_INT]        = {"INT",         sizeof(int),             true     , "VALUE64_INT"},
    [VALUE64_LONG]       = {"LNG",         sizeof(long),            true     , "VALUE64_LONG"},
    [VALUE64_ULONG]      = {"ULONG",       sizeof(unsigned long),   true     , "VALUE64_ULONG"},
    [VALUE64_DBL]        = {"DBL",         sizeof(double),          true     , "VALUE64_DBL"},
    [VALUE64_CHR]        = {"CHR",         sizeof(char),            true     , "VALUE64_CHR"},
    [VALUE64_BOOL]       = {"BOOL",        sizeof(bool),            true     , "VALUE64_BOOL"},
    [VALUE64_PTR]        = {"PTR",         sizeof(void *),          true     , "VALUE64_PTR"},
    [VALUE64_FS]         = {"FS",          sizeof(fs *),            true     , "VALUE64_FS"},
    [VALUE64_STR]        = {"STR",         sizeof(char *),          true     , "VALUE64_STR"},
    [VALUE64_FILE]       = {"FILE",        sizeof(FILE *),          true     , "VALUE64_FILE"},
    [VALUE64_TYPE_COUNT] = {"",            0,                       false    , ""}
};

_Static_assert(COUNT(value64_info) == VALUE64_TYPE_COUNT + 1,
               "Размер массива value65_info не совпадает с количеством типов!");

const value64_typeinfo              *value64_info_get(value64_type typ) {
    if (typ < 0 || typ >= COUNT(value64_info) || !value64_info[typ].is_valid)
        return NULL;
    return &value64_info[typ];
}

// the part of mass creation API, probably'll be changed
// create value from pointer, value64 constructor ANY type, MOVE semantic
value64                             value64_pcopy_move(void *p, value64_type typ, bool move){
    invraisecode(p != NULL, ERR_NULLABLE_PTR, "Null pointer");
    value64     tmp = LITERAL64_ZERO;  // init
    switch (typ){
        case VALUE64_INT:
            tmp.ival = *(const int *)p;
            if (move)
                *(int *)p = 0;
            break;
        case VALUE64_LONG:
            tmp.lval = *(const long *)p;
            if (move)
                *(long *)p = 0L;
            break;
        case VALUE64_ULONG:
            tmp.ulval = *(const unsigned long *)p;
            if (move)
                *(unsigned long *)p = 0L;
            break;
        case VALUE64_DBL:
            tmp.dval = *(const double *)p;
            if (move)
                *(double *)p = 0.0;
            break;
        case VALUE64_CHR:
            tmp.cval = *(const char *)p;
            if (move)
                *(char *)p = '\0';
            break;
        case VALUE64_BOOL:
            tmp.bval = *(const bool *)p;
            if (move)
                *(bool *)p = false;
            break;
        case VALUE64_PTR:
            tmp.pval = *(void * const *) p;
            if (move)
                *(void **) p = NULL;
            break;
        case VALUE64_STR:
            if (move)
                tmp.sval = (char *) p;  //MOVE POINTER
            else
                tmp = value64_createstr(p);
            break;
        // create fs body in head with FS_FLAG_BODYALLOC
        case VALUE64_FS:
            if (move)
                tmp = value64_movefs(p);
            else
                tmp = value64_createfs(p);
            break;
        case VALUE64_FILE:
            tmp.FILEval = (FILE *) p;
            if (move)
                *(FILE **) p = NULL;
            break;
        default:
            userraiseint(ERR_UNSUPPORTED_TYPE, "type %d %s isn't suppoted", typ, value64_typename(typ) );
    }
    return tmp;
}

/**
 * @brief Calculates a 64-bit hash value for a given value64 object.
 * 
 * This function computes a hash based on the internal representation of the 
 * value64 object. For primitive types (int, long, double, pointer, char), 
 * it uses a bitwise hash of the raw bits. For string-based types 
 * (FS, STR), it uses the DJB2 algorithm on the underlying string content.
 *
 * @param value The value64 object to be hashed.
 * @param typ   The type of the value64 object, which determines the hashing logic.
 * 
 * @return A 64-bit unsigned integer representing the hash value.
 */
unsigned long               value64_lhash(value64 value, value64_type typ){
    // probably it's better to calc hash by u64 attr (except fs for sure)
    value64      tmp = LITERAL64_ZERO;
    switch (typ){
        case VALUE64_INT:
            tmp.u64 = (uint64_t) value64_int(value);
        break;
        case VALUE64_LONG:
            tmp.u64 = (uint64_t) value64_long(value);
        break;
        case VALUE64_ULONG:
            tmp.u64 = (uint64_t) value64_ulong(value);
        break;
        case VALUE64_DBL:
            tmp.dval = value64_dbl(value);
        break;
        case VALUE64_PTR:
            tmp.u64 = (uint64_t) value64_ptr(value);    // or just do nothing as for HSET_DBL
        break;
        case VALUE64_CHR:
            tmp.u64 = (uint64_t) value64_char(value);
        break;
        case VALUE64_BOOL:
            tmp.u64 = (uint64_t) value64_bool(value);
        break;
        case VALUE64_FS:
            return  hash_djb2(fs_str(value64_fs(value) ) );
        break;
        case VALUE64_STR:
            return  hash_djb2(value64_str(value) );
        break;
        case VALUE64_FILE:
            tmp.u64 = (uint64_t) value64_FILE(value);
        break;
        default:
        break;
    }
    return hash_long(tmp.u64);
}

/**
 * @brief Maps a string representation of a type to its corresponding value64_type.
 *
 * This function iterates through the internal value64_info registry to find
 * a matching type name. It performs a case-sensitive comparison.
 *
 * @param str A null-terminated string representing the type name.
 * @return The matching value64_type if a match is found; otherwise VALUE64_UNKNOWN.
 *         Returns VALUE64_UNKNOWN if the input string is NULL.
 */
value64_type            value64_gettype(const char *str){
    if (str){
        for (size_t i = 0; i < COUNT(value64_info); i++) {
            if (value64_info_get(i) && strcmp(str, value64_info_get(i)->name) == 0)
                return (value64_type) i;
         }
    }
    return VALUE64_UNKNOWN;
}

/**
 * @brief Transfers the payload from the source object to the target object (Move Semantics).
 * 
 * This function performs a move operation, transferring the internal data from 
 * `source` to `target`. Unlike a copy, the `source` object is left in a 
 * "zeroed" or invalid state after the transfer to prevent resource leaks 
 * or double-frees.
 * 
 * @note This function does NOT change the logical type of the `target` object; 
 *       it only overwrites its contents. The caller is responsible for ensuring 
 *       that the `typ` provided matches the intended state of the `target`.
 * 
 * @param target Pointer to the existing object that will receive the data.
 * @param source Pointer to the source object whose data will be moved.
 * @param typ    The type of the data being moved, which determines the 
 *               transfer logic (e.g., heap transfer for FS).
 * 
 * @return Pointer to the `target` object for method chaining.
 * 
 * @throws ERR_NULLABLE_PTR if either `target` or `source` is NULL.
 * @throws ERR_UNABLE_ALLOCATE if the heap transfer for VALUE64_FS fails.
 */
value64                     *value64_moveto(value64 *restrict target, value64 *restrict source, value64_type typ){
    invraisecode(target && source,  ERR_NULLABLE_PTR, 
            "Null pointers %p %p", target, source);

    switch (typ){
        case VALUE64_FS:    // note: this's NOT the same as value64_movefs!
            target->fsval = fs_moveto_heap(source->fsval);  // no need to null source, fs_moveto_heap'll do that
            if (!target->fsval)
                userraiseint(ERR_UNABLE_ALLOCATE, "Unable to alloc new fs body");
        break;
        case VALUE64_DBL:
            target->dval = source->dval;
            source->dval = 0.0;     // double zero
        break;
        default:    // ALL others type even VALUE64_STR follows the same logic!
            target->u64 = source->u64;
            source->u64 = 0L;   // u64 cover all types
        break;
    }
    return target;
}

/**
 * @brief Swaps the contents of two value64 variables.
 * 
 * @note This is a low-level function and does not perform any safety checks.
 * @param v1 Pointer to the first value.
 * @param v2 Pointer to the second value.
 */
void                        value64_exch(value64 *v1, value64 *v2){
    value64 tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}

// ------------------------------- Comparators support ---------------------------------

/**
 * @brief Structure that groups all comparator types into a single entry.
 */
typedef struct {
    value64_Comparator      val_comp;      // For getComparator
    value64_Comparator      rev_val_comp; // For getRevComparator
    value64_PComparator     p_comp;       // For getPComparator
    value64_PComparator     p_rev_comp;   // For getPRevComparator
} value64_comparator_dispatch_t;

/**
 * @brief Unified master matrix for comparator mappings.
 * This eliminates redundant switch-case blocks and centralizes type definitions.
 * @note No comparator for VALUE64_FILE
 */
static const value64_comparator_dispatch_t comparator_matrix[VALUE64_TYPE_COUNT] = {
    [VALUE64_INT] = {
        value64_int_comp,   value64_int_rev_comp, 
        value64_pint_comp,   value64_pint_rev_comp
    },
    [VALUE64_LONG] = {
        value64_long_comp,  value64_long_rev_comp, 
        value64_plong_comp,  value64_plong_rev_comp
    },
    [VALUE64_ULONG] = {
        value64_ulong_comp, value64_ulong_rev_comp, 
        value64_pulong_comp, value64_pulong_rev_comp
    },
    [VALUE64_CHR] = {
        value64_char_comp,  value64_char_rev_comp, 
        value64_pchar_comp,  value64_pchar_rev_comp
    },
    [VALUE64_BOOL] = {
        value64_bool_comp,  value64_bool_rev_comp, 
        value64_pbool_comp,  value64_pbool_rev_comp
    },
    [VALUE64_DBL] = {
        value64_dbl_comp,   value64_dbl_rev_comp, 
        value64_pdbl_comp,   value64_pdbl_rev_comp
    },
    [VALUE64_PTR] = {
        value64_ptr_comp,   value64_ptr_rev_comp, 
        value64_pptr_comp,   value64_pptr_rev_comp
    },
    [VALUE64_FS] = {
        value64_fs_comp,    value64_fs_rev_comp, 
        value64_pfs_comp,    value64_pfs_rev_comp
    },
    [VALUE64_STR] = {
        value64_str_comp,   value64_str_rev_comp, 
        value64_pstr_comp,   value64_pstr_rev_comp
    }
};

/**
 * @brief Returns a pointer-based comparator for use in generic algorithms.
 * @param typ The type of the data.
 * @return Pointer to a value64_PComparator.
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized or invalid.
 */
value64_PComparator                 value64_getPComparator(value64_type typ) {
    if (typ < 1 || typ >= VALUE64_TYPE_COUNT) {
        userraiseint(ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);
        return NULL;
    }
    value64_PComparator comp = comparator_matrix[typ].p_comp;
    if (!comp) {
        userraiseint(ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);
    }
    return comp;
}

/**
 * @brief Returns a reverse pointer-based comparator.
 * @param typ The type of the data.
 * @return Pointer to a reversed value64_PComparator.
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized or invalid.
 */
value64_PComparator                 value64_getPRevComparator(value64_type typ) {
    if (typ < 1 || typ >= VALUE64_TYPE_COUNT) {
        userraiseint(ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);
        return NULL;
    }
    value64_PComparator comp = comparator_matrix[typ].p_rev_comp;
    if (!comp) {
        userraiseint(ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);
    }
    return comp;
}

/**
 * @brief Returns a value-based comparator.
 * @param typ The type of the data.
 * @return Pointer to a value64_Comparator.
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized or invalid.
 */
 value64_Comparator                 value64_getComparator(value64_type typ) {
    if (typ < 1 || typ >= VALUE64_TYPE_COUNT) {
        return userraise(NULL, ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);
    }
    value64_Comparator comp = comparator_matrix[typ].val_comp;
    if (!comp) {
        return userraise(NULL, ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);
    }
    return comp;
}

/**
 * @brief Returns a reverse value-based comparator.
 * @param typ The type of the data.
 * @return Pointer to a reversed value64_Comparator.
 * @throws ERR_UNSUPPORTED_TYPE if the type is not recognized or invalid.
 */
value64_Comparator                  value64_getRevComparator(value64_type typ) {
    if (typ < 1 || typ >= VALUE64_TYPE_COUNT) {
        return userraise(NULL, ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);
    }
    value64_Comparator comp = comparator_matrix[typ].rev_val_comp;
    if (!comp) {
        return userraise(NULL, ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);
    }
    return comp;
}

/**
 * @name Low-level Comparators
 * @brief Internal comparison functions for value64 types.
 * @note These functions perform no safety checks (no NULL or type validation) 
 *       and are intended for high-performance use in sorting and searching.
 * @return An integer less than, equal to, or greater than zero.
 * @{
 */

/** @brief Compares two integer values. */
int                             value64_int_comp(value64 v1, value64 v2) {
    return compare_int(v1.ival, v2.ival);
}
int                             value64_long_comp(value64 v1, value64 v2) {
    return compare_long(v1.lval, v2.lval);
}
int                             value64_ulong_comp(value64 v1, value64 v2) {
    return compare_ulong(v1.lval, v2.lval);
}
int                             value64_dbl_comp(value64 v1, value64 v2) {
    return compare_dbl(v1.dval, v2.dval);
}
int                             value64_char_comp(value64 v1, value64 v2) {
    return compare_char(v1.cval, v2.cval);
}
int                             value64_bool_comp(value64 v1, value64 v2) {
    return compare_bool(v1.bval, v2.bval);
}
int                             value64_fs_comp(value64 v1, value64 v2) {
    if (fs_isnull(v1.fsval) || fs_isnull(v2.fsval) )
        userraiseint(ERR_NULLABLE_PTR, "Null pointers (or .v) %p %p", v1.fsval, v2.fsval);
    return fs_cmp(v1.fsval, v2.fsval);
}
// compare fs vs provided c-str
int                             value64_fs_compstr(value64 v1, const char *str) {
    if (fs_isnull(v1.fsval) || str == NULL)
        userraiseint(ERR_NULLABLE_PTR, 
                "Null pointers v1.fsval %p or str %p", v1.fsval, str);
    return fs_cmpstr(v1.fsval, str);
}
//
int                             value64_str_comp(value64 v1, value64 v2) {
    if (!v1.sval || !v2.sval)
        userraiseint(ERR_NULLABLE_PTR, "Null pointers %p %p", v1.sval, v2.sval);
    return strcmp(v1.sval, v2.sval);
}
int                             value64_ptr_comp(value64 v1, value64 v2) {
    return compare_ptr(v1.pval, v2.pval);
}
/** @} */

/**
 * @name Reverse Comparators
 * @brief Functions for descending order comparison.
 * @note These are implemented by negating the result of the standard comparators.
 * @{
 */

/** @brief Reverse comparison for integers. */
int                             value64_int_rev_comp(value64 v1, value64 v2) {
    return -compare_int(v1.ival, v2.ival);
}
int                             value64_long_rev_comp(value64 v1, value64 v2) {
    return -compare_long(v1.lval, v2.lval);
}
int                             value64_ulong_rev_comp(value64 v1, value64 v2) {
    return -compare_ulong(v1.lval, v2.lval);
}
int                             value64_dbl_rev_comp(value64 v1, value64 v2) {
    return -compare_dbl(v1.dval, v2.dval);
}
int                             value64_char_rev_comp(value64 v1, value64 v2) {
    return -compare_char(v1.cval, v2.cval);
}
int                             value64_bool_rev_comp(value64 v1, value64 v2) {
    return -compare_bool(v1.bval, v2.bval);
}
int                             value64_fs_rev_comp(value64 v1, value64 v2) {
    //if (!v1.fsval || !v2.fsval) // FIXME: fsisnull must be here
    if (fs_isnull(v1.fsval) || fs_isnull(v2.fsval) )
        userraiseint(ERR_NULLABLE_PTR, "Null pointers (or .v) %p %p", v1.fsval, v2.fsval);
    return -fs_cmp(v1.fsval, v2.fsval);
}
int                             value64_str_rev_comp(value64 v1, value64 v2) {
    if (!v1.sval || !v2.sval)
        userraiseint(ERR_NULLABLE_PTR, "Null pointers %p %p", v1.sval, v2.sval);
    return -strcmp(v1.sval, v2.sval);
}
int                             value64_ptr_rev_comp(value64 v1, value64 v2) {
    return -compare_ptr(v1.pval, v2.pval);
}
/** @} */

/**
 * @brief Performs a high-level comparison of two value64 objects.
 * 
 * This function compares two values based on the provided type. Unlike 
 * low-level comparators, this function includes runtime validation for 
 * pointer-based types (FS and STR) to ensure memory safety.
 * 
 * @warning This is a high-level function that may raise exceptions 
 *          (ERR_NULLABLE_PTR, ERR_UNSUPPORTED_TYPE) if the inputs or 
 *          the underlying data are invalid. It is intended for general-purpose 
 *          use where type-safety and runtime validation are required.
 *
 * @param v1   The first value64 object (passed by value).
 * @param v2   The second value64 object (passed by value).
 * @param typ  The type to use for the comparison.
 * 
 * @return An integer:
 *         < 0 if v1 < v2,
 *         0 if v1 == v2,
 *         > 0 if v1 > v2.
 * 
 * @throws ERR_NULLABLE_PTR if the underlying pointers for FS or STR types are NULL.
 * @throws ERR_UNSUPPORTED_TYPE if the provided type is not recognized.
 */
int                                     value64_compare(value64 v1, value64 v2, value64_type typ) {
    if (typ < 1 || typ >= VALUE64_TYPE_COUNT)
        userraiseint(ERR_UNSUPPORTED_TYPE, "%s: %d", value64_typename(typ), typ);

    value64_Comparator comp = comparator_matrix[typ].val_comp;
    if (!comp)
        userraiseint(ERR_UNSUPPORTED_TYPE, "No comparator for %d/%s", typ, value64_typename(typ));

    return comp(v1, v2);
}

/**
 * @brief Generic pointer-based comparison dispatcher.
 * 
 * This function provides a generic way to compare two value64 objects 
 * via pointers without copying their contents. It uses the provided `typ` 
 * to dispatch the comparison to the appropriate low-level pointer 
 * comparator.
 * 
 * @note This is a high-level dispatcher intended for generic interfaces 
 *       (e.g., automated sorting of opaque pointers or generic engine loops).
 *       Because it involves a switch-case and multiple runtime checks, 
 *       it is significantly slower than the direct primitive comparators.
 *
 * @param v1   Pointer to the first value64 object.
 * @param v2   Pointer to the second value64 object.
 * @param typ  The type of the objects, used to select the correct comparison logic.
 * 
 * @return An integer indicating the relationship between the objects:
 *         < 0 if v1 < v2,
 *         0 if v1 == v2,
 *         > 0 if v1 > v2.
 * 
 * @throws ERR_NULLABLE_PTR if either v1 or v2 is NULL, or if internal 
 *         data pointers (for FS/STR types) are NULL.
 * @throws ERR_UNSUPPORTED_TYPE_CONV if the provided `typ` has no defined comparator.
 */
int                                 value64_pt_compare(const value64* restrict v1, const value64 *restrict v2, value64_type typ) {
    if (v1 == NULL || v2 == NULL)
        userraiseint(ERR_NULLABLE_PTR, "Null pointers %p %p", v1, v2);
    
    if (typ < 1 || typ >= VALUE64_TYPE_COUNT)
        userraiseint(ERR_UNSUPPORTED_TYPE, "%d:%s", typ, value64_typename(typ));

    value64_PComparator comp = comparator_matrix[typ].p_comp;
    if (!comp)
        userraiseint(ERR_UNSUPPORTED_TYPE, "No pointer comparator for %d/%s", typ, value64_typename(typ));

    return comp(v1, v2);
}

/**
 * @name Low-Level Pointer Comparators
 * @brief Wrappers for primitive comparators used in generic algorithms (qsort, bsearch).
 * 
 * @warning These functions are optimized for performance and DO NOT perform 
 *          NULL pointer checks or type validation. They assume that the 
 *          provided pointers are non-NULL and point to valid `value64` 
 *          structures of the expected type.
 * 
 * @return An integer: < 0 if v1 < v2, 0 if v1 == v2, > 0 if v1 > v2.
 * @{
 */

/** @brief Compares two integer-typed value64 objects. */
int                         value64_pint_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return compare_int(val1->ival, val2->ival); // from common.h
}
int                         value64_plong_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return compare_long(val1->lval, val2->lval);
}
int                         value64_pulong_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return compare_ulong(val1->lval, val2->lval);
}
int                         value64_pdbl_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return compare_dbl(val1->dval, val2->dval);
}
int                         value64_pchar_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return compare_char(val1->cval, val2->cval);
}
int                         value64_pbool_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return compare_bool(val1->bval, val2->bval);
}
int                         value64_pptr_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return compare_ptr(val1->pval, val2->pval);
}
int                         value64_pstr_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return strcmp(val1->sval, val2->sval);
}
int                         value64_pfs_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return fs_cmp(val1->fsval, val2->fsval);
}
/** @} */

/**
 * @name Reverse Pointer Comparators
 * @brief Reverses the comparison result for descending order sorting.
 * 
 * These functions are identical to their standard counterparts but return 
 * the negated result to facilitate descending order.
 * 
 * @warning No NULL checks or type validation are performed.
 * @{
*/

/** @brief Reverse comparison for integer-typed objects. */
int                         value64_pint_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -compare_int(val1->ival, val2->ival); // from common.h
}
int                         value64_plong_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -compare_long(val1->lval, val2->lval);
}
int                         value64_pulong_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -compare_ulong(val1->lval, val2->lval);
}
int                         value64_pdbl_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -compare_dbl(val1->dval, val2->dval);
}
int                         value64_pchar_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -compare_char(val1->cval, val2->cval);
}
int                         value64_pbool_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -compare_bool(val1->bval, val2->bval);
}
int                         value64_pptr_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -compare_ptr(val1->pval, val2->pval);
}
int                         value64_pstr_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -strcmp(val1->sval, val2->sval);
}
// no fs checking!!!
int                         value64_pfs_rev_comp(const void *restrict v1, const void *restrict v2){
    const value64 *val1 = (const value64 *) v1;
    const value64 *val2 = (const value64 *) v2;
    return -fs_cmp(val1->fsval, val2->fsval);
}
/** @} */

// ---------------------------- Sorting and searching -----------------------------------

/**
 * @brief Sorts an array of value64 elements in ascending order.
 * 
 * @param typ The type of the elements in the array.
 * @param arr Pointer to the array to be sorted.
 * @param sz The number of elements in the array.
 * @throws ERR_UNSUPPORTED_TYPE if no comparator is available for the specified type.
 */
void                        value64_sort(value64_type typ, value64 *arr, int sz){
    value64_PComparator pcomp = value64_getPComparator(typ);
    if (!pcomp)
         userraiseint(ERR_UNSUPPORTED_TYPE, "No comparator for %s: %d", value64_typename(typ), typ);
    qsort(arr, sz, sizeof(value64), pcomp);
}

/**
 * @brief Sorts an array of value64 elements in descending order.
 * 
 * @param typ The type of the elements in the array.
 * @param arr Pointer to the array to be sorted.
 * @param sz The number of elements in the array.
 * @throws ERR_UNSUPPORTED_TYPE if no reverse comparator is available for the specified type.
 */
void                        value64_revsort(value64_type typ, value64 *arr, int sz){ 
    value64_PComparator revpcomp = value64_getPRevComparator(typ);
    if (!revpcomp)
         userraiseint(ERR_UNSUPPORTED_TYPE, "No comparator for %s: %d", value64_typename(typ), typ);
    qsort(arr, sz, sizeof(value64), revpcomp);
}

/**
 * @brief Performs a linear search for a value in an array.
 * 
 * @param val The value to search for.
 * @param typ The type of elements in the array.
 * @param arr Pointer to the array of values.
 * @param sz The number of elements in the array.
 * @return The index of the first match found, or -1 if not found.
 * @throws ERR_NULLABLE_PTR if the array pointer is NULL while sz > 0.
 * @throws ERR_UNSUPPORTED_TYPE if no comparator is available for the specified type.
 */
int                         value64_search(value64 val, value64_type typ, const value64 *arr, int sz){
    invraisecode(arr || sz == 0, ERR_NULLABLE_PTR, 
        "Null pointer while sz > 0 %p %d", arr, sz);

    value64_Comparator comp = value64_getComparator(typ);
    if (!comp)
         userraiseint(ERR_UNSUPPORTED_TYPE, "No comparator for %s: %d", value64_typename(typ), typ);

    for (int i = 0; i < sz; i++)
        if (comp(val, arr[i]) == 0)
            return logsimpleret(i, "Found %d", i);
    return logsimpleerr(-1, "Not found"); // just a stub
}

/**
 * @brief Performs a linear search for a value in an array in reverse order.
 * 
 * @param val The value to search for.
 * @param typ The type of elements in the array.
 * @param arr Pointer to the array of values.
 * @param sz The number of elements in the array.
 * @return The index of the first match found (searching from the end), or -1 if not found.
 * @throws ERR_NULLABLE_PTR if the array pointer is NULL while sz > 0.
 * @throws ERR_UNSUPPORTED_TYPE if no comparator is available for the specified type.
 */
int                         value64_revsearch(value64 val, value64_type typ, const value64 *arr, int sz){
    invraisecode(arr || sz == 0, ERR_NULLABLE_PTR, 
            "Null pointer while sz > 0 %p %d", arr, sz);

    value64_Comparator comp = value64_getRevComparator(typ);
    if (!comp)
        userraiseint(ERR_UNSUPPORTED_TYPE, "No comparator for %s: %d", value64_typename(typ), typ);

    for (int i = sz; i > 0; i--)
        if (comp(val, arr[i - 1]) == 0)
            return logsimpleret(i - 1, "Found reverse %d", i - 1); 
    return logsimpleerr(-1, "Not found"); // just a stub
}

/**
 * @brief Performs a binary search for a value in an ascending-ordered array.
 * 
 * This function uses the standard `bsearch` algorithm. The input array 
 * must be sorted in ascending order according to the specified type's comparator.
 *
 * @param val The value to search for.
 * @param typ The type of elements in the array.
 * @param arr Pointer to the array of value64 elements.
 * @param sz The number of elements in the array.
 * @return The zero-based index of the found element, or -1 if not found or sz == 0.
 * @throws ERR_NULLABLE_PTR if the array pointer is NULL while sz > 0.
 * @throws ERR_UNSUPPORTED_TYPE if no comparator is available for the specified type.
 */int                         value64_binsearch(value64 val, value64_type typ, const value64 *arr, int sz){
    //bsearch(const void *key, const void *base, size_t nel, size_t width, int (*compar) (const void *, const void *));
    invraisecode(arr || sz == 0, ERR_NULLABLE_PTR, 
            "Null pointer while sz > 0 %p %d", arr, sz);
    if (sz == 0)
        return logsimpleerr(-1, "Noting to find, sz == 0");

    value64_PComparator pcomp = value64_getPComparator(typ);
    if (!pcomp)
        userraiseint(ERR_UNSUPPORTED_TYPE, "No comparator for %d/%s", typ, value64_typename(typ));
    const value64 *find = bsearch(&val, arr, sz, sizeof(value64), pcomp);
    if (!find)
        return logsimpleerr(-1, "Not found");
    else
        return logsimpleret(find - arr, "Found %lu", find - arr);
}

/**
 * @brief Performs a binary search for a value in a descending-ordered array.
 * 
 * This function uses the standard `bsearch` algorithm with a reverse comparator.
 * The input array MUST be sorted in descending order.
 *
 * @param val The value to search for.
 * @param typ The type of elements in the array.
 * @param arr Pointer to the array of value64 elements.
 * @param sz The number of elements in the array.
 * @return The zero-based index of the found element, or -1 if not found or sz == 0.
 * @throws ERR_NULLABLE_PTR if the array pointer is NULL while sz > 0.
 * @throws ERR_UNSUPPORTED_TYPE if no reverse comparator is available for the specified type.
 */int                         value64_rev_binsearch(value64 val, value64_type typ, const value64 *arr, int sz){
    //bsearch(const void *key, const void *base, size_t nel, size_t width, int (*compar) (const void *, const void *));
    invraisecode(arr || sz == 0, ERR_NULLABLE_PTR, 
            "Null pointer while sz > 0 %p %d", arr, sz);

    if (sz == 0)
        return logsimpleerr(-1, "Noting to find, sz == 0");

    value64_PComparator revpcomp = value64_getPRevComparator(typ);
    if (!revpcomp)
        userraiseint(ERR_UNSUPPORTED_TYPE, "No comparator for %d/%s", typ, value64_typename(typ));
    const value64 *find = bsearch(&val, arr, sz, sizeof(value64), revpcomp);
    if (!find)
        return logsimpleerr(-1, "Not found");
    else
        return logsimpleret(find - arr, "Found %lu", find - arr);
}

// ----------------------------- CONVERTERS ----------------------------------------

// --------------------------------- Converter support ------------------------------

typedef struct {
    value64_ConverterFunc       converter;   // Функция преобразования (NULL, если не нужно)
    value64_ConverterMoveFunc   move_converter;  // move
    value64_ValidatorFunc       validator;    // Функция проверки на безопасность (NULL, если всегда безопасно)
} value64_dispatch_t;

#define VALUE64_DISPATCH_ALL(c, m, v)    { .converter = c, .move_converter = m, .validator = v }
#define VALUE64_DISPATCH_CONV(c)         { .converter = c, .move_converter = NULL, .validator = NULL }
#define VALUE64_DISPATCH_MOVE(m)         { .converter = NULL, .move_converter = m, .validator = NULL }
#define VALUE64_DISPATCH_SAFE(c, v)      { .converter = c, .move_converter = NULL, .validator = v }
#define VALUE64_DISPATCH_MOVE_SAFE(m, v) { .converter = NULL, .move_converter = m, .validator = v }


// ---------------------------- Range chechers for converters --------------------------------
/**
 * @brief Checks if an int fits within the unsigned char range.
 */
static inline bool                  is_int_char_range(value64 v) {
    int val = value64_int(v);
    return val >= 0 && val <= (int)UCHAR_MAX;
}
static inline bool is_int_ulong_range(value64 v) {
    int val = value64_int(v);
    return val >= 0;
}
/**
 * @brief Checks if a long integer fits within the standard int range.
 */
static inline bool                  is_long_int_range(value64 v) {
    long val = value64_long(v);
    return val >= INT_MIN && val <= INT_MAX;
}
/**
 * @brief Checks if a long fits within the unsigned char range.
 */
static inline bool                  is_long_char_range(value64 v) {
    long val = value64_long(v);
    return val >= 0L && val <= (long)UCHAR_MAX;
}
/**
 * @brief Checks if a long integer fits within the standard unsigned long range.
 */
static inline bool                  is_long_ulong_range(value64 v) {
    long val = value64_long(v);
    return val >= 0L;
}
/**
 * @brief Checks if a unsigned long integer fits within the standard int range.
 */
static inline bool                  is_ulong_int_range(value64 v) {
    unsigned long val = value64_ulong(v);
    return val <= (unsigned long)INT_MAX; // unsigned всегда >= 0
}
/**
 * @brief Checks if a unsigned long integer fits within the standard  long range.
 */
static inline bool                  is_ulong_long_range(value64 v) {
    unsigned long val = value64_ulong(v);
    return val <= (unsigned long)LONG_MAX;
}
/**
 * @brief Checks if a unsigned long fits within the unsigned char range.
 */
static inline bool                  is_ulong_char_range(value64 v) {
    unsigned long val = value64_ulong(v);
    return val <= (unsigned long)UCHAR_MAX;
}
/**
 * @brief Checks if a double can be safely represented as an int.
 */
static inline bool                  is_dbl_int_range(value64 v) {
    double val = value64_dbl(v);
    return val >= (double)INT_MIN && val <= (double)INT_MAX;
}
/**
 * @brief Checks if a double can be safely represented as a long.
 * @note Precision issues may occur for values exceeding 2^53.
 */
static inline bool                  is_dbl_long_range(value64 v) {
    double val = value64_dbl(v);
    return val >= (double)LONG_MIN && val <= (double)LONG_MAX;
}
/**
 * @brief Checks if a double can be safely represented as a unsigned long.
 * @note Precision issues may occur for values exceeding 2^53.
 */
static inline bool                  is_dbl_ulong_range(value64 v) {
    double val = value64_dbl(v);
    return val >= 0.0 && val <= (double)ULONG_MAX;
}

static const value64_dispatch_t dispatch_conv_matrix[VALUE64_TYPE_COUNT][VALUE64_TYPE_COUNT] = {
    [VALUE64_INT] = {
        [VALUE64_INT]   = VALUE64_DISPATCH_CONV(value64_convert_int_to_int),
        [VALUE64_LONG]  = VALUE64_DISPATCH_CONV(value64_convert_int_to_lng),
        [VALUE64_ULONG] = VALUE64_DISPATCH_SAFE(value64_convert_int_to_ulong, is_int_ulong_range),
        [VALUE64_DBL]   = VALUE64_DISPATCH_CONV(value64_convert_int_to_dbl),
        [VALUE64_CHR]   = VALUE64_DISPATCH_SAFE(value64_convert_int_to_char, is_int_char_range),
        [VALUE64_BOOL]  = VALUE64_DISPATCH_CONV(value64_convert_int_to_bool),
        [VALUE64_FS]    = VALUE64_DISPATCH_CONV(value64_convert_int_to_fs),
        [VALUE64_STR]   = VALUE64_DISPATCH_CONV(value64_convert_int_to_str)
    },
    [VALUE64_LONG] = {
        [VALUE64_INT]    = VALUE64_DISPATCH_SAFE(value64_convert_lng_to_int, is_long_int_range),
        [VALUE64_LONG]   = VALUE64_DISPATCH_CONV(value64_convert_lng_to_lng),
        [VALUE64_ULONG]  = VALUE64_DISPATCH_SAFE(value64_convert_lng_to_ulong, is_long_ulong_range),
        [VALUE64_DBL]    = VALUE64_DISPATCH_CONV(value64_convert_lng_to_dbl),
        [VALUE64_CHR]    = VALUE64_DISPATCH_SAFE(value64_convert_lng_to_char, is_long_char_range),
        [VALUE64_BOOL]   = VALUE64_DISPATCH_CONV(value64_convert_lng_to_bool),
        [VALUE64_FS]     = VALUE64_DISPATCH_CONV(value64_convert_lng_to_fs),
        [VALUE64_STR]    = VALUE64_DISPATCH_CONV(value64_convert_lng_to_str)
    },
    [VALUE64_ULONG] = {
        [VALUE64_INT]   = VALUE64_DISPATCH_SAFE(value64_convert_ulong_to_int, is_ulong_int_range),
        [VALUE64_LONG]  = VALUE64_DISPATCH_SAFE(value64_convert_ulong_to_lng, is_ulong_long_range),
        [VALUE64_ULONG] = VALUE64_DISPATCH_CONV(value64_convert_ulong_to_ulong),
        [VALUE64_DBL]   = VALUE64_DISPATCH_CONV(value64_convert_ulong_to_dbl),
        [VALUE64_CHR]   = VALUE64_DISPATCH_SAFE(value64_convert_ulong_to_char, is_ulong_char_range),
        [VALUE64_BOOL]  = VALUE64_DISPATCH_CONV(value64_convert_ulong_to_bool),
        [VALUE64_FS]    = VALUE64_DISPATCH_CONV(value64_convert_ulong_to_fs),
        [VALUE64_STR]   = VALUE64_DISPATCH_CONV(value64_convert_ulong_to_str)
    },
    [VALUE64_CHR] = {
        [VALUE64_INT]   = VALUE64_DISPATCH_CONV(value64_convert_char_to_int),
        [VALUE64_LONG]  = VALUE64_DISPATCH_CONV(value64_convert_char_to_lng),
        [VALUE64_ULONG] = VALUE64_DISPATCH_CONV(value64_convert_char_to_ulong),
        [VALUE64_BOOL]  = VALUE64_DISPATCH_CONV(value64_convert_char_to_bool),
        [VALUE64_FS]    = VALUE64_DISPATCH_CONV(value64_convert_char_to_fs),
        [VALUE64_STR]   = VALUE64_DISPATCH_CONV(value64_convert_char_to_str),
        [VALUE64_CHR]   = VALUE64_DISPATCH_CONV(value64_convert_char_to_char)
    },
    [VALUE64_BOOL] = {
        [VALUE64_INT]   = VALUE64_DISPATCH_CONV(value64_convert_bool_to_int),
        [VALUE64_LONG]  = VALUE64_DISPATCH_CONV(value64_convert_bool_to_lng),
        [VALUE64_ULONG] = VALUE64_DISPATCH_CONV(value64_convert_bool_to_ulong),
        [VALUE64_CHR]   = VALUE64_DISPATCH_CONV(value64_convert_bool_to_char),
        [VALUE64_FS]    = VALUE64_DISPATCH_CONV(value64_convert_bool_to_fs),
        [VALUE64_STR]   = VALUE64_DISPATCH_CONV(value64_convert_bool_to_str),
        [VALUE64_BOOL]  = VALUE64_DISPATCH_CONV(value64_convert_bool_to_bool)
    },
    [VALUE64_DBL] = {
        [VALUE64_INT]   = VALUE64_DISPATCH_SAFE(value64_convert_dbl_to_int, is_dbl_int_range),
        [VALUE64_LONG]  = VALUE64_DISPATCH_SAFE(value64_convert_dbl_to_lng, is_dbl_long_range),
        [VALUE64_ULONG] = VALUE64_DISPATCH_SAFE(value64_convert_dbl_to_ulong, is_dbl_ulong_range),
        [VALUE64_DBL]   = VALUE64_DISPATCH_CONV(value64_convert_dbl_to_dbl),
        [VALUE64_FS]    = VALUE64_DISPATCH_CONV(value64_convert_dbl_to_fs),
        [VALUE64_STR]   = VALUE64_DISPATCH_CONV(value64_convert_dbl_to_str)
    },
    [VALUE64_FS] = {
        [VALUE64_INT]   = VALUE64_DISPATCH_CONV(value64_convert_fs_to_int),
        [VALUE64_LONG]  = VALUE64_DISPATCH_CONV(value64_convert_fs_to_lng),
        [VALUE64_ULONG] = VALUE64_DISPATCH_CONV(value64_convert_fs_to_ulong),
        [VALUE64_DBL]   = VALUE64_DISPATCH_CONV(value64_convert_fs_to_dbl),
        [VALUE64_CHR]   = VALUE64_DISPATCH_CONV(value64_convert_fs_to_char),
        [VALUE64_BOOL]  = VALUE64_DISPATCH_CONV(value64_convert_fs_to_bool),
        [VALUE64_STR]   = VALUE64_DISPATCH_ALL(value64_convert_fs_to_str, value64_convert_move_fs_to_str, NULL),
        [VALUE64_FS]    = VALUE64_DISPATCH_ALL(value64_convert_fs_to_fs, value64_convert_move_fs_to_fs, NULL)
    },
    [VALUE64_STR] = {
        [VALUE64_INT]   = VALUE64_DISPATCH_CONV(value64_convert_str_to_int),
        [VALUE64_LONG]  = VALUE64_DISPATCH_CONV(value64_convert_str_to_lng),
        [VALUE64_ULONG] = VALUE64_DISPATCH_CONV(value64_convert_str_to_ulong),
        [VALUE64_DBL]   = VALUE64_DISPATCH_CONV(value64_convert_str_to_dbl),
        [VALUE64_CHR]   = VALUE64_DISPATCH_CONV(value64_convert_str_to_char),
        [VALUE64_BOOL]  = VALUE64_DISPATCH_CONV(value64_convert_str_to_bool),
        [VALUE64_FS]    = VALUE64_DISPATCH_ALL(value64_convert_str_to_fs, value64_convert_move_str_to_fs, NULL),
        [VALUE64_STR]   = VALUE64_DISPATCH_ALL(value64_convert_str_to_str, value64_convert_move_str_to_str, NULL)
    }
};

// get validator
value64_ValidatorFunc           value64_is_validator(value64_type from, value64_type to) {
    return dispatch_conv_matrix[from][to].validator;
}
// get transision method (copy)
value64_ConverterFunc           value64_is_copyconverted(value64_type from, value64_type to) {
    return dispatch_conv_matrix[from][to].converter;
}
// get transision MOVE method
value64_ConverterMoveFunc       value64_is_moveconverted(value64_type from, value64_type to) {
    return dispatch_conv_matrix[from][to].move_converter;
}

/**
 * @brief Checks if a value can be safely converted from one type to another without loss of data.
 * 
 * @param v    The value to test.
 * @param from The source type.
 * @param to   The target type.
 * 
 * @return true if the conversion is safe (lossless), false otherwise.
 */
bool                            value64_is_convertable(value64 v, value64_type from, value64_type to) {
    if ( (from < 1 || from >= VALUE64_TYPE_COUNT) || (to < 1 || to >= VALUE64_TYPE_COUNT) )
        return false;

    value64_dispatch_t dispatch = dispatch_conv_matrix[from][to];

    if (!dispatch.converter)
        return false;
    // no validator => ok, no checking
    return (dispatch.validator == NULL) ? true : dispatch.validator(v);
}

/**
 * @brief Performs a type conversion using the registered conversion matrix.
 * 
 * This function looks up the appropriate converter for the given (from, to) 
 * type pair. If a converter exists, it is executed and its result is returned.
 *
 * @warning This function follows a "fail-fast" policy. If no converter is 
 *          registered for the requested transition, an error is raised 
 *          immediately to prevent type-mismatch propagation.
 *
 * @param v    The source value to convert.
 * @param from The source type.
 * @param to   The target type.
 * @param check If validation is required
 * 
 * @return The converted value.
 * 
 * @throws ERR_UNSUPPORTED_TYPE_CONV if no converter is found in the dispatch 
 *         table for the specified type transition.
 */

value64                         value64_convert_common(value64 v, value64_type from, value64_type to, bool check) {
    if ( (from < 1 || from >= VALUE64_TYPE_COUNT) || (to < 1 || to >= VALUE64_TYPE_COUNT) )
        userraiseint(ERR_OUT_OF_RANGE, 
            "from = %d, to = %d is out of 1 ..%d", from, to, VALUE64_TYPE_COUNT);

    value64_dispatch_t dispatch = dispatch_conv_matrix[from][to];
    if (check && dispatch.validator)
        if (!dispatch.validator(v))
            userraiseint(ERR_VALIDATION_FAILED,  "%d/%s => %d/%s", from, value64_typename(from), to, value64_typename(to));
    if (dispatch.converter == NULL)
        userraiseint(ERR_UNSUPPORTED_TYPE_CONV, "%d/%s => %d/%s", from, value64_typename(from), to, value64_typename(to));
    return  dispatch.converter(v);
}

/**
 * @name Value64 Conversion Matrix
 * @brief A collection of conversion functions for transforming one value64 type to another.
 * 
 * These functions are organized by their source type. They range from simple
 * bitwise/value copies to complex string parsing and heap allocations.
 * @{
 */

// --- Group INT ---
/** @name Integer to [Type] Conversions */
/** @{ */
/** @brief Identity conversion (int to int). */
value64                     value64_convert_int_to_int(value64 v) {
    return value64_createint(value64_int(v));
}
/** @brief Converts int to long. */
value64                     value64_convert_int_to_lng(value64 v) {
    return  value64_createlong((long) value64_int(v) );
}
/** @brief Converts int to unsigned long. */
value64                     value64_convert_int_to_ulong(value64 v) {
    return  value64_createulong((long) value64_int(v) );
}
/** @brief Converts int to double. */
value64                     value64_convert_int_to_dbl(value64 v) {
    return  value64_createdbl((double) value64_int(v) );
}
/** @brief Converts int to fs  (heap allocation). */
value64                     value64_convert_int_to_fs(value64 v) {
    value64     result = LITERAL64_ZERO;
    fs          tmp = fscopyf("%d", value64_int(v));
    result.fsval = fs_moveto_heap(&tmp);
    return result;
}
/** @brief Converts int to string (buffer-based). */
value64                     value64_convert_int_to_str(value64 v) {
    char        buf[100];       // CAn't use fs in STR
    snprintf(buf, sizeof(buf) - 1, "%d", value64_int(v));
    return value64_createstr(buf);
}
/** @brief Converts int to char (checks range). */
value64                     value64_convert_int_to_char(value64 v) {
    return  value64_createchar(value64_int(v) );
}
/** @brief Converts int to bool */
value64                     value64_convert_int_to_bool(value64 v) {
    return  value64_createbool(value64_bool(v) );
}
/** @} */

// --- Группа LONG ---
/** @name Long to [Type] Conversions */
/** @{ */
/** @brief Converts long to int */
value64                     value64_convert_lng_to_int(value64 v) {
    return value64_createint( value64_long(v) );
}
/** @brief Identity conversion (long to long). */
value64                     value64_convert_lng_to_lng(value64 v) {
    return value64_createlong(value64_long(v));
}
/** @brief Identity conversion (long to unsigned long). */
value64                     value64_convert_lng_to_ulong(value64 v) {
    return value64_createulong(value64_long(v));
}
/** @brief Converts long to char (checks range). */
value64                     value64_convert_lng_to_char(value64 v) {
    return  value64_createchar(value64_int(v) );
}
/** @brief Converts int to bool */
value64                     value64_convert_lng_to_bool(value64 v) {
    return  value64_createbool(value64_long(v) );
}
/** @brief Converts long to double. */
value64                     value64_convert_lng_to_dbl(value64 v) {
    return value64_createdbl((double) value64_long(v) );
}
/** @brief Converts long to fs  (heap allocation). */
value64                     value64_convert_lng_to_fs(value64 v) {
    value64     result = LITERAL64_ZERO;
    fs          tmp = fscopyf("%ld", value64_long(v) );
    result.fsval = fs_moveto_heap(&tmp);
    return result;
}
/** @brief Converts long to string. */
value64                     value64_convert_lng_to_str(value64 v) {
    char        buf[100];
    snprintf(buf, sizeof(buf) - 1, "%ld", value64_long(v) );
    return value64_createstr(buf);
}
/** @} */

// --- Группа ULONG ---
/** @name Long to [Type] Conversions */
/** @{ */
/** @brief Converts long to int (checks range). */

value64                     value64_convert_ulong_to_int(value64 v) {
    return value64_createint( value64_ulong(v) );
}
value64                     value64_convert_ulong_to_lng(value64 v) {
    return value64_createlong( value64_ulong(v) );
}
value64                     value64_convert_ulong_to_ulong(value64 v) {
    return value64_createulong( value64_ulong(v) );
}
value64                     value64_convert_ulong_to_dbl(value64 v) {
    return value64_createdbl( value64_ulong(v) );
}
value64                     value64_convert_ulong_to_char(value64 v) {
    return value64_createchar( value64_ulong(v) );
}
value64                     value64_convert_ulong_to_bool(value64 v) {
    return value64_createbool(value64_ulong(v) );
}
value64                     value64_convert_ulong_to_fs(value64 v) {
    value64     result = LITERAL64_ZERO;
    fs          tmp = fscopyf("%lu", value64_ulong(v) );
    result.fsval = fs_moveto_heap(&tmp);
    return result;
}
value64                     value64_convert_ulong_to_str(value64 v) {
    char        buf[100];
    snprintf(buf, sizeof(buf) - 1, "%ld", value64_long(v) );
    return value64_createstr(buf);
}

/** @} */

// --- Группа DBL ---
/** @name Double to [Type] Conversions */
/** @{ */
/** @brief Converts double to int (checks range). */
value64                     value64_convert_dbl_to_int(value64 v) {
    return value64_createint((int) value64_dbl(v) );
}
/** @brief Converts double to long (checks range). */
value64                     value64_convert_dbl_to_lng(value64 v) {
    return value64_createlong((long) value64_dbl(v) );
}
/** @brief Converts double to long (checks range). */
value64                     value64_convert_dbl_to_ulong(value64 v) {
    return value64_createulong((long) value64_dbl(v) );
}
/** @brief Converts double to fs */
value64                     value64_convert_dbl_to_fs(value64 v) {
    value64     result = LITERAL64_ZERO;
    fs tmp = fscopyf("%g", value64_dbl(v) );       // context must be used! TODO:
    result.fsval = fs_moveto_heap(&tmp);
    return result;
}
/** @brief Converts double to string. */
value64                     value64_convert_dbl_to_str(value64 v) {
    char        buf[100];
    snprintf(buf, sizeof(buf), "%lf", value64_dbl(v));    // // context must be used! TODO:
    return value64_createstr(buf);
}
/** @brief Identity conversion (double to double). */
value64                     value64_convert_dbl_to_dbl(value64 v) {
    return value64_createdbl(value64_dbl(v));
}
/** @} */

// --- Группа FS ---
/** @name fs to [Type] Conversions */
/** @{ */
/** @brief Converts FS object to int. */
value64                     value64_convert_fs_to_int(value64 v) {
    fs          *fsval = value64_fs(v);
    return value64_createint(fs_getint(fsval) );
}
/** @brief Converts FS object to long. */
value64                     value64_convert_fs_to_lng(value64 v) {
    fs          *fsval = value64_fs(v);
    return value64_createlong(fs_getlong(fsval) );
}
/** @brief Converts FS object to unsigned long. */
value64                     value64_convert_fs_to_ulong(value64 v) {
    fs          *fsval = value64_fs(v);
    return value64_createulong(fs_getulong(fsval) );
}
/** @brief Converts FS object to double. */
value64                     value64_convert_fs_to_dbl(value64 v) {
    fs          *fsval = value64_fs(v);
    return value64_createdbl(fs_getdouble(fsval) );
}
/** @brief Converts FS object to string (copy semantic). */
value64                     value64_convert_fs_to_str(value64 v) {
    fs          *fsval = value64_fs(v);
    return value64_createstr(fsval->v);
}
/** @brief Converts FS object to FS (copy semantic). */
value64                     value64_convert_fs_to_fs(value64 v){
    fs          *fsval = value64_fs(v);
    return value64_createfs(fsval);
}
/** @brief Converts FS object to char. */
value64                     value64_convert_fs_to_char(value64 v){
    fs          *fsval = value64_fs(v);
    return value64_createchar(fs_getchar(fsval));       // probably with checking
}
/** @brief Converts FS object to bool. */
value64                     value64_convert_fs_to_bool(value64 v){
    fs      *fsval = value64_fs(v);
    bool    b;
    if (!try_parse_bool(fs_str(fsval), &b))
        userraiseint(ERR_INVALID_CONVERSION, "fs->bool fail");
    return value64_createbool(b);     // TODO: context is required here
}
/** @} */

// --- Группа STR ---
/** @name String to [Type] Conversions */
/** @{ */
/** @brief Parses string to int (may raise ERR_INVALID_CONVERSION). */
value64                     value64_convert_str_to_int(value64 v) {
    char        *sval = value64_str(v);
    value64     result = LITERAL64_ZERO;
    if (!try_parse_int(sval, &result.ival))
        userraiseint(ERR_INVALID_CONVERSION, "str->int fail");
    return result;
}
/** @brief Parses string to long (may raise ERR_INVALID_CONVERSION). */
value64                     value64_convert_str_to_lng(value64 v) {
    char        *sval = value64_str(v);
    value64     result = LITERAL64_ZERO;
    if (!try_parse_long(sval, &result.lval))
        userraiseint(ERR_INVALID_CONVERSION, "str->long fail");
    return result;
}
value64                     value64_convert_str_to_ulong(value64 v) {
    char        *sval = value64_str(v);
    value64     result = LITERAL64_ZERO;
    if (!try_parse_ulong(sval, &result.ulval))
        userraiseint(ERR_INVALID_CONVERSION, "str->ulong fail");
    return result;
}
/** @brief Parses string to double (may raise ERR_INVALID_CONVERSION). */
value64                     value64_convert_str_to_dbl(value64 v) {
    char        *sval = value64_str(v);
    value64     result = LITERAL64_ZERO;
    if (!try_parse_double(sval, &result.dval))
        userraiseint(ERR_INVALID_CONVERSION, "str->double fail");
    return result;
}
/** @brief Converts string to FS object (heap allocation). */
value64                     value64_convert_str_to_fs(value64 v) {
    char        *sval = value64_str(v);
    value64     result = LITERAL64_ZERO;
    result.fsval = fs_heapcopy(sval);
    return result;
}
/** @brief Converts string to string (deep copy). */
value64                     value64_convert_str_to_str(value64 v) {
    char        *sval = value64_str(v);
    value64     result = LITERAL64_ZERO;
    result = value64_createstr(sval);
    return result;
}
/** @brief Converts first character of string to char. Even if '\0' */
value64                     value64_convert_str_to_char(value64 v) {
    char        *sval = value64_str(v);
    return value64_createchar(*sval);  // not sure, no null checking
}
/** @brief Converts first character of string to bool. */
value64                     value64_convert_str_to_bool(value64 v) {
    char        *sval = value64_str(v);
    bool         b;
    if (!try_parse_bool(sval, &b))
        userraiseint(ERR_INVALID_CONVERSION, "str->bool fail");
    return value64_createbool(b);
}
/** @} */

// --- Группа CHAR ---
/** @name Character to [Type] Conversions */
/** @{ */
/** @brief Converts char to int. */
value64                     value64_convert_char_to_int(value64 v) {
    return value64_createint((int) value64_char(v));
}
/** @brief Converts char to long. */
value64                     value64_convert_char_to_lng(value64 v) {
    return value64_createlong((long) value64_char(v));
}
/** @brief Converts char to unsigned long. */
value64                     value64_convert_char_to_ulong(value64 v) {
    return value64_createulong( (unsigned long) value64_char(v));
}
/** @brief Converts char to FS object. */
value64                     value64_convert_char_to_fs(value64 v) {
    fs      tmp = fscopyf("%c", value64_char(v));
    return value64_movefs(&tmp); // to avoid double alloc
}
/** @brief Converts char to string. */
value64                     value64_convert_char_to_str(value64 v) {
    char    buf[] = { value64_char(v), '\0' };
    return value64_createstr(buf);
}
/** @brief Identity conversion (char to char). */
value64                     value64_convert_char_to_char(value64 v) {
    return value64_createchar(value64_char(v));
}
/** @brief Converts character to bool */
value64                     value64_convert_char_to_bool(value64 v) {
    char        cval = value64_char(v);
    return value64_createchar(cval ? true: false);  // not sure, no null checking
}
/** @} */

// --- Группа BOOL ---
/** @name Character to [Type] Conversions */
/** @{ */
/** @brief Converts char to int. */
value64                     value64_convert_bool_to_int(value64 v) {
    return value64_createint( (int) value64_bool(v));
}
/** @brief Converts char to long. */
value64                     value64_convert_bool_to_lng(value64 v) {
    return value64_createlong((long) value64_bool(v));
}
/** @brief Converts char to unsigned long. */
value64                     value64_convert_bool_to_ulong(value64 v) {
    return value64_createulong((unsigned long) value64_bool(v));
}
/** @brief Converts char to FS object. */
value64                     value64_convert_bool_to_fs(value64 v) {
    fs      tmp = fscopyf("%s", bool_str(value64_bool(v)) );    // TODO: context?
    return value64_movefs(&tmp); // to avoid double alloc
}
/** @brief Converts char to string. */
value64                     value64_convert_bool_to_str(value64 v) {
    return value64_createstr(bool_str(value64_bool(v)) );       // TODO: context?
}
/** @brief Identity conversion (char to char). */
value64                     value64_convert_bool_to_char(value64 v) {
    return value64_createchar(value64_bool(v) ? 't': 'f');  
}
/** @brief Converts character to bool */
value64                     value64_convert_bool_to_bool(value64 v) {
    return value64_createbool(value64_bool(v) );  // not sure, no null checking
}
/** @} */


/** @} */

// converters, MOVE semantic
/**
 * @name Move Semantics Conversion
 * @brief Functions for high-performance, destructive type conversion.
 * 
 * These functions implement "move" logic: instead of copying data, they 
 * transfer ownership of resources (like heap memory for strings or FS) 
 * from the source object to the result.
 * 
 * @warning These functions are DESTRUCTIVE. The source object (passed by pointer) 
 *          will be modified and its internal resource pointers will be cleared 
 *          to prevent double-freeing.
 * @{
 */

/**
 * @brief Generic dispatcher for move-based conversions.
 * 
 * Uses the `conv_move_matrix` to find and execute the appropriate 
 * move-conversion function.
 *
 * @param v    Pointer to the source object to be modified.
 * @param from The type of the source object.
 * @param to   The target type.
 * 
 * @return A new value64 object containing the moved data.
 * @throws ERR_UNSUPPORTED_TYPE_CONV if no move-conversion is defined for the given types.
 */
value64                     value64_convert_move(value64 *pv, value64_type from, value64_type to) {
    if (from >= VALUE64_TYPE_COUNT || to >= VALUE64_TYPE_COUNT)
        return *pv;
       
    value64_dispatch_t dispatch = dispatch_conv_matrix[from][to];
    if (dispatch.validator)
        if (!dispatch.validator(*pv) )
            userraiseint(ERR_VALIDATION_FAILED,  "%d/%s => %d/%s", from, value64_typename(from), to, value64_typename(to));
    if (dispatch.move_converter == NULL)
        userraiseint(ERR_UNSUPPORTED_TYPE_CONV, "%d/%s => %d/%s", from, value64_typename(from), to, value64_typename(to));
    return  dispatch.move_converter(pv);
}

/** @name Specialized Move Implementations */
/** @{ */

/** @brief Moves resource from FS to a String object. */
value64                     value64_convert_move_fs_to_str(value64 *v){
    value64     result = LITERAL64_ZERO;
    result.sval = fsmovetostr(v->fsval);    // because fs_movefrom_heapstr() is out of fs
    return result;
}
/** @brief Moves resource from FS to another FS object (identity move). */
value64                     value64_convert_move_fs_to_fs(value64 *v){
    value64     result = LITERAL64_ZERO;
    result.fsval = v->fsval;
    v->fsval = 0;  // NO FREE HERE
    return result;
}
/** @brief Moves resource from String to an FS object. */
value64                     value64_convert_move_str_to_fs(value64 *v){
    value64     result = LITERAL64_ZERO;
    result.fsval = fs_moveto_heapstr(&v->sval);
    v->sval = NULL;
    return result;
}
/** @brief Moves resource from String to another String object (identity move). */
value64                     value64_convert_move_str_to_str(value64 *v){
    value64     result = LITERAL64_ZERO;
    result.sval = v->sval;
    v->sval = NULL;
    return result;
}
/** @} */

/** @} */

// ------------------------ PRINTERS/CHECKERS ---------------------------------------

/**
 * @name String Escaping Utilities
 * @brief Functions for serializing strings with escaped special characters.
 * 
 * These functions handle the escaping of special characters:
 * `"` $\to$ `\"`, `\` $\to$ `\\`, `\n` $\to$ `\n`, `\r` $\to$ `\r`, and `\t` $\to$ `\t`.
 * @{
 */

/**
 * @brief Writes an escaped string to a standard file stream.
 * 
 * This is a high-level, convenience function. It uses `fprintf` for each 
 * character, making it easy to implement but relatively slow due to 
 * the overhead of format string parsing.
 * 
 * @note Not recommended for performance-critical loops or large data volumes.
 *
 * @param out Pointer to the target `FILE` stream.
 * @param s   The null-terminated string to be escaped and written.
 * 
 * @return The number of characters written, or a negative value on error.
 */
static int                  fprint_str_escaped(FILE *restrict out, const char *restrict s) {
    int     cnt = 0;
    if (out && s) {
        cnt = fprintf(out, "\"");
        for (const char *p = s; *p; p++) {
            switch (*p) {
                case '"':  cnt += fprintf(out, "\\\""); break;
                case '\\': cnt += fprintf(out, "\\\\"); break;
                case '\n': cnt += fprintf(out, "\\n");  break;
                case '\r': cnt += fprintf(out, "\\r");  break;
                case '\t': cnt += fprintf(out, "\\t");  break;
                default:   cnt += fprintf(out, "%c", *p);
            }
        }
        cnt += fprintf(out, "\"");
    }
    return cnt;
}
/**
 * @brief High-performance escaped serialization to an `fs` buffer.
 * 
 * This is a low-level, performance-optimized function that directly 
 * manipulates the `fs` buffer. It is designed for high-throughput 
 * serialization scenarios.
 * 
 * @warning This function is highly efficient but relies on the internal 
 *          state of the `fs` object and its `elemnext` iteration mechanism.
 *
 * @param out Pointer to the `fs` buffer where the escaped string will be appended.
 * @param s   The null-terminated string to be escaped.
 * 
 * @return The number of bytes appended to the `fs` buffer.
 */
static int                  sprint_str_escaped(fs *restrict out, const char *restrict s) {
    invraisecode(out != NULL && s != NULL, ERR_NULLABLE_PTR, 
            "Null pointers %p %p", out, s);

    int    len = fs_len(out); 
    fsnew  iter = fsiapp(out);   //  not fsinew(out);, concat logic

    elemnext(iter) = '"';
    for (char p = *s; p; p = *++s) {
        switch (p) {
            case '"':  elemnext(iter) = '\\'; elemnext(iter) = '"';  break;
            case '\\': elemnext(iter) = '\\'; elemnext(iter) = '\\'; break;
            case '\n': elemnext(iter) = '\\'; elemnext(iter) = 'n';  break;
            case '\r': elemnext(iter) = '\\'; elemnext(iter) = 'r';  break;
            case '\t': elemnext(iter) = '\\'; elemnext(iter) = 't';  break;
            default:   elemnext(iter) = p;   break;
        }
    }
    elemnext(iter) = '"';
    elemend(iter);
    return fs_len(out) - len;
}
/** @} */

// print adapters, actually format must be configurable in context.c
/**
 * @name Formatted Output Functions
 * @brief Specialized functions for printing value64 objects in quoted format.
 * 
 * These functions are responsible for converting the internal representation
 * of a value64 into a human-readable, quoted string in a file stream.
 * @{
 */

/** @brief Prints an integer value wrapped in quotes. */
int                         value64_fprint_int(FILE *restrict out, value64 val) {
    return fprintf(out, "\"%d\"", value64_int(val) );
}
/** @brief Prints a long integer value wrapped in quotes. */
int                         value64_fprint_long(FILE *restrict out, value64 val) {
    return fprintf(out, "\"%ld\"", value64_long(val) );
}
/** @brief Prints a unsigned long integer value wrapped in quotes. */
int                         value64_fprint_ulong(FILE *restrict out, value64 val) {
    return fprintf(out, "\"%lu\"", value64_ulong(val) );
}
/** @brief Prints a double value wrapped in quotes using precision settings. */
int                         value64_fprint_dbl(FILE *restrict out, value64 val) {
    return fprintf(out, "\"%.*g\"", DBL_DECIMAL_DIG, value64_dbl(val) );
}
/** @brief Prints a pointer address wrapped in quotes. */
int                         value64_fprint_ptr(FILE *restrict out, value64 val) {
    return fprintf(out, "\"%p\"", value64_ptr(val) );
}
/** @brief Prints a single character wrapped in quotes. */
int                         value64_fprint_char(FILE *restrict out, value64 val) {
    char    buf[] = { value64_char(val), '\0' };
    return fprint_str_escaped(out, buf);
}
/** @brief Prints a bool wrapped in quotes. */
int                         value64_fprint_bool(FILE *restrict out, value64 val) {
    return fprint_str_escaped(out, bool_str(value64_bool(val)) );
}
/** @brief Prints an escaped string. */
int                         value64_fprint_str(FILE *restrict out, value64 val) {
    return fprint_str_escaped(out, value64_str(val) );
}
/** @brief Prints a fs  wrapped in quotes. */
int                         value64_fprint_fs(FILE *restrict out, value64 val) {
    return fprint_str_escaped(out, fs_str(value64_fs(val) ) ); // till fs_fprint isn'y support escaping
}
/** @brief Prints a FILE * pointer address wrapped in quotes. */
int                         value64_fprint_FILE(FILE *restrict out, value64 val) {
    return fprintf(out, "\"%p\"", value64_FILE(val) );
}
/** @} */

typedef int                                 (*value64_formatter_fn)(FILE *restrict, value64);
typedef bool                                (*value64_fs_reader_fn)(value64 *restrict, fs *restrict);
typedef int                                 (*value64_fs_writer_fn)(fs *restrict, value64);
// TODO: need to be refactored to use DS as common source!
typedef struct {
    value64_formatter_fn        file_writer;        // write TYPE from FILE *
    value64_fs_writer_fn        fs_writer;     // write TYPE to fs*
    value64_fs_reader_fn        fs_reader;          // read TYPE from fs
} value64_IO;


static const value64_IO                     value64_io_adapters[] = {
    [VALUE64_INT]   = { .file_writer = value64_fprint_int,      .fs_writer = value64_tostr_int,      .fs_reader = value64_sreadval_int },
    [VALUE64_LONG]  = { .file_writer = value64_fprint_long,     .fs_writer = value64_tostr_long,     .fs_reader = value64_sreadval_long },
    [VALUE64_ULONG] = { .file_writer = value64_fprint_ulong,    .fs_writer = value64_tostr_ulong,    .fs_reader = value64_sreadval_ulong },
    [VALUE64_DBL]   = { .file_writer = value64_fprint_dbl,      .fs_writer = value64_tostr_dbl,      .fs_reader = value64_sreadval_dbl },
    [VALUE64_PTR]   = { .file_writer = value64_fprint_ptr,      .fs_writer = value64_tostr_ptr,      .fs_reader = NULL },
    [VALUE64_CHR]   = { .file_writer = value64_fprint_char,     .fs_writer = value64_tostr_char,     .fs_reader = value64_sreadval_char },
    [VALUE64_BOOL]  = { .file_writer = value64_fprint_bool,     .fs_writer = value64_tostr_bool,     .fs_reader = value64_sreadval_bool },
    [VALUE64_STR]   = { .file_writer = value64_fprint_str,      .fs_writer = value64_tostr_str,      .fs_reader = value64_sreadval_str },
    [VALUE64_FS]    = { .file_writer = value64_fprint_fs,       .fs_writer = value64_tostr_fs,       .fs_reader = value64_sreadval_fs},
    [VALUE64_FILE]  = { .file_writer = value64_fprint_FILE,     .fs_writer = NULL,                   .fs_reader = NULL }
};

/**
 * @name Generic Formatter
 * @brief High-level formatting function with message support and error handling.
 * 
 * This function acts as a dispatcher. It can print an optional message 
 * followed by the correctly formatted value64 object based on the provided type.
 *
 * @param out   The output file stream.
 * @param msg   An optional prefix message to print before the value.
 * @param val   The value64 object to be formatted.
 * @param typ   The type of the value (determines the formatting logic).
 * 
 * @return The total number of characters printed.
 * 
 * @throws ERR_UNSUPPORTED_TYPE if the provided type is not recognized by the formatter.
 * @note This function uses IOCHECKER to monitor and handle write errors.
 */
int                         value64_fprint_msg(FILE *restrict out, const char *restrict msg, value64 val, value64_type typ){
    int     cnt = 0;
    if (!out)
        return 0;

    if (typ >= 0 && typ < VALUE64_TYPE_COUNT && value64_io_adapters[typ].file_writer) {
        if (msg) {
            IOCHECKER(w, fprintf(out, "%s ", msg), -1)
               cnt += w;
        }
        IOCHECKER(w, value64_io_adapters[typ].file_writer(out, val), -1)
            cnt += w;
    } else {
        fprintf(out, "Unsupported %d!\n", typ);
        return userraise(-1, ERR_UNSUPPORTED_TYPE, "Unsupported %d!\n", typ);
    }

    return cnt;
}

/**
 * @brief technical printer
 *
 * @param out stream, opened for write
 * @param val the value64
 * @param typ type of value64
 * @note TO BE FERACTORED TO USE STANDARD SERIALIZATION
 */
int                        value64_techfprint(FILE *restrict out, value64 val, value64_type typ, const char *restrict name) {
    int     cnt = 0;
    if (!out)
        return 0;
    IOCHECKER(w, fprintf(out, "VALUE64:%s [", name ? name : "NULL"), -1)
        cnt += w;

    if (typ >= 0 && typ < VALUE64_TYPE_COUNT && value64_io_adapters[typ].file_writer) {
        IOCHECKER(w, value64_io_adapters[typ].file_writer(out, val), -1)
            cnt += w;
    } else {
        IOCHECKER(w, fprintf(out, "Unsupported TYPE %d!", typ), -1)
            cnt += w;
    }
    IOCHECKER(w, fprintf(out, "]"), -1)
        cnt += w;

    return cnt;
}

// --------------------------------- SERIALIZATION -----------------------------------------

// TODO: need to be refactored to universal DS
/**
 * @brief Serializes a value64 object into a human-readable text format in a file.
 * 
 * This function writes the value to the provided file stream. It can optionally
 * include type metadata (e.g., `VALUE64(INT):123`) to make the output 
 * self-describing, which is useful for debugging and logging.
 * 
 * @note This is a textual serialization. For high-performance data storage 
 *       and transfer, use the Data Serialization (DS) implementation.
 *
 * @param out           Pointer to the destination file stream.
 * @param val           The value64 object to be serialized.
 * @param typ           The type of the object (used to determine formatting).
 * @param savetypeinfo  If true, the output will include the type name as a prefix.
 * 
 * @return The total number of characters written to the file.
 * 
 * @throws ERR_NULLABLE_PTR if the output file stream `out` is NULL.
 */
int                             value64_tofile(FILE *out, value64 val, value64_type typ, bool savetypeinfo) {
    invraisecode(out != NULL, ERR_NULLABLE_PTR,
        "Null pointer");
    int cnt = 0;
    if (savetypeinfo) {
        IOCHECKER(w, fprintf(out, "VALUE64(%s):", value64_typename(typ) ), -1)
            cnt += w;
    } else {
        IOCHECKER(w, fprintf(out, "VALUE64:"), -1)
            cnt += w;
    }
    IOCHECKER(w, value64_fprint(out, val, typ), -1)
        cnt += w;
    IOCHECKER(w, fprintf(out, "\n"), -1)
        cnt += w;
    return cnt; //logsimpleret(cnt, "Saved 1 value");
}
// string readers
// fs must be initialized, val can be NULL, it means just check

/**
 * @name String-based Parsers and Readers
 * @brief Functions for parsing and reading value64 objects from an `fs` buffer.
 * 
 * These functions attempt to interpret the contents of a buffer (`fs`) as 
 * a specific type. They support a "dry-run" mode where no data is stored 
 * if the `pval` pointer is NULL.
 * 
 * @warning These functions are destructive in terms of error logging (via `logsimpleerr`) 
 *          and may raise exceptions if `buf` is NULL.
 * @{
 */

/**
 * @brief Reads a string from the buffer.
 * 
 * If `pval` is not NULL, the result is stored in `*pval`. 
 * If `pval` is NULL, the function only validates that the buffer is not NULL.
 * 
 * @param pval Pointer to store the resulting value64 object.
 * @param buf  The source buffer.
 * @return true if successful, false otherwise.
 */
bool                            value64_sreadval_str(value64 *restrict pval, fs *restrict buf) {
    invraisecode(buf != NULL, ERR_NULLABLE_PTR,
        "Null pointers %p", buf);

    value64 v = value64_createstr(fs_str(buf) );    // strdup here
    if (pval)
        *pval = v;
    return logsimpleret(true, "read %s %zu", pval == NULL ? "DUMMY" : "", fs_len(buf) );
}
/**
 * @brief Parses an integer from the buffer.
 * 
 * If parsing succeeds and `pval` is not NULL, the result is stored in `*pval`.
 * 
 * @param pval Pointer to store the parsed integer value.
 * @param buf  The source buffer.
 * @return true if parsing was successful, false if the string is not a valid integer.
 */
bool                            value64_sreadval_int(value64 *restrict pval, fs *restrict buf){
    invraisecode(buf != NULL, ERR_NULLABLE_PTR,
        "Null pointers %p", buf);

    int     ival;
    if (!try_parse_int(fs_str(buf), &ival))
         return logsimpleerr(false, "Invalid int string: '%.20s'", fs_str(buf) );
    value64 v = value64_createint(ival);
    if (pval)
        *pval = v;
    return logsimpleret(true, "read %s %zu", pval == NULL ? "DUMMY" : "", fs_len(buf) );
}
/**
 * @brief Parses a long integer from the buffer.
 * 
 * If parsing succeeds and `pval` is not NULL, the result is stored in `*pval`.
 * 
 * @param pval Pointer to store the parsed long value.
 * @param buf  The source buffer.
 * @return true if parsing was successful, false if the string is not a valid long.
 */
bool                            value64_sreadval_long( value64 *restrict pval, fs *restrict buf){
    invraisecode(buf != NULL, ERR_NULLABLE_PTR,
        "Null pointers %p", buf);

    long    lval;
    if (!try_parse_long(fs_str(buf), &lval))
        return logsimpleerr(false, "Invalid long string: '%.30s'", fs_str(buf) );
    value64 v = value64_createlong(lval);
    if (pval)
        *pval = v;
    return logsimpleret(true, "read %s %zu", pval == NULL ? "DUMMY" : "", fs_len(buf) );
}
/**
 * @brief Parses a unsigned long integer from the buffer.
 * 
 * If parsing succeeds and `pval` is not NULL, the result is stored in `*pval`.
 * 
 * @param pval Pointer to store the parsed long value.
 * @param buf  The source buffer.
 * @return true if parsing was successful, false if the string is not a valid unsigned long.
 */
bool                            value64_sreadval_ulong( value64 *restrict pval, fs *restrict buf){
    invraisecode(buf != NULL, ERR_NULLABLE_PTR,
        "Null pointers %p", buf);

    unsigned long    lval;
    if (!try_parse_ulong(fs_str(buf), &lval))
        return logsimpleerr(false, "Invalid long string: '%.30s'", fs_str(buf) );
    value64 v = value64_createulong(lval);
    if (pval)
        *pval = v;
    return logsimpleret(true, "read %s %zu", pval == NULL ? "DUMMY" : "", fs_len(buf) );
}
/**
 * @brief Parses a double from the buffer.
 * 
 * If parsing succeeds and `pval` is not NULL, the result is stored in `*pval`.
 * 
 * @param pval Pointer to store the parsed double value.
 * @param buf  The source buffer.
 * @return true if parsing was successful, false if the string is not a valid double.
 */
bool                            value64_sreadval_dbl(value64 *restrict pval, fs *restrict buf){
    invraisecode(buf != NULL, ERR_NULLABLE_PTR,
        "Null pointers %p", buf);

    double  dval;
    if (!try_parse_double(fs_str(buf), &dval))
        return logsimpleerr(false, "Invalid double string: '%.50s'", fs_str(buf) );
    value64 v = value64_createdbl(dval);
    if (pval)
        *pval = v;
    return logsimpleret(true, "read %s %zu", pval == NULL ? "DUMMY" : "", fs_len(buf) );
}
/**
 * @brief Validates/Extracts an FS resource from the buffer.
 * 
 * This function treats the buffer itself as the resource.
 * 
 * @param pval Pointer to store the resulting FS value64 object.
 * @param buf  The source buffer.
 * @return true always (if buf is not NULL).
 */
bool                            value64_sreadval_fs(value64 *restrict pval, fs *restrict buf){
    invraisecode(buf != NULL, ERR_NULLABLE_PTR,
        "Null pointers %p", buf);

    value64  v = value64_createfs(buf);
    if (pval)
         *pval = v;
    return logsimpleret(true, "read %s %zu", pval == NULL ? "DUMMY" : "", fs_len(buf) );
}
/**
 * @brief Parses a single character from the buffer.
 * 
 * If parsing succeeds and `pval` is not NULL, the result is stored in `*pval`.
 * 
 * @param pval Pointer to store the parsed character.
 * @param buf  The source buffer.
 * @return true if parsing was successful, false otherwise.
 */
bool                         value64_sreadval_char(value64 *restrict pval, fs *restrict buf) {
    invraisecode(buf != NULL, ERR_NULLABLE_PTR,
            "Null pointers %p", buf);

    const char *str = fs_str(buf);

    // ANALYZE: this is not pretty clear behaviour, probably better return false in that case
    if (str == NULL || str[0] == '\0') {
        if (pval) 
            *pval = value64_createchar('\0');
        return logsimpleret(true, "read null char from \"\"");
    }
    char cval;
    if (!try_parse_char(str, &cval))
        return logsimpleerr(false, "Invalid char string: '%s'", str);

    value64 v = value64_createchar(cval);
    if (pval)
        *pval = v;
     return logsimpleret(true, "read %s %zu", pval == NULL ? "DUMMY" : "", fs_len(buf) );       
}
/**
 * @brief Reads a boolean value from a string buffer.
 *
 * Calls the existing try_parse_bool() function.  An empty string is treated
 * as false (as if "false" were written).
 *
 * @param pval  pointer to the value64 to fill (may be NULL)
 * @param buf   fast‑string containing the textual representation
 * @return      true on success, false on parse error
 */
bool                            value64_sreadval_bool(value64 *restrict pval, fs *restrict buf) {
    invraisecode(buf != NULL, ERR_NULLABLE_PTR, 
            "Null pointer %p", buf);
    const char *str = fs_str(buf);

    bool        b;
    if (!try_parse_bool(str, &b))
        return logsimpleerr(false, "Invalid bool string: '%s'", str);

    if (pval)
        *pval = value64_createbool(b);
    return logsimpleret(true, "read bool = %s", b ? "true" : "false");
}

/**
 * @brief Reads a boolean value from a string buffer.
 *
 * Calls the existing try_parse_bool() function.  An empty string is treated
 * as false (as if "false" were written).
 * TODO: only getconvstring_ds read a DS!!!  Refactoring is required
 *
 * @param pval  pointer to the value64 to fill (may be NULL)
 * @param buf   fast‑string containing the textual representation
 * @return      true on success, false on parse error
 */

bool                            value64_dsreadval(DS *restrict ds, value64_type typ, value64 *restrict val, fs *restrict buf) {
    invraisecode(ds != NULL && buf != NULL, ERR_NULLABLE_PTR,
        "Null pointers %p %p", ds, buf);

    // TODO: need to be refactored to make dapters use directly DS
    if (!getconvstring_ds(ds, buf, true) )
        return userraise(false, ERR_WRONG_INPUT_FORMAT, "EOF or wrong format");
    
    if (typ >= 0 && typ < VALUE64_TYPE_COUNT && value64_io_adapters[typ].file_writer)
        return value64_io_adapters[typ].fs_reader(val, buf);
    else
        return userraise(false, ERR_UNSUPPORTED_TYPE, "Reading of %d/%s isn't supported", typ, value64_typename(typ) );
}
/** @} */

/**
 * @name Data Deserialization Engine
 * @brief Functions for parsing serialized value64 objects from files or memory.
 * @{
 */

/**
 * @brief Parses the metadata header of a serialized value.
 * 
 * This function extracts the type information from the input stream.
 * Supported formats:
 * - Explicit: `VALUE64(TYPE) :` (e.g., `VALUE64(INT) :`)
 * - Implicit: `VALUE64 :`
 * 
 * @param pds           Pointer to the Data Stream structure.
 * @param loadtypeinfo  If true, the function parses the type from the header.
 *                      If false, it uses the provided `typ` directly.
 * @param typ           The assumed type of the data (used if loadtypeinfo is false).
 * 
 * @return The parsed `value64_type`.
 * @throws ERR_WRONG_INPUT_FORMAT if the header format is incorrect.
 * @throws ERR_UNKNOWN_TYPE if the parsed type string is not recognized.
 */
static value64_type             value64_parse_header(DS *pds, bool loadtypeinfo, value64_type typ) {
    #define VALUE64_FLOAD_FORMAT_LEN        32
    #define VALUE64_FLOAD_FORMAT_MUNUS1     31

    value64_type    loadedtyp = VALUE64_UNKNOWN;
    char            type_str[VALUE64_FLOAD_FORMAT_LEN];
    int             headercnt = 0;
    if (loadtypeinfo) {
        // Format: VALUE64(INT) : или VALUE64(STR) :
        if (pds->type == DS_FILE) {
            if (fscanf(pds->fp, "VALUE64(%" TOSTRING(VALUE64_FLOAD_FORMAT_MUNUS1) "[^)]) :", type_str) != 1)
                return userraise(ERR_WRONG_INPUT_FORMAT, false, "Missing or invalid VALUE64 header");
        } else if (dsIsstr(pds) ) {
            if (sscanf(dsStrbuf(pds), "VALUE64(%" TOSTRING(VALUE64_FLOAD_FORMAT_MUNUS1) "[^)]) :%n", type_str, &headercnt) != 1)
                return userraise(ERR_WRONG_INPUT_FORMAT, false, "Missing or invalid VALUE64 header");
        }
        loadedtyp = value64_gettype(type_str);
        if (loadedtyp == VALUE64_UNKNOWN)
            return userraise(ERR_UNKNOWN_TYPE, false, "Unknown type '%s'", type_str);
    } else {
        // Без информации о типе – просто VALUE64:
        if (pds->type == DS_FILE) {
            if (fscanf(pds->fp, "VALUE64 :") != 0)   // fscanf returns 0 if ok
                return userraise(ERR_WRONG_INPUT_FORMAT, false, "Missing VALUE64 header");
        } else if (dsIsstr(pds) ) {
            if (sscanf(dsStrbuf(pds), "VALUE64 :%n", &headercnt) != 0)   // fscanf returns 0 if ok
                return userraise(ERR_WRONG_INPUT_FORMAT, false, "Missing VALUE64 header");
        }
        loadedtyp = typ;    // use imcoming
    }
    if (dsIsstr(pds) )
        pds->pos += headercnt;
    #undef VALUE64_FLOAD_FORMAT_LEN
    #undef VALUE64_FLOAD_FORMAT_MUNUS1
    return loadedtyp;
}
/**
 * @brief Loads a single value64 object from a Data Stream.
 * 
 * This function handles the full lifecycle of reading a value:
 * 1. Parsing the header (metadata).
 * 2. Managing temporary buffers if no user buffer is provided.
 * 3. Triggering the specific value reader.
 * 4. Calculating the offset for the next read.
 * 
 * @param pds           Pointer to the Data Stream.
 * @param val           Pointer to the destination `value64` object.
 * @param typ           The type of the data to be loaded.
 * @param buf           An optional pre-allocated `fs` buffer. If NULL, 
 *                      a temporary buffer is allocated and freed locally.
 * 
 * @return The number of bytes consumed from the stream (offset).
 * 
 * @throws ERR_NULLABLE_PTR if `pds` is NULL.
 * @throws ERR_WRONG_INPUT_FORMAT if the stream header is corrupted.
 * @throws ERR_UNKNOWN_TYPE if the parsed header contains an invalid type.
 * 
 * @note If `buf` is NULL, the function performs a local allocation for the 
 *       intermediate buffer to ensure memory safety.
 */
int                         value64_loadds(DS *restrict pds, value64 *restrict val, value64_type typ, bool loadtypeinfo, fs *restrict buf) {
    invraisecode(pds != NULL, ERR_NULLABLE_PTR,
        "Null pointers %p", pds);

    int             currpos = dsIsstr(pds) ? pds->pos : 0;
    value64_type    newtyp = value64_parse_header(pds, loadtypeinfo, typ);
    if (newtyp == VALUE64_UNKNOWN)
        userraise(-1, ERR_UNKNOWN_TYPE, "Unknown type parsed");

    // Temporary fs, will be ref here to avoid allocation and free
    fs              tmp = FS();
    bool            localalloc = false;
    if (!buf) {
        tmp = fsinit(32);
        buf = &tmp;
        localalloc = true;
    }

    // Generic reader
    if (!value64_dsreadval(pds, newtyp, val, buf)) {
        if (localalloc)
            fsfree(tmp);
        return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Failed to read value");
    }

    if (localalloc)
        fsfree(tmp);
    // TODO: 1 for FILE * stream is NOT correct, value64_dsreadval must be refactored
    currpos = dsIsstr(pds) ? pds->pos - currpos : 1;
    return currpos;
}
/** @} */

// to string adapters, actually format must be configurable in context.c
/**
 * @name String Serialization Adapters
 * @brief Functions to convert value64 types into their string representations within an `fs` buffer.
 * 
 * These functions are responsible for formatting the data of a value64 object 
 * into a quoted or escaped string format. They are designed to be used for 
 * text-based serialization (e.g., writing to log files or CSV/JSON-like formats).
 * @{
 */

/**
 * @brief Converts an integer to a quoted string.
 * @param target The target `fs` buffer to which the result is appended.
 * @param val    The source value64 containing an integer.
 * @return The number of bytes written to the buffer.
 */
int                         value64_tostr_int(fs *target, value64 val) {
    return fs_sprintf_concat(target, "\"%d\"", value64_int(val) );
}
/**
 * @brief Converts a long integer to a quoted string.
 * @param target The target `fs` buffer.
 * @param val    The source value64 containing a long.
 * @return The number of bytes written.
 */
int                         value64_tostr_long(fs *target, value64 val) {
    return fs_sprintf_concat(target, "\"%ld\"", value64_long(val) );
}
/**
 * @brief Converts a unsgined long integer to a quoted string.
 * @param target The target `fs` buffer.
 * @param val    The source value64 containing a unsigned long.
 * @return The number of bytes written.
 */
int                         value64_tostr_ulong(fs *target, value64 val) {
    return fs_sprintf_concat(target, "\"%lu\"", value64_long(val) );
}
/**
 * @brief Converts a double to a quoted string with precision handling.
 * @param target The target `fs` buffer.
 * @param val    The source value64 containing a double.
 * @return The number of bytes written.
 */
int                         value64_tostr_dbl(fs *target, value64 val) {
    return fs_sprintf_concat(target, "\"%.*g\"", DBL_DECIMAL_DIG, value64_dbl(val) );
}
/**
 * @brief Converts a pointer address to a quoted string representation.
 * @param target The target `fs` buffer.
 * @param val    The source value64 containing a pointer.
 * @return The number of bytes written.
 */
int                         value64_tostr_ptr(fs *target, value64 val) {
    return fs_sprintf_concat(target, "\"%p\"", value64_ptr(val) );
}
/**
 * @brief Escapes and converts a string into the target buffer.
 * @details Uses `sprint_str_escaped` to ensure special characters 
 *          (like \n, \t, \) are properly escaped.
 * @param target The target `fs` buffer.
 * @param val    The source value64 containing a string.
 * @return The number of bytes written.
 */
int                         value64_tostr_str(fs *target, value64 val) {
    return sprint_str_escaped(target, value64_str(val) );
}
/**
 * @brief Converts a filesystem path into an escaped string in the target buffer.
 * @param target The target `fs` buffer.
 * @param val    The source value64 containing an `fs` resource.
 * @return The number of bytes written.
 */
int                         value64_tostr_fs(fs *target, value64 val) {
    return sprint_str_escaped(target, fs_str(value64_fs(val) ) ); // till fs_fprint isn'y support escaping
}
/**
 * @brief Converts a character to a quoted string.
 * @param target The target `fs` buffer.
 * @param val    The source value64 containing a character.
 * @return The number of bytes written.
 */
int                         value64_tostr_char(fs *target, value64 val) {
    return fs_sprintf_concat(target, "\"%c\"", value64_char(val) );
}
/**
 * @brief Converts a bool to a quoted string.
 * @param target The target `fs` buffer.
 * @param val    The source value64 containing a bool.
 * @return The number of bytes written.
 */
int                         value64_tostr_bool(fs *target, value64 val) {
    return fs_sprintf_concat(target, "\"%s\"", bool_str(value64_bool(val)) );
}
/** @} */

// to string : fs MUST be initialized
/**
 * @brief High-level serialization of a value64 object into a formatted string.
 * 
 * This function is the primary entry point for converting a value64 object 
 * into a human-readable string stored in an `fs` buffer. It supports an 
 * optional "self-describing" mode where the type metadata is included 
 * in the output.
 * 
 * @details The output format follows these rules:
 * - With type info: `VALUE64(TYPE_NAME):<escaped_value>` (e.g., `VALUE64(INT):123`)
 * - Without type info: `VALUE64:<escaped_value>` (e.g., `VALUE64:123`)
 * 
 * The function handles all necessary escaping (for strings and file paths) 
 * and error checking during the process.
 *
 * @param target         Pointer to the target `fs` buffer. Must be a heap-allocated 
 *                       buffer to ensure memory safety during writes.
 * @param val            The `value64` object to be serialized.
 * @param typ            The type of the object (used for header and dispatching).
 * @param savetypeinfo   If true, the output includes the type name in the header.
 * 
 * @return The total number of characters successfully written to the buffer.
 * 
 * @throws ERR_NULLABLE_PTR if the `target` buffer is NULL.
 * @throws ERR_UNSUPPORTED_TYPE if the provided `typ` is not recognized.
 * @throws ERR_WRONG_INPUT_FORMAT if writing to the buffer fails.
 */
int                          value64_tostr(fs *target, value64 val, value64_type typ, bool savetypeinfo){
    invraisecode(fs_isheapalloc(target), ERR_NULLABLE_PTR,
        "Not heap allocated or null %p %d", target, target ? target->flags : -1);
    
    int     cnt = 0;

    if (savetypeinfo)
        IOCHECKER(w, fs_sprintf_concat(target, "VALUE64(%s):", value64_typename(typ)), -1)
            cnt += w;
    else 
        IOCHECKER(w, fs_sprintf_concat(target, "VALUE64:"), -1)
            cnt += w;

    if (typ >= 0 && typ < VALUE64_TYPE_COUNT && value64_io_adapters[typ].fs_writer) {
        IOCHECKER(w, value64_io_adapters[typ].fs_writer(target, val), -1)
            cnt += w;
    } else
        return userraise(-1, ERR_UNSUPPORTED_TYPE, "Type %d/%s serialization isn't supported", typ, value64_typename(typ));

    return cnt;
}

// ---------------------------------------- Filters ------------------------------------------------
/**
 * @name Value64 Filtering Module
 * @brief High-performance predicates for filtering value64 objects.
 * 
 * This module provides a wide range of filters for various data types,
 * including trivial filters, filesystem-based filters, and numerical 
 * relational operators.
 * @{
 */

/**
 * @name Trivial Filters
 * @brief Filters that always return a constant value.
 * @{
 */
/** @brief Always returns true. Used as a neutral element in filter chains. */
bool                        value64_filter_true(value64 v, value64 data) {
    (void)v; (void)data;
    return true;
}
/** @brief Always returns false. Used to exclude all elements. */
bool                        value64_filter_false(value64 v, value64 data) {
    (void)v; (void)data;
    return false;
}
/** @} */
// ---------------- fs filters -----------------

/**
 * @name faststring (fs) Filters
 * @brief Filters that perform operations on filesystem/resource types.
 * @{
 */

/** 
 * @brief Checks if the fs length is at least the specified integer value.
 *      fs filters (assuming v as fs*), data as int, check len >= data
 * 
 * @param v The value64 object containing the FS resource.
 * @param data The target minimum length.
 * @return true if fs_len(v) >= data.
 */
bool                        value64_filter_fsminlen_int(value64 v, value64 data) {
    const fs *f = value64_fs(v);
    return !fs_isnull(f) && (int) fs_len(f) >= value64_int(data);
}
/** 
 * @brief Checks if the fs length is at most the specified integer value.
 * @param v The value64 object containing the FS resource.
 * @param data The target maximum length.
 * @return true if fs_len(v) <= data.
 */
bool                        value64_filter_fsmaxlen_int(value64 v, value64 data) {
    const fs *f = value64_fs(v);
    return !fs_isnull(f) && (int) fs_len(f) <= value64_int(data);
}
/** 
 * @brief Checks if the fs  length matches the specified integer value exactly.
 * @param v The value64 object containing the FS resource.
 * @param data The target length.
 * @return true if fs_len(v) == data.
 */
bool                        value64_filter_fslen_int(value64 v, value64 data) {
    const fs *f = value64_fs(v);
    return !fs_isnull(f) && (int) fs_len(f) == value64_int(data);
}

/** 
 * @brief Checks if the fs  starts with the specified string prefix.
 * @param v The value64 object containing the FS resource.
 * @param data The value64 object containing the prefix string.
 * @return true if the fs starts with the string.
 */
bool                        value64_filter_fsprefix_str(value64 v, value64 data) {
    const fs    *f = value64_fs(v);
    if (fs_isnull(f) || !value64_str(data) )
        return false;
    fs l = FSLITERAL(value64_str(data) );
    return fs_ncmp(f, &l, fslen(l) ) == 0;
}
/** 
 * @brief Checks if the fs  is exactly equal to the specified string.
 * @param v The value64 object containing the FS resource.
 * @param data The value64 object containing the target string.
 * @return true if the fs  matches the string exactly.
 */
bool                        value64_filter_fsequals_str(value64 v, value64 data) {
    const fs *f = value64_fs(v);
    return !fs_isnull(f) && value64_str(data)
            && strcmp(f->v, value64_str(data) ) == 0;    // dangerous one
}
/** 
 * @brief Performs a "LIKE" operation (substring search) for fs.
 * @param v The value64 object containing the FS resource.
 * @param data The value64 object containing the search pattern.
 * @return true if the pattern exists anywhere within the fs.
 */
bool                        value64_filter_fslike_str(value64 v, value64 data) {
    const fs *f = value64_fs(v);
    if (fs_isnull(f) || !value64_str(data))
        return false;
    fs needle = FSLITERAL(value64_str(data));
    return fs_instr(f, &needle) >= 0;   // -1 if not found
}
/** 
 * @brief Performs a case-insensitive "LIKE" operation for fs.
 * @param v The value64 object containing the FS resource.
 * @param data The value64 object containing the search pattern.
 * @return true if the pattern exists (case-insensitive) within the fs.
 */
bool                        value64_filter_fsulike_str(value64 v, value64 data) {
    const fs *f = value64_fs(v);
    if (fs_isnull(f) || !value64_str(data))
        return false;
    fs needle = FSLITERAL(value64_str(data));
    return fs_iinstr(f, &needle) >= 0;  // -1 if not found
}
/** @} */

/**
 * @name Numerical Comparators
 * @brief Relational operators for different numeric types.
 * @{
 */

/** @name Integer Comparators (Int) */
/** @{ */
/** @brief Check if v < data. */
bool                        value64_filter_intlt_int(value64 v, value64 data) {
    return value64_int(v) < value64_int(data);
}
/** @brief Check if v <= data. */
bool                        value64_filter_intle_int(value64 v, value64 data) {
    return value64_int(v) <= value64_int(data);
}
/** @brief Check if v > data. . */
bool                        value64_filter_intgt_int(value64 v, value64 data) {
    return value64_int(v) >  value64_int(data);
}
/** @brief Check if v >= data. */
bool                        value64_filter_intge_int(value64 v, value64 data) {
    return value64_int(v) >= value64_int(data);
}
/** @brief Check if v == data. */
bool                        value64_filter_inteq_int(value64 v, value64 data) {
    return value64_int(v) == value64_int(data);
}
/** @brief Check if v != data. */
bool                        value64_filter_intne_int(value64 v, value64 data) {
    return value64_int(v) != value64_int(data);
}
/** @} */

// ======================== LONG vs LONG ============================

/** @name Long Comparators (Lng) */
/** @{ */
bool value64_filter_lnglt_lng(value64 v, value64 data) {
    return v.lval < data.lval;
}
bool value64_filter_lngle_lng(value64 v, value64 data) {
    return v.lval <= data.lval;
}
bool value64_filter_lnggt_lng(value64 v, value64 data) {
    return v.lval > data.lval;
}
bool value64_filter_lngge_lng(value64 v, value64 data) {
    return v.lval >= data.lval;
}
bool value64_filter_lngeq_lng(value64 v, value64 data) {
    return v.lval == data.lval;
}
bool value64_filter_lngne_lng(value64 v, value64 data) {
    return v.lval != data.lval;
}
/** @} */

// ======================== DOUBLE vs DOUBLE ========================

/** @name Double Comparators (Dbl) */
/** @{ */
bool value64_filter_dbllt_dbl(value64 v, value64 data) {
    return v.dval < data.dval;
}
bool value64_filter_dblle_dbl(value64 v, value64 data) {
    return v.dval <= data.dval;
}
bool value64_filter_dblgt_dbl(value64 v, value64 data) {
    return v.dval > data.dval;
}
bool value64_filter_dblge_dbl(value64 v, value64 data) {
    return v.dval >= data.dval;
}
bool value64_filter_dbleq_dbl(value64 v, value64 data) {
    return v.dval == data.dval;
}
bool value64_filter_dblne_dbl(value64 v, value64 data) {
    return v.dval != data.dval;
}
/** @} */

/** @name Range Comparators */
/** @{ */
/** 
 * @brief Checks if the integer value falls within the range [data1, data2].
 * @param v The value to check.
 * @param data1 The minimum bound.
 * @param data2 The maximum bound.
 * @return true if data1 <= v <= data2.
 */
bool                        value64_filter2_intbetween_int_int(value64 v, value64 data1, value64 data2){
    return value64_int(v) >= value64_int(data1) && value64_int(v) <= value64_int(data2);
}
/** @} */

/** @} */

// ------------------------------------ ETC. ----------------------------------------
// 
/**
 * @brief Validates a value64 value of a given type.
 *
 * For double, checks that the value is not NaN or ±Inf.
 * For fs, delegates to fs_validate().
 * For str, checks that the pointer is not NULL.
 * All other types are always valid.
 *
 * @param out Output stream for error messages (NULL defaults to stderr).
 * @param v   Value to validate.
 * @param typ Type of the value.
 * @return true if valid, false otherwise (with error message printed to out).
 * 
 * @note validation always use DIRECT fprintf (no userraise) to be able to work
 *       if you want to log using logging system, exec value64_validate(logfile, v, typ);
 */
bool                            value64_validate(FILE *out, value64 v, value64_type typ)
{
    switch (typ) {
        case VALUE64_DBL:
            if (isnan(v.dval) || isinf(v.dval)) {
                if (out)
                    fprintf(out, "Invalid double value: %f\n", v.dval);
                return logsimpleerr(false, "Invalid double value: %f\n", v.dval);
            }
            return true;

        case VALUE64_FS:
            if (!v.fsval) {
                if (out)
                    fprintf(out, "FS value is NULL\n");
                return logsimpleerr(false, "FS pointer is NULL");
            }
            return fs_validate(out, v.fsval);

        case VALUE64_STR:
            if (!v.sval) {
                if (out)
                    fprintf(out, "String value is NULL\n");
                return logsimpleerr(false, "String value is NULL");
            }
            return true;

        default:   /* INT, LNG, BOOL, CHR, PTR, ULONG – always valid */
            return true;
    }
}


// ---------------------------------------- Testing ------------------------------------------
#ifdef VALUE64TESTING

#include "test.h"

//types for testing

// ------------------------- TEST init_free ---------------------------------

static TestStatus
tf_init_free(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. int */
    test_sub("subtest %d: value64 int", ++subnum);
    {
        value64 v = value64_createint(42);
        test_validatefree(
            v.ival == 42,
            value64free(v, VALUE64_INT),
            "Int value mismatch: got %d, expected 42", v.ival
        );
        test_validatefree(
            value64_int(v) == 42,
            value64free(v, VALUE64_INT),
            "Int value mismatch: got %d, expected 42", v.ival
        );
        // для int освобождение не требуется
        value64free(v, VALUE64_INT);
    }

    /* 2. long */
    test_sub("subtest %d: value64 long", ++subnum);
    {
        value64 v = value64_createlong(1234567890L);
        test_validate(
            v.lval == 1234567890L,
            "Long value mismatch: got %ld, expected 1234567890", v.lval
        );
        test_validate(
            value64_long(v) == 1234567890L,
            "Long value mismatch: got %ld, expected 1234567890", v.lval
        );
    }
    /* unsigned long */
    test_sub("subtest %d: value64 ulong", ++subnum);
    {
        value64 v = value64_createulong(1234567890L);
        test_validate(
            v.lval == 1234567890UL,
            "Long value mismatch: got %ld, expected 1234567890UL", v.ulval
        );
        test_validate(
            value64_ulong(v) == 1234567890UL,
            "Long value mismatch: got %ld, expected 1234567890U:", v.ulval
        );
    }

    /* 3. double */
    test_sub("subtest %d: value64 double", ++subnum);
    {
        value64 v = value64_createdbl(3.14159265);
        test_validate(
            fabs(v.dval - 3.14159265) < 0.00000001,
            "Double value mismatch: got %f, expected 3.14159265", v.dval
        );
        test_validate(
            fabs(value64_dbl(v) - 3.14159265) < 0.00000001,
            "Double value mismatch: got %f, expected 3.14159265", v.dval
        );
    }

    /* 4. pointer */
    test_sub("subtest %d: value64 pointer", ++subnum);
    {
        int x = 77;
        value64 v = value64_createptr(&x);
        test_validate(
            v.pval == &x,
            "Pointer mismatch: got %p, expected %p", v.pval, (void*)&x
        );
        test_validate(
            value64_ptr(v) == &x,
            "Pointer mismatch: got %p, expected %p", v.pval, (void*)&x
        );
    }

    /* 5. fs (copy) */
    test_sub("subtest %d: value64 createfs (copy)", ++subnum);
    {
        const char *text = "hello value64";
        fs orig = fscopy(text);
        value64 v = value64_createfs(&orig);

        test_validatefree(
            strcmp(fs_str(v.fsval), text) == 0,
            (fsfree(orig), fs_free(v.fsval)),
            "FS copy mismatch: got '%s', expected '%s'", fs_str(v.fsval), text
        );
        test_validatefree(
            strcmp(fs_str(value64_fs(v) ), text) == 0,
            (fsfree(orig), fs_free(v.fsval)),
            "FS copy mismatch: got '%s', expected '%s'", fs_str(v.fsval), text
        );
        test_validatefree(
            fs_bodyalloc(v.fsval),
            (fsfree(orig), fs_free(v.fsval)),
            "Copied fs must have FS_FLAG_BODYALLOC flag"
        );

        fsfree(orig);
        value64freefs(v);
        fs_alloc_check(true);
    }

    /* 6. fs (move) */
    test_sub("subtest %d: value64 movefs", ++subnum);
    {
        const char *text = "move me";
        fs orig = fscopy(text);
        value64 v = value64_movefs(&orig);   // orig будет опустошён

        test_validatefree(
            strcmp(fs_str(v.fsval), text) == 0,
            value64freefs(v),
            "Moved fs value mismatch: got '%s', expected '%s'", fs_str(v.fsval), text
        );
        test_validatefree(
            strcmp(fs_str(value64_fs(v) ), text) == 0,
            value64freefs(v),
            "Moved fs value mismatch: got '%s', expected '%s'", fs_str(v.fsval), text
        );
        test_validatefree(
            fs_bodyalloc(v.fsval),
            value64freefs(v),
            "Moved fs must have FS_FLAG_BODYALLOC flag"
        );
        // Проверяем, что оригинал действительно опустошён
        test_validatefree(
            fslen(orig) == 0 && fsstr(orig) == NULL,
            value64freefs(v),
            "After move, original fs must be empty (len=%zu, str=%p)", fslen(orig), (void*)fsstr(orig)
        );

        value64freefs(v);
        fsfree(orig);   // orig пуст, но fsfree безопасен
        fs_alloc_check(true);
    }

    /* 7. Множественные вызовы и проверка утечек */
    test_sub("subtest %d: value64 multiple create/free (leak check)", ++subnum);
    {
        const char *words[] = {"one", "two", "three"};
        value64 vals[COUNT(words)];

        for (int i = 0; i < COUNT(words); i++) {
            vals[i] = value64_createfs(
                &(fs){ .v = (char*)words[i], .len = strlen(words[i]), .sz = 0, .flags = FS_FLAG_STATIC }
            );   // временный fs для создания копии
        }

        for (int i = 0; i < COUNT(words); i++) {
            test_validatefree(
                strcmp(fs_str(vals[i].fsval), words[i]) == 0,
                (value64freefs(vals[0]), value64freefs(vals[1]), value64freefs(vals[2])),
                "FS %d mismatch: got '%s', expected '%s'", i, fs_str(vals[i].fsval), words[i]
            );
        }

        for (int i = 0; i < COUNT(words); i++)
            value64freefs(vals[i]);
        fs_alloc_check(true);
    }
    test_sub("subtest %d: value64 str", ++subnum);
    {
        const char *text = "hello c-string";
        value64 v = value64_createstr(text);

        test_validatefree(
            strcmp(v.sval, text) == 0,
            value64freestr(v),
            "Str copy mismatch: got '%s', expected '%s'", v.sval, text
        );
        test_validatefree(
            strcmp(value64_str(v), text) == 0,
            value64freestr(v),
            "Str copy mismatch: got '%s', expected '%s'", v.sval, text
        );

        free(v.sval);
    }
    // VALUE64_CHR simple init/validate -------------------------

    /* 1. Создание символа и проверка значения */
    test_sub("subtest %d: create and read char", ++subnum);
    {
        value64 v = value64_createchar('A');
        test_validate(value64_char(v) == 'A',
                      "Created char must be 'A', got '%c'", value64_char(v));
        // для char нет владеющих ресурсов, освобождать нечего
    }

    /* 2. Создание нулевого символа */
    test_sub("subtest %d: create zero char", ++subnum);
    {
        value64 v = value64_createchar('\0');
        test_validate(value64_char(v) == '\0',
                      "Zero char must be '\\0', got '%c'", value64_char(v));
    }

    /* 3. Проверка типа */
    test_sub("subtest %d: type identification", ++subnum);
    {
        value64 v = value64_createchar('Z');
        (void) v;
        test_validate(value64_gettype(value64_typename(VALUE64_CHR)) == VALUE64_CHR,
                      "Type must be VALUE64_CHR");
        // также можно проверить через value64_typename
    }

    /* BOOL */
    test_sub("subtest %d: create and read true", ++subnum);
    {
        value64 v = value64_createbool(true);
        test_validate(value64_bool(v) == true,
                      "Created bool must be true, got %s", value64_bool(v) ? "true" : "false");
    }

    /* BOOL false */
    test_sub("subtest %d: create and read false", ++subnum);
    {
        value64 v = value64_createbool(false);
        test_validate(value64_bool(v) == false,
                      "Created bool must be false, got %s", value64_bool(v) ? "true" : "false");
    }

    /* 3. Проверка типа */
    test_sub("subtest %d: type identification", ++subnum);
    {
        value64 v = value64_createbool(true);
        (void) v;
        test_validate(value64_gettype(value64_typename(VALUE64_BOOL)) == VALUE64_BOOL,
                      "Type must be VALUE64_BOOL");
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_pcopy_move ---------------------------------
static TestStatus
tf_point_init(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- value64_pinit (copy) ---------- */

    /* 1. copy int */
    test_sub("subtest %d: pinit int", ++subnum);
    {
        int     ival = 123;
        value64 v = value64_pinit(&ival, VALUE64_INT);
        test_validate(v.ival == 123, "Copy int: got %d, expected 123", v.ival);
    }

    /* 2. copy long */
    test_sub("subtest %d: pinit long", ++subnum);
    {
        long    lval = 999999999L;
        value64 v = value64_pinit(&lval, VALUE64_LONG);
        test_validate(v.lval == 999999999L, "Copy long: got %ld, expected 999999999", v.lval);
    }
    /* copy unsigned long */
    test_sub("subtest %d: pinit ulong", ++subnum);
    {
        long    lval = 999999999UL;
        value64 v = value64_pinit(&lval, VALUE64_ULONG);
        test_validate(v.ulval == 999999999UL, "Copy ulong: got %lu, expected 999999999", v.ulval);
    }

    /* 3. copy double */
    test_sub("subtest %d: pinit double", ++subnum);
    {
        double  dval = 2.7182818;
        value64 v = value64_pinit(&dval, VALUE64_DBL);
        test_validate(fabs(v.dval - 2.7182818) < 0.0000001,
                      "Copy double: got %f, expected 2.7182818", v.dval);
    }

    /* 4. copy char */
    test_sub("subtest %d: pinit char", ++subnum);
    {
        char    cval = 'X';
        value64 v = value64_pinit(&cval, VALUE64_CHR);
        test_validate(v.cval == 'X', "Copy char: got '%c', expected 'X'", v.cval);
    }
    /* 4. copy bool */
    test_sub("subtest %d: pinit bool", ++subnum);
    {
        char    bval = true;
        value64 v = value64_pinit(&bval, VALUE64_BOOL);
        test_validate(v.bval == true, "Copy bool: got '%s', expected 'true'", bool_str(v.bval) );
    }

    /* 4. copy pointer */
    test_sub("subtest %d: pinit pointer", ++subnum);
    {
        int     x = 5;
        void   *ptr = &x;
        value64 v = value64_pinit(&ptr, VALUE64_PTR);
        test_validate(v.pval == ptr,
                      "Copy pointer: got %p, expected %p", v.pval, ptr);
    }

    /* 5. copy C-string */
    test_sub("subtest %d: pinit str", ++subnum);
    {
        const char  *text = "copy-me";
        value64      v = value64_pinit(text, VALUE64_STR);
        test_validatefree(
            strcmp(v.sval, text) == 0,
            value64freestr(v),
            "Copy str: got '%s', expected '%s'", v.sval, text
        );
        test_validatefree(
            v.sval != text,
            value64freestr(v),
            "Copy str must have different address from original"
        );
        value64freestr(v);
    }

    /* 6. copy fs */
    test_sub("subtest %d: pinit fs", ++subnum);
    {
        const char *text = "fs-copy";
        fs          orig = fscopy(text);
        value64     v = value64_pinit(&orig, VALUE64_FS);

        test_validatefree(
            strcmp(fs_str(v.fsval), text) == 0,
            (fsfree(orig), value64freefs(v)),
            "Copy fs: got '%s', expected '%s'", fs_str(v.fsval), text
        );
        test_validatefree(
            fs_bodyalloc(v.fsval),
            (fsfree(orig), value64freefs(v)),
            "Copy fs must have FS_FLAG_BODYALLOC"
        );

        fsfree(orig);
        value64freefs(v);
        fs_alloc_check(true);
    }

    /* ---------- value64_pmove (move) ---------- */

    /* ---------- INT ---------- */
    test_sub("subtest %d: move INT", ++subnum);
    {
        int ival = 42;
        value64 v = value64_pmove(&ival, VALUE64_INT);
        test_validate(v.ival == 42 && ival == 0,
                      "Move INT: v=%d, ival=%d (expected 42, 0)", v.ival, ival);
    }

    /* ---------- LONG ---------- */
    test_sub("subtest %d: move LONG", ++subnum);
    {
        long lval = 999888777L;
        value64 v = value64_pmove(&lval, VALUE64_LONG);
        test_validate(v.lval == 999888777L && lval == 0L,
                      "Move LONG: v=%ld, lval=%ld (expected 999888777, 0)", v.lval, lval);
    }
    /* ---------- ULONG ---------- */
    test_sub("subtest %d: move ULONG", ++subnum);
    {
        unsigned long   ulval = ULONG_MAX;
        value64         v = value64_pmove(&ulval, VALUE64_ULONG);
        test_validate(v.ulval == ULONG_MAX && ulval == 0L,
                      "Move LONG: v=%ld, lval=%ld (expected ULONG_MAX, 0)", v.ulval, ulval);
    }
    /* ---------- ULONG ---------- */
    test_sub("subtest %d: move ULONG", ++subnum);
    {
        unsigned long   ulval = 999888777UL;
        value64         v = value64_pmove(&ulval, VALUE64_ULONG);
        test_validate(v.ulval == 999888777UL && ulval == 0L,
                      "Move LONG: v=%ld, lval=%ld (expected 999888777UL, 0)", v.ulval, v.ulval);
    }

    /* ---------- DBL ---------- */
    test_sub("subtest %d: move DOUBLE", ++subnum);
    {
        double dval = 3.14159;
        value64 v = value64_pmove(&dval, VALUE64_DBL);
        test_validate(v.dval == 3.14159 && dval == 0.0,
                      "Move DOUBLE: v=%f, dval=%f (expected 3.14159, 0.0)", v.dval, dval);
    }

    /* ---------- CHR ---------- */
    test_sub("subtest %d: move CHAR", ++subnum);
    {
        char cval = 'A';
        value64 v = value64_pmove(&cval, VALUE64_CHR);
        test_validate(v.cval == 'A' && cval == '\0',
                      "Move CHAR: v='%c', cval='%c' (expected 'A', '\\0')", v.cval, cval);
    }
    /* ---------- BOOL ---------- */
    test_sub("subtest %d: move BOOL", ++subnum);
    {
        bool    bval = true;
        value64 v = value64_pmove(&bval, VALUE64_BOOL);
        test_validate(v.bval == true && bval == false,
                      "Move BOOL: v='%s', bval='%s' (expected 'true', 'false')", bool_str(v.bval), bool_str(bval) );
    }

    /* 11. move C-string (забирает владение) */
    test_sub("subtest %d: pmove str", ++subnum);
    {
        char *text = strdup("move-str");
        value64 v = value64_pmove(text, VALUE64_STR);
        test_validatefree(
            strcmp(v.sval, "move-str") == 0,
            free(v.sval),
            "Move str: got '%s', expected 'move-str'", v.sval
        );
        // text больше не владеет памятью, его нельзя освобождать
        free(v.sval);
    }

    /* 12. move fs (оригинал опустошается) */
    test_sub("subtest %d: pmove fs", ++subnum);
    {
        const char *text = "fs-move";
        fs orig = fscopy(text);
        value64 v = value64_pmove(&orig, VALUE64_FS);

        test_validatefree(
            strcmp(fs_str(v.fsval), text) == 0,
            fs_free(v.fsval),
            "Move fs: got '%s', expected '%s'", fs_str(v.fsval), text
        );
        test_validatefree(
            fs_bodyalloc(v.fsval),
            fs_free(v.fsval),
            "Move fs must have FS_FLAG_BODYALLOC"
        );
        test_validatefree(
            fslen(orig) == 0 && fsstr(orig) == NULL,
            fs_free(v.fsval),
            "After move, original fs must be empty (len=%zu, str=%p)", fslen(orig), (void*)fsstr(orig)
        );

        fs_free(v.fsval);
        fsfree(orig);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_clone ---------------------------------

static TestStatus
tf_clone(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- value64_create* (конструкторы) ---------- */

    /* 1. int */
    test_sub("subtest %d: create int", ++subnum);
    {
        value64 v = value64_createint(42);
        test_validate(v.ival == 42, "Create int: got %d, expected 42", v.ival);
    }

    /* 2. long */
    test_sub("subtest %d: create long", ++subnum);
    {
        value64 v = value64_createlong(1234567890L);
        test_validate(v.lval == 1234567890L, "Create long: got %ld, expected 1234567890", v.lval);
    }
    /*  ulong */
    test_sub("subtest %d: create ulong", ++subnum);
    {
        value64 v = value64_createulong(1234567890UL);
        test_validate(v.ulval == 1234567890UL, "Create ulong: got %lu, expected 1234567890", v.ulval);
    }

    /* 3. double */
    test_sub("subtest %d: create double", ++subnum);
    {
        value64 v = value64_createdbl(2.718281828);
        test_validate(fabs(v.dval - 2.718281828) < 0.000000001,
                      "Create double: got %f, expected 2.718281828", v.dval);
    }

    /* 4. pointer */
    test_sub("subtest %d: create pointer", ++subnum);
    {
        int x = 77;
        value64 v = value64_createptr(&x);
        test_validate(v.pval == &x,
                      "Create pointer: got %p, expected %p", v.pval, (void*)&x);
    }

    /* 5. C-string (копирование) */
    test_sub("subtest %d: create str", ++subnum);
    {
        const char *text = "hello value64";
        value64 v = value64_createstr(text);

        test_validatefree(
            strcmp(v.sval, text) == 0,
            value64free(v, VALUE64_STR),
            "Create str: got '%s', expected '%s'", v.sval, text
        );
        test_validatefree(
            v.sval != text,
            value64free(v, VALUE64_STR),
            "Create str must have its own memory"
        );
        value64free(v, VALUE64_STR);
    }

    /* 6. fs (копирование) */
    test_sub("subtest %d: create fs", ++subnum);
    {
        const char *text = "hello fs";
        fs orig = fscopy(text);
        value64 v = value64_createfs(&orig);

        test_validatefree(
            strcmp(fs_str(v.fsval), text) == 0,
            (fsfree(orig), value64free(v, VALUE64_FS)),
            "Create fs: got '%s', expected '%s'", fs_str(v.fsval), text
        );
        test_validatefree(
            fs_bodyalloc(v.fsval),
            (fsfree(orig), value64free(v, VALUE64_FS)),
            "Create fs must have FS_FLAG_BODYALLOC"
        );

        fsfree(orig);
        value64free(v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ---------- value64_clone ---------- */

    /* 7. clone int */
    test_sub("subtest %d: clone int", ++subnum);
    {
        value64 orig = value64_createint(100);
        value64 copy = value64_clone(orig, VALUE64_INT);
        test_validate(copy.ival == 100, "Clone int: got %d, expected 100", copy.ival);
    }

    /* 8. clone long */
    test_sub("subtest %d: clone long", ++subnum);
    {
        value64 orig = value64_createlong(999999999L);
        value64 copy = value64_clone(orig, VALUE64_LONG);
        test_validate(copy.lval == 999999999L, "Clone long: got %ld, expected 999999999", copy.lval);
    }
    /* clone ulong */
    test_sub("subtest %d: clone ulong", ++subnum);
    {
        value64 orig = value64_createulong(999999999UL);
        value64 copy = value64_clone(orig, VALUE64_ULONG);
        test_validate(copy.lval == 999999999UL, "Clone long: got %lu, expected 999999999", copy.ulval);
    }

    /* 9. clone double */
    test_sub("subtest %d: clone double", ++subnum);
    {
        value64 orig = value64_createdbl(1.6180339);
        value64 copy = value64_clone(orig, VALUE64_DBL);
        test_validate(fabs(copy.dval - 1.6180339) < 0.0000001,
                      "Clone double: got %f, expected 1.6180339", copy.dval);
    }
    /* clone char */
    test_sub("subtest %d: clone char", ++subnum);
    {
        value64 orig = value64_createchar('Z');
        value64 copy = value64_clone(orig, VALUE64_CHR);
        test_validate(copy.cval == 'Z',
                    "Clone char: got '%c', expected 'Z'", copy.cval);
    }

    /* clone bool */
    test_sub("subtest %d: clone bool", ++subnum);
    {
        value64 orig = value64_createbool(true);
        value64 copy = value64_clone(orig, VALUE64_BOOL);
        test_validate(copy.bval == true,
                    "Clone bool: got '%s', expected 'true'", bool_str(copy.bval) );
    }
    test_sub("subtest %d: clone bool false", ++subnum);
    {
        value64 orig = value64_createbool(false);
        value64 copy = value64_clone(orig, VALUE64_BOOL);
        test_validate(copy.bval == false,
                    "Clone bool: got '%s', expected 'false'", bool_str(copy.bval) );
    }

    /* 10. clone pointer */
    test_sub("subtest %d: clone pointer", ++subnum);
    {
        int x = 123;
        value64 orig = value64_createptr(&x);
        value64 copy = value64_clone(orig, VALUE64_PTR);
        test_validate(copy.pval == &x,
                      "Clone pointer: got %p, expected %p", copy.pval, (void*)&x);
    }

    /* 11. clone C-string */
    test_sub("subtest %d: clone str", ++subnum);
    {
        const char *text = "clone-string";
        value64 orig = value64_createstr(text);
        value64 copy = value64_clone(orig, VALUE64_STR);

        test_validatefree(
            strcmp(copy.sval, text) == 0,
            (value64free(orig, VALUE64_STR), value64free(copy, VALUE64_STR)),
            "Clone str: got '%s', expected '%s'", copy.sval, text
        );
        test_validatefree(
            copy.sval != orig.sval,
            (value64free(orig, VALUE64_STR), value64free(copy, VALUE64_STR)),
            "Clone str must have different address"
        );

        value64free(orig, VALUE64_STR);
        value64free(copy, VALUE64_STR);
    }

    /* 12. clone fs */
    test_sub("subtest %d: clone fs", ++subnum);
    {
        const char *text = "clone-fs";
        fs orig_fs = fscopy(text);
        value64 orig = value64_createfs(&orig_fs);
        value64 copy = value64_clone(orig, VALUE64_FS);

        test_validatefree(
            strcmp(fs_str(copy.fsval), text) == 0,
            (fsfree(orig_fs), value64free(orig, VALUE64_FS), value64free(copy, VALUE64_FS)),
            "Clone fs: got '%s', expected '%s'", fs_str(copy.fsval), text
        );
        test_validatefree(
            fs_bodyalloc(copy.fsval),
            (fsfree(orig_fs), value64free(orig, VALUE64_FS), value64free(copy, VALUE64_FS)),
            "Clone fs must have FS_FLAG_BODYALLOC"
        );
        test_validatefree(
            copy.fsval != orig.fsval,
            (fsfree(orig_fs), value64free(orig, VALUE64_FS), value64free(copy, VALUE64_FS)),
            "Clone fs must have different pointer"
        );

        fsfree(orig_fs);
        value64free(orig, VALUE64_FS);
        value64free(copy, VALUE64_FS);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_move ---------------------------------

static TestStatus
tf_move(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. move int */
    test_sub("subtest %d: move int", ++subnum);
    {
        value64 src = value64_createint(42);
        value64 dst = LITERAL64_ZERO;
        value64 *ret = value64_moveto_int(&dst, &src);

        test_validate(ret == &dst, "move_int must return &dst");
        test_validate(value64_int(dst) == 42, "dst must be 42, got %d", value64_int(dst));
        test_validate(value64_int(src) == 0, "src must be 0 after move, got %d", value64_int(src));
    }

    /* 2. move long */
    test_sub("subtest %d: move long", ++subnum);
    {
        value64 src = value64_createlong(123456789L);
        value64 dst = LITERAL64_ZERO;
        value64_moveto_long(&dst, &src);

        test_validate(value64_long(dst) == 123456789L, "dst mismatch, got %ld", value64_long(dst));
        test_validate(value64_long(src) == 0L, "src must be 0 after move, got %ld", value64_long(src));
    }
    /* 2. move ulong */
    test_sub("subtest %d: move ulong", ++subnum);
    {
        value64 src = value64_createulong(123456789UL);
        value64 dst = LITERAL64_ZERO;
        value64_moveto_long(&dst, &src);

        test_validate(value64_long(dst) == 123456789UL, "dst mismatch, got %lu", value64_long(dst));
        test_validate(value64_long(src) == 0UL, "src must be 0 after move, got %ld", value64_long(src));
    }

    /* 3. move double */
    test_sub("subtest %d: move double", ++subnum);
    {
        value64 src = value64_createdbl(3.1415);
        value64 dst = LITERAL64_ZERO;
        value64_moveto_dbl(&dst, &src);

        test_validate(fabs(value64_dbl(dst) - 3.1415) < 0.0001, "dst mismatch, got %f", value64_dbl(dst));
        test_validate(fabs(value64_dbl(src) - 0.0) < 1e-12, "src must be 0.0 after move, got %f", value64_dbl(src));
    }

    /* move char (value64_moveto_chr) */
    test_sub("subtest %d: move char", ++subnum);
    {
        value64 src = value64_createchar('A');
        value64 dst = LITERAL64_ZERO;
        value64 *ret = value64_moveto_chr(&dst, &src);

        test_validate(ret == &dst, "move_char must return &dst");
        test_validate(value64_char(dst) == 'A', "dst must be 'A', got '%c'", value64_char(dst));
        test_validate(value64_char(src) == '\0', "src must be '\\0' after move, got '%c'", value64_char(src));
    }

    /* move bool */
    test_sub("subtest %d: move char", ++subnum);
    {
        value64 src = value64_createbool(true);
        value64 dst = LITERAL64_ZERO;
        value64 *ret = value64_moveto_bool(&dst, &src);

        test_validate(ret == &dst, "move_char must return &dst");
        test_validate(value64_bool(dst) == true, "dst must be 'true', got '%s'", bool_str(value64_bool(dst)) );
        test_validate(value64_bool(src) == false, "src must be 'false' after move, got '%s'", bool_str(value64_bool(src)) );
    }

    /* 4. move pointer */
    test_sub("subtest %d: move pointer", ++subnum);
    {
        int x = 7;
        value64 src = value64_createptr(&x);
        value64 dst = LITERAL64_ZERO;
        value64_moveto_ptr(&dst, &src);

        test_validate(value64_ptr(dst) == &x, "dst must point to x, got %p", value64_ptr(dst));
        test_validate(value64_ptr(src) == NULL, "src must be NULL after move, got %p", value64_ptr(src));
    }

    /* 5. move str */
    test_sub("subtest %d: move str", ++subnum);
    {
        const char *text = "movable string";
        value64 src = value64_createstr(text);
        value64 dst = LITERAL64_ZERO;
        value64_moveto_str(&dst, &src);

        test_validatefree(
            value64_str(dst) != NULL && strcmp(value64_str(dst), text) == 0,
            value64free(dst, VALUE64_STR),
            "dst must be '%s', got '%s'", text, value64_str(dst)
        );
        test_validatefree(
            value64_str(src) == NULL,
            value64free(dst, VALUE64_STR),
            "src must be NULL after move, got %p", value64_str(src)
        );
        value64free(dst, VALUE64_STR);
    }

    /* 6. move fs */
    test_sub("subtest %d: move fs", ++subnum);
    {
        const char *text = "fs-move-target";
        fs orig = fscopy(text);
        value64 src = value64_createfs(&orig);
        fsfree(orig);

        fs *src_fs_before = value64_fs(src);   // запоминаем указатель до move

        value64 dst = LITERAL64_ZERO;
        value64_moveto_fs(&dst, &src);

        fs *dst_fs = value64_fs(dst);

        test_validatefree(
            dst_fs != NULL && strcmp(fs_str(dst_fs), text) == 0,
            value64free(dst, VALUE64_FS),
            "dst must be '%s', got '%s'", text, fs_str(dst_fs)
        );
        test_validatefree(
            fs_bodyalloc(dst_fs),
            value64free(dst, VALUE64_FS),
            "dst fs must have FS_FLAG_BODYALLOC"
        );
       // Убеждаемся, что dst получил новую память (не ту, что была у src)
        test_validatefree(
            dst_fs != src_fs_before,
            value64free(dst, VALUE64_FS),
            "dst fs pointer must differ from original src pointer"
        );
        value64free(dst, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 7. multiple fs moves (leak check) */
    test_sub("subtest %d: multiple fs moves (leak check)", ++subnum);
    {
        const char *words[] = {"first", "second", "third"};
        value64 dst[3] = { LITERAL64_ZERO, LITERAL64_ZERO, LITERAL64_ZERO };

        for (int i = 0; i < COUNT(words); i++) {
            fs orig = fscopy(words[i]);
            value64 src = value64_createfs(&orig);
            fsfree(orig);
            value64_moveto_fs(&dst[i], &src);
        }

        for (int i = 0; i < COUNT(words); i++) {
            fs *dst_fs = value64_fs(dst[i]);
            test_validatefree(
                strcmp(fs_str(dst_fs), words[i]) == 0,
                (value64free(dst[0], VALUE64_FS), value64free(dst[1], VALUE64_FS), value64free(dst[2], VALUE64_FS)),
                "dst[%d] must be '%s', got '%s'", i, words[i], fs_str(dst_fs)
            );
        }
        for (int i = 0; i < COUNT(words); i++) {
            value64free(dst[i], VALUE64_FS);
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_lhash ---------------------------------

static TestStatus
tf_lhash(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. int */
    test_sub("subtest %d: hash int", ++subnum);
    {
        value64 v1 = value64_createint(42);
        value64 v2 = value64_createint(42);
        value64 v3 = value64_createint(100);

        unsigned long h1 = value64_lhash(v1, VALUE64_INT);
        unsigned long h2 = value64_lhash(v2, VALUE64_INT);
        unsigned long h3 = value64_lhash(v3, VALUE64_INT);

        test_validate(h1 == h2, "Same ints must have same hash: %lu vs %lu", h1, h2);
        test_validate(h1 != h3, "Different ints should differ: %lu vs %lu", h1, h3);
    }

    /* 2. long */
    test_sub("subtest %d: hash long", ++subnum);
    {
        value64 v1 = value64_createlong(999999999L);
        value64 v2 = value64_createlong(999999999L);
        value64 v3 = value64_createlong(0L);

        unsigned long h1 = value64_lhash(v1, VALUE64_LONG);
        unsigned long h2 = value64_lhash(v2, VALUE64_LONG);
        unsigned long h3 = value64_lhash(v3, VALUE64_LONG);

        test_validate(h1 == h2, "Same longs must have same hash");
        test_validate(h1 != h3, "Different longs should differ");
    }
    /* ulong */
    test_sub("subtest %d: hash ulong", ++subnum);
    {
        value64 v1 = value64_createulong(999999999UL);
        value64 v2 = value64_createlong(999999999UL);
        value64 v3 = value64_createulong(10UL);

        unsigned long h1 = value64_lhash(v1, VALUE64_ULONG);
        unsigned long h2 = value64_lhash(v2, VALUE64_ULONG);
        unsigned long h3 = value64_lhash(v3, VALUE64_ULONG);

        test_validate(h1 == h2, "Same longs must have same hash");
        test_validate(h1 != h3, "Different longs should differ");
    }

    /* char */
    test_sub("subtest %d: hash char", ++subnum);
    {
        value64 v1 = value64_createchar('A');
        value64 v2 = value64_createchar('A');
        value64 v3 = value64_createchar('Z');

        unsigned long h1 = value64_lhash(v1, VALUE64_CHR);
        unsigned long h2 = value64_lhash(v2, VALUE64_CHR);
        unsigned long h3 = value64_lhash(v3, VALUE64_CHR);

        test_validate(h1 == h2, "Same chars must have same hash: %lu vs %lu", h1, h2);
        test_validate(h1 != h3, "Different chars should differ: %lu vs %lu", h1, h3);
    }

    /* bool */
    test_sub("subtest %d: hash bool", ++subnum);
    {
        value64 v1 = value64_createbool(true);
        value64 v2 = value64_createbool(true);
        value64 v3 = value64_createbool(false);

        unsigned long h1 = value64_lhash(v1, VALUE64_BOOL);
        unsigned long h2 = value64_lhash(v2, VALUE64_BOOL);
        unsigned long h3 = value64_lhash(v3, VALUE64_BOOL);

        test_validate(h1 == h2, "Same chars must have same hash: %lu vs %lu", h1, h2);
        test_validate(h1 != h3, "Different chars should differ: %lu vs %lu", h1, h3);
    }

    /* 3. double */
    test_sub("subtest %d: hash double", ++subnum);
    {
        value64 v1 = value64_createdbl(3.1415);
        value64 v2 = value64_createdbl(3.1415);
        value64 v3 = value64_createdbl(2.718);

        unsigned long h1 = value64_lhash(v1, VALUE64_DBL);
        unsigned long h2 = value64_lhash(v2, VALUE64_DBL);
        unsigned long h3 = value64_lhash(v3, VALUE64_DBL);

        test_validate(h1 == h2, "Same doubles must have same hash");
        test_validate(h1 != h3, "Different doubles should differ");
    }

    /* 4. pointer */
    test_sub("subtest %d: hash pointer", ++subnum);
    {
        int x = 1, y = 2;
        value64 v1 = value64_createptr(&x);
        value64 v2 = value64_createptr(&x);
        value64 v3 = value64_createptr(&y);

        unsigned long h1 = value64_lhash(v1, VALUE64_PTR);
        unsigned long h2 = value64_lhash(v2, VALUE64_PTR);
        unsigned long h3 = value64_lhash(v3, VALUE64_PTR);

        test_validate(h1 == h2, "Same pointers must have same hash");
        test_validate(h1 != h3, "Different pointers should differ");
    }

    /* 5. C-string */
    test_sub("subtest %d: hash str", ++subnum);
    {
        const char *text = "hash-me";
        value64 v1 = value64_createstr(text);
        value64 v2 = value64_createstr(text);
        value64 v3 = value64_createstr("other");

        unsigned long h1 = value64_lhash(v1, VALUE64_STR);
        unsigned long h2 = value64_lhash(v2, VALUE64_STR);
        unsigned long h3 = value64_lhash(v3, VALUE64_STR);

        test_validate(h1 == h2, "Same strings must have same hash");
        test_validate(h1 != h3, "Different strings should differ");

        value64free(v1, VALUE64_STR);
        value64free(v2, VALUE64_STR);
        value64free(v3, VALUE64_STR);
    }

    /* 6. fs (с ненулевым содержимым) */
    test_sub("subtest %d: hash fs", ++subnum);
    {
        const char *text = "fs-hash";
        fs orig = fscopy(text);
        value64 v1 = value64_createfs(&orig);
        value64 v2 = value64_createfs(&orig);   // ещё одна копия
        fsfree(orig);

        value64 v3 = value64_createfs(&(fs){ .v = "different", .len = 9, .sz = 0, .flags = FS_FLAG_STATIC });

        unsigned long h1 = value64_lhash(v1, VALUE64_FS);
        unsigned long h2 = value64_lhash(v2, VALUE64_FS);
        unsigned long h3 = value64_lhash(v3, VALUE64_FS);

        test_validate(h1 == h2, "Same fs must have same hash");
        test_validate(h1 != h3, "Different fs should differ");

        value64free(v1, VALUE64_FS);
        value64free(v2, VALUE64_FS);
        value64free(v3, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 7. fs с NULL указателем (пустая строка) */
    test_sub("subtest %d: hash fs with NULL body", ++subnum);
    {
        fs empty = FSLITERAL("");                    // v == NULL
        value64 v = value64_createfs(&empty);

        unsigned long h = value64_lhash(v, VALUE64_FS);
        // Главное, чтобы не упало – hash_djb2 должен обработать NULL
        test_validate(h == 5381, "Hash of empty fs (v=NULL) must be 5381, got %lu", h);

        value64free(v, VALUE64_FS);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_compare ---------------------------------

static TestStatus
tf_compare(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. compare int */
    test_sub("subtest %d: compare int", ++subnum);
    {
        value64 v1 = value64_createint(42);
        value64 v2 = value64_createint(42);
        value64 v3 = value64_createint(100);

        test_validate(value64_compare(v1, v2, VALUE64_INT) == 0, "Equal ints must return 0");
        test_validate(value64_compare(v1, v3, VALUE64_INT) != 0, "Different ints must not return 0");
    }

    /* 2. compare long */
    test_sub("subtest %d: compare long", ++subnum);
    {
        value64 v1 = value64_createlong(999999999L);
        value64 v2 = value64_createlong(999999999L);
        value64 v3 = value64_createlong(0L);

        test_validate(value64_compare(v1, v2, VALUE64_LONG) == 0, "Equal longs must return 0");
        test_validate(value64_compare(v1, v3, VALUE64_LONG) != 0, "Different longs must not return 0");
    }
    /* 2. compare ulong */
    test_sub("subtest %d: compare ulong", ++subnum);
    {
        value64 v1 = value64_createulong(999999999UL);
        value64 v2 = value64_createulong(999999999UL);
        value64 v3 = value64_createulong(70L);

        test_validate(value64_compare(v1, v2, VALUE64_ULONG) == 0, "Equal longs must return 0");
        test_validate(value64_compare(v1, v3, VALUE64_ULONG) != 0, "Different longs must not return 0");
    }

    /* compare char */
    test_sub("subtest %d: compare char", ++subnum);
    {
        value64 v1 = value64_createchar('A');
        value64 v2 = value64_createchar('A');
        value64 v3 = value64_createchar('C');

        test_validate(value64_compare(v1, v2, VALUE64_CHR) == 0, "Equal chars must return 0");
        test_validate(value64_compare(v1, v3, VALUE64_CHR) != 0, "Different chars must not return 0");
    }

    /* 1. Равные значения */
    test_sub("subtest %d: compare equal BOOL", ++subnum);
    {
        value64 a = value64_createbool(true);
        value64 b = value64_createbool(true);
        test_validate(value64_compare(a, b, VALUE64_BOOL) == 0,
                      "true == true must return 0");
    }

    /* 2. Разные значения (true vs false) */
    test_sub("subtest %d: compare true vs false", ++subnum);
    {
        value64 a = value64_createbool(true);
        value64 b = value64_createbool(false);
        test_validate(value64_compare(a, b, VALUE64_BOOL) > 0,
                      "true > false must return >0");
        test_validate(value64_compare(b, a, VALUE64_BOOL) < 0,
                      "false < true must return <0");
    }

    /* 3. compare double */
    test_sub("subtest %d: compare double", ++subnum);
    {
        value64 v1 = value64_createdbl(3.1415);
        value64 v2 = value64_createdbl(3.1415);
        value64 v3 = value64_createdbl(2.718);

        test_validate(value64_compare(v1, v2, VALUE64_DBL) == 0, "Equal doubles must return 0");
        test_validate(value64_compare(v1, v3, VALUE64_DBL) != 0, "Different doubles must not return 0");
    }

    /* 4. compare pointer */
    test_sub("subtest %d: compare pointer", ++subnum);
    {
        int x = 1, y = 2;
        value64 v1 = value64_createptr(&x);
        value64 v2 = value64_createptr(&x);
        value64 v3 = value64_createptr(&y);

        test_validate(value64_compare(v1, v2, VALUE64_PTR) == 0, "Same pointers must return 0");
        test_validate(value64_compare(v1, v3, VALUE64_PTR) != 0, "Different pointers must not return 0");
    }

    /* 5. compare C-string (returns bool) */
    test_sub("subtest %d: compare str", ++subnum);
    {
        const char *text = "compare-me";
        value64 v1 = value64_createstr(text);
        value64 v2 = value64_createstr(text);
        value64 v3 = value64_createstr("other");

        // value64_compare для строк возвращает true/false
        test_validate(value64_compare(v1, v2, VALUE64_STR) == 0, "Equal strings must return true");
        test_validate(value64_compare(v1, v3, VALUE64_STR) != 0, "Different strings must return false");

        value64free(v1, VALUE64_STR);
        value64free(v2, VALUE64_STR);
        value64free(v3, VALUE64_STR);
    }

    /* 6. compare fs */
    test_sub("subtest %d: compare fs", ++subnum);
    {
        const char *text = "fs-cmp";
        fs orig = fscopy(text);
        value64 v1 = value64_createfs(&orig);
        value64 v2 = value64_createfs(&orig);
        fsfree(orig);

        value64 v3 = value64_createfs( &FSLITERAL("different") );

        test_validate(value64_compare(v1, v2, VALUE64_FS) == 0, "Equal fs must return 0");
        test_validate(value64_compare(v1, v3, VALUE64_FS) != 0, "Different fs must not return 0");

        value64free(v1, VALUE64_FS);
        value64free(v2, VALUE64_FS);
        value64free(v3, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 7. compare fs with empty (0-len string "") */
    test_sub("subtest %d: compare fs empty", ++subnum);
    {
        fs empty1 = FSLITERAL("");
        fs empty2 = FSLITERAL("");

        value64 v1 = value64_createfs(&empty1);
        value64 v2 = value64_createfs(&empty2);

        test_validate(value64_compare(v1, v2, VALUE64_FS) == 0, "Empty fs must be equal");

        value64free(v1, VALUE64_FS);
        value64free(v2, VALUE64_FS);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_convert ---------------------------------

static TestStatus
tf_convert(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== INT → INT ========== */
    test_sub("subtest %d: INT -> INT", ++subnum);
    {
        value64 src = value64_createint(42);
        value64 dst = value64_convert(src, VALUE64_INT, VALUE64_INT);
        test_validate(value64_int(dst) == 42,
            "INT->INT: expected 42, got %d", value64_int(dst));
    }

    /* ========== INT → LONG ========== */
    test_sub("subtest %d: INT -> LONG", ++subnum);
    {
        value64 src = value64_createint(42);
        value64 dst = value64_convert(src, VALUE64_INT, VALUE64_LONG);
        test_validate(value64_long(dst) == 42L,
            "INT->LONG: expected 42, got %ld", value64_long(dst));
    }

    /* ========== INT → ULONG ========== */
    test_sub("subtest %d: INT -> ULONG (positive)", ++subnum);
    {
        value64 src = value64_createint(42);
        value64 dst = value64_convert(src, VALUE64_INT, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 42UL,
            "INT->ULONG: expected 42, got %lu", value64_ulong(dst));
    }

    test_sub("subtest %d: INT -> ULONG (negative)", ++subnum);
    {
        value64 src = value64_createint(-5);
        if (!try()) {
            value64_convert(src, VALUE64_INT, VALUE64_ULONG);
            test_validate(false, "INT->ULONG negative must raise error");
        } else {
            test_validate(true, "INT->ULONG negative correctly raised error");
        }
    }

    /* ========== 2. INT → DBL ========== */
    test_sub("subtest %d: INT -> DBL", ++subnum);
    {
        value64 src = value64_createint(42);
        value64 dst = value64_convert(src, VALUE64_INT, VALUE64_DBL);
        test_validate(fabs(value64_dbl(dst) - 42.0) < 0.0001,
            "INT->DBL: expected 42.0, got %f", value64_dbl(dst));
    }

    /* int -> char (valid) */
    test_sub("subtest %d: int -> char", ++subnum);
    {
        value64 src = value64_createint(42);
        value64 dst = value64_convert_int_to_char(src);
        test_validate(value64_char(dst) == 42,
                      "int->char: expected 42, got %d", value64_char(dst));
    }

    /* int -> char (overflow) – must raise error */
    test_sub("subtest %d: int -> char overflow", ++subnum);
    {
        value64 src = value64_createint(9999);
        if (!try()) {
            value64 dst = value64_convert_int_to_char(src);
            test_validate(false, "Should have raised SIGINT for overflow");
            (void)dst;
        } else {
            logsimple("Exception correctly raised on int->char overflow");
        }
    }

    test_sub("subtest %d: INT(0) -> BOOL", ++subnum);
    {
        value64 src = value64_createint(0);
        value64 dst = value64_convert_int_to_bool(src);
        test_validate(value64_bool(dst) == false,
                      "int 0 -> bool: expected false");
    }

    test_sub("subtest %d: INT(1) -> BOOL", ++subnum);
    {
        value64 src = value64_createint(1);
        value64 dst = value64_convert_int_to_bool(src);
        test_validate(value64_bool(dst) == true,
                      "int 1 -> bool: expected true");
    }

    test_sub("subtest %d: INT(999) -> BOOL", ++subnum);
    {
        value64 src = value64_createint(999);
        value64 dst = value64_convert_int_to_bool(src);
        test_validate(value64_bool(dst) == true,
                      "int 999 -> bool: expected true (non‑zero)");
    }

    /* ========== 3. INT → FS ========== */
    test_sub("subtest %d: INT -> FS", ++subnum);
    {
        value64 src = value64_createint(42);
        value64 dst = value64_convert(src, VALUE64_INT, VALUE64_FS);
        fs *dst_fs = value64_fs(dst);
        test_validatefree(
            strcmp(fs_str(dst_fs), "42") == 0,
            value64free(dst, VALUE64_FS),
            "INT->FS: expected '42', got '%s'", fs_str(dst_fs)
        );
        value64free(dst, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ========== 4. INT → STR ========== */
    test_sub("subtest %d: INT -> STR", ++subnum);
    {
        value64 src = value64_createint(42);
        value64 dst = value64_convert(src, VALUE64_INT, VALUE64_STR);
        test_validatefree(
            strcmp(value64_str(dst), "42") == 0,
            value64free(dst, VALUE64_STR),
            "INT->STR: expected '42', got '%s'", value64_str(dst)
        );
        value64free(dst, VALUE64_STR);
    }

    /* ========== 5. LONG → INT (в пределах) ========== */
    test_sub("subtest %d: LONG -> INT (in range)", ++subnum);
    {
        value64 src = value64_createlong(123456L);
        value64 dst = value64_convert(src, VALUE64_LONG, VALUE64_INT);
        test_validate(value64_int(dst) == 123456,
            "LONG->INT: expected 123456, got %d", value64_int(dst));
    }

    /* ========== 6. LONG → INT (переполнение) ========== */
    test_sub("subtest %d: LONG -> INT (overflow)", ++subnum);
    {
        value64 src = value64_createlong(2147483648L);  // > INT_MAX
        if (!try()) {
            value64 dst = value64_convert(src, VALUE64_LONG, VALUE64_INT);
            test_validate(false, "LONG->INT overflow must raise error, but returned %d", value64_int(dst));
        } else {
            test_validate(true, "LONG->INT overflow correctly raised error");
        }
    }

    /* ========== LONG → LONG ========== */
    test_sub("subtest %d: LONG -> LONG (positive)", ++subnum);
    {
        value64 src = value64_createlong(123L);
        value64 dst = value64_convert(src, VALUE64_LONG, VALUE64_LONG);
        test_validate(value64_long(dst) == 123L,
            "LONG->LONG: expected 123, got %lu", value64_long(dst));
    }

    /* ========== LONG → ULONG ========== */
    test_sub("subtest %d: LONG -> ULONG (positive)", ++subnum);
    {
        value64 src = value64_createlong(123L);
        value64 dst = value64_convert(src, VALUE64_LONG, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 123UL,
            "LONG->ULONG: expected 123, got %lu", value64_ulong(dst));
    }

    test_sub("subtest %d: LONG -> ULONG (negative)", ++subnum);
    {
        value64 src = value64_createlong(-1L);
        if (!try()) {
            value64_convert(src, VALUE64_LONG, VALUE64_ULONG);
            test_validate(false, "LONG->ULONG negative must raise error");
        } else {
            test_validate(true, "LONG->ULONG negative correctly raised error");
        }
    }

    /* ========== 7. LONG → DBL ========== */
    test_sub("subtest %d: LONG -> DBL", ++subnum);
    {
        value64 src = value64_createlong(999999999L);
        value64 dst = value64_convert(src, VALUE64_LONG, VALUE64_DBL);
        test_validate(fabs(value64_dbl(dst) - 999999999.0) < 0.0001,
            "LONG->DBL: expected 999999999.0, got %f", value64_dbl(dst));
    }

    test_sub("subtest %d: LNG(0L) -> BOOL", ++subnum);
    {
        value64 src = value64_createlong(0L);
        value64 dst = value64_convert_lng_to_bool(src);
        test_validate(value64_bool(dst) == false,
                      "long 0 -> bool: expected false");
    }

    test_sub("subtest %d: LNG(1000L) -> BOOL", ++subnum);
    {
        value64 src = value64_createlong(1000L);
        value64 dst = value64_convert_lng_to_bool(src);
        test_validate(value64_bool(dst) == true,
                      "long 1000 -> bool: expected true (non‑zero)");
    }

    /* long -> char (valid) */
    test_sub("subtest %d: long -> char", ++subnum);
    {
        value64 src = value64_createlong(100L);
        value64 dst = value64_convert_lng_to_char(src);
        test_validate(value64_char(dst) == 100,
                      "long->char: expected 100, got %d", value64_char(dst));
    }

    /* long -> char (overflow) */
    test_sub("subtest %d: long -> char overflow", ++subnum);
    {
        value64 src = value64_createlong(9999L);
        if (!try()) {
            value64 dst = value64_convert_lng_to_char(src);
            test_validate(false, "Should have raised SIGINT");
            (void)dst;
        } else {
            logsimple("Exception correctly raised on long->char overflow");
        }
    }

    /* ========== 8. LONG → FS ========== */
    test_sub("subtest %d: LONG -> FS", ++subnum);
    {
        value64 src = value64_createlong(-123456789L);
        value64 dst = value64_convert(src, VALUE64_LONG, VALUE64_FS);
        fs *dst_fs = value64_fs(dst);
        test_validatefree(
            strcmp(fs_str(dst_fs), "-123456789") == 0,
            value64free(dst, VALUE64_FS),
            "LONG->FS: expected '-123456789', got '%s'", fs_str(dst_fs)
        );
        value64free(dst, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ========== 9. LONG → STR ========== */
    test_sub("subtest %d: LONG -> STR", ++subnum);
    {
        value64 src = value64_createlong(0L);
        value64 dst = value64_convert(src, VALUE64_LONG, VALUE64_STR);
        test_validatefree(
            strcmp(value64_str(dst), "0") == 0,
            value64free(dst, VALUE64_STR),
            "LONG->STR: expected '0', got '%s'", value64_str(dst)
        );
        value64free(dst, VALUE64_STR);
    }

    /* ========== ULONG → ULONG ========== */
    test_sub("subtest %d: ULONG -> ULONG", ++subnum);
    {
        value64 src = value64_createulong(100UL);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_ULONG);
        test_validate(
            value64_ulong(dst) == 100UL,
            "ULONG->ULONG: expected 100UL, got %lu", value64_ulong(dst));
    }

    /* ========== ULONG → INT ========== */
    test_sub("subtest %d: ULONG -> INT", ++subnum);
    {
        value64 src = value64_createulong(100);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_INT);
        test_validate(value64_int(dst) == 100,
            "ULONG->INT: expected 100, got %d", value64_int(dst));
    }

    test_sub("subtest %d: ULONG -> INT overflow", ++subnum);
    {
        value64 src = value64_createulong((unsigned long)INT_MAX + 1);
        if (!try()) {
            value64_convert(src, VALUE64_ULONG, VALUE64_INT);
            test_validate(false, "ULONG->INT overflow must raise error");
        } else {
            test_validate(true, "ULONG->INT overflow correctly raised error");
        }
    }

    /* ========== ULONG → LONG ========== */
    test_sub("subtest %d: ULONG -> LONG", ++subnum);
    {
        value64 src = value64_createulong(LONG_MAX);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_LONG);
        test_validate(value64_long(dst) == LONG_MAX,
            "ULONG->LONG: expected %ld, got %ld", LONG_MAX, value64_long(dst));
    }

    test_sub("subtest %d: ULONG -> LONG overflow", ++subnum);
    {
        value64 src = value64_createulong((unsigned long) LONG_MAX + 1);
        if (!try()) {
            value64_convert(src, VALUE64_ULONG, VALUE64_LONG);
            test_validate(false, "ULONG->LONG overflow must raise error");
        } else {
            test_validate(true, "ULONG->LONG overflow correctly raised error");
        }
    }

    /* ========== ULONG → LONG ========== */
    test_sub("subtest %d: ULONG -> LONG", ++subnum);
    {
        value64 src = value64_createulong(LONG_MAX);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_LONG);
        test_validate(value64_long(dst) == LONG_MAX,
            "ULONG->LONG: expected %ld, got %ld", LONG_MAX, value64_long(dst));
    }

    test_sub("subtest %d: ULONG -> LONG overflow", ++subnum);
    {
        value64 src = value64_createulong((unsigned long)LONG_MAX + 1);
        if (!try()) {
            value64_convert(src, VALUE64_ULONG, VALUE64_LONG);
            test_validate(false, "ULONG->LONG overflow must raise error");
        } else {
            test_validate(true, "ULONG->LONG overflow correctly raised error");
        }
    }

    /* ========== ULONG → DBL ========== */
    test_sub("subtest %d: ULONG -> DBL", ++subnum);
    {
        value64 src = value64_createulong(12345);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_DBL);
        test_validate(value64_dbl(dst) == 12345.0,
            "ULONG->DBL: expected 12345.0, got %f", value64_dbl(dst));
    }

    /* ========== ULONG → CHR ========== */
    test_sub("subtest %d: ULONG -> CHR (valid)", ++subnum);
    {
        value64 src = value64_createulong('A');
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_CHR);
        test_validate(value64_char(dst) == 'A',
            "ULONG->CHR: expected 'A', got '%c'", value64_char(dst));
    }

    test_sub("subtest %d: ULONG -> CHR (overflow)", ++subnum);
    {
        value64 src = value64_createulong(256);
        if (!try()) {
            value64_convert(src, VALUE64_ULONG, VALUE64_CHR);
            test_validate(false, "ULONG->CHR overflow must raise error");
        } else {
            test_validate(true, "ULONG->CHR overflow correctly raised error");
        }
    }

    /* ========== ULONG → BOOL ========== */
    test_sub("subtest %d: ULONG -> BOOL (zero)", ++subnum);
    {
        value64 src = value64_createulong(0);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_BOOL);
        test_validate(value64_bool(dst) == false,
            "ULONG(0)->BOOL: expected false, got %s", value64_bool(dst) ? "true" : "false");
    }

    test_sub("subtest %d: ULONG -> BOOL (nonzero)", ++subnum);
    {
        value64 src = value64_createulong(42);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_BOOL);
        test_validate(value64_bool(dst) == true,
            "ULONG(42)->BOOL: expected true, got %s", value64_bool(dst) ? "true" : "false");
    }

    /* ========== ULONG → STR ========== */
    test_sub("subtest %d: ULONG -> STR", ++subnum);
    {
        value64 src = value64_createulong(9876543210UL);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_STR);
        test_validatefree(
            strcmp(value64_str(dst), "9876543210") == 0,
            value64free(dst, VALUE64_STR),
            "ULONG->STR: expected '9876543210', got '%s'", value64_str(dst)
        );
        value64free(dst, VALUE64_STR);
    }

    /* ========== ULONG → FS ========== */
    test_sub("subtest %d: ULONG -> FS", ++subnum);
    {
        value64 src = value64_createulong(1234567890);
        value64 dst = value64_convert(src, VALUE64_ULONG, VALUE64_FS);
        test_validatefree(
            strcmp(fs_str(value64_fs(dst)), "1234567890") == 0,
            value64free(dst, VALUE64_FS),
            "ULONG->FS: expected '1234567890', got '%s'", fs_str(value64_fs(dst))
        );
        value64free(dst, VALUE64_FS);
    }

    /* ========================================================================
     * CHAR → other types
     * ======================================================================== */

    /* char -> int */
    test_sub("subtest %d: char -> int", ++subnum);
    {
        value64 src = value64_createchar('A');
        value64 dst = value64_convert_char_to_int(src);
        test_validate(value64_int(dst) == 'A',
                      "char->int: expected %d, got %d", 'A', value64_int(dst));
    }

    /* char -> long */
    test_sub("subtest %d: char -> long", ++subnum);
    {
        value64 src = value64_createchar('Z');
        value64 dst = value64_convert_char_to_lng(src);
        test_validate(value64_long(dst) == 'Z',
                      "char->long: expected %ld, got %ld", (long)'Z', value64_long(dst));
    }

    /* ========== CHR → ULONG ========== */
    test_sub("subtest %d: CHR -> ULONG", ++subnum);
    {
        value64 src = value64_createchar('Z');
        value64 dst = value64_convert(src, VALUE64_CHR, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == (unsigned char)'Z',
            "CHR->ULONG: expected %u, got %lu", (unsigned char)'Z', value64_ulong(dst));
    }

    test_sub("subtest %d: char -> dbl not convertible", ++subnum);
    {
        value64 src = value64_createchar('A');
        test_validate(!value64_is_convertable(src, VALUE64_CHR, VALUE64_DBL),
                      "char->dbl must not be convertible");
    }

    /* char -> fs */
    test_sub("subtest %d: char -> fs", ++subnum);
    {
        value64 src = value64_createchar('X');
        value64 dst = value64_convert_char_to_fs(src);
        test_validatefree(
            value64_fs(dst) != NULL && strcmp(fs_str(value64_fs(dst)), "X") == 0,
            value64_freefs(&dst),
            "char->fs: expected 'X', got '%s'",
            value64_fs(dst) ? fs_str(value64_fs(dst)) : "NULL"
        );
        value64_freefs(&dst);
    }
    fs_alloc_check(true);

    /* char -> str */
    test_sub("subtest %d: char -> str", ++subnum);
    {
        value64 src = value64_createchar('Y');
        value64 dst = value64_convert_char_to_str(src);
        test_validatefree(
            value64_str(dst) != NULL && strcmp(value64_str(dst), "Y") == 0,
            value64_freestr(&dst),
            "char->str: expected 'Y', got '%s'",
            value64_str(dst) ? value64_str(dst) : "NULL"
        );
        value64_freestr(&dst);
    }

    /* char -> char (identity) */
    test_sub("subtest %d: char -> char (identity)", ++subnum);
    {
        value64 src = value64_createchar('M');
        value64 dst = value64_convert_char_to_char(src);
        test_validate(value64_char(dst) == 'M',
                      "char->char: expected 'M', got '%c'", value64_char(dst));
    }

    /* ========================================================================
     * BOOL → другие типы (прямые вызовы)
     * ======================================================================== */

    test_sub("subtest %d: BOOL -> INT (true)", ++subnum);
    {
        value64 src = value64_createbool(true);
        value64 dst = value64_convert_bool_to_int(src);
        test_validate(value64_int(dst) == 1,
                      "true -> int: expected 1, got %d", value64_int(dst));
    }

    test_sub("subtest %d: BOOL -> INT (false)", ++subnum);
    {
        value64 src = value64_createbool(false);
        value64 dst = value64_convert_bool_to_int(src);
        test_validate(value64_int(dst) == 0,
                      "false -> int: expected 0, got %d", value64_int(dst));
    }

    test_sub("subtest %d: BOOL -> LNG (true)", ++subnum);
    {
        value64 src = value64_createbool(true);
        value64 dst = value64_convert_bool_to_lng(src);
        test_validate(value64_long(dst) == 1L,
                      "true -> long: expected 1L, got %ld", value64_long(dst));
    }

    /* ========== BOOL → ULONG ========== */
    test_sub("subtest %d: BOOL -> ULONG (true)", ++subnum);
    {
        value64 src = value64_createbool(true);
        value64 dst = value64_convert(src, VALUE64_BOOL, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 1UL,
            "BOOL(true)->ULONG: expected 1, got %lu", value64_ulong(dst));
    }

    test_sub("subtest %d: BOOL -> ULONG (false)", ++subnum);
    {
        value64 src = value64_createbool(false);
        value64 dst = value64_convert(src, VALUE64_BOOL, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 0UL,
            "BOOL(false)->ULONG: expected 0, got %lu", value64_ulong(dst));
    }

    test_sub("subtest %d: BOOL -> FS (true)", ++subnum);
    {
        value64 src = value64_createbool(true);
        value64 dst = value64_convert_bool_to_fs(src);
        test_validatefree(
            value64_fs(dst) != NULL && strcmp(fs_str(value64_fs(dst)), "true") == 0,
            value64_freefs(&dst),
            "true -> fs: expected 'true', got '%s'",
            value64_fs(dst) ? fs_str(value64_fs(dst)) : "NULL"
        );
        value64_freefs(&dst);
    }

    test_sub("subtest %d: BOOL -> FS (false)", ++subnum);
    {
        value64 src = value64_createbool(false);
        value64 dst = value64_convert_bool_to_fs(src);
        test_validatefree(
            value64_fs(dst) != NULL && strcmp(fs_str(value64_fs(dst)), "false") == 0,
            value64_freefs(&dst),
            "false -> fs: expected 'false', got '%s'",
            value64_fs(dst) ? fs_str(value64_fs(dst)) : "NULL"
        );
        value64_freefs(&dst);
    }

    test_sub("subtest %d: BOOL -> STR (true)", ++subnum);
    {
        value64 src = value64_createbool(true);
        value64 dst = value64_convert_bool_to_str(src);
        test_validatefree(
            value64_str(dst) != NULL && strcmp(value64_str(dst), "true") == 0,
            value64_freestr(&dst),
            "true -> str: expected 'true', got '%s'",
            value64_str(dst) ? value64_str(dst) : "NULL"
        );
        value64_freestr(&dst);
    }

    test_sub("subtest %d: BOOL -> STR (false)", ++subnum);
    {
        value64 src = value64_createbool(false);
        value64 dst = value64_convert_bool_to_str(src);
        test_validatefree(
            value64_str(dst) != NULL && strcmp(value64_str(dst), "false") == 0,
            value64_freestr(&dst),
            "false -> str: expected 'false', got '%s'",
            value64_str(dst) ? value64_str(dst) : "NULL"
        );
        value64_freestr(&dst);
    }

    /* ========== 10. DBL → INT (в пределах) ========== */
    test_sub("subtest %d: DBL -> INT (in range)", ++subnum);
    {
        value64 src = value64_createdbl(3.14);
        value64 dst = value64_convert(src, VALUE64_DBL, VALUE64_INT);
        test_validate(value64_int(dst) == 3,
            "DBL->INT: expected 3, got %d", value64_int(dst));
    }

    /* ========== 11. DBL → INT (переполнение) ========== */
    test_sub("subtest %d: DBL -> INT (overflow)", ++subnum);
    {
        value64 src = value64_createdbl(1.0e30);
        if (!try()) {
            value64 dst = value64_convert(src, VALUE64_DBL, VALUE64_INT);
            test_validate(false, "DBL->INT overflow must raise error, but returned %d", value64_int(dst));
        } else {
            test_validate(true, "DBL->INT overflow correctly raised error");
        }
    }

    /* ========== 12. DBL → LONG (в пределах) ========== */
    test_sub("subtest %d: DBL -> LONG (in range)", ++subnum);
    {
        value64 src = value64_createdbl(2.71828);
        value64 dst = value64_convert(src, VALUE64_DBL, VALUE64_LONG);
        test_validate(value64_long(dst) == 2L,
            "DBL->LONG: expected 2, got %ld", value64_long(dst));
    }

    /* ========== 13. DBL → LONG (переполнение) ========== */
    test_sub("subtest %d: DBL -> LONG (overflow)", ++subnum);
    {
        value64 src = value64_createdbl(1.0e30);
        if (!try()) {
            value64 dst = value64_convert(src, VALUE64_DBL, VALUE64_LONG);
            test_validate(false, "DBL->LONG overflow must raise error, but returned %ld", value64_long(dst));
        } else {
            test_validate(true, "DBL->LONG overflow correctly raised error");
        }
    }

    /* ========================================================================
     * Double ↔ char must NOT be convertible
     * ======================================================================== */
    test_sub("subtest %d: dbl -> char not convertible", ++subnum);
    {
        value64 src = value64_createdbl(3.14);
        test_validate(!value64_is_convertable(src, VALUE64_DBL, VALUE64_CHR),
                      "dbl->char must not be convertible");
    }

    /* ========== DBL → ULONG ========== */
    test_sub("subtest %d: DBL -> ULONG (exact integer)", ++subnum);
    {
        value64 src = value64_createdbl(100.0);
        value64 dst = value64_convert(src, VALUE64_DBL, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 100UL,
            "DBL->ULONG: expected 100, got %lu", value64_ulong(dst));
    }

    test_sub("subtest %d: DBL -> ULONG (fractional)", ++subnum);
    {
        value64 src = value64_createdbl(3.14);
        value64 dst = value64_convert(src, VALUE64_DBL, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 3UL,
            "DBL->ULONG: expected 3 trunk(3.14), got %lu", value64_ulong(dst));
    }

    test_sub("subtest %d: DBL -> ULONG (negative)", ++subnum);
    {
        value64 src = value64_createdbl(-10.0);
        if (!try()) {
            value64_convert(src, VALUE64_DBL, VALUE64_ULONG);
            test_validate(false, "DBL->ULONG negative must raise error");
        } else {
            test_validate(true, "DBL->ULONG negative correctly raised error");
        }
    }

    /* ========== 14. DBL → FS ========== */
    test_sub("subtest %d: DBL -> FS", ++subnum);
    {
        value64 src = value64_createdbl(3.14159265);
        value64 dst = value64_convert(src, VALUE64_DBL, VALUE64_FS);
        fs *dst_fs = value64_fs(dst);
        // %g убирает лишние нули, проверяем, что строка начинается с "3.14159"
        test_validatefree(
            strncmp(fs_str(dst_fs), "3.14159", 7) == 0,
            value64free(dst, VALUE64_FS),
            "DBL->FS: expected prefix '3.14159', got '%s'", fs_str(dst_fs)
        );
        value64free(dst, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ========== 15. DBL → STR ========== */
    test_sub("subtest %d: DBL -> STR", ++subnum);
    {
        value64 src = value64_createdbl(2.5);
        value64 dst = value64_convert(src, VALUE64_DBL, VALUE64_STR);
        // snprintf с "%lf" даст "2.500000"
        test_validatefree(
            strcmp(value64_str(dst), "2.500000") == 0,
            value64free(dst, VALUE64_STR),
            "DBL->STR: expected '2.500000', got '%s'", value64_str(dst)
        );
        value64free(dst, VALUE64_STR);
    }

    /* ========== 16. FS → INT (корректная строка) ========== */
    test_sub("subtest %d: FS -> INT (valid)", ++subnum);
    {
        fs tmp = fscopy("123");
        value64 src = value64_createfs(&tmp);
        fsfree(tmp);
        value64 dst = value64_convert(src, VALUE64_FS, VALUE64_INT);
        test_validatefree(
            value64_int(dst) == 123,
            value64free(src, VALUE64_FS),
            "FS->INT: expected 123, got %d", value64_int(dst)
        );
        value64free(src, VALUE64_FS);
    }

    /* ========== 17. FS → INT (некорректная строка) ========== */
    test_sub("subtest %d: FS -> INT (invalid)", ++subnum);
    {
        fs tmp = fscopy("abc");
        value64 src = value64_createfs(&tmp);
        fsfree(tmp);
        if (!try()) {
            value64 dst = value64_convert(src, VALUE64_FS, VALUE64_INT);
            test_validatefree(false, value64free(src, VALUE64_FS),
                "FS->INT invalid must raise error, but returned %d", value64_int(dst));
        } else {
            test_validatefree(true, value64free(src, VALUE64_FS),
                "FS->INT invalid correctly raised error");
        }
        value64free(src, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ========== 18. FS → LONG ========== */
    test_sub("subtest %d: FS -> LONG", ++subnum);
    {
        fs tmp = fscopy("-999999999");
        value64 src = value64_createfs(&tmp);
        fsfree(tmp);
        value64 dst = value64_convert(src, VALUE64_FS, VALUE64_LONG);
        test_validatefree(
            value64_long(dst) == -999999999L,
            value64free(src, VALUE64_FS),
            "FS->LONG: expected -999999999, got %ld", value64_long(dst)
        );
        value64free(src, VALUE64_FS);
    }

    /* ========== FS → ULONG ========== */
    test_sub("subtest %d: FS -> ULONG (valid)", ++subnum);
    {
        fs f = fscopy("1024");
        value64 src = value64_createfs(&f);  // предполагаем, что есть такой конструктор
        value64 dst = value64_convert(src, VALUE64_FS, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 1024UL,
            "FS->ULONG: expected 1024, got %lu", value64_ulong(dst));
        value64free(src, VALUE64_FS);
        fsfree(f);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: FS -> ULONG (invalid)", ++subnum);
    {
        fs f = fscopy("not_a_number");
        value64 src = value64_createfs(&f);
        if (!try()) {
            value64_convert(src, VALUE64_FS, VALUE64_ULONG);
            test_validate(false, "FS->ULONG invalid must raise error");
        } else {
            test_validate(true, "FS->ULONG invalid correctly raised error");
        }
        value64free(src, VALUE64_FS);
        fsfree(f);
        fs_alloc_check(true);
    }

    /* ========== FS → ULONG via value64_createfs_asstr ========== */
    test_sub("subtest %d: FS -> ULONG (valid)", ++subnum);
    {
        value64 src = value64_createfs_asstr("1024");
        value64 dst = value64_convert(src, VALUE64_FS, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 1024UL,
            "FS->ULONG: expected 1024, got %lu", value64_ulong(dst));
        value64free(src, VALUE64_FS);
        fs_alloc_check(true);
    }
    /*test_sub("subtest %d: FS -> ULONG (valid 2)", ++subnum);
    {
        value64 src = value64_createfs_asstr("1024nnn");
        value64 dst = value64_convert(src, VALUE64_FS, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 1024UL,
            "FS->ULONG: expected 1024, got %lu", value64_ulong(dst));
        value64free(src, VALUE64_FS);
        fs_alloc_check(true);
    } */
    test_sub("subtest %d: FS -> ULONG (invalid)", ++subnum);
    {
        value64 src = value64_createfs_asstr("not_a_number");
        if (!try()) {
            value64_convert(src, VALUE64_FS, VALUE64_ULONG);
            test_validate(false, "FS->ULONG invalid must raise error");
        } else {
            test_validate(true, "FS->ULONG invalid correctly raised error");
        }
        value64free(src, VALUE64_FS);
        fs_alloc_check(true);
    }
    

    /* ========== 19. FS → DBL ========== */
    test_sub("subtest %d: FS -> DBL", ++subnum);
    {
        fs tmp = fscopy("3.1415");
        value64 src = value64_createfs(&tmp);
        fsfree(tmp);
        value64 dst = value64_convert(src, VALUE64_FS, VALUE64_DBL);
        test_validatefree(
            fabs(value64_dbl(dst) - 3.1415) < 0.0001,
            value64free(src, VALUE64_FS),
            "FS->DBL: expected ~3.1415, got %f", value64_dbl(dst)
        );
        value64free(src, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* fs -> char (single char string) */
    test_sub("subtest %d: fs -> char", ++subnum);
    {
        value64 src = value64_createfs_asstr("K");
        value64 dst = value64_convert_fs_to_char(src);
        test_validatefree(
            value64_char(dst) == 'K',
            value64_freefs(&src),
            "fs->char: expected 'K', got '%c'", value64_char(dst)
        );
        value64free(src, VALUE64_FS);
    }
    fs_alloc_check(true);

    /* fs -> char (empty string) – error */
    test_sub("subtest %d: fs -> char empty", ++subnum);
    {
        value64 src = value64_createfs_asstr("");
        value64 dst = value64_convert_fs_to_char(src);
        test_validatefree(
            value64_char(dst) == '\0',
            value64_freefs(&src),
            "fs->char: expected 'null-term', got '%c'", value64_char(dst)
        );
        value64free(src, VALUE64_FS);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS 'on' -> BOOL", ++subnum);
    {
        value64 src = value64_createfs_asstr("on");
        value64 dst = value64_convert_fs_to_bool(src);
        test_validatefree(
            value64_bool(dst) == true,
            (value64_freefs(&src), value64_freefs(&dst)),
            "fs 'on' -> bool: expected true"
        );
        value64free(src, VALUE64_FS);
        value64free(src, VALUE64_BOOL);
    }

    test_sub("subtest %d: FS 'OFF' -> BOOL (case insensitive)", ++subnum);
    {
        value64 src = value64_createfs_asstr("OFF");
        value64 dst = value64_convert_fs_to_bool(src);
        test_validatefree(
            value64_bool(dst) == false,
            (value64_freefs(&src), value64_freefs(&dst)),
            "fs 'OFF' -> bool: expected false"
        );
        value64free(src, VALUE64_FS);
        value64free(src, VALUE64_BOOL);
    }

    test_sub("subtest %d: FS 'unknown' -> BOOL raises", ++subnum);
    {
        value64 src = value64_createfs_asstr("unknown");
        if (!try()) {
            value64 dst = value64_convert_fs_to_bool(src);
            test_validatefree(false, value64_freefs(&src), "Should have raised SIGINT");
            (void)dst;
        } else {
            logsimple("Exception correctly raised on fs->bool invalid");
            value64free(src, VALUE64_FS);
        }
    }

    /* ========== 20. FS → FS (копирование) ========== */
    test_sub("subtest %d: FS -> FS (copy)", ++subnum);
    {
        fs tmp = fscopy("clone-me");
        value64 src = value64_createfs(&tmp);
        fsfree(tmp);
        value64 dst = value64_convert(src, VALUE64_FS, VALUE64_FS);
        fs *src_fs = value64_fs(src);
        fs *dst_fs = value64_fs(dst);
        test_validatefree(
            strcmp(fs_str(src_fs), fs_str(dst_fs)) == 0 && src_fs != dst_fs,
            (value64free(src, VALUE64_FS), value64free(dst, VALUE64_FS)),
            "FS->FS: copy must have same content but different pointer"
        );
        value64free(src, VALUE64_FS);
        value64free(dst, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ========== 21. FS → STR ========== */
    test_sub("subtest %d: FS -> STR", ++subnum);
    {
        fs tmp = fscopy("to-string");
        value64 src = value64_createfs(&tmp);
        fsfree(tmp);
        value64 dst = value64_convert(src, VALUE64_FS, VALUE64_STR);
        test_validatefree(
            strcmp(value64_str(dst), "to-string") == 0,
            (value64free(src, VALUE64_FS), value64free(dst, VALUE64_STR)),
            "FS->STR: expected 'to-string', got '%s'", value64_str(dst)
        );
        value64free(src, VALUE64_FS);
        value64free(dst, VALUE64_STR);
    }

    /* ========== 22. STR → INT (корректная) ========== */
    test_sub("subtest %d: STR -> INT (valid)", ++subnum);
    {
        value64 src = value64_createstr("42");
        value64 dst = value64_convert(src, VALUE64_STR, VALUE64_INT);
        test_validatefree(
            value64_int(dst) == 42,
            value64free(src, VALUE64_STR),
            "STR->INT: expected 42, got %d", value64_int(dst)
        );
        value64free(src, VALUE64_STR);
    }

    /* ========== 23. STR → INT (некорректная) ========== */
    test_sub("subtest %d: STR -> INT (invalid)", ++subnum);
    {
        value64 src = value64_createstr("not-a-number");
        if (!try()) {
            value64 dst = value64_convert(src, VALUE64_STR, VALUE64_INT);
            test_validatefree(false, value64free(src, VALUE64_STR),
                "STR->INT invalid must raise error, but returned %d", value64_int(dst));
        } else {
            test_validatefree(true, value64free(src, VALUE64_STR),
                "STR->INT invalid correctly raised error");
        }
        value64free(src, VALUE64_STR);
    }

    /* ========== 24. STR → LONG (корректная) ========== */
    test_sub("subtest %d: STR -> LONG (valid)", ++subnum);
    {
        value64 src = value64_createstr("-123456789");
        value64 dst = value64_convert(src, VALUE64_STR, VALUE64_LONG);
        test_validatefree(
            value64_long(dst) == -123456789L,
            value64free(src, VALUE64_STR),
            "STR->LONG: expected -123456789, got %ld", value64_long(dst)
        );
        value64free(src, VALUE64_STR);
    }

    /* str -> char (single char string) */
    test_sub("subtest %d: str -> char", ++subnum);
    {
        value64 src = value64_createstr("P");
        value64 dst = value64_convert_str_to_char(src);
        test_validatefree(
            value64_char(dst) == 'P',
            value64_freestr(&src),
            "str->char: expected 'P', got '%c'", value64_char(dst)
        );
        value64free(src, VALUE64_STR);
    }

    /* str -> char (empty) – error */
    test_sub("subtest %d: str -> char empty", ++subnum);
    {
        value64 src = value64_createstr("");
        value64 dst = value64_convert_str_to_char(src);
        test_validatefree(
            value64_char(dst) == '\0',
            value64_freestr(&src),
            "str->char: expected 'P', got '%c'", value64_char(dst)
        );
        value64free(src, VALUE64_STR);
        value64free(dst, VALUE64_CHR);
    }

    test_sub("subtest %d: STR 'true' -> BOOL", ++subnum);
    {
        value64 src = value64_createstr("true");
        value64 dst = value64_convert_str_to_bool(src);
        test_validatefree(
            value64_bool(dst) == true,
            (value64_freestr(&src), value64_freestr(&dst)),
            "str 'true' -> bool: expected true"
        );
        value64free(src, VALUE64_STR);
        value64free(dst, VALUE64_BOOL);
    }

    test_sub("subtest %d: STR 'false' -> BOOL", ++subnum);
    {
        value64 src = value64_createstr("false");
        value64 dst = value64_convert_str_to_bool(src);
        test_validatefree(
            value64_bool(dst) == false,
            (value64_freestr(&src), value64_freestr(&dst)),
            "str 'false' -> bool: expected false"
        );
        value64free(src, VALUE64_STR);
        value64free(dst, VALUE64_BOOL);
    }

    test_sub("subtest %d: STR 'yes' -> BOOL", ++subnum);
    {
        value64 src = value64_createstr("yes");
        value64 dst = value64_convert_str_to_bool(src);
        test_validatefree(
            value64_bool(dst) == true,
            (value64_freestr(&src), value64_freestr(&dst)),
            "str 'yes' -> bool: expected true"
        );
        value64free(src, VALUE64_STR);
        value64free(dst, VALUE64_BOOL);
    }

    test_sub("subtest %d: STR 'no' -> BOOL", ++subnum);
    {
        value64 src = value64_createstr("no");
        value64 dst = value64_convert_str_to_bool(src);
        test_validatefree(
            value64_bool(dst) == false,
            (value64_freestr(&src), value64_freestr(&dst)),
            "str 'no' -> bool: expected false"
        );
        value64free(src, VALUE64_STR);
        value64free(dst, VALUE64_BOOL);
    }

    test_sub("subtest %d: STR 'invalid' -> BOOL raises", ++subnum);
    {
        value64 src = value64_createstr("invalid");
        if (!try()) {
            value64 dst = value64_convert_str_to_bool(src);
            test_validatefree(false, value64_freestr(&src), "Should have raised SIGINT");
            (void) dst;
        } else {
            logsimple("Exception correctly raised on str->bool invalid");
            value64_freestr(&src);
        }
    }

    /* ========== STR → ULONG ========== */
    test_sub("subtest %d: STR -> ULONG (valid)", ++subnum);
    {
        value64 src = value64_createstr("789");
        value64 dst = value64_convert(src, VALUE64_STR, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 789UL,
            "STR->ULONG: expected 789, got %lu", value64_ulong(dst));
        value64free(src, VALUE64_STR);
    }
    /*test_sub("subtest %d: STR -> ULONG (valid 2)", ++subnum);
    {
        value64 src = value64_createstr("789xxxxxxx");
        value64 dst = value64_convert(src, VALUE64_STR, VALUE64_ULONG);
        test_validate(value64_ulong(dst) == 789UL,
            "STR->ULONG: expected 789, got %lu", value64_ulong(dst));
        value64free(src, VALUE64_STR);
    }*/

    test_sub("subtest %d: STR -> ULONG (invalid)", ++subnum);
    {
        value64 src = value64_createstr("u12abc");
        if (!try()) {
            value64_convert(src, VALUE64_STR, VALUE64_ULONG);
            test_validate(false, "STR->ULONG invalid must raise error");
        } else {
            test_validate(true, "STR->ULONG invalid correctly raised error");
        }
        value64free(src, VALUE64_STR);
    }

    /* ========== 25. STR → DBL (корректная) ========== */
    test_sub("subtest %d: STR -> DBL (valid)", ++subnum);
    {
        value64 src = value64_createstr("2.71828");
        value64 dst = value64_convert(src, VALUE64_STR, VALUE64_DBL);
        test_validatefree(
            fabs(value64_dbl(dst) - 2.71828) < 0.00001,
            value64free(src, VALUE64_STR),
            "STR->DBL: expected ~2.71828, got %f", value64_dbl(dst)
        );
        value64free(src, VALUE64_STR);
    }

    /* ========== 26. STR → STR (копирование) ========== */
    test_sub("subtest %d: STR -> STR (copy)", ++subnum);
    {
        value64 src = value64_createstr("copy-string");
        value64 dst = value64_convert(src, VALUE64_STR, VALUE64_STR);
        test_validatefree(
            strcmp(value64_str(src), value64_str(dst)) == 0 && value64_str(src) != value64_str(dst),
            (value64free(src, VALUE64_STR), value64free(dst, VALUE64_STR)),
            "STR->STR: copy must have same content but different pointer"
        );
        value64free(src, VALUE64_STR);
        value64free(dst, VALUE64_STR);
    }

    /* ========== 27. STR → FS ========== */
    test_sub("subtest %d: STR -> FS", ++subnum);
    {
        value64 src = value64_createstr("hello-fs");
        value64 dst = value64_convert(src, VALUE64_STR, VALUE64_FS);
        fs *dst_fs = value64_fs(dst);
        test_validatefree(
            strcmp(fs_str(dst_fs), "hello-fs") == 0,
            (value64free(src, VALUE64_STR), value64free(dst, VALUE64_FS)),
            "STR->FS: expected 'hello-fs', got '%s'", fs_str(dst_fs)
        );
        value64free(src, VALUE64_STR);
        value64free(dst, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ========== 28. Граничные значения: INT_MAX → LONG → DBL → STR → FS и обратно ========== */
    test_sub("subtest %d: round-trip INT_MAX", ++subnum);
    {
        value64 src = value64_createint(INT_MAX);
        value64 tmp = value64_convert(src, VALUE64_INT, VALUE64_LONG);
        test_validate(value64_long(tmp) == (long)INT_MAX, "INT_MAX -> LONG mismatch");
        tmp = value64_convert(tmp, VALUE64_LONG, VALUE64_DBL);
        test_validate(fabs(value64_dbl(tmp) - (double)INT_MAX) < 10.0, "INT_MAX -> DBL mismatch");
        // дальше можно в STR и обратно, но не будем усложнять
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_convert_move -----------------------------

static TestStatus
tf_convert_move(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. STR -> STR (move) */
    test_sub("subtest %d: STR -> STR (move)", ++subnum);
    {
        value64 src = value64_createstr("hello");
        value64 dst = value64_convert_move(&src, VALUE64_STR, VALUE64_STR);
        test_validatefree(
            strcmp(value64_str(dst), "hello") == 0 && value64_str(src) == NULL,
            value64free(dst, VALUE64_STR),
            "STR->STR: dst='%s', src must be NULL (got %p)",
            value64_str(dst), (void*)value64_str(src)
        );
        // Безопасно освобождаем пустой src
        value64free(src, VALUE64_STR);
        value64free(dst, VALUE64_STR);
    }

    /* 2. STR -> FS (move) */
    test_sub("subtest %d: STR -> FS (move)", ++subnum);
    {
        value64 src = value64_createstr("world");
        value64 dst = value64_convert_move(&src, VALUE64_STR, VALUE64_FS);
        fs *dst_fs = value64_fs(dst);
        test_validatefree(
            strcmp(fs_str(dst_fs), "world") == 0 && fs_bodyalloc(dst_fs),
            value64free(dst, VALUE64_FS),
            "STR->FS: dst='%s', src.sval must be NULL (got %p)",
            fs_str(dst_fs), (void*)value64_str(src)
        );
        test_validate(value64_str(src) == NULL, "After move, src.sval must be NULL");
        value64free(dst, VALUE64_FS);
        value64free(src, VALUE64_STR);   // src уже пуст, безопасно
        fs_alloc_check(true);
    }

    /* 3. FS -> STR (move) */
    test_sub("subtest %d: FS -> STR (move)", ++subnum);
    {
        fs tmp = fscopy("fs-string");
        value64 src = value64_createfs(&tmp);
        //fstechprint(*value64_fs(src) );

        fsfree(tmp);
        value64 dst = value64_convert_move(&src, VALUE64_FS, VALUE64_STR);

        test_validatefree(
            strcmp(value64_str(dst), "fs-string") == 0,
            value64free(dst, VALUE64_STR),
            "FS->STR: dst='%s', src.fsval must be NULL or emptied",
            value64_str(dst)
        );
        test_validate(
            value64_fs(src) == NULL || fs_len(value64_fs(src)) == 0,
            "src.fsval must be empty after move"
        );
        value64free(dst, VALUE64_STR);
        value64free(src, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 4. FS -> FS (move) */
    test_sub("subtest %d: FS -> FS (move)", ++subnum);
    {
        fs tmp = fscopy("move-fs");
        value64 src = value64_createfs(&tmp);
        fsfree(tmp);
        value64 dst = value64_convert_move(&src, VALUE64_FS, VALUE64_FS);
        fs *dst_fs = value64_fs(dst);
        test_validatefree(
            strcmp(fs_str(dst_fs), "move-fs") == 0 && fs_bodyalloc(dst_fs) &&
            value64_fs(src) == NULL,
            value64free(dst, VALUE64_FS),
            "FS->FS: dst='%s', src.fsval must be NULL (got %p)",
            fs_str(dst_fs), (void*)value64_fs(src)
        );
        value64free(dst, VALUE64_FS);
        value64free(src, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 5. INT -> STR (неподдерживаемая комбинация, должна упасть) */
    test_sub("subtest %d: INT -> STR (must raise error)", ++subnum);
    {
        value64 src = value64_createint(42);
        if (!try()) {
            value64 dst = value64_convert_move(&src, VALUE64_INT, VALUE64_STR);
            test_validate(
                false,
                "INT->STR must raise error, but returned %p",
                (void*)value64_str(dst)
            );
        } else {
            test_validate(
                true,
                "INT->STR correctly raised error"
            );
        }
        // src не изменился, освобождать не нужно
    }

    /* 6. STR -> INT (неподдерживаемая комбинация) */
    test_sub("subtest %d: STR -> INT (must raise error)", ++subnum);
    {
        value64 src = value64_createstr("123");
        if (!try()) {
            value64 dst = value64_convert_move(&src, VALUE64_STR, VALUE64_INT);
            test_validatefree(
                false,
                value64free(src, VALUE64_STR),
                "STR->INT must raise error, but returned %d", value64_int(dst)
            );
        } else {
            test_validatefree(
                true,
                value64free(src, VALUE64_STR),
                "STR->INT correctly raised error"
            );
        }
        value64free(src, VALUE64_STR);
    }

    /* 7. NULL source (должен упасть) */
    test_sub("subtest %d: NULL source", ++subnum);
    {
        if (!try()) {
            value64_convert_move(NULL, VALUE64_STR, VALUE64_FS);
            test_validate(
                false,
                "convert_move(NULL) must raise error"
            );
        } else {
            test_validate(
                true,
                "convert_move(NULL) correctly raised error"
            );
        }
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_is_convertable -----------------------------
static TestStatus
tf_is_convertable(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT -> INT: допустимо */
    test_sub("subtest %d: INT->INT (allowed)", ++subnum);
    {
        value64 v = value64_createint(42);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_INT),
            "INT->INT must be convertable"
        );
    }

    /* 1. INT -> LONG: допустимо */
    test_sub("subtest %d: INT->LONG (allowed)", ++subnum);
    {
        value64 v = value64_createint(42);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_LONG),
            "INT->LONG must be convertable"
        );
    }

    test_sub("subtest %d: INT to ULONG (positive)", ++subnum);
    {
        value64 v = LITERAL64_INT(42);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_ULONG),
            "INT(42) -> ULONG must be convertable"
        );
    }

    test_sub("subtest %d: INT to ULONG (negative)", ++subnum);
    {
        value64 v = LITERAL64_INT(-1);
        test_validate(
            !value64_is_convertable(v, VALUE64_INT, VALUE64_ULONG),
            "INT(-1) -> ULONG must NOT be convertable"
        );
    }

    /* 2. INT -> DBL: допустимо */
    test_sub("subtest %d: INT->DBL (allowed)", ++subnum);
    {
        value64 v = value64_createint(42);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_DBL),
            "INT->DBL must be convertable"
        );
    }

    /* 3. INT -> INT: допустимо (identity) */
    test_sub("subtest %d: INT->INT (allowed)", ++subnum);
    {
        value64 v = value64_createint(42);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_INT),
            "INT->INT must be convertable"
        );
    }

    /* 5. INT -> CHR (в диапазоне) */
    test_sub("subtest %d: INT->CHR (in range)", ++subnum);
    {
        value64 v = value64_createint(65);   // 'A'
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_CHR),
            "INT->CHR (65) must be convertable"
        );
    }

    /* 6. INT -> CHR (вне диапазона) */
    test_sub("subtest %d: INT->CHR (out of range)", ++subnum);
    {
        value64 v = value64_createint(9999);
        test_validate(
            !value64_is_convertable(v, VALUE64_INT, VALUE64_CHR),
            "INT->CHR (9999) must NOT be convertable"
        );
    }

    /* 7. INT → BOOL (разрешено всегда) */
    test_sub("subtest %d: INT(0) -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createint(0);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_BOOL),
            "INT(0)->BOOL must be convertable"
        );
    }

    test_sub("subtest %d: INT(1) -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createint(1);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_BOOL),
            "INT(1)->BOOL must be convertable"
        );
    }

    test_sub("subtest %d: INT(999) -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createint(999);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_BOOL),
            "INT(999)->BOOL must be convertable (any non‑zero -> true)"
        );
    }

    /* 3. INT -> FS */
    test_sub("subtest %d: INT->FS (allowed)", ++subnum);
    {
        value64 v = value64_createint(-5);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_FS),
            "INT->FS must be convertable"
        );
    }

    /* 4. INT -> STR */
    test_sub("subtest %d: INT->STR (allowed)", ++subnum);
    {
        value64 v = value64_createint(0);
        test_validate(
            value64_is_convertable(v, VALUE64_INT, VALUE64_STR),
            "INT->STR must be convertable"
        );
    }

    /* 4. LONG -> INT (в пределах) – допустимо */
    test_sub("subtest %d: LNG->INT (in range)", ++subnum);
    {
        value64 v = value64_createlong(123456L);
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_INT),
            "LNG->INT (in range) must be convertable"
        );
    }

    /* 5. LONG -> INT (переполнение) – НЕ допустимо */
    test_sub("subtest %d: LNG->INT (overflow)", ++subnum);
    {
        value64 v = value64_createlong(2147483648L);
        test_validate(
            !value64_is_convertable(v, VALUE64_LONG, VALUE64_INT),
            "LNG->INT overflow must NOT be convertable"
        );
    }

    test_sub("subtest %d: LONG to ULONG (positive fits)", ++subnum);
    {
        value64 v = LITERAL64_LONG(123L);
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_ULONG),
            "LONG(123) -> ULONG must be convertable"
        );
    }

    test_sub("subtest %d: LONG to ULONG (negative)", ++subnum);
    {
        value64 v = LITERAL64_LONG(-5L);
        test_validate(
            !value64_is_convertable(v, VALUE64_LONG, VALUE64_ULONG),
            "LONG(-5) -> ULONG must NOT be convertable"
        );
    }

    /* 9. LNG -> DBL */
    test_sub("subtest %d: LNG->DBL (allowed)", ++subnum);
    {
        value64 v = value64_createlong(1L << 60);
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_DBL),
            "LNG->DBL must be convertable"
        );
    }

    /* 10. LNG -> FS */
    test_sub("subtest %d: LNG->FS (allowed)", ++subnum);
    {
        value64 v = value64_createlong(0L);
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_FS),
            "LNG->FS must be convertable"
        );
    }

    /* 11. LNG -> STR */
    test_sub("subtest %d: LNG->STR (allowed)", ++subnum);
    {
        value64 v = value64_createlong(-999L);
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_STR),
            "LNG->STR must be convertable"
        );
    }

    /* 12. LNG -> CHR (в диапазоне) */
    test_sub("subtest %d: LNG->CHR (in range)", ++subnum);
    {
        value64 v = value64_createlong(97L);   // 'a'
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_CHR),
            "LNG->CHR (97) must be convertable"
        );
    }

    /* 13. LNG -> CHR (вне диапазона) */
    test_sub("subtest %d: LNG->CHR (out of range)", ++subnum);
    {
        value64 v = value64_createlong(100000L);
        test_validate(
            !value64_is_convertable(v, VALUE64_LONG, VALUE64_CHR),
            "LNG->CHR (100000) must NOT be convertable"
        );
    }

    /* 8. LNG → BOOL (разрешено всегда) */
    test_sub("subtest %d: LNG(0L) -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createlong(0L);
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_BOOL),
            "LNG(0)->BOOL must be convertable"
        );
    }

    test_sub("subtest %d: LNG(1L) -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createlong(1L);
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_BOOL),
            "LNG(1)->BOOL must be convertable"
        );
    }

    test_sub("subtest %d: LNG(1000L) -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createlong(1000L);
        test_validate(
            value64_is_convertable(v, VALUE64_LONG, VALUE64_BOOL),
            "LNG(1000)->BOOL must be convertable (any non‑zero -> true)"
        );
    }

    /* ========== VALUE64_ULONG convertability ========== */

    test_sub("subtest %d: ULONG to ULONG is convertable", ++subnum);
    {
        value64 v = value64_createulong(12345UL);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_ULONG),
            "ULONG -> ULONG must be convertable"
        );
    }

    test_sub("subtest %d: ULONG to INT (fits)", ++subnum);
    {
        value64 v = value64_createulong(42UL);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_INT),
            "ULONG(42) -> INT must be convertable"
        );
    }

    test_sub("subtest %d: ULONG to INT (overflow)", ++subnum);
    {
        value64 v = value64_createulong(ULONG_MAX);
        test_validate(
            !value64_is_convertable(v, VALUE64_ULONG, VALUE64_INT),
            "ULONG(ULONG_MAX) -> INT must NOT be convertable"
        );
    }

    test_sub("subtest %d: ULONG to LONG (fits)", ++subnum);
    {
        value64 v = value64_createulong(LONG_MAX);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_LONG),
            "ULONG(LONG_MAX) -> LONG must be convertable"
        );
    }

    test_sub("subtest %d: ULONG to LONG (overflow)", ++subnum);
    {
        value64 v = value64_createulong((unsigned long)LONG_MAX + 1UL);
        test_validate(
            !value64_is_convertable(v, VALUE64_ULONG, VALUE64_LONG),
            "ULONG(LONG_MAX+1) -> LONG must NOT be convertable"
        );
    }

    test_sub("subtest %d: ULONG to DBL (always)", ++subnum);
    {
        value64 v = value64_createulong(ULONG_MAX);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_DBL),
            "ULONG -> DBL must be convertable (even with precision loss)"
        );
    }

    test_sub("subtest %d: ULONG to non‑numeric types", ++subnum);
    {
        value64 v = value64_createulong(0);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_BOOL) &&
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_CHR)  &&
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_STR)  &&
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_FS)   &&
            !value64_is_convertable(v, VALUE64_ULONG, VALUE64_PTR),
            "ULONG -> BOOL/CHR/STR/FS must be convertable, PTR must NOT"
        );
    }

    /* ========== ULONG convertability to non‑numeric ========== */

    test_sub("subtest %d: ULONG to BOOL is convertable", ++subnum);
    {
        value64 v = value64_createulong(0);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_BOOL),
            "ULONG -> BOOL must be convertable"
        );
    }

    test_sub("subtest %d: ULONG to CHR is convertable", ++subnum);
    {
        value64 v = value64_createulong(65);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_CHR),
            "ULONG -> CHR must be convertable"
        );
    }

    test_sub("subtest %d: ULONG to STR is convertable", ++subnum);
    {
        value64 v = value64_createulong(12345);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_STR),
            "ULONG -> STR must be convertable"
        );
    }

    test_sub("subtest %d: ULONG to FS is convertable", ++subnum);
    {
        value64 v = value64_createulong(99999);
        test_validate(
            value64_is_convertable(v, VALUE64_ULONG, VALUE64_FS),
            "ULONG -> FS must be convertable"
        );
    }

    test_sub("subtest %d: ULONG to PTR is NOT convertable", ++subnum);
    {
        value64 v = value64_createulong(0);
        test_validate(
            !value64_is_convertable(v, VALUE64_ULONG, VALUE64_PTR),
            "ULONG -> PTR must NOT be convertable"
        );
    }

    /* CHAR */
    test_sub("subtest %d: CHR->INT", ++subnum);
    {
        value64 v = value64_createchar('X');
        test_validate(
            value64_is_convertable(v, VALUE64_CHR, VALUE64_INT),
            "CHR->PTR must  be convertable"
        );
    }

    test_sub("subtest %d: CHR->LNG", ++subnum);
    {
        value64 v = value64_createchar('X');
        test_validate(
            value64_is_convertable(v, VALUE64_CHR, VALUE64_LONG),
            "CHR->PTR must be convertable"
        );
    }

    test_sub("subtest %d: CHR->FS", ++subnum);
    {
        value64 v = value64_createchar('X');
        test_validate(
            value64_is_convertable(v, VALUE64_CHR, VALUE64_FS),
            "CHR->FS must  be convertable"
        );
    }

    test_sub("subtest %d: CHR->STR", ++subnum);
    {
        value64 v = value64_createchar('X');
        test_validate(
            value64_is_convertable(v, VALUE64_CHR, VALUE64_STR),
            "CHR->STR must  be convertable"
        );
    }

    /* 22. CHR -> PTR (forbidden) */
    test_sub("subtest %d: CHR->PTR (forbidden)", ++subnum);
    {
        value64 v = value64_createchar('X');
        test_validate(
            !value64_is_convertable(v, VALUE64_CHR, VALUE64_PTR),
            "CHR->PTR must NOT be convertable"
        );
    }

    /* ========================================================================
     * BOOL → другие типы
     * ======================================================================== */

    /* 1. BOOL -> INT (разрешено всегда) */
    test_sub("subtest %d: BOOL -> INT (always allowed)", ++subnum);
    {
        value64 v = value64_createbool(true);
        test_validate(
            value64_is_convertable(v, VALUE64_BOOL, VALUE64_INT),
            "BOOL->INT must be convertable"
        );
    }

    /* 2. BOOL -> LNG (разрешено всегда) */
    test_sub("subtest %d: BOOL -> LNG (always allowed)", ++subnum);
    {
        value64 v = value64_createbool(false);
        test_validate(
            value64_is_convertable(v, VALUE64_BOOL, VALUE64_LONG),
            "BOOL->LNG must be convertable"
        );
    }

    /* 3. BOOL -> DBL (ЗАПРЕЩЕНО) */
    test_sub("subtest %d: BOOL -> DBL (forbidden)", ++subnum);
    {
        value64 v = value64_createbool(true);
        test_validate(
            !value64_is_convertable(v, VALUE64_BOOL, VALUE64_DBL),
            "BOOL->DBL must NOT be convertable"
        );
    }

    /* 3. BOOL -> PTR (ЗАПРЕЩЕНО) */
    test_sub("subtest %d: BOOL -> PTR (forbidden)", ++subnum);
    {
        value64 v = value64_createbool(true);
        test_validate(
            !value64_is_convertable(v, VALUE64_BOOL, VALUE64_PTR),
            "BOOL->PTR must NOT be convertable"
        );
    }

    /* 4. BOOL -> FS (разрешено) */
    test_sub("subtest %d: BOOL -> FS (allowed)", ++subnum);
    {
        value64 v = value64_createbool(false);
        test_validate(
            value64_is_convertable(v, VALUE64_BOOL, VALUE64_FS),
            "BOOL->FS must be convertable"
        );
    }

    /* 5. BOOL -> STR (разрешено) */
    test_sub("subtest %d: BOOL -> STR (allowed)", ++subnum);
    {
        value64 v = value64_createbool(true);
        test_validate(
            value64_is_convertable(v, VALUE64_BOOL, VALUE64_STR),
            "BOOL->STR must be convertable"
        );
    }

    /* 6. BOOL -> CHR (запрещено) */
    test_sub("subtest %d: BOOL -> CHR (allowed)", ++subnum);
    {
        value64 v = value64_createbool(true);
        test_validate(
            value64_is_convertable(v, VALUE64_BOOL, VALUE64_CHR),
            "BOOL->CHR must  be convertable"
        );
    }

    /* 6. DBL -> INT (в пределах) – допустимо */
    test_sub("subtest %d: DBL->INT (in range)", ++subnum);
    {
        value64 v = value64_createdbl(3.14);
        test_validate(
            value64_is_convertable(v, VALUE64_DBL, VALUE64_INT),
            "DBL->INT (in range) must be convertable"
        );
    }

    /* 7. DBL -> INT (переполнение) – НЕ допустимо */
    test_sub("subtest %d: DBL->INT (overflow)", ++subnum);
    {
        value64 v = value64_createdbl(1.0e30);
        test_validate(
            !value64_is_convertable(v, VALUE64_DBL, VALUE64_INT),
            "DBL->INT overflow must NOT be convertable"
        );
    }

    /* 8. DBL -> INT (NaN) – НЕ допустимо */
    test_sub("subtest %d: DBL->INT (NaN)", ++subnum);
    {
        value64 v = value64_createdbl(NAN);
        test_validate(
            !value64_is_convertable(v, VALUE64_DBL, VALUE64_INT),
            "DBL->INT NaN must NOT be convertable"
        );
    }

    /* 16. DBL -> LNG (в диапазоне) */
    test_sub("subtest %d: DBL->LNG (in range)", ++subnum);
    {
        value64 v = value64_createdbl(5.0e9);
        test_validate(
            value64_is_convertable(v, VALUE64_DBL, VALUE64_LONG),
            "DBL->LNG (5e9) must be convertable"
        );
    }

    /* 9. DBL -> LONG (переполнение) – НЕ допустимо */
    test_sub("subtest %d: DBL->LNG (overflow)", ++subnum);
    {
        value64 v = value64_createdbl(1.0e30);
        test_validate(
            !value64_is_convertable(v, VALUE64_DBL, VALUE64_LONG),
            "DBL->LNG overflow must NOT be convertable"
        );
    }

    test_sub("subtest %d: DBL to ULONG (positive integer)", ++subnum);
    {
        value64 v = LITERAL64_DBL(100.0);
        test_validate(
            value64_is_convertable(v, VALUE64_DBL, VALUE64_ULONG),
            "DBL(100.0) -> ULONG must be convertable"
        );
    }

    test_sub("subtest %d: DBL to ULONG (fractional)", ++subnum);
    {
        value64 v = LITERAL64_DBL(3.14);
        test_validate(
            value64_is_convertable(v, VALUE64_DBL, VALUE64_ULONG),
            "DBL(3.14) -> ULONG must be convertable"
        );
    }

    test_sub("subtest %d: DBL to ULONG (negative)", ++subnum);
    {
        value64 v = LITERAL64_DBL(-1.0);
        test_validate(
            !value64_is_convertable(v, VALUE64_DBL, VALUE64_ULONG),
            "DBL(-1.0) -> ULONG must NOT be convertable"
        );
    }

    /* 21. DBL -> CHR */
    test_sub("subtest %d: DBL->CHR (forbidden)", ++subnum);
    {
        value64 v = value64_createdbl(65.0);
        test_validate(
            !value64_is_convertable(v, VALUE64_DBL, VALUE64_CHR),
            "DBL->CHR must NOT be convertable"
        );
    }

    /* 9. DBL → BOOL (ЗАПРЕЩЕНО) */
    test_sub("subtest %d: DBL(0.0) -> BOOL (forbidden)", ++subnum);
    {
        value64 v = value64_createdbl(0.0);
        test_validate(
            !value64_is_convertable(v, VALUE64_DBL, VALUE64_BOOL),
            "DBL->BOOL must NOT be convertable"
        );
    }

    test_sub("subtest %d: DBL(3.14) -> BOOL (forbidden)", ++subnum);
    {
        value64 v = value64_createdbl(3.14);
        test_validate(
            !value64_is_convertable(v, VALUE64_DBL, VALUE64_BOOL),
            "DBL(3.14)->BOOL must NOT be convertable"
        );
    }

    /* 18. DBL -> FS */
    test_sub("subtest %d: DBL->FS (allowed)", ++subnum);
    {
        value64 v = value64_createdbl(3.14);
        test_validate(
            value64_is_convertable(v, VALUE64_DBL, VALUE64_FS),
            "DBL->FS must be convertable"
        );
    }

    /* 19. DBL -> STR */
    test_sub("subtest %d: DBL->STR (allowed)", ++subnum);
    {
        value64 v = value64_createdbl(0.0);
        test_validate(
            value64_is_convertable(v, VALUE64_DBL, VALUE64_STR),
            "DBL->STR must be convertable"
        );
    }


    /* 10. FS -> INT: не должно падать */
    test_sub("subtest %d: FS->INT (must not crash)", ++subnum);
    {
        fs tmp = fscopy("123");
        value64 v = value64_createfs(&tmp);
        fsfree(tmp);
        // Просто вызываем, результат не проверяем, но убеждаемся, что не упали
        test_validatefree(
            true,   // основное условие — дошли до этой точки
            value64free(v, VALUE64_FS),
            "FS->INT must not crash (result was %s)",
            bool_str(value64_is_convertable(v, VALUE64_FS, VALUE64_INT))
        );
        value64free(v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 23. FS -> PTR (forbidden) */
    test_sub("subtest %d: FS->PTR (forbidden)", ++subnum);
    {
        value64 v = value64_createfs_asstr("/tmp");
        test_validatefree(
            !value64_is_convertable(v, VALUE64_FS, VALUE64_PTR),
            value64_freefs(&v),
            "FS->PTR must NOT be convertable"
        );
        value64_freefs(&v);
    }
    fs_alloc_check(true);

    /* 11. FS → BOOL (аналогично строкам) */
    test_sub("subtest %d: FS 'on' -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createfs_asstr("on");
        test_validatefree(
            value64_is_convertable(v, VALUE64_FS, VALUE64_BOOL),
            value64_freefs(&v),
            "FS 'on' -> BOOL must be convertable"
        );
        value64_freefs(&v);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: FS 'OFF' -> BOOL (case insensitive, allowed)", ++subnum);
    {
        value64 v = value64_createfs_asstr("OFF");
        test_validatefree(
            value64_is_convertable(v, VALUE64_FS, VALUE64_BOOL),
            value64_freefs(&v),
            "FS 'OFF' -> BOOL must be convertable"
        );
        value64_freefs(&v);
        fs_alloc_check(true);
    }

    /*test_sub("subtest %d: FS 'unknown' -> BOOL (forbidden)", ++subnum);
    {
        value64 v = value64_createfs_asstr("unknown");
        test_validatefree(
            !value64_is_convertable(v, VALUE64_FS, VALUE64_BOOL),
            value64_freefs(&v),
            "FS 'unknown' -> BOOL must NOT be convertable"
        );
        value64_freefs(&v);
        fs_alloc_check(true);
    }*/

    /* 11. STR -> DBL: не должно падать */
    test_sub("subtest %d: STR->DBL (must not crash)", ++subnum);
    {
        value64 v = value64_createstr("3.14");
        test_validatefree(
            true,
            value64free(v, VALUE64_STR),
            "STR->DBL must not crash (result was %s)",
            bool_str(value64_is_convertable(v, VALUE64_STR, VALUE64_DBL))
        );
        value64free(v, VALUE64_STR);
    }

    /* 24. STR -> PTR (forbidden) */
    test_sub("subtest %d: STR->PTR (forbidden)", ++subnum);
    {
        value64 v = value64_createstr("hello");
        test_validatefree(
            !value64_is_convertable(v, VALUE64_STR, VALUE64_PTR),
            value64_freestr(&v),
            "STR->PTR must NOT be convertable"
        );
        value64_freestr(&v);
    }

    /* 10. STR → BOOL (разрешено для валидных строк) */
    test_sub("subtest %d: STR 'true' -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createstr("true");
        test_validatefree(
            value64_is_convertable(v, VALUE64_STR, VALUE64_BOOL),
            value64_freestr(&v),
            "STR 'true' -> BOOL must be convertable"
        );
        value64_freestr(&v);
    }

    test_sub("subtest %d: STR 'false' -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createstr("false");
        test_validatefree(
            value64_is_convertable(v, VALUE64_STR, VALUE64_BOOL),
            value64_freestr(&v),
            "STR 'false' -> BOOL must be convertable"
        );
        value64_freestr(&v);
    }

    test_sub("subtest %d: STR 'yes' -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createstr("yes");
        test_validatefree(
            value64_is_convertable(v, VALUE64_STR, VALUE64_BOOL),
            value64_freestr(&v),
            "STR 'yes' -> BOOL must be convertable"
        );
        value64_freestr(&v);
    }

    test_sub("subtest %d: STR 'no' -> BOOL (allowed)", ++subnum);
    {
        value64 v = value64_createstr("no");
        test_validatefree(
            value64_is_convertable(v, VALUE64_STR, VALUE64_BOOL),
            value64_freestr(&v),
            "STR 'no' -> BOOL must be convertable"
        );
        value64_freestr(&v);
    }

    /*test_sub("subtest %d: STR 'invalid' -> BOOL (forbidden)", ++subnum);
    {
        value64 v = value64_createstr("invalid");
        test_validatefree(
            !value64_is_convertable(v, VALUE64_STR, VALUE64_BOOL),
            value64_freestr(&v),
            "STR 'invalid' -> BOOL must NOT be convertable"
        );
        value64_freestr(&v);
    } */

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_value64_pt_compare -----------------------------
static TestStatus
tf_pt_compare(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- INT ---------- */
    test_sub("subtest %d: cmp INT equal", ++subnum);
    {
        value64 a = value64_createint(42);
        value64 b = value64_createint(42);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_INT) == 0, 
            "42 must equal 42"
        );
    }

    test_sub("subtest %d: cmp INT less / greater", ++subnum);
    {
        value64 a = value64_createint(10);
        value64 b = value64_createint(20);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_INT) < 0,
            "10 must be less than 20"
        );
        test_validate(
            value64_pt_compare(&b, &a, VALUE64_INT) > 0,
            "20 must be greater than 10"
        );
    }

    /* ---------- LONG ---------- */
    test_sub("subtest %d: cmp LONG equal", ++subnum);
    {
        value64 a = value64_createlong(-999999L);
        value64 b = value64_createlong(-999999L);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_LONG) == 0,
            "-999999L must equal -999999L"
        );
    }

    test_sub("subtest %d: cmp LONG less / greater", ++subnum);
    {
        value64 a = value64_createlong(100L);
        value64 b = value64_createlong(200L);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_LONG) < 0,
            "100L must be less than 200L"
        );
        test_validate(
            value64_pt_compare(&b, &a, VALUE64_LONG) > 0,
            "200L must be greater than 100L"
        );
    }

    /* ---------- ULONG ---------- */
    test_sub("subtest %d: cmp ULONG equal", ++subnum);
    {
        value64 a = value64_createulong(999999UL);
        value64 b = value64_createulong(999999UL);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_ULONG) == 0,
            "999999UL must equal 999999UL"
        );
    }

    test_sub("subtest %d: cmp ULONG less / greater", ++subnum);
    {
        value64 a = value64_createulong(100UL);
        value64 b = value64_createulong(200UL);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_ULONG) < 0,
            "100UL must be less than 200UL"
        );
        test_validate(
            value64_pt_compare(&b, &a, VALUE64_ULONG) > 0,
            "200UL must be greater than 100UL"
        );
    }

    /* ---------- DBL ---------- */
    test_sub("subtest %d: cmp DBL equal", ++subnum);
    {
        value64 a = value64_createdbl(3.1415);
        value64 b = value64_createdbl(3.1415);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_DBL) == 0,
            "3.1415 must equal 3.1415"
        );
    }

    test_sub("subtest %d: cmp DBL less / greater", ++subnum);
    {
        value64 a = value64_createdbl(1.0);
        value64 b = value64_createdbl(2.0);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_DBL) < 0,
            "1.0 must be less than 2.0"
        );
        test_validate(
            value64_pt_compare(&b, &a, VALUE64_DBL) > 0,
            "2.0 must be greater than 1.0"
        );
    }

    test_sub("subtest %d: cmp DBL special (NaN, inf)", ++subnum);
    {
        value64 a = value64_createdbl(NAN);
        value64 b = value64_createdbl(NAN);
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_DBL) == 0,
            "NaN must equal NaN (by implementation)"
        );

        value64 inf1 = value64_createdbl(INFINITY);
        value64 inf2 = value64_createdbl(INFINITY);
        test_validate(
            value64_pt_compare(&inf1, &inf2, VALUE64_DBL) == 0,
            "+inf must equal +inf"
        );

        value64 ninf = value64_createdbl(-INFINITY);
        test_validate(
            value64_pt_compare(&inf1, &ninf, VALUE64_DBL) > 0,
            "+inf must be greater than -inf"
        );
    }

    /* ---------- CHAR ---------- */
    test_sub("subtest %d: cmp CHAR equal", ++subnum);
    {
        value64 a = value64_createchar('a');
        value64 b = value64_createchar('a');
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_CHR) == 0, 
            "'a' must equal 'a''"
        );
    }

    test_sub("subtest %d: cmp INT less / greater", ++subnum);
    {
        value64 a = value64_createint('a');
        value64 b = value64_createint('b');
        test_validate(
            value64_pt_compare(&a, &b, VALUE64_CHR) < 0,
            "'a' must be less than 'b'"
        );
        test_validate(
            value64_pt_compare(&b, &a, VALUE64_CHR) > 0,
            "'b' must be greater than 'a"
        );
    }

    /* ---------- BOOL ---------- */

    /* 1. Равные значения */
    test_sub("subtest %d: equal BOOL values", ++subnum);
    {
        value64 a = value64_createbool(true);
        value64 b = value64_createbool(true);
        test_validate(value64_pt_compare(&a, &b, VALUE64_BOOL) == 0,
                      "true == true must return 0");
    }

    /* 2. Разные значения (true vs false) */
    test_sub("subtest %d: true > false", ++subnum);
    {
        value64 a = value64_createbool(true);
        value64 b = value64_createbool(false);
        test_validate(value64_pt_compare(&a, &b, VALUE64_BOOL) > 0,
                      "true > false must be positive");
        test_validate(value64_pt_compare(&b, &a, VALUE64_BOOL) < 0,
                      "false < true must be negative");
    }

    /* 3. Сравнение с самим собой */
    test_sub("subtest %d: compare with itself", ++subnum);
    {
        value64 a = value64_createbool(true);
        test_validate(value64_pt_compare(&a, &a, VALUE64_BOOL) == 0,
                      "same instance must be equal");
    }

    /* 4. NULL-указатели (должны вызывать ошибку) */
    test_sub("subtest %d: NULL pointers raise SIGINT", ++subnum);
    {
        value64 a = value64_createbool(false);
        if (!try()) {
            value64_pt_compare(NULL, &a, VALUE64_BOOL);
            test_validate(false, "Should have raised SIGINT for NULL first arg");
        } else {
            logsimple("Exception correctly raised on NULL first arg");
        }
        if (!try()) {
            value64_pt_compare(&a, NULL, VALUE64_BOOL);
            test_validate(false, "Should have raised SIGINT for NULL second arg");
        } else {
            logsimple("Exception correctly raised on NULL second arg");
        }
        if (!try()) {
            value64_pt_compare(NULL, NULL, VALUE64_BOOL);
            test_validate(false, "Should have raised SIGINT for both NULL");
        } else {
            logsimple("Exception correctly raised on both NULL");
        }
    }

    /* ---------- PTR ---------- */
    test_sub("subtest %d: cmp PTR equal / not equal", ++subnum);
    {
        int x = 1, y = 2;
        value64 a = value64_createptr(&x);
        value64 b = value64_createptr(&x);
        value64 c = value64_createptr(&y);

        test_validate(
            value64_pt_compare(&a, &b, VALUE64_PTR) == 0,
            "same address must be equal"
        );
        test_validate(
            value64_pt_compare(&a, &c, VALUE64_PTR) != 0,
            "different addresses must not be equal"
        );
    }

    /* ---------- STR ---------- */
    test_sub("subtest %d: cmp STR equal", ++subnum);
    {
        value64 a = value64_createstr("hello");
        value64 b = value64_createstr("hello");
        test_validatefree(
            value64_pt_compare(&a, &b, VALUE64_STR) == 0,
            (value64free(a, VALUE64_STR), value64free(b, VALUE64_STR)),
            "'hello' must equal 'hello'"
        );
        value64freestr(a);
        value64freestr(b);
    }

    test_sub("subtest %d: cmp STR not equal", ++subnum);
    {
        value64 a = value64_createstr("abc");
        value64 b = value64_createstr("xyz");
        test_validatefree(
            value64_pt_compare(&a, &b, VALUE64_STR) != 0,
            (value64freestr(a), value64freestr(b) ),
            "'abc' must not equal 'xyz'"
        );
        value64freestr(a);
        value64freestr(b);
    }

    /* ---------- FS ---------- */
    test_sub("subtest %d: cmp FS equal", ++subnum);
    {
        fs tmp = fscopy("fs-data");
        value64 a = value64_createfs(&tmp);
        value64 b = value64_createfs(&tmp);
        fsfree(tmp);

        test_validatefree(
            value64_pt_compare(&a, &b, VALUE64_FS) == 0,
            (value64freefs(a), value64freefs(b)),
            "fs 'fs-data' must equal itself"
        );
        value64freefs(a);
        value64freefs(b);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: cmp FS not equal", ++subnum);
    {
        fs tmp1 = fscopy("alpha");
        fs tmp2 = fscopy("beta");
        value64 a = value64_createfs(&tmp1);
        value64 b = value64_createfs(&tmp2);
        fsfree(tmp1);
        fsfree(tmp2);

        test_validatefree(
            value64_pt_compare(&a, &b, VALUE64_FS) != 0,
            (value64freefs(a), value64freefs(b)),
            "fs 'alpha' must not equal 'beta'"
        );
        value64freefs(a);
        value64freefs(b);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: cmp FS empty vs non-empty", ++subnum);
    {
        fs empty_str = fscopy("");
        value64 a = value64_createfs(&empty_str);
        fsfree(empty_str);

        fs tmp2 = fscopy("non-empty");
        value64 b = value64_createfs(&tmp2);
        fsfree(tmp2);

        test_validatefree(
            value64_pt_compare(&a, &b, VALUE64_FS) != 0,
            (value64freestr(a), value64freestr(b)),
            "empty fs must not equal non-empty"
        );
        value64freefs(a);
        value64freefs(b);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_(rev)search -----------------------------

static TestStatus
tf_search(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- INT ---------- */
    test_sub("subtest %d: search INT – first", ++subnum);
    {
        value64 arr[] = { value64_createint(10), value64_createint(20), value64_createint(30) };
        test_validate(
            value64_search(value64_createint(10), VALUE64_INT, arr, COUNT(arr)) == 0,
            "10 must be at index 0"
        );
    }

    test_sub("subtest %d: search INT – middle", ++subnum);
    {
        value64 arr[] = { value64_createint(10), value64_createint(20), value64_createint(30) };
        test_validate(
            value64_search(value64_createint(20), VALUE64_INT, arr, COUNT(arr)) == 1,
            "20 must be at index 1"
        );
    }

    test_sub("subtest %d: search INT – last", ++subnum);
    {
        value64 arr[] = { value64_createint(10), value64_createint(20), value64_createint(30) };
        test_validate(
            value64_search(value64_createint(30), VALUE64_INT, arr, COUNT(arr)) == 2,
            "30 must be at index 2"
        );
    }

    test_sub("subtest %d: search INT – not found", ++subnum);
    {
        value64 arr[] = { value64_createint(10), value64_createint(20), value64_createint(30) };
        test_validate(
            value64_search(value64_createint(99), VALUE64_INT, arr, COUNT(arr)) == -1,
            "99 must not be found, return -1"
        );
    }

    test_sub("subtest %d: revsearch INT – last", ++subnum);
    {
        value64 arr[] = { value64_createint(10), value64_createint(20), value64_createint(30) };
        test_validate(
            value64_revsearch(value64_createint(30), VALUE64_INT, arr, COUNT(arr)) == 2,
            "30 must be at index 2 (reverse)"
        );
    }

    test_sub("subtest %d: revsearch INT – not found", ++subnum);
    {
        value64 arr[] = { value64_createint(10), value64_createint(20), value64_createint(30) };
        test_validate(
            value64_revsearch(value64_createint(99), VALUE64_INT, arr, COUNT(arr)) == -1,
            "99 must not be found (reverse), return -1"
        );
    }

    /* ---------- LONG ---------- */
    test_sub("subtest %d: search LONG – middle", ++subnum);
    {
        value64 arr[] = { value64_createlong(100L), value64_createlong(200L), value64_createlong(300L) };
        test_validate(
            value64_search(value64_createlong(200L), VALUE64_LONG, arr, COUNT(arr)) == 1,
            "200L must be at index 1"
        );
    }

    /* ---------- ULONG search ---------- */
    test_sub("subtest %d: search ULONG – middle", ++subnum);
    {
        value64 arr[] = { value64_createulong(100UL), value64_createulong(200UL), value64_createulong(300UL) };
        test_validate(
            value64_search(value64_createulong(200UL), VALUE64_ULONG, arr, COUNT(arr)) == 1,
            "200UL must be at index 1"
        );
    }

    test_sub("subtest %d: search ULONG – first", ++subnum);
    {
        value64 arr[] = { value64_createulong(10UL), value64_createulong(20UL), value64_createulong(30UL) };
        test_validate(
            value64_search(value64_createulong(10UL), VALUE64_ULONG, arr, COUNT(arr)) == 0,
            "10UL must be at index 0"
        );
    }

    test_sub("subtest %d: search ULONG – last", ++subnum);
    {
        value64 arr[] = { value64_createulong(1UL), value64_createulong(2UL), value64_createulong(3UL) };
        test_validate(
            value64_search(value64_createulong(3UL), VALUE64_ULONG, arr, COUNT(arr)) == 2,
            "3UL must be at index 2"
        );
    }

    test_sub("subtest %d: search ULONG – not found", ++subnum);
    {
        value64 arr[] = { value64_createulong(50UL), value64_createulong(60UL) };
        test_validate(
            value64_search(value64_createulong(999UL), VALUE64_ULONG, arr, COUNT(arr)) == -1,
            "999UL must not be found, return -1"
        );
    }

    test_sub("subtest %d: search ULONG – empty array", ++subnum);
    {
        test_validate(
            value64_search(value64_createulong(0UL), VALUE64_ULONG, NULL, 0) == -1,
            "empty array search must return -1"
        );
    }

    /* ---------- ULONG reverse search ---------- */
    test_sub("subtest %d: revsearch ULONG – last occurrence", ++subnum);
    {
        value64 arr[] = { value64_createulong(5UL), value64_createulong(10UL), value64_createulong(5UL) };
        test_validate(
            value64_revsearch(value64_createulong(5UL), VALUE64_ULONG, arr, COUNT(arr)) == 2,
            "5UL must be found at last index 2 (reverse)"
        );
    }

    test_sub("subtest %d: revsearch ULONG – not found", ++subnum);
    {
        value64 arr[] = { value64_createulong(7UL), value64_createulong(8UL) };
        test_validate(
            value64_revsearch(value64_createulong(0UL), VALUE64_ULONG, arr, COUNT(arr)) == -1,
            "0UL must not be found (reverse), return -1"
        );
    }

    test_sub("subtest %d: revsearch ULONG – empty array", ++subnum);
    {
        test_validate(
            value64_revsearch(value64_createulong(123UL), VALUE64_ULONG, NULL, 0) == -1,
            "empty array revsearch must return -1"
        );
    }

    /* ========== CHR ========== */
    test_sub("subtest %d: CHR search found", ++subnum);
    {
        value64 arr[] = { value64_createchar('A'), value64_createchar('B'), value64_createchar('C') };
        int idx = value64_search(value64_createchar('B'), VALUE64_CHR, arr, 3);
        test_validate(idx == 1, "CHR search: expected idx=1, got %d", idx);
    }

    test_sub("subtest %d: CHR rev search found (last)", ++subnum);
    {
        value64 arr[] = { value64_createchar('x'), value64_createchar('y'), value64_createchar('y'), value64_createchar('z') };
        int idx = value64_revsearch(value64_createchar('y'), VALUE64_CHR, arr, 4);
        test_validate(idx == 2, "CHR revsearch: expected idx=2, got %d", idx);
    }
    
    test_sub("subtest %d: CHR search found at first position", ++subnum);
    {
        value64 arr[] = { value64_createchar('A'), value64_createchar('B'), value64_createchar('C') };
        int idx = value64_search(value64_createchar('A'), VALUE64_CHR, arr, 3);
        test_validate(idx == 0, "CHR search first: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: CHR search found at last position", ++subnum);
    {
        value64 arr[] = { value64_createchar('X'), value64_createchar('Y'), value64_createchar('Z') };
        int idx = value64_search(value64_createchar('Z'), VALUE64_CHR, arr, 3);
        test_validate(idx == 2, "CHR search last: expected idx=2, got %d", idx);
    }

    test_sub("subtest %d: CHR search not found", ++subnum);
    {
        value64 arr[] = { value64_createchar('a'), value64_createchar('b'), value64_createchar('c') };
        int idx = value64_search(value64_createchar('x'), VALUE64_CHR, arr, 3);
        test_validate(idx == -1, "CHR search missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHR search in single element array (found)", ++subnum);
    {
        value64 arr[] = { value64_createchar('M') };
        int idx = value64_search(value64_createchar('M'), VALUE64_CHR, arr, 1);
        test_validate(idx == 0, "CHR search single found: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: CHR search in single element array (not found)", ++subnum);
    {
        value64 arr[] = { value64_createchar('M') };
        int idx = value64_search(value64_createchar('N'), VALUE64_CHR, arr, 1);
        test_validate(idx == -1, "CHR search single missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHR rev search found at first position (from end)", ++subnum);
    {
        value64 arr[] = { value64_createchar('A'), value64_createchar('B'), value64_createchar('A') };
        int idx = value64_revsearch(value64_createchar('A'), VALUE64_CHR, arr, 3);
        test_validate(idx == 2, "CHR revsearch: expected last idx=2, got %d", idx);
    }

    test_sub("subtest %d: CHR rev search found at last position (from end)", ++subnum);
    {
        value64 arr[] = { value64_createchar('X'), value64_createchar('Y'), value64_createchar('Z') };
        int idx = value64_revsearch(value64_createchar('X'), VALUE64_CHR, arr, 3);
        test_validate(idx == 0, "CHR revsearch first from end: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: CHR rev search not found", ++subnum);
    {
        value64 arr[] = { value64_createchar('p'), value64_createchar('q'), value64_createchar('r') };
        int idx = value64_revsearch(value64_createchar('s'), VALUE64_CHR, arr, 3);
        test_validate(idx == -1, "CHR revsearch missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHR rev search in single element array (found)", ++subnum);
    {
        value64 arr[] = { value64_createchar('K') };
        int idx = value64_revsearch(value64_createchar('K'), VALUE64_CHR, arr, 1);
        test_validate(idx == 0, "CHR revsearch single found: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: CHR rev search in single element array (not found)", ++subnum);
    {
        value64 arr[] = { value64_createchar('K') };
        int idx = value64_revsearch(value64_createchar('L'), VALUE64_CHR, arr, 1);
        test_validate(idx == -1, "CHR revsearch single missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHR search empty array", ++subnum);
    {
        int idx = value64_search(value64_createchar('A'), VALUE64_CHR, NULL, 0);
        test_validate(idx == -1, "CHR search empty: expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHR rev search empty array", ++subnum);
    {
        int idx = value64_revsearch(value64_createchar('A'), VALUE64_CHR, NULL, 0);
        test_validate(idx == -1, "CHR revsearch empty: expected -1, got %d", idx);
    }

    /* ========== BOOL ========== */

    /* ========== Linear acs(first) ========== */
    test_sub("subtest %d: BOOL search found (first)", ++subnum);
    {
        value64 arr[] = {
            value64_createbool(false),
            value64_createbool(true),
            value64_createbool(false)
        };
        int idx = value64_search(value64_createbool(true), VALUE64_BOOL, arr, 3);
        test_validate(idx == 1,
                      "BOOL search true: expected idx=1, got %d", idx);
    }

    test_sub("subtest %d: BOOL search found (first of duplicates)", ++subnum);
    {
        value64 arr[] = {
            value64_createbool(true),
            value64_createbool(false),
            value64_createbool(true)
        };
        int idx = value64_search(value64_createbool(true), VALUE64_BOOL, arr, 3);
        test_validate(idx == 0,
                      "BOOL search first true: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: BOOL search not found", ++subnum);
    {
        value64 arr[] = {
            value64_createbool(false),
            value64_createbool(false)
        };
        int idx = value64_search(value64_createbool(true), VALUE64_BOOL, arr, 2);
        test_validate(idx == -1,
                      "BOOL search true missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: BOOL search single element found", ++subnum);
    {
        value64 arr[] = { value64_createbool(true) };
        int idx = value64_search(value64_createbool(true), VALUE64_BOOL, arr, 1);
        test_validate(idx == 0,
                      "BOOL search single found: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: BOOL search single element not found", ++subnum);
    {
        value64 arr[] = { value64_createbool(false) };
        int idx = value64_search(value64_createbool(true), VALUE64_BOOL, arr, 1);
        test_validate(idx == -1,
                      "BOOL search single missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: BOOL search empty array", ++subnum);
    {
        int idx = value64_search(value64_createbool(true), VALUE64_BOOL, NULL, 0);
        test_validate(idx == -1,
                      "BOOL search empty: expected -1, got %d", idx);
    }

    /* ========== Linear desc (last) ========== */
    test_sub("subtest %d: BOOL revsearch found (last)", ++subnum);
    {
        value64 arr[] = {
            value64_createbool(true),
            value64_createbool(false),
            value64_createbool(true)
        };
        int idx = value64_revsearch(value64_createbool(true), VALUE64_BOOL, arr, 3);
        test_validate(idx == 2,
                      "BOOL revsearch true: expected idx=2, got %d", idx);
    }

    test_sub("subtest %d: BOOL revsearch found (last of all false)", ++subnum);
    {
        value64 arr[] = {
            value64_createbool(false),
            value64_createbool(false),
            value64_createbool(false)
        };
        int idx = value64_revsearch(value64_createbool(false), VALUE64_BOOL, arr, 3);
        test_validate(idx == 2,
                      "BOOL revsearch false: expected idx=2, got %d", idx);
    }

    test_sub("subtest %d: BOOL revsearch not found", ++subnum);
    {
        value64 arr[] = {
            value64_createbool(false),
            value64_createbool(false)
        };
        int idx = value64_revsearch(value64_createbool(true), VALUE64_BOOL, arr, 2);
        test_validate(idx == -1,
                      "BOOL revsearch true missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: BOOL revsearch single element found", ++subnum);
    {
        value64 arr[] = { value64_createbool(true) };
        int idx = value64_revsearch(value64_createbool(true), VALUE64_BOOL, arr, 1);
        test_validate(idx == 0,
                      "BOOL revsearch single found: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: BOOL revsearch single element not found", ++subnum);
    {
        value64 arr[] = { value64_createbool(false) };
        int idx = value64_revsearch(value64_createbool(true), VALUE64_BOOL, arr, 1);
        test_validate(idx == -1,
                      "BOOL revsearch single missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: BOOL revsearch empty array", ++subnum);
    {
        int idx = value64_revsearch(value64_createbool(true), VALUE64_BOOL, NULL, 0);
        test_validate(idx == -1,
                      "BOOL revsearch empty: expected -1, got %d", idx);
    }

    /* ========== Проверка NULL-указателя (должно вызывать ошибку) ========== */
    test_sub("subtest %d: BOOL search NULL array with sz>0 raises", ++subnum);
    {
        if (!try()) {
            value64_search(value64_createbool(true), VALUE64_BOOL, NULL, 5);
            test_validate(false, "Should have raised SIGINT for NULL arr with sz>0");
        } else {
            logsimple("Exception correctly raised on NULL arr with sz>0");
        }
    }

    test_sub("subtest %d: BOOL revsearch NULL array with sz>0 raises", ++subnum);
    {
        if (!try()) {
            value64_revsearch(value64_createbool(true), VALUE64_BOOL, NULL, 5);
            test_validate(false, "Should have raised SIGINT for NULL arr with sz>0");
        } else {
            logsimple("Exception correctly raised on NULL arr with sz>0");
        }
    }

    /* ---------- DBL ---------- */
    test_sub("subtest %d: search DBL – equal", ++subnum);
    {
        value64 arr[] = { value64_createdbl(1.5), value64_createdbl(2.5), value64_createdbl(3.5) };
        test_validate(
            value64_search(value64_createdbl(2.5), VALUE64_DBL, arr, COUNT(arr)) == 1,
            "2.5 must be at index 1"
        );
    }

    /* ---------- STR ---------- */
    test_sub("subtest %d: search STR – found", ++subnum);
    {
        value64 arr[] = { value64_createstr("apple"), value64_createstr("banana"), value64_createstr("cherry") };
        value64 key = value64_createstr("banana");
        int pos;
        test_validate(
            (pos = value64_search(key, VALUE64_STR, arr, COUNT(arr) ) ) == 1, 
            "'banana' must be at index 1, got %d", pos
        );
        value64free(key, VALUE64_STR);
        for (int i = 0; i < COUNT(arr); i++) value64free(arr[i], VALUE64_STR);
    }

    test_sub("subtest %d: revsearch STR – last", ++subnum);
    {
        value64 arr[] = { value64_createstr("apple"), value64_createstr("banana"), value64_createstr("cherry") };
        value64 key = value64_createstr("cherry");
        int pos;
        test_validate(
            (pos = value64_revsearch(key, VALUE64_STR, arr, COUNT(arr) ) ) == 2,
            "'cherry' must be at index 2 (reverse), got %d", pos
        );
        value64free(key, VALUE64_STR);
        for (int i = 0; i < COUNT(arr); i++)
            value64free(arr[i], VALUE64_STR);
    }

    /* ---------- FS ---------- */
    test_sub("subtest %d: search FS – found", ++subnum);
    {
        fs tmp1 = fscopy("alpha"), tmp2 = fscopy("beta"), tmp3 = fscopy("gamma");
        value64 arr[] = { value64_createfs(&tmp1), value64_createfs(&tmp2), value64_createfs(&tmp3) };
        fsfree(tmp1); fsfree(tmp2); fsfree(tmp3);

        fs key_fs = fscopy("beta");
        value64 key = value64_createfs(&key_fs);
        fsfree(key_fs);

        int pos;
        test_validate(
            (pos = value64_search(key, VALUE64_FS, arr, COUNT(arr) ) ) == 1,
            "'beta' must be at index 1, got %d", pos
        );

        value64free(key, VALUE64_FS);
        for (int i = 0; i < COUNT(arr); i++)
            value64free(arr[i], VALUE64_FS);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: revsearch FS – first", ++subnum);
    {
        fs tmp1 = fscopy("alpha"), tmp2 = fscopy("beta"), tmp3 = fscopy("gamma");
        value64 arr[] = { value64_createfs(&tmp1), value64_createfs(&tmp2), value64_createfs(&tmp3) };
        fsfree(tmp1); fsfree(tmp2); fsfree(tmp3);

        fs key_fs = fscopy("alpha");
        value64 key = value64_createfs(&key_fs);
        fsfree(key_fs);

        int pos;
        test_validate(
            (pos = value64_revsearch(key, VALUE64_FS, arr, COUNT(arr) ) ) == 0,
            "'alpha' must be at index 0 (reverse), got %d", pos
        );

        value64free(key, VALUE64_FS);
        for (int i = 0; i < COUNT(arr); i++) value64free(arr[i], VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ---------- empty / edge ---------- */
    test_sub("subtest %d: search – empty array", ++subnum);
    {
        value64 key = value64_createint(1);
        test_validate(
            value64_search(key, VALUE64_INT, NULL, 0) == -1, 
            "empty array must return -1"
        );
    }

    test_sub("subtest %d: revsearch – empty array", ++subnum);
    {
        value64 key = value64_createint(1);
        test_validate(
            value64_revsearch(key, VALUE64_INT, NULL, 0) == -1,
            "empty array must return -1 (reverse)"
        );
    }
    /* 1. INT: присутствует */
    test_sub("subtest %d: value64_in – found", ++subnum);
    {
        value64 arr[] = { value64_createint(1), value64_createint(2), value64_createint(3) };
        test_validate(
            value64_in(value64_createint(2), VALUE64_INT, arr, COUNT(arr)),
            "2 must be in {1,2,3}"
        );
    }

    /* 2. INT: отсутствует */
    test_sub("subtest %d: value64_in – not found", ++subnum);
    {
        value64 arr[] = { value64_createint(1), value64_createint(2), value64_createint(3) };
        test_validate(
            !value64_in(value64_createint(99), VALUE64_INT, arr, COUNT(arr)),
            "99 must NOT be in {1,2,3}"
        );
    }

    /* 3. INT: notin (должно быть true) */
    test_sub("subtest %d: value64_notin – true", ++subnum);
    {
        value64 arr[] = { value64_createint(1), value64_createint(2), value64_createint(3) };
        test_validate(
            value64_notin(value64_createint(99), VALUE64_INT, arr, COUNT(arr)),
            "99 must be NOT in {1,2,3}"
        );
    }

    /* 4. INT: notin (должно быть false) */
    test_sub("subtest %d: value64_notin – false", ++subnum);
    {
        value64 arr[] = { value64_createint(1), value64_createint(2), value64_createint(3) };
        test_validate(
            !value64_notin(value64_createint(1), VALUE64_INT, arr, COUNT(arr)),
            "1 must be in {1,2,3}, so notin must be false"
        );
    }

    /* 5. Пустой массив – in возвращает false, notin возвращает true */
    test_sub("subtest %d: empty array", ++subnum);
    {
        test_validate(
            !value64_in(value64_createint(42), VALUE64_INT, NULL, 0),
            "in must be false for empty array"
        );
        test_validate(
            value64_notin(value64_createint(42), VALUE64_INT, NULL, 0),
            "notin must be true for empty array"
        );
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_get(Rev)Comparator -----------------------------

static TestStatus
tf_getComparator(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT comparator */
    test_sub("subtest %d: getComparator INT", ++subnum);
    {
        value64_Comparator cmp = value64_getComparator(VALUE64_INT);
        test_validate(cmp != NULL, "INT comparator must not be NULL");

        value64 a = value64_createint(10);
        value64 b = value64_createint(20);
        test_validate(cmp(a, b) < 0, "10 < 20 must be negative");
        test_validate(cmp(b, a) > 0, "20 > 10 must be positive");
        test_validate(cmp(a, a) == 0, "10 == 10 must be zero");
    }

    /* 2. INT rev comparator */
    test_sub("subtest %d: getRevComparator INT", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_INT);
        test_validate(rcmp != NULL, "INT rev comparator must not be NULL");

        value64 a = value64_createint(10);
        value64 b = value64_createint(20);
        test_validate(rcmp(a, b) > 0, "rev: 10 < 20 must give >0");
        test_validate(rcmp(b, a) < 0, "rev: 20 > 10 must give <0");
        test_validate(rcmp(a, a) == 0, "rev: 10 == 10 must be 0");
    }

    /* 3. LONG comparator */
    test_sub("subtest %d: getComparator LONG", ++subnum);
    {
        value64_Comparator cmp = value64_getComparator(VALUE64_LONG);
        test_validate(cmp != NULL, "LONG comparator must not be NULL");

        value64 a = value64_createlong(100L);
        value64 b = value64_createlong(200L);
        test_validate(cmp(a, b) < 0, "100 < 200 must be negative");
        test_validate(cmp(a, a) == 0, "100 == 100 must be zero");
    }

    /* 4. LONG rev comparator */
    test_sub("subtest %d: getRevComparator LONG", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_LONG);
        test_validate(rcmp != NULL, "LONG rev comparator must not be NULL");

        value64 a = value64_createlong(100L);
        value64 b = value64_createlong(200L);
        test_validate(rcmp(a, b) > 0, "rev: 100 < 200 must give >0");
    }

    /* 1. ULONG comparator */
    test_sub("subtest %d: getComparator ULONG", ++subnum);
    {
        value64_Comparator cmp = value64_getComparator(VALUE64_ULONG);
        test_validate(cmp != NULL, "ULONG comparator must not be NULL");

        value64 a = value64_createulong(10);
        value64 b = value64_createulong(20);
        test_validate(cmp(a, b) < 0, "10 < 20 must be negative");
        test_validate(cmp(b, a) > 0, "20 > 10 must be positive");
        test_validate(cmp(a, a) == 0, "10 == 10 must be zero");
    }

    /* 2. ULONG rev comparator */
    test_sub("subtest %d: getRevComparator ULONG", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_ULONG);
        test_validate(rcmp != NULL, "ULONG rev comparator must not be NULL");

        value64 a = value64_createulong(10);
        value64 b = value64_createulong(20);
        test_validate(rcmp(a, b) > 0, "rev: 10 < 20 must give >0");
        test_validate(rcmp(b, a) < 0, "rev: 20 > 10 must give <0");
        test_validate(rcmp(a, a) == 0, "rev: 10 == 10 must be 0");
    }

    /* CHAR comparator */
    test_sub("subtest %d: getComparator CHAR", ++subnum);
    {
        value64_Comparator cmp = value64_getComparator(VALUE64_CHR);
        test_validate(cmp != NULL, "CHAR comparator must not be NULL");

        value64 a = value64_createchar('t');
        value64 b = value64_createchar('z');
        test_validate(cmp(a, b) < 0, "'t' < 'z' must be negative");
        test_validate(cmp(b, a) > 0, "'z' > 't' must be positive");
        test_validate(cmp(a, a) == 0, "'t' == 't' must be zero");
    }

    /* CHAR rev comparator */
    test_sub("subtest %d: getRevComparator CHAR", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_CHR);
        test_validate(rcmp != NULL, "CHAR rev comparator must not be NULL");

        value64 a = value64_createchar('t');
        value64 b = value64_createchar('z');
        test_validate(rcmp(a, b) > 0, "rev: 't' < 'z' must give >0");
        test_validate(rcmp(b, a) < 0, "rev: 't' > 'z' must give <0");
        test_validate(rcmp(a, a) == 0, "rev: 't' == 't' must be 0");
    }

    /* 1. getComparator BOOL */
    test_sub("subtest %d: getComparator BOOL", ++subnum);
    {
        value64_Comparator cmp = value64_getComparator(VALUE64_BOOL);
        test_validate(cmp != NULL, "BOOL comparator must not be NULL");

        value64 a = value64_createbool(false);
        value64 b = value64_createbool(true);
        test_validate(cmp(a, b) < 0, "false < true must be negative");
        test_validate(cmp(b, a) > 0, "true > false must be positive");
        test_validate(cmp(a, a) == 0, "false == false must be zero");
        test_validate(cmp(b, b) == 0, "true == true must be zero");
    }

    /* 2. getRevComparator BOOL */
    test_sub("subtest %d: getRevComparator BOOL", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_BOOL);
        test_validate(rcmp != NULL, "BOOL rev comparator must not be NULL");

        value64 a = value64_createbool(false);
        value64 b = value64_createbool(true);
        test_validate(rcmp(a, b) > 0, "rev: false < true must give >0");
        test_validate(rcmp(b, a) < 0, "rev: true > false must give <0");
        test_validate(rcmp(a, a) == 0, "rev: false == false must be 0");
        test_validate(rcmp(b, b) == 0, "rev: true == true must be 0");
    }

    /* 5. DBL comparator */
    test_sub("subtest %d: getComparator DBL", ++subnum);
    {
        value64_Comparator cmp = value64_getComparator(VALUE64_DBL);
        test_validate(cmp != NULL, "DBL comparator must not be NULL");

        value64 a = value64_createdbl(1.5);
        value64 b = value64_createdbl(2.5);
        test_validate(cmp(a, b) < 0, "1.5 < 2.5 must be negative");
        test_validate(cmp(a, a) == 0, "1.5 == 1.5 must be zero");
    }

    /* 6. DBL rev comparator */
    test_sub("subtest %d: getRevComparator DBL", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_DBL);
        test_validate(rcmp != NULL, "DBL rev comparator must not be NULL");

        value64 a = value64_createdbl(1.5);
        value64 b = value64_createdbl(2.5);
        test_validate(rcmp(a, b) > 0, "rev: 1.5 < 2.5 must give >0");
    }

    /* 7. FS comparator */
    test_sub("subtest %d: getComparator FS", ++subnum);
    {
        value64_Comparator cmp = value64_getComparator(VALUE64_FS);
        test_validate(cmp != NULL, "FS comparator must not be NULL");

        fs tmp1 = fscopy("alpha"), tmp2 = fscopy("beta");
        value64 a = value64_createfs(&tmp1), b = value64_createfs(&tmp2);
        fsfree(tmp1); fsfree(tmp2);

        test_validatefree(
            cmp(a, b) < 0,
            (value64free(a, VALUE64_FS), value64free(b, VALUE64_FS)),
            "FS: 'alpha' < 'beta' must be negative"
        );
        // проверка равенства
        fs tmp3 = fscopy("alpha");
        value64 a2 = value64_createfs(&tmp3);
        fsfree(tmp3);
        test_validatefree(
            cmp(a, a2) == 0,
            (value64free(a, VALUE64_FS), value64free(a2, VALUE64_FS)),
            "FS: 'alpha' == 'alpha' must be zero"
        );
        value64free(a, VALUE64_FS); value64free(b, VALUE64_FS); value64free(a2, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 8. FS rev comparator */
    test_sub("subtest %d: getRevComparator FS", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_FS);
        test_validate(rcmp != NULL, "FS rev comparator must not be NULL");

        fs tmp1 = fscopy("alpha"), tmp2 = fscopy("beta");
        value64 a = value64_createfs(&tmp1), b = value64_createfs(&tmp2);
        fsfree(tmp1); fsfree(tmp2);

        test_validatefree(
            rcmp(a, b) > 0,
            (value64free(a, VALUE64_FS), value64free(b, VALUE64_FS)),
            "FS rev: 'alpha' < 'beta' must give >0"
        );
        value64free(a, VALUE64_FS); value64free(b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 9. STR comparator */
    test_sub("subtest %d: getComparator STR", ++subnum);
    {
        value64_Comparator cmp = value64_getComparator(VALUE64_STR);
        test_validate(cmp != NULL, "STR comparator must not be NULL");

        value64 a = value64_createstr("hello");
        value64 b = value64_createstr("world");
        test_validatefree(
            cmp(a, b) < 0,
            (value64free(a, VALUE64_STR), value64free(b, VALUE64_STR)),
            "STR: 'hello' < 'world' must be negative"
        );
        value64free(a, VALUE64_STR); value64free(b, VALUE64_STR);
    }

    /* 10. STR rev comparator */
    test_sub("subtest %d: getRevComparator STR", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_STR);
        test_validate(rcmp != NULL, "STR rev comparator must not be NULL");

        value64 a = value64_createstr("hello");
        value64 b = value64_createstr("world");
        test_validatefree(
            rcmp(a, b) > 0,
            (value64free(a, VALUE64_STR), value64free(b, VALUE64_STR)),
            "STR rev: 'hello' < 'world' must give >0"
        );
        value64free(a, VALUE64_STR); value64free(b, VALUE64_STR);
    }

    /* 11. PTR comparator (требуется исправление getComparator) */
    test_sub("subtest %d: getComparator PTR", ++subnum);
    {
        // Предполагаем, что вы исправили VALUE64_PTR -> value64_ptr_comp
        value64_Comparator cmp = value64_getComparator(VALUE64_PTR);
        test_validate(cmp != NULL, "PTR comparator must not be NULL");

        int x = 1, y = 2;
        value64 a = value64_createptr(&x);
        value64 b = value64_createptr(&y);
        test_validate(cmp(a, b) != 0, "different addresses must not be zero");
        test_validate(cmp(a, a) == 0, "same address must be zero");
    }

    /* 12. PTR rev comparator */
    test_sub("subtest %d: getRevComparator PTR", ++subnum);
    {
        value64_Comparator rcmp = value64_getRevComparator(VALUE64_PTR);
        test_validate(rcmp != NULL, "PTR rev comparator must not be NULL");

        int x = 1, y = 2;
        value64 a = value64_createptr(&x);
        value64 b = value64_createptr(&y);
        int direct = value64_getComparator(VALUE64_PTR)(a, b);
        test_validate(rcmp(a, b) == -direct, "PTR rev must negate direct result");
    }

    /* 13. Неподдерживаемый тип */
    test_sub("subtest %d: unsupported type (must raise error)", ++subnum);
    {
        if (!try()) {
            value64_Comparator cmp = value64_getComparator(VALUE64_UNKNOWN);
            test_validate(false, "Unsupported type must raise error, but returned %p", (void*)cmp);
        } else {
            test_validate(true, "Unsupported type correctly raised error");
        }
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_getP(Rev)Comparator -----------------------------
static TestStatus
tf_getPComparator(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- 1. P_INT comparator ---------- */
    test_sub("subtest %d: getPComparator INT", ++subnum);
    {
        value64_PComparator cmp = value64_getPComparator(VALUE64_INT);
        test_validate(cmp != NULL, "INT P-comparator must not be NULL");

        value64 a = value64_createint(10);
        value64 b = value64_createint(20);
        test_validate(cmp(&a, &b) < 0, "10 < 20 must be negative");
        test_validate(cmp(&b, &a) > 0, "20 > 10 must be positive");
        test_validate(cmp(&a, &a) == 0, "10 == 10 must be zero");
    }

    /* 2. P_INT rev comparator */
    test_sub("subtest %d: getPRevComparator INT", ++subnum);
    {
        value64_PComparator rcmp = value64_getPRevComparator(VALUE64_INT);
        test_validate(rcmp != NULL, "INT P-rev-comparator must not be NULL");

        value64 a = value64_createint(10);
        value64 b = value64_createint(20);
        test_validate(rcmp(&a, &b) > 0, "rev: 10 < 20 must give >0");
        test_validate(rcmp(&b, &a) < 0, "rev: 20 > 10 must give <0");
        test_validate(rcmp(&a, &a) == 0, "rev: 10 == 10 must be 0");
    }

    /* ---------- 3. P_LONG comparator ---------- */
    test_sub("subtest %d: getPComparator LONG", ++subnum);
    {
        value64_PComparator cmp = value64_getPComparator(VALUE64_LONG);
        test_validate(cmp != NULL, "LONG P-comparator must not be NULL");

        value64 a = value64_createlong(100L);
        value64 b = value64_createlong(200L);
        test_validate(cmp(&a, &b) < 0, "100 < 200 must be negative");
        test_validate(cmp(&a, &a) == 0, "100 == 100 must be zero");
    }

    /* 4. P_LONG rev comparator */
    test_sub("subtest %d: getPRevComparator LONG", ++subnum);
    {
        value64_PComparator rcmp = value64_getPRevComparator(VALUE64_LONG);
        test_validate(rcmp != NULL, "LONG P-rev-comparator must not be NULL");

        value64 a = value64_createlong(100L);
        value64 b = value64_createlong(200L);
        test_validate(rcmp(&a, &b) > 0, "rev: 100 < 200 must give >0");
    }

    /* ---------- 1. P_ULONG comparator ---------- */
    test_sub("subtest %d: getPComparator ULONG", ++subnum);
    {
        value64_PComparator cmp = value64_getPComparator(VALUE64_ULONG);
        test_validate(cmp != NULL, "ULONG P-comparator must not be NULL");

        value64 a = value64_createulong(10);
        value64 b = value64_createulong(20);
        test_validate(cmp(&a, &b) < 0, "10 < 20 must be negative");
        test_validate(cmp(&b, &a) > 0, "20 > 10 must be positive");
        test_validate(cmp(&a, &a) == 0, "10 == 10 must be zero");
    }

    /* 2. P_ULONG rev comparator */
    test_sub("subtest %d: getPRevComparator ULONG", ++subnum);
    {
        value64_PComparator rcmp = value64_getPRevComparator(VALUE64_ULONG);
        test_validate(rcmp != NULL, "ULONG P-rev-comparator must not be NULL");

        value64 a = value64_createulong(10);
        value64 b = value64_createulong(20);
        test_validate(rcmp(&a, &b) > 0, "rev: 10 < 20 must give >0");
        test_validate(rcmp(&b, &a) < 0, "rev: 20 > 10 must give <0");
        test_validate(rcmp(&a, &a) == 0, "rev: 10 == 10 must be 0");
    }

    /* ----------  P_CHARcomparator ---------- */
    test_sub("subtest %d: getPComparator CHAR", ++subnum);
    {
        value64_PComparator cmp = value64_getPComparator(VALUE64_CHR);
        test_validate(cmp != NULL, "CHAR P-comparator must not be NULL");

        value64 a = value64_createint('a');
        value64 b = value64_createint('z');
        test_validate(cmp(&a, &b) < 0, "'a' < 'z' must be negative");
        test_validate(cmp(&b, &a) > 0, "'z' > 'a' must be positive");
        test_validate(cmp(&a, &a) == 0, "'a' == 'a' must be zero");
    }

    /*  P_CHAR rev comparator */
    test_sub("subtest %d: getPRevComparator CHAR", ++subnum);
    {
        value64_PComparator rcmp = value64_getPRevComparator(VALUE64_CHR);
        test_validate(rcmp != NULL, "CHAR P-rev-comparator must not be NULL");

        value64 a = value64_createint('a');
        value64 b = value64_createint('z');
        test_validate(rcmp(&a, &b) > 0, "rev: 'a' < 'z' must give >0");
        test_validate(rcmp(&b, &a) < 0, "rev: 'z' > 'a' must give <0");
        test_validate(rcmp(&a, &a) == 0, "rev: 'a' == 'a' must be 0");
    }

    /* 3. getPComparator BOOL (pointer version) */
    test_sub("subtest %d: getPComparator BOOL", ++subnum);
    {
        value64_PComparator pcmp = value64_getPComparator(VALUE64_BOOL);
        test_validate(pcmp != NULL, "BOOL P-comparator must not be NULL");

        value64 a = value64_createbool(false);
        value64 b = value64_createbool(true);
        test_validate(pcmp(&a, &b) < 0, "P-cmp: false < true must be negative");
        test_validate(pcmp(&b, &a) > 0, "P-cmp: true > false must be positive");
        test_validate(pcmp(&a, &a) == 0, "P-cmp: false == false must be zero");
    }

    /* 4. getPRevComparator BOOL (pointer version, reverse) */
    test_sub("subtest %d: getPRevComparator BOOL", ++subnum);
    {
        value64_PComparator prev = value64_getPRevComparator(VALUE64_BOOL);
        test_validate(prev != NULL, "BOOL P-rev-comparator must not be NULL");

        value64 a = value64_createbool(false);
        value64 b = value64_createbool(true);
        test_validate(prev(&a, &b) > 0, "P-rev: false < true must give >0");
        test_validate(prev(&b, &a) < 0, "P-rev: true > false must give <0");
        test_validate(prev(&a, &a) == 0, "P-rev: false == false must be 0");
    }


    /* ---------- 5. P_DBL comparator ---------- */
    test_sub("subtest %d: getPComparator DBL", ++subnum);
    {
        value64_PComparator cmp = value64_getPComparator(VALUE64_DBL);
        test_validate(cmp != NULL, "DBL P-comparator must not be NULL");

        value64 a = value64_createdbl(1.5);
        value64 b = value64_createdbl(2.5);
        test_validate(cmp(&a, &b) < 0, "1.5 < 2.5 must be negative");
        test_validate(cmp(&a, &a) == 0, "1.5 == 1.5 must be zero");
    }

    /* 6. P_DBL rev comparator */
    test_sub("subtest %d: getPRevComparator DBL", ++subnum);
    {
        value64_PComparator rcmp = value64_getPRevComparator(VALUE64_DBL);
        test_validate(rcmp != NULL, "DBL P-rev-comparator must not be NULL");

        value64 a = value64_createdbl(1.5);
        value64 b = value64_createdbl(2.5);
        test_validate(rcmp(&a, &b) > 0, "rev: 1.5 < 2.5 must give >0");
    }

    /* ---------- 7. P_FS comparator ---------- */
    test_sub("subtest %d: getPComparator FS", ++subnum);
    {
        value64_PComparator cmp = value64_getPComparator(VALUE64_FS);
        test_validate(cmp != NULL, "FS P-comparator must not be NULL");

        fs tmp1 = fscopy("alpha"), tmp2 = fscopy("beta");
        value64 a = value64_createfs(&tmp1), b = value64_createfs(&tmp2);
        fsfree(tmp1); fsfree(tmp2);

        test_validatefree(
            cmp(&a, &b) < 0,
            (value64free(a, VALUE64_FS), value64free(b, VALUE64_FS)),
            "FS P-comparator: 'alpha' < 'beta' must be negative"
        );

        fs tmp3 = fscopy("alpha");
        value64 a2 = value64_createfs(&tmp3);
        fsfree(tmp3);
        test_validatefree(
            cmp(&a, &a2) == 0,
            (value64free(a, VALUE64_FS), value64free(a2, VALUE64_FS)),
            "FS P-comparator: 'alpha' == 'alpha' must be zero"
        );
        value64free(a, VALUE64_FS); value64free(b, VALUE64_FS); value64free(a2, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 8. P_FS rev comparator */
    test_sub("subtest %d: getPRevComparator FS", ++subnum);
    {
        value64_PComparator rcmp = value64_getPRevComparator(VALUE64_FS);
        test_validate(rcmp != NULL, "FS P-rev-comparator must not be NULL");

        fs tmp1 = fscopy("alpha"), tmp2 = fscopy("beta");
        value64 a = value64_createfs(&tmp1), b = value64_createfs(&tmp2);
        fsfree(tmp1); fsfree(tmp2);

        test_validatefree(
            rcmp(&a, &b) > 0,
            (value64free(a, VALUE64_FS), value64free(b, VALUE64_FS)),
            "FS P-rev: 'alpha' < 'beta' must give >0"
        );
        value64free(a, VALUE64_FS); value64free(b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ---------- 9. P_STR comparator ---------- */
    test_sub("subtest %d: getPComparator STR", ++subnum);
    {
        value64_PComparator cmp = value64_getPComparator(VALUE64_STR);
        test_validate(cmp != NULL, "STR P-comparator must not be NULL");

        value64 a = value64_createstr("hello");
        value64 b = value64_createstr("world");
        test_validatefree(
            cmp(&a, &b) < 0,
            (value64free(a, VALUE64_STR), value64free(b, VALUE64_STR)),
            "STR P-comparator: 'hello' < 'world' must be negative"
        );
        value64free(a, VALUE64_STR); value64free(b, VALUE64_STR);
    }

    /* 10. P_STR rev comparator */
    test_sub("subtest %d: getPRevComparator STR", ++subnum);
    {
        value64_PComparator rcmp = value64_getPRevComparator(VALUE64_STR);
        test_validate(rcmp != NULL, "STR P-rev-comparator must not be NULL");

        value64 a = value64_createstr("hello");
        value64 b = value64_createstr("world");
        test_validatefree(
            rcmp(&a, &b) > 0,
            (value64free(a, VALUE64_STR), value64free(b, VALUE64_STR)),
            "STR P-rev: 'hello' < 'world' must give >0"
        );
        value64free(a, VALUE64_STR); value64free(b, VALUE64_STR);
    }

    /* ---------- 11. P_PTR comparator ---------- */
    test_sub("subtest %d: getPComparator PTR", ++subnum);
    {
        value64_PComparator cmp = value64_getPComparator(VALUE64_PTR);
        test_validate(cmp != NULL, "PTR P-comparator must not be NULL");

        int x = 1, y = 2;
        value64 a = value64_createptr(&x);
        value64 b = value64_createptr(&y);
        test_validate(cmp(&a, &b) != 0, "different addresses must not be zero");
        test_validate(cmp(&a, &a) == 0, "same address must be zero");
    }

    /* 12. P_PTR rev comparator */
    test_sub("subtest %d: getPRevComparator PTR", ++subnum);
    {
        value64_PComparator rcmp = value64_getPRevComparator(VALUE64_PTR);
        test_validate(rcmp != NULL, "PTR P-rev-comparator must not be NULL");

        int x = 1, y = 2;
        value64 a = value64_createptr(&x);
        value64 b = value64_createptr(&y);
        int direct = value64_getPComparator(VALUE64_PTR)(&a, &b);
        test_validate(rcmp(&a, &b) == -direct, "PTR P-rev must negate direct result");
    }

    /* ---------- 13. Неподдерживаемый тип ---------- */
    test_sub("subtest %d: unsupported type (must raise error)", ++subnum);
    {
        if (!try()) {
            value64_PComparator cmp = value64_getPComparator(VALUE64_UNKNOWN);
            test_validate(false, "Unsupported type must raise error, but returned %p", (void*)cmp);
        } else {
            test_validate(true, "Unsupported type correctly raised error");
        }
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_(rev_)binsearch -----------------------------

static TestStatus
tf_binsearch(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- INT ascending (value64_binsearch) ---------- */
    test_sub("subtest %d: binsearch INT – found", ++subnum);
    {
        // массив строго по возрастанию
        value64 arr[] = {
            value64_createint(10),
            value64_createint(20),
            value64_createint(30),
            value64_createint(40),
            value64_createint(50)
        };
        value64 key = value64_createint(30);
        test_validate(
            value64_binsearch(key, VALUE64_INT, arr, COUNT(arr)) == 2,
            "30 must be at index 2"
        );
    }

    test_sub("subtest %d: binsearch INT – not found", ++subnum);
    {
        value64 arr[] = {
            value64_createint(10),
            value64_createint(20),
            value64_createint(30)
        };
        value64 key = value64_createint(25);
        test_validate(
            value64_binsearch(key, VALUE64_INT, arr, COUNT(arr)) == -1,
            "25 must not be found"
        );
    }

    test_sub("subtest %d: binsearch INT – first element", ++subnum);
    {
        value64 arr[] = {
            value64_createint(10),
            value64_createint(20),
            value64_createint(30)
        };
        value64 key = value64_createint(10);
        test_validate(
            value64_binsearch(key, VALUE64_INT, arr, COUNT(arr)) == 0,
            "10 must be at index 0"
        );
    }

    test_sub("subtest %d: binsearch INT – last element", ++subnum);
    {
        value64 arr[] = {
            value64_createint(10),
            value64_createint(20),
            value64_createint(30)
        };
        value64 key = value64_createint(30);
        test_validate(
            value64_binsearch(key, VALUE64_INT, arr, COUNT(arr)) == 2,
            "30 must be at index 2"
        );
    }

    /* ---------- INT descending (value64_rev_binsearch) ---------- */
    test_sub("subtest %d: rev_binsearch INT – found", ++subnum);
    {
        // массив строго по убыванию
        value64 arr[] = {
            value64_createint(50),
            value64_createint(40),
            value64_createint(30),
            value64_createint(20),
            value64_createint(10)
        };
        value64 key = value64_createint(30);
        test_validate(
            value64_rev_binsearch(key, VALUE64_INT, arr, COUNT(arr)) == 2,
            "30 must be at index 2 (descending)"
        );
    }

    test_sub("subtest %d: rev_binsearch INT – not found", ++subnum);
    {
        value64 arr[] = {
            value64_createint(50),
            value64_createint(40),
            value64_createint(30)
        };
        value64 key = value64_createint(35);
        test_validate(
            value64_rev_binsearch(key, VALUE64_INT, arr, COUNT(arr)) == -1,
            "35 must not be found in descending array"
        );
    }

    /* ---------- ULONG binsearch ---------- */
    test_sub("subtest %d: binsearch ULONG – middle", ++subnum);
    {
        value64 arr[] = {
            value64_createulong(100UL),
            value64_createulong(200UL),
            value64_createulong(300UL)
        };
        test_validate(
            value64_binsearch(value64_createulong(200UL), VALUE64_ULONG, arr, COUNT(arr)) == 1,
            "200UL must be at index 1"
        );
    }

    test_sub("subtest %d: binsearch ULONG – first", ++subnum);
    {
        value64 arr[] = {
            value64_createulong(10UL),
            value64_createulong(20UL),
            value64_createulong(30UL)
        };
        test_validate(
            value64_binsearch(value64_createulong(10UL), VALUE64_ULONG, arr, COUNT(arr)) == 0,
            "10UL must be at index 0"
        );
    }

    test_sub("subtest %d: binsearch ULONG – last", ++subnum);
    {
        value64 arr[] = {
            value64_createulong(1UL),
            value64_createulong(2UL),
            value64_createulong(3UL)
        };
        test_validate(
            value64_binsearch(value64_createulong(3UL), VALUE64_ULONG, arr, COUNT(arr)) == 2,
            "3UL must be at index 2"
        );
    }

    test_sub("subtest %d: binsearch ULONG – not found", ++subnum);
    {
        value64 arr[] = {
            value64_createulong(50UL),
            value64_createulong(60UL)
        };
        test_validate(
            value64_binsearch(value64_createulong(55UL), VALUE64_ULONG, arr, COUNT(arr)) == -1,
            "55UL must not be found, return -1"
        );
    }

    test_sub("subtest %d: binsearch ULONG – empty array", ++subnum);
    {
        test_validate(
            value64_binsearch(value64_createulong(0UL), VALUE64_ULONG, NULL, 0) == -1,
            "empty array binsearch must return -1"
        );
    }

    /* ---------- ULONG reverse binsearch ---------- */
    test_sub("subtest %d: rev_binsearch ULONG – last occurrence", ++subnum);
    {
        value64 arr[] = {
            value64_createulong(5UL),
            value64_createulong(5UL),
            value64_createulong(10UL)
        };
        test_validate(
            value64_rev_binsearch(value64_createulong(5UL), VALUE64_ULONG, arr, COUNT(arr)) == 1,
            "5UL last occurrence must be at index 1"
        );
    }

    test_sub("subtest %d: rev_binsearch ULONG – not found", ++subnum);
    {
        value64 arr[] = {
            value64_createulong(7UL),
            value64_createulong(8UL)
        };
        test_validate(
            value64_rev_binsearch(value64_createulong(0UL), VALUE64_ULONG, arr, COUNT(arr)) == -1,
            "0UL must not be found (reverse), return -1"
        );
    }

    test_sub("subtest %d: rev_binsearch ULONG – empty array", ++subnum);
    {
        test_validate(
            value64_rev_binsearch(value64_createulong(123UL), VALUE64_ULONG, NULL, 0) == -1,
            "empty array rev_binsearch must return -1"
        );
    }

    /* ========== CHAR ascending ========== */
    test_sub("subtest %d: CHAR asc binsearch found", ++subnum);
    {
        value64 arr[] = { value64_createchar('a'), value64_createchar('b'), value64_createchar('c'),
                          value64_createchar('d'), value64_createchar('e') };
        int idx = value64_binsearch(value64_createchar('c'), VALUE64_CHR, arr, 5);
        test_validate(idx == 2, "CHAR asc search 'c': expected idx=2, got %d", idx);
    }

    test_sub("subtest %d: CHAR asc binsearch not found", ++subnum);
    {
        value64 arr[] = { value64_createchar('a'), value64_createchar('b'), value64_createchar('c') };
        int idx = value64_binsearch(value64_createchar('x'), VALUE64_CHR, arr, 3);
        test_validate(idx == -1, "CHAR asc missing 'x': expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHAR asc binsearch first element", ++subnum);
    {
        value64 arr[] = { value64_createchar('A'), value64_createchar('B'), value64_createchar('C') };
        int idx = value64_binsearch(value64_createchar('A'), VALUE64_CHR, arr, 3);
        test_validate(idx == 0, "CHAR asc first: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: CHAR asc binsearch last element", ++subnum);
    {
        value64 arr[] = { value64_createchar('A'), value64_createchar('B'), value64_createchar('C') };
        int idx = value64_binsearch(value64_createchar('C'), VALUE64_CHR, arr, 3);
        test_validate(idx == 2, "CHAR asc last: expected idx=2, got %d", idx);
    }

    test_sub("subtest %d: CHAR asc binsearch with duplicates (any match)", ++subnum);
    {
        value64 arr[] = { value64_createchar('a'), value64_createchar('b'), value64_createchar('b'),
                          value64_createchar('c') };
        int idx = value64_binsearch(value64_createchar('b'), VALUE64_CHR, arr, 4);
        test_validate(idx >= 1 && idx <= 2, "CHAR asc duplicate: idx must be 1 or 2, got %d", idx);
    }

    test_sub("subtest %d: CHAR asc binsearch single element found", ++subnum);
    {
        value64 arr[] = { value64_createchar('M') };
        int idx = value64_binsearch(value64_createchar('M'), VALUE64_CHR, arr, 1);
        test_validate(idx == 0, "CHAR asc single found: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: CHAR asc binsearch single element not found", ++subnum);
    {
        value64 arr[] = { value64_createchar('M') };
        int idx = value64_binsearch(value64_createchar('N'), VALUE64_CHR, arr, 1);
        test_validate(idx == -1, "CHAR asc single missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHAR asc binsearch empty array", ++subnum);
    {
        int idx = value64_binsearch(value64_createchar('A'), VALUE64_CHR, NULL, 0);
        test_validate(idx == -1, "CHAR asc empty: expected -1, got %d", idx);
    }

    /* ========== descending ========== */
    test_sub("subtest %d: CHAR desc binsearch found", ++subnum);
    {
        value64 arr[] = { value64_createchar('e'), value64_createchar('d'), value64_createchar('c'),
                          value64_createchar('b'), value64_createchar('a') };
        int idx = value64_rev_binsearch(value64_createchar('c'), VALUE64_CHR, arr, 5);
        test_validate(idx == 2, "CHAR desc search 'c': expected idx=2, got %d", idx);
    }

    test_sub("subtest %d: CHAR desc binsearch not found", ++subnum);
    {
        value64 arr[] = { value64_createchar('z'), value64_createchar('y'), value64_createchar('x') };
        int idx = value64_rev_binsearch(value64_createchar('m'), VALUE64_CHR, arr, 3);
        test_validate(idx == -1, "CHAR desc missing 'm': expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHAR desc binsearch first element", ++subnum);
    {
        value64 arr[] = { value64_createchar('C'), value64_createchar('B'), value64_createchar('A') };
        int idx = value64_rev_binsearch(value64_createchar('C'), VALUE64_CHR, arr, 3);
        test_validate(idx == 0, "CHAR desc first: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: CHAR desc binsearch last element", ++subnum);
    {
        value64 arr[] = { value64_createchar('C'), value64_createchar('B'), value64_createchar('A') };
        int idx = value64_rev_binsearch(value64_createchar('A'), VALUE64_CHR, arr, 3);
        test_validate(idx == 2, "CHAR desc last: expected idx=2, got %d", idx);
    }

    test_sub("subtest %d: CHAR desc binsearch with duplicates (any match)", ++subnum);
    {
        value64 arr[] = { value64_createchar('d'), value64_createchar('c'), value64_createchar('c'),
                          value64_createchar('a') };
        int idx = value64_rev_binsearch(value64_createchar('c'), VALUE64_CHR, arr, 4);
        test_validate(idx >= 1 && idx <= 2, "CHAR desc duplicate: idx must be 1 or 2, got %d", idx);
    }

    test_sub("subtest %d: CHAR desc binsearch single element found", ++subnum);
    {
        value64 arr[] = { value64_createchar('Z') };
        int idx = value64_rev_binsearch(value64_createchar('Z'), VALUE64_CHR, arr, 1);
        test_validate(idx == 0, "CHAR desc single found: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: CHAR desc binsearch single element not found", ++subnum);
    {
        value64 arr[] = { value64_createchar('Z') };
        int idx = value64_rev_binsearch(value64_createchar('A'), VALUE64_CHR, arr, 1);
        test_validate(idx == -1, "CHAR desc single missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: CHAR desc binsearch empty array", ++subnum);
    {
        int idx = value64_rev_binsearch(value64_createchar('A'), VALUE64_CHR, NULL, 0);
        test_validate(idx == -1, "CHAR desc empty: expected -1, got %d", idx);
    }

    /* ========== BOOL ascending ========== */
    test_sub("subtest %d: BOOL asc binsearch found first element", ++subnum);
    {
        value64 arr[] = { value64_createbool(false), value64_createbool(true) };
        int idx = value64_binsearch(value64_createbool(false), VALUE64_BOOL, arr, 2);
        test_validate(idx == 0, "BOOL asc search false: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: BOOL asc binsearch found last element", ++subnum);
    {
        value64 arr[] = { value64_createbool(false), value64_createbool(true) };
        int idx = value64_binsearch(value64_createbool(true), VALUE64_BOOL, arr, 2);
        test_validate(idx == 1, "BOOL asc search true: expected idx=1, got %d", idx);
    }

    test_sub("subtest %d: BOOL asc binsearch not found (missing value)", ++subnum);
    {
        value64 arr[] = { value64_createbool(false) };
        int idx = value64_binsearch(value64_createbool(true), VALUE64_BOOL, arr, 1);
        test_validate(idx == -1, "BOOL asc search true missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: BOOL asc binsearch with duplicates", ++subnum);
    {
        value64 arr[] = { value64_createbool(false), value64_createbool(false),
                          value64_createbool(true), value64_createbool(true) };
        int idx = value64_binsearch(value64_createbool(false), VALUE64_BOOL, arr, 4);
        // bsearch может вернуть любой из дубликатов, но он должен быть в диапазоне
        test_validate(idx >= 0 && idx <= 1, "BOOL asc duplicate: idx must be 0 or 1, got %d", idx);
    }

    test_sub("subtest %d: BOOL asc binsearch single element found", ++subnum);
    {
        value64 arr[] = { value64_createbool(true) };
        int idx = value64_binsearch(value64_createbool(true), VALUE64_BOOL, arr, 1);
        test_validate(idx == 0, "BOOL asc single found: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: BOOL asc binsearch single element not found", ++subnum);
    {
        value64 arr[] = { value64_createbool(true) };
        int idx = value64_binsearch(value64_createbool(false), VALUE64_BOOL, arr, 1);
        test_validate(idx == -1, "BOOL asc single missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: BOOL asc binsearch empty array", ++subnum);
    {
        int idx = value64_binsearch(value64_createbool(true), VALUE64_BOOL, NULL, 0);
        test_validate(idx == -1, "BOOL asc empty: expected -1, got %d", idx);
    }

    /* ========== BOOL descending (true > false) ========== */
    test_sub("subtest %d: BOOL desc binsearch found first element", ++subnum);
    {
        value64 arr[] = { value64_createbool(true), value64_createbool(false) };
        int idx = value64_rev_binsearch(value64_createbool(true), VALUE64_BOOL, arr, 2);
        test_validate(idx == 0, "BOOL desc search true: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: BOOL desc binsearch found last element", ++subnum);
    {
        value64 arr[] = { value64_createbool(true), value64_createbool(false) };
        int idx = value64_rev_binsearch(value64_createbool(false), VALUE64_BOOL, arr, 2);
        test_validate(idx == 1, "BOOL desc search false: expected idx=1, got %d", idx);
    }

    test_sub("subtest %d: BOOL desc binsearch not found (missing value)", ++subnum);
    {
        value64 arr[] = { value64_createbool(true) };
        int idx = value64_rev_binsearch(value64_createbool(false), VALUE64_BOOL, arr, 1);
        test_validate(idx == -1, "BOOL desc search false missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: BOOL desc binsearch with duplicates", ++subnum);
    {
        value64 arr[] = { value64_createbool(true), value64_createbool(true),
                          value64_createbool(false), value64_createbool(false) };
        int idx = value64_rev_binsearch(value64_createbool(true), VALUE64_BOOL, arr, 4);
        test_validate(idx >= 0 && idx <= 1, "BOOL desc duplicate: idx must be 0 or 1, got %d", idx);
    }

    test_sub("subtest %d: BOOL desc binsearch single element found", ++subnum);
    {
        value64 arr[] = { value64_createbool(false) };
        int idx = value64_rev_binsearch(value64_createbool(false), VALUE64_BOOL, arr, 1);
        test_validate(idx == 0, "BOOL desc single found: expected idx=0, got %d", idx);
    }

    test_sub("subtest %d: BOOL desc binsearch single element not found", ++subnum);
    {
        value64 arr[] = { value64_createbool(false) };
        int idx = value64_rev_binsearch(value64_createbool(true), VALUE64_BOOL, arr, 1);
        test_validate(idx == -1, "BOOL desc single missing: expected -1, got %d", idx);
    }

    test_sub("subtest %d: BOOL desc binsearch empty array", ++subnum);
    {
        int idx = value64_rev_binsearch(value64_createbool(true), VALUE64_BOOL, NULL, 0);
        test_validate(idx == -1, "BOOL desc empty: expected -1, got %d", idx);
    }

    /* ========== NULL array with sz>0 must raise ========== */
    test_sub("subtest %d: BOOL binsearch NULL array with sz>0 raises", ++subnum);
    {
        if (!try()) {
            value64_binsearch(value64_createbool(true), VALUE64_BOOL, NULL, 5);
            test_validate(false, "Should have raised SIGINT for NULL arr with sz>0");
        } else {
            logsimple("Exception correctly raised on NULL arr with sz>0");
        }
    }

    test_sub("subtest %d: BOOL rev_binsearch NULL array with sz>0 raises", ++subnum);
    {
        if (!try()) {
            value64_rev_binsearch(value64_createbool(true), VALUE64_BOOL, NULL, 5);
            test_validate(false, "Should have raised SIGINT for NULL arr with sz>0");
        } else {
            logsimple("Exception correctly raised on NULL arr with sz>0");
        }
    }

    /* ---------- STR ascending ---------- */
    test_sub("subtest %d: binsearch STR – found", ++subnum);
    {
        value64 arr[] = {
            value64_createstr("apple"),
            value64_createstr("banana"),
            value64_createstr("cherry")
        };
        value64 key = value64_createstr("banana");
        test_validatefree(
            value64_binsearch(key, VALUE64_STR, arr, COUNT(arr)) == 1,
            (value64free(key, VALUE64_STR),
             value64free(arr[0], VALUE64_STR),
             value64free(arr[1], VALUE64_STR),
             value64free(arr[2], VALUE64_STR)),
            "'banana' must be at index 1"
        );
        value64free(key, VALUE64_STR);
        for (int i = 0; i < COUNT(arr); i++)
            value64free(arr[i], VALUE64_STR);
    }

    /* ---------- STR descending ---------- */
    test_sub("subtest %d: rev_binsearch STR – found", ++subnum);
    {
        value64 arr[] = {
            value64_createstr("cherry"),
            value64_createstr("banana"),
            value64_createstr("apple")
        };
        value64 key = value64_createstr("banana");
        test_validatefree(
            value64_rev_binsearch(key, VALUE64_STR, arr, COUNT(arr)) == 1,
            (value64free(key, VALUE64_STR),
             value64free(arr[0], VALUE64_STR),
             value64free(arr[1], VALUE64_STR),
             value64free(arr[2], VALUE64_STR)),
            "'banana' must be at index 1 (descending)"
        );
        value64free(key, VALUE64_STR);
        for (int i = 0; i < COUNT(arr); i++)
            value64free(arr[i], VALUE64_STR);
    }

    /* ---------- FS ascending ---------- */
    test_sub("subtest %d: binsearch FS – found", ++subnum);
    {
        fs t1 = fscopy("alpha"), t2 = fscopy("beta"), t3 = fscopy("gamma");
        value64 arr[] = { value64_createfs(&t1), value64_createfs(&t2), value64_createfs(&t3) };
        fsfree(t1); fsfree(t2); fsfree(t3);

        fs key_fs = fscopy("beta");
        value64 key = value64_createfs(&key_fs);
        fsfree(key_fs);

        test_validatefree(
            value64_binsearch(key, VALUE64_FS, arr, COUNT(arr)) == 1,
            (value64free(key, VALUE64_FS),
             value64free(arr[0], VALUE64_FS),
             value64free(arr[1], VALUE64_FS),
             value64free(arr[2], VALUE64_FS)),
            "'beta' must be at index 1 (ascending)"
        );
        value64free(key, VALUE64_FS);
        for (int i = 0; i < COUNT(arr); i++) value64free(arr[i], VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ---------- FS descending ---------- */
    test_sub("subtest %d: rev_binsearch FS – found", ++subnum);
    {
        fs t1 = fscopy("gamma"), t2 = fscopy("beta"), t3 = fscopy("alpha");
        value64 arr[] = { value64_createfs(&t1), value64_createfs(&t2), value64_createfs(&t3) };
        fsfree(t1); fsfree(t2); fsfree(t3);

        fs key_fs = fscopy("beta");
        value64 key = value64_createfs(&key_fs);
        fsfree(key_fs);

        test_validatefree(
            value64_rev_binsearch(key, VALUE64_FS, arr, COUNT(arr)) == 1,
            (value64free(key, VALUE64_FS),
             value64free(arr[0], VALUE64_FS),
             value64free(arr[1], VALUE64_FS),
             value64free(arr[2], VALUE64_FS)),
            "'beta' must be at index 1 (descending)"
        );
        value64free(key, VALUE64_FS);
        for (int i = 0; i < COUNT(arr); i++) value64free(arr[i], VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ---------- empty array ---------- */
    test_sub("subtest %d: binsearch – empty array", ++subnum);
    {
        value64 key = value64_createint(1);
        test_validate(
            value64_binsearch(key, VALUE64_INT, NULL, 0) == -1,
            "empty array must return -1"
        );
        test_validate(
            value64_rev_binsearch(key, VALUE64_INT, NULL, 0) == -1,
            "empty array must return -1 (reverse)"
        );
    }

    /* ---------- unsupported type (must raise error) ----------
    test_sub("subtest %d: binsearch – unsupported type", ++subnum);
    {
        value64 key = value64_createint(1);
        if (!try()) {
            value64_binsearch(key, VALUE64_UNKNOWN, NULL, 0);
            test_validate(false, "Unsupported type must raise error, but didn't");
        } else {
            test_validate(true, "Unsupported type correctly raised error");
        }
    }*/

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_(rev_)sort -----------------------------

static TestStatus
tf_sort(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- 1. INT ascending ---------- */
    test_sub("subtest %d: sort INT asc", ++subnum);
    {
        value64 arr[] = {
            value64_createint(30),
            value64_createint(10),
            value64_createint(20)
        };
        value64_sort(VALUE64_INT, arr, COUNT(arr));

        test_validate(
            arr[0].ival == 10,
            "arr[0] must be 10, got %d", arr[0].ival
        );
        test_validate(
            arr[1].ival == 20,
            "arr[1] must be 20, got %d", arr[1].ival
        );
        test_validate(
            arr[2].ival == 30,
            "arr[2] must be 30, got %d", arr[2].ival
        );
    }

    /* 2. INT descending */
    test_sub("subtest %d: sort INT desc", ++subnum);
    {
        value64 arr[] = {
            value64_createint(10),
            value64_createint(30),
            value64_createint(20)
        };
        value64_revsort(VALUE64_INT, arr, COUNT(arr));

        test_validate(
            arr[0].ival == 30,
            "arr[0] must be 30, got %d", arr[0].ival
        );
        test_validate(
            arr[1].ival == 20,
            "arr[1] must be 20, got %d", arr[1].ival
        );
        test_validate(
            arr[2].ival == 10,
            "arr[2] must be 10, got %d", arr[2].ival
        );
    }

    /* ---------- ULONG ascending ---------- */
    test_sub("subtest %d: sort ULONG asc", ++subnum);
    {
        value64 arr[] = {
            value64_createulong(30),
            value64_createulong(10),
            value64_createulong(20)
        };
        value64_sort(VALUE64_ULONG, arr, COUNT(arr));

        test_validate(value64_ulong(arr[0]) == 10,
            "arr[0] must be 10, got %lu", value64_ulong(arr[0]));
        test_validate(value64_ulong(arr[1]) == 20,
            "arr[1] must be 20, got %lu", value64_ulong(arr[1]));
        test_validate(value64_ulong(arr[2]) == 30,
            "arr[2] must be 30, got %lu", value64_ulong(arr[2]));
    }

    /* ---------- ULONG descending ---------- */
    test_sub("subtest %d: sort ULONG desc", ++subnum);
    {
        value64 arr[] = {
            value64_createulong(10),
            value64_createulong(30),
            value64_createulong(20)
        };
        value64_revsort(VALUE64_ULONG, arr, COUNT(arr));

        test_validate(value64_ulong(arr[0]) == 30,
            "arr[0] must be 30, got %lu", value64_ulong(arr[0]));
        test_validate(value64_ulong(arr[1]) == 20,
            "arr[1] must be 20, got %lu", value64_ulong(arr[1]));
        test_validate(value64_ulong(arr[2]) == 10,
            "arr[2] must be 10, got %lu", value64_ulong(arr[2]));
    }

    /* ========== CHAR ascending ========== */
    test_sub("subtest %d: CHAR sort asc basic", ++subnum);
    {
        value64 arr[] = { value64_createchar('z'), value64_createchar('a'), value64_createchar('m') };
        value64_sort(VALUE64_CHR, arr, 3);
        test_validate(arr[0].cval == 'a' && arr[1].cval == 'm' && arr[2].cval == 'z',
                      "CHAR asc: expected a,m,z got %c,%c,%c",
                      arr[0].cval, arr[1].cval, arr[2].cval);
    }

    test_sub("subtest %d: CHAR sort asc with duplicates", ++subnum);
    {
        value64 arr[] = { value64_createchar('b'), value64_createchar('a'), value64_createchar('b'),
                          value64_createchar('c'), value64_createchar('a') };
        value64_sort(VALUE64_CHR, arr, 5);
        // после сортировки: a,a,b,b,c
        test_validate(arr[0].cval == 'a' && arr[1].cval == 'a' &&
                      arr[2].cval == 'b' && arr[3].cval == 'b' &&
                      arr[4].cval == 'c',
                      "CHAR asc duplicates: expected a,a,b,b,c");
    }

    test_sub("subtest %d: CHAR sort asc single element", ++subnum);
    {
        value64 arr[] = { value64_createchar('X') };
        value64_sort(VALUE64_CHR, arr, 1);
        test_validate(arr[0].cval == 'X', "CHAR asc single: must stay 'X', got '%c'", arr[0].cval);
    }

    test_sub("subtest %d: CHAR sort asc empty array", ++subnum);
    {
        // сортировка пустого массива не должна падать
        value64_sort(VALUE64_CHR, NULL, 0);
        test_validate(true, "CHAR asc empty: must not crash");
    }

    test_sub("subtest %d: CHAR sort asc already sorted", ++subnum);
    {
        value64 arr[] = { value64_createchar('A'), value64_createchar('B'), value64_createchar('C') };
        value64_sort(VALUE64_CHR, arr, 3);
        test_validate(arr[0].cval == 'A' && arr[1].cval == 'B' && arr[2].cval == 'C',
                      "CHAR asc already sorted: must stay A,B,C");
    }

    test_sub("subtest %d: CHAR sort asc reversed initial", ++subnum);
    {
        value64 arr[] = { value64_createchar('d'), value64_createchar('c'), value64_createchar('b'),
                          value64_createchar('a') };
        value64_sort(VALUE64_CHR, arr, 4);
        test_validate(arr[0].cval == 'a' && arr[1].cval == 'b' &&
                      arr[2].cval == 'c' && arr[3].cval == 'd',
                      "CHAR asc reversed: expected a,b,c,d");
    }

    /* ========== CHAR descending ========== */
    test_sub("subtest %d: CHAR sort desc basic", ++subnum);
    {
        value64 arr[] = { value64_createchar('a'), value64_createchar('z'), value64_createchar('m') };
        value64_revsort(VALUE64_CHR, arr, 3);
        test_validate(arr[0].cval == 'z' && arr[1].cval == 'm' && arr[2].cval == 'a',
                      "CHAR desc: expected z,m,a got %c,%c,%c",
                      arr[0].cval, arr[1].cval, arr[2].cval);
    }

    test_sub("subtest %d: CHAR sort desc with duplicates", ++subnum);
    {
        value64 arr[] = { value64_createchar('c'), value64_createchar('a'), value64_createchar('c'),
                          value64_createchar('b') };
        value64_revsort(VALUE64_CHR, arr, 4);
        // после сортировки: c,c,b,a
        test_validate(arr[0].cval == 'c' && arr[1].cval == 'c' &&
                      arr[2].cval == 'b' && arr[3].cval == 'a',
                      "CHAR desc duplicates: expected c,c,b,a");
    }

    test_sub("subtest %d: CHAR sort desc single element", ++subnum);
    {
        value64 arr[] = { value64_createchar('Y') };
        value64_revsort(VALUE64_CHR, arr, 1);
        test_validate(arr[0].cval == 'Y', "CHAR desc single: must stay 'Y', got '%c'", arr[0].cval);
    }

    test_sub("subtest %d: CHAR sort desc empty array", ++subnum);
    {
        value64_revsort(VALUE64_CHR, NULL, 0);
        test_validate(true, "CHAR desc empty: must not crash");
    }

    test_sub("subtest %d: CHAR sort desc already sorted", ++subnum);
    {
        value64 arr[] = { value64_createchar('C'), value64_createchar('B'), value64_createchar('A') };
        value64_revsort(VALUE64_CHR, arr, 3);
        test_validate(arr[0].cval == 'C' && arr[1].cval == 'B' && arr[2].cval == 'A',
                      "CHAR desc already sorted: must stay C,B,A");
    }

    test_sub("subtest %d: CHAR sort desc reversed initial", ++subnum);
    {
        value64 arr[] = { value64_createchar('a'), value64_createchar('b'), value64_createchar('c') };
        value64_revsort(VALUE64_CHR, arr, 3);
        test_validate(arr[0].cval == 'c' && arr[1].cval == 'b' && arr[2].cval == 'a',
                      "CHAR desc reversed: expected c,b,a");
    }

    /* ========== BOOL ascending ========== */
    test_sub("subtest %d: BOOL sort asc basic", ++subnum);
    {
        value64 arr[] = { value64_createbool(true), value64_createbool(false), value64_createbool(true) };
        value64_sort(VALUE64_BOOL, arr, 3);
        // после сортировки: false, true, true
        test_validate(arr[0].bval == false && arr[1].bval == true && arr[2].bval == true,
                      "BOOL asc: expected false,true,true got %s,%s,%s",
                      arr[0].bval ? "true" : "false",
                      arr[1].bval ? "true" : "false",
                      arr[2].bval ? "true" : "false");
    }

    test_sub("subtest %d: BOOL sort asc all false", ++subnum);
    {
        value64 arr[] = { value64_createbool(false), value64_createbool(false), value64_createbool(false) };
        value64_sort(VALUE64_BOOL, arr, 3);
        test_validate(arr[0].bval == false && arr[1].bval == false && arr[2].bval == false,
                      "BOOL asc all false: must stay false,false,false");
    }

    test_sub("subtest %d: BOOL sort asc all true", ++subnum);
    {
        value64 arr[] = { value64_createbool(true), value64_createbool(true), value64_createbool(true) };
        value64_sort(VALUE64_BOOL, arr, 3);
        test_validate(arr[0].bval == true && arr[1].bval == true && arr[2].bval == true,
                      "BOOL asc all true: must stay true,true,true");
    }

    test_sub("subtest %d: BOOL sort asc single element", ++subnum);
    {
        value64 arr[] = { value64_createbool(false) };
        value64_sort(VALUE64_BOOL, arr, 1);
        test_validate(arr[0].bval == false, "BOOL asc single: must stay false");
    }

    test_sub("subtest %d: BOOL sort asc empty array", ++subnum);
    {
        value64_sort(VALUE64_BOOL, NULL, 0); // не должно упасть
        test_validate(true, "BOOL asc empty: must not crash");
    }

    test_sub("subtest %d: BOOL sort asc already sorted", ++subnum);
    {
        value64 arr[] = { value64_createbool(false), value64_createbool(false),
                          value64_createbool(true), value64_createbool(true) };
        value64_sort(VALUE64_BOOL, arr, 4);
        test_validate(arr[0].bval == false && arr[1].bval == false &&
                      arr[2].bval == true && arr[3].bval == true,
                      "BOOL asc already sorted: must stay false,false,true,true");
    }

    test_sub("subtest %d: BOOL sort asc reversed initial", ++subnum);
    {
        value64 arr[] = { value64_createbool(true), value64_createbool(true),
                          value64_createbool(false), value64_createbool(false) };
        value64_sort(VALUE64_BOOL, arr, 4);
        test_validate(arr[0].bval == false && arr[1].bval == false &&
                      arr[2].bval == true && arr[3].bval == true,
                      "BOOL asc reversed: expected false,false,true,true");
    }

    /* ========== BOOL descending ========== */
    test_sub("subtest %d: BOOL sort desc basic", ++subnum);
    {
        value64 arr[] = { value64_createbool(false), value64_createbool(true), value64_createbool(false) };
        value64_revsort(VALUE64_BOOL, arr, 3);
        // после сортировки: true, false, false
        test_validate(arr[0].bval == true && arr[1].bval == false && arr[2].bval == false,
                      "BOOL desc: expected true,false,false got %s,%s,%s",
                      arr[0].bval ? "true" : "false",
                      arr[1].bval ? "true" : "false",
                      arr[2].bval ? "true" : "false");
    }

    test_sub("subtest %d: BOOL sort desc all false", ++subnum);
    {
        value64 arr[] = { value64_createbool(false), value64_createbool(false), value64_createbool(false) };
        value64_revsort(VALUE64_BOOL, arr, 3);
        test_validate(arr[0].bval == false && arr[1].bval == false && arr[2].bval == false,
                      "BOOL desc all false: must stay false,false,false");
    }

    test_sub("subtest %d: BOOL sort desc all true", ++subnum);
    {
        value64 arr[] = { value64_createbool(true), value64_createbool(true), value64_createbool(true) };
        value64_revsort(VALUE64_BOOL, arr, 3);
        test_validate(arr[0].bval == true && arr[1].bval == true && arr[2].bval == true,
                      "BOOL desc all true: must stay true,true,true");
    }

    test_sub("subtest %d: BOOL sort desc single element", ++subnum);
    {
        value64 arr[] = { value64_createbool(true) };
        value64_revsort(VALUE64_BOOL, arr, 1);
        test_validate(arr[0].bval == true, "BOOL desc single: must stay true");
    }

    test_sub("subtest %d: BOOL sort desc empty array", ++subnum);
    {
        value64_revsort(VALUE64_BOOL, NULL, 0);
        test_validate(true, "BOOL desc empty: must not crash");
    }

    test_sub("subtest %d: BOOL sort desc already sorted", ++subnum);
    {
        value64 arr[] = { value64_createbool(true), value64_createbool(true),
                          value64_createbool(false), value64_createbool(false) };
        value64_revsort(VALUE64_BOOL, arr, 4);
        test_validate(arr[0].bval == true && arr[1].bval == true &&
                      arr[2].bval == false && arr[3].bval == false,
                      "BOOL desc already sorted: must stay true,true,false,false");
    }

    test_sub("subtest %d: BOOL sort desc reversed initial", ++subnum);
    {
        value64 arr[] = { value64_createbool(false), value64_createbool(false),
                          value64_createbool(true), value64_createbool(true) };
        value64_revsort(VALUE64_BOOL, arr, 4);
        test_validate(arr[0].bval == true && arr[1].bval == true &&
                      arr[2].bval == false && arr[3].bval == false,
                      "BOOL desc reversed: expected true,true,false,false");
    }

    /* ---------- 3. STR ascending ---------- */
    test_sub("subtest %d: sort STR asc", ++subnum);
    {
        value64 arr[] = {
            value64_createstr("cherry"),
            value64_createstr("apple"),
            value64_createstr("banana")
        };
        value64_sort(VALUE64_STR, arr, COUNT(arr));

        test_validatefree(
            strcmp(value64_str(arr[0]), "apple") == 0 &&
            strcmp(value64_str(arr[1]), "banana") == 0 &&
            strcmp(value64_str(arr[2]), "cherry") == 0,
            (value64free(arr[0], VALUE64_STR),
             value64free(arr[1], VALUE64_STR),
             value64free(arr[2], VALUE64_STR)),
            "STR asc order mismatch: got [%s, %s, %s]",
            value64_str(arr[0]), value64_str(arr[1]), value64_str(arr[2])
        );
        for (int i = 0; i < COUNT(arr); i++)
            value64free(arr[i], VALUE64_STR);
    }

    /* 4. STR descending */
    test_sub("subtest %d: sort STR desc", ++subnum);
    {
        value64 arr[] = {
            value64_createstr("apple"),
            value64_createstr("cherry"),
            value64_createstr("banana")
        };
        value64_revsort(VALUE64_STR, arr, COUNT(arr));

        test_validatefree(
            strcmp(value64_str(arr[0]), "cherry") == 0 &&
            strcmp(value64_str(arr[1]), "banana") == 0 &&
            strcmp(value64_str(arr[2]), "apple") == 0,
            (value64free(arr[0], VALUE64_STR),
             value64free(arr[1], VALUE64_STR),
             value64free(arr[2], VALUE64_STR)),
            "STR desc order mismatch: got [%s, %s, %s]",
            value64_str(arr[0]), value64_str(arr[1]), value64_str(arr[2])
        );
        for (int i = 0; i < COUNT(arr); i++)
            value64free(arr[i], VALUE64_STR);
    }

    /* ---------- 5. FS ascending ---------- */
    test_sub("subtest %d: sort FS asc", ++subnum);
    {
        fs t1 = fscopy("gamma"), t2 = fscopy("alpha"), t3 = fscopy("beta");
        value64 arr[] = { value64_createfs(&t1), value64_createfs(&t2), value64_createfs(&t3) };
        fsfree(t1); fsfree(t2); fsfree(t3);

        value64_sort(VALUE64_FS, arr, COUNT(arr));

        test_validatefree(
            strcmp(fs_str(value64_fs(arr[0])), "alpha") == 0 &&
            strcmp(fs_str(value64_fs(arr[1])), "beta") == 0 &&
            strcmp(fs_str(value64_fs(arr[2])), "gamma") == 0,
            (value64free(arr[0], VALUE64_FS),
             value64free(arr[1], VALUE64_FS),
             value64free(arr[2], VALUE64_FS)),
            "FS asc order mismatch"
        );
        for (int i = 0; i < COUNT(arr); i++)
            value64free(arr[i], VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 6. FS descending */
    test_sub("subtest %d: sort FS desc", ++subnum);
    {
        fs t1 = fscopy("alpha"), t2 = fscopy("gamma"), t3 = fscopy("beta");
        value64 arr[] = { value64_createfs(&t1), value64_createfs(&t2), value64_createfs(&t3) };
        fsfree(t1); fsfree(t2); fsfree(t3);

        value64_revsort(VALUE64_FS, arr, COUNT(arr));

        test_validatefree(
            strcmp(fs_str(value64_fs(arr[0])), "gamma") == 0 &&
            strcmp(fs_str(value64_fs(arr[1])), "beta") == 0 &&
            strcmp(fs_str(value64_fs(arr[2])), "alpha") == 0,
            (value64free(arr[0], VALUE64_FS),
             value64free(arr[1], VALUE64_FS),
             value64free(arr[2], VALUE64_FS)),
            "FS desc order mismatch"
        );
        for (int i = 0; i < COUNT(arr); i++)
            value64free(arr[i], VALUE64_FS);
        fs_alloc_check(true);
    }

    /* ---------- 7. empty array (must not crash) ---------- */
    test_sub("subtest %d: sort empty array", ++subnum);
    {
        value64_sort(VALUE64_INT, NULL, 0);
        value64_revsort(VALUE64_INT, NULL, 0);
        test_validate(
            true,
            "empty sort must not crash"
        );
    }
    /* ---------- 8. large array (1000 INT) ---------- */
    test_sub("subtest %d: sort large INT array (1000 elements)", ++subnum);
    {
        enum { N = 1000, MAX = 100000 };
        value64 arr[N];
        // заполняем случайными числами
        for (int i = 0; i < N; i++)
            arr[i] = value64_createint(rand() % MAX);

        value64_sort(VALUE64_INT, arr, N);

        // проверяем неубывание
        for (int i = 1; i < N; i++)
            test_validate(
                arr[i - 1].ival <= arr[i].ival,
                "%d elem = %d must be <= that %d elem = %d", i - 1, arr[i - 1].ival, i, arr[i].ival
            );

        // теперь тестируем обратную сортировку: перемешаем заново
        for (int i = 0; i < N; i++)
            arr[i] = value64_createint(rand() % MAX);

        value64_revsort(VALUE64_INT, arr, N);

        for (int i = 1; i < N; i++)
            test_validate(
                arr[i - 1].ival >= arr[i].ival,
                "%d elem = %d must be >= that %d elem = %d", i - 1, arr[i - 1].ival, i, arr[i].ival
            );
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_tofile -----------------------------
static TestStatus
tf_fsave(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: save INT", ++subnum);
    {
        value64     v = value64_createint(42);
        const char  fname[] = "res/values64/int_save.dat";
        FILE       *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open file for 'w' %s", fname);
        int         written = value64_tofile(f, v, VALUE64_INT, true);
        fclose(f);
        logmsg("Saved INT to '%s', written=%d", fname, written);
    }

    test_sub("subtest %d: save LONG", ++subnum);
    {
        value64     v = value64_createlong(1234567890123L);
        const char  fname[] = "res/values64/long_save.dat";
        FILE       *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open file for 'w' %s", fname);
        int         written = value64_tofile(f, v, VALUE64_LONG, true);
        fclose(f);
        logmsg("Saved LONG to '%s', written=%d", fname, written);
    }

    test_sub("subtest %d: save DBL", ++subnum);
    {
        value64     v = value64_createdbl(3.14159265358979);
        const char  fname[] = "res/values64/dbl_save.dat";
        FILE       *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open file for 'w' %s", fname);
        int         written = value64_tofile(f, v, VALUE64_DBL, true);
        fclose(f);
        logmsg("Saved DBL to '%s', written=%d", fname, written);
    }

    test_sub("subtest %d: save STR", ++subnum);
    {
        value64     v = value64_createstr("hello world");
        const char  fname[] = "res/values64/str_save.dat";
        FILE       *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open file for 'w' %s", fname);
        int         written = value64_tofile(f, v, VALUE64_STR, true);
        fclose(f);
        value64freestr(v);
        logmsg("Saved STR to '%s', written=%d", fname, written);
    }

    test_sub("subtest %d: save STR empty", ++subnum);
    {
        value64     v = value64_createstr("");
        const char  fname[] = "res/values64/str_empty_save.dat";
        FILE       *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open file for 'w' %s", fname);
        int         written = value64_tofile(f, v, VALUE64_STR, true);
        fclose(f);
        value64freestr(v);
        logmsg("Saved empty STR to '%s', written=%d", fname, written);
    }

    test_sub("subtest %d: save FS", ++subnum);
    {
        fs          tmp = fscopy("fs-data");
        value64     v = value64_createfs(&tmp);
        fsfree(tmp);
        const char  fname[] = "res/values64/fs_save.dat";
        FILE       *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open file for 'w' %s", fname);
        int         written = value64_tofile(f, v, VALUE64_FS, true);
        fclose(f);
        value64free(v, VALUE64_FS);
        logmsg("Saved FS to '%s', written=%d", fname, written);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: save PTR", ++subnum);
    {
        int         x = 77;
        value64     v = value64_createptr(&x);
        const char  fname[] = "res/values64/ptr_save.dat";
        FILE       *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open file for 'w' %s", fname);
        int         written = value64_tofile(f, v, VALUE64_PTR, true);
        fclose(f);
        logmsg("Saved PTR to '%s', written=%d", fname, written);
    }

    test_sub("subtest %d: save without type info (INT)", ++subnum);
    {
        value64     v = value64_createint(99);
        const char  fname[] = "res/values64/int_notype.dat";
        FILE       *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open file for 'w' %s", fname);
        int         written = value64_tofile(f, v, VALUE64_INT, false);
        fclose(f);
        logmsg("Saved INT (no type) to '%s', written=%d", fname, written);
    }

    return logret(TEST_MANUAL, "PLEASE CHECK");
}

// ------------------------- TEST value64_tofile/fload -----------------------------
static TestStatus
tf_fsave_fload(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT */
    test_sub("subtest %d: INT save/load", ++subnum);
    {
        const char fname[] = "res/values64/save_int.dat";
        value64 orig = value64_createint(42);

        FILE *f = fopen(fname, "w");
        if (!f)
            return userraise(TEST_FAILED, ERR_UNABLE_OPEN_FILE_WRITE, "Cannot open %s for writing", fname);
        value64_tofile(f, orig, VALUE64_INT, true);
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return userraise(TEST_FAILED, ERR_UNABLE_OPEN_FILE_READ, "Cannot open %s for reading", fname);
        value64 loaded;
        test_validate(
            value64_loadfile(f, &loaded, VALUE64_UNKNOWN, true, NULL) == 1,
            "INT load must return 1"
        );
        fclose(f);

        test_validate(
            value64_compare(orig, loaded, VALUE64_INT) == 0,
            "INT save/load mismatch: original %d, loaded %d", orig.ival, loaded.ival
        );
        fs_alloc_check(true);
    }

    /* ---------- ULONG save/load ---------- */
    test_sub("subtest %d: ULONG save/load", ++subnum);
    {
        const char fname[] = "res/values64/save_ulong.dat";
        value64 orig = value64_createulong(1234567890UL);

        FILE *f = fopen(fname, "w");
        if (!f)
            return userraise(TEST_FAILED, ERR_UNABLE_OPEN_FILE_WRITE, "Cannot open %s for writing", fname);
        value64_tofile(f, orig, VALUE64_ULONG, true);
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return userraise(TEST_FAILED, ERR_UNABLE_OPEN_FILE_READ, "Cannot open %s for reading", fname);
        value64 loaded;
        test_validate(
            value64_loadfile(f, &loaded, VALUE64_UNKNOWN, true, NULL) == 1,
            "ULONG load must return 1"
        );
        fclose(f);

        test_validate(
            value64_compare(orig, loaded, VALUE64_ULONG) == 0,
            "ULONG save/load mismatch: original %lu, loaded %lu",
            value64_ulong(orig), value64_ulong(loaded)
        );
        fs_alloc_check(true);
    }

    /* 2. LONG */
    test_sub("subtest %d: LONG save/load", ++subnum);
    {
        const char fname[] = "res/values64/save_long.dat";
        value64 orig = value64_createlong(1234567890123L);

        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        value64_tofile(f, orig, VALUE64_LONG, true);
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);
        value64 loaded;
        test_validate(
            value64_loadfile(f, &loaded, VALUE64_UNKNOWN, true, NULL) == 1,
            "LONG load must return 1"
        );
        fclose(f);

        test_validate(
            value64_compare(orig, loaded, VALUE64_LONG) == 0,
            "LONG save/load mismatch: original %ld, loaded %ld", orig.lval, loaded.lval
        );
        fs_alloc_check(true);
    }

    /* 1. save/load simple char */
    test_sub("subtest %d: CHAR save/load 'A'", ++subnum);
    {
        value64 orig = value64_createchar('A');
        const char *fname = "res/values64/char_A.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        int written = value64_tofile(fp, orig, VALUE64_CHR, true);
        fclose(fp);
        test_validate(written > 0, "CHAR save failed");

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_CHR, true, NULL) > 0 &&
            value64_char(loaded) == 'A',
            fclose(fp),
            "CHAR load: expected 'A', got '%c'", value64_char(loaded)
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 2. save/load special chars */
    test_sub("subtest %d: CHAR save/load special chars", ++subnum);
    {
        const char chars[] = {' ', '\n', '\t', '\\', '\'', '\"'};
        for (size_t i = 0; i < COUNT(chars); i++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "res/values64/char_%02X.dat", (unsigned char)chars[i]);
            value64 orig = value64_createchar(chars[i]);
            FILE *fp = fopen(buf, "w");
            test_validate(fp != NULL, "Cannot open file for writing");
            value64_tofile(fp, orig, VALUE64_CHR, true);
            fclose(fp);

            fp = fopen(buf, "r");
            test_validate(fp != NULL, "Cannot open file for reading");
            value64 loaded;
            test_validatefree(
                value64_loadfile(fp, &loaded, VALUE64_CHR, true, NULL) > 0 &&
                value64_char(loaded) == chars[i],
                fclose(fp),
                "CHAR load special: expected '%c' (0x%02X), got '%c'",
                chars[i], (unsigned char)chars[i], value64_char(loaded)
            );
            fclose(fp);
            fs_alloc_check(true);
        }
    }

    /* 3. save/load with type info false (plain value) */
    test_sub("subtest %d: CHAR save/load without typeinfo", ++subnum);
    {
        value64 orig = value64_createchar('Z');
        const char *fname = "res/values64/char_Z_notype.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        value64_tofile(fp, orig, VALUE64_CHR, false);
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_CHR, false, NULL) > 0 &&
            value64_char(loaded) == 'Z',
            fclose(fp),
            "CHAR load notype: expected 'Z', got '%c'", value64_char(loaded)
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 4. NULL char (should work) */
    test_sub("subtest %d: CHAR save/load null char", ++subnum);
    {
        value64 orig = value64_createchar('\0');
        const char *fname = "res/values64/char_null.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        value64_tofile(fp, orig, VALUE64_CHR, true);
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_CHR, true, NULL) > 0 &&
            value64_char(loaded) == '\0',
            fclose(fp),
            "CHAR load null: expected '\\0', got '%c'", value64_char(loaded)
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* ====================== BOOL ===================== */
    /* 1. save/load true with type info */
    test_sub("subtest %d: BOOL save/load true", ++subnum);
    {
        value64 orig = value64_createbool(true);
        const char *fname = "res/values64/bool_true.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        value64_tofile(fp, orig, VALUE64_BOOL, true);
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_BOOL, true, NULL) > 0 &&
            value64_bool(loaded) == true,
            fclose(fp),
            "BOOL load true: expected true, got %s",
            value64_bool(loaded) ? "true" : "false"
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 2. save/load false with type info */
    test_sub("subtest %d: BOOL save/load false", ++subnum);
    {
        value64 orig = value64_createbool(false);
        const char *fname = "res/values64/bool_false.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        value64_tofile(fp, orig, VALUE64_BOOL, true);
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_BOOL, true, NULL) > 0 &&
            value64_bool(loaded) == false,
            fclose(fp),
            "BOOL load false: expected false, got %s",
            value64_bool(loaded) ? "true" : "false"
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 3. save/load without type info (savetypeinfo = false) */
    test_sub("subtest %d: BOOL save/load without typeinfo", ++subnum);
    {
        value64 orig = value64_createbool(true);
        const char *fname = "res/values64/bool_true_notype.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        value64_tofile(fp, orig, VALUE64_BOOL, false);
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_BOOL, false, NULL) > 0 &&
            value64_bool(loaded) == true,
            fclose(fp),
            "BOOL load notype: expected true, got %s",
            value64_bool(loaded) ? "true" : "false"
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 4. load from manually written file with "true" */
    test_sub("subtest %d: BOOL load from manual 'true' file", ++subnum);
    {
        const char *fname = "res/values64/bool_manual_true.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        fprintf(fp, "VALUE64(BOOL):\"true\"");
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_BOOL, true, NULL) > 0 &&
            value64_bool(loaded) == true,
            fclose(fp),
            "BOOL load manual 'true': expected true"
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 5. load from manually written file with "FALSE" (case insensitive) */
    test_sub("subtest %d: BOOL load from manual 'FALSE' file", ++subnum);
    {
        const char *fname = "res/values64/bool_manual_false.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        fprintf(fp, "VALUE64(BOOL):\"FALSE\"");
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_BOOL, true, NULL) > 0 &&
            value64_bool(loaded) == false,
            fclose(fp),
            "BOOL load manual 'FALSE': expected false"
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 6. load from manually written file with invalid string */
    test_sub("subtest %d: BOOL load invalid string fails", ++subnum);
    {
        const char *fname = "res/values64/bool_invalid.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        fprintf(fp, "VALUE64(BOOL):\"invalid\"");
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_BOOL, true, NULL) <= 0,
            fclose(fp),
            "BOOL load invalid string must fail"
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 7. load from empty quoted string (should fail) */
    test_sub("subtest %d: BOOL load empty string fails", ++subnum);
    {
        const char *fname = "res/values64/bool_empty.dat";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Cannot open file for writing");
        fprintf(fp, "VALUE64(BOOL):\"\"");
        fclose(fp);

        fp = fopen(fname, "r");
        test_validate(fp != NULL, "Cannot open file for reading");
        value64 loaded;
        test_validatefree(
            value64_loadfile(fp, &loaded, VALUE64_BOOL, true, NULL) <= 0,
            fclose(fp),
            "BOOL load empty string must fail"
        );
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 3. DBL */
    test_sub("subtest %d: DBL save/load", ++subnum);
    {
        const char fname[] = "res/values64/save_dbl.dat";
        value64 orig = value64_createdbl(3.14159265358979);

        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        value64_tofile(f, orig, VALUE64_DBL, true);
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);
        value64 loaded;
        test_validate(
            value64_loadfile(f, &loaded, VALUE64_UNKNOWN, true, NULL) == 1,
            "DBL load must return 1"
        );
        fclose(f);

        test_validate(
            value64_compare(orig, loaded, VALUE64_DBL) == 0,
            "DBL save/load mismatch: original %g, loaded %g", orig.dval, loaded.dval
        );
        fs_alloc_check(true);
    }


    /* 4. STR (обычная) */
    test_sub("subtest %d: STR save/load normal", ++subnum);
    {
        const char fname[] = "res/values64/save_str.dat";
        value64 orig = value64_createstr("hello world");

        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        value64_tofile(f, orig, VALUE64_STR, true);
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);
        value64 loaded;
        test_validate(
            value64_loadfile(f, &loaded, VALUE64_UNKNOWN, true, NULL) == 1,
            "STR normal load must return 1"
        );
        fclose(f);

        test_validate(
            value64_compare(orig, loaded, VALUE64_STR) == 0,
            "STR normal save/load mismatch: original '%s', loaded '%s'",
            value64_str(orig), value64_str(loaded)
        );

        value64free(orig, VALUE64_STR);
        value64free(loaded, VALUE64_STR);
        fs_alloc_check(true);
    }

    /* 5. STR (пустая) */
    test_sub("subtest %d: STR save/load empty", ++subnum);
    {
        const char fname[] = "res/values64/save_str_empty.dat";
        value64 orig = value64_createstr("");

        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        value64_tofile(f, orig, VALUE64_STR, true);
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);
        value64 loaded;
        test_validate(
            value64_loadfile(f, &loaded, VALUE64_UNKNOWN, true, NULL) == 1,
            "STR empty load must return 1"
        );
        fclose(f);

        test_validate(
            value64_compare(orig, loaded, VALUE64_STR) == 0,
            "STR empty save/load mismatch: original '', loaded '%s'",
            value64_str(loaded)
        );

        value64free(orig, VALUE64_STR);
        value64free(loaded, VALUE64_STR);
        fs_alloc_check(true);
    }

    /* 6. FS */
    test_sub("subtest %d: FS save/load", ++subnum);
    {
        const char fname[] = "res/values64/save_fs.dat";
        fs tmp = fscopy("fs-data");
        value64 orig = value64_createfs(&tmp);
        fsfree(tmp);

        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        value64_tofile(f, orig, VALUE64_FS, true);
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);
        value64 loaded;
        test_validate(
            value64_loadfile(f, &loaded, VALUE64_UNKNOWN, true, NULL) == 1,
            "FS load must return 1"
        );
        fclose(f);

        test_validatefree(
            value64_compare(orig, loaded, VALUE64_FS) == 0,
            (value64free(orig, VALUE64_FS), value64free(loaded, VALUE64_FS)),
            "FS save/load mismatch"
        );
        value64free(orig, VALUE64_FS);
        value64free(loaded, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 7. Сохранение без информации о типе (INT) */
    test_sub("subtest %d: INT save/load without type info", ++subnum);
    {
        const char fname[] = "res/values64/save_int_notype.dat";
        value64 orig = value64_createint(99);

        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        value64_tofile(f, orig, VALUE64_INT, false);
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);
        value64 loaded;
        test_validate(
            value64_loadfile(f, &loaded, VALUE64_INT, false, NULL) == 1,
            "INT no-type load must return 1"
        );
        fclose(f);

        test_validate(
            value64_compare(orig, loaded, VALUE64_INT) == 0,
            "INT no-type save/load mismatch: original %d, loaded %d", orig.ival, loaded.ival
        );
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_tostr (savetypeinfo=true) -------------------------
static TestStatus
tf_tostr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== INT ========== */
    test_sub("subtest %d: INT to string", ++subnum);
    {
        value64 v = LITERAL64_INT(42);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_INT, true);
        test_validatefree(
            written == 17 &&
            strcmp(fs_str(&buf), "VALUE64(INT):\"42\"") == 0,
            fsfree(buf),
            "INT: expected 'VALUE64(INT):\"42\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    /* ========== LONG ========== */
    test_sub("subtest %d: LONG to string", ++subnum);
    {
        value64 v = LITERAL64_LONG(123456789L);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_LONG, true);
        test_validatefree(
            written == 24 &&
            strcmp(fs_str(&buf), "VALUE64(LNG):\"123456789\"") == 0,
            fsfree(buf),
            "LONG: expected 'VALUE64(LNG):\"123456789\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    /* ========== ULONG ========== */
    test_sub("subtest %d: ULONG to string with typeinfo", ++subnum);
    {
        value64 v = value64_createulong(1234567890UL);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_ULONG, true);
        test_validatefree(
            written == 27 &&
            strcmp(fs_str(&buf), "VALUE64(ULONG):\"1234567890\"") == 0,
            fsfree(buf),
            "ULONG with typeinfo: expected 'VALUE64(ULONG):\"1234567890\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    test_sub("subtest %d: ULONG to string without typeinfo", ++subnum);
    {
        value64 v = value64_createulong(1234567890UL);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_ULONG, false);
        test_validatefree(
            written == 20 &&
            strcmp(fs_str(&buf), "VALUE64:\"1234567890\"") == 0,
            fsfree(buf),
            "ULONG without typeinfo: expected 'VALUE64:\"1234567890\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    /* ========== CHAR ========== */
    test_sub("subtest %d: CHAR to string", ++subnum);
    {
        value64 v = LITERAL64_CHR('a');
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_CHR, true);
        test_validatefree(
            written == 16 &&
            strcmp(fsstr(buf), "VALUE64(CHR):\"a\"") == 0,
            fsfree(buf),
            "INT: expected 'VALUE64(CHR):\"a\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    /* ========== BOOL ========== */
    test_sub("subtest %d: BOOL to string true with typeinfo", ++subnum);
    {
        value64 v = value64_createbool(true);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_BOOL, true);
        test_validatefree(
            written == 20 &&
            strcmp(fs_str(&buf), "VALUE64(BOOL):\"true\"") == 0,
            fsfree(buf),
            "BOOL true: expected 'VALUE64(BOOL):\"true\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    test_sub("subtest %d: BOOL to string false with typeinfo", ++subnum);
    {
        value64 v = value64_createbool(false);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_BOOL, true);
        test_validatefree(
            written == 21 &&
            strcmp(fs_str(&buf), "VALUE64(BOOL):\"false\"") == 0,
            fsfree(buf),
            "BOOL false: expected 'VALUE64(BOOL):\"false\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    test_sub("subtest %d: BOOL to string true without typeinfo", ++subnum);
    {
        value64 v = value64_createbool(true);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_BOOL, false);
        test_validatefree(
            written == 14 &&   // длина "VALUE64:\"true\""
            strcmp(fs_str(&buf), "VALUE64:\"true\"") == 0,
            fsfree(buf),
            "BOOL true (no type): expected 'VALUE64:\"true\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    test_sub("subtest %d: BOOL to string false without typeinfo", ++subnum);
    {
        value64 v = value64_createbool(false);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_BOOL, false);
        test_validatefree(
            written == 15 &&   // длина "VALUE64:\"false\""
            strcmp(fs_str(&buf), "VALUE64:\"false\"") == 0,
            fsfree(buf),
            "BOOL false (no type): expected 'VALUE64:\"false\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
}

    /* ========== DBL ========== */
    test_sub("subtest %d: DBL to string", ++subnum);
    {
        value64 v = LITERAL64_DBL(3.141592653589793);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_DBL, true);
        // количество знаков после запятой зависит от DBL_DECIMAL_DIG,
        // поэтому проверяем только наличие заголовка и первых цифр
        test_validatefree(
            strstr(fs_str(&buf), "VALUE64(DBL):\"3.14159") != NULL,
            fsfree(buf),
            "DBL: expected 'VALUE64(DBL):\"3.14159…\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    /* ========== PTR ========== */
    test_sub("subtest %d: PTR to string", ++subnum);
    {
        int dummy = 99;
        value64 v = LITERAL64_PTR(&dummy);
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_PTR, true);
        test_validatefree(
            strstr(fs_str(&buf), "VALUE64(PTR):\"") != NULL,
            fsfree(buf),
            "PTR: expected 'VALUE64(PTR):\"…\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        fsfree(buf);
    }

    /* ========== STR ========== */
    test_sub("subtest %d: STR to string", ++subnum);
    {
        value64 v = value64_createstr("Hello");
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_STR, true);
        test_validatefree(
            written == 20 &&
            strcmp(fs_str(&buf), "VALUE64(STR):\"Hello\"") == 0,
            (value64_freestr(&v), fsfree(buf)),
            "STR: expected 'VALUE64(STR):\"Hello\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        value64_freestr(&v);
        fsfree(buf);
    }

    /* ========== FS ========== */
    test_sub("subtest %d: FS to string", ++subnum);
    {
        value64 v = value64_createfs_asstr("/tmp/file");
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_FS, true);
        test_validatefree(
            written == 23 &&
            strcmp(fs_str(&buf), "VALUE64(FS):\"/tmp/file\"") == 0,
            (value64_freefs(&v), fsfree(buf)),
            "FS: expected 'VALUE64(FS):\"/tmp/file\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        value64_free(&v, VALUE64_FS);
        fsfree(buf);
    }

    /* ========== Edge: empty STR ========== */
    test_sub("subtest %d: Empty STR to string", ++subnum);
    {
        value64 v = value64_createstr("");
        fs buf = FS();
        int written = value64_tostr(&buf, v, VALUE64_STR, true);
        test_validatefree(
            written == 15 &&
            strcmp(fs_str(&buf), "VALUE64(STR):\"\"") == 0,
            (value64_freestr(&v), fsfree(buf)),
            "Empty STR: expected 'VALUE64(STR):\"\"', got '%s' (written=%d)",
            fs_str(&buf), written
        );
        value64_free(&v, VALUE64_STR);
        fsfree(buf);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_createfs_asstr -----------------------------

static TestStatus
tf_createfs_asstr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: create FS from valid string", ++subnum);
    {
        value64 v = value64_createfs_asstr("/tmp/test_fs");
        // Проверяем, что поле fsval не NULL и внутренний указатель v тоже не NULL
        test_validatefree(
            v.fsval != NULL && v.fsval->v != NULL,
            value64freefs(v),
            "FS from string: fsval or fsval->v is NULL"
        );
        value64freefs(v);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create FS from empty string", ++subnum);
    {
        value64 v = value64_createfs_asstr("");
        test_validatefree(
            v.fsval != NULL,
            value64freefs(v),
            "FS from empty string: fsval is NULL"
        )
        value64freefs(v);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create FS from long path", ++subnum);
    {
        const char *longpath = "/very/long/path/that/goes/on/and/on/and/on/and/on";
        value64 v = value64_createfs_asstr(longpath);
        test_validatefree(
            v.fsval != NULL,
            value64freefs(v),
            "FS from long path: fsval is NULL"
        );
        value64freefs(v);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create FS from NULL string (must raise)", ++subnum);
    {
        if (!try()) {
            // try-блок: внутри исключение превращается в переход к else
            value64 v = value64_createfs_asstr(NULL); // должно вызвать ERR_NULLABLE_PTR
            (void) v;
            // если мы здесь, исключения не было → тест провален
            test_validate(false, "NULL input should have raised SIGINT");
        } else {
            // исключение перехвачено, всё корректно
            logmsg("Exception correctly raised on NULL input");
        }
    }
    fs_alloc_check(true);  // убеждаемся, что неудачная попытка не оставила утечек

    test_sub("subtest %d: double free safety check", ++subnum);
    {
        value64 v = value64_createfs_asstr("/tmp/some");
        value64freefs(v);
        value64freefs(v);  // повторный вызов не должен упасть
        // Если дошли сюда – ОК, value64free идемпотентен
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_setzero ---------------------------------
static TestStatus
tf_setzero(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: setzero INT", ++subnum);
    {
        value64 v = LITERAL64_INT(42);
        value64_setzero(&v, VALUE64_INT);
        test_validate(
            v.ival == 0,
            "INT should be 0 after setzero"
        );
    }

    test_sub("subtest %d: setzero LONG", ++subnum);
    {
        value64 v = LITERAL64_LONG(123456789L);
        value64_setzero(&v, VALUE64_LONG);
        test_validate(
            v.lval == 0L,
            "LONG should be 0 after setzero"
        );
    }

    test_sub("subtest %d: setzero ULONG", ++subnum);
    {
        value64 v = LITERAL64_ULONG(123456789UL);
        value64_setzero(&v, VALUE64_ULONG);
        test_validate(
            v.ulval == 0UL,
            "LONG should be 0 after setzero"
        );
    }

    test_sub("subtest %d: setzero CHAR", ++subnum);
    {
        value64 v = LITERAL64_CHR('a');
        value64_setzero(&v, VALUE64_CHR);
        test_validate(
            v.cval == '\0',
            "CHAR should be 0 after setzero"
        );
    }

    test_sub("subtest %d: setzero BOOL", ++subnum);
    {
        value64 v = LITERAL64_BOOL(true);
        value64_setzero(&v, VALUE64_BOOL);
        test_validate(
            v.cval == false,
            "CHAR should be false after setzero"
        );
    }

    test_sub("subtest %d: setzero DBL", ++subnum);
    {
        value64 v = LITERAL64_DBL(3.14159);
        value64_setzero(&v, VALUE64_DBL);
        test_validate(
            v.dval == 0.0,
            "DOUBLE should be 0.0 after setzero"
        );
    }

    test_sub("subtest %d: setzero PTR", ++subnum);
    {
        int dummy = 10;
        value64 v = LITERAL64_PTR(&dummy);
        value64_setzero(&v, VALUE64_PTR);
        test_validate(
            v.pval == NULL,
            "PTR should be NULL after setzero"
        );
    }

    test_sub("subtest %d: setzero STR (ownership not transferred)", ++subnum);
    {
        value64 v = value64_createstr("hello");
        char *str_ptr = v.sval;          // запоминаем, чтобы вручную освободить
        value64_setzero(&v, VALUE64_STR);
        test_validatefree(
            v.sval == NULL,
            free(str_ptr),
            "STR sval should be NULL after setzero"
        );
        free(str_ptr);                   // очищаем старую строку
    }
    fs_alloc_check(true);  // STR не связан с FS, но на всякий случай

    test_sub("subtest %d: setzero FS (ownership not transferred)", ++subnum);
    {
        value64 v = value64_createfs_asstr("/tmp/test");
        fs *fs_ptr = v.fsval;            // сохраняем, чтобы освободить отдельно
        value64_setzero(&v, VALUE64_FS);
        test_validatefree(
            v.fsval == NULL,
            fs_free(fs_ptr),
            "FS fsval should be NULL after setzero"
        );
        fs_free(fs_ptr);                 // освобождаем старый объект
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_move ---------------------------------
static TestStatus
tf_value64_move(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- INT ---------- */
    test_sub("subtest %d: move INT", ++subnum);
    {
        value64 src = LITERAL64_INT(42);
        value64 dst = value64_move(&src, VALUE64_INT);
        test_validate(
            dst.ival == 42 && src.ival == 0,
            "Move INT: dst=%d, src=%d (expected 42, 0)", dst.ival, src.ival
        );
    }

    /* ---------- LONG ---------- */
    test_sub("subtest %d: move LONG", ++subnum);
    {
        value64 src = { .lval = 999888777L };
        value64 dst = value64_move(&src, VALUE64_LONG);
        test_validate(
            dst.lval == 999888777L && src.lval == 0L,
            "Move LNG: dst=%ld, src=%ld (expected 999888777, 0)", dst.lval, src.lval
        );
    }

    /* ---------- LONG ---------- */
    test_sub("subtest %d: move LONG", ++subnum);
    {
        value64 src = { .lval = 999888777UL };
        value64 dst = value64_move(&src, VALUE64_ULONG);
        test_validate(
            dst.ulval == 999888777UL && src.ulval == 0UL,
            "Move LNG: dst=%lu, src=%lu (expected 999888777, 0)", dst.ulval, src.ulval
        );
    }

    /* ---------- CHAR ---------- */
    test_sub("subtest %d: move CHAR", ++subnum);
    {
        value64 src = LITERAL64_CHR('r');
        value64 dst = value64_move(&src, VALUE64_CHR);
        test_validate(
            dst.cval == 'r' && src.cval == 0,
            "Move CHAR: dst=%d, src=%d (expected 'r', 0)", dst.cval, src.cval
        );
    }

    /* ---------- BOOL ---------- */
    test_sub("subtest %d: move BOOL", ++subnum);
    {
        value64 src = LITERAL64_BOOL(true);
        value64 dst = value64_move(&src, VALUE64_BOOL);
        test_validate(
            dst.bval == true && src.bval == false,
            "Move CHAR: dst=%s, src=%s (expected true, false)", bool_str(dst.bval), bool_str(src.bval)
        );
    }

    /* ---------- DBL ---------- */
    test_sub("subtest %d: move DBL", ++subnum);
    {
        value64 src = LITERAL64_DBL(3.14159);
        value64 dst = value64_move(&src, VALUE64_DBL);
        test_validate(
            dst.dval == 3.14159 && src.dval == 0.0,
            "Move DBL: dst=%f, src=%f (expected 3.14159, 0.0)", dst.dval, src.dval
        );
    }

    /* ---------- PTR ---------- */
    test_sub("subtest %d: move PTR", ++subnum);
    {
        int dummy = 10;
        value64 src = LITERAL64_PTR(&dummy);
        value64 dst = value64_move(&src, VALUE64_PTR);
        test_validate(
            dst.pval == &dummy && src.pval == NULL,
            "Move PTR: dst=%p, src=%p (expected %p, NULL)", dst.pval, src.pval, &dummy
        );
    }

    /* ---------- STR (owned) ---------- */
    test_sub("subtest %d: move STR", ++subnum);
    {
        value64 src = value64_createstr("hello");
        char *orig = src.sval;
        value64 dst = value64_move(&src, VALUE64_STR);
        test_validatefree(
            dst.sval == orig && src.sval == NULL,
            value64_freestr(&dst),
            "Move STR: dst=%p, src=%p (expected %p, NULL)", dst.sval, src.sval, orig
        );
        value64_freestr(&dst);
    }

    /* ---------- FS (owned) ---------- */
    test_sub("subtest %d: move FS", ++subnum);
    {
        value64 src = value64_createfs_asstr("/tmp/test");
        fs *orig_fs = src.fsval;
        value64 dst = value64_move(&src, VALUE64_FS);
        test_validatefree(
            dst.fsval == orig_fs && src.fsval == NULL,
            value64_freefs(&dst),
            "Move FS: dst=%p, src=%p (expected %p, NULL)", dst.fsval, src.fsval, orig_fs
        );
        value64_freefs(&dst);
    }
    fs_alloc_check(true);

    /* ---------- Edge: move from zero ---------- */
    test_sub("subtest %d: move from zero (INT)", ++subnum);
    {
        value64 src = LITERAL64_ZERO;
        value64 dst = value64_move(&src, VALUE64_INT);
        test_validate(
            dst.ival == 0 && src.ival == 0,
            "Move from zero: dst=%d, src=%d", dst.ival, src.ival
        );
    }

    /* ---------- Edge: move from empty FS ---------- */
    test_sub("subtest %d: move from empty FS (already moved)", ++subnum);
    {
        value64 src = LITERAL64_ZERO;   // fsval == NULL
        value64 dst = value64_move(&src, VALUE64_FS);
        test_validate(
            dst.fsval == NULL && src.fsval == NULL,
            "Move from empty FS: dst=%p, src=%p", dst.fsval, src.fsval
        );
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64_techfprint() manual ---------------------------------
static TestStatus
tf_techfprint(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d:  INT", ++subnum);
    {
        value64 vint = LITERAL64_INT(5);
        VALUE64_TECHFPRINT(logfile, vint, VALUE64_INT);
    }
    test_sub("subtest %d:  LONG", ++subnum);
    {
        value64 vlong = LITERAL64_LONG(5000L);
        VALUE64_TECHFPRINT(logfile, vlong, VALUE64_LONG);
    }
    test_sub("subtest %d:  ULONG", ++subnum);
    {
        value64 vulong = LITERAL64_ULONG(58888888UL);
        VALUE64_TECHFPRINT(logfile, vulong, VALUE64_ULONG);
    }
    test_sub("subtest %d:  CHAR", ++subnum);
    {
        value64 vchar = LITERAL64_CHR('q');
        VALUE64_TECHFPRINT(logfile, vchar, VALUE64_CHR);
    }
    test_sub("subtest %d:  BOOL", ++subnum);
    {
        value64 bchar = LITERAL64_BOOL(false);
        VALUE64_TECHFPRINT(logfile, bchar, VALUE64_BOOL);
    }
    test_sub("subtest %d:  DOUBLE", ++subnum);
    {
        value64 vdouble = LITERAL64_DBL(8.9);
        VALUE64_TECHFPRINT(logfile, vdouble, VALUE64_DBL);
    }
    test_sub("subtest %d:  STR", ++subnum);
    {
        value64 vstr = LITERAL64_STR("bla bla");
        VALUE64_TECHFPRINT(logfile, vstr, VALUE64_STR);
    }
    test_sub("subtest %d:  FS", ++subnum);
    {
        value64 vfs = value64_createfs_asstr("qwertyui1");
        VALUE64_TECHFPRINT(logfile, vfs, VALUE64_FS);
        value64_free(&vfs, VALUE64_FS);
    }
    fs_alloc_check(true);
    return logret(TEST_MANUAL, "done");
}

// ------------------------- TEST value64_str_serialization -------------------------
static TestStatus
tf_str_serialization(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== INT ========== */
    test_sub("subtest %d: INT save/load to string", ++subnum);
    {
        value64 orig = LITERAL64_INT(42);
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_INT, true);
        //VALUE64_TECHPRINT(orig, VALUE64_INT);
        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_INT),
            fsfree(buf),
            "INT str round-trip failed (string='%s')", fs_str(&buf)
        );
        fsfree(buf);
        fs_alloc_check(true);
    }

    /* ========== LONG ========== */
    test_sub("subtest %d: LONG save/load to string", ++subnum);
    {
        value64 orig = LITERAL64_LONG(123456789L);
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_LONG, true);

        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_LONG),
            fsfree(buf),
            "LONG str round-trip failed"
        );
        fsfree(buf);
        fs_alloc_check(true);
    }

    /* ========== ULONG ========== */
    test_sub("subtest %d: ULONG save/load to string", ++subnum);
    {
        value64 orig = value64_createulong(1234567890UL);
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_ULONG, true);
        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_ULONG),
            fsfree(buf),
            "ULONG str round-trip failed (string='%s')", fs_str(&buf)
        );
        fsfree(buf);
        fs_alloc_check(true);
    }

    /* ========== CHAR ========== */
    test_sub("subtest %d: CHAR save/load to string", ++subnum);
    {
        value64 orig = LITERAL64_CHR('a');
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_CHR, true);
        //VALUE64_TECHPRINT(orig, VALUE64_INT);
        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_CHR),
            fsfree(buf),
            "CHAR str round-trip failed (string='%s')", fs_str(&buf)
        );
        fsfree(buf);
        fs_alloc_check(true);
    }
    /* ========== BOOL ========== */
    test_sub("subtest %d: BOOL save/load to string true", ++subnum);
    {
        value64 orig = value64_createbool(true);
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_BOOL, true);
        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_BOOL),
            fsfree(buf),
            "BOOL true str round-trip failed (string='%s')", fs_str(&buf)
        );
        fsfree(buf);
        value64free(orig, VALUE64_BOOL);
        value64free(loaded, VALUE64_BOOL);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: BOOL save/load to string false", ++subnum);
    {
        value64 orig = value64_createbool(false);
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_BOOL, true);
        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_BOOL),
            fsfree(buf),
            "BOOL false str round-trip failed (string='%s')", fs_str(&buf)
        );
        fsfree(buf);
        value64free(orig, VALUE64_BOOL);
        value64free(loaded, VALUE64_BOOL);
        fs_alloc_check(true);
    }

    /* ========== DBL ========== */
    test_sub("subtest %d: DBL save/load to string", ++subnum);
    {
        value64 orig = LITERAL64_DBL(3.141592653589793);
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_DBL, true);

        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_DBL),
            fsfree(buf),
            "DBL str round-trip failed"
        );
        fsfree(buf);
        fs_alloc_check(true);
    }

    /* ========== STR (обычная) ========== */
    test_sub("subtest %d: STR save/load to string", ++subnum);
    {
        value64 orig = value64_createstr("Hello, World!");
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_STR, true);

        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_STR),
            (value64_freestr(&orig), value64_freestr(&loaded), fsfree(buf)),
            "STR round-trip failed (string='%s')", fs_str(&buf)
        );
        value64_freestr(&orig);
        value64_freestr(&loaded);
        fsfree(buf);
        fs_alloc_check(true);
    }

    /* ========== STR (спецсимволы) ========== */
    test_sub("subtest %d: STR with quotes and escapes", ++subnum);
    {
        value64 orig = value64_createstr("Line1\nLine2\t\"quoted\"\\end");
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_STR, true);

        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_STR),
            (value64_free(&orig, VALUE64_STR), fsfree(buf), value64_free(&loaded, VALUE64_STR) ),
            "STR escapes round-trip failed"
        );
        value64_free(&orig, VALUE64_STR);
        value64_free(&loaded, VALUE64_STR);
        fsfree(buf);
        fs_alloc_check(true);
    }

    /* ========== FS ========== */
    test_sub("subtest %d: FS save/load to string", ++subnum);
    {
        value64 orig = value64_createfs_asstr("/home/user/file.txt");
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_FS, true);

        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_FS),
            (value64_freefs(&orig), fsfree(buf)),
            "FS round-trip failed (string='%s')", fs_str(&buf)
        );
        value64_free(&orig, VALUE64_FS);
        value64_free(&loaded, VALUE64_FS);
        fsfree(buf);
        fs_alloc_check(true);
    }

    /* ========== PTR (только проверка заголовка) ========== */
    test_sub("subtest %d: PTR save to string", ++subnum);
    {
        int dummy = 99;
        value64 orig = LITERAL64_PTR(&dummy);
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_PTR, true);
        test_validatefree(
            strstr(fs_str(&buf), "VALUE64(PTR)") != NULL,
            fsfree(buf),
            "PTR serialization must contain VALUE64(PTR) header"
        );
        fsfree(buf);
        fs_alloc_check(true);
    }

    /* ========== Edge cases ========== */
    test_sub("subtest %d: empty string round-trip", ++subnum);
    {
        value64 orig = value64_createstr("");
        fs buf = FS();
        value64_tostr(&buf, orig, VALUE64_STR, true);

        value64 loaded;
        test_validatefree(
            value64_loadstr(fs_str(&buf), &loaded, VALUE64_UNKNOWN, true, NULL) &&
            value64_equal(orig, loaded, VALUE64_STR),
            (value64_freestr(&orig), value64_freestr(&loaded), fsfree(buf)),
            "Empty STR round-trip failed"
        );
        value64_freestr(&orig);
        value64_freestr(&loaded);
        fsfree(buf);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: load from malformed string fails", ++subnum);
    {
        value64 loaded;
        test_validate(
            value64_loadstr("not a valid header", &loaded, VALUE64_UNKNOWN, true, NULL) < 0,
            "Malformed string must return < 0>"
        );
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST VALUE64_FILE tests -------------------------
static TestStatus
tf24_file(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. create FILE and getter */
    test_sub("subtest %d: FILE create and getter", ++subnum);
    {
        FILE *f = tmpfile();
        test_validatefree(f != NULL, fclose(f), "tmpfile failed");

        value64 v = value64_createFILE(f);
        test_validatefree(
            value64_FILE(v) == f,
            fclose(f),
            "FILE getter mismatch"
        );
        fclose(f);   // файл закрыт вручную
    }

    /* 2. move constructor for FILE */
    test_sub("subtest %d: FILE move constructor", ++subnum);
    {
        FILE *f = tmpfile();
        test_validatefree(f != NULL, fclose(f), "tmpfile failed");

        value64 v = value64_createFILE(f);
        value64 moved = value64_move(&v, VALUE64_FILE);
        test_validatefree(
            value64_FILE(moved) == f,
            fclose(f),
            "move: target FILE mismatch"
        );
        test_validatefree(
            v.FILEval == NULL,
            fclose(f),
            "move: source FILE must be cleared"
        );
        fclose(f);
    }

    /* 3. moveto FILE */
    test_sub("subtest %d: FILE moveto", ++subnum);
    {
        FILE *f = tmpfile();
        test_validatefree(f != NULL, fclose(f), "tmpfile failed");

        value64 src = value64_createFILE(f);
        value64 dst = LITERAL64_ZERO;
        value64 *res = value64_moveto_FILE(&dst, &src);
        test_validatefree(
            res == &dst,
            fclose(f),
            "moveto should return target ptr"
        );
        test_validatefree(
            value64_FILE(dst) == f,
            fclose(f),
            "moveto: target FILE mismatch"
        );
        test_validatefree(
            src.FILEval == NULL,
            fclose(f),
            "moveto: source FILE must be cleared"
        );
        fclose(f);
    }

    /* 4. clone FILE must raise error */
    test_sub("subtest %d: FILE clone must raise", ++subnum);
    {
        FILE *f = tmpfile();
        test_validatefree(f != NULL, fclose(f), "tmpfile failed");

        value64 v = value64_createFILE(f);
        if (!try()) {
            value64 cloned = value64_clone(v, VALUE64_FILE);
            // clone должен падать, сюда не должны попасть
            test_validatefree(
                false,
                (value64_free(&cloned, VALUE64_FILE), fclose(f)),
                "clone FILE must raise error"
            );
        } else {
            test_validatefree(true, fclose(f), "clone FILE correctly raised error");
        }
        fclose(f);
    }

    /* 5. lhash for FILE smoke test */
    test_sub("subtest %d: FILE hash smoke", ++subnum);
    {
        FILE *f = tmpfile();
        test_validatefree(f != NULL, fclose(f), "tmpfile failed");

        value64 v = value64_createFILE(f);
        unsigned long h = value64_lhash(v, VALUE64_FILE);
        test_validatefree(
            h == hash_long((uint64_t)f),
            fclose(f),
            "hash should match hash_long(ptr)"
        );
        fclose(f);
    }

    return TEST_PASSED;
}

// =====================================================================
// value64.h — tests for value64_fs_compstr / value64_fs_comp
// =====================================================================
// Замечания по семантике, которая проверяется:
//   * fs_cmpstr / fs_cmp -> strcmp-like контракт: <0, 0, >0 (не -1/1)
//   * "null fs" (fsval == NULL или .v == NULL, .sz == 0) -> userraiseint
//     через ERR_NULLABLE_PTR (longjmp, ловится try())
//   * пустая строка "" — это ВАЛИДНЫЙ fs, не null
//     (fs_isnull возвращает true только для NULL/sz==0&&v==0)

// ---------------------- value64_fs_compstr ---------------------------
static TestStatus
tf25_value64_fs_compstr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. равные строки -> 0 */
    test_sub("subtest %d: equal strings -> 0", ++subnum);
    {
        value64 v = value64_createfs_asstr("hello");
        test_validatefree(value64_fs_compstr(v, "hello") == 0,
                          value64_free(&v, VALUE64_FS),
                          "expected 0 for equal strings");
        value64_free(&v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 2. v1 < str -> <0 */
    test_sub("subtest %d: v1 < str", ++subnum);
    {
        value64 v = value64_createfs_asstr("abc");
        test_validatefree(value64_fs_compstr(v, "abd") < 0,
                          value64_free(&v, VALUE64_FS),
                          "expected <0 when v1 < str");
        value64_free(&v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 3. v1 > str -> >0 */
    test_sub("subtest %d: v1 > str", ++subnum);
    {
        value64 v = value64_createfs_asstr("abd");
        test_validatefree(value64_fs_compstr(v, "abc") > 0,
                          value64_free(&v, VALUE64_FS),
                          "expected >0 when v1 > str");
        value64_free(&v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 4. префикс: короткая vs длинная */
    test_sub("subtest %d: prefix shorter < longer", ++subnum);
    {
        value64 v = value64_createfs_asstr("abc");
        test_validatefree(value64_fs_compstr(v, "abcd") < 0,
                          value64_free(&v, VALUE64_FS),
                          "prefix must be < longer");
        value64_free(&v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 5. пустые строки: "" vs "" -> 0 */
    test_sub("subtest %d: empty vs empty -> 0", ++subnum);
    {
        value64 v = value64_createfs_asstr("");
        test_validatefree(value64_fs_compstr(v, "") == 0,
                          value64_free(&v, VALUE64_FS),
                          "empty vs empty must be 0");
        value64_free(&v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 6. "" vs "x" -> <0, "x" vs "" -> >0 */
    test_sub("subtest %d: empty vs nonempty", ++subnum);
    {
        value64 e = value64_createfs_asstr("");
        value64 n = value64_createfs_asstr("x");

        test_validatefree(value64_fs_compstr(e, "x") < 0,
                          (value64_free(&e, VALUE64_FS), value64_free(&n, VALUE64_FS)),
                          "empty must be < nonempty");
        test_validatefree(value64_fs_compstr(n, "") > 0,
                          (value64_free(&e, VALUE64_FS), value64_free(&n, VALUE64_FS)),
                          "nonempty must be > empty");

        value64_free(&e, VALUE64_FS);
        value64_free(&n, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 7. case-sensitive */
    test_sub("subtest %d: case sensitive", ++subnum);
    {
        value64 lo = value64_createfs_asstr("abc");
        value64 up = value64_createfs_asstr("ABC");

        test_validatefree(value64_fs_compstr(lo, "ABC") > 0,
                          (value64_free(&lo, VALUE64_FS), value64_free(&up, VALUE64_FS)),
                          "strcmp: 'abc' > 'ABC' (lower > upper in ASCII)");
        test_validatefree(value64_fs_compstr(up, "abc") < 0,
                          (value64_free(&lo, VALUE64_FS), value64_free(&up, VALUE64_FS)),
                          "strcmp: 'ABC' < 'abc'");

        value64_free(&lo, VALUE64_FS);
        value64_free(&up, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 8. пробельные/спецсимволы */
    test_sub("subtest %d: whitespace and specials", ++subnum);
    {
        value64 v = value64_createfs_asstr("with space");

        test_validatefree(value64_fs_compstr(v, "with space") == 0,
                          value64_free(&v, VALUE64_FS), "eq with space");
        test_validatefree(value64_fs_compstr(v, "with_space") < 0,
                          value64_free(&v, VALUE64_FS), "space < underscore");
        test_validatefree(value64_fs_compstr(v, "with\tspace") > 0,
                          value64_free(&v, VALUE64_FS), "space > tab");

        value64_free(&v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 9. встроенный NUL в str (C-строка) не должен учитываться за пределами NUL */
    test_sub("subtest %d: fs longer, str cut at NUL", ++subnum);
    {
        value64 v = value64_createfs_asstr("abcdef");
        /* strcmp остановится на 'd' перед \0 */
        test_validatefree(value64_fs_compstr(v, "abc") > 0,
                          value64_free(&v, VALUE64_FS),
                          "'abcdef' > 'abc'");
        value64_free(&v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 10. NULL str -> raise */
    test_sub("subtest %d: NULL str raises", ++subnum);
    {
        value64 v = value64_createfs_asstr("x");
        if (!try()) {
            value64_fs_compstr(v, NULL);
            test_validatefree(false, value64_free(&v, VALUE64_FS),
                              "must raise for NULL str");
        } else {
            test_validatefree(true, value64_free(&v, VALUE64_FS),
                              "correctly raised");
        }
        value64_free(&v, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 11. null fs (fsval == NULL) -> raise */
    test_sub("subtest %d: null fs raises", ++subnum);
    {
        value64 v = LITERAL64_ZERO;   // fsval == NULL
        if (!try()) {
            value64_fs_compstr(v, "x");
            test_validate(false, "must raise for null fs");
        } else {
            test_validate(true, "correctly raised");
        }
        fs_alloc_check(true);
    }

    /* 12. fs with .v == NULL -> raise (fs_isnull true) */
    test_sub("subtest %d: fs with .v == NULL raises", ++subnum);
    {
        /* Создаём value64 с fs, у которого .v == NULL — через LITERAL64_FS(NULL fs)? */
        /* Если такого API нет — пропускаем. Проверяем через пустую FS() */
        fs nullfs = FSEMPTY;   // .sz == 0, .v == "" -> это НЕ null для fs_isnull
        value64 v = LITERAL64_FS(nullfs);
        /* .v = "" (не NULL), .sz = 0 -> fs_isnull(s) == (s==NULL || (sz==0 && v==0)) */
        /* v == "" != 0, значит НЕ null. Тест на "null fs" через FSEMPTY не сработает. */
        /* Пропускаем — см. subtest 11 для реального null fs. */
        (void) v;
        test_validate(true, "skipped (fs_isnull semantics)");
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ---------------------- value64_fs_comp ------------------------------
static TestStatus
tf26_value64_fs_comp(const char *name) 
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. равные -> 0 */
    test_sub("subtest %d: equal -> 0", ++subnum);
    {
        value64 a = value64_createfs_asstr("hello");
        value64 b = value64_createfs_asstr("hello");
        test_validatefree(value64_fs_comp(a, b) == 0,
                          (value64_free(&a, VALUE64_FS), value64_free(&b, VALUE64_FS)),
                          "equal strings must give 0");
        value64_free(&a, VALUE64_FS);
        value64_free(&b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 2. a < b -> <0 */
    test_sub("subtest %d: a < b", ++subnum);
    {
        value64 a = value64_createfs_asstr("abc");
        value64 b = value64_createfs_asstr("abd");
        test_validatefree(value64_fs_comp(a, b) < 0,
                          (value64_free(&a, VALUE64_FS), value64_free(&b, VALUE64_FS)),
                          "expected <0");
        value64_free(&a, VALUE64_FS);
        value64_free(&b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 3. a > b -> >0 */
    test_sub("subtest %d: a > b", ++subnum);
    {
        value64 a = value64_createfs_asstr("abd");
        value64 b = value64_createfs_asstr("abc");
        test_validatefree(value64_fs_comp(a, b) > 0,
                          (value64_free(&a, VALUE64_FS), value64_free(&b, VALUE64_FS)),
                          "expected >0");
        value64_free(&a, VALUE64_FS);
        value64_free(&b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 4. антисимметрия: sign(comp(a,b)) == -sign(comp(b,a)) */
    test_sub("subtest %d: antisymmetry", ++subnum);
    {
        value64 a = value64_createfs_asstr("alpha");
        value64 b = value64_createfs_asstr("beta");

        int ab = value64_fs_comp(a, b);
        int ba = value64_fs_comp(b, a);

        test_validatefree((ab < 0 && ba > 0) || (ab > 0 && ba < 0) || (ab == 0 && ba == 0),
                          (value64_free(&a, VALUE64_FS), value64_free(&b, VALUE64_FS)),
                          "sign mismatch: ab=%d ba=%d", ab, ba);

        value64_free(&a, VALUE64_FS);
        value64_free(&b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 5. транзитивность на трёх строках */
    test_sub("subtest %d: transitivity", ++subnum);
    {
        value64 a = value64_createfs_asstr("aaa");
        value64 b = value64_createfs_asstr("bbb");
        value64 c = value64_createfs_asstr("ccc");

        int ab = value64_fs_comp(a, b);
        int bc = value64_fs_comp(b, c);
        int ac = value64_fs_comp(a, c);

        test_validatefree(ab < 0 && bc < 0 && ac < 0,
                          (value64_free(&a, VALUE64_FS),
                           value64_free(&b, VALUE64_FS),
                           value64_free(&c, VALUE64_FS)),
                          "expected all <0: ab=%d bc=%d ac=%d", ab, bc, ac);

        value64_free(&a, VALUE64_FS);
        value64_free(&b, VALUE64_FS);
        value64_free(&c, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 6. обе пустые -> 0 */
    test_sub("subtest %d: both empty -> 0", ++subnum);
    {
        value64 a = value64_createfs_asstr("");
        value64 b = value64_createfs_asstr("");
        test_validatefree(value64_fs_comp(a, b) == 0,
                          (value64_free(&a, VALUE64_FS), value64_free(&b, VALUE64_FS)),
                          "empty vs empty must be 0");
        value64_free(&a, VALUE64_FS);
        value64_free(&b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 7. пустая < непустой */
    test_sub("subtest %d: empty < nonempty", ++subnum);
    {
        value64 e = value64_createfs_asstr("");
        value64 n = value64_createfs_asstr("x");
        test_validatefree(value64_fs_comp(e, n) < 0,
                          (value64_free(&e, VALUE64_FS), value64_free(&n, VALUE64_FS)),
                          "empty must be < nonempty");
        value64_free(&e, VALUE64_FS);
        value64_free(&n, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 8. префикс */
    test_sub("subtest %d: prefix < longer", ++subnum);
    {
        value64 s = value64_createfs_asstr("abc");
        value64 l = value64_createfs_asstr("abcd");
        test_validatefree(value64_fs_comp(s, l) < 0,
                          (value64_free(&s, VALUE64_FS), value64_free(&l, VALUE64_FS)),
                          "prefix must be < longer");
        value64_free(&s, VALUE64_FS);
        value64_free(&l, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 9. согласованность с value64_fs_compstr */
    test_sub("subtest %d: consistency with compstr", ++subnum);
    {
        value64 a = value64_createfs_asstr("foo");
        value64 b = value64_createfs_asstr("bar");

        int via_comp    = value64_fs_comp(a, b);
        int via_compstr = value64_fs_compstr(a, "bar");

        test_validatefree((via_comp < 0) == (via_compstr < 0) &&
                          (via_comp > 0) == (via_compstr > 0) &&
                          (via_comp == 0) == (via_compstr == 0),
                          (value64_free(&a, VALUE64_FS), value64_free(&b, VALUE64_FS)),
                          "sign mismatch comp=%d compstr=%d", via_comp, via_compstr);

        value64_free(&a, VALUE64_FS);
        value64_free(&b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 10. NULL v1.fsval -> raise */
    test_sub("subtest %d: NULL v1.fsval raises", ++subnum);
    {
        value64 a = LITERAL64_ZERO;
        value64 b = value64_createfs_asstr("x");

        if (!try()) {
            value64_fs_comp(a, b);
            test_validatefree(false, value64_free(&b, VALUE64_FS),
                              "must raise for NULL v1.fsval");
        } else {
            test_validatefree(true, value64_free(&b, VALUE64_FS),
                              "correctly raised");
        }
        value64_free(&b, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 11. NULL v2.fsval -> raise */
    test_sub("subtest %d: NULL v2.fsval raises", ++subnum);
    {
        value64 a = value64_createfs_asstr("x");
        value64 b = LITERAL64_ZERO;

        if (!try()) {
            value64_fs_comp(a, b);
            test_validatefree(false, value64_free(&a, VALUE64_FS),
                              "must raise for NULL v2.fsval");
        } else {
            test_validatefree(true, value64_free(&a, VALUE64_FS),
                              "correctly raised");
        }
        value64_free(&a, VALUE64_FS);
        fs_alloc_check(true);
    }

    /* 12. оба NULL -> raise (проверка порядка проверок) */
    test_sub("subtest %d: both NULL raises", ++subnum);
    {
        value64 a = LITERAL64_ZERO;
        value64 b = LITERAL64_ZERO;
        if (!try()) {
            value64_fs_comp(a, b);
            test_validate(false, "must raise");
        } else {
            test_validate(true, "correctly raised");
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------------------------------------------------------------------------------------------------------------
int
main(/* int argc, const char *argv[] */)
{

    logsimpleinit("Start");
    testenginestd(
        TESTADD(tf_init_free,               "Simple init and validate test"),
        TESTADD(tf_point_init,              "Simple value64_pcopy_move() test"),
        TESTADD(tf_clone,                   "Simple value64_clone() test"),
        TESTADD(tf_move,                    "Simple value64_moveto() test"),
        TESTADD(tf_lhash,                   "Simple value64_lhash() test"),
        TESTADD(tf_compare,                 "Simple value64_compare() test"),
        TESTADD(tf_is_convertable,          "Simple value64_is_convertable() test"),
        TESTADD(tf_convert,                 "Simple value64_convert() test"),
        TESTADD(tf_convert_move,            "Simple value64_convert_move() test"),
        TESTADD(tf_pt_compare,              "Simple value64_pt_compare() test"),
        TESTADD(tf_search,                  "Simple value64_(rev)search test"),
        TESTADD(tf_getComparator,           "Simple value64_get(Rev)Comparator test"),
        TESTADD(tf_getPComparator,          "Simple value64_getP(Rev)Comparator test"),
        TESTADD(tf_binsearch,               "Simple value64_(rev_)binsearch test"),
        TESTADD(tf_sort,                    "Simple value64_(rev_)sort test"),
        TESTADD(tf_fsave,                   "Simple value64_tofile manual test"),
        TESTADD(tf_fsave_fload,             "Simple value64_tofile/fload test"),
        TESTADD(tf_tostr,                   "Simple value64_tostr_<type> test"),
        TESTADD(tf_createfs_asstr,          "Simple value64_createfs_asstr test"),
        TESTADD(tf_setzero,                 "Simple value64_setzero test"),
        TESTADD(tf_value64_move,            "Simple value64_move() test"),
        TESTADD(tf_techfprint,              "Simple value64_techfprint() manual test"),
        TESTADD(tf_str_serialization,       "Simple serialization (tostr/loadstr) test"),
        TESTADD(tf24_file,                  "Simple VALUE64_FILE tests"),
        TESTADD(tf25_value64_fs_compstr,    "Simple value64_fs_compstr() tests"),
        TESTADD(tf26_value64_fs_comp,       "Simple value64_fs_comp() tests")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* HSETTESTING */


