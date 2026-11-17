#define _GNU_SOURCE
#include "audio/control_wire.h"
#include "settings/model.h"
#include "settings/sound_channel.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(expr) do { if(!(expr)) { fprintf(stderr,"sound channel line %d: %s\n",__LINE__,#expr); exit(1); } } while(0)

static struct ae_audio_message available(void)
{
    struct ae_audio_message message={.kind=AE_AUDIO_MESSAGE_SNAPSHOT,
        .snapshot.audio={.revision=3,.state=AE_AUDIO_READY,.count=1}};
    message.snapshot.audio.outputs[0]=(struct ae_audio_output){
        .ref={1,27,4},.name="alsa_output.analog",.description="Line out",
        .is_default=true,.volume_available=true,.volume=0.45};
    return message;
}

static void launcher_admission(void)
{
    int sound[2],stream[2];
    CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,sound));
    CHECK(!socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,stream));
    CHECK(!ae_settings_sound_channel_admit(sound[0]));

    pid_t child=fork();
    CHECK(child>=0);
    if(!child) {
        close(sound[0]);
        close(stream[0]);
        CHECK(!ae_settings_sound_channel_admit(stream[1]));
        CHECK(!fcntl(sound[1],F_SETFD,0));
        CHECK(ae_settings_sound_channel_admit(sound[1]));
        CHECK(fcntl(sound[1],F_GETFD)==FD_CLOEXEC);
        _exit(0);
    }
    int status;
    CHECK(waitpid(child,&status,0)==child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status)==0);
    close(sound[0]);
    close(sound[1]);
    close(stream[0]);
    close(stream[1]);
}

struct submit_fixture {
    struct ae_audio_command command;
    unsigned calls;
    enum ae_audio_submit result;
};

static enum ae_audio_submit submit(void *data,const struct ae_audio_command *command)
{
    struct submit_fixture *fixture=data;
    fixture->command=*command;
    ++fixture->calls;
    return fixture->result;
}

static void round_trip(void)
{
    int fd[2];
    CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,fd));
    struct ae_settings model;
    ae_settings_init(&model);
    struct ae_settings_sound_channel view;
    ae_settings_sound_channel_init(&view,fd[0]);
    CHECK(ae_settings_sound_channel_receive(&view,&model)==0 && !model.sound.attached);

    struct ae_audio_message snapshot=available(),incoming;
    CHECK(!ae_audio_wire_send(fd[1],&snapshot));
    CHECK(ae_settings_sound_channel_receive(&view,&model)==1 && model.sound.attached);
    uint64_t id=model.sound.ids[0];
    CHECK(id && ae_settings_sound_request(&model,id,AE_AUDIO_VOLUME,0.75,false));
    CHECK(ae_settings_sound_channel_send(&view,&model)==1);
    CHECK(ae_settings_sound_channel_send(&view,&model)==0);
    struct ae_audio_wire_requests owner={0};
    struct submit_fixture effects={.result=AE_AUDIO_SUBMITTED};
    CHECK(ae_audio_wire_dispatch(fd[1],&owner,submit,&effects)==1 && owner.last==1);
    CHECK(effects.calls==1 && effects.command.revision==3 &&
          effects.command.target.serial==27 && effects.command.kind==AE_AUDIO_VOLUME &&
          effects.command.volume==0.75);
    CHECK(ae_audio_wire_dispatch(fd[1],&owner,submit,&effects)==0 && effects.calls==1);
    CHECK(model.sound.observation.outputs[0].volume==0.45 && !ae_settings_sound_enabled(&model));

    struct ae_audio_message result={.kind=AE_AUDIO_MESSAGE_RESULT,.request=1,.result=AE_AUDIO_SUBMITTED};
    CHECK(ae_settings_sound_channel_receive(&view,&model)==1 &&
          model.sound.feedback==AE_SETTINGS_SOUND_SUBMITTED && !ae_settings_sound_enabled(&model));
    CHECK(model.sound.observation.outputs[0].volume==0.45);
    snapshot.snapshot.audio.revision=4;
    snapshot.snapshot.audio.command=1;
    snapshot.snapshot.audio.command_state=AE_AUDIO_COMMAND_OBSERVED;
    snapshot.snapshot.audio.command_target=snapshot.snapshot.audio.outputs[0].ref;
    snapshot.snapshot.audio.outputs[0].volume=0.75;
    CHECK(!ae_audio_wire_send(fd[1],&snapshot));
    CHECK(ae_settings_sound_channel_receive(&view,&model)==1 &&
          model.sound.feedback==AE_SETTINGS_SOUND_OBSERVED && model.sound.ids[0]==id);
    CHECK(ae_settings_sound_request(&model,id,AE_AUDIO_MUTE,0,true));
    CHECK(ae_settings_sound_channel_send(&view,&model)==1);
    CHECK(!ae_audio_wire_receive(fd[1],&incoming) && incoming.request==2 &&
          incoming.command.kind==AE_AUDIO_MUTE && incoming.command.mute);
    result.request=1;
    CHECK(!ae_audio_wire_send(fd[1],&result));
    CHECK(ae_settings_sound_channel_receive(&view,&model)==-EPROTO && model.sound.fenced);
    CHECK(model.sound.feedback==AE_SETTINGS_SOUND_UNKNOWN);
    CHECK(ae_settings_sound_channel_send(&view,&model)==-ESHUTDOWN);
    close(fd[0]);
    close(fd[1]);
}

