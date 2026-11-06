#define _GNU_SOURCE
#include "welcome.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
static void ascii(char out[AE_WELCOME_FIELD],const char *input,size_t capacity) {
    size_t n=strnlen(input,capacity),take=n<AE_WELCOME_FIELD-1?n:AE_WELCOME_FIELD-1;
    for(size_t i=0;i<take;++i) {unsigned char c=(unsigned char)input[i];out[i]=c>=32 && c<127?(char)c:'?';}
    out[take]=0;
    if(n>take && take>=3) memcpy(out+take-3,"...",3);
}
void ae_shell_welcome_capture(struct ae_shell_welcome *out) {
    if(!out) return;
    *out=(struct ae_shell_welcome){.uid=(uint64_t)getuid()};
    struct utsname kernel={0};
    if(!uname(&kernel)) {
        ascii(out->system,kernel.sysname,sizeof(kernel.sysname));ascii(out->release,kernel.release,sizeof(kernel.release));
        ascii(out->machine,kernel.machine,sizeof(kernel.machine));ascii(out->host,kernel.nodename,sizeof(kernel.nodename));
        out->kernel_known=true;
    }
    long cpus=sysconf(_SC_NPROCESSORS_ONLN);if(cpus>0) {out->cpus=(uint64_t)cpus;out->cpus_known=true;}
    struct sysinfo memory={0};
    if(!sysinfo(&memory)) {
        if(memory.mem_unit && memory.totalram && (uint64_t)memory.totalram<=UINT64_MAX/memory.mem_unit) {
            out->memory_bytes=(uint64_t)memory.totalram*memory.mem_unit;out->memory_known=true;
        }
        if(memory.uptime>=0) {out->uptime_seconds=(uint64_t)memory.uptime;out->uptime_known=true;}
    }
}
struct builder { char text[AE_WELCOME_BYTES];size_t used; };
static bool append(struct builder *b,const char *text,size_t size) {
    if(size>=sizeof(b->text)-b->used) return false;
    memcpy(b->text+b->used,text,size);b->used+=size;b->text[b->used]=0;return true;
}
static bool row(struct builder *b,const char *mark,const char *text,size_t size,unsigned column) {
    size_t n=strlen(mark);
    if(n && (!append(b,"\033[94m",5) || !append(b,mark,n) || !append(b,"\033[0m",4))) return false;
    if(column) for(size_t i=n;i<column;++i) if(!append(b," ",1)) return false;
    return append(b,text,size) && append(b,"\n",1);
}
bool ae_shell_welcome_format(const struct ae_shell_welcome *s,unsigned cols,char *output,size_t capacity,size_t *size) {
    if(size) *size=0;
    if(!s || !output || !size || cols<16 || cols>4096 ||
       !memchr(s->system,0,sizeof(s->system)) || !memchr(s->release,0,sizeof(s->release)) ||
       !memchr(s->machine,0,sizeof(s->machine)) || !memchr(s->host,0,sizeof(s->host))) return false;
    char system[AE_WELCOME_FIELD],release[AE_WELCOME_FIELD],machine[AE_WELCOME_FIELD],host[AE_WELCOME_FIELD];
    ascii(system,s->system,sizeof(s->system));ascii(release,s->release,sizeof(s->release));
    ascii(machine,s->machine,sizeof(s->machine));ascii(host,s->host,sizeof(s->host));
    const char *unknown="unavailable";char lines[9][160]={{0}};int n;
    memcpy(lines[0],"Aetheri development",20);
    n=snprintf(lines[1],sizeof(lines[1]),"uid %" PRIu64 " @ %s",s->uid,s->kernel_known?host:unknown);
    if(n<0 || (size_t)n>=sizeof(lines[1])) return false;
    memcpy(lines[2],"--------------------",21);
    n=snprintf(lines[3],sizeof(lines[3]),"Kernel  %s%s%s",s->kernel_known?system:unknown,s->kernel_known?" ":"",s->kernel_known?release:"");
    if(n<0 || (size_t)n>=sizeof(lines[3])) return false;
    n=snprintf(lines[4],sizeof(lines[4]),"Arch    %s",s->kernel_known?machine:unknown);
    if(n<0 || (size_t)n>=sizeof(lines[4])) return false;
    if(s->cpus_known) n=snprintf(lines[5],sizeof(lines[5]),"CPUs    %" PRIu64 " online",s->cpus);
    else n=snprintf(lines[5],sizeof(lines[5]),"CPUs    %s",unknown);
    if(n<0 || (size_t)n>=sizeof(lines[5])) return false;
    const uint64_t gib=UINT64_C(1024)*1024*1024;
    if(s->memory_known) n=snprintf(lines[6],sizeof(lines[6]),"Memory  %" PRIu64 ".%" PRIu64 " GiB total",s->memory_bytes/gib,(s->memory_bytes%gib)*10/gib);
    else n=snprintf(lines[6],sizeof(lines[6]),"Memory  %s",unknown);
    if(n<0 || (size_t)n>=sizeof(lines[6])) return false;
    if(s->uptime_known) n=snprintf(lines[7],sizeof(lines[7]),"Uptime  %" PRIu64 "d %02" PRIu64 "h %02" PRIu64 "m",s->uptime_seconds/86400,s->uptime_seconds/3600%24,s->uptime_seconds/60%60);
    else n=snprintf(lines[7],sizeof(lines[7]),"Uptime  %s",unknown);
    if(n<0 || (size_t)n>=sizeof(lines[7])) return false;
    memcpy(lines[8],"Shell   Aetheri profile0",sizeof("Shell   Aetheri profile0"));
    static const char *const logo[]={"          .","         / \\","        /   \\","       /  *  \\","      /  / \\  \\","     /  /   \\  \\","    /__/_____\\__\\","       A E T H E R I"};
    static const char *const compact[]={"    /\\","   / *\\","  /___\\"," AETHERI"};
    struct builder b={0};if(!append(&b,"\n",1)) return false;
    bool wide=cols>=72;unsigned column=wide?26u:0u,width=cols-1-column;size_t at=0;
    if(!wide) {
        const char *const *mark=cols<26?compact:logo;size_t count=cols<26?4u:8u;
        for(size_t i=0;i<count;++i) if(!row(&b,mark[i],"",0,0)) return false;
        if(!append(&b,"\n",1)) return false;
    }
    for(unsigned i=0;i<9;++i) {
        size_t length=strlen(lines[i]);
        for(size_t offset=0;offset<length;offset+=width,++at) {
            size_t take=length-offset;if(take>width) take=width;
            if(!row(&b,wide && at<8?logo[at]:"",lines[i]+offset,take,column)) return false;
        }
    }
    if(!append(&b,"\n",1) || capacity<=b.used) return false;
    memcpy(output,b.text,b.used+1);*size=b.used;return true;
}
