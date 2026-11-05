#include "layout.h"
#include <math.h>
bool ae_files_layout_make(int w,int h,float s,bool grid,bool details,bool sidebar,bool naming,struct ae_files_layout *out) {
    if (!out || w<640 || h<420 || w>16384 || h>8192 || !isfinite(s) || s<1 || s>2) return false;
    struct ae_files_layout l={0};float width=(float)w,height=(float)h;
    l.compact=width<800*s || height<480*s;l.padding=(l.compact ? 16 : 24)*s;l.gap=12*s;
    l.sidebar_visible=sidebar && width>=640*s;
    float side=l.sidebar_visible ? 190*s : 0;
    float chrome=30*s,toolbar=48*s,heading=(l.compact ? 32 : 64)*s,footer=28*s;
    l.chrome=(struct ae_files_rect){0,0,width,chrome};l.sidebar=(struct ae_files_rect){0,chrome,side,height-chrome-footer};
    l.toolbar=(struct ae_files_rect){side,chrome,width-side,toolbar};l.status=(struct ae_files_rect){0,height-footer,width,footer};
    float left=side+l.padding,body_width=width-side-2*l.padding;
    l.inspector_visible=details && body_width>=560*s && height-footer-chrome-toolbar-heading>=260*s;float info=l.inspector_visible ? 210*s : 0;
    l.heading=(struct ae_files_rect){left,chrome+toolbar,body_width,heading};
    float top=chrome+toolbar+heading;
    if (!grid && !l.compact) top+=22*s;
    l.inspector=(struct ae_files_rect){width-l.padding-info,chrome+toolbar+heading,info,height-footer-chrome-toolbar-heading};
    float main_width=body_width-(info ? info+20*s : 0);
    l.content=(struct ae_files_rect){left,top,main_width,height-footer-top};
    float x=left,by=chrome+8*s,button=32*s,gap=6*s;
    l.back=(struct ae_files_rect){x,by,button,button};x+=button+gap;
    l.list=(struct ae_files_rect){x,by,28*s,button};x+=28*s;
    l.grid=(struct ae_files_rect){x,by,28*s,button};x+=28*s+gap;
    l.new_folder=(struct ae_files_rect){x,by,button,button};x+=button+gap;
    l.new_file=(struct ae_files_rect){x,by,button,button};x+=button+gap;
    l.details=(struct ae_files_rect){width-l.padding-button,by,button,button};
    float search_width=fminf(190*s,l.details.x-gap-x);
    l.search=(struct ae_files_rect){l.details.x-gap-search_width,by,search_width,button};
    l.more=(struct ae_files_rect){width-40*s,2*s,30*s,26*s};
    l.close=(struct ae_files_rect){12*s,5*s,20*s,20*s};l.fullscreen=(struct ae_files_rect){36*s,5*s,20*s,20*s};
    l.columns=grid ? (size_t)fmaxf(1,floorf((main_width+l.gap)/(168*s+l.gap))) : 1;
    l.cell_width=(main_width-(float)(l.columns-1)*l.gap)/(float)l.columns;
    l.cell_height=(grid ? 146 : naming ? 64 : 44)*s;
    l.compact_cards=grid && l.content.h<l.cell_height;
    if (l.compact_cards) l.cell_height=fmaxf(64*s,fminf(l.cell_height,l.content.h));
    l.row_gap=grid ? l.gap : 3*s;
    l.lines=(size_t)fmaxf(0,floorf((l.content.h+l.row_gap)/(l.cell_height+l.row_gap)));
    l.capacity=l.lines*l.columns;
    if (l.content.w<120*s || l.content.h<0 || l.search.w<70*s || !l.capacity) return false;
    *out=l;return true;
}
bool ae_files_layout_cell(const struct ae_files_layout *l,size_t slot,struct ae_files_rect *out) {
    if (!l || !out || !l->columns || slot>=l->capacity) return false;
    *out=(struct ae_files_rect){l->content.x+(float)(slot%l->columns)*(l->cell_width+l->gap),
        l->content.y+(float)(slot/l->columns)*(l->cell_height+l->row_gap),l->cell_width,l->cell_height};return true;
}
size_t ae_files_layout_reveal(const struct ae_files_layout *l,size_t count,size_t selected,size_t first) {
    if (!l || !l->columns || !l->capacity || !count || selected>=count) return 0;
    size_t last_line=(count-1)/l->columns,selected_line=selected/l->columns;
    size_t lines=l->capacity/l->columns;if (!lines) return 0;
    size_t max_first=last_line>=lines ? last_line-lines+1 : 0;
    size_t line=first/l->columns;if (line>max_first) line=max_first;
    if (selected_line<line) line=selected_line;
    else if (selected_line-line>=lines) line=selected_line-lines+1;
    return line*l->columns;
}
