/* Finite diskless integration fixture, never a normal login/authority service.
 * Reuse the original fixture's checked device/parser/child operations. */
#define AETHERI_AUDIO_POLICY
#define AETHERI_AUDIO_NATIVE_CONTROL
#define AETHERI_AUDIO_USER_SESSION
#define main ae_original_audio_fixture_main
#include "audio.c"
#undef main
#include <dirent.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>
#include "../audio/session_guard.h"
#include "../audio/manager.h"

static char session_case[32]="playback";
/* Fixed test barriers only. They select no PID, path, unit or command. The
 * root test driver never treats user-written bytes as authorization/evidence. */
static bool barrier(const char *leaf) {
    char path[128],pending[160];snprintf(path,sizeof(path),"/run/aetheri-audio/%s",leaf);
    snprintf(pending,sizeof(pending),"%s.pending",path);
    int fd=open(pending,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0) return false;
    bool okay=write(fd,"1\n",2)==2;
    if(close(fd) || !okay) return false;
    /* A reader must never see the empty create-before-write interval. These
     * are ephemeral RAM test barriers, not durable operation receipts. */
    return renameat2(AT_FDCWD,pending,AT_FDCWD,path,RENAME_NOREPLACE)==0;
}
static bool await_barrier(const char *leaf,unsigned turns) {
    char path[128],value[8];snprintf(path,sizeof(path),"/run/aetheri-audio/%s",leaf);
    for(unsigned i=0;i<turns;++i) {
        if(read_text(path,value,sizeof(value))) return !strcmp(value,"1\n");
        pause_ms(50);
    }
    return false;
}

