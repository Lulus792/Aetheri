/* Development PTY bootstrap, never the privileged Aetheri execution starter. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
extern char **environ;
int main(int argc,char **argv) {
    bool welcome=argc==2 && !strcmp(argv[1],"--welcome");
    if(argc!=2 || (!welcome && strcmp(argv[1],"--quiet"))) return 2;
    struct stat code,helper,tty,out,err;struct termios term;
    if(getuid()==0 || getuid()!=geteuid() || getgid()!=getegid() || getsid(0)!=getpid() || getpgrp()!=getpid() ||
       fstat(3,&code) || !S_ISREG(code.st_mode) || (code.st_mode&(S_ISUID|S_ISGID)) ||
       fstat(4,&helper) || !S_ISREG(helper.st_mode) || (helper.st_mode&(S_ISUID|S_ISGID)) ||
       fstat(0,&tty) || !S_ISCHR(tty.st_mode) || fstat(1,&out) || fstat(2,&err) ||
       tty.st_dev!=out.st_dev || tty.st_ino!=out.st_ino || tty.st_rdev!=out.st_rdev ||
       tty.st_dev!=err.st_dev || tty.st_ino!=err.st_ino || tty.st_rdev!=err.st_rdev ||
       tcgetattr(0,&term) || ioctl(0,TIOCSCTTY,0) || tcgetsid(0)!=getpid() || tcgetpgrp(0)!=getpid()) return 125;
    /* Close any unexpected inputs too; only the two terminal sources passed
       the parent file actions. The final shell gets stdio only. */
    if(close_range(5,~0u,0) || close(4) || fcntl(3,F_SETFD,FD_CLOEXEC)) return 125;
    char *quiet[]={"aetheri-shell","-i","--edit",NULL};
    char *greeting[]={"aetheri-shell","-i","--edit","--welcome",NULL};
    execveat(3,"",welcome?greeting:quiet,environ,AT_EMPTY_PATH);return 126;
}
