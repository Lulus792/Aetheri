#include "workspace.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
struct validation {bool nodes[AE_TERM_NODES],slots[AE_TERM_PANES];uint64_t ids[AE_TERM_NODES+AE_TERM_TABS];unsigned count;};
static bool identity(struct validation *v,uint64_t id,uint64_t serial) {
    if(!id || id>serial || v->count>=AE_TERM_NODES+AE_TERM_TABS) return false;
    for(unsigned i=0;i<v->count;++i) if(v->ids[i]==id) return false;
    v->ids[v->count++]=id;return true;
}
static bool validate_tree(const struct ae_terminal_workspace *w,unsigned index,struct validation *v,unsigned focus,bool *found) {
    if(index>=AE_TERM_NODES || v->nodes[index]) return false;
    const struct ae_terminal_partition_node *n=&w->nodes[index];v->nodes[index]=true;
    if(!identity(v,n->id,w->serial)) return false;
    if(n->kind==AE_PART_LEAF) {
        if(n->slot>=AE_TERM_PANES || v->slots[n->slot]) return false;
        v->slots[n->slot]=true;if(n->slot==focus) *found=true;return true;
    }
    return (n->kind==AE_PART_COLUMNS || n->kind==AE_PART_ROWS) && isfinite(n->ratio) && n->ratio>=.1f && n->ratio<=.9f &&
        validate_tree(w,n->first,v,focus,found) && validate_tree(w,n->second,v,focus,found);
}
bool ae_terminal_workspace_valid(const struct ae_terminal_workspace *w) {
    if(!w || w->count>AE_TERM_TABS || (w->count?w->active>=w->count:w->active!=0)) return false;
    struct validation v={0};
    for(unsigned i=0;i<w->count;++i) {
        bool found=false;if(!identity(&v,w->tabs[i].id,w->serial) || !validate_tree(w,w->tabs[i].root,&v,w->tabs[i].focus,&found) || !found) return false;
    }
    for(unsigned i=0;i<AE_TERM_NODES;++i) if(v.nodes[i]!=(w->nodes[i].kind!=AE_PART_FREE)) return false;
    return true;
}
void ae_terminal_workspace_init(struct ae_terminal_workspace *w) {if(w) memset(w,0,sizeof(*w));}
static unsigned leaf(const struct ae_terminal_workspace *w,unsigned slot) {
    for(unsigned i=0;i<AE_TERM_NODES;++i) if(w->nodes[i].kind==AE_PART_LEAF && w->nodes[i].slot==slot) return i;
    return AE_TERM_NODES;
}
static unsigned unused(const struct ae_terminal_workspace *w,unsigned except) {
    for(unsigned i=0;i<AE_TERM_NODES;++i) if(i!=except && w->nodes[i].kind==AE_PART_FREE) return i;
    return AE_TERM_NODES;
}
bool ae_terminal_workspace_tab(struct ae_terminal_workspace *w,unsigned slot) {
    if(!ae_terminal_workspace_valid(w) || slot>=AE_TERM_PANES || w->count>=AE_TERM_TABS ||
       leaf(w,slot)!=AE_TERM_NODES || w->serial>UINT64_MAX-2) return false;
    unsigned index=unused(w,AE_TERM_NODES);if(index==AE_TERM_NODES) return false;
    struct ae_terminal_workspace next=*w;
    next.tabs[next.count]=(struct ae_terminal_tab){.root=index,.focus=slot,.id=++next.serial};
    next.nodes[index]=(struct ae_terminal_partition_node){.kind=AE_PART_LEAF,.slot=slot,.id=++next.serial};
    next.active=next.count++;*w=next;return true;
}
unsigned ae_terminal_workspace_focused(const struct ae_terminal_workspace *w) {
    return ae_terminal_workspace_valid(w) && w->count?w->tabs[w->active].focus:AE_TERM_PANES;
}
bool ae_terminal_workspace_split(struct ae_terminal_workspace *w,unsigned slot,bool columns) {
    if(!ae_terminal_workspace_valid(w) || !w->count || slot>=AE_TERM_PANES || leaf(w,slot)!=AE_TERM_NODES || w->serial>UINT64_MAX-2) return false;
    unsigned original=leaf(w,w->tabs[w->active].focus),first=unused(w,AE_TERM_NODES),second=unused(w,first);
    if(original==AE_TERM_NODES || first==AE_TERM_NODES || second==AE_TERM_NODES) return false;
    struct ae_terminal_workspace next=*w;
    next.nodes[first]=next.nodes[original];
    next.nodes[second]=(struct ae_terminal_partition_node){.kind=AE_PART_LEAF,.slot=slot,.id=++next.serial};
    next.nodes[original]=(struct ae_terminal_partition_node){.kind=columns?AE_PART_COLUMNS:AE_PART_ROWS,.first=first,.second=second,.id=++next.serial,.ratio=.5f};
    next.tabs[next.active].focus=slot;*w=next;return true;
}
static bool has(const struct ae_terminal_workspace *w,unsigned node,unsigned slot) {
    const struct ae_terminal_partition_node *n=&w->nodes[node];
    return n->kind==AE_PART_LEAF?n->slot==slot:has(w,n->first,slot) || has(w,n->second,slot);
}
static unsigned first_slot(const struct ae_terminal_workspace *w,unsigned node) {
    return w->nodes[node].kind==AE_PART_LEAF?w->nodes[node].slot:first_slot(w,w->nodes[node].first);
}
bool ae_terminal_workspace_remove(struct ae_terminal_workspace *w,unsigned slot) {
    if(!ae_terminal_workspace_valid(w)) return false;
    unsigned index=leaf(w,slot);if(index==AE_TERM_NODES) return false;
    struct ae_terminal_workspace next=*w;unsigned tab=0;
    while(tab<next.count && !has(&next,next.tabs[tab].root,slot)) ++tab;
    if(tab==next.count) return false;
    if(next.tabs[tab].root==index) {
        for(unsigned i=tab+1;i<next.count;++i) next.tabs[i-1]=next.tabs[i];
        next.tabs[--next.count]=(struct ae_terminal_tab){0};
        if(!next.count) next.active=0;
        else if(next.active>tab || next.active==next.count) --next.active;
    } else {
        unsigned parent=AE_TERM_NODES;
        for(unsigned i=0;i<AE_TERM_NODES;++i) if((next.nodes[i].kind==AE_PART_COLUMNS || next.nodes[i].kind==AE_PART_ROWS) &&
            (next.nodes[i].first==index || next.nodes[i].second==index)) {parent=i;break;}
        if(parent==AE_TERM_NODES) return false;
        unsigned sibling=next.nodes[parent].first==index?next.nodes[parent].second:next.nodes[parent].first;
        if(next.tabs[tab].root==parent) next.tabs[tab].root=sibling;
        else for(unsigned i=0;i<AE_TERM_NODES;++i) if(next.nodes[i].kind==AE_PART_COLUMNS || next.nodes[i].kind==AE_PART_ROWS) {
            if(next.nodes[i].first==parent) next.nodes[i].first=sibling;
            else if(next.nodes[i].second==parent) next.nodes[i].second=sibling;
        }
        next.nodes[parent]=(struct ae_terminal_partition_node){0};
        if(next.tabs[tab].focus==slot) next.tabs[tab].focus=first_slot(&next,sibling);
    }
    next.nodes[index]=(struct ae_terminal_partition_node){0};
    if(!ae_terminal_workspace_valid(&next)) return false;
    *w=next;return true;
}
bool ae_terminal_workspace_select(struct ae_terminal_workspace *w,unsigned tab) {
    if(!ae_terminal_workspace_valid(w) || tab>=w->count) return false;
    w->active=tab;return true;
}
bool ae_terminal_workspace_focus(struct ae_terminal_workspace *w,unsigned slot) {
    if(!ae_terminal_workspace_valid(w) || !w->count || !has(w,w->tabs[w->active].root,slot)) return false;
    w->tabs[w->active].focus=slot;return true;
}
static void order(const struct ae_terminal_workspace *w,unsigned node,unsigned slots[AE_TERM_PANES],unsigned *count) {
    const struct ae_terminal_partition_node *n=&w->nodes[node];
    if(n->kind==AE_PART_LEAF) slots[(*count)++]=n->slot;
    else {order(w,n->first,slots,count);order(w,n->second,slots,count);}
}
unsigned ae_terminal_workspace_order(const struct ae_terminal_workspace *w,unsigned slots[AE_TERM_PANES]) {
    if(!slots || !ae_terminal_workspace_valid(w) || !w->count) return 0;
    unsigned count=0;order(w,w->tabs[w->active].root,slots,&count);return count;
}
struct minimum {float w,h;};
static struct minimum measure(const struct ae_terminal_workspace *w,unsigned node,float mw,float mh,float gap) {
    const struct ae_terminal_partition_node *n=&w->nodes[node];
    if(n->kind==AE_PART_LEAF) return (struct minimum){mw,mh};
    struct minimum a=measure(w,n->first,mw,mh,gap),b=measure(w,n->second,mw,mh,gap);
    return n->kind==AE_PART_COLUMNS?(struct minimum){a.w+b.w+gap,fmaxf(a.h,b.h)}:(struct minimum){fmaxf(a.w,b.w),a.h+b.h+gap};
}
static void arrange(const struct ae_terminal_workspace *w,unsigned node,struct ae_terminal_area r,float mw,float mh,float gap,struct ae_terminal_workspace_layout *out) {
    const struct ae_terminal_partition_node *n=&w->nodes[node];out->bounds[node]=r;
    if(n->kind==AE_PART_LEAF) {out->panes[n->slot]=r;out->visible|=1u<<n->slot;return;}
    struct minimum a=measure(w,n->first,mw,mh,gap),b=measure(w,n->second,mw,mh,gap);
    bool columns=n->kind==AE_PART_COLUMNS;float extent=(columns?r.w:r.h)-gap;
    float amount=fmaxf(columns?a.w:a.h,fminf(extent-(columns?b.w:b.h),extent*n->ratio));
    struct ae_terminal_area first=r,second=r,divider=r;
    if(columns) {first.w=amount;second.x+=amount+gap;second.w=extent-amount;divider.x+=amount;divider.w=gap;}
    else {first.h=amount;second.y+=amount+gap;second.h=extent-amount;divider.y+=amount;divider.h=gap;}
    out->dividers[node]=divider;arrange(w,n->first,first,mw,mh,gap,out);arrange(w,n->second,second,mw,mh,gap,out);
}
bool ae_terminal_workspace_layout(const struct ae_terminal_workspace *w,struct ae_terminal_area r,float mw,float mh,float gap,struct ae_terminal_workspace_layout *out) {
    if(!out || !ae_terminal_workspace_valid(w) || !w->count || !isfinite(r.x) || !isfinite(r.y) || !isfinite(r.w) || !isfinite(r.h) ||
       !isfinite(mw) || !isfinite(mh) || !isfinite(gap) || !isfinite(r.x+r.w) || !isfinite(r.y+r.h) ||
       mw<=0 || mh<=0 || gap<0 || r.w<=0 || r.h<=0) return false;
    struct minimum m=measure(w,w->tabs[w->active].root,mw,mh,gap);if(!isfinite(m.w) || !isfinite(m.h) || r.w<m.w || r.h<m.h) return false;
    struct ae_terminal_workspace_layout next={0};arrange(w,w->tabs[w->active].root,r,mw,mh,gap,&next);*out=next;return true;
}
bool ae_terminal_workspace_ratio(struct ae_terminal_workspace *w,unsigned node,float ratio) {
    if(!ae_terminal_workspace_valid(w) || node>=AE_TERM_NODES || !isfinite(ratio) || ratio<.1f || ratio>.9f ||
       (w->nodes[node].kind!=AE_PART_COLUMNS && w->nodes[node].kind!=AE_PART_ROWS)) return false;
    w->nodes[node].ratio=ratio;return true;
}
