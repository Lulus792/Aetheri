#include "authority/projection.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"projection wire line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static sd_bus *connection;
static struct ae_install_snapshot expected;
static int fault,calls;
static void id(uint8_t bytes[16],uint8_t last)
{
    memset(bytes,0,16); bytes[6]=0x40; bytes[8]=0x80; bytes[15]=last;
}
static sd_bus_message *message(void)
{
    sd_bus_message *result=NULL;
    CHECK(sd_bus_message_new_signal(connection,&result,"/org/aetheri/TestProjection","org.aetheri.TestProjection1","Observation")>=0);
    return result;
}
static void ready(sd_bus_message *value)
{
    CHECK(sd_bus_message_seal(value,1,0)>=0 && sd_bus_message_rewind(value,1)>=0);
}
static int equal(const struct ae_install_snapshot *a,const struct ae_install_snapshot *b)
{
    return memcmp(a->system_epoch,b->system_epoch,16)==0 && memcmp(a->system_runtime,b->system_runtime,16)==0 &&
        memcmp(a->boot,b->boot,16)==0 && memcmp(a->install,b->install,16)==0 && strcmp(a->component,b->component)==0 &&
        memcmp(a->publisher,b->publisher,16)==0 && memcmp(a->account,b->account,16)==0 && memcmp(a->scope,b->scope,16)==0 &&
        memcmp(a->generation,b->generation,16)==0 && memcmp(a->artifact,b->artifact,32)==0 && a->gate==b->gate &&
        a->install_revision==b->install_revision && a->generation_revision==b->generation_revision &&
        a->gate_revision==b->gate_revision && a->catalogue_revision==b->catalogue_revision;
}
/* Independent wire constructor; stop at each field boundary, vary individual
 * lengths/integers/tokens, or append an extra field. Not a transport/auth test. */
