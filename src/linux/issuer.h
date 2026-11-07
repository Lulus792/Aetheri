#ifndef AETHERI_LINUX_ISSUER_H
#define AETHERI_LINUX_ISSUER_H
#include "core/unit_custody.h"
#include "core/unit_issuer.h"
#include "linux/process.h"
#include "linux/image.h"

struct ae_issuer;
/* Calling Root process only, independently protected literal service/label and
 * immutable original Root executable observation. The connection must be the
 * original exclusive bus in origin, never an app/caller-selected bus/name.
 * Retains the bus-authenticated self ProcessFD, actual current MainPID/invocation/
 * cgroup and pidfs identity. Does not infer service/image admission from a digest
 * alone, check loaded libraries or establish volume/clone/owner-replacement proof.
 * No process/unit control or effect; output starts NULL and errors preserve it. */
int ae_issuer_open(sd_bus *,const struct ae_unit_origin *,const char *expected_unit,
    const char *expected_label,const struct ae_image *,struct ae_issuer **);
int ae_issuer_verify(struct ae_issuer *,sd_bus *,const struct ae_unit_origin *);
int ae_issuer_snapshot(struct ae_issuer *,struct ae_unit_issuer *);
/* Local ProcessFD retirement only. No original bus closure, holder drain,
 * authority transfer, effect replay or lease/SQL settlement follows. */
int ae_issuer_close(struct ae_issuer **);
#endif
