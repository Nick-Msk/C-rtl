# Metrics & Instrumentation Module (`metric.h`)

> [!NOTE]
> `metric.h` is a lightweight telemetry module designed to track performance metrics, operation counts, and algorithmic complexity during execution.

## Overview

The Metrics module provides a centralized way to instrument code. Instead of using manual counters, you can define named `Metric` objects. This is particularly useful for:
* **Complexity Analysis:** Counting comparisons, swaps, or recursive calls to verify Big-O complexity.
* **Performance Profiling:** Tracking the frequency of specific code paths.
* **Telemetry:** Collecting statistics on system operations (e.g., "files_processed", "network_packets").

## Data Structures

### `Metric`
A named counter or accumulator.
- `name`: A unique identifier for the metric.
- `value`: The current value (supports `int` and `double` through a union).

---

## API Reference

### Lifecycle Management
| Function | Description |
| :--- | :--- |
| `metric_create(name)` | Creates and initializes a new metric with `value = 0`. |
| `metric_acq(name)` | "Acquires" an existing metric. Returns `NULL` if the metric does not exist. |
| `metric_reset()` | Resets the entire metric registry (resets all counters to zero). |

### Manipulation (Counters)
| Function | Description |
| :--- | :--- |
| `metric_inc(m)` | Increments the metric value by 1. |
| `metric_add(m, val)` | Increments the metric value by a specified integer `val`. |
| `metric_setval(m, val)` | Sets the metric to a specific integer value. |
| `metric_getval(m)` | Returns the current integer value of the metric. |

### Observation & Debugging
| Function | Description |
| :--- | :--- |
| `metric_print(m)` | Prints the name and current value of a metric to `stdout`. |
| `metric_fprint(f, m)` | Prints the metric to a specific file stream. |

---

## Complexity & Limits

* **Search Complexity:** $O(N)$ where $N$ is the number of active metrics (due to linear search by name).
* **Storage:** Uses a fixed-size global array (`MAX_METRIC = 100`) to ensure zero dynamic memory overhead during critical paths.
* **Type Safety:** Currently optimized for `int` values.

---

## Usage Example

This example demonstrates how to use metrics to measure the complexity of a simple algorithm.

```c
#include "metric.h"
#include <stdio.h>

// A function we want to profile
void bubble_sort(int *arr, int n) {
    Metric *iter_count = metric_create("iterations");
    Metric *swap_count = metric_create("swaps");

    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            // Track every comparison/iteration
            metric_inc(iter_count);

            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
                
                // Track swaps
                metric_inc(swap_count);
            }
        }
    }

    printf("Sort completed.\n");
    metric_print(iter_count); // Output: [iterations] = [28]
    metric_print(swap_count); // Output: [swaps] = [10]
}

int main() {
    int data[] = {64, 34, 25, 12, 22, 11, 90, 5, 1, 100};
    int n = sizeof(data) / sizeof(data[0]);

    bubble_sort(data, n);

    return 0;
}
```

## Implementation Notes

> [!IMPORTANT]
> This module is currently optimized for **integer** counters. While the `Metric` structure supports `double`, the current `metric_getval` and `metric_setval` implementations focus on `int`. For floating-point profiling, future updates will expand the API.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       