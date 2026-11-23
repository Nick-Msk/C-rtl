#include <stdio.h>
#include <string.h>

#include "log.h"
#include "command_executor.h"

// go through cmd until cmd->proc is null
bool process_command(
    const char *restrict name,
    Command *restrict cmd,
    Runtimedata *restrict ctx)
{
    logenter("Cmd [%s]", name);
    // 1-st version, just a fullscan
    while (cmd->proc){
        if ( (cmd->shortlen == 0 && strcmp (name, cmd->name) == 0 ) ||
             (cmd->shortlen > 0 && strncmp(name, cmd->name, cmd->shortlen) == 0 )
        ){
            cmd->proc(ctx);
            return logret(true, "Command %s was processed", name);
        }
        cmd++;      // next
    }
    return logerr(false, "Command %s not found", name);
}

