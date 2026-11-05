#define _GNU_SOURCE
#include "model.h"
#include "text/grapheme.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct snapshot { size_t count;struct ae_files_entry entries[AE_FILES_ENTRIES]; };
struct ae_files { int directories[AE_FILES_DEPTH+1];char folders[AE_FILES_DEPTH+1][AE_FILES_LABEL+1];unsigned depth;
    struct snapshot *snapshot;size_t visible[AE_FILES_ENTRIES],count,selected;bool hidden;char query[AE_FILES_QUERY+1];uint64_t revision; };
static enum ae_files_kind kind(mode_t mode) { return S_ISDIR(mode) ? AE_FILES_DIRECTORY : S_ISREG(mode) ? AE_FILES_REGULAR : S_ISLNK(mode) ? AE_FILES_LINK : AE_FILES_OTHER; }
static void label(const char *name,char out[AE_FILES_LABEL+1]) {
    static const char hex[]="0123456789abcdef";size_t size=strlen(name),at=0,used=0;
    while (at<size) {
        size_t before=at;uint32_t c;bool valid=ae_utf8_next(name,size,&at,&c);
        bool escape=!valid || c<32 || (c>=0x7f && c<=0x9f) || c=='\\' || (c>=0x200b && c<=0x200f) || (c>=0x202a && c<=0x202e) || (c>=0x2060 && c<=0x206f);
        if (!valid) at=before+1;
        if (escape) for (size_t i=before;i<at;++i) { unsigned char b=(unsigned char)name[i];out[used++]='\\';out[used++]='x';out[used++]=hex[b>>4];out[used++]=hex[b&15u]; }
        else { memcpy(out+used,name+before,at-before);used+=at-before; }
    }
    out[used]=0;
}
static int compare(const void *left,const void *right) {
    const struct ae_files_entry *a=left,*b=right;bool ad=a->kind==AE_FILES_DIRECTORY,bd=b->kind==AE_FILES_DIRECTORY;
    return ad!=bd ? ad ? -1 : 1 : strcmp(a->name,b->name);
}
static enum ae_files_result scan(int directory,struct snapshot **out) {
    struct snapshot *s=calloc(1,sizeof(*s));if (!s) return AE_FILES_NOMEM;
    int fd=openat(directory,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if (fd<0) { free(s);return AE_FILES_IO; }
    DIR *stream=fdopendir(fd);if (!stream) { close(fd);free(s);return AE_FILES_IO; }
    enum ae_files_result result=AE_FILES_OK;errno=0;struct dirent *entry;
    while ((entry=readdir(stream))) {
        if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
        size_t n=strlen(entry->d_name);if (!n || n>AE_FILES_NAME || s->count==AE_FILES_ENTRIES) { result=AE_FILES_LIMIT;break; }
        struct ae_files_entry *e=&s->entries[s->count++];memcpy(e->name,entry->d_name,n+1);label(e->name,e->label);struct stat st;
        if (fstatat(directory,e->name,&st,AT_SYMLINK_NOFOLLOW)) {
            if (errno==ENOENT) { result=AE_FILES_CHANGED;break; }
            if (errno!=EACCES && errno!=EPERM) { result=AE_FILES_IO;break; }
            e->kind=AE_FILES_UNAVAILABLE;
        } else { e->kind=kind(st.st_mode);e->device=st.st_dev;e->inode=st.st_ino;e->size=st.st_size>=0 ? (uint64_t)st.st_size : 0; }
        errno=0;
    }
    if (result==AE_FILES_OK && errno) result=AE_FILES_IO;
    if (closedir(stream) && result==AE_FILES_OK) result=AE_FILES_IO;
    if (result!=AE_FILES_OK) { free(s);return result; }
    qsort(s->entries,s->count,sizeof(s->entries[0]),compare);*out=s;return AE_FILES_OK;
}
static void filter(struct ae_files *f) {
    f->count=0;for (size_t i=0;i<f->snapshot->count;++i) { const struct ae_files_entry *e=&f->snapshot->entries[i];if ((!f->hidden && e->name[0]=='.') || !strstr(e->name,f->query)) continue;f->visible[f->count++]=i; }
    if (f->selected>=f->count) f->selected=f->count ? f->count-1 : 0;
}
static enum ae_files_result replace(struct ae_files *f,int directory) {
    if (f->revision==UINT64_MAX) return AE_FILES_EXHAUSTED;
    struct snapshot *s=NULL;enum ae_files_result r=scan(directory,&s);if (r!=AE_FILES_OK) return r;
    free(f->snapshot);f->snapshot=s;++f->revision;f->selected=0;filter(f);return AE_FILES_OK;
}
struct ae_files *ae_files_create(int root,enum ae_files_result *result) {
    if (!result) return NULL;
    *result=AE_FILES_INVALID;struct stat st;if (root<0 || fstat(root,&st) || !S_ISDIR(st.st_mode)) return NULL;
    struct ae_files *f=calloc(1,sizeof(*f));if (!f) { *result=AE_FILES_NOMEM;return NULL; }
    for (unsigned i=0;i<=AE_FILES_DEPTH;++i) f->directories[i]=-1;
    f->directories[0]=fcntl(root,F_DUPFD_CLOEXEC,4);if (f->directories[0]<0) { free(f);*result=AE_FILES_IO;return NULL; }
    memcpy(f->folders[0],"Root",5);*result=replace(f,f->directories[0]);if (*result!=AE_FILES_OK) { ae_files_destroy(f);return NULL; }return f;
}
void ae_files_destroy(struct ae_files *f) { if (f) { for (unsigned i=0;i<=f->depth;++i) if (f->directories[i]>=0) close(f->directories[i]);free(f->snapshot);free(f); } }
enum ae_files_result ae_files_refresh(struct ae_files *f) { return f ? replace(f,f->directories[f->depth]) : AE_FILES_INVALID; }
const struct ae_files_entry *ae_files_entry(const struct ae_files *f,size_t i) { return f && i<f->count ? &f->snapshot->entries[f->visible[i]] : NULL; }
static bool identity(const struct ae_files_entry *e,int fd) { struct stat st;return !fstat(fd,&st) && e->device==st.st_dev && e->inode==st.st_ino && e->kind==kind(st.st_mode); }
enum ae_files_result ae_files_enter(struct ae_files *f) {
    if (!f) return AE_FILES_INVALID;
    const struct ae_files_entry *e=ae_files_entry(f,f->selected);if (!e || e->kind!=AE_FILES_DIRECTORY) return AE_FILES_UNSUPPORTED;
    if (f->depth==AE_FILES_DEPTH) return AE_FILES_LIMIT;
    char title[AE_FILES_LABEL+1];memcpy(title,e->label,sizeof(title));
    int fd=openat(f->directories[f->depth],e->name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if (fd<0) return AE_FILES_CHANGED;
    if (!identity(e,fd)) { close(fd);return AE_FILES_CHANGED; }
    enum ae_files_result r=replace(f,fd);if (r!=AE_FILES_OK) { close(fd);return r; }
    ++f->depth;f->directories[f->depth]=fd;memcpy(f->folders[f->depth],title,sizeof(title));f->query[0]=0;f->selected=0;filter(f);return AE_FILES_OK;
}
enum ae_files_result ae_files_up(struct ae_files *f) {
    if (!f) return AE_FILES_INVALID;
    if (!f->depth) return AE_FILES_ROOT;
    enum ae_files_result r=replace(f,f->directories[f->depth-1]);if (r!=AE_FILES_OK) return r;
    close(f->directories[f->depth--]);f->query[0]=0;f->selected=0;filter(f);return AE_FILES_OK;
}
bool ae_files_filter(struct ae_files *f,const char *query,size_t size) {
    if (!f || (!query && size) || size>AE_FILES_QUERY) return false;
    size_t at=0;while (at<size) { uint32_t c;if (!ae_utf8_next(query,size,&at,&c) || c<32 || (c>=0x7f && c<=0x9f)) return false; }
    if (size) memmove(f->query,query,size);
    f->query[size]=0;f->selected=0;filter(f);return true;
}
void ae_files_hidden(struct ae_files *f,bool show) { if (f) { f->hidden=show;filter(f); } }
bool ae_files_select(struct ae_files *f,size_t i) { if (!f || i>=f->count) return false;f->selected=i;return true; }
size_t ae_files_count(const struct ae_files *f) { return f ? f->count : 0; }
size_t ae_files_total(const struct ae_files *f) { return f ? f->snapshot->count : 0; }
size_t ae_files_selected(const struct ae_files *f) { return f ? f->selected : 0; }
unsigned ae_files_depth(const struct ae_files *f) { return f ? f->depth : 0; }
const char *ae_files_query(const struct ae_files *f) { return f ? f->query : ""; }
const char *ae_files_folder(const struct ae_files *f) { return f ? f->folders[f->depth] : ""; }
int ae_files_directory(const struct ae_files *f) { if (!f) { errno=EINVAL;return -1; }return fcntl(f->directories[f->depth],F_DUPFD_CLOEXEC,4); }
static enum ae_files_result hold_selected(const struct ae_files *f,int *directory,int *witness,char leaf[AE_FILES_NAME+1],bool allow_directory) {
    if (!directory || !witness || directory==witness || !leaf) return AE_FILES_INVALID;
    *directory=*witness=-1;leaf[0]=0;const struct ae_files_entry *e=f ? ae_files_entry(f,f->selected) : NULL;
    if (!e || (e->kind!=AE_FILES_REGULAR && !(allow_directory && e->kind==AE_FILES_DIRECTORY))) return AE_FILES_UNSUPPORTED;
    int pin=openat(f->directories[f->depth],e->name,O_PATH|O_CLOEXEC|O_NOFOLLOW);if (pin<0) return AE_FILES_CHANGED;
    if (!identity(e,pin)) { close(pin);return AE_FILES_CHANGED; }
    if (pin<4) { int copy=fcntl(pin,F_DUPFD_CLOEXEC,4);close(pin);if (copy<0) return AE_FILES_IO;pin=copy; }
    int parent=fcntl(f->directories[f->depth],F_DUPFD_CLOEXEC,4);if (parent<0) { close(pin);return AE_FILES_IO; }
    *directory=parent;*witness=pin;memcpy(leaf,e->name,strlen(e->name)+1);return AE_FILES_OK;
}
enum ae_files_result ae_files_open_selected(const struct ae_files *f,int *directory,int *witness,char leaf[AE_FILES_NAME+1]) { return hold_selected(f,directory,witness,leaf,false); }
enum ae_files_result ae_files_hold_selected(const struct ae_files *f,int *directory,int *witness,char leaf[AE_FILES_NAME+1]) { return hold_selected(f,directory,witness,leaf,true); }
