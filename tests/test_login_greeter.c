#define _GNU_SOURCE
#include "login/greeter.h"
#include "login/handoff.h"
#include "login/connection.h"
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
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/lsan_interface.h>
#endif

/* Explicit disposable-carrier fixture only. Its independently Root-created
   profile is test infrastructure, not a production deployment/profile parser. */
#define CHECK(x) do {if(!(x)){fprintf(stderr,"greeter fixture line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#ifdef AE_LOGIN_TEST_SPLIT_ROLES
#define BACKEND_LABEL "system_u:system_r:aetheri_login_backend_t:s0"
#define WORKER_LABEL "system_u:system_r:aetheri_login_worker_t:s0"
#define GREETER_LABEL "system_u:system_r:aetheri_greeter_t:s0"
#define BACKEND_FILE_LABEL "system_u:object_r:aetheri_login_backend_exec_t:s0"
#define CONFIG_FILE_LABEL "system_u:object_r:aetheri_login_config_t:s0"
#define GREETER_FILE_LABEL "system_u:object_r:aetheri_greeter_exec_t:s0"
#else
#define BACKEND_LABEL "system_u:system_r:unconfined_service_t:s0"
#define WORKER_LABEL BACKEND_LABEL
#define GREETER_LABEL BACKEND_LABEL
#define BACKEND_FILE_LABEL "system_u:object_r:bin_t:s0"
#define CONFIG_FILE_LABEL "system_u:object_r:etc_t:s0"
#define GREETER_FILE_LABEL BACKEND_FILE_LABEL
#endif
#ifndef AE_LOGIN_TEST_GREETER_UID
#define AE_LOGIN_TEST_GREETER_UID 1000
#define AE_LOGIN_TEST_GREETER_GID 1000
#endif
#ifdef AE_LOGIN_TEST_DEDICATED_GREETER
static const gid_t selected_groups[]={AE_LOGIN_TEST_GREETER_GID};
#else
/* Existing carrier account, independently selected fixture membership only. */
static const gid_t selected_groups[]={10,1000};
#endif
#ifdef AE_LOGIN_TEST_CONFINED_STARTUP
#define AE_LOGIN_TEST_HANDOFF_LABEL "system_u:object_r:aetheri_login_handoff_t:s0"
#else
#define AE_LOGIN_TEST_HANDOFF_LABEL "system_u:object_r:var_run_t:s0"
#endif
static char test_secret[65];
static void clear_test_secret(void) {volatile char *p=test_secret;for(size_t i=0;i<sizeof test_secret;++i)p[i]=0;}
/* Explicit fixture input only: systemd OpenFile passes one Root-owned sealed
   anonymous read-only file as fd3. Never read a password from argv/environment,
   print it, or use this test provider as a production credential-input API. */
