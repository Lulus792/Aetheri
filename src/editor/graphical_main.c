/* Native observation loop follows Aetheri Settings' reviewed NexaUI scaffold.
 * Original scaffold: NexaUI examples/21_aetheri_model, MIT, Copyright (c) 2026
 * NexaUI contributors. See ../settings/NEXAUI-NOTICE.txt. All editor state,
 * presentation and local document effects are owned by Aetheri. */
#define _GNU_SOURCE
#include "editor/workspace.h"
#include "files/model.h"
#include <nexaui/nexaui.h>
#include <nexaui/software.h>
#include <nexaui/wayland_clipboard.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum { ROWS=512,LAYOUTS=2048,CACHE=1024*1024 };
enum mode { EDITING,PICK_OPEN,PICK_SAVE };
enum action { NONE,NEW,OPEN,SAVE,CLOSE_TAB,CLOSE_WINDOW,CANCEL,DISCARD,CONFIRM,UP,CHOOSE,SMALLER,LARGER,ZOOM,SELECT_ROW,SELECT_TAB };
struct ids { uint64_t document,tab,close,canvas,text,sidebar; };
struct painted_row { size_t start,size;nx_text_layout *layout;nx_rect bounds; };
struct line {size_t start,size;};
struct paste {uint64_t native,document,canvas;struct ae_direct_token input;};
struct app {
    struct ae_editor_workspace *workspace;struct ae_files *picker;int root;
    nx_context *context;nx_font *font,*mono_font;nx_font_collection *fonts,*mono_fonts;nx_window *window;nx_software_view *renderer;
    nx_text_field *name;nx_text_layout *layouts[LAYOUTS];unsigned layout_count;
    struct ids ids[AE_EDITOR_TABS];uint64_t next_id,modal_id,field_id,row_ids[AE_FILES_ENTRIES],picker_revision,review_serial;
    struct ae_editor_close_token review;struct ae_editor_destination_token destination;
    struct painted_row rows[ROWS];unsigned row_count;nx_rect canvas;
    struct line *lines;size_t line_count,line_capacity,top,first_entry;
    char *bytes;size_t byte_size,byte_capacity;uint64_t document,revision;struct ae_document_info info;
    struct ae_direct_selection selection;float width,height,density,scale,left,em,line_height;
    enum mode mode;bool german,contrast,light,diagnostics,dirty,running,close_all,focused,focus_document,focus_modal,dragging,ensure_caret;
    unsigned text_percent;uint64_t dragging_document;char message[256],root_label[AE_FILES_LABEL+1];
    uint64_t action_document;enum action action;
    nx_rect picker_list,review_title;float review_scroll,review_max_scroll;
    bool overlay_shown,close_seen;uint64_t clicked_row;double clicked_at;
    struct paste paste;
};
static volatile sig_atomic_t stopping,signal_fd=-1;
static void stop_signal(int number) { (void)number;int saved=errno;stopping=1;if(signal_fd>=0) {char byte=1;(void)write((int)signal_fd,&byte,1);}errno=saved; }
static bool signal_pipe(int fds[2]) {
    if(pipe2(fds,O_CLOEXEC|O_NONBLOCK)) return false;
    signal_fd=fds[1];struct sigaction action={0};action.sa_handler=stop_signal;sigemptyset(&action.sa_mask);
    return sigaction(SIGTERM,&action,NULL)==0 && sigaction(SIGINT,&action,NULL)==0;
}
static const char *tr(const struct app *a,const char *en,const char *de) { return a->german?de:en; }
static nx_utf8_span span(const char *s) { return (nx_utf8_span){s,strlen(s)}; }
static nx_color rgb(unsigned n) { return (nx_color){(float)((n>>16)&255u)/255,(float)((n>>8)&255u)/255,(float)(n&255u)/255,1}; }
static nx_color foreground(const struct app *a) {return rgb(a->contrast?0xffffff:a->light?0x20232c:0xe8eaf0);}
static nx_color background(const struct app *a) {return rgb(a->contrast?0:a->light?0xf5f6f8:0x20232c);}
static nx_color secondary(const struct app *a) {return rgb(a->contrast?0xffffff:a->light?0x4c5564:0xb9c1ce);}
static nx_color surface(const struct app *a) {return rgb(a->contrast?0:a->light?0xe5e8ee:0x282c36);}
static nx_color selected(const struct app *a) {return rgb(a->contrast?0xffe240:a->light?0xc8d6ea:0x3b4c66);}
static nx_color focus_color(const struct app *a) {return rgb(a->contrast?0xffe240:a->light?0x315a96:0xc6d8ef);}
static bool contains(nx_rect r,float x,float y) {return x>=r.x && x<r.x+r.width && y>=r.y && y<r.y+r.height;}
static uint64_t identity(struct app *a) { if(a->next_id==UINT64_MAX) return 0;return ++a->next_id; }
static bool modal_identity(struct app *a) {
    a->modal_id=identity(a);for(unsigned i=0;i<15;++i) if(!identity(a)) return false;
    a->field_id=a->modal_id+12;return a->modal_id!=0;
}
static void close_overlay(struct app *a) {
    if(a->overlay_shown) (void)nx_overlay_close(a->context);
    a->overlay_shown=false;a->review_serial=0;
}
static struct ids *tab_ids(struct app *a,uint64_t document) {
    for(unsigned i=0;i<AE_EDITOR_TABS;++i) if(a->ids[i].document==document) return &a->ids[i];
    for(unsigned i=0;i<AE_EDITOR_TABS;++i) if(!a->ids[i].document) {
        a->ids[i]=(struct ids){document,identity(a),identity(a),identity(a),identity(a),identity(a)};return a->ids[i].sidebar?&a->ids[i]:NULL;
    }
    return NULL;
}
static void message(struct app *a,enum ae_editor_workspace_result result) {
    const char *text="";
    switch(result) {
    case AE_EDITOR_WORKSPACE_OK:break;
    case AE_EDITOR_WORKSPACE_CONFLICT:text=tr(a,"The file changed. Your document is still open.","Die Datei wurde geändert. Dein Dokument bleibt geöffnet.");break;
    case AE_EDITOR_WORKSPACE_UNKNOWN:text=tr(a,"The save could not be confirmed. Keep this document open.","Das Speichern konnte nicht bestätigt werden. Lass dieses Dokument geöffnet.");break;
    case AE_EDITOR_WORKSPACE_LIMIT:text=tr(a,"Close a document before opening another.","Schließe ein Dokument, bevor du ein weiteres öffnest.");break;
    case AE_EDITOR_WORKSPACE_NOMEM:text=tr(a,"Not enough memory to finish this action.","Nicht genug Arbeitsspeicher für diese Aktion.");break;
    case AE_EDITOR_WORKSPACE_UNSUPPORTED:text=tr(a,"This file cannot be edited here.","Diese Datei kann hier nicht bearbeitet werden.");break;
    default:text=tr(a,"The action could not be completed. Your document is still open.","Die Aktion konnte nicht abgeschlossen werden. Dein Dokument bleibt geöffnet.");break;
    }
    snprintf(a->message,sizeof(a->message),"%s",text);a->dirty=true;
    if(a->diagnostics) {printf("RESULT %d\n",(int)result);fflush(stdout);}
}
static void cancel_paste(struct app *a) {
    uint64_t token=a->paste.native;a->paste=(struct paste){0};
    if(token) {
        nx_result result=nx_wayland_clipboard_read_cancel(a->window,token);
        if(a->diagnostics) {printf("PASTE cancel=%d\n",(int)result);fflush(stdout);}
        a->dirty=true;
    }
}
static void cancel_input(struct app *a) {
    cancel_paste(a);
    struct ae_direct *input=ae_editor_workspace_input(a->workspace);if(input) (void)ae_direct_cancel_input(input);a->dragging=false;
}
static bool sync_document(struct app *a) {
    uint64_t current=ae_editor_workspace_active(a->workspace);struct ae_editor_tab tab;
    for(unsigned i=0;i<AE_EDITOR_TABS;++i) if(a->ids[i].document && !ae_editor_workspace_get(a->workspace,a->ids[i].document,&tab)) a->ids[i]=(struct ids){0};
    if(!current) {a->document=0;a->row_count=0;a->line_count=0;return true;}
    if(!ae_editor_workspace_get(a->workspace,current,&tab) || !ae_document_info(tab.document,&a->info)) return false;
    if(current!=a->document || a->revision!=a->info.revision) {
        if(a->info.size>a->byte_capacity) {
            char *bytes=realloc(a->bytes,a->info.size);if(!bytes) return false;a->bytes=bytes;a->byte_capacity=a->info.size;
        }
        if(a->info.lines>a->line_capacity) {
            struct line *lines=realloc(a->lines,a->info.lines*sizeof(*lines));if(!lines) return false;a->lines=lines;a->line_capacity=a->info.lines;
        }
        if(!ae_document_copy(tab.document,0,a->info.size,a->bytes,a->byte_capacity)) return false;
        for(size_t i=0;i<a->info.lines;++i) {struct ae_document_line line;if(!ae_document_line(tab.document,i,&line)) return false;a->lines[i]=(struct line){line.start,line.size};}
        if(current!=a->document) {a->top=0;a->left=0;}
        a->byte_size=a->info.size;a->line_count=a->info.lines;a->document=current;a->revision=a->info.revision;
    }
    if(!ae_editor_workspace_selection(a->workspace,current,&a->selection)) return false;
    return true;
}
static void close_picker(struct app *a) {
    close_overlay(a);
    if(a->picker) ae_files_destroy(a->picker);
    a->picker=NULL;
    a->mode=EDITING;a->picker_revision=0;a->focus_document=true;a->dirty=true;
}
static bool begin_picker(struct app *a,bool saving) {
    cancel_input(a);
    if(saving) {
        enum ae_editor_workspace_result result=ae_editor_workspace_destination(a->workspace,ae_editor_workspace_active(a->workspace),&a->destination);
        if(result!=AE_EDITOR_WORKSPACE_OK) {message(a,result);return true;}
    }
    enum ae_files_result result;a->picker=ae_files_create(a->root,&result);
    if(!a->picker || (saving && (a->name?nx_text_field_set_text(a->name,"",0):nx_text_field_create("",0,16384,&a->name))!=NX_OK)) {close_picker(a);message(a,AE_EDITOR_WORKSPACE_IO);return true;}
    close_overlay(a);if(!modal_identity(a)) return false;
    a->mode=saving?PICK_SAVE:PICK_OPEN;a->first_entry=0;a->picker_revision=0;
    a->focus_modal=true;a->message[0]=0;return a->field_id!=0;
}
static bool advance_close(struct app *a) {
    while(a->close_all && ae_editor_workspace_count(a->workspace)) {
        uint64_t id=ae_editor_workspace_active(a->workspace);
        enum ae_editor_workspace_result result=ae_editor_workspace_request_close(a->workspace,id,&a->review);
        if(result==AE_EDITOR_WORKSPACE_REVIEW) {a->focus_modal=true;return true;}
        if(result!=AE_EDITOR_WORKSPACE_OK) {message(a,result);a->close_all=false;return true;}
    }
    if(a->close_all) a->running=false;
    return true;
}
static bool choose_file(struct app *a) {
    enum ae_editor_workspace_result result;
    if(a->mode==PICK_SAVE) {
        nx_utf8_span name;if(nx_text_field_text(a->name,&name)!=NX_OK) return false;
        if(!name.length || name.length>AE_FILES_NAME || memchr(name.data,'/',name.length) || memchr(name.data,0,name.length)) {message(a,AE_EDITOR_WORKSPACE_INVALID);return true;}
        char leaf[AE_FILES_NAME+1];memcpy(leaf,name.data,name.length);leaf[name.length]=0;
        int directory=ae_files_directory(a->picker);if(directory<0) {message(a,AE_EDITOR_WORKSPACE_IO);return true;}
        result=ae_editor_workspace_save_new(a->workspace,&a->destination,directory,leaf);close(directory);
    } else {
        const struct ae_files_entry *entry=ae_files_entry(a->picker,ae_files_selected(a->picker));
        if(entry && entry->kind==AE_FILES_DIRECTORY) {
            if(ae_files_enter(a->picker)!=AE_FILES_OK) message(a,AE_EDITOR_WORKSPACE_IO);else {a->first_entry=0;a->message[0]=0;}
            return true;
        }
        int directory=-1,witness=-1;char leaf[AE_FILES_NAME+1];uint64_t id;
        if(ae_files_open_selected(a->picker,&directory,&witness,leaf)!=AE_FILES_OK) {message(a,AE_EDITOR_WORKSPACE_IO);return true;}
        result=ae_editor_workspace_open_selected(a->workspace,directory,leaf,witness,&id);close(directory);close(witness);
    }
    message(a,result);
    if(result==AE_EDITOR_WORKSPACE_OK || result==AE_EDITOR_WORKSPACE_UNKNOWN) {
        bool saved=a->mode==PICK_SAVE;close_picker(a);
        if(saved && result==AE_EDITOR_WORKSPACE_OK && ae_editor_workspace_review(a->workspace,&a->review)) {
            result=ae_editor_workspace_resolve_close(a->workspace,&a->review,AE_EDITOR_CLOSE_SAVE);message(a,result);if(result==AE_EDITOR_WORKSPACE_OK) return advance_close(a);
        }
    }
    a->ensure_caret=true;return true;
}
static bool apply(struct app *a,enum action action,uint64_t document) {
    if(a->mode==EDITING && (action==NEW || action==OPEN || action==SAVE || action==CLOSE_TAB ||
       action==CLOSE_WINDOW || action==SELECT_TAB || action==SMALLER || action==LARGER || action==ZOOM)) cancel_input(a);
    uint64_t id;enum ae_editor_workspace_result result=AE_EDITOR_WORKSPACE_OK;
    if(action==NEW) {result=ae_editor_workspace_new(a->workspace,&id);a->focus_document=true;}
    else if(action==OPEN) return begin_picker(a,false);
    else if(action==SAVE) {
        result=ae_editor_workspace_save(a->workspace,ae_editor_workspace_active(a->workspace));
        if(result==AE_EDITOR_WORKSPACE_NEEDS_PATH) return begin_picker(a,true);
    } else if(action==CLOSE_TAB) {result=ae_editor_workspace_request_close(a->workspace,document,&a->review);if(result==AE_EDITOR_WORKSPACE_REVIEW) {a->focus_modal=true;return true;}a->focus_document=true;}
    else if(action==CLOSE_WINDOW) {a->close_all=true;return advance_close(a);}
    else if(action==CANCEL) {
        if(a->mode!=EDITING) close_picker(a);
        else if(ae_editor_workspace_review(a->workspace,&a->review)) result=ae_editor_workspace_resolve_close(a->workspace,&a->review,AE_EDITOR_CLOSE_CANCEL);
        close_overlay(a);a->close_all=false;a->focus_document=true;
    } else if(action==DISCARD || action==CONFIRM) {
        result=ae_editor_workspace_resolve_close(a->workspace,&a->review,action==DISCARD?AE_EDITOR_CLOSE_DISCARD:AE_EDITOR_CLOSE_SAVE);
        if(result==AE_EDITOR_WORKSPACE_NEEDS_PATH) return begin_picker(a,true);
        if(result==AE_EDITOR_WORKSPACE_OK) {close_overlay(a);a->focus_document=true;return advance_close(a);}
    } else if(action==CHOOSE) return choose_file(a);
    else if(action==UP) {if(ae_files_up(a->picker)!=AE_FILES_OK) result=AE_EDITOR_WORKSPACE_IO;else a->first_entry=0;}
    else if(action==SMALLER || action==LARGER) {
        cancel_input(a);if(action==LARGER && a->text_percent<200) a->text_percent+=50;
        if(action==SMALLER && a->text_percent>100) a->text_percent-=50;
    } else if(action==ZOOM) {
        nx_window_status status;if(nx_window_get_status(a->window,&status)!=NX_OK) return false;
        nx_result r=(status.state&NX_WINDOW_STATE_MAXIMIZED)?nx_window_restore(a->window):nx_window_maximize(a->window);
        if(r!=NX_OK && r!=NX_ERROR_STATE && r!=NX_ERROR_UNSUPPORTED) return false;
    }
    if(action==SELECT_TAB) {
        cancel_input(a);result=ae_editor_workspace_select(a->workspace,document);a->focus_document=true;
    } else if(action==SELECT_ROW) {
        size_t index=(size_t)document;if(!ae_files_select(a->picker,index)) return false;
        const struct ae_files_entry *entry=ae_files_entry(a->picker,index);if(!entry) return false;
        struct timespec clock;if(clock_gettime(CLOCK_MONOTONIC,&clock)) return false;
        double now=(double)clock.tv_sec+(double)clock.tv_nsec/1e9;
        bool twice=a->clicked_row==a->row_ids[index] && now-a->clicked_at<.4;
        a->clicked_row=a->row_ids[index];a->clicked_at=now;
        if(entry->kind==AE_FILES_DIRECTORY && twice) {
            if(ae_files_enter(a->picker)!=AE_FILES_OK) result=AE_EDITOR_WORKSPACE_IO;else a->first_entry=0;
        } else if(a->mode==PICK_OPEN && twice) return choose_file(a);
        else if(a->mode==PICK_SAVE && entry->kind==AE_FILES_REGULAR && nx_text_field_set_text(a->name,entry->name,strlen(entry->name))!=NX_OK) result=AE_EDITOR_WORKSPACE_UNSUPPORTED;
    }
    message(a,result);a->ensure_caret=true;return true;
}
static nx_text_layout *layout_fonts(struct app *a,nx_font_collection *fonts,const char *text,size_t size,float em,float width) {
    if(a->layout_count==LAYOUTS) return NULL;
    nx_text_layout *value=NULL;
    if(nx_text_layout_create(fonts,text,size,NX_BIDI_LTR,em,width,&value)!=NX_OK) return NULL;
    a->layouts[a->layout_count++]=value;return value;
}
static nx_text_layout *layout(struct app *a,const char *text,size_t size,float em,float width) {return layout_fonts(a,a->fonts,text,size,em,width);}
static bool label(struct app *a,const char *text,nx_rect bounds,float em,nx_color color) {
    nx_text_layout *value=layout(a,text,strlen(text),em,10000000);if(!value) return false;
    return nx_push_clip(a->context,bounds)==NX_OK &&
        nx_draw_text_layout(a->context,value,(nx_point){bounds.x,bounds.y+(bounds.height-(float)nx_text_layout_height(value))/2},color,CACHE)==NX_OK && nx_pop_clip(a->context)==NX_OK;
}
static bool wrapped(struct app *a,uint64_t id,uint64_t parent,const char *text,nx_rect bounds,float em,nx_color color) {
    nx_text_layout *value=layout(a,text,strlen(text),em,bounds.width);if(!value) return false;
    nx_semantic_desc description={.id=id,.parent_id=parent,.role=NX_ROLE_TEXT,.bounds=bounds,.name=span(text)};
    return nx_semantics_add(a->context,&description,NULL)==NX_OK && nx_push_clip(a->context,bounds)==NX_OK &&
        nx_draw_text_layout(a->context,value,(nx_point){bounds.x,bounds.y},color,CACHE)==NX_OK && nx_pop_clip(a->context)==NX_OK;
}
static bool button(struct app *a,uint64_t id,uint64_t parent,nx_rect rect,const char *text,const char *name,
    bool disabled,bool chosen,enum action action,uint64_t document) {
    nx_widget_desc widget={.semantic={.id=id,.parent_id=parent,.role=action==SELECT_TAB && parent==5?NX_ROLE_TAB:NX_ROLE_BUTTON,.bounds=rect,.name=span(name),
        .state=(disabled?NX_SEM_DISABLED:0u)|(chosen?NX_SEM_SELECTED:0u),.actions=NX_ACTION_PRESS|NX_ACTION_FOCUS},.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE};
    nx_interaction_state interaction;if(nx_widget_register(a->context,&widget,&interaction)!=NX_OK) return false;
    nx_color color=chosen || interaction.held?selected(a):surface(a);
    if(nx_fill_round_rect(a->context,rect,5,color)!=NX_OK ||
       !label(a,text,(nx_rect){rect.x+8,rect.y,rect.width-16,rect.height},13*a->scale,(chosen || interaction.held) && a->contrast?rgb(0):disabled?secondary(a):foreground(a))) return false;
    if(interaction.focused && nx_stroke_rect(a->context,rect,2,focus_color(a))!=NX_OK) return false;
    if(interaction.clicked && !disabled && a->action==NONE) {a->action=action;a->action_document=document;}
    if(a->diagnostics) printf("CONTROL %llu %.2f %.2f %.2f %.2f %s\n",(unsigned long long)id,rect.x,rect.y,rect.width,rect.height,name);
    return true;
}
static bool register_document(struct app *a,struct ids *ids) {
    bool blocked=a->mode!=EDITING || a->review.id!=0;
    nx_widget_desc widget={.semantic={.id=ids->canvas,.parent_id=1,.role=NX_ROLE_CUSTOM,.bounds=a->canvas,
        .name=span(tr(a,"Edit document","Dokument bearbeiten")),.state=blocked?NX_SEM_DISABLED:0,.actions=NX_ACTION_FOCUS},
        .flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE|NX_WIDGET_SCROLLABLE};
    nx_interaction_state interaction;
    if(nx_widget_register(a->context,&widget,&interaction)!=NX_OK) return false;
    if(a->focus_document && !blocked) {if(nx_focus_set(a->context,ids->canvas)!=NX_OK) return false;a->focus_document=false;}
    if(a->byte_size<=NX_TEXT_SNAPSHOT_MAX_BYTES) {
        nx_semantic_desc content={.id=ids->text,.parent_id=ids->canvas,.role=NX_ROLE_TEXT,.bounds=a->canvas,
            .name=span(tr(a,"Document content","Dokumentinhalt")),.value={a->bytes?a->bytes:"",a->byte_size},.state=NX_SEM_READONLY};
        size_t anchor=a->selection.active?(a->info.cursor==a->selection.begin?a->selection.end:a->selection.begin):a->info.cursor;
        if(nx_semantics_add_readonly_text_snapshot(a->context,&content,ids->document,anchor,a->info.cursor,NULL)!=NX_OK) return false;
    }
    return true;
}
static bool paint_document(struct app *a) {
    a->row_count=0;if(!a->document) return true;
    struct ids *ids=tab_ids(a,a->document);if(!ids || !register_document(a,ids)) return false;
    size_t caret_row=0;while(caret_row+1<a->line_count && a->lines[caret_row+1].start<=a->info.cursor) ++caret_row;
    size_t visible=(size_t)fmaxf(1,floorf(a->canvas.height/a->line_height));if(visible>ROWS) visible=ROWS;
    if(a->ensure_caret) {
        if(caret_row<a->top) a->top=caret_row;
        if(caret_row>=a->top+visible) a->top=caret_row-visible+1;
    }
    if(a->top>=a->line_count) a->top=a->line_count-1;
    float gutter=54*a->scale,text_x=a->canvas.x+gutter+12*a->scale;
    for(size_t row=a->top;row<a->line_count && a->row_count<visible;++row) {
        struct line line=a->lines[row];nx_text_layout *value=layout_fonts(a,a->mono_fonts,a->bytes?a->bytes+line.start:"",line.size,a->em,10000000);if(!value) return false;
        float y=a->canvas.y+(float)(row-a->top)*a->line_height;
        a->rows[a->row_count++]=(struct painted_row){line.start,line.size,value,{text_x,y,a->canvas.width-gutter-24*a->scale,a->line_height}};
        if(row==caret_row && a->ensure_caret) {
            for(size_t i=0;i<nx_text_layout_caret_count(value);++i) {
                nx_text_caret caret;if(nx_text_layout_caret_at(value,i,&caret)!=NX_OK) return false;
                if(caret.byte_offset==a->info.cursor-line.start) {
                    float x=(float)caret.x;
                    if(x<a->left) a->left=x;
                    if(x>a->left+a->rows[a->row_count-1].bounds.width-12) a->left=fmaxf(0,x-a->rows[a->row_count-1].bounds.width+12);
                    break;
                }
            }
        }
    }
    if(nx_push_clip(a->context,a->canvas)!=NX_OK) return false;
    for(unsigned n=0;n<a->row_count;++n) {
        struct painted_row *row=&a->rows[n];char number[32];snprintf(number,sizeof(number),"%zu",a->top+n+1);
        if(!label(a,number,(nx_rect){a->canvas.x+8,row->bounds.y,gutter-12*a->scale,a->line_height},a->em,secondary(a))) return false;
        nx_point origin={row->bounds.x-a->left,row->bounds.y};size_t count=0;
        nx_text_selection_rect *rectangles=NULL;
        size_t begin=a->selection.begin>row->start?a->selection.begin-row->start:0;
        size_t end=a->selection.end>row->start?a->selection.end-row->start:0;if(end>row->size) end=row->size;
        if(a->selection.active && begin<end && begin<=row->size) {
            nx_result query=nx_text_layout_selection(row->layout,begin,end,NULL,0,&count);
            if(query!=NX_OK && query!=NX_ERROR_LIMIT) return false;
            if(count>SIZE_MAX/sizeof(*rectangles)) return false;
            if(count) {rectangles=malloc(count*sizeof(*rectangles));if(!rectangles || nx_text_layout_selection(row->layout,begin,end,rectangles,count,&count)!=NX_OK) {free(rectangles);return false;}}
        }
        nx_rect clip={row->bounds.x,row->bounds.y,row->bounds.width,row->bounds.height};
        if(nx_push_clip(a->context,clip)!=NX_OK) {free(rectangles);return false;}
        for(size_t i=0;i<count;++i) if(nx_fill_rect(a->context,(nx_rect){origin.x+(float)rectangles[i].x,origin.y+(float)rectangles[i].y,(float)rectangles[i].width,(float)rectangles[i].height},selected(a))!=NX_OK) {free(rectangles);return false;}
        if(nx_draw_text_layout(a->context,row->layout,origin,foreground(a),CACHE)!=NX_OK) {free(rectangles);return false;}
        if(a->contrast) for(size_t i=0;i<count;++i) {
            nx_rect selected_rect={origin.x+(float)rectangles[i].x,origin.y+(float)rectangles[i].y,(float)rectangles[i].width,(float)rectangles[i].height};
            if(nx_fill_rect(a->context,selected_rect,selected(a))!=NX_OK || nx_push_clip(a->context,selected_rect)!=NX_OK ||
               nx_draw_text_layout(a->context,row->layout,origin,rgb(0),CACHE)!=NX_OK || nx_pop_clip(a->context)!=NX_OK) {free(rectangles);return false;}
        }
        free(rectangles);
        if(a->focused && nx_focus_id(a->context)==ids->canvas && !a->review.id && a->mode==EDITING &&
           a->info.cursor>=row->start && a->info.cursor<=row->start+row->size) {
            for(size_t i=0;i<nx_text_layout_caret_count(row->layout);++i) {
                nx_text_caret caret;if(nx_text_layout_caret_at(row->layout,i,&caret)!=NX_OK) return false;
                if(caret.byte_offset==a->info.cursor-row->start) {
                    if(nx_fill_rect(a->context,(nx_rect){origin.x+(float)caret.x,origin.y+(float)caret.top,1.5f,(float)caret.height},foreground(a))!=NX_OK) return false;
                    break;
                }
            }
        }
        if(nx_pop_clip(a->context)!=NX_OK) return false;
    }
    a->ensure_caret=false;return nx_pop_clip(a->context)==NX_OK;
}
static bool modal_begin(struct app *a,uint64_t id,const char *name,nx_rect rect) {
    nx_overlay_desc overlay={.panel={.id=id,.parent_id=1,.bounds=rect,.name=span(name)},.backdrop_id=id+1};int dismissed=0;
    if(nx_modal_begin(a->context,&overlay,&dismissed)!=NX_OK) return false;
    a->overlay_shown=true;return nx_fill_round_rect(a->context,rect,12,surface(a))==NX_OK;
}
static bool paint_review(struct app *a) {
    struct ae_editor_tab tab;if(!ae_editor_workspace_get(a->workspace,a->review.id,&tab)) return false;
    if(a->review_serial!=a->review.serial) {
        a->review_serial=a->review.serial;if(!modal_identity(a)) return false;a->focus_modal=true;a->review_scroll=0;
    }
    char title[AE_FILES_LABEL+1];
    if(tab.leaf) {if(!ae_files_format_name(tab.leaf,strlen(tab.leaf),title)) return false;}else snprintf(title,sizeof(title),"%s",tr(a,"Untitled","Unbenannt"));
    float inset=16,gap=8,width=fminf(a->width-32,620*a->scale),button_height=fmaxf(36,13*a->scale*1.4f+12);
    bool stacked=width<440*a->scale;float footer=stacked?3*button_height+2*gap:button_height;
    const char *question=tab.uncertain?tr(a,"The save could not be confirmed. Keep this document open.","Das Speichern konnte nicht bestätigt werden. Lass dieses Dokument geöffnet."):
        tr(a,"Save the changes before closing?","Änderungen vor dem Schließen speichern?");
    nx_text_layout *name_layout=layout(a,title,strlen(title),16*a->scale,width-2*inset);
    nx_text_layout *question_layout=layout(a,question,strlen(question),13*a->scale,width-2*inset);
    if(!name_layout || !question_layout) return false;
    float qh=(float)nx_text_layout_height(question_layout),nh=(float)nx_text_layout_height(name_layout);
    float name_height=fminf(nh,fmaxf(24,a->height-64-3*gap-qh-footer-2*inset));
    float height=2*inset+name_height+qh+3*gap+footer;
    nx_rect box={(a->width-width)/2,(a->height-height)/2,width,height};
    if(!modal_begin(a,a->modal_id,tr(a,"Unsaved document","Ungespeichertes Dokument"),box)) return false;
    a->review_title=(nx_rect){box.x+inset,box.y+inset,width-2*inset,name_height};a->review_max_scroll=fmaxf(0,nh-name_height);
    a->review_scroll=fminf(a->review_scroll,a->review_max_scroll);
    nx_semantic_desc name_sem={.id=a->modal_id+2,.parent_id=a->modal_id,.role=NX_ROLE_TEXT,.bounds=a->review_title,.name=span(title)};
    if(nx_semantics_add(a->context,&name_sem,NULL)!=NX_OK || nx_push_clip(a->context,a->review_title)!=NX_OK ||
       nx_draw_text_layout(a->context,name_layout,(nx_point){a->review_title.x,a->review_title.y-a->review_scroll},foreground(a),CACHE)!=NX_OK || nx_pop_clip(a->context)!=NX_OK ||
       !wrapped(a,a->modal_id+3,a->modal_id,question,(nx_rect){box.x+inset,box.y+inset+name_height+gap,width-2*inset,qh},13*a->scale,secondary(a))) return false;
    if(a->review_max_scroll && nx_fill_rect(a->context,(nx_rect){box.x+width-8,box.y+inset+name_height*a->review_scroll/(nh?nh:1),2,fmaxf(8,name_height*name_height/nh)},secondary(a))!=NX_OK) return false;
    const char *labels[]={tr(a,"Discard","Verwerfen"),tr(a,"Cancel","Abbrechen"),tr(a,"Save","Speichern")};
    const enum action actions[]={DISCARD,CANCEL,CONFIRM};float bw=stacked?width-2*inset:(width-2*inset-2*gap)/3;
    for(unsigned i=0;i<3;++i) {
        nx_rect r={box.x+inset+(stacked?0:(float)i*(bw+gap)),box.y+height-inset-button_height-(stacked?(float)(2-i)*(button_height+gap):0),bw,button_height};
        if(!button(a,a->modal_id+4+i,a->modal_id,r,labels[i],labels[i],tab.uncertain && i!=1,false,actions[i],a->review.id)) return false;
    }
    if(a->focus_modal) {if(nx_focus_set(a->context,a->modal_id+5)!=NX_OK) return false;a->focus_modal=false;}
    return nx_overlay_end(a->context)==NX_OK;
}
static bool paint_picker(struct app *a) {
    if(ae_files_revision(a->picker)!=a->picker_revision) {
        a->picker_revision=ae_files_revision(a->picker);
        for(size_t i=0;i<ae_files_count(a->picker);++i) if(!(a->row_ids[i]=identity(a))) return false;
    }
    float width=fminf(a->width-32,760*a->scale),height=fminf(a->height-32,580*a->scale),inset=16;
    nx_rect box={(a->width-width)/2,(a->height-height)/2,width,height};
    const char *title=a->mode==PICK_SAVE?tr(a,"Save document","Dokument speichern"):tr(a,"Open document","Dokument öffnen");
    if(!modal_begin(a,a->modal_id,title,box)) return false;
    float heading=box.y+inset,control=fmaxf(32,13*a->scale*1.4f+12),title_height=16*a->scale*1.4f;
    if(!wrapped(a,a->modal_id+2,a->modal_id,title,(nx_rect){box.x+inset,heading,width-2*inset,title_height},16*a->scale,foreground(a))) return false;
    nx_rect up={box.x+inset,heading+title_height+8,48*a->scale,control};
    if(!button(a,a->modal_id+3,a->modal_id,up,"..",tr(a,"Parent folder","Übergeordneter Ordner"),!ae_files_depth(a->picker),false,UP,0) ||
       !label(a,ae_files_depth(a->picker)?ae_files_folder(a->picker):a->root_label,(nx_rect){up.x+up.width+12,up.y,width-up.width-2*inset-12,up.height},12*a->scale,secondary(a))) return false;
    float bottom=box.y+height-inset,error_height=a->message[0]?20*a->scale+8:0;
    float name_label_height=18*a->scale;
    float footer=control+8+(a->mode==PICK_SAVE?control+8+name_label_height:0)+error_height;
    nx_rect list={box.x+inset,up.y+up.height+8,width-2*inset,fmaxf(0,bottom-footer-up.y-up.height-8)};a->picker_list=list;
    float rh=control;size_t rows=(size_t)fmaxf(0,floorf(list.height/rh)),count=ae_files_count(a->picker);
    if(a->first_entry>=count) a->first_entry=count?count-1:0;
    if(nx_push_clip(a->context,list)!=NX_OK) return false;
    for(size_t row=0;row<rows && a->first_entry+row<count;++row) {
        size_t index=a->first_entry+row;const struct ae_files_entry *entry=ae_files_entry(a->picker,index);if(!entry) return false;
        nx_rect r={list.x,list.y+(float)row*rh,list.width,rh-2};char shown[AE_FILES_LABEL+8];
        snprintf(shown,sizeof(shown),"%s%s",entry->kind==AE_FILES_DIRECTORY?"/ ":"  ",entry->label);
        if(!button(a,a->row_ids[index],a->modal_id,r,shown,entry->label,false,index==ae_files_selected(a->picker),SELECT_ROW,index)) return false;
    }
    if(nx_pop_clip(a->context)!=NX_OK) return false;
    if(!count && !label(a,tr(a,"This folder is empty.","Dieser Ordner ist leer."),list,13*a->scale,secondary(a))) return false;
    if(a->mode==PICK_SAVE) {
        if(!label(a,tr(a,"File name","Dateiname"),(nx_rect){box.x+inset,bottom-2*control-8-error_height-name_label_height,width-2*inset,name_label_height},12*a->scale,foreground(a))) return false;
        nx_text_field_desc field={.id=a->field_id,.parent_id=a->modal_id,.bounds={box.x+inset,bottom-2*control-8-error_height,width-2*inset,control},
            .name=span(tr(a,"File name","Dateiname")),.fonts=a->fonts,.pixels_per_em=13*a->scale,.glyph_cache_bytes=CACHE,.ime_window=a->window};
        nx_interaction_state interaction;int valid=1,submitted=0;
        if(nx_text_field_widget(a->context,&field,a->name,&interaction,&valid,&submitted)!=NX_OK) return false;
        if(submitted) a->action=CHOOSE;
        if(a->focus_modal) {if(nx_focus_set(a->context,a->field_id)!=NX_OK) return false;a->focus_modal=false;}
    }
    if(a->message[0] && !label(a,a->message,(nx_rect){box.x+inset,bottom-control-error_height,width-2*inset,error_height-8},12*a->scale,foreground(a))) return false;
    float bw=fminf(140*a->scale,(width-2*inset-8)/2),by=bottom-control;
    if(!button(a,a->modal_id+4,a->modal_id,(nx_rect){box.x+width-inset-2*bw-8,by,bw,control},tr(a,"Cancel","Abbrechen"),tr(a,"Cancel","Abbrechen"),false,false,CANCEL,0) ||
       !button(a,a->modal_id+5,a->modal_id,(nx_rect){box.x+width-inset-bw,by,bw,control},a->mode==PICK_SAVE?tr(a,"Save","Speichern"):tr(a,"Open","Öffnen"),title,false,false,CHOOSE,0)) return false;
    if(a->focus_modal) {if(nx_focus_set(a->context,a->modal_id+5)!=NX_OK) return false;a->focus_modal=false;}
    return nx_overlay_end(a->context)==NX_OK;
}
static bool theme(struct app *a) {
    nx_theme value;if(nx_theme_preset(a->contrast?NX_THEME_HIGH_CONTRAST:a->light?NX_THEME_LIGHT:NX_THEME_DARK,&value)!=NX_OK) return false;
    const nx_color_token tokens[]={NX_COLOR_BACKGROUND,NX_COLOR_SURFACE,NX_COLOR_SURFACE_RAISED,NX_COLOR_TEXT,NX_COLOR_TEXT_MUTED,
        NX_COLOR_BORDER,NX_COLOR_ACCENT,NX_COLOR_ACCENT_TEXT,NX_COLOR_HOVER,NX_COLOR_PRESSED,NX_COLOR_FOCUS,NX_COLOR_SELECTION};
    const nx_color colors[]={background(a),surface(a),surface(a),foreground(a),secondary(a),secondary(a),foreground(a),background(a),
        surface(a),selected(a),focus_color(a),selected(a)};
    for(size_t i=0;i<sizeof(tokens)/sizeof(tokens[0]);++i) if(nx_theme_set_color(&value,tokens[i],colors[i])!=NX_OK) return false;
    return nx_context_set_theme(a->context,&value)==NX_OK;
}
static bool frame(struct app *a,bool *busy) {
    for(unsigned i=0;i<a->layout_count;++i) nx_text_layout_destroy(a->layouts[i]);
    a->layout_count=0;
    a->scale=(float)a->text_percent/100;a->em=14*a->scale;a->line_height=a->em*1.55f;a->action=NONE;a->action_document=0;
    if(!sync_document(a) || !theme(a) || nx_begin_frame(a->context,a->width,a->height,0)!=NX_OK) return false;
    if(!ae_editor_workspace_review(a->workspace,&a->review)) a->review=(struct ae_editor_close_token){0};
    bool blocked=a->mode!=EDITING || a->review.id!=0;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,a->width,a->height},.name=span(tr(a,"Editor","Editor"))};
    if(nx_semantics_add(a->context,&root,NULL)!=NX_OK || nx_fill_rect(a->context,root.bounds,background(a))!=NX_OK) return false;
    float header=fmaxf(36,13*a->scale*1.4f+12);
    if(!button(a,10,1,(nx_rect){8,5,28,28},"×",tr(a,"Close window","Fenster schließen"),blocked,false,CLOSE_WINDOW,0) ||
       !button(a,11,1,(nx_rect){40,5,28,28},"−",tr(a,"Minimize","Minimieren"),true,false,NONE,0) ||
       !button(a,12,1,(nx_rect){72,5,28,28},"□",tr(a,"Zoom or restore","Vergrößern oder wiederherstellen"),blocked,false,ZOOM,0) ||
       !label(a,"Aetheri Editor",(nx_rect){112,0,a->width-124,header},13*a->scale,foreground(a))) return false;
    if(a->width<320 || a->height<240) {
        if(!wrapped(a,4,1,tr(a,"Enlarge the window to continue.","Vergrößere das Fenster, um fortzufahren."),
            (nx_rect){12,header+16,a->width-24,a->height-header-24},13*a->scale,foreground(a))) return false;
        goto render;
    }
    float x=8,y=header+4,h=fmaxf(32,13*a->scale*1.4f+12);
    const char *tools[]={tr(a,"New","Neu"),tr(a,"Open","Öffnen"),tr(a,"Save","Speichern"),tr(a,"Close document","Dokument schließen"),"A−","A+"};
    const enum action actions[]={NEW,OPEN,SAVE,CLOSE_TAB,SMALLER,LARGER};const float widths[]={76,88,100,164,48,48};
    nx_semantic_desc toolbar={.id=2,.parent_id=1,.role=NX_ROLE_TOOLBAR,.bounds={0,y,a->width,2*h+8},.name=span(tr(a,"Document actions","Dokumentaktionen"))};
    if(nx_semantics_add(a->context,&toolbar,NULL)!=NX_OK) return false;
    for(unsigned i=0;i<6;++i) {
        float width=widths[i]*a->scale;if(x+width>a->width-8 && x>8) {x=8;y+=h+4;}
        bool disabled=blocked || ((i==2 || i==3) && !a->document) || (i==4 && a->text_percent==100) || (i==5 && a->text_percent==200);
        if(!button(a,20+i,2,(nx_rect){x,y,width,h},tools[i],tools[i],disabled,false,actions[i],a->document)) return false;
        x+=width+4;
    }
    y+=h+8;unsigned count=ae_editor_workspace_count(a->workspace),active_index=0;struct ae_editor_tab tab;
    for(unsigned i=0;i<count;++i) {if(!ae_editor_workspace_at(a->workspace,i,&tab)) return false;if(tab.active) active_index=i;}
    float nav=32*a->scale,tab_height=fmaxf(32,13*a->scale*1.4f+10);
    unsigned page=(unsigned)fmaxf(1,floorf((a->width-2*nav-24)/(190*a->scale))),first=active_index/page*page;
    float tab_width=(a->width-2*nav-24)/(float)(count && count<page?count:page);
    nx_semantic_desc tabs={.id=5,.parent_id=1,.role=NX_ROLE_TAB_LIST,.bounds={0,y,a->width,tab_height},.name=span(tr(a,"Open documents","Geöffnete Dokumente"))};
    if(nx_semantics_add(a->context,&tabs,NULL)!=NX_OK) return false;
    uint64_t previous=0,next=0;
    if(count) {struct ae_editor_tab item;if(!ae_editor_workspace_at(a->workspace,(active_index+count-1)%count,&item)) return false;previous=item.id;
        if(!ae_editor_workspace_at(a->workspace,(active_index+1)%count,&item)) return false;
        next=item.id;}
    if(!button(a,30,5,(nx_rect){8,y,nav,tab_height},"‹",tr(a,"Previous document","Vorheriges Dokument"),blocked || count<2,false,SELECT_TAB,previous) ||
       !button(a,31,5,(nx_rect){a->width-nav-8,y,nav,tab_height},"›",tr(a,"Next document","Nächstes Dokument"),blocked || count<2,false,SELECT_TAB,next)) return false;
    for(unsigned i=first;i<count && i<first+page;++i) {
        if(!ae_editor_workspace_at(a->workspace,i,&tab)) return false;
        struct ids *ids=tab_ids(a,tab.id);if(!ids) return false;
        char name[AE_FILES_LABEL+1],display[AE_FILES_LABEL+8];struct ae_document_info info;if(!ae_document_info(tab.document,&info)) return false;
        if(tab.leaf) {if(!ae_files_format_name(tab.leaf,strlen(tab.leaf),name)) return false;}else snprintf(name,sizeof(name),"%s",tr(a,"Untitled","Unbenannt"));
        snprintf(display,sizeof(display),"%s%s",name,tab.uncertain?" !":info.dirty?" *":"");
        nx_rect r={nav+12+(float)(i-first)*tab_width,y,tab_width-3,tab_height};float close_width=28*a->scale;
        if(!button(a,ids->tab,5,(nx_rect){r.x,r.y,r.width-close_width,r.height},display,name,blocked,tab.active,SELECT_TAB,tab.id) ||
           !button(a,ids->close,5,(nx_rect){r.x+r.width-close_width,r.y,close_width,r.height},"×",tr(a,"Close document","Dokument schließen"),blocked,false,CLOSE_TAB,tab.id)) return false;
    }
    y+=tab_height+8;float status_height=24*a->scale,side=a->width>=950*a->scale?200*a->scale:0;
    a->canvas=(nx_rect){side,y,a->width-side,fmaxf(0,a->height-status_height-y)};
    if(side) {
        nx_semantic_desc sidebar={.id=6,.parent_id=1,.role=NX_ROLE_GROUP,.bounds={0,y,side,a->canvas.height},.name=span(tr(a,"Workspace","Arbeitsbereich"))};
        if(nx_semantics_add(a->context,&sidebar,NULL)!=NX_OK || nx_fill_rect(a->context,sidebar.bounds,surface(a))!=NX_OK ||
           !label(a,tr(a,"WORKSPACE","ARBEITSBEREICH"),(nx_rect){12,y+8,side-24,26*a->scale},11*a->scale,secondary(a))) return false;
        float sy=y+38*a->scale;
        for(unsigned i=0;i<count;++i) {
            if(!ae_editor_workspace_at(a->workspace,i,&tab)) return false;
            struct ids *ids=tab_ids(a,tab.id);char name[AE_FILES_LABEL+1];if(!ids) return false;
            if(tab.leaf) {if(!ae_files_format_name(tab.leaf,strlen(tab.leaf),name)) return false;}else snprintf(name,sizeof(name),"%s",tr(a,"Untitled","Unbenannt"));
            if(!button(a,ids->sidebar,6,(nx_rect){8,sy,side-16,h},name,name,blocked,tab.active,SELECT_TAB,tab.id)) return false;
            sy+=h+4;
        }
    }
    if(a->document) {if(!paint_document(a)) return false;}
    else if(!wrapped(a,4,1,tr(a,"No document open. Create a document or open a file.","Kein Dokument geöffnet. Erstelle ein Dokument oder öffne eine Datei."),
        (nx_rect){a->canvas.x+32,y+32,a->canvas.width-64,100*a->scale},16*a->scale,foreground(a))) return false;
    char status[320];size_t row=0;
    const char *notice=a->paste.native?tr(a,"Receiving clipboard text…","Zwischenablagentext wird empfangen…"):a->message;
    if(a->document) {while(row+1<a->line_count && a->lines[row+1].start<=a->info.cursor) ++row;
        snprintf(status,sizeof(status),"%s%zu  ·  %s%zu  ·  %s",tr(a,"Line ","Zeile "),row+1,tr(a,"Byte ","Byte "),a->info.cursor-a->lines[row].start+1,notice);}
    else snprintf(status,sizeof(status),"%s",notice);
    nx_semantic_desc footer={.id=3,.parent_id=1,.role=NX_ROLE_STATUS,.bounds={0,a->height-status_height,a->width,status_height},.name=span(status)};
    if(nx_semantics_add(a->context,&footer,NULL)!=NX_OK || !label(a,status,(nx_rect){12,a->height-status_height,a->width-24,status_height},11*a->scale,secondary(a))) return false;
    if(a->mode!=EDITING) {if(!paint_picker(a)) return false;}
    else if(a->review.id && !paint_review(a)) return false;
