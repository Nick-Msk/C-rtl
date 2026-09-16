# lwset

`lwset` (Lightweight Set) is an ultra-fast, memory-efficient bitset implementation for C. 

It represents a set of elements using a single **64-bit integer**, where each bit corresponds to an index. This makes it incredibly efficient for scenarios involving small, fixed-size collections where performance and minimal memory footprint are critical.

## 🚀 Key Features

* **Extreme Efficiency:** Uses a single `uint64_t` to store up to 64 elements.
* **Range-Based Constraints:** Includes `low` and `high` bounds to restrict the valid index range of the set.
* **Bitwise Set Operations:** Implements standard set theory operations (Union, Intersection, Difference, Symmetric Difference) using high-speed CPU bitwise instructions.
* **Range Initialization:** Easily initialize sets with specific bit ranges or from lists of indices.
* **Zero-Allocation:** Does not use the heap. All operations are stack-based and $O(1)$ (constant time) or $O(W)$ (where $W$ is the word size, e.g., 64).
* **Serialization:** Includes built-in support for saving/loading sets to files or filesystem resources.

## 🏗 Data Structure

A `lwset` consists of the bitmask and its validity boundaries:

```c
typedef struct {
    uint64_t value;  // The 64-bit bitmask
    unsigned short low;  // Minimum valid index (inclusive)
    unsigned short high; // Maximum valid index (exclusive)
} lwset;
```

## 📖 Usage Guide

### 1. Initialization

You can initialize a set using macros for convenience or functions for range control.

```c
// Initialize empty (all bits 0) with full range [0, 63]
lwset s = lwset_initunlim();

// Initialize with specific bits set (bits 1, 3, and 5)
lwset s = LWSET_LIST(1, 3, 5);

// Initialize a range of bits (bits 10 through 19)
lwset s = lwset_init1(10, 20);

// Initialize with all bits set to 1
lwset s = lwset_init1unlim();
```

### 2. Bit Manipulation

Directly set or check specific indices.

```c
lwset s = lwset_initunlim();

// Setting/Unsetting bits
lwset_set(&s, 5);          // Sets bit 5 to true
lwset_unset(&s, 3);        // Sets bit 3 to false
lwset_setrangevalue(&s, 10, 20, true); // Sets bits 10-19 to true

// Checking bits
bool is_set = lwset_get(&s, 5);
```

### 3. Set Theory Operations

Perform mathematical set operations on two sets.

```c
lwset s1 = LWSET_LIST(1, 2, 3);
lwset s2 = LWSET_LIST(3, 4, 5);

// Union (s1 | s2) -> {1, 2, 3, 4, 5}
lwset_union(&s1, &s2);

// Intersection (s1 & s2) -> {3}
lwset_intersect(&s1, &s2);

// Difference (s1 \ s2) -> {1, 2}
lwset_minus(&s1, &s2);

// Symmetric Difference (s1 ^ s2) -> {1, 2, 4, 5}
lwset_symmdiff(&s1, &s2);
```

### 4. Set Analysis

```c
lwset s1 = LWSET_LIST(1, 2);
lwset s2 = LWSET_LIST(1, 2, 3, 4);

bool is_subset = lwset_in(&s1, &s2);      // true
bool is_strict  = lwset_strictin(&s1, &s2); // true
int count       = lwset_count(&s1);        // 2
```

## 💾 Serialization

`lwset` supports pseudo-JSON serialization for easy storage.

```c
lwset s = LWSET_LIST(1, 5, 10);

// Save to file
lwset_save(&s); 

// Load from file
lwset loaded_s;
lwset_load(&loaded_s);
```

## ⚠️ Constraints & Limitations

* **Maximum Size:** The set is limited to **64 elements** (one `uint64_t`).
* **Not for Large Sets:** This is not a replacement for a dynamic bitset or hash set if you need to store more than 64 elements.
* **Index Bounds:** Always ensure indices are within the `[low, high)` range defined during initialization to avoid `ERR_OUT_OF_RANGE`.

## 🛠 Complexity Summary

| Operation | Complexity | Description |
| :--- | :--- | :--- |
| `get` / `set` | $O(1)$ | Constant time bitwise manipulation. |
| `union` / `intersect` | $O(1)$ | Constant time bitwise logic. |
| `count` | $O(W)$ | Linear relative to bit-width (64). |
| `list` initialization | $O(N)$ | Linear relative to number of elements. |

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       