#include "array.h"
#include "ds.h"
#include "ds_adapter.h"

/********************************************************************
                 ARRAY  IO IMPLEMENTATION
********************************************************************/

const char              *g_custom_print_line     = 0;   // TODO: rework that to normal (in Array structure)
// TODO: move into context
const char              *g_save_format_double    = "%8zu\t%.17g\n";
const char              *g_save_format_int       = "%8zu\t%d\n";
const char              *g_save_format_long      = "%8zu\t%ld\n";
const char              *g_save_format_pointer   = "%8zu\t%p\n";
const char              *g_save_format_char      = "%8zu\t%c\n";

#define                         ARRAY_MAX_TYPE_STR          20
#define                         ARRAY_MAX_TYPE_STR_WO_LAST  19

// -------------------------- Utilities -----------------------------

// ------- Helpers -----------
/**
 * @brief Loads array elements from a text stream.
 *
 * The array header must already be read, and the array must be correctly
 * created before calling this function.
 *
 * @param in  input stream, already opened for reading
 * @param arr pointer to the array to fill
 * @return positive value in suceess, -1 if failed 
 */
static long                         
arrayFileLoadValues(FILE *restrict in, Array *restrict parr) {
    ArrayType   typ = arrayGettype(parr);
    fs          buf = FS();
    long        cnt = 0;
    
    Array_pforeach_idx(parr, i) {
        size_t        ind;
        if (fscanf(in, "%6ld\t", &ind) != 1)
            return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Can't parse index");        
        if (ind >= parr->len)
            return userraise(-1, ERR_OUT_OF_RANGE, "%zu must be < %zu", ind, parr->len);

        switch (typ) {
            case ARRAY_INT:
                if (fscanf(in, "%d\n", parr->iv + ind) != 1)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to fscanf a int value");
                break;
            case ARRAY_LONG:
                if (fscanf(in, "%ld\n", parr->lv + ind) != 1)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to fscanf a long value");
                break;
            case ARRAY_DOUBLE:
                if (fscanf(in, "%lg\n", parr->dv + ind) != 1)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to fscanf a double value");
                break;
            case ARRAY_POINTER:
                if (fscanf(in, "%p\n", parr->pv + ind) != 1)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to fscanf a ptr value");
                break;
            case ARRAY_CHAR:
                if (fscanf(in, "%c\n", parr->cv + ind) != 1)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to fscanf a char value");
                break;
            case ARRAY_V64:
                if (value64_loadfile(in, &parr->v64[ind], parr->v64type, true, &buf) != 1)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to fscanf a V64 containered value");
                break;
            default:
                return userraise(-1, ERR_UNSUPPORTED_TYPE, "%d", typ);
        }
        cnt++;
    }
    fsfree(buf);
    return logsimpleret(cnt, "Readed %ld", cnt);
}

static long
arrayLoadValuesFromDS(DS *restrict in, Array *restrict parr, size_t paircount) {
    ArrayType   typ = arrayGettype(parr);
    long        cnt = 0;

    // initially fill by zero
    // arrayFillAll(parr, ARRAY_FILLTYPE_ZERO);
    // load paircount elements
    while (paircount-- > 0) {
        size_t        ind;

        if (!dsParseUnsignedLong(in, &ind))
            return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Can't parse index");  

        if (ind >= parr->len)
            return userraise(-1, ERR_OUT_OF_RANGE, "%ld must be < %zu", ind, parr->len);
        dsSkipSpace(in);    // skip \t

        switch (typ) {
            case ARRAY_INT:
                if (!dsParseInt(in, parr->iv + ind) )
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to parse int");
                break;
            case ARRAY_LONG:
                if (!dsParseLong(in, parr->lv + ind) )
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to parse long");
                break;
            case ARRAY_DOUBLE:
                if (!dsParseDouble(in, parr->dv + ind) )
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to parse double");
                break;
            case ARRAY_CHAR:
                if (!dsParseChar(in, parr->cv + ind, false) )
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unable to parse char");
                break;
            case ARRAY_V64:
                userraiseint(ERR_NOT_IMPLEMENTED_FEATURE, "Not yet implemented loading v64");
            default:    // and pointer!
                return userraise(-1L, ERR_UNSUPPORTED_TYPE, "%d/%s", typ, arrayTypeGetName(typ));
        }
        cnt++;
        if (!dsSkipNl(in))
            return userraise(-1, ERR_WRONG_INPUT_FORMAT,
                "expected newline after value at idx %zu", ind);
    }

    return logsimpleret(cnt, "Read %ld", cnt);
}

/**
 * @brief Writes the array elements into a text stream.
 *
 * This function outputs only the element data, without header or footer.
 *
 * @param out output stream, already opened for writing
 * @param arr constant pointer to the array
 * @return number of bytes written
 */
static long                         
arraySaveValues(FILE *restrict out, const Array *restrict parr) {
    long        total = 0L;
    ArrayType   typ = arrayGettype(parr);
    Array_pforeach_idx(parr, i)
        switch (typ) {
            case ARRAY_INT:
                IOCHECKER(written, fprintf(out, g_save_format_int, i, parr->iv[i]), -1)
                    total += written;
                break;
            case ARRAY_LONG:
                IOCHECKER(written, fprintf(out, g_save_format_long, i, parr->lv[i]), -1)
                    total += written;
                break;
            case ARRAY_DOUBLE:
                IOCHECKER(written, fprintf(out, g_save_format_double, i, parr->dv[i]), -1)
                    total += written;
                break;
            case ARRAY_POINTER:
                IOCHECKER(written, fprintf(out, g_save_format_pointer, i, parr->pv[i]), -1)
                    total += written;
                break;
            case ARRAY_CHAR:
                IOCHECKER(written, fprintf(out, g_save_format_char, i, parr->cv[i]), -1)
                    total += written;
                break;
            case ARRAY_V64:
                IOCHECKER(written, fprintf(out, "%8zu\t", i), -1)   // to supply format
                    total += written;
                IOCHECKER(written, value64_tofile(out, parr->v64[i], parr->v64type, true), -1)
                    total += written;
                break;
            default:
                break;
        }
    return total;
}
/**
 * @brief Writes the array elements into a fs.
 *
 * This function outputs only the element data, without header or footer.
 *
 * @param out pointer to initialized (via FS() at least) fs
 * @param arr constant pointer to the array
 * @return number of bytes written
 */
static long                 
arraySerializeValuesTofs(fs *restrict s, const Array *restrict parr) {
    long        total = 0L;
    ArrayType   typ = arrayGettype(parr);

    Array_pforeach_idx(parr, i) {
        switch (typ) {
            case ARRAY_INT:
                IOCHECKER(written, fs_sprintf_concat(s, g_save_format_int, i, parr->iv[i]), -1)
                    total += written;
                break;
            case ARRAY_LONG:
                IOCHECKER(written, fs_sprintf_concat(s, g_save_format_long, i, parr->lv[i]), -1)
                    total += written;
                break;
            case ARRAY_DOUBLE:
                IOCHECKER(written, fs_sprintf_concat(s, g_save_format_double, i, parr->dv[i]), -1)
                    total += written;
                break;
            case ARRAY_POINTER:
                IOCHECKER(written, fs_sprintf_concat(s, g_save_format_pointer, i, parr->pv[i]), -1)
                    total += written;
                break;
            case ARRAY_CHAR:
                IOCHECKER(written, fs_sprintf_concat(s, g_save_format_char, i, parr->cv[i]), -1)
                    total += written;
                break;
            case ARRAY_V64: {
                total += value64_tostr(s, parr->v64[i], parr->v64type, true);
                break;
            }
            default:
                return userraise(-1, ERR_UNKNOWN_TYPE, "Unknown type %d/%s", typ, arrayTypeGetName(typ));
        }
    }
    return total;
}

