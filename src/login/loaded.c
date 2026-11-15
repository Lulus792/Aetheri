#define _GNU_SOURCE
#include "login/loaded.h"
#include "login/service_internal.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNIT "org.freedesktop.systemd1.Unit"
#define SERVICE "org.freedesktop.systemd1.Service"
#define COMMAND "sasasttttuii"
enum { MAX_DROPINS=AE_LOGIN_SOURCES_MAX-3 };

struct command_record {
    uint64_t start_monotonic;
    uint64_t start_realtime;
};

struct ae_login_loaded {
    struct ae_login_service *service;
    struct ae_login_sources *sources;
    char fragment[AE_LOGIN_SOURCE_FULL_PATH_MAX];
    char program[AE_LOGIN_SOURCE_FULL_PATH_MAX];
    char config[AE_LOGIN_SOURCE_FULL_PATH_MAX];
    char dropins[MAX_DROPINS][AE_LOGIN_SOURCE_FULL_PATH_MAX];
    size_t dropin_count;
    size_t program_index;
    size_t config_index;
    struct command_record original;
    pid_t owner,thread;
    int error;
    bool busy;
    bool config_required;
    bool config_seen;
};

struct selected_unit {
    sd_bus *bus;
    const char *manager;
    const char *path;
};

static int owner(const struct ae_login_loaded *loaded)
{
    if(!loaded)return -EINVAL;
    if(getpid()!=loaded->owner)return -ECHILD;
    if(gettid()!=loaded->thread)return -EXDEV;
    return loaded->busy?-EBUSY:0;
}

static bool literal_path(const char *path)
{
    static const char allowed[]="/._-abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    return path[0]=='/' && path[1] && strspn(path,allowed)==strlen(path);
}

static int string_property(const struct selected_unit *unit,const char *interface,
                           const char *key,const char *expected)
{
    char *value=NULL;
    int r=sd_bus_get_property_string(unit->bus,unit->manager,unit->path,interface,key,NULL,&value);
    if(r>=0)r=value && !strcmp(value,expected)?0:-ESTALE;
    free(value);
    return r;
}

static int false_property(const struct selected_unit *unit,const char *key)
{
    int value=1;
    int r=sd_bus_get_property_trivial(unit->bus,unit->manager,unit->path,UNIT,key,NULL,'b',&value);
    return r<0?r:value?-ESTALE:0;
}

static int property(const struct selected_unit *unit,const char *interface,const char *key,
                    const char *signature,sd_bus_message **out)
{
    return sd_bus_get_property(unit->bus,unit->manager,unit->path,interface,key,NULL,out,signature);
}

static bool end_property(sd_bus_message *message)
{
    return sd_bus_message_at_end(message,0)==1 && sd_bus_message_exit_container(message)>0 &&
        sd_bus_message_at_end(message,1)==1;
}

static int empty_property(const struct selected_unit *unit,const char *interface,
                          const char *key,const char *signature,const char *element)
{
    sd_bus_message *message=NULL;
    int r=property(unit,interface,key,signature,&message);
    if(r>=0)r=sd_bus_message_enter_container(message,'a',element)>0 &&
        sd_bus_message_at_end(message,0)==1 && sd_bus_message_exit_container(message)>0 &&
        end_property(message)?0:-ESTALE;
    sd_bus_message_unref(message);
    return r;
}

static bool read_literal(sd_bus_message *message,const char *expected)
{
    const char *value=NULL;
    return sd_bus_message_read(message,"s",&value)>0 && value && !strcmp(value,expected);
}

static int check_dropins(const struct selected_unit *unit,const struct ae_login_loaded *loaded)
{
    sd_bus_message *message=NULL;
    int r=property(unit,UNIT,"DropInPaths","as",&message);
    if(r>=0) {
        r=sd_bus_message_enter_container(message,'a',"s")>0?0:-ESTALE;
        for(size_t i=0;!r && i<loaded->dropin_count;++i)
            if(!read_literal(message,loaded->dropins[i]))r=-ESTALE;
        if(!r && (sd_bus_message_at_end(message,0)!=1 || sd_bus_message_exit_container(message)<=0 ||
                  !end_property(message)))r=-ESTALE;
    }
    sd_bus_message_unref(message);
    return r;
}

