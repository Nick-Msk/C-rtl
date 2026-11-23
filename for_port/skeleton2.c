#include <stdio.h>
#include <string.h>

#include "log.h"
#include "common.h"
#include "error.h"
#include "checker.h"
#include "parse_keys.h"

typedef struct Keys {
    bool              version;    // bool example
    bool              string;
    const char       *filename;
    int               maxline;
    // ...
} Keys;

#define                 Keysinit(...) (Keys){ .version = false, .string = 0, .maxline = 0, .filename __VA_ARGS__}
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
                parse_bool('s', string);
                parse_string('f', filename);
                default:    // probaly it's possible to ignore unknows parameters
                    return userraise(-1, ERR_WRONG_PARAMETER, "Illegal [%c], params [%d] argc %d", c, params, argc);
            }
    }
    return logret(argc, "params %d, argc %d, %s", params, argc, ptr = 0);
}

const char *usage_str = "Usage: %s <val1:TYPE1> <val2:TYPE2>\n";

int                     main(int argc, const char *argv[]){
    logsimpleinit("Start");

    Keys    ke = Keysinit();
    argc = parse_keys(argv, &ke);

    if (argc < 0) {
        printf(usage_str, *argv);
        return 1;
    }
    if (ke.version){
        printf("%s SKELETON EXAMPLE\n", __FILE__);
        printf(usage_str, *argv);
        return 0;
    }

    // TODO: code from here!

    if (!fs_alloc_check(false))
        logmsg("Warning: incorrect allocation of fs's");

    return logret(0, "end...");  // as replace of logclose()
}

