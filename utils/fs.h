#ifndef _FS_H
#define _FS_H

// ---------------------------------------------------------------------------------
// --------------------------- Public Faststring API -------------------------------
// ---------------------------------------------------------------------------------

// --------------- Includes -----------------------

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
//
#include "bool.h"
#include "log.h"
#include "common.h"
#include "error.h"
#include "checker.h"

// ----------- CONSTANTS AND GLOBALS ---------------

// ------------------- TYPES -----------------------

typedef enum  {
               FS_FLAG_DEFUNCT      = 0x0
             , FS_FLAG_ALLOC        = 0x10     // standard allocation
             , FS_FLAG_STATIC       = 0x1
             , FS_FLAG_CONST        = 0x2     // Not user for now
             , FS_FLAG_LOCAL        = 0x4
             , FS_FLAG_BODYALLOC    = 0x8     // for fsarr_move()
} FS_FLAGS;

// type-support functions

// to string
static inline const char * fs_flag_str(int flag){
    switch(flag){
        CASE_RETURN(FS_FLAG_STATIC);        // not sure whether is good to use CASE_RETURN
        CASE_RETURN(FS_FLAG_CONST);
        CASE_RETURN(FS_FLAG_LOCAL);
        CASE_RETURN(FS_FLAG_ALLOC);
        CASE_RETURN(FS_FLAG_BODYALLOC | FS_FLAG_ALLOC);
        default:
            return "Unknown action";
    }
}

// faststring
typedef struct fs {
    size_t      len, sz; // sz >= len + 1 because of last '\0'
    int         flags;
    char       *v;      // with '\0'
} fs;

// flag checkers
static inline bool          fs_flag_static(FS_FLAGS fl){
    return fl & FS_FLAG_STATIC;
}

static inline bool          fs_static(const fs *s){
    return fs_flag_static(s->flags);
}

static inline bool          fs_flag_const(FS_FLAGS fl){
    return fl & FS_FLAG_CONST;
}

static inline bool          fs_const(const fs *s){
    return fs_flag_const(s->flags);
}

static inline bool          fs_flag_local(FS_FLAGS fl){
    return fl & FS_FLAG_LOCAL;
}

static inline bool          fs_local(const fs *s){
    return fs_flag_local(s->flags);
}

static inline bool          fs_flag_alloc(FS_FLAGS fl){
    return fl & FS_FLAG_ALLOC; // heap marker
}

static inline bool          fs_alloc(const fs *s){
    return s && fs_flag_alloc(s->flags);
}

static inline bool          fs_flag_bodyalloc(FS_FLAGS fl){
    return fl & FS_FLAG_BODYALLOC;
}

static inline bool          fs_bodyalloc(const fs *s){
    return fs_flag_bodyalloc(s->flags);
}

static inline bool          fs_flag_defunct(FS_FLAGS fl){
    return fl & FS_FLAG_DEFUNCT;
}

static inline bool          fs_defunct(const fs *s){
    return s && fs_flag_defunct(s->flags);
}

// ------------- CONSTRUCTOTS/DESTRUCTORS ----------

/**
 * @brief Copies the logical content from the source buffer to the destination buffer.
 *
 * This function copies @c s->len bytes from @p src to @p dest. 
 * It is "smart" — if the destination buffer is dynamic (@c FS_FLAG_ALLOC) 
 * and has insufficient capacity to hold the source content, it will 
 * automatically attempt to expand @p dest using @ref increasesize.
 *
 * @param[in]  src   The source @c fs buffer.
 * @param[out] dest  The destination @c fs buffer.
 *
 * @return Returns 0 on success, or an error code/EOF if expansion fails 
 *         or if @p src is invalid.
 *
 * @warning If @p dest is a dynamic buffer and a reallocation occurs, 
 *          all existing pointers to @p dest become invalid.
 * @note If @p src and @p dest overlap in memory, the behavior is undefined 
 *       (it is recommended to ensure they are distinct buffers).
 */
static inline fs             fscopyf(const char *fmt, ...) __attribute__ (( format (printf, 1, 2) ) );

