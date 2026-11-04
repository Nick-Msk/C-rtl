 #include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>
#include <limits.h>

TestSuite(fs2_api_serialization, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_serialize — meta: replaces content, returns length
 * ========================================================================= */

Test(fs2_api_serialization, serialize_replaces_content_and_returns_length) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("old content");

    long n = fs2_serialize(&s, 42);          /* int -> "%d" */

    CHECK_EQ  ("return length",  n,  2L);
    CHECK_STR_EQ("replaced",     s.v, "42");
    CHECK_EQ  ("len",            s.len, 2);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize — char family
 *   (char)        -> "%c"   -> the character itself
 *   (signed char) -> "%hhd" -> signed number
 *   (unsigned char)->"%hhu" -> unsigned number
 * ========================================================================= */

Test(fs2_api_serialization, serialize_char_prints_character) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, (char)'A');            /* explicit cast: type is char */
    CHECK_STR_EQ("A", s.v, "A");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_serialization, serialize_signed_char_prints_number) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, (signed char)-42);
    CHECK_STR_EQ("-42", s.v, "-42");

    fs2_serialize(&s, (signed char)-128);
    CHECK_STR_EQ("-128", s.v, "-128");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_serialization, serialize_unsigned_char_prints_number) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, (unsigned char)200);
    CHECK_STR_EQ("200", s.v, "200");

    fs2_serialize(&s, (unsigned char)255);
    CHECK_STR_EQ("255", s.v, "255");

    fs2_free(&s);
    assert_balanced();
}

/* GOTCHA: in C, character constants like 'A' have type int (not char).
 * So _Generic picks int -> "%d" -> "65", NOT "%c" -> "A".
 * To get "A", caller must cast: (char)'A'. */
Test(fs2_api_serialization, serialize_char_constant_is_int_gotcha) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, 'A');                  /* 'A' is int -> "%d" -> "65" */
    CHECK_STR_EQ("65", s.v, "65");

    fs2_serialize(&s, 'z');                  /* 'z' is int -> "122" */
    CHECK_STR_EQ("122", s.v, "122");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize — signed integer chain
 * ========================================================================= */

Test(fs2_api_serialization, serialize_signed_chain) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, (short)-12345);        /* "%hd" */
    CHECK_STR_EQ("-12345", s.v, "-12345");

    fs2_serialize(&s, (int)-123456789);      /* "%d" */
    CHECK_STR_EQ("-123456789", s.v, "-123456789");

    fs2_serialize(&s, (long)-1L);            /* "%ld" */
    CHECK_STR_EQ("-1", s.v, "-1");

    fs2_serialize(&s, (long long)-1LL);      /* "%lld" */
    CHECK_STR_EQ("-1", s.v, "-1");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_serialization, serialize_signed_minmax) {
    __fs2_checkreset();
    fs2 s = FS2();
    char buf[64];

    fs2_serialize(&s, (int)INT_MAX);
    snprintf(buf, sizeof buf, "%d", INT_MAX);
    CHECK_STR_EQ("INT_MAX", s.v, buf);

    fs2_serialize(&s, (int)INT_MIN);
    snprintf(buf, sizeof buf, "%d", INT_MIN);
    CHECK_STR_EQ("INT_MIN", s.v, buf);

    fs2_serialize(&s, (long long)LLONG_MAX);
    snprintf(buf, sizeof buf, "%lld", LLONG_MAX);
    CHECK_STR_EQ("LLONG_MAX", s.v, buf);

    fs2_serialize(&s, (long long)LLONG_MIN);
    snprintf(buf, sizeof buf, "%lld", LLONG_MIN);
    CHECK_STR_EQ("LLONG_MIN", s.v, buf);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize — unsigned chain
 * ========================================================================= */

Test(fs2_api_serialization, serialize_unsigned_chain) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, (unsigned short)65535);        /* "%hu" */
    CHECK_STR_EQ("65535", s.v, "65535");

    fs2_serialize(&s, (unsigned)UINT_MAX);           /* "%u" */
    CHECK_EQ("UINT_MAX ok",
             (unsigned long long)strtoull(s.v, NULL, 10),
             (unsigned long long)UINT_MAX);

    fs2_serialize(&s, (unsigned long)ULONG_MAX);     /* "%lu" */
    CHECK_EQ("ULONG_MAX len > 0", s.len > 0, true);

    fs2_serialize(&s, (unsigned long long)ULLONG_MAX); /* "%llu" */
    CHECK_STR_EQ("ULLONG_MAX", s.v, "18446744073709551615");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize — floating point
 *   float -> "%f" ; double -> "%lf" (both behave as %f in printf)
 * ========================================================================= */

