#include <stdarg.h>
#include <stdatomic.h>

#include "fs.h"
#include "error.h"
#include "fileutils.h"
#include "checker.h"
#include "guard.h"
#include "getword.h"
#include "common.h"
#include "log.h"

/********************************************************************
                    FAST STRING MODULE IMPLEMENTATION
********************************************************************/

// external contol
size_t                          FS_MIN_ACCOC            = 128;
size_t                          FS_TECH_PRINT_COUNT     = 100; // symplos to print
// mem leak checking
static atomic_int               g_alloc_cnt             = 0;
static atomic_int               g_free_cnt              = 0;
static atomic_int               g_alloc_body_cnt        = 0;
static atomic_int               g_free_body_cnt         = 0;
// ---------- pseudo-header for utility procedures -----------------

// -------------------------- (Utility) printers -------------------

// ------------------------------ Utilities -------------- ----------

/**
 * @brief Manages the capacity of an fs object.
 *
 * This function handles memory expansion for buffers allocated on the heap.
 * If the buffer is a fixed-size/static buffer (does not have @c FS_FLAG_ALLOC),
 * this function performs no resizing operations.
 *
 * @param[in,out] s      Pointer to the fs structure.
 * @param[in]     newsz  The target capacity in bytes.
 * @param[in]     init   Growth strategy flag:
 *                      - If @c true: Uses exponential growth (via @ref calcnewsize)
 *                        to ensure amortized O(1) complexity for heap-allocated buffers.
 *                      - If @c false: Attempts to resize to the exact @p newsz.
 *
 * @return Returns a log/status object containing information about the
 *         result (e.g., the final size @c s->sz).
 *
 * @note **Conditional Behavior**: The resizing logic (reallocation) is only 
 *      executed if @ref fs_alloc(s) returns true (i.e., the buffer was 
 *      initialized with @c FS_FLAG_ALLOC).
 * 
 * @warning **Pointer Invalidation**: For heap-allocated buffers, a successful 
 *          reallocation invalidates all existing pointers previously obtained 
 *          from this buffer (e.g., via @ref elem0).
 * 
 * @warning **Exception Handling**: For heap-allocated buffers, this function 
 *          may raise a system exception via @ref userraiseint if memory 
 *          allocation fails.
 * 
 * @details 
 * The internal execution flow is as follows:
 * 1. Check if @ref fs_alloc(s) is true. If false, return immediately.
 * 2. Check if the current capacity `s->sz` is sufficient for `newsz`.
 * 3. If more space is needed:
 *    a. Calculate new target size (exponential if @p init is true).
 *    b. Attempt to allocate new memory using `realloc`.
 *    c. If successful, update `s->v` and `s->sz`.
 *    d. If allocation fails, raise exception via @ref userraiseint.
 */
static fs                               increasesize(fs *s, size_t newsz, bool init){
    logenter("oldsz %zu, newsz %zu, init %s v %p, is alloc? %s", s->sz, newsz, bool_str(init), s->v, bool_str(fs_alloc(s)) );
    if (fs_alloc(s) ){
        if (init)
            newsz = calcnewsize(SIZE_POWER2, newsz);    // with increase type
        if (newsz > s->sz || !init){
                char *v = realloc(s->v, newsz);
                #if defined(FS_ALLOCATOR)
                if (init)   // init
                    s->pos = findempty();   // try to find an place
                attach(s->pos, s->v);
                #endif
                if (!v)
                    userraiseint(ERR_UNABLE_ALLOCATE, "Unable to allocate %zu bytes", newsz);
                // now it's ok
                if (s->v == 0)      // only in case of REALLY new allocation logauto(++g_alloc_cnt);
                    atomic_fetch_add(&g_alloc_cnt, 1); 
                s->v = v;
                s->sz = newsz;
         }
    }
    return logret(*s, "%s%zu", fs_alloc(s) ? "increased to " :"No change because non-heap ", s->sz);
}

/**
 * @brief Low-level copy of source data and its null terminator to the target.
 *
 * This function appends the content of @p source to @p target, starting at the 
 * current position defined by @p target->len. It also copies the null terminator 
 * (\0) from the source and updates the target's length.
 *
 * @warning **NO BOUNDS CHECKING PERFORMED.** 
 * The caller is strictly responsible for ensuring that:
 * 1. @p target->sz >= (target->len + source->len + 1).
 * 2. @p source->v contains a null terminator at @p source->v[source->len].
 *
 * @param[out] target Pointer to the destination fs structure.
 * @param[in] source  Pointer to the source fs structure.
 * @return Pointer to the updated @p target.
 */
static inline fs                       *strcopy(fs *restrict target, const fs *restrict source){
    memcpy(target->v + target->len, source->v, source->len + 1);   // with last '\0'
    target->len = target->len + source->len;
    return target;
}

// --------------------------- API ---------------------------------

// ------------------ General functions ----------------------------

// move only heap alloc fs: TO BE REMOVED
fs                                       fs_move(fs *orig){
    if (!fs_alloc(orig) )
        userraiseint(ERR_FS_NOT_ALLOC_FLAG, "Unable to move not allocated fs (type %s)", fs_flag_str(orig->flags) );    // 10001 interrupt
    fs tmp = *orig;
    *orig = FS();
    return tmp; // logsimpleret(tmp, "fs moved %d: %p", tmp.sz, tmp.v);
}
// normal move, now ANY types!
fs                                      *fs_moveto(fs *dst, fs *src) {
    if (!dst || !src)
        userraiseint(ERR_NULLABLE_PTR, "%p %p", dst, src);
    if (dst != src) {    
        fs_free(dst);
        *dst = *src;
        *src = FSDEFUNCT;
    }
    return dst;
}

// move whole fs (body and string)
fs                                      *fs_moveto_heap(fs *orig){
    invraisecode(orig != NULL, ERR_NULLABLE_PTR, "Null pointer");
    if (! (fs_alloc(orig) || fs_static(orig) ) )
        userraiseint(ERR_UNSUPPORTED_TYPE, "Unable to moveall not allocated fs (type %s)", fs_flag_str(orig->flags) );
    fs  *tmp = fs_create();
    *tmp = *orig;
    tmp->flags |= FS_FLAG_BODYALLOC;
    // clear orig
    orig->v = NULL;
    fs_free(orig);  // orig->v is null, so won't be freed, but orig can be FS_FLAG_BODYALLOC too
    return tmp;
}
// actually this is MOVE CONSTRUCTOR, *orig MUST be heap allocated c-str
// move to heap fs (FS_FLAG_BODYALLOC) whole c-str
fs                                      *fs_moveto_heapstr(char **orig){
    invraisecode(orig != NULL && *orig != NULL, ERR_NULLABLE_PTR, "Null pointer");
    fs  *tmp = fs_create();  // FS_FLAG_BODYALLOC here
    tmp->v = *orig;  // MOVE as pointer
    atomic_fetch_add(&g_alloc_cnt, 1);  //++g_alloc_cnt;      // refactoring is required TODO:
    tmp->flags |= FS_FLAG_ALLOC;
    tmp->len = strlen(tmp->v);
    tmp->sz = tmp->len + 1;
    *orig = NULL;  // clear orig
    return tmp;
}
// semi-free! move-destructor for normal or FS_FLAG_BODYALLOC fs
char                                   *fs_movetostr(fs *ps) {
    invraisecode(ps != NULL, ERR_NULLABLE_PTR, "Null pointer");
   
    char *str = ps->v;  // save the pointer (CAN be NULL here)
    if (ps->v != NULL)
        atomic_fetch_add(&g_free_cnt, 1);  //++g_free_cnt;   // inc counter only if move real data
    ps->v = NULL;   // trick with free!
    fs_free(ps);    // do nothing but free body
    
    return str;
}
//
fs                                      *fs_shrink(fs *s){
    if (fs_alloc(s)){
        size_t     newsz = s->len + 1; // final '\0' is assumed!
        increasesize(s, newsz, false);
        return logsimpleret(s, "Shrinked %zu", s->sz);
    } else
        return logsimpleret(s, "Not heap: shrink is skipped");
}

char                                    *fs_elem(fs *s, size_t pos){
    if (pos >= s->sz){
        increasesize(s, pos < FS_MIN_ACCOC ? FS_MIN_ACCOC : pos + 1, true);   // len remains the same here! sz is changed
    }
    return fs_get(s, pos);
}

char                                    *fs_elem0(fs *s, size_t pos){
    if (pos + 1 >= s->sz){ // + 1 for '\0'
        increasesize(s, pos < FS_MIN_ACCOC ? FS_MIN_ACCOC : pos, true);   // len remains the same here! sz is changed
    }
    if (s->len <= pos)   // check
        fs_setlen(s, pos + 1);
    return fs_get(s, pos);
}
// sprintf to particular position
long                                    fs_sprintf_position(fs *restrict s, size_t pos, const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long    cnt = fs_vsprintf_position(s, pos, fmt, ap);
    va_end(ap);
    return cnt;
}

// sprintf to particular position, va_list
long                                    fs_vsprintf_position(fs *restrict s, size_t pos, const char *restrict fmt, va_list ap)
{
    // logenter("len %d pos %d", s->len, pos);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    // va_end(ap);
    if (needed < 0)
        return logsimpleerr(-1L, "vsnprintf length failed");

    if (s->sz < pos + 1 + needed)
        increasesize(s, pos + 1 + needed, true);
    long cnt = vsnprintf(fs_str(s) + pos, s->sz - pos, fmt, ap2);

    s->len = pos + needed;      // note: string CAN BE CUTTED!
    va_end(ap2);
    return logsimpleret(cnt, "printed %ld to pos %zu", cnt, pos);
}

fs                                      *fs_resize(fs *s, size_t newsz){
    if (newsz > s->sz){
        increasesize(s, newsz, false);
        logsimple("size is adjusted to %zu (pos %zu)", s->sz, newsz);
    }
    return s;
}

fs                                      fs_cat(fs *target, fs source) {
    if (source.sz == 0)
        return *target;
    size_t sumlen = target->len + source.len;
    if (target->sz <= sumlen) // sz must be at least len1 + len2 + 1
        increasesize(target, sumlen + 1, true);
    
    return *strcopy(target, &source);
}

// put beginner at the start position of the target
fs                                      fs_rev_catstr(fs *restrict target, const char *restrict beginner){
    size_t len;
    if (beginner && (len = strlen(beginner) ) != 0 ){
        fs_resize(target, target->len + len);
        memmove(target->v + len, target->v, target->len);
        memcpy(target->v, beginner, len);
        fs_setlen(target, len + target->len);
        logsimple("Origin moved to the end (%zu)", len);
    }
    return *target;
}

// fast in-place!
fs                                      fs_substr(fs *s, size_t from, size_t len){
    invraisecode(s != 0 && from >= 0 && len >= 0,
        ERR_NULLABLE_PTR, "Input violation %p, from %zu, len %zu", s, from, len);     // asssertion if NOINVARIANT is NOT defined

    if (from >= s->len) {
        fs_setlen(s, 0);
        return logsimpleret(*s, "[]");
    }
    len = MIN(len, s->len - from);
    if (len > 0)
        memmove(s->v, s->v + from, len);     // TODO: probably to use strcopy?
    fs_setlen(s, len);
    return logsimpleret(*s, "[%.20s]", s->v);
}

// constructor version
fs                                      fs_newsubstr(const fs *s, size_t from, size_t len){
    invraisecode(s != 0 && from >= 0 && len >= 0,
        ERR_NULLABLE_PTR, "%p from %zu, len %zu", s, from, len);     // asssertion if NOINVARIANT is NOT defined

    if (from >= s->len) {
        fs tmp = fsinit(1);
        fsetlen(tmp, 0);
        return logsimpleret(tmp, "from %zu over length", from);
    }

    len = MIN(len, s->len - from);
    fs tmp = fsinit(len + 1);  // not possible to use fs_cpy or fs_cat here!
    if (len > 0)
        memmove(tmp.v, s->v + from, len);
    fsetlen(tmp, len);
    return logsimpleret(tmp, "Created substr %zu", tmp.len);
}
// right padding up to len
fs                                       fs_rpad(fs *restrict str, size_t len, const fs *restrict pad){
    invraisecode(str != 0 && len >= 0 && pad != 0,
            ERR_NULLABLE_PTR, "Null pointers or negative len %p %p %zu", str, pad, len);
    if (fs_len(str) >= len )
        return logsimpleret(fs_substr(str, 0, len), "Cuted to %zu", len);
    if (fs_len(pad) > 0){
        fs_resize(str, len + 1);
        for (size_t i = fs_len(str); i < len; i++)
            fs_str(str)[i] = fs_str( (fs *) pad)[ (i - fs_len(str) ) % fs_len(pad)];
        fs_setlen(str, len);
    }
    return logsimpleret(*str, "padded to %zu", fs_len(str) );
}
// left padding up to len
fs                                       fs_lpad(fs *restrict str, size_t len, const fs *restrict pad){
    invraisecode(str != 0 && len >= 0 && pad != 0,
            ERR_NULLABLE_PTR, "Null pointers or negative len %p %p %zu", str, pad, len);
    if (fs_len(str) >= len )
        return logsimpleret(fs_substr(str, 0, len), "Cuted to %zu", len);
    // str->len < len => check size
    if (fs_len(pad) > 0){
        fs_resize(str, len + 1);
        memmove(fs_str(str) + len - fs_len(str), fs_str(str), fs_len(str) );
        for (size_t i = 0; i < len - fs_len(str); i++)
            fs_str(str)[i] = fs_str( (fs *) pad)[i % fs_len(pad)];
        fs_setlen(str, len);
    }
    return logsimpleret(*str, "padded to %zu", fs_len(str) );
}
// limiter search (primitive alg)
long                                    fs_lim_instr(const fs* restrict str1, const fs* restrict str2, size_t lim, bool lowercase){
    invraise(str1 != 0 && str2 != 0 && lim >= 0, 
        "%p %p, %zu", str1, str2, lim);
    const char *s1 = str1->v, *s2 = str2->v;
    size_t pos = 0;

    for (pos = 0; pos < MIN(str1->len, lim) - str2->len; pos++){
        size_t j = pos, i = 0;
        while (clower(s1[j], lowercase) == clower(s2[i], lowercase) && s1[j] != '\0' && s2[i] != '\0' && RGUARDM)
            i++, j++;
        if (s2[i] == '\0')
            return logsimpleret(pos, "Found %zu", pos);
    }
    return logsimpleret(-1, "substr not found");
}

// sorting as array
void                                     fs_sort(fs *s, bool asc){
    invraise(s || s != 0, "Nullable pointer or fs");
    qsort(s->v, s->len, 1, asc ? pchar_cmp : pchar_revcmp);
}

// -------------------------- (API) printers -----------------------
long                                     fs_fprint(FILE *restrict out, const fs *restrict s, const char *restrict name){
    long     cnt = 0;
    if (s){
        cnt = fprintf(out, "[%s%s%s]", name ? name : "", name ? ": " : "", s->v);
    }
    return cnt;
}

// with  limit!
long                                     fs_fprintlim(FILE *restrict out, const fs *restrict s, size_t lim, const char *restrict name){
    long     cnt = 0;
    if (s){
        cnt = fprintf(out, "[%s%s%.*s]", name ? name : "", name ? ": " : "", (unsigned) lim, s->v);
    }
    return cnt;
}

long                                    fs_fprint_arr(FILE *restrict out, const fs *restrict arr[]){
    long    cnt = 0;
    size_t  i = 0;
    if (arr)
        for (; arr[i] != 0 && i < G_GLOB_AVERAGE; i++) // G_GLOB_AVERAGE to avoid endless loop
            cnt += fprintf(out, "[fs_arr_%zu: %s]", i, arr[i]->v);
    if (i == G_GLOB_AVERAGE)
        logsimple("G_GLOB_AVERAGE's reached!!! %d", G_GLOB_AVERAGE);
    return cnt;
}

long                                    fs_techfprint(FILE *restrict out, const fs *restrict s, const char *restrict name){
    // technical print, statis attributes for now
    long     cnt = 0;
    if (out){
        if (s){
            size_t     len = MIN(FS_TECH_PRINT_COUNT, s->len);
            // TODO: use guard here
            cnt += fprintf(out, "FS: %s: len [%zu], sz [%zu], flags [%d], s(%p)[%.*s", 
                        name, s->len, s->sz, s->flags, s->v, (unsigned) len, s->v);
            if (FS_TECH_PRINT_COUNT < s->len)
                cnt += fprintf(out, "...");
            cnt += fprintf(out, "]\n");
        } else
            cnt += fprintf(out, "FS: %s: <NULL>\n", name);
    }
    return cnt;
}

// out == 0 is OK fow now
bool                                    fs_validate(FILE *restrict out, const fs *restrict s){

    if (!s){    // TODO: think about string creation via faststring here!!
        if (out)
            fprintf(out, "null pointer");
        return logsimpleerr(false, "null pointer");
    }
    if (!s->v){
        if (out)
            fprintf(out, "nullable string");
        return logsimpleerr(false, "nullable string");
    }
    // depents on which iterator engine is active
    if (s->len >= s->sz){
        if (out)
            fprintf(out, "len [%zu] must be < sz [%zu]", s->len, s->sz);
        return logsimpleerr(false, "len [%zu] must be < sz [%zu]", s->len, s->sz);
    }
    {
        size_t len = strlen(s->v);
        if (len != s->len){
            if (out)
                fprintf(out, "srtlen [%zu] can't be more than len [%zu]", len, s->len);
            return logsimpleerr(false, "srtlen [%zu] can't be more than len [%zu]", len, s->len);
        }
    }
    return true;
}
// TODO: refactor that!!!!!!!
extern bool                             fs_free_body_alloc_checker(int *freecnt, int *alloccnt){
    int     lfreebodycnt = atomic_load_explicit(&g_free_body_cnt, memory_order_relaxed),
            lallocbodycnt = atomic_load_explicit(&g_alloc_body_cnt, memory_order_relaxed);
    if (freecnt)
        *freecnt = lfreebodycnt;
    if (alloccnt)
        *alloccnt = lallocbodycnt;
    return logsimpleret(lfreebodycnt == lallocbodycnt, "body allocated %d, freed %d", lallocbodycnt, lfreebodycnt);
}
// TODO: refactor that!!!!!!!
bool                                    fs_free_alloc_checker(int *freecnt, int *alloccnt){
    int     lfreecnt = atomic_load_explicit(&g_free_cnt, memory_order_relaxed),
            lalloccnt = atomic_load_explicit(&g_alloc_cnt, memory_order_relaxed);
    if (freecnt)
        *freecnt = lfreecnt;
    if (alloccnt)
        *alloccnt = lalloccnt;
    return logsimpleret(lfreecnt == lalloccnt, "string allocated %d, freed %d", lalloccnt, lfreecnt);
}
// TODO: refactor that!!!!!!!
// internal, for testing
static bool                             check_leak(bool raise){
    int     free_v, alloc_v;
    fs_free_alloc_checker(&free_v, &alloc_v);
    if (free_v != alloc_v){
        if (raise)
            userraiseint(WARN_MEM_LEAK_DETECTED, "string allocated %d, freed %d", alloc_v, free_v);
        else
            userraise(false, WARN_MEM_LEAK_DETECTED, "WARNING: string allocated %d, freed %d", alloc_v, free_v);
    }
    if (free_v != alloc_v)
        return false;
    if (free_v != alloc_v){
        if (raise)
            userraiseint(WARN_MEM_LEAK_DETECTED, "body allocated %d, freed %d", alloc_v, free_v);
        else
            userraise(false, WARN_MEM_LEAK_DETECTED, "WARNING: body allocated %d, freed %d", alloc_v, free_v);
    }
    return free_v == alloc_v;
}

// just a wrapper for check_leak
bool                                    fs_alloc_check(bool raise){
    return check_leak(raise);
}
// just print
bool                                    fs_fprint_checker_cnt(FILE *restrict out, const char *str){
    if (!out)
        return false;
    fprintf(out, "%.6s: g_free_cnt %d g_alloc_cnt %d g_free_body_cnt %d g_alloc_body_cnt %d\n",
         str, atomic_load_explicit(&g_free_cnt, memory_order_relaxed)
            , atomic_load_explicit(&g_alloc_cnt, memory_order_relaxed)
            , atomic_load_explicit(&g_free_body_cnt, memory_order_relaxed)
            , atomic_load_explicit(&g_alloc_body_cnt, memory_order_relaxed));
    return true;
}

// --------------------------------- SERIALIZATION -----------------------------------------

// seqialization (strictly FULL save into the steam with only FS and .len info), out must be opened for write
long                                     fs_fsave(FILE *restrict out, const fs *restrict str){
    long cnt = 0;
    if (out){
        fprintf(out, "FS(%zu):[%s]\n", str->len, str->v);
        cnt++;
    }
    return cnt;
}

long                                     fs_save(const char *restrict fname, const fs *restrict str){ 
    FILE *out = fopen(fname, "w");
    long     cnt = 0;
    if (out){
        cnt = fs_fsave(out, str);
        fclose(out);
    } else
        userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s for write", fname);
    return cnt;
}

//  arr must be a pointer to NULL terminated array!
long                                     fs_fsave_arr(FILE *restrict out, const fs *restrict arr){
    long cnt = 0;
    while (arr && cnt < G_GLOB_AVERAGE)
        cnt += fs_fsave(out, arr++);
    return cnt;
}

