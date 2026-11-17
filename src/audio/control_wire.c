#define _GNU_SOURCE
#include "audio/control_wire.h"
#include "text/grapheme.h"
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

enum { HEADER=16, SNAPSHOT=48, OUTPUT=800, COMMAND=40,
       WIRE_MAX=HEADER+SNAPSHOT+AE_AUDIO_OUTPUTS_MAX*OUTPUT };
_Static_assert(sizeof(double)==8 && FLT_RADIX==2 && DBL_MANT_DIG==53 &&
               DBL_MAX_EXP==1024,"audio wire binary64 profile");
_Static_assert(AE_AUDIO_CONNECTING==0 && AE_AUDIO_DISCONNECTED==3 &&
               AE_AUDIO_ERROR_ENDPOINT==11 && AE_AUDIO_COMMAND_UNKNOWN==3 &&
               AE_AUDIO_DEFAULT==2 && AE_AUDIO_BUSY==4,"audio wire v1 values");

bool ae_audio_wire_admit_parent(int fd)
{
    pid_t parent=getppid();
    if(fd<3 || parent<=1 || getuid()!=geteuid() || getgid()!=getegid()) return false;
    int type;
    socklen_t size=sizeof type;
    if(getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&size) || size!=sizeof type ||
       type!=SOCK_SEQPACKET) return false;
    struct sockaddr_un name;
    size=sizeof name;
    if(getsockname(fd,(struct sockaddr *)&name,&size) || size!=sizeof name.sun_family ||
       name.sun_family!=AF_UNIX) return false;
    size=sizeof name;
    if(getpeername(fd,(struct sockaddr *)&name,&size) || size!=sizeof name.sun_family ||
       name.sun_family!=AF_UNIX) return false;
    struct ucred peer;
    size=sizeof peer;
    return !getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&size) && size==sizeof peer &&
        peer.pid==parent && peer.uid==getuid() && peer.gid==getgid() &&
        getppid()==parent && fcntl(fd,F_SETFD,FD_CLOEXEC)==0;
}

static void put(uint8_t *dst,size_t size,uint64_t value)
{
    while(size) { dst[--size]=(uint8_t)value; value>>=8; }
}

static uint64_t get(const uint8_t *src,size_t size)
{
    uint64_t value=0;
    for(size_t i=0;i<size;++i) value=(value<<8)|src[i];
    return value;
}

static void put_volume(uint8_t *dst,double value)
{
    uint64_t bits;
    memcpy(&bits,&value,sizeof bits);
    put(dst,8,bits);
}

static double get_volume(const uint8_t *src)
{
    uint64_t bits=get(src,8);
    double value;
    memcpy(&value,&bits,sizeof value);
    return value;
}

static void put_ref(uint8_t *dst,struct ae_audio_output_ref ref)
{
    put(dst,8,ref.connection);
    put(dst+8,8,ref.serial);
    put(dst+16,4,ref.id);
}

static struct ae_audio_output_ref get_ref(const uint8_t *src)
{
    return (struct ae_audio_output_ref){get(src,8),get(src+8,8),(uint32_t)get(src+16,4)};
}

static bool valid_ref(struct ae_audio_output_ref ref)
{
    return ref.connection && ref.serial && ref.id;
}

static bool zero_ref(struct ae_audio_output_ref ref)
{
    return !ref.connection && !ref.serial && !ref.id;
}

static bool valid_number(double value,double max)
{
    return isfinite(value) && value>=0 && value<=max;
}

static bool valid_command(const struct ae_audio_command *c)
{
    if(!c->revision || !valid_ref(c->target)) return false;
    switch(c->kind) {
    case AE_AUDIO_VOLUME: return !c->mute && valid_number(c->volume,1);
    case AE_AUDIO_MUTE: return c->volume==0;
    case AE_AUDIO_DEFAULT: return !c->mute && c->volume==0;
    }
    return false;
}

static bool text_length(const char *text,size_t capacity,bool required,size_t *length)
{
    const char *end=memchr(text,'\0',capacity);
    if(!end || (required && end==text)) return false;
    size_t bytes=(size_t)(end-text),offset=0;
    uint32_t scalar;
    while(offset<bytes)
        if(!ae_utf8_next(text,bytes,&offset,&scalar) || scalar<32 || scalar==127) return false;
    *length=bytes;
    return true;
}

static bool get_text(char *dst,const uint8_t *src,size_t capacity,bool required)
{
    size_t length;
    if(!text_length((const char *)src,capacity,required,&length)) return false;
    for(size_t i=length;i<capacity;++i) if(src[i]) return false;
    memcpy(dst,src,length);
    return true;
}

