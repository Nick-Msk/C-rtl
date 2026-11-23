#ifndef METRIC_H
#define METRIC_H

#include <stdio.h>

#include "bool.h"

// ---------------------------------------------------------------------------------
// --------------------------- Public Metric API ----------------------------------
// ---------------------------------------------------------------------------------

// ----------- CONSTANTS AND GLOBALS ---------------

static const int            MAX_METRIC_NAME = 64;
static const int            MAX_METRIC      = 100;

// ------------------- TYPES -----------------------

typedef struct {
    char    name[MAX_METRIC_NAME];
    union {
        int     value;
        double  dvalue;
    };
} Metric;

// ---------------- EXTERNAL ----------------------

extern int                  g_metricfreepos;
extern Metric               g_metric_array[MAX_METRIC];

// ------------- CONSTRUCTOTS/DESTRUCTORS ----------

// get or create
extern Metric            *metric_get(const char *name, bool create);

static inline Metric     *metric_create(const char *name){
    return metric_get(name, true);
}

static inline Metric     *metric_acq(const char *name){
    return metric_get(name, false);
}

static inline void       metric_free(void){
    g_metricfreepos = 0;        // that's all what we need for now, but in future cycle with free() will be required
}

// -------------- ACCESS AND MODIFICATION ----------

static inline Metric    *metric_inc(Metric *m){
    if (m)
        m->value++;
    return m;
}

static inline Metric    *metric_add(Metric *m, int val){
    if (m)
        m->value += val;
    return m;
}

static inline Metric    *metric_reset(Metric *m){
    if (m)
        m->value = 0;
    return m;
}

static inline bool       metric_setval(Metric *m, int val){
    if (m){
        m->value = val;
        return true;
    } else
        return false;
}

static inline int       metric_getval(Metric *m){
    if (m)
        return m->value;
    else
        return 0;
}

// ----------------- PRINTERS ----------------------

int                       metric_fprint(FILE *f, Metric *m);

static inline int         metric_print(Metric *m){
    return metric_fprint(stdout, m);
}

// ------------------ ETC. -------------------------

#endif /* !METRIC_H */

