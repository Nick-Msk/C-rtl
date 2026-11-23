#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "context.h"
#include "common.h"
#include "log.h"
#include "error.h"
#include "checker.h"
#include "guard.h"
#include "numeric_ops.h"
#include "iterator.h"

/********************************************************************
                    CONTEXT MODULE IMPLEMENTATION
********************************************************************/

static const int                CTX_MAX_NAME_LEN    = 8192;
static const int                CTX_MAX_VALUE_LEN   = 65536;

// ------------------------------ Utilities ------------------------

static unsigned             get_hash(const Context *c, const char *str){
    invraise(c != 0, "Null pointer");
    return hash_djb2(str) % c->cnt;
}

static void                 free_element(ContextSortedElem *el){
    free(el->name);
    free(el->value);
    free(el);
}

static void                 free_elements(ContextSortedElem *els){
    //int  cnt = 0;
    for (ContextSortedElem *p = els, *n = 0; p != 0; p = n /* next */){
        n = p->next;
        free_element(p);
        //cnt++;
    }
    //logsimple("freed list of %d", cnt);
}

static ContextSortedElem   *getprev(const Context *restrict c, const char *restrict name, unsigned *restrict phash, ContextSortedElem **pel, ContextSortedElem **pequal){
    logenter("Loopup [%s]", name);

    ContextSortedElem *el = 0, *prevel = 0;
    unsigned hash;
    for (el =c->ctx[hash = get_hash(c, name)]; el != 0 && strcmp(el->name, name) < 0; el = el->next){
        logmsg("el %p, hash %u", el, hash);
        prevel = el;
    }
    logmsg("after: el %p, hash %u", el, hash);
    if (phash)
        *phash = hash;
    if (pel)
        *pel = el;
    if (pequal){
        if (el && strcmp(el->name, name) == 0) //  found EXACLTY!
            *pequal = el;
        else
            *pequal = 0;        // NOT found exactly!
    }
    return logret(prevel, "Found prev %p, el %p (%s)", prevel, el, el ? el->name : "");   // < or = in the list
}

static ContextSortedElem   *elemalloc(void){
    return malloc(sizeof(ContextSortedElem) );
}

// construct a single element
static ContextSortedElem   *createelem(const char *restrict name, const char *restrict value){
    ContextSortedElem   *el = elemalloc();
    if (el == 0)
        userraiseint(ERR_UNABLE_ALLOCATE, "ContextSortedElem %lu", sizeof(ContextSortedElem) );
    el->flags = 0;
    el->next  = 0;
    el->name  = strndup(name, CTX_MAX_NAME_LEN);    // strndup is more preferable
    el->value = strndup(value, CTX_MAX_VALUE_LEN);
    return el;
}

static int                  printlist(FILE *restrict out, const ContextSortedElem *restrict elem){
    int     cnt = 0;
    if (out){
        while (elem){ // TODO: replace to for (; elem; elem->next)
            cnt += ctx_fprintelem(out, elem);
            elem = elem->next;
        }
        if (cnt > 0)
            cnt += fprintf(out, "\n");
    }
    return cnt;
}

static int                  countlist(const ContextSortedElem *elem){
    int     cnt = 0;
    for (; elem; elem = elem->next)
        cnt++;
    return cnt;
}

// --------------------------- API ---------------------------------
// ------------------ API Constructs/Destrucor  --------------------

Context                     ctxinit(int cnt){
    invraise(cnt > 0, "Wrong init size");
    logenter("%d", cnt);
    unsigned    newcnt = next_prime(cnt);
    Context     tmp = (Context) {.cnt = newcnt};
    unsigned    newsz = (newcnt + 1) * sizeof(ContextSortedElem *);  // 1 for last 0
    tmp.ctx = malloc(newsz);
    if (!tmp.ctx)
        userraiseint(ERR_UNABLE_ALLOCATE, "Unable to allocated %d", newsz);
    memset(tmp.ctx, 0, newsz); // not sure
    return logret(tmp, "Created with %d", tmp.cnt);
}

void                        ctxfreed(Context *c){
    invraise(c != 0 && c->ctx != 0, "Null pointer");
    ctxreset(c);
    free(c->ctx);   // free array, but Context is in stack memory
    c->ctx = 0;
    c->cnt = 0;
    logsimple("Freed");
}

