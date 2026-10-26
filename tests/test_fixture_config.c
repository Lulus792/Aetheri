#include "authority/fixture.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "fixture config line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void put(uint8_t *bytes, uint64_t value, size_t size)
{
    for (size_t n=0; n<size; ++n) { bytes[size-n-1U]=(uint8_t)value; value >>= 8; }
}
int main(void)
{
    uint8_t bytes[AE_FIXTURE_CONFIG_BYTES+1U] = {0};
    memcpy(bytes,"AETHFIC1",8);
    for (size_t n=0; n<10; ++n) { bytes[8U+n*16U+6U]=0x40; bytes[8U+n*16U+8U]=0x80; bytes[8U+n*16U+15U]=(uint8_t)(n+1U); }
    memset(bytes+168,0x12,32); put(bytes+200,7,8); put(bytes+208,123456,8); put(bytes+216,234567,8);
    for (size_t offset=224; offset<=260; offset+=36) {
        put(bytes+offset,0xef53,8); memcpy(bytes+offset+8,"01234567",8);
        put(bytes+offset+16,offset,8); put(bytes+offset+24,1777777777,8); put(bytes+offset+32,999999999,4);
    }
    memset(bytes+296,0x34,32);
    struct ae_fixture_config config, unchanged;
    memset(&unchanged,0xa5,sizeof unchanged);
    CHECK(ae_fixture_config_decode(bytes,AE_FIXTURE_CONFIG_BYTES,&config)==0);
    CHECK(config.request.plan[15]==1 && config.request.boot[15]==10 && config.request.os_uid==1000);
    CHECK(config.request.scope_revision==7 && config.request.expires_boottime_ns==123456 && config.request.accepted_unix_ms==234567);
    CHECK(config.source.inode==224 && config.staging.inode==260 && config.source.birth_nanoseconds==999999999);
    CHECK(config.request.artifact[0]==0x12 && config.request.expected_intent[31]==0x34);
    for (size_t size=0; size<=sizeof bytes; ++size) if (size!=AE_FIXTURE_CONFIG_BYTES) {
        config=unchanged;
        CHECK(ae_fixture_config_decode(bytes,size,&config)==-EBADMSG && memcmp(&config,&unchanged,sizeof config)==0);
    }
    for (size_t n=0; n<10; ++n) {
        bytes[8U+n*16U+6U]=0;
        config=unchanged;
        CHECK(ae_fixture_config_decode(bytes,AE_FIXTURE_CONFIG_BYTES,&config)==-EBADMSG && memcmp(&config,&unchanged,sizeof config)==0);
        bytes[8U+n*16U+6U]=0x40;
    }
    put(bytes+256,1000000000,4);
    CHECK(ae_fixture_config_decode(bytes,AE_FIXTURE_CONFIG_BYTES,&config)==-EBADMSG);
    put(bytes+256,999999999,4);
    bytes[200]=0x80;
    CHECK(ae_fixture_config_decode(bytes,AE_FIXTURE_CONFIG_BYTES,&config)==-EBADMSG);
    CHECK(ae_fixture_config_decode(NULL,AE_FIXTURE_CONFIG_BYTES,&config)==-EINVAL);
    CHECK(ae_fixture_config_decode(bytes,AE_FIXTURE_CONFIG_BYTES,NULL)==-EINVAL);
    puts("Protected fixture configuration bounds, identities and object-witness decoding passed.");
    return 0;
}
