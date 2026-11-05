/* Independent libwayland consumer: no wlroots or compositor-private linkage. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client-protocol.h"
#ifdef AETHERI_TEST_TEXT_INPUT
#include "compose-text-input-v3-client.h"
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "client check failed: %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
struct app;
struct clipboard_source { struct app *app; struct wl_data_source *object; unsigned index; };
struct view {
    struct app *app;
    struct wl_surface *surface;
    struct xdg_surface *xdg;
    struct xdg_toplevel *top;
    struct wl_callback *frame;
    unsigned index;
    int32_t width, height;
    bool fullscreen;
};
struct buffer {
    struct wl_buffer *object;
    struct wl_list link;
};
struct app {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *shell;
    struct wl_output *outputs[3];
    unsigned output_count;
    struct wl_list buffers;
    struct view views[2];
    uint32_t shell_version;
    bool input,text_mode;
#ifdef AETHERI_TEST_TEXT_INPUT
    struct zwp_text_input_manager_v3 *text_manager;
    struct zwp_text_input_v3 *text[2];
#endif
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct view *pointer_focus, *keyboard_focus;
    struct xkb_context *xkb;
    struct xkb_keymap *keymap;
    struct xkb_state *key_state;
    unsigned grab_mode;
    uint32_t last_button;
    struct wl_callback *sync;
    const char *clipboard_payload;char *owned_payload;size_t clipboard_size;bool stall_clipboard,plain_clipboard;
    int stalled[16],readers[16];unsigned stalled_count,reader_count;
    struct wl_data_device_manager *data_manager;
    struct wl_data_device *data_device;
    struct wl_data_offer *offers[64], *selection, *previous_selection;
    size_t offer_count, source_count;
    struct clipboard_source sources[16];
    uint32_t clipboard_serial, old_clipboard_serial;
};
static void source_target(void *data, struct wl_data_source *source, const char *mime) { (void)data; (void)source; (void)mime; }
static void source_send(void *data, struct wl_data_source *source, const char *mime, int32_t fd) {
    (void)source;
    struct clipboard_source *s=data;
    CHECK(!strcmp(mime,s->app->plain_clipboard ? "text/plain" : "text/plain;charset=utf-8"));
    if (s->app->stall_clipboard) {
        CHECK(s->app->stalled_count<16);s->app->stalled[s->app->stalled_count++]=fd;
        printf("CLIPBOARD_STALLED %u\n",s->index);return;
    }
    size_t length=s->app->clipboard_size;
    ssize_t n;
    do { n=write(fd,s->app->clipboard_payload,length); } while (n<0 && errno==EINTR);
    CHECK(n==(ssize_t)length); CHECK(close(fd)==0);
    printf("CLIPBOARD_SENT %u\n",s->index);
}
static void source_cancelled(void *data, struct wl_data_source *source) {
    (void)source; struct clipboard_source *s=data; printf("CLIPBOARD_CANCEL %u\n",s->index);
}
static const struct wl_data_source_listener source_listener={.target=source_target,.send=source_send,.cancelled=source_cancelled};
static void offer_mime(void *data, struct wl_data_offer *offer, const char *mime) { (void)data; (void)offer; CHECK(!strcmp(mime,"text/plain;charset=utf-8") || !strcmp(mime,"text/plain")); }
static const struct wl_data_offer_listener offer_listener={.offer=offer_mime};
static void device_offer(void *data, struct wl_data_device *device, struct wl_data_offer *offer) {
    (void)device; struct app *a=data;
    CHECK(a->offer_count<64); a->offers[a->offer_count++]=offer;
    CHECK(wl_data_offer_add_listener(offer,&offer_listener,a)==0);
}
static void device_selection(void *data, struct wl_data_device *device, struct wl_data_offer *offer) {
    (void)device; struct app *a=data;
    if (a->selection) a->previous_selection=a->selection;
    a->selection=offer;
    printf("CLIPBOARD_SELECTION %d\n",offer!=NULL);
}
static const struct wl_data_device_listener device_listener={.data_offer=device_offer,.selection=device_selection};
static void clipboard_command(struct app *a, char c) {
    if (c=='T') {
        for (unsigned i=0;i<a->stalled_count;++i) if (a->stalled[i]>=0) {
            ssize_t sent=write(a->stalled[i],"x",1);
            if (sent<0 && errno==EPIPE) { close(a->stalled[i]);a->stalled[i]=-1;puts("CLIPBOARD_STALL_BROKEN"); }
            else CHECK(sent==1 || (sent<0 && (errno==EAGAIN || errno==EWOULDBLOCK)));
        }
    } else if (c=='W') {
        CHECK(a->selection && a->reader_count<16);int fds[2];CHECK(pipe2(fds,O_CLOEXEC|O_NONBLOCK)==0);
        wl_data_offer_receive(a->selection,"text/plain;charset=utf-8",fds[1]);close(fds[1]);a->readers[a->reader_count++]=fds[0];
    } else if (c=='F') {
        for (unsigned i=0;i<a->reader_count;++i) if (a->readers[i]>=0) {
            unsigned char bytes[4096];size_t total=0;bool eof=false;
            for (;;) {
                ssize_t got=read(a->readers[i],bytes,sizeof(bytes));
                if (got<0 && errno==EINTR) continue;
                if (got<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) break;
                CHECK(got>=0);if (!got) { eof=true;break; }
                total+=(size_t)got;CHECK(total<=8*1024*1024);
            }
            printf("CLIPBOARD_DRAIN %u %zu %d\n",i,total,eof);close(a->readers[i]);a->readers[i]=-1;
        }
    } else if (c=='C' || c=='S' || c=='Z' || c=='J') {
        CHECK(a->source_count<16);
        struct clipboard_source *s=&a->sources[a->source_count];
        *s=(struct clipboard_source){.app=a,.index=(unsigned)a->source_count}; ++a->source_count;
        s->object=wl_data_device_manager_create_data_source(a->data_manager); CHECK(s->object);
        CHECK(wl_data_source_add_listener(s->object,&source_listener,s)==0);
        wl_data_source_offer(s->object,a->plain_clipboard ? "text/plain" : "text/plain;charset=utf-8");
        if (c=='J') {
            CHECK(a->keyboard_focus);
            wl_data_source_set_actions(s->object,WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
            wl_data_device_start_drag(a->data_device,s->object,a->keyboard_focus->surface,NULL,a->last_button);
        } else wl_data_device_set_selection(a->data_device,s->object,c=='Z' ? 0 : c=='S' ? a->old_clipboard_serial : a->clipboard_serial);
    } else if (c=='E') wl_data_device_set_selection(a->data_device,NULL,a->clipboard_serial);
    else if (c=='D') {
        CHECK(a->keyboard_focus);
        wl_data_device_start_drag(a->data_device,NULL,a->keyboard_focus->surface,NULL,a->last_button);
    } else if (c=='H') {
        CHECK(a->keyboard_focus && a->source_count);
        wl_data_device_start_drag(a->data_device,a->sources[a->source_count-1].object,a->keyboard_focus->surface,NULL,a->last_button);
    } else if (c=='R' || c=='O') {
        struct wl_data_offer *offer=c=='R' ? a->selection : a->previous_selection;
        if (!offer) {
            puts("CLIPBOARD_READ 0");
            CHECK(wl_display_roundtrip(a->display)>=0); printf("CLIPBOARD_SYNC %c\n",c); return;
        }
        int fds[2]; CHECK(pipe2(fds,O_CLOEXEC)==0);
        wl_data_offer_receive(offer,"text/plain;charset=utf-8",fds[1]); CHECK(close(fds[1])==0);
        CHECK(wl_display_roundtrip(a->display)>=0);
        unsigned char bytes[128]; size_t used=0;
        for (;;) {
            struct pollfd p={.fd=fds[0],.events=POLLIN};
            int ready; do { ready=poll(&p,1,4000); } while (ready<0 && errno==EINTR);
            CHECK(ready>0 && used<sizeof(bytes));
            ssize_t n=read(fds[0],bytes+used,sizeof(bytes)-used);
            if (n<0 && errno==EINTR) continue;
            CHECK(n>=0); if (!n) break; used+=(size_t)n;
        }
        CHECK(close(fds[0])==0);
        printf("CLIPBOARD_READ %zu",used);
        for (size_t i=0;i<used;++i) printf(" %02x",bytes[i]);
        putchar('\n');
    } else CHECK(false);
    CHECK(wl_display_roundtrip(a->display)>=0);
    printf("CLIPBOARD_SYNC %c\n",c);
}
static void release(void *data, struct wl_buffer *object) {
    struct buffer *b = data;
    wl_buffer_destroy(object);
    wl_list_remove(&b->link);
    free(b);
}
static const struct wl_buffer_listener buffer_listener = {.release = release};

static struct wl_buffer *make_buffer(struct view *v) {
    CHECK(v->width > 0 && v->width <= 3840 && v->height > 0 && v->height <= 2160);
    size_t size = (size_t)v->width * (size_t)v->height * 4;
    int fd = memfd_create("aetheri-wayland-test", MFD_CLOEXEC);
    CHECK(fd >= 0 && ftruncate(fd, (off_t)size) == 0);
    uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK(pixels != MAP_FAILED);
    uint32_t color = v->index == 0 ? UINT32_C(0xff2963c4) : UINT32_C(0xff26b478);
    for (size_t i = 0; i < size / 4; ++i) pixels[i] = color;
    /* A contrasting inset makes stride/order/cropping mistakes observable. */
    for (int y = 12; y < 32; ++y) for (int x = 12; x < 32; ++x)
        pixels[(size_t)y * (size_t)v->width + (size_t)x] = UINT32_C(0xffe9edf5);
    struct wl_shm_pool *pool = wl_shm_create_pool(v->app->shm, fd, (int32_t)size);
    CHECK(pool);
    struct buffer *b = calloc(1, sizeof(*b));
    CHECK(b);
    b->object = wl_shm_pool_create_buffer(pool, 0, v->width, v->height, v->width * 4, WL_SHM_FORMAT_XRGB8888);
    CHECK(b->object && wl_buffer_add_listener(b->object, &buffer_listener, b) == 0);
    wl_list_insert(&v->app->buffers, &b->link);
    wl_shm_pool_destroy(pool);
    CHECK(munmap(pixels, size) == 0 && close(fd) == 0);
    return b->object;
}
static void frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    (void)time;
    struct view *v = data;
    CHECK(v->frame == callback);
    wl_callback_destroy(callback);
    v->frame = NULL;
    printf("FRAME %u %d %d %u\n", v->index, v->width, v->height, v->fullscreen ? 1u : 0u);
}
static const struct wl_callback_listener frame_listener = {.done = frame_done};
static void configured(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct view *v = data;
    xdg_surface_ack_configure(surface, serial);
    struct wl_buffer *buffer = make_buffer(v);
    if (v->frame) wl_callback_destroy(v->frame);
    v->frame = wl_surface_frame(v->surface);
    CHECK(v->frame && wl_callback_add_listener(v->frame, &frame_listener, v) == 0);
    wl_surface_attach(v->surface, buffer, 0, 0);
    wl_surface_damage_buffer(v->surface, 0, 0, v->width, v->height);
    wl_surface_commit(v->surface);
    printf("CONFIG %u %d %d %u\n", v->index, v->width, v->height, v->fullscreen ? 1u : 0u);
}
static const struct xdg_surface_listener surface_listener = {.configure = configured};
static void top_configure(void *data, struct xdg_toplevel *top, int32_t width, int32_t height, struct wl_array *states) {
    (void)top;
    struct view *v = data;
    v->width = width > 0 ? width : 360;
    v->height = height > 0 ? height : 220;
    v->fullscreen = false;
    uint32_t *state;
    wl_array_for_each(state, states) if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) v->fullscreen = true;
}
static void top_close(void *data, struct xdg_toplevel *top) {
    (void)data; (void)top;
    CHECK(false);
}
static void top_bounds(void *data, struct xdg_toplevel *top, int32_t width, int32_t height) {
    (void)top; struct view *v=data;
    CHECK(width > 0 && height > 0 && width <= 3840 && height <= 2160);
    printf("BOUNDS %u %d %d\n",v->index,width,height);
}
static void top_capabilities(void *data, struct xdg_toplevel *top, struct wl_array *capabilities) {
    (void)top;
    struct view *v = data;
    unsigned mask = 0;
    uint32_t *capability;
    wl_array_for_each(capability, capabilities) {
        CHECK(*capability == XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE ||
              *capability == XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
        mask |= 1u << *capability;
    }
    CHECK(mask == ((1u << XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE) |
                   (1u << XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN)));
    printf("CAPABILITIES %u\n", v->index);
}
static const struct xdg_toplevel_listener top_listener = {
    .configure = top_configure, .close = top_close,
    .configure_bounds = top_bounds, .wm_capabilities = top_capabilities};
static void ping(void *data, struct xdg_wm_base *shell, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(shell, serial);
}
static const struct xdg_wm_base_listener shell_listener = {.ping = ping};
static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
        int32_t width, int32_t height, int32_t subpixel, const char *make, const char *model, int32_t transform) {
    (void)data; (void)output; (void)width; (void)height; (void)subpixel;
    (void)make; (void)model; (void)transform;
    printf("OUTPUT_POSITION %d %d\n", x, y);
}
static void output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    (void)data; (void)output; (void)refresh;
    if (flags & WL_OUTPUT_MODE_CURRENT) printf("OUTPUT_MODE %d %d\n", width, height);
}
static const struct wl_output_listener output_listener = {.geometry = output_geometry, .mode = output_mode};
static struct view *find_view(struct app *a, struct wl_surface *surface) {
    for (unsigned i = 0; i < 2; ++i) if (a->views[i].surface == surface) return &a->views[i];
    CHECK(false);
    return NULL;
}
static void synced(void *data, struct wl_callback *callback, uint32_t serial) {
    (void)serial;
    struct app *a = data;
    CHECK(a->sync == callback);
    wl_callback_destroy(callback); a->sync = NULL; puts("INPUT_SYNC");
}
static const struct wl_callback_listener sync_listener = {.done = synced};
static void input_sync(struct app *a) {
    CHECK(!a->sync);
    a->sync = wl_display_sync(a->display);
    CHECK(a->sync && wl_callback_add_listener(a->sync, &sync_listener, a) == 0);
}
static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
    (void)pointer; (void)serial;
    struct app *a = data; a->pointer_focus = find_view(a, surface);
    printf("POINTER_ENTER %u %.2f %.2f\n", a->pointer_focus->index, wl_fixed_to_double(x), wl_fixed_to_double(y));
}
static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    (void)pointer; (void)serial; (void)surface;
    struct app *a = data;
    puts("POINTER_LEAVE"); a->pointer_focus = NULL;
}
static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)pointer; (void)time;
    struct app *a = data; CHECK(a->pointer_focus);
    printf("MOTION %u %.2f %.2f\n", a->pointer_focus->index, wl_fixed_to_double(x), wl_fixed_to_double(y));
}
static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void)pointer; (void)time;
    struct app *a = data; CHECK(a->pointer_focus);
    printf("BUTTON %u %u %u\n", a->pointer_focus->index, button, state);
    if (!state) return;
    a->last_button = serial;
    if (a->grab_mode == 1) xdg_toplevel_move(a->pointer_focus->top, a->seat, serial);
    if (a->grab_mode == 2) xdg_toplevel_resize(a->pointer_focus->top, a->seat, serial, XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT);
    if (a->grab_mode == 3) xdg_toplevel_move(a->views[1].top, a->seat, serial);
    if (a->grab_mode) { a->grab_mode = 0; input_sync(a); }
}
static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)pointer; (void)time; (void)axis;
    struct app *a = data; CHECK(a->pointer_focus);
    printf("AXIS %u %.2f\n", a->pointer_focus->index, wl_fixed_to_double(value));
}
static void pointer_frame(void *data, struct wl_pointer *pointer) { (void)data; (void)pointer; }
static void axis_source(void *data, struct wl_pointer *pointer, uint32_t source) { (void)data; (void)pointer; (void)source; }
static void axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis) { (void)data; (void)pointer; (void)time; (void)axis; }
static void axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t step) { (void)data; (void)pointer; (void)axis; (void)step; }
static const struct wl_pointer_listener pointer_listener = {.enter = pointer_enter, .leave = pointer_leave,
    .motion = pointer_motion, .button = pointer_button, .axis = pointer_axis, .frame = pointer_frame,
    .axis_source = axis_source, .axis_stop = axis_stop, .axis_discrete = axis_discrete};
