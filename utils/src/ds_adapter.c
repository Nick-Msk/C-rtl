
#include "ds_adapter.h"

/********************************************************************
                 DS - fs adapter MODULE IMPLEMENTATION
********************************************************************/

// ------------------------------ Utilities ------------------------

/**
 * @brief Internal helper to print formatted string into a fixed-size buffer.
 * 
 * This function calculates the required size first, then writes the content.
 * It ensures that the buffer does not overflow and handles null-termination.
 *
 * @param[in] ptr      Pointer to the start of the destination buffer.
 * @param[in] pos      Current write position (offset) in the buffer.
 * @param[in] cap      Total capacity of the buffer (limit).
 * @param[in] fmt      Format control string.
 * @param[in] ap       Variable argument list.
 * 
 * @return The number of characters written (excluding null terminator), 
 *         or -1 if the buffer is too small or an error occurred.
 */
static int                      dsHelperVPrintStr(char *ptr, size_t pos, size_t cap, const char *fmt, va_list ap) {
    int needed = vsnprintf(NULL, 0, fmt, ap);
    if (needed < 0)
        return userraise(-1, ERR_STREAM_ERROR, "Unable to vsnprintf NULL");
    if ((size_t) needed + pos + 1 > cap)
        return -1;

    int written = vsnprintf(ptr + pos, cap - pos, fmt, ap);

    if (written < 0) {
        return userraise(-1, ERR_STREAM_ERROR, "Unable to vsnprintf");
    }
    return written;
}

/**
 * @brief Helper that performs sscanf with automatic position advance.
 *
 * The format string is extended with a trailing " %n" to capture the
 * number of consumed characters.  The position `*ppos` is updated
 * accordingly.  This requires a second pass, therefore a copy of the
 * variadic argument list is used.
 *
 * @param buf   null‑terminated input buffer
 * @param ppos  pointer to the current read offset (will be updated)
 * @param fmt   scanf‑style format string
 * @param ap    variadic argument list (as passed to vfscanf)
 * @return      number of successfully matched items, or a negative value on error
 */
static int                      dsHelperVScanf(const char *buf, size_t cap, size_t *ppos, const char *fmt, va_list ap) {
    size_t remaining = cap - *ppos;
    if (remaining == 0)
        return userraise(-1, ERR_OUT_OF_BUFFER, "Buffer exhausted");

    FILE *mem = fmemopen((void *) (buf + *ppos), remaining, "r");
    if (!mem)
        return userraise(-1, ERR_UNABLE_OPEN_FILE_READ, "Unable to fmemopen");

    int ret = vfscanf(mem, fmt, ap);
    if (ret < 0) {
        fclose(mem);
        return userraise(-1, ERR_STREAM_ERROR, "vfscanf error");
    }
    long offset = ftell(mem);
    fclose(mem);

    if (offset > 0)
        *ppos += (size_t) offset;
    return ret;
}

/**
 * @brief Internal helper to parse an integer from a c-string.
 *
 * This function uses @c strtol to convert a string to a long, then verifies 
 * that the result fits within the bounds of a standard integer (@c INT_MIN to @c INT_MAX).
 *
 * @param[in]  str  The source null-terminated string.
 * @param[out] plval Pointer to the long where the parsed value will be stored.
 *
 * @return true if parsing was successful and the value is within integer bounds, 
 *         false otherwise (raises error via @c userraise).
 */
static bool                     dsHelperParseLong(const char *restrict str, long *restrict plval, size_t *restrict pos) {
    char    *endptr;
    errno = 0;
    long    val = strtol(str, &endptr, 10);

    if (str == endptr)
        return userraise(false, ERR_UNABLE_PARSE_DATA, "err parse int/long %ld, errno %s", val, strerror(errno));
    
    *pos = endptr - str;
    if (plval)
        *plval = val;
    return true;
}

/**
 * @brief Wrapper for parsing int, including bounds checking.
 */
static bool                     dsHelperParseInt(const char *restrict str, int *restrict pival, size_t *restrict pos) {
    long temp_val;
    if (!dsHelperParseLong(str, &temp_val, pos)) {
        return false; 
    }
    // check int borders
    if (temp_val > INT_MAX || temp_val < INT_MIN)
        return userraise(false, ERR_UNABLE_PARSE_DATA, "value %ld out of int range", temp_val);
    
    if (pival)
        *pival = (int)temp_val;
    return true;
}

/**
 * @brief Internal helper to parse a double from a null-terminated string.
 *
 * @param[in]  str    The source string.
 * @param[out] pdval  Pointer to store the double value.
 *
 * @return true if parsing was successful, false otherwise.
 */
static bool                     dsHelperParseDouble(const char *restrict str, double *restrict pdval, size_t *restrict pos) {
    char *endptr;
    errno = 0;
    double val = strtod(str, &endptr);

    // Проверяем, что:
    // 1. endptr не равен str (значит, хотя бы одна цифра была прочитана)
    // 2. errno не содержит ошибок (например, переполнение RANGE)
    if (str == endptr || errno != 0) 
        return userraise(false, ERR_UNABLE_PARSE_DATA, "err parse double, errno %s", strerror(errno));

    *pos += endptr - str;
    if (pdval)
        *pdval = val;
    return true;
}

/**
 * @brief Internal helper to parse a char from a string.
 */
static bool                     dsHelperParseChar(const char *restrict str, char *restrict pval, size_t *restrict pos) {
    // Skip leading whitespace to find the first character
    size_t skip = 0;
    while (str[skip] && isspace((unsigned char)str[skip]))
        skip++;
    if (str[skip] == '\0')
        return userraise(false, ERR_UNABLE_PARSE_DATA, "Empty or whitespace string for char");
    
    *pos += skip;          // spaces
    if (pval)
        *pval = str[skip];
    *pos += 1;             // sym
    return true;
}


/**
 * @brief Internal helper to parse an unsigned long from a string.
 */
static bool                     dsHelperParseUnsignedLong(const char *restrict str, unsigned long *restrict plval, size_t *restrict pos) {
    char *endptr;
    errno = 0;
    unsigned long val = strtoul(str, &endptr, 10);

    if (str == endptr || errno != 0) {
        return userraise(false, ERR_UNABLE_PARSE_DATA, "err parse unsigned long, errno %s", strerror(errno));
    }

    *pos += endptr - str;
    if (plval)
        *plval = val;
    return true;
}

/**
 * @brief Wrapper for parsing unsigned int, including bounds checking.
 */
static bool                     
dsHelperParseUnsigned(const char *restrict str, unsigned *restrict pival, size_t *restrict pos) {
    unsigned long temp_val;
    if (!dsHelperParseUnsignedLong(str, &temp_val, pos) )
        return false; 

    // check int borders
    if (temp_val > UINT_MAX)
        return userraise(false, ERR_UNABLE_PARSE_DATA, "value %ld out of int range", temp_val);
    
    if (pival)
        *pival = (int)temp_val;
    return true;
}


// bool
// dsHelperParseEscapedString(DS *restrict in, char *restrict dst, size_t dst_capacity, size_t *restrict out_len) {
    
//     size_t      pos = dsSavepos(in);                       // запоминаем позицию
//     bool        error = false;
//     size_t      len = 0;

//     if (dst_capacity == 0)
//         return userraise(false, ERR_WRONG_INPUT_PARAMETERS, "capacity can't be 0");

//     int c = dsgetc(in);
//     if (c != '"')
//         error = true;

//     while (!error && (c = dsgetc(in)) != EOF && c != '"') {

//         if (c == '\\') {
//             if (!dsgetcEscaped(in, &c)) {
//                 error = true; // Ошибка, если после '\' ничего нет или неизвестный символ
//                 break;
//             }
//         }
//         if (len + 1 >= dst_capacity) {
//             error = true;             // never shoud be here if normal serialization 
//             logsimple("WARN: len + 1 > dst_capacity (%zu)", dst_capacity);
//         } else 
//             dst[len++] = (unsigned char) c;
//     }
//     if (c != '"')
//         error = true;

//     if (out_len)
//         *out_len = len;
//     dst[len] = '\0';        // must setup \0 even of error

//     if (error) {
//         dsRestorepos(in, pos);                 // rollback only if error
//         return userraise(false, ERR_UNABLE_PARSE_DATA, 
//             "Unable to parse quoted line!");
//     }            

//     return true;
// }

/*
 * Core engine for parsing quoted strings.
 * It reads from 'in' and writes processed characters to 'out'.
 */
static bool                     
ds_parse_quoted_core(DS *restrict in, DS *restrict out, size_t maxlen, unsigned char begin, unsigned char end, bool word) {
    invraisecode(in != NULL && out != NULL, ERR_NULLABLE_PTR, 
        "Null pointers %p %p", in, out);
    invraisecode(out->type == DS_STR || out->type == DS_FS, ERR_UNSUPPORTED_TYPE,
        "Not suppoted type for out: %d/%s", out->type, dsTypeName(out->type) );

    bool        error = false;
    size_t      pos = dsSavepos(in);
    bool        quot = begin != '\0';   // check if quoted, word is ignored in that case
    bool        str = !quot && !word;   // parse till EOF or \n
    bool        stop = false, startword = false;
    int         c;

    if (quot) {
        c = dsgetc(in);
        if (c != begin)
            error = true;
    }

    while (!error && !stop && (c = dsgetc(in)) != EOF) {

        if (quot) {
            if ((unsigned char) c == end) 
                break;   // закрывающая кавычка
            if (c == '\\') {
                if (!dsgetcEscaped(in, &c)) {
                    error = true; // Ошибка, если после '\' ничего нет или неизвестный символ
                    break;
                }
            }
        } else if (str && end != '\0' && (unsigned char) c == end)     // '\n'
            stop = true;                               
        else if (word) {
            if (!startword) {
                if (isspace((unsigned char) c) )
                    continue;
                else if (isalnum_u((unsigned char) c) )
                    startword = true;
                else {        // wrong symbol!
                    logsimple("Wrong word symbol '%c'", c);
                    error = true;
                    break;
                }
            } else if (startword && !isalnum_u((unsigned char) c) ) {         // end of word
                dsungetc(c, in);
                break;
            }
        }

        // for every parsing type!
        if (maxlen > 0L && out->pos + 1 >= maxlen) { // if maxlen == 0 - UNLIM
            error = true;             // never shoud be here if normal serialization 
            logsimple("WARN: len (%zu) + 1 > dst_capacity (%zu)", out->pos, maxlen);
            break;
        } 
        if (dsputc(c, out) < 0) {
            userraise(false, ERR_STREAM_ERROR, "out ds stream error!"); // no return here!
            error = true;
            break;
        }
    }

    dsputc(EOF, out);      // out is DS_FS or DS_STR. Set it Even if error!!!

    if (quot && c != end) {
        logsimple("quoted: final \" not found");
        error = true;
    }
    if (word && !startword) {
        logsimple("Can't pars a word!");
        error = true;
    }

    if (error) {
        dsRestorepos(in, pos);                 // rollback only if error
        return userraise(false, ERR_UNABLE_PARSE_DATA, 
            "Unable to parse %s %s!", quot ? "quoted": "", word ? "word": "line");
    }

    return true;
}

/**
 * @brief Internal helper to format technical metadata of an @ref fs object into a buffer.
 *
 * This function is used by @ref fs_dstechprint to generate a diagnostic string 
 * describing the state of an @ref fs object. It captures the length, size, 
 * flags, and a truncated snippet of the actual data.
 *
 * The formatted string follows this pattern:
 * @code
 * FS: <name>: len [<len>], sz [<sz>], flags [<flags>], s [<data>]
 * @endcode
 * 
 * If the data content exceeds @c FS_TECH_PRINT_COUNT, the string is truncated 
 * and appended with @c "..." to indicate remaining data.
 *
 * @details 
 * The function uses a position-based writing approach (`fs_sprintf_position`) 
 * to allow for sequential building of the diagnostic string. It tracks the 
 * delta of the position to return the total number of bytes appended.
 *
 * @param[in,out] out   The destination @ref fs object where the diagnostic 
 *                      string will be written.
 * @param[in]     pos   The starting position (offset) within the @ref out 
 *                      object where writing should begin.
 * @param[in]     s     The source @ref fs object to be inspected. 
 *                      If @c NULL, a "<NULL>" placeholder is written.
 * @param[in]     name  A label representing the object being inspected.
 *
 * @return The number of bytes appended to the @ref out object (the delta 
 *         of @p pos). Returns -1 if any write operation fails.
 *
 * @note This function is for debugging purposes only and is not intended 
 *       for use in production data serialization.
 */
static long                     
dsfsHelperTechprintTofs(fs *restrict out, size_t pos, const fs *restrict s, const char *restrict name) {
    long    initpos = pos;
    if (s) {
        size_t     len = MIN(FS_TECH_PRINT_COUNT, s->len);
        pos += WRITE_OR_RET(fs_sprintf_position(out, pos, 
            "FS: %s: len [%zu], sz [%zu], flags [%d], s [", name, s->len, s->sz, s->flags), -1L);
        
        if (s->v)
            pos += WRITE_OR_RET(fs_sprintf_position(out, pos, "%.*s", (unsigned) len, s->v), -1L);
        else
            pos += WRITE_OR_RET(fs_sprintf_position(out, pos, "<NULL>"), -1);

        if (FS_TECH_PRINT_COUNT < s->len)
            pos += WRITE_OR_RET(fs_sprintf_position(out, pos, "..."), -1L);
        pos += WRITE_OR_RET(fs_sprintf_position(out, pos, "]\n"), -1L);
    } else 
        pos += WRITE_OR_RET(fs_sprintf_position(out, pos, "FS: %s: <NULL>\n", name), -1);
    return pos - initpos; 
}
/**
 * @brief Internal helper to wtire fs into a @ref DS stream.
 */
static long
dsfsHelperFsDSWrite(DS *restrict out, const fs *restrict s) {
    // just use direct write
    return dswrite(out, s->v, s->len);
}

static DS
dsPrepareout(fs *restrict dst, bool use_buffer, size_t maxlen) {
    fs      tmp = FS();         // стековая структура с флагом FS_FLAG_ALLOC, но без BODYALLOC
    fs     *buf = use_buffer ? &tmp : dst;

    if (maxlen > 0)
        fs_resize(buf, maxlen);     // not necessary but for opt

    fs_setlen(buf, 0);  // WA until normal fs_cmp/fs_cmpstr
    return dsCreatefs(buf);
}

// --------------------------- API ---------------------------------

int                         
dsPrintf(DS *restrict pds, const char *restrict msg, ...) {
    invraisecode(pds != NULL && msg != NULL, ERR_NULLABLE_PTR, 
        "Null input %p %p", pds, msg);

    va_list     ap;
    int         total = 0, wr;
    va_start(ap, msg);
    switch (pds->type) {
        case DS_FILE:
            total += WRITE_OR_RET(vfprintf(pds->fp, msg, ap), -1);
            break;
        case DS_STR: // this is NOT autoextendable, till end of pds->ptr only
            wr = WRITE_OR_RET(dsHelperVPrintStr(pds->ptr, pds->pos, pds->cap, msg, ap), -1);
            total += wr;
            pds->pos += wr;
            break;
        case DS_FS:     // this is autoextendable
            wr = WRITE_OR_RET(fs_vsprintf_position(&pds->s, pds->pos, msg, ap), -1);
            total += wr;
            pds->pos += wr; // iterator over fs pds->s
            break;
        default:
            va_end(ap); // for lulz
            return userraise(-1, ERR_ACTION_NOT_APPLICABLE, 
                "Can't write to  %d/%s", pds->type, dsTypeName(pds->type) );
    }
    va_end(ap);
    return total;
}

int                         
dsScanf(DS *restrict pds, const char *restrict msg, ...) {
    invraisecode(pds != NULL && msg != NULL, ERR_NULLABLE_PTR, 
        "Null input %p %p", pds, msg);
    
    va_list     ap;
    int         ret;
    va_start(ap, msg);

    switch (pds->type) {
        case DS_FILE: {
            ret = vfscanf(pds->fp, msg, ap);
            break;
        }
        case DS_STR:
        case DS_FS:
        case DS_CONSTSTR: {
            const char *buf = dsStrbuf(pds);
            ret = dsHelperVScanf(buf, pds->type == DS_FS ? pds->s.len : pds->cap, &pds->pos, msg, ap);
            }
            break;
        default:
            ret = userraise(-1, ERR_UNSUPPORTED_TYPE, "Unsupported %s", dsTypeName(pds->type) );
    }

    va_end(ap);
    return ret;
}

bool                        
dsParseInt(DS *restrict pds, int *restrict pval) {
    invraisecode(pds != NULL && pval != NULL, ERR_NULLABLE_PTR, 
        "Null input %p %p", pds, pval);
    switch (pds->type) {
        case DS_FILE:
            if (fscanf(pds->fp, "%d", pval) != 1)
                return userraise(false, ERR_UNABLE_PARSE_DATA, "Failed to parse int from file");
            break;
        case DS_STR:
        case DS_FS:
        case DS_CONSTSTR: 
                return dsHelperParseInt(dsStrbuf(pds) + pds->pos, pval, &pds->pos);
            break;
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", dsTypeName(pds->type) );
    }
    return true;
}

bool                        
dsParseLong(DS *restrict pds, long *restrict pval) {
    invraisecode(pds != NULL && pval != NULL, ERR_NULLABLE_PTR, 
        "Null input %p %p", pds, pval);
    switch (pds->type) {
        case DS_FILE:
            if (fscanf(pds->fp, "%ld", pval) != 1)
                return userraise(false, ERR_UNABLE_PARSE_DATA, "Failed to parse int from file");
            break;
        case DS_STR:
        case DS_FS:
        case DS_CONSTSTR: 
                return dsHelperParseLong(dsStrbuf(pds) + pds->pos, pval, &pds->pos);
            break;
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", dsTypeName(pds->type) );
    }
    return true;
}

bool                        
dsParseUnsigned(DS *restrict pds, unsigned int *restrict pval) {
    invraisecode(pds != NULL && pval != NULL, ERR_NULLABLE_PTR, "Null input %p %p", pds, pval);
    switch (pds->type) {
        case DS_FILE:
            if (fscanf(pds->fp, "%u", pval) != 1)
                return userraise(false, ERR_UNABLE_PARSE_DATA, "Failed to read unsigned int from file");
            break;
        case DS_STR:
        case DS_FS:
        case DS_CONSTSTR:
            return dsHelperParseUnsigned(dsStrbuf(pds) + pds->pos, pval, &pds->pos);
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", dsTypeName(pds->type));
    }
    return true;
}

bool                        
dsParseUnsignedLong(DS *restrict pds, unsigned long *restrict pval) {
    invraisecode(pds != NULL && pval != NULL, ERR_NULLABLE_PTR, "Null input %p %p", pds, pval);
    switch (pds->type) {
        case DS_FILE:
            if (fscanf(pds->fp, "%lu", pval) != 1)
                return userraise(false, ERR_UNABLE_PARSE_DATA, "Failed to read unsigned long from file");
            break;
        case DS_STR:
        case DS_FS:
        case DS_CONSTSTR:
            return dsHelperParseUnsignedLong(dsStrbuf(pds) + pds->pos, pval, &pds->pos);
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", dsTypeName(pds->type));
    }
    return true;
}

bool                        
dsParseDouble(DS *restrict pds, double *restrict pdval) {
    invraisecode(pds != NULL && pdval != NULL, ERR_NULLABLE_PTR, 
        "Null input %p %p", pds, pdval);

    switch (pds->type) {
        case DS_FILE: {
            // %lf - double в fscanf
            if (fscanf(pds->fp, "%lf", pdval) != 1)
                return userraise(false, ERR_UNABLE_PARSE_DATA, "Failed to parse double from file");
            break;
        }
        case DS_STR:
        case DS_FS:
        case DS_CONSTSTR: {
            return dsHelperParseDouble(dsStrbuf(pds) + pds->pos, pdval, &pds->pos);
        }
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", dsTypeName(pds->type));
    }
    return true;
}

bool                        
dsParseChar(DS *restrict pds, char *restrict pval) {
    invraisecode(pds != NULL && pval != NULL, ERR_NULLABLE_PTR, "Null input %p %p", pds, pval);

    switch (pds->type) {
        case DS_FILE:
            if (fscanf(pds->fp, " %c", pval) != 1) // " %c" skips whitespace
                return userraise(false, ERR_UNABLE_PARSE_DATA, "Failed to read char from file");
            break;
        case DS_STR: case DS_FS: case DS_CONSTSTR:
            return dsHelperParseChar(dsStrbuf(pds) + pds->pos, pval, &pds->pos);
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", dsTypeName(pds->type));
    }
    return true;
}

bool                      
dsParseQuotedLimfs(DS *restrict in, fs *restrict dst, size_t maxlen, bool use_buffer) {
    if (in == NULL || dst == NULL || !fs_alloc(dst))
        return userraiseint(ERR_NULL_INPUT, "%p %p/%s", in, dst, bool_str(fs_alloc(dst)) );

    DS      outtmp = dsPrepareout(dst, use_buffer, maxlen);

    bool res = ds_parse_quoted_core(in, &outtmp, maxlen, '"', '"', false);
    if (!res) {
        dsFree(&outtmp);
        return userraise(false, ERR_UNABLE_PARSE_DATA, "Unable to parse quoted fs");
    }
    dsReleaseFs(dst, &outtmp);   

    return true;
}

bool 
dsParseQuotedLimString(DS *restrict in, char *restrict dst, size_t dst_capacity, size_t *restrict out_len, bool use_buffer) {
    if (in == NULL || dst == NULL || dst_capacity == 0)
        return userraise(false, ERR_NULL_INPUT, 
            "Null input or zero capacity %p %p %zu", in, dst, dst_capacity);

    fs      tmp = (fs) {.v = dst, .len = dst_capacity - 1, .sz = dst_capacity, .flags = FS_FLAG_STATIC};   // static

    DS      outtmp = dsPrepareout(&tmp, use_buffer, dst_capacity);

    // exec core, quoted line
    bool res = ds_parse_quoted_core(in, &outtmp, dst_capacity, '"', '"', false);
    if (!res) {
        dsFree(&outtmp);
        return userraise(false, ERR_UNABLE_PARSE_DATA, "Unable to parse quoted fs");
    }
    if (out_len)
        *out_len = dsGetpos(&outtmp);   // фактически записанная длина

    if (use_buffer) {
        memcpy(dst, outtmp.s.v, dsGetpos(&outtmp) + 1);
        dsFree(&outtmp);
    }   

    return res;
}

