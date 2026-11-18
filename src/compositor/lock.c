#include "lock.h"
#include "input.h"
#include "lock_wire.h"
#include "lock_view.h"
#include "server.h"
#include "linux/pidfd_channel.h"
#include "linux/unnamed_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

static const char request[] = "AELK1";
static const char covered[] = "AELK1 COVERED";

struct ae_lock {
    struct server *server;
    struct wl_event_source *source, *owner_source, *release_idle;
    int fd, owner;
    struct stat owner_identity;
    uint64_t generation;
    bool active, managed, presented, release_requested;
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
    if (lock->release_idle) {
        wl_event_source_remove(lock->release_idle);
        lock->release_idle = NULL;
    }
    if (lock->owner_source) {
        wl_event_source_remove(lock->owner_source);
        lock->owner_source = NULL;
    }
    if (lock->owner >= 0) {
        close(lock->owner);
        lock->owner = -1;
    }
    lock->release_requested = false;
    ae_lock_view_hide(lock->server->lock_view);
}

void ae_lock_view_lost(struct ae_lock *lock) {
    if (lock) retire_channel(lock);
}

static void schedule_cover(struct output *o) {
    wlr_damage_ring_add_whole(&o->scene->damage_ring);
    wlr_output_schedule_frame(o->output);
}

static bool cover_presented(const struct server *s) {
    if (s->failed || s->shutting_down || s->output_count == 0) return false;
    for (unsigned i = 0; i < s->output_count; ++i) {
        const struct output *o = &s->outputs[i];
        if (!o->output || !o->output->enabled || !o->cover_pending ||
                !o->cover_committed || !o->cover_presented) return false;
    }
    return true;
}

static bool owner_live(const struct ae_lock *lock) {
    if (lock->owner < 0) return false;
    struct pollfd pin = {.fd = lock->owner, .events = POLLIN};
    return poll(&pin, 1, 0) == 0;
}

bool ae_lock_view_endpoint(const struct ae_lock *lock, int fd) {
    if (!lock || !lock->managed || !owner_live(lock) || ae_unnamed_socket_check(fd, SOCK_STREAM)) return false;
    int peer = -1;
    socklen_t size = sizeof peer;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERPIDFD, &peer, &size)) return false;
    struct stat identity;
    bool matches = peer >= 0 && size == sizeof peer && !fstat(peer, &identity) &&
        identity.st_dev == lock->owner_identity.st_dev && identity.st_ino == lock->owner_identity.st_ino;
    if (peer >= 0) close(peer);
    return matches;
}

static bool respond(struct ae_lock *lock, enum ae_lock_action action, uint64_t generation) {
    unsigned char response[AE_LOCK_WIRE_SIZE];
    ae_lock_wire_write(response, action, generation);
    if (owner_live(lock) &&
            send(lock->fd, response, sizeof response, MSG_DONTWAIT | MSG_NOSIGNAL) == (ssize_t)sizeof response)
        return true;
    retire_channel(lock);
    return false;
}

static void release_if_ready(void *data) {
    struct ae_lock *lock = data;
    lock->release_idle = NULL;
    struct server *s = lock->server;
    if (!lock->release_requested || !cover_presented(s) || !ae_lock_view_ready(s->lock_view) ||
            !ae_input_release_ready(s->input)) return;
    if (!respond(lock, AE_LOCK_RELEASED, lock->generation)) return;
    lock->active = lock->presented = lock->release_requested = false;
    ae_lock_view_hide(s->lock_view);
    for (unsigned i = 0; i < s->output_count; ++i) s->outputs[i].cover_pending = false;
    wlr_scene_node_set_enabled(&s->normal_tree->node, true);
    ae_input_uncover(s->input);
}

void ae_lock_input_changed(struct ae_lock *lock) {
    if (!lock) return;
    ae_lock_view_focus(lock->server->lock_view);
    if (!lock->release_requested || lock->release_idle) return;
    /* The keyboard backend updates modifiers after the key callback. Sample
       the seat after that dispatch, before giving any client normal focus. */
    lock->release_idle = wl_event_loop_add_idle(wl_display_get_event_loop(lock->server->display),
        release_if_ready, lock);
    if (!lock->release_idle) retire_channel(lock);
}

static void acknowledge(struct server *s) {
    struct ae_lock *lock = s->lock;
    if (lock->fd < 0 || !cover_presented(s)) return;
    if (lock->managed) {
        if (!lock->presented && respond(lock, AE_LOCK_COVERED, lock->generation)) {
            lock->presented = true;
            ae_lock_view_show(s->lock_view);
        }
        ae_lock_input_changed(lock);
        return;
    }
    /* A send failure leaves the scene covered and cannot produce an ack. */
    (void)send(lock->fd, covered, sizeof covered - 1, MSG_DONTWAIT | MSG_NOSIGNAL);
    retire_channel(lock);
}

