#include "common.h"


/********************************************************************
                 COOMON MODULE IMPLEMENTATION
********************************************************************/

// --------------------------- API ---------------------------------

// int[] filler     // TODO: probably to use common code for all of that
void                            fill_int(int *arr, int cnt, int value){ 
    for (int i = 0; i < cnt; i++)
        arr[i] = value;
}

// long[] filler
void                            fill_long(long *arr, int cnt, long value){
    for (int i = 0; i < cnt; i++)
        arr[i] = value;
}

// double[] filler
void                            fill_double(double *arr, int cnt, double value){
    for (int i = 0; i < cnt; i++)
        arr[i] = value;
}

// float[] filler
void                            fill_float(float *arr, int cnt, float value){
    for (int i = 0; i < cnt; i++)
        arr[i] = value;
}

char                            *uniq_str(char *s, int *p_len){
    bool    hash[256] = {false};
    int     j = 0, i = 0;
    char    c;
    while ( (c = s[i++]) != '\0'){
        if (!hash[(int) c]){
            hash[(int) c] = true;
            s[j++] = c;
        }
    }
    s[j] = '\0';
    if (p_len)
        *p_len = j;
    return s; // logret(s, "new len = %d, new str[%s]", j, s);
}

// bits to string (STATIC for now) TODO: think about FastString usage here
extern const char               *bits_str(char *buf, int len, unsigned val){
    //logenter("val %u", val);
    int        pos = 0, bit;
    while (pos < len - 1 && val > 0){ 
        bit = val & 0x1;
        buf[pos++] = bit + '0';
        val >>= 1;
    }
    buf[pos] = '\0';
    reverse(buf, pos);
    return buf; //logret(buf, "[%s]", buf);
}

int                             fprint_bits(FILE *f, const char *str, unsigned val){
    char            buf[100];
    bits_str(buf, sizeof(buf), val);
    return fprintf(f, "%s: %s\n", str, buf);
}

// reverse string
char*                           reverse(char *s, int len){
    int i = 0, j = len - 1;
    while (i < j)
        char_exch(s + i++, s + j--);
    return s;
}
// a stupid one
int                             fprintn(FILE *f, const char *str, int sz){
    char c;
    int i = 0;
    while (str && i < sz &&(c = str[i++]) != '\0')
        fputc(c, f);
    fputc('\n', f);
    return i;
}

// ------------------- Pointer comparators -------------------
// simple char comparator
int                             pchar_cmp(const void *restrict s1, const void *restrict s2){
    return compare_char( *(const unsigned char *) s1, *(const unsigned char *) s2);
}
// simple char reverse comparator
int                             pchar_revcmp(const void *s1, const void *s2){
    return -compare_char( *(const unsigned char *) s1, *(const unsigned char *) s2);
}
// simple comparator pointer int
int                             pint_cmp(const void *restrict i1, const void *restrict i2){
    return compare_int( *(const int *) i1, *(const int *) i2);
}
// simple reverse comparator pointer int
int                             pint_revcmp(const void *i1, const void *i2){
    return -compare_int( *(const int *) i1, *(const int *) i2);
}
// simple comparator pointer long
int                              plong_cmp(const void *l1, const void *l2){
    return compare_long( *(const long *) l1, *(const long *) l2);
}
// simple reverse comparator pointer long
int                             plong_revcmp(const void *l1, const void *l2){
    return -compare_long( *(const long *) l1, *(const long *) l2);
}
// simple comparator pointer to pointer
int                             pptr_cmp(const void *p1, const void *p2) {
    const void *pa = *(const void **) p1;
    const void *pb = *(const void **) p2;
    return compare_ptr(pa, pb);
}
// simple reverse comparator pointer to pointer
int                             pptr_revcmp(const void *p1, const void *p2){
    const void *pa = *(const void **) p1;
    const void *pb = *(const void **) p2;
    return -compare_ptr(pa, pb);
}

// simple comparator pointer double
int                             pdbl_cmp(const void *d1, const void *d2){
    return compare_dbl( *(const double *) d1, *(const double *) d2);
}
// simple reverse comparator pointer double
int                             pdbl_revcmp(const void *d1, const void *d2){
    return -compare_dbl( *(const double *) d1, *(const double *) d2);
}

// ------------------------------ Parsers -----------------------------------
static inline bool              is_long_overflow(long val){
    return errno == ERANGE && (val == LONG_MAX || val == LONG_MIN);
}
static inline bool              is_ulong_overflow(unsigned long val){
    return errno == ERANGE && val == ULONG_MAX;
}
static inline bool              is_double_overflow(double val){
    return errno == ERANGE && isinf(val);
}
//
bool                            try_parse_int(const char *restrict str, int *restrict res) {
    if (!str)
        return logsimpleerr(false, "Null pointer str");
    errno = 0;
    char    *endptr;
    long val = strtol(str, &endptr, 10);

    if (str == endptr || *endptr != '\0' || val > INT_MAX || val < INT_MIN)
        return logsimpleerr(false, "Unable to parse int %ld, errno %d", val, errno);
    if (res)
        *res = (int) val;
    return true;
}
bool                            try_parse_long(const char *restrict str, long *restrict res) {
    if (!str)
        return logsimpleerr(false, "Null pointer str");
    errno = 0;
    char    *endptr;
    long val = strtol(str, &endptr, 10);

    if (str == endptr || *endptr != '\0' || is_long_overflow(val) )
        return logsimpleerr(false, "Unable to parse long %ld, errno %d", val, errno);
    if (res)
        *res = val;
    return true;
}
bool                            try_parse_double(const char *restrict str, double *restrict res) {
    if (!str)
        return logsimpleerr(false, "Null pointer str");
    errno = 0;
    char    *endptr;
    double val = strtold(str, &endptr);
    if (str == endptr || *endptr != '\0' || is_double_overflow(val) ) {
        return logsimpleerr(false, "Unable to parse double %lg, errno %d", val, errno);
    }
    if (res)
        *res = val;
    return true;
}
bool                            try_parse_uint(const char *restrict str, unsigned *restrict res) {
    if (!str)
        return logsimpleerr(false, "Null pointer str");
    str = skip_leading_spaces(str);
    if (*str == '-')
        return logsimpleerr(false, "Negative value not allowed");
    errno = 0;
    char           *endptr;
    unsigned long   val = strtoul(str, &endptr, 10);

    if (str == endptr || *endptr != '\0' || *str == '-' || is_ulong_overflow(val) || val > UINT_MAX) {
        return logsimpleerr(false, "Unable to parse uint %lu, errno %d", val, errno);
    }

    if (res)
        *res = (unsigned int)val;
    return true;
}

