#ifndef _BUFFER_H
#define _BUFFER_H

#include <stdio.h>

// ****************** TO BE REPLACED BY DS ***** DON'T USE ANYMORE *****************

// ---------------------------------------------------------------------------------
// --------------------------- Public BUFFER API -----------------------------------
// ---------------------------------------------------------------------------------

// NOTE: THIS IS DEPRECATED, USE DS.H INSTEAD

// ----------- CONSTANTS AND GLOBALS ---------------

// ------------------- TYPES -----------------------

// ------------- CONSTRUCTOTS/DESTRUCTORS ----------

extern void                     buffer_clear(void);

// -------------- ACCESS AND MODIFICATION ----------

extern bool                     buffer_set(FILE *in);

extern int                      getch(void);

extern void                     ungetch(int c);

extern int                      ungets(const char *s);

extern int                      ungetrevs(const char *s, int len);

// ----------------- PRINTERS ----------------------

extern int                      buffer_fprint(FILE *f);

static inline int               buffer_print(void){
    return buffer_fprint(stdout);
}

// ------------------ ETC. -------------------------

#endif /* !_BUFFER_H */

