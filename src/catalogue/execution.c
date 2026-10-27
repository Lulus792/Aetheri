#include "catalogue/execution.h"
#include "core/digest.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

static int valid(const struct ae_execution_profile *p)
{
    if (p==NULL) return -EINVAL;
    const uint8_t *ids[]={p->system_epoch,p->system_runtime,p->broker_epoch,p->broker_runtime,p->boot,p->account,
        p->install,p->generation,p->data,p->storage,p->launch,p->lease,p->root_operation,p->user_operation};
    for (size_t n=0;n<14;++n) if ((ids[n][6]>>4)!=4 || (ids[n][8]&0xc0)!=0x80) return -EBADMSG;
    if (p->install_revision<1 || p->install_gate_revision<1 || p->data_gate_revision<1) return -EBADMSG;
    /* Artifact digests may be all zero as byte values. Provenance/verification
     * is owed by the authority; this codec does not turn hashes into trust. */
    struct ae_launch_request request={.install_revision=p->install_revision};
    const uint8_t *source[]={p->system_epoch,p->lease,p->launch,p->broker_epoch,p->broker_runtime,p->account,p->install,p->generation,p->data,p->boot};
    uint8_t *target[]={request.system_epoch,request.lease,request.launch,request.broker_epoch,request.broker_runtime,request.account,request.install,request.generation,request.data,request.boot};
    for (size_t n=0;n<10;++n) memcpy(target[n],source[n],16);
    strcpy(request.action,"run"); strcpy(request.session,"p0"); strcpy(request.seat,"none");
    uint8_t digest[32]; int result=ae_launch_fingerprint(&request,digest);
    if (result==0 && memcmp(digest,p->request_digest,32)!=0) result=-EBADMSG;
    return result;
}
int ae_execution_encode(const struct ae_execution_profile *p,uint8_t output[AE_EXECUTION_BYTES])
{
    if (output==NULL) return -EINVAL;
    int result=valid(p); if (result!=0) return result;
    uint8_t bytes[AE_EXECUTION_BYTES]={0}; memcpy(bytes,"AETHEXE1",8);
    const uint8_t *ids[]={p->system_epoch,p->system_runtime,p->broker_epoch,p->broker_runtime,p->boot,p->account,
        p->install,p->generation,p->data,p->storage,p->launch,p->lease,p->root_operation,p->user_operation};
    for (size_t n=0;n<14;++n) memcpy(bytes+8+16*n,ids[n],16);
    const int64_t numbers[]={p->install_revision,p->install_gate_revision,p->data_gate_revision};
    for (size_t n=0;n<3;++n) for (size_t m=0;m<8;++m) bytes[232+8*n+m]=(uint8_t)((uint64_t)numbers[n]>>(56-8*m));
    memcpy(bytes+256,p->artifact,32); memcpy(bytes+288,p->starter_digest,32);
    memcpy(bytes+320,p->request_digest,32); memcpy(bytes+352,p->allocation_digest,32);
    memcpy(output,bytes,sizeof bytes); return 0;
}
int ae_execution_decode(const uint8_t *bytes,size_t size,struct ae_execution_profile *output)
{
    if (bytes==NULL || output==NULL) return -EINVAL;
    if (size!=AE_EXECUTION_BYTES || memcmp(bytes,"AETHEXE1",8)!=0) return -EBADMSG;
    struct ae_execution_profile p={0};
    uint8_t *ids[]={p.system_epoch,p.system_runtime,p.broker_epoch,p.broker_runtime,p.boot,p.account,
        p.install,p.generation,p.data,p.storage,p.launch,p.lease,p.root_operation,p.user_operation};
    for (size_t n=0;n<14;++n) memcpy(ids[n],bytes+8+16*n,16);
    int64_t *numbers[]={&p.install_revision,&p.install_gate_revision,&p.data_gate_revision};
    for (size_t n=0;n<3;++n) {
        uint64_t v=0; for (size_t m=0;m<8;++m) v=(v<<8)|bytes[232+8*n+m];
        if (v<1 || v>INT64_MAX) return -EBADMSG;
        *numbers[n]=(int64_t)v;
    }
    memcpy(p.artifact,bytes+256,32); memcpy(p.starter_digest,bytes+288,32);
    memcpy(p.request_digest,bytes+320,32); memcpy(p.allocation_digest,bytes+352,32);
    int result=valid(&p); if (result==0) *output=p; return result;
}
int ae_execution_digest(const struct ae_execution_profile *p,uint8_t output[32])
{
    if (output==NULL) return -EINVAL;
    uint8_t bytes[AE_EXECUTION_BYTES]; int result=ae_execution_encode(p,bytes);
    return result==0 ? ae_sha256_bytes(bytes,sizeof bytes,output) : result;
}
