#define _GNU_SOURCE
#include "login/ui/host.h"
#include "connection_poll.h"
#include <nexaui/software.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

enum { STARTUP_MS = 15000, REQUEST_MS = 10000, MAX_EVENTS = 256 };
struct host {
    const struct ae_login_ui_profile *profile;
    nx_window *window;
    nx_software_view *renderer;
    aetheri_login_view *view;
    int wake;
    bool focused, enabled, dirty;
};

static int stopped(int fd)
{
    struct pollfd stop = { .fd = fd, .events = POLLIN };
    int result = poll(&stop, 1, 0);
    if (result < 0) return -errno;
    return result ? -ECANCELED : 0;
}

static int current(struct host *host)
{
    int result = stopped(host->profile->stop_fd);
    if (!result) result = ae_session_check_greeter(host->profile->session);
    if (!result) result = ae_login_display_check(host->profile->display);
    return result;
}

static int read_events(void *pointer)
{
    struct host *host = pointer;
    int result = current(host);
    if (result) return result;
    nx_window_info info;
    if (nx_window_get_info(host->window, &info) != NX_OK) return -EIO;
    if (info.should_close) return -ECANCELED;
    for (unsigned count = 0; count < MAX_EVENTS; ++count) {
        nx_event event;
        int available;
        if (nx_window_next_event(host->window, &event, &available) != NX_OK) return -EIO;
        if (!available) return 0;
        if (event.type == NX_EVENT_FOCUS) {
            if (host->enabled && !event.focused) return -ESTALE;
            host->focused = event.focused != 0;
        }
        nx_result routed = NX_OK;
        if (host->enabled && host->focused)
            routed = nx_context_queue_event(aetheri_login_view_context(host->view), &event);
        explicit_bzero(&event, sizeof event);
        if (routed != NX_OK) return -EIO;
        host->dirty = true;
    }
    return -EOVERFLOW;
}

static int admission(void *pointer)
{
    struct host *host = pointer;
    int result = current(host);
    if (!result && !host->focused) result = -ESTALE;
    if (!result) result = host->profile->backend->check(host->profile->backend->userdata);
    return result > 0 ? -EACCES : result;
}

static int set_deadline(uint64_t *out, unsigned milliseconds)
{
    if (aetheri_login_poll_time(out) != NX_OK) return -EIO;
    uint64_t budget = (uint64_t)milliseconds * 1000000u;
    if (*out > UINT64_MAX - budget) return -EOVERFLOW;
    *out += budget;
    return 0;
}

static int within(uint64_t deadline)
{
    uint64_t now;
    if (aetheri_login_poll_time(&now) != NX_OK) return -EIO;
    return now < deadline ? 0 : -ETIMEDOUT;
}

static int receive_display(const struct ae_login_ui_profile *profile, uint64_t deadline, int *fd)
{
    for (;;) {
        int result = within(deadline);
        if (!result) result = stopped(profile->stop_fd);
        if (!result) result = ae_session_check_greeter(profile->session);
        if (result) return result;
        result = ae_login_display_take_original(profile->display, fd);
        if (result != -EAGAIN) return result;
        int channel, child;
        result = ae_login_display_watch(profile->display, &channel, &child);
        if (result) return result;
        struct pollfd events[] = {
            { .fd = channel, .events = POLLIN }, { .fd = child, .events = POLLIN },
            { .fd = profile->stop_fd, .events = POLLIN },
        };
        if (poll(events, 3, 50) < 0 && errno != EINTR) return -errno;
    }
}

static void *native_allocate(void *unused, size_t size)
{
    (void)unused;
    return malloc(size);
}

static void native_release(void *unused, void *memory, size_t size)
{
    (void)unused;
    if (memory) explicit_bzero(memory, size);
    free(memory);
}

static int open_window(struct host *host, int *stream)
{
    char descriptor[32];
    int n = snprintf(descriptor, sizeof descriptor, "%d", *stream);
    if (n < 1 || (size_t)n >= sizeof descriptor) return -EINVAL;
    if (setenv("NEXAUI_PLATFORM", "wayland", 1) || unsetenv("DISPLAY") ||
        unsetenv("WAYLAND_DISPLAY") || unsetenv("WAYLAND_DEBUG") ||
        setenv("WAYLAND_SOCKET", descriptor, 1)) return -errno;
    const nx_window_desc desc = { .title_utf8 = "Sign in to Aetheri", .width = 540, .height = 340,
        .allocator = { NULL, native_allocate, native_release } };
    nx_result opened = nx_window_create(&desc, &host->window);
    bool consumed = getenv("WAYLAND_SOCKET") == NULL;
    if (consumed) *stream = -1;
    else unsetenv("WAYLAND_SOCKET");
    if (!consumed || opened != NX_OK ||
        nx_window_set_wayland_app_id(host->window, "org.aetheri.Greeter") != NX_OK ||
        nx_software_view_open(host->window, &host->renderer) != NX_OK ||
        aetheri_login_view_create_connection(NULL, host->profile->fonts, host->window, &host->view) != NX_OK)
        return -EIO;
    return 0;
}

