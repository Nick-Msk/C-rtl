
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

/**
 * @brief Internal helper to parse escaped sequences from a @ref DS stream.
 *
 * This function scans the input stream for a quoted string. It handles 
 * standard escape sequences:
 * - @code \n @endcode -> newline
 * - @code \r @endcode -> carriage return
 * - @code \t @endcode -> tab
 * - @code \\ @endcode -> backslash
 * - @code \" @endcode -> double quote
 *
 * @details
 * The function implements a transactional parsing approach:
 * <ol>
 *   <li>It saves the current position of the input stream.</li>
 *   <li>It parses the quoted content, decoding escape sequences.</li>
 *   <li>If a parsing error occurs (e.g., invalid escape, missing 
 *       closing quote, or buffer overflow), the input stream position 
 *       is restored to its original state via @ref dsRestorepos.</li>
 * </ol>
 *
 * @param[in]  in           Pointer to the source @ref DS stream.
 * @param[in,out] dst       Pointer to the destination character buffer.
 * @param[in]  dst_capacity The maximum number of characters the buffer 
 *                          can hold (excluding the null terminator).
 *
 * @return The number of decoded characters written to @p dst (excluding 
 *         the quotes), or 0 if a parsing error occurred.
 *
 * @note This function automatically adds a null terminator ('\0') at 
 *       @code dst[len] @endcode upon successful parsing.
 * @warning This function modifies the input stream position.
 */
typedef void (*extend_func) ();

static bool
dsHelperParseEscapedString(DS *restrict in, char *restrict dst, size_t dst_capacity, size_t *restrict out_len) {
    
    size_t      pos = dsSavepos(in);                       // запоминаем позицию
    bool        error = false;
    size_t      len = 0;

    if (dst_capacity == 0)
        return userraise(false, ERR_WRONG_INPUT_PARAMETERS, "capacity can't be 0");

    int c = dsgetc(in);
    if (c != '"')
        error = true;

    while (!error && (c = dsgetc(in)) != EOF && c != '"') {

        if (c == '\\') {
            if (!dsgetcEscaped(in, &c)) {
                error = true; // Ошибка, если после '\' ничего нет или неизвестный символ
                break;
            }
        }
        if (len + 1 >= dst_capacity) {
            error = true;             // never shoud be here if normal serialization 
            logsimple("WARN: len + 1 > dst_capacity (%zu)", dst_capacity);
        } else 
            dst[len++] = (unsigned char) c;
    }
    if (c != '"')
        error = true;

    if (out_len)
        *out_len = len;
    dst[len] = '\0';        // must setup \0 even of error

    if (error) {
        dsRestorepos(in, pos);                 // rollback only if error
        return userraise(false, ERR_UNABLE_PARSE_DATA, 
            "Unable to parse quoted line!");
    }            

    return true;
}

/*
 * Core engine for parsing quoted strings.
 * It reads from 'in' and writes processed characters to 'out'.
 */
