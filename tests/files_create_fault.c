/* Separate native UI test binary only. The ordinary Files binary has no hook. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int __real_mkdirat(int directory,const char *name,mode_t mode);
int __real_renameat2(int od,const char *old,int nd,const char *name,unsigned flags);
int __wrap_renameat2(int od,const char *old,int nd,const char *name,unsigned flags) {
    int r=__real_renameat2(od,old,nd,name,flags);
    if (!r && !strcmp(name,"unknown-name")) {
        puts("FAULT_RENAMED entry");fflush(stdout);
        for (unsigned n=0;n<5000 && access("release-rename",F_OK);++n) usleep(1000);
        errno=EIO;return -1;
    }
    return r;
}
int __wrap_mkdirat(int directory,const char *name,mode_t mode) {
    int r=__real_mkdirat(directory,name,mode);
    if (!r && !strcmp(name,"unknown-folder")) {
        /* Real effect first, independently released acknowledgement second. */
        puts("FAULT_CREATED directory");fflush(stdout);
        for (unsigned n=0;n<5000 && access("release-create",F_OK);++n) usleep(1000);
        errno=EIO;return -1;
    }
    return r;
}
