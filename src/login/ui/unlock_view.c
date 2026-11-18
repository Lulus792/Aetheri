#define _GNU_SOURCE
#include "login/unlock_pam.h"
#include "login/unlock_view_contract.h"
#include "linux/unnamed_socket.h"
#include "linux/wayland_shm.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

enum { WIDTH = AE_UNLOCK_VIEW_WIDTH, HEIGHT = AE_UNLOCK_VIEW_HEIGHT };
struct view {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_surface *surface;
    struct xkb_context *xkb;
    struct xkb_state *keys;
    struct ae_wayland_buffer buffers[2];
    TTF_Font *font;
    const char *account;
    int broker, feedback;
    unsigned char password[AE_UNLOCK_PASSWORD_MAX];
    size_t length, entered;
    bool german, focused, action, pending, dirty, failed;
};

static void reset(struct view *v) {
    explicit_bzero(v->password, sizeof v->password);
    v->length = v->entered = 0;
}

static void repaint(void *data) { ((struct view *)data)->dirty = true; }

/* The event loop retries a pending redraw after dispatching the release. */
static void buffer_released(void *data) { (void)data; }

static Uint32 color(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b) {
    return SDL_MapSurfaceRGB(surface, r, g, b);
}
static bool fill(SDL_Surface *surface, SDL_Rect box, Uint32 pixel) {
    return SDL_FillSurfaceRect(surface, &box, pixel);
}
static bool text(struct view *v, SDL_Surface *surface, const char *value, int size,
        int x, int y, SDL_Color ink) {
    if (!TTF_SetFontSize(v->font, (float)size)) return false;
    SDL_Surface *glyphs = TTF_RenderText_Blended(v->font, value, 0, ink);
    if (!glyphs) return false;
    SDL_Rect target = {.x = x < 0 ? (WIDTH - glyphs->w) / 2 : x, .y = y};
    bool drawn = SDL_BlitSurface(glyphs, NULL, surface, &target);
    SDL_DestroySurface(glyphs);
    return drawn;
}

static bool paint(struct view *v, struct ae_wayland_buffer *buffer, const struct tm *now) {
    SDL_Surface *surface = SDL_CreateSurfaceFrom(WIDTH, HEIGHT, SDL_PIXELFORMAT_XRGB8888,
        buffer->pixels, buffer->pitch);
    if (!surface) return false;
    const SDL_Color main_text = {236, 240, 255, 255}, subtext = {192, 202, 227, 255};
    const SDL_Color action_text = {24, 30, 52, 255}, error_text = {255, 190, 197, 255};
    Uint32 edge = color(surface, 65, 76, 113), panel = color(surface, 24, 28, 43);
    Uint32 accent = color(surface, 179, 190, 255), field = color(surface, 15, 19, 33);
    Uint32 muted = color(surface, 85, 96, 132);
    bool field_focus = v->focused && !v->action && !v->pending;
    bool action_focus = v->focused && v->action && !v->pending;
    char time[12];
    if (!strftime(time, sizeof time, "%H:%M", now)) { SDL_DestroySurface(surface); return false; }
    const char *label = v->german ? "Passwort" : "Password";
    const char *action = v->german ? "Entsperren" : "Unlock";
    const char *feedback = v->pending ? (v->german ? "Wird geprüft …" : "Checking …") :
        v->feedback == AE_UNLOCK_VIEW_DENIED ? (v->german ? "Passwort abgelehnt" : "Password not accepted") :
        v->feedback == AE_UNLOCK_VIEW_ERROR ? (v->german ? "Prüfung nicht verfügbar" : "Verification unavailable") : NULL;
    bool ok = SDL_FillSurfaceRect(surface, NULL, edge) &&
        fill(surface, (SDL_Rect){1, 1, WIDTH - 2, HEIGHT - 2}, panel) &&
        text(v, surface, time, 54, -1, 19, main_text) &&
        text(v, surface, v->account, 21, -1, 94, main_text) &&
        text(v, surface, label, 16, 32, 143, subtext) &&
        fill(surface, (SDL_Rect){32, 169, WIDTH - 64, 49}, field_focus ? accent : edge) &&
        fill(surface, (SDL_Rect){34, 171, WIDTH - 68, 45}, field) &&
        fill(surface, (SDL_Rect){32, 234, WIDTH - 64, 47}, action_focus ? accent : panel) &&
        fill(surface, (SDL_Rect){35, 237, WIDTH - 70, 41}, v->length && !v->pending ? accent : muted) &&
        text(v, surface, action, 17, -1, 247, v->length && !v->pending ? action_text : main_text);
    /* Only the brief masked projection reaches SDL and shared display memory. */
    for (size_t i = 0; i < v->entered && i < 23 && ok; ++i)
        ok = fill(surface, (SDL_Rect){48 + (int)i * 13, 189, 8, 8}, accent);
    if (ok && feedback)
        ok = text(v, surface, feedback, 16, -1, 305, v->pending ? subtext : error_text);
    SDL_DestroySurface(surface);
    return ok;
}

