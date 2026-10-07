CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude
CRIT_CFLAGS = $(shell pkg-config --cflags criterion)
CRIT_LIBS   = $(shell pkg-config --libs criterion)

BUILD_DIR ?= build
BUILD   := $(BUILD_DIR)

LOG		= log
DEBUG   = $(BUILD)/debug
RELEASE = $(BUILD)/release

CLANG_TIDY ?= clang-tidy
SCAN_BUILD ?= scan-build

TEST_SRC = test/test_bool.c
TEST_BIN = $(DEBUG)/test_bool

SCR_INCLUDE = include/bool.h

# --- test build (debug flags) ---
$(TEST_BIN): $(TEST_SRC) $(SCR_INCLUDE) | $(DEBUG)
	$(CC) $(CFLAGS) $(CRIT_CFLAGS) -g -O0 -o $@ $(TEST_SRC) $(CRIT_LIBS)

$(DEBUG) $(RELEASE):
	@mkdir -p $@

.PHONY: test clean check

# --- tasks ---
test: $(TEST_BIN)
	@$(TEST_BIN)

analyze:
	@mkdir -p $(BUILD_DIR)/analyze
	@rm -rf $(BUILD_DIR)/debug
	@$(SCAN_BUILD) --status-bugs -o $(BUILD_DIR)/analyze \
	    $(MAKE) test

check:
	$(CLANG_TIDY) $(TEST_SRC) -- $(CFLAGS) $(CRIT_CFLAGS)

# stub
release: | $(RELEASE)
	@echo "release build not implemented yet"

clean:
	@rm -rf $(BUILD)
	@rm -rf $(LOG)/*.log



.PHONY: test release clean

no_targets__:
list:
	sh -c "$(MAKE) -p no_targets__ | awk -F':' '/^[a-zA-Z0-9][^\$$#\/\\t=]*:([^=]|$$)/ {split(\$$1,A,/ /);for(i in A)print A[i]}' | grep -v '__\$$' | sort"