/**
 * @brief Writes a formatted string into the buffer starting at the current position.
 *
 * This function acts like @c sprintf, but it is aware of the buffer's capacity.
 * It writes the formatted string into the @c fs buffer starting at @p s->pos.
 * If the resulting string exceeds the current capacity, the buffer will be 
 * automatically resized via @ref increasesize.
 *
 * @param[in,out] s      The target @c fs buffer.
 * @param[in]     fmt    Format control string (standard printf syntax).
 * @param[...]    args   Variable arguments for formatting.
 *
 * @return Returns the number of characters written (not including the null terminator), 
 *         or a negative value if an error or expansion failure occurs.
 *
 * @note The function automatically updates @p s->pos and @p s->len after 
 *       a successful write. It also ensures the buffer is null-terminated 
 *       if @c FS_FLAG_ALLOC is set.
 * @warning This function may trigger a `realloc` and/or throw a system 
 *          exception if memory allocation fails.
 */
extern long                  fs_vsprintf_position(fs *restrict s, size_t pos, const char *restrict fmt, va_list ap);

#define             FSEMPTY (fs){.sz = 0, .len = 0, .flags = FS_FLAG_STATIC, .v = ""}
#define             FSLITERAL(val) (fs){.sz = strlen(val) + 1, .len = strlen(val), .flags = FS_FLAG_STATIC, .v = (char *) (val) }
#define             FSINITSTATIC(...)  (fs){.sz = 1, .len = 0, .flags = FS_FLAG_STATIC, .v = "", ##__VA_ARGS__}

#define             FS(...)   (fs){.sz = 0, .len = 0, .flags = FS_FLAG_ALLOC, .v = 0, ##__VA_ARGS__}
#define             FSDEFUNCT FS(.flags = FS_FLAG_DEFUNCT)

#define             fsfree(s) fs_free(&(s))

#define             fsfreeall(...) fs_freeall( (fs *[]){__VA_ARGS__, 0} )

#define             CONCATENATE(prefix, name) prefix ## _ ## name
#define             fslocal(name, leng) char CONCATENATE(_FS_TMP_, name)[(leng) + 1];\
 fs name = (fs) {.len = (leng), .sz = (leng) + 1, .flags = FS_FLAG_LOCAL, .v = CONCATENATE(_FS_TMP_, name) }

/**
 * @brief Releases all resources associated with the @c fs object.
 *
 * For buffers initialized with @c FS_FLAG_ALLOC, this function deallocates 
 * the heap-allocated memory. For static buffers (fixed size), this 
 * function does nothing.
 *
 * @param[in,out] s The @c fs buffer to be cleared.
 *
 * @warning After calling this function, the buffer @p s is in an invalid state. 
 *          All pointers previously obtained from @p s (via @ref elem0) 
 *          become dangling and must not be accessed.
 * @note This function resets the buffer state (sets @p s->v = NULL, @p s->sz = 0, etc.) 
 *       to prevent accidental reuse of freed memory.
 */
extern void                 fs_free(fs *s);

// mass destructor
static inline void          fs_freeall(fs **arr){
    fs  *s;
    while ( (s = *arr++) )
        if (s->v)
            fs_free(s);
}
// --------------------------------------------------------------------------
extern fs                   fs_clone(const fs *s);

static inline fs            fsclone(fs s){
    return fs_clone(&s);
}
// create ONLY fs body with FS_FLAG_BODYALLOC
extern fs                  *fs_create(void);

// fs HEAP creator !
extern fs                  *fs_heapcreate(const fs *orig);

// lit MUST BE static!
static inline fs            fsliteral(const char *lit){
    fs s = FSEMPTY;
    s.v = (char *) lit;
    s.len = strlen(lit);
    s.sz = s.len + 1;
    return s;
}
extern fs                   fsinit(size_t sz);

// wrapper for functions
static inline fs           *fs_init_or_use(fs *s) {
    if (s)
        return s;
    else
        return fs_create();
}
//
static inline fs            fsempty(void){
    return FS();
}
// body local copy from char *
static inline fs            fscopy(const char *str){
    fs         tmp = fsliteral(str);
    return  fs_clone(&tmp);
}
//  fs HEAP copy creator !
static inline  fs          *fs_heapcopy(const char *str){
    fs         tmp = fsliteral(str);    // FSLITERAL ??? TODO:
    return fs_heapcreate(&tmp);
}

// TODO: fs_const() NOT SURE

static inline fs            fscopyf(const char *fmt, ...) {
    invraisecode(fmt != 0, ERR_NULLABLE_PTR, "Null pointer");
    fs s = FS();
    va_list ap;
    va_start(ap, fmt);
    fs_vsprintf_position(&s, 0, fmt, ap);
    va_end(ap);
    return s;
}

