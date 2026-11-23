
#include "getword.h"

// ------------------------------------ PUBLIC API ----------------------------------------

// probably some configuration is required to  gather tolower, comments, skip_newline
static inline int       skip_spaces(bool get_newline){
    int c;
    while (isspace( c = getch() ) )
        if (get_newline && c == '\n')
            break;
    return c;
}

static int               skip_cl(void){
    int             cnt = 0;        // total count of comments
    int             c, c1;
    bool            comment_and_lines = true;

    while (comment_and_lines){

        c = skip_spaces(false);
        // SKIP comment or literals
        comment_and_lines = false;
        if (c == '/'){
            if ( (c1 = getch() ) == '/'){  // start comment type 1
                cnt++;
                comment_and_lines = true;
                while ( (c1 = getch()) != EOF && c1 != '\n')
                    ;
            } else if (c1 == '*'){  // comment type 2
                cnt++;
                comment_and_lines = true;
                do {
                    while ( (c1 = getch()) != EOF && c1 != '*')
                        ;
                } while ( (c1 = getch() ) != '/' && c1 != EOF);  // end of /* */ /*
            } else  // not a comment! comment_and_lines remains false
                c = c1; // just like ungetch
        } else if (c == '"') {
            comment_and_lines = true;   // line, so setup flag
            while ( (c = getch()) != EOF && c != '"')
                ;
        } else if (c == '\'') {
            comment_and_lines = true;   // anyway!
            while ( (c = getch()) != EOF && c != '\'')
                ;
        }
        //logsimple("comment_and_lines %s", bool_str(comment_and_lines));
    }
    return logsimpleret(c, "[%c], comments %d", c, cnt);
}

static inline int       charconv(int c){
    switch (c) {
        case 'n':  return '\n'; break;
        case 'r':  return '\r'; break;
        case 't':  return '\t'; break;
        case '"':  return '"';  break;
        case '\\': return '\\'; break;
        default:   return c;    break;
    }
}

bool                        getconvstring_ds(DS *restrict in, fs *restrict str, bool removequot) {
    invraisecode(in != NULL && str != NULL, ERR_NULLABLE_PTR, 
        "Null pointers %p - %p", in, str);

    int         c;
    bool        skipped_first = false;   // true after the opening quote has been consumed
    fsnew       iter = fsinew(str);     //  iterator for the fast‑string

    while ((c = dsgetc(in)) != EOF && c != '\n') {
        // ---------- opening quote ----------
        if (removequot && !skipped_first && iter.pos == 0) {
            if (c == '"') {
                skipped_first = true;
                // peek ahead – might be an empty quoted string ""
                int next = dsgetc(in);
                if (next == '"') {
                    // empty string – success, buffer stays empty
                    elemend(iter);
                    return logsimpleret(true, "empty quoted string");
                } else if (next == EOF) {
                    elemend(iter);
                    return userraise(false, ERR_WRONG_INPUT_FORMAT,
                                     "Unterminated empty quoted string");
                } else {
                    // put the character back; it is the first content byte
                    dsungetc(next, in);
                    continue;   // next iteration will write it into the buffer
                }
            } else {
                elemend(iter);
                return userraise(false, ERR_WRONG_INPUT_FORMAT, "Missing opening quote");
            }
        }

        // ---------- closing quote (when inside a quoted string) ----------
        if (removequot && skipped_first && c == '"') {
            // closing quote – end of token, do NOT store it
            elemend(iter);
            return logsimpleret(true, "line %zu [%.10s]", fs_len(str), fs_str(str));
        }

        // ---------- escape sequences ----------
        if (c == '\\') {
            int tmp = dsgetc(in);
            if (tmp == EOF) {
                elemend(iter);
                return userraise(false, ERR_WRONG_INPUT_FORMAT,
                                 "Escape at end of input");
            }
            c = charconv(tmp);   // convert \n, \r, \t, \\ to real characters
        }

        // ---------- ordinary character ----------
        elemnext(iter) = c;
    }

    // after the loop: if we expected a closing quote but didn't get one
    if (removequot && skipped_first) {
        elemend(iter);
        return userraise(false, ERR_WRONG_INPUT_FORMAT, "No trailing \" found");
    }

    elemend(iter);

    // no data at all
    if (c == EOF && fs_len(str) == 0)
        return logsimpleret(false, "EOF");

    return logsimpleret(true, "line %zu [%.30s]", fs_len(str), fs_str(str));
}