static void submit(struct view *v) {
    if (!v->length) return;
    struct iovec body[] = {{(void *)AE_UNLOCK_VIEW_MAGIC, AE_UNLOCK_VIEW_MAGIC_SIZE},
        {v->password, v->length}};
    struct msghdr message = {.msg_iov = body, .msg_iovlen = 2};
    ssize_t bytes = sendmsg(v->broker, &message, MSG_DONTWAIT | MSG_NOSIGNAL);
    v->pending = bytes == (ssize_t)(AE_UNLOCK_VIEW_MAGIC_SIZE + v->length);
    if (!v->pending) v->failed = true;
    reset(v);
}

static void keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size) {
    (void)keyboard;
    struct view *v = data;
    struct xkb_keymap *map = NULL;
    if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size > 1 && size <= 1024u * 1024u) {
        const char *bytes = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (bytes != MAP_FAILED) {
            if (!bytes[size - 1])
                map = xkb_keymap_new_from_buffer(v->xkb, bytes, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1,
                    XKB_KEYMAP_COMPILE_NO_FLAGS);
            munmap((void *)bytes, size);
        }
    }
    close(fd);
    xkb_state_unref(v->keys);
    v->keys = map ? xkb_state_new(map) : NULL;
    xkb_keymap_unref(map);
    if (!v->keys) v->failed = true;
    reset(v);
    repaint(v);
}
static void enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface,
        struct wl_array *held) {
    (void)keyboard;
    (void)serial;
    struct view *v = data;
    if (surface != v->surface || held->size) { v->failed = true; return; }
    v->focused = true;
    v->action = false;
    repaint(v);
}
static void leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {
    (void)keyboard;
    (void)serial;
    (void)surface;
    struct view *v = data;
    v->focused = v->pending = false;
    v->feedback = 0;
    reset(v);
    repaint(v);
}
static void key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t when,
        uint32_t code, uint32_t action) {
    (void)keyboard;
    (void)serial;
    (void)when;
    struct view *v = data;
    if (!v->focused || v->pending || !v->keys || action != WL_KEYBOARD_KEY_STATE_PRESSED || code > KEY_MAX) return;
    xkb_keysym_t symbol = xkb_state_key_get_one_sym(v->keys, code + 8);
    v->feedback = 0;
    if (symbol == XKB_KEY_Escape) { reset(v); v->action = false; }
    else if (symbol == XKB_KEY_Tab || symbol == XKB_KEY_ISO_Left_Tab) v->action = !v->action;
    else if (symbol == XKB_KEY_Return || symbol == XKB_KEY_KP_Enter) submit(v);
    else if (symbol == XKB_KEY_BackSpace && !v->action && v->length) {
        size_t end = v->length - 1;
        while (end && (v->password[end] & 0xc0u) == 0x80u) --end;
        explicit_bzero(v->password + end, v->length - end);
        v->length = end;
        --v->entered;
    } else if (!v->action && !xkb_state_mod_name_is_active(v->keys, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE)) {
        uint32_t scalar = xkb_keysym_to_utf32(symbol);
        if (scalar >= 0x20 && scalar != 0x7f) {
            char bytes[8] = {0};
            int size = xkb_state_key_get_utf8(v->keys, code + 8, bytes, sizeof bytes);
            if (size > 0 && (size_t)size < sizeof bytes && (size_t)size <= sizeof v->password - v->length) {
                memcpy(v->password + v->length, bytes, (size_t)size);
                v->length += (size_t)size;
                ++v->entered;
            }
            explicit_bzero(bytes, sizeof bytes);
        }
    }
    repaint(v);
}
static void modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t depressed,
        uint32_t latched, uint32_t locked, uint32_t group) {
    (void)keyboard;
    (void)serial;
    struct view *v = data;
    if (v->keys) xkb_state_update_mask(v->keys, depressed, latched, locked, 0, 0, group);
}
static void repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
    (void)data;
    (void)keyboard;
    (void)rate;
    (void)delay;
}
static const struct wl_keyboard_listener keyboard_events = {
    .keymap = keymap, .enter = enter, .leave = leave, .key = key, .modifiers = modifiers,
    .repeat_info = repeat_info,
};
static void capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
    struct view *v = data;
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !v->keyboard) {
        v->keyboard = wl_seat_get_keyboard(seat);
        if (!v->keyboard || wl_keyboard_add_listener(v->keyboard, &keyboard_events, v)) v->failed = true;
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && v->keyboard) {
        wl_keyboard_release(v->keyboard);
        v->keyboard = NULL;
        leave(v, NULL, 0, NULL);
    }
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data;
    (void)seat;
    (void)name;
}
static const struct wl_seat_listener seat_events = {.capabilities = capabilities, .name = seat_name};
static void shm_format(void *data, struct wl_shm *shm, uint32_t format) {
    (void)data;
    (void)shm;
    (void)format;
}
static const struct wl_shm_listener shm_events = {.format = shm_format};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    struct view *v = data;
    if (!v->compositor && !strcmp(interface, wl_compositor_interface.name))
        v->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    else if (!v->shm && !strcmp(interface, wl_shm_interface.name)) {
        v->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
        if (!v->shm || wl_shm_add_listener(v->shm, &shm_events, v)) v->failed = true;
    } else if (!v->seat && version >= 4 && !strcmp(interface, wl_seat_interface.name)) {
        v->seat = wl_registry_bind(registry, name, &wl_seat_interface, 4);
        if (!v->seat || wl_seat_add_listener(v->seat, &seat_events, v)) v->failed = true;
    }
}
static void remove_global(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}
static const struct wl_registry_listener registry_events = {.global = global, .global_remove = remove_global};