static long
arraySerializeValuesToDs(DS *restrict out, const Array *restrict parr) {
    long        total = 0L, cnt = 0L;
    ArrayType   typ = arrayGettype(parr);
    // simple via switch for now: TODO: to be reworked via dispatcher!!!!!

    Array_pforeach_idx(parr, i) {
        switch (typ) {
            case ARRAY_INT:
                    total += WRITE_OR_RET(dsPrintf(out, g_save_format_int, i, parr->iv[i]), -1L);
                break;
            case ARRAY_LONG:
                    total += WRITE_OR_RET(dsPrintf(out, g_save_format_long, i, parr->lv[i]), -1L);
                break;
            case ARRAY_DOUBLE:
                    total += WRITE_OR_RET(dsPrintf(out, g_save_format_double, i, parr->dv[i]), -1L);
                break;
            case ARRAY_POINTER:
                    total += WRITE_OR_RET(dsPrintf(out, g_save_format_pointer, i, parr->pv[i]), -1L);
                break;
            case ARRAY_CHAR:
                    total += WRITE_OR_RET(dsPrintf(out, g_save_format_char, i, parr->cv[i]), -1L);
                break;
            case ARRAY_V64: {
                return userraise(-1, ERR_NOT_IMPLEMENTED_FEATURE, "Not implements for v64 container");
                // total += value64_tods(s, parr->v64[i], parr->v64type, true);
                break;
            }
            default:
                return userraise(-1, ERR_UNKNOWN_TYPE, "Unknown type %d/%s", typ, arrayTypeGetName(typ));
        }
        cnt++;
    }

    return logsimpleret(total, "Total bytes %ld, elements %ld", total, cnt);
}

/**
 * @brief Loads array element values from a text string.
 *
 * Reads values from the string pointed to by `*pdata`, advancing the
 * pointer accordingly.  The array must already be created with the
 * correct type and length.
 *
 * @param pdata pointer to a string pointer
 * @param arr   pointer to the array to fill
 * @return count of bytes read
 */
static long             
arrayFsLoadValues(const char *restrict initdata, Array *restrict parr) {
    const char     *data = initdata;
    ArrayType       typ = arrayGettype(parr);
    fs              buf = FS();

    //for (size_t i = 0; i < arr->len; i++) { // foreach
    Array_pforeach_idx(parr, i) {
        char             *endptr;
        
        long             lind = strtol(data, &endptr, 10);
        if (data == endptr)
            return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Can't parse index");        
        if (lind < 0 || lind >= (long) parr->len)
            return userraise(-1, ERR_OUT_OF_RANGE, "%ld must be between 0 and %zu", lind, parr->len);
        data = endptr;
        size_t              ind = lind;
        switch (typ) {
            case ARRAY_INT:
                parr->iv[ind] = strtol(data, &endptr, 10);
                if (data == endptr)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Can't parse int value");
                data = endptr;
                break;
            case ARRAY_LONG:
                parr->lv[ind] = strtol(data, &endptr, 10);
                if (data == endptr)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Can't parse long value");
                data = endptr;                break;
            case ARRAY_DOUBLE:
                parr->dv[ind] = strtod(data, &endptr);
                if (data == endptr)
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Can't parse double value");
                data = endptr;                break;
            case ARRAY_CHAR:
                data = skip_leading_spaces_nl(data);
                
                if (*data == '\0') 
                    return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Unexpected EO Line");

                parr->cv[ind] = *data; // Используем ind!
                data++; 

                // Пропускаем пробелы после символа, чтобы подготовить data к следующему strtol
                data = skip_leading_spaces_nl(data);
                break;
            case ARRAY_V64: {
                data += value64_loadstr(data, &parr->v64[ind], parr->v64type, true, &buf);
                break;
            }
            default:
                return userraise(-1, ERR_UNSUPPORTED_TYPE, "unsupported type %d/%s", typ, arrayTypeGetName(typ));   // unsupported type
        }
    }
    fsfree(buf);
    return data - initdata; // total read
}

static Array *                 
arrayParseHeaderFile(FILE *in) {
    long                cnt = 0;
    char                typ[ARRAY_MAX_TYPE_STR], v64typ[ARRAY_MAX_TYPE_STR] = "";

    // Read header: "ARRAY: <type> / <v64type> : <count>"
    if (fscanf(in, "ARRAY: %" TOSTRING(ARRAY_MAX_TYPE_STR_WO_LAST) "s / %" TOSTRING(ARRAY_MAX_TYPE_STR_WO_LAST) "s : %ld ", 
                typ, 
                v64typ, 
                &cnt) != 3)
        userraiseint(ERR_WRONG_INPUT_FORMAT, "Array header wrong format");

    Array *parr;// = arraycreateempty();  //ArrayInit();       // zero-init
    ArrayType atype =  arrayTypeFromName(typ);
    switch (atype) {
        case ARRAY_V64: {
            value64_type vt = value64_gettype(v64typ);
            if (vt == VALUE64_UNKNOWN)
                userraiseint(ERR_WRONG_INPUT_FORMAT, "Array header V64 wrong format '%s'", v64typ);
            parr = V64ArrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY, vt);
            break;
        }
        case ARRAY_INT:
            parr = IarrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
            break;
        case ARRAY_LONG:
            parr = LarrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
            break;
        case ARRAY_DOUBLE:
            parr = DarrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
            break;
        case ARRAY_POINTER:
            parr = ParrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
            break;
        case ARRAY_CHAR:
            parr = CarrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
            break;
        default:
            parr = NULL;
            break;
    }
    if (!parr)
        return userraise(parr, ERR_UNSUPPORTED_TYPE, "Unsupported type '%s'", typ);
    else
        return parr;
}

static Array *                   
arrayParseHeaderStr(const char **base) {
    // ---------- 1. Parse header ----------
    char            typ[ARRAY_MAX_TYPE_STR], v64typ[ARRAY_MAX_TYPE_STR] = "";
    size_t          cnt = 0;
    int             header_len = 0;
    Array           *parr = NULL;  // = ArrayInit();
    const char     *data = *base;

    if (sscanf(data, "ARRAY: %" TOSTRING(ARRAY_MAX_TYPE_STR_WO_LAST) "s / %" TOSTRING(ARRAY_MAX_TYPE_STR_WO_LAST) "s : %zu %n", 
                    typ, v64typ, &cnt, &header_len) != 3) {
        return userraise(parr, ERR_WRONG_INPUT_FORMAT, "arrayLoadFromfs: header mismatch");
    } 
    data += header_len;

    // ---------- Create empty array ----------
    ArrayType       atype = arrayTypeFromName(typ);
    value64_type    vt = value64_gettype(v64typ);
    // will set error flag if case of anything
    parr = arrayOnlyCreate(cnt, atype, vt);
    if (!parr)  // 
        return userraise(parr, ERR_UNSUPPORTED_TYPE, "arrayLoadFromfs: unsupported type '%s'", typ);

    *base = data;
    return parr;
}

