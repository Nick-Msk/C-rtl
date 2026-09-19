#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <stdarg.h>
#include "log.h"
#include "common.h"
#include "error.h"

/********************************************************************
                 ERROR MODULE IMPLEMENTATION
********************************************************************/

// static globals

static const int                        ERROR_DEFAULT_INCREMENT  = 16;
static const int						ERROR_INIT_COUNT		 = 100;
static const int                    	ERR_DEFHANDLER_JUMP_CODE = 10;

// internal types

typedef struct {
        ErrorType               		type;                // not used for now, reserved for future use
        int                     		code;
        char                    		msg[ERROR_MESSAGE_MAX_LENGTH];                         // TODO: to be replaced to 'fs'
} Error;

_Thread_local Error						g_error_init[ERROR_INIT_COUNT];
_Thread_local Error                    *g_error = NULL;
_Thread_local int                       g_currerr = 0, g_allocerr = ERROR_INIT_COUNT;

_Thread_local ExceptionData   			g_env;			// for longjmp

// ---------- pseudo-header for utility procedures -----------------

// ------------------------------ Utilities ------------------------

static inline const char *
err_type_text(ErrorType t)
{
	switch (t){
		case ERR_USER	: return "ERR_USER";
		case ERR_SYS	: return "ERR_SYS";
		default			: return "Unknown";
	}
}

static inline bool
err_isinit(void)
{
	logsimple("%s", bool_str(g_error == g_error_init));
	return g_error == g_error_init;
}

static int
err_raisesig(int sig)
{
	logsimple("raise %d [%s: %s]", sig, sig_str(sig), sig_str_desc(sig));
	if (raise(sig) == -1)
	{
		logsimple("unable to raise %d signal, will try to raise SIGTERM %d", sig, SIGTERM);
		if (raise(SIGTERM) == -1)
			logsimple("unable to raise SIGTERM");
	}
	return -1;
}

// returns 0 if unable to allocate
static int
err_increase(void)
{
    Error   *err;
    int      newalloc = g_allocerr + ERROR_DEFAULT_INCREMENT;

	if (err_isinit())
	{
		if ( (err = malloc(newalloc * sizeof(Error))) == 0)
			return logsimple(0, "Unable to init alloc of %d elements", newalloc);
	}
	else {
       	if ( (err = realloc(g_error, newalloc * sizeof(Error))) == 0)
           	return logsimpleerr(0, "Unable to extend error array to %d", newalloc);
	}

    g_error    = err;
	g_allocerr = newalloc;
    return logsimpleret(g_allocerr, "Error array is increased to %d", g_allocerr);
}

// put new error at the top of error stack
// TODO: refactor here! переделать, с помощью fs
static void
err_put(ErrorType tp, int errcode, const char *msg, va_list ap)
{
	// TEMPORARY SOLUTION TO RESOLV thread local
	if (g_error == NULL) {
		g_error = g_error_init;
		g_currerr = 0, g_allocerr = ERROR_INIT_COUNT; 
	}
	Error 	*err = g_error + g_currerr++;		// currect error, must be valid pointer
	logauto(err->type = tp);
	logsimple("%s", msg);	// logsimple(msg, ap) ??? TODO:

	switch (tp){
		case ERR_USER:
			err->code = errcode;
			vsnprintf(err->msg, sizeof err->msg, msg, ap);
		break;
		case ERR_SYS:
			err->code = errno;		// system error number
			if (strerror_r(err->code, err->msg, sizeof err->msg) != ERANGE)		// enough space in the buffer
			{
				int		 len = strlen(err->msg);	// length of system error message
				char 	*pos 	= err->msg + len;
				int		 sz 	= sizeof err->msg - len - 1;
				if (sz > 0){							// TODO: not sure if this is good implementation, better to use faststring!
					len = snprintf(pos, sz, ": ");
					pos += len;
					sz  -= len;
					if (sz > 0)
						vsnprintf(pos, sz, msg, ap);		// TODO: not sure about -1, it depends on how exactly sprintf works
				} else
					logsimple("Not enough space for user message (%zu, offset %ld)", sizeof err->msg, pos - err->msg);
			} else
				logsimple("Not enough space for system message strerror (%zu)", sizeof err->msg);
		break;
		default:
			snprintf(err->msg, sizeof err->msg, "Unknown error type (%d)", tp);
		break;
	}
}

//  setjmp/longsmp API
static void
err_default_handler(int sig)
{
    logsimple("HANDLER %d", sig);
    if (!errenv.init_flag)
    {
        logsimple("Env buffer is empty");
		if (sig == SIGINT)
			logsimple("No env buffer - just working as igrone SIGINT");
		else {
			logsimpleerr(0, "terminating SIGSTOP");
        	raise(SIGSTOP);
		}
    } else {
        logsimple("make a longjmp... init flag %s, code %d", bool_str(errenv.init_flag), ERR_DEFHANDLER_JUMP_CODE);
    	longjmp(errenv.env, ERR_DEFHANDLER_JUMP_CODE);
    }
}

