#define _GNU_SOURCE
#include "login/loaded.h"
#include "login/handoff.h"
#include "login/startup.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"loaded fixture line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

/* Explicit administrative carrier gate only, never deployment policy. */
static int fixture_policy(void *unused){(void)unused;return security_getenforce()==1?0:-EACCES;}

static void source_args(struct ae_login_source_expect *entry,const char *size,const char *hash)
{
    unsigned long long value=0;
    char tail;
    CHECK(sscanf(size,"%llu%c",&value,&tail)==1 && strlen(hash)==64);
    entry->size=(uint64_t)value;
    for(size_t i=0;i<32;++i) {
        unsigned int byte=0;
        CHECK(sscanf(hash+2*i,"%2x",&byte)==1);
        entry->digest[i]=(uint8_t)byte;
    }
}

static int connect_fixture(const struct sockaddr_un *address)
{
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
    CHECK(fd>=0 && !connect(fd,(const struct sockaddr *)address,sizeof *address));
    return fd;
}

static void fixture_notice(const struct sockaddr_un *address,char command)
{
    int fd=connect_fixture(address);
    CHECK(send(fd,&command,1,MSG_NOSIGNAL)==1);
    struct pollfd item={.fd=fd,.events=POLLIN};
    CHECK(poll(&item,1,5000)==1);
    char answer=0;
    CHECK(recv(fd,&answer,1,0)==1 && answer=='y');
    close(fd);
}