static void keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd, uint32_t size) {
    (void)keyboard;
    struct app *a = data;
    CHECK(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size && size <= 1024 * 1024);
    char *text = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    CHECK(text != MAP_FAILED && text[size - 1] == '\0');
    struct xkb_keymap *map = xkb_keymap_new_from_string(a->xkb, text, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    CHECK(map);
    xkb_state_unref(a->key_state); xkb_keymap_unref(a->keymap);
    a->keymap = map; a->key_state = xkb_state_new(map); CHECK(a->key_state);
    CHECK(munmap(text, size) == 0 && close(fd) == 0);
    puts("KEYMAP");
}
static void keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    (void)keyboard; (void)serial;
    struct app *a = data; a->keyboard_focus = find_view(a, surface);
    uint32_t *key;
    wl_array_for_each(key, keys) CHECK(*key != 15); /* compositor-consumed Tab */
    a->old_clipboard_serial=a->clipboard_serial; a->clipboard_serial=serial;
    printf("KEYBOARD_ENTER %u\n", a->keyboard_focus->index);
}
static void keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {
    (void)keyboard; (void)serial; (void)surface;
    struct app *a = data; a->keyboard_focus = NULL; puts("KEYBOARD_LEAVE");
}
static void keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t code, uint32_t state) {
    (void)keyboard; (void)serial; (void)time;
    struct app *a = data; CHECK(a->key_state && a->keyboard_focus);
    uint32_t character = xkb_state_key_get_utf32(a->key_state, code + 8);
    printf("KEY %u %u %u %u\n", a->keyboard_focus->index, code, state, character);
}
static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    (void)keyboard; (void)serial;
    struct app *a = data; CHECK(a->key_state);
    (void)xkb_state_update_mask(a->key_state, depressed, latched, locked, 0, 0, group);
    printf("MODIFIERS %u\n", depressed);
}
static void repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
    (void)data; (void)keyboard; CHECK(rate >= 0 && delay >= 0);
}
static const struct wl_keyboard_listener keyboard_listener = {.keymap = keyboard_keymap, .enter = keyboard_enter,
    .leave = keyboard_leave, .key = keyboard_key, .modifiers = keyboard_modifiers, .repeat_info = repeat_info};