static Array *
arrayParseHeaderFromDS(DS *restrict source, size_t *restrict paircount) {
    invraisecode(source != NULL && paircount != NULL, ERR_NULL_INPUT,
                "DS is null %p or pair count is null %p", source, paircount);

    fs              typ = FS(), v64typ = FS();
    unsigned long   arrsize = 0L, totalelem = 0L;
    Array           *parr = NULL;

    if (!dsExpect(source, "ARRAY: ") )
        return userraise(parr, ERR_WRONG_INPUT_FORMAT, "'Array' keyword mismatch");

    if (!dsParseWordBuffer(source, &typ) )
        return userraiseact(
            parr, 
            (fsfree(typ), fsfree(v64typ)),
            ERR_WRONG_INPUT_FORMAT, 
            "Unable to parse type"
        );
    if (!dsExpect(source, " / ") )
        return userraiseact(
            parr, 
            (fsfree(typ), fsfree(v64typ)),
            ERR_WRONG_INPUT_FORMAT, 
            "'/' keyword mismatch"
        );
    if (!dsParseWordBuffer(source, &v64typ) )
        return userraiseact(
            parr, 
            (fsfree(typ), fsfree(v64typ)),
            ERR_WRONG_INPUT_FORMAT, "Unable to parse v64 type"
    );
    if (!dsExpect(source, " : ") )
        return userraiseact(
            parr, 
            (fsfree(typ), fsfree(v64typ)),
            ERR_WRONG_INPUT_FORMAT, 
            "':' keyword mismatch"
        );
    if (!dsParseUnsignedLong(source, &arrsize) )
        return userraiseact(
            parr, 
            (fsfree(typ), fsfree(v64typ)),
            ERR_WRONG_INPUT_FORMAT, 
            "Unable to parse arrsize"
        );
    if (!dsExpect(source, " / ") )
        return userraiseact(
            parr, 
            (fsfree(typ), fsfree(v64typ)),
            ERR_WRONG_INPUT_FORMAT, 
            "'/' keyword mismatch"
        );
    if (!dsParseUnsignedLong(source, &totalelem) )
        return userraiseact(
            parr, 
            (fsfree(typ), fsfree(v64typ)),
            ERR_WRONG_INPUT_FORMAT, 
            "Unable to parse paircount"
        );
    *paircount = totalelem;
    
    // ---------- Create empty array ----------
    parr = arrayCreateFromTextparam(arrsize, typ.v, v64typ.v);
    if (!parr) {
        fsfree(typ), fsfree(v64typ);
        return userraise(parr, ERR_UNSUPPORTED_TYPE, 
            "Unable to create array");
    }

    fsfree(typ), fsfree(v64typ);
    return parr;
} 

static bool                     
arrayParseFooterFile(FILE *in) {
    char            typ[ARRAY_MAX_TYPE_STR];
    if (fscanf(in, " ARRAY: %" TOSTRING(ARRAY_MAX_TYPE_STR_WO_LAST) "s", typ) != 1 || strcmp(typ, "DONE") != 0)
        return userraise(false, ERR_WRONG_INPUT_FORMAT, "Wrong final piece '%s'", typ);
    else
        return true;
}

static bool                     
arrayParseFooterStr(const char **base) {
    const char     *data = *base;
    int             footer_len = 0;
    if (sscanf(data, "ARRAY: DONE%n", &footer_len) != 1) {
        return userraise(false, ERR_WRONG_INPUT_FORMAT, 
            "arrayLoadFromfs: footer mismatch '%.30s'", data);
    }
    data += footer_len;
    *base = data;
    return true;
}

static bool
arrayParseFooterFromDS(DS *source) {
    if (!dsExpect(source, "ARRAY: DONE") )
        return userraise(false, ERR_WRONG_INPUT_FORMAT, "'ARRAY: DONE' keywords mismatch");
    return true;
}

// -------------------------- (API) printers ------------------------

/**
 * @brief Prints the contents of an array to a file stream.
 *
 * Output format can be customised via the global variables
 * `g_custom_print_line` and `g_array_rec_line`.
 * For ARRAY_V64 the specialised printer `value64_techfprint()` is used.
 *
 * @param f     output stream (must be opened for writing)
 * @param val   array (by value)
 * @param limit maximum number of elements to print (0 = print all)
 * @return      number of characters printed
 */
long                         
arrayfprint(FILE *restrict out, const Array *restrict val, size_t limit) {
    invraisecode(val != NULL, ERR_NULLABLE_PTR, "Input array is null");
    if (!out)
        return logsimpleerr(0L, "Output file is null");
    long    cnt = 0;
    size_t  i;
    int     array_rec_line = 20;      // default value

    limit = (limit == 0) ? val->len : (limit < val->len) ? limit : val->len;
    if (g_array_rec_line)
        array_rec_line = g_array_rec_line;

    cnt += fprintf(out, "Array (%s[%zu of total %zu]):\n",
                   arrayTypeGetName(val->flags), limit, val->len);

    const char *custom = g_custom_print_line;

    for (i = 0; i < limit; i++) {
        switch (arrayGettype(val)) {
            case ARRAY_INT:
                IOCHECKER(written, fprintf(out, custom ? custom : "[%zu - %6d]\t", i, val->iv[i]), -1L)
                     cnt += written;
                break;
            case ARRAY_LONG:
                IOCHECKER(written, fprintf(out, custom ? custom : "[%zu - %6ld]\t", i, val->lv[i]), -1L)
                    cnt += written;
                break;
            case ARRAY_DOUBLE:
                IOCHECKER(written, fprintf(out, custom ? custom : "[%zu - %.8lg]\t", i, val->dv[i]), -1L)
                    cnt += written;
                break;
            case ARRAY_POINTER:
                IOCHECKER(written, fprintf(out, custom ? custom : "[%zu - %p]\t", i, val->pv[i]), -1L)
                    cnt += written;
                break;
            case ARRAY_CHAR:
                IOCHECKER(written, fprintf(out, custom ? custom : "[%zu - %c]\t", i, val->cv[i]), -1L)
                    cnt += written;
                break;
            case ARRAY_V64:
                // custom format not supported for value64; always use dedicated printer
                IOCHECKER(written, value64_techfprint(out, val->v64[i], val->v64type, ""), -1L)
                    cnt += written;
                break;
            default:
                IOCHECKER(written, fprintf(out, "[%zu - ?]\t", i), -1L )
                    cnt += written;
                break;
        }

        if (((i + 1) % array_rec_line) == 0)
            cnt += fprintf(out, "\n");
    }

    if (i < val->len)
        cnt += fprintf(out, "and more (%zu) ...\n", val->len - i);
    else
        cnt += fprintf(out, "\n");

    return cnt;
}

/**
 * @brief Saves array values to a text file, separated by a delimiter.
 *
 * Each element is written on a single line, with the delimiter appended.
 * For ARRAY_V64 the dedicated value64_tofile() is used.
 *
 * @param arr   array (by value)
 * @param fname file name
 * @param delim delimiter character
 * @return number of bytes written, or -1 on error
 */
long                        
arraySaveFilevalues(const Array *restrict parr, const char *restrict fname, char delim) {
    logenter("%s, [%c]", fname, delim);

    FILE *f = fopen(fname, "w");
    if (!f)
        return userraise(-1L, ERR_UNABLE_OPEN_FILE_WRITE, "Can't open '%s' for writing", fname);

    long          total_written = 0;
    ArrayType     typ = arrayGettype(parr), status = 0;

    for (size_t i = 0; i < parr->len; i++) {
        if (i > 0) {
            if (fputc(delim, f) == EOF) {
                status = -1;
                break;
            }
            total_written++;
        }
        int     written = 0;
        switch (typ) {
            case ARRAY_INT:
                written = fprintf(f, "%d", parr->iv[i]);
                break;
            case ARRAY_LONG:
                written = fprintf(f, "%ld", parr->lv[i]);
                break;
            case ARRAY_DOUBLE:
                written = fprintf(f, "%12.12lf", parr->dv[i]);  // ??????
                break;
            case ARRAY_POINTER:
                written = fprintf(f, "%p", parr->pv[i]);
                break;
            case ARRAY_CHAR:
                written = fprintf(f, "%c", parr->iv[i]);
                break;
            case ARRAY_V64:
                written = value64_tofile(f, parr->v64[i], parr->v64type, true);
                break;
            default:
                fclose(f);
                return userraise(-1L, ERR_UNSUPPORTED_TYPE, 
                    "Unsupported type %d/%s\n", arrayGettype(parr), arrayGetTypeName(parr));
        }
        if (written < 0) {
            status = -1;
            break;
        }
        total_written += written;
    }
    if (status != 0) {
        fclose(f);
        return userraise(-1L, ERR_STREAM_ERROR, "Write error in '%s'", fname);
    }

    if (fclose(f) != 0) {   // NOT SURE
        return userraise(-1L, ERR_STREAM_ERROR, "Error closing file '%s'", fname);
    }

    return logret(total_written, "Done %ld", total_written);
}