static const char *manager_name="org.freedesktop.systemd1";
static struct ae_audio_manager *manager_guard;
static bool manager_current(void) {return manager_guard && ae_audio_manager_check(manager_guard)==0;}
static bool await_initial_manager(sd_bus *bus) {
    /* Read-only initial discovery, before capturing an original or submitting
     * any unit effect. Bus availability precedes systemd's name acquisition.
     * Only NameHasNoOwner may wait; no admitted/replaced owner is retried. */
    for(unsigned i=0;i<100;++i) {
        sd_bus_message *reply=NULL;sd_bus_error error=SD_BUS_ERROR_NULL;const char *name=NULL;
        int r=sd_bus_call_method(bus,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","GetNameOwner",&error,&reply,"s","org.freedesktop.systemd1");
        bool ready=r>=0 && sd_bus_message_has_signature(reply,"s") && sd_bus_message_read(reply,"s",&name)>0 && name && name[0]==':' && sd_bus_service_name_is_valid(name);
        bool absent=r<0 && sd_bus_error_has_name(&error,SD_BUS_ERROR_NAME_HAS_NO_OWNER);
        sd_bus_message_unref(reply);sd_bus_error_free(&error);
        if(ready) {printf("AETHERI_AUDIO_MANAGER_DISCOVERY initial=1 attempts=%u\n",i+1);return true;}
        if(!absent) return false;
        pause_ms(50);
    }
    return false;
}
static bool retain_units(sd_bus *bus) {
    if(!manager_current()) return false;
    const char *units[]={"aetheri-session.target","aetheri-audio.target","aetheri-pipewire.service","aetheri-wireplumber.service"};
    for(unsigned i=0;i<4;++i) {
        sd_bus_message *reply=NULL;sd_bus_error error=SD_BUS_ERROR_NULL;
        int r=sd_bus_call_method(bus,manager_name,"/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","RefUnit",&error,&reply,"s",units[i]);
        bool okay=r>=0 && sd_bus_message_at_end(reply,1)>0;
        if(!okay) printf("AETHERI_AUDIO_USER_BUS_FAIL method=RefUnit unit=%s code=%d error=%s\n",units[i],r,error.name ? error.name : "reply");
        sd_bus_message_unref(reply);sd_bus_error_free(&error);if(!okay) return false;
    }
    /* These are per-connection GC references, not process/invocation or lease
     * evidence. This dedicated connection drops them at its final close. No
     * unknown ref call is replayed and no replacement owner is adopted. */
    printf("AETHERI_AUDIO_USER_REFS owner=%s retained=4\n",manager_name);return true;
}
static bool method(sd_bus *bus,const char *name,const char *unit) {
    if(!manager_current()) return false;
    sd_bus_message *reply=NULL;const char *job=NULL;
    sd_bus_error error=SD_BUS_ERROR_NULL;
    int r=sd_bus_call_method(bus,manager_name,"/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager",name,&error,&reply,"ss",unit,"fail");
    bool okay=r>=0 && sd_bus_message_read(reply,"o",&job)>0 && job && *job;
    if(!okay) printf("AETHERI_AUDIO_USER_BUS_FAIL method=%s unit=%s code=%d error=%s\n",name,unit,r,error.name ? error.name : "reply");
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);return okay; /* Unknown calls are never replayed. */
}
static bool unit_path(sd_bus *bus,const char *unit,char path[256]) {
    if(!manager_current()) return false;
    sd_bus_message *reply=NULL;const char *value=NULL;
    int r=sd_bus_call_method(bus,manager_name,"/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","GetUnit",NULL,&reply,"s",unit);
    bool okay=r>=0 && sd_bus_message_read(reply,"o",&value)>0 && strlen(value)<256;
    if(okay) memcpy(path,value,strlen(value)+1);
    sd_bus_message_unref(reply);return okay;
}
static bool active(sd_bus *bus,const char *unit,uint32_t *pid) {
    char path[256];char *state=NULL;bool okay=unit_path(bus,unit,path) &&
        sd_bus_get_property_string(bus,manager_name,path,"org.freedesktop.systemd1.Unit","ActiveState",NULL,&state)>=0 && !strcmp(state,"active") &&
        sd_bus_get_property_trivial(bus,manager_name,path,"org.freedesktop.systemd1.Service","MainPID",NULL,'u',pid)>=0 && *pid;
    free(state);return okay;
}
static bool inactive(sd_bus *bus,const char *unit) {
    char path[256];char *state=NULL;
    bool okay=unit_path(bus,unit,path) && sd_bus_get_property_string(bus,manager_name,path,"org.freedesktop.systemd1.Unit","ActiveState",NULL,&state)>=0 && ( !strcmp(state,"inactive") || !strcmp(state,"failed") );
    free(state);return okay;
}
static void unit_debug(sd_bus *bus,const char *unit) {
    char path[256];if(!unit_path(bus,unit,path)) {printf("AETHERI_AUDIO_UNIT_DEBUG unit=%s missing=1\n",unit);return;}
    const char *names[]={"LoadState","ActiveState","SubState","Result"};
    for(unsigned i=0;i<4;++i) {
        char *value=NULL;int r=sd_bus_get_property_string(bus,manager_name,path,i==3 ? "org.freedesktop.systemd1.Service" : "org.freedesktop.systemd1.Unit",names[i],NULL,&value);
        printf("AETHERI_AUDIO_UNIT_DEBUG unit=%s property=%s result=%d value=%.128s\n",unit,names[i],r,value ? value : "-");free(value);
    }
    int32_t status=0;int r=sd_bus_get_property_trivial(bus,manager_name,path,"org.freedesktop.systemd1.Service","ExecMainStatus",NULL,'i',&status);
    printf("AETHERI_AUDIO_UNIT_DEBUG unit=%s property=ExecMainStatus result=%d value=%d\n",unit,r,(int)status);
}
static bool child_actor(sd_bus *bus,const char *unit,const char *image,const char *role,int *pin) {
    uint32_t pid=0,again=0;if(!active(bus,unit,&pid)) return false;
    int fd=(int)syscall(SYS_pidfd_open,(pid_t)pid,0);if(fd<0) return false;
    char path[128],status[4096],executable[256];snprintf(path,sizeof(path),"/proc/%u/status",pid);
    bool okay=read_text(path,status,sizeof(status));bool status_read=okay;snprintf(path,sizeof(path),"/proc/%u/exe",pid);
    ssize_t n=readlink(path,executable,sizeof(executable)-1);int image_error=n<0 ? errno : 0;if(n<0) okay=false;else executable[n]=0;
    okay=okay && !strcmp(executable,image) && strstr(status,"Uid:\t1000\t1000\t1000\t1000\n") && strstr(status,"Gid:\t1000\t1000\t1000\t1000\n") &&
        (line(status,"Groups:\t1000 ") || line(status,"Groups:\t")) &&
        strstr(status,"CapEff:\t0000000000000000\n") && strstr(status,"CapPrm:\t0000000000000000\n") && strstr(status,"CapInh:\t0000000000000000\n") && strstr(status,"NoNewPrivs:\t1\n") &&
        active(bus,unit,&again) && pid==again;
    struct pollfd p={fd,POLLIN,0};okay=okay && poll(&p,1,0)==0;
    if(!okay) {
        printf("AETHERI_AUDIO_ACTOR_CONTEXT role=%s pid=%u status_read=%d image_error=%d image=%s\n",role,pid,status_read,image_error,n>=0 ? executable : "-");
        const char *keys[]={"Uid:","Gid:","Groups:","CapEff:","CapPrm:","CapInh:","NoNewPrivs:"};
        if(status_read) for(unsigned i=0;i<7;++i) {const char *field=strstr(status,keys[i]);if(field) {const char *end=strchr(field,'\n');if(end && end-field<128) printf("AETHERI_AUDIO_ACTOR_FIELD role=%s %.*s\n",role,(int)(end-field),field);}}
        close(fd);return false;
    }
    printf("AETHERI_AUDIO_ACTOR role=%s uid=1000 cap_zero=1 nnp=1\n",role);
    printf("AETHERI_AUDIO_USER_UNIT name=%s pid=%u image=%s\n",unit,pid,image);*pin=fd;return true;
}
static bool access_check(bool allowed) {
    gid_t group=0;int groups=getgroups(1,&group);
    if(getuid()!=1000 || geteuid()!=1000 || getgid()!=1000 || getegid()!=1000 || !(groups==0 || (groups==1 && group==1000)) || !zero_caps() || prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)!=1) return false;
    for(unsigned i=0;i<2;++i) {
        const char *name=i ? "/dev/snd/pcmC0D0p" : "/dev/snd/controlC0";
        int fd=open(name,O_RDWR|O_CLOEXEC|O_NOFOLLOW);int error=errno;
        if(fd>=0) close(fd);
        if(allowed ? fd<0 : fd>=0 || (error!=EACCES && error!=EPERM)) return false;
    }
    printf("AETHERI_AUDIO_SESSION_ACCESS allowed=%d uid=1000 groups=%d cap_zero=1\n",allowed,groups);return true;
}
static bool local_session(void) {
    char *session=NULL,*seat=NULL,*type=NULL,*class=NULL;uid_t uid=0;
    int found=sd_pid_get_session(0,&session),owner=-1,active_session=-1,remote_session=-1,seat_result=-1,type_result=-1,class_result=-1;
    if(found>=0) {owner=sd_session_get_uid(session,&uid);active_session=sd_session_is_active(session);remote_session=sd_session_is_remote(session);seat_result=sd_session_get_seat(session,&seat);type_result=sd_session_get_type(session,&type);class_result=sd_session_get_class(session,&class);}
    gid_t group=0;int groups=getgroups(1,&group);
    bool okay=getuid()==1000 && getgid()==1000 && (groups==0 || (groups==1 && group==1000)) && zero_caps() && found>=0 && owner>=0 && uid==1000 && active_session>0 && remote_session==0 &&
        seat_result>=0 && !strcmp(seat,"seat0") && type_result>=0 && !strcmp(type,"wayland") && class_result>=0 && !strcmp(class,"user");
    if(!okay) printf("AETHERI_AUDIO_SESSION_CONTEXT uid=%u gid=%u groups=%d caps_zero=%d found=%d owner=%d session_uid=%u active=%d remote=%d seat=%s type=%s class=%s\n",(unsigned)getuid(),(unsigned)getgid(),getgroups(0,NULL),zero_caps(),found,owner,(unsigned)uid,active_session,remote_session,seat ? seat : "-",type ? type : "-",class ? class : "-");
    if(okay) printf("AETHERI_AUDIO_LOCAL_SESSION id=%s uid=1000 seat=seat0 active=1 remote=0 type=wayland class=user\n",session);
    free(session);free(seat);free(type);free(class);return okay;
}
static bool node_metadata(const char *phase) {
    for(unsigned i=0;i<2;++i) {
        const char *path=i ? "/dev/snd/pcmC0D0p" : "/dev/snd/controlC0";struct stat s;
        if(lstat(path,&s) || !S_ISCHR(s.st_mode)) return false;
        printf("AETHERI_AUDIO_SESSION_NODE phase=%s path=%s uid=%u gid=%u mode=%04o\n",phase,path,(unsigned)s.st_uid,(unsigned)s.st_gid,(unsigned)(s.st_mode&0777));
        if(s.st_uid!=0 || s.st_gid!=63 || (s.st_mode&0777)!=0660) return false;
    }
    return true;
}
static int user_check(void) {
    sd_bus *bus=NULL,*system=NULL;int pins[2]={-1,-1},image=-1;bool okay=false,stop_requested=false;char sink[256];const char *reason="local-session";
    struct ae_audio_session_guard *guard=NULL;unsigned loss=0;
    if(!local_session() || !access_check(true)) goto done;
    reason="user-bus";
    for(unsigned i=0;i<100 && access("/run/user/1000/bus",F_OK);++i) pause_ms(50);
    if(sd_bus_open_user(&bus)<0) goto done;
    if(sd_bus_open_system(&system)<0 || sd_bus_set_method_call_timeout(bus,1000000)<0 || sd_bus_set_method_call_timeout(system,1000000)<0) goto done;
    reason="initial-manager-discovery";if(!await_initial_manager(bus)) goto done;
    reason="user-manager-admission";image=open("/usr/lib/systemd/systemd",O_PATH|O_CLOEXEC|O_NOFOLLOW);if(image<0) goto done;
    manager_guard=ae_audio_manager_open(bus,system,1000,image);
    if(!manager_guard) {printf("AETHERI_AUDIO_MANAGER_REJECT errno=%d\n",errno);goto done;}
    manager_name=ae_audio_manager_name(manager_guard);
    printf("AETHERI_AUDIO_USER_MANAGER owner=%s\n",manager_name);
    puts("AETHERI_AUDIO_MANAGER_ADMITTED original=1 pid1_main=1 image=1 invocation=1 cgroup=1");
    reason="user-start";if(!method(bus,"StartUnit","aetheri-session.target") || !method(bus,"StartUnit","aetheri-audio.target")) goto done;
    reason="unit-references";if(!retain_units(bus)) goto done;
    uint32_t pid=0;bool ready=false;
    for(unsigned i=0;i<100;++i) {if(active(bus,"aetheri-wireplumber.service",&pid)) {ready=true;break;}pause_ms(50);}
    reason="user-actors";
    if(!ready || !child_actor(bus,"aetheri-pipewire.service","/usr/bin/pipewire","server",&pins[0]) || !child_actor(bus,"aetheri-wireplumber.service","/usr/bin/wireplumber","policy",&pins[1])) {
        unit_debug(bus,"aetheri-session.target");unit_debug(bus,"aetheri-audio.target");unit_debug(bus,"aetheri-pipewire.service");unit_debug(bus,"aetheri-wireplumber.service");goto done;
    }
    reason="session-guard";guard=ae_audio_session_guard_open(1000,"seat0",pins[0],pins[1]);if(!guard) goto done;
    reason="native-control";if(!manager_current() || !policy_output_original(sink,sizeof(sink),pins[0])) goto done;
    char *args[]={"/usr/libexec/aetheri-audio-check","--remote",(char *)remote,"--frames","96000","--wait-for-start","--autoconnect","--server-pidfd","3",NULL};
    reason="player";if(!manager_current() || !spawn_with_pin(args,false,"player",-1,pins[0],&player)) goto done;
    char *links[]={"/usr/bin/pw-link","-r",(char *)remote,"-o","-l",NULL};char output[8192];ready=false;
    for(unsigned i=0;i<100;++i) {if(!command(links,false,output,sizeof(output))) goto done;if(automatic_links(output,sink)) {ready=true;break;}pause_ms(25);}
    printf("POLICY_LINKS\n%s",output);reason="playback";
    int status=0;bool player_end=false;
    if(!ready || syscall(SYS_pidfd_send_signal,player.pin,SIGUSR1,NULL,0)) goto done;
    for(unsigned i=0;i<800;++i) {
        if(ae_audio_session_guard_wait(guard,25) || !manager_current()) goto done;
        if(wait_child(&player,0,&status)) {player_end=true;break;}
    }
    if(!player_end || status) goto done;
    pause_ms(500);
    if(strcmp(session_case,"playback")) {
        reason="loss-injection";
        if(!strcmp(session_case,"vt-loss")) {if(!barrier("loss-ready")) goto done;}
        else {
            unsigned selected=!strcmp(session_case,"server-loss") ? 0u : 1u;
            printf("AETHERI_AUDIO_LOSS_INJECT kind=%s original_pidfd=1\n",selected ? "policy" : "server");
            if(syscall(SYS_pidfd_send_signal,pins[selected],SIGKILL,NULL,0)) goto done;
        }
        reason="loss-observation";
        for(unsigned i=0;i<200 && !loss;++i) loss=ae_audio_session_guard_wait(guard,50);
        unsigned expected=!strcmp(session_case,"vt-loss") ? AE_AUDIO_SESSION_INACTIVE : !strcmp(session_case,"server-loss") ? AE_AUDIO_SESSION_SERVER_EXIT : AE_AUDIO_SESSION_POLICY_EXIT;
        if(!loss || (loss&AE_AUDIO_SESSION_OBSERVATION_FAILED) || !(loss&expected)) goto done;
        printf("AETHERI_AUDIO_LOSS_OBSERVED case=%s mask=%u latched=1\n",session_case,loss);
    } else if(ae_audio_session_guard_wait(guard,0)) goto done;
    reason="user-stop";stop_requested=true;
    if(loss) puts("AETHERI_AUDIO_LOSS_STEP name=stop-request");
    if(!method(bus,"StopUnit","aetheri-session.target")) goto done;
    if(loss) puts("AETHERI_AUDIO_LOSS_STEP name=stop-acknowledged");
    for(unsigned i=0;i<100;++i) {
        struct pollfd p[2]={{pins[0],POLLIN,0},{pins[1],POLLIN,0}};
        if(poll(p,2,0)==2 && (p[0].revents&POLLIN) && (p[1].revents&POLLIN)) {okay=true;break;}pause_ms(50);
    }
    if(!okay) goto done;
    if(loss) puts("AETHERI_AUDIO_LOSS_STEP name=originals-ended");
    if(loss) {
        okay=false;reason="loss-retirement";
        struct stat st;bool retired=false;
        for(unsigned i=0;i<100;++i) {
            if(lstat("/run/user/1000/aetheri-audio",&st)<0 && errno==ENOENT && inactive(bus,"aetheri-audio.target") && inactive(bus,"aetheri-pipewire.service") && inactive(bus,"aetheri-wireplumber.service")) {retired=true;break;}
            pause_ms(50);
        }
        if(!retired) {
            int result=lstat("/run/user/1000/aetheri-audio",&st),error=errno;
            printf("AETHERI_AUDIO_LOSS_RUNTIME_DEBUG lstat=%d errno=%d\n",result,error);
            unit_debug(bus,"aetheri-session.target");unit_debug(bus,"aetheri-audio.target");unit_debug(bus,"aetheri-pipewire.service");unit_debug(bus,"aetheri-wireplumber.service");goto done;
        }
        puts("AETHERI_AUDIO_LOSS_STEP name=runtime-retired");
        if(!strcmp(session_case,"vt-loss")) {
            reason="inactive-device-access";bool denied=false;
            for(unsigned i=0;i<100 && !denied;++i) {denied=access_check(false);if(!denied) pause_ms(50);}
            if(!denied || !barrier("loss-stopped")) goto done;
            char *session=NULL;reason="reactivation";if(sd_pid_get_session(0,&session)<0) goto done;
            bool returned=false;
            for(unsigned i=0;i<100;++i) {if(sd_session_is_active(session)>0) {returned=true;break;}pause_ms(50);}
            free(session);if(!returned) goto done;
        }
        pause_ms(1000);reason="no-automatic-restart";
        if(ae_audio_session_guard_wait(guard,0)!=loss || !inactive(bus,"aetheri-session.target") || !inactive(bus,"aetheri-audio.target") || !inactive(bus,"aetheri-pipewire.service") || !inactive(bus,"aetheri-wireplumber.service") || lstat("/run/user/1000/aetheri-audio",&st)==0 || errno!=ENOENT) goto done;
        printf("AETHERI_AUDIO_SESSION_LOSS case=%s originals=2 endpoint_removed=1 restart=0 reactivated=%d\n",session_case,!strcmp(session_case,"vt-loss"));okay=true;
    }
    puts("AETHERI_AUDIO_USER_END originals=2 session_target_stopped=1");
done:
    if(!okay) printf("AETHERI_AUDIO_SESSION_FAIL reason=%s\n",reason);
    if(!okay && bus && manager_guard && !stop_requested) {stop_requested=true;(void)method(bus,"StopUnit","aetheri-session.target");}
    if(!stop(&player)) okay=false;
    ae_audio_session_guard_close(&guard);
    for(unsigned i=0;i<2;++i) if(pins[i]>=0) close(pins[i]);
    ae_audio_manager_close(&manager_guard);
    if(image>=0) close(image);
    sd_bus_unref(bus);sd_bus_unref(system);manager_name="org.freedesktop.systemd1";return okay ? 0 : 1;
}
static int root_check(void) {
    const char *reason="root-setup";bool okay=false;char profile[128],value[32];
    struct child login_actor={.pid=-1,.pin=-1};
    if(getuid()!=0 || !read_text("/etc/aetheri-boot-proof",profile,sizeof(profile)) || strlen(profile)<33 || profile[32]!='\n') goto done;
    profile[32]=0;
    char *codec[]={"/usr/bin/modprobe","-C","/dev/null","snd_hda_codec_generic",NULL},*controller[]={"/usr/bin/modprobe","-C","/dev/null","snd_hda_intel",NULL};
    reason="device";if(!command(codec,false,NULL,0) || !command(controller,false,NULL,0)) goto done;
    for(unsigned i=0;i<100 && access("/dev/snd/controlC0",F_OK);++i) pause_ms(20);
    if(!read_text("/sys/class/sound/card0/device/vendor",value,sizeof(value)) || strcmp(value,"0x8086\n") || !read_text("/sys/class/sound/card0/device/device",value,sizeof(value)) || strcmp(value,"0x2668\n")) goto done;
    if(mkdir("/run/aetheri-audio",0700) || chown("/run/aetheri-audio",1000,1000) || mkdir("/home",0755) || mkdir("/home/aetheri",0700) || chown("/home/aetheri",1000,1000)) goto done;
    reason="udev";if(!start_udev() || !mixer(true,true)) goto done;
    puts("AETHERI_AUDIO_NATIVE_BASELINE hardware_unity=1 before_server=1");
    puts("AETHERI_AUDIO_DEVICE pci=8086:2668 card=0 playback=1");
    reason="udev-node-permissions";if(!node_metadata("before")) goto done;
    char *deny[]={"/usr/libexec/aetheri-audio-session-check","--denied",NULL};
    reason="before-login";if(!command_named(deny,true,NULL,NULL,0,1000)) goto done;
    char *services[]={"/usr/bin/systemctl","start","systemd-journald.socket","systemd-journald-dev-log.socket","systemd-journald.service","dbus.socket","dbus.service","systemd-logind.service",NULL};
    reason="login-services";if(!command_named(services,false,NULL,NULL,0,3000)) goto done;
    char *vt[]={"/usr/bin/chvt","8",NULL};if(!command(vt,false,NULL,0)) goto done;
    char *login[]={"/usr/bin/systemctl","start","aetheri-audio-login.service",NULL};
    reason="login-test";
    if(!strcmp(session_case,"vt-loss")) {
        reason="login-spawn";if(!spawn(login,false,NULL,-1,&login_actor)) goto done;
        reason="loss-ready";if(!await_barrier("loss-ready",1800)) goto done;
        puts("AETHERI_AUDIO_LOSS_INJECT kind=vt target=1");
        char *away[]={"/usr/bin/chvt","1",NULL};reason="switch-away";if(!command(away,false,NULL,0)) goto done;
        puts("AETHERI_AUDIO_LOSS_STEP name=away-acknowledged");
        reason="loss-stopped";if(!await_barrier("loss-stopped",200)) goto done;
        puts("AETHERI_AUDIO_LOSS_INJECT kind=vt target=8");reason="switch-return";if(!command(vt,false,NULL,0)) goto done;
        reason="login-result";
        int result=0;if(!wait_child(&login_actor,30000,&result) || result) goto done;
    } else if(!command_named(login,false,NULL,NULL,0,9000)) goto done;
    reason="logout";bool gone=false;
    for(unsigned i=0;i<100;++i) {char **sessions=NULL;int n=sd_uid_get_sessions(1000,0,&sessions);if(sessions) {for(int j=0;j<n;++j) free(sessions[j]);free(sessions);}if(n==0 && access("/run/user/1000",F_OK)<0 && errno==ENOENT) {gone=true;break;}pause_ms(50);}
    if(!gone || !command_named(deny,true,NULL,NULL,0,1000) || !node_metadata("after")) goto done;
    puts("AETHERI_AUDIO_SESSION_REVOKED before=denied active=allowed after=denied owner=0 group=63 mode=0660");
    reason="udev-stop";if(!stop(&udev)) goto done;
    printf("AETHERI_AUDIO_SESSION_PASS nonce=%s user_end=1 runtime_removed=1 udev_end=1\n",profile);okay=true;
done:
    if(!okay) printf("AETHERI_AUDIO_SESSION_FAIL reason=%s errno=%d\n",reason,errno);
    if(!stop(&udev)) okay=false;
    if(!stop(&login_actor)) okay=false;
    return okay ? 0 : 1;
}
#ifndef AETHERI_AUDIO_SESSION_ENTRY
#define AETHERI_AUDIO_SESSION_ENTRY main
#endif
int AETHERI_AUDIO_SESSION_ENTRY(int argc,char **argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    char selected[32];if(!read_text("/etc/aetheri-audio/session-case",selected,sizeof(selected))) return 2;
    size_t size=strlen(selected);if(!size || selected[size-1]!='\n') return 2;selected[size-1]=0;
    if(strcmp(selected,"playback") && strcmp(selected,"vt-loss") && strcmp(selected,"server-loss") && strcmp(selected,"policy-loss")) return 2;
    memcpy(session_case,selected,size);
    if(argc==2 && !strcmp(argv[1],"--user")) return user_check();
    if(argc==2 && !strcmp(argv[1],"--denied")) return access_check(false) ? 0 : 1;
    if(argc!=1) return 2;
    return root_check();
}
