PROJNAME = fs2

REQUIRES = error common log bool checker

# Expose __fs2_checkreset and the counter accessors for leak tests.
# In a real shipped build you would remove this line — the reset
# function then disappears and the counters stay private.
CFLAGS += -DFS2TEST

SRC_LIST      = src/fs2.c src/fs2_api.c src/fs2_tech.c src/fs2array.c
SRC_HDR       = include/fs2.h include/fs2_api.h include/fs2array.h
TEST_SRC_LIST = test/test_fs2.c test/test_fs2_api.c test/test_fs2_api_any_class.c \
				test/test_fs2_api_sort_search.c test/test_fs2_api_parse.c test/test_fs2_api_instr.c \
				test/test_fs2_api_serialization.c test/test_fs2_api_modificators.c \
				test/test_fs2_tech.c test/test_fs2array.c

include ../Makefile.common

