/* Test-only actual fsync adapter. No hook in aetheri-text-editor. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
int __real_fsync(int);
static int command_fd=-1;static unsigned generation,stage;static bool inject;
int __wrap_fsync(int fd) {
    struct stat info;
    if(fstat(fd,&info)) return -1;
    if(S_ISREG(info.st_mode)) {
        if(command_fd<0) {
            const char *text=getenv("AETHERI_TEST_SAVE_FD");char *end=NULL;long parsed=text?strtol(text,&end,10):-1;
            if(!text || !*text || !end || *end || parsed<3 || parsed>65535 || fcntl((int)parsed,F_GETFD)<0) _Exit(125);
            command_fd=(int)parsed;
        }
        stage=0;printf("SAVE_PAUSE %u\n",++generation);fflush(stdout);char byte;ssize_t n;
        do {n=read(command_fd,&byte,1);} while(n<0 && errno==EINTR);
        if(n!=1 || (byte!='c' && byte!='f')) _Exit(125);
        inject=byte=='f';
    }
    int result=__real_fsync(fd);++stage;
    if(inject && stage==3) {errno=EIO;return -1;}
    return result;
}