static bool feedback(struct view *v) {
    char bytes[AE_UNLOCK_VIEW_MAGIC_SIZE + 1];
    struct iovec body = {bytes, sizeof bytes};
    struct msghdr message = {.msg_iov = &body, .msg_iovlen = 1};
    ssize_t size = recvmsg(v->broker, &message, MSG_DONTWAIT);
    if (size == -1 && (errno == EINTR || errno == EAGAIN)) return true;
    if (size != (ssize_t)sizeof bytes || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) ||
            memcmp(bytes, AE_UNLOCK_VIEW_MAGIC, AE_UNLOCK_VIEW_MAGIC_SIZE)) return false;
    int kind = bytes[AE_UNLOCK_VIEW_MAGIC_SIZE];
    if (kind != AE_UNLOCK_VIEW_DENIED && kind != AE_UNLOCK_VIEW_ERROR) return false;
    if (v->pending) { v->pending = false; v->feedback = kind; v->action = false; repaint(v); }
    return true;
}

static bool run(struct view *v) {
    struct pollfd descriptors[] = {{wl_display_get_fd(v->display), POLLIN, 0}, {v->broker, POLLIN, 0}};
    int minute = -1;
    while (!v->failed) {
        if (wl_display_dispatch_pending(v->display) < 0) return false;
        time_t wall = time(NULL);
        struct tm now;
        if (!localtime_r(&wall, &now)) return false;
        if (now.tm_min != minute) { minute = now.tm_min; repaint(v); }
        if (v->dirty) for (size_t i = 0; i < 2; ++i) if (!v->buffers[i].busy) {
            if (!paint(v, &v->buffers[i], &now)) return false;
            ae_wayland_buffer_present(&v->buffers[i], v->surface, WIDTH, HEIGHT);
            v->dirty = false;
            break;
        }
        int flushed = wl_display_flush(v->display);
        if (flushed < 0 && errno != EAGAIN) return false;
        descriptors[0].events = flushed < 0 ? POLLIN | POLLOUT : POLLIN;
        int result = poll(descriptors, 2, (60 - now.tm_sec) * 1000);
        if (result < 0) { if (errno == EINTR) continue; return false; }
        if ((descriptors[0].revents | descriptors[1].revents) & POLLNVAL) return false;
        if (descriptors[0].revents & (POLLERR | POLLHUP)) return true;
        if (descriptors[0].revents & POLLIN && wl_display_dispatch(v->display) < 0) return false;
        if (descriptors[1].revents & (POLLERR | POLLHUP)) return true;
        if (descriptors[1].revents & POLLIN && !feedback(v)) return false;
    }
    return false;
}

