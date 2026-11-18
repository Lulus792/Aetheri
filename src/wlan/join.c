#include "wlan/join.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define IWD_NETWORK AE_IWD_BUS ".Network"
#define IWD_AGENT AE_IWD_BUS ".Agent"
#define IWD_AGENT_MANAGER AE_IWD_BUS ".AgentManager"
#define IWD_MANAGER_PATH "/net/connman/iwd"
#define AGENT_PATH "/org/aetheri/Wlan1/Agent"
#define RECHECK_USEC UINT64_C(500000)

struct join_attempt {
    const struct ae_peer *iwd;
    const char *path;
    bool (*authorized)(void *);
    void *caller;
    char passphrase[AE_WLAN_PASSPHRASE_CAPACITY];
    bool answered, complete;
    int result;
};

static uint64_t now_usec(void) {
    struct timespec time;
    if(clock_gettime(CLOCK_MONOTONIC,&time)<0) return 0;
    return (uint64_t)time.tv_sec*UINT64_C(1000000)+(uint64_t)time.tv_nsec/1000;
}

static bool original_sender(sd_bus_message *message,const struct ae_peer *iwd) {
    const char *sender=sd_bus_message_get_sender(message);
    return sender && !strcmp(sender,iwd->unique_name) && ae_peer_alive(iwd)>=0;
}

static int passphrase_request(sd_bus_message *message,void *data,sd_bus_error *error) {
    struct join_attempt *attempt=data;
    const char *path=NULL;
    if(attempt->answered || !attempt->passphrase[0] || !original_sender(message,attempt->iwd) ||
       sd_bus_message_read(message,"o",&path)<=0 || strcmp(path,attempt->path) ||
       !attempt->authorized(attempt->caller))
        return sd_bus_error_set_const(error,IWD_AGENT ".Error.Canceled","No authorized answer for this request");
    attempt->answered=true;
    sd_bus_message *reply=NULL;
    int result=sd_bus_message_new_method_return(message,&reply);
    if(result>=0) result=sd_bus_message_sensitive(reply);
    if(result>=0) result=sd_bus_message_append(reply,"s",attempt->passphrase);
    explicit_bzero(attempt->passphrase,sizeof attempt->passphrase);
    if(result>=0) result=sd_bus_send(NULL,reply,NULL);
    sd_bus_message_unref(reply);
    return result;
}

static int agent_retired(sd_bus_message *message,void *data,sd_bus_error *error) {
    (void)error;
    struct join_attempt *attempt=data;
    if(original_sender(message,attempt->iwd)) {
        explicit_bzero(attempt->passphrase,sizeof attempt->passphrase);
        if(!attempt->complete) {
            attempt->complete=true;
            attempt->result=-ENOLINK;
        }
    }
    return 1;
}

static const sd_bus_vtable agent_methods[]={
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("RequestPassphrase","o","s",passphrase_request,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Cancel","s","",agent_retired,SD_BUS_VTABLE_UNPRIVILEGED|SD_BUS_VTABLE_METHOD_NO_REPLY),
    SD_BUS_METHOD("Release","","",agent_retired,SD_BUS_VTABLE_UNPRIVILEGED|SD_BUS_VTABLE_METHOD_NO_REPLY),
    SD_BUS_VTABLE_END,
};

static int joined(sd_bus_message *message,void *data,sd_bus_error *error) {
    (void)error;
    struct join_attempt *attempt=data;
    attempt->complete=true;
    if(!original_sender(message,attempt->iwd)) attempt->result=-ETIMEDOUT;
    else if(sd_bus_message_is_method_error(message,NULL)!=0) attempt->result=-ENOLINK;
    else attempt->result=sd_bus_message_has_signature(message,"")?0:-ETIMEDOUT;
    return 1;
}

static int await_join(sd_bus *bus,struct join_attempt *attempt) {
    uint64_t start=now_usec(),next_check=start;
    if(!start) return -ETIMEDOUT;
    while(!attempt->complete) {
        uint64_t now=now_usec();
        if(!now || now-start>=AE_WLAN_JOIN_USEC || ae_peer_alive(attempt->iwd)<0) return -ETIMEDOUT;
        if(now>=next_check) {
            if(!attempt->authorized(attempt->caller)) return -EPERM;
            next_check=now+RECHECK_USEC;
        }
        int result=sd_bus_process(bus,NULL);
        if(result<0 || (!result && sd_bus_wait(bus,next_check-now)<0)) return -ETIMEDOUT;
    }
    return attempt->result;
}

int ae_wlan_join(sd_bus *bus,const struct ae_peer *iwd,const char *network,enum ae_wlan_security security,
                  const char *passphrase,bool (*authorized)(void *),void *caller) {
    size_t size=strnlen(passphrase,AE_WLAN_PASSPHRASE_CAPACITY);
    if((security!=AE_WLAN_OPEN && security!=AE_WLAN_PERSONAL) ||
       (size && (security!=AE_WLAN_PERSONAL || !ae_wlan_passphrase_valid(passphrase,size)))) return -EINVAL;
    const char *expected=security==AE_WLAN_OPEN?"open":"psk";
    char *actual=NULL;
    int result=sd_bus_get_property_string(bus,iwd->unique_name,network,IWD_NETWORK,"Type",NULL,&actual);
    bool exact=result>=0 && !strcmp(actual,expected) && ae_peer_alive(iwd)>=0;
    free(actual);
    if(!exact) return -ESTALE;
    if(!authorized(caller)) return -EPERM;

    struct join_attempt attempt={.iwd=iwd,.path=network,.authorized=authorized,.caller=caller};
    sd_bus_slot *agent=NULL,*request=NULL;
    if(security==AE_WLAN_PERSONAL) {
        memcpy(attempt.passphrase,passphrase,size);
        result=sd_bus_add_object_vtable(bus,&agent,AGENT_PATH,IWD_AGENT,agent_methods,&attempt);
        if(result>=0)
            result=sd_bus_call_method(bus,iwd->unique_name,IWD_MANAGER_PATH,IWD_AGENT_MANAGER,
                                     "RegisterAgent",NULL,NULL,"o",AGENT_PATH);
        if(result<0) result=-ENOLINK;
    }
    if(result>=0 && !authorized(caller)) result=-EPERM;
    if(result>=0 && ae_peer_alive(iwd)<0) result=-ESTALE;
    sd_bus_message *connect=NULL;
    if(result>=0)
        result=sd_bus_message_new_method_call(bus,&connect,iwd->unique_name,network,IWD_NETWORK,"Connect");
    if(result>=0) result=sd_bus_call_async(bus,&request,connect,joined,&attempt,AE_WLAN_JOIN_USEC);
    sd_bus_message_unref(connect);
    if(result>=0) result=await_join(bus,&attempt);
    explicit_bzero(attempt.passphrase,sizeof attempt.passphrase);
    sd_bus_slot_unref(request);
    sd_bus_slot_unref(agent);
    if(result<0) return result;
    if(!authorized(caller)) return -EPERM;
    int connected=0;
    result=sd_bus_get_property_trivial(bus,iwd->unique_name,network,IWD_NETWORK,"Connected",NULL,'b',&connected);
    return result>=0 && connected && ae_peer_alive(iwd)>=0?0:-ETIMEDOUT;
}