Test(fs2_api_serialization, serialize_float) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, 1.5f);                 /* float -> "%f" -> 6 decimals */
    CHECK_STR_EQ("1.500000", s.v, "1.500000");

    fs2_serialize(&s, 0.0f);
    CHECK_STR_EQ("0.000000", s.v, "0.000000");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_serialization, serialize_double) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, 2.25);                 /* double -> "%lf" -> 6 decimals */
    CHECK_STR_EQ("2.250000", s.v, "2.250000");

    fs2_serialize(&s, -0.5);
    CHECK_STR_EQ("-0.500000", s.v, "-0.500000");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize — pointers
 *   void* / const void* -> "%p"
 *   %p output is implementation-defined (0x..., (nil), ...),
 *   so we only assert: non-empty and stable for the same pointer.
 * ========================================================================= */

Test(fs2_api_serialization, serialize_void_pointer) {
    __fs2_checkreset();
    int x = 42;
    void *p = &x;
    fs2 s = FS2();

    fs2_serialize(&s, p);
    cr_assert_gt(s.len, 0, "pointer output must be non-empty");
    char *first = strdup(s.v);
    cr_assert_not_null(first);

    /* Same pointer, serialized again -> same string */
    fs2_serialize(&s, p);
    CHECK_STR_EQ("stable", s.v, first);

    free(first);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_serialization, serialize_const_void_pointer) {
    __fs2_checkreset();
    int x = 42;
    const void *p = &x;
    fs2 s = FS2();

    fs2_serialize(&s, p);
    cr_assert_gt(s.len, 0, "pointer output must be non-empty");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize — chaining / multiple calls
 * ========================================================================= */

Test(fs2_api_serialization, serialize_overwrites_each_time) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, 42);
    CHECK_STR_EQ("42", s.v, "42");

    fs2_serialize(&s, (short)-7);
    CHECK_STR_EQ("-7", s.v, "-7");           /* not "42-7" — replaced */

    fs2_serialize(&s, 3.14);
    CHECK_STR_EQ("3.140000", s.v, "3.140000");

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_from_new — one test per _Generic branch
 * ========================================================================= */

Test(fs2_api_serialization, from_new_signed_family) {
    __fs2_checkreset();

    fs2 a = fs2_from_new((signed char)-42);
    CHECK_STR_EQ("-42",  a.v, "-42");
    CHECK_FS2VALID(1, &a);
    fs2_free(&a);

    fs2 b = fs2_from_new((short)-12345);
    CHECK_STR_EQ("-12345", b.v, "-12345");
    fs2_free(&b);

    fs2 c = fs2_from_new((int)-123456789);
    CHECK_STR_EQ("-123456789", c.v, "-123456789");
    fs2_free(&c);

    fs2 d = fs2_from_new((long)-1L);
    CHECK_STR_EQ("-1", d.v, "-1");
    fs2_free(&d);

    fs2 e = fs2_from_new((long long)LLONG_MIN);
    char expect[64]; snprintf(expect, sizeof expect, "%lld", LLONG_MIN);
    CHECK_STR_EQ("LLONG_MIN", e.v, expect);
    fs2_free(&e);

    assert_balanced();
}

Test(fs2_api_serialization, from_new_unsigned_family) {
    __fs2_checkreset();

    fs2 a = fs2_from_new((unsigned char)200);
    CHECK_STR_EQ("200", a.v, "200");
    fs2_free(&a);

    fs2 b = fs2_from_new((unsigned short)65535);
    CHECK_STR_EQ("65535", b.v, "65535");
    fs2_free(&b);

    fs2 c = fs2_from_new((unsigned)UINT_MAX);
    char expect[64]; snprintf(expect, sizeof expect, "%u", UINT_MAX);
    CHECK_STR_EQ("UINT_MAX", c.v, expect);
    fs2_free(&c);

    fs2 d = fs2_from_new((unsigned long)ULONG_MAX);
    CHECK_EQ("ULONG_MAX len > 0", d.len > 0, true);
    fs2_free(&d);

    fs2 e = fs2_from_new((unsigned long long)ULLONG_MAX);
    CHECK_STR_EQ("ULLONG_MAX", e.v, "18446744073709551615");
    fs2_free(&e);

    assert_balanced();
}

