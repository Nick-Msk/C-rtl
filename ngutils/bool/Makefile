CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib
LDLIBS  = -lcriterion

BUILD   = build
DEBUG   = $(BUILD)/debug
RELEASE = $(BUILD)/release

TEST_SRC = test/test_bool.c
TEST_BIN = $(DEBUG)/test_bool

# --- test build (debug flags) ---
$(TEST_BIN): $(TEST_SRC) include/bool.h | $(DEBUG)
	$(CC) $(CFLAGS) -g -O0 -o $@ $(TEST_SRC) $(LDFLAGS) $(LDLIBS)

$(DEBUG) $(RELEASE):
	mkdir -p $@

# --- tasks ---
test: $(TEST_BIN)
	./$(TEST_BIN)

# заготовка на будущее: релизная сборка библиотеки
release: | $(RELEASE)
	@echo "release build not implemented yet"

clean:
	rm -rf $(BUILD)

.PHONY: test release clean

no_targets__:
list:
	sh -c "$(MAKE) -p no_targets__ | awk -F':' '/^[a-zA-Z0-9][^\$$#\/\\t=]*:([^=]|$$)/ {split(\$$1,A,/ /);for(i in A)print A[i]}' | grep -v '__\$$' | sort"

