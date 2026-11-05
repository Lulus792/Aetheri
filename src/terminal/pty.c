#define _GNU_SOURCE
#include "pty.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
extern char **environ;
int ae_terminal_pty_resize(struct ae_terminal_pty *p,unsigned cols,unsigned rows) {
    if (cols<2 || cols>240 || rows<2 || rows>100) { errno=EINVAL;return -1; }
    struct winsize size={.ws_col=(unsigned short)cols,.ws_row=(unsigned short)rows};
    return ioctl(p->master,TIOCSWINSZ,&size);
}
int ae_terminal_pty_open(struct ae_terminal_pty *p,int source,unsigned cols,unsigned rows) {
    *p=(struct ae_terminal_pty){.master=-1,.pin=-1,.pid=-1};struct stat st;
    if (source<3 || getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() || fstat(source,&st) || !S_ISREG(st.st_mode)) { errno=EINVAL;return -1; }
    struct sigaction inherited;if (sigaction(SIGCHLD,NULL,&inherited) || inherited.sa_handler==SIG_IGN || (inherited.sa_flags&SA_NOCLDWAIT)) { errno=EINVAL;return -1; }
    for (int i=0;i<3;++i) if (fcntl(i,F_GETFD)<0) return -1;
    int master=posix_openpt(O_RDWR|O_NOCTTY|O_CLOEXEC|O_NONBLOCK);
    if (master<0) return -1;
    p->master=master;
    int slave=-1;
    if (grantpt(master) || unlockpt(master) || ae_terminal_pty_resize(p,cols,rows) ||
            (slave=ioctl(master,TIOCGPTPEER,O_RDWR|O_NOCTTY|O_CLOEXEC))<0) goto failed;
    struct termios modes;
    if (tcgetattr(slave,&modes)) goto failed;
    modes.c_iflag=ICRNL|IXON|IUTF8;modes.c_oflag=OPOST|ONLCR;
    modes.c_lflag=ISIG|ICANON|ECHO|ECHOE|ECHOK|IEXTEN|ECHOCTL|ECHOKE;
    modes.c_cflag&=(tcflag_t)~(CSIZE|PARENB);modes.c_cflag|=CS8|CREAD;
    modes.c_cc[VINTR]=3;modes.c_cc[VQUIT]=28;modes.c_cc[VERASE]=127;modes.c_cc[VKILL]=21;modes.c_cc[VEOF]=4;modes.c_cc[VSUSP]=26;
    if (tcsetattr(slave,TCSANOW,&modes)) goto failed;
    pid_t pid=fork();if (pid<0) goto failed;
    if (!pid) {
        if (setsid()<0 || ioctl(slave,TIOCSCTTY,0)) _Exit(125);
        for (int i=0;i<3;++i) if (dup2(slave,i)<0) _Exit(125);
        const int sigs[]={SIGINT,SIGQUIT,SIGHUP,SIGTERM,SIGCHLD,SIGPIPE,SIGTSTP,SIGTTIN,SIGTTOU};
        struct sigaction action={.sa_handler=SIG_DFL};sigemptyset(&action.sa_mask);
        for (unsigned i=0;i<sizeof(sigs)/sizeof(sigs[0]);++i) if (sigaction(sigs[i],&action,NULL)) _Exit(125);
        sigset_t mask;sigemptyset(&mask);if (sigprocmask(SIG_SETMASK,&mask,NULL)) _Exit(125);
        if (unsetenv("WAYLAND_SOCKET") || fcntl(source,F_SETFD,FD_CLOEXEC)) _Exit(125);
        if (source>3 && close_range(3,(unsigned)source-1,0)) _Exit(125);
        if (close_range((unsigned)source+1,UINT_MAX,0)) _Exit(125);
        char *args[]={"aetheri-shell","-i",NULL};execveat(source,"",args,environ,AT_EMPTY_PATH);_Exit(126);
    }
    p->pid=pid;close(slave);slave=-1;
    p->pin=(int)syscall(SYS_pidfd_open,pid,0);
    if (p->pin<0) {
        /* No input has been sent to this fresh trusted shell. Closing its PTY
           aborts setup/hangs up the idle shell; retain/wait the original child. */
        int saved=errno;close(master);p->master=-1;int status;pid_t waited;
        do { waited=waitpid(pid,&status,0); } while (waited<0 && errno==EINTR);
        p->ended=waited==pid;p->eof=true;errno=saved;return -1;
    }
    return 0;
failed: {
        int saved=errno;if (slave>=0) close(slave);close(master);p->master=-1;errno=saved;return -1;
    }
}
ssize_t ae_terminal_pty_read(struct ae_terminal_pty *p,void *buffer,size_t length) {
    if (!length) { errno=EINVAL;return -1; }
    ssize_t n=read(p->master,buffer,length);
    if (!n || (n<0 && errno==EIO)) { p->eof=true;return 0; }
    return n;
}
int ae_terminal_pty_observe(struct ae_terminal_pty *p) {
    if (p->pid<=0) { errno=EINVAL;return -1; }
    if (p->ended) return 1;
    int status=0;pid_t result=waitpid(p->pid,&status,WNOHANG);
    if (!result || (result<0 && errno==EINTR)) return 0;
    if (result!=p->pid) return -1;
    p->ended=true;p->status=WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128+WTERMSIG(status) : 125;return 1;
}
int ae_terminal_pty_hangup(struct ae_terminal_pty *p) {
    if (p->hangup || p->ended) return 0;
    p->hangup=true;return (int)syscall(SYS_pidfd_send_signal,p->pin,SIGHUP,NULL,0);
}
int ae_terminal_pty_close(struct ae_terminal_pty *p) {
    if (p->pid>0 && (!p->ended || !p->eof)) { errno=EBUSY;return -1; }
    if (p->master>=0) close(p->master);
    if (p->pin>=0) close(p->pin);
    p->master=p->pin=-1;return 0;
}
