#define _GNU_SOURCE
#include "login/unlock_channel.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "unlock channel check failed at line %d\n", __LINE__); exit(1); \
} } while (0)

enum fixture {
    GOOD, BAD_TICKET, FOREIGN_PROCESS, SECOND_PACKET, EXTRA_FDS, NONZERO_EXIT,
    EMPTY_PACKET, MISSING_REPLY, LATE_EXIT, RETAINED_PEER, CANCELLED,
};
static const char password[] = "synthetic only";
enum { GENERATION = 71, HEADER_SIZE = 22 };

static unsigned open_fds(void) {
    DIR *dir = opendir("/proc/self/fd");
    CHECK(dir);
    unsigned count = 0;
    while (readdir(dir)) ++count;
    CHECK(!closedir(dir));
    return count;
}

static void answer_with_fds(int fd, unsigned char *reply, size_t length) {
    int source = open("/dev/null", O_RDONLY | O_CLOEXEC);
    CHECK(source >= 0);
    int passed[12];
    for (size_t i = 0; i < sizeof passed / sizeof passed[0]; ++i) passed[i] = source;
    struct iovec part = {reply, length};
    union { struct cmsghdr alignment; unsigned char b[CMSG_SPACE(sizeof passed)]; } control = {0};
    struct msghdr packet = {.msg_iov = &part, .msg_iovlen = 1,
        .msg_control = control.b, .msg_controllen = sizeof control.b};
    struct cmsghdr *header = CMSG_FIRSTHDR(&packet);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof passed);
    memcpy(CMSG_DATA(header), passed, sizeof passed);
    CHECK(sendmsg(fd, &packet, MSG_NOSIGNAL) == (ssize_t)length);
    close(source);
}

static void actor(int fd, int gate, enum fixture mode) {
    unsigned char request[128];
    ssize_t size = recv(fd, request, sizeof request, 0);
    CHECK(size == HEADER_SIZE + (ssize_t)sizeof password - 1);
    CHECK(!memcmp(request, "AEUP1Q", 6) && !memcmp(request + HEADER_SIZE, password, sizeof password - 1));
    unsigned char reply[HEADER_SIZE + 1];
    memcpy(reply, request, HEADER_SIZE);
    memcpy(reply, "AEUP1R", 6);
    reply[HEADER_SIZE] = AE_UNLOCK_ACCEPTED;
    if (mode == BAD_TICKET) reply[6] ^= 1u;
    if (mode == FOREIGN_PROCESS) {
        pid_t other = fork();
        CHECK(other >= 0);
        if (other) {
            int code;
            CHECK(waitpid(other, &code, 0) == other && WIFEXITED(code) && !WEXITSTATUS(code));
            _exit(0);
        }
    }
    if (mode == EXTRA_FDS) answer_with_fds(fd, reply, sizeof reply);
    else if (mode != MISSING_REPLY && mode != CANCELLED) {
        size_t length = mode == EMPTY_PACKET ? 0 : sizeof reply;
        CHECK(send(fd, reply, length, MSG_NOSIGNAL) == (ssize_t)length);
        if (mode == SECOND_PACKET) CHECK(send(fd, reply, length, MSG_NOSIGNAL) == (ssize_t)length);
    }
    close(fd);
    if (mode == LATE_EXIT) {
        char signal;
        CHECK(read(gate, &signal, 1) == 1 && signal == 'x');
    }
    _exit(mode == NONZERO_EXIT ? 37 : 0);
}

static int take(struct ae_unlock_attempt *a, enum ae_unlock_result *result) {
    for (int i = 0; i < 8; ++i) {
        struct pollfd events[2];
        CHECK(!ae_unlock_attempt_poll(a, events));
        CHECK(poll(events, 2, 2000) > 0);
        int status = ae_unlock_attempt_receive(a, GENERATION, result);
        if (status != -EAGAIN) return status;
    }
    CHECK(false);
    return -EIO;
}

static void verify(enum fixture mode, int expected) {
    unsigned before = open_fds();
    int pair[2], gate[2];
    CHECK(!ae_unlock_worker_pair(pair));
    CHECK(!pipe2(gate, O_CLOEXEC));
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        close(pair[0]); close(gate[1]);
        actor(pair[1], gate[0], mode);
    }
    if (mode != RETAINED_PEER) close(pair[1]);
    close(gate[0]);
    int pidfd = pidfd_open(child, 0);
    CHECK(pidfd >= 0);
    struct ae_unlock_attempt *attempt = NULL;
    CHECK(!ae_unlock_attempt_begin(&attempt, pair[0], pidfd, GENERATION, password, sizeof password - 1));
    enum ae_unlock_result result = AE_UNLOCK_UNAVAILABLE;
    int status;
    if (mode == LATE_EXIT) {
        struct pollfd events[2];
        CHECK(!ae_unlock_attempt_poll(attempt, events));
        CHECK(poll(events, 2, 2000) > 0);
        CHECK(ae_unlock_attempt_receive(attempt, GENERATION, &result) == -EAGAIN);
        CHECK(result == AE_UNLOCK_UNAVAILABLE);
        CHECK(write(gate[1], "x", 1) == 1);
    }
    if (mode == CANCELLED) status = ae_unlock_attempt_receive(attempt, GENERATION + 1, &result);
    else status = take(attempt, &result);
    CHECK(status == expected && result == (expected ? AE_UNLOCK_UNAVAILABLE : AE_UNLOCK_ACCEPTED));
    CHECK(ae_unlock_attempt_receive(attempt, GENERATION, &result) == -ESTALE);
    ae_unlock_attempt_close(&attempt);
    if (mode == RETAINED_PEER) close(pair[1]);
    close(pidfd); close(gate[1]);
    int code;
    CHECK(waitpid(child, &code, 0) == child && WIFEXITED(code));
    CHECK(WEXITSTATUS(code) == (mode == NONZERO_EXIT ? 37 : 0));
    CHECK(!attempt && open_fds() == before);
}

int main(void) {
    verify(GOOD, 0);
    verify(BAD_TICKET, -EPROTO);
    verify(FOREIGN_PROCESS, -EPROTO);
    verify(SECOND_PACKET, -EPROTO);
    verify(EXTRA_FDS, -EPROTO);
    verify(NONZERO_EXIT, -ECHILD);
    verify(EMPTY_PACKET, -EPROTO);
    verify(MISSING_REPLY, -ECONNRESET);
    verify(LATE_EXIT, 0);
    verify(RETAINED_PEER, -EPROTO);
    verify(CANCELLED, -ESTALE);
    return 0;
}
