#include "ds.h"

/********************************************************************
                 Datasource MODULE IMPLEMENTATION
********************************************************************/

// ------------------------------ Utilities ------------------------

/**
 * @brief Internal helper for reading from a memory buffer.
 */
static inline int           dsgetc_buffer(const char *restrict ptr, size_t *restrict pos) {
    if (ptr[*pos] == '\0') {
        return EOF;
    }
    return (unsigned char) ptr[(*pos)++];
}

/**
 * @brief Internal helper for mutable string unget.
 */
static inline int           dsreplace_str(char *restrict ptr, size_t *restrict pos, int c) {
    if (*pos > 0)
        return ptr[--(*pos)] = (unsigned char) c;
    else
        return EOF;
}

/**
 * @brief Internal helper for mutable string put.
 */
static inline int           dsputc_strbuf(char *ptr, size_t pos, size_t cap, int c) {
    if (cap > 0 && pos < cap) {
        ptr[pos] = (unsigned char) c;
        return 1;   // shift
    }
    else
        return EOF;
}

/**
 * @brief Internal helper for constant string unget (conditional rollback).
 */
static inline int           dsungetc_conststr(const char *restrict ptr, size_t *restrict pos, int c) {
    if (*pos > 0 && (unsigned char) ptr[*pos - 1] == (unsigned char) c) {
        (*pos)--;
        return c;
    }
    return EOF;
}

/**
 * @brief Escapes non-printable characters for debug output.
 * @return Number of characters printed.
 */
static int                  ds_escape_print(FILE *out, unsigned char c) {
    switch (c) {
        case '\n': return fprintf(out, "\\n");
        case '\r': return fprintf(out, "\\r");
        case '\t': return fprintf(out, "\\t");
        case '\0': return fprintf(out, "\\0");
        default:
            if (isprint(c)) {
                fputc(c, out);
                return 1;
            } else {
                return fprintf(out, "[0x%02X]", c);
            }
    }
}

/**
 * @brief Helper to print the buffer content and closing delimiters.
 * @return Total characters written to 'out'.
 */
static int                  ds_print_buffer_content(FILE *restrict out, const char *restrict ptr, size_t start, size_t end) {
    int total = 0;
    for (size_t i = start; (end == 0 || i < end) && ptr[i] != '\0'; i++) {
        total += ds_escape_print(out, (unsigned char) ptr[i]);
    }
    return total;
}

// --------------------------- API ---------------------------------

// -------------------- CONSTRUCTOTS/DESTRUCTORS -------------------

bool                        dsInitf(DS *restrict pds, FILE *restrict fp) {
    if (pds == NULL || fp == NULL)
        return false;
    pds->type = DS_FILE;
    pds->fp = fp;
    return true;
}

bool                        dsInitFilename(DS *restrict pds, const char *restrict fname, const char *restrict mode) {
    if (pds == NULL || fname == NULL || mode == NULL)
        return false;
    FILE *f = fopen(fname, mode);
    if (f == NULL)
        return sysraise(false, 
            "Unable to open file '%s' for '%s'", fname, mode);
    return dsInitf(pds, f);     // now pds owned f
}


bool                        dsInitstrCap(DS *restrict pds, char *restrict buf, size_t cap) {
    if (pds == NULL || buf == NULL)
        return false;
    pds->type = DS_STR;
    pds->ptr = buf;
    if (cap != 0L) { // write or read/write => need to empty buffer
        pds->cap = cap;
        // memset(pds->ptr, '\0', cap);
    } else
        pds->cap = strlen(buf);
    pds->pos = 0;
    return true;
}

bool                        dsInitconst(DS *restrict pds, const char *restrict buf) {
    if (pds == NULL || buf == NULL)
        return false;
    pds->type = DS_CONSTSTR;
    pds->constptr = buf;
    pds->cap = strlen(buf); // MUST BE '\0' at the EOL
    pds->pos = 0;
    return true;
}

#ifndef NO_FSDS
    bool                    dsInitfs(DS *restrict pds, fs *restrict s) {
        if (pds == NULL || s == NULL)
            return false;
        pds->type = DS_FS;
        pds->pos = 0;        // iterator
        fs_moveto(&pds->s, s);
        return true;
    }
#endif  /* !NO_FSDS */   

// --------------------- ACCESS AND MODIFICATION --------------------

int                         dsgetc(DS *pds) {
    switch (pds->type) {
        case DS_FILE:
            return fgetc(pds->fp);
        case DS_STR:
            return dsgetc_buffer(pds->ptr, &pds->pos);
        case DS_CONSTSTR:
            return dsgetc_buffer(pds->constptr, &pds->pos);
        case DS_FS:
#ifndef NO_FSDS
            if (fsstr(pds->s))
                return dsgetc_buffer(pds->s.v, &pds->pos);
            else
                return EOF;
#else
            return EOF;
#endif  /* !NO_FSDS */ 
    }
}

int                             dsungetc(int c, DS *pds) {
    if (c == EOF)   // NOT SURE, LET IT BE FOR NOW
        return EOF;
    switch (pds->type) {
        case DS_FILE:
            return ungetc(c, pds->fp);
        case DS_STR:
#ifndef NO_FSDS
        case DS_FS:
#endif  /* !NO_FSDS */ 
        case DS_CONSTSTR: {
            const char *ptr = dsStrbuf(pds);
            // the same logic for FS, STR and CONSTSTR
            return dsungetc_conststr(ptr, &pds->pos, c);
        }
        default:      
            return EOF;
    }
}

// only for DS_STR and DS_FS
int                         dsreplacec(int c, DS *pds) {
    if (c == EOF)
        return EOF;
    switch (pds->type) {
        case DS_STR:
#ifndef NO_FSDS
        case DS_FS:
#endif  /* !NO_FSDS */      
            char *ptr = (char *) dsStrbuf(pds);
            // the same logic for FS and STR
            return dsreplace_str(ptr, &pds->pos, c);
        default:
            return EOF;
    }
}

int                         dsputc(int c, DS *pds) {
    if (c == EOF) {
        if (pds->type == DS_FILE)
            return logsimpleerr(EOF, "put EOF - do nothing");
        else
            c = '\0';
    }

    switch (pds->type) {
        case DS_CONSTSTR:
            return userraise(EOF, ERR_STREAM_ERROR, "Unable to put to contant stream DS_CONSTSTR");     // только для чтения
        case DS_STR: {
            if (c != '\0') {
                pds->pos += WRITE_OR_RET(dsputc_strbuf(pds->ptr, pds->pos, pds->cap, c), EOF);
                return 1;
            } else {
                dsputc_strbuf(pds->ptr, pds->pos, pds->cap, c);
                return 0;
            }
        }
        case DS_FILE:
            WRITE_OR_RET(fputc(c, pds->fp), EOF);
            pds->pos++;
            return 1;  // increment
        case DS_FS:
#ifndef NO_FSDS
            if (c == '\0') {
                fs_setlen(&pds->s, pds->pos);
                return 0;
            } else {
                elem(pds->s, pds->pos++) = (unsigned char) c;
                return 1; // increment
            }
#else
        default:
            return EOF;
#endif
    }
}

int                      dsputcEscape(int c, DS *pds) {
    int cnt = 1;
    switch (c) {
        case '"':   WRITE_OR_RET(dsputc('\\', pds), EOF); 
                    WRITE_OR_RET(dsputc('"', pds), EOF);
                    cnt++;  
            break;
        case '\\':  WRITE_OR_RET(dsputc('\\', pds), EOF); 
                    WRITE_OR_RET(dsputc('\\', pds), EOF); 
                    cnt++; 
            break;
        case '\n':  WRITE_OR_RET(dsputc('\\', pds), EOF); 
                    WRITE_OR_RET(dsputc('n', pds), EOF);
                    cnt++; 
            break;
        case '\r':  WRITE_OR_RET(dsputc('\\', pds), EOF); 
                    WRITE_OR_RET(dsputc('r', pds), EOF);
                    cnt++; 
            break;
        case '\t':  WRITE_OR_RET(dsputc('\\', pds), EOF); 
                    WRITE_OR_RET(dsputc('t', pds), EOF);
                    cnt++; 
            break;
        default:    WRITE_OR_RET(dsputc(c, pds), EOF);   
            break;
    }
    return cnt;
}

// helper for ecranned '//' symbols
bool                        dsgetcEscaped(DS *restrict in, int *c) {
    int esc = dsgetc(in);

    if (esc == EOF)
        return false;

    switch (esc) {
        case '\\': case '"':  break;
        case 'n':  esc = '\n'; break;
        case 'r':  esc = '\r'; break;
        case 't':  esc = '\t'; break;
        default:
            dsungetc(esc, in); 
            return false;
    }
    if (c)
        *c = esc;
    return true;
}

// process symbol (normal or ecranned)
// int                      dsparseEscaped(DS *restrict in, bool *restrict error) {
//     int c = dsgetc(in);
//     if (c == EOF)
//         return EOF; // no more sym

//     if (c != '\\')
//         return c;   // normal sym

//     if (!dsgetcEscaped(in, &c) ) {
//         if (error)
//             *error = true;
//         return EOF; // something wrong with ercanning, stream is rolled back
//     } 
//     return c;   // converted from ecranned
// }

long                     dswrite(DS *restrict out, const char *ptr, size_t len) {

    size_t  total_prepared = len, actual_written = 0L;
    switch (out->type) {
        case DS_FILE: {
            size_t written = fwrite(ptr, sizeof(char), total_prepared, out->fp);
            if (written < total_prepared)
                return userraise(-1L, ERR_STREAM_ERROR,
                    "Unable to fwrite %zu bytes (only %zu)", total_prepared, written);
            actual_written = written;
            break;
        }
        case DS_STR: {
            size_t       remaining = out->cap - out->pos;
            if (remaining > 0)
                remaining--;
            actual_written = (remaining < total_prepared) ? remaining : total_prepared;
            if (remaining > 0)
                memcpy(out->ptr + out->pos, ptr, actual_written);
            out->pos += actual_written;
            break;
        }
#ifndef NO_FSDS
        case DS_FS:
            fs_setlen(&out->s, out->pos);   // just in case
            fs_catmem(&out->s, ptr, total_prepared);
            out->pos += (actual_written = total_prepared);
            break;
#endif  /* !NO_FSDS */  
        default:    // just to avoid warning
    }

    return actual_written;
}

bool                        dsExpect(DS *restrict pds, const char *literal) {
    if (pds == NULL || literal == NULL)
        return userraiseint(ERR_NULL_INPUT, 
            "Ds or literal is null %p %p", pds, literal);
    size_t pos = dsSavepos(pds);
    size_t  i = 0;
    while(literal[i] != '\0') {
        int c = dsgetc(pds);
        if (c == EOF || c != (unsigned char) literal[i]) {
            dsRestorepos(pds, pos);
            return false;
        }
        i++;
    }
    return true;
}

