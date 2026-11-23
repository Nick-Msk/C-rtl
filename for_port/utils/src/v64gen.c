/********************************************************************
                    VALUE64GEN MODULE IMPLEMENTATION
********************************************************************/

#include "v64gen.h"

// --------------------------------- CONSTANTS AND GLOBALS --------------------------

static const fs                 V64GEN_NULL_FS = FSEMPTY;

// --------------------------- Utilities --------------------------------------------

static value64           
v64GenFileToStringTargetByNewline(v64Gen *gen);

static inline bool
haslimit(off_t remaining) {
    return remaining == -1L || remaining > 0;
}

static inline void 
v64GenAdvance(v64Gen *gen, off_t steps)
{
    gen->position += steps;
    if (gen->limit > 0) {
        gen->limit -= steps;
        if (gen->limit < 0)
            gen->limit = 0;
    }
}
// for byNewline generators
static inline void
v64GenAdvanceByLine(v64Gen *gen, off_t delta)
{
    gen->position += delta;
    if (gen->limit > 0) {
        gen->limit -= delta;
        if (gen->limit < 0)
            gen->limit = 0;
    }
}

// ----------------------- Utilities REMAINING -----------------------------------------

// for fs source
static off_t            
v64GenFsUpdateCount(v64Gen *gen) {
    const fs *src = V64GENREGVAL0(gen).fsval;    // REG
    if (!src->v)
        return userraise(-1L, ERR_NULLABLE_PTR, "Nullable fs-source");
    
    if (fs_len(src) < (size_t) gen->position)   // log the issue
        userraise(-1L, ERR_STREAM_ERROR, 
            "fs source reduced!!! from %lld to %zu", gen->position, fs_len(src));

    // update limit
    gen->limit = fs_len(src) - gen->position;

    return gen->limit;
}
// for c-str source
static off_t            
v64GenStrUpdateCount(v64Gen *gen) {
    const char *src = V64GENREGVAL0(gen).sval;    // REG
    if (!src)
        return userraise(-1L, ERR_NULLABLE_PTR, "Nullable cstr-source");
    
    // update limit to unlimit, because we have no idea where the end ('\0)
    gen->limit = -1L;

    return gen->limit;
}

// for FILE * source
static off_t
v64GenFileUpdateCount(v64Gen *gen)
{
    FILE *fp = (FILE *) V64GENREGVAL0(gen).pval;
    if (!fp)
        return userraiseint(ERR_NULLABLE_PTR, "Nullable FILE-source");

    off_t new_size = getfilesize(fp);

    if (new_size < 0)
        return userraise(new_size, ERR_IRREGULAR_STREAM, "Unable to get file size");

    off_t prev_size = V64GENREGVAL1(gen).ulval;   // предыдущий размер
    if (prev_size == 0 && new_size > 0) {
        off_t pos = ftell(fp);
        if (pos < 0)
            pos = 0;    //not sure
        gen->limit = (new_size > pos) ? (new_size - pos) : 0;
        V64GENREGVAL1(gen).ulval = new_size;
    } else {
        off_t delta = new_size - prev_size;

        if (delta < 0)
            return userraise(delta, ERR_STREAM_ERROR, "File size decreased");

        if (delta > 0) {
            clearerr(fp);   // если был EOF
            gen->limit += delta;                     // добавляем только новые байты
            V64GENREGVAL1(gen).ulval = new_size;     // обновляем предыдущий размер
        }
    }
    return gen->limit;
}

// ------------------------- CONSTRUCTOTS/DESTRUCTORS -------------------------------

v64Gen                          
v64GenInit(v64GenFunc func, value64_type type, off_t limit, 
            v64typed reg0, v64typed reg1, v64typed reg2, v64typed reg3) {
            
    invraisecode(func != NULL, ERR_NULLABLE_PTR, "Generation function can't be null");
    invraisecode(value64_checktype(type), ERR_UNSUPPORTED_TYPE, "value64 type %d isn't supported", type);

    v64Gen res = (v64Gen) {
        .fnext      = func,
        .type       = type,
        .position   = 0L,
        .limit      = limit,
        .updater    = NULL,
        .data     = { [0] = reg0, [1] = reg1, [2] = reg2, [3] = reg3 }
    };
    return res;
}

// --------------------------- Creator from Source (c-str, FILE *, DS *) series ------------

// RETURNS: CHR
// REGITRSY ALLOCATION:
// data[0] STR as SOURCE (no ownership)
v64Gen                          v64GenCreatorSourceCstrToChar(const char *src, long maxlen) {
    invraisecode(src != NULL, ERR_NULLABLE_PTR, "NUll src c-str");

    if (maxlen <= 0)
        maxlen = -1L; // unlim
    if (*src == '\0') {
        logsimple("Since src is empty limit is truncated to 0");
        maxlen = 0L;
    }

    v64Gen gen =  v64GenInit1(v64GenStringToChar, VALUE64_CHR,
                                maxlen,
                                v64typedCreateCstrSource(src));

    gen.updater = v64GenStrUpdateCount;

    return gen;
}

// RETURNS: FS/CHAR
// REGITRSY ALLOCATION:
// data[0] FS as SOURCE (no ownership)
static v64Gen                    v64GenCreatorSourceFsToCommonOutput(const fs *src, value64_type typ){
    invraisecode(src != NULL, ERR_NULLABLE_PTR, 
        "NUll fs source %p", src);
    invraisecode(typ == VALUE64_FS || typ == VALUE64_STR || typ == VALUE64_CHR, ERR_UNSUPPORTED_TYPE
        , "Type %d/%s isn't supported by %s", typ, value64_typename(typ), __func__);

    // determine generator
    v64GenFunc func = NULL;
    if (typ == VALUE64_FS)
        func = v64GenFSToFsByNewline;
    else if (typ == VALUE64_STR)
        func = v64GenFSToStrByNewline;
    else if (typ == VALUE64_CHR)
        func = v64GenFSToChar;
    // determine finallizer
    // v64GenFinalizerFunc final = NULL;
    // if (typ == VALUE64_FS)
    //     final = v64GenFSToFsByNewlineFinalize;
    // else if (typ == VALUE64_STR)
    //     final = v64GenFSToStrByNewlineFinalize;

    v64Gen gen = v64GenInit1(
                    func, 
                    typ,
                    src->len,     // not sure
                    v64typedCreateFsSource(src));    // data[0] PTR to fs

    //gen.finalizer   = NULL;
    gen.updater     = v64GenFsUpdateCount;
    return gen;
}


// RETURNS: CHR
// REGITRSY ALLOCATION:
// data[0] FS as SOURCE (no ownership)
v64Gen                          v64GenCreatorSourceFsToChar(const fs *src) {
    return v64GenCreatorSourceFsToCommonOutput(src, VALUE64_CHR);
}

// RETURNS: FS
// REGITRSY ALLOCATION:
// data[0] FS as SOURCE (no ownership)
v64Gen                          v64GenCreatorSourceFsToFsByNewline(const fs *src) {
    return v64GenCreatorSourceFsToCommonOutput(src, VALUE64_FS);
}
// RETURNS: STR
// REGITRSY ALLOCATION:
// data[0] FS as SOURCE (no ownership)
v64Gen                          v64GenCreatorSourceFsToStrByNewline(const fs *src) {
    return v64GenCreatorSourceFsToCommonOutput(src, VALUE64_STR);
}

/**
 * @brief Создаёт построчный генератор из FILE*.
 *
 * @param file  FILE * opened for r, not owned
 * @param typ   target type: VALUE64_FS или VALUE64_STR
 * @return      v64Gen
 * @note Регистры:
 *      data[0] = FILE* (PTR)
 *      data[1] = file size
 *      data[2] = fs as buf (owner)
 */
static v64Gen                   v64GenCreatorSourceFileToCommonOutput(FILE *file, value64_type typ)
{
    invraisecode(file != NULL, ERR_NULLABLE_PTR, "Null FILE*");
    invraisecode(typ == VALUE64_FS || typ == VALUE64_STR || typ == VALUE64_CHR, ERR_UNSUPPORTED_TYPE,
                 "Type %d/%s not supported", typ, value64_typename(typ));

    // determine generator
    v64GenFunc func = 
            typ == VALUE64_CHR ? v64GenFileToChar: v64GenFileToStringTargetByNewline;
    // determine finallizer
    // v64GenFinalizerFunc final = 
    //         typ == VALUE64_CHR ? NULL : v64GenFileToStringTargetByNewlineFinalize;

    v64Gen gen = v64GenInit3(func,
                             typ,
                             0L,        // Temporary
                             v64typedCreateFILE(file),
                             v64typedCreateULong(0UL),
                             typ == VALUE64_CHR ? v64typedCreateUnk() :
                                v64typedCreate(LITERAL64_PFS(fs_create()), VALUE64_FS) // TODO: do that normal
                             );

    // gen.finalizer   = NULL;
    gen.updater     = v64GenFileUpdateCount;   

    v64GenFileUpdateCount(&gen);   //  // fill limit and REG1

    return gen;
}

// RETURNS: VALUE64/FS
// REGITRSY ALLOCATION:
// data[0] FILE * (no ownership)
// data[2] FS as buf (owner)
v64Gen                          v64GenCreatorSourceFileToFsByNewline(FILE *src) {
    return v64GenCreatorSourceFileToCommonOutput(src, VALUE64_FS);
}
// RETURNS: VALUE64/STR
// REGITRSY ALLOCATION:
// data[0] FILE * (no ownership)
// data[2] FS as buf (owner)
v64Gen                           v64GenCreatorSourceFileToStrByNewline(FILE *src) {
    return v64GenCreatorSourceFileToCommonOutput(src, VALUE64_STR);
}

/**
 * @brief Creates a stream character generator for a FILE* source.
 *
 * The generator reads characters sequentially. The remaining count is
 * initialized from the current file position to the end of the file.
 *
 * data[0] – PTR to FILE* (non-owning)
 *
 * @param file  Open FILE* stream (must be readable)
 * @return      v64Gen object for character reading
 */
v64Gen                          v64GenCreatorSourceFileToChar(FILE *src) {
    return v64GenCreatorSourceFileToCommonOutput(src, VALUE64_CHR);
}

// -------------------- ACCESS AND MODIFICATORS -------------------------------------

/**
 * @brief Returns the next generated value and advances the generator.
 *
 * Calls gen->next(gen) which produces the value and increments gen->counter
 * as needed (for series generators).  The caller takes ownership of the
 * returned value.
 *
 * @param gen  pointer to the generator (must not be NULL)
 * @return     the next value64 of the generator's type
 */
value64                         
v64GenNext(v64Gen *gen) {
    return gen->fnext(gen);
}

bool
v64GenHasnext(const v64Gen *gen) {
    return gen->limit > 0 || gen->limit == -1L;
}

bool                              
v64GenGetIfHasnext(v64Gen *restrict gen, value64 *restrict val) {
    if (!v64GenHasnext(gen) )
        return false;
    value64 res = gen->fnext(gen);
    if (val)
        *val = res;
    return true;
}
// NOT IMPLEMENTED YET
value64                             
v64GenCurr(v64Gen *gen) {
    (void ) gen;
    value64     res = LITERAL64_ZERO;
    // TODO: SUGGESTION: stoge in v64Gen value64 oldval;
    userraiseint(ERR_NOT_IMPLEMENTED_FEATURE, "N/A");
    return res;
}

// ------------------------------ GENERATORS ----------------------------------------
// ------------------------ pre-created func V64 typed ------------------------------

value64
v64GenUnlimNull(v64Gen *gen) {
    value64 res = LITERAL64_ZERO;
    switch (gen->type) {
        case VALUE64_FS:
            res.fsval = (fs *) &V64GEN_NULL_FS;
            break;
        case VALUE64_STR:
            res = LITERAL64_STR(NULL);
            break;
        default:
            /* unknown type – return literal zero */
            return userraise(LITERAL64_ZERO, ERR_UNSUPPORTED_TYPE, 
                "%d/%s not supported", gen->type, value64_typename(gen->type));
    }
    v64GenAdvance(gen, 1);
    return res;
}

/**
 * @brief Standard generator for zero / empty values.
 *
 * Returns a "zero" value of the generator's type:
 * - numeric types → 0 / 0L / 0.0 / 0UL
 * - bool          → false
 * - char          → '\0'
 * - pointer       → NULL
 * - fs            → empty fs (via value64_createfs_asstr(""))
 * - str           → empty string (via value64_createstr(""))
 *
 * @param gen  pointer to the generator (type must be set)
 * @return     a zero value64 of the requested type (ownership passed to caller)
 */
value64                             
v64GenUnlimZero(v64Gen *gen) {   // TODO: need to be refactored via Table!
    v64GenAdvance(gen, 1);

    switch (gen->type) {
        case VALUE64_INT:
            return value64_createint(0);    // TODO: ANY value here!
        case VALUE64_LONG:
            return value64_createlong(0L);
        case VALUE64_DBL:
            return value64_createdbl(0.0);
        case VALUE64_ULONG:
            return value64_createulong(0UL);
        case VALUE64_BOOL:
            return value64_createbool(false);
        case VALUE64_CHR:
            return value64_createchar('\0');
        case VALUE64_PTR:
            return value64_createptr(NULL);
        case VALUE64_FS:
            return value64_createfs_asstr("");
        case VALUE64_STR:
            return value64_createstr("");
        default:
            /* unknown type – return literal zero */
            return userraise(LITERAL64_ZERO, ERR_UNSUPPORTED_TYPE, 
                "%d/%s not supported", gen->type, value64_typename(gen->type));
    }
}

// REG0 - value
// REG1 - only for FS/STR pattern "bla bla %d" or like that
static value64
v64GenAscValue(v64Gen *gen, int value) { // TODO: need to be refactored via Table!
    v64GenAdvance(gen, 1);

    value64 result = V64GENREGVAL0(gen);
    if (gen->type == VALUE64_STR || gen->type == VALUE64_FS) {
            int         val = value64_long(result);       

            const char *fmt = v64typedNvlStr(V64GENREG1(gen), "%d");
            fs          tmp = fscopyf(fmt, val);

            if (gen->type == VALUE64_FS) 
                result = value64_movefs(&tmp); // VALUE64_FS
            else {
                result = LITERAL64_STR(fs_movetostr(&tmp) );
            }
    } 
    v64typedAdd(&V64GENREG0(gen), value);

    return result;
}

/**
 * @brief Unchecked unlimited ascending series generator.
 *
 * Numeric types: data[0] holds the current value and is incremented.
 * Boolean:        data[0] toggles.
 * String types:   data[0] holds the current integer, data[1] holds an
 *                optional printf-template (STR or FS). If absent, "%d".
 *
 * @param gen  pointer to the generator
 * @return     the next value64 of the generator's type (ownership passed to caller)
 */
value64
v64GenAscSeries(v64Gen *gen) {
    return v64GenAscValue(gen, 1);
}

/**
 * @brief Unchecked unlimited ascending random series generator.
 *
 * Numeric types: data[0] holds the current value and is incremented.
 * Boolean:        data[0] toggles.
 * String types:   data[0] holds the current integer, data[1] holds an
 *                optional printf-template (STR or FS). If absent, "%d".
 *
 * @param gen  pointer to the generator
 * @return     the next value64 of the generator's type (ownership passed to caller)
 */
