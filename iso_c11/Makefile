CC=gcc #clang

GNU23_SUPPORT := $(shell $(CC) -std=gnu23 -E - < /dev/null > /dev/null 2>&1; echo $$?)
ifeq ($(GNU23_SUPPORT),0)
    C_STD := -std=gnu23
else
    # Если gnu23 нет, пробуем c23 (для Clang 21 и подобных, где c23 есть, а gnu23 может отсутствовать)
    C23_SUPPORT := $(shell $(CC) -std=c23 -E - < /dev/null > /dev/null 2>&1; echo $$?)
    ifeq ($(C23_SUPPORT),0)
        C_STD := -std=c23
    else
        # На самых старых (GCC 11 на Oracle Linux) возвращаемся к gnu11
        C_STD := -std=gnu11
    endif
endif

CFLAGS= -I../utils/ -Wall -Wextra -g -O3 -Wno-gnu-folding-constant $(C_STD)
#CFLAGS= -I../utils/ -Wall -Wextra -std=gnu11 -g -O3 -Wno-gnu-folding-constant -std=c23
# -Wno-initializer-overrides -Wno-unused-function -Wno-format-zero-length
LDFLAGS=-
LDLIBS=

all:

rnd_test: src/rnd_test.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

strerr_check: src/strerr_check.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

malloc_test: src/malloc_test.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

print_format_test: src/print_format_test.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

anon_struct: src/anon_struct.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

anon_struct2: src/anon_struct2.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

singleton: src/singleton.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

printf_n: src/printf_n.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

round_up2: src/round_up2.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

power: src/power.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

t7.8.1: src/t7.8.1.c
	$(CC) $(CFLAGS) $^ -o $@_test

va_test: src/va_test.c ../log.c
	$(CC) $(CFLAGS) $^ -o $@_test

limit-test: src/limit-test.c
	$(CC) $(CFLAGS) $^ -o $@_test

gethostname_t: src/gethostname_t.c
	$(CC) $(CFLAGS) $^ -o $@_test

setjump_test: src/setjump_test.c
	$(CC) $(CFLAGS) $^ -o $@_test

assert_test: src/assert_test.c
	$(CC) $(CFLAGS) $^ -o $@_test

address_in_stack: src/address_in_stack.c
	$(CC) $(CFLAGS) $^ -o $@_test

strerror_test: src/strerror_test.c
	$(CC) $(CFLAGS) $^ -o $@_test

return_test: src/return_test.c
	$(CC) $(CFLAGS) $^ -o $@_test

setjump2_test: src/setjump2_test.c
	$(CC) $(CFLAGS) $^ -o $@_test

comparator_test: src/comparator_test.c ../log.c
	#$(CC) $(CFLAGS) $^ -o $@
	$(CC) $(CFLAGS) $^ -o $@_test

macro-cat-test: src/macro-cat-test.c
	$(CC) -E $^

timespec_get_test: src/timespec_get_test.c
	$(CC) $(CFLAGS) $^ -o $@_test

test-cmp-doc: src/test-cmp-doc.c
	$(CC) $(CFLAGS) $^ -o $@_test

test-next-prime: src/test-next-prime.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

cycleguarg: src/cycleguard.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

check_alignment: src/check_alignment.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

output_off: src/output_off.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

check_static_array_size: src/check_static_array_size.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

test_while: src/test_while.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

check_clone: src/check_clone.c 
	$(CC) $(CFLAGS) $^ -o out/$@_test

iter_test: src/iter_test.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

iter_test_ind: src/iter_test_ind.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

iter_test_arrcnt: src/iter_test_arrcnt.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

conv_test: src/conv_test.c
	$(CC) $(CFLAGS) $^ -o out/$@_test

.PHONY: clean cleanlog cleanall no_targets__ list
clean:
	-rm *.o
	-rm *_test
	-rm -r *.dSYM
	-rm *.gch
	find . -perm -u=x -type f -ls -maxdepth 1 -exec rm {} \;

cleanlog:
	-rm *.log

cleanall: clean cleanlog

no_targets__:
list:
	sh -c "$(MAKE) -p no_targets__ | awk -F':' '/^[a-zA-Z0-9][^\$$#\/\\t=]*:([^=]|$$)/ {split(\$$1,A,/ /);for(i in A)print A[i]}' | grep -v '__\$$' | sort"

