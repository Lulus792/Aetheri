/* Fixed diskless QEMU HDA test owner. Never install on the carrier or laptop. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <linux/capability.h>
#include <sound/asound.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct child {pid_t pid;int pin;};
static struct child server={.pid=-1,.pin=-1},player={.pid=-1,.pin=-1};
#ifdef AETHERI_AUDIO_POLICY
static struct child policy={.pid=-1,.pin=-1},udev={.pid=-1,.pin=-1};
#endif
static const char remote[]="/run/aetheri-audio/pw-hda";
static char *const environment[]={"PATH=/usr/bin","HOME=/run/aetheri-audio","XDG_RUNTIME_DIR=/run/aetheri-audio",
    "PIPEWIRE_CONFIG_DIR=/etc/aetheri-audio","PIPEWIRE_MODULE_DIR=/usr/lib64/pipewire-0.3","SPA_PLUGIN_DIR=/usr/lib64/spa-0.2",
#ifdef AETHERI_AUDIO_POLICY
    "ALSA_CONFIG_PATH=/usr/share/alsa/alsa.conf","WIREPLUMBER_CONFIG_DIR=/etc/aetheri-wireplumber",
    "WIREPLUMBER_DATA_DIR=/usr/share/wireplumber","WIREPLUMBER_MODULE_DIR=/usr/lib64/wireplumber-0.5",
    "XDG_STATE_HOME=/run/aetheri-audio/state","GIO_MODULE_DIR=/nonexistent","GIO_USE_VFS=local",
#else
    "ALSA_CONFIG_PATH=/etc/aetheri-audio/alsa.conf",
#endif
    "PIPEWIRE_REMOTE=/run/aetheri-audio/pw-hda",NULL};
static void pause_ms(long ms) {struct timespec t={.tv_sec=ms/1000,.tv_nsec=(ms%1000)*1000000};while(nanosleep(&t,&t) && errno==EINTR) {}}
static bool read_text(const char *path,char *out,size_t capacity) {
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0) return false;
    ssize_t n=read(fd,out,capacity-1);char extra;ssize_t tail=n>=0 ? read(fd,&extra,1) : -1;int result=close(fd);
    if(n<0 || tail!=0 || result || memchr(out,0,(size_t)n)) return false;
    out[n]=0;return true;
}
static bool zero_caps(void) {
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3,.pid=0};struct __user_cap_data_struct d[2]={{0}};
    return syscall(SYS_capget,&h,d)==0 && !(d[0].effective|d[0].permitted|d[0].inheritable|d[1].effective|d[1].permitted|d[1].inheritable);
}
static bool spawn(char *const args[],bool ordinary,const char *role,int output,struct child *out) {
    pid_t pid=fork();if(pid<0) return false;
    if(!pid) {
        if(output>=0 && dup2(output,STDOUT_FILENO)<0) _exit(125);
        if(syscall(SYS_close_range,3u,~0u,0u)) _exit(125);
        if(ordinary && (setgroups(0,NULL) || setresgid(1000,1000,1000) || setresuid(1000,1000,1000) || prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0) || !zero_caps())) _exit(125);
        if(role) dprintf(STDOUT_FILENO,"AETHERI_AUDIO_ACTOR role=%s uid=%u cap_zero=%d nnp=%d\n",role,(unsigned)getuid(),zero_caps(),prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0));
        execve(args[0],args,environment);_exit(127);
    }
    /* This is our unreaped direct child: its PID cannot have been reused. */
    int pin=(int)syscall(SYS_pidfd_open,pid,0);
    if(pin<0) {int status;(void)kill(pid,SIGKILL);while(waitpid(pid,&status,0)<0 && errno==EINTR) {}return false;}
    *out=(struct child){pid,pin};return true;
}
static bool wait_child(struct child *c,int ms,int *result) {
    struct pollfd p={c->pin,POLLIN,0};int n;do {n=poll(&p,1,ms);} while(n<0 && errno==EINTR);
    if(n!=1 || !(p.revents&POLLIN)) return false;
    int status;pid_t got;do {got=waitpid(c->pid,&status,0);} while(got<0 && errno==EINTR);
    if(got!=c->pid) return false;
    *result=WIFEXITED(status) ? WEXITSTATUS(status) : 128+(WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    close(c->pin);c->pid=-1;c->pin=-1;return true;
}
static bool stop(struct child *c) {
    if(c->pid<0) return true;
    int result;
    if(syscall(SYS_pidfd_send_signal,c->pin,SIGTERM,NULL,0) && errno!=ESRCH) return false;
    if(wait_child(c,3000,&result)) return result==0;
    (void)syscall(SYS_pidfd_send_signal,c->pin,SIGKILL,NULL,0);
    (void)wait_child(c,3000,&result);return false;
}
static bool capture(int fd,char *out,size_t capacity,size_t *used) {
    for(;;) {
        char bytes[256];ssize_t n=read(fd,bytes,sizeof(bytes));
        if(n>0) {if((size_t)n>=capacity-*used) return false;memcpy(out+*used,bytes,(size_t)n);*used+=(size_t)n;continue;}
        if(n<0 && errno==EINTR) continue;
        return n==0 || errno==EAGAIN;
    }
}
static bool command(char *const args[],bool ordinary,char *output,size_t capacity) {
    int pipes[2]={-1,-1};if(output && pipe2(pipes,O_CLOEXEC|O_NONBLOCK)) return false;
    struct child c={.pid=-1,.pin=-1};
    if(!spawn(args,ordinary,NULL,pipes[1],&c)) {if(output) {close(pipes[0]);close(pipes[1]);}return false;}
    if(pipes[1]>=0) close(pipes[1]);
    size_t used=0;bool okay=true;int result=-1;
    for(unsigned turn=0;turn<500;++turn) {
        if(output) okay=capture(pipes[0],output,capacity,&used);
        if(!okay || wait_child(&c,10,&result)) break;
    }
    if(c.pid>=0) {puts("AETHERI_AUDIO_COMMAND_TIMEOUT limit_ms=5000");okay=false;(void)stop(&c);}
    if(output) {if(!capture(pipes[0],output,capacity,&used)) okay=false;output[used]=0;close(pipes[0]);}
    return okay && result==0;
}
static bool line(const char *text,const char *value) {
    size_t n=strlen(value);
    for(const char *p=text;*p;) {const char *end=strchr(p,'\n');if(!end) return false;if((size_t)(end-p)==n && !memcmp(p,value,n)) return true;p=end+1;}
    return false;
}
static bool device_access(const char *path,bool configure) {
    struct stat st;
    if(lstat(path,&st) || !S_ISCHR(st.st_mode) || major(st.st_rdev)!=116) return false;
    if(!configure) return st.st_uid==1000 && st.st_gid==1000 && (st.st_mode&0777)==0600;
    /* Root still owns the fresh devtmpfs node while changing its mode. */
    return !chmod(path,0600) && !chown(path,1000,1000);
}
static bool mixer(bool configure) {
    int fd=open("/dev/snd/controlC0",O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(fd<0) return false;
    struct snd_ctl_elem_id ids[16];struct snd_ctl_elem_list list={.space=16,.pids=ids};bool okay=false;
    if(ioctl(fd,SNDRV_CTL_IOCTL_ELEM_LIST,&list) || list.count>16 || list.used!=list.count) goto done;
    struct snd_ctl_elem_value changes[2]={0};unsigned selected=0;
    for(unsigned i=0;i<list.used;++i) {
        struct snd_ctl_elem_info info={.id=ids[i]};
        if(ioctl(fd,SNDRV_CTL_IOCTL_ELEM_INFO,&info)) goto done;
        printf("AETHERI_AUDIO_CONTROL name=%.44s type=%d count=%u access=%u\n",info.id.name,info.type,info.count,info.access);
        if(info.type==SNDRV_CTL_ELEM_TYPE_BOOLEAN || info.type==SNDRV_CTL_ELEM_TYPE_INTEGER) {
            struct snd_ctl_elem_value value={.id=ids[i]};
            if(!info.count || info.count>2 || ioctl(fd,SNDRV_CTL_IOCTL_ELEM_READ,&value)) goto done;
            printf("AETHERI_AUDIO_VALUES min=%ld max=%ld left=%ld right=%ld\n",info.value.integer.min,info.value.integer.max,value.value.integer.value[0],value.value.integer.value[info.count>1 ? 1 : 0]);
        }
        bool volume=!strcmp((char *)info.id.name,"Master Playback Volume"),enable=!strcmp((char *)info.id.name,"Master Playback Switch");
        if(volume || enable) {
            unsigned slot=volume ? 0u : 1u;
            if((selected&(1u<<slot)) || info.id.iface!=SNDRV_CTL_ELEM_IFACE_MIXER || info.id.index || info.id.device || info.id.subdevice || info.count!=2 || !(info.access&SNDRV_CTL_ELEM_ACCESS_WRITE) || info.type!=(volume ? SNDRV_CTL_ELEM_TYPE_INTEGER : SNDRV_CTL_ELEM_TYPE_BOOLEAN) || info.value.integer.min!=0 || info.value.integer.max!=(volume ? 74 : 1)) goto done;
            selected|=1u<<slot;changes[slot].id=ids[i];
            changes[slot].value.integer.value[0]=changes[slot].value.integer.value[1]=volume ? 74 : 1;
        }
    }
    if(selected!=3) goto done;
    /* Only the independently identified, disposable QEMU card. Validate both
       controls before any write; do not disable the emulated mixer. */
    for(unsigned i=0;i<2;++i) {
        if(configure && ioctl(fd,SNDRV_CTL_IOCTL_ELEM_WRITE,&changes[i])) goto done;
        struct snd_ctl_elem_value actual={.id=changes[i].id};
        if(ioctl(fd,SNDRV_CTL_IOCTL_ELEM_READ,&actual) || actual.value.integer.value[0]!=changes[i].value.integer.value[0] || actual.value.integer.value[1]!=changes[i].value.integer.value[1]) goto done;
    }
    puts("AETHERI_AUDIO_MIXER volume=74 switch=1 channels=2 verified=1");
    okay=true;
done:
    if(close(fd)) okay=false;
    return okay;
}
#ifdef AETHERI_AUDIO_POLICY
static bool property(const char *text,const char *key,char *value,size_t capacity) {
    const char *at=strstr(text,key);if(!at) return false;at+=strlen(key);
    const char *end=strchr(at,'"');if(!end || end==at || (size_t)(end-at)>=capacity || strstr(end+1,key)) return false;
    for(const char *p=at;p<end;++p) if(!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || strchr("._:-",*p))) return false;
    memcpy(value,at,(size_t)(end-at));value[end-at]=0;return true;
}
static bool automatic_links(const char *text,const char *sink) {
    bool found[2]={false,false};int channel=-1;
    for(const char *p=text;*p;) {
        const char *end=strchr(p,'\n');if(!end) return false;
        while(p<end && (*p==' ' || *p=='\t')) ++p;
        if((size_t)(end-p)==strlen("aetheri.audio.check:output_FL") && !memcmp(p,"aetheri.audio.check:output_FL",(size_t)(end-p))) channel=0;
        else if((size_t)(end-p)==strlen("aetheri.audio.check:output_FR") && !memcmp(p,"aetheri.audio.check:output_FR",(size_t)(end-p))) channel=1;
        else if(channel>=0 && (size_t)(end-p)>4 && !memcmp(p,"|-> ",4)) {
            char expected[512];int n=snprintf(expected,sizeof(expected),"|-> %s:playback_%s",sink,channel==0 ? "FL" : "FR");
            if(n<0 || (size_t)n>=sizeof(expected)) return false;
            if((size_t)(end-p)!=(size_t)n || memcmp(p,expected,(size_t)n) || found[channel]) return false;
            found[channel]=true;
        } else channel=-1;
        p=end+1;
    }
    return found[0] && found[1];
}
static bool start_udev(void) {
    char *daemon[]={"/usr/bin/systemd-udevd","--resolve-names=never",NULL};
    if(!spawn(daemon,false,"udev",-1,&udev)) return false;
    for(unsigned i=0;i<250 && access("/run/udev/control",F_OK);++i) pause_ms(20);
    struct stat st;if(lstat("/run/udev/control",&st) || !S_ISSOCK(st.st_mode) || st.st_uid) return false;
    char *trigger[]={"/usr/bin/udevadm","trigger","--action=change","--subsystem-match=sound",NULL};
    char *settle[]={"/usr/bin/udevadm","settle","--timeout=3",NULL};
    char *info[]={"/usr/bin/udevadm","info","--query=property","--path=/sys/class/sound/card0",NULL};char output[4096];
    if(!command(trigger,false,NULL,0) || !command(settle,false,NULL,0) || !command(info,false,output,sizeof(output))) return false;
    printf("UDEV_PROPERTIES\n%s",output);
    if(!line(output,"ID_SOUND=1") || !line(output,"SOUND_INITIALIZED=1")) return false;
    puts("AETHERI_AUDIO_UDEV initialized=1 database=1");return true;
}
static bool policy_output(char *sink,size_t capacity) {
    char output[8192]={0};char *inspect[]={"/usr/bin/wpctl","inspect","@DEFAULT_AUDIO_SINK@",NULL};bool ready=false;
    char *list[]={"/usr/bin/wpctl","list","audio","sinks",NULL};
    for(unsigned i=0;i<100;++i) {
        if(!command(list,true,output,sizeof(output))) return false;
        if(strstr(output,"alsa_output.") && strchr(output,'*')) {ready=true;break;}
        pause_ms(25);
    }
    if(!ready || !command(inspect,true,output,sizeof(output))) return false;
    printf("POLICY_SINK\n%s",output);
    if(!strstr(output,"media.class = \"Audio/Sink\"") || !property(output,"node.name = \"",sink,capacity) || strncmp(sink,"alsa_output.",12)) return false;
    char device[32];if(!property(output,"device.id = \"",device,sizeof(device)) || strspn(device,"0123456789")!=strlen(device)) return false;
    char *card[]={"/usr/bin/wpctl","inspect",device,NULL};
    if(!command(card,true,output,sizeof(output))) return false;
    printf("POLICY_CARD\n%s",output);
    if(!strstr(output,"device.api = \"alsa\"")) return false;
    char *profile[]={"/usr/bin/wpctl","get-profile",device,NULL};
    if(!command(profile,true,output,sizeof(output))) return false;
    printf("POLICY_PROFILE\n%s",output);
    char *volume[]={"/usr/bin/wpctl","set-volume","@DEFAULT_AUDIO_SINK@","1.0","--limit","1.0",NULL};
    char *unmute[]={"/usr/bin/wpctl","set-mute","@DEFAULT_AUDIO_SINK@","0",NULL};
    return command(volume,true,NULL,0) && command(unmute,true,NULL,0) && mixer(false);
}
#endif
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);const char *reason="setup";int result=1;char profile[128],vendor[32],device[32];
    if(getuid()!=0 || geteuid()!=0 || !read_text("/etc/aetheri-boot-proof",profile,sizeof(profile))) goto done;
    if(strlen(profile)<34 || profile[32]!='\n') goto done;
    profile[32]=0;
    for(unsigned i=0;i<32;++i) if(!((profile[i]>='0' && profile[i]<='9') || (profile[i]>='a' && profile[i]<='f'))) goto done;
    char *codec[]={"/usr/bin/modprobe","-C","/dev/null","snd_hda_codec_generic",NULL};
    char *controller[]={"/usr/bin/modprobe","-C","/dev/null","snd_hda_intel",NULL};
    reason="module-load";if(!command(codec,false,NULL,0) || !command(controller,false,NULL,0)) goto done;
    reason="device";
    /* devtmpfs publishes PCM and control nodes separately. Both are required
       before the one-time ownership changes; PCM alone is not readiness. */
    for(unsigned i=0;i<100 && (access("/dev/snd/pcmC0D0p",F_OK) || access("/dev/snd/controlC0",F_OK));++i) pause_ms(20);
    if(!read_text("/sys/class/sound/card0/device/vendor",vendor,sizeof(vendor)) || strcmp(vendor,"0x8086\n") || !read_text("/sys/class/sound/card0/device/device",device,sizeof(device)) || strcmp(device,"0x2668\n")) goto done;
