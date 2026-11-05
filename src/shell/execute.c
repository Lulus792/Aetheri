#define _GNU_SOURCE
#include "execute.h"
#include "jobs.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
const char *ae_shell_environment(void *context,const char *name) {
    struct ae_shell_session *s=context;
    if (!strcmp(name,"?")) { snprintf(s->status_text,sizeof(s->status_text),"%d",s->status); return s->status_text; }
    return getenv(name);
}
static int report(const char *message,int status) { dprintf(2,"aetheri-shell: %s\n",message); return status; }
static bool parent_builtin(const char *name) { return !strcmp(name,"cd") || !strcmp(name,"export") || !strcmp(name,"unset") || !strcmp(name,"exit") || !strcmp(name,"jobs") || !strcmp(name,"fg") || !strcmp(name,"bg"); }
static int builtin(struct ae_shell_session *s,const struct ae_shell_command *c) {
    const char *name=c->argv[0];
    if (!strcmp(name,"exit")) {
        if (c->argc>2) return report("usage: exit [0..255]",2);
        if (s->terminal && ae_shell_jobs_busy(s->terminal)) return report("original jobs remain; use jobs/fg/bg before exit",1);
        unsigned status=(unsigned)s->status;
        if (c->argc==2) {
            status=0;const unsigned char *p=(const unsigned char *)c->argv[1];
            if (!*p) return report("usage: exit [0..255]",2);
            for (;*p;++p) { if (*p<'0' || *p>'9' || status>25) return report("usage: exit [0..255]",2); status=status*10u+(unsigned)(*p-'0'); }
            if (status>255) return report("usage: exit [0..255]",2);
        }
        s->exiting=true;return (int)status;
    }
    if (!strcmp(name,"export")) {
        if (c->argc!=3 || !ae_shell_name(c->argv[1])) return report("usage: export NAME VALUE",2);
        return setenv(c->argv[1],c->argv[2],1) ? report("cannot set environment value",1) : 0;
    }
    if (!strcmp(name,"unset")) {
        if (c->argc!=2 || !ae_shell_name(c->argv[1])) return report("usage: unset NAME",2);
        return unsetenv(c->argv[1]) ? report("cannot unset environment value",1) : 0;
    }
    if (!strcmp(name,"cd")) {
        if (c->argc>2) return report("usage: cd [PATH]",2);
        const char *path=c->argc==2 ? c->argv[1] : getenv("HOME");
        if (!path) return report("HOME is unset",1);
        if (chdir(path)) return report(strerror(errno),1);
        char *cwd=getcwd(NULL,0);bool ok=cwd && setenv("PWD",cwd,1)==0;free(cwd);
        if (!ok) { (void)unsetenv("PWD");return report("directory changed but PWD could not be updated",1); }
        return 0;
    }
    if (!strcmp(name,"pwd")) {
        if (c->argc!=1) return report("usage: pwd",2);
        char *cwd=getcwd(NULL,0);
        if (!cwd) return report(strerror(errno),1);
        int n=dprintf(1,"%s\n",cwd);free(cwd);return n<0 ? 1 : 0;
    }
    if (!strcmp(name,"help")) {
        if (c->argc!=1) return report("usage: help",2);
        return dprintf(1,"Aetheri experimental shell profile 0\nBuiltins: cd [PATH], export NAME VALUE, unset NAME, exit [STATUS], pwd, help\nInteractive -i: jobs, fg [ID], bg [ID]; Ctrl+C interrupt, Ctrl+Z stop\nForeground pipes and < > >> 2> 2>>; variables $NAME ${NAME} $?\nPOSIX script compatibility and persistent sessions are not implemented.\n")<0 ? 1 : 0;
    }
    return ae_shell_jobs_command(s,c);
}
static void external(struct ae_shell_command *c) {
    if (strchr(c->argv[0],'/')) {
        execve(c->argv[0],c->argv,environ);int saved=errno;
        int result=saved==ENOENT || saved==ENOTDIR ? 127 : 126;
        (void)report(strerror(saved),result);_Exit(result);
    }
    const char *path=getenv("PATH");if (!path) path="/usr/bin:/bin";
    int failure=ENOENT;char candidate[AE_SHELL_BYTES];
    for (;;) {
        const char *end=strchr(path,':');size_t n=end ? (size_t)(end-path) : strlen(path),a=strlen(c->argv[0]);
        if (n+a+2>sizeof(candidate)) { (void)report("PATH entry exceeds execution budget",126);_Exit(126); }
        if (n) { memcpy(candidate,path,n);candidate[n++]='/'; }
        memcpy(candidate+n,c->argv[0],a+1);execve(candidate,c->argv,environ);
        int saved=errno;
        if (saved==EACCES) failure=EACCES;
        else if (saved!=ENOENT && saved!=ENOTDIR) { (void)report(strerror(saved),126);_Exit(126); }
        if (!end) break;
        path=end+1;
    }
    (void)report(strerror(failure),failure==ENOENT ? 127 : 126);_Exit(failure==ENOENT ? 127 : 126);
}
static int connect_fd(int source,int target) {
    if (source!=target) return dup2(source,target)<0 ? -1 : 0;
    return fcntl(target,F_SETFD,0);
}
static void child(struct ae_shell_session *s,struct ae_shell_program *p,size_t index,int pipes[AE_SHELL_COMMANDS-1][2],int gate[2],struct ae_shell_job *job) {
    if (job && setpgid(0,job->group)) _Exit(125);
    const int signals[]={SIGINT,SIGQUIT,SIGPIPE,SIGTERM,SIGTSTP,SIGTTIN,SIGTTOU,SIGHUP,SIGCHLD};
    struct sigaction action={.sa_handler=SIG_DFL};sigemptyset(&action.sa_mask);
    for (size_t i=0;i<sizeof(signals)/sizeof(signals[0]);++i) if (sigaction(signals[i],&action,NULL)) _Exit(125);
    close(gate[1]);char byte;ssize_t n;
    do { n=read(gate[0],&byte,1); } while (n<0 && errno==EINTR);
    if (n!=1 || byte!='R') _Exit(125);
    /* Descriptor setup itself may block (for example opening a FIFO). Once
       released, terminal signals must already interrupt that phase. */
    sigset_t mask;sigemptyset(&mask);if (sigprocmask(SIG_SETMASK,&mask,NULL)) { (void)report("cannot establish child signal mask",125);_Exit(125); }
    if ((index && connect_fd(pipes[index-1][0],0)) || (index+1<p->count && connect_fd(pipes[index][1],1))) {
        (void)report("cannot connect pipeline descriptors",125);_Exit(125);
    }
    struct ae_shell_command *c=&p->commands[index];
    for (unsigned i=0;i<3;++i) if (c->files[i].mode) {
        int flags=c->files[i].mode==AE_SHELL_READ ? O_RDONLY : O_WRONLY|O_CREAT|(c->files[i].mode==AE_SHELL_APPEND ? O_APPEND : O_TRUNC);
        int fd=open(c->files[i].path,flags|O_CLOEXEC,0666);
        if (fd<0) { (void)report(strerror(errno),1);_Exit(1); }
        if (connect_fd(fd,(int)i)) { (void)report("cannot bind redirection",125);_Exit(125); }
        if (fd!=(int)i) close(fd);
    }
    if (close_range(3,UINT_MAX,0)) { (void)report("cannot close inherited descriptors",125);_Exit(125); }
    int result=builtin(s,c);if (result>=0) _Exit(result);
    external(c);
}
int ae_shell_execute(struct ae_shell_session *s,struct ae_shell_program *p) {
    if (!p->count) return s->status;
    if (s->terminal && ae_shell_jobs_check(s->terminal)) return s->status=125;
    for (size_t i=0;i<p->count;++i) if (parent_builtin(p->commands[i].argv[0])) {
        if (p->count!=1 || p->commands[i].files[0].mode || p->commands[i].files[1].mode || p->commands[i].files[2].mode)
            return s->status=report("state-changing builtins require one command without redirection",2);
        return s->status=builtin(s,&p->commands[i]);
    }
    struct ae_shell_job *job=s->terminal ? ae_shell_job_prepare(s->terminal,p->commands[0].argv[0]) : NULL;
    if (s->terminal && !job) return s->status=report("job budget or terminal admission unavailable",125);
    int pipes[AE_SHELL_COMMANDS-1][2],gate[2];size_t made=0,forked=0;pid_t children[AE_SHELL_COMMANDS];bool released=false;
    if (pipe2(gate,O_CLOEXEC)) {
        if (job) ae_shell_job_cancel(s->terminal,job);
        return s->status=report("cannot create start barrier",125);
    }
    for (;made+1<p->count;++made) if (pipe2(pipes[made],O_CLOEXEC)) break;
    bool setup=true;
    if (made+1==p->count) while (forked<p->count) {
        pid_t pid=fork();
        if (pid<0) break;
        if (!pid) child(s,p,forked,pipes,gate,job);
        children[forked++]=pid;
        if (job && ae_shell_job_add(job,pid)) { setup=false;break; }
    }
    close(gate[0]);
    if (forked==p->count && setup && (!job || ae_shell_job_take(s->terminal,job,false)==0)) {
        char release[AE_SHELL_COMMANDS];memset(release,'R',forked);ssize_t n;
        do { n=write(gate[1],release,forked); } while (n<0 && errno==EINTR);
        released=n==(ssize_t)forked;
    }
    close(gate[1]);
    for (size_t i=0;i<made;++i) { close(pipes[i][0]);close(pipes[i][1]); }
    if (released && job) return s->status=ae_shell_job_wait(s->terminal,job);
    int result=released ? 0 : report("pipeline setup/release failed",125);bool wait_failed=false;
    for (size_t i=0;i<forked;++i) {
        int status=0;pid_t waited;
        do { waited=waitpid(children[i],&status,0); } while (waited<0 && errno==EINTR);
        if (waited!=children[i]) { wait_failed=true;(void)report("original child wait failed",125); }
        else if (released && i+1==p->count) result=WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128+WTERMSIG(status) : 125;
    }
    if (job && !wait_failed) ae_shell_job_cancel(s->terminal,job);
    return s->status=wait_failed ? 125 : result;
}
