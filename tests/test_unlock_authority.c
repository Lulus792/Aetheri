#define _GNU_SOURCE
#include "login/unlock_authority.h"

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "unlock authority check failed at line %d\n", __LINE__); exit(1); \
} } while (0)

enum { FIXTURE_ID = 65534 };
static const char password[] = "fixture credentials";

int __wrap_ae_unlock_pam_verify(uid_t uid, const char *account, const void *data, size_t length) {
    if (getuid() || geteuid() || uid != FIXTURE_ID || strcmp(account, "fixture"))
        return -EIO;
    return length == sizeof password - 1 && !memcmp(data, password, length) ? 0 : -EACCES;
}

static void client_wait(struct ae_unlock_authority_client *client) {
    struct pollfd fds[2];
    ae_unlock_authority_client_poll(client, fds);
    CHECK(poll(fds, 2, 4000) > 0);
}

static void attempt(struct ae_unlock_authority_client *client, const char *secret,
        enum ae_unlock_result expected) {
    CHECK(!ae_unlock_authority_client_begin(client, 1, secret, strlen(secret)));
    client_wait(client);
    enum ae_unlock_result observed = AE_UNLOCK_UNAVAILABLE;
    CHECK(!ae_unlock_authority_client_receive(client, 1, &observed) && observed == expected);
}

static void ordinary(int inherited, int sync, pid_t parent) {
    CHECK(!setgroups(0, NULL) && !setgid(FIXTURE_ID) && !setuid(FIXTURE_ID));
    CHECK(!prctl(PR_SET_PDEATHSIG, SIGTERM, 0L, 0L, 0L) && getppid() == parent);
    CHECK(!prctl(PR_SET_NO_NEW_PRIVS, 1L, 0L, 0L, 0L));
    char ready;
    CHECK(read(sync, &ready, 1) == 1 && ready == 'A');
    struct ae_unlock_authority_client *client = NULL;
    CHECK(!ae_unlock_authority_client_open(&client, inherited));
    CHECK(fcntl(inherited, F_GETFD) == FD_CLOEXEC && prctl(PR_GET_NO_NEW_PRIVS, 0L, 0L, 0L, 0L) == 1);
    attempt(client, "wrong fixture entry", AE_UNLOCK_DENIED);
    attempt(client, password, AE_UNLOCK_ACCEPTED);
    CHECK(write(sync, "D", 1) == 1);
    client_wait(client);
    enum ae_unlock_result outcome = AE_UNLOCK_DENIED;
    CHECK(ae_unlock_authority_client_receive(client, 1, &outcome) == -ESTALE);
    CHECK(outcome == AE_UNLOCK_DENIED);
    ae_unlock_authority_client_close(&client);
    _exit(0);
}

int main(void) {
    if (getuid() || geteuid()) return 77;
    int channel[2], sync[2];
    CHECK(!ae_unlock_authority_pair(channel));
    CHECK(!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sync));
    pid_t parent = getpid(), child = fork();
    CHECK(child >= 0);
    if (!child) {
        close(channel[0]); close(sync[0]);
        ordinary(channel[1], sync[1], parent);
    }
    close(sync[1]);
    int pin = pidfd_open(child, 0);
    CHECK(pin >= 0);
    struct ae_unlock_authority *authority = NULL;
    CHECK(ae_unlock_authority_open(&authority, channel[0], pin, FIXTURE_ID, FIXTURE_ID,
        "aetheri-greeter") == -EINVAL && !authority);
    CHECK(!ae_unlock_authority_open(&authority, channel[0], pin, FIXTURE_ID, FIXTURE_ID, "fixture"));
    CHECK(write(sync[0], "A", 1) == 1);
    for (;;) {
        struct pollfd fds[AE_UNLOCK_AUTHORITY_FDS + 1];
        int timeout;
        ae_unlock_authority_poll(authority, fds, &timeout);
        fds[AE_UNLOCK_AUTHORITY_FDS] = (struct pollfd){.fd = sync[0], .events = POLLIN};
        if (timeout < 0 || timeout > 4000) timeout = 4000;
        CHECK(poll(fds, sizeof fds / sizeof fds[0], timeout) > 0);
        if (fds[AE_UNLOCK_AUTHORITY_FDS].revents) break;
        CHECK(!ae_unlock_authority_step(authority));
    }
    char done;
    CHECK(read(sync[0], &done, 1) == 1 && done == 'D');
    /* This well-formed next attempt comes from a copy of the endpoint,
       outside the original desktop process. The authority must fence it. */
    const unsigned char foreign[] = {'A','E','U','A','1',0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,3,'x'};
    CHECK(send(channel[1], foreign, sizeof foreign, MSG_NOSIGNAL) == (ssize_t)sizeof foreign);
    close(channel[1]);
    CHECK(ae_unlock_authority_step(authority) == -EPROTO);
    CHECK(ae_unlock_authority_step(authority) == -ESTALE);
    ae_unlock_authority_close(&authority);
    close(sync[0]); close(pin);
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    return 0;
}
