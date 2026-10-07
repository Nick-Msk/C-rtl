/**
 * @file log.h
 * @brief Lightweight file-based logging engine with per-module level control.
 *
 * Provides a printf-style logging API with:
 *  - Per-module log-level filtering
 *  - Configurable output format (file/func/time)
 *  - Indentation-aware messages (logenter / logret / logerr)
 *  - "Simple" (unpreambled) logging for one-liners
 *  - Auto-formatting via _Generic (logauto / logautoret)
 *  - Compile-time removal via -DNODEBUG
 *
 * Usage skeleton:
 * @code
 *   LOG(myapp);                          // init in main()
 *   ...
 *   logenter("starting subsystem %s", name);
 *   ...
 *   logret(0, "done, count=%d", n);     // or: return logret(...)
 *   ...
 *   logclose("bye");
 * @endcode
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

/** Maximum length of a module name (NUL-terminated). */
enum { MAX_MODULE = 64 };		// for gcc

/**
 * @brief Log severity / enable levels.
 *
 * A module whose level is below the effective (lv × g_offset_inc)
 * of a message will suppress that message.
 */
typedef enum {
	LOGOFF					= 0,		/**< Module fully silent. */
	LOGERR					= 1,		/**< Errors only (reserved, not used yet) */
	LOGWARN					= 2,		/**< Warnings + errors (reserved, not used yet). */
	LOGALL					= 3,		/**< Everything. */
	_LOGSTOP				= -1		/**< Sentinel terminating the module list; internal only. */
} Loglevel;

/**
 * @brief Flags describing *what* a log call is and *how* it formats.
 *
 * Combine with bitwise OR: `LOG_ENTER | LOG_SIMPLE | LOG_NONEWLINE`.
 */
typedef enum {
	LOG_ENTER				= 0		/**< Function entry (increments indent). */
  , LOG_LEAVE						/**< Normal return (decrements indent). */
  , LOG_ERR							/**< Error / failure path (decrements indent). */
  , LOG_MSG							/**< Mid-function informational message. */
  , LOG_SIMPLE				= 0x10 	/**< Suppress preambule & indent (one-liner). */
	// new line policy
  , LOG_NOPREAMBULE			= 0x20	/**< Do not print the `[file:func:line]` prefix. */
  , LOG_NONEWLINE			= 0x40	/**< Do not print trailing newline. */
} LogAction;

typedef enum {
	LOG_FORMAT_EMPTY		= 0			/**< Nothing — just the message. */
  , LOG_FORMAT_ALL						/**< File + func + line. */
  , LOG_FORMAT_ONLY_FILE				/**< File only. */
  , LOG_FORMAT_ONLY_FUNC				/**< Function only. */
  , LOG_FORMAT_SIMPLE					/**< Minimal (func). */
  , LOG_FORMAT_ONLY_TIME				/**< Minimal + timestamp. */
} LogFormat;

/**
 * @brief Pair of module name and its log level.
 */
typedef struct {
	char 		module[MAX_MODULE];		/**< Module name (max MAX_MODULE).  */
	Loglevel	level;					/**< Level filter for this module. */
} LogModlevel;

/**
 * @brief Convert a Loglevel to a human-readable string.
 * @param lv  Level to convert.
 * @return Static string (e.g. `"LOGALL"`), never NULL.
 */
