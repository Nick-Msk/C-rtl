#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include "systemutils.h"
#include "iterator.h"
#include "log.h"
#include "error.h"

/********************************************************************
                    SYSTEM UTILS MODULE IMPLEMENTATION
********************************************************************/

static const int        SAVER_MAX = 10;
static int              g_savers_fd[SAVER_MAX];
static int              g_saver_ptr = 0;
static const char      *g_dev_null = "/dev/null";

// --------------------------- API ---------------------------------

bool                    su_stddisable(void){
    logenter("...");
    if (g_saver_ptr != 0)
        return logerr(false, "Std's alredy disabled");

    int             dev_null_fd;
    fflush(0);

    if ( (dev_null_fd = open(g_dev_null, O_WRONLY) ) < 0)
        return sysraise(false, "Unable to open dev/null");

    // semi-iterator
    foreachint (fd, STDOUT_FILENO, STDERR_FILENO){
        int saver_fd;
        if (g_saver_ptr < SAVER_MAX){
            if ( (saver_fd = dup(fd) ) < 0 )
                return sysraise(false, "Unable to dup %d (pos %d)", fd, g_saver_ptr);

            g_savers_fd[g_saver_ptr++] = saver_fd;
            if ( (dup2(dev_null_fd, fd) ) < 0)
                return sysraise(false, "Unable to dup2 %d -> %d (/dev/null)", fd, dev_null_fd);
        } else {
            logmsg("Out of savers... Break %d\n", g_saver_ptr);
            break;
        }
    }
    close(dev_null_fd);
    return logret(true, "Disabled");;
}

bool                    su_stdenable(void){
    logenter("...");
    fflush(0);

    if (g_saver_ptr == 0)
        return logerr(false, "Std not disabled");

    g_saver_ptr = 0;

    foreachint (fd, STDOUT_FILENO, STDERR_FILENO){
        int saver_fd = g_savers_fd[g_saver_ptr++]; //fu_saver_get();
        if ( (dup2(saver_fd, fd) ) < 0)  // not corrent but for now
            return sysraise(false, "Unable to dup2 %d (/dev/null) -> %d", fd, saver_fd);
        close(saver_fd);
    }
    return logret(true, "Enabled");
}

bool                    su_reset(void){
    while (g_saver_ptr >= 0)
        close(g_savers_fd[--g_saver_ptr] );
    g_saver_ptr = 0;
    return true;
}

// -------------------------------Testing --------------------------
#ifdef SYSTEMUTILSTESTING

#include "test.h"
#include "checker.h"

//types for testing


// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: ", ++subnum);
    {
        printf("Slots: %d\n", getdtablesize());

        if (!su_stddisable() ){
            fprintf(stderr, "Unable to disable\n");
            logerr(TEST_FAILED, "Unable to disable streams");
        }

        int     a = 5, cnt = 0;
        fprintf(stdout, "out %d: a = %d\n", ++cnt, a);
        fprintf(stderr, "err: %d: a = %d\n", ++cnt, a);

        if (!su_stdenable() ){
            fprintf(stderr, "Unable to enable\n");
            logerr(TEST_FAILED, "Unable to enable streams");
        }

        a += 2;
        fprintf(stdout, "out: %d: a = %d\n", ++cnt, a);
        fprintf(stderr, "err: %d: a = %d\n", ++cnt, a);
    }
    return logret(TEST_MANUAL, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 2 ---------------------------------

static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: disable then disable", ++subnum);
    {
        if (!su_stddisable() )
            logerr(TEST_FAILED, "Unable to disable streams");
        if (su_stddisable() )
            logerr(TEST_FAILED, "Secind disable must return false!");

        if (!su_stdenable() ){
            fprintf(stderr, "Unable to enable\n");
            logerr(TEST_FAILED, "Unable to enable streams");
        }
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------------------------------------------------------------------------------------------------------------
int
main( /* int argc, const char *argv[] */)
{
    logsimpleinit("Start");

    testenginestd(
        testnew(.f2 = tf1,  .num =  1, .name = "Simple init and validate test"      , .desc=""                , .mandatory=true)
      , testnew(.f2 = tf2,  .num =  2, .name = "Double disable test"                , .desc=""                , .mandatory=true)
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* FSTESTING */