#ifndef AETHERI_AUDIO_POLICY
    reason="device-access";if(!device_access("/dev/snd/controlC0",true) || !device_access("/dev/snd/pcmC0D0p",true)) goto done;
    if(access("/dev/snd/timer",F_OK)==0 && !device_access("/dev/snd/timer",true)) goto done;
    reason="mixer-setup";if(!mixer(true)) goto done;
#endif
    if(mkdir("/run/aetheri-audio",0700) || chown("/run/aetheri-audio",1000,1000)) goto done;
#ifdef AETHERI_AUDIO_POLICY
    reason="udev";if(!start_udev()) goto done;
    reason="udev-access";if(!device_access("/dev/snd/controlC0",false) || !device_access("/dev/snd/pcmC0D0p",false)) goto done;
    if(access("/dev/snd/timer",F_OK)==0 && !device_access("/dev/snd/timer",false)) goto done;
    puts("AETHERI_AUDIO_UDEV_ACCESS uid=1000 gid=1000 mode=0600 verified=1");
#endif
    puts("AETHERI_AUDIO_DEVICE pci=8086:2668 card=0 playback=1");
    char *server_args[]={"/usr/bin/pipewire","-c","server.conf",NULL};
    reason="server";if(!spawn(server_args,true,"server",-1,&server)) goto done;
    for(unsigned i=0;i<100 && access(remote,F_OK);++i) pause_ms(20);
    struct stat socket_info;if(lstat(remote,&socket_info) || !S_ISSOCK(socket_info.st_mode) || socket_info.st_uid!=1000 || (socket_info.st_mode&0777)!=0600) goto done;