static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    struct app *a = data;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !a->pointer) {
        a->pointer = wl_seat_get_pointer(seat); CHECK(a->pointer);
        CHECK(wl_pointer_add_listener(a->pointer, &pointer_listener, a) == 0);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && a->pointer) {
        wl_pointer_release(a->pointer); a->pointer = NULL; a->pointer_focus = NULL;
    }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !a->keyboard) {
        a->keyboard = wl_seat_get_keyboard(seat); CHECK(a->keyboard);
        CHECK(wl_keyboard_add_listener(a->keyboard, &keyboard_listener, a) == 0);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && a->keyboard) {
        wl_keyboard_release(a->keyboard); a->keyboard = NULL; a->keyboard_focus = NULL;
    }
    printf("CAPS %u\n", caps);
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) { (void)data; (void)seat; CHECK(!strcmp(name, "seat0")); }
static const struct wl_seat_listener seat_listener = {.capabilities = seat_capabilities, .name = seat_name};
#ifdef AETHERI_TEST_TEXT_INPUT
static unsigned text_index(struct app *a,struct zwp_text_input_v3 *input) {
    for (unsigned i=0;i<2;++i) if (a->text[i]==input) return i;
    CHECK(false);return 0;
}
static void text_enter(void *data,struct zwp_text_input_v3 *input,struct wl_surface *surface) {
    struct app *a=data;printf("TEXT_ENTER %u %u\n",text_index(a,input),find_view(a,surface)->index);
}
static void text_leave(void *data,struct zwp_text_input_v3 *input,struct wl_surface *surface) {
    (void)surface;printf("TEXT_LEAVE %u\n",text_index(data,input));
}
static void text_bytes(struct app *a,struct zwp_text_input_v3 *input,const char *kind,const char *bytes) {
    size_t n=bytes?strlen(bytes):0;printf("TEXT_%s %u %zu",kind,text_index(a,input),n);
    for (size_t i=0;i<n;++i) printf(" %02x",(unsigned char)bytes[i]);
    putchar('\n');
}
static void text_preedit(void *data,struct zwp_text_input_v3 *input,const char *text,int32_t begin,int32_t end) {
    CHECK(begin>=-1 && end>=-1);text_bytes(data,input,"PREEDIT",text);
}
static void text_commit(void *data,struct zwp_text_input_v3 *input,const char *text) { text_bytes(data,input,"COMMIT",text); }
static void text_delete(void *data,struct zwp_text_input_v3 *input,uint32_t before,uint32_t after) {
    (void)data;(void)input;(void)before;(void)after;CHECK(false);
}
static void text_done(void *data,struct zwp_text_input_v3 *input,uint32_t serial) {
    printf("TEXT_DONE %u %u\n",text_index(data,input),serial);
}
static const struct zwp_text_input_v3_listener text_listener={.enter=text_enter,.leave=text_leave,
    .preedit_string=text_preedit,.commit_string=text_commit,.delete_surrounding_text=text_delete,.done=text_done};
