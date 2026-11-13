# Dispatcher Makefile for ngutils/*

# ── GNU Make version guard ──────────────────────────────────────────────────
#
# 4.0+ is required: earlier versions have unreliable $(eval) inside
# $(foreach), different handling of multiline define/call, and no
# $(file ...). Apple's shipped Make 3.81 fails on our REQ_RULE and
# docker-test-recipe patterns.
#
# macOS users with Homebrew:
#     brew install make
#     export PATH="/opt/homebrew/opt/make/libexec/gnubin:$PATH"

MAKE_VERSION_MAJOR := $(firstword $(subst ., ,$(MAKE_VERSION)))
MAKE_VERSION_MINOR := $(word 2,$(subst ., ,$(MAKE_VERSION)))

ifeq ($(MAKE_VERSION_MAJOR),3)
  ifneq ($(shell test $(MAKE_VERSION_MINOR) -ge 80 && echo ok),ok)
    $(error GNU Make 3.80 or newer required; found $(MAKE_VERSION))
  endif
else ifneq ($(shell test $(MAKE_VERSION_MAJOR) -ge 4 && echo ok),ok)
  $(error GNU Make 3.81 or newer required; found $(MAKE_VERSION))
endif

MAKEFLAGS += --no-print-directory

PROJECTS        := $(patsubst %/Makefile,%,$(wildcard */Makefile))
PROJECTS 		:= $(filter-out skeleton,$(PROJECTS))

FORWARD_TARGETS := all test check analyze analyze-test

BUILD_DEBUG    	:= build/debug
BUILD_RELEASE  	:= build/release

NGUTILS_LIB         := $(BUILD_DEBUG)/libngutils.a
NGUTILS_LIB_RELEASE := $(BUILD_RELEASE)/libngutils.a

.PHONY: help list listall docs 											\
		build-docker test-docker clean-docker check-docker 				\
		delete-docker test-docker-leak test-docker-ub test-docker-all 	\
		lib librelease install install-release clean 					\
        $(FORWARD_TARGETS) 												\
        $(foreach t,$(FORWARD_TARGETS),$(addprefix $(t)-,$(PROJECTS))) 	\
        $(addprefix lib-,$(PROJECTS)) 									\
		$(addprefix librelease-,$(PROJECTS)) 							\
		asan_check asan_leak 											\
		ai-check ai-models ai-check-file ai-arch 						\
        $(addprefix ai-check-,$(PROJECTS)) 								\
		$(addprefix ai-check-notest-,$(PROJECTS)) 						\
		ai-fix ai-fix-file 												\
        $(addprefix ai-fix-,$(PROJECTS))								\
		ai-fix ai-fix-commit ai-fix-file ai-fix-commit-file 			\
        $(addprefix ai-fix-,$(PROJECTS)) 								\
        $(addprefix ai-fix-commit-,$(PROJECTS))

