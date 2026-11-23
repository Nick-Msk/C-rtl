#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>
#include "bool.h"

static const int		MAX_MODULE = 20;

typedef enum {
	LOGOFF					= 0,
	LOGERR					= 1,		// not using for now
	LOGWARN					= 2,		// not using for now
	LOGALL					= 3,
	_LOGSTOP				= -1		// technical, shouldn't use by application
} Loglevel;

typedef enum {
	LOG_ENTER				= 0
  , LOG_LEAVE
  , LOG_ERR
  , LOG_MSG
  , LOG_SIMPLE				= 0x10 	// simple logging fla
	// new line policy
  , LOG_NOPREAMBULE			= 0x20		// don't print preambule
  , LOG_NONEWLINE			= 0x40		// don't print new line
} LogAction;

typedef enum {
	LOG_FORMAT_EMPTY		= 0
  , LOG_FORMAT_ALL
  , LOG_FORMAT_ONLY_FILE
  , LOG_FORMAT_ONLY_FUNC
  , LOG_FORMAT_SIMPLE					// minimalized
  , LOG_FORMAT_ONLY_TIME				// minimalized + time
} LogFormat;

typedef struct {
	char 		module[MAX_MODULE];
	Loglevel	level;
} LogModlevel;

// simple loglevel printer
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

// constructor
extern bool
log_init(const char *		  logname
	   , bool                 append
	   , LogFormat			  lformat
       );


// init module list
extern bool
log_modinit(LogModlevel *modlist);

// save module list to file
extern bool
log_modsave(const char *name);

// load module list from file
extern bool
log_modload(const char *name);

// checker
extern bool
log_isinit(void);

// return stream
extern FILE *
log_file(void);

// cleaner
extern void
log_close(void);

// return current offset in symbols
extern int
log_offset(void);

extern int
log_prog_switch(bool logon_mode);

// change format (TODO: should be for module, but not global
extern bool
log_format(LogFormat);

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

// return current offset for this function
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

// print particular bytes
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

#ifndef NODEBUG

/* Interface macros:
	1. logenter("text", par);				// initialor _LG_LV
	2. logret(retcode, "text", par) or return logret(retcode, "text", par);
	3. logerr(retcode, "text", par) or return logerr(retcode, "text", par);
	4. logmsg("text", par); // return value is 0
	5. logtype(ERR or LEAVE, retcode, "text", par)
	6. logsimple("text", par)							// all simple are not using _LG_LV
	7. logsimpleerr(retcode, "text", par)
	8. logsimpleleave(retcode, "text", par)
	9. logsympletype(ERR or LEAVE, retcode, "text", par)
*/

#define _LG_LV _LG_LV