Test(fs2_api_serialization, from_new_floats) {
    __fs2_checkreset();

    fs2 a = fs2_from_new((float)1.5f);
    CHECK_STR_EQ("1.500000", a.v, "1.500000");
    fs2_free(&a);

    fs2 b = fs2_from_new((double)-0.5);
    CHECK_STR_EQ("-0.500000", b.v, "-0.500000");
    fs2_free(&b);

    assert_balanced();
}

Test(fs2_api_serialization, from_new_char_explicit_cast) {
    __fs2_checkreset();
    fs2 r = fs2_from_new((char)'A');         /* explicit cast: type is char -> "%c" -> "A" */
    CHECK_STR_EQ("A", r.v, "A");
    fs2_free(&r);
    assert_balanced();
}

/* GOTCHA: 'A' in C has type int, not char; _Generic picks int -> "%d" -> "65". */
Test(fs2_api_serialization, from_new_char_constant_is_int_gotcha) {
    __fs2_checkreset();

    fs2 a = fs2_from_new('A');               /* int -> "65" */
    CHECK_STR_EQ("65", a.v, "65");
    fs2_free(&a);

    fs2 b = fs2_from_new('z');               /* int -> "122" */
    CHECK_STR_EQ("122", b.v, "122");
    fs2_free(&b);

    assert_balanced();
}

Test(fs2_api_serialization, from_new_pointers) {
    __fs2_checkreset();
    int x = 42;

    fs2 a = fs2_from_new((void *)&x);
    cr_assert_gt(a.len, 0, "pointer output must be non-empty");
    char *first = strdup(a.v);
    cr_assert_not_null(first);
    fs2_free(&a);

    fs2 b = fs2_from_new((const void *)&x);
    cr_assert_gt(b.len, 0, "pointer output must be non-empty");
    /* Same address -> same serialization (assuming same format) */
    CHECK_STR_EQ("same address, same text", b.v, first);
    fs2_free(&b);

    free(first);
    assert_balanced();
}

/* =========================================================================
 * fs2_from_new — meta
 * ========================================================================= */

