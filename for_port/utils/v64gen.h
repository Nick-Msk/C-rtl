/**
 * @file v64gen.h
 * @brief v64 Generator!
 */

#ifndef _V64GEN_H
#define _V64GEN_H

// ---------------------------------------------------------------------------------
// ------------------------------ Public value64 API -------------------------------
// ---------------------------------------------------------------------------------

// ----------------------------- Includes ------------------------------------------

#include <stdio.h>
#include "value64.h"
#include "v64typed.h"
#include "error.h"
#include "getword.h"

// --------------------------------- CONSTANTS AND GLOBALS --------------------------

enum v64GenConstants {
                                        V64GENCOUNT = 4
};

// ---------------------------------- TYPES -----------------------------------------


typedef struct v64Gen                   v64Gen;

typedef value64                         (*v64GenFunc)(v64Gen *gen);
typedef off_t                           (*v64GenUpdaterFunc) (v64Gen *gen);
typedef value64                         (*v64GenFinalizerFunc)(v64Gen *gen);

typedef struct v64Gen {
    v64GenFunc              fnext;
    value64_type            type;
    off_t                   limit;
    off_t                   position;  // remaned from counter 
    // func
    v64GenUpdaterFunc       updater;
    // register for common usage
    v64typed                data[V64GENCOUNT];
} v64Gen;

// for common API
typedef v64Gen          (*v64GenTypedFactory)(/*value64_type vt, */long count, long start, int increment);

// -------------------------- Registry Support API -----------------------------------

// now shortcut only for 4 registers
static inline value64              *v64GenReg0(v64Gen *gen) {
    return &gen->data[0].val;
}
#define V64GENREG0(gen) (gen->data[0])
#define V64GENREGVAL0(gen) V64GENREG0(gen).val

static inline value64              *v64GenReg1(v64Gen *gen) {
    return &gen->data[1].val;
}
#define V64GENREG1(gen) (gen->data[1])
#define V64GENREGVAL1(gen) V64GENREG1(gen).val

static inline value64              *v64GenReg2(v64Gen *gen) {
    return &gen->data[2].val;
}
#define V64GENREG2(gen) (gen->data[2])
#define V64GENREGVAL2(gen) V64GENREG2(gen).val

static inline value64              *v64GenReg3(v64Gen *gen) {
    return &gen->data[3].val;
}
#define V64GENREG3(gen) (gen->data[3])
#define V64GENREGVAL3(gen) V64GENREG3(gen).val

// ---------------------------- Unility -------------------------------
// internal, no-checking
static inline void              v64GenFixRndinc(int *rndinc) {
    if (*rndinc < 1)
        *rndinc = 1;
}

// ------------------------- CONSTRUCTOTS/DESTRUCTORS -------------------------------

extern v64Gen                       v64GenInit(v64GenFunc func, value64_type type, 
                                               off_t limit,
                                               v64typed initdata1, v64typed initdata2,
                                               v64typed initdata3, v64typed initdata4);
static inline v64Gen                v64GenInit0(v64GenFunc func, value64_type type, off_t limit) {
    return v64GenInit(func, type, limit,
        V64TYPEDZERO(), V64TYPEDZERO(), V64TYPEDZERO(), V64TYPEDZERO());
}  
static inline v64Gen                v64GenInit1(v64GenFunc func, value64_type type, 
                                                off_t limit, v64typed initdata1) {
    return v64GenInit(func, type, limit, initdata1, 
        V64TYPEDZERO(), V64TYPEDZERO(), V64TYPEDZERO());
}    
static inline v64Gen                v64GenInit2(v64GenFunc func, value64_type type, 
                                                off_t limit,
                                                v64typed initdata1, 
                                                v64typed initdata2) {
    return v64GenInit(func, type, limit, initdata1, initdata2, 
        V64TYPEDZERO(), V64TYPEDZERO());
}    
static inline v64Gen                v64GenInit3(v64GenFunc func, value64_type type, 
                                                off_t limit, v64typed initdata1, 
                                                v64typed initdata2, v64typed initdata3) {
    return v64GenInit(func, type, limit,
            initdata1, initdata2, initdata3, V64TYPEDZERO());
} 
static inline v64Gen                v64GenInit4(v64GenFunc func, value64_type type, 
                                                off_t limit, v64typed initdata1, 
                                                v64typed initdata2, v64typed initdata3, 
                                                v64typed initdata4) {
    return v64GenInit(func, type, limit, initdata1, initdata2, initdata3, initdata4);
} 
                     
