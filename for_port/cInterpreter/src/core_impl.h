#ifndef _CORE_IMPL_H
#define _CORE_IMPL_H

#include <stdio.h>
#include "bool.h"
#include "command_executor.h"
#include "fs.h"
#include "fs_array.h"
#include "getword.h"

// renamed from Runtimedata
typedef struct Runtimedata {
    Lexem            lex;
    bool             quit;   // flag for quit
    const Command   *cmds;   // link to all commands
    // ----- TODO: use a separate structure for real data

    // real data from here
    fsarray         include;        // not user for now
    fsarray         functions;      // not user for now
    fsarray         body;           // code
    int             includeptr, functionsptr, bodyptr;
    int             prevrunptr;    // pointer to body to point of prev run
    FILE           *fl, *runfl;
    const char     *flname, *runflname;
} Runtimedata;

/*
typedef struct RTFILE {
    FILE            *f;
    const char      *name;  // static?
} RTFILE;

typedef struct Runtimecoredata {
    Lexem            lex;
    bool             quit;   // flag for quit
    Command         *cmds;   // link to all commands
} Runtimecoredata;

// it should look like
typedef struct Runtimedata {
    Runtimecoredata     core;
    fsarrayiter         includes, functions, body;
    RTFILE              fl, funfl;
} Runtimedata;

*/

/*
fsarrayiter - TODO!!! in fs_array.h, not here
*/

// TODO: refactor need to be here
#define             RuntimedataInit(...) (Runtimedata)\
{.include = fsarr_empty(), .includeptr = 0,\
 .functions = fsarr_empty(), .functionsptr = 0,\
 .body = fsarr_empty(), .bodyptr = 0, \
 .quit = false, .fl = 0, .runfl = 0, .flname = 0, .runflname = 0, .lex = lexeminit(),\
 .cmds = 0, ##__VA_ARGS__}

// ------------------------------------------ Utilities -------------------------------------------

// --------------------------------------- AUXILLARY PROCS -------------------------------------------

extern int                 proc_quit(Runtimedata *rt);
extern int                 proc_help(Runtimedata *rt);
extern int                 proc_techdump(Runtimedata *rt);

// ------------------------------------------- PROCS -------------------------------------------------

extern int                 proc_load(Runtimedata *rt);
extern int                 proc_print(Runtimedata *rt);
extern int                 proc_run(Runtimedata *rt);
extern int                 proc_save(Runtimedata *rt);
extern int                 proc_clear(Runtimedata *rt);
extern int                 proc_par(Runtimedata *rt);

// ------------------------------- Contructor/destructor -----------------------------------------------

extern void                freeRuntimedata(Runtimedata *rt);
extern Runtimedata         initRuntimedata(const char *restrict flname, const char *restrict runflname, const Command *cmds);

// ------------------------------------------- Methods -------------------------------------------------
extern bool                addline(Runtimedata *rt, fs str);

#endif /* !_CORE_IMPL_H */
