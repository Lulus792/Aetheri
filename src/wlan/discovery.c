#define _POSIX_C_SOURCE 200809L
#include "wlan/discovery.h"
#include "text/grapheme.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define IWD_STATION AE_IWD_BUS ".Station"
#define IWD_NETWORK AE_IWD_BUS ".Network"
#define IWD_ROOT "/net/connman/iwd/"
#define STATION_LIMIT 8
#define OBJECT_LIMIT 2048
#define BACKEND_USEC UINT64_C(2500000)

struct station {
    const char *path;
    bool scanning;
};

struct network {
    const char *path, *name;
    enum ae_wlan_security security;
    bool connected;
};

struct inventory {
    size_t station_count, network_count;
    struct station stations[STATION_LIMIT];
    struct network networks[OBJECT_LIMIT];
};

static uint64_t now_usec(void) {
    struct timespec time;
    if(clock_gettime(CLOCK_MONOTONIC,&time)<0) return 0;
    return (uint64_t)time.tv_sec*UINT64_C(1000000)+(uint64_t)time.tv_nsec/1000;
}

static int call(sd_bus *bus,const struct ae_peer *iwd,uint64_t deadline,const char *path,
                const char *interface,const char *method,sd_bus_message **reply) {
    uint64_t now=now_usec();
    if(!now || now>=deadline) return -ETIMEDOUT;
    if(ae_peer_alive(iwd)<0) return -ESTALE;
    sd_bus_message *request=NULL;
    int result=sd_bus_message_new_method_call(bus,&request,iwd->unique_name,path,interface,method);
    if(result>=0) result=sd_bus_call(bus,request,deadline-now,NULL,reply);
    sd_bus_message_unref(request);
    if(result<0) return result;
    const char *sender=sd_bus_message_get_sender(*reply);
    if(!sender || strcmp(sender,iwd->unique_name)) return -EBADMSG;
    return ae_peer_alive(iwd);
}

static enum ae_wlan_security security(const char *type) {
    if(!strcmp(type,"open")) return AE_WLAN_OPEN;
    if(!strcmp(type,"psk")) return AE_WLAN_PERSONAL;
    if(!strcmp(type,"8021x")) return AE_WLAN_ENTERPRISE;
    return AE_WLAN_UNKNOWN;
}

/* Copy a UI label. SSIDs are not trusted prose, even when they are UTF-8. */
static bool display_name(const char *input,char output[AE_WLAN_NAME_CAPACITY]) {
    size_t length=strnlen(input,AE_WLAN_NAME_CAPACITY),offset=0,written=0;
    if(!length || length==AE_WLAN_NAME_CAPACITY) return false;
    while(offset<length) {
        size_t start=offset;
        uint32_t scalar;
        if(!ae_utf8_next(input,length,&offset,&scalar)) return false;
        bool control=scalar<32 || (scalar>=127 && scalar<=159) || scalar==0x061c ||
            scalar==0x200e || scalar==0x200f || (scalar>=0x2028 && scalar<=0x202e) ||
            (scalar>=0x2066 && scalar<=0x2069);
        if(control) {
            if(written+3>=AE_WLAN_NAME_CAPACITY) return false;
            memcpy(output+written,"\xef\xbf\xbd",3);
            written+=3;
        } else {
            size_t bytes=offset-start;
            if(written+bytes>=AE_WLAN_NAME_CAPACITY) return false;
            memcpy(output+written,input+start,bytes);
            written+=bytes;
        }
    }
    output[written]='\0';
    return true;
}

static bool read_properties(sd_bus_message *m,struct station *station,struct network *network) {
    int result,flag=0;
    const char *name=NULL,*value=NULL;
    bool scanning_known=false,connected_known=false,type_known=false;
    if(sd_bus_message_enter_container(m,'a',"{sv}")<=0) return false;
    while((result=sd_bus_message_enter_container(m,'e',"sv"))>0) {
        if(sd_bus_message_read(m,"s",&name)<=0) return false;
        if(station && !strcmp(name,"Scanning")) {
            if(scanning_known || sd_bus_message_read(m,"v","b",&flag)<=0) return false;
            scanning_known=true;
            station->scanning=flag!=0;
        } else if(network && !strcmp(name,"Name")) {
            if(network->name || sd_bus_message_read(m,"v","s",&network->name)<=0) return false;
        } else if(network && !strcmp(name,"Type")) {
            if(type_known || sd_bus_message_read(m,"v","s",&value)<=0) return false;
            type_known=true;
            network->security=security(value);
        } else if(network && !strcmp(name,"Connected")) {
            if(connected_known || sd_bus_message_read(m,"v","b",&flag)<=0) return false;
            connected_known=true;
            network->connected=flag!=0;
        } else if(sd_bus_message_skip(m,"v")<0) return false;
        if(sd_bus_message_exit_container(m)<0) return false;
    }
    return result==0 && sd_bus_message_exit_container(m)>=0 &&
        (!station || scanning_known) && (!network || (network->name && type_known && connected_known));
}