// -------------------- ACCESS AND MODIFICATORS ------------------------

static inline long           fs_sprintf(fs *restrict s, const char *restrict fmt, ...) 
    __attribute__ (( format (printf, 2, 3) ) );
static inline long           fs_sprintf_concat(fs *restrict s, const char *restrict fmt, ...)
    __attribute__ (( format (printf, 2, 3) ) );

// move only heap alloc fs, TO BE REMOVED
extern fs                    fs_move(fs *orig);
//
extern fs                   *fs_moveto(fs *dst, fs *src);
// move whole fs (body and string)
extern fs                   *fs_moveto_heap(fs *orig);
// actually this is MOVE CONSTRUCTOR, *orig MUST be heap allocated c-str
// move to heap fs (FS_FLAG_BODYALLOC) whole c-str
extern fs                   *fs_moveto_heapstr(char **orig);
// semi-free! move-destructor for normal or BODY_ALLOC fs
char                        *fs_movetostr(fs *ps);
// semi-free! move-destructor for BODY_ALLOC
// extern char                 *fs_movefrom_heapstr(fs **pfs);

// direct access, NO change len or sz, position MUST be < sz
static inline char          *fs_get(const fs *s, size_t pos){
    //return logsimpleret(s->v + pos, "Getting %p[%c]", s->v + pos, s->v[pos]);
    return s->v + pos;
}

/**
 * @brief Retrieves the element at the specified position.
 * 
 * If the requested position exceeds the current capacity, the buffer 
 * is automatically resized to accommodate the new index.
 *
 * @param s Pointer to the file system structure.
 * @param pos The index of the element to retrieve.
 * @return Pointer to the character at the specified position.
 */
extern char                 *fs_elem(fs *s, size_t pos);

/**
 * @brief Retrieves an element and ensures space for a null terminator.
 * 
 * Automatically expands the buffer capacity if the position plus one 
 * exceeds current bounds. Also updates the logical length to include 
 * the space for a null terminator at (pos + 1).
 *
 * @param s Pointer to the file system structure.
 * @param pos The index of the element to retrieve.
 * @return Pointer to the character at the specified position.
 */
extern char                 *fs_elem0(fs *s, size_t pos);

static inline char          *fs_setlen(fs *s, size_t poslen){
    s->len = poslen;
    *fs_elem(s, poslen) = '\0';
    return fs_get(s, poslen);
}

static inline char          *fs_clear(fs *s){
    return fs_setlen(s, 0);
}

static inline size_t         fs_len(const fs *s){
    return s->len;
}

static inline size_t         fslen(fs s){
    return s.len;
}
// just and FS() or after freed
static inline bool           fsisnull(fs s){
    return s.sz == 0 && s.v == 0;
}
static inline bool           fs_isnull(const fs *s){
    return s == NULL || (s->sz == 0 && s->v == 0);
}
// check in fs is empty! stacked version
static inline bool           fsisempty(fs s) {
    return s.v == 0 || *s.v == '\0';
}
// check in fs is empty! pointer version
static inline bool           fs_isempty(const fs *s){
    return s == NULL || s->v == 0 || *s->v == '\0';
}
//
static inline bool           fs_isheapalloc(const fs *s){
    return s != NULL && fs_alloc(s);
}

static inline size_t        fssz(fs s){
    return s.sz;
}
static inline size_t        fs_sz(const fs *s){
    return s->sz;
}
// shrink to real len + 1 ( + 1 because '\0' is ASSUMED)
extern fs                   *fs_shrink(fs *s);

extern fs                   *fs_resize(fs *s, size_t newsz);

static inline fs            *fs_increase(fs *s, size_t inc){
    return fs_resize(s, s->sz + inc);
}

static inline char          *fs_str(fs *s){
    return s->v;
}

static inline const char    *fs_strdup(fs *s){
    return (const char *) strdup(s->v);
}

// ------------------------------------ FS Comparators --------------------------------------

// fs vs fs

