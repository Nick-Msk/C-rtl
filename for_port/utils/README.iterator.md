# Lightweight Iterators for C

`iterator.h` is a high-performance, macro-based iteration toolkit for the C programming language. It provides ergonomic and safe ways to loop over fixed lists, arrays of pointers, and generic arrays, significantly reducing boilerplate code while maintaining zero runtime overhead.

This library is designed to bridge the gap between low-level C performance and the expressive syntax found in higher-level languages.

## 🚀 Key Features

* **Sentinel-Based List Iteration:** Iterates over inline lists of primitives (int, long, double, etc.) and strings using internal sentinel values (like `INT_MAX` or `NULL`), eliminating the need for manual size management.
* **Pointer Array Iterators:** Specialized macros for iterating over arrays of pointers, providing easy access to either the **pointer to the element** or the **element's index**.
* **Generic Array Iteration:** Powerful macros for iterating over any standard array by providing a size/count.
* **Type-Safe via `_Static_assert`:** Uses compile-time checks to ensure that pointer-based iterators are only used with actual pointer arrays.
* **Collision-Resistant:** Utilizes `__COUNTER__` and unique ID generation to ensure macros work correctly even when nested inside complex loops.
* **Advanced C Extensions:** Leverages `typeof_unqual` (available in GCC/Clang) to provide seamless dereferencing and type handling.

## 📖 Usage Guide

### 1. Iterating over Fixed Lists
The `foreach[type]` macros allow you to iterate over a comma-separated list of values defined directly in the loop.

```c
// Iterating over integers
foreachint(val, 1, 10, 25, 50) {
    printf("Number: %d\n", val);
}

// Iterating over strings
foreachstring(str, "Apple", "Banana", "Cherry") {
    printf("Fruit: %s\n", str);
}
```

### 2. Iterating over Arrays of Pointers
Useful for collections of strings or object pointers.

#### Getting the Pointer to the element
```c
const char *names[] = {"Alice", "Bob", "Charlie"};

// 'ptr' is a pointer to the current element
pforeach_arrptr(ptr, names) {
    printf("Name: %s\n", *ptr);
}
```

#### Getting the Index
```c
const char *names[] = {"Alice", "Bob", "Charlie"};

// 'i' is the current index
foreach_arrptr(i, names) {
    printf("Index %d: %s\n", i, names[i]);
}
```

### 3. Generic Array Iteration
When you have a standard array and a defined size, use the `foreach_arr` macros.

```c
double data[] = {1.1, 2.2, 3.3, 4.4};
int n = 4;

// 'val' is the current element
foreach_arr(val, data, n) {
    printf("Value: %f\n", val);
}

// 'ptr' is the pointer to the current element
pforeach_arr(ptr, data, n) {
    printf("Value: %f\n", *ptr);
}
```

## 🛠 Technical Specifications

### Requirements
* **Compiler:** Requires a C compiler that supports `typeof_unqual` (GCC or Clang) for advanced pointer handling.
* **Complexity:** All macros evaluate to standard `for` loops, meaning they introduce zero runtime overhead beyond the iteration itself ($O(1)$ per step).

### Macro Comparison Table

| Macro | Input Type | Iteration Target | Result Variable |
| :--- | :--- | :--- | :--- |
| `foreachint` | List | Primitives (`int`) | The value |
| `foreachstring`| List | `const char*` | The pointer |
| `pforeach_arrptr`| Array of pointers| Pointers | The pointer |
| `foreach_arrptr`| Array of pointers| Pointers | The index |
| `foreach_arr` | Any Array | Elements | The element |
| `pforeach_arr` | Array of pointers| Pointers | The pointer |

## ⚠️ Safety Note
Because this library relies heavily on the preprocessor and `typeof_unqual`, it is highly optimized for **GCC and Clang**. If using other compilers, some pointer-based iterators may require adjustments or may not be supported.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       