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
AI_MERGE_ARGS = $(if $(DRY_RUN),--dry-run,) $(if $(KEEP),--keep,) \
                $(if $(KEY),--key "$(KEY)",)

AI_DECIDE_ARGS = $(if $(FIXLIST),--only "$(FIXLIST)",) \
                 $(if $(REASON),--reason "$(REASON)",) \
                 $(if $(ALL_PROJECTS),--all-projects,)

# ── transition policy ───────────────────────────────────────────────
# ACTOR=<name>                    -> flip EVERY transition of the actor
# ACTOR + STATUS + NEW_STATUS     -> flip exactly ONE transition
AI_TRANS_ARGS = $(if $(ACTOR),--actor "$(ACTOR)",) \
                $(if $(STATUS),--status "$(STATUS)",) \
                $(if $(NEW_STATUS),--new-status "$(NEW_STATUS)",) \
                $(if $(ENABLED),--enabled-only,)

define require_actor
	@test -n "$(ACTOR)" || { \
	    echo "ERROR: ACTOR is required for $@"; \
	    echo "usage: make $@ ACTOR=<name> [STATUS=<src> NEW_STATUS=<dst>]"; \
	    echo "       actors/transitions: make ai-config-transitions"; \
	    exit 1; \
	}
endef

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

ai-fix-propose:  ## force-patch analyze-by-dev/no-action, all projects
	@test -n "$(FIXLIST)" || { \
	    echo "ERROR: FIXLIST is required for ai-fix-propose"; \
	    echo "usage: make ai-fix-propose FIXLIST=\"<runid>/<chunk>/<local>\""; \
	    echo "       (see 'make ai-status-<proj>' for eligible keys)"; \
	    exit 1; \
	}
	@$(AI_RUN) src/fix.py --all --propose $(AI_FIX_ARGS)

ai-fix-propose-%:  ## dry-run force-patch for one project
	@test -n "$(FIXLIST)" || { \
	    echo "ERROR: FIXLIST is required for ai-fix-propose-$*"; \
	    echo "usage: make ai-fix-propose-$* FIXLIST=\"<runid>/<chunk>/<local>\""; \
	    echo "       (see 'make ai-status-$*' for eligible keys)"; \
	    exit 1; \
	}
	@$(AI_RUN) src/fix.py --project $* --propose $(AI_FIX_ARGS)

ai-fix-propose-commit-%:  ## force-patch + commit for one project
	@test -n "$(FIXLIST)" || { \
	    echo "ERROR: FIXLIST is required for ai-fix-propose-commit-$*"; \
	    echo "usage: make ai-fix-propose-commit-$* FIXLIST=\"<runid>/<chunk>/<local>\""; \
	    exit 1; \
	}
	@$(AI_RUN) src/fix.py --project $* --propose --apply $(AI_FIX_ARGS)

ai-fix-forcepropose:  ## force-propose every project (analyze-by-dev + no-action)
	@$(AI_RUN) src/fix.py --all --propose --force-propose $(AI_FIX_ARGS)

ai-fix-forcepropose-%:  ## force-propose one project (analyze-by-dev + no-action)
	@$(AI_RUN) src/fix.py --project $* --propose --force-propose $(AI_FIX_ARGS)

ai-merge:  ## merge fixed branches, all projects
	@$(AI_RUN) src/merge.py --all $(AI_MERGE_ARGS)

ai-merge-sync:  ## reconcile only, every project (never git merge)
	@$(AI_RUN) src/merge.py --all --sync-only $(AI_MERGE_ARGS)

ai-merge-sync-%:  ## reconcile only, one project
	@$(AI_RUN) src/merge.py --project $* --sync-only $(AI_MERGE_ARGS)

ai-merge-%:  ## merge fixed issues of one project (FIXLIST optional)
	@$(AI_RUN) src/merge.py --project $* $(AI_MERGE_ARGS)

ai-status-unmerged:  ## fixed findings awaiting merge, every project
	@$(AI_RUN) src/status.py --unmerged

ai-status-unmerged-%:  ## fixed findings awaiting merge, one project
	@$(AI_RUN) src/status.py --project $* --unmerged

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

