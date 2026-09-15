# Numeric Operations & Hashing (`numeric_ops.h`)

> [!NOTE]
> `numeric_ops.h` is a collection of lightweight mathematical utilities. It provides efficient implementations for primality testing and a variety of well-known hashing algorithms for both strings and integers.

## Overview

The module is divided into two main functional domains:
1.  **Number Theory:** Tools for working with prime numbers.
2.  **Hashing Algorithms:** Implementations of industry-standard hash functions for strings and high-quality bit-mixing for integers.

---

## 1. Prime Number Utilities

These functions are useful for cryptographic applications, random number generation, or mathematical simulations.

| Function | Complexity | Description |
| :--- | :--- |
| `isprime(n)` | $O(\sqrt{n})$ | Simple primality test using trial division. |
| `calc_next_prime(n)` | Variable | Finds the first prime number greater than `n`. |
| `next_prime(n)` | (External) | Optimized version for finding the next prime. |
| `is_prime_miller(n)` | $O(k \log^3 n)$ | Advanced primality test (Miller-Rabin) for larger/complex numbers. |

---

## 2. Hashing Algorithms

This module provides several hashing algorithms, allowing the developer to choose between speed and collision resistance depending on the workload.

### String Hashing
Ideal for hash tables or quick identification of strings.

| Function | Algorithm | Characteristics |
| :--- | :--- | :--- |
| `hash_djb2(str)` | **DJB2** | Extremely fast, excellent distribution for short strings. |
| `hash_fnv1a(str)` | **FNV-1a** | High collision resistance, very popular for general-purpose use. |
| `hash_sdbm(str)` | **SDBM** | Used in various databases; effective for varied string lengths. |

### Numeric Hashing (Bit Mixing)
Used to ensure that integer values are distributed uniformly across a hash table, even when inputs are sequential.

| Function | Algorithm | Description |
| :--- | :--- |
| `hash_long(x)` | **SplitMix64-style** | A high-quality bit-mixer for 64-bit integers. Uses constant "magic numbers" to ensure high entropy in the output bits. |

---

## Usage Example

```c
#include "numeric_ops.h"
#include <stdio.h>

int main() {
    // --- Primality Testing ---
    int num = 997;
    if (isprime(num)) {
        printf("%d is a prime number.\n", num);
    }

    // --- String Hashing ---
    const char *data = "hello_world";
    unsigned long h = hash_fnv1a(data);
    printf("FNV1a hash of '%s': %lu\n", data, h);

    // --- Integer Bit-Mixing ---
    int64_t val = 123456789;
    uint64_t mixed = hash_long(val);
    printf("Original: %ld, Mixed: %lu\n", val, mixed);

    return 0;
}
```

## Dependencies
* `math.h` (for `sqrt`)
* `stdint.h` (for fixed-width integer types)
* `stdbool.h`

## Complexity Summary
| Task | Complexity | Best For |
| :--- | :--- | :--- |
| **Primality (Small $n$)** | $O(\sqrt{n})$ | Simple checks. |
| **Primality (Large $n$)** | Probabilistic | Heavy mathematical computations. |
| **String Hashing** | $O(L)$ | Hash tables, fast lookups. |
| **Integer Mixing** | $O(1)$ | Preventing collisions in sequential data. |

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       