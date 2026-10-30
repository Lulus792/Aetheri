#include "launcher/preparation.h"
#include "launcher/execution.h"
#include "linux/execute.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

/* A distinct executing image. Never use the old setup helper's no-exec drain
 * proof for this binary, including a failure before registration or exec. */
int main(int argc,char **argv)
{
    (void)argv; if (argc!=1) return 2;
    const char *stage="context"; int r=ae_starter_context(),writer=-1;
    struct ae_execution_profile profile; uint8_t digest[32]; sd_bus *bus=NULL;
    struct ae_execution_client *client=NULL; struct ae_starter_views *views=NULL;
    struct ae_starter_target target;
    if (r==0) { stage="profile"; r=ae_execution_profile_load(&profile,digest); }
    if (r==0) { stage="self"; r=ae_starter_self(&profile); }
    if (r==0) { stage="bus"; r=sd_bus_new(&bus); }
    if (r>=0) r=sd_bus_set_address(bus,"unix:path=/run/dbus/system_bus_socket");
    if (r>=0) r=sd_bus_set_bus_client(bus,1);
    if (r>=0) r=sd_bus_negotiate_fds(bus,1);
    if (r>=0) r=sd_bus_start(bus);
    if (r>=0) { stage="root"; r=ae_execution_client_new(bus,&profile,AE_STARTER_ROOT_UNIT,&client); }
    if (r==0) { stage="register"; r=ae_execution_client_register(client,&writer); }
    if (r==0) { stage="views"; r=ae_starter_views_open(&profile,&views); }
    if (r==0) { stage="target"; r=ae_starter_views_target(views,&target); }
    if (r==0) { stage="prepared"; r=ae_execution_client_prepared(client); }
    (void)ae_execution_client_close(&client);
    sd_bus_close(bus); bus=sd_bus_unref(bus);
    if (r==0) { stage="recheck"; r=ae_starter_views_target(views,&target); }
    if (r==0) {
        char *arguments[AE_FIXTURE_ARGUMENTS+2]={0};
        if (target.command.argc<1 || target.command.argc>AE_FIXTURE_ARGUMENTS+1) r=-EBADMSG;
        else {
            for (uint32_t n=0;n<target.command.argc;++n) arguments[n]=target.command.arguments[n];
            /* Fixed inert EXEC fixture contract. No inherited launcher/loader,
             * bus, credential, HOME, PATH or supervisor-notify environment. */
            char *environment[]={"LANG=C.UTF-8","AETHERI_EXPECTED_CONTEXT=" AE_STARTER_TARGET_CONTEXT,"AETHERI_SCENARIO=p0",NULL};
            struct ae_execute request={.image_fd=target.image_fd,.data_fd=target.data_fd,.error_fd=writer,
                .uid=1000,.gid=1000,.image=&target.image,.label=AE_STARTER_TARGET_CONTEXT,.argv=arguments,.envp=environment};
            ae_execute_target(&request);
        }
    }
    fprintf(stderr,"Starter failed at %s: %d; preserve execution journal until independent reconciliation\n",stage,r);
    (void)ae_starter_views_close(&views);
    ae_execute_fail(writer,AE_EXEC_PROFILE,r<0 && r>=-4095 ? -r : EIO);
}
