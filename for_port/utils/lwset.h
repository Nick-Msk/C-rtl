#ifndef _LWSET_H
#define _LWSET_H

// ---------------------------------------------------------------------------------
// -------------------------- Public lightweight API -------------------------------
// ---------------------------------------------------------------------------------

// ----------------------------- Includes ------------------------------------------

#include <inttypes.h>
#include <stdio.h>

#include "checker.h"
#include "error.h"
#include "log.h"
#include "common.h"
#include "fs.h"
#include "fs_iter.h"

// --------------------------------- CONSTANTS AND GLOBALS --------------------------

// ---------------------------------- TYPES -----------------------------------------

/// @brief Lightweight set structure using a single 64-bit integer to represent the set of bits
/// @details The lwset structure uses a single 64-bit integer to represent a set of bits, where each bit corresponds to an element in the set. The low and high fields define the range of valid indices for the set.
/// @note The lwset structure is designed for efficiency and simplicity, making it suitable for scenarios where a small, fixed-size set is needed. It is not intended for large or dynamic sets.
typedef struct {
    uint64_t value;  // now only 1 int64_t value is used, but in the future, we can add more fields if needed
    unsigned short low, high;  // range of valid indices for the set, non-iclusive, low < high, high <= 64
} lwset;

/// @brief msx bits in lwset
static const unsigned short     LWSET_MAX_BITS = sizeof(uint64_t) * 8;  // 64 bits

// ------------------------- CONSTRUCTOTS/DESTRUCTORS -------------------------------

/// @brief Initializes a lwset with all bits set to 0 and the range [0, 63]
static inline lwset             lwset_initunlim(void) {
    return (lwset) {.value = 0, .low = 0, .high = LWSET_MAX_BITS};  // set high to the maximum bit index for int64_t
}

/// @brief Initializes a lwset with all bits set to 1 and the range [0, 63]
static inline lwset             lwset_init1unlim(void) {
    return (lwset) {.value = UINT64_MAX, .low = 0, .high = LWSET_MAX_BITS};  // set high to the maximum bit index for int64_t
}

/// @brief  initializes a lwset with all bits set to 0 and the specified range [low, high]
/// @param low the starting index of the range (inclusive)
/// @param high the ending index of the range (inclusive)
/// @return the initialized lwset
static inline lwset             lwset_init0(unsigned short low, unsigned short high) {
    if (low >= high || high > LWSET_MAX_BITS)
        userraiseint(ERR_OUT_OF_RANGE, "Invalid lwset range: low=%u, high=%u", low, high);
    return (lwset) {.value = 0, .low = low, .high = high};  // high is inclusive, so we subtract 1 to make it non-inclusive
}

/// @brief  initializes a lwset with all bits set to 1 and the specified range [low, high]
/// @param low the starting index of the range (inclusive)
/// @param high the ending index of the range (inclusive)
/// @return the initialized lwset
static inline lwset lwset_init1(unsigned short low, unsigned short high) {
    if (low >= high || high > LWSET_MAX_BITS)
        userraiseint(ERR_OUT_OF_RANGE, "Invalid lwset range: low=%u, high=%u", low, high);
    unsigned short width = high - low;
    uint64_t mask = (width >= 64) ? UINT64_MAX : ((1ULL << width) - 1);
    return (lwset) { .value = mask << low, .low = low, .high = high };
}

/// @brief  Clones a lwset, creating a new lwset with the same value and range as the original
/// @param s  pointer to the lwset to clone
/// @return  the cloned lwset
static inline lwset             lwset_clone(const lwset *s) {
    invraisecode(ERR_NULLABLE_PTR, s != NULL, "input pointer is NULL");
    return (lwset) {.value = s->value, .low = s->low, .high = s->high};
}

