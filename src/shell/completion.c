#define _GNU_SOURCE
#include "completion.h"
#include "text/grapheme.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
enum mode { COMMAND, FILES, DIRECTORIES };
struct query { size_t begin,end,size;enum mode mode;char value[AE_LINE_BYTES+1]; };
static bool space(char c) { return c==' ' || c=='\t'; }
static bool operator(char c) { return c=='|' || c=='<' || c=='>'; }
/* Mirror literal quoting from profile0; never evaluate a variable. Earlier
   dynamic words still occupy an argument, but cannot be identified as cd. */
static bool query(const char *s,size_t length,size_t cursor,struct query *q) {
    bool command=true,file=false,cd=false;size_t at=0,args=0;
    while (at<=length) {
        while (at<cursor && space(s[at])) ++at;
        if (at==cursor && (at==length || space(s[at]))) {
            *q=(struct query){.begin=at,.end=at,.mode=file ? FILES : command ? COMMAND : cd && !args ? DIRECTORIES : FILES};return true;
        }
        if (at>=length || s[at]=='#') return false;
        if (operator(s[at]) || (s[at]=='2' && at+1<length && s[at+1]=='>')) {
            size_t begin=at;char op=s[at++];if (op=='2') op=s[at++];
            if (op=='>' && at<length && s[at]=='>') ++at;
            if (cursor>=begin && cursor<at) return false;
            if (op=='|') { command=true;file=cd=false;args=0; }else file=true;
            continue;
        }
        size_t begin=at,n=0,prefix=0;char quote=0;bool dynamic=false,split=false;
        char value[AE_LINE_BYTES+1];
        while (at<length) {
            char c=s[at];if (!quote && (space(c) || operator(c))) break;
            if (at==cursor) prefix=n;
            if (!quote && (c==';' || c=='&' || c=='(' || c==')' || c=='`')) return false;
            if (c=='\'' || c=='"') {
                if (!quote) { quote=c;++at;continue; }
                if (quote==c) { quote=0;++at;continue; }
            }
            if (c=='\\' && quote!='\'') {
                ++at;if (at==length) return false;
                if (at==cursor) split=true;
                c=s[at];if (quote=='"' && c!='"' && c!='\\' && c!='$') value[n++]='\\';
            } else if (c=='$' && quote!='\'') dynamic=true;
            value[n++]=c;++at;
        }
        value[n]=0;if (at==cursor) prefix=n;
        if (cursor>=begin && cursor<=at) {
            if (split || dynamic || prefix!=n) return false;
            q->begin=begin;q->end=at;q->size=prefix;memcpy(q->value,value,prefix+1);
            q->mode=file ? FILES : command ? COMMAND : cd && !args ? DIRECTORIES : FILES;return true;
        }
        if (quote || cursor<begin) return false;
        if (file) file=false;
        else if (command) { command=false;cd=!dynamic && !strcmp(value,"cd"); }
        else ++args;
    }
    return false;
}
bool ae_shell_quote(const char *value,size_t size,char *out,size_t capacity,size_t *required) {
    if (!required) return false;
    *required=0;if (size>AE_LINE_BYTES || !ae_line_valid_utf8(value,size)) return false;
    size_t n=2;for (size_t i=0;i<size;++i) n+=value[i]=='\'' ? 4u : 1u;
    *required=n;if (!out || capacity<=n) return false;
    size_t at=0;out[at++]='\'';
    for (size_t i=0;i<size;++i) {
        if (value[i]=='\'') { memcpy(out+at,"'\\''",4);at+=4; }else out[at++]=value[i];
    }
    out[at++]='\'';out[at]=0;return true;
}
struct scan {
    struct ae_shell_completion *out;
    bool (*cancelled)(void *);void *context;
    unsigned entries,directories;uint64_t deadline;
};
static uint64_t tick(void) {
    struct timespec t;if (clock_gettime(CLOCK_MONOTONIC,&t) || t.tv_sec<0) return 0;
    return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;
}
static enum ae_completion_status check(struct scan *s) {
    if (s->cancelled && s->cancelled(s->context)) return AE_COMPLETION_CANCELLED;
    uint64_t now=tick();return !now || now>=s->deadline ? AE_COMPLETION_LIMIT : AE_COMPLETION_READY;
}
static enum ae_completion_status add(struct scan *s,const char *value,size_t size,bool directory) {
    struct ae_shell_completion *o=s->out;if (!ae_line_valid_utf8(value,size)) return AE_COMPLETION_READY;
    char name[AE_LINE_BYTES+1];if (size+(directory ? 1u : 0u)>AE_LINE_BYTES) return AE_COMPLETION_LIMIT;
    memcpy(name,value,size);if (directory) name[size++]='/';name[size]=0;
    size_t at=0;while (at<o->count && strcmp(o->values+o->items[at].offset,name)<0) ++at;
    if (at<o->count && !strcmp(o->values+o->items[at].offset,name)) return AE_COMPLETION_READY;
    if (o->count==AE_COMPLETION_COUNT || size+1>AE_COMPLETION_BYTES-o->used) return AE_COMPLETION_LIMIT;
    memmove(o->items+at+1,o->items+at,(o->count-at)*sizeof(*o->items));
    o->items[at]=(struct ae_completion_item){o->used,directory};memcpy(o->values+o->used,name,size+1);
    o->used+=size+1;++o->count;return AE_COMPLETION_READY;
}
static enum ae_completion_status directory(struct scan *s,const char *parent,const char *prefix,const char *base,bool executable,bool dirs,bool path_search) {
    enum ae_completion_status status=check(s);if (status!=AE_COMPLETION_READY) return status;
    if (++s->directories>64) return AE_COMPLETION_LIMIT;
    DIR *d=opendir(*parent ? parent : ".");
    if (!d) return path_search && (errno==ENOENT || errno==ENOTDIR || errno==EACCES) ? AE_COMPLETION_READY : AE_COMPLETION_IO;
    size_t length=strlen(prefix),root=strlen(base);struct dirent *entry;
    for (;;) {
        status=check(s);if (status!=AE_COMPLETION_READY) break;
        errno=0;entry=readdir(d);if (!entry) { status=errno ? AE_COMPLETION_IO : AE_COMPLETION_READY;break; }
        if (++s->entries>32768) { status=AE_COMPLETION_LIMIT;break; }
        const char *name=entry->d_name;
        if (!strcmp(name,".") || !strcmp(name,"..") || (name[0]=='.' && prefix[0]!='.') || strncmp(name,prefix,length)) continue;
        struct stat st;if (fstatat(dirfd(d),name,&st,0)) {
            if (errno==ENOENT || errno==ENOTDIR || errno==EACCES || errno==ELOOP) continue;
            status=AE_COMPLETION_IO;break;
        }
        bool isdir=S_ISDIR(st.st_mode);if ((dirs && !isdir) || (path_search && isdir)) continue;
        if (executable && !isdir && (!S_ISREG(st.st_mode) || faccessat(dirfd(d),name,X_OK,0))) continue;
        size_t n=strlen(name);if (n>AE_LINE_BYTES-root) { status=AE_COMPLETION_LIMIT;break; }
        char value[AE_LINE_BYTES+1];memcpy(value,base,root);memcpy(value+root,name,n+1);
        status=add(s,value,root+n,isdir);if (status!=AE_COMPLETION_READY) break;
    }
    if (closedir(d) && status==AE_COMPLETION_READY) status=AE_COMPLETION_IO;
    return status;
}
enum ae_completion_status ae_shell_complete(const char *line,size_t size,size_t cursor,const char *path,
        bool (*cancelled)(void *),void *context,struct ae_shell_completion *out) {
    if (!out) return AE_COMPLETION_UNSUPPORTED;
    memset(out,0,sizeof(*out));
    if ((!line && size) || size>AE_LINE_BYTES || cursor>size || !ae_line_valid_utf8(line,size)) return AE_COMPLETION_UNSUPPORTED;
    size_t boundaries[AE_LINE_BYTES+1],count;bool valid_cursor=false;
    if (!ae_text_graphemes(line,size,boundaries,AE_LINE_BYTES+1,&count)) return AE_COMPLETION_UNSUPPORTED;
    for (size_t i=0;i<count;++i) valid_cursor|=boundaries[i]==cursor;
    struct query q;if (!valid_cursor || !query(line,size,cursor,&q)) return AE_COMPLETION_UNSUPPORTED;
    struct scan s={.out=out,.cancelled=cancelled,.context=context};s.deadline=tick();
    if (!s.deadline || s.deadline>UINT64_MAX-250u) return AE_COMPLETION_LIMIT;
    s.deadline+=250u;enum ae_completion_status status=check(&s);
    if (status!=AE_COMPLETION_READY) goto done;
    const char *slash=strrchr(q.value,'/');
    if (q.mode==COMMAND && !slash) {
        static const char *const names[]={"bg","cd","exit","export","fg","help","jobs","pwd","unset"};
        for (size_t i=0;i<sizeof(names)/sizeof(*names);++i) if (!strncmp(names[i],q.value,q.size)) {
            status=add(&s,names[i],strlen(names[i]),false);if (status!=AE_COMPLETION_READY) goto done;
        }
        if (!path) path="/usr/bin:/bin";
        if (strnlen(path,AE_LINE_BYTES+1)>AE_LINE_BYTES) { status=AE_COMPLETION_LIMIT;goto done; }
        for (;;) {
            const char *end=strchr(path,':');size_t n=end ? (size_t)(end-path) : strlen(path);
            char parent[AE_LINE_BYTES+1];memcpy(parent,path,n);parent[n]=0;
            status=directory(&s,parent,q.value,"",true,false,true);if (status!=AE_COMPLETION_READY || !end) break;
            path=end+1;
        }
    } else {
        char parent[AE_LINE_BYTES+1]=".",base[AE_LINE_BYTES+1]="";
        if (slash) { size_t n=(size_t)(slash-q.value)+1;memcpy(parent,q.value,n);parent[n]=0;memcpy(base,parent,n+1); }
        status=directory(&s,parent,slash ? slash+1 : q.value,base,q.mode==COMMAND,q.mode==DIRECTORIES,false);
    }
    if (status!=AE_COMPLETION_READY) goto done;
    if (!out->count) { status=AE_COMPLETION_NONE;goto done; }
    out->begin=q.begin;out->end=q.end;
    const char *first=out->values+out->items[0].offset;size_t n=strlen(first);
    for (size_t i=1;i<out->count;++i) {
        const char *other=out->values+out->items[i].offset;size_t j=0;while (j<n && first[j]==other[j]) ++j;n=j;
    }
    if (out->count>1) {
        size_t points;
        if (!ae_text_graphemes(first,strlen(first),boundaries,AE_LINE_BYTES+1,&points)) { status=AE_COMPLETION_IO;goto done; }
        size_t safe=0;for (size_t i=0;i<points && boundaries[i]<=n;++i) safe=boundaries[i];n=safe;
        if (n<=q.size) goto done;
    }
    size_t required;
    if (!ae_shell_quote(first,n,out->replacement,sizeof(out->replacement),&required)) { status=AE_COMPLETION_LIMIT;goto done; }
    bool trailing=out->count==1 && !out->items[0].directory && (q.end==size || !space(line[q.end]));
    if (required+(trailing ? 1u : 0u)>AE_LINE_BYTES-(size-(q.end-q.begin))) { status=AE_COMPLETION_LIMIT;goto done; }
    if (trailing) out->replacement[required++]=' ';
    out->replacement[required]=0;out->replacement_size=required;
done:
    if (status==AE_COMPLETION_READY) status=check(&s);
    if (status!=AE_COMPLETION_READY) memset(out,0,sizeof(*out));
    return status;
}
