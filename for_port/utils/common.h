#ifndef _COMMON_H
#define _COMMON_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <sys/errno.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L && __has_include(<stdbit.h>)
    #include <stdbit.h>
    #define HAS_STDC_BIT_CEIL 1
#else
    #define HAS_STDC_BIT_CEIL 0
#endif

#include "bool.h"
#include "log.h"

/***************************************************************
				USEFUL MACRO AND FUNCTIONS
***************************************************************/

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static const int    G_GLOB_AVERAGE = INT_MAX;

// universale comparator (for simple type cast in qsort)
typedef int(*Comparator)(const void *, const void *);

//#define 			COUNT(arr) (int)(sizeof arr/sizeof(typeof(*arr)) )
#define             COUNT(arr) (int)(sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief Count the number of non-NULL pointers in a NULL-terminated
 *        array of C-strings.
 *
 * Iterates from @p p until the first NULL pointer is encountered and
 * returns the number of elements seen before it. The contents of the
 * individual strings are not inspected: an empty string @c "" is a
 * valid element and is counted; only a NULL pointer terminates the
 * scan.
 *
 * @param[in] p  Pointer to a NULL-terminated array of @c const @c char*.
 *               Must not be NULL, and must be terminated by a NULL
 *               pointer. Behavior is undefined if the array is not
 *               terminated.
 *
 * @return Number of elements before the terminating NULL, cast to
 *         @c int. Returns @c 0 when @c p points directly at a NULL
 *         entry.
 *
 * @par Example
 * @code
 *   const char *names[] = { "alpha", "beta", "", "gamma", NULL };
 *   int n = countstrings(names);   // n == 4 (empty string is counted)
 *
 *   int m = countstrings((const char * const[]){ NULL });  // m == 0
 * @endcode
 *
 * @note The return type is @c int, while the internal pointer
 *       difference is @c ptrdiff_t. For arrays larger than @c INT_MAX
 *       elements the result is truncated. In practice this limit is
 *       never reached for the intended use (config lists, argv-like
 *       arrays, test fixtures). If a larger range is needed, change
 *       the return type to @c ptrdiff_t or @c size_t.
 *
 * @warning Passing @c NULL as @p p is undefined behavior. The function
 *          does not validate its argument.
 */
static inline size_t             countstrings(const char * const *p){
    const char * const *t = p;
    while (*t)
        t++;
    return t - p;
}

static inline const char        *skip_leading_spaces(const char *str) {
    while ((unsigned char) *str == ' ' || (unsigned char) *str == '\t')
        str++;
    return str;
}

static inline const char        *skip_leading_spaces_nl(const char *str) {
    while (*str && isspace( (unsigned char) *str))
        str++;
    return str;
}

static const char 	NULLSTR[] = "(null)";

#define MIN(x, y)\
        ({ typeof (x) _x = (x); \
           typeof (y) _y = (y); \
       _x < _y? _x : _y; } )

#define MAX(x, y)\
        ({ typeof (x) _x = (x); \
           typeof (y) _y = (y); \
       _x > _y? _x : _y; } )

// TODO:
#define LEAST(a, ...) ({ typeof(a) _ARR[] = {a, ##__VA_ARGS__};\
	   				     typeof(a) _MIN = *_ARR;\
						 for (int i = 1; i < COUNT(_ARR); i++)\
							 if (_ARR[i] < _MIN)\
								 _MIN = _ARR[i];\
						 _MIN;\
						     })

#define GREATEST(a, ...) ({ typeof(a) _ARR[] = {a, ##__VA_ARGS__};\
                            typeof(a) _MAX = *_ARR;\
                        	for (int i = 1; i < COUNT(_ARR); i++)\
                            	if (_ARR[i] > _MAX)\
                                	_MAX = _ARR[i];\
                        	_MAX;\
                          })

#define STRING(x) #x

// check if compatible
#define IS_COMPATIBLE(x, T) _Generic((x), T: 1, default 0)

#define FREE(x) { free(x); (x) = 0; }

#ifndef NDEBUG

	#include <stdio.h>

	// a big qwestion abount printing float
	#define TYPEFORMAT(x) _Generic((x), \
    	char					: "%c", \
    	signed char				: "%hhd", \
    	unsigned char			: "%hhu", \
    	signed short			: "%hd", \
    	unsigned short			: "%hu", \
    	signed int				: "%d", \
    	unsigned int			: "%u", \
    	long int				: "%ld", \
    	unsigned long int		: "%lu", \
    	long long int			: "%lld", \
    	unsigned long long int	: "%llu", \
    	float					: "%f", \
    	double					: "%f", \
    	long double				: "%Lf", \
    	char *					: "%s", \
		const char *			: "%s",\
    	void *					: "%p",\
		const void *			: "%p"\
	)

	#define typeprint(a) { printf(#a " = ");  printf(TYPEFORMAT(a), a); putchar('\n'); }