help:
	@echo "ngutils dispatcher"
	@echo ""
	@echo "  make list             		list discovered projects"
	@echo "  make all              		run lib + test + librelease in all projects"
	@echo "  make lib              		per-project debug .a + build/debug/libngutils.a"
	@echo "  make librelease       		per-project release .a + build/release/libngutils.a"
	@echo ""
	@echo "  make test             		run tests in all projects"
	@echo "  make check            		run clang-tidy in all projects"
	@echo "  make analyze          		run clang static analyzer in all projects"
	@echo "  make analyze-test     		run clang static analyzer for test suite in all projects"
	@echo ""
	@echo "  make ai-check         		AI review of all projects (LM Studio / ai)"
	@echo "  make ai-check-<n>     		AI review of one project (e.g. ai-check-fs2, ai-check-log)"
	@echo "  make ai-check-file    		AI review of one file (F=log/src/log.c)"
	@echo "  make ai-check-notest      	AI review of all projects, skip test/ dirs"
	@echo "  make ai-check-notest-<n>  	AI review of one project, skip test/ dirs (e.g. ai-check-notest-fs2)"
	@echo "  make ai-models        		list models available in LM Studio"
	@echo "  make ai-arch  		   		pack build/ai-checker/ into ai-checker-reports.tar.zst"
	@echo ""
	@echo "  make ai-fix              dry-run: propose local fixes for all projects"
	@echo "  make ai-fix-<n>          dry-run, one project (e.g. ai-fix-fs2)"
	@echo "  make ai-fix-file F=<p>   dry-run, one file's report"
	@echo "  make ai-fix-commit-<n>   apply + one branch per fix (ai-checker-<n>-fix<N>-<seq>)"
	@echo "  make ai-fix-commit       apply, all projects"
	@echo ""
	@echo "  FIXLIST=\"1/1, 1/7\"       process only these finding ids (from the report)"
	@echo "  AI_FIX_MINOR=1           include 'Minor' section (Мелочи)"
	@echo "  AI_FIX_YES=1             skip per-fix confirmation prompt"
	@echo "  AI_FIX_LIMIT=N           cap when FIXLIST is not set (default 50)"
	@echo ""
	@echo "  make clean            		clean all projects"
	@echo "  make install          		install libngutils.a + headers to PREFIX"
	@echo "  make <target>-<name>  		run one target in one project"
	@echo "  make docs             		build doxygen HTML"
	@echo ""
	@echo "  make build-docker     		build the Linux dev image"
	@echo "  make clean-docker     		delete the Linux dev image"
	@echo "  make check-docker     		verify docker CLI + daemon"
	@echo "  make test-docker      		run all tests inside Docker (Linux)"
	@echo "  make test-docker-ub   		ASan + UBSan inside the container (Linux)"
	@echo "  make test-docker-leak 		run ASan+LSan across all projects (Linux)"
	@echo "  make delete-docker    		remove the $(DOCKER_IMAGE) image"
	@echo "  make test-docker-all  		regular + ASan+UBSan + ASan+LSan in Docker"
	@echo ""
	@echo "  make cloneproj NAME=x  	create new utility from skeleton"
	@echo ""
	@echo "  make asan_check   			rebuild with ASan + UBSan (macOS + Linux)"
	@echo "  make asan_leak    			ASan + LSan (Linux only; error skipped)"
	@echo ""
	@echo "Projects:"
	@for p in $(PROJECTS); do echo "  - $$p"; done

list:
	@for p in $(PROJECTS); do echo "$$p"; done

PREFIX ?= /usr/local

install: lib
	@install -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include
	@install -m 644 $(NGUTILS_LIB) $(DESTDIR)$(PREFIX)/lib/
	@for p in $(PROJECTS); do \
	    install -m 644 $$p/include/*.h $(DESTDIR)$(PREFIX)/include/; \
	done
	@echo "Installed to $(DESTDIR)$(PREFIX)"

install-release: librelease
	@install -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include
	@install -m 644 $(NGUTILS_LIB_RELEASE) $(DESTDIR)$(PREFIX)/lib/libngutils.a
	@for p in $(PROJECTS); do \
	    install -m 644 $$p/include/*.h $(DESTDIR)$(PREFIX)/include/; \
	done
	@echo "Installed (release) to $(DESTDIR)$(PREFIX)"

# Aggregates (no recipe; recipe lives per-project)
$(FORWARD_TARGETS): %: $(addprefix %-,$(PROJECTS))

# lib is special: per-project archives, then combine into one

lib: $(addprefix lib-,$(PROJECTS))
	@mkdir -p $(dir $(NGUTILS_LIB))
	@ar rcs $(NGUTILS_LIB) $$(find */build/debug/obj -name '*.o' ! -name 'test_*' 2>/dev/null)
	@echo "Built: $(NGUTILS_LIB)"

librelease: $(addprefix librelease-,$(PROJECTS))
	@mkdir -p $(dir $(NGUTILS_LIB_RELEASE))
	@ar rcs $(NGUTILS_LIB_RELEASE) $$(find */build/release/obj -name '*.o' 2>/dev/null)
	@echo "Built: $(NGUTILS_LIB_RELEASE)"