int main(int argc,char **argv)
{
    if(argc!=13 && argc!=15)return 77;
    const char *socket_path=argv[1],*unit=argv[2],*directory=argv[3];
    const char *config_leaf=argv[4],*mode=argv[5];
    bool publish=!strcmp(mode,"publish");
    bool startup=!strcmp(mode,"startup");
    bool image_notice=!strcmp(mode,"image-notice");
    bool image_check=startup || publish || image_notice || !strcmp(mode,"image") || !strcmp(mode,"reject-image");
    if((getuid()==0 || geteuid()==0)!=image_check)return 77;
    struct ae_login_source_expect entries[]={
        {.directory=argv[6],.leaf=unit,.label="system_u:object_r:systemd_unit_file_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0444},
        {.directory=directory,.leaf="backend",.label="system_u:object_r:bin_t:s0",.kind=AE_LOGIN_SOURCE_ELF,.mode=0555},
        {.directory=directory,.leaf=config_leaf,.label="system_u:object_r:etc_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0444},
        {.directory="/usr/lib/systemd/system/service.d",.leaf="10-timeout-abort.conf",
         .label="system_u:object_r:systemd_unit_file_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0644},
        {.directory=directory,.leaf="greeter",.label="system_u:object_r:bin_t:s0",.kind=AE_LOGIN_SOURCE_ELF,.mode=0555}
    };
    for(size_t i=0;i<3;++i)source_args(&entries[i],argv[7+2*i],argv[8+2*i]);
    source_args(&entries[3],"596","ae6b234f92bc22f1201a7572b59b454c9809f33c80d13f361b9674e1801acc37");
    if(argc==15)source_args(&entries[4],argv[13],argv[14]);
    if(startup) {
        CHECK(argc==15);
        struct sockaddr_un selected={.sun_family=AF_UNIX};CHECK(strlen(socket_path)<sizeof selected.sun_path);
        strcpy(selected.sun_path,socket_path);int connected=connect_fixture(&selected);
        sd_bus *system=NULL;CHECK(sd_bus_open_system(&system)>=0 && sd_bus_set_method_call_timeout(system,2000000)>=0);
        char parent[AE_LOGIN_SOURCE_FULL_PATH_MAX];int n=snprintf(parent,sizeof parent,"%s/runtime",directory);
        CHECK(n>0 && (size_t)n<sizeof parent);const size_t drop[]={3};
        struct ae_login_startup_expect expected={
            .service={.unit=unit,.uid=0,.gid=0,.label="system_u:system_r:unconfined_service_t:s0",
                .socket_label="system_u:system_r:unconfined_service_t:s0-s0:c0.c1023"},
            .sources=entries,.source_count=5,.fragment=0,.program=1,.config=2,.dropins=drop,.dropin_count=1,
            .directory=parent,.leaf="backend-observation",.label="system_u:object_r:var_run_t:s0",.check=fixture_policy};
        struct ae_login_startup *coordinator=NULL;
        int result=ae_login_startup_create(connected,system,&expected,&coordinator);
        if(result)fprintf(stderr,"startup create: %d\n",result);
        CHECK(!result && coordinator && !ae_login_startup_check(coordinator));
        CHECK(!ae_login_startup_publish(coordinator) && ae_login_startup_publish(coordinator)==-EALREADY);
        CHECK(!ae_login_startup_close(&coordinator) && !coordinator);close(connected);sd_bus_unref(system);
        puts("Native startup published in the original peer mount namespace without replay");return 0;
    }
    struct ae_login_sources *sources=NULL;
    int r=ae_login_sources_create(entries,argc==15?5:4,&sources);
    if(r)fprintf(stderr,"sources: %d\n",r);
    CHECK(!r && sources);
    struct sockaddr_un address={.sun_family=AF_UNIX};
    CHECK(strlen(socket_path)<sizeof address.sun_path);
    strcpy(address.sun_path,socket_path);
    int fd=connect_fixture(&address);
    /* The inert fixture read loop is sequential. Keep the peer pin for the
       guard but release this connection so the next test control can run. */
    sd_bus *bus=NULL;
    CHECK(sd_bus_open_system(&bus)>=0 && sd_bus_set_method_call_timeout(bus,2000000)>=0);
    struct ae_login_service_expect expected={.unit=unit,.uid=0,.gid=0,
        .label="system_u:system_r:unconfined_service_t:s0",
        .socket_label="system_u:system_r:unconfined_service_t:s0-s0:c0.c1023"};
    struct ae_login_service *service=NULL;
    r=ae_login_service_create(fd,bus,&expected,&service);
    if(r)fprintf(stderr,"service: %d\n",r);
    CHECK(!r && service);
    struct ae_login_loaded *loaded=NULL;
    const size_t dropin[]={3};
    CHECK(ae_login_loaded_create(service,sources,0,1,0,dropin,1,&loaded)==-EINVAL && !loaded);
    bool omit_dropin=!strcmp(mode,"reject-dropins");
    r=ae_login_loaded_create(service,sources,0,1,2,dropin,omit_dropin?0:1,&loaded);
    if(publish || !strncmp(mode,"handoff",7)) {
        CHECK(!r && loaded);
        char parent[AE_LOGIN_SOURCE_FULL_PATH_MAX];
        int length=snprintf(parent,sizeof parent,"%s/runtime",directory);
        CHECK(length>0 && (size_t)length<sizeof parent);
        int directory_fd=open(parent,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
        CHECK(directory_fd>=0);
        const char *leaf="backend-observation",*label="system_u:object_r:var_run_t:s0";
        if(publish) {
            CHECK(!ae_login_handoff_publish(loaded,directory_fd,parent,leaf,label));
            CHECK(ae_login_handoff_publish(loaded,directory_fd,parent,leaf,label)==-EEXIST);
            puts("Protected backend observation published without replacement");
        } else {
            struct ae_login_handoff *handoff=NULL;
            CHECK(ae_login_handoff_publish(loaded,directory_fd,parent,leaf,label)==-EACCES);
            CHECK(ae_login_handoff_open(loaded,directory_fd,parent,leaf,
                  "system_u:object_r:etc_t:s0",&handoff)==-EACCES && !handoff);
            int opened=ae_login_handoff_open(loaded,directory_fd,parent,leaf,label,&handoff);
            if(!strcmp(mode,"handoff-reject")) {
                CHECK(opened==-ESTALE && !handoff);
                puts("Different protected observation rejected");
            } else {
                if(opened)fprintf(stderr,"handoff: %d\n",opened);
                CHECK(!opened && handoff && !ae_login_handoff_check(handoff));
                CHECK(openat(directory_fd,leaf,O_WRONLY|O_CLOEXEC|O_NOFOLLOW)<0 && errno==EACCES);
                CHECK(unlinkat(directory_fd,leaf,0)<0 && errno==EACCES);
                if(!strcmp(mode,"handoff-hold")) {
                    puts("HANDOFF_READY");fflush(stdout);
                    CHECK(getchar()=='x');
                    int lost=ae_login_handoff_check(handoff);
                    CHECK(lost<0);
                    puts("HANDOFF_FENCED");fflush(stdout);
                    CHECK(getchar()=='y' && ae_login_handoff_check(handoff)==lost);
                } else CHECK(!strcmp(mode,"handoff"));
                CHECK(!ae_login_handoff_close(&handoff) && !handoff);
                puts("Protected backend observation consumed");
            }
        }
        close(directory_fd);
    } else if(!strcmp(mode,"notice-real")) {
        CHECK(!r && loaded);
        CHECK(ae_login_loaded_check_config(loaded)==0 && ae_login_loaded_check(loaded)==0);
        puts("Running backend configuration notice matched");
    } else if(!strncmp(mode,"notice-",7)) {
        CHECK(!r && loaded);
        close(fd);fd=-1;
        if(!strcmp(mode,"notice-policy")) {
            CHECK(ae_login_loaded_check_config(loaded)==-ESTALE);
        } else {
            CHECK(ae_login_loaded_check_config(loaded)==-EAGAIN);
            CHECK(ae_login_loaded_check(loaded)==-EAGAIN);
            if(!strcmp(mode,"notice-digest"))fixture_notice(&address,'d');
            else if(!strcmp(mode,"notice-invocation"))fixture_notice(&address,'i');
            else {
                CHECK(!strcmp(mode,"notice-loss"));
                CHECK(ae_login_loaded_check_config(loaded)==-EAGAIN);
                fixture_notice(&address,'n');
                CHECK(ae_login_loaded_check_config(loaded)==0);
                CHECK(ae_login_loaded_check(loaded)==0);
                fixture_notice(&address,'e');
                CHECK(ae_login_loaded_check(loaded)==-ESTALE);
            }
            CHECK(ae_login_loaded_check_config(loaded)==-ESTALE);
        }
        fixture_notice(&address,'n');
        CHECK(ae_login_loaded_check_config(loaded)==-ESTALE);
        CHECK(ae_login_loaded_check(loaded)==-ESTALE);
        puts("Consumed configuration checks passed");
    } else if(!strcmp(mode,"reject") || omit_dropin) {
        CHECK(r==-ESTALE && !loaded);
        puts("Loaded command rejection passed");
    } else if(image_check) {
        if(r)fprintf(stderr,"loaded: %d\n",r);
        CHECK(!r && loaded);
        struct ae_image image,marker,expected_image;
        memset(&marker,0xa5,sizeof marker);
        image=marker;
        CHECK(!ae_login_sources_image(sources,1,&expected_image));
        if(image_notice)CHECK(ae_login_loaded_check_config(loaded)==0);
        int result=ae_login_loaded_match_image(loaded,&image);
        if(image_notice || !strcmp(mode,"image")) {
            if(result)fprintf(stderr,"image: %d\n",result);
            CHECK(!result && ae_image_equal(&image,&expected_image));
            puts("Original running Root executable matched");
        } else {
            CHECK(result==-ESTALE && !memcmp(&image,&marker,sizeof image));
            CHECK(ae_login_loaded_match_image(loaded,&image)==result && !memcmp(&image,&marker,sizeof image));
            puts("Different running ELF object rejected and fenced");
        }
    } else {
        if(r)fprintf(stderr,"loaded: %d\n",r);
        CHECK(!strcmp(mode,"accept") && !r && loaded && !ae_login_loaded_check(loaded));
        struct ae_image denied,marker;
        memset(&marker,0xa5,sizeof marker);denied=marker;
        CHECK(ae_login_loaded_match_image(loaded,&denied)==-EACCES && !memcmp(&denied,&marker,sizeof denied));
        CHECK(!ae_login_loaded_check(loaded));
        puts("Loaded command and protected sources matched");
        struct ucred credentials;
        socklen_t size=sizeof credentials;
        CHECK(!getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&credentials,&size) && size==sizeof credentials);
        char path[64];
        int length=snprintf(path,sizeof path,"/proc/%ld/exe",(long)credentials.pid);
        CHECK(length>0 && (size_t)length<sizeof path);
        int executable=open(path,O_RDONLY|O_CLOEXEC);
        if(executable<0)printf("Ordinary client executable open errno=%d\n",errno);
        else {puts("Ordinary client executable open succeeded");close(executable);}
    }
    CHECK(!ae_login_loaded_close(&loaded) && !loaded);
    CHECK(!ae_login_service_close(&service) && !service);
    CHECK(!ae_login_sources_close(&sources) && !sources);
    if(fd>=0)close(fd);
    sd_bus_unref(bus);
    return 0;
}