#else /* !NDEBUG */
	#define typeprint(a)
#endif /* NDEBUG */

#define				DUMMY

#define				CASE_RETURN(x) case x:  return #x

// -------------------------------------- Random ------------------------------

static inline void
rndinit(void){
    #if defined(__unix__) || defined(__APPLE__)
        srand48(time(0));
    #endif
    srand(time(0));
}
/// @brief random [0, max]
/// @param int value
/// @return random between 0 and max
static inline int
rndint(int max)
{
    if (max <= 0)
        return 0;
    // 
    int range = max + 1; // Диапазон [0, max]
    int limit = RAND_MAX - (RAND_MAX % range);
    int r;
    do {
        r = rand();
    } while (r >= limit);
    //
    return r % range;
}
static inline char 
rndlowchar(void) {
    return (char)('a' + rndint(25));
}

static inline char 
rndupperchar(void) {
    return (char)('A' + rndint(25));
}

static inline char 
rnddigitchar(void) {
    return (char)('0' + rndint(9));
}
static inline unsigned
rnduint(unsigned max)
{
    if (max == 0)
        return 0;

    // Если max больше, чем RAND_MAX, метод с RAND_MAX % range не сработает
    // так же, как и в rndint. Но для unsigned max это критично.
    // Для простоты и точности в рамках стандартного rand():
    if (max >= (unsigned) RAND_MAX) {
        return (unsigned)(((double)rand() / (double)RAND_MAX) * max);
    }

    unsigned range = max + 1;
    unsigned limit = (unsigned)RAND_MAX - ((unsigned)RAND_MAX % range);
    unsigned r;

    do {
        r = (unsigned) rand();
    } while (r >= limit);

    return r % range;
}
static inline long
rndlong(long max)
{
    if (max <= 0 && max > RAND_MAX)
        return 0;       // as we can't userraise here
    // 
    long    range = max + 1;
    long    limit = RAND_MAX - (RAND_MAX % range);
    long    r;
    do {
        r = rand();
    } while (r >= limit);
    //
    return r % range;
}
// probably rework is required
static inline unsigned long
rndulong(unsigned long max)
{
    return (unsigned long)rand() * max / RAND_MAX;
}

static inline double
drand(void)
{
    #if defined(__unix__) || defined(__APPLE__)
        return drand48();
    #else
        return (double) rand() / (RAND_MAX + 1.0);
    #endif
}

// random from 0 till dmax
static inline double
rnddbl(double dmax)
{
    return drand() * dmax;
}
// ----------------------------------- Comparators ----------------------------

static inline int               compare_char(unsigned char v1, unsigned char v2){
    if (v1 > v2)
        return 1;
    else if (v1 < v2)
        return -1;
    else
        return 0;
}
static inline int               compare_bool(bool v1, bool v2){
    if (v1 > v2)
        return 1;
    else if (v1 < v2)
        return -1;
    else
        return 0;
}
static inline int               compare_int(int v1, int v2){
    if (v1 > v2)
        return 1;
    else if (v1 < v2)
        return -1;
    else
        return 0;
}
static inline int               compare_uint(unsigned v1, unsigned v2){
    if (v1 > v2)
        return 1;
    else if (v1 < v2)
        return -1;
    else
        return 0;
}
static inline int               compare_long(long v1, long v2){
    if (v1 > v2)
        return 1;
    else if (v1 < v2)
        return -1;
    else
        return 0;
}
static inline int               compare_ulong(unsigned long v1, unsigned long v2){
    if (v1 > v2)
        return 1;
    else if (v1 < v2)
        return -1;
    else
        return 0;
}
static inline int               compare_dbl(double v1, double v2){
    if (isnan(v1) && isnan(v2))
        return 0;
    if (isnan(v1))
        return -1;
    if (isnan(v2))
        return 1;
    if (v1 > v2)
        return 1;
    if (v1 < v2)
        return -1;
    return 0; // равны (включая случай +0.0 и -0.0)
}
static inline int               compare_ptr(const void *restrict v1, const void *restrict v2){
    uintptr_t ua = (uintptr_t) v1;
    uintptr_t ub = (uintptr_t) v2;
    return (ua > ub) - (ua < ub);
    //return (v1 > v2) - (v2 < v1);
}

// ------------------- Pointer comparators -------------------

