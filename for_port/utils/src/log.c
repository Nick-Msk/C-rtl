#include "log.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------------

					GENERAL LOGGING MODULE
API:
	loginit
	logclose
	logenter
	logret
	logerr
---------------------------------------------------------------------------------- */

#define 		TOTAL_MOD		"Total modules[%d]\n"
#define 		MODULE_DESC		"Modname(%d) %s: level[%d]\n"

static 			const int		MAX_SZ				=4096;			// better to use POSIX or SC limitation instead of this constant
static 			const int		TIME_SZ				=100;
static			const int		OFFSET_INC			=4;

static			int				g_offset			=0;				// current logging offset
static			int				g_offset_inc		=OFFSET_INC;	// currect offset increment
static			char			g_logname[MAX_SZ]	="default.log";	// current  logging file name
static			FILE		   *g_logfile			=0;				// current logging file (init on 1 exec if null)
static			char			g_time_buf[TIME_SZ];				// buffer for time print

static 			int				g_modcount			= 1;						// count of registered modules
static          LogModlevel		def_mod				= MOD(DEFAULT_MOD, LOGALL); // structure for default module  TODO: replace to char * later!
static      	LogModlevel	   *g_modules			= &def_mod;				// array of registered modules

static			bool			g_logon_mode		= true;				// program switch on/off mode
static			LogFormat		g_format_schema		= LOG_FORMAT_ALL;	// TODO: it should be for module, but not general

// internal, utility
static inline const char *
print_time(void)
{
	time_t t = time(0);
	struct tm *now = localtime(&t);
	strftime(g_time_buf, TIME_SZ, "%H:%M:%S", now);
	return g_time_buf;
}

// comparator
static int
cmp(const void *v1, const void *v2)
{
	const LogModlevel *m1 = v1, *m2 = v2;
	return strcmp(m1->module, m2->module);
}

// return level by module name, g_modules must be initialized
static int
getlevel(const char *module)
{
	LogModlevel *m = bsearch(module, g_modules, g_modcount, sizeof(LogModlevel), cmp);
	if (m)
		return m->level;
	else
		return -1;
}

// reset to default
static void
resetmod(void)
{
	g_modules = &def_mod;		//  what about free???
	g_modcount = 1;
}

// return false if smth goes wrong
static bool
log_preambule(
	LogAction            act,
	int                  lv,                // offset of logging (must be mutlipied to g_offset_inc to make effective level)
	Loglevel             msglevel,          // level of logging type, always = LOGALL now
	const char *restrict modname,
	const char *restrict filename,
	const char *restrict funcname,
	int                  lineno
)
{
	const char *act_msg;
	// parse action to determine what to do
	bool    	simple          = act & LOG_SIMPLE;     // simple means just flat message
	bool    	no_preambule    = act & LOG_NOPREAMBULE;

	act &= ~LOG_SIMPLE;
	act &= ~LOG_NOPREAMBULE;

	if (getlevel(modname) < msglevel)   // no logging in this case			// TODO: не используется сейчас по факту, мб неверно и нужно рассчитывать корректный lg_lv
        return false;

	switch(act)
    {
        case LOG_ENTER:
			lv = g_offset;  // that is why we need g_offset
			if (!simple)
                g_offset += g_offset_inc;
            act_msg = "ENTER";
		break;
		case LOG_LEAVE: case LOG_ERR:
			if (!simple)
                g_offset = lv -= g_offset_inc;
			else
                lv = g_offset;
            act_msg = (act == LOG_LEAVE) ? "LEAVE" : "ERROR";
		break;
		case LOG_MSG:
            if (!simple)
                g_offset = lv;
			else
				lv = g_offset;
			act_msg = "";
		break;
		default:
			fprintf(stderr, "Preambule: Incorrect act = [%u]\n", act);
			return false;
	}
	assert(lv >= 0);

	if (!g_logon_mode)
		return false;

	if (!no_preambule)
	{
		switch(g_format_schema)     // TODO: it should be for module, but not global
		{
			case LOG_FORMAT_EMPTY:
                fprintf(g_logfile, "%*c", lv + 1, '\0');
			break;
			case LOG_FORMAT_ALL:
				fprintf(g_logfile, "%*c%s:%s:%s(%d)]:%s(%s): ", lv + 1, '[', (strcmp(modname, DEFAULT_MOD) == 0) ? "": modname,
					 filename, funcname, lineno, act_msg, print_time());
			break;
			case LOG_FORMAT_SIMPLE:
				fprintf(g_logfile, "%*c%s(%d)]:%s%c ", lv + 1, '[', funcname, lineno, act_msg, *act_msg != '\0' ? ':' : ' ');
			break;
			case LOG_FORMAT_ONLY_FUNC:
				fprintf(g_logfile, "%*c%s(%d)]:%s%c ", lv + 1, '[', funcname, lineno, act_msg, *act_msg != '\0' ? ':' : ' ');
			break;
			case LOG_FORMAT_ONLY_TIME:
				fprintf(g_logfile, "%*c%s(%d)]:%s(%s): ", lv + 1, '[', funcname, lineno, act_msg, print_time());
			break;
			case LOG_FORMAT_ONLY_FILE:
				fprintf(g_logfile, "%*c%s:%s(%d)]:%s%c ", lv + 1, '[', (strcmp(modname, DEFAULT_MOD) == 0) ? "": modname, filename, lineno, act_msg, *act_msg != '\0' ? ':' : ' ');
			break;
			default:
				fprintf(stderr, "Logger: Incorrect format schema [%d]\n", g_format_schema);
			break;
		 }
	}
	return true;
}

