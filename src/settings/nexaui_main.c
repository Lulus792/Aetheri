/* The event-loop/Wayland observation scaffold is adapted from NexaUI's
 * examples/21_aetheri_model/main.c at e71ee304f1e6c23fa562d9e4b11f7e8e12152079.
 * Copyright (c) 2026 NexaUI contributors. MIT; see NEXAUI-NOTICE.txt.
 * Aetheri owns the presentation, model and original process lifetime.
 */
#define _POSIX_C_SOURCE 200809L
#include "settings/nexaui_view.h"
#include <nexaui/software.h>
#include <nexaui/wayland_outputs.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping=0, signal_fd=-1;
static void stop_signal(int number) {
    (void)number;
    int saved=errno; stopping=1;
    if(signal_fd>=0) {char byte=1; (void)write((int)signal_fd,&byte,1);}
    errno=saved;
}
static int signal_pipe(int fds[2]) {
    if(pipe(fds)!=0) return 0;
    for(int i=0;i<2;++i) {
        int flags=fcntl(fds[i],F_GETFL), fdflags=fcntl(fds[i],F_GETFD);
        if(flags<0 || fdflags<0 || fcntl(fds[i],F_SETFL,flags|O_NONBLOCK)<0 ||
           fcntl(fds[i],F_SETFD,fdflags|FD_CLOEXEC)<0) goto fail;
    }
    signal_fd=fds[1];
    struct sigaction action={0}; action.sa_handler=stop_signal; sigemptyset(&action.sa_mask);
    if(sigaction(SIGTERM,&action,NULL)!=0 || sigaction(SIGINT,&action,NULL)!=0) goto fail;
    return 1;
fail:
    signal_fd=-1; close(fds[0]); close(fds[1]); fds[0]=fds[1]=-1; return 0;
}

static double monotonic_seconds(void) {
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC,&t)==0 ? (double)t.tv_sec+(double)t.tv_nsec/1e9 : -1;
}

