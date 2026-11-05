#include "grapheme.h"
#include "grapheme_data.h"
bool ae_utf8_next(const char *text,size_t size,size_t *offset,uint32_t *scalar) {
    if (!text || !offset || !scalar || *offset>=size) return false;
    const unsigned char *p=(const unsigned char *)text;size_t at=*offset;
    uint32_t code=p[at++],minimum=0;unsigned tail=0;
    if (code>=0xc2 && code<=0xdf) { code&=31u;tail=1;minimum=0x80; }
    else if (code>=0xe0 && code<=0xef) { code&=15u;tail=2;minimum=0x800; }
    else if (code>=0xf0 && code<=0xf4) { code&=7u;tail=3;minimum=0x10000; }
    else if (code>=0x80) return false;
    if (tail>size-at) return false;
    for (unsigned i=0;i<tail;++i) {
        if (p[at]<0x80 || p[at]>0xbf) return false;
        code=(code<<6)|(p[at++]&63u);
    }
    if (code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff)) return false;
    *offset=at;*scalar=code;return true;
}
static unsigned property(uint32_t code) {
    size_t lo=0,hi=sizeof(ae_grapheme_ranges)/sizeof(ae_grapheme_ranges[0]);
    while (lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if (code<ae_grapheme_ranges[mid].first) hi=mid;
        else if (code>ae_grapheme_ranges[mid].last) lo=mid+1;
        else return ae_grapheme_ranges[mid].property;
    }
    return 0;
}
struct context { unsigned previous;bool start,ep,zwj_ep,linker,ri_odd; };
static bool control(unsigned g) { return g==GB_CR || g==GB_LF || g==GB_CONTROL; }
static bool boundary(const struct context *s,unsigned p) {
    unsigned g=p&15u,i=(p>>4)&3u,a=s->previous;
    if (s->start) return true;                                           /* GB1 */
    if (a==GB_CR && g==GB_LF) return false;                               /* GB3 */
    if (control(a) || control(g)) return true;                            /* GB4/5 */
    if (a==GB_L && (g==GB_L || g==GB_V || g==GB_LV || g==GB_LVT)) return false;
    if ((a==GB_LV || a==GB_V) && (g==GB_V || g==GB_T)) return false;       /* GB6/7 */
    if ((a==GB_LVT || a==GB_T) && g==GB_T) return false;                   /* GB8 */
    if (g==GB_EXTEND || g==GB_ZWJ || g==GB_SPACINGMARK) return false;      /* GB9/9a */
    if (a==GB_PREPEND) return false;                                     /* GB9b */
    if (i==INCB_CONSONANT && s->linker) return false;                     /* GB9c, v18 */
    if ((p&64u) && a==GB_ZWJ && s->zwj_ep) return false;                  /* GB11 */
    if (a==GB_RI && g==GB_RI && s->ri_odd) return false;                  /* GB12/13 */
    return true;                                                        /* GB999 */
}
static void advance(struct context *s,unsigned p) {
    unsigned g=p&15u,i=(p>>4)&3u;
    s->zwj_ep=g==GB_ZWJ && s->ep;
    s->ep=(p&64u)!=0 || (g==GB_EXTEND && s->ep);
    s->linker=i==INCB_LINKER || (i==INCB_EXTEND && s->linker);
    s->ri_odd=g==GB_RI && !s->ri_odd;s->previous=g;s->start=false;
}
static bool scan(const char *text,size_t size,size_t *output,size_t *count) {
    size_t at=0,n=0;struct context state={.start=true};
    while (at<size) {
        size_t original=at;uint32_t scalar;
        if (!ae_utf8_next(text,size,&at,&scalar)) return false;
        unsigned p=property(scalar);
        if (boundary(&state,p)) { if (n==SIZE_MAX) return false;if (output) output[n]=original;++n; }
        advance(&state,p);
    }
    if (n==SIZE_MAX) return false;
    if (output) output[n]=size;
    *count=n+1;return true;                                               /* GB2 */
}
bool ae_text_graphemes(const char *text,size_t size,size_t *output,size_t capacity,size_t *count) {
    if (!count) return false;
    *count=0;
    if ((!text && size) || (!output && capacity) || size==SIZE_MAX) return false;
    size_t needed;
    if (!scan(text,size,NULL,&needed)) return false;
    *count=needed;
    if (!output) return true;
    if (capacity<needed) return false;
    return scan(text,size,output,count);
}
