#define _GNU_SOURCE
#include "files/session.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"notifications %d: %s errno=%d\n",__LINE__,#x,errno);exit(1); } } while (0)
static void put(int root,const char *name,const char *bytes) {
    int fd=openat(root,name,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);CHECK(fd>=0);
    CHECK(write(fd,bytes,strlen(bytes))==(ssize_t)strlen(bytes) && !close(fd));
}
static void drain(int fd) { int changed;do { changed=ae_files_notifications_read(fd);CHECK(changed>=0); } while(changed); }
static void updated(struct ae_files_session *s) {
    int fd=ae_files_notification_fd(s->files);struct pollfd event={fd,POLLIN,0};CHECK(poll(&event,1,2000)==1);
    CHECK(ae_files_notifications_read(fd)==1);ae_files_session_directory_update(s,true);
}
static void select_name(struct ae_files *f,const char *name) {
    for(size_t i=0;i<ae_files_count(f);++i) if(!strcmp(ae_files_entry(f,i)->name,name)) { CHECK(ae_files_select(f,i));return; }
    CHECK(false);
}
int main(void) {
    CHECK(getuid()!=0);char path[]="/tmp/files-notify-XXXXXX";CHECK(mkdtemp(path));
    int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);put(root,"selected","old");CHECK(!mkdirat(root,"sub",0700));
    struct ae_files_session s;ae_files_session_init(&s,NULL,NULL,NULL);enum ae_files_result result;
    s.files=ae_files_create(root,&result);CHECK(s.files && result==AE_FILES_OK);s.rows=10;
    CHECK(ae_files_layout_make(1000,680,1,false,true,true,false,&s.layout));
    int notify=ae_files_notification_fd(s.files);CHECK(notify>=0 && (fcntl(notify,F_GETFD)&FD_CLOEXEC) && (fcntl(notify,F_GETFL)&O_NONBLOCK));
    struct pollfd idle={notify,POLLIN,0};CHECK(!poll(&idle,1,0));select_name(s.files,"selected");
    pid_t child=fork();CHECK(child>=0);if(!child) {put(root,"aaa-external","new");_exit(0);}
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    updated(&s);CHECK(ae_files_count(s.files)==3 && !strcmp(ae_files_entry(s.files,ae_files_selected(s.files))->name,"selected"));
    drain(notify);CHECK(!poll(&idle,1,0));put(root,"selected","new bytes");updated(&s);
    CHECK(ae_files_entry(s.files,ae_files_selected(s.files))->size==9);
    CHECK(ae_files_filter(s.files,"selected",8));put(root,".hidden","hidden");updated(&s);
    CHECK(ae_files_count(s.files)==1 && !strcmp(ae_files_query(s.files),"selected"));
    CHECK(ae_files_filter(s.files,"",0));s.dialog=true;put(root,"deferred","bytes");updated(&s);
    CHECK(s.external_pending && ae_files_total(s.files)==4);s.dialog=false;ae_files_session_directory_update(&s,false);
    CHECK(!s.external_pending && ae_files_total(s.files)==5);
    drain(notify);select_name(s.files,"sub");CHECK(ae_files_enter(s.files)==AE_FILES_OK);
    int sub=ae_files_directory(s.files);CHECK(sub>=0);drain(notify);
    CHECK(!renameat(root,"sub",root,"moved") && !mkdirat(root,"sub",0700));
    put(sub,"held-child","retained");updated(&s);CHECK(ae_files_count(s.files)==1 && !strcmp(ae_files_entry(s.files,0)->name,"held-child"));
    int replacement=openat(root,"sub",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(replacement>=0);drain(notify);
    put(replacement,"foreign-child","not selected");CHECK(!poll(&idle,1,0));
    CHECK(ae_files_up(s.files)==AE_FILES_OK && ae_files_notification_fd(s.files)==notify);drain(notify);
    CHECK(!unlinkat(root,"aaa-external",0));updated(&s);CHECK(ae_files_count(s.files)==4);
    /* A burst is bounded per read and remains an invalidation, not replayed
       create/remove commands. The model scans only after we request refresh. */
    drain(notify);for(unsigned i=0;i<1200;++i) {put(root,"burst","");CHECK(!unlinkat(root,"burst",0));}
    CHECK(ae_files_notifications_read(notify)==1 && poll(&idle,1,0)==1);drain(notify);
    ae_files_session_directory_update(&s,true);CHECK(ae_files_count(s.files)==4 && !poll(&idle,1,0));
    ae_files_destroy(s.files);CHECK(fcntl(notify,F_GETFD)<0 && errno==EBADF);
    CHECK(!unlinkat(sub,"held-child",0) && !close(sub));CHECK(!unlinkat(replacement,"foreign-child",0) && !close(replacement));
    CHECK(!unlinkat(root,"selected",0) && !unlinkat(root,".hidden",0) && !unlinkat(root,"deferred",0));
    CHECK(!unlinkat(root,"moved",AT_REMOVEDIR) && !unlinkat(root,"sub",AT_REMOVEDIR) && !close(root) && !rmdir(path));
    puts("Files notifications: real external process, idle FD, modify/filter/selection, modal deferral, held renamed directory, bounded burst and cleanup passed");return 0;
}