// ------------------------------------ PUBLIC API ----------------------------------------

// str must have heap alloc
fs                      getword(fs str, bool lower, bool comments, bool get_newline){

    logenter("tolower %s, get_newline %s", bool_str(lower), bool_str(get_newline) );

    fsclear(str);   // reset
    int              c;
    fsnew            iter = fsinew(&str);

    c = comments ? skip_spaces(get_newline) : skip_cl();       // comment and so on are allowed! TODO: probably use flag -c
    if (c != EOF){
        elemnext(iter) = clower(c, lower);
    } else
        elemclear(iter);    // end flag

    if (!isalpha_u(c) ){
        elemend(iter);
        return logret(str, "%c:%zu - [%s]", c, fslen(str), str.v);
    }
    while ( (c = getch()) != EOF){
        if (!isalnum_u(c) ){
            ungetch(c);
            break;
        } else
            elemnext(iter) = clower(c, lower);
    }
    elemend(iter);

    return logret(str, "%zu - [%s]", fslen(str), str.v); // that is probably new str
}
// true if ANY data is processed (even \n), false if EOF
// if newline then last '\n' is returned
bool                    getstring_nl(FILE *restrict in, fs *restrict str, bool newline, bool append) {
    invraisecode(in != NULL && str != NULL, ERR_NULLABLE_PTR, "%p - %p", in, str);
    
    int     c;
    size_t  initlen = append ? fs_len(str): 0;
    fsnew   iter = append ? fsiapp(str) : fsinew(str);
    while ( (c = getc(in)) != EOF && c != '\n')
        elemnext(iter) = c;
    if (newline && c == '\n')
        elemnext(iter) = c;
    elemend(iter);
    
    return c != EOF || fs_len(str) != initlen;
}

// for v64gen FILE * source
GetlineStattus          getstring_newline_append(FILE *restrict in, fs *restrict str) {
    if (!getstring_nl(in, str, true, true) )
        return GETLINE_EOF;     // no new data at all
    size_t lastpos = fs_len(str) - 1;
    if (str->v[lastpos] == '\n') {
        fs_setlen(str, lastpos);
        return GETLINE_LINE;
    }
    else
        return GETLINE_PARTIAL;
}

// not using buffer.c, VERY simple, empty line is OK, just "" empty fs
/*bool                    getpurestring(FILE *restrict in, fs *restrict str){
    invraisecode(in != NULL && str != NULL, ERR_NULLABLE_PTR, "%p - %p", in, str);

    int     c;
    fsnew   iter = fsinew(str);
    while ( (c = getc(in)) != EOF && c != '\n')
        elemnext(iter) = c;
    elemend(iter);
    if (c == EOF && fs_len(str) == 0)   // no data at all
        return logsimpleret(false, "EOF");
    else
        return logsimpleret(true, "line %zu [%10s]", fs_len(str), fs_str(str) );
}*/

/*bool                     getnwelinestring(FILE *restrict in, fs *restrict str) {
    fsnew iter = fsiapp(str);
    int   c;
    while ((c = fgetc(in)) != EOF && c != '\n') {
        elemnext(iter) = c;
    }
    elemend(iter);          // завершаем строку '\0' и обновляем len

    return c == '\n';
}*/


// conversion string from FILE *
bool                 getconvstring(FILE *restrict in, fs *restrict str, bool removequot) {
    invraisecode(in != NULL && str != NULL, ERR_NULLABLE_PTR,
        "Nullable input %p %p", in, str);
    DS s = dsCreatef(in);
    return getconvstring_ds(&s, str, removequot);
}
// conversion string from const char *
bool                 getconvstring_cstr(const char *restrict in, fs *restrict str, bool removequot){
    invraisecode(in != NULL && str != NULL, ERR_NULLABLE_PTR,
        "Nullable input %p %p", in, str);
    DS s = dsCreateconst(in);
    return getconvstring_ds(&s, str, removequot);
}


