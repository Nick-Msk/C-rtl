# Debugging Guards & Diagnostic Macros (`guard.h`)

> [!WARNING]
> ### 🛠 DEBUG ONLY
> This module is strictly intended for **Development and Debugging** builds. All functionality within this file is stripped out in production builds (`#ifdef NODEBUG`). Using these macros in production may lead to unexpected behavior and performance overhead.

## Overview

`guard.h` provides advanced diagnostic tools designed to catch "creeping" errors—bugs that do not cause an immediate crash but manifest after many iterations or under specific periodic conditions. 

While standard `assert()` is designed for immediate failure, these "Guards" allow for **threshold-based error triggering** and **probabilistic execution**, which is essential for debugging complex, high-iteration logic (like loops, physics engines, or data processing streams).

---

## Key Features

### 1. Threshold-based Guards (Counter Guards)
Sometimes a single error is a fluke, but the same error occurring 1,000 times indicates a catastrophic logic failure. The `GUARD` macros use static counters to trigger a program interrupt only after a specific threshold is reached.

| Macro | Threshold | Behavior |
| :--- | :--- | :--- |
| `GUARDK` | 1,000 | Logs/Raises error after 1,000 occurrences. |
| `GUARDM` | 1,000,000 | Logs/Raises error after 1,000,000 occurrences. |
| `GUARDB` | 1,000,000,000 | Logs/Raises error after 1,000,000,000 occurrences. |
| `GUARDL` | 1,000,000,000,000 | Logs/Raises error after 1T occurrences. |

**Variations:**
* **`RGUARD` (Raise):** Immediately triggers a `userraise` (crash/interrupt) when the threshold is hit.
* **`FGUARD` (Functional):** Evaluates a boolean expression. If the expression is false AND the threshold is hit, it triggers the error.
* **`FRGUARD` (Function + Raise):** Evaluates a boolean expression; if false, it triggers the error and continues/raises.

### 2. Periodic Execution (Modular Execution)
In high-performance loops, logging every single event can ruin performance. The `MOD` macros allow you to execute code (like logging or heavy diagnostics) only once every `N` iterations.

| Macro | Description |
| :--- | :--- |
| `MODEXEC(mod, action)` | Executes `action` only once every `mod` iterations. Returns `true` if the action was executed. |
| `IFMOD(mod)` | Returns `true` every `mod` iterations. Useful for conditional logic: `if (IFMOD(100)) { ... }` |
| `MODEXECL(mod, action)` | Same as `MODEXEC`, but works with `long` counters for extremely large loops. |

### 3. Singleton Execution
| Macro | Description |
| :--- | :--- |
| `SINGLETON(action)` | Ensures that the `action` is executed **exactly once**, regardless of how many times the macro is called. |

---

## Usage Example

```c
#include "guard.h"
#include "error.h"
#include <stdio.h>

void complex_simulation() {
    for (int i = 0; i < 1000000; i++) {
        
        // 1. Periodic Logging: Only log every 10,000 iterations
        // Prevents flooding the console while still providing progress.
        if (IFMOD(10000)) {
            printf("Current iteration: %d\n", i);
        }

        // 2. Threshold Guard: Detect if a mathematical instability occurs 
        // repeatedly, even if it doesn't crash the program immediately.
        double instability = check_math_stability();
        FGUARDM(instability < 0.0001); 

        // 3. Single-run Debugging: Log an event only the very first time it happens
        SINGLETON({
            printf("System entered critical state zone!\n");
        });
    }
}
```

## Summary Table of Guard Types

| Type | Purpose | Best used for... |
| :--- | :--- | :--- |
| **Standard Guard** | Silent threshold check | Detecting logical drift in loops. |
| **Raising Guard** | Immediate crash on threshold | Hard-stopping the dev environment when a bug is confirmed. |
| **Functional Guard** | Logic-dependent threshold | Checking if a specific boolean condition fails repeatedly. |

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       