static bool                     
ds_parse_quoted_core(DS *restrict in, DS *restrict out, size_t maxlen, unsigned char begin, unsigned char end) {
    invraisecode(in != NULL && out != NULL, ERR_NULLABLE_PTR, 
        "Null pointers %p %p", in, out);

    bool        error = false;
    size_t      pos = dsSavepos(in);

    int c = dsgetc(in);
    if (c != begin)
        error = true;
    
    while (!error && (c = dsgetc(in)) != EOF && (unsigned char) c != end) {

        if (c == '\\') {
            if (!dsgetcEscaped(in, &c)) {
                error = true; // Ошибка, если после '\' ничего нет или неизвестный символ
                break;
            }
        }
        if (maxlen > 0L && out->pos + 1 >= maxlen) { // if maxlen == 0 - UNLIM
            error = true;             // never shoud be here if normal serialization 
            logsimple("WARN: len (%zu) + 1 > dst_capacity (%zu)", out->pos, maxlen);
        } else if (dsputc(c, out) < 0) {
            userraise(false, ERR_STREAM_ERROR, "out ds stream error!");
            error = true;
        }
    }
    if (!error && c != end)
        error = true;

    if (error) {
        dsRestorepos(in, pos);                 // rollback only if error
        return userraise(false, ERR_UNABLE_PARSE_DATA, 
            "Unable to parse quoted line!");
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
 */static long                     
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

// --------------------------- API ---------------------------------

int                         dsPrintf(DS *restrict pds, const char *restrict msg, ...) {
    invraisecode(pds != NULL && msg != NULL, ERR_NULLABLE_PTR, 
        "Null input %p %p", pds, msg);

    va_list     ap;
    int         total = 0;
    va_start(ap, msg);
    switch (pds->type) {
        case DS_FILE:
            IOCHECKER(wr, vfprintf(pds->fp, msg, ap), -1)
                total += wr;
            break;
        case DS_STR: // this is NOT autoextendable, till end of pds->ptr only
            IOCHECKER(wr, dsHelperVPrintStr(pds->ptr, pds->pos, pds->cap, msg, ap), -1) {
                total += wr;
                pds->pos += wr;
            }
            break;
        case DS_FS:     // this is autoextendable
            IOCHECKER(wr, fs_vsprintf_position(&pds->s, pds->pos, msg, ap), -1) {
                total += wr;
                pds->pos += wr; // iterator over fs pds->s
            }
            break;
        default:
            va_end(ap); // for lulz
            return userraise(-1, ERR_ACTION_NOT_APPLICABLE, 
                "Can't write to  %d/%s", pds->type, DSTypeName(pds->type) );
    }
    va_end(ap);
    return total;
}

int                         dsScanf(DS *restrict pds, const char *restrict msg, ...) {
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
            ret = userraise(-1, ERR_UNSUPPORTED_TYPE, "Unsupported %s", DSTypeName(pds->type) );
    }

    va_end(ap);
    return ret;
}

bool                        dsParseInt(DS *restrict pds, int *restrict pval) {
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
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", DSTypeName(pds->type) );
    }
    return true;
}

bool                        dsParseLong(DS *restrict pds, long *restrict pval) {
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
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", DSTypeName(pds->type) );
    }
    return true;
}

bool                        dsParseUnsigned(DS *restrict pds, unsigned int *restrict pval) {
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
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", DSTypeName(pds->type));
    }
    return true;
}

bool                        dsParseUnsignedLong(DS *restrict pds, unsigned long *restrict pval) {
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
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", DSTypeName(pds->type));
    }
    return true;
}

bool                        dsParseDouble(DS *restrict pds, double *restrict pdval) {
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
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", DSTypeName(pds->type));
    }
    return true;
}

bool                        dsParseChar(DS *restrict pds, char *restrict pval) {
    invraisecode(pds != NULL && pval != NULL, ERR_NULLABLE_PTR, "Null input %p %p", pds, pval);

    switch (pds->type) {
        case DS_FILE:
            if (fscanf(pds->fp, " %c", pval) != 1) // " %c" skips whitespace
                return userraise(false, ERR_UNABLE_PARSE_DATA, "Failed to read char from file");
            break;
        case DS_STR: case DS_FS: case DS_CONSTSTR:
            return dsHelperParseChar(dsStrbuf(pds) + pds->pos, pval, &pds->pos);
        default:
            return userraise(false, ERR_UNSUPPORTED_TYPE, "Unsupported %s", DSTypeName(pds->type));
    }
    return true;
}

