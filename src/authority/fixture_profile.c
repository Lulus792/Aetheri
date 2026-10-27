#include "authority/fixture.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

#define ROOT "/var/lib/aetheri-p0"

const struct ae_fixture_registry *ae_fixture_registry(enum ae_fixture_selection selection)
{
    static const struct ae_fixture_registry profiles[]={
        {ROOT "/registry/fixture.conf",ROOT "/registry/fixture.manifest",ROOT "/registry/fixture-store.conf",ROOT "/registry/fixture-activation.conf",ROOT "/registry/admission.conf",
         ROOT "/registry/app-data-owner.conf",ROOT "/registry/app-data.conf","system_u:object_r:aetheri_app_data_t:s0:c33,c34"},
        {ROOT "/registry/exec-fixture.conf",ROOT "/registry/exec-fixture.manifest",ROOT "/registry/exec-fixture-store.conf",ROOT "/registry/exec-fixture-activation.conf",ROOT "/registry/exec-admission.conf",
         ROOT "/registry/exec-app-data-owner.conf",ROOT "/registry/exec-app-data.conf","system_u:object_r:aetheri_app_data_t:s0:c35,c36"}
    };
    return selection==AE_FIXTURE_ORIGINAL || selection==AE_FIXTURE_EXEC ? &profiles[selection] : NULL;
}

static uint64_t number(const uint8_t *bytes, size_t size)
{
    uint64_t value = 0;
    for (size_t n = 0; n < size; ++n) value = (value << 8) | bytes[n];
    return value;
}
static int witness_decode(const uint8_t *bytes, struct ae_root_identity *root)
{
    uint64_t type = number(bytes, 8), inode = number(bytes + 16, 8), birth = number(bytes + 24, 8);
    uint64_t nanos = number(bytes + 32, 4);
    if (type > INT64_MAX || inode > INT64_MAX || birth > INT64_MAX || nanos >= 1000000000U) return -EBADMSG;
    root->filesystem_type = (int64_t)type; root->inode = inode;
    root->birth_seconds = (int64_t)birth; root->birth_nanoseconds = (uint32_t)nanos;
    root->birth_present = inode != 0;
    memcpy(root->filesystem_id, bytes + 8, 8);
    return 0;
}
int ae_fixture_config_decode(const uint8_t *bytes, size_t size, struct ae_fixture_config *output)
{
    if (bytes == NULL || output == NULL) return -EINVAL;
    if (size != AE_FIXTURE_CONFIG_BYTES || memcmp(bytes, "AETHFIC1", 8) != 0) return -EBADMSG;
    struct ae_fixture_config config = {0};
    struct ae_fixture_request *request = &config.request;
    uint8_t *ids[] = {request->plan, request->operation, request->actor, request->request_key, request->scope,
        request->install, request->generation, request->slot, request->publisher, request->boot};
    for (size_t n = 0; n < sizeof ids / sizeof ids[0]; ++n) {
        memcpy(ids[n], bytes + 8U + n * 16U, 16);
        if ((ids[n][6] >> 4) != 4 || (ids[n][8] & 0xc0) != 0x80) return -EBADMSG;
    }
    memcpy(request->artifact, bytes + 168, 32);
    uint64_t revision = number(bytes + 200, 8), expires = number(bytes + 208, 8), accepted = number(bytes + 216, 8);
    if (revision < 1 || revision > INT64_MAX || expires > INT64_MAX || accepted > INT64_MAX) return -EBADMSG;
    request->scope_revision = (int64_t)revision; request->expires_boottime_ns = (int64_t)expires;
    request->accepted_unix_ms = (int64_t)accepted; request->os_uid = 1000;
    memcpy(request->expected_intent, bytes + 296, 32);
    int result = witness_decode(bytes + 224, &config.source);
    if (result == 0) result = witness_decode(bytes + 260, &config.staging);
    if (result == 0) *output = config;
    return result;
}

int ae_fixture_store_config_decode(const uint8_t *bytes,size_t size,struct ae_fixture_store_config *output)
{
    if (bytes==NULL || output==NULL) return -EINVAL;
    if (size!=AE_FIXTURE_STORE_CONFIG_BYTES || memcmp(bytes,"AETHSTO1",8)!=0) return -EBADMSG;
    struct ae_fixture_store_config config={0};
    memcpy(config.stage_digest,bytes+8,32);
    int result=witness_decode(bytes+40,&config.store);
    if (result==0) result=witness_decode(bytes+76,&config.container);
    if (result==0 && (config.store.inode==0 || config.container.inode==0)) result=-EBADMSG;
    if (result==0) *output=config;
    return result;
}

int ae_fixture_activation_config_decode(const uint8_t *bytes,size_t size,struct ae_fixture_activation *output)
{
    if (bytes==NULL || output==NULL) return -EINVAL;
    if (size!=AE_FIXTURE_ACTIVATION_CONFIG_BYTES || memcmp(bytes,"AETHACT1",8)!=0) return -EBADMSG;
    struct ae_fixture_activation activation={0};
    const uint64_t install=number(bytes+40,8),generation=number(bytes+48,8),gate=number(bytes+56,8),stamp=number(bytes+64,8);
    if (install<1 || install>=INT64_MAX || generation<1 || generation>=INT64_MAX || gate<1 || gate>INT64_MAX || stamp>INT64_MAX) return -EBADMSG;
    memcpy(activation.publication_evidence,bytes+8,32);
    activation.install_revision=(int64_t)install; activation.generation_revision=(int64_t)generation;
    activation.gate_revision=(int64_t)gate; activation.completed_unix_ms=(int64_t)stamp;
    *output=activation;
    return 0;
}
