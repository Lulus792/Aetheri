/* Test-only inherited socket capability. Never linked into aetheri-compositor. */
#include "compositor_input_driver.h"
#include "compositor_text_input.h"
#include "compositor/server.h"
#include "compositor/input.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/types/wlr_output.h>
struct input_driver {
    struct server *server;
    struct wl_event_source *source;
    int fd;
    struct wlr_keyboard keyboard;
    struct wlr_pointer pointer;
    bool live;
    uint32_t time;
    struct ae_test_text_input *ime;
};
static const struct wlr_keyboard_impl keyboard_impl = {.name = "aetheri-test-keyboard"};
static const struct wlr_pointer_impl pointer_impl = {.name = "aetheri-test-pointer"};
static bool set_layout(struct input_driver *d,bool german) {
    struct xkb_context *context=xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
    struct xkb_rule_names names={.layout=german ? "de" : "us"};
    struct xkb_keymap *map=context ? xkb_keymap_new_from_names(context,&names,XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
    bool ok=map && wlr_keyboard_set_keymap(&d->keyboard,map);xkb_keymap_unref(map);xkb_context_unref(context);return ok;
}
static bool add(struct input_driver *d, bool german) {
    if (d->live) return false;
    wlr_keyboard_init(&d->keyboard, &keyboard_impl, "private fixture keyboard");
    wlr_pointer_init(&d->pointer, &pointer_impl, "private fixture pointer");
    d->live = true;
    if (german) {
        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
        struct xkb_rule_names names = {.layout = "de"};
        struct xkb_keymap *map = context ? xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
        bool ok = map && wlr_keyboard_set_keymap(&d->keyboard, map);
        xkb_keymap_unref(map); xkb_context_unref(context);
        if (!ok) return false;
    }
    return ae_input_add(d->server->input, &d->keyboard.base) && ae_input_add(d->server->input, &d->pointer.base);
}
static void remove_devices(struct input_driver *d) {
    if (!d->live) return;
    wlr_pointer_finish(&d->pointer);
    wlr_keyboard_finish(&d->keyboard);
    d->live = false;
}
static int receive_input(int fd, uint32_t mask, void *data) {
    struct input_driver *d = data;
    (void)mask;
    char text[96] = {0};
    ssize_t length = recv(fd, text, sizeof(text) - 1, MSG_DONTWAIT | MSG_TRUNC);
    if (length < 0 && (errno == EINTR || errno == EAGAIN)) return 0;
    if (length <= 0 || (size_t)length >= sizeof(text) - 1) goto invalid;
    unsigned code, state;
    unsigned output_index, width, height;
    double x, y;
    int used = 0;
    /* Native input timestamps are milliseconds on the monotonic clock. A
       command counter fabricates old events after real test waits and cannot
       exercise freshness checks at a presented confirmation boundary. */
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC,&now)) goto invalid;
    d->time=(uint32_t)((uint64_t)now.tv_sec*UINT64_C(1000)+(uint64_t)now.tv_nsec/UINT64_C(1000000));
    if (d->live && (!strcmp(text,"L de") || !strcmp(text,"L us"))) { if (!set_layout(d,text[2]=='d')) goto invalid; }
    else if (!strcmp(text,"I on")) { if (d->server->text_input || d->ime || !(d->ime=ae_test_text_input_create(d->server))) goto invalid; }
    else if (!strcmp(text,"I 7") && d->live) {
        /* One native flush queues Escape before a commit. The app must not
           adopt this already-queued old text after cancelling its draft. */
        struct wlr_keyboard_key_event event={.time_msec=d->time,.keycode=1,.state=WL_KEYBOARD_KEY_STATE_PRESSED,.update_state=true};
        wlr_keyboard_notify_key(&d->keyboard,&event);
        if (!ae_test_text_input_command(d->ime,1)) goto invalid;
        event.state=WL_KEYBOARD_KEY_STATE_RELEASED;wlr_keyboard_notify_key(&d->keyboard,&event);puts("IME boundary=7");
    }
    else if (sscanf(text,"I %u%n",&code,&used)==1 && used==length) { if (!ae_test_text_input_command(d->ime,code)) goto invalid; }
    else if (!strcmp(text, "D")) remove_devices(d);
    else if (!strcmp(text, "A") || !strcmp(text, "G")) { if (!add(d, text[0] == 'G')) goto invalid; }
    else if (sscanf(text, "O %u %u %u%n", &output_index, &width, &height, &used) == 3 && used == length &&
            output_index < d->server->output_count && width >= 320 && width <= 3840 && height >= 240 && height <= 2160) {
        struct wlr_output_state requested; wlr_output_state_init(&requested);
        wlr_output_state_set_custom_mode(&requested, (int32_t)width, (int32_t)height, 60000);
        struct wlr_output_event_request_state event = {.output = d->server->outputs[output_index].output, .state = &requested};
        if (!event.output) { wlr_output_state_finish(&requested); goto invalid; }
        wl_signal_emit_mutable(&event.output->events.request_state, &event);
        wlr_output_state_finish(&requested);
    }
    else if (d->live && sscanf(text, "M %lf %lf%n", &x, &y, &used) == 2 && used == length && isfinite(x) && isfinite(y) && x >= 0 && x < 8320 && y >= 0 && y < 2160) {
        struct wlr_pointer_motion_absolute_event event = {.pointer = &d->pointer, .time_msec = d->time, .x = x / 8320.0, .y = y / 2160.0};
        wl_signal_emit_mutable(&d->pointer.events.motion_absolute, &event);
    } else if (d->live && sscanf(text, "B %u %u%n", &code, &state, &used) == 2 && used == length && code >= 0x110 && code < 0x120 && state <= 1) {
        struct wlr_pointer_button_event event = {.pointer = &d->pointer, .time_msec = d->time, .button = code, .state = state};
        wlr_pointer_notify_button(&d->pointer, &event);
    } else if (d->live && sscanf(text, "K %u %u%n", &code, &state, &used) == 2 && used == length && code < 256 && state <= 1) {
        struct wlr_keyboard_key_event event = {.time_msec = d->time, .keycode = code, .state = state, .update_state = true};
        wlr_keyboard_notify_key(&d->keyboard, &event);
    } else if (d->live && sscanf(text, "S %lf%n", &y, &used) == 1 && used == length && isfinite(y) && fabs(y) <= 120) {
        struct wlr_pointer_axis_event event = {.pointer = &d->pointer, .time_msec = d->time,
            .source = WL_POINTER_AXIS_SOURCE_WHEEL, .orientation = WL_POINTER_AXIS_VERTICAL_SCROLL,
            .relative_direction = WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL, .delta = y, .delta_discrete = y > 0 ? 120 : -120};
        wl_signal_emit_mutable(&d->pointer.events.axis, &event);
    } else goto invalid;
    if (d->live) wl_signal_emit_mutable(&d->pointer.events.frame, &d->pointer);
    wl_display_flush_clients(d->server->display);
    if (send(fd, "OK", 2, MSG_NOSIGNAL) != 2) goto invalid;
    return 0;
invalid:
    d->server->failed = true;
    wl_display_terminate(d->server->display);
    return 0;
}
struct input_driver *input_driver_start(struct server *s, int fd) {
    int type = 0; socklen_t size = sizeof(type);
    struct sockaddr_storage peer; socklen_t peer_size = sizeof(peer);
    if (fd < 3 || getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_SEQPACKET ||
            getpeername(fd, (struct sockaddr *)&peer, &peer_size) || peer.ss_family != AF_UNIX ||
            fcntl(fd, F_SETFD, FD_CLOEXEC) || fcntl(fd, F_SETFL, O_NONBLOCK)) return NULL;
    struct input_driver *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->server = s; d->fd = fd;
    if (!add(d, false)) { remove_devices(d); free(d); return NULL; }
    d->source = wl_event_loop_add_fd(wl_display_get_event_loop(s->display), fd, WL_EVENT_READABLE, receive_input, d);
    if (!d->source) { remove_devices(d); free(d); return NULL; }
    return d;
}
void input_driver_close(struct input_driver *d) {
    if (!d) return;
    wl_event_source_remove(d->source);ae_test_text_input_destroy(d->ime);remove_devices(d);close(d->fd);free(d);
}