/// @brief  Create from ushort array of values, and count of values
/// @param values pointer to the array of unsigned short values
/// @param count the number of values in the array
/// @return  the initialized lwset with bits set according to the values in the array
static inline lwset             lwset_list(const unsigned short *values, size_t count) {
    lwset s = lwset_initunlim();
    unsigned short max = 0;
    for (size_t i = 0; i < count; ++i) {
            invraisecode(ERR_OUT_OF_RANGE, values[i] >= 0 && values[i] < s.high
                , "value %hd is out of bounds for lwset", values[i]);
        if (values[i] > max)
            max = values[i];
        s.value |= (1UL << values[i]);
    }
    s.high = max + 1;  // set high to the maximum value + 1 to make it non-inclusive
    return s;
}

// simplifier macro-constructor for lwset_list
#define LWSET_LIST(...) lwset_list( (unsigned short[]){__VA_ARGS__}, \
    COUNT( ( (unsigned short[]){__VA_ARGS__} ) ) )


// -------------------- ACCESS AND MODIFICATORS -------------------------------------
/// @brief  Gets the value of a specific bit in the lwset
/// @param s pointer to the lwset
/// @param index the index of the bit to get
/// @return the value of the bit at the specified index (true or false)
static inline bool           lwset_get(const lwset *s, unsigned short index) {
    invraisecode(ERR_NULLABLE_PTR, s != NULL, "input pointer is NULL");
    invraisecode(ERR_OUT_OF_RANGE, index >= s->low && index < s->high, 
        "index %u is out of bounds for lwset", index);
    return (s->value >> index) & 1;
}

/// @brief  Checks if two lwsets are equal, comparing their values and ranges
/// @param s1 pointer to the first lwset
/// @param s2  pointer to the second lwset
/// @return  true if the lwsets are equal, false otherwise
static inline bool           lwset_equals(const lwset *restrict s1, const lwset *restrict s2) {
    invraisecode(ERR_NULLABLE_PTR, s1 != NULL && s2 != NULL, 
        "Pointers is NULL %p %p", (void*) s1, (void*) s2);
    // compare even if the ranges are NOT  the same
    return s1->value == s2->value; // && s1->low == s2->low && s1->high == s2->high;
}

/// @brief  Checks if two lwsets are not equal, comparing their values and ranges
/// @param s1  pointer to the first lwset
/// @param s2  pointer to the second lwset
/// @return  true if the lwsets are not equal, false otherwise
static inline bool           lwset_notequal(const lwset *restrict s1, const lwset *restrict s2) {
    invraisecode(ERR_NULLABLE_PTR, s1 != NULL && s2 != NULL, 
        "Pointers is NULL %p %p", (void*) s1, (void*) s2);
    return (s1->value != s2->value);  // without checking low/high, because we want to compare the actual values
}

/// @brief  Checks if lwset s1 is a subset of lwset s2, comparing their values
/// @param s1  pointer to the first lwset
/// @param s2  pointer to the second lwset
/// @return  true if s1 is a subset of s2, false otherwise
static inline bool            lwset_in(const lwset *restrict s1, const lwset *restrict s2) {
    invraisecode(ERR_NULLABLE_PTR, s1 != NULL && s2 != NULL, 
        "Pointers is NULL %p %p", (void*) s1, (void*) s2);
    return (s1->value & s2->value) == s1->value;
}

/// @brief  Checks if lwset s1 is a strict subset of lwset s2, comparing their values
/// @param s1  pointer to the first lwset
/// @param s2  pointer to the second lwset
/// @return  true if s1 is a strict subset of s2, false otherwise
static inline bool           lwset_strictin(const lwset *restrict s1, const lwset *restrict s2) {
    return lwset_in(s1, s2) && !lwset_equals(s1, s2);
}
/// @brief  Checks if lwset s1 is not empty
/// @param s  pointer to the lwset
/// @return  true if s1 is not empty, false otherwise
static inline bool           lwset_notempty(const lwset *s) {
    invraisecode(ERR_NULLABLE_PTR, s != NULL, "Pointer is NULL");
    return s->value != 0;
}

