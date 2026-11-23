#include <stdlib.h>

#include "common.h"
#include "log.h"
#include "fs.h"
#include "error.h"
#include "fs_array.h"

/********************************************************************
                    FAST STRING ARRAY MODULE IMPLEMENTATION
********************************************************************/

// ---------- pseudo-header for utility procedures -----------------

// -------------------------- (Utility) printers --------------------

int                             fsarr_techfprintlim(FILE *f, fsarray arr, int lim){
    int cnt = fprintf(f, "FSARRAY: sz %d, cnt %d, ptr %d namedfs ptr %p:\n", arr.sz, arr.cnt, arr.ptr, arr.ar);

    if (arr.ar)
        for (int i = 0; i < MIN(arr.sz, lim); i++){
            if (arr.ar[i].ps) {
                cnt += fstechfprint(f, *arr.ar[i].ps);   // directly name!
            }
        }
    if (lim < arr.sz)
        cnt += fprintf(f, "and more... (%d)\n", arr.sz - lim);
    return cnt;
}

int                             fsarr_techfprint(FILE *f, fsarray arr){
    return fsarr_techfprintlim(f, arr, arr.sz);
}

// ------------------------------ Utilities -------------------------

// should depent on increase strategy, simplest return n + 1;
static inline int               calcnewsize(int n){
    int sz = round_up_2(n);
    return sz; //logsimpleret(sz, "newsz = %d", sz);
}

static int                      increasesize(fsarray *fa, int newsz, bool init){
    logenter("oldsz %d, newsz %d init %s", fa->sz, newsz, bool_str(init));
    if (init)   // from fsarr_init
        newsz = calcnewsize(newsz);
    if (newsz > fa->sz || !init) {   // if exec from faarr_increate() then resize anyway
        namedfs *tmp = realloc(fa->ar, newsz * sizeof (namedfs) );
        if (!tmp) {
            return userraise(-1, 10, "Unable to allocate %lu bytes", newsz * sizeof (namedfs) );  // what about LG_LV here??/ TODO:
        }
        fa->sz = newsz;
        fa->ar = tmp;
    }
    return logret(newsz, "Increased to %d elements", newsz);
}

// compare as pointer
static int                      namedfscmp(const void *ns1, const void *ns2){
    return ((namedfs *) ns1)->ps->v - ((namedfs *) ns2)->ps->v; // OMG
}

// sorting via qsort()
static fsarray                  sortfs(const fsarray *origin){
    // make a copy first
    fsarray tmp = FSARRAY();
    increasesize(&tmp, origin->cnt, false);  // exactly count of origin
    int     j = 0;
    for (int i = 0; i < origin->sz && j < origin->cnt; i++)
        if (origin->ar[i].ps ) {
            tmp.ar[j++] = origin->ar[i];    // all namedfs, not reeally need actually
        }
    // sort
    qsort(tmp.ar, origin->cnt, sizeof(namedfs), namedfscmp);
    return tmp;
}

// --------------------------- API ---------------------------------
// ------------------ General functions ----------------------------