static void create_text(struct app *a,unsigned index) {
    a->text[index]=zwp_text_input_manager_v3_get_text_input(a->text_manager,a->seat);CHECK(a->text[index]);
    CHECK(!zwp_text_input_v3_add_listener(a->text[index],&text_listener,a));
}
static void text_command(struct app *a,char c) {
    struct zwp_text_input_v3 *input=a->text[0];
    if (c=='U') {
        zwp_text_input_v3_enable(input);CHECK(wl_display_roundtrip(a->display)>=0);puts("TEXT_SYNC U");return;
    }
    if (c=='E' || c=='R') zwp_text_input_v3_enable(input);
    else if (c=='X') zwp_text_input_v3_disable(input);
    else if (c=='C') zwp_text_input_v3_set_cursor_rectangle(input,10,10,5,20);
    else if (strchr("PIHSN",c)) zwp_text_input_v3_set_content_type(input,
        c=='H'?ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT:c=='S'?ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA:0,
        c=='P'?ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD:c=='I'?ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PIN:ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
    else if (c=='A' || c=='B') {
        input=a->text[1];
        if (c=='A') zwp_text_input_v3_enable(input);else zwp_text_input_v3_disable(input);
    } else if (c=='T') {
        zwp_text_input_v3_destroy(input);a->text[0]=NULL;create_text(a,0);input=a->text[0];
    } else if (c!='V') CHECK(false);
    zwp_text_input_v3_commit(input);
    CHECK(wl_display_roundtrip(a->display)>=0);printf("TEXT_SYNC %c\n",c);
}
#endif
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    struct app *a = data;
    if (!strcmp(interface, "wl_compositor")) {
        CHECK(!a->compositor && version >= 4);
        a->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (!strcmp(interface, "wl_shm")) {
        CHECK(!a->shm);
        a->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (!strcmp(interface, "xdg_wm_base")) {
        CHECK(!a->shell && version >= a->shell_version);
        a->shell = wl_registry_bind(registry, name, &xdg_wm_base_interface, a->shell_version);
        CHECK(xdg_wm_base_add_listener(a->shell, &shell_listener, a) == 0);
    } else if (!strcmp(interface, "wl_output")) {
        CHECK(a->output_count < 3);
        /* Version 1 needs no release request; no events are required for selection. */
        struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, 1);
        CHECK(output && wl_output_add_listener(output, &output_listener, a) == 0);
        a->outputs[a->output_count++] = output;
    } else if (a->input && !strcmp(interface, "wl_seat")) {
        CHECK(!a->seat && version >= 5);
        a->seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
        CHECK(a->seat && wl_seat_add_listener(a->seat, &seat_listener, a) == 0);
    }
#ifdef AETHERI_TEST_TEXT_INPUT
    else if (a->text_mode && !strcmp(interface,"zwp_text_input_manager_v3")) {
        CHECK(!a->text_manager);a->text_manager=wl_registry_bind(registry,name,&zwp_text_input_manager_v3_interface,1);CHECK(a->text_manager);
    }
#endif
    else if (a->clipboard_payload && !strcmp(interface,"wl_data_device_manager")) {
        CHECK(!a->data_manager && version>=3);
        a->data_manager=wl_registry_bind(registry,name,&wl_data_device_manager_interface,3);
    }
}
static void global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
    CHECK(false);
}
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = global_remove};
static void create_view(struct app *a, unsigned index) {
    struct view *v = &a->views[index];
    *v = (struct view){.app = a, .index = index, .width = 360, .height = 220};
    v->surface = wl_compositor_create_surface(a->compositor);
    v->xdg = xdg_wm_base_get_xdg_surface(a->shell, v->surface);
    v->top = xdg_surface_get_toplevel(v->xdg);
    CHECK(v->surface && v->xdg && v->top);
    CHECK(xdg_surface_add_listener(v->xdg, &surface_listener, v) == 0);
    CHECK(xdg_toplevel_add_listener(v->top, &top_listener, v) == 0);
    xdg_toplevel_set_title(v->top, index == 0 ? "Aetheri protocol test A" : "Aetheri protocol test B");
    xdg_toplevel_set_app_id(v->top, "aetheri.private.compositor-test");
    wl_surface_commit(v->surface);
}
static void destroy_view(struct view *v) {
    if (!v->surface) return;
    if (v->frame) wl_callback_destroy(v->frame);
    xdg_toplevel_destroy(v->top);
    xdg_surface_destroy(v->xdg);
    wl_surface_destroy(v->surface);
    v->surface = NULL;
    v->frame = NULL;
}
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    CHECK(argc == 1 || (argc == 2 && (!strcmp(argv[1], "--legacy") || !strcmp(argv[1], "--input") || !strcmp(argv[1],"--text-input") || !strcmp(argv[1],"--clipboard-a") || !strcmp(argv[1],"--clipboard-b") || !strcmp(argv[1],"--clipboard-stall"))) ||
          (argc==3 && (!strcmp(argv[1],"--clipboard-file") || !strcmp(argv[1],"--clipboard-plain"))));
    CHECK(signal(SIGPIPE,SIG_IGN)!=SIG_ERR); /* Test peer may observe a cancelled receive. */
    struct app a = {.shell_version = argc == 2 && !strcmp(argv[1], "--legacy") ? 3 : 5,
                    .input = argc == 2 && !strcmp(argv[1], "--input")};
    a.text_mode=argc==2 && !strcmp(argv[1],"--text-input");a.input=a.input || a.text_mode;
