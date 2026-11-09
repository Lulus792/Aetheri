#define _GNU_SOURCE
#include "settings/nexaui_main.h"
#include "settings/audio_host.h"
#include "audio/session_control.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <systemd/sd-login.h>
#include <unistd.h>

/* Explicit disposable audio-lab client. The independent root fixture binds
 * originals before trusted PID1/PAM FD delivery: graph3, policy4, profile5,
 * manager image6, graph image7, policy image8. This is not normal app IPC,
 * caller admission, a supported public profile or an installed service. */
static void retired(void *data) {*(bool *)data=true;}
static gboolean deadline(void *data) {*(bool *)data=true;return G_SOURCE_REMOVE;}
static bool laboratory(void) {
    char data[64]={0};int fd=open("/sys/class/dmi/id/product_name",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0) return false;
    ssize_t n=read(fd,data,sizeof(data)-1);close(fd);
    return n>0 && !strcmp(data,"Aetheri audio lab VM\n") && getuid()==1000 && geteuid()==1000 && getgid()==1000 && getegid()==1000;
}
static bool profile(char bytes[4096],char *fields[6]) {
    struct stat st;
    if(fstat(5,&st) || !S_ISREG(st.st_mode) || st.st_uid || (st.st_mode&07777)!=0444 || st.st_nlink!=1 ||
            st.st_size<9 || st.st_size>4095 || (fcntl(5,F_GETFL)&O_ACCMODE)!=O_RDONLY) return false;
    ssize_t n=pread(5,bytes,(size_t)st.st_size,0);
    if(n!=st.st_size || memchr(bytes,0,(size_t)n) || memcmp(bytes,"AETHAUI1\n",9)) return false;
    bytes[n]=0;char *next=bytes+9;
    for(unsigned i=0;i<6;++i) {
        char *end=strchr(next,'\n');if(!end || end==next) return false;
        *end=0;fields[i]=next;next=end+1;
    }
    return !*next && fields[1][0]=='/';
}
int main(int argc,char **argv) {
    if(argc<3 || !laboratory()) return 2;
    char bytes[4096],*fields[6];if(!profile(bytes,fields)) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    for(int fd=3;fd<=8;++fd) {
        int flags=fcntl(fd,F_GETFD);if(flags<0 || fcntl(fd,F_SETFD,flags|FD_CLOEXEC)<0) return 2;
    }
    if(close_range(9,~0u,0)) return 2;
    char *session=NULL;
    if(sd_pid_get_session(0,&session)<0) return 1;
    printf("AETHERI_SETTINGS_AUDIO_SESSION id=%s\n",session);free(session);
    sd_bus *user=NULL,*system=NULL;GMainContext *context=g_main_context_new();
    if(!context) return 1;
    g_main_context_push_thread_default(context);
    int result=1;bool destroyed=false,expired=false;
    struct ae_audio_session_control *audio=NULL;struct ae_settings_audio_host *host=NULL;
    if(sd_bus_open_user(&user)<0 || sd_bus_open_system(&system)<0 ||
            sd_bus_set_method_call_timeout(user,3000000)<0 || sd_bus_set_method_call_timeout(system,3000000)<0) goto done;
    const struct ae_audio_session_profile selected={.user_bus=user,.system_bus=system,.uid=1000,
        .seat=fields[0],.remote=fields[1],.graph_unit=fields[2],.policy_unit=fields[3],
        .graph_context=fields[4],.policy_context=fields[5],.graph_pidfd=3,.policy_pidfd=4,
        .manager_image=6,.graph_image=7,.policy_image=8};
    audio=ae_audio_session_control_open(context,&selected,&destroyed,retired);
    for(int fd=3;fd<=8;++fd) close(fd);
    if(!audio) {fprintf(stderr,"AETHERI_SETTINGS_AUDIO_OPEN_FAIL errno=%d\n",errno);goto done;}
    host=ae_settings_audio_host_open(context,audio);if(!host) goto done;
    puts("AETHERI_SETTINGS_AUDIO_OPEN original=1");
    result=ae_settings_nexaui_run(argc,argv,host);
done:
    if(host && !ae_settings_audio_host_close(&host)) result=1;
    if(audio) {
        if(!ae_audio_session_control_close(&audio)) result=1;
        GSource *timer=g_timeout_source_new(8000);g_source_set_callback(timer,deadline,&expired,NULL);g_source_attach(timer,context);
        while(!destroyed && !expired) g_main_context_iteration(context,TRUE);
        g_source_destroy(timer);g_source_unref(timer);
        if(!destroyed) {fputs("AETHERI_SETTINGS_AUDIO_RETIRE_PENDING\n",stderr);result=1;}
        else puts("AETHERI_SETTINGS_AUDIO_RETIRED local_observer_only=1");
    }
    sd_bus_unref(user);sd_bus_unref(system);
    g_main_context_pop_thread_default(context);g_main_context_unref(context);return result;
}