static void request_retirement(void)
{
    int fd[2];
    CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,fd));
    struct ae_audio_message command={.kind=AE_AUDIO_MESSAGE_COMMAND,.request=2,
        .command={.revision=3,.target={1,27,4},.kind=AE_AUDIO_MUTE,.mute=true}};
    struct ae_audio_wire_requests owner={0};
    struct submit_fixture effects={.result=AE_AUDIO_STALE};
    CHECK(!ae_audio_wire_send(fd[0],&command));
    CHECK(ae_audio_wire_dispatch(fd[1],&owner,submit,&effects)==1 && effects.calls==1);
    struct ae_audio_message result;
    CHECK(!ae_audio_wire_receive(fd[0],&result) && result.kind==AE_AUDIO_MESSAGE_RESULT &&
          result.request==2 && result.result==AE_AUDIO_STALE);
    CHECK(!ae_audio_wire_send(fd[0],&command));
    CHECK(ae_audio_wire_dispatch(fd[1],&owner,submit,&effects)==-EPROTO);
    CHECK(owner.retired && effects.calls==1);
    close(fd[0]);
    close(fd[1]);

    CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,fd));
    int limit=4096;
    CHECK(!setsockopt(fd[1],SOL_SOCKET,SO_SNDBUF,&limit,sizeof limit));
    unsigned queued=0;
    while(queued<128 && send(fd[1],"x",1,MSG_DONTWAIT)==1) ++queued;
    CHECK(queued && queued<128 && errno==EAGAIN);
    CHECK(!ae_audio_wire_send(fd[0],&command));
    owner=(struct ae_audio_wire_requests){0};
    effects=(struct submit_fixture){.result=AE_AUDIO_SUBMITTED};
    CHECK(ae_audio_wire_dispatch(fd[1],&owner,submit,&effects)==-ENOBUFS);
    CHECK(owner.retired && effects.calls==1);
    CHECK(ae_audio_wire_dispatch(fd[1],&owner,submit,&effects)==-ESHUTDOWN && effects.calls==1);
    close(fd[0]);
    close(fd[1]);
}

static void invalid_inputs(void)
{
    int fd[2];
    CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,fd));
    struct ae_audio_message message={.kind=AE_AUDIO_MESSAGE_COMMAND,.request=1,
        .command={.revision=3,.target={1,27,4},.kind=AE_AUDIO_VOLUME,.volume=NAN}};
    CHECK(ae_audio_wire_send(fd[0],&message)==-EINVAL);
    message.command.volume=1.01;
    CHECK(ae_audio_wire_send(fd[0],&message)==-EINVAL);
    message.command.volume=0.95;
    message.command.target.serial=0;
    CHECK(ae_audio_wire_send(fd[0],&message)==-EINVAL);
    message=available();
    message.snapshot.audio.outputs[0].description[0]=(char)0xff;
    CHECK(ae_audio_wire_send(fd[0],&message)==-EINVAL);
    message.snapshot.audio.count=AE_AUDIO_OUTPUTS_MAX+1;
    CHECK(ae_audio_wire_send(fd[0],&message)==-EINVAL);
    CHECK(ae_audio_wire_receive(fd[1],&message)==-EAGAIN);

    unsigned char bad[52000]={0};
    memcpy(bad,"ASND\2",5);
    CHECK(send(fd[0],bad,17,0)==17);
    CHECK(ae_audio_wire_receive(fd[1],&message)==-EPROTO &&
          message.snapshot.audio.count==AE_AUDIO_OUTPUTS_MAX+1);
    bad[4]=1;
    CHECK(send(fd[0],bad,sizeof bad,0)==(ssize_t)sizeof bad);
    CHECK(ae_audio_wire_receive(fd[1],&message)==-EPROTO);
    CHECK(send(fd[0],bad,15,0)==15);
    CHECK(ae_audio_wire_receive(fd[1],&message)==-EPROTO);
    close(fd[0]);
    close(fd[1]);
}

