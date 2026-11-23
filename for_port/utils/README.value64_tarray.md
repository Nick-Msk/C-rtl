# Value64 Typed Array

`value64_tarray` is a companion module to the **Value64** library. While the core `value64` union is a high-performance, "type-blind" 64-bit container, `value64_tarray` provides the necessary wrapper to store these values in arrays while maintaining their type information.

This module is essential when you need to build **heterogeneous collections** (arrays containing different types like `int`, `double`, and `string` simultaneously) without managing parallel type arrays manually.

## 🏗 Core Concepts

### `value64_typed`
The fundamental unit of this module. It bundles a `value64` value with its corresponding `value64_type`.
```c
typedef struct {
    value64      val;   // The 64-bit data
    value64_type typ;   // The explicit type tag
} value64_typed;
```

### `value64_tarray`
A dynamic, heap-allocated array of `value64_typed` elements. It supports automatic resizing and provides specialized constructors for common primitive arrays.

### `value64_static_tarray`
A lightweight, non-heap-allocated wrapper designed for static initialization using powerful preprocessor macros.

## 🚀 Key Features

### 1. Heterogeneous Storage
Unlike standard C arrays where every element must be the same type, a `value64_tarray` can store an `int`, a `double`, and a `char*` in the same container safely.

### 2. Advanced Memory Semantics
* **Push:** Adds a copy of an element to the array (safe).
* **Move:** Transfers ownership of a `value64_typed` element into the array (extremely fast, avoids `strdup`/`malloc` for strings and filesystem objects).

### 3. Macro-Driven Static Initialization
The library provides a sophisticated macro system to create static, typed arrays of primitives without any runtime allocation overhead.

```c
// Creating a static array of integers using specialized macros
value64_static_tarray my_static_arr = V64TYP_INTLIST(10, 20, 30, 40);
```

## 📖 Usage Guide

### Dynamic Array (Runtime)
Use this for collections that grow during program execution.

```c
#include "value64_tarray.h"

// 1. Initialize with capacity
value64_tarray arr = value64_tarray_init(4);

// 2. Add elements (Copying)
value64_typed e1 = V64TYP_INT(100);
value64_tarray_push(&arr, e1);

// 3. Add elements (Moving - transfers ownership of strings/fs)
value64_typed e2 = V64TYP_STR("Moving this string");
value64_tarray_move(&arr, &e2); 
// e2 is now 'empty' (type UNKNOWN), ownership is inside 'arr'

// 4. Accessing elements
int val = value64_int(value64_tarray_get(&arr, 0));

// 5. Cleanup (Crucial for STR and FS types!)
value64_tarray_free(&arr);
```

### Static Array (Compile-time)
Use this for constant lookup tables or fixed configuration data.

```c
// Creates a static, read-only typed array of doubles
value64_static_tarray constants = V64TYP_DBLLIST(1.1, 2.2, 3.3);

// Accessing
double d = value64_dbl(constants.base.v[0].val);
```

## 🛠 API Reference

### Constructors & Destructors
* `value64_tarray_init(cap)`: Allocates a new dynamic array.
* `value64_tarray_free(arr)`: Frees the array and **all** internal resources (strings, etc.).
* `value64_tarray_int_from_arr`, `value64_tarray_str_from_arr`, etc.: Fast conversion from existing C arrays.

### Accessors
* `value64_tarray_getptr(arr, i)`: Returns a pointer to the $i$-th element (with bounds checking).
* `value64_tarray_get(arr, i)`: Returns a copy of the $i$-th element.
* `VALUE64_TARRAY_ELEM(arr, pos)`: Macro for quick element access.

### Debugging
* `VALUE64_TARRAY_TECHPRINT(arr)`: Technical printer that outputs the array contents and types to `stdout`.

## ⚠️ Safety Warnings

1.  **Memory Ownership:** If you use `value64_tarray_move`, the original `value64_typed` object is "cleared" (set to `UNKNOWN`). This is intentional to prevent double-freeing when the array is eventually destroyed.
2.  **Manual Freeing:** Always call `value64_tarray_free()` on dynamic arrays. Failing to do so will result in memory leaks, especially when storing `VALUE64_STR` or `VALUE64_FS`.
3.  **Static Arrays:** Do **NOT** call `value64_tarray_free()` on a `value64_static_tarray`. They are initialized via stack/static memory and do not own heap pointers.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       