value64
v64GenAscRnd(v64Gen *gen) {
    int r = value64_int(V64GENREGVAL2(gen) );

    return v64GenAscValue(gen, rndint(r - 1) + 1);
}
/**
 * @brief Unchecked unlimited descending series generator.
 *
 * Numeric types: data[0] holds the current value and is incremented.
 * Boolean:        data[0] toggles.
 * String types:   data[0] holds the current integer, data[1] holds an
 *                optional printf-template (STR or FS). If absent, "%d".
 *
 * @param gen  pointer to the generator
 * @return     the next value64 of the generator's type (ownership passed to caller)
 */
value64                         
v64GenDescSeries(v64Gen *gen) {
    return v64GenAscValue(gen, -1);
}
/**
 * @brief Unchecked unlimited descending random series generator.
 *
 * Numeric types: data[0] holds the current value and is incremented.
 * Boolean:        data[0] toggles.
 * String types:   data[0] holds the current integer, data[1] holds an
 *                optional printf-template (STR or FS). If absent, "%d".
 *
 * @param gen  pointer to the generator
 * @return     the next value64 of the generator's type (ownership passed to caller)
 * @note
 *  REG2: rndinc value
 */
value64                         
v64GenDescRnd(v64Gen *gen) {
    int     r = value64_int(V64GENREGVAL2(gen) );

    return v64GenAscValue(gen, -(rndint(r - 1) + 1) );
}

value64                         
v64GenRandom(v64Gen *gen) {
    v64GenAdvance(gen, 1);

    int         r = value64_int(V64GENREGVAL0(gen) );
    r = rndint(r);
    switch (gen->type) {        // type of output generation
        case VALUE64_INT:
            return value64_createint(r);
        case VALUE64_LONG:
            return value64_createlong(r);
        case VALUE64_ULONG:
            return value64_createulong(r);
        case VALUE64_DBL: {
            double      dr;
            dr = rnddbl( (double) r);
            return value64_createdbl(dr);
        }
        case VALUE64_CHR:
            return value64_createchar(r);
        case VALUE64_BOOL:
            return value64_createbool(r);
        case VALUE64_FS: case VALUE64_STR: {
            const char *fmt = v64typedNvlStr(V64GENREG1(gen), "%d");
            fs          tmp = fscopyf(fmt, r);
            value64     result;
            if (gen->type == VALUE64_FS) 
                result = value64_movefs(&tmp); // VALUE64_FS
            else {
                result = LITERAL64_STR(fs_movetostr(&tmp) );
            }
            return result;      // FS or STR
        }
        default:
            return userraise(LITERAL64_ZERO, ERR_UNSUPPORTED_TYPE, 
                "Type %d/%s isn't supported", gen->type, value64_typename(gen->type) );
    }
}

// ------------------------- Source generators (DS or fs or c-str or FILE *) ------------------------

/**
 * @brief Parse fs from fs, dividev by newline
 * @note
 * @return: value64/fs
 * @note
 * data[0] – PTR to source fs (non-owning)
 */
static value64                 
v64GenFSToStringTargetByNewline(v64Gen *gen, value64_type typ){
    invraisecode(gen != NULL, ERR_NULLABLE_PTR, "Null generator");
    invraisecode(typ == VALUE64_FS || typ == VALUE64_STR, ERR_UNSUPPORTED_TYPE
        , "Type %d/%s isn't supported by %s", typ, value64_typename(typ), __func__);

    fs           *src = (fs *) V64GENREGVAL0(gen).pval;
    off_t         pos = gen->position;

    if (fs_isnull(src))
        userraiseint(ERR_NULLABLE_PTR, "Source fs is null %p", src);
        
    if ( (size_t) pos >= src->len)
        return userraise(LITERAL64_ZERO, ERR_OUT_OF_RANGE, 
            "Fs is out of range (%lld/%zu)", pos, src ? src->len : 0LU);

    // TODO: refactor that to normal code!
    const char   *start = src->v + pos;
    const char   *newline = strchr(start, '\n');
    value64       res = LITERAL64_ZERO;
    size_t        line_len;

    if (newline) {  // TODO: refactor here!
        line_len = newline - start + 1;
    } else
        line_len = src->len - pos;      // until '\0'

    fs      line = fs_newsubstr(src, pos, line_len);     // must be freed!!!!!
    
    // update current position and limit
    v64GenAdvanceByLine(gen, line_len);
    if (typ == VALUE64_FS)
        res = value64_movefs(&line);
    else    // VALUE64_STR
        res = LITERAL64_STR(fs_movetostr(&line));

    return res;
}

/**
 * @brief Parse fs from fs, dividev by newline
 * @note
 * @return: value64/str
 * @note
 * data[0] – PTR to source fs (non-owning)
 */
value64                         
v64GenFSToStrByNewline(v64Gen *gen) {
    return v64GenFSToStringTargetByNewline(gen, VALUE64_STR);
}
/**
 * @brief Parse fs from fs, dividev by newline
 * @note
 * @return: value64/fs
 * @note
 * data[0] – PTR to source fs (non-owning)
 */
value64                         
v64GenFSToFsByNewline(v64Gen *gen) {
    return v64GenFSToStringTargetByNewline(gen, VALUE64_FS);
}

/**
 * @brief Parse fs/str from FILE *, dividev by newline
 * @note
 * @return: value64/(fs|str)
 * @note
 * data[0] – PTR to source FILE * (non-owning)
 * 
 * data[2] - fs as buffer
 */
static value64           
v64GenFileToStringTargetByNewline(v64Gen *gen) {
    invraisecode(gen != NULL, ERR_NULLABLE_PTR, "Null generator");
    
    if (gen->limit == 0L)
        return logsimpleret(LITERAL64_ZERO, "zero limit! stream eshausted");

    fs      *buf = V64GENREGVAL2(gen).fsval;
    FILE    *in = V64GENREGVAL0(gen).FILEval;

    if (!buf || !in)
        return userraise(LITERAL64_ZERO, ERR_NULLABLE_PTR, "buf is null %p %p", buf, in);
    
    //GetlineStattus st = //getstring_newline_append(in, buf);
    if (!getstring_nl(in, buf, true, false) ) {
        gen->limit = 0L; 
        return logsimpleret(LITERAL64_ZERO, "EOF detected");
    }

    value64  res;
    if (gen->type == VALUE64_STR) 
        res = value64_createstr(fs_str(buf));
    else
        res = value64_createfs(buf);

    v64GenAdvanceByLine(gen, buf->len);

    return res;
} 



// RETURNS: CHR
// REGITRSY ALLOCATION:
// data[0]: STR as SOURCE (no ownership)
value64                         
v64GenStringToChar(v64Gen *gen) {
    invraisecode(gen != NULL, ERR_NULLABLE_PTR, "Null generator");

    const char  *base = (const char *) V64GENREGVAL0(gen).pval;
    if (!base)
        userraiseint(ERR_NULLABLE_PTR, "str (REG0) is null");

    const char  *str = base + gen->position;
    if (*str == '\0') {    // not an error, normal situation
        gen->limit = 0UL;
        return logsimpleret(value64_createchar('\0'), "cstr source is exhausted by \\0");
    }

    off_t        remaining = gen->limit;   // -1L - unlim
    if (!haslimit(remaining)) {
        gen->limit = 0UL;
        return logsimpleret(value64_createchar('\0'), 
                    "cstr source is exhausted by limit (%lld)", remaining);
    }

    v64GenAdvance(gen, 1);

    return value64_createchar(*str);
}

// RETURNS: CHR
// REGITRSY ALLOCATION:
// data[0]: fs (non-owning)
value64                         
v64GenFSToChar(v64Gen *gen){
    invraisecode(gen != NULL, ERR_NULLABLE_PTR, "Null generator");

    fs              *src = (fs *) V64GENREGVAL0(gen).pval;
    off_t            pos = gen->position;

    if (fs_isnull(src) )
        userraiseint(ERR_NULLABLE_PTR, "fs (REG0) is null");
    if ( (size_t) pos >= src->len)
        return logsimpleret(value64_createchar('\0'), "fs source is exhausted by limit");

    v64GenAdvance(gen, 1);

    return value64_createchar( fs_str(src)[pos] );
}

/**
 * @brief Stream character generator over a FILE* source.
 * @note
 * RETURNS: VALUE64/CHR
 * data[0] – PTR to FILE* (non-owning)
 */
value64                         v64GenFileToChar(v64Gen *gen)
{
    FILE    *fp = (FILE *) V64GENREGVAL0(gen).pval;
    if (!fp)
        userraiseint(ERR_NULLABLE_PTR, "FILE * (REG0) is null");

    off_t   remaining = gen->limit;
    if (remaining == 0)
        return logsimpleret(value64_createchar('\0'), "FILE source is exhausted by limit");

    int c = fgetc(fp);
    if (c == EOF) {
        gen->limit = 0L;
        return logsimpleret(value64_createchar('\0'), "FILE source is exhausted by EOF");
    } else
        v64GenAdvance(gen, 1);

    return value64_createchar((char)c);
}

// ------------------------ PRINTERS/CHECKERS ---------------------------------------

int                             v64GenTechfprint(FILE *restrict out, const v64Gen *restrict gen, const char *restrict name) {
    int cnt = 0;
    if (out) {
        IOCHECKER(w, fprintf(out, "V64GEN:%s [", name), -1)  // name cab be NULL
            cnt += w;
        if (!gen) {
            IOCHECKER(w, fprintf(out, "<NULL>"), -1)
                cnt += w;
        } else {
            IOCHECKER(w, fprintf(out, "fnext=%p, updater=%p, pos=%lld, limit=%lld type=%d/%s\t",
                        gen->fnext, gen->updater, gen->position, gen->limit, gen->type, value64_typename(gen->type) ), -1)
                cnt += w;
            for (int i = 0; i < V64GENCOUNT; i++) {
                char    buf[50];
                snprintf(buf, sizeof(buf) - 1, "REG%d", i);
                v64typedTechfprint(out, gen->data[i], buf);
            }
        }
        IOCHECKER(w, fprintf(out, "]\n"), -1)
            cnt += w;
    }
    return cnt;
}


// ------------------------------------ ETC. ----------------------------------------

// validation always use DIRECT fprintf (no userraise) to be able to work
// if you want to log using logging system, exec value64GenValidate(logfile, gen);
bool                            v64GenValidate(FILE *restrict out, const v64Gen *restrict gen) {
    if (!gen) {
        if (out)
            fprintf(out, "Gen is null");
        return logsimpleret(false, "Gen is null\n");
    }
    if (!gen->fnext) {
        if (out)
            fprintf(out, "Next Function is null\n");
        return logsimpleret(false, "Next Function is null");
    }
    if (!value64_checktype(gen->type)) {
        if (out)
            fprintf(out, "Value64 type %d is incorrect\n", gen->type);
        return logsimpleret(false, "Value64 type %d is incorrect", gen->type);
    }
    for (int i = 0; i < V64GENCOUNT; i++) {
        if (!value64_validate(out, gen->data[i].val, gen->data[i].typ) ) {
            if (out)
                fprintf(out, "V64 data[%d] is incorrect\n", i);
            return logsimpleret(false, "V64 data[%d] is incorrect", i);
        }
    }
    return true;
}

// ---------------------------------------- Testing ------------------------------------------
#ifdef V64GEN_TESTING

#include "test.h"

//types for testing

// ------------------------- TEST init_free ---------------------------------

