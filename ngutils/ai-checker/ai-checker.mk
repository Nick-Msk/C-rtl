_AI_MK := $(lastword $(MAKEFILE_LIST))
AI_CHECKER_DIR    ?= $(abspath $(dir $(_AI_MK)))
AI_CHECKER_ROOT   ?= $(CURDIR)
AI_CHECKER_CONFIG ?= $(wildcard $(CURDIR)/ai-checker.workspace.py)
AI_CHECKER_PROJECTS ?=

AI_RUN = cd "$(AI_CHECKER_DIR)" && \
	AI_CHECKER_ROOT="$(AI_CHECKER_ROOT)" \
	AI_CHECKER_CONFIG="$(AI_CHECKER_CONFIG)" \
	AI_CHECKER_PROJECTS="$(AI_CHECKER_PROJECTS)" \
	PYTHONPATH="$(AI_CHECKER_DIR)/src" \
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
AI_MERGE_ARGS = $(if $(DRY_RUN),--dry-run,) $(if $(DELETE),--delete,) \
                $(if $(KEY),--key "$(KEY)",)

ai-check:  ## AI review of all projects (LM Studio)
	@$(AI_RUN) src/check.py --all $(AI_CHECK_EXTRA)

ai-check-notest:  ## AI review, skip test/ dirs
	@$(AI_RUN) src/check.py --all --no-tests $(AI_CHECK_EXTRA)

ai-check-%:  ## AI review of one project
	@$(AI_RUN) src/check.py --project $* $(AI_CHECK_EXTRA)

ai-check-notest-%:  ## AI review of one project, skip test/
	@$(AI_RUN) src/check.py --project $* --no-tests $(AI_CHECK_EXTRA)

ai-fix:  ## dry-run: propose local fixes, all projects
	@$(AI_RUN) src/fix.py --all $(AI_FIX_ARGS)

ai-fix-%:  ## dry-run, one project
	@$(AI_RUN) src/fix.py --project $* $(AI_FIX_ARGS)

ai-fix-commit:  ## apply + one branch per fix, all projects
	@$(AI_RUN) src/fix.py --all $(AI_FIX_COMMIT)

ai-fix-commit-%:  ## apply + commit, one project
	@$(AI_RUN) src/fix.py --project $* $(AI_FIX_COMMIT)

ai-merge:  ## merge fixed branches, all projects
	@$(AI_RUN) src/merge.py --all $(AI_MERGE_ARGS)

ai-merge-%:  ## merge fixed branches of one project
	@$(AI_RUN) src/merge.py --project $* $(AI_MERGE_ARGS)

ai-merge-sync:  ## reconcile only: record hand-merged branches
	@$(AI_RUN) src/merge.py --all --sync-only

ai-status:  ## per-project status summary from check.db
	@$(AI_RUN) src/status.py $(AI_STATUS_ARGS)

ai-status-%:  ## list issues of one project
	@$(AI_RUN) src/status.py --project $* --list $(AI_STATUS_ARGS)

ai-runs:  ## recent LLM runs (llmruns table)
	@$(AI_RUN) src/status.py --runs $(AI_STATUS_ARGS)

ai-config:  ## list tunable params from check.db
	@$(AI_RUN) src/params.py $(AI_CONFIG_ARGS)

ai-config-set:  ## set param: MODULE= NAME= VALUE=
	@$(AI_RUN) src/params.py set $(MODULE) $(NAME) $(VALUE)

ai-del-branches:  ## delete merged ai-checker-* branches
	@$(AI_RUN) src/gitutil.py --all $(DEL_BRANCHES_ARGS)

ai-del-branches-%:  ## same, one project
	@$(AI_RUN) src/gitutil.py --project $* $(DEL_BRANCHES_ARGS)

ai-reject:  ## reject finding: KEY=runid/chunk/local [REASON=...]
	@$(AI_RUN) src/decide.py --status rejected $(if $(REASON),--reason "$(REASON)",) $(KEY)

ai-models:  ## list models loaded in LM Studio
	@LMSTUDIO_URL="$(LMSTUDIO_URL)" uv run python check.py --models

ai-arch:  ## pack reports into ai-checker-reports.tar.zst
	@test -d $(REPORTS_DIR) || { echo "nothing to archive"; exit 0; }
	@tar -C $(ROOT)/build --zstd -cf "$(REPORTS_TAR)" ai-checker

.PHONY: ai-help _ai_targets

ai-help:  ## full AI help (knobs, examples, env vars)
	@echo "ai-checker — AI review / fix / status pipeline"
	@echo ""
	@grep -hE '^ai-[a-zA-Z0-9_-]+:.*##' $(MAKEFILE_LIST) | \
	    sed 's/:.*## /\t/' | sort -u | \
	    awk -F'\t' '{gsub(/%/,"<n>",$$1); printf "  make %-28s %s\n",$$1,$$2}'
	@echo ""
	@echo "Knobs:"
	@echo "  FIXLIST=\"1/1, 1/7\"       only these findings"
	@echo "  AI_FIX_LIMIT=N           cap (default 50)"
	@echo "  AI_FIX_REFETCH=1         ignore cached proposal"
	@echo "  AI_FIX_REPROCESS=1       revisit terminal statuses"
	@echo "  AI_STATUS_ARGS='--status fixed --list'"
	@echo "  AI_CONFIG_ARGS=--long"
	@echo "  FORCE=1 DRY_RUN=1        (ai-del-branches)"
	@echo "  LMSTUDIO_URL=... QWEN_MODEL=..."

# Machine-readable target list for shell completion.
# Concrete ai-* targets + per-project expansion. Projects come from
# AI_PROJECTS (set by the embedding Makefile); if empty, ask workspace.
_ai_targets:
	@grep -hE '^ai-[a-zA-Z0-9_-]+:' $(MAKEFILE_LIST) | \
	    sed 's/:.*//' | sort -u | grep -v '^_'
	@projs="$(AI_PROJECTS)"; \
	 if [ -z "$$projs" ]; then \
	   projs=$$(cd "$(AI_CHECKER_DIR)" && PYTHONPATH=src \
	   	  uv run python -c \
	     "import workspace; print(' '.join(workspace.project_names() or []))" \
	     2>/dev/null); \
	 fi; \
	 for p in $$projs; do \
	   for t in ai-check ai-check-notest ai-check-reset ai-fix \
	            ai-fix-commit ai-status ai-del-branches; do \
	     echo "$$t-$$p"; \
	   done; \
	 done