// parse only LEXEM_STR or LEXEM_CMD!
bool                    getstring(Lexem *lex){

    int     c;
    fsnew   iter = fsinew(&lex->str);

    c = skip_spaces(true);
    logsimple("[%c]", c);
    if (c == EOF){
        lex->typ = LEXEM_UNK;
        return logsimpleret(false, "EOF detected");
    } else if (c == '\\')
        lex->typ = LEXEM_CMD;  // if command, then just return line without init '\'
    else {
        lex->typ = LEXEM_STR;
        if (c !='\n')
            elemnext(iter) = c;
        else {
            elemend(iter);
            return logsimpleret(true, "Empty line!");
        }
    }
    while ( (c = getch()) != EOF && c != '\n')
        elemnext(iter) = c;
    elemend(iter);
    // no need to ungetch

    return logsimpleret(true, "Parsed as str %s:[%s]", Lexemtype_str(lex->typ), lex->str.v);
}

bool                    getlexem(Lexem *lex, bool ign_comments){
    int     c;
    fsnew   iterstr;    // iterator-constructor

    c = ign_comments ? skip_cl() : skip_spaces(false);
    if (c != EOF){
        fsclear(lex->str);  // only if any valuable input
        iterstr = fsinew(&lex->str);
    } else
        return logsimpleret(false, "EOF detected");
    lex->typ = LEXEM_UNK;  // init state
    elemnext(iterstr) = c; // put into fs
    // check the numbers
    if (isdigit_signed(c) ){
        if (c == '+' || c == '-')
            c = getch();
        //
        if (!isdigit(c) ){   // not a number actually
            lex->typ = LEXEM_SYM;
        } else {
            while ( (c = getch()) != EOF && isdigit(c) )
                elemnext(iterstr) = c;
            lex->typ = LEXEM_INT;
        }
    } else if (isalnum_u(c) || c == '\\' ){      // just identifier or so on
        if (c == '\\'){
            lex->typ = LEXEM_CMD;
            iterstr.pos--;  // remove command sign
        } else
            lex->typ = LEXEM_WORD;
        // process the identifier
        while ( (c = getch()) != EOF && isalnum_u(c) )
            elemnext(iterstr) = c;
    } else {
        elemend(iterstr);
        return logsimpleret(true, "Unknown symbol [%c]", c);
    }
    elemend(iterstr);
    ungetch(c);
    return logsimpleret(true, "Parsed %s:%s", Lexemtype_str(lex->typ), lex->str.v );
}

// -------------------------------Testing --------------------------
#ifdef GETWORDTESTING

#include "test.h"
#include "checker.h"
#include "fs_array.h"

//types for testing

