#define _GNU_SOURCE
#include "terminal/pty.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s (%s)\n",__LINE__,#x,strerror(errno));exit(1); } } while (0)
static void receive(struct ae_terminal_pty *p,char *text,size_t capacity,const char *expected) {
    size_t used=strlen(text);
    for (unsigned rounds=0;rounds<100;++rounds) {
        if (strstr(text,expected)) return;
        struct pollfd fd={p->master,POLLIN,0};CHECK(poll(&fd,1,50)>=0);
        ssize_t n=ae_terminal_pty_read(p,text+used,capacity-used-1);
        if (n>0) { used+=(size_t)n;text[used]=0; }
        else CHECK(n<0 && (errno==EAGAIN || errno==EINTR));
    }
    CHECK(!"PTY expected text deadline");
}
static void settle(struct ae_terminal_pty *p) {
    for (unsigned n=0;n<100 && (!p->ended || !p->eof);++n) {
        struct pollfd fds[2]={{p->eof ? -1 : p->master,POLLIN,0},{p->ended ? -1 : p->pin,POLLIN,0}};CHECK(poll(fds,2,50)>=0);
        if (fds[0].revents) { char bytes[4096];ssize_t size;do { size=ae_terminal_pty_read(p,bytes,sizeof(bytes)); } while (size>0); }
        if (fds[1].revents) CHECK(ae_terminal_pty_observe(p)>=0);
    }
    CHECK(p->ended && p->eof);
}
int main(int argc,char **argv) {
    if (argc==3 && !strcmp(argv[1],"-i") && !strcmp(argv[2],"--edit")) {
        /* Trusted test image: close the PTY but retain the original process.
           The alarm bounds failed-test cleanup without a numeric PID lookup. */
        CHECK(signal(SIGHUP,SIG_IGN)!=SIG_ERR);alarm(8);
        for (int fd=0;fd<3;++fd) close(fd);
        for (;;) pause();
    }
    CHECK(argc==2 && getuid()!=0);int source=open(argv[1],O_RDONLY|O_CLOEXEC);CHECK(source>=3);
    struct ae_terminal_pty p;CHECK(ae_terminal_pty_open(&p,source,80,24)==0);close(source);
    CHECK(p.master>=0 && p.pin>=0 && p.pid>0);CHECK(ae_terminal_pty_close(&p)<0 && errno==EBUSY);
    char byte;CHECK(ae_terminal_pty_read(&p,&byte,0)<0 && errno==EINVAL && !p.eof);
    char text[8192]="";receive(&p,text,sizeof(text),"aetheri> ");
    CHECK(!strstr(text,"Aetheri development"));
    CHECK(ae_terminal_pty_resize(&p,100,32)==0);
    const char command[]="printf 'PTY_NATIVE\\n'\r";CHECK(write(p.master,command,sizeof(command)-1)==(ssize_t)(sizeof(command)-1));
    text[0]=0;receive(&p,text,sizeof(text),"PTY_NATIVE\r\n");
    CHECK(ae_terminal_pty_hangup(&p)==0 && ae_terminal_pty_hangup(&p)==0);
    settle(&p);
    CHECK(p.ended && p.eof && p.status==129);CHECK(ae_terminal_pty_close(&p)==0);

    source=open(argv[1],O_RDONLY|O_CLOEXEC);CHECK(source>=3);
    CHECK(ae_terminal_pty_open(&p,source,80,24)==0);close(source);
    int writer=ioctl(p.master,TIOCGPTPEER,O_RDWR|O_NOCTTY|O_CLOEXEC);CHECK(writer>=3);
    text[0]=0;receive(&p,text,sizeof(text),"aetheri> ");
    CHECK(write(p.master,"exit 0\r",7)==7);
    for (unsigned n=0;n<100 && !p.ended;++n) {
        struct pollfd pin={p.pin,POLLIN,0};CHECK(poll(&pin,1,50)>=0);CHECK(ae_terminal_pty_observe(&p)>=0);
    }
    CHECK(p.ended && p.status==0 && !p.eof);
    CHECK(ae_terminal_pty_close(&p)<0 && errno==EBUSY);
    CHECK(write(writer,"RETAINED_WRITER\n",16)==16);
    text[0]=0;receive(&p,text,sizeof(text),"RETAINED_WRITER\r\n");
    close(writer);settle(&p);CHECK(ae_terminal_pty_close(&p)==0);

    source=open(argv[1],O_RDONLY|O_CLOEXEC);CHECK(source>=3);
    CHECK(ae_terminal_pty_open_welcome(&p,source,80,24)==0);close(source);
    text[0]=0;receive(&p,text,sizeof(text),"aetheri> ");
    CHECK(strstr(text,"Aetheri development") && strstr(text,"Kernel  Linux") && strstr(text,"GiB total") && strstr(text,"Shell   Aetheri profile0"));
    const char welcome_command[]="printf 'WELCOME_ONE\\n'\r";
    CHECK(write(p.master,welcome_command,sizeof(welcome_command)-1)==(ssize_t)(sizeof(welcome_command)-1));
    text[0]=0;receive(&p,text,sizeof(text),"WELCOME_ONE\r\n");
    CHECK(write(p.master,"exit 0\r",7)==7);settle(&p);CHECK(p.status==0 && ae_terminal_pty_close(&p)==0);

    source=open(argv[0],O_RDONLY|O_CLOEXEC);CHECK(source>=3);
    CHECK(ae_terminal_pty_open(&p,source,80,24)==0);close(source);
    for (unsigned n=0;n<100 && !p.eof;++n) {
        struct pollfd fd={p.master,POLLIN,0};CHECK(poll(&fd,1,50)>=0);
        char bytes[128];ssize_t got=ae_terminal_pty_read(&p,bytes,sizeof(bytes));
        CHECK(got>=0 || errno==EAGAIN || errno==EINTR);
    }
    CHECK(p.eof && ae_terminal_pty_observe(&p)==0 && !p.ended);
    CHECK(ae_terminal_pty_close(&p)<0 && errno==EBUSY);
    CHECK(syscall(SYS_pidfd_send_signal,p.pin,SIGTERM,NULL,0)==0);
    settle(&p);CHECK(p.status==143 && ae_terminal_pty_close(&p)==0);
    puts("terminal PTY: original shell, actual input/output, resize, pidfd hangup and independent stream/process end passed");return 0;
}