static void present(struct wl_listener *listener, void *data) {
    struct output *o = wl_container_of(listener, o, present);
    const struct wlr_output_event_present *event = data;
    if (o->index == 0 && event->output == o->output)
        ae_lock_view_present(o->server->lock_view, event->commit_seq, event->presented);
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
    uint32_t sequence = o->output->commit_seq + 1;
    struct ae_lock_view *view = o->index == 0 ? o->server->lock_view : NULL;
    if ((state->committed & WLR_OUTPUT_STATE_BUFFER) && state->buffer)
        ae_lock_view_before_commit(view, sequence);
    bool track = ae_lock_active(o->server->lock) && !o->cover_pending &&
        (state->committed & WLR_OUTPUT_STATE_BUFFER) && state->buffer;
    if (track) {
        o->cover_seq = sequence;
        o->cover_pending = true;
        o->cover_committed = false;
        o->cover_presented = false;
    }
    bool ok = wlr_output_commit_state(o->output, state);
    ae_lock_view_commit(view, sequence, ok);
    if (track && ok) {
        o->cover_committed = true;
        acknowledge(o->server);
    }
    return ok;
}

static void begin_cover(struct ae_lock *lock) {
    struct server *s = lock->server;
    ++lock->generation;
    lock->active = true;
    ae_input_cover(s->input);
    wlr_scene_node_set_enabled(&s->normal_tree->node, false);
    for (unsigned i = 0; i < s->output_count; ++i) {
        struct output *o = &s->outputs[i];
        o->cover_pending = false;
        if (o->output && o->scene) schedule_cover(o);
    }
}

static int receive_cover(int fd, uint32_t mask, void *data) {
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

    begin_cover(lock);
    return 0;
}

static int receive_managed(int fd, uint32_t mask, void *data) {
    struct ae_lock *lock = data;
    unsigned char packet[AE_LOCK_WIRE_SIZE];
    ssize_t size = ae_pidfd_receive(fd, packet, sizeof packet, &lock->owner_identity);
    if (size == -EAGAIN && !(mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR))) return 0;
    if (size != (ssize_t)sizeof packet || memcmp(packet, "AELK2", 5) ||
            (packet[5] != AE_LOCK_COVER && packet[5] != AE_LOCK_RELEASE) || !owner_live(lock)) {
        retire_channel(lock);
        return 0;
    }
    uint64_t generation = ae_lock_wire_generation(packet);
    if (packet[5] == AE_LOCK_COVER && generation == lock->generation && !lock->active &&
            generation < UINT64_MAX && !lock->server->failed && !lock->server->shutting_down) {
        begin_cover(lock);
    } else if (packet[5] == AE_LOCK_RELEASE && generation == lock->generation &&
            lock->presented && !lock->release_requested && cover_presented(lock->server) &&
            ae_lock_view_ready(lock->server->lock_view)) {
        lock->release_requested = true;
        ae_lock_input_changed(lock);
    } else {
        (void)respond(lock, AE_LOCK_REJECTED, generation);
    }
    return 0;
}

static int owner_ended(int fd, uint32_t mask, void *data) {
    (void)fd;
    (void)mask;
    retire_channel(data);
    return 0;
}

static bool admit_owner(struct ae_lock *lock, int fd, struct wl_event_loop *loop) {
    struct ucred peer;
    socklen_t size = sizeof peer;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &size) || size != sizeof peer ||
            peer.pid <= 1 || peer.pid != getppid() || peer.uid != getuid() || peer.gid != getgid()) return false;
    size = sizeof lock->owner;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERPIDFD, &lock->owner, &size) || size != sizeof lock->owner ||
            !owner_live(lock) || fstat(lock->owner, &lock->owner_identity)) return false;
    const int enabled = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_PASSPIDFD, &enabled, sizeof enabled)) return false;
    lock->owner_source = wl_event_loop_add_fd(loop, lock->owner, WL_EVENT_READABLE, owner_ended, lock);
    return lock->owner_source != NULL;
}

struct ae_lock *ae_lock_create(struct server *s, int fd, bool managed) {
    if (ae_unnamed_socket_check(fd, SOCK_SEQPACKET) || fcntl(fd, F_SETFD, FD_CLOEXEC)) return NULL;
    struct ae_lock *lock = calloc(1, sizeof *lock);
    if (!lock) return NULL;
    lock->server = s;
    lock->fd = lock->owner = -1;
    lock->managed = managed;
    struct wl_event_loop *loop = wl_display_get_event_loop(s->display);
    if (!managed || admit_owner(lock, fd, loop))
        lock->source = wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
            managed ? receive_managed : receive_cover, lock);
    if (!lock->source) {
        ae_lock_destroy(lock);
        return NULL;
    }
    lock->fd = fd;
    return lock;
}

void ae_lock_destroy(struct ae_lock *lock) {
    if (!lock) return;
    retire_channel(lock);
    free(lock);
}
