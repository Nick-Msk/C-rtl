# Changelog — ai-checker

All notable changes to the `ai-checker` tool. Format:
[Keep a Changelog](https://keepachangelog.com/). Versioning of this
file is independent of `ngutils`.

## [Unreleased]

### Added

- `check.py` — AI code review pipeline. Sends project sources to a
  local LM Studio instance (OpenAI-compatible API), streams the
  response, writes Markdown reports to `build/ai-checker/<proj>/`,
  and ingests findings into `config/check.db`. Large projects are
  chunked; a `_types.h` digest of all `struct`/`union`/`enum`/
  `typedef` declarations (from headers **and** sources) is
  prepended to every chunk so the model always sees the types it
  needs — even types defined in `.c` files.
- `fix.py` — AI code fixer. Reads the latest check report, asks the
  LLM to classify each finding as *local* (safe in-place fix) or
  *needs_review* (requires human thought). Applies only the *local*
  ones; everything else is listed for review with a suggested
  direction, never auto-applied.
- Local-fix validation:
  - at most 2 files; if 2, they must be a paired source/header
    with the same stem in the same directory (`.c`/`.h`,
    `.cpp`/`.hpp`, `.cc`/`.hpp`);
  - edits confined to one function body per file (or one
    `#include` line, one macro, one comment block attached to a
    declaration);
  - no signature change, no new symbol, no struct layout change,
    no public rename, no new `#include`;
  - idempotent; weighted diff ≤ `fix.max_diff_lines` (default 20);
  - `count` field for uniform replacements — same change applied to
    N occurrences of an identical snippet, verified against actual
    occurrence count before apply.
- One git commit per fix on a dedicated branch
  `ai-checker-<proj>-fix<N>-<seq>/<chunk>-<local>`. Requires a
  clean working tree. Commit subject follows conventional commits:
  `fix(ngutils/<proj>): <english subject> (<branch>)`. Accept with
  `git merge --no-ff`, reject with `git branch -D`.
- Dry-run by default. `ai-fix-*` prints diffs and caches validated
  proposals in `proposal_commit`; `ai-fix-commit-*` applies them.
  `AI_FIX_REFETCH=1` forces a fresh LLM call;
  `AI_FIX_REPROCESS=1` re-processes terminal findings.
- `FIXLIST` selection syntax: `1/1` (exact), `1/1-5` (range),
  `1/1001-1011` (table rows), `1/*` (whole chunk), `*`
  (everything). Unmatched selectors print the available ranges.
- `ai-checker/config/check.db` (SQLite, gitignored) — persists
  findings, cached proposals, statuses, and tunable params.
  Schema v6. Auto-migration on first `connect()`; version mismatch
  refuses to proceed. Survives `make clean`.
- Issue lifecycle: `added` → `proposed-to-fix` / `analyze-by-dev` /
  `failed` → `fixed` / `rejected`. `fixed`, `rejected`, and
  `analyze-by-dev` are terminal; subsequent `ai-fix-*` runs skip
  them unless `--reprocess`.
- `ai-status` / `ai-status-<proj>` — inspect findings from
  `check.db` (summary / per-finding / `--status <s>` / `--key`
  for FIXLIST-ready output / `--json`). For `fixed` findings the
  status column shows `fixed(<short-sha>)` and the branch line is
  omitted once the branch is merged and deleted.
- `ai-config` / `ai-config-set` — tune `check.max_chars` (default
  80 000), `fix.max_source_chars` (100 000), `fix.default_limit`
  (50), `fix.max_diff_lines` (20). Stored in `params` table.
- `ai-del-branches` / `ai-del-branches-<proj>` — delete merged
  `ai-checker-*` branches. `FORCE=1` also deletes unmerged;
  `DRY_RUN=1` previews. Uses `git merge-base --is-ancestor` to
  classify merged vs unmerged.
- Review-side targets: `ai-models`, `ai-check`, `ai-check-notest`,
  `ai-check-file`, `ai-checker-arch`.
- Fix-side targets: `ai-fix`, `ai-fix-file`, `ai-fix-commit`,
  `ai-fix-commit-file`.
- Single-instance lock via `flock(2)` on
  `build/ai-checker/.lock` — `ai-check` and `ai-fix` cannot run
  simultaneously; the lock is released on process exit (including
  `SIGKILL`), no stale locks to clean up.
- Module layout with enforced invariants: all SQL in `db.py`, all
  git in `gitutil.py`, all sysinfo in `sysinfo.py` — no
  `sqlite3.connect`, no `subprocess.run(["git", ...])`, no
  `platform.system` outside those modules.
- Report header (both check and fix reports): model, endpoint,
  duration, per-chunk durations, prompt/completion/total tokens,
  completion throughput, host, OS, CPU, RAM, Python version.

- `modules(name, descr)` table — registry of param namespaces. Each
  `params` row now carries `module TEXT REFERENCES modules(name)`
  instead of a `CHECK (category IN (...))` constraint. Adding a new
  param namespace is `INSERT INTO modules`, not a schema migration.
  `ai-config --modules` lists the registry, `ai-config --long`
  groups params by module and shows the description.
- `ai-config-set` now takes `MODULE=` (was `CATEGORY=`). The
  migration from schema v6 to v7 renames the column and preserves
  any custom values set via the old CLI.

- `llm.py` — single HTTP client for LM Studio. All `POST
  /v1/chat/completions` and `GET /v1/models` calls go through it;
  `check.py` and `fix.py` no longer import `requests` directly.
  `llm.resolve_url()` and `llm.resolve_model()` consult (in order)
  explicit CLI arg → `llm.url` / `llm.model_name` DB params →
  `LMSTUDIO_URL` / `QWEN_MODEL` env vars → hard-coded default, so
  existing env-var workflows keep working.

- New `llm` module (module table) with params `llm.url` and
  `llm.model_name`, plus `check.temperature` (0.2) and
  `fix.temperature` (0.0). These were previously hard-coded in
  `check.py` / `fix.py`. Adjust with `make ai-config-set
  MODULE=llm NAME=url VALUE=http://192.168.1.10:1234`.

### Changed

- Schema version bumped to v7; `params.category` renamed to
  `params.module` with FK to `modules(name)`.
- `make clean` now preserves `build/ai-checker/` and
  `build/ai-checker-reports*.tar.zst` — review history survives
  routine cleaning.

