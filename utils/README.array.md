A high-performance, polymorphic array utility library for C.

array.h
 provides a flexible and type-safe way to manage dynamic arrays in C. It is designed to handle primitive types (int, long, double, char, void*) and a custom value64 container through a unified interface, using an optimized memory layout.

🚀 Key Features
Polymorphic Storage: Uses a union inside the Array structure to store different data types without the overhead of heavy object-oriented abstractions.
Type Safety: Built-in type checking and metadata (ArrayTypeInfo) to prevent invalid memory access.
Rich Initialization: Supports multiple filling strategies:
ZERO (initialized to zero)
RND (random values)
ASC / DESC (ascending/descending sequences)
ASC_SERIES / DESC_SERIES (for numeric steps)
SAFE_EMPTY (for string/pointer safety)

Advanced Iteration: Supports foreach loops and generator-based filling (v64Gen).
Slicing Support: Includes ArraySlice for creating lightweight views of existing arrays (work in progress).

Built-in Algorithms:
Sorting: arrayQsort (QuickSort)
Searching: Optimized binary search for all supported types (arrayBsearch).
Transformation: arrayShuffle (Fisher-Yates algorithm).
Serialization: Easy saving/loading via files, custom fs (fast-string) formats, and DS (Data Structures) containers.

🛠 Supported Types
The library natively supports: 
| Type | Internal Representation | Description | 
| :--- | :--- | :--- | 
| ARRAY_INT | int* | Standard integer | 
| ARRAY_LONG | long* | Standard long integer | 
| ARRAY_DOUBLE | double* | Floating point | 
| ARRAY_CHAR | char* | Character array (not null-terminated array) | 
| ARRAY_POINTER | void** | Array of pointers | 
| ARRAY_V64 | value64* | High-level polymorphic container |

📦 Installation
Simply include 
array.h
 in your project and ensure that the dependent modules (common.h, log.h, value64.h, etc.) are available in your include path.


Apply
#include "array.h"
📖 Usage Examples
1. Creating and Filling an Array

Apply
// Create an integer array with 10 elements, filled with random values
Array *my_arr = IarrayCreate(10, ARRAY_FILLTYPE_RND);

// Create a double array with a specific sequence
Array *seq_arr = DarrayCreate(5, ARRAY_FILLTYPE_ASC_SERIES);

// Freeing the array
arrayFree(my_arr);
2. Using Convenience Macros (for testing/small fixtures)

Apply
// Quick creation using compound literals (convenience macros)
Array *quick_arr = IARRAY_CREATE(10, 20, 30, 40, 50);

// Iterate through the array
Array_pforeach_idx(quick_arr, i) {
    printf("Element at index %zu: %d\n", i, quick_arr->iv[i]);
}
3. Searching and Sorting

Apply
// Sort an integer array in descending order
arrayQsort(my_arr, ARRAY_SORTTYPE_DESC);

// Binary search for a value
long index = arrayBsearchInt(my_arr, 42);
if (index >= 0) {
    printf("Found 42 at index %ld\n", index);
}
⚙️ Complexity Analysis
| Operation | Time Complexity | Note | | :--- | :--- | :--- | | Access (by index) | $O(1)$ | Direct pointer arithmetic | | Search (Binary) | $O(\log n)$ | Requires sorted array | | Sort (QuickSort) | $O(n \log n)$ | | | Add / Delete | $O(n)$ | Due to element shifting | | Shuffle | $O(n)$ | Fisher-Yates |

⚠️ Constraints & Warnings
Memory Management: Always use arrayFree() to release memory. Using arrayFreeBody() is intended only for stack-allocated Array descriptors.
Index Bounds: Most functions clamp or check indices, but passing invalid pointers will lead to Undefined Behavior.
Slicing: ArraySlice is currently in development and should be used with caution.
