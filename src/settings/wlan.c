#define _POSIX_C_SOURCE 200809L
#include "settings/wlan.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#define WLAN_BUS "org.aetheri.Wlan1"
#define WLAN_PATH "/org/aetheri/Wlan1"
#define BUS_SOCKET "unix:path=/run/dbus/system_bus_socket"
#define CALL_USEC UINT64_C(4000000)
#define READ_INTERVAL UINT64_C(200000)
#define CHANGE_WINDOW UINT64_C(10000000)

static uint64_t now_usec(void) {
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC,&now)<0) return 0;
    return (uint64_t)now.tv_sec*UINT64_C(1000000)+(uint64_t)now.tv_nsec/1000;
}

void ae_settings_wlan_close(struct ae_settings_wlan_client *client) {
    sd_bus_close_unref(client->bus);
    if(client->wait_fd>=0) close(client->wait_fd);
    *client=(struct ae_settings_wlan_client){.model=client->model,.wait_fd=-1,.parent_fd=-1};
}

static void finish(struct ae_settings_wlan_client *client,enum ae_settings_wlan_state state,bool uncertain) {
    struct ae_settings_wlan *model=client->model;
    model->state=state;
    model->busy=false;
    model->changing=false;
    model->uncertain=uncertain;
    if(state!=AE_SETTINGS_WLAN_ON && !uncertain) {
        model->scan=AE_SETTINGS_SCAN_IDLE;
        model->networks=(struct ae_wlan_networks){0};
    }
    ae_settings_wlan_close(client);
}

static void lost(struct ae_settings_wlan_client *client) {
    struct ae_settings_wlan *model=client->model;
    if(model->scan==AE_SETTINGS_SCAN_RUNNING) {
        model->scan=AE_SETTINGS_SCAN_FAILED;
        finish(client,model->state,false);
        return;
    }
    bool uncertain=model->changing || model->uncertain;
    finish(client,uncertain?model->state:AE_SETTINGS_WLAN_UNAVAILABLE,uncertain);
}

static enum ae_settings_wlan_state read_state(sd_bus_message *message) {
    const char *state=NULL;
    if(sd_bus_message_is_method_error(message,NULL)!=0 ||
       !sd_bus_message_has_signature(message,"s") || sd_bus_message_read(message,"s",&state)<=0)
        return AE_SETTINGS_WLAN_UNAVAILABLE;
    if(!strcmp(state,"off")) return AE_SETTINGS_WLAN_OFF;
    if(!strcmp(state,"on")) return AE_SETTINGS_WLAN_ON;
    if(!strcmp(state,"starting")) return AE_SETTINGS_WLAN_STARTING;
    if(!strcmp(state,"stopping")) return AE_SETTINGS_WLAN_STOPPING;
    if(!strcmp(state,"failed")) return AE_SETTINGS_WLAN_FAILED;
    return AE_SETTINGS_WLAN_UNAVAILABLE;
}

static bool read_networks(sd_bus_message *m,struct ae_wlan_networks *out) {
    struct ae_wlan_networks snapshot={0};
    int scanning,more;
    if(sd_bus_message_is_method_error(m,NULL)!=0 ||
       !sd_bus_message_has_signature(m,AE_WLAN_NETWORK_REPLY) ||
       sd_bus_message_read(m,"bb",&scanning,&more)<=0 ||
       sd_bus_message_enter_container(m,'a',"(" AE_WLAN_NETWORK_FIELDS ")")<=0) return false;
    snapshot.scanning=scanning!=0;
    snapshot.more=more!=0;
    int result;
    while((result=sd_bus_message_enter_container(m,'r',AE_WLAN_NETWORK_FIELDS))>0) {
        const char *name=NULL;
        uint8_t security=0;
        int16_t signal=0;
        int connected=0;
        if(snapshot.count==AE_WLAN_NETWORK_LIMIT ||
           sd_bus_message_read(m,AE_WLAN_NETWORK_FIELDS,&name,&security,&signal,&connected)<=0 ||
           sd_bus_message_exit_container(m)<0 || security>AE_WLAN_ENTERPRISE ||
           signal>0 || signal<-10000) return false;
        size_t length=strnlen(name,AE_WLAN_NAME_CAPACITY);
        if(!length || length==AE_WLAN_NAME_CAPACITY) return false;
        struct ae_wlan_network *row=&snapshot.rows[snapshot.count++];
        memcpy(row->name,name,length+1);
        row->security=(enum ae_wlan_security)security;
        row->signal=signal;
        row->connected=connected!=0;
    }
    if(result<0 || sd_bus_message_exit_container(m)<0 ||
       (snapshot.scanning && (snapshot.more || snapshot.count))) return false;
    *out=snapshot;
    return true;
}

