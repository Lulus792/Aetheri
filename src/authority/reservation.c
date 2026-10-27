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
