# Value64 Typed

`v64typed` is the primary, recommended interface for the **Value64** ecosystem. 

While the base `value64` union is a high-performance, "type-blind" 64-bit container, `v64typed` provides a type-safe wrapper that bundles the data with its type metadata. This ensures that values are always accompanied by their descriptors, enabling safe access, automated memory management, and type-aware arithmetic.

## 🌟 Why use `v64typed`?

The raw `value64` union is optimized for memory footprint, but it carries the risk of "type mismatch" errors if the developer loses track of the data's type. `v64typed` solves this by:
1.  **Binding Type to Data:** The type is inseparable from the value.
2.  **Automated Safety:** Getters include runtime checks to prevent interpreting integer bits as pointers.
3.  **Memory Ownership:** Simplifies management of heap-allocated types (`STR`, `FS`) through unified constructors and destructors.

## 🏗 Core Structure

The core of this module is the `v64typed` structure:

```c
typedef struct {
    value64      val;   // The 64-bit data
    value64_type typ;   // The explicit type (e.h. VALUE64_INT, VALUE64_STR, etc.)
} v64typed;
```

## 📖 Usage Guide

### 1. Initialization
You can initialize `v64typed` objects using high-speed macros or flexible constructor functions.

#### Using Macros (Fastest)
```c
// For primitives
v64typed my_int    = V64TYPEDINT(100);
v64typed my_double = V64TYPEDDBL(3.14);
v64typed my_bool   = V64TYPEDBOOL(true);

// For strings (creates a deep copy)
v64typed my_str    = V64TYPEDSTR("Hello World");
```

#### Using Constructor Functions (Flexible)
```c
// Creating a generic typed object
v64typed generic = v64typedCreate(LITERAL64_INT(50), VALUE64_INT);

// Creating a managed string (heap allocation)
v64typed str = v64typedCreateStr("Managed string");

// Creating a filesystem resource
v64typed fs = v64typedCreateFs(my_fs_ptr);
```

### 2. Memory Management
Managing lifecycle is critical for resource-owning types (`STR`, `FS`).

```c
v64typed my_str = v64typedCreateStr("Important data");

// ... use the object ...

// Safely free the internal resources and reset the object
v64typedFree(&my_str);
```

#### Move Semantics
To transfer ownership of a resource from one object to another without copying the underlying data, use `v64typedMove`. The source object is reset to `UNKNOWN` to prevent double-frees.

```c
v64typed source = v64typedCreateStr("Transfer me");
v64typed target = v64typedMove(&source); 
// 'source' is now UNKNOWN. 'target' now owns the string memory.
```

### 3. Accessing Data
Accessing data is done through type-specific getters. These functions perform safety checks and will raise an error if the requested type does not match the stored type.

```c
v64typed my_val = V64TYPEDINT(42);

// Direct access
int i = v64typeGetInt(my_val);
double d = v64typeGetDouble(my_val);
char* s = v64typeGetStr(my_val);

// Casting (Converting)
// Converts a typed object to a primitive C type
int casted_i = v64typedCastToInt(my_val);
```

### 4. Advanced Utilities

#### Null Value Handling (NVL)
Useful for providing fallback values when dealing with strings or filesystem resources.

```c
v64typed my_str = V64TYPEDSTR(""); // Empty string

// Returns the string if not empty, otherwise returns "Default"
const char* result = v64typedNvlStr(my_str, "Default");
```

#### Arithmetic and Logic
The module allows for direct manipulation of the values within the typed container.

```c
v64typed counter = V64TYPEDINT(10);

// Increments the value within the container
v64typedAdd(&counter, 5); // counter is now 15

// Flips a boolean
v64typedBoolNegative(&counter); 
```

## 🛠 API Summary

| Category | Functions/Macros |
| :--- | :--- |
| **Initialization** | `V64TYPED...` (Macros), `v64typedCreate...` (Functions) |
| **Memory** | `v64typedClone`, `v64typedMove`, `v64typedFree` |
| **Accessors** | `v64typeGet...` (int, long, double, str, fs, etc.) |
| **Casting** | `v64typedCastToInt`, `v64typedCastToDouble`, etc. |
| **Utilities** | `v64typedNvlStr`, `v64typedNvlFs`, `v64typedAdd` |

## ⚠️ Safety Warning
Always ensure that you call `v64typedFree` on any `v64typed` object that was created using a "Create" function (like `v64typedCreateStr` or `v64typedCreateFs`) to avoid memory leaks. Use `v64typed_move` when you want to transfer ownership of the internal pointer to another object.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