clean: $(addprefix clean-,$(PROJECTS))
	@find build -mindepth 1 -maxdepth 1 \
		! -name ai-checker \
		! -name 'ai-checker-reports*' \
		-exec rm -rf {} + 2>/dev/null || true
	@echo "Cleaned: build (kept build/ai-checker*)"

# Static pattern rules
$(addprefix lib-,$(PROJECTS)): lib-%:
	@echo "==> lib $*"
	@$(MAKE) -C $* lib

$(addprefix librelease-,$(PROJECTS)): librelease-%:
	@echo "==> librelease $*"
	@$(MAKE) -C $* librelease

$(addprefix all-,$(PROJECTS)): all-%:
	@echo "==> all $*"
	@$(MAKE) -C $* all

$(addprefix test-,$(PROJECTS)): test-%:
	@echo "==> test $*"
	@$(MAKE) -C $* test

$(addprefix check-,$(PROJECTS)): check-%:
	@echo "==> check $*"
	@$(MAKE) -C $* check

$(addprefix analyze-,$(PROJECTS)): analyze-%:
	@echo "==> analyze $*"
	@$(MAKE) -C $* analyze

$(addprefix analyze-test-,$(PROJECTS)): analyze-test-%:
	@echo "==> analyze-test $*"
	@$(MAKE) -C $* analyze-test

$(addprefix clean-,$(PROJECTS)): clean-%:
	@echo "==> clean $*"
	@$(MAKE) -C $* clean

# --- AI review (LM Studio / Qwen, OpenAI-compatible API) ---
#
# Reports live in build/ai-checker/<project>/<YYYY-MM-DD_HH-MM-SS>[_notest].md
# The '_notest' suffix is added automatically when test/ dirs are excluded,
# so full and test-less reviews never collide.
# Reports are intentionally preserved by 'make clean'.
# 'make ai-checker-arch' packs them into build/ai-checker-reports.tar.zst.

AI_CHECKER    := ai-checker
LMSTUDIO_URL ?= http://127.0.0.1:1234
QWEN_MODEL   ?=
REPORTS_DIR  := build/ai-checker
REPORTS_TAR  := build/ai-checker-reports.tar.zst

ai-check:
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python check.py --all $(AI_CHECK_EXTRA)

ai-check-notest:
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python check.py --all --no-tests $(AI_CHECK_EXTRA)

$(addprefix ai-check-,$(PROJECTS)): ai-check-%:
	@echo "==> ai-check $*"
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python check.py --project $* $(AI_CHECK_EXTRA)

$(addprefix ai-check-notest-,$(PROJECTS)): ai-check-notest-%:
	@echo "==> ai-check-notest $*"
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python check.py --project $* --no-tests $(AI_CHECK_EXTRA)

ai-check-file:
	@test -n "$(F)" || { echo "usage: make ai-check-file F=<path>" >&2; exit 1; }
	@test -f "$(F)" || { echo "ERROR: file not found: $(F)" >&2; exit 1; }
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python check.py --file "$(CURDIR)/$(F)" $(AI_CHECK_EXTRA)

ai-models:
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" \
		uv run python check.py --models

ai-checker-arch:
	@test -d $(REPORTS_DIR) || { echo "nothing to archive: $(REPORTS_DIR) missing"; exit 0; }
	@mkdir -p build
	@tar -C build --zstd -cf "$(CURDIR)/$(REPORTS_TAR)" ai-checker
	@echo "Archived: $(REPORTS_TAR) ($$(du -h "$(REPORTS_TAR)" | cut -f1))"

# --- AI fix (applies only self-contained local fixes from the latest report) ---
#
# Usage:
#   make ai-fix-fs2                         dry-run, all findings
#   make ai-fix-fs2 FIXLIST="1/1, 1/7"      dry-run, only those ids
#   make ai-fix-fs2 AI_FIX_MINOR=1          dry-run, include Мелочи
#
#   make ai-fix-commit-fs2                  apply, one branch per fix
#   make ai-fix-commit-fs2 FIXLIST="1/1"    apply only that one
#   make ai-fix-commit-fs2 AI_FIX_YES=1     apply without prompts
#
# Each accepted fix gets its own branch ai-checker-<proj>-fix<N>-<seq>,
# one commit on top of the current HEAD. Review/inspect/merge/delete per
# branch; cherry-pick is not needed.
#
# Refuses: cross-file edits, signature changes, new functions, struct
# changes, public renames, new #includes, edits spanning more than one
# function body, diffs > 20 lines. Everything else is listed as
# "needs human thought" with a direction, never auto-applied.