#ifndef AETHERI_TEST_TEXT_INPUT
    CHECK(!a.text_mode);
#endif
    if (argc==2 && !strncmp(argv[1],"--clipboard-",12)) {
        a.input=true;a.stall_clipboard=!strcmp(argv[1],"--clipboard-stall");a.clipboard_payload=argv[1][12]=='a' ? "Aetheri A Grüße\n" : "Aetheri B Grüße\n";
    }
    if (argc==3) {
        a.plain_clipboard=!strcmp(argv[1],"--clipboard-plain");
        int fd=open(argv[2],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);struct stat st;CHECK(fd>=0 && !fstat(fd,&st) && S_ISREG(st.st_mode) && st.st_size>0 && st.st_size<=65536);
        a.clipboard_size=(size_t)st.st_size;a.owned_payload=malloc(a.clipboard_size+1);CHECK(a.owned_payload);
        size_t at=0;while (at<a.clipboard_size) { ssize_t n=read(fd,a.owned_payload+at,a.clipboard_size-at);if (n<0 && errno==EINTR) continue;CHECK(n>0);at+=(size_t)n; }
        CHECK(close(fd)==0);a.owned_payload[a.clipboard_size]=0;a.clipboard_payload=a.owned_payload;a.input=true;
    } else if (a.clipboard_payload) a.clipboard_size=strlen(a.clipboard_payload);
    if (a.input) { a.xkb = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES); CHECK(a.xkb); }
    wl_list_init(&a.buffers);
    a.display = wl_display_connect(NULL);
    CHECK(a.display);
    a.registry = wl_display_get_registry(a.display);
    CHECK(a.registry && wl_registry_add_listener(a.registry, &registry_listener, &a) == 0);
    CHECK(wl_display_roundtrip(a.display) >= 0 && a.compositor && a.shm && a.shell && a.output_count == 3);