/**
 * @brief Saves an array to a text stream in the full ARRAY format.
 *
 * The format is:
 *   ARRAY: <type> / <v64type> : <count>\n
 *   <elements>
 *   ARRAY: DONE\n
 *
 * @param out output stream, already opened for writing
 * @param arr array (by value)
 * @return number of bytes written
 */

long                        
arraySaveFile(FILE *restrict out, const Array *restrict parr) {  
    invraisecode(parr != NULL, ERR_NULLABLE_PTR, "Array is null");
    if (!out)
        return logsimpleret(0L,  "Output is null"); 

    long        total_written = 0L;
    const char  *typ = arrayGetTypeRealName(parr);
    const char  *v64_type  = arrayIsV64(parr) ? arrayGetV64typeName(parr) : "NONV64_TYPE";

    IOCHECKER(written, fprintf(out, "ARRAY: %s / %s : %zu\n", typ, v64_type, parr->len), -1)
        total_written += written;
    IOCHECKER(written, arraySaveValues(out, parr), -1)
        total_written += written;
    IOCHECKER(written, fprintf(out, "ARRAY: DONE\n"), -1)
        total_written += written;
    return total_written;
}

/**
 * @brief Saves an array to a file.
 *
 * Opens the file for writing, calls arraySaveFile(), and closes the file.
 *
 * @param arr   array (by value)
 * @param fname file path
 * @return number of bytes written, or a negative value on error
 */
long                        
arraySaveFileByName(const Array *parr, const char *fname) {
    logenter("%s", fname);

    FILE        *out = fopen(fname, "w");
    if (out == 0)
        return userraise(-1, ERR_UNABLE_OPEN_FILE_WRITE, "Can't open '%s' for write", fname);

    long        res = arraySaveFile(out, parr);
    fclose(out);

    if(res < 0)
        return userraise(res, ERR_STREAM_ERROR, "Unable to save array");
    else
        return logret(res, "Done %ld", res);
}

/**
 * @brief Loads an array from a text stream in the full ARRAY format.
 *
 * Reads the header, creates the array, fills its elements, and checks the
 * footer.
 *
 * @param in input stream, already opened for reading
 * @return loaded array, or NULL
 */
Array *
arrayLoadFile(FILE *in) {
    invraisecode(ERR_NULLABLE_PTR, in != NULL, "Nullable input");

    Array *parr = arrayParseHeaderFile(in); 
    if (!parr)
        return userraise(parr, ERR_UNSUPPORTED_TYPE, "Unable to create array");

    if (arrayFileLoadValues(in, parr) < 0) {
        arrayFree(parr);
        userraise(parr, ERR_WRONG_INPUT_FORMAT, "Unable to read value from file");
    }

    if (!arrayParseFooterFile(in) ) {
        arrayFree(parr);
        userraise(parr, ERR_WRONG_INPUT_FORMAT, "Unable to finish create array");
    }

    return parr;
}

/**
 * @brief Loads an array from a file.
 *
 * Opens the file for reading, calls arrayLoadFile(), and closes the file.
 *
 * @param fname file path
 * @return loaded array, or an array with the error flag set
 */
Array *
arrayLoadFileByName(const char *fname) {
    invraisecode(ERR_NULLABLE_PTR, fname != NULL, "Nullable fname");

    logenter("%s", fname);
    FILE    *in = fopen(fname, "r");

    if (in == 0)
        userraiseint(ERR_UNABLE_OPEN_FILE_READ, "Can't open for read '%s'", fname);
    
    Array   *arr = arrayLoadFile(in);
    
    fclose(in);
    return logret(arr, "Done %zu", arr->len);
}

// -------------------------- (API) serialization -----------------------

long                            
arraySaveToDS(DS *restrict out, Array *restrict parr) {
    invraisecode(out != NULL && parr != NULL, ERR_NULLABLE_PTR, 
        "Out or parr is null %p %p", out, parr);

    long         total_written = 0L;
    const char  *typ = arrayTypeGetName(parr->flags);
    const char  *v64_type  =  arrayGetV64typeName(parr);
    off_t        pos = dsGetpos(out);

    // paircnt == len using this saver
    total_written += WRITE_OR_RET_ACTION(
            dsPrintf(out, "ARRAY: %s / %s : %zu / %zu\n", typ, v64_type, parr->len, parr->len), 
                    -1L, dsRestorepos(out, pos));

    total_written += WRITE_OR_RET_ACTION(
            arraySerializeValuesToDs(out, parr), -1L, dsRestorepos(out, pos));
    total_written += WRITE_OR_RET_ACTION(
            dsPrintf(out, "ARRAY: DONE\n"), -1L, dsRestorepos(out, pos));

    return total_written;
}

long                            
arraySaveTofs(fs *restrict s, const Array *restrict parr) {
    invraisecode(ERR_NULLABLE_PTR, s != NULL && parr != NULL, 
            "Fs nullable or arr is null %p %p", s, parr);

    long        total_written = 0L;
    const char  *typ = arrayTypeGetName(parr->flags);
    const char  *v64_type  = arrayIsV64(parr) ? arrayGetV64typeName(parr) : "NONV64_TYPE";

    total_written += fs_sprintf_concat(s, "ARRAY: %s / %s : %zu\n", typ, v64_type, parr->len);
    total_written += arraySerializeValuesTofs(s, parr);
    total_written += fs_sprintf_concat(s, "ARRAY: DONE\n");
    return total_written;
}

/**
 * @brief Loads an array from a data stream (DS).
 *
 * This function parses a serialized array from the provided stream using a 
 * Header-Body-Footer format. It performs type-safe data loading and ensures 
 * data integrity via the footer.
 *
 * @param source  Pointer to the source data stream.
 * @param parr    Pointer to the destination Array structure.
 *                - If non-NULL: The existing array content is released, and 
 *                  the structure is updated with the newly loaded data.
 *                - If NULL: The loaded data is discarded (dump read) to avoid leaks.
 *
 * @return The total number of elements successfully loaded.
 * @retval Negative error code if parsing, reading, or stream restoration fails.
 *
 * @note If an error occurs during the loading process, the function attempts 
 *       to restore the stream position to its original state using `dsRestorepos`.
 * @note The function assumes the stream follows the format:
 *       "ARRAY: <type> / <v64_type> : <size> / <count> <values> ARRAY: DONE"
 */
