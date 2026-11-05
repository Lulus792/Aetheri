/* Public adapter contract on an independent real Wayland connection. */
#define _GNU_SOURCE
#include "terminal/clipboard.h"
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"clipboard API %d: %s\n",__LINE__,#x);exit(1); } } while (0)
struct test { struct ae_clipboard *clipboard;unsigned wakes; };
static void rejected(struct test *t) {
    struct ae_clipboard *original=t->clipboard;
    CHECK(original && ae_clipboard_watch_fd(original)==-1);
    CHECK(!ae_clipboard_focused(original) && !ae_clipboard_focus_epoch(original) && !ae_clipboard_deadline(original));
    struct ae_clipboard_observation out={.bytes=123};
    CHECK(ae_clipboard_observe(original,&out)==EINVAL && out.bytes==123);
    CHECK(ae_clipboard_pump(original)==EINVAL && ae_clipboard_cancel(original)==EINVAL);
    CHECK(ae_clipboard_receive(original,4096)==EINVAL);
    char text[]="literal",*pointer=text;size_t size=0;
    CHECK(ae_clipboard_publish(original,&pointer,7)==EINVAL && pointer==text);
    CHECK(ae_clipboard_result(original,&pointer,&size)==AE_CLIPBOARD_IO_ERROR && pointer==text && !size);
    CHECK(ae_clipboard_close(&t->clipboard)==EBUSY && t->clipboard==original);
}
static void wake(void *data) { struct test *t=data;++t->wakes;rejected(t); }
static void *foreign(void *data) { rejected(data);return NULL; }
static void global(void *data,struct wl_registry *registry,uint32_t id,const char *name,uint32_t version) {
    struct wl_compositor **out=data;
    if (!strcmp(name,"wl_compositor") && !*out) *out=wl_registry_bind(registry,id,&wl_compositor_interface,version>=4 ? 4 : version);
}
static void removed(void *data,struct wl_registry *registry,uint32_t id) { (void)data;(void)registry;(void)id; }
static const struct wl_registry_listener listener={.global=global,.global_remove=removed};
static unsigned fds(void) {
    DIR *directory=opendir("/proc/self/fd");CHECK(directory);unsigned n=0;
    while (readdir(directory)) ++n;
    CHECK(!closedir(directory));return n;
}
int main(void) {
    struct wl_display *display=wl_display_connect(NULL),*other=wl_display_connect(NULL);CHECK(display && other);
    struct wl_compositor *compositor=NULL;
    struct wl_registry *registry=wl_display_get_registry(display);
    CHECK(registry && !wl_registry_add_listener(registry,&listener,&compositor));
    CHECK(wl_display_roundtrip(display)>=0 && compositor);
    struct wl_surface *surface=wl_compositor_create_surface(compositor);CHECK(surface);
    struct test t={0};CHECK(ae_clipboard_open(&t.clipboard,other,surface,wake,&t)==EINVAL && !t.clipboard);
    unsigned baseline=fds();
    for (unsigned i=0;i<16;++i) {
        CHECK(!ae_clipboard_open(&t.clipboard,display,surface,wake,&t));
        CHECK(ae_clipboard_open(&t.clipboard,display,surface,wake,&t)==EINVAL);
        int flags=fcntl(ae_clipboard_watch_fd(t.clipboard),F_GETFD);CHECK(flags>=0 && (flags&FD_CLOEXEC));
        CHECK(wl_display_roundtrip(display)>=0 && wl_display_roundtrip(display)>=0);
        struct ae_clipboard_observation out={0};CHECK(!ae_clipboard_observe(t.clipboard,&out));
        CHECK(out.available && !out.focused && !out.bytes && !out.receive && !out.sync_pending);
        CHECK(!ae_clipboard_pump(t.clipboard));
        CHECK(ae_clipboard_receive(t.clipboard,4096)==EACCES);
        char *copy=strdup("owned");CHECK(copy);char *original=copy;
        CHECK(ae_clipboard_publish(t.clipboard,&copy,5)==EACCES && copy==original);free(copy);
        pthread_t thread;CHECK(!pthread_create(&thread,NULL,foreign,&t));CHECK(!pthread_join(thread,NULL));
        CHECK(!ae_clipboard_close(&t.clipboard) && !t.clipboard);
        CHECK(!ae_clipboard_close(&t.clipboard));
        CHECK(wl_display_roundtrip(display)>=0 && fds()==baseline);
    }
    CHECK(t.wakes>=16);
    wl_surface_destroy(surface);wl_compositor_destroy(compositor);wl_registry_destroy(registry);
    CHECK(wl_display_roundtrip(display)>=0);wl_display_disconnect(other);wl_display_disconnect(display);
    puts("clipboard API: borrowed connection, wrong-thread/wake reentry rejection, no-focus refusal and repeated exact FD cleanup passed");
    return 0;
}
