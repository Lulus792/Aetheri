#define _GNU_SOURCE
#include "editor/view.h"
#include "terminal/model.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"editor view %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static int width(uint32_t code) { return wcwidth((wchar_t)code); }
static bool contains(const struct ae_terminal *t,unsigned row,const char *text) {
    char line[AE_TERM_COLS+1];for (unsigned x=0;x<t->cols;++x) line[x]=t->screen[row][x].code<128 ? (char)t->screen[row][x].code : '?';line[t->cols]=0;
    return strstr(line,text)!=NULL;
}
int main(void) {
    CHECK(setlocale(LC_CTYPE,"C.UTF-8"));enum ae_document_result r;
    static const char initial[]="one\t漢字\nsecond\r\nlast";
    struct ae_document *d=ae_document_create(initial,sizeof(initial)-1,&r);CHECK(d && r==AE_DOCUMENT_OK);
    struct ae_modal *m=ae_modal_create(d);struct ae_editor_view *v=ae_editor_view_create();struct ae_terminal *t=malloc(sizeof(*t));CHECK(m && v && t);
    struct ae_editor_view_state s={.title="name\033[2J\xff.txt",.message="Ctrl+S Save"};struct ae_editor_frame f;
    unsigned dimensions[][2]={{80,24},{30,8},{120,40},{240,100},{20,6}};
    for (size_t i=0;i<sizeof(dimensions)/sizeof(*dimensions);++i) {
        unsigned cols=dimensions[i][0],rows=dimensions[i][1];ae_terminal_init(t,cols,rows,width);
        CHECK(ae_editor_view_render(v,m,&s,cols,rows,&f) && f.x<cols && f.y<rows && f.compact==(cols<30));
        ae_terminal_feed(t,f.bytes,f.size);CHECK(!t->unsupported && t->x==f.x && t->y==f.y);
        if (!f.compact) { CHECK(contains(t,0,"Aetheri Editor"));CHECK(contains(t,rows-2,"NORMAL"));CHECK(contains(t,1,"one")); }
    }
    CHECK(ae_modal_action(m,AE_MODAL_LINE_APPEND)==AE_DOCUMENT_OK);
    char long_line[500];memset(long_line,'x',sizeof(long_line));CHECK(ae_modal_text(m,long_line,sizeof(long_line))==AE_DOCUMENT_OK);
    CHECK(ae_editor_view_render(v,m,&s,30,8,&f) && f.x>=8 && f.x<29 && f.y==1);
    ae_terminal_init(t,30,8,width);ae_terminal_feed(t,f.bytes,f.size);CHECK(!t->unsupported && t->x==f.x && contains(t,6,"INSERT"));
    s.uncertain=true;s.dialog="[e] Copy [d] Drop [Esc] Back";
    s.title="/a/long/parent/directory/note.txt";
    CHECK(ae_editor_view_render(v,m,&s,30,8,&f));ae_terminal_feed(t,f.bytes,f.size);CHECK(contains(t,6,"SAVE UNKNOWN") && contains(t,7,"[Esc] Back"));
    CHECK(contains(t,0,"note.txt"));
    s.command="export abcdefghijklmnopqrstuvwxyz0123456789";s.dialog="Type DISCARD; Esc cancels";
    CHECK(ae_editor_view_render(v,m,&s,30,8,&f) && f.y==7 && f.x<29);ae_terminal_feed(t,f.bytes,f.size);CHECK(contains(t,6,"Type DISCARD") && contains(t,7,"0123456789"));
    CHECK(ae_modal_action(m,AE_MODAL_ESCAPE)==AE_DOCUMENT_OK && ae_modal_action(m,AE_MODAL_HOME)==AE_DOCUMENT_OK);
    for (const char *prefix="/?";*prefix;++prefix) {
        s.command="literal";s.command_prefix=*prefix;s.dialog="Match: Enter choose; Esc back";s.highlight_size=3;
        CHECK(ae_editor_view_render(v,m,&s,30,8,&f) && f.y==7 && f.x==8);
        ae_terminal_feed(t,f.bytes,f.size);CHECK(t->screen[7][0].code==(uint32_t)*prefix && contains(t,7,"literal") && contains(t,6,"Esc back"));
        CHECK((t->screen[1][8].style&AE_TERM_REVERSE) && (t->screen[1][10].style&AE_TERM_REVERSE) && !(t->screen[1][11].style&AE_TERM_REVERSE));
    }
    s.command=NULL;s.dialog=NULL;s.highlight_size=0;s.message=NULL;CHECK(ae_modal_select(m,false));
    CHECK(ae_editor_view_render(v,m,&s,30,8,&f));ae_terminal_feed(t,f.bytes,f.size);
    CHECK(contains(t,6,"VISUAL") && contains(t,7,"Esc Back") && (t->screen[1][8].style&AE_TERM_REVERSE) && !(t->screen[1][9].style&AE_TERM_REVERSE));
    CHECK(ae_modal_select(m,true));CHECK(ae_editor_view_render(v,m,&s,30,8,&f));ae_terminal_feed(t,f.bytes,f.size);CHECK(contains(t,6,"LINES"));
    CHECK(ae_modal_action(m,AE_MODAL_ESCAPE)==AE_DOCUMENT_OK);
    s.help=true;CHECK(ae_editor_view_render(v,m,&s,80,24,&f));ae_terminal_init(t,80,24,width);ae_terminal_feed(t,f.bytes,f.size);CHECK(contains(t,1,"keyboard help") && !t->unsupported);
    CHECK(!ae_editor_view_render(v,m,&s,513,24,&f));
    free(t);ae_editor_view_destroy(v);ae_modal_destroy(m);ae_document_destroy(d);return 0;
}