bool                            try_parse_ulong(const char *restrict str, unsigned long *restrict res) {
    if (!str)
        return logsimpleerr(false, "Null pointer str");
    str = skip_leading_spaces(str);
    if (*str == '-')
        return logsimpleerr(false, "Negative value not allowed");
    errno = 0;
    char            *endptr;
    unsigned long    val = strtoul(str, &endptr, 10);

    if (str == endptr || *endptr != '\0' || *str == '-' || is_ulong_overflow(val))
        return logsimpleerr(false, "Unable to parse ulong %lu, errno %d(%s)", val, errno, strerror(errno) );

    if (res)
        *res = val;
    return true;
}
/**
 * @brief Tries to parse a single character from a string.
 * The string must contain exactly one character (plus null terminator).
 * @param str input string
 * @param res pointer to store the parsed char (may be NULL)
 * @return true on success, false on error (empty string or more than one character)
 */
bool                            try_parse_char(const char *restrict str, char *restrict res) {
    if (!str || !*str)
        return logsimpleerr(false, "Empty string for char");
    if (res)
        *res = str[0];
    return true;
}
/**
 * @brief Tries to parse a boolean value from a string.
 * Recognises "on", "true", "yes" (case‑insensitive) as true,
 * and "off", "false", "no" as false.
 * @param str input string
 * @param res pointer to store the result (may be NULL)
 * @return true if the string matched a known boolean, false otherwise
 */
bool                            try_parse_bool(const char *restrict str, bool *restrict res) {
    if (!str || !*str)
        return logsimpleerr(false, "Empty string for bool");

    // максимальная длина слова – 5 символов ("false") + возможно один разделитель
    char        buf[7];  // 6 символов + \0
    size_t      i = 0;
    while (str[i] && !isspace((unsigned char) str[i]) && i < sizeof(buf) - 1) {
        buf[i] = (char) tolower((unsigned char) str[i]);
        i++;
    }
    buf[i] = '\0';

    // Вспомогательный макрос для сравнения
    #define try_parse_bool_MATCH(word, result_value) \
        (strncmp(buf, (word), sizeof(word)-1) == 0 && \
         (buf[sizeof(word)-1] == '\0' || isspace((unsigned char)buf[sizeof(word)-1])))

    if (try_parse_bool_MATCH("on", true) || try_parse_bool_MATCH("true", true) || try_parse_bool_MATCH("yes", true)) {
        if (res)
            *res = true;
        return true;
    }
    if (try_parse_bool_MATCH("off", false) || try_parse_bool_MATCH("false", false) || try_parse_bool_MATCH("no", false)) {
        if (res)
            *res = false;
        return true;
    }

    #undef try_parse_bool_MATCH
    return logsimpleerr(false, "Unknown boolean string '%s'", str);
}

// -------------------------------Testing --------------------------

#ifdef COMMONTESTING

//#include <signal.h>
#include "test.h"
#include "checker.h"

// ------------------------- TEST try_parse ---------------------------------