// note: arr can be nullable, this mean 0 length array
long                                     fs_save_arr(const char *restrict fname, const fs *restrict arr){
    FILE *out = fopen(fname, "w");
    long     cnt = 0;
    if (out){
        cnt = fs_fsave_arr(out, arr);
        fclose(out);
    } else
        userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s for write", fname);
    return cnt;
}
//
fs                                     *fs_fscanf(FILE *restrict in, fs *restrict s){
    invraisecode(in != NULL, ERR_NULLABLE_PTR, "Null pointer %p - %p", in, s);

    fs *original_s = s;
    s = fs_init_or_use(s);
    if (getpurestring(in, s) )
        return logsimpleret(s, "Line read len %zu", fs_len(s) );
    else {
        if (!original_s)
            fs_free(s);
        return logsimpleerr(NULL, "EOF detected while fs_fscanf");
    }
}

// raise int in case of wrong format
fs                                      fs_fload(FILE *restrict in, fs *restrict s){
    invraisecode(in != 0, ERR_NULLABLE_PTR, "Null pointer %p - %p", in, s);

    // FORMAT: FS(%d):[%s]\n
    size_t       len = 0;
    char         pt1[] = "FS(", pt2[] = "):[", pt3[] = "]\n";

    // TODO: refactor that
    // TODO: FUSKIPFORMAT(in, pt) macro
    if (!freadpattern(in, pt1) )
        userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read pattern '%s'", pt1);

    // TODO: FUGETVALUE() // int, char *, double are supported via _generic()
    if (fscanf(in, "%zu", &len) < 1)
        userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read fs length");

    if (!freadpattern(in, pt2) )
        userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read pattern '%s'", pt2);

    fs          res = FS();
    // TODO: think is create a someting like: fs init_or_use(fs *origin)
    if (s)
        fs_resize(s, len + 1);
    else {
        res = fsinit(len + 1);
        s = &res;
    }
    // just read len bytes from current position
    if (fread(s->v, 1, len, in) < len)
        userraiseint(ERR_NOT_ENOGH_VALUES, "Unable to read %zu bytes from stream", len);

    fsend(*s, len);  // fix the fs
    logsimple("%zu[%s]", len, s->v);
    if (!freadpattern(in, pt3) )
        userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read pattern '%s'", pt3);

    return *s;
}

fs                                      fs_load(const char *restrict fname, fs *restrict s){
    FILE *in = fopen(fname, "r");
    if (!in)
        userraiseint(ERR_UNABLE_OPEN_FILE_READ, "Unable to open %s for read", fname);
    fs str = fs_fload(in, s);
    fclose(in);
    return str;
}

// ------------------ API Constructs/Destrucor  ----------------------------
// local creation
fs                                      fsinit(size_t n){
    fs      res = FS();     // fsalloc flag
    increasesize(&res, n, true);
    fs_setlen(&res, 0);
    return logsimpleret(res, "Created empty with sz %zu", res.sz);
}
// just create fs in heap!
fs                                     *fs_create(void){
    fs  *new_fs = malloc(sizeof(fs) );
    if (!new_fs)
        userraiseint(ERR_UNABLE_ALLOCATE, "Unable to allocate fs body");
    else {
        *new_fs = FS();
        new_fs->flags |= FS_FLAG_BODYALLOC;   // чтобы fs_free удалил и тело, и будущую строку
        atomic_fetch_add(&g_alloc_body_cnt, 1); //logauto(++g_alloc_body_cnt);
    }
    return new_fs;
}
// full clone (str + body in heap)
fs                                     *fs_heapcreate(const fs *orig) {
    fs      *new_fs = fs_create();       // выделяет fs в куче, ставит FS_FLAG_BODYALLOC
    *new_fs = fs_clone(orig);       // копирует строку и поля
    new_fs->flags |= FS_FLAG_BODYALLOC; // гарантируем флаг перемещения
    return new_fs;
}
// clone as body local
fs                                      fs_clone(const fs *s){
    fs tmp = FS();
    if (s && s->v){
        tmp = fsinit(s->len + 1);
        strcopy(&tmp, s);
    }
    return tmp;
}
// destructor, macro wrapper will be
// free fs string and fs body if FS_FLAG_BODYALLOC
void                                    fs_free(fs *s){
    if (!s || fs_static(s) || fs_defunct(s))     // don't modify literals!
        return;
    bool  bdllloc = fs_bodyalloc(s);  // flags based
    if (fs_alloc(s) )    // actualy alloc must be a flag, but not statememnt TODO:
        if (s->v){
            atomic_fetch_add(&g_free_cnt, 1);
            logsimple("freed... %p", s->v);   // WOW, logsimpleact?
            free(s->v);
        }
    s->sz = s->len = 0;
    s->v = 0;
    if (bdllloc){
        s->flags &= ~FS_FLAG_BODYALLOC;   // clear  FS_FLAG_BODYALLOC
        atomic_fetch_add(&g_free_body_cnt, 1);
        logsimple("fs body %p freed (g_free_body_cnt %d)", s, 
            atomic_load_explicit(&g_free_body_cnt, memory_order_relaxed));
        free(s);
    }
}

// -------------------------------Testing --------------------------
#ifdef FSTESTING

#include "test.h"
#include "checker.h"

//types for testing


// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fsempty", ++subnum);
    {

        fs s = fsempty();
        if (s.len != 0 || s.sz != 0 || s.v != 0)
            return logerr(TEST_FAILED, "Empty fs validation failed");
        if (!fs_alloc(&s))
            return logerr(TEST_FAILED, "fs have no ALLOC");
        fsfree(s);  // should work normally
    }
    check_leak(true);
    test_sub("subtest %d: fsinit(100)", ++subnum);
    {
        fs s = fsinit(100);
        if (!fs_validate(logfile, &s) )
            return logacterr(fsfree(s), TEST_FAILED, "Validation's failed");
        if (!inv(s.len == 0 && s.sz >= 100 && s.v != 0 && fs_alloc(&s), "Failed") )
            return logacterr(fsfree(s), TEST_FAILED, "Condition violated, len [%zu], sz [%zu], v [%p], is alloc? (%s)",
                    s.len, s.sz, s.v, bool_str(fs_alloc(&s) ) );
        fsfree(s);
    }
    check_leak(true);
    test_sub("subtest %d: fsliteral", ++subnum);
    {
        const char *pattern = "1234567890";
        fs s = fsliteral(pattern);
        if (!fs_validate(logfile, &s) )
            return logerr(TEST_FAILED, "Validation's failed");
        if (!inv(s.len == strlen(pattern) && s.sz == strlen(pattern) + 1 && s.v != 0 && fs_static(&s), "Failed") )
            return logerr(TEST_FAILED, "Condition violated, len [%zu], sz [%zu], v [%p], is static? (%s)",
                    s.len, s.sz, s.v, bool_str(fs_static(&s) ) );
        fsfree(s);
    }
    check_leak(true);
    test_sub("subtest %d: fscopy", ++subnum);
    {

        const char *pattern = "1234567890";
        fs s = fscopy(pattern);
        if (!fs_validate(logfile, &s) )
            return logacterr(fsfree(s), TEST_FAILED, "Validation's failed");
        if (!inv(s.len == strlen(pattern) && s.sz >= strlen(pattern) + 1 && s.v != 0 && fs_alloc(&s), "Failed") )
            return logacterr(fsfree(s), TEST_FAILED, "Condition violated, len [%zu], sz [%zu], v [%p], is alloc? (%s)",
                s.len, s.sz, s.v, bool_str(fs_alloc(&s) ) );
        if (!inv(strcmp(fsstr(s), pattern) == 0, "Not equal") )
            return logacterr(fsfree(s), TEST_FAILED, "[%s] != pt [%s]", fsstr(s), pattern);

    test_sub("subtest %d: fsclone", ++subnum);

        fs s2 = fsclone(s);
        fsfree(s);

        if (!fs_validate(logfile, &s2) )
            return logacterr( fsfree(s2), TEST_FAILED, "Validation's failed");
        if (!inv(s2.len == strlen(pattern) && s2.sz >= strlen(pattern) + 1 && s2.v != 0 && fs_alloc(&s2), "Failed") )
            return logacterr( fsfree(s2), TEST_FAILED, "Condition violated, len [%zu], sz [%zu], v [%p], is alloc? (%s)",
                s2.len, s2.sz, s2.v, bool_str(fs_alloc(&s2) ) );
        if (!inv(strcmp(fsstr(s2), pattern) == 0, "Not equal") )
            return logacterr(fsfree(s2), TEST_FAILED, "[%s] != pt [%s]", fsstr(s2), pattern);

        fsfree(s2);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}


// ------------------------- TEST 2 ---------------------------------

