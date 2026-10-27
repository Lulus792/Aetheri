#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <systemd/sd-bus.h>
#include <unistd.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"projection controller line %d\n",__LINE__); return 1; } } while (0)
/* Root selects this reviewed cap-zero user-domain helper in a unique system unit;
 * it cannot choose another installation, SQL, path, policy or supervisor context. */
int main(int argc,char **argv)
{
    CHECK(argc==1 || (argc==2 && (strcmp(argv[1],"--reserve")==0 || strcmp(argv[1],"--launch-prepare")==0 || strcmp(argv[1],"--launch-cancel")==0)));
    CHECK(geteuid()==1000);
    sd_bus *bus=NULL;
    CHECK(sd_bus_new(&bus)>=0 && sd_bus_set_address(bus,"unix:path=/var/lib/aetheri-p0/user-bus")>=0);
    CHECK(sd_bus_set_bus_client(bus,1)>=0 && sd_bus_set_method_call_timeout(bus,argc==1 ? 5000000 : 15000000)>=0 && sd_bus_start(bus)>=0);
    sd_bus_error error=SD_BUS_ERROR_NULL;
    sd_bus_message *reply=NULL;
    const char *method=argc==1 ? "RefreshFixtureInstall" : strcmp(argv[1],"--reserve")==0 ? "TestFixtureReservation" :
        strcmp(argv[1],"--launch-prepare")==0 ? "PrepareFixtureLaunch" : "CancelFixtureLaunch";
    int result=sd_bus_call_method(bus,"org.aetheri.User1","/org/aetheri/Development1","org.aetheri.Development1",method,&error,&reply,"");
    if (result<0) fprintf(stderr,"projection controller: %s\n",error.message==NULL ? "call failed" : error.message);
    CHECK(result>=0 && sd_bus_message_has_signature(reply,"aytt"));
    const void *id=NULL; size_t size=0;
    uint64_t install_revision=0,local_revision=0;
    CHECK(sd_bus_message_read_array(reply,'y',&id,&size)>0 && size==16);
    CHECK(sd_bus_message_read(reply,"tt",&install_revision,&local_revision)>0 && sd_bus_message_at_end(reply,1)==1);
    CHECK(install_revision>0 && install_revision<=INT64_MAX && local_revision>0 && local_revision<=INT64_MAX);
    fputs(argc==1 ? "projection " : strcmp(argv[1],"--reserve")==0 ? "reservation " : "launch ",stdout);
    for (size_t n=0;n<16;++n) printf("%02x",(unsigned int)((const uint8_t *)id)[n]);
    printf(" %" PRIu64 " %" PRIu64 "\n",install_revision,local_revision);
    sd_bus_message_unref(reply); sd_bus_error_free(&error); sd_bus_flush_close_unref(bus);
    return 0;
}