typedef int                     (*pointer_comparator)(const void *restrict s1, const void *restrict s2);
// simple char comparator
extern int                      pchar_cmp(const void *restrict s1, const void *restrict s2);
// simple char reverse comparator
extern int                      pchar_revcmp(const void *restrict s1, const void *restrict s2);
// simple comparator pointer int
extern int                      pint_cmp(const void *restrict i1, const void *restrict i2);
// simple reverse comparator pointer int
extern int                      pint_revcmp(const void *restrict i1, const void *restrict i2);
// simple comparator pointer long
extern int                      plong_cmp(const void *restrict l1, const void *restrict l2);
// simple reverse comparator pointer long
extern int                      plong_revcmp(const void *restrict l1, const void *restrict l2);
// simple comparator pointer to pointer
extern int                      pptr_cmp(const void *restrict p1, const void *restrict p2);
// simple reverse comparator pointer to pointer
extern int                      pptr_revcmp(const void *restrict p1, const void *restrict p2);
// simple comparator pointer double
extern int                      pdbl_cmp(const void *restrict d1, const void *restrict d2);
// simple reverse comparator pointer double
extern int                      pdbl_revcmp(const void *restrict d1, const void *restrict d2);

// (void *) comparator
//static inline int               pointer_cmp(const void *p1, const void *p2){
//    return *(const void **) p1 - *(const void **) p2;   // (void *) - (void *)!
//}

// ----------------------------------- Exchangers ----------------------------------

// simple char exhanger
static inline void              char_exch(char *s1, char *s2){
    char c = *s1;
    *s1 = *s2;
    *s2 = c;
}
// simple int exchanger
static inline void              int_exch(int *i1, int *i2){
    int tmp = *i1;
    *i1 = *i2;
    *i2 = tmp;
}
// simple long exchanger
static inline void              long_exch(long *i1, long *i2){
    long tmp = *i1;
    *i1 = *i2;
    *i2 = tmp;
}
// simple double exchanger  TODO: think about generic exchanger
static inline void              dbl_exch(double *d1, double *d2) {
    double tmp = *d1;
    *d1 = *d2;
    *d2 = tmp;
}

// simple pointer exchanger  TODO: think about generic exchanger
static inline void              ptr_exch(void **v1, void **v2) {
    void    *tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}
// str exch
static inline void              str_exch(const char **s1, const char **s2) {
    const char *tmp = *s1;
    *s1 = *s2;
    *s2 = tmp;
}
/// @brief generallized exchanger, any size of item
/// @param v1 pointer to the elem
/// @param v2 pointer to the elem
/// @param sz size of elem
static inline void              item_exch(void *v1, void *v2, size_t sz) {
    char        buffer[256]; // Маленький буфер на стеке
    bool        allocflag = false;
    void       *tmp;

    if (v1 == v2) 
        return;
    if (sz <= sizeof(buffer)) {
        tmp = buffer;
    } else {
        tmp = malloc(sz);
        if (!tmp) 
            return;         // can't userraise there!!!
        allocflag = true;
    }
    memcpy(tmp, v1, sz);
    memcpy(v1, v2, sz);
    memcpy(v2, tmp, sz);
    if (allocflag)
        free(tmp);
}

// -------------------------------- Converters ---------------------------
static inline int               ctoi(char c){
    return c - '0';
}
static inline int               ctoihex(char c){
    if (isdigit(c) )
        return c - '0';
    char low = tolower((unsigned char)c);
    if (low >= 'a' && low <= 'f')
        return low - 'a' + 10;
    else
        return -1;
}
static inline char              itoc(int c){
    return c + '0';
}
static inline char              itohex(int c){
    if (c < 10)
        return c + '0';
    else
        return c - 10 + 'A';
}
static inline char              itoupper(int c){
    return c + 'A';
}
static inline char              itolower(int c){
    return c + 'a';
}
// ----------------------------------- Utilities -------------------------------------------

//  binary char search
static inline char              *bcharsearch(char c, const char *pt, int len){
    return  bsearch(&c, pt, len, 1, pchar_cmp /* from common.h */);
}

static inline char              *sort_str(char *s, int len, bool asc){
    qsort(s, len, 1, asc ? pchar_cmp : pchar_revcmp);
    return s;
}

// --------------------------------------- Fillers -----------------------------------------

// make s unique by symbols (no odering!!!)
extern char                     *uniq_str(char *s, int *p_len);

// int[] filler
extern void                     fill_int(int *arr, int cnt, int value);

// long[] filler
extern void                     fill_long(long *arr, int cnt, long value);

// double[] filler
extern void                     fill_double(double *arr, int cnt, double value);

// float[] filler
extern void                     fill_float(float *arr, int cnt, float value);

