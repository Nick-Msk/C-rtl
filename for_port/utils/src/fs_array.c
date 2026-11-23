#include <stdlib.h>

#include "common.h"
#include "log.h"
#include "fs.h"
#include "error.h"
#include "fs_array.h"
#include "fileutils.h"

/********************************************************************
                    FAST STRING ARRAY MODULE IMPLEMENTATION
********************************************************************/

// mem leak checking
static int                      g_fsarr_alloc_cnt             = 0;
static int                      g_fsarr_free_cnt              = 0;

// ---------- pseudo-header for utility procedures -----------------

// -------------------------- (Utility) printers --------------------

int                             fsarr_techfprintlim(FILE *restrict f, const fsarray *restrict arr, int lim){
    int cnt = 0;
    if (arr){
        cnt += fprintf(f, "FSARRAY: sz %d, cnt %d, ptr %p:\n", arr->sz, arr->cnt, arr->ar);
        if (arr->ar)
            for (int i = 0; i < MIN(arr->cnt, lim); i++){
                cnt += fstechfprint(f, arr->ar[i]);   // directly name!
            }
        if (lim < arr->cnt)
            cnt += fprintf(f, "and more... (%d)\n", arr->cnt - lim);
    }
    return cnt;
}

int                             fsarr_techfprint(FILE *restrict f, const fsarray *restrict arr){
    return fsarr_techfprintlim(f, arr, arr->sz);
}

// ------------------------------ Utilities -------------------------

static int                      fsarr_mass_init(fsarray *arr, int min, int max){
    int cnt = 0;
    for (int i = min; i < max; i++)
        arr->ar[i] = FS(), cnt++;  // with alloc flag
    return logsimpleret(cnt, "Initialixed %d", cnt);
}

static int                      fsarr_mass_free(fsarray *arr, int min, int  max){
    int cnt = 0;
    for (int i = max - 1; i >= min; i--)
        if (!fsisnull(arr->ar[i]) )
            fsfree(arr->ar[i]), cnt++;
    return logsimpleret(cnt, "Freed %d", cnt);
}

static int                      fsarr_increasesize(fsarray *fa, int newsz, bool init){
    logenter("oldsz %d, newsz %d init %s", fa->sz, newsz, bool_str(init));
    if (init)   // from fsarr_init
        newsz = calcnewsize(SIZE_POWER2, newsz);
    if (newsz > fa->sz || !init) {   // if exec from faarr_increate() then resize anyway
        fs *tmp = realloc(fa->ar, newsz * sizeof (fs) );
        if (!tmp) {
            return userraise(-1, ERR_UNABLE_ALLOCATE, "Unable to allocate %lu bytes", newsz * sizeof (fs) );  // what about LG_LV here??/ TODO:
        }
        if (fa->ar == 0)
            logauto(++g_fsarr_alloc_cnt);
        fa->sz = newsz;
        fa->ar = tmp;
    }
    return logret(newsz, "Increased to %d elements", newsz);
}

// compare as pointer
static int                      fsptrcmp(const void *ns1, const void *ns2){
    return ((fs *) ns1)->v - ((fs *) ns2)->v; // OMG
}

// sorting via qsort()
static fsarray                  sortfs(const fsarray *origin){
    // make a copy first
    fsarray tmp = FSARRAY();
    fsarr_increasesize(&tmp, origin->cnt, false);  // exactly count of origin
    int     cnt = 0;
    for (int i = 0; i < origin->cnt; i++)
        if ( !fsisnull(origin->ar[i] ) )
            tmp.ar[cnt++] = origin->ar[i];    // all namedfs, not reeally need actually
    // sort
    tmp.sz = cnt;
    if (tmp.sz > 0)
        qsort(tmp.ar, tmp.sz, sizeof(fs), fsptrcmp);
    return tmp;
}


// --------------------------- API ---------------------------------
// ------------------ General functions ----------------------------

