#define _GNU_SOURCE
#include "login/startup.h"
#include "core/root.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/nsfs.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Controlled service/source/policy/identity adapters, actual nsfs descriptors,
   thread/fork and FD lifetimes. This is not a privileged namespace/PID1 test. */
#define CHECK(x) do {if(!(x)){fprintf(stderr,"startup model line %d: %s\n",__LINE__,#x);abort();}}while(0)
static int socket_fd=-1,space=-1,last_space=-1,tokens[3];
static int policy_error,source_error,enter_error,publish_error;
static int policy_calls,enter_calls,publish_calls,service_closes,source_closes,loaded_closes;
static bool pending,drift,reenter,fail_after_publish;
static bool saved_user,saved_group;
static bool unset_errno;
static struct ae_login_startup *active;
static struct ae_login_source_expect entries[3];
static int gate(void *p) {
    CHECK(p==&tokens);++policy_calls;
    if(reenter && active)CHECK(ae_login_startup_check(active)==-EBUSY);
    return policy_error;
}
int __wrap_getresuid(uid_t *r,uid_t *e,uid_t *s){*r=*e=0;*s=saved_user?1000:0;return 0;}
int __wrap_getresgid(gid_t *r,gid_t *e,gid_t *s){*r=*e=0;*s=saved_group?1000:0;return 0;}
int __wrap_setns(int fd,int type){CHECK(fd>=0 && type==CLONE_NEWNS);++enter_calls;if(enter_error || unset_errno){errno=enter_error;return -1;}return 0;}
int __real_ioctl(int,unsigned long,...);
int __wrap_ioctl(int fd,unsigned long request,...) {
    if(request==PIDFD_GET_MNT_NAMESPACE) {
        CHECK(fd==42);last_space=fcntl(space,F_DUPFD_CLOEXEC,3);CHECK(last_space>=0);return last_space;
    }
    CHECK(request==NS_GET_NSTYPE);return __real_ioctl(fd,request);
}
int __real_fstat(int,struct stat *);
int __wrap_fstat(int fd,struct stat *out) {
    int r=__real_fstat(fd,out);
    if(!r && drift && fd==last_space)++out->st_ino;
    return r;
}
int __wrap_ae_login_service_create(int fd,sd_bus *bus,const struct ae_login_service_expect *e,struct ae_login_service **out) {
    CHECK(fd==socket_fd && bus && e && !*out);*out=(struct ae_login_service *)&tokens[0];return 0;
}
int __wrap_ae_login_service_check(void *p){CHECK(p==&tokens[0]);return 0;}
int __wrap_ae_login_service_peer(struct ae_login_service *p,int *out){CHECK((void *)p==&tokens[0]);*out=42;return 0;}
int __wrap_ae_login_service_close(struct ae_login_service **p){if(*p){CHECK((void *)*p==&tokens[0]);++service_closes;*p=NULL;}return 0;}
int __wrap_ae_login_sources_create(const struct ae_login_source_expect *e,size_t n,struct ae_login_sources **out){CHECK(e==entries && n==3 && !*out);if(source_error)return source_error;*out=(struct ae_login_sources *)&tokens[1];return 0;}
int __wrap_ae_login_sources_close(struct ae_login_sources **p){if(*p){CHECK((void *)*p==&tokens[1]);++source_closes;*p=NULL;}return 0;}
int __wrap_ae_login_loaded_create(struct ae_login_service *s,struct ae_login_sources *c,size_t f,size_t p,size_t d,const size_t *drop,size_t n,struct ae_login_loaded **out) {
    CHECK((void *)s==&tokens[0] && (void *)c==&tokens[1] && f==0 && p==1 && d==2 && !drop && !n && !*out);
    *out=(struct ae_login_loaded *)&tokens[2];return 0;
}
int __wrap_ae_login_loaded_close(struct ae_login_loaded **p){if(*p){CHECK((void *)*p==&tokens[2]);++loaded_closes;*p=NULL;}return 0;}
int __wrap_ae_login_loaded_check_config(void *p){CHECK(p==&tokens[2]);return pending?-EAGAIN:0;}
int __wrap_ae_login_loaded_match_image(struct ae_login_loaded *p,struct ae_image *out){CHECK((void *)p==&tokens[2]);memset(out,0,sizeof *out);return 0;}
int __wrap_ae_root_observe(int fd,struct ae_root_identity *out){CHECK(fd>=0);memset(out,0,sizeof *out);out->mode=S_IFDIR|0755;return 0;}
int __wrap_ae_root_check_path(int fd,const char *path,const struct ae_root_identity *id){CHECK(fd>=0 && !strcmp(path,"/tmp") && id->mode==(S_IFDIR|0755));return 0;}
int __wrap_ae_login_handoff_publish(struct ae_login_loaded *p,int fd,const char *path,const char *leaf,const char *label) {
    CHECK((void *)p==&tokens[2] && fd>=0 && !strcmp(path,"/tmp") && !strcmp(leaf,"observation") && !strcmp(label,"test-label"));
    ++publish_calls;CHECK(ae_login_startup_publish(active)==-EBUSY);
    if(fail_after_publish)policy_error=-EACCES;
    return publish_error;
}
static struct ae_login_startup_expect expectation(void) {
    return (struct ae_login_startup_expect){.service={.unit="selected.service",.label="process",.socket_label="socket"},
        .sources=entries,.source_count=3,.fragment=0,.program=1,.config=2,
        .directory="/tmp",.leaf="observation",.label="test-label",.check=gate,.userdata=&tokens};
}
static sd_bus *bus(void){return (sd_bus *)&tokens;}
static void reset(void) {
    CHECK(!active);policy_error=source_error=enter_error=publish_error=0;
    policy_calls=enter_calls=publish_calls=service_closes=source_closes=loaded_closes=0;
    pending=drift=reenter=fail_after_publish=saved_user=saved_group=unset_errno=false;last_space=-1;
}
static void close_active(void){CHECK(!ae_login_startup_close(&active) && !active && service_closes==1 && source_closes==1 && loaded_closes==1);}
static void *other_thread(void *p) {
    (void)p;CHECK(ae_login_startup_check(active)==-EXDEV);
    struct ae_login_startup *copy=active;CHECK(ae_login_startup_close(&copy)==-EXDEV && copy==active);return NULL;
}
static void lifecycle(void) {
    reset();struct ae_login_startup_expect e=expectation();
    CHECK(!ae_login_startup_create(socket_fd,bus(),&e,&active) && active && enter_calls==1 && !publish_calls);
    pending=true;CHECK(ae_login_startup_check(active)==-EAGAIN);CHECK(ae_login_startup_check(active)==-EAGAIN && !publish_calls);
    pending=false;reenter=true;CHECK(!ae_login_startup_check(active));
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,other_thread,NULL) && !pthread_join(thread,NULL));
    pid_t child=fork();CHECK(child>=0);
    if(!child){CHECK(ae_login_startup_check(active)==-ECHILD && !ae_login_startup_close(&active));_exit(0);}
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(!ae_login_startup_publish(active) && publish_calls==1);
    CHECK(ae_login_startup_publish(active)==-EALREADY && publish_calls==1 && !ae_login_startup_check(active));close_active();
}
static void failures(void) {
    struct ae_login_startup_expect e=expectation();
    reset();saved_user=true;CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-EACCES && !active && !enter_calls);
    reset();saved_group=true;CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-EACCES && !active && !enter_calls);
    reset();e.check=NULL;CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-EINVAL && !active && !enter_calls);e=expectation();
    policy_error=-EACCES;CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-EACCES && !active && !enter_calls);
    reset();enter_error=EPERM;CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-EPERM && !active && enter_calls==1 && service_closes==1);
    reset();unset_errno=true;CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-EIO && !active && enter_calls==1 && service_closes==1);
    reset();source_error=-ESTALE;CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-ESTALE && !active && enter_calls==1 && service_closes==1);
    reset();CHECK(!ae_login_startup_create(socket_fd,bus(),&e,&active));
    drift=true;CHECK(ae_login_startup_check(active)==-ESTALE);drift=false;CHECK(ae_login_startup_check(active)==-ESTALE && ae_login_startup_publish(active)==-ESTALE && !publish_calls);close_active();
    reset();CHECK(!ae_login_startup_create(socket_fd,bus(),&e,&active));policy_error=-EAGAIN;
    CHECK(ae_login_startup_check(active)==-ESTALE);policy_error=0;CHECK(ae_login_startup_check(active)==-ESTALE && !publish_calls);close_active();
    reset();CHECK(!ae_login_startup_create(socket_fd,bus(),&e,&active));publish_error=-EIO;
    CHECK(ae_login_startup_publish(active)==-EIO && publish_calls==1);publish_error=0;CHECK(ae_login_startup_publish(active)==-EIO && publish_calls==1);close_active();
    reset();CHECK(!ae_login_startup_create(socket_fd,bus(),&e,&active));fail_after_publish=true;
    CHECK(ae_login_startup_publish(active)==-EACCES && publish_calls==1);policy_error=0;
    CHECK(ae_login_startup_check(active)==-EACCES && ae_login_startup_publish(active)==-EACCES && publish_calls==1);close_active();
    reset();CHECK(!ae_login_startup_create(socket_fd,bus(),&e,&active));pending=true;
    CHECK(ae_login_startup_publish(active)==-EAGAIN && !publish_calls);pending=false;CHECK(ae_login_startup_publish(active)==-EAGAIN && !publish_calls);close_active();
}
static pthread_barrier_t barrier;
static void *held_thread(void *unused) {
    (void)unused;int r=pthread_barrier_wait(&barrier);CHECK(!r || r==PTHREAD_BARRIER_SERIAL_THREAD);
    r=pthread_barrier_wait(&barrier);CHECK(!r || r==PTHREAD_BARRIER_SERIAL_THREAD);return NULL;
}
static void wrong_namespace(void) {
    reset();struct ae_login_startup_expect e=expectation();
    int original=space;space=open("/proc/self/ns/net",O_RDONLY|O_CLOEXEC);CHECK(space>=0);
    CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-EPROTO && !active && !enter_calls);
    close(space);space=original;
}
static void threaded_constructor(void) {
    reset();struct ae_login_startup_expect e=expectation();pthread_t thread;
    CHECK(!pthread_barrier_init(&barrier,NULL,2) && !pthread_create(&thread,NULL,held_thread,NULL));
    int r=pthread_barrier_wait(&barrier);CHECK(!r || r==PTHREAD_BARRIER_SERIAL_THREAD);
    CHECK(ae_login_startup_create(socket_fd,bus(),&e,&active)==-EBUSY && !active && !enter_calls);
    r=pthread_barrier_wait(&barrier);CHECK(!r || r==PTHREAD_BARRIER_SERIAL_THREAD);
    CHECK(!pthread_join(thread,NULL) && !pthread_barrier_destroy(&barrier));
}
int main(void) {
    space=open("/proc/self/ns/mnt",O_RDONLY|O_CLOEXEC);socket_fd=open("/dev/null",O_RDONLY|O_CLOEXEC);CHECK(space>=0 && socket_fd>=0);
    /* Keep single-threaded constructor cases independent of earlier thread
       lifetimes by running them all before the first pthread. */
    wrong_namespace();failures();lifecycle();threaded_constructor();close(socket_fd);close(space);
    puts("Startup coordinator model: namespace binding, policy/notice distinction, one publication, permanent fences, thread/fork and cleanup passed");return 0;
}