// fill with 0.0 cnt elements
static inline void              clean_double(double *arr, int cnt){
    return fill_double(arr, cnt, 0.0);
}
// fill with 0.0f cnt elements
static inline void              clean_float(float *arr, int cnt){
    return fill_float(arr, cnt, 0.0f);
}
// no filler, only cleaner for pointers!
static inline void              clean_ptr(void **arr, int cnt){
    memset(arr, 0, cnt * sizeof(void *) );
}

// ------------------------------- BITS Operations ------------------------------------------

// print int as bits
extern int                      fprint_bits(FILE *f, const char *str, unsigned val);

// bits to string (STATIC for now)
extern const char              *bits_str(char *buf, int len, unsigned val);

// print int as bits
static inline int               print_bits(const char *str, unsigned  val){
    return fprint_bits(stdout, str, val);
}

// cycle
static inline int               cycleinc(int val, int cycle){
    if (val >= cycle - 1)
        val = 0;
    else
        val++;
    return val;
}
// print n chars to f
extern int                      fprintn(FILE *f, const char *str, int sz);

static inline int               printn(const char *str, int sz){
    return fprintn(stdout, str, sz);
}

/**
 * @brief Calculates the next power of two strictly greater than the input value.
 *
 * This function is designed for memory capacity expansion strategies. 
 * Unlike standard bit-ceil implementations, this function ensures that 
 * the result is always strictly greater than @p val, even if @p val 
 * is already a power of two.
 *
 * @note This behavior is critical for preventing buffer overflows when 
 *       appending a null terminator at the end of a buffer, ensuring 
 *       there is always enough room for the extra byte.
 *
 * @example
 * round_up_2(3) -> 4
 * round_up_2(4) -> 8
 * round_up_2(7) -> 8
 *
 * @param val The input ulong value.
 * @return The smallest power of two that is strictly greater than @p val. 
 *         Returns 0 if @p val is 0.
 *
 * @complexity O(1) if using C23 (via @c stdc_bit_ceil), 
 *             otherwise O(k) where k is the number of set bits.
 */
static inline unsigned long   round_up_2(unsigned long val){
#if HAS_STDC_BIT_CEIL
    return stdc_bit_ceil(val + 1);
#else /* !__STDC_VERSION__ >= 202311L */
    unsigned long   prev = 0;
    while (val)
        val &= ( (prev = val) - 1);
    return prev << 1;
#endif 
}

// ------------------------------ c-string func -----------------------------------------
// reverse c-string with len
extern char                     *reverse(char *s, int len);
// reverse c-string 
static inline char              *reversel(char *s){
    int     len = strlen(s);
    return reverse(s, len);
}
// 
static inline bool               strisempty(const char *str) {
    return str == NULL || *str == '\0';
}

// ----------------------------------- CHAR ----------------------------------------
// isalpha or '_'
static inline bool              isalpha_u(int c){
    return isalpha(c) || c == '_';
}

// isalnum or '_'
static inline bool              isalnum_u(int c){
    return isalnum(c) || c == '_';
}

static inline bool              isdigit_signed(int c){
    return isdigit(c) || c == '+' || c == '-';
}

static inline int               clower(int c, bool lower){
    return lower ? tolower(c) : c;
}

static inline int               cupper(int c, bool upper){
    return upper ? toupper(c) : c;
}
// unsigned char max normalizer
static inline unsigned char     ucharmax(int c) {
    if (c > UCHAR_MAX)
        c = UCHAR_MAX;
    return c;
}

typedef enum {SIZE_NONE = 0, SIZE_POWER2, SIZE_MIN10 } Tincrease;

/**
 * @brief Computes the target allocation size based on the specified growth strategy.
 *
 * This function transforms a requested minimum size into a concrete allocation 
 * size by applying the chosen growth policy. It includes defensive checks for 
 * minimum bounds and error handling for invalid strategies.
 *
 * @details The result depends on the @p t parameter:
 * - @c SIZE_NONE: Returns the requested size @p n exactly.
 * - @c SIZE_MIN10: Returns @p n, but ensures the result is at least 10. 
 *   This provides a safety buffer even if @p n is less than 10 or negative.
 * - @c SIZE_POWER2: Rounds the size up to the next power of two using 
 *   @ref round_up_2.
 * - @c DEFAULT: If an unknown strategy is provided, returns -1 as an error sentinel.
 *
 * @param t The growth strategy to apply (@c Tincrease).
 * @param n The minimum requested size.
 * @return The calculated size, or -1 if an invalid strategy is provided.
 *
 * @note The return value -1 serves as an error marker. Callers should check 
 *       if the return value is negative before using it for allocation.
 *
 * @complexity O(1) for most strategies, or O(k) when using @c SIZE_POWER2 
 *             (where k is the number of set bits in @p n).
 */
 static inline long               calcnewsize(Tincrease t, long n){
    long        sz = n;
    switch (t){
        case SIZE_NONE:
            // do, nothing
        break;
        case SIZE_MIN10:
            if (sz < 10)    // even if negative: be carefull!
                sz = 10;
        break;
        case SIZE_POWER2:
            sz = round_up_2(sz);
        break;
        default:
            logsimple("Unknow size grouth type %d", t);
            sz = -1;    // fail mark
        break;
    }
    return sz; //logsimpleret(sz, "newsz = %d", sz);
}