#ifdef AETHERI_AUDIO_POLICY
    char *policy_args[]={"/usr/bin/wireplumber","-p","aetheri-hda-test",NULL};char sink[256];
    reason="policy";if(!spawn(policy_args,true,"policy",-1,&policy)) goto done;
    reason="policy-output";if(!policy_output(sink,sizeof(sink))) goto done;
    char *player_args[]={"/usr/libexec/aetheri-audio-check","--remote",(char *)remote,"--frames","96000","--wait-for-start","--autoconnect",NULL};
#else
    char *player_args[]={"/usr/libexec/aetheri-audio-check","--remote",(char *)remote,"--frames","96000","--wait-for-start",NULL};
#endif
    reason="player";if(!spawn(player_args,true,"player",-1,&player)) goto done;
#ifdef AETHERI_AUDIO_POLICY
    char *links[]={"/usr/bin/pw-link","-r",(char *)remote,"-o","-l",NULL};char linked[8192]={0};bool ready=false;
    reason="policy-links";
    for(unsigned i=0;i<100;++i) {
        if(!command(links,true,linked,sizeof(linked))) goto done;
        if(automatic_links(linked,sink)) {ready=true;break;}
        pause_ms(25);
    }
    printf("POLICY_LINKS\n%s",linked);if(!ready) goto done;