static inline void                  v64GenFree(v64Gen *gen) {
    if (gen) {
        for (int i = 0; i < V64GENCOUNT; i++)
            v64typedFree(&gen->data[i]);    // even if data = 0LL
    }
}

static inline v64Gen                v64GenZero(void) {
    return (v64Gen) {
        .fnext      = NULL,
        .type       = VALUE64_UNKNOWN,
        .position   = 0L,
        .limit      = 0L,  // -1 no limit, 0 out of lim
        .updater  = NULL,
        .data = {
            [0] = { .val = LITERAL64_ZERO, .typ = VALUE64_UNKNOWN },
            [1] = { .val = LITERAL64_ZERO, .typ = VALUE64_UNKNOWN },
            [2] = { .val = LITERAL64_ZERO, .typ = VALUE64_UNKNOWN },
            [3] = { .val = LITERAL64_ZERO, .typ = VALUE64_UNKNOWN }
        }
    };
}

#define V64GENFREE(gen) { v64GenFree(gen); gen = v64GenZero(); }

// -------------------- ACCESS AND MODIFICATORS -------------------------------------

// -------------------------- GENERALLIZED ACCESS ------------------------------
extern value64                  v64GenNext(v64Gen *gen);
extern value64                  v64GenCurr(v64Gen *gen);
extern bool                     v64GenHasnext(const v64Gen *gen);
extern bool                     v64GenGetIfHasnext(v64Gen *restrict gen, value64 *restrict val);

// ----------------------- GENERATORS (extern all) ---------------------------
// ------------------ pre-created func V64 typed -----------------------------

// truly genenarator constrctors
extern value64                  v64GenUnlimNull(v64Gen *gen);
// zero/empty
extern value64                  v64GenUnlimZero(v64Gen *gen);
// unchecker group. 0 -> 1 -> 2 ... INT_MAX -> INT_MIN etc...
// for bool false -> true -> false ...
// EXCEPT double. That type w/o cycling.
extern value64                  v64GenAscSeries(v64Gen *gen);
extern value64                  v64GenAscRnd(v64Gen *gen);
extern value64                  v64GenDescSeries(v64Gen *gen);
extern value64                  v64GenDescRnd(v64Gen *gen);
extern value64                  v64GenRandom(v64Gen *gen);

// SOURCE based genenarator constrctors
// source (DS or c-str or FILE *) group
extern value64                  v64GenStringToChar(v64Gen *gen);
extern value64                  v64GenFSToChar(v64Gen *gen);
/**
 * @brief Parse fs from fs, dividev by newline
 * @note
 * @return: value64/fs
 * @note
 * data[0] – PTR to source fs (non-owning)
 */
extern value64                  v64GenFSToFsByNewline(v64Gen *gen);
/**
 * @brief Parse fs from fs, dividev by newline
 * @note
 * @return: value64/str
 * @note
 * data[0] – PTR to source fs (non-owning)
 */
extern value64                  v64GenFSToStrByNewline(v64Gen *gen);
/**
 * @brief File stream generator
 *
 * data[0] – PTR на FILE* (невладеющий)
 */
extern value64                  v64GenFileToChar(v64Gen *gen);
/**
 * @brief File stream generator
 *
 * data[0] – PTR на FILE* (невладеющий)
 * 
 * data[2] = fs as buf (owner)
 */
