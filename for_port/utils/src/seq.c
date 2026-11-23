/********************************************************************
                    SEQUENCE MODULE IMPLEMENTATION
********************************************************************/

#include "seq.h"

enum { SEQ_MAXCOUNT = 128, G_TECHPTINTPERLINE = 10 };        // for now

// == 0 means dropped or not init
static seqv_t        sequences[SEQ_MAXCOUNT] = {0};
static seqnum_t      sequencesptr = 0;

// ------------------------------------ Utilities ------------------------------------------


int                 findseq(void) {
    for (seqnum_t i = 0; i < sequencesptr; i++)
        if (!sequences[i])   // found dropped
            return logsimpleret(i, "Found %d", i);
    return logsimpleret(-1, "Unable to find empty seq");
}

// ------------------------------------- API -----------------------------------------------

seqnum_t            initseq(void) {
    seqnum_t     num;
    if (sequencesptr >= SEQ_MAXCOUNT) {
        if ( (num = findseq() ) < 0)
            userraiseint(ERR_UNABLE_ALLOCATE_SEQ, "Unable to allocate sequence");   // probably need to be configurable in context
    } else
        num = sequencesptr++;
    sequences[num] = 1L;    // init
    return num;
}

void                dropseq(seqnum_t s) {
    if (! (s >= 0 && s < SEQ_MAXCOUNT) )
        userraiseint(ERR_OUT_OF_RANGE, "%d must be positive and < %d", s, SEQ_MAXCOUNT);
    sequences[s] = 0L;    // un init
}

seqv_t              currval(seqnum_t s) {
    if (! (s >= 0 && s < SEQ_MAXCOUNT) )
        userraiseint(ERR_OUT_OF_RANGE, "%d must be positive and < %d", s, SEQ_MAXCOUNT);
    return sequences[s];
}

seqv_t              nextval(seqnum_t s) {
    if (! (s >= 0 && s < SEQ_MAXCOUNT) )
        userraiseint(ERR_OUT_OF_RANGE, "%d must be positive and < %d", s, SEQ_MAXCOUNT);
    return ++sequences[s];
}

void                resetseq(void) {
    for (int i = 0; i < SEQ_MAXCOUNT; i++)
        sequences[i] = 0L;
    sequencesptr = 0;
}
// ------------------------ Printers ---------------------------------------
int                 seq_techfprint(FILE *out) {
    int     cnt = 0;
    if (out) {
        int total = 0;
        fprintf(out, "SEQ_T: %d/%d [\n", sequencesptr, SEQ_MAXCOUNT);
        for (int i = 0; i < sequencesptr; i++) {
            if (sequences[i])
                fprintf(out, "%3d:%6lld\t", i, sequences[i]), total++;
        }
        fprintf(out, "]\nTotal allocated: %d\n", total);
    }
    return cnt;
}

// ---------------------------------------- Testing ------------------------------------------
#ifdef SEQTESTING

#include "test.h"

