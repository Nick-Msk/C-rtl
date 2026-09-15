# Command Dispatcher (`command_executor.h`)

> [!IMPORTANT]
> This module provides a generic, type-safe command dispatching mechanism using **Opaque Contexts**. It is designed for high modularity and strict encapsulation.

## Overview

The `command_executor` allows for the registration and execution of commands. It separates the **dispatching logic** (finding and running a command) from the **command logic** (the actual implementation). 

A key architectural feature of this module is the use of an **Opaque Context** (`Runtimedata`), which ensures that the command executor remains completely decoupled from the application's internal state.

## Key Architectural Concept: Opaque Context

The module uses an incomplete type definition:
`typedef struct Runtimedata Runtimedata;`

**Why this matters:**
1. **Encapsulation:** The executor does not know the internal fields of `Runtimedata`. It cannot access them, and it doesn't need to.
2. **Type Safety:** Unlike using `void *`, this approach prevents accidental passing of unrelated pointers. You can only pass a pointer of type `Runtimedata*`.
3. **Binary Stability:** You can change the structure of `Runtimedata` in your `.c` files without changing the header, meaning the executor's implementation never needs to change.

## Data Structures

### `Command`
A structure representing a single executable command.
- `name`: The string identifier for the command.
- `shortlen`: Cached length of the name for optimized comparisons.
- `desc`: A human-readable description.
- `proc`: A `process_unit` function pointer that performs the command's logic.

### `process_unit`
The function signature required for any command implementation:
`int (*process_unit)(Runtimedata *tr);`

## API Reference

| Function | Description |
| :--- | :--- |
| `process_command(name, cmd, rt)` | Finds the command by `name` and executes its `proc` function, passing the provided `rt` context. Returns `true` if the command was found and executed successfully. |

### Macros
| Macro | Description |
| :--- | :--- |
| `CommandInit(...)` | A designated initializer macro for quick and safe initialization of `Command` structures. |

## Usage Example

```c
#include "command_executor.h"
#include <stdio.h>

// 1. Define the actual data structure in your implementation file
struct Runtimedata {
    int player_score;
    char *player_name;
};

// 2. Implement the command logic
int cmd_score(Runtimedata *rt) {
    printf("Player %s score: %d\n", rt->player_name, rt->player_score);
    return 0;
}

int main() {
    // 3. Initialize the context
    struct Runtimedata context = { .player_score = 100, .player_name = "Hero" };

    // 4. Define the command table
    Command cmd_table[] = {
        CommandInit(.name = "score", .desc = "Show current score", .proc = cmd_score)
    };

    // 5. Execute via the generic dispatcher
    process_command("score", &cmd_table[0], &context);

    return 0;
}
```

## Complexity Analysis
* **Space Complexity:** $O(1)$ per command.
* **Time Complexity:** $O(L)$ where $L$ is the length of the command name for the initial comparison.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