static void no_received_rights(void)
{
    int fd[2],pipefd[2];
    CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,fd));
    CHECK(!pipe2(pipefd,O_NONBLOCK|O_CLOEXEC));
    unsigned char bytes[17]="ASND\1\3";
    bytes[15]=1;
    struct iovec body={.iov_base=bytes,.iov_len=sizeof bytes};
    union {struct cmsghdr align; unsigned char bytes[CMSG_SPACE(sizeof(int))];} control={0};
    struct msghdr packet={.msg_iov=&body,.msg_iovlen=1,
        .msg_control=control.bytes,.msg_controllen=sizeof control.bytes};
    struct cmsghdr *header=CMSG_FIRSTHDR(&packet);
    header->cmsg_level=SOL_SOCKET;
    header->cmsg_type=SCM_RIGHTS;
    header->cmsg_len=CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(header),&pipefd[1],sizeof(int));
    CHECK(sendmsg(fd[0],&packet,0)==17);
    close(pipefd[1]);
    struct ae_audio_message message;
    CHECK(ae_audio_wire_receive(fd[1],&message)==-EPROTO);
    char byte;
    CHECK(read(pipefd[0],&byte,1)==0);
    close(pipefd[0]);
    close(fd[1]);
    close(fd[0]);
}

static void capacity_and_loss(void)
{
    int fd[2];
    CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,fd));
    struct ae_audio_message message=available();
    for(size_t i=1;i<AE_AUDIO_OUTPUTS_MAX;++i) {
        message.snapshot.audio.outputs[i]=message.snapshot.audio.outputs[0];
        message.snapshot.audio.outputs[i].ref.id=(uint32_t)(4+i);
        message.snapshot.audio.outputs[i].ref.serial=27+i;
        message.snapshot.audio.outputs[i].is_default=false;
    }
    message.snapshot.audio.count=AE_AUDIO_OUTPUTS_MAX;
    struct ae_audio_output *last=&message.snapshot.audio.outputs[AE_AUDIO_OUTPUTS_MAX-1];
    memset(last->description,'x',sizeof last->description-1);
    CHECK(!ae_audio_wire_send(fd[1],&message));
    struct ae_settings model;
    ae_settings_init(&model);
    struct ae_settings_sound_channel view;
    ae_settings_sound_channel_init(&view,fd[0]);
    CHECK(ae_settings_sound_channel_receive(&view,&model)==1);
    CHECK(model.sound.observation.count==AE_AUDIO_OUTPUTS_MAX &&
          !strcmp(model.sound.observation.outputs[AE_AUDIO_OUTPUTS_MAX-1].description,last->description));
    CHECK(ae_settings_sound_request(&model,model.sound.ids[0],AE_AUDIO_DEFAULT,0,false));
    CHECK(ae_settings_sound_channel_send(&view,&model)==1);
    close(fd[1]);
    CHECK(ae_settings_sound_channel_receive(&view,&model)==-ECONNRESET &&
          model.sound.feedback==AE_SETTINGS_SOUND_UNKNOWN && model.sound.fenced);
    close(fd[0]);
}

int main(void)
{
    launcher_admission();
    round_trip();
    request_retirement();
    invalid_inputs();
    no_received_rights();
    capacity_and_loss();
    puts("Sound channel: launcher admission, snapshots, ordered controls, malformed packets, ancillary closure and loss passed");
    return 0;
}
