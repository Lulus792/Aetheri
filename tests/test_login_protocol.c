/* Synthetic conversations only. Never receives or logs real credentials. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/login/protocol.c"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"login check %d: %s\n",__LINE__,#x);abort();}} while(0)
static struct ae_login *fresh(void) {struct ae_login *s=NULL;CHECK(!ae_login_create(&s));return s;}
static enum ae_login_phase phase(struct ae_login *s) {struct ae_login_status v;CHECK(!ae_login_status(s,&v));return v.phase;}
static void sent(struct ae_login *s) {
    const uint8_t *p=NULL;size_t n=0;CHECK(!ae_login_output(s,&p,&n) && n>4);uint32_t length;memcpy(&length,p,4);CHECK(length==n-4);
    while(n){CHECK(!ae_login_sent(s,1) && !*p);++p;--n;}
    CHECK(phase(s)==AE_LOGIN_WAITING);
}
static int reply(struct ae_login *s,const char *text) {
    size_t n=strlen(text);uint32_t length=(uint32_t)n;uint8_t bytes[AE_LOGIN_WIRE_MAX+4];CHECK(n<=AE_LOGIN_WIRE_MAX);memcpy(bytes,&length,4);memcpy(bytes+4,text,n);
    for(size_t i=0;i<n+3;++i)CHECK(!ae_login_receive(s,bytes+i,1));
    return ae_login_receive(s,bytes+n+3,1);
}
static struct ae_login *waiting(void) {struct ae_login *s=fresh();CHECK(!ae_login_begin(s,"original",8));sent(s);return s;}
static struct ae_login *prompt(const char *kind) {
    struct ae_login *s=waiting();char body[256];snprintf(body,sizeof(body),"{\"type\":\"auth_message\",\"auth_message_type\":\"%s\",\"auth_message\":\"Question\"}",kind);
    CHECK(reply(s,body)==1 && phase(s)==AE_LOGIN_PROMPT);return s;
}
static void zero(const uint8_t *p,size_t n) {for(size_t i=0;i<n;++i)CHECK(!p[i]);}
static void unknown(struct ae_login *s) {CHECK(phase(s)==AE_LOGIN_UNKNOWN && ae_login_start(s)==-EBUSY && ae_login_begin(s,"original",8)==-EBUSY);zero(s->tx,sizeof(s->tx));zero(s->rx,sizeof(s->rx));zero((uint8_t *)s->text,sizeof(s->text));ae_login_close(&s);CHECK(!s);}
static void unchanged_answer(struct ae_login *s,const char *p,size_t n) {
    struct ae_login *before=malloc(sizeof(*before));CHECK(before);memcpy(before,s,sizeof(*s));CHECK(ae_login_answer(s,p,n)<0 && !memcmp(before,s,sizeof(*s)));free(before);
}
static void malformed(void) {
    const char *cases[]={"", "{}","[]","null","{\"type\":null}","{\"type\":\"success\",}","{\"type\":\"success\"}x",
        "{\"type\":\"success\",\"type\":\"success\"}","{\"type\":\"success\",\"ty\\u0070e\":\"success\"}",
        "{\"type\":\"success\",\"extra\":\"x\"}","{\"type\":\"success\",\"auth_message\":\"x\"}",
        "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\"}",
        "{\"type\":\"auth_message\",\"auth_message_type\":\"other\",\"auth_message\":\"x\"}",
        "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"\\u0000\"}",
        "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"\\ud800\"}",
        "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"\\udc00\"}",
        "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"\\ud800\\u0041\"}",
        "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"\\x00\"}",
        "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"\xc0\x80\"}",
        "{\"type\":\"error\",\"error_type\":\"other\",\"description\":\"x\"}",
        "{\"type\":\"error\",\"error_type\":\"error\",\"description\":\"x\",\"auth_message\":\"x\"}"};
    for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i){struct ae_login *s=waiting();size_t n=strlen(cases[i]);uint32_t len=(uint32_t)n;CHECK(!ae_login_receive(s,(const uint8_t *)&len,3));int r=ae_login_receive(s,((const uint8_t *)&len)+3,1);
        if(n){CHECK(!r);CHECK(ae_login_receive(s,(const uint8_t *)cases[i],n)==-EPROTO);}else CHECK(r==-EPROTO);unknown(s);}
}
static int wire(void) {
    /* Test peer exchange over pipes. Inputs are fixed synthetic test values. */
    struct ae_login *s=fresh();CHECK(!ae_login_begin(s,"test\"\\\xc3\xa9",8));
    for(unsigned step=0;step<3;++step){
        const uint8_t *p;size_t n;CHECK(!ae_login_output(s,&p,&n));CHECK(fwrite(p,1,n,stdout)==n && !fflush(stdout));CHECK(!ae_login_sent(s,n));
        uint8_t buffer[64];
        while(phase(s)==AE_LOGIN_WAITING){size_t got=fread(buffer,1,1,stdin);CHECK(got==1 && ae_login_receive(s,buffer,got)>=0);}
        if(step==0){const char answer[]="synthetic\"\\\n\xe2\x82\xac";CHECK(!ae_login_answer(s,answer,sizeof(answer)-1));}
        else if(step==1)CHECK(!ae_login_start(s));
    }
    CHECK(phase(s)==AE_LOGIN_START_ACKNOWLEDGED);ae_login_close(&s);return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--wire"))return wire();
    CHECK(argc==1);
    struct ae_login *s=fresh(),*original=s;CHECK(ae_login_create(&s)==-EINVAL && s==original);CHECK(ae_login_start(s)==-EBUSY && ae_login_cancel(s)==-EBUSY);
    CHECK(ae_login_begin(s,"bad\nname",8)==-EINVAL && phase(s)==AE_LOGIN_IDLE);
    char username[]={"a\"\\\xc3\xa9"};CHECK(!ae_login_begin(s,username,sizeof(username)-1));memset(username,'x',sizeof(username));
    const uint8_t *p;size_t n;CHECK(!ae_login_output(s,&p,&n));CHECK(strstr((const char *)p+4,"a\\\"\\\\\xc3\xa9"));CHECK(ae_login_sent(s,n+1)==-EBUSY);sent(s);
    CHECK(reply(s,"{\"auth_message\":\"P\\u00e4\\ud83d\\ude00\",\"type\":\"auth_message\",\"auth_message_type\":\"secret\"}")==1);
    CHECK(s->prompt==AE_LOGIN_SECRET && !strcmp(s->text,"P\xc3\xa4\xf0\x9f\x98\x80"));
    unchanged_answer(s,NULL,0);unchanged_answer(s,"\xc0\x80",2);unchanged_answer(s,"x\0y",3);unchanged_answer(s,"x",AE_LOGIN_RESPONSE_MAX+1);
    CHECK(!ae_login_answer(s,"",0));sent(s);CHECK(reply(s,"{\"type\":\"success\"}")==1 && phase(s)==AE_LOGIN_AUTHENTICATED);
    CHECK(!ae_login_start(s));CHECK(!ae_login_output(s,&p,&n));CHECK(strstr((const char *)p+4,"\"cmd\":[\"/usr/libexec/aetheri/aetheri-session\"]") && strstr((const char *)p+4,"XDG_SESSION_TYPE=wayland"));sent(s);
    CHECK(reply(s,"{\"type\":\"success\"}")==1 && phase(s)==AE_LOGIN_START_ACKNOWLEDGED);ae_login_interrupted(s);CHECK(phase(s)==AE_LOGIN_START_ACKNOWLEDGED && ae_login_start(s)==-EBUSY);ae_login_close(&s);
    for(unsigned i=0;i<2;++i){s=prompt(i?"error":"info");unchanged_answer(s,"",0);CHECK(!ae_login_answer(s,NULL,0));sent(s);CHECK(reply(s,"{\"type\":\"success\"}")==1);CHECK(!ae_login_cancel(s));sent(s);CHECK(reply(s,"{\"type\":\"success\"}")==1 && phase(s)==AE_LOGIN_IDLE);ae_login_close(&s);}
    s=prompt("visible");char maximum[AE_LOGIN_RESPONSE_MAX];memset(maximum,'\n',sizeof(maximum));CHECK(!ae_login_answer(s,maximum,sizeof(maximum)));CHECK(!ae_login_output(s,&p,&n) && n>6*sizeof(maximum) && n<10240);CHECK(!ae_login_sent(s,3));ae_login_interrupted(s);unknown(s);
    s=waiting();CHECK(reply(s,"{\"type\":\"error\",\"description\":\"Denied\",\"error_type\":\"auth_error\"}")==1 && phase(s)==AE_LOGIN_DENIED && s->authentication_error);CHECK(ae_login_begin(s,"new",3)==-EBUSY);CHECK(!ae_login_cancel(s));sent(s);CHECK(reply(s,"{\"type\":\"success\"}")==1);CHECK(!ae_login_begin(s,"new",3));ae_login_interrupted(s);unknown(s);
    s=prompt("secret");CHECK(!ae_login_cancel(s));sent(s);CHECK(reply(s,"{\"type\":\"error\",\"error_type\":\"error\",\"description\":\"unknown\"}")==-EPROTO);unknown(s);
    s=waiting();CHECK(reply(s,"{\"type\":\"success\"}")==1 && !ae_login_start(s));sent(s);CHECK(reply(s,"{\"type\":\"error\",\"error_type\":\"error\",\"description\":\"unknown\"}")==-EPROTO);unknown(s);
    malformed();
    const char good[]="{\"type\":\"success\"}";uint8_t frame[sizeof(good)+4];uint32_t length=sizeof(good)-1;memcpy(frame,&length,4);memcpy(frame+4,good,sizeof(good)-1);
    for(size_t cut=0;cut<sizeof(good)+3;++cut){s=waiting();CHECK(!ae_login_receive(s,frame,cut));ae_login_interrupted(s);unknown(s);}
    s=waiting();frame[sizeof(frame)-1]='x';CHECK(ae_login_receive(s,frame,sizeof(frame))==-EPROTO);unknown(s);
    s=waiting();length=AE_LOGIN_WIRE_MAX+1;CHECK(ae_login_receive(s,(uint8_t *)&length,4)==-EPROTO);unknown(s);
    s=fresh();CHECK(ae_login_receive(s,frame,1)==-EPROTO);unknown(s);
    s=waiting();uint8_t *large=malloc(AE_LOGIN_WIRE_MAX+4);CHECK(large);length=AE_LOGIN_WIRE_MAX;memcpy(large,&length,4);memset(large+4,' ',length);memcpy(large+4,good,sizeof(good)-1);CHECK(ae_login_receive(s,large,AE_LOGIN_WIRE_MAX+4)==1 && phase(s)==AE_LOGIN_AUTHENTICATED);free(large);ae_login_close(&s);
    puts("Login protocol: bounded framing/JSON/UTF-8, ordered prompts, fixed command, no replay and owned-buffer wiping passed");return 0;
}
