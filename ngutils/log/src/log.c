/**
 * @file log.c
 * @brief Implementation of the file-based logging engine declared in @ref log.h.
 *
 * Maintains a single global log FILE*, a sorted module-level table,
 * and a recursive indentation offset. All public functions are thread-unsafe
 * by design (single-writer assumption).
 */

#include "log.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

/* ---------------------------------------------------------------------------------

					GENERAL LOGGING MODULE
API:
	loginit
	logclose
	logenter
	logret
	logerr
---------------------------------------------------------------------------------- */

// ── Tunables ─────────────────────────────────────────────────────────────────

/** Format string for the module-count line in saved files. */
#define 		TOTAL_MOD		"Total modules[%d]\n"

/** Format string for one module entry in saved files (output). */
#define 		MODULE_DESC		"Modname(%d) %s: level[%d]\n"

/** Input variant with width-limited %19s to prevent buffer overflow on read. */
#define 		MODULE_DESC_IN	"Modname(%d) %19s: level[%d]\n"

/** Maximum length of the log file name buffer. */
enum { LOG_MAX_SZ = 4096 };			// better to use POSIX or SC limitation instead of this constant

/** Buffer size for the time string. */
enum { LOG_TIME_SZ = 100 };

/** Spaces added per indent level. */
static			const int		OFFSET_INC			= 4;
static 			const int 		LOG_MAXMODS 		= 4096;

// ── Global state ─────────────────────────────────────────────────────────────

/** Current indentation offset (in spaces). */
static			int				g_offset			=0;				// current logging offset

/** Increment per indent level. */
static			int				g_offset_inc		=OFFSET_INC;	// currect offset increment

/** Current log file name. */
static			char			g_logname[LOG_MAX_SZ]	="default.log";	// current  logging file name

/** Open log stream (NULL = not initialised). */
static			FILE		   *g_logfile			=0;				// current logging file (init on 1 exec if null)

/** Reusable time-string buffer (avoids malloc per call). */
static			char			g_time_buf[LOG_TIME_SZ];				// buffer for time print

/** Number of entries in g_modules (including the sentinel). */
static 			int				g_modcount			= 1;						// count of registered modules

/** Fallback module used when no explicit list is loaded. */
static          LogModlevel		def_mod				= MOD(DEFAULT_MOD, LOGALL); // structure for default module  TODO: replace to char * later!

/** Pointer to the active module array (either &def_mod or heap). */
static      	LogModlevel	   *g_modules			= &def_mod;				// array of registered modules

/** Global on/off switch. */
static			bool			g_logon_mode		= true;				// program switch on/off mode

/** Current preambule format (TODO: should be per-module). */
static			LogFormat		g_format_schema		= LOG_FORMAT_ALL;	// TODO: it should be for module, but not general

// ── Internal utilities ───────────────────────────────────────────────────────

/**
 * @brief Fill @a g_time_buf with "HH:MM:SS" and return it.
 * @return Pointer to the internal buffer (valid until next call).
 */
static inline const char *
print_time(void)
{
	time_t t = time(0);
	struct tm *now = localtime(&t);
	strftime(g_time_buf, LOG_TIME_SZ, "%H:%M:%S", now);
	return g_time_buf;
}

/**
 * @brief Comparator for bsearch/qsort on LogModlevel by module name.
 * @return <0, 0, or >0 in strcmp fashion.
 */
static int
cmp(const void *v1, const void *v2)
{
	const LogModlevel *m1 = v1, *m2 = v2;
	return strcmp(m1->module, m2->module);
}

/**
 * @brief Look up a module's level by name.
 * @param module  Module name to search for.
 * @return The module's Loglevel, or -1 if not found.
 * @note Requires g_modules to be initialised and sorted.
 */
static int
getlevel(const char *module)
{
	LogModlevel tmp;
	memset(&tmp, 0, sizeof(tmp));
	strncpy(tmp.module, module, MAX_MODULE - 1);
	LogModlevel *m = bsearch(&tmp, g_modules, g_modcount, sizeof(LogModlevel), cmp);
	if (m)
		return (int)m->level;
	else
		return -1;
}

/**
 * @brief Reset the module table to the built-in default.
 * @note Does NOT free g_modules — caller must call log_modclear() first
 *       if the array is heap-allocated.
 */
static void
resetmod(void)
{
	g_modules = &def_mod;		//  what about free???
	g_modcount = 1;
}