// never raise exception! just return true/false and put messages into f (if != 0)
bool                fsarr_validate(FILE *out, fsarray arr){
    logenter("%p", out);
    logmsg("check 1");
    if (arr.sz < 0 || arr.cnt < 0){
        if (out)
            fprintf(out, "%s: sz and cnt must be positive", __func__);
        return logerr(false, "sz and cnt must be positive");
    }
    logmsg("check 2");
    if (arr.sz < arr.cnt){
        if (out)
            fprintf(out, "sz %d must be >= cnt %d", arr.sz, arr.cnt);
        return logerr(false, "sz %d must be > cnt %d", arr.sz, arr.cnt);
    }
    logmsg("check 3");
    // now check total valid fs via cnt
    int     cnt = 0, invcnt = 0;
    for (int i = 0; i < arr.sz; i++)
        if (arr.ar[i].ps){
            cnt++;  // found a fs pointer
            if (!fs_validate(out, arr.ar[i].ps) )
                invcnt++;
        }
    if (cnt != arr.cnt){
        if (out)
            fprintf(out, "Total fs pointer allocated = %d but cnt = %d", cnt, arr.cnt);
        return logerr(false, "Total fs pointer allocated = %d but cnt = %d", cnt, arr.cnt);
    }
    if (invcnt > 0){
        if (out)
            fprintf(out, "Total invalid fs = %d", invcnt);
        return logerr(false, "Total invalid fs = %d", invcnt);
    }
    if (arr.cnt > 0){
        logmsg("sz > 0, check 4");
        fsarray   tmp = sortfs(&arr);   // make a sorting by arr.ar.s.v, tmp is shrinked and NOT valid, all fs are pointer to fs of arr
        int     duplcount = 0;
        for (int i = 1; i < tmp.sz; i++)
            if (tmp.ar[i - 1].ps->v == tmp.ar[i].ps->v){
                if (out)
                    fprintf(out, "elem [%d] == elem [%d] == %p:[%s]", i - 1, i, tmp.ar[i].ps->v, tmp.ar[i].ps->v);
                duplcount++;
            }
        free(tmp.ar);       // free temporary, no need to free ar[].s
        if (duplcount > 0){
            if (out)
                fprintf(out, "Total duplicates %d", duplcount);
            return logerr(false, "Total duplicates %d", duplcount);
        }
    }
    logmsg("all tests for now...");
    return logret(true, "Ok");
}

// -------------------- ACCESS AND MODIFICATORS ------------------------

// low level attach
int                  fsarr_attach(fsarray *restrict arr, fs *restrict s){
    // cnt is'nt used for now, just ++
    int ptr = arr->ptr;
    if (arr->ptr >= arr->sz)
        increasesize(arr, arr->sz * 2, false);
    arr->ar[arr->ptr++].ps = s;    //  for now just a copy! actually for gc() it's reuiqred only fs->v
    arr->cnt++; // just for recoding
    return ptr;
}

// -------------------------- (API) printers -----------------------

// this is normal printer
int                         fsarr_fprint(FILE *f, fsarray arr){
    int  cnt = 0;
    for (int i = 0; i < arr.sz; i++){
        if (arr.ar[i].ps->v) {
            fsfprint(f, *arr.ar[i].ps);   // directly name!
            cnt++;
            fputc('\n', f);
        }
    }
    return cnt;     // total printed items
}

// -------------------------- (API) Serialization  -----------------------
int                         fsarr_save(const char *restrict fname, const fsarray *restrict arr){
    FILE    *out = fopen(fname, "w");
    if (!out)
        return userraise(ERR_UNABLE_OPEN_FILE_READ, -1, "Unable to open %s for write", fname);
    int     cnt = fsarr_savef(out, arr);
    fclose(out);
    return cnt;
}

// f is open for write
int                         fsarr_fsave(FILE *restrict f, const fsarray *restrict arr){
    // signature
    int cnt = 0;
    fprintf(f, "FSARRAY(%d):[\n", arr->cnt);
    if (!arr->ar)
        //return logsimpleret(-1, "Nullable ar, can't serialize");    // userraise?
        return userraise(ERR_NULLABLE_PTR, -1, "Nullable ar, can't serialize"); 
    for (int i = 0; i < arr->sz; i++){
        fprintf(f, "%4d:", i);
        if (arr->ar[i].ps)  // fsarr_get() use here TODO:
            //fs_fprint(f, arr->ar[i].ps, ""), cnt++;
            fs_fsave(f, fsarr_get(arr, i) ), cnt++;
        else
            fprintf(f, "null");
        fputc('\n', f);
    }
    fprintf(f, "]");
    return logsimpleret(cnt, "Total printed %d of %d", cnt, arr->sz);
}

fsarray                     fsarr_load(const char *fname){
    FILE    *out = fopen(fname, "r");
    if (!out)
        userraiseint(ERR_UNABLE_OPEN_FILE_READ, "Unable to open %s for read", fname);
    fsarray fa = fsarr_loadf(out);
    fclose(out);
    return fa;
}

