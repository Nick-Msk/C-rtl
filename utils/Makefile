CC=gcc #clang
CFLAGS= -I./ -Wall -Wextra -g -O0 -Wno-initializer-overrides -Wno-gnu-folding-constant -std=c23 -fsanitize=address  -MD -MP
#CFLAGS += -fanalyzer

# -Wno-initializer-overrides -Wno-unused-function -Wno-format-zero-length
LDFLAGS=
LDLIBS=

.PHONY: clean cleanlog cleanall no_targets__ list
.PHONY: checker error log common test metric numeric_ops context hashset value64 array buffer fs fs_iter fs_array
.PHONY: getword systemutils command_executor fileutils value64_tarray seq all hashset_utils lwset
.PHONY: ds ds_nofs ds_adapter v64gen v64typed

all: checker error log common test metric numeric_ops context hashset value64 array buffer fs fs_iter fs_array \
	getword systemutils command_executor fileutils value64_tarray seq hashset_utils lwset ds ds_adapter v64gen \
	v64typed

lib/checker.o: src/checker.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/error.o: src/error.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/log.o: src/log.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/common.o: src/common.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/test.o: src/test.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/metric.o: src/metric.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/numeric_ops.o: src/numeric_ops.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/context.o: src/context.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/hashset.o: src/hashset.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/hashset_utils.o: src/hashset_utils.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/value64.o: src/value64.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/value64_tarray.o: src/value64_tarray.c
	$(CC) $(CFLAGS) -c $< -o $@
	
lib/v64typed.o: src/v64typed.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/v64gen.o: src/v64gen.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/array.o: src/array.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/arrayIO.o: src/arrayIO.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/buffer.o: src/buffer.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/fs.o: src/fs.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/fs_iter.o: src/fs_iter.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/fs_array.o: src/fs_array.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/fileutils.o: src/fileutils.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/getword.o: src/getword.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/ds.o: src/ds.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/ds_nofs.o: src/ds.c
	$(CC) $(CFLAGS) -DDS_NOFS -c $< -o $@

lib/ds_adapter.o: src/ds_adapter.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/command_executor.o: src/command_executor.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/systemutils.o: src/systemutils.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/seq.o: src/seq.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/lwset.o: src/lwset.c
	$(CC) $(CFLAGS) -c $< -o $@

test/checker_test: src/checker.c lib/log.o lib/test.o lib/error.o | lib/checker.o$
	$(CC) $(CFLAGS) -DCHECKERTESTING $^ -o $@ $(LDLIBS)

test/error_test: src/error.c lib/log.o lib/common.o lib/test.o | lib/error.o
	$(CC) $(CFLAGS) -DERRORTESTING $^ -o $@ $(LDLIBS)

test/log_test: src/log.c lib/test.o lib/error.o | lib/log.o
	$(CC) $(CFLAGS) -DLOGTESTING $^ -o $@ $(LDLIBS)

test/common_test: src/common.c lib/test.o lib/error.o lib/log.o | lib/common.o
	$(CC) $(CFLAGS) -DCOMMONTESTING $^ -o $@ $(LDLIBS)

test/test_test: src/test.c lib/error.o lib/log.o | lib/test.o
	$(CC) $(CFLAGS) -DTESTTESTING $^ -o $@ $(LDLIBS)

test/metric_test: src/metric.c lib/log.o lib/test.o lib/error.o | lib/metric.o
	$(CC) $(CFLAGS) -DMETRICTESTING $^ -o $@ $(LDLIBS)

test/array_test: src/array.c lib/log.o lib/test.o lib/error.o lib/checker.o lib/common.o \
	lib/value64.o lib/fs.o lib/numeric_ops.o lib/fileutils.o lib/getword.o lib/buffer.o \
	lib/ds.o lib/v64gen.o lib/v64typed.o lib/arrayIO.o lib/ds.o | lib/array.o
	$(CC) $(CFLAGS) -DARRAYTESTING $^ -o $@ $(LDLIBS)

test/buffer_test:       src/buffer.c lib/log.o lib/test.o lib/error.o lib/checker.o lib/common.o | lib/buffer.o
	$(CC) $(CFLAGS) -DBUFFERTESTING $^ -o $@ $(LDLIBS)

test/fs_test:   src/fs.c lib/log.o lib/test.o lib/error.o lib/checker.o lib/common.o \
			lib/fileutils.o lib/getword.o lib/buffer.o  lib/ds.o| lib/fs.o
	$(CC) $(CFLAGS) -DFSTESTING $^ -o $@ $(LDLIBS)

