/* Native host follows Aetheri Settings' reviewed NexaUI observation scaffold.
 * Original scaffold: NexaUI examples/21_aetheri_model at e71ee30, MIT,
 * Copyright (c) 2026 NexaUI contributors. See ../settings/NEXAUI-NOTICE.txt.
 * Files owns its controller, workers and original document lifetime. */
#define _GNU_SOURCE
#include "nexaui_view.h"
#include <nexaui/software.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

struct chrome_pointer {
    bool title_down,suppress_release,escape_down;nx_point down,current,last_click;
    double last_release;nx_cursor_shape cursor;uint32_t mode;
};
struct host {
    struct ae_files_session s;struct ae_files_view *view;nx_window *window;nx_software_view *renderer;
    thrd_t worker;bool worker_live,failed,busy,unavailable,close_seen,escape_release_pending;
    int wake[2];struct chrome_pointer pointer;float width,height,density;
};
static volatile sig_atomic_t stop_requested,signal_writer=-1;
static void signal_stop(int n) { (void)n;int saved=errno;stop_requested=1;if(signal_writer>=0) {char b=1;(void)write((int)signal_writer,&b,1);}errno=saved; }
static int worker(void *data) {
    struct host *h=data;ae_files_session_apply_worker(&h->s);char b=1;(void)write(h->wake[1],&b,1);return 0;
}
static bool start_worker(void *data,struct ae_files_session *s) {
    struct host *h=data;if(s!=&h->s || h->worker_live) return false;
    h->worker_live=thrd_create(&h->worker,worker,h)==thrd_success;return h->worker_live;
}
static void join_worker(void *data) {
    struct host *h=data;int result=-1;
    if(!h->worker_live || thrd_join(h->worker,&result)!=thrd_success || result) {
        /* Never return a successful join or free operation inputs on an
           impossible adapter state. No production recovery is implied. */
        fputs("files: original worker join failed\n",stderr);_Exit(125);
    }
    h->worker_live=false;
}
static unsigned char *load_font(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb");if(!f) return NULL;long size;unsigned char *bytes=NULL;
    if(!fseek(f,0,SEEK_END) && (size=ftell(f))>0 && size<64*1024*1024 && !fseek(f,0,SEEK_SET) &&
       (bytes=malloc((size_t)size)) && fread(bytes,1,(size_t)size,f)==(size_t)size) *length=(size_t)size;
    else {free(bytes);bytes=NULL;}
    if(fclose(f)) {free(bytes);bytes=NULL;}return bytes;
}
static bool accepted(nx_result r) { return r==NX_OK || r==NX_ERROR_STATE || r==NX_ERROR_UNSUPPORTED; }
static nx_result zoom_window(nx_window *window,const nx_window_status *status) {
    bool restore=(status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN))!=0;
    if(!(status->capabilities&(restore?NX_WINDOW_CAP_RESTORE:NX_WINDOW_CAP_MAXIMIZE))) return NX_ERROR_UNSUPPORTED;
    return restore?nx_window_restore(window):nx_window_maximize(window);
}
static float distance2(nx_point a,nx_point b) {float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
/* Same original-press rules as own Settings. Files additionally rechecks modal
   admission before a pending caption press can become a native gesture. */
static bool chrome(struct host *h,const nx_event *e,const nx_window_status *status,bool *handled) {
    *handled=false;struct chrome_pointer *p=&h->pointer;bool diagnostics=h->s.diagnostics;
    if(e->type==NX_EVENT_KEY_DOWN || (e->type==NX_EVENT_POINTER_DOWN && e->button!=1)) p->last_release=0;
    if((e->type==NX_EVENT_KEY_DOWN || e->type==NX_EVENT_KEY_UP) && e->key==NX_KEY_ESCAPE) {
        if(e->type==NX_EVENT_KEY_UP && p->escape_down) {p->escape_down=false;*handled=true;return true;}
        if(e->type==NX_EVENT_KEY_DOWN && (p->title_down || p->escape_down)) {
            p->title_down=false;p->suppress_release=true;p->last_release=0;
            if(!p->escape_down && diagnostics) {puts("CHROME_TITLE cancel");fflush(stdout);}
            p->escape_down=true;*handled=true;return true;
        }
    }
    uint32_t mode=status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN);
    if(mode!=p->mode) {if(p->title_down) p->suppress_release=true;p->title_down=false;p->last_release=0;p->mode=mode;}
    if(e->type==NX_EVENT_POINTER_CANCEL || (e->type==NX_EVENT_FOCUS && !e->focused)) {
        p->title_down=false;p->suppress_release=false;p->escape_down=false;p->last_release=0;
        if(p->cursor!=NX_CURSOR_DEFAULT) {if(!accepted(nx_window_set_cursor(h->window,NX_CURSOR_DEFAULT))) return false;p->cursor=NX_CURSOR_DEFAULT;}
        return true;
    }
    if(e->type==NX_EVENT_RESIZE) {if(p->title_down) p->suppress_release=true;p->title_down=false;p->last_release=0;return true;}
    if(p->title_down) {
        bool title;nx_window_resize_edge edge;nx_cursor_shape cursor;
        if(!ae_files_view_native_hit(h->view,p->down,status,&title,&edge,&cursor)) return false;
        if(!title) {p->title_down=false;p->suppress_release=true;p->last_release=0;}
    }
    if(e->type==NX_EVENT_POINTER_MOVE || e->type==NX_EVENT_POINTER_DOWN) {
        if(!isfinite(e->x) || !isfinite(e->y)) return false;
        p->current=(nx_point){e->x,e->y};
    }
    if(e->type==NX_EVENT_POINTER_MOVE && p->title_down) {
        *handled=true;
        if(distance2(p->current,p->down)>16) {
            p->title_down=false;p->suppress_release=true;p->last_release=0;
            nx_result result=NX_ERROR_UNSUPPORTED;
            if(!mode && (status->capabilities&NX_WINDOW_CAP_MOVE)) result=nx_window_begin_move(h->window);
            if(diagnostics) {printf("CHROME_MOVE result=%d\n",(int)result);fflush(stdout);}
            return accepted(result);
        }
        return true;
    }
    if(e->type==NX_EVENT_POINTER_MOVE && p->suppress_release) {*handled=true;return true;}
    if(e->type==NX_EVENT_POINTER_DOWN && e->button==1) {
        if(p->suppress_release || p->title_down) {*handled=true;return true;}
        bool title;nx_window_resize_edge edge;nx_cursor_shape cursor;
        if(!ae_files_view_native_hit(h->view,p->current,status,&title,&edge,&cursor)) return false;
        if(title) {p->down=p->current;p->title_down=true;*handled=true;if(diagnostics) {puts("CHROME_TITLE down");fflush(stdout);}return true;}
        p->last_release=0;
        if(edge) {
            *handled=true;p->suppress_release=true;nx_result result=nx_window_begin_resize(h->window,edge);
            if(diagnostics) {printf("CHROME_RESIZE edge=%d result=%d\n",(int)edge,(int)result);fflush(stdout);}return accepted(result);
        }
    }
    if(e->type==NX_EVENT_POINTER_UP && e->button==1) {
        if(p->suppress_release) {p->suppress_release=false;*handled=true;return true;}
        if(p->title_down) {
            p->title_down=false;*handled=true;struct timespec time;if(clock_gettime(CLOCK_MONOTONIC,&time)) return false;
            double now=(double)time.tv_sec+(double)time.tv_nsec/1e9;
            if(distance2(p->current,p->down)<=16 && p->last_release>0 && now-p->last_release<=.45 && distance2(p->current,p->last_click)<=36) {
                p->last_release=0;nx_result result=zoom_window(h->window,status);
                if(diagnostics) {printf("CHROME_ZOOM result=%d\n",(int)result);fflush(stdout);}return accepted(result);
            }
            p->last_release=now;p->last_click=p->current;if(diagnostics) {puts("CHROME_TITLE release");fflush(stdout);}return true;
        }
    }
    if(e->type==NX_EVENT_POINTER_MOVE) {
        bool title;nx_window_resize_edge edge;nx_cursor_shape cursor;
        if(!ae_files_view_native_hit(h->view,p->current,status,&title,&edge,&cursor)) return false;
        if(cursor!=p->cursor) {if(!accepted(nx_window_set_cursor(h->window,cursor))) return false;p->cursor=cursor;}
    }
    return true;
}
static bool draw(struct host *h) {
    nx_window_info info;nx_window_status status;
    if(nx_window_get_info(h->window,&info)!=NX_OK || nx_window_get_status(h->window,&status)!=NX_OK) return false;
    if(info.width!=h->width || info.height!=h->height || info.scale!=h->density) h->s.dirty=true;
    h->width=info.width;h->height=info.height;h->density=info.scale;
    if(info.should_close && !h->close_seen) {h->close_seen=true;ae_files_session_request_close(&h->s);}
    if(!h->s.dirty) return true;
    if(info.width<64 || info.height<64) {h->s.dirty=false;return true;}
    bool changed=false;h->s.dirty=false;
    nx_context *ui=ae_files_view_context(h->view);
    if(nx_context_set_text_raster_scale(ui,info.scale)!=NX_OK || !ae_files_view_frame(h->view,info.width,info.height,&status,&changed)) return false;
    if(ae_files_view_take_zoom(h->view)) {
        nx_result r=zoom_window(h->window,&status);
        if(!accepted(r)) return false;
    }
    const nx_command *commands;size_t count;nx_color background;nx_software_view_outcome outcome;
    if(nx_frame_commands(ui,&commands,&count)!=NX_OK || nx_theme_color(ui,NX_COLOR_BACKGROUND,&background)!=NX_OK ||
       nx_software_view_render(h->renderer,commands,count,background,&outcome)!=NX_OK) return false;
    h->busy=outcome==NX_SOFTWARE_VIEW_BUSY;
    if(h->busy) {h->s.dirty=true;return true;}
    if(outcome!=NX_SOFTWARE_VIEW_READY) return true;
    nx_result ax=nx_window_publish_semantics(h->window,ui);
    if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED) return false;
    h->unavailable=ax==NX_ERROR_UNSUPPORTED;
    h->s.dirty=h->s.dirty || changed;
    ++h->s.frames;
    if(h->s.diagnostics && !changed) {
        printf("VIEW count=%zu selected=%zu depth=%u grid=%d dialog=%d creating=%d unknown=%d name_bad=%d query_bad=%d german=%d info_overlay=%d scale=%.2f revision=%llu width=%.0f height=%.0f closing=%d\n",
            ae_files_count(h->s.files),ae_files_selected(h->s.files),ae_files_depth(h->s.files),h->s.grid,h->s.dialog,h->s.work_active,
            ae_files_session_unresolved(&h->s),h->s.name_bad,h->s.query_bad,h->s.german,h->s.info_overlay,(double)h->s.scale,(unsigned long long)ae_files_revision(h->s.files),info.width,info.height,h->s.closing);
        for(unsigned i=0;i<AE_FV_CONTROLS;++i) {
            uint64_t id=ae_files_view_control_id(h->view,(enum ae_files_control)i);uint32_t index;nx_semantic_view sem;
            if(id && nx_semantics_find(ui,id,&index)==NX_OK && nx_semantics_get(ui,index,&sem)==NX_OK)
                printf("CONTROL %u %llu %.2f %.2f %.2f %.2f %u\n",i,(unsigned long long)id,sem.bounds.x,sem.bounds.y,sem.bounds.width,sem.bounds.height,sem.state);
        }
        for(size_t i=h->s.first;i<ae_files_count(h->s.files) && i-h->s.first<h->s.rows;++i) {
            uint64_t id=ae_files_view_entry_id(h->view,i);uint32_t index;nx_semantic_view sem;
            if(id && nx_semantics_find(ui,id,&index)==NX_OK && nx_semantics_get(ui,index,&sem)==NX_OK)
                printf("ENTRY %zu %llu %.2f %.2f %.2f %.2f\n",i,(unsigned long long)id,sem.bounds.x,sem.bounds.y,sem.bounds.width,sem.bounds.height);
        }
        if(h->escape_release_pending && !h->s.dirty) {puts("KEY_END escape");h->escape_release_pending=false;}
        fflush(stdout);
    }
    return true;
}
int main(int argc,char **argv) {
    struct host h={.wake={-1,-1}};ae_files_session_init(&h.s,start_worker,join_worker,&h);
    const char *root=NULL,*font_path=NULL;
    for(int i=1;i<argc;++i) {
        const char **slot=!strcmp(argv[i],"--root")?&root:!strcmp(argv[i],"--font")?&font_path:!strcmp(argv[i],"--cache")?&h.s.cache:
            !strcmp(argv[i],"--terminal")?&h.s.terminal:!strcmp(argv[i],"--editor")?&h.s.editor:!strcmp(argv[i],"--terminal-font")?&h.s.terminal_font:NULL;
        if(slot && !*slot && i+1<argc) *slot=argv[++i];else if(!strcmp(argv[i],"--diagnostics") && !h.s.diagnostics) h.s.diagnostics=true;else return 2;
    }
    struct stat st;
    if(!root || !font_path || !h.s.cache || !h.s.terminal || !h.s.editor || !h.s.terminal_font || root[0]!='/' || getuid()==0 ||
       getuid()!=geteuid() || getgid()!=getegid() || lstat(h.s.cache,&st) || !S_ISDIR(st.st_mode) || st.st_uid!=getuid() || (st.st_mode&07777)!=0700) return 2;
    setvbuf(stdout,NULL,_IOLBF,0);int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);enum ae_files_result result;
    h.s.files=fd>=0?ae_files_create(fd,&result):NULL;if(fd>=0) close(fd);if(!h.s.files) return 1;
    const char *end=root+strlen(root);while(end>root+1 && end[-1]=='/') --end;const char *leaf=end;while(leaf>root && leaf[-1]!='/') --leaf;
    if(leaf==end) memcpy(h.s.root_label,"Filesystem",11);else if(!ae_files_format_name(leaf,(size_t)(end-leaf),h.s.root_label)) {ae_files_destroy(h.s.files);return 2;}
    size_t font_size=0;unsigned char *font=load_font(font_path,&font_size);
    struct sigaction stop={.sa_handler=signal_stop},child={.sa_handler=SIG_DFL};sigemptyset(&stop.sa_mask);sigemptyset(&child.sa_mask);
    if(!font || pipe2(h.wake,O_CLOEXEC|O_NONBLOCK) || sigaction(SIGINT,&stop,NULL) || sigaction(SIGTERM,&stop,NULL) || sigaction(SIGCHLD,&child,NULL) ||
       setenv("NEXAUI_PLATFORM","wayland",1)) {h.failed=true;goto done;}
    signal_writer=h.wake[1];
    nx_window_desc desc={"Aetheri Files",1000,680,NX_WINDOW_CLIENT_CHROME|NX_WINDOW_STATE_EVENTS,{0}};
    if(nx_window_create(&desc,&h.window)!=NX_OK || nx_window_set_wayland_app_id(h.window,"org.aetheri.Files")!=NX_OK ||
       !ae_files_view_create(&h.s,font,font_size,&h.view) || nx_software_view_open(h.window,&h.renderer)!=NX_OK) {h.failed=true;goto done;}
    if(h.s.diagnostics) puts("READY files nexaui software");
    while(h.s.running) {
        ae_files_session_create_update(&h.s);ae_files_session_child_update(&h.s);
        if(stop_requested) {stop_requested=0;ae_files_session_request_close(&h.s);}
        if(!h.s.running) break;
        if(!h.failed && !draw(&h)) {h.failed=true;ae_files_session_request_close(&h.s);}
        if(h.failed) {
            struct pollfd p={h.wake[0],POLLIN,0};(void)poll(&p,1,200);char b[64];while(read(h.wake[0],b,sizeof(b))>0) {}continue;
        }
        uint32_t ready=0;unsigned timeout=h.s.dirty?0:h.busy?16:h.s.work_active || ae_files_session_busy(&h.s)?200:h.unavailable?1000:UINT32_MAX;
        if(nx_window_poll_with_fd(h.window,timeout,h.wake[0],&ready)!=NX_OK || (ready&(NX_WINDOW_FD_ERROR|NX_WINDOW_FD_HANGUP))) {h.failed=true;ae_files_session_request_close(&h.s);continue;}
        if(ready) {char bytes[64];while(read(h.wake[0],bytes,sizeof(bytes))>0) {}}
        nx_event event;int available=0;
        do {
            if(nx_window_next_event(h.window,&event,&available)!=NX_OK) {h.failed=true;break;}
            if(available) {
                if(event.type==NX_EVENT_KEY_UP && event.key==NX_KEY_ESCAPE) h.escape_release_pending=true;
                nx_window_status status;bool handled=false;
                if(nx_window_get_status(h.window,&status)!=NX_OK || !chrome(&h,&event,&status,&handled) ||
                   (!handled && !ae_files_view_event(h.view,&event)) || !draw(&h)) {h.failed=true;break;}
            }
        }while(available && h.s.running);
        size_t routed=0;
        if(!h.failed && nx_window_route_events(h.window,ae_files_view_context(h.view),&routed)!=NX_OK) h.failed=true;
        if(routed) h.s.dirty=true;
        if(!h.failed && !draw(&h)) h.failed=true;
        if(h.failed) ae_files_session_request_close(&h.s);
        if(h.unavailable && !h.s.dirty) (void)nx_window_publish_semantics(h.window,ae_files_view_context(h.view));
    }
