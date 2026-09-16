# Value64

A high-performance, lightweight **64-bit discriminated union system** implemented in C. 

`Value64` provides a unified way to handle various data types within a fixed 64-bit memory footprint. It is designed for high-speed data processing, efficient memory management (supporting both copy and move semantics), and robust serialization.

## ⚠️ Important Architecture Note
The `value64` union **does not store its own type tag**. To ensure maximum performance and a minimal 64-bit footprint, the type (`value64_type`) must be managed externally by the developer. Always pair a `value64` object with its corresponding `value64_type` when calling API functions.

## 🚀 Key Features

* **Fixed-Size Memory:** Exactly 64 bits (size of `uint64_t`), making it ideal for high-density arrays and hash map keys.
* **Memory Management:** 
    * **Copy Semantics:** Traditional deep/shallow copying.
    * **Move Semantics:** High-efficiency "destructive" moves to transfer ownership of heap-allocated resources (like strings or filesystem objects) without expensive `memcpy` or `strdup` calls.
* **Comprehensive Conversion Matrix:** A massive suite of specialized functions for converting between almost any supported type.
* **Standard Library Compatibility:** Includes pointer-based comparators compatible with standard C functions like `qsort` and `bsearch`.
* **Powerful Serialization:** Built-in support for reading/writing values to files, strings, and custom data structures (`DS`).
* **Advanced Filtering:** A collection of predicate functions for filtering datasets (numeric comparisons, string prefix matching, etc.).

## 🛠 Supported Types

| Type | C Equivalent | Description |
| :--- | :--- | :--- |
| `VALUE64_INT` | `int` | Standard integer |
| `VALUE64_LONG` | `long` | Standard long |
| `VALUE64_ULONG` | `unsigned long` | Unsigned long |
| `VALUE64_DBL` | `double` | Double precision float |
| `VALUE64_CHR` | `char` | Single character |
| `VALUE64_BOOL` | `bool` | Boolean |
| `VALUE64_PTR` | `void*` | Generic pointer |
| `VALUE64_STR` | `char*` | Heap-allocated C-string |
| `VALUE64_FS` | `fs*` | Filesystem resource object |
| `VALUE64_FILE` | `FILE*` | Standard C file pointer |

## 📖 Usage Guide

### Initialization
Use the `LITERAL64_*` macros for rapid initialization.

```c
// Primitive initialization
value64 v_int = LITERAL64_INT(42);
value64 v_dbl = LITERAL64_DBL(3.14159);

// String initialization (Note: This performs a pointer copy, not a deep copy)
value64 v_str = LITERAL64_STR("Hello World");

// Filesystem initialization
value64 v_fs = LITERAL64_FS(my_fs_object);
```

### Conversions
The library provides two ways to convert types:

```c
// 1. Copy Conversion (Non-destructive)
// v_str remains intact
value64 v_new_str = value64_convert(v_str, VALUE64_STR, VALUE64_STR);

// 2. Move Conversion (Destructive/High Performance)
// v_str is cleared/reset to 0 after execution
value64 v_moved_fs = value64_convert_move(&v_str, VALUE64_STR, VALUE64_FS);
```

### Memory Management
To prevent memory leaks when using resource-owning types (`STR`, `FS`), you must manually free them.

```c
// Destructors
value64_free(&v_str, VALUE64_STR);
value64_free(&v_fs, VALUE64_FS);
```

### Sorting and Searching
The library includes optimized sorting and searching algorithms for different data types.

```c
value64 arr[10];
// ... populate arr ...

// Sort an array of integers
value64_sort_int(arr, 10);

// Perform a binary search
int index = value64_binsearch(target_val, VALUE64_INT, arr, 10);
```

## 📂 API Modules

### 🔧 Constructors & Destructors
* `value64_create...`: Creates new objects (with heap allocation if necessary).
* `value64_clone`: Performs a deep copy of the object.
* `value64_free`: Safely releases resources for `STR` and `FS` types.

### 🔄 Conversions
* `value64_convert`: General conversion function (Copy Semantics).
* `value64_convert_move`: Transfers ownership (Move Semantics).
* `value64_convert_...`: Specialized direct conversion functions for performance.

### 🔍 Comparison & Sorting
* `value64_compare`: Generic comparison between two objects.
* `value64_sort` / `value64_revsort`: Sorting (Ascending/Descending).
* `value64_binsearch`: Binary search for sorted arrays.

### 💾 Serialization
* `value64_tofile`: Serializes a value to a file.
* `value64_loadfile`: Deserializes a value from a file.
* `value64_tostr`: Converts a value into a string format.

### 🎯 Filtering
* `value64_filter_...`: Predicate functions used for data validation and filtering (e.g., `value64_filter_intgt_int` for "greater than" comparisons).

## ⚠️ Safety Warnings

1.  **Manual Type Management:** Because the union is "blind" to its own type, passing the wrong `value64_type` to a function will result in undefined behavior (interpreting integer bits as a pointer, for example).
2.  **Dangling Pointers:** Using `LITERAL64_STR` with a local stack variable will result in a dangling pointer once the variable goes out of scope.
3.  **Memory Ownership:** Always track whether a `value64` object owns heap memory (types `STR` and `FS`) and call `value64_free` accordingly.

## 🛠 Dependencies

The library relies on the following internal components:
* `bool.h`, `log.h`, `common.h`, `error.h`, `checker.h`, `fs.h`, `fs_iter.h`, `numeric_ops.h`, `getword.h`, `fileutils.h`.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       