ai-config-transitions:  ## show status machine (ACTOR=, STATUS=, ENABLED=1)
	@$(AI_RUN) src/params.py --transitions $(AI_TRANS_ARGS)

ai-config-disable:  ## disable an actor or one transition (ACTOR=, STATUS=, NEW_STATUS=)
	$(require_actor)
	@$(AI_RUN) src/params.py $(if $(STATUS),--set-allowed n $(AI_TRANS_ARGS),--actor-off "$(ACTOR)")

ai-config-enable:  ## enable an actor or one transition
	$(require_actor)
	@$(AI_RUN) src/params.py $(if $(STATUS),--set-allowed y $(AI_TRANS_ARGS),--actor-on "$(ACTOR)")
	
ai-config-save:  ## dump DB-owned config as SQL inserts (FILENAME=...)
	@$(AI_RUN) src/params.py --save $(if $(FILENAME),"$(abspath $(FILENAME))",)

ai-config-restore:  ## apply a config dump (FILENAME=...)
	@$(AI_RUN) src/params.py --restore $(if $(FILENAME),"$(abspath $(FILENAME))",)

ai-config-reset-transitions:  ## restore every allowed='y'
	@$(AI_RUN) src/params.py --reset-transitions

ai-del-branches:  ## delete merged ai-checker-* branches
	@$(AI_RUN) src/gitutil.py --all $(DEL_BRANCHES_ARGS)

ai-del-branches-%:  ## same, one project
	@$(AI_RUN) src/gitutil.py --project $* $(DEL_BRANCHES_ARGS)

ai-reject:  ## reject issues: FIXLIST=93/1/2 [REASON=...]
	@$(AI_RUN) src/decide.py --status rejected $(AI_DECIDE_ARGS)

ai-reject-%:  ## reject issues of one project
	@$(AI_RUN) src/decide.py --status rejected --project $* $(AI_DECIDE_ARGS)

ai-no-action:  ## mark not-a-defect: FIXLIST=...
	@$(AI_RUN) src/decide.py --status no-action $(AI_DECIDE_ARGS)

ai-no-action-%:  ## same, one project
	@$(AI_RUN) src/decide.py --status no-action --project $* $(AI_DECIDE_ARGS)

ai-reopen:  ## reopen issues (-> added): FIXLIST=...
	@$(AI_RUN) src/decide.py --status added $(AI_DECIDE_ARGS)

ai-reopen-%:  ## same, one project
	@$(AI_RUN) src/decide.py --status added --project $* $(AI_DECIDE_ARGS)
	
ai-models:  ## list models loaded in LM Studio
	@LMSTUDIO_URL="$(LMSTUDIO_URL)" uv run python check.py --models

ai-arch:  ## pack reports into ai-checker-reports.tar.zst
	@test -d $(REPORTS_DIR) || { echo "nothing to archive"; exit 0; }
	@tar -C $(ROOT)/build --zstd -cf "$(REPORTS_TAR)" ai-checker

ai-mergearea-clean:  ## remove baselines with no pending work
	@$(AI_RUN) src/gitutil.py --clean-baselines

ai-clean:  ## nuke mergearea + branches of EVERY project (logs preserved)
	@$(AI_RUN) src/gitutil.py --clean --all $(if $(DRY_RUN),--dry-run,)

ai-clean-%:  ## nuke mergearea + branches of one project (logs preserved)
	@$(AI_RUN) src/gitutil.py --clean --project $* $(if $(DRY_RUN),--dry-run,)

ai-push:  ## push findings of EVERY project to the tracker
	@$(AI_RUN) src/tracker_common.py --all $(if $(DRY_RUN),--dry-run,) $(if $(FORCE),--force,)

ai-push-%:  ## push findings of one project (DRY_RUN=1, FORCE=1)
	@$(AI_RUN) src/tracker_common.py --project $* $(if $(DRY_RUN),--dry-run,) $(if $(FORCE),--force,)

install-completion:  ## install shell completion (auto-detects zsh/bash)
	$(if $(SHELL_KIND),--shell=$(SHELL_KIND),)
	@bash scripts/install-completion.sh

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