AI_FIXER      := fix.py
FIXLIST       ?=
AI_FIX_MINOR  ?=
AI_FIX_YES    ?=
AI_FIX_LIMIT  ?=

AI_FIX_BASE    = $(if $(FIXLIST),--only "$(FIXLIST)",) \
                 $(if $(AI_FIX_MINOR),--include-minor,) \
                 $(if $(AI_FIX_LIMIT),--limit $(AI_FIX_LIMIT),)
AI_FIX_ARGS    = $(AI_FIX_BASE)
AI_FIX_COMMIT  = $(AI_FIX_BASE) --apply $(if $(AI_FIX_YES),--yes,)

ai-fix:
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python $(AI_FIXER) --all $(AI_FIX_ARGS)

ai-fix-commit:
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python $(AI_FIXER) --all $(AI_FIX_COMMIT)

$(addprefix ai-fix-,$(PROJECTS)): ai-fix-%:
	@echo "==> ai-fix $*"
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python $(AI_FIXER) --project $* $(AI_FIX_ARGS)

$(addprefix ai-fix-commit-,$(PROJECTS)): ai-fix-commit-%:
	@echo "==> ai-fix-commit $*"
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python $(AI_FIXER) --project $* $(AI_FIX_COMMIT)

ai-fix-file:
	@test -n "$(F)" || { echo "usage: make ai-fix-file F=<path>" >&2; exit 1; }
	@test -f "$(F)" || { echo "ERROR: file not found: $(F)" >&2; exit 1; }
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python $(AI_FIXER) --file "$(CURDIR)/$(F)" $(AI_FIX_ARGS)

ai-fix-commit-file:
	@test -n "$(F)" || { echo "usage: make ai-fix-commit-file F=<path>" >&2; exit 1; }
	@test -f "$(F)" || { echo "ERROR: file not found: $(F)" >&2; exit 1; }
	@cd $(CURDIR)/$(AI_CHECKER) && \
		LMSTUDIO_URL="$(LMSTUDIO_URL)" QWEN_MODEL="$(QWEN_MODEL)" \
		uv run python $(AI_FIXER) --file "$(CURDIR)/$(F)" $(AI_FIX_COMMIT)
		
# --- docs ---

docs:
	@doxygen Doxyfile
	@echo "docs/html/index.html"

# --- Docker ---

DOCKER 				?= docker
DOCKER_IMAGE 		:= ngutils-dev
DOCKER_BUILD_DIR 	:= /tmp/ngutils-docker-build
# Strip jobserver flags that are host-specific and meaningless in a container
DOCKER_MAKEFLAGS 	 = $(filter-out --jobserver%,$(MAKEFLAGS))

# --- docker helpers ---
# Check that the Docker CLI is present and the daemon responds.
check-docker:
	@command -v $(DOCKER) >/dev/null 2>&1 || { \
	    echo "ERROR: docker CLI not found in PATH" >&2; \
	    echo "  macOS:  brew install --cask docker  (or: brew install colima)" >&2; \
	    echo "  Debian: sudo apt install docker.io" >&2; \
	    echo "  Fedora: sudo dnf install docker" >&2; \
	    exit 1; \
	}
	@$(DOCKER) info >/dev/null 2>&1 || { \
	    echo "ERROR: docker daemon is not running" >&2; \
	    echo "  macOS:  open -a Docker   (wait until the menu bar icon stops animating)" >&2; \
	    echo "  Linux:  sudo systemctl start docker" >&2; \
	    exit 1; \
	}

