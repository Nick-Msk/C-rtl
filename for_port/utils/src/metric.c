#include <stdio.h>
#include <strings.h>

#include "log.h"
#include "metric.h"
#include "bool.h"

/********************************************************************
                 METRIC MODULE IMPLEMENTATION
********************************************************************/

// static globals

Metric                      g_metric_array[MAX_METRIC];
int                         g_metricfreepos = 0;

// internal type

// ---------- pseudo-header for utility procedures -----------------

static Metric *metric_cr(int pos, const char *name);
static Metric *metric_search(const char *name);

// ------------------------------ Utilities ------------------------

static Metric *metric_search(const char *name){
    logsimple("name [%s]", name);
    for (int i = 0; i < g_metricfreepos; i++)
        if (strcmp(g_metric_array[i].name, name) == 0)
            return g_metric_array + i;
    return 0;
}

static Metric *metric_cr(int pos, const char *name){
    logsimple("create on %d position with [%s]", pos, name);
    g_metric_array[pos].value = 0;
    strncpy(g_metric_array[pos].name, name, MAX_METRIC_NAME - 1);
    g_metric_array[pos].name[MAX_METRIC_NAME - 1] = '\0';
    return g_metric_array + pos;
}

// -------------------------- (Utility) printers -------------------

// --------------------------- API ---------------------------------

// -------------------- CONSTRUCTOTS/DESTRUCTORS -------------------

// get or create
Metric      *metric_get(const char *name, bool create){
    logenter("name=[%s], create - %s", name, bool_str(create));
    Metric *m = metric_search(name);
    if (m)
        return logret(m, "found");
    // not found
    if (create && g_metricfreepos < MAX_METRIC)
        m = metric_cr(g_metricfreepos++, name);
    return logret(m, "created? %p", m);
}

// -------------------------- (API) printers -----------------------

int                       metric_fprint(FILE *f, Metric *m){
    if (m)
        return fprintf(f, "\nMETRIC:\n[%s] = [%d]\n", m->name, m->value);
    else
        return 0;
}

// -------------------------------Testing --------------------------


// SKIP FOR NOW
#ifdef METRICTESTING

#include "test.h"
// #include <signal.h>

// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    {
        test_sub("subtest %d", ++subnum);

        const char *name = "abc";
        int         ret;
        Metric *m1 = metric_create(name);
        if (!m1)
            return logret(TEST_FAILED, "Not created");

        test_sub("subtest %d", ++subnum);
        Metric *m2 = metric_acq(name);
        if (!m2)
            return logret(TEST_FAILED, "Unable to acquired %s", name);

        test_sub("subtest %d", ++subnum);
        if ( (ret = metric_getval(m2) ) != 0)
            return logret(TEST_FAILED, "Newly created metric must be 0 but not %d", ret);

        test_sub("subtest %d", ++subnum);
        metric_inc(m2);
        if ( (ret = metric_getval(m2) ) != 1)
            return logret(TEST_FAILED, "Metric must ruturn 1 after increment, but not %d", ret);

        test_sub("subtest %d", ++subnum);
        metric_free();
        m1 = metric_acq(name);
        if (m1 != 0)
            return logret(TEST_FAILED, "It should be impossible to acquire metric after freed");

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

        const char  *name = "abc";
        int          ret;
        Metric      *m = metric_create(name);

        metric_setval(m, 100);
        metric_inc(m);
        if ( (ret = metric_getval(m)) != 101)
            return logret(TEST_FAILED, "Metric must ruturn 101 after increment, but not %d", ret);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// -------------------------------------------------------------------
int
main(int argc, char *argv[])
{
    const char *logfilename = "log/metric.log";

    if (argc > 1)
        logfilename = argv[1];

    loginit(logfilename, false, 0, "Starting");

    testenginestd(
        testnew(.f2 = tf1, .num = 1, .name = "Simple Create test"  , .desc = "", .mandatory=true)
      , testnew(.f2 = tf2, .num = 2, .name = "Inc value test"      , .desc = "", .mandatory=true)
    );

    logclose("end...");
    return 0;
}


#endif /* METRICTESTING */

