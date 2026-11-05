#include "paste.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static bool decode(const unsigned char *p,size_t available,uint32_t *code,size_t *length) {
    if (!available) return false;
    uint32_t c=p[0],minimum=0;size_t n;
    if (c<0x80) n=1;
    else if (c>=0xc2 && c<=0xdf) { n=2;c&=31;minimum=0x80; }
    else if (c>=0xe0 && c<=0xef) { n=3;c&=15;minimum=0x800; }
    else if (c>=0xf0 && c<=0xf4) { n=4;c&=7;minimum=0x10000; }
    else return false;
    if (n>available) return false;
    for (size_t i=1;i<n;++i) { if (p[i]<0x80 || p[i]>0xbf) return false;c=(c<<6)|(p[i]&63u); }
    if (c<minimum || c>0x10ffff || (c>=0xd800 && c<=0xdfff)) return false;
    *code=c;*length=n;return true;
}
enum ae_term_paste_result ae_terminal_paste_set(struct ae_terminal_paste *paste,const void *bytes,size_t size) {
    if (!paste || (!bytes && size)) return AE_TERM_PASTE_UTF8;
    if (!size) return AE_TERM_PASTE_EMPTY;
    if (size>AE_TERM_PASTE_BYTES) return AE_TERM_PASTE_TOO_LARGE;
    const unsigned char *p=bytes;struct ae_terminal_paste next={.lines=1};
    for (size_t i=0;i<size;) {
        uint32_t code;size_t n;if (!decode(p+i,size-i,&code,&n)) return AE_TERM_PASTE_UTF8;
        if ((code<32 && code!='\n' && code!='\r' && code!='\t') || (code>=0x7f && code<=0x9f) ||
                (code>=0x202a && code<=0x202e) || (code>=0x2066 && code<=0x2069)) return AE_TERM_PASTE_CONTROL;
        if (code=='\r') { next.text[next.size++]='\n';++next.lines;next.review=true;i+=n;if (i<size && p[i]=='\n') ++i; }
        else {
            memcpy(next.text+next.size,p+i,n);next.size+=n;i+=n;
            if (code=='\n') { ++next.lines;next.review=true; }
            if (code=='\t' || code==0x2028 || code==0x2029) next.review=true;
        }
    }
    next.text[next.size]=0;*paste=next;return AE_TERM_PASTE_OK;
}
void ae_terminal_paste_clear(struct ae_terminal_paste *paste) { memset(paste,0,sizeof(*paste)); }
size_t ae_terminal_paste_preview(const struct ae_terminal_paste *paste,size_t offset,unsigned columns,ae_term_width width,char line[AE_TERM_PREVIEW_BYTES]) {
    if (!paste || !line) return offset;
    line[0]=0;if (offset>=paste->size || columns<16 || columns>120) return offset;
    size_t at=offset,out=0;unsigned cells=0;
    while (at<paste->size) {
        uint32_t code;size_t n;if (!decode((const unsigned char *)paste->text+at,paste->size-at,&code,&n)) return offset;
        char escape[16];const char *text=paste->text+at;size_t bytes=n;int advance=width ? width(code) : 1;
        if (code=='\\' || code=='\t' || code=='\n') {
            escape[0]='\\';escape[1]=code=='\\' ? '\\' : code=='\t' ? 't' : 'n';text=escape;bytes=2;advance=2;
        } else if (advance<1 || advance>2 || code==0x2028 || code==0x2029) {
            int count=snprintf(escape,sizeof(escape),"\\u{%" PRIx32 "}",code);
            if (count<1 || (size_t)count>=sizeof(escape)) return offset;
            text=escape;bytes=(size_t)count;advance=count;
        }
        if (cells+(unsigned)advance>columns || bytes>=AE_TERM_PREVIEW_BYTES-out) break;
        memcpy(line+out,text,bytes);out+=bytes;cells+=(unsigned)advance;at+=n;
        if (code=='\n') break;
    }
    line[out]=0;return at;
}
