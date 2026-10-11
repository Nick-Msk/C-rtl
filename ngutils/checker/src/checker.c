
#include <stdio.h>
#include <stdarg.h>

#include "log.h"
#include "checker.h"



/********************************************************************
                 CHECK MODULE IMPLEMENTATION
********************************************************************/

// static globals

// internal type

// ---------- pseudo-header for utility procedures -----------------

// ------------------------------ Utilities ------------------------

// -------------------------- (Utility) printers -------------------

// --------------------------- API ---------------------------------

// -------------------------- (API) printers -----------------------

int                     inv_fprintf_int(FILE *restrict out, const char *restrict expr, int res, int val, const char *restrict msg, ...){
	int 	cnt = 0;

    if (out) {
        cnt = fprintf(out, "Invariant for [%s] is violated\n", expr);
            logsimple("Invariant for [%s] is violated\n", expr);		// TODO: think about how to imtegrate log and fprintf in one

        if (val && val != res){	// check if complex invariant
            cnt += fprintf(out,  "Result must be [%d] but equal to [%d]\n", val, res);
                logsimple("Result must be [%d] but equal to [%d]\n", val, res);
        }

        // then print user message, TODO: think if logger can accept va_list
        va_list  ap;
        va_start(ap, msg);
        cnt += vfprintf(out, msg, ap);
        cnt += fprintf(out, "\n");
        va_end(ap);
    }

	return cnt;
}