// init module list, level == -1 is stop signal
bool
log_modinit(LogModlevel *modlist)
{
	FILE   *out = g_logfile ? g_logfile : stderr;
	int		cnt = 0;

	if (g_modules != &def_mod)
	{
		fprintf(out, "Modules are already loaded, run log_modclear before");
		return false;
	}

	if (!modlist)	// no modules, use DEF_MODULE
		return true;

	// determine size
	while (modlist[cnt].level >= 0)
		cnt++;

	if (cnt == 0)	// wring input, at lease one module must be presented
		return false;

	if (!(g_modules = malloc(cnt * sizeof(LogModlevel))))
	{
		fprintf(g_logfile, "Unable to allocate memory for module list (%lu)", cnt * sizeof(LogModlevel));
		return false;
	}
	// copy data
	for (int i = 0; i < cnt; i++)
		 g_modules[i] = modlist[i];

	g_modcount = cnt;
	g_logon_mode = true;

	// sort data by module name
	qsort(g_modules, g_modcount, sizeof(LogModlevel), cmp);
	return true;
}

// destructor
void
log_modclear(void)
{
	if (g_modules != &def_mod)
		free(g_modules);
    resetmod();
}

// save module list to file
bool
log_modsave(const char *name)
{
	FILE 	*f = fopen(name, "w");
	if (!f)
	{
		fprintf(stderr, "Unable to open %s for write\n", name);
		return false;
	}

	fprintf(f, TOTAL_MOD, g_modcount);				// not sure, may be better to use literal.
	for (int i = 0; i < g_modcount; i++)
		fprintf(f, MODULE_DESC, i, g_modules[i].module, g_modules[i].level);

	fclose(f);
	return true;
}

// load module list from file
bool
log_modload(const char *name)
{
	FILE * f = fopen(name, "r");
	if (!f)
	{
		fprintf(stderr, "Unable to open %s for read\n", name);
		return false;
	}

	int  modcnt = 0;
	int  cnt = fscanf(f, TOTAL_MOD, &modcnt);
	if (cnt < 1)
	{
		fprintf(stderr, "Unable to read number of modules (by pattern [%s])\n", TOTAL_MOD);
		fclose(f);
		return false;
	}

	if (!(g_modules = malloc(modcnt * sizeof(LogModlevel))))
    {
        fprintf(g_logfile, "Unable to allocate memory for module list (%lu)", modcnt * sizeof(LogModlevel));
		fclose(f);
        return false;
    }
	for (int i = 0; i < modcnt; i++)
	{
		int	lv;
		if (fscanf(f, MODULE_DESC, &modcnt, g_modules[i].module, &lv) < 3 || lv > LOGALL || lv < LOGOFF)
		{
			fprintf(stderr, "Wrong input in line %d (by pattern [%s])\n", i, MODULE_DESC);
			fclose(f);
			log_modclear();
			return false;
		}
		g_modules[i].level = lv;
	}
	g_modcount = modcnt;
	g_logon_mode = true;

	return true;
}