static bool valid_snapshot(const struct ae_audio_session_snapshot *s)
{
    const struct ae_audio_snapshot *a=&s->audio;
    unsigned known_loss=AE_AUDIO_CONTROL_MANAGER_LOST|AE_AUDIO_CONTROL_GRAPH_LOST|
        AE_AUDIO_CONTROL_POLICY_LOST|AE_AUDIO_CONTROL_SESSION_LOST|AE_AUDIO_CONTROL_BACKEND_LOST;
    if((s->loss&~known_loss) || a->count>AE_AUDIO_OUTPUTS_MAX ||
       a->state<AE_AUDIO_CONNECTING || a->state>AE_AUDIO_DISCONNECTED ||
       a->error<AE_AUDIO_ERROR_NONE || a->error>AE_AUDIO_ERROR_ENDPOINT ||
       a->command_state<AE_AUDIO_COMMAND_NONE || a->command_state>AE_AUDIO_COMMAND_UNKNOWN ||
       a->command_kind<AE_AUDIO_VOLUME || a->command_kind>AE_AUDIO_DEFAULT ||
       (a->count && a->state!=AE_AUDIO_READY) ||
       (s->loss && a->state!=AE_AUDIO_DISCONNECTED)) return false;
    if(a->command_state==AE_AUDIO_COMMAND_NONE)
        return !a->command && zero_ref(a->command_target) && a->command_kind==AE_AUDIO_VOLUME;
    return a->command && valid_ref(a->command_target);
}

static bool encode_snapshot(uint8_t *dst,const struct ae_audio_session_snapshot *s)
{
    const struct ae_audio_snapshot *a=&s->audio;
    if(!valid_snapshot(s)) return false;
    put(dst,8,a->revision);
    put(dst+8,8,a->command);
    put_ref(dst+16,a->command_target);
    put(dst+36,4,s->loss);
    put(dst+40,2,a->count);
    dst[42]=(uint8_t)a->state;
    dst[43]=(uint8_t)a->error;
    dst[44]=(uint8_t)a->command_state;
    dst[45]=(uint8_t)a->command_kind;
    dst+=SNAPSHOT;
    for(size_t i=0;i<a->count;++i,dst+=OUTPUT) {
        const struct ae_audio_output *o=&a->outputs[i];
        size_t name,description;
        if(!valid_ref(o->ref) || !valid_number(o->volume,16) ||
           !text_length(o->name,sizeof o->name,true,&name) ||
           !text_length(o->description,sizeof o->description,false,&description)) return false;
        put_ref(dst,o->ref);
        dst[20]=(uint8_t)((o->is_default?1u:0u)|(o->volume_available?2u:0u)|(o->muted?4u:0u));
        put_volume(dst+24,o->volume);
        memcpy(dst+32,o->name,name);
        memcpy(dst+288,o->description,description);
    }
    return true;
}

static bool decode_snapshot(struct ae_audio_session_snapshot *s,const uint8_t *src,size_t size)
{
    if(size<SNAPSHOT || src[46] || src[47]) return false;
    struct ae_audio_snapshot *a=&s->audio;
    a->count=(size_t)get(src+40,2);
    if(a->count>AE_AUDIO_OUTPUTS_MAX || size!=SNAPSHOT+a->count*OUTPUT) return false;
    a->revision=get(src,8);
    a->command=get(src+8,8);
    a->command_target=get_ref(src+16);
    s->loss=(unsigned)get(src+36,4);
    a->state=(enum ae_audio_state)src[42];
    a->error=(enum ae_audio_error)src[43];
    a->command_state=(enum ae_audio_command_state)src[44];
    a->command_kind=(enum ae_audio_command_kind)src[45];
    if(!valid_snapshot(s)) return false;
    src+=SNAPSHOT;
    for(size_t i=0;i<a->count;++i,src+=OUTPUT) {
        struct ae_audio_output *o=&a->outputs[i];
        o->ref=get_ref(src);
        uint8_t flags=src[20];
        o->volume=get_volume(src+24);
        if(!valid_ref(o->ref) || (flags&0xf8u) || src[21] || src[22] || src[23] ||
           !valid_number(o->volume,16) ||
           !get_text(o->name,src+32,sizeof o->name,true) ||
           !get_text(o->description,src+288,sizeof o->description,false)) return false;
        o->is_default=(flags&1u)!=0;
        o->volume_available=(flags&2u)!=0;
        o->muted=(flags&4u)!=0;
    }
    return true;
}

