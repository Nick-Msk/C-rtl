#ifndef _V64TYPED_H
#define _V64TYPED_H

// -----------------------------------------------------------------------------------
// ------------------------------ Public typed v64 API -------------------------------
// -----------------------------------------------------------------------------------

// ------------------------------- Includes ------------------------------------------

#include <stdio.h>

#include "value64.h"
#include "error.h"
#include "checker.h"

// ---------------------------------- CONSTANTS AND GLOBALS --------------------------

// ------------------------------------ TYPES ----------------------------------------

/** @brief Wrapper over value64 (value + type) */
typedef struct {
    value64      val;   ///< само значение
    value64_type typ;   ///< его тип (VALUE64_INT, VALUE64_FS, ...)
} v64typed;

// ---------------------------- CONSTRUCTORS / DESTRUCTORS ---------------------------

// COMMON ZERO INIT
#define V64TYPEDZERO(...)   (v64typed) { .val = LITERAL64_ZERO, .typ = VALUE64_UNKNOWN, __VA_ARGS__ }

// PER TYPE
#define V64TYPEDINT(val)    (v64typed) { .val = LITERAL64_INT(val),   .typ = VALUE64_INT }
#define V64TYPEDLONG(val)   (v64typed) { .val = LITERAL64_LONG(val),  .typ = VALUE64_LONG }
#define V64TYPEDULONG(val)  (v64typed) { .val = LITERAL64_ULONG(val), .typ = VALUE64_ULONG }
#define V64TYPEDCHAR(val)   (v64typed) { .val = LITERAL64_CHR(val),   .typ = VALUE64_CHR }
#define V64TYPEDBOOL(val)   (v64typed) { .val = LITERAL64_BOOL(val),  .typ = VALUE64_BOOL }
#define V64TYPEDDBL(val)    (v64typed) { .val = LITERAL64_DBL(val),   .typ = VALUE64_DBL }
#define V64TYPEDPTR(val)    (v64typed) { .val = LITERAL64_PTR(val),   .typ = VALUE64_PTR }
#define V64TYPEDSTR(val)    (v64typed) { .val = LITERAL64_STR(val),   .typ = VALUE64_STR }
#define V64TYPEDFILE(val)   (v64typed) { .val = LITERAL64_FILE(val),  .typ = VALUE64_FILE }


static inline v64typed              v64typedCreate(value64 val, value64_type typ) {
    return V64TYPEDZERO(.val = val, .typ = typ);
}
// not get ownership, for generators
static inline v64typed              v64typedCreateCstrSource(const char *str) {
    return v64typedCreate(LITERAL64_PTR( (void *) str), VALUE64_PTR); // PTR for simulation!
}
// not get ownership, for generators
static inline v64typed              v64typedCreateFsSource(const fs *str) {
    return v64typedCreate(LITERAL64_PTR( (void *) str), VALUE64_PTR); // PTR for simulation!
}
static inline v64typed              v64typedCreateInt(int x) {
    return v64typedCreate(LITERAL64_INT(x), VALUE64_INT);
}
static inline v64typed              v64typedCreateLong(long x) {
    return (v64typed){ .val = LITERAL64_LONG(x), .typ = VALUE64_LONG };
}
static inline v64typed              v64typedCreateULong(unsigned long x) {
    return (v64typed){ .val = LITERAL64_ULONG(x), .typ = VALUE64_ULONG };
}
static inline v64typed              v64typedCreateChar(char x) {
    return v64typedCreate(LITERAL64_CHR(x), VALUE64_CHR);
}
static inline v64typed              v64typedCreateBool(bool x) {
    return v64typedCreate(LITERAL64_BOOL(x), VALUE64_BOOL);
}
static inline v64typed              v64typedCreateDbl(double x) {
    return (v64typed){ .val = LITERAL64_DBL(x), .typ = VALUE64_DBL };
}
static inline v64typed              v64typedCreateStr(const char *s) {
    return (v64typed){ .val = value64_createstr(s), .typ = VALUE64_STR };
}
static inline v64typed              v64typedCreateFs(fs *s) {
    return (v64typed){ .val = value64_createfs(s), .typ = VALUE64_FS };
}
static inline v64typed              v64typedCreateFsAsStr(const char *s) {
    return (v64typed){ .val = value64_createfs_asstr(s), .typ = VALUE64_FS };
}
static inline v64typed              v64typedCreateFILE(FILE *file) {
    return (v64typed){ .val = LITERAL64_FILE(file), .typ = VALUE64_FILE };
}
static inline v64typed              v64typedCreateUnk(void) {
    return (v64typed){ .val = LITERAL64_ZERO, .typ = VALUE64_UNKNOWN };
}
static inline v64typed              v64typedClone(v64typed v) {
    return (v64typed) { .val = value64_clone(v.val, v.typ), .typ = v.typ };
}
//  move constructor
static inline v64typed              v64typedMove(v64typed *v) {
    v64typed   res = { .val = value64_move(&v->val, v->typ), .typ = v->typ};
    v->typ = VALUE64_UNKNOWN;
    return res;
}

