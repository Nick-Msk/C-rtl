# Fast-String Array (`fs_array.h`)

> [!IMPORTANT]
> **Dependency Required:** This module requires `fs.h` to function. It is a high-level container designed specifically to manage collections of `fs` (autoextendable string) objects.

## Overview

`fs_array.h` provides a dynamic, growable array of `fs` objects. Unlike a standard array of `char*`, this is an array of **managed string objects**. Each element in the array carries its own memory management metadata (via `FS_FLAGS`), allowing the array to store a mix of static literals, stack-allocated strings, and heap-allocated strings seamlessly.

The module provides tools for dynamic resizing, ownership transfer (moving strings in/out of the array), and efficient iteration using the `fsl` (Fast-String List) iterator.

## Core Concepts

### 1. Ownership Transfer (Move Semantics)
Because `fs` objects manage their own memory, the array implements "Move" semantics to prevent memory leaks and double-frees:
* **`fsarr_attach`**: Moves ownership of an `fs` object into the array at a specific position. The original `fs` object should no longer be used.
* **`fsarr_detach`**: Removes an `fs` object from the array and returns it. The array no longer manages the memory of that element; the caller is now responsible for it.

### 2. The `fsl` Iterator
To avoid expensive copying of the `fs` structures, the module provides the `fsl` (Fast-String List) type. It acts as a lightweight "view" or "cursor" into a specific element of the array, allowing efficient access to the string content.

---

## API Reference

### Lifecycle & Memory Management
| Function | Description |
| :--- |
| `fsarr_init(cnt)` | Initializes an array with a given capacity. |
| `fsarr_free(arr)` | Frees the array and all `fs` elements contained within it. |
| `fsarr_increase(arr, n)` | Increases the array capacity. |
| `fsarr_shrink(arr, n)` | Reduces the array capacity. |
| `fsarr_clean(arr, free)` | Clears the array. If `free` is true, it deallocates all elements. |

### Element Access
| Function | Description |
| :--- | :--- |
| `fsarr_get(arr, pos)` | Returns a pointer to the `fs` object at the specified index. |
| `fsarr_exists(arr, pos)`| Returns `true` if an element exists at the given position. |
| `fsarr_cnt(arr)` | Returns the current number of elements. |
| `fsarr_sz(arr)` | Returns the total allocated capacity. |

### Iteration & View (`fsl` API)
| Function | Description |
| :--- | :--- |
| `fsarr_getfsl(arr, pos)`| Creates a lightweight iterator (`fsl`) for the element at `pos`. |
| `fsl_get(l, p)` | Retrieves the `fs` object at position `p` via an iterator. |
| `fsl_elem(l, p)` | Returns a pointer to the internal character data. |
| `fsl_len(l)` | Returns the length of the string in the iterator. |

### Serialization & I/O
| Function | Description |
| :--- | :--- |
| `fsarr_fprint(f, arr)` | Prints the array contents to a file stream. |
| `fsarr_save(fname, arr)`| Saves the entire array to a file. |
| `fsarr_load(fname)` | Loads an array from a file. |

---

## Usage Example

```c
#include "fs.h"
#include "fs_array.h"
#include <stdio.h>

int main() {
    // 1. Initialize array
    fsarray my_list = fsarr_empty();

    // 2. Add elements (Moving ownership into the array)
    fs s1 = fscopy("Hello World");
    fsarr_attach(&my_list, 0, &s1);

    fs s2 = fscopy("Fast Strings are great");
    fsarr_attach(&my_list, 1, &s2);

    // 3. Using the Iterator (fsl)
    fsl iterator = fsarr_getfsl(&my_list, 0);
    printf("Element 0: %s\n", fsl_get(iterator, 0).v);

    // 4. Detaching an element (Transferring ownership to caller)
    fs my_moved_string = fsarr_detach(&my_list, 1);
    printf("Detached string: %s\n", my_moved_string.v);
    
    // IMPORTANT: We must free the detached string manually
    fsfree(&my_moved_string);

    // 5. Cleanup the array
    fsarr_free(&my_list);

    return 0;
}
```

## Complexity Analysis
* **Access:** $O(1)$
* **Insertion/Deletion:** $O(N)$
* **Space Complexity:** $O(N \times \text{size\_of(fs)})$

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       