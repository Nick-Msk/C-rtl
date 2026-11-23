#ifndef _COMMAND_EXECUTOR_H
#define _COMMAND_EXECUTOR_H

// renamed from Context
typedef struct Runtimedata Runtimedata;     // just to use a pointer

typedef int         (*process_unit)(Runtimedata *tr);   // function, process context

typedef struct Command {
    const char *    name;
    int             shortlen;
    const char *    desc;
    process_unit    proc;
} Command;

#define             CommandInit(...) (Command) {.name = 0, .proc = 0, .desc = 0, .shortlen = 0, ##__VA_ARGS__}

extern bool
process_command(const char *restrict name,
                Command *restrict cmd,
                Runtimedata *restrict rt);

#endif /* !_COMMAND_EXECUTOR_H */
