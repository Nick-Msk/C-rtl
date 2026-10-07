# Dispatcher Makefile for ngutils/*
#
# Finds every immediate subdirectory that contains a Makefile and forwards
# the requested target to it. No need to edit this file when adding a new
# utility — just give it a Makefile with a `test` target.

PROJECTS := $(patsubst %/Makefile,%,$(wildcard */Makefile))
FORWARD_TARGETS := test clean check

MAKEFLAGS += --no-print-directory

.PHONY: help list docker-build test-docker \
        $(FORWARD_TARGETS) $(foreach t,$(FORWARD_TARGETS),$(addprefix $(t)-,$(PROJECTS)))

help:
	@echo "ngutils dispatcher"
	@echo ""
	@echo "Targets:"
	@echo "  make list             list discovered projects"
	@echo "  make test             run tests in all projects"
	@echo "  make clean            clean all projects"
	@echo "  make test-<project>   run tests in one project"
	@echo "  make docker-build     build the Linux dev image"
	@echo "  make test-docker      run all tests inside Docker (Linux)"
	@echo ""
	@echo "Projects:"
	@for p in $(PROJECTS); do echo "  - $$p"; done

list:
	@for p in $(PROJECTS); do echo "$$p"; done
	@echo "---"
	@for p in $(PROJECTS); do echo "test-$$p"; done

test:  $(addprefix test-,$(PROJECTS))
clean: $(addprefix clean-,$(PROJECTS))
check: $(addprefix check-,$(PROJECTS))

$(addprefix test-,$(PROJECTS)): test-%:
	@echo "==> test $*"
	@$(MAKE) -C $* test

$(addprefix clean-,$(PROJECTS)): clean-%:
	@echo "==> clean $*"
	@$(MAKE) -C $* clean

$(addprefix check-,$(PROJECTS)): check-%:
	@echo "==> check $*"
	@$(MAKE) -C $* check

# --- Docker ---

DOCKER_IMAGE := ngutils-dev

build-docker:
	docker build -f Dockerfile.dev -t $(DOCKER_IMAGE) .

test-docker:
	@docker run --rm -v "$(CURDIR):/work" -w /work $(DOCKER_IMAGE) \
	    sh -c 'BUILD_DIR=/tmp/build $(MAKE) clean && BUILD_DIR=/tmp/build $(MAKE) test'

no_targets__:
listall:
	@echo "# Generic targets:"
	@grep -E '^[a-zA-Z][a-zA-Z0-9_-]*:' Makefile | sed 's/:.*//' | sort -u | sed 's/^/  /'
	@echo ""
	@echo "# Per-project targets:"
	@for p in $(PROJECTS); do \
	  echo "  test-$$p"; \
	  echo "  clean-$$p"; \
	done