static inline void                  v64typedFree(v64typed *vt) {
    if (vt) {
        value64_free(&vt->val, vt->typ);
        *vt = V64TYPEDZERO();
    }
}

// -------------------- ACCESS AND MODIFICATORS -------------------------------------

// getters - for all types
static inline int               v64typeGetInt(v64typed tv) {
    if (tv.typ == VALUE64_INT)
        return value64_int(tv.val);
    else
        return userraise(0, ERR_TYPES_MISMATCH, "Expected INT but not %d %s", tv.typ, value64_typename(tv.typ) );
}
static inline long              v64typeGetLong(v64typed tv) {
    if (tv.typ == VALUE64_LONG)
        return value64_long(tv.val);
    else
        return userraise(0L, ERR_TYPES_MISMATCH, "Expected LONG but not %d %s", tv.typ, value64_typename(tv.typ) );
}
static inline unsigned long     v64typeGetULong(v64typed tv) {
    if (tv.typ == VALUE64_ULONG)
        return value64_ulong(tv.val);
    else
        return userraise(0UL, ERR_TYPES_MISMATCH, "Expected ULONG but not %d %s", tv.typ, value64_typename(tv.typ) );
}
static inline bool              v64typeGetBool(v64typed tv) {
    if (tv.typ == VALUE64_BOOL)
        return value64_bool(tv.val);
    else
        return userraise(false, ERR_TYPES_MISMATCH, "Expected BOOL but not %d %s", tv.typ, value64_typename(tv.typ) );
}
static inline char              v64typeGetChar(v64typed tv) {
    if (tv.typ == VALUE64_CHR)
        return value64_char(tv.val);
    else
        return userraise(0, ERR_TYPES_MISMATCH, "Expected CHR but not %d %s", tv.typ, value64_typename(tv.typ) );
}
static inline double            v64typeGetDouble(v64typed tv) {
    if (tv.typ == VALUE64_DBL)
        return value64_dbl(tv.val);
    else
        return userraise(0.0, ERR_TYPES_MISMATCH, "Expected DBL but not %d %s", tv.typ, value64_typename(tv.typ) );
}
static inline char             *v64typeGetStr(v64typed tv) {
    if (tv.typ == VALUE64_STR)
        return value64_str(tv.val);
    else
        return userraise(NULL, ERR_TYPES_MISMATCH, "Expected STR but not %d %s", tv.typ, value64_typename(tv.typ) );
}
static inline fs               *v64typeGetFs(v64typed tv) {
    if (tv.typ == VALUE64_FS)
        return value64_fs(tv.val);
    else
        return userraise(NULL, ERR_TYPES_MISMATCH, "Expected FS but not %d %s", tv.typ, value64_typename(tv.typ) );
}

// nvl for STR
static inline const char*
v64typedNvlStr(v64typed tv, const char *default_fmt)
{
    if ( (tv.typ == VALUE64_STR) && !strisempty(value64_str(tv.val) ) )
            return value64_str(tv.val);
    else 
        return default_fmt;
}
// nvl for FS
static inline const char*
v64typedNvlFs(v64typed tv, const char *restrict default_fmt) {

    if ( tv.typ == VALUE64_FS && !fs_isempty(value64_fs(tv.val) ) )
            return fs_str(value64_fs(tv.val) );
    else
        return logsimpleerr(default_fmt, "Incorrent type %d %s, FS expected", tv.typ, value64_typename(tv.typ) );
}

static inline bool
v64typedBoolNegative(v64typed *tv) {
    switch (tv->typ) {
        case VALUE64_BOOL:
            tv->val.bval = !tv->val.bval;
            break;
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, 
                "Unsupported typ %d %s", tv->typ, value64_typename(tv->typ) );
    }
    return true;
}

