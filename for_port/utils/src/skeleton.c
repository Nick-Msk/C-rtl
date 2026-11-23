

#include "skeleton.h"

/********************************************************************
                 <SKELETON> MODULE IMPLEMENTATION
********************************************************************/

// static globals

// internal type

// ---------- pseudo-header for utility procedures -----------------

// ------------------------------ Utilities ------------------------

// -------------------------- (Utility) printers -------------------

// --------------------------- API ---------------------------------

// -------------------------- (API) printers -----------------------

// -------------------------------Testing --------------------------

#ifdef METRICTESTING

#include "testing.h"
#include <signal.h>

// ------------------------- TEST 1 ---------------------------------

// ------------------------- TEST 2 ---------------------------------

// -------------------------------------------------------------------
int
main(int argc, char *argv[])
{
    const char *logfilename = "checker.log";

    if (argc > 1)
        logfilename = argv[1];

    loginit(logfilename, false, 0, "Starting");

    testenginestd(
        testnew(.f2 = tf1, .num = 1, .name = "Simple invariant text"       , .desc = "", .mandatory=true)
      , testnew(.f2 = tf2, .num = 2, .name = "Complex invariant test"      , .desc = "", .mandatory=true)
      //, testnew(.f2 = f3, .num = 3, .name = "Interrupt raising test"        , .desc = "Exception test."                                                             , .mandatory=true)
      //, testnew(.f2 = f4, .num = 4, .name = "System error test."            , .desc = "System error raising test (w/o exception)."  , .mandatory=true)
    );

        logclose("end...");
    return 0;
}


#endif /* SKELETONTESTING */

