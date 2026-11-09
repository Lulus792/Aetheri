#define _GNU_SOURCE
#include "audio/actor.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Explicit disposable-carrier probe. The administrative driver selects the
 * protected unit/image/context and passes its original main-process pin. */
int main(int argc,char **argv) {
    if(argc!=6 || getuid()!=1000 || geteuid()!=1000 || getgid()!=1000 || getegid()!=1000) return 2;
    char *end=NULL;errno=0;long value=strtol(argv[4],&end,10);
    if(errno || !end || *end || value<3 || value>INT_MAX ||
       (strcmp(argv[5],"reject") && strcmp(argv[5],"wait-exit"))) return 2;
    int input=(int)value;bool reject=!strcmp(argv[5],"reject");
    alarm(30);setvbuf(stdout,NULL,_IOLBF,0);
    sd_bus *user=NULL,*system=NULL;struct ae_audio_manager *manager=NULL;struct ae_audio_actor *actor=NULL;
    int image=-1,systemd=-1,result=1;
    if(sd_bus_open_user(&user)<0 || sd_bus_open_system(&system)<0 ||
       sd_bus_set_method_call_timeout(user,1000000)<0 || sd_bus_set_method_call_timeout(system,1000000)<0) goto done;
    systemd=open("/usr/lib/systemd/systemd",O_PATH|O_CLOEXEC|O_NOFOLLOW);
    image=open(argv[2],O_PATH|O_CLOEXEC|O_NOFOLLOW);if(systemd<0 || image<0) goto done;
    manager=ae_audio_manager_open(user,system,1000,systemd);
    if(!manager) {printf("ACTOR_MANAGER_REJECT errno=%d\n",errno);goto done;}
    actor=ae_audio_actor_open(manager,argv[1],input,image,argv[3]);int error=errno;
    close(input);input=-1;close(image);image=-1;close(systemd);systemd=-1;
    if(reject) {
        if(actor || ae_audio_manager_check(manager)<0) goto done;
        printf("ACTOR_REJECTED errno=%d original_manager_live=1\n",error);result=0;goto done;
    }
    if(!actor) {printf("ACTOR_OPEN_REJECT errno=%d\n",error);goto done;}
    ae_audio_manager_close(&manager);
    if(ae_audio_actor_check(actor)<0) goto done;
    puts("ACTOR_ADMITTED original=1 user_unit=1 image=1 context=1 caller_handles_closed=1");
    int loss=0;
    for(unsigned i=0;i<200 && !loss;++i) {usleep(50000);loss=ae_audio_actor_check(actor);}
    if(loss>=0) goto done;
    for(unsigned i=0;i<8;++i) if(ae_audio_actor_check(actor)!=loss) goto done;
    printf("ACTOR_LOST error=%d latched=1\n",loss);result=0;
done:
    ae_audio_actor_close(&actor);ae_audio_manager_close(&manager);
    if(image>=0) close(image);
    if(systemd>=0) close(systemd);
    if(input>=0) close(input);
    sd_bus_flush_close_unref(user);sd_bus_flush_close_unref(system);
    if(!result) puts("ACTOR_RETIRED handles=closed no_service_mutation=1");
    return result;
}
