#include <stdio.h>

#include "error.h"
#include "checker.h"
#include "fs.h"
#include "fs_array.h"
#include "core_impl.h"
// actually only for proc_par lexem is need
#include "getword.h"

static const char           BUILD[] = "make -s -C res/ _run1";
static const char           RUN[]   = "make -C res/ run";

// ------------------------------------------ Utilities -------------------------------------------

static int                      Runtimedata_techfprint(FILE *restrict out, const Runtimedata *restrict rt){
    invraise(rt != 0, "Null pointer");

    int     cnt = 0;
    if (out){
        cnt += fprintf(out, "Run time data: flname %p [%s], runflname %p [%s], bodyptr %d, prevrunptr %d\n", 
                rt->fl, rt->flname, rt->runfl, rt->runflname, rt->bodyptr, rt->prevrunptr);
        cnt += lexem_techfprint(out, &rt->lex);
        cnt += fsarr_techfprint(out, &rt->body);
        cnt += fprintf(out, "END of Run time data");
    }
    return logsimpleret(cnt, "Printed %d", cnt);
}

static inline int               Runtimedata_techprint(const Runtimedata *rt){
    return Runtimedata_techfprint(stdout, rt);
}

static int                      util_save(FILE *restrict out, const fsarray *restrict arr, int cnt, int prevrunptr){
    int     i;
    cnt = cnt <= 0 ? fsarr_cnt(arr) : cnt;
    if (prevrunptr > 0)
        fprintf(out, "%s\n", "su_stddisable();");   // disable std output
    for (i = 0; i < cnt; i++){
        if (prevrunptr > -1 && i == prevrunptr)
            fprintf(out, "%s\n", "su_stdenable();");    // enable std output
        fs *s = arr->ar + i;
        fprintf(out, "%s\n", fs_str(s) );
    }
    return i;
}

static bool                     util_run(const char *fname){
    if (system(RUN) != 0)
        return userraise(false, ERR_UNABLE_TO_RUN_MAKE, "Unable to run %s", RUN);
    return logsimpleret(true, "run");
}

static bool                     util_build(const char *fname){
    if (system(BUILD) != 0)
        return userraise(false, ERR_UNABLE_TO_RUN_MAKE, "Unable to %s", BUILD);
    return logsimpleret(true, "builded");
}

// check + build + run
static bool                     util_sysexec(const char *fname){
    // run - now directly
    if (!util_build(fname))
        return userraise(false, ERR_UNABLE_TO_RUN_MAKE, "Unble to build, check errors");
    util_run(fname);
    return logsimpleret(true, "Executed %s", fname);
}

// --------------------------------------- AUXILLARY PROCS -------------------------------------------

// example of working procedures
// local
int                             proc_quit(Runtimedata *rt){
    rt->quit = true;
    return 1;
}

int                             proc_help(Runtimedata *rt){
    invraise(rt != 0, "Null pointer");

    const Command   *cm = rt->cmds;
    logsimple("%p", cm);
    while (cm->proc){
        printf("%s(%d)\t\t: %s\n", cm->name, cm->shortlen, cm->desc);
        cm++;
    }
    return 1;
}

int                             proc_techdump(Runtimedata *rt){
    invraise(rt != 0, "Null pointer");

    // just technical print
    Runtimedata_techprint(rt);
    return 1;
}

// ------------------------------------------- PROCS -------------------------------------------------

int                             proc_load(Runtimedata *rt){
    invraise(rt != 0, "Null pointer");

    int     cnt = 0;
    if (rt->fl){
        int    lines = 0;
        if ( (lines = fsarr_floadlines(rt->fl, &rt->body, 0) ) < 0)
            fprintf(stderr, "Unable to load %s", rt->flname);
        else {
            rt->bodyptr = lines;
            printf("%d line(s) were loaded from %s\n", lines, rt->flname);
        }
    }
    return logsimpleret(cnt, "%d Lines were load", cnt);
}