// -------------------------- (Utility) printers -------------------

// print particular error
static inline int
err_fprinterr(FILE *restrict out, const Error *err)
{
	return fprintf(out, "%s: [%d] %s\n", err_type_text(err->type), err->code, err->msg);		// TODO: think about mapping between code and message!
}

// --------------------------- API ---------------------------------

void
err_clean(bool force){
	if (force && !err_isinit())
    	free(g_error);	// if NULL then ok
    g_error = g_error_init;
    g_currerr = 0;
	g_allocerr = ERROR_INIT_COUNT;
    logsimple("Error stack is cleaned");
}

extern void
err_raise(ErrorType tp, int raise, int errcode, const char *msg, ...)
{
	// put data into stack and raise sig (????)
	if (g_currerr >= g_allocerr)
		if (!err_increase())
		{
			logsimpleact(fprintf(stderr, "Unable to allocate more size\n"), "Unable");		// ?? TODO: logsimple ? or logstderr
			err_raisesig(SIGTERM);
		}

	va_list		ap;
	va_start(ap, msg);

	err_put(tp, errcode, msg, ap);
	va_end(ap);

	if (raise)
		err_raisesig(raise);
}

// setjmp/longjmp API
ExceptionData*
err_getexception_info()
{
    return &g_env;      // access to global
}

bool
err_sethandler(sig_t handler)
{
    if (!handler)
        handler = err_default_handler;

    if (signal(SIGINT, handler) == SIG_ERR)
        return sysraise(false, "Unable to setup err_default_handler for SIGINT\n");

    return logsimpleret(true, "err_default_handler is activated\n");
}

// -------------------------- (API) printers -----------------------

extern int
err_fprintstacktrace(FILE *out)
{
	int		res = 0;
	logauto(g_currerr);

	res += fprintf(out, "\n------------- PRINT STACK TRACE START -----------\n\n");
	for (int i = 0; i < g_currerr; i++)
	{
		res += fprintf(out, "[%4d]: ", i);					// имя функции???
		res += err_fprinterr(out, g_error +i);
	}

	res += fprintf(out, "\n------------- PRINT STACK TRACE END -------------\n\n");
	return logautoret(res);
}

// ------------------ general functions ----------------------------

// -------------------------------Testing --------------------------
#ifdef ERRORTESTING

#include "test.h"

//types for testing

static int						g_get_sig = 0;

static void
sugnal_handler1(int	sig)
{
	logsimple("Signal %d (%s) is catched", sig, sig_str(sig));
	g_get_sig++;
}

// ------------------------- TEST 1 ---------------------------------

// Bare err_raise test (w/o wrapper)
static TestStatus
tf1(const char *name)
{
    logenter("%s: Bare err_raise test (w/o wrapper)", name);

	err_raise(ERR_USER, 0, 10, "Test err %d", 10);

	err_printstacktrace();
	if (g_currerr != 1)
		return logactret(err_clean(true), TEST_FAILED, "g_currerr = %d but must be = 1", g_currerr);

	err_clean(true);

    return logret(TEST_MANUAL, "done"); // TEST_FAILED
}

// ------------------------- TEST 2 ---------------------------------

// Create several vals test (via macro, with junt retcode)
static TestStatus
tf2(const char *name)
{
    logenter("%s: Create several vals test (via macro, with junt retcode)", name);

	const int 		cnt = 500;
	// 50 more than 20
	for (int i = 0; i < cnt; i++)
		userraise(0, 100 + i, "Error number 100 + %d", i);		// TODO: ???

	if (g_currerr != cnt)
		return logactret(err_clean(true), TEST_FAILED, "g_currerr = %d but must be = %d", g_currerr, cnt);

	err_printstacktrace();

	err_clean(true);

	return logret(TEST_MANUAL, "done"); // TEST_FAILED
}

// ------------------------- TEST 3 ---------------------------------
// Interrupt raising test
static TestStatus
tf3(const char *name)
{
	logenter("%s: Interrupt raising test", name);
	sig_t 	prev_action = signal(SIGINT, sugnal_handler1);

	logmsg("prev action = %p, SIG_ERR = %p", prev_action, SIG_ERR);

	if (prev_action == SIG_ERR)
		return logacterr(err_clean(true), TEST_FAILED, "Can't set up signal handler (%p)", SIG_ERR);

	userraiseint(15, "Hadled interrupt");

	if (g_get_sig != 1)
		return logactret(err_clean(true), TEST_FAILED, "g_get_sig = %d, but must be equal 1", g_get_sig);

	err_printstacktrace();
	err_clean(true);

	return logret(TEST_PASSED, "Signal is catched"); // TEST_FAILED
}

// ------------------------- TEST 4 ---------------------------------
//System error raising test (w/o exception)