// just a wrapper over helper dsHelperParseEscapedString
bool                      dsParseQuotedLimitedfs(DS *restrict in, fs *restrict dst, size_t maxlen, bool use_buffer) {
    if (in == NULL || dst == NULL)
        return userraiseint(ERR_NULL_INPUT, "%p %p", in, dst);

    // TODO: use_buffer
    if (maxlen > 0)
        fs_resize(dst, maxlen);     // not necessary but for opt
    fs_setlen(dst, 0);  // WA until normal fs_cmp/fs_cmpstr

    DS      buf = dsCreatefs(dst);

    bool res = ds_parse_quoted_core(in, &buf, maxlen, '"', '"');
    if (!res) {
        dsFree(&buf);
        return userraise(false, ERR_UNABLE_PARSE_DATA, "Unable to parse quoted fs");
    }
    dsReleaseFs(dst, &buf);

    return true;
}
// unlimited quoted line
// TODO: remove that old impl
bool                       dsParseQuotedUnlimfs(DS *restrict in, fs *restrict dst/* , bool use_buffer */) {
    if (in == NULL || dst == NULL || !fs_alloc(dst))
        return userraiseint(ERR_NULL_INPUT, 
            "Null input or non-allocatable fs ds %p fs %p/%s", in, dst, dst ? bool_str(fs_alloc(dst)): "");

    size_t      pos = dsSavepos(in);                       // запоминаем позицию
    bool        error = false;
    size_t      len = 0;

    int         c = dsgetc(in);
    if (c != '"')
        error = true;

    while (!error && (c = dsgetc(in)) != EOF && c != '"') {

        if (c == '\\') {
            if (!dsgetcEscaped(in, &c))
                error = true; // Ошибка, если после '\' ничего нет или неизвестный символ
        }
        if (!error)
            elem(*dst, len++) = (unsigned char) c;   // allocation if required
    }
    if (c != '"')
        error = true;

    fs_setlen(dst, len);

    if (error) {
        dsRestorepos(in, pos);                 // rollback only if error
        return userraise(false, ERR_UNABLE_PARSE_DATA, 
            "Unable to parse quoted line!");
    }

    return true;
}

// -------------------------------------- fs adapters ------------------------------------------------
// ------------------------------- NOTE: no call to fs.c from here -----------------------------------

// write fs data into stream out
long                            fs_dswrite(DS *restrict out, const fs *restrict s) {
    if (!out)
        return userraise(-1L, ERR_NULL_OUTPUT, "");
    if (!s) // that is normal behaviour, just log
        return logsimpleret(0L, "NUll fs");
    if (int_notin(out->type, DS_FILE, DS_STR, DS_FS) )
        return userraise(-1L, ERR_UNSUPPORTED_TYPE, 
            "Unsupported %d/%s", out->type, DSTypeName(out->type));
    return dsfsHelperFsDSWrite(out, s);
}

// techprint used temporary fs buffer (low performace) in order to have the same logic for all path
long                            fs_dstechprint(DS *restrict out, const fs *restrict s, const char *restrict name) {
    if (!out)
        return userraise(-1L, ERR_NULL_OUTPUT, "");
    if (int_notin(out->type, DS_FILE, DS_STR, DS_FS) )
        return userraise(-1L, ERR_UNSUPPORTED_TYPE, 
            "Unsupported %d/%s", out->type, DSTypeName(out->type));

    fs           buf = FS();      // empty
    // common printer for all types! 
    // That is not very good for perf, but ok for techprint
    WRITE_OR_RET_ACTION(dsfsHelperTechprintTofs(&buf, 0L, s, name), -1, 
                        fsfree(buf));

    long  actual_written = dsfsHelperFsDSWrite(out, &buf);
    fsfree(buf);
    return actual_written;
}

long                            fs_dsserialize(DS *restrict out, const fs *restrict s) {
    if (!out || !s)
        return userraise(-1L, ERR_NULL_OUTPUT, "%p %p", out, s);
    long    total = 0L;

    total += WRITE_OR_RET(dsPrintf(out, "FS(\"%zu\"): \"", s->len), -1L);

    for (size_t i = 0; i < s->len; i++)
        total += WRITE_OR_RET(dsputcEscape((unsigned char) s->v[i], out), -1L);

    total += WRITE_OR_RET(dsPrintf(out, "\"\n"), -1L);

    return total;
}