/**
 * @brief Emit the preambule (indent + file/func/line + action tag + time).
 *
 * Also adjusts @a g_offset based on @a act (ENTER increments,
 * LEAVE/ERR decrements). Returns false (and skips the message) when
 * the module level is insufficient or logging is switched off.
 *
 * @param act       LogAction flags (LOG_SIMPLE / LOG_NONEWLINE already stripped).
 * @param lv        Level multiplier (interpreted per act type).
 * @param msglevel  Minimum level required (currently always LOGALL).
 * @param modname   Module name.
 * @param filename  Source file.
 * @param funcname  Function name.
 * @param lineno    Line number.
 * @return true if the message should be printed, false to suppress.
 */
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

	if (getlevel(modname) < msglevel || g_logfile == NULL) {   // no logging in this case			// TODO: не используется сейчас по факту, мб неверно и нужно рассчитывать корректный lg_lv
        fprintf(stderr, "Mod %s NOT FOUND!!!\n", modname);
		return false;
	}

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
			fprintf(stderr, "Preambule: Incorrect act = [%d]\n", act);
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

// ── Module list management ───────────────────────────────────────────────────

/**
 * @brief Register a null-sentinel-terminated module list.
 *
 * Allocates a private copy (sorted by name). Fails if modules are already
 * loaded (call log_modclear() first) or if the list is empty.
 *
 * @param modlist  Array of LogModlevel terminated by level == _LOGSTOP.
 * @return true on success.
 */
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

	// determine size (bounded to prevent OOB if sentinel is missing)
	while (cnt < LOG_MAXMODS && modlist[cnt].level >= 0)
		cnt++;

	if (cnt == 0 || cnt >= LOG_MAXMODS)	// invalid: empty or no sentinel found
		return false;

	if (!(g_modules = malloc((size_t)cnt * sizeof(LogModlevel))))
	{
		fprintf(out, "Unable to allocate memory for module list (%zu)\n", (size_t)cnt * sizeof(LogModlevel));
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

/**
 * @brief Free the heap-allocated module list and restore the default.
 * Safe to call when already in default state.
 */
void
log_modclear(void)
{
	if (g_modules != &def_mod)
		free(g_modules);
    resetmod();
}

/**
 * @brief Write the current module table to a text file.
 * @param name  Destination path.
 * @return true on success.
 */
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

/**
 * @brief Load a module table previously saved by log_modsave().
 *
 * Replaces any existing heap-allocated list. On parse error the table
 * is rolled back to the default.
 *
 * @param name  Source path.
 * @return true on success.
 */
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

	log_modclear(); // free old heap array + reset g_modcount
	
	if (modcnt < 1 || modcnt > LOG_MAXMODS)
	{
		fprintf(stderr, "Bad module count %d\n", modcnt);
		fclose(f);
		return false;
	}
	if (!(g_modules = malloc((size_t)modcnt * sizeof(LogModlevel))))
    {

        fprintf(stderr, "Unable to allocate memory for module list (%zu)\n", (size_t)modcnt * sizeof(LogModlevel));
		fclose(f);
        return false;
    }
	for (int i = 0; i < modcnt; i++)
	{
		int	lv, idx;
		if (fscanf(f, MODULE_DESC_IN, &idx, g_modules[i].module, &lv) < 3 || lv > (int)LOGALL || lv < (int)LOGOFF)
		{
			fprintf(stderr, "Wrong input in line %d (by pattern [%s])\n", i + 1, MODULE_DESC);
			fclose(f);
			log_modclear();
			return false;
		}
		g_modules[i].level = lv;
	}
	g_modcount = modcnt;
	g_logon_mode = true;
	qsort(g_modules, g_modcount, sizeof(LogModlevel), cmp);
    fclose(f);

	return true;
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

/**
 * @brief Open the log file and configure buffering/format.
 *
 * Sets the stream to unbuffered (_IONBF). Safe to call multiple times —
 * subsequent calls are no-ops if already initialised.
 *
 * @param logname   File path (NULL = use current g_logname).
 * @param append    true = append mode ("a"), false = truncate ("w").
 * @param logformat Preambule format to apply.
 * @return true on success.
 */
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
		strncpy(g_logname, logname, LOG_MAX_SZ-1);
		g_logname[LOG_MAX_SZ - 1] = '\0';	// in case of overflow
	}
	if ((g_logfile = fopen(g_logname, append? "a" : "w"))==0)
		return false;

	log_format(logformat);

	if (setvbuf(g_logfile, 0, _IONBF, 0) != 0)		// unable to setup unbuf mode, put warning
		fprintf(g_logfile, "WARNING: unable to setup unbuffered mode!\n");
	return true;
}