render:
    if(nx_end_frame(a->context)!=NX_OK) return false;
    const nx_command *commands=NULL;size_t command_count=0;nx_software_view_outcome outcome;
    if(nx_frame_commands(a->context,&commands,&command_count)!=NX_OK || nx_software_view_render(a->renderer,commands,command_count,background(a),&outcome)!=NX_OK) return false;
    *busy=outcome==NX_SOFTWARE_VIEW_BUSY;a->dirty=*busy;
    if(outcome==NX_SOFTWARE_VIEW_READY) {
        nx_result result=nx_window_publish_semantics(a->window,a->context);if(result!=NX_OK && result!=NX_ERROR_UNSUPPORTED) return false;
        if(a->diagnostics) {nx_utf8_span draft={0};if(a->mode==PICK_SAVE && nx_text_field_text(a->name,&draft)!=NX_OK) return false;
            printf("FRAME %.0f %.0f %u docs=%u active=%llu revision=%llu cursor=%zu mode=%d review=%llu focus=%llu draft=%zu error=%u selected=%u begin=%zu end=%zu density=%.2f paste=%u focused=%u\n",
            a->width,a->height,a->text_percent,ae_editor_workspace_count(a->workspace),(unsigned long long)a->document,
            (unsigned long long)a->revision,a->document?a->info.cursor:0,(int)a->mode,(unsigned long long)a->review.id,(unsigned long long)nx_focus_id(a->context),
            draft.length,a->message[0]?1u:0u,a->selection.active?1u:0u,a->selection.begin,a->selection.end,a->density,a->paste.native?1u:0u,a->focused?1u:0u);fflush(stdout);}
    }
    if(a->action!=NONE) {if(!apply(a,a->action,a->action_document)) return false;a->dirty=true;}
    return true;
}
static unsigned encode(uint32_t scalar,char out[4]) {
    if(scalar<0x80) {out[0]=(char)scalar;return 1;}
    if(scalar<0x800) {out[0]=(char)(0xc0u|(scalar>>6));out[1]=(char)(0x80u|(scalar&63));return 2;}
    if(scalar>=0xd800 && scalar<=0xdfff) return 0;
    if(scalar<0x10000) {out[0]=(char)(0xe0u|(scalar>>12));out[1]=(char)(0x80u|((scalar>>6)&63));out[2]=(char)(0x80u|(scalar&63));return 3;}
    if(scalar<=0x10ffff) {out[0]=(char)(0xf0u|(scalar>>18));out[1]=(char)(0x80u|((scalar>>12)&63));out[2]=(char)(0x80u|((scalar>>6)&63));out[3]=(char)(0x80u|(scalar&63));return 4;}
    return 0;
}
static bool pointer_position(struct app *a,float x,float y,bool extend) {
    if(!a->row_count) return true;
    unsigned row=y<a->rows[0].bounds.y?0:(unsigned)fmaxf(0,floorf((y-a->rows[0].bounds.y)/a->line_height));
    if(row>=a->row_count) row=a->row_count-1;
    struct painted_row *painted=&a->rows[row];nx_text_caret caret;
    if(nx_text_layout_hit_test(painted->layout,(double)(x-painted->bounds.x+a->left),0,&caret)!=NX_OK) return false;
    struct ae_direct *input=ae_editor_workspace_input(a->workspace);
    if(!input || !ae_direct_seek(input,painted->start+caret.byte_offset,extend)) return true;
    a->ensure_caret=false;return true;
}
static void clipboard_failure(struct app *a,bool paste) {
    snprintf(a->message,sizeof(a->message),"%s",paste?tr(a,"Couldn't paste this clipboard text. Your document is unchanged.","Dieser Zwischenablagentext konnte nicht eingefügt werden. Dein Dokument bleibt unverändert."):
        tr(a,"Couldn't copy the selection. Your document is unchanged.","Die Auswahl konnte nicht kopiert werden. Dein Dokument bleibt unverändert."));
    a->dirty=true;
}
static bool paste_context(struct app *a,struct ae_direct **out) {
    if(!a->paste.native || !a->focused || a->mode!=EDITING || a->review.id ||
       a->paste.document!=ae_editor_workspace_active(a->workspace) || nx_focus_id(a->context)!=a->paste.canvas) return false;
    struct ae_direct *input=ae_editor_workspace_input(a->workspace);struct ae_direct_token token;
    const struct ae_direct_token *original=&a->paste.input;
    if(!input || !ae_direct_token(input,&token) || token.identity!=original->identity || token.interaction!=original->interaction ||
       token.revision!=original->revision || token.cursor!=original->cursor || token.anchor!=original->anchor || token.selecting!=original->selecting) return false;
    *out=input;return true;
}
/* Only after all already-queued native events and their focus frames. A wake is
 * advisory; the native token and original direct-input context must both match. */
