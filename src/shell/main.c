#define _GNU_SOURCE
#include "execute.h"
#include "jobs.h"
#include "readline.h"
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int line(struct ae_shell_session *s,struct ae_shell_program *p,const char *text,size_t length) {
    size_t offset=0;enum ae_shell_error error=ae_shell_parse(text,length,ae_shell_environment,s,p,&offset);
    if (error!=AE_SHELL_OK) {
        const char *message=error==AE_SHELL_LIMIT ? "line/expansion budget exceeded" : error==AE_SHELL_UNSET ? "unset environment value" : "unsupported or invalid syntax";
        dprintf(2,"aetheri-shell: byte %zu: %s\n",offset,message);s->status=2;return -1;
    }
    ae_shell_execute(s,p);return 0;
}
static int interactive(struct ae_shell_session *s,struct ae_shell_program *p,bool editing) {
    struct ae_shell_terminal *t=calloc(1,sizeof(*t));
    if (!t) return 125;
    if (ae_shell_jobs_open(t)) { ae_shell_jobs_close(t);free(t);return 125; }
    struct ae_line_editor *editor=editing ? ae_line_create() : NULL;
    if (editing && (!editor || !setlocale(LC_CTYPE,"C.UTF-8"))) { ae_line_destroy(editor);ae_shell_jobs_close(t);free(t);return 125; }
    s->terminal=t;char buffer[AE_SHELL_BYTES];
    for (;;) {
        if (ae_shell_jobs_check(t)) { s->status=ae_shell_jobs_shutdown(t);break; }
        if (!editing) dprintf(t->fd,"aetheri> ");
        ssize_t got=editing ? ae_shell_readline(t,editor,buffer,sizeof(buffer)) : ae_shell_jobs_read(t,buffer,sizeof(buffer)-1);
        if (got==-1 || t->shutdown_signal) { s->status=ae_shell_jobs_shutdown(t);break; }
        if (got==-2) { s->status=130;dprintf(t->fd,"\n");continue; }
        if (got==-3) continue;
        if (!got) {
            if (ae_shell_jobs_busy(t)) { dprintf(t->fd,"\noriginal jobs remain; use jobs/fg/bg before EOF\n");continue; }
            break;
        }
        size_t used=(size_t)got;
        /* Check the raw record before trimming CR/line delimiters. N_TTY can
           already have discarded input at its canonical capacity. */
        if (!editing && used>=4095) { dprintf(t->fd,"interactive line budget exceeded; not executed\n");s->status=2;continue; }
        if (used) {
            cc_t last=(cc_t)buffer[used-1];
            if (last=='\n' || (t->modes.c_cc[VEOL]!=(cc_t)_POSIX_VDISABLE && last==t->modes.c_cc[VEOL]) ||
                    ((t->modes.c_lflag&IEXTEN) && t->modes.c_cc[VEOL2]!=(cc_t)_POSIX_VDISABLE && last==t->modes.c_cc[VEOL2])) --used;
        }
        if (used && buffer[used-1]=='\r') --used;
        (void)line(s,p,buffer,used);
        if (t->shutdown_signal) { s->status=ae_shell_jobs_shutdown(t);break; }
        if (s->exiting) break;
    }
    s->terminal=NULL;ae_line_destroy(editor);ae_shell_jobs_close(t);if (t->failed) s->status=125;free(t);return s->status;
}
int main(int argc,char **argv) {
    if (getuid()==0 || getuid()!=geteuid() || getgid()!=getegid()) { dprintf(2,"aetheri-shell: ordinary user execution required\n");return 125; }
    for (int i=0;i<3;++i) if (fcntl(i,F_GETFD)<0) return 125;
    /* An inherited SIGCHLD ignore/NOCLDWAIT disposition must not discard the
       original direct-child results before this shell can reap them. */
    struct sigaction child_action={.sa_handler=SIG_DFL};sigemptyset(&child_action.sa_mask);
    if (sigaction(SIGCHLD,&child_action,NULL)) return 125;
    const char *source=NULL;int fd=-1;bool editing=argc==3 && !strcmp(argv[1],"-i") && !strcmp(argv[2],"--edit");
    bool tty=editing || (argc==2 && !strcmp(argv[1],"-i"));
    if (tty) fd=0;
    else if (argc==3 && !strcmp(argv[1],"-c")) source=argv[2];
    else if (argc==3 && !strcmp(argv[1],"--file")) {
        fd=open(argv[2],O_RDONLY|O_CLOEXEC);
        if (fd<0) { dprintf(2,"aetheri-shell: script: %s\n",strerror(errno));return 1; }
    } else if (argc==2 && !strcmp(argv[1],"--stdin") && !isatty(0)) fd=0;
    else { dprintf(2,"usage: aetheri-shell -i [--edit] | -c SOURCE | --file FILE | --stdin (noninteractive)\n");return 2; }
    struct ae_shell_program *program=malloc(sizeof(*program));
    if (!program) { if (fd>0) close(fd);return 125; }
    struct ae_shell_session session={0};char buffer[AE_SHELL_BYTES];size_t used=0;
    if (tty) { int result=interactive(&session,program,editing);free(program);return result; }
    for (;;) {
        unsigned char c;ssize_t n;
        if (source) { c=(unsigned char)*source;n=c ? 1 : 0;if (c) ++source; }
        else { do { n=read(fd,&c,1); } while (n<0 && errno==EINTR); }
        if (n<0) { dprintf(2,"aetheri-shell: script read: %s\n",strerror(errno));session.status=1;break; }
        if (!n || c=='\n') {
            if (used && buffer[used-1]=='\r') --used;
            if (line(&session,program,buffer,used)<0 || session.exiting || !n) break;
            used=0;continue;
        }
        if (used==sizeof(buffer)-1) { dprintf(2,"aetheri-shell: source line budget exceeded\n");session.status=2;break; }
        buffer[used++]=(char)c;
    }
    free(program);if (fd>0) close(fd);return session.status;
}
