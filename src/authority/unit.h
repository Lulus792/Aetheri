#ifndef AETHERI_AUTHORITY_UNIT_H
#define AETHERI_AUTHORITY_UNIT_H
#include "authority/execution.h"
#include "linux/unit.h"
#include "linux/image.h"

struct ae_root_unit;
struct ae_root_unit_image {
    int directory_fd,image_fd;
    char directory[4096],leaf[AE_UNIT_TEXT],label[512];
    struct ae_root_identity parent;
    struct ae_image image;
};
/* Compiled current caller/data/mutation admission. The independent unit/image
 * selection is checked separately by this owner, before and after the callout. */
typedef int (*ae_root_unit_admit)(void *,const struct ae_execution_profile *);
struct ae_root_unit_status { struct ae_unit_status unit; int used,held,error; };
/* Root/process-owned single-threaded setup BEFORE REQUEST. MOVE the unused unit
 * owner on success only. Match an independently authorized protected selection,
 * exact launch-derived name and held/named starter ELF against AETHEXE1.
 * Copies the image selection/duplicates FDs. No persistent registry enrollment,
 * Ref/Subscribe/Start or effect permission is created by this constructor.
 * Caller owes original unused allocation, volume/configuration/MAC admission,
 * mutation exclusion and trusted host buses; never adopt candidate self-claims. */
int ae_root_unit_new(struct ae_catalogue *,struct ae_unit **,
    const struct ae_unit_profile *independent_selection,const struct ae_root_unit_image *,
    const struct ae_execution_profile *,const struct ae_launch_context *,
    ae_root_unit_admit,void *,struct ae_root_unit **);
/* ae_execution_effect request adapter ONLY for the original coordinator's
 * confirmed fresh REQUEST. Read-only journal checking never grants replay.
 * Retains the original owner/runtime through Subscribe/Ref/Start and afterwards,
 * including lost replies, source fencing and completed jobs. No lease release. */
int ae_root_unit_request(void *,const struct ae_execution_profile *,const struct ae_dispatch_snapshot *);
sd_bus *ae_root_unit_bus(struct ae_root_unit *); /* Borrow only to drive events. */
int ae_root_unit_status(struct ae_root_unit *,struct ae_root_unit_status *);
/* Requires independently admitted ORIGINAL end/holder evidence; current broker
 * delegation/fresh-source admission is deliberately not a cleanup prerequisite.
 * Does not stop processes, settle SQL/leases or establish durable recovery. */
int ae_root_unit_retire(struct ae_root_unit *,ae_start_guard original_end,void *);
int ae_root_unit_close(struct ae_root_unit **);
/* Local connection/observer retirement only. Preserves every durable obligation
 * and unknown external effect; never a substitute for end/holder evidence. */
int ae_root_unit_abandon(struct ae_root_unit **);
#endif
