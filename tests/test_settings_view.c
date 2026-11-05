#include "settings/nexaui_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static nx_semantic_view find(nx_context *ui, uint64_t id) {
    uint32_t index;nx_semantic_view item;
    CHECK(nx_semantics_find(ui,id,&index)==NX_OK);
    CHECK(nx_semantics_get(ui,index,&item)==NX_OK);return item;
}
static void frame(struct ae_settings_view *v, float w, float h) {
    nx_window_status status={.known=NX_WINDOW_STATE_MAXIMIZED,.capabilities=NX_WINDOW_CAP_MAXIMIZE|NX_WINDOW_CAP_RESTORE|NX_WINDOW_CAP_MINIMIZE};
    bool changed;
    CHECK(ae_settings_view_frame(v,w,h,&status,&changed));
}
static void key(nx_context *ui, nx_key code, uint32_t modifiers) {
    nx_event event={.type=NX_EVENT_KEY_DOWN,.key=code,.modifiers=modifiers};
    CHECK(nx_context_queue_event(ui,&event)==NX_OK);
    event.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(ui,&event)==NX_OK);
}
static void activate(struct ae_settings_view *v, uint64_t id) {
    CHECK(nx_focus_set(ae_settings_view_context(v),id)==NX_OK);
    key(ae_settings_view_context(v),NX_KEY_SPACE,0);frame(v,1000,720);frame(v,1000,720);
}
static void visible(nx_context *ui, uint64_t id, float w, float h) {
    nx_semantic_view item=find(ui,id);
    CHECK(item.bounds.x>=0 && item.bounds.y>=0 && item.bounds.width>0 && item.bounds.height>0);
    CHECK(item.bounds.x+item.bounds.width<=w+0.1f && item.bounds.y+item.bounds.height<=h+0.1f);
}
static double luminance(nx_color c) {
    double channels[]={c.r,c.g,c.b};
    for(size_t i=0;i<3;++i) channels[i]=channels[i]<=0.04045?channels[i]/12.92:pow((channels[i]+0.055)/1.055,2.4);
    return channels[0]*0.2126+channels[1]*0.7152+channels[2]*0.0722;
}
static void contrast(nx_context *ui,nx_color_token foreground,nx_color_token background,double minimum) {
    nx_color a,b;CHECK(nx_theme_color(ui,foreground,&a)==NX_OK && nx_theme_color(ui,background,&b)==NX_OK);
    CHECK(a.a==1 && b.a==1);
    double x=luminance(a),y=luminance(b);
    CHECK((fmax(x,y)+0.05)/(fmin(x,y)+0.05)>=minimum);
}
int main(int argc,char **argv) {
    CHECK(argc==2 || argc==3);
    unsigned density_only=0;
    if(argc==3) {CHECK(!strcmp(argv[2],"1") || !strcmp(argv[2],"2"));density_only=(unsigned)(argv[2][0]-'0');}
    FILE *f=fopen(argv[1],"rb");CHECK(f);
    CHECK(!fseek(f,0,SEEK_END));long length=ftell(f);CHECK(length>0 && length<64*1024*1024);
    CHECK(!fseek(f,0,SEEK_SET));void *font=malloc((size_t)length);CHECK(font);
    CHECK(fread(font,1,(size_t)length,f)==(size_t)length && !fclose(f));
    struct ae_settings m;ae_settings_init(&m);
    struct ae_settings_view *v=NULL;
    CHECK(ae_settings_view_create(&m,font,(size_t)length,&v));
    nx_context *ui=ae_settings_view_context(v);
    frame(v,1000,720);
    CHECK(find(ui,50).state&NX_SEM_SELECTED);
    CHECK(find(ui,10).name.length==5 && !memcmp(find(ui,10).name.data,"Close",5));
    activate(v,51);CHECK(m.page==AE_SETTINGS_APPEARANCE);
    activate(v,102);CHECK(m.theme==AE_SETTINGS_CONTRAST && (find(ui,102).state&NX_SEM_CHECKED));
    activate(v,112);CHECK(m.text_percent==200);
    activate(v,52);activate(v,121);CHECK(m.german);
    /* Replacing a view retains the original owner's model, not widget state. */
    ae_settings_view_destroy(v);v=NULL;CHECK(ae_settings_view_create(&m,font,(size_t)length,&v));
    ui=ae_settings_view_context(v);frame(v,1000,720);
    CHECK(find(ui,121).state&NX_SEM_CHECKED);
    const float sizes[][2]={{320,240},{640,420},{1000,720},{1920,1080},{3840,2160}};
    unsigned frames=0;
    for(unsigned density=1;density<=2;++density)
    for(unsigned theme=0;theme<3;++theme) for(unsigned german=0;german<2;++german)
    for(unsigned scale=100;scale<=200;scale+=50) for(size_t size=0;size<sizeof(sizes)/sizeof(sizes[0]);++size)
    for(unsigned page=0;page<3;++page) {
        if(density_only && density_only!=density) continue;
        m.theme=(enum ae_settings_theme)theme;m.german=german!=0;m.text_percent=scale;m.page=(enum ae_settings_page)page;
        CHECK(nx_context_set_text_raster_scale(ui,(float)density)==NX_OK);
        float w=sizes[size][0],h=sizes[size][1];frame(v,w,h);
        contrast(ui,NX_COLOR_TEXT,NX_COLOR_BACKGROUND,4.5);
        contrast(ui,NX_COLOR_TEXT_MUTED,NX_COLOR_BACKGROUND,4.5);
        contrast(ui,NX_COLOR_TEXT,NX_COLOR_SELECTION,4.5);
        contrast(ui,NX_COLOR_FOCUS,NX_COLOR_SURFACE,3);
        for(uint64_t id=10;id<=12;++id) visible(ui,id,w,h);
        for(uint64_t id=50;id<=52;++id) visible(ui,id,w,h);
        nx_semantic_view scroll=find(ui,5);CHECK(scroll.bounds.height>=40);
        const uint64_t last=page==1?112:page==2?121:40;
        CHECK(nx_focus_set(ui,last)==NX_OK);frame(v,w,h);visible(ui,last,w,h);
        if(page==1) {
            CHECK(nx_focus_set(ui,100)==NX_OK);frame(v,w,h);
            const uint64_t order[]={100,101,102,110,111,112};
            for(size_t i=0;i<6;++i) {
                CHECK(nx_focus_id(ui)==order[i]);visible(ui,order[i],w,h);
                if(i<5) {key(ui,NX_KEY_TAB,0);frame(v,w,h);}
            }
            key(ui,NX_KEY_TAB,NX_MOD_SHIFT);frame(v,w,h);CHECK(nx_focus_id(ui)==111);
        }
        const nx_command *commands;size_t count;
        CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK && count>10);
        for(size_t i=0;i<count;++i) if(commands[i].type==NX_COMMAND_BITMAP)
            CHECK(isfinite(commands[i].rect.x) && isfinite(commands[i].rect.y));
        ++frames;
    }
    m.page=AE_SETTINGS_APPEARANCE;frame(v,640,420);
    CHECK(nx_focus_set(ui,112)==NX_OK);frame(v,640,420);
    key(ui,NX_KEY_END,0);frame(v,640,420);visible(ui,25,640,420);
    float bottom=find(ui,25).bounds.y;
    key(ui,NX_KEY_PAGE_UP,0);frame(v,640,420);CHECK(find(ui,25).bounds.y>bottom);
    key(ui,NX_KEY_HOME,0);frame(v,640,420);visible(ui,20,640,420);
    CHECK(nx_focus_id(ui)==112 && m.theme==AE_SETTINGS_CONTRAST && m.german && m.text_percent==200);
    struct ae_settings_display d[2]={
        {.id=42,.name="HEADLESS-1",.width=1920,.height=1080,.scale=1,.refresh=60,.mode_known=true},
        {.id=91,.name="Second display",.width=3840,.height=2160,.scale=2,.refresh=0,.mode_known=true}};
    CHECK(ae_settings_snapshot(&m,d,2));m.page=AE_SETTINGS_DISPLAY;frame(v,1000,720);
    CHECK(nx_semantics_request_action(ui,UINT64_C(0x100000000)+91,NX_ACTION_PRESS)==NX_OK);
    frame(v,1000,720);CHECK(m.selected==1);
    struct ae_settings_display reverse[2]={d[1],d[0]};
    CHECK(ae_settings_snapshot(&m,reverse,2) && m.selected==0);frame(v,1000,720);
    CHECK(find(ui,UINT64_C(0x100000000)+91).state&NX_SEM_CHECKED);
    CHECK(nx_semantics_request_action(ui,UINT64_C(0x100000000)+42,NX_ACTION_PRESS)==NX_OK);
    CHECK(!ae_settings_snapshot(&m,NULL,1));frame(v,1000,720);
    CHECK(m.selected==0 && (find(ui,UINT64_C(0x100000000)+42).state&NX_SEM_DISABLED));
    activate(v,40);CHECK(ae_settings_view_take_refresh(v));CHECK(!ae_settings_view_take_refresh(v));
    CHECK(ae_settings_snapshot(&m,NULL,0));frame(v,1000,720);
    activate(v,11);CHECK(ae_settings_view_take_action(v)==AE_SETTINGS_WINDOW_ZOOM);
    CHECK(ae_settings_view_take_action(v)==AE_SETTINGS_WINDOW_NONE);
    nx_window_status unavailable={0};bool changed;
    CHECK(ae_settings_view_frame(v,1000,720,&unavailable,&changed));
    CHECK(find(ui,11).state&NX_SEM_DISABLED);
    CHECK(nx_semantics_request_action(ui,11,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(!ae_settings_view_frame(v,NAN,720,&unavailable,&changed));
    CHECK(!ae_settings_view_frame(v,319,720,&unavailable,&changed));
    memset(d[0].name,'W',sizeof(d[0].name)-1);d[0].name[sizeof(d[0].name)-1]=0;
    CHECK(ae_settings_snapshot(&m,d,1));m.page=AE_SETTINGS_DISPLAY;frame(v,320,320);
    nx_semantic_view long_name=find(ui,UINT64_C(0x100000000)+42);
    CHECK(long_name.name.length==130 && !memcmp(long_name.name.data+3,d[0].name,127));
    ae_settings_view_destroy(v);free(font);
    printf("Settings presentation: %u theme/language/text/viewport/page frames, keyboard, ownership and stale-state cases\n",frames);
    return 0;
}