static int start_command(const struct selected_unit *unit,const struct ae_login_loaded *loaded,
                         pid_t main_pid,struct command_record *out)
{
    sd_bus_message *message=NULL;
    int r=property(unit,SERVICE,"ExecStartEx","a(" COMMAND ")",&message);
    if(r<0)return r;
    uint64_t real=0,monotonic=0,exit_real=0,exit_monotonic=0;
    uint32_t pid=0;
    int32_t code=0,status=0;
    bool matches=sd_bus_message_enter_container(message,'a',"(" COMMAND ")")>0 &&
        sd_bus_message_enter_container(message,'r',COMMAND)>0 && read_literal(message,loaded->program) &&
        sd_bus_message_enter_container(message,'a',"s")>0 && read_literal(message,loaded->program) &&
        read_literal(message,"--config") && read_literal(message,loaded->config) &&
        sd_bus_message_at_end(message,0)==1 && sd_bus_message_exit_container(message)>0 &&
        sd_bus_message_enter_container(message,'a',"s")>0 && sd_bus_message_at_end(message,0)==1 &&
        sd_bus_message_exit_container(message)>0 &&
        sd_bus_message_read(message,"ttttuii",&real,&monotonic,&exit_real,&exit_monotonic,&pid,&code,&status)>0 &&
        real && monotonic && !exit_real && !exit_monotonic && pid==(uint32_t)main_pid && !code && !status &&
        sd_bus_message_exit_container(message)>0 && sd_bus_message_at_end(message,0)==1 &&
        sd_bus_message_exit_container(message)>0 && end_property(message);
    sd_bus_message_unref(message);
    if(!matches)return -ESTALE;
    *out=(struct command_record){.start_monotonic=monotonic,.start_realtime=real};
    return 0;
}

struct observation {
    struct ae_login_loaded *loaded;
    const struct ae_image *expected_image;
    struct ae_image image;
    const uint8_t *expected_config;
    bool config_matches;
};

static void hex_encode(char *out,const uint8_t *bytes,size_t size)
{
    static const char alphabet[]="0123456789abcdef";
    for(size_t i=0;i<size;++i) {
        out[i*2]=alphabet[bytes[i]>>4];
        out[i*2+1]=alphabet[bytes[i]&0xf];
    }
}

static int config_notice(const struct selected_unit *unit,const uint8_t invocation[16],
                         struct observation *observation)
{
    int r=string_property(unit,SERVICE,"NotifyAccess","main");
    if(r)return r;
    char *status=NULL;
    r=sd_bus_get_property_string(unit->bus,unit->manager,unit->path,SERVICE,"StatusText",NULL,&status);
    if(r<0)return r;
    static const char prefix[]="AETHERI_CONFIG_V1 ";
    char expected[sizeof prefix+32+1+64];
    memcpy(expected,prefix,sizeof prefix-1);
    hex_encode(expected+sizeof prefix-1,invocation,16);
    expected[sizeof prefix-1+32]=' ';
    hex_encode(expected+sizeof prefix+32,observation->expected_config,32);
    expected[sizeof expected-1]=0;
    if(!status)r=-EBADMSG;
    else if(!*status)r=observation->loaded->config_seen?-ESTALE:0;
    else if(strcmp(status,expected))r=-ESTALE;
    else {observation->config_matches=true;r=0;}
    free(status);
    if(!r)r=string_property(unit,SERVICE,"NotifyAccess","main");
    return r;
}

static int inspect_loaded(sd_bus *bus,const char *manager,const char *path,
                          const struct ae_image_process *process,const uint8_t invocation[16],void *data)
{
    struct observation *observation=data;
    struct ae_login_loaded *loaded=observation->loaded;
    const struct selected_unit unit={bus,manager,path};
    int r=string_property(&unit,UNIT,"LoadState","loaded");
    if(!r)r=string_property(&unit,UNIT,"FragmentPath",loaded->fragment);
    if(!r)r=string_property(&unit,UNIT,"SourcePath","");
    if(!r)r=check_dropins(&unit,loaded);
    if(!r)r=false_property(&unit,"Transient");
    if(!r)r=false_property(&unit,"NeedDaemonReload");
    if(!r)r=string_property(&unit,SERVICE,"Type","exec");
    static const char *const extra_commands[]={
        "ExecConditionEx","ExecStartPreEx","ExecStartPostEx","ExecReloadEx",
        "ExecReloadPostEx","ExecStopEx","ExecStopPostEx"
    };
    for(size_t i=0;!r && i<sizeof extra_commands/sizeof extra_commands[0];++i)
        r=empty_property(&unit,SERVICE,extra_commands[i],"a(" COMMAND ")","(" COMMAND ")");
    struct command_record record={0};
    if(!r)r=start_command(&unit,loaded,process->pid,&record);
    if(!r && loaded->original.start_monotonic &&
       (record.start_monotonic!=loaded->original.start_monotonic ||
        record.start_realtime!=loaded->original.start_realtime))r=-ESTALE;
    if(!r && observation->expected_config)r=config_notice(&unit,invocation,observation);
    if(!r && observation->expected_image) {
        if(process->uid || process->gid)r=-EACCES;
        else r=ae_image_match_pinned(process,observation->expected_image,&observation->image);
    }
    if(!r)loaded->original=record;
    return r;
}

