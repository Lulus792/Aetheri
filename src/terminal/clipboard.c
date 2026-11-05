#define _GNU_SOURCE
#include "clipboard.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

enum { OFFERS=64, SOURCES=4, SENDS=8, IO_BUDGET=16384 };
static const char utf8_mime[]="text/plain;charset=utf-8",plain_mime[]="text/plain";
struct offer { struct wl_data_offer *object;bool utf8,plain; };
struct source {
    struct ae_clipboard *owner;struct wl_data_source *object;
    char *text;size_t length;unsigned references;
};
struct send { int fd;struct source *source;size_t position;uint64_t deadline,cookie; };
struct ae_clipboard {
    pthread_t thread;struct wl_display *display;struct wl_surface *surface;
    struct wl_registry *registry;struct wl_seat *seat;struct wl_keyboard *keyboard;
    struct wl_data_device_manager *manager;struct wl_data_device *device;
    struct wl_callback *initial_sync,*receive_sync;
    uint32_t seat_name,manager_name,serial;unsigned seat_count;
    bool enumerated,focused,disabled,failed,in_wake;uint64_t focus_epoch;
    void (*wake)(void *);void *userdata;int epoll;
    struct offer offers[OFFERS],*selection;struct source sources[SOURCES];
    struct send sends[SENDS];uint64_t cookie;size_t memory;
    int receive_fd;char *receive_text;size_t receive_size,receive_limit;
    uint64_t receive_deadline,receive_cookie;enum ae_clipboard_result result;
};
static bool owner(const struct ae_clipboard *c) { return c && pthread_equal(c->thread,pthread_self()) && !c->in_wake; }
static uint64_t now_ms(void) {
    struct timespec t;if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return (uint64_t)t.tv_sec*UINT64_C(1000)+(uint64_t)t.tv_nsec/UINT64_C(1000000);
}
static void changed(struct ae_clipboard *c) {
    if (c->wake) { c->in_wake=true;c->wake(c->userdata);c->in_wake=false; }
}
static bool available(const struct ae_clipboard *c) { return c->enumerated && c->seat_count==1 && c->device && c->keyboard && !c->disabled && !c->failed; }
bool ae_clipboard_focused(const struct ae_clipboard *c) { return owner(c) && available(c) && c->focused && c->serial; }
uint64_t ae_clipboard_focus_epoch(const struct ae_clipboard *c) { return ae_clipboard_focused(c) ? c->focus_epoch : 0; }
static void source_unref(struct source *s) {
    if (!--s->references) {
        s->owner->memory-=s->length+1;free(s->text);*s=(struct source){0};
    }
}
static void close_send(struct ae_clipboard *c,unsigned i) {
    struct send *s=&c->sends[i];if (s->fd<0) return;
    (void)epoll_ctl(c->epoll,EPOLL_CTL_DEL,s->fd,NULL);close(s->fd);
    source_unref(s->source);*s=(struct send){.fd=-1};
}
static void receive_end(struct ae_clipboard *c,enum ae_clipboard_result result) {
    if (c->receive_fd>=0) { (void)epoll_ctl(c->epoll,EPOLL_CTL_DEL,c->receive_fd,NULL);close(c->receive_fd);c->receive_fd=-1; }
    c->receive_deadline=0;
    if (result!=AE_CLIPBOARD_DATA && c->receive_text) {
        c->memory-=c->receive_limit+1;free(c->receive_text);c->receive_text=NULL;c->receive_size=c->receive_limit=0;
    }
    c->result=result;changed(c);
}
static void cancel_receive(struct ae_clipboard *c) {
    if (c->result==AE_CLIPBOARD_PENDING || c->result==AE_CLIPBOARD_DATA) receive_end(c,AE_CLIPBOARD_CANCELLED);
}
static void disable(struct ae_clipboard *c) { c->disabled=true;c->focused=false;c->serial=0;cancel_receive(c);changed(c); }
static bool text_mime(const char *s) { return s && (!strcmp(s,utf8_mime) || !strcmp(s,plain_mime)); }
static int add_fd(struct ae_clipboard *c,int fd,uint32_t events,unsigned slot,uint64_t *cookie) {
    if (c->cookie>=UINT64_MAX>>8) return EOVERFLOW;
    uint64_t token=(++c->cookie<<8)|slot;struct epoll_event event={.events=events,.data.u64=token};
    if (epoll_ctl(c->epoll,EPOLL_CTL_ADD,fd,&event)) return errno;
    *cookie=token;return 0;
}
static int nonblocking_stream(int fd) {
    struct stat st;if (fstat(fd,&st)) return errno;
    if (!S_ISFIFO(st.st_mode)) {
        int kind=0;socklen_t n=sizeof(kind);
        if (!S_ISSOCK(st.st_mode) || getsockopt(fd,SOL_SOCKET,SO_TYPE,&kind,&n) || kind!=SOCK_STREAM) return EINVAL;
    }
    int flags=fcntl(fd,F_GETFL);if (flags<0 || fcntl(fd,F_SETFD,FD_CLOEXEC) || fcntl(fd,F_SETFL,flags|O_NONBLOCK)) return errno;
    return 0;
}
/* Keep SIGPIPE local to this write. Do not change process-wide dispositions or
   consume a signal already pending on the owner thread. */