build-docker: check-docker
	$(DOCKER) build -f Dockerfile.dev -t $(DOCKER_IMAGE) .

# Shared recipe for docker-based test runs.
#
# $(1) and $(2) are labels only — they appear in the echo for humans,
#      nothing else uses them.
# $(3) is the env prefix passed to the inner 'make test' in the
#      container (SANITIZE=..., BUILD_DIR=..., REQUIRE_CRITERION=1).
define docker-test-recipe
	@if [ $(words $(PROJECTS)) -gt 100 ]; then \
	    echo "too many projects for a single docker run" >&2; \
	    exit 1; \
	fi
	@mkdir -p $(DOCKER_BUILD_DIR)
	@$(foreach p,$(PROJECTS),mkdir -p $(DOCKER_BUILD_DIR)/$(p);)
	@$(DOCKER) run --rm \
		-e MAKEFLAGS="$(DOCKER_MAKEFLAGS)" \
		-v "$(CURDIR):/work" \
		$(foreach p,$(PROJECTS),-v $(DOCKER_BUILD_DIR)/$(p):/work/$(p)/build) \
		-v $(DOCKER_BUILD_DIR):/work/build \
		-w /work $(DOCKER_IMAGE) \
		sh -c '$(3) make test'
endef

test-docker-all: check-docker
	@$(MAKE) test-docker
	@$(MAKE) test-docker-ub
	@$(MAKE) test-docker-leak

test-docker: check-docker
	$(call docker-test-recipe,build,,REQUIRE_CRITERION=1)

test-docker-ub: check-docker
	$(call docker-test-recipe,build/asan-ubsan,asan,SANITIZE=asan BUILD_DIR=build/asan-ubsan REQUIRE_CRITERION=1)

test-docker-leak: check-docker
	$(call docker-test-recipe,build/asan-lsan,lsan,SANITIZE=lsan BUILD_DIR=build/asan-lsan REQUIRE_CRITERION=1)

clean-docker:
	@rm -rf $(DOCKER_BUILD_DIR)
	@echo "Cleaned: $(DOCKER_BUILD_DIR)"

delete-docker:
	@running=$$($(DOCKER) ps -q --filter ancestor=$(DOCKER_IMAGE)); \
	 if [ -n "$$running" ]; then \
	    echo "ERROR: running containers use $(DOCKER_IMAGE)" >&2; \
	    echo "       stop them first: docker stop $$running" >&2; \
	    exit 1; \
	 fi
	@$(DOCKER) image rm $(DOCKER_IMAGE) 2>/dev/null || true
	@echo "Deleted: $(DOCKER_IMAGE)"

listall:
	@echo "# Generic targets:"
	@grep -E '^[a-zA-Z][a-zA-Z0-9_-]*:' Makefile | sed 's/:.*//' | sort -u | sed 's/^/  /'
	@echo ""
	@echo "# Per-project targets:"
	@for p in $(PROJECTS); do \
	  for t in $(FORWARD_TARGETS); do echo "  $$t-$$p"; done; \
	done

cloneproj:
	@test -n "$(NAME)" || { echo "usage: make cloneproj NAME=<name>" >&2; exit 1; }
	@scripts/cloneproj.sh $(NAME)

# Projects excluded from leak checks — error has .signal tests that
# abort before teardown, which corrupts the leak report.
LEAK_CHECK_SKIP := error

LEAK_PROJECTS := $(filter-out $(LEAK_CHECK_SKIP),$(PROJECTS))

asan_check:
	@echo "==> asan_check: all projects"
	@SANITIZE=asan BUILD_DIR=build/asan-ubsan $(MAKE) test

asan_leak:
	@if [ "$$(uname)" = "Darwin" ]; then \
	    echo "SKIP: LeakSanitizer not available on macOS"; \
	    echo "      Use Linux or:  make test-docker"; \
	else \
	    echo "==> asan_leak: all projects"; \
	    for p in $(LEAK_PROJECTS); do \
	        echo "--- $$p ---"; \
	        $(MAKE) -C $$p asan_leak || exit 1; \
	    done; \
	fi

