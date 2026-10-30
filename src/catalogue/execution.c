#include "catalogue/execution.h"
#include "core/digest.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
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
int ae_execution_completion_encode(const struct ae_execution_completion *v,uint8_t output[AE_EXECUTION_COMPLETION_BYTES])
{
    if (v==NULL || output==NULL) return -EINVAL;
    const uint8_t *ids[]={v->system_epoch,v->system_runtime,v->boot,v->operation,v->launch,v->lease,v->invocation};
    unsigned int invocation=0;
    for (size_t n=0;n<7;++n) {
        if (n<6 && ((ids[n][6]>>4)!=4 || (ids[n][8]&0xc0)!=0x80)) return -EBADMSG;
        if (n==6) for (size_t m=0;m<16;++m) invocation|=ids[n][m];
    }
    if (!invocation || v->revision<1 || v->completed_unix_ms<0) return -EBADMSG;
    uint8_t bytes[AE_EXECUTION_COMPLETION_BYTES]; memcpy(bytes,"AETHLCR1",8);
    for (size_t n=0;n<7;++n) memcpy(bytes+8+16*n,ids[n],16);
    memcpy(bytes+120,v->profile_digest,32); memcpy(bytes+152,v->observation_digest,32);
    const int64_t numbers[]={v->revision,v->completed_unix_ms};
    for (size_t n=0;n<2;++n) for (size_t m=0;m<8;++m) bytes[184+8*n+m]=(uint8_t)((uint64_t)numbers[n]>>(56-8*m));
    memcpy(output,bytes,sizeof bytes); return 0;
}
int ae_execution_completion_decode(const uint8_t *bytes,size_t length,struct ae_execution_completion *output)
{
    if (bytes==NULL || output==NULL) return -EINVAL;
    if (length!=AE_EXECUTION_COMPLETION_BYTES || memcmp(bytes,"AETHLCR1",8)!=0) return -EBADMSG;
    struct ae_execution_completion v={0}; uint8_t *ids[]={v.system_epoch,v.system_runtime,v.boot,v.operation,v.launch,v.lease,v.invocation};
    for (size_t n=0;n<7;++n) memcpy(ids[n],bytes+8+16*n,16);
    memcpy(v.profile_digest,bytes+120,32); memcpy(v.observation_digest,bytes+152,32);
    int64_t *numbers[]={&v.revision,&v.completed_unix_ms};
    for (size_t n=0;n<2;++n) { uint64_t value=0; for (size_t m=0;m<8;++m) value=(value<<8)|bytes[184+8*n+m]; if (value>INT64_MAX) return -EBADMSG; *numbers[n]=(int64_t)value; }
    uint8_t check[AE_EXECUTION_COMPLETION_BYTES]; int r=ae_execution_completion_encode(&v,check);
    if (r==0) *output=v;
    return r;
}
int ae_execution_completion_match(const struct ae_execution_profile *p,const struct ae_execution_evidence *e,
    const uint8_t observation[32],const struct ae_execution_completion *v)
{
    if (e==NULL || observation==NULL || v==NULL) return -EINVAL;
    uint8_t bytes[AE_EXECUTION_COMPLETION_BYTES],hash[32]; int r=ae_execution_completion_encode(v,bytes);
    if (r==0) r=ae_execution_digest(p,hash);
    if (r==0 && (memcmp(hash,v->profile_digest,32)!=0 || memcmp(observation,v->observation_digest,32)!=0 || memcmp(e->invocation,v->invocation,16)!=0 ||
        memcmp(p->system_epoch,v->system_epoch,16)!=0 || memcmp(p->system_runtime,v->system_runtime,16)!=0 || memcmp(p->boot,v->boot,16)!=0 ||
        memcmp(p->root_operation,v->operation,16)!=0 || memcmp(p->launch,v->launch,16)!=0 || memcmp(p->lease,v->lease,16)!=0)) r=-ESTALE;
    return r;
}
int ae_execution_diagnostic_encode(const struct ae_execution_profile *p,const struct ae_execution_evidence *e,
    const struct ae_execution_diagnostic *d,uint8_t output[32],char reason[64])
{
    if (e==NULL || d==NULL || output==NULL || reason==NULL) return -EINVAL;
    unsigned int nonzero=0; for (size_t n=0;n<16;++n) nonzero|=e->invocation[n];
    if (!nonzero || d->kind<AE_EXECUTION_DIAGNOSTIC_UNKNOWN || d->kind>AE_EXECUTION_DIAGNOSTIC_OBSERVER) return -EINVAL;
    if (d->kind==AE_EXECUTION_DIAGNOSTIC_FAILURE) {
        if (d->phase<1 || d->phase>AE_EXECUTION_PHASE_MAX || d->error<1 || d->error>4095) return -EINVAL;
    } else if (d->phase || (d->kind==AE_EXECUTION_DIAGNOSTIC_OBSERVER ? d->error<1 || d->error>4095 : d->error!=0)) return -EINVAL;
    uint8_t bytes[100]={0},hash[32]; char text[64]={0}; memcpy(bytes,"AETHEXD1",8);
    int r=ae_execution_digest(p,bytes+8); if (r!=0) return r;
    memcpy(bytes+40,e->invocation,16); memcpy(bytes+56,e->context_digest,32);
    uint32_t fields[]={(uint32_t)d->kind,d->phase,d->error};
    for (size_t n=0;n<3;++n) for (size_t m=0;m<4;++m) bytes[88+4*n+m]=(uint8_t)(fields[n]>>(24-8*m));
    int size=d->kind==AE_EXECUTION_DIAGNOSTIC_UNKNOWN ? snprintf(text,sizeof text,"exec_channel_unknown") :
        d->kind==AE_EXECUTION_DIAGNOSTIC_MALFORMED ? snprintf(text,sizeof text,"exec_channel_malformed") :
        d->kind==AE_EXECUTION_DIAGNOSTIC_OBSERVER ? snprintf(text,sizeof text,"exec_observer_e%u",d->error) :
        snprintf(text,sizeof text,"exec_failure_p%u_e%u",d->phase,d->error);
    if (size<1 || size>=(int)sizeof text) return -EOVERFLOW;
    r=ae_sha256_bytes(bytes,sizeof bytes,hash);
    if (r==0) { memcpy(output,hash,32); memcpy(reason,text,sizeof text); }
    return r;
}
