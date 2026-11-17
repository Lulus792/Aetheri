#define _GNU_SOURCE
#include "login/greeter_runner.h"
#include "linux/image.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

struct child {
    pid_t pid;
    int pin;
};

static int connect_hint(void)
{
    const char *path = getenv("GREETD_SOCK");
    static const char prefix[] = "/run/greetd-";
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    if (!path || strnlen(path, sizeof address.sun_path) == sizeof address.sun_path ||
        strncmp(path, prefix, sizeof prefix - 1)) return -EINVAL;
    const char *number = path + sizeof prefix - 1;
    size_t digits = strspn(number, "0123456789");
    if (!digits || digits > 10 || number[0] == '0' || strcmp(number + digits, ".sock"))
        return -EINVAL;
    strcpy(address.sun_path, path);
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) return -errno;
    if (!connect(fd, (const struct sockaddr *)&address, sizeof address)) return fd;
    int result = -errno;
    close(fd);
    return result;
}

static int system_bus(sd_bus **out)
{
    int result = sd_bus_new(out);
    if (result >= 0) result = sd_bus_set_address(*out, "unix:path=/run/dbus/system_bus_socket");
    if (result >= 0) result = sd_bus_set_bus_client(*out, 1);
    if (result >= 0) result = sd_bus_set_method_call_timeout(*out, 1000000);
    if (result >= 0) result = sd_bus_start(*out);
    return result < 0 ? result : 0;
}

static int runtime_check(const char *path)
{
    if (!path || path[0] != '/') return -EINVAL;
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -errno;
    struct stat info;
    int result = fstat(fd, &info) ? -errno : 0;
    if (!result && (info.st_uid != getuid() || info.st_gid != getgid() ||
                    (info.st_mode & 07777) != 0700)) result = -EACCES;
    close(fd);
    return result;
}

static int retired(struct child *child)
{
    if (!child->pid) return 0;
    if (child->pin < 0) {
        int status;
        pid_t waited;
        do { waited = waitpid(child->pid, &status, 0); } while (waited < 0 && errno == EINTR);
        return waited == child->pid ? 0 : -ECHILD;
    }
    struct pollfd end = { .fd = child->pin, .events = POLLIN };
    int ready = poll(&end, 1, 500);
    if (ready < 0) return -errno;
    if (!ready) {
        if (pidfd_send_signal(child->pin, SIGTERM, NULL, 0) && errno != ESRCH) return -errno;
        ready = poll(&end, 1, 4500);
        if (!ready) return -ETIMEDOUT;
        if (ready < 0) return -errno;
    }
    if (!(end.revents & POLLIN)) return -EIO;
    int status;
    pid_t waited;
    do { waited = waitpid(child->pid, &status, WNOHANG); } while (waited < 0 && errno == EINTR);
    if (waited != child->pid) return -ECHILD;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -EIO;
}

static void exec_child(int executable, int channel, int gate, pid_t original,
    const char *path, const struct ae_login_greeter_profile *profile)
{
    char runtime[AE_LOGIN_SOURCE_FULL_PATH_MAX + 32];
    char libraries[AE_LOGIN_SOURCE_FULL_PATH_MAX + 32];
    char quirks[AE_LOGIN_SOURCE_FULL_PATH_MAX + 32];
    char descriptor[24];
    char permit;
    if (prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0) || getppid() != original ||
        read(gate, &permit, 1) != 1 || permit != 'G') _exit(125);
    close(gate);
    sigset_t mask;
    struct sigaction pipe_signal = { .sa_handler = SIG_DFL };
    if (sigemptyset(&mask) || sigprocmask(SIG_SETMASK, &mask, NULL) ||
        sigemptyset(&pipe_signal.sa_mask) || sigaction(SIGPIPE, &pipe_signal, NULL) ||
        fcntl(channel, F_SETFD, 0)) _exit(125);
    if (snprintf(descriptor, sizeof descriptor, "%d", channel) < 1) _exit(125);
    snprintf(runtime, sizeof runtime, "XDG_RUNTIME_DIR=%s", profile->runtime);
    char *environment[7] = { "LIBSEAT_BACKEND=logind", "LANG=C.UTF-8",
        "PATH=/usr/bin", runtime };
    unsigned entry = 4;
    if (profile->libraries) {
        snprintf(libraries, sizeof libraries, "LD_LIBRARY_PATH=%s", profile->libraries);
        environment[entry++] = libraries;
    }
    if (profile->input_quirks) {
        snprintf(quirks, sizeof quirks, "LIBINPUT_QUIRKS_DIR=%s", profile->input_quirks);
        environment[entry] = quirks;
    }
    char *arguments[] = { (char *)path, "--drm", "aetheri-login", "-", "auto",
        "--greeter-fd", descriptor, NULL };
    execveat(executable, "", arguments, environment, AT_EMPTY_PATH);
    _exit(126);
}

