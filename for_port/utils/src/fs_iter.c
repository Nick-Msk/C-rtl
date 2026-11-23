#include "common.h"
#include "log.h"
#include "fs.h"
#include "error.h"
#include "fs_iter.h"

/********************************************************************
                    FAST STRING MODULE IMPLEMENTATION
********************************************************************/

// ---------- pseudo-header for utility procedures -----------------

// -------------------------- (Utility) printers -------------------


// ------------------------------ Utilities -------------- ----------

// --------------------------- API ---------------------------------

// ------------------ General functions ----------------------------

// -------------------------- (API) printers -----------------------

// ------------------ API Constructs/Destrucor  ----------------------------

// -------------------------------Testing --------------------------

#ifdef FSITERTESTING

#include "test.h"
#include "checker.h"

//types for testing

// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    {
        test_sub("subtest %d", ++subnum);

        const char *arr = "abcdefgh";
        fs s = fsliteral(arr);

        fstechfprint(logfile, s);    // for manual checking

        if (fslen(s) != (int) strlen(arr) )
             return logret(TEST_FAILED, "len = %d != strlen () = %lu", fslen(s), strlen(arr) );

        for (fsiter i = fseach(s); hasnext(i); next(i) )
            if (curr(i) != *arr++)
                return logerr(TEST_FAILED, "current iterator value [%c] != [%c]", curr(i), *(arr - 1));

        test_sub("subtest %d", ++subnum);

        fs      str = fsinit(100);
        int     pos = 0;
        for (fsiter i = fseach(s); hasnext(i); next(i) )
            elem(str, pos++) = curr(i);
        fsend(str, pos);

        fstechfprint(logfile, str);    // for manual checking

        if (fslen(str) != fslen(s) )
            return logactret(fsfree(str), TEST_FAILED, "str.len = %d != s.len () = %d", fslen(str), fslen(str) );

        if (fscmp(str, s) != 0)
            return logactret(fsfree(str), TEST_FAILED, "[%s] not equal to origin [%s]", fsstr(str), fsstr(s) );

        test_sub("subtest %d", ++subnum);

        pos = 0;
        for (fsiter i = fsslice(s, 4, 11); hasnext(i); next(i) )
            elem(str, pos++) = curr(i);
        fsend(str, pos);
        if (strcmp(s.v + 4, fsstr(str) ) != 0)
            return logactret(fsfree(str), TEST_FAILED, "[%s] not equal to origin [%s]", fsstr(str), s.v + 4);
        fsfree(str);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 2 ---------------------------------

static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    {
        test_sub("subtest %d", ++subnum);

        fs      str = fsinit(100);
        const char *arr = "abcdefgh";
        fs      s = fsliteral(arr);
        int     pos = 0;
        fsiterrev i;
        for (i = fseachrev(s); hasnextrev(i); nextrev(i) )
            elem(str, pos++) = currrev(i);
        fsend(str, pos);

        printf("BEFORE REV: %s\n", fsstr(str) );
        fstechfprint(logfile, str);
        fs_reverse(str);

        if (fscmp(str, s) != 0)
            return logactret(fsfree(str), TEST_FAILED, " REVERSED [%s] not equal to origin [%s]", fsstr(str), fsstr(s) );
        fsfree(str);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 3 ---------------------------------

static TestStatus
tf3(const char *name)
{
    logenter("%s", name);

    int         subnum = 0;
    fs      str = fsinit(100);
    const char arr[] = "abc1234567890";

    test_sub("subtest %d: construct from literal", ++subnum);
    {

        logauto(COUNT(arr));

        fsnew   it = fsinew(&str);
        for (int i = 0; i < arr[i] != '\0'; i++)
            elemnext(it) = arr[i];
        elemend(it);

        fstechfprint(logfile, str);    // for manual checking
        //logmsg("strlen: %lu, len %d", strlen(arr), fslen(str) );
        if (fslen(str) != strlen(arr) )
            return logactret(fsfree(str), TEST_FAILED, "len = %d != strlen () = %lu", fslen(str), strlen(arr) );

        if (strcmp(fsstr(str), arr) != 0)
            return logactret(fsfree(str), TEST_FAILED, "str [%s] not equal to origin arr [%s]", fsstr(str), arr );
    }

    test_sub("subtest %d: construct from another fs", ++subnum);
    {

        fs l = fsliteral(arr);      // sourcce
        fs s = fsinit(100);         // target
        // -------------------------
        fsnew it2 = fsinew(&s);     // start iter builder
        //logmsg("lit len = %d", fslen(l) );
        for (fsiter i = fseach(l); hasnext(i); next(i) )
            elemnext(it2) = curr(i);
        elemend(it2);
        //fstechfprint(logfile, s);
        if (fslen(s) != fslen(l) || fslen(l) != strlen(arr))
            return logactret( (fsfree(str), fsfree(s) ), TEST_FAILED, "s.len = %d != l.len () = %d != strlen() = %lu", fslen(s), fslen(l), strlen(arr) );
        if (fscmp(s, l) != 0)
            return logactret( (fsfree(str), fsfree(s) ), TEST_FAILED, "str [%s] not equal to origin arr [%s]", fsstr(s), fsstr(l) );

    test_sub("subtest %d: appent iterator test", ++subnum);

        it2 = fsiapp(&str);
        int     len_before = fslen(str);
        logmsg("before: str [%s], s[%s]", fsstr(str), fsstr(s) );
        for (fsiter i = fseach(s); hasnext(i); next(i) )
            elemnext(it2) = curr(i);
        elemend(it2);

        fstechfprint(logfile, str); // manual
        if (!fs_validate(logfile, &str) )
            return logactret( (fsfree(str), fsfree(s) ), TEST_FAILED, "Validation failed");
        if (fslen(s) + len_before != fslen(str) )
            return logactret( (fsfree(str), fsfree(s) ), TEST_FAILED, "str.len = %d != sum of initial lengths %d", fslen(s), fslen(s) + len_before);

        fsfree(s);
    }
    fsfree(str);
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------------------------------------------------

int
main(int argc, const char *argv[])
{
    const char *logfilename = "log/fs_iter.log";

    loginit(logfilename, false, 0, "Starting");

    if (argc > 1)
        logfilename = argv[1];

    testenginestd(
        testnew(.f2 = tf1, .num = 1, .name = "Simple iteration test" ,          .desc="", .mandatory=true)
      , testnew(.f2 = tf2, .num = 2, .name = "Reverse iteration test" ,         .desc="", .mandatory=true)
      , testnew(.f2 = tf3, .num = 3, .name = "Construct/iteration test" ,       .desc="", .mandatory=true)
    );

    logclose("end...");
    return 0;

}

#endif /* FSITERTESTING */

