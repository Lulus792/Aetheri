#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
/* Test-only producer; never linked into the actual aetheri-editor executable. */
int __real_fsync(int fd);
int __wrap_fsync(int fd) {
    static unsigned calls;const char *fault=getenv("AETHERI_TEST_EDITOR_FAILURE");++calls;
    if (fault && !strcmp(fault,"directory-after") && calls==3) { errno=EIO;return -1; }
    if (fault && !strcmp(fault,"hold-first-sync") && calls==1) {
        int ready=open("editor-sync-ready",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if (ready<0) return -1;
        if (close(ready)) return -1;
        struct timespec start,now;if (clock_gettime(CLOCK_MONOTONIC,&start)) return -1;
        for (;;) {
            struct stat s;
            if (!fstatat(AT_FDCWD,"editor-sync-release",&s,AT_SYMLINK_NOFOLLOW)) {
                if (!S_ISREG(s.st_mode) || s.st_uid!=geteuid() || (s.st_mode&0777)!=0600 || s.st_nlink!=1 || s.st_size) { errno=EPERM;return -1; }
                break;
            }
            if (errno!=ENOENT || clock_gettime(CLOCK_MONOTONIC,&now)) return -1;
            if (now.tv_sec-start.tv_sec>=5) { errno=ETIMEDOUT;return -1; }
            struct timespec pause={0,5000000};(void)nanosleep(&pause,NULL);
        }
    }
    return __real_fsync(fd);
}
