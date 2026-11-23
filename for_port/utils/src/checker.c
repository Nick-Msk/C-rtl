
#include <stdio.h>
#include <stdarg.h>

#include "log.h"
#include "checker.h"



/********************************************************************
                 CHECK MODULE IMPLEMENTATION
********************************************************************/

// static globals

// internal type

// ---------- pseudo-header for utility procedures -----------------

// ------------------------------ Utilities ------------------------

// -------------------------- (Utility) printers -------------------

// --------------------------- API ---------------------------------

// -------------------------- (API) printers -----------------------

int                     inv_fprintf_int(FILE *restrict out, const char *restrict expr, int res, int val, const char *restrict msg, ...){
	int 	cnt;

	cnt = fprintf(out, "Invariant for [%s] is violated\n", expr);
		  logsimple("Invariant for [%s] is violated\n", expr);		// TODO: think about how to imtegrate log and fprintf in one

	if (val && val != res){	// check if complex invariant
		cnt += fprintf(out,  "Result must be [%d] but equal to [%d]\n", val, res);
			   logsimple("Result must be [%d] but equal to [%d]\n", val, res);
	}

	// then print user message, TODO: think if logger can accept va_list
	va_list  ap;
	va_start(ap, msg);
	cnt += vfprintf(out, msg, ap);
	cnt += fprintf(out, "\n");
	va_end(ap);

	return cnt;
}

// -------------------------------Testing --------------------------
#ifdef CHECKERTESTING

#include "test.h"
#include <signal.h>

//types for testing

static int                      g_get_sig = 0;

static void
sugnal_handler_simple_print(int sig)
{
    logsimple("Signal %d (%s:%s) is catched", sig, sig_str(sig), sig_str_desc(sig));
    g_get_sig++;
}


// ------------------------- TEST 1 ---------------------------------
// Simple invariant text

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);

	int     get_sig_prev = g_get_sig;
	inv(2 + 2 == 4, "Simple 2+2 test");

	logmsg("Set up interrupt for SIGINT");

	if (signal(SIGINT, sugnal_handler_simple_print) == SIG_ERR)			// was SIG_IGN
		return logerr(TEST_FAILED, "Unable to setup ignore for SIGINT (%s)", sig_str_desc(SIGINT));

	inv(2 + 2 == 5, "Failed 2+2 test");

	if (g_get_sig != get_sig_prev + 1)
        return logerr(TEST_FAILED, "Signal SIGINT was not raised...");

    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 2 ---------------------------------
// Complex invariant test

static TestStatus
tf2(const char *name)
{
	logenter("%s", name);

	int		get_sig_prev = g_get_sig;
	inv2(2 + 7, 9, "Inv 1 (2+7==9)");

	if (signal(SIGINT, sugnal_handler_simple_print) == SIG_ERR)
        return logerr(TEST_FAILED, "Unable to setup ignore for SIGINT (%s)", sig_str_desc(SIGINT));

	inv2(5 + 3, 11, "Inv 2 (5+3==8)");

	if (g_get_sig != get_sig_prev + 1)		// inv(g_get_sig != 1); can be used here!! )))))))
		return logerr(TEST_FAILED, "Signal SIGINT was not raised...");

	return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// -------------------------------------------------------------------
int
main(int argc, char *argv[])
{
    const char *logfilename = "checker.log";

    if (argc > 1)
        logfilename = argv[1];

    loginit(logfilename, false, 0, "Starting");

    testenginestd(
        testnew(.f2 = tf1, .num = 1, .name = "Simple invariant text"       , .desc = "", .mandatory=true)
      , testnew(.f2 = tf2, .num = 2, .name = "Complex invariant test"      , .desc = "", .mandatory=true)
      //, testnew(.f2 = f3, .num = 3, .name = "Interrupt raising test"        , .desc = "Exception test."                                                             , .mandatory=true)
      //, testnew(.f2 = f4, .num = 4, .name = "System error test."            , .desc = "System error raising test (w/o exception)."  , .mandatory=true)
    );

        logclose("end...");
    return 0;
}


#endif /* CHECKERTESTING */