static int render(struct host *host, bool *submitted, uint64_t *request_deadline)
{
    nx_window_info info;
    if (nx_window_get_info(host->window, &info) != NX_OK) return -EIO;
    if (info.should_close) return -ECANCELED;
    if (info.width < 240 || info.height < 200) return -ERANGE;
    if (!host->dirty) return 0;
    unsigned changes;
    for (unsigned frame = 0;; ++frame) {
        if (frame == 4 || aetheri_login_view_frame(host->view, info.width, info.height, &changes) != NX_OK)
            return -EIO;
        if (changes & AETHERI_LOGIN_REQUEST_QUEUED) {
            if (!host->enabled || *request_deadline) return -EPROTO;
            int result = set_deadline(request_deadline, REQUEST_MS);
            if (result) return result;
        }
        if (!(changes & AETHERI_LOGIN_REPAINT)) break;
    }
    const nx_command *commands;
    size_t count;
    nx_software_view_outcome outcome;
    const nx_color background = {17.f/255, 22.f/255, 32.f/255, 1.f};
    if (nx_frame_commands(aetheri_login_view_context(host->view), &commands, &count) != NX_OK ||
        nx_software_view_render(host->renderer, commands, count, background, &outcome) != NX_OK)
        return -EIO;
    *submitted = outcome == NX_SOFTWARE_VIEW_READY;
    host->dirty = !*submitted;
    return 0;
}

static int wait_window(struct host *host, uint64_t deadline)
{
    int result = within(deadline);
    if (!result && nx_window_poll(host->window, 50) != NX_OK) result = -EIO;
    if (!result) result = read_events(host);
    if (!result) {
        nx_window_info info;
        if (nx_window_get_info(host->window, &info) != NX_OK) result = -EIO;
        else if (info.should_close) result = -ECANCELED;
    }
    return result;
}

static int wake_open(struct host *host)
{
    int channel, child;
    struct pollfd seat;
    uint64_t until;
    int result = ae_login_display_watch(host->profile->display, &channel, &child);
    if (!result) result = ae_session_poll(host->profile->session, &seat, &until);
    if (result) return result;
    host->wake = epoll_create1(EPOLL_CLOEXEC);
    if (host->wake < 0) return -errno;
    const int fds[] = {host->profile->stop_fd, child, seat.fd};
    struct epoll_event readable = { .events = EPOLLIN };
    for (size_t i = 0; i < sizeof fds / sizeof fds[0]; ++i)
        if (epoll_ctl(host->wake, EPOLL_CTL_ADD, fds[i], &readable)) return -errno;
    return 0;
}

int ae_login_ui_run(const struct ae_login_ui_profile *profile, sd_bus *bus, int *backend_fd,
                    enum ae_login_ui_end *out)
{
    if (!profile || !profile->display || !profile->session || !profile->fonts ||
        !profile->backend || !profile->backend->check || profile->stop_fd < 0 ||
        !bus || !backend_fd || *backend_fd < 0 || !out || gettid() != getpid()) return -EINVAL;
    struct host host = { .profile = profile, .wake = -1, .dirty = true };
    struct ae_login_client *client = NULL;
    struct ae_login_connection *connection = NULL;
    aetheri_login_poll *poller = NULL;
    struct ae_login_client_expect expected = *profile->backend;
    expected.check = admission;
    expected.userdata = &host;
    uint64_t startup_deadline, request_deadline = 0;
    bool submitted = false;
    enum ae_login_ui_end end = AE_LOGIN_UI_STOPPED;
    int stream = -1;
    int result = set_deadline(&startup_deadline, STARTUP_MS);
    if (!result) result = receive_display(profile, startup_deadline, &stream);
    if (!result) result = open_window(&host, &stream);
    while (!result && (!submitted || !host.focused)) {
        result = wait_window(&host, startup_deadline);
        if (!result) result = render(&host, &submitted, &request_deadline);
    }
    if (!result) result = ae_login_client_create(backend_fd, bus, &expected, &client);
    while (!result && !connection) {
        result = ae_login_client_admit(client, &connection);
        if (result == -EAGAIN) result = wait_window(&host, startup_deadline);
    }
    if (!result) result = within(startup_deadline);
    if (!result) {
        aetheri_login_view_destroy(host.view);
        host.view = NULL;
        if (aetheri_login_view_create_connection(connection, profile->fonts, host.window, &host.view) != NX_OK)
            result = -EIO;
    }
    host.dirty = true;
    submitted = false;
    while (!result && !submitted) {
        result = wait_window(&host, startup_deadline);
        if (!result && !host.focused) result = -ESTALE;
        if (!result) result = render(&host, &submitted, &request_deadline);
    }
    if (!result) result = wake_open(&host);
    if (!result && aetheri_login_poll_open(connection, host.wake, &poller) != NX_OK) result = -EIO;
    if (!result) host.enabled = true;

    while (!result) {
        aetheri_login_poll_outcome outcome;
        if (aetheri_login_poll_step_guarded(poller, host.window, host.dirty ? 24u : 1000u,
                request_deadline, read_events, &host, &outcome) != NX_OK) {
            result = -EIO;
            break;
        }
        struct ae_login_status status;
        result = ae_login_connection_status(connection, &status);
        if (result) break;
        if (status.phase == AE_LOGIN_START_ACKNOWLEDGED && !outcome.connection_error) {
            end = AE_LOGIN_UI_START_ACK;
            break;
        }
        if (outcome.connection_error) {
            result = outcome.connection_error;
            break;
        }
        if (status.phase != AE_LOGIN_SENDING && status.phase != AE_LOGIN_WAITING) request_deadline = 0;
        if (outcome.changed) host.dirty = true;
        result = render(&host, &submitted, &request_deadline);
    }
    if (connection && end != AE_LOGIN_UI_START_ACK) (void)ae_login_connection_interrupt(connection);
    aetheri_login_poll_close(&poller);
    aetheri_login_view_destroy(host.view);
    if (host.renderer && nx_software_view_close(&host.renderer) != NX_OK && !result) result = -EIO;
    if (host.window) nx_window_destroy(host.window);
    if (host.wake >= 0) close(host.wake);
    if (stream >= 0) close(stream);
    int closed = ae_login_client_close(&client);
    if (!result) result = closed;
    if (result == -ECANCELED && stopped(profile->stop_fd) == -ECANCELED) result = 0;
    if (!result) *out = end;
    return result;
}
