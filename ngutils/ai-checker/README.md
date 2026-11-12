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

## See also

- `Makefile` — target definitions (`ai-check*`, `ai-checker-arch`).
- `DISCLAIMER.md` — repository-wide disclaimer; the AI-specific
  section there applies to every report produced by this tool.
- `CHANGELOG.md` — release history.