// pointer version
static inline int            fs_cmp(const fs *restrict str1, const fs *restrict str2){
    return strcmp(str1->v, str2->v);
}
// pointer version, limited
static inline int            fs_ncmp(const fs *restrict str1, const fs *restrict str2, size_t len){
    return strncmp(str1->v, str2->v, len);
}
// local version
static inline int            fscmp(fs str1, fs str2){
    return strcmp(str1.v, str2.v);
}
// local version, limited
static inline int            fsncmp(fs str1, fs str2, size_t len){
    return strncmp(str1.v, str2.v, len);
}
// STRICT pointer version
static inline bool           fs_cmp_strict(const fs *restrict str1, const fs *restrict str2){
    return fs_len(str1) == fs_len(str2) && fs_cmp(str1, str2) == 0;
}
// STRICT local version
static inline bool           fscmp_strict(fs str1, fs str2){
    return fslen(str1) == fslen(str2) && fscmp(str1, str2) == 0;
}
// pointer version, insensitive
static inline int            fs_icmp(const fs* restrict str1, const fs* restrict str2){
    return strcasecmp(str1->v, str2->v);
}
// pointer version, insensitive, limited
static inline int            fs_nicmp(const fs* restrict str1, const fs* restrict str2, size_t len){
    return strncasecmp(str1->v, str2->v, len);
}
// local version, insensitive
static inline int            fsicmp(fs str1, fs str2){
    return strcasecmp(str1.v, str2.v);
}
// local version, insensitive, limited
static inline int            fsnicmp(fs str1, fs str2, size_t len){
    return strncasecmp(str1.v, str2.v, len);
}

// fs vs c-str

// pointer version
static inline int            fs_cmpstr(const fs *restrict str1, const char *restrict str2){
    return strcmp(str1->v, str2);
}
// pointer version, limited
static inline int            fs_ncmpstr(const fs *restrict str1, const char *restrict str2, size_t len){
    return strncmp(str1->v, str2, len);
}
// local version
static inline int            fscmpstr(fs str1, const char *str2){
    return strcmp(str1.v, str2);
}
// local version, limited
static inline int            fsncmpstr(fs str1, const char *str2, size_t len){
    return strncmp(str1.v, str2, len);
}
// pointer version, insensitive
static inline int            fs_icmpstr(const fs* restrict str1, const char *restrict str2){
    return strcasecmp(str1->v, str2);
}
// pointer version, insensitive, limited
static inline int            fs_nicmpstr(const fs* restrict str1, const char *restrict str2, size_t len){
    return strncasecmp(str1->v, str2, len);
}
// local version, insensitive
static inline int            fsicmpstr(fs str1, const char *str2){
    return strcasecmp(str1.v, str2);
}
// local version, insensitive, limited
static inline int            fsnicmpstr(fs str1, const char *str2, size_t len){
    return strncasecmp(str1.v, str2, len);
}

// ------------------------------------------ substring srearch ---------------------------------------------------
// pointer, sensitive, unlim
static inline long           fs_instr(const fs* restrict str1, const fs* restrict str2){
    const char *p = strstr(str1->v, str2->v);
    return p == 0 ? -1 : p - str1->v;
}
// pointer, insensitive, unlim
static inline long           fs_iinstr(const fs* restrict str1, const fs* restrict str2){
    const char *p = strcasestr(str1->v, str2->v);   // NOTE: not portable solution
    return p == 0 ? -1 : p - str1->v;
}
// local, sensitive, unlim
static inline long           fsinstr(fs str1, fs str2){
    const char *p = strstr(str1.v, str2.v);   // NOTE: not portable solution
    return p == 0 ? -1 : p - str1.v;
}
// local, insensitive, unlim
static inline long           fsiinstr(fs str1, fs str2){
    const char *p = strcasestr(str1.v, str2.v);   // NOTE: not portable solution
    return p == 0 ? -1 : p - str1.v;
}
// TODO: fs_ninstr(fs *, fs *, int); fs_niinstr(fs *, fs *, n);
// common search for limited!
extern long                  fs_lim_instr(const fs* restrict str1, const fs* restrict str2, size_t lim, bool lowercase);

