#include "model.h"
#include <string.h>
enum { NORMAL, ESCAPE, CSI, STRING, STRING_ESCAPE, ESC_INTERMEDIATE };
static const uint32_t colors[16]={0x10141c,0xd66b78,0x50c878,0xe6c875,0x7ca8e8,0xc08bda,0x6bcbd0,0xdce4ef,
    0x69768c,0xff8b98,0x8be3a0,0xffe297,0xa2c7ff,0xe4b1f2,0x92e6eb,0xffffff};
static struct ae_term_cell blank(const struct ae_terminal *m) { return (struct ae_term_cell){.code=' ',.fg=m->fg,.bg=m->bg,.width=1}; }
static void erase(struct ae_terminal *m,struct ae_term_cell *row,unsigned first,unsigned last) {
    if (first<last && first && row[first].width==0) row[first-1]=blank(m);
    if (last && last<AE_TERM_COLS && row[last-1].width==2) row[last]=blank(m);
    for (unsigned x=first;x<last;++x) row[x]=blank(m);
}
static void reset(struct ae_terminal *m) {
    m->fg=colors[7];m->bg=colors[0];m->style=0;m->x=m->y=0;m->top=0;m->bottom=m->rows-1;
    m->cursor=true;m->wrap=false;m->application_cursor=false;
    for (unsigned y=0;y<AE_TERM_ROWS;++y) erase(m,m->screen[y],0,AE_TERM_COLS);
}
void ae_terminal_init(struct ae_terminal *m,unsigned cols,unsigned rows,ae_term_width width) {
    memset(m,0,sizeof(*m));m->cols=cols>=2 && cols<=AE_TERM_COLS ? cols : 80;
    m->rows=rows>=2 && rows<=AE_TERM_ROWS ? rows : 24;m->width=width;reset(m);
}
static void remember(struct ae_terminal *m) {
    if (m->alt) return;
    unsigned at=(m->history_head+m->history_count)%AE_TERM_HISTORY;
    memcpy(m->history[at],m->screen[0],sizeof(m->screen[0]));
    if (m->history_count<AE_TERM_HISTORY) ++m->history_count;
    else m->history_head=(m->history_head+1)%AE_TERM_HISTORY;
    if (m->view && m->view<m->history_count) ++m->view;
}
static void scroll_up(struct ae_terminal *m) {
    if (!m->top && m->bottom==m->rows-1) remember(m);
    memmove(m->screen[m->top],m->screen[m->top+1],(m->bottom-m->top)*sizeof(m->screen[0]));
    erase(m,m->screen[m->bottom],0,AE_TERM_COLS);
}
static void linefeed(struct ae_terminal *m) {
    m->wrap=false;
    if (m->y==m->bottom) scroll_up(m);
    else if (m->y+1<m->rows) ++m->y;
}
bool ae_terminal_resize(struct ae_terminal *m,unsigned cols,unsigned rows) {
    if (cols<2 || cols>AE_TERM_COLS || rows<2 || rows>AE_TERM_ROWS) return false;
    while (m->y>=rows) { m->top=0;m->bottom=m->rows-1;scroll_up(m);--m->y; }
    if (rows>m->rows) for (unsigned y=m->rows;y<rows;++y) erase(m,m->screen[y],0,AE_TERM_COLS);
    m->rows=rows;m->cols=cols;m->top=0;m->bottom=rows-1;m->wrap=m->normal_wrap=false;
    if (m->x>=cols) m->x=cols-1;
    ++m->sequence;return true;
}
static void clean_cell(struct ae_terminal *m,unsigned x) {
    struct ae_term_cell *r=m->screen[m->y];
    if (r[x].width==0 && x) r[x-1]=blank(m);
    if (r[x].width==2 && x+1<AE_TERM_COLS) r[x+1]=blank(m);
    r[x]=blank(m);
}
static void character(struct ae_terminal *m,uint32_t code) {
    int width=m->width ? m->width(code) : 1;
    if (width==0) {
        unsigned x=m->wrap ? m->x : m->x ? m->x-1 : 0;
        if (!m->screen[m->y][x].width && x) --x;
        struct ae_term_cell *c=&m->screen[m->y][x];
        if (c->count<3) c->marks[c->count++]=code;else ++m->unsupported;
        return;
    }
    if (width<0 || width>2) { code=0xfffd;width=1; }
    if (m->wrap || (width==2 && m->x+1>=m->cols)) { m->x=0;linefeed(m); }
    clean_cell(m,m->x);
    if (width==2) clean_cell(m,m->x+1);
    m->screen[m->y][m->x]=(struct ae_term_cell){.code=code,.fg=m->fg,.bg=m->bg,.style=m->style,.width=(uint8_t)width};
    if (width==2) m->screen[m->y][m->x+1]=(struct ae_term_cell){.fg=m->fg,.bg=m->bg,.style=m->style,.width=0};
    if (m->x+(unsigned)width>=m->cols) { m->x=m->cols-1;m->wrap=true; }
    else m->x+=(unsigned)width;
}
static unsigned value(const struct ae_terminal *m,unsigned index,unsigned fallback) {
    return index<m->parameter_count && m->parameters[index] ? m->parameters[index] : fallback;
}
static uint32_t color(unsigned index) {
    if (index<16) return colors[index];
    if (index<232) { unsigned i=index-16,r=i/36,g=(i/6)%6,b=i%6;return ((r ? r*40+55 : 0)<<16)|((g ? g*40+55 : 0)<<8)|(b ? b*40+55 : 0); }
    unsigned v=8+(index-232)*10;return (v<<16)|(v<<8)|v;
}
static void sgr(struct ae_terminal *m) {
    for (unsigned i=0;i<m->parameter_count;++i) {
        unsigned p=m->parameters[i];
        if (!p) { m->fg=colors[7];m->bg=colors[0];m->style=0; }
        else if (p==1) m->style|=AE_TERM_BOLD;
        else if (p==4) m->style|=AE_TERM_UNDERLINE;
        else if (p==7) m->style|=AE_TERM_REVERSE;
        else if (p==22) m->style&=(uint8_t)~AE_TERM_BOLD;
        else if (p==24) m->style&=(uint8_t)~AE_TERM_UNDERLINE;
        else if (p==27) m->style&=(uint8_t)~AE_TERM_REVERSE;
        else if (p>=30 && p<=37) m->fg=colors[p-30];
        else if (p>=40 && p<=47) m->bg=colors[p-40];
        else if (p>=90 && p<=97) m->fg=colors[p-90+8];
        else if (p>=100 && p<=107) m->bg=colors[p-100+8];
        else if (p==39) m->fg=colors[7];
        else if (p==49) m->bg=colors[0];
        else if ((p==38 || p==48) && i+2<m->parameter_count && m->parameters[i+1]==5 && m->parameters[i+2]<256) {
            if (p==38) m->fg=color(m->parameters[i+2]);else m->bg=color(m->parameters[i+2]);i+=2;
        } else if ((p==38 || p==48) && i+4<m->parameter_count && m->parameters[i+1]==2) {
            unsigned r=m->parameters[i+2],g=m->parameters[i+3],b=m->parameters[i+4];
            if (r<256 && g<256 && b<256) { uint32_t c=(r<<16)|(g<<8)|b;if (p==38) m->fg=c;else m->bg=c; }
            i+=4;
        } else ++m->unsupported;
    }
}
static void control(struct ae_terminal *m,unsigned char final) {
    if (m->bad_control) { ++m->unsupported;return; }
    unsigned p=value(m,0,1);bool wrap=m->wrap;m->wrap=false;
    if (m->private_mode && (final=='h' || final=='l')) {
        m->wrap=wrap;
        bool on=final=='h';
        for (unsigned i=0;i<m->parameter_count;++i) {
            unsigned mode=m->parameters[i];
            if (mode==25) m->cursor=on;
            else if (mode==1) m->application_cursor=on;
            else if (mode==1049 && on!=m->alt) {
                if (on) { memcpy(m->alternate,m->screen,sizeof(m->screen));m->normal_x=m->x;m->normal_y=m->y;m->normal_fg=m->fg;m->normal_bg=m->bg;m->normal_style=m->style;m->normal_wrap=m->wrap;reset(m);m->alt=true;m->view=0; }
                else { memcpy(m->screen,m->alternate,sizeof(m->screen));m->alt=false;m->x=m->normal_x<m->cols ? m->normal_x : m->cols-1;m->y=m->normal_y<m->rows ? m->normal_y : m->rows-1;m->top=0;m->bottom=m->rows-1;m->fg=m->normal_fg;m->bg=m->normal_bg;m->style=m->normal_style;m->wrap=m->normal_wrap; }
            } else ++m->unsupported;
        }
    } else if (m->private_mode) ++m->unsupported;
    else if (final=='m') { m->wrap=wrap;sgr(m); }
    else if (final=='A') m->y=p>m->y ? 0 : m->y-p;
    else if (final=='B') m->y=p>=m->rows-m->y ? m->rows-1 : m->y+p;
    else if (final=='C') m->x=p>=m->cols-m->x ? m->cols-1 : m->x+p;
    else if (final=='D') m->x=p>m->x ? 0 : m->x-p;
    else if (final=='G') m->x=p>=m->cols ? m->cols-1 : p-1;
    else if (final=='d') m->y=p>=m->rows ? m->rows-1 : p-1;
    else if (final=='H' || final=='f') { m->y=p>=m->rows ? m->rows-1 : p-1;unsigned x=value(m,1,1);m->x=x>=m->cols ? m->cols-1 : x-1; }
    else if (final=='K') { p=m->parameters[0];if (p==0) erase(m,m->screen[m->y],m->x,m->cols);else if (p==1) erase(m,m->screen[m->y],0,m->x+1);else if (p==2) erase(m,m->screen[m->y],0,m->cols);else ++m->unsupported; }
    else if (final=='J') {
        p=m->parameters[0];
        if (p==2) for (unsigned y=0;y<m->rows;++y) erase(m,m->screen[y],0,m->cols);
        else if (p==3) m->history_head=m->history_count=m->view=0;
        else if (p==0) { erase(m,m->screen[m->y],m->x,m->cols);for (unsigned y=m->y+1;y<m->rows;++y) erase(m,m->screen[y],0,m->cols); }
        else if (p==1) { for (unsigned y=0;y<m->y;++y) erase(m,m->screen[y],0,m->cols);erase(m,m->screen[m->y],0,m->x+1); }
        else ++m->unsupported;
    } else if (final=='r') { unsigned bottom=value(m,1,m->rows);if (p<bottom && bottom<=m->rows) { m->top=p-1;m->bottom=bottom-1;m->x=m->y=0; }else ++m->unsupported; }
    else if (final=='s') { m->saved_x=m->x;m->saved_y=m->y; }
    else if (final=='u') { m->x=m->saved_x<m->cols ? m->saved_x : m->cols-1;m->y=m->saved_y<m->rows ? m->saved_y : m->rows-1; }
    else ++m->unsupported;
}
static void byte(struct ae_terminal *m,unsigned char b) {
    if (b==24 || b==26) { m->parser=NORMAL;m->need=0;return; }
    if (m->parser==STRING || m->parser==STRING_ESCAPE) {
        if (b==7 || (m->parser==STRING_ESCAPE && b=='\\')) m->parser=NORMAL;
        else m->parser=b==27 ? STRING_ESCAPE : STRING;
        return;
    }
    if (b==27) { m->parser=ESCAPE;m->need=0;return; }
    if (m->parser==ESC_INTERMEDIATE) {
        if (b<0x20 || b>0x2f) m->parser=NORMAL;
        return;
    }
    if (m->parser==ESCAPE) {
        m->parser=NORMAL;
        if (b=='[') { m->parser=CSI;m->parameter_count=1;memset(m->parameters,0,sizeof(m->parameters));m->private_mode=m->bad_control=false;m->control_bytes=0; }
        else if (b==']' || b=='P' || b=='_' || b=='^') { m->parser=STRING;++m->unsupported; }
        else if (b=='7') { m->saved_x=m->x;m->saved_y=m->y; }
        else if (b=='8') { m->x=m->saved_x<m->cols ? m->saved_x : m->cols-1;m->y=m->saved_y<m->rows ? m->saved_y : m->rows-1; }
        else if (b=='c') { reset(m);m->alt=false;m->history_head=m->history_count=m->view=0;m->saved_x=m->saved_y=0; }
        else if (b=='D') linefeed(m);
        else if (b>=0x20 && b<=0x2f) { m->parser=ESC_INTERMEDIATE;++m->unsupported; }
        else ++m->unsupported;
        return;
    }
    if (m->parser==CSI) {
        if (++m->control_bytes>256) m->bad_control=true;
        if (b==27) { m->parser=ESCAPE;return; }
        if (b>=0x40 && b<=0x7e) { control(m,b);m->parser=NORMAL;return; }
        if (b=='?' && m->control_bytes==1) m->private_mode=true;
        else if (b==';') { if (m->parameter_count<16) ++m->parameter_count;else m->bad_control=true; }
        else if (b>='0' && b<='9') { unsigned *p=&m->parameters[m->parameter_count-1];if (*p>999) m->bad_control=true;else *p=*p*10u+(unsigned)(b-'0'); }
        else m->bad_control=true;
        return;
    }
    if (m->need) {
        if (b>=0x80 && b<=0xbf) {
            m->unicode=(m->unicode<<6)|(b&63u);
            if (!--m->need) { uint32_t c=m->unicode;character(m,c<m->minimum || c>0x10ffff || (c>=0xd800 && c<=0xdfff) ? 0xfffd : c); }
            return;
        }
        m->need=0;character(m,0xfffd);
    }
    if (b==27) m->parser=ESCAPE;
    else if (b=='\r') { m->x=0;m->wrap=false; }
    else if (b=='\n' || b=='\v' || b=='\f') linefeed(m);
    else if (b=='\b') { if (m->wrap) m->wrap=false;else if (m->x) --m->x; }
    else if (b=='\t') { unsigned x=(m->x/8+1)*8;m->x=x>=m->cols ? m->cols-1 : x;m->wrap=false; }
    else if (b>=0x20 && b<0x7f) character(m,b);
    else if (b>=0xc2 && b<=0xf4) { m->need=b<0xe0 ? 1u : b<0xf0 ? 2u : 3u;m->minimum=b<0xe0 ? 0x80u : b<0xf0 ? 0x800u : 0x10000u;m->unicode=b&(b<0xe0 ? 31u : b<0xf0 ? 15u : 7u); }
    else if (b>=0x80) character(m,0xfffd);
}
void ae_terminal_feed(struct ae_terminal *m,const void *bytes,size_t count) {
    const unsigned char *p=bytes;for (size_t i=0;i<count;++i) byte(m,p[i]);if (count) ++m->sequence;
}
const struct ae_term_cell *ae_terminal_row(const struct ae_terminal *m,unsigned row) {
    if (row>=m->rows) return NULL;
    unsigned offset=m->alt ? 0 : m->view;
    if (row<offset) return m->history[(m->history_head+m->history_count-offset+row)%AE_TERM_HISTORY];
    return m->screen[row-offset];
}
void ae_terminal_scroll(struct ae_terminal *m,int lines) {
    if (m->alt) return;
    if (lines>0) { unsigned add=(unsigned)lines;m->view=add>m->history_count-m->view ? m->history_count : m->view+add; }
    else { unsigned sub=(unsigned)(-(int64_t)lines);m->view=sub>m->view ? 0 : m->view-sub; }
    ++m->sequence;
}
