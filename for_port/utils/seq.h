#ifndef _SEQ_H
#define _SEQ_H

// ---------------------------------------------------------------------------------
// ------------------------------ Public sequance API -------------------------------
// ---------------------------------------------------------------------------------

#include <stdio.h>
#include <stdint.h>

#include "log.h"
#include "checker.h"

// --------------------------------- CONSTANTS AND GLOBALS --------------------------

// ---------------------------------- TYPES -----------------------------------------

typedef int64_t         seqv_t;
typedef int             seqnum_t;

// -------------------- ACCESS AND MODIFICATORS -------------------------------------

/**
 * @brief allocate a new seq
 */
extern seqnum_t         initseq(void);
/**
 * @brief drop sequence
 *
 * @param s sequence to drop
 */
extern void             dropseq(seqnum_t s);
/**
 * @brief get a current value
 *
 * @param s sequence
 */
extern seqv_t           currval(seqnum_t s);
/**
 * @brief get a next value
 *
 * @param s sequence
 */
extern seqv_t           nextval(seqnum_t s);
/**
 * @brief reset sequence engine
 */
extern void             resetseq(void);
/**
 * @brief technical printer
 *
 * @param out stream opened for write
 */
extern int              seq_techfprint(FILE *out);
static inline int       seq_techprint(void) {
    return seq_techfprint(stdout);
}

// ------------------------------------ ETC. ----------------------------------------

#endif /* _SEQ_H */