static int reply(sd_bus_message *message,void *data,sd_bus_error *error) {
    (void)error;
    struct ae_settings_wlan_client *client=data;
    client->reply_ready=true;
    switch(client->sent) {
        case AE_SETTINGS_WLAN_READ:
            client->reply_state=read_state(message);
            break;
        case AE_SETTINGS_WLAN_SCAN:
            client->reply_valid=sd_bus_message_is_method_error(message,NULL)==0 &&
                sd_bus_message_has_signature(message,"");
            break;
        case AE_SETTINGS_WLAN_LIST:
            client->reply_valid=read_networks(message,&client->reply_networks);
            break;
        default:
            client->refused=sd_bus_message_is_method_error(message,"org.aetheri.Error1.NotAuthorized")>0;
            break;
    }
    return 1;
}

static bool submit(struct ae_settings_wlan_client *client,enum ae_settings_wlan_request request) {
    int result;
    if(request==AE_SETTINGS_WLAN_ENABLE || request==AE_SETTINGS_WLAN_DISABLE)
        result=sd_bus_call_method_async(client->bus,NULL,WLAN_BUS,WLAN_PATH,WLAN_BUS,"SetEnabled",reply,client,
                                      "b",request==AE_SETTINGS_WLAN_ENABLE);
    else {
        const char *member=request==AE_SETTINGS_WLAN_SCAN?"StartScan":
            request==AE_SETTINGS_WLAN_LIST?"GetNetworks":"GetState";
        result=sd_bus_call_method_async(client->bus,NULL,WLAN_BUS,WLAN_PATH,WLAN_BUS,member,reply,client,"");
    }
    if(result<0) return false;
    client->sent=request;
    client->next_read=0;
    return true;
}

static bool start(struct ae_settings_wlan_client *client,enum ae_settings_wlan_request request,uint64_t now) {
    int result=sd_bus_new(&client->bus);
    if(result>=0) result=sd_bus_set_address(client->bus,BUS_SOCKET);
    if(result>=0) result=sd_bus_set_bus_client(client->bus,1);
    if(result>=0) result=sd_bus_set_method_call_timeout(client->bus,CALL_USEC);
    if(result>=0) result=sd_bus_start(client->bus);
    if(result<0) return false;
    client->wait_fd=epoll_create1(EPOLL_CLOEXEC);
    if(client->wait_fd<0 || !submit(client,request)) return false;
    client->target=request==AE_SETTINGS_WLAN_ENABLE?AE_SETTINGS_WLAN_ON:AE_SETTINGS_WLAN_OFF;
    client->deadline=now+CHANGE_WINDOW;
    return true;
}

static bool watch(int wait_fd,int fd,uint32_t mask) {
    struct epoll_event event={.events=mask};
    if(epoll_ctl(wait_fd,EPOLL_CTL_ADD,fd,&event)==0) return true;
    return errno==EEXIST && epoll_ctl(wait_fd,EPOLL_CTL_MOD,fd,&event)==0;
}

static void schedule(struct ae_settings_wlan_client *client,enum ae_settings_wlan_request request,uint64_t when) {
    client->next=request;
    client->next_read=when;
}

static void limit_timeout(uint32_t *timeout,uint64_t now,uint64_t when) {
    uint64_t usec=when>now?when-now:0;
    uint64_t millis=usec/1000+(usec%1000!=0);
    if(millis<*timeout) *timeout=(uint32_t)millis;
}

