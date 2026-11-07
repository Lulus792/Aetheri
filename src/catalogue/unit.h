#ifndef AETHERI_CATALOGUE_UNIT_H
#define AETHERI_CATALOGUE_UNIT_H
#include "catalogue/execution.h"
#include "catalogue/effect.h"
#include "core/unit_custody.h"

/* A separate unit-allocation ID, never an app StorageRef/DataEpoch or the
 * execution profile's private-view allocation digest. Recipe is the expected
 * primary fragment SHA-256, selected independently before any file creation. */
struct ae_unit_allocation { uint8_t id[16],recipe[32],placement[32]; int64_t prepared_unix_ms; };
struct ae_unit_allocation_snapshot {
    struct ae_unit_allocation allocation;
    uint8_t selection[32];
    int witnessed;
    int64_t witnessed_unix_ms;
};
/* Only fresh confirmed prepare returns create_required=1. Every retry/lost
 * acknowledgement gives no new filesystem effect right. Caller owns current
 * independent source/volume/MAC admission and immutable configuration. */
int ae_catalogue_prepare_unit(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_launch_context *,const struct ae_unit_allocation *,int *create_required);
/* Publish a digest of independently retained original unit/image witnesses.
 * Metadata alone proves no file creation, loaded configuration, caller or drain.
 * Fresh publication precedes REQUEST; exact historical retry changes nothing.
 * Neither method changes execution steps, gates, operations or generation leases. */
int ae_catalogue_witness_unit(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_launch_context *,const struct ae_unit_allocation *,const uint8_t selection[32],int64_t witnessed_unix_ms);
/* Historical original profile under the CURRENT Root owner of the same epoch.
 * May read an earlier runtime/boot, never adopt handles, permit replay or launch.
 * Outputs unchanged on all errors; no row is backfilled by migration/startup. */
int ae_catalogue_get_unit(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_effect_owner *current,struct ae_unit_allocation_snapshot *);
struct ae_unit_custody_step {
    enum ae_unit_effect effect;
    unsigned int node;
    int confirmed;
    int64_t intended_unix_ms,confirmed_unix_ms;
};
struct ae_unit_custody_snapshot {
    struct ae_unit_origin origin;
    unsigned int node_count,count;
    int64_t prepared_unix_ms;
    struct ae_unit_custody_step steps[AE_UNIT_EFFECTS_MAX];
};
/* Root v10 only. These are durable one-shot connection/RPC observations, never
 * permission to repeat effects or settle a unit/allocation/run/lease. Original
 * owner/context required for mutation; getter permits historical same-epoch
 * current-owner inspection without adopting any original handles. All outputs
 * remain unchanged on error. Only fresh confirmed commits return effect_required=1.
 * Exact retries return zero and cannot repeat filesystem, bus or process effects.
 * Caller serializes original source/broker admission and independently proves
 * original end/holder drain before invoking release effects. */
int ae_catalogue_prepare_unit_custody(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_launch_context *,const struct ae_unit_origin *,unsigned int node_count,int64_t,int *effect_required);
int ae_catalogue_intend_unit_effect(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_effect_owner *,enum ae_unit_effect,unsigned int node,int64_t,int *effect_required);
int ae_catalogue_confirm_unit_effect(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_effect_owner *,enum ae_unit_effect,unsigned int node,int64_t);
int ae_catalogue_get_unit_custody(struct ae_catalogue *,const struct ae_execution_profile *,
    const struct ae_effect_owner *,struct ae_unit_custody_snapshot *);
#endif
