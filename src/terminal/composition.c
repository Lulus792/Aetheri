#include "composition.h"
#include "paste.h"
#include <string.h>
void ae_terminal_composition_clear(struct ae_terminal_composition *c) { if (c) memset(c,0,sizeof(*c)); }
bool ae_terminal_composition_set(struct ae_terminal_composition *c,const void *text,size_t bytes,int start,int length) {
    if (!c || (!text && bytes) || bytes>AE_TERM_COMPOSITION_BYTES || start< -1) return false;
    if (!bytes) { ae_terminal_composition_clear(c);return true; }
    struct ae_terminal_paste validated={0};
    if (ae_terminal_paste_set(&validated,text,bytes)!=AE_TERM_PASTE_OK || validated.review) return false;
    struct ae_terminal_composition next={.size=bytes};memcpy(next.text,text,bytes);
    for (size_t i=0;i<bytes;++i) if (((unsigned char)next.text[i]&0xc0u)!=0x80u) next.offsets[next.count++]=(uint16_t)i;
    next.offsets[next.count]=(uint16_t)bytes;
    if (start>=0) {
        int64_t end=length== -1 ? start : (int64_t)start+length;
        if ((unsigned)start>next.count || end<0 || (uint64_t)end>next.count) return false;
        next.caret=true;next.cursor=(unsigned)start;next.anchor=(unsigned)end;
    } else if (length< -1 || (length>=0 && (unsigned)length>next.count)) return false;
    *c=next;return true;
}
static uint32_t scalar(const struct ae_terminal_composition *c,unsigned i) {
    size_t at=c->offsets[i],end=c->offsets[i+1];uint32_t code=(unsigned char)c->text[at++];
    if (end-at) {
        code&=end-at==1 ? 31u : end-at==2 ? 15u : 7u;
        while (at<end) code=(code<<6)|((unsigned char)c->text[at++]&63u);
    }
    return code;
}
bool ae_terminal_composition_view(const struct ae_terminal_composition *c,unsigned columns,ae_term_width width,struct ae_terminal_composition_view *out) {
    if (!c || !out || !columns || columns>AE_TERM_COLS || c->size>AE_TERM_COMPOSITION_BYTES || c->count>AE_TERM_COMPOSITION_BYTES ||
        c->cursor>c->count || c->anchor>c->count) return false;
    unsigned cells[AE_TERM_COMPOSITION_BYTES+1]={0};
    for (unsigned i=0;i<c->count;++i) {
        int advance=width ? width(scalar(c,i)) : 1;
        cells[i+1]=cells[i]+(advance>=0 && advance<=2 ? (unsigned)advance : 1);
    }
    unsigned cursor=c->caret ? c->cursor : c->count;
    unsigned desired=cells[cursor]>=columns ? cells[cursor]-columns+1 : 0,first=0,last;
    while (desired && first<c->count && cells[first+1]<=desired) ++first;
    while (first && first<c->count && cells[first+1]==cells[first]) --first;
    last=first;while (last<c->count && cells[last+1]-cells[first]<=columns) ++last;
    if (last==first && last<c->count) ++last;
    unsigned low=c->cursor<c->anchor ? c->cursor : c->anchor,high=c->cursor>c->anchor ? c->cursor : c->anchor;
    if (low<first) low=first;
    if (low>last) low=last;
    if (high<first) high=first;
    if (high>last) high=last;
    *out=(struct ae_terminal_composition_view){.begin=c->offsets[first],.end=c->offsets[last],.cells=cells[last]-cells[first],
        .cursor=cells[cursor]-cells[first],.selection_begin=cells[low]-cells[first],.selection_end=cells[high]-cells[first],
        .caret=c->caret,.hidden_before=first!=0,.hidden_after=last<c->count};
    return true;
}
