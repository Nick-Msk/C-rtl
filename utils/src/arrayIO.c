#include "array.h"
#include "ds.h"
#include "ds_adapter.h"

/********************************************************************
                 ARRAY  IO IMPLEMENTATION
********************************************************************/

const char              *g_custom_print_line     = 0;   // TODO: rework that to normal (in Array structure)
// TODO: move into context
const char              *g_save_format_double    = "%6zu      %15.15lg\n";
const char              *g_save_format_int       = "%6zu\t%6d\n";
const char              *g_save_format_long      = "%6zu\t%6ld\n";
const char              *g_save_format_pointer   = "%6zu\t%p\n";
const char              *g_save_format_char      = "%6zu\t%c\n";

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
            return userraise(-1, ERR_OUT_OF_RANGE, "%ld must be < %zu", ind, parr->len);

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
                // TODO: // now stupidly via printf
                    total += WRITE_OR_RET(dsPrintf(out, g_save_format_int, i, parr->iv[i]), -1L);
                break;
            case ARRAY_LONG:
                break;
            case ARRAY_DOUBLE:

                break;
            case ARRAY_POINTER:

                break;
            case ARRAY_CHAR:

                break;
            case ARRAY_V64: {
                return userraise(-1, ERR_NOT_IMPLEMENTED_FEATURE, "Not implements for v64 container");
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

static Array                  *arrayParseHeaderFile(FILE *in) {
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
            parr = LArrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
            break;
        case ARRAY_DOUBLE:
            parr = DArrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
            break;
        case ARRAY_POINTER:
            parr = PArrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
            break;
        case ARRAY_CHAR:
            parr = CArrayCreate(cnt, ARRAY_FILLTYPE_SAFE_EMPTY);
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

static Array                   *arrayParseHeaderStr(const char **base) {
    // ---------- 1. Parse header ----------
    char            typ[ARRAY_MAX_TYPE_STR], v64typ[ARRAY_MAX_TYPE_STR] = "";
    size_t          cnt = 0;
    int             header_len = 0;
    Array           *parr = NULL;  // = ArrayInit();
    const char     *data = *base;

    if (sscanf(data, "ARRAY: %" TOSTRING(ARRAY_MAX_TYPE_STR_WO_LAST) "s / %" TOSTRING(ARRAY_MAX_TYPE_STR_WO_LAST) "s : %zu %n", typ, v64typ, &cnt, &header_len) != 3) {
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

static bool                     arrayParseFooterFile(FILE *in) {
    char            typ[ARRAY_MAX_TYPE_STR];
    if (fscanf(in, " ARRAY: %" TOSTRING(ARRAY_MAX_TYPE_STR_WO_LAST) "s", typ) != 1 || strcmp(typ, "DONE") != 0)
        return userraise(false, ERR_WRONG_INPUT_FORMAT, "Wrong final piece '%s'", typ);
    else
        return true;
}

static bool                     arrayParseFooterStr(const char **base) {
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
long                         arrayfprint(FILE *restrict out, const Array *restrict val, size_t limit) {
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
long                        arraySaveFilevalues(const Array *restrict parr, const char *restrict fname, char delim) {
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

long                        arraySaveFile(FILE *restrict out, const Array *restrict parr) {  
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
long                        arraySaveFileByName(const Array *parr, const char *fname) {
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
Array                           *arrayLoadFile(FILE *in) {
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
Array                       *arrayLoadFileByName(const char *fname) {
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


long                            arraySaveToDS(DS *restrict out, Array *restrict parr) {
    invraisecode(ERR_NULLABLE_PTR, out != NULL && parr != NULL, 
        "Out or parr is null %p %p", out, parr);

    long         total_written = 0L;
    const char  *typ = arrayTypeGetName(parr->flags);
    const char  *v64_type  =  arrayGetV64typeName(parr);
    size_t       pos = dsGetpos(out);

    total_written += WRITE_OR_RET_ACTION(dsPrintf(out,  "ARRAY: %s / %s : %zu\n", typ, v64_type, parr->len), -1L, dsRestorepos(pos));

    total_written += WRITE_OR_RET_ACTION(arraySerializeValuesToDs(out, parr), -1L, dsRestorepos(pos));
    total_written += WRITE_OR_RET_ACTION(dsPrintf(out, "ARRAY: DONE\n"), -1, dsRestorepos(pos));
    return total_written;
}

long                            arraySaveTofs(fs *restrict s, const Array *restrict parr) {
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

// new DS 
long                            arrayLoadFromDS(DS *restrict source, Array *restrict parr) {
    long total = 0L;
    // TODO:



    return total;
}

long                            arrayLoadFromfs(const fs *restrict s, Array *restrict parr) {
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
