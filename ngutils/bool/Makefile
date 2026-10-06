CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude
CRIT_CFLAGS = $(shell pkg-config --cflags criterion)
CRIT_LIBS   = $(shell pkg-config --libs criterion)

BUILD_DIR ?= build
BUILD   := $(BUILD_DIR)

LOG		= log
DEBUG   = $(BUILD)/debug
RELEASE = $(BUILD)/release

TEST_SRC = test/test_bool.c
TEST_BIN = $(DEBUG)/test_bool

# --- test build (debug flags) ---
$(TEST_BIN): $(TEST_SRC) include/bool.h | $(DEBUG)
	$(CC) $(CFLAGS) $(CRIT_CFLAGS) -g -O0 -o $@ $(TEST_SRC) $(CRIT_LIBS)

$(DEBUG) $(RELEASE):
	mkdir -p $@

# --- tasks ---
test: $(TEST_BIN)
	$(TEST_BIN)

# stub
release: | $(RELEASE)
	@echo "release build not implemented yet"

clean:
	rm -rf $(BUILD)
	rm -rf $(LOG)/*.log

.PHONY: test release clean

no_targets__:
list:
	sh -c "$(MAKE) -p no_targets__ | awk -F':' '/^[a-zA-Z0-9][^\$$#\/\\t=]*:([^=]|$$)/ {split(\$$1,A,/ /);for(i in A)print A[i]}' | grep -v '__\$$' | sort"