#ifdef AETHERI_TEST_TEXT_INPUT
    if (a.text_mode) { CHECK(a.text_manager && a.seat);create_text(&a,0);create_text(&a,1); }
#endif
    if (a.clipboard_payload) {
        CHECK(a.data_manager && a.seat);
        a.data_device=wl_data_device_manager_get_data_device(a.data_manager,a.seat); CHECK(a.data_device);
        CHECK(wl_data_device_add_listener(a.data_device,&device_listener,&a)==0);
    }
    create_view(&a, 0);
    create_view(&a, 1);
    for (;;) {
        CHECK(wl_display_dispatch_pending(a.display) >= 0);
        CHECK(wl_display_flush(a.display) >= 0 || errno == EAGAIN);
        struct pollfd fds[2] = {{wl_display_get_fd(a.display), POLLIN, 0}, {STDIN_FILENO, POLLIN, 0}};
        int r;
        do { r = poll(fds, 2, 10000); } while (r < 0 && errno == EINTR);
        CHECK(r > 0);
        if (fds[0].revents) CHECK(wl_display_dispatch(a.display) >= 0);
        if (fds[1].revents) {
            char c;
            CHECK(read(STDIN_FILENO, &c, 1) == 1);
            if (c == 'q') break;
            if (c == 'f') xdg_toplevel_set_fullscreen(a.views[0].top, a.outputs[2]);
            else if (c == 'r') xdg_toplevel_unset_fullscreen(a.views[0].top);
            else if (c == 'm') xdg_toplevel_set_maximized(a.views[0].top);
            else if (c == 'n') xdg_toplevel_unset_maximized(a.views[0].top);
            else if (c == 'u') {
                char title[600] = "Grüße\n"; size_t used = strlen(title);
                for (unsigned i = 0; i < 280; ++i) { title[used++] = (char)0xc3; title[used++] = (char)0xa4; }
                title[used] = '\0'; xdg_toplevel_set_title(a.views[0].top, title);
                puts("METADATA_SENT");
            }
            else if (c == 'p') {
                CHECK(!a.views[0].frame);
                a.views[0].frame = wl_surface_frame(a.views[0].surface);
                CHECK(a.views[0].frame && wl_callback_add_listener(a.views[0].frame, &frame_listener, &a.views[0]) == 0);
                wl_surface_commit(a.views[0].surface);
            }
            else if (c == 'd') { destroy_view(&a.views[1]); printf("DESTROY 1\n"); }
            else if (c == 'w') { destroy_view(&a.views[0]); printf("DESTROY 0\n"); }
            else if (c == 'c') { CHECK(!a.views[1].surface); create_view(&a, 1); }
            else if (a.input && c == 'g') { a.grab_mode = 1; puts("ARM_MOVE"); }
            else if (a.input && c == 'z') { a.grab_mode = 2; puts("ARM_RESIZE"); }
            else if (a.input && c == 'h') { a.grab_mode = 3; puts("ARM_FOREIGN"); }
            else if (a.input && c == 't') { xdg_toplevel_move(a.views[0].top, a.seat, a.last_button); input_sync(&a); }
            else if (a.clipboard_payload && c=='U') {
                destroy_view(&a.views[0]); destroy_view(&a.views[1]);
                CHECK(wl_display_roundtrip(a.display)>=0); puts("CLIPBOARD_VIEWS_UNMAPPED");
            } else if (a.clipboard_payload && c=='N') {
                CHECK(!a.views[0].surface && !a.views[1].surface);
                create_view(&a,0); create_view(&a,1); puts("CLIPBOARD_VIEWS_CREATED");
            }
            else if (a.clipboard_payload && strchr("CSZERODJHTWF",c)) clipboard_command(&a,c);
#ifdef AETHERI_TEST_TEXT_INPUT
            else if (a.text_mode) text_command(&a,c);
#endif
            else CHECK(false);
        }
    }
    for (unsigned i = 0; i < 2; ++i) destroy_view(&a.views[i]);
