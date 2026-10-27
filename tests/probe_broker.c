#include "core/owner.h"
#include <stdio.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "broker probe line %d\n", __LINE__); return 1; } } while (0)

int main(int argc, char **argv)
{
    CHECK((argc == 5 || (argc==6 && strcmp(argv[5],"--direct-user-bus")==0)) && geteuid() == 1000);
    uint8_t ids[4][16];
    for (unsigned int n = 0; n < 4; ++n) CHECK(ae_id_from_hex(argv[n + 1], strlen(argv[n + 1]), ids[n]) == 0);
    sd_bus *bus = NULL;
    CHECK(sd_bus_new(&bus) >= 0 && sd_bus_set_address(bus, "unix:path=/run/dbus/system_bus_socket") >= 0);
    CHECK(sd_bus_set_bus_client(bus, 1) >= 0 && sd_bus_set_method_call_timeout(bus, 2000000) >= 0 && sd_bus_start(bus) >= 0);
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL, *request = NULL;
    CHECK(sd_bus_call_method(bus, "org.aetheri.System1", "/org/aetheri/Management1", "org.aetheri.Management1", "GetProtocolInfo", &error, &reply, "") >= 0);
    CHECK(sd_bus_message_has_signature(reply, "uuasayay"));
    sd_bus_message_unref(reply); reply = NULL;
    CHECK(sd_bus_message_new_method_call(bus, &request, "org.aetheri.System1", "/org/aetheri/Broker1", "org.aetheri.Broker1", "Register") >= 0);
    for (unsigned int n = 0; n < 4; ++n) CHECK(sd_bus_message_append_array(request, 'y', ids[n], 16) >= 0);
    CHECK(sd_bus_call(bus, request, 2000000, &error, &reply) < 0);
    CHECK(sd_bus_error_has_name(&error, "org.aetheri.Error1.NotAuthorized"));
    sd_bus_error_free(&error); sd_bus_message_unref(request); sd_bus_message_unref(reply); reply = NULL;
    CHECK(sd_bus_message_new_method_call(bus,&request,"org.aetheri.System1","/org/aetheri/Catalogue1","org.aetheri.Catalogue1","GetReservationReceipt")>=0);
    CHECK(sd_bus_message_append_array(request,'y',ids[0],16)>=0);
    CHECK(sd_bus_call(bus,request,2000000,&error,&reply)<0 && sd_bus_error_has_name(&error,"org.aetheri.Error1.NotAuthorized"));
    sd_bus_error_free(&error); sd_bus_message_unref(request); sd_bus_message_unref(reply); reply=NULL;
    CHECK(sd_bus_message_new_method_call(bus, &request, "org.aetheri.System1", "/org/aetheri/Catalogue1", "org.aetheri.Catalogue1", "ResolveAppInstall") >= 0);
    CHECK(sd_bus_message_append_array(request, 'y', ids[0], 16) >= 0);
    CHECK(sd_bus_message_append_array(request, 'y', ids[3], 16) >= 0);
    CHECK(sd_bus_message_append_array(request, 'y', ids[1], 16) >= 0);
    CHECK(sd_bus_call(bus, request, 2000000, &error, &reply) < 0);
    CHECK(sd_bus_error_has_name(&error, "org.aetheri.Error1.NotAuthorized"));
    sd_bus_error_free(&error); sd_bus_message_unref(request); sd_bus_message_unref(reply); reply = NULL;
    CHECK(sd_bus_message_new_method_call(bus, &request, "org.aetheri.System1", "/org/aetheri/Broker1", "org.aetheri.Broker1", "Check") >= 0);
    CHECK(sd_bus_message_append_array(request, 'y', ids[3], 16) >= 0);
    CHECK(sd_bus_call(bus, request, 2000000, &error, &reply) < 0);
    CHECK(sd_bus_error_has_name(&error, "org.aetheri.Error1.NotAuthorized"));
    sd_bus_error_free(&error); sd_bus_message_unref(request); sd_bus_message_unref(reply); reply = NULL;
    CHECK(sd_bus_message_new_method_call(bus, &request, "org.aetheri.System1", "/org/aetheri/Runtime1", "org.aetheri.Runtime1", "GetDispatch") >= 0);
    CHECK(sd_bus_message_append_array(request, 'y', ids[3], 16) >= 0);
    CHECK(sd_bus_call(bus, request, 2000000, &error, &reply) < 0);
    CHECK(sd_bus_error_has_name(&error, "org.aetheri.Error1.NotAuthorized"));
    sd_bus_error_free(&error); sd_bus_message_unref(request); sd_bus_message_unref(reply);
    request=NULL; reply=NULL;
    CHECK(sd_bus_message_new_method_call(bus,&request,"org.aetheri.System1","/org/aetheri/Runtime1","org.aetheri.Runtime1","ReserveLaunch")>=0);
    for (size_t n=0;n<10;++n) CHECK(sd_bus_message_append_array(request,'y',ids[n%4],16)>=0);
    CHECK(sd_bus_message_append(request,"tsss",UINT64_C(1),"main","p0","none")>=0);
    CHECK(sd_bus_call(bus,request,2000000,&error,&reply)<0 && sd_bus_error_has_name(&error,"org.aetheri.Error1.NotAuthorized"));
    CHECK(strcmp(error.message,"Active registered broker required")==0);
    sd_bus_error_free(&error); sd_bus_message_unref(request); sd_bus_message_unref(reply); request=NULL; reply=NULL;
    CHECK(sd_bus_message_new_method_call(bus,&request,"org.aetheri.System1","/org/aetheri/Runtime1","org.aetheri.Runtime1","CloseLaunch")>=0);
    for (size_t n=0;n<4;++n) CHECK(sd_bus_message_append_array(request,'y',ids[n],16)>=0);
    CHECK(sd_bus_message_append(request,"t",UINT64_C(1))>=0);
    CHECK(sd_bus_call(bus,request,2000000,&error,&reply)<0 && sd_bus_error_has_name(&error,"org.aetheri.Error1.NotAuthorized"));
    CHECK(strcmp(error.message,"Active registered broker required")==0);
    sd_bus_error_free(&error); sd_bus_message_unref(request); sd_bus_message_unref(reply);
    sd_bus *user=NULL;
    CHECK(sd_bus_new(&user)>=0 && sd_bus_set_address(user,argc==6 ? "unix:path=/run/user/1000/bus" : "unix:path=/var/lib/aetheri-p0/user-bus")>=0);
    CHECK(sd_bus_set_bus_client(user,1)>=0 && sd_bus_set_method_call_timeout(user,2000000)>=0 && sd_bus_start(user)>=0);
    reply=NULL;
    CHECK(sd_bus_call_method(user,"org.aetheri.User1","/org/aetheri/Development1","org.aetheri.Development1","RefreshFixtureInstall",&error,&reply,"")<0);
    CHECK(sd_bus_error_has_name(&error,"org.aetheri.Error1.NotAuthorized"));
    sd_bus_error_free(&error); sd_bus_message_unref(reply); reply=NULL;
    CHECK(sd_bus_call_method(user,"org.aetheri.User1","/org/aetheri/Development1","org.aetheri.Development1","TestFixtureReservation",&error,&reply,"")<0);
    CHECK(sd_bus_error_has_name(&error,"org.aetheri.Error1.NotAuthorized"));
    sd_bus_error_free(&error); sd_bus_message_unref(reply); reply=NULL;
    const char *launch_methods[]={"PrepareFixtureLaunch","CancelFixtureLaunch"};
    for (size_t n=0;n<2;++n) {
        CHECK(sd_bus_call_method(user,"org.aetheri.User1","/org/aetheri/Development1","org.aetheri.Development1",launch_methods[n],&error,&reply,"")<0);
        CHECK(sd_bus_error_has_name(&error,"org.aetheri.Error1.NotAuthorized"));
        sd_bus_error_free(&error); sd_bus_message_unref(reply); reply=NULL;
    }
    sd_bus_flush_close_unref(user);
    sd_bus_flush_close_unref(bus);
    puts("broker: public transport works; copied identity cannot register/check/use runtime");
    return 0;
}
