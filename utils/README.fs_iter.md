 # Fast-String Iterators (`fs_iter.h`)

> [!NOTE]
> `fs_iter.h` provides a high-level abstraction for navigating and building `fs` (fast-string) objects. It implements three distinct iterator patterns: **Forward Traversal**, **Reverse Traversal**, and an **Incremental Builder**.

## Core Iteration Patterns

### 1. Forward & Reverse Traversal
Used to walk through an existing `fs` string without manual pointer arithmetic.

| Macro / Function | Type | Description |
| :--- | :--- | :--- |
| `fsforeach(s, i)` | Macro | A high-level loop for forward iteration. |
| `fseach(s)` | Function | Initializes a forward `fsiter`. |
| `fseachrev(s)` | Function | Initializes a reverse `fsiterrev`. |
| `hasnext(i)` | Macro | Checks if the iterator has more elements. |
| `next(i)` | Macro | Advances the iterator and returns the current char. |
| `curr(i)` | Macro | Returns the current character without advancing. |

### 2. Slicing (Sub-string views)
Allows creating an iterator that only sees a specific subset of the original string.

| Function | Description |
| :--- | :--- |
| `fsslice(s, from, to)` | Creates an iterator restricted to the range `[from, to)`. |

### 3. Incremental Construction (Builder Pattern)
This is a specialized iterator mode (`fsnew`) used to build a new string character-by-character. 

> [!WARNING]
> **Requirement:** The `fs` object passed to `fsinew` or `fsiapp` **must** be heap-allocated (`FS_FLAG_ALLOC`) to allow for dynamic resizing during construction.

| Function | Description |
| :--- | :--- |
| `fsinew(fs*)` | Initializes a builder for a new, empty heap-allocated string. |
| `fsiapp(fs*)` | Initializes a builder at the end of an existing heap-allocated string (for appending). |
| `elemnext(i)` | Appends the next character/element to the builder. |
| `elemend(i)` | Finalizes the string by setting the length and null-terminator. |

---

## API Reference Summary

### Iterators

| Struct | Purpose | Direction |
| :--- | :--- | :--- |
| `fsiter` | Standard iteration | Forward $\rightarrow$ |
| `fsiterrev` | Reverse iteration | Backward $\leftarrow$ |
| `fsnew` | Building a new string | Forward $\rightarrow$ |

### Essential Macros

```c
// Forward Loop
for (fsiter i = fseach(my_fs); hasnext(i); next(i)) {
    char c = curr(i);
    // ...
}

// Reverse Loop
for (fsiterrev i = fseachrev(my_fs); hasnextrev(i); nextrev(i)) {
    char c = currrev(i);
    // ...
}

// Construction (Builder)
fs my_new_str = fsinew(&heap_fs);
for (int i = 0; i < 10; i++) {
    char c = (i % 2 == 0) ? 'A' : 'B';
    elemcurr(my_new_str) = c;
}
elemend(my_new_str);
```

## Implementation Details

* **Performance:** Most functions are `static inline` to ensure minimal overhead, performing similarly to raw pointer arithmetic.
* **Safety:** Uses `invraise` and `userraiseint` for bounds checking and error reporting during construction.
* **Memory:** The `fsnew` pattern is highly optimized for building strings in a single allocation pass (when the size is known) or via dynamic resizing.

## Dependencies
- `fs.h` (The core fast-string type)
- `bool.h`, `log.h`, `common.h`, `error.h`, `checker.h`

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       