// note only int value to add, but generally it's ok
// no OF checking version
static inline bool
v64typedAdd(v64typed *tv, int value) {
    switch (tv->typ) {
        case VALUE64_INT:
            tv->val.ival += value;
            break;
        case VALUE64_LONG:
            tv->val.lval += (long) value;
            break;
        case VALUE64_ULONG:
            tv->val.ulval += (unsigned long) value;
            break;
        case VALUE64_CHR:
            tv->val.cval += (unsigned char) value;
            break;
        case VALUE64_DBL:
            tv->val.dval += (double) value;
            break;
        case VALUE64_BOOL:
            v64typedBoolNegative(tv);   // true -> false -> true...
            break;
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, 
                "Unsupported typ %d %s", tv->typ, value64_typename(tv->typ) );
    }
    return true;
}





/* type cast as int - EXAMPLE, must be generated via marco
#define v64_cast(tval, type) _Generic((type)0, \
    int: v64typedCastToInt(tval), \
    long: v64typedCastToLong(tval), \
    unsigned long: v64typedCastToULong(tval), \
    double: v64typedCastToDouble(tval) \
)

// Использование:
int i = v64_cast(my_v64, int); 
// --------------------------------------------------------------
// Макрос для генерации функций приведения типов
#define V64_CAST_FUNC(return_type, func_name) \
static inline return_type func_name(v64typed tval) { \
    return_type res = 0; \
    switch (tval.typ) { \
        case VALUE64_INT:   res = value64_int(tval.val);   break; \
        case VALUE64_LONG:  res = value64_long(tval.val);  break; \
        case VALUE64_ULONG: res = value64_ulong(tval.val); break; \
        case VALUE64_CHR:   res = value64_char(tval.val);   break; \
        case VALUE64_BOOL:  res = value64_bool(tval.val);   break; \
        case VALUE64_DBL:   res = value64_dbl(tval.val);    break; \
        default: break; \
    } \
    return res; \
}

// Генерация функций
V64_CAST_FUNC(int, v64typedCastToInt)
V64_CAST_FUNC(long, v64typedCastToLong)
V64_CAST_FUNC(unsigned long, v64typedCastToULong)
V64_CAST_FUNC(double, v64typedCastToDouble)

#undef V64_CAST_FUNC
*/


// TODO: refactor that!!
static inline int
v64typedCastToInt(v64typed tval) {
    // NOTE: no range checking for now TODO: use checkers from value64 converters
    int     ival = 0;
    switch (tval.typ) {
        case VALUE64_INT:
            ival = value64_int(tval.val);
            break;
        case VALUE64_LONG:
            ival = value64_long(tval.val);
            break;
        case VALUE64_ULONG:
            ival = value64_ulong(tval.val);
            break;
        case VALUE64_CHR:
            ival = value64_char(tval.val);
            break;
        case VALUE64_BOOL:
            ival = value64_bool(tval.val);
            break;
        case VALUE64_DBL:
            ival = value64_dbl(tval.val);
            break;
        default:
    }
    return ival;
}

// TODO: refactor that!! TEMPORARY
static inline int
v64typedCastToLong(v64typed tval) {
    // NOTE: no range checking for now TODO: use checkers from value64 converters
    long     lval = 0;
    switch (tval.typ) {
        case VALUE64_INT:
            lval = value64_int(tval.val);
            break;
        case VALUE64_LONG:
            lval = value64_long(tval.val);
            break;
        case VALUE64_ULONG:
            lval = value64_ulong(tval.val);
            break;
        case VALUE64_CHR:
            lval = value64_char(tval.val);
            break;
        case VALUE64_BOOL:
            lval = value64_bool(tval.val);
            break;
        case VALUE64_DBL:
            lval = value64_dbl(tval.val);
            break;
        default:
    }
    return lval;
}



// ------------------------ PRINTERS/CHECKERS ---------------------------------------

extern int                      v64typedTechfprint(FILE *restrict out, v64typed tval, const char *restrict name);
static inline int               v64typedTechprint(v64typed tval, const char * name) {
    return v64typedTechfprint(stdout, tval, name);
}

#define V64TYPED_TECHFPRINT(out, val, typ)   v64typedTechfprint( (out), (val), #val)
#define V64TYPED_TECHPRINT(val, typ)         v64typedTechprint((val), #val)

// ------------------------------------ ETC. ----------------------------------------
extern bool                     v64typedValidate(FILE *out, v64typed tval);

#endif /* _V64TYPED_H */
