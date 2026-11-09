#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/capability.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sound/asound.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>

/* Fixed disposable-QEMU diagnostic, not an application ALSA transport. The
 * administrator independently identifies the virtual card and passes one exact
 * device FD3. No device is opened from a caller's path or numeric minor. */
static bool ordinary(void) {
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3};struct __user_cap_data_struct d[2]={{0}};
    gid_t group=0;int groups=getgroups(1,&group);
    return getuid()==1000 && geteuid()==1000 && getgid()==1000 && getegid()==1000 && (groups==0 || (groups==1 && group==1000)) &&
        prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)==1 && !syscall(SYS_capget,&h,d) &&
        !(d[0].effective|d[0].permitted|d[0].inheritable|d[1].effective|d[1].permitted|d[1].inheritable);
}
static bool device(int fd) {
    struct stat st;return !fstat(fd,&st) && S_ISCHR(st.st_mode) && major(st.st_rdev)==116;
}
static bool lab(void) {
    char text[64];int fd=open("/sys/class/dmi/id/product_name",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0) return false;
    ssize_t n=read(fd,text,sizeof(text));close(fd);
    return n==21 && !memcmp(text,"Aetheri audio lab VM\n",21);
}
static int baseline(void) {
    if(getuid()!=0 || geteuid()!=0 || !lab() || !device(3)) return 2;
    struct snd_ctl_card_info card={0};if(ioctl(3,SNDRV_CTL_IOCTL_CARD_INFO,&card) || card.card!=0 || memcmp(card.driver,"HDA-Intel",sizeof("HDA-Intel"))) return 2;
    struct snd_ctl_elem_id ids[16];struct snd_ctl_elem_list list={.space=16,.pids=ids};
    if(ioctl(3,SNDRV_CTL_IOCTL_ELEM_LIST,&list) || list.count>16 || list.used!=list.count) return 1;
    struct snd_ctl_elem_value changes[2]={0};unsigned found=0;
    for(unsigned i=0;i<list.used;++i) {
        struct snd_ctl_elem_info info={.id=ids[i]};if(ioctl(3,SNDRV_CTL_IOCTL_ELEM_INFO,&info)) return 1;
        bool volume=!memcmp(info.id.name,"Master Playback Volume",sizeof("Master Playback Volume")),enable=!memcmp(info.id.name,"Master Playback Switch",sizeof("Master Playback Switch"));
        if(!volume && !enable) continue;
        unsigned slot=volume ? 0u : 1u;
        if(found&(1u<<slot) || info.id.iface!=SNDRV_CTL_ELEM_IFACE_MIXER || info.id.index || info.id.device || info.id.subdevice || info.count!=2 ||
           !(info.access&SNDRV_CTL_ELEM_ACCESS_WRITE) || info.type!=(volume ? SNDRV_CTL_ELEM_TYPE_INTEGER : SNDRV_CTL_ELEM_TYPE_BOOLEAN) ||
           info.value.integer.min!=0 || info.value.integer.max!=(volume ? 74 : 1)) return 1;
        found|=1u<<slot;changes[slot].id=info.id;
        changes[slot].value.integer.value[0]=changes[slot].value.integer.value[1]=volume ? 74 : 1;
    }
    if(found!=3) return 1;
    for(unsigned i=0;i<2;++i) {
        struct snd_ctl_elem_value before={.id=changes[i].id},after={.id=changes[i].id};
        if(ioctl(3,SNDRV_CTL_IOCTL_ELEM_READ,&before)) return 1;
        printf("PCM_BASELINE_BEFORE slot=%u left=%ld right=%ld\n",i,before.value.integer.value[0],before.value.integer.value[1]);
        if(ioctl(3,SNDRV_CTL_IOCTL_ELEM_WRITE,&changes[i]) || ioctl(3,SNDRV_CTL_IOCTL_ELEM_READ,&after) ||
           after.value.integer.value[0]!=changes[i].value.integer.value[0] || after.value.integer.value[1]!=changes[i].value.integer.value[1]) return 1;
    }
    puts("PCM_BASELINE unity=1 unmuted=1 virtual_card=0");return 0;
}
static void exact(struct snd_pcm_hw_params *p,int parameter,unsigned value) {
    struct snd_interval *v=&p->intervals[parameter-SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    *v=(struct snd_interval){.min=value,.max=value,.integer=1};
}
static void mask(struct snd_pcm_hw_params *p,int parameter,unsigned value) {
    struct snd_mask *v=&p->masks[parameter-SNDRV_PCM_HW_PARAM_FIRST_MASK];memset(v,0,sizeof(*v));v->bits[value/32u]=1u<<(value%32u);
}
static int play(bool wait_start) {
    if(!ordinary() || !device(3)) return 2;
    int flags=fcntl(3,F_GETFL);if(flags<0 || (flags&O_ACCMODE)!=O_WRONLY) return 2;
    struct snd_pcm_info info={0};int version=0;
    if(ioctl(3,SNDRV_PCM_IOCTL_PVERSION,&version) || ioctl(3,SNDRV_PCM_IOCTL_INFO,&info) || info.card!=0 || info.device || info.subdevice || info.stream!=SNDRV_PCM_STREAM_PLAYBACK) return 2;
    printf("PCM_ORIGINAL card=0 device=0 subdevice=0 playback=1 protocol=%d uid=1000 cap_zero=1 nnp=1\n",version);
    if(wait_start) {
        sigset_t signals;sigemptyset(&signals);sigaddset(&signals,SIGUSR1);
        if(sigprocmask(SIG_BLOCK,&signals,NULL)) return 1;
        puts("PCM_READY start=explicit");int number=0;
        if(sigwait(&signals,&number) || number!=SIGUSR1 || sigprocmask(SIG_UNBLOCK,&signals,NULL)) return 1;
    }
    struct snd_pcm_hw_params hw={.rmask=UINT_MAX};
    for(unsigned i=0;i<sizeof(hw.masks)/sizeof(hw.masks[0]);++i) for(unsigned j=0;j<sizeof(hw.masks[i].bits)/sizeof(hw.masks[i].bits[0]);++j) hw.masks[i].bits[j]=UINT_MAX;
    for(unsigned i=0;i<sizeof(hw.intervals)/sizeof(hw.intervals[0]);++i) hw.intervals[i]=(struct snd_interval){.max=UINT_MAX};
    mask(&hw,SNDRV_PCM_HW_PARAM_ACCESS,SNDRV_PCM_ACCESS_RW_INTERLEAVED);mask(&hw,SNDRV_PCM_HW_PARAM_FORMAT,SNDRV_PCM_FORMAT_S16_LE);mask(&hw,SNDRV_PCM_HW_PARAM_SUBFORMAT,SNDRV_PCM_SUBFORMAT_STD);
    exact(&hw,SNDRV_PCM_HW_PARAM_CHANNELS,2);exact(&hw,SNDRV_PCM_HW_PARAM_RATE,48000);exact(&hw,SNDRV_PCM_HW_PARAM_SAMPLE_BITS,16);exact(&hw,SNDRV_PCM_HW_PARAM_FRAME_BITS,32);
    exact(&hw,SNDRV_PCM_HW_PARAM_PERIOD_SIZE,1024);exact(&hw,SNDRV_PCM_HW_PARAM_PERIODS,4);exact(&hw,SNDRV_PCM_HW_PARAM_BUFFER_SIZE,4096);
    if(ioctl(3,SNDRV_PCM_IOCTL_HW_PARAMS,&hw)) {perror("PCM hw params");return 1;}
    struct snd_pcm_sw_params sw={.period_step=1,.avail_min=1024,.xfer_align=1,.start_threshold=4096,.stop_threshold=4096,.boundary=4096u*1024u};
    if(ioctl(3,SNDRV_PCM_IOCTL_SW_PARAMS,&sw) || ioctl(3,SNDRV_PCM_IOCTL_PREPARE)) {perror("PCM prepare");return 1;}
    /* Keep a bounded silent tail in this diagnostic stream. Virtual DMA drain
       alone did not preserve the last audible samples at the host capture.
       The independent reference still requires all 96000 tone frames; this
       padding is not evidence of physical-device/production drain. */
    enum {signal_frames=96000,silence_frames=8192,total_frames=signal_frames+silence_frames};
    int16_t samples[2048];unsigned frame=0;
    while(frame<total_frames) {
        unsigned count=total_frames-frame;if(count>1024) count=1024;
        for(unsigned i=0;i<count;++i) for(unsigned c=0;c<2;++c) {
            double phase=2.0*3.14159265358979323846*(c ? 880.0 : 440.0)*(double)(frame+i)/48000.0;
            samples[2*i+c]=frame+i<signal_frames ? (int16_t)lrint(4096.0*sin(phase)) : 0;
        }
        struct snd_xferi transfer={.buf=samples,.frames=count};
        int r=ioctl(3,SNDRV_PCM_IOCTL_WRITEI_FRAMES,&transfer);
        if(r || transfer.result<=0 || (unsigned long)transfer.result>count) {
            if(!r) errno=transfer.result<0 && transfer.result>=-INT_MAX ? (int)-transfer.result : EIO;
            perror("PCM write; no replay");return 1;
        }
        frame+=(unsigned)transfer.result;
    }
    if(ioctl(3,SNDRV_PCM_IOCTL_DRAIN)) {perror("PCM drain");return 1;}
    puts("PCM_DRAINED signal_frames=96000 silence_frames=8192 submitted_frames=104192 rate=48000 channels=2 format=S16_LE");return 0;
}
int main(int argc,char **argv) {
    bool wait_start=argc==3 && !strcmp(argv[1],"--play") && !strcmp(argv[2],"--wait-for-start");
    if(argc!=2 && !wait_start) return 2;
    bool setup=!strcmp(argv[1],"--baseline"),playback=!strcmp(argv[1],"--play"),deny=!strcmp(argv[1],"--denied-open");
    if(!setup && !playback && !deny) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);alarm(20);
    if(syscall(SYS_close_range,4u,UINT_MAX,0)) return 2;
    int result=2;
    if(setup) result=baseline();
    if(playback) result=play(wait_start);
    if(deny && ordinary()) {
        int fd=open("/dev/snd/pcmC0D0p",O_WRONLY|O_CLOEXEC|O_NOFOLLOW);int error=errno;
        if(fd>=0) close(fd);
        if(fd<0 && error==EACCES) {puts("PCM_OPEN_DENIED uid=1000 layer=unspecified");result=0;}
    }
    close(3);return result;
}