size_t                      dsGetcap(const DS *pds) {
    if (pds == NULL)
        return userraiseint(ERR_NULL_INPUT, 
            "Ds or literal is null %p", pds);
    size_t      size = 0L;

    switch (pds->type) {
        case DS_FILE: {
            off_t sz = getfilesize(pds->fp); // can't use fileutils
            size = (sz > 0) ? (size_t) sz : 0;
            break;
        }
        case DS_STR:
        case DS_CONSTSTR:            
            if (pds->cap > 0)
                size = pds->cap;
            else
                size = strlen(dsStrbuf(pds));
            break;
        case DS_FS:
#ifndef NO_FSDS
            size = pds->s.len;
            break;
#endif
        default:
            return userraise(0L, ERR_UNSUPPORTED_TYPE, 
                "Unsupported %d/%s", pds->type, DSTypeName(pds->type));
    }

    return size;
}

int                         dsTechFPrint(FILE *restrict out, const DS *restrict pds, const char *restrict name) {
    if (!pds || !out) 
        return -1;

    int total = 0;

    switch (pds->type) {
        case DS_FILE:
            IOCHECKERSIMPLE(written, fprintf(out, "[DS_FILE %s] %p\n", name ? name : "", pds->fp), -1)
                total += written;
            break;
        case DS_STR:
        case DS_CONSTSTR: {
            const char *ptr = dsStrbuf(pds);
            IOCHECKERSIMPLE(written, fprintf(out, "[DS_%s %s] pos=%zu, cap=%zu, data=\"", 
                    (pds->type == DS_STR) ? "STR" : "CONSTSTR", 
                        name ? name : "", pds->pos, pds->type == DS_STR ? pds->cap : 0), -1)
                    total += written;
            //
            if (ptr) {
                IOCHECKERSIMPLE(written, ds_print_buffer_content(out, ptr, 0, pds->pos), -1)
                    total += written;
                
                if (pds->pos < pds->cap) {
                    IOCHECKERSIMPLE(written, fprintf(out, "\" => \""), -1)
                        total += written;
                    IOCHECKERSIMPLE(written, ds_print_buffer_content(out, ptr, pds->pos, pds->cap), -1)
                        total += written;
                }
            }
            IOCHECKERSIMPLE(written, fprintf(out, "\"\n"), -1)
                total += written;
            break;
        }
        case DS_FS:
#ifndef NO_FSDS
            IOCHECKERSIMPLE(written, fprintf(out, "[DS_FS %s] pos=%zu ", name ? name : "", pds->pos), -1)
                total += written;
            IOCHECKERSIMPLE(written, fs_techfprint(out, &pds->s, "joined fs"), -1)
                total += written;
#else
            IOCHECKERSIMPLE(written, fprintf(out, "[DS_FS %s (Not supported)]\n", name ? name : ""), -1)
                total += written;
#endif  /* !NO_FSDS */ 
            break;
        default:
            IOCHECKERSIMPLE(written, fprintf(out, "[DS_UNKNOWN]\n"), -1)
                total += written;
            break;
    }
    return total;
}

// -------------------------- (API) printers -----------------------

// -------------------------------Testing --------------------------

#ifdef DS_TESTING

#include "test.h"
#include "checker.h"

// ------------------------- TEST DS (DataSource) -------------------------
static TestStatus
tf_ds(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== 1. Строковый источник: чтение ========== */
    test_sub("subtest %d: dsgetc from string", ++subnum);
    {
        DS ds;
        char text[] = "Hello";
        dsInitstr(&ds, text);

        test_validate(dsgetc(&ds) == 'H', "First char must be 'H'");
        test_validate(dsgetc(&ds) == 'e', "Second char must be 'e'");
        test_validate(dsgetc(&ds) == 'l', "Third char must be 'l'");
        test_validate(dsgetc(&ds) == 'l', "Fourth char must be 'l'");
        test_validate(dsgetc(&ds) == 'o', "Fifth char must be 'o'");
        test_validate(dsgetc(&ds) == EOF, "After end must be EOF");
    }

    /* ========== 2. dsungetc для строки ========== */
    test_sub("subtest %d: dsungetc for string", ++subnum);
    {
        DS ds;
        char text[] = "AB";
        dsInitstr(&ds, text);

        int c1 = dsgetc(&ds);  // 'A'
        test_validate(c1 == 'A', "First char must be 'A'");
        int ret = dsungetc(c1, &ds);
        test_validate(ret == 'A', "dsungetc must return 'A'");
        int c2 = dsgetc(&ds);  // снова 'A'
        test_validate(c2 == 'A', "After ungetc, char must be 'A' again");
        int c3 = dsgetc(&ds);  // 'B'
        test_validate(c3 == 'B', "Next char must be 'B'");
    }

    /* ========== 3. Константный строковый источник ========== */
    test_sub("subtest %d: const string source", ++subnum);
    {
        DS ds;
        const char *text = "World";
        dsInitconst(&ds, text);

        test_validate(dsgetc(&ds) == 'W', "First char must be 'W'");
        test_validate(dsgetc(&ds) == 'o', "Second char must be 'o'");
        test_validate(dsgetc(&ds) == 'r', "Third char must be 'r'");
        test_validate(dsgetc(&ds) == 'l', "Fourth char must be 'l'");
        test_validate(dsgetc(&ds) == 'd', "Fifth char must be 'd'");
        test_validate(dsgetc(&ds) == EOF, "After end must be EOF");
    }

    /* ========== 4. dsungetc для константной строки ========== */
    test_sub("subtest %d: dsungetc for const string", ++subnum);
    {
        DS ds;
        const char *text = "XY";
        dsInitconst(&ds, text);

        int c1 = dsgetc(&ds);  // 'X'
        test_validate(c1 == 'X', "First char must be 'X'");
        dsungetc(c1, &ds);
        int c2 = dsgetc(&ds);
        test_validate(c2 == 'X', "After ungetc, char must be 'X' again");
    }

    /* ========== 5. Файловый источник: чтение ========== */
    test_sub("subtest %d: file source read", ++subnum);
    {
        const char *fname = "res/ds/test_ds_file.ds";
        FILE *fp = fopen(fname, "w");
        test_validate(fp != NULL, "Failed to create temporary file");
        fprintf(fp, "Test");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds;
        dsInitf(&ds, fp);
        test_validate(dsgetc(&ds) == 'T', "First char must be 'T'");
        test_validate(dsgetc(&ds) == 'e', "Second char must be 'e'");
        test_validate(dsgetc(&ds) == 's', "Third char must be 's'");
        test_validate(dsgetc(&ds) == 't', "Fourth char must be 't'");
        test_validate(dsgetc(&ds) == EOF, "After end must be EOF");
        //fclose(fp);
        dsFree(&ds);
    }

    /* ========== 6. dsungetc для файла ========== */
    test_sub("subtest %d: dsungetc for file", ++subnum);
    {
        const char *fname = "res/ds/test_ds_ungetc.ds";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "Z");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds;
        dsInitf(&ds, fp);
        int c = dsgetc(&ds);
        test_validate(c == 'Z', "Char must be 'Z'");
        dsungetc(c, &ds);
        int c2 = dsgetc(&ds);
        test_validate(c2 == 'Z', "After ungetc, char must be 'Z' again");
        //fclose(fp);
        dsFree(&ds);
    }

    /* ========== 7. dsTechFPrint для строки (вывод в файл) ========== */
    test_sub("subtest %d: dsTechFPrint for string", ++subnum);
    {
        DS ds;
        char text[] = "Data";
        dsInitstr(&ds, text);
        dsgetc(&ds);  // 'D'
        dsgetc(&ds);  // 'a'

        const char *fname = "res/ds/test_techprint.ds";
        FILE *out = fopen(fname, "w");
        DSTECHFPRINT(out, ds);
        fclose(out);

        char buf[128];
        FILE *in = fopen(fname, "r");
        size_t n = fread(buf, 1, sizeof(buf) - 1, in);
        buf[n] = '\0';
        fclose(in);

        test_validate(strstr(buf, "DS_STR") != NULL, "Techprint must contain 'DS_STR'");
        test_validate(strstr(buf, "\"Da\" => \"ta\"") != NULL, "Techprint must show read part 'Da' and remaining 'ta'");
    }

    /* ========== 8. Граничные случаи ========== */
    test_sub("subtest %d: empty string", ++subnum);
    {
        DS ds;
        char text[] = "";
        dsInitstr(&ds, text);
        test_validate(dsgetc(&ds) == EOF, "Empty string must return EOF immediately");
    }

    test_sub("subtest %d: dsungetc on empty string (buffer underflow)", ++subnum);
    {
        DS ds;
        char text[] = "X";
        dsInitstr(&ds, text);
        int c = dsgetc(&ds);  // 'X'
        // пробуем вернуть символ, когда позиция уже в начале (после возврата)
        dsungetc(c, &ds);
        // повторный ungetc должен вернуть EOF или последний символ? По реализации dsungetc_str возвращает EOF если pos==0.
        // после первого ungetc pos стало 0, второй вызов:
        int ret = dsungetc('Y', &ds);
        test_validate(ret == EOF, "Second consecutive dsungetc on string must return EOF");
        // но первый ungetc был успешен, поэтому 'X' снова доступен
        int c2 = dsgetc(&ds);
        test_validate(c2 == 'X', "After first ungetc, char must be 'X' again");
    }

    /* ========== 9. dsreplacec ========== */
    test_sub("subtest %d: dsreplacec basic", ++subnum);
    {
        DS ds;
        char text[] = "Hello";
        dsInitstr(&ds, text);

        int c1 = dsgetc(&ds);  // 'H'
        test_validate(c1 == 'H', "First char must be 'H'");
        // заменяем 'H' на 'X'
        int ret = dsreplacec('X', &ds);
        test_validate(ret == 'X', "dsreplacec must return 'X'");
        // перечитываем с той же позиции
        int c2 = dsgetc(&ds);
        test_validate(c2 == 'X', "After replace, char must be 'X'");
        // следующий символ должен быть 'e'
        int c3 = dsgetc(&ds);
        test_validate(c3 == 'e', "Next char must be 'e'");
    }

    test_sub("subtest %d: dsreplacec with EOF", ++subnum);
    {
        DS ds;
        char text[] = "AB";
        dsInitstr(&ds, text);
        dsgetc(&ds);  // 'A'
        // замена на EOF должна вернуть EOF и не менять позицию
        int ret = dsreplacec(EOF, &ds);
        test_validate(ret == EOF, "dsreplacec(EOF) must return EOF");
        // повторное чтение должно вернуть 'B'
        int c = dsgetc(&ds);
        test_validate(c == 'B', "Next char must be 'B'");
    }

    test_sub("subtest %d: dsreplacec at pos 0", ++subnum);
    {
        DS ds;
        char text[] = "XY";
        dsInitstr(&ds, text);
        // позиция 0, замена невозможна
        int ret = dsreplacec('Z', &ds);
        test_validate(ret == EOF, "dsreplacec at pos 0 must return EOF");
        // первое чтение должно вернуть 'X'
        int c = dsgetc(&ds);
        test_validate(c == 'X', "First char must be 'X'");
    }

    test_sub("subtest %d: dsreplacec on non‑string sources", ++subnum);
    {
        // файловый источник не поддерживает замену
        const char *fname = "res/ds/test_replace.ds";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "File");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds;
        dsInitf(&ds, fp);
        int c = dsgetc(&ds);  // 'F'
        test_validate(c == 'F', "First file char must be 'F'");
        int ret = dsreplacec('G', &ds);
        test_validate(ret == EOF, "dsreplacec on file must return EOF");
        fclose(fp);

        // константная строка также не поддерживает замену
        const char *ctext = "Const";
        dsInitconst(&ds, ctext);
        c = dsgetc(&ds);  // 'C'
        test_validate(c == 'C', "First const char must be 'C'");
        ret = dsreplacec('D', &ds);
        test_validate(ret == EOF, "dsreplacec on const string must return EOF");
    }

    /* ========== dsReset ========== */
    test_sub("subtest %d: dsReset on DS_STR", ++subnum);
    {
        char text[] = "XYZ";
        DS ds = dsCreatestr(text);
        dsgetc(&ds); // 'X'
        dsgetc(&ds); // 'Y'
        dsReset(&ds);
        int c = dsgetc(&ds); // должно быть 'X'
        test_validate(c == 'X', "After reset, next char must be 'X', got '%c'", c);
    }

    test_sub("subtest %d: dsReset on DS_FILE", ++subnum);
    {
        const char *fname = "res/ds/tmp_reset.ds";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "ABC");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds = dsCreatef(fp);
        dsgetc(&ds); // 'A'
        dsgetc(&ds); // 'B'
        dsReset(&ds);
        int c = dsgetc(&ds); // должно быть 'A'
        test_validate(c == 'A', "After reset on file, next char must be 'A', got '%c'", c);
        fclose(fp);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST DS additional functions -------------------------
