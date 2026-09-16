# Value64 Params

> [!WARNING]  
> **NOT RECOMMENDED for general-purpose use.**  
> This module is a specialized, fixed-size container designed specifically for passing a limited set of arguments to predicates or map functions.  
> 
> **If you need a dynamic or larger collection of values, use [value64_tarray.h](value64_tarray.h) instead.**

`value64_params` provides a lightweight way to bundle up to 4 `value64` objects into a single structure. This is useful for passing multiple criteria to filter functions or map operations where the number of arguments is known at compile-time and is very small.

## 🏗 Architecture

The module provides a fixed-size container `value64_params_t`. Depending on your build configuration, it behaves in two ways:

### 1. Standard Mode (Default)
When `VALUE64_PARAMS_FREE` is **not** defined, the container is a simple collection of `value64` values. It is extremely fast and intended for primitive values (integers, doubles, etc.) that do not require memory cleanup.

### 2. Resource-Aware Mode (Memory Management)
If `VALUE64_PARAMS_FREE` is defined during compilation, the container gains:
*   An array of `value64_type` tags.
*   A `count` field.
*   The `free_value64_params()` function to safely release heap-allocated resources (like `STR` or `FS`) held within the container.

## 🚀 Usage Guide

### Initialization

Use the provided macros to create parameter containers.

#### Standard Initialization (Primitives)
```c
// Creates a container with 2 primitive parameters
value64_params_t params = VALUE64_PARAMS2(LITERAL64_INT(10), LITERAL64_INT(20));
```

#### Resource-Aware Initialization (Requires `VALUE64_PARAMS_FREE`)
If you are passing strings or filesystem objects, you **must** specify the types so they can be freed later.

```c
#ifdef VALUE64_PARAMS_FREE
value64_params_t params = VALUE64_PARAMS2(
    LITERAL64_STR("Hello"), VALUE64_INT, 
    LITERAL64_FS(my_fs),      VALUE64_FS
);

// Later, to prevent memory leaks:
free_value64_params(&params);
#endif
```

### Accessing Parameters

You can access parameters using indexed access or convenience getters.

```c
// Using the index-based getter (Safe: returns LITERAL64_ZERO if out of bounds)
value64 v = value64_getpar(&params, 1);

// Using convenience getters for speed
value64 v1 = value64_getpar1(&params);
value64 v2 = value64_getpar2(&params);
```

## 📋 API Reference

| Function/Macro | Description |
| :--- | :--- |
| `VALUE64_PARAMS1..4` | Macros to initialize 1, 2, 3, or 4 parameters. |
| `value64_getpar(p, i)` | Returns the $i$-th parameter (safe). |
| `value64_getpar1(p)` | Returns the 1st parameter. |
| `value64_getpar2(p)` | Returns the 2nd parameter. |
| `value64_getpar3(p)` | Returns the 3rd parameter. |
| `value64_getpar4(p)` | Returns the 4th parameter. |
| `free_value64_params(p)` | *(Available only if `VALUE64_PARAMS_FREE` is defined)* Cleans up resources. |

## ⚠️ Summary of Constraints

1.  **Fixed Capacity:** Maximum of **4** parameters.
2.  **Manual Cleanup:** If you use dynamic types (`STR`, `FS`) and have `VALUE64_PARAMS_FREE` enabled, you **must** call `free_value64_params`.
3.  **Alternative:** For any scenario involving more than 4 elements or dynamic growth, use **`value64_tarray.h`**.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
