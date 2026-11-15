#define _GNU_SOURCE
#include "login/loaded.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"loaded fixture line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

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

int main(int argc,char **argv)
{
    if(argc!=13 || !getuid() || !geteuid())return 77;
    const char *socket_path=argv[1],*unit=argv[2],*directory=argv[3];
    const char *config_leaf=argv[4],*mode=argv[5];
    struct ae_login_source_expect entries[]={
        {.directory=argv[6],.leaf=unit,.label="system_u:object_r:systemd_unit_file_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0444},
        {.directory=directory,.leaf="backend",.label="system_u:object_r:bin_t:s0",.kind=AE_LOGIN_SOURCE_ELF,.mode=0555},
        {.directory=directory,.leaf=config_leaf,.label="system_u:object_r:etc_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0444},
        {.directory="/usr/lib/systemd/system/service.d",.leaf="10-timeout-abort.conf",
         .label="system_u:object_r:systemd_unit_file_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0644}
    };
    for(size_t i=0;i<3;++i)source_args(&entries[i],argv[7+2*i],argv[8+2*i]);
    source_args(&entries[3],"596","ae6b234f92bc22f1201a7572b59b454c9809f33c80d13f361b9674e1801acc37");
    struct ae_login_sources *sources=NULL;
    int r=ae_login_sources_create(entries,4,&sources);
    if(r)fprintf(stderr,"sources: %d\n",r);
    CHECK(!r && sources);
    struct sockaddr_un address={.sun_family=AF_UNIX};
    CHECK(strlen(socket_path)<sizeof address.sun_path);
    strcpy(address.sun_path,socket_path);
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
    CHECK(fd>=0 && !connect(fd,(struct sockaddr *)&address,sizeof address));
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
    if(!strcmp(mode,"reject") || omit_dropin) {
        CHECK(r==-ESTALE && !loaded);
        puts("Loaded command rejection passed");
    } else {
        if(r)fprintf(stderr,"loaded: %d\n",r);
        CHECK(!strcmp(mode,"accept") && !r && loaded && !ae_login_loaded_check(loaded));
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
    close(fd);
    sd_bus_unref(bus);
    return 0;
}