static inline const char *
log_levelstr(Loglevel lv)
{
	switch (lv)
	{
		case LOGOFF: 	return "LOGOFF";
		case LOGERR: 	return "LOGERR";
		case LOGWARN: 	return "LOGWARN";
		case LOGALL:	return "LOGALL";
		default:		return "Unknown log level";
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Open the log file and set global format.
 * @param logname   Path to the log file.
 * @param append    true = append, false = truncate.
 * @param lformat   Preambule format to use.
 * @return true on success, false on failure.
 */
extern bool
log_init(const char *		  logname
	   , bool                 append
	   , LogFormat			  lformat
       );


/**
 * @brief Register the per-module level list.
 * @param modlist  Null-terminated (LOGOFF sentinel) array of LogModlevel.
 * @return true on success.
 */
extern bool
log_modinit(LogModlevel *modlist);

/**
 * @brief Persist the current module list to a text file.
 * @param name  Destination file path.
 * @return true on success.
 */
extern bool
log_modsave(const char *name);

/**
 * @brief Load the module list from a text file (replaces current list).
 * @param name  Source file path.
 * @return true on success.
 */
extern bool
log_modload(const char *name);

/** @return true if log_init() was called successfully. */
extern bool
log_isinit(void);

/** @return The active log FILE* (or NULL if not initialised). */
extern FILE *
log_file(void);

/** Close the log file and release resources. Safe to call when not open. */
extern void
log_close(void);

/**
 * @brief Current indentation depth (in "symbols"/spaces).
 * @return Number of spaces to print before a message.
 */
extern int
log_offset(void);

/**
 * @brief Globally switch logging on/off at runtime.
 * @param logon_mode  true = enable, false = silence everything.
 * @return 0 on success.
 */
extern int
log_prog_switch(bool logon_mode);

/**
 * @brief Change the global output format.
 * @todo Should be per-module, not global.
 * @param fmt  New LogFormat.
 * @return true on success.
 */
extern bool
log_format(LogFormat);

// ─────────────────────────────────────────────────────────────────────────────
//  Core log functions
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Emit one formatted log message.
 *
 * The effective severity is `lv * g_offset_inc`; if the module's level
 * is lower, the message is suppressed.
 *
 * @param act       LogAction flags (LOG_ENTER, LOG_LEAVE | LOG_SIMPLE, …).
 * @param lv        Raw level multiplier (0 = info, >0 = deeper).
 * @param msglevel  Reserved; always LOGALL in current builds.
 * @param modname   Module name (see @ref MODNAME).
 * @param filename  `__FILE__`.
 * @param funcname  `__func__`.
 * @param lineno    `__LINE__`.
 * @param fmt       printf-style format string.
 * @param ...       Format arguments.
 * @return 0 on success, negative errno-like on I/O failure.
 */
extern
int
log_msg(LogAction            act,
        int                  lv,         // level of logging (must be mutlipied to g_offset_inc to make effective level)
	    Loglevel             msglevel,          // level of logging type, always = LOGALL now
	    const char *restrict modname,
	    const char *restrict filename,
	    const char *restrict funcname,
	    int                  lineno,
	    const char *restrict msg,
	    ...
       ) __attribute__ ((format (printf, 8, 9)));

/**
 * @brief va_list variant of @ref log_msg.
 * Caller must `va_end(ap)` if not consumed by the engine.
 */
extern
int
log_msg_ap(LogAction            act,
           int                  lv,         // level of logging (must be mutlipied to g_offset_inc to make effective level)
    	   Loglevel             msglevel,          // level of logging type, always = LOGALL now
		   const char *restrict modname,
           const char *restrict filename,
           const char *restrict funcname,
           int                  lineno,
           const char *restrict msg,
           va_list			    ap
           );

/**
 * @brief Dump a raw byte buffer in hex to the log.
 * @param act       LogAction flags.
 * @param lv        Level multiplier.
 * @param msglevel  Reserved.
 * @param modname   Module name.
 * @param filename  `__FILE__`.
 * @param funcname  `__func__`.
 * @param lineno    `__LINE__`.
 * @param bytes     Pointer to the buffer.
 * @param sz        Number of bytes to print.
 * @return 0 on success.
 */
extern int
log_numbers(LogAction            act,
       	    int                  lv,         // level of logging (must be mutlipied to g_offset_inc to make effective level)
            Loglevel             msglevel,          // level of logging type, always = LOGALL now
            const char *restrict modname,
            const char *restrict filename,
         	const char *restrict funcname,
         	int                  lineno,
		 	const char *restrict bytes,
		 	int 				  sz

);

// ─────────────────────────────────────────────────────────────────────────────
//  Debug macros (compiled out with -DNODEBUG)
// ─────────────────────────────────────────────────────────────────────────────

#ifndef NODEBUG

/** @name Interface macros overview
 *  @{
 *
 * | Macro              | Purpose                         | Returns |
 * |--------------------|---------------------------------|---------|
 * | `logenter(fmt,..)`| Declare `_LG_LV` at entry       | int (0) |
 * | `logret(rc, fmt..)`| Normal return                  | rc      |
 * | `logerr(rc, fmt..)`| Error return                   | rc      |
 * | `logmsg(fmt, ...)`| Mid-function info               | 0       |
 * | `logact(A, fmt..)` | Perform A, log, return 0      | 0       |
 * | `logsimple(...)`  | One-liner, no preambule        | 0       |
 * | `logauto(val)`    | Auto-format via `_Generic`     | 0       |
 * | `logautoret(val)` | Auto-format + return val       | val     |
 *  @}
 */

#define _LG_LV _LG_LV

/** @brief Core: call log_msg with standard preambule args. */
#define _LOG_MSG(type, lglv, fmt, ...) log_msg((type), (lglv), LOGALL, MODNAME, __FILE__ , __func__, __LINE__, ( fmt ), ##__VA_ARGS__ )

/** @brief Core: va_list variant of _LOG_MSG. */
#define _LOG_MSG_AP(type, lglv, fmt, ap) log_msg_ap((type), (lglv), LOGALL, MODNAME, __FILE__ , __func__, __LINE__, ( fmt ), (ap))

/**
 * @brief Core: log then perform ACTION and yield retcode (GNU statement
 *        expression — usable in `return _LOG_MSG_RET(...)`).
 * @param ACTION   Statement executed after logging (e.g. `free(p);`).
 * @param type     LogAction.
 * @param retcode  Value to return/yield.
 * @param lglv     Level (usually `_LG_LV`).
 */
#define _LOG_MSG_RET(ACTION, type, retcode, lglv, fmt, ...) \
		({\
		   _LOG_MSG(type, lglv, fmt, ##__VA_ARGS__);\
		   ACTION;\
		  (retcode);\
		})

/** @brief va_list variant of _LOG_MSG_RET. */
#define _LOG_MSG_RET_AP(ACTION, type, retcode, lglv, fmt, ap) \
		({\
		     _LOG_MSG_AP(type, lglv, fmt, ap);\
		     ACTION;\
		     (retcode);\
		})

/** @brief Dump raw bytes with simple formatting. */
#define lognumbers(bytes, sz)				log_numbers(LOG_MSG | LOG_SIMPLE, 0, LOGALL, MODNAME, __FILE__ , __func__, __LINE__, bytes, sz)

// ── Entry / return / error ────────────────────────────────────────────────────

/**
 * @brief Mark function entry; defines local `_LG_LV` for later use.
 * Must be the first statement in a function that uses logret/logerr.
 */
#define logenter(fmt, ...) 		int _LG_LV = 					_LOG_MSG(LOG_ENTER, 0	 							, fmt, ##__VA_ARGS__)

/**
 * @brief Generic typed return: log, then yield retcode.
 * @param type      LOG_LEAVE or LOG_ERR.
 * @param retcode   Value to return.
 */
#define logtype(type, retcode, fmt, ...) 	 					_LOG_MSG_RET(, type, retcode, _LG_LV 				, fmt, ##__VA_ARGS__)		// TODO: rework implementaion!!!

/** @brief va_list variant of logtype. */
#define logtype_ap(type, retcode, fmt, ap)						_LOG_MSG_RET_AP(, type, retcode, _LG_LV             , fmt, ap)

/** @brief Normal return with message.  Usage: `return logret(0, "ok");` */
#define logret(retcode, fmt, ...)            					logtype(LOG_LEAVE, retcode         					, fmt, ##__VA_ARGS__)

/** @brief Error return with message.  Usage: `return logerr(-1, "fail: %s", msg);` */
#define logerr(retcode, fmt, ...)            					logtype(LOG_ERR, retcode 							, fmt, ##__VA_ARGS__)

/** @brief Mid-function informational message (no indent change). */
#define logmsg(fmt, ...)                	 					logtype(LOG_MSG, 0 									, fmt, ##__VA_ARGS__)

/** @brief va_list variant of logmsg. */
#define	logmsg_ap(fmt, ap)										logtype_ap(LOG_MSG, 0								, fmt, ap)

// ── With action (cleanup before return) ───────────────────────────────────────

/** @brief Log error, perform ACTION (cleanup), yield retcode. */
#define logacterr(ACTION, retcode, fmt, ...)					_LOG_MSG_RET(ACTION, LOG_ERR, retcode, _LG_LV		, fmt, ##__VA_ARGS__)

/** @brief Log normal leave, perform ACTION (cleanup), yield retcode. */
#define logactret(ACTION, retcode, fmt, ...)   					_LOG_MSG_RET(ACTION, LOG_LEAVE, retcode, _LG_LV		, fmt, ##__VA_ARGS__)

/** @brief Perform ACTION, log message, return 0. */
#define	logact(ACTION, fmt, ...)								_LOG_MSG_RET(ACTION, LOG_MSG, 0, _LG_LV				, fmt, ##__VA_ARGS__)	// TODO: need to be refactored to avoid using log_msg_ret

/** @brief va_list variant of logact. */
#define logact_ap(ACTION, fmt, ap)								_LOG_MSG_RET_AP(ACTION, LOG_MSG, 0, _LG_LV          , fmt, ap)

//#define logmsg(fmt, ...) _LOG_MSG_RET(LOG_MSG, 0, _LG_LV, fmt, ...)

/** @brief Simple one-liner + ACTION (cleanup), no indent/preambule. */
#define logsimpletype_act(ACTION, type, retcode, fmt, ...)		_LOG_MSG_RET(ACTION, type | LOG_SIMPLE, retcode, 0	, fmt, ##__VA_ARGS__)

/** @brief Simple typed one-liner with va_list. */
#define logsimpletype_act_ap(ACTION, type, retcode, fmt, ap)	_LOG_MSG_RET_AP(ACTION, type | LOG_SIMPLE, retcode, 0  , fmt, ap)

/** @brief Simple typed one-liner (generic): no preambule, no indent. */
#define logsimpletype(type, retcode, fmt, ...)                  logsimpletype_act(, type, retcode					, fmt, ##__VA_ARGS__)

/** @brief One-liner: no preambule, no indent. Returns 0. */
#define logsimple(fmt, ...)                  					logsimpletype(LOG_MSG, 0							, fmt, ##__VA_ARGS__)		// TODO: подумать об объединении с logmsg, можно ли обойти проблему  _LG_LV??

/** @brief va_list variant of logsimple. */
#define logsimple_ap(fmt, ap)									logsimpletype_act_ap(, LOG_MSG, 0                   , fmt, ap)

/** @brief Simple error one-liner. */
#define logsimpleerr(retcode, fmt, ...)		 					logsimpletype(LOG_ERR  , retcode					, fmt, ##__VA_ARGS__)

/** @brief Simple leave one-liner. */
#define logsimpleret(retcode, fmt, ...)	     					logsimpletype(LOG_LEAVE, retcode					, fmt, ##__VA_ARGS__)

/** @brief Simple leave one-liner with ACTION. */
#define logsimpleacterr(ACTION, retcode, fmt, ...)				logsimpletype_act(ACTION, LOG_ERR, retcode			, fmt, ##__VA_ARGS__)

/** @brief Simple informational one-liner with ACTION. */
#define logsimpleactret(ACTION, retcode, fmt, ...)              logsimpletype_act(ACTION, LOG_LEAVE, retcode		, fmt, ##__VA_ARGS__)

/** @brief Simple informational one-liner with ACTION. */
#define	logsimpleact(ACTION, fmt, ...)							logsimpletype_act(ACTION, LOG_MSG, 0				, fmt, ##__VA_ARGS__)

/** @brief va_list variant of logsimpleact. */
#define logsimpleact_ap(ACTION, fmt, ap)						logsimpletype_act_ap(ACTION, LOG_MSG, 0             , fmt, ap)

// ── Auto-format via _Generic ─────────────────────────────────────────────────

/**
 * @brief Selects a printf format specifier based on the type of @p x.
 *
 * Supports all scalar types, pointers, and char*.
 * Used internally by logautotype / logauto / logautoret / logautoerr.
 */
#define LOGTYPEFORMAT(x) _Generic((x), \
	bool					: "%d", \
    char                    : "%c", \
    signed char             : "%hhd", \
    unsigned char           : "%hhu", \
    signed short            : "%hd", \
    unsigned short          : "%hu", \
    signed int              : "%d", \
    unsigned int            : "%u", \
    long int                : "%ld", \
    unsigned long int       : "%lu", \
    long long int           : "%lld", \
    unsigned long long int  : "%llu", \
    float                   : "%f", \
    double                  : "%f", \
    long double             : "%Lf", \
    char *                  : "%s", \
    const char *            : "%s", \
    void *                  : "%p",\
    const void *            : "%p" \
    )

/**
 * @brief Print `name = value` using auto-selected format.
 * Emits two lines: a label (no preambule) and the value (no preambule).
 * @param type  LogAction (LOG_MSG, LOG_LEAVE, LOG_ERR, …).
 * @param val   Any scalar / pointer / string.
 */
#define logautotype(type, val)							{ logsimpletype(type | LOG_NONEWLINE, 0, "%s = ", #val); \
                                                          logsimpletype(type | LOG_NOPREAMBULE, 0, LOGTYPEFORMAT(val), val); }

/** @internal Helper: log autotype then yield val. */
#define logautoret_err(type, val)						({ logautotype(type, val); (val); })

/**
 * @brief Auto-format and log a value (mid-function info). Returns 0.
 * @param val  Any scalar / pointer / string.
 */
#define logauto(val) 									logautotype(LOG_MSG, val)
/**
 * @brief Auto-format, log, and return the value.
 * Usage: `return logautoret(ptr);`
 */

#define logautoret(val) 								logautoret_err(LOG_LEAVE, val)
/**
 * @brief Auto-format, log as error, and return the value.
 * Usage: `return logautoerr(-1);`
 */
#define logautoerr(val)									logautoret_err(LOG_ERR  , val)


// ── Module list helpers ───────────────────────────────────────────────────────

/**
 * @brief Declare one module entry for a MODULES list.
 * @param name  Module name (stringified).
 * @param lv    Loglevel (LOGALL, LOGOFF, …).
 */
#define MOD(name, lv)	{ .module = (#name), .level = (lv) }

/**
 * @brief Build a null-sentinel-terminated LogModlevel array.
 * @param ...  One or more MOD(name, lv) entries.
 *
 * @code
 *   MODULES(MOD(parser, LOGALL), MOD(io, LOGOFF))
 * @endcode
 */
#define MODULES(...) \
 	(LogModlevel []) { \
	__VA_ARGS__ \
	, MOD (, _LOGSTOP ) \
	}

// ── Initialisation macros ─────────────────────────────────────────────────────

/**
 * @brief Full init: open file, register modules, log entry line.
 * @param logname   File path.
 * @param append    true = append mode.
 * @param format    LogFormat.
 * @param modules   MODULES(...) array.
 * @param fmt       printf format for the "start" line.
 * @param ...       Args for fmt.
 */
#define loginits(logname, append, format, modules, fmt, ...) \
	log_init( (logname), (append), (format));\
	if (!log_modinit(modules))\
		fprintf(stderr, "Unable to init module list");\
 	logenter(fmt, ##__VA_ARGS__);

/**
 * @brief Init with LOG_FORMAT_ONLY_FUNC (convenience).
 * @param logname   File path.
 * @param append    true = append mode.
 * @param modules   MODULES(...) array.
 * @param fmt       printf format for the "start" line.
 */
#define loginit(logname, append, modules, fmt, ...) \
	loginits((logname), (append), LOG_FORMAT_ONLY_FUNC, (modules), (fmt), ##__VA_ARGS__)

/**
 * @brief Init log in a given directory; filename = `dirname/__FILE__.log`.
 * @param dirname  Directory (will be stringified).
 * @param fmt      printf format for the start line.
 */
#define logsimpleinitdir(dirname, fmt, ...)\
	loginit(#dirname"/"__FILE__".log", false, 0, (fmt), ##__VA_ARGS__)

/**
 * @brief Init log in `log/` directory (convenience for `logsimpleinitdir`).
 */
#define logsimpleinit(fmt, ...)\
    logsimpleinitdir(log, (fmt), ##__VA_ARGS__)

/**
 * @brief One-shot: init (truncate) + log "Start logging (cut)".
 * @param dirname  Directory string (NOT stringified — passed as literal).
 */
#define LOG(dirname)\
    loginit(dirname"/"__FILE__".log", false, 0, "Start logging (cut)")

/**
 * @brief One-shot: init (append) + log "Start logging (append)".
 * @note Known issue: does not work properly in all build configs yet.
 */
#define LOGAPPEND(dirname)\
    loginit(dirname"/"__FILE__".log", true, 0, "Start logging (append)")

// ── Close / convenience ───────────────────────────────────────────────────────

/**
 * @brief Log a "return" message, close the log file, yield retcode.
 * @param retcode  Value to return from main().
 * @param fmt      printf format for the final line.
 */
#define logcloseret(retcode, fmt, ...) \
	({logret( (retcode), (fmt), ##__VA_ARGS__); \
      log_close(); \
	  (retcode); })

/** @brief Log a "done" message and close the log file. */
#define logclose(fmt, ...) \
    logcloseret(0, (fmt), ##__VA_ARGS__);

// ── Convenience accessors ─────────────────────────────────────────────────────

/** @brief Shortcut for `log_file()`. */
#define	logfile								(log_file())

/** @brief Shortcut for `log_offset()`. */
#define logoffset							(log_offset())	

/**
 * @brief Print `log_offset()` spaces to the log stream.
 * @return Number of characters written.
 */
static inline int
logprintoffset(void) {
	int offset = log_offset();
	if (offset > 0)
		return fprintf(logfile, "%*s", offset, "");
	return 0;
}

/** @brief Silently disable all logging at runtime. */
#define logoff()                            log_prog_switch(false)

/** @brief Re-enable logging at runtime. */
#define logon()								log_prog_switch(true)

/** @brief Perform ACTION wrapped in an auto-log block. */
#define LOGAUTO(ACTION)						{ logauto(ACTION); }


// ── Module name defaults ──────────────────────────────────────────────────────

/** Default module name used when no specific module is set. */
#define MODNAME 							"DEFAULT_MOD"		// for using as 'default mode logging'
/** Alias for MODNAME. */
#define DEFAULT_MOD							"DEFAULT_MOD"

#else /* NODEBUG */

/** @brief In NODEBUG builds there is no module name. */
#define DEFAULT_MOD							""

/** @brief Module entry (retained for struct compatibility). */
#define MOD(name, lv)						{ .module = (#name), .level = (lv) }

/** @brief Init is a no-op, always returns false. */
#define loginit(...)						(false)				// in NODEBUG mode result is false

/** @brief Full init is a no-op in NODEBUG mode. */
#define loginits(...)

/** @brief Close is a no-op, returns 0. */
#define logclose(...)						(0)

/** @brief Yield retcode without logging. */
#define logcloseret(ret, ...)               (ret)

/** @brief No-op. */
#define logsimpleinitdir(...)

/** @brief No-op. */
#define logsimpleinit(...)

// ── Debug-mode stubs (return appropriate values, no I/O) ─────────────────────

/** @brief No-op, returns 0. */
#define logmsg(...)	 						(0)

/** @brief Consume ap, return 0. */
#define logmsg_ap(...)						({ va_end(ap); 0; })

/** @brief No-op (no _LG_LV in NODEBUG). */
#define logenter(...)

/** @brief Yield retcode. */
#define logtype(type, retcode, ...) 		(retcode)

/** @brief Yield retcode. */
#define logret(retcode, ...) 				(retcode)

/** @brief Yield retcode. */
#define logerr(retcode, ...) 				(retcode)
// with action
/** @brief Perform ACTION only. */
#define logact(ACTION, fmt, ...)			({ ACTION; })

/** @brief Consume ap, perform ACTION. */
#define logact_ap(ACTION, fmt, ap)          ({ va_end(ap); ACTION; })

/** @brief Perform ACTION, yield retcode. */
#define logactret(ACTION, retcode, ...)   	({ ACTION; (retcode); })

/** @brief Perform ACTION, yield retcode. */
#define logacterr(ACTION, retcode, ...)		({ ACTION; (retcode); })
// simple group

/** @brief No-op, returns 0. */
#define logsimple(...) 						(0)

/** @brief Consume ap, return 0. */
#define logsimple_ap(...)					({ va_end(ap); 0; })

/** @brief Yield retcode. */
#define logsimpleerr(retcode, ...) 			(retcode)

/** @brief Yield retcode. */
#define logsimpleret(retcode, ...) 			(retcode)

/** @brief Yield retcode. */
#define logsimpletype(type, retcode, ...) 	(retcode)

/** @brief Perform ACTION, yield retcode. */
#define logsimpleacterr(ACTION, retcode, fmt, ...) ({ ACTION; (retcode); })

/** @brief Perform ACTION, yield retcode. */
#define logsimpleactret(ACTION, retcode, fmt, ...) ({ ACTION; (retcode); })

/** @brief Perform ACTION only. */
#define logsimpleact(ACTION, fmt, ...)		({ ACTION; })

/** @brief Consume ap, perform ACTION. */
#define logsimpleact_ap(ACTION, fmt, ap)	({ va_end(ap); ACTION; })

// Auto group:
/** @brief No-op. */
#define logautotype(type, val)

/** @brief Yield val. */
#define logautoret_err(type, val)           (val)

/** @brief Yield val. */
#define logautoret(ret)						(ret)

/** @brief Yield ret. */
#define logautoerr(ret)						(ret)

// Accessors in NODEBUG:
/** @brief In NODEBUG, "log file" is NULL. */
#define logfile								(NULL)

/** @brief Always 0 offset. */
#define logoffset							(0)

/** @brief No-op. */
#define logprintoffset						(void) (0)

/** @brief No-op. */
#define logoff()							(void) (0)

/** @brief No-op. */
#define	logon()								(void) (0)

/** @brief No-op. */
#define LOG(...)

/** @brief No-op. */
#define LOGAPPEND(...)

/** @brief No-op. */
#define LOGAUTO(ACTION)

#endif /* !NODEBUG */


#endif /* !LOG_H */