// constructor
bool
log_init(const char *restrict logname
	   , bool 				  append
	   , LogFormat			  logformat
	   )
{
	if (log_isinit())
		return true;

	if (logname)
	{
		strncpy(g_logname, logname, MAX_SZ-1);
		g_logname[MAX_SZ - 1] = '\0';	// in case of overflow
	}
	if ((g_logfile = fopen(g_logname, append? "a" : "w"))==0)
		return false;

	log_format(logformat);

	if (setvbuf(g_logfile, 0, _IONBF, 0) != 0)		// unable to setup unbuf mode, put warning
		fprintf(g_logfile, "WARNING: unable to setup unbuffered mode!\n");
	return true;
}

// checker
bool
log_isinit(void)
{
	return g_logfile != 0;
}

// return stream
FILE *
log_file(void)
{
	return g_logfile;
}

// cleaner
void
log_close(void)
{
	// close logfile
	if (g_logfile)
		fclose(g_logfile);
	g_logfile = 0;			// must be 0 when not initialized
	g_format_schema = LOG_FORMAT_ALL;
	// free modules
	log_modclear();
	*g_logname = '\0';
}

// return current offset in symbols
int
log_offset(void)
{
	return g_offset;
}

int
log_prog_switch(bool logon_mode)
{
	return g_logon_mode = logon_mode;
}

bool
log_format(LogFormat lf)
{
	if (lf < LOG_FORMAT_EMPTY || lf > LOG_FORMAT_ONLY_TIME)
	{
		fprintf(stderr, "Logger: Unable to setup log format to %d\n", lf);
		return false;
	}
	 g_format_schema = lf;
	return true;
}


// wrapper for log_msg_ap
int
log_msg(LogAction            act,
        int                  lv,                // offset of logging (must be mutlipied to g_offset_inc to make effective level)
        Loglevel             msglevel,          // level of logging type, always = LOGALL now
        const char *restrict modname,
        const char *restrict filename,
        const char *restrict funcname,
        int                  lineno,
        const char *restrict msg,
        ...
       )
{
	va_list		ap;
	va_start(ap, msg);
	// close ap automatically
	return log_msg_ap(act, lv, msglevel, modname, filename, funcname, lineno, msg, ap);
}

// log bytes as number


// only one real  log message function
// return current offset for this function
int
log_msg_ap(
		LogAction            act,
		int					 lv,				// offset of logging (must be mutlipied to g_offset_inc to make effective level)
		Loglevel			 msglevel,			// level of logging type, always = LOGALL now
		const char *restrict modname,
		const char *restrict filename,
		const char *restrict funcname,
		int 				 lineno,
		const char *restrict msg,
		va_list				 ap
	   )
{
	if (!g_logfile && !log_init(0, false, LOG_FORMAT_ALL))		// TODO: it's qwestion about default mode here
		return -1;

	bool    no_newline 		= act & LOG_NONEWLINE;
	act	&= ~LOG_NONEWLINE;

	if (!log_preambule(act, lv, msglevel, modname, filename, funcname, lineno))
		return g_offset;

	if (msg) // print user message if any
		vfprintf(g_logfile, msg, ap);

	if (!no_newline)
		fputc('\n', g_logfile);		// goto next line

	va_end(ap);     // close ap in all cases

	return g_offset;
}

// print particular bytes
int
log_numbers(LogAction            act,
            int                  lv,         		// level of logging (must be mutlipied to g_offset_inc to make effective level)
            Loglevel             msglevel,          // level of logging type, always = LOGALL now
            const char *restrict modname,
            const char *restrict filename,
            const char *restrict funcname,
            int                  lineno,
            const char *restrict bytes,
            int                  sz
            )
{
	if (!log_preambule(act, lv, msglevel, modname, filename, funcname, lineno))
		return -1;

	// print only valuable data for now
	for (int i = 0; i < sz; i++)
	{
		char 	c = bytes[sz];
		if (c >= 0 && c <= 9)
			putc(c + '0', g_logfile);
		else
			putc(c, g_logfile);
	}
	putc('\n', g_logfile);
	return sz + 1;
}


#ifdef LOGTESTING
/*
	TESTING
	1. logint + logclose
	2. direct logging function
	3. enclosure test
	4. common macro test
	5. enclosure macro test
	6. interruption macro test
*/

#include "test.h"

/* test implementation */

static void
print_modules(void)
{
	for (int i = 0; i < g_modcount; i++)
		printf("Modname(%d) %s: level[%s]\n", i, g_modules[i].module, log_levelstr(g_modules[i].level));
}