long                            
arrayLoadFromDS(DS *restrict source, Array *restrict parr) {
    if (source == NULL)
        userraiseint(ERR_NULL_INPUT, "DS source is null");

    off_t            initpos = dsGetpos(source);
    if (initpos < 0)
        userraise(-1L, ERR_STREAM_ERROR, "Unable to get stream position, but 'll continue");    // just err looging
    
    size_t           paircount = 0L;
    Array           *pa = arrayParseHeaderFromDS(source, &paircount); 
    if (!pa)
        return userraise(-1L, ERR_WRONG_INPUT_FORMAT, "Unable to create empty array");

    long total = arrayLoadValuesFromDS(source, pa, paircount);
    if (total < 0) {
        arrayFree(pa);
        if (initpos >= 0L && !dsRestorepos(source, initpos) )
            userraise(-1L, ERR_STREAM_ERROR, "Unable to restore stream position");  // just err looging
        return userraise(total, ERR_WRONG_INPUT_FORMAT, "Unable to read values");
    }

    if (!arrayParseFooterFromDS(source) ) {
        arrayFree(pa);
        if (initpos >= 0L && !dsRestorepos(source, initpos) )
            userraise(-1L, ERR_STREAM_ERROR, "Unable to restore stream position");  // just err looging
        return userraise(-1L, ERR_WRONG_INPUT_FORMAT, "Unable to finish create array");
    }
    if (parr) {   // if arr is NULL then dump read
        Array temp = *pa;
        arrayFree(parr);        // release if exists
        *parr = temp;
        free(pa);       // a bit stupid, but let it as is for now, probably Array **restrict pparr is required to avoid free
    }
    else
        arrayFree(pa);

    return logsimpleret(total, "Loaded %ld", total);
}

long                            
arrayLoadFromfs(const fs *restrict s, Array *restrict parr) {
    invraisecode(ERR_NULLABLE_PTR, fs_isnull(s),
                 "Nullable input %p", (void*) s);

    const char     *data = s->v;
    long            data_len;
    Array           *pa = arrayParseHeaderStr(&data); 

    if (!pa)
        return userraise(-1, ERR_UNSUPPORTED_TYPE, "Unable to create array");

    if ( (data_len = arrayFsLoadValues(data, pa) )  < 0) {
        arrayFree(pa);
        userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read value from str");
    } else
        data += data_len;   // shift

    if (!arrayParseFooterStr(&data) ) {
        arrayFree(pa);
        userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to finish create array");
    }

   
    if (parr)    // if arr is NULL then dump read
        *parr = *pa;
    return (long) (data - s->v);
}

// -------------------------------Testing --------------------------

#ifdef ARRAYIO_TESTING

#include "test.h"


