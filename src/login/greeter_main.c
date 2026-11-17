#define _GNU_SOURCE
#include "login/greeter_runner.h"
#include <fcntl.h>
#include <stdio.h>
#include <systemd/sd-journal.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>

static int redirect_diagnostics(void)
{
    if (!isatty(STDERR_FILENO)) return 0;
    int stream = sd_journal_stream_fd("aetheri-greeter", LOG_INFO, 0);
    if (stream < 0) return 0;
    int status = dup2(stream, STDERR_FILENO) < 0;
    return close(stream) || status;
}

int main(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) return 2;
    if (!getuid() || getuid() != geteuid() || !getgid() || getgid() != getegid() ||
        prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 1) return 1;
    for (int fd = 0; fd < 3; ++fd)
        if (fcntl(fd, F_GETFD) < 0) return 1;
    if (close_range(3, ~0u, CLOSE_RANGE_UNSHARE)) return 1;
    if (redirect_diagnostics()) return 1;

    struct ae_login_greeter_profile profile = {0};
    enum ae_login_ui_end end = AE_LOGIN_UI_STOPPED;
    int result = ae_login_greeter_deployment_select(&profile);
    if (!result) result = ae_login_greeter_run(&profile, &end);
    if (result) {
        fprintf(stderr, "AETHERI_GREETER_FAIL error=%d\n", result);
        return 1;
    }
    fprintf(stderr, "AETHERI_GREETER_END start_ack=%d\n", end == AE_LOGIN_UI_START_ACK);
    return 0;
}
