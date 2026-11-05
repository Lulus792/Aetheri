#define _GNU_SOURCE
#include "shell/completion.h"
#include "shell/parse.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"completion %d: %s\n",__LINE__,#x);exit(1); } } while (0)
static struct ae_shell_completion output;
static void file(const char *name,bool executable) {
    int fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,executable ? 0700 : 0600);CHECK(fd>=0);
    const char content[]="#!/bin/sh\n: > completion-executed\n";CHECK(write(fd,content,sizeof(content)-1)==(ssize_t)(sizeof(content)-1));CHECK(!close(fd));
}
static enum ae_completion_status get(const char *line,const char *path) { return ae_shell_complete(line,strlen(line),strlen(line),path,NULL,NULL,&output); }
static void replacement(const char *line,const char *expected,size_t arg) {
    char result[AE_LINE_BYTES+1];size_t size=strlen(line),length=output.replacement_size+size-(output.end-output.begin);
    CHECK(length<=AE_LINE_BYTES);memcpy(result,line,output.begin);memcpy(result+output.begin,output.replacement,output.replacement_size);
    memcpy(result+output.begin+output.replacement_size,line+output.end,size-output.end);result[length]=0;
    struct ae_shell_program p;size_t offset;CHECK(ae_shell_parse(result,length,NULL,NULL,&p,&offset)==AE_SHELL_OK);
    CHECK(p.count==1 && p.commands[0].argc>arg && !strcmp(p.commands[0].argv[arg],expected));
}
static unsigned fds(void) {
    DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned n=0;while (readdir(d)) ++n;CHECK(!closedir(d));return n;
}
static bool cancel(void *p) { unsigned *left=p;return !(*left)--; }
int main(void) {
    char root[]="/tmp/aetheri-completion-XXXXXX";CHECK(mkdtemp(root) && !chdir(root));printf("completion evidence: %s\n",root);
    CHECK(!mkdir("bin-a",0700) && !mkdir("bin-b",0700) && !mkdir("only-dir",0700));
    file("bin-a/only-run",true);file("bin-a/only-nope",false);file("bin-b/only-run",true);file("bin-b/cd",true);
    file("alpha-one",false);file("alpha-two",false);file("weird $;'file",false);file(".hidden",false);
    file("uä-x",false);file("uå-y",false);file("bad\033name",false);file("bad\xffname",false);
    unsigned before=fds();
    CHECK(get("on","bin-a:bin-b")==AE_COMPLETION_READY && output.count==1);replacement("on","only-run",0);
    CHECK(access("completion-executed",F_OK) && fds()==before);
    CHECK(get("c","bin-b")==AE_COMPLETION_READY && output.count==1);replacement("c","cd",0);
    file("local-exec",true);CHECK(get("local-e","")==AE_COMPLETION_READY);replacement("local-e","local-exec",0);
    CHECK(get("./bin-a/only-","missing")==AE_COMPLETION_READY && output.count==1);replacement("./bin-a/only-","./bin-a/only-run",0);
    CHECK(get("printf only-","")==AE_COMPLETION_READY && output.count==1);replacement("printf only-","only-dir/",1);
    CHECK(get("cd on","")==AE_COMPLETION_READY && output.count==1);replacement("cd on","only-dir/",1);
    CHECK(get("printf alpha","")==AE_COMPLETION_READY && output.count==2 && !strcmp(output.replacement,"'alpha-'"));
    CHECK(get("printf u","")==AE_COMPLETION_READY && output.count==2 && !output.replacement_size);
    const char *quoted="printf 'wei";CHECK(get(quoted,"")==AE_COMPLETION_READY);replacement(quoted,"weird $;'file",1);
    quoted="printf \"wei\"";CHECK(get(quoted,"")==AE_COMPLETION_READY);replacement(quoted,"weird $;'file",1);
    CHECK(get("printf bad","")==AE_COMPLETION_NONE && !output.count);
    CHECK(get("printf .h","")==AE_COMPLETION_READY);replacement("printf .h",".hidden",1);
    CHECK(get("printf ","")==AE_COMPLETION_READY);for (size_t i=0;i<output.count;++i) CHECK(strcmp(output.values+output.items[i].offset,".hidden"));
    const char *middle="printf 'wei' tail";CHECK(ae_shell_complete(middle,strlen(middle),11,"",NULL,NULL,&output)==AE_COMPLETION_READY);replacement(middle,"weird $;'file",1);
    CHECK(ae_shell_complete("only-run",8,2,"bin-a",NULL,NULL,&output)==AE_COMPLETION_UNSUPPORTED);
    CHECK(get("printf $HOME/a","")==AE_COMPLETION_UNSUPPORTED);
    CHECK(get("# comment","")==AE_COMPLETION_UNSUPPORTED);
    CHECK(get("printf a;","")==AE_COMPLETION_UNSUPPORTED);
    CHECK(get("printf x | on","bin-a")==AE_COMPLETION_READY);CHECK(output.begin==11);
    CHECK(get("printf x > alpha","")==AE_COMPLETION_READY && output.count==2);
    CHECK(get("printf x 2> alpha","")==AE_COMPLETION_READY && output.count==2);
    CHECK(get("< alpha","")==AE_COMPLETION_READY && output.count==2);
    unsigned left=0;CHECK(ae_shell_complete("on",2,2,"bin-a",cancel,&left,&output)==AE_COMPLETION_CANCELLED && !output.count);
    left=4;CHECK(ae_shell_complete("on",2,2,"bin-a",cancel,&left,&output)==AE_COMPLETION_CANCELLED && fds()==before);
    char small[8];memset(small,'X',sizeof(small));size_t needed;CHECK(!ae_shell_quote("a'b",3,small,4,&needed));CHECK(needed==8);
    for (size_t i=0;i<sizeof(small);++i) CHECK(small[i]=='X');
    CHECK(!ae_shell_quote("bad\n",4,small,sizeof(small),&needed) && !needed);
    for (unsigned i=0;i<129;++i) { char name[32];CHECK(snprintf(name,sizeof(name),"limit-%03u",i)>0);file(name,false); }
    CHECK(get("printf limit-","")==AE_COMPLETION_LIMIT && !output.count && !output.replacement_size && fds()==before);
    CHECK(access("completion-executed",F_OK));puts("completion: literal parser round trips, PATH/type/hidden filters, bounds, cancellation, no candidate execution or FD leaks passed");return 0;
}