// pointer, sensitive, lim
static inline long           fs_ninstr(const fs* restrict str1, const fs* restrict str2, size_t lim){
    return  fs_lim_instr(str1, str2, lim, false);
}
// pointer, insensitive, lim
static inline long           fs_niinstr(const fs* restrict str1, const fs* restrict str2, size_t lim){
    return  fs_lim_instr(str1, str2, lim, true);
}
// local, sensitive, lim
static inline long           fsninstr(fs str1, fs str2, size_t lim){
    return  fs_lim_instr(&str1, &str2, lim, false);
}
// local, insensitive, lim
static inline long           fsniinstr(fs str1, fs str2, size_t lim){
    return  fs_lim_instr(&str1, &str2, lim, true);
}
// ------------------------------------------ one symbol search --------------------------------------------------------
// pointer, sensitive, unlim (library call)
static inline long           fs_chr(const fs *str, char c){
    const char *p = strchr(str->v, c);
    return p ? p - str->v : -1;
}
// pointer, sensitive, lim
static inline long           fs_nchr(const fs *str, char c, size_t lim){
    const char *p = str->v;
    while (lim-- > 0 && *p != '\0') {
        if (*p == c)
            return p - str->v;
        p++;
    }
    return -1;
}
// pointer, insensitive, lim
static inline long           fs_nichr(const fs *str, char c, size_t lim){
    const char *p = str->v;
    c = tolower(c);
    while (lim-- > 0 && *p != '\0') {
        if (*p == c)
            return p - str->v;
        p++;
    }
    return -1;
}
// pointer, insensitive, unlim
static inline long           fs_ichr(const fs *str, char c){
    return fs_nichr(str, c, INT_MAX);
}
// local version
// local, sensitive, unlim
static inline long           fschr(fs str, char c){
    return fs_chr(&str, c);
}
// local, sensitive, lim
static inline long           fsnchr(fs str, char c, size_t lim){
    return fs_nchr(&str, c, lim);
}
// local, insensitive, lim
static inline long           fsnichr(fs str, char c, size_t lim){
    return fs_nichr(&str, c, lim);
}
// local, insensitive, unlim
static inline long           fsichr(fs str, char c){
    return fs_ichr(&str, c);
}
// ------------------------------------------ ONE SYMBOL REVERSE SEARCH  --------------------------------------------------------
// pointer, sensitive, unlim
static inline long           fs_rchr(const fs *str, char c){
    const char *p = str->v;
    long        pos = (long) str->len - 1;
    while (pos >= 0 && p[pos] != c)
        pos--;
    return pos;
}
// pointer, insensitive, unlim
static inline long           fs_irchr(const fs *str, char c){
    const char *p = str->v;
    int         pos = (long) str->len - 1;
    c = tolower( (unsigned char) c);
    while (pos >= 0 && tolower( (unsigned char) p[pos] ) != c)
        pos--;
    return pos; // tolower( (unsigned char) p[pos] ) == c ? pos : -1;
}
// local, sensitive, unlim
static inline int            fsrchr(fs str, char c){
    return fs_rchr(&str, c);
}
// pointer, insensitive, unlim
static inline int            fsirchr(fs str, char c){
    return fs_irchr(&str, c);
}
// ------------------------------------------------------ EXCNAHGERS ------------------------------------------------------------

static inline void           fs_exch(fs *s1, fs *s2){
    fs tmp = *s1;
    *s1 = *s2;
    *s2 = tmp;
}

static inline fs             fs_reverse(fs str){
    reverse(str.v, str.len);
    return str;
}

extern void                  fs_sort(fs *s, bool asc);
// ----------------------------------------------------------------- CAT/CPY/ETC -------------------------------------------------
extern fs                    fs_cat(fs *target, fs source);
// concatenate c-str
static inline fs             fs_catstr(fs *restrict target, const char *restrict source) {
    fs l = fsliteral(source);
    return fs_cat(target, l);
}
// concatenant c-str with size, INTERNAL
static inline fs            fs_catmem(fs *restrict target, const char *restrict source, size_t len) {
    fs l = (fs) { .v = (char *) source, .flags = FS_FLAG_STATIC, .len = len, .sz = len + 1};
    fs_cat(target, l);
    fs_setlen(target, target->len);
    
    return *target;
}
// put beginner at the start position of the target
extern fs                    fs_rev_catstr(fs *restrict target, const char *restrict beginner);

static inline fs             fs_cpy(fs *target, fs source) {
    target->len = 0;
    return fs_cat(target, source);
}

static inline fs             fs_cpystr(fs *restrict target, const char *restrict source){
    fs l = fsliteral(source);
    return fs_cpy(target, l);
}

