#include "catalogue/setup.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

int ae_setup_decode(const uint8_t *bytes,size_t size,struct ae_setup_profile *output)
{
    if (bytes==NULL || output==NULL) return -EINVAL;
    if (size!=AE_SETUP_BYTES || memcmp(bytes,"AETHSUP1",8)!=0) return -EBADMSG;
    struct ae_setup_profile profile={0};
    uint8_t *ids[]={profile.system_epoch,profile.system_runtime,profile.broker_epoch,profile.broker_runtime,profile.boot,profile.account,
        profile.install,profile.generation,profile.data,profile.storage,profile.launch,profile.lease,profile.root_operation,profile.user_operation};
    for (size_t n=0;n<14;++n) {
        const uint8_t *id=bytes+8+16*n;
        if ((id[6]>>4)!=4 || (id[8]&0xc0)!=0x80) return -EBADMSG;
        memcpy(ids[n],id,16);
    }
    int64_t *numbers[]={&profile.install_revision,&profile.install_gate_revision,&profile.data_gate_revision};
    for (size_t n=0;n<3;++n) {
        uint64_t value=0; for (size_t m=0;m<8;++m) value=(value<<8)|bytes[232+8*n+m];
        if (value<1 || value>INT64_MAX) return -EBADMSG;
        *numbers[n]=(int64_t)value;
    }
    memcpy(profile.artifact,bytes+256,32); memcpy(profile.image_digest,bytes+288,32); *output=profile; return 0;
}
