# Dispatcher Makefile for ngutils/*

MAKEFLAGS += --no-print-directory

PROJECTS        := $(patsubst %/Makefile,%,$(wildcard */Makefile))
PROJECTS 		:= $(filter-out skeleton,$(PROJECTS))

FORWARD_TARGETS := all test check analyze

BUILD_DEBUG    	:= build/debug
BUILD_RELEASE  	:= build/release

NGUTILS_LIB         := $(BUILD_DEBUG)/libngutils.a
NGUTILS_LIB_RELEASE := $(BUILD_RELEASE)/libngutils.a

.PHONY: help list listall docs build-docker test-docker lib librelease install install-release clean \
        $(FORWARD_TARGETS) \
        $(foreach t,$(FORWARD_TARGETS),$(addprefix $(t)-,$(PROJECTS))) \
        $(addprefix lib-,$(PROJECTS)) \
	$(addprefix librelease-,$(PROJECTS))

help:
	@echo "ngutils dispatcher"
	@echo ""
	@echo "  make list             list discovered projects"
	@echo "  make all              run lib + test + librelease in all projects"
	@echo "  make lib              per-project debug .a + build/debug/libngutils.a"
	@echo "  make librelease       per-project release .a + build/release/libngutils.a"
	@echo "  make test             run tests in all projects"
	@echo "  make check            run clang-tidy in all projects"
	@echo "  make analyze          run clang static analyzer in all projects"
	@echo "  make clean            clean all projects"
	@echo "  make install          install libngutils.a + headers to PREFIX"
	@echo "  make <target>-<name>  run one target in one project"
	@echo "  make docs             build doxygen HTML"
	@echo "  make build-docker     build the Linux dev image"
	@echo "  make test-docker      run all tests inside Docker (Linux)"
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
	@rm -rf build
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

$(addprefix clean-,$(PROJECTS)): clean-%:
	@echo "==> clean $*"
	@$(MAKE) -C $* clean

# --- docs ---

docs:
	@doxygen Doxyfile
	@echo "docs/html/index.html"

# --- Docker ---

DOCKER_IMAGE := ngutils-dev

build-docker:
	docker build -f Dockerfile.dev -t $(DOCKER_IMAGE) .

test-docker:
	@docker run --rm -v "$(CURDIR):/work" -w /work $(DOCKER_IMAGE) \
	    sh -c 'BUILD_DIR=/tmp/build REQUIRE_CRITERION=1 \
	           make clean && \
	           BUILD_DIR=/tmp/build REQUIRE_CRITERION=1 make test'

listall:
	@echo "# Generic targets:"
	@grep -E '^[a-zA-Z][a-zA-Z0-9_-]*:' Makefile | sed 's/:.*//' | sort -u | sed 's/^/  /'
	@echo ""
	@echo "# Per-project targets:"
	@for p in $(PROJECTS); do \
	  for t in $(FORWARD_TARGETS); do echo "  $$t-$$p"; done; \
	done

