# Context Management Utility (`context.h`)

A lightweight, minimalistic key-value pair management utility for C applications. It provides a simple way to store, retrieve, and manage configuration or state information using a sorted context of elements.

## Features

* **Key-Value Storage:** Store string-based keys and values.
* **Sorted Elements:** Maintains elements in a structured way (suitable for sorted access).
* **Lightweight API:** Minimalist design with low overhead.
* **Debugging Support:** Built-in functions for technical printing and inspection of the context state.

## Data Structures

### `ContextSortedElem`
Represents a single entry in the context.
- `name`: The key.
- `value`: The associated value.
- `flags`: Configuration flags for the element.
- `next`: Pointer to the next element (linked list structure).

### `Context`
The main container for the context.
- `cnt`: Current number of elements.
- `ctx`: Array of pointers to `ContextSortedElem`.

## API Reference

### Lifecycle
| Function | Description |
| :--- | :--- |
| `ctxinit(int sz)` | Initializes a new context with a capacity of `sz`. |
| `ctxfreed(Context *c)` | Frees all memory associated with the context. |
| `ctxfree(c)` | Macro for `ctxfreed(&c)`. |
| `ctxreset(Context *c)` | Resets the context to its initial state. |

### Access & Modification
| Function | Description |
| :--- | :--- |
| `ctxadd(Context *c, name, value)` | Adds a new key-value pair to the context. |
| `ctxdel(Context *c, name)` | Removes an element by name. |
| `ctxget(Context *c, name)` | Returns a pointer to the element by name. |
| `ctxexists(Context *c, name)` | Returns `true` if the key exists. |
| `ctxgetvalue(Context *c, name)` | Returns the string value associated with the key. |
| `ctxcount(Context *c)` | Returns the current number of elements. |

### Debugging
| Function | Description |
| :--- | :--- |
| `ctx_techprint(Context *c, name)`| Prints technical details of the context to `stdout`. |
| `ctxprintelem(elem)` | Prints details of a specific element. |

## Usage Example

```c
#include "context.h"
#include <stdio.h>

int main() {
    // Initialize context with capacity for 10 elements
    Context my_ctx = ctxinit(10);

    // Adding elements
    ctxadd(&my_ctx, "version", "1.0.0");
    ctxadd(&my_ctx, "mode", "debug");

    // Checking existence and retrieving values
    if (ctxexists(&my_ctx, "version")) {
        printf("Version: %s\n", ctxgetvalue(&my_ctx, "version"));
    }

    // Printing context info for debugging
    ctx_techprint(&my_ctx, NULL);

    // Cleanup
    ctxfreed(&my_ctx);

    return 0;
}
```

## Dependencies

The header relies on the following internal project headers:
- `bool.h`
- `log.h`
- `common.h`
- `error.h`
- `checker.h`

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       