#define _GNU_SOURCE
#include "files/nexaui_view.h"
#include "text/grapheme.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"Files view %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static void frame(struct ae_files_view *v,float w,float h) {
    nx_window_status status={.capabilities=NX_WINDOW_CAP_MOVE|NX_WINDOW_CAP_RESIZE|NX_WINDOW_CAP_MAXIMIZE|NX_WINDOW_CAP_RESTORE};
    bool changed=true;unsigned limit=0;
    do { CHECK(ae_files_view_frame(v,w,h,&status,&changed));CHECK(++limit<8); }while(changed);
}
static void key(struct ae_files_view *v,nx_key k,uint32_t mods) {
    nx_event e={.type=NX_EVENT_KEY_DOWN,.key=k,.modifiers=mods};CHECK(ae_files_view_event(v,&e));
    e.type=NX_EVENT_KEY_UP;CHECK(ae_files_view_event(v,&e));
}
static nx_semantic_view get(struct ae_files_view *v,uint64_t id) {
    uint32_t i;nx_semantic_view s;CHECK(id && nx_semantics_find(ae_files_view_context(v),id,&i)==NX_OK);
    CHECK(nx_semantics_get(ae_files_view_context(v),i,&s)==NX_OK);return s;
}
static uint64_t named(struct ae_files_view *v,nx_semantic_role role,const char *name) {
    size_t count;CHECK(nx_semantics_count(ae_files_view_context(v),&count)==NX_OK);
    for(size_t i=0;i<count;++i) { nx_semantic_view node;CHECK(nx_semantics_get(ae_files_view_context(v),(uint32_t)i,&node)==NX_OK);
        if(node.role==role && node.name.length==strlen(name) && !memcmp(node.name.data,name,node.name.length)) return node.id;
    }
    CHECK(false);return 0;
}
static void press(struct ae_files_view *v,uint64_t id) {
    CHECK(nx_semantics_request_action(ae_files_view_context(v),id,NX_ACTION_PRESS)==NX_OK);frame(v,1000,680);
}
static void text(struct ae_files_view *v,const char *s) {
    for(size_t i=0;s[i];++i) {nx_event e={.type=NX_EVENT_TEXT,.scalar=(unsigned char)s[i]};CHECK(ae_files_view_event(v,&e));}
    frame(v,1000,680);
}
static void unicode(struct ae_files_view *v,const char *s) {
    size_t at=0,n=strlen(s);uint32_t scalar;
    while(at<n) {CHECK(ae_utf8_next(s,n,&at,&scalar));nx_event e={.type=NX_EVENT_TEXT,.scalar=scalar};CHECK(ae_files_view_event(v,&e));}
    frame(v,1000,680);
}
static void pointer(struct ae_files_view *v,nx_event_type type,float x,float y,uint32_t mods) {
    nx_event e={.type=type,.x=x,.y=y,.button=1,.modifiers=mods};CHECK(ae_files_view_event(v,&e));frame(v,1000,680);
}
static void editing_cases(struct ae_files_view *v,struct ae_files_session *s) {
    CHECK(ae_files_select(s->files,0));frame(v,1000,680);uint64_t old=ae_files_view_entry_id(v,0);
    CHECK(nx_semantics_request_action(ae_files_view_context(v),old,NX_ACTION_PRESS)==NX_OK);
    CHECK(ae_files_filter(s->files,"abc",3));frame(v,1000,680);CHECK(!s->dialog && ae_files_depth(s->files)==0);
    key(v,NX_KEY_L,NX_MOD_CONTROL);frame(v,1000,680);text(v,"abcd");
    key(v,NX_KEY_HOME,0);key(v,NX_KEY_RIGHT,0);key(v,NX_KEY_RIGHT,NX_MOD_SHIFT);key(v,NX_KEY_RIGHT,NX_MOD_SHIFT);text(v,"X");
    CHECK(!strcmp(ae_files_query(s->files),"aXd"));
    key(v,NX_KEY_BACKSPACE,0);key(v,NX_KEY_DELETE,0);frame(v,1000,680);CHECK(!strcmp(ae_files_query(s->files),"a"));
    key(v,NX_KEY_A,NX_MOD_CONTROL);text(v,"replacement");CHECK(!strcmp(ae_files_query(s->files),"replacement"));
    key(v,NX_KEY_U,NX_MOD_CONTROL);frame(v,1000,680);unicode(v,"a\xcc\x81" "b");key(v,NX_KEY_LEFT,0);key(v,NX_KEY_BACKSPACE,0);frame(v,1000,680);
    CHECK(!strcmp(ae_files_query(s->files),"b"));
    key(v,NX_KEY_U,NX_MOD_CONTROL);frame(v,1000,680);unicode(v,"\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb" "x");
    key(v,NX_KEY_HOME,0);key(v,NX_KEY_DELETE,0);frame(v,1000,680);CHECK(!strcmp(ae_files_query(s->files),"x"));
    key(v,NX_KEY_U,NX_MOD_CONTROL);frame(v,1000,680);text(v,"pointer");nx_rect r=get(v,ae_files_view_control_id(v,AE_FV_SEARCH)).bounds;
    pointer(v,NX_EVENT_POINTER_DOWN,r.x+2,r.y+r.height/2,0);pointer(v,NX_EVENT_POINTER_UP,r.x+2,r.y+r.height/2,0);
    text(v,"a");CHECK(!strcmp(ae_files_query(s->files),"apointer"));
    pointer(v,NX_EVENT_POINTER_DOWN,r.x+r.width-3,r.y+r.height/2,0);
    pointer(v,NX_EVENT_POINTER_MOVE,r.x+2,r.y+r.height/2,0);pointer(v,NX_EVENT_POINTER_UP,r.x+2,r.y+r.height/2,0);
    text(v,"dragged");CHECK(!strcmp(ae_files_query(s->files),"dragged"));
    key(v,NX_KEY_ESCAPE,0);frame(v,1000,680);key(v,NX_KEY_N,NX_MOD_CONTROL);frame(v,1000,680);text(v,"abcd");
    key(v,NX_KEY_HOME,0);key(v,NX_KEY_RIGHT,0);key(v,NX_KEY_END,NX_MOD_SHIFT);text(v,"Q");CHECK(!strcmp(s->name,"aQ"));
    key(v,NX_KEY_A,NX_MOD_CONTROL);text(v,"bad");nx_event slash={.type=NX_EVENT_TEXT,.scalar='/'};
    CHECK(ae_files_view_event(v,&slash));frame(v,1000,680);CHECK(s->name_bad && !strcmp(s->name,"bad"));
    key(v,NX_KEY_A,NX_MOD_CONTROL);key(v,NX_KEY_BACKSPACE,0);text(v,"ignored");CHECK(s->name_bad && !strcmp(s->name,"bad"));
    key(v,NX_KEY_U,NX_MOD_CONTROL);frame(v,1000,680);CHECK(!s->name_bad && !s->name_size && !*s->message);
    char full[AE_FILES_NAME+1];memset(full,'w',AE_FILES_NAME);full[AE_FILES_NAME]=0;text(v,full);CHECK(s->name_size==AE_FILES_NAME);
    key(v,NX_KEY_A,NX_MOD_CONTROL);text(v,"small");CHECK(!s->name_bad && !strcmp(s->name,"small"));
    key(v,NX_KEY_ESCAPE,0);frame(v,1000,680);
}
static void chrome_hits(struct ae_files_view *v,struct ae_files_session *s) {
    nx_window_status status={.capabilities=NX_WINDOW_CAP_MOVE|NX_WINDOW_CAP_RESIZE|NX_WINDOW_CAP_MAXIMIZE|NX_WINDOW_CAP_RESTORE};
    bool title;nx_window_resize_edge edge;nx_cursor_shape cursor;
    CHECK(ae_files_view_native_hit(v,(nx_point){180,16},&status,&title,&edge,&cursor) && title && !edge);
    const nx_point points[]={{2,2},{500,2},{998,2},{998,340},{998,678},{500,678},{2,678},{2,340}};
    const nx_window_resize_edge edges[]={NX_WINDOW_RESIZE_TOP_LEFT,NX_WINDOW_RESIZE_TOP,NX_WINDOW_RESIZE_TOP_RIGHT,NX_WINDOW_RESIZE_RIGHT,
        NX_WINDOW_RESIZE_BOTTOM_RIGHT,NX_WINDOW_RESIZE_BOTTOM,NX_WINDOW_RESIZE_BOTTOM_LEFT,NX_WINDOW_RESIZE_LEFT};
    for(size_t i=0;i<8;++i) CHECK(ae_files_view_native_hit(v,points[i],&status,&title,&edge,&cursor) && !title && edge==edges[i] && cursor!=NX_CURSOR_DEFAULT);
    CHECK(ae_files_view_native_hit(v,(nx_point){28,16},&status,&title,&edge,&cursor) && !title && !edge);
    status.state=NX_WINDOW_STATE_MAXIMIZED;
    CHECK(ae_files_view_native_hit(v,(nx_point){180,16},&status,&title,&edge,&cursor) && title && !edge);
    CHECK(ae_files_view_native_hit(v,(nx_point){998,678},&status,&title,&edge,&cursor) && !title && !edge);
    status.state=NX_WINDOW_STATE_FULLSCREEN;
    CHECK(ae_files_view_native_hit(v,(nx_point){180,16},&status,&title,&edge,&cursor) && !title && !edge);
    status.state=0;status.capabilities=0;
    CHECK(ae_files_view_native_hit(v,(nx_point){180,16},&status,&title,&edge,&cursor) && !title && !edge);
    status.capabilities=NX_WINDOW_CAP_MOVE|NX_WINDOW_CAP_RESIZE;
    ae_files_session_dialog_open(s,AE_FILES_CREATE_FILE);frame(v,1000,680);
    CHECK(ae_files_view_native_hit(v,(nx_point){180,16},&status,&title,&edge,&cursor) && !title && !edge);
    CHECK(ae_files_view_native_hit(v,(nx_point){998,678},&status,&title,&edge,&cursor) && !title && !edge);
    ae_files_session_dialog_close(s);frame(v,1000,680);
}
static void native_value_cases(struct ae_files_view *v,struct ae_files_session *s,unsigned *attempts) {
    key(v,NX_KEY_N,NX_MOD_CONTROL);frame(v,1000,680);text(v,"before");
    uint64_t id=ae_files_view_control_id(v,AE_FV_NAME),confirm=ae_files_view_control_id(v,AE_FV_CONFIRM);
    CHECK(get(v,id).actions&NX_ACTION_SET_VALUE);
    CHECK(nx_semantics_request_text_value(ae_files_view_context(v),id,(nx_utf8_span){"after",5})==NX_OK);
    CHECK(nx_semantics_request_action(ae_files_view_context(v),confirm,NX_ACTION_PRESS)==NX_OK);
    frame(v,1000,680);CHECK(!strcmp(s->name,"after") && *attempts==0 && !s->work_active);
    CHECK(ae_files_view_control_id(v,AE_FV_CONFIRM)!=confirm);
    id=ae_files_view_control_id(v,AE_FV_NAME);
    CHECK(nx_semantics_request_text_value(ae_files_view_context(v),id,(nx_utf8_span){"/",1})==NX_OK);
    frame(v,1000,680);CHECK(s->name_bad && !strcmp(s->name,"after"));
    id=ae_files_view_control_id(v,AE_FV_NAME);CHECK(get(v,id).state&NX_SEM_READONLY);
    CHECK(nx_semantics_request_text_value(ae_files_view_context(v),id,(nx_utf8_span){"blocked",7})!=NX_OK);
    key(v,NX_KEY_U,NX_MOD_CONTROL);frame(v,1000,680);CHECK(!s->name_bad && !s->name_size);
    text(v,"held");nx_event focus={.type=NX_EVENT_FOCUS,.focused=0};CHECK(ae_files_view_event(v,&focus));frame(v,1000,680);
    key(v,NX_KEY_ENTER,0);frame(v,1000,680);CHECK(*attempts==0 && !s->work_active && !strcmp(s->name,"held"));
    focus.focused=1;CHECK(ae_files_view_event(v,&focus));frame(v,1000,680);key(v,NX_KEY_U,NX_MOD_CONTROL);frame(v,1000,680);
    id=ae_files_view_control_id(v,AE_FV_NAME);
    CHECK(nx_semantics_request_text_value(ae_files_view_context(v),id,(nx_utf8_span){"retired",7})==NX_OK);
    ae_files_session_dialog_close(s);ae_files_session_dialog_open(s,AE_FILES_CREATE_FILE);frame(v,1000,680);
    CHECK(!s->name_size && *attempts==0);key(v,NX_KEY_ESCAPE,0);frame(v,1000,680);
}
static void naming_bounds(struct ae_files_view *v,float w,float h) {
    const enum ae_files_control controls[]={AE_FV_NAME,AE_FV_CANCEL,AE_FV_CONFIRM};nx_rect boxes[3];
    for(size_t i=0;i<3;++i) {nx_semantic_view node=get(v,ae_files_view_control_id(v,controls[i]));boxes[i]=node.bounds;
        nx_semantic_view parent=get(v,node.parent_id);
        if(controls[i]==AE_FV_NAME) {CHECK(parent.role==NX_ROLE_GROUP);parent=get(v,parent.parent_id);}
        CHECK(parent.role==NX_ROLE_DIALOG);
        CHECK(boxes[i].width>0 && boxes[i].height>0 && boxes[i].x>=0 && boxes[i].y>=0 && boxes[i].x+boxes[i].width<=w && boxes[i].y+boxes[i].height<=h);
        CHECK(boxes[i].x>=parent.bounds.x && boxes[i].y>=parent.bounds.y && boxes[i].x+boxes[i].width<=parent.bounds.x+parent.bounds.width && boxes[i].y+boxes[i].height<=parent.bounds.y+parent.bounds.height);
    }
    for(size_t i=0;i<3;++i) for(size_t j=i+1;j<3;++j)
        CHECK(boxes[i].x+boxes[i].width<=boxes[j].x || boxes[j].x+boxes[j].width<=boxes[i].x || boxes[i].y+boxes[i].height<=boxes[j].y || boxes[j].y+boxes[j].height<=boxes[i].y);
}
static bool reject_worker(void *context,struct ae_files_session *s) {
    (void)s;++*(unsigned *)context;return false;
}
static void unexpected_join(void *context) { (void)context;CHECK(false); }
int main(int argc,char **argv) {
    CHECK(argc==3 && getuid()!=0);int density=atoi(argv[2]);CHECK(density==1 || density==2);
    FILE *f=fopen(argv[1],"rb");CHECK(f && !fseek(f,0,SEEK_END));long length=ftell(f);CHECK(length>0 && !fseek(f,0,SEEK_SET));
    void *font=malloc((size_t)length);CHECK(font && fread(font,1,(size_t)length,f)==(size_t)length && !fclose(f));
    char path[]="files-view-XXXXXX";CHECK(mkdtemp(path) && !chmod(path,0700));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=3);
    CHECK(!mkdirat(root,".hidden",0700) && !mkdirat(root,"folder",0700));int fd=openat(root,"abc",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0 && !close(fd));
    unsigned attempts=0;struct ae_files_session s;ae_files_session_init(&s,reject_worker,unexpected_join,&attempts);enum ae_files_result r;s.files=ae_files_create(root,&r);CHECK(s.files);
    memcpy(s.root_label,"Documents",10);struct ae_files_view *v=NULL;CHECK(ae_files_view_create(&s,font,(size_t)length,&v));
    CHECK(nx_context_set_text_raster_scale(ae_files_view_context(v),(float)density)==NX_OK);frame(v,1000,680);
    chrome_hits(v,&s);
    native_value_cases(v,&s,&attempts);
    key(v,NX_KEY_H,NX_MOD_CONTROL);frame(v,1000,680);
    uint64_t hidden=named(v,NX_ROLE_BUTTON,".hidden");
    CHECK(nx_semantics_request_action(ae_files_view_context(v),hidden,NX_ACTION_PRESS)==NX_OK);
    key(v,NX_KEY_H,NX_MOD_CONTROL);frame(v,1000,680);
    CHECK(ae_files_depth(s.files)==0); /* Hidden location cannot retarget to the next sidebar item. */
    uint64_t old=ae_files_view_entry_id(v,0),revision=ae_files_revision(s.files);CHECK(old);
    CHECK(nx_semantics_request_action(ae_files_view_context(v),old,NX_ACTION_PRESS)==NX_OK);
    CHECK(ae_files_refresh(s.files)==AE_FILES_OK && ae_files_revision(s.files)>revision);frame(v,1000,680);
    CHECK(ae_files_depth(s.files)==0 && ae_files_view_entry_id(v,0)!=old);
    press(v,ae_files_view_entry_id(v,0));CHECK(ae_files_depth(s.files)==1);
    press(v,ae_files_view_control_id(v,AE_FV_UP));CHECK(ae_files_depth(s.files)==0);
    key(v,NX_KEY_F10,0);frame(v,1000,680);old=ae_files_view_control_id(v,AE_FV_RENAME);CHECK(old);
    CHECK(nx_semantics_request_action(ae_files_view_context(v),old,NX_ACTION_PRESS)==NX_OK);
    CHECK(ae_files_select(s.files,1));frame(v,1000,680);CHECK(!s.dialog);
    key(v,NX_KEY_ESCAPE,0);frame(v,1000,680);key(v,NX_KEY_N,NX_MOD_CONTROL);frame(v,1000,680);text(v,"new");
    old=ae_files_view_control_id(v,AE_FV_CONFIRM);CHECK(old && !(get(v,old).state&NX_SEM_DISABLED));
    CHECK(nx_semantics_request_action(ae_files_view_context(v),old,NX_ACTION_PRESS)==NX_OK);
    uint64_t serial=s.dialog_serial;ae_files_session_dialog_close(&s);ae_files_session_dialog_open(&s,AE_FILES_CREATE_FILE);ae_files_session_name_input(&s,"new");
    CHECK(s.dialog_serial>serial);frame(v,1000,680);CHECK(attempts==0 && s.creations==0 && !s.work_active);
    CHECK(faccessat(root,"new",F_OK,0)<0);
    old=ae_files_view_control_id(v,AE_FV_CONFIRM);CHECK(old);
    CHECK(nx_semantics_request_action(ae_files_view_context(v),old,NX_ACTION_PRESS)==NX_OK);
    s.name_size=0;s.name[0]=0;ae_files_session_name_input(&s,"different");frame(v,1000,680);
    CHECK(attempts==0 && !s.work_active);
    press(v,ae_files_view_control_id(v,AE_FV_CONFIRM));CHECK(attempts==1 && !s.work_active);
    key(v,NX_KEY_ESCAPE,0);frame(v,1000,680);key(v,NX_KEY_L,NX_MOD_CONTROL);frame(v,1000,680);
    nx_event start={.type=NX_EVENT_TEXT,.scalar='a',.ime_before=NX_IME_COMMIT_START};CHECK(ae_files_view_event(v,&start));
    CHECK(!*ae_files_query(s.files));nx_event end={.type=NX_EVENT_TEXT,.scalar='b',.ime_before=NX_IME_COMMIT_END|NX_IME_COMMIT_FILTERED};
    CHECK(ae_files_view_event(v,&end));frame(v,1000,680);CHECK(s.query_bad && !*ae_files_query(s.files));
    key(v,NX_KEY_L,NX_MOD_CONTROL);frame(v,1000,680);CHECK(ae_files_view_event(v,&start));end.ime_before=NX_IME_COMMIT_END;
    CHECK(ae_files_view_event(v,&end));frame(v,1000,680);CHECK(!s.query_bad && !strcmp(ae_files_query(s.files),"ab"));
    key(v,NX_KEY_ESCAPE,0);frame(v,1000,680);
    editing_cases(v,&s);
    const int sizes[][2]={{640,420},{1000,680},{1920,1080},{3840,2160}};unsigned frames=0;
    for(unsigned contrast=0;contrast<2;++contrast) for(unsigned german=0;german<2;++german)
    for(unsigned zoom=0;zoom<4;++zoom) for(unsigned grid=0;grid<2;++grid) for(size_t size=0;size<4;++size) {
        const float scales[]={1,1.25f,1.5f,2};s.contrast=contrast!=0;s.german=german!=0;s.scale=scales[zoom];s.grid=grid!=0;
        frame(v,(float)sizes[size][0],(float)sizes[size][1]);
        const enum ae_files_control ids[]={AE_FV_CLOSE,AE_FV_ZOOM,AE_FV_UP,AE_FV_NEW_FILE,AE_FV_NEW_FOLDER,AE_FV_SEARCH};
        for(size_t i=0;i<sizeof(ids)/sizeof(ids[0]);++i) { nx_semantic_view node=get(v,ae_files_view_control_id(v,ids[i]));
            CHECK(node.bounds.x>=0 && node.bounds.y>=0 && node.bounds.x+node.bounds.width<=(float)sizes[size][0]+.1f && node.bounds.y+node.bounds.height<=(float)sizes[size][1]+.1f); }
        ++frames;
    }
    s.scale=2;s.german=true;
    for(unsigned grid=0;grid<2;++grid) {
        s.grid=grid!=0;frame(v,640,420);key(v,NX_KEY_N,NX_MOD_CONTROL);frame(v,640,420);naming_bounds(v,640,420);
        CHECK(nx_focus_id(ae_files_view_context(v))==ae_files_view_control_id(v,AE_FV_NAME));
        key(v,NX_KEY_TAB,0);frame(v,640,420);CHECK(nx_focus_id(ae_files_view_context(v))==ae_files_view_control_id(v,AE_FV_CANCEL));
        key(v,NX_KEY_ESCAPE,0);frame(v,640,420);CHECK(!s.dialog);
    }
    CHECK(ae_files_select(s.files,1));frame(v,640,420);key(v,NX_KEY_X,NX_MOD_CONTROL);
    CHECK(ae_files_select(s.files,0));key(v,NX_KEY_ENTER,0);frame(v,640,420);CHECK(ae_files_depth(s.files)==1);
    key(v,NX_KEY_V,NX_MOD_CONTROL);frame(v,640,420);CHECK(s.moving && s.dialog);naming_bounds(v,640,420);
    key(v,NX_KEY_ESCAPE,0);frame(v,640,420);CHECK(!s.dialog);
    key(v,NX_KEY_HOME,NX_MOD_ALT);frame(v,640,420);key(v,NX_KEY_F9,0);frame(v,640,420);CHECK(s.info_overlay);
    nx_semantic_view info_close=get(v,ae_files_view_control_id(v,AE_FV_INFO_CLOSE));size_t nodes;
    CHECK(nx_semantics_count(ae_files_view_context(v),&nodes)==NX_OK);
    for(size_t i=0;i<nodes;++i) {nx_semantic_view node;CHECK(nx_semantics_get(ae_files_view_context(v),(uint32_t)i,&node)==NX_OK);
        if(node.role==NX_ROLE_TEXT && node.parent_id==info_close.parent_id) CHECK(node.bounds.y+node.bounds.height<=info_close.bounds.y);
    }
    key(v,NX_KEY_ESCAPE,0);frame(v,640,420);
    frame(v,240,180);CHECK(ae_files_view_control_id(v,AE_FV_CLOSE)==10 && !ae_files_view_control_id(v,AE_FV_NEW_FILE));
    key(v,NX_KEY_N,NX_MOD_CONTROL);CHECK(!s.dialog);frame(v,1000,680);
    s.dialog_serial=UINT64_MAX;ae_files_session_dialog_open(&s,AE_FILES_CREATE_FILE);CHECK(!s.dialog);
    ae_files_view_destroy(v);ae_files_session_dialog_close(&s);ae_files_destroy(s.files);close(root);free(font);
    printf("Files view: %u layouts at density %d; stale snapshot/selection/dialog/sidebar, modal keyboard/200%% bounds and whole-commit guards\n",frames,density);return 0;
}
