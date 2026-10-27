#include "catalogue/data.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

static int valid_id(const uint8_t id[16]) { return (id[6]>>4)==4 && (id[8]&0xc0)==0x80; }

int ae_data_enrollment_valid(const struct ae_data_enrollment *request)
{
    if (request==NULL || request->install_revision<1 || request->base_revision<1 || request->schema_version<0) return -EINVAL;
    const uint8_t *ids[]={request->install,request->publisher,request->system_epoch,request->user_epoch};
    for (size_t n=0;n<4;++n) if (!valid_id(ids[n])) return -EBADMSG;
    for (size_t n=0;n<AE_OWNER_FIELDS;++n) {
        if (!valid_id(request->owner.id[n])) return -EBADMSG;
        for (size_t m=0;m<n;++m) if (memcmp(request->owner.id[n],request->owner.id[m],16)==0) return -EBADMSG;
    }
    uint8_t bytes[AE_ALLOCATION_BYTES];
    if (!ae_owner_matches(&request->owner,&request->allocation.owner) ||
        ae_allocation_encode(&request->allocation,bytes,sizeof bytes)!=0) return -EBADMSG;
    return 0;
}

int ae_data_enrollment_decode(const uint8_t *bytes,size_t size,struct ae_data_enrollment *output)
{
    if (bytes==NULL || output==NULL) return -EINVAL;
    if (size!=AE_DATA_CONFIG_BYTES || memcmp(bytes,"AETHDTA1",8)!=0) return -EBADMSG;
    struct ae_data_enrollment request={0};
    if (ae_owner_decode(bytes+8,AE_OWNER_BYTES,&request.owner)!=0) return -EBADMSG;
    memcpy(request.install,bytes+112,16); memcpy(request.publisher,bytes+128,16);
    memcpy(request.system_epoch,bytes+144,16); memcpy(request.user_epoch,bytes+160,16);
    int64_t *numbers[]={&request.install_revision,&request.base_revision,&request.schema_version};
    for (size_t n=0;n<3;++n) {
        uint64_t value=0;
        for (size_t m=0;m<8;++m) value=(value<<8)|bytes[176+8*n+m];
        if (value>INT64_MAX) return -EBADMSG;
        *numbers[n]=(int64_t)value;
    }
    if (ae_allocation_decode(bytes+200,AE_ALLOCATION_BYTES,&request.allocation)!=0 || ae_data_enrollment_valid(&request)!=0) return -EBADMSG;
    *output=request;
    return 0;
}