#ifdef AETHERI_TEST_TEXT_INPUT
    if (a.text_manager) {
        for (unsigned i=0;i<2;++i) zwp_text_input_v3_destroy(a.text[i]);
        zwp_text_input_manager_v3_destroy(a.text_manager);
    }
#endif
    CHECK(wl_display_roundtrip(a.display) >= 0);
    struct buffer *b, *next;
    wl_list_for_each_safe(b, next, &a.buffers, link) release(b, b->object);
    for (unsigned i = 0; i < a.output_count; ++i) wl_output_destroy(a.outputs[i]);
    xdg_wm_base_destroy(a.shell);
    wl_shm_destroy(a.shm);
    wl_compositor_destroy(a.compositor);
    wl_registry_destroy(a.registry);
    if (a.sync) wl_callback_destroy(a.sync);
    if (a.pointer) wl_pointer_release(a.pointer);
    if (a.keyboard) wl_keyboard_release(a.keyboard);
    for (unsigned i=0;i<a.stalled_count;++i) if (a.stalled[i]>=0) close(a.stalled[i]);
    for (unsigned i=0;i<a.reader_count;++i) if (a.readers[i]>=0) close(a.readers[i]);
    free(a.owned_payload);
    if (a.seat) wl_seat_destroy(a.seat);
    for (size_t i=0;i<a.source_count;++i) wl_data_source_destroy(a.sources[i].object);
    for (size_t i=0;i<a.offer_count;++i) wl_data_offer_destroy(a.offers[i]);
    if (a.data_device) wl_data_device_release(a.data_device);
    if (a.data_manager) wl_data_device_manager_destroy(a.data_manager);
    wl_display_disconnect(a.display);
    xkb_state_unref(a.key_state); xkb_keymap_unref(a.keymap); xkb_context_unref(a.xkb);
    puts("CLOSED");
    return 0;
}
