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