long                           fs_dsload(DS *restrict in, fs *restrict dst, bool use_buffer) {
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

        if (!dsParseQuotedLimitedfsDirect(in, &buf, expected_len + 1) ) {
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

        if (!dsParseQuotedLimitedfsDirect(in, dst, expected_len + 1) ) {
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

// ------------------------- TEST dsParseQuotedLimitedfsDirect -------------------------
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 16);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 8);
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
        
        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 20);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 8);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 8);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 8);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 3);
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
            dsParseQuotedLimitedfsDirect(NULL, &dst, 8);
            test_validatefree(false, fsfree(dst), "must raise error for NULL DS");
        } else {
            test_validatefree(true, fsfree(dst), "correctly raised error");
        }

        if (!try()) {
            dsParseQuotedLimitedfsDirect(&ds, NULL, 8);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 16);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 16);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 16);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 8);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 8);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 32);
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

        bool res = dsParseQuotedLimitedfsDirect(&ds, &dst, 16);
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

// ------------------------- TEST dsParseQuotedUnlimfs -------------------------
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = dsParseQuotedUnlimfs(&ds, &dst);
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
            dsParseQuotedUnlimfs(NULL, &dst);
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
            dsParseQuotedUnlimfs(&ds, NULL);
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
            dsParseQuotedUnlimfs(&ds, &dst);
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 3, '"', '"'); // лимит 3, строка "hello" (5 симв.)
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

        bool res = ds_parse_quoted_core(&in, &out, 3, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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
        DS in = dsCreateconst(input);
        char buf[10] = {0};
        DS out = dsCreatestrCap(buf, sizeof(buf));

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
        test_validatefree(
            res && out.pos == 4,
            (dsFree(&in), dsFree(&out)),
            "expected pos=4, got %zu", out.pos
        );
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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

        bool res = ds_parse_quoted_core(&in, &out, 0, '"', '"');
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
            ds_parse_quoted_core(NULL, &out, 0, '"', '"');
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
            ds_parse_quoted_core(&in, NULL, 0, '"', '"');
            test_validate(false, "must raise error for NULL out");
        } else {
            test_validate(true, "correctly raised error");
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
        TESTADD(tf_ds_printf,               "dsPrintf() simple tests")
      , TESTADD(tf_ds_scanf,                "dsScanf() simple tests")
      , TESTADD(tf_ds_scanf_printf,         "dsScanf() and dsPrintf() combined tests")
      , TESTADD(tf_ds_parsers,              "dsParse<type> simple tests")
      , TESTADD(tf5_fs_dstechprint,         "fs_dstechprint simple test")
      , TESTADD(tf6_fs_dswrite,             "fs_dswrite simple test")
      , TESTADD(tf7_fs_dsserialize_full,    "fs_dsserialize full test (all edges)")
      , TESTADD(tf8_ds_parse_quoted_line,   "dsParseQuotedLimitedfsDirect simple test")
      , TESTADD(tf9_fs_ds_DS_STR_roundtrip, "fs_dsserialize/fs_dsload() DS_STR round-trip test")
      , TESTADD(tf10_fs_ds_CONST_roundtrip, "fs_dsload() with DS_CONSTSTR round-trip and errors")
      , TESTADD(tf11_fs_ds_FS_roundtrip,    "fs_dsload() with DS_FS round-trip and errors")
      , TESTADD(tf12_fs_ds_FILE_roundtrip,  "fs_dsload() with DS_FILE round-trip and errors")
      , TESTADD(tf13_ds_release_fs,         "dsReleaseFs() simple test")
      , TESTADD(tf14_ds_parse_quoted_unlim, "dsParseQuotedUnlimfs() simple test")
      , TESTADD(tf15_ds_parse_quoted_core,  "ds_parse_quoted_core() simple test")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* DS_ADAPTER_TESTING */
