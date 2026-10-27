#include "linux/view.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <unistd.h>

struct ae_view_namespace { pid_t process; int fd; dev_t device; ino_t inode; size_t views; };
struct ae_view {
    struct ae_view_namespace *space;
    int source,fd;
    struct ae_root_identity original,clone;
    enum ae_view_role role;
    enum ae_view_phase phase;
};

static int current(struct ae_view_namespace *space)
{
    if (space==NULL) return -EINVAL;
    if (space->process!=getpid()) return -ECHILD;
    struct stat retained,named;
    if (fstat(space->fd,&retained)!=0 || stat("/proc/self/ns/mnt",&named)!=0) return -errno;
    if (retained.st_dev!=space->device || retained.st_ino!=space->inode || named.st_dev!=space->device || named.st_ino!=space->inode) return -ESTALE;
    return 0;
}
static int single_thread(void)
{
    DIR *tasks=opendir("/proc/self/task");
    if (tasks==NULL) return -errno;
    size_t count=0; int result=0; struct dirent *entry;
    for (;;) {
        errno=0; entry=readdir(tasks);
        if (entry==NULL) { result=errno ? -errno : 0; break; }
        if (strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0 && ++count>1) { result=-EBUSY; break; }
    }
    if (closedir(tasks)!=0 && result==0) result=-errno;
    return result==0 && count!=1 ? -ESTALE : result;
}
int ae_view_namespace_enter(struct ae_view_namespace **output)
{
    if (output==NULL) return -EINVAL;
    if (geteuid()!=0) return -EPERM;
    int result=single_thread();
    struct stat before,after;
    if (result==0 && stat("/proc/self/ns/mnt",&before)!=0) result=-errno;
    if (result!=0) return result;
    struct ae_view_namespace *space=calloc(1,sizeof *space);
    if (space==NULL) return -ENOMEM;
    space->fd=-1;
    if (unshare(CLONE_NEWNS)!=0) result=-errno;
    if (result==0 && mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)!=0) result=-errno;
    if (result==0) {
        space->fd=open("/proc/self/ns/mnt",O_RDONLY|O_CLOEXEC);
        if (space->fd<0) result=-errno;
    }
    if (result==0 && fstat(space->fd,&after)!=0) result=-errno;
    if (result==0 && before.st_dev==after.st_dev && before.st_ino==after.st_ino) result=-ESTALE;
    if (result!=0) { if (space->fd>=0) close(space->fd); free(space); return result; }
    space->process=getpid(); space->device=after.st_dev; space->inode=after.st_ino;
    *output=space; return 0;
}
static int root_matches(int fd,const struct ae_root_identity *expected)
{
    struct ae_root_identity actual; int result=ae_root_observe(fd,&actual);
    return result==0 && !ae_root_equal(&actual,expected) ? -ESTALE : result;
}
static int flags_valid(int fd,enum ae_view_role role)
{
    struct statvfs status;
    if (fstatvfs(fd,&status)!=0) return -errno;
    unsigned long required=ST_NOSUID|ST_NODEV;
    if (role==AE_VIEW_CODE) required|=ST_RDONLY;
    else required|=ST_NOEXEC;
    if ((status.f_flag&required)!=required) return -EACCES;
    if (role==AE_VIEW_CODE && (status.f_flag&ST_NOEXEC)) return -EACCES;
    if (role==AE_VIEW_DATA && (status.f_flag&ST_RDONLY)) return -EROFS;
    return 0;
}
int ae_view_clone(struct ae_view_namespace *space,int source,const struct ae_root_identity *expected,
                  enum ae_view_role role,struct ae_view **output)
{
    if (source<0 || expected==NULL || output==NULL || (role!=AE_VIEW_CODE && role!=AE_VIEW_DATA)) return -EINVAL;
    int result=current(space);
    if (result==0 && space->views==SIZE_MAX) result=-EOVERFLOW;
    if (result==0) result=root_matches(source,expected);
    if (result!=0) return result;
    struct ae_view *view=calloc(1,sizeof *view);
    if (view==NULL) return -ENOMEM;
    view->source=-1; view->fd=-1;
    view->space=space; view->role=role; view->phase=AE_VIEW_DETACHED; view->original=*expected;
    view->source=fcntl(source,F_DUPFD_CLOEXEC,3);
    if (view->source<0) result=-errno;
    if (result==0) {
        view->fd=open_tree(view->source,"",AT_EMPTY_PATH|OPEN_TREE_CLONE|OPEN_TREE_CLOEXEC);
        if (view->fd<0) result=-errno;
    }
    struct mount_attr attributes={.attr_set=MOUNT_ATTR_NOSUID|MOUNT_ATTR_NODEV|MOUNT_ATTR_NOSYMFOLLOW,
                                  .propagation=MS_PRIVATE};
    attributes.attr_set|=role==AE_VIEW_CODE ? MOUNT_ATTR_RDONLY : MOUNT_ATTR_NOEXEC;
    if (result==0 && mount_setattr(view->fd,"",AT_EMPTY_PATH,&attributes,sizeof attributes)!=0) result=-errno;
    if (result==0) result=flags_valid(view->fd,role);
    if (result==0) result=ae_root_observe(view->fd,&view->clone);
    if (result==0) {
        struct ae_root_identity comparable=view->clone; comparable.mount_id=expected->mount_id;
        if (view->clone.mount_id==expected->mount_id || !ae_root_equal(&comparable,expected)) result=-ESTALE;
    }
    if (result==0) result=root_matches(view->source,expected);
    if (result!=0) { if (view->fd>=0) close(view->fd); if (view->source>=0) close(view->source); free(view); return result; }
    ++space->views; *output=view; return 0;
}
static int check(struct ae_view *view)
{
    if (view==NULL) return -EINVAL;
    int result=current(view->space);
    if (result==0) result=root_matches(view->source,&view->original);
    if (result==0) result=root_matches(view->fd,&view->clone);
    if (result==0) result=flags_valid(view->fd,view->role);
    return result;
}
int ae_view_check(struct ae_view *view,enum ae_view_phase *phase,struct ae_root_identity *identity)
{
    if (phase==NULL || identity==NULL) return -EINVAL;
    int result=check(view);
    if (result==0) { *phase=view->phase; *identity=view->clone; }
    return result;
}
static int target(int parent,const char *slot)
{
    struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,
        .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV};
    int fd;
    do { fd=(int)syscall(SYS_openat2,parent,slot,&how,sizeof how); } while (fd<0 && errno==EINTR);
    return fd<0 ? -errno : fd;
}
static int empty(int fd)
{
    int duplicate=openat(fd,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if (duplicate<0) return -errno;
    DIR *directory=fdopendir(duplicate);
    if (directory==NULL) { int result=-errno; close(duplicate); return result; }
    int result=0; struct dirent *entry;
    for (;;) {
        errno=0; entry=readdir(directory);
        if (entry==NULL) { result=errno ? -errno : 0; break; }
        if (strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0) { result=-ENOTEMPTY; break; }
    }
    if (closedir(directory)!=0 && result==0) result=-errno;
    return result;
}
int ae_view_attach(struct ae_view *view,int parent,const struct ae_root_identity *expected,const char *slot)
{
    if (view==NULL || parent<0 || expected==NULL || slot==NULL) return -EINVAL;
    const char *wanted=view->role==AE_VIEW_CODE ? "app" : "data";
    if (strcmp(slot,wanted)!=0) return -EINVAL;
    int result=check(view);
    if (result==0 && view->phase!=AE_VIEW_DETACHED) result=-EALREADY;
    struct ae_root_identity parent_id;
    if (result==0) result=ae_root_observe(parent,&parent_id);
    int destination=result==0 ? target(parent,slot) : -1;
    if (result==0 && destination<0) result=destination;
    if (result==0) result=root_matches(destination,expected);
    if (result==0 && (parent_id.mount_id!=expected->mount_id || expected->uid!=(uint32_t)geteuid() ||
        (expected->mode&07777)!=0700 || parent_id.uid!=(uint32_t)geteuid() || (parent_id.mode&0022)!=0 ||
        (parent_id.device_major==expected->device_major && parent_id.device_minor==expected->device_minor && parent_id.inode==expected->inode) ||
        (view->original.device_major==expected->device_major && view->original.device_minor==expected->device_minor && view->original.inode==expected->inode))) result=-EACCES;
    if (result==0) result=empty(destination);
    if (result==0) result=check(view);
    if (result==0) {
        view->phase=AE_VIEW_UNCERTAIN;
        if (move_mount(view->fd,"",destination,"",MOVE_MOUNT_F_EMPTY_PATH|MOVE_MOUNT_T_EMPTY_PATH)!=0) result=-errno;
    }
    /* Traversal crosses exactly the newly attached mount, so NO_XDEV is omitted
     * only for this post-move single-component lookup under the trusted parent. */
    int named=-1;
    if (result==0) {
        named=openat(parent,slot,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        result=named<0 ? -errno : root_matches(named,&view->clone);
    }
    if (result==0) result=root_matches(parent,&parent_id);
    if (result==0) result=check(view);
    if (named>=0) close(named);
    if (destination>=0) close(destination);
    if (result==0) view->phase=AE_VIEW_ATTACHED;
    return result;
}
int ae_view_close(struct ae_view **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_view *view=*output; if (view==NULL) return 0;
    if (view->space->process!=getpid()) return -ECHILD;
    int result=close(view->fd)==0 ? 0 : -errno;
    if (close(view->source)!=0 && result==0) result=-errno;
    --view->space->views; free(view); *output=NULL; return result;
}
int ae_view_namespace_close(struct ae_view_namespace **output)
{
    if (output==NULL) return -EINVAL;
    struct ae_view_namespace *space=*output; if (space==NULL) return 0;
    if (space->process!=getpid()) return -ECHILD;
    if (space->views!=0) return -EBUSY;
    int result=close(space->fd)==0 ? 0 : -errno;
    free(space); *output=NULL; return result;
}
