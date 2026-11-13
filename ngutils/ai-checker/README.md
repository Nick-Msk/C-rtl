# ai-checker

AI-assisted code review for `ngutils` projects. Talks to a locally
running LM Studio instance over the OpenAI-compatible HTTP API and
writes Markdown reports to `build/ai-checker/`.

The model is expected to be a Qwen-family LLM, but any chat model
served by LM Studio will do — the target name is historical.

## Requirements

- Python >= 3.10
- [`uv`](https://docs.astral.sh/uv/) on `PATH`
- LM Studio running locally with at least one chat model loaded
  (default endpoint: `http://127.0.0.1:1234`)

`uv` creates a throwaway venv from `pyproject.toml` and installs
`requests` on first run; no manual setup is needed.

## Quick start

```sh
make ai-models              # what models does LM Studio have loaded?
make ai-check               # review every discovered project
make ai-check-fs2           # review a single project
make ai-check-notest-fs2    # same, but skip test/ directories
make ai-check-file F=log/src/log.c
make ai-checker-arch        # pack build/ai-checker/ into a .tar.zst
```

The dispatcher passes `LMSTUDIO_URL` and `QWEN_MODEL` through to the
script; both can be overridden per invocation:

```sh
make ai-check LMSTUDIO_URL=http://192.168.1.10:1234
make ai-check QWEN_MODEL=qwen/qwen3-8b
```

## Targets

All targets are defined in the top-level `Makefile`.

| Target                       | What it does                                              |
|------------------------------|-----------------------------------------------------------|
| `make ai-models`             | Print models currently loaded in LM Studio.               |
| `make ai-check`              | Review all discovered projects (include `test/`).         |
| `make ai-check-<proj>`       | Review one project, e.g. `ai-check-log`.                  |
| `make ai-check-notest`       | Review all projects, exclude `test/` directories.         |
| `make ai-check-notest-<p>`   | Review one project, exclude `test/`.                      |
| `make ai-check-file F=<p>`   | Review a single source file.                              |
| `make ai-checker-arch`       | Tar+zstd `build/ai-checker/` into `build/ai-checker-reports.tar.zst`. |

Reports are intentionally preserved by `make clean`.

## Reports layout

```
build/ai-checker/
├── .lock                                 # single-instance lock (flock)
├── _machine.json                         # system snapshot of this run
├── _files/
│   └── 2024-11-12_15-04-11_log.md        # ai-check-file output
├── log/
│   ├── 2024-11-12_14-23-45.md            # full review
│   └── 2024-11-12_15-04-11_notest.md     # test-less review
└── fs2/
    └── ...
```

Each report opens with a metadata table:

```markdown
# AI review: fs2

| field    | value |
|----------|-------|
| started  | 2024-11-12 15:01:02 |
| duration | 2m 34s  (per chunk: 1m 17s, 1m 17s) |
| model    | `qwen/qwen3-8b` |
| endpoint | `http://127.0.0.1:1234` |
| tokens   | 24,320 prompt / 3,935 completion / 28,255 total (2 req) |
| speed    | 25.6 completion tok/s |
| host     | MacBookPro18,3 |
| os       | macOS 14.6.1 (arm64) |
| cpu      | Apple M1 Pro (8p/10l) |
| ram      | 32.0 GB |
| python   | 3.12.4 |
```

followed by the model's answer (`## Критично`, `## Важно`,
`## Мелочи`, `## Что хорошо`).

## Configuration

| Variable        | Default                    | Notes                               |
|-----------------|----------------------------|-------------------------------------|
| `LMSTUDIO_URL`  | `http://127.0.0.1:1234`    | Base URL (no `/v1` suffix).         |
| `QWEN_MODEL`    | *(empty)*                  | Empty = first loaded model whose id contains `qwen`. |

Both variables are read by the script and can be set either in the
environment or on the `make` command line.

## Architecture

- `check.py` is a single-file script. On startup it queries
  `/v1/models`, picks a model, then for each project:
  1. collects `include/`, `src/` (and optionally `test/`) source
     files;
  2. packs them into chunks of at most 40 000 characters, one prompt
     per chunk;
  3. streams the response via `/v1/chat/completions` and writes the
     accumulated Markdown to `build/ai-checker/<project>/<ts>.md`.
- Reports are written once at the end of each project so a Ctrl+C
  during a chunk does not corrupt the file already on disk.
- `flock(2)` on `build/ai-checker/.lock` guarantees a single running
  instance. The lock is released automatically when the process
  exits — including `SIGKILL` — so there are no stale locks to clean
  up by hand.
- Token accounting is read from the `usage` field returned by LM
  Studio. With `stream_options.include_usage` enabled it arrives in
  the last SSE event before `[DONE]`; if the server ignores the flag,
  the report simply says *"not reported by server"*.

## Type digest

By default, `check.py` does **not** send header files verbatim. Full
headers in this repository can exceed 200 KB (e.g. `fs2_api.h` is
91 KB, `fs2.h` is 60 KB) — duplicating them per chunk would waste
the entire context window.

Instead, only **type declarations** (`struct`, `union`, `enum`,
`typedef`) are extracted from all headers of the project and
prepended to **every** chunk as a synthetic file `_types.h`. The
digest is also written to `build/ai-checker/<project>/_types.h` so
you can inspect what the model actually sees.