/** @return true if the log file is currently open. */
bool
log_isinit(void)
{
	return g_logfile != 0;
}

/** @return The active FILE* stream, or NULL if not initialised. */
FILE *
log_file(void)
{
	return g_logfile;
}

/**
 * @brief Close the log file, reset format, and free the module table.
 * Safe to call when not initialised.
 */
void
log_close(void)
{
	// close logfile
	if (g_logfile)
		fclose(g_logfile);
	g_logfile = 0;			// must be 0 when not initialized
	g_format_schema = LOG_FORMAT_ALL;
	g_offset = 0;			// reset indentation
	g_offset_inc = OFFSET_INC;
	// free modules
	log_modclear();
	*g_logname = '\0';
}

/**
 * @brief Current indentation offset (in spaces).
 * @return Number of spaces to prefix the next message.
 */
int
log_offset(void)
{
	return g_offset;
}

/**
 * @brief Globally enable or disable all logging.
 * @param logon_mode  true = on, false = off.
 * @return The previous state (as int).
 */
int
log_prog_switch(bool logon_mode)
{
	int prev = g_logon_mode;
    g_logon_mode = logon_mode;
    return prev;}

/**
 * @brief Set the global preambule format.
 * @param lf  Desired LogFormat.
 * @return true if accepted, false if out of range.
 */
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


// ── Core message functions ───────────────────────────────────────────────────

/**
 * @brief Emit one formatted log message (printf-style variadic).
 *
 * Convenience wrapper around @ref log_msg_ap that starts and ends the
 * va_list automatically.
 *
 * @param act       LogAction flags.
 * @param lv        Level / indent hint.
 * @param msglevel  Minimum level (always LOGALL for now).
 * @param modname   Module name.
 * @param filename  `__FILE__`.
 * @param funcname  `__func__`.
 * @param lineno    `__LINE__`.
 * @param msg       printf format string.
 * @param ...       Format arguments.
 * @return Current g_offset on success, -1 on I/O failure.
 */
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
	int ret = log_msg_ap(act, lv, msglevel, modname, filename, funcname, lineno, msg, ap);
    va_end(ap);
    return ret;
}

/**
 * @brief Emit one formatted log message (va_list variant).
 *
 * This is the single real log-output function. It:
 *  1. Auto-initialises the log file if not yet open.
 *  2. Calls @ref log_preambule (which may suppress the message).
 *  3. Calls vfprintf with the user format.
 *  4. Appends a newline unless LOG_NONEWLINE is set.
 *
 * @param act       LogAction flags.
 * @param lv        Level / indent hint.
 * @param msglevel  Minimum level (always LOGALL for now).
 * @param modname   Module name.
 * @param filename  `__FILE__`.
 * @param funcname  `__func__`.
 * @param lineno    `__LINE__`.
 * @param msg       printf format string (may be NULL).
 * "@param ap        Pre-started va_list; caller is responsible for va_end().
 * @return Current g_offset on success, -1 on failure.
 */
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

	bool    no_newline 		= (act & LOG_NONEWLINE) != 0;
	act	&= ~LOG_NONEWLINE;

	if (!log_preambule(act, lv, msglevel, modname, filename, funcname, lineno))
		return g_offset;

	if (msg) // print user message if any
		vfprintf(g_logfile, msg, ap);

	if (!no_newline)
		fputc('\n', g_logfile);		// goto next line

	return g_offset;
}

/**
 * @brief Dump a raw byte buffer to the log (hex-ish printable dump).
 *
 * Prints digits as-is, other bytes as characters. Always appends a newline.
 *
 * @note Uses isdigit() for a simple printable-digit filter.
 *
 * @param act       LogAction flags.
 * @param lv        Level / indent hint.
 * @param msglevel  Minimum level.
 * @param modname   Module name.
 * @param filename  `__FILE__`.
 * @param funcname  `__func__`.
 * @param lineno    `__LINE__`.
 * @param bytes     Pointer to the buffer to dump.
 * @param sz        Number of bytes.
 * @return sz+1 on success, -1 if preambule suppressed the output.
 */
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
		unsigned char 	c = bytes[i];
		if (c <= 9)
			putc(c + '0', g_logfile);
		else
			putc(c, g_logfile);
	}
	putc('\n', g_logfile);
	return sz + 1;
}
