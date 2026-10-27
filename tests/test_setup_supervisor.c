#include "authority/setup.h"
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"setup supervisor line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static sd_bus *connection;
static int fault,refs,invocations;
static sd_bus_message *message(void)
{
    sd_bus_message *m=NULL;
    CHECK(sd_bus_message_new_signal(connection,&m,"/org/aetheri/Test","org.aetheri.Test1","Snapshot")>=0); return m;
}
static void seal(sd_bus_message *m) { CHECK(sd_bus_message_seal(m,1,0)>=0 && sd_bus_message_rewind(m,1)>=0); }
int __wrap_ae_setup_verify_image(const struct ae_setup_profile *p) { (void)p; return fault==1 ? -EACCES : 0; }
int __wrap_sd_bus_call_method(sd_bus *bus,const char *destination,const char *path,const char *interface,const char *method,
    sd_bus_error *error,sd_bus_message **reply,const char *signature,...)
{
    (void)path; (void)interface; (void)error;
    CHECK(bus==connection && strcmp(destination,"org.freedesktop.systemd1")==0 && strcmp(signature,"s")==0);
    if (strcmp(method,"RefUnit")==0) { if (fault==2) return -EACCES; ++refs; return 1; }
    if (strcmp(method,"UnrefUnit")==0) { --refs; return 1; }
    CHECK(strcmp(method,"GetUnit")==0); *reply=message();
    CHECK(sd_bus_message_append(*reply,"o","/org/freedesktop/systemd1/unit/selected")>=0); seal(*reply); return 1;
}
int __wrap_sd_bus_get_property_string(sd_bus *bus,const char *destination,const char *path,const char *interface,
    const char *property,sd_bus_error *error,char **output)
{
    (void)bus; (void)destination; (void)path; (void)interface; (void)error;
    const char *value=NULL;
    if (strcmp(property,"Type")==0) value=fault==3 ? "exec" : "notify";
    if (strcmp(property,"User")==0) value=fault==4 ? "aetheri" : "root";
    if (strcmp(property,"Group")==0) value="root";
    if (strcmp(property,"Restart")==0) value=fault==5 ? "always" : "no";
    if (strcmp(property,"ActiveState")==0) value=fault==17 ? "active" : "failed";
    if (strcmp(property,"ControlGroup")==0) value=fault==18 ? "/system.slice/leftover" : "";
    CHECK(value!=NULL); *output=strdup(value); CHECK(*output!=NULL); return 1;
}
int __wrap_sd_bus_get_property_trivial(sd_bus *bus,const char *destination,const char *path,const char *interface,
    const char *property,sd_bus_error *error,char type,void *output)
{
    (void)bus; (void)destination; (void)path; (void)interface; (void)error;
    if (strcmp(property,"NRestarts")==0) { CHECK(type=='u'); *(uint32_t *)output=fault==7 ? 1U : 0U; }
    else if (strcmp(property,"NoNewPrivileges")==0) { CHECK(type=='b'); *(int *)output=fault==8 ? 0 : 1; }
    else if (strcmp(property,"PrivateMounts")==0) { CHECK(type=='b'); *(int *)output=fault==9 ? 0 : 1; }
    else if (strcmp(property,"CapabilityBoundingSet")==0) { CHECK(type=='t'); *(uint64_t *)output=fault==10 ? UINT64_MAX : UINT64_C(0x200004); }
    else { CHECK(strcmp(property,"MainPID")==0 && type=='u'); *(uint32_t *)output=fault==19 ? 123U : 0U; }
    return 1;
}
int __wrap_sd_bus_get_property(sd_bus *bus,const char *destination,const char *path,const char *interface,
    const char *property,sd_bus_error *error,sd_bus_message **output,const char *signature)
{
    (void)bus; (void)destination; (void)path; (void)interface; (void)error;
    sd_bus_message *m=message();
    if (strcmp(property,"SELinuxContext")==0) {
        CHECK(strcmp(signature,"(bs)")==0);
        CHECK(sd_bus_message_append(m,"(bs)",fault==6,"system_u:system_r:aetheri_setup_t:s0")>=0);
    } else if (strcmp(property,"InvocationID")==0) {
        uint8_t id[16]={1}; if (fault==21 && invocations>0) id[0]=2;
        ++invocations; CHECK(sd_bus_message_append_array(m,'y',id,fault==22 ? 15U : 16U)>=0);
    } else if (strcmp(property,"Job")==0) {
        CHECK(sd_bus_message_append(m,"(uo)",fault==20 ? 1U : 0U,"/")>=0);
    } else {
        CHECK(strcmp(signature,"a(sasbttttuii)")==0);
        CHECK(sd_bus_message_open_container(m,'a',"(sasbttttuii)")>=0);
        if (strcmp(property,"ExecStart")==0 || (fault==11 && strcmp(property,"ExecStopPost")==0)) {
            const char *image="/var/lib/aetheri-p0/setup-helper";
            CHECK(sd_bus_message_open_container(m,'r',"sasbttttuii")>=0);
            CHECK(sd_bus_message_append(m,"s",fault==12 ? "/other/image" : image)>=0);
            CHECK(sd_bus_message_open_container(m,'a',"s")>=0 && sd_bus_message_append(m,"s",image)>=0);
            if (fault==13) CHECK(sd_bus_message_append(m,"s","extra")>=0);
            CHECK(sd_bus_message_close_container(m)>=0);
            CHECK(sd_bus_message_append(m,"bttttuii",fault==14,UINT64_C(100),UINT64_C(10),UINT64_C(110),fault==16 ? UINT64_C(9) : UINT64_C(20),
                123U,CLD_EXITED,fault==15 ? 0 : 5)>=0);
            CHECK(sd_bus_message_close_container(m)>=0);
        }
        CHECK(sd_bus_message_close_container(m)>=0);
    }
    seal(m); *output=m; return 1;
}
int main(void)
{
    CHECK(sd_bus_open_user(&connection)>=0);
    struct ae_setup_profile p={0}; p.launch[6]=0x40; p.launch[8]=0x80; p.launch[15]=1;
    uint8_t hash[32]={3},expected[32]={0};
    for (int test=0;test<=22;++test) {
        fault=test; invocations=0;
        struct ae_setup_service s={.bus=connection,.peer=AE_PEER_INIT};
        int result=ae_setup_observe_exit(&s,&p,hash);
        if (test==0) { CHECK(result==0 && refs==1 && s.context.invocation[0]==1); memcpy(expected,s.evidence,32); }
        else CHECK(result<0 && refs==0);
        ae_setup_service_clear(&s); CHECK(refs==0);
    }
    fault=0; invocations=0;
    struct ae_setup_service s={.bus=connection,.peer=AE_PEER_INIT};
    CHECK(ae_setup_observe_exit(&s,&p,hash)==0 && memcmp(expected,s.evidence,32)==0);
    ae_setup_service_clear(&s); CHECK(refs==0);
    sd_bus_flush_close_unref(connection);
    puts("No-exec recovery: exact image/unit/argv/label/restart/exit/invocation and independent drain rejection passed; no held peer inferred."); return 0;
}
