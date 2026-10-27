#include "authority/reservation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"reservation wire line %d\n",__LINE__); exit(1); } } while (0)
static sd_bus *connection;
static sd_bus_message *message(void)
{
    sd_bus_message *result=NULL;
    CHECK(sd_bus_message_new_signal(connection,&result,"/org/aetheri/Test","org.aetheri.Test1","Request")>=0);
    return result;
}
static void ready(sd_bus_message *value) { CHECK(sd_bus_message_seal(value,1,0)>=0 && sd_bus_message_rewind(value,1)>=0); }
static sd_bus_message *fixture(int fields,int bad_id,uint64_t revision,const char *action,const char *session,int extra)
{
    sd_bus_message *value=message();
    for (int n=0;n<10 && n<fields;++n) {
        uint8_t id[17]={0}; id[6]=0x40; id[8]=0x80; id[15]=(uint8_t)(n+1);
        CHECK(sd_bus_message_append_array(value,'y',id,n==bad_id ? 17U : 16U)>=0);
    }
    if (fields>10) CHECK(sd_bus_message_append(value,"t",revision)>=0);
    if (fields>11) CHECK(sd_bus_message_append(value,"s",action)>=0);
    if (fields>12) CHECK(sd_bus_message_append(value,"s",session)>=0);
    if (fields>13) CHECK(sd_bus_message_append(value,"s","none")>=0);
    if (extra) CHECK(sd_bus_message_append(value,"u",1U)>=0);
    ready(value); return value;
}
int main(void)
{
    CHECK(sd_bus_open_user(&connection)>=0);
    struct ae_launch_request output,marker; memset(&marker,0xa5,sizeof marker);
    sd_bus_message *value=fixture(14,-1,2,"main","p0",0);
    CHECK(ae_reservation_read(value,&output)==0 && output.lease[15]==2 && output.boot[15]==10 && output.install_revision==2 && strcmp(output.session,"p0")==0);
    struct ae_launch_request expected=output;
    sd_bus_message_unref(value);
    for (int fields=0;fields<14;++fields) {
        value=fixture(fields,-1,2,"main","p0",0); output=marker;
        CHECK(ae_reservation_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    }
    for (int n=0;n<10;++n) {
        value=fixture(14,n,2,"main","p0",0); output=marker;
        CHECK(ae_reservation_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    }
    char long_action[66],long_session[130]; memset(long_action,'a',65); long_action[65]=0; memset(long_session,'a',129); long_session[129]=0;
    const char *actions[]={"","Bad",long_action,"main","main","main","main"};
    const char *sessions[]={"p0","p0","p0",long_session,"bad\n","p0","p0"};
    for (int n=0;n<7;++n) {
        value=fixture(14,-1,n==5 ? UINT64_MAX : n==6 ? 0U : 2U,actions[n],sessions[n],0); output=marker;
        CHECK(ae_reservation_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    }
    value=fixture(14,-1,2,"main","p0",1); output=marker;
    CHECK(ae_reservation_read(value,&output)<0 && memcmp(&output,&marker,sizeof marker)==0); sd_bus_message_unref(value);
    value=message(); CHECK(ae_reservation_write(value,&expected)==0); ready(value);
    CHECK(ae_reservation_read(value,&output)==0 && memcmp(output.launch,expected.launch,16)==0 && strcmp(output.seat,"none")==0); sd_bus_message_unref(value);
    expected.boot[6]=0;
    value=message(); CHECK(ae_reservation_write(value,&expected)<0 && strcmp(sd_bus_message_get_signature(value,1),"")==0); sd_bus_message_unref(value);
    sd_bus_flush_close_unref(connection);
    puts("Reservation wire: independent fields, truncations, limits, tokens, integers, extra arguments and unchanged output passed.");
    return 0;
}