// ------------------------- TEST 1 -------------------------
// Loginit/close test
static TestStatus
f1(const char *name)
{
	const  char *test_name="test1.log";

	printf("%s - %s: Loginit/close test\n", name, __func__);

	if (!log_init(test_name, false, LOG_FORMAT_ALL)){
		printf("\t\tUnable to init logging\n");
		return TEST_FAILED;
	}
	printf("\t\tLoginit ok, then close\n");
	log_close();
	printf("\t\tCleaning...\n");
	remove(test_name);
	return TEST_PASSED;
}

// ------------------------ TEST 2 ----------------------------
// Log_msg test
static TestStatus
f2(const char *name)
{
	const  char *test_name="test2.log";

	printf("%s - %s: Log_msg test\n", name, __func__);

	if (!log_init(test_name, false, LOG_FORMAT_ALL)){
        printf("\t\tUnable to init logging\n");
        return TEST_FAILED;
    }
	int LG_LV=0, NEW_LG_LV;	// simulation of macros
 	NEW_LG_LV = log_msg(LOG_ENTER, LG_LV, LOGALL, DEFAULT_MOD, __FILE__, __func__, __LINE__, "entering to level %d", 1);
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_MSG, LG_LV, LOGALL, DEFAULT_MOD, __FILE__, __func__, __LINE__, "typing message... %s", "Hello World");
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_MSG, LG_LV,  LOGALL, DEFAULT_MOD, __FILE__, __func__, __LINE__, "try one more msg without additional vars");
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_LEAVE, LG_LV,  LOGALL, DEFAULT_MOD, __FILE__, __func__, __LINE__, "leaving level %d", 1);
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	log_close();

    return TEST_PASSED;
}

// ------------------------ TEST 3 ----------------------------
// test several level of logging
static TestStatus
f3(const char *name)
{
	const  char *test_name="test3.log";

	printf("%s - %s: test several level of logging\n", name, __func__);

    if (!log_init(test_name, false, LOG_FORMAT_ALL)){
        printf("\t\tUnable to init logging\n");
        return TEST_FAILED;
    }
    int level=0, LG_LV=0, NEW_LG_LV, LG_LV1; // simulation of macros

	NEW_LG_LV = LG_LV1 = log_msg(LOG_ENTER, LG_LV, LOGALL, "DEFAULT_MOD", __FILE__, __func__, __LINE__, "entering to level %d", ++level);
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_ENTER, LG_LV, LOGALL, "DEFAULT_MOD", __FILE__, "F_2", __LINE__, "entering to level %d", ++level);
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_ENTER, LG_LV, LOGALL, "DEFAULT_MOD", __FILE__, "F_3", __LINE__, "entering to level %d", ++level);
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_ERR, LG_LV, LOGALL, "DEFAULT_MOD", __FILE__, "F_3", __LINE__, "leaving level %d", level--);
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_ENTER, LG_LV, LOGALL, "DEFAULT_MOD", __FILE__, "F_31", __LINE__, "entering to level %d", ++level);
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_MSG, LG_LV, LOGALL, "DEFAULT_MOD", __FILE__, "F_31", __LINE__, "somthing happens here...");
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	NEW_LG_LV = log_msg(LOG_LEAVE, LG_LV1, LOGALL, "DEFAULT_MOD", __FILE__, __func__, __LINE__, "leaving level %d", 1);
	printf("NEW_LG_LV=%d\n", LG_LV = NEW_LG_LV);
	log_close();

	return TEST_PASSED;
}

// ------------------------ TEST 4 ----------------------------
// Common macro test
static TestStatus
f4(const char *name)
{
	const  char *test_name="test4.log";
	int c=0;

	printf("%s - %s: Common macro test\n", name, __func__);

	loginit(test_name, false, 0, "start test macro... %c", '-');
	logmsg("test message [%f]", 1.2345);
	logclose("finishing with %d", c);

	return TEST_PASSED;
}

// ------------------------ TEST 5 ----------------------------
// Enclosure macro test
static void
f5_lv2(void)
{
    logenter("start... 2");
    logmsg("smth wrong %f", 2.34);
    logerr(0,"sorry... %s", "########");
}

static void
f5_lv1(void)
{
	logenter("");
	logmsg("inside 1");
	f5_lv2();
	logmsg("outside 1");
	logret(0, "");
}

static TestStatus
f5(const char *name)
{
	const  char *test_name="test5.log";
	int c=5;

	printf("%s - %s: Enclosure macro test\n", name, __func__);

	loginit(test_name, false, 0, "start test enclosure macro...");
	logmsg("test1");
	logmsg("");
	logmsg("-");
	logmsg(0);
	logmsg("-----");
	logmsg("test%d", 222);
	f5_lv1();
	f5_lv2();
	logclose("finishing with %d", c);

	return TEST_PASSED;
}