static int descriptor(const char *value, int type) {
    if (!*value || strspn(value, "0123456789") != strlen(value)) return -1;
    errno = 0;
    long fd = strtol(value, NULL, 10);
    if (errno || fd < 3 || fd > INT_MAX || ae_unnamed_socket_check((int)fd, type)) return -1;
    struct ucred peer;
    socklen_t size = sizeof peer;
    if (getsockopt((int)fd, SOL_SOCKET, SO_PEERCRED, &peer, &size) || size != sizeof peer ||
            peer.pid <= 1 || peer.pid != getppid() || peer.uid != getuid() || peer.gid != getgid()) return -1;
    return (int)fd;
}

int main(int argc, char **argv) {
    if (argc != 6 || !getuid() || getuid() != geteuid() || getgid() != getegid() ||
            strlen(argv[4]) > 32 || !*argv[4] || (strcmp(argv[5], "de") && strcmp(argv[5], "en")))
        return 2;
    int display = descriptor(argv[1], SOCK_STREAM), broker = descriptor(argv[2], SOCK_SEQPACKET);
    if (display < 0 || broker < 0 || fcntl(broker, F_SETFD, FD_CLOEXEC) ||
            prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L) || !SDL_Init(0)) return 2;
    if (!TTF_Init()) { SDL_Quit(); return 2; }
    struct view v = {.account = argv[4], .german = !strcmp(argv[5], "de"), .broker = broker, .dirty = true};
    v.font = TTF_OpenFont(argv[3], 16);
    v.xkb = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
    v.display = wl_display_connect_to_fd(display);
    int status = 1;
    if (v.font && v.xkb && v.display) {
        v.registry = wl_display_get_registry(v.display);
        if (v.registry && !wl_registry_add_listener(v.registry, &registry_events, &v) &&
                wl_display_roundtrip(v.display) >= 0 && v.compositor && v.shm && v.seat) {
            v.surface = wl_compositor_create_surface(v.compositor);
            if (v.surface && !ae_wayland_buffer_open(&v.buffers[0], v.shm, WL_SHM_FORMAT_XRGB8888,
                        WIDTH, HEIGHT, buffer_released, NULL) &&
                    !ae_wayland_buffer_open(&v.buffers[1], v.shm, WL_SHM_FORMAT_XRGB8888,
                        WIDTH, HEIGHT, buffer_released, NULL))
                status = run(&v) ? 0 : 1;
        }
    }
    reset(&v);
    for (size_t i = 0; i < 2; ++i) ae_wayland_buffer_close(&v.buffers[i]);
    if (v.surface) wl_surface_destroy(v.surface);
    if (v.keyboard) wl_keyboard_release(v.keyboard);
    if (v.seat) wl_seat_destroy(v.seat);
    if (v.shm) wl_shm_destroy(v.shm);
    if (v.compositor) wl_compositor_destroy(v.compositor);
    if (v.registry) wl_registry_destroy(v.registry);
    if (v.display) wl_display_disconnect(v.display);
    close(broker);
    xkb_state_unref(v.keys);
    xkb_context_unref(v.xkb);
    if (v.font) TTF_CloseFont(v.font);
    TTF_Quit();
    SDL_Quit();
    return status;
}
