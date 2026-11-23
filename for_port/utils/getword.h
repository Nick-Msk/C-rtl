#ifndef _GETWORD_H
#define _GETWORD_H

#include "fs.h"
#include "fs_iter.h"
#include "bool.h"
#include "checker.h"
#include "log.h"
#include "buffer.h"
#include "ds.h"

extern fs                           getword(fs str, bool sens, bool comments, bool get_newline);

// STR MUST BE init with alloc flag
static inline bool                  getsimpleword(fs *str){
    *str = getword(*str, false, false, false);
    return !fsisnull(*str);
}

// LEXEM_CMD for \<cmd>
typedef enum { 
    LEXEM_UNK = -1, 
    LEXEM_WORD, 
    LEXEM_INT, 
    LEXEM_FLOAT, 
    LEXEM_SYM, 
    LEXEM_CMD,
    LEXEM_STR = 10000000 // separate api getstring!
} Lexemtype;

typedef enum {
    GETLINE_EOF = 0,
    GETLINE_LINE,
    GETLINE_PARTIAL
} GetlineStattus;

static inline const char           *Lexemtype_str(Lexemtype typ){
    switch (typ){
        CASE_RETURN(LEXEM_UNK);
        CASE_RETURN(LEXEM_WORD);
        CASE_RETURN(LEXEM_INT);
        CASE_RETURN(LEXEM_FLOAT);
        CASE_RETURN(LEXEM_SYM);
        CASE_RETURN(LEXEM_CMD);
        CASE_RETURN(LEXEM_STR);
        default: return "";
    }
}

typedef struct Lexem {
    fs          str;
    Lexemtype   typ;
} Lexem;

static inline const char           *lexemstr(Lexem l){
    return fsstr(l.str);
}
// pointer version
static inline const char           *lexem_str(const Lexem *l){
    invraise(l != 0, "Wrong input");
    return l->str.v;
}

static inline int                   lexemlen(Lexem l){
    return fslen(l.str);
}

static inline int                   lexem_len(const Lexem *l){
    invraise(l != 0, "Wrong input");
    return fslen(l->str);
}

static inline int                   lexem_techfprint(FILE *restrict out, const Lexem *restrict l){
    invraise(l != 0, "Wrong input");
    int     cnt = 0;
    if (out){
        cnt += fprintf(out, "LEXEM: %s:[", Lexemtype_str(l->typ));
        cnt += fs_techfprint(out, &l->str, "");
        cnt += fprintf(out, "]\n");
    }
    return cnt;
}

static inline int                   lexem_techprint(const Lexem *l){
    return lexem_techfprint(stdout, l);
}

static inline void                  lexem_free(Lexem *l){
    invraise(l != 0, "Wrong input");
    fsfree(l->str);
    l->typ = LEXEM_UNK;
}

#define                     lexeminit(...) {.str = FS(), .typ = LEXEM_UNK }
#define                     lexemfree(l) lexem_free( &(l) )

// true if ANY data is processed (even \n), false if EOF
// if newline then last '\n' is returned
extern bool                         getstring_nl(FILE *restrict in, fs *restrict str, bool newline, bool append);

// not using buffer.c, VERY simple, empty line is OK, just "" empty fs
static inline bool                  getpurestring(FILE *restrict in, fs *restrict str) {
    return getstring_nl(in, str, false, false);
}

// for v64gen FILE * source
extern GetlineStattus                getstring_newline_append(FILE *restrict in, fs *restrict str);

/**
 * @brief Reads a text token from a generic data source (file, string, const string).
 *
 * The token may be quoted.  When `removequot` is true the surrounding
 * double quotes are stripped and escape sequences (`\\n`, `\\r`, `\\t`,
 * `\\\\`) are converted to their real characters.  An empty quoted string
 * `""` is recognised as a valid empty token.
 *
 * @param in          data source (already initialised)
 * @param str         output fast‑string – will be resized and filled
 * @param removequot  if true, remove surrounding quotes and process escapes
 * @return true on success, false on format error or EOF
 * @throws ERR_NULLABLE_PTR if `in` or `str` is NULL
 * @throws ERR_WRONG_INPUT_FORMAT on missing / unterminated quotes or
 *         a dangling backslash at the end of input
 */
bool                        getconvstring_ds(DS *restrict in, fs *restrict str, bool removequot);

/**
 * @brief Reads a text token from a FILE
 *
 * The token may be quoted.  When `removequot` is true the surrounding
 * double quotes are stripped and escape sequences (`\\n`, `\\r`, `\\t`,
 * `\\\\`) are converted to their real characters.  An empty quoted string
 * `""` is recognised as a valid empty token.
 *
 * @param in          FILE *, opened for read
 * @param str         output fast‑string – will be resized and filled
 * @param removequot  if true, remove surrounding quotes and process escapes
 * @return true on success, false on format error or EOF
 * @throws ERR_NULLABLE_PTR if `in` or `str` is NULL
 * @throws ERR_WRONG_INPUT_FORMAT on missing / unterminated quotes or
 *         a dangling backslash at the end of input
 */
extern bool                         getconvstring(FILE *restrict in, fs *restrict str, bool removequot);
 
/**
 * @brief Reads a text token from a c-string
 *
 * The token may be quoted.  When `removequot` is true the surrounding
 * double quotes are stripped and escape sequences (`\\n`, `\\r`, `\\t`,
 * `\\\\`) are converted to their real characters.  An empty quoted string
 * `""` is recognised as a valid empty token.
 *
 * @param in          const char *
 * @param str         output fast‑string – will be resized and filled
 * @param removequot  if true, remove surrounding quotes and process escapes
 * @return true on success, false on format error or EOF
 * @throws ERR_NULLABLE_PTR if `in` or `str` is NULL
 * @throws ERR_WRONG_INPUT_FORMAT on missing / unterminated quotes or
 *         a dangling backslash at the end of input
 */
extern bool                         getconvstring_cstr(const char *restrict in, fs *restrict str, bool removequot);
// parse only LEXEM_STR or LEXEM_CMD!
extern bool                         getstring(Lexem *lex);
// any lexem, word or number
extern bool                         getlexem(Lexem *lex, bool ign_comments);

// invariant: lex != 0 && str != 0
static inline bool                  lexem_eq(const Lexem *restrict lex, const char *restrict str, int len){
    invraise(lex != 0 && str != 0, "Wrong input %p %p", lex, str);
    return strncmp(lex->str.v, str, len) == 0;
}
// fs port
static inline bool                  lexem_fseq(const Lexem *restrict lex, const fs *restrict str, int len){
    invraise(lex != 0 && str != 0, "Wrong input %p %p", lex, str);
    return fs_ncmp(&lex->str, str, len) == 0;
}
// comparator
static inline bool                  lexem_cmp(const Lexem *restrict lex, const char *restrict str){
    invraise(lex != 0 && str != 0, "Wrong input %p %p", lex, str);
    return strcmp(lex->str.v, str);
}
// n-comparator
static inline bool                  lexem_ncmp(const Lexem *restrict lex, const char *restrict str, int cnt){
    invraise(lex != 0 && str != 0, "Wrong input %p %p", lex, str);
    return strncmp(lex->str.v, str, cnt);
}

#endif /* ! _GETWORD_H */

