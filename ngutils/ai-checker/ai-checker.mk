_AI_MK := $(lastword $(MAKEFILE_LIST))
AI_CHECKER_DIR    ?= $(abspath $(dir $(_AI_MK)))
AI_CHECKER_ROOT   ?= $(CURDIR)
AI_CHECKER_CONFIG ?= $(wildcard $(CURDIR)/ai-checker.workspace.py)
AI_CHECKER_PROJECTS ?=

AI_RUN = cd "$(AI_CHECKER_DIR)" && \
	AI_CHECKER_ROOT="$(AI_CHECKER_ROOT)" \
	AI_CHECKER_CONFIG="$(AI_CHECKER_CONFIG)" \
	AI_CHECKER_PROJECTS="$(AI_CHECKER_PROJECTS)" \
	uv run python


FIXLIST          ?=
AI_FIX_LIMIT     ?=
AI_FIX_REFETCH   ?=
AI_FIX_REPROCESS ?=
AI_CHECK_EXTRA   ?=
AI_STATUS_ARGS 	 ?=
AI_CONFIG_ARGS   ?=

AI_FIX_BASE   = $(if $(FIXLIST),--only "$(FIXLIST)",) \
                $(if $(AI_FIX_LIMIT),--limit $(AI_FIX_LIMIT),) \
                $(if $(AI_FIX_REFETCH),--refetch,) \
                $(if $(AI_FIX_REPROCESS),--reprocess,)
AI_FIX_ARGS   = $(AI_FIX_BASE)
AI_FIX_COMMIT = $(AI_FIX_BASE) --apply

ai-check:            ; @$(AI_RUN) check.py --all $(AI_CHECK_EXTRA)
ai-check-notest:     ; @$(AI_RUN) check.py --all --no-tests $(AI_CHECK_EXTRA)
ai-check-%:          ; @$(AI_RUN) check.py --project $* $(AI_CHECK_EXTRA)
ai-check-notest-%:   ; @$(AI_RUN) check.py --project $* --no-tests $(AI_CHECK_EXTRA)
ai-fix-%:            ; @$(AI_RUN) fix.py --project $* $(AI_FIX_ARGS)
ai-fix-commit-%:     ; @$(AI_RUN) fix.py --project $* $(AI_FIX_COMMIT)
ai-status:           ; @$(AI_RUN) status.py $(AI_STATUS_ARGS)
ai-status-%:         ; @$(AI_RUN) status.py --project $* --list $(AI_STATUS_ARGS)
ai-config:           ; @$(AI_RUN) params.py $(AI_CONFIG_ARGS)
ai-reject:           ; @$(AI_RUN) decide.py --status rejected $(if $(REASON),--reason "$(REASON)",) $(KEY)

# ── help ────────────────────────────────────────────────────────────

ai-help:
	@echo "ai-checker (ROOT=$(ROOT))"
	@echo ""
	@echo "  make ai-check                 AI review of all projects"
	@echo "  make ai-check-<n>             AI review of one project"
	@echo "  make ai-check-reset           AI review of all projects with resetting suppressed"
	@echo "  make ai-check-reset-<n>       AI review of one project with resetting suppressed"
	@echo ""
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
