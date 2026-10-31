PROJNAME = fs2

REQUIRES = error common log bool checker

# Expose __fs2_checkreset and the counter accessors for leak tests.
# In a real shipped build you would remove this line — the reset
# function then disappears and the counters stay private.
CFLAGS += -DFS2TEST

SRC_LIST      = src/fs2.c src/fs2_api.c
SRC_HDR       = include/fs2.h include/fs2_api.h
#TEST_SRC_LIST = test/test_fs2.c test/test_fs2_api.c
TEST_SRC_LIST = test/test_fs2.c

include ../Makefile.common

