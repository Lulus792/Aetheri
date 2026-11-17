#define _GNU_SOURCE
#include "settings/sound_channel.h"
#include "settings/model.h"
#include "audio/control_wire.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

bool ae_settings_sound_channel_admit(int fd)
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

void ae_settings_sound_channel_init(struct ae_settings_sound_channel *channel,int fd)
{
    *channel=(struct ae_settings_sound_channel){.fd=fd};
}

static int retire(struct ae_settings_sound_channel *channel,struct ae_settings *model,int result)
{
    channel->retired=true;
    channel->pending=0;
    (void)ae_settings_sound_snapshot(model,NULL,AE_AUDIO_CONTROL_BACKEND_LOST);
    ae_settings_sound_result(model,AE_AUDIO_UNAVAILABLE);
    return result;
}

int ae_settings_sound_channel_receive(struct ae_settings_sound_channel *channel,struct ae_settings *model)
{
    if(!channel || !model) return -EINVAL;
    if(channel->retired) return -ESHUTDOWN;
    struct ae_audio_message message;
    int result=ae_audio_wire_receive(channel->fd,&message);
    if(result==-EAGAIN) return 0;
    if(result<0) return retire(channel,model,result);
    if(message.kind==AE_AUDIO_MESSAGE_SNAPSHOT) {
        if(!ae_settings_sound_snapshot(model,&message.snapshot.audio,message.snapshot.loss))
            return retire(channel,model,-EPROTO);
        if(model->sound.fenced) return retire(channel,model,1);
        return 1;
    }
    if(message.kind!=AE_AUDIO_MESSAGE_RESULT || !channel->pending ||
       message.request!=channel->pending || !model->sound.dispatching)
        return retire(channel,model,-EPROTO);
    channel->pending=0;
    ae_settings_sound_result(model,message.result);
    return 1;
}

int ae_settings_sound_channel_send(struct ae_settings_sound_channel *channel,struct ae_settings *model)
{
    if(!channel || !model) return -EINVAL;
    if(channel->retired) return -ESHUTDOWN;
    struct ae_settings_sound_request intent;
    if(channel->pending || !ae_settings_sound_take(model,&intent)) return 0;
    if(channel->request==UINT64_MAX) return retire(channel,model,-EOVERFLOW);

    struct ae_audio_message message={.kind=AE_AUDIO_MESSAGE_COMMAND,.request=++channel->request,
        .command={.revision=intent.revision,.target=intent.target,.kind=intent.kind,
            .volume=intent.kind==AE_AUDIO_VOLUME?intent.volume:0,
            .mute=intent.kind==AE_AUDIO_MUTE && intent.mute}};
    int result=ae_audio_wire_send(channel->fd,&message);
    if(result==-EAGAIN) {
        ae_settings_sound_result(model,AE_AUDIO_BUSY);
        return 0;
    }
    if(result<0) return retire(channel,model,result);
    channel->pending=message.request;
    return 1;
}