/// @brief  Checks if lwset s1 is empty
/// @param s  pointer to the lwset
/// @return  true if s1 is empty, false otherwise
static inline bool           lwset_isempty(const lwset *s) {
    invraisecode(ERR_NULLABLE_PTR, s != NULL, "Pointer is NULL");
    return s->value == 0;
}

/// @brief  Counts the number of bits set to 1 in the lwset 
/// @param s  pointer to the lwset
/// @return  the number of bits set to 1 in the lwset
static inline int                      lwset_count(const lwset *s) {
    invraisecode(ERR_NULLABLE_PTR, s != NULL, "Pointer is NULL");
    int count = 0;
    for (unsigned short i = s->low; i < s->high; ++i)
        count += ((s->value >> i) & 1);
    return count;
}

// --------------------- Modification functions -------------------------------
/// @brief Sets a specific bit in the lwset to a specified value (true or false)
/// @param s pointer to the lwset
/// @param index the index of the bit to set
/// @param value the value to set the bit to (true or false)
/// @return pointer to the modified lwset
static inline lwset          *lwset_setvalue(lwset *s, unsigned short index, bool value) {
    invraisecode(ERR_NULLABLE_PTR, s != NULL, "input pointer is NULL");
    invraisecode(ERR_OUT_OF_RANGE, index >= s->low && index < s->high, 
            "index %u is out of bounds for lwset", index);
    if (value)
        s->value |= (1UL << index);
    else
        s->value &= ~(1UL << index);
    return s;
}

/// @brief Sets a specific bit in the lwset to true (1)
/// @param s pointer to the lwset 
/// @param index  the index of the bit to set
/// @return  pointer to the modified lwset
static inline lwset          *lwset_set(lwset *s, unsigned short index) {
    return lwset_setvalue(s, index, true);
}
/// @brief Sets a specific bit in the lwset to false (0)
/// @param s pointer to the lwset 
/// @param index  the index of the bit to set
/// @return  pointer to the modified lwset
static inline lwset          *lwset_unset(lwset *s, unsigned short index) {
    return lwset_setvalue(s, index, false);
}

/// @brief Sets a range of bits in the lwset to a specified value (true or false)
/// @param s pointer to the lwset 
/// @param low the starting index of the range (inclusive)
/// @param high the ending index of the range (inclusive)
/// @param value the value to set the bits to (true or false)
/// @return pointer to the modified lwset
static inline lwset           *lwset_setrangevalue(lwset *s, unsigned short low, unsigned short high, bool value) {
    invraisecode(ERR_NULLABLE_PTR, s != NULL, "Pointer is NULL");
    invraisecode(ERR_OUT_OF_RANGE, low >= s->low && high <= s->high && low < high, 
        "range [%u, %u] is out of bounds for lwset with range [%u, %u]", low, high, s->low, s->high);
    for (unsigned short i = low; i < high; ++i)
        lwset_setvalue(s, i, value);
    return s;
}

/// @brief  Computes the union of two lwsets and stores the result in s1  
/// @param s1   first lwset pointer
/// @param s2   second lwset pointer
/// @return  pointer to the modified s1
static inline lwset                    *lwset_union(lwset *restrict s1, const lwset *restrict s2) {
    invraisecode(ERR_NULLABLE_PTR, s1 != NULL && s2 != NULL, 
        "Pointers is NULL %p %p", (void*) s1, (void*) s2);
    s1->value |= s2->value;
    return s1;
}

/// @brief  Computes the intersection of two lwsets and stores the result in s1
/// @param s1   first lwset pointer
/// @param s2   second lwset pointer
/// @return  pointer to the modified s1
static inline lwset                    *lwset_intersect(lwset *restrict s1, const lwset *restrict s2) {
    invraisecode(ERR_NULLABLE_PTR, s1 != NULL && s2 != NULL, 
        "Pointers is NULL %p %p", (void*) s1, (void*) s2);
    s1->value &= s2->value;
    return s1;
}

