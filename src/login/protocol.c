#include "protocol.h"
#include "../text/grapheme.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct ae_login {
    enum ae_login_phase phase;
    enum ae_login_prompt prompt;
    enum ae_login_operation operation;
    int authentication_error;
    uint8_t tx[AE_LOGIN_WIRE_MAX+4],rx[AE_LOGIN_WIRE_MAX+4];
    size_t tx_size,tx_sent,rx_size,rx_need,text_size;
    char text[AE_LOGIN_TEXT_MAX+1];
};
static void wipe(void *pointer,size_t size) {volatile unsigned char *p=pointer;while(size--)*p++=0;}
static void clear_text(struct ae_login *s) {wipe(s->text,sizeof(s->text));s->text_size=0;s->prompt=AE_LOGIN_NO_PROMPT;s->authentication_error=0;}
int ae_login_create(struct ae_login **out) {
    if(!out || *out)return -EINVAL;
    struct ae_login *s=calloc(1,sizeof(*s));if(!s)return -ENOMEM;
    *out=s;return 0;
}
void ae_login_close(struct ae_login **out) {if(out && *out){struct ae_login *s=*out;*out=NULL;wipe(s,sizeof(*s));free(s);}}
void ae_login_interrupted(struct ae_login *s) {
    if(!s)return;
    if(s->phase!=AE_LOGIN_START_ACKNOWLEDGED)s->phase=AE_LOGIN_UNKNOWN;
    wipe(s->tx,sizeof(s->tx));wipe(s->rx,sizeof(s->rx));s->tx_size=s->tx_sent=s->rx_size=s->rx_need=0;clear_text(s);
}
static bool text_ok(const char *text,size_t size,bool username) {
    size_t at=0;uint32_t cp;
    if(!text && size)return false;
    while(at<size)if(!ae_utf8_next(text,size,&at,&cp) || !cp || (username && (cp<32 || cp==127)))return false;
    return true;
}
struct writer {uint8_t *p;size_t used;};
static void literal(struct writer *w,const char *text) {size_t n=strlen(text);memcpy(w->p+w->used,text,n);w->used+=n;}
static void quoted(struct writer *w,const char *text,size_t size) {
    static const char digits[]="0123456789abcdef";
    w->p[w->used++]='"';
    for(size_t i=0;i<size;++i){
        unsigned char c=(unsigned char)text[i];
        if(c=='"' || c=='\\'){w->p[w->used++]='\\';w->p[w->used++]=c;}
        else if(c<32){literal(w,"\\u00");w->p[w->used++]=(uint8_t)digits[c>>4];w->p[w->used++]=(uint8_t)digits[c&15];}
        else w->p[w->used++]=c;
    }
    w->p[w->used++]='"';
}
/* Input budgets below make every whole request fit, including JSON escaping. */
_Static_assert(AE_LOGIN_RESPONSE_MAX*6+256<10240,"backend worker budget");
_Static_assert(AE_LOGIN_RESPONSE_MAX*6+256<AE_LOGIN_WIRE_MAX,"login wire budget");
static int request(struct ae_login *s,enum ae_login_operation op,const char *text,size_t size) {
    struct writer w={.p=s->tx+4};
    if(op==AE_LOGIN_CREATE){literal(&w,"{\"type\":\"create_session\",\"username\":");quoted(&w,text,size);}
    else if(op==AE_LOGIN_ANSWER){literal(&w,"{\"type\":\"post_auth_message_response\",\"response\":");if(text)quoted(&w,text,size);else literal(&w,"null");}
    else if(op==AE_LOGIN_CANCEL)literal(&w,"{\"type\":\"cancel_session\"");
    else literal(&w,"{\"type\":\"start_session\",\"cmd\":[\"/usr/libexec/aetheri/aetheri-session\"],\"env\":[\"XDG_SESSION_TYPE=wayland\",\"XDG_CURRENT_DESKTOP=Aetheri\",\"XDG_SESSION_DESKTOP=Aetheri\"]");
    literal(&w,"}");uint32_t n=(uint32_t)w.used;memcpy(s->tx,&n,4);
    s->tx_size=w.used+4;s->tx_sent=0;s->operation=op;s->phase=AE_LOGIN_SENDING;clear_text(s);return 0;
}
int ae_login_begin(struct ae_login *s,const char *text,size_t size) {
    if(!s || !text || !size || size>256 || !text_ok(text,size,true))return -EINVAL;
    if(s->phase!=AE_LOGIN_IDLE)return -EBUSY;
    return request(s,AE_LOGIN_CREATE,text,size);
}
int ae_login_answer(struct ae_login *s,const char *text,size_t size) {
    if(!s || size>AE_LOGIN_RESPONSE_MAX || !text_ok(text,size,false))return -EINVAL;
    if(s->phase!=AE_LOGIN_PROMPT)return -EBUSY;
    bool information=s->prompt==AE_LOGIN_INFO || s->prompt==AE_LOGIN_ERROR;
    if((information && text) || (!information && !text))return -EINVAL;
    return request(s,AE_LOGIN_ANSWER,text,size);
}
int ae_login_cancel(struct ae_login *s) {
    if(!s)return -EINVAL;
    if(s->phase!=AE_LOGIN_PROMPT && s->phase!=AE_LOGIN_AUTHENTICATED && s->phase!=AE_LOGIN_DENIED)return -EBUSY;
    return request(s,AE_LOGIN_CANCEL,NULL,0);
}
int ae_login_start(struct ae_login *s) {if(!s)return -EINVAL;if(s->phase!=AE_LOGIN_AUTHENTICATED)return -EBUSY;return request(s,AE_LOGIN_START,NULL,0);}
int ae_login_status(const struct ae_login *s,struct ae_login_status *out) {
    if(!s || !out)return -EINVAL;
    *out=(struct ae_login_status){.phase=s->phase,.prompt=s->prompt,.operation=s->operation,.text=s->text,.text_size=s->text_size,.authentication_error=s->authentication_error};return 0;
}
int ae_login_output(const struct ae_login *s,const uint8_t **bytes,size_t *size) {
    if(!s || !bytes || !size)return -EINVAL;
    if(s->phase!=AE_LOGIN_SENDING)return -EBUSY;
    *bytes=s->tx+s->tx_sent;*size=s->tx_size-s->tx_sent;return 0;
}
int ae_login_sent(struct ae_login *s,size_t size) {
    if(!s || !size)return -EINVAL;
    if(s->phase!=AE_LOGIN_SENDING || size>s->tx_size-s->tx_sent)return -EBUSY;
    wipe(s->tx+s->tx_sent,size);s->tx_sent+=size;
    if(s->tx_sent==s->tx_size){s->tx_size=s->tx_sent=0;s->phase=AE_LOGIN_WAITING;}
    return 0;
}
struct parser {const uint8_t *p;size_t at,size;};
static void space(struct parser *p) {while(p->at<p->size && (p->p[p->at]==' ' || p->p[p->at]=='\t' || p->p[p->at]=='\r' || p->p[p->at]=='\n'))++p->at;}
static bool byte(struct parser *p,unsigned char c) {space(p);if(p->at==p->size || p->p[p->at]!=c)return false;++p->at;return true;}
static bool hex4(struct parser *p,uint32_t *out) {
    uint32_t n=0;
    for(unsigned i=0;i<4;++i){if(p->at==p->size)return false;unsigned c=p->p[p->at++],v;
        if(c>='0' && c<='9')v=c-'0';else if(c>='a' && c<='f')v=c-'a'+10;else if(c>='A' && c<='F')v=c-'A'+10;else return false;n=n*16+v;}
    *out=n;return true;
}
static bool scalar(char *out,size_t cap,size_t *size,uint32_t cp) {
    uint8_t b[4];size_t n;
    if(!cp || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return false;
    if(cp<128){n=1;b[0]=(uint8_t)cp;}
    else if(cp<2048){n=2;b[0]=(uint8_t)(0xc0|(cp>>6));b[1]=(uint8_t)(0x80|(cp&63));}
    else if(cp<65536){n=3;b[0]=(uint8_t)(0xe0|(cp>>12));b[1]=(uint8_t)(0x80|((cp>>6)&63));b[2]=(uint8_t)(0x80|(cp&63));}
    else {n=4;b[0]=(uint8_t)(0xf0|(cp>>18));b[1]=(uint8_t)(0x80|((cp>>12)&63));b[2]=(uint8_t)(0x80|((cp>>6)&63));b[3]=(uint8_t)(0x80|(cp&63));}
    if(n>=cap-*size)return false;
    memcpy(out+*size,b,n);*size+=n;return true;
}
static bool string(struct parser *p,char *out,size_t cap) {
    size_t size=0;if(!byte(p,'"'))return false;
    while(p->at<p->size){
        unsigned char c=p->p[p->at++];
        if(c=='"'){out[size]=0;return text_ok(out,size,false);}
        if(c<32 || size+1>=cap)return false;
        if(c!='\\'){out[size++]=(char)c;continue;}
        if(p->at==p->size)return false;
        c=p->p[p->at++];
        if(c=='"' || c=='\\' || c=='/')out[size++]=(char)c;
        else if(c=='b')out[size++]='\b';else if(c=='f')out[size++]='\f';else if(c=='n')out[size++]='\n';else if(c=='r')out[size++]='\r';else if(c=='t')out[size++]='\t';
        else if(c=='u'){
            uint32_t cp,low;if(!hex4(p,&cp))return false;
            if(cp>=0xd800 && cp<=0xdbff){
                if(p->size-p->at<2 || p->p[p->at++]!='\\' || p->p[p->at++]!='u' || !hex4(p,&low) || low<0xdc00 || low>0xdfff)return false;
                cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);
            }
            if(!scalar(out,cap,&size,cp))return false;
        }else return false;
    }
    return false;
}
struct reply {char type[32],style[32],error[32],text[AE_LOGIN_TEXT_MAX+1];unsigned seen;};
static bool parse(const uint8_t *data,size_t size,struct reply *r) {
    struct parser p={.p=data,.size=size};char key[32],value[AE_LOGIN_TEXT_MAX+1];bool ok=false;
    if(!byte(&p,'{'))goto done;
    for(unsigned count=0;count<4;++count){
        unsigned bit;char *target;size_t cap;
        if(!string(&p,key,sizeof(key)) || !byte(&p,':') || !string(&p,value,sizeof(value)))goto done;
        if(!strcmp(key,"type")){bit=1;target=r->type;cap=sizeof(r->type);}
        else if(!strcmp(key,"auth_message_type")){bit=2;target=r->style;cap=sizeof(r->style);}
        else if(!strcmp(key,"auth_message")){bit=4;target=r->text;cap=sizeof(r->text);}
        else if(!strcmp(key,"error_type")){bit=8;target=r->error;cap=sizeof(r->error);}
        else if(!strcmp(key,"description")){bit=16;target=r->text;cap=sizeof(r->text);}
        else goto done;
        size_t n=strlen(value);if(r->seen&bit || n>=cap)goto done;
        memcpy(target,value,n+1);r->seen|=bit;wipe(value,sizeof(value));space(&p);
        if(p.at<p.size && p.p[p.at]=='}'){++p.at;space(&p);ok=p.at==p.size;goto done;}
        if(!byte(&p,','))goto done;
    }
done:
    wipe(value,sizeof(value));return ok;
}
static bool apply(struct ae_login *s) {
    struct reply r={0};bool ok=false;
    if(!parse(s->rx+4,s->rx_need-4,&r))goto done;
    if(r.seen==1 && !strcmp(r.type,"success")){
        s->phase=s->operation==AE_LOGIN_CANCEL?AE_LOGIN_IDLE:s->operation==AE_LOGIN_START?AE_LOGIN_START_ACKNOWLEDGED:AE_LOGIN_AUTHENTICATED;ok=true;
    }else if(r.seen==7 && !strcmp(r.type,"auth_message") && (s->operation==AE_LOGIN_CREATE || s->operation==AE_LOGIN_ANSWER)){
        if(!strcmp(r.style,"visible"))s->prompt=AE_LOGIN_VISIBLE;else if(!strcmp(r.style,"secret"))s->prompt=AE_LOGIN_SECRET;
        else if(!strcmp(r.style,"info"))s->prompt=AE_LOGIN_INFO;else if(!strcmp(r.style,"error"))s->prompt=AE_LOGIN_ERROR;else goto done;
        memcpy(s->text,r.text,strlen(r.text)+1);s->text_size=strlen(r.text);s->phase=AE_LOGIN_PROMPT;ok=true;
    }else if(r.seen==25 && !strcmp(r.type,"error") && (!strcmp(r.error,"auth_error") || !strcmp(r.error,"error"))){
        if(s->operation==AE_LOGIN_CANCEL || s->operation==AE_LOGIN_START)goto done;
        memcpy(s->text,r.text,strlen(r.text)+1);s->text_size=strlen(r.text);s->authentication_error=!strcmp(r.error,"auth_error");s->phase=AE_LOGIN_DENIED;ok=true;
    }
done:
    wipe(&r,sizeof(r));return ok;
}
int ae_login_receive(struct ae_login *s,const uint8_t *data,size_t size) {
    if(!s || (!data && size))return -EINVAL;
    if(s->phase!=AE_LOGIN_WAITING){if(size){ae_login_interrupted(s);return -EPROTO;}return -EBUSY;}
    while(size){
        size_t need=s->rx_need?s->rx_need:4,n=need-s->rx_size;if(n>size)n=size;
        memcpy(s->rx+s->rx_size,data,n);s->rx_size+=n;data+=n;size-=n;
        if(s->rx_size==4 && !s->rx_need){uint32_t length;memcpy(&length,s->rx,4);if(!length || length>AE_LOGIN_WIRE_MAX)goto bad;s->rx_need=(size_t)length+4;}
        if(s->rx_need && s->rx_size==s->rx_need){
            if(size || !apply(s))goto bad;
            wipe(s->rx,sizeof(s->rx));s->rx_size=s->rx_need=0;return 1;
        }
    }
    return 0;
bad:
    ae_login_interrupted(s);return -EPROTO;
}