#else
    char *inputs[]={"/usr/bin/pw-link","-r",(char *)remote,"-i",NULL};
    char *outputs[]={"/usr/bin/pw-link","-r",(char *)remote,"-o",NULL};char in[4096]={0},out[4096]={0};bool ready=false;
    reason="ports";
    for(unsigned i=0;i<60;++i) {
        if(!command(inputs,true,in,sizeof(in)) || !command(outputs,true,out,sizeof(out))) goto done;
        ready=line(in,"aetheri.test.hda:playback_FL") && line(in,"aetheri.test.hda:playback_FR") && line(out,"aetheri.audio.check:output_FL") && line(out,"aetheri.audio.check:output_FR");
        if(ready) break;
        pause_ms(25);
    }
    if(!ready) {printf("PORT_INPUTS\n%sPORT_OUTPUTS\n%s",in,out);goto done;}
    char *left[]={"/usr/bin/pw-link","-r",(char *)remote,"aetheri.audio.check:output_FL","aetheri.test.hda:playback_FL",NULL};
    char *right[]={"/usr/bin/pw-link","-r",(char *)remote,"aetheri.audio.check:output_FR","aetheri.test.hda:playback_FR",NULL};
    reason="link";if(!command(left,true,NULL,0) || !command(right,true,NULL,0)) goto done;
