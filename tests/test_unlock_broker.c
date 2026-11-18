#define _GNU_SOURCE
#include "compositor/lock_wire.h"
#include "login/unlock_broker.h"
#include "login/unlock_view_contract.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/pidfd.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "unlock broker check failed at line %d\n", __LINE__); exit(1); \
} } while (0)

static void pair(int fds[2]) { CHECK(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds)); }

static void send_frame(int fd, enum ae_lock_action action) {
    unsigned char frame[AE_LOCK_WIRE_SIZE];
    ae_lock_wire_write(frame, action, 1);
    CHECK(send(fd, frame, sizeof frame, MSG_NOSIGNAL) == (ssize_t)sizeof frame);
}

static void check_parent(pid_t owner) {
    CHECK(!prctl(PR_SET_PDEATHSIG, SIGTERM, 0L, 0L, 0L) && getppid() == owner);
}

static void wait_child(pid_t child) {
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
}

static bool session_live(void *flag) { return *(bool *)flag; }

int main(int argc, char **argv) {
    if (!getuid()) return 77;
    CHECK(argc == 2);
    int control[2], credential[2], gate[2];
    pair(control);
    pair(credential);
    pair(gate);
    pid_t owner = getpid();
    pid_t compositor = fork();
    CHECK(compositor >= 0);
    if (!compositor) {
        check_parent(owner);
        close(control[0]); close(credential[0]); close(credential[1]); close(gate[0]);
        char flag;
        CHECK(recv(gate[1], &flag, 1, 0) == 1 && flag == 'a');
        const int enabled = 1;
        CHECK(!setsockopt(control[1], SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled));
        send_frame(control[1], AE_LOCK_REQUEST);
        send_frame(control[1], AE_LOCK_COVERED);
        CHECK(send(gate[1], "b", 1, MSG_NOSIGNAL) == 1);
        CHECK(recv(control[1], &flag, 1, 0) == 0);
        _exit(0);
    }
    pid_t view = fork();
    CHECK(view >= 0);
    if (!view) {
        check_parent(owner);
        close(control[0]); close(control[1]); close(credential[0]); close(gate[0]); close(gate[1]);
        char flag;
        CHECK(recv(credential[1], &flag, 1, 0) == 0);
        _exit(0);
    }
    close(control[1]); close(gate[1]);
    int pins[] = {pidfd_open(compositor, 0), pidfd_open(view, 0)};
    CHECK(pins[0] >= 0 && pins[1] >= 0);
    bool admitted = true;
    struct ae_unlock_broker *broker = NULL;
    struct ae_unlock_broker_config profile = {
        .control = control[0], .credentials = credential[0],
        .compositor = pins[0], .view = pins[0], .worker = argv[1], .account = "fixture",
        .session_live = session_live, .session = &admitted,
    };
    CHECK(ae_unlock_broker_open(&broker, &profile) == -ESTALE && !broker);
    profile.view = pins[1];
    CHECK(!ae_unlock_broker_open(&broker, &profile));
    CHECK(ae_unlock_broker_open_desktop(broker));
    CHECK(send(gate[0], "a", 1, MSG_NOSIGNAL) == 1);
    char gate_reply;
    CHECK(recv(gate[0], &gate_reply, 1, 0) == 1 && gate_reply == 'b');
    CHECK(!ae_unlock_broker_step(broker) && !ae_unlock_broker_open_desktop(broker));
    CHECK(!ae_unlock_broker_step(broker) && !ae_unlock_broker_open_desktop(broker));

    /* The original view remains alive. A copied peer endpoint and well-formed
       synthetic payload cannot grant another process its sender identity. */
    const char candidate[] = AE_UNLOCK_VIEW_MAGIC "fixture";
    CHECK(send(credential[1], candidate, sizeof candidate - 1, MSG_NOSIGNAL) == (ssize_t)sizeof candidate - 1);
    close(credential[1]);
    CHECK(ae_unlock_broker_step(broker) == -EPROTO && !ae_unlock_broker_open_desktop(broker));
    CHECK(ae_unlock_broker_step(broker) == -ESTALE);
    struct pollfd fds[AE_UNLOCK_BROKER_FDS];
    int timeout;
    ae_unlock_broker_poll(broker, fds, &timeout);
    CHECK(timeout == -1);
    for (size_t i = 0; i < AE_UNLOCK_BROKER_FDS; ++i) CHECK(fds[i].fd == -1);
    ae_unlock_broker_close(&broker);
    CHECK(!broker);
    close(gate[0]);
    close(pins[0]); close(pins[1]);
    wait_child(compositor);
    wait_child(view);
    return 0;
}
