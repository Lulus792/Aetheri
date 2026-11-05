#define _GNU_SOURCE
#include "launch.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
int ae_files_launch(struct ae_files_child *c,const char *terminal,const char *editor,const char *font,const char *cache,int directory,int witness,const char *leaf,bool diagnostics) {
    struct stat st;
    if (!c || c->pid>0 || !terminal || terminal[0]!='/' || !editor || editor[0]!='/' || !font || font[0]!='/' || !cache || cache[0]!='/' || !leaf || !*leaf || strlen(leaf)>255 || strchr(leaf,'/') || !strcmp(leaf,".") || !strcmp(leaf,"..") || directory<3 || witness<3 ||
        fstat(directory,&st) || !S_ISDIR(st.st_mode) || fstat(witness,&st) || !S_ISREG(st.st_mode)) { errno=EINVAL;return -1; }
    struct sigaction inherited;if (getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() || sigaction(SIGCHLD,NULL,&inherited) || inherited.sa_handler==SIG_IGN || (inherited.sa_flags&SA_NOCLDWAIT)) { errno=EINVAL;return -1; }
    int dir=fcntl(directory,F_DUPFD_CLOEXEC,5),file=fcntl(witness,F_DUPFD_CLOEXEC,5);
    if (dir<0 || file<0) { int e=errno;if (dir>=0) close(dir);if (file>=0) close(file);errno=e;return -1; }
    size_t count=0;while (environ[count]) ++count;char **environment=calloc(count+1,sizeof(*environment));
    if (!environment) { close(dir);close(file);return -1; }
    size_t used=0;for (size_t i=0;i<count;++i) if (strncmp(environ[i],"WAYLAND_SOCKET=",15)) environment[used++]=environ[i];
    posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;int e=posix_spawn_file_actions_init(&actions);
    if (e) goto done;
    e=posix_spawnattr_init(&attributes);if (e) { posix_spawn_file_actions_destroy(&actions);goto done; }
    sigset_t empty;sigemptyset(&empty);
    if (!(e=posix_spawn_file_actions_adddup2(&actions,dir,3)) && !(e=posix_spawn_file_actions_adddup2(&actions,file,4)) &&
        !(e=posix_spawn_file_actions_addfchdir_np(&actions,3)) && !(e=posix_spawn_file_actions_addclosefrom_np(&actions,5)) &&
        !(e=posix_spawnattr_setpgroup(&attributes,0)) && !(e=posix_spawnattr_setsigmask(&attributes,&empty)) &&
        !(e=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP|POSIX_SPAWN_SETSIGMASK))) {
        char *argv[]={(char *)terminal,"--editor",(char *)editor,"--open-leaf",(char *)leaf,"--font",(char *)font,"--cache",(char *)cache,diagnostics ? "--diagnostics" : NULL,NULL};
        pid_t pid=-1;e=posix_spawn(&pid,terminal,&actions,&attributes,argv,environment);
        if (!e) { *c=(struct ae_files_child){.pid=pid,.pin=(int)syscall(SYS_pidfd_open,pid,0)}; }
    }
    posix_spawnattr_destroy(&attributes);posix_spawn_file_actions_destroy(&actions);
done:
    free(environment);close(dir);close(file);if (e) { errno=e;return -1; }return 0;
}
int ae_files_child_observe(struct ae_files_child *c) {
    if (!c || c->pid<=0) { errno=EINVAL;return -1; }
    if (c->ended) return 1;
    int status;pid_t p=waitpid(c->pid,&status,WNOHANG);if (!p || (p<0 && errno==EINTR)) return 0;
    if (p!=c->pid) return -1;
    c->ended=true;c->status=WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128+WTERMSIG(status) : 125;return 1;
}
int ae_files_child_close(struct ae_files_child *c) {
    if (!c || (c->pid>0 && !c->ended)) { errno=EBUSY;return -1; }
    if (c->pid>0 && c->pin>=0) close(c->pin);
    *c=(struct ae_files_child){.pid=-1,.pin=-1};return 0;
}