static void check_test_secret_fd(void) {
    struct stat st;int flags=fcntl(3,F_GETFL),seals=fcntl(3,F_GET_SEALS);
    CHECK(!fstat(3,&st) && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_gid==0 &&
        st.st_nlink==0 && (st.st_mode&07777)==0600 && st.st_size==64 && flags>=0 &&
        (flags&O_ACCMODE)==O_RDONLY && seals==(F_SEAL_WRITE|F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_SEAL));
}
static bool load_test_secret(bool once) {
    CHECK(!atexit(clear_test_secret));
    check_test_secret_fd();
    ssize_t size=once?read(3,test_secret,64):pread(3,test_secret,64,0);
    if(once && size==0){CHECK(!close(3));return false;}
    CHECK(size==64);
    for(size_t i=0;i<64;++i)CHECK((test_secret[i]>='0' && test_secret[i]<='9') || (test_secret[i]>='a' && test_secret[i]<='f'));
    CHECK(!close(3));
    return true;
}
static void test_log(int fd) {
    struct stat st;int flags=fcntl(fd,F_GETFL);
    CHECK(!fstat(fd,&st) && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_gid==0 &&
        st.st_nlink==1 && (st.st_mode&07777)==0600 && flags>=0 && (flags&O_APPEND) &&
        (flags&O_ACCMODE)==O_RDWR && dup2(fd,1)==1 && dup2(fd,2)==2 && !close(fd));
}
static void pause_test(void) {struct timespec t={.tv_nsec=100000000};while(nanosleep(&t,&t) && errno==EINTR){}}
static int connect_test(void)
{
    const char *path=getenv("GREETD_SOCK");
    struct sockaddr_un address={.sun_family=AF_UNIX};
    CHECK(path && *path=='/' && strlen(path)<sizeof address.sun_path);
    strcpy(address.sun_path,path);
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
    CHECK(fd>=0 && !connect(fd,(const struct sockaddr *)&address,sizeof address));
    return fd;
}
static struct ae_login_service *service_test(int fd,sd_bus *bus,const char *unit)
{
    struct ae_login_service_expect expected={.unit=unit,.uid=0,.gid=0,.label=BACKEND_LABEL,
        .socket_label=BACKEND_LABEL};
    struct ae_login_service *service=NULL;
    int r=ae_login_service_create(fd,bus,&expected,&service);
    if(r) {
        char label[512]={0};socklen_t size=sizeof label;
        int observed=getsockopt(fd,SOL_SOCKET,SO_PEERSEC,label,&size);
        fprintf(stderr,"service: %d; peer socket label: %.*s (read %d)\n",r,(int)(size<sizeof label?size:sizeof label),label,observed);
    }
    CHECK(!r && service);return service;
}
static void *other_thread(void *data)
{
    CHECK(ae_login_greeter_check(data)==-EXDEV);return NULL;
}
struct admission {struct ae_login_greeter *greeter;struct ae_login_handoff *handoff;};
static int admission_check(void *data)
{
    struct admission *a=data;
    int r=ae_login_greeter_check(a->greeter);
    return r?r:ae_login_handoff_check(a->handoff);
}
static void finish_reply(struct ae_login_connection *connection,enum ae_login_phase expected)
{
    for(size_t i=0;i<100;++i) {
        int r=ae_login_connection_pump(connection);
        if(r<0 && expected==AE_LOGIN_UNKNOWN) {
            struct ae_login_status status;
            CHECK(r==-EPROTO && !ae_login_connection_status(connection,&status) && status.phase==AE_LOGIN_UNKNOWN);
            CHECK(ae_login_connection_begin(connection,"aetheri",7)==r);
            return;
        }
        CHECK(r>=0);
        if(r==1) {
            struct ae_login_status status;
            CHECK(!ae_login_connection_status(connection,&status) && status.phase==expected);
            return;
        }
        int fd,pin;short events;
        CHECK(!ae_login_connection_watch(connection,&fd,&events,&pin));
        struct pollfd items[]={{.fd=fd,.events=events},{.fd=pin,.events=POLLIN}};
        CHECK(poll(items,2,100)>=0);
    }
    CHECK(0);
}
int main(int argc,char **argv)
{
    if((argc!=2 && argc!=3) || getuid()!=AE_LOGIN_TEST_GREETER_UID || geteuid()!=AE_LOGIN_TEST_GREETER_UID)return 77;
    bool retry=argc==3 && !strcmp(argv[2],"retry");
    bool start=argc==3 && !strcmp(argv[2],"start");
    bool authenticate=argc==3 && (!strcmp(argv[2],"authenticate") || start);
    bool outsider=argc==3 && !strcmp(argv[2],"outsider");
    bool groups=argc==3 && !strcmp(argv[2],"groups");
    bool idle=false;
    bool wrong_sources=false;
    bool split_auth=false;
    bool split_start=false;
    sigset_t termination;
#ifdef AE_LOGIN_TEST_SPLIT_ROLES
    wrong_sources=argc==3 && !strcmp(argv[2],"handoff-mismatch");
    split_start=argc==3 && !strcmp(argv[2],"handoff-start");
    split_auth=split_start || (argc==3 && !strcmp(argv[2],"handoff-authenticate"));
    idle=wrong_sources || split_auth || (argc==3 && !strcmp(argv[2],"handoff"));
    if(!idle)return 77;
#endif
    if(argc==3 && !retry && !outsider && !authenticate && !groups && !idle)return 77;
    if(start || split_auth)test_log(4);
    if(groups || (idle && !split_auth))test_log(3);
    if(idle) {
        CHECK(!sigemptyset(&termination) && !sigaddset(&termination,SIGTERM));
        CHECK(!sigprocmask(SIG_BLOCK,&termination,NULL));
    }
    if(authenticate && !load_test_secret(start)) {
#ifdef AE_LOGIN_TEST_IGNORE_TERM
        /* Explicit pending-shutdown fixture only; never a production greeter. */
        struct sigaction action={.sa_handler=SIG_IGN};
        CHECK(!sigemptyset(&action.sa_mask) && !sigaction(SIGTERM,&action,NULL));
#endif
        puts("GREETER_NO_INPUT");fflush(stdout);
        for(size_t i=0;i<300;++i)pause_test();
        return 0;
    }
    setvbuf(stdout,NULL,_IOLBF,0);
    int profile=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    struct stat st;
    CHECK(profile>=0 && !fstat(profile,&st) && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_gid==0 &&
        (st.st_mode&07777)==0444 && st.st_nlink==1 && st.st_size>0 && st.st_size<4096);
    char text[4096];ssize_t n=read(profile,text,sizeof(text)-1);
    CHECK(n==st.st_size);text[n]=0;close(profile);
    char *lines[10],*save=NULL;
    for(size_t i=0;i<10;++i){lines[i]=strtok_r(i?NULL:text,"\n",&save);CHECK(lines[i]);}
    CHECK(!strtok_r(NULL,"\n",&save));
    const char *unit=lines[0],*directory=lines[1];
    struct ae_login_source_expect entries[]={
        {.directory="/run/systemd/system",.leaf=unit,.label="system_u:object_r:systemd_unit_file_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0444},
        {.directory=directory,.leaf="backend",.label=BACKEND_FILE_LABEL,.kind=AE_LOGIN_SOURCE_ELF,.mode=0555},
        {.directory=directory,.leaf="config",.label=CONFIG_FILE_LABEL,.kind=AE_LOGIN_SOURCE_DATA,.mode=0444},
        {.directory="/usr/lib/systemd/system/service.d",.leaf="10-timeout-abort.conf",.label="system_u:object_r:systemd_unit_file_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0644},
        {.directory=directory,.leaf="greeter",.label=GREETER_FILE_LABEL,.kind=AE_LOGIN_SOURCE_ELF,.mode=0555}
    };
    const char *sizes[]={lines[2],lines[4],lines[6],"596",lines[8]};
    const char *hashes[]={lines[3],lines[5],lines[7],"ae6b234f92bc22f1201a7572b59b454c9809f33c80d13f361b9674e1801acc37",lines[9]};
    for(size_t i=0;i<5;++i) {
        char tail;unsigned long long size;
        CHECK(sscanf(sizes[i],"%llu%c",&size,&tail)==1 && strlen(hashes[i])==64);
        entries[i].size=(uint64_t)size;
        for(size_t j=0;j<32;++j){unsigned int byte;CHECK(sscanf(hashes[i]+2*j,"%2x",&byte)==1);entries[i].digest[j]=(uint8_t)byte;}
    }
    struct ae_login_sources *sources=NULL;
    int source_result=ae_login_sources_create(entries,5,&sources);
    if(source_result)fprintf(stderr,"greeter sources: %d\n",source_result);
    CHECK(!source_result);
    sd_bus *bus=NULL;
    CHECK(sd_bus_open_system(&bus)>=0 && sd_bus_set_method_call_timeout(bus,2000000)>=0);
    struct ae_login_greeter_expect expected={.uid=AE_LOGIN_TEST_GREETER_UID,.gid=AE_LOGIN_TEST_GREETER_GID,
        .groups=selected_groups,.group_count=sizeof(selected_groups)/sizeof(selected_groups[0]),
        .label=GREETER_LABEL,.worker_label=WORKER_LABEL,.program=4};
    int fd=connect_test();
    struct ae_login_service *service=service_test(fd,bus,unit);
    struct ae_login_greeter *greeter=NULL;
    int original_peer=-1;
    if(groups) {
        /* Private NSS fixture only. The controller independently checks the
           actual original worker/child before permitting this test to run.
           No credential conversation, handoff publication or Start occurs. */
        char parent[AE_LOGIN_SOURCE_FULL_PATH_MAX];
        int length=snprintf(parent,sizeof parent,"%s/runtime",directory);
        CHECK(length>0 && (size_t)length<sizeof parent &&
            !ae_login_service_peer(service,&original_peer));
        int dir=open(parent,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
        CHECK(dir>=0);puts("GREETER_GROUP_WAIT");
        bool approved=false;
        for(size_t i=0;i<150;++i) {
            if(!fstatat(dir,"groups-approved",&st,AT_SYMLINK_NOFOLLOW)) {
                CHECK(S_ISREG(st.st_mode) && st.st_uid==0 && st.st_gid==0 &&
                    (st.st_mode&07777)==0444 && st.st_nlink==1);
                approved=true;break;
            }
            CHECK(errno==ENOENT && !ae_login_sources_check(sources) &&
                !ae_login_service_check(service));pause_test();
        }
        CHECK(approved && !close(dir));
        gid_t actual[2];CHECK(getgroups(2,actual)==2);
        if(actual[0]>actual[1]){gid_t swap=actual[0];actual[0]=actual[1];actual[1]=swap;}
        CHECK(actual[0]==992 && actual[1]==60000);
    }
    int r=ae_login_greeter_create(service,sources,&expected,&greeter);
    if(groups) {
        CHECK(r==-EACCES && !greeter && ae_login_service_check(service)==r);
        /* Only this independently selected fixture expectation changes.
           Keep the failed observer fenced. A fresh guard must still select
           the same live kernel peer; never rearm/adopt a failed guard. */
        int next_fd=connect_test(),next_peer;
        struct ae_login_service *next=service_test(next_fd,bus,unit);
        struct stat original_object,next_object;
        struct pollfd original_live={.fd=original_peer,.events=POLLIN};
        CHECK(!ae_login_service_peer(next,&next_peer) &&
            !fstat(original_peer,&original_object) && !fstat(next_peer,&next_object) &&
            original_object.st_dev==next_object.st_dev &&
            original_object.st_ino==next_object.st_ino && !poll(&original_live,1,0));
        CHECK(!ae_login_service_close(&service) && !close(fd));service=next;fd=next_fd;
        const gid_t control[]={992,60000};
        expected.groups=control;expected.group_count=2;
        CHECK(!ae_login_greeter_create(service,sources,&expected,&greeter) &&
            greeter && !ae_login_greeter_check(greeter));
        CHECK(!ae_login_greeter_close(&greeter) && !ae_login_service_close(&service) &&
            !ae_login_sources_close(&sources) && !close(fd));sd_bus_unref(bus);
#if defined(__SANITIZE_ADDRESS__)
        CHECK(!__lsan_do_recoverable_leak_check());puts("GREETER_GROUP_LEAK_CHECKED");
#endif
        puts("GREETER_GROUP_EXTRA_REJECTED same_context_control=1");return 0;
    }
    if(outsider) {
        CHECK(r<0 && !greeter);
        puts("GREETER_OUTSIDER_REJECTED");
        close(fd);CHECK(!ae_login_service_close(&service) && !ae_login_sources_close(&sources));sd_bus_unref(bus);return 0;
    }
    if(r)fprintf(stderr,"greeter: %d\n",r);
    CHECK(!r && greeter && !ae_login_greeter_check(greeter));
    if(!idle) {
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,other_thread,greeter) && !pthread_join(thread,NULL));
    pid_t child=fork();CHECK(child>=0);
    if(!child) {
        CHECK(ae_login_greeter_check(greeter)==-ECHILD && !ae_login_greeter_close(&greeter));
        struct ae_login_sources *fresh_sources=NULL;
        CHECK(!ae_login_sources_create(entries,5,&fresh_sources));
        sd_bus *fresh_bus=NULL;
        CHECK(sd_bus_open_system(&fresh_bus)>=0 && sd_bus_set_method_call_timeout(fresh_bus,2000000)>=0);
        int fresh_fd=connect_test();
        struct ae_login_service *fresh_service=service_test(fresh_fd,fresh_bus,unit);
        struct ae_login_greeter *wrong=NULL;
        CHECK(ae_login_greeter_create(fresh_service,fresh_sources,&expected,&wrong)==-ESTALE && !wrong);
        CHECK(!ae_login_service_close(&fresh_service) && !ae_login_sources_close(&fresh_sources));
        sd_bus_unref(fresh_bus);close(fresh_fd);
        _exit(0);
    }
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    for(size_t i=0;i<5;++i) {
        int negative_fd=connect_test();struct ae_login_service *negative_service=service_test(negative_fd,bus,unit);
        struct ae_login_greeter_expect bad=expected;
        if(i==0)bad.uid=1001;
        if(i==1)bad.label="system_u:system_r:unconfined_t:s0";
        if(i==2)bad.worker_label="system_u:system_r:unconfined_t:s0";
        if(i==3)bad.program=1;
        if(i==4){bad.groups=NULL;bad.group_count=0;}
        struct ae_login_greeter *wrong=NULL;
        CHECK(ae_login_greeter_create(negative_service,sources,&bad,&wrong)<0 && !wrong);
        CHECK(!ae_login_service_close(&negative_service));close(negative_fd);
    }
    }
    struct ae_login_loaded *loaded=NULL;const size_t dropin[]={3};
    r=ae_login_loaded_create(service,sources,0,1,2,dropin,1,&loaded);
    if(r)fprintf(stderr,"greeter loaded unit: %d\n",r);
    CHECK(!r);
    char parent[AE_LOGIN_SOURCE_FULL_PATH_MAX],program[AE_LOGIN_SOURCE_FULL_PATH_MAX];
    CHECK(snprintf(parent,sizeof parent,"%s/runtime",directory)>0 && snprintf(program,sizeof program,"%s/greeter",directory)>0);
    int directory_fd=open(parent,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    CHECK(directory_fd>=0);
    puts("GREETER_READY");
    bool found=false;
    for(size_t i=0;i<150;++i) {
        if(!fstatat(directory_fd,"backend-observation",&st,AT_SYMLINK_NOFOLLOW)){found=true;break;}
        CHECK(errno==ENOENT && !ae_login_greeter_check(greeter));pause_test();
    }
    CHECK(found);
    struct ae_login_handoff *handoff=NULL;
    r=ae_login_handoff_open(loaded,directory_fd,parent,"backend-observation",AE_LOGIN_TEST_HANDOFF_LABEL,&handoff);
    if(r)fprintf(stderr,"greeter handoff: %d\n",r);
    if(wrong_sources) {
        CHECK(r==-ESTALE && !handoff);
        puts("GREETER_HANDOFF_REJECTED");
        struct timespec deadline={.tv_sec=30};
        CHECK(sigtimedwait(&termination,NULL,&deadline)==SIGTERM);
        CHECK(!close(fd));
        goto cleanup;
    }
    CHECK(!r);
    struct admission admission={greeter,handoff};
    int pin;CHECK(!ae_login_service_peer(service,&pin));
    struct ae_login_connection_source source={.peer_pidfd=pin,.uid=0,.gid=0,.check=admission_check,.userdata=&admission};
    struct ae_login_connection *connection=NULL;
    r=ae_login_connection_create(&connection,&fd,&source);
    if(r)fprintf(stderr,"greeter connection: %d\n",r);
    CHECK(!r && fd==-1);
    if(idle) {
        struct ae_login_status state;
        CHECK(!ae_login_connection_status(connection,&state) && state.phase==AE_LOGIN_IDLE);
        CHECK(!admission_check(&admission));
        puts("GREETER_HANDOFF_ACCEPTED");
        if(split_auth) {
            if(split_start) {
                check_test_secret_fd();
                off_t offset=lseek(3,0,SEEK_CUR);
                CHECK(offset==0 || offset==64);
                if(offset==64) {
                    CHECK(!close(3) && !ae_login_connection_close(&connection));
                    puts("GREETER_REPLACEMENT_IDLE");
                    struct timespec deadline={.tv_sec=30};
                    CHECK(sigtimedwait(&termination,NULL,&deadline)==SIGTERM);
                    goto cleanup;
                }
            }
            CHECK(!ae_login_connection_begin(connection,"aetheri",7));
            finish_reply(connection,AE_LOGIN_PROMPT);
            CHECK(!ae_login_connection_status(connection,&state) && state.prompt==AE_LOGIN_SECRET);
            CHECK(!ae_login_connection_answer(connection,"",0));
            finish_reply(connection,AE_LOGIN_DENIED);
            CHECK(!ae_login_connection_cancel(connection));
            finish_reply(connection,AE_LOGIN_IDLE);
            puts("GREETER_EMPTY_ANSWER_DENIED");

            CHECK(!ae_login_connection_begin(connection,"aetheri",7));
            finish_reply(connection,AE_LOGIN_PROMPT);
            CHECK(!ae_login_connection_status(connection,&state) && state.prompt==AE_LOGIN_SECRET);
            CHECK(load_test_secret(split_start));
            int answer=ae_login_connection_answer(connection,test_secret,64);
            clear_test_secret();
            CHECK(!answer);
            finish_reply(connection,AE_LOGIN_AUTHENTICATED);
            if(split_start) {
                puts("GREETER_START_WAIT");
                found=false;
                for(size_t i=0;i<150;++i) {
                    if(!fstatat(directory_fd,"start-approved",&st,AT_SYMLINK_NOFOLLOW)) {
                        CHECK(S_ISREG(st.st_mode) && st.st_uid==0 && st.st_gid==0 &&
                              (st.st_mode&07777)==0444 && st.st_nlink==1);
                        found=true;
                        break;
                    }
                    CHECK(errno==ENOENT && !admission_check(&admission));
                    pause_test();
                }
                CHECK(found && !ae_login_connection_start(connection));
                finish_reply(connection,AE_LOGIN_START_ACKNOWLEDGED);
                puts("GREETER_START_ACK");
                CHECK(!ae_login_connection_close(&connection));
                goto cleanup;
            }
            CHECK(!ae_login_connection_cancel(connection));
            finish_reply(connection,AE_LOGIN_IDLE);
            puts("GREETER_AUTH_CANCEL_ACK");
        }
        struct timespec deadline={.tv_sec=30};
        CHECK(sigtimedwait(&termination,NULL,&deadline)==SIGTERM);
        CHECK(!ae_login_connection_close(&connection));
        goto cleanup;
    }
    CHECK(!ae_login_connection_begin(connection,"aetheri",7));finish_reply(connection,AE_LOGIN_PROMPT);
    struct ae_login_status prompt;
    CHECK(!ae_login_connection_status(connection,&prompt) && prompt.prompt==AE_LOGIN_SECRET);
    if(retry || authenticate) {
        puts("GREETER_PROMPT_WAIT");found=false;
        for(size_t i=0;i<150;++i) {
            if(!fstatat(directory_fd,"prompt-inspected",&st,AT_SYMLINK_NOFOLLOW)){found=true;break;}
            CHECK(errno==ENOENT && !admission_check(&admission));pause_test();
        }
        CHECK(found);
    }
    CHECK(!ae_login_connection_cancel(connection));finish_reply(connection,AE_LOGIN_IDLE);
    puts("GREETER_CANCEL_ACK");
    CHECK(!ae_login_connection_begin(connection,"aetheri",7));finish_reply(connection,AE_LOGIN_PROMPT);
    CHECK(!ae_login_connection_status(connection,&prompt) && prompt.prompt==AE_LOGIN_SECRET);
    CHECK(!ae_login_connection_answer(connection,"",0));finish_reply(connection,AE_LOGIN_DENIED);
    puts("GREETER_PAM_DENIED");
    /* The pinned backend can fail sending Cancel to a worker that already
       exited after denial. Preserve the model's UNKNOWN fence on that error;
       do not turn a failed cancellation into a retry or an implied drain. */
    CHECK(!ae_login_connection_cancel(connection));
    if(retry || authenticate) {
        finish_reply(connection,AE_LOGIN_IDLE);
        puts("GREETER_RETRY_ACK");
        for(size_t attempt=0;!authenticate && attempt<2;++attempt) {
            CHECK(!ae_login_connection_begin(connection,"aetheri",7));finish_reply(connection,AE_LOGIN_PROMPT);
            CHECK(!ae_login_connection_status(connection,&prompt) && prompt.prompt==AE_LOGIN_SECRET);
            CHECK(!ae_login_connection_cancel(connection));finish_reply(connection,AE_LOGIN_IDLE);
            puts("GREETER_RETRY_PROMPT");
        }
    } else {
        finish_reply(connection,AE_LOGIN_UNKNOWN);
        puts("GREETER_CANCEL_UNKNOWN");
    }
    if(authenticate) {
        CHECK(!ae_login_connection_begin(connection,"aetheri",7));finish_reply(connection,AE_LOGIN_PROMPT);
        CHECK(!ae_login_connection_status(connection,&prompt) && prompt.prompt==AE_LOGIN_SECRET);
        int answered=ae_login_connection_answer(connection,test_secret,64);
        clear_test_secret();CHECK(!answered);finish_reply(connection,AE_LOGIN_AUTHENTICATED);
        puts("GREETER_AUTHENTICATED");
        if(start) {
            puts("GREETER_START_WAIT");found=false;
            for(size_t i=0;i<150;++i) {
                if(!fstatat(directory_fd,"start-approved",&st,AT_SYMLINK_NOFOLLOW)){found=true;break;}
                CHECK(errno==ENOENT && !admission_check(&admission));pause_test();
            }
            CHECK(found && !ae_login_connection_start(connection));finish_reply(connection,AE_LOGIN_START_ACKNOWLEDGED);
            puts("GREETER_START_ACK");
        } else {
            CHECK(!ae_login_connection_cancel(connection));finish_reply(connection,AE_LOGIN_IDLE);
            puts("GREETER_AUTH_CANCEL_ACK");
        }
    }
    CHECK(!ae_login_connection_close(&connection));
    if(start)goto cleanup;
    puts("GREETER_WAIT_DRIFT");found=false;
    for(size_t i=0;i<150;++i){CHECK(!stat(program,&st));if((st.st_mode&07777)==0755){found=true;break;}pause_test();}
    CHECK(found);int loss=ae_login_greeter_check(greeter);CHECK(loss<0);
    puts("GREETER_FENCED");found=false;
    for(size_t i=0;i<150;++i){CHECK(!stat(program,&st));if((st.st_mode&07777)==0555){found=true;break;}pause_test();}
    CHECK(found && ae_login_greeter_check(greeter)==loss);
cleanup:
    CHECK(!ae_login_handoff_close(&handoff) && !ae_login_loaded_close(&loaded) && !ae_login_greeter_close(&greeter));
    CHECK(!ae_login_service_close(&service) && !ae_login_sources_close(&sources));sd_bus_unref(bus);close(directory_fd);
#if defined(__SANITIZE_ADDRESS__)
    CHECK(!__lsan_do_recoverable_leak_check());
    puts("GREETER_LEAK_CHECKED");
#endif
    puts("GREETER_DONE");
    if(start || idle)return 0;
    /* Keep the original test greeter alive until the controller stops exactly
       its original service; do not provoke an automatic greeter replacement. */
    for(size_t i=0;i<300;++i)pause_test();
    return 0;
}