bool                       
dsParseUnlimfs(DS *restrict in, fs *restrict dst, bool use_buffer) {
    if (in == NULL || dst == NULL || !fs_alloc(dst))
        return userraiseint(ERR_NULL_INPUT, "%p %p/%s", in, dst, bool_str(fs_alloc(dst)) );

    DS      outtmp = dsPrepareout(dst, use_buffer, 0L);

    // \0 - non-espaced mode, \n - line terminator
    bool res = ds_parse_quoted_core(in, &outtmp, 0L, '\0', '\n', false);
    if (!res) {
        dsFree(&outtmp);
        return userraise(false, ERR_UNABLE_PARSE_DATA, "Unable to parse line");
    }
    dsReleaseFs(dst, &outtmp);   

    return true;
}

// till any delimeter
bool                       
dsParseWord(DS *restrict in, fs *restrict dst, bool use_buffer) {
    if (in == NULL || dst == NULL || !fs_alloc(dst))
        return userraiseint(ERR_NULL_INPUT, "%p %p/%s", in, dst, bool_str(fs_alloc(dst)) );
    
    DS      outtmp = dsPrepareout(dst, use_buffer, 0L);

    // \0 - non-espaced mode, \n - line terminator
    bool res = ds_parse_quoted_core(in, &outtmp, 0L, '\0', '\0', true);

    if (!res) {
        dsFree(&outtmp);
        return userraise(false, ERR_UNABLE_PARSE_DATA, "Unable to parse line");
    }
    dsReleaseFs(dst, &outtmp);   

    return true;
}

// -------------------------------------- fs adapters ------------------------------------------------
// ------------------------------- NOTE: no call to fs.c from here -----------------------------------

// write fs data into stream out
long                            
fs_dswrite(DS *restrict out, const fs *restrict s) {
    if (!out)
        return userraise(-1L, ERR_NULL_OUTPUT, "");
    if (!s) // that is normal behaviour, just log
        return logsimpleret(0L, "NUll fs");
    if (int_notin(out->type, DS_FILE, DS_STR, DS_FS) )
        return userraise(-1L, ERR_UNSUPPORTED_TYPE, 
            "Unsupported %d/%s", out->type, dsTypeName(out->type));
    return dsfsHelperFsDSWrite(out, s);
}

// techprint used temporary fs buffer (low performace) in order to have the same logic for all path
long                            
fs_dstechprint(DS *restrict out, const fs *restrict s, const char *restrict name) {
    if (!out)
        return userraise(-1L, ERR_NULL_OUTPUT, "");
    if (int_notin(out->type, DS_FILE, DS_STR, DS_FS) )
        return userraise(-1L, ERR_UNSUPPORTED_TYPE, 
            "Unsupported %d/%s", out->type, dsTypeName(out->type));

    fs           buf = FS();      // empty
    // common printer for all types! 
    // That is not very good for perf, but ok for techprint
    WRITE_OR_RET_ACTION(dsfsHelperTechprintTofs(&buf, 0L, s, name), -1, 
                        fsfree(buf));

    long  actual_written = dsfsHelperFsDSWrite(out, &buf);
    fsfree(buf);
    return actual_written;
}

long                            
fs_dsserialize(DS *restrict out, const fs *restrict s) {
    if (!out || !s)
        return userraise(-1L, ERR_NULL_OUTPUT, "%p %p", out, s);
    long    total = 0L;

    total += WRITE_OR_RET(dsPrintf(out, "FS(\"%zu\"): \"", s->len), -1L);

    for (size_t i = 0; i < s->len; i++)
        total += WRITE_OR_RET(dsputcEscape((unsigned char) s->v[i], out), -1L);

    total += WRITE_OR_RET(dsPrintf(out, "\"\n"), -1L);

    return total;
}

long                           
fs_dsload(DS *restrict in, fs *restrict dst, bool use_buffer) {
    if (!in || !dst)
        return userraise(-1L, ERR_NULL_INPUT, 
            "Input DS or fs is null %p %p", in, dst);
    
    size_t pos = dsSavepos(in);

    if (!dsExpect(in, "FS(\""))       // not shift position if failed
        return userraise(-1L, ERR_WRONG_INPUT_FORMAT, "Expected 'FS('");

    unsigned long expected_len = 0;
    if (!dsParseUnsignedLong(in, &expected_len)) {
        dsRestorepos(in, pos);
        return userraise(-1L, ERR_UNABLE_PARSE_DATA, "Failed to read length");
    }

    if (!dsExpect(in, "\"): ")) {
        dsRestorepos(in, pos);
        return userraise(-1L, ERR_WRONG_INPUT_FORMAT, "Expected '): \"'");
    }

    if (use_buffer) {
        fs buf = FS();

        if (!dsParseQuotedLimfsDirect(in, &buf, expected_len + 1) ) {
            dsRestorepos(in, pos);
            fsfree(buf);
            return userraise(-1L, ERR_WRONG_INPUT_FORMAT,
                            "Wrong quoted line");
        }
        if (buf.len != expected_len) {
            dsRestorepos(in, pos);
            fsfree(buf);
            return userraise(-1L, ERR_WRONG_INPUT_FORMAT,
                            "Wrong quoted line Length mismatch: header %lu, actual %zu",
                            expected_len, buf.len);
        }
        fs_cat(dst, buf);   // at least "" here

        fsfree(buf);
    } else {
        fs_setlen(dst, 0);

        if (!dsParseQuotedLimfsDirect(in, dst, expected_len + 1) ) {
            dsRestorepos(in, pos);
            return userraise(-1L, ERR_WRONG_INPUT_FORMAT,
                            "Wrong quoted line");
        }
        if (dst->len != expected_len) {
            dsRestorepos(in, pos);
            return userraise(-1L, ERR_WRONG_INPUT_FORMAT,
                            "Wrong quoted line Length mismatch: header %lu, actual %zu",
                            expected_len, dst->len);
        }
    }
    dsSkipNl(in);

    return dst->len;
}

// -------------------- CONSTRUCTOTS/DESTRUCTORS -------------------

// N/A

// -------------------------------Testing --------------------------

#ifdef DS_ADAPTER_TESTING

#include "test.h"