// ------------------ General functions ----------------------------

bool                  ctxreset(Context *c){
    invraise(c != 0 && c->ctx != 0, "Null pointer");
    /* for (int i = 0; i < c->cnt; i++)  // TODO: think about foreach iter
        if (c->ctx[i])
            free_elements(c->ctx[i]), c->ctx[i] = 0;*/
    // pforeach iterate via array elements
    foreach_arr(item, c->ctx, c->cnt){
        if (item){
            free_elements(item);
            item = 0;
        }
    }
    return true;
}

ContextSortedElem    *ctxget(const Context *restrict c, const char *restrict name){
    invraise(c != 0 && name != 0, "Null pointer");
    logenter("Loopup %s", name);
    // ref using new getprev(c, name, &hash, &el)
    ContextSortedElem *el = 0;
     getprev(c, name, 0, 0, &el);   // no need hash or nextel

    if (el) // no mor echecking!
        return logret(el, "Found %p", el);   // exact
    else
        return logerr( (ContextSortedElem *) 0, "Not found");
}

bool                  ctxadd(Context *restrict c, const char *restrict name, const char *restrict value){
    invraise(c != 0 && name != 0 && value != 0, "Null pointer");
    logenter("Adding %.20s:%.40s", name, value);

    unsigned hash  = 0;
    ContextSortedElem *equal = 0, *nextel = 0;
    ContextSortedElem *prevel = getprev(c, name, &hash, &nextel, &equal);

    if (equal){ // just replace previous value
        int     newlen = strnlen(value, CTX_MAX_VALUE_LEN) + 1;
        char   *tmp;
        logmsg("Revalue from %.40s", equal->value);
        if (newlen > (int) strlen(equal->value) + 1 ){
            tmp = realloc(equal->value, newlen);
            if (!tmp)
                userraiseint(ERR_UNABLE_ALLOCATE, "Realloc value for %d", newlen);
            logmsg("tmp %p, prev value %p", tmp, equal->value);
            equal->value = tmp;
        }
        memcpy(equal->value, value, newlen);
    } else {
        ContextSortedElem *newel = createelem(name, value);
        if (prevel){    // mount
            prevel->next = newel;
            newel->next = nextel;
            logmsg("Mounted to elem %.20s, next %p", prevel->name, newel->next);
        }
        else {
            c->ctx[hash] = newel;
            newel->next = nextel;
            logmsg("Mounter directly to ctx %u, next %p", hash, newel->next);
        }
    }
    return logret(true, "Ok");
}

bool                         ctxdel(Context *restrict c, const char *restrict name){
    logenter("%.20s", name);

    unsigned            hash;
    ContextSortedElem  *el = 0;
    ContextSortedElem  *prevel = getprev(c, name, &hash, 0, &el);
    if (!el)    // not found
        return logerr(false, "Not deleted");
    // umount el
    if (!prevel)
         c->ctx[hash] = el->next;
    else
        prevel->next = el->next;
    free_element(el);
    return logret(true, "Deleted");
}

int                          ctxcount(const Context *c){
    int cnt = 0;
    if (c && c->ctx){
        foreach_arr(item, c->ctx, c->cnt)
            if (item)
                cnt += countlist(item);
    }
    return logsimpleret(cnt, "Total %d", cnt);
}

// -------------------------- (API) printers -----------------------

int                          ctx_fprintelem(FILE *restrict out, const ContextSortedElem *restrict elem){
    invraise(elem != 0, "Null pointer");
    int         cnt = 0;
    if (out)
        cnt += fprintf(out, "%d [%.20s:%.100s]%s\t", elem->flags, elem->name, elem->value, elem->next ? "->": "");   // TODO: 20 and 100 can be configurable
    return cnt;
}

int                          ctx_techfprint(FILE *restrict out, const Context *restrict c, const char *restrict name){
    invraise(c != 0, "Null pointer");
    int cnt = 0;
    if (out){
        cnt += fprintf(out, "CONTEXT %s[%d - %p]:\n", name, c->cnt, c->ctx);
        /*foreach_arr(item, c->ctx, c->cnt){
            if (item)
                cnt += printlist(out, item);
        }*/
        for (int i = 0; i < c->cnt; i++)
            if (c->ctx[i]){
                fprintf(out, "HASH %3d: ", i);
                cnt += printlist(out, c->ctx[i] );
            }
        cnt += fprintf(out, "\n");
    }
    return cnt;
}