static bool progress_paste(struct app *a) {
    if(!a->paste.native) return false;
    struct ae_direct *input=NULL;if(!paste_context(a,&input)) {cancel_paste(a);return true;}
    char *bytes=NULL;size_t size=0;int ready=0;
    nx_result result=nx_wayland_clipboard_read_take(a->window,a->paste.native,&bytes,&size,&ready);
    if(result==NX_OK && !ready) return false;
    struct ae_direct_token token=a->paste.input;
    /* Failed takes can invalidate a native request. Retire only our own token;
     * never cancel/displace a filename widget's unrelated clipboard request. */
    if(ready) a->paste=(struct paste){0};else cancel_paste(a);
    if(result==NX_OK && ready) {
        if(size) {
            enum ae_document_result inserted=ae_direct_paste(input,&token,bytes,size);
            if(inserted!=AE_DOCUMENT_OK) clipboard_failure(a,true);
            else {a->ensure_caret=true;a->message[0]=0;}
        }
    } else clipboard_failure(a,true);
    if(a->diagnostics) {printf("PASTE complete=%d ready=%d bytes=%zu\n",(int)result,ready,size);fflush(stdout);}
    nx_wayland_clipboard_read_release(bytes,size);a->dirty=true;return true;
}
static bool clipboard(struct app *a,nx_key key) {
    cancel_input(a);
    struct ae_direct *input=ae_editor_workspace_input(a->workspace);if(!input) return true;
    if(key==NX_KEY_V) {
        struct paste pending={.document=ae_editor_workspace_active(a->workspace),.canvas=nx_focus_id(a->context)};
        if(!ae_direct_token(input,&pending.input)) return false;
        nx_result result=nx_wayland_clipboard_read_begin(a->window,AE_DOCUMENT_BYTES,5000,1,&pending.native);
        if(result!=NX_OK) {clipboard_failure(a,true);return true;}
        a->paste=pending;a->dirty=true;
        if(a->diagnostics) {puts("PASTE begin");fflush(stdout);}
    } else {
        struct ae_direct_selection selection;if(!ae_direct_selection(input,&selection)) return false;
        size_t size=selection.end-selection.begin;if(!selection.active || !size) return true;
        char *bytes=malloc(size);if(!bytes) {message(a,AE_EDITOR_WORKSPACE_NOMEM);return true;}
        bool copied=ae_document_copy(ae_direct_document(input),selection.begin,size,bytes,size) && nx_window_clipboard_set_text(a->window,bytes,size)==NX_OK;
        free(bytes);
        if(!copied) clipboard_failure(a,false);
        if(copied && key==NX_KEY_X && ae_direct_delete(input,false)!=AE_DOCUMENT_OK) {
            snprintf(a->message,sizeof(a->message),"%s",tr(a,"Couldn't cut the selection. Your document is unchanged.","Die Auswahl konnte nicht ausgeschnitten werden. Dein Dokument bleibt unverändert."));
            a->dirty=true;
        }
    }
    a->ensure_caret=true;return true;
}
static bool event(struct app *a,const nx_event *e,bool *handled) {
    *handled=false;
    if(e->type==NX_EVENT_FOCUS) {a->focused=e->focused!=0;cancel_input(a);return true;}
    if(e->type==NX_EVENT_POINTER_CANCEL) {cancel_input(a);return true;}
    bool review=a->mode==EDITING && a->review.id!=0;
    if(e->type==NX_EVENT_KEY_DOWN && e->key==NX_KEY_ESCAPE) {
        cancel_paste(a);
        *handled=true;
        if(a->mode!=EDITING || review) return apply(a,CANCEL,0);
        struct ae_direct *input=ae_editor_workspace_input(a->workspace);if(input) (void)ae_direct_clear_selection(input);a->dragging=false;return true;
    }
    if(review) {
        if(e->type==NX_EVENT_WHEEL || (e->type==NX_EVENT_KEY_DOWN && (e->key==NX_KEY_PAGE_UP || e->key==NX_KEY_PAGE_DOWN))) {
            float delta=e->type==NX_EVENT_WHEEL?-e->delta_y*36:(e->key==NX_KEY_PAGE_UP?-a->review_title.height:a->review_title.height);
            a->review_scroll=fmaxf(0,fminf(a->review_max_scroll,a->review_scroll+delta));*handled=true;
        }
        return true;
    }
    if(a->mode!=EDITING) {
        if(e->type==NX_EVENT_WHEEL && contains(a->picker_list,e->x,e->y)) {
            size_t total=ae_files_count(a->picker);double next=(double)a->first_entry-(double)e->delta_y*3;
            a->first_entry=(size_t)fmax(0,fmin(total?(double)total-1:0,next));*handled=true;
        }
        if(e->type==NX_EVENT_KEY_DOWN && e->key==NX_KEY_ENTER && nx_focus_id(a->context)!=a->field_id) {
            const struct ae_files_entry *entry=ae_files_entry(a->picker,ae_files_selected(a->picker));*handled=true;
            if(entry && entry->kind==AE_FILES_DIRECTORY) {if(ae_files_enter(a->picker)!=AE_FILES_OK) message(a,AE_EDITOR_WORKSPACE_IO);else a->first_entry=0;return true;}
            return choose_file(a);
        }
        return true;
    }
    struct ae_direct *input=ae_editor_workspace_input(a->workspace);struct ids *ids=a->document?tab_ids(a,a->document):NULL;
    if(e->type==NX_EVENT_POINTER_DOWN && e->button==1 && contains(a->canvas,e->x,e->y) && input && ids) {
        if(nx_focus_set(a->context,ids->canvas)!=NX_OK || !pointer_position(a,e->x,e->y,(e->modifiers&NX_MOD_SHIFT)!=0)) return false;
        a->dragging=true;a->dragging_document=a->document;
    } else if(e->type==NX_EVENT_POINTER_MOVE && a->dragging) {
        if(a->dragging_document==a->document && !pointer_position(a,e->x,e->y,true)) return false;
        *handled=true;
    } else if(e->type==NX_EVENT_POINTER_UP && e->button==1 && a->dragging) {
        if(a->dragging_document==a->document && !pointer_position(a,e->x,e->y,true)) return false;
        a->dragging=false;
    } else if(e->type==NX_EVENT_WHEEL && contains(a->canvas,e->x,e->y)) {
        if(e->modifiers&NX_MOD_SHIFT) a->left=fmaxf(0,a->left-e->delta_y*30);
        else {double next=(double)a->top-(double)e->delta_y*3;a->top=(size_t)fmax(0,fmin(a->line_count?(double)a->line_count-1:0,next));}
        *handled=true;
    }
    if(e->type==NX_EVENT_KEY_DOWN && (e->modifiers&NX_MOD_CONTROL)) {
        *handled=true;
        if(e->key==NX_KEY_N) return apply(a,NEW,0);
        if(e->key==NX_KEY_O) return apply(a,OPEN,0);
        if(e->key==NX_KEY_S && input) return apply(a,SAVE,0);
        if(e->key==NX_KEY_W && input) return apply(a,CLOSE_TAB,a->document);
        if(e->key==NX_KEY_Q) return apply(a,CLOSE_WINDOW,0);
        if(e->key==NX_KEY_TAB) {
            unsigned count=ae_editor_workspace_count(a->workspace),index=0;struct ae_editor_tab tab;
            for(unsigned i=0;i<count;++i) {if(!ae_editor_workspace_at(a->workspace,i,&tab)) return false;if(tab.active) index=i;}
            if(count && ae_editor_workspace_at(a->workspace,(index+((e->modifiers&NX_MOD_SHIFT)?count-1:1))%count,&tab)) return apply(a,SELECT_TAB,tab.id);
        }
        if(input && ids && nx_focus_id(a->context)==ids->canvas) {
            bool changed=false;
            if(e->key==NX_KEY_A) return ae_direct_select_all(input);
            if(e->key==NX_KEY_C || e->key==NX_KEY_X || e->key==NX_KEY_V) return clipboard(a,e->key);
            if(e->key==NX_KEY_Z || e->key==NX_KEY_Y) {
                enum ae_document_result r=ae_direct_history(input,e->key==NX_KEY_Y || (e->modifiers&NX_MOD_SHIFT)?1:-1,&changed);
                a->ensure_caret=true;return r==AE_DOCUMENT_OK;
            }
            if(e->key==NX_KEY_HOME || e->key==NX_KEY_END) {a->ensure_caret=true;return ae_direct_move(input,e->key==NX_KEY_HOME?AE_DIRECT_FIRST:AE_DIRECT_LAST,(e->modifiers&NX_MOD_SHIFT)!=0);}
        }
        return true;
    }
    if(e->type==NX_EVENT_KEY_DOWN && e->key==NX_KEY_F6 && ids) {
        cancel_input(a);
        *handled=true;return nx_focus_set(a->context,nx_focus_id(a->context)==ids->canvas?20:ids->canvas)==NX_OK;
    }
    if(!input || !ids || !a->focused || nx_focus_id(a->context)!=ids->canvas) return true;
    if(e->type==NX_EVENT_TEXT) {
        *handled=true;if(e->ime_target || e->scalar<32 || (e->modifiers&(NX_MOD_CONTROL|NX_MOD_SUPER))) return true;
        char encoded[4];unsigned size=encode(e->scalar,encoded);if(!size) return true;
        enum ae_document_result r=ae_direct_text(input,encoded,size);if(r!=AE_DOCUMENT_OK) message(a,r==AE_DOCUMENT_NOMEM?AE_EDITOR_WORKSPACE_NOMEM:AE_EDITOR_WORKSPACE_UNSUPPORTED);
        a->ensure_caret=true;return true;
    }
    if(e->type==NX_EVENT_KEY_DOWN) {
        *handled=true;bool extend=(e->modifiers&NX_MOD_SHIFT)!=0;enum ae_document_result result=AE_DOCUMENT_OK;
        switch(e->key) {
        case NX_KEY_LEFT:if(!ae_direct_move(input,AE_DIRECT_LEFT,extend)) return false;break;
        case NX_KEY_RIGHT:if(!ae_direct_move(input,AE_DIRECT_RIGHT,extend)) return false;break;
        case NX_KEY_UP:if(!ae_direct_move(input,AE_DIRECT_UP,extend)) return false;break;
        case NX_KEY_DOWN:if(!ae_direct_move(input,AE_DIRECT_DOWN,extend)) return false;break;
        case NX_KEY_HOME:if(!ae_direct_move(input,AE_DIRECT_HOME,extend)) return false;break;
        case NX_KEY_END:if(!ae_direct_move(input,AE_DIRECT_END,extend)) return false;break;
        case NX_KEY_BACKSPACE:result=ae_direct_delete(input,true);break;
        case NX_KEY_DELETE:result=ae_direct_delete(input,false);break;
        case NX_KEY_ENTER:result=ae_direct_newline(input);break;
        case NX_KEY_TAB:result=ae_direct_text(input,"\t",1);break;
        default:*handled=false;return true;
        }
        if(result!=AE_DOCUMENT_OK) message(a,result==AE_DOCUMENT_NOMEM?AE_EDITOR_WORKSPACE_NOMEM:AE_EDITOR_WORKSPACE_UNSUPPORTED);
        a->ensure_caret=true;
    }
    return true;
}
static unsigned char *font_bytes(const char *path,size_t *size) {
    FILE *file=fopen(path,"rb");if(!file) return NULL;unsigned char *bytes=NULL;long length;
    if(!fseek(file,0,SEEK_END) && (length=ftell(file))>0 && length<64*1024*1024 && !fseek(file,0,SEEK_SET)) {
        bytes=malloc((size_t)length);if(bytes && fread(bytes,1,(size_t)length,file)==(size_t)length) *size=(size_t)length;else {free(bytes);bytes=NULL;}
    }
    fclose(file);return bytes;
}
int main(int argc,char **argv) {
    const char *font_path=NULL,*mono_path=NULL,*directory=".";const char *files[AE_EDITOR_TABS];unsigned count=0;
    struct app a={.root=-1,.next_id=1000,.text_percent=100,.running=true,.dirty=true,.focus_document=true,.ensure_caret=true};
    for(int i=1;i<argc;++i) {
        if(!strcmp(argv[i],"--help")) {puts("aetheri-text-editor FONT [--mono-font FONT] [--directory DIRECTORY] [--german] [--light|--contrast] [--text-percent 100|150|200] [--diagnostics] [FILE ...]");return 0;}
        if(!strcmp(argv[i],"--mono-font") && i+1<argc) mono_path=argv[++i];
        else if(!strcmp(argv[i],"--directory") && i+1<argc) directory=argv[++i];
        else if(!strcmp(argv[i],"--german")) a.german=true;
        else if(!strcmp(argv[i],"--light")) a.light=true;
        else if(!strcmp(argv[i],"--contrast")) a.contrast=true;
        else if(!strcmp(argv[i],"--diagnostics")) a.diagnostics=true;
        else if(!strcmp(argv[i],"--text-percent") && i+1<argc) {const char *value=argv[++i];if(strcmp(value,"100") && strcmp(value,"150") && strcmp(value,"200")) return 2;a.text_percent=(unsigned)strtoul(value,NULL,10);}
        else if(argv[i][0]!='-' && !font_path) font_path=argv[i];
        else if(argv[i][0]!='-' && count<AE_EDITOR_TABS) files[count++]=argv[i];else return 2;
    }
    if(!font_path) return 2;
    unsigned char *font=NULL,*mono=NULL;size_t font_size=0,mono_size=0;int fds[2]={-1,-1},result=1;
    a.root=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);a.workspace=ae_editor_workspace_create();
    if(a.root<0 || !a.workspace || !signal_pipe(fds)) goto done;
    char *display=realpath(directory,NULL);if(!display || !ae_files_format_name(display,strlen(display),a.root_label)) {free(display);goto done;}free(display);
    for(unsigned i=0;i<count;++i) {uint64_t id;if(ae_editor_workspace_open(a.workspace,a.root,files[i],&id)!=AE_EDITOR_WORKSPACE_OK) goto done;}
    if(!count) {uint64_t id;if(ae_editor_workspace_new(a.workspace,&id)!=AE_EDITOR_WORKSPACE_OK) goto done;}
    font=font_bytes(font_path,&font_size);if(mono_path) mono=font_bytes(mono_path,&mono_size);
    nx_window_desc description={"Aetheri Editor",1100,740,NX_WINDOW_CLIENT_CHROME|NX_WINDOW_STATE_EVENTS,{0}};
    if(!font || (mono_path && !mono) || nx_context_create(NULL,&a.context)!=NX_OK || nx_font_open(font,font_size,NULL,&a.font)!=NX_OK ||
       nx_font_collection_create(NULL,&a.fonts)!=NX_OK || nx_font_collection_add(a.fonts,a.font)!=NX_OK || nx_window_create(&description,&a.window)!=NX_OK ||
       nx_window_set_wayland_app_id(a.window,"org.aetheri.Editor")!=NX_OK || nx_software_view_open(a.window,&a.renderer)!=NX_OK) goto done;
    if(mono) {
        if(nx_font_open(mono,mono_size,NULL,&a.mono_font)!=NX_OK || nx_font_collection_create(NULL,&a.mono_fonts)!=NX_OK || nx_font_collection_add(a.mono_fonts,a.mono_font)!=NX_OK) goto done;
    } else a.mono_fonts=a.fonts;
    bool busy=false;
    while(a.running) {
        nx_window_info info;if(nx_window_get_info(a.window,&info)!=NX_OK) goto done;
        if(a.width!=info.width || a.height!=info.height || a.density!=info.scale) a.dirty=true;
        a.width=info.width;a.height=info.height;a.density=info.scale;
        /* The pinned SDK exposes a sticky observation, not a consumable close
         * request. Never interpret it again after the user cancels review. */
        if(info.should_close && !a.close_seen) {a.close_seen=true;if(!apply(&a,CLOSE_WINDOW,0)) goto done;a.dirty=true;}
        if(stopping) {stopping=0;if(!apply(&a,CLOSE_WINDOW,0)) goto done;a.dirty=true;}
        if(!a.running) break;
        if(a.dirty && info.width>=64 && info.height>=64) {
            if(nx_context_set_text_raster_scale(a.context,info.scale)!=NX_OK || !frame(&a,&busy)) goto done;
            if(a.dirty && !busy) continue;
        }
        nx_event incoming;int available=0;
        if(nx_window_next_event(a.window,&incoming,&available)!=NX_OK) goto done;
        if(available) {
            bool handled=false;if(!event(&a,&incoming,&handled)) goto done;
            if(!handled && nx_context_queue_event(a.context,&incoming)!=NX_OK) goto done;
            a.dirty=true;
            /* Render each event before accepting the next: focus/selection and
             * direct document input must share the original native ordering. */
            continue;
        }
        /* Retry drawing only for an actual receive transition. An unrelated
         * dirty but too-small drawable must still reach the native wait. */
        if(progress_paste(&a) && !busy) continue;
        uint32_t signals=0;
        if(nx_window_poll_with_fd(a.window,busy?16:UINT32_MAX,fds[0],&signals)!=NX_OK) goto done;
        if(signals&(NX_WINDOW_FD_ERROR|NX_WINDOW_FD_HANGUP)) goto done;
        if(signals&NX_WINDOW_FD_READABLE) {char bytes[64];while(read(fds[0],bytes,sizeof(bytes))>0) {} }
    }
    result=0;
done:
    cancel_paste(&a);
    if(result) fputs("Aetheri graphical Editor stopped unexpectedly; normal close was not completed.\n",stderr);
    if(a.renderer && nx_software_view_close(&a.renderer)!=NX_OK) result=1;
    if(a.context && a.window) (void)nx_window_publish_semantics(a.window,NULL);
    if(a.picker) ae_files_destroy(a.picker);
    for(unsigned i=0;i<a.layout_count;++i) nx_text_layout_destroy(a.layouts[i]);
    nx_context_destroy(a.context);nx_text_field_destroy(a.name);
    if(a.mono_fonts!=a.fonts) nx_font_collection_destroy(a.mono_fonts);
    nx_font_collection_destroy(a.fonts);nx_font_close(a.mono_font);nx_font_close(a.font);nx_window_destroy(a.window);
    ae_editor_workspace_destroy(a.workspace);free(a.bytes);free(a.lines);free(font);free(mono);
    signal_fd=-1;if(fds[0]>=0) close(fds[0]);if(fds[1]>=0) close(fds[1]);if(a.root>=0) close(a.root);
    return result;
}