// ------------------------- TEST dsPrintf -------------------------
static TestStatus
tf_ds_printf(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_FILE: простая запись */
    test_sub("subtest %d: dsPrintf to file", ++subnum);
    {
        const char *fname = "res/ds/test_printf_file.dsadp";
        FILE *fp = fopen(fname, "w");
        DS ds = dsCreatef(fp);

        int written = dsPrintf(&ds, "Hello %d", 42);
        //fclose(fp);
        dsFree(&ds);
        test_validate(written > 0, "dsPrintf must return > 0, got %d", written);

        fp = fopen(fname, "r");
        char buf[32] = {0};
        fread(buf, 1, sizeof(buf)-1, fp);
        fclose(fp);
        test_validate(strcmp(buf, "Hello 42") == 0,
                      "File must contain 'Hello 42', got '%s'", buf);
    }

    /* 2. DS_FILE: запись с форматированием */
    test_sub("subtest %d: dsPrintf to file with multiple args", ++subnum);
    {
        const char *fname = "res/ds/test_printf_multi.dsadp";
        FILE *fp = fopen(fname, "w");
        DS ds = dsCreatef(fp);

        int written = dsPrintf(&ds, "%d + %d = %d", 2, 3, 5);
        //fclose(fp);
        dsFree(&ds);

        test_validate(written > 0, "dsPrintf must return > 0");

        fp = fopen(fname, "r");
        char buf[32] = {0};
        fread(buf, 1, sizeof(buf)-1, fp);
        fclose(fp);
        
        test_validate(strcmp(buf, "2 + 3 = 5") == 0,
                      "File must contain '2 + 3 = 5', got '%s'", buf);
    }

#ifndef NO_FSDS
    /* 3. DS_FS: запись в fs */
    test_sub("subtest %d: dsPrintf to FS", ++subnum);
    {
        fs s = FS();
        DS ds = dsCreatefs(&s);
        int written = dsPrintf(&ds, "Value=%d", 99);
        test_validate(written > 0, "dsPrintf must return > 0");

        // после dsCreatefs переменная s перемещена, читаем из ds.s
        test_validate(strcmp(ds.s.v, "Value=99") == 0,
                      "FS must contain 'Value=99', got '%s'", ds.s.v);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 4. DS_FS: множественная запись */
    test_sub("subtest %d: dsPrintf to FS multiple calls", ++subnum);
    {
        fs s = FS();
        DS ds = dsCreatefs(&s);
        dsPrintf(&ds, "Line1\n");
        dsPrintf(&ds, "Line2");
        test_validate(strcmp(ds.s.v, "Line1\nLine2") == 0,
                      "FS must contain 'Line1\\nLine2', got '%s'", ds.s.v);
        dsFree(&ds);
        fs_alloc_check(true);
    }
#endif /* !NO_FSDS */

    /* 5. NULL DS должен вызвать исключение */
    test_sub("subtest %d: dsPrintf with NULL DS raises SIGINT", ++subnum);
    {
        if (!try()) {
            dsPrintf(NULL, "test");
            test_validate(false, "Should have raised SIGINT for NULL DS");
        } else {
            logsimple("Exception correctly raised for NULL DS");
        }
    }
    /* 4. DS_STR: запись ровно на границе буфера (без переполнения) */
    test_sub("subtest %d: dsPrintf string boundary (no overflow)", ++subnum);
    {
        char buf[] = "..........";   // strlen = 10 → cap = 10 (достаточно для "123456789" + '\0')
        DS ds = dsCreatestr(buf);

        int written = dsPrintf(&ds, "123456789");   // нужно 9 символов + '\0' → 10
        test_validate(written == 9,
                    "dsPrintf must return 9, got %d", written);
        test_validate(strcmp(buf, "123456789") == 0,
                    "Buffer must contain '123456789', got '%s'", buf);
        test_validate(ds.pos == 9,
                    "pos must be 9, got %zu", ds.pos);
    }

    /* 5. DS_STR: переполнение буфера */
    test_sub("subtest %d: dsPrintf string overflow", ++subnum);
    {
        char buf[5] = "12";               // strlen = 2 → cap = 2
        DS ds = dsCreatestr(buf);
        int written = dsPrintf(&ds, "Hello World");   // нужно 11 символов
        test_validate(written == -1,
                    "dsPrintf overflow must return -1, got %d", written);
        test_validate(buf[0] == '1',
                    "Buffer must remain unchanged, got '%s'", buf);
        test_validate(buf[1] == '2',
                    "Buffer must remain unchanged, got '%s'", buf);
        test_validate(buf[2] == '\0',
                    "Buffer must remain unchanged, got '%s'", buf);
        test_validate(ds.pos == 0,
                    "pos must stay 0, got %zu", ds.pos);
    }
    /* 6. DS_STR: множественная запись */
    test_sub("subtest %d: dsPrintf to string multiple calls", ++subnum);
    {
        const int cnt = 50;
        char buf[cnt];
        DS ds = dsCreatestrCap(buf, cnt);
        dsPrintf(&ds, "Line1\n");
        dsPrintf(&ds, "Line2");

        test_validate(strcmp(buf, "Line1\nLine2") == 0,
                      "Buffer must contain 'Line1\\nLine2', got '%s'", buf);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsScanf -------------------------
static TestStatus
tf_ds_scanf(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_FILE: чтение из файла */
    test_sub("subtest %d: dsScanf from file", ++subnum);
    {
        const char *fname = "res/ds/test_scanf_file.dsadp";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "42 3.14 hello");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds = dsCreatef(fp);
        int i;
        double d;
        char s[10];
        int ret = dsScanf(&ds, "%d %lf %s", &i, &d, s);
        //fclose(fp);
        dsFree(&ds);
        test_validate(ret == 3, "dsScanf must return 3, got %d", ret);
        test_validate(i == 42 && d == 3.14 && strcmp(s, "hello") == 0,
                      "File values: i=%d, d=%.2f, s='%s' (expected 42, 3.14, 'hello')",
                      i, d, s);
    }

    /* 2. DS_STR: чтение из строки с авто‑сдвигом */
    test_sub("subtest %d: dsScanf from mutable string", ++subnum);
    {
        char buf[32] = "10 20 30";
        DS ds = dsCreatestr(buf);
        int a, b, c;
        dsScanf(&ds, "%d", &a);
        dsScanf(&ds, "%d", &b);
        dsScanf(&ds, "%d", &c);
        test_validate(a == 10 && b == 20 && c == 30,
                      "STR values: a=%d, b=%d, c=%d (expected 10,20,30)", a, b, c);
        test_validate(ds.pos == 8, "After three scans pos must be 8, got %zu", ds.pos);
    }

    /* 3. DS_CONSTSTR: чтение из константной строки */
    test_sub("subtest %d: dsScanf from const string", ++subnum);
    {
        const char *text = "3.14";
        DS ds = dsCreateconst(text);
        double d;
        int ret = dsScanf(&ds, "%lf", &d);
        test_validate(ret == 1, "dsScanf must return 1, got %d", ret);
        test_validate(d == 3.14, "CONSTSTR: d=%.2f (expected 3.14)", d);
        test_validate(ds.pos == 4, "pos must be 4, got %zu", ds.pos);
    }

#ifndef NO_FSDS
    /* 4. DS_FS: чтение из fs */
    test_sub("subtest %d: dsScanf from FS", ++subnum);
    {
        fs s = FS();
        DS ds = dsCreatefs(&s);
        dsputc('A', &ds);
        dsputc('B', &ds);
        dsputc(EOF, &ds);
        ds.pos = 0;
        char ch1, ch2;
        int ret = dsScanf(&ds, "%c%c", &ch1, &ch2);
        test_validate(ret == 2, "dsScanf must return 2, got %d", ret);
        test_validate(ch1 == 'A' && ch2 == 'B',
                      "FS chars: ch1='%c', ch2='%c' (expected 'A','B')", ch1, ch2);
        dsFree(&ds);
        fs_alloc_check(true);
    }
#endif /* !NO_FSDS */

    /* 5. Ошибка: пустая строка */
    test_sub("subtest %d: dsScanf on empty string", ++subnum);
    {
        char buf[4] = "";
        DS ds = dsCreatestr(buf);
        int val;
        int ret = dsScanf(&ds, "%d", &val);
        test_validate(ret <= 0, "dsScanf on empty string must fail, got %d", ret);
    }

    /* 6. Ошибка: неверный формат */
    test_sub("subtest %d: dsScanf with invalid format", ++subnum);
    {
        char buf[8] = "abc";
        DS ds = dsCreatestr(buf);
        int val;
        int ret = dsScanf(&ds, "%d", &val);
        test_validate(ret <= 0, "dsScanf on non‑numeric string must fail, got %d", ret);
    }

    test_sub("subtest %d: dsScanf with NULL DS raises SIGINT", ++subnum);
    {
        if (!try()) {
            int dummy;
            dsScanf(NULL, "%d", &dummy);
            test_validate(false, "Should have raised SIGINT for NULL DS");
        } else {
            logsimple("Exception correctly raised for NULL DS");
        }
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsScanf() and dsPrintf() combined tests -------------------------
static TestStatus
tf_ds_scanf_printf(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: dsPrintf + dsScanf round-trip (position)", ++subnum);
    {
        char buf[128];
        DS ds = dsCreatestrCap(buf, sizeof(buf));   // cap = 128

        // Записываем несколько значений
        size_t written_pos = dsPrintf(&ds, "%d %s %c", 42, "test", 'X');

        // Сбрасываем позицию для чтения
        dsReset(&ds);

        // Читаем обратно
        int i;
        char s[10];
        char c;
        int ret = dsScanf(&ds, "%d %s %c", &i, s, &c);
        size_t read_pos = ds.pos;       // позиция после чтения

        test_validate(ret == 3, "Scanf must read 3 items, got %d", ret);
        test_validate(i == 42 && strcmp(s, "test") == 0 && c == 'X',
                    "Values: i=%d, s='%s', c='%c' (expected 42, 'test', 'X')",
                    i, s, c);
        test_validate(written_pos == read_pos,
                    "Position after read (%zu) must equal position after write (%zu)",
                    read_pos, written_pos);
    }

    test_sub("subtest %d: multiple printf / scanf round‑trip", ++subnum);
    {
        char buf[256];
        DS ds = dsCreatestrCap(buf, sizeof(buf));

        // ---------- запись ----------
        int pt1 = dsPrintf(&ds, "%d ", 10);      // "10 "
        int pt2 = dsPrintf(&ds, "%s ", "hello"); // "10 hello "
        int pt3 = dsPrintf(&ds, "%c", '!');      // "10 hello !"
        int pt4 = dsPrintf(&ds, "%s ", " ?");
        //int pt4 = dsPrintf(&ds, "%c", '?');
        int total_written = pt1 + pt2 + pt3 + pt4;                 // позиция после третьей записи
        DSTECHPRINT(ds);

        // ---------- чтение ----------
        dsReset(&ds);

        int i;
        char s[10];
        char c, c1, c2, c3;

        int r1 = dsScanf(&ds, "%d", &i);         // "10"
        test_validate(
            r1 == 1,
            "r1 must be 1, got '%d'", r1
        );

        int r2 = dsScanf(&ds, "%s", s);          // "hello"
        test_validate(
            r2 == 1,
            "r2 must be 1, got '%d'", r2
        );

        int r3 = dsScanf(&ds, "%c%c", &c1, &c);         // "!"
        test_validate(
            r3 == 2,
            "r3 must be 2, got '%d'", r3
        );

        int r4 = dsScanf(&ds, " %c%c", &c2, &c3);         // "? "
        test_validate(
            r4 == 2,
            "r3 must be 1, got '%d'", r4
        );

        size_t total_read = ds.pos;

        test_validate(i == 10, "First scanf: i=%d (expected 10)", i);
        test_validate(strcmp(s, "hello") == 0, "Second scanf: s='%s' (expected 'hello')", s);
        test_validate(c == '!', "Third scanf: c='%c' (expected '!')", c);
        test_validate(c2 == '?', "Forth scanf: c2='%c' (expected '?')", c2);
        test_validate(c3 == ' ', "Forth scanf: c3='%c' (expected ' ')", c3);
        test_validate( (int) total_read == total_written,
                    "Total positions must match: written=%d, read=%zu",
                    total_written, total_read);
    }

    return logret(TEST_PASSED, "done");
}

// --------------------------- TEST dsParse*  ---------------------------------
static TestStatus
tf_ds_parsers(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== dsParseInt ========== */
    test_sub("subtest %d: dsParseInt from mutable string", ++subnum);
    {
        char buf[64] = "42 123";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        int val;
        test_validate(dsParseInt(&ds, &val) && val == 42,
                      "dsParseInt: expected 42, got %d", val);
        test_validate(ds.pos == 2, "pos must be 2, got %zu", ds.pos);
    }

    test_sub("subtest %d: dsParseInt from const string", ++subnum);
    {
        const char *text = "-10 999";
        DS ds = dsCreateconst(text);
        int val;
        test_validate(dsParseInt(&ds, &val) && val == -10,
                      "dsParseInt const: expected -10, got %d", val);
        test_validate(ds.pos == 3, "pos must be 3, got %zu", ds.pos);
    }

    test_sub("subtest %d: dsParseInt from file", ++subnum);
    {
        const char *fname = "res/ds/test_parse_int.dsadp";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "77");
        fclose(fp);
        fp = fopen(fname, "r");
        DS ds = dsCreatef(fp);
        int val;
        test_validatefree(dsParseInt(&ds, &val) && val == 77,
                          fclose(fp),
                          "dsParseInt file: expected 77, got %d", val);
        //fclose(fp);
        dsFree(&ds);
    }

    test_sub("subtest %d: dsParseInt fails on non‑numeric", ++subnum);
    {
        char buf[32] = "abc";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        int val;
        test_validate(!dsParseInt(&ds, &val),
                      "dsParseInt on 'abc' must fail, got val=%d", val);
        test_validate(ds.pos == 0, "pos must stay 0, got %zu", ds.pos);
    }

    test_sub("subtest %d: dsParseInt NULL DS raises", ++subnum);
    {
        if (!try()) {
            int v;
            dsParseInt(NULL, &v);
            test_validate(false, "Should have raised SIGINT");
        } else {
            logsimple("Exception correctly raised");
        }
    }

    /* ========== dsParseLong ========== */
    test_sub("subtest %d: dsParseLong basic", ++subnum);
    {
        char buf[64] = "123456789012";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        long val;
        test_validate(dsParseLong(&ds, &val) && val == 123456789012L,
                      "dsParseLong: expected 123456789012, got %ld", val);
    }

    test_sub("subtest %d: dsParseLong fails on empty", ++subnum);
    {
        char buf[4] = "";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        long val;
        test_validate(!dsParseLong(&ds, &val),
                      "dsParseLong on empty must fail");
    }

    /* ========== dsParseUnsigned ========== */
    test_sub("subtest %d: dsParseUnsigned basic", ++subnum);
    {
        char buf[64] = "3000000000";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        unsigned int val;
        test_validate(dsParseUnsigned(&ds, &val) && val == 3000000000u,
                      "dsParseUnsigned: expected 3000000000u, got %u", val);
    }

    /* ========== dsParseUnsignedLong ========== */
    test_sub("subtest %d: dsParseUnsignedLong basic", ++subnum);
    {
        char buf[64] = "18446744073709551615";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        unsigned long val;
        test_validate(dsParseUnsignedLong(&ds, &val) && val == 18446744073709551615UL,
                      "dsParseUnsignedLong: expected max, got %lu", val);
    }

    /* ========== dsParseDouble ========== */
    test_sub("subtest %d: dsParseDouble basic", ++subnum);
    {
        char buf[64] = "3.1415 -2.5e1";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        double val;
        test_validate(dsParseDouble(&ds, &val) && val == 3.1415,
                      "dsParseDouble: expected 3.1415, got %lf", val);
        test_validate(dsParseDouble(&ds, &val) && val == -25.0,
                      "dsParseDouble second: expected -25.0, got %lf", val);
    }

    test_sub("subtest %d: dsParseDouble fails on text", ++subnum);
    {
        char buf[16] = "hello";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        double val;
        test_validate(!dsParseDouble(&ds, &val),
                      "dsParseDouble on text must fail");
    }

    /* ========== dsParseChar ========== */
    test_sub("subtest %d: dsParseChar basic", ++subnum);
    {
        char buf[64] = "A B";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        char c;
        test_validate(dsParseChar(&ds, &c) && c == 'A',
                      "dsParseChar: expected 'A', got '%c'", c);
        test_validate(ds.pos == 1, "pos must be 1, got %zu", ds.pos);
        test_validate(dsParseChar(&ds, &c) && c == 'B',
                      "dsParseChar second: expected 'B', got '%c'", c);
        test_validate(ds.pos == 3, "pos must be 3, got %zu", ds.pos);
    }

    test_sub("subtest %d: dsParseChar fails on empty", ++subnum);
    {
        char buf[4] = "";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        char c;
        test_validate(!dsParseChar(&ds, &c),
                      "dsParseChar on empty must fail");
    }

    /* ========== DS_FS (если доступно) ========== */
#ifndef NO_FSDS
    test_sub("subtest %d: dsParseInt from FS", ++subnum);
    {
        fs s = FS();
        DS ds = dsCreatefs(&s);
        dsPrintf(&ds, "%d", 202);
        dsReset(&ds);
        int val;
        test_validatefree(dsParseInt(&ds, &val) && val == 202,
                          dsFree(&ds),
                          "dsParseInt FS: expected 202, got %d", val);
        dsFree(&ds);
        fs_alloc_check(true);
    }
#endif

    /* ========== Переполнение буфера при записи + чтение ========== */
    test_sub("subtest %d: dsParseInt after overflow", ++subnum);
    {
        char buf[8] = "";   // cap = 8
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        dsPrintf(&ds, "12345678");   // попытка записи не влезет
        dsReset(&ds);
        int val;
        test_validate(!dsParseInt(&ds, &val),
                      "dsParseInt on overflowed buffer must fail");
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_dstechprint -------------------------
static TestStatus
tf5_fs_dstechprint(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_FILE output */
    test_sub("subtest %d: DS_FILE output", ++subnum);
    {
        DS      ds = dsCreateFilename("res/ds_adapter/dstechprintf_file.ds", "w+");
        fs      sample = fscopy("hello");

        long    written = fs_dstechprint(&ds, &sample, "sample_fs");
        test_validatefree(written > 0, (fsfree(sample), dsFree(&ds)),
                          "expected positive written count");

        // перематываем и читаем файл
        rewind(ds.fp);
        char    buf[256];
        size_t  n = fread(buf, 1, sizeof(buf)-1, ds.fp);
        buf[n] = '\0';

        test_validatefree(strstr(buf, "sample_fs") != NULL,
                          (fsfree(sample), dsFree(&ds)),
                          "file content must contain name");
        test_validatefree(strstr(buf, "hello") != NULL,
                          (fsfree(sample), dsFree(&ds)),
                          "file content must contain fs data");

        fsfree(sample);
        //fclose(fp);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 2. DS_STR output */
    test_sub("subtest %d: DS_STR output", ++subnum);
    {
        char buffer[256];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("test");

        long written = fs_dstechprint(&ds, &sample, "str_fs");
        test_validatefree(written > 0, (fsfree(sample)), "expected positive count");

        buffer[ds.pos] = '\0';   // добавляем терминатор
        test_validatefree(strstr(buffer, "str_fs") != NULL,
                          (fsfree(sample)),
                          "DS_STR content must contain name");
        test_validatefree(strstr(buffer, "test") != NULL,
                          (fsfree(sample)),
                          "DS_STR content must contain fs data");
        test_validatefree(ds.pos == (size_t)written,
                          (fsfree(sample)),
                          "DS_STR pos must equal written");

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 3. DS_FS output */
    test_sub("subtest %d: DS_FS output", ++subnum);
    {
        fs out = FS();
        DS ds = dsCreatefs(&out);   // out перемещается в DS, не освобождаем отдельно
        fs sample = fscopy("world");

        long written = fs_dstechprint(&ds, &sample, "fs_fs");
        test_validatefree(written > 0, (fsfree(sample), dsFree(&ds)),
                          "expected positive count");

        // проверяем содержимое DS_FS (там fs в ds.s)
        test_validatefree(strstr(fs_str(&ds.s), "fs_fs") != NULL,
                          (fsfree(sample), dsFree(&ds)),
                          "DS_FS content must contain name");
        test_validatefree(strstr(fs_str(&ds.s), "world") != NULL,
                          (fsfree(sample), dsFree(&ds)),
                          "DS_FS content must contain fs data");
        test_validatefree(ds.pos == (size_t)written,
                          (fsfree(sample), dsFree(&ds)),
                          "DS_FS pos must equal written");

        fsfree(sample);
        dsFree(&ds);   // освобождает fs внутри DS
        fs_alloc_check(true);
    }

    /* 4. Ошибка при неподдерживаемом типе DS */
    test_sub("subtest %d: unsupported DS type", ++subnum);
    {
        DS ds = {0};      // type = DS_FILE по нумерации, но мы сделаем заведомо неверный
        ds.type = (DSType)999;

        fs sample = FS();
        if (!try()) {
            fs_dstechprint(&ds, &sample, "bad");
            test_validatefree(false, (fsfree(sample)), "must raise error");
        } else {
            test_validatefree(true, (fsfree(sample)), "correctly raised error");
        }

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 5. NULL выходной параметр */
    test_sub("subtest %d: NULL output", ++subnum);
    {
        fs sample = FS();

        if (!try()) {
            fs_dstechprint(NULL, &sample, "null");
            test_validatefree(false, (fsfree(sample)), "must raise error");
        } else {
            test_validatefree(true, (fsfree(sample)), "correctly raised error");
        }

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 6. Пустая строка с выделенной памятью (fscopy("")) */
    test_sub("subtest %d: empty fs with allocated buffer", ++subnum);
    {
        char buffer[256];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("");

        long written = fs_dstechprint(&ds, &sample, "empty_fs");
        test_validatefree(written > 0, (fsfree(sample)), "expected positive count");

        buffer[ds.pos] = '\0';
        test_validatefree(strstr(buffer, "empty_fs") != NULL,
                          (fsfree(sample)),
                          "buffer must contain name");
        test_validatefree(strstr(buffer, "len [0]") != NULL,
                          (fsfree(sample)),
                          "buffer must indicate len 0");

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 7. Пустой fs без памяти (FS()) */
    test_sub("subtest %d: empty fs without memory (FS())", ++subnum);
    {
        char buffer[256];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = FS();

        long written = fs_dstechprint(&ds, &sample, "null_fs");
        test_validatefree(written > 0, (fsfree(sample)), "expected positive count");

        buffer[ds.pos] = '\0';
        test_validatefree(strstr(buffer, "null_fs") != NULL,
                          (fsfree(sample)),
                          "buffer must contain name");
        test_validatefree(strstr(buffer, "<NULL>") != NULL,
                          (fsfree(sample)),
                          "buffer must indicate NULL string");

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 8. NULL fs (s == NULL) */
    test_sub("subtest %d: NULL fs (s == NULL)", ++subnum);
    {
        char buffer[256];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));

        long written = fs_dstechprint(&ds, NULL, "null_ptr");
        test_validate(written > 0, "expected positive count");

        buffer[ds.pos] = '\0';
        test_validate(strstr(buffer, "null_ptr") != NULL,
                      "buffer must contain name");
        test_validate(strstr(buffer, "<NULL>") != NULL,
                      "buffer must indicate NULL");
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_dswrite -------------------------
static TestStatus
tf6_fs_dswrite(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Вывод в DS_FILE */
    test_sub("subtest %d: DS_FILE output", ++subnum);
    {
        const char *fname = "res/ds_adapter/dsprintf_file.ds";
        DS ds = dsCreateFilename(fname, "w+");
        test_validatefree(ds.fp != NULL, (dsFree(&ds)), "can't open file");

        fs sample = fscopy("hello");
        long written = fs_dswrite(&ds, &sample);
        test_validatefree(written == 5, (dsFree(&ds), fsfree(sample)),
                          "expected 5, got %ld", written);

        rewind(ds.fp);
        char buf[16];
        size_t n = fread(buf, 1, sizeof(buf)-1, ds.fp);
        buf[n] = '\0';
        test_validatefree(strcmp(buf, "hello") == 0,
                          (dsFree(&ds), fsfree(sample)),
                          "file content mismatch: '%s'", buf);

        dsFree(&ds);
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 2. Вывод в DS_STR с достаточной ёмкостью */
    test_sub("subtest %d: DS_STR output, enough capacity", ++subnum);
    {
        char buffer[256];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("world");

        long written = fs_dswrite(&ds, &sample);
        test_validatefree(written == 5, (fsfree(sample)), "expected 5, got %ld", written);

        buffer[ds.pos] = '\0';
        test_validatefree(strcmp(buffer, "world") == 0,
                          (fsfree(sample)),
                          "buffer mismatch: '%s'", buffer);
        test_validatefree(ds.pos == 5, (fsfree(sample)),
                          "pos expected 5, got %zu", ds.pos);

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 3. Вывод в DS_STR с ограниченной ёмкостью (truncate) */
    test_sub("subtest %d: DS_STR output, limited capacity", ++subnum);
    {
        char buffer[4];   // реально поместится 3 символа + '\0'
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("hello");

        long written = fs_dswrite(&ds, &sample);
        test_validatefree(written == 3, (fsfree(sample)),
                          "expected truncated 3, got %ld", written);

        buffer[ds.pos] = '\0';
        test_validatefree(strcmp(buffer, "hel") == 0,
                          (fsfree(sample)),
                          "buffer mismatch: '%s'", buffer);
        test_validatefree(ds.pos == 3, (fsfree(sample)),
                          "pos expected 3, got %zu", ds.pos);

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 4. Вывод в DS_FS */
    test_sub("subtest %d: DS_FS output", ++subnum);
    {
        fs out = FS();
        DS ds = dsCreatefs(&out);   // владение out переходит в ds
        fs sample = fscopy("fsdata");

        long written = fs_dswrite(&ds, &sample);
        test_validatefree(written == 6, (dsFree(&ds), fsfree(sample)),
                          "expected 6, got %ld", written);

        test_validatefree(strcmp(fs_str(&ds.s), "fsdata") == 0,
                          (dsFree(&ds), fsfree(sample)),
                          "DS_FS content mismatch");

        dsFree(&ds);
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 5. Пустая строка с выделенной памятью */
    test_sub("subtest %d: empty fs with allocated buffer", ++subnum);
    {
        char buffer[16];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("");

        long written = fs_dswrite(&ds, &sample);
        test_validatefree(written == 0, (fsfree(sample)), "expected 0, got %ld", written);

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 6. Пустой fs без памяти (FS()) */
    test_sub("subtest %d: empty fs without memory (FS())", ++subnum);
    {
        char buffer[16];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = FS();

        long written = fs_dswrite(&ds, &sample);
        test_validatefree(written == 0, (fsfree(sample)), "expected 0, got %ld", written);

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 7. NULL fs (s == NULL) */
    test_sub("subtest %d: NULL fs (s == NULL)", ++subnum);
    {
        char buffer[16];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));

        long written = fs_dswrite(&ds, NULL);
        test_validate(written == 0, "expected 0, got %ld", written);

        fs_alloc_check(true);
    }

    /* 8. Ошибка при неподдерживаемом типе DS */
    test_sub("subtest %d: unsupported DS type", ++subnum);
    {
        DS ds = {0};
        ds.type = (DSType)999;

        fs sample = fscopy("x");
        if (!try()) {
            fs_dswrite(&ds, &sample);
            test_validatefree(false, (fsfree(sample)), "must raise error");
        } else {
            test_validatefree(true, (fsfree(sample)), "correctly raised error");
        }

        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 9. NULL выходной параметр */
    test_sub("subtest %d: NULL output", ++subnum);
    {
        fs sample = fscopy("x");

        if (!try()) {
            fs_dswrite(NULL, &sample);
            test_validatefree(false, (fsfree(sample)), "must raise error");
        } else {
            test_validatefree(true, (fsfree(sample)), "correctly raised error");
        }

        fsfree(sample);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_dsserialize (full, with edges) -------------------------
static TestStatus
tf7_fs_dsserialize_full(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Обычная строка в DS_STR */
    test_sub("subtest %d: simple string to DS_STR", ++subnum);
    {
        char buffer[256];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("hello");

        long written = fs_dsserialize(&ds, &sample);
        test_validatefree(written > 0, (dsFree(&ds), fsfree(sample)),
                          "expected positive bytes written");

        buffer[ds.pos] = '\0';
        test_validatefree(strcmp(buffer, "FS(\"5\"): \"hello\"\n") == 0,
                          (dsFree(&ds), fsfree(sample)),
                          "serialized mismatch: '%s'", buffer);

        dsFree(&ds);
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 2. Пустая строка в DS_STR */
    test_sub("subtest %d: empty string to DS_STR", ++subnum);
    {
        char buffer[256];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("");

        long written = fs_dsserialize(&ds, &sample);
        test_validatefree(written > 0, (dsFree(&ds), fsfree(sample)),
                          "expected positive bytes written");

        buffer[ds.pos] = '\0';
        test_validatefree(strcmp(buffer, "FS(\"0\"): \"\"\n") == 0,
                          (dsFree(&ds), fsfree(sample)),
                          "serialized mismatch: '%s'", buffer);

        dsFree(&ds);
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 3. Строка со всеми экранируемыми символами в DS_STR */
    test_sub("subtest %d: string with escapes to DS_STR", ++subnum);
    {
        char buffer[512];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("a\"b\\c\nd\re\tf");

        long written = fs_dsserialize(&ds, &sample);
        test_validatefree(written > 0, (dsFree(&ds), fsfree(sample)),
                          "expected positive bytes written");

        buffer[ds.pos] = '\0';
        // Ожидаемая строка: FS "9" "a\"b\\c\nd\re\tf"
        const char *expected = "FS(\"11\"): \"a\\\"b\\\\c\\nd\\re\\tf\"\n";
        test_validatefree(strcmp(buffer, expected) == 0,
                          (dsFree(&ds), fsfree(sample)),
                          "serialized mismatch:\n got: '%s'\nwant: '%s'",
                          buffer, expected);

        dsFree(&ds);
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 4. Вывод в DS_FILE */
    test_sub("subtest %d: DS_FILE output", ++subnum);
    {
        const char *fname = "res/ds_adapter/dsserialize_file_full.ds";
        DS ds = dsCreateFilename(fname, "w+");
        test_validatefree(ds.fp != NULL, (dsFree(&ds)), "can't open file");

        fs sample = fscopy("file_test");
        long written = fs_dsserialize(&ds, &sample);
        test_validatefree(written > 0, (dsFree(&ds), fsfree(sample)),
                          "expected positive bytes written");

        rewind(ds.fp);
        char buf[128];
        size_t n = fread(buf, 1, sizeof(buf)-1, ds.fp);
        buf[n] = '\0';
        test_validatefree(strcmp(buf, "FS(\"9\"): \"file_test\"\n") == 0,
                          (dsFree(&ds), fsfree(sample)),
                          "file content mismatch: '%s'", buf);

        dsFree(&ds);
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 5. Вывод в DS_FS */
    test_sub("subtest %d: DS_FS output", ++subnum);
    {
        fs out = FS();
        DS ds = dsCreatefs(&out);   // владение out переходит в ds
        fs sample = fscopy("fsdata");

        long written = fs_dsserialize(&ds, &sample);
        test_validatefree(written > 0, (dsFree(&ds), fsfree(sample)),
                          "expected positive bytes written");

        test_validatefree(strcmp(fs_str(&ds.s), "FS(\"6\"): \"fsdata\"\n") == 0,
                          (dsFree(&ds), fsfree(sample)),
                          "fs content mismatch: '%s'", fs_str(&ds.s));

        dsFree(&ds);
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 6. DS_STR с ограниченной ёмкостью (truncate) */
    test_sub("subtest %d: DS_STR limited capacity truncates", ++subnum);
    {
        char buffer[12];   // реально поместится 9 символов + '\0'
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        fs sample = fscopy("hello world");

        long written = fs_dsserialize(&ds, &sample);
        test_validatefree(written == EOF, //> 0 && written < (long)(sample.len + 10),
                          (dsFree(&ds), fsfree(sample)),
                          "expected truncated write with EOF, got %ld bytes", written);

        buffer[sizeof(buffer) - 1] = '\0';

        // Проверяем, что это префикс ожидаемой строки и что нет выхода за границы
        test_validatefree(strncmp(buffer, "FS(\"11\"): \"", sizeof(buffer) - 1) == 0,
                          (dsFree(&ds), fsfree(sample)),
                          "truncated content does not start correctly: '%s'", buffer);

        dsFree(&ds);
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 7. NULL выходной параметр */
    test_sub("subtest %d: NULL output", ++subnum);
    {
        fs sample = fscopy("x");
        if (!try()) {
            fs_dsserialize(NULL, &sample);
            test_validatefree(false, (fsfree(sample)), "must raise error");
        } else {
            test_validatefree(true, (fsfree(sample)), "correctly raised error");
        }
        fsfree(sample);
        fs_alloc_check(true);
    }

    /* 8. NULL fs (s == NULL) */
    test_sub("subtest %d: NULL fs", ++subnum);
    {
        char buffer[64];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));

        if (!try()) {
            fs_dsserialize(&ds, NULL);
            test_validate(false, "must raise error");
        } else {
            test_validate(true, "correctly raised error");
        }

        dsFree(&ds);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseQuotedLimfsDirect -------------------------
static TestStatus
tf8_ds_parse_quoted_line(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простая строка */
    test_sub("subtest %d: parse simple quoted string", ++subnum);
    {
        const char *input = "\"hello\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 16);
        test_validatefree(
            res,
            fsfree(dst),
            "Invalid incoming  '%s'", input
        );
                         
        test_validatefree(
            dst.len == 5, 
            fsfree(dst),
            "expected 5, got %zu", dst.len
        );

        test_validatefree(fscmpstr(dst, "hello") == 0,
                          fsfree(dst),
                          "content mismatch: '%s'", fs_str(&dst));
        test_validatefree(ds.pos == 7, fsfree(dst),
                          "pos expected 7, got %zu", ds.pos);

        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Пустая строка */
    test_sub("subtest %d: parse empty quoted string", ++subnum);
    {
        const char *input = "\"\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 8);
        test_validatefree(
            res,
            fsfree(dst),
            "Invalid fs"
        );
        test_validatefree(
            dst.len == 0, 
            fsfree(dst),
            "expected 0, got %zu", dst.len
        );

        test_validatefree(fslen(dst) == 0 && *dst.v == '\0',
                          fsfree(dst),
                          "expected empty string %zu '%c'", fslen(dst), *dst.v);

        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Строка с escape-последовательностями */
    test_sub("subtest %d: parse escaped string", ++subnum);
    {
        const char *input = "\"a\\\"b\\\\c\\nd\\te\\rf\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();
        
        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 20);
        test_validatefree(
            res && dst.len == 11,
            (dsFree(&ds), fsfree(dst)),
            "expected 11, got %zu", dst.len
        );
        test_validatefree(
            fscmp(dst, FSLITERAL("a\"b\\c\nd\te\rf") ) == 0,
            (dsFree(&ds), fsfree(dst)),
            "content mismatch: got '%s'", fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Отсутствие открывающей кавычки */
    test_sub("subtest %d: missing opening quote restores pos", ++subnum);
    {
        const char *input = "hello\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();
        size_t saved = dsGetpos(&ds);

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 8);
        test_validatefree(
            !res,
            fsfree(dst),
            "Invalid fs"
        );
        test_validatefree(
            dst.len == 0, 
            fsfree(dst),
            "expected 0, got %zu", dst.len
        );
        test_validatefree(ds.pos == saved, fsfree(dst),
                        "pos must be restored to %zu, got %zu", saved, ds.pos);
        test_validatefree(fs_len(&dst) == 0, fsfree(dst),
                        "buffer must be empty after error");

        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Незакрытая строка */
    test_sub("subtest %d: unterminated string restores pos", ++subnum);
    {
        const char *input = "\"hello";
        DS ds = dsCreateconst(input);
        fs dst = fsinit(8);
        size_t saved = dsGetpos(&ds);

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 8);
        test_validatefree(
            !res,
            fsfree(dst),
            "MUST be Invalid fs"
        );
        test_validatefree(ds.pos == saved, fsfree(dst),
                        "pos must be restored to %zu, got %zu", saved, ds.pos);
        test_validatefree(fs_len(&dst) == 0, fsfree(dst),
                        "buffer must be empty after error");

        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Недопустимый escape */
    test_sub("subtest %d: invalid escape restores pos", ++subnum);
    {
        const char *input = "\"a\\x\"";
        DS ds = dsCreateconst(input);
        fs dst = fsinit(8);
        size_t saved = dsGetpos(&ds);

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 8);
        test_validatefree(
            !res,
            fsfree(dst),
            "MUST be Invalid fs"
        );
        test_validatefree(ds.pos == saved, fsfree(dst),
                        "pos must be restored to %zu, got %zu", saved, ds.pos);
        test_validatefree(fs_len(&dst) == 0, fsfree(dst),
                        "buffer must be empty after error");

        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Переполнение буфера (dst слишком мал) */
    test_sub("subtest %d: buffer overflow restores pos and raises error", ++subnum);
    {
        const char *input = "\"hello\"";
        DS ds = dsCreateconst(input);
        fs dst = fsinit(3);   // ёмкость 2 байта, нужно 5
        size_t saved = dsGetpos(&ds);

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 3);
        test_validatefree(
            !res,
            fsfree(dst),
            "MUST not be parsed (buf to small)"
        );
        test_validatefree(dst.len == 0, fsfree(dst),
                        "expected 0, got %zu", dst.len);
        test_validatefree(ds.pos == saved, fsfree(dst),
                        "pos must be restored to %zu, got %zu", saved, ds.pos);
        test_validatefree(fs_len(&dst) == 0, fsfree(dst),
                        "buffer must be empty after error");

        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 8. NULL входные параметры */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        const char *input = "\"x\"";
        DS ds = dsCreateconst(input);
        fs dst = fsinit(8);

        if (!try()) {
            dsParseQuotedLimfsDirect(NULL, &dst, 8);
            test_validatefree(false, fsfree(dst), "must raise error for NULL DS");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }

        if (!try()) {
            dsParseQuotedLimfsDirect(&ds, NULL, 8);
            test_validatefree(false, fsfree(dst), "must raise error for NULL fs");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }

        fsfree(dst);
        fs_alloc_check(true);
    }

        /* 8. Строка, состоящая только из экранированной кавычки */
    test_sub("subtest %d: string containing just an escaped quote", ++subnum);
    {
        const char *input = "\"\\\"\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 16);
        test_validatefree(
            res,
            fsfree(dst),
            "Invalid fs"
        );
        test_validatefree(dst.len == 1, fsfree(dst),
                          "expected 1, got %zu", dst.len);
        test_validatefree(fs_str(&dst)[0] == '"' && fs_str(&dst)[1] == '\0',
                          fsfree(dst),
                          "content must be just a quote");
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 9. Строка с пробелами внутри */
    test_sub("subtest %d: string with spaces", ++subnum);
    {
        const char *input = "\"  hello  \"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 16);
        test_validatefree(
            res,
            fsfree(dst),
            "Invalid fs"
        );
        test_validatefree(dst.len == 9, fsfree(dst),
                          "expected 9, got %zu", dst.len);
        test_validatefree(strcmp(fs_str(&dst), "  hello  ") == 0,
                          fsfree(dst),
                          "content mismatch: '%s'", fs_str(&dst));
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 10. Строка, заканчивающаяся экранированным слэшем */
    test_sub("subtest %d: string ending with escaped backslash", ++subnum);
    {
        const char *input = "\"abc\\\\\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 16);
        test_validatefree(
            res,
            fsfree(dst),
            "Invalid fs"
        );
        test_validatefree(dst.len == 4, fsfree(dst),
                          "expected 4, got %zu", dst.len);
        test_validatefree(strcmp(fs_str(&dst), "abc\\") == 0,
                          fsfree(dst),
                          "content mismatch: '%s'", fs_str(&dst));
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 11. Строка только с символом новой строки */
    test_sub("subtest %d: string with just newline escape", ++subnum);
    {
        const char *input = "\"\\n\"";
        DS ds = dsCreateconst(input);
        fs dst = fsinit(8);

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 8);
        test_validatefree(
            res,
            fsfree(dst),
            "Invalid fs"
        );
        test_validatefree(dst.len == 1, fsfree(dst),
                          "expected 1, got %zu", dst.len);
        test_validatefree(fs_str(&dst)[0] == '\n' && fs_str(&dst)[1] == '\0',
                          fsfree(dst),
                          "content must be newline");
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 12. Строка только со слэшем */
    test_sub("subtest %d: string with just a backslash", ++subnum);
    {
        const char *input = "\"\\\\\"";
        DS ds = dsCreateconst(input);
        fs dst = fsinit(8);

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 8);
        test_validatefree(
            res,
            fsfree(dst),
            "Invalid fs"
        );
        test_validatefree(dst.len == 1, fsfree(dst),
                          "expected 1, got %zu", dst.len);
        test_validatefree(fs_str(&dst)[0] == '\\' && fs_str(&dst)[1] == '\0',
                          fsfree(dst),
                          "content must be backslash");
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 13. Строка с несколькими escape подряд */
    test_sub("subtest %d: multiple escapes in sequence", ++subnum);
    {
        const char *input = "\"\\n\\t\\r\\\"\\\\\"";
        DS ds = dsCreateconst(input);
        fs dst = fsinit(32);

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 32);
        test_validatefree(
            res,
            fsfree(dst),
            "Invalid fs"
        );
        test_validatefree(dst.len == 5, fsfree(dst),
                          "expected 5, got %zu", dst.len);
        test_validatefree(fs_str(&dst)[0] == '\n' &&
                          fs_str(&dst)[1] == '\t' &&
                          fs_str(&dst)[2] == '\r' &&
                          fs_str(&dst)[3] == '"'  &&
                          fs_str(&dst)[4] == '\\' &&
                          fs_str(&dst)[5] == '\0',
                          fsfree(dst),
                          "escape sequence mismatch");
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 14. Некорректный escape в середине должен очистить буфер */
    test_sub("subtest %d: invalid escape clears buffer and restores pos", ++subnum);
    {
        const char *input = "\"ab\\xc\"";
        DS ds = dsCreateconst(input);
        fs dst = fsinit(16);
        size_t saved = dsGetpos(&ds);

        bool res = dsParseQuotedLimfsDirect(&ds, &dst, 16);
        test_validatefree(
            !res,
            fsfree(dst),
            "MUST be Invalid fs"
        );
        // test_validatefree(len == 0, fsfree(dst),
        //                   "expected 0, got %zu", len);
        test_validatefree(ds.pos == saved, fsfree(dst),
                          "pos must be restored to %zu, got %zu", saved, ds.pos);
        // test_validatefree(fs_len(&dst) == 0 && fs_str(&dst)[0] == '\0',
        //                   fsfree(dst),
        //                   "buffer must be empty after error");
        fsfree(dst);
        fs_alloc_check(true);
    }
    /* 20. use_buffer=false с maxlen=2 и одним символом (проверка небуферизованного режима) */
    test_sub("subtest %d: use_buffer=false, one char, maxlen=2", ++subnum);
    {
        const char *input = "\"x\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfs(&in, &dst, 2, false);
        test_validatefree(res && dst.len == 1,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 1");
        test_validatefree(fscmp(dst, FSLITERAL("x")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_dsserialize / fs_dsload DS_STR round-trip -------------------------
static TestStatus
tf9_fs_ds_DS_STR_roundtrip(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простая строка (use_buffer = true) */
    test_sub("subtest %d: roundtrip simple string (use_buffer=true)", ++subnum);
    {
        fs src = fscopy("hello");
        char buffer[128];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));
        long written = fs_dsserialize(&out_ds, &src);
        test_validatefree(written > 0, fsfree(src), "serialize failed");

        dsFree(&out_ds);

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, true);
        test_validatefree(read_len == (long)src.len, (fsfree(src), fsfree(dst)),
                          "read length mismatch: expected %zu, got %ld", src.len, read_len);
        test_validatefree(fscmp(dst, src) == 0,
                          (fsfree(src), fsfree(dst)),
                          "content mismatch: src='%s', dst='%s'", fsstr(src), fsstr(dst));
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Пустая строка (use_buffer=true) */
    test_sub("subtest %d: roundtrip empty string (use_buffer=true)", ++subnum);
    {
        fs src = fscopy("");
        char buffer[128];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));
        long written = fs_dsserialize(&out_ds, &src);
        test_validatefree(written > 0, fsfree(src), "serialize failed");

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, true);
        test_validatefree(read_len == 0 && fslen(dst) == 0,
                          (fsfree(src), fsfree(dst)),
                          "expected empty, got len=%ld, fs_len=%zu", read_len, fs_len(&dst));
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Строка со спецсимволами (use_buffer=true) */
    test_sub("subtest %d: roundtrip string with escapes (use_buffer=true)", ++subnum);
    {
        fs src = fscopy("a\"b\\c\nd\te\rf");
        char buffer[256];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));
        long written = fs_dsserialize(&out_ds, &src);
        test_validatefree(written > 0, fsfree(src), "serialize failed");

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, true);
        test_validatefree(read_len == (long) src.len, (fsfree(src), fsfree(dst)),
                          "read length mismatch: expected %zu, got %ld", src.len, read_len);
        test_validatefree(fscmp(dst, src) == 0,
                          (fsfree(src), fsfree(dst)),
                          "content mismatch:\n src='%s'\n dst='%s'", fsstr(src), fsstr(dst));
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Простая строка (use_buffer=false) */
    test_sub("subtest %d: roundtrip simple string (use_buffer=false)", ++subnum);
    {
        fs src = fscopy("hello");
        char buffer[128];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));
        long written = fs_dsserialize(&out_ds, &src);
        test_validatefree(written > 0, fsfree(src), "serialize failed");

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, false);
        test_validatefree(read_len == (long) src.len, (fsfree(src), fsfree(dst)),
                          "read length mismatch: expected %zu, got %ld", src.len, read_len);
        test_validatefree(fscmp(dst, src) == 0,
                          (fsfree(src), fsfree(dst)),
                          "content mismatch: src='%s', dst='%s'", fsstr(src), fsstr(dst));
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Пустая строка (use_buffer=false) */
    test_sub("subtest %d: roundtrip empty string (use_buffer=false)", ++subnum);
    {
        fs src = fscopy("");
        char buffer[128];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));
        long written = fs_dsserialize(&out_ds, &src);
        test_validatefree(written > 0, fsfree(src), "serialize failed");

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, false);
        test_validatefree(read_len == 0 && fslen(dst) == 0,
                          (fsfree(src), fsfree(dst)),
                          "expected empty, got len=%ld, fs_len=%zu", read_len, fslen(dst));
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Строка со спецсимволами (use_buffer=false) */
    test_sub("subtest %d: roundtrip string with escapes (use_buffer=false)", ++subnum);
    {
        fs src = fscopy("a\"b\\c\nd\te\rf");
        char buffer[256];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));
        long written = fs_dsserialize(&out_ds, &src);
        test_validatefree(written > 0, fsfree(src), "serialize failed");

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, false);
        test_validatefree(read_len == (long)src.len, (fsfree(src), fsfree(dst)),
                          "read length mismatch: expected %zu, got %ld", src.len, read_len);
        test_validatefree(fscmp(dst, src) == 0,
                          (fsfree(src), fsfree(dst)),
                          "content mismatch:\n src='%s'\n dst='%s'", fsstr(src), fsstr(dst));
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Длинная строка (1000 символов) */
    test_sub("subtest %d: roundtrip long string (1000 chars)", ++subnum);
    {
        const size_t N = 1000;
        char *src_buf = malloc(N + 1);
        for (size_t i = 0; i < N; ++i)
            src_buf[i] = (char)('A' + (i % 26));
        src_buf[N] = '\0';

        fs src = fscopy(src_buf);
        char *out_buf = malloc(N * 2 + 64);  // с запасом
        DS out_ds = dsCreatestrCap(out_buf, N * 2 + 64);

        long written = fs_dsserialize(&out_ds, &src);
        test_validatefree(
            written > 0, 
            (fsfree(src), free(src_buf), free(out_buf)),
            "serialize failed"
        );

        DS in_ds = dsCreateconst(out_buf);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, true);
        test_validatefree(read_len == (long) N, (fsfree(src), fsfree(dst), free(src_buf), free(out_buf)),
                          "read length mismatch: expected %zu, got %ld", N, read_len);
        test_validatefree(
            fscmpstr(dst, src_buf) == 0,
                        (fsfree(src), fsfree(dst), free(src_buf), free(out_buf)),
                    "content mismatch '%s' vs '%s'", fsstr(src), fsstr(dst));

        fsfree(src);
        fsfree(dst);
        free(src_buf);
        free(out_buf);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_dsload with DS_CONSTSTR -------------------------
static TestStatus
tf10_fs_ds_CONST_roundtrip(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простая строка */
    test_sub("subtest %d: load simple string from CONSTSTR", ++subnum);
    {
        fs src = fscopy("hello");
        char buffer[128];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));

        long written = fs_dsserialize(&out_ds, &src);
        test_validatefree(written > 0, fsfree(src), "serialize failed");

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, true);
        test_validatefree(read_len == (long)src.len,
                          (fsfree(src), fsfree(dst)),
                          "read length mismatch: expected %zu, got %ld", src.len, read_len);
        test_validatefree(fscmp(dst, src) == 0,
                          (fsfree(src), fsfree(dst)),
                          "content mismatch: src='%s', dst='%s'", fs_str(&src), fs_str(&dst));

        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Пустая строка */
    test_sub("subtest %d: load empty string from CONSTSTR", ++subnum);
    {
        fs src = fscopy("");
        char buffer[128];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));

        fs_dsserialize(&out_ds, &src);

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, true);
        test_validatefree(read_len == 0 && fs_len(&dst) == 0,
                          (fsfree(src), fsfree(dst)),
                          "expected empty, got len=%ld, fs_len=%zu", read_len, fs_len(&dst));

        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Строка со спецсимволами */
    test_sub("subtest %d: load escaped string from CONSTSTR", ++subnum);
    {
        fs src = fscopy("a\"b\\c\nd\te\rf");
        char buffer[256];
        DS out_ds = dsCreatestrCap(buffer, sizeof(buffer));

        fs_dsserialize(&out_ds, &src);

        DS in_ds = dsCreateconst(buffer);
        fs dst = FS();
        long read_len = fs_dsload(&in_ds, &dst, true);
        test_validatefree(read_len == (long)src.len,
                          (fsfree(src), fsfree(dst)),
                          "read length mismatch: expected %zu, got %ld", src.len, read_len);
        test_validatefree(fscmp(dst, src) == 0,
                          (fsfree(src), fsfree(dst)),
                          "content mismatch:\n src='%s'\n dst='%s'", fs_str(&src), fs_str(&dst));

        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Неверный заголовок */
    test_sub("subtest %d: invalid header restores pos", ++subnum);
    {
        const char *serialized = "BAD(\"5\"): \"hello\"";
        DS ds = dsCreateconst(serialized);
        fs dst = FS();
        size_t saved = ds.pos;

        long res = fs_dsload(&ds, &dst, true);
        test_validatefree(res == -1, fsfree(dst),
                          "expected -1, got %ld", res);
        test_validatefree(ds.pos == saved, fsfree(dst),
                          "pos must be restored to %zu, got %zu", saved, ds.pos);

        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Несовпадение длины */
    test_sub("subtest %d: length mismatch restores pos", ++subnum);
    {
        const char *serialized = "FS(\"10\"): \"hello\"";
        DS ds = dsCreateconst(serialized);
        fs dst = FS();
        size_t saved = ds.pos;

        long res = fs_dsload(&ds, &dst, true);
        test_validatefree(res == -1, fsfree(dst),
                          "expected -1, got %ld", res);
        test_validatefree(ds.pos == saved, fsfree(dst),
                          "pos must be restored to %zu, got %zu", saved, ds.pos);

        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            fs_dsload(NULL, &dst, true);
            test_validatefree(false, fsfree(dst), "must raise error for NULL DS");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }

        const char *serialized = "FS(\"1\"): \"a\"";
        DS ds = dsCreateconst(serialized);
        if (!try()) {
            fs_dsload(&ds, NULL, true);
            test_validate(false, "must raise error for NULL fs");
        } else {
            test_validate(true, "correctly raised error");
        }

        fsfree(dst);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_dsload with DS_FS (round-trip) -------------------------
static TestStatus
tf11_fs_ds_FS_roundtrip(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простая строка, use_buffer = true */
    test_sub("subtest %d: roundtrip simple string from DS_FS (buffer)", ++subnum);
    {
        fs src = fscopy("hello");
        fs serialized = FS();
        DS ds = dsCreatefs(&serialized);   // владение serialized переходит в ds

        long written = fs_dsserialize(&ds, &src);
        test_validatefree(
            written > 0, 
            (dsFree(&ds), fsfree(src)), 
            "serialize failed"
        );

        dsReset(&ds);                       // сбрасываем позицию для чтения
        // TODO: fs returned = dsDetachFs(&ds);

        // 
        fs dst = FS();
        long read_len = fs_dsload(&ds, &dst, true);
        test_validatefree(
            read_len == 5 && fscmp(dst, src) == 0,
            (dsFree(&ds), fsfree(src), fsfree(dst)),
            "roundtrip failed: len=%ld, dst='%s'", read_len, fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Простая строка, use_buffer = false */
    test_sub("subtest %d: roundtrip simple string from DS_FS (direct)", ++subnum);
    {
        fs src = fscopy("hello");
        fs serialized = FS();
        DS ds = dsCreatefs(&serialized);

        fs_dsserialize(&ds, &src);
        dsReset(&ds);

        fs dst = FS();
        long read_len = fs_dsload(&ds, &dst, false);
        test_validatefree(read_len == 5 && fscmp(dst, src) == 0,
                          (dsFree(&ds), fsfree(src), fsfree(dst)),
                          "roundtrip failed: len=%ld, dst='%s'", read_len, fs_str(&dst));

        dsFree(&ds);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Пустая строка */
    test_sub("subtest %d: roundtrip empty string from DS_FS", ++subnum);
    {
        fs src = fscopy("");
        fs serialized = FS();
        DS ds = dsCreatefs(&serialized);

        fs_dsserialize(&ds, &src);
        dsReset(&ds);

        fs dst = FS();
        long read_len = fs_dsload(&ds, &dst, true);
        test_validatefree(
            read_len == 0 && fs_len(&dst) == 0,
            (dsFree(&ds), fsfree(src), fsfree(dst)),
            "expected empty, got len=%ld, fs_len=%zu", read_len, fs_len(&dst)
        );

        dsFree(&ds);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Строка со спецсимволами */
    test_sub("subtest %d: roundtrip escaped string from DS_FS", ++subnum);
    {
        fs src = fscopy("a\"b\\c\nd\te\rf");
        fs serialized = FS();
        DS ds = dsCreatefs(&serialized);

        fs_dsserialize(&ds, &src);
        dsReset(&ds);

        fs dst = FS();
        long read_len = fs_dsload(&ds, &dst, true);
        test_validatefree(
            read_len == (long) src.len && fscmp(dst, src) == 0,
            (dsFree(&ds), fsfree(src), fsfree(dst)),
            "roundtrip failed: len=%ld, dst='%s'", read_len, fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Неверный заголовок (создаём вручную) */
    test_sub("subtest %d: invalid header restores pos", ++subnum);
    {
        fs bad = fscopy("BAD(\"5\"): \"hello\"");
        DS ds = dsCreatefs(&bad);
        fs dst = FS();
        size_t saved = ds.pos;

        long res = fs_dsload(&ds, &dst, true);
        test_validatefree(
            res == -1, (dsFree(&ds), fsfree(dst)),
            "expected -1, got %ld", res
        );
        test_validatefree(
            ds.pos == saved, 
            (dsFree(&ds), fsfree(dst)),
            "pos must be restored to %zu, got %zu", saved, ds.pos
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Несовпадение длины */
    test_sub("subtest %d: length mismatch restores pos", ++subnum);
    {
        fs bad = fscopy("FS(\"10\"): \"hello\"");
        DS ds = dsCreatefs(&bad);
        fs dst = FS();
        size_t saved = ds.pos;

        long res = fs_dsload(&ds, &dst, true);
        test_validatefree(
            res == -1, 
            (dsFree(&ds), fsfree(dst)),
            "expected -1, got %ld", res
        );
        test_validatefree(
            ds.pos == saved, 
            (dsFree(&ds), fsfree(dst)),
            "pos must be restored to %zu, got %zu", saved, ds.pos
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            fs_dsload(NULL, &dst, true);
            test_validatefree(false, fsfree(dst), "must raise error for NULL DS");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }

        fs src = fscopy("FS(\"1\"): \"a\"");
        DS ds = dsCreatefs(&src);
        if (!try()) {
            fs_dsload(&ds, NULL, true);
            test_validate(false, "must raise error for NULL fs");
        } else {
            test_validate(true, "correctly raised error");
        }

        fsfree(dst);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: multiple fs round-trip via DS_FS", ++subnum);
    {
        fs sources[] = {
            fscopy("hello11111111111"),
            fscopy(""),
            fscopy("a\"b\\c\nd\te\rf"),
            fscopy("final")
        };
        const size_t count = COUNT(sources);

        fs serialized = FS();
        DS ds = dsCreatefs(&serialized);

        /* Сериализуем все объекты подряд */
        for (size_t i = 0; i < count; i++) {
            long w = fs_dsserialize(&ds, &sources[i]);
            test_validatefree(
                w > 0,
                (dsFree(&ds), fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3])),
                "serialize failed at index %zu", i
            );
        }

        dsReset(&ds);       // replace to dsRelease()
        
        /* Последовательно читаем и сравниваем */
        for (size_t i = 0; i < count; i++) {
            fs dst = FS();
            long len = fs_dsload(&ds, &dst, true);
            test_validatefree(
                len == (long) sources[i].len && fscmp(dst, sources[i]) == 0,
                (dsFree(&ds), fsfree(dst), fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3])),
                "roundtrip mismatch at index %zu: len=%ld, expected=%zu",
                i, len, sources[i].len
            );
            fsfree(dst);
        }

        /* После извлечения всех записей должен быть конец потока */
        fs dst = FS();
        long res = fs_dsload(&ds, &dst, true);
        test_validatefree(
            res == -1,
            (dsFree(&ds), fsfree(dst), fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3])),
            "expected -1 at end of stream, got %ld", res
        );
        fsfree(dst);

        dsFree(&ds);
        fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3]);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_dsload with DS_FILE (round-trip) -------------------------
static TestStatus
tf12_fs_ds_FILE_roundtrip(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простая строка, use_buffer = true */
    test_sub("subtest %d: roundtrip simple string from DS_FILE (buffer)", ++subnum);
    {
        const char *path = "res/ds_adapter/fs_dsFILE_simple_buf.ds";

        fs src = fscopy("hello");
        DS ds = dsCreateFilename(path, "w+");
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds), fsfree(src)), "failed to create DS_FILE");

        long written = fs_dsserialize(&ds, &src);
        test_validatefree(written > 0, (dsFree(&ds), fsfree(src)), "serialize failed");

        dsReset(&ds);

        fs dst = FS();
        long read_len = fs_dsload(&ds, &dst, true);
        test_validatefree(
            read_len == 5 && fscmp(dst, src) == 0,
            (dsFree(&ds), fsfree(src), fsfree(dst)),
            "roundtrip failed: len=%ld, dst='%s'", read_len, fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Простая строка, use_buffer = false */
    test_sub("subtest %d: roundtrip simple string from DS_FILE (direct)", ++subnum);
    {
        const char *path = "res/ds_adapter/fs_dsFILE_simple_direct.ds";

        fs src = fscopy("hello");
        DS ds = dsCreateFilename(path, "w+");
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds), fsfree(src)), "failed to create DS_FILE");

        fs_dsserialize(&ds, &src);
        dsReset(&ds);

        fs dst = FS();
        long read_len = fs_dsload(&ds, &dst, false);
        test_validatefree(
            read_len == 5 && fscmp(dst, src) == 0,
            (dsFree(&ds), fsfree(src), fsfree(dst)),
            "roundtrip failed: len=%ld, dst='%s'", read_len, fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Пустая строка */
    test_sub("subtest %d: roundtrip empty string from DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/fs_dsFILE_empty_buf.ds";

        fs src = fscopy("");
        DS ds = dsCreateFilename(path, "w+");
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds), fsfree(src)), "failed to create DS_FILE");

        fs_dsserialize(&ds, &src);
        dsReset(&ds);

        fs dst = FS();
        long read_len = fs_dsload(&ds, &dst, true);
        test_validatefree(
            read_len == 0 && fs_len(&dst) == 0,
            (dsFree(&ds), fsfree(src), fsfree(dst)),
            "expected empty, got len=%ld, fs_len=%zu", read_len, fs_len(&dst)
        );

        dsFree(&ds);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Строка со спецсимволами */
    test_sub("subtest %d: roundtrip escaped string from DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/fs_dsFILE_escaped_buf.ds";

        fs src = fscopy("a\"b\\c\nd\te\rf");
        DS ds = dsCreateFilename(path, "w+");
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds), fsfree(src)), "failed to create DS_FILE");

        fs_dsserialize(&ds, &src);
        dsReset(&ds);

        fs dst = FS();
        long read_len = fs_dsload(&ds, &dst, true);
        test_validatefree(
            read_len == (long) src.len && fscmp(dst, src) == 0,
            (dsFree(&ds), fsfree(src), fsfree(dst)),
            "roundtrip failed: len=%ld, dst='%s'", read_len, fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Множественный round-trip через DS_FILE */
    test_sub("subtest %d: multiple fs round-trip via DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/fs_dsFILE_multiple_buf.ds";

        fs sources[] = {
            fscopy("hello11111111111"),
            fscopy(""),
            fscopy("a\"b\\c\nd\te\rf"),
            fscopy("final")
        };
        const size_t count = COUNT(sources);

        DS ds = dsCreateFilename(path, "w+");
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds), fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3])), "failed to create DS_FILE");

        for (size_t i = 0; i < count; i++) {
            long w = fs_dsserialize(&ds, &sources[i]);
            test_validatefree(
                w > 0,
                (dsFree(&ds), fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3])),
                "serialize failed at index %zu", i
            );
        }

        dsReset(&ds);

        for (size_t i = 0; i < count; i++) {
            fs dst = FS();
            long len = fs_dsload(&ds, &dst, true);
            test_validatefree(
                len == (long) sources[i].len && fscmp(dst, sources[i]) == 0,
                (dsFree(&ds), fsfree(dst), fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3])),
                "roundtrip mismatch at index %zu: len=%ld, expected=%zu",
                i, len, sources[i].len
            );
            fsfree(dst);
        }

        fs dst = FS();
        long res = fs_dsload(&ds, &dst, true);
        test_validatefree(
            res == -1,
            (dsFree(&ds), fsfree(dst), fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3])),
            "expected -1 at end of stream, got %ld", res
        );
        fsfree(dst);

        dsFree(&ds);
        fsfreeall(&sources[0], &sources[1], &sources[2], &sources[3]);
        fs_alloc_check(true);
    }

    /* 6. Неверный заголовок */
    test_sub("subtest %d: invalid header restores pos", ++subnum);
    {
        const char *path = "res/ds_adapter/fs_dsFILE_invalid_header.ds";

        FILE *fp = fopen(path, "w");
        test_validate(fp != NULL, "failed to create file for test");
        fprintf(fp, "BAD(\"5\"): \"hello\"");
        fclose(fp);

        DS ds = dsCreateFilename(path, "r");
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds)), "failed to open DS_FILE");

        fs dst = FS();
        off_t saved = dsGetpos(&ds);

        long res = fs_dsload(&ds, &dst, true);
        test_validatefree(
            res == -1,
            (dsFree(&ds), fsfree(dst)),
            "expected -1, got %ld", res
        );
        test_validatefree(
            dsGetpos(&ds) == saved,
            (dsFree(&ds), fsfree(dst)),
            "pos must be restored to %lld, got %lld", saved, dsGetpos(&ds)
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Несовпадение длины */
    test_sub("subtest %d: length mismatch restores pos", ++subnum);
    {
        const char *path = "res/ds_adapter/fs_dsFILE_length_mismatch.ds";

        FILE *fp = fopen(path, "w");
        test_validate(fp != NULL, "failed to create file for test");
        fprintf(fp, "FS(\"10\"): \"hello\"");
        fclose(fp);

        DS ds = dsCreateFilename(path, "r");
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds)), "failed to open DS_FILE");

        fs dst = FS();
        off_t saved = dsGetpos(&ds);

        long res = fs_dsload(&ds, &dst, true);
        test_validatefree(
            res == -1,
            (dsFree(&ds), fsfree(dst)),
            "expected -1, got %ld", res
        );
        test_validatefree(
            dsGetpos(&ds) == saved,
            (dsFree(&ds), fsfree(dst)),
            "pos must be restored to %lld, got %lld", saved, dsGetpos(&ds)
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 8. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            fs_dsload(NULL, &dst, true);
            test_validatefree(false, fsfree(dst), "must raise error for NULL DS");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }

        const char *path = "res/ds_adapter/fs_dsFILE_null_args.ds";

        FILE *fp = fopen(path, "w");
        test_validate(fp != NULL, "failed to create file for test");
        fprintf(fp, "FS(\"1\"): \"a\"");
        fclose(fp);

        DS ds = dsCreateFilename(path, "r");
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds)), "failed to open DS_FILE");

        if (!try()) {
            fs_dsload(&ds, NULL, true);
            test_validate(false, "must raise error for NULL fs");
        } else {
            test_validate(true, "correctly raised error");
        }

        fsfree(dst);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsReleaseFs -------------------------
static TestStatus
tf13_ds_release_fs(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: release and deserialize round-trip", ++subnum);
    {
        fs src = fscopy("hello");
        fs serialized = FS();
        DS ds = dsCreatefs(&serialized);   // serialized перемещён в ds

        long written = fs_dsserialize(&ds, &src);
        test_validatefree(written > 0, (fsfree(src)), "serialize failed");

        fs extracted = FS();
        test_validatefree(
            dsReleaseFs(&extracted, &ds),
            (fsfree(src), fsfree(extracted)),
            "release failed"
        );

        // extracted содержит сериализованные данные; создаём DS для чтения
        DS reader = dsCreatefs(&extracted);   // extracted перемещён в reader
        fs dst = FS();
        long read_len = fs_dsload(&reader, &dst, true);
        test_validatefree(
            read_len == (long)src.len,
            (dsFree(&reader), fsfree(src), fsfree(dst)),
            "read length mismatch: expected %zu, got %ld", src.len, read_len
        );
        test_validatefree(
            fscmp(dst, src) == 0,
            (dsFree(&reader), fsfree(src), fsfree(dst)),
            "content mismatch: src='%s', dst='%s'", fs_str(&src), fs_str(&dst)
        );

        dsFree(&reader);
        fsfree(src);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Только освобождение внутреннего fs */
    test_sub("subtest %d: release with dst == NULL", ++subnum);
    {
        fs          serialized = FS();
        DS          ds = dsCreatefs(&serialized);
        fs          src = fscopy("world");
        fs_dsserialize(&ds, &src);
        
        test_validatefree(
            dsReleaseFs(NULL, &ds),
            (fsfree(src)),
            "release with NULL must succeed"
        );
        test_validatefree(
            ds.s.v == NULL && ds.s.sz == 0,
            (dsFree(&ds), fsfree(src)),
            "internal fs must be freed"
        );
        // dsFree(&ds);
        fsfree(src);
        fs_alloc_check(true);
    }

    /* 3. Ошибка при не-FS DS */
    test_sub("subtest %d: release on non-FS type fails", ++subnum);
    {
        char buf[16] = "test";
        DS ds = dsCreatestrCap(buf, sizeof(buf));
        fs result = FS();

        test_validate(
            !dsReleaseFs(&result, &ds),
            "must return false for non-FS"
        );
        fsfree(result);
        fs_alloc_check(true);
    }

    /* 4. NULL pds */
    test_sub("subtest %d: NULL pds fails", ++subnum);
    {
        fs result = FS();

        test_validate(
            !dsReleaseFs(&result, NULL),
            "must return false for NULL"
        );
        fsfree(result);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: release and deserialize round-trip multiple strings", ++subnum);
    {
        // Исходные строки для теста
        fs src1 = fscopy("hello");
        fs src2 = fscopy("world");
        fs src3 = fscopy("");   // пустая строка тоже допустима

        // Создаём поток для записи
        fs serialized = FS();
        DS writer = dsCreatefs(&serialized);   // serialized перемещён в writer

        // Сериализуем все три строки в один поток
        long written1 = fs_dsserialize(&writer, &src1);
        long written2 = fs_dsserialize(&writer, &src2);
        long written3 = fs_dsserialize(&writer, &src3);

        // Можно проверить writtenX > 0, но для наглядности опустим
        test_validatefree(
            written1 > 5 && written2 > 5 && written3 > 0,
        (fsfree(src1), fsfree(src2), fsfree(src3)),
            "serialize failed"
        );
        // Передаём сериализованные данные через dsReleaseFs
        fs extracted = FS();
        test_validatefree(
            dsReleaseFs(&extracted, &writer),
            (fsfree(src1), fsfree(src2), fsfree(src3), fsfree(extracted)),
            "release failed"
        );

        // Создаём поток для чтения из извлечённых данных
        DS reader = dsCreatefs(&extracted);   // extracted перемещён в reader

        // Загружаем первую строку
        fs dst1 = FS();
        long read1 = fs_dsload(&reader, &dst1, true);
        test_validatefree(
            read1 == (long)src1.len,
            (dsFree(&reader), fsfree(src1), fsfree(src2), fsfree(src3), fsfree(dst1)),
            "read length mismatch for string 1: expected %zu, got %ld", src1.len, read1
        );
        test_validatefree(
            fscmp(dst1, src1) == 0,
            (dsFree(&reader), fsfree(src1), fsfree(src2), fsfree(src3), fsfree(dst1)),
            "content mismatch for string 1: src='%s', dst='%s'", fs_str(&src1), fs_str(&dst1)
        );
        fsfree(dst1);

        // Загружаем вторую строку
        fs dst2 = FS();
        long read2 = fs_dsload(&reader, &dst2, true);
        test_validatefree(
            read2 == (long)src2.len,
            (dsFree(&reader), fsfree(src1), fsfree(src2), fsfree(src3), fsfree(dst2)),
            "read length mismatch for string 2: expected %zu, got %ld", src2.len, read2
        );
        test_validatefree(
            fscmp(dst2, src2) == 0,
            (dsFree(&reader), fsfree(src1), fsfree(src2), fsfree(src3), fsfree(dst2)),
            "content mismatch for string 2: src='%s', dst='%s'", fs_str(&src2), fs_str(&dst2)
        );
        fsfree(dst2);

        // Загружаем третью строку
        fs dst3 = FS();
        long read3 = fs_dsload(&reader, &dst3, true);
        test_validatefree(
            read3 == (long)src3.len,
            (dsFree(&reader), fsfree(src1), fsfree(src2), fsfree(src3), fsfree(dst3)),
            "read length mismatch for string 3: expected %zu, got %ld", src3.len, read3
        );
        test_validatefree(
            fscmp(dst3, src3) == 0,
            (dsFree(&reader), fsfree(src1), fsfree(src2), fsfree(src3), fsfree(dst3)),
            "content mismatch for string 3: src='%s', dst='%s'", fs_str(&src3), fs_str(&dst3)
        );
        fsfree(dst3);

        // Освобождаем все ресурсы
        dsFree(&reader);
        fsfree(src1);
        fsfree(src2);
        fsfree(src3);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseQuotedUnlimfsDirect -------------------------
static TestStatus
tf14_ds_parse_quoted_unlim(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простая строка без escape */
    test_sub("subtest %d: parse simple quoted string", ++subnum);
    {
        const char *input = "\"hello\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            res && dst.len == 5,
            (dsFree(&ds), fsfree(dst)),
            "expected len 5, got %zu", dst.len
        );
        test_validatefree(
            fscmp(dst, FSLITERAL("hello")) == 0,
            (dsFree(&ds), fsfree(dst)),
            "content mismatch: got '%s'", fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Пустая строка */
    test_sub("subtest %d: parse empty quoted string", ++subnum);
    {
        const char *input = "\"\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            res && dst.len == 0,
            (dsFree(&ds), fsfree(dst)),
            "expected len 0, got %zu", dst.len
        );
        test_validatefree(
            fscmp(dst, FSLITERAL("")) == 0,
            (dsFree(&ds), fsfree(dst)),
            "content mismatch: got '%s'", fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Escape-последовательности: \\, \", \n, \r, \t */
    test_sub("subtest %d: parse escaped string", ++subnum);
    {
        const char *input = "\"a\\\"b\\\\c\\nd\\te\\rf\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            res && dst.len == 11,
            (dsFree(&ds), fsfree(dst)),
            "expected len 11, got %zu", dst.len
        );
        test_validatefree(
            fscmp(dst, FSLITERAL("a\"b\\c\nd\te\rf")) == 0,
            (dsFree(&ds), fsfree(dst)),
            "content mismatch: got '%s'", fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Экранированная кавычка в середине строки */
    test_sub("subtest %d: escaped quote inside", ++subnum);
    {
        const char *input = "\"he\\\"llo\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            res && dst.len == 6,
            (dsFree(&ds), fsfree(dst)),
            "expected len 6, got %zu", dst.len
        );
        test_validatefree(
            fscmp(dst, FSLITERAL("he\"llo")) == 0,
            (dsFree(&ds), fsfree(dst)),
            "content mismatch: got '%s'", fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Несколько escape подряд */
    test_sub("subtest %d: multiple escapes", ++subnum);
    {
        const char *input = "\"\\n\\t\\r\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            res && dst.len == 3,
            (dsFree(&ds), fsfree(dst)),
            "expected len 3, got %zu", dst.len
        );
        test_validatefree(
            fscmp(dst, FSLITERAL("\n\t\r")) == 0,
            (dsFree(&ds), fsfree(dst)),
            "content mismatch: got '%s'", fs_str(&dst)
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Ошибка: нет открывающей кавычки */
    test_sub("subtest %d: missing opening quote", ++subnum);
    {
        const char *input = "hello";
        DS ds = dsCreateconst(input);
        fs dst = FS();
        size_t saved = ds.pos;

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            !res,
            (dsFree(&ds), fsfree(dst)),
            "expected false, got true"
        );
        test_validatefree(
            ds.pos == saved,
            (dsFree(&ds), fsfree(dst)),
            "pos must be restored, expected %zu, got %zu", saved, ds.pos
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Ошибка: нет закрывающей кавычки */
    test_sub("subtest %d: missing closing quote", ++subnum);
    {
        const char *input = "\"hello";
        DS ds = dsCreateconst(input);
        fs dst = FS();
        size_t saved = ds.pos;

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            !res,
            (dsFree(&ds), fsfree(dst)),
            "expected false, got true"
        );
        test_validatefree(
            ds.pos == saved,
            (dsFree(&ds), fsfree(dst)),
            "pos must be restored, expected %zu, got %zu", saved, ds.pos
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 8. Ошибка: некорректный escape (неизвестный символ после \\) */
    test_sub("subtest %d: invalid escape sequence", ++subnum);
    {
        const char *input = "\"\\x\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();
        size_t saved = ds.pos;

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            !res,
            (dsFree(&ds), fsfree(dst)),
            "expected false, got true"
        );
        test_validatefree(
            ds.pos == saved,
            (dsFree(&ds), fsfree(dst)),
            "pos must be restored, expected %zu, got %zu", saved, ds.pos
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 9. Ошибка: EOF сразу после открывающей кавычки */
    test_sub("subtest %d: EOF after opening quote", ++subnum);
    {
        const char *input = "\"";
        DS ds = dsCreateconst(input);
        fs dst = FS();
        size_t saved = ds.pos;

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            !res,
            (dsFree(&ds), fsfree(dst)),
            "expected false, got true"
        );
        test_validatefree(
            ds.pos == saved,
            (dsFree(&ds), fsfree(dst)),
            "pos must be restored, expected %zu, got %zu", saved, ds.pos
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 10. Длинная строка для проверки автоматического расширения */
    test_sub("subtest %d: long string with auto-resize", ++subnum);
    {
        char input[1024];
        memset(input, 'a', 100);
        input[100] = '\0';
        char quoted[105];
        snprintf(quoted, sizeof(quoted), "\"%s\"", input);
        
        DS ds = dsCreateconst(quoted);
        fs dst = FS();

        bool res = dsParseQuotedUnlimfsDirect(&ds, &dst);
        test_validatefree(
            res && dst.len == 100,
            (dsFree(&ds), fsfree(dst)),
            "expected len 100, got %zu", dst.len
        );
        test_validatefree(
            strncmp(fs_str(&dst), input, 100) == 0,
            (dsFree(&ds), fsfree(dst)),
            "content mismatch"
        );

        dsFree(&ds);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 11. NULL in */
    test_sub("subtest %d: NULL in raises error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            dsParseQuotedUnlimfsDirect(NULL, &dst);
            test_validatefree(false, fsfree(dst), "must raise error for NULL in");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 12. NULL dst */
    test_sub("subtest %d: NULL dst raises error", ++subnum);
    {
        DS ds = dsCreateconst("\"test\"");
        if (!try()) {
            dsParseQuotedUnlimfsDirect(&ds, NULL);
            test_validate(false, "must raise error for NULL dst");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 13. Не-аллоцируемый dst (например, FSLITERAL) */
    test_sub("subtest %d: non-allocatable dst raises error", ++subnum);
    {
        DS ds = dsCreateconst("\"test\"");
        fs dst = FSLITERAL("initial");  // статический, не аллоцируемый
        if (!try()) {
            dsParseQuotedUnlimfsDirect(&ds, &dst);
            test_validate(false, "must raise error for non-allocatable dst");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&ds);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST ds_parse_quoted_core -------------------------
static TestStatus
tf15_ds_parse_quoted_core(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простая строка, безлимит (maxlen=0), out = DS_FS */
    test_sub("subtest %d: simple string, unlimited, out DS_FS", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 5,
            (dsFree(&in), dsFree(&out)),
            "expected success and pos=5, got res=%d pos=%zu", res, out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");
        test_validatefree(
            fscmp(f, FSLITERAL("hello")) == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch: got '%s'", fs_str(&f)
        );

        dsFree(&in);
        fsfree(f);   // освобождает f
        fs_alloc_check(true);
    }

    /* 2. Пустая строка, maxlen=0 */
    test_sub("subtest %d: empty quoted string", ++subnum);
    {
        const char *input = "\"\"";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 0,
            (dsFree(&in), dsFree(&out)),
            "expected pos=0, got %zu", out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");
        test_validatefree(
            fscmp(f, FSLITERAL("")) == 0,
            (dsFree(&in), dsFree(&out)),
            "expected empty, got '%s'", fs_str(&f)
        );

        dsFree(&in);
        fsfree(f);
        fs_alloc_check(true);
    }

    /* 3. Escape-последовательности: \\, \", \n, \r, \t */
    test_sub("subtest %d: escaped string with all escapes", ++subnum);
    {
        const char *input = "\"a\\\"b\\\\c\\nd\\te\\rf\"";  // на входе: a\"b\\c\nd\te\rf
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 11,
            (dsFree(&in), dsFree(&out)),
            "expected pos=11, got %zu", out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");
        test_validatefree(
            fscmp(f, FSLITERAL("a\"b\\c\nd\te\rf")) == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch: got '%s'", fs_str(&f)
        );

        dsFree(&in);
        fsfree(f);
        fs_alloc_check(true);
    }

    /* 4. Экранированная кавычка внутри строки */
    test_sub("subtest %d: escaped quote inside", ++subnum);
    {
        const char *input = "\"he\\\"llo\"";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 6,
            (dsFree(&in), dsFree(&out)),
            "expected pos=6, got %zu", out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");
        test_validatefree(
            fscmp(f, FSLITERAL("he\"llo")) == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch: got '%s'", fs_str(&f)
        );

        dsFree(&in);
        fsfree(f);
        fs_alloc_check(true);
    }

    /* 5. Несколько escape подряд */
    test_sub("subtest %d: multiple escapes in a row", ++subnum);
    {
        const char *input = "\"\\n\\t\\r\"";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 3,
            (dsFree(&in), dsFree(&out)),
            "expected pos=3, got %zu", out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");
        test_validatefree(
            fscmp(f, FSLITERAL("\n\t\r")) == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch"
        );

        dsFree(&in);
        fsfree(f);
        fs_alloc_check(true);
    }

    /* 6. Ошибка: отсутствует начальный символ */
    test_sub("subtest %d: missing begin", ++subnum);
    {
        const char *input = "hello";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);
        size_t saved = in.pos;

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            !res,
            (dsFree(&in), dsFree(&out)),
            "expected false, got true"
        );
        test_validatefree(
            in.pos == saved,
            (dsFree(&in), dsFree(&out)),
            "in pos not restored: expected %zu, got %zu", saved, in.pos
        );

        dsFree(&in);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 7. Ошибка: отсутствует конечный символ */
    test_sub("subtest %d: missing end", ++subnum);
    {
        const char *input = "\"hello";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);
        size_t saved = in.pos;

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            !res,
            (dsFree(&in), dsFree(&out)),
            "expected false, got true"
        );
        test_validatefree(
            in.pos == saved,
            (dsFree(&in), dsFree(&out)),
            "in pos not restored"
        );

        dsFree(&in);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 8. Ошибка: некорректный escape */
    test_sub("subtest %d: invalid escape", ++subnum);
    {
        const char *input = "\"\\x\"";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);
        size_t saved = in.pos;

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            !res,
            (dsFree(&in), dsFree(&out)),
            "expected false, got true"
        );
        test_validatefree(
            in.pos == saved,
            (dsFree(&in), dsFree(&out)),
            "in pos not restored"
        );

        dsFree(&in);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 9. Ошибка: EOF сразу после начального символа */
    test_sub("subtest %d: EOF after begin", ++subnum);
    {
        const char *input = "\"";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);
        size_t saved = in.pos;

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            !res,
            (dsFree(&in), dsFree(&out)),
            "expected false, got true"
        );
        test_validatefree(
            in.pos == saved,
            (dsFree(&in), dsFree(&out)),
            "in pos not restored"
        );

        dsFree(&in);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 10. Ограничение maxlen: строка длиннее лимита -> ошибка, восстановление */
    test_sub("subtest %d: maxlen exceeded", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);
        size_t saved = in.pos;

        bool res = ds_parse_quoted_core(&in, &out, 3, '"', '"', false); // лимит 3, строка "hello" (5 симв.)
        test_validatefree(
            !res,
            (dsFree(&in), dsFree(&out)),
            "expected false, got true"
        );
        test_validatefree(
            in.pos == saved,
            (dsFree(&in), dsFree(&out)),
            "in pos not restored"
        );

        dsFree(&in);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 11. Ограничение maxlen: строка точно в лимит -> успех */
    test_sub("subtest %d: maxlen exactly fits", ++subnum);
    {
        const char *input = "\"he\"";   // 2 символа, maxlen=3 (по нашей логике влезает)
        DS in = dsCreateconst(input);
        fs f = fsinit(8);
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 3, '"', '"', false);
        test_validatefree(
            res && out.pos == 2,
            (dsFree(&in), dsFree(&out)),
            "expected success and pos=2, got res=%d pos=%zu", res, out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");
        test_validatefree(
            fscmp(f, FSLITERAL("he")) == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch"
        );

        dsFree(&in);
        dsFree(&out);
        fsfree(f);
        fs_alloc_check(true);
    }

    /* 12. Ограничение maxlen=0 (безлимит) с длинной строкой */
    test_sub("subtest %d: unlimited long string", ++subnum);
    {
        char longstr[101];
        memset(longstr, 'a', 100);
        longstr[100] = '\0';
        char input[110];
        snprintf(input, sizeof(input), "\"%s\"", longstr);
        DS in = dsCreateconst(input);
        fs f = FS();
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 100,
            (dsFree(&in), dsFree(&out)),
            "expected pos=100, got %zu", out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");
        
        test_validatefree(
            fsncmpstr(f, longstr, 100) == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch"
        );

        dsFree(&in);
        fsfree(f);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 13. Выходной поток DS_STR (фиксированный буфер) с достаточной ёмкостью */
    test_sub("subtest %d: out DS_STR with capacity", ++subnum);
    {
        const char *input = "\"test\"";
        DS          in = dsCreateconst(input);
        char        buf[5] = {'1', '2', '3', '4', '5'};
        DS          out = dsCreatestrCap(buf, sizeof(buf));

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 4,
            (dsFree(&in), dsFree(&out)),
            "expected pos=4, got %zu", out.pos
        );
        
        DSTECHPRINT(out);
        printf("11111111: '%s'\n", buf);

        test_validatefree(
            strcmp(buf, "test") == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch: got '%s'", buf
        );

        dsFree(&in);
        dsFree(&out);   // DS_STR не освобождает буфер, только сбрасывает структуру
        fs_alloc_check(true);
    }

    /* 14. Разные типы входного потока: DS_STR */
    test_sub("subtest %d: in DS_STR", ++subnum);
    {
        char mutable_input[] = "\"abc\"";
        DS in = dsCreatestr(mutable_input);
        fs f = FS();
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 3,
            (dsFree(&in), dsFree(&out)),
            "expected pos=3, got %zu", out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");

        test_validatefree(
            fscmp(f, FSLITERAL("abc")) == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch"
        );

        dsFree(&in);
        dsFree(&out);
        fsfree(f);
        fs_alloc_check(true);
    }

    /* 15. Разные типы входного потока: DS_FS */
    test_sub("subtest %d: in DS_FS", ++subnum);
    {
        fs input_fs = fscopy("\"xyz\"");
        DS in = dsCreatefs(&input_fs);
        fs f = FS();
        DS out = dsCreatefs(&f);

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"', false);
        test_validatefree(
            res && out.pos == 3,
            (dsFree(&in), dsFree(&out)),
            "expected pos=3, got %zu", out.pos
        );
        bool released = dsReleaseFs(&f, &out);
        test_validatefree(released,
                          (dsFree(&in), fsfree(f)),
                          "dsReleaseFs failed");
        test_validatefree(
            fscmp(f, FSLITERAL("xyz")) == 0,
            (dsFree(&in), dsFree(&out)),
            "content mismatch"
        );

        dsFree(&in);
        dsFree(&out);
        fsfree(f);
        fs_alloc_check(true);
    }

    /* 16. NULL in */
    test_sub("subtest %d: NULL in raises error", ++subnum);
    {
        fs f = FS();
        DS out = dsCreatefs(&f);
        if (!try()) {
            ds_parse_quoted_core(NULL, &out, 0, '"', '"', false);
            test_validatefree(false, dsFree(&out), "must raise error for NULL in");
        } else {
            test_validatefree(true, dsFree(&out), "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 17. NULL out */
    test_sub("subtest %d: NULL out raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        if (!try()) {
            ds_parse_quoted_core(&in, NULL, 0, '"', '"', false);
            test_validate(false, "must raise error for NULL out");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseQuotedLimfsBuffered (maxlen > 0) -------------------------
static TestStatus
tf16_ds_parse_quoted_limfs_buffered(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_CONSTSTR: успех, maxlen больше длины */
    test_sub("subtest %d: DS_CONSTSTR success (maxlen > len)", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfs(&in, &dst, 10, true);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 5, got %zu", dst.len);
        test_validatefree(fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. DS_CONSTSTR: успех, точное совпадение maxlen == длина */
    test_sub("subtest %d: DS_CONSTSTR success (maxlen == len + 1)", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 5 + 1);    // 5 + 1 для '\0'
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 5, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("hello")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. DS_CONSTSTR: превышение maxlen, dst не изменён */
    test_sub("subtest %d: DS_CONSTSTR maxlen exceeded, dst unchanged", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 3);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false, got true");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. DS_CONSTSTR: пустая строка с maxlen > 0 */
    test_sub("subtest %d: DS_CONSTSTR empty string, maxlen > 0", ++subnum);
    {
        const char *input = "\"\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 1);
        test_validatefree(res && dst.len == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 0, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. DS_STR: успех, maxlen больше длины */
    test_sub("subtest %d: DS_STR success", ++subnum);
    {
        char input[] = "\"world\"";
        DS in = dsCreatestr(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 10);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 5, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("world")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. DS_STR: превышение maxlen, dst не изменён */
    test_sub("subtest %d: DS_STR maxlen exceeded, dst unchanged", ++subnum);
    {
        char input[] = "\"world\"";
        DS in = dsCreatestr(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 3);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. DS_FS: успех, maxlen больше длины */
    test_sub("subtest %d: DS_FS success", ++subnum);
    {
        fs input_fs = fscopy("\"from fs\"");
        DS in = dsCreatefs(&input_fs);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 10);
        test_validatefree(res && dst.len == 7,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 7, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("from fs")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 8. DS_FS: превышение maxlen, dst не изменён */
    test_sub("subtest %d: DS_FS maxlen exceeded, dst unchanged", ++subnum);
    {
        fs input_fs = fscopy("\"from fs\"");
        DS in = dsCreatefs(&input_fs);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 3);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 9. DS_FILE: успех, maxlen больше длины */
    test_sub("subtest %d: DS_FILE success", ++subnum);
    {
        const char *path = "res/ds_adapter/dsParseQuotedLimitedfsBuffered_file_success.txt";
        FILE *fp = fopen(path, "w");
        test_validate(fp != NULL, "failed to create test file");
        fputs("\"file ok\"", fp);
        fclose(fp);

        DS in = dsCreateFilename(path, "r");
        test_validatefree(in.type == DS_FILE, (dsFree(&in)), "failed to open DS_FILE");

        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 10);
        test_validatefree(res && dst.len == 7,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 7, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("file ok")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 10. DS_FILE: превышение maxlen, dst не изменён */
    test_sub("subtest %d: DS_FILE maxlen exceeded, dst unchanged", ++subnum);
    {
        const char *path = "res/ds_adapter/dsParseQuotedLimitedfsBuffered_file_error.txt";
        FILE *fp = fopen(path, "w");
        test_validate(fp != NULL, "failed to create test file");
        fputs("\"file ok\"", fp);   // содержимое корректное, но мы дадим маленький maxlen
        fclose(fp);

        DS in = dsCreateFilename(path, "r");
        test_validatefree(in.type == DS_FILE, (dsFree(&in)), "failed to open DS_FILE");

        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = dsGetpos(&in);

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 3);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree( (size_t) dsGetpos(&in) == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 11. NULL in */
    test_sub("subtest %d: NULL in raises error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            dsParseQuotedLimfsBuffered(NULL, &dst, 10);
            test_validatefree(false, fsfree(dst), "must raise error");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 12. NULL dst */
    test_sub("subtest %d: NULL dst raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        if (!try()) {
            dsParseQuotedLimfsBuffered(&in, NULL, 10);
            test_validate(false, "must raise error");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

        /* 13. DS_CONSTSTR: пустая строка, maxlen=1 (ровно на нуль-терминатор) */
    test_sub("subtest %d: empty string, maxlen=1", ++subnum);
    {
        const char *input = "\"\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 1);
        test_validatefree(res && dst.len == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty success, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty content");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 14. DS_CONSTSTR: строка из одного символа, maxlen=2 (успех) */
    test_sub("subtest %d: one char, maxlen=2", ++subnum);
    {
        const char *input = "\"a\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 2);
        test_validatefree(res && dst.len == 1,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 1, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("a")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 15. DS_CONSTSTR: строка из одного символа, maxlen=1 (ошибка) */
    test_sub("subtest %d: one char, maxlen=1 (error)", ++subnum);
    {
        const char *input = "\"a\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 1);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected error, got true");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 16. DS_CONSTSTR: строка из двух символов, maxlen=2 (ошибка, нужен 1 байт под '\0') */
    test_sub("subtest %d: two chars, maxlen=2 (error)", ++subnum);
    {
        const char *input = "\"ab\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 2);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected error");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 17. DS_CONSTSTR: строка из двух символов, maxlen=3 (успех) */
    test_sub("subtest %d: two chars, maxlen=3", ++subnum);
    {
        const char *input = "\"ab\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 3);
        test_validatefree(res && dst.len == 2,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 2, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("ab")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 18. DS_FS: пустая строка, maxlen=1 */
    test_sub("subtest %d: DS_FS empty string, maxlen=1", ++subnum);
    {
        fs input_fs = fscopy("\"\"");
        DS in = dsCreatefs(&input_fs);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 1);
        test_validatefree(res && dst.len == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty success");
        test_validatefree(fscmp(dst, FSLITERAL("")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty content");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 19. DS_FS: один символ, maxlen=2 */
    test_sub("subtest %d: DS_FS one char, maxlen=2", ++subnum);
    {
        fs input_fs = fscopy("\"z\"");
        DS in = dsCreatefs(&input_fs);
        fs dst = fscopy("original");

        bool res = dsParseQuotedLimfsBuffered(&in, &dst, 2);
        test_validatefree(res && dst.len == 1,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 1");
        test_validatefree(fscmp(dst, FSLITERAL("z")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseQuotedUnlimfsBufferre (безлимитный буферизованный режим) -------------------------
static TestStatus
tf17_ds_parse_quoted_unlimfs_buffered(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_CONSTSTR: успех, простая строка */
    test_sub("subtest %d: DS_CONSTSTR success", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 5, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("hello")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree(in.pos == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected %zu, got %zu", strlen(input), in.pos);

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. DS_CONSTSTR: успех, пустая строка */
    test_sub("subtest %d: DS_CONSTSTR empty string", ++subnum);
    {
        const char *input = "\"\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 0, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty content");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. DS_CONSTSTR: успех, экранированные символы */
    test_sub("subtest %d: DS_CONSTSTR escaped string", ++subnum);
    {
        const char *input = "\"a\\\"b\\\\c\\nd\\te\\rf\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 11,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 11, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("a\"b\\c\nd\te\rf")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. DS_CONSTSTR: успех, очень длинная строка (проверка расширения) */
    test_sub("subtest %d: DS_CONSTSTR long string", ++subnum);
    {
        char longstr[201];
        memset(longstr, 'A', 200);
        longstr[200] = '\0';
        char input[205];
        snprintf(input, sizeof(input), "\"%s\"", longstr);

        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 200,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 200, got %zu", dst.len);
        test_validatefree(strncmp(fs_str(&dst), longstr, 200) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. DS_CONSTSTR: ошибка (нет открывающей кавычки), dst не изменён */
    test_sub("subtest %d: DS_CONSTSTR missing begin, dst unchanged", ++subnum);
    {
        const char *input = "hello";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false, got true");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. DS_CONSTSTR: ошибка (нет закрывающей кавычки), dst не изменён */
    test_sub("subtest %d: DS_CONSTSTR missing end, dst unchanged", ++subnum);
    {
        const char *input = "\"hello";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false, got true");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. DS_CONSTSTR: ошибка (некорректный escape), dst не изменён */
    test_sub("subtest %d: DS_CONSTSTR invalid escape, dst unchanged", ++subnum);
    {
        const char *input = "\"\\x\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false, got true");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 8. DS_STR: успех */
    test_sub("subtest %d: DS_STR success", ++subnum);
    {
        char input[] = "\"from str\"";
        DS in = dsCreatestr(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 8,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 8, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("from str")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 9. DS_FS: успех */
    test_sub("subtest %d: DS_FS success", ++subnum);
    {
        fs input_fs = fscopy("\"from fs\"");
        DS in = dsCreatefs(&input_fs);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 7,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 7, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("from fs")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 10. DS_FILE: успех (создаём файл в res/ds_adapter) */
    test_sub("subtest %d: DS_FILE success", ++subnum);
    {
        const char *path = "res/ds_adapter/dsParseQuotedUnlimfsBufferre_file_success.txt";
        FILE *fp = fopen(path, "w");
        test_validate(fp != NULL, "failed to create test file");
        fputs("\"from file\"", fp);
        fclose(fp);

        DS in = dsCreateFilename(path, "r");
        test_validatefree(in.type == DS_FILE, (dsFree(&in)), "failed to open DS_FILE");

        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 9,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 9, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("from file")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 11. DS_FILE: ошибка, dst не изменён (файл без кавычек) */
    test_sub("subtest %d: DS_FILE error, dst unchanged", ++subnum);
    {
        const char *path = "res/ds_adapter/dsParseQuotedUnlimfsBufferre_file_error.txt";
        FILE *fp = fopen(path, "w");
        test_validate(fp != NULL, "failed to create test file");
        fputs("not quoted", fp);
        fclose(fp);

        DS in = dsCreateFilename(path, "r");
        test_validatefree(in.type == DS_FILE, (dsFree(&in)), "failed to open DS_FILE");

        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = dsGetpos(&in);

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree( (size_t) dsGetpos(&in) == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 12. DS_CONSTSTR: строка только из перевода строки (escape \n) */
    test_sub("subtest %d: DS_CONSTSTR newline escape only", ++subnum);
    {
        const char *input = "\"\\n\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 1,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 1, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("\n")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 13. DS_CONSTSTR: строка с одним обратным слэшем (escape \\) */
    test_sub("subtest %d: DS_CONSTSTR backslash escape only", ++subnum);
    {
        const char *input = "\"\\\\\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 1,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 1, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("\\")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 14. DS_CONSTSTR: экранированная кавычка в конце строки */
    test_sub("subtest %d: DS_CONSTSTR escaped quote at end", ++subnum);
    {
        const char *input = "\"abc\\\"\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(res && dst.len == 4,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 4, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("abc\"")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 15. DS_CONSTSTR: ошибка: обратный слэш в конце без символа */
    test_sub("subtest %d: DS_CONSTSTR backslash at end", ++subnum);
    {
        const char *input = "\"\\";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 16. DS_CONSTSTR: ошибка: экранированная кавычка без закрывающей */
    test_sub("subtest %d: DS_CONSTSTR escaped quote without closing", ++subnum);
    {
        const char *input = "\"abc\\\"";
        DS in = dsCreateconst(input);
        fs dst = fscopy("original");
        size_t saved_len = dst.len;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedUnlimfsBufferre(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(dst.len == saved_len &&
                          fscmp(dst, FSLITERAL("original")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 17. NULL in */
    test_sub("subtest %d: NULL in raises error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            dsParseQuotedUnlimfsBufferre(NULL, &dst);
            test_validatefree(false, fsfree(dst), "must raise error");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 18. NULL dst */
    test_sub("subtest %d: NULL dst raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        if (!try()) {
            dsParseQuotedUnlimfsBufferre(&in, NULL);
            test_validate(false, "must raise error");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

        /* 19. DS_CONSTSTR: несколько строк подряд через массив */
    test_sub("subtest %d: multiple sequential strings (array)", ++subnum);
    {
        const char *input = "\"first\"\"second\"\"third\"";
        DS in = dsCreateconst(input);
        fs dst[3] = { fscopy("original"), fscopy("original"), fscopy("original") };
        const char *expected[] = { "first", "second", "third" };
        bool all_ok = true;

        for (int i = 0; i < 3; i++) {
            bool res = dsParseQuotedUnlimfsBufferre(&in, &dst[i]);
            if (!res || dst[i].len != strlen(expected[i]) ||
                fscmp(dst[i], FSLITERAL(expected[i])) != 0) {
                all_ok = false;
                break;
            }
        }

        test_validatefree(all_ok,
                          (dsFree(&in), fsfree(dst[0]), fsfree(dst[1]), fsfree(dst[2])),
                          "mismatch during sequential parsing");

        // проверяем, что позиция в конце
        test_validatefree(in.pos == strlen(input),
                          (dsFree(&in), fsfree(dst[0]), fsfree(dst[1]), fsfree(dst[2])),
                          "in.pos expected %zu, got %zu", strlen(input), in.pos);

        // попытка прочитать ещё раз должна вернуть false и не менять dst
        fs dst4 = fscopy("original");
        size_t saved_len = dst4.len;
        size_t saved_pos = in.pos;
        bool res4 = dsParseQuotedUnlimfsBufferre(&in, &dst4);
        test_validatefree(!res4 &&
                          dst4.len == saved_len &&
                          fscmp(dst4, FSLITERAL("original")) == 0 &&
                          in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst[0]), fsfree(dst[1]), fsfree(dst[2]), fsfree(dst4)),
                          "expected EOF or error after all strings");

        dsFree(&in);
        fsfreeall(dst + 0, dst + 1, dst + 2, &dst4);
        
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseQuotedLimStringDirect (прямой режим) -------------------------
static TestStatus
tf18_ds_parse_quoted_lim_string_direct(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Успех: простая строка, буфер достаточного размера */
    test_sub("subtest %d: simple string, buffer enough", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        char dst[10];
        memset(dst, 'x', sizeof(dst));   // заполняем ненулевыми
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 5,
                          (dsFree(&in)),
                          "expected success and out_len=5, got res=%d out_len=%zu", res, out_len);
        test_validatefree(strcmp(dst, "hello") == 0,
                          (dsFree(&in)),
                          "content mismatch: got '%s'", dst);
        test_validatefree(in.pos == strlen(input),
                          (dsFree(&in)),
                          "in.pos expected %zu, got %zu", strlen(input), in.pos);

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 2. Успех: пустая строка */
    test_sub("subtest %d: empty string", ++subnum);
    {
        const char *input = "\"\"";
        DS in = dsCreateconst(input);
        char dst[10];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 0,
                          (dsFree(&in)),
                          "expected success and out_len=0, got res=%d out_len=%zu", res, out_len);
        test_validatefree(strcmp(dst, "") == 0,
                          (dsFree(&in)),
                          "expected empty, got '%s'", dst);

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 3. Успех: escape-последовательности */
    test_sub("subtest %d: escaped string", ++subnum);
    {
        const char *input = "\"a\\\"b\\\\c\\nd\\te\\rf\"";
        DS in = dsCreateconst(input);
        char dst[20];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 11,
                          (dsFree(&in)),
                          "expected out_len=11, got res=%d out_len=%zu", res, out_len);
        test_validatefree(strcmp(dst, "a\"b\\c\nd\te\rf") == 0,
                          (dsFree(&in)),
                          "content mismatch: got '%s'", dst);

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 4. Ошибка: нет открывающей кавычки, позиция восстановлена */
    test_sub("subtest %d: missing begin, pos restored", ++subnum);
    {
        const char *input = "hello";
        DS in = dsCreateconst(input);
        char dst[10];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(!res,
                          (dsFree(&in)),
                          "expected false, got true");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in)),
                          "in.pos not restored: expected %zu, got %zu", saved_pos, in.pos);

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 5. Ошибка: буфер слишком мал (maxlen меньше строки) */
    test_sub("subtest %d: buffer too small", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        char dst[3];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(!res,
                          (dsFree(&in)),
                          "expected false, got true");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in)),
                          "in.pos not restored");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 6. Ошибка: некорректный escape, позиция восстановлена */
    test_sub("subtest %d: invalid escape", ++subnum);
    {
        const char *input = "\"\\x\"";
        DS in = dsCreateconst(input);
        char dst[10];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(!res,
                          (dsFree(&in)),
                          "expected false");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in)),
                          "in.pos not restored");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 7. Успех: DS_STR как источник */
    test_sub("subtest %d: input DS_STR", ++subnum);
    {
        char input[] = "\"world\"";
        DS in = dsCreatestr(input);
        char dst[10];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 5,
                          (dsFree(&in)),
                          "expected out_len=5, got %zu", out_len);
        test_validatefree(strcmp(dst, "world") == 0,
                          (dsFree(&in)),
                          "content mismatch");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 8. Успех: DS_FS как источник */
    test_sub("subtest %d: input DS_FS", ++subnum);
    {
        fs input_fs = fscopy("\"from fs\"");
        DS in = dsCreatefs(&input_fs);
        char dst[16];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 7,
                          (dsFree(&in)),
                          "expected out_len=7, got %zu", out_len);
        test_validatefree(strcmp(dst, "from fs") == 0,
                          (dsFree(&in)),
                          "content mismatch");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 9. Успех: DS_FILE как источник */
    test_sub("subtest %d: input DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/dsParseQuotedLimitStringDirect_file.txt";
        FILE *fp = fopen(path, "w");
        test_validate(fp != NULL, "failed to create test file");
        fputs("\"from file\"", fp);
        fclose(fp);

        DS in = dsCreateFilename(path, "r");
        test_validatefree(in.type == DS_FILE, (dsFree(&in)), "failed to open DS_FILE");

        char dst[16];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringDirect(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 9,
                          (dsFree(&in)),
                          "expected out_len=9, got %zu", out_len);
        test_validatefree(strcmp(dst, "from file") == 0,
                          (dsFree(&in)),
                          "content mismatch");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 10. NULL in */
    test_sub("subtest %d: NULL in raises error", ++subnum);
    {
        char dst[10];
        memset(dst, 'x', sizeof(dst));
        if (!try()) {
            dsParseQuotedLimStringDirect(NULL, dst, sizeof(dst), NULL);
            test_validate(false, "must raise error for NULL in");
        } else {
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 11. NULL dst */
    test_sub("subtest %d: NULL dst raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        if (!try()) {
            dsParseQuotedLimStringDirect(&in, NULL, 10, NULL);
            test_validate(false, "must raise error for NULL dst");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 12. dst_capacity = 0 */
    test_sub("subtest %d: zero capacity raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        char dst[1];
        memset(dst, 'x', sizeof(dst));
        if (!try()) {
            dsParseQuotedLimStringDirect(&in, dst, 0, NULL);
            test_validate(false, "must raise error for zero capacity");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseQuotedLimStringBuffer (буферизованный режим) -------------------------
static TestStatus
tf19_ds_parse_quoted_lim_string_buffer(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Успех: простая строка, буфер достаточного размера */
    test_sub("subtest %d: simple string, buffer enough", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        char dst[10];
        memset(dst, 'x', sizeof(dst));   // заполняем ненулевыми для проверки терминатора
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 5,
                          (dsFree(&in)),
                          "expected success and out_len=5, got res=%d out_len=%zu", res, out_len);
        test_validatefree(strcmp(dst, "hello") == 0,
                          (dsFree(&in)),
                          "content mismatch: got '%s'", dst);
        test_validatefree(in.pos == strlen(input),
                          (dsFree(&in)),
                          "in.pos expected %zu, got %zu", strlen(input), in.pos);

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 2. Успех: пустая строка */
    test_sub("subtest %d: empty string", ++subnum);
    {
        const char *input = "\"\"";
        DS in = dsCreateconst(input);
        char dst[5];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 0,
                          (dsFree(&in)),
                          "expected success and out_len=0, got res=%d out_len=%zu", res, out_len);
        test_validatefree(strcmp(dst, "") == 0,
                          (dsFree(&in)),
                          "expected empty, got '%s'", dst);

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 3. Успех: escape-последовательности */
    test_sub("subtest %d: escaped string", ++subnum);
    {
        const char *input = "\"a\\\"b\\\\c\\nd\\te\\rf\"";
        DS in = dsCreateconst(input);
        char dst[20];
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 11,
                          (dsFree(&in)),
                          "expected out_len=11, got res=%d out_len=%zu", res, out_len);
        test_validatefree(strcmp(dst, "a\"b\\c\nd\te\rf") == 0,
                          (dsFree(&in)),
                          "content mismatch: got '%s'", dst);

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 4. Успех: граничный размер — строка длиной capacity-1 */
    test_sub("subtest %d: exact fit (len == capacity-1)", ++subnum);
    {
        const char *input = "\"hello\"";   // 5 символов
        DS in = dsCreateconst(input);
        char dst[6];                        // capacity = 6, значит строка может быть 5 символов
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 5,
                          (dsFree(&in)),
                          "expected success and out_len=5, got res=%d out_len=%zu", res, out_len);
        test_validatefree(strcmp(dst, "hello") == 0,
                          (dsFree(&in)),
                          "content mismatch: got '%s'", dst);

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 5. Ошибка: переполнение (capacity слишком мала) и dst не изменён */
    test_sub("subtest %d: overflow, dst unchanged", ++subnum);
    {
        const char *input = "\"hello\"";   // требует capacity >= 6
        DS in = dsCreateconst(input);
        char dst[5];                        // capacity = 5, строка "hello" не влезет (нужно 6)
        memset(dst, 'y', sizeof(dst));      // заполняем ненулевыми
        size_t out_len = 0;
        size_t saved_pos = in.pos;
        char saved_dst[5];
        memcpy(saved_dst, dst, sizeof(dst));

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(!res,
                          (dsFree(&in)),
                          "expected false, got true");
        test_validatefree(memcmp(dst, saved_dst, sizeof(dst)) == 0,
                          (dsFree(&in)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in)),
                          "in.pos not restored");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 6. Ошибка: нет открывающей кавычки, dst не изменён */
    test_sub("subtest %d: missing begin, dst unchanged", ++subnum);
    {
        const char *input = "hello";
        DS in = dsCreateconst(input);
        char dst[10];
        memset(dst, 'z', sizeof(dst));
        size_t out_len = 0;
        size_t saved_pos = in.pos;
        char saved_dst[10];
        memcpy(saved_dst, dst, sizeof(dst));

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(!res,
                          (dsFree(&in)),
                          "expected false");
        test_validatefree(memcmp(dst, saved_dst, sizeof(dst)) == 0,
                          (dsFree(&in)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in)),
                          "in.pos not restored");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 7. Ошибка: нет закрывающей кавычки, dst не изменён */
    test_sub("subtest %d: missing end, dst unchanged", ++subnum);
    {
        const char *input = "\"hello";
        DS in = dsCreateconst(input);
        char dst[10];
        memset(dst, 'z', sizeof(dst));
        size_t out_len = 0;
        size_t saved_pos = in.pos;
        char saved_dst[10];
        memcpy(saved_dst, dst, sizeof(dst));

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(!res,
                          (dsFree(&in)),
                          "expected false");
        test_validatefree(memcmp(dst, saved_dst, sizeof(dst)) == 0,
                          (dsFree(&in)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in)),
                          "in.pos not restored");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 8. Ошибка: некорректный escape, dst не изменён */
    test_sub("subtest %d: invalid escape, dst unchanged", ++subnum);
    {
        const char *input = "\"\\x\"";
        DS in = dsCreateconst(input);
        char dst[10];
        memset(dst, 'z', sizeof(dst));
        size_t out_len = 0;
        size_t saved_pos = in.pos;
        char saved_dst[10];
        memcpy(saved_dst, dst, sizeof(dst));

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(!res,
                          (dsFree(&in)),
                          "expected false");
        test_validatefree(memcmp(dst, saved_dst, sizeof(dst)) == 0,
                          (dsFree(&in)),
                          "dst must remain unchanged");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in)),
                          "in.pos not restored");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 9. Успех: длинная строка в пределах capacity */
    test_sub("subtest %d: long string", ++subnum);
    {
        char longstr[101];
        memset(longstr, 'a', 100);
        longstr[100] = '\0';
        char input[105];
        snprintf(input, sizeof(input), "\"%s\"", longstr);

        DS in = dsCreateconst(input);
        char dst[102];   // 101 символ + нуль
        memset(dst, 'x', sizeof(dst));
        size_t out_len = 0;

        bool res = dsParseQuotedLimStringBuffer(&in, dst, sizeof(dst), &out_len);
        test_validatefree(res && out_len == 100,
                          (dsFree(&in)),
                          "expected success and out_len=100, got res=%d out_len=%zu", res, out_len);
        test_validatefree(strcmp(dst, longstr) == 0,
                          (dsFree(&in)),
                          "content mismatch");

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 10. Ошибка: capacity = 0 */
    test_sub("subtest %d: zero capacity raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        char dst[1] = {0};
        if (!try()) {
            dsParseQuotedLimStringBuffer(&in, dst, 0, NULL);
            test_validate(false, "must raise error for zero capacity");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 11. NULL in */
    test_sub("subtest %d: NULL in raises error", ++subnum);
    {
        char dst[10];
        memset(dst, 'x', sizeof(dst));
        if (!try()) {
            dsParseQuotedLimStringBuffer(NULL, dst, sizeof(dst), NULL);
            test_validate(false, "must raise error for NULL in");
        } else {
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 12. NULL dst */
    test_sub("subtest %d: NULL dst raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        if (!try()) {
            dsParseQuotedLimStringBuffer(&in, NULL, 10, NULL);
            test_validate(false, "must raise error for NULL dst");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseQuotedLimfsDirect (прямой режим для fs) -------------------------
static TestStatus
tf20_ds_parse_quoted_limfs_direct(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Успех: простая строка, maxlen=0 (безлимит) */
    test_sub("subtest %d: simple string, unlimited", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&in, &dst, 0);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 5, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("hello")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Успех: пустая строка */
    test_sub("subtest %d: empty string", ++subnum);
    {
        const char *input = "\"\"";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&in, &dst, 0);
        test_validatefree(res && dst.len == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 0, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty, got '%s'", fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Успех: escape-последовательности */
    test_sub("subtest %d: escaped string", ++subnum);
    {
        const char *input = "\"a\\\"b\\\\c\\nd\\te\\rf\"";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&in, &dst, 0);
        test_validatefree(res && dst.len == 11,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 11, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("a\"b\\c\nd\te\rf")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Успех: ограничение maxlen = 6 (ровно для "hello") */
    test_sub("subtest %d: maxlen == len+1", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseQuotedLimfsDirect(&in, &dst, 6);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len 5, got %zu", dst.len);
        test_validatefree(fscmp(dst, FSLITERAL("hello")) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Ошибка: превышение maxlen, dst становится пустым */
    test_sub("subtest %d: maxlen exceeded, dst reset", ++subnum);
    {
        const char *input = "\"hello\"";
        DS in = dsCreateconst(input);
        fs dst = FS();
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsDirect(&in, &dst, 3);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false, got true");
        test_validatefree(fs_isnull(&dst),
                          (dsFree(&in), fsfree(dst)),
                          "dst should be reset after error (use_buffer=false)");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Ошибка: нет открывающей кавычки, dst становится пустым */
    test_sub("subtest %d: missing begin, dst reset", ++subnum);
    {
        const char *input = "hello";
        DS in = dsCreateconst(input);
        fs dst = FS();
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsDirect(&in, &dst, 0);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(fs_isnull(&dst),
                          (dsFree(&in), fsfree(dst)),
                          "dst should be reset after error");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Ошибка: нет закрывающей кавычки, dst становится пустым */
    test_sub("subtest %d: missing end, dst reset", ++subnum);
    {
        const char *input = "\"hello";
        DS in = dsCreateconst(input);
        fs dst = FS();
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsDirect(&in, &dst, 0);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(fs_isnull(&dst),
                          (dsFree(&in), fsfree(dst)),
                          "dst should be reset after error");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 8. Ошибка: некорректный escape, dst становится пустым */
    test_sub("subtest %d: invalid escape, dst reset", ++subnum);
    {
        const char *input = "\"\\x\"";
        DS in = dsCreateconst(input);
        fs dst = FS();
        size_t saved_pos = in.pos;

        bool res = dsParseQuotedLimfsDirect(&in, &dst, 0);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "expected false");
        test_validatefree(fs_isnull(&dst),
                          (dsFree(&in), fsfree(dst)),
                          "dst should be reset after error");
        test_validatefree(in.pos == saved_pos,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos not restored");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 9. NULL in */
    test_sub("subtest %d: NULL in raises error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            dsParseQuotedLimfsDirect(NULL, &dst, 0);
            test_validatefree(false, fsfree(dst), "must raise error");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 10. NULL dst */
    test_sub("subtest %d: NULL dst raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        if (!try()) {
            dsParseQuotedLimfsDirect(&in, NULL, 0);
            test_validate(false, "must raise error");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 11. Неаллоцируемый dst (например, FSLITERAL) */
    test_sub("subtest %d: non-allocatable dst raises error", ++subnum);
    {
        DS in = dsCreateconst("\"test\"");
        fs dst = FSLITERAL("original");
        if (!try()) {
            dsParseQuotedLimfsDirect(&in, &dst, 0);
            test_validate(false, "must raise error for non-allocatable dst");
        } else {
            test_validate(true, "correctly raised error");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseUnlimfsDirect (direct mode, w/o escape) -------------------------
static TestStatus
tf21_ds_parse_unlimfs_direct(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Успешное чтение строки, оканчивающейся '\n' */
    test_sub("subtest %d: simple line with newline", ++subnum);
    {
        const char *input = "hello\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsDirect(&in, &dst);

        test_validatefree(res && dst.len == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "expected success and len=%zu, got res=%d len=%zu", strlen(input), res, dst.len);
        test_validatefree(fscmpstr(dst, input) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected %zu, got %lld", strlen(input), (long long)dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Успешное чтение последней строки без '\n' (EOF) */
    test_sub("subtest %d: line at EOF without newline", ++subnum);
    {
        const char *input = "world";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsDirect(&in, &dst);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected success and len=5, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, "world") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected %zu, got %lld", strlen(input), (long long)dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Пустая строка (только '\n') */
    test_sub("subtest %d: empty line (just newline)", ++subnum);
    {
        const char *input = "\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsDirect(&in, &dst);
        test_validatefree(res && dst.len == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "expected success and \\n, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, input) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty string");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Пустой вход (EOF сразу) */
    test_sub("subtest %d: empty input (EOF)", ++subnum);
    {
        const char *input = "";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsDirect(&in, &dst);
        test_validatefree(res && dst.len == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected success and empty, got res=%d len=%zu", res, dst.len);

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Строка, состоящая только из пробелов (до '\n') */
    test_sub("subtest %d: line with spaces", ++subnum);
    {
        const char *input = "   \t  \n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsDirect(&in, &dst);
        test_validatefree(res && dst.len == strlen(input),   // три пробела, таб, два пробела
                          (dsFree(&in), fsfree(dst)),
                          "expected len=6, got %zu", dst.len);
        test_validatefree(fscmpstr(dst, input) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Длинная строка (проверка автоматического расширения fs) */
    test_sub("subtest %d: long line", ++subnum);
    {
        char longstr[256];
        memset(longstr, 'A', 255);
        longstr[255] = '\0';
        char input[260];
        snprintf(input, sizeof(input), "%s\n", longstr);

        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsDirect(&in, &dst);
        test_validatefree(res && dst.len == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "expected len=%zu, got %zu", strlen(input), dst.len);
        test_validatefree(strncmp(fsstr(dst), input, strlen(input)) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch '%s'", fsstr(dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Несколько строк подряд (повторные вызовы) */
    test_sub("subtest %d: multiple lines sequentially", ++subnum);
    {
        const char *input = "first\nsecond\nthird";
        DS in = dsCreateconst(input);

        fs dst1 = FS();
        bool r1 = dsParseUnlimfsDirect(&in, &dst1);
        test_validatefree(r1 && fscmpstr(dst1, "first\n") == 0,
                          (dsFree(&in), fsfree(dst1)),
                          "first line mismatch '%s", fsstr(dst1) );

        fs dst2 = FS();
        bool r2 = dsParseUnlimfsDirect(&in, &dst2);
        test_validatefree(r2 && fscmpstr(dst2, "second\n") == 0,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2)),
                          "second line mismatch");

        fs dst3 = FS();
        bool r3 = dsParseUnlimfsDirect(&in, &dst3);
        test_validatefree(r3 && fscmpstr(dst3, "third") == 0,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2), fsfree(dst3)),
                          "third line mismatch");

        dsFree(&in);
        fsfree(dst1);
        fsfree(dst2);
        fsfree(dst3);
        fs_alloc_check(true);
    }

    /* 8. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            dsParseUnlimfsDirect(NULL, &dst);
            test_validatefree(false, fsfree(dst), "must raise error for NULL in");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error for NULL in");
        }

        DS in = dsCreateconst("test");
        if (!try()) {
            dsParseUnlimfsDirect(&in, NULL);
            test_validate(false, "must raise error for NULL dst");
        } else {
            test_validate(true, "correctly raised error for NULL dst");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 9. Неаллоцируемый dst (FSLITERAL) */
    test_sub("subtest %d: non-allocatable dst raises error", ++subnum);
    {
        DS in = dsCreateconst("test\n");
        fs dst = FSLITERAL("initial");
        if (!try()) {
            dsParseUnlimfsDirect(&in, &dst);
            test_validate(false, "must raise error for non-allocatable dst");
        } else {
            test_validate(true, "correctly raised error for non-allocatable dst");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseUnlimfsBuffer (buffer mode, w/o escape) -------------------------
static TestStatus
tf22_ds_parse_unlimfs_buffer(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Успешное чтение строки, оканчивающейся '\n' */
    test_sub("subtest %d: simple line with newline", ++subnum);
    {
        const char *input = "hello\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsBuffer(&in, &dst);

        test_validatefree(res && dst.len == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "expected success and len=%zu, got res=%d len=%zu", strlen(input), res, dst.len);
        test_validatefree(fscmpstr(dst, input) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected %zu, got %lld", strlen(input), (long long)dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Успешное чтение последней строки без '\n' (EOF) */
    test_sub("subtest %d: line at EOF without newline", ++subnum);
    {
        const char *input = "world";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsBuffer(&in, &dst);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected success and len=5, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, "world") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected %zu, got %lld", strlen(input), (long long)dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Пустая строка (только '\n') */
    test_sub("subtest %d: empty line (just newline)", ++subnum);
    {
        const char *input = "\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsBuffer(&in, &dst);
        test_validatefree(res && dst.len == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "expected success and \\n, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, input) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected empty string");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Пустой вход (EOF сразу) */
    test_sub("subtest %d: empty input (EOF)", ++subnum);
    {
        const char *input = "";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsBuffer(&in, &dst);
        test_validatefree(res && dst.len == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected success and empty, got res=%d len=%zu", res, dst.len);

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Строка, состоящая только из пробелов (до '\n') */
    test_sub("subtest %d: line with spaces", ++subnum);
    {
        const char *input = "   \t  \n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsBuffer(&in, &dst);
        test_validatefree(res && dst.len == strlen(input),   // три пробела, таб, два пробела
                          (dsFree(&in), fsfree(dst)),
                          "expected len=6, got %zu", dst.len);
        test_validatefree(fscmpstr(dst, input) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Длинная строка (проверка автоматического расширения fs) */
    test_sub("subtest %d: long line", ++subnum);
    {
        char longstr[256];
        memset(longstr, 'A', 255);
        longstr[255] = '\0';
        char input[260];
        snprintf(input, sizeof(input), "%s\n", longstr);

        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseUnlimfsBuffer(&in, &dst);
        test_validatefree(res && dst.len == strlen(input),
                          (dsFree(&in), fsfree(dst)),
                          "expected len=%zu, got %zu", strlen(input), dst.len);
        test_validatefree(strncmp(fsstr(dst), input, strlen(input)) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch '%s'", fsstr(dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Несколько строк подряд (повторные вызовы) */
    test_sub("subtest %d: multiple lines sequentially", ++subnum);
    {
        const char *input = "first\nsecond\nthird";
        DS in = dsCreateconst(input);

        fs dst1 = FS();
        bool r1 = dsParseUnlimfsBuffer(&in, &dst1);
        test_validatefree(r1 && fscmpstr(dst1, "first\n") == 0,
                          (dsFree(&in), fsfree(dst1)),
                          "first line mismatch '%s", fsstr(dst1) );

        fs dst2 = FS();
        bool r2 = dsParseUnlimfsBuffer(&in, &dst2);
        test_validatefree(r2 && fscmpstr(dst2, "second\n") == 0,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2)),
                          "second line mismatch");

        fs dst3 = FS();
        bool r3 = dsParseUnlimfsBuffer(&in, &dst3);
        test_validatefree(r3 && fscmpstr(dst3, "third") == 0,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2), fsfree(dst3)),
                          "third line mismatch");

        dsFree(&in);
        fsfree(dst1);
        fsfree(dst2);
        fsfree(dst3);
        fs_alloc_check(true);
    }

    /* 8. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            dsParseUnlimfsBuffer(NULL, &dst);
            test_validatefree(false, fsfree(dst), "must raise error for NULL in");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error for NULL in");
        }

        DS in = dsCreateconst("test");
        if (!try()) {
            dsParseUnlimfsBuffer(&in, NULL);
            test_validate(false, "must raise error for NULL dst");
        } else {
            test_validate(true, "correctly raised error for NULL dst");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 9. Неаллоцируемый dst (FSLITERAL) */
    test_sub("subtest %d: non-allocatable dst raises error", ++subnum);
    {
        DS in = dsCreateconst("test\n");
        fs dst = FSLITERAL("initial");
        if (!try()) {
            dsParseUnlimfsBuffer(&in, &dst);
            test_validate(false, "must raise error for non-allocatable dst");
        } else {
            test_validate(true, "correctly raised error for non-allocatable dst");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseWordDirect (direct mode, word parsing) -------------------------
static TestStatus
tf23_ds_parse_word_direct(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простое слово до EOF */
    test_sub("subtest %d: simple word at EOF", ++subnum);
    {
        const char *input = "hello";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);

        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected success len=5, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 5,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 5, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Слово, ограниченное '\n' — '\n' НЕ должен попасть в результат и должен остаться в потоке */
    test_sub("subtest %d: word before newline", ++subnum);
    {
        const char *input = "hello\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len=5, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 5,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 5 (before \\n, ungetc), got %lld",
                          (long long) dsGetpos(&in));

        /* Проверим, что '\n' реально остался в потоке */
        int c = dsgetc(&in);
        test_validatefree(c == '\n',
                          (dsFree(&in), fsfree(dst)),
                          "expected '\\n' still in stream, got %d", c);

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Слово после ведущих пробелов — пробелы НЕ должны попасть в результат */
    test_sub("subtest %d: leading spaces skipped", ++subnum);
    {
        const char *input = "   \t  hello\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len=5 (no leading spaces), got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Слово, ограниченное пробелом (пробел остаётся в потоке) */
    test_sub("subtest %d: word before space", ++subnum);
    {
        const char *input = "hello world";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected 'hello', got res=%d '%s'", res, fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 5,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 5 (before space), got %lld",
                          (long long) dsGetpos(&in));
        test_validatefree(dsgetc(&in) == ' ',
                          (dsFree(&in), fsfree(dst)),
                          "space should remain in stream");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Слово с цифрами и подчёркиванием (внутри слова) */
    test_sub("subtest %d: word with digits and underscore inside", ++subnum);
    {
        const char *input = "abc_123\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "abc_123") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected 'abc_123', got res=%d '%s'", res, fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Слово, начинающееся с цифры/подчёркивания.
     * Если isalnum_u их принимает как старт — ok. Если нет — тест надо убрать/поправить. */
    test_sub("subtest %d: word starting with digit", ++subnum);
    {
        const char *input = "123abc\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "123abc") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected '123abc', got res=%d '%s'", res, fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Слово, заканчивающееся на не-идентификатор: '-' должен остаться в потоке */
    test_sub("subtest %d: word terminated by non-alnum", ++subnum);
    {
        const char *input = "abc-def";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "abc") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected 'abc', got res=%d '%s'", res, fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 3,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 3 (before '-'), got %lld",
                          (long long) dsGetpos(&in));
        test_validatefree(dsgetc(&in) == '-',
                          (dsFree(&in), fsfree(dst)),
                          "'-' should remain in stream");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 8. Длинное слово — проверка авторасширения fs */
    test_sub("subtest %d: long word", ++subnum);
    {
        char input[300];
        memset(input, 'a', 255);
        input[255] = '\n';
        input[256] = '\0';

        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && dst.len == 255,
                          (dsFree(&in), fsfree(dst)),
                          "expected len=255, got res=%d len=%zu", res, dst.len);
        test_validatefree( (size_t) dsGetpos(&in) == 255,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 255, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 9. Несколько слов последовательно */
    test_sub("subtest %d: multiple words sequentially", ++subnum);
    {
        const char *input = "foo bar_baz 42qux";
        DS in = dsCreateconst(input);

        fs dst1 = FS();
        bool r1 = dsParseWordDirect(&in, &dst1);
        test_validatefree(r1 && fscmpstr(dst1, "foo") == 0,
                          (dsFree(&in), fsfree(dst1)),
                          "word1 mismatch: got res=%d '%s'", r1, fs_str(&dst1));

        fs dst2 = FS();
        bool r2 = dsParseWordDirect(&in, &dst2);
        test_validatefree(r2 && fscmpstr(dst2, "bar_baz") == 0,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2)),
                          "word2 mismatch: got res=%d '%s'", r2, fs_str(&dst2));

        fs dst3 = FS();
        bool r3 = dsParseWordDirect(&in, &dst3);
        test_validatefree(r3 && fscmpstr(dst3, "42qux") == 0,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2), fsfree(dst3)),
                          "word3 mismatch: got res=%d '%s'", r3, fs_str(&dst3));

        /* после последнего слова — EOF */
        fs dst4 = FS();
        if (!try()) {
            dsParseWordDirect(&in, &dst4);
            test_validatefree(false, (dsFree(&in), fsfree(dst1), fsfree(dst2), fsfree(dst3), fsfree(dst4)),
                              "expected error at EOF after last word");
        } else {
            test_validatefree(true, (dsFree(&in), fsfree(dst1), fsfree(dst2), fsfree(dst3), fsfree(dst4)),
                              "correctly raised error at EOF");
        }

        dsFree(&in);
        fsfree(dst1);
        fsfree(dst2);
        fsfree(dst3);
        fs_alloc_check(true);
    }

        /* 7b. Слово, начинающееся с подчёркивания */
    test_sub("subtest %d: word starting with underscore", ++subnum);
    {
        const char *input = "_foo\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "_foo") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected '_foo', got res=%d '%s'", res, fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7c. Слово из одних подчёркиваний */
    test_sub("subtest %d: word of underscores only", ++subnum);
    {
        const char *input = "___\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "___") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected '___', got res=%d '%s'", res, fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7d. Слово, начинающееся с подчёркивания, с цифрами/буквами */
    test_sub("subtest %d: _1a_2 word", ++subnum);
    {
        const char *input = "_1a_2 rest";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "_1a_2") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected '_1a_2', got res=%d '%s'", res, fs_str(&dst));
        /* разделитель остался в потоке */
        test_validatefree( (size_t) dsGetpos(&in) == 5,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 5, got %lld", (long long) dsGetpos(&in));
        test_validatefree(dsgetc(&in) == ' ',
                          (dsFree(&in), fsfree(dst)),
                          "space should remain in stream");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 10. Пустой вход — ошибка, позиция откатывается */
    test_sub("subtest %d: empty input -> error, pos rolled back", ++subnum);
    {
        const char *input = "";
        DS in = dsCreateconst(input);
        fs dst = FS();

        if (!try()) {
            dsParseWordDirect(&in, &dst);
            test_validatefree(false, (dsFree(&in), fsfree(dst)),
                              "must raise error for empty input");
        } else {
            test_validatefree( (size_t) dsGetpos(&in) == 0,
                              (dsFree(&in), fsfree(dst)),
                              "pos must rollback to 0, got %lld", (long long) dsGetpos(&in));
        }

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 11. Только пробелы — ошибка, позиция откатывается */
    test_sub("subtest %d: only spaces -> error, pos rolled back", ++subnum);
    {
        const char *input = "     \t  ";
        DS in = dsCreateconst(input);
        fs dst = FS();

        if (!try()) {
            dsParseWordDirect(&in, &dst);
            test_validatefree(false, (dsFree(&in), fsfree(dst)),
                              "must raise error for whitespace-only input");
        } else {
            test_validatefree( (size_t) dsGetpos(&in) == 0,
                              (dsFree(&in), fsfree(dst)),
                              "pos must rollback to 0, got %lld", (long long) dsGetpos(&in));
        }

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 12. Невалидный первый символ — ошибка, позиция откатывается */
    test_sub("subtest %d: invalid first symbol -> error, pos rolled back", ++subnum);
    {
        const char *input = "!hello";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(
            !res, 
            (dsFree(&in), fsfree(dst)),
             "must not be parsed");
        test_validatefree( (size_t) dsGetpos(&in) == 0,
                              (dsFree(&in), fsfree(dst)),
                              "pos must rollback to 0, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 13. Невалидный символ после ведущих пробелов — тоже ошибка + rollback */
    test_sub("subtest %d: spaces then invalid symbol -> error, pos rolled back", ++subnum);
    {
        const char *input = "   !hello";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordDirect(&in, &dst);
        test_validatefree(
            !res, 
            (dsFree(&in), fsfree(dst)),
            "must not be parsed"
        );
        test_validatefree( (size_t) dsGetpos(&in) == 0,
                              (dsFree(&in), fsfree(dst)),
                              "pos must rollback to 0, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 14. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            dsParseWordDirect(NULL, &dst);
            test_validatefree(false, fsfree(dst), "must raise error for NULL in");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error for NULL in");
        }

        DS in = dsCreateconst("test");
        if (!try()) {
            dsParseWordDirect(&in, NULL);
            test_validate(false, "must raise error for NULL dst");
        } else {
            test_validate(true, "correctly raised error for NULL dst");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 15. Неаллоцируемый dst (FSLITERAL) */
    test_sub("subtest %d: non-allocatable dst raises error", ++subnum);
    {
        DS in = dsCreateconst("hello\n");
        fs dst = FSLITERAL("initial");
        if (!try()) {
            dsParseWordDirect(&in, &dst);
            test_validate(false, "must raise error for non-allocatable dst");
        } else {
            test_validate(true, "correctly raised error for non-allocatable dst");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsParseWordBuffer (buffer mode, word parsing) -------------------------
static TestStatus
tf24_ds_parse_word_buffer(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Простое слово до EOF */
    test_sub("subtest %d: simple word at EOF", ++subnum);
    {
        const char *input = "hello";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);

        DSTECHFPRINT(logfile, in);
        fstechfprint(logfile, dst);

        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected success len=5, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 5,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 5, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 2. Слово перед '\n' — '\n' остаётся в потоке */
    test_sub("subtest %d: word before newline", ++subnum);
    {
        const char *input = "hello\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len=5, got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 5,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 5, got %lld", (long long) dsGetpos(&in));

        int c = dsgetc(&in);
        test_validatefree(c == '\n',
                          (dsFree(&in), fsfree(dst)),
                          "expected '\\n' still in stream, got %d", c);

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 3. Ведущие пробелы не попадают в результат */
    test_sub("subtest %d: leading spaces skipped", ++subnum);
    {
        const char *input = "   \t  hello\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && dst.len == 5,
                          (dsFree(&in), fsfree(dst)),
                          "expected len=5 (no leading spaces), got res=%d len=%zu", res, dst.len);
        test_validatefree(fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "content mismatch: got '%s'", fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 4. Слово перед пробелом — пробел остаётся в потоке */
    test_sub("subtest %d: word before space", ++subnum);
    {
        const char *input = "hello world";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected 'hello', got res=%d '%s'", res, fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 5,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 5, got %lld", (long long) dsGetpos(&in));
        test_validatefree(dsgetc(&in) == ' ',
                          (dsFree(&in), fsfree(dst)),
                          "space should remain in stream");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 5. Слово с цифрами и подчёркиванием внутри */
    test_sub("subtest %d: word with digits and underscore inside", ++subnum);
    {
        const char *input = "abc_123\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "abc_123") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected 'abc_123', got res=%d '%s'", res, fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 6. Слово, начинающееся с цифры */
    test_sub("subtest %d: word starting with digit", ++subnum);
    {
        const char *input = "123abc\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "123abc") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected '123abc', got res=%d '%s'", res, fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 7. Слово, начинающееся с подчёркивания */
    test_sub("subtest %d: word starting with underscore", ++subnum);
    {
        const char *input = "_foo\n";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "_foo") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected '_foo', got res=%d '%s'", res, fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 8. Слово, ограниченное не-идентификатором: '-' остаётся в потоке */
    test_sub("subtest %d: word terminated by non-alnum", ++subnum);
    {
        const char *input = "abc-def";
        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "abc") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected 'abc', got res=%d '%s'", res, fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 3,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 3, got %lld", (long long) dsGetpos(&in));
        test_validatefree(dsgetc(&in) == '-',
                          (dsFree(&in), fsfree(dst)),
                          "'-' should remain in stream");

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 9. Длинное слово — авторасширение fs */
    test_sub("subtest %d: long word", ++subnum);
    {
        char input[300];
        memset(input, 'a', 255);
        input[255] = '\n';
        input[256] = '\0';

        DS in = dsCreateconst(input);
        fs dst = FS();

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && dst.len == 255,
                          (dsFree(&in), fsfree(dst)),
                          "expected len=255, got res=%d len=%zu", res, dst.len);
        test_validatefree( (size_t) dsGetpos(&in) == 255,
                          (dsFree(&in), fsfree(dst)),
                          "in.pos expected 255, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 10. Несколько слов последовательно */
    test_sub("subtest %d: multiple words sequentially", ++subnum);
    {
        const char *input = "foo bar_baz 42qux";
        DS in = dsCreateconst(input);

        fs dst1 = FS();
        bool r1 = dsParseWordBuffer(&in, &dst1);
        test_validatefree(r1 && fscmpstr(dst1, "foo") == 0,
                          (dsFree(&in), fsfree(dst1)),
                          "word1 mismatch: got res=%d '%s'", r1, fs_str(&dst1));

        fs dst2 = FS();
        bool r2 = dsParseWordBuffer(&in, &dst2);
        test_validatefree(r2 && fscmpstr(dst2, "bar_baz") == 0,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2)),
                          "word2 mismatch: got res=%d '%s'", r2, fs_str(&dst2));

        fs dst3 = FS();
        bool r3 = dsParseWordBuffer(&in, &dst3);
        test_validatefree(r3 && fscmpstr(dst3, "42qux") == 0,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2), fsfree(dst3)),
                          "word3 mismatch: got res=%d '%s'", r3, fs_str(&dst3));

        /* EOF после последнего слова — на buffer-режиме тоже возвращает false (не raise) */
        fs dst4 = FS();
        bool r4 = dsParseWordBuffer(&in, &dst4);
        test_validatefree(!r4,
                          (dsFree(&in), fsfree(dst1), fsfree(dst2), fsfree(dst3), fsfree(dst4)),
                          "expected failure at EOF after last word");

        dsFree(&in);
        fsfree(dst1);
        fsfree(dst2);
        fsfree(dst3);
        fsfree(dst4);
        fs_alloc_check(true);
    }

    /* 11. Пустой вход — ошибка, содержимое dst сохранено (buffer-инвариант) */
    test_sub("subtest %d: empty input -> error, dst content preserved", ++subnum);
    {
        const char *input = "";
        DS in = dsCreateconst(input);

        fs dst = fscopy("SENTINEL");
        test_validatefree(dst.v != NULL && fscmpstr(dst, "SENTINEL") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "setup: fscopy failed");

        size_t saved_len = dst.len;
        size_t saved_sz  = dst.sz;

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "must not parse empty input");
        test_validatefree(dst.len == saved_len && fscmpstr(dst, "SENTINEL") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst content must be preserved, got len=%zu '%s'",
                          dst.len, fs_str(&dst));
        test_validatefree(dst.sz == saved_sz,
                          (dsFree(&in), fsfree(dst)),
                          "dst capacity must not change (%zu -> %zu)", saved_sz, dst.sz);
        test_validatefree( (size_t) dsGetpos(&in) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "pos must rollback to 0, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 12. Только пробелы — ошибка, dst (свежий) остался пустым */
    test_sub("subtest %d: only spaces -> error, fresh dst stays empty", ++subnum);
    {
        const char *input = "     \t  ";
        DS in = dsCreateconst(input);

        fs dst = FS();       // свежий — проверяем, что buffer-режим его не аллоцирует зря

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "must not parse whitespace-only input");
        test_validatefree(dst.len == 0,
                          (dsFree(&in), fsfree(dst)),
                          "fresh dst must remain empty, got len=%zu", dst.len);
        test_validatefree( (size_t) dsGetpos(&in) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "pos must rollback to 0, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 13. Невалидный первый символ — содержимое dst сохранено */
    test_sub("subtest %d: invalid first symbol -> error, dst content preserved", ++subnum);
    {
        const char *input = "!hello";
        DS in = dsCreateconst(input);

        fs dst = fscopy("SENTINEL");
        test_validatefree(dst.v != NULL,
                          (dsFree(&in), fsfree(dst)),
                          "setup: fscopy failed");

        size_t saved_len = dst.len;

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "must not parse invalid input");
        test_validatefree(dst.len == saved_len && fscmpstr(dst, "SENTINEL") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst content must be preserved, got len=%zu '%s'",
                          dst.len, fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "pos must rollback to 0, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 14. Пробелы, потом невалидный символ — dst content preserved */
    test_sub("subtest %d: spaces then invalid -> error, dst content preserved", ++subnum);
    {
        const char *input = "   !hello";
        DS in = dsCreateconst(input);

        fs dst = fscopy("SENTINEL");
        test_validatefree(dst.v != NULL,
                          (dsFree(&in), fsfree(dst)),
                          "setup: fscopy failed");

        size_t saved_len = dst.len;

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "must not parse");
        test_validatefree(dst.len == saved_len && fscmpstr(dst, "SENTINEL") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "dst content must be preserved, got len=%zu '%s'",
                          dst.len, fs_str(&dst));
        test_validatefree( (size_t) dsGetpos(&in) == 0,
                          (dsFree(&in), fsfree(dst)),
                          "pos must rollback to 0, got %lld", (long long) dsGetpos(&in));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 15. dst с длинным содержимым — проверка, что не обрезан и не изменён */
    test_sub("subtest %d: long sentinel preserved on error", ++subnum);
    {
        const char *input = "@not a word";
        DS in = dsCreateconst(input);

        fs dst = fscopy("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
        test_validatefree(dst.v != NULL && dst.len == 40,
                          (dsFree(&in), fsfree(dst)),
                          "setup: fscopy failed");

        size_t saved_len = dst.len;
        size_t saved_sz  = dst.sz;

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(!res,
                          (dsFree(&in), fsfree(dst)),
                          "must not parse");
        test_validatefree(dst.len == saved_len && fscmpstr(dst, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "long dst must be preserved, got len=%zu", dst.len);
        test_validatefree(dst.sz == saved_sz,
                          (dsFree(&in), fsfree(dst)),
                          "dst capacity must not change (%zu -> %zu)", saved_sz, dst.sz);

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 16. dst уже содержит данные — при успехе должны быть перезаписаны */
    test_sub("subtest %d: pre-filled dst replaced on success", ++subnum);
    {
        const char *input = "hello\n";
        DS in = dsCreateconst(input);

        fs dst = fscopy("OLD");

        bool res = dsParseWordBuffer(&in, &dst);
        test_validatefree(res && fscmpstr(dst, "hello") == 0,
                          (dsFree(&in), fsfree(dst)),
                          "expected 'hello' after success, got res=%d '%s'", res, fs_str(&dst));

        dsFree(&in);
        fsfree(dst);
        fs_alloc_check(true);
    }

    /* 17. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            dsParseWordBuffer(NULL, &dst);
            test_validatefree(false, fsfree(dst), "must raise error for NULL in");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error for NULL in");
        }

        DS in = dsCreateconst("test");
        if (!try()) {
            dsParseWordBuffer(&in, NULL);
            test_validate(false, "must raise error for NULL dst");
        } else {
            test_validate(true, "correctly raised error for NULL dst");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    /* 18. Неаллоцируемый dst (FSLITERAL) */
    test_sub("subtest %d: non-allocatable dst raises error", ++subnum);
    {
        DS in = dsCreateconst("hello\n");
        fs dst = FSLITERAL("initial");
        if (!try()) {
            dsParseWordBuffer(&in, &dst);
            test_validate(false, "must raise error for non-allocatable dst");
        } else {
            test_validate(true, "correctly raised error for non-allocatable dst");
        }
        dsFree(&in);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// -------------------------------------------------------------------
int
main( /*int argc, char *argv[] */ )
{
    logsimpleinit("Start");

    testenginestd(
        TESTADD(tf_ds_printf,                               "dsPrintf() simple tests")
      , TESTADD(tf_ds_scanf,                                "dsScanf() simple tests")
      , TESTADD(tf_ds_scanf_printf,                         "dsScanf() and dsPrintf() combined tests")
      , TESTADD(tf_ds_parsers,                              "dsParse<type> simple tests")
      , TESTADD(tf5_fs_dstechprint,                         "fs_dstechprint simple test")
      , TESTADD(tf6_fs_dswrite,                             "fs_dswrite simple test")
      , TESTADD(tf7_fs_dsserialize_full,                    "fs_dsserialize full test (all edges)")
      , TESTADD(tf8_ds_parse_quoted_line,                   "dsParseQuotedLimfsDirect simple test")
      , TESTADD(tf9_fs_ds_DS_STR_roundtrip,                 "fs_dsserialize/fs_dsload() DS_STR round-trip test")
      , TESTADD(tf10_fs_ds_CONST_roundtrip,                 "fs_dsload() with DS_CONSTSTR round-trip and errors")
      , TESTADD(tf11_fs_ds_FS_roundtrip,                    "fs_dsload() with DS_FS round-trip and errors")
      , TESTADD(tf12_fs_ds_FILE_roundtrip,                  "fs_dsload() with DS_FILE round-trip and errors")
      , TESTADD(tf13_ds_release_fs,                         "dsReleaseFs() simple test")
      , TESTADD(tf15_ds_parse_quoted_core,                  "ds_parse_quoted_core() simple test")
        //
      , TESTADD(tf14_ds_parse_quoted_unlim,                 "dsParseQuotedUnlimfsDirect() simple test")
      , TESTADD(tf16_ds_parse_quoted_limfs_buffered,        "dsParseQuotedLimfs() with limit and buffer tests")
      , TESTADD(tf17_ds_parse_quoted_unlimfs_buffered,      "dsParseQuotedLimfs() unlimit and buffer tests")
      , TESTADD(tf18_ds_parse_quoted_lim_string_direct,     "dsParseQuotedLimStringDirect() tests")
      , TESTADD(tf19_ds_parse_quoted_lim_string_buffer,     "dsParseQuotedLimStringBuffer() tests")
      , TESTADD(tf20_ds_parse_quoted_limfs_direct,          "dsParseQuotedLimfsDirect tests")
      // normal str
      , TESTADD(tf21_ds_parse_unlimfs_direct,               "dsParseUnlimfsDirect() simple tests")
      , TESTADD(tf22_ds_parse_unlimfs_buffer,               "dsParseUnlimfsBuffer() simple tests")
      // word ([a-z], [A-Z], [0-9], _)
      , TESTADD(tf23_ds_parse_word_direct,                  "dsParseWordDirect() simple tests")
      , TESTADD(tf24_ds_parse_word_buffer,                  "dsParseWordBuffer() simple tests")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* DS_ADAPTER_TESTING */
