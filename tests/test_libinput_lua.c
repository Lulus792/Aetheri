/* Aetheri: matching-version Lua plugin admission regression. MIT licensed. */
#define _POSIX_C_SOURCE 200809L
#include <libinput.h>
#include <lua.h>
#include <lauxlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned device_opens,text_seen,binary_seen,binary_rejected;
static int deny_device(const char *path,int flags,void *data) {
    (void)path;(void)flags;(void)data;++device_opens;return -EPERM;
}
static void close_device(int fd,void *data) {(void)data;close(fd);}
static void log_message(struct libinput *li,enum libinput_log_priority priority,const char *format,va_list args) {
    (void)li;(void)priority;char text[2048];vsnprintf(text,sizeof(text),format,args);
    if(strstr(text,"AETHERI_TEXT_LOADED"))++text_seen;
    if(strstr(text,"AETHERI_BINARY_LOADED"))++binary_seen;
    if(strstr(text,"binary chunk") && strstr(text,"mode is 't'"))++binary_rejected;
}
static int write_all(int fd,const void *bytes,size_t size) {
    const unsigned char *p=bytes;
    while(size) {
        ssize_t n=write(fd,p,size);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return 1;
        p+=(size_t)n;size-=(size_t)n;
    }
    return 0;
}
static int dump(lua_State *L,const void *p,size_t size,void *data) {
    (void)L;return write_all(*(int *)data,p,size);
}
static int load_directory(const char *directory) {
    const struct libinput_interface interface={deny_device,close_device};
    struct libinput *li=libinput_path_create_context(&interface,NULL);
    if(!li)return 1;
    libinput_log_set_handler(li,log_message);libinput_log_set_priority(li,LIBINPUT_LOG_PRIORITY_DEBUG);
    libinput_plugin_system_append_path(li,directory);
    int result=libinput_plugin_system_load_plugins(li,LIBINPUT_PLUGIN_SYSTEM_FLAG_NONE);
    libinput_unref(li);return result;
}
int main(int argc,char **argv) {
    if(argc!=2 || lua_version(NULL)!=505 || LUA_VERSION_NUM!=505)return 2;
    char textdir[4096],binarydir[4096],path[4096];
    if(snprintf(textdir,sizeof(textdir),"%s/text",argv[1])>=(int)sizeof(textdir) || snprintf(binarydir,sizeof(binarydir),"%s/binary",argv[1])>=(int)sizeof(binarydir))return 2;
    if(mkdir(argv[1],0700) || mkdir(textdir,0700) || mkdir(binarydir,0700))return 2;
    const char text[]="assert(_VERSION == 'Lua 5.5')\nlibinput:register({1})\nlibinput:log_error('AETHERI_TEXT_LOADED')\n";
    if(snprintf(path,sizeof(path),"%s/10-test.lua",textdir)>=(int)sizeof(path))return 2;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if(fd<0 || write_all(fd,text,sizeof(text)-1) || fsync(fd) || close(fd))return 2;
    const char binary[]="assert(_VERSION == 'Lua 5.5')\nlibinput:register({1})\nlibinput:log_error('AETHERI_BINARY_LOADED')\n";
    lua_State *L=luaL_newstate();
    if(!L || luaL_loadbufferx(L,binary,sizeof(binary)-1,"matching-bytecode","t"))return 2;
    if(snprintf(path,sizeof(path),"%s/10-test.lua",binarydir)>=(int)sizeof(path))return 2;
    fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if(fd<0 || lua_dump(L,dump,&fd,0) || fsync(fd) || close(fd))return 2;
    lua_close(L);
    if(load_directory(textdir)<0 || text_seen!=1 || binary_seen || device_opens)return 1;
    int result=load_directory(binarydir);
    if(binary_seen) {fputs("MATCHING_BYTECODE_ACCEPTED\n",stderr);return 1;}
    if(!binary_rejected || device_opens)return 1;
    printf("TEXT_ACCEPTED lua=505 device_opens=0\nMATCHING_BYTECODE_REJECTED loader_result=%d\n",result);
    return 0;
}