test/fs_iter_test: src/fs_iter.c lib/log.o lib/test.o lib/error.o lib/checker.o lib/common.o lib/fs.o | lib/fs_iter.o
	$(CC) $(CFLAGS) -DFSITERTESTING $^ -o $@ $(LDLIBS)

test/fs_array_test: src/fs_array.c lib/log.o lib/test.o lib/error.o lib/checker.o lib/common.o lib/fs.o \
		lib/fileutils.o | lib/fs_array.o
	$(CC) $(CFLAGS) -DFSARRAYTESTING $^ -o $@ $(LDLIBS)

test/fileutils_test: src/fileutils.c lib/log.o lib/test.o lib/error.o lib/checker.o lib/common.o lib/fs.o \
        lib/getword.o lib/buffer.o lib/ds.o | lib/fileutils.o
	$(CC) $(CFLAGS) -DFILEUTILSTESTING $^ -o $@ $(LDLIBS)

test/getword_test: src/getword.c lib/log.o lib/test.o lib/error.o lib/checker.o lib/common.o lib/fs.o\
			 lib/buffer.o lib/fs_array.o lib/fileutils.o lib/ds.o | lib/getword.o
	$(CC) $(CFLAGS) -DGETWORDTESTING $^ -o $@ $(LDLIBS)

# NOTE: need 2 version of ds - normal and  NO_FSDS
test/ds_test: src/ds.c lib/test.o lib/log.o lib/error.o  \
		 lib/fs.o lib/fileutils.o lib/common.o lib/getword.o lib/buffer.o \
		  | lib/ds.o
	$(CC) $(CFLAGS) -DDS_TESTING $^ -o $@ $(LDLIBS)

test/ds_adapter_test: src/ds_adapter.c lib/test.o lib/log.o lib/error.o  \
		 lib/fs.o lib/fileutils.o lib/common.o lib/getword.o lib/buffer.o lib/ds.o \
		  | lib/ds_adapter.o
	$(CC) $(CFLAGS) -DDS_ADAPTER_TESTING $^ -o $@ $(LDLIBS)

test/ds_test_nofs: src/ds.c lib/test.o lib/log.o lib/error.o  \
		  | lib/ds.o
	$(CC) $(CFLAGS) -DDS_NOFS -DDS_TESTING $^ -o $@ $(LDLIBS)

test/systemutils_test: src/systemutils.c lib/log.o lib/test.o lib/error.o lib/checker.o | lib/systemutils.o
	$(CC) $(CFLAGS) -DSYSTEMUTILSTESTING $^ -o $@ $(LDLIBS)

test/seq_test: src/seq.c lib/log.o lib/test.o lib/error.o lib/checker.o | lib/seq.o
	$(CC) $(CFLAGS) -DSEQTESTING $^ -o $@ $(LDLIBS)

test/lwset_test: src/lwset.c lib/log.o lib/test.o lib/error.o lib/checker.o lib/fs_iter.o lib/fs.o \
		lib/common.o lib/fileutils.o lib/getword.o lib/buffer.o | lib/lwset.o
	$(CC) $(CFLAGS) -DLWSET_TESTING $^ -o $@ $(LDLIBS)

test/numeric_ops_test: src/numeric_ops.c lib/log.o lib/test.o lib/error.o | lib/numeric_ops.o
	$(CC) $(CFLAGS) -DNUMERIC_OPS_TESTING $^ -o $@ $(LDLIBS)

test/context_test: src/context.c lib/log.o lib/test.o lib/error.o lib/numeric_ops.o | lib/context.o
	$(CC) $(CFLAGS) -DCONTEXTTESTING $^ -o $@ $(LDLIBS)

test/hashset_test: src/hashset.c lib/log.o lib/test.o lib/error.o lib/numeric_ops.o\
			lib/fs.o lib/fileutils.o lib/array.o lib/common.o lib/getword.o lib/buffer.o lib/value64.o | lib/hashset.o
	$(CC) $(CFLAGS) -DHSETTESTING $^ -o $@ $(LDLIBS)

test/hashset_utils_test: src/hashset_utils.c lib/log.o lib/test.o lib/error.o lib/numeric_ops.o\
			lib/fs.o lib/fileutils.o lib/array.o lib/common.o lib/getword.o lib/buffer.o lib/value64.o lib/hashset.o | lib/hashset_utils.o
	$(CC) $(CFLAGS) -DHASHSET_UTILS_TESTING $^ -o $@ $(LDLIBS)

