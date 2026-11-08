#define _GNU_SOURCE
#include "audio/remote.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int ae_audio_remote_connect(const char *path) {
    struct sockaddr_un address={.sun_family=AF_UNIX};
    if(!path || path[0]!='/' || strlen(path)>=sizeof(address.sun_path)) {errno=EINVAL;return -1;}
    memcpy(address.sun_path,path,strlen(path)+1);
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if(fd<0) return -1;
    struct ucred peer={0};socklen_t size=sizeof(peer);
    if(connect(fd,(struct sockaddr *)&address,(socklen_t)(offsetof(struct sockaddr_un,sun_path)+strlen(path)+1)) ||
       getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&size)) {
        int error=errno;(void)close(fd);errno=error;return -1;
    }
    if(size!=sizeof(peer) || peer.uid!=getuid() || peer.pid<=0) {(void)close(fd);errno=EPERM;return -1;}
    return fd;
}
