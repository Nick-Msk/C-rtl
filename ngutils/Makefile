# Dispatcher Makefile for ngutils/*

MAKEFLAGS += --no-print-directory

PROJECTS        := $(patsubst %/Makefile,%,$(wildcard */Makefile))
PROJECTS 		:= $(filter-out skeleton,$(PROJECTS))

FORWARD_TARGETS := all test check analyze analyze-test

BUILD_DEBUG    	:= build/debug
BUILD_RELEASE  	:= build/release

NGUTILS_LIB         := $(BUILD_DEBUG)/libngutils.a
NGUTILS_LIB_RELEASE := $(BUILD_RELEASE)/libngutils.a

.PHONY: help list listall docs \
		build-docker test-docker clean-docker check-docker delete-docker test-docker-leak test-docker-ub \
		lib librelease install install-release clean \
        $(FORWARD_TARGETS) \
        $(foreach t,$(FORWARD_TARGETS),$(addprefix $(t)-,$(PROJECTS))) \
        $(addprefix lib-,$(PROJECTS)) \
		$(addprefix librelease-,$(PROJECTS)) \
		asan_check asan_leak

help:
	@echo "ngutils dispatcher"
	@echo ""
	@echo "  make list             list discovered projects"
	@echo "  make all              run lib + test + librelease in all projects"
	@echo "  make lib              per-project debug .a + build/debug/libngutils.a"
	@echo "  make librelease       per-project release .a + build/release/libngutils.a"
	@echo ""
	@echo "  make test             run tests in all projects"
	@echo "  make check            run clang-tidy in all projects"
	@echo "  make analyze          run clang static analyzer in all projects"
	@echo "  make analyze-test     run clang static analyzer for test suite in all projects"
	@echo "  make clean            clean all projects"
	@echo "  make install          install libngutils.a + headers to PREFIX"
	@echo "  make <target>-<name>  run one target in one project"
	@echo "  make docs             build doxygen HTML"
	@echo ""
	@echo "  make build-docker     build the Linux dev image"
	@echo "  make clean-docker     delete the Linux dev image"
	@echo "  make check-docker     verify docker CLI + daemon"
	@echo "  make test-docker      run all tests inside Docker (Linux)"
	@echo "  make test-docker-ub   ASan + UBSan inside the container (Linux)"
	@echo "  make test-docker-leak run ASan+LSan across all projects (Linux)"
	@echo "  make delete-docker    remove the $(DOCKER_IMAGE) image"
	@echo ""
	@echo "  make cloneproj NAME=x  create new utility from skeleton"
	@echo ""
	@echo "  make asan_check   rebuild with ASan + UBSan (macOS + Linux)"
	@echo "  make asan_leak    ASan + LSan (Linux only; error skipped)"
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
	@find build -mindepth 1 -exec rm -rf {} + 2>/dev/null || true
	@echo "Cleaned: build"

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
# Usage: $(call docker-test-recipe,<build_dir>,<sanitize>,<env_prefix>)
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