int                             proc_print(Runtimedata *rt){
    invraise(rt != 0, "Null pointer");

    int     cnt = 0;
    cnt += fsarr_print(&rt->body);
    return logsimpleret(cnt, "%d Lines were print", cnt);
}

// main!
int                             proc_run(Runtimedata *rt){
    invraise(rt != 0 && rt->runfl != 0, "Null pointer");

    fprintf(rt->runfl, "#include <all.h>\nint main(int argc, const char *argv[]){\n");
    util_save(rt->runfl, &rt->body, rt->bodyptr, rt->prevrunptr);  // apprend
    fprintf(rt->runfl, "\n}\n");
    fflush(rt->runfl);
    // this version 0.1 will NOT switch stdout/err
    if (!util_sysexec(rt->runflname) )
        userraise(-1, ERR_UNABLE_TO_EXEC_FILE, "Unable to exec %s", rt->runflname);
    //
    rt->prevrunptr = rt->bodyptr;   // add pointer
    return 1;   // as OK
}

int                             proc_save(Runtimedata *rt){
    invraise(rt != 0, "Null pointer");

    int     cnt = 0;
    if (rt->fl){
        rt->fl = freopen(NULL, "w+", rt->fl);
        if (!rt->fl)
            sysraiseint("Unable to reopen %s", rt->flname);
        cnt = util_save(rt->fl, &rt->body, rt->bodyptr, -1);
    }
    printf("%d line(s) were saved into %s\n", cnt, rt->flname);
    return logsimpleret(cnt, "%d Lines were saved", cnt);
}

int                             proc_clear(Runtimedata *rt){
    invraise(rt != 0, "Null pointer");

    printf("Cleared %d lines\n", fsarr_clean(&rt->body, false) );
    rt->prevrunptr = rt->bodyptr = 0;
    return logsimpleret(1, "Cleared!");
}

int                             proc_par(Runtimedata *rt){
    invraise(rt != 0, "Null pointer");

    // TODO!!!
    return logsimpleret(1, "Pars...");
}

Runtimedata                     initRuntimedata(const char *restrict flname, const char *restrict runflname, const Command *cmds){
    invraise(flname != 0 && runflname != 0, "Null input files names");

    // chech up shell
    if (!system(0) )
        sysraiseint("Unable to find command shell");

    Runtimedata rt = RuntimedataInit(.cmds = cmds);
    rt.fl = fopen(flname, "r+");
    if (!rt.fl)
        sysraiseint("Unable to open %s for r+", flname);
    rt.flname = flname;
    rt.runfl = fopen(runflname, "w");
    if (!rt.runfl)
        sysraiseint("Unable to open %s for w", runflname);
    rt.runflname = runflname;
    rt.body = fsarr_init(100);
    return logsimpleret(rt, "Created with %s, %s", flname, runflname);
}

void                            freeRuntimedata(Runtimedata *rt){
    invraise(rt != 0, "Null pointer");

    logsimple("Cleanup rt");
    fsarr_free(&rt->body);
    if (rt->fl)
        fclose(rt->fl), rt->fl = 0;
    if (rt->runfl)
        fclose(rt->runfl), rt->runfl = 0;
    rt->flname = rt->runflname = 0; // must be static or full program live! No free here
    lexem_free(&rt->lex);
    logsimple("Clean rt done");
}

bool                            addline(Runtimedata *restrict rt, fs str){
    invraise(rt != 0, "Null pointer");

    // MOVE fs into fsarray
    fsarray     *b = &rt->body;
    if (rt->bodyptr >= fsarr_cnt(b) )
        if (fsarr_increase(b, rt->bodyptr + 100) < 0)
            return userraise(false, ERR_UNABLE_ALLOCATE, "Unable to add lines to body");
    fs_cpy(b->ar + rt->bodyptr++, str);
    return logsimpleret(true, "Added");
}

