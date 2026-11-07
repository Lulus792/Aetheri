#include "authority/unit.h"
#include "core/digest.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MANAGER "org.freedesktop.systemd1"
#define PATH "/org/freedesktop/systemd1"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"root unit %s line %d: %s errno=%d\n",s.mode,__LINE__,#x,errno); exit(1); } } while (0)
static struct {
    const char *mode,*fault,*directory;
    unsigned int effects,guards,faults,ends;
    int fault_enabled,after;
    struct ae_root_unit *root;
    struct ae_catalogue *store;
    struct ae_execution_profile *profile;
    struct ae_launch_context *broker;
    struct ae_root_unit_image *source;
    unsigned int links;
    int create_fault;
    struct ae_unit_file created;
} s;
void ae_test_root_unit_commit_fault(int);
void ae_test_root_unit_close_gate(const char *);
int __real_linkat(int,const char *,int,const char *,int);
int __wrap_linkat(int fd,const char *old,int parent,const char *name,int flags)
{
    if (!s.create_fault) return __real_linkat(fd,old,parent,name,flags);
    ++s.links;
    if (s.create_fault==1) { errno=EIO; return -1; }
    int r=__real_linkat(fd,old,parent,name,flags);
    if (!r) {
        CHECK(!statx(fd,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,STATX_BASIC_STATS|STATX_MNT_ID_UNIQUE|STATX_BTIME,&s.created.object));
        CHECK(!ae_sha256_fd(fd,s.created.object.stx_size,s.created.digest));
        errno=EIO; return -1;
    }
    return r;
}
int __real_sd_bus_call(sd_bus *,sd_bus_message *,uint64_t,sd_bus_error *,sd_bus_message **);
int __wrap_sd_bus_call(sd_bus *bus,sd_bus_message *q,uint64_t timeout,sd_bus_error *error,sd_bus_message **out)
{
    int chosen=s.fault_enabled && s.fault && sd_bus_message_is_method_call(q,MANAGER ".Manager",s.fault);
    if (chosen) { ++s.faults; if (!s.after) return -EIO; }
    int r=__real_sd_bus_call(bus,q,timeout,error,out);
    return chosen && r>=0 ? -EIO : r;
}
int __real_sd_bus_call_async(sd_bus *,sd_bus_slot **,sd_bus_message *,sd_bus_message_handler_t,void *,uint64_t);
int __wrap_sd_bus_call_async(sd_bus *bus,sd_bus_slot **slot,sd_bus_message *q,sd_bus_message_handler_t fn,void *data,uint64_t timeout)
{
    if (sd_bus_message_is_method_call(q,MANAGER ".Manager","StartUnit")) ++s.effects;
    return __real_sd_bus_call_async(bus,slot,q,fn,data,timeout);
}
static int fds(void)
{ DIR *d=opendir("/proc/self/fd"); CHECK(d); int n=0; while (readdir(d)) ++n; CHECK(!closedir(d)); return n; }
static void connect_bus(sd_bus **out)
{ CHECK(sd_bus_new(out)>=0 && sd_bus_set_address(*out,"unix:path=/run/dbus/system_bus_socket")>=0 && sd_bus_set_bus_client(*out,1)>=0 && sd_bus_start(*out)>=0); }
static void manager(sd_bus *bus,struct ae_peer *out)
{
    sd_bus_creds *creds=NULL; const char *name=NULL;
    CHECK(sd_bus_get_name_creds(bus,MANAGER,SD_BUS_CREDS_UNIQUE_NAME,&creds)>=0 && sd_bus_creds_get_unique_name(creds,&name)>=0);
    sd_bus_message *q=NULL,*reply=NULL;
    CHECK(sd_bus_message_new_method_call(bus,&q,name,PATH,"org.freedesktop.DBus.Peer","Ping")>=0 && sd_bus_call(bus,q,1000000,NULL,&reply)>=0);
    CHECK(!ae_peer_capture_reply(reply,q,out) && out->connection_pid==1 && !out->connection_uid);
    sd_bus_message_unref(reply); sd_bus_message_unref(q); sd_bus_creds_unref(creds);
}
static int ending(void *);
static void reentry(void)
{
    struct ae_root_unit *same=s.root; CHECK(ae_root_unit_close(&same)==-EBUSY && same==s.root);
    CHECK(ae_root_unit_abandon(&same)==-EBUSY && same==s.root);
    struct ae_catalogue *store=s.store; CHECK(ae_catalogue_close(&store)==-EBUSY && store==s.store);
    CHECK(ae_root_unit_retire(s.root,ending,NULL)==-EBUSY);
}
static int admit(void *data,const struct ae_execution_profile *p)
{
    (void)data; CHECK(p==s.profile || !memcmp(p,s.profile,sizeof *p)); ++s.guards; reentry();
    if (!strcmp(s.mode,"guard-image")) CHECK(!fchmod(s.source->image_fd,0755));
    if (!strcmp(s.mode,"late-fence")) { struct ae_execution_fence_status status; CHECK(!ae_catalogue_fence_execution(s.store,&status)); }
    if (!strcmp(s.mode,"last-gate") && s.guards==12) ae_test_root_unit_close_gate(s.directory);
    return !strcmp(s.mode,"guard-deny") ? -EPERM : 0;
}
static int create_admit(void *data,const struct ae_execution_profile *p)
{
    (void)data; CHECK(!memcmp(p,s.profile,sizeof *p));
    struct ae_catalogue *same=s.store; CHECK(ae_catalogue_close(&same)==-EBUSY && same==s.store);
    if (!strcmp(s.mode,"create-fence")) { struct ae_execution_fence_status status; CHECK(!ae_catalogue_fence_execution(s.store,&status)); }
    return 0;
}
static void retire_image(struct ae_root_unit_image *image,const uint8_t expected[32])
{
    struct stat st; uint8_t hash[32];
    CHECK(!fstatat(image->directory_fd,image->leaf,&st,AT_SYMLINK_NOFOLLOW) && (uint64_t)st.st_ino==image->image.inode);
    CHECK(!ae_sha256_fd(image->image_fd,image->image.size,hash) && !memcmp(hash,expected,32));
    CHECK(!unlinkat(image->directory_fd,image->leaf,0) && !fsync(image->directory_fd));
    close(image->image_fd); close(image->directory_fd); CHECK(!rmdir(image->directory));
}
static int ending(void *data)
{
    (void)data; ++s.ends; reentry();
    struct ae_execution_fence_status status; CHECK(!ae_catalogue_execution_fence_status(s.store,&status) && status.active>=2);
    return 0; /* ONLY the exact inert administrative true helper in this test. */
}
static int deny(void *data) { (void)data; ++s.ends; return -EPERM; }
static int release(void *data,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{ (void)data; (void)p; (void)d; CHECK(0); return -ENOTSUP; }
static void decode(const char *text,uint8_t out[32])
{
    const char digits[]="0123456789abcdef";
    for (unsigned int i=0;i<32;++i) { const char *a=strchr(digits,text[2*i]),*b=strchr(digits,text[2*i+1]); CHECK(a && b); out[i]=(uint8_t)((a-digits)*16+(b-digits)); }
}
void ae_test_unit_codec(const char *path)
{
    s.mode="codec"; uint8_t bytes[480],digest[32],marker[32]; memset(marker,0xa5,32);
    FILE *file=fopen(path,"rb"); CHECK(file && fread(bytes,1,sizeof bytes,file)==sizeof bytes && fgetc(file)==EOF && !fclose(file));
    struct ae_execution_profile execution; CHECK(!ae_execution_decode(bytes,384,&execution));
    struct ae_unit_profile *p=calloc(1,sizeof *p); CHECK(p); p->count=3;
    struct ae_unit_node *n=&p->nodes[0]; n->kind=AE_UNIT_SERVICE;
    char launch[33]; static const char hex[]="0123456789abcdef";
    for (unsigned int i=0;i<16;++i) { launch[2*i]=hex[execution.launch[i]>>4]; launch[2*i+1]=hex[execution.launch[i]&15]; } launch[32]=0;
    CHECK(snprintf(n->name,sizeof n->name,"aetheri-app-%s.service",launch)>0);
    strcpy(n->directory,"/run/systemd/system"); n->directory_fd=42; n->file_fd=43;
    n->parent=(struct ae_root_identity){.mount_id=101,.inode=201,.device_major=8,.device_minor=1,.mode=S_IFDIR|0700,
        .filesystem_type=0xef53,.birth_present=1,.birth_seconds=1000,.birth_nanoseconds=23};
    for (unsigned int i=0;i<8;++i) n->parent.filesystem_id[i]=(uint8_t)(i+1);
    n->file.object=(struct statx){.stx_mask=STATX_BASIC_STATS|STATX_MNT_ID_UNIQUE|STATX_BTIME,.stx_mnt_id=101,.stx_ino=301,
        .stx_dev_major=8,.stx_dev_minor=1,.stx_mode=S_IFREG|0444,.stx_nlink=1,.stx_size=1234,
        .stx_mtime={.tv_sec=2000,.tv_nsec=5},.stx_ctime={.tv_sec=2001,.tv_nsec=6},.stx_btime={.tv_sec=1001,.tv_nsec=7}};
    memset(n->file.digest,0x31,32); n->dropin_count=1;
    struct ae_unit_dropin *d=&n->dropins[0]; strcpy(d->directory,"/usr/lib/systemd/system/service.d"); strcpy(d->leaf,"10-test.conf");
    d->directory_fd=44; d->file_fd=45; d->parent=n->parent; d->parent.inode=202; d->parent.mount_id=102;
    d->file=n->file; d->file.object.stx_ino=401; d->file.object.stx_mnt_id=102; memset(d->file.digest,0x32,32);
    strcpy(n->executable,"/unit/image/starter"); strcpy(n->argv[0],n->executable); strcpy(n->argv[1],"--fixed"); n->argc=2;
    n->envc=1; strcpy(n->environment[0],"ONE=1"); strcpy(n->label,"system_u:system_r:test_starter_t:s0"); n->capabilities=0x1234; n->no_new_privileges=1; n->umask=0077;
    n->edges[AE_UNIT_REQUIRES]=n->edges[AE_UNIT_AFTER]=2;
    p->nodes[1].kind=AE_UNIT_SYSTEM_SLICE; strcpy(p->nodes[1].name,"system.slice"); p->nodes[1].edges[AE_UNIT_REQUIRES]=p->nodes[1].edges[AE_UNIT_AFTER]=4;
    p->nodes[2].kind=AE_UNIT_ROOT_SLICE; strcpy(p->nodes[2].name,"-.slice");
    struct ae_root_unit_image source={.directory_fd=46,.image_fd=47,.parent=n->parent,
        .image={.inode=501,.size=4096,.device_major=8,.device_minor=1,.mode=S_IFREG|0555,.birth_seconds=1002,.birth_ns=3,
                .mtime_seconds=2002,.mtime_ns=4,.ctime_seconds=2003,.ctime_ns=5}};
    source.parent.inode=203; source.parent.mount_id=103;
    strcpy(source.directory,"/unit/image"); strcpy(source.leaf,"starter"); strcpy(source.label,"system_u:object_r:bin_t:s0"); memcpy(source.image.digest,execution.starter_digest,32);
    CHECK(!ae_unit_profile_digest(execution.boot,p,digest) && !memcmp(digest,bytes+384,32));
    CHECK(!ae_root_unit_selection_digest(&execution,p,&source,digest) && !memcmp(digest,bytes+416,32));
    CHECK(!ae_root_unit_placement_digest(n->directory,&n->parent,digest) && !memcmp(digest,bytes+448,32));
    n->file_fd=100; n->directory_fd=101; source.image_fd=102; source.directory_fd=103;
    CHECK(!ae_root_unit_selection_digest(&execution,p,&source,digest) && !memcmp(digest,bytes+416,32));
    ++n->file.object.stx_btime.tv_nsec;
    CHECK(!ae_root_unit_selection_digest(&execution,p,&source,digest) && memcmp(digest,bytes+416,32)); --n->file.object.stx_btime.tv_nsec;
    n->file.object.stx_mask&=~STATX_BTIME; memcpy(digest,marker,32);
    CHECK(ae_root_unit_selection_digest(&execution,p,&source,digest)==-EINVAL && !memcmp(digest,marker,32)); n->file.object.stx_mask|=STATX_BTIME;
    source.parent.birth_present=0;
    CHECK(ae_root_unit_selection_digest(&execution,p,&source,digest)==-EINVAL && !memcmp(digest,marker,32)); source.parent.birth_present=1;
    ++source.image.digest[0]; CHECK(ae_root_unit_selection_digest(&execution,p,&source,digest)==-ESTALE && !memcmp(digest,marker,32)); --source.image.digest[0];
    p->count=AE_UNIT_MAX+1; CHECK(ae_root_unit_selection_digest(&execution,p,&source,digest)==-EINVAL && !memcmp(digest,marker,32));
    free(p); puts("unit selection codec: independent frozen bytes, FD independence, birth/drift and error outputs passed");
}
static int terminal(sd_bus *bus,const char *name)
{
    sd_bus_message *m=NULL; const char *path=NULL; char *state=NULL; uint32_t pid=1;
    CHECK(sd_bus_call_method(bus,MANAGER,PATH,MANAGER ".Manager","GetUnit",NULL,&m,"s",name)>=0 && sd_bus_message_read(m,"o",&path)>0);
    CHECK(sd_bus_get_property_string(bus,MANAGER,path,MANAGER ".Unit","ActiveState",NULL,&state)>=0);
    int ended=!strcmp(state,"inactive"); free(state);
    CHECK(sd_bus_get_property_trivial(bus,MANAGER,path,MANAGER ".Service","MainPID",NULL,'u',&pid)>=0);
    sd_bus_message_unref(m); return ended && !pid;
}
void ae_test_root_unit(struct ae_catalogue *store,struct ae_catalogue *user,struct ae_execution_profile *p,
    struct ae_launch_context *broker,const char *directory,const char *mode)
{
    s.mode=mode; s.store=store; s.profile=p; s.broker=broker; s.directory=directory;
    CHECK(!getuid() && !geteuid() && !getgid() && !getegid() && security_getenforce()==1);
    struct stat carrier; CHECK(!lstat("/etc/aetheri-p0-carrier",&carrier) && S_ISREG(carrier.st_mode) && !carrier.st_uid && !(carrier.st_mode&0022));
    FILE *f=fopen("/sys/class/dmi/id/product_name","r"); char product[128]={0};
    CHECK(f && fgets(product,sizeof product,f) && !fclose(f) && !strcmp(product,"Aetheri P0 disposable VM\n"));
    CHECK(!stat("/sys/firmware/efi",&carrier));
    int baseline=fds();
    struct ae_root_unit_image image={.directory_fd=-1,.image_fd=-1}; s.source=&image;
    CHECK(snprintf(image.directory,sizeof image.directory,"%s/admin",directory)>0 && !mkdir(image.directory,0700));
    image.directory_fd=open(image.directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); CHECK(image.directory_fd>=0);
    CHECK(!ae_root_observe(image.directory_fd,&image.parent)); strcpy(image.leaf,"true");
    strcpy(image.label,"system_u:object_r:bin_t:s0");
    int input=open("/usr/bin/true",O_RDONLY|O_NOFOLLOW|O_CLOEXEC),output=openat(image.directory_fd,image.leaf,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    CHECK(input>=0 && output>=0); uint8_t bytes[65536]; ssize_t amount;
    while ((amount=read(input,bytes,sizeof bytes))>0) CHECK(write(output,bytes,(size_t)amount)==amount);
    CHECK(!amount && !close(input) && !fsetfilecon_raw(output,image.label) && !fchmod(output,0555) && !fsync(output) && !close(output) && !fsync(image.directory_fd));
    image.image_fd=openat(image.directory_fd,image.leaf,O_RDONLY|O_NOFOLLOW|O_CLOEXEC); CHECK(image.image_fd>=0);
    struct stat st; CHECK(!fstat(image.image_fd,&st) && st.st_size>0);
    CHECK(!ae_sha256_fd(image.image_fd,(uint64_t)st.st_size,p->starter_digest) && !ae_image_verify(image.image_fd,p->starter_digest,&image.image));
    struct ae_user_execution_result prepared; CHECK(!ae_catalogue_prepare_user_execution(user,p,&prepared) && prepared.submit_required);
    CHECK(!ae_catalogue_upgrade_lifetimes(store) && !ae_catalogue_upgrade_processes(store) && !ae_catalogue_upgrade_handoffs(store) &&
        !ae_catalogue_upgrade_recovery(store) && !ae_catalogue_upgrade_owner_recovery(store) && !ae_catalogue_upgrade_interruptions(store) && !ae_catalogue_upgrade_units(store));
    struct ae_unit_profile *selection=calloc(1,sizeof *selection); CHECK(selection); selection->count=3;
    struct ae_unit_node *n=&selection->nodes[0]; n->kind=AE_UNIT_SERVICE;
    struct ae_dispatch_snapshot dispatch; CHECK(!ae_catalogue_get_dispatch(store,p->lease,broker,&dispatch)); strcpy(n->name,dispatch.unit);
    strcpy(n->directory,"/run/systemd/system"); n->directory_fd=open(n->directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); CHECK(n->directory_fd>=0);
    CHECK(!ae_root_observe(n->directory_fd,&n->parent));
    CHECK(snprintf(n->executable,sizeof n->executable,"%s/%s",image.directory,image.leaf)>0);
    CHECK(strlen(n->executable)<sizeof n->argv[0]); strcpy(n->argv[0],n->executable); n->argc=1;
    strcpy(n->label,"system_u:system_r:unconfined_service_t:s0"); n->no_new_privileges=1; n->umask=0077;
    char text[8192]; int size=snprintf(text,sizeof text,
        "[Unit]\nDescription=Aetheri Root custody administrative true test\nDefaultDependencies=no\n"
        "[Service]\nType=exec\nExitType=main\nUser=root\nGroup=root\nRestart=no\nKillMode=control-group\nSlice=system.slice\n"
        "SELinuxContext=system_u:system_r:unconfined_service_t:s0\nNoNewPrivileges=yes\nPrivateMounts=no\nPrivateNetwork=no\nPrivateDevices=no\n"
        "FileDescriptorStoreMax=0\nFileDescriptorStorePreserve=no\nStandardInput=null\nStandardOutput=null\nStandardError=null\nCapabilityBoundingSet=\nAmbientCapabilities=\nUMask=0077\nExecStart=%s\n",n->executable);
    CHECK(size>0 && size<(int)sizeof text);
    struct ae_unit_allocation allocation={.prepared_unix_ms=123500}; sd_id128_t token; CHECK(sd_id128_randomize(&token)>=0);
    memcpy(allocation.id,token.bytes,16); allocation.id[6]=(uint8_t)((allocation.id[6]&15U)|64U); allocation.id[8]=(uint8_t)((allocation.id[8]&63U)|128U);
    CHECK(!ae_sha256_bytes((const uint8_t *)text,(size_t)size,allocation.recipe));
    CHECK(!ae_root_unit_placement_digest(n->directory,&n->parent,allocation.placement));
    struct ae_root_unit_create creation={.directory_fd=n->directory_fd,.directory=n->directory,.parent=n->parent,.bytes=(const uint8_t *)text,.size=(size_t)size};
    n->file_fd=-1;
    int factory=!strncmp(mode,"create-",7);
    if (!strcmp(mode,"create-before")) s.create_fault=1;
    if (!strcmp(mode,"create-after")) s.create_fault=2;
    if (!strcmp(mode,"create-lost")) ae_test_root_unit_commit_fault(2);
    if (!strcmp(mode,"create-recipe")) ++allocation.recipe[0];
    if (!strcmp(mode,"create-placement")) ++allocation.placement[0];
    struct ae_unit_file factory_marker; memset(&factory_marker,0xa5,sizeof factory_marker); n->file=factory_marker;
    int created=ae_root_unit_create(store,p,broker,&allocation,&creation,create_admit,NULL,&n->file_fd,&n->file);
    if (factory) {
        CHECK(created==(!strcmp(mode,"create-recipe") ? -EBADMSG : !strcmp(mode,"create-placement") ? -ESTALE : !strcmp(mode,"create-fence") ? -ESHUTDOWN : -EIO));
        CHECK(n->file_fd==-1 && !memcmp(&n->file,&factory_marker,sizeof factory_marker));
        struct ae_unit_allocation_snapshot snap; struct ae_effect_owner own;
        memcpy(own.epoch,p->system_epoch,16); memcpy(own.runtime,p->system_runtime,16); memcpy(own.boot,p->boot,16);
        int got=ae_catalogue_get_unit(store,p,&own,&snap);
        CHECK((!strcmp(mode,"create-recipe") || !strcmp(mode,"create-placement")) ? got==-ENOENT : !got && !snap.witnessed);
        unsigned int original_links=s.links;
        CHECK(ae_root_unit_create(store,p,broker,&allocation,&creation,create_admit,NULL,&n->file_fd,&n->file)==
            (!strcmp(mode,"create-recipe") ? -EBADMSG : !strcmp(mode,"create-placement") ? -ESTALE : !strcmp(mode,"create-fence") ? -ESHUTDOWN : -EALREADY));
        CHECK(s.links==original_links && !s.effects);
        s.create_fault=0;
        if (!strcmp(mode,"create-after")) {
            int held=openat(n->directory_fd,n->name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC); CHECK(held>=0);
            struct ae_unit_file actual; CHECK(!ae_unit_file_capture(held,allocation.recipe,&actual) && ae_unit_file_equal(&actual,&s.created));
            CHECK(!unlinkat(n->directory_fd,n->name,0) && !fsync(n->directory_fd)); close(held);
        } else { struct stat absent; CHECK(fstatat(n->directory_fd,n->name,&absent,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT); }
        char path[4096]; CHECK(snprintf(path,sizeof path,"%s/factory-receipt.txt",directory)>0); FILE *receipt=fopen(path,"wx"); CHECK(receipt);
        CHECK(fprintf(receipt,"unit=%s\nimage=%s/%s\nlinks=%u\nno_target_submission=1\n",n->name,image.directory,image.leaf,s.links)>0 && !fflush(receipt) && !fsync(fileno(receipt)) && !fclose(receipt));
        retire_image(&image,p->starter_digest); close(n->directory_fd); free(selection); CHECK(fds()==baseline);
        printf("Root unit %s: original allocation intent retained, no retry/adoption/start after uncertainty\n",mode); return;
    }
    CHECK(!created && n->file_fd>=0);
    int rejected=-1; struct ae_unit_file file_marker=n->file;
    CHECK(ae_root_unit_create(store,p,broker,&allocation,&creation,create_admit,NULL,&rejected,&file_marker)==-EALREADY && rejected==-1 && ae_unit_file_equal(&file_marker,&n->file));
    uint8_t hash[32]; memcpy(hash,allocation.recipe,32);
    CHECK(snprintf(text,sizeof text,"%s/admin-receipt.txt",directory)>0); f=fopen(text,"wx"); CHECK(f);
    CHECK(fprintf(f,"unit=%s\nunit_inode=%llu\nunit_mount=%llu\nimage=%s\nimage_inode=%llu\nimage_sha256=",n->name,
        (unsigned long long)n->file.object.stx_ino,(unsigned long long)n->file.object.stx_mnt_id,n->executable,(unsigned long long)image.image.inode)>0);
    for (unsigned int i=0;i<32;++i) CHECK(fprintf(f,"%02x",p->starter_digest[i])==2);
    CHECK(fprintf(f,"\n")>0 && !fflush(f) && !fsync(fileno(f)) && !fclose(f));
    n->dropin_count=1; struct ae_unit_dropin *drop=&n->dropins[0];
    strcpy(drop->directory,"/usr/lib/systemd/system/service.d"); strcpy(drop->leaf,"10-timeout-abort.conf");
    drop->directory_fd=open(drop->directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); CHECK(drop->directory_fd>=0 && !ae_root_observe(drop->directory_fd,&drop->parent));
    drop->file_fd=openat(drop->directory_fd,drop->leaf,O_RDONLY|O_NOFOLLOW|O_CLOEXEC); CHECK(drop->file_fd>=0);
    decode("ae6b234f92bc22f1201a7572b59b454c9809f33c80d13f361b9674e1801acc37",hash); CHECK(!ae_unit_file_capture(drop->file_fd,hash,&drop->file));
    selection->nodes[1].kind=AE_UNIT_SYSTEM_SLICE; strcpy(selection->nodes[1].name,"system.slice");
    selection->nodes[2].kind=AE_UNIT_ROOT_SLICE; strcpy(selection->nodes[2].name,"-.slice");
    n->edges[AE_UNIT_REQUIRES]=n->edges[AE_UNIT_AFTER]=2;
    selection->nodes[1].edges[AE_UNIT_REQUIRES]=selection->nodes[1].edges[AE_UNIT_AFTER]=4;
    sd_bus *bus=NULL,*control=NULL; connect_bus(&bus); connect_bus(&control); struct ae_peer pid1=AE_PEER_INIT; manager(bus,&pid1);
    struct ae_unit *unit=NULL; CHECK(!ae_unit_new(&bus,&pid1,p->boot,selection,&unit) && !bus); ae_peer_clear(&pid1);
    n->ambient=1; CHECK(ae_root_unit_new(store,&unit,selection,&image,p,broker,admit,NULL,&s.root)==-ESTALE && unit && !s.root); n->ambient=0;
    image.image.digest[0]^=1; CHECK(ae_root_unit_new(store,&unit,selection,&image,p,broker,admit,NULL,&s.root)==-ESTALE && unit && !s.root); image.image.digest[0]^=1;
    CHECK(ae_root_unit_new(store,&unit,selection,&image,p,broker,admit,NULL,&s.root)==-ESTALE && unit && !s.root);
    uint8_t selected[32]; CHECK(!ae_root_unit_selection_digest(p,selection,&image,selected));
    CHECK(!ae_catalogue_witness_unit(store,p,broker,&allocation,selected,123501));
    CHECK(!ae_root_unit_new(store,&unit,selection,&image,p,broker,admit,NULL,&s.root) && !unit);
    if (!strcmp(mode,"copy")) { strcpy(n->argv[0],"changed"); strcpy(image.label,"changed"); }
    if (!strcmp(mode,"precommit")) {
        CHECK(ae_root_unit_request(s.root,p,&dispatch)==-ESTALE && !s.effects);
        struct ae_root_unit_status status; CHECK(!ae_root_unit_status(s.root,&status) && !status.held && !status.unit.subscribe_attempted);
        CHECK(!ae_root_unit_close(&s.root));
    } else {
        sd_event *event=NULL; CHECK(sd_event_new(&event)>=0);
        struct ae_execution_effects effects={.owner=s.root,.request=ae_root_unit_request,.release=release};
        struct ae_execution_service *service=NULL; CHECK(!ae_execution_service_new(store,control,event,p,broker,&effects,&service));
        if (!strcmp(mode,"commit-lost")) ae_test_root_unit_commit_fault(2);
        if (!strcmp(mode,"image")) CHECK(!fchmod(image.image_fd,0755));
        s.after=strstr(mode,"after")!=NULL;
        int release_fault=!strncmp(mode,"unref-",6) || !strncmp(mode,"unsubscribe-",12);
        if (!strncmp(mode,"subscribe-",10)) s.fault="Subscribe";
        if (!strncmp(mode,"ref-",4)) s.fault="RefUnit";
        if (release_fault) s.fault=!strncmp(mode,"unref-",6) ? "UnrefUnit" : "Unsubscribe";
        int acquisition_fault=s.fault && !release_fault;
        s.fault_enabled=acquisition_fault;
        int denied=!strcmp(mode,"guard-deny") || !strcmp(mode,"late-fence") || !strcmp(mode,"image") || !strcmp(mode,"guard-image");
        int lost=!strcmp(mode,"commit-lost");
        int blocked_start=!strcmp(mode,"last-gate");
        struct ae_execution_result result,marker; memset(&marker,0xa5,sizeof marker); result=marker;
        int r=ae_execution_service_request(service,broker,&result);
        CHECK(r==(lost || s.fault_enabled ? -EIO : !strcmp(mode,"guard-deny") ? -EPERM : !strcmp(mode,"late-fence") ? -ESHUTDOWN : blocked_start ? -ESTALE : denied ? -EACCES : 0));
        if (r) CHECK(!memcmp(&result,&marker,sizeof result));
        CHECK(ae_execution_service_request(service,broker,&result)==-EALREADY);
        s.fault_enabled=0;
        struct ae_root_unit_status status; CHECK(!ae_root_unit_status(s.root,&status));
        int acquired=!denied && !lost;
        CHECK(status.held==acquired && s.effects==(acquired && !acquisition_fault && !blocked_start ? 1U : 0U));
        if (blocked_start) CHECK(s.guards==12 && status.unit.referenced==7 && !status.unit.start.attempted);
        struct ae_execution_fence_status fence; CHECK(!ae_catalogue_execution_fence_status(store,&fence) && fence.active==(acquired ? 1U : 0U));
        if (lost) { struct ae_root_unit *other=NULL; struct ae_unit *again=NULL; connect_bus(&bus); manager(bus,&pid1);
            CHECK(!ae_unit_new(&bus,&pid1,p->boot,selection,&again)); ae_peer_clear(&pid1);
            CHECK(ae_root_unit_new(store,&again,selection,&image,p,broker,admit,NULL,&other)==-EALREADY && !other && again);
            CHECK(!ae_unit_close(&again)); }
        if (acquired && !acquisition_fault) {
            for (unsigned int i=0;!blocked_start && i<500;++i) { CHECK(sd_bus_process(ae_root_unit_bus(s.root),NULL)>=0 && !ae_root_unit_status(s.root,&status));
                if (status.unit.start.job_removed) break;
                CHECK(sd_bus_wait(ae_root_unit_bus(s.root),10000)>=0); }
            CHECK((blocked_start || (status.unit.start.reply_confirmed && status.unit.start.job_removed && !strcmp(status.unit.start.result,"done"))) && status.held);
            int ended=0; for (unsigned int i=0;i<200 && !ended;++i) { ended=terminal(control,dispatch.unit); if (!ended) usleep(10000); } CHECK(ended);
            struct ae_catalogue *same=store; CHECK(ae_catalogue_close(&same)==-EBUSY && same==store);
            uint8_t runtime[16]; memcpy(runtime,p->system_runtime,16); ++runtime[15]; CHECK(ae_catalogue_begin_runtime(store,p->boot,runtime)==-EBUSY);
            CHECK(ae_root_unit_close(&s.root)==-EBUSY && s.root);
            if (!strcmp(mode,"fork")) { pid_t child=fork(); CHECK(child>=0); if (!child) { CHECK(ae_root_unit_abandon(&s.root)==-ECHILD); _exit(0); }
                int code; CHECK(waitpid(child,&code,0)==child && WIFEXITED(code) && !WEXITSTATUS(code)); }
            if (!strcmp(mode,"fenced")) CHECK(!ae_catalogue_fence_execution(store,&fence));
            if (!strcmp(mode,"end-deny")) CHECK(ae_root_unit_retire(s.root,deny,NULL)==-EPERM && s.root);
            s.fault_enabled=release_fault;
            CHECK(ae_root_unit_retire(s.root,ending,NULL)==(release_fault ? -EIO : 0));
            s.fault_enabled=0;
            CHECK(!ae_root_unit_status(s.root,&status) && status.held==release_fault);
            if (release_fault) { unsigned int count=s.faults; CHECK(ae_root_unit_retire(s.root,ending,NULL)==-EAGAIN && s.faults==count); }
            else CHECK(status.unit.retired && !status.held);
        }
        if (acquired && s.fault) { CHECK(ae_root_unit_close(&s.root)==-EBUSY); CHECK(!ae_root_unit_abandon(&s.root)); }
        else CHECK(!ae_root_unit_close(&s.root));
        CHECK(!ae_catalogue_execution_fence_status(store,&fence) && !fence.active);
        CHECK(!ae_execution_service_close(&service)); sd_event_unref(event);
    }
    CHECK(!s.root);
    struct ae_dispatch_snapshot final; CHECK(!ae_catalogue_get_dispatch(store,p->lease,broker,&final) && !strcmp(final.state,"reserved") && !final.invocation_present);
    /* Inspect exact owned administrative source objects before retirement. This
     * does not settle the synthetic REQUEST/user preparation/lease journals. */
    struct ae_unit_file file; CHECK(!ae_unit_file_capture(n->file_fd,n->file.digest,&file) && file.object.stx_ino==n->file.object.stx_ino);
    CHECK(!fstatat(n->directory_fd,n->name,&st,AT_SYMLINK_NOFOLLOW) && (uint64_t)st.st_ino==n->file.object.stx_ino);
    CHECK(!unlinkat(n->directory_fd,n->name,0) && !fsync(n->directory_fd));
    retire_image(&image,p->starter_digest);
    close(drop->file_fd); close(drop->directory_fd); close(n->file_fd); close(n->directory_fd); free(selection);
    sd_bus_flush_close_unref(control); CHECK(fds()==baseline);
    printf("Root unit %s: original REQUEST/custody held through job, one-shot effects and unchanged reserved lease\n",mode);
}