static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fscopy", ++subnum);
    {
        const char *pt = "just pattern";
        fs s = fscopy(pt);

        char c = get(s, 0);
        if (!inv (c == 'j', "[0] returns [%c], but should be [%c]", c, 'j') )
            return logacterr( fsfree(s), TEST_FAILED, "[0] returns [%c], but should be [%c]", c, 'j');

        test_sub("subtest %d: get", ++subnum);
        get(s, 3) = 'y';
        if (!inv(strcmp(fsstr(s), "jusy pattern") == 0, "[%s] != pt [%s]", fsstr(s), "jusy pattern") )
            return logacterr(fsfree(s), TEST_FAILED, "[%s] != pt [%s]", fsstr(s), "jusy pattern");

        // check 3 for 'y'
        c = get(s, 3);
        if (!inv (c == 'y', "[0] returns [%c], but should be [%c]", c, 'y') )
            return logacterr( fsfree(s), TEST_FAILED, "[0] returns [%c], but should be [%c]", c, 'y');

        // reallocation test
    test_sub("subtest %d: elem with realloc", ++subnum);

        elem(s, 100) = 'z';
        if (!fs_validate(logfile, &s) )
            return logacterr( fsfree(s), TEST_FAILED, "Validation's failed");
        c = get(s, 100);
        if (!inv (c == 'z', "[100] returns [%c], but should be [%c]", c, 'z') )
            return logacterr( fsfree(s), TEST_FAILED, "[100] returns [%c], but should be [%c]", c, 'z');
        fstechfprint(logfile, s);

        test_sub("subtest %d: validate", ++subnum);
        fsshrink(s);
        if (!fs_validate(logfile, &s) )
            return logacterr( fsfree(s), TEST_FAILED, "Validation's failed");
        fstechfprint(logfile, s);

        fsfree(s);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 3 fs_elem / fs_elem0 ---------------------------------

static TestStatus
tf3(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: elem", ++subnum);
    {
        int     i;
        fs      s = fsempty();  // TODO: think if avoid
        char    pt[] = "test parretn 1234567";

        for (i = 0; i < (int) sizeof(pt); i++)
            elem(s, i) = pt[i];
        fsetlen(s, i);

        if (strcmp(s.v, pt) != 0)
            return logacterr( fsfree(s), TEST_FAILED, "Must be [%s] but not [%s]", pt, s.v);
        fsfree(s);
    }
    check_leak(true);

    /* ========== fs_elem ========== */
    test_sub("subtest %d: fs_elem within bounds (no grow)", ++subnum);
    {
        fs s = FS();                   // sz = 0, len = 0
        *fs_elem(&s, 3) = 'A';        // автоматически расширится
        test_validatefree(
            s.sz >= 4 && s.len == 0,  // len не должен измениться
            fsfree(s),
            "fs_elem within bounds: sz=%zu (expected >=4), len=%zu (expected 0)", s.sz, s.len
        );
        test_validatefree(
            *fs_elem(&s, 3) == 'A',
            fsfree(s),
            "Character at pos 3 must be 'A'"
        );
        fsfree(s);
    }
    check_leak(true);

    test_sub("subtest %d: fs_elem forces grow", ++subnum);
    {
        fs s = FS();
        // sz=0, обращение к pos=10 должно увеличить sz
        *fs_elem(&s, 10) = 'X';
        test_validatefree(
            s.sz >= 11 && s.len == 0,
            fsfree(s),
            "fs_elem force grow: sz=%zu (expected >=11), len=%zu (expected 0)", s.sz, s.len
        );
        test_validatefree(
            *fs_elem(&s, 10) == 'X',
            fsfree(s),
            "Character at pos 10 must be 'X'"
        );
        fsfree(s);
    }
    check_leak(true);

    /* ========== fs_elem0 ========== */
    test_sub("subtest %d: fs_elem0 updates len and ensures null terminator", ++subnum);
    {
        fs s = FS();
        *fs_elem0(&s, 0) = '0';
        *fs_elem0(&s, 1) = '1';
        *fs_elem0(&s, 2) = 'Y';       // pos=2, должно дать len=3 и '\0' в pos=3
        test_validatefree(
            s.sz >= 4 && s.len == 3,
            fsfree(s),
            "fs_elem0 pos=2: sz=%zu (expected >=4), len=%zu (expected 3)", s.sz, s.len
        );
        // проверяем, что строка корректна и заканчивается '\0'
        test_validatefree(
            s.v[0] == '0' && s.v[1] == '1' && s.v[2] == 'Y' && s.v[3] == '\0',
            fsfree(s),
            "fs_elem0 must leave '\\0' at positions < len and at len"
        );
        fsfree(s);
    }
    check_leak(true);

    test_sub("subtest %d: fs_elem0 grow and len update", ++subnum);
    {
        fs s = FS();
        *fs_elem0(&s, 5) = 'Z';       // большой скачок
        test_validatefree(
            s.sz >= 7 && s.len == 6,
            fsfree(s),
            "fs_elem0 pos=5: sz=%zu (expected >=7), len=%zu (expected 6)", s.sz, s.len
        );
        // строка должна читаться без вылета
        test_validatefree(
            s.v[5] == 'Z' && s.v[6] == '\0',
            fsfree(s),
            "fs_elem0 must correctly set char and null terminator"
        );
        fsfree(s);
    }
    check_leak(true);

    test_sub("subtest %d: fs_elem0 does not shrink len", ++subnum);
    {
        fs      s = FS();
        *fs_elem0(&s, 3) = 'A';
        size_t  old_len = s.len;
        *fs_elem0(&s, 1) = 'B';       // позиция меньше текущей len
        test_validatefree(
            s.len == old_len,         // len не должен уменьшиться
            fsfree(s),
            "fs_elem0 pos=1 after pos=3: len must stay %zu, got %zu", old_len, s.len
        );
        fsfree(s);
    }
    check_leak(true);

    /* ========== Edge cases ========== */
    test_sub("subtest %d: fs_elem0 on empty fs (pos=0)", ++subnum);
    {
        fs s = FS();
        *fs_elem0(&s, 0) = 'H';
        test_validatefree(
            s.sz >= 2 && s.len == 1 && s.v[0] == 'H' && s.v[1] == '\0',
            fsfree(s),
            "fs_elem0 pos=0 on empty fs: sz=%zu, len=%zu, str='%s'", s.sz, s.len, fs_str(&s)
        );
        fsfree(s);
    }
    check_leak(true);

    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- SIMPLE TEST fs_close ---------------------------------

static TestStatus
tf_fs_clone(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Клонирование обычной строки (fscopy) */
    test_sub("subtest %d: clone normal string", ++subnum);
    {
        const char *text = "hello, world";
        fs      orig = fscopy(text);
        fs      clone = fs_clone(&orig);

        // Оригинал не изменился
        test_validatefree(
            strcmp(fsstr(orig), text) == 0 && fslen(orig) == strlen(text),
            (fsfree(orig), fsfree(clone)),
            "Original must be unchanged after clone"
        );
        // Клон содержит ту же строку
        test_validatefree(
            strcmp(fsstr(clone), text) == 0 && fslen(clone) == strlen(text),
            (fsfree(orig), fsfree(clone)),
            "Clone must contain the same string, got '%s'", fsstr(clone)
        );
        // Клон физически независим: меняем оригинал – клон не меняется
        fs_sprintf(&orig, "modified");
        test_validatefree(
            strcmp(fsstr(clone), text) == 0,
            (fsfree(orig), fsfree(clone)),
            "Clone must remain unchanged after original modification"
        );

        fsfree(orig);
        fsfree(clone);
        fs_alloc_check(true);
    }

    /* 2. Клонирование пустой строки (v == NULL, созданной через FS()) */
    test_sub("subtest %d: clone empty string (v=NULL)", ++subnum);
    {
        fs      orig = FS();                    // v == NULL, len == 0
        fs      clone = fs_clone(&orig);

        test_validatefree(
            clone.v == NULL && fslen(clone) == 0,
            fsfree(clone),
            "Clone of empty (v=NULL) must have v=NULL and len=0, got v=%p, len=%zu",
            (void*)clone.v, fslen(clone)
        );
        test_validatefree(
            orig.v == NULL && fslen(orig) == 0,
            fsfree(clone),
            "Original must stay empty after clone"
        );

        fsfree(clone);
        fs_alloc_check(true);
    }

    /* 3. Клонирование пустой строки с v != NULL (fscopy("")) */
    test_sub("subtest %d: clone empty string (v!=NULL)", ++subnum);
    {
        fs      orig = fscopy("");              // v != NULL, len == 0
        fs      clone = fs_clone(&orig);

        test_validatefree(
            clone.v != NULL && fslen(clone) == 0,
            fsfree(clone),
            "Clone of empty (v!=NULL) must have v!=NULL and len=0, got v=%p, len=%zu",
            (void*)clone.v, fslen(clone)
        );
        test_validatefree(
            strcmp(fsstr(clone), "") == 0,
            fsfree(clone),
            "Clone must be empty string, got '%s'", fsstr(clone)
        );

        fsfree(orig);
        fsfree(clone);
        fs_alloc_check(true);
    }

    /* 4. Клонирование литерала (FS_FLAG_STATIC) */
    test_sub("subtest %d: clone literal", ++subnum);
    {
        fs      orig = fsliteral("static string");
        fs      clone = fs_clone(&orig);

        test_validatefree(
            strcmp(fsstr(clone), fsstr(orig)) == 0 && fslen(clone) == fslen(orig),
            fsfree(clone),
            "Clone of literal must have same content, got '%s'", fsstr(clone)
        );
        // Клон не должен быть статическим (должен быть копией в куче)
        // Проверим, что clone можно освободить (fsfree не упадёт) и что он не ссылается на ту же память
        test_validatefree(
            fsstr(clone) != fsstr(orig),        // указатели должны различаться
            fsfree(clone),
            "Clone of literal must have its own memory"
        );

        fsfree(clone);
        // orig – литерал, не освобождаем
        fs_alloc_check(true);
    }

    /* 5. Последовательные клонирования и проверка утечек */
    test_sub("subtest %d: multiple clones (leak check)", ++subnum);
    {
        const char *words[] = {"one", "two", "three"};
        fs      clones[COUNT(words)];

        for (int i = 0; i < COUNT(words); i++) {
            fs      orig = fscopy(words[i]);
            clones[i] = fs_clone(&orig);
            fsfree(orig);                       // оригинал больше не нужен
        }

        for (int i = 0; i < COUNT(words); i++) {
            test_validatefree(
                strcmp(fsstr(clones[i]), words[i]) == 0,
                (fsfree(clones[0]), fsfree(clones[1]), fsfree(clones[2])),
                "Clone %d must be '%s', got '%s'", i, words[i], fsstr(clones[i])
            );
            fsfree(clones[i]);
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 4 ---------------------------------

static TestStatus
tf4(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fscat()", ++subnum);
    {
        const char  arr1[] = "q123456";
        const char  arr2[] = "zxcvbnm901";
        fs          s1 = fscopy(arr1);
        fs          s2 = fsliteral(arr2);

        fscat(s1, s2);
        fstechfprint(logfile, s1);   // for manual

        if (s1.len != (int) strlen(arr1) + strlen(arr2) )
            return logacterr( fsfree(s1), TEST_FAILED, "len = %zu must be equal sum of l1 + l2 = %ld", fslen(s1), strlen(arr1) + strlen(arr2) );
        if (strstr(fsstr(s1), arr1) != fsstr(s1) )
            return logacterr( fsfree(s1), TEST_FAILED, "[%s] must start from [%s]", fsstr(s1), arr1);
        if (strstr(fsstr(s1), arr2) != fsstr(s1) + strlen(arr1) )
            return logacterr( fsfree(s1), TEST_FAILED, "[%s] must have [%s] on position %lu", fsstr(s1), arr2, strlen(arr1) );

    test_sub("subtest %d: fscatstr()", ++subnum);

        const char  arr3[] = "zaq1";
        size_t         len1 = fslen(s1);
        fscatstr(s1, arr3);
        fstechfprint(logfile, s1);   // for manual

        if (fslen(s1) != len1 + strlen(arr3) )
            return logacterr( fsfree(s1), TEST_FAILED, "len = %zu must be equal sum of l1 + l2 = %lu", fslen(s1), len1 + strlen(arr3) );
        if (strstr(fsstr(s1), arr3) != fsstr(s1) + len1 )
            return logacterr( fsfree(s1), TEST_FAILED, "[%s] must have [%s] on position %zu", fsstr(s1), arr3, len1 );
        if (strstr(fsstr(s1), arr1) != fsstr(s1) )  // the same test like in subtest 1
            return logacterr( fsfree(s1), TEST_FAILED, "[%s] must start from [%s]", fsstr(s1), arr1);

    test_sub("subtest %d: fscat() with empty initial test", ++subnum);
        {
            fs s3 = fscopy("");
            fstechfprint(logfile, s3);   // for manual
            fscat(s3, s2);
            fstechfprint(logfile, s3);   // for manual
            if (fslen(s3) != fslen(s2) )
                return logacterr( (fsfree(s1), fsfree(s3) ), TEST_FAILED, "len s3 = %zu must be equal s2 = %zu ", fslen(s3), fslen(s2) );
            if (fscmp(s3, s2) != 0)
                return logacterr( (fsfree(s1), fsfree(s3) ), TEST_FAILED, "%s not equal to %s", fsstr(s3), fsstr(s2) );
            fsfree(s3);
        }
        fsfree(s1);
    }
    check_leak(true);

    /* 1. Пустой target (FS()) + непустой source */
    test_sub("subtest %d: cat to empty target (FS())", ++subnum);
    {
        fs target = FS();
        fs source = fscopy("hello");

        fs_cat(&target, source);

        test_validatefree(fs_len(&target) == 5, (fsfree(target), fsfree(source)),
                          "len expected 5, got %zu", fs_len(&target));
        test_validatefree(strcmp(fs_str(&target), "hello") == 0,
                          (fsfree(target), fsfree(source)),
                          "content mismatch: '%s'", fs_str(&target));
        test_validatefree(fs_sz(&target) >= 6, (fsfree(target), fsfree(source)),
                          "sz expected >= 6, got %zu", fs_sz(&target));

        fsfree(target);
        fsfree(source);
        fs_alloc_check(true);
    }

    /* 2. Непустой target + пустой source (FS()) */
    test_sub("subtest %d: cat empty source (FS())", ++subnum);
    {
        fs target = fscopy("hello");
        fs source = FS();          // v == NULL

        fs_cat(&target, source);

        test_validatefree(fs_len(&target) == 5, fsfree(target),
                          "len expected 5, got %zu", fs_len(&target));
        test_validatefree(strcmp(fs_str(&target), "hello") == 0,
                          fsfree(target),
                          "content must stay 'hello', got '%s'", fs_str(&target));

        fsfree(target);
        fs_alloc_check(true);
    }

    /* 3. Оба пустые (FS() + FS()) */
    test_sub("subtest %d: cat empty to empty", ++subnum);
    {
        fs target = FS();
        fs source = FS();

        fs_cat(&target, source);

        test_validatefree(fs_len(&target) == 0, fsfree(target),
                          "len expected 0, got %zu", fs_len(&target));
        test_validatefree(fs_str(&target) == NULL || fs_str(&target)[0] == '\0',
                          fsfree(target),
                          "empty target must be empty");

        fsfree(target);
        fs_alloc_check(true);
    }

    /* 4. Непустой target + пустой source с выделенной памятью (fscopy("")) */
    test_sub("subtest %d: cat empty allocated source", ++subnum);
    {
        fs target = fscopy("hello");
        fs source = fscopy("");     // v != NULL, len = 0

        fs_cat(&target, source);

        test_validatefree(fs_len(&target) == 5, fsfree(target),
                          "len expected 5, got %zu", fs_len(&target));
        test_validatefree(strcmp(fs_str(&target), "hello") == 0,
                          fsfree(target),
                          "content must stay 'hello'");

        fsfree(target);
        fsfree(source);
        fs_alloc_check(true);
    }

    /* 5. Две непустые строки */
    test_sub("subtest %d: cat two non-empty strings", ++subnum);
    {
        fs target = fscopy("abc");
        fs source = fscopy("def");

        fs_cat(&target, source);

        test_validatefree(fs_len(&target) == 6, (fsfree(target), fsfree(source)),
                          "len expected 6, got %zu", fs_len(&target));
        test_validatefree(strcmp(fs_str(&target), "abcdef") == 0,
                          (fsfree(target), fsfree(source)),
                          "content mismatch: '%s'", fs_str(&target));

        fsfree(target);
        fsfree(source);
        fs_alloc_check(true);
    }

    /* 6. Расширение буфера при cat */
    test_sub("subtest %d: cat with buffer expansion", ++subnum);
    {
        fs target = fsinit(4);      // после округления sz может быть 8
        fs source = fscopy("long string");

        fs_cat(&target, source);

        test_validatefree(fs_len(&target) == strlen("long string"),
                          (fsfree(target), fsfree(source)),
                          "len mismatch: got %zu", fs_len(&target));
        test_validatefree(strcmp(fs_str(&target), "long string") == 0,
                          (fsfree(target), fsfree(source)),
                          "content mismatch: '%s'", fs_str(&target));
        test_validatefree(fs_sz(&target) >= fs_len(&target) + 1,
                          (fsfree(target), fsfree(source)),
                          "sz too small");

        fsfree(target);
        fsfree(source);
        fs_alloc_check(true);
    }

    /* 7. fs_catstr на пустой target */
    test_sub("subtest %d: catstr to empty target", ++subnum);
    {
        fs target = FS();

        fs_catstr(&target, "hello");

        test_validatefree(fs_len(&target) == 5, fsfree(target),
                          "len expected 5, got %zu", fs_len(&target));
        test_validatefree(strcmp(fs_str(&target), "hello") == 0,
                          fsfree(target),
                          "content mismatch: '%s'", fs_str(&target));

        fsfree(target);
        fs_alloc_check(true);
    }

    /* 8. fs_catmem с бинарными данными (включая \0) */
    test_sub("subtest %d: catmem with embedded null", ++subnum);
    {
        fs target = fscopy("abc");
        const char data[] = {'d', '\0', 'e'};

        fs_catmem(&target, data, sizeof(data) - 1);   // добавляем 'd' и '\0'

        test_validatefree(fs_len(&target) == 5, fsfree(target),
                          "len expected 5, got %zu", fs_len(&target));
        test_validatefree(memcmp(fs_str(&target), "abc", 3) == 0,
                          fsfree(target),
                          "prefix mismatch");
        test_validatefree(fs_str(&target)[3] == 'd' &&
                          fs_str(&target)[4] == '\0',
                          fsfree(target),
                          "embedded null mismatch");

        fsfree(target);
        fs_alloc_check(true);
    }

    /* 9. fs_catmem с size=0 */
    test_sub("subtest %d: catmem zero size", ++subnum);
    {
        fs target = fscopy("abc");

        fs_catmem(&target, "xyz", 0);

        test_validatefree(fs_len(&target) == 3, fsfree(target),
                          "len expected 3, got %zu", fs_len(&target));
        test_validatefree(strcmp(fs_str(&target), "abc") == 0,
                          fsfree(target),
                          "content must stay 'abc'");

        fsfree(target);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 5 ---------------------------------

static TestStatus
tf5(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fscpy()", ++subnum);
    {

        const char  arr1[] = "q123456";
        const char  arr2[] = "zxcvbnm901";
        fs          s1 = fscopy(arr1);
        fs          s2 = fsliteral(arr2);

        fscpy(s1, s2);
        fstechfprint(logfile, s1);
        if (fslen(s1) != fslen(s2) )
            return logacterr( fsfree(s1), TEST_FAILED, "len s2 = %zu must be equal s1 = %zu ", fslen(s2), fslen(s1) );
        if (fscmp(s1, s2) != 0)
            return logacterr( fsfree(s1), TEST_FAILED, "%s not equal to %s", fsstr(s1), fsstr(s2) );

        test_sub("subtest %d: fscpy()", ++subnum);

        fscpystr(s1, arr1);
        if (fslen(s1) != strlen(arr1) )
            return logacterr( fsfree(s1), TEST_FAILED, "len s2 = %lu must be equal s1 = %zu ",strlen(arr1), fslen(s1) );
        if (strcmp(fsstr(s1), arr1) != 0)
            return logacterr( fsfree(s1), TEST_FAILED, "%s not equal to %s", fsstr(s1), arr1);

        fsfree(s1);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 6 ---------------------------------

static TestStatus
tf6(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fsfreeall", ++subnum);
    {

        fs i1 = fsinit(100);
        fs i2 = fsinit(1000);
        fs i3 = fsinit(10);
        fsfreeall(&i1, &i2, &i3);
        if (i1.v || i2.v || i3.v)
            return logacterr( (fsfree(i1), fsfree(i2), fsfree(i3) ), TEST_FAILED, "Still not null");
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 7 ---------------------------------

static TestStatus
tf7(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fsprint MANUAL", ++subnum);
    {

        char    arr[] = "Hello, World from fs!\n";

        fs s = fscopy(arr);
        fsprint(s);

        int     lim = 3;
        printf("Limit %d\n", lim);
        fsprintlim(s, lim);

        fsfree(s);
    }
    check_leak(true);
    return logret(TEST_MANUAL, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 8 ---------------------------------

static TestStatus
tf8(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fsprint_arr MANUAL", ++subnum);
    {

        fs s1 = fsliteral("123");
        fs s2 = fsliteral("345");
        fs s3 = fsliteral("67890");

        fsfprint_arr(logfile, &s1, &s2, &s3);

        fsfreeall(&s1, &s2, &s3);
    }
    check_leak(true);
    return logret(TEST_MANUAL, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 9 ---------------------------------
static TestStatus
tf9(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    char        buf[1000];
    const char  fmt[] = "%d, %f, %s\n";

    test_sub("subtest %d: fs_sprintf EMPTY", ++subnum);
    {
        fs s1 = FS();

        snprintf(buf, sizeof(buf), fmt, 1223, 1.445, "Blablabla");
        fs_sprintf(&s1, fmt, 1223, 1.445, "Blablabla");

        // Проверяем длину
        test_validatefree(
            strlen(buf) == fslen(s1),
            fsfree(s1),
            "Length mismatch: buf=%zu, s1=%zu", strlen(buf), fslen(s1)
        );
        // Проверяем содержимое
        test_validatefree(
            strcmp(buf, fsstr(s1)) == 0,
            fsfree(s1),
            "Content mismatch: buf='%s', s1='%s'", buf, fsstr(s1)
        );

        fsfree(s1);
    }
    /* ---- 1. fs_sprintf в большой буфер ---- */
    test_sub("subtest %d: fs_sprintf (large buffer)", ++subnum);
    {
        fs s1 = fsinit(1000);

        snprintf(buf, sizeof(buf), fmt, 1223, 1.445, "Blablabla");
        fs_sprintf(&s1, fmt, 1223, 1.445, "Blablabla");

        // Проверяем длину
        test_validatefree(
            strlen(buf) == fslen(s1),
            fsfree(s1),
            "Length mismatch: buf=%zu, s1=%zu", strlen(buf), fslen(s1)
        );
        // Проверяем содержимое
        test_validatefree(
            strcmp(buf, fsstr(s1)) == 0,
            fsfree(s1),
            "Content mismatch: buf='%s', s1='%s'", buf, fsstr(s1)
        );
        fsfree(s1);
    }

    /* ---- 2. fs_sprintf в крошечный буфер (расширение) ---- */
    test_sub("subtest %d: fs_sprintf (small buffer, expands)", ++subnum);
    {
        fs s2 = fsinit(1);   // начальный размер 1

        snprintf(buf, sizeof(buf), fmt, 12345, 9.8765, "XXXYYYYZZZZZZZZZRRRRR");
        fs_sprintf(&s2, fmt, 12345, 9.8765, "XXXYYYYZZZZZZZZZRRRRR");

        test_validatefree(
            strlen(buf) == fslen(s2),
            fsfree(s2),
            "Length mismatch after expand: buf=%zu, s2=%zu", strlen(buf), fslen(s2)
        );
        test_validatefree(
            strcmp(buf, fsstr(s2)) == 0,
            fsfree(s2),
            "Content mismatch after expand: buf='%s', s2='%s'", buf, fsstr(s2)
        );
        fsfree(s2);
    }

    /* ---- 3. fs_sprintf_concat (дописывание) ---- */
    test_sub("subtest %d: fs_sprintf_concat", ++subnum);
    {
        fs s = fsinit(10);

        // Начальная строка
        fs_sprintf(&s, "Hello");
        // Конкатенация
        fs_sprintf_concat(&s, ", %s!", "World");

        const char expected[] = "Hello, World!";
        test_validatefree(
            strlen(expected) == fslen(s),
            fsfree(s),
            "Concat length mismatch: expected=%zu, got=%zu", strlen(expected), fslen(s)
        );
        test_validatefree(
            strcmp(expected, fsstr(s)) == 0,
            fsfree(s),
            "Concat content mismatch: expected='%s', got='%s'", expected, fsstr(s)
        );
        fsfree(s);
    }

    /* ---- 4. Пустой fs_sprintf ---- */
    test_sub("subtest %d: fs_sprintf empty format", ++subnum);
    {
        fs s = fsinit(16);
        int cntres = fs_sprintf(&s, "");
        test_validatefree(
            cntres == 0,
            fsfree(s),
            "Empty format should return cntres == 0, got %zu", fslen(s)
        );
        test_validatefree(
            fslen(s) == 0,
            fsfree(s),
            "Empty format should produce length 0, got %zu", fslen(s)
        );
        test_validatefree(
            strcmp("", fsstr(s)) == 0,
            fsfree(s),
            "Empty format should be empty string, got '%s'", fsstr(s)
        );
        fsfree(s);
    }

    check_leak(true);
    return logret(TEST_PASSED, "done");
}
/*
static TestStatus
tf9(const char *name)
{
    logenter("%s", name);

    int         subnum = 0;
    char buf[1000], fmt[] = "%d, %f, %s\n";

    test_sub("subtest %d: fs_printf with more than enough (1000 char)", ++subnum);
    {
        {
            fs s1 = fsinit(1000);

            snprintf(buf, sizeof(buf) - 1, fmt, 1223, 1.445, "Blablabla");
            fs_sprintf(&s1, fmt, 1223, 1.445, "Blablabla");
            fstechfprint(logfile, s1);  // for manual checking

            if ( strlen(buf) != s1.len)
                return logacterr( fsfree(s1), TEST_FAILED, "len buf = %lu must be equal s1 = %d ", strlen(buf), fslen(s1) );
            // compary strings TODO:
            if (strcmp(buf, fsstr(s1) ) != 0)
                return logacterr( fsfree(s1), TEST_FAILED, "buf [%s] must be equal s1 [%s]", buf, fsstr(s1) );
            fsfree(s1);
        }
    test_sub("subtest %d: fs_printf with small init size (2 char)", ++subnum);
        {
            fs s2 = fsinit(1);
            // the same test, but for very small fs
            snprintf(buf, sizeof(buf) - 1, fmt, 12345, 9.8765, "XXXYYYYZZZZZZZZZRRRRR");
            fs_sprintf(&s2, fmt, 12345, 9.8765, "XXXYYYYZZZZZZZZZRRRRR");
            fstechfprint(logfile, s2);  // for manual checking

            if ( strlen(buf) != s2.len)
                return logacterr( fsfree(s2), TEST_FAILED, "len buf = %lu must be equal s1 = %d ", strlen(buf), fslen(s2) );
            // compary strings TODO:
            if (strcmp(buf, fsstr(s2) ) != 0)
                return logacterr( fsfree(s2), TEST_FAILED, "buf [%s] must be equal s1 [%s]", buf, fsstr(s2) );
            fsfree(s2);
        }
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}  */

// ------------------------- TEST 10 ---------------------------------

static TestStatus
tf10(const char *name)
{
    logenter("%s", name);

    int         subnum = 0;

    test_sub("subtest %d: fslocal", ++subnum);
    {
        fslocal(s1, 15);
        elem(s1, 0) = 't';
        get(s1, 1) = 'z';
        fsend(s1, 2);
        char c = get(s1, 0), c1 = get(s1, 1);
        if (c != 't' || c1 != 'z')
            return logerr(TEST_FAILED, "s1[0] must be = 't' but not '%c', OR s1[1] must be 'z' but not '%c'", c, c1);

    test_sub("subtest %d: test with pattern", ++subnum);

        const char *pt = "qwe1234567890";

        strcpy(fsstr(s1), pt);
        // .len is still 2
        if (strcmp(fsstr(s1), pt) != 0)
            return logerr(TEST_FAILED, "fs [%s] != [%s]", fsstr(s1), pt);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 11 ---------------------------------

static TestStatus
tf11(const char *name)
{
    logenter("%s", name);

    int         subnum = 0;

    test_sub("subtest %d: fssave/load 1 fs", ++subnum);
    {
        const char fname[] = "res/fs/test_save_load.dat";
        fs s = fscopy("Tra la la 1234567890");
        if (fssave(fname, s) != 1)
            return logacterr(fsfree(s), TEST_FAILED, "Fssave returns != 1 value");
        fs s2 = fs_load(fname, 0); // w/o macro
        fstechfprint(logfile, s2);
        if (s.len != s2.len || fscmp(s, s2) != 0)
            return logacterr(fsfreeall(&s, &s2), TEST_FAILED, "Strings not equal (%zu - %zu)[%s] - [%s]", s.len, s2.len, fsstr(s), fsstr(s2) );

    test_sub("subtest %d: fs_save multiples fs", ++subnum);

        const char   fname2[] = "res/fs/test_save_load_mass.dat";
        FILE        *f = fopen(fname2, "w+");
        if (!f)
            return logacterr(fsfreeall(&s, &s2), TEST_FAILED, "Unable to open %s for w+", fname2);
        for (int i = 0; i < 15; i++){
            fssprintf(s, "Check val %d", i);
            if (fsfsave(f, s) != 1)
                return logacterr(fsfreeall(&s, &s2), TEST_FAILED, "Fssave returns != 1 value");
        }
        // try to load
    test_sub("subtest %d: fs_load multiples fs", ++subnum);

        rewind(f);  // from the start
        char        buf[100];
        for (int i = 0; i < 15; i++){
            s2 = fsfload(f, s2);
            snprintf(buf, sizeof(buf) - 1, "Check val %d", i);    // make a pattern
            if (strcmp(buf, fsstr(s2) ) != 0)
                return logacterr( (fclose(f), fsfreeall(&s, &s2) ), TEST_FAILED, "Strings not equal, loaded %s != pattern %s", fsstr(s2), buf);
        }
        fclose(f);
        fsfreeall(&s, &s2);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 12 ---------------------------------

static TestStatus
tf12(const char *name)
{
    logenter("%s", name);

    int         subnum = 0;
    int         freed, allocated;

    test_sub("subtest %d: simple init", ++subnum);
    {
        fs s1 = fsinit(100);
        fsfree(s1);

        fs_free_alloc_checker(&freed, &allocated);
        if (freed !=  allocated)
            return logerr(TEST_FAILED, "must be equal f/a counter  0:0, but not %d:%d", freed, allocated);
    }
    test_sub("subtest %d: via elem()", ++subnum);
    {
        fs s1 = FS();   // empty, alloc
        for (int i = 0; i < 1000000; i++)
            elem(s1, i) = 'a';
        fsfree(s1);
        fs_free_alloc_checker(&freed, &allocated);
        if (freed !=  allocated)
            return logerr(TEST_FAILED, "must be equal f/a counter  0:0, but not %d:%d", freed, allocated);
    }
    test_sub("subtest %d: via clone()", ++subnum);
    {
        fs s1 = fsliteral("12345");
        fs s2 = fsclone(s1);
        elem(s2, 10000) = 'c';
        fsfree(s2);
        fs_free_alloc_checker(&freed, &allocated);
        if (freed !=  allocated)
            return logerr(TEST_FAILED, "must be equal f/a counter  0:0, but not %d:%d", freed, allocated);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 13 ---------------------------------

static TestStatus
tf13(const char *name)
{
    logenter("%s", name);

    int         subnum = 0;

    test_sub("subtest %d: simple fs_move", ++subnum);
    {
        const char pt[] = "qwertyuiop1234567890";
        fs s1 = fscopy(pt);
        fs s2 = fsmove(s1);
        if (!fsisnull(s1) )
            return logacterr( fsfree(s2), TEST_FAILED, "s1 must becaume nullable after moving, but not (%zu:%zu:%p)", s1.len, s1.sz, s1.v);
        if (s2.len != (int) strlen(pt) )
            return logacterr( fsfree(s2), TEST_FAILED, "Len of origin pattern %lu must be equal of len move fs %zu", strlen(pt), s2.len);
        if (strcmp(fsstr(s2), pt) != 0)
            return logacterr( fsfree(s2), TEST_FAILED, "origin [%s] must be equal move fs [%s]", pt, fsstr(s2) );
        fsfree(s2);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 14 ---------------------------------
/*
static TestStatus
tf14(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fssubstr", ++subnum);
    {
        const char  pt[] = "qwertyuiop1234567890";
        int         len = 3;
        fs          s1 = fscopy(pt);

        fssubstr(s1, 10, len);
        test_validatefree(fslen(s1) == len, fsfree(s1),
                "Len of substr %d must be equal to %d", fslen(s1), len);
        test_validatefree(strcmp(fsstr(s1), "123") == 0, fsfree(s1),
                "substr [%s] must be equal of %s", fsstr(s1), "123");

        fsfree(s1);
    }
    test_sub("subtest %d: fssubstr", ++subnum);
    {
        const char  pt[] = "qwertyuiop1234567890";
        int         len = 5;
        fs          s1 = fscopy(pt);
        fs          s2 = fsnewsubstr(s1, 10, len);

        test_validatefree(fslen(s2) == len, fsfreeall(&s1, &s2),
                "Len of substr %d must be equal to %d", fslen(s2), len);
        test_validatefree(strcmp(fsstr(s2), "12345") == 0, fsfreeall(&s1, &s2),
                "substr [%s] must be equal of %s", fsstr(s2), "12345");

    test_sub("subtest %d: right part of a string", ++subnum);

        int         offset = 1;
        fs          s3 = fsnewsubstr(s1, offset, 100);
        test_validatefree(fslen(s3) == strlen(pt) - offset, fsfreeall(&s1, &s2, &s3),
                "Len of substr %d must be equal to %lu - %d", fslen(s3), strlen(pt), offset);
        test_validatefree(strcmp(fsstr(s3), pt + offset) == 0, fsfreeall(&s1, &s2, &s3),
                "substr [%s] must be equal of %s", fsstr(s3), pt + offset);

        fsfreeall(&s1, &s2, &s3);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
} */
static TestStatus
tf14(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    /* ================= fs_substr (in-place) ================= */

    /* 1. Обрезание с начала (from = 0) до меньшей длины */
    test_sub("subtest %d: fs_substr cut from start", ++subnum);
    {
        const char *orig = "hello world";
        size_t      new_len = 5;
        fs          s = fscopy(orig);
        fs          result = fs_substr(&s, 0, new_len);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == new_len,
            fsfree(s),
            "fs_substr cut: length must be %zu, got %zu", new_len, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "hello") == 0,
            fsfree(s),
            "fs_substr cut: expected 'hello', got '%s'", fsstr(s)
        );
        fsfree(s);
    }

    /* 2. Взятие подстроки с середины */
    test_sub("subtest %d: fs_substr mid part", ++subnum);
    {
        const char *orig = "abcdefghij";
        size_t      from = 3, to = 4;
        fs          s = fscopy(orig);
        fs          result = fs_substr(&s, from, to);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == to,
            fsfree(s),
            "fs_substr mid: length must be %zu, got %zu", to, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "defg") == 0,
            fsfree(s),
            "fs_substr mid: expected 'defg', got '%s'", fsstr(s)
        );
        fsfree(s);
    }

    /* 3. from выходит за пределы строки -> пустая строка */
    test_sub("subtest %d: fs_substr from beyond length", ++subnum);
    {
        const char *orig = "abc";
        int         from = 10, to = 2;
        fs          s = fscopy(orig);
        fs          result = fs_substr(&s, from, to);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == 0,
            fsfree(s),
            "fs_substr beyond: length must be 0, got %zu", fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "") == 0,
            fsfree(s),
            "fs_substr beyond: expected empty, got '%s'", fsstr(s)
        );
        fsfree(s);
    }

    /* 4. Пустая исходная строка */
    test_sub("subtest %d: fs_substr on empty string", ++subnum);
    {
        fs          s = FS();               /* пустая строка */
        fs          result = fs_substr(&s, 0, 5);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == 0,
            fsfree(s),
            "fs_substr empty: length must be 0, got %zu", fslen(s)
        );
        fsfree(s);
    }

    test_sub("subtest %d: fs_substr len beyond length", ++subnum);
    {
        const char *orig = "abcdefgh";
        int         from = 5, to = 200;
        fs          s = fscopy(orig);
        fs          result = fs_substr(&s, from, to);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == strlen(orig) - from,
            fsfree(s),
            "fs_substr beyond: length must be %zu, got %zu", strlen(orig) - from, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "fgh") == 0,
            fsfree(s),
            "fs_substr beyond: expected '%s', got '%s'", "fgh", fsstr(s)
        );
        fsfree(s);
    }

    /* ================= fs_newsubstr (constructor) ================= */

    /* 5. Копирование подстроки с середины (оригинал не меняется) */
    test_sub("subtest %d: fs_newsubstr copy mid part", ++subnum);
    {
        const char *orig_str = "abcdefghij";
        size_t      from = 2, to = 5;
        fs          orig = fscopy(orig_str);
        fs          sub = fs_newsubstr(&orig, from, to);

        // Проверяем подстроку
        test_validatefree(
            fslen(sub) == to,
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr copy: length must be %zu, got %zu", to, fslen(sub)
        );
        test_validatefree(
            strcmp(fsstr(sub), "cdefg") == 0,
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr copy: expected 'cdefg', got '%s'", fsstr(sub)
        );
        // Проверяем, что оригинал не изменился
        test_validatefree(
            strcmp(fsstr(orig), orig_str) == 0 && fslen(orig) == strlen(orig_str),
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr copy: original must stay unchanged, got '%s' (len %zu)",
            fsstr(orig), fslen(orig)
        );
        fsfree(orig);
        fsfree(sub);
    }

    /* 6. from за пределами -> пустая строка, оригинал цел */
    test_sub("subtest %d: fs_newsubstr from beyond length", ++subnum);
    {
        const char *orig_str = "abc";
        int         from = 10, to = 2;
        fs          orig = fscopy(orig_str);
        fs          sub = fs_newsubstr(&orig, from, to);

        test_validatefree(
            fslen(sub) == 0,
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr beyond: result length must be 0, got %zu", fslen(sub)
        );
        test_validatefree(
            strcmp(fsstr(sub), "") == 0,
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr beyond: result must be empty, got '%s'", fsstr(sub)
        );
        test_validatefree(
            strcmp(fsstr(orig), orig_str) == 0,
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr beyond: original must stay '%s', got '%s'", orig_str, fsstr(orig)
        );
        fsfree(orig);
        fsfree(sub);
    }

    /* 7. to больше оставшейся длины -> обрезается до конца строки */
    test_sub("subtest %d: fs_newsubstr to exceeds length", ++subnum);
    {
        const char *orig_str = "hello";
        int         from = 2, to = 100;
        fs          orig = fscopy(orig_str);
        fs          sub = fs_newsubstr(&orig, from, to);

        size_t      expected_len = strlen(orig_str) - from;
        test_validatefree(
            fslen(sub) == expected_len,
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr exceed: length must be %zu, got %zu", expected_len, fslen(sub)
        );
        test_validatefree(
            strcmp(fsstr(sub), orig_str + from) == 0,
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr exceed: expected '%s', got '%s'", orig_str + from, fsstr(sub)
        );
        fsfree(orig);
        fsfree(sub);
    }

    /* 8. Пустая исходная строка */
    test_sub("subtest %d: fs_newsubstr on empty string", ++subnum);
    {
        fs          orig = FS();
        fs          sub = fs_newsubstr(&orig, 0, 5);

        test_validatefree(
            fslen(sub) == 0,
            (fsfree(orig), fsfree(sub)),
            "fs_newsubstr empty: result length must be 0, got %zu", fslen(sub)
        );
        fsfree(orig);
        fsfree(sub);
    }

    check_leak(true);
    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 15 ---------------------------------

static TestStatus
tf15(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fs_ifnotin", ++subnum);   // sensitive, negative
    {
        const char  pt[] = "qwe1";
        fs          s = fscopy(pt);

        if (!fs_ifnotin(s, "a", "in", "z", "bbbbm", "qqqqq") )
            return logacterr( fsfree(s), TEST_FAILED, "NOT IN return false, but it must be true");

        if (fs_ifnotin(s, "a", "in", "z", "bbbbm", "qqqqq", pt) )
            return logacterr( fsfree(s), TEST_FAILED, "NOT IN return true, but it must be false");

    test_sub("subtest %d: fs_ifinotin", ++subnum);  // insensetive, netagive

        if (!fs_ifinotin(s, "a", "in", "z", "bbbbm", "QQQQQ") )
            return logacterr( fsfree(s), TEST_FAILED, "NOT IN return false, but it must be true");

        if (fs_ifinotin(s, "a", "in", "z", "VVBDFDFFDS", "qqqqq", "QWE1", "sds") )
            return logacterr( fsfree(s), TEST_FAILED, "NOT IN return true, but it must be false");

    test_sub("subtest %d: fs_ifin", ++subnum);  // sensetive, positive

        if (fs_ifin(s, "a", "in", "z", "bbbbm", "qqqqq") )
            return logacterr( fsfree(s), TEST_FAILED, "IN return true, but it must be false");

        if (!fs_ifin(s, "a", "in", "z", "bbbbm", "qqqqq", pt) )
            return logacterr( fsfree(s), TEST_FAILED, "IN return false, but it must be true");

    test_sub("subtest %d: fs_ifiin", ++subnum);  // insensetive, positive

        if (fs_ifiin(s, "a", "in", "z", "bbbbm", "qqqqq") )
            return logacterr( fsfree(s), TEST_FAILED, "IN return true, but it must be false");

        if (!fs_ifiin(s, "a", "in", "z", "bbbbm", "qqqqq", "QWE1", "sds") )
            return logacterr( fsfree(s), TEST_FAILED, "IN return false, but it must be true");

        fsfree(s);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}


// ------------------------- TEST 16 ---------------------------------

static TestStatus
tf16(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fs_instr", ++subnum);   // sensitive, negative
    {
        const char  pt[] = "qwertyuiop1234567890";
        fs          orig = fsliteral(pt);   // statis, not alloc
        long        pos = 8, res;
        fs          s = fsnewsubstr(orig, pos, 6);

        test_validatefree( (res = fs_instr(&orig, &s) ) == pos,
                          fsfree(s), "Must be %zu but returns %zu", pos, res);

        fscatstr(s, "Not exists in original pattern"); 
        test_validatefree( (res = fs_instr(&orig, &s) ) == -1,
                          fsfree(s), "Must be %d but returns %zu", -1, res);

        fsfree(s);
    }
    test_sub("subtest %d: fs_iinstr", ++subnum);   // sensitive, negative
    {
        const char  pt[] = "qweRTYUIOP1234567890III";
        fs          orig = fsliteral(pt);
        int         pos = 8, res;
        fs          s = fsnewsubstr(orig, pos, 6);
        fs_tolower(&s);

        test_validatefree( (res = fs_iinstr(&orig, &s) ) == pos,
                          fsfree(s), "Must be %d but returns %d", pos, res);

        fscatstr(s, "Not exists in original pattern");
        test_validatefree( (res = fs_iinstr(&orig, &s) ) == -1,
                          fsfree(s), "Must be %d but returns %d", -1, res);


        fsfree(s);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 17 ---------------------------------

static TestStatus
tf17(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    fs          s = FS();

    test_sub("subtest %d: fs_chr tests (unlim)", ++subnum);   // sensitive, negative
    {
        const char  pt[] = "qwertyuiop1234567890";
        fs          lit = fsliteral(pt);
        int         pos = 7, res;
        s = fs_newsubstr(&lit, pos, 1);
        char        c = *fsstr(s);

        test_validatefree( (res = fs_chr(&lit, c) ) == pos, fsfree(s),
                        "Position must be %d but returns %d", pos, res);
        c = toupper(c);
        test_validatefree( (res = fs_chr(&lit, c) ) == -1, fsfree(s),
                        "Position must be -1 (not found) but returns %d", res);
        fsfree(s);
    }

    test_sub("subtest %d: fs_nchr tests (lim)", ++subnum);   // sensitive, negative
    {
        const char  pt[] = "qwertyuiop1234567890xxxxx";
        fs          lit = fsliteral(pt);
        int         pos = 10, res, lim = 5;
        fs          s = fs_newsubstr(&lit, pos, 1);
        char        c = *fsstr(s);

        test_validatefree( (res = fs_nchr(&lit, c, 20) ) == pos, fsfree(s),
                        "Position must be %d but returns %d", pos, res);
        test_validatefree( (res = fs_nchr(&lit, c, lim) ) == -1, fsfree(s),
                        "Position must be -1 (not found) because of limitation %d but returns %d", lim, res);
        fsfree(s);
    }
    test_sub("subtest %d: fs_nchr iteration tests (lim)", ++subnum);   // sensitive, negative
    {
        const char  pt[] = "qwertyuiop1234567890xxxxx";
        fs          lit = fsliteral(pt);
        int         pos = 9, res, lim = 5;
        fs          s = fs_newsubstr(&lit, pos, 1);
        char        c = *fsstr(s);

        for (lim = 1; lim < (int) sizeof(pt); lim++){
            if (lim <= pos){
                test_validatefree( (res = fs_nchr(&lit, c, lim) ) == -1, fsfree(s),
                     "[%c] Position must be -1 (not found) because of limitation %d but returns %d\n%s\n%s\n", c, lim, res, pt, "0123456789ABCDE");
            } else {
                test_validatefree( (res = fs_nchr(&lit, c, 20) ) == pos, fsfree(s),
                    "[%c] Position must be %d but returns %d", c, pos, res);
            }
        }
        fsfree(s);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 18 ---------------------------------

static TestStatus
tf18(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fs_rchr tests (unlim)", ++subnum);   // sensitive
    {
        const char  pt[] = "qwertyuiop1234567890";
        fs          lit = fsliteral(pt);
        int         pos = 7, res;
        fs          s = fs_newsubstr(&lit, pos, 1);
        fstechprint(s);
        fstechprint(lit);
        char        c = *fsstr(s);

        test_validatefree( (res = fs_rchr(&lit, c) ) == pos, fsfree(s),
                        "Position must be %d but returns %d", pos, res);

        c = 'z';
        test_validatefree( (res = fs_rchr(&lit, c) ) == -1, fsfree(s),
                        "Position must be -1 but returns %d", res);
        fsfree(s);
    }
    test_sub("subtest %d: fs_irchr tests (unlim)", ++subnum);   // insensitive reverse
    {
        const char  pt[] = "qwertyuiop1234567890";
        fs          lit = fsliteral(pt);
        int         pos = 7, res;
        fs          s = fs_newsubstr(&lit, pos, 1);
        char        c = toupper(*fsstr(s) );

        test_validatefree( (res = fs_irchr(&lit, c) ) == pos, fsfree(s),
                        "Position must be %d but returns %d", pos, res);

        c = toupper('z');
        test_validatefree( (res = fs_irchr(&lit, c) ) == -1, fsfree(s),
                        "Position must be -1 but returns %d", res);
        fsfree(s);

    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 19 ---------------------------------

static TestStatus
tf19(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fs_ninstr test (lim)", ++subnum);   // sensitive
    {
        const char  pt[] = "qwertyuiop1234567890xxxxxxxxxxxxxxx";
        fs          lit = fsliteral(pt);
        int         pos = 7, cnt = 11, res;
        fs          s = fs_newsubstr(&lit, pos, cnt);

        test_validatefree( (res = fs_ninstr(&lit, &s, 20) ) == pos, fsfree(s),
                    "Position must be %d but returns %d", pos, res);
        test_validatefree( (res = fs_ninstr(&lit, &s, 15) ) == -1, fsfree(s),
                    "Position must be %d (not found) but returns %d" , -1, res);

        fsfree(s);
    }
    test_sub("subtest %d: fs_ninstr iteration test (lim)", ++subnum);   // sensitive
    {
        const char  pt[] = "qwertyuiop1234567890xxxxxxxxxxxxxxx";
        fs          lit = fsliteral(pt);
        int         pos = 5, cnt = 10, res;
        fs          s = fs_newsubstr(&lit, pos, cnt);
        logauto(lit.v);

        for (size_t lim = 1; lim < fslen(lit); lim++)
            if ((int) lim >= pos + cnt){       // positive case
                test_validatefree( (res = fs_ninstr(&lit, &s, 20) ) == pos, fsfree(s),
                        "Position must be %d but returns %d", pos, res);
            } else {    // -1
                test_validatefree( (res = fs_ninstr(&lit, &s, 15) ) == -1, fsfree(s),
                        "Position must be %d (not found) but returns %d" , -1, res);
            }

        fsfree(s);
    }
    test_sub("subtest %d: fs_niinstr test (lim)", ++subnum);   // insensitive
    {
        const char  pt[] = "qwertyuiop1234567890xxxxxxxxxxxxxxx";
        fs          lit = fsliteral(pt);
        int         pos = 7, cnt = 11, res;
        fs          s = fs_newsubstr(&lit, pos, cnt);
        fs_toupper(&s);

        test_validatefree( (res = fs_niinstr(&lit, &s, 20) ) == pos, fsfree(s),
                    "Position must be %d but returns %d", pos, res);
        test_validatefree( (res = fs_niinstr(&lit, &s, 15) ) == -1, fsfree(s),
                    "Position must be %d (not found) but returns %d" , -1, res);

        fsfree(s);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 20 ---------------------------------

static TestStatus
tf20(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fsrevcatstr simple", ++subnum);
    {
        const char  pt[] = "qwertyuiop1234567890";
        const char  ptinit[] = "ABCDE";
        fs          s = fscopy(ptinit);

        fsrevcatstr(s, pt);

        test_validatefree(fslen(s) == strlen(pt) + strlen(ptinit), fsfree(s),
                        "Length of final fs (%zu) must be sum of length of original (%zu + %zu)", fslen(s), strlen(ptinit), strlen(pt) );

        test_validatefree(strncmp(pt, fsstr(s), strlen(pt) ) == 0,  fsfree(s),
                        "Pattern [%s] and first %zu symbols of fs [%.*s] must be equal", pt, strlen(pt), (int) strlen(pt), fsstr(s) );

        test_validatefree(strcmp(ptinit, fsstr(s) + strlen(pt) ) == 0,  fsfree(s),
                        "Init Pattern [%s] and last %zu symbols of fs [%s] must be equal", ptinit, strlen(ptinit), fsstr(s) + strlen(pt) );
        fsfree(s);
    }
    test_sub("subtest %d: fsrevcatstr iteration", ++subnum);
    {
        const char  pt[] = "qwertyuiop1234567890111";
        const char  ptinit[] = "ABCDEF";
        fs          s = fscopy(ptinit);
        int         cnt = 10;

        for (int i = 0; i < cnt; i++){
            fsrevcatstr(s, pt);
        }

        test_validatefree(fslen(s) == strlen(pt) * cnt + strlen(ptinit), fsfree(s),
                             "Length of final fs (%zu) must be sum of length of original (%zu + %zu * %d)", fslen(s), strlen(ptinit), strlen(pt), cnt );

        test_validatefree(strncmp(pt, fsstr(s), strlen(pt) ) == 0,  fsfree(s),
                        "Pattern [%s] and first %zu symbols of fs [%.*s] must be equal", pt, strlen(pt), (int) strlen(pt), fsstr(s) );

        test_validatefree(strcmp(ptinit, fsstr(s) + strlen(pt) * cnt ) == 0,  fsfree(s),
                        "Init Pattern [%s] and last %zu symbols of fs [%s] must be equal", ptinit, strlen(ptinit), fsstr(s) + strlen(pt) * cnt );

        fsfree(s);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 21 ---------------------------------

static TestStatus
tf21(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: just simple", ++subnum);
    {
        const char  pt[] = "qwertyuiop      IOPKKNKNMN_1234567890";

        fs s = fscopy(pt);
        fssort(s, false);

        for (size_t i = 1; i < s.len; i++)
            test_validatefree(
                s.v[i - 1] >= s.v[i], 
                fsfree(s),
                "s[%zu] == [%c] must be >= s[%zu] == [%c]", i - 1, s.v[i - 1], i, s.v[i]
            );

        fsfree(s);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 22 ---------------------------------

static TestStatus
tf22(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    fs s = FS();
    const char shift[] = "----------";

    test_sub("subtest %d: getint simple", ++subnum);
    {
        int     ival = 2347890;
        fssprintf(s, "%s%d", shift, ival);

        int     ires = fsgetintpos(s, sizeof(shift) - 1);

        test_validatefree(ires == ival, fsfree(s),
                "%d must be == %d", ires, ival);
    }
    test_sub("subtest %d: getlong simple", ++subnum);
    {
        long    lval = 987654321099887;
        fssprintf(s, "%s%ld", shift, lval);

        fstechfprint(logfile, s);
        long    lres = fsgetlongpos(s, sizeof(shift) - 1);

        test_validatefree(lres == lval, fsfree(s),
                "%ld must be == %ld", lres, lval);
    }
    test_sub("subtest %d: getdouble simple", ++subnum);
    {
        double  dval = 12.345;
        fssprintf(s, "%s%lf", shift, dval);

        double  dres = fsgetdoublepos(s, sizeof(shift) - 1);

        test_validatefree(dres == dval, fsfree(s),
                "%lf must be == %lf", dres, dval);
    }
    fsfree(s);
    check_leak(true);

    // additional
    /* ========== fs_getint ========== */

    /* 1. Обычное число */
    test_sub("subtest %d: fs_getint normal", ++subnum);
    {
        fs s = fscopy("42");
        test_validatefree(
            fs_getint(&s) == 42,
            fsfree(s),
            "fs_getint('42') must return 42"
        );
        fsfree(s);
    }

    /* 2. Отрицательное число */
    test_sub("subtest %d: fs_getint negative", ++subnum);
    {
        fs s = fscopy("-123");
        test_validatefree(
            fs_getint(&s) == -123,
            fsfree(s),
            "fs_getint('-123') must return -123"
        );
        fsfree(s);
    }

    /* 3. Нечисловая строка – должна упасть (перехватываем сигнал) */
    test_sub("subtest %d: fs_getint non‑numeric", ++subnum);
    {
        fs s = fscopy("abc");
        if (!try()) {
            int val = fs_getint(&s);   // должно вызвать userraiseint
            test_validatefree(
                false,
                fsfree(s),
                "fs_getint('abc') must raise error, but returned %d", val
            );
        } else {
            test_validatefree(
                true,
                fsfree(s),
                "fs_getint('abc') correctly raised error"
            );
        }
        fsfree(s);
    }

    /* 4. Пустая строка – должна упасть */
    test_sub("subtest %d: fs_getint empty", ++subnum);
    {
        fs s = FS();   // пустая строка, v == NULL
        if (!try()) {
            fs_getint(&s);
            test_validatefree(false, fsfree(s), "fs_getint(empty) must raise error");
        } else {
            test_validatefree(true, fsfree(s), "fs_getint(empty) correctly raised error");
        }
        fsfree(s);
    }

    /* 5. Работа с позицией: fs_getintpos */
    test_sub("subtest %d: fs_getintpos", ++subnum);
    {
        fs s = fscopy("abc 123");
        test_validatefree(
            fs_getintpos(&s, 4) == 123,
            fsfree(s),
            "fs_getintpos('abc 123', 4) must return 123"
        );
        fsfree(s);
    }

    /* ========== fs_getlong ========== */

    /* 6. Обычное значение */
    test_sub("subtest %d: fs_getlong normal", ++subnum);
    {
        fs s = fscopy("9999999999");
        test_validatefree(
            fs_getlong(&s) == 9999999999L,
            fsfree(s),
            "fs_getlong('9999999999') must return 9999999999"
        );
        fsfree(s);
    }

    /* 7. Нечисловая строка */
    test_sub("subtest %d: fs_getlong non‑numeric", ++subnum);
    {
        fs s = fscopy("xyz");
        if (!try()) {
            fs_getlong(&s);
            test_validatefree(false, fsfree(s), "fs_getlong('xyz') must raise error");
        } else {
            test_validatefree(true, fsfree(s), "fs_getlong('xyz') correctly raised error");
        }
        fsfree(s);
    }

    /* 8. Пустая строка */
    test_sub("subtest %d: fs_getlong empty", ++subnum);
    {
        fs s = FS();
        if (!try()) {
            fs_getlong(&s);
            test_validatefree(false, fsfree(s), "fs_getlong(empty) must raise error");
        } else {
            test_validatefree(true, fsfree(s), "fs_getlong(empty) correctly raised error");
        }
        fsfree(s);
    }

    /* 9. Позиционное чтение */
    test_sub("subtest %d: fs_getlongpos", ++subnum);
    {
        fs s = fscopy("--- -777");
        test_validatefree(
            fs_getlongpos(&s, 4) == -777L,
            fsfree(s),
            "fs_getlongpos('--- -777', 4) must return -777"
        );
        fsfree(s);
    }

    /* ========== fs_getulong ========== */

    /* 10. Обычное значение */
    test_sub("subtest %d: fs_getulong normal", ++subnum);
    {
        fs s = fscopy("9999999999");
        test_validatefree(
            fs_getulong(&s) == 9999999999UL,
            fsfree(s),
            "fs_getulong('9999999999') must return 9999999999"
        );
        fsfree(s);
    }

    /* 11. Максимальное 64‑битное значение */
    test_sub("subtest %d: fs_getulong max", ++subnum);
    {
        fs s = fscopy("18446744073709551615");
        test_validatefree(
            fs_getulong(&s) == 18446744073709551615UL,
            fsfree(s),
            "fs_getulong('18446744073709551615') must return ULONG_MAX"
        );
        fsfree(s);
    }

    /* 12. Нечисловая строка */
    test_sub("subtest %d: fs_getulong non‑numeric", ++subnum);
    {
        fs s = fscopy("xyz");
        if (!try()) {
            fs_getulong(&s);
            test_validatefree(false, fsfree(s), "fs_getulong('xyz') must raise error");
        } else {
            test_validatefree(true, fsfree(s), "fs_getulong('xyz') correctly raised error");
        }
        fsfree(s);
    }

    /* 13. Пустая строка */
    test_sub("subtest %d: fs_getulong empty", ++subnum);
    {
        fs s = FS();
        if (!try()) {
            fs_getulong(&s);
            test_validatefree(false, fsfree(s), "fs_getulong(empty) must raise error");
        } else {
            test_validatefree(true, fsfree(s), "fs_getulong(empty) correctly raised error");
        }
        fsfree(s);
    }

    /* 14. Позиционное чтение */
    test_sub("subtest %d: fs_getulongpos", ++subnum);
    {
        fs s = fscopy("--- 777");
        test_validatefree(
            fs_getulongpos(&s, 4) == 777UL,
            fsfree(s),
            "fs_getulongpos('--- 777', 4) must return 777"
        );
        fsfree(s);
    }
    fs_alloc_check(true);

    /* ========== fs_getdouble ========== */

    /* 10. Обычное число */
    test_sub("subtest %d: fs_getdouble normal", ++subnum);
    {
        fs s = fscopy("3.1415");
        test_validatefree(
            fabs(fs_getdouble(&s) - 3.1415) < 0.0001,
            fsfree(s),
            "fs_getdouble('3.1415') must return ~3.1415"
        );
        fsfree(s);
    }

    /* 11. Специальные значения (inf, nan) */
    test_sub("subtest %d: fs_getdouble special", ++subnum);
    {
        fs s_inf = fscopy("inf");
        double val = fs_getdouble(&s_inf);
        test_validatefree(
            isinf(val) && val > 0,
            fsfree(s_inf),
            "fs_getdouble('inf') must be +inf"
        );
        fsfree(s_inf);

        fs s_nan = fscopy("nan");
        val = fs_getdouble(&s_nan);
        test_validatefree(
            isnan(val),
            fsfree(s_nan),
            "fs_getdouble('nan') must be NaN"
        );
        fsfree(s_nan);
    }

    /* 12. Нечисловая строка – должна упасть */
    test_sub("subtest %d: fs_getdouble non‑numeric", ++subnum);
    {
        fs s = fscopy("not_a_number");
        if (!try()) {
            fs_getdouble(&s);
            test_validatefree(false, fsfree(s), "fs_getdouble('not_a_number') must raise error");
        } else {
            test_validatefree(true, fsfree(s), "fs_getdouble('not_a_number') correctly raised error");
        }
        fsfree(s);
    }

    /* 13. Пустая строка – должна упасть */
    test_sub("subtest %d: fs_getdouble empty", ++subnum);
    {
        fs s = FS();
        if (!try()) {
            fs_getdouble(&s);
            test_validatefree(false, fsfree(s), "fs_getdouble(empty) must raise error");
        } else {
            test_validatefree(true, fsfree(s), "fs_getdouble(empty) correctly raised error");
        }
        fsfree(s);
    }

    /* 14. Позиционное чтение double */
    test_sub("subtest %d: fs_getdoublepos", ++subnum);
    {
        fs s = fscopy("val: 2.71828");
        test_validatefree(
            fabs(fs_getdoublepos(&s, 5) - 2.71828) < 0.00001,
            fsfree(s),
            "fs_getdoublepos('val: 2.71828', 5) must return ~2.71828"
        );
        fsfree(s);
    }

    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 23 ---------------------------------


static TestStatus
tf23(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: local version fscmp_strinct", ++subnum);
    {
        const char pt[] = "Qwertyuiop12345";
        fs  s = fscopy(pt);;
        fs  s1 = fsliteral(pt);

        test_validatefree(
            fscmp_strict(s, s1), (fsfree(s), fsfree(s1) ),
            "Literal and normal fs must be equal '%s' != '%s' || %zu != %zu", fsstr(s), fsstr(s1), fslen(s), fslen(s1)
        );

        fs_sprintf(&s, "bla bla bla %s %s %s %s ...", pt, pt, pt, pt);
        fs  s2 = fsclone(s);

        test_validatefree(
            fscmp_strict(s, s2), (fsfree(s), fsfree(s1), fsfree(s2) ),
             "Origin and clone fs must be equal '%s' != '%s' || %zu != %zu", fsstr(s), fsstr(s2), fslen(s), fslen(s2)
        );
        fsfree(s), fsfree(s1), fsfree(s2);
    }
    test_sub("subtest %d: pointer version fs_cmp_strinct", ++subnum);
    {

    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 24 ---------------------------------

static TestStatus
tf24(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: move local", ++subnum);
    {
        const char pattern[] = "Qwertyuiop12345";
        fs      s = fscopy(pattern);
        fs      *sp = fsmoveto_heap(s);
        test_validatefree(
            s.v == NULL && s.sz == 0 && s.len == 0, fs_free(sp),
            "Must be empty after moving but not %p %zu: %zu %d", s.v, s.sz, s.len, s.flags
        );
        test_validatefree(
            strcmp(pattern, sp->v) == 0 && fs_len(sp) == strlen(pattern), fs_free(sp),
            "Must be equal '%s' and '%s'", pattern, sp->v
        );
        fsfree(s);  // should work!
        fs_free(sp);
    }
    test_sub("subtest %d: move literal", ++subnum);
    {
        const char pattern[] = "Qwertyuiop1234567890";
        fs      lit = fsliteral(pattern);
        fs      *sp = fsmoveto_heap(lit);

        test_validatefree(
            strcmp(pattern, fs_str(sp) ) == 0 && fs_len(sp) == strlen(pattern), fs_free(sp),
            "Must be equal '%s' and '%s'", pattern, fs_str(sp)
        );
        //fstechprint(*sp);
        //fstechprint(lit);
        test_validatefree(
             strcmp(pattern, fs_str(sp) ) == 0 && fs_len(sp) == strlen(pattern), fs_free(sp),
             "Origin and clone fs must be equal '%s' != '%s' || %lu != %zu", pattern, fs_str(sp), strlen(pattern), fs_len(sp)
        );
        fsfree(lit);  // should work!
        fs_free(sp);
    }
    test_sub("subtest %d: move MOVED fs", ++subnum);
    {
        const char pattern[] = "Qwertyuiop1234567890!!!!!!!!";
        fs         s = fscopy(pattern);
        fs        *sp = fsmoveto_heap(s);
        fs        *sp2 = fs_moveto_heap(sp);
        //fstechprint(s);
        ////fstechprint(*sp); // impossible, sp pointer to nowhere
        //fstechprint(*sp2);
        test_validatefree(
            strcmp(pattern, fs_str(sp2) ) == 0 && fs_len(sp2) == strlen(pattern), fs_free(sp2),
            "Origin and clone fs must be equal '%s' != '%s' || %lu != %zu", pattern, fs_str(sp), strlen(pattern), fs_len(sp)
        );
        fsfree(s);
        //fs_free(sp);  // impossible, sp pointer to nowhere
        fs_free(sp2);   // !!!
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 25 ---------------------------------
static TestStatus
tf25(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: fscanf nothing", ++subnum);
    {
        const char   fname[] = "res/fs/fscanf.dat";
        FILE        *f = fopen(fname, "w+");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s for w+", fname);
        fclose(f);
        f = fopen(fname, "r");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_READ, "Unable to open %s for r", fname);
        fs  s = FS();
        test_validatefree(
            fs_fscanf(f, &s) == false,
            (fsfree(s), fclose(f) ),
            "Line must NOT be read, cnt %zu [%s]", fslen(s), fsstr(s)
        );
        fsfree(s);
        fclose(f);
    }
    test_sub("subtest %d: just fscanf several lines", ++subnum);
    {
        const char   fname[] = "res/fs/fscanf.dat";
        FILE        *f = fopen(fname, "w+");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s for w+", fname);
        const int       cnt = 20;
        char            buf[200];
        for (int i = 0; i < cnt; i++){
            snprintf(buf, sizeof(buf) - 1, "just a string %4d --- hz", i);
            fprintf(f, "%s\n", buf);
        }
        fflush(f);
        rewind(f);
        fs  s = FS();
        for (int i = 0; i < cnt; i++){
            test_validatefree(
                fs_fscanf(f, &s),
                (fsfree(s), fclose(f) ),
                "Unable to fs_fscanf() %d line", i
            );
            snprintf(buf, sizeof(buf) - 1, "just a string %4d --- hz", i);
            test_validatefree(
                strcmp(buf, fsstr(s)) == 0,
                (fsfree(s), fclose(f) ),
                "Line %d not matched [%s] vs fs [%s]", i, buf, fsstr(s)
            );
        }
        fclose(f);
        fsfree(s);
    }
    /* 3. Пустая строка (только перевод строки) */
    test_sub("subtest %d: read empty line", ++subnum);
    {
        const char fname[] = "res/fs/fscanf_empty.dat";
        FILE *f = fopen(fname, "w+");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s", fname);
        fprintf(f, "\n");
        fflush(f);
        rewind(f);

        fs s = FS();
        test_validatefree(
            fs_fscanf(f, &s) && fslen(s) == 0,
            (fsfree(s), fclose(f)),
            "Empty line must be read with length 0, got len=%zu, str='%s'", fslen(s), fsstr(s)
        );
        fsfree(s);
        fclose(f);
    }

    /* 4. Последняя строка без завершающего перевода строки */
    test_sub("subtest %d: read last line without newline", ++subnum);
    {
        const char fname[] = "res/fs/fscanf_last.dat";
        FILE *f = fopen(fname, "w+");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s", fname);
        fprintf(f, "line1\nline2");
        fflush(f);
        rewind(f);

        fs s = FS();

        // читаем первую строку
        test_validatefree(
            fs_fscanf(f, &s) && strcmp(fsstr(s), "line1") == 0,
            (fsfree(s), fclose(f)),
            "First line must be 'line1', got '%s'", fsstr(s)
        );

        // читаем вторую (последнюю) строку без \n
        test_validatefree(
            fs_fscanf(f, &s) && strcmp(fsstr(s), "line2") == 0,
            (fsfree(s), fclose(f)),
            "Last line without newline must be 'line2', got '%s'", fsstr(s)
        );

        fsfree(s);
        fclose(f);
    }

    /* 5. Строка с пробелами и спецсимволами */
    test_sub("subtest %d: read line with spaces and special chars", ++subnum);
    {
        const char fname[] = "res/fs/fscanf_special.dat";
        FILE *f = fopen(fname, "w+");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s", fname);
        const char *special_line = "  hello,\tworld!  ";
        fprintf(f, "%s\n", special_line);
        fflush(f);
        rewind(f);

        fs s = FS();
        test_validatefree(
            fs_fscanf(f, &s) && strcmp(fsstr(s), special_line) == 0,
            (fsfree(s), fclose(f)),
            "Line with spaces must be '%s', got '%s'", special_line, fsstr(s)
        );
        fsfree(s);
        fclose(f);
    }

    /* 6. Очень длинная строка (2000 символов) */
    test_sub("subtest %d: read long line", ++subnum);
    {
        const char fname[] = "res/fs/fscanf_long.dat";
        FILE *f = fopen(fname, "w+");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s", fname);

        // генерируем длинную строку
        size_t    long_len = 200000;
        char     *long_str = malloc(long_len + 1);
        if (!long_str)
            userraiseint(ERR_UNABLE_ALLOCATE, "malloc failed");
        for (size_t i = 0; i < long_len; i++)
            long_str[i] = itoupper(i % 26);   // 'A' + (i % 26);
        long_str[long_len] = '\0';

        fprintf(f, "%s\n", long_str);
        fflush(f);
        rewind(f);

        fs s = FS();
        test_validatefree(
            fs_fscanf(f, &s) && fslen(s) == long_len && strcmp(fsstr(s), long_str) == 0,
            (fsfree(s), free(long_str), fclose(f)),
            "Long line length must be %zu, got len=%zu", long_len, fslen(s)
        );
        fsfree(s);
        free(long_str);
        fclose(f);
    }

    /* 7. Перезапись существующего fs (повторный вызов fs_fscanf) */
    test_sub("subtest %d: overwrite existing fs", ++subnum);
    {
        const char fname[] = "res/fs/fscanf_overwrite.dat";
        FILE *f = fopen(fname, "w+");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s", fname);
        fprintf(f, "old line11111\nnew line\n");
        fflush(f);
        rewind(f);

        fs s = FS();

        // читаем первую строку
        test_validatefree(
            fs_fscanf(f, &s) && strcmp(fsstr(s), "old line11111") == 0,
            (fsfree(s), fclose(f)),
            "First line must be 'old line', got '%s'", fsstr(s)
        );

        // читаем вторую строку в тот же объект s – он должен перезаписаться
        test_validatefree(
            fs_fscanf(f, &s) && strcmp(fsstr(s), "new line") == 0,
            (fsfree(s), fclose(f)),
            "After second read, s must be 'new line', got '%s'", fsstr(s)
        );

        fsfree(s);
        fclose(f);
    }
    check_leak(true);
    /* 8. Вызов fs_fscanf(f, NULL) – успешное чтение */
    test_sub("subtest %d: fs_fscanf with NULL (create new)", ++subnum);
    {
        const char fname[] = "res/fs/fscanf_null_new.dat";
        FILE *f = fopen(fname, "w+");
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s", fname);
        fprintf(f, "hello from NULL\n");
        rewind(f);

        fs *sp = fs_fscanf(f, NULL);   // создаёт новый fs
        test_validatefree(
            sp != NULL && strcmp(fs_str(sp), "hello from NULL") == 0,
            (sp ? fs_free(sp) : (void)0, fclose(f)),
            "fs_fscanf(NULL) should return new fs with correct string, got %p, str='%s'",
            (void*)sp, sp ? fs_str(sp) : "null"
        );
        //if (sp)
        fs_free(sp);    // MUST work even if sp is NULL
        fclose(f);
    }

    /* 9. Вызов fs_fscanf(f, NULL) – EOF (пустой файл) */
    test_sub("subtest %d: fs_fscanf with NULL (EOF)", ++subnum);
    {
        const char fname[] = "res/fs/fscanf_null_eof.dat";
        FILE *f = fopen(fname, "w+");   // создали пустой файл
        if (!f)
            userraiseint(ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s", fname);
        rewind(f);

        fs *sp = fs_fscanf(f, NULL);
        test_validatefree(
            sp == NULL,
            fclose(f),
            "fs_fscanf(NULL) on empty file must return NULL"
        );
        // just to be sure
        //if (sp)
        fs_free(sp); // MUST work even if sp is NULL
        // если бы вернулся не NULL, надо было бы освободить, но этого не произошло
        fclose(f);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 26 ---------------------------------
static TestStatus
tf26(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    /* 1. Пустой формат */
    test_sub("subtest %d: empty format", ++subnum);
    {
        fs      s = fscopyf("");
        test_validatefree(
            fslen(s) == 0,
            fsfree(s),
            "Empty format must have length 0, got %zu", fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "") == 0,
            fsfree(s),
            "Empty format must produce empty string, got '%s'", fsstr(s)
        );
        fsfree(s);
    }
    check_leak(true);

    /* 2. Простой текст (без спецификаторов) */
    test_sub("subtest %d: plain text", ++subnum);
    {
        const char *text = "hello, world";
        fs      s = fscopyf("%s", text);   /* можно и просто fscopyf("hello, world") */
        test_validatefree(
            fslen(s) == strlen(text),
            fsfree(s),
            "Plain text: length mismatch, got %zu, expected %zu", fslen(s), strlen(text)
        );
        test_validatefree(
            strcmp(fsstr(s), text) == 0,
            fsfree(s),
            "Plain text: content mismatch, got '%s', expected '%s'", fsstr(s), text
        );
        fsfree(s);
    }
    check_leak(true);

    /* 3. Формат с числами и строкой */
    test_sub("subtest %d: formatted string", ++subnum);
    {
        int     ival = 42;
        double  dval = 3.1415;
        const char *sval = "test";
        char    expected[200];
        snprintf(expected, sizeof(expected), "int=%d, double=%.4f, str=%s", ival, dval, sval);

        fs      s = fscopyf("int=%d, double=%.4f, str=%s", ival, dval, sval);
        test_validatefree(
            fslen(s) == strlen(expected),
            fsfree(s),
            "Formatted length: got %zu, expected %zu", fslen(s), strlen(expected)
        );
        test_validatefree(
            strcmp(fsstr(s), expected) == 0,
            fsfree(s),
            "Formatted content: got '%s', expected '%s'", fsstr(s), expected
        );
        fsfree(s);
    }
    check_leak(true);

    /* 4. Длинная строка (проверка расширения буфера) */
    test_sub("subtest %d: long string (expands buffer)", ++subnum);
    {
        // Генерируем длинную строку из повторяющихся символов
        size_t   repeat = 20000;
        char    *long_text = malloc(repeat + 1);
        if (!long_text)
            userraiseint(ERR_UNABLE_ALLOCATE, "malloc failed");
        memset(long_text, 'A', repeat);
        long_text[repeat] = '\0';

        fs      s = fscopyf("%s", long_text);
        test_validatefree(
            fslen(s) == repeat,
            fsfree(s),
            "Long string length: got %zu, expected %zu", fslen(s), repeat
        );
        test_validatefree(
            strcmp(fsstr(s), long_text) == 0,
            fsfree(s),
            "Long string content mismatch"
        );

        free(long_text);
        fsfree(s);
    }
    check_leak(true);

    /* 5. Последовательные вызовы (проверка утечек) */
    test_sub("subtest %d: sequential calls (no leaks)", ++subnum);
    {
        // Несколько созданий и удалений, чтобы убедиться, что память не утекает
        for (int i = 0; i < 50; i++) {
            fs s = fscopyf("iteration_%d", i);
            fsfree(s);
        }
        // Если бы были утечки, check_leak в конце функции их обнаружит
    }
    check_leak(true);

    /* 6. Строка со спецсимволами и пробелами */
    test_sub("subtest %d: special chars and spaces", ++subnum);
    {
        const char *special = "  \t\n\r  hello  \t\n\r  ";
        fs      s = fscopyf("%s", special);
        test_validatefree(
            strcmp(fsstr(s), special) == 0,
            fsfree(s),
            "Special chars mismatch: got '%s', expected '%s'", fsstr(s), special
        );
        fsfree(s);
    }
    check_leak(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 27 ---------------------------------

static TestStatus
tf27(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    /* ================= fs_rpad ================= */

    /* 1. rpad: короткая строка, pad длиннее */
    test_sub("subtest %d: rpad short string with longer pad", ++subnum);
    {
        size_t  newlen = 7;
        fs      s = fscopy("abc");
        fs      pad = fscopy("XY");
        fs      result = fs_rpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == newlen,
            (fsfree(s), fsfree(pad) ),
            "rpad: length must be %zu, got %zu", newlen, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "abcXYXY") == 0,
            (fsfree(s), fsfree(pad) ),
            "rpad: expected 'abcXYXY', got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    /* 2. rpad: обрезание длинной строки */
    test_sub("subtest %d: rpad cut long string", ++subnum);
    {
        size_t  newlen = 5;
        fs      s = fscopy("hello world");
        fs      pad = fscopy(".");
        fs      result = fs_rpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == newlen,
            (fsfree(s), fsfree(pad)),
            "rpad cut: length must be %zu, got %zu", newlen, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "hello") == 0,
            (fsfree(s), fsfree(pad)),
            "rpad cut: expected 'hello', got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    /* 3. rpad: пустой pad – строка остаётся без изменений (короткая) */
    test_sub("subtest %d: rpad empty pad (short string)", ++subnum);
    {
        size_t  newlen = 5;
        fs      s = fscopy("abc");
        fs      pad = FS();               /* пустая строка */
        fs      result = fs_rpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == 3,
            (fsfree(s), fsfree(pad)),
            "rpad empty pad: length must stay 3, got %zu", fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "abc") == 0,
            (fsfree(s), fsfree(pad)),
            "rpad empty pad: expected 'abc', got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    /* 4. rpad: циклическое заполнение (pad короче) */
    test_sub("subtest %d: rpad cyclic pad", ++subnum);
    {
        size_t  newlen = 8;
        fs      s = fscopy("AB");
        fs      pad = fscopy("123");
        fs      result = fs_rpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == newlen,
            (fsfree(s), fsfree(pad)),
            "rpad cyclic: length must be %zu, got %zu", newlen, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "AB123123") == 0,
            (fsfree(s), fsfree(pad)),
            "rpad cyclic: expected 'AB123123', got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    /* 5. rpad: len == 0, строка непустая – обрезается до пустой */
    test_sub("subtest %d: rpad len=0 (cut to empty)", ++subnum);
    {
        size_t  newlen = 0;
        fs      s = fscopy("cutme");
        fs      pad = fscopy("-");
        fs      result = fs_rpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == 0,
            (fsfree(s), fsfree(pad)),
            "rpad len=0: length must be 0, got %zu", fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "") == 0,
            (fsfree(s), fsfree(pad)),
            "rpad len=0: expected empty, got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }

    /* ================= fs_lpad ================= */

    /* 6. lpad: короткая строка, pad длиннее */
    test_sub("subtest %d: lpad short string with longer pad", ++subnum);
    {
        size_t  newlen = 7;
        fs      s = fscopy("abc");
        fs      pad = fscopy("XY");
        fs      result = fs_lpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == newlen,
            (fsfree(s), fsfree(pad)),
            "lpad: length must be %zu, got %zu", newlen, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "XYXYabc") == 0,
            (fsfree(s), fsfree(pad)),
            "lpad: expected 'XYXYabc', got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    /* 7. lpad: обрезание длинной строки */
    test_sub("subtest %d: lpad cut long string", ++subnum);
    {
        size_t  newlen = 5;
        fs      s = fscopy("hello world");
        fs      pad = fscopy(".");
        fs      result = fs_lpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == newlen,
            (fsfree(s), fsfree(pad)),
            "lpad cut: length must be %zu, got %zu", newlen, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "hello") == 0,
            (fsfree(s), fsfree(pad)),
            "lpad cut: expected 'hello', got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    /* 8. lpad: пустой pad – строка не меняется (короткая) */
    test_sub("subtest %d: lpad empty pad (short string)", ++subnum);
    {
        size_t  newlen = 5;
        fs      s = fscopy("abc");
        fs      pad = FS();
        fs      result = fs_lpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == 3,
            (fsfree(s), fsfree(pad)),
            "lpad empty pad: length must stay 3, got %zu", fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "abc") == 0,
            (fsfree(s), fsfree(pad)),
            "lpad empty pad: expected 'abc', got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    /* 9. lpad: циклическое заполнение */
    test_sub("subtest %d: lpad cyclic pad", ++subnum);
    {
        size_t  newlen = 8;
        fs      s = fscopy("AB");
        fs      pad = fscopy("123");
        fs      result = fs_lpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == newlen,
            (fsfree(s), fsfree(pad)),
            "lpad cyclic: length must be %zu, got %zu", newlen, fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "123123AB") == 0,
            (fsfree(s), fsfree(pad)),
            "lpad cyclic: expected '123123AB', got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    /* 10. lpad: len == 0 */
    test_sub("subtest %d: lpad len=0 (cut to empty)", ++subnum);
    {
        size_t  newlen = 0;
        fs      s = fscopy("cutme");
        fs      pad = fscopy("-");
        fs      result = fs_lpad(&s, newlen, &pad);
        fstechfprint(logfile, result);

        test_validatefree(
            fslen(s) == 0,
            (fsfree(s), fsfree(pad)),
            "lpad len=0: length must be 0, got %zu", fslen(s)
        );
        test_validatefree(
            strcmp(fsstr(s), "") == 0,
            (fsfree(s), fsfree(pad)),
            "lpad len=0: expected empty, got '%s'", fsstr(s)
        );
        fsfree(s);
        fsfree(pad);
    }
    check_leak(true);
    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 28 ---------------------------------
static TestStatus
tf28(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Создание копии из пустой строки (FS()) */
    test_sub("subtest %d: heapcreate from empty FS", ++subnum);
    {
        fs      orig = FS();                    // пустая строка
        fs     *copy = fs_heapcreate(&orig);

        // Оригинал не должен пострадать
        test_validatefree(
            fslen(orig) == 0 && fsstr(orig) == NULL,
            fs_free(copy),
            "Original must stay empty after heapcreate"
        );
        // Копия должна быть пустой, но валидной
        test_validatefree(
            copy != NULL && fs_len(copy) == 0,
            fs_free(copy),
            "Copy must be non-NULL with length 0"
        );
        test_validatefree(
            fs_bodyalloc(copy),   // проверяем FS_FLAG_BODYALLOC
            fs_free(copy),
            "Copy must have FS_FLAG_BODYALLOC flag"
        );

        fs_free(copy);
        fs_alloc_check(true);
    }

    /* 2. Создание копии из обычной строки (fscopy) */
    test_sub("subtest %d: heapcreate from normal string", ++subnum);
    {
        const char *text = "test string";
        fs      orig = fscopy(text);
        fs     *copy = fs_heapcreate(&orig);

        // Оригинал не должен измениться
        test_validatefree(
            strcmp(fsstr(orig), text) == 0 && fslen(orig) == strlen(text),
            (fsfree(orig), fs_free(copy)),
            "Original must be unchanged after heapcreate"
        );
        test_validatefree(
            !fs_bodyalloc(&orig),
            (fsfree(orig), fs_free(copy)),
            "Original must NOT have FS_FLAG_BODYALLOC"
        );

        // Копия должна содержать ту же строку и быть независимой
        test_validatefree(
            strcmp(fs_str(copy), text) == 0 && fs_len(copy) == strlen(text),
            (fsfree(orig), fs_free(copy)),
            "Copy must contain the same string"
        );
        test_validatefree(
            fs_bodyalloc(copy),
            (fsfree(orig), fs_free(copy)),
            "Copy must have FS_FLAG_BODYALLOC flag"
        );

        // Модифицируем оригинал – копия не должна измениться
        fs_sprintf(&orig, "modified");
        test_validatefree(
            strcmp(fs_str(copy), text) == 0,
            (fsfree(orig), fs_free(copy)),
            "Copy must remain unchanged after original modification"
        );

        fsfree(orig);
        fs_free(copy);
        fs_alloc_check(true);
    }

    /* 3. Копирование уже перемещённого объекта (после fs_moveto_heap) */
    test_sub("subtest %d: heapcreate from moved object", ++subnum);
    {
        const char *text = "move_then_copy";
        fs      orig = fscopy(text);
        fs     *moved = fs_moveto_heap(&orig);      // orig опустошён, moved в куче с FS_FLAG_BODYALLOC

        // orig больше не владеет строкой
        test_validatefree(
            fslen(orig) == 0 && fsstr(orig) == NULL,
            fs_free(moved),
            "After moveto_heap, original must be empty"
        );

        // Теперь создаём копию из перемещённого объекта
        fs     *copy = fs_heapcreate(moved);

        test_validatefree(
            strcmp(fs_str(copy), text) == 0 && fs_len(copy) == strlen(text),
            (fs_free(moved), fs_free(copy)),
            "Copy from moved object must contain original string"
        );
        test_validatefree(
            fs_bodyalloc(copy),
            (fs_free(moved), fs_free(copy)),
            "Copy from moved object must have FS_FLAG_BODYALLOC"
        );

        fs_free(moved);
        fs_free(copy);
        fs_alloc_check(true);
    }

    /* 4. NULL-указатель – ожидаем аварийное завершение (перехватываем сигнал) */
    test_sub("subtest %d: heapcreate with NULL pointer (must raise error)", ++subnum);
    {
        if (!try()) {
            fs *copy = fs_heapcreate(NULL);     // должно упасть внутри fs_heapcreate
            // Если мы здесь, ошибки не было – тест провален
            test_validate(
                false,
                "fs_heapcreate(NULL) must raise an error, but it didn't"
            );
            if (copy) fs_free(copy);            // на всякий случай
        } else {
            // Сигнал перехвачен – это ожидаемое поведение
            test_validate(
                true,
                "fs_heapcreate(NULL) correctly raised an error"
            );
        }
        fs_alloc_check(true);
    }

    /* 5. Последовательные вызовы и проверка утечек */
    test_sub("subtest %d: heapcreate multiple calls (leak check)", ++subnum);
    {
        const char *words[] = {"one", "two", "three"};
        fs     *copies[3];

        for (int i = 0; i < COUNT(words); i++) {
            fs      orig = fscopy(words[i]);
            copies[i] = fs_heapcreate(&orig);
            fsfree(orig);   // оригинал больше не нужен
        }

        // Проверяем содержимое
        for (int i = 0; i < COUNT(words); i++) {
            test_validatefree(
                strcmp(fs_str(copies[i]), words[i]) == 0,
                (fs_free(copies[0]), fs_free(copies[1]), fs_free(copies[2])),
                "Copy %d must be '%s'", i, words[i]
            );
        }

        for (int i = 0; i < COUNT(words); i++) fs_free(copies[i]);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_heapcopy ---------------------------------
static TestStatus
tf_fs_heapcopy(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Обычная строка */
    test_sub("subtest %d: heapcopy normal string", ++subnum);
    {
        const char *text = "hello, world";
        fs     *copy = fs_heapcopy(text);

        test_validatefree(
            strcmp(fs_str(copy), text) == 0 && fs_len(copy) == strlen(text),
            fs_free(copy),
            "Heapcopy must contain '%s', got '%s' (len %zu)", text, fs_str(copy), fs_len(copy)
        );
        test_validatefree(
            fs_bodyalloc(copy),
            fs_free(copy),
            "Copy must have FS_FLAG_BODYALLOC flag"
        );

        fs_free(copy);
        fs_alloc_check(true);
    }

    /* 2. Пустая строка "" */
    test_sub("subtest %d: heapcopy empty string", ++subnum);
    {
        const char *text = "";
        fs     *copy = fs_heapcopy(text);

        test_validatefree(
            fs_len(copy) == 0 && copy->v != NULL && fs_str(copy)[0] == '\0',
            fs_free(copy),
            "Heapcopy of empty string must have len=0, v!=NULL, got len=%zu, v=%p", fs_len(copy), (void*)copy->v
        );
        test_validatefree(
            fs_bodyalloc(copy),
            fs_free(copy),
            "Copy must have FS_FLAG_BODYALLOC flag"
        );

        fs_free(copy);
        fs_alloc_check(true);
    }

    /* 3. Несколько вызовов и проверка утечек */
    test_sub("subtest %d: heapcopy multiple calls (leak check)", ++subnum);
    {
        const char *words[] = {"one", "two", "three"};
        fs     *copies[3];

        for (int i = 0; i < COUNT(words); i++) {
            copies[i] = fs_heapcopy(words[i]);
        }

        for (int i = 0; i < COUNT(words); i++) {
            test_validatefree(
                strcmp(fs_str(copies[i]), words[i]) == 0,
                (fs_free(copies[0]), fs_free(copies[1]), fs_free(copies[2])),
                "Copy %d must be '%s', got '%s'", i, words[i], fs_str(copies[i])
            );
            fs_free(copies[i]);
        }
        fs_alloc_check(true);
    }

    /* 4. NULL-указатель (ожидаем ошибку) */
    test_sub("subtest %d: heapcopy NULL pointer (must raise error)", ++subnum);
    {
        if (!try()) {
            fs *copy = fs_heapcopy(NULL);
            test_validate(
                false,
                "fs_heapcopy(NULL) must raise an error, but it didn't"
            );
            if (copy) fs_free(copy);
        } else {
            test_validate(
                true,
                "fs_heapcopy(NULL) correctly raised an error"
            );
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_moveto_heapstr ---------------------------------

static TestStatus
tf_moveto_heapstr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Обычная строка */
    test_sub("subtest %d: move normal string", ++subnum);
    {
        const char   pt[] = "hello world";
        char        *orig = strdup(pt);
        fs          *moved = fs_moveto_heapstr(&orig);

        //fstechprint(*moved);

        test_validatefree(
            moved != NULL && strcmp(fs_str(moved), pt) == 0 && fs_len(moved) == sizeof(pt) - 1,
            fs_free(moved),
            "Moved string mismatch: got '%s', len %zu, expected '%s', %lu",
            fs_str(moved), fs_len(moved), pt, sizeof(pt) - 1
        );
        test_validatefree(
            fs_bodyalloc(moved),
            fs_free(moved),
            "Moved fs must have FS_FLAG_BODYALLOC"
        );
        test_validate(
            orig == NULL,
            "Original pointer must be NULL after move"
        );

        fs_free(moved);
        // orig уже NULL, free не нужен
        fs_alloc_check(true);
    }

    /* 2. Пустая строка (только '\0') */
    test_sub("subtest %d: move empty string", ++subnum);
    {
        char *orig = strdup("");
        fs *moved = fs_moveto_heapstr(&orig);

        test_validatefree(
            moved != NULL && fs_len(moved) == 0 && fs_str(moved)[0] == '\0',
            fs_free(moved),
            "Empty string must have len=0, got %zu, str='%s'", fs_len(moved), fs_str(moved)
        );
        test_validatefree(
            fs_bodyalloc(moved),
            fs_free(moved),
            "Empty moved fs must have FS_FLAG_BODYALLOC"
        );
        test_validate(
            orig == NULL, 
            "Original pointer must be NULL after move"
        );

        fs_free(moved);
        fs_alloc_check(true);
    }

    /* 3. NULL-указатель orig (сам char** == NULL) – должен упасть */
    test_sub("subtest %d: move with NULL orig", ++subnum);
    {
        if (!try()) {
            fs *moved = fs_moveto_heapstr(NULL);
            test_validate(
                false,
                "fs_moveto_heapstr(NULL) must raise error, but returned %p",
                (void*)moved
            );
            if (moved)
                fs_free(moved);
        } else {
            test_validate(
                true,
                "fs_moveto_heapstr(NULL) correctly raised error"
            );
        }
        fs_alloc_check(true);
    }

    /* 4. *orig == NULL – должен упасть */
    test_sub("subtest %d: move with *orig == NULL", ++subnum);
    {
        char *orig = NULL;
        if (!try()) {
            fs *moved = fs_moveto_heapstr(&orig);
            test_validate(
                false,
                "fs_moveto_heapstr(&orig) with *orig==NULL must raise error"
            );
            if (moved)
                fs_free(moved);
        } else {
            test_validate(
                true,
                "fs_moveto_heapstr(&orig) with *orig==NULL correctly raised error"
            );
        }
        fs_alloc_check(true);
    }

    /* 5. Последовательные вызовы и проверка утечек */
    test_sub("subtest %d: multiple moves (leak check)", ++subnum);
    {
        const char  *words[] = {"one", "two", "three"};
        fs          *moved[3] = { NULL, NULL, NULL };

        for (int i = 0; i < COUNT(words); i++) {
            char *orig = strdup(words[i]);
            moved[i] = fs_moveto_heapstr(&orig);
            test_validate(
                orig == NULL,
                "orig must be NULL after move %d", i
            );
        }

        for (int i = 0; i < COUNT(words); i++) {
            test_validatefree(
                strcmp(fs_str(moved[i]), words[i]) == 0,
                (fs_free(moved[0]), fs_free(moved[1]), fs_free(moved[2])),
                "Mismatch at %d: expected '%s', got '%s'", i, words[i], fs_str(moved[i])
            );
            fs_free(moved[i]);
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fsmovetostr ---------------------------------

static TestStatus
tf31_fsmovetostr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Обычная строка */
    test_sub("subtest %d: move normal string", ++subnum);
    {
        fs     *fs_ptr = fs_heapcopy("hello world");
        char   *str = fsmovetostr(fs_ptr);

        test_validatefree(
            str != NULL && strcmp(str, "hello world") == 0,
            free(str),
            "Moved string mismatch: got '%s', expected 'hello world'", str
        );
        test_validatefree(
            fs_ptr == NULL,
            free(str),
            "After move, fs_ptr must be NULL");
        free(str);
        fs_alloc_check(true);
    }

    /* 2. Пустая строка */
    test_sub("subtest %d: move empty string", ++subnum);
    {
        fs     *fs_ptr = fs_heapcopy("");
        char   *str = fsmovetostr(fs_ptr);

        test_validatefree(
            str != NULL && str[0] == '\0',
            free(str),
            "Empty string must have length 0, got '%s'", str
        );
        test_validate(fs_ptr == NULL, "After move, fs_ptr must be NULL");
        free(str);
        fs_alloc_check(true);
    }

    /* 3. NULL-указатель на fs* (pfs == NULL) – должен упасть */
    test_sub("subtest %d: NULL pfs", ++subnum);
    {
        char    *str = NULL;
        if (!try()) {
            str = fs_movetostr(NULL);
            test_validate(
                false,
                "NULL pfs must raise error, but returned %p",
                (void*)str
            );
            if (str)
                free(str);
        } else {
            test_validate(
                true,
                "NULL pfs correctly raised error"
            );
            if (str)
                free(str);
        }
        fs_alloc_check(true);
    }

    /* 4. *pfs == NULL – должен упасть */
    test_sub("subtest %d: *pfs == NULL", ++subnum);
    {
        char    *str = NULL;
        fs     *fs_ptr = NULL;
        if (!try()) {
            str = fsmovetostr(fs_ptr);
            test_validate(
                false,
                "*pfs == NULL must raise error, but returned %p",
                (void*)str
            );
            if (str)
                free(str);
        } else {
            test_validate(true,
                "*pfs == NULL correctly raised error"
            );
            if (str)
                 free(str);
        }
        fs_alloc_check(true);
    }

    /* 5. Последовательные вызовы (проверка утечек) */
    test_sub("subtest %d: multiple moves (leak check)", ++subnum);
    {
        const char *words[] = {"one", "two", "three"};
        for (int i = 0; i < COUNT(words); i++) {
            fs     *fs_ptr = fs_heapcopy(words[i]);
            char   *str = fsmovetostr(fs_ptr);
            test_validatefree(
                strcmp(str, words[i]) == 0,
                 free(str),
                "Mismatch at %d", i
            );
            free(str);
            test_validate(
                fs_ptr == NULL,
                "fs_ptr must be NULL after move"
            );
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_genrnd() simple tests ---------------------------------

static TestStatus
tf_genrnd(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Строчные буквы */
    test_sub("subtest %d: fs_genrnd lowercase", ++subnum);
    {
        fs s = fsinit(20);
        fs_genrnd(&s, 10, 'a');
        test_validatefree(
            fs_len(&s) == 10,
            fsfree(s),
            "Length must be 10, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 10; i++) {
            test_validatefree(
                s.v[i] >= 'a' && s.v[i] <= 'z',
                fsfree(s),
                "Char at %d must be in 'a'..'z', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 2. Заглавные буквы */
    test_sub("subtest %d: fs_genrnd uppercase", ++subnum);
    {
        fs s = fsinit(20);
        fs_genrnd(&s, 8, 'A');
        test_validatefree(
            fs_len(&s) == 8,
            fsfree(s),
            "Length must be 8, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 8; i++) {
            test_validatefree(
                s.v[i] >= 'A' && s.v[i] <= 'Z',
                fsfree(s),
                "Char at %d must be in 'A'..'Z', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 3. Цифры */
    test_sub("subtest %d: fs_genrnd digits", ++subnum);
    {
        fs s = fsinit(20);
        fs_genrnd(&s, 6, '0');
        test_validatefree(
            fs_len(&s) == 6,
            fsfree(s),
            "Length must be 6, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 6; i++) {
            test_validatefree(
                s.v[i] >= '0' && s.v[i] <= '9',
                fsfree(s),
                "Char at %d must be in '0'..'9', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 4. Нулевая длина */
    test_sub("subtest %d: fs_genrnd zero length", ++subnum);
    {
        fs s = fsinit(10);
        fs_genrnd(&s, 0, 'a');
        test_validatefree(
            fs_len(&s) == 0,
            fsfree(s),
            "Length must be 0, got %zu", fs_len(&s)
        );
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 6. Неизвестный тип – должны быть '?' */
    test_sub("subtest %d: fs_genrnd unknown type", ++subnum);
    {
        fs s = fsinit(10);
        fs_genrnd(&s, 5, 'x');
        test_validatefree(
            fs_len(&s) >= 5,
            fsfree(s),
            "Length must be >= 5, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 5; i++) {
            test_validatefree(
                s.v[i] == '?',
                fsfree(s),
                "Char at %d must be '?', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 1. Строчные буквы */
    test_sub("subtest %d: fs_genrnd lowercase zero init", ++subnum);
    {
        fs s = FS();
        fs *res = fs_genrnd(&s, 10, 'a');
        test_validatefree(
            res == &s && fs_len(&s) == 10,
            fsfree(s),
            "Returned pointer must be &s, length must be 10, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 10; i++) {
            test_validatefree(
                s.v[i] >= 'a' && s.v[i] <= 'z',
                fsfree(s),
                "Char at %d must be in 'a'..'z', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 2. Заглавные буквы */
    test_sub("subtest %d: fs_genrnd uppercase zero init", ++subnum);
    {
        fs s = FS();
        fs_genrnd(&s, 8, 'A');
        test_validatefree(
            fs_len(&s) == 8,
            fsfree(s),
            "Length must be 8, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 8; i++) {
            test_validatefree(
                s.v[i] >= 'A' && s.v[i] <= 'Z',
                fsfree(s),
                "Char at %d must be in 'A'..'Z', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 3. Цифры */
    test_sub("subtest %d: fs_genrnd digits zero init", ++subnum);
    {
        fs s = FS();
        fs_genrnd(&s, 6, '0');
        test_validatefree(
            fs_len(&s) == 6,
            fsfree(s),
            "Length must be 6, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 6; i++) {
            test_validatefree(
                s.v[i] >= '0' && s.v[i] <= '9',
                fsfree(s),
                "Char at %d must be in '0'..'9', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 4. Нулевая длина */
    test_sub("subtest %d: fs_genrnd zero length zero init", ++subnum);
    {
        fs s = FS();
        fs_genrnd(&s, 0, 'a');
        test_validatefree(
            fs_len(&s) == 0,
            fsfree(s),
            "Length must be 0, got %zu", fs_len(&s)
        );
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 6. Неизвестный тип – символ '?' */
    test_sub("subtest %d: fs_genrnd unknown type zero init", ++subnum);
    {
        fs s = FS();
        fs_genrnd(&s, 5, 'x');
        test_validatefree(
            fs_len(&s) == 5,
            fsfree(s),
            "Length must be 5, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 5; i++) {
            test_validatefree(
                s.v[i] == '?',
                fsfree(s),
                "Char at %d must be '?', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_initrnd (constructor) -------------------------
static TestStatus
tf_initrnd(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Строчные буквы */
    test_sub("subtest %d: fs_initrnd lowercase", ++subnum);
    {
        fs s = fs_initrnd(10, 'a');
        test_validatefree(
            fs_len(&s) == 10,
            fsfree(s),
            "Length must be 10, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 10; i++) {
            test_validatefree(
                s.v[i] >= 'a' && s.v[i] <= 'z',
                fsfree(s),
                "Char at %d must be in 'a'..'z', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 2. Заглавные буквы */
    test_sub("subtest %d: fs_initrnd uppercase", ++subnum);
    {
        fs s = fs_initrnd(8, 'A');
        test_validatefree(
            fs_len(&s) == 8,
            fsfree(s),
            "Length must be 8, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 8; i++) {
            test_validatefree(
                s.v[i] >= 'A' && s.v[i] <= 'Z',
                fsfree(s),
                "Char at %d must be in 'A'..'Z', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 3. Цифры */
    test_sub("subtest %d: fs_initrnd digits", ++subnum);
    {
        fs s = fs_initrnd(6, '0');
        test_validatefree(
            fs_len(&s) == 6,
            fsfree(s),
            "Length must be 6, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 6; i++) {
            test_validatefree(
                s.v[i] >= '0' && s.v[i] <= '9',
                fsfree(s),
                "Char at %d must be in '0'..'9', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 4. Нулевая длина */
    test_sub("subtest %d: fs_initrnd zero length", ++subnum);
    {
        fs s = fs_initrnd(0, 'a');
        test_validatefree(
            fs_len(&s) == 0,
            fsfree(s),
            "Length must be 0, got %zu", fs_len(&s)
        );
        fsfree(s);
    }
    fs_alloc_check(true);

    /* 6. Неизвестный тип – символы '?' */
    test_sub("subtest %d: fs_initrnd unknown type", ++subnum);
    {
        fs s = fs_initrnd(5, 'x');
        test_validatefree(
            fs_len(&s) == 5,
            fsfree(s),
            "Length must be 5, got %zu", fs_len(&s)
        );
        for (int i = 0; i < 5; i++) {
            test_validatefree(
                s.v[i] == '?',
                fsfree(s),
                "Char at %d must be '?', got '%c'", i, s.v[i]
            );
        }
        fsfree(s);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST fs_isnull -------------------------
static TestStatus
tf34_fs_isnull(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. NULL pointer must be considered null */
    test_sub("subtest %d: fs_isnull(NULL)", ++subnum);
    {
        test_validate(fs_isnull(NULL) == true,
                      "NULL must be null");
    }

    /* 2. Empty stack fs (FS() with sz=0, v=0) must be null */
    test_sub("subtest %d: fs_isnull(empty stack fs)", ++subnum);
    {
        fs s = FS();
        test_validate(fs_isnull(&s) == true,
                      "FS() must be null (sz=0, v=0)");
        fsfree(s); // безопасно, даже если не было выделений
    }

    /* 3. fs with allocated empty string must not be null */
    test_sub("subtest %d: fs_isnull(fscopy(\"\"))", ++subnum);
    {
        fs s = fscopy("");
        test_validate(fs_isnull(&s) == false,
                      "fscopy(\"\") must not be null (has allocated buffer)");
        fsfree(s);
    }

    /* 4. fs with non-empty string must not be null */
    test_sub("subtest %d: fs_isnull(non-empty fs)", ++subnum);
    {
        fs s = fscopy("hello");
        test_validate(fs_isnull(&s) == false,
                      "non-empty fs must not be null");
        fsfree(s);
    }

    /* 5. fsinit result must not be null, but after fsfree it becomes null */
    test_sub("subtest %d: fs_isnull(fsinit(16)) and after free", ++subnum);
    {
        fs s = fsinit(16);
        test_validate(fs_isnull(&s) == false,
                      "fsinit must not be null (has buffer)");
        fsfree(s);
        test_validate(fs_isnull(&s) == true,
                      "after fsfree fs must be null again");
    }

    return TEST_PASSED;
}

// ------------------------- TEST fs_isempty / fsisempty -------------------------
static TestStatus
tf35_fs_isempty(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* fsisempty (stack version) */
    test_sub("subtest %d: fsisempty(FS()) is true", ++subnum);
    {
        fs s = FS();
        test_validate(fsisempty(s) == true, "FS() must be empty");
        fsfree(s);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: fsisempty(fscopy(\"\")) is true", ++subnum);
    {
        fs s = fscopy("");
        test_validate(fsisempty(s) == true, "fscopy(\"\") must be empty");
        fsfree(s);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: fsisempty(non-empty fs) is false", ++subnum);
    {
        fs s = fscopy("hello");
        test_validate(fsisempty(s) == false, "non-empty fs must not be empty");
        fsfree(s);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: fsisempty after fsfree is true", ++subnum);
    {
        fs s = fscopy("hello");
        fsfree(s);
        test_validate(fsisempty(s) == true, "after fsfree fs must be empty");
    }

    /* fs_isempty (pointer version) */
    test_sub("subtest %d: fs_isempty(NULL) is true", ++subnum);
    {
        test_validate(fs_isempty(NULL) == true, "NULL must be empty");
    }

    test_sub("subtest %d: fs_isempty(empty stacked FS()) is true", ++subnum);
    {
        fs s = FS();
        test_validate(fs_isempty(&s) == true, "FS() pointer must be empty");
        fsfree(s);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: fs_isempty(fscopy(\"\")) is true", ++subnum);
    {
        fs s = fscopy("");
        test_validate(fs_isempty(&s) == true, "fscopy(\"\") pointer must be empty");
        fsfree(s);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: fs_isempty(non-empty fs pointer) is false", ++subnum);
    {
        fs s = fscopy("hello");
        test_validate(fs_isempty(&s) == false, "non-empty fs pointer must not be empty");
        fsfree(s);
        fs_alloc_check(true);
    }

    return TEST_PASSED;
}

// ------------------------- TEST fs_movetostr / fsmovetostr -------------------------
static TestStatus
tf36_fs_movetostr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Stack fs: move string out */
    test_sub("subtest %d: fs_movetostr with stacked fs", ++subnum);
    {
        fs s = fscopy("hello");
        char *str = fs_movetostr(&s);
        test_validate(str != NULL && strcmp(str, "hello") == 0,
                      "moved string mismatch");
        test_validate(s.v == NULL && s.sz == 0 && s.len == 0,
                      "source fs must be empty after move");
        
        fs_alloc_check(true);
        free(str);          // освобождаем строку, счётчик g_free_cnt уже был увеличен
        // fsfree(s) не нужен, так как s пуста
        fs_alloc_check(true);
    }

    /* 2. Stack fs with empty string (v == NULL) */
    test_sub("subtest %d: fs_movetostr with empty fs", ++subnum);
    {
        fs s = FS();        // v == NULL
        char *str = fs_movetostr(&s);
        test_validate(str == NULL, "must return NULL for empty fs");
        test_validate(s.v == NULL && s.sz == 0 && s.len == 0,
                      "source fs must remain empty");
        // ничего не освобождаем
        fs_alloc_check(true);
    }

    /* 3. Перемещение из bodyalloc fs через fsmovetostr */
    test_sub("subtest %d: fsmovetostr from bodyalloc fs", ++subnum);
    {
        fs *p = fs_heapcopy("world");
        test_validate(p != NULL && fs_bodyalloc(p), "fs must be bodyalloc");

        char *str = fsmovetostr(p);
        test_validate(str != NULL && strcmp(str, "world") == 0,
                      "moved string mismatch");
        test_validate(p == NULL, "pointer must be NULL after move");
        fs_alloc_check(true);
        free(str);
        fs_alloc_check(true);
    }

    /* 4. Комбинация: стековый fs -> fs_heapcreate -> fsmovetostr */
    test_sub("subtest %d: stack fs -> heapcreate -> movefrom_heapstr", ++subnum);
    {
        const char *text = "combo1";
        fs orig = fscopy(text);
        fs *heap = fs_heapcreate(&orig);
        fsfree(orig);   // стековый больше не нужен

        test_validate(heap != NULL && fs_bodyalloc(heap), "heap must be bodyalloc");
        test_validate(strcmp(fs_str(heap), text) == 0, "heap content mismatch");

        char *str = fsmovetostr(heap);
        fs_alloc_check(true);

        test_validate(str != NULL && strcmp(str, text) == 0, "moved string mismatch");
        test_validate(heap == NULL, "heap pointer must be NULL after move");
        free(str);
        fs_alloc_check(true);
    }

    /* 5. Комбинация: стековый fs -> fs_moveto_heap -> fsmovetostr */
    test_sub("subtest %d: stack fs -> moveto_heap -> movefrom_heapstr", ++subnum);
    {
        const char *text = "combo2";
        fs orig = fscopy(text);
        fs *heap = fs_moveto_heap(&orig);   // перенос владения, orig опустошается
        test_validate(orig.v == NULL && orig.sz == 0 && orig.len == 0,
                      "original must be empty after moveto_heap");
        test_validate(heap != NULL && fs_bodyalloc(heap), "heap must be bodyalloc");
        test_validate(strcmp(fs_str(heap), text) == 0, "heap content mismatch");

        char *str = fsmovetostr(heap);
        fs_alloc_check(true);

        test_validate(str != NULL && strcmp(str, text) == 0, "moved string mismatch");
        test_validate(heap == NULL, "heap pointer must be NULL after move");
        free(str);
        fs_alloc_check(true);
    }

    /* 6. fs_movetostr на bodyalloc fs должен вызывать ошибку */
    test_sub("subtest %d: fs_movetostr on bodyalloc fs raises error", ++subnum);
    {
        fs *p = fs_heapcopy("bodyalloc");
        if (!try()) {
            char *str = fs_movetostr(p);
            test_validate(false, "must raise error for bodyalloc");
            free(str);
            fs_free(p);
        } else {
            test_validate(true, "correctly raised error");
            // Если userraise вернул управление, освобождаем ресурсы
            fs_free(p);
        }
        fs_alloc_check(true);
    }

    /* 8. Проверка независимости после перемещения */
    test_sub("subtest %d: independence after move", ++subnum);
    {
        fs orig = fscopy("original");
        char *str = fs_movetostr(&orig);
        test_validate(str != NULL && strcmp(str, "original") == 0,
                      "move string mismatch");
        test_validate(orig.v == NULL && orig.len == 0 && orig.sz == 0,
                      "original must be empty after move");

        str[0] = 'X';
        test_validate(orig.v == NULL, "orig must remain NULL after string modification");

        free(str);
        fs_alloc_check(true);
    }

    return TEST_PASSED;
}

// ------------------------- TEST fs_cmpstr() series simple tests -------------------------
static TestStatus
tf37_fs_cmpstr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. fs_cmpstr equal */
    test_sub("subtest %d: fs_cmpstr equal", ++subnum);
    {
        fs s = fscopy("hello");
        int res = fs_cmpstr(&s, "hello");
        test_validatefree(res == 0, fsfree(s), "expected 0, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 2. fs_cmpstr less */
    test_sub("subtest %d: fs_cmpstr less", ++subnum);
    {
        fs s = fscopy("abc");
        int res = fs_cmpstr(&s, "abd");
        test_validatefree(res < 0, fsfree(s), "expected negative, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 3. fs_cmpstr greater */
    test_sub("subtest %d: fs_cmpstr greater", ++subnum);
    {
        fs s = fscopy("xyz");
        int res = fs_cmpstr(&s, "abc");
        test_validatefree(res > 0, fsfree(s), "expected positive, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 4. fs_cmpstr with empty string */
    test_sub("subtest %d: fs_cmpstr empty", ++subnum);
    {
        fs s = fscopy("");
        int res1 = fs_cmpstr(&s, "");
        int res2 = fs_cmpstr(&s, "a");
        test_validatefree(res1 == 0, fsfree(s), "expected 0 for \"\", got %d", res1);
        test_validatefree(res2 < 0, fsfree(s), "expected negative for empty vs \"a\", got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 5. fs_ncmpstr limited equal (within limit) */
    test_sub("subtest %d: fs_ncmpstr limited equal", ++subnum);
    {
        fs s = fscopy("hello");
        int res = fs_ncmpstr(&s, "help", 3);
        test_validatefree(res == 0, fsfree(s), "expected 0 for first 3 chars, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 6. fs_ncmpstr limited difference within limit */
    test_sub("subtest %d: fs_ncmpstr limited diff within", ++subnum);
    {
        fs s = fscopy("hello");
        int res = fs_ncmpstr(&s, "help", 4);
        test_validatefree(res < 0, fsfree(s), "expected negative, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 7. fs_ncmpstr limited with different length strings */
    test_sub("subtest %d: fs_ncmpstr limited different lengths", ++subnum);
    {
        fs s = fscopy("abc");
        int res1 = fs_ncmpstr(&s, "abcdef", 3);  // first 3 equal
        int res2 = fs_ncmpstr(&s, "abcd", 4);    // 4th char '\0' vs 'd' -> negative
        test_validatefree(res1 == 0, fsfree(s), "first 3 chars equal, expected 0, got %d", res1);
        test_validatefree(res2 < 0, fsfree(s), "expected negative, got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 8. fs_ncmpstr with empty string */
    test_sub("subtest %d: fs_ncmpstr empty fs", ++subnum);
    {
        fs s = fscopy("");
        int res1 = fs_ncmpstr(&s, "", 1);
        int res2 = fs_ncmpstr(&s, "a", 1);
        test_validatefree(res1 == 0, fsfree(s), "expected 0, got %d", res1);
        test_validatefree(res2 < 0, fsfree(s), "expected negative, got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 9. fscmpstr (local) equal */
    test_sub("subtest %d: fscmpstr equal (local)", ++subnum);
    {
        fs s = fscopy("hello");
        int res = fscmpstr(s, "hello");
        test_validatefree(res == 0, fsfree(s), "expected 0, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 10. fsncmpstr (local) limited */
    test_sub("subtest %d: fsncmpstr limited (local)", ++subnum);
    {
        fs s = fscopy("hello");
        int res1 = fsncmpstr(s, "help", 3);
        int res2 = fsncmpstr(s, "help", 4);
        test_validatefree(res1 == 0, fsfree(s), "first 3 equal, expected 0, got %d", res1);
        test_validatefree(res2 < 0, fsfree(s), "expected negative, got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

    return TEST_PASSED;
}

// ------------------------- TEST fs_icmpstr / fs_nicmpstr -------------------------
static TestStatus
tf38_fs_icmpstr(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. fs_icmpstr equal (case-insensitive) */
    test_sub("subtest %d: fs_icmpstr equal", ++subnum);
    {
        fs s = fscopy("HelloWorld");
        int res = fs_icmpstr(&s, "helloworld");
        test_validatefree(res == 0, fsfree(s), "expected 0, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 2. fs_icmpstr less (case-insensitive) */
    test_sub("subtest %d: fs_icmpstr less", ++subnum);
    {
        fs s = fscopy("abc");
        int res = fs_icmpstr(&s, "ABCd");   // 'a' vs 'a', 'b' vs 'b', 'c' vs 'c', '\0' vs 'd'
        test_validatefree(res < 0, fsfree(s), "expected negative, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 3. fs_icmpstr greater (case-insensitive) */
    test_sub("subtest %d: fs_icmpstr greater", ++subnum);
    {
        fs s = fscopy("xyz");
        int res = fs_icmpstr(&s, "ABC");
        test_validatefree(res > 0, fsfree(s), "expected positive, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 4. fs_icmpstr with empty string */
    test_sub("subtest %d: fs_icmpstr empty", ++subnum);
    {
        fs s = fscopy("");
        int res1 = fs_icmpstr(&s, "");
        int res2 = fs_icmpstr(&s, "A");
        test_validatefree(res1 == 0, fsfree(s), "expected 0 for \"\", got %d", res1);
        test_validatefree(res2 < 0, fsfree(s), "expected negative for empty vs \"A\", got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 5. fs_nicmpstr limited equal (case-insensitive) */
    test_sub("subtest %d: fs_nicmpstr limited equal", ++subnum);
    {
        fs s = fscopy("Hello");
        int res = fs_nicmpstr(&s, "hElLo", 5);
        test_validatefree(res == 0, fsfree(s), "expected 0, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 6. fs_nicmpstr limited difference within limit */
    test_sub("subtest %d: fs_nicmpstr limited diff", ++subnum);
    {
        fs s = fscopy("Hello");
        int res = fs_nicmpstr(&s, "help", 3);   // 'h','e','l' equal, limit 3
        test_validatefree(res == 0, fsfree(s), "expected 0 for first 3 chars, got %d", res);

        // now compare 4 chars: 'h','e','l','l' vs 'h','e','l','p' -> 'l' (108) vs 'p' (112) => negative
        int res2 = fs_nicmpstr(&s, "help", 4);
        test_validatefree(res2 < 0, fsfree(s), "expected negative, got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 7. fs_nicmpstr limited different lengths */
    test_sub("subtest %d: fs_nicmpstr limited different lengths", ++subnum);
    {
        fs s = fscopy("abc");
        int res1 = fs_nicmpstr(&s, "ABCdef", 3);
        int res2 = fs_nicmpstr(&s, "ABCD", 4);    // '\0' vs 'D' (case-insensitive '\0' < 'D')
        test_validatefree(res1 == 0, fsfree(s), "first 3 equal, expected 0, got %d", res1);
        test_validatefree(res2 < 0, fsfree(s), "expected negative, got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 8. fs_nicmpstr with empty string */
    test_sub("subtest %d: fs_nicmpstr empty fs", ++subnum);
    {
        fs s = fscopy("");
        int res1 = fs_nicmpstr(&s, "", 1);
        int res2 = fs_nicmpstr(&s, "A", 1);
        test_validatefree(res1 == 0, fsfree(s), "expected 0, got %d", res1);
        test_validatefree(res2 < 0, fsfree(s), "expected negative, got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

        /* 9. fsicmpstr (local) case-insensitive equal */
    test_sub("subtest %d: fsicmpstr equal (local)", ++subnum);
    {
        fs s = fscopy("HeLLo");
        int res = fsicmpstr(s, "hello");
        test_validatefree(res == 0, fsfree(s), "expected 0, got %d", res);
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 10. fsnicmpstr (local) limited case-insensitive */
    test_sub("subtest %d: fsnicmpstr limited (local)", ++subnum);
    {
        fs s = fscopy("HelloWorld");
        int res1 = fsnicmpstr(s, "HELLO", 5);
        int res2 = fsnicmpstr(s, "HELLO", 6);   // 'W' vs '\0' → positive
        test_validatefree(res1 == 0, fsfree(s), "first 5 equal, expected 0, got %d", res1);
        test_validatefree(res2 > 0, fsfree(s), "expected positive, got %d", res2);
        fsfree(s);
        fs_alloc_check(true);
    }

    return TEST_PASSED;
}

// ------------------------- TEST fs_sprintf_position -------------------------
static TestStatus
tf39_fs_sprintf_position(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Запись в пустой буфер с pos=0 */
    test_sub("subtest %d: write into empty buffer at pos 0", ++subnum);
    {
        fs s = fsinit(16);
        long cnt = fs_sprintf_position(&s, 0, "hello %d", 42);
        test_validatefree(cnt == 8, fsfree(s), "expected 8, got %ld", cnt);
        test_validatefree(strcmp(fs_str(&s), "hello 42") == 0, fsfree(s),
                          "expected 'hello 42', got '%s'", fs_str(&s));
        test_validatefree(fs_len(&s) == 8, fsfree(s), "expected len 8, got %zu", fs_len(&s));
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 2. Дописывание в конец (pos = len) */
    test_sub("subtest %d: append at end", ++subnum);
    {
        fs s = fsinit(16);
        long cnt = fs_sprintf_position(&s, 0, "abc");
        cnt = fs_sprintf_position(&s, cnt, "def");
        test_validatefree(cnt == 3, fsfree(s), "expected 3, got %ld", cnt);
        test_validatefree(strcmp(fs_str(&s), "abcdef") == 0, fsfree(s),
                          "expected 'abcdef', got '%s'", fs_str(&s));
        test_validatefree(
            fs_len(&s) == 6, 
            fsfree(s), 
            "expected len 6, got %zu", fs_len(&s)
        );
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 3. Перезапись в середине (pos < len) */
    test_sub("subtest %d: overwrite at middle truncates", ++subnum);
    {
        fs s = fsinit(16);
        fs_sprintf_position(&s, 0, "hello");
        long cnt = fs_sprintf_position(&s, 1, "A");
        test_validatefree(cnt == 1, fsfree(s), "expected 1, got %ld", cnt);
        test_validatefree(strcmp(fs_str(&s), "hA") == 0, fsfree(s),
                        "expected 'hA', got '%s'", fs_str(&s));
        test_validatefree(fs_len(&s) == 2, fsfree(s), "expected len 2, got %zu", fs_len(&s));
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 4. Расширение буфера при нехватке места */
    test_sub("subtest %d: buffer expands when needed", ++subnum);
    {
        fs s = fsinit(4);
        long cnt = fs_sprintf_position(&s, 0, "long text %d", 123);
        test_validatefree(cnt == 13, fsfree(s), "expected 13, got %ld", cnt);
        test_validatefree(fs_sz(&s) >= 14, fsfree(s), "expected sz >= 14, got%zu", fs_sz(&s));
        test_validatefree(strcmp(fs_str(&s), "long text 123") == 0, fsfree(s),
                          "expected 'long text 123', got '%s'", fs_str(&s));
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 5. Несколько последовательных записей с разными позициями */
    test_sub("subtest %d: multiple writes", ++subnum);
    {
        fs s = fsinit(16);
        fs_sprintf_position(&s, 0, "one");
        fs_sprintf_position(&s, 3, "-two");
        fs_sprintf_position(&s, 7, "-three");
        test_validatefree(strcmp(fs_str(&s), "one-two-three") == 0, fsfree(s),
                          "expected 'one-two-three', got '%s'", fs_str(&s));
        test_validatefree(fs_len(&s) == 13, fsfree(s), "expected len 13, got %zu", fs_len(&s));
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 1. Пустой буфер (FS()) и непустое сообщение */
    test_sub("subtest %d: empty buffer with non-empty message", ++subnum);
    {
        fs s = FS();   // sz=0
        long cnt = fs_sprintf_position(&s, 0, "hello");
        test_validatefree(cnt == 5, fsfree(s), "expected 5, got %ld", cnt);
        test_validatefree(strcmp(fs_str(&s), "hello") == 0, fsfree(s),
                          "expected 'hello', got '%s'", fs_str(&s));
        test_validatefree(fs_sz(&s) >= 6, fsfree(s), "expected sz >= 6, got %zu", fs_sz(&s));
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: write into FS() at pos 0", ++subnum);
    {
        fs s = FS();
        int     pos = 100;
        long cnt = fs_sprintf_position(&s, pos, "hello %d", 42);
        test_validatefree(cnt == 8, fsfree(s), "expected 8, got %ld", cnt);
        test_validatefree(strcmp(fs_str(&s) + pos, "hello 42") == 0, fsfree(s),
                          "expected 'hello 42' at pos %d, got '%s'", pos, fs_str(&s));
        // no validation here
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 2. Пустое сообщение (""), пустой буфер */
    test_sub("subtest %d: empty message", ++subnum);
    {
        fs s = FS();
        long cnt = fs_sprintf_position(&s, 0, "");
        test_validatefree(cnt == 0, fsfree(s), "expected 0, got %ld", cnt);
        test_validatefree(fs_str(&s) != NULL && fs_str(&s)[0] == '\0', fsfree(s),
                          "expected empty string");
        test_validatefree(fs_len(&s) == 0, fsfree(s), "expected len 0, got %zu", fs_len(&s));
        test_validatefree(fs_sz(&s) >= 1, fsfree(s), "expected sz >= 1, got %zu", fs_sz(&s));
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 3. Запись за пределами текущей длины (pos > len) */
    test_sub("subtest %d: write beyond current length", ++subnum);
    {
        fs s = fsinit(16);
        fs_sprintf_position(&s, 0, "a");       // len=1
        long cnt = fs_sprintf_position(&s, 5, "b");
        test_validatefree(cnt == 1, fsfree(s), "expected 1, got %ld", cnt);
        test_validatefree(fs_len(&s) == 6, fsfree(s), "expected len 6, got %zu", fs_len(&s));
        // содержимое между 1 и 4 может быть мусором, поэтому проверяем только конец
        test_validatefree(fs_str(&s)[5] == 'b', fsfree(s), "expected 'b' at pos 5");
        // no validation here! Writing out a bound!
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 9. Расширение при точной границе pos == sz-1 */
    test_sub("subtest %d: buffer expansion at exact boundary", ++subnum);
    {
        fs s = fsinit(4);   // sz станет 8 после округления
        long cnt1 = fs_sprintf_position(&s, 0, "abc");          // len=3
        long cnt2 = fs_sprintf_position(&s, 3, "defghijklmnop"); // 13 символов
        test_validatefree(cnt1 == 3, fsfree(s), "expected 3, got %ld", cnt1);
        test_validatefree(cnt2 == 13, fsfree(s), "expected 13, got %ld", cnt2);
        test_validatefree(strcmp(fs_str(&s), "abcdefghijklmnop") == 0, fsfree(s),
                        "expected 'abcdefghijklmnop', got '%s'", fs_str(&s));
        test_validatefree(fs_len(&s) == 16, fsfree(s), "expected len 16, got %zu", fs_len(&s));
        test_validatefree(fs_sz(&s) >= 17, fsfree(s), "expected sz >= 17, got %zu", fs_sz(&s));
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 5. Многократное дописывание с расширением */
    test_sub("subtest %d: multiple appends with expansions", ++subnum);
    {
        fs s = fsinit(2);
        fs_sprintf_position(&s, 0, "1");
        fs_sprintf_position(&s, 1, "2");
        fs_sprintf_position(&s, 2, "3");
        fs_sprintf_position(&s, 3, "4");
        test_validatefree(strcmp(fs_str(&s), "1234") == 0, fsfree(s),
                          "expected '1234', got '%s'", fs_str(&s));
        test_validatefree(fs_len(&s) == 4, fsfree(s), "expected len 4, got %zu", fs_len(&s));
        test_validatefree(
            fs_validate(logfile, &s),
            fsfree(s), 
            "Validation failed"
        );
        fsfree(s);
        fs_alloc_check(true);
    }

    return TEST_PASSED;
}

// ------------------------- TEST fs_catstr / fs_catmem -------------------------
static TestStatus
tf40_fs_catstr_mem(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ================= fs_catstr ================= */

    /* 1. Конкатенация в пустой fs */
    test_sub("subtest %d: catstr into empty fs", ++subnum);
    {
        fs s = FS();
        fs_catstr(&s, "hello");
        test_validatefree(fs_len(&s) == 5, fsfree(s),
                          "len expected 5, got %zu", fs_len(&s));
        test_validatefree(strcmp(fs_str(&s), "hello") == 0, fsfree(s),
                          "content mismatch: '%s'", fs_str(&s));
        test_validatefree(fs_sz(&s) >= 6, fsfree(s),
                          "sz expected >= 6, got %zu", fs_sz(&s));
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 2. Конкатенация к непустой строке */
    test_sub("subtest %d: catstr append", ++subnum);
    {
        fs s = fscopy("Hello ");
        fs_catstr(&s, "world");
        test_validatefree(strcmp(fs_str(&s), "Hello world") == 0, fsfree(s),
                          "content mismatch: '%s'", fs_str(&s));
        test_validatefree(fs_len(&s) == 11, fsfree(s),
                          "len expected 11, got %zu", fs_len(&s));
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 3. Конкатенация пустой строки */
    test_sub("subtest %d: catstr empty source", ++subnum);
    {
        fs s = fscopy("abc");
        fs_catstr(&s, "");
        test_validatefree(strcmp(fs_str(&s), "abc") == 0, fsfree(s),
                          "content must stay 'abc'");
        test_validatefree(fs_len(&s) == 3, fsfree(s),
                          "len expected 3, got %zu", fs_len(&s));
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 4. Расширение буфера */
    test_sub("subtest %d: catstr buffer expansion", ++subnum);
    {
        fs s = fsinit(4);   // небольшой буфер
        fs_catstr(&s, "long text");
        test_validatefree(strcmp(fs_str(&s), "long text") == 0, fsfree(s),
                          "content mismatch: '%s'", fs_str(&s));
        test_validatefree(fs_sz(&s) >= 10, fsfree(s),
                          "sz expected >= 10, got %zu", fs_sz(&s));
        fsfree(s);
        fs_alloc_check(true);
    }

    /* ================= fs_catmem ================= */

    /* 5. Конкатенация фрагмента заданной длины */
    test_sub("subtest %d: catmem append fixed size", ++subnum);
    {
        fs s = fscopy("abc");
        fs_catmem(&s, "defgh", 3);   // добавит только "def"
        test_validatefree(strcmp(fs_str(&s), "abcdef") == 0, fsfree(s),
                          "content mismatch: '%s'", fs_str(&s));
        test_validatefree(fs_len(&s) == 6, fsfree(s),
                          "len expected 6, got %zu", fs_len(&s));
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 6. Конкатенация фрагмента с нулевым символом внутри */
    test_sub("subtest %d: catmem with embedded null", ++subnum);
    {
        fs s = fscopy("abc");
        const char data[] = {'d', '\0', 'e'};
        fs_catmem(&s, data, sizeof(data) - 1);
        test_validatefree(fs_len(&s) == 5, fsfree(s),
                          "len expected 5, got %zu", fs_len(&s));
        // Проверяем, что первые символы корректны, включая встроенный '\0'
        test_validatefree(memcmp(fs_str(&s), "abc", 3) == 0, fsfree(s),
                          "prefix mismatch");
        test_validatefree(fs_str(&s)[3] == 'd', fsfree(s),
                          "byte at pos 3 must be 'd'");
        test_validatefree(fs_str(&s)[4] == '\0', fsfree(s),
                          "byte at pos 4 must be '\\0'");
        test_validatefree(fs_str(&s)[5] == '\0', fsfree(s),
                          "byte at pos 5 must be '\\0'");
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 7. size = 0 */
    test_sub("subtest %d: catmem zero size", ++subnum);
    {
        fs s = fscopy("abc");
        fs_catmem(&s, "xyz", 0);
        test_validatefree(strcmp(fs_str(&s), "abc") == 0, fsfree(s),
                          "content must stay 'abc'");
        test_validatefree(fs_len(&s) == 3, fsfree(s),
                          "len expected 3, got %zu", fs_len(&s));
        fsfree(s);
        fs_alloc_check(true);
    }

    /* 8. Расширение буфера для catmem */
    test_sub("subtest %d: catmem buffer expansion", ++subnum);
    {
        fs s = fsinit(2);
        fs_catmem(&s, "abcdef", 6);
        test_validatefree(fs_len(&s) == 6, fsfree(s),
                          "len expected 6, got %zu", fs_len(&s));
        test_validatefree(memcmp(fs_str(&s), "abcdef", 6) == 0, fsfree(s),
                          "content mismatch");
        fsfree(s);
        fs_alloc_check(true);
    }

    return TEST_PASSED;
}

// ------------------------- TEST fs_moveto -------------------------
static TestStatus
tf41_fs_moveto(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Перемещение из ALLOC в пустой ALLOC */
    test_sub("subtest %d: move ALLOC to empty ALLOC", ++subnum);
    {
        fs          src = fscopy("hello");
        fs          dst = FS();
        const char *orig_v = src.v;
        int         orig_flags = src.flags;

        fs *result = fs_moveto(&dst, &src);
        test_validatefree(result == &dst,
                          (fsfree(src), fsfree(dst)),
                          "returned pointer mismatch");
        test_validatefree(dst.v == orig_v && dst.flags == orig_flags,
                          (fsfree(src), fsfree(dst)),
                          "dst should inherit src pointer and flags");
        test_validatefree(fscmp(dst, FSLITERAL("hello")) == 0,
                          (fsfree(src), fsfree(dst)),
                          "dst content mismatch: '%s'", fs_str(&dst));
        test_validatefree(fs_defunct(&src),
                          (fsfree(src), fsfree(dst)),
                          "src must be defunct after move");

        fsfree(dst);   // src defunct, fsfree(src) безопасен
        fs_alloc_check(true);
    }

    /* 2. Перемещение из STATIC в ALLOC */
    test_sub("subtest %d: move STATIC to ALLOC", ++subnum);
    {
        fs          src = FSLITERAL("static");
        fs          dst = FS();
        const char *orig_v = src.v;
        int         orig_flags = src.flags;

        fs_moveto(&dst, &src);
        test_validatefree(dst.v == orig_v && dst.flags == orig_flags,
                          (fsfree(dst)),
                          "dst should inherit src flags and pointer");
        test_validatefree(fs_defunct(&src),
                          (fsfree(dst)),
                          "src must be defunct");
        fsfree(dst);   // dst статический, fsfree не должен ничего делать
        fs_alloc_check(true);
    }

    /* 3. Перемещение из LOCAL в ALLOC */
    test_sub("subtest %d: move LOCAL to ALLOC", ++subnum);
    {
        fslocal(localbuf, 16);
        fs          src = localbuf;
        fs_cpystr(&src, "local data");
        fs          dst = FS();
        const char *orig_v = src.v;
        int         orig_flags = src.flags;

        fs_moveto(&dst, &src);
        test_validatefree(dst.v == orig_v && dst.flags == orig_flags,
                          (fsfree(dst)),
                          "dst should inherit src flags and pointer");
        test_validatefree(fs_defunct(&src),
                          (fsfree(dst)),
                          "src must be defunct");
        fsfree(dst);   // dst локальный, fsfree не должен ничего делать
        fs_alloc_check(true);
    }

    /* 4. Перемещение dst == src (само в себя) */
    test_sub("subtest %d: move to itself", ++subnum);
    {
        fs      s = fscopy("self");
        size_t  len_before = s.len;
        char   *v_before = s.v;

        fs *result = fs_moveto(&s, &s);
        test_validatefree(result == &s,
                          (fsfree(s)),
                          "returned pointer mismatch");
        test_validatefree(s.len == len_before && s.v == v_before,
                          (fsfree(s)),
                          "self-move should not change object");

        fsfree(s);
        fs_alloc_check(true);
    }

    /* 5. Перемещение из DEFUNCT (уже перемещённый) */
    test_sub("subtest %d: move from DEFUNCT", ++subnum);
    {
        fs src = fscopy("original");
        fs dst = FS();
        fs_moveto(&dst, &src);   // теперь src defunct

        fs dst2 = FS();
        fs_moveto(&dst2, &src);  // src defunct, dst2 станет defunct
        test_validatefree(fs_defunct(&dst2),
                          (fsfree(dst), fsfree(dst2)),
                          "dst2 should become defunct");
        test_validatefree(fs_defunct(&src),
                          (fsfree(dst), fsfree(dst2)),
                          "src must still be defunct");

        fsfree(dst);
        fsfree(dst2);
        fs_alloc_check(true);
    }

        /* 6a. NULL dst */
    test_sub("subtest %d: NULL dst raises error", ++subnum);
    {
        fs src = fscopy("test");
        if (!try()) {
            fs_moveto(NULL, &src);
            fsfree(src);   // освобождаем, так как функция не освободила
            test_validate(false, "must raise error for NULL dst");
        } else {
            fsfree(src);
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

    /* 6b. NULL src */
    test_sub("subtest %d: NULL src raises error", ++subnum);
    {
        fs dst = FS();
        if (!try()) {
            fs_moveto(&dst, NULL);
            fsfree(dst);
            test_validate(false, "must raise error for NULL src");
        } else {
            fsfree(dst);
            test_validate(true, "correctly raised error");
        }
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}
// ------------------------------------------------------------------------------------------------------------------------------
int
main( /* int argc, const char *argv[] */)
{
    logsimpleinit("Start");

    testenginestd(
        TESTADD(tf1,                    "Simple init and validate test"),
        TESTADD(tf2,                    "Access read/write test"),
        TESTADD(tf3,                    "Elem() test"),
        // 
        TESTADD(tf_fs_clone,            "fs_clone() simple test"),
        TESTADD(tf4,                    "fs_cat/fs_catstr test"),
        TESTADD(tf5,                    "fs_cpy/fs_cpystr test"),
        TESTADD(tf6,                    "fsfreeall test"),
        TESTADD(tf7,                    "fsprint/printlim manual test"),
        TESTADD(tf8,                    "fsprint_arr manual test"),
        TESTADD(tf9,                    "fs_sprintf formatted test"),
        TESTADD(tf10,                   "fslocal simple test"),
        TESTADD(tf11,                   "fs_save/load test"),
        TESTADD(tf12,                   "fs_free_alloc_checker test"),
        TESTADD(tf13,                   "fs_move simple test"),
        TESTADD(tf14,                   "fs_substr/newsubstr simple test"),
        TESTADD(tf15,                   "fs_ifnotin/fs_ifinotin simple test"),
        TESTADD(tf16,                   "fs_instr/fs_iinstr simple test"),
        TESTADD(tf17,                   "fs_(n)(i)chr simple tests"),
        TESTADD(tf18,                   "fs_(i)rchr simple tests"),
        TESTADD(tf19,                   "fs_n(i)instr simple tests"),
        TESTADD(tf20,                   "fs_rev_catstr simple tests"),
        TESTADD(tf21,                   "fs_str simple tests"),
        TESTADD(tf22,                   "fs_get<int/long/double>pos simple tests"),
        TESTADD(tf23,                   "fs_cmp_strict simple tests"),
        TESTADD(tf24,                   "fs_moveto_heap simple tests"),
        TESTADD(tf25,                   "fs_fscanf simple tests"),
        TESTADD(tf26,                   "fscopyf simple tests"),
        TESTADD(tf27,                   "fs_rpad()/lpad() simple tests"),
        TESTADD(tf28,                   "fs_heapcreate() simple tests"),
        TESTADD(tf_fs_heapcopy,         "fs_heapcopy() simple tests"),
        TESTADD(tf_moveto_heapstr,      "fs_moveto_heapstr() simple tests"),
        TESTADD(tf31_fsmovetostr,       "fsmovetostr macro simple tests"),
        TESTADD(tf_genrnd,              "fs_genrnd() simple tests"),
        TESTADD(tf_initrnd,             "fs_initrnd() simple tests"),
        TESTADD(tf34_fs_isnull,         "fs_isnull() simple tests"),
        TESTADD(tf35_fs_isempty,        "fs_isempty() simple tests"),
        TESTADD(tf36_fs_movetostr,      "fs_movetostr() simple tests"),
        TESTADD(tf37_fs_cmpstr,         "fs_cmpstr() series simple tests"),
        TESTADD(tf38_fs_icmpstr,        "fs_icmpstr()/fs_nicmpstr()  simple tests"),
        TESTADD(tf39_fs_sprintf_position,   "fs_sprintf_position() simple tests"),
        TESTADD(tf40_fs_catstr_mem,         "fs_catstr / fs_catmem() simple tests"),
        TESTADD(tf41_fs_moveto,             "fs_moveto() simple tests")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* FSTESTING */