// ------------------------- TEST v64GenInit / v64GenFree -------------------------
static TestStatus
tf1_gen_init_free(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: init and free with Zero generator", ++subnum);
    {
        v64Gen gen = v64GenInit2(v64GenUnlimZero, VALUE64_INT, -1L,
                                        V64TYPEDZERO(), V64TYPEDZERO());
        test_validate(gen.fnext != NULL, "generator function must be set");
        test_validate(gen.type == VALUE64_INT, "type must be INT");
        test_validate(gen.position == 0, "initial counter must be 0");
        test_validate(gen.data[0].typ == VALUE64_UNKNOWN, "data[0] should start UNKNOWN");
        test_validate(gen.data[1].typ == VALUE64_UNKNOWN, "data[1] should start UNKNOWN");

        v64GenFree(&gen);   // must not crash / leak
        fs_alloc_check(true);
    }

    test_sub("subtest %d: init and free with STR type", ++subnum);
    {
        v64Gen gen = v64GenInit2(v64GenUnlimZero, VALUE64_STR, -1L,
                                        V64TYPEDZERO(), V64TYPEDZERO());
        test_validate(gen.type == VALUE64_STR, "type must be STR");
        test_validate(gen.fnext != NULL, "fnext must be set");
        test_validate(gen.position == 0, "initial counter must be 0");
        test_validate(gen.data[0].typ == VALUE64_UNKNOWN, "data[0] should start UNKNOWN");
        test_validate(gen.data[1].typ == VALUE64_UNKNOWN, "data[1] should start UNKNOWN");
        

        v64GenFree(&gen);   // data[] were zero, no dynamic memory to free
        fs_alloc_check(true);
    }
    test_sub("subtest %d: init and free with STR type using simplifire creator", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimZero(VALUE64_STR);
        test_validate(gen.type == VALUE64_STR, "type must be STR");
        test_validate(gen.fnext == v64GenUnlimZero, "fnext (%p) must be set to v64GenUnlimZero (%p)", gen.fnext, v64GenUnlimZero);
        test_validate(gen.position == 0, "initial counter must be 0");
        test_validate(gen.data[0].typ == VALUE64_UNKNOWN, "data[0] should start UNKNOWN");
        test_validate(gen.data[1].typ == VALUE64_UNKNOWN, "data[1] should start UNKNOWN");
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST v64GenNext (zero) -------------------------
static TestStatus
tf2_gen_next_zero(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: Next with Zero generator (INT)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimZero(VALUE64_INT);   //v64GenInit0(v64GenUnlimZero, VALUE64_INT);

        // Zero generator ignores counter and always returns zero
        value64 v1 = v64GenNext(&gen);
        test_validate(value64_int(v1) == 0, "first call must return 0");
        test_validate(gen.position == 1, "counter must be 1");

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_int(v2) == 0, "second call must return 0");
        test_validate(gen.position == 2, "counter must be 2");

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_int(v3) == 0, "third call must return 0");
        test_validate(gen.position == 3, "counter must be 3");

        v64GenFree(&gen);
    }

    test_sub("subtest %d: Next with Zero generator (STR)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimZero(VALUE64_STR);

        value64 v = v64GenNext(&gen);
        test_validatefree(
            value64_str(v) && strcmp(value64_str(v), "") == 0,
            value64free(v, VALUE64_STR),
            "Zero for STR must return empty string"
        );
        test_validate(gen.position == 1, "counter must be 1");
        value64free(v, VALUE64_STR);
        
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 3. Zero generator for FS – stress test (many allocations) */
    test_sub("subtest %d: Next with Zero generator (FS), 100 elements", ++subnum);
    {
        v64Gen gen = v64GenInit0(v64GenUnlimZero, VALUE64_FS, -1L);

        enum { N = 100 };
        value64 arr[N];

        for (int i = 0; i < N; i++) {
            arr[i] = v64GenNext(&gen);
        }

        /* Verify all elements are empty strings and no corruption occurred */
        for (int i = 0; i < N; i++) {
            test_validate(arr[i].fsval != NULL && fs_len(arr[i].fsval) == 0,
                        "FS[%d] must be empty", i);
            test_validate(gen.position == N, "Must be %d", N);
        }

        /* Free all generated FS values */
        for (int i = 0; i < N; i++) {
            value64free(arr[i], VALUE64_FS);
        }

        v64GenFree(&gen);
        fs_alloc_check(true);   // must not detect any leak
    }
    /* Zero generator for STR – 300 empty strings */
    test_sub("subtest %d: Next with Zero generator (STR), 300 elements", ++subnum);
    {
        v64Gen gen = v64GenInit0(v64GenUnlimZero, VALUE64_STR, -1L);

        enum { N = 300 };
        value64 arr[N];
        for (int i = 0; i < N; i++) {
            arr[i] = v64GenNext(&gen);
        }

        for (int i = 0; i < N; i++) {
            test_validate(value64_str(arr[i]) != NULL && strlen(value64_str(arr[i])) == 0,
                          "STR[%d] must be empty", i);
        }
        test_validate(gen.position == N, "Zero generator counter must be %d", N);

        for (int i = 0; i < N; i++) {
            value64free(arr[i], VALUE64_STR);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }
    test_sub("subtest %d: wrapper for INT", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimZero(VALUE64_INT);
        for (int i = 0; i < 400; i++) {
            int res;
            test_validate(
                (res = v64GenNext(&gen).ival) == 0,
                "Iteration %d got %d", i, res
            );
        }
        v64GenFree(&gen);
    }
    test_sub("subtest %d: wrapper for FS", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimZero(VALUE64_FS);
        for (int i = 0; i < 400; i++) {
            fs *res = v64GenNext(&gen).fsval;
            test_validatefree(
                strcmp(fs_str(res), "") == 0,
                (fs_free(res), v64GenFree(&gen) ),
                "Iteration %d got %s instead of \"\"", i, fs_str(res)
            );
            fs_free(res);
        }
        v64GenFree(&gen);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST value64UncheckGenUnlimAscSeries -------------------------
static TestStatus
tf3_gen_asc_series(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT ascending from 10 */
    test_sub("subtest %d: AscSeries INT from 10", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscSeries(v64typedCreateInt(10) );
        value64 v1 = v64GenNext(&gen);
        test_validate(value64_int(v1) == 10, "first must be 10");
        test_validate(gen.position == 1, "counter must be 1");

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_int(v2) == 11, "second must be 11");
        test_validate(gen.position == 2, "counter must be 2");

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_int(v3) == 12, "third must be 12");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_INT);
        value64_free(&v2, VALUE64_INT);
        value64_free(&v3, VALUE64_INT);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 2. LONG ascending from 100 */
    test_sub("subtest %d: AscSeries LONG from 100", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscSeries(v64typedCreateLong(100L) );
        value64 v1 = v64GenNext(&gen);
        test_validate(value64_long(v1) == 100L, "first must be 100");
        value64 v2 = v64GenNext(&gen);
        test_validate(value64_long(v2) == 101L, "second must be 101");
        value64 v3 = v64GenNext(&gen);
        test_validate(value64_long(v3) == 102L, "third must be 102");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_LONG);
        value64_free(&v2, VALUE64_LONG);
        value64_free(&v3, VALUE64_LONG);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 3. DBL ascending from 10.5 */
    test_sub("subtest %d: AscSeries DBL from 10.5 directly via v64GenInit1", ++subnum);
    {
        // only via v64GenInit1 for now, no simplifier for that
        v64Gen gen = v64GenInit1(v64GenAscSeries, VALUE64_DBL, -1L,
                                        v64typedCreateDbl(10.5));
        value64 v1 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v1) - 10.5) < 1e-9, "first must be 10.5");
        value64 v2 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v2) - 11.5) < 1e-9, "second must be 11.5");
        value64 v3 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v3) - 12.5) < 1e-9, "third must be 12.5");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_DBL);
        value64_free(&v2, VALUE64_DBL);
        value64_free(&v3, VALUE64_DBL);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }
    test_sub("subtest %d: AscSeries DBL from 10.5 via creator", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscSeries(v64typedCreateDbl(10.5) );

        value64 v1 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v1) - 10.5) < 1e-9, "first must be 10.5");
        value64 v2 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v2) - 11.5) < 1e-9, "second must be 11.5");
        value64 v3 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v3) - 12.5) < 1e-9, "third must be 12.5");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_DBL);
        value64_free(&v2, VALUE64_DBL);
        value64_free(&v3, VALUE64_DBL);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 4. ULONG ascending from 200 */
    test_sub("subtest %d: AscSeries ULONG from 200", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscSeries(v64typedCreateULong(200UL) );

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_ulong(v1) == 200UL, "first must be 200");
        value64 v2 = v64GenNext(&gen);
        test_validate(value64_ulong(v2) == 201UL, "second must be 201");
        value64 v3 = v64GenNext(&gen);
        test_validate(value64_ulong(v3) == 202UL, "third must be 202");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_ULONG);
        value64_free(&v2, VALUE64_ULONG);
        value64_free(&v3, VALUE64_ULONG);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 5. BOOL toggles */
    test_sub("subtest %d: AscSeries BOOL toggles directly", ++subnum);
    {
        v64Gen gen = v64GenInit1(v64GenAscSeries, VALUE64_BOOL, -1L,
                                        v64typedCreateBool(false));
        value64 v1 = v64GenNext(&gen);
        test_validate(value64_bool(v1) == false, "first must be false");
        value64 v2 = v64GenNext(&gen);
        test_validate(value64_bool(v2) == true, "second must be true");
        value64 v3 = v64GenNext(&gen);
        test_validate(value64_bool(v3) == false, "third must be false");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_BOOL);
        value64_free(&v2, VALUE64_BOOL);
        value64_free(&v3, VALUE64_BOOL);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscSeries BOOL toggles, via creator", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscSeries(v64typedCreateBool(false) );

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_bool(v1) == false, "first must be false");
        value64 v2 = v64GenNext(&gen);
        test_validate(value64_bool(v2) == true, "second must be true");
        value64 v3 = v64GenNext(&gen);
        test_validate(value64_bool(v3) == false, "third must be false");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_BOOL);
        value64_free(&v2, VALUE64_BOOL);
        value64_free(&v3, VALUE64_BOOL);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 6. CHAR ascending from 'A' (now supported via RegAdd) */
    test_sub("subtest %d: AscSeries CHAR from 'A' directly", ++subnum);
    {
        v64Gen gen = v64GenInit1(v64GenAscSeries, VALUE64_CHR, -1L,
                                        v64typedCreateChar('A'));
        value64 v1 = v64GenNext(&gen);
        test_validate(value64_char(v1) == 'A', "first must be 'A'");
        value64 v2 = v64GenNext(&gen);
        test_validate(value64_char(v2) == 'B', "second must be 'B'");
        value64 v3 = v64GenNext(&gen);
        test_validate(value64_char(v3) == 'C', "third must be 'C'");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_CHR);
        value64_free(&v2, VALUE64_CHR);
        value64_free(&v3, VALUE64_CHR);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscSeries CHAR from 'A' via creator", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscSeries(v64typedCreateChar('A') );

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_char(v1) == 'A', "first must be 'A'");
        value64 v2 = v64GenNext(&gen);
        test_validate(value64_char(v2) == 'B', "second must be 'B'");
        value64 v3 = v64GenNext(&gen);
        test_validate(value64_char(v3) == 'C', "third must be 'C'");
        test_validate(gen.position == 3, "counter must be 3");

        value64_free(&v1, VALUE64_CHR);
        value64_free(&v2, VALUE64_CHR);
        value64_free(&v3, VALUE64_CHR);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 7. STR without template (data[0]=int, data[1]=zero) */
    test_sub("subtest %d: AscSeries STR default template", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscStrSeries(0, NULL);

        value64 v1 = v64GenNext(&gen);
        test_validate(
            strcmp(value64_str(v1), "0") == 0, 
            "first must be '0', but got '%s'", value64_str(v1)
        );
        value64_free(&v1, VALUE64_STR);

        value64 v2 = v64GenNext(&gen);
        test_validate(
            strcmp(value64_str(v2), "1") == 0,
            "second must be '1', got '%s'", value64_str(v2)
        );
        value64_free(&v2, VALUE64_STR);

        value64 v3 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v3), "2") == 0, "third must be '2'");
        value64_free(&v3, VALUE64_STR);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 8. STR with template "item %d" starting at 3 (template is FS) */
    test_sub("subtest %d: AscSeries STR with template", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscStrSeries(3, "item %d");
        value64 v1 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v1), "item 3") == 0, "first must be 'item 3'");
        value64_free(&v1, VALUE64_STR);

        value64 v2 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v2), "item 4") == 0, "second must be 'item 4'");
        value64_free(&v2, VALUE64_STR);

        value64 v3 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v3), "item 5") == 0, "third must be 'item 5'");
        value64_free(&v3, VALUE64_STR);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 9. FS without template (data[0]=int, data[1]=zero) */
    test_sub("subtest %d: AscSeries FS default template", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscFsSeries(0, NULL);

        value64 v1 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v1)), "0") == 0, "first must be '0'");
        value64_free(&v1, VALUE64_FS);

        value64 v2 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v2)), "1") == 0, "second must be '1'");
        value64_free(&v2, VALUE64_FS);

        value64 v3 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v3)), "2") == 0, "third must be '2'");
        value64_free(&v3, VALUE64_FS);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 10. FS with template "val_%d" starting at 7 */
    test_sub("subtest %d: AscSeries FS with template", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscFsSeries(7, "val_%d");
        value64 v1 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v1)), "val_7") == 0, "first must be 'val_7'");
        value64_free(&v1, VALUE64_FS);

        value64 v2 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v2)), "val_8") == 0, "second must be 'val_8'");
        value64_free(&v2, VALUE64_FS);

        value64 v3 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v3)), "val_9") == 0, "third must be 'val_9'");
        value64_free(&v3, VALUE64_FS);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST v64gen creators (simple wrappers) -------------------------
/*static TestStatus
tf4_gen_creators(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

     1. v64GenCreatorUnlimStrValue: cоздаёт генератор типа STR c региcтром data[0] = cтрока,
       а v64GenNext возвращает пуcтую cтроку (zero) 
    test_sub("subtest %d: v64GenCreatorUnlimStrValue (STR)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimStrValue("hello");

        test_validate(gen.type == VALUE64_STR, "type must be STR");
        test_validate(gen.data[0].typ == VALUE64_STR, "data[0] must be STR");
        test_validate(strcmp(value64_str(gen.data[0].val), "hello") == 0,
                      "data[0] value mismatch");

        value64 v = v64GenNext(&gen);
        test_validatefree(
            value64_str(v) != NULL && strcmp(value64_str(v), "") == 0,
            v64typedFree(&(v64typed){.val = v, .typ = VALUE64_STR}),
            "Zero for STR must be empty string, got '%s'", value64_str(v)
        );
        v64typedFree(&(v64typed){.val = v, .typ = VALUE64_STR});
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

     2. v64GenCreatorUnlimValue (LONG): генератор типа LONG, data[0]=long, zero = 0L 
    test_sub("subtest %d: v64GenCreatorUnlimValue (LONG)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimValue(12345L);

        test_validate(gen.type == VALUE64_LONG, "type must be LONG");
        test_validate(gen.data[0].typ == VALUE64_LONG, "data[0] must be LONG");
        test_validate(value64_long(gen.data[0].val) == 12345L,
                      "data[0] value mismatch");

        value64 v = v64GenNext(&gen);
        test_validate(value64_long(v) == 0L, "Zero for LONG must be 0, got %ld", value64_long(v));
        // no ownership to free for LONG
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: v64GenCreatorUnlimDouble (DBL)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDouble(3.14);

        // Эти проверки должны пройти поcле иcправления типа на VALUE64_DBL
        test_validate(gen.type == VALUE64_DBL, "type must be DBL (bug: got %d %s)",
                      gen.type, value64_typename(gen.type));
        test_validate(gen.data[0].typ == VALUE64_DBL, "data[0] must be DBL");
        test_validate(fabs(value64_dbl(gen.data[0].val) - 3.14) < 1e-9,
                      "data[0] value mismatch");

        value64 v = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v) - 0.0) < 1e-9,
                      "Zero for DBL must be 0.0, got %f", value64_dbl(v));
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
} */

