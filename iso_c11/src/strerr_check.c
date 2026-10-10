#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(void) {
    char buf[256] = {0};
    int r = strerror_r(ENOENT, buf, sizeof buf);
    printf("r = %d\nbuf = [%s]\n", r, buf);
    return 0;
}

