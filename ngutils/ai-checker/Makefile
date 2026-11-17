# ai-checker/Makefile — AI review / fix / status pipeline.
#
# Normally invoked through the root dispatcher, which forwards every
# ai-* target here verbatim:
#     make ai-check-fs2
# Standalone works too:
#     make -C ai-checker ai-check-fs2
#
# All repo paths are resolved against ROOT, never CURDIR, so both
# invocation styles behave identically. uv runs in CURDIR (= ai-checker),
# where pyproject.toml lives — no 'cd' needed anymore.

ROOT := $(abspath $(CURDIR)/..)

# Same discovery as the root Makefile. ai-checker now HAS a Makefile,
# so it must be filtered out: it is a tool, not a library project.
PROJECTS := $(patsubst $(ROOT)/%/Makefile,%,$(wildcard $(ROOT)/*/Makefile))
PROJECTS := $(filter-out skeleton ai-checker,$(PROJECTS))

MAKEFLAGS += --no-print-directory

# ── knobs (command line or environment) ─────────────────────────────
LMSTUDIO_URL   ?= http://127.0.0.1:1234
QWEN_MODEL     ?=
AI_CHECK_EXTRA ?=

REPORTS_DIR := $(ROOT)/build/ai-checker
REPORTS_TAR := $(ROOT)/build/ai-checker-reports.tar.zst

FIXLIST          ?=
AI_FIX_LIMIT     ?=
AI_FIX_REFETCH   ?=
AI_FIX_REPROCESS ?=

AI_FIX_BASE   = $(if $(FIXLIST),--only "$(FIXLIST)",) \
                $(if $(AI_FIX_LIMIT),--limit $(AI_FIX_LIMIT),) \
                $(if $(AI_FIX_REFETCH),--refetch,) \
                $(if $(AI_FIX_REPROCESS),--reprocess,)
AI_FIX_ARGS   = $(AI_FIX_BASE)
AI_FIX_COMMIT = $(AI_FIX_BASE) --apply

AI_STATUS_ARGS ?=
AI_CONFIG_ARGS ?=

# F is relative to the repo root (or absolute), never to ai-checker/.
F_ABS = $(abspath $(if $(filter /%,$(F)),$(F),$(ROOT)/$(F)))

UV_RUN = LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" uv run python

DEL_BRANCHES_ARGS = $(if $(FORCE),--force,) $(if $(DRY_RUN),--dry-run,) \
                    $(if $(AI_DEL_CHECK_RUN),--check-run $(AI_DEL_CHECK_RUN),)

.PHONY: ai-help ai-check ai-check-notest ai-check-file ai-models \
        ai-arch ai-checker-arch \
        ai-fix ai-fix-commit ai-fix-file ai-fix-commit-file \
        ai-status ai-runs ai-del-branches ai-config ai-config-set \
        $(addprefix ai-check-,$(PROJECTS)) \
        $(addprefix ai-check-notest-,$(PROJECTS)) \
        $(addprefix ai-fix-,$(PROJECTS)) \
        $(addprefix ai-fix-commit-,$(PROJECTS)) \
        $(addprefix ai-status-,$(PROJECTS)) \
        $(addprefix ai-del-branches-,$(PROJECTS))

# ── review ──────────────────────────────────────────────────────────
# Reports: build/ai-checker/<project>/<YYYY-MM-DD_HH-MM-SS>[_notest].md
# The '_notest' suffix keeps full and test-less reviews from colliding.
# Reports survive 'make clean'; 'make ai-arch' packs them into
# build/ai-checker-reports.tar.zst.

ai-check:
	@$(UV_RUN) check.py --all $(AI_CHECK_EXTRA)

ai-check-notest:
	@$(UV_RUN) check.py --all --no-tests $(AI_CHECK_EXTRA)

$(addprefix ai-check-,$(PROJECTS)): ai-check-%:
	@echo "==> ai-check $*"
	@$(UV_RUN) check.py --project $* $(AI_CHECK_EXTRA)

$(addprefix ai-check-notest-,$(PROJECTS)): ai-check-notest-%:
	@echo "==> ai-check-notest $*"
	@$(UV_RUN) check.py --project $* --no-tests $(AI_CHECK_EXTRA)

ai-check-file:
	@test -n "$(F)" || { echo "usage: make ai-check-file F=<path>" >&2; exit 1; }
	@test -f "$(F_ABS)" || { echo "ERROR: file not found: $(F_ABS)" >&2; exit 1; }
	@$(UV_RUN) check.py --file "$(F_ABS)" $(AI_CHECK_EXTRA)

ai-models:
	@LMSTUDIO_URL="$(LMSTUDIO_URL)" uv run python check.py --models

ai-arch ai-checker-arch:
	@test -d $(REPORTS_DIR) || { echo "nothing to archive: $(REPORTS_DIR) missing"; exit 0; }
	@mkdir -p $(ROOT)/build
	@tar -C $(ROOT)/build --zstd -cf "$(REPORTS_TAR)" ai-checker
	@echo "Archived: $(REPORTS_TAR) ($$(du -h "$(REPORTS_TAR)" | cut -f1))"

# ── fix ─────────────────────────────────────────────────────────────
# Each accepted fix gets its own branch, one commit on top of HEAD.
# Refuses: unpaired multi-file edits, signature changes, new functions,
# struct layout changes, public renames, new #includes, edits spanning
# more than one function body, diffs > 20 lines. A paired X.c + X.h is
# allowed. Everything else is reported as "needs human thought".

ai-fix:
	@$(UV_RUN) fix.py --all $(AI_FIX_ARGS)

ai-fix-commit:
	@$(UV_RUN) fix.py --all $(AI_FIX_COMMIT)

$(addprefix ai-fix-,$(PROJECTS)): ai-fix-%:
	@echo "==> ai-fix $*"
	@$(UV_RUN) fix.py --project $* $(AI_FIX_ARGS)

$(addprefix ai-fix-commit-,$(PROJECTS)): ai-fix-commit-%:
	@echo "==> ai-fix-commit $*"
	@$(UV_RUN) fix.py --project $* $(AI_FIX_COMMIT)

ai-fix-file:
	@test -n "$(F)" || { echo "usage: make ai-fix-file F=<path>" >&2; exit 1; }
	@test -f "$(F_ABS)" || { echo "ERROR: file not found: $(F_ABS)" >&2; exit 1; }
	@$(UV_RUN) fix.py --file "$(F_ABS)" $(AI_FIX_ARGS)

ai-fix-commit-file:
	@test -n "$(F)" || { echo "usage: make ai-fix-commit-file F=<path>" >&2; exit 1; }
	@test -f "$(F_ABS)" || { echo "ERROR: file not found: $(F_ABS)" >&2; exit 1; }
	@$(UV_RUN) fix.py --file "$(F_ABS)" $(AI_FIX_COMMIT)

# ── status / runs / branches / config ───────────────────────────────

ai-status:
	@uv run python status.py $(AI_STATUS_ARGS)

$(addprefix ai-status-,$(PROJECTS)): ai-status-%:
	@uv run python status.py --project $* --list $(AI_STATUS_ARGS)

ai-runs:
	@uv run python status.py --runs $(AI_STATUS_ARGS)

ai-del-branches:
	@uv run python gitutil.py --all $(DEL_BRANCHES_ARGS)

$(addprefix ai-del-branches-,$(PROJECTS)): ai-del-branches-%:
	@echo "==> ai-del-branches $*"
	@uv run python gitutil.py --project $* $(DEL_BRANCHES_ARGS)

ai-config:
	@uv run python params.py $(AI_CONFIG_ARGS)

ai-config-set:
	@test -n "$(MODULE)" -a -n "$(NAME)" || { \
	    echo "usage: make ai-config-set MODULE=<mod> NAME=<name> [VALUE=<v>]" >&2; \
	    echo "       omit VALUE to reset to default" >&2; \
	    echo "       run 'make ai-config AI_CONFIG_ARGS=--modules' for module list" >&2; \
	    exit 1; }
	@uv run python params.py set $(MODULE) $(NAME) $(VALUE)

# ── help ────────────────────────────────────────────────────────────

ai-help:
	@echo "ai-checker (ROOT=$(ROOT))"
	@echo ""
	@echo "  make ai-check                 AI review of all projects"
	@echo "  make ai-check-<n>             AI review of one project"
	@echo "  make ai-check-notest[-<n>]    same, skip test/ dirs"
	@echo "  make ai-check-file F=<p>      AI review of one file"
	@echo "  make ai-models                list models in LM Studio"
	@echo "  make ai-arch                  pack reports into ai-checker-reports.tar.zst"
	@echo ""
	@echo "  make ai-fix[-<n>]             dry-run: propose local fixes"
	@echo "  make ai-fix-commit[-<n>]      apply + one branch per fix"
	@echo "  make ai-fix-file F=<p>        dry-run, one file's report"
	@echo "  make ai-fix-commit-file F=<p> apply, one file's report"
	@echo ""
	@echo "  make ai-status                per-project status summary"
	@echo "  make ai-status-<n>            list issues of one project"
	@echo "  make ai-status AI_STATUS_ARGS='--statuses'"
	@echo "  make ai-status AI_STATUS_ARGS='--status fixed --list'"
	@echo "  make ai-runs [AI_STATUS_ARGS='--limit 50']"
	@echo ""
	@echo "  make ai-del-branches          delete merged ai-checker-* branches"
	@echo "  make ai-del-branches-<n>      same, one project"
	@echo "  make ai-del-branches FORCE=1  also unmerged"
	@echo "  make ai-del-branches DRY_RUN=1"
	@echo "  make ai-del-branches AI_DEL_CHECK_RUN=7"
	@echo ""
	@echo "  make ai-config [AI_CONFIG_ARGS=--long|--modules]"
	@echo "  make ai-config-set MODULE=llm NAME=url VALUE=http://192.168.1.10:1234"
	@echo ""
	@echo "  FIXLIST=\"1/1, 1/7\"            only these findings (chunk/local)"
	@echo "  AI_FIX_LIMIT=N                cap when FIXLIST is unset (default 50)"
	@echo "  AI_FIX_REFETCH=1              ignore cached proposal, re-query LLM"
	@echo "  AI_FIX_REPROCESS=1            revisit terminal statuses + analyze-by-dev"
	@echo "  AI_CHECK_EXTRA='--full-headers'"
	@echo "  LMSTUDIO_URL=... QWEN_MODEL=..."
	@echo ""
	@echo "Projects:"
	@for p in $(PROJECTS); do echo "  - $$p"; done