static ssize_t write_stream(int fd,const void *bytes,size_t length) {
    sigset_t set,old,pending;sigemptyset(&set);sigaddset(&set,SIGPIPE);
    int error=pthread_sigmask(SIG_BLOCK,&set,&old);if (error) { errno=error;return -1; }
    if (sigpending(&pending)) { error=errno;(void)pthread_sigmask(SIG_SETMASK,&old,NULL);errno=error;return -1; }
    ssize_t n=write(fd,bytes,length);error=errno;
    if (n<0 && error==EPIPE && !sigismember(&pending,SIGPIPE)) { struct timespec zero={0};(void)sigtimedwait(&set,NULL,&zero); }
    int restored=pthread_sigmask(SIG_SETMASK,&old,NULL);errno=restored ? restored : error;return restored ? -1 : n;
}
static void source_target(void *data,struct wl_data_source *object,const char *mime) { (void)data;(void)object;(void)mime; }
static void source_send(void *data,struct wl_data_source *object,const char *mime,int32_t fd) {
    struct source *s=data;struct ae_clipboard *c=s->owner;(void)object;
    unsigned i=0;while (i<SENDS && c->sends[i].fd>=0) ++i;
    uint64_t now=now_ms();
    if (!text_mime(mime) || i==SENDS || !now || now>UINT64_MAX-5000 || nonblocking_stream(fd)) { close(fd);return; }
    struct send *send=&c->sends[i];
    int error=add_fd(c,fd,EPOLLOUT,i+1,&send->cookie);
    if (error) { close(fd);return; }
    send->fd=fd;send->source=s;send->position=0;send->deadline=now+5000;++s->references;changed(c);
}
static void source_cancelled(void *data,struct wl_data_source *object) {
    struct source *s=data;struct ae_clipboard *c=s->owner;
    if (s->object==object) { wl_data_source_destroy(object);s->object=NULL;source_unref(s);changed(c); }
}
static void source_drop(void *data,struct wl_data_source *object) { (void)data;(void)object; }
static void source_action(void *data,struct wl_data_source *object,uint32_t action) { (void)data;(void)object;(void)action; }
static const struct wl_data_source_listener source_listener={.target=source_target,.send=source_send,.cancelled=source_cancelled,.dnd_drop_performed=source_drop,.dnd_finished=source_drop,.action=source_action};
static void offer_mime(void *data,struct wl_data_offer *object,const char *mime) {
    struct offer *o=data;(void)object;if (!strcmp(mime,utf8_mime)) o->utf8=true;else if (!strcmp(mime,plain_mime)) o->plain=true;
}
static void offer_actions(void *data,struct wl_data_offer *object,uint32_t actions) { (void)data;(void)object;(void)actions; }
static const struct wl_data_offer_listener offer_listener={.offer=offer_mime,.source_actions=offer_actions,.action=offer_actions};
static struct offer *find_offer(struct ae_clipboard *c,struct wl_data_offer *object) {
    for (unsigned i=0;i<OFFERS;++i) if (c->offers[i].object==object) return &c->offers[i];
    return NULL;
}
static void drop_offer(struct offer *o) { if (o->object) wl_data_offer_destroy(o->object);*o=(struct offer){0}; }
static void drop_selection(struct ae_clipboard *c) {
    c->selection=NULL;
    for (unsigned i=0;i<OFFERS;++i) drop_offer(&c->offers[i]);
}
static void device_offer(void *data,struct wl_data_device *device,struct wl_data_offer *object) {
    struct ae_clipboard *c=data;(void)device;
    for (unsigned i=0;i<OFFERS;++i) if (!c->offers[i].object) {
        c->offers[i]=(struct offer){.object=object};
        if (wl_data_offer_add_listener(object,&offer_listener,&c->offers[i])) c->failed=true;
        return;
    }
    /* A trusted host exceeding the declared object budget is a terminal adapter
       error, not permission to grow an unbounded offer list. */
    wl_data_offer_destroy(object);c->failed=true;changed(c);
}
static void device_selection(void *data,struct wl_data_device *device,struct wl_data_offer *object) {
    struct ae_clipboard *c=data;(void)device;struct offer *next=object ? find_offer(c,object) : NULL;
    if (object && !next) c->failed=true;
    cancel_receive(c);c->selection=next;
    for (unsigned i=0;i<OFFERS;++i) if (&c->offers[i]!=next) drop_offer(&c->offers[i]);
    changed(c);
}
static void device_enter(void *data,struct wl_data_device *device,uint32_t serial,struct wl_surface *surface,wl_fixed_t x,wl_fixed_t y,struct wl_data_offer *object) {
    struct ae_clipboard *c=data;(void)device;(void)serial;(void)surface;(void)x;(void)y;
    struct offer *o=object ? find_offer(c,object) : NULL;if (o && o!=c->selection) drop_offer(o); /* No DnD import. */
}
static void device_leave(void *data,struct wl_data_device *device) { (void)data;(void)device; }
static void device_motion(void *data,struct wl_data_device *device,uint32_t time,wl_fixed_t x,wl_fixed_t y) { (void)data;(void)device;(void)time;(void)x;(void)y; }
static const struct wl_data_device_listener device_listener={.data_offer=device_offer,.enter=device_enter,.leave=device_leave,.motion=device_motion,.drop=device_leave,.selection=device_selection};
static void keyboard_keymap(void *data,struct wl_keyboard *keyboard,uint32_t format,int32_t fd,uint32_t size) { (void)data;(void)keyboard;(void)format;(void)size;close(fd); }
static void keyboard_enter(void *data,struct wl_keyboard *keyboard,uint32_t serial,struct wl_surface *surface,struct wl_array *keys) {
    struct ae_clipboard *c=data;(void)keyboard;(void)keys;cancel_receive(c);
    if (c->focus_epoch==UINT64_MAX) { disable(c);return; }
    ++c->focus_epoch;c->focused=surface==c->surface;c->serial=c->focused ? serial : 0;
    if (!c->focused) drop_selection(c);
    changed(c);
}
static void keyboard_leave(void *data,struct wl_keyboard *keyboard,uint32_t serial,struct wl_surface *surface) {
    struct ae_clipboard *c=data;(void)keyboard;(void)serial;(void)surface;c->focused=false;c->serial=0;cancel_receive(c);drop_selection(c);changed(c);
}
static void keyboard_key(void *data,struct wl_keyboard *keyboard,uint32_t serial,uint32_t time,uint32_t key,uint32_t state) {
    struct ae_clipboard *c=data;(void)keyboard;(void)time;(void)key;(void)state;if (c->focused) c->serial=serial;
}
static void keyboard_modifiers(void *data,struct wl_keyboard *keyboard,uint32_t serial,uint32_t a,uint32_t b,uint32_t d,uint32_t group) { (void)data;(void)keyboard;(void)serial;(void)a;(void)b;(void)d;(void)group; }
static void keyboard_repeat(void *data,struct wl_keyboard *keyboard,int32_t rate,int32_t delay) { (void)data;(void)keyboard;(void)rate;(void)delay; }
static const struct wl_keyboard_listener keyboard_listener={.keymap=keyboard_keymap,.enter=keyboard_enter,.leave=keyboard_leave,.key=keyboard_key,.modifiers=keyboard_modifiers,.repeat_info=keyboard_repeat};
static void seat_caps(void *data,struct wl_seat *seat,uint32_t caps) {
    struct ae_clipboard *c=data;
    if ((caps&WL_SEAT_CAPABILITY_KEYBOARD) && !c->keyboard && !c->disabled) {
        c->keyboard=wl_seat_get_keyboard(seat);
        if (!c->keyboard || wl_keyboard_add_listener(c->keyboard,&keyboard_listener,c)) c->failed=true;
    } else if (!(caps&WL_SEAT_CAPABILITY_KEYBOARD) && c->keyboard) {
        wl_keyboard_release(c->keyboard);c->keyboard=NULL;c->focused=false;c->serial=0;cancel_receive(c);drop_selection(c);
    }
    changed(c);
}
static void seat_name(void *data,struct wl_seat *seat,const char *name) { (void)data;(void)seat;(void)name; }
static const struct wl_seat_listener seat_listener={.capabilities=seat_caps,.name=seat_name};
static void create_device(struct ae_clipboard *c) {
    if (c->seat && c->manager && !c->device && !c->disabled) {
        c->device=wl_data_device_manager_get_data_device(c->manager,c->seat);
        if (!c->device || wl_data_device_add_listener(c->device,&device_listener,c)) c->failed=true;
    }
}
static void registry_global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
    struct ae_clipboard *c=data;
    if (!strcmp(interface,"wl_seat")) {
        if (++c->seat_count!=1 || version<5) { disable(c);return; }
        c->seat_name=name;c->seat=wl_registry_bind(registry,name,&wl_seat_interface,5);
        if (!c->seat || wl_seat_add_listener(c->seat,&seat_listener,c)) c->failed=true;
    } else if (!strcmp(interface,"wl_data_device_manager")) {
        if (c->manager || version<3) { disable(c);return; }
        c->manager_name=name;c->manager=wl_registry_bind(registry,name,&wl_data_device_manager_interface,3);if (!c->manager) c->failed=true;
    }
    create_device(c);
}
static void registry_remove(void *data,struct wl_registry *registry,uint32_t name) {
    struct ae_clipboard *c=data;(void)registry;if (name==c->seat_name || name==c->manager_name) disable(c);
}
static const struct wl_registry_listener registry_listener={.global=registry_global,.global_remove=registry_remove};
static void sync_done(void *data,struct wl_callback *callback,uint32_t serial) {
    struct ae_clipboard *c=data;(void)serial;
    if (callback==c->initial_sync) { c->initial_sync=NULL;c->enumerated=true; }
    else if (callback==c->receive_sync) c->receive_sync=NULL;
    else c->failed=true;
    wl_callback_destroy(callback);changed(c);
}
static const struct wl_callback_listener sync_listener={.done=sync_done};
static struct wl_callback *sync_request(struct ae_clipboard *c) {
    struct wl_display *wrapper=wl_proxy_create_wrapper(c->display);if (!wrapper) return NULL;
    wl_proxy_set_queue((struct wl_proxy *)wrapper,wl_proxy_get_queue((struct wl_proxy *)c->surface));
    struct wl_callback *callback=wl_display_sync(wrapper);wl_proxy_wrapper_destroy(wrapper);
    if (callback && wl_callback_add_listener(callback,&sync_listener,c)) { wl_callback_destroy(callback);return NULL; }
    return callback;
}
int ae_clipboard_open(struct ae_clipboard **out,struct wl_display *display,struct wl_surface *surface,void (*wake)(void *),void *userdata) {
    if (!out || *out || !display || !surface || wl_proxy_get_display((struct wl_proxy *)surface)!=display) return EINVAL;
    struct ae_clipboard *c=calloc(1,sizeof(*c));if (!c) return ENOMEM;
    c->thread=pthread_self();c->display=display;c->surface=surface;c->wake=wake;c->userdata=userdata;c->epoll=-1;c->receive_fd=-1;
    for (unsigned i=0;i<SENDS;++i) c->sends[i].fd=-1;
    c->epoll=epoll_create1(EPOLL_CLOEXEC);if (c->epoll<0) { int error=errno;free(c);return error; }
    struct wl_display *wrapper=wl_proxy_create_wrapper(display);
    if (wrapper) {
        wl_proxy_set_queue((struct wl_proxy *)wrapper,wl_proxy_get_queue((struct wl_proxy *)surface));
        c->registry=wl_display_get_registry(wrapper);wl_proxy_wrapper_destroy(wrapper);
    }
    if (!c->registry || wl_registry_add_listener(c->registry,&registry_listener,c) || !(c->initial_sync=sync_request(c))) {
        (void)ae_clipboard_close(&c);return ENOMEM;
    }
    *out=c;return 0;
}
int ae_clipboard_publish(struct ae_clipboard *c,char **text,size_t length) {
    if (!owner(c) || !text || !*text || !length || length>AE_CLIPBOARD_TEXT_LIMIT || (*text)[length] || memchr(*text,0,length)) return EINVAL;
    if (!ae_clipboard_focused(c)) return EACCES;
    if (length+1>AE_CLIPBOARD_MEMORY_LIMIT-c->memory) return ENOSPC;
    unsigned i=0;while (i<SOURCES && c->sources[i].references) ++i;if (i==SOURCES) return EBUSY;
    struct source *s=&c->sources[i];s->owner=c;s->object=wl_data_device_manager_create_data_source(c->manager);
    if (!s->object || wl_data_source_add_listener(s->object,&source_listener,s)) { if (s->object) wl_data_source_destroy(s->object);*s=(struct source){0};return ENOMEM; }
    s->text=*text;s->length=length;s->references=1;c->memory+=length+1;*text=NULL;
    wl_data_source_offer(s->object,utf8_mime);wl_data_source_offer(s->object,plain_mime);
    wl_data_device_set_selection(c->device,s->object,c->serial);changed(c);return 0;
}
int ae_clipboard_receive(struct ae_clipboard *c,size_t limit) {
    if (!owner(c) || !limit || limit>AE_CLIPBOARD_TEXT_LIMIT) return EINVAL;
    if (!ae_clipboard_focused(c)) return EACCES;
    if (c->result==AE_CLIPBOARD_PENDING || c->result==AE_CLIPBOARD_DATA || c->receive_sync) return EBUSY;
    const char *mime=c->selection && c->selection->utf8 ? utf8_mime : c->selection && c->selection->plain ? plain_mime : NULL;
    if (!mime) { c->result=AE_CLIPBOARD_NO_TEXT;changed(c);return 0; }
    if (limit+1>AE_CLIPBOARD_MEMORY_LIMIT-c->memory) return ENOSPC;
    uint64_t now=now_ms();if (!now || now>UINT64_MAX-5000) return EIO;
    char *buffer=malloc(limit+1);if (!buffer) return ENOMEM;
    int pipes[2];if (pipe2(pipes,O_CLOEXEC|O_NONBLOCK)) { int error=errno;free(buffer);return error; }
    int error=add_fd(c,pipes[0],EPOLLIN,0,&c->receive_cookie);
    if (error) { close(pipes[0]);close(pipes[1]);free(buffer);return error; }
    c->receive_fd=pipes[0];c->receive_text=buffer;c->receive_limit=limit;c->receive_size=0;c->receive_deadline=now+5000;c->memory+=limit+1;c->result=AE_CLIPBOARD_PENDING;
    wl_data_offer_receive(c->selection->object,mime,pipes[1]);close(pipes[1]);
    c->receive_sync=sync_request(c);
    if (!c->receive_sync) { c->failed=true;receive_end(c,AE_CLIPBOARD_IO_ERROR);return EIO; }
    changed(c);return 0;
}
int ae_clipboard_cancel(struct ae_clipboard *c) { if (!owner(c)) return EINVAL;cancel_receive(c);return 0; }
enum ae_clipboard_result ae_clipboard_result(struct ae_clipboard *c,char **text,size_t *length) {
    if (!owner(c) || !text || !length || *text || *length) return AE_CLIPBOARD_IO_ERROR;
    enum ae_clipboard_result result=c->result;
    if (result==AE_CLIPBOARD_NONE || result==AE_CLIPBOARD_PENDING) return result;
    if (result==AE_CLIPBOARD_DATA) {
        if (!ae_clipboard_focused(c)) { receive_end(c,AE_CLIPBOARD_CANCELLED);result=AE_CLIPBOARD_CANCELLED; }
        else { *text=c->receive_text;*length=c->receive_size;c->memory-=c->receive_limit+1;c->receive_text=NULL;c->receive_size=c->receive_limit=0; }
    }
    c->result=AE_CLIPBOARD_NONE;return result;
}
int ae_clipboard_watch_fd(const struct ae_clipboard *c) { return owner(c) ? c->epoll : -1; }
int ae_clipboard_observe(const struct ae_clipboard *c,struct ae_clipboard_observation *out) {
    if (!owner(c) || !out) return EINVAL;
    struct ae_clipboard_observation observed={.bytes=c->memory,.receive=c->receive_fd>=0,.sync_pending=c->receive_sync!=NULL,.available=available(c),.focused=ae_clipboard_focused(c)};
    for (unsigned i=0;i<SOURCES;++i) if (c->sources[i].references) ++observed.sources;
    for (unsigned i=0;i<SENDS;++i) if (c->sends[i].fd>=0) ++observed.sends;
    for (unsigned i=0;i<OFFERS;++i) if (c->offers[i].object) ++observed.offers;
    *out=observed;return 0;
}
uint64_t ae_clipboard_deadline(const struct ae_clipboard *c) {
    if (!owner(c)) return 0;
    uint64_t deadline=c->receive_deadline;
    for (unsigned i=0;i<SENDS;++i) if (c->sends[i].fd>=0 && (!deadline || c->sends[i].deadline<deadline)) deadline=c->sends[i].deadline;
    return deadline;
}
int ae_clipboard_pump(struct ae_clipboard *c) {
    if (!owner(c)) return EINVAL;
    if (c->failed || wl_display_get_error(c->display)) return EIO;
    uint64_t now=now_ms();if (!now) return EIO;
    if (c->receive_fd>=0 && now>=c->receive_deadline) receive_end(c,AE_CLIPBOARD_TIMEOUT);
    for (unsigned i=0;i<SENDS;++i) if (c->sends[i].fd>=0 && now>=c->sends[i].deadline) close_send(c,i);
    struct epoll_event events[SENDS+1];int count=epoll_wait(c->epoll,events,SENDS+1,0);
    if (count<0) return errno==EINTR ? 0 : errno;
    for (int i=0;i<count;++i) {
        unsigned slot=(unsigned)(events[i].data.u64&255u);size_t budget=IO_BUDGET;
        if (!slot && c->receive_fd>=0 && events[i].data.u64==c->receive_cookie) {
            while (c->receive_fd>=0 && budget) {
                uint64_t current=now_ms();if (!current || current>=c->receive_deadline) { receive_end(c,AE_CLIPBOARD_TIMEOUT);break; }
                unsigned char bytes[4096];size_t wanted=c->receive_limit-c->receive_size+1;
                if (wanted>sizeof(bytes)) wanted=sizeof(bytes);
                if (wanted>budget) wanted=budget;
                ssize_t n=read(c->receive_fd,bytes,wanted);
                if (n<0 && errno==EINTR) continue;
                if (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) break;
                if (n<0) { receive_end(c,AE_CLIPBOARD_IO_ERROR);break; }
                current=now_ms();if (!current || current>=c->receive_deadline) { receive_end(c,AE_CLIPBOARD_TIMEOUT);break; }
                if (!n) { c->receive_text[c->receive_size]=0;receive_end(c,AE_CLIPBOARD_DATA);break; }
                if ((size_t)n>c->receive_limit-c->receive_size) { receive_end(c,AE_CLIPBOARD_LIMIT);break; }
                memcpy(c->receive_text+c->receive_size,bytes,(size_t)n);c->receive_size+=(size_t)n;budget-=(size_t)n;
            }
        } else if (slot && slot<=SENDS) {
            struct send *s=&c->sends[slot-1];
            if (s->fd<0 || events[i].data.u64!=s->cookie) continue;
            while (s->fd>=0 && budget) {
                uint64_t current=now_ms();if (!current || current>=s->deadline) { close_send(c,slot-1);break; }
                size_t remaining=s->source->length-s->position;if (remaining>budget) remaining=budget;
                ssize_t n=write_stream(s->fd,s->source->text+s->position,remaining);
                if (n<0 && errno==EINTR) continue;
                if (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) break;
                if (n<=0) { close_send(c,slot-1);break; }
                s->position+=(size_t)n;budget-=(size_t)n;
                current=now_ms();if (!current || current>=s->deadline) { close_send(c,slot-1);break; }
                if (s->position==s->source->length) close_send(c,slot-1);
            }
        }
    }
    if (wl_display_flush(c->display)<0 && errno!=EAGAIN) return errno;
    return 0;
}
int ae_clipboard_close(struct ae_clipboard **ptr) {
    if (!ptr || !*ptr) return 0;
    struct ae_clipboard *c=*ptr;if (!owner(c)) return EBUSY;
    for (unsigned i=0;i<SENDS;++i) close_send(c,i);
    if (c->receive_fd>=0 || c->receive_text) receive_end(c,AE_CLIPBOARD_CANCELLED);
    if (c->initial_sync) wl_callback_destroy(c->initial_sync);
    if (c->receive_sync) wl_callback_destroy(c->receive_sync);
    for (unsigned i=0;i<SOURCES;++i) if (c->sources[i].object) { wl_data_source_destroy(c->sources[i].object);c->sources[i].object=NULL;source_unref(&c->sources[i]); }
    for (unsigned i=0;i<OFFERS;++i) drop_offer(&c->offers[i]);
    if (c->device) wl_data_device_release(c->device);
    if (c->keyboard) wl_keyboard_release(c->keyboard);
    if (c->seat) wl_seat_release(c->seat);
    if (c->manager) wl_data_device_manager_destroy(c->manager);
    if (c->registry) wl_registry_destroy(c->registry);
    close(c->epoll);free(c);*ptr=NULL;return 0;
}