// ------------------------- TEST sequence API ---------------------------------
static TestStatus
tf_sequence(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: init single sequence", ++subnum);
    {
        resetseq();
        seqnum_t s = initseq();
        test_validate(s == 0, "First initseq should return 0, got %d", s);
        dropseq(s);
    }

    test_sub("subtest %d: currval after init is 1", ++subnum);
    {
        resetseq();
        seqnum_t s = initseq();
        seqv_t v = currval(s);
        test_validate(v == 1L, "currval after init should be 1, got %ld", (long)v);
        dropseq(s);
    }

    test_sub("subtest %d: nextval increments", ++subnum);
    {
        resetseq();
        seqnum_t s = initseq();
        seqv_t v1 = nextval(s);
        seqv_t v2 = nextval(s);
        seqv_t cur = currval(s);
        test_validate(v1 == 2L && v2 == 3L && cur == 3L,
                      "nextval: v1=%ld, v2=%ld, cur=%ld (expected 2,3,3)",
                      (long)v1, (long)v2, (long)cur);
        seq_techfprint(stdout);
        dropseq(s);
    }

    test_sub("subtest %d: no reuse before exhaustion", ++subnum);
    {
        resetseq();
        seqnum_t a = initseq();                     // 0
        seqnum_t b = initseq();                     // 1
        dropseq(a);                                 // освобождаем 0

        seqnum_t c = initseq();                     // должен получить новый индекс 2, а не 0
        test_validate(c == 2,
                      "Before full, initseq should return new index (2), got %d", c);

        // Проверим, что слот 0 всё ещё свободен и не тронут
        test_validate(sequences[0] == 0L,
                      "Slot 0 must remain dropped (0), got %ld", (long)sequences[0]);

        dropseq(b);
        dropseq(c);
    }

    test_sub("subtest %d: reuse after full", ++subnum);
    {
        resetseq();
        // Заполняем все SEQ_MAXCOUNT слотов
        seqnum_t used[SEQ_MAXCOUNT] = {0};
        for (int i = 0; i < SEQ_MAXCOUNT; i++) {
            used[i] = initseq();
            test_validate(used[i] >= 0, "initseq failed at slot %d", i);
        }

        // Освобождаем слот в середине
        dropseq(used[SEQ_MAXCOUNT/2]);

        // Теперь следующий initseq должен переиспользовать освобождённый слот
        seqnum_t s = initseq();
        test_validate(s == used[SEQ_MAXCOUNT/2],
                      "After full, initseq should reuse dropped index %d, got %d",
                      used[SEQ_MAXCOUNT/2], s);

        // Очистка
        for (int i = 0; i < SEQ_MAXCOUNT; i++) {
            if (i != SEQ_MAXCOUNT/2)
                dropseq(used[i]);
        }
        dropseq(s);
    }

    test_sub("subtest %d: init more than SEQ_MAXCOUNT raises", ++subnum);
    {
        resetseq();
        // Заполняем все слоты
        seqnum_t used[SEQ_MAXCOUNT] = {0};
        int cnt = 0;
        for (int i = 0; i < SEQ_MAXCOUNT; i++) {
            seqnum_t s = initseq();
            if (s < 0) break;
            used[cnt++] = s;
        }
        // Теперь попытка создать ещё один должна вызвать ошибку
        if (!try()) {
            seqnum_t extra = initseq();
            test_validate(false, "Should have raised SIGINT when out of slots");
            if (extra >= 0) dropseq(extra);
        } else {
            logsimple("Exception correctly raised on sequence exhaustion");
        }
        // Освобождаем занятые слоты
        for (int i = 0; i < cnt; i++) {
            dropseq(used[i]);
        }
    }

    test_sub("subtest %d: drop invalid index raises", ++subnum);
    {
        resetseq();
        if (!try()) {
            dropseq(-1);
            test_validate(false, "Should have raised on negative index");
        } else {
            logsimple("Exception on dropseq(-1)");
        }
        if (!try()) {
            dropseq(SEQ_MAXCOUNT);
            test_validate(false, "Should have raised on index == SEQ_MAXCOUNT");
        } else {
            logsimple("Exception on dropseq(SEQ_MAXCOUNT)");
        }
    }

    test_sub("subtest %d: currval/nextval invalid index raises", ++subnum);
    {
        resetseq();
        if (!try()) {
            currval(-5);
            test_validate(false, "currval(-5) should have raised");
        } else {
            logsimple("Exception on currval(-5)");
        }
        if (!try()) {
            nextval(SEQ_MAXCOUNT + 10);
            test_validate(false, "nextval(out of range) should have raised");
        } else {
            logsimple("Exception on nextval(out of range)");
        }
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------------------------------------------------------------------------------------------------------------
int
main(int argc, const char *argv[])
{
    logsimpleinit("Start");
    bool    runall = argc == 1;

    while (runall || *++argv){
        int     num = INT_MAX;    // INT_MAX for all test
        if (!runall){
            num = atoi(*argv);
            if (num < 0){
                fprintf(stderr,"Invalid test num %d\n", num);
                continue;
            }
        }
        testenginestd_run(num,
            testnew(.f2 = tf_sequence,                                 .num = 1, .name = "tf_value64_tarray"
                , .desc="", .mandatory=true)
        );
        if (runall)
            break;
    }

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* SEQTESTING */


