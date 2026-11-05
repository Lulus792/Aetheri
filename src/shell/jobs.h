#ifndef AETHERI_SHELL_JOBS_H
#define AETHERI_SHELL_JOBS_H
#include "execute.h"
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <termios.h>
enum { AE_SHELL_JOBS=16 };
enum ae_shell_child_state { AE_JOB_RUNNING, AE_JOB_STOPPED, AE_JOB_ENDED, AE_JOB_UNKNOWN };
struct ae_shell_child { pid_t pid; enum ae_shell_child_state state; int status; };
struct ae_shell_job {
    uint32_t id; pid_t group; size_t count;
    struct ae_shell_child children[AE_SHELL_COMMANDS];
    char label[257];
    struct termios modes;
    bool modes_valid, continuing;
    int notified;
};
struct ae_shell_terminal {
    int fd, signals, shutdown_signal;
    pid_t group, session;
    uint32_t next_id;
    struct termios modes;
    sigset_t old_mask;
    struct sigaction old_actions[8];
    unsigned actions;
    bool masked, failed;
    struct ae_shell_job jobs[AE_SHELL_JOBS], *active;
};
/* Private ephemeral terminal owner. Requires an independently foreground,
   canonical controlling terminal and an existing own process-group leader.
   This is not a durable app supervisor or arbitrary descendant-drain proof. */
int ae_shell_jobs_open(struct ae_shell_terminal *terminal);
void ae_shell_jobs_close(struct ae_shell_terminal *terminal);
bool ae_shell_jobs_busy(const struct ae_shell_terminal *terminal);
int ae_shell_jobs_check(struct ae_shell_terminal *terminal);
/* >=0 bytes/EOF; -2 interrupted input, -3 background notification, -1 shutdown. */
ssize_t ae_shell_jobs_read(struct ae_shell_terminal *terminal,char *buffer,size_t length);
int ae_shell_jobs_shutdown(struct ae_shell_terminal *terminal);
struct ae_shell_job *ae_shell_job_prepare(struct ae_shell_terminal *terminal,const char *label);
int ae_shell_job_add(struct ae_shell_job *job,pid_t child);
int ae_shell_job_take(struct ae_shell_terminal *terminal,struct ae_shell_job *job,bool resume);
int ae_shell_job_wait(struct ae_shell_terminal *terminal,struct ae_shell_job *job);
/* Fresh denied-start only: caller has closed the barrier and waited each child. */
void ae_shell_job_cancel(struct ae_shell_terminal *terminal,struct ae_shell_job *job);
int ae_shell_jobs_command(struct ae_shell_session *session,const struct ae_shell_command *command);
#endif