int ae_audio_wire_send(int fd,const struct ae_audio_message *m)
{
    if(fd<0 || !m) return -EINVAL;
    uint8_t packet[WIRE_MAX];
    memcpy(packet,"ASND\1",5);
    packet[5]=(uint8_t)m->kind;
    packet[6]=packet[7]=0;
    put(packet+8,8,m->request);
    uint8_t *body=packet+HEADER;
    size_t size;
    switch(m->kind) {
    case AE_AUDIO_MESSAGE_SNAPSHOT:
        if(m->request || m->snapshot.audio.count>AE_AUDIO_OUTPUTS_MAX) return -EINVAL;
        size=SNAPSHOT+m->snapshot.audio.count*OUTPUT;
        memset(body,0,size);
        if(!encode_snapshot(body,&m->snapshot)) return -EINVAL;
        break;
    case AE_AUDIO_MESSAGE_COMMAND:
        if(!m->request || !valid_command(&m->command)) return -EINVAL;
        size=COMMAND;
        put(body,8,m->command.revision);
        put_ref(body+8,m->command.target);
        body[28]=(uint8_t)m->command.kind;
        body[29]=(uint8_t)m->command.mute;
        body[30]=body[31]=0;
        put_volume(body+32,m->command.volume);
        break;
    case AE_AUDIO_MESSAGE_RESULT:
        if(!m->request || m->result<AE_AUDIO_SUBMITTED || m->result>AE_AUDIO_BUSY) return -EINVAL;
        size=1;
        body[0]=(uint8_t)m->result;
        break;
    default: return -EINVAL;
    }
    ssize_t sent;
    do { sent=send(fd,packet,HEADER+size,MSG_DONTWAIT|MSG_NOSIGNAL); } while(sent<0 && errno==EINTR);
    return sent<0 ? -errno : (size_t)sent==HEADER+size ? 0 : -EPROTO;
}

int ae_audio_wire_receive(int fd,struct ae_audio_message *out)
{
    if(fd<0 || !out) return -EINVAL;
    uint8_t packet[WIRE_MAX];
    struct iovec body={.iov_base=packet,.iov_len=sizeof packet};
    /* Linux discards excess ancillary FDs when the receive buffer has no
     * control space. Reject the reported truncation, even on valid bytes. */
    struct msghdr message={.msg_iov=&body,.msg_iovlen=1};
    ssize_t received;
    do { received=recvmsg(fd,&message,MSG_DONTWAIT|MSG_CMSG_CLOEXEC); } while(received<0 && errno==EINTR);
    if(received<0) return -errno;
    if(!received) return -ECONNRESET;
    if(received<HEADER || (message.msg_flags&(MSG_TRUNC|MSG_CTRUNC)) ||
       memcmp(packet,"ASND\1",5) || packet[6] || packet[7]) return -EPROTO;
    struct ae_audio_message decoded={.kind=(enum ae_audio_message_kind)packet[5],.request=get(packet+8,8)};
    const uint8_t *src=packet+HEADER;
    size_t size=(size_t)received-HEADER;
    switch(decoded.kind) {
    case AE_AUDIO_MESSAGE_SNAPSHOT:
        if(decoded.request || !decode_snapshot(&decoded.snapshot,src,size)) return -EPROTO;
        break;
    case AE_AUDIO_MESSAGE_COMMAND:
        if(!decoded.request || size!=COMMAND || src[29]>1 || src[30] || src[31]) return -EPROTO;
        decoded.command=(struct ae_audio_command){.revision=get(src,8),.target=get_ref(src+8),
            .kind=(enum ae_audio_command_kind)src[28],.mute=src[29]!=0,.volume=get_volume(src+32)};
        if(!valid_command(&decoded.command)) return -EPROTO;
        break;
    case AE_AUDIO_MESSAGE_RESULT:
        if(!decoded.request || size!=1 || src[0]>AE_AUDIO_BUSY) return -EPROTO;
        decoded.result=(enum ae_audio_submit)src[0];
        break;
    default: return -EPROTO;
    }
    *out=decoded;
    return 0;
}

int ae_audio_wire_dispatch(int fd,struct ae_audio_wire_requests *state,
                           ae_audio_wire_submit submit,void *data)
{
    if(fd<0 || !state || !submit) return -EINVAL;
    if(state->retired) return -ESHUTDOWN;
    struct ae_audio_message message;
    int r=ae_audio_wire_receive(fd,&message);
    if(r==-EAGAIN) return 0;
    if(!r && (message.kind!=AE_AUDIO_MESSAGE_COMMAND || message.request<=state->last)) r=-EPROTO;
    if(!r) {
        state->last=message.request;
        struct ae_audio_message response={.kind=AE_AUDIO_MESSAGE_RESULT,.request=message.request,
            .result=submit(data,&message.command)};
        r=ae_audio_wire_send(fd,&response);
        if(r==-EAGAIN) r=-ENOBUFS;
    }
    if(r<0) {
        state->retired=true;
        return r;
    }
    return 1;
}