#endif
    reason="activation";if(syscall(SYS_pidfd_send_signal,player.pin,SIGUSR1,NULL,0)) goto done;
    reason="playback";int player_result;if(!wait_child(&player,20000,&player_result) || player_result!=0) goto done;
    /* Fixture settling interval, not a production device-drain proof. The
       independent host capture must still contain the complete signal. */
    pause_ms(500);
#ifdef AETHERI_AUDIO_POLICY
    reason="policy-stop";if(!stop(&policy)) goto done;
    reason="udev-stop";if(!stop(&udev)) goto done;
    puts("AETHERI_AUDIO_POLICY alsa_monitor=1 acp=1 autolink=1 policy_end=1 udev_end=1");
#endif
    reason="server-stop";if(!stop(&server)) goto done;
    printf("AETHERI_AUDIO_PASS nonce=%s frames=96000 rate=48000 channels=2 server_end=1 player_end=1\n",profile);result=0;
done:
    if(result) printf("AETHERI_AUDIO_FAIL reason=%s errno=%d\n",reason,errno);
    bool player_stopped=stop(&player),server_stopped=stop(&server);
    if(!player_stopped || !server_stopped) result=1;
#ifdef AETHERI_AUDIO_POLICY
    bool policy_stopped=stop(&policy),udev_stopped=stop(&udev);
    if(!policy_stopped || !udev_stopped) result=1;
#endif
    return result;
}
