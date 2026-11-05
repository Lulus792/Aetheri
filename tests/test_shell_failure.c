#define _GNU_SOURCE
#include "shell/execute.h"
#include "shell/jobs.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1); } } while (0)
static unsigned forks,pipes;static bool fail_pipe,fail_terminal;
pid_t __real_fork(void);
int __real_pipe2(int fds[2],int flags);
int __real_tcsetpgrp(int fd,pid_t group);
pid_t __wrap_fork(void) { if (++forks==2 && !fail_terminal) { errno=EAGAIN;return -1; }return __real_fork(); }
int __wrap_pipe2(int fds[2],int flags) { if (fail_pipe && ++pipes==2) { errno=EMFILE;return -1; }return __real_pipe2(fds,flags); }
int __wrap_tcsetpgrp(int fd,pid_t group) { if (fail_terminal) { errno=EPERM;return -1; }return __real_tcsetpgrp(fd,group); }
int main(int argc,char **argv) {
    CHECK(getuid()!=0);char directory[]="/tmp/aetheri-shell-failure-XXXXXX";CHECK(mkdtemp(directory));
    char source[512];CHECK(snprintf(source,sizeof(source),"pwd > %s/output | /usr/bin/cat",directory)>0);
    struct ae_shell_program *p=malloc(sizeof(*p));CHECK(p);struct ae_shell_session s={0};size_t at=0;
    CHECK(ae_shell_parse(source,strlen(source),ae_shell_environment,&s,p,&at)==AE_SHELL_OK);
    if (argc==2 && !strcmp(argv[1],"--tty")) {
        struct ae_shell_terminal terminal;CHECK(ae_shell_jobs_open(&terminal)==0);s.terminal=&terminal;fail_terminal=true;
        CHECK(ae_shell_execute(&s,p)==125);CHECK(waitpid(-1,NULL,WNOHANG)==-1 && errno==ECHILD);
        char output[512];CHECK(snprintf(output,sizeof(output),"%s/output",directory)>0);
        CHECK(access(output,F_OK)!=0 && errno==ENOENT);CHECK(tcgetpgrp(0)==getpgrp());
        ae_shell_jobs_close(&terminal);free(p);CHECK(rmdir(directory)==0);
        puts("terminal handoff failure: no command/file effects and no lost original children");return 0;
    }
    CHECK(argc==1);
    for (unsigned mode=0;mode<2;++mode) {
        fail_pipe=mode!=0;forks=pipes=0;
        CHECK(ae_shell_execute(&s,p)==125);
        char output[512];CHECK(snprintf(output,sizeof(output),"%s/output",directory)>0);
        CHECK(access(output,F_OK)!=0 && errno==ENOENT);
        CHECK(waitpid(-1,NULL,WNOHANG)==-1 && errno==ECHILD);
        CHECK(mode ? forks==0 : forks==2);
    }
    free(p);CHECK(rmdir(directory)==0);puts("shell pipeline failures: no command/file effects before complete creation; all original children reaped");return 0;
}
