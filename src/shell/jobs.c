#define _GNU_SOURCE
#include "jobs.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/tty.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static const int managed[]={SIGCHLD,SIGINT,SIGHUP,SIGTERM,SIGQUIT,SIGTSTP,SIGTTIN,SIGTTOU,SIGWINCH};
static int failure(struct ae_shell_terminal *t,const char *message) {
    dprintf(2,"aetheri-shell: %s\n",message);t->failed=true;
    if (!t->shutdown_signal) t->shutdown_signal=SIGHUP;
    return -1;
}
static bool owns(struct ae_shell_terminal *t,pid_t group) {
    return tcgetsid(t->fd)==t->session && tcgetpgrp(t->fd)==group;
}
static int state(const struct ae_shell_job *j) {
    bool running=false,stopped=false;
    for (size_t i=0;i<j->count;++i) {
        if (j->children[i].state==AE_JOB_UNKNOWN) return 3;
        running|=j->children[i].state==AE_JOB_RUNNING;
        stopped|=j->children[i].state==AE_JOB_STOPPED;
    }
    return running ? 0 : stopped ? 1 : 2;
}
static int result(const struct ae_shell_job *j) {
    int current=state(j);
    if (current==3 || !j->count) return 125;
    if (current==1) for (size_t i=0;i<j->count;++i)
        if (j->children[i].state==AE_JOB_STOPPED) return 128+WSTOPSIG(j->children[i].status);
    int status=j->children[j->count-1].status;
    return WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128+WTERMSIG(status) : 125;
}
static void notice(struct ae_shell_terminal *t,const struct ae_shell_job *j,int current) {
    if (j->continuing) dprintf(t->fd,"\n[%u] continue requested %s\n",j->id,j->label);
    else if (!current) dprintf(t->fd,"\n[%u] running %s\n",j->id,j->label);
    else dprintf(t->fd,"\n[%u] %s %d %s\n",j->id,current==1 ? "stopped" : current==2 ? "done" : "unknown",result(j),j->label);
}
static bool reap(struct ae_shell_terminal *t,bool notify) {
    bool printed=false;
    for (unsigned i=0;i<AE_SHELL_JOBS;++i) {
        struct ae_shell_job *j=&t->jobs[i];if (!j->id) continue;
        for (size_t k=0;k<j->count;++k) {
            struct ae_shell_child *c=&j->children[k];
            while (c->state!=AE_JOB_ENDED && c->state!=AE_JOB_UNKNOWN) {
                int status=0;pid_t pid=waitpid(c->pid,&status,WNOHANG|WUNTRACED|WCONTINUED);
                if (pid<0 && errno==EINTR) continue;
                if (!pid) break;
                if (pid!=c->pid) { c->state=AE_JOB_UNKNOWN;(void)failure(t,"original job child is no longer waitable");break; }
                c->status=status;
                /* CONTINUED can coalesce with an immediate new TTIN/stop.
                   A fresh wait observation still completes this continue attempt. */
                if (j->continuing) j->notified=-1;
                j->continuing=false;
                c->state=WIFEXITED(status) || WIFSIGNALED(status) ? AE_JOB_ENDED : WIFSTOPPED(status) ? AE_JOB_STOPPED : AE_JOB_RUNNING;
            }
        }
        int current=state(j);
        if (j!=t->active && notify && !j->continuing && current!=j->notified) { notice(t,j,current);j->notified=current;printed=true; }
        if (j!=t->active && current==2) memset(j,0,sizeof(*j));
    }
    return printed;
}
static int signals(struct ae_shell_terminal *t) {
    int interrupted=0;
    for (;;) {
        struct signalfd_siginfo info;ssize_t n=read(t->signals,&info,sizeof(info));
        if (n<0 && errno==EINTR) continue;
        if (n<0 && errno==EAGAIN) break;
        if (n!=(ssize_t)sizeof(info)) return failure(t,"terminal signal channel failed");
        if (info.ssi_signo==SIGINT) interrupted=1;
        if (info.ssi_signo==SIGWINCH) t->resized=true;
        if ((info.ssi_signo==SIGHUP || info.ssi_signo==SIGTERM) && !t->shutdown_signal) t->shutdown_signal=(int)info.ssi_signo;
    }
    return interrupted;
}
bool ae_shell_jobs_busy(const struct ae_shell_terminal *t) {
    for (unsigned i=0;i<AE_SHELL_JOBS;++i) if (t->jobs[i].id && state(&t->jobs[i])!=2) return true;
    return false;
}
int ae_shell_jobs_check(struct ae_shell_terminal *t) {
    if (t->shutdown_signal) return -1;
    return owns(t,t->group) ? 0 : failure(t,"terminal foreground owner changed; refusing takeover");
}
int ae_shell_jobs_open(struct ae_shell_terminal *t) {
    memset(t,0,sizeof(*t));t->fd=t->signals=t->edit_fd=-1;t->group=getpgrp();t->session=getsid(0);
    int discipline=-1;
    if (t->group!=getpid() || tcgetsid(0)!=t->session || tcgetpgrp(0)!=t->group ||
            tcgetattr(0,&t->modes) || !(t->modes.c_lflag&ICANON) || !(t->modes.c_lflag&ISIG) ||
            ioctl(0,TIOCGETD,&discipline) || discipline!=N_TTY)
        return failure(t,"-i requires an own foreground process group and canonical controlling terminal");
    t->fd=fcntl(0,F_DUPFD_CLOEXEC,3);
    if (t->fd<0) return failure(t,"cannot retain controlling terminal");
    sigset_t mask;sigemptyset(&mask);
    for (unsigned i=0;i<4;++i) sigaddset(&mask,managed[i]);
    sigaddset(&mask,SIGWINCH);
    if (sigprocmask(SIG_BLOCK,&mask,&t->old_mask)) return failure(t,"cannot block terminal signals");
    t->masked=true;
    for (unsigned i=0;i<9;++i) {
        struct sigaction action={.sa_handler=i<4 || i==8 ? SIG_DFL : SIG_IGN};sigemptyset(&action.sa_mask);
        if (sigaction(managed[i],&action,&t->old_actions[i])) return failure(t,"cannot configure terminal signal actions");
        ++t->actions;
    }
    t->signals=signalfd(-1,&mask,SFD_CLOEXEC|SFD_NONBLOCK);
    if (t->signals<0) return failure(t,"cannot create terminal signal channel");
    return 0;
}
void ae_shell_jobs_close(struct ae_shell_terminal *t) {
    if (t->editing && owns(t,t->group)) (void)ae_shell_jobs_edit_mode(t,false);
    if (t->edit_fd>=0) close(t->edit_fd);
    if (t->signals>=0) close(t->signals);
    if (t->fd>=0) close(t->fd);
    for (unsigned i=0;i<t->actions;++i) (void)sigaction(managed[i],&t->old_actions[i],NULL);
    if (t->masked) (void)sigprocmask(SIG_SETMASK,&t->old_mask,NULL);
}
ssize_t ae_shell_jobs_read(struct ae_shell_terminal *t,char *buffer,size_t length) {
    return ae_shell_jobs_read_timeout(t,buffer,length,-1);
}
ssize_t ae_shell_jobs_read_timeout(struct ae_shell_terminal *t,char *buffer,size_t length,int timeout_ms) {
    uint64_t deadline=0;
    if (timeout_ms>=0) {
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC,&now) || now.tv_sec<0) return failure(t,"editor deadline clock failed");
        deadline=(uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u+(unsigned)timeout_ms;
    }
    for (;;) {
        if (t->interrupted) { t->interrupted=false;return -2; }
        if (t->editing && t->resized) { t->resized=false;return -5; }
        if (reap(t,true)) return -3;
        if (t->shutdown_signal) return -1;
        if (!owns(t,t->group)) return failure(t,"terminal foreground owner changed; refusing takeover");
        int input=t->editing ? t->edit_fd : t->fd;
        struct pollfd fds[2]={{input,POLLIN,0},{t->signals,POLLIN,0}};int n;
        int remaining=-1;
        if (timeout_ms>=0) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC,&now) || now.tv_sec<0) return failure(t,"editor deadline clock failed");
            uint64_t tick=(uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;
            if (tick>=deadline) return -4;
            remaining=(int)(deadline-tick);
        }
        n=poll(fds,2,remaining);
        if (n<0 && errno==EINTR) continue;
        if (n<0) return failure(t,"terminal event wait failed");
        if (!n) return -4;
        if (fds[1].revents) {
            int interrupted=signals(t);
            if (t->shutdown_signal) return -1;
            bool printed=reap(t,true);
            if (interrupted>0) return -2;
            if (printed) return -3;
            if (t->editing && t->resized) { t->resized=false;return -5; }
        }
        if (fds[0].revents&(POLLHUP|POLLERR|POLLNVAL)) { t->shutdown_signal=SIGHUP;return -1; }
        if (fds[0].revents&POLLIN) {
            ssize_t got=read(input,buffer,length);
            if (got<0 && (errno==EINTR || errno==EAGAIN)) continue;
            if (got<0) return failure(t,"controlling terminal read failed");
            return got;
        }
    }
}
static bool same_modes(const struct termios *a,const struct termios *b) {
    return a->c_iflag==b->c_iflag && a->c_oflag==b->c_oflag && a->c_cflag==b->c_cflag && a->c_lflag==b->c_lflag &&
        a->c_line==b->c_line && !memcmp(a->c_cc,b->c_cc,sizeof(a->c_cc)) && cfgetispeed(a)==cfgetispeed(b) && cfgetospeed(a)==cfgetospeed(b);
}
int ae_shell_jobs_edit_mode(struct ae_shell_terminal *t,bool enabled) {
    if (!owns(t,t->group)) return failure(t,"editor terminal foreground owner changed");
    if (enabled && t->shutdown_signal) return -1;
    if (enabled==t->editing) return 0;
    struct termios current;
    if (tcgetattr(t->fd,&current) || !same_modes(&current,t->editing ? &t->edit_modes : &t->modes))
        return failure(t,"editor terminal modes drifted; refusing replacement");
    if (enabled && t->edit_fd<0) {
        int fd=open("/dev/tty",O_RDWR|O_NOCTTY|O_CLOEXEC|O_NONBLOCK);unsigned original=0,selected=0;
        if (fd<0) return failure(t,"cannot open private editor terminal");
        if (ioctl(t->fd,TIOCGDEV,&original) || ioctl(fd,TIOCGDEV,&selected) || original!=selected ||
            tcgetsid(fd)!=t->session || tcgetpgrp(fd)!=t->group) {
            close(fd);return failure(t,"editor terminal differs from original controlling terminal");
        }
        t->edit_fd=fd;
    }
    if (enabled) {
        t->edit_modes=t->modes;t->edit_modes.c_lflag&=(tcflag_t)~(ICANON|ECHO|ECHONL|IEXTEN|NOFLSH);
        t->edit_modes.c_iflag&=(tcflag_t)~(IXON|IXOFF);t->edit_modes.c_cc[VMIN]=1;t->edit_modes.c_cc[VTIME]=0;
    }
    if (tcsetattr(t->fd,TCSANOW,enabled ? &t->edit_modes : &t->modes)) return failure(t,"cannot select editor terminal modes");
    t->editing=enabled;
    if (tcgetattr(t->fd,&current) || !same_modes(&current,enabled ? &t->edit_modes : &t->modes))
        return failure(t,"editor terminal mode selection was not confirmed");
    return owns(t,t->group) ? 0 : failure(t,"editor terminal owner changed during mode selection");
}
int ae_shell_jobs_write(struct ae_shell_terminal *t,const char *bytes,size_t length) {
    if (!t->editing || t->edit_fd<0) return failure(t,"editor output outside its mode");
    size_t done=0;
    while (done<length) {
        if (t->shutdown_signal || ae_shell_jobs_check(t)) return -1;
        ssize_t n=write(t->edit_fd,bytes+done,length-done);
        if (n>0) { done+=(size_t)n;continue; }
        if (n<0 && errno==EINTR) continue;
        if (n>=0 || errno!=EAGAIN) return failure(t,"editor terminal output failed");
        struct pollfd fds[2]={{t->edit_fd,POLLOUT,0},{t->signals,POLLIN,0}};int ready;
        do { ready=poll(fds,2,-1); } while (ready<0 && errno==EINTR);
        if (ready<0 || (fds[0].revents&(POLLERR|POLLHUP|POLLNVAL))) return failure(t,"editor output wait failed");
        if (fds[1].revents) {
            int interrupted=signals(t);if (interrupted<0 || t->shutdown_signal) return -1;
            if (interrupted) { t->interrupted=true;return -2; }
        }
    }
    return 0;
}
struct ae_shell_job *ae_shell_job_prepare(struct ae_shell_terminal *t,const char *label) {
    if (t->editing) { (void)failure(t,"editor has not yielded terminal modes before dispatch");return NULL; }
    (void)reap(t,true);
    if (ae_shell_jobs_check(t) || t->next_id==UINT32_MAX) return NULL;
    for (unsigned i=0;i<AE_SHELL_JOBS;++i) if (!t->jobs[i].id) {
        struct ae_shell_job *j=&t->jobs[i];memset(j,0,sizeof(*j));j->id=++t->next_id;
        size_t n=0;
        for (const unsigned char *p=(const unsigned char *)label;*p && n<sizeof(j->label)-1;++p)
            j->label[n++]=*p>=0x20 && *p<0x7f ? (char)*p : '?';
        j->label[n]=0;return j;
    }
    return NULL;
}
int ae_shell_job_add(struct ae_shell_job *j,pid_t child) {
    if (j->count==AE_SHELL_COMMANDS || child<=0) return -1;
    if (!j->count) j->group=child;
    j->children[j->count++]=(struct ae_shell_child){.pid=child,.state=AE_JOB_RUNNING};
    return setpgid(child,j->group);
}
static int restore(struct ae_shell_terminal *t,struct ae_shell_job *j) {
    if (!owns(t,t->group)) {
        if (!owns(t,j->group)) return failure(t,"terminal owner drift; refusing foreground replacement");
        if (tcsetpgrp(t->fd,t->group)) return failure(t,"cannot reclaim original terminal");
    }
    if (tcsetattr(t->fd,TCSANOW,&t->modes)) return failure(t,"cannot restore shell terminal modes");
    return 0;
}
int ae_shell_job_take(struct ae_shell_terminal *t,struct ae_shell_job *j,bool resume) {
    if (!owns(t,t->group)) return failure(t,"terminal foreground owner changed before dispatch");
    t->active=j;
    if (resume) { (void)reap(t,true);if (state(j)==2) return 0; }
    bool member=false;
    for (size_t i=0;i<j->count;++i) if (j->children[i].state!=AE_JOB_ENDED && j->children[i].state!=AE_JOB_UNKNOWN &&
            getpgid(j->children[i].pid)==j->group) member=true;
    if (!member || tcsetpgrp(t->fd,j->group)) return failure(t,"cannot select original job process group");
    if (resume && j->modes_valid && tcsetattr(t->fd,TCSANOW,&j->modes)) return failure(t,"cannot restore stopped job terminal modes");
    if (resume && state(j)==1) {
        if (kill(-j->group,SIGCONT)) return failure(t,"cannot resume original job");
        j->continuing=true;
    }
    return 0;
}
int ae_shell_job_wait(struct ae_shell_terminal *t,struct ae_shell_job *j) {
    for (;;) {
        (void)reap(t,true);
        if (t->shutdown_signal) return 128+t->shutdown_signal;
        int current=state(j);
        if ((current==1 && !j->continuing) || current==2) {
            if (current==1) {
                if (!owns(t,j->group) || tcgetattr(t->fd,&j->modes)) return failure(t,"cannot observe stopped job terminal modes");
                j->modes_valid=true;notice(t,j,current);j->notified=current;
            }
            int status=result(j);
            if (restore(t,j)) return 125;
            t->active=NULL;
            if (current==2) memset(j,0,sizeof(*j));
            return status;
        }
        struct pollfd fd={t->signals,POLLIN,0};int n;
        do { n=poll(&fd,1,-1); } while (n<0 && errno==EINTR);
        if (n<0 || (fd.revents&(POLLERR|POLLHUP|POLLNVAL))) return failure(t,"foreground job signal wait failed");
        if (signals(t)<0) return 125;
    }
}
void ae_shell_job_cancel(struct ae_shell_terminal *t,struct ae_shell_job *j) {
    if (t->active==j) { (void)restore(t,j);t->active=NULL; }
    memset(j,0,sizeof(*j));
}
int ae_shell_jobs_command(struct ae_shell_session *s,const struct ae_shell_command *c) {
    const char *name=c->argv[0];bool list=!strcmp(name,"jobs"),fg=!strcmp(name,"fg"),bg=!strcmp(name,"bg");
    if (!list && !fg && !bg) return -1;
    struct ae_shell_terminal *t=s->terminal;
    if (!t) { dprintf(2,"aetheri-shell: job commands require -i\n");return 2; }
    if (ae_shell_jobs_check(t)) return 125;
    (void)reap(t,true);
    if ((list && c->argc!=1) || c->argc>2) { dprintf(2,"aetheri-shell: usage: jobs | fg [ID] | bg [ID]\n");return 2; }
    if (list) {
        for (unsigned i=0;i<AE_SHELL_JOBS;++i) if (t->jobs[i].id) notice(t,&t->jobs[i],state(&t->jobs[i]));
        return 0;
    }
    uint32_t id=0;
    if (c->argc==2) {
        const unsigned char *p=(const unsigned char *)c->argv[1];
        if (!*p) return 2;
        for (;*p;++p) { if (*p<'0' || *p>'9' || id>(UINT32_MAX-(unsigned)(*p-'0'))/10u) return 2;id=id*10u+(unsigned)(*p-'0'); }
        if (!id) return 2;
    } else for (unsigned i=0;i<AE_SHELL_JOBS;++i) if (t->jobs[i].id>id && (!bg || state(&t->jobs[i])==1)) id=t->jobs[i].id;
    struct ae_shell_job *j=NULL;
    for (unsigned i=0;i<AE_SHELL_JOBS;++i) if (id && t->jobs[i].id==id) j=&t->jobs[i];
    if (!j) { dprintf(2,"aetheri-shell: no such original job\n");return 1; }
    if (fg) return ae_shell_job_take(t,j,true) ? 125 : ae_shell_job_wait(t,j);
    if (state(j)!=1) { dprintf(2,"aetheri-shell: job is not stopped\n");return 1; }
    bool member=false;
    for (size_t i=0;i<j->count;++i) if (j->children[i].state==AE_JOB_STOPPED && getpgid(j->children[i].pid)==j->group) member=true;
    if (!member || kill(-j->group,SIGCONT)) return failure(t,"cannot continue original background job");
    j->continuing=true;dprintf(t->fd,"[%u] continue requested\n",j->id);return 0;
}
int ae_shell_jobs_shutdown(struct ae_shell_terminal *t) {
    int stop_signal=t->shutdown_signal ? t->shutdown_signal : SIGHUP;
    for (unsigned i=0;i<AE_SHELL_JOBS;++i) for (size_t k=0;k<t->jobs[i].count;++k) {
        struct ae_shell_child *c=&t->jobs[i].children[k];
        if (c->state==AE_JOB_RUNNING || c->state==AE_JOB_STOPPED) { (void)kill(c->pid,stop_signal);(void)kill(c->pid,SIGCONT); }
    }
    t->active=NULL;
    while (ae_shell_jobs_busy(t)) {
        (void)reap(t,false);
        bool unknown=false;
        for (unsigned i=0;i<AE_SHELL_JOBS;++i) if (t->jobs[i].id && state(&t->jobs[i])==3) unknown=true;
        if (unknown || !ae_shell_jobs_busy(t)) break;
        struct pollfd fd={t->signals,POLLIN,0};int n;
        do { n=poll(&fd,1,-1); } while (n<0 && errno==EINTR);
        if (n<0 || signals(t)<0) { t->failed=true;break; }
    }
    return t->failed ? 125 : 128+stop_signal;
}