extern value64                  v64GenFileToFsByNewline(v64Gen *gen);
/**
 * @brief File stream generator
 *
 * data[0] – PTR на FILE* (невладеющий)
 * 
 * data[2] = fs as buf (owner)
 */
extern value64                  v64GenFileToStrByNewline(v64Gen *gen);

// ---------------------------- CONSTRUCTORS ---------------------------------------
// ----------------------------------------------------------------------------------
// ----------------------- truly genenarators (NON SOURCE) -----------------------------------

// limited NULL (fs & str only)
static inline v64Gen                v64GenCreatorNull(value64_type rettyp, off_t limit) {
    if (rettyp != VALUE64_FS && rettyp != VALUE64_STR)
        userraiseint(ERR_UNSUPPORTED_TYPE, "Type %d not supported", rettyp);
    
    // REGITRSY ALLOCATION: NONE
    return v64GenInit0(v64GenUnlimNull, rettyp, limit);
}
// unlimited
static inline v64Gen                v64GenCreatorUnlimNull(value64_type rettyp) {
    return v64GenCreatorNull(rettyp, -1L);
}
// limited ZERO
static inline v64Gen            v64GenCreatorZero(value64_type rettyp, off_t limit) {
    // not sure what to do, now just raiseint
    if (!value64_checktype(rettyp))
        userraiseint(ERR_UNSUPPORTED_TYPE, "Type %d not supported", rettyp);

    // REGITRSY ALLOCATION: NONE
    return v64GenInit0(v64GenUnlimZero, rettyp, limit);       // quite simple, LOL
}
// unlimited ZERO
static inline v64Gen            v64GenCreatorUnlimZero(value64_type rettyp) {
    return v64GenCreatorZero(rettyp, -1L);
}
/*
// REGITRSY ALLOCATION:
// data[0] as c-str for STR
static inline v64Gen            v64GenCreatorUnlimStrValue(const char *str) {
    return v64GenInit1(v64GenUnlimZero, VALUE64_STR, v64typedCreateStr(str));       // quite simple, LOL
}
// REGITRSY ALLOCATION:
// data[0]  value for LONG
static inline v64Gen            v64GenCreatorUnlimValue(long val) {
    return v64GenInit1(v64GenUnlimZero, VALUE64_LONG, v64typedCreateLong(val));       // quite simple, LOL
}
// REGITRSY ALLOCATION:
// data[0]  value for DBL
static inline v64Gen            v64GenCreatorUnlimDouble(double val) {
    return v64GenInit1(v64GenUnlimZero, VALUE64_DBL, v64typedCreateDbl(val));       // quite simple, LOL
} */

// ---------------------------- CONSTRUCTORS ACS SERIES ------------------------------------------------

// REGITRSY ALLOCATION:
// data[0] LONG as startpos for numeric generator
// data[1] STR as pattern for printing
// LIMITED ACS
static inline v64Gen            v64GenCreatorAscFsSeries(off_t limit, long startpos, const char *fmt) {
    v64Gen tmp;
    if (fmt != NULL)
        tmp = v64GenInit2(v64GenAscSeries,
                        VALUE64_FS, limit,
                        v64typedCreateLong(startpos), v64typedCreateStr(fmt) );
    else
        tmp = v64GenInit1(v64GenAscSeries, 
                        VALUE64_FS, limit,
                        v64typedCreateLong(startpos));
    return tmp;
}
static inline v64Gen            v64GenCreatorUnlimAscFsSeries(long startpos, const char *fmt) {
    return v64GenCreatorAscFsSeries(-1L, startpos, fmt);
}