#define             FS_VALIDATE_FAILED(out, msg, ...) {\
                    if (out)\
                        fprintf( (out), "%s: ", __func__), fprintf( (out), (msg), ##__VA_ARGS__), fputc('\n', (out) );\
                    return logerr(false, (msg), ##__VA_ARGS__); }

// never raise exception! just return true/false and put messages into f (if != 0)
bool                fsarr_validate(FILE *restrict out, const fsarray *restrict arr){
    logenter("%p", out);
    logmsg("check 1");
    if (arr->sz < 0 || arr->cnt < 0 || arr->cnt > arr->sz){
        FS_VALIDATE_FAILED(out, "sz(%d) and cnt(%d) must be positive, and sz >= cnt", arr->sz, arr->cnt);
    }
    if (arr->cnt > 0){
        logmsg("cnt > 0, check 2");
        fsarray   tmp = sortfs(arr);   // make a sorting by pointer arr.ar[].v, tmp is shrinked and NOT valid, all fs are pointer to fs of arr
        int     duplcount = 0;
        for (int i = 1; i < tmp.sz; i++)
            if (tmp.ar[i - 1].v == tmp.ar[i].v){
                if (out)
                    fprintf(out, "elem [%d] == elem [%d] == %p:[%s]", i - 1, i, tmp.ar[i].v, tmp.ar[i].v);
                duplcount++;
            }
        free(tmp.ar);       // free temporary array (not fs!)
        if (duplcount > 0)
            FS_VALIDATE_FAILED(out, "Total duplicates %d", duplcount);
    }
    logmsg("all tests for now...");
    return logret(true, "Ok");
}

// internal, for testing
static bool                             fsarr_check_leak(bool raise){
    int     f = g_fsarr_free_cnt, a = g_fsarr_alloc_cnt;
    if (f != a){
        if (raise)
            userraiseint(WARN_MEM_LEAK_DETECTED, "allocaed %d, freed %d", a, f);
        else
            userraise(false, WARN_MEM_LEAK_DETECTED, "WARNING: allocaed %d, freed %d", a, f);
    }
    return f == a;
}

// just a wrapper for check_leak
bool                                    fsarr_alloc_check(bool raise){
    return fsarr_check_leak(raise);
}



// -------------------- ACCESS AND MODIFICATORS ------------------------

// TODO: via fsl ( struct fsl { int loc; }; 
// fsl f1 = getfsl(fs_array a);
// f1.elem() = ... 
// f1.get() = ...

int                         fsarr_increase(fsarray *arr, int newcnt){
    logenter("newsize %d", newcnt);
    if (newcnt > arr->sz)
        if (fsarr_increasesize(arr, newcnt, true) < 0)
            return userraise(-1, ERR_UNABLE_ALLOCATE, "New cnt %d", newcnt);
    fsarr_mass_init(arr, arr->cnt, newcnt);
    arr->cnt = newcnt;
    return logret(arr->sz, "New sz %d, cnt %d", arr->sz, arr->cnt);
}

int                         fsarr_shrink(fsarray *arr, int newcnt){
    if (newcnt < arr->sz && newcnt >= 0){
        if (newcnt < arr->cnt)
            fsarr_mass_free(arr, newcnt, arr->cnt), arr->cnt = newcnt; // from from newcnt to current cnt
        if (fsarr_increasesize(arr, newcnt, false) < 0)
            return userraise(-1, ERR_UNABLE_ALLOCATE, "Shrinked cnt %d", newcnt);
    }
    return logsimpleret(arr->sz, "Shrinked to %d, new cnt %d", arr->sz, arr->cnt);
}

// empty or free allocared fs
extern int                  fsarr_clean(fsarray *arr, bool free){
    invraise(arr != 0, "Null pointer");

    int     cnt = 0;
    // TODO: iterator must be here
    for (int i = 0; i < arr->cnt; i++){
        if (!fsisnull(arr->ar[i]) ){
            if (free)
                fsfree(arr->ar[i]);
            else
                fsclear(arr->ar[i]);
            cnt++;
        }
    }
    if (free)
        arr->cnt = 0;   // reset counter
    return logsimpleret(cnt, "Cleaned %d free ? %s", cnt, bool_str(free) );
}

// ---------------------------------------- Comparator -------------------------------------------

bool                        fsarr_cmp(const fsarray *restrict arr1, const fsarray *restrict arr2, int *restrict pos){

    bool    res = true;
    int     i;
    for (i = 0; i < MIN(fsarr_cnt(arr1), fsarr_cnt(arr2) ); i++)
        if (fscmp(arr1->ar[i], arr2->ar[i]) != 0){
            res = false;
            logsimple("diff on %d", i);
            break;
        }

    if (res)
        res = fsarr_cnt(arr1) == fsarr_cnt(arr2);
    if (!res && pos)
        *pos = i;
    return logsimpleret(res, "Compared %s: %d", bool_str(res), i);
}

// -------------------------- (API) printers -----------------------

// this is normal printer
int                         fsarr_fprint(FILE *restrict out, const fsarray *restrict arr){
    int  cnt = 0;
    if (out)
        for (int i = 0; i < arr->cnt; i++){
            if ( !fsisnull(arr->ar[i] ) ) {
                /*fsfprint(f, arr->ar[i]);   // directly name!
                cnt++;
                fputc('\n', f);*/
                fprintf(out, "%s\n", fsstr(arr->ar[i]) );
            }
        }
    return cnt;     // total printed items
}



// -------------------------- (API) Serialization  -----------------------
int                         fsarr_save(const char *restrict fname, const fsarray *restrict arr){
    FILE    *out = fopen(fname, "w");
    if (!out)
        return userraise(ERR_UNABLE_OPEN_FILE_READ, -1, "Unable to open %s for write", fname);
    int     cnt = fsarr_fsave(out, arr);
    fclose(out);
    return cnt;
}

// f is open for write
int                         fsarr_fsave(FILE *restrict f, const fsarray *restrict arr){
    // signature
    fprintf(f, "FSARRAY(%d):[\n", arr->cnt);
    if (!arr->ar)
        return userraise(ERR_NULLABLE_PTR, -1, "Nullable ar, can't serialize"); 
    for (int i = 0; i < arr->cnt; i++){
        fprintf(f, "%4d:", i);
        fs_fsave(f, fsarr_get(arr, i) );
        //fputc('\n', f);  not sure
    }
    fprintf(f, "]");
    return logsimpleret(arr->cnt, "Total saved %d", arr->cnt);
}

fsarray                     fsarr_load(const char *fname){
    FILE    *out = fopen(fname, "r");
    if (!out)
        userraiseint(ERR_UNABLE_OPEN_FILE_READ, "Unable to open %s for read", fname);
    fsarray fa = fsarr_fload(out);
    fclose(out);
    return fa;
}

fsarray                     fsarr_fload(FILE *restrict f){
    // format: "FSARRAY: sz %d cnt %d ptr %d:[\n"
    FUSKIPFORMAT_RAISE(f, "FSARRAY(");
    int cnt = FUGETUNSIGNED_RAISE(f);

    FUSKIPFORMAT_RAISE(f, "):[\n");
    fsarray     a = fsarr_init(cnt);
    // cycle for fs
    for (int i = 0; i < cnt; i++){
        FUSKIPFORMATPRINTF_RAISE(f, "%4d:", i);
        a.ar[i] = fs_fload(f, 0);       // TODO: probably to usr fsarr_get()?
    }
    FUSKIPFORMAT_RAISE(f, "]");
    return a;
}

// returns count of saved line
int                         fsarr_fsavelines(FILE *restrict out, const fsarray *restrict arr, int cntline){
    invraise(arr != 0, "Null pointer");
    int     cnt = 0;
    cntline = cntline <= 0 ? arr->cnt : cntline;
    if (out)
        for (int i = 0; i < cntline; i++)
            fprintf(out, "%s\n", fsstr(arr->ar[i] ) ), cnt++;
    return logsimpleret(cnt, "Saved %d", cnt);
}

int                         fsarr_floadlines(FILE *restrict in, fsarray *restrict arr, int maxcnt){
    invraise(arr != 0 && in != 0, "Null pointer");

    int     cnt = 0;
    // TODO: actually fsarrayiter must be here
    if (cnt >= fsarr_cnt(arr) )
        if (fsarr_increase(arr, G_FSARRAY_LOAD_INC + cnt) < 0)
            return userraise(-1, ERR_UNABLE_LOAD_FSARRAY, "Unable to load fsarray");

    if (maxcnt == 0)
        maxcnt = INT_MAX; // omg, unlim
    while (maxcnt-- > 0 && fgetslim_fs(in, arr->ar + cnt) >= 0){
        if (++cnt >= fsarr_cnt(arr) )
            if (fsarr_increase(arr, G_FSARRAY_LOAD_INC + cnt) < 0)
                return userraise(-1, ERR_UNABLE_LOAD_FSARRAY, "Unable to load fsarray");
    }
    // setup count directly
    arr->cnt = cnt;

    return logsimpleret(cnt, "Loaded %d", cnt);
}

// ------------------ API Constructs/Destrucor  ----------------------------

int                 fsarr_free(fsarray *arr){

    logenter("%p: %p, cnt %d", arr, arr ? arr->ar : 0, arr ? arr->cnt : 0);
    int   cnt = 0;
    if (arr){
        cnt = fsarr_mass_free(arr, 0, arr->cnt);
        logauto(++g_fsarr_free_cnt);
    }
    free(arr->ar);
    arr->ar = 0;
    arr->sz = arr->cnt = 0;
    return logret(cnt, "Freed %d", cnt);
}

// initialize
fsarray             fsarr_init(int cnt){
    fsarray ar = FSARRAY();
    fsarr_increasesize(&ar, cnt, true);
    fsarr_mass_init(&ar, 0, ar.cnt = cnt);
    return logsimpleret(ar, "Created with sz %d, cnt %d", ar.sz, ar.cnt);
}

// null-last array, of cnt == 0, then <= cnt elements
fsarray              fsarr_fromarr(const char * const *arr, int cnt){
    const char * const *iter = arr;
    while (*iter && cnt == 0 ? true : iter - arr < cnt)
        iter++;
    cnt = iter - arr;
    logsimple("%d will be allocated", cnt);
    fsarray     tmp = fsarr_init(cnt);
    for (int i = 0; i < cnt; i++)
        tmp.ar[i] = fscopy(arr[i]);    // allocate one by one
    return logsimpleret(tmp, "Created fsarray with %d", cnt);
}

// -------------------------------Testing --------------------------
#ifdef FSARRAYTESTING

#include "test.h"
#include "checker.h"

//types for testing

// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: init", ++subnum);
    {

        int cnt = 50;
        fsarray fa = fsarr_init(cnt);

        fsarr_techfprint(logfile, &fa);
        test_validatefree(fsarr_validate(stderr, &fa), fsarr_free(&fa), "Validation failed");
        //if (!fsarr_validate(stderr, &fa) )
        //    return logacterr(fsarr_free(&fa), TEST_FAILED, "Validation failed");

        if (fa.sz < cnt)
            return logacterr(fsarr_free(&fa), TEST_FAILED, "sz(%d) must be >= initial cnt(%d)", fa.sz, cnt);

        if (fa.cnt != cnt)
            return logacterr(fsarr_free(&fa), TEST_FAILED, "cnt(%d) must be = initial cnt(%d)", fa.cnt, cnt);

        test_sub("subtest %d: freeall", ++subnum);

        fsarr_techfprint(logfile, &fa);
        fsarr_free(&fa);
        fsarr_techfprint(logfile, &fa);
    }
    fs_alloc_check(true);
    return logret(TEST_MANUAL, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 2 ---------------------------------

static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: init + validation", ++subnum);
    {

        int cnt = 30;
        fsarray fa = fsarr_init(cnt);

        test_validatefree(fa.sz >= cnt && fa.cnt == cnt, fsarr_free(&fa), "sz(%d) must be >= initial cnt(%d) and cnt(%d) must be equal to initial", fa.sz, fa.cnt, cnt);

        for (int i = 0; i < cnt; i++){
            if (i < cnt / 2)     // fs_attach to be here?
                fa.ar[i] = fsinit(100);        // 50 with init, 50 - automatic
            // introdured fssprintf(s, format, ...);
            fssprintf(fa.ar[i], "str - %d", i);
        }
        fsarr_techfprint(logfile, &fa);
        test_validate(fsarr_validate(stderr, &fa), "Validation failed");

    test_sub("subtest %d: check lines", ++subnum);

        char buf[100];
        // check the lines
        for (int i = 0; i < cnt; i++){
            snprintf(buf, sizeof(buf) - 1, "str - %d", i);        // ok in this case
            test_validatefree(strcmp(fa.ar[i].v, buf) == 0, fsarr_free(&fa),
                "fsarr[%d]: '%s' != origin '%s'", i, fa.ar[i].v, buf);
        }

    test_sub("subtest %d: freeall", ++subnum);

        fsarrfree(fa); // via macro
        fsarr_techfprint(logfile, &fa);
        test_validate(fsarr_validate(stderr, &fa), "Validation failed after free");
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 3 ---------------------------------

static TestStatus
tf3(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: cycle attach + validation", ++subnum);
    {

        int cnt = 50;
        fsarray fa = fsarr_init(cnt);
        fs      s;

        for (int i = 0; i < cnt; i++){
            s = FS();
            fssprintf(s, "value bla bla %d", i);
            fsarrattach(fa, i, s);
        }
        fsarr_techfprintlim(logfile, &fa, 3);

        test_validate(fsarr_validate(stderr, &fa), "Validation failed");

    test_sub("subtest %d: check array", ++subnum);

        for (int i = 0; i < cnt; i++){
            char buf[100];
            snprintf(buf, sizeof(buf) - 1, "value bla bla %d", i);
            test_validatefree(strcmp(buf, fa.ar[i].v) == 0, fsarrfree(fa), "[%s] must  be equal fs[%d] [%s]", buf, i, fa.ar[i].v );
        }

    test_sub("subtest %d: freeall", ++subnum);

        fsarrfree(fa); // via macro
        fsarr_techfprintlim(logfile, &fa, 3);
        test_validate(fsarr_validate(stderr, &fa), "Validation failed after free");
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 4 ---------------------------------

static TestStatus
tf4(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    {
        test_sub("subtest %d: fsarr_save", ++subnum);

        int     cnt = 10;
        fsarray  fa = fsarr_init(cnt);
        //fs      s1[sz]; //temporary
        for (int i = 0; i < cnt; i++){
            fssprintf(fsarrget(fa, i), "Just test string %d", i);
        }
        int     res = fsarr_save("res/fsarr2.sv", &fa);
        test_validatefree(res == cnt, fsarrfree(fa), "fsarr_save have to return %d but not %d", cnt, res);
        fsarrfree(fa); // via macro
    }
    fs_alloc_check(true);
    return logret(TEST_MANUAL, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 5 ---------------------------------

static TestStatus
tf5(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;
    const char     *fname = "res/fsarr_save_load2.sv";

    test_sub("subtest %d: fsarr_save", ++subnum);
    {

        int     cnt = 10;
        fsarray fa = fsarr_init(cnt);
        //fs      s1[sz]; //temporary
        for (int i = 0; i < cnt; i++){
            fssprintf(fa.ar[i], "Just test string %d", i);
        }
        int     res = fsarr_save(fname, &fa);
        test_validatefree(res == cnt, fsarrfree(fa), "fsarr_save have to return %d but not %d", cnt, res);

    test_sub("subtest %d: fsarr_load", ++subnum);

        fsarray fa1 = fsarr_load(fname);

        test_validatefree(fa1.cnt == fa.cnt, (fsarrfree(fa1), fsarrfree(fa) ), "Loaded cnt = %d must be equal to ogiginal cnt %d", fa1.cnt, fa.cnt);

        for (int i = 0; i < fa.cnt; i++) {
            test_validatefree(fslen(fa.ar[i] ) == fslen(fa.ar[i] ), (fsarrfree(fa1), fsarrfree(fa) ),
                    "Length of fs %d of origin(%d) and loaded(%d) array must be euqal", i, fslen(fa.ar[i] ), fslen(fa.ar[i] ) );
            if (fs_cmp(fsarr_get(&fa, i), fsarr_get(&fa1, i) ) != 0)
                    test_validatefree(fs_cmp(fsarr_get(&fa, i), fsarr_get(&fa1, i) ) == 0,
                                        (fsarrfree(fa1), fsarrfree(fa) ),
                                         "%d Loaded string [%s] != origin string [%s]", i, fs_str(fsarr_get(&fa, i)), fs_str(fsarr_get(&fa1, i) ) );
        }
        fsarrfree(fa1);
        fsarrfree(fa);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 6 ---------------------------------

static TestStatus
tf6(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;

    test_sub("subtest %d: fsarr_shrink", ++subnum);
    {
        fsarray     fa = fsarr_init(50);
        int         shrinkcnt = 20;

        // fill several vals over shrinkcnt
        elem(fa.ar[shrinkcnt + 5], 100) = 'c';      // real allocation around 128b!
        fsarr_shrink(&fa, shrinkcnt);

        test_validate(fsarr_validate(stderr, &fa), "Validation failed after shrink");
        test_validatefree(fa.cnt == shrinkcnt, fsarrfree(fa), "Cnt = %d afrer shrinking, but not %d", fa.cnt, shrinkcnt);
        test_validatefree(fa.sz == shrinkcnt, fsarrfree(fa), "Cnt = %d afrer shrinking, but not %d", fa.sz, shrinkcnt);

    test_sub("subtest %d: fsarr_increase", ++subnum);

        int         increasecnt = 200;
        fsarr_increase(&fa, 200);

        test_validatefree(fa.cnt == increasecnt, fsarrfree(fa), "Cnt = %d afrer increasing, but not %d", fa.cnt, increasecnt);
        // accect to last elem
        elem(fa.ar[increasecnt - 1], 1000) = 'd';

        fsarrfree(fa);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 7 ---------------------------------

static TestStatus
tf7(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;

    test_sub("subtest %d: fsarr_attach/detach", ++subnum);
    {
        const char pt[] = "A test message!";

        fsarray     fa = fsarr_init(10);
        int         pos = 5; // < 10

        fs          s = fscopy(pt);
        if (!fsarrattach(fa, pos, s) )
            return logacterr((fsarrfree(fa), fsfree(s) ), TEST_FAILED, "Unable to attach!");

        test_validatefree( fsisnull(s), (fsarrfree(fa), fsfree(s) ),
            "%d fs in the array must be null after attaching", pos);
        test_validatefree( strlen(pt) == fa.ar[pos].len, (fsarrfree(fa), fsfree(s) ),
            "Length of original pattern (%lu) must be equat to the lengths of fs in the array (%d)", strlen(pt), fa.ar[pos].len);
        test_validatefree( strcmp(pt, fa.ar[pos].v) == 0, (fsarrfree(fa), fsfree(s) ),
            "Original pattern [%s] must be equal to the fs [%s]", pt, fa.ar[pos].v);

        fs          s2 = fsarrdetach(fa, pos);
        test_validatefree( fsisnull(fa.ar[pos]), (fsarrfree(fa), fsfree(s2) ),
            "%d fs in the array must be null after detaching", pos);
        test_validatefree( strlen(pt) == s2.len, (fsarrfree(fa), fsfree(s) ),
            "Length of original pattern (%lu) must be equat to the lengths of fs (%d)", strlen(pt), s2.len);
        test_validatefree( strcmp(pt, s2.v) == 0, (fsarrfree(fa), fsfree(s) ),
            "Original pattern [%s] must be equal to the fs [%s]", pt, s2.v);

        // no need to free s!
        fsfree(s2);
        fsarrfree(fa);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 8 ---------------------------------

static TestStatus
tf8(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;

    test_sub("subtest %d: fslh", ++subnum);
    {
        const char pt[] = "A test message!";

        fsarray     fa = fsarr_init(10);
        int         pos = 5; // < 10

        fsl         s = fsarr_getfsl(&fa, pos);
        fsl_cpystr(s, pt);

        fs         *ps = fsl_fs(s);
        test_validatefree( strlen(pt) == ps->len, fsarrfree(fa),
            "Length of original pattern (%lu) must be equat to the lengths of fs (%d)", strlen(pt), ps->len);
        test_validatefree( strcmp(pt, ps->v) == 0, fsarrfree(fa),
            "Original pattern [%s] must be equal to the fs [%s]", pt, ps->v);

        char    pt2 = 'a';
        fslelem(s, 200) = pt2;

        char c = fslget(s, 200);
        test_validatefree(c == pt2, fsarrfree(fa), "Symbol [%c] must be equal to iriginal [%c]", c, pt2);

        fsarrfree(fa);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 9 ---------------------------------

static TestStatus
tf9(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;


    test_sub("subtest %d: just a clean", ++subnum);
    {
        fsarray     fa = fsarr_init(50);
        for (int i = 0; i < fa.cnt; i++){
            int     len = 100;
            elem(fa.ar[i], len) = 'c';      // real allocation around 128b!
            fsetlen(fa.ar[i], len + 1);
        }
        logmsg("Len of 0 = %d, cnt %d", fslen(fa.ar[0]), fa.cnt );
        fsarr_clean(&fa, false);
        for (int i = 0; i < fa.cnt; i++){
            int res = fslen(fa.ar[i] );
            test_validatefree(res == 0, fsarrfree(fa), "Length of %d elem must be 0, but not %d", i, res);
            test_validatefree(fa.ar[i].v != 0, fsarrfree(fa), "Fs %d must be valuable but not null", i);
        }
        fsarrfree(fa);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: free", ++subnum);
    {
        fsarray     fa = fsarr_init(50);
        for (int i = 0; i < fa.cnt; i++){
            int     len = 1000;
            elem(fa.ar[i], len) = 'c';      // real allocation around 1024b!
            fsetlen(fa.ar[i], len + 1);
        }
        logmsg("Len of 0 = %d", fslen(fa.ar[0]) );
        fsarr_clean(&fa, true);
        for (int i = 0; i < fa.cnt; i++){
            int res = fslen(fa.ar[i] );
            test_validatefree(res == 0, fsarrfree(fa), "Length of %d elem must be 0, but not %d", i, res);
            test_validatefree(fa.ar[i].v == 0, fsarrfree(fa), "Fs %d must be null after fs_clean(.., true)", i);
        }
        fsarrfree(fa);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 10 ---------------------------------

static TestStatus
tf10(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;
    const char * const arr[] = {"Tra ta ta str1", "Bla bla bla str2", "Hu hu vot str3", "Nu meg got str4", "str5", "str6", 0};

    test_sub("subtest %d: fsarr_fromarr from null-ended array", ++subnum);
    {
        fsarray fa = fsarr_fromarr(arr, 0);
        int cnt  = countstrings(arr);
        test_validatefree(cnt == fa.cnt, fsarrfree(fa), "Count of fsarray = %d but must be %d", fa.cnt, cnt);
        for (int i = 0; i < fa.cnt; i++)
            test_validatefree(strcmp(fsstr(fa.ar[i]), arr[i] ) == 0, fsarrfree(fa), "fsarray[%d] = [%s] but must be [%s]", i, fsstr(fa.ar[i]), arr[i]);

        fsarrfree(fa);
    }
    test_sub("subtest %d: fsarr_fromarr with limit", ++subnum);
    {
        fsarray fa = fsarr_fromarr(arr, 2);
        test_validatefree(2 == fa.cnt, fsarrfree(fa), "Count of fsarray = %d but must be %d", fa.cnt, 2);

        for (int i = 0; i < fa.cnt; i++)
            test_validatefree(strcmp(fsstr(fa.ar[i]), arr[i] ) == 0, fsarrfree(fa), "fsarray[%d] = [%s] but must be [%s]", i, fsstr(fa.ar[i]), arr[i]);

        fsarrfree(fa);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 11 ---------------------------------

static TestStatus
tf11(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;
    const char * const arr[] = {"Tra ta ta str1", "Bla bla bla str2", "Hu hu vot str3", "Nu meg got str4", "str5", "str6", 0};
    int arrcnt = countstrings(arr);

    test_sub("subtest %d: fsarr_fsavelines just save as line", ++subnum);
    {
        fsarray fa = fsarr_fromarr(arr, 0);
        const char  fname[] = "res/fsarray_fsarr_fsavelines1.dat";

        FILE        *f = fopen(fname, "w+");
        if (!f)
            return logerr(TEST_FAILED, "Unable to open %s for w+", fname);

        int savedcnt = fsarr_fsavelines(f, &fa, 0);

        test_validatefree(savedcnt == arrcnt, (fclose(f), fsarrfree(fa) ),
            "Saved count %d must be equal origin count %d", savedcnt, arrcnt
        );
        // TODO: tfile utilities are required... to check file length, count of lines etc...

        fclose(f);
        fsarrfree(fa);
    }
    fs_alloc_check(true);
    return logret(TEST_MANUAL, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 12 ---------------------------------

static TestStatus
tf12(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;
    const char * const arr[] = {"Tra ta ta str1", "Bla bla bla str2", "Hu hu vot str3", "Nu meg got str4", "str5", "", "", "str6", 0};
    int arrcnt = countstrings(arr);
    logauto(arrcnt);

    test_sub("subtest %d: fsarr_fsavelines then fsarr_floadlines", ++subnum);
    {
        fsarray fa = fsarr_fromarr(arr, 0);
        const char  fname[] = "res/fsarray_fsarr_fsavelines2.dat";

        FILE        *f = fopen(fname, "w+");
        if (!f)
            return logerr(TEST_FAILED, "Unable to open %s for w+", fname);

        int savedcnt = fsarr_fsavelines(f, &fa, 0);

        test_validatefree(savedcnt == arrcnt, (fclose(f), fsarrfree(fa) ),
            "Saved count %d must be equal origin count %d", savedcnt, arrcnt
        );

        rewind(f);
        fsarray fa2 = FSARRAY();

        int loadcnt = fsarr_floadlines(f, &fa2, 0);
        fclose(f);
        test_validatefree(loadcnt == savedcnt, (fsarrfree(fa), fsarrfree(fa2) ),
            "Loaded count %d must be equal to saved count %d", loadcnt, savedcnt
        );
        test_validatefree(fsarr_cnt(&fa) == fsarr_cnt(&fa2), (fsarrfree(fa), fsarrfree(fa2) ),
            "Count of loaded arr %d must be equal to saved count %d", fsarr_cnt(&fa2), fsarr_cnt(&fa)
        );
        // TODO: compare one by one
        for (int i = 0; i < fsarr_cnt(&fa); i++)
            test_validatefree(fscmp(fa.ar[i], fa2.ar[i]) == 0, (fsarrfree(fa), fsarrfree(fa2) ),
                "%d element of loaded [%s] and saved [%s] arrays must be equal", i, fsstr(fa2.ar[i]), fsstr(fa.ar[i])
            );

        fsarrfree(fa);
        fsarrfree(fa2);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 13 ---------------------------------

static TestStatus
tf13(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;
    const char * const arr[] = {"Tra ta ta str1", "", "Bla bla bla str2", "Hu hu vot str3", "", "Nu meg got str4", "str5", "str6", 0};

    test_sub("subtest %d: fsarr_cmp equal test", ++subnum);
    {
        fsarray     fa  = fsarr_fromarr(arr, 0);
        fsarray     fa2 = fsarr_fromarr(arr, 0);
        int         pos;
        test_validatefree( fsarr_cmp(&fa, &fa2, &pos), (fsarrfree(fa), fsarrfree(fa2) ),
            "Must be equal, but not diff at line %d", pos
        );

        fsarrfree(fa);
        fsarrfree(fa2);
    }
    test_sub("subtest %d: fsarr_cmp not equal test", ++subnum);
    {
        fsarray     fa  = fsarr_fromarr(arr, 0);
        fsarray     fa2 = fsarr_fromarr(arr, 0);
        int         pos, p = 2;

        fscpystr(fa.ar[p], "ANOTHER VALUE");
        test_validatefree( !fsarr_cmp(&fa, &fa2, &pos), (fsarrfree(fa), fsarrfree(fa2) ),
            "Must be not equal, but diff at line %d", p
        );
        fsarrfree(fa);
        fsarrfree(fa2);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------------------------------------------------
int
main( /* int argc, const char *argv[] */)
{
    logsimpleinit("Start");

    testenginestd(
        testnew(.f2 =  tf1,  .num = 1, .name = "Simple init free test"                        , .desc=""                , .mandatory=true)
      , testnew(.f2 =  tf2,  .num = 2, .name = "Init/free test with valid fs (random)"        , .desc=""                , .mandatory=true)
      , testnew(.f2 =  tf3,  .num = 3, .name = "fsarr_attach test"                            , .desc=""                , .mandatory=true)
      , testnew(.f2 =  tf4,  .num = 4, .name = "fsarr_save test"                              , .desc=""                , .mandatory=true)
      , testnew(.f2 =  tf5,  .num = 5, .name = "fsarr_save/load test"                         , .desc=""                , .mandatory=true)
      , testnew(.f2 =  tf6,  .num = 6, .name = "fsarr_shrink/increase test"                   , .desc=""                , .mandatory=true)
      , testnew(.f2 =  tf7,  .num = 7, .name = "fsarr_detach/attach test"                     , .desc=""                , .mandatory=true)
      , testnew(.f2 =  tf8,  .num = 8, .name = "fsl simple test"                              , .desc=""                , .mandatory=true)
      , testnew(.f2 =  tf9,  .num = 9, .name = "fsarr_clean simple test"                      , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf10, .num = 10, .name = "fsarr_fromarr simple test"                    , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf11, .num = 11, .name = "fsarr_fsavelines manual test"                 , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf12, .num = 12, .name = "fsarr_floadlines simple test"                 , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf13, .num = 13, .name = "fsarr_cmp simple test"                        , .desc=""                , .mandatory=true)
    );

    return logcloseret(0, "end...");
}

#endif /* FSARRAYTESTING */