fsarray                     fsarr_fload(FILE *restrict f){
    // format: "FSARRAY: sz %d cnt %d ptr %d:[\n"
    FUSKIPFORMAT(f, "FSARRAY(");
    int cnt = FUGETUNSIGNED(f);

    FUSKIPFORMAT(f, "):[\n");
    fsarray     ar = fsarr_init(cnt);
    // cycle for fs
    for (int i = 0; i < cnt; i++){
        fs tmp = fs_fload(f, 0);
        //  NEED fs_move!!!!!

    }
    FUSKIPFORMAT(f, "]");
    return ar;
}

// ------------------ API Constructs/Destrucor  ----------------------------


int                 fsarr_free(fsarray *arr){
    logenter("%p: %p", arr, arr ? arr->ar : 0);
    int     cnt = 0;
    if (arr && arr->ar){
        for (int i = 0; i < arr->sz; i++){ // not cnt!! cnt is just a sum of attached fs
            if (arr->ar[i].ps) {
                fs_free(arr->ar[i].ps);
                cnt++;
                //arr->ar[i].name = 0; // it's ok because name is STATIC
            }
        }
        free(arr->ar);
        arr->cnt = arr->sz = arr->ptr = 0;
        arr->ar = 0;
    } else
        logsimple("Null pointer");
    return logret(cnt, "Freed %d", cnt);
}

// initialize
fsarray             fsarr_init(int sz){
    fsarray ar = FSARRAY();
    increasesize(&ar, sz, true);
    return logsimpleret(ar, "Created with sz %d", ar.sz);
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

        int sz = 100;
        fsarray fa = fsarr_init(sz);

        fsarr_techfprint(stdout, fa);
        if (!fsarr_validate(stderr, fa) )
            return logacterr(fsarr_free(&fa), TEST_FAILED, "Validation failed");

        if (fa.sz < sz)
            return logacterr(fsarr_free(&fa), TEST_FAILED, "sz (%d) must be >= initial sz %d", fa.sz, sz);

        if (fa.cnt != 0)
            return logacterr(fsarr_free(&fa), TEST_FAILED, "Cnt = %d but must be 0, because not any objects after init", fa.sz);

        test_sub("subtest %d: freeall", ++subnum);

        fsarr_techfprint(stdout, fa);
        fsarr_free(&fa);
        fsarr_techfprint(stdout, fa);
    }
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

        int sz = 30;
        fsarray fa = fsarr_init(sz);

        if (fa.sz < sz)
            return logacterr(fsarr_free(&fa), TEST_FAILED, "sz (%d) must be >= initial sz %d", fa.sz, sz);

        if (fa.cnt != 0)
            return logacterr(fsarr_free(&fa), TEST_FAILED, "Cnt = %d but must be 0, because not any objects after init", fa.sz);

        fs ars[sz];
        // try to use array but it UNUSUAL way, just as array
        for (int i = 0; i < sz; i++){
            // actually almost useless test
            ars[i] = fsinit(100);
            // introdured fssprintf(s, format, ...);
            fssprintf(ars[i], "str - %d", i);
            // NOW just a stupid iteration, WITHOUT fsarr_attach()
            namedfs nf = {.ps = ars + i};       // NOT possible to attach this way
            fa.ar[fa.cnt++] = nf;
        }
        // ptr == 0 here, it' ok because didn't use attach
        fsarr_techfprint(logfile, fa);
        if (!fsarr_validate(stderr, fa) )
            return logacterr(fsarr_free(&fa),TEST_FAILED, "Validation failed");

    test_sub("subtest %d: check lines", ++subnum);

        char buf[100];
        // check the lines
        for (int i = 0; i < sz; i++){
            sprintf(buf, "str - %d", i);        // ok in this case
            if (strcmp(fa.ar[i].ps->v, buf) != 0)
                return logacterr(fsarr_free(&fa), TEST_FAILED, "fsarr[%d]: '%s' != origin '%s'", 
                    i, fa.ar[i].ps->v, buf);
        }

    test_sub("subtest %d: freeall", ++subnum);

        fsarrfree(fa); // via macro
        fsarr_techfprint(logfile, fa);
        if (!fsarr_validate(stderr, fa) )
            return logerr(TEST_FAILED, "Validation failed after free");
    }
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

        int sz = 50;
        fsarray fa = fsarr_init(sz);

        // this test became a bit stupoid...
        fs s1[sz * 7];

        for (int i = 0; i < sz * 7; i++){
            s1[i] = FS();
            fssprintf(s1[i], "value bla bla %d", i);
            int pos /* instread of fsl */ = fsarr_attach(&fa, s1 + i);  // probably to check result instead of raise? not sure
            if (i % 100 == 0)
                fs_techfprint(logfile, fa.ar[i].ps, "");
        }
        fsarr_techfprintlim(logfile, fa, 3);

        if (!fsarr_validate(stderr, fa) )
            return logacterr(fsarrfree(fa),TEST_FAILED, "Validation failed");

        test_sub("subtest %d: freeall", ++subnum);

        fsarrfree(fa); // via macro
        fsarr_techfprintlim(logfile, fa, 3);
        if (!fsarr_validate(stderr, fa) )
            return logerr(TEST_FAILED, "Validation failed after free");
    }
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

        int     sz = 10;
        fsarray fa = fsarr_init(sz);
        fs      s1[sz]; //temporary
        for (int i = 0; i < sz; i++){
            s1[i] = FS();
            fssprintf(s1[i], "Just test string %d", i);
            fsarr_attach(&fa, s1 + i);
        }
        int     res;
        if ( (res = fsarr_save("res/fsarr.sv", &fa) ) != sz)
            return logacterr(fsarrfree(fa), TEST_FAILED, "fsarr_save have to return %d but not %d", res, sz);
        fsarrfree(fa); // via macro
    }
    {

    }
    return logret(TEST_MANUAL, "done"); // TEST_FAILED, TEST_PASSED
}

