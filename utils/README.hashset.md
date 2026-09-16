# hset

`hset` is a high-performance, universal hash set designed to store `value64` elements. 

While standard C arrays require manual iteration for searching, `hset` uses a hash table with collision chaining to provide nearly instantaneous lookups, even as the number of elements grows. Because it is built on the `value64` foundation, it is truly "universal"—it can store integers, floating-point numbers, strings, filesystem objects, and pointers in a single unified collection.

## 🚀 Key Features

* **Universal Type Support:** Leverages the `value64` union to store any supported type without needing separate implementations for each.
* **High-Speed Lookups:** Uses a hash table for $O(1)$ average-case complexity for `get`, `set`, and `del` operations.
* **Specialized Creation:**
    * **Macros:** Rapid creation from literal lists using `HSET_CREATE_INT(...)`, `HSET_CREATE_DBL(...)`, etc.
    * **Array Conversion:** Direct conversion from existing C arrays (integers, longs, doubles, pointers, etc.) via `hset_from_..._arr`.
* **Powerful Iterators:** A family of optimized `HSET_FOREACH` macros that allow you to iterate over the set with specific types (e.g., `HSET_FOREACH_INT`) or generically.
* **Serialization Support:** Built-in ability to save and load the entire set to/from files or filesystem resources.
* **Memory Management:** Supports different allocation modes and provides simple cloning and clearing utilities.

## 🛠 Core API

### 🏗 Initialization & Creation

| Function/Macro | Description |
| :--- | :--- |
| `hset_init(sz, typ)` | Standard constructor. Sets initial size and the expected `value64_type`. |
| `HSET_CREATE_INT(...)` | **Macro:** Quickly creates a hash set from a list of integer literals. |
| `hset_from_..._arr(...)` | Creates a set by copying elements from a standard C array. |
| `hset_clone(s)` | Creates a complete copy of an existing set. |

### 🔍 Element Management

| Function | Description |
| :--- | :--- |
| `hset_set(s, val)` | Adds an element to the set (if not already present). |
| `hset_get(s, val)` | Checks if an element exists in the set. |
| `hset_del(s, val)` | Removes an element from the set. |
| `hset_cnt(s)` | Returns the number of elements currently in the set. |

### 🔄 Iteration (Macros)

The library provides specialized macros to avoid manual pointer manipulation and type casting during loops:

* **Typed Iteration:** `HSET_FOREACH_INT(se, var)`, `HSET_FOREACH_DBL(se, var)`, etc.
* **Generic Iteration:** `HSET_FOREACH(se, var)` (iterates through `value64` objects).
* **Deletion Iteration:** `HSET_FOREACH_DEL(se, var)` (safe iteration when you intend to remove elements).

## 📖 Usage Examples

### 1. Rapid Creation and Search
```c
// Create a set from literals using a macro
hset my_set = HSET_CREATE_INT(10, 20, 30, 40, 50);

// Check if an element exists
value64 search_val = LITERAL64_INT(30);
if (hset_get(&my_set, search_val)) {
    printf("Found!\n");
}
```

### 2. Iterating with Type Awareness
```c
hset my_set = hset_init_dbl(10);
hset_set(&my_set, LITERAL64_DBL(1.1));
hset_set(&my_set, LITERAL64_DBL(2.2));

// Using the typed macro for easy access
HSET_FOREACH_DBL(&my_set, val) {
    printf("Value: %f\n", val);
}
```

### 3. Serialization
```c
hset my_set = HSET_CREATE_INT(1, 2, 3);

// Save to a file
hset_save("my_data.hset", &my_set);

// Load from a file
hset loaded_set;
hset_load("my_data.hset", &loaded_set);
```

## ⚠️ Technical Notes

* **Type Consistency:** While `hset` is universal, it is highly recommended to initialize it with a specific `value64_type` (e.g., `hset_init_int`) if you know the contents in advance. This ensures consistent behavior during iteration.
* **Complexity:** All primary set operations (set, get, del) operate in $O(1)$ average time.
* **Memory:** `hset_free()` must be called to release the internal hash table memory and any heap-allocated elements (like strings) stored within the set.

## 📊 Complexity Summary

| Operation | Complexity (Avg) | Complexity (Worst) |
| :--- | :--- | :--- |
| **Insertion** | $O(1)$ | $O(N)$ |
| **Lookup** | $O(1)$ | $O(N)$ |
| **Deletion** | $O(1)$ | $O(N)$ |
| **Iteration** | $O(N)$ | $O(N)$ |

*(where $N$ is the number of elements)*

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       