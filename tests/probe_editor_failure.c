#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Test-only producer; never linked into the actual aetheri-editor executable. */
int __real_fsync(int fd);
int __wrap_fsync(int fd) {
    static unsigned calls;const char *fault=getenv("AETHERI_TEST_EDITOR_FAILURE");++calls;
    if (fault && !strcmp(fault,"directory-after") && calls==3) { errno=EIO;return -1; }
    return __real_fsync(fd);
}