// int SQL not in ver2 (with size)
static inline bool              common_int_notin2(int val, const int *arr, int sz){
    const int *iter = arr;
    while (iter - arr < sz)
        if (*iter++ == val)
            return false;
    return true;
}
// int SQL in ver2 (with size)
static inline bool              common_int_in2(int val, const int *arr, int sz){
    const int *iter = arr;
    while (iter - arr < sz)
        if (*iter++ == val)
            return true;
    return false;
}
#define int_in(val, ...)    common_int_in2   ( (val), (const int []) { __VA_ARGS__ }, COUNT(((const int[]){__VA_ARGS__})) )
#define int_notin(val, ...) common_int_notin2( (val), (const int []) { __VA_ARGS__ }, COUNT(((const int[]){__VA_ARGS__})) )

//#define int_notin(val, ...) common_int_notin2( (val), (const int []) { __VA_ARGS__ }, COUNT((const int[]){__VA_ARGS__}) )
//#define int_in(val, ...)    common_int_in2   ( (val), (const int []) { __VA_ARGS__ }, COUNT((const int[]){__VA_ARGS__}) )

// int SQL not in
static inline bool              common_int_notin(int val, const int *arr){
    while (*arr != val)
        if (val == *arr++)
            return false;
    return true;
}
// int SQL in
static inline bool              common_int_in(int val, const int *arr){
    while (*arr != INT_MIN)
        if (val == *arr++)
            return true;
    return false;
}
/*
// TODO: think about typeof
#define                         int_notin(val, ...) common_int_notin( (val),  (const int []) { __VA_ARGS__, INT_MIN} )
#define                         int_in(val, ...)    common_int_in( (val),     (const int []) { __VA_ARGS__, INT_MIN} )
*/

// THAT IS OBSOLETE! Use ds_adapter API
extern bool              try_parse_int(const char *restrict str, int *restrict res);
extern bool              try_parse_long(const char *restrict str, long *restrict res);
extern bool              try_parse_double(const char *restrict str, double *restrict res);
extern bool              try_parse_uint(const char *restrict str, unsigned *restrict res);
extern bool              try_parse_ulong(const char *restrict str, unsigned long *restrict res);
extern bool              try_parse_char(const char *restrict str, char *restrict res);
extern bool              try_parse_bool(const char *restrict str, bool *restrict res);

// ----------------------------------- IO -------------------------------------
#define IOCHECKER(w, cmd, ret) \
    for (long w = (cmd), _once = 1; _once; _once = 0) \
        if (w < 0) \
            return userraise( (ret), ERR_STREAM_ERROR, "IO error"); \
        else

// simple return
#define WRITE_OR_RET_NOLOG(cmd, ret)                                \
    ({ long _w = (cmd);                                             \
       if (_w < 0)                                                  \
           return (ret);   \
       _w; })
// logging error
#define WRITE_OR_RET(cmd, ret)                                      \
    ({ long _w = (cmd);                                             \
       if (_w < 0)                                                  \
           return userraise((ret), ERR_STREAM_ERROR, "IO error");   \
       _w; })
// free or fclose or release
#define WRITE_OR_RET_ACTION(cmd, ret, act) \
    ({ long _w = (cmd); \
       if (_w < 0) { \
            (act); \
           return userraise((ret), ERR_STREAM_ERROR, "IO error"); }  \
       _w; })

// simple return
#define IOCHECKERSIMPLE(w, cmd, ret) \
    for (long w = (cmd), _once = 1; _once; _once = 0) \
        if (w < 0) \
            return (ret); \
        else

#define IOCHECKERACTION(w, cmd, ret, act) \
    for (long w = (cmd), _once = 1; _once; _once = 0) \
        if (w < 0) {\
            (act); \
            return userraise( (ret), ERR_STREAM_ERROR, "IO error"); \
        } else

#endif /* ! _COMMON_H */
