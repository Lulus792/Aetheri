#include "terminal/model.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1); } } while (0)
static int width(uint32_t c) { return c==0x301 ? 0 : c==0x4e2d ? 2 : 1; }
static void feed(struct ae_terminal *m,const char *s) { ae_terminal_feed(m,s,strlen(s)); }
int main(void) {
    struct ae_terminal *m=malloc(sizeof(*m));CHECK(m);ae_terminal_init(m,8,3,width);
    feed(m,"hello\r\nGr\xc3");ae_terminal_feed(m,"\xbc\xc3\x9f" "e",4);
    CHECK(m->screen[1][2].code==0xfc && m->screen[1][3].code==0xdf);
    feed(m,"\r\033[32mX\033[0m");CHECK(m->screen[1][0].code=='X' && m->screen[1][0].fg==0x50c878);
    feed(m,"\033[2;2H\033[38;2;12;34;56mQ");CHECK(m->screen[1][1].fg==0x0c2238);
    feed(m,"\033[2J\033[H12345678Z");CHECK(m->screen[1][0].code=='Z' && m->history_count==0);
    feed(m,"\r\033[K");CHECK(m->screen[1][0].code==' ');
    feed(m,"\033[He\xcc\x81");CHECK(m->screen[0][0].code=='e' && m->screen[0][0].count==1);
    feed(m,"\033[H\xe4\xb8\xad");CHECK(m->screen[0][0].width==2 && !m->screen[0][1].width);
    feed(m,"\033[1;2H\033[K");CHECK(m->screen[0][0].code==' ' && m->screen[0][1].width==1);
    feed(m,"\033[Hnormal\033[?1049h\033[Halt\033[s\033[?1049l");CHECK(m->screen[0][0].code=='n');
    feed(m,"\033]52;c;Y2xpcGJvYXJk\007VISIBLE");CHECK(m->unsupported>0);
    for (unsigned i=0;i<AE_TERM_HISTORY+100;++i) feed(m,"\r\nline");
    CHECK(m->history_count==AE_TERM_HISTORY);ae_terminal_scroll(m,10);CHECK(m->view==10 && ae_terminal_row(m,0));
    CHECK(ae_terminal_resize(m,40,20) && ae_terminal_resize(m,8,3));CHECK(!ae_terminal_resize(m,0,1000));
    ae_terminal_init(m,8,3,width);feed(m,"12345678\033[32mZ");
    CHECK(m->screen[0][7].code=='8' && m->screen[1][0].code=='Z');
    ae_terminal_init(m,8,3,width);feed(m,"12345678\033[?1049halt\033[?1049lZ");
    CHECK(m->screen[0][7].code=='8' && m->screen[1][0].code=='Z');
    ae_terminal_init(m,8,3,width);feed(m,"\033(0X");
    CHECK(m->screen[0][0].code=='X' && m->x==1);
    feed(m,"\033[?1049h\033cQ");
    CHECK(!m->alt && m->history_count==0 && m->screen[0][0].code=='Q');
    uint32_t random=5;
    for (unsigned n=0;n<20000;++n) {
        unsigned char data[64];for (unsigned i=0;i<sizeof(data);++i) { random=random*UINT32_C(1664525)+UINT32_C(1013904223);data[i]=(unsigned char)(random>>24); }
        ae_terminal_feed(m,data,sizeof(data));
        CHECK(m->x<m->cols && m->y<m->rows && m->cols<=AE_TERM_COLS && m->rows<=AE_TERM_ROWS && m->history_count<=AE_TERM_HISTORY);
        CHECK(m->parameter_count<=16 && m->top<=m->bottom && m->bottom<m->rows);
        for (unsigned y=0;y<m->rows;++y) CHECK(ae_terminal_row(m,y));
    }
    free(m);puts("terminal model: streaming UTF-8, cells/colors/cursor, bounded history/escape parsing, alternate buffer and hostile streams passed");return 0;
}