// core macro
#define _LOG_MSG(type, lglv, fmt, ...) log_msg((type), (lglv), LOGALL, MODNAME, __FILE__ , __func__, __LINE__, ( fmt ), ##__VA_ARGS__ )
//with ap
#define _LOG_MSG_AP(type, lglv, fmt, ap) log_msg_ap((type), (lglv), LOGALL, MODNAME, __FILE__ , __func__, __LINE__, ( fmt ), (ap))
// core return macro (as block inside operator)
#define _LOG_MSG_RET(ACTION, type, retcode, lglv, fmt, ...) \
		({\
		   _LOG_MSG(type, lglv, fmt, ##__VA_ARGS__);\
		   ACTION;\
		  (retcode);\
		})

// with ap TODO: think, how to avoid duplicate
#define _LOG_MSG_RET_AP(ACTION, type, retcode, lglv, fmt, ap) \
		({\
		     _LOG_MSG_AP(type, lglv, fmt, ap);\
		     ACTION;\
		     (retcode);\
		})

// direct for now, TODO: think, if possible to integrate into macro-engine
#define lognumbers(bytes, sz)				log_numbers(LOG_MSG | LOG_SIMPLE, 0, LOGALL, MODNAME, __FILE__ , __func__, __LINE__, bytes, sz)

// interface
// initialize
#define logenter(fmt, ...) 		int _LG_LV = 					_LOG_MSG(LOG_ENTER, 0	 							, fmt, ##__VA_ARGS__)
// return common
#define logtype(type, retcode, fmt, ...) 	 					_LOG_MSG_RET(, type, retcode, _LG_LV 				, fmt, ##__VA_ARGS__)		// TODO: rework implementaion!!!

#define logtype_ap(type, retcode, fmt, ap)						_LOG_MSG_RET_AP(, type, retcode, _LG_LV             , fmt, ap)

// return details
#define logret(retcode, fmt, ...)            					logtype(LOG_LEAVE, retcode         					, fmt, ##__VA_ARGS__)
#define logerr(retcode, fmt, ...)            					logtype(LOG_ERR, retcode 							, fmt, ##__VA_ARGS__)
#define logmsg(fmt, ...)                	 					logtype(LOG_MSG, 0 									, fmt, ##__VA_ARGS__)
// with ap
//#define logmsg_ap(fmt, ap)										logtype_ap(LOG_MSG, 0,             					, fmt, ap)
#define	logmsg_ap(fmt, ap)										logtype_ap(LOG_MSG, 0								, fmt, ap)
// return with destructor/action
//
#define logacterr(ACTION, retcode, fmt, ...)					_LOG_MSG_RET(ACTION, LOG_ERR, retcode, _LG_LV		, fmt, ##__VA_ARGS__)
// LEGACY!!! COMMENTED FOR NOW
//#define logderr(ACTION, retcode, fmt, ...)	 					logacterr(ACTION, retcode							, fmt, ##__VA_ARGS__)
// return with action
#define logactret(ACTION, retcode, fmt, ...)   					_LOG_MSG_RET(ACTION, LOG_LEAVE, retcode, _LG_LV		, fmt, ##__VA_ARGS__)
// just log action
#define	logact(ACTION, fmt, ...)								_LOG_MSG_RET(ACTION, LOG_MSG, 0, _LG_LV				, fmt, ##__VA_ARGS__)	// TODO: need to be refactored to avoid using log_msg_ret
// with ap
#define logact_ap(ACTION, fmt, ap)								_LOG_MSG_RET_AP(ACTION, LOG_MSG, 0, _LG_LV          , fmt, ap)

//#define logmsg(fmt, ...) _LOG_MSG_RET(LOG_MSG, 0, _LG_LV, fmt, ...)

// simple log with action
#define logsimpletype_act(ACTION, type, retcode, fmt, ...)		_LOG_MSG_RET(ACTION, type | LOG_SIMPLE, retcode, 0	, fmt, ##__VA_ARGS__)

// with ap
#define logsimpletype_act_ap(ACTION, type, retcode, fmt, ap)	_LOG_MSG_RET_AP(ACTION, type | LOG_SIMPLE, retcode, 0  , fmt, ap)

// common simple log
#define logsimpletype(type, retcode, fmt, ...)                  logsimpletype_act(, type, retcode					, fmt, ##__VA_ARGS__)

// simple log interface
#define logsimple(fmt, ...)                  					logsimpletype(LOG_MSG, 0							, fmt, ##__VA_ARGS__)		// TODO: подумать об объединении с logmsg, можно ли обойти проблему  _LG_LV??

// with ap
#define logsimple_ap(fmt, ap)									logsimpletype_act_ap(, LOG_MSG, 0                   , fmt, ap)

#define logsimpleerr(retcode, fmt, ...)		 					logsimpletype(LOG_ERR  , retcode					, fmt, ##__VA_ARGS__)
#define logsimpleret(retcode, fmt, ...)	     					logsimpletype(LOG_LEAVE, retcode					, fmt, ##__VA_ARGS__)

// simple log imterface with action
#define logsimpleacterr(ACTION, retcode, fmt, ...)				logsimpletype_act(ACTION, LOG_ERR, retcode			, fmt, ##__VA_ARGS__)
#define logsimpleactret(ACTION, retcode, fmt, ...)              logsimpletype_act(ACTION, LOG_LEAVE, retcode		, fmt, ##__VA_ARGS__)

// analog of logact for simple class
#define	logsimpleact(ACTION, fmt, ...)							logsimpletype_act(ACTION, LOG_MSG, 0				, fmt, ##__VA_ARGS__)

#define logsimpleact_ap(ACTION, fmt, ap)						logsimpletype_act_ap(ACTION, LOG_MSG, 0             , fmt, ap)

// auto formatted API

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

// automatic log group
#define logautotype(type, val)							{ logsimpletype(type | LOG_NONEWLINE, 0, "%s = ", #val); \
                                                          logsimpletype(type | LOG_NOPREAMBULE, 0, LOGTYPEFORMAT(val), val); }

// tech, internal
#define logautoret_err(type, val)						({ logautotype(type, val); (val); })

// simple auto message
#define logauto(val) 									logautotype(LOG_MSG, val)
// ret or err auto message,     usage: return logautoret(val);
#define logautoret(val) 								logautoret_err(LOG_LEAVE, val)
#define logautoerr(val)									logautoret_err(LOG_ERR  , val)


// modules interface
#define MOD(name, lv)	{ .module = (#name), .level = (lv) }

#define MODULES(...) \
 	(LogModlevel []) { \
	__VA_ARGS__ \
	, MOD (, _LOGSTOP ) \
	}