static inline long           fs_sprintf(fs *restrict s, const char *restrict fmt, ...) {
    invraisecode(s != 0 && fmt != 0, ERR_NULLABLE_PTR, "%p - %p", s, fmt);
    va_list ap;
    va_start(ap, fmt);
    int res = fs_vsprintf_position(s, 0, fmt, ap);
    va_end(ap);
    return res;
}

static inline long           fs_sprintf_concat(fs *restrict s, const char *restrict fmt, ...) {
    invraisecode(s != 0 && fmt != 0, ERR_NULLABLE_PTR, "%p - %p", s, fmt);
    va_list ap;
    va_start(ap, fmt);
    int res = fs_vsprintf_position(s, s->len, fmt, ap);
    va_end(ap);
    return res;
}
// sprintf to particular position
extern long                  fs_sprintf_position(fs *restrict s, size_t pos, const char *restrict fmt, ...)
    __attribute__ (( format (printf, 3, 4) ) );

// fast in-place left substring
static inline fs             fs_left(fs *s, size_t cnt){
    if (cnt > s->len)
        cnt = s->len;
    fs_setlen(s, cnt);
    return *s;
}

// fast in-place!
extern fs                    fs_substr(fs *s, size_t from, size_t len);

// constructor version
extern fs                    fs_newsubstr(const fs *s, size_t from, size_t len);

static inline fs            *fs_tolower_interval(fs *str, size_t from, size_t to){
    // TODO: probably fs_iter - think about it
    for (size_t i = from; i < MIN(to, str->len); i++)
        str->v[i] = tolower(str->v[i]);
    return str;
}

static inline fs            *fs_tolower(fs *str){
    return fs_tolower_interval(str, 0, str->len);
}

static inline fs            *fs_toupper_interval(fs *str, size_t from, size_t to){
    // TODO: probably fs_iter - think about it
    for (size_t i = from; i < MIN(to, str->len); i++)
        str->v[i] = toupper(str->v[i]);
    return str;
}
static inline fs            *fs_toupper(fs *str){
    return fs_toupper_interval(str, 0, str->len);
}
extern fs                    fs_rpad(fs *restrict str, size_t len, const fs *restrict pad);
extern fs                    fs_lpad(fs *restrict str, size_t len, const fs *restrict pad);
static inline fs             fs_rpadstr(fs *restrict str, size_t len, const char *restrict padstr){
    fs l = fsliteral(padstr);
    return fs_rpad(str, len, &l);
}
static inline fs             fs_lpadstr(fs *restrict str, size_t len, const char *restrict padstr){
    fs l = fsliteral(padstr);
    return fs_lpad(str, len, &l);
}

/**
 * @brief Fill a fs with random characters of a given class.
 * @param s    pointer to fs to fill
 * @param len  desired length
 * @param type 'a' for lowercase, 'A' for uppercase, '0' for digits
 */
static inline fs            *fs_genrnd(fs *s, size_t len, char type) {
    invraisecode(s != NULL, ERR_NULLABLE_PTR, "Nullable fs");

    for (size_t i = 0; i < len; i++) {
        char    c;
        switch (type) {
            case 'a': c = rndlowchar(); 
                break;
            case 'A': c = rndupperchar(); 
                break;
            case '0': c = rnddigitchar(); 
                break;
            default:  c = '?';
                break;
        }
        *fs_elem(s, i) = c;
    }
    if (len > fs_len(s) )
        fs_setlen(s, len);
    return s;
}

/**
 * @brief consructor fs with random characters of a given class.
 * @param len  desired length
 * @param type 'a' for lowercase, 'A' for uppercase, '0' for digits
 */
static inline fs             fs_initrnd(size_t len, char type) {
    fs tmp = FS();
    return *fs_genrnd(&tmp, len, type);
}

// ----------------------------------- Finders -----------------------------------
// full scan version
static inline bool           fs_notin(fs s, const char *strs[]){
    while (*strs){
        if (strcmp(s.v, *strs) == 0)
            return false;
        else
            strs++;
    }
    return true;
}

// full scan version, insensitive
static inline bool           fs_inotin(fs s, const char *strs[]){
    while (*strs){
        if (strcasecmp(s.v, *strs) == 0)
            return false;
        else
            strs++;
    }
    return true;
}

// full scan version
static inline bool           fs_in(fs s, const char *strs[]){
    while (*strs){
        if (strcmp(s.v, *strs) == 0)
            return true;
        else
            strs++;
    }
    return false;
}

