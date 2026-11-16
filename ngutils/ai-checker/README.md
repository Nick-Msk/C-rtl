# ai-checker

AI-assisted code review and local auto-fix for `ngutils` projects.
Talks to a locally running LM Studio instance over the
OpenAI-compatible HTTP API.

Two phases:

- **review** (`ai-check-*`) — LLM reads source, writes a Markdown
  report, ingests findings into a local SQLite DB.
- **fix** (`ai-fix-*`) — LLM classifies each finding as *local*
  (safe to auto-apply) or *needs-review* (human decides), then
  either proposes edits (dry-run) or commits them to dedicated git
  branches (one commit per fix).

The model is expected to be a Qwen-family LLM, but any chat model
served by LM Studio will do — the target name is historical.

## Requirements

- Python >= 3.10
- [`uv`](https://docs.astral.sh/uv/) on `PATH`
- LM Studio running locally with at least one chat model loaded
  (default endpoint: `http://127.0.0.1:1234`)
- Git working tree clean when running `ai-fix-commit-*`

`uv` creates a throwaway venv from `pyproject.toml` and installs
`requests` on first run; no manual setup is needed.

## Workflow

Full cycle for one project, with real output so you can compare
against your own runs. Replace `<proj>` with a project name (`bool`,
`checker`, `common`, `error`, `fs2`, `log`, `path`).

```
┌──────────────┐  ┌──────────────┐  ┌────────────────────┐
│ ai-check-<p> │─▶│  ai-fix-<p>  │─▶│ ai-fix-commit-<p>  │
│  review      │  │  classify    │  │  apply + commit    │
│  → check.db  │  │  → proposals │  │  → git branches    │
└──────────────┘  └──────────────┘  └────────────────────┘
                                            │
                                            ▼
                                  ┌────────────────────┐
                                  │  git merge / -D    │
                                  │  ai-del-branches   │
                                  └────────────────────┘
```

### 1. Review — `make ai-check-<proj>`

Sends sources to LM Studio, writes a Markdown report, ingests
findings into `config/check.db` (status `added`).

```sh
$ make ai-check-bool
==> ai-check bool
[ai-checker] endpoint=http://127.0.0.1:1234 model=qwen3.8-27b-splash
[ai-checker] host=Mac16,6  cpu=Apple M4 Max (14p/14l)  ram=36.0 GB
[ai-checker] bool: 3 files, 1 chunk(s) -> build/ai-checker/bool/2026-11-16_09-13-25.md
## Критично

Ничего. Код `bool.c` — 12 строк константных данных и два тривиальных геттера.
UB, утечек, race-условий нет.

## Важно
**1. Тест `stable_pointer` проверяет implementation-defined поведение**
...
## Мелочи
- **Стиль комментариев в тесте.** ...
- **`bool_versions()` возвращает указатель на внутренний массив.** ...
- **NULL-терминатор в `versions[]`.** ...
## Что хорошо
...
[ai-checker]   chunk 1/1: 47.4s  prompt=1,233  completion=2,074
[ai-checker] db: ingested 3 issue(s)
[ai-checker] rotated 1 older report(s) -> build/ai-checker/bool/hist/
[ai-checker] done: bool -> build/ai-checker/bool/2026-11-16_09-13-25.md  [47.4s · 1,233 in / 2,074 out]
```

What to check:

- `1 chunk(s)` — the whole project fit into one prompt. If you
  see `N chunks`, the report is split into `## Part 1/N`,
  `## Part 2/N`, ... each with its own `Критично / Важно /
  Мелочи`.
- `ingested N issue(s)` — number of findings written to
  `check.db`. Compare with `make ai-status-<proj> | wc -l`.
- `rotated M older` — previous reports moved into `<proj>/hist/`.

Immediately after:

```sh
$ make ai-status-bool
bool   1/1001     Мелочи     added            **Стиль комментариев в тесте.**
bool   1/1002     Мелочи     added            **`bool_versions()` возвращает указатель на внутренний масси
bool   1/1003     Мелочи     added            **NULL-терминатор в `versions[]`.** Хорошая практика, но в д
```

All `added`. Decoding the key:

- `1/1001` = `<chunk>/<local>`; chunk = 1 (single-chunk report),
  local = 1001 (first row of the `Мелочи` table; headers use
  `1/1..1/999`, tables use `1/1001..1/1999`).
- Multi-chunk: `2/1001` = first `Мелочи` row of chunk 2. Keys are
  unique across the whole report.

### 2. Classify — `make ai-fix-<proj>` (dry-run)

LLM classifies each finding as `local` (safe to auto-apply) or
`needs_review` (human decides). The diff is printed but **nothing
is committed**. Validated edits are cached in `proposal_commit`
(status `proposed-to-fix`).

```sh
$ make ai-fix-bool
==> ai-fix bool
[ai-fixer] endpoint=http://127.0.0.1:1234 model=qwen3.8-27b-splash

--- bool: build/ai-checker/bool/2026-11-16_09-13-25.md ---

=== Finding 1/1001  (1/3)  [Мелочи] ===
  **Стиль комментариев в тесте.** Раздел `// version control` использует `//`, тогда как все остальные секции — `/* --- ... --- */`. Унифицируйте....
  LOCAL: Match the /* --- ... --- */ style used by all other section headers in this file.
    --- a/bool/test/test_bool.c
    +++ b/bool/test/test_bool.c
    @@ -1 +1 @@
    -// version control
    +/* --- version control --- */
  [dry-run] not applied (saved as proposal)

=== Finding 1/1002  (2/3)  [Мелочи] ===
  **`bool_versions()` возвращает указатель на внутренний массив.** Потребитель может записать через `const_cast` (хотя `const char *const *` делает это менее удобным). Для публичного API это допустимо, ...
  LOCAL: Add explicit const_cast warning to bool_versions doc comment
    --- a/bool/include/bool.h
    +++ b/bool/include/bool.h
    @@ -1,3 +1,6 @@
      * @return Pointer to a static array of version strings. Read-only,
      *         must not be freed.
    + *
    + * @warning The returned array is read-only; do not modify through
    + *          const_cast.
      */
  [dry-run] not applied (saved as proposal)

=== Finding 1/1003  (3/3)  [Мелочи] ===
  **NULL-терминатор в `versions[]`.** Хорошая практика, но в доке стоит упомянуть, что массив NULL-terminated, ...
  NEEDS REVIEW: The doc comment for bool_versions() in bool.h already states 'terminated by @c NULL'. No edit is needed; the finding is already satisfied by existing documentation.

[ai-fixer] bool: 0 applied, 2 ready (not committed), 0 skipped, 1 needs review, 54.4s, 9,715 in / 1,332 out
[ai-fixer] report: build/ai-checker/bool/fix/2026-11-16_09-14-33.md

[ai-fixer] 2 fix(es) ready to commit for bool:
    make ai-fix-commit-bool FIXLIST="1/1001,1/1002"
  or commit all proposed (no FIXLIST):
    make ai-fix-commit-bool
  discard and reclassify with LLM:
    make ai-fix-log FIXLIST="<id>" AI_FIX_REFETCH=1

=== Needs human thought (1) ===
[1] id=1/1003  **NULL-терминатор в `versions[]`.** Хорошая практика, но в д
    why: The doc comment for bool_versions() in bool.h already states 'terminated by @c NULL'. No edit is needed; the finding is already satisfied by existing documentation.
```

Reading the output:

| Line | Meaning |
|---|---|
| `=== Finding 1/1001 (1/3) [Мелочи] ===` | key matches `ai-status`; `1/3` is position in this run |
| `LOCAL: <explanation>` + diff | edit is valid, cached, not applied |
| `[dry-run] not applied (saved as proposal)` | `ai-fix-log` never commits |
| `NEEDS REVIEW: <reason>` + `direction` | model declined (multi-file, multi-function, signature change, etc.) |
| `ready (not committed)` | `proposed-to-fix` in DB, waiting for `ai-fix-commit-*` |
| `ready to commit for bool: make ai-fix-commit-bool FIXLIST="..."` | copy-paste command |

Immediately after:

```sh
$ make ai-status-bool
bool   1/1001     Мелочи     proposed-to-fix  **Стиль комментариев в тесте.** Раздел `// version control`
bool   1/1002     Мелочи     proposed-to-fix  **`bool_versions()` возвращает указатель на внутренний масси
bool   1/1003     Мелочи     analyze-by-dev   **NULL-терминатор в `versions[]`.** Хорошая практика, но в д
```

`proposed-to-fix` — can be committed; `analyze-by-dev` — cannot
(LLM said "already documented", but verify yourself — see
`report: ...fix/2026-11-16_09-14-33.md`).

A **second** `ai-fix-bool` without `--refetch` does **not** call the
LLM — it reads the cached proposals and prints in under a second:

```sh
$ make ai-fix-bool
...
=== Finding 1/1001  (1/3)  [Мелочи] ===
  ...
  LOCAL (cached 2026-11-16T09:14:49): Match the /* --- ... --- */ style...
    --- a/bool/test/test_bool.c
    +++ ...
  [dry-run] cached proposal from 2026-11-16T09:14:49
...
[ai-fixer] bool: 0 applied, 2 ready (not committed), 0 skipped, 1 needs review, 0.1s, 0 in / 0 out
```

Note `0 in / 0 out`, `0.1s` — LLM was not called. To
**re**-classify (e.g. a different model is loaded):

```sh
make ai-fix-bool AI_FIX_REFETCH=1
```

### 3. Apply — `make ai-fix-commit-<proj>`

Applies proposals (or only `FIXLIST`) and commits each fix to a
**separate branch**, one commit per fix. No prompts: running
`-commit-` already means the decision is made.

```sh
$ make ai-fix-commit-bool FIXLIST="1/1001,1/1002"
==> ai-fix-commit bool
[ai-fixer] endpoint=http://127.0.0.1:1234 model=qwen3.8-27b-splash

--- bool: build/ai-checker/bool/2026-11-16_09-13-25.md ---
  skip 1 already-resolved (fixed/rejected/analyze-by-dev); use --reprocess to include

=== Finding 1/1001  (1/2)  [Мелочи] ===
  **Стиль комментариев в тесте.** ...
  LOCAL (cached 2026-11-16T09:14:49): Match the /* --- ... --- */ style used by all other section headers in this file.
    --- a/bool/test/test_bool.c
    +++ b/bool/test/test_bool.c
    ...
  COMMITTED 2bc49ed  ai-checker-bool-fix1-1/1-1001  bool/test/test_bool.c

=== Finding 1/1002  (2/2)  [Мелочи] ===
  ...
  COMMITTED b7d6838  ai-checker-bool-fix1-2/1-1002  bool/include/bool.h

[ai-fixer] bool: 2 applied, 0 ready (not committed), 0 skipped, 0 needs review, 0.1s, 0 in / 0 out
[ai-fixer] report: build/ai-checker/bool/fix/2026-11-16_09-43-49_commit.md

[ai-fixer] 2 fix branch(es) (run_id=ai-checker-bool-fix1):

  ai-checker-bool-fix1-1/1-1001
    id:       1/1001
    summary:  **Стиль комментариев в тесте.** Раздел `// version control` ...
    inspect:  git show 2bc49ed
    merge:    git merge --no-ff ai-checker-bool-fix1-1/1-1001
    reject:   git branch -D ai-checker-bool-fix1-1/1-1001

  ai-checker-bool-fix1-2/1-1002
    id:       1/1002
    summary:  **`bool_versions()` возвращает указатель на внутренний массив.** ...
    inspect:  git show b7d6838
    merge:    git merge --no-ff ai-checker-bool-fix1-2/1-1002
    reject:   git branch -D ai-checker-bool-fix1-2/1-1002

[ai-fixer] from 'master', inspect all at once:
    git show 2bc49ed b7d6838

[ai-fixer] from 'master', merge in order:
    git merge --no-ff ai-checker-bool-fix1-1/1-1001
    git merge --no-ff ai-checker-bool-fix1-2/1-1002

[ai-fixer] reject all:
    git branch -D ai-checker-bool-fix1-1/1-1001 ai-checker-bool-fix1-2/1-1002
[ai-fixer] json log:    build/ai-checker/fixes/2026-11-16_09-43-49_bool.json

[ai-fixer] all branches from this run:
  ai-checker-bool-fix1-1/1-1001     bool      id=1/1001
  ai-checker-bool-fix1-2/1-1002     bool      id=1/1002
[ai-fixer] return to base:  git checkout master
```

Key points:

- Branch name: `ai-checker-<proj>-fix<N>-<seq>/<chunk>-<local>`.
  `<N>` = run, `<seq>` = order within the run, `<chunk>-<local>` =
  the key from `ai-status`. Greppable and self-documenting.
- `2 applied` — fixes are in git; `0 in / 0 out` — LLM was not
  called (cached proposals used).
- `[ai-fixer] return to base: git checkout master` — process is
  back on master, working tree is clean.

Immediately after:

```sh
$ git branch
  ai-checker-bool-fix1-1/1-1001
  ai-checker-bool-fix1-2/1-1002
* master

$ git status
On branch master
Your branch is up to date with 'origin/master'.
nothing to commit, working tree clean
```

`master` is clean — fixes live on side branches. Inspecting one:

```sh
$ git show 2bc49ed
commit 2bc49ed... (ai-checker-bool-fix1-1/1-1001)
Author: ...
Date:   ...

    fix(ngutils/bool): Unify version control section comment style in test (ai-checker-bool-fix1-1/1-1001)

    Section: Мелочи
    Finding: 1/1001
    File:    bool/test/test_bool.c
    Reason:  Single comment style fix ...
    Change:  Match the /* --- ... --- */ style ...

    Full finding:
    **Стиль комментариев в тесте.** ...

    Applied by ai-checker/fix.py

diff --git a/ngutils/bool/test/test_bool.c b/ngutils/bool/test/test_bool.c
...
-// version control
+/* --- version control --- */
```

Subject: `fix(ngutils/<proj>): <english> (<branch>)` — conventional
commits, scope `ngutils/<proj>`, branch ref included. One line via
`git log -1 --format='%s' 2bc49ed`.

`ai-status` right after:

```sh
$ make ai-status-bool
bool   1/1001     Мелочи     fixed(2bc49ed)   **Стиль комментариев в тесте.**
bool   1/1002     Мелочи     fixed(b7d6838)   **`bool_versions()` возвращает указатель на внутренний масси
bool   1/1003     Мелочи     analyze-by-dev   **NULL-терминатор в `versions[]`.**
```

`fixed(<sha>)` — short sha; `git show 2bc49ed` shows the diff.
Compact, no second line pointing at a (possibly already deleted)
branch.

### 4. Accept / reject

Each fix is **one commit on its own branch**. Accept with normal
`git merge --no-ff`, reject with `git branch -D`.

```sh
# Inspect
$ git show 2bc49ed
$ git log --oneline master..ai-checker-bool-fix1-1/1-1001

# Accept (one fix)
$ git merge --no-ff ai-checker-bool-fix1-1/1-1001
Merge made by the 'ort' strategy.
 ngutils/bool/test/test_bool.c | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)

