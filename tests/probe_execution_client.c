#include "launcher/execution.h"
#include "core/digest.h"
#include "linux/exec_channel.h"
#include "execution-wire.h"
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"execution client probe %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } } while (0)
static struct ae_execution_profile profile;
static uint8_t digest[32];
static char mode[64]="success";
static uint32_t registers,prepared;
static int finished;
static struct ae_exec_channel *channel;
static sd_bus_message *pending;

static int authorize(sd_bus_message *m)
{
    struct ae_peer p=AE_PEER_INIT; int r=ae_peer_capture(m,&p);
    if (r==0 && p.connection_uid!=0) r=-EACCES;
    ae_peer_clear(&p); return r;
}
static int info(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)userdata; (void)error;
    uint8_t epoch[16],runtime[16]; memcpy(epoch,profile.system_epoch,16); memcpy(runtime,profile.system_runtime,16);
    if (strcmp(mode,"discovery-epoch")==0) epoch[15]^=1;
    if (strcmp(mode,"discovery-runtime")==0) runtime[15]^=1;
    sd_bus_message *reply=NULL; int r=sd_bus_message_new_method_return(m,&reply);
    if (r>=0) r=sd_bus_message_append(reply,"uu",strcmp(mode,"discovery-version")==0 ? 2U : 1U,1U);
    if (r>=0) r=sd_bus_message_open_container(reply,'a',"s");
    if (r>=0) r=sd_bus_message_close_container(reply);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',epoch,16);
    if (r>=0) r=sd_bus_message_append_array(reply,'y',runtime,16);
    if (r>=0) r=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); return r;
}
static int control(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)userdata; (void)error;
    CHECK(authorize(m)==0);
    if (strcmp(sd_bus_message_get_member(m),"Finish")==0) { finished=1; return sd_bus_reply_method_return(m,""); }
    if (strcmp(sd_bus_message_get_member(m),"Counts")==0) return sd_bus_reply_method_return(m,"uu",registers,prepared);
    const char *text=NULL; CHECK(sd_bus_message_read(m,"s",&text)>0 && strlen(text)<sizeof mode);
    strcpy(mode,text); registers=0; prepared=0;
    sd_bus_message_unref(pending); pending=NULL;
    CHECK(ae_exec_channel_close(&channel)==0);
    return sd_bus_reply_method_return(m,"");
}
static int exchange(sd_bus_message *m,void *userdata,sd_bus_error *error)
{
    (void)userdata; (void)error; CHECK(authorize(m)==0);
    const void *bytes=NULL; size_t size=0;
    CHECK(sd_bus_message_read_array(m,'y',&bytes,&size)>0 && size==16 && memcmp(bytes,profile.lease,16)==0);
    CHECK(sd_bus_message_read_array(m,'y',&bytes,&size)>0 && size==32 && memcmp(bytes,digest,32)==0 && sd_bus_message_at_end(m,1)==1);
    int registration=strcmp(sd_bus_message_get_member(m),"RegisterStarter")==0;
    if (registration) ++registers; else ++prepared;
    if (strcmp(mode,registration ? "register-error" : "prepared-error")==0)
        return sd_bus_reply_method_errorf(m,"org.aetheri.TestError","Intentional fixture reply");
    if (registration && strcmp(mode,"register-timeout")==0) { CHECK(pending==NULL); pending=sd_bus_message_ref(m); return 1; }
    int writer=-1;
    if (registration) {
        CHECK(channel==NULL && ae_exec_channel_new(&channel,&writer)==0);
        if (strcmp(mode,"writer-device")==0) { CHECK(close(writer)==0); writer=open("/dev/null",O_RDWR|O_CLOEXEC); CHECK(writer>=3); }
        if (strcmp(mode,"writer-blocking")==0) CHECK(fcntl(writer,F_SETFL,fcntl(writer,F_GETFL)&~O_NONBLOCK)==0);
    }
    const uint8_t *fields[]={profile.system_epoch,profile.system_runtime,profile.boot,profile.lease,profile.root_operation,digest};
    const char *names[]={"epoch","runtime","boot","lease","operation","digest"};
    sd_bus_message *reply=NULL; int r=sd_bus_message_new_method_return(m,&reply);
    for (size_t n=0;r>=0 && n<6;++n) {
        uint8_t value[32]; size_t length=n==5 ? 32U : 16U; memcpy(value,fields[n],length);
        char wanted[64]; CHECK(snprintf(wanted,sizeof wanted,"%s-%s",registration ? "register" : "prepared",names[n])>0);
        if (strcmp(mode,wanted)==0) value[length-1]^=1;
        if (registration && n==0 && strcmp(mode,"register-short")==0) --length;
        r=sd_bus_message_append_array(reply,'y',value,length);
    }
    uint64_t revision=registration ? 1U : 2U;
    if (strcmp(mode,registration ? "register-zero" : "prepared-zero")==0) revision=0;
    if (strcmp(mode,registration ? "register-overflow" : "prepared-overflow")==0) revision=UINT64_MAX;
    if (!registration && strcmp(mode,"prepared-same-revision")==0) revision=1;
    if (r>=0) r=sd_bus_message_append(reply,"t",revision);
    if (r>=0 && registration) r=sd_bus_message_append(reply,"h",writer);
    if (r>=0) r=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply); if (writer>=0) CHECK(close(writer)==0);
    return r;
}
static const sd_bus_vtable information[]={SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetProtocolInfo","","uuasayay",info,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static const sd_bus_vtable execution[]={SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD(AE_EXECUTION_REGISTER_METHOD,AE_EXECUTION_REGISTER_INPUT,AE_EXECUTION_REGISTER_OUTPUT,exchange,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD(AE_EXECUTION_PREPARED_METHOD,AE_EXECUTION_PREPARED_INPUT,AE_EXECUTION_PREPARED_OUTPUT,exchange,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static const sd_bus_vtable controls[]={SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Reset","s","",control,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Counts","","uu",control,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Finish","","",control,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
static int descriptors(void)
{
    DIR *d=opendir("/proc/self/fd"); CHECK(d!=NULL); int n=0;
    while (readdir(d)!=NULL) ++n;
    CHECK(closedir(d)==0); return n;
}
static void reset(sd_bus *bus,const char *value)
{ CHECK(sd_bus_call_method(bus,"org.aetheri.System1","/org/aetheri/TestExecutionClient","org.aetheri.TestExecutionClient1","Reset",NULL,NULL,"s",value)>=0); }
static void counts(sd_bus *bus,uint32_t reg,uint32_t prep)
{
    sd_bus_message *reply=NULL; uint32_t a=0,b=0;
    CHECK(sd_bus_call_method(bus,"org.aetheri.System1","/org/aetheri/TestExecutionClient","org.aetheri.TestExecutionClient1","Counts",NULL,&reply,"")>=0);
    CHECK(sd_bus_message_read(reply,"uu",&a,&b)>0 && a==reg && b==prep); sd_bus_message_unref(reply);
}
static void client(sd_bus *bus,const char *unit)
{
    const char *cases[]={"success","discovery-epoch","discovery-runtime","discovery-version","wrong-unit",
        "register-epoch","register-runtime","register-boot","register-lease","register-operation","register-digest","register-short",
        "register-zero","register-overflow","writer-device","writer-blocking","register-error","register-timeout",
        "prepared-epoch","prepared-runtime","prepared-boot","prepared-lease","prepared-operation","prepared-digest",
        "prepared-zero","prepared-overflow","prepared-same-revision","prepared-error"};
    int baseline=descriptors();
    for (size_t n=0;n<sizeof cases/sizeof cases[0];++n) {
        const char *test=cases[n]; reset(bus,test); struct ae_execution_client *c=NULL;
        int r=ae_execution_client_new(bus,&profile,strcmp(test,"wrong-unit")==0 ? "wrong.service" : unit,&c);
        int early=strncmp(test,"discovery-",10)==0 || strcmp(test,"wrong-unit")==0;
        if (early) { CHECK(r<0 && c==NULL); counts(bus,0,0); continue; }
        CHECK(r==0 && c!=NULL && ae_execution_client_prepared(c)==-EALREADY);
        int writer=0; CHECK(ae_execution_client_register(c,&writer)==-EINVAL && writer==0); writer=-1;
        r=ae_execution_client_register(c,&writer);
        int failed_register=strncmp(test,"register-",9)==0 || strncmp(test,"writer-",7)==0;
        if (failed_register) CHECK(r<0 && writer==-1); else CHECK(r==0 && writer>=3);
        int other=-1; CHECK(ae_execution_client_register(c,&other)==-EALREADY && other==-1);
        if (!failed_register) {
            r=ae_execution_client_prepared(c); CHECK(strcmp(test,"success")==0 ? r==0 : r<0);
            CHECK(ae_execution_client_prepared(c)==-EALREADY);
        }
        counts(bus,1,failed_register ? 0U : 1U);
        CHECK(ae_execution_client_close(&c)==0 && c==NULL && ae_execution_client_close(&c)==0);
        if (writer>=0) { CHECK(fcntl(writer,F_GETFD)>=0); CHECK(close(writer)==0); }
        CHECK(descriptors()==baseline);
    }
    reset(bus,"success"); struct ae_execution_client *c=NULL;
    CHECK(ae_execution_client_new(bus,&profile,unit,&c)==0);
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) { int writer=-1; CHECK(ae_execution_client_register(c,&writer)==-ECHILD && ae_execution_client_close(&c)==-ECHILD && c!=NULL); _exit(0); }
    int status=0; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    child=fork(); CHECK(child>=0);
    if (child==0) {
        CHECK(setgroups(0,NULL)==0 && setresgid(1000,1000,1000)==0 && setresuid(1000,1000,1000)==0);
        struct ae_execution_client *denied=NULL;
        CHECK(ae_execution_client_new(bus,&profile,unit,&denied)==-EPERM && denied==NULL); _exit(0);
    }
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(sd_bus_call_method(bus,"org.aetheri.System1","/org/aetheri/TestExecutionClient","org.aetheri.TestExecutionClient1","Finish",NULL,NULL,"")>=0);
    /* Actual Root peer death may race the control reply. No further fixture
     * effects can run: its loop exits after Finish; a stale/absent destination
     * fails and is never followed to a replacement. */
    int writer=-1; CHECK(ae_execution_client_register(c,&writer)<0 && writer==-1);
    CHECK(ae_execution_client_register(c,&writer)==-EALREADY && ae_execution_client_close(&c)==0);
    CHECK(descriptors()==baseline);
    printf("execution client: %zu protocol cases, actual Root UID/domain/unit/invocation/cgroup, writer lifetime, no retries and owner/departure checks passed\n",sizeof cases/sizeof cases[0]);
}
int main(int argc,char **argv)
{
    CHECK((argc==3 || argc==4) && getuid()==0 && geteuid()==0);
    FILE *f=fopen(argv[2],"rb"); CHECK(f!=NULL); uint8_t input[AE_EXECUTION_BYTES+1U];
    CHECK(fread(input,1,sizeof input,f)==AE_EXECUTION_BYTES && fclose(f)==0 && ae_execution_decode(input,AE_EXECUTION_BYTES,&profile)==0 && ae_execution_digest(&profile,digest)==0);
    sd_bus *bus=NULL; CHECK(sd_bus_new(&bus)>=0 && sd_bus_set_address(bus,"unix:path=/run/dbus/system_bus_socket")>=0 && sd_bus_set_bus_client(bus,1)>=0 && sd_bus_start(bus)>=0);
    if (strcmp(argv[1],"serve")==0) {
        CHECK(argc==3);
        CHECK(sd_bus_add_object_vtable(bus,NULL,"/org/aetheri/Management1","org.aetheri.Management1",information,NULL)>=0);
        CHECK(sd_bus_add_object_vtable(bus,NULL,"/org/aetheri/Execution1","org.aetheri.Execution1",execution,NULL)>=0);
        CHECK(sd_bus_add_object_vtable(bus,NULL,"/org/aetheri/TestExecutionClient","org.aetheri.TestExecutionClient1",controls,NULL)>=0);
        CHECK(sd_bus_request_name(bus,"org.aetheri.System1",0)>=0 && sd_notify(0,"READY=1")>0);
        while (!finished) {
            int r=sd_bus_process(bus,NULL);
            if (r<0) fprintf(stderr,"fixture mode=%s registers=%u prepared=%u process=%d (%s)\n",mode,registers,prepared,r,strerror(-r));
            CHECK(r>=0); if (r==0) CHECK(sd_bus_wait(bus,UINT64_MAX)>=0);
        }
        CHECK(ae_exec_channel_close(&channel)==0); sd_bus_message_unref(pending); CHECK(sd_bus_flush(bus)>=0);
    } else { CHECK(strcmp(argv[1],"client")==0 && argc==4); client(bus,argv[3]); }
    sd_bus_flush_close_unref(bus); return 0;
}
