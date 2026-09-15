# System Utilities (`systemutils.h`)

> [!NOTE]
> ### 🚧 Work in Progress
> This module is in its **initial development stage**. The current functionality is minimal and is expected to expand significantly as the project evolves.

## Overview

`systemutils.h` provides low-level utilities for interacting with the system environment and managing standard I/O streams. It is designed to provide fine-grained control over how the application communicates with the terminal/console.

## Current Functionality

The module currently focuses on the management of standard output and error streams (`stdout` and `stderr`).

### API Reference

| Function | Description |
| :--- | :--- |
| `su_stddisable()` | Disables standard output/error streams (redirects/suppresses output). Returns `true` if successful. |
| `su_stdenable()` | Re-enables standard output/error streams. |
| `su_reset()` | Resets the system utility state to its default configuration. |

## Roadmap

* [ ] Enhanced signal handling utilities.
* [ ] Process environment management.
* [ ] Advanced terminal control.
* [ ] System resource monitoring.

## Dependencies
- `stdbool.h`

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       