# Reject (another)
$ git branch -D ai-checker-bool-fix1-2/1-1002
Deleted branch ai-checker-bool-fix1-2/1-1002 (was b7d6838).
```

Why `--no-ff`: without it, if the branch is a direct descendant of
master, git does a fast-forward and the fix looks like an ordinary
linear commit. With `--no-ff` it's an explicit merge bubble — clear
"this is a fix, accepted via merge". Both are valid; `--no-ff` is
more uniform.

No `cherry-pick` — to keep "only some fixes", merge the chosen
branches and `branch -D` the rest. If two fixes touch the same
file, `git merge` of the second will surface a conflict — resolve
it or drop one of the branches.

```sh
$ git hist | head -6
*   9b02996 - 2026-11-16 | Merge branch 'ai-checker-bool-fix1-1/1-1001' (HEAD -> master)
|\  
| * 2bc49ed - 2026-11-16 | fix(ngutils/bool): Unify version control ... (ai-checker-bool-fix1-1/1-1001)
|/  
* 9701a29 - 2026-11-16 | refactor(ngutils/ai-check): git api moved from fix.py to gitutils
```

### 5. Cleanup — `make ai-del-branches-<proj>`

After merge, the branches are dead. `ai-del-branches` deletes
**only merged** ones (`git branch -d`); unmerged branches are
preserved unless `FORCE=1`.

```sh
$ make ai-del-branches-bool DRY_RUN=1
==> ai-del-branches bool
pattern: 'ai-checker-bool-*'