Test(fs2_api_serialization, from_new_returns_alloc_by_value) {
    __fs2_checkreset();
    fs2 r = fs2_from_new(42);

    CHECK_EQ  ("len",  r.len, 2);
    CHECK_TRUE("ALLOC",           fs2_is_alloc(&r));
    CHECK_FALSE("not BODYALLOC",  fs2_is_bodyalloc(&r));
    CHECK_FALSE("not STATIC",     fs2_is_static(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

/* Each call allocates its own buffer — results are independent. */
Test(fs2_api_serialization, from_new_calls_are_independent) {
    __fs2_checkreset();

    fs2 a = fs2_from_new(1);
    fs2 b = fs2_from_new(2);

    CHECK_TRUE("independent buffers", a.v != b.v);
    CHECK_STR_EQ("a", a.v, "1");
    CHECK_STR_EQ("b", b.v, "2");

    /* Mutating one must not affect the other */
    a.v[0] = 'X';
    CHECK_STR_EQ("a mutated",       a.v, "X");
    CHECK_STR_EQ("b still original", b.v, "2");

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_from — same as from_new, but BODYALLOC | ALLOC
 * ========================================================================= */

Test(fs2_api_serialization, heap_from_basic) {
    __fs2_checkreset();
    fs2 *r = fs2_heap_from(42);

    cr_assert_not_null(r);
    CHECK_STR_EQ("42", r->v, "42");
    CHECK_EQ  ("len",  r->len, 2);
    CHECK_TRUE("ALLOC",      fs2_is_alloc(r));
    CHECK_TRUE("BODYALLOC",  fs2_is_bodyalloc(r));
    CHECK_FS2VALID(1, r);

    fs2_free(r);
    assert_balanced();
}

Test(fs2_api_serialization, heap_from_negative_and_double) {
    __fs2_checkreset();

    fs2 *a = fs2_heap_from((int)-42);
    CHECK_STR_EQ("-42", a->v, "-42");
    fs2_free(a);

    fs2 *b = fs2_heap_from((double)3.14);
    CHECK_STR_EQ("3.140000", b->v, "3.140000");
    fs2_free(b);

    assert_balanced();
}

Test(fs2_api_serialization, heap_from_calls_are_independent) {
    __fs2_checkreset();

    fs2 *a = fs2_heap_from(1);
    fs2 *b = fs2_heap_from(2);

    cr_assert(a != b);
    CHECK_TRUE("independent buffers", a->v != b->v);

    fs2_free(a);
    fs2_free(b);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize_concat — appends, does not replace
 * ========================================================================= */

Test(fs2_api_serialization, serialize_concat_appends) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    long n = fs2_serialize_concat(&s, 42);
    CHECK_EQ  ("return",     n,  2L);
    CHECK_STR_EQ("abc42",    s.v, "abc42");
    CHECK_EQ  ("len",        s.len, 5);

    fs2_serialize_concat(&s, (short)-7);
    CHECK_STR_EQ("abc42-7",  s.v, "abc42-7");

    fs2_serialize_concat(&s, (double)1.5);
    CHECK_STR_EQ("abc42-71.500000", s.v, "abc42-71.500000");

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* Appending to empty — the "initial" case */
Test(fs2_api_serialization, serialize_concat_on_empty) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize_concat(&s, 42);
    CHECK_STR_EQ("42", s.v, "42");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * FS2_SER_ERRMSG — standalone: renders exactly the format the macro picks
 *
 * The raise path in fs2_from_new is practically unreachable (fs2_sprintf
 * on a fresh FS2() with a fixed tiny format cannot return <= 0). But the
 * _Generic dispatch behind FS2_SER_ERRMSG can be verified independently
 * by rendering it through snprintf.
 * ========================================================================= */

Test(fs2_api_serialization, ser_errmsg_int) {
    char buf[128];
    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((int)42), (int)42);
    CHECK_STR_EQ("Failed to serialize int 42", buf, "Failed to serialize int 42");
}

Test(fs2_api_serialization, ser_errmsg_negative_and_minmax) {
    char buf[128];

    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((int)-42), (int)-42);
    CHECK_STR_EQ("neg", buf, "Failed to serialize int -42");

    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((long long)LLONG_MIN), (long long)LLONG_MIN);
    CHECK_EQ("llmin prefix", strncmp(buf, "Failed to serialize long long ", 30), 0);
}

Test(fs2_api_serialization, ser_errmsg_char) {
    char buf[128];
    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((char)'A'), (char)'A');
    CHECK_STR_EQ("char A", buf, "Failed to serialize char A");
}

Test(fs2_api_serialization, ser_errmsg_char_constant_gotcha) {
    char buf[128];
    /* 'A' is int -> picks "%d" -> "65", not "%c" -> "A" */
    snprintf(buf, sizeof buf, FS2_SER_ERRMSG('A'), 'A');
    CHECK_STR_EQ("int 65", buf, "Failed to serialize int 65");
}

Test(fs2_api_serialization, ser_errmsg_unsigned) {
    char buf[128];
    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((unsigned)UINT_MAX), (unsigned)UINT_MAX);
    char expect[128]; snprintf(expect, sizeof expect, "Failed to serialize unsigned %u", UINT_MAX);
    CHECK_STR_EQ("unsigned max", buf, expect);

    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((unsigned long long)ULLONG_MAX),
             (unsigned long long)ULLONG_MAX);
    CHECK_STR_EQ("ullong max", buf, "Failed to serialize unsigned long long 18446744073709551615");
}

Test(fs2_api_serialization, ser_errmsg_floats) {
    char buf[128];

    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((float)1.5f), (float)1.5f);
    CHECK_STR_EQ("float", buf, "Failed to serialize float 1.500000");

    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((double)-0.5), (double)-0.5);
    CHECK_STR_EQ("double", buf, "Failed to serialize double -0.500000");
}