static TestStatus
tf_ds_extra(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. dsStrbuf – строка */
    test_sub("subtest %d: dsStrbuf on DS_STR", ++subnum);
    {
        char text[] = "Hello";
        DS ds = dsCreatestr(text);
        const char *p = dsStrbuf(&ds);
        test_validate(p == text, "dsStrbuf must return original buffer pointer");
    }

    /* 2. dsStrbuf – константная строка */
    test_sub("subtest %d: dsStrbuf on DS_CONSTSTR", ++subnum);
    {
        const char *text = "World";
        DS ds = dsCreateconst(text);
        const char *p = dsStrbuf(&ds);
        test_validate(p == text, "dsStrbuf must return original const pointer");
    }

    /* 3. dsStrbuf – файловый источник должен вернуть NULL */
    test_sub("subtest %d: dsStrbuf on DS_FILE returns NULL", ++subnum);
    {
        const char *fname = "res/ds/tmp_strbuf.ds";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "ignored");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds = dsCreatef(fp);
        const char *p = dsStrbuf(&ds);
        test_validate(p == NULL, "dsStrbuf on file must return NULL");
        fclose(fp);
    }

    /* 4. dsIsstr – строковые источники */
    test_sub("subtest %d: dsIsstr true for DS_STR and DS_CONSTSTR", ++subnum);
    {
        DS ds1 = dsCreatestr("a");
        DS ds2 = dsCreateconst("b");
        test_validate(dsIsstr(&ds1) && dsIsstr(&ds2), "Both DS_STR and DS_CONSTSTR must be recognised as strings");
    }

    /* 5. dsIsstr – файловый источник не строка */
    test_sub("subtest %d: dsIsstr false for DS_FILE", ++subnum);
    {
        const char *fname = "res/ds/tmp_isstr.ds";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "x");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds = dsCreatef(fp);
        test_validate(!dsIsstr(&ds), "File source must not be a string");
        fclose(fp);
    }

    /* 6. dsSavepos / dsRestorepos – строковый источник */
    test_sub("subtest %d: dsSavepos/dsRestorepos on DS_STR", ++subnum);
    {
        char text[] = "ABCDEF";
        DS ds = dsCreatestr(text);
        // читаем несколько символов
        dsgetc(&ds); // 'A'
        dsgetc(&ds); // 'B'
        size_t pos = dsSavepos(&ds);
        dsgetc(&ds); // 'C'
        dsgetc(&ds); // 'D'
        dsRestorepos(&ds, pos);
        int c = dsgetc(&ds); // должно быть 'C'
        test_validate(c == 'C', "After restore, next char must be 'C', got '%c'", c);
    }

    /* 7. dsSavepos / dsRestorepos – файловый источник */
    test_sub("subtest %d: dsSavepos/dsRestorepos on DS_FILE", ++subnum);
    {
        const char *fname = "res/ds/tmp_savepos.ds";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "12345");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds = dsCreatef(fp);
        dsgetc(&ds); // '1'
        dsgetc(&ds); // '2'
        size_t pos = dsSavepos(&ds);
        dsgetc(&ds); // '3'
        dsgetc(&ds); // '4'
        dsRestorepos(&ds, pos);
        int c = dsgetc(&ds); // должно быть '3'
        test_validate(c == '3', "After restore on file, next char must be '3', got '%c'", c);
        fclose(fp);
    }

    /* 8. dsRestorepos без предварительного Save – не ломается, просто возвращается в начало? */
    test_sub("subtest %d: dsRestorepos without Save (DS_STR)", ++subnum);
    {
        char text[] = "XYZ";
        DS ds = dsCreatestr(text);
        dsgetc(&ds); // 'X'
        dsgetc(&ds); // 'Y'
        dsRestorepos(&ds, 0L);
        // просто проверяем, что не краш
        int c = dsgetc(&ds);
        test_validate(c == 'X', "Restore without save must not crash");
    }
    /* 9. dsRestorepos without Save (DS_FILE) */
    test_sub("subtest %d: dsRestorepos without Save (DS_FILE)", ++subnum);
    {
        const char *fname = "res/ds/tmp_restore_nosave.ds";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "ABC");
        fclose(fp);

        fp = fopen(fname, "r");
        DS ds = dsCreatef(fp);
        dsgetc(&ds); // 'A'
        dsgetc(&ds); // 'B'
        dsRestorepos(&ds, 0L);
        int c = dsgetc(&ds); // должно быть 'A'
        test_validate(c == 'A', "Restore without save on file must reset to beginning, got '%c'", c);
        fclose(fp);
    }

    return logret(TEST_PASSED, "done");
}