test/value64_test: src/value64.c lib/log.o lib/test.o lib/error.o lib/common.o lib/fs.o lib/fileutils.o\
	lib/getword.o lib/buffer.o lib/numeric_ops.o lib/common.o lib/ds.o | lib/value64.o
	$(CC) $(CFLAGS) -DVALUE64TESTING $^ -o $@ $(LDLIBS)

test/value64_tarray_test: src/value64_tarray.c lib/log.o lib/test.o lib/error.o lib/common.o lib/fs.o lib/common.o \
	lib/fileutils.o lib/getword.o lib/buffer.o lib/value64.o lib/numeric_ops.o | lib/value64_tarray.o
	$(CC) $(CFLAGS) -DVALUE64_TARRAY_TESTING $^ -o $@ $(LDLIBS)

test/v64gen_test: src/v64gen.c lib/log.o lib/test.o lib/error.o lib/common.o lib/fs.o lib/fileutils.o\
	lib/getword.o lib/buffer.o lib/numeric_ops.o lib/common.o lib/ds.o lib/value64.o lib/v64typed.o | lib/v64gen.o
	$(CC) $(CFLAGS) -DV64GEN_TESTING $^ -o $@ $(LDLIBS)


test/v64typed_test: src/v64typed.c lib/log.o lib/test.o lib/error.o lib/common.o lib/fs.o lib/fileutils.o\
	lib/getword.o lib/buffer.o lib/numeric_ops.o lib/common.o lib/ds.o lib/value64.o | lib/v64typed.o
	$(CC) $(CFLAGS) -DV64TYPED_TESTING $^ -o $@ $(LDLIBS)

CFLAGS_LEAK := $(filter-out -fsanitize=address -MD -MP,$(CFLAGS))

forceleak_%:
	$(MAKE) clean
	$(MAKE) test/$*_test CFLAGS="$(CFLAGS_LEAK)"
	MallocStackLogging=1 leaks --atExit -- ./test/$*_test

leak_%:
	$(MAKE) -B test/$*_test CFLAGS="$(CFLAGS_LEAK)"
	MallocStackLogging=1 leaks --atExit -- ./test/$*_test

UTIL_OBJS = lib/log.o lib/test.o lib/error.o lib/common.o lib/checker.o \
	lib/fs.o lib/fileutils.o lib/getword.o lib/buffer.o \
	lib/fs_iter.o lib/fs_array.o lib/command_executor.o \
	lib/numeric_ops.o lib/hashset.o lib/value64.o \
	lib/context.o lib/systemutils.o \
	lib/fs.o lib/fileutils.o lib/array.o lib/metric.o lib/value64_tarray.o \
	lib/hashset_utils.o lib/seq.o lib/lwset.o lib/ds.o lib/ds_adapter.o lib/v64gen.o

lib/cmutils.a: $(UTIL_OBJS)
	ar rcs $@ $^

# faked targets
checker: test/checker_test
error:   test/error_test
log:     test/log_test
common:  test/common_test
test:    test/test_test
metric:  test/metric_test
numeric_ops: test/numeric_ops_test
context: test/context_test
hashset: test/hashset_test
hashset_utils: test/hashset_utils_test
value64: test/value64_test
value64_tarray: test/value64_tarray_test
v64gen: test/v64gen_test
v64typed: test/v64typed_test
systemutils: test/systemutils_test
getword: test/getword_test
ds: test/ds_test
ds_adapter: test/ds_adapter_test
ds_nofs: test/ds_test_nofs
fileutils: test/fileutils_test
fs_array: test/fs_array_test
fs_iter: test/fs_iter_test
fs: test/fs_test
buffer: test/buffer_test
array: test/array_test
seq: test/seq_test
lwset: test/lwset_test


clean:
	@rm -f lib/*.o
	@rm -f *.gch
	@rm -rf test/*
	@rm -f lib/*
	@find . -perm -u=x -type f -ls -maxdepth 1 -exec rm {} \;

cleanlog:
	-rm *.log

cleanall: clean cleanlog

no_targets__:
list:
	sh -c "$(MAKE) -p no_targets__ | awk -F':' '/^[a-zA-Z0-9][^\$$#\/\\t=]*:([^=]|$$)/ {split(\$$1,A,/ /);for(i in A)print A[i]}' | grep -v '__\$$' | sort"

-include $(wildcard lib/*.d)