static sd_bus_message *fixture(int fields,int bad_id,int bad_integer,const char *component,const char *gate,int extra)
{
    sd_bus_message *value=message();
    const uint8_t *ids[]={expected.system_epoch,expected.system_runtime,expected.boot,expected.install,expected.publisher,
        expected.account,expected.scope,expected.generation,expected.artifact};
    int field=0;
#define MORE() do { if (field++>=fields) goto finish; } while (0)
    for (int n=0;n<4;++n) { MORE(); CHECK(sd_bus_message_append_array(value,'y',ids[n],n==bad_id ? 15U : 16U)>=0); }
    MORE(); CHECK(sd_bus_message_append(value,"s",component)>=0);
    for (int n=4;n<7;++n) {
        MORE(); uint8_t padded[17]={0}; memcpy(padded,ids[n],16);
        CHECK(sd_bus_message_append_array(value,'y',padded,n==bad_id ? 17U : 16U)>=0);
    }
    MORE(); CHECK(sd_bus_message_append(value,"t",bad_integer==0 ? UINT64_MAX : (uint64_t)expected.install_revision)>=0);
    MORE(); CHECK(sd_bus_message_append_array(value,'y',ids[7],bad_id==7 ? 0U : 16U)>=0);
    MORE(); CHECK(sd_bus_message_append_array(value,'y',ids[8],bad_id==8 ? 31U : 32U)>=0);
    MORE(); CHECK(sd_bus_message_append(value,"t",bad_integer==1 ? UINT64_MAX : (uint64_t)expected.generation_revision)>=0);
    MORE(); CHECK(sd_bus_message_append(value,"s",gate)>=0);
    MORE(); CHECK(sd_bus_message_append(value,"t",bad_integer==2 ? UINT64_MAX : (uint64_t)expected.gate_revision)>=0);
    MORE(); CHECK(sd_bus_message_append(value,"t",bad_integer==3 ? UINT64_MAX : (uint64_t)expected.catalogue_revision)>=0);
    if (extra) CHECK(sd_bus_message_append(value,"u",7U)>=0);
finish:
#undef MORE
    ready(value);
    return value;
}
int __wrap_sd_bus_call(sd_bus *bus,sd_bus_message *request,uint64_t timeout,sd_bus_error *error,sd_bus_message **reply)
{
    (void)error;
    CHECK(bus==connection && timeout==2000000);
    CHECK(strcmp(sd_bus_message_get_destination(request),"org.aetheri.System1")==0 &&
        strcmp(sd_bus_message_get_path(request),"/org/aetheri/Catalogue1")==0 &&
        strcmp(sd_bus_message_get_interface(request),"org.aetheri.Catalogue1")==0 &&
        strcmp(sd_bus_message_get_member(request),"ResolveAppInstall")==0);
    ++calls;
    ready(request);
    const uint8_t *references[]={expected.system_epoch,expected.system_runtime,expected.install};
    for (size_t n=0;n<3;++n) {
        const void *bytes=NULL; size_t size=0;
        CHECK(sd_bus_message_read_array(request,'y',&bytes,&size)>0 && size==16 && memcmp(bytes,references[n],16)==0);
    }
    CHECK(sd_bus_message_at_end(request,1)==1);
    if (fault==1) return -ETIMEDOUT;
    struct ae_install_snapshot snapshot=expected;
    if (fault==2) snapshot.system_epoch[15]^=1;
    if (fault==3) snapshot.system_runtime[15]^=1;
    if (fault==4) snapshot.boot[15]^=1;
    if (fault==5) snapshot.account[15]^=1;
    if (fault==6) snapshot.install[15]^=1;
    *reply=message(); CHECK(ae_projection_write(*reply,&snapshot)==0); ready(*reply);
    return 1;
}
int main(void)
{
    CHECK(sd_bus_open_user(&connection)>=0);
    uint8_t *ids[]={expected.system_epoch,expected.system_runtime,expected.boot,expected.install,expected.publisher,
        expected.account,expected.scope,expected.generation};
    for (size_t n=0;n<8;++n) id(ids[n],(uint8_t)(n+1U));
    memset(expected.artifact,0x71,32); strcpy(expected.component,"aetheri.probe");
    expected.install_revision=17; expected.generation_revision=18; expected.gate_revision=19; expected.catalogue_revision=20;
    expected.gate=AE_INSTALL_GATE_CLOSED;
    struct ae_install_snapshot output,marker; memset(&marker,0xa5,sizeof marker);
    sd_bus_message *value=fixture(15,-1,-1,expected.component,"closed",0);
    CHECK(ae_projection_read(value,&output)==0 && equal(&output,&expected)); sd_bus_message_unref(value);
    for (int fields=0;fields<15;++fields) {
        value=fixture(fields,-1,-1,expected.component,"closed",0); output=marker;
        CHECK(ae_projection_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    }
    for (int n=0;n<9;++n) {
        value=fixture(15,n,-1,expected.component,"closed",0); output=marker;
        CHECK(ae_projection_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    }
    for (int n=0;n<4;++n) {
        value=fixture(15,-1,n,expected.component,"closed",0); output=marker;
        CHECK(ae_projection_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    }
    char long_component[130]; memset(long_component,'a',129); long_component[129]=0;
    const char *components[]={"","Bad.Token",long_component};
    for (size_t n=0;n<3;++n) {
        value=fixture(15,-1,-1,components[n],"closed",0); output=marker;
        CHECK(ae_projection_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    }
    value=fixture(15,-1,-1,expected.component,"unknown",0); output=marker;
    CHECK(ae_projection_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    value=fixture(15,-1,-1,expected.component,"closed",1); output=marker;
    CHECK(ae_projection_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    for (int gate=AE_INSTALL_GATE_OPEN;gate<=AE_INSTALL_GATE_CLOSED;++gate) {
        expected.gate=(enum ae_install_gate)gate;
        value=message(); CHECK(ae_projection_write(value,&expected)==0); ready(value);
        CHECK(ae_projection_read(value,&output)==0 && equal(&output,&expected)); sd_bus_message_unref(value);
    }
    struct ae_install_snapshot invalid=expected; invalid.system_epoch[6]=0;
    value=message(); CHECK(ae_projection_write(value,&invalid)<0 && strcmp(sd_bus_message_get_signature(value,1),"")==0); sd_bus_message_unref(value);
    struct ae_projection_context context;
    memcpy(context.system_epoch,expected.system_epoch,16); memcpy(context.system_runtime,expected.system_runtime,16);
    memcpy(context.boot,expected.boot,16); memcpy(context.user_runtime,expected.system_runtime,16);
    for (fault=0;fault<=6;++fault) {
        int previous=calls; output=marker;
        int result=ae_projection_fetch(connection,&context,expected.account,expected.install,&output);
        CHECK(calls==previous+1);
        if (fault==0) CHECK(result==0 && equal(&output,&expected));
        else CHECK(result==(fault==1 ? -ETIMEDOUT : -ESTALE) && memcmp(&output,&marker,sizeof marker)==0);
    }
    sd_bus_flush_close_unref(connection);
    puts("Projection codec: independent wire, all truncations/lengths/integers/tokens, source tuple mismatch and one-call timeout passed.");
    return 0;
}
