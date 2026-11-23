#include "fs.h"
#include "log.h"
#include "checker.h"
#include "guard.h"
#include "error.h"
#include "command_executor.h"
#include "parse_keys.h"
#include "core_impl.h"
#include "getword.h"

typedef struct Keys {
    bool              version;    // bool example
    //bool              unbuf;
    const char       *bufname;
    const char       *runname;
    // ...
} Keys;

#define                 Keysinit(...) (Keys){ .version = false, .bufname = 0, .runname = 0, __VA_ARGS__}

static int              parse_keys(const char *argv[], Keys *ke){

    invraise(ke, "Zero ke!!! Error!"); // raise here
    logenter("...");

    int     argc = 1, params = 0;
    char    c;
    const char *ptr;

    while (*++argv != 0 && **argv == '-'){
        argc++;
        while ( (c = *++argv[0]) )
            switch (tolower(c)){
                parse_bool('v', version);
                //parse_bool('u', unbuf);
                parse_string('b', bufname);
                parse_string('r', runname);
                default:    // probaly it's possible to ignore unknows parameters
                    return userraise(-1, ERR_WRONG_PARAMETER, "Illegal [%c], params [%d] argc %d", c, params, argc);
            }
    }
    return logret(argc, "params %d, argc %d, %s", params, argc, ptr = 0);
}

const char *usage_str = "Usage: %s ... TODO\n";
const char *version = "0.1 (init)";

static bool             launch(const char *restrict buffilename, const char *runfilename);

int                     main(int argc, const char *argv[]){
    logsimpleinit("Start");

    Keys    ke = Keysinit();
    argc = parse_keys(argv, &ke);


    if (argc < 0) {
        printf(usage_str, *argv);
        return 1;
    }
    if (ke.version){
        printf("%s, cInterprepet ver %s\n", __FILE__, version);
        printf(usage_str, *argv);
        return 0;
    }
    const char *bufname = "res/file.buf";
    const char *runname = "res/_run1.c";
    if (ke.bufname)
        bufname = ke.bufname;
    if (ke.runname)
        runname = ke.runname;

    launch(bufname, runname);

    return logret(0, "end...");  // as replace of logclose()
}

// filled locally!
static Command cmds[] = {
    CommandInit(.name = "quit"          , .shortlen = 1, .proc = proc_quit         , .desc = "Just quit the runner"                                            )
  , CommandInit(.name = "help"          , .shortlen = 1, .proc = proc_help         , .desc = "Type all command and descriptions"                               )
  , CommandInit(.name = "techdump"      , .shortlen = 4, .proc = proc_techdump     , .desc = "Technical dump of run-time data"                                 )
    // -------------------------------------------
  , CommandInit(.name = "run"           , .shortlen = 1, .proc = proc_run          , .desc = "Save and run   m _cInt_run.c file"                               )
  , CommandInit(.name = "par"           , .shortlen = 2, .proc = proc_par          , .desc = "add or reset parameter (into Makefile)"                          )
  , CommandInit(.name = "print"         , .shortlen = 1, .proc = proc_print        , .desc = "Print of main wrap + body data"                                  )
  , CommandInit(.name = "save"          , .shortlen = 2, .proc = proc_save         , .desc = "Save run-time data into _cInt_buf.c file"                        )
  , CommandInit(.name = "clear"         , .shortlen = 2, .proc = proc_clear        , .desc = "Clear run-time data"                                             )
  , CommandInit(.name = "load"          , .shortlen = 2, .proc = proc_load         , .desc = "Load run-time data from _cInt_buf.c file"                        )
  , CommandInit()
};

static bool             launch(const char *restrict bufname, const char *runname){
    logenter("%s:%s", bufname, runname);

    bool        ret = true;

    Runtimedata rt = initRuntimedata(bufname, runname, cmds);
    printf(" >");
    while (!rt.quit && getstring(&rt.lex) ){
        if (rt.lex.typ == LEXEM_CMD){
            // find + exec
            if (!process_command( lexemstr(rt.lex), cmds, &rt) )
                fprintf(stderr, "Command %s not found\n", lexemstr(rt.lex) );
        } else if (rt.lex.typ == LEXEM_STR){
            logauto(fslen(rt.lex.str) );
            if (fslen(rt.lex.str) > 1)
                addline(&rt, rt.lex.str);
        } else
            fprintf(stderr, "Incorrent lexem type %d:%s\n", rt.lex.typ, Lexemtype_str(rt.lex.typ) );
        if (!rt.quit)
            printf(" >");
    }
    //lexemfree(lex); // no need
    freeRuntimedata(&rt);
    return logret(ret, "%s", bool_str(ret) );
}