// REGITRSY ALLOCATION:
// data[0] LONG as startpos for numeric generator
// data[1] STR as pattern for printing
// LIMITED
static inline v64Gen            v64GenCreatorAscStrSeries(off_t limit, long startpos, const char *fmt) {
    v64Gen tmp;
    if (fmt != NULL)
        tmp = v64GenInit2(v64GenAscSeries,
                        VALUE64_STR, limit,
                        v64typedCreateLong(startpos), v64typedCreateStr(fmt) );
    else
        tmp = v64GenInit1(v64GenAscSeries, 
                        VALUE64_STR, limit,
                        v64typedCreateLong(startpos));
    return tmp;
}
// UNLIMITED
static inline v64Gen            v64GenCreatorUnlimAscStrSeries(long startpos, const char *fmt) {
    return  v64GenCreatorAscStrSeries(-1L, startpos, fmt);
} 

// REGITRSY ALLOCATION:
// data[0] <PARAM VT TYPE> as startpos for numeric asc generator
// LIMITED
static inline v64Gen            v64GenCreatorAscSeries(v64typed vt, off_t limit) {
    value64_type rettyp = vt.typ;     // that is it!!!

    if (int_notin(rettyp, VALUE64_BOOL, VALUE64_CHR, VALUE64_INT, VALUE64_LONG, VALUE64_ULONG, VALUE64_DBL))
        userraiseint(ERR_UNSUPPORTED_TYPE, "Type %d %s not supported by %s", 
                rettyp, value64_typename(rettyp), __func__);

    v64Gen res = v64GenInit1(v64GenAscSeries, rettyp, limit, vt);
    return res;
}
// UNLIMITED
static inline v64Gen            v64GenCreatorUnlimAscSeries(v64typed vt) {
    return v64GenCreatorAscSeries(vt, -1L);
}

// -------------------------------------------- DESC SERIES ------------------------------------------------

// REGITRSY ALLOCATION:
// data[0] LONG as startpos for numeric desc generator
// data[1] STR as pattern for printing str ONLY
// LIMITED
static inline v64Gen            v64GenCreatorDescFsSeries(off_t limit, long startpos, const char *fmt) {
    v64Gen tmp;
    if (fmt != NULL)
        tmp = v64GenInit2(v64GenDescSeries,
                        VALUE64_FS, limit,
                        v64typedCreateLong(startpos), v64typedCreateStr(fmt) );
    else
        tmp = v64GenInit1(v64GenDescSeries, 
                        VALUE64_FS, limit,
                        v64typedCreateLong(startpos));
    return tmp;
}
// UNLIMITED
static inline v64Gen            v64GenCreatorUnlimDescFsSeries(long startpos, const char *fmt) {
    return v64GenCreatorDescFsSeries(-1L, startpos, fmt);
}
// REGITRSY ALLOCATION:
// data[0] LONG as startpos for numeric desc generator
// data[1] STR as pattern for printing STR ONLY
// LIMITED
static inline v64Gen            v64GenCreatorDescStrSeries(off_t limit, long startpos, const char *fmt) {
    v64Gen tmp;
    if (fmt != NULL)
        tmp = v64GenInit2(v64GenDescSeries,
                        VALUE64_STR, limit,
                        v64typedCreateLong(startpos), v64typedCreateStr(fmt) );
    else
        tmp = v64GenInit1(v64GenDescSeries, 
                        VALUE64_STR, limit,
                        v64typedCreateLong(startpos));
    return tmp;
}
// UNLIMITED
static inline v64Gen            v64GenCreatorUnlimDescStrSeries(long startpos, const char *fmt) {
    return v64GenCreatorDescStrSeries(-1L, startpos, fmt);
}
// REGITRSY ALLOCATION:
// data[0] <PARAM VT TYPE> as startpos for numeric asc generator
// LIMITED
static inline v64Gen        v64GenCreatorDescSeries(v64typed vt, off_t limit) {
    value64_type rettyp = vt.typ;     // that is it!!!

    if (int_notin(rettyp, VALUE64_BOOL, VALUE64_CHR, VALUE64_INT, VALUE64_LONG, VALUE64_ULONG, VALUE64_DBL))
        userraiseint(ERR_UNSUPPORTED_TYPE, "Type %d %s not supported by %s", 
                rettyp, value64_typename(rettyp), __func__);

    v64Gen res = v64GenInit1(v64GenDescSeries, 
                    rettyp, limit, vt);
    return res;
}
// UNLIMITED
static inline v64Gen        v64GenCreatorUnlimDescSeries(v64typed vt) {
    return v64GenCreatorDescSeries(vt, -1L);
}

