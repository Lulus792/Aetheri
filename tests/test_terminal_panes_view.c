#define _XOPEN_SOURCE 700
#include "terminal/nexaui_view.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"pane view %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char *load(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");CHECK(f && !fseek(f,0,SEEK_END));long bytes=ftell(f);CHECK(bytes>0 && bytes<16*1024*1024 && !fseek(f,0,SEEK_SET));
    *size=(size_t)bytes;unsigned char *out=malloc(*size);CHECK(out && fread(out,1,*size,f)==*size && !fclose(f));return out;
}
static int width(uint32_t value) {return wcwidth((wchar_t)value);}
static uint64_t first_row(struct ae_terminal_view *v,const char *name) {
    size_t count;CHECK(nx_semantics_count(ae_terminal_view_context(v),&count)==NX_OK && count<1000);uint64_t group=0;
    for(size_t i=0;i<count;++i) {
        nx_semantic_view node;CHECK(nx_semantics_get(ae_terminal_view_context(v),(uint32_t)i,&node)==NX_OK);
        if(node.role==NX_ROLE_GROUP && node.name.length==strlen(name) && !memcmp(node.name.data,name,node.name.length)) group=node.id;
    }
    CHECK(group);
    for(size_t i=0;i<count;++i) {
        nx_semantic_view node;CHECK(nx_semantics_get(ae_terminal_view_context(v),(uint32_t)i,&node)==NX_OK);
        if(node.role==NX_ROLE_TEXT && node.parent_id==group) return node.id;
    }
    CHECK(0);return 0;
}
static void frame(struct ae_terminal_view *v,struct ae_terminal *models[8],unsigned mask) {
    CHECK(ae_terminal_view_begin(v,900,240,1,16,10,22,17));
    for(unsigned i=0;i<8;++i) if(mask&(1u<<i)) {
        char name[32];snprintf(name,sizeof(name),"Pane %u",i);
        struct ae_terminal_pane_view pane={.id=i+1u,.name=name,.model=models[i],
            .bounds={(float)(i%4)*220,(float)(i/4)*110,200,66},.enabled=true,.focusable=i==0,.caret=i==0};
        CHECK(ae_terminal_view_pane(v,&pane));
    }
    const unsigned char *pixels=NULL;size_t stride;CHECK(ae_terminal_view_finish(v,900,240,&pixels,&stride) && pixels && stride==3600);
    for(unsigned i=0;i<8;++i) if(mask&(1u<<i)) {
        const unsigned char *pixel=pixels+(size_t)((i/4)*110+1)*stride+(size_t)((i%4)*220+1)*4;
        uint32_t bg=models[i]->screen[0][0].bg;
        CHECK(pixel[0]==(unsigned char)(bg>>16) && pixel[1]==(unsigned char)(bg>>8) && pixel[2]==(unsigned char)bg && pixel[3]==255);
    }
}
int main(int argc,char **argv) {
    CHECK(argc==3 && setlocale(LC_CTYPE,"C.UTF-8"));size_t rn,bn;
    unsigned char *regular=load(argv[1],&rn),*bold=load(argv[2],&bn);struct ae_terminal_view *v=NULL;
    CHECK(ae_terminal_view_create(regular,rn,bold,bn,&v));struct ae_terminal *models[8];
    for(unsigned i=0;i<8;++i) {models[i]=malloc(sizeof(*models[i]));CHECK(models[i]);ae_terminal_init(models[i],20,3,width);ae_terminal_feed(models[i],"same content",12);models[i]->screen[0][0].bg=0x200000u+i*0x001820u;}
    frame(v,models,255);uint64_t ids[8];
    for(unsigned i=0;i<8;++i) {char name[32];snprintf(name,sizeof(name),"Pane %u",i);ids[i]=first_row(v,name);for(unsigned j=0;j<i;++j) CHECK(ids[j]!=ids[i]);}
    CHECK(ae_terminal_view_focus_pane_row(v,1,0));frame(v,models,255);CHECK(ae_terminal_view_reading(v));
    CHECK(nx_semantics_request_action(ae_terminal_view_context(v),ids[1],NX_ACTION_FOCUS)!=NX_OK);
    ae_terminal_feed(models[0],"X",1);frame(v,models,255);CHECK(!ae_terminal_view_reading(v));
    CHECK(first_row(v,"Pane 0")!=ids[0] && first_row(v,"Pane 1")==ids[1]);
    CHECK(nx_semantics_request_action(ae_terminal_view_context(v),ids[0],NX_ACTION_FOCUS)!=NX_OK);
    uint64_t before_hide=first_row(v,"Pane 0");frame(v,models,254);
    CHECK(!ae_terminal_view_focus_pane_row(v,1,0));
    frame(v,models,255);CHECK(first_row(v,"Pane 0")!=before_hide && first_row(v,"Pane 1")==ids[1]);
    uint64_t resized=first_row(v,"Pane 0");CHECK(ae_terminal_resize(models[0],19,3));frame(v,models,255);
    CHECK(first_row(v,"Pane 0")!=resized && first_row(v,"Pane 1")==ids[1]);
    CHECK(ae_terminal_resize(models[0],20,3));frame(v,models,255);CHECK(first_row(v,"Pane 0")!=resized);
    CHECK(ae_terminal_view_begin(v,900,240,1,16,10,22,17));
    struct ae_terminal_pane_view rebind={.id=1,.name="wrong model",.model=models[1],.bounds={0,0,200,66},.enabled=true};
    CHECK(!ae_terminal_view_pane(v,&rebind));CHECK(!ae_terminal_view_grid(v,models[0],rebind.bounds,false,false,false));
    const unsigned char *pixels=NULL;size_t stride;CHECK(ae_terminal_view_finish(v,900,240,&pixels,&stride));
    ae_terminal_view_destroy(v);v=NULL;CHECK(ae_terminal_view_create(regular,rn,bold,bn,&v));
    uint64_t selected=100;
    for(unsigned pass=0;pass<3;++pass) {
        CHECK(ae_terminal_view_begin(v,400,160,1,16,10,22,17));
        uint64_t requested_tab=selected;
        CHECK(ae_terminal_view_tabs(v,7,(nx_rect){0,32,400,28},"Terminal tabs",pass==2,&requested_tab));
        if(pass!=2) selected=requested_tab;
        else CHECK(requested_tab==100); /* The host discards the old request. */
        CHECK(selected==(pass?101u:100u));
        for(unsigned i=0;i<2;++i) {
            struct ae_terminal_view_control control={.id=100u+i,.parent=7,.bounds={(float)i*200,32,196,28},
                .name=i?"Second tab":"First tab",.label=i?"Second":"First",.tab=true,.selected=100u+i==selected,.disabled=pass==2};bool requested=false;
            CHECK(ae_terminal_view_control(v,&control,&requested));CHECK(!requested);
        }
        CHECK(ae_terminal_view_finish(v,400,160,&pixels,&stride));
        uint32_t group;nx_semantic_view semantic;
        CHECK(nx_semantics_find(ae_terminal_view_context(v),7,&group)==NX_OK);
        CHECK(nx_semantics_get(ae_terminal_view_context(v),group,&semantic)==NX_OK);
        CHECK((semantic.state&(NX_SEM_SELECTION_SINGLE|NX_SEM_SELECTION_REQUIRED))==(NX_SEM_SELECTION_SINGLE|NX_SEM_SELECTION_REQUIRED));
        if(!pass) CHECK(nx_semantics_request_action(ae_terminal_view_context(v),101,NX_ACTION_SELECT)==NX_OK);
        if(pass==1) CHECK(nx_semantics_request_action(ae_terminal_view_context(v),100,NX_ACTION_SELECT)==NX_OK);
        if(pass==2) CHECK(nx_semantics_request_action(ae_terminal_view_context(v),100,NX_ACTION_SELECT)!=NX_OK);
        CHECK(nx_semantics_request_action(ae_terminal_view_context(v),selected,NX_ACTION_DESELECT)!=NX_OK);
    }
    ae_terminal_view_destroy(v);for(unsigned i=0;i<8;++i) free(models[i]);free(regular);free(bold);
    puts("pane view: disjoint identical-token models, content/resize/hidden retirement, preserved neighbor identities, refused model rebinding and deferred native tab selection passed");return 0;
}
