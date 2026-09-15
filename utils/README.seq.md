# Sequence Management Module (`seq.h`)

> [!NOTE]
> `seq.h` provides a lightweight mechanism for managing multiple independent integer sequences. It is designed for scenarios where you need many separate counters (e.g., unique IDs, packet counters, or session IDs) without manually managing a collection of variables.

## Overview

The module manages a fixed pool of sequences. Each sequence is identified by a `seqnum_t` (an index) and holds a `seqv_t` (an `int64_t` value). The module handles the allocation and reuse of these indices, allowing you to "drop" a sequence and reuse its slot for a new one later.

## Key Concepts

### Data Types
* **`seqv_t` (Sequence Value):** An `int64_t` representing the actual current value of the sequence.
* **`seqnum_t` (Sequence Number):** An `int` acting as a handle (ID/index) for a specific sequence. You use this ID to interact with the sequence.

### Lifecycle & Management
* **Allocation:** When you initialize a sequence, the module finds an available slot in the internal pool and returns a unique ID.
* **Deallocation (`dropseq`):** Releases the ID back to the pool, making it available for future `initseq` calls.
* **Reuse:** The module automatically reuses indices from dropped sequences to prevent exhaustion of the `SEQ_MAXCOUNT` limit.

---

## API Reference

### Lifecycle Management
| Function | Description |
| :--- | :--- |
| `initseq()` | Initializes a new sequence and returns its unique `seqnum_t`. |
| `dropseq(s)` | Frees the sequence with ID `s`, making the ID available for reuse. |
| `resetseq()` | Resets the entire sequence engine, clearing all sequences. |

### Access & Mutation
| Function | Description |
| :--- | :--- |
| `currval(s)` | Returns the current value of the sequence with ID `s`. |
| `nextval(s)` | Increments the value of sequence `s` and returns the new value. |

### Debugging
| Function | Description |
| :--- | :--- |
| `seq_techprint()`| Prints technical diagnostic information about the current state of all sequences in the pool. |

---

## Usage Example

```c
#include "seq.h"
#include <stdio.h>

int main() {
    // 1. Initialize a new sequence
    seqnum_t my_id = initseq();
    printf("New sequence ID: %d, Initial Value: %lld\n", my_id, (long long)currval(my_id));

    // 2. Increment the sequence
    printf("Next value: %lld\n", (long long)nextval(my_id));
    printf("Current value: %lld\n", (long long)currval(my_id));

    // 3. Drop the sequence to free up the slot
    dropseq(my_id);

    // 4. Create another sequence (might reuse the same ID)
    seqnum_t new_id = initseq();
    printf("New sequence ID (reused?): %d\n", new_id);

    return 0;
}
```

## Implementation Details

* **Fixed Capacity:** The module uses a pre-allocated pool of `SEQ_MAXCOUNT` (default: 128) sequences. 
* **Error Handling:** If the pool is full and no slots are available via `dropseq`, `initseq` will trigger a system error (`ERR_UNABLE_ALLOCATE_SEQ`).
* **Thread Safety:** **Not thread-safe.** This module uses static global state and is intended for single-threaded use or execution within a single thread context.

## Complexity
* **Initialization:** $O(N)$ where $N$ is `SEQ_MAXCOUNT` (to find an empty slot).
* **Access/Mutation:** $O(1)$.
* **Drop/Release:** $O(1)$.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       