#define _GNU_SOURCE
#define main ae_test_runner_main
#include "../src/login/main.c"
#undef main
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"runner entry line %d: %s\n",__LINE__,#x);abort();}}while(0)
static int canary=-1;
static bool fail_close;
int ae_login_deployment_select(struct ae_login_run_profile *out) {
    (void)out;int n=fcntl(canary,F_GETFD);char answer=n<0 && errno==EBADF?'y':'n';
    CHECK(write(1,&answer,1)==1);return -EACCES;
}
int __real_close_range(unsigned int,unsigned int,int);
int __wrap_close_range(unsigned int first,unsigned int last,int flags) {
    CHECK(first==3 && last==~0u && flags==CLOSE_RANGE_UNSHARE);
    if(fail_close){errno=EPERM;return -1;}
    return __real_close_range(first,last,flags);
}
static void attempt(unsigned mode) {
    int pipes[2];CHECK(!pipe(pipes));canary=open("/dev/null",O_RDONLY|O_CLOEXEC);CHECK(canary>=3);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid) {
        CHECK(dup2(pipes[1],1)==1);fail_close=mode==1;
        if(mode==2)CHECK(!close(0));
        char *args[]={"aetheri-login-startup","unexpected",NULL};
        int result=ae_test_runner_main(mode==3?2:1,args);
        if(mode==1){CHECK(fcntl(canary,F_GETFD)>=0);CHECK(write(1,"f",1)==1);}
        if(mode==2)CHECK(write(1,"m",1)==1);
        if(mode==3)CHECK(write(1,"a",1)==1);
        _exit(result==((mode==3)?2:1)?0:2);
    }
    close(pipes[1]);char answer=0;CHECK(read(pipes[0],&answer,1)==1 && answer==(mode==0?'y':mode==1?'f':mode==2?'m':'a'));
    close(pipes[0]);int status;CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(fcntl(canary,F_GETFD)>=0);close(canary);
}
int main(void) {
    for(unsigned i=0;i<4;++i)attempt(i);
    puts("Runner entry: inherited FD closure, failed cleanup, missing stdio, argument rejection and parent descriptor preservation passed");return 0;
}
