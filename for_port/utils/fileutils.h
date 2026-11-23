#ifndef _FILEUTILS_H
#define _FILEUTILS_H

#include <stdlib.h>
#include <string.h>

#include "fs.h"

/***************************************************************
                USEFUL MACRO AND FUNCTIONS
***************************************************************/

// ---------------------- API ----------------------------------------

// --------------- fs based API ---------------------------
extern int                     fgetline_cmn_fs(FILE *restrict in, fs *restrict s, bool newline);

// line without last '\n'
static inline int              fgetslim_fs(FILE *restrict in, fs *restrict s){
    return fgetline_cmn_fs(in, s, false);
}

// with filal '\n'
static inline int               getslim_fs(fs *str){
    return fgetslim_fs(stdin, str);
}

// with final '\n'
static inline int              fgetline_fs(FILE *restrict in, fs *restrict s){
    return fgetline_cmn_fs(in, s, true);
}

// with filal '\n'
static inline int               getline_fs(fs *str){
    return fgetline_fs(stdin, str);
}



// ALL file as one fs!
extern fs                       readfs_file(FILE *f);

// read all lines separately into fs[]
extern int                      freadlines(FILE *restrict f, fs **restrict lines);

static inline int               readlines(fs **lines){
    return freadlines(stdin, lines);
}

// write cnt fs into stream
extern int                      fwritelines(FILE *restrict f, const fs *restrict ptr, int cnt);

static inline int               writelines(const fs *lines, int cnt){
    return fwritelines(stdout, lines, cnt);
}
// TODO: remove that after migration to fsarray
extern void                     freelines(fs *lineptr, int nlines);

extern int                      fprint_file(FILE *restrict out, FILE *restrict f);

static inline int               print_file(FILE *restrict f){
    return fprint_file(stdout, f);
}

// OLD char * API
extern int                      get_line(char *line, int lim);
// simpe file reader, must call free(s); after usage!!
extern char                     *read_from_file(FILE *f, int *p_cnt);

extern bool                     fread_pattern(FILE *restrict f, const char *restrict, int sz);

extern bool                     fread_pattern_printf(FILE *restrict f, const char *restrict fmt, ...) __attribute__ ( (format (printf, 2, 3) ) );;

#define                         freadpattern(in, p) fread_pattern(in, (p), strlen(p) )
/*
#define                         FUSKIPFORMAT(in, pt)\
                                if (!freadpattern( (in), (pt)) )\
                                    userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read pattern '%s'", (pt) );

#define                         FUSKIPFORMATPRINTF(in, fmt, ...)\
                                if (!fread_pattern_printf( (in), (fmt), ##__VA_ARGS__) )\
                                    userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read dynamic pattern");
*/

// particular for unsigned
#define                         FUGETUNSIGNED_RAISE(in) ({unsigned _tmp; if (fscanf(in, "%u", &_tmp) < 1)\
                 userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read unsigned"); _tmp;})

#define                         FUSKIPFORMAT_RAISE(in, pt)\
                                if (!freadpattern( (in), (pt)) )\
                                    userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read pattern '%s'", (pt) );

#define                         FUSKIPFORMATPRINTF_RAISE(in, fmt, ...)\
                                if (!fread_pattern_printf( (in), (fmt), ##__VA_ARGS__) )\
                                    userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read dynamic pattern");

// particular for unsigned
#define                         FUGETUNSIGNED_RAISE(in) ({unsigned _tmp; if (fscanf(in, "%u", &_tmp) < 1)\
                 userraiseint(ERR_WRONG_INPUT_FORMAT, "Unable to read unsigned"); _tmp;})

// universal strict scanf, current versio for int, double, long, char *.
extern int                      fstrict_scanf(FILE * restrict f, const char *restrict fmt, ...) __attribute__ ( (format (scanf, 2, 3) ) );
#define                         strict_scanf(fmt, ...) fstrict_scanf(stdin, (fmt), ##__VA_ARGS__);

extern off_t                    getfilesize(FILE *file);

#endif /* ! _FILEUTILS_H */