merged (safe to delete) (2):
  ai-checker-bool-fix1-1/1-1001
  ai-checker-bool-fix1-2/1-1002

[dry-run] would delete 2 branch(es)

$ make ai-del-branches-bool
==> ai-del-branches bool
pattern: 'ai-checker-bool-*'

merged (safe to delete) (2):
  ai-checker-bool-fix1-1/1-1001
  ai-checker-bool-fix1-2/1-1002

  deleted: ai-checker-bool-fix1-1/1-1001
  deleted: ai-checker-bool-fix1-2/1-1002

2 deleted, 0 failed
```

If some branches are unmerged (rejected but never `git branch -D`):

```sh
$ make ai-del-branches-bool
pattern: 'ai-checker-bool-*'

merged (safe to delete) (1):
  ai-checker-bool-fix1-1/1-1001

unmerged (preserved unless FORCE=1) (1):
  ai-checker-bool-fix1-2/1-1002

  deleted: ai-checker-bool-fix1-1/1-1001

1 deleted, 0 failed

$ make ai-del-branches-bool FORCE=1
...
  deleted: ai-checker-bool-fix1-2/1-1002

2 deleted, 0 failed
```

After cleanup, `ai-status-bool` still shows `fixed(<sha>)` — the
branch is gone, but the commit is reachable from master:

```sh
$ make ai-status-bool
bool   1/1001     Мелочи     fixed(2bc49ed)   **Стиль комментариев в тесте.**
bool   1/1002     Мелочи     fixed(b7d6838)   **`bool_versions()` возвращает ...**
bool   1/1003     Мелочи     analyze-by-dev   **NULL-терминатор в `versions[]`.**
```

### 6. Inspect — `make ai-status-*`

```sh
$ make ai-status
project          added  proposed-to-fix  analyze-by-dev  fixed  rejected  failed
bool                 0                0               1      2         0       0
checker              3                1               5      2         0       0
fs2                 30                0               0      0         0       0
log                 11               11               6      0         0       0
```

```sh
$ make ai-status-bool
bool   1/1001     Мелочи     fixed(2bc49ed)   **Стиль комментариев в тесте.**
bool   1/1002     Мелочи     fixed(b7d6838)   **`bool_versions()` возвращает указатель на внутренний масси
bool   1/1003     Мелочи     analyze-by-dev   **NULL-терминатор в `versions[]`.** Хорошая практика, но в д