// ------------------------- TEST 5 ---------------------------------

static TestStatus
tf5(const char *name)
{
    logenter("%s", name);
    int             subnum = 0;
    const char     *fname = "res/fsarr_save_load.sv";

    test_sub("subtest %d: fsarr_save", ++subnum);
    {

        int     sz = 10;
        fsarray fa = fsarr_init(sz);
        fs      s1[sz]; //temporary
        for (int i = 0; i < sz; i++){
            s1[i] = FS();
            fssprintf(s1[i], "Just test string %d", i);
            fsarr_attach(&fa, s1 + i);
        }
        int     res;
        if ( (res = fsarr_save(fname, &fa) ) != sz)
            return logacterr(fsarrfree(fa), TEST_FAILED, "fsarr_save have to return %d but not %d", res, sz);

    test_sub("subtest %d: fsarr_load", ++subnum);

        fsarray fa1 = fsarr_load(fname);

         // TODO: compare with initial fa
        if (fa1.cnt != fa.cnt)
            return logacterr((fsarrfree(fa1), fsarrfree(fa)), TEST_FAILED, "Loaded cnt = %d must be equal to ogiginal cnt %d", fa1.cnt, fa.cnt);

        int i, j;
        for (i = 0, j = 0; i < fa.sz && j < fa1.sz; i++, j++){
            while (i < fa.sz && !fsarr_ex(&fa, i))   // if exists a faststring
                i++;
            while (j < fa1.sz && !fsarr_ex(&fa1, j))
                j++;
            if (i < fa.sz && j < fa1.sz)    // fsarr_get() is introduced!
                if (fs_cmp(fsarr_get(&fa, i), fsarr_get(&fa1, j) ) != 0)
                    return logacterr((fsarrfree(fa1), fsarrfree(fa)), TEST_FAILED,
                        "Loaded string %d [%s] != string %d [%s]", i, fs_str(fsarr_get(&fa, i)), j, fs_str(fsarr_get(&fa1, j)) );
        }
        fsarrfree(fa1);
        fsarrfree(fa);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED
}


// ------------------------------------------------------------------
int
main( /* int argc, const char *argv[] */)
{
    logsimpleinit("Start");

    testenginestd(
        testnew(.f2 = tf1, .num = 1, .name = "Simple init free test"                        , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf2, .num = 2, .name = "Init/free test with valid fs (random)"        , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf3, .num = 3, .name = "fsarr_attach test"                            , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf4, .num = 4, .name = "fsarr_save test"                              , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf5, .num = 5, .name = "fsarr_save/load test"                         , .desc=""                , .mandatory=true)
    );

    return logcloseret(0, "end...");
}

#endif /* FSARRAYTESTING */