// constructor/destructor macro   TODO: решить проблему с  возможностью if (loginit(... ))
#define loginits(logname, append, format, modules, fmt, ...) \
	log_init( (logname), (append), (format));\
	if (!log_modinit(modules))\
		fprintf(stderr, "Unable to init module list");\
 	logenter(fmt, ##__VA_ARGS__);

#define loginit(logname, append, modules, fmt, ...) \
	loginits((logname), (append), LOG_FORMAT_ONLY_FUNC, (modules), (fmt), ##__VA_ARGS__)

// simplified init
#define logsimpleinitdir(dirname, fmt, ...)\
	loginit(#dirname"/"__FILE__".log", false, 0, (fmt), ##__VA_ARGS__)
// simplified init
#define logsimpleinit(fmt, ...)\
    logsimpleinitdir(log, (fmt), ##__VA_ARGS__)

#define LOG(dirname)\
    loginit(dirname"/"__FILE__".log", false, 0, "Start logging (cut)")

// not working preporly for now
#define LOGAPPEND(dirname)\
    loginit(dirname"/"__FILE__".log", true, 0, "Start logging (append)")


#define logcloseret(retcode, fmt, ...) \
	({logret( (retcode), (fmt), ##__VA_ARGS__); \
      log_close(); \
	  (retcode); })

#define logclose(fmt, ...) \
    logcloseret(0, (fmt), ##__VA_ARGS__);

#define	logfile								(log_file())			// wrapper for log_file()
#define logoffset							(log_offset())		// wrapper for log_offset()

/// @brief Prints spaces corresponding to the current log offset to the log file
static inline int
logprintoffset(void) {
	int offset = log_offset();
	if (offset > 0)
		return fprintf(logfile, "%*s", offset, "");
	return 0;
}

// TODO: перавая версия простая, но подумать, можно ли сделать автовключение лога при возврате на предыдущий/текущей уровень _LG_LV
#define logoff()                            log_prog_switch(false)
#define logon()								log_prog_switch(true)

// can be variable definition and so on
//#define LOG(...)							__VA_ARGS__;
// ACTION must be expresstion of simple type (for example: i++ )
#define LOGAUTO(ACTION)						{ logauto(ACTION); }

#define MODNAME 							"DEFAULT_MOD"		// for using as 'default mode logging'
#define DEFAULT_MOD							"DEFAULT_MOD"

#else /* NODEBUG */

#define DEFAULT_MOD							""
#define MOD(name, lv)						{ .module = (#name), .level = (lv) }

#define loginit(...)						(false)				// in NOBEBUG mode result is false
#define logclose(...)						(0)
#define logcloseret(ret, ...)               (ret)

#define logsimpleinitdir(...)
#define logsimpleinit(...)
//
#define logmsg(...)	 						(0)
#define logmsg_ap(...)						({ va_end(ap); 0; })
#define logenter(...)
#define logtype(type, retcode, ...) 		(retcode)
#define logret(retcode, ...) 				(retcode)
#define logerr(retcode, ...) 				(retcode)
// with action
	// legacy
#define logderr(ACTION, retcode, ...)		({ ACTION; (retcode); })
#define logact(ACTION, fmt, ...)			({ ACTION; })
#define logact_ap(ACTION, fmt, ap)          ({ va_end(ap); ACTION; })
#define logactret(ACTION, retcode, ...)   	({ ACTION; (retcode); })
#define logacterr(ACTION, retcode, ...)		({ ACTION; (retcode); })
// simple group
#define logsimple(...) 						(0)
#define logsimple_ap(...)					({ va_end(ap); 0; })
#define logsimpleerr(retcode, ...) 			(retcode)
#define logsimpleret(retcode, ...) 			(retcode)
#define logsimpletype(type, retcode, ...) 	(retcode)

#define logsimpleacterr(ACTION, retcode, fmt, ...) ({ ACTION; (retcode); })
#define logsimpleactret(ACTION, retcode, fmt, ...) ({ ACTION; (retcode); })

#define logsimpleact(ACTION, fmt, ...)		({ ACTION; })
#define logsimpleact_ap(ACTION, fmt, ap)	({ va_end(ap); ACTION; })

//auto group
#define logautotype(type, val)
#define logautoret_err(type, val)           (val)
#define logautoret(ret)						(ret)
#define logautoerr(ret)						(ret)


#define logfile								(stdout)		// in case of no logging engine
#define logoffset							(0)				//
#define logprintoffset						(void) (0)

#define logoff()							(void) (0)
#define	logon()								(void) (0)

#define LOG(...)
#define LOGAPPENT(...)
#define LOGAUTO(ACTION)

#endif /* !NODEBUG */


#endif /* !_LOG_H */