$ make ai-status-bool AI_STATUS_ARGS='--status analyze-by-dev'
bool   1/1003     Мелочи     analyze-by-dev   **NULL-терминатор в `versions[]`.** Хорошая практика, но в д

$ make ai-status-bool AI_STATUS_ARGS='--key'
1/1001
1/1002
1/1003

$ make ai-status-bool AI_STATUS_ARGS='--json'
[
  {
    "project": "bool",
    "chunk_id": 1,
    "local_id": 1001,
    ...
    "key": "1/1001"
  },
  ...
]
```

`--key` is FIXLIST-ready: `$(make ai-status-bool
AI_STATUS_ARGS='--key' | paste -sd, -)` produces
`1/1001,1/1002,1/1003`.

### 7. Re-running, resetting, and debugging

| Situation | Command |
|---|---|
| Re-classify `needs_review` findings | `make ai-fix-<p> AI_FIX_REFETCH=1 FIXLIST="<id>"` |
| Re-process `fixed`/`rejected` | `make ai-fix-<p> AI_FIX_REPROCESS=1` |
| Reset a `fixed` row in the DB (after `git reset`) | `sqlite3 config/check.db "UPDATE issue SET status='added' WHERE ..."` |
| Rebuild the DB from scratch | `rm ai-checker/config/check.db && make ai-check-<p>` |
| Inspect what the model actually saw | `build/ai-checker/<p>/_types.h` and `build/ai-checker/<p>/fix/<ts>.md` |

## Typical session — `bool` end to end

```sh
# 1. review
make ai-check-bool
# → 3 findings, status=added