/// @brief  Computes the difference of two lwsets and stores the result in s1
/// @param s1   first lwset pointer
/// @param s2   second lwset pointer
/// @return  pointer to the modified s1
static inline lwset                    *lwset_minus(lwset *restrict s1, const lwset *restrict s2) {
    invraisecode(ERR_NULLABLE_PTR, s1 != NULL && s2 != NULL, 
        "Pointers is NULL %p %p", (void*) s1, (void*) s2);
    s1->value &= ~s2->value;
    return s1;
}

/// @brief Computes the symmetric difference of two lwsets and stores the result in s1
/// @param s1  first lwset pointer
/// @param s2  second lwset pointer
/// @return  pointer to the modified s1
lwset                    *lwset_symmdiff(lwset *restrict s1, const lwset *restrict s2) {
    invraisecode(ERR_NULLABLE_PTR, s1 != NULL && s2 != NULL, 
        "Pointers is NULL %p %p", (void*) s1, (void*) s2);
    s1->value ^= s2->value;
    return s1;
}

// ----------------------------- CONVERTERS ----------------------------------------

// TODO: probably that in converter.h
// hset *lwset_to_hset(const lwset *s); create  a new VALUE64_INT hset

// ------------------------ PRINTERS/CHECKERS ---------------------------------------
/// @brief Prints the lwset to the specified output stream
/// @param out output stream (e.g., stdout, stderr, or a file), CAN be NULL
/// @param s pointer to the lwset
/// @return number of characters printed
extern int                      lwset_techfprint(FILE *restrict out, const lwset *restrict s);

/// @brief  Prints the lwset to the standard output
/// @param s pointer to the lwset
/// @return number of characters printed
static inline int               lwset_techprint(const lwset * s) {
    return lwset_techfprint(stdout, s);
}

/// @brief Prints the lwset to the logfile with offset spaces before the actual content  
/// @param s pointer to the lwset
/// @return  number of characters printed
static inline int               lwset_techlog(const lwset * s) {
    return lwset_techfprint(logfile, s);
}
/// @brief  Checks if the lwset is valid, ensuring that the range is within bounds and that no bits outside the specified range are set
/// @param s  pointer to the lwset
/// @return  true if the lwset is valid, false otherwise
extern bool              lwset_isvalid(const lwset * s);

// --------------------------------- SERIALIZATION ----------------------------------
/// @brief Saves the lwset to a specified output stream in a pseudo-json format
/// @param out output stream (e.g., stdout, stderr, or a file), CAN be NULL
/// @param s pointer to the lwset
/// @return number of characters printed
extern int                      lwset_fsave(FILE *restrict out, const  lwset *restrict s);

/// @brief  Saves the lwset to the standard output in a pseudo-json format
/// @param s pointer to the lwset 
/// @return number of characters printed
static inline int               lwset_save(const lwset * s) {
    return lwset_fsave(stdout, s);
}

/// @brief Loads the lwset from a specified input stream in a pseudo-json format
/// @param in input stream (e.g., stdin, a file)
/// @param s pointer to the lwset (if NULL - dummy read)
/// @return true - ok, false - wrong format, etc... 
extern bool                      lwset_fload(FILE *restrict in, lwset *restrict s);

/// @brief  Loads the lwset from the standard input in a pseudo-json format
/// @param s  pointer to the lwset (if NULL - dummy read)
/// @return  true - ok, false - wrong format, etc... 
static inline bool               lwset_load(lwset * s) {
    return lwset_fload(stdin, s);
}

// fs serialization
/// @brief Saves the lwset to a specified output stream in a pseudo-json format
/// @param target pointer to valid fs
/// @param s pointer to the lwset
/// @return number of characters printed
extern int                       lwset_serialize(fs *restrict target, const lwset *restrict s);

/// @brief Load the lwset from a specified fs
/// @param target pointer to valid fs
/// @param s pointer to the lwset
/// @return number of characters printed
extern bool                      lwset_loadfs(const fs *restrict target, lwset *restrict s);

// ------------------------------------ ETC. ----------------------------------------

#endif /* !_LWSET_H */