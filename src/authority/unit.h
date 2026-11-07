#ifndef AETHERI_AUTHORITY_UNIT_H
#define AETHERI_AUTHORITY_UNIT_H
#include "authority/execution.h"
#include "linux/unit.h"
#include "linux/image.h"
#include "linux/issuer.h"
#include "catalogue/unit.h"

struct ae_root_unit;
struct ae_root_unit_image {
    int directory_fd,image_fd;
    char directory[4096],leaf[AE_UNIT_TEXT],label[512];
    struct ae_root_identity parent;
    struct ae_image image;
};
struct ae_root_unit_issuer_selection {
    char unit[256],label[512];
    struct ae_image image;
};
/* Compiled current caller/data/mutation admission. The independent unit/image
 * selection is checked separately by this owner, before and after the callout. */
typedef int (*ae_root_unit_admit)(void *,const struct ae_execution_profile *);
/* Prepare native intent, then create the primary fragment once using O_TMPFILE,
 * full flush and create-only link. Directory/parent/content are independently
 * selected, copied before callouts and never supplied by an app. No daemon reload
 * or service start. Errors preserve any possible named file and original intent;
 * never retry allocation or adopt a matching name. The output FD starts negative.
 * Caller still owns original volume/MAC/configuration and current data admission. */
struct ae_root_unit_create { int directory_fd; const char *directory; struct ae_root_identity parent; const uint8_t *bytes; size_t size; };
/* AETHUPL1 binds the independently selected directory and original parent
 * observation before file effects, not just the eventual witnessed selection. */
int ae_root_unit_placement_digest(const char *directory,const struct ae_root_identity *,uint8_t output[32]);
int ae_root_unit_create(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_launch_context *,
    const struct ae_unit_allocation *,const struct ae_root_unit_create *,ae_root_unit_admit,void *,int *,struct ae_unit_file *);
/* AETHUNT1 = SHA256(tag + execution digest + AETHUNP1 profile digest +
 * AETHUIM1 image-selection digest). No FDs/native padding or new admission. */
int ae_root_unit_selection_digest(const struct ae_execution_profile *,const struct ae_unit_profile *,
    const struct ae_root_unit_image *,uint8_t output[32]);
struct ae_root_unit_status { struct ae_unit_status unit; int used,held,error; };
/* Root/process-owned single-threaded setup BEFORE REQUEST. MOVE the unused unit
 * owner on success only. Match an independently authorized protected selection,
 * exact launch-derived name and held/named starter ELF against AETHEXE1.
 * Copies the image selection/duplicates FDs and requires the original witnessed
 * v9 allocation/selection in a v11 catalogue. The independently selected Root
 * main-process role/image is pinned and durably enrolled before REQUEST. No
 * Ref/Subscribe/Start or new effect permission is created by this constructor.
 * Unknown enrollment acknowledgement preserves the original row/connection;
 * never substitute another origin/process/image after uncertainty.
 * Caller owes original unused allocation, volume/configuration/MAC admission,
 * mutation exclusion and trusted host buses; never adopt candidate self-claims. */
int ae_root_unit_new(struct ae_catalogue *,struct ae_unit **,
    const struct ae_unit_profile *independent_selection,const struct ae_root_unit_image *,
    const struct ae_root_unit_issuer_selection *,
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