// ------------------------ TEST 6 ----------------------------
// Interrupted macro test
static void
f6_lv2(void)
{
	logenter(0);
	logmsg("bla bla bla");
}

static void
f6_lv1(void)
{
	logenter("");
	logmsg("do smth...");
	f6_lv2();
}

static TestStatus
f6(const char *name)
{
	const  char *test_name="test6.log";
	int c=9;

	printf("%s - %s: Interrupted macro test\n", name, __func__);

	loginit(test_name, false, 0, "start test interruption macro...");
	logmsg("...");
	f6_lv1();
	logmsg("..................");
	logclose("finishing with %d", c);

	return TEST_PASSED;
}

// ------------------------ TEST 7 ----------------------------
// Module init testing
static TestStatus
f7(const char *name)
{

	printf("%s - %s: Module init testing\n", name, __func__);

	if (!log_modinit( (LogModlevel []) {
			{ .module = "TST", .level = LOGALL }
		  , { .module = "PRC", .level = LOGOFF }
		  , { .module = "DBD", .level = LOGALL }
		  , { .module = "TMP", .level = LOGALL }
		  , { .level = _LOGSTOP }
		 } ) )
	{
		printf("Failed while modinit");
		return TEST_FAILED;
	}

	print_modules();

	log_modclear();
	print_modules();

	printf("macro test\n");

#undef  MODNAME
#define MODNAME		"TST"

	loginit("log7.log", false,
				MODULES( MOD(TST, LOGALL),
						 MOD(DBG, LOGALL),
						 MOD(QWERTY, LOGOFF),
						 MOD(AZERTY, LOGOFF)
					   ),
				"init... %s", "------");

	print_modules();

	logclose("end of %s", __func__);
	print_modules();
	return TEST_MANUAL;	// ???? TODO:: Manual?
}

// ------------------------ TEST 8 ----------------------------
// Level of loging testing
static TestStatus
f8(const char *name)
{
	printf("%s - %s: Level of loging testing\n", name, __func__);

#undef 	 	MODNAME
#define  	MODNAME  "TEST8"

	{
		loginit("log8.log", false,
				MODULES( MOD(TEST8, LOGALL)), "Init... LOGALL %s", __func__);

		logmsg("test message LOGALL");
		// fprintf(log_file(), "test!!!!!\n");
		logclose("end LOGALL...");
	}
	{
		loginit("log8.log", true,
                MODULES( MOD(TEST8, LOGOFF)), "Init... NOLOG %s", __func__);	// TODO: возможно сделать, что-бы можно было собрать файл с уровнем логирования не более заданного

		//fprintf(log_file(), "test 2222222!!!!!\n");
		logmsg("test message NOLOG");
		logclose("end NOLOG...");
	}

	printf("end of 8-th test\n");

	return TEST_PASSED;
}

// ------------------------ TEST 9 ----------------------------
// Default module logging test
static TestStatus
f9(const char *name)
{
#undef  MODNAME
#define MODNAME		DEFAULT_MOD

	printf("%s - %s: Default module logging test\n", name, __func__);

	loginit("log9.log", false, 0, "Init %s...", __func__);
	logmsg("Bla bla bla default %c", '-');
	logclose("end DEFAULT test");

	printf("end of %s\n", __func__);

    return TEST_PASSED;
}

// ------------------------ TEST 10 ----------------------------
// Typed log test
static TestStatus
f10(const char *name)
{
	printf("%s - %s: Typed log test\n", name, __func__);

	loginit("log10.log", false, 0, "typed log test...");

	const int 	i = 9;
	logauto(i);		// formely logtyped
	logauto(9);
	logauto(i+1.1);
	logauto("hello world!");

	logclose("done...");

	return TEST_PASSED;
}

// ------------------------ TEST 11 ----------------------------
// Program switch off/on test
static TestStatus
f11(const char *name)
{
	printf("%s - %s: Program switch off/on test\n", name, __func__);

	loginit("log11.log", false, 0, "Program switch off/on test...");

	logmsg("log before");
	logoff();

	const int MAX_ITER = 1000;
	for (int i = 0; i < MAX_ITER; i++)
	{
		if (i == MAX_ITER - 5)
			logon();
		logmsg("iter %d", i);
	}
	logmsg("log after");

	logclose("done...");

	return TEST_MANUAL;
}

