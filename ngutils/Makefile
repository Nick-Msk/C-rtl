# Dispatcher Makefile for ngutils/*

MAKEFLAGS += --no-print-directory

PROJECTS        := $(patsubst %/Makefile,%,$(wildcard */Makefile))
FORWARD_TARGETS := all lib test check analyze clean

.PHONY: help list listall docs docker-build test-docker \
        $(FORWARD_TARGETS) \
        $(foreach t,$(FORWARD_TARGETS),$(addprefix $(t)-,$(PROJECTS)))

help:
	@echo "ngutils dispatcher"
	@echo ""
	@echo "  make list             list discovered projects"
	@echo "  make all              build object files in all projects"
	@echo "  make lib              build static archives in all projects"
	@echo "  make test             run tests in all projects"
	@echo "  make check            run clang-tidy in all projects"
	@echo "  make analyze          run clang static analyzer in all projects"
	@echo "  make clean            clean all projects"
	@echo "  make <target>-<name>  run one target in one project"
	@echo "  make docs             build doxygen HTML"
	@echo "  make build-docker     build the Linux dev image"
	@echo "  make test-docker      run all tests inside Docker (Linux)"
	@echo ""
	@echo "Projects:"
	@for p in $(PROJECTS); do echo "  - $$p"; done

list:
	@for p in $(PROJECTS); do echo "$$p"; done

listall:
	@echo "# Generic targets:"
	@grep -E '^[a-zA-Z][a-zA-Z0-9_-]*:' Makefile | sed 's/:.*//' | sort -u | sed 's/^/  /'
	@echo ""
	@echo "# Per-project targets:"
	@for p in $(PROJECTS); do \
	  for t in $(FORWARD_TARGETS); do echo "  $$t-$$p"; done; \
	done

# Aggregates
$(FORWARD_TARGETS): %: $(addprefix %-,$(PROJECTS))

# Static pattern rules
$(addprefix all-,$(PROJECTS)): all-%:
	@echo "==> all $*"
	@$(MAKE) -C $* all

$(addprefix lib-,$(PROJECTS)): lib-%:
	@echo "==> lib $*"
	@$(MAKE) -C $* lib

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
	    sh -c 'BUILD_DIR=/tmp/build $(MAKE) clean && BUILD_DIR=/tmp/build $(MAKE) test'

