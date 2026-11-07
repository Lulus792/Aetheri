#include "linux/start.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <selinux/selinux.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MANAGER "org.freedesktop.systemd1"
#define PATH "/org/freedesktop/systemd1"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"actual PID1 start line %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } } while (0)
struct fixture {
    sd_bus *bus;
    struct ae_start *start;
    char name[128],fragment[256],path[256],text[256];
    int directory;
    unsigned int guards,notices;
    struct stat file;
};
static void carrier(void)
{
    struct stat st; CHECK(getuid()==0 && geteuid()==0 && getgid()==0 && getegid()==0 && security_getenforce()==1);
    CHECK(lstat("/etc/aetheri-p0-carrier",&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_nlink==1 && !(st.st_mode&0022));
    CHECK(stat("/sys/firmware/efi",&st)==0 && S_ISDIR(st.st_mode));
    FILE *f=fopen("/sys/class/dmi/id/product_name","r"); char text[128]={0};
    CHECK(f && fgets(text,sizeof text,f) && fclose(f)==0 && strcmp(text,"Aetheri P0 disposable VM\n")==0);
}
static int descriptors(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int n=0; while (readdir(d)) ++n; CHECK(closedir(d)==0); return n; }
static void file_check(struct fixture *s)
{
    int fd=openat(s->directory,s->name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW); struct stat st; char text[256]; CHECK(fd>=0);
    CHECK(fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_dev==s->file.st_dev && st.st_ino==s->file.st_ino &&
        st.st_uid==0 && st.st_gid==0 && st.st_nlink==1 && (st.st_mode&07777)==0600 && st.st_size==(off_t)strlen(s->text));
    CHECK(read(fd,text,sizeof text)==(ssize_t)strlen(s->text) && memcmp(text,s->text,strlen(s->text))==0 && close(fd)==0);
}
static void property(struct fixture *s,const char *key,const char *expected)
{
    char *value=NULL; CHECK(sd_bus_get_property_string(s->bus,MANAGER,s->path,MANAGER ".Unit",key,NULL,&value)>=0);
    CHECK(strcmp(value,expected)==0); free(value);
}
static void check_graph(struct fixture *s)
{
    const char *empty[]={"After","Before","Requires","Wants","Requisite","BindsTo","PartOf","Upholds","Conflicts",
        "OnFailure","OnSuccess","Triggers","TriggeredBy","PropagatesStopTo","PropagatesReloadTo"};
    for (size_t n=0;n<sizeof empty/sizeof empty[0];++n) {
        sd_bus_message *m=NULL; CHECK(sd_bus_get_property(s->bus,MANAGER,s->path,MANAGER ".Unit",empty[n],NULL,&m,"as")>=0);
        CHECK(sd_bus_message_enter_container(m,'a',"s")>0 && sd_bus_message_at_end(m,0)==1 && sd_bus_message_exit_container(m)>0);
        sd_bus_message_unref(m);
    }
    int reload=1,transient=1,defaults=1;
    CHECK(sd_bus_get_property_trivial(s->bus,MANAGER,s->path,MANAGER ".Unit","NeedDaemonReload",NULL,'b',&reload)>=0 && !reload);
    CHECK(sd_bus_get_property_trivial(s->bus,MANAGER,s->path,MANAGER ".Unit","Transient",NULL,'b',&transient)>=0 && !transient);
    CHECK(sd_bus_get_property_trivial(s->bus,MANAGER,s->path,MANAGER ".Unit","DefaultDependencies",NULL,'b',&defaults)>=0 && !defaults);
}
static int quiet(struct fixture *s,const char *expected)
{
    char *state=NULL; sd_bus_message *m=NULL; uint32_t id=UINT32_MAX; const char *path=NULL;
    CHECK(sd_bus_get_property_string(s->bus,MANAGER,s->path,MANAGER ".Unit","ActiveState",NULL,&state)>=0);
    CHECK(sd_bus_get_property(s->bus,MANAGER,s->path,MANAGER ".Unit","Job",NULL,&m,"(uo)")>=0 && sd_bus_message_read(m,"(uo)",&id,&path)>0);
    int same=strcmp(state,expected)==0 && !id && strcmp(path,"/")==0;
    free(state); sd_bus_message_unref(m); return same;
}
static int admit(void *userdata)
{
    struct fixture *s=userdata; ++s->guards; file_check(s); check_graph(s);
    property(s,"Id",s->name); property(s,"LoadState","loaded"); property(s,"FragmentPath",s->fragment);
    CHECK(quiet(s,"inactive")); return 0;
}
static int deny(void *userdata) { (void)userdata; return -EPERM; }
static void changed(void *userdata,const struct ae_start_status *status)
{
    struct fixture *s=userdata; ++s->notices;
    CHECK(!status->quarantined); struct ae_start_status current;
    CHECK(ae_start_status(s->start,&current)==0 && memcmp(&current,status,sizeof current)==0);
    struct ae_start *copy=s->start; CHECK(ae_start_close(&copy)==-EBUSY && copy==s->start);
}
int main(void)
{
    carrier(); int baseline=descriptors(); struct fixture s={.directory=-1};
    sd_id128_t token,boot; CHECK(sd_id128_randomize(&token)>=0 && sd_id128_get_boot(&boot)>=0);
    char id[SD_ID128_STRING_MAX]; sd_id128_to_string(token,id);
    CHECK(snprintf(s.name,sizeof s.name,"aetheri-test-start-%s.target",id)>0);
    CHECK(snprintf(s.fragment,sizeof s.fragment,"/run/systemd/system/%s",s.name)>0);
    CHECK(snprintf(s.text,sizeof s.text,"[Unit]\nDescription=Aetheri process-free original StartUnit test\nDefaultDependencies=no\n")>0);
    CHECK(sd_bus_new(&s.bus)>=0 && sd_bus_set_address(s.bus,"unix:path=/run/dbus/system_bus_socket")>=0 &&
        sd_bus_set_bus_client(s.bus,1)>=0 && sd_bus_start(s.bus)>=0);
    sd_bus_creds *creds=NULL; const char *unique=NULL; char manager[256];
    CHECK(sd_bus_get_name_creds(s.bus,MANAGER,SD_BUS_CREDS_UNIQUE_NAME,&creds)>=0 && sd_bus_creds_get_unique_name(creds,&unique)>=0);
    CHECK(strlen(unique)<sizeof manager); strcpy(manager,unique); sd_bus_creds_unref(creds);
    sd_bus_message *q=NULL,*m=NULL;
    CHECK(sd_bus_message_new_method_call(s.bus,&q,manager,PATH,"org.freedesktop.DBus.Peer","Ping")>=0 && sd_bus_call(s.bus,q,1000000,NULL,&m)>=0);
    struct ae_peer peer=AE_PEER_INIT; int r=ae_peer_capture_reply(m,q,&peer);
    if (r) fprintf(stderr,"actual PID1 capture: %s\n",strerror(-r));
    CHECK(r==0 && peer.connection_pid==1 && peer.connection_uid==0 && ae_peer_alive(&peer)==0);
    sd_bus_message_unref(q); sd_bus_message_unref(m); m=NULL;
    /* Verify absence across PID1's actual search paths before create-only setup. */
    CHECK(sd_bus_get_property(s.bus,MANAGER,PATH,MANAGER ".Manager","UnitPath",NULL,&m,"as")>=0 && sd_bus_message_enter_container(m,'a',"s")>0);
    const char *directory=NULL; unsigned int count=0;
    while ((r=sd_bus_message_read(m,"s",&directory))>0) {
        char path[4096]; struct stat st; CHECK(++count<=64 && directory[0]=='/' && snprintf(path,sizeof path,"%s/%s",directory,s.name)>0);
        CHECK(lstat(path,&st)<0 && errno==ENOENT);
    }
    CHECK(r==0 && count && sd_bus_message_exit_container(m)>0); sd_bus_message_unref(m); m=NULL;
    sd_bus_error error=SD_BUS_ERROR_NULL;
    CHECK(sd_bus_call_method(s.bus,MANAGER,PATH,MANAGER ".Manager","GetUnit",&error,NULL,"s",s.name)<0 && sd_bus_error_has_name(&error,"org.freedesktop.systemd1.NoSuchUnit"));
    sd_bus_error_free(&error);
    s.directory=open("/run/systemd/system",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW); CHECK(s.directory>=0);
    struct stat st; CHECK(fstat(s.directory,&st)==0 && st.st_uid==0 && !(st.st_mode&0022));
    int fd=openat(s.directory,s.name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600); CHECK(fd>=0);
    CHECK(fsetfilecon_raw(fd,"system_u:object_r:systemd_unit_file_t:s0")==0 && write(fd,s.text,strlen(s.text))==(ssize_t)strlen(s.text) &&
        fsync(fd)==0 && fstat(fd,&s.file)==0 && close(fd)==0 && fsync(s.directory)==0);
    CHECK(sd_bus_call_method(s.bus,MANAGER,PATH,MANAGER ".Manager","LoadUnit",NULL,&m,"s",s.name)>=0);
    const char *path=NULL; CHECK(sd_bus_message_read(m,"o",&path)>0 && strlen(path)<sizeof s.path); strcpy(s.path,path); sd_bus_message_unref(m); m=NULL;
    CHECK(sd_bus_call_method(s.bus,MANAGER,PATH,MANAGER ".Manager","RefUnit",NULL,NULL,"s",s.name)>=0);
    CHECK(sd_bus_call_method(s.bus,MANAGER,PATH,MANAGER ".Manager","Subscribe",NULL,NULL,"")>=0);
    CHECK(ae_start_new(s.bus,&peer,boot.bytes,s.name,changed,&s,&s.start)==0);
    CHECK(ae_start_request(s.start,deny,NULL)==-EPERM && quiet(&s,"inactive"));
    CHECK(ae_start_request(s.start,admit,&s)==-EALREADY && ae_start_close(&s.start)==0);
    CHECK(ae_start_new(s.bus,&peer,boot.bytes,s.name,changed,&s,&s.start)==0 && ae_start_request(s.start,admit,&s)==0 && s.guards==2);
    struct ae_start_status status; CHECK(ae_start_status(s.start,&status)==0 && status.attempted && status.pending && status.cookie);
    CHECK(ae_start_close(&s.start)==-EBUSY && s.start);
    for (unsigned int n=0;n<500;++n) {
        CHECK(sd_bus_process(s.bus,NULL)>=0 && ae_start_status(s.start,&status)==0 && !status.quarantined);
        if (status.job_removed) break;
        CHECK(sd_bus_wait(s.bus,10000)>=0);
    }
    CHECK(status.reply_confirmed && status.job_removed && !status.pending && status.job_id && strcmp(status.result,"done")==0 && s.notices>=1 && quiet(&s,"active"));
    file_check(&s); check_graph(&s); CHECK(ae_peer_alive(&peer)==0);
    printf("actual PID1 queued job %u %s result %s; two guards, callback EBUSY and process-free active target verified\n",status.job_id,status.job,status.result);
    CHECK(ae_start_request(s.start,admit,&s)==-EALREADY && ae_start_close(&s.start)==0);
    /* Administrative cleanup addresses only the exact owned inert target. It
     * is not executing-app drain or a catalogue lease-release protocol. */
    CHECK(sd_bus_call_method(s.bus,MANAGER,PATH,MANAGER ".Manager","StopUnit",NULL,NULL,"ss",s.name,"fail")>=0);
    int stopped=0; for (unsigned int n=0;n<200 && !stopped;++n) { stopped=quiet(&s,"inactive"); if (!stopped) usleep(10000); }
    CHECK(stopped); file_check(&s); check_graph(&s);
    CHECK(sd_bus_call_method(s.bus,MANAGER,PATH,MANAGER ".Manager","UnrefUnit",NULL,NULL,"s",s.name)>=0);
    CHECK(sd_bus_call_method(s.bus,MANAGER,PATH,MANAGER ".Manager","Unsubscribe",NULL,NULL,"")>=0);
    file_check(&s); CHECK(unlinkat(s.directory,s.name,0)==0 && fsync(s.directory)==0 && close(s.directory)==0);
    ae_peer_clear(&peer); sd_bus_flush_close_unref(s.bus); CHECK(descriptors()==baseline);
    puts("actual PID1 observer retired; original inert target inactive/no-job and exact owned file removed; no app execution or lease release"); return 0;
}