// ------------------------------------ ASC RANDOM SERIES -----------------------------------------------

// Внутренний конструктор для AscRnd (уже с limit).
static inline v64Gen
v64GenCreatorAscRndCommon(value64_type typ, off_t limit, v64typed reg0, v64typed reg1, int rndinc)
{
    v64GenFixRndinc(&rndinc);
    return v64GenInit3(v64GenAscRnd, typ, limit,
                       reg0,
                       reg1,
                       v64typedCreateInt(rndinc));
}

// ---------- FS ----------
// LIMITED
static inline v64Gen
v64GenCreatorAscFsRnd(off_t limit, long startpos, const char *fmt, int rndinc)
{
    return v64GenCreatorAscRndCommon(
        VALUE64_FS, limit,
        v64typedCreateLong(startpos),
        fmt ? v64typedCreateStr(fmt) : V64TYPEDZERO(),
        rndinc);
}
// UNLIMITED
static inline v64Gen
v64GenCreatorUnlimAscFsRnd(long startpos, const char *fmt, int rndinc)
{
    return v64GenCreatorAscFsRnd(-1L, startpos, fmt, rndinc);
}

// ---------- STR ----------
// LIMITED
static inline v64Gen
v64GenCreatorAscStrRnd(off_t limit, long startpos, const char *fmt, int rndinc)
{
    return v64GenCreatorAscRndCommon(
        VALUE64_STR, limit,
        v64typedCreateLong(startpos),
        fmt ? v64typedCreateStr(fmt) : V64TYPEDZERO(),
        rndinc);
}
// UNLIMITED
static inline v64Gen
v64GenCreatorUnlimAscStrRnd(long startpos, const char *fmt, int rndinc)
{
    return v64GenCreatorAscStrRnd(-1L, startpos, fmt, rndinc);
}

// ---------- Numeric types ----------
// LIMITED
static inline v64Gen
v64GenCreatorAscRnd(v64typed vt, off_t limit, int rndinc)
{
    value64_type rettyp = vt.typ;
    if (int_notin(rettyp, VALUE64_BOOL, VALUE64_CHR, VALUE64_INT,
                  VALUE64_LONG, VALUE64_ULONG, VALUE64_DBL))
        userraiseint(ERR_UNSUPPORTED_TYPE, "Type %d %s not supported by %s",
                     rettyp, value64_typename(rettyp), __func__);

    return v64GenCreatorAscRndCommon(rettyp, limit, vt, V64TYPEDZERO(), rndinc);
}
// UNLIMITED
static inline v64Gen
v64GenCreatorUnlimAscRnd(v64typed vt, int rndinc)
{
    return v64GenCreatorAscRnd(vt, -1L, rndinc);
}

// ------------------------------------ DESC RANDOM SERIES -----------------------------------------------

// REGITRSY ALLOCATION:
// data[0] LONG as startpos for numeric desc random generator
// data[1] STR as pattern for printing
// data[2] INT as pattern for increment
static inline v64Gen
v64GenCreatorDescRndCommon(value64_type typ, off_t limit, v64typed reg0, v64typed reg1, int rndinc)
{
    v64GenFixRndinc(&rndinc);
    return v64GenInit3(v64GenDescRnd, typ, limit,
                       reg0,
                       reg1,
                       v64typedCreateInt(rndinc));
}

// ---------- FS ----------
// LIMITED
static inline v64Gen
v64GenCreatorDescFsRnd(off_t limit, long startpos, const char *fmt, int rndinc)
{
    return v64GenCreatorDescRndCommon(
        VALUE64_FS, limit,
        v64typedCreateLong(startpos),
        fmt ? v64typedCreateStr(fmt) : V64TYPEDZERO(),
        rndinc);
}
// UNLIMITED
static inline v64Gen
v64GenCreatorUnlimDescFsRnd(long startpos, const char *fmt, int rndinc)
{
    return v64GenCreatorDescFsRnd(-1L, startpos, fmt, rndinc);
}