struct chrome_pointer {
    bool title_down, suppress_release, escape_down;
    nx_point down, current, last_click;
    double last_release;
    nx_cursor_shape cursor;
    uint32_t mode;
};
static bool expected_request(nx_result result) {
    return result==NX_OK || result==NX_ERROR_STATE || result==NX_ERROR_UNSUPPORTED;
}
static nx_result zoom_window(nx_window *window,const nx_window_status *status) {
    bool restore=(status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN))!=0;
    if(!(status->capabilities&(restore?NX_WINDOW_CAP_RESTORE:NX_WINDOW_CAP_MAXIMIZE))) return NX_ERROR_UNSUPPORTED;
    return restore?nx_window_restore(window):nx_window_maximize(window);
}
static float distance2(nx_point a,nx_point b) {float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
/* Only native events can begin gestures. The SDK retains their still-held press
   and serial; no caller-supplied token, retry or synthetic pointer is used. */
static bool chrome_event(nx_window *window,const struct ae_settings *model,
    const nx_window_info *info,const nx_window_status *status,const nx_event *event,
    struct chrome_pointer *pointer,bool diagnostics,bool *handled) {
    *handled=false;
    if((event->type==NX_EVENT_KEY_DOWN || event->type==NX_EVENT_KEY_UP) && event->key==NX_KEY_ESCAPE) {
        if(event->type==NX_EVENT_KEY_UP && pointer->escape_down) {
            pointer->escape_down=false;*handled=true;return true;
        }
        if(event->type==NX_EVENT_KEY_DOWN && (pointer->title_down || pointer->escape_down)) {
            pointer->title_down=false;pointer->suppress_release=true;pointer->last_release=0;
            if(!pointer->escape_down && diagnostics) {puts("CHROME_TITLE cancel");fflush(stdout);}
            pointer->escape_down=true;*handled=true;return true;
        }
    }
    uint32_t mode=status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN);
    if(mode!=pointer->mode) {
        if(pointer->title_down) pointer->suppress_release=true;
        pointer->title_down=false;pointer->last_release=0;pointer->mode=mode;
    }
    if(event->type==NX_EVENT_POINTER_CANCEL || (event->type==NX_EVENT_FOCUS && !event->focused)) {
        pointer->title_down=false;pointer->suppress_release=false;pointer->escape_down=false;pointer->last_release=0;
        if(pointer->cursor!=NX_CURSOR_DEFAULT) {
            if(!expected_request(nx_window_set_cursor(window,NX_CURSOR_DEFAULT))) return false;
            pointer->cursor=NX_CURSOR_DEFAULT;
        }
        return true;
    }
    if(event->type==NX_EVENT_RESIZE) {
        if(pointer->title_down) pointer->suppress_release=true;
        pointer->title_down=false;pointer->last_release=0;return true;
    }
    if(event->type==NX_EVENT_POINTER_MOVE || event->type==NX_EVENT_POINTER_DOWN) {
        if(!isfinite(event->x) || !isfinite(event->y)) return false;
        pointer->current=(nx_point){event->x,event->y};
    }
    if(event->type==NX_EVENT_POINTER_MOVE && pointer->title_down) {
        *handled=true;
        if(distance2(pointer->current,pointer->down)>16) {
            pointer->title_down=false;pointer->suppress_release=true;pointer->last_release=0;
            nx_result result=NX_ERROR_UNSUPPORTED;
            if(!(status->state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN)) &&
               (status->capabilities&NX_WINDOW_CAP_MOVE)) result=nx_window_begin_move(window);
            if(diagnostics) {printf("CHROME_MOVE result=%d\n",(int)result);fflush(stdout);}
            return expected_request(result);
        }
        return true;
    }
    if(event->type==NX_EVENT_POINTER_MOVE && pointer->suppress_release) {*handled=true;return true;}
    if(event->type==NX_EVENT_POINTER_DOWN && event->button==1) {
        if(pointer->suppress_release || pointer->title_down) {*handled=true;return true;}
        struct ae_settings_hit hit;
        if(!ae_settings_chrome_hit(model,info->width,info->height,status,pointer->current,&hit)) return false;
        if(hit.kind==AE_SETTINGS_HIT_TITLE) {
            pointer->down=pointer->current;pointer->title_down=true;*handled=true;
            if(diagnostics) {puts("CHROME_TITLE down");fflush(stdout);}return true;
        }
        pointer->last_release=0;
        if(hit.kind==AE_SETTINGS_HIT_RESIZE) {
            *handled=true;pointer->suppress_release=true;
            nx_result result=nx_window_begin_resize(window,hit.edge);
            if(diagnostics) {printf("CHROME_RESIZE edge=%d result=%d\n",(int)hit.edge,(int)result);fflush(stdout);}
            return expected_request(result);
        }
    }
    if(event->type==NX_EVENT_POINTER_UP && event->button==1) {
        if(pointer->suppress_release) {pointer->suppress_release=false;*handled=true;return true;}
        if(pointer->title_down) {
            pointer->title_down=false;*handled=true;
            double now=monotonic_seconds();if(now<0) return false;
            if(distance2(pointer->current,pointer->down)<=16 && pointer->last_release>0 &&
               now-pointer->last_release<=0.45 && distance2(pointer->current,pointer->last_click)<=36) {
                pointer->last_release=0;nx_result result=zoom_window(window,status);
                if(diagnostics) {printf("CHROME_ZOOM result=%d\n",(int)result);fflush(stdout);}
                return expected_request(result);
            }
            pointer->last_release=now;pointer->last_click=pointer->current;
            if(diagnostics) {puts("CHROME_TITLE release");fflush(stdout);}return true;
        }
    }
    if(event->type==NX_EVENT_POINTER_MOVE) {
        struct ae_settings_hit hit;
        if(!ae_settings_chrome_hit(model,info->width,info->height,status,pointer->current,&hit)) return false;
        if(hit.cursor!=pointer->cursor) {
            if(!expected_request(nx_window_set_cursor(window,hit.cursor))) return false;
            pointer->cursor=hit.cursor;
        }
    }
    return true;
}