static TestStatus
tf3_ds_putc(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_FILE */
    test_sub("subtest %d: dsputc to file", ++subnum);
    {
        const char *fname = "res/ds/test_putc_file.ds";
        FILE *fp = fopen(fname, "w");
        DS ds = dsCreatef(fp);
        int res;

        res = dsputc('A', &ds);
        test_validate(res == 1, "dsputc 'A' must return 1, got %d", res);
        res = dsputc('B', &ds);
        test_validate(res == 1, "dsputc 'B' must return 1, got %d", res);
        res = dsputc('C', &ds);
        test_validate(res == 1, "dsputc 'C' must return 1, got %d", res);

        fclose(fp);
        fp = fopen(fname, "r");
        char buf[8] = {0};
        fread(buf, 1, 3, fp);
        fclose(fp);
        test_validate(strcmp(buf, "ABC") == 0,
                      "File must contain 'ABC', got '%s'", buf);
    }

    /* 2. DS_STR с явной ёмкостью */
    test_sub("subtest %d: dsputc to mutable string", ++subnum);
    {
        char text[10] = ".........";
        DS ds = dsCreatestrCap(text, sizeof(text));
        int res;

        res = dsputc('X', &ds);
        test_validate(res == 1, "dsputc 'X' must return 1, got %d", res);
        res = dsputc('Y', &ds);
        test_validate(res == 1, "dsputc 'Y' must return 1, got %d", res);
        res = dsputc('Z', &ds);
        test_validate(res == 1, "dsputc 'Z' must return 1, got %d", res);

        test_validate(text[0] == 'X' && text[1] == 'Y' && text[2] == 'Z',
                      "String must contain 'XYZ', got '%c%c%c'",
                      text[0], text[1], text[2]);
        test_validate(ds.pos == 3, "pos must be 3, got %zu", ds.pos);
    }

    /* 3. DS_STR: переполнение */
    test_sub("subtest %d: dsputc past capacity returns 0", ++subnum);
    {
        char text[3];
        DS ds = dsCreatestrCap(text, sizeof(text));
        ds.pos = 2;                   // почти заполнен
        int res = dsputc('X', &ds);   // запишется, pos станет 3? 
        test_validate(res == 1, "first write must succeed");
        res = dsputc('Y', &ds);       // места нет (cap=3, pos=3)
        test_validate(res == EOF, "dsputc past capacity must return EOF, got %d", res);
    }

    /* 4. DS_CONSTSTR */
    test_sub("subtest %d: dsputc to const string returns EOF", ++subnum);
    {
        const char *text = "const";
        DS ds = dsCreateconst(text);
        int res = dsputc('A', &ds);
        test_validate(res == EOF,
                      "dsputc on const string must return EOF, got %d", res);
    }

    /* 5. dsputc(EOF) */
    test_sub("subtest %d: dsputc(EOF) returns EOF", ++subnum);
    {
        DS ds = dsCreatef(stdout);
        int res = dsputc(EOF, &ds);
        test_validate(res == EOF,
                      "dsputc(EOF) must return EOF, got %d", res);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsputc / dsgetc for DS_FS -------------------------
static TestStatus
tf4_ds_fs(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

#ifndef NO_FSDS
    test_sub("subtest %d: dsputc/dsgetc round-trip on FS", ++subnum);
    {
        fs s = FS();
        DS ds = dsCreatefs(&s);
        int res;

        res = dsputc('A', &ds);
        test_validate(res == 1, "dsputc 'A' must return 1");
        res = dsputc('B', &ds);
        test_validate(res == 1, "dsputc 'B' must return 1");
        res = dsputc('C', &ds);
        test_validate(res == 1, "dsputc 'C' must return 1");
        dsputc(EOF, &ds);

        dsReset(&ds);
        test_validate(dsgetc(&ds) == 'A', "read A");
        test_validate(dsgetc(&ds) == 'B', "read B");
        test_validate(dsgetc(&ds) == 'C', "read C");
        test_validate(dsgetc(&ds) == EOF, "after C must be EOF");
        dsFree(&ds);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: dsputc auto-extend on FS", ++subnum);
    {
        fs s = FS();
        DS ds = dsCreatefs(&s);
        ds.pos = 5;
        int res = dsputc('X', &ds);
        test_validatefree(
            res == 1, 
            dsFree(&ds),
            "dsputc at pos 5 must return 1"
        );
        dsputc(EOF, &ds);
        test_validatefree(
            res == 1, 
            dsFree(&ds),
            "dsputc at pos 5 must return 1"
        );
        test_validate(ds.s.len == 6 && ds.s.v[5] == 'X' && ds.s.v[6] == '\0',
                      "FS len= %zu, str[5]='%c', str[6]='%c'", ds.s.len, ds.s.v[5], ds.s.v[6]);
        dsFree(&ds);
    }
    fs_alloc_check(true);

    /* 3. DS_FS: попытка чтения за пределами len возвращает EOF */
    test_sub("subtest %d: dsgetc beyond len on FS returns EOF", ++subnum);
    {
        const char  pattern[] = "Test1";
        int         c; 
        fs          s = fscopy(pattern);
        DS          ds = dsCreatefs(&s);
        //ds.pos = 0;
        for (int i = 0; i < (int) strlen(pattern); i++) {
            c = dsgetc(&ds);
            test_validatefree(
                (unsigned char) c == pattern[i],
                dsFree(&ds),
                "%d elem must be '%c', got '%c", i, pattern[i], c
            );
        }
        c = dsgetc(&ds);
        test_validatefree(
            c == EOF, 
            dsFree(&ds),
            "dsgetc on empty FS must return EOF, got '%c'", c
        );
        dsFree(&ds);
    }
    fs_alloc_check(true);

    /* 4. DS_FS: попытка чтения за пределами len возвращает EOF */
    test_sub("subtest %d: dsgetc beyond len on EMPTY FS returns EOF", ++subnum);
    {
        fs s = FS();
        DS ds = dsCreatefs(&s);
        //ds.pos = 0;
        int c = dsgetc(&ds);
        test_validate(c == EOF, "dsgetc on empty FS must return EOF, got '%c'", c);
        dsFree(&ds);
    }
    fs_alloc_check(true);
#endif /* !NO_FSDS */

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsCreateFilename / dsInitFilename -------------------------
static TestStatus
tf5_ds_create_filename(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Создание нового файла для записи, закрытие через dsFree */
    test_sub("subtest %d: create new file for write", ++subnum);
    {
        const char *fname = "res/ds/ds_create_write.ds";
        DS ds = dsCreateFilename(fname, "w");
        test_validatefree(ds.fp != NULL, (dsFree(&ds)),
                          "file must be opened for writing");

        fprintf(ds.fp, "hello\n");
        dsFree(&ds);   // dsFree теперь закрывает FILE*
        fs_alloc_check(true);
    }

    /* 2. Открытие существующего файла для чтения */
    test_sub("subtest %d: open existing file for read", ++subnum);
    {
        const char *fname = "res/ds/ds_create_read.ds";
        FILE *w = fopen(fname, "w");
        test_validate(w != NULL, "can't create file");
        fputs("world", w);
        fclose(w);

        DS ds = dsCreateFilename(fname, "r");
        test_validatefree(ds.fp != NULL, (dsFree(&ds)),
                          "file must be opened for reading");

        char buf[10];
        size_t n = fread(buf, 1, sizeof(buf)-1, ds.fp);
        buf[n] = '\0';
        test_validatefree(strcmp(buf, "world") == 0,
                          (dsFree(&ds)),
                          "content mismatch: '%s'", buf);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 3. Открытие несуществующего файла для чтения должно завершиться ошибкой */
    test_sub("subtest %d: open non-existing for read fails", ++subnum);
    {
        const char *fname = "res/ds/no_such_file.ds";
        DS ds = dsCreateFilename(fname, "r");
        test_validate(ds.fp == NULL,
                      "expected fp == NULL, got %p", (void*)ds.fp);
        // dsFree не вызываем, так как fp == NULL (или вызвать и проверить, что не падает)
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 4. Режим добавления */
    test_sub("subtest %d: append mode", ++subnum);
    {
        const char *fname = "res/ds/ds_create_append.ds";
        FILE *w = fopen(fname, "w");
        test_validate(w != NULL, "can't create file");
        fputs("one", w);
        fclose(w);

        DS ds = dsCreateFilename(fname, "a");
        test_validatefree(ds.fp != NULL, (dsFree(&ds)),
                          "file must be opened for append");
        fputs("two", ds.fp);
        dsFree(&ds);

        FILE *r = fopen(fname, "r");
        test_validate(r != NULL, "can't reopen for check");
        char buf[20];
        size_t n = fread(buf, 1, sizeof(buf)-1, r);
        buf[n] = '\0';
        test_validatefree(strcmp(buf, "onetwo") == 0,
                          (fclose(r)),
                          "append content mismatch: '%s'", buf);
        fclose(r);
        fs_alloc_check(true);
    }

    /* 5. NULL аргументы */
    test_sub("subtest %d: NULL arguments fail", ++subnum);
    {
        DS tmp = DSFILE();
        bool ok1 = dsInitFilename(NULL, "file", "r");
        bool ok2 = dsInitFilename(&tmp, NULL, "r");
        bool ok3 = dsInitFilename(&tmp, "file", NULL);

        test_validate(!ok1 && !ok2 && !ok3,
                      "all NULL variants must fail");

        // dsCreateFilename с NULL должен вернуть пустой DS
        DS bad = dsCreateFilename(NULL, "r");
        test_validate(bad.fp == NULL && bad.type == 0,
                      "dsCreateFilename(NULL) must return empty DS");
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dswrite -------------------------
static TestStatus
tf6_dswrite(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Запись в DS_FILE */
    test_sub("subtest %d: DS_FILE write", ++subnum);
    {
        const char *fname = "res/ds/dswrite_file.ds";
        DS ds = dsCreateFilename(fname, "w+");
        test_validatefree(ds.fp != NULL, (dsFree(&ds)), "can't open file");

        const char *data = "hello file";
        long written = dswrite(&ds, data, strlen(data));
        test_validatefree(written == (long)strlen(data),
                          (dsFree(&ds)),
                          "expected %zu, got %ld", strlen(data), written);

        rewind(ds.fp);
        char buf[64];
        size_t n = fread(buf, 1, sizeof(buf)-1, ds.fp);
        buf[n] = '\0';
        test_validatefree(strcmp(buf, data) == 0,
                          (dsFree(&ds)),
                          "content mismatch: '%s'", buf);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 2. Запись в DS_STR с достаточной ёмкостью */
    test_sub("subtest %d: DS_STR write, enough capacity", ++subnum);
    {
        char buffer[64];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        const char *data = "hello str";
        long written = dswrite(&ds, data, strlen(data));
        test_validatefree(written == (long)strlen(data),
                          (dsFree(&ds)),
                          "expected %zu, got %ld", strlen(data), written);

        buffer[ds.pos] = '\0';
        test_validatefree(strcmp(buffer, data) == 0,
                          (dsFree(&ds)),
                          "content mismatch: '%s'", buffer);
        test_validatefree(ds.pos == strlen(data), (dsFree(&ds)),
                          "pos expected %zu, got %zu", strlen(data), ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 3. Запись в DS_STR с ограниченной ёмкостью (truncate, резерв байта) */
    test_sub("subtest %d: DS_STR write, limited capacity", ++subnum);
    {
        char buffer[4];   // реально можно записать 3 символа + '\0'
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        const char *data = "hello";
        long written = dswrite(&ds, data, strlen(data));
        test_validatefree(written == 3,
                          (dsFree(&ds)),
                          "expected truncated 3, got %ld", written);

        buffer[ds.pos] = '\0';
        test_validatefree(strcmp(buffer, "hel") == 0,
                          (dsFree(&ds)),
                          "content mismatch: '%s'", buffer);
        test_validatefree(ds.pos == 3, (dsFree(&ds)),
                          "pos expected 3, got %zu", ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 4. Запись пустой строки (len=0) в DS_STR */
    test_sub("subtest %d: DS_STR write zero length", ++subnum);
    {
        char buffer[16];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));
        long written = dswrite(&ds, "abc", 0);
        test_validatefree(written == 0, (dsFree(&ds)),
                          "expected 0, got %ld", written);
        test_validatefree(ds.pos == 0, (dsFree(&ds)),
                          "pos expected 0, got %zu", ds.pos);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 5. Запись в DS_FS */
    test_sub("subtest %d: DS_FS write", ++subnum);
    {
        fs out = FS();
        DS ds = dsCreatefs(&out);   // владение переходит в ds
        const char *data = "hello fs";
        long written = dswrite(&ds, data, strlen(data));
        test_validatefree(written == (long)strlen(data),
                          (dsFree(&ds)),
                          "expected %zu, got %ld", strlen(data), written);

        test_validatefree(strcmp(fs_str(&ds.s), data) == 0,
                          (dsFree(&ds)),
                          "content mismatch: '%s'", fs_str(&ds.s));
        test_validatefree(ds.pos == strlen(data), (dsFree(&ds)),
                          "pos expected %zu, got %zu", strlen(data), ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 6. Запись в DS_FS с встроенным нулём (бинарные данные) */
    test_sub("subtest %d: DS_FS write binary data", ++subnum);
    {
        fs out = FS();
        DS ds = dsCreatefs(&out);
        const char data[] = {'a','\0','b'};
        long written = dswrite(&ds, data, sizeof(data) - 1);
        test_validatefree(written == (long)sizeof(data) - 1,
                          (dsFree(&ds)),
                          "expected %zu, got %ld", sizeof(data), written);

        test_validatefree(fs_len(&ds.s) == sizeof(data) - 1,
                          (dsFree(&ds)),
                          "len expected %zu, got %zu", sizeof(data) - 1, fs_len(&ds.s));
        test_validatefree(memcmp(fs_str(&ds.s), data, sizeof(data) - 1) == 0,
                          (dsFree(&ds)),
                          "binary content mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 7. Ошибка при неподдерживаемом типе DS (DS_CONSTSTR) */
    test_sub("subtest %d: unsupported DS type", ++subnum);
    {
        DS ds = {0};
        ds.type = DS_CONSTSTR;   // только для чтения
        const char *data = "test";
        if (!try()) {
            dswrite(&ds, data, strlen(data));
            test_validate(false, "must raise error");
        } else {
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 8. NULL выходной параметр */
    test_sub("subtest %d: NULL output", ++subnum);
    {
        const char *data = "test";
        if (!try()) {
            dswrite(NULL, data, strlen(data));
            test_validate(false, "must raise error");
        } else {
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

        /* 9. Последовательные записи в DS_STR */
    test_sub("subtest %d: DS_STR multiple writes accumulate", ++subnum);
    {
        char buffer[64];
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));

        const char *parts[] = {"Hello", " ", "world", "!"};
        size_t total = 0;
        for (size_t i = 0; i < COUNT(parts); i++) {
            long w = dswrite(&ds, parts[i], strlen(parts[i]));
            test_validatefree(w == (long)strlen(parts[i]),
                              (dsFree(&ds)),
                              "part %zu: expected %zu, got %ld",
                              i, strlen(parts[i]), w);
            total += w;
        }

        buffer[ds.pos] = '\0';
        test_validatefree(strcmp(buffer, "Hello world!") == 0,
                          (dsFree(&ds)),
                          "accumulated mismatch: '%s'", buffer);
        test_validatefree(ds.pos == total,
                          (dsFree(&ds)),
                          "pos expected %zu, got %zu", total, ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 10. Последовательные записи в DS_FILE */
    test_sub("subtest %d: DS_FILE multiple writes accumulate", ++subnum);
    {
        const char *fname = "res/ds/dswrite_file_multi.ds";
        DS ds = dsCreateFilename(fname, "w+");
        test_validatefree(ds.fp != NULL, (dsFree(&ds)), "can't open file");

        const char *a = "foo";
        const char *b = "bar";
        dswrite(&ds, a, strlen(a));
        dswrite(&ds, b, strlen(b));

        rewind(ds.fp);
        char buf[32];
        size_t n = fread(buf, 1, sizeof(buf)-1, ds.fp);
        buf[n] = '\0';
        test_validatefree(strcmp(buf, "foobar") == 0,
                          (dsFree(&ds)),
                          "file content mismatch: '%s'", buf);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 11. Последовательные записи в DS_FS */
    test_sub("subtest %d: DS_FS multiple writes accumulate", ++subnum);
    {
        fs out = FS();
        DS ds = dsCreatefs(&out);

        const char *a = "one";
        const char *b = "-two";
        dswrite(&ds, a, strlen(a));
        dswrite(&ds, b, strlen(b));

        test_validatefree(strcmp(fs_str(&ds.s), "one-two") == 0,
                          (dsFree(&ds)),
                          "fs content mismatch: '%s'", fs_str(&ds.s));
        test_validatefree(ds.pos == strlen("one-two"),
                          (dsFree(&ds)),
                          "pos expected %zu, got %zu", strlen("one-two"), ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 12. Пустая запись (len=0) в DS_FILE и DS_FS */
    test_sub("subtest %d: zero-length write in DS_FILE", ++subnum);
    {
        // DS_FILE
        const char *fname = "res/ds/dswrite_zero_file.ds";
        DS ds_file = dsCreateFilename(fname, "w+");
        test_validatefree(ds_file.fp != NULL, (dsFree(&ds_file)), "can't open file");
        long w_file = dswrite(&ds_file, "abc", 0);
        test_validatefree(w_file == 0, (dsFree(&ds_file)), "expected 0, got %ld", w_file);
        dsFree(&ds_file);

    }
    test_sub("subtest %d: zero-length write in DS_FS", ++subnum);
    {
        // DS_FS
        fs out = FS();
        DS ds_fs = dsCreatefs(&out);
        long w_fs = dswrite(&ds_fs, "abc", 0);
        test_validatefree(w_fs == 0, (dsFree(&ds_fs)), "expected 0, got %ld", w_fs);
        test_validatefree(fs_len(&ds_fs.s) == 0,
                          (dsFree(&ds_fs)),
                          "fs len expected 0, got %zu", fs_len(&ds_fs.s));
        dsFree(&ds_fs);
        fs_alloc_check(true);
    }

    /* 13. DS_STR с нулевой ёмкостью (если конструктор позволяет) */
    test_sub("subtest %d: DS_STR with zero capacity", ++subnum);
    {
        // dsCreatestrCap требует cap > 0, поэтому используем DSSTR() и установим cap=0 вручную
        DS ds = DSSTR();
        ds.cap = 0;
        const char *data = "test";
        long written = dswrite(&ds, data, strlen(data));
        test_validate(written == 0, "expected 0 for zero capacity, got %ld", written);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsExpect -------------------------
// ------------------------- TEST dsExpect (without ok variable) -------------------------
static TestStatus
tf7_ds_expect(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_CONSTSTR: успешное совпадение */
    test_sub("subtest %d: dsExpect success on CONSTSTR", ++subnum);
    {
        const char *text = "hello";
        DS ds = dsCreateconst(text);

        test_validate(dsExpect(&ds, "hell"),
                      "must return true for matching prefix");
        test_validate(ds.pos == 4, "pos must advance to 4, got %zu", ds.pos);
    }

    /* 2. DS_CONSTSTR: несовпадение и откат позиции */
    test_sub("subtest %d: dsExpect mismatch restores position", ++subnum);
    {
        const char *text = "hello";
        DS ds = dsCreateconst(text);
        size_t save = ds.pos;

        test_validate(!dsExpect(&ds, "help"),
                      "must return false for mismatching prefix");
        test_validate(ds.pos == save,
                      "pos must be restored to %zu, got %zu", save, ds.pos);
    }

    /* 3. DS_CONSTSTR: литерал длиннее остатка */
    test_sub("subtest %d: dsExpect longer than available", ++subnum);
    {
        const char *text = "hi";
        DS ds = dsCreateconst(text);

        test_validate(!dsExpect(&ds, "hello"),
                      "must return false when literal longer than data");
        test_validate(ds.pos == 0, "pos must remain 0, got %zu", ds.pos);
    }

    /* 4. DS_STR: совпадение и позиция */
    test_sub("subtest %d: dsExpect success on STR", ++subnum);
    {
        char buf[16] = "world";
        DS ds = dsCreatestrCap(buf, sizeof(buf));

        test_validate(dsExpect(&ds, "world"),
                      "must return true");
        test_validate(ds.pos == 5, "pos must be 5, got %zu", ds.pos);
    }

    /* 5. DS_FILE: совпадение и позиция */
    test_sub("subtest %d: dsExpect on FILE", ++subnum);
    {
        const char *fname = "res/ds/dsexpect_file.tmp";
        FILE *fp = fopen(fname, "w+");
        test_validatefree(fp != NULL, fclose(fp), "can't open file");

        fputs("hello", fp);
        rewind(fp);

        DS ds = dsCreatef(fp);
        test_validatefree(dsExpect(&ds, "hel"),
                          fclose(fp),
                          "must match 'hel'");
        test_validatefree(ftell(fp) == 3,
                          fclose(fp),
                          "file pos must be 3, got %ld", ftell(fp));
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 6. DS_FILE: несовпадение и откат */
    test_sub("subtest %d: dsExpect mismatch restores file pos", ++subnum);
    {
        const char *fname = "res/ds/dsexpect_file_mismatch.ds";
        FILE *fp = fopen(fname, "w+");
        test_validatefree(fp != NULL, fclose(fp), "can't open file");

        fputs("hello", fp);
        rewind(fp);

        DS ds = dsCreatef(fp);
        test_validatefree(!dsExpect(&ds, "help"),
                          fclose(fp),
                          "must return false");
        test_validatefree(ftell(fp) == 0,
                          fclose(fp),
                          "file pos must be restored to 0, got %ld", ftell(fp));
        fclose(fp);
        fs_alloc_check(true);
    }
    /* 8. DS_FS: несовпадение и восстановление позиции */
    test_sub("subtest %d: dsExpect mismatch on FS restores position", ++subnum);
    {
        fs s = fscopy("hello");
        DS ds = dsCreatefs(&s);
        size_t save = ds.pos;

        test_validatefree(!dsExpect(&ds, "help"),
                          dsFree(&ds),
                          "must return false");
        test_validatefree(ds.pos == save,
                          dsFree(&ds),
                          "pos must be restored to %zu, got %zu", save, ds.pos);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 9. DS_FS: литерал длиннее остатка */
    test_sub("subtest %d: dsExpect longer than FS data", ++subnum);
    {
        fs s = fscopy("hi");
        DS ds = dsCreatefs(&s);

        test_validatefree(!dsExpect(&ds, "hello"),
                          dsFree(&ds),
                          "must return false");
        test_validatefree(ds.pos == 0,
                          dsFree(&ds),
                          "pos must remain 0, got %zu", ds.pos);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 10. DS_FS: пустой литерал */
    test_sub("subtest %d: dsExpect empty literal on FS", ++subnum);
    {
        fs s = fscopy("hello");
        DS ds = dsCreatefs(&s);

        test_validatefree(dsExpect(&ds, ""),
                          dsFree(&ds),
                          "empty literal must always match");
        test_validatefree(ds.pos == 0,
                          dsFree(&ds),
                          "pos must remain 0, got %zu", ds.pos);
        dsFree(&ds);
        fs_alloc_check(true);
    }

#ifndef NO_FSDS

    /* 7. DS_FS: совпадение и позиция */
    test_sub("subtest %d: dsExpect on FS", ++subnum);
    {
        fs s = fscopy("hello");
        DS ds = dsCreatefs(&s);   // владение переходит в ds

        test_validatefree(dsExpect(&ds, "hello"),
                          dsFree(&ds),
                          "must match 'hello'");
        test_validatefree(ds.pos == 5,
                          dsFree(&ds),
                          "pos must be 5, got %zu", ds.pos);
        dsFree(&ds);
        fs_alloc_check(true);
    }

#endif  /* !NO_FSDS */  

    /* 8. Пустой литерал */
    test_sub("subtest %d: dsExpect empty literal", ++subnum);
    {
        const char *text = "hello";
        DS ds = dsCreateconst(text);

        test_validate(dsExpect(&ds, ""),
                      "empty literal must always match");
        test_validate(ds.pos == 0, "pos must remain 0, got %zu", ds.pos);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsGetcap -------------------------
static TestStatus
tf8_ds_getsize(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_STR с явной ёмкостью */
    test_sub("subtest %d: dsGetcap on DS_STR with cap", ++subnum);
    {
        char buffer[20] = "hello";
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));   // cap = 20
        size_t size = dsGetcap(&ds);
        test_validate(size == sizeof(buffer),
                      "expected %zu, got %zu", sizeof(buffer), size);
    }

    /* 2. DS_STR без ёмкости (dsCreatestr) */
    test_sub("subtest %d: dsGetcap on DS_STR without cap", ++subnum);
    {
        char buffer[20] = "hello";
        DS ds = dsCreatestr(buffer);                     // cap = 0, strlen = 5
        size_t size = dsGetcap(&ds);
        test_validate(size == strlen(buffer),
                      "expected %zu, got %zu", strlen(buffer), size);
    }

    /* 3. DS_CONSTSTR */
    test_sub("subtest %d: dsGetcap on DS_CONSTSTR", ++subnum);
    {
        const char *text = "constant string";
        DS ds = dsCreateconst(text);
        size_t size = dsGetcap(&ds);
        test_validate(size == strlen(text),
                      "expected %zu, got %zu", strlen(text), size);
    }

    /* 4. DS_FILE */
    test_sub("subtest %d: dsGetcap on DS_FILE", ++subnum);
    {
        const char *fname = "res/ds/ds_getsize_file.ds";
        FILE *fp = fopen(fname, "w+");
        test_validatefree(fp != NULL, fclose(fp), "can't create file");

        const char *data = "file data";
        fwrite(data, 1, strlen(data), fp);
        fflush(fp);

        DS ds = dsCreatef(fp);
        size_t size = dsGetcap(&ds);
        test_validatefree(size == strlen(data),
                          dsFree(&ds),
                          "expected %zu, got %zu", strlen(data), size);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 5. DS_FS */
    test_sub("subtest %d: dsGetcap on DS_FS", ++subnum);
    {
        fs s = fscopy("fs content");
        DS ds = dsCreatefs(&s);   // владение переходит в ds

        size_t size = dsGetcap(&ds);
        test_validatefree(size == strlen("fs content"),
                          dsFree(&ds),
                          "expected %zu, got %zu", strlen("fs content"), size);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: dsGetcap on DS_FS empty", ++subnum);
    {
        fs s = fscopy("");
        DS ds = dsCreatefs(&s);   // владение переходит в ds

        size_t size = dsGetcap(&ds);
        test_validatefree(size == 0L,
                          dsFree(&ds),
                          "expected %zu, got %zu", strlen("fs content"), size);
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 6. NULL входной параметр */
    test_sub("subtest %d: dsGetcap(NULL)", ++subnum);
    {
        if (!try()) {
            dsGetcap(NULL);
            test_validate(false, "must raise error");
        } else {
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsGetpos -------------------------
static TestStatus
tf9_ds_getpos(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_CONSTSTR: начальная позиция и после чтения */
    test_sub("subtest %d: dsGetpos on CONSTSTR", ++subnum);
    {
        const char *text = "hello";
        DS ds = dsCreateconst(text);

        off_t p0 = dsGetpos(&ds);
        test_validate(p0 == 0, "initial pos expected 0, got %lld", (long long)p0);

        dsgetc(&ds);
        off_t p1 = dsGetpos(&ds);
        test_validate(p1 == 1, "after one read expected 1, got %lld", (long long)p1);
    }

    /* 2. DS_STR с ёмкостью: позиция после записи */
    test_sub("subtest %d: dsGetpos on DS_STR with writes", ++subnum);
    {
        char buffer[16] = "abc";
        DS ds = dsCreatestrCap(buffer, sizeof(buffer));

        off_t p0 = dsGetpos(&ds);
        test_validate(p0 == 0, "initial pos expected 0, got %lld", (long long)p0);

        dsputc('X', &ds);
        off_t p1 = dsGetpos(&ds);
        test_validate(p1 == 1, "after one write expected 1, got %lld", (long long)p1);

        dsputc('Y', &ds);
        off_t p2 = dsGetpos(&ds);
        test_validate(p2 == 2, "after two writes expected 2, got %lld", (long long)p2);
    }

    /* 3. DS_FS: позиция после записи */
    test_sub("subtest %d: dsGetpos on DS_FS", ++subnum);
    {
        fs s = FS();
        DS ds = dsCreatefs(&s);   // владение переходит в ds

        off_t p0 = dsGetpos(&ds);
        test_validatefree(p0 == 0, dsFree(&ds),
                          "initial pos expected 0, got %lld", (long long)p0);

        dsputc('A', &ds);
        off_t p1 = dsGetpos(&ds);
        test_validatefree(p1 == 1, dsFree(&ds),
                          "after write expected 1, got %lld", (long long)p1);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: dsGetpos on DS_FILE", ++subnum);
    {
        const char *fname = "res/ds/ds_getpos_file.tmp";
        FILE *fp = fopen(fname, "w+");
        test_validatefree(fp != NULL, fclose(fp), "can't open file");

        fputs("hello", fp);
        fflush(fp);
        rewind(fp);

        DS ds = dsCreatef(fp);
        off_t p0 = dsGetpos(&ds);
        test_validatefree(p0 == 0, fclose(fp),
                        "initial file pos expected 0, got %lld", (long long)p0);

        dsgetc(&ds);
        off_t p1 = dsGetpos(&ds);
        test_validatefree(p1 == 1, fclose(fp),
                        "after one read expected 1, got %lld", (long long)p1);

        // Перед записью синхронизируем позицию (требование стандарта C)
        fseek(fp, p1, SEEK_SET);
        dsputc('Z', &ds);
        off_t p2 = dsGetpos(&ds);
        test_validatefree(p2 == 2, fclose(fp),
                        "after write expected 2, got %lld", (long long)p2);

        fclose(fp);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsSkipNl (corrected) -------------------------
static TestStatus
tf10_ds_skip_nl(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Символ '\n' — потребляется, возвращается true, позиция сдвигается */
    test_sub("subtest %d: dsSkipNl consumes newline on DS_STR", ++subnum);
    {
        char buf[] = "ab\ncd";
        DS ds = dsCreatestrCap(buf, sizeof(buf));

        // Читаем 'a' и 'b', встаём на '\n'
        int c1 = dsgetc(&ds);
        int c2 = dsgetc(&ds);
        test_validate(c1 == 'a' && c2 == 'b', "wrong chars: %c %c", c1, c2);
        test_validate(ds.pos == 2, "pos must be 2, got %zu", ds.pos);

        bool res = dsSkipNl(&ds);
        test_validate(res == true, "expected true");
        test_validate(ds.pos == 3, "pos must advance to 3, got %zu", ds.pos);
        int c = dsgetc(&ds);
        test_validate(c == 'c', "next char must be 'c', got '%c'", c);
    }

    /* 2. Не '\n' символ — возвращается обратно, false, позиция не меняется */
    test_sub("subtest %d: dsSkipNl leaves non-newline on DS_STR", ++subnum);
    {
        char buf[] = "abc";
        DS ds = dsCreatestrCap(buf, sizeof(buf));

        // Читаем 'a', встаём на 'b'
        int c1 = dsgetc(&ds);
        test_validate(c1 == 'a', "wrong char: %c", c1);
        test_validate(ds.pos == 1, "pos must be 1, got %zu", ds.pos);

        bool res = dsSkipNl(&ds);
        test_validate(res == false, "expected false for non-newline");
        test_validate(ds.pos == 1, "pos must remain 1, got %zu", ds.pos);
        int c = dsgetc(&ds);
        test_validate(c == 'b', "next char must still be 'b', got '%c'", c);
    }

    /* 3. EOF — возвращает false */
    test_sub("subtest %d: dsSkipNl at EOF on DS_STR", ++subnum);
    {
        char buf[] = "ab\n";
        DS ds = dsCreatestrCap(buf, sizeof(buf));

        // Читаем 'a','b','\n'
        dsgetc(&ds);
        dsgetc(&ds);
        dsgetc(&ds);   // теперь на EOF
        bool res = dsSkipNl(&ds);
        test_validate(res == false, "expected false at EOF");
        test_validate(ds.pos == 3, "pos must remain 3, got %zu", ds.pos);
    }

    /* 4. DS_CONSTSTR: проверка */
    test_sub("subtest %d: dsSkipNl on DS_CONSTSTR", ++subnum);
    {
        const char *buf = "x\ny";
        DS ds = dsCreateconst(buf);

        // Читаем 'x', встаём на '\n'
        int c = dsgetc(&ds);
        test_validate(c == 'x', "wrong char");
        bool res = dsSkipNl(&ds);
        test_validate(res == true, "expected true");
        test_validate(ds.pos == 2, "pos must advance to 2, got %zu", ds.pos);
        c = dsgetc(&ds);
        test_validate(c == 'y', "next char must be 'y'");
    }

    /* 5. DS_FS: проверка */
    test_sub("subtest %d: dsSkipNl on DS_FS", ++subnum);
    {
        fs s = fscopy("q\nr");
        DS ds = dsCreatefs(&s);

        // Читаем 'q', встаём на '\n'
        int c = dsgetc(&ds);
        test_validatefree(c == 'q', dsFree(&ds), "wrong char");
        bool res = dsSkipNl(&ds);
        test_validatefree(res == true, dsFree(&ds), "expected true");
        test_validatefree(ds.pos == 2, dsFree(&ds), "pos must advance to 2, got %zu", ds.pos);
        c = dsgetc(&ds);
        test_validatefree(c == 'r', dsFree(&ds), "next char must be 'r'");
        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 6. DS_FILE: на файле */
    test_sub("subtest %d: dsSkipNl on DS_FILE", ++subnum);
    {
        const char *fname = "res/ds/ds_skip_nl_file.tmp";
        FILE *fp = fopen(fname, "w+");
        test_validatefree(fp != NULL, fclose(fp), "can't open file");
        fputs("ab\ncd", fp);
        fflush(fp);
        rewind(fp);

        DS ds = dsCreatef(fp);
        // Читаем 'a','b'
        dsgetc(&ds);
        dsgetc(&ds);
        bool res = dsSkipNl(&ds);
        test_validatefree(res == true, fclose(fp), "expected true");
        test_validatefree(ftell(fp) == 3, fclose(fp), "file pos must be 3, got %ld", ftell(fp));
        int c = dsgetc(&ds);
        test_validatefree(c == 'c', fclose(fp), "next char must be 'c'");
        fclose(fp);
        fs_alloc_check(true);
    }

    /* 7. Цикл while (dsSkipNl(&ds)); проматывает несколько \n */
    test_sub("subtest %d: dsSkipNl loop skips multiple newlines", ++subnum);
    {
        char buf[] = "a\n\n\nb";
        DS ds = dsCreatestrCap(buf, sizeof(buf));

        // читаем 'a'
        int c = dsgetc(&ds);
        test_validate(c == 'a', "wrong first char");

        // проматываем все \n
        while (dsSkipNl(&ds))
            ;

        // теперь должны стоять на 'b'
        test_validate(ds.pos == 4, "pos must be 4, got %zu", ds.pos);
        c = dsgetc(&ds);
        test_validate(c == 'b', "next char must be 'b', got '%c'", c);
    }

    /* 8. Цикл while (dsSkipNl(&ds)); останавливается на EOF */
    test_sub("subtest %d: dsSkipNl loop stops at EOF", ++subnum);
    {
        char buf[] = "a\n";
        DS ds = dsCreatestrCap(buf, sizeof(buf));

        int c = dsgetc(&ds);   // 'a'
        test_validate(c == 'a', "wrong first char");

        while (dsSkipNl(&ds))
            ;

        test_validate(ds.pos == 2, "pos must be 2 at EOF");
        c = dsgetc(&ds);
        test_validate(c == EOF, "must be EOF");
    }

    /* 9. Цикл while (dsSkipNl(&ds)); останавливается на значащем символе */
    test_sub("subtest %d: dsSkipNl loop stops at non-newline", ++subnum);
    {
        char buf[] = "x\nabc";
        DS ds = dsCreatestrCap(buf, sizeof(buf));

        int c = dsgetc(&ds);   // 'x'
        test_validate(c == 'x', "wrong first char");

        // пропускаем один \n, затем цикл должен остановиться на 'a'
        bool res = dsSkipNl(&ds);
        test_validate(res == true, "first skip must be true");
        while (dsSkipNl(&ds))
            ;

        test_validate(ds.pos == 2, "pos must be 2, got %zu", ds.pos);
        c = dsgetc(&ds);
        test_validate(c == 'a', "next char must be 'a', got '%c'", c);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsSkipSpace -------------------------
static TestStatus
tf11_ds_skipspace(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ============ DS_FILE ============ */
    test_sub("subtest %d: DS_FILE - skip leading spaces", ++subnum);
    {
        FILE *f = fmemopen((void*)"   hello", 8, "r");
        DS ds = dsCreatef(f);
        test_validatefree(ds.type == DS_FILE, (dsFree(&ds)), "failed to create DS_FILE");

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds); // возвращаем 'h', поток снова "hello"

        test_validatefree(dsExpect(&ds, "hello"), (dsFree(&ds)), "remaining mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_FILE - no leading spaces", ++subnum);
    {
        FILE *f = fmemopen((void*)"hello", 5, "r");
        DS ds = dsCreatef(f);

        bool skipped = dsSkipSpace(&ds);
        test_validatefree(!skipped, (dsFree(&ds)), "expected false, got true");

        // позиция не должна измениться (ftell == 0)
        test_validatefree(ftell(f) == 0, (dsFree(&ds)), "ftell should be 0");

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_FILE - only spaces -> EOF", ++subnum);
    {
        FILE *f = fmemopen((void*)"   \t\n", 5, "r");
        DS ds = dsCreatef(f);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == EOF, (dsFree(&ds)), "expected EOF, got %d", c);

        // можно проверить, что находимся в конце файла
        test_validatefree(feof(f) != 0, (dsFree(&ds)), "expected feof");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_FILE - empty string", ++subnum);
    {
        FILE *f = tmpfile();
        DS ds = dsCreatef(f);

        bool skipped = dsSkipSpace(&ds);
        test_validatefree(!skipped, (dsFree(&ds)), "expected false, got true");
        test_validatefree(ftell(f) == 0, (dsFree(&ds)), "ftell should be 0");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_FILE - mixed whitespace", ++subnum);
    {
        const char *teststr = " \t\n hello";
        FILE *f = fmemopen((void*)teststr, strlen(teststr), "r");
        DS ds = dsCreatef(f);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        test_validatefree(dsExpect(&ds, "hello"), (dsFree(&ds)), "remaining mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* ============ DS_STR ============ */
    test_sub("subtest %d: DS_STR - skip leading spaces", ++subnum);
    {
        char buf[] = "   hello";
        DS ds = dsCreatestr(buf);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        test_validatefree(dsExpect(&ds, "hello"), (dsFree(&ds)), "remaining mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_STR - no leading spaces", ++subnum);
    {
        char buf[] = "hello";
        DS ds = dsCreatestr(buf);

        bool skipped = dsSkipSpace(&ds);
        test_validatefree(!skipped, (dsFree(&ds)), "expected false, got true");
        test_validatefree(ds.pos == 0, (dsFree(&ds)), "pos should be 0, got %zu", ds.pos);

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_STR - only spaces -> EOF", ++subnum);
    {
        char buf[] = "   \t\n";
        DS ds = dsCreatestr(buf);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == EOF, (dsFree(&ds)), "expected EOF, got %d", c);
        test_validatefree(ds.pos == strlen(buf), (dsFree(&ds)), "pos expected %zu, got %zu", strlen(buf), ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_STR - empty string", ++subnum);
    {
        char buf[] = "";
        DS ds = dsCreatestr(buf);

        bool skipped = dsSkipSpace(&ds);
        test_validatefree(!skipped, (dsFree(&ds)), "expected false, got true");
        test_validatefree(ds.pos == 0, (dsFree(&ds)), "pos expected 0, got %zu", ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_STR - mixed whitespace", ++subnum);
    {
        char buf[] = " \t\n hello";
        DS ds = dsCreatestr(buf);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        test_validatefree(dsExpect(&ds, "hello"), (dsFree(&ds)), "remaining mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* ============ DS_CONSTSTR ============ */
    test_sub("subtest %d: DS_CONSTSTR - skip leading spaces", ++subnum);
    {
        const char *buf = "   hello";
        DS ds = dsCreateconst(buf);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        test_validatefree(dsExpect(&ds, "hello"), (dsFree(&ds)), "remaining mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_CONSTSTR - no leading spaces", ++subnum);
    {
        const char *buf = "hello";
        DS ds = dsCreateconst(buf);

        bool skipped = dsSkipSpace(&ds);
        test_validatefree(!skipped, (dsFree(&ds)), "expected false, got true");
        test_validatefree(ds.pos == 0, (dsFree(&ds)), "pos should be 0, got %zu", ds.pos);

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_CONSTSTR - only spaces -> EOF", ++subnum);
    {
        const char *buf = "   \t\n";
        DS ds = dsCreateconst(buf);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == EOF, (dsFree(&ds)), "expected EOF, got %d", c);
        test_validatefree(ds.pos == strlen(buf), (dsFree(&ds)), "pos expected %zu, got %zu", strlen(buf), ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_CONSTSTR - empty string", ++subnum);
    {
        const char *buf = "";
        DS ds = dsCreateconst(buf);

        bool skipped = dsSkipSpace(&ds);
        test_validatefree(!skipped, (dsFree(&ds)), "expected false, got true");
        test_validatefree(ds.pos == 0, (dsFree(&ds)), "pos expected 0, got %zu", ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_CONSTSTR - mixed whitespace", ++subnum);
    {
        const char *buf = " \t\n hello";
        DS ds = dsCreateconst(buf);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        test_validatefree(dsExpect(&ds, "hello"), (dsFree(&ds)), "remaining mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* ============ DS_FS ============ */
    test_sub("subtest %d: DS_FS - skip leading spaces", ++subnum);
    {
        fs src = fscopy("   hello");
        DS ds = dsCreatefs(&src);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        test_validatefree(dsExpect(&ds, "hello"), (dsFree(&ds)), "remaining mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_FS - no leading spaces", ++subnum);
    {
        fs src = fscopy("hello");
        DS ds = dsCreatefs(&src);

        bool skipped = dsSkipSpace(&ds);
        test_validatefree(!skipped, (dsFree(&ds)), "expected false, got true");
        test_validatefree(ds.pos == 0, (dsFree(&ds)), "pos should be 0, got %zu", ds.pos);

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_FS - only spaces -> EOF", ++subnum);
    {
        fs src = fscopy("   \t\n");
        DS ds = dsCreatefs(&src);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == EOF, (dsFree(&ds)), "expected EOF, got %d", c);
        test_validatefree(ds.pos == ds.s.len, (dsFree(&ds)), "pos expected %zu, got %zu", ds.s.len, ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_FS - empty string", ++subnum);
    {
        fs src = fscopy("");
        DS ds = dsCreatefs(&src);

        bool skipped = dsSkipSpace(&ds);
        test_validatefree(!skipped, (dsFree(&ds)), "expected false, got true");
        test_validatefree(ds.pos == 0, (dsFree(&ds)), "pos expected 0, got %zu", ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: DS_FS - mixed whitespace", ++subnum);
    {
        fs src = fscopy(" \t\n hello");
        DS ds = dsCreatefs(&src);

        while (dsSkipSpace(&ds));

        int c = dsgetc(&ds);
        test_validatefree(c == 'h', (dsFree(&ds)), "expected 'h', got '%c'", c);
        dsungetc(c, &ds);

        test_validatefree(dsExpect(&ds, "hello"), (dsFree(&ds)), "remaining mismatch");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* ============ NULL argument ============ */
    test_sub("subtest %d: NULL DS raises error", ++subnum);
    {
        if (!try()) {
            dsSkipSpace(NULL);
            test_validate(false, "must raise error for NULL DS");
        } else {
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST dsgetcEscaped -------------------------
static TestStatus
tf12_ds_getc_ecran(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. DS_STR: все корректные escape-последовательности */
    test_sub("subtest %d: DS_STR - valid escapes", ++subnum);
    {
        char buf[] = "\\\"nrt";
        DS ds = dsCreatestr(buf);
        int c;

        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\\', (dsFree(&ds)), "backslash failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '"',  (dsFree(&ds)), "quote failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\n', (dsFree(&ds)), "newline failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\r', (dsFree(&ds)), "carriage return failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\t', (dsFree(&ds)), "tab failed");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 2. DS_STR: неверный escape символ возвращается обратно */
    test_sub("subtest %d: DS_STR - invalid escape char is ungot", ++subnum);
    {
        char buf[] = "x";
        DS ds = dsCreatestr(buf);
        int c = 0;
        bool res = dsgetcEscaped(&ds, &c);
        test_validatefree(!res, (dsFree(&ds)), "expected false, got true");

        int ch = dsgetc(&ds);
        test_validatefree(ch == 'x', (dsFree(&ds)), "expected 'x' after unget, got '%c'", ch);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 3. DS_STR: EOF (пустая строка) */
    test_sub("subtest %d: DS_STR - EOF returns false", ++subnum);
    {
        char buf[] = "";
        DS ds = dsCreatestr(buf);
        int c = 0;
        bool res = dsgetcEscaped(&ds, &c);
        test_validatefree(!res, (dsFree(&ds)), "expected false, got true");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 4. DS_STR: NULL c — просто потребляет корректный символ */
    test_sub("subtest %d: DS_STR - NULL c consumes valid escape", ++subnum);
    {
        char buf[] = "n";
        DS ds = dsCreatestr(buf);
        bool res = dsgetcEscaped(&ds, NULL);
        test_validatefree(res, (dsFree(&ds)), "expected true for valid escape");

        // Позиция должна продвинуться на 1
        test_validatefree(ds.pos == 1, (dsFree(&ds)), "pos expected 1, got %zu", ds.pos);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 5. DS_CONSTSTR: все корректные escape */
    test_sub("subtest %d: DS_CONSTSTR - valid escapes", ++subnum);
    {
        const char *buf = "\\\"nrt";
        DS ds = dsCreateconst(buf);
        int c;

        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\\', (dsFree(&ds)), "backslash failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '"',  (dsFree(&ds)), "quote failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\n', (dsFree(&ds)), "newline failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\r', (dsFree(&ds)), "carriage return failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\t', (dsFree(&ds)), "tab failed");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 6. DS_FS: все корректные escape */
    test_sub("subtest %d: DS_FS - valid escapes", ++subnum);
    {
        fs src = fscopy("\\\"nrt");
        DS ds = dsCreatefs(&src);
        int c;

        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\\', (dsFree(&ds)), "backslash failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '"',  (dsFree(&ds)), "quote failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\n', (dsFree(&ds)), "newline failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\r', (dsFree(&ds)), "carriage return failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\t', (dsFree(&ds)), "tab failed");

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 7. DS_FS: неверный escape возвращается обратно */
    test_sub("subtest %d: DS_FS - invalid escape char is ungot", ++subnum);
    {
        fs src = fscopy("q");
        DS ds = dsCreatefs(&src);
        int c = 0;
        bool res = dsgetcEscaped(&ds, &c);
        test_validatefree(!res, (dsFree(&ds)), "expected false, got true");

        int ch = dsgetc(&ds);
        test_validatefree(ch == 'q', (dsFree(&ds)), "expected 'q' after unget, got '%c'", ch);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 8. DS_FILE: все корректные escape (через tmpfile) */
    test_sub("subtest %d: DS_FILE - valid escapes", ++subnum);
    {
        const char filename[] = "res/ds/dsgetcEscape_valid_file.ds";
        FILE *fp = fopen(filename, "w+");
        test_validate(fp != NULL, "failed to create temporary file");
        fputs("\\\"nrt", fp);
        rewind(fp);

        DS ds = dsCreatef(fp);
        int c;

        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\\', (dsFree(&ds)), "backslash failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '"',  (dsFree(&ds)), "quote failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\n', (dsFree(&ds)), "newline failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\r', (dsFree(&ds)), "carriage return failed");
        test_validatefree(dsgetcEscaped(&ds, &c) && c == '\t', (dsFree(&ds)), "tab failed");

        dsFree(&ds);   // закроет файл
        fs_alloc_check(true);
    }

    /* 9. DS_FILE: неверный escape возвращается обратно */
    test_sub("subtest %d: DS_FILE - invalid escape char is ungot", ++subnum);
    {
        const char filename[] = "res/ds/dsgetcEscape_wrong_file.ds";
        FILE *fp = fopen(filename, "w+");
        test_validate(fp != NULL, "failed to create temporary file");
        fputc('z', fp);
        rewind(fp);

        DS ds = dsCreatef(fp);
        int c = 0;
        bool res = dsgetcEscaped(&ds, &c);
        test_validatefree(!res, (dsFree(&ds)), "expected false, got true");

        int ch = dsgetc(&ds);
        test_validatefree(ch == 'z', (dsFree(&ds)), "expected 'z' after unget, got '%c'", ch);

        dsFree(&ds);
        fs_alloc_check(true);
    }

    /* 10. NULL DS должен вызывать ошибку */
    test_sub("subtest %d: NULL DS raises error", ++subnum);
    {
        if (!try()) {
            dsgetcEscaped(NULL, NULL);
            test_validate(false, "must raise error for NULL DS");
        } else {
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// // ------------------------- TEST dsparseEscaped -------------------------
// static TestStatus
// tf13_dsparseEscaped(const char *name)
// {
//     logenter("%s", name);
//     int subnum = 0;

//     /* 1. DS_STR: обычный символ без слэша */
//     test_sub("subtest %d: DS_STR - ordinary char", ++subnum);
//     {
//         char buf[] = "a";
//         DS ds = dsCreatestr(buf);
//         bool err = false;
//         int c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == 'a' && !err, (dsFree(&ds)), "expected 'a', got %d, err=%d", c, err);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 2. DS_STR: все корректные escape-последовательности */
//     test_sub("subtest %d: DS_STR - valid escapes", ++subnum);
//     {
//         char buf[] = {'\\', '\\', '"', '\\', 'n', '\\', 'r', '\\', 't', '\0'};   // символы: \ " n r t
//         DS ds = dsCreatestr(buf);
//         bool err = false;
//         int c;

//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\\' && !err, (dsFree(&ds)), "backslash failed");
//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '"'  && !err, (dsFree(&ds)), "quote failed");
//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\n' && !err, (dsFree(&ds)), "newline failed");
//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\r' && !err, (dsFree(&ds)), "carriage return failed");
//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\t' && !err, (dsFree(&ds)), "tab failed");

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 3. DS_STR: некорректный escape -> EOF, error=true, символ возвращается */
//     test_sub("subtest %d: DS_STR - invalid escape", ++subnum);
//     {
//         char buf[] = "\\x";
//         DS ds = dsCreatestr(buf);
//         bool err = false;
//         int c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == EOF && err, (dsFree(&ds)), "expected EOF and error, got c=%d err=%d", c, err);

//         int ch = dsgetc(&ds);
//         test_validatefree(ch == 'x', (dsFree(&ds)), "expected 'x' after unget, got '%c'", ch);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 4. DS_STR: EOF (пустая строка) -> EOF, error=false */
//     test_sub("subtest %d: DS_STR - EOF", ++subnum);
//     {
//         char buf[] = "";
//         DS ds = dsCreatestr(buf);
//         bool err = false;
//         int c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == EOF && !err, (dsFree(&ds)), "expected EOF, got c=%d err=%d", c, err);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 5. DS_STR: error == NULL, корректный escape */
//     test_sub("subtest %d: DS_STR - error NULL, valid escape", ++subnum);
//     {
//         char buf[] = "\\n";
//         DS ds = dsCreatestr(buf);
//         int c = dsparseEscaped(&ds, NULL);
//         test_validatefree(c == '\n', (dsFree(&ds)), "expected newline, got %d", c);

//         // проверяем, что позиция продвинулась на 2 (слэш и 'n')
//         test_validatefree(ds.pos == 2, (dsFree(&ds)), "pos expected 2, got %zu", ds.pos);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 6. DS_STR: error == NULL, некорректный escape */
//     test_sub("subtest %d: DS_STR - error NULL, invalid escape", ++subnum);
//     {
//         char buf[] = "\\z";
//         DS ds = dsCreatestr(buf);
//         int c = dsparseEscaped(&ds, NULL);
//         test_validatefree(c == EOF, (dsFree(&ds)), "expected EOF, got %d", c);

//         int ch = dsgetc(&ds);
//         test_validatefree(ch == 'z', (dsFree(&ds)), "expected 'z' after unget, got '%c'", ch);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 7. DS_CONSTSTR: обычный символ */
//     test_sub("subtest %d: DS_CONSTSTR - ordinary char", ++subnum);
//     {
//         const char *buf = "h";
//         DS ds = dsCreateconst(buf);
//         bool err = false;
//         int c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == 'h' && !err, (dsFree(&ds)), "expected 'h', got %d", c);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 8. DS_CONSTSTR: корректный и некорректный escape */
//     test_sub("subtest %d: DS_CONSTSTR - mixed escapes", ++subnum);
//     {
//         const char *buf = "\\n\\q";
//         DS ds = dsCreateconst(buf);
//         bool err = false;
//         int c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\n' && !err, (dsFree(&ds)), "newline failed");

//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == EOF && err, (dsFree(&ds)), "expected invalid escape error");

//         // после ошибки 'q' должна быть в потоке
//         int ch = dsgetc(&ds);
//         test_validatefree(ch == 'q', (dsFree(&ds)), "expected 'q', got '%c'", ch);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 9. DS_FS: обычный символ */
//     test_sub("subtest %d: DS_FS - ordinary char", ++subnum);
//     {
//         fs src = fscopy("b");
//         DS ds = dsCreatefs(&src);
//         bool err = false;
//         int c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == 'b' && !err, (dsFree(&ds)), "expected 'b', got %d", c);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 10. DS_FS: все корректные escape */
//     test_sub("subtest %d: DS_FS - valid escapes", ++subnum);
//     {
//         fs src = fscopy((const char[]){'\\', '\\', '\\', '"', '\\', 'n', '\\', 'r', '\\', 't', '\0'});
//         DS ds = dsCreatefs(&src);
//         bool err = false;
//         int c;

//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\\' && !err, (dsFree(&ds)), "backslash failed");
//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '"'  && !err, (dsFree(&ds)), "quote failed");
//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\n' && !err, (dsFree(&ds)), "newline failed");
//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\r' && !err, (dsFree(&ds)), "carriage return failed");
//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\t' && !err, (dsFree(&ds)), "tab failed");

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//         /* 11. DS_FILE: обычный символ и escape (используем обычный файл) */
//     test_sub("subtest %d: DS_FILE - mixed escapes", ++subnum);
//     {
//         const char *path = "res/ds/dsparse_ecraned_file.ds";
//         FILE *fp = fopen(path, "w");
//         test_validate(fp != NULL, "failed to create file for test");
//         fputs("a\\n\\x", fp);   // обычный 'a', затем валидный \n, затем невалидный \x
//         fclose(fp);

//         DS ds = dsCreateFilename(path, "r");
//         test_validatefree(ds.type == DS_FILE, (dsFree(&ds)), "failed to open DS_FILE");

//         bool err = false;
//         int c;

//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == 'a' && !err, (dsFree(&ds)), "ordinary char failed");

//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == '\n' && !err, (dsFree(&ds)), "newline failed");

//         c = dsparseEscaped(&ds, &err);
//         test_validatefree(c == EOF && err, (dsFree(&ds)), "invalid escape failed");

//         int ch = dsgetc(&ds);
//         test_validatefree(ch == 'x', (dsFree(&ds)), "expected 'x' after unget, got '%c'", ch);

//         dsFree(&ds);
//         fs_alloc_check(true);
//     }

//     /* 12. NULL DS должен вызывать ошибку */
//     test_sub("subtest %d: NULL DS raises error", ++subnum);
//     {
//         if (!try()) {
//             dsparseEscaped(NULL, NULL);
//             test_validate(false, "must raise error for NULL DS");
//         } else {
//             test_validate(true, "correctly raised error");
//         }
//         fs_alloc_check(true);
//     }

//     return logret(TEST_PASSED, "done");
// }

// -------------------------------------------------------------------
int
main( /*int argc, char *argv[] */ )
{
    logsimpleinit("Start");

    testenginestd(
        TESTADD(tf_ds,                   "DS (DataSource) simple tests")
      , TESTADD(tf_ds_extra,             "DS additional functions")
      , TESTADD(tf3_ds_putc,             "dsputc() simple test")
      , TESTADD(tf4_ds_fs,               "dsputc() / dsgetc() for DS_FS test")
      , TESTADD(tf5_ds_create_filename,  "dsCreateFilename/dsInitFilename simple test")
      , TESTADD(tf6_dswrite,             "dswrite() simple test")
      , TESTADD(tf7_ds_expect,           "dsExpect() simple test")
      , TESTADD(tf8_ds_getsize,          "dsGetcap() simple test")
      , TESTADD(tf9_ds_getpos,           "dsGetpos() simple test")
      , TESTADD(tf10_ds_skip_nl,         "dsSkipNl() simple test")
      , TESTADD(tf11_ds_skipspace,       "dsSkipSpaces() simple test")
      , TESTADD(tf12_ds_getc_ecran,      "dsgetcEscaped() simple test")
      //, TESTADD(tf13_dsparseEscaped,     "dsparseEscaped() simple test")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* DS_TESTING */