done:
    ae_files_session_join_worker(&h.s);
    ae_files_create_close(h.s.creation);ae_files_create_close(h.s.unknown);ae_files_rename_close(h.s.rename_op);ae_files_rename_close(h.s.rename_unknown);
    if(h.s.create_dir>=0) close(h.s.create_dir);
    if(h.s.rename_witness>=0) close(h.s.rename_witness);
    if(h.s.move_target>=0) close(h.s.move_target);
    ae_files_session_clipboard_clear(&h.s);signal_writer=-1;
    if(h.s.child.pid>0 && !h.s.child.ended) return 125;
    if(h.s.child.pid>0) (void)ae_files_child_close(&h.s.child);
    if(h.renderer && nx_software_view_close(&h.renderer)!=NX_OK) return 125;
    if(h.window && h.view && nx_window_publish_semantics(h.window,NULL)!=NX_OK) h.failed=true;
    ae_files_view_destroy(h.view);nx_window_destroy(h.window);
    for(unsigned i=0;i<2;++i) if(h.wake[i]>=0) close(h.wake[i]);
    free(font);ae_files_destroy(h.s.files);
    if(h.s.diagnostics) printf("CLOSED files frames=%u failed=%d\n",h.s.frames,h.failed || h.s.failed);
    return h.failed || h.s.failed?1:0;
}
