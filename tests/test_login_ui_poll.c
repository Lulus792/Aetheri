#define _GNU_SOURCE
#include "connection_poll.h"
#include "login/connection.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/pidfd.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "login UI poll:%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static bool native_polled;

nx_result __wrap_nx_window_poll_with_fd_events(nx_window *window, uint32_t timeout, int fd,
    uint32_t interests, uint32_t *out)
{
    CHECK(window && fd >= 0 && timeout <= 100 && interests == NX_WINDOW_FD_READABLE);
    native_polled = true;
    *out = NX_WINDOW_FD_READABLE;
    return NX_OK;
}

static int connection_test(void *error)
{
    return *(const int *)error;
}

static int input_gate(void *state)
{
    CHECK(native_polled);
    return *(const int *)state;
}

static void check(int native_error, int backend_error)
{
    int fd[2];
    CHECK(!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd));
    int pin = pidfd_open(getpid(), 0);
    CHECK(pin >= 0);
    int admission = 0;
    struct ae_login_connection_source source = { .peer_pidfd = pin, .uid = getuid(),
        .gid = getgid(), .check = connection_test, .userdata = &admission };
    struct ae_login_connection *connection = NULL;
    CHECK(!ae_login_connection_create(&connection, &fd[0], &source) && fd[0] == -1);
    CHECK(!ae_login_connection_begin(connection, "aetheri", 7));
    admission = backend_error;
    aetheri_login_poll *poller = NULL;
    CHECK(aetheri_login_poll_open(connection, -1, &poller) == NX_OK);
    native_polled = false;
    uint64_t deadline = 0;
    CHECK(aetheri_login_poll_time(&deadline) == NX_OK);
    aetheri_login_poll_outcome outcome;
    CHECK(aetheri_login_poll_step_guarded(poller, (nx_window *)(void *)&native_polled,
        100, deadline + 1000000000u, input_gate, &native_error, &outcome) == NX_OK);
    CHECK(native_polled);
    struct ae_login_status status;
    unsigned char bytes[128];
    ssize_t received = recv(fd[1], bytes, sizeof bytes, MSG_DONTWAIT);
    int receive_error = errno;
    CHECK(!ae_login_connection_status(connection, &status));
    if (native_error || backend_error) {
        CHECK(outcome.connection_error == (native_error ? native_error : backend_error) &&
            status.phase == AE_LOGIN_UNKNOWN);
        CHECK(received == -1 && receive_error == EAGAIN);
        CHECK(ae_login_connection_pump(connection) == (native_error ? -ECANCELED : backend_error));
    } else {
        CHECK(!outcome.connection_error && status.phase == AE_LOGIN_WAITING && received > 4);
    }
    aetheri_login_poll_close(&poller);
    CHECK(!ae_login_connection_close(&connection));
    close(pin);
    close(fd[1]);
}

int main(void)
{
    check(0, 0);
    check(-ESTALE, 0);
    check(-ECANCELED, -EAGAIN);
    check(0, -EAGAIN);
    check(-ECANCELED, -ESTALE);
    return 0;
}