// ---------- STR ----------
// LIMITED
static inline v64Gen
v64GenCreatorDescStrRnd(off_t limit, long startpos, const char *fmt, int rndinc)
{
    return v64GenCreatorDescRndCommon(
        VALUE64_STR, limit,
        v64typedCreateLong(startpos),
        fmt ? v64typedCreateStr(fmt) : V64TYPEDZERO(),
        rndinc);
}
// UNLIMITED
static inline v64Gen
v64GenCreatorUnlimDescStrRnd(long startpos, const char *fmt, int rndinc)
{
    return v64GenCreatorDescStrRnd(-1L, startpos, fmt, rndinc);
}

// ---------- Numeric ----------
// LIMITED
static inline v64Gen
v64GenCreatorDescRnd(v64typed vt, off_t limit, int rndinc)
{
    value64_type rettyp = vt.typ;
    if (int_notin(rettyp, VALUE64_BOOL, VALUE64_CHR, VALUE64_INT,
                  VALUE64_LONG, VALUE64_ULONG, VALUE64_DBL))
        userraiseint(ERR_UNSUPPORTED_TYPE, "Type %d %s not supported by %s",
                     rettyp, value64_typename(rettyp), __func__);

    return v64GenCreatorDescRndCommon(rettyp, limit, vt, V64TYPEDZERO(), rndinc);
}
// UNLIMITED
static inline v64Gen
v64GenCreatorUnlimDescRnd(v64typed vt, int rndinc) 
{
    return v64GenCreatorDescRnd(vt, -1L, rndinc);
}

// ------------------------------------ JUST RANDOM -----------------------------------------------
// REGITRSY ALLOCATION:
// data[0] INT as lim for rnsint

// ----------------- ВАРИАНТ С RNDINC ПАРАМЕТРОМ -----------------
static inline v64Gen
v64GenCreatorRndCommon(value64_type typ, off_t limit, int rndinc, v64typed reg1)
{
    v64GenFixRndinc(&rndinc);
    return v64GenInit2(v64GenRandom, typ, limit,
                       v64typedCreateInt(rndinc),
                       reg1);
}

// ---------- Numeric (INT, LONG, ULONG, DBL, CHR, BOOL) ----------
static inline v64Gen
v64GenCreatorRnd(value64_type rettyp, off_t limit, int rndinc)
{
    if (int_notin(rettyp, VALUE64_BOOL, VALUE64_CHR, VALUE64_INT,
                  VALUE64_LONG, VALUE64_ULONG, VALUE64_DBL))
        userraiseint(ERR_UNSUPPORTED_TYPE, "Type %d %s not supported by %s",
                     rettyp, value64_typename(rettyp), __func__);

    return v64GenCreatorRndCommon(rettyp, limit, rndinc, V64TYPEDZERO());
}
static inline v64Gen
v64GenCreatorUnlimRnd(value64_type rettyp, int rndinc)
{
    return v64GenCreatorRnd(rettyp, -1L, rndinc);
}

// ---------- STR ----------
static inline v64Gen
v64GenCreatorStrRnd(off_t limit, const char *fmt, int rndinc)
{
    return v64GenCreatorRndCommon(
        VALUE64_STR, limit, rndinc,
        fmt ? v64typedCreateStr(fmt) : V64TYPEDZERO());
}
static inline v64Gen
v64GenCreatorUnlimStrRnd(const char *fmt, int rndinc)
{
    return v64GenCreatorStrRnd(-1L, fmt, rndinc);
}

