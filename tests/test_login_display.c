#define _GNU_SOURCE
#include "login/display.h"
#include "login/display_channel.h"
#include "linux/image_internal.h"
#include "linux/image.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <selinux/selinux.h>
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

#define CHECK(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); exit(1); } } while (0)

static bool use_real_image;
static pid_t expected_child;
static unsigned image_calls;
int __real_ae_image_match_pinned(const struct ae_image_process *, const struct ae_image *, struct ae_image *);
int __wrap_ae_image_match_pinned(const struct ae_image_process *p, const struct ae_image *expected,
    struct ae_image *actual)
{
    CHECK(p->pid == expected_child && p->pidfd >= 0 && p->uid == getuid() && p->gid == getgid());
    ++image_calls;
    if (use_real_image) return __real_ae_image_match_pinned(p, expected, actual);
    *actual = *expected;
    return 0;
}

enum case_kind { CORRECT, FOREIGN_CREATOR, EXTRA_RIGHT, LOST_CHILD };

static void extra_right(int control, const int rights[2])
{
    char signature[] = "AGW1";
    struct iovec body = { .iov_base = signature, .iov_len = sizeof signature - 1 };
    union { struct cmsghdr aligned; unsigned char bytes[CMSG_SPACE(2 * sizeof(int))]; } ancillary = {0};
    struct msghdr packet = { .msg_iov = &body, .msg_iovlen = 1,
        .msg_control = ancillary.bytes, .msg_controllen = sizeof ancillary.bytes };
    struct cmsghdr *header = CMSG_FIRSTHDR(&packet);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(2 * sizeof(int));
    memcpy(CMSG_DATA(header), rights, 2 * sizeof(int));
    CHECK(sendmsg(control, &packet, MSG_NOSIGNAL) == 4);
}

static void check_offer(enum case_kind kind, const struct ae_login_display_expect *expected)
{
    int control[2], gate[2], foreign[2];
    CHECK(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, control));
    CHECK(!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, foreign));
    CHECK(!pipe2(gate, O_CLOEXEC));
    pid_t parent = getpid();
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        CHECK(!prctl(PR_SET_PDEATHSIG, SIGKILL) && getppid() == parent);
        close(control[0]); close(gate[1]); close(foreign[0]);
        char ready;
        CHECK(read(gate[0], &ready, 1) == 1);
        close(gate[0]);
        if (kind == LOST_CHILD) _exit(0);
        int stream[2];
        CHECK(!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, stream));
        if (kind == EXTRA_RIGHT) extra_right(control[1], stream);
        else CHECK(!ae_login_display_offer(control[1], kind == CORRECT ? stream[1] : foreign[1]));
        close(stream[1]); close(control[1]);
        int held = kind == FOREIGN_CREATOR ? foreign[1] : stream[0];
        while (read(held, &ready, 1) < 0 && errno == EINTR) {}
        close(stream[0]); close(foreign[1]);
        _exit(0);
    }
    expected_child = child;
    unsigned before = image_calls;
    close(control[1]); close(gate[0]); close(foreign[1]);
    int child_fd = pidfd_open(child, 0);
    CHECK(child_fd >= 0);
    struct ae_login_display *display = NULL;
    CHECK(!ae_login_display_create(&control[0], child_fd, expected, &display) && control[0] == -1);
    int stream = -1, control_fd = -1, original_child = -1;
    CHECK(ae_login_display_take_original(display, &stream) == -EAGAIN && stream == -1);
    CHECK(!ae_login_display_watch(display, &control_fd, &original_child));
    CHECK(write(gate[1], "R", 1) == 1);
    close(gate[1]);
    struct pollfd watch = { .fd = kind == LOST_CHILD ? original_child : control_fd, .events = POLLIN };
    CHECK(poll(&watch, 1, 3000) == 1);
    int result = ae_login_display_take_original(display, &stream);
    if (kind == CORRECT) {
        CHECK(!result && stream >= 0 && (fcntl(stream, F_GETFD) & FD_CLOEXEC));
        CHECK(!ae_login_display_check(display) && image_calls == before + 2);
        int extra = -1;
        CHECK(ae_login_display_take_original(display, &extra) == -EALREADY && extra == -1);
        close(stream);
    } else {
        int wanted = kind == FOREIGN_CREATOR ? -ESTALE : kind == EXTRA_RIGHT ? -EPROTO : -ESRCH;
        CHECK(result == wanted && stream == -1 && image_calls == before);
        CHECK(ae_login_display_take_original(display, &stream) == result && stream == -1);
        if (kind == FOREIGN_CREATOR) CHECK(!pidfd_send_signal(child_fd, SIGTERM, NULL, 0));
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child);
    if (kind == FOREIGN_CREATOR) CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM);
    else CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (kind == CORRECT) {
        CHECK(ae_login_display_check(display) == -ESRCH);
        CHECK(ae_login_display_check(display) == -ESRCH);
    }
    CHECK(!ae_login_display_close(&display) && !display);
    close(child_fd); close(foreign[0]);
}

int main(int argc, char **argv)
{
    if (!getuid() || !getgid() || security_getenforce() != 1) return 77;
    char *process = NULL, socket_label[512] = {0};
    int peers[2];
    socklen_t size = sizeof socket_label - 1;
    CHECK(!getcon_raw(&process) && process);
    CHECK(!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, peers));
    CHECK(!getsockopt(peers[0], SOL_SOCKET, SO_PEERSEC, socket_label, &size));
    close(peers[0]); close(peers[1]);
    struct ae_login_display_expect expected = { .process_label = process, .socket_label = socket_label };
    if (argc == 3) {
        CHECK(strlen(argv[2]) == 64 && strspn(argv[2], "0123456789abcdef") == 64);
        uint8_t digest[32];
        for (size_t i = 0; i < sizeof digest; ++i) {
            unsigned byte;
            CHECK(sscanf(argv[2] + 2 * i, "%2x", &byte) == 1);
            digest[i] = (uint8_t)byte;
        }
        int file = open(argv[1], O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        CHECK(file >= 0 && !ae_image_verify(file, digest, &expected.image) && !close(file));
        use_real_image = true;
    } else CHECK(argc == 1);
    check_offer(CORRECT, &expected);
    check_offer(FOREIGN_CREATOR, &expected);
    check_offer(EXTRA_RIGHT, &expected);
    check_offer(LOST_CHILD, &expected);
    freecon(process);
    puts(use_real_image ? "DISPLAY_PROTECTED_IMAGE_OK" : "DISPLAY_KERNEL_PEER_OK");
    return 0;
}
