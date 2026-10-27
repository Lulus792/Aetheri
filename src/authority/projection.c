#include "authority/projection.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

static int blob(sd_bus_message *message,uint8_t *output,size_t length)
{
    const void *data=NULL;
    size_t size=0;
    int result=sd_bus_message_read_array(message,'y',&data,&size);
    if (result<0) return result;
    if (result==0 || size!=length) return -EBADMSG;
    memcpy(output,data,length);
    return 0;
}

int ae_projection_read(sd_bus_message *message,struct ae_install_snapshot *output)
{
    if (message==NULL || output==NULL) return -EINVAL;
    struct ae_install_snapshot snapshot={0};
    uint8_t *ids[]={snapshot.system_epoch,snapshot.system_runtime,snapshot.boot,snapshot.install};
    int result=0;
    for (size_t n=0;result==0 && n<4;++n) result=blob(message,ids[n],16);
    const char *component=NULL,*gate=NULL;
    if (result==0) {
        int code=sd_bus_message_read(message,"s",&component);
        if (code<=0 || component==NULL || strnlen(component,129)>128) result=code<0 ? code : -EBADMSG;
        else memcpy(snapshot.component,component,strlen(component)+1U);
    }
    if (result==0) result=blob(message,snapshot.publisher,16);
    if (result==0) result=blob(message,snapshot.account,16);
    if (result==0) result=blob(message,snapshot.scope,16);
    uint64_t install_revision=0,generation_revision=0,gate_revision=0,catalogue_revision=0;
    if (result==0) { int code=sd_bus_message_read(message,"t",&install_revision); if (code<=0) result=code<0 ? code : -EBADMSG; }
    if (result==0) result=blob(message,snapshot.generation,16);
    if (result==0) result=blob(message,snapshot.artifact,32);
    if (result==0) { int code=sd_bus_message_read(message,"tstt",&generation_revision,&gate,&gate_revision,&catalogue_revision); if (code<=0) result=code<0 ? code : -EBADMSG; }
    if (result==0 && (install_revision>INT64_MAX || generation_revision>INT64_MAX || gate_revision>INT64_MAX || catalogue_revision>INT64_MAX)) result=-EBADMSG;
    if (result==0) {
        snapshot.install_revision=(int64_t)install_revision; snapshot.generation_revision=(int64_t)generation_revision;
        snapshot.gate_revision=(int64_t)gate_revision; snapshot.catalogue_revision=(int64_t)catalogue_revision;
        if (gate==NULL) result=-EBADMSG;
        else if (strcmp(gate,"open")==0) snapshot.gate=AE_INSTALL_GATE_OPEN;
        else if (strcmp(gate,"closing")==0) snapshot.gate=AE_INSTALL_GATE_CLOSING;
        else if (strcmp(gate,"closed")==0) snapshot.gate=AE_INSTALL_GATE_CLOSED;
        else result=-EBADMSG;
    }
    if (result==0 && sd_bus_message_at_end(message,1)!=1) result=-EBADMSG;
    if (result==0) result=ae_install_snapshot_valid(&snapshot);
    if (result==0) *output=snapshot;
    return result;
}

int ae_projection_write(sd_bus_message *message,const struct ae_install_snapshot *snapshot)
{
    if (message==NULL) return -EINVAL;
    int result=ae_install_snapshot_valid(snapshot);
    const uint8_t *ids[4]={NULL,NULL,NULL,NULL};
    if (result==0) { ids[0]=snapshot->system_epoch; ids[1]=snapshot->system_runtime; ids[2]=snapshot->boot; ids[3]=snapshot->install; }
    for (size_t n=0;result>=0 && n<4;++n) result=sd_bus_message_append_array(message,'y',ids[n],16);
    if (result>=0) result=sd_bus_message_append(message,"s",snapshot->component);
    if (result>=0) result=sd_bus_message_append_array(message,'y',snapshot->publisher,16);
    if (result>=0) result=sd_bus_message_append_array(message,'y',snapshot->account,16);
    if (result>=0) result=sd_bus_message_append_array(message,'y',snapshot->scope,16);
    if (result>=0) result=sd_bus_message_append(message,"t",(uint64_t)snapshot->install_revision);
    if (result>=0) result=sd_bus_message_append_array(message,'y',snapshot->generation,16);
    if (result>=0) result=sd_bus_message_append_array(message,'y',snapshot->artifact,32);
    const char *gate=result>=0 && snapshot->gate==AE_INSTALL_GATE_OPEN ? "open" : result>=0 && snapshot->gate==AE_INSTALL_GATE_CLOSING ? "closing" : "closed";
    if (result>=0) result=sd_bus_message_append(message,"tstt",(uint64_t)snapshot->generation_revision,gate,
        (uint64_t)snapshot->gate_revision,(uint64_t)snapshot->catalogue_revision);
    return result<0 ? result : 0;
}

int ae_projection_fetch(sd_bus *system,const struct ae_projection_context *context,
                         const uint8_t account[16],const uint8_t install[16],
                         struct ae_install_snapshot *output)
{
    if (system==NULL || context==NULL || account==NULL || install==NULL || output==NULL) return -EINVAL;
    sd_bus_message *request=NULL,*reply=NULL;
    int result=sd_bus_message_new_method_call(system,&request,"org.aetheri.System1","/org/aetheri/Catalogue1","org.aetheri.Catalogue1","ResolveAppInstall");
    if (result>=0) result=sd_bus_message_append_array(request,'y',context->system_epoch,16);
    if (result>=0) result=sd_bus_message_append_array(request,'y',context->user_runtime,16);
    if (result>=0) result=sd_bus_message_append_array(request,'y',install,16);
    if (result>=0) result=sd_bus_call(system,request,2000000,NULL,&reply);
    struct ae_install_snapshot snapshot;
    if (result>=0) result=ae_projection_read(reply,&snapshot);
    if (result==0 && (memcmp(snapshot.system_epoch,context->system_epoch,16)!=0 || memcmp(snapshot.system_runtime,context->system_runtime,16)!=0 ||
        memcmp(snapshot.boot,context->boot,16)!=0 || memcmp(snapshot.install,install,16)!=0 || memcmp(snapshot.account,account,16)!=0)) result=-ESTALE;
    sd_bus_message_unref(request); sd_bus_message_unref(reply);
    if (result==0) *output=snapshot;
    return result;
}
