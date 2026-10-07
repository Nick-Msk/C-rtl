CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude
CRIT_CFLAGS = $(shell pkg-config --cflags criterion)
CRIT_LIBS   = $(shell pkg-config --libs criterion)

CLANG_TIDY ?= clang-tidy
SCAN_BUILD ?= scan-build

BUILD_DIR ?= build
BUILD   := $(BUILD_DIR)
LOG		= logs
DEBUG   = $(BUILD)/debug
RELEASE = $(BUILD)/release
OBJ      = $(BUILD_DIR)/obj

SRC 	 = src/bool.c
TEST_SRC = test/test_bool.c
HDR      = include/bool.h

SRC_OBJ       = $(OBJ)/bool.o
TEST_OBJ      = $(OBJ)/test_bool.o
TEST_BIN      = $(DEBUG)/test_bool
DEPS          = $(SRC_OBJ:.o=.d) $(TEST_OBJ:.o=.d)

# --- compile ---

$(SRC_OBJ): $(SRC) $(HDR) | $(OBJ)
	$(CC) $(CFLAGS) -g -O0 -MMD -MP -c $< -o $@

$(TEST_OBJ): $(TEST_SRC) $(HDR) | $(OBJ)
	$(CC) $(CFLAGS) $(CRIT_CFLAGS) -g -O0 -MMD -MP -c $< -o $@

$(TEST_BIN): $(SRC_OBJ) $(TEST_OBJ) | $(DEBUG)
	$(CC) -o $@ $(SRC_OBJ) $(TEST_OBJ) $(CRIT_LIBS)

$(OBJ) $(DEBUG) $(RELEASE):
	@mkdir -p $@

-include $(DEPS)

# --- tasks ---

.PHONY: test check analyze release clean clean-logs

test: $(TEST_BIN)
	@$(TEST_BIN)

check:
	$(CLANG_TIDY) $(TEST_SRC) -- $(CFLAGS) $(CRIT_CFLAGS)

analyze:
	@mkdir -p $(BUILD_DIR)/analyze
	@rm -rf $(DEBUG) $(OBJ)
	@$(SCAN_BUILD) --status-bugs -o $(BUILD_DIR)/analyze $(MAKE) test

# stub
release: | $(RELEASE)
	@echo "release build not implemented yet"

clean:
	@rm -rf $(BUILD)
	@rm -rf $(LOG)/*.log

clean-logs:
	@rm -f logs/*.log

no_targets__:
list:
	sh -c "$(MAKE) -p no_targets__ | awk -F':' '/^[a-zA-Z0-9][^\$$#\/\\t=]*:([^=]|$$)/ {split(\$$1,A,/ /);for(i in A)print A[i]}' | grep -v '__\$$' | sort"