// full scan version, insensitive
static inline bool           fs_iin(fs s, const char *strs[]){
    while (*strs){
        if (strcasecmp(s.v, *strs) == 0)
            return true;
        else
            strs++;
    }
    return false;
}

// TODO: ordered version, via binsearch

// try to parse value
static inline int            fs_getintpos(const fs *ps, size_t pos){
    int     res;
    if (!try_parse_int(ps->v + pos, &res) )
        userraiseint(ERR_INVALID_CONVERSION, "Unablew to convert to int %.50s", ps->v + pos);            // this must be configurable! 
    return res;
}
//
static inline int            fs_getint(const fs *ps){
    return fs_getintpos(ps, 0);
}

// try to parse long value
static inline long           fs_getlongpos(const fs *ps, size_t pos) {
    long     res;
    if (!try_parse_long(ps->v + pos, &res) )
        userraiseint(ERR_INVALID_CONVERSION, "Unable to convert to long %.50s", ps->v + pos);            // this must be configurable! 
    return res;
}
//
static inline long            fs_getlong(const fs *ps) {
    return fs_getlongpos(ps, 0);
}
// try to parse long value
static inline unsigned long  fs_getulongpos(const fs *ps, size_t pos) {
    unsigned long     res;
    if (!try_parse_ulong(ps->v + pos, &res) )
        userraiseint(ERR_INVALID_CONVERSION, "Unable to convert to unsigned long %.50s", ps->v + pos);            // this must be configurable! 
    return res;
}
//
static inline unsigned long   fs_getulong(const fs *ps) {
    return fs_getulongpos(ps, 0);
}
// try to parse double value
static inline double          fs_getdoublepos(const fs *ps, size_t pos) {
    double     res;
    if (!try_parse_double(ps->v + pos, &res) )
        userraiseint(ERR_INVALID_CONVERSION, "Unablew to convert to double %.50s", ps->v + pos);            // this must be configurable!
    return res;
}
//
static inline double          fs_getdouble(const fs *ps) {
    return fs_getdoublepos(ps, 0);
}
// probably null checking will be here
static inline char             fs_getchar(const fs *ps) {
    return *ps->v;
}

#define                      get(s, pos) *fs_get(&(s), (pos) )
#define                      getv(s) (s.v)
#define                      elem(s, pos) *fs_elem( &(s), (pos) )
#define                      elem0(s, pos) *fs_elem0( &(s), (pos) )

#define                      fsetlen(s, poslen) *fs_setlen( (&s), (poslen) )
// NOT sure if need fsend()
#define                      fsend(s, poslen) *fs_setlen( (&s), (poslen) )
#define                      fsclear(s) *fs_clear(&s)

#define                      fsstr(s) fs_str(&(s) )

#define                      fsincrease(s, inc) fs_increase( &(s), (inc) )

#define                      fsshrink(s) fs_shrink(&(s) )

#define                      fscat(t, s) fs_cat(&(t), s)
#define                      fscatstr(t, s) fs_catstr(&(t), s)
#define                      fsrevcatstr(t, s) fs_rev_catstr( &(t), s)

#define                      fscpy(t, s) fs_cpy(&(t), s)
#define                      fscpystr(t, s) fs_cpystr(&(t), s)

#define                      fssprintf(s, fmt, ...) fs_sprintf(&(s), (fmt), ##__VA_ARGS__)

#define                      fssubstr(s, from, to) fs_substr( &(s), from, to)
#define                      fsnewsubstr(s, from, to) fs_newsubstr( &(s), from, to)

#define                      fs_ifnotin(s, ...)  fs_notin ( (s), (const char *[]) { __VA_ARGS__, 0 } )
#define                      fs_ifinotin(s, ...) fs_inotin( (s), (const char *[]) { __VA_ARGS__, 0 } )
#define                      fs_ifin(s, ...)  fs_in ( (s), (const char *[]) { __VA_ARGS__, 0 } )
#define                      fs_ifiin(s, ...) fs_iin( (s), (const char *[]) { __VA_ARGS__, 0 } )

#define                      fssort(s, asc) fs_sort( &(s), (asc) )

#define                      fsgetint(s) fs_getint(s)
#define                      fsgetintpos(s, pos) fs_getintpos( &(s), (pos) )

