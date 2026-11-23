# hset_utils

`hset_utils` is a high-level functional toolkit for the **hset** library. While `hset` provides the storage, `hset_utils` provides the logic: mathematical set operations, data aggregation (reduction), complex filtering (SQL-like), and value mapping.

It is designed to allow developers to treat hash sets as queryable, transformable, and aggregatable data structures.

## 🚀 Core Modules

### 1. Set Mathematics (Set Theory)
Perform classic algebraic operations between two sets. This module allows you to create new sets based on the relationships between existing ones.

* **Union (`hset_union`):** Elements present in either set.
* **Intersection (`hset_intersect`):** Elements present in both sets.
* **Difference (`hset_minus`):** Elements in the first set that are not in the second.
* **Symmetric Difference (`hset_symmdiff`):** Elements present in one set or the other, but not both.
* **Subset Checks:** Verify if one set is a subset (`hset_in`) or a strict subset (`hset_strictin`) of another.

### 2. The Reduction Engine (Aggregation)
The reduction engine allows you to collapse a set of values into a single "accumulator" value. This is the C implementation of the `reduce` pattern found in functional programming.

* **Standard Reduction:** Pass a custom function to calculate sums, products, or complex objects.
* **Specialized Aggregators:** High-speed, pre-defined reducers for:
    * **Numeric:** Sum, Max, Min, Count.
    * **Filesystem/Strings:** `hset_agg_fs` for aggregating string patterns.
* **Filtered Reduction:** Apply a predicate (filter) *during* the reduction process to only process elements that meet certain criteria.

### 3. The Filtering Engine (SQL-style)
The most powerful feature of the library. It allows you to create "views" of a set by applying logic-based filters. The API is designed to mimic SQL `WHERE` clauses.

* **String/FS Filters:** Match prefixes, use `LIKE` patterns, or filter by string length.
* **Integer Filters:** Perform range comparisons (`<`, `<=`, `>`, `>=`, `==`, `!=`) and "between" checks.
* **Two-Step Workflow:** 
    1. **Create:** `hset_create_..._filter(...)` creates a new set representing the filtered results.
    2. **Apply:** `hset_apply_..._filter(...)` applies the filter logic directly to an existing set.

### 4. Mapping (Transformation)
Transform every element in a set into a new value using a mapper function.
* **`hset_init_map`:** Creates a new set where each element is the result of `mapper(original_element)`.

## 📖 Usage Guide

### Set Algebra Example
```c
hset set_a = HSET_CREATE_INT(1, 2, 3, 4);
hset set_b = HSET_CREATE_INT(3, 4, 5, 6);

// Result: {3, 4}
hset intersection = hset_intersect(&set_a, &set_b);

// Result: {1, 2}
hset difference = hset_minus(&set_a, &set_b);
```

### Filtering Example (SQL-style)
```c
// Assume we have a set of filesystem objects
hset fs_set = ...; 

// "SELECT * FROM fs_set WHERE length >= 10"
hset limited_set = hset_create_fsminlen_int(fs_set, 10);

// "SELECT * FROM fs_set WHERE name LIKE 'report_%"
hset reports = hset_create_fsulike_str(fs_set, "report_%");
```

### Reduction Example (Aggregation)
```c
// Create an accumulator for integers
hset_accum acc = HSET_ACCUM_INT_ZERO;

// Custom reducer: cumulative sum
hset_reduce(&my_set, acc, my_sum_func);

printf("Total sum: %d\n", hset_accum_getint(&acc)->value.ival);
```

## 🛠 API Reference Summary

| Module | Function Prefix | Typical Use Case |
| :--- | :--- | :--- |
| **Set Logic** | `hset_union`, `hset_intersect`, `hset_minus`, `hset_symmdiff` | Combining/comparing sets. |
| **Aggregators** | `hset_reduce`, `hset_sum_...`, `hset_max_...` | Calculating stats (sum, max) from a set. |
| **Filtering** | `hset_create_..._filter`, `hset_apply_..._filter` | Finding elements matching a pattern. |
| **Mapping** | `hset_init_map` | Transforming values (e.g., `int` $\rightarrow$ `double`). |

## ⚠️ Safety & Requirements

* **Memory:** Most operations create new `hset` objects. You must call `hset_free` on the resulting sets to prevent memory leaks.
* **Dependencies:** Requires `hset` and `value64_tarray` to be properly initialized.
* **Complexity:** While set operations are $O(N)$ (where $N$ is the size of the sets), the internal hash table lookups keep the constant factor extremely low.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007