// ------------------------- TEST : asc random generator -------------------------
static TestStatus
tf5_gen_asc_rnd(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: AscRnd INT increments by 1..6", ++subnum);
    {
       v64Gen gen = v64GenCreatorUnlimAscRnd(v64typedCreateInt(10), 5);
 
        value64 v1 = v64GenNext(&gen);
        test_validate(value64_int(v1) == 10, "first must be 10");

        value64 v2 = v64GenNext(&gen);
        int diff = value64_int(v2) - value64_int(v1);
        test_validate(diff >= 1 && diff <= 6, "increment must be in [1,6], got %d", diff);

        value64 v3 = v64GenNext(&gen);
        int diff2 = value64_int(v3) - value64_int(v2);
        test_validate(diff2 >= 1 && diff2 <= 6, "second increment must be in [1,6], got %d", diff2);

        value64_free(&v1, gen.type);
        value64_free(&v2, gen.type);
        value64_free(&v3, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd LONG increments by 1..6", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscRnd(v64typedCreateInt(100L), 5);

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_long(v1) == 100L, "first must be 100");

        value64 v2 = v64GenNext(&gen);
        long diff = value64_long(v2) - value64_long(v1);
        test_validate(diff >= 1 && diff <= 6, "increment must be in [1,6], got %ld", diff);

        value64 v3 = v64GenNext(&gen);
        long diff2 = value64_long(v3) - value64_long(v2);
        test_validate(diff2 >= 1 && diff2 <= 6, "second increment must be in [1,6], got %ld", diff2);

        value64_free(&v1, gen.type);
        value64_free(&v2, gen.type);
        value64_free(&v3, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd DBL increments by 1.0..6.0 direct call", ++subnum);
    {
        v64Gen gen = v64GenInit3(v64GenAscRnd, VALUE64_DBL, -1L,
                                v64typedCreateDbl(10.0), V64TYPEDZERO(), v64typedCreateInt(5) );

        value64 v1 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v1) - 10.0) < 1e-9, "first must be 10.0");

        value64 v2 = v64GenNext(&gen);
        double diff = value64_dbl(v2) - value64_dbl(v1);
        test_validate(diff >= 1.0 && diff <= 6.0, "increment must be in [1.0,6.0], got %f", diff);

        value64 v3 = v64GenNext(&gen);
        double diff2 = value64_dbl(v3) - value64_dbl(v2);
        test_validate(diff2 >= 1.0 && diff2 <= 6.0, "second increment must be in [1.0,6.0], got %f", diff2);

        value64_free(&v1, gen.type);
        value64_free(&v2, gen.type);
        value64_free(&v3, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd DBL increments by 1.0..6.0 via creator", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscRnd(v64typedCreateDbl(10.0), 5);

        value64 v1 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v1) - 10.0) < 1e-9, "first must be 10.0");

        value64 v2 = v64GenNext(&gen);
        double diff = value64_dbl(v2) - value64_dbl(v1);
        test_validate(diff >= 1.0 && diff <= 6.0, "increment must be in [1.0,6.0], got %f", diff);

        value64 v3 = v64GenNext(&gen);
        double diff2 = value64_dbl(v3) - value64_dbl(v2);
        test_validate(diff2 >= 1.0 && diff2 <= 6.0, "second increment must be in [1.0,6.0], got %f", diff2);

        value64_free(&v1, gen.type);
        value64_free(&v2, gen.type);
        value64_free(&v3, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd ULONG increments by 1..6", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscRnd(v64typedCreateULong(200UL), 5);

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_ulong(v1) == 200UL, "first must be 200");

        value64 v2 = v64GenNext(&gen);
        unsigned long diff = value64_ulong(v2) - value64_ulong(v1);
        test_validate(diff >= 1 && diff <= 6, "increment must be in [1,6], got %lu", diff);

        value64 v3 = v64GenNext(&gen);
        unsigned long diff2 = value64_ulong(v3) - value64_ulong(v2);
        test_validate(diff2 >= 1 && diff2 <= 6, "second increment must be in [1,6], got %lu", diff2);

        value64_free(&v1, gen.type);
        value64_free(&v2, gen.type);
        value64_free(&v3, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd CHAR increments by 1..6", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscRnd(v64typedCreateChar('A'), 5);

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_char(v1) == 'A', "first must be 'A'");

        value64 v2 = v64GenNext(&gen);
        int diff = (unsigned char)value64_char(v2) - (unsigned char)value64_char(v1);
        test_validate(diff >= 1 && diff <= 6, "increment must be in [1,6], got %d", diff);

        value64 v3 = v64GenNext(&gen);
        int diff2 = (unsigned char)value64_char(v3) - (unsigned char)value64_char(v2);
        test_validate(diff2 >= 1 && diff2 <= 6, "second increment must be in [1,6], got %d", diff2);

        value64_free(&v1, gen.type);
        value64_free(&v2, gen.type);
        value64_free(&v3, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd BOOL toggles", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscRnd(v64typedCreateBool(false), 0);

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_bool(v1) == false, "first must be false");

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_bool(v2) == true, "second must be true");

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_bool(v3) == false, "third must be false");

        value64_free(&v1, gen.type);
        value64_free(&v2, gen.type);
        value64_free(&v3, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd STR with template, data[0] increments", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscStrRnd(0, "item %d", 5);

        value64 v1 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v1), "item 0") == 0,
                      "first must be 'item 0', got '%s'", value64_str(v1));
        value64_free(&v1, gen.type);

        int before = v64typedCastToInt(gen.data[0]);
        value64 v2 = v64GenNext(&gen);
        int after = v64typedCastToInt(gen.data[0]);
        int diff = after - before;
        test_validate(diff >= 1 && diff <= 6,
                      "data[0] must increase by 1..6, got %d", diff);

        value64_free(&v2, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }
    test_sub("subtest %d: AscRnd FS with template, data[0] increments", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscFsRnd(0, "item %d", 5);

        value64 v1 = v64GenNext(&gen);
        test_validate(fs_cmpstr(value64_fs(v1), "item 0") == 0,
                      "first must be 'item 0', got '%s'", value64_fsstr(v1) );
        value64_free(&v1, gen.type);    // WOW THINK:

        int before = v64typedCastToInt(gen.data[0]);
        value64 v2 = v64GenNext(&gen);
        int after = v64typedCastToInt(gen.data[0]);
        int diff = after - before;
        test_validate(diff >= 1 && diff <= 6,
                      "data[0] must increase by 1..6, got %d", diff);

        value64_free(&v2, gen.type);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST: AscRnd with custom rndinc -------------------------
static TestStatus
tf6_gen_asc_rnd_custom(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    srand((unsigned)time(NULL)); 

    /* 1. INT with rndinc=1: increments should be 1 or 2, and both must appear */
    test_sub("subtest %d: AscRnd INT with rndinc=2 (increments 1..2)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscRnd(v64typedCreateInt(100), 2);

        value64 v0 = v64GenNext(&gen);
        test_validate(value64_int(v0) == 100, "first must be 100");
        value64_free(&v0, gen.type);

        bool saw_1 = false, saw_2 = false;
        value64 prev = value64_createint(100);  // не используется напрямую, можно просто отслеживать предыдущее число
        (void) prev;
        int prev_val = 100;
        for (int i = 0; i < 20; i++) {
            value64 v = v64GenNext(&gen);
            int diff = value64_int(v) - prev_val;
            test_validate(diff >= 1 && diff <= 2,
                          "increment must be in [1,2], got %d", diff);
            if (diff == 1)
                saw_1 = true;
            if (diff == 2)
                saw_2 = true;
            prev_val = value64_int(v);
            value64_free(&v, gen.type);
        }
        test_validate(saw_1 && saw_2,
                      "both increments 1 and 2 should appear with rndinc=1");

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 2. LONG with rndinc=5: increments should be 1..6 (по текущей реализации) */
    test_sub("subtest %d: AscRnd LONG with rndinc=5 (increments 1..6)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscRnd(v64typedCreateInt(500L), 6);

        value64 v0 = v64GenNext(&gen);
        test_validate(value64_long(v0) == 500L, "first must be 500");
        value64_free(&v0, gen.type);

        long prev_val = 500L;
        for (int i = 0; i < 30; i++) {
            value64 v = v64GenNext(&gen);
            long diff = value64_long(v) - prev_val;
            test_validate(diff >= 1 && diff <= 6,
                          "increment must be in [1,6], got %ld", diff);
            prev_val = value64_long(v);
            value64_free(&v, gen.type);
        }

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd STR with rndinc=3 and template", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscStrRnd(0, "val %d", 4);

        value64 v0 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v0), "val 0") == 0,
                    "first must be 'val 0', got '%s'", value64_str(v0));
        value64_free(&v0, gen.type);

        for (int i = 0; i < 15; i++) {
            int before = v64typedCastToInt(gen.data[0]);   // счётчик до вызова
            value64 v = v64GenNext(&gen);
            int after = v64typedCastToInt(gen.data[0]);    // после вызова
            int diff = after - before;
            test_validate(diff >= 1 && diff <= 4,
                        "data[0] must increase by 1..4, got %d", diff);
            value64_free(&v, gen.type);
        }

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: AscRnd FS with rndinc=3 and template", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimAscFsRnd(0, "val %d", 4);

        value64 v0 = v64GenNext(&gen);
        test_validate(fs_cmpstr(value64_fs(v0), "val 0") == 0,
                    "first must be 'val 0', got '%s'", fs_str(value64_fs(v0) ) );
        value64_free(&v0, gen.type);

        for (int i = 0; i < 15; i++) {
            int before = v64typedCastToInt(gen.data[0]);   // счётчик до вызова
            value64 v = v64GenNext(&gen);
            int after = v64typedCastToInt(gen.data[0]);    // после вызова
            int diff = after - before;
            test_validate(diff >= 1 && diff <= 4,
                        "data[0] must increase by 1..4, got %d", diff);
            value64_free(&v, gen.type);
        }

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 6: DescSeries -------------------------
static TestStatus
tf7_gen_desc_series(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT descending from 10 */
    test_sub("subtest %d: DescSeries INT from 10", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescSeries(v64typedCreateInt(10) );

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_int(v1) == 10, "first must be 10");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_int(v2) == 9, "second must be 9");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_int(v3) == 8, "third must be 8");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 2. LONG descending from 100 */
    test_sub("subtest %d: DescSeries LONG from 100", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescSeries(v64typedCreateLong(100L) );

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_long(v1) == 100L, "first must be 100");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_long(v2) == 99L, "second must be 99");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_long(v3) == 98L, "third must be 98");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 3. DBL descending from 10.5 */
    test_sub("subtest %d: DescSeries DBL from 10.5", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescSeries(v64typedCreateDbl(10.5) );

        value64 v1 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v1) - 10.5) < 1e-9, "first must be 10.5");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v2) - 9.5) < 1e-9, "second must be 9.5");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v3) - 8.5) < 1e-9, "third must be 8.5");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 4. ULONG descending from 200 */
    test_sub("subtest %d: DescSeries ULONG from 200", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescSeries(v64typedCreateULong(200UL) );

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_ulong(v1) == 200UL, "first must be 200");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_ulong(v2) == 199UL, "second must be 199");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_ulong(v3) == 198UL, "third must be 198");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 5. BOOL toggles */
    test_sub("subtest %d: DescSeries BOOL toggles", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescSeries(v64typedCreateBool(false) );

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_bool(v1) == false, "first must be false");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_bool(v2) == true, "second must be true");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_bool(v3) == false, "third must be false");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 6. CHAR descending from 'C' */
    test_sub("subtest %d: DescSeries CHAR from 'C'", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescSeries(v64typedCreateChar('C') );

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_char(v1) == 'C', "first must be 'C'");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_char(v2) == 'B', "second must be 'B'");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_char(v3) == 'A', "third must be 'A'");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 7. STR default template starting at 3 */
    test_sub("subtest %d: DescSeries STR default template", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescStrSeries(3L, NULL);

        value64 v1 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v1), "3") == 0, "first must be '3'");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v2), "2") == 0, "second must be '2'");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v3), "1") == 0, "third must be '1'");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 8. STR with template "item %d" */
    test_sub("subtest %d: DescSeries STR with template 'item %%d'", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescStrSeries(3L, "item %d");

        value64 v1 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v1), "item 3") == 0, "first must be 'item 3'");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v2), "item 2") == 0, "second must be 'item 2'");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(strcmp(value64_str(v3), "item 1") == 0, "third must be 'item 1'");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 9. FS default template starting at 2 */
    test_sub("subtest %d: DescSeries FS default template", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescFsSeries(2L, NULL);

        value64 v1 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v1)), "2") == 0, "first must be '2'");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v2)), "1") == 0, "second must be '1'");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v3)), "0") == 0, "third must be '0'");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 10. FS with template "val_%d" */
    test_sub("subtest %d: DescSeries FS with template 'val_%%d'", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescFsSeries(7L, "val_%d");

        value64 v1 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v1)), "val_7") == 0, "first must be 'val_7'");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v2)), "val_6") == 0, "second must be 'val_6'");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(strcmp(fs_str(value64_fs(v3)), "val_5") == 0, "third must be 'val_5'");
        value64_free(&v3, gen.type);

        test_validate(gen.position == 3, "counter must be 3");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 7: DescRnd (descending random) -------------------------
