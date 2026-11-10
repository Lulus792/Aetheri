/* Ordinary-user synthetic socket conversations only; no real credentials/PAM. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
static ssize_t connection_test_send(int,const void *,size_t,int);
static ssize_t connection_test_recvmsg(int,struct msghdr *,int);
#define send connection_test_send
#define recvmsg connection_test_recvmsg
#include "../src/login/connection.c"
#undef send
#undef recvmsg
#define CHECK(x) do{if(!(x)){fprintf(stderr,"connection check %d: %s errno=%d\n",__LINE__,#x,errno);abort();}}while(0)
static int send_fault,receive_fault;
static size_t send_limit;
static size_t control_limit;
static ssize_t connection_test_send(int fd,const void *body,size_t size,int flags) {
    CHECK(flags==(MSG_DONTWAIT|MSG_NOSIGNAL));
    if(send_fault){errno=send_fault;send_fault=0;return -1;}
    if(send_limit && size>send_limit)size=send_limit;
    return send(fd,body,size,flags);
}
static ssize_t connection_test_recvmsg(int fd,struct msghdr *message,int flags) {
    CHECK(flags==(MSG_DONTWAIT|MSG_CMSG_CLOEXEC));
    if(receive_fault){errno=receive_fault;receive_fault=0;return -1;}
    if(control_limit && message->msg_controllen>control_limit)message->msg_controllen=control_limit;
    return recvmsg(fd,message,flags);
}
struct guard {unsigned calls;int error;struct ae_login_connection *connection;bool reenter;};
static int check_guard(void *p) {
    struct guard *g=p;++g->calls;
    if(g->reenter){CHECK(ae_login_connection_pump(g->connection)==-EBUSY);struct ae_login_connection *copy=g->connection;CHECK(ae_login_connection_close(&copy)==-EBUSY && copy==g->connection);}
    return g->error;
}
struct fixture {struct ae_login_connection *c;int server;struct guard guard;};
static void fresh(struct fixture *f) {
    memset(f,0,sizeof(*f));int pair[2];CHECK(!socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,pair));
    int pin=pidfd_open(getpid(),0);CHECK(pin>=0);
    struct ae_login_connection_source source={.peer_pidfd=pin,.uid=getuid(),.gid=getgid(),.check=check_guard,.userdata=&f->guard};
    CHECK(!ae_login_connection_create(&f->c,&pair[0],&source) && pair[0]==-1);close(pin);f->server=pair[1];f->guard.connection=f->c;
    CHECK(fcntl(f->c->socket,F_GETFD)&FD_CLOEXEC);CHECK(fcntl(f->c->peer,F_GETFD)&FD_CLOEXEC);
}
static enum ae_login_phase phase(struct fixture *f) {struct ae_login_status s;CHECK(!ae_login_connection_status(f->c,&s));return s.phase;}
static void end(struct fixture *f) {CHECK(!ae_login_connection_close(&f->c) && !f->c);if(f->server>=0)close(f->server);}
static void request(struct fixture *f,const char *expected) {
    CHECK(phase(f)==AE_LOGIN_SENDING);CHECK(!ae_login_connection_pump(f->c));CHECK(phase(f)==AE_LOGIN_WAITING);
    uint8_t bytes[8192];ssize_t n=recv(f->server,bytes,sizeof(bytes),MSG_DONTWAIT);CHECK(n>4);uint32_t length;memcpy(&length,bytes,4);CHECK(length==(uint32_t)n-4);
    CHECK(strlen(expected)==length && !memcmp(bytes+4,expected,length));
}
static void transmit(struct fixture *f,const char *body,bool fragmented) {
    uint8_t bytes[8192];size_t size=strlen(body);CHECK(size+4<sizeof(bytes));uint32_t length=(uint32_t)size;memcpy(bytes,&length,4);memcpy(bytes+4,body,size);
    if(fragmented){for(size_t i=0;i<size+4;++i){CHECK(send(f->server,bytes+i,1,MSG_NOSIGNAL)==1);CHECK(ae_login_connection_pump(f->c)==(i==size+3?1:0));}}
    else{CHECK(send(f->server,bytes,size+4,MSG_NOSIGNAL)==(ssize_t)size+4);CHECK(ae_login_connection_pump(f->c)==1);}
}
static void conversation(void) {
    struct fixture f;fresh(&f);int fd=-1,pin=-1;short events=0;
    CHECK(!ae_login_connection_watch(f.c,&fd,&events,&pin) && fd>=0 && pin>=0 && events==POLLIN);
    CHECK(ae_login_connection_start(f.c)==-EBUSY);CHECK(!ae_login_connection_begin(f.c,"original",8));
    CHECK(!ae_login_connection_watch(f.c,&fd,&events,&pin) && events==(POLLIN|POLLOUT));
    request(&f,"{\"type\":\"create_session\",\"username\":\"original\"}");
    transmit(&f,"{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Synthetic question\"}",true);
    CHECK(phase(&f)==AE_LOGIN_PROMPT && ae_login_connection_answer(f.c,NULL,0)==-EINVAL);
    CHECK(!ae_login_connection_answer(f.c,"synthetic",9));request(&f,"{\"type\":\"post_auth_message_response\",\"response\":\"synthetic\"}");
    transmit(&f,"{\"type\":\"success\"}",false);CHECK(phase(&f)==AE_LOGIN_AUTHENTICATED);
    f.guard.reenter=true;CHECK(!ae_login_connection_start(f.c));f.guard.reenter=false;
    request(&f,"{\"type\":\"start_session\",\"cmd\":[\"/usr/libexec/aetheri/aetheri-session\"],\"env\":[\"XDG_SESSION_TYPE=wayland\",\"XDG_CURRENT_DESKTOP=Aetheri\",\"XDG_SESSION_DESKTOP=Aetheri\"]}");
    transmit(&f,"{\"type\":\"success\"}",false);CHECK(phase(&f)==AE_LOGIN_START_ACKNOWLEDGED);
    close(f.server);f.server=-1;CHECK(ae_login_connection_pump(f.c)==-ECONNRESET && phase(&f)==AE_LOGIN_START_ACKNOWLEDGED);
    CHECK(ae_login_connection_start(f.c)==-ECONNRESET);end(&f);
}
static void interrupted(void) {
    struct fixture f;fresh(&f);CHECK(!ae_login_connection_begin(f.c,"original",8));
    send_limit=3;CHECK(!ae_login_connection_pump(f.c) && phase(&f)==AE_LOGIN_SENDING);send_limit=0;
    CHECK(ae_login_connection_interrupt(f.c)==-ECANCELED && phase(&f)==AE_LOGIN_UNKNOWN);
    CHECK(ae_login_connection_pump(f.c)==-ECANCELED && ae_login_connection_begin(f.c,"original",8)==-ECANCELED);
    uint8_t bytes[256];CHECK(recv(f.server,bytes,sizeof(bytes),MSG_DONTWAIT)==3);CHECK(recv(f.server,bytes,sizeof(bytes),MSG_DONTWAIT)==-1 && errno==EAGAIN);end(&f);
}
static void partial_and_would_block(void) {
    struct fixture f;fresh(&f);CHECK(!ae_login_connection_begin(f.c,"original",8));
    const uint8_t *bytes;size_t size;CHECK(!ae_login_output(f.c->model,&bytes,&size));
    send_fault=EAGAIN;CHECK(!ae_login_connection_pump(f.c) && phase(&f)==AE_LOGIN_SENDING && bytes[0]);
    receive_fault=EINTR;CHECK(!ae_login_connection_pump(f.c) && phase(&f)==AE_LOGIN_SENDING && bytes[0]);
    send_fault=EINTR;CHECK(!ae_login_connection_pump(f.c) && phase(&f)==AE_LOGIN_SENDING && bytes[0]);
    send_limit=1;for(size_t i=0;i<size;++i){CHECK(!ae_login_connection_pump(f.c));CHECK(bytes[i]==0);}send_limit=0;
    CHECK(phase(&f)==AE_LOGIN_WAITING);uint8_t sent[8192];CHECK(recv(f.server,sent,sizeof(sent),MSG_DONTWAIT)==(ssize_t)size);
    CHECK(!ae_login_connection_pump(f.c));transmit(&f,"{\"type\":\"error\",\"error_type\":\"auth_error\",\"description\":\"Synthetic denial\"}",false);
    CHECK(phase(&f)==AE_LOGIN_DENIED && ae_login_connection_begin(f.c,"original",8)==-EBUSY);
    CHECK(!ae_login_connection_cancel(f.c));request(&f,"{\"type\":\"cancel_session\"}");transmit(&f,"{\"type\":\"success\"}",false);CHECK(phase(&f)==AE_LOGIN_IDLE);end(&f);
}
static void losses_and_early_input(void) {
    struct fixture f;fresh(&f);CHECK(!ae_login_connection_begin(f.c,"original",8));
    CHECK(send(f.server,"x",1,MSG_NOSIGNAL)==1);CHECK(ae_login_connection_pump(f.c)==-EPROTO && phase(&f)==AE_LOGIN_UNKNOWN);
    char byte;CHECK(recv(f.server,&byte,1,MSG_DONTWAIT)==-1 && errno==EAGAIN);end(&f);
    fresh(&f);CHECK(!ae_login_connection_begin(f.c,"original",8));request(&f,"{\"type\":\"create_session\",\"username\":\"original\"}");
    uint32_t length=100;CHECK(send(f.server,&length,sizeof(length),MSG_NOSIGNAL)==sizeof(length));CHECK(!ae_login_connection_pump(f.c));close(f.server);f.server=-1;
    CHECK(ae_login_connection_pump(f.c)==-ECONNRESET && phase(&f)==AE_LOGIN_UNKNOWN);end(&f);
    fresh(&f);CHECK(!ae_login_connection_begin(f.c,"original",8));request(&f,"{\"type\":\"create_session\",\"username\":\"original\"}");
    length=AE_LOGIN_WIRE_MAX+1;CHECK(send(f.server,&length,sizeof(length),MSG_NOSIGNAL)==sizeof(length));CHECK(ae_login_connection_pump(f.c)==-EPROTO && phase(&f)==AE_LOGIN_UNKNOWN);end(&f);
    fresh(&f);CHECK(!ae_login_connection_begin(f.c,"original",8));f.guard.error=-ESTALE;
    CHECK(ae_login_connection_pump(f.c)==-ESTALE && phase(&f)==AE_LOGIN_UNKNOWN);f.guard.error=0;CHECK(ae_login_connection_pump(f.c)==-ESTALE);CHECK(recv(f.server,&byte,1,MSG_DONTWAIT)==-1 && errno==EAGAIN);end(&f);
    fresh(&f);CHECK(!ae_login_connection_begin(f.c,"original",8));close(f.server);f.server=-1;CHECK(ae_login_connection_pump(f.c)==-ECONNRESET && phase(&f)==AE_LOGIN_UNKNOWN);end(&f);
}
static size_t fd_count(void) {DIR *d=opendir("/proc/self/fd");CHECK(d);size_t n=0;while(readdir(d))++n;closedir(d);return n;}
static void ancillary(void) {
    for(unsigned truncated=0;truncated<2;++truncated){
    struct fixture f;fresh(&f);int input=open("/dev/null",O_RDONLY|O_CLOEXEC);CHECK(input>=0);size_t before=fd_count();
    int fds[253];for(size_t i=0;i<253;++i)fds[i]=input;
    union {struct cmsghdr align;uint8_t bytes[CMSG_SPACE(sizeof(fds))];} control={0};
    char data='x';struct iovec iov={.iov_base=&data,.iov_len=1};struct msghdr m={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
    struct cmsghdr *c=CMSG_FIRSTHDR(&m);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(fds));memcpy(CMSG_DATA(c),fds,sizeof(fds));
    CHECK(sendmsg(f.server,&m,MSG_NOSIGNAL)==1);control_limit=truncated?CMSG_SPACE(sizeof(int)):0;
    CHECK(ae_login_connection_pump(f.c)==-EPROTO && phase(&f)==AE_LOGIN_UNKNOWN);control_limit=0;CHECK(fd_count()==before);close(input);end(&f);
    }
}
static void original_peer_exit(void) {
    int bootstrap[2],barrier[2];CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,bootstrap));CHECK(!pipe2(barrier,O_CLOEXEC));
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        close(bootstrap[0]);close(barrier[1]);int pair[2];CHECK(!socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,pair));
        union {struct cmsghdr align;uint8_t bytes[CMSG_SPACE(sizeof(pair))];} control={0};char data='x';struct iovec iov={.iov_base=&data,.iov_len=1};
        struct msghdr m={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};struct cmsghdr *c=CMSG_FIRSTHDR(&m);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(pair));memcpy(CMSG_DATA(c),pair,sizeof(pair));
        CHECK(sendmsg(bootstrap[1],&m,MSG_NOSIGNAL)==1);close(pair[0]);close(pair[1]);close(bootstrap[1]);char byte;CHECK(read(barrier[0],&byte,1)==0);close(barrier[0]);_exit(0);
    }
    close(bootstrap[1]);close(barrier[0]);int pin=pidfd_open(child,0);CHECK(pin>=0);
    union {struct cmsghdr align;uint8_t bytes[CMSG_SPACE(2*sizeof(int))];} control={0};char data;struct iovec iov={.iov_base=&data,.iov_len=1};
    struct msghdr m={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};CHECK(recvmsg(bootstrap[0],&m,MSG_CMSG_CLOEXEC)==1 && !(m.msg_flags&MSG_CTRUNC));
    struct cmsghdr *c=CMSG_FIRSTHDR(&m);CHECK(c && c->cmsg_level==SOL_SOCKET && c->cmsg_type==SCM_RIGHTS && c->cmsg_len==CMSG_LEN(2*sizeof(int)));int pair[2];memcpy(pair,CMSG_DATA(c),sizeof(pair));close(bootstrap[0]);
    struct fixture f={.server=pair[1]};struct ae_login_connection_source source={.peer_pidfd=pin,.uid=getuid(),.gid=getgid(),.check=check_guard,.userdata=&f.guard};
    CHECK(!ae_login_connection_create(&f.c,&pair[0],&source));f.guard.connection=f.c;close(pin);CHECK(!ae_login_connection_begin(f.c,"original",8));request(&f,"{\"type\":\"create_session\",\"username\":\"original\"}");
    close(barrier[1]);int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    /* The original socket remains usable through the separately retained server
       endpoint; that does not make its original creator alive or admit replies. */
    CHECK(send(f.server,"x",1,MSG_NOSIGNAL)==1);CHECK(ae_login_connection_pump(f.c)==-ESRCH && phase(&f)==AE_LOGIN_UNKNOWN);end(&f);
}
static void *wrong_thread(void *p) {struct fixture *f=p;CHECK(ae_login_connection_begin(f->c,"original",8)==-EXDEV);CHECK(ae_login_connection_pump(f->c)==-EXDEV);CHECK(ae_login_connection_close(&f->c)==-EXDEV && f->c);return NULL;}
static void ownership(void) {
    struct fixture f;fresh(&f);pthread_t t;CHECK(!pthread_create(&t,NULL,wrong_thread,&f));CHECK(!pthread_join(t,NULL));CHECK(phase(&f)==AE_LOGIN_IDLE);
    pid_t child=fork();CHECK(child>=0);
    if(!child){CHECK(ae_login_connection_pump(f.c)==-ECHILD);CHECK(!ae_login_connection_close(&f.c));close(f.server);_exit(0);}
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);CHECK(phase(&f)==AE_LOGIN_IDLE);end(&f);
}
static void admission_failures(void) {
    int pair[2];CHECK(!socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,pair));int pin=pidfd_open(getpid(),0);CHECK(pin>=0);struct guard g={0};struct ae_login_connection *c=NULL;
    struct ae_login_connection_source source={.peer_pidfd=pin,.uid=getuid(),.gid=getgid(),.check=check_guard,.userdata=&g};size_t before=fd_count();
    source.uid=getuid()+1;CHECK(ae_login_connection_create(&c,&pair[0],&source)==-EACCES && !c && pair[0]>=0 && fd_count()==before);source.uid=getuid();
    g.error=-ESTALE;CHECK(ae_login_connection_create(&c,&pair[0],&source)==-ESTALE && !c && pair[0]>=0 && fd_count()==before);g.error=0;
    source.peer_pidfd=pair[1];CHECK(ae_login_connection_create(&c,&pair[0],&source)==-EPROTO && !c && fd_count()==before);source.peer_pidfd=pin;
    int pipes[2];CHECK(!pipe2(pipes,O_CLOEXEC));pid_t child=fork();CHECK(child>=0);
    if(!child){close(pipes[1]);char x;CHECK(read(pipes[0],&x,1)==0);_exit(0);}
    close(pipes[0]);int other=pidfd_open(child,0);CHECK(other>=0);source.peer_pidfd=other;
    CHECK(ae_login_connection_create(&c,&pair[0],&source)==-ESTALE && !c && pair[0]>=0);close(pipes[1]);int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    close(other);close(pair[0]);close(pair[1]);close(pin);
}
int main(void) {
    if(!getuid())return 77;
    conversation();interrupted();partial_and_would_block();losses_and_early_input();ancillary();original_peer_exit();ownership();admission_failures();
    puts("login.connection: conversations, kernel peer pins, loss fences, partial I/O, ancillary cleanup and ownership pass");return 0;
}