// -------------------------------Testing --------------------------

#ifdef CONTEXTTESTING

#include "test.h"
#include "checker.h"

//types for testing


// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: init + free", ++subnum);
    {
        int     initcnt = 100;
        Context c = ctxinit(initcnt);
        test_validatefree(c.cnt > initcnt, ctxfree(c), "cnt %d  must be prime number and >= %d", c.cnt, initcnt);
        int     countval = ctxcount(&c);

        ctxtechfprint(stdout, c);

        test_validatefree(countval == 0, ctxfree(c), "Total count must be zero");
        ctxfree(c);
    }
    return logret(TEST_MANUAL, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}


// ------------------------- TEST 2 ---------------------------------

static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: add + get", ++subnum);
    {
        int     initcnt = 100;
        Context c = ctxinit(initcnt);
        test_validatefree(c.cnt > initcnt, ctxfree(c), "cnt %d  must be prime number and >= %d", c.cnt, initcnt);

        const char *name = "Test name 1", *value = "Test value 1";
        ctxadd(&c, name, value);
        ctxtechfprint(logfile, c);

        int     countval = ctxcount(&c);
        test_validatefree(countval == 1, ctxfree(c), "Total count must be 1 but not %d", countval);

        const char *res = ctxgetvalue(&c, name);

        test_validatefree(res != 0, ctxfree(c), "Not found - error\n");

        test_validatefree(strcmp(res, value) == 0, ctxfree(c), "value [%s] must be equal to init value [%s]", res, value);

        ctxfree(c);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 3 ---------------------------------

static TestStatus
tf3(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: add + get + del", ++subnum);
    {
        int     initcnt = 10;
        Context c = ctxinit(initcnt);
        test_validatefree(c.cnt > initcnt, ctxfree(c), "cnt %d  must be prime number and >= %d", c.cnt, initcnt);

        const char *name = "Test name xxx", *value = "Test value yyy";
        ctxadd(&c, name, value);

        int     countval = ctxcount(&c);
        test_validatefree(countval == 1, ctxfree(c), "Total count must be 1 but not %d", countval);

        ctxtechfprint(logfile, c);

        const char *res = ctxgetvalue(&c, name);
        test_validatefree(res != 0, ctxfree(c), "Not found - error\n");

        test_validatefree(strcmp(res, value) == 0, ctxfree(c), "value [%s] must be equal to init value [%s]", res, value);

        bool deleted = ctxdel(&c, "NOT A VAR");
        test_validatefree(!deleted, ctxfree(c), "I: Must be false, because no such var");

        deleted = ctxdel(&c, name);

        ctxtechfprint(logfile, c);
        test_validatefree(deleted, ctxfree(c), "II: Not deleted!!! Must be true");

        countval = ctxcount(&c);
        test_validatefree(countval == 0, ctxfree(c), "Total count must be 0 but not %d", countval);

        deleted = ctxdel(&c, name);
        test_validatefree(!deleted, ctxfree(c), "III: Must be false because %s already deleted", name);

        ctxfree(c);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 4 ---------------------------------

static TestStatus
tf4(const char *name)
{

    logenter("%s", name);
    int         subnum = 0;

    int     initcnt = 10;
    Context c = ctxinit(initcnt);
    test_sub("subtest %d: multiple add", ++subnum);
    {
        const char    name[] = "Var name 1";
        const char    value1[] = "Val1";
        const char    value2[] = "Vallllllllllllllll2";

        ctxadd(&c, name, value1);
        ctxadd(&c, name, value2);

        int           countval = ctxcount(&c);
        test_validatefree(countval == 1, ctxfree(c), "Total count must be %d but not %d", 1, countval);

        const char *res = ctxgetvalue(&c, name);
        test_validatefree(res != 0, ctxfree(c), "Not found - error\n");
        test_validatefree(strcmp(res, value2) == 0, ctxfree(c), "value [%s] must be equal to init value [%s]", res, value2);

        ctxfree(c);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 5 ---------------------------------

static TestStatus
tf5(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    int         initcnt = 10;
    Context c = ctxinit(initcnt);
    test_sub("subtest %d: multiple add", ++subnum);
    {
        char    name[100], value[100];
        int     cntasc = 25;
        // asc
        for (int i = 0; i < cntasc; i++){
            snprintf(name,  sizeof(name) - 1,  "par name %d", i);
            snprintf(value, sizeof(value) - 1, "par value %d", i);
            ctxadd(&c, name, value);
        }
        ctxtechfprint(logfile, c);

        int     countval = ctxcount(&c);
        test_validatefree(countval == cntasc, ctxfree(c), "Total count must be %d but not %d", cntasc, countval);

        // desc
        for (int i = 50; i > 0; i--){
            snprintf(name,  sizeof(name) - 1,  "par name2 %d", i);
            snprintf(value, sizeof(value) - 1, "par value2 %d", i);
            ctxadd(&c, name, value);
        }
        ctxtechfprint(logfile, c);
    }
    test_sub("subtest %d: multiple get", ++subnum);
    {
        char            name[100], value[100];
        const char     *res;
        for (int i = 50; i > 0; i--){
            snprintf(name,  sizeof(name) - 1,  "par name2 %d", i);
            snprintf(value, sizeof(value) - 1, "par value2 %d", i);
            res = ctxgetvalue(&c, name);

            test_validatefree(res != 0, ctxfree(c), "%s Not found - error\n", name);
            logmsg("name [%s], res value [%s]", name, res);
            // checking
            test_validatefree(strcmp(res, value) == 0, ctxfree(c), "value [%s] must be equal to init value [%s]", res, value);
        }
    }
    test_sub("subtest %d: last free", ++subnum);
    {
        ctxfree(c);
        int     countval = ctxcount(&c);
        test_validate(countval == 0, "Total count must be %d but not %d", 0, countval);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 6 ---------------------------------

static TestStatus
tf6(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: multiple add", ++subnum);
    {
        char        name[100], value[100];
        int         cntasc = 250, delcnt = 0;
        int         initsz = 100;
        Context     c = ctxinit(initsz);
        // asc
        for (int i = 0; i < cntasc; i++){
            snprintf(name,  sizeof(name) - 1,  "param name %d", i);
            snprintf(value, sizeof(value) - 1, "param value %d", i);
            ctxadd(&c, name, value);
        }
        for (int i = 0; i < cntasc; i++){
            if (i % 3 == 0){
                delcnt++;
                snprintf(name,  sizeof(name) - 1,  "param name %d", i);
                if (!ctxdel(&c, name) )
                    return logacterr(ctxfree(c), TEST_FAILED, "Unable to del %s", name);
            }
        }
        for (int i = 0; i < cntasc; i++){
            if (i %3 != 0){
                snprintf(name,  sizeof(name) - 1,  "param name %d", i);
                snprintf(value, sizeof(value) - 1, "param value %d", i);
                const char *res = ctxgetvalue(&c, name);
                // checking
                test_validatefree(res != 0 && strcmp(res, value) == 0, ctxfree(c), "value [%s] must be equal to init value [%s]", res, value);
            }
        }
        for (int i = 0; i < cntasc; i++){
            if (i % 3 == 0){
                snprintf(name,  sizeof(name) - 1,  "param name %d", i);
                const char *res = ctxgetvalue(&c, name);
                test_validatefree(res == 0, ctxfree(c), "Should NOT be found [%s] but not [%s]", name, res);
            }
        }
        int         cnt = ctxcount(&c);
        test_validate(cnt == cntasc - delcnt, "Total count must be %d but not %d", cntasc - delcnt, cnt);
        // done
        ctxfree(c);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------------------------------------------------------------------------------------------------------------
int
main( /* int argc, const char *argv[] */)
{
    logsimpleinit("Start");

    testenginestd(
        testnew(.f2 = tf1, .num = 1, .name = "Simple init and validate test"              , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf2, .num = 2, .name = "Simple add and get test"                    , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf3, .num = 3, .name = "Simple add + get + del test"                , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf4, .num = 4, .name = "Multiple add + free test"                   , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf5, .num = 5, .name = "Multiple add + get test (cycle)"            , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf6, .num = 6, .name = "Composite test (cycle)"                     , .desc=""                , .mandatory=true)
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* CONTEXTTESTING */