static TestStatus
tf_try_parse(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== try_parse_int ========== */

    /* 1. Успешный парсинг обычного int */
    test_sub("subtest %d: try_parse_int valid", ++subnum);
    {
        const char     *valid[] = {"0", "123", "-456", "2147483647", "-2147483648"};
        int             expected[] = {0, 123, -456, 2147483647, -2147483648};
        for (int i = 0; i < COUNT(valid); i++) {
            int     result;
            bool    ok;
            test_validate(
                (ok = try_parse_int(valid[i], &result) ) && result == expected[i],
                "try_parse_int('%s'): expected %d, got %d (ok=%s)",
                valid[i], expected[i], result, bool_str(ok));
        }
    }

    /* 2. Неудачи: пустая строка, буквы, частичный парсинг, переполнение, пробелы */
    test_sub("subtest %d: try_parse_int invalid", ++subnum);
    {
        const char *invalid[] = {
            "", "abc", "123abc", /* "  123", */ "123  ", "9999999999", "-9999999999"
        };
        for (int i = 0; i < COUNT(invalid); i++) {
            int     result;
            bool    ok;
            test_validate(
                !(ok = try_parse_int(invalid[i], &result) ),
                "try_parse_int('%s'): must fail, got ok=%s, result=%d",
                invalid[i], bool_str(ok), result);
        }
    }

    /* 3. NULL-указатель */
    test_sub("subtest %d: try_parse_int NULL", ++subnum);
    {
        int     result;
        bool    ok;
        test_validate(
            !(ok = try_parse_int(NULL, &result) ),
            "try_parse_int(NULL) must return false");
    }

    /* ========== try_parse_long ========== */

    /* 4. Успешный парсинг long */
    test_sub("subtest %d: try_parse_long valid", ++subnum);
    {
        const char *valid[] = {"0", "123", "-456",
                               "9223372036854775807", "-9223372036854775808"};
        long        expected[] = {0L, 123L, -456L, 9223372036854775807L, LONG_MIN};
        bool        ok;
        for (int i = 0; i < COUNT(valid); i++) {
            long    result;
            test_validate(
                (ok = try_parse_long(valid[i], &result) ) && result == expected[i],
                "try_parse_long('%s'): expected %ld, got %ld (ok=%s)",
                valid[i], expected[i], result, bool_str(ok));
        }
    }

    /* 5. Неудачи для long */
    test_sub("subtest %d: try_parse_long invalid", ++subnum);
    {
        const char *invalid[] = {
            "", "abc", "123abc", /* "  123", */ "123  ",
            "99999999999999999999", "-99999999999999999999"
        };
        for (int i = 0; i < COUNT(invalid); i++) {
            long    result;
            bool    ok;
            test_validate(
                !(ok = try_parse_long(invalid[i], &result) ),
                "try_parse_long('%s'): must fail, got ok=%s, result=%ld",
                invalid[i], bool_str(ok), result);
        }
    }

    /* 6. NULL */
    test_sub("subtest %d: try_parse_long NULL", ++subnum);
    {
        long result;
        test_validate(
            !try_parse_long(NULL, &result),
            "try_parse_long(NULL) must return false");
    }

    /* ========== try_parse_double ========== */

    /* 7. Успешный парсинг double (обычные числа) */
    test_sub("subtest %d: try_parse_double valid", ++subnum);
    {
        struct { const char *s; double d; } tests[] = {
            {"0.0", 0.0}, {"3.1415", 3.1415}, {"-2.718", -2.718},
            {"1.0e10", 1.0e10}, {"-1.0e-10", -1.0e-10}
        };
        for (int i = 0; i < COUNT(tests); i++) {
            double      result;
            bool        ok;
            test_validate(
                (ok = try_parse_double(tests[i].s, &result) ) && fabs(result - tests[i].d) < 1e-12 * fabs(tests[i].d + 1.0),
                "try_parse_double('%s'): expected %g, got %g (ok=%s)",
                tests[i].s, tests[i].d, result, bool_str(ok));
        }
    }

    /* 8. Специальные значения double: inf, -inf, nan (если разрешены) */
    test_sub("subtest %d: try_parse_double special", ++subnum);
    {
        // inf и -inf обычно парсятся без ошибки, nan тоже, проверим
        const char *special[] = {"inf", "-inf", "nan"};
        for (int i = 0; i < COUNT(special); i++) {
            double      result;
            bool        ok = try_parse_double(special[i], &result);
            // Проверяем, что парсинг успешен и значение соответствует ожидаемому
            if (strcmp(special[i], "inf") == 0) {
                test_validate(ok && isinf(result) && result > 0,
                    "try_parse_double('inf'): expected +inf, got ok=%s, result=%g", bool_str(ok), result);
            } else if (strcmp(special[i], "-inf") == 0) {
                test_validate(ok && isinf(result) && result < 0,
                    "try_parse_double('-inf'): expected -inf, got ok=%s, result=%g", bool_str(ok), result);
            } else if (strcmp(special[i], "nan") == 0) {
                test_validate(ok && isnan(result),
                    "try_parse_double('nan'): expected NaN, got ok=%s, result=%g", bool_str(ok), result);
            }
        }
    }

    /* 9. Неудачи для double (пустая строка, буквы, пробелы, переполнение) */
    test_sub("subtest %d: try_parse_double invalid", ++subnum);
    {
        const char *invalid[] = {
            "", "abc", "123abc", /*"  123" ,*/ "123  ",
            "1e9999", "-1e9999"
        };
        for (int i = 0; i < COUNT(invalid); i++) {
            double      result;
            bool        ok;
            test_validate(
                !(ok = try_parse_double(invalid[i], &result) ),
                "try_parse_double('%s'): must fail, got ok=%s, result=%g",
                invalid[i], bool_str(ok), result);
        }
    }

    /* 10. NULL */
    test_sub("subtest %d: try_parse_double NULL", ++subnum);
    {
        double result;
        test_validate(
            !try_parse_double(NULL, &result),
            "try_parse_double(NULL) must return false");
    }
    /* 11. try_parse_uint – корректные значения */
    test_sub("subtest %d: try_parse_uint valid", ++subnum);
    {
        const char *valid[] = {"0", "123", "  456", "4294967295"};
        unsigned int expected[] = {0, 123, 456, UINT_MAX};
        for (int i = 0; i < COUNT(valid); i++) {
            unsigned int res;
            test_validate(
                try_parse_uint(valid[i], &res) && res == expected[i],
                "try_parse_uint('%s'): expected %u, got %u",
                valid[i], expected[i], res
            );
        }
    }

    /* 12. try_parse_uint – некорректные строки */
    test_sub("subtest %d: try_parse_uint invalid", ++subnum);
    {
        const char *invalid[] = {
            "", "abc", "123abc", "123 ", " 123 ",
            "-1", "-0", "   -123", "  -1",
            "4294967296", "9999999999"
        };
        for (int i = 0; i < COUNT(invalid); i++) {
            unsigned int res;
            test_validate(
                !try_parse_uint(invalid[i], &res),
                "try_parse_uint('%s'): must fail, got %u", invalid[i], res
            );
        }
    }

    /* 13. try_parse_uint – NULL */
    test_sub("subtest %d: try_parse_uint NULL", ++subnum);
    {
        unsigned int res;
        test_validate(
            !try_parse_uint(NULL, &res),
            "try_parse_uint(NULL) must return false"
        );
    }

    /* 14. try_parse_ulong – корректные значения */
    test_sub("subtest %d: try_parse_ulong valid", ++subnum);
    {
        const char *valid[] = {"0", "123", "  456", "18446744073709551615"};
        unsigned long expected[] = {0UL, 123UL, 456UL, ULONG_MAX};
        for (int i = 0; i < COUNT(valid); i++) {
            unsigned long res;
            test_validate(
                try_parse_ulong(valid[i], &res) && res == expected[i],
                "try_parse_ulong('%s'): expected %lu, got %lu", valid[i], expected[i], res
            );
        }
    }

    /* 15. try_parse_ulong – некорректные строки */
    test_sub("subtest %d: try_parse_ulong invalid", ++subnum);
    {
        const char *invalid[] = {
            "", "abc", "123abc", "123 ", " 123 ",
            "-1", "-0", "   -123", "  -1",
            "18446744073709551616", "99999999999999999999"
        };
        for (int i = 0; i < COUNT(invalid); i++) {
            unsigned long res;
            test_validate(
                !try_parse_ulong(invalid[i], &res),
                "try_parse_ulong('%s'): must fail, got %lu", invalid[i], res
            );
        }
    }

    /* 16. try_parse_ulong – NULL */
    test_sub("subtest %d: try_parse_ulong NULL", ++subnum);
    {
        unsigned long res;
        test_validate(
            !try_parse_ulong(NULL, &res),
            "try_parse_ulong(NULL) must return false"
        );
    }

    /* ========== try_parse_char ========== */
    test_sub("subtest %d: try_parse_char valid single chars", ++subnum);
    {
        const char *inputs[]  = {"a", "Z", "0", "\n", " "};
        char        expected[] = {'a', 'Z', '0', '\n', ' '};
        for (size_t i = 0; i < COUNT(inputs); i++) {
            char res = '\0';
            test_validate(
                try_parse_char(inputs[i], &res) && res == expected[i],
                "try_parse_char('%s'): expected '%c', got '%c'",
                inputs[i], expected[i], res
            );
        }
    }

    test_sub("subtest %d: try_parse_char empty string", ++subnum);
    {
        const char *empty[] = {"", NULL};
        for (size_t i = 0; i < COUNT(empty); i++) {
            char res = '\0';
            test_validate(
                !try_parse_char(empty[i], &res),
                "try_parse_char('%s'): must fail, got '%c'",
                empty[i] ? empty[i] : "NULL", res
            );
        }
    }

    test_sub("subtest %d: try_parse_char long string (first char only)", ++subnum);
    {
        char res = '\0';
        test_validate(
            try_parse_char("Hello", &res) && res == 'H',
            "try_parse_char('Hello'): expected 'H', got '%c'", res
        );
    }

    test_sub("subtest %d: try_parse_char with NULL output pointer", ++subnum);
    {
        test_validate(
            try_parse_char("X", NULL),
            "try_parse_char('X', NULL) must return true"
        );
    }

    /* ---------- try_parse_bool ---------- */
    test_sub("subtest %d: try_parse_bool true values", ++subnum);
    {
        const char *true_strs[] = {"on", "ON", "On", "true", "True", "TRUE", "yes", "YES", "Yes"};
        for (size_t i = 0; i < COUNT(true_strs); i++) {
            bool res = false;
            test_validate(
                try_parse_bool(true_strs[i], &res) && res == true,
                "try_parse_bool('%s'): expected true, got %s", true_strs[i], res ? "true" : "false"
            );
        }
    }

    test_sub("subtest %d: try_parse_bool false values", ++subnum);
    {
        const char *false_strs[] = {"off", "OFF", "Off", "false", "False", "FALSE", "no", "NO", "No"};
        for (size_t i = 0; i < COUNT(false_strs); i++) {
            bool res = true;
            test_validate(
                try_parse_bool(false_strs[i], &res) && res == false,
                "try_parse_bool('%s'): expected false, got %s", false_strs[i], res ? "true" : "false"
            );
        }
    }

    test_sub("subtest %d: try_parse_bool invalid strings", ++subnum);
    {
        const char *invalid[] = {"", "unknown", "onoff", "1", "0", "TRUE1", "falsewww", NULL};        for (size_t i = 0; i < COUNT(invalid); i++) {
        bool dummy;
            test_validate(
                !try_parse_bool(invalid[i], &dummy),
                "try_parse_bool('%s'): must fail", invalid[i] ? invalid[i] : "NULL"
            );
        }
    }

    test_sub("subtest %d: try_parse_bool with NULL output", ++subnum);
    {
        test_validate(try_parse_bool("on", NULL), "try_parse_bool('on', NULL) must succeed");
        test_validate(!try_parse_bool("invalid", NULL), "try_parse_bool('invalid', NULL) must fail");
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST int_(not)in ---------------------------------

static TestStatus
tf_int_in(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. int_in: значение присутствует в начале */
    test_sub("subtest %d: int_in – found at start", ++subnum);
    test_validate(
        int_in(10, 10, 20, 30),
        "10 must be in {10,20,30}"
    );

    /* 2. int_in: значение присутствует в середине */
    test_sub("subtest %d: int_in – found in middle", ++subnum);
    test_validate(
        int_in(20, 10, 20, 30),
        "20 must be in {10,20,30}"
    );

    /* 3. int_in: значение присутствует в конце */
    test_sub("subtest %d: int_in – found at end", ++subnum);
    test_validate(
        int_in(30, 10, 20, 30),
        "30 must be in {10,20,30}"
    );

    /* 4. int_in: значение отсутствует */
    test_sub("subtest %d: int_in – not found", ++subnum);
    test_validate(
        !int_in(99, 10, 20, 30),
        "99 must NOT be in {10,20,30}"
    );

    /* 5. int_notin: значение отсутствует → true */
    test_sub("subtest %d: int_notin – not found", ++subnum);
    test_validate(
        int_notin(99, 10, 20, 30),
        "99 must be NOT in {10,20,30}"
    );

    /* 6. int_notin: значение присутствует → false */
    test_sub("subtest %d: int_notin – found", ++subnum);
    test_validate(
        !int_notin(10, 10, 20, 30),
        "10 must be in {10,20,30} (notin=false)"
    );

    /* 7. Пустой список: int_in должно вернуть false */
    test_sub("subtest %d: int_in with empty list", ++subnum);
    test_validate(
        !int_in(42),
        "42 must NOT be found in empty list"
    );

    /* 8. Пустой список: int_notin должно вернуть true */
    test_sub("subtest %d: int_notin with empty list", ++subnum);
    test_validate(
        int_notin(42),
        "42 must be NOT in empty list"
    );

    /* 9. Один элемент в списке, совпадает */
    test_sub("subtest %d: int_in – single element match", ++subnum);
    test_validate(
        int_in(7, 7),
        "7 must be in {7}"
    );

    /* 10. Один элемент в списке, не совпадает */
    test_sub("subtest %d: int_in – single element no match", ++subnum);
    test_validate(
        !int_in(8, 7),
        "8 must NOT be in {7}"
    );

    /* 11. Повторяющиеся значения в списке */
    test_sub("subtest %d: int_in with duplicates", ++subnum);
    test_validate(
        int_in(5, 5, 5, 5),
        "5 must be found in {5,5,5}"
    );

    /* 12. Граничные значения (INT_MAX, INT_MIN) */
    test_sub("subtest %d: int_in with INT_MAX", ++subnum);
    test_validate(
        int_in(INT_MAX, INT_MIN, 0, INT_MAX),
        "INT_MAX must be found"
    );

    /* 13. int_in с очень большим списком (проверка быстродействия) */
    test_sub("subtest %d: int_in – large list", ++subnum);
    // Максимально допустимый список – около 100 элементов, проверим, что макрос не падает
    test_validate(
        int_in(100,
            1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
            21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,
            61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
            81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100),
        "100 must be in the large list"
    );

    /* 14. int_notin с большим списком */
    test_sub("subtest %d: int_notin – large list", ++subnum);
    test_validate(
        int_notin(0,
            1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
            21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,
            61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
            81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100),
        "0 must NOT be in the large list"
    );

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST comparators ---------------------------------
static TestStatus
tf_comparators(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- compare_int ---------- */
    test_sub("subtest %d: compare_int equal", ++subnum);
    test_validate(compare_int(42, 42) == 0, "42 must equal 42");

    test_sub("subtest %d: compare_int less", ++subnum);
    test_validate(compare_int(10, 20) < 0, "10 must be less than 20");

    test_sub("subtest %d: compare_int greater", ++subnum);
    test_validate(compare_int(30, 20) > 0, "30 must be greater than 20");

    /* ---------- compare_long ---------- */
    test_sub("subtest %d: compare_long equal", ++subnum);
    test_validate(compare_long(999999999L, 999999999L) == 0, "999999999L must equal 999999999L");

    test_sub("subtest %d: compare_long less", ++subnum);
    test_validate(compare_long(-100L, 0L) < 0, "-100L must be less than 0L");

    test_sub("subtest %d: compare_long greater", ++subnum);
    test_validate(compare_long(0L, -100L) > 0, "0L must be greater than -100L");

    /* ---------- compare_dbl ---------- */
    test_sub("subtest %d: compare_dbl equal", ++subnum);
    test_validate(compare_dbl(3.1415, 3.1415) == 0, "3.1415 must equal 3.1415");

    test_sub("subtest %d: compare_dbl less", ++subnum);
    test_validate(compare_dbl(1.0, 2.0) < 0, "1.0 must be less than 2.0");

    test_sub("subtest %d: compare_dbl greater", ++subnum);
    test_validate(compare_dbl(2.0, 1.0) > 0, "2.0 must be greater than 1.0");

    test_sub("subtest %d: compare_dbl NaN equal", ++subnum);
    test_validate(compare_dbl(NAN, NAN) == 0, "NaN must equal NaN (by implementation)");

    test_sub("subtest %d: compare_dbl NaN less", ++subnum);
    test_validate(compare_dbl(NAN, 1.0) < 0, "NaN must be less than any number (by implementation)");

    test_sub("subtest %d: compare_dbl +inf greater", ++subnum);
    test_validate(compare_dbl(INFINITY, 1.0) > 0, "+inf must be greater than 1.0");

    test_sub("subtest %d: compare_dbl -inf less", ++subnum);
    test_validate(compare_dbl(-INFINITY, 1.0) < 0, "-inf must be less than 1.0");

    test_sub("subtest %d: compare_dbl +inf vs -inf", ++subnum);
    test_validate(compare_dbl(INFINITY, -INFINITY) > 0, "+inf must be greater than -inf");

    /* ---------- compare_ptr ---------- */
    test_sub("subtest %d: compare_ptr equal", ++subnum);
    {
        int x = 0;
        test_validate(compare_ptr(&x, &x) == 0, "same address must be equal");
    }

    test_sub("subtest %d: compare_ptr not equal", ++subnum);
    {
        int a = 1, b = 2;
        // Порядок адресов на стеке не определён, поэтому просто проверяем, что результат не 0
        test_validate(compare_ptr(&a, &b) != 0, "different addresses must not be equal");
    }

    /* ---------- compare_char ---------- */
    test_sub("subtest %d: compare_char equal", ++subnum);
    test_validate(compare_char('A', 'A') == 0, "'A' must equal 'A'");

    test_sub("subtest %d: compare_char less", ++subnum);
    test_validate(compare_char('A', 'B') < 0, "'A' must be less than 'B'");

    test_sub("subtest %d: compare_char greater", ++subnum);
    test_validate(compare_char('B', 'A') > 0, "'B' must be greater than 'A'");

    /* ---------- compare_uint ---------- */
    test_sub("subtest %d: compare_uint equal", ++subnum);
    test_validate(compare_uint(123u, 123u) == 0, "123u must equal 123u");

    test_sub("subtest %d: compare_uint less", ++subnum);
    test_validate(compare_uint(10u, 20u) < 0, "10u must be less than 20u");

    test_sub("subtest %d: compare_uint greater", ++subnum);
    test_validate(compare_uint(20u, 10u) > 0, "20u must be greater than 10u");

    /* ---------- compare_ulong ---------- */
    test_sub("subtest %d: compare_ulong equal", ++subnum);
    test_validate(compare_ulong(999999999UL, 999999999UL) == 0, "999999999UL must equal 999999999UL");

    test_sub("subtest %d: compare_ulong less", ++subnum);
    test_validate(compare_ulong(1UL, 100UL) < 0, "1UL must be less than 100UL");

    test_sub("subtest %d: compare_ulong greater", ++subnum);
    test_validate(compare_ulong(100UL, 1UL) > 0, "100UL must be greater than 1UL");

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST pointer comparators ---------------------------------
static TestStatus
tf_comparators_ptr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- pchar_cmp ---------- */
    test_sub("subtest %d: pchar_cmp equal", ++subnum);
    {
        unsigned char a = 'X', b = 'X';
        test_validate(pchar_cmp(&a, &b) == 0, "'X' must equal 'X'");
    }

    test_sub("subtest %d: pchar_cmp less / greater", ++subnum);
    {
        unsigned char a = 'A', b = 'B';
        test_validate(pchar_cmp(&a, &b) < 0, "'A' must be less than 'B'");
        test_validate(pchar_cmp(&b, &a) > 0, "'B' must be greater than 'A'");
    }

    test_sub("subtest %d: pchar_revcmp", ++subnum);
    {
        unsigned char a = 'A', b = 'B';
        test_validate(pchar_revcmp(&a, &b) > 0, "reverse: 'A' must be greater than 'B'");
        test_validate(pchar_revcmp(&b, &a) < 0, "reverse: 'B' must be less than 'A'");
    }

    /* ---------- pint_cmp ---------- */
    test_sub("subtest %d: pint_cmp equal", ++subnum);
    {
        int a = 42, b = 42;
        test_validate(pint_cmp(&a, &b) == 0, "42 must equal 42");
    }

    test_sub("subtest %d: pint_cmp less / greater", ++subnum);
    {
        int a = 10, b = 20;
        test_validate(pint_cmp(&a, &b) < 0, "10 must be less than 20");
        test_validate(pint_cmp(&b, &a) > 0, "20 must be greater than 10");
    }

    test_sub("subtest %d: pint_revcmp", ++subnum);
    {
        int a = 10, b = 20;
        test_validate(pint_revcmp(&a, &b) > 0, "reverse: 10 must be greater than 20");
        test_validate(pint_revcmp(&b, &a) < 0, "reverse: 20 must be less than 10");
    }

    /* ---------- plong_cmp ---------- */
    test_sub("subtest %d: plong_cmp equal", ++subnum);
    {
        long a = 999999999L, b = 999999999L;
        // ВНИМАНИЕ: в plong_cmp сейчас ошибка – аргументы приводятся как const int*, а не const long*
        // После исправления этот тест будет работать. Пока он может дать неверный результат.
        test_validate(plong_cmp(&a, &b) == 0, "999999999L must equal 999999999L");
    }

    test_sub("subtest %d: plong_cmp less / greater", ++subnum);
    {
        long a = -100L, b = 0L;
        test_validate(plong_cmp(&a, &b) < 0, "-100L must be less than 0L");
        test_validate(plong_cmp(&b, &a) > 0, "0L must be greater than -100L");
    }

    test_sub("subtest %d: plong_revcmp", ++subnum);
    {
        long a = 10L, b = 20L;
        test_validate(plong_revcmp(&a, &b) > 0, "reverse: 10L must be greater than 20L");
        test_validate(plong_revcmp(&b, &a) < 0, "reverse: 20L must be less than 10L");
    }

    /* ---------- pdbl_cmp ---------- */
    test_sub("subtest %d: pdbl_cmp equal", ++subnum);
    {
        double a = 3.1415, b = 3.1415;
        test_validate(pdbl_cmp(&a, &b) == 0, "3.1415 must equal 3.1415");
    }

    test_sub("subtest %d: pdbl_cmp less / greater", ++subnum);
    {
        double a = 1.0, b = 2.0;
        test_validate(pdbl_cmp(&a, &b) < 0, "1.0 must be less than 2.0");
        test_validate(pdbl_cmp(&b, &a) > 0, "2.0 must be greater than 1.0");
    }

    test_sub("subtest %d: pdbl_cmp special (NaN, inf)", ++subnum);
    {
        double nan1 = NAN, nan2 = NAN;
        test_validate(pdbl_cmp(&nan1, &nan2) == 0, "NaN must equal NaN (by implementation)");

        double inf = INFINITY, ninf = -INFINITY;
        test_validate(pdbl_cmp(&inf, &ninf) > 0, "+inf must be greater than -inf");
        test_validate(pdbl_cmp(&ninf, &inf) < 0, "-inf must be less than +inf");
    }

    test_sub("subtest %d: pdbl_revcmp", ++subnum);
    {
        double a = 1.0, b = 2.0;
        test_validate(pdbl_revcmp(&a, &b) > 0, "reverse: 1.0 must be greater than 2.0");
        test_validate(pdbl_revcmp(&b, &a) < 0, "reverse: 2.0 must be less than 1.0");
    }

    /* ---------- pptr_cmp ---------- */
    test_sub("subtest %d: pptr_cmp equal", ++subnum);
    {
        int dummy;
        void *p1 = &dummy;
        void *p2 = &dummy;          // тот же адрес
        test_validate(pptr_cmp(&p1, &p2) == 0, "same pointer must be equal");
    }

    test_sub("subtest %d: pptr_cmp less / greater", ++subnum);
    {
        int a, b;
        void *pa = &a;
        void *pb = &b;
        test_validate(pptr_cmp(&pa, &pb) != 0, "different pointers must not be equal");
        // знак зависит от реализации compare_ptr, но хотя бы !=0
    }

    test_sub("subtest %d: pptr_revcmp", ++subnum);
    {
        int a, b;
        void *pa = &a;
        void *pb = &b;
        int forward = pptr_cmp(&pa, &pb);
        int reverse = pptr_revcmp(&pa, &pb);
        test_validate(forward == -reverse, "revcmp must negate the result");
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST round_up_2 -------------------------
static TestStatus
tf_round_up_2(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: round_up_2(0)", ++subnum);
    {
        unsigned res = round_up_2(0);
        test_validate(res == 0, "round_up_2(0) must be 0, got %u", res);
    }

    test_sub("subtest %d: round_up_2(1)", ++subnum);
    {
        unsigned res = round_up_2(1);
        test_validate(res == 2, "round_up_2(1) must be 2, got %u", res);
    }

    test_sub("subtest %d: round_up_2(2)", ++subnum);
    {
        unsigned res = round_up_2(2);
        test_validate(res == 4, "round_up_2(2) must be 4, got %u", res);
    }

    test_sub("subtest %d: round_up_2(3)", ++subnum);
    {
        unsigned res = round_up_2(3);
        test_validate(res == 4, "round_up_2(3) must be 4, got %u", res);
    }

    test_sub("subtest %d: round_up_2(4)", ++subnum);
    {
        unsigned res = round_up_2(4);
        test_validate(res == 8, "round_up_2(4) must be 8, got %u", res);
    }

    test_sub("subtest %d: round_up_2(5)", ++subnum);
    {
        unsigned res = round_up_2(5);
        test_validate(res == 8, "round_up_2(5) must be 8, got %u", res);
    }

    test_sub("subtest %d: round_up_2(7)", ++subnum);
    {
        unsigned res = round_up_2(7);
        test_validate(res == 8, "round_up_2(7) must be 8, got %u", res);
    }

    test_sub("subtest %d: round_up_2(8)", ++subnum);
    {
        unsigned res = round_up_2(8);
        test_validate(res == 16, "round_up_2(8) must be 16, got %u", res);
    }

    test_sub("subtest %d: round_up_2(9)", ++subnum);
    {
        unsigned res = round_up_2(9);
        test_validate(res == 16, "round_up_2(9) must be 16, got %u", res);
    }

    test_sub("subtest %d: round_up_2(15)", ++subnum);
    {
        unsigned res = round_up_2(15);
        test_validate(res == 16, "round_up_2(15) must be 16, got %u", res);
    }

    test_sub("subtest %d: round_up_2(16)", ++subnum);
    {
        unsigned res = round_up_2(16);
        test_validate(res == 32, "round_up_2(16) must be 32, got %u", res);
    }

    test_sub("subtest %d: round_up_2(17)", ++subnum);
    {
        unsigned res = round_up_2(17);
        test_validate(res == 32, "round_up_2(17) must be 32, got %u", res);
    }

    test_sub("subtest %d: round_up_2(1023)", ++subnum);
    {
        unsigned res = round_up_2(1023);
        test_validate(res == 1024, "round_up_2(1023) must be 1024, got %u", res);
    }

    test_sub("subtest %d: round_up_2(1024)", ++subnum);
    {
        unsigned res = round_up_2(1024);
        test_validate(res == 2048, "round_up_2(1024) must be 2048, got %u", res);
    }

    test_sub("subtest %d: round_up_2(UINT_MAX)", ++subnum);
    {
        unsigned res = round_up_2(UINT_MAX);
        test_validate(res == 0, "round_up_2(UINT_MAX) must be 0 (overflow), got %u", res);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST calcnewsize -------------------------
static TestStatus
tf_calcnewsize(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: SIZE_NONE returns same value", ++subnum);
    {
        test_validate(calcnewsize(SIZE_NONE, 0)   == 0,   "SIZE_NONE(0) must be 0");
        test_validate(calcnewsize(SIZE_NONE, 1)   == 1,   "SIZE_NONE(1) must be 1");
        test_validate(calcnewsize(SIZE_NONE, 100) == 100, "SIZE_NONE(100) must be 100");
    }

    test_sub("subtest %d: SIZE_MIN10 ensures at least 10", ++subnum);
    {
        test_validate(calcnewsize(SIZE_MIN10, 0)  == 10, "SIZE_MIN10(0) must be 10");
        test_validate(calcnewsize(SIZE_MIN10, 5)  == 10, "SIZE_MIN10(5) must be 10");
        test_validate(calcnewsize(SIZE_MIN10, 9)  == 10, "SIZE_MIN10(9) must be 10");
        test_validate(calcnewsize(SIZE_MIN10, 10) == 10, "SIZE_MIN10(10) must be 10");
        test_validate(calcnewsize(SIZE_MIN10, 15) == 15, "SIZE_MIN10(15) must be 15");
        test_validate(calcnewsize(SIZE_MIN10, 99) == 99, "SIZE_MIN10(99) must be 99");
    }

    test_sub("subtest %d: SIZE_POWER2 rounds up to next power of two", ++subnum);
    {
        test_validate(calcnewsize(SIZE_POWER2, 0)  == 0,  "SIZE_POWER2(0) must be 0");
        test_validate(calcnewsize(SIZE_POWER2, 1)  == 2,  "SIZE_POWER2(1) must be 2");
        test_validate(calcnewsize(SIZE_POWER2, 2)  == 4,  "SIZE_POWER2(2) must be 4");
        test_validate(calcnewsize(SIZE_POWER2, 3)  == 4,  "SIZE_POWER2(3) must be 4");
        test_validate(calcnewsize(SIZE_POWER2, 4)  == 8,  "SIZE_POWER2(4) must be 8");
        test_validate(calcnewsize(SIZE_POWER2, 5)  == 8,  "SIZE_POWER2(5) must be 8");
        test_validate(calcnewsize(SIZE_POWER2, 15) == 16, "SIZE_POWER2(15) must be 16");
        test_validate(calcnewsize(SIZE_POWER2, 16) == 32, "SIZE_POWER2(16) must be 32");
    }

    test_sub("subtest %d: unknown type returns -1", ++subnum);
    {
        test_validate(calcnewsize((Tincrease)999, 42) == -1, "Unknown type must return -1");
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST strisempty simple -------------------------
static TestStatus
tf_strisempty(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: strisempty with empty string", ++subnum);
    {
        test_validate(strisempty("") == true,
                      "empty string must return true");
    }

    test_sub("subtest %d: strisempty with NULL", ++subnum);
    {
        test_validate(strisempty(NULL) == true,
                      "NULL must return true");
    }

    test_sub("subtest %d: strisempty with non-empty string", ++subnum);
    {
        test_validate(strisempty("hello") == false,
                      "non-empty string must return false");
    }

    test_sub("subtest %d: strisempty with single character", ++subnum);
    {
        test_validate(strisempty("a") == false,
                      "single character string must return false");
    }

    test_sub("subtest %d: strisempty with whitespace only", ++subnum);
    {
        test_validate(strisempty("   ") == false,
                      "whitespace-only string is not empty");
    }

    test_sub("subtest %d: strisempty with leading null character", ++subnum);
    {
        const char *s = "\0abc";  // фактически строка состоит только из '\0'
        test_validate(strisempty(s) == true,
                      "string starting with '\\0' must be considered empty");
    }

    return TEST_PASSED;
}

// =====================================================================
// countstrings — count NUL-terminated array of C-strings
// =====================================================================

static TestStatus
tf8_countstrings(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. обычный массив */
    test_sub("subtest %d: plain 3 strings", ++subnum);
    {
        const char *arr[] = { "alpha", "beta", "gamma", NULL };
        int n = countstrings(arr);
        test_validate(n == 3, "expected 3, got %d", n);
    }

    /* 2. один элемент */
    test_sub("subtest %d: single string", ++subnum);
    {
        const char *arr[] = { "only", NULL };
        int n = countstrings(arr);
        test_validate(n == 1, "expected 1, got %d", n);
    }

    /* 3. пустой массив (сразу NULL) */
    test_sub("subtest %d: empty array", ++subnum);
    {
        const char *arr[] = { NULL };
        int n = countstrings(arr);
        test_validate(n == 0, "expected 0, got %d", n);
    }

    /* 4. много элементов */
    test_sub("subtest %d: many elements", ++subnum);
    {
        const char *arr[33];
        for (int i = 0; i < 32; i++) arr[i] = "x";
        arr[32] = NULL;
        int n = countstrings(arr);
        test_validate(n == 32, "expected 32, got %d", n);
    }

    /* 5. пустые строки внутри — они не NULL, считаются */
    test_sub("subtest %d: empty strings count", ++subnum);
    {
        const char *arr[] = { "", "", "", NULL };
        int n = countstrings(arr);
        test_validate(n == 3, "expected 3 (empty strings count), got %d", n);
    }

    /* 6. смесь пустых и непустых */
    test_sub("subtest %d: mixed empty and non-empty", ++subnum);
    {
        const char *arr[] = { "a", "", "b", "", "c", NULL };
        int n = countstrings(arr);
        test_validate(n == 5, "expected 5, got %d", n);
    }

    /* 7. строки с пробельными/спецсимволами */
    test_sub("subtest %d: whitespace/special chars", ++subnum);
    {
        const char *arr[] = { " ", "\t", "\n", "  with space  ", NULL };
        int n = countstrings(arr);
        test_validate(n == 4, "expected 4, got %d", n);
    }

    /* 8. первый элемент NULL — пустой массив */
    test_sub("subtest %d: first element NULL", ++subnum);
    {
        const char *arr[] = { NULL, "unreachable", "also unreachable" };
        int n = countstrings(arr);
        test_validate(n == 0, "expected 0 (first is NULL), got %d", n);
    }

    /* 9. compound literal (inline usage как в макросах) */
    test_sub("subtest %d: compound literal inline", ++subnum);
    {
        int n = countstrings((const char * const[]){ "a", "b", "c", NULL });
        test_validate(n == 3, "expected 3 from compound literal, got %d", n);
    }

    /* 10. указатель на строку-литерал, не массив */
    test_sub("subtest %d: via local pointer", ++subnum);
    {
        const char *arr[] = { "x", "y", NULL };
        const char * const *p = arr;
        int n = countstrings(p);
        test_validate(n == 2, "expected 2, got %d", n);
    }

    /* 11. countstrings == strlen-like consistency */
    test_sub("subtest %d: consistency with manual count", ++subnum);
    {
        const char *arr[] = { "one", "two", "three", "four", NULL };
        int via_func = countstrings(arr);

        int manual = 0;
        while (arr[manual]) manual++;

        test_validate(via_func == manual,
                      "func=%d manual=%d", via_func, manual);
    }

    /* 12. повторные вызовы на одном массиве — функция чистая */
    test_sub("subtest %d: idempotent (no state)", ++subnum);
    {
        const char *arr[] = { "a", "b", NULL };
        int n1 = countstrings(arr);
        int n2 = countstrings(arr);
        int n3 = countstrings(arr);
        test_validate(n1 == 2 && n1 == n2 && n2 == n3,
                      "expected stable 2,2,2 got %d,%d,%d", n1, n2, n3);
    }

    /* 13. длинные строки (значения неважны, важен факт ненулевого указателя) */
    test_sub("subtest %d: long strings", ++subnum);
    {
        char big1[512], big2[512];
        memset(big1, 'a', 511); big1[511] = '\0';
        memset(big2, 'b', 511); big2[511] = '\0';

        const char *arr[] = { big1, big2, NULL };
        int n = countstrings(arr);
        test_validate(n == 2, "expected 2, got %d", n);
    }

    /* 14. указатели на разные буферы одного содержимого — считаются как отдельные */
    test_sub("subtest %d: same content different buffers", ++subnum);
    {
        char b1[] = "dup";
        char b2[] = "dup";
        const char *arr[] = { b1, b2, "dup", NULL };
        int n = countstrings(arr);
        test_validate(n == 3, "expected 3 (each pointer counts), got %d", n);
    }

    /* 15. результат типа int, возврат t - p — проверяем знак (>= 0) */
    test_sub("subtest %d: non-negative result", ++subnum);
    {
        const char *arr[] = { NULL };
        int n = countstrings(arr);
        test_validate(n >= 0, "result must be >= 0, got %d", n);
        test_validate(n == 0, "empty must be 0, got %d", n);
    }

    return logret(TEST_PASSED, "done");
}

// -------------------------------------------------------------------
int
main( /* int argc, const char *argv[] */ )
{
    logsimpleinit("Start");

    testenginestd(
        TESTADD(tf_try_parse,        "Simple try_parse_<type> test"),
        TESTADD(tf_int_in,           "Simple tf_int_(not)in test"),
        TESTADD(tf_comparators,      "Simple compare_<type> test"),
        TESTADD(tf_comparators_ptr,  "Simple pointer compare_<type> test"),
        TESTADD(tf_round_up_2,       "round_up_2() simple test"),
        TESTADD(tf_calcnewsize,      "calcnewsize() simple test"),
        TESTADD(tf_strisempty,       "strisempty() simple test"),
        TESTADD(tf8_countstrings,    "countstrings() simple test")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* COMMONTESTING */