static unsigned char *load_font(const char *path, size_t *length) {
    FILE *f=fopen(path,"rb"); if(!f) return NULL;
    unsigned char *data=NULL; long n;
    if(fseek(f,0,SEEK_END)==0 && (n=ftell(f))>0 && n<64*1024*1024 &&
       fseek(f,0,SEEK_SET)==0 && (data=malloc((size_t)n)) &&
       fread(data,1,(size_t)n,f)==(size_t)n) *length=(size_t)n;
    else { free(data); data=NULL; }
    if(fclose(f)!=0) {free(data);data=NULL;}
    return data;
}
/* Aetheri owns the model; the Wayland adapter provides a bounded observation,
   never a device identity or an instruction to change a display. */
static int observe_displays(nx_window *window, struct ae_settings *model,
                            uint64_t *seen, int *dirty, int diagnostics) {
    nx_wayland_output_info outputs[AE_SETTINGS_DISPLAYS]; size_t count=0; uint64_t revision=0;
    nx_result result=nx_window_get_wayland_outputs(window,outputs,AE_SETTINGS_DISPLAYS,&count,&revision);
    if(result==NX_ERROR_STATE || result==NX_ERROR_LIMIT) {
        if(model->refreshes && !model->stale) {model->stale=true;*dirty=1;}
        if(result==NX_ERROR_LIMIT && !model->stale) {model->stale=true;*dirty=1;}
        return 1;
    }
    if(result!=NX_OK) return 0;
    if(*seen==revision && !model->stale) return 1;
    struct ae_settings_display records[AE_SETTINGS_DISPLAYS]={0};
    for(size_t i=0;i<count;++i) {
        nx_wayland_output_info *source=&outputs[i]; struct ae_settings_display *target=&records[i];
        target->id=source->id; target->scale=(float)source->scale;
        target->width=source->width; target->height=source->height;
        target->refresh=(float)source->refresh_millihz/1000.0f; target->mode_known=source->mode_known!=0;
        if(source->name[0]) memcpy(target->name,source->name,sizeof(target->name));
        else if(snprintf(target->name,sizeof(target->name),"Output %u",source->id)<0) return 0;
    }
    if(!ae_settings_snapshot(model,records,count)) {model->stale=true;*dirty=1;return 1;}
    *seen=revision; *dirty=1;
    if(diagnostics) {
        printf("OUTPUTS count=%zu revision=%llu\n",count,(unsigned long long)revision);
        for(size_t i=0;i<count;++i)
            printf("OUTPUT %u %d %d %d %d %d\n",outputs[i].id,outputs[i].width,outputs[i].height,
                   outputs[i].refresh_millihz,outputs[i].scale,outputs[i].mode_known);
        fflush(stdout);
    }
    return 1;
}
int main(int argc, char **argv) {
    const char *font_path=NULL, *cache_path=NULL; int verify=0, diagnostics=0, fixture_displays=0;
    for(int i=1;i<argc;++i) {
        if(!strcmp(argv[i],"--font") && i+1<argc && !font_path) font_path=argv[++i];
        else if(!strcmp(argv[i],"--cache") && i+1<argc && !cache_path) cache_path=argv[++i];
        else if(!strcmp(argv[i],"--diagnostics") && !diagnostics) diagnostics=1;
        else if(!strcmp(argv[i],"--verify") && !verify) verify=1;
        else if(!strcmp(argv[i],"--fixture-displays") && !fixture_displays) fixture_displays=1;
        else if(argv[i][0]!='-' && !font_path) font_path=argv[i];
        else return 2;
    }
    if(!font_path || (cache_path && !*cache_path)) return 2;
    int fds[2]={-1,-1}; if(!signal_pipe(fds)) return 1;
    int okay=1, dirty=1, unavailable=0, busy=0;
    double next_ax_retry=0;
    uint64_t outputs_seen=UINT64_MAX;
    nx_window_info previous={0};
    struct chrome_pointer pointer={0};
    size_t length=0; unsigned char *font=load_font(font_path,&length);
    struct ae_settings model; ae_settings_init(&model);
    nx_window *window=NULL; nx_software_view *renderer=NULL; struct ae_settings_view *view=NULL;
    nx_window_desc desc={"Aetheri Settings",1000,720,NX_WINDOW_CLIENT_CHROME|NX_WINDOW_STATE_EVENTS,{0}};
    if(fixture_displays) {
        const struct ae_settings_display sample[2]={
            {.id=42,.name="Monitor A",.width=1920,.height=1080,.scale=1,.refresh=60,.mode_known=1},
            {.id=91,.name="Monitor B",.scale=1,.refresh=0,.mode_known=0}};
        if(!ae_settings_snapshot(&model,sample,2)) {okay=0;goto done;}
    }
    if(!font || nx_window_create(&desc,&window)!=NX_OK ||
       nx_window_set_wayland_app_id(window,"org.aetheri.Settings")!=NX_OK ||
       !ae_settings_view_create(&model,font,length,&view) || nx_software_view_open(window,&renderer)!=NX_OK) {
        okay=0; goto done;
    }
    while(okay && !stopping) {
        nx_window_info info; nx_event event; int available=0, quit=0;
        uint32_t signal_events=0;
        if(nx_window_poll_with_fd(window,busy?16:dirty?0:unavailable?1000:UINT32_MAX,fds[0],&signal_events)!=NX_OK ||
           nx_window_get_info(window,&info)!=NX_OK) {okay=0;break;}
        if(signal_events & (NX_WINDOW_FD_ERROR|NX_WINDOW_FD_HANGUP)) {okay=0;break;}
        if(stopping) break;
        if(info.should_close) break;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK) {okay=0;break;}
            if(available) {
                nx_window_status event_status;bool chrome_handled=false;
                if(nx_window_get_status(window,&event_status)!=NX_OK ||
                   !chrome_event(window,&model,&info,&event_status,&event,&pointer,diagnostics,&chrome_handled)) {okay=0;break;}
                if(!chrome_handled && event.type==NX_EVENT_KEY_DOWN && (event.key==NX_KEY_ESCAPE ||
                   (event.key==NX_KEY_Q && event.modifiers==NX_MOD_CONTROL))) quit=1;
                else if(event.type==NX_EVENT_KEY_DOWN && event.key==NX_KEY_F5) {outputs_seen=UINT64_MAX;dirty=1;}
                else if(!chrome_handled && nx_context_queue_event(ae_settings_view_context(view),&event)!=NX_OK) {okay=0;break;}
                dirty=1;
            }
        } while(available);
        /* Route native accessibility requests too; next_event only drains the
           ordinary window queue and does not transfer AT-SPI actions. */
        size_t routed=0;
        if(okay && nx_window_route_events(window,ae_settings_view_context(view),&routed)!=NX_OK) okay=0;
        if(routed) dirty=1;
        if(!okay || quit) break;
        if(!fixture_displays && !observe_displays(window,&model,&outputs_seen,&dirty,diagnostics)) {okay=0;break;}
        if(info.width!=previous.width || info.height!=previous.height || info.scale!=previous.scale) dirty=1;
        previous=info;
        if(!dirty) {
            if(unavailable) {
                double now=monotonic_seconds(); if(now<0) {okay=0;break;}
                if(now>=next_ax_retry) {
                    nx_result ax=nx_window_publish_semantics(window,ae_settings_view_context(view));
                    if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED) {okay=0;break;}
                    unavailable=ax==NX_ERROR_UNSUPPORTED; next_ax_retry=now+1;
                }
            }
            continue;
        }
        if(info.width<64 || info.height<64) {dirty=0;busy=0;continue;}
        bool changed=false; nx_window_status status;
        if(nx_window_get_status(window,&status)!=NX_OK) {okay=0;break;}
        if(nx_context_set_text_raster_scale(ae_settings_view_context(view),info.scale)!=NX_OK ||
           !ae_settings_view_frame(view,info.width,info.height,&status,&changed)) {okay=0;break;}
        enum ae_settings_window_action action=ae_settings_view_take_action(view);
        if(action==AE_SETTINGS_WINDOW_CLOSE) break;
        nx_result request=NX_OK;
        if(action==AE_SETTINGS_WINDOW_MINIMIZE) request=nx_window_minimize(window);
        if(action==AE_SETTINGS_WINDOW_ZOOM) request=zoom_window(window,&status);
        if(request!=NX_OK && request!=NX_ERROR_UNSUPPORTED && request!=NX_ERROR_STATE) {okay=0;break;}
        if(ae_settings_view_take_refresh(view)) {outputs_seen=UINT64_MAX;changed=true;}
        const nx_command *commands=NULL; size_t count=0; nx_color background;
        nx_software_view_outcome outcome;
        if(nx_frame_commands(ae_settings_view_context(view),&commands,&count)!=NX_OK ||
           nx_theme_color(ae_settings_view_context(view),NX_COLOR_BACKGROUND,&background)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,background,&outcome)!=NX_OK) {okay=0;break;}
        busy=outcome==NX_SOFTWARE_VIEW_BUSY;
        if(busy) {dirty=1;continue;}
        if(outcome!=NX_SOFTWARE_VIEW_READY) {dirty=0;continue;}
        nx_result ax=nx_window_publish_semantics(window,ae_settings_view_context(view));
        if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED) {okay=0;break;}
        unavailable=ax==NX_ERROR_UNSUPPORTED;
        if(unavailable) {
            double now=monotonic_seconds(); if(now<0) {okay=0;break;}
            next_ax_retry=now+1;
        }
        if(diagnostics) {printf("FRAME %.0f %.0f %.2f\n",info.width,info.height,info.scale);
            printf("FOCUS %llu\n",(unsigned long long)nx_focus_id(ae_settings_view_context(view)));
            size_t nodes=0;
            if(nx_semantics_count(ae_settings_view_context(view),&nodes)!=NX_OK) {okay=0;break;}
            for(size_t i=0;i<nodes;++i) {
                nx_semantic_view item;
                if(nx_semantics_get(ae_settings_view_context(view),(uint32_t)i,&item)!=NX_OK) {okay=0;break;}
                if(item.role==NX_ROLE_BUTTON || item.role==NX_ROLE_RADIO || item.role==NX_ROLE_SLIDER || item.role==NX_ROLE_SWITCH)
                    printf("CONTROL %llu %.2f %.2f %.2f %.2f %u\n",(unsigned long long)item.id,
                        item.bounds.x,item.bounds.y,item.bounds.width,item.bounds.height,item.state);
                if(item.id==25) printf("FOOTER %.2f %.2f\n",item.bounds.y,item.bounds.height);
            }
            fflush(stdout);}
        if(verify || diagnostics) {printf("MODEL theme=%d german=%d text=%u page=%d\n",(int)model.theme,model.german,model.text_percent,(int)model.page);fflush(stdout);}
        if(diagnostics && !changed) {
            printf("SETTLED theme=%d german=%d text=%u page=%d %.0f %.0f\n",
                (int)model.theme,model.german,model.text_percent,(int)model.page,info.width,info.height);
            fflush(stdout);
        }
        if(diagnostics && !fixture_displays) {
            printf("SELECTED_OUTPUT %u\n",model.count?model.displays[model.selected].id:0);fflush(stdout);
        }
        if(fixture_displays && (verify || diagnostics)) {
            printf("FIXTURE_DISPLAYS count=%zu selected=%u stale=%d\n",model.count,
                   model.count?model.displays[model.selected].id:0,model.stale);fflush(stdout);
        }
        dirty=changed?1:0;
    }
done:
    if(renderer && nx_software_view_close(&renderer)!=NX_OK) okay=0;
    if(window && view && nx_window_publish_semantics(window,NULL)!=NX_OK) okay=0;
    ae_settings_view_destroy(view); nx_window_destroy(window); free(font);
    signal_fd=-1; close(fds[0]); close(fds[1]);
    if(stopping && diagnostics) {puts("SHUTDOWN signal");fflush(stdout);}
    if(!okay) fputs("Aetheri Settings view failed\n",stderr);
    return okay?0:1;
}
