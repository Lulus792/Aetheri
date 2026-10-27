#include "authority/reservation.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

int ae_reservation_write(sd_bus_message *message,const struct ae_launch_request *r)
{
    if (message==NULL) return -EINVAL;
    int result=ae_launch_request_valid(r);
    if (result!=0) return result;
    const uint8_t *ids[]={r->system_epoch,r->lease,r->launch,r->broker_epoch,r->broker_runtime,r->account,r->install,r->generation,r->data,r->boot};
    for (size_t n=0;result>=0 && n<10;++n) result=sd_bus_message_append_array(message,'y',ids[n],16);
    if (result>=0) result=sd_bus_message_append(message,"tsss",(uint64_t)r->install_revision,r->action,r->session,r->seat);
    return result<0 ? result : 0;
}
int ae_reservation_read(sd_bus_message *message,struct ae_launch_request *output)
{
    if (message==NULL || output==NULL) return -EINVAL;
    struct ae_launch_request r={0};
    uint8_t *ids[]={r.system_epoch,r.lease,r.launch,r.broker_epoch,r.broker_runtime,r.account,r.install,r.generation,r.data,r.boot};
    int result=0;
    for (size_t n=0;result==0 && n<10;++n) {
        const void *bytes=NULL; size_t size=0;
        int code=sd_bus_message_read_array(message,'y',&bytes,&size);
        if (code<=0 || size!=16) result=code<0 ? code : -EBADMSG;
        else memcpy(ids[n],bytes,16);
    }
    uint64_t revision=0;
    if (result==0) { int code=sd_bus_message_read(message,"t",&revision); if (code<=0 || revision>INT64_MAX) result=code<0 ? code : -EBADMSG; }
    char *targets[]={r.action,r.session,r.seat}; const size_t sizes[]={sizeof r.action,sizeof r.session,sizeof r.seat};
    for (size_t n=0;result==0 && n<3;++n) {
        const char *text=NULL;
        int code=sd_bus_message_read(message,"s",&text);
        if (code<=0 || text==NULL || strnlen(text,sizes[n])>=sizes[n]) result=code<0 ? code : -EBADMSG;
        else memcpy(targets[n],text,strlen(text)+1U);
    }
    if (result==0) r.install_revision=(int64_t)revision;
    if (result==0 && sd_bus_message_at_end(message,1)!=1) result=-EBADMSG;
    if (result==0) result=ae_launch_request_valid(&r);
    if (result==0) *output=r;
    return result;
}

static int receipt_valid(const struct ae_reservation_receipt *r)
{
    if (r==NULL || r->dispatch.revision<1 || r->dispatch.install_gate_revision<1 || (r->cause_present!=0 && r->cause_present!=1) ||
        (r->withdrawn!=0 && r->withdrawn!=1)) return -EBADMSG;
    const uint8_t *ids[]={r->system_epoch,r->system_runtime,r->boot,r->plan,r->dispatch.operation,r->broker_runtime,r->dispatch.launch,r->lease,r->cause_epoch,r->cause};
    for (size_t n=0;n<(r->cause_present ? 10U : 8U);++n) if ((ids[n][6]>>4)!=4 || (ids[n][8]&0xc0)!=0x80) return -EBADMSG;
    if (memchr(r->dispatch.state,0,sizeof r->dispatch.state)==NULL) return -EBADMSG;
    const char *states[]={"reserved","dispatching","observed","closing","terminal","uncertain"};
    int known=0; for (size_t n=0;n<6;++n) if (strcmp(r->dispatch.state,states[n])==0) known=1;
    if (!known || (r->withdrawn && (!r->cause_present || strcmp(r->dispatch.state,"terminal")!=0))) return -EBADMSG;
    return 0;
}
int ae_receipt_write(sd_bus_message *message,const struct ae_reservation_receipt *r)
{
    if (message==NULL) return -EINVAL;
    int result=receipt_valid(r);
    if (result!=0) return result;
    const uint8_t *ids[]={r->system_epoch,r->system_runtime,r->boot,r->plan,r->dispatch.operation,r->intent,r->fingerprint,r->broker_runtime,r->dispatch.launch,r->lease};
    for (size_t n=0;result>=0 && n<10;++n) result=sd_bus_message_append_array(message,'y',ids[n],n==5 || n==6 ? 32U : 16U);
    if (result>=0) result=sd_bus_message_append(message,"sttb",r->dispatch.state,(uint64_t)r->dispatch.install_gate_revision,(uint64_t)r->dispatch.revision,r->withdrawn);
    if (result>=0) result=sd_bus_message_append_array(message,'y',r->cause_epoch,r->cause_present ? 16U : 0U);
    if (result>=0) result=sd_bus_message_append_array(message,'y',r->cause,r->cause_present ? 16U : 0U);
    return result<0 ? result : 0;
}
int ae_receipt_read(sd_bus_message *message,struct ae_reservation_receipt *output)
{
    if (message==NULL || output==NULL) return -EINVAL;
    struct ae_reservation_receipt r={0};
    uint8_t *ids[]={r.system_epoch,r.system_runtime,r.boot,r.plan,r.dispatch.operation,r.intent,r.fingerprint,r.broker_runtime,r.dispatch.launch,r.lease};
    int result=0;
    for (size_t n=0;result==0 && n<10;++n) {
        const void *data=NULL; size_t size=0; int code=sd_bus_message_read_array(message,'y',&data,&size);
        if (code<=0 || size!=(n==5 || n==6 ? 32U : 16U)) result=code<0 ? code : -EBADMSG;
        else memcpy(ids[n],data,size);
    }
    const char *state=NULL; uint64_t gate=0,revision=0;
    if (result==0 && (sd_bus_message_read(message,"sttb",&state,&gate,&revision,&r.withdrawn)<=0 || gate>INT64_MAX || revision>INT64_MAX || strnlen(state,16)>=16)) result=-EBADMSG;
    if (result==0) { strcpy(r.dispatch.state,state); r.dispatch.revision=(int64_t)revision; r.dispatch.install_gate_revision=(int64_t)gate; }
    size_t first=0;
    for (size_t n=0;result==0 && n<2;++n) {
        const void *data=NULL; size_t size=0;
        int code=sd_bus_message_read_array(message,'y',&data,&size);
        if (code<=0 || (size!=0 && size!=16) || (n==1 && size!=first)) result=code<0 ? code : -EBADMSG;
        else { if (size) memcpy(n==0 ? r.cause_epoch : r.cause,data,size); first=size; }
    }
    r.cause_present=first==16;
    if (result==0 && sd_bus_message_at_end(message,1)!=1) result=-EBADMSG;
    if (result==0) result=receipt_valid(&r);
    if (result==0) *output=r;
    return result;
}