// ------------------------- TEST 1 ---------------------------------
static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: read input by getstring()", ++subnum);
    {
        const char  fname[] = "res/getword/test_file_getstring.dat";
        const char *pts[] = { "Fiest line xxx", "Second line yyyyy", "Third line oooooo", 0};
        FILE        *f = fopen(fname, "w+");
        if (!f)
            return logerr(TEST_FAILED, "Unable to open %s for w+", fname);

        fsarray     fa = fsarr_fromarr(pts, 0);
        fsarr_fsavelines(f, &fa, 0);  // save ONLY lines, divided by '\n'

        rewind(f);
        buffer_set(f);  // read from f instead of stdin

        Lexem       lex = lexeminit();
        int         i = 0;
        while (getstring(&lex) ){
            test_validatefree(lex.typ = LEXEM_STR, (lexemfree(lex), fsarrfree(fa), fclose(f) ),
                "Lexem type must be LEXEM_STR but not %s", Lexemtype_str(lex.typ) );
            test_validatefree(lexem_cmp(&lex, pts[i]) == 0, (lexemfree(lex), fsarrfree(fa), fclose(f) ),
                "String must be [%s] but not [%s]", lexemstr(lex), pts[i] );
            logauto(i++);
        }

        lexemfree(lex);
        fsarrfree(fa);
        fclose(f);
    }

    test_sub("subtest %d: read input by with command getstring()", ++subnum);
    {
        const char  fname[] = "res/getword/test_file_getstring2.dat";
        // odd - LEXEM_STR
        const char *pts[] = { "First line 1", "\\Cmd line 1 xxx", "Second line yyyyy", "\\Cmd line 2 oooooo", "", "\\CMD 3", "Third (4) line oooooo", 0};
        FILE        *f = fopen(fname, "w+");
        if (!f)
            return logerr(TEST_FAILED, "Unable to open %s for w+", fname);

        fsarray     fa = fsarr_fromarr(pts, 0);
        fsarr_fsavelines(f, &fa, 0);  // save ONLY lines, divided by '\n'

        rewind(f);
        buffer_set(f);  // read from f instead of stdin

        Lexem       lex = lexeminit();
        int         i = 0;
        while (getstring(&lex) ){
            Lexemtype typ = (i % 2 == 0) ? LEXEM_STR : LEXEM_CMD;
            test_validatefree(lex.typ = LEXEM_STR, (lexemfree(lex), fsarrfree(fa), fclose(f) ),
                "Iter %d: Lexem type must be %s but not %s", i, Lexemtype_str(typ), Lexemtype_str(lex.typ)
            );
            test_validatefree(lexemlen(lex) == (int) strlen(pts[i] + (typ == LEXEM_CMD ? 1: 0) ),  (lexemfree(lex), fsarrfree(fa), fclose(f) ),
                "Iter %d: Length of lexem = %d but not %lu", i, lexemlen(lex), strlen(pts[i] + (typ == LEXEM_CMD ? 1: 0) )
            );
            test_validatefree(lexem_cmp(&lex, pts[i] + (typ == LEXEM_CMD ? 1: 0) ) == 0, (lexemfree(lex), fsarrfree(fa), fclose(f) ),
                "Iter %d: String must be [%s] but not [%s]", i, lexemstr(lex), pts[i] + (typ == LEXEM_CMD ? 1: 0)
            );
            i++;
        }

        lexemfree(lex);
        fsarrfree(fa);
        fclose(f);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 2 ---------------------------------
static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: empty getpurestring()", ++subnum);
    {
        const char  fname[] = "res/getword/test_file_puregetstring.dat";
        FILE        *f = fopen(fname, "w+");
        if (!f)
            return userraise(TEST_FAILED, ERR_UNABLE_OPEN_FILE_WRITE, "Unable to open %s for w+", fname);

        fclose(f);
        f = fopen(fname, "r");
        if (!f)
            return userraise(TEST_FAILED, ERR_UNABLE_OPEN_FILE_READ, "Unable to open %s for r", fname);
        fs      s = FS();
        test_validatefree(
            getpurestring(f, &s) == false,
            (fsfree(s), fclose(f) ),
            "Must not be read"
        );
        fsfree(s);
        fclose(f);
    }
    test_sub("subtest %d: read 1 line getpurestring()", ++subnum);
    {
        const char  fname[] = "res/getword/test_file_puregetstring.dat";
        FILE        *f = fopen(fname, "w+");
        if (!f)
            return logerr(TEST_FAILED, "Unable to open %s for w+", fname);
        fprintf(f, "%s\n", fname);
        rewind(f);
        fs      s = FS();
        test_validatefree(
            getpurestring(f, &s) && strcmp(fname, fsstr(s) ) == 0,
            (fsfree(s), fclose(f) ),
            "Line must be equal [%s] vs origin [%s]", fsstr(s), fname
        );

        fsfree(s);
        fclose(f);
    }
    test_sub("subtest %d: read multiples lines via getpurestring()", ++subnum);
    {
        const char  fname[] = "res/getword/test_file_puregetstring.dat";
        FILE        *f = fopen(fname, "w+");
        const int    cnt = 12;
        if (!f)
            return logerr(TEST_FAILED, "Unable to open %s for w+", fname);
        for (int i = 0; i < cnt; i++)
            fprintf(f, "%d - %s\n", i, fname);
        rewind(f);
        fs      s = FS();
        char    buf[200];
        bool    res;
        for (int i = 0; i < cnt; i++){
            snprintf(buf, sizeof(buf) -  1, "%d - %s", i, fname);
            test_validatefree(
                (res = getpurestring(f, &s) ) && strcmp(buf, fsstr(s) ) == 0,
                (fsfree(s), fclose(f) ),
                "%s - %d: Line must be equal [%s]/%zu vs origin [%s]/%lu", 
                bool_str(res), i, fsstr(s), fslen(s), buf, strlen(buf)
            );
        }
        fsfree(s);
        fclose(f);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST getconvstring ---------------------------------

static TestStatus
tf_getconvstring(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Обычная строка без escape-символов */
    test_sub("subtest %d: plain string", ++subnum);
    {
        const char      fname[] = "res/getword/conv_plain.txt";
        FILE           *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        fprintf(f, "hello world\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);

        fs s = FS();
        test_validatefree(
            getconvstring(f, &s, false) && strcmp(fs_str(&s), "hello world") == 0,
            (fsfree(s), fclose(f)),
            "Plain string mismatch: got '%s', expected 'hello world'", fs_str(&s)
        );
        fsfree(s);
        fclose(f);
        fs_alloc_check(true);
    }

    /* 2. Строка с escape-последовательностями (\\n, \\t, \\\", \\\\) */
    test_sub("subtest %d: escaped string", ++subnum);
    {
        const char      fname[] = "res/getword/conv_escaped.txt";
        FILE           *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        // Записываем строку, содержащую символы, которые будут выглядеть как escape-последовательности в файле
        // В файле мы должны записать "line1\\nline2\\tend\\\"quote\\\\"
        fprintf(f, "line1\\nline2\\tend\\\"quote\\\\\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);

        fs s = FS();
        // После getconvstring должны получить: "line1\nline2\tend\"quote\\"
        test_validatefree(
            getconvstring(f, &s, false) && strcmp(fs_str(&s), "line1\nline2\tend\"quote\\") == 0,
            (fsfree(s), fclose(f)),
            "Escaped string mismatch: got '%s'", fs_str(&s)
        );
        fsfree(s);
        fclose(f);
        fs_alloc_check(true);
    }

    /* 3. Пустая строка (только \\n) */
    test_sub("subtest %d: empty string (just newline)", ++subnum);
    {
        const char fname[] = "res/getword/conv_empty.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        fprintf(f, "\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);

        fs s = FS();
        test_validatefree(
            getconvstring(f, &s, false) && fs_len(&s) == 0,
            (fsfree(s), fclose(f)),
            "Empty string mismatch: len=%zu, expected 0, str='%s'", fs_len(&s), fs_str(&s)
        );
        fsfree(s);
        fclose(f);
        fs_alloc_check(true);
    }

    /* 4. Неизвестная escape-последовательность (например, \\x) */
    test_sub("subtest %d: unknown escape", ++subnum);
    {
        const char fname[] = "res/getword/conv_unknown_esc.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        fprintf(f, "hello\\xworld\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);

        fs s = FS();
        // Поскольку 'x' не является известной escape-последовательностью, charconv вернёт 'x',
        // и в строке должно остаться "helloxworld"
        test_validatefree(
            getconvstring(f, &s, false) && strcmp(fs_str(&s), "helloxworld") == 0,
            (fsfree(s), fclose(f)),
            "Unknown escape mismatch: got '%s', expected 'helloxworld'", fs_str(&s)
        );
        fsfree(s);
        fclose(f);
        fs_alloc_check(true);
    }

    /* 5. Строка без завершающего \\n в конце файла (последняя строка) */
    test_sub("subtest %d: last line without newline", ++subnum);
    {
        const char fname[] = "res/getword/conv_last_line.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        // Записываем строку без \n в конце
        fprintf(f, "final line");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);

        fs s = FS();
        test_validatefree(
            getconvstring(f, &s, false) && strcmp(fs_str(&s), "final line") == 0,
            (fsfree(s), fclose(f)),
            "Last line mismatch: got '%s', expected 'final line'", fs_str(&s)
        );
        fsfree(s);
        fclose(f);
        fs_alloc_check(true);
    }

    /* 6. EOF без данных (пустой файл) */
    test_sub("subtest %d: eof on empty file", ++subnum);
    {
        const char fname[] = "res/getword/conv_eof.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for writing", fname);
        fclose(f);   // создали пустой файл

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s for reading", fname);

        fs s = FS();
        test_validatefree(
            !getconvstring(f, &s, false),   // должно вернуть false (EOF)
            (fsfree(s), fclose(f)),
            "EOF on empty file must return false"
        );
        fsfree(s);
        fclose(f);
        fs_alloc_check(true);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST getconvstring removequot==true ---------------------------------

static TestStatus
tf_getconvstring_removequot(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Парные кавычки – успех */
    test_sub("subtest %d: paired quotes", ++subnum);
    {
        const char fname[] = "res/getword/conv_rmq_paired.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);
        fprintf(f, "\"hello world\"\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);

        fs s = FS();
        test_validatefree(
            getconvstring(f, &s, true) && strcmp(fs_str(&s), "hello world") == 0,
            (fsfree(s), fclose(f)),
            "Paired quotes: got '%s', expected 'hello world'", fs_str(&s)
        );
        fsfree(s);
        fclose(f);
    }

    /* 2. Пустая строка "" – успех */
    test_sub("subtest %d: empty quoted", ++subnum);
    {
        const char fname[] = "res/getword/conv_rmq_empty.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);
        fprintf(f, "\"\"\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);

        fs s = FS();
        test_validatefree(
            getconvstring(f, &s, true) && fs_len(&s) == 0,
            (fsfree(s), fclose(f)),
            "Empty quoted: len=%zu, expected 0", fs_len(&s)
        );
        fsfree(s);
        fclose(f);
    }

    /* 3. Escape-последовательности внутри кавычек – успех */
    test_sub("subtest %d: escaped inside quotes", ++subnum);
    {
        const char fname[] = "res/getword/conv_rmq_esc.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);
        fprintf(f, "\"line1\\nline2\\tend\\\"quote\\\\\"\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);

        fs s = FS();
        test_validatefree(
            getconvstring(f, &s, true) &&
            strcmp(fs_str(&s), "line1\nline2\tend\"quote\\") == 0,
            (fsfree(s), fclose(f)),
            "Escaped inside quotes: got '%s'", fs_str(&s)
        );
        fsfree(s);
        fclose(f);
    }

    /* 4. Нет начальной кавычки – ошибка формата (возвращает false) */
    test_sub("subtest %d: missing opening quote", ++subnum);
    {
        const char fname[] = "res/getword/conv_rmq_no_open.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);
        fprintf(f, "no opening\"\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);

        fs s = FS();
        test_validatefree(
            !getconvstring(f, &s, true),   // должно вернуть false
            (fsfree(s), fclose(f)),
            "Missing opening quote must return false"
        );
        fsfree(s);
        fclose(f);
    }

    /* 5. Нет конечной кавычки – ошибка формата (возвращает false) */
    test_sub("subtest %d: missing closing quote", ++subnum);
    {
        const char fname[] = "res/getword/conv_rmq_no_close.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);
        fprintf(f, "\"no closing\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);

        fs s = FS();
        test_validatefree(
            !getconvstring(f, &s, true),
            (fsfree(s), fclose(f)),
            "Missing closing quote must return false"
        );
        fsfree(s);
        fclose(f);
    }

    /* 6. Вообще без кавычек – ошибка формата (возвращает false) */
    test_sub("subtest %d: no quotes at all", ++subnum);
    {
        const char fname[] = "res/getword/conv_rmq_no_quotes.txt";
        FILE *f = fopen(fname, "w");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);
        fprintf(f, "plain string\n");
        fclose(f);

        f = fopen(fname, "r");
        if (!f)
            return logerr(TEST_FAILED, "Cannot open %s", fname);

        fs s = FS();
        test_validatefree(
            !getconvstring(f, &s, true),
            (fsfree(s), fclose(f)),
            "No quotes must return false"
        );
        fsfree(s);
        fclose(f);
    }


    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST getconvstring_str() simple test ---------------------------------

static TestStatus
tf_getconvstring_str(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: getconvstring from file (quoted)", ++subnum);
    {
        const char *fname = "res/ds/test_getconv_file.txt";
        FILE *fp = fopen(fname, "w");
        fprintf(fp, "\"hello world\"");
        fclose(fp);

        fp = fopen(fname, "r");
        fs buf = FS();
        test_validatefree(
            getconvstring(fp, &buf, true) && fs_len(&buf) == 11 &&
            strcmp(fs_str(&buf), "hello world") == 0,
            fsfree(buf),
            "getconvstring file quoted: len=%zu, str='%s' (expected 11, 'hello world')",
            fs_len(&buf), fs_str(&buf)
        );
        fsfree(buf);
    }

    test_sub("subtest %d: getconvstring_cstr (quoted)", ++subnum);
    {
        const char *input = "\"Hello, World!\"";
        fs buf = FS();
        test_validatefree(
            getconvstring_cstr(input, &buf, true) && fs_len(&buf) == 13 &&
            strcmp(fs_str(&buf), "Hello, World!") == 0,
            fsfree(buf),
            "getconvstring_cstr quoted: len=%zu, str='%s'", fs_len(&buf), fs_str(&buf)
        );
        fsfree(buf);
    }

    test_sub("subtest %d: getconvstring_cstr (unquoted)", ++subnum);
    {
        const char *input = "plain";
        fs buf = FS();
        test_validatefree(
            getconvstring_cstr(input, &buf, false) && fs_len(&buf) == 5 &&
            strcmp(fs_str(&buf), "plain") == 0,
            fsfree(buf),
            "getconvstring_cstr unquoted: len=%zu, str='%s'", fs_len(&buf), fs_str(&buf)
        );
        fsfree(buf);
    }

    test_sub("subtest %d: getconvstring_cstr empty", ++subnum);
    {
        const char *input = "";
        fs buf = FS();
        test_validatefree(
            !getconvstring_cstr(input, &buf, true),
            fsfree(buf),
            "getconvstring_cstr empty: must return false, len=%zu", fs_len(&buf)
        );
        fsfree(buf);
    }

    test_sub("subtest %d: getconvstring_cstr missing opening quote", ++subnum);
    {
        const char *input = "noquote\"";
        fs buf = FS();
        test_validatefree(
            !getconvstring_cstr(input, &buf, true),
            fsfree(buf),
            "getconvstring_cstr missing opening quote: must return false"
        );
        fsfree(buf);
    }

    test_sub("subtest %d: getconvstring_cstr escape sequences", ++subnum);
    {
        const char *input = "\"a\\nb\"";
        fs buf = FS();
        test_validatefree(
            getconvstring_cstr(input, &buf, true) && fs_len(&buf) == 3 &&
            buf.v[0] == 'a' && buf.v[1] == '\n' && buf.v[2] == 'b',
            fsfree(buf),
            "getconvstring_cstr escape: len=%zu, expected 3, chars='a','\\n','b'", fs_len(&buf)
        );
        fsfree(buf);
    }
    /* ========== 11. getconvstring_cstr empty quoted ========== */
    test_sub("subtest %d: getconvstring_cstr empty quoted", ++subnum);
    {
        const char *input = "\"\"";
        fs buf = FS();
        test_validatefree(
            getconvstring_cstr(input, &buf, true) && fs_len(&buf) == 0,
            fsfree(buf),
            "Empty quoted string must succeed and produce empty buffer, len=%zu", fs_len(&buf)
        );
        fsfree(buf);
    }

    /* ========== 12. getconvstring_cstr hanging backslash ========== */
    test_sub("subtest %d: getconvstring_cstr hanging backslash", ++subnum);
    {
        const char *input = "\"value\\";
        fs buf = FS();
        test_validatefree(
            !getconvstring_cstr(input, &buf, true),
            fsfree(buf),
            "Hanging backslash must cause error"
        );
        fsfree(buf);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST getstring_newline_append -------------------------
static TestStatus
tf6_getstring_newline_append(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: full lines and partial", ++subnum);
    {
        const char *fname = "res/getword/getword_nl_test.dat";
        FILE *w = fopen(fname, "w");
        test_validate(w != NULL, "fopen(w) failed");
        fwrite("line1\nline2\nlast", 1, 17, w);
        fclose(w);

        FILE *fr = fopen(fname, "r");
        test_validate(fr != NULL, "fopen(r) failed");

        fs buf = FS();

        GetlineStattus st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_LINE && fs_cmpstr(&buf, "line1") == 0,
                          (fsfree(buf), fclose(fr)),
                          "first line mismatch");
        fs_clear(&buf);

        st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_LINE && fs_cmpstr(&buf, "line2") == 0,
                          (fsfree(buf), fclose(fr)),
                          "second line mismatch");
        fs_clear(&buf);

        st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_PARTIAL && fs_cmpstr(&buf, "last") == 0,
                          (fsfree(buf), fclose(fr)),
                          "partial line mismatch");

        // После частичной строки следующий вызов должен дать EOF, буфер не меняется
        st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_EOF && fs_cmpstr(&buf, "last") == 0,
                          (fsfree(buf), fclose(fr)),
                          "expected EOF after partial");

        fsfree(buf);
        fclose(fr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: empty lines", ++subnum);
    {
        const char *fname = "res/getword/getword_nl_empty.dat";
        FILE *w = fopen(fname, "w");
        test_validate(w != NULL, "fopen(w) failed");
        fwrite("\n\n", 1, 2, w);
        fclose(w);

        FILE *fr = fopen(fname, "r");
        test_validate(fr != NULL, "fopen(r) failed");

        fs buf = FS();

        GetlineStattus st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_LINE && fs_len(&buf) == 0,
                          (fsfree(buf), fclose(fr)),
                          "first empty line mismatch");
        fs_clear(&buf);

        st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_LINE && fs_len(&buf) == 0,
                          (fsfree(buf), fclose(fr)),
                          "second empty line mismatch");
        fs_clear(&buf);

        st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_EOF && fs_len(&buf) == 0,
                          (fsfree(buf), fclose(fr)),
                          "expected EOF after empty lines");

        fsfree(buf);
        fclose(fr);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: empty file", ++subnum);
    {
        const char *fname = "res/getword/getword_nl_emptyfile.dat";
        FILE *w = fopen(fname, "w");
        test_validate(w != NULL, "fopen(w) failed");
        fclose(w);

        FILE *fr = fopen(fname, "r");
        test_validate(fr != NULL, "fopen(r) failed");

        fs buf = FS();

        GetlineStattus st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_EOF && fs_len(&buf) == 0,
                          (fsfree(buf), fclose(fr)),
                          "expected EOF for empty file");

        fsfree(buf);
        fclose(fr);
        remove(fname);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: resume after EOF with partial buffer", ++subnum);
    {
        const char *fname = "res/getword/getword_resume_partial.tmp";

        // Начальный файл: строка без завершающего \n
        FILE *w = fopen(fname, "w");
        test_validatefree(w != NULL, remove(fname), "fopen(w) failed");
        fwrite("part", 1, 4, w);
        fclose(w);

        FILE *fr = fopen(fname, "r");
        FILE *fa = fopen(fname, "a");
        test_validatefree(fr && fa, (fr ? fclose(fr) : 0, fa ? fclose(fa) : 0),
                        "fopen failed");

        fs buf = FS();

        // Читаем: достигнут EOF, но в буфере неполная строка
        GetlineStattus st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_PARTIAL && fs_cmpstr(&buf, "part") == 0,
                        (fsfree(buf), fclose(fr), fclose(fa)),
                        "expected partial line but got '%s'", fsstr(buf) );
        
        // Не очищаем буфер, чтобы дописать к нему новые данные

        // Дописываем остаток строки с \n
        fwrite("ial\n", 1, 4, fa);
        fflush(fa);

        //fseek(fr, 0L, SEEK_CUR);
        clearerr(fr);
        // Повторный вызов должен дописать к буферу и вернуть полную строку
        st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_LINE && fs_cmpstr(&buf, "partial") == 0,
                        (fsfree(buf), fclose(fr), fclose(fa)),
                        "after append expected 'partial' but got '%s'", fsstr(buf) );

        fsfree(buf);
        fclose(fr);
        fclose(fa);
        fs_alloc_check(true);
    }

    test_sub("subtest %d: resume after EOF from empty file", ++subnum);
    {
        const char *fname = "res/getword/getword_resume_emptyfile.tmp";
        FILE *w = fopen(fname, "w");
        test_validatefree(w != NULL, remove(fname), "fopen(w) failed");
        fclose(w);

        FILE *fr = fopen(fname, "r");
        FILE *fa = fopen(fname, "a");
        test_validatefree(fr && fa, (fr ? fclose(fr) :0, fa ? fclose(fa): 0),
                        "fopen failed");

        fs buf = FS();

        // Первый вызов: файл пуст -> EOF, буфер пуст
        GetlineStattus st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_EOF && fs_len(&buf) == 0,
                        (fsfree(buf), fclose(fr), fclose(fa)),
                        "expected EOF for empty file");

        // Дописываем строку
        fwrite("new\n", 1, 4, fa);
        fflush(fa);

        //fseek(fr, 0L, SEEK_CUR);
        clearerr(fr);
        // Повторный вызов должен прочитать новую строку
        st = getstring_newline_append(fr, &buf);
        test_validatefree(st == GETLINE_LINE && fs_cmpstr(&buf, "new") == 0,
                        (fsfree(buf), fclose(fr), fclose(fa)),
                        "after append expected 'new'");

        fsfree(buf);
        fclose(fr);
        fclose(fa);
        fs_alloc_check(true);
    }

    return TEST_PASSED;
}

// ------------------------------------------------------------------------------------------------------------------------------
int
main( /* int argc, const char *argv[] */)
{
    logsimpleinit("Start");

    testenginestd(
        TESTADD(tf1,                            "getstring() simple file test"),
        TESTADD(tf2,                            "getpurestring() simple file test"),
        TESTADD(tf_getconvstring,               "getconvstring() simple file test"),
        TESTADD(tf_getconvstring_removequot,    "getconvstring() removequot==true simple file test"),
        TESTADD(tf_getconvstring_str,           "getconvstring_str() simple test"),
        TESTADD(tf6_getstring_newline_append,   "getstring_newline_append() simple test")
    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* GETWORDTESTING */

