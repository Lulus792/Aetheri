/* Explicit inert administrative carrier fixture. No PAM or actual login. */
#define _GNU_SOURCE
#include "login/service.h"
#include "login/connection.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"service check %d: %s errno=%d\n",__LINE__,#x,errno);exit(1);}}while(0)
int __real_ioctl(int,unsigned long,...);
int __wrap_ioctl(int fd,unsigned long request,...) {
    va_list args;va_start(args,request);void *data=va_arg(args,void *);va_end(args);
    int r=__real_ioctl(fd,request,data);if(r<0)fprintf(stderr,"fixture ioctl failed: %d\n",errno);return r;
}
int __real_sd_bus_get_name_creds(sd_bus *,const char *,uint64_t,sd_bus_creds **);
int __wrap_sd_bus_get_name_creds(sd_bus *bus,const char *name,uint64_t mask,sd_bus_creds **out) {
    int r=__real_sd_bus_get_name_creds(bus,name,mask,out);if(r<0)fprintf(stderr,"fixture manager credentials failed: %d\n",r);return r;
}
int __real_getpidcon_raw(pid_t,char **);
int __wrap_getpidcon_raw(pid_t pid,char **out) {int r=__real_getpidcon_raw(pid,out);if(r<0)fprintf(stderr,"fixture current label failed: %d\n",errno);else fprintf(stderr,"fixture current label: %.511s\n",*out);return r;}
int __real_getsockopt(int,int,int,void *,socklen_t *);
int __wrap_getsockopt(int fd,int level,int option,void *value,socklen_t *size) {int r=__real_getsockopt(fd,level,option,value,size);if(!r && level==SOL_SOCKET && option==SO_PEERSEC)fprintf(stderr,"fixture socket label: %.*s\n",(int)*size,(char *)value);return r;}
int __real_sd_pidfd_get_cgroup(int,char **);
int __wrap_sd_pidfd_get_cgroup(int fd,char **out) {int r=__real_sd_pidfd_get_cgroup(fd,out);if(r<0)fprintf(stderr,"fixture process cgroup failed: %d\n",r);return r;}
static const char fixture_label[]="system_u:system_r:unconfined_service_t:s0";
static const char fixture_socket_label[]="system_u:system_r:unconfined_service_t:s0-s0:c0.c1023";
static size_t fd_count(void) {DIR *d=opendir("/proc/self/fd");CHECK(d);size_t n=0;while(readdir(d))++n;closedir(d);return n;}
static int connect_fixture(const char *path) {
    CHECK(strlen(path)<sizeof(((struct sockaddr_un *)0)->sun_path));int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);CHECK(fd>=0);
    struct sockaddr_un a={.sun_family=AF_UNIX};memcpy(a.sun_path,path,strlen(path)+1);CHECK(!connect(fd,(struct sockaddr *)&a,sizeof(a)));return fd;
}
static int server(const char *path) {
    CHECK(!getuid() && !geteuid());CHECK(strlen(path)<sizeof(((struct sockaddr_un *)0)->sun_path));int listener=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);CHECK(listener>=0);
    struct sockaddr_un a={.sun_family=AF_UNIX};memcpy(a.sun_path,path,strlen(path)+1);CHECK(!bind(listener,(struct sockaddr *)&a,sizeof(a)));CHECK(!chmod(path,0666) && !listen(listener,8));
    puts("AETHERI_LOGIN_SERVICE_FIXTURE_LISTENING");fflush(stdout);
    bool quit=false;while(!quit){int fd=accept4(listener,NULL,NULL,SOCK_CLOEXEC);CHECK(fd>=0);char byte;ssize_t n=recv(fd,&byte,1,0);CHECK(n==0 || n==1);quit=n==1 && byte=='q';close(fd);}
    close(listener);puts("AETHERI_LOGIN_SERVICE_FIXTURE_ENDED");return 0;
}
static void *wrong_thread(void *data) {struct ae_login_service *s=data;CHECK(ae_login_service_check(s)==-EXDEV);struct ae_login_service *copy=s;CHECK(ae_login_service_close(&copy)==-EXDEV && copy==s);return NULL;}
static int client(const char *path,const char *unit) {
    CHECK(getuid()!=0);sd_bus *bus=NULL;CHECK(sd_bus_open_system(&bus)>=0);CHECK(sd_bus_set_method_call_timeout(bus,2000000)>=0);
    struct ae_login_service_expect expected={.unit=unit,.label=fixture_label,.socket_label=fixture_socket_label,.uid=0,.gid=0};struct ae_login_service *s=NULL;int fd;
    for(unsigned i=0;i<5;++i){
        struct ae_login_service_expect bad=expected;if(i==0)bad.unit="aetheri-login-observer-wrong.service";if(i==1)bad.label="system_u:system_r:init_t:s0";if(i==2)bad.uid=1000;if(i==3)bad.gid=1000;if(i==4)bad.socket_label="system_u:system_r:unconfined_service_t:s0";
        fd=connect_fixture(path);int flags=fcntl(fd,F_GETFL);size_t before=fd_count();int r=ae_login_service_create(fd,bus,&bad,&s);CHECK(r<0 && !s && fcntl(fd,F_GETFL)==flags && fd_count()==before);close(fd);
    }
    fd=connect_fixture(path);int flags=fcntl(fd,F_GETFL);int created=ae_login_service_create(fd,bus,&expected,&s);if(created)fprintf(stderr,"fixture create failed: %d\n",created);CHECK(!created && s && fcntl(fd,F_GETFL)==flags);CHECK(!ae_login_service_check(s));
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,wrong_thread,s));CHECK(!pthread_join(thread,NULL));CHECK(!ae_login_service_check(s));
    int pin=-1;CHECK(!ae_login_service_peer(s,&pin) && pin>=0 && (fcntl(pin,F_GETFD)&FD_CLOEXEC));
    /* Service-only composition is valid for this inert fixture, not production
       image/configuration/greeter admission. No username or secret is sent. */
    struct ae_login_connection_source source={.peer_pidfd=pin,.uid=0,.gid=0,.check=ae_login_service_check,.userdata=s};
    struct ae_login_connection *connection=NULL;CHECK(!ae_login_connection_create(&connection,&fd,&source) && fd==-1);
    struct ae_login_status status;CHECK(!ae_login_connection_status(connection,&status) && status.phase==AE_LOGIN_IDLE);
    CHECK(!ae_login_connection_close(&connection) && !connection);CHECK(!ae_login_service_check(s));fd=connect_fixture(path);
    CHECK(send(fd,"q",1,MSG_NOSIGNAL)==1);struct pollfd p={.fd=pin,.events=POLLIN};CHECK(poll(&p,1,10000)==1 && (p.revents&POLLIN));
    int r=ae_login_service_check(s);CHECK(r<0 && ae_login_service_check(s)==r);int unchanged=123;CHECK(ae_login_service_peer(s,&unchanged)==r && unchanged==123);
    CHECK(!ae_login_service_close(&s) && !s);close(fd);sd_bus_unref(bus);puts("login.service: actual Root peer/current service, wrong selections, owner and terminal fences pass");return 0;
}
static int child_client(const char *path,const char *unit) {
    CHECK(getuid()!=0);sd_bus *bus=NULL;CHECK(sd_bus_open_system(&bus)>=0);CHECK(sd_bus_set_method_call_timeout(bus,2000000)>=0);
    struct ae_login_service_expect expected={.unit=unit,.label=fixture_label,.socket_label=fixture_socket_label,.uid=0,.gid=0};
    struct ae_login_service *s=NULL;int fd=connect_fixture(path);int r=ae_login_service_create(fd,bus,&expected,&s);bool rejected=r==-EACCES && !s;
    CHECK(!ae_login_service_close(&s));CHECK(send(fd,"q",1,MSG_NOSIGNAL)==1);close(fd);sd_bus_unref(bus);
    CHECK(rejected);puts("login.service: same-UID/domain/unit child rejected as non-MainPID");return 0;
}
int main(int argc,char **argv) {
    if(argc==3 && !strcmp(argv[1],"--server"))return server(argv[2]);
    if(argc==4 && !strcmp(argv[1],"--client"))return client(argv[2],argv[3]);
    if(argc==3 && !strcmp(argv[1],"--server-child")) {
        CHECK(!getuid());pid_t child=fork();CHECK(child>=0);if(!child)_exit(server(argv[2]));
        int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status));return WEXITSTATUS(status);
    }
    if(argc==4 && !strcmp(argv[1],"--client-child"))return child_client(argv[2],argv[3]);
    return 77;
}