// ---------- FS ----------
static inline v64Gen
v64GenCreatorFsRnd(off_t limit, const char *fmt, int rndinc)
{
    return v64GenCreatorRndCommon(
        VALUE64_FS, limit, rndinc,
        fmt ? v64typedCreateStr(fmt) : V64TYPEDZERO());
}
static inline v64Gen
v64GenCreatorUnlimFsRnd(const char *fmt, int rndinc)
{
    return v64GenCreatorFsRnd(-1L, fmt, rndinc);
}
// --------------------------- Creator from Source (c-str, FILE *, DS *) series ------------

// RETURNS: VALUE64/CHR
// REGITRSY ALLOCATION:
// data[0] STR as SOURCE (no ownership)
extern v64Gen                   v64GenCreatorSourceCstrToChar(const char *src, long maxlen);

// RETURNS: VALUE64/CHR
// REGITRSY ALLOCATION:
// data[0] FS as SOURCE (no ownership)
extern v64Gen                   v64GenCreatorSourceFsToChar(const fs *src);

// RETURNS: VALUE64/FS
// REGITRSY ALLOCATION:
// data[0] FS as SOURCE (no ownership)
extern v64Gen                   v64GenCreatorSourceFsToFsByNewline(const fs *src);

// RETURNS: VALUE64/STR
// REGITRSY ALLOCATION:
// data[0] FS as SOURCE (no ownership)
extern v64Gen                   v64GenCreatorSourceFsToStrByNewline(const fs *src);
// RETURNS: VALUE64/CHR
// REGITRSY ALLOCATION:
// data[0] FILE as SOURCE (no ownership)
extern v64Gen                   v64GenCreatorSourceFileToChar(FILE *file);
// RETURNS: VALUE64/FS
// REGITRSY ALLOCATION:
// data[0] FILE * (no ownership)
// 
// data[2] FS as buf (owner)
extern v64Gen                   v64GenCreatorSourceFileToFsByNewline(FILE *src);
// RETURNS: VALUE64/STR
// REGITRSY ALLOCATION:
// data[0] FILE * (no ownership)
// 
// data[2] FS as buf (owner)
extern v64Gen                   v64GenCreatorSourceFileToStrByNewline(FILE *src);

// --------------------------------- Indirect API --------------------------------------

static inline unsigned long     v64GenGetRemainingCount(v64Gen *gen) {
    invraisecode(gen != NULL, ERR_NULLABLE_PTR, "NUll gen");

    return gen->limit;
}

// ------- Stream checker -------
static inline unsigned long     v64GenUpdateLimit(v64Gen *gen) {
    invraisecode(gen != NULL, ERR_NULLABLE_PTR, "NUll gen");

    if (gen->updater)
        return gen->updater(gen);
    else
        //return userraise(-1L, ERR_UNSUPPORTED_GENERATOR, "Only sereval SOURCE generators support GetRemaining");
        return gen->limit;
}
// ----- Source stream finallizer -------------
// static inline value64           v64GenFinalize(v64Gen *gen) {
//     invraisecode(gen != NULL, ERR_NULLABLE_PTR, "NUll gen");

//     if (gen->finalizer)
//         return gen->finalizer(gen);
//     else
//         return userraise(LITERAL64_ZERO, ERR_UNSUPPORTED_GENERATOR, "Only sereval SOURCE generators support GenFinalize");
// }

// ------------------------ PRINTERS/CHECKERS ---------------------------------------

extern int                      v64GenTechfprint(FILE *restrict out, const v64Gen *restrict gen, const char *restrict name);
static inline int               v64GenTechprint(const v64Gen *restrict gen, const char *restrict name) {
    return v64GenTechfprint(stdout, gen, name);
}

#define                         V64GENTECHFPRINT(out, gen) v64GenTechfprint( (out), &(gen), #gen)
#define                         V64GENTECHPRINT(gen) v64GenTechprint( &(gen), #gen)

// ------------------------------------ ETC. ----------------------------------------

extern bool                     v64GenValidate(FILE *restrict out, const v64Gen *restrict gen);

#endif /* !_VALUE64GEN_H */
