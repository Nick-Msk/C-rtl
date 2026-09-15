# Boolean Utility (`bool.h`)

> [!NOTE]
> This is a minimal utility header. Originally created to provide boolean support in pre-C99 environments, it now serves primarily as a helper for string conversion.

## Overview

A lightweight wrapper around the standard `<stdbool.h>`, providing convenient conversion from boolean types to human-readable strings.

## API Reference

### Functions

| Function | Description |
| :--- |
| `bool_str(bool v)` | Converts a `bool` value to its string representation: `"true"` or `"false"`. |

## Usage Example

```c
#include "bool.h"
#include <stdio.h>

int main() {
    bool is_active = true;
    printf("Status: %s\n", bool_str(is_active)); // Output: Status: true
    return 0;
}
```

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       