static TestStatus
tf4(const char *name)
{
	logenter("%s: System error raising test (w/o exception)", name);

	const char *fname = "aaa.bbb.ccc";
	// try to open some non-existent file fname
	FILE * f = fopen(fname, "r");

	logauto(errno);
	if (!f)
		sysraise(0, "Unable to open file ... %s", fname);		// TODO: think about how to inject automatic logging here

	if (g_currerr != 1)
        return logactret(err_clean(true), TEST_FAILED, "g_currerr = %d but must be = 1", g_currerr);

	err_printstacktrace();
	err_clean(true);

	return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 5 ---------------------------------
// ACTION return test

static int tf5_check_int(void)
{
	logsimple("....... return 55 as retcode, 30 as error code");
	return userraiseact(55, printf("Helllo world from %s\n", __func__), 30, "from tf5 checker");
}

static void tf5_check_void(void)
{
	logsimple("....... return void, 40 as error code");
	userraiseact(0, printf("Helllo world from %s\n", __func__), 40, "from tf5 checker");		// TODO: подумать... спорно тут на счёт фиктивного 0
}

static TestStatus
tf5(const char *name)
{
	logenter("%s: ACTION return test", name);

	int res, errcode, res_val = 55, res_errcode = 30, errcount;

	logmsg("exec tf5_check_raise");

	errcount = g_currerr;
	res = tf5_check_int();

	if (g_currerr != errcount + 1)
        return logactret(err_clean(true), TEST_FAILED, "g_currerr = %d but must be = %d", g_currerr, errcount + 1);

	// checking errnum
	if ( (errcode = g_error[g_currerr - 1].code) != res_errcode)
		return logacterr(err_clean(true), TEST_FAILED, "error code of tf5_check_int = %d, but must be %d", errcode, res_errcode);

	if (res != 55)
		return logacterr(err_clean(true), TEST_FAILED, "result of tf5_check_int = %d, but must be %d", res, res_val);

	logmsg("exec tf5_check_void");

	errcount = g_currerr;
	tf5_check_void();

	if (g_currerr != errcount + 1)
        return logactret(err_clean(true), TEST_FAILED, "g_currerr = %d but must be = %d", g_currerr, errcount + 1);

	res_errcode = 40;
	// checking errnum  TODO: inv2 can be used here
    if ( (errcode = g_error[g_currerr - 1].code) != res_errcode) {
        return logacterr(err_clean(true), TEST_FAILED, "error code of tf5_check_void = %d, but must be %d", errcode, res_errcode);
	}
	
	err_printstacktrace();

	err_clean(true);
	return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 6 ---------------------------------
// Default handler test

static TestStatus
tf6(const char *name)
{
	logenter("%s: Default handler test", name);

	if (!errsethandler())
		return logerr(TEST_FAILED, "Unable to setup handler");

    if (!try() ){
	    // 35 as errcode
	    userraiseint(35, "Raising smth");
    } else
	    err_printstacktrace();

	err_clean(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 7 ---------------------------------
// Try + catch  test
static TestStatus
tf7(const char *name)
{
	logenter("%s: Try + catch  test", name);
	int		check_if_exception = 0;

	if (!errsethandler())
        return logerr(TEST_FAILED, "Unable to setup handler");

	if (!try())
	{
		logmsg("Hello, World!");
		logmsg("Something goes wrong... userraiseint here with errcode 10!");
		userraiseint(10, "Fail...");
	} else
	{
		logmsg("Exception occurs !!!!!!!");
		err_fprintstacktrace(logfile);
		check_if_exception++;
	}

	if (check_if_exception == 0)
		return logacterr(err_clean(true), TEST_FAILED, "No exception occures check_if_exception=%d", check_if_exception);
	else
		logmsg("Exception handled normally");

	err_clean(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------------------------------------------------
int
main( /*int argc, const char *argv[]*/ )
{
    LOG("log");
    //logsimpleinit("Start");

        testenginestd(
            testnew(.f2 = tf1, .num = 1, .name = "Bare err_raise test"		, .desc = "Bare err_raise test (w/o wrapper)."				, .mandatory=true)
          , testnew(.f2 = tf2, .num = 2, .name = "Several error raise test"	, .desc = "Several test (via macro, with junt retcode = 0).", .mandatory=true)
          , testnew(.f2 = tf3, .num = 3, .name = "Interrupt raising test"	, .desc = "Exception test."									, .mandatory=true)
		  , testnew(.f2 = tf4, .num = 4, .name = "System error test" 		, .desc = "System error raising test (w/o exception)."		, .mandatory=true)
          , testnew(.f2 = tf5, .num = 5, .name = "Return with action test"  , .desc = "Just raise with ACTION."							, .mandatory=true)
		  , testnew(.f2 = tf6, .num = 6, .name = "Default handler test"  	, .desc = "Trying default handler."                         , .mandatory=true)
		  , testnew(.f2 = tf7, .num = 7, .name = "Try + catch local  test"	, .desc = "Full local cycle with catching and longjmp."     , .mandatory=true)
        );

        logclose("end...");
    return 0;
}


#endif /* ERRTESTING */