#define                      fsgetlong(s) fs_getlong(s)
#define                      fsgetlongpos(s, pos) fs_getlongpos( &(s), (pos) )

#define                      fsgetdoublepos(s, pos) fs_getdoublepos( &(s), (pos) )
#define                      fsgetdouble(s) fs_getdouble(s)

#define                      fsmoveto_heap(s) fs_moveto_heap(&(s))
#define                      fsmove(s) fs_move(&(s) )

#define                      fsmovetostr(s) ({ char *tmp = fs_movetostr(s); s = NULL; tmp; } )

// ------------------------ PRINTERS/CHECKERS --------------------------

extern long                  fs_techfprint(FILE *restrict out, const fs *restrict s, const char *name);
static inline long           fs_techprint(const fs *s, const char *name){
    return fs_techfprint(stdout, s, name);
}

extern long                  fs_fprint(FILE *restrict out, const fs *restrict s, const char *name);
static inline long           fs_print(const fs *restrict s, const char *restrict name){
    return fs_fprint(stdout, s, name);
}

extern long                  fs_fprintlim(FILE *restrict out, const fs *restrict s, size_t lim, const char *name);
static inline long           fs_printlim(const fs *restrict s, size_t lim, const char *restrict name){
    return fs_fprintlim(stdout, s, lim, name);
}

extern long                  fs_fprint_arr(FILE *restrict out, const fs *restrict arrs[]);
static inline long           fs_print_arr(const fs *restrict arrs[]){
    return fs_fprint_arr(stdout, arrs);
}

extern fs                   *fs_fscanf(FILE *restrict in, fs *restrict s);
static inline fs            *fs_scanf(fs *s){
    return fs_fscanf(stdin, s);
}

#define                      fstechfprint(out, s) fs_techfprint( (out), &(s), #s)
#define                      fstechprint(s)       fs_techprint( &(s), #s)

#define                      fsfprint(out, s) fs_fprint( (out), &(s), #s)
#define                      fsprint(s)       fs_print( &(s), #s)

#define                      fsfprintlim(out, s, lim) fs_fprintlim( (out), &(s), (lim), #s)
#define                      fsprintlim(s, lim)       fs_printlim( &(s), lim, #s)

#define                      fsfprint_arr(out, ...) fs_fprint_arr(out, (const fs *[]) { __VA_ARGS__, 0} )
#define                      fsprint_arr(...)       fs_print_arr( (const fs *[]) { __VA_ARGS__, 0} )

extern bool                  fs_validate(FILE *restrict out, const fs *restrict s);
extern bool                  fs_free_body_alloc_checker(int *freecnt, int *alloccnt);
extern bool                  fs_free_alloc_checker(int *freecnt, int *alloccnt);
extern bool                  fs_alloc_check(bool raise);
extern bool                  fs_fprint_checker_cnt(FILE *restrict out, const char *str);

// --------------------------------- SERIALIZATION -----------------------------------------

// seqialization (strictly FULL save into the steam with only FS and .len info), out must be opened for write
extern long                  fs_fsave(FILE *restrict out, const fs *restrict str);
extern long                  fs_save(const char *restrict fname, const fs *restrict str);

//  arr must be a pointer to NULL terminated array!
extern long                  fs_fsave_arr(FILE *restrict out, const fs *restrict arr);
extern long                  fs_save_arr(const char *restrict fname, const fs *restrict arr);

extern fs                    fs_fload(FILE *restrict in, fs *restrict s);
extern fs                    fs_load(const char *restrict fname, fs *restrict s);
// NOTE: load_arr is part of fsarr functionnality

#define                      fsfsave(f, str)        fs_fsave((f), &(str))
#define                      fssave(fname, str)     fs_save((fname), &(str))

#define                      fsfsave_arr(out, ...)  fs_fsave_arr( (out), (const fs *[]) { __VA_ARGS__, 0} )
#define                      fssave_arr(fname, ...) fs_save_arr( (fname), (const fs *[]) { __VA_ARGS__, 0} )

#define                      fsfload(f, s)          fs_fload(f, (&s) )
#define                      fsload(fname, s)       fs_load(fname, &(s) )

// ------------------------------------ ETC. ------------------------------------------------

extern size_t                   FS_MIN_ACCOC;          // to config.c (via sqlite)
extern size_t                   FS_TECH_PRINT_COUNT;   // to config.c TODO:

#endif /* !_FS_H */

