#define _GNU_SOURCE
#include "shell/welcome.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"welcome %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned fixture;
extern int __real_uname(struct utsname *);
extern int __real_sysinfo(struct sysinfo *);
extern long __real_sysconf(int);
int __wrap_uname(struct utsname *u) {
    if(!fixture) return __real_uname(u);
    if(fixture==1) return -1;
    memset(u,0,sizeof(*u));strcpy(u->sysname,"Linux");strcpy(u->release,"\033[31mbad\n");
    strcpy(u->machine,"x86_64");strcpy(u->nodename,"host\r\033[2J");return 0;
}
int __wrap_sysinfo(struct sysinfo *s) {
    if(!fixture) return __real_sysinfo(s);
    if(fixture==1) return -1;
    memset(s,0,sizeof(*s));s->totalram=ULONG_MAX;s->mem_unit=UINT_MAX;s->uptime=-1;return 0;
}
long __wrap_sysconf(int name) {return fixture && name==_SC_NPROCESSORS_ONLN?(fixture==1?-1:4):__real_sysconf(name);}
static void columns(const char *text,size_t size,unsigned cols) {
    unsigned width=0;
    for(size_t i=0;i<size;) {
        unsigned char c=(unsigned char)text[i];
        if(c==27) {
            if(i+5<=size && !memcmp(text+i,"\033[94m",5)) i+=5;
            else {CHECK(i+4<=size && !memcmp(text+i,"\033[0m",4));i+=4;}
        } else if(c=='\n') {CHECK(width<cols);width=0;++i;}
        else {CHECK(c>=32 && c<127);++width;++i;}
    }
    CHECK(!width && size>1 && text[size-1]=='\n' && text[size-2]=='\n');
}
int main(void) {
    struct ae_shell_welcome s;ae_shell_welcome_capture(&s);
    struct utsname kernel;struct sysinfo memory;
    CHECK(!__real_uname(&kernel) && !__real_sysinfo(&memory));
    CHECK(s.kernel_known && !strcmp(s.release,kernel.release) && !strcmp(s.machine,kernel.machine));
    CHECK(s.uid==(uint64_t)getuid() && s.cpus_known && s.cpus==(uint64_t)__real_sysconf(_SC_NPROCESSORS_ONLN));
    CHECK(s.memory_known && s.memory_bytes==(uint64_t)memory.totalram*memory.mem_unit && s.uptime_known && s.uptime_seconds<=(uint64_t)memory.uptime);
    s=(struct ae_shell_welcome){.uid=4321,.memory_bytes=UINT64_C(9126805504),.uptime_seconds=90061,.cpus=4,
        .kernel_known=true,.memory_known=true,.uptime_known=true,.cpus_known=true};
    strcpy(s.system,"Linux");strcpy(s.release,"example-kernel");strcpy(s.machine,"x86_64");strcpy(s.host,"local-host");
    char output[AE_WELCOME_BYTES],unchanged[AE_WELCOME_BYTES];size_t size;
    CHECK(ae_shell_welcome_format(&s,80,output,sizeof(output),&size));columns(output,size,80);
    CHECK(strstr(output,"A E T H E R I") && strstr(output,"Aetheri development") && strstr(output,"uid 4321 @ local-host"));
    CHECK(strstr(output,"Memory  8.5 GiB total") && strstr(output,"Uptime  1d 01h 01m") && strstr(output,"Shell   Aetheri profile0"));
    size_t required=size+1;memset(unchanged,'q',sizeof(unchanged));memcpy(output,unchanged,sizeof(output));
    CHECK(!ae_shell_welcome_format(&s,80,output,required-1,&size) && !size && !memcmp(output,unchanged,sizeof(output)));
    CHECK(ae_shell_welcome_format(&s,80,output,required,&size) && size+1==required);
    for(unsigned cols=16;cols<=240;++cols) {CHECK(ae_shell_welcome_format(&s,cols,output,sizeof(output),&size));columns(output,size,cols);}
    CHECK(ae_shell_welcome_format(&s,4096,output,sizeof(output),&size));columns(output,size,4096);
    memset(output,'q',sizeof(output));
    CHECK(!ae_shell_welcome_format(&s,15,output,sizeof(output),&size) && !size && !memcmp(output,unchanged,sizeof(output)));
    CHECK(!ae_shell_welcome_format(&s,4097,output,sizeof(output),&size) && !size);
    memset(s.host,'x',sizeof(s.host));CHECK(!ae_shell_welcome_format(&s,80,output,sizeof(output),&size) && !size);
    fixture=1;ae_shell_welcome_capture(&s);
    CHECK(!s.kernel_known && !s.memory_known && !s.cpus_known && !s.uptime_known);
    CHECK(ae_shell_welcome_format(&s,80,output,sizeof(output),&size) && strstr(output,"unavailable"));columns(output,size,80);
    fixture=2;ae_shell_welcome_capture(&s);
    CHECK(s.kernel_known && s.cpus_known && !s.memory_known && !s.uptime_known);
    CHECK(!strcmp(s.release,"?[31mbad?") && !strcmp(s.host,"host??[2J"));
    memset(s.release,'x',sizeof(s.release)-1);s.release[sizeof(s.release)-1]=0;
    s.host[0]='\033';s.host[1]='\n';s.host[2]=0;
    for(unsigned cols=16;cols<=80;++cols) {CHECK(ae_shell_welcome_format(&s,cols,output,sizeof(output),&size));columns(output,size,cols);}
    puts("welcome: actual kernel snapshot, explicit unknowns, overflow, safe terminal text, responsive bounds and atomic capacity passed");return 0;
}
