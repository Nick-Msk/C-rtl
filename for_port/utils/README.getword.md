# Tokenizer & String Parsing Utility (`getword.h`)

> [!WARNING]
> ### ⚠️ DEPRECATED
> **This module is deprecated.** 
> The functionality provided here is being phased out. For all new development, please use the new **Data Source API** located in **`ds.h`**.
> 
> This header is maintained only for backward compatibility with legacy code.

## Overview

`getword.h` provides a lightweight lexical analyzer and string parsing utilities. It is designed to transform raw input streams (files, strings, or custom data sources) into structured tokens called `Lexem`.

The utility handles complex parsing tasks such as:
* Distinguishing between integers, floats, and words.
* Handling quoted strings.
* Processing escape sequences (e.g., `\n`, `\t`, `\\`).
* Managing different input sources (`FILE*`, `const char*`, `DS*`).

## Data Structures

### `Lexem`
The fundamental unit of parsing.
- `str`: A `fs` (fast-string) containing the raw content of the token.
- `typ`: A `Lexemtype` identifying the nature of the token.

### `Lexemtype` (Enum)
Defines the category of the parsed token:
- `LEXEM_WORD`: Standard text/identifier.
- `LEXEM_INT`: Integer values.
- `LEXEM_FLOAT`: Floating-point values.
- `LEXEM_SYM`: Symbols/Operators.
- `LEXEM_CMD`: Commands (starting with `<`).
- `LEXEM_STR`: Explicit string tokens.
- `LEXEM_UNK`: Unknown/Error state.

## API Reference

### Lexical Analysis
| Function | Description |
| :--- | :--- |
| `getword(fs, ...)` | The primary entry point for reading a single word from a fast-string. |
| `getlexem(Lexem*, bool)` | Extracts the next token from the current stream and identifies its type. |
| `getstring(Lexem*)` | Specifically parses a string or a command (`LEXEM_STR` or `LEXEM_CMD`). |
| `lexem_eq(...)` | Compares a lexem with a string for equality. |

### String Parsing (Conversion)
These functions handle quoted text and escape sequences.
| Function | Description |
| :--- | :--- |
| `getconvstring(FILE*, fs*, bool)` | Parses a quoted/unquoted token from a **file**. |
| `getconvstring_cs(const char*, fs*, bool)` | Parses a quoted/unquoted token from a **C-string**. |
| `getconvstring_ds(DS*, fs*, bool)` | Parses a quoted/unquoted token from a **Data Source**. |

### Low-Level Utilities
| Function | Description |
| :--- | :--- |
| `getstring_nl(...)` | Reads a string from a file, with options for newlines and appending. |
| `getpurestring(...)` | A simple wrapper for reading a line without newline characters. |

## Usage Example

**Note: Use `ds.h` instead of this for new projects.**

```c
#include "getword.h"
#include <stdio.h>

void parse_input(FILE *fp) {
    Lexem token;
    
    // Initialize and attempt to get a lexem
    if (getlexem(&token, true)) {
        printf("Token Type: %s\n", Lexemtype_str(token.typ));
        printf("Token Value: %s\n", lexem_str(&token));
        
        // Clean up the lexem
        lexem_free(&token);
    }
}

int main() {
    FILE *file = fopen("input.txt", "r");
    if (file) {
        parse_input(file);
        fclose(file);
    }
    return 0;
}
```

## Dependencies
- `fs.h` (Fast-string implementation)
- `ds.h` (Data source management)
- `bool.h`, `checker.h`, `log.h`
- `stddef.h`, `stdio.h`, `string.h`

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       