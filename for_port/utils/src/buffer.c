#include "bool.h"
#include "buffer.h"

/********************************************************************
                 BUFFER MODULE IMPLEMENTATION
********************************************************************/

// static globals

static const int            BUFSIZE = 8192;
static char                 buf[BUFSIZE];
static int                  bufp = 0;
static int                  eofpos = -1;
static FILE                *input = 0;

// internal type

// ---------- pseudo-header for utility procedures -----------------

// ------------------------- CONSTRUCTOTS/DESTRUCTORS --------------

void                    buffer_clear(void){
    bufp = 0;
    eofpos = -1;
}
bool                    buffer_set(FILE *in){
    return (input = in) == 0;
}
// ------------------------------ Utilities ------------------------

int                     getch(void){
    if (input == 0)
        input = stdin;
    if (bufp == eofpos)
        return EOF;
    else
        return bufp > 0 ? buf[--bufp] : getc(input);
}
void                    ungetch(int c){
    if (bufp >= BUFSIZE)
        fprintf(stderr, "Unable to ungetch [%c], because of overflow (%d)\n", c, bufp);
    else {
        if (c != EOF)
            buf[bufp++] = c;
        else
            eofpos = bufp;
    }
}

int                     ungets(const char *s){
    const char * sp = s;
    while (*sp && bufp < BUFSIZE)
        buf[bufp++] = *sp++;
    return sp - s;  // count of ungetched chars
}

// TODO: the should be replaced to fs
int                     ungetrevs(const char *s, int len){
    int i = len - 1, cnt = 0;
    while (i >= 0 && bufp < BUFSIZE)
        buf[bufp++] = s[i--], cnt++;
    return cnt;  // count of ungetched chars
}

// -------------------------- (Utility) printers -------------------

// --------------------------- API ---------------------------------

// -------------------------- (API) printers -----------------------

int                     buffer_fprint(FILE *f){
    int cnt = 0;
    cnt = fprintf(f, "BUFFER (%d)/(%d):\n", bufp, eofpos);
    for (int i = 0; i < bufp; i++){
        cnt += fprintf(f, "[%d] - [%c]\t", i, buf[i]);
        if ( (i + 1) % 10 == 0)
             cnt += fprintf(f, "\n");
    }
    cnt += fprintf(f, "\n-------------------------------------\n");
    return cnt;
}

// -------------------------------Testing --------------------------

#ifdef BUFFERTESTING

#include <signal.h>
#include <strings.h>
#include "test.h"

//types for testing

// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    {
        test_sub("subtest %d", ++subnum);
        char res, some_val = 'e';
        ungetch(some_val);
        res = getch();
        if (res != some_val)
            return logerr(TEST_FAILED, "Getch returns [%c] but is must be [%c]", res, some_val);
    }
    {
        test_sub("subtest %d", ++subnum);

        char res, some_val;;
        for (int i = 0; i < 26; i++)
            ungetch(i + 'a');
        for (int i = 0; i < 26; i++){
            res = getch();
            some_val = 25 - i + 'a';
            if (res != some_val)
                return logerr(TEST_FAILED, "Getch returns [%c] but is must be [%c] (i = %d)", res, some_val, i);
        }
    }
    {
        test_sub("subtest %d", ++subnum);
        ungetch(EOF);
        char some_val = 'x';
        int res;
        ungetch(some_val);
        if ( (res = getch()) != some_val)
            return logerr(TEST_FAILED, "Getch returns [%c] but is must be [%c]", res, some_val);
        for (int i = 0; i < 3; i++) // cehck several times
            if ( (res = getch() ) != EOF)
                return logerr(TEST_FAILED, "Getch returns [%c - %d] but is must be EOF", res, res);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}

// ------------------------- TEST 2 ---------------------------------

static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;
    {
        test_sub("subtest %d", ++subnum);

        buffer_clear();
        const char str[] = "bla simple to ungetch, bla bbla bla";
        int res, len = strlen(str);
        if ( (res = ungets(str)) != len)
            return logerr(TEST_FAILED, "Ungets() returns %d but is must be %d", res, len);

        test_sub("subtest %d", ++subnum);

        char c;
        while (len > 0 && (c = getch()) != EOF)
            if (str[--len] != c)
                return logerr(TEST_FAILED, "Ungets, Pos %d is [%c] but is must be [%c]", len, c, str[len]);

        test_sub("subtest %d", ++subnum);

        len = strlen(str);
        if ( (res = ungetrevs(str, len)) != len)
            return logerr(TEST_FAILED, "Ungetrevs() returns %d but is must be %d", res, len);

        test_sub("subtest %d", ++subnum);
        int i = 0;
        while (i < len && (c = getch()) != EOF)
            if (str[i++] != c)
                return logerr(TEST_FAILED, "Ungetrevs, Pos %d is [%c] but is must be [%c]", i, c, str[i]);
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED
}
// -------------------------------------------------------------------

int
main(int argc, char *argv[])
{
    const char *logfilename = "log/buffer.log";

    if (argc > 1)
        logfilename = argv[1];

    loginit(logfilename, false, 0, "Starting");

    testenginestd(
        testnew(.f2 = tf1, .num = 1, .name = "Simple and mult ungetch/getch test"   , .desc = "", .mandatory=true)
      , testnew(.f2 = tf2, .num = 2, .name = "Simple ungets/ungetrevs test"         , .desc = "", .mandatory=true)
    );

    logclose("end...");
    return 0;
}


#endif /* BUFFERTESTING */

