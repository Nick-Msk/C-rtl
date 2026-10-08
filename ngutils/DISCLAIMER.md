## Disclaimer

This is a personal utility collection, not a polished library.

- **No API stability guarantee before 1.0.** Utilities are versioned
  individually and may change between minor releases. Pin to a tag if
  you depend on a specific behavior.
- **No warranty.** Provided "as is", without any express or implied
  warranty. See [LICENSE](LICENSE) for details.
- **Known gaps.** Some functions are marked `@deprecated` or carry
  known bugs (`log_numbers` off-by-one, `log_modload` parsing).
  Refer to each utility's documentation before relying on them.
- **Not thread-safe.** `log` and similar utilities assume a single
  writer. Wrap access in your own synchronization if needed.
- **Tested on a small set of platforms.** macOS (arm64) and Linux
  (Ubuntu, `x86_64`/arm64) via CI. Other platforms may work but are
  untested.
- **Not intended for safety-critical systems.** No formal verification,
  no MISRA/CERT compliance claims, no static analysis reports beyond
  what you see in CI logs.