static int start_child(const struct ae_login_greeter_profile *p, int executable,
    const char *path, const struct ae_image *image, struct child *child,
    struct ae_login_display **display)
{
    int channel[2], gate[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, channel)) return -errno;
    if (pipe2(gate, O_CLOEXEC)) {
        int result = -errno;
        close(channel[0]); close(channel[1]);
        return result;
    }
    pid_t original = getpid();
    child->pid = fork();
    if (!child->pid) {
        close(channel[0]); close(gate[1]);
        exec_child(executable, channel[1], gate[0], original, path, p);
    }
    int result = child->pid < 0 ? -errno : 0;
    if (child->pid < 0) child->pid = 0;
    close(channel[1]); close(gate[0]);
    if (!result) {
        child->pin = pidfd_open(child->pid, 0);
        if (child->pin < 0) result = -errno;
    }
    const struct ae_login_display_expect expected = { .image = *image,
        .process_label = p->compositor_label, .socket_label = p->compositor_socket_label };
    if (!result) result = ae_login_display_create(&channel[0], child->pin, &expected, display);
    if (!result && write(gate[1], "G", 1) != 1) result = -EIO;
    if (channel[0] >= 0) close(channel[0]);
    close(gate[1]);
    return result;
}

static bool optional_directory(const char *path)
{
    return !path || (path[0] == '/' && strnlen(path, AE_LOGIN_SOURCE_FULL_PATH_MAX) <
                     AE_LOGIN_SOURCE_FULL_PATH_MAX && !strchr(path, ':'));
}

int ae_login_greeter_run(const struct ae_login_greeter_profile *p, enum ae_login_ui_end *out)
{
    if (!p || !out || !p->backend.check || !p->seat || !p->vt ||
        !p->runtime || !optional_directory(p->runtime) || !optional_directory(p->libraries) ||
        !optional_directory(p->input_quirks) || !p->font || !p->font_size ||
        p->font_size > 16u * 1024u * 1024u || getpid() != gettid()) return -EINVAL;
    int result = p->backend.check(p->backend.userdata);
    if (result) return result < 0 ? result : -EACCES;
    result = runtime_check(p->runtime);
    struct ae_session *session = NULL;
    struct ae_login_sources *sources = NULL;
    struct ae_login_display *display = NULL;
    struct child child = { .pin = -1 };
    sd_bus *bus = NULL;
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    int executable = -1, backend = -1, stop = -1;
    enum ae_login_ui_end end = AE_LOGIN_UI_STOPPED;
    struct ae_image image;
    char path[AE_LOGIN_SOURCE_FULL_PATH_MAX];
    sigset_t signals;
    if (!result && (sigemptyset(&signals) || sigaddset(&signals, SIGTERM) ||
        sigaddset(&signals, SIGINT) || sigprocmask(SIG_BLOCK, &signals, NULL))) result = -errno;
    struct sigaction pipe_signal = { .sa_handler = SIG_IGN };
    if (!result && (sigemptyset(&pipe_signal.sa_mask) || sigaction(SIGPIPE, &pipe_signal, NULL)))
        result = -errno;
    if (!result) {
        stop = signalfd(-1, &signals, SFD_NONBLOCK | SFD_CLOEXEC);
        if (stop < 0) result = -errno;
    }
    if (!result) result = ae_session_open_greeter(p->backend.greeter.uid, p->seat, p->vt, &session);
    if (!result) result = ae_login_sources_create(&p->compositor, 1, &sources);
    if (!result) result = ae_login_sources_path(sources, 0, AE_LOGIN_SOURCE_ELF, path);
    if (!result) result = ae_login_sources_image(sources, 0, &image);
    if (!result) {
        executable = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (executable < 0) result = -errno;
    }
    struct ae_image actual;
    if (!result) result = ae_image_verify(executable, image.digest, &actual);
    if (!result && !ae_image_equal(&image, &actual)) result = -ESTALE;
    if (!result) result = system_bus(&bus);
    if (!result) {
        backend = connect_hint();
        if (backend < 0) result = backend;
    }
    if (!result) result = ae_session_check_greeter(session);
    if (!result) result = start_child(p, executable, path, &image, &child, &display);
    if (!result && (nx_font_open(p->font, p->font_size, NULL, &font) != NX_OK ||
        nx_font_collection_create(NULL, &fonts) != NX_OK || nx_font_collection_add(fonts, font) != NX_OK))
        result = -EIO;
    if (!result) {
        const struct ae_login_ui_profile ui = { .display = display, .session = session,
            .fonts = fonts, .backend = &p->backend, .stop_fd = stop };
        result = ae_login_ui_run(&ui, bus, &backend, &end);
    }
    ae_login_display_close(&display);
    int ended = retired(&child);
    if (!result) result = ended;
    ae_login_sources_close(&sources);
    ae_session_close(&session);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    sd_bus_unref(bus);
    const int descriptors[] = {child.pin, executable, backend, stop};
    for (size_t i = 0; i < sizeof descriptors / sizeof descriptors[0]; ++i)
        if (descriptors[i] >= 0) close(descriptors[i]);
    if (!result) *out = end;
    return result;
}
