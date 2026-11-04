 #include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>
#include <limits.h>

TestSuite(fs2_api_serialization, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_serialize — meta: replaces content, returns length
 * ========================================================================= */

Test(fs2_api_ser, serialize_replaces_content_and_returns_length) {
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

Test(fs2_api_ser, serialize_char_prints_character) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, (char)'A');            /* explicit cast: type is char */
    CHECK_STR_EQ("A", s.v, "A");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_ser, serialize_signed_char_prints_number) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, (signed char)-42);
    CHECK_STR_EQ("-42", s.v, "-42");

    fs2_serialize(&s, (signed char)-128);
    CHECK_STR_EQ("-128", s.v, "-128");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_ser, serialize_unsigned_char_prints_number) {
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
Test(fs2_api_ser, serialize_char_constant_is_int_gotcha) {
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

Test(fs2_api_ser, serialize_signed_chain) {
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

Test(fs2_api_ser, serialize_signed_minmax) {
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

Test(fs2_api_ser, serialize_unsigned_chain) {
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

Test(fs2_api_ser, serialize_float) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize(&s, 1.5f);                 /* float -> "%f" -> 6 decimals */
    CHECK_STR_EQ("1.500000", s.v, "1.500000");

    fs2_serialize(&s, 0.0f);
    CHECK_STR_EQ("0.000000", s.v, "0.000000");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_ser, serialize_double) {
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

Test(fs2_api_ser, serialize_void_pointer) {
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

Test(fs2_api_ser, serialize_const_void_pointer) {
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

Test(fs2_api_ser, serialize_overwrites_each_time) {
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
