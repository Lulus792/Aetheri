#include "lock.h"
#include "input.h"
#include "server.h"
#include "linux/unnamed_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

static const char request[] = "AELK1";
static const char covered[] = "AELK1 COVERED";

struct ae_lock {
    struct server *server;
    struct wl_event_source *source;
    int fd;
    bool active;
};

bool ae_lock_active(const struct ae_lock *lock) {
    return lock && lock->active;
}

static void retire_channel(struct ae_lock *lock) {
    if (lock->source) {
        wl_event_source_remove(lock->source);
        lock->source = NULL;
    }
    if (lock->fd >= 0) {
        close(lock->fd);
        lock->fd = -1;
    }
}

static void schedule_cover(struct output *o) {
    wlr_damage_ring_add_whole(&o->scene->damage_ring);
    wlr_output_schedule_frame(o->output);
}

static void acknowledge(struct server *s) {
    struct ae_lock *lock = s->lock;
    if (lock->fd < 0 || s->failed || s->shutting_down || s->output_count == 0) return;
    for (unsigned i = 0; i < s->output_count; ++i) {
        const struct output *o = &s->outputs[i];
        if (!o->output || !o->output->enabled || !o->cover_pending ||
                !o->cover_committed || !o->cover_presented) return;
    }
    /* A send failure leaves the scene covered and cannot produce an ack. */
    (void)send(lock->fd, covered, sizeof covered - 1, MSG_DONTWAIT | MSG_NOSIGNAL);
    retire_channel(lock);
}

static void present(struct wl_listener *listener, void *data) {
    struct output *o = wl_container_of(listener, o, present);
    const struct wlr_output_event_present *event = data;
    if (!o->cover_pending || event->output != o->output ||
            event->commit_seq != o->cover_seq) return;
    if (!event->presented) {
        o->cover_pending = false;
        schedule_cover(o);
        return;
    }
    o->cover_presented = true;
    acknowledge(o->server);
}

void ae_lock_add_output(struct output *o) {
    if (!o->server->lock) return;
    o->present.notify = present;
    wl_signal_add(&o->output->events.present, &o->present);
}

void ae_lock_remove_output(struct output *o) {
    if (o->present.notify) wl_list_remove(&o->present.link);
}

bool ae_lock_commit(struct output *o, const struct wlr_output_state *state) {
    bool track = ae_lock_active(o->server->lock) && !o->cover_pending &&
        (state->committed & WLR_OUTPUT_STATE_BUFFER) && state->buffer;
    if (track) {
        o->cover_seq = o->output->commit_seq + 1;
        o->cover_pending = true;
        o->cover_committed = false;
        o->cover_presented = false;
    }
    bool ok = wlr_output_commit_state(o->output, state);
    if (track && ok) {
        o->cover_committed = true;
        acknowledge(o->server);
    }
    return ok;
}

static int receive_request(int fd, uint32_t mask, void *data) {
    struct ae_lock *lock = data;
    (void)mask;
    char message[sizeof request - 1];
    /* No ancillary buffer: the kernel discards and closes any passed FDs.
       Check control truncation so a packet carrying an FD cannot request cover. */
    struct iovec body = {.iov_base = message, .iov_len = sizeof message};
    struct msghdr packet = {.msg_iov = &body, .msg_iovlen = 1};
    ssize_t size = recvmsg(fd, &packet, MSG_DONTWAIT);
    if (size < 0 && (errno == EINTR || errno == EAGAIN)) return 0;
    if (size != (ssize_t)sizeof message || (packet.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) ||
            memcmp(message, request, sizeof message) || lock->active) {
        retire_channel(lock);
        return 0;
    }

    struct server *s = lock->server;
    lock->active = true;
    ae_input_cover(s->input);
    wlr_scene_node_set_enabled(&s->normal_tree->node, false);
    for (unsigned i = 0; i < s->output_count; ++i) {
        struct output *o = &s->outputs[i];
        if (o->output && o->scene) schedule_cover(o);
    }
    return 0;
}

struct ae_lock *ae_lock_create(struct server *s, int fd) {
    if (ae_unnamed_socket_check(fd, SOCK_SEQPACKET) || fcntl(fd, F_SETFD, FD_CLOEXEC)) return NULL;
    struct ae_lock *lock = calloc(1, sizeof *lock);
    if (!lock) return NULL;
    lock->server = s;
    lock->fd = fd;
    lock->source = wl_event_loop_add_fd(wl_display_get_event_loop(s->display), fd,
        WL_EVENT_READABLE, receive_request, lock);
    if (!lock->source) {
        free(lock);
        return NULL;
    }
    return lock;
}

void ae_lock_destroy(struct ae_lock *lock) {
    if (!lock) return;
    retire_channel(lock);
    free(lock);
}
