#define _GNU_SOURCE
#include "login/runner.h"
#include <fcntl.h>
#include <linux/close_range.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc,char **argv) {
    (void)argv;
    if(argc!=1){fputs("aetheri-login-startup accepts no arguments\n",stderr);return 2;}
    for(int fd=0;fd<3;++fd)if(fcntl(fd,F_GETFD)<0)return 1;
    if(close_range(3,~0u,CLOSE_RANGE_UNSHARE))return 1;
    setvbuf(stdout,NULL,_IOLBF,0);
    struct ae_login_run_profile profile={0};
    int result=ae_login_deployment_select(&profile);
    if(!result)result=ae_login_startup_run(&profile);
    if(result){fprintf(stderr,"AETHERI_LOGIN_STARTUP_FAIL error=%d\n",result);return 1;}
    puts("AETHERI_LOGIN_STARTUP_COMPLETE");return 0;
}