static int check(struct ae_login_loaded *loaded,struct ae_image *image,bool require_config)
{
    int r=owner(loaded);
    if(r)return r;
    if(loaded->error)return loaded->error;
    if(require_config)loaded->config_required=true;
    loaded->busy=true;
    struct observation observation={.loaded=loaded};
    struct ae_image expected;
    uint8_t expected_config[32];
    if(image) {
        r=ae_login_sources_image(loaded->sources,loaded->program_index,&expected);
        if(!r)observation.expected_image=&expected;
    } else r=ae_login_sources_check(loaded->sources);
    if(!r && loaded->config_required) {
        r=ae_login_sources_data_digest(loaded->sources,loaded->config_index,expected_config);
        if(!r)observation.expected_config=expected_config;
    }
    if(!r)r=ae_login_service_inspect(loaded->service,inspect_loaded,&observation);
    if(!r)r=ae_login_sources_check(loaded->sources);
    loaded->busy=false;
    if(r)loaded->error=r;
    else {
        if(observation.expected_config && !observation.config_matches)return -EAGAIN;
        if(observation.config_matches)loaded->config_seen=true;
        if(image)*image=observation.image;
    }
    return r;
}

int ae_login_loaded_check(void *value) {return check(value,NULL,false);}
int ae_login_loaded_check_config(void *value) {return check(value,NULL,true);}

int ae_login_loaded_match_image(struct ae_login_loaded *loaded,struct ae_image *out)
{
    if(!out)return -EINVAL;
    if(getuid() || geteuid() || getgid() || getegid())return -EACCES;
    return check(loaded,out,false);
}

int ae_login_loaded_create(struct ae_login_service *service,struct ae_login_sources *sources,
                           size_t fragment,size_t program,size_t config,
                           const size_t *dropins,size_t dropin_count,struct ae_login_loaded **out)
{
    if(!out || *out || !service || !sources || fragment==config || program==fragment || program==config ||
       dropin_count>MAX_DROPINS || (dropin_count && !dropins))
        return -EINVAL;
    for(size_t i=0;i<dropin_count;++i) {
        if(dropins[i]==fragment || dropins[i]==program || dropins[i]==config)return -EINVAL;
        for(size_t j=0;j<i;++j)if(dropins[i]==dropins[j])return -EINVAL;
    }
    struct ae_login_loaded *loaded=calloc(1,sizeof *loaded);
    if(!loaded)return -ENOMEM;
    loaded->owner=getpid();
    loaded->thread=gettid();
    loaded->service=service;
    loaded->sources=sources;
    loaded->dropin_count=dropin_count;
    loaded->program_index=program;
    loaded->config_index=config;
    int r=ae_login_sources_path(sources,fragment,AE_LOGIN_SOURCE_DATA,loaded->fragment);
    if(!r)r=ae_login_sources_path(sources,program,AE_LOGIN_SOURCE_ELF,loaded->program);
    if(!r)r=ae_login_sources_path(sources,config,AE_LOGIN_SOURCE_DATA,loaded->config);
    for(size_t i=0;!r && i<dropin_count;++i)
        r=ae_login_sources_path(sources,dropins[i],AE_LOGIN_SOURCE_DATA,loaded->dropins[i]);
    if(!r && (!literal_path(loaded->program) || !literal_path(loaded->config)))r=-EINVAL;
    if(!r)r=ae_login_loaded_check(loaded);
    if(r){free(loaded);return r;}
    *out=loaded;
    return 0;
}

int ae_login_loaded_close(struct ae_login_loaded **value)
{
    if(!value)return -EINVAL;
    struct ae_login_loaded *loaded=*value;
    if(!loaded)return 0;
    int r=owner(loaded);
    if(r && r!=-ECHILD)return r;
    free(loaded);
    *value=NULL;
    return 0;
}
