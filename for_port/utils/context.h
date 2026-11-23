#ifndef _CONTEXT_H
#define _CONTEXT_H

// ---------------------------------------------------------------------------------
// --------------------------- Public Context API -------------------------------
// ---------------------------------------------------------------------------------

// --------------------- Includes (no FS here to avoid recursive deps)  -----------------------

#include <stdio.h>
#include <string.h>
//
#include "bool.h"
#include "log.h"
#include "common.h"
#include "error.h"
#include "checker.h"

// ----------- CONSTANTS AND GLOBALS ---------------

enum { ContextElemActive = 0x1 };

// ------------------- TYPES -----------------------

// very simple, no groups or smth like that
typedef struct ContextSortedElem {
    char                       *name;
    char                       *value; // LIST of value must be here!!! TODO:
    unsigned                    flags;
    struct ContextSortedElem   *next;
} ContextSortedElem;

typedef struct Context {
    int                   cnt;
    ContextSortedElem   **ctx;  // TODO: think abound  ctx[] // array w/o limit
} Context;

// ------------------ CONSTRUCTOTS/DESTRUCTORS -----------------------

extern Context                      ctxinit(int sz);
extern void                         ctxfreed(Context *c);

#define                     ctxfree(c) (ctxfreed( &(c) ))

// -------------------- ACCESS AND MODIFICATORS ------------------------

extern ContextSortedElem           *ctxget(const Context *restrict c, const char *restrict name);
static inline bool                  ctxexists(const Context *restrict c, const char *restrict name){
    return ctxget(c, name) != 0;
}

static inline const char           *ctxgetvalue(const Context *restrict c, const char *restrict name){
    ContextSortedElem *l = ctxget(c, name);
    return l != 0 ? l->value : 0;
}

extern bool                         ctxadd(Context *restrict c, const char *restrict name, const char *restrict value);
extern bool                         ctxdel(Context *restrict c, const char *restrict name);
extern bool                         ctxreset(Context *c);
extern int                          ctxcount(const Context *c);

// ------------------------ PRINTERS/CHECKERS --------------------------

// technical, not for use
extern int                          ctx_techfprint(FILE *restrict out, const Context *restrict c, const char *restrict name);
static inline int                   ctx_techprint(const Context *restrict c, const char *restrict name){
    return ctx_techfprint(stdout, c, name);
}

#define                             ctxtechfprint(out, ctx) ctx_techfprint( (out), &(ctx), #ctx )
#define                             ctxtechprint(ctx) ctx_techprint( &(ctx), #ctx)

extern int                          ctx_fprintelem(FILE *restrict out, const ContextSortedElem *restrict elem);
static inline int                   ctx_printelem(const ContextSortedElem *elem){
    return ctx_fprintelem(stdout, elem);
}

#define                             ctxfprintelem(out, ctx) ctx_fprintelem( &(out), (ctx) )
#define                             ctxprintelem(ctx) ctx_printelem(ctx)


#endif /* !_CONTEXT_H */