// ------------------------- TEST arraySaveToDS (DS_STR, граничные случаи) -------------------------
static TestStatus
tf1_array_save_to_ds_str(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Успешная запись ARRAY_INT в DS_STR */
    test_sub("subtest %d: save ARRAY_INT to DS_STR", ++subnum);
    {
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->iv[0] = 10;
        arr->iv[1] = 20;
        arr->iv[2] = 30;
        arr->len = 3;

        char buffer[256];
        memset(buffer, 'x', sizeof(buffer));   // заполняем ненулевыми
        DS out = dsCreatestrCap(buffer, sizeof(buffer));
        size_t pos_before = dsGetpos(&out);

        long written = arraySaveToDS(&out, arr);
        size_t pos_after = dsGetpos(&out);

        const char *expected =
            "ARRAY: INT / NONV64_TYPE : 3\n"
            "     0\t    10\n"
            "     1\t    20\n"
            "     2\t    30\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(pos_after - pos_before == (size_t)written,
                          (arrayFree(arr), dsFree(&out)),
                          "position increment mismatch: wrote %ld, moved %zu",
                          written, pos_after - pos_before);
        test_validatefree(strncmp(buffer, expected, expected_len) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "content mismatch:\n--- got ---\n%.*s\n--- expected ---\n%s",
                          (int)written, buffer, expected);
        // Проверяем, что за пределами записанных данных остались 'x'
        test_validatefree(buffer[written] == '\0', //'x',
                          (arrayFree(arr), dsFree(&out)),
                          "buffer overrun: byte at %ld is %c, expected '\\0'",
                          written, buffer[written]);

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 2. Успешная запись ARRAY_LONG в DS_STR */
    test_sub("subtest %d: save ARRAY_LONG to DS_STR", ++subnum);
    {
        Array *arr = LarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->lv[0] = 123456789L;
        arr->lv[1] = -987654321L;
        arr->len = 2;

        char buffer[256];
        memset(buffer, 'x', sizeof(buffer));
        DS out = dsCreatestrCap(buffer, sizeof(buffer));

        long written = arraySaveToDS(&out, arr);

        const char *expected =
            "ARRAY: LONG / NONV64_TYPE : 2\n"
            "     0\t123456789\n"
            "     1\t-987654321\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(strncmp(buffer, expected, expected_len) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "content mismatch:\n--- got ---\n%.*s\n--- expected ---\n%s",
                          (int)written, buffer, expected);
        test_validatefree(buffer[written] == '\0',
                          (arrayFree(arr), dsFree(&out)),
                          "buffer overrun");

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 3. Успешная запись ARRAY_DOUBLE (проверяем только структуру) */
    test_sub("subtest %d: save ARRAY_DOUBLE to DS_STR", ++subnum);
    {
        Array *arr = DarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->dv[0] = 1.5;
        arr->dv[1] = -0.25;
        arr->len = 2;

        char buffer[256];
        memset(buffer, 'x', sizeof(buffer));
        DS out = dsCreatestrCap(buffer, sizeof(buffer));

        long written = arraySaveToDS(&out, arr);

        // Проверяем, что заголовок и завершающий маркер присутствуют
        test_validatefree(written > 0,
                          (arrayFree(arr), dsFree(&out)),
                          "expected positive return, got %ld", written);
        test_validatefree(strncmp(buffer, "ARRAY: DOUBLE / NONV64_TYPE : 2\n", 32) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "header mismatch (%.36s)", buffer);
        test_validatefree(strncmp(buffer + written - strlen("ARRAY: DONE\n"),
                                  "ARRAY: DONE\n", strlen("ARRAY: DONE\n")) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "footer missing");
        // Проверяем, что после маркера не было лишней записи
        test_validatefree(buffer[written] == '\0',
                          (arrayFree(arr), dsFree(&out)),
                          "buffer overrun");

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 4. Успешная запись ARRAY_CHAR с символом перевода строки */
    test_sub("subtest %d: save ARRAY_CHAR to DS_STR", ++subnum);
    {
        Array *arr = CarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'A';
        arr->cv[1] = 'B';
        arr->cv[2] = '\n';
        arr->len = 3;

        char buffer[256];
        memset(buffer, 'x', sizeof(buffer));
        DS out = dsCreatestrCap(buffer, sizeof(buffer));

        long written = arraySaveToDS(&out, arr);

        const char *expected =
            "ARRAY: CHAR / NONV64_TYPE : 3\n"
            "     0\tA\n"
            "     1\tB\n"
            "     2\t\n\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(strncmp(buffer, expected, expected_len) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "content mismatch:\n--- got ---\n%.*s\n--- expected ---\n%s",
                          (int)written, buffer, expected);
        test_validatefree(buffer[written] == '\0',
                          (arrayFree(arr), dsFree(&out)),
                          "buffer overrun");

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 5. Пустой массив */
    test_sub("subtest %d: save empty ARRAY_INT", ++subnum);
    {
        Array *arr = IarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->len = 0;

        char buffer[256];
        memset(buffer, 'x', sizeof(buffer));
        DS out = dsCreatestrCap(buffer, sizeof(buffer));

        long written = arraySaveToDS(&out, arr);

        const char *expected =
            "ARRAY: INT / NONV64_TYPE : 0\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(strncmp(buffer, expected, expected_len) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "content mismatch");
        test_validatefree(buffer[written] == '\0',
                          (arrayFree(arr), dsFree(&out)),
                          "buffer overrun");

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 6. Переполнение буфера (маленькая ёмкость) */
    test_sub("subtest %d: buffer overflow, rollback", ++subnum);
    {
        Array *arr = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->iv[0] = 42;
        arr->len = 1;

        char buffer[10];   // слишком мало для заголовка
        memset(buffer, 'x', sizeof(buffer));
        DS out = dsCreatestrCap(buffer, sizeof(buffer));
        size_t pos_before = dsGetpos(&out);

        long res = arraySaveToDS(&out, arr);

        test_validatefree(res == -1,
                          (arrayFree(arr), dsFree(&out)),
                          "expected -1, got %ld", res);
        test_validatefree( (size_t) dsGetpos(&out) == pos_before,
                          (arrayFree(arr), dsFree(&out)),
                          "position must be restored to %zu, got %lld",
                          pos_before, dsGetpos(&out));
        // Проверяем, что буфер не был изменён (остался заполнен 'x')
        for (size_t i = 0; i < sizeof(buffer); i++) {
            test_validatefree(buffer[i] == 'x',
                              (arrayFree(arr), dsFree(&out)),
                              "buffer byte %zu changed: %c", i, buffer[i]);
        }

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 7. Точное соответствие размера буфера (впритык) */
    test_sub("subtest %d: exact fit buffer", ++subnum);
    {
        Array *arr = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->iv[0] = 7;
        arr->len = 1;

        // Рассчитаем точный размер выходной строки
        const char *expected =
            "ARRAY: INT / NONV64_TYPE : 1\n"
            "     0\t     7\n"
            "ARRAY: DONE\n";
        size_t  expected_len = strlen(expected);

        size_t  buf_size = expected_len + 1;

        char   *buffer = malloc(buf_size);
        memset(buffer, 'x', buf_size);
        DS      out = dsCreatestrCap(buffer, buf_size);

        long written = arraySaveToDS(&out, arr);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out), free(buffer)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(memcmp(buffer, expected, expected_len) == 0,
                          (arrayFree(arr), dsFree(&out), free(buffer)),
                          "content mismatch");

        arrayFree(arr);
        dsFree(&out);
        free(buffer);
        fs_alloc_check(true);
    }

        /* 7. Точное соответствие размера буфера без запаса под '\0' -> ошибка */
    test_sub("subtest %d: exact fit without terminator -> error", ++subnum);
    {
        Array *arr = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->iv[0] = 7;
        arr->len = 1;

        const char *expected =
            "ARRAY: INT / NONV64_TYPE : 1\n"
            "     0\t     7\n"
            "ARRAY: DONE\n";
        size_t  expected_len = strlen(expected);

        char   *buffer = malloc(expected_len);
        memset(buffer, 'x', expected_len);
        DS      out = dsCreatestrCap(buffer, expected_len);
        size_t  pos_before = dsGetpos(&out);

        long    written = arraySaveToDS(&out, arr);

        test_validatefree(written == -1,
                          (arrayFree(arr), dsFree(&out), free(buffer)),
                          "expected -1, got %ld", written);
        test_validatefree( (size_t) dsGetpos(&out) == pos_before,
                          (arrayFree(arr), dsFree(&out), free(buffer)),
                          "position must be restored to %zu, got %lld",
                          pos_before, dsGetpos(&out)
        );

        arrayFree(arr);
        dsFree(&out);
        free(buffer);
        fs_alloc_check(true);
    }

    /* 8. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        Array *arr = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        char buffer[100];
        DS out = dsCreatestrCap(buffer, sizeof(buffer));

        if (!try()) {
            arraySaveToDS(NULL, arr);
            test_validatefree(false, (arrayFree(arr), dsFree(&out)),
                              "must raise error for NULL out");
        } else {
            test_validatefree(true, (arrayFree(arr), dsFree(&out)),
                              "correctly raised error for NULL out");
        }

        if (!try()) {
            arraySaveToDS(&out, NULL);
            test_validatefree(false, (arrayFree(arr), dsFree(&out)),
                              "must raise error for NULL arr");
        } else {
            test_validatefree(true, (arrayFree(arr), dsFree(&out)),
                              "correctly raised error for NULL arr");
        }

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arraySaveToDS (DS_FS, граничные случаи) -------------------------
static TestStatus
tf2_array_save_to_ds_fs(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Успешная запись ARRAY_INT в DS_FS */
    test_sub("subtest %d: save ARRAY_INT to DS_FS", ++subnum);
    {
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->iv[0] = 10;
        arr->iv[1] = 20;
        arr->iv[2] = 30;
        arr->len = 3;

        fs data = FS();
        DS out = dsCreatefs(&data);
        off_t pos_before = dsGetpos(&out);

        long written = arraySaveToDS(&out, arr);
        off_t pos_after = dsGetpos(&out);

        const char *expected =
            "ARRAY: INT / NONV64_TYPE : 3\n"
            "     0\t    10\n"
            "     1\t    20\n"
            "     2\t    30\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(pos_after - pos_before == (off_t)written,
                          (arrayFree(arr), dsFree(&out)),
                          "position increment mismatch: wrote %ld, moved %lld",
                          written, (long long)(pos_after - pos_before));
        test_validatefree(fscmpstr(out.s, expected) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "content mismatch:\n--- got ---\n%s\n--- expected ---\n%s",
                          fs_str(&out.s), expected);

        arrayFree(arr);
        dsFree(&out);   // освобождает out.s
        fs_alloc_check(true);
    }

    /* 2. Успешная запись ARRAY_LONG в DS_FS */
    test_sub("subtest %d: save ARRAY_LONG to DS_FS", ++subnum);
    {
        Array *arr = LarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->lv[0] = 123456789L;
        arr->lv[1] = -987654321L;
        arr->len = 2;

        fs data = FS();
        DS out = dsCreatefs(&data);

        long written = arraySaveToDS(&out, arr);

        const char *expected =
            "ARRAY: LONG / NONV64_TYPE : 2\n"
            "     0\t123456789\n"
            "     1\t-987654321\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(fscmpstr(out.s, expected) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "content mismatch:\n--- got ---\n%s\n--- expected ---\n%s",
                          fs_str(&out.s), expected);

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 3. Успешная запись ARRAY_DOUBLE в DS_FS */
    test_sub("subtest %d: save ARRAY_DOUBLE to DS_FS", ++subnum);
    {
        Array *arr = DarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->dv[0] = 1.5;
        arr->dv[1] = -0.25;
        arr->len = 2;

        fs data = FS();
        DS out = dsCreatefs(&data);

        long written = arraySaveToDS(&out, arr);

        test_validatefree(written > 0,
                          (arrayFree(arr), dsFree(&out)),
                          "expected positive return, got %ld", written);
        // Проверяем заголовок и завершающий маркер
        test_validatefree(fsinstr(out.s, FSLITERAL("ARRAY: DOUBLE")) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "header missing");
        test_validatefree(fsinstr(out.s, FSLITERAL("ARRAY: DONE\n")) == (long)(out.s.len - strlen("ARRAY: DONE\n")),
                          (arrayFree(arr), dsFree(&out)),
                          "footer missing");

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 4. Успешная запись ARRAY_CHAR с символом перевода строки */
    test_sub("subtest %d: save ARRAY_CHAR to DS_FS", ++subnum);
    {
        Array *arr = CarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'A';
        arr->cv[1] = 'B';
        arr->cv[2] = '\n';
        arr->len = 3;

        fs data = FS();
        DS out = dsCreatefs(&data);

        long written = arraySaveToDS(&out, arr);

        const char *expected =
            "ARRAY: CHAR / NONV64_TYPE : 3\n"
            "     0\tA\n"
            "     1\tB\n"
            "     2\t\n\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(fscmpstr(out.s, expected) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "content mismatch:\n--- got ---\n%s\n--- expected ---\n%s",
                          fs_str(&out.s), expected);

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 5. Пустой массив */
    test_sub("subtest %d: save empty ARRAY_INT", ++subnum);
    {
        Array *arr = IarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->len = 0;

        fs data = FS();
        DS out = dsCreatefs(&data);

        long written = arraySaveToDS(&out, arr);

        const char *expected =
            "ARRAY: INT / NONV64_TYPE : 0\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(fscmpstr(out.s, expected) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "content mismatch");

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 6. Ошибка: V64 не реализован, откат позиции */
    test_sub("subtest %d: V64 not implemented, rollback", ++subnum);
    {
        Array *arr = V64ArrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY, VALUE64_INT);
        arr->len = 1;

        fs data = FS();
        DS out = dsCreatefs(&data);
        off_t pos_before = dsGetpos(&out);

        if (!try()) {
            long res = arraySaveToDS(&out, arr);
            test_validatefree(res == -1,
                              (arrayFree(arr), dsFree(&out)),
                              "expected -1, got %ld", res);
            test_validatefree(dsGetpos(&out) == pos_before,
                              (arrayFree(arr), dsFree(&out)),
                              "position must be restored to %lld, got %lld",
                              (long long)pos_before, (long long)dsGetpos(&out));
        } else {
            // Если возбуждено исключение, это тоже допустимо
            test_validatefree(true,
                              (arrayFree(arr), dsFree(&out)),
                              "exception raised (acceptable)");
        }

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 7. NULL аргументы */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        Array *arr = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        fs data = FS();
        DS out = dsCreatefs(&data);

        if (!try()) {
            arraySaveToDS(NULL, arr);
            test_validatefree(false, (arrayFree(arr), dsFree(&out)),
                              "must raise error for NULL out");
        } else {
            test_validatefree(true, (arrayFree(arr), dsFree(&out)),
                              "correctly raised error for NULL out");
        }

        if (!try()) {
            arraySaveToDS(&out, NULL);
            test_validatefree(false, (arrayFree(arr), dsFree(&out)),
                              "must raise error for NULL arr");
        } else {
            test_validatefree(true, (arrayFree(arr), dsFree(&out)),
                              "correctly raised error for NULL arr");
        }

        arrayFree(arr);
        dsFree(&out);
        fs_alloc_check(true);
    }

    /* 8. Запись в непустой DS_FS (добавление в конец) */
    test_sub("subtest %d: append to existing DS_FS", ++subnum);
    {
        Array *arr1 = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr1->iv[0] = 100;
        arr1->len = 1;
        Array *arr2 = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr2->iv[0] = 200;
        arr2->len = 1;

        fs data = FS();
        DS out = dsCreatefs(&data);

        // Первая запись
        long written1 = arraySaveToDS(&out, arr1);
        // Вторая запись (позиция уже в конце)
        off_t pos_before_second = dsGetpos(&out);
        long written2 = arraySaveToDS(&out, arr2);
        off_t pos_after_second = dsGetpos(&out);

        // Проверяем, что вторая запись добавилась в конец
        const char *expected_total =
            "ARRAY: INT / NONV64_TYPE : 1\n"
            "     0\t   100\n"
            "ARRAY: DONE\n"
            "ARRAY: INT / NONV64_TYPE : 1\n"
            "     0\t   200\n"
            "ARRAY: DONE\n";
        // Ожидаем, что обе записи корректны
        test_validatefree(written1 > 0 && written2 > 0,
                          (arrayFree(arr1), arrayFree(arr2), dsFree(&out)),
                          "expected positive returns, got %ld and %ld", written1, written2);
        test_validatefree(pos_after_second - pos_before_second == (off_t)written2,
                          (arrayFree(arr1), arrayFree(arr2), dsFree(&out)),
                          "second write position mismatch: wrote %ld, moved %lld",
                          written2, (long long)(pos_after_second - pos_before_second));
        test_validatefree(fscmpstr(out.s, expected_total) == 0,
                          (arrayFree(arr1), arrayFree(arr2), dsFree(&out)),
                          "content mismatch:\n--- got ---\n%s\n--- expected ---\n%s",
                          fs_str(&out.s), expected_total);

        arrayFree(arr1);
        arrayFree(arr2);
        dsFree(&out);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST arraySaveToDS (DS_FILE и DS_CONSTSTR) -------------------------
static TestStatus
tf3_array_save_to_ds_file(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Успешная запись ARRAY_INT в DS_FILE */
    test_sub("subtest %d: save ARRAY_INT to DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/arraySaveToDS_file_test.ds";
        FILE *fp = fopen(path, "w+");
        test_validate(fp != NULL, "failed to create test file");

        DS out = dsCreatef(fp);
        Array *arr = IarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->iv[0] = 111;
        arr->iv[1] = 222;
        arr->iv[2] = 333;
        arr->len = 3;

        off_t pos_before = dsGetpos(&out);
        long written = arraySaveToDS(&out, arr);
        off_t pos_after = dsGetpos(&out);

        const char *expected =
            "ARRAY: INT / NONV64_TYPE : 3\n"
            "     0\t   111\n"
            "     1\t   222\n"
            "     2\t   333\n"
            "ARRAY: DONE\n";
        size_t expected_len = strlen(expected);

        test_validatefree(written == (long)expected_len,
                          (arrayFree(arr), dsFree(&out)),
                          "expected written %zu, got %ld", expected_len, written);
        test_validatefree(pos_after - pos_before == (off_t)written,
                          (arrayFree(arr), dsFree(&out)),
                          "position increment mismatch: wrote %ld, moved %lld",
                          written, (long long)(pos_after - pos_before));

        // Читаем файл обратно для проверки содержимого
        fflush(fp);
        fseek(fp, 0, SEEK_SET);
        char buffer[512] = {0};
        size_t nread = fread(buffer, 1, sizeof(buffer)-1, fp);
        buffer[nread] = '\0';

        test_validatefree(strcmp(buffer, expected) == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "file content mismatch:\n--- got ---\n%s\n--- expected ---\n%s",
                          buffer, expected);

        arrayFree(arr);
        dsFree(&out);   // закрывает файл
        fs_alloc_check(true);
    }

    /* 2. Ошибка при записи в DS_CONSTSTR */
    test_sub("subtest %d: DS_CONSTSTR raises error", ++subnum);
    {
        const char *input = "some dummy data";
        DS out = dsCreateconst(input);
        Array *arr = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->iv[0] = 42;
        arr->len = 1;

        off_t pos_before = dsGetpos(&out);
        if (!try()) {
            long res = arraySaveToDS(&out, arr);
            test_validatefree(res == -1,
                              (arrayFree(arr), dsFree(&out)),
                              "expected -1, got %ld", res);
        } else {
            // Исключение допустимо
            test_validatefree(true,
                              (arrayFree(arr), dsFree(&out)),
                              "exception raised (acceptable)");
        }

        // Проверяем, что позиция не изменилась (восстановилась)
        test_validatefree(dsGetpos(&out) == pos_before,
                          (arrayFree(arr), dsFree(&out)),
                          "position must be restored to %lld, got %lld",
                          (long long)pos_before, (long long)dsGetpos(&out));

        // Проверяем, что строка осталась нетронутой
        test_validatefree(strcmp(input, "some dummy data") == 0,
                          (arrayFree(arr), dsFree(&out)),
                          "const string must not change");

        arrayFree(arr);
        dsFree(&out);   // DS_CONSTSTR не освобождает память
        fs_alloc_check(true);
    }

    /* 3. NULL аргументы для DS_FILE  */
    test_sub("subtest %d: NULL arguments raise error", ++subnum);
    {
        Array *arr = IarrayCreate(1, ARRAY_FILLTYPE_SAFE_EMPTY);
        if (!try()) {
            arraySaveToDS(NULL, arr);
            test_validatefree(false, arrayFree(arr), "must raise error for NULL out");
        } else {
            test_validatefree(true, arrayFree(arr), "correctly raised error for NULL out");
        }
    }
    test_sub("subtest %d: NULL array  returns -1", ++subnum);
    {
        const char *path = "res/ds_adapter/arraySaveToDS_file_null_test.ds";
        FILE *fp = fopen(path, "w+");
        test_validate(fp != NULL, "failed to create test file");
        DS out = dsCreatef(fp);
        if (!try()) {
            arraySaveToDS(&out, NULL);
            test_validatefree(false, dsFree(&out), "must raise error for NULL arr");
        } else {
            test_validatefree(true, dsFree(&out), "correctly raised error for NULL arr");
        }
        fs_alloc_check(true);
    }

    /* 2. ARRAY_LONG */
    test_sub("subtest %d: ARRAY_LONG to DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/arraySaveToDS_file_long.ds";
        FILE *fp = fopen(path, "w+");
        test_validate(fp != NULL, "failed to create file");
        DS out = dsCreatef(fp);
        Array *arr = LarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->lv[0] = 123456789L;
        arr->lv[1] = -987654321L;
        arr->len = 2;

        long written = arraySaveToDS(&out, arr);
        dsFree(&out);
        arrayFree(arr);

        test_validate(written > 0, "write failed");

        FILE *f = fopen(path, "rb");
        test_validate(f != NULL, "failed to open file for reading");
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf)-1, f);
        buf[n] = '\0';
        fclose(f);

        const char *expected =
            "ARRAY: LONG / NONV64_TYPE : 2\n"
            "     0\t123456789\n"
            "     1\t-987654321\n"
            "ARRAY: DONE\n";
        test_validate(strcmp(buf, expected) == 0, "content mismatch");
        fs_alloc_check(true);
    }

        /* 6. ARRAY_DOUBLE (проверка заголовка и значений через разбор) */
    test_sub("subtest %d: ARRAY_DOUBLE to DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/arraySaveToDS_file_double.ds";
        FILE *fp = fopen(path, "w+");
        test_validate(fp != NULL, "failed to create file");
        DS out = dsCreatef(fp);
        Array *arr = DarrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->dv[0] = 3.14159265358979;
        arr->dv[1] = -2.718281828459;
        arr->len = 2;

        // Сохраняем ожидаемые значения до освобождения массива
        double expected0 = arr->dv[0];
        double expected1 = arr->dv[1];

        long written = arraySaveToDS(&out, arr);
        dsFree(&out);
        arrayFree(arr);

        test_validate(written > 0, "write failed");

        FILE *f = fopen(path, "rb");
        test_validate(f != NULL, "failed to open file for reading");
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf)-1, f);
        buf[n] = '\0';
        fclose(f);

        // Проверяем заголовок
        const char *header = "ARRAY: DOUBLE / NONV64_TYPE : 2\n";
        test_validate(strncmp(buf, header, strlen(header)) == 0, "header mismatch");
        // Проверяем маркер завершения
        const char *footer = "ARRAY: DONE\n";
        test_validate(strncmp(buf + strlen(buf) - strlen(footer), footer, strlen(footer)) == 0, "footer missing");

        // Разбираем числовые строки между заголовком и маркером
        char *line_start = buf + strlen(header);
        char *line_end = buf + strlen(buf) - strlen(footer) - 1;
        char saved = *line_end;
        *line_end = '\0';

        double v1, v2;
        int matched = sscanf(line_start, "%*s\t%lf\n%*s\t%lf", &v1, &v2);
        *line_end = saved;

        test_validate(matched == 2, "failed to parse double values");
        test_validate(fabs(v1 - expected0) < 1e-12 && fabs(v2 - expected1) < 1e-12,
                      "double values mismatch");
        fs_alloc_check(true);
    }

    /* 4. ARRAY_CHAR (включая спецсимволы) */
    test_sub("subtest %d: ARRAY_CHAR to DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/arraySaveToDS_file_char.ds";
        FILE *fp = fopen(path, "w+");
        test_validate(fp != NULL, "failed to create file");
        DS out = dsCreatef(fp);
        Array *arr = CarrayCreate(3, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->cv[0] = 'A';
        arr->cv[1] = 'B';
        arr->cv[2] = '\n';
        arr->len = 3;

        long written = arraySaveToDS(&out, arr);
        dsFree(&out);
        arrayFree(arr);

        test_validate(written > 0, "write failed");

        FILE *f = fopen(path, "rb");
        test_validate(f != NULL, "failed to open file for reading");
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf)-1, f);
        buf[n] = '\0';
        fclose(f);

        const char *expected =
            "ARRAY: CHAR / NONV64_TYPE : 3\n"
            "     0\tA\n"
            "     1\tB\n"
            "     2\t\n\n"
            "ARRAY: DONE\n";
        test_validate(strcmp(buf, expected) == 0, "content mismatch");
        fs_alloc_check(true);
    }

    /* 5. Пустой массив */
    test_sub("subtest %d: empty ARRAY_INT to DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/arraySaveToDS_file_empty.ds";
        FILE *fp = fopen(path, "w+");
        test_validate(fp != NULL, "failed to create file");
        DS out = dsCreatef(fp);
        Array *arr = IarrayCreate(0, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->len = 0;

        long written = arraySaveToDS(&out, arr);
        dsFree(&out);
        arrayFree(arr);

        test_validate(written > 0, "write failed");

        FILE *f = fopen(path, "rb");
        test_validate(f != NULL, "failed to open file for reading");
        char buf[256];
        size_t n = fread(buf, 1, sizeof(buf)-1, f);
        buf[n] = '\0';
        fclose(f);

        const char *expected =
            "ARRAY: INT / NONV64_TYPE : 0\n"
            "ARRAY: DONE\n";
        test_validate(strcmp(buf, expected) == 0, "content mismatch");
        fs_alloc_check(true);
    }

    test_sub("subtest %d: ARRAY_POINTER to DS_FILE", ++subnum);
    {
        const char *path = "res/ds_adapter/arraySaveToDS_file_pointer.ds";
        FILE *fp = fopen(path, "w+");
        test_validate(fp != NULL, "failed to create file");
        DS out = dsCreatef(fp);

        Array *arr = ParrayCreate(2, ARRAY_FILLTYPE_SAFE_EMPTY);
        arr->pv[0] = (void*)0x1234;
        arr->pv[1] = (void*)0x5678;
        arr->len = 2;

        long written = arraySaveToDS(&out, arr);
        dsFree(&out);
        arrayFree(arr);

        test_validate(written > 0, "write failed");

        FILE *f = fopen(path, "rb");
        test_validate(f != NULL, "failed to open file for reading");
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf)-1, f);
        buf[n] = '\0';
        fclose(f);

        // Проверяем заголовок и завершающий маркер
        const char *header = "ARRAY: POINTER / NONV64_TYPE : 2\n";
        test_validate(strncmp(buf, header, strlen(header)) == 0, "header mismatch");
        const char *footer = "ARRAY: DONE\n";
        test_validate(strncmp(buf + strlen(buf) - strlen(footer), footer, strlen(footer)) == 0,
                    "footer missing");

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
        TESTADD(tf1_array_save_to_ds_str,                "arraySaveToDS DS_STR tests")
      , TESTADD(tf2_array_save_to_ds_fs,                 "arraySaveToDS DS_FS tests")
      , TESTADD(tf3_array_save_to_ds_file,               "arraySaveToDS DS_FILE tests")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* ARRAYIO_TESTING */