static bool read_inventory(sd_bus_message *m,struct inventory *inventory,bool start_scan) {
    if(!sd_bus_message_has_signature(m,"a{oa{sa{sv}}}") ||
       sd_bus_message_enter_container(m,'a',"{oa{sa{sv}}}")<=0) return false;
    unsigned objects=0;
    int entry;
    while((entry=sd_bus_message_enter_container(m,'e',"oa{sa{sv}}"))>0) {
        const char *path=NULL;
        if(++objects>OBJECT_LIMIT || sd_bus_message_read(m,"o",&path)<=0 ||
           sd_bus_message_enter_container(m,'a',"{sa{sv}}")<=0) return false;
        int interface;
        while((interface=sd_bus_message_enter_container(m,'e',"sa{sv}"))>0) {
            const char *name=NULL;
            if(sd_bus_message_read(m,"s",&name)<=0) return false;
            struct station *station=NULL;
            struct network *network=NULL;
            if(!strcmp(name,IWD_STATION)) {
                if(inventory->station_count==STATION_LIMIT) return false;
                station=&inventory->stations[inventory->station_count++];
                station->path=path;
            } else if(!start_scan && !strcmp(name,IWD_NETWORK)) {
                if(inventory->network_count==OBJECT_LIMIT) return false;
                network=&inventory->networks[inventory->network_count++];
                network->path=path;
            }
            if(station || network) {
                if(strncmp(path,IWD_ROOT,sizeof IWD_ROOT-1) || strnlen(path,256)==256 ||
                   !read_properties(m,station,network)) return false;
            } else if(sd_bus_message_skip(m,"a{sv}")<0) return false;
            if(sd_bus_message_exit_container(m)<0) return false;
        }
        if(interface<0 || sd_bus_message_exit_container(m)<0 || sd_bus_message_exit_container(m)<0)
            return false;
    }
    return entry==0 && sd_bus_message_exit_container(m)>=0;
}

static int scan_radios(sd_bus *bus,const struct ae_peer *iwd,uint64_t deadline,
                       const struct inventory *inventory,bool (*authorized)(void *),void *caller) {
    bool accepted=false;
    for(size_t i=0;i<inventory->station_count;++i) {
        const struct station *station=&inventory->stations[i];
        if(!authorized(caller)) return -EPERM;
        if(station->scanning) {
            accepted=true;
            continue;
        }
        sd_bus_message *response=NULL;
        int result=call(bus,iwd,deadline,station->path,IWD_STATION,"Scan",&response);
        if(result>=0 && sd_bus_message_has_signature(response,"")) accepted=true;
        sd_bus_message_unref(response);
    }
    return accepted?0:-EIO;
}

static void add_network(struct ae_wlan_networks *out,const struct network *network,int16_t signal) {
    struct ae_wlan_network row={.security=network->security,.signal=signal,.connected=network->connected};
    if(signal>0 || signal<-10000 || !display_name(network->name,row.name)) return;
    for(size_t i=0;i<out->count;++i) {
        struct ae_wlan_network *old=&out->rows[i];
        if(old->security==row.security && !strcmp(old->name,row.name)) {
            if(row.signal>old->signal) old->signal=row.signal;
            old->connected=old->connected || row.connected;
            return;
        }
    }
    if(out->count==AE_WLAN_NETWORK_LIMIT) {out->more=true;return;}
    out->rows[out->count++]=row;
}

static int ordered_networks(sd_bus_message *m,const struct inventory *inventory,struct ae_wlan_networks *out) {
    if(!sd_bus_message_has_signature(m,"a(on)") || sd_bus_message_enter_container(m,'a',"(on)")<=0)
        return -EBADMSG;
    unsigned count=0;
    int result;
    while((result=sd_bus_message_enter_container(m,'r',"on"))>0) {
        const char *path=NULL;
        int16_t signal=0;
        if(++count>OBJECT_LIMIT || sd_bus_message_read(m,"on",&path,&signal)<=0 ||
           sd_bus_message_exit_container(m)<0) return -EBADMSG;
        for(size_t i=0;i<inventory->network_count;++i) {
            if(strcmp(path,inventory->networks[i].path)) continue;
            add_network(out,&inventory->networks[i],signal);
            break;
        }
    }
    if(result<0 || sd_bus_message_exit_container(m)<0) return -EBADMSG;
    return 0;
}

int ae_wlan_discover(sd_bus *bus,const struct ae_peer *iwd,struct ae_wlan_networks *out,
                     bool start_scan,bool (*authorized)(void *),void *caller) {
    uint64_t now=now_usec();
    if(!now) return -EIO;
    uint64_t deadline=now+BACKEND_USEC;
    struct inventory *inventory=calloc(1,sizeof *inventory);
    if(!inventory) return -ENOMEM;
    sd_bus_message *objects=NULL;
    struct ae_wlan_networks snapshot={0};
    int result=call(bus,iwd,deadline,"/","org.freedesktop.DBus.ObjectManager","GetManagedObjects",&objects);
    if(result>=0 && !read_inventory(objects,inventory,start_scan)) result=-EBADMSG;
    if(result>=0 && !inventory->station_count) result=-ENODEV;
    if(result<0) goto done;
    if(start_scan) {
        result=scan_radios(bus,iwd,deadline,inventory,authorized,caller);
        goto done;
    }
    for(size_t i=0;i<inventory->station_count;++i) snapshot.scanning|=inventory->stations[i].scanning;
    if(!snapshot.scanning) {
        for(size_t i=0;i<inventory->station_count;++i) {
            sd_bus_message *reply=NULL;
            result=call(bus,iwd,deadline,inventory->stations[i].path,IWD_STATION,"GetOrderedNetworks",&reply);
            if(result>=0) result=ordered_networks(reply,inventory,&snapshot);
            sd_bus_message_unref(reply);
            if(result<0) goto done;
        }
    }
    if(ae_peer_alive(iwd)<0) result=-ESTALE;
    if(result>=0) *out=snapshot;
done:
    sd_bus_message_unref(objects);
    free(inventory);
    return result;
}