static TestStatus
tf8_gen_desc_rnd_custom(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT with rndinc=1 (decrements 1..2) */
    test_sub("subtest %d: DescRnd INT with rndinc=1 (decrements 1..2)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescRnd(v64typedCreateInt(100), 2);

        value64 v0 = v64GenNext(&gen);
        test_validate(value64_int(v0) == 100, "first must be 100");
        value64_free(&v0, gen.type);

        bool saw_1 = false, saw_2 = false;
        int prev_val = 100;
        for (int i = 0; i < 30; i++) {
            value64 v = v64GenNext(&gen);
            int diff = prev_val - value64_int(v);
            test_validate(diff >= 1 && diff <= 2,
                          "decrement must be in [1,2], got %d", diff);
            if (diff == 1) saw_1 = true;
            if (diff == 2) saw_2 = true;
            prev_val = value64_int(v);
            value64_free(&v, gen.type);
        }
        test_validate(saw_1 && saw_2,
                      "both decrements 1 and 2 should appear with rndinc=1");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 2. LONG with rndinc=5 (decrements 1..6) */
    test_sub("subtest %d: DescRnd LONG with rndinc=5 (decrements 1..6)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescRnd(v64typedCreateLong(500L), 6);

        value64 v0 = v64GenNext(&gen);
        test_validate(value64_long(v0) == 500L, "first must be 500");
        value64_free(&v0, gen.type);

        long prev_val = 500L;
        for (int i = 0; i < 30; i++) {
            value64 v = v64GenNext(&gen);
            long diff = prev_val - value64_long(v);
            test_validate(diff >= 1 && diff <= 6,
                          "decrement must be in [1,6], got %ld", diff);
            prev_val = value64_long(v);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 3. DBL with rndinc=3 (decrements 1.0..4.0) */
    test_sub("subtest %d: DescRnd DBL with rndinc=3 (decrements 1.0..4.0)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescRnd(v64typedCreateDbl(20.0), 4);

        value64 v0 = v64GenNext(&gen);
        test_validate(fabs(value64_dbl(v0) - 20.0) < 1e-9, "first must be 20.0");
        value64_free(&v0, gen.type);

        double prev_val = 20.0;
        for (int i = 0; i < 20; i++) {
            value64 v = v64GenNext(&gen);
            double diff = prev_val - value64_dbl(v);
            test_validate(diff >= 1.0 && diff <= 4.0,
                          "decrement must be in [1.0,4.0], got %f", diff);
            prev_val = value64_dbl(v);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 4. ULONG with rndinc=2 (decrements 1..3) */
    test_sub("subtest %d: DescRnd ULONG with rndinc=2 (decrements 1..3)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescRnd(v64typedCreateULong(1000UL), 3);

        value64 v0 = v64GenNext(&gen);
        test_validate(value64_ulong(v0) == 1000UL, "first must be 1000");
        value64_free(&v0, gen.type);

        unsigned long prev_val = 1000UL;
        for (int i = 0; i < 20; i++) {
            value64 v = v64GenNext(&gen);
            unsigned long diff = prev_val - value64_ulong(v);
            test_validate(diff >= 1 && diff <= 3,
                          "decrement must be in [1,3], got %lu", diff);
            prev_val = value64_ulong(v);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 5. CHR with rndinc=1 (decrements 1..2) */
    test_sub("subtest %d: DescRnd CHAR with rndinc=1 (decrements 1..2)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescRnd(v64typedCreateChar('Z'), 2);

        value64 v0 = v64GenNext(&gen);
        test_validate(value64_char(v0) == 'Z', "first must be 'Z'");
        value64_free(&v0, gen.type);

        int prev_val = (unsigned char)'Z';
        for (int i = 0; i < 10; i++) {
            value64 v = v64GenNext(&gen);
            int diff = prev_val - (unsigned char)value64_char(v);
            test_validate(diff >= 1 && diff <= 2,
                          "decrement must be in [1,2], got %d", diff);
            prev_val = (unsigned char)value64_char(v);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 6. BOOL toggles (value ignored) */
    test_sub("subtest %d: DescRnd BOOL toggles", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescRnd(v64typedCreateBool(false), 3);

        value64 v1 = v64GenNext(&gen);
        test_validate(value64_bool(v1) == false, "first must be false");
        value64_free(&v1, gen.type);

        value64 v2 = v64GenNext(&gen);
        test_validate(value64_bool(v2) == true, "second must be true");
        value64_free(&v2, gen.type);

        value64 v3 = v64GenNext(&gen);
        test_validate(value64_bool(v3) == false, "third must be false");
        value64_free(&v3, gen.type);

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DescRnd STR with template and rndinc=3", ++subnum);
{
    v64Gen gen = v64GenCreatorUnlimDescStrRnd(10, "item %d", 4);

    value64 v0 = v64GenNext(&gen);
    test_validate(strcmp(value64_str(v0), "item 10") == 0,
                  "first must be 'item 10', got '%s'", value64_str(v0));
    value64_free(&v0, gen.type);

    for (int i = 0; i < 15; i++) {
        int before = v64typedCastToInt(gen.data[0]);   // текущее значение до вызова
        value64 v = v64GenNext(&gen);
        int after = v64typedCastToInt(gen.data[0]);    // после вызова
        int diff = before - after;
        test_validate(diff >= 1 && diff <= 4,
                      "data[0] must decrease by 1..4, got %d", diff);
        value64_free(&v, gen.type);
    }

    v64GenFree(&gen);
    fs_alloc_check(true);
}

    /* 8. FS with template "val %d" and rndinc=2 (decrements 1..3) */
    test_sub("subtest %d: DescRnd FS with template and rndinc=2", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimDescFsRnd(7, "val %d", 4);

        value64 v0 = v64GenNext(&gen);
        test_validate(fs_cmpstr(value64_fs(v0), "val 7") == 0,
                      "first must be 'val 7', got '%s'", fs_str(value64_fs(v0)));
        value64_free(&v0, gen.type);

        for (int i = 0; i < 10; i++) {
            int before = v64typedCastToInt(gen.data[0]);   // текущее значение до вызова
            value64 v = v64GenNext(&gen);
            int after = v64typedCastToInt(gen.data[0]);    // после вызова
            int diff = before - after;
            test_validate(diff >= 1 && diff <= 4,
                        "data[0] must decrease by 1..4, got %d", diff);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 8: UnlimRandom generators -------------------------
static TestStatus
tf9_gen_unlim_random(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. INT random in [0, rndinc] */
    test_sub("subtest %d: UnlimRandom INT with rndinc=10", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimRnd(VALUE64_INT, 10);
        for (int i = 0; i < 50; i++) {
            value64 v = v64GenNext(&gen);
            int val = value64_int(v);
            test_validate(val >= 0 && val <= 10,
                          "INT random value out of range: %d", val);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 2. LONG random in [0, rndinc] */
    test_sub("subtest %d: UnlimRandom LONG with rndinc=10", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimRnd(VALUE64_LONG, 10);
        for (int i = 0; i < 50; i++) {
            value64 v = v64GenNext(&gen);
            long val = value64_long(v);
            test_validate(val >= 0 && val <= 10,
                          "LONG random value out of range: %ld", val);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 3. ULONG random in [0, rndinc] */
    test_sub("subtest %d: UnlimRandom ULONG with rndinc=10", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimRnd(VALUE64_ULONG, 10);
        for (int i = 0; i < 50; i++) {
            value64 v = v64GenNext(&gen);
            unsigned long val = value64_ulong(v);
            test_validate(val >= 0 && val <= 10,
                          "ULONG random value out of range: %lu", val);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 4. DBL random in [0, rndinc] (integer values as double) */
    test_sub("subtest %d: UnlimRandom DBL with rndinc=10", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimRnd(VALUE64_DBL, 10);
        for (int i = 0; i < 50; i++) {
            value64 v = v64GenNext(&gen);
            double val = value64_dbl(v);
            test_validate(val >= 0.0 && val <= 10.0,
                          "DBL random value out of range: %f", val);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 5. CHAR random in [0, rndinc] */
    test_sub("subtest %d: UnlimRandom CHAR with rndinc=25", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimRnd(VALUE64_CHR, 25);
        for (int i = 0; i < 50; i++) {
            value64 v = v64GenNext(&gen);
            unsigned char c = (unsigned char)value64_char(v);
            test_validate(c >= 0 && c <= 25,
                          "CHAR random value out of range: %d", c);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 6. BOOL random (rndinc=1) should produce both true and false */
    test_sub("subtest %d: UnlimRandom BOOL with rndinc=1", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimRnd(VALUE64_BOOL, 1);
        bool saw_true = false, saw_false = false;
        for (int i = 0; i < 50; i++) {
            value64 v = v64GenNext(&gen);
            bool b = value64_bool(v);
            if (b) saw_true = true;
            else saw_false = true;
            value64_free(&v, gen.type);
        }
        test_validate(saw_true && saw_false,
                      "BOOL random should produce both true and false");
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 7. STR random with template "val %d" and rndinc=5 */
    test_sub("subtest %d: UnlimRandom STR with template and rndinc=5", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimStrRnd("val %d", 5);
        for (int i = 0; i < 30; i++) {
            value64 v = v64GenNext(&gen);
            const char *str = value64_str(v);
            int num;
            bool ok = (sscanf(str, "val %d", &num) == 1) && num >= 0 && num <= 5;
            test_validate(ok,
                          "STR random pattern mismatch: '%s'", str);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 8. STR random without template (default "%d") and rndinc=7 */
    test_sub("subtest %d: UnlimRandom STR without template, rndinc=7", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimStrRnd(NULL, 7);
        for (int i = 0; i < 30; i++) {
            value64 v = v64GenNext(&gen);
            const char *str = value64_str(v);
            int num;
            bool ok = (sscanf(str, "%d", &num) == 1) && num >= 0 && num <= 7;
            test_validate(ok,
                          "STR random default pattern mismatch: '%s'", str);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 9. FS random with template "item %d" and rndinc=3 */
    test_sub("subtest %d: UnlimRandom FS with template and rndinc=3", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimFsRnd("item %d", 3);
        for (int i = 0; i < 30; i++) {
            value64 v = v64GenNext(&gen);
            const char *str = fs_str(value64_fs(v));
            int num;
            bool ok = (sscanf(str, "item %d", &num) == 1) && num >= 0 && num <= 3;
            test_validate(ok,
                          "FS random pattern mismatch: '%s'", str);
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 10. FS random without template (default "%d") and rndinc=4 */
    test_sub("subtest %d: UnlimRandom FS without template, rndinc=4", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimFsRnd(NULL, 4);
        for (int i = 0; i < 30; i++) {
            value64 v = v64GenNext(&gen);
            const char *str = fs_str(value64_fs(v));
            int num;
            bool ok = (sscanf(str, "%d", &num) == 1) && num >= 0 && num <= 4;
            test_validate(ok,
                          "FS random default pattern mismatch: '%s', num %d", 
                          str, num
            );
            value64_free(&v, gen.type);
        }
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 10: Source C-string to char generator (LONG lim) -------------------------
static TestStatus
tf10_gen_string_source(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Обычная строка "hello", безлимит (maxlen <= 0) */
    test_sub("subtest %d: v64GenStringToChar basic 'hello' unlimited", ++subnum);
    {
        v64Gen gen = v64GenCreatorSourceCstrToChar("hello", 0);

        const char expected[] = "hello";
        for (int i = 0; i < (int)strlen(expected); i++) {
            value64 v = v64GenNext(&gen);
            test_validatefree(
                value64_char(v) == expected[i],
                v64GenFree(&gen),
                "pos %d: expected '%c', got '%c'", i, expected[i], value64_char(v)
            );
            value64_free(&v, gen.type);
        }

        value64 v_end = v64GenNext(&gen);
        test_validatefree(
            value64_char(v_end) == '\0',
            v64GenFree(&gen),
            "after end expected '\\0', got '%c'", value64_char(v_end)
        );
        value64_free(&v_end, gen.type);

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 2. Пустая строка */
    test_sub("subtest %d: v64GenStringToChar empty string", ++subnum);
    {
        v64Gen gen = v64GenCreatorSourceCstrToChar("", 0);

        value64 v = v64GenNext(&gen);
        test_validate(value64_char(v) == '\0',
                      "empty string must return '\\0', got '%c'", value64_char(v));
        value64_free(&v, gen.type);

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 3. Ограничение maxlen: строка "hello", maxlen=3 – только "hel" */
    test_sub("subtest %d: v64GenStringToChar with maxlen=3", ++subnum);
    {
        int     cnt = 3;
        v64Gen  gen = v64GenCreatorSourceCstrToChar("hello", cnt);

        for (int i = 0; i < cnt; i++) {
            value64 v = v64GenNext(&gen);
            test_validate(value64_char(v) == "hel"[i],
                          "pos %d: expected '%c', got '%c'", i, "hel"[i], value64_char(v));
            value64_free(&v, gen.type);
        }
        for (int i = 0; i < 33; i++)  {
            value64 v = v64GenNext(&gen);
            test_validate(
                value64_char(v) == '\0',
                "after reading all %d iter generator must return only \\0, but got %c", cnt, value64_char(v)
            );
        }

        value64 v_end = v64GenNext(&gen);
        test_validate(value64_char(v_end) == '\0',
                      "after maxlen expected '\\0', got '%c'", value64_char(v_end));
        value64_free(&v_end, gen.type);

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 4. maxlen больше длины строки: строка "abc", maxlen=10 */
    test_sub("subtest %d: v64GenStringToChar maxlen > strlen", ++subnum);
    {
        v64Gen gen = v64GenCreatorSourceCstrToChar("abc", 10);

        for (int i = 0; i < 3; i++) {
            value64 v = v64GenNext(&gen);
            test_validate(value64_char(v) == "abc"[i],
                          "pos %d: expected '%c', got '%c'", i, "abc"[i], value64_char(v));
            value64_free(&v, gen.type);
        }

        value64 v_end = v64GenNext(&gen);
        test_validate(value64_char(v_end) == '\0',
                      "after string expected '\\0', got '%c'", value64_char(v_end));
        value64_free(&v_end, gen.type);

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 5. NULL источник должен безопасно возвращать '\0' */
    /*test_sub("subtest %d: v64GenStringToChar NULL source", ++subnum);
    {
        v64Gen gen = v64GenCreatorSourceCstrToChar(NULL, 0);

        value64 v = v64GenNext(&gen);
        test_validate(value64_char(v) == '\0',
                      "NULL source must return '\\0', got '%c'", value64_char(v));
        value64_free(&v, gen.type);

        v64GenFree(&gen);
        fs_alloc_check(true);
    }*/

    /* 6. Проверка отсутствия утечек (многократные вызовы) */
    test_sub("subtest %d: v64GenStringToChar no leaks (multiple reads)", ++subnum);
    {
        const char  pt[] = "abcdefghij";
        v64Gen gen = v64GenCreatorSourceCstrToChar(pt, 0);

        for (int i = 0; i < 20; i++) {
            value64 v = v64GenNext(&gen);
            if (i > (int) strlen(pt))
                test_validatefree(
                    value64_char(v) == '\0',
                    value64_free(&v, gen.type),
                    "%d iter must be \\0, but got %c", i, value64_char(v)
                );
            value64_free(&v, gen.type);
        }

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 11: Sequential chunks from growing fs -------------------------
static TestStatus
tf11_gen_string_chunks(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: sequential chunks reading from growing fs", ++subnum);
    {
        fs          buf = FS();
        int         total_len = 0;
        const int   iterations = 10;
        char        pt[] = "abcdefghijklmnopqrstuvwxyz";

        srand(42); // детерминированный seed для воспроизводимости

        for (int iter = 0; iter < iterations; iter++) {
            int chunk_len = rndint(sizeof(pt) - 2) + 1;
            
            {
                char c = pt[chunk_len];
                pt[chunk_len] = '\0';
                fs_catstr(&buf, pt);
                pt[chunk_len] = c;  // restore
            }
            // Генератор читает только эту порцию
            v64Gen gen = v64GenCreatorSourceCstrToChar(fs_str(&buf) + total_len, chunk_len);

            for (int j = 0; j < chunk_len; j++) {
                value64 v = v64GenNext(&gen);
                test_validate(
                    value64_char(v) == 'a' + j,
                    "iter %d chunk %d: expected '%c', got '%c'",
                    iter, j, 'a' + j, value64_char(v)
                );
            }

            // После прочтения лимита генератор должен вернуть '\0'
            value64 v_end = v64GenNext(&gen);
            test_validate(
                value64_char(v_end) == '\0',
                "iter %d: expected null after chunk, got '%c'",
                iter, value64_char(v_end)
            );
            value64_free(&v_end, VALUE64_CHR);

            total_len += chunk_len;
            v64GenFree(&gen);   // for the next
        }

        // Проверяем итоговую длину
        test_validate(total_len == (int)fs_len(&buf),
                      "total_len %d mismatch fs_len %zu", total_len, fs_len(&buf));

        fsfree(buf);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 12: Append to source generator -------------------------
static TestStatus
tf12_gen_string_append(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: append chunks to source generator", ++subnum);
    {
        fs          buf = fsinit(10000); // to avoid realloc, realoc isn't suported by v64GenCreatorSourceCstrToChar
        int         total_len = 0;
        const int   iterations = 10;
        char        pt[] = "abcdefghijklmnopqrstuvwxyz";

        // Генератор создаём один раз, источник пустой
        v64Gen gen = v64GenCreatorSourceCstrToChar(buf.v, 0);

        srand(42);

        for (int iter = 0; iter < iterations; iter++) {
            int chunk_len = rndint(sizeof(pt) - 2) + 1;   // 1..25

            // Добавляем порцию символов в буфер
            {
                char c = pt[chunk_len];
                pt[chunk_len] = '\0';
                fs_catstr(&buf, pt);
                pt[chunk_len] = c;
            }

            // Вычисляем актуальный адрес начала порции ПОСЛЕ fs_catstr
            //const char *chunk_start = fs_str(&buf) + total_len;

            // Перенастраиваем генератор на новую порцию
            v64GenUpdateLimit(&gen /*chunk_start, */ );

            // Читаем и проверяем символы
            for (int j = 0; j < chunk_len; j++) {
                value64 v = v64GenNext(&gen);
                test_validate(
                    value64_char(v) == 'a' + j,
                    "iter %d pos %d: expected '%c', got '%c'",
                    iter, j, 'a' + j, value64_char(v)
                );
                value64_free(&v, VALUE64_CHR);
            }

            // После порции должен быть '\0'
            value64 v_end = v64GenNext(&gen);
            test_validate(
                value64_char(v_end) == '\0',
                "iter %d: expected null after chunk", iter
            );
            value64_free(&v_end, VALUE64_CHR);

            total_len += chunk_len;
        }

        test_validate(total_len == (int)fs_len(&buf),
                      "total_len %d mismatch fs_len %zu", total_len, fs_len(&buf));

        v64GenFree(&gen);
        fsfree(buf);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 13: v64GenFSToChar()  simple test -------------------------

static TestStatus
tf13_gen_fs_stream_simple(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: fs stream reads appended chunks automatically", ++subnum);
    {
        fs buf = fsinit(10);          // начальный маленький буфер -> будут realloc'и
        int total_len = 0;
        const int iterations = 10;
        char pt[] = "abcdefghijklmnopqrstuvwxyz";

        v64Gen gen = v64GenCreatorSourceFsToChar(&buf);

        srand(42);

        for (int iter = 0; iter < iterations; iter++) {
            int chunk_len = rndint(sizeof(pt) - 2) + 1;

            // Добавляем порцию
            {
                char c = pt[chunk_len];
                pt[chunk_len] = '\0';
                fs_catstr(&buf, pt);
                pt[chunk_len] = c;
            }

            // Читаем порцию — генератор автоматически видит новые данные
            for (int j = 0; j < chunk_len; j++) {
                value64 v = v64GenNext(&gen);
                test_validate(value64_char(v) == 'a' + j,
                              "iter %d pos %d: expected '%c', got '%c'",
                              iter, j, 'a' + j, value64_char(v));
                value64_free(&v, VALUE64_CHR);
            }

            // После порции должен быть '\0' (генератор ещё не знает о новых данных)
            value64 v_end = v64GenNext(&gen);
            test_validate(value64_char(v_end) == '\0',
                          "iter %d: expected null after chunk", iter);
            value64_free(&v_end, VALUE64_CHR);

            total_len += chunk_len;
        }

        test_validate(total_len == (int) fs_len(&buf),
                      "total_len %d mismatch fs_len %zu", total_len, fs_len(&buf));

        v64GenFree(&gen);
        fsfree(buf);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 14: v64GenFSToFsByNewline()  simple test-------------------------

static TestStatus
tf14_gen_fs_bynewline(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: lines with empty and waiting", ++subnum);
    {
        // "abc\n\nqwertt" – после первого \n идёт пустая строка, затем qwertt без \n
        fs buf = fscopy("abc\n\nqwertt");
        v64Gen gen = v64GenCreatorSourceFsToFsByNewline(&buf);

        // 1-я строка "abc"
        value64 v1 = v64GenNext(&gen);
        test_validatefree(
            !fs_isnull(value64_fs(v1)),
            (value64_free(&v1, VALUE64_FS), v64GenFree(&gen) ),
            "fs v1 is null %p",  value64_fs(v1) 
        );
        test_validatefree(
            fs_cmpstr(value64_fs(v1), "abc\n") == 0,
            (value64_free(&v1, VALUE64_FS), v64GenFree(&gen) ),
            "line1 mismatch '%s'", value64_fs(v1)->v
        );
        value64_free(&v1, VALUE64_FS);

        // 2-я строка: пустая (из-за \n\n) — должна быть НЕ null и длина 0
        value64 v2 = v64GenNext(&gen);
        test_validatefree(
            !fs_isnull(value64_fs(v2)),
            (value64_free(&v2, VALUE64_FS), v64GenFree(&gen) ),
            "fs v2 is null"
        );
        test_validatefree(
            fs_len(value64_fs(v2)) == 1 && fs_cmpstr(value64_fs(v2), "\n") == 0,
            (value64_free(&v2, VALUE64_FS), v64GenFree(&gen) ),
            "expected empty line (not null) '%s'", value64_fs(v2)->v
        );
        value64_free(&v2, VALUE64_FS);

        // 3-я строка: данных нет — должна быть null fs (ожидание)
        value64 v3 = v64GenNext(&gen);
        test_validatefree(
            !fs_isnull(value64_fs(v3)),
            (value64_free(&v3, VALUE64_FS), v64GenFree(&gen) ),
            "fs v1 is null %p",  value64_fs(v1) 
        );
        test_validatefree(
            fs_cmpstr(value64_fs(v3), "qwertt") == 0,
            (value64_free(&v3, VALUE64_FS), v64GenFree(&gen) ),
            "line1 mismatch '%s'", value64_fs(v3)->v
        );
        value64_free(&v3, VALUE64_FS);

        // Финализация: остаток "qwertt"
        // value64 v_last = v64GenFinalize(&gen);
        // test_validatefree(
        //     !fs_isnull(value64_fs(v_last)),
        //     value64_free(&v_last, VALUE64_FS),
        //     "final line is null %p", value64_fs(v_last)
        // );
        // test_validatefree(
        //     fs_cmpstr(value64_fs(v_last), "qwertt") == 0,
        //     (value64_free(&v_last, VALUE64_FS), v64GenFree(&gen) ),
        //     "final line mismatch '%s'", value64_fs(v_last)->v
        // );
        // value64_free(&v_last, VALUE64_FS);

        // После финализации — null fs
        value64 v_end = v64GenNext(&gen);
        test_validatefree(
            fs_isnull(value64_fs(v_end)),
            (value64_free(&v_end, VALUE64_FS), v64GenFree(&gen) ),
            "expected null after finalize"
        );
        value64_free(&v_end, VALUE64_FS);

        v64GenFree(&gen);
        fsfree(buf);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 15: v64GenGetRemainingCount with fs char generator -------------------------
static TestStatus
tf15_gen_fs_remaining(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: remaining count decreases while reading fs char generator", ++subnum);
    {
        fs buf = fscopy("hello");
        v64Gen gen = v64GenCreatorSourceFsToChar(&buf);

        // В начале оставшихся символов должно быть равно длине строки
        unsigned long rem = v64GenUpdateLimit(&gen);    //v64GenGetRemainingCount(&gen);
        test_validatefree(
            rem == 5, 
            v64GenFree(&gen),
            "expected remaining 5, got %lu", rem
        );

        // Читаем два символа
        value64 v1 = v64GenNext(&gen);
        value64 v2 = v64GenNext(&gen);
        test_validatefree(
            value64_char(v1) == 'h' && value64_char(v2) == 'e',
            v64GenFree(&gen),
            "first two chars mismatch"
        );

        // После двух прочтений остаток должен быть 3
        rem = v64GenGetRemainingCount(&gen);
        test_validatefree(
            rem == 3, 
            v64GenFree(&gen),
            "expected remaining 3 after two reads, got %lu", rem
        );

        value64_free(&v1, VALUE64_CHR);
        value64_free(&v2, VALUE64_CHR);

        // Дочитываем оставшиеся три символа
        for (int i = 0; i < 3; i++) {
            value64 v = v64GenNext(&gen);
            value64_free(&v, VALUE64_CHR);
        }

        // Теперь остаток должен быть 0
        rem = v64GenGetRemainingCount(&gen);
        test_validatefree(
            rem == 0, 
            v64GenFree(&gen),
            "expected remaining 0 after full read, got %lu", rem
        );

        // Дальше генератор должен возвращать '\0' (конец)
        value64 v_end = v64GenNext(&gen);
        test_validatefree(
            value64_char(v_end) == '\0',
            v64GenFree(&gen),
            "expected null character after end"
        );
        value64_free(&v_end, VALUE64_CHR);

        v64GenFree(&gen);
        fsfree(buf);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: remaining count increases after appending data", ++subnum);
    {
        fs buf = fscopy("abc");
        v64Gen gen = v64GenCreatorSourceFsToChar(&buf);

        unsigned long rem = v64GenUpdateLimit(&gen);
        test_validatefree(
            rem == fslen(buf), 
            v64GenFree(&gen),
            "after full read remaining should be %zu, got %lu", fslen(buf), rem
        );

        // Читаем все три символа
        for (int i = 0; i < 3; i++) {
            value64 v = v64GenNext(&gen);
            value64_free(&v, VALUE64_CHR);
        }

        rem = v64GenUpdateLimit(&gen);
        test_validate(rem == 0, "after full read remaining should be 0, got %lu", rem);

        // Добавляем новые данные в буфер
        fs_catstr(&buf, "def");

        // Генератор должен увидеть новые данные, позиция осталась на 3
        rem = v64GenUpdateLimit(&gen);
        test_validatefree(
            rem == 3, 
            v64GenFree(&gen),
            "after appending remaining should be 3, got %lu", rem
        );

        // Читаем новые символы
        value64 v1 = v64GenNext(&gen);
        test_validatefree(
            value64_char(v1) == 'd', 
            v64GenFree(&gen),
            "expected 'd', got '%c'", value64_char(v1)
        );
        value64_free(&v1, VALUE64_CHR);

        rem = v64GenUpdateLimit(&gen);
        test_validatefree(
            rem == 2, 
            v64GenFree(&gen),
            "after reading one new char remaining should be 2, got %lu", rem
        );

        // Дочитываем остальные
        for (int i = 0; i < 2; i++) {
            value64 v = v64GenNext(&gen);
            value64_free(&v, VALUE64_CHR);
        }

        rem = v64GenUpdateLimit(&gen);
        test_validatefree(
            rem == 0, 
            v64GenFree(&gen),
            "after reading all new chars remaining should be 0, got %lu", rem
        );

        v64GenFree(&gen);
        fsfree(buf);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 16: FsToFsByNewline remaining count -------------------------
static TestStatus
tf16_gen_fs_bynewline_remaining(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: remaining count for FsToFsByNewline", ++subnum);
    {
        const char *data = "abc\n\nqwertt";
        fs buf = fscopy(data);
        v64Gen gen = v64GenCreatorSourceFsToFsByNewline(&buf);

        /* Начальный остаток равен длине буфера (11 символов) */
        unsigned long rem = v64GenUpdateLimit(&gen);
        test_validate(rem == 11, "initial remaining expected 11, got %lu", rem);

        const char *expected[] = { "abc\n", "\n", "qwertt" };
        int idx = 0;

        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);

            test_validatefree(!fs_isnull(value64_fs(v)),
                              (value64_free(&v, VALUE64_FS), v64GenFree(&gen), fsfree(buf)),
                              "Fs is null at line %d", idx);

            test_validatefree(fs_cmpstr(value64_fs(v), expected[idx]) == 0,
                              (value64_free(&v, VALUE64_FS), v64GenFree(&gen), fsfree(buf)),
                              "line %d mismatch, expected '%s' got '%s'",
                              idx, expected[idx], value64_fs(v)->v);
            value64_free(&v, VALUE64_FS);

            idx++;
        }

        test_validate(idx == 3, "expected 3 lines, got %d", idx);

        rem = v64GenUpdateLimit(&gen);
        test_validate(rem == 0, "after all lines remaining expected 0, got %lu", rem);

        v64GenFree(&gen);
        fsfree(buf);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 17: v64GenFSToStrByNewline simple test (hasnext/getnext) -------------------------
static TestStatus
tf17_gen_fs_tostr_bynewline(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: lines with empty and waiting (STR)", ++subnum);
    {
        const char *data = "abc\n\nqwertt";
        fs buf = fscopy(data);
        v64Gen gen = v64GenCreatorSourceFsToStrByNewline(&buf);

        const char *expected[] = { "abc\n", "\n", "qwertt" };
        int idx = 0;

        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);

            test_validatefree(value64_str(v) != NULL,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(buf)),
                              "STR is null at line %d", idx);

            test_validatefree(strcmp(value64_str(v), expected[idx]) == 0,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(buf)),
                              "line %d mismatch, expected '%s' got '%s'",
                              idx, expected[idx], value64_str(v));

            value64_free(&v, VALUE64_STR);
            idx++;
        }

        test_validatefree(
            idx == 3, 
            v64GenFree(&gen),
            "expected 3 lines, got %d", idx
        );

        v64GenFree(&gen);
        fsfree(buf);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}
// ------------------------- TEST 20: v64GenStreamUpdate with FILE* source -------------------------
static TestStatus
tf18_gen_stream_update_file(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: stream update after file append (two FILE*)", ++subnum);
    {
        const char *fname = "res/v64gen/stream_update_test.tmp";

        // Начальные данные
        FILE *w = fopen(fname, "w");
        test_validate(w != NULL, "fopen(w) failed");
        fwrite("abc", 1, 3, w);
        fclose(w);

        // Открываем два независимых FILE* на один файл
        FILE *fr = fopen(fname, "r");     // для генератора
        FILE *fa = fopen(fname, "a");     // для дозаписи
        test_validatefree(fr != NULL && fa != NULL,
                          (fr ? fclose(fr) : 0, fa ? fclose(fa) : 0),
                          "fopen(r/a) failed");

        v64Gen gen = v64GenCreatorSourceFileToChar(fr);

        // Начальный остаток = 3
        unsigned long rem = v64GenUpdateLimit(&gen);
        test_validatefree(rem == 3,
                          (v64GenFree(&gen), fclose(fr), fclose(fa)),
                          "initial remaining expected 3, got %lu", rem);

        // Читаем два символа 'a', 'b'
        value64 v1 = v64GenNext(&gen);
        value64 v2 = v64GenNext(&gen);
        test_validatefree(
            value64_char(v1) == 'a' && value64_char(v2) == 'b',
            (value64_free(&v1, VALUE64_CHR), value64_free(&v2, VALUE64_CHR),
             v64GenFree(&gen), fclose(fr), fclose(fa)),
            "first two characters mismatch"
        );
        value64_free(&v1, VALUE64_CHR);
        value64_free(&v2, VALUE64_CHR);

        // Остаток = 1 (ещё 'c')
        rem = v64GenUpdateLimit(&gen);
        test_validatefree(rem == 1,
                          (v64GenFree(&gen), fclose(fr), fclose(fa)),
                          "remaining after two chars expected 1, got %lu", rem);

        // Дописываем "def" в конец файла через отдельный поток
        fwrite("def", 1, 3, fa);
        fflush(fa);

        // Обновляем информацию о потоке
        v64GenUpdateLimit(&gen);   // amount игнорируется файловым updater

        // Теперь remaining должен быть 4: 'c', 'd', 'e', 'f'
        rem = v64GenUpdateLimit(&gen);
        test_validatefree(rem == 4,
                          (v64GenFree(&gen), fclose(fr), fclose(fa)),
                          "remaining after append expected 4, got %lu", rem);

        // Читаем оставшиеся 4 символа
        const char *expected_rest = "cdef";
        for (int i = 0; i < 4; i++) {
            value64 v = v64GenNext(&gen);
            test_validatefree(
                value64_char(v) == expected_rest[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fclose(fr), fclose(fa)),
                "char %d mismatch: expected '%c', got '%c'",
                i, expected_rest[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
        }

        // Конец файла -> должен вернуть '\0'
        value64 v_end = v64GenNext(&gen);
        test_validatefree(
            value64_char(v_end) == '\0',
            (value64_free(&v_end, VALUE64_CHR), v64GenFree(&gen), fclose(fr), fclose(fa)),
            "expected null after end"
        );
        value64_free(&v_end, VALUE64_CHR);

        rem = v64GenUpdateLimit(&gen);
        test_validatefree(rem == 0,
                          (v64GenFree(&gen), fclose(fr), fclose(fa)),
                          "remaining after end expected 0, got %lu", rem);

        v64GenFree(&gen);
        fclose(fr);
        fclose(fa);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: stream update without updater must fail", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimZero(VALUE64_INT);
        if (!try()) {
            v64GenUpdateLimit(&gen);
            // Если не выпало, это ошибка
            test_validate(false, "v64GenStreamUpdate must raise error when updater is NULL");
            v64GenFree(&gen);
        } else {
            test_validate(true, "v64GenStreamUpdate correctly raised error");
            v64GenFree(&gen);
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 19: v64GenCreatorSourceFileToCommonOutput simple test -------------------------
static TestStatus
tf19_gen_file_bynewline(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. FS version */
    test_sub("subtest %d: FS version lines", ++subnum);
    {
        const char *fname = "res/v64gen_file_bynewline_fs.tmp";
        FILE *w = fopen(fname, "w");
        test_validatefree(w != NULL, remove(fname), "fopen(w) failed");
        fwrite("line1\nline2\nlast", 1, 17, w);
        fclose(w);

        FILE *fr = fopen(fname, "r");
        test_validatefree(fr != NULL, remove(fname), "fopen(r) failed");

        v64Gen gen = v64GenCreatorSourceFileToCommonOutput(fr, VALUE64_FS);

        const char *expected[] = { "line1\n", "line2\n", "last" };
        int idx = 0;

        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);

            test_validatefree(!fs_isnull(value64_fs(v)),
                              (value64_free(&v, VALUE64_FS), v64GenFree(&gen), fclose(fr), remove(fname)),
                              "Fs is null at line %d", idx);
            test_validatefree(fs_cmpstr(value64_fs(v), expected[idx]) == 0,
                              (value64_free(&v, VALUE64_FS), v64GenFree(&gen), fclose(fr), remove(fname)),
                              "line %d mismatch: expected '%s' got '%s'",
                              idx, expected[idx], value64_fs(v)->v);
            value64_free(&v, VALUE64_FS);
            idx++;
        }

        test_validate(idx == 3, "expected 3 lines, got %d", idx);

        v64GenFree(&gen);
        fclose(fr);
        remove(fname);
        fs_alloc_check(true);
    }

    /* 2. STR version */
    test_sub("subtest %d: STR version lines", ++subnum);
    {
        const char *fname = "res/v64gen_file_bynewline_str.tmp";
        FILE *w = fopen(fname, "w");
        test_validatefree(w != NULL, remove(fname), "fopen(w) failed");
        fwrite("line1\nline2\nlast", 1, 17, w);
        fclose(w);

        FILE *fr = fopen(fname, "r");
        test_validatefree(fr != NULL, remove(fname), "fopen(r) failed");

        v64Gen gen = v64GenCreatorSourceFileToCommonOutput(fr, VALUE64_STR);

        const char *expected[] = { "line1\n", "line2\n", "last" };
        int idx = 0;

        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);

            test_validatefree(value64_str(v) != NULL,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fclose(fr), remove(fname)),
                              "STR is null at line %d", idx);
            test_validatefree(strcmp(value64_str(v), expected[idx]) == 0,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fclose(fr), remove(fname)),
                              "line %d mismatch: expected '%s' got '%s'",
                              idx, expected[idx], value64_str(v));
            value64_free(&v, VALUE64_STR);
            idx++;
        }

        test_validate(idx == 3, "expected 3 lines, got %d", idx);

        v64GenFree(&gen);
        fclose(fr);
        remove(fname);
        fs_alloc_check(true);
    }

    /* 3. Дозапись после EOF (STR) */
    test_sub("subtest %d: append after EOF (STR)", ++subnum);
    {
        const char *fname = "res/v64gen_file_bynewline_append.tmp";
        FILE *w = fopen(fname, "w");
        test_validatefree(w != NULL, remove(fname), "fopen(w) failed");
        fwrite("first\n", 1, 6, w);
        fclose(w);

        FILE *fr = fopen(fname, "r");
        FILE *fa = fopen(fname, "a");
        test_validatefree(fr && fa,
                          (fr ? fclose(fr) : 0, fa ? fclose(fa) : 0, remove(fname)),
                          "fopen failed");

        v64Gen gen = v64GenCreatorSourceFileToCommonOutput(fr, VALUE64_STR);

        // читаем первую строку
        value64 v;
        test_validatefree(v64GenHasnext(&gen), (v64GenFree(&gen), fclose(fr), fclose(fa), remove(fname)), "expected first line");
        v = v64GenNext(&gen);
        test_validatefree(strcmp(value64_str(v), "first\n") == 0,
                          (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fclose(fr), fclose(fa), remove(fname)),
                          "first line mismatch");
        value64_free(&v, VALUE64_STR);

        // после первой строки данных нет
        test_validatefree(!v64GenHasnext(&gen), (v64GenFree(&gen), fclose(fr), fclose(fa), remove(fname)), "expected no more after first");

        // дописываем вторую строку
        fwrite("second\n", 1, 7, fa);
        fflush(fa);
        v64GenUpdateLimit(&gen);   // сбрасываем EOF и пересчитываем лимит

        // читаем вторую строку
        test_validatefree(v64GenHasnext(&gen), (v64GenFree(&gen), fclose(fr), fclose(fa), remove(fname)), "expected second line after append");
        v = v64GenNext(&gen);
        test_validatefree(strcmp(value64_str(v), "second\n") == 0,
                          (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fclose(fr), fclose(fa), remove(fname)),
                          "second line mismatch");
        value64_free(&v, VALUE64_STR);

        v64GenFree(&gen);
        fclose(fr);
        fclose(fa);
        remove(fname);
        fs_alloc_check(true);
    }

    /* 4. remaining count */
    test_sub("subtest %d: remaining count", ++subnum);
    {
        const char *fname = "res/v64gen_file_bynewline_remaining.tmp";
        FILE *w = fopen(fname, "w");
        test_validatefree(w != NULL, remove(fname), "fopen(w) failed");
        fwrite("abc\n", 1, 4, w);
        fclose(w);

        FILE *fr = fopen(fname, "r");
        test_validatefree(fr != NULL, remove(fname), "fopen(r) failed");

        v64Gen gen = v64GenCreatorSourceFileToCommonOutput(fr, VALUE64_STR);

        unsigned long rem = v64GenUpdateLimit(&gen);
        test_validate(rem == 4, "initial remaining expected 4, got %lu", rem);

        value64 v = v64GenNext(&gen);
        value64_free(&v, VALUE64_STR);

        rem = v64GenUpdateLimit(&gen);
        test_validate(rem == 0, "after reading expected 0, got %lu", rem);

        v64GenFree(&gen);
        fclose(fr);
        remove(fname);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 20: Limited C-string to CHAR generator -------------------------
static TestStatus
tf20_gen_string_source_limited(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. maxlen equal to string length */
    test_sub("subtest %d: limit equals string length", ++subnum);
    {
        const char *text = "hello";
        v64Gen gen = v64GenCreatorSourceCstrToChar(text, 5);

        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == text[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
                "pos %zu: expected '%c', got '%c'", i, text[i], value64_char(v)
            );
            logauto(v.cval);
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(
            i == 5, 
            v64GenFree(&gen),
            "expected 5 chars, got %zu", i
        );

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 2. maxlen less than string length */
    test_sub("subtest %d: limit less than string length", ++subnum);
    {
        const char *text = "hello";
        v64Gen gen = v64GenCreatorSourceCstrToChar(text, 3);

        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == text[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
                "pos %zu: expected '%c', got '%c'", i, text[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validate(i == 3, "expected 3 chars, got %zu", i);

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 3. maxlen greater than string length */
    test_sub("subtest %d: limit greater than string length", ++subnum);
    {
        const char *text = "hi";
        v64Gen gen = v64GenCreatorSourceCstrToChar(text, 10);

        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == text[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
                "pos %zu: expected '%c', got '%c'", i, text[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(
            i == strlen(text) + 1, 
            (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
            "expected %zu chars, got %zu", strlen(text) + 1, i
        );

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 4. unlimited (maxlen=0) */
    test_sub("subtest %d: unlimited (maxlen=0)", ++subnum);
    {
        const char *text = "abc";
        v64Gen gen = v64GenCreatorSourceCstrToChar(text, 0);

        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == text[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
                "pos %zu: expected '%c', got '%c'", i, text[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(
            i == strlen(text) + 1, 
            (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
            "expected %zu chars, got %zu", strlen(text) + 1, i
        );

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 5. имитация дозаписи: после исчерпания лимита дополняем строку и продолжаем */
    test_sub("subtest %d: append data after limit", ++subnum);
    {
        char buf[64];
        const char *part1 = "hello";
        const char *part2 = " world";

        strcpy(buf, part1);
        v64Gen gen = v64GenCreatorSourceCstrToChar(buf, 5);

        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == part1[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
                "part1 pos %zu mismatch", i
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(
            i == strlen(part1), 
            (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
            "expected %zu chars from part1, got %zu", strlen(part1), i
        );

        // дописываем новые данные
        strcpy(buf + 5, part2);           // теперь "hello world"

        test_validatefree(
            !v64GenHasnext(&gen),
            v64GenFree(&gen),
            "Hasnext must return false before v64GenUpdateLimit()!"
        );

        v64GenUpdateLimit(&gen);          // сбрасываем лимит на unlimited

        test_validatefree(
            v64GenHasnext(&gen),
            v64GenFree(&gen),
            "Hasnext must return true after v64GenUpdateLimit()!"
        );

        i = 0;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == part2[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
                "part2 pos %zu mismatch", i
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(
            i == strlen(part2) + 1, 
            (value64_free(&v, VALUE64_CHR), v64GenFree(&gen)),
            "expected %zu chars from part2, got %zu", strlen(part2), i
        );

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: empty c-string source returns no data", ++subnum);
    {
        const char *empty = "";
        v64Gen gen = v64GenCreatorSourceCstrToChar(empty, -1L);   // unlimited, но пустая строка

        value64 v;
        bool has = v64GenGetIfHasnext(&gen, &v);
        test_validatefree(
            has == false,
            (v64GenFree(&gen)),
            "expected no data for empty string"
        );

        // Дополнительно проверяем, что v64GenNext тоже возвращает '\0'
        value64 res = v64GenNext(&gen);
        test_validatefree(
            value64_char(res) == '\0',
            (value64_free(&res, VALUE64_CHR), v64GenFree(&gen)),
            "expected null char from v64GenNext"
        );
        value64_free(&res, VALUE64_CHR);

        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 30: fs -> CHAR generator (limit, append) -------------------------
static TestStatus
tf21_gen_fs_char(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Чтение всего содержимого */
    test_sub("subtest %d: read all chars", ++subnum);
    {
        const char *text = "hello";
        fs src = fscopy(text);
        v64Gen gen = v64GenCreatorSourceFsToChar(&src);

        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == text[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fsfree(src)),
                "pos %zu: expected '%c', got '%c'", i, text[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(i == 5,
                          (v64GenFree(&gen), fsfree(src)),
                          "expected 5 chars, got %zu", i);

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 2. Дозапись после первоначального чтения */
    test_sub("subtest %d: append data and continue reading", ++subnum);
    {
        fs src = fscopy("hello");
        v64Gen gen = v64GenCreatorSourceFsToChar(&src);

        // читаем первые 5 символов
        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == "hello"[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fsfree(src)),
                "part1 pos %zu mismatch", i
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(i == 5,
                          (v64GenFree(&gen), fsfree(src)),
                          "expected 5 chars from part1, got %zu", i);

        // дозаписываем " world"
        fs_catstr(&src, " world");
        v64GenUpdateLimit(&gen);   // пересчитает limit = fs_len - position

        i = 0;
        const char *expect = " world";
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == expect[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fsfree(src)),
                "part2 pos %zu mismatch", i
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(i == strlen(expect),
                          (v64GenFree(&gen), fsfree(src)),
                          "expected %zu chars from part2, got %zu", strlen(expect), i);

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 3. Проверка что после конца данных возвращается false */
    test_sub("subtest %d: no more data after end", ++subnum);
    {
        fs src = fscopy("abc");
        v64Gen gen = v64GenCreatorSourceFsToChar(&src);

        value64 v;
        int count = 0;
        while (v64GenGetIfHasnext(&gen, &v)) {
            value64_free(&v, VALUE64_CHR);
            count++;
        }
        test_validatefree(count == 3,
                          (v64GenFree(&gen), fsfree(src)),
                          "expected 3 chars, got %d", count);

        // ещё раз убеждаемся, что цикл не пойдёт снова
        test_validatefree(
            !v64GenGetIfHasnext(&gen, &v),
            (v64GenFree(&gen), fsfree(src)),
            "expected no more data"
        );

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 1. Пустая строка fscopy("") – должно быть 0 символов */
    test_sub("subtest %d: empty fs (fscopy empty)", ++subnum);
    {
        fs src = fscopy("");
        v64Gen gen = v64GenCreatorSourceFsToChar(&src);

        value64 v;
        bool has = v64GenGetIfHasnext(&gen, &v);
        test_validatefree(
            has == false,
            (v64GenFree(&gen), fsfree(src)),
            "expected no chars for empty fs, but got hasnext=true"
        );

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 2. Неинициализированный FS() – тоже 0 символов */
    test_sub("subtest %d: uninitialized FS()", ++subnum);
    {
        fs src = FS();
        v64Gen gen = v64GenCreatorSourceFsToChar(&src);

        value64 v;
        bool has = v64GenGetIfHasnext(&gen, &v);
        test_validatefree(
            has == false,
            (v64GenFree(&gen), fsfree(src)),
            "expected no chars for FS(), but got hasnext=true"
        );

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 3. Полное чтение, затем вызов v64GenUpdateLimit без изменений */
    test_sub("subtest %d: update limit without appending data", ++subnum);
    {
        fs src = fscopy("abc");
        v64Gen gen = v64GenCreatorSourceFsToChar(&src);

        // читаем все 3 символа
        value64 v;
        int count = 0;
        while (v64GenGetIfHasnext(&gen, &v)) {
            value64_free(&v, VALUE64_CHR);
            count++;
        }
        test_validatefree(count == 3,
                          (v64GenFree(&gen), fsfree(src)),
                          "expected 3 chars, got %d", count);

        test_validatefree(
            !v64GenHasnext(&gen),
            (v64GenFree(&gen), fsfree(src)),
            "v64GenHasnext must return false because no data"
        );

        // вызываем обновление без изменения источника
        v64GenUpdateLimit(&gen);

        // данных быть не должно
        test_validatefree(
            !v64GenGetIfHasnext(&gen, &v),
            (v64GenFree(&gen), fsfree(src)),
            "after update without appending, hasnext must be false"
        );

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 4. Частичное чтение, затем дозапись и продолжение */
    test_sub("subtest %d: partial read, append, continue", ++subnum);
    {
        fs src = fscopy("hello");
        v64Gen gen = v64GenCreatorSourceFsToChar(&src);

        // читаем два символа
        value64 v1, v2;
        bool h1 = v64GenGetIfHasnext(&gen, &v1);
        bool h2 = v64GenGetIfHasnext(&gen, &v2);
        test_validatefree(
            h1 && h2 && value64_char(v1) == 'h' && value64_char(v2) == 'e',
            (value64_free(&v1, VALUE64_CHR), value64_free(&v2, VALUE64_CHR),
             v64GenFree(&gen), fsfree(src)),
            "first two chars mismatch"
        );
        value64_free(&v1, VALUE64_CHR);
        value64_free(&v2, VALUE64_CHR);

        // дозаписываем " world"
        fs_catstr(&src, " world");
        v64GenUpdateLimit(&gen);   // пересчитает limit = fs_len - position

        // читаем оставшиеся символы
        size_t i = 0;
        const char *expect = "llo world";
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == expect[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fsfree(src)),
                "pos %zu: expected '%c', got '%c'", i, expect[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(i == strlen(expect),
                          (v64GenFree(&gen), fsfree(src)),
                          "expected %zu chars, got %zu", strlen(expect), i);

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

        /* 5. Пустая fs, попытка чтения, затем дозапись и продолжение */
    test_sub("subtest %d: empty fs, read attempt, append, continue", ++subnum);
    {
        fs src = fscopy("");     // или FS()
        v64Gen gen = v64GenCreatorSourceFsToChar(&src);

        // Попытка чтения при пустом буфере
        value64 v;
        bool has = v64GenGetIfHasnext(&gen, &v);
        test_validatefree(
            has == false,
            (v64GenFree(&gen), fsfree(src)),
            "expected no data on empty fs"
        );

        // Дозаписываем строку
        const char *text = "hello";
        fs_catstr(&src, text);

        // Обновляем лимит: теперь должен появиться доступ к данным
        v64GenUpdateLimit(&gen);

        // Читаем все символы
        size_t i = 0;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == text[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fsfree(src)),
                "pos %zu: expected '%c', got '%c'", i, text[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(
            i == strlen(text),
            (v64GenFree(&gen), fsfree(src)),
            "expected %zu chars, got %zu", strlen(text), i
        );

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 33: FILE* -> CHAR generator -------------------------
static TestStatus
tf22_gen_file_char(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Обычное чтение из файла */
    test_sub("subtest %d: read chars from file", ++subnum);
    {
        FILE *fp = tmpfile();
        test_validatefree(fp != NULL, fclose(fp), "tmpfile failed");

        const char *text = "hello";
        fwrite(text, 1, strlen(text), fp);
        fflush(fp);
        rewind(fp);

        v64Gen gen = v64GenCreatorSourceFileToChar(fp);

        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == text[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fclose(fp)),
                "pos %zu: expected '%c', got '%c'", i, text[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(i == strlen(text),
                          (v64GenFree(&gen), fclose(fp)),
                          "expected %zu chars, got %zu", strlen(text), i);

        v64GenFree(&gen);
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 2. Пустой файл */
    test_sub("subtest %d: empty file", ++subnum);
    {
        FILE *fp = tmpfile();
        test_validatefree(fp != NULL, fclose(fp), "tmpfile failed");
        rewind(fp);

        v64Gen gen = v64GenCreatorSourceFileToChar(fp);

        value64 v;
        bool has = v64GenGetIfHasnext(&gen, &v);
        test_validatefree(
            has == false,
            (v64GenFree(&gen), fclose(fp)),
            "expected no data for empty file"
        );

        v64GenFree(&gen);
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 3. Дозапись после EOF */
    test_sub("subtest %d: append data after EOF and continue", ++subnum);
    {
        FILE *fp = tmpfile();
        test_validatefree(fp != NULL, fclose(fp), "tmpfile failed");

        const char *part1 = "hello";
        fwrite(part1, 1, strlen(part1), fp);
        fflush(fp);
        rewind(fp);

        v64Gen gen = v64GenCreatorSourceFileToChar(fp);

        // читаем первую часть
        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(value64_char(v) == part1[i],
                              (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fclose(fp)),
                              "part1 pos %zu mismatch", i);
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(i == strlen(part1),
                          (v64GenFree(&gen), fclose(fp)),
                          "expected %zu chars from part1, got %zu", strlen(part1), i);

        // дописываем вторую часть
        const char *part2 = " world";
        fseek(fp, 0, SEEK_END);
        fwrite(part2, 1, strlen(part2), fp);
        fflush(fp);
        // возвращаемся в позицию, где остановились (ftell после чтения = strlen(part1))
        fseek(fp, strlen(part1), SEEK_SET);

        v64GenUpdateLimit(&gen);   // пересчитает limit

        i = 0;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(value64_char(v) == part2[i],
                              (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fclose(fp)),
                              "part2 pos %zu mismatch", i);
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(i == strlen(part2),
                          (v64GenFree(&gen), fclose(fp)),
                          "expected %zu chars from part2, got %zu", strlen(part2), i);

        v64GenFree(&gen);
        fclose(fp);
        fs_alloc_check(true);
    }

        /* 4. Пустой файл, попытка чтения, затем дозапись и продолжение */
    test_sub("subtest %d: empty file, read attempt, append, continue", ++subnum);
    {
        FILE *fp = tmpfile();
        test_validatefree(fp != NULL, fclose(fp), "tmpfile failed");

        // Файл пустой, генератор создан
        v64Gen gen = v64GenCreatorSourceFileToChar(fp);

        value64 v;
        bool has = v64GenGetIfHasnext(&gen, &v);
        test_validatefree(
            has == false,
            (v64GenFree(&gen), fclose(fp)),
            "expected no data on empty file"
        );

        // Дозаписываем строку
        const char *text = "hello";
        fwrite(text, 1, strlen(text), fp);
        fflush(fp);

        // Возвращаемся в начало, чтобы updater корректно вычислил позицию
        fseek(fp, 0, SEEK_SET);

        // Обновляем лимит
        v64GenUpdateLimit(&gen);

        // Читаем все символы
        size_t i = 0;
        while (v64GenGetIfHasnext(&gen, &v)) {
            test_validatefree(
                value64_char(v) == text[i],
                (value64_free(&v, VALUE64_CHR), v64GenFree(&gen), fclose(fp)),
                "pos %zu: expected '%c', got '%c'", i, text[i], value64_char(v)
            );
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(
            i == strlen(text),
            (v64GenFree(&gen), fclose(fp)),
            "expected %zu chars, got %zu", strlen(text), i
        );

        v64GenFree(&gen);
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 5. Фиктивный v64GenUpdateLimit() без изменения файла */
    test_sub("subtest %d: update limit without file change", ++subnum);
    {
        FILE *fp = tmpfile();
        test_validatefree(fp != NULL, fclose(fp), "tmpfile failed");

        const char *text = "abc";
        fwrite(text, 1, strlen(text), fp);
        fflush(fp);
        rewind(fp);

        v64Gen gen = v64GenCreatorSourceFileToChar(fp);

        // Читаем все символы
        size_t i = 0;
        value64 v;
        while (v64GenGetIfHasnext(&gen, &v)) {
            value64_free(&v, VALUE64_CHR);
            i++;
        }
        test_validatefree(
            i == strlen(text),
            (v64GenFree(&gen), fclose(fp)),
            "expected %zu chars, got %zu", strlen(text), i
        );

        // Вызываем обновление без изменения размера
        v64GenUpdateLimit(&gen);

        // Данных быть не должно
        test_validatefree(
            !v64GenGetIfHasnext(&gen, &v),
            (v64GenFree(&gen), fclose(fp)),
            "expected no data after update without appending"
        );

        v64GenFree(&gen);
        fclose(fp);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 23: fs -> STR by newline generator -------------------------
static TestStatus
tf23_gen_fs_tostr_bynewline(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Обычные строки и пустая строка */
    test_sub("subtest %d: lines with empty and waiting", ++subnum);
    {
        const char *data = "abc\n\nqwertt";   // qwertt без \n
        fs src = fscopy(data);
        v64Gen gen = v64GenCreatorSourceFsToStrByNewline(&src);

        const char *expected[] = { "abc\n", "\n", "qwertt" };
        int idx = 0;

        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);

            test_validatefree(value64_str(v) != NULL,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(src)),
                              "STR is null at line %d", idx);
            test_validatefree(strcmp(value64_str(v), expected[idx]) == 0,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(src)),
                              "line %d mismatch, expected '%s' got '%s'",
                              idx, expected[idx], value64_str(v));

            value64_free(&v, VALUE64_STR);
            idx++;
        }

        test_validate(idx == 3, "expected 3 lines, got %d", idx);

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 2. Пустая fs */
    test_sub("subtest %d: empty fs", ++subnum);
    {
        fs src = fscopy("");
        v64Gen gen = v64GenCreatorSourceFsToStrByNewline(&src);

        test_validatefree(
            !v64GenHasnext(&gen),
            (v64GenFree(&gen), fsfree(src)),
            "expected no data for empty fs"
        );

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 3. Дозапись: после EOF добавляем новые строки */
    test_sub("subtest %d: append data after EOF and continue", ++subnum);
    {
        fs src = fscopy("line1\n");
        v64Gen gen = v64GenCreatorSourceFsToStrByNewline(&src);

        // Читаем первую строку
        size_t idx = 0;
        const char *part1[] = { "line1\n" };
        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);
            test_validatefree(value64_str(v) != NULL,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(src)),
                              "STR is null at first part");
            test_validatefree(strcmp(value64_str(v), part1[idx]) == 0,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(src)),
                              "first line mismatch, expected '%s' got '%s'",
                              part1[idx], value64_str(v));
            value64_free(&v, VALUE64_STR);
            idx++;
        }
        test_validate(idx == 1, "expected 1 line from part1, got %zu", idx);

        // После этого лимит должен быть 0
        unsigned long rem = v64GenUpdateLimit(&gen);
        test_validatefree(rem == 0,
                          (v64GenFree(&gen), fsfree(src)),
                          "expected remaining 0 after first line, got %lu", rem);

        // Дозаписываем "line2\n"
        fs_catstr(&src, "line2\n");
        v64GenUpdateLimit(&gen);   // пересчитывает limit

        // Читаем вторую строку
        idx = 0;
        const char *part2[] = { "line2\n" };
        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);
            test_validatefree(value64_str(v) != NULL,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(src)),
                              "STR is null at second part");
            test_validatefree(strcmp(value64_str(v), part2[idx]) == 0,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(src)),
                              "second line mismatch, expected '%s' got '%s'",
                              part2[idx], value64_str(v));
            value64_free(&v, VALUE64_STR);
            idx++;
        }
        test_validate(idx == 1, "expected 1 line from part2, got %zu", idx);

        // После этого снова 0
        rem = v64GenUpdateLimit(&gen);
        test_validatefree(rem == 0,
                          (v64GenFree(&gen), fsfree(src)),
                          "expected remaining 0 after second line, got %lu", rem);

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 4. Обновление лимита без дописывания не приводит к появлению данных */
    test_sub("subtest %d: update limit without append", ++subnum);
    {
        fs src = fscopy("one\n");
        v64Gen gen = v64GenCreatorSourceFsToStrByNewline(&src);

        // Читаем единственную строку
        size_t idx = 0;
        const char *only[] = { "one\n" };
        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);
            test_validatefree(value64_str(v) != NULL,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(src)),
                              "STR is null");
            test_validatefree(strcmp(value64_str(v), only[idx]) == 0,
                              (value64_free(&v, VALUE64_STR), v64GenFree(&gen), fsfree(src)),
                              "only line mismatch, expected '%s' got '%s'",
                              only[idx], value64_str(v));
            value64_free(&v, VALUE64_STR);
            idx++;
        }
        test_validate(idx == 1, "expected 1 line, got %zu", idx);

        unsigned long rem = v64GenUpdateLimit(&gen);
        test_validatefree(rem == 0,
                          (v64GenFree(&gen), fsfree(src)),
                          "expected remaining 0, got %lu", rem);

        // ещё раз обновляем без изменений
        v64GenUpdateLimit(&gen);

        test_validatefree(
            !v64GenHasnext(&gen),
            (v64GenFree(&gen), fsfree(src)),
            "expected no data after update without append"
        );

        v64GenFree(&gen);
        fsfree(src);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST Null generator (FS/STR) -------------------------
static TestStatus
tf24_gen_null(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. FS limited 3 */
    test_sub("subtest %d: Null FS limited 3", ++subnum);
    {
        v64Gen  gen = v64GenCreatorNull(VALUE64_FS, 3);
        int     count = 0;
        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);
            test_validatefree(value64_fs(v) != NULL && fs_isempty(value64_fs(v) ),
                              (v64GenFree(&gen)),
                              "expected fs == FS() but not null");
            // try to free just for fun
            value64_free(&v, VALUE64_FS);
            // try to clone
            fs clones = fs_clone(v.fsval);
            fs_sprintf(&clones, "bla bla");
            fsfree(clones);
            count++;
        }
        test_validate(count == 3, "expected 3 elements, got %d", count);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 2. STR limited 2 */
    test_sub("subtest %d: Null STR limited 2", ++subnum);
    {
        v64Gen gen = v64GenCreatorNull(VALUE64_STR, 2);
        int count = 0;
        while (v64GenHasnext(&gen)) {
            value64 v = v64GenNext(&gen);
            test_validatefree(value64_str(v) == NULL,
                              (v64GenFree(&gen)),
                              "expected NULL string");
            count++;
        }
        test_validate(count == 2, "expected 2 elements, got %d", count);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 3. Unsupported type must fail */
    test_sub("subtest %d: Null INT not supported", ++subnum);
    {
        if (!try()) {
            v64Gen gen = v64GenCreatorNull(VALUE64_INT, 1);
            test_validate(false, "must raise error");
            v64GenFree(&gen);
        } else {
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }
    /* 4. FS unlimited – read 5 values */
    test_sub("subtest %d: Null FS unlimited (5 reads)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimNull(VALUE64_FS);
        const int   reads = 5;
        int         count = 0;
        for (int i = 0; i < reads; i++) {
            test_validatefree(v64GenHasnext(&gen),
                              (v64GenFree(&gen)),
                              "expected hasnext true");
            value64 v = v64GenNext(&gen);
            test_validatefree(value64_fs(v) != NULL && fs_isempty(value64_fs(v) ),
                              (v64GenFree(&gen)),
                              "expected FS() , but not null");
            count++;
        }
        test_validate(count == reads, "expected %d reads, got %d", reads, count);
        v64GenFree(&gen);
        fs_alloc_check(true);
    }

    /* 5. STR unlimited – read 5 values */
    test_sub("subtest %d: Null STR unlimited (5 reads)", ++subnum);
    {
        v64Gen gen = v64GenCreatorUnlimNull(VALUE64_STR);
        const int reads = 5;
        int count = 0;
        for (int i = 0; i < reads; i++) {
            test_validatefree(v64GenHasnext(&gen),
                              (v64GenFree(&gen)),
                              "expected hasnext true");
            value64 v = v64GenNext(&gen);
            test_validatefree(value64_str(v) == NULL,
                              (v64GenFree(&gen)),
                              "expected NULL string");
            count++;
        }
        test_validate(count == reads, "expected %d reads, got %d", reads, count);
        v64GenFree(&gen);
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
        TESTADD(tf1_gen_init_free,              "Simple init and validate test")
      , TESTADD(tf2_gen_next_zero,              "v64GenNext (zero) simple test")
      , TESTADD(tf3_gen_asc_series,             "value64UncheckGenUnlimAscSeries() simple test")
    //  , TESTADD(tf4_gen_creators,               "v64gen creators (simple wrappers) simple test")
      , TESTADD(tf5_gen_asc_rnd,                "v64GenAscRnd() simple test")
      , TESTADD(tf6_gen_asc_rnd_custom,         "AscRnd with custom rndinc simple test")
      , TESTADD(tf7_gen_desc_series,            "v64GenDescSeries() simple test")
      , TESTADD(tf8_gen_desc_rnd_custom,        "DescRnd with custom rndinc simple test")
      , TESTADD(tf9_gen_unlim_random,           "v64GenCreatorUnlimRnd...() generators simple test")
      , TESTADD(tf10_gen_string_source,         "Source C-string to char generator (LONG lim)")
      , TESTADD(tf11_gen_string_chunks,         "Sequential chunks from growing fs test")
      , TESTADD(tf12_gen_string_append,         "Append to source generator test")
      , TESTADD(tf13_gen_fs_stream_simple,      "v64GenFSToChar()  simple test")
      , TESTADD(tf14_gen_fs_bynewline,          "v64GenFSToFsByNewline()  simple test")
      , TESTADD(tf15_gen_fs_remaining,          "v64GenGetRemainingCount with fs char generator")
      , TESTADD(tf16_gen_fs_bynewline_remaining, "v64GenGetRemainingCount with FsToFsByNewlin")
      , TESTADD(tf17_gen_fs_tostr_bynewline,    "v64GenFSToStrByNewline() simple test")
      , TESTADD(tf18_gen_stream_update_file,    "v64GenCreatorSourceFileToChar() simple test")
      , TESTADD(tf19_gen_file_bynewline,        "v64GenCreatorSourceFileToCommonOutput() simple test")
      , TESTADD(tf20_gen_string_source_limited, "Limited C-string to CHAR generator simple test")
      , TESTADD(tf21_gen_fs_char,               "fs -> CHAR generator (limit, append) simple test")
      , TESTADD(tf22_gen_file_char,             "FILE* -> CHAR generator simple test")
      , TESTADD(tf23_gen_fs_tostr_bynewline,    "fs -> STR by newline generator simple test")
      , TESTADD(tf24_gen_null,                  "v64GenCreatorNull (FS/STR) simple test")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* V64GEN_TESTING */

