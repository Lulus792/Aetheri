#define _GNU_SOURCE
#include "login/sources.h"
#include "core/digest.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
static const char config[]="[general]\nsource_profile = false\n[terminal]\nvt = \"none\"\n[default_session]\ncommand = \"/usr/libexec/aetheri/aetheri-greeter\"\nuser = \"aetheri-greeter\"\n";
static int fd_count(void)
{
    DIR *d=opendir("/proc/self/fd");CHECK(d);int n=0;while(readdir(d))++n;closedir(d);return n;
}
static void digest(const char *s,uint8_t out[32])
{
    CHECK(strlen(s)==64);
    for(size_t n=0;n<32;++n){unsigned int v=0;CHECK(sscanf(s+2*n,"%2x",&v)==1);out[n]=(uint8_t)v;}
}
static void rejected(struct ae_login_source_expect *e,size_t n)
{
    struct ae_login_sources *s=NULL;int before=fd_count();
    CHECK(ae_login_sources_create(e,n,&s)<0);CHECK(!s && fd_count()==before);
}
struct thread_input {struct ae_login_sources *s;};
static void *other_thread(void *p)
{
    struct thread_input *i=p;struct ae_login_sources *s=i->s;
    CHECK(ae_login_sources_check(s)==-EXDEV);CHECK(ae_login_sources_close(&s)==-EXDEV && s==i->s);return NULL;
}
static void signal_file(const char *directory,const char *name)
{
    char p[1200];int n=snprintf(p,sizeof p,"%s/%s",directory,name);CHECK(n>0 && (size_t)n<sizeof p);
    int fd=open(p,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0);CHECK(write(fd,"1",1)==1);CHECK(close(fd)==0);
}
static void wait_file(const char *directory,const char *name)
{
    char p[1200];int n=snprintf(p,sizeof p,"%s/%s",directory,name);CHECK(n>0 && (size_t)n<sizeof p);
    for(unsigned int i=0;i<400;++i){if(access(p,F_OK)==0)return;usleep(25000);}CHECK(false);
}
int main(int argc,char **argv)
{
    /* Explicit ordinary-client fixture. Never install or auto-run as Root. */
    if(argc!=6 || getuid()==0 || geteuid()==0)return 77;
    const char *directory=argv[1],*control=argv[4],*test=argv[5];
    unsigned long long size=0;char tail=0;CHECK(sscanf(argv[3],"%llu%c",&size,&tail)==1);
    struct ae_login_source_expect e[2]={
        {.directory=directory,.leaf="greetd",.label="system_u:object_r:bin_t:s0",.kind=AE_LOGIN_SOURCE_ELF,.mode=0555,.size=(uint64_t)size},
        {.directory=directory,.leaf="config.ini",.label="system_u:object_r:etc_t:s0",.kind=AE_LOGIN_SOURCE_DATA,.mode=0444,.size=sizeof config-1}
    };
    char path_copy[AE_LOGIN_SOURCE_PATH_MAX],leaf_copy[256]="config.ini",label_copy[AE_LOGIN_SOURCE_LABEL_MAX]="system_u:object_r:etc_t:s0";
    CHECK(strlen(directory)<sizeof path_copy);strcpy(path_copy,directory);
    e[0].directory=e[1].directory=path_copy;e[1].leaf=leaf_copy;e[1].label=label_copy;
    digest(argv[2],e[0].digest);CHECK(ae_sha256_bytes(config,sizeof config-1,e[1].digest)==0);
    int before=fd_count();struct ae_login_sources *s=NULL;
    if(!strcmp(test,"reject")) {
        /* Host selected the precise invalid fixture, not a successful baseline. */
        rejected(e,2);puts("source rejection passed");return 0;
    }
    CHECK(ae_login_sources_create(e,2,&s)==0 && s);
    struct ae_image image;memset(&image,0xa5,sizeof image);struct ae_image untouched=image;
    CHECK(ae_login_sources_image(s,1,&image)==-EINVAL && !memcmp(&image,&untouched,sizeof image));
    CHECK(ae_login_sources_image(s,2,&image)==-EINVAL && !memcmp(&image,&untouched,sizeof image));
    CHECK(ae_login_sources_image(s,0,&image)==0 && image.size==size && !memcmp(image.digest,e[0].digest,32));
    if(!strcmp(test,"change")) {
        signal_file(control,"ready");wait_file(control,"changed");
        int r=ae_login_sources_check(s);CHECK(r<0);
        signal_file(control,"observed");wait_file(control,"restored");
        CHECK(ae_login_sources_check(s)==r);image=untouched;
        CHECK(ae_login_sources_image(s,0,&image)==r && !memcmp(&image,&untouched,sizeof image));
        CHECK(ae_login_sources_close(&s)==0 && !s && fd_count()==before);
        puts("source loss and restored-object fencing passed");return 0;
    }
    CHECK(!strcmp(test,"normal"));
    /* Expectations are copied, not retained caller strings or candidate data. */
    char path_byte=path_copy[1];path_copy[1]='X';leaf_copy[0]='X';label_copy[0]='X';
    CHECK(ae_login_sources_check(s)==0);path_copy[1]=path_byte;leaf_copy[0]='c';label_copy[0]='s';
    uint8_t saved=e[1].digest[0];e[1].digest[0]^=1;e[1].leaf="absent";
    CHECK(ae_login_sources_check(s)==0);e[1].leaf="config.ini";e[1].digest[0]=saved;
    struct thread_input i={s};pthread_t thread;CHECK(pthread_create(&thread,NULL,other_thread,&i)==0);
    CHECK(pthread_join(thread,NULL)==0 && ae_login_sources_check(s)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){CHECK(ae_login_sources_check(s)==-ECHILD);CHECK(ae_login_sources_close(&s)==0 && !s);_exit(0);}
    int status=0;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(ae_login_sources_check(s)==0);CHECK(ae_login_sources_close(&s)==0 && !s && fd_count()==before);
    CHECK(ae_login_sources_close(&s)==0);
    CHECK(ae_login_sources_create(e,0,&s)==-EINVAL && !s);
    CHECK(ae_login_sources_create(e,AE_LOGIN_SOURCES_MAX+1,&s)==-EINVAL && !s);
    e[1].digest[0]^=1;rejected(e,2);e[1].digest[0]^=1;
    const char *original=e[1].label;e[1].label="system_u:object_r:bin_t:s0";rejected(e,2);e[1].label=original;
    mode_t mode=e[1].mode;e[1].mode=0644;rejected(e,2);e[1].mode=mode;
    ++e[1].size;rejected(e,2);--e[1].size;
    original=e[1].leaf;const char *bad[]={"",".","..","../config.ini","/config.ini"};
    for(size_t n=0;n<sizeof bad/sizeof bad[0];++n){e[1].leaf=bad[n];rejected(e,2);}e[1].leaf=original;
    original=e[1].directory;e[1].directory="relative";rejected(e,2);e[1].directory="/opt/../opt";rejected(e,2);
    e[1].directory="/opt//test";rejected(e,2);e[1].directory="/opt/";rejected(e,2);e[1].directory=original;
    char long_leaf[257],long_path[AE_LOGIN_SOURCE_PATH_MAX+1],long_label[AE_LOGIN_SOURCE_LABEL_MAX+1];
    memset(long_leaf,'x',sizeof long_leaf);long_leaf[sizeof long_leaf-1]=0;
    memset(long_path,'x',sizeof long_path);long_path[0]='/';long_path[sizeof long_path-1]=0;
    memset(long_label,'x',sizeof long_label);long_label[sizeof long_label-1]=0;
    original=e[1].leaf;e[1].leaf=long_leaf;rejected(e,2);e[1].leaf=original;
    original=e[1].directory;e[1].directory=long_path;rejected(e,2);
    e[1].directory="/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q";rejected(e,2);e[1].directory=original;
    original=e[1].label;e[1].label=long_label;rejected(e,2);e[1].label=original;
    uint64_t original_size=e[1].size;e[1].size=0;rejected(e,2);
    e[1].size=AE_LOGIN_SOURCE_DATA_MAX+1;rejected(e,2);e[1].size=original_size;
    enum ae_login_source_kind original_kind=e[1].kind;e[1].kind=(enum ae_login_source_kind)0;rejected(e,2);e[1].kind=original_kind;
    struct ae_login_source_expect saved_entry=e[1];e[1]=e[0];rejected(e,2);e[1]=saved_entry;
    CHECK(fd_count()==before);puts("protected source selection, image, bounds and ownership passed");return 0;
}
