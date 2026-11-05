#include "shell/parse.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1); } } while (0)
static char variable_value[]="value with | operators; untouched";
static const char *lookup(void *context,const char *name) {
    (void)context;
    if (!strcmp(name,"VALUE")) return variable_value;
    if (!strcmp(name,"EMPTY")) return "";
    if (!strcmp(name,"?")) return "17";
    return NULL;
}
static enum ae_shell_error parse(struct ae_shell_program *p,const char *source) {
    size_t offset=0;return ae_shell_parse(source,strlen(source),lookup,NULL,p,&offset);
}
int main(void) {
    struct ae_shell_program *p=malloc(sizeof(*p));CHECK(p);
    CHECK(parse(p," # ignored | bad syntax &&")==AE_SHELL_OK && !p->count);
    CHECK(parse(p,"printf '%s\\n' a\" b\"' c' \"$VALUE\" ${EMPTY} \"\" $? literal#hash \\| \\$")==AE_SHELL_OK);
    CHECK(p->count==1 && p->commands[0].argc==10);
    char **a=p->commands[0].argv;
    CHECK(!strcmp(a[1],"%s\\n") && !strcmp(a[2],"a b c") && !strcmp(a[3],variable_value));
    CHECK(!*a[4] && !*a[5] && !strcmp(a[6],"17") && !strcmp(a[7],"literal#hash") && !strcmp(a[8],"|") && !strcmp(a[9],"$"));
    variable_value[0]='X';CHECK(a[3][0]=='v');variable_value[0]='v';
    CHECK(parse(p,"cat < 'a b' | printf x > out 2>> err")==AE_SHELL_OK && p->count==2);
    CHECK(p->commands[0].files[0].mode==AE_SHELL_READ && !strcmp(p->commands[0].files[0].path,"a b"));
    CHECK(p->commands[1].files[1].mode==AE_SHELL_WRITE && p->commands[1].files[2].mode==AE_SHELL_APPEND);
    const char *bad[]={"a |","| a","a || b","a && b","a; b","a &","a > x > y","a <","a 'oops","a \\","a $(pwd)","a `pwd`","a ${bad-name}","''","> output","a | > output","a \"unterminated"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) { CHECK(parse(p,bad[i])==AE_SHELL_SYNTAX);CHECK(!p->count && !p->used); }
    CHECK(parse(p,"a $MISSING")==AE_SHELL_UNSET && !p->count);
    CHECK(parse(p,"a '$MISSING' ") == AE_SHELL_OK);
    CHECK(parse(p,"echo Grüße \"\\n\"")==AE_SHELL_OK && !strcmp(p->commands[0].argv[2],"\\n"));
    CHECK(ae_shell_name("_VALUE2") && !ae_shell_name("2VALUE") && !ae_shell_name("bad-name"));
    char limit[AE_SHELL_BYTES+1];memset(limit,'a',sizeof(limit));size_t offset=0;
    CHECK(ae_shell_parse(limit,AE_SHELL_BYTES,lookup,NULL,p,&offset)==AE_SHELL_LIMIT && !p->count);
    limit[0]=0;CHECK(ae_shell_parse(limit,2,lookup,NULL,p,&offset)==AE_SHELL_SYNTAX);
    char args[1024]="a";
    for (unsigned i=1;i<AE_SHELL_ARGS;++i) strcat(args," b");
    CHECK(parse(p,args)==AE_SHELL_OK && p->commands[0].argc==AE_SHELL_ARGS);
    strcat(args," c");CHECK(parse(p,args)==AE_SHELL_LIMIT);
    strcpy(args,"a");for (unsigned i=1;i<AE_SHELL_COMMANDS;++i) strcat(args," | a");
    CHECK(parse(p,args)==AE_SHELL_OK && p->count==AE_SHELL_COMMANDS);
    strcat(args," | a");CHECK(parse(p,args)==AE_SHELL_LIMIT);
    args[0]=0;
    for (unsigned command=0;command<5;++command) {
        if (command) strcat(args," | ");
        strcat(args,"a");for (unsigned i=1;i<AE_SHELL_ARGS;++i) strcat(args," b");
    }
    CHECK(parse(p,args)==AE_SHELL_LIMIT);
    /* Reproducible hostile byte inputs: parsed references must remain owned and
       bounded; failures must never leave an executable partial pipeline. */
    uint32_t random=1;
    for (unsigned round=0;round<20000;++round) {
        char data[96];size_t length=round%sizeof(data);
        for (size_t i=0;i<length;++i) { random=random*UINT32_C(1664525)+UINT32_C(1013904223);data[i]=(char)(random>>24); }
        enum ae_shell_error result=ae_shell_parse(data,length,lookup,NULL,p,&offset);
        CHECK(offset<=length);
        if (result!=AE_SHELL_OK) { CHECK(!p->count && !p->used);continue; }
        CHECK(p->count<=AE_SHELL_COMMANDS && p->used<=AE_SHELL_BYTES);
        for (size_t i=0;i<p->count;++i) {
            struct ae_shell_command *c=&p->commands[i];CHECK(c->argc && c->argc<=AE_SHELL_ARGS && c->argv[c->argc]==NULL);
            for (size_t j=0;j<c->argc;++j) { CHECK(c->argv[j]>=p->bytes && c->argv[j]<p->bytes+p->used);CHECK(memchr(c->argv[j],0,(size_t)(p->bytes+p->used-c->argv[j]))); }
        }
    }
    free(p);puts("shell parser: owned words, quotes, scalar expansions, syntax rejection, limits and 20000 hostile cases passed");return 0;
}