// ------------------------ TEST 12 ----------------------------
// *_ap group test

static void
f12_local_ap_simpleact(const char *msg, va_list ap)
{
	logenter("...");
	logsimpleact_ap(printf("ACTION from logsimpleact_ap\n"), msg, ap);
	logret(0, "leave logsimpleact_ap");
}

static void
f12_local_ap_simple(const char *msg, va_list ap)
{
	logenter("...");
	logsimple_ap(msg, ap);
	logret(0, "leave logsimple_ap");
}

static void
f12_local_ap_msg(const char *msg, va_list ap)
{
	logenter("...");

	logmsg_ap(msg, ap);

	logret(0, "leave logmsg_ap");	// logret ?
}

static void
f12_local_ap_act(const char *msg, va_list ap)
{
	logenter("...");

	logact_ap(printf("ACTION from logact_ap\n"), msg, ap);

	logret(0, "leave logact_ap");
}

static void
f12_local_launcher(const char *msg, ...)
{
    logenter("........");
    va_list ap1, ap2, ap3, ap4;

    logmsg("try logsimple_ap");
    va_start(ap1, msg);
    f12_local_ap_simple(msg, ap1);

    logmsg("try log_ap");
    va_start(ap2, msg);
    f12_local_ap_msg(msg, ap2);

    logmsg("try logsimpleact_ap");
    va_start(ap3, msg);
    f12_local_ap_simpleact(msg, ap3);

    logmsg("try logact_ap");
    va_start(ap4, msg);
    f12_local_ap_act(msg, ap4);

    logret(0, "done");
}


// ------------------------ TEST 12 ----------------------------
// *_ap group test
static TestStatus
f12(const char *name)
{
	printf("%s - %s: *_ap group test\n", name, __func__);

	loginit("log12.log", false, 0, " *_ap group test");
	log_format(LOG_FORMAT_ALL);

	f12_local_launcher("%d - %s, %c, %g", 10, "ertyu", 'y', 1.245);

	logclose("done...");

	return TEST_PASSED;
}

// ------------------------ TEST 13 ----------------------------
// Logger format test
static TestStatus
f13(const char * name)
{
	printf("%s - %s: Logger format test\n", name, __func__);

	loginits("log13.log", false, LOG_FORMAT_SIMPLE, 0, "simple format test");

	logmsg(".. simple....");

	log_format(LOG_FORMAT_ONLY_TIME);

	logmsg("...only time ...");

	log_format(LOG_FORMAT_ONLY_FILE);

	logmsg(".. only file ..");

	log_format(LOG_FORMAT_EMPTY);

	logmsg("...empty ..");

	log_format(LOG_FORMAT_ALL);

	logmsg("...all ..");

	logclose("...");

	return TEST_PASSED;
}

// ------------------------ MAIN -------------------------------
int
main(int argc, char *argv[])
{
	printf("Exec %d %s\n", argc, argv[0]);

	testenginestd(testnew(.f2 = f1      , .num = 1      , .name = "Loginit/close test"			, .mandatory = true)
				, testnew(.f2 = f2      , .num = 2      , .name = "Log_msg test"				, .mandatory = true)
				, testnew(.f2 = f3      , .num = 3      , .name = "Enclosure test"        		, .mandatory = true)
				, testnew(.f2 = f4      , .num = 4      , .name = "Common macro test"        	, .mandatory = true)
				, testnew(.f2 = f5      , .num = 5      , .name = "Enclosure macro test"        , .mandatory = true)
				, testnew(.f2 = f6      , .num = 6      , .name = "Interrupted macro test"      , .mandatory = true)
				, testnew(.f2 = f7      , .num = 7      , .name = "Module init testing"        	, .mandatory = true)
				, testnew(.f2 = f8      , .num = 8      , .name = "Level of loging testing"     , .mandatory = true)
				, testnew(.f2 = f9      , .num = 9      , .name = "Default module logging test" , .mandatory = true)
				, testnew(.f2 = f10     , .num = 10     , .name = "Typed log test"        		, .mandatory = true)
				, testnew(.f2 = f11     , .num = 11     , .name = "Program switch off/on test"  , .mandatory = true)
				, testnew(.f2 = f12     , .num = 12     , .name = "*_ap group test"        		, .mandatory = true)
				, testnew(.f2 = f13     , .num = 13     , .name = "Logger format test"        	, .mandatory = true)
	);

}

#endif /* LOGTESTING */