Test(fs2_api_serialization, ser_errmsg_pointer_prefix) {
    /* %p output is implementation-defined; only the prefix is portable. */
    int x = 42;
    char buf[128];
    snprintf(buf, sizeof buf, FS2_SER_ERRMSG((void *)&x), (void *)&x);
    CHECK_EQ("pointer prefix",
                 strncmp(buf, "Failed to serialize pointer ", 28), 0);
    cr_assert_gt(strlen(buf), 28, "pointer text after prefix must be non-empty");
}

/* =========================================================================
 * fs2_nonspace_span — independent expected values (not compared to cspn)
 * ========================================================================= */

Test(fs2_api_serialization, nonspace_span_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");         /* ' ' at 5 */

    CHECK_EQ("from 0", fs2_nonspace_span(&s, 0), 5);   /* "hello" */
    CHECK_EQ("from 5", fs2_nonspace_span(&s, 5), 0);   /* starts with ' ' */
    CHECK_EQ("from 6", fs2_nonspace_span(&s, 6), 5);   /* "world" */
    CHECK_EQ("from 11", fs2_nonspace_span(&s, 11), 0); /* pos == len */

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_serialization, nonspace_span_stops_at_any_whitespace) {
    __fs2_checkreset();
    /* Exercise every member of " \t\n\r\v\f" */
    struct { const char *src; size_t expected; } cases[] = {
        { "abc def",   3 },   /* space */
        { "abc\tdef",  3 },   /* tab */
        { "abc\ndef",  3 },   /* LF */
        { "abc\rdef",  3 },   /* CR */
        { "abc\vdef",  3 },   /* VT */
        { "abc\fdef",  3 },   /* FF */
        { "abcdef",    6 },   /* no whitespace — whole string */
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        fs2 s = fs2_fromstr(cases[i].src);
        CHECK_EQ("run until whitespace", fs2_nonspace_span(&s, 0), cases[i].expected);
        fs2_free(&s);
    }
    assert_balanced();
}

Test(fs2_api_serialization, nonspace_span_only_whitespace) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   \t\n\r\v\f   ");
    CHECK_EQ("immediate", fs2_nonspace_span(&s, 0), 0);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_nonspace_span — empty cases (FS2EMPTY / FS2() / pos >= len)
 * ========================================================================= */

Test(fs2_api_serialization, nonspace_span_fs2empty_v_not_null_returns_zero) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    cr_assert_not_null(s.v, "precondition: FS2EMPTY has v == \"\"");

    CHECK_EQ("pos=0", fs2_nonspace_span(&s, 0), 0);
    CHECK_EQ("pos=5", fs2_nonspace_span(&s, 5), 0);

    assert_balanced();
}

Test(fs2_api_serialization, nonspace_span_canon_v_null_returns_zero) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_null(s.v, "precondition: FS2() has v == NULL");

    CHECK_EQ("pos=0",        fs2_nonspace_span(&s, 0),        0);
    CHECK_EQ("pos=SIZE_MAX", fs2_nonspace_span(&s, SIZE_MAX), 0);

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_serialization, nonspace_span_pos_at_or_beyond_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("pos=len",   fs2_nonspace_span(&s, 5),   0);
    CHECK_EQ("pos>len",   fs2_nonspace_span(&s, 100), 0);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_nonspace_span — guards (inherited via fs2_cspn)
 * ========================================================================= */

Test(fs2_api_serialization, nonspace_span_null_s_aborts, .signal = SIGABRT) {
    fs2_nonspace_span(NULL, 0);
}

/* =========================================================================
 * fs2_nonspace_span — STATIC src
 * ========================================================================= */

Test(fs2_api_serialization, nonspace_span_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("hello world");

    CHECK_EQ("from 0", fs2_nonspace_span(&s, 0), 5);
    CHECK_EQ("from 6", fs2_nonspace_span(&s, 6), 5);

    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

/* =========================================================================
 * Consistency: nonspace_span == cspn(..., " \t\n\r\v\f")
 *   (kept as a guard against divergence if either changes)
 * ========================================================================= */

Test(fs2_api_serialization, nonspace_span_matches_cspn) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello\tworld\nfoo");

    for (size_t p = 0; p <= s.len; p++) {
        CHECK_EQ("matches cspn",
                 fs2_nonspace_span(&s, p),
                 fs2_cspn(&s, p, " \t\n\r\v\f"));
    }

    fs2_free(&s);
    assert_balanced();
}