int ae_settings_wlan_prepare(struct ae_settings_wlan_client *client,int parent_fd,uint32_t *timeout,int *dirty) {
    struct ae_settings_wlan *model=client->model;
    if(!client->bus && model->request==AE_SETTINGS_WLAN_NONE) return parent_fd;
    uint64_t now=now_usec();
    if(!client->bus) {
        enum ae_settings_wlan_request request=model->request;
        model->request=AE_SETTINGS_WLAN_NONE;
        if(!now || !start(client,request,now)) goto failed;
    }
    if(!now || now>=client->deadline) goto failed;
    if(client->next_read && client->next_read<=now && !submit(client,client->next)) goto failed;
    if(client->parent_fd!=parent_fd) {
        if(client->parent_fd>=0) (void)epoll_ctl(client->wait_fd,EPOLL_CTL_DEL,client->parent_fd,NULL);
        if(!watch(client->wait_fd,parent_fd,EPOLLIN)) goto failed;
        client->parent_fd=parent_fd;
    }
    int fd=sd_bus_get_fd(client->bus);
    int events=sd_bus_get_events(client->bus);
    uint64_t bus_timeout;
    if(fd<0 || events<0 || sd_bus_get_timeout(client->bus,&bus_timeout)<0) goto failed;
    uint32_t mask=0;
    if(events&POLLIN) mask|=EPOLLIN;
    if(events&POLLOUT) mask|=EPOLLOUT;
    if(!watch(client->wait_fd,fd,mask)) goto failed;
    if(client->drain) *timeout=0;
    limit_timeout(timeout,now,bus_timeout);
    limit_timeout(timeout,now,client->deadline);
    if(client->next_read) limit_timeout(timeout,now,client->next_read);
    return client->wait_fd;
failed:
    lost(client);
    *dirty=1;
    *timeout=0;
    return parent_fd;
}

static bool scan_result(struct ae_settings_wlan_client *client,enum ae_settings_wlan_request sent,uint64_t now) {
    struct ae_settings_wlan *model=client->model;
    if(!client->reply_valid) {
        lost(client);
        return true;
    }
    if(sent==AE_SETTINGS_WLAN_SCAN) {
        schedule(client,AE_SETTINGS_WLAN_LIST,now);
        return false;
    }
    if(client->reply_networks.scanning) {
        schedule(client,AE_SETTINGS_WLAN_LIST,now+READ_INTERVAL);
        return false;
    }
    model->scan=AE_SETTINGS_SCAN_READY;
    model->networks=client->reply_networks;
    finish(client,model->state,false);
    return true;
}

bool ae_settings_wlan_process(struct ae_settings_wlan_client *client) {
    if(!client->bus) return false;
    struct epoll_event events[2];
    int ready;
    do {ready=epoll_wait(client->wait_fd,events,2,0);} while(ready<0 && errno==EINTR);
    if(ready<0) {lost(client);return true;}
    int result=0;
    for(unsigned i=0;i<12 && !client->reply_ready;++i) {
        result=sd_bus_process(client->bus,NULL);
        if(result<=0) break;
    }
    client->drain=result>0 && !client->reply_ready;
    if(result<0) {lost(client);return true;}
    if(!client->reply_ready) return false;

    uint64_t now=now_usec();
    if(!now) {lost(client);return true;}
    enum ae_settings_wlan_request sent=client->sent;
    client->sent=AE_SETTINGS_WLAN_NONE;
    client->reply_ready=false;
    if(sent==AE_SETTINGS_WLAN_SCAN || sent==AE_SETTINGS_WLAN_LIST) return scan_result(client,sent,now);
    if(sent!=AE_SETTINGS_WLAN_READ) {
        if(client->refused) {finish(client,AE_SETTINGS_WLAN_UNAVAILABLE,false);return true;}
        /* A job reply, timeout or malformed reply all need fresh readback.
         * This path only schedules GetState and never repeats SetEnabled. */
        schedule(client,AE_SETTINGS_WLAN_READ,now);
        return false;
    }
    struct ae_settings_wlan *model=client->model;
    enum ae_settings_wlan_state state=client->reply_state;
    if(state==AE_SETTINGS_WLAN_UNAVAILABLE) {lost(client);return true;}
    bool moving=state==AE_SETTINGS_WLAN_STARTING || state==AE_SETTINGS_WLAN_STOPPING;
    if(state==AE_SETTINGS_WLAN_FAILED || (!moving && (!model->changing || state==client->target))) {
        finish(client,state,false);
        return true;
    }
    bool changed=model->state!=state;
    model->state=state;
    schedule(client,AE_SETTINGS_WLAN_READ,now+READ_INTERVAL);
    return changed;
}
