#define _GNU_SOURCE
#include "terminal/session.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s errno=%d\n",__FILE__,__LINE__,#x,errno);exit(1); } } while (0)

ssize_t __real_write(int,const void *,size_t);
static int fault_fd=-1,fault_step;
ssize_t __wrap_write(int fd,const void *bytes,size_t size) {
    if (fd==fault_fd) {
        if (fault_step==1) { ++fault_step;errno=EINTR;return -1; }
        if (fault_step==2) { ++fault_step;return __real_write(fd,bytes,size>17 ? 17 : size); }
        if (fault_step==3) { ++fault_step;errno=EAGAIN;return -1; }
        if (fault_step==5) { ++fault_step;errno=EPIPE;return -1; }
        if (fault_step==7) { ++fault_step;return 0; }
    }
    return __real_write(fd,bytes,size);
}
static void bounded_io(struct ae_terminal *m) {
    struct ae_terminal_session s=AE_TERMINAL_SESSION_INIT;s.model=m;
    CHECK(!ae_terminal_session_accepting(&s));
    CHECK(!ae_terminal_session_enqueue(&s,"x",1) && errno==EBUSY);
    int fds[2];CHECK(socketpair(AF_UNIX,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0,fds)==0);
    s.pty.master=fds[0];s.pty.pin=(int)syscall(SYS_pidfd_open,getpid(),0);s.pty.pid=getpid();CHECK(s.pty.pin>=0);
    ae_terminal_init(m,80,24,NULL);ae_terminal_feed(m,"selected\r\nnext",14);
    CHECK(ae_terminal_select_begin(m,0,0) && ae_terminal_select_extend(m,0,8));
    m->view=1;struct ae_term_selection selection=m->selection;
    unsigned char bytes[AE_TERM_INPUT_BYTES+1];for (size_t i=0;i<sizeof(bytes);++i) bytes[i]=(unsigned char)(i%251);
    CHECK(ae_terminal_session_accepting(&s));
    CHECK(!ae_terminal_session_enqueue(&s,bytes,sizeof(bytes)) && errno==ENOBUFS);
    CHECK(s.input_count==0 && s.input_bytes==0 && m->view==1 && !memcmp(&selection,&m->selection,sizeof(selection)));
    CHECK(!ae_terminal_session_enqueue(&s,NULL,1) && errno==EINVAL);
    CHECK(ae_terminal_session_enqueue(&s,NULL,0) && m->view==1 && m->selection.count);
    CHECK(ae_terminal_session_enqueue(&s,bytes,AE_TERM_INPUT_BYTES));
    CHECK(s.input_count==AE_TERM_INPUT_BYTES && s.input_bytes==AE_TERM_INPUT_BYTES && m->view==0 && !m->selection.count);
    CHECK(!ae_terminal_session_enqueue(&s,"x",1) && errno==ENOBUFS && s.input_bytes==AE_TERM_INPUT_BYTES);
    fault_fd=s.pty.master;fault_step=1;
    CHECK(ae_terminal_session_flush(&s)==0 && fault_step==4 && s.input_count==AE_TERM_INPUT_BYTES-17);
    CHECK(!memcmp(s.input,bytes+17,s.input_count));unsigned char result[AE_TERM_INPUT_BYTES];
    CHECK(read(fds[1],result,sizeof(result))==17 && !memcmp(result,bytes,17));
    CHECK(!ae_terminal_session_enqueue(&s,bytes,18) && errno==ENOBUFS);
    fault_step=5;CHECK(ae_terminal_session_flush(&s)<0 && errno==EPIPE && fault_step==6);
    CHECK(s.input_count==AE_TERM_INPUT_BYTES-17 && !memcmp(s.input,bytes+17,s.input_count));
    fault_step=7;CHECK(ae_terminal_session_flush(&s)<0 && errno==EIO && fault_step==8);
    CHECK(s.input_count==AE_TERM_INPUT_BYTES-17);
    s.closing=true;CHECK(!ae_terminal_session_enqueue(&s,"new",3) && errno==EBUSY);
    CHECK(ae_terminal_session_flush(&s)==0 && s.input_count==0);
    CHECK(read(fds[1],result,sizeof(result))==AE_TERM_INPUT_BYTES-17 && !memcmp(result,bytes+17,AE_TERM_INPUT_BYTES-17));
    CHECK(s.input_bytes==AE_TERM_INPUT_BYTES);fault_fd=-1;

    /* Real queued output, with a per-turn bound and no invented child end. */
    unsigned char output[20000];memset(output,'X',sizeof(output));
    CHECK(write(fds[1],output,sizeof(output))==(ssize_t)sizeof(output));
    bool changed=false;CHECK(ae_terminal_session_read(&s,&changed)==0 && changed);
    unsigned char unread[sizeof(output)];
    CHECK(recv(fds[0],unread,sizeof(unread),MSG_PEEK)==(ssize_t)(sizeof(output)-AE_TERM_READ_BUDGET));
    CHECK(!memcmp(unread,output,sizeof(output)-AE_TERM_READ_BUDGET));
    CHECK(!s.pty.ended && !s.pty.eof);
    CHECK(ae_terminal_session_read(&s,&changed)==0 && changed);
    uint64_t sequence=m->sequence;CHECK(ae_terminal_session_read(&s,&changed)==0 && !changed && m->sequence==sequence);
    close(fds[1]);CHECK(ae_terminal_session_read(&s,&changed)==0 && !changed && s.pty.eof && !s.pty.ended);
    CHECK(!ae_terminal_session_enqueue(&s,"x",1) && errno==EBUSY);
    CHECK(ae_terminal_pty_close(&s.pty)<0 && errno==EBUSY);
    close(s.pty.master);close(s.pty.pin);
}
static bool has_text(const struct ae_terminal *m,const char *expected) {
    for (unsigned y=0;y<m->rows;++y) {
        char row[AE_TERM_COLS+1];const struct ae_term_cell *cells=ae_terminal_row(m,y);
        for (unsigned x=0;x<m->cols;++x) row[x]=cells[x].code<128 ? (char)cells[x].code : '?';
        row[m->cols]=0;if (strstr(row,expected)) return true;
    }
    return false;
}
static void turn(struct ae_terminal_session *s) {
    struct pollfd fds[2]={{s->pty.eof ? -1 : s->pty.master,(short)(POLLIN|(s->input_count ? POLLOUT : 0)),0},
        {s->pty.ended ? -1 : s->pty.pin,POLLIN,0}};
    int n;do { n=poll(fds,2,50); } while (n<0 && errno==EINTR);CHECK(n>=0);
    bool changed;CHECK(ae_terminal_session_read(s,&changed)==0);
    CHECK(ae_terminal_pty_observe(&s->pty)>=0);CHECK(ae_terminal_session_flush(s)==0);
}
static void receive(struct ae_terminal_session *s,const char *expected) {
    for (unsigned n=0;n<100 && !has_text(s->model,expected);++n) turn(s);
    CHECK(has_text(s->model,expected));
}
static void original_shell(struct ae_terminal *m,const char *shell) {
    struct ae_terminal_session s=AE_TERMINAL_SESSION_INIT;s.model=m;ae_terminal_init(m,80,24,NULL);
    int source=open(shell,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);CHECK(source>=3);
    CHECK(ae_terminal_pty_open(&s.pty,source,80,24)==0);close(source);
    CHECK(ae_terminal_pty_close(&s.pty)<0 && errno==EBUSY);receive(&s,"aetheri> ");
    CHECK(ae_terminal_session_resize(&s,100,32)==0 && m->cols==100 && m->rows==32);
    CHECK(ae_terminal_select_begin(m,0,0) && ae_terminal_select_extend(m,0,4));
    uint64_t sequence=m->sequence;
    CHECK(ae_terminal_session_resize(&s,241,32)<0 && errno==EINVAL && m->sequence==sequence && m->selection.count);
    const char command[]="printf '%s%s\\n' SESSION_ NATIVE\r";
    CHECK(ae_terminal_session_enqueue(&s,command,sizeof(command)-1) && !m->selection.count);
    CHECK(ae_terminal_session_flush(&s)==0);receive(&s,"SESSION_NATIVE");
    CHECK(!s.closing && !s.pty.ended && !s.pty.eof);
    CHECK(ae_terminal_session_request_close(&s)==0 && s.closing && s.pty.hangup);
    CHECK(!ae_terminal_session_enqueue(&s,"exit\r",5) && errno==EBUSY);
    CHECK(ae_terminal_session_request_close(&s)==0);
    for (unsigned n=0;n<100 && (!s.pty.ended || !s.pty.eof);++n) turn(&s);
    CHECK(s.pty.ended && s.pty.eof && s.pty.status==129);CHECK(ae_terminal_pty_close(&s.pty)==0);
}
int main(int argc,char **argv) {
    CHECK(argc==2 && getuid()!=0);struct ae_terminal *model=malloc(sizeof(*model));CHECK(model);
    bounded_io(model);original_shell(model,argv[1]);free(model);
    puts("terminal session: bounded atomic intake, partial/error suffix, original output budget, shell and independent close passed");return 0;
}
