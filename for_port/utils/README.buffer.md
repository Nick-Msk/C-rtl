# Legacy Character Buffer (`buffer.h`)

> [!CAUTION]
> ### ⚠️ LEGACY / DEPRECATED
> This module is part of the **legacy `4gl` example code** (`old_work`). It is no longer used in the main project and is kept only for historical context and backward compatibility with older versions of the parser.

## Purpose

This is a low-level character buffer utility. It was designed specifically as a helper for the `getword.h` tokenizer.

### The Problem
The standard C library function `ungetc()` only allows pushing **one single character** back into the input stream. In complex lexical analysis (like the one used in `getword.h`), a parser sometimes needs to "rewind" multiple characters at once when it realizes a token has ended prematurely.

### The Solution
This module provides an extended buffer capability:
* **`ungetch(int)`**: Pushes a single character back into the buffer.
* **`ungets(const char *s)`**: Pushes a **full string** back into the buffer, allowing the parser to backtrack over multiple characters seamlessly.

## API Reference

| Function | Description |
| :--- | :--- |
| `getch()` | Reads the next character from the buffer. |
| `ungetch(int)` | Pushes a single character back into the buffer. |
| `ungets(const char *s)` | Pushes a sequence of characters back into the buffer. |

## Context
* **Location:** `kr_book/simple/old_work/4gl/exapmle.4.3/src/`
* **Primary Dependency:** Used by the deprecated `getword.h`.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       