# 2. classify
make ai-fix-bool
# → 2 proposed-to-fix, 1 analyze-by-dev
# → hint: FIXLIST="1/1001,1/1002"

# 3. apply (paste the hint)
make ai-fix-commit-bool FIXLIST="1/1001,1/1002"
# → 2 branches, 2 commits, status=fixed

# 4. accept
git merge --no-ff ai-checker-bool-fix1-1/1-1001
git merge --no-ff ai-checker-bool-fix1-2/1-1002

# 5. cleanup
make ai-del-branches-bool

# 6. run tests
make test-bool

# 7. verify
make ai-status-bool
# → both fixed(sha), one analyze-by-dev
```

Wall-clock: review ~47s + classify ~54s + apply ~0.1s + merge &
cleanup instantaneous. One cycle per project is roughly two
minutes.

### 1. Review

```sh
make ai-check-<proj>
```

`check.py` sends source files to LM Studio, gets a Markdown report,
ingests every finding into `config/check.db` with status `added`.
The report goes to `build/ai-checker/<proj>/<ts>.md`. Older reports
rotate into `<proj>/hist/`.

Scope variants:

```sh
make ai-check-notest-<proj>          # skip test/ directories
make ai-check-file F=log/src/log.c   # single file
make ai-check-fs2 AI_CHECK_EXTRA="--full-headers"
```

### 2. Classify (dry-run)

```sh
make ai-fix-<proj>                   # all findings
make ai-fix-<proj> FIXLIST="1/1-5"   # only some
```

For every finding the classifier asks the LLM:

- **local** → edit validated (single file or paired `.c/.h`, one
  function body, no signature change, diff under limit). The diff
  is printed and the edit is cached as a *proposal* in the
  `proposal_commit` table. DB status: `proposed-to-fix`.
- **needs_review** → listed with a suggested direction, never
  auto-applied. DB status: `analyze-by-dev`.
- **failed** → LLM error, invalid JSON, or edit failed validation.

The fix report goes to `build/ai-checker/<proj>/fix/<ts>.md`.
Re-running `ai-fix-<proj>` does **not** re-classify cached
proposals — it just shows the cached diff (fast). Use
`AI_FIX_REFETCH=1` to force a fresh LLM call.

### 3. Apply

```sh
make ai-fix-commit-<proj>            # all proposals
make ai-fix-commit-<proj> FIXLIST="1/1001,1/1003"
```

For every proposal (or matching `FIXLIST`), the fixer:

1. verifies the working tree is clean (refuses otherwise);
2. creates a branch from the current `HEAD`:
   `ai-checker-<proj>-fix<N>-<seq>/<chunk>-<local>`;
3. applies the edit, `git add`, `git commit` with message
   `fix(ngutils/<proj>): <english subject> (<branch>)`;
4. returns to the base branch.

No interactive prompts. The dry-run already showed you the diffs.

### 4. Accept or reject (git-level)

Each fix is **one commit on its own branch**:

```sh
git branch --list 'ai-checker-<proj>-*'

# inspect
git show <sha>
git log --oneline <branch>

# accept
git merge --no-ff <branch>

