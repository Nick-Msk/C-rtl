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

## AI-Assisted Reviews

The `ai-checker/` tool and the `make ai-check*` targets produce
machine-generated code reviews. They are provided **as-is** and
carry no warranty of correctness, completeness, or fitness for any
purpose.

By using these targets you acknowledge that:

- **Reports are not engineering advice.** They are suggestions
  produced by a statistical language model and may contain
  factual errors, invented APIs, or recommendations that are
  actively wrong for this codebase.
- **No finding is authoritative until a human verifies it.** Treat
  every `## Критично` or `## Важно` item as a hypothesis to be
  checked against the source, not as a defect report.
- **The tool sends source code over HTTP** to the LM Studio
  endpoint configured via `LMSTUDIO_URL`. The default is
  `http://127.0.0.1:1234`, i.e. the same machine. If you point it
  at a remote host, you are responsible for the confidentiality
  of the code being transmitted.
- **Reports are retained** under `build/ai-checker/` and survive
  `make clean`. Archive or delete them according to your own data
  retention policy.
- **No automated decisions** should be made on the basis of a
  report without human review. This includes, but is not limited
  to, merging code, closing issues, or blocking releases.

The maintainers of `ngutils` do not accept any liability for
consequences arising from the use of AI-generated reviews.

### Auto-Applied Fixes

The `make ai-fix*` targets write to source files and create git
commits. Additional terms apply:

- **Only self-contained, single-function, ≤20-line edits are
  applied.** Everything that would ripple — parameter changes,
  signature changes, new helpers, struct changes — is listed for
  human review. This is a *safety heuristic*, not a correctness
  guarantee: a 20-line edit can still be wrong.
- **A clean working tree is required.** The fixer refuses to run on a
  dirty tree, so that every commit contains exactly one fix and
  nothing else. Do not bypass this by stashing mid-run.
- **Review the branch before merging.** Fixes land on
  `ai-checker-<label>-fix<N>`, not on your working branch. Inspect
  with `git log`, `git diff`, `git show <sha>`, then cherry-pick
  selectively. Deleting the branch discards all of its commits.
- **Always run `make test-<proj>`** after cherry-picking. A fix that
  compiles can still break behaviour; tests are the only check the
  tool cannot do for you.
- **The maintainers accept no liability** for auto-applied changes.
  Review, test, commit responsibly.