This guarantees the model always has access to type definitions —
even in chunk 2, 3, ... — without paying for full headers.

Function prototypes, macros, variable declarations, and inline
function bodies are **not** included in the digest. If you want full
headers, pass `--full-headers`:

```sh
make ai-check-fs2 AI_CHECK_EXTRA="--full-headers"
```
Types are extracted from headers and source files, so both public API typedefs and private struct definitions (e.g. struct fs2 with field borrow) are available to the model in every chunk. This eliminates a whole class of "I cannot see the header, but if the type were X..." findings.

## Limitations

- **Not a replacement for `make check`, `make analyze`, or tests.**
  Use it as a second pair of eyes, not as a gate.
- **LLMs hallucinate.** Findings must be verified against the source
  before any action.
- **No incremental mode.** Every invocation reviews the whole project
  (or file). A `.git` diff mode is not implemented.
- **Prompt length is capped** at 40 000 characters per chunk. Very
  large projects will be split, which means cross-chunk issues may
  be missed.
- **LM Studio only.** The script uses `POST /v1/chat/completions`
  with `stream_options.include_usage`; other OpenAI-compatible
  servers work if they implement that flag, otherwise token usage
  will show as *"not reported"*.

## AI fixer

`fix.py` reads the latest report produced by `check.py` and applies
only *self-contained local* fixes as **one git commit per fix** on a
dedicated branch. Everything that would ripple through the codebase
— parameter changes, signature changes, new helpers, struct layout,
cross-file edits — is listed for the reviewer with a suggested
direction and **never** auto-applied.

### Usage

```sh
make ai-fix-fs2                    # dry-run: show proposed edits
make ai-fix-fs2 APPLY=1            # apply + per-fix git commit
make ai-fix-fs2 APPLY=1 YES=1      # apply all, no prompts
make ai-fix-file F=log/src/log.c
make ai-fix-fs2 FIX_LIMIT=5        # cap findings per project
```

`APPLY=1` requires a clean git working tree. The fixer refuses to run
otherwise, so that each commit contains exactly one fix and nothing
else.

### What "local" means

A fix is applied only if **all** of these hold:

1. One file, one function body (or one `#include` line).
2. **No function signature change** — adding/removing a parameter is
   forbidden because every caller would need updating.
3. No new function, struct, union, enum, or global variable.
4. No struct/union/enum layout, field type, or order change.
5. No public symbol rename or removal.
6. No new `#include` (typo fixes in existing includes are OK).
7. No changes required in any other file.
8. Idempotent.
9. Diff ≤ 20 lines (additions + deletions combined).

If any of these fail — or the model is not sure — the finding goes
into the *needs human thought* section.

### Git workflow

When `--apply` is used, the fixer:

1. Verifies the working tree is clean.
2. Processes findings one by one. For the first applied fix of each
   project it creates `ai-checker-<label>-fix<N>` from the current
   HEAD (N is auto-incremented if the branch exists).
3. For each applied fix: writes the file, `git add`s it, and creates
   a commit with a self-explanatory message (section, file, reason,
   finding text, `Applied by ai-checker/fix.py` trailer).
4. Writes a JSON log to `build/ai-checker/fixes/<ts>_<label>.json`.
5. Returns to the original branch at the end.

Reviewing what was produced:

```sh
git log --oneline master..ai-checker-fs2-fix1
git diff master..ai-checker-fs2-fix1
git show <sha>                          # a single fix
```

Keeping only some of the fixes:

```sh
git checkout master
git cherry-pick <sha1> <sha3>           # keep commits 1 and 3
git branch -D ai-checker-fs2-fix1       # drop the rest
```

Dropping everything:

```sh
git branch -D ai-checker-fs2-fix1
```

### Safety rails

- **Dry-run by default.** `--apply` is required to write to disk.
- **Clean tree required.** Untracked or modified files abort the run
  before any fix is committed.
- **`old_snippet` must appear exactly once** in the target file.
- **Forbidden paths.** Only C/C++ sources, headers, and tests inside
  a project directory. `build/`, `.github/`, `scripts/`, `docs/`,
  `skeleton/`, `bkp/`, `ai-checker/`, `Makefile*`, `Doxyfile`,
  `Dockerfile*`, `*.toml`, `*.md` are rejected.
- **Same `flock(2)`** as `check.py` — `ai-check` and `ai-fix` cannot
  run simultaneously.
- **Rollback on commit failure.** If `git commit` fails (e.g. a
  pre-commit hook), the file edit is reverted so the tree stays clean.

### After applying

1. `git log --oneline <branch>` — see all fixes.
2. `git show <sha>` — inspect a single fix.
3. `git cherry-pick <sha>` — keep the good ones on your working branch.
4. `make test-<proj>` — verify.
5. `git branch -D <branch>` — discard the rest.

## See also

- `Makefile` — target definitions (`ai-check*`, `ai-checker-arch`).
- `DISCLAIMER.md` — repository-wide disclaimer; the AI-specific
  section there applies to every report produced by this tool.
- `CHANGELOG.md` — release history.