# reject
git branch -D <branch>
```

No cherry-pick. If two fixes touch the same file, `git merge`
will surface a conflict — resolve or reject one of the branches.

### 5. Cleanup

```sh
make ai-del-branches-<proj>             # delete merged branches
make ai-del-branches-<proj> DRY_RUN=1   # preview
make ai-del-branches-<proj> FORCE=1     # also delete unmerged
make ai-del-branches                    # all projects
```

By default, only branches already merged into `HEAD` are removed
(`git branch -d`). Unmerged branches stay until `FORCE=1`
(`git branch -D`).

### Inspecting state

```sh
make ai-status                        # per-project summary
make ai-status-<proj>                 # individual findings
make ai-status-<proj> AI_STATUS_ARGS='--status proposed-to-fix'
make ai-status-<proj> AI_STATUS_ARGS='--key'      # FIXLIST-ready
make ai-status-<proj> AI_STATUS_ARGS='--json'
```

Example output:

```
bool   1/1001  Мелочи  fixed(2bc49ed)   **Стиль комментариев в тесте.**
bool   1/1002  Мелочи  fixed(b7d6838)   **bool_versions() возвращает...**
bool   1/1003  Мелочи  analyze-by-dev   **NULL-терминатор в versions[].**
log    1/1     Критично  proposed-to-fix  **g_offset коррупируется...**
log    1/2     Важно    added            **%19s vs MAX_MODULE...**
```

### Status values

| Status             | Set by                    | Meaning                              |
|--------------------|---------------------------|--------------------------------------|
| `added`            | `check.py` ingest         | just parsed, not yet classified      |
| `proposed-to-fix`  | `fix.py` dry-run (local)  | cached edit ready to apply           |
| `analyze-by-dev`   | `fix.py` (needs_review)   | waiting for human decision           |
| `fixed`            | `fix.py --apply`          | committed to a fix branch            |
| `rejected`         | `--reprocess` + user      | manually discarded                   |
| `failed`           | `fix.py`                  | stale proposal, LLM error, bad edit  |

`fixed`, `rejected`, `analyze-by-dev` are **terminal**: on
subsequent `ai-fix` runs those findings are skipped. Use
`--reprocess` to override.

## Targets

All targets are defined in the top-level `Makefile`.

### Review

| Target                       | What it does                                     |
|------------------------------|--------------------------------------------------|
| `make ai-models`             | Print models currently loaded in LM Studio.      |
| `make ai-check`              | Review all discovered projects (include `test/`).|
| `make ai-check-<proj>`       | Review one project.                              |
| `make ai-check-notest`       | Review all projects, exclude `test/`.            |
| `make ai-check-notest-<p>`   | Review one project, exclude `test/`.             |
| `make ai-check-file F=<p>`   | Review a single source file.                     |
| `make ai-checker-arch`       | Tar+zstd `build/ai-checker/` into `.tar.zst`.    |

Reports are intentionally preserved by `make clean`.

### Fix

| Target                          | What it does                                   |
|---------------------------------|------------------------------------------------|
| `make ai-fix`                   | Dry-run: classify + propose fixes (all).       |
| `make ai-fix-<proj>`            | Dry-run for one project.                       |
| `make ai-fix-file F=<p>`        | Dry-run for one file's report.                 |
| `make ai-fix-commit`            | Apply + commit to fix branches (all).          |
| `make ai-fix-commit-<proj>`     | Apply + commit for one project.                |
| `make ai-fix-commit-file F=<p>` | Apply + commit for one file's report.          |

### Inspect / cleanup

| Target                          | What it does                                   |
|---------------------------------|------------------------------------------------|
| `make ai-status`                | Per-project summary from `check.db`.           |
| `make ai-status-<proj>`         | Individual findings for one project.           |
| `make ai-config`                | List tunable params from `check.db`.           |
| `make ai-config-set CATEGORY=<c> NAME=<n> [VALUE=<v>]` | Edit or reset a param. |
| `make ai-del-branches`          | Delete merged `ai-checker-*` branches.         |
| `make ai-del-branches-<proj>`   | Same, for one project.                         |

## FIXLIST syntax

`FIXLIST` selects findings by `<chunk>/<local_id>` (the key shown
by `ai-status`). Supported forms, comma- or space-separated:

| Form             | Meaning                                    |
|------------------|--------------------------------------------|
| `1/1`            | exact finding                              |
| `1/1-5`          | locals 1..5 in chunk 1                     |
| `1/1001-1011`    | table findings (local ≥ 1000)              |
| `1/*`            | everything in chunk 1                      |
| `*`              | everything (same as omitting FIXLIST)      |
| (omit)           | all, capped by `fix.default_limit` (50)    |

Examples:

```sh
make ai-fix-commit-log FIXLIST="1/1001,1/1002"
make ai-fix-commit-log FIXLIST="1/1-5,1/1010"
make ai-fix-commit-log FIXLIST="1/*"          # whole chunk
make ai-fix-log FIXLIST="*" AI_FIX_LIMIT=0    # everything, no cap
```

If `FIXLIST` matches nothing, the fixer prints `cut to: <available
ranges>` so you can copy-paste the right one.

### Flags

| Var / flag             | Default | Notes                              |
|------------------------|---------|------------------------------------|
| `FIXLIST`              | *(empty)* | select findings (see above)      |
| `AI_FIX_LIMIT`         | 50      | max findings when FIXLIST omitted  |
| `AI_FIX_REFETCH=1`     | —       | ignore cached proposals; re-call LLM |
| `AI_FIX_REPROCESS=1`   | —       | also re-process fixed/rejected/analyze |
| `FORCE=1` (del-branches)| —      | delete unmerged branches too       |
| `DRY_RUN=1` (del-branches)| —    | print plan, do nothing             |

## Configuration

### Environment

| Variable        | Default                    | Notes                               |
|-----------------|----------------------------|-------------------------------------|
| `LMSTUDIO_URL`  | `http://127.0.0.1:1234`    | Base URL (no `/v1` suffix).         |
| `QWEN_MODEL`    | *(empty)*                  | Empty = first loaded model whose id contains `qwen`. |

Both can be set in the shell or on the `make` command line.

### Tunable params (`config/check.db`)

```sh
make ai-config
make ai-config-set CATEGORY=check NAME=max_chars VALUE=120000
make ai-config-set CATEGORY=fix   NAME=max_diff_lines VALUE=30
make ai-config-set CATEGORY=check NAME=max_chars         # reset
```

| Category | Name                | Default   | What                        |
|----------|---------------------|-----------|-----------------------------|
| `check`  | `max_chars`         | 80 000    | chunk size (chars)          |
| `fix`    | `max_source_chars`  | 100 000   | prompt budget (chars)       |
| `fix`    | `default_limit`     | 50        | findings per project        |
| `fix`    | `max_diff_lines`    | 20        | max diff for a "local" fix  |

Params live in `config/check.db` and are read fresh on every run.

## Reports layout

```
build/ai-checker/
├── .lock                                 # single-instance lock (flock)
├── _machine.json                         # system snapshot
├── _files/
│   └── 2026-11-12_15-04-11_log.md        # ai-check-file output
├── log/
│   ├── 2026-11-12_14-23-45.md            # check report
│   ├── hist/                             # older check reports
│   │   └── 2026-11-10_08-15-00.md
│   └── fix/
│       ├── 2026-11-14_10-00-00.md        # fix dry-run report
│       └── 2026-11-14_10-10-00_commit.md # fix apply report
├── fixes/
│   └── 2026-11-14_10-10-00_log.json      # per-run JSON log
└── fs2/
    └── ...
```

Check reports and fix reports share the same header:

```markdown
# AI review: fs2

| field    | value |
|----------|-------|
| started  | 2026-11-14 15:01:02 |
| duration | 2m 34s  (per step: 1m 17s, 1m 17s) |
| model    | `qwen3.8-27b-splash` |
| endpoint | `http://127.0.0.1:1234` |
| tokens   | 24,320 prompt / 3,935 completion / 28,255 total (2 req) |
| speed    | 25.6 completion tok/s |
| host     | Mac16,6 |
| os       | macOS 14.6.1 (arm64) |
| cpu      | Apple M4 Max (14p/14l) |
| ram      | 36.0 GB |
| python   | 3.12.4 |
```

followed by the model's answer (`## Критично`, `## Важно`,
`## Мелочи`, `## Что хорошо`).

## What "local" means

A fix is applied only if **all** of these hold:

1. **At most two files.** If two, they must be a paired
   source/header with the same stem in the same directory:
   `X.c` + `X.h`, `X.cpp` + `X.hpp`, `X.cc` + `X.hpp`.
   Typical case: body in `X.c` and matching declaration in `X.h`.
2. **One function body per file** (or one `#include` line, one
   macro definition, or one comment block attached to a declaration).
3. **No function signature change.** Adding or removing a parameter
   is forbidden, even if `X.h` is also updated — every caller in
   other translation units would need updating.
4. No new function, struct, union, enum, or global variable.
5. No struct/union/enum layout, field type, or order change.
6. No public symbol rename or removal.
7. No new `#include` (typo fixes in existing includes are OK).
8. No changes required in any other file.
9. Idempotent.
10. Diff ≤ `fix.max_diff_lines` (default 20), weighted by `count`.

Two extras:

- **`count`** for uniform replacements — the same textual change
  applied to N occurrences of the same snippet (e.g. an identical
  `@param` doc-comment in 7 functions). `count` is verified against
  the actual number of occurrences before apply.
- **Compound findings** with multiple independent issues
  (`"in log_msg_ap, log_preambule, log_modsave"`) are classified
  as `needs_review` — never auto-applied as a partial fix.

If any of these fail — or the model is not sure — the finding goes
into the *needs human thought* section with a suggested direction.

## Architecture

| Module            | Responsibility                                    |
|-------------------|---------------------------------------------------|
| `check.py`        | review pipeline: chunk, ask LLM, write report, ingest |
| `fix.py`          | fix pipeline: classify, validate, apply, commit   |
| `common.py`       | shared constants, regexes, parser, lock, discovery |
| `db.py`           | **all** SQL: schema, migrations, ingest, queries  |
| `gitutil.py`      | **all** git operations: clean check, branches, commit |
| `sysinfo.py`      | best-effort system snapshot (host/cpu/ram)        |
| `report.py`       | report header formatting (markdown table)         |
| `status.py`       | `ai-status*` CLI                                  |
| `params.py`       | `ai-config*` CLI                                  |
| `gitutil.py main` | `ai-del-branches*` CLI                            |

Invariants:

- No `sqlite3` calls outside `db.py`.
- No `subprocess(["git", ...])` outside `gitutil.py`.
- No `platform.system` outside `sysinfo.py`.

`check.py` lifecycle:

1. query `/v1/models`, pick a model;
2. for each project: collect sources, build chunks (with `_types.h`
   digest prepended), stream responses, accumulate markdown;
3. write the `.md` report, ingest findings into `check.db`.

`fix.py` lifecycle:

1. resolve the latest report (or `--report`, or `--file`);
2. apply `FIXLIST` / `--limit` / resolved-filter;
3. per finding: load cached proposal (fast) or classify (LLM);
4. dry-run: print diff + save proposal;
5. `--apply`: clean-tree check → for each fix: new branch from
   `HEAD`, apply, commit, return to base;
6. write fix report + per-run JSON log.

`flock(2)` on `build/ai-checker/.lock` guarantees a single running
instance of `check.py` or `fix.py`. The lock is released
automatically when the process exits — including `SIGKILL`.

## Type digest

By default, `check.py` does **not** send header files verbatim.
Full headers in this repository can exceed 200 KB (e.g.
`fs2_api.h` is 91 KB, `fs2.h` is 60 KB) — duplicating them per
chunk would waste the entire context window.

Instead, only **type declarations** (`struct`, `union`, `enum`,
`typedef`) are extracted from all headers **and source files** of
the project and prepended to **every** chunk as a synthetic file
`_types.h`. The digest is also written to
`build/ai-checker/<project>/_types.h` so you can inspect what the
model actually sees.

This guarantees the model always has access to type definitions —
even in chunk 2, 3, ... — and to private struct definitions like
`struct fs2 { uint32_t borrow; ... }` even when that struct is
defined in `fs2.c` rather than a header. This eliminates a whole
class of "I cannot see the header, but if the type were X..." false
positives.

Function prototypes, macros, variable declarations, and inline
function bodies are **not** included in the digest. If you want full
headers instead, pass `--full-headers`:

```sh
make ai-check-fs2 AI_CHECK_EXTRA="--full-headers"
```

## Database

`ai-checker/config/check.db` — SQLite, gitignored, survives
`make clean`. Schema (v6):

- `base_commit(sha, short_sha, branch, first_dt)`
- `report(id, project, path, base_sha, model, kind, started, duration_s, dt)`
- `issue(id, report_id, finding_id, chunk_id, local_id, section,
  title, text, status, reason, direction, fix_branch, fix_commit,
  fix_subject, dt)`
- `proposal_commit(issue_id, seq, file, old_snippet, new_snippet,
  explanation, commit_msg, commit_subject, reason, file_sha256,
  edits_json, dt)`
- `params(category, name, value, default_value, dt)`

If schema changes between `ai-checker` versions, an automatic
migration runs on the first `connect()`. If the migration fails, the
tool prints the version mismatch and refuses to proceed — delete
`check.db` to start fresh (loses `proposed-to-fix`/`fixed`/etc.
history, but reports in `build/ai-checker/` are not touched).

## Limitations

- **Not a replacement for `make check`, `make analyze`, or tests.**
  Use it as a second pair of eyes, not as a gate.
- **LLMs hallucinate.** Findings must be verified against the source
  before any action.
- **No incremental mode.** Every check invocation reviews the whole
  project (or file). A `.git` diff mode is not implemented.
- **Prompt length is capped** at `check.max_chars` per chunk. Very
  large projects split across chunks; cross-chunk issues may be
  missed.
- **LM Studio only.** The scripts use `POST /v1/chat/completions`
  with `stream_options.include_usage`; other OpenAI-compatible
  servers work if they implement that flag, otherwise token usage
  will show as *"not reported"*.
- **`fix.py` requires a clean git tree.** Untracked or modified
  files abort `--apply` before any commit is made.

## See also

- `Makefile` — target definitions (`ai-check*`, `ai-fix*`,
  `ai-status*`, `ai-config*`, `ai-del-branches*`).
- `DISCLAIMER.md` — repository-wide disclaimer; the AI-specific
  section there applies to every report and every auto-applied fix.
- `CHANGELOG.md` — release history.
