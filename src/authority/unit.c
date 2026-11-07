#include "authority/unit.h"
#include "linux/unit_bytes.h"
#include "core/digest.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <selinux/selinux.h>
#include <unistd.h>

struct ae_root_unit {
    pid_t owner;
    int busy,used,held,error;
    struct ae_catalogue *catalogue;
    struct ae_unit *unit;
    struct ae_execution_profile profile;
    struct ae_launch_context broker;
    struct ae_root_unit_image source;
    uint8_t digest[32],selection[32];
    char name[80];
    ae_root_unit_admit admit;
    void *admission;
};
static int owner(struct ae_root_unit *s) { return !s ? -EINVAL : s->owner==getpid() ? 0 : -ECHILD; }
int ae_root_unit_placement_digest(const char *directory,const struct ae_root_identity *parent,uint8_t out[32])
{
    if (!directory || directory[0]!='/' || strnlen(directory,4096)==4096 || !parent || !out) return -EINVAL;
    uint8_t raw[8192]; struct ae_unit_bytes b={.data=raw,.capacity=sizeof raw};
    ae_ub_bytes(&b,"AETHUPL1",8); ae_ub_text(&b,directory); ae_ub_root(&b,parent);
    return b.error ? b.error : ae_sha256_bytes(raw,b.used,out);
}
int ae_root_unit_selection_digest(const struct ae_execution_profile *p,const struct ae_unit_profile *units,
    const struct ae_root_unit_image *source,uint8_t out[32])
{
    if (!p || !source || !out || !memchr(source->directory,0,sizeof source->directory) || source->directory[0]!='/' ||
        !memchr(source->leaf,0,sizeof source->leaf) || !source->leaf[0] || strchr(source->leaf,'/') ||
        !strcmp(source->leaf,".") || !strcmp(source->leaf,"..") || !memchr(source->label,0,sizeof source->label) || !source->label[0]) return -EINVAL;
    uint8_t raw[8192],execution[32],unit[32],image_raw[112],image_digest[32],selection[104];
    int r=ae_execution_digest(p,execution);
    if (!r) r=ae_unit_profile_digest(p->boot,units,unit);
    if (!r) r=ae_image_encode(&source->image,image_raw);
    if (!r && memcmp(p->starter_digest,source->image.digest,32)) r=-ESTALE;
    struct ae_unit_bytes b={.data=raw,.capacity=sizeof raw};
    if (!r) {
        ae_ub_bytes(&b,"AETHUIM1",8); ae_ub_text(&b,source->directory); ae_ub_text(&b,source->leaf); ae_ub_text(&b,source->label);
        ae_ub_root(&b,&source->parent); ae_ub_bytes(&b,image_raw,112);
        r=b.error ? b.error : ae_sha256_bytes(raw,b.used,image_digest);
    }
    if (!r) {
        memcpy(selection,"AETHUNT1",8); memcpy(selection+8,execution,32); memcpy(selection+40,unit,32); memcpy(selection+72,image_digest,32);
        r=ae_sha256_bytes(selection,sizeof selection,out);
    }
    return r;
}
int ae_root_unit_create(struct ae_catalogue *store,const struct ae_execution_profile *profile,const struct ae_launch_context *broker,
    const struct ae_unit_allocation *allocation,const struct ae_root_unit_create *input,ae_root_unit_admit admit,void *data,
    int *output,struct ae_unit_file *witness)
{
    if (!store || !profile || !broker || !allocation || !input || !admit || !output || *output>=0 || !witness ||
        input->directory_fd<0 || !input->directory || input->directory[0]!='/' || strnlen(input->directory,4096)==4096 ||
        !input->bytes || !input->size || input->size>AE_UNIT_FILE_MAX || memchr(input->bytes,0,input->size)) return -EINVAL;
    if (getuid() || geteuid() || getgid() || getegid()) return -EPERM;
    const struct ae_execution_profile p=*profile; const struct ae_launch_context c=*broker;
    const struct ae_unit_allocation a=*allocation; const struct ae_root_identity parent=input->parent;
    char directory[4096]; strcpy(directory,input->directory);
    uint8_t bytes[AE_UNIT_FILE_MAX],hash[32]; size_t size=input->size; memcpy(bytes,input->bytes,size);
    int r=ae_sha256_bytes(bytes,size,hash);
    if (!r && memcmp(hash,a.recipe,32)) r=-EBADMSG;
    if (!r) r=ae_root_unit_placement_digest(directory,&parent,hash);
    if (!r && memcmp(hash,a.placement,32)) r=-ESTALE;
    int dir=-1,writer=-1,reader=-1;
    if (!r) { dir=fcntl(input->directory_fd,F_DUPFD_CLOEXEC,3); if (dir<0) r=-errno; }
    if (!r) r=ae_root_check_path(dir,directory,&parent);
    if (!r) r=ae_catalogue_begin_execution_effect(store,&c);
    int frame=!r,create=0;
    if (!r) r=ae_catalogue_prepare_unit(store,&p,&c,&a,&create);
    if (!r && !create) r=-EALREADY;
    if (!r) r=admit(data,&p);
    if (r>0) r=-EIO;
    if (!r) r=ae_catalogue_check_execution_effect(store,&c);
    int present=0;
    if (!r) r=ae_catalogue_execution_request_state(store,&p,&c,&present);
    if (!r && present) r=-EALREADY;
    if (!r) r=ae_root_check_path(dir,directory,&parent);
    struct ae_dispatch_snapshot d;
    if (!r) r=ae_catalogue_get_dispatch(store,p.lease,&c,&d);
    if (!r) { writer=openat(dir,".",O_TMPFILE|O_RDWR|O_CLOEXEC,0600); if (writer<0) r=-errno; }
    if (!r && fsetfilecon_raw(writer,"system_u:object_r:systemd_unit_file_t:s0")) r=-errno;
    size_t offset=0;
    while (!r && offset<size) {
        ssize_t amount=pwrite(writer,bytes+offset,size-offset,(off_t)offset);
        if (amount<0) { if (errno==EINTR) continue; r=-errno; }
        else if (!amount) r=-EIO;
        else offset+=(size_t)amount;
    }
    if (!r && fchmod(writer,0444)) r=-errno;
    if (!r && fsync(writer)) r=-errno;
    if (!r && linkat(writer,"",dir,d.unit,AT_EMPTY_PATH)) r=-errno;
    if (!r && fsync(writer)) r=-errno;
    if (!r && fsync(dir)) r=-errno;
    if (!r) r=ae_root_check_path(dir,directory,&parent);
    if (!r) {
        struct open_how how={.flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW,.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
        reader=(int)syscall(SYS_openat2,dir,d.unit,&how,sizeof how); if (reader<0) r=-errno;
    }
    struct ae_unit_file value,original;
    if (!r) r=ae_unit_file_capture(reader,a.recipe,&value);
    if (!r && statx(writer,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW,STATX_BASIC_STATS|STATX_BTIME|STATX_MNT_ID_UNIQUE,&original.object)) r=-errno;
    unsigned int required=STATX_BASIC_STATS|STATX_BTIME|STATX_MNT_ID_UNIQUE;
    if (!r && (original.object.stx_mask&required)!=required) r=-ENOTSUP;
    if (!r) r=ae_sha256_fd(writer,size,original.digest);
    if (!r && !ae_unit_file_equal(&original,&value)) r=-ESTALE;
    if (!r) r=ae_catalogue_check_execution_effect(store,&c);
    if (frame) { int ended=ae_catalogue_end_execution_effect(store); if (!r) r=ended; }
    if (writer>=0) close(writer);
    if (dir>=0) close(dir);
    if (r) { if (reader>=0) close(reader); return r; }
    *output=reader; *witness=value; return 0;
}
static struct ae_effect_owner context(struct ae_root_unit *s)
{
    struct ae_effect_owner value;
    memcpy(value.epoch,s->profile.system_epoch,16); memcpy(value.runtime,s->profile.system_runtime,16); memcpy(value.boot,s->profile.boot,16);
    return value;
}
static int image_label(int fd,const char *expected)
{
    char label[512]; ssize_t size=fgetxattr(fd,"security.selinux",label,sizeof label);
    if (size<0) return -errno;
    if (size>0 && !label[size-1]) --size;
    return (size_t)size==strlen(expected) && !memcmp(label,expected,(size_t)size) ? 0 : -EACCES;
}
static int image(struct ae_root_unit *s)
{
    const struct ae_root_unit_image *a=&s->source;
    int r=ae_root_check_path(a->directory_fd,a->directory,&a->parent);
    struct ae_image held,named;
    if (!r) r=ae_image_verify(a->image_fd,s->profile.starter_digest,&held);
    if (!r && !ae_image_equal(&held,&a->image)) r=-ESTALE;
    if (!r) r=image_label(a->image_fd,a->label);
    int fd=-1;
    if (!r) {
        struct open_how how={.flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW,
            .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
        fd=(int)syscall(SYS_openat2,a->directory_fd,a->leaf,&how,sizeof how); if (fd<0) r=-errno;
    }
    if (!r) r=ae_image_verify(fd,s->profile.starter_digest,&named);
    if (!r && !ae_image_equal(&held,&named)) r=-ESTALE;
    if (!r) r=image_label(fd,a->label);
    if (fd>=0) close(fd);
    return r;
}
static void release_hold(struct ae_root_unit *s)
{
    if (s->held) {
        int r=ae_catalogue_end_execution_effect(s->catalogue);
        if (!r) s->held=0; else if (!s->error) s->error=r;
    }
}
static int fresh(void *data)
{
    struct ae_root_unit *s=data;
    int r=ae_catalogue_check_execution_effect(s->catalogue,&s->broker);
    int present=0;
    if (!r) r=ae_catalogue_execution_request_state(s->catalogue,&s->profile,&s->broker,&present);
    if (!r && !present) r=-ESTALE;
    struct ae_unit_allocation_snapshot allocation; struct ae_effect_owner original=context(s);
    if (!r) r=ae_catalogue_get_unit(s->catalogue,&s->profile,&original,&allocation);
    if (!r && (!allocation.witnessed || memcmp(allocation.selection,s->selection,32))) r=-ESTALE;
    if (!r) r=image(s);
    if (!r) r=s->admit(s->admission,&s->profile);
    if (r>0) r=-EIO;
    if (!r) r=image(s);
    if (!r) r=ae_catalogue_execution_request_state(s->catalogue,&s->profile,&s->broker,&present);
    if (!r && !present) r=-ESTALE;
    if (!r) r=ae_catalogue_check_execution_effect(s->catalogue,&s->broker);
    return r;
}
int ae_root_unit_new(struct ae_catalogue *store,struct ae_unit **unit,const struct ae_unit_profile *selected,
    const struct ae_root_unit_image *source,const struct ae_execution_profile *p,const struct ae_launch_context *broker,
    ae_root_unit_admit admit,void *data,struct ae_root_unit **out)
{
    if (!store || !unit || !*unit || !selected || !source || !p || !broker || !admit || !out || *out ||
        source->directory_fd<0 || source->image_fd<0 || !memchr(source->directory,0,sizeof source->directory) || source->directory[0]!='/' ||
        !memchr(source->leaf,0,sizeof source->leaf) || !source->leaf[0] || strchr(source->leaf,'/') ||
        !strcmp(source->leaf,".") || !strcmp(source->leaf,"..") || !memchr(source->label,0,sizeof source->label) || !source->label[0] ||
        source->parent.uid || source->parent.gid || (source->parent.mode&0022)) return -EINVAL;
    if (getuid() || geteuid() || getgid() || getegid()) return -EPERM;
    int r=ae_unit_match_profile(*unit,p->boot,selected);
    struct ae_unit_status state;
    if (!r) r=ae_unit_status(*unit,&state);
    if (!r && (state.used || state.subscribe_attempted || state.ref_attempted || state.start_used || state.retire_used || state.retired ||
        state.fenced || state.quarantined || state.error)) r=-EALREADY;
    if (r) return r;
    struct ae_root_unit *s=calloc(1,sizeof *s); if (!s) return -ENOMEM;
    s->owner=getpid(); s->catalogue=store; s->profile=*p; s->broker=*broker; s->source=*source;
    s->source.directory_fd=-1; s->source.image_fd=-1; s->admit=admit; s->admission=data;
    r=ae_execution_digest(p,s->digest);
    if (!r && memcmp(p->starter_digest,source->image.digest,32)) r=-ESTALE;
    struct ae_dispatch_snapshot dispatch; int present=0;
    if (!r) r=ae_catalogue_check_execution_effect(store,broker);
    if (!r) r=ae_catalogue_execution_request_state(store,p,broker,&present);
    if (!r && present) r=-EALREADY;
    if (!r) r=ae_root_unit_selection_digest(p,selected,source,s->selection);
    struct ae_unit_allocation_snapshot allocation; struct ae_effect_owner original=context(s);
    if (!r) r=ae_catalogue_get_unit(store,p,&original,&allocation);
    if (!r && (!allocation.witnessed || memcmp(allocation.selection,s->selection,32) || memcmp(allocation.allocation.recipe,selected->nodes[0].file.digest,32))) r=-ESTALE;
    uint8_t placement[32];
    if (!r) r=ae_root_unit_placement_digest(selected->nodes[0].directory,&selected->nodes[0].parent,placement);
    if (!r && memcmp(placement,allocation.allocation.placement,32)) r=-ESTALE;
    if (!r) r=ae_catalogue_get_dispatch(store,p->lease,broker,&dispatch);
    if (!r) {
        static const char hex[]="0123456789abcdef"; memcpy(s->name,"aetheri-app-",12);
        for (unsigned int i=0;i<16;++i) { s->name[12+2*i]=hex[p->launch[i]>>4]; s->name[13+2*i]=hex[p->launch[i]&15]; }
        memcpy(s->name+44,".service",9);
        char path[4354]; int size=snprintf(path,sizeof path,"%s/%s",source->directory,source->leaf);
        if (size<=0 || (size_t)size>=sizeof path) r=-E2BIG;
        else if (selected->nodes[0].kind!=AE_UNIT_SERVICE || strcmp(selected->nodes[0].name,s->name) ||
            strcmp(dispatch.unit,s->name) || strcmp(selected->nodes[0].executable,path)) r=-ESTALE;
    }
    if (!r) { s->source.directory_fd=fcntl(source->directory_fd,F_DUPFD_CLOEXEC,3); if (s->source.directory_fd<0) r=-errno; }
    if (!r) { s->source.image_fd=fcntl(source->image_fd,F_DUPFD_CLOEXEC,3); if (s->source.image_fd<0) r=-errno; }
    if (!r) r=image(s);
    if (r) {
        if (s->source.directory_fd>=0) close(s->source.directory_fd);
        if (s->source.image_fd>=0) close(s->source.image_fd);
        free(s); return r;
    }
    s->unit=*unit; *unit=NULL; *out=s; return 0;
}
int ae_root_unit_request(void *data,const struct ae_execution_profile *p,const struct ae_dispatch_snapshot *d)
{
    struct ae_root_unit *s=data; int r=owner(s);
    if (!r && (!p || !d)) r=-EINVAL;
    if (!r && s->busy) r=-EBUSY;
    if (!r && s->used) r=-EALREADY;
    if (r) return r;
    s->busy=1; s->used=1; uint8_t hash[32]; r=ae_execution_digest(p,hash);
    if (!r && (!memchr(d->unit,0,sizeof d->unit) || !memchr(d->state,0,sizeof d->state))) r=-EINVAL;
    if (!r && (memcmp(hash,s->digest,32) || memcmp(d->operation,p->root_operation,16) || memcmp(d->launch,p->launch,16) ||
        d->install_gate_revision!=p->install_gate_revision || strcmp(d->unit,s->name) || strcmp(d->state,"reserved") || d->invocation_present)) r=-ESTALE;
    if (!r) r=ae_catalogue_begin_execution_effect(s->catalogue,&s->broker);
    int frame=!r; struct ae_effect_owner original=context(s);
    if (!r) r=ae_catalogue_begin_owner_cleanup(s->catalogue,&original);
    if (!r) s->held=1;
    if (!r) r=ae_unit_acquire(s->unit,fresh,s);
    if (!r) r=ae_unit_start(s->unit,fresh,s,NULL,NULL);
    struct ae_unit_status state;
    if (s->held && ae_unit_status(s->unit,&state)==0 && !state.subscribe_attempted && !state.ref_attempted && !state.start.attempted) release_hold(s);
    if (frame) { int ended=ae_catalogue_end_execution_effect(s->catalogue); if (!r) r=ended; }
    s->busy=0; return r;
}
sd_bus *ae_root_unit_bus(struct ae_root_unit *s) { return !owner(s) ? ae_unit_bus(s->unit) : NULL; }
int ae_root_unit_status(struct ae_root_unit *s,struct ae_root_unit_status *out)
{
    int r=owner(s); if (!r && !out) r=-EINVAL;
    struct ae_root_unit_status value={0};
    if (!r) r=ae_unit_status(s->unit,&value.unit);
    if (!r) { value.used=s->used; value.held=s->held; value.error=s->error; *out=value; }
    return r;
}
struct ending { struct ae_root_unit *root; ae_start_guard admit; void *data; };
static int end_guard(void *data)
{
    struct ending *end=data; struct ae_root_unit *s=end->root;
    struct ae_effect_owner original=context(s);
    int r=ae_catalogue_begin_owner_cleanup(s->catalogue,&original);
    if (!r) r=ae_catalogue_end_execution_effect(s->catalogue);
    if (!r) r=end->admit(end->data);
    if (r>0) r=-EIO;
    if (!r) r=ae_catalogue_begin_owner_cleanup(s->catalogue,&original);
    if (!r) r=ae_catalogue_end_execution_effect(s->catalogue);
    return r;
}
int ae_root_unit_retire(struct ae_root_unit *s,ae_start_guard admit,void *data)
{
    int r=owner(s); if (!r && !admit) r=-EINVAL; if (!r && s->busy) r=-EBUSY;
    if (r) return r;
    s->busy=1; struct ae_effect_owner original=context(s);
    r=ae_catalogue_begin_owner_cleanup(s->catalogue,&original); int frame=!r;
    struct ending end={.root=s,.admit=admit,.data=data};
    if (!r) r=ae_unit_retire(s->unit,end_guard,&end);
    struct ae_unit_status state;
    if (!r) r=ae_unit_status(s->unit,&state);
    if (!r && state.retired) release_hold(s);
    if (frame) { int ended=ae_catalogue_end_execution_effect(s->catalogue); if (!r) r=ended; }
    if (!r && s->error) r=s->error;
    s->busy=0; return r;
}
static int dispose(struct ae_root_unit **out,int abandon)
{
    if (!out) return -EINVAL;
    struct ae_root_unit *s=*out; if (!s) return 0;
    int r=owner(s); if (!r && s->busy) r=-EBUSY;
    if (r) return r;
    r=abandon ? ae_unit_abandon(&s->unit) : ae_unit_close(&s->unit);
    if (r) return r;
    release_hold(s); if (s->held) return s->error ? s->error : -EBUSY;
    close(s->source.directory_fd); close(s->source.image_fd); free(s); *out=NULL; return 0;
}
int ae_root_unit_close(struct ae_root_unit **out) { return dispose(out,0); }
int ae_root_unit_abandon(struct ae_root_unit **out) { return dispose(out,1); }
