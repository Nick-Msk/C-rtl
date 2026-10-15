PROJNAME = fs2

REQUIRES = error common log bool checker

# Expose __fs2_checkreset and the counter accessors for leak tests.
# In a real shipped build you would remove this line — the reset
# function then disappears and the counters stay private.
CFLAGS += -DFS2TEST

include ../Makefile.common

