#ifndef AETHERI_AUTHORITY_EXECUTION_PROFILE_H
#define AETHERI_AUTHORITY_EXECUTION_PROFILE_H
#include "catalogue/execution.h"
#include "catalogue/data.h"
#include "core/fixture.h"
#include "linux/image.h"

/* Protected fixed P0 execution selection only. No caller-supplied registry paths,
 * action, categories, account UID or fallback to the original no-exec fixture.
 * These read-only checks do not admit a launch, open gates, prepare mount views,
 * authenticate a volume, register an app or prove future image immutability.
 * Caller owes current catalogue/User fences and enforcing policy excluding
 * mutation/relabel/rename by the target. Output unchanged on every error. */
int ae_execution_profile_load(struct ae_execution_profile *,uint8_t digest[32]);
/* Return a held verified starter ELF; output FD must initially be negative. */
int ae_execution_starter_open(const struct ae_execution_profile *,int *,struct ae_image *);
/* Root or trusted User read-side private allocation check, independent of code
 * store access. Publisher comes from the authenticated selected install, never
 * caller package text. Reopens protected configuration and exact witnessed owner/
 * allocation/payload with full UID/GID/mode/MAC checks. Output FD starts negative;
 * both outputs unchanged on failure. This is neither catalogue admission nor a
 * promise about later mutation; caller still serializes owner operations. */
int ae_execution_data_open(const struct ae_execution_profile *,const uint8_t publisher[16],
    int *payload,struct ae_data_enrollment *);
struct ae_execution_sources {
    int code_fd,data_fd,image_fd;
    struct ae_fixture_command command;
    struct ae_image image;
};
#define AE_EXECUTION_SOURCES_INIT { .code_fd=-1,.data_fd=-1,.image_fd=-1 }
/* System-owner code admission without reading User private data. Same source
 * inventory/manifest/image checks; success returns code/image and publisher,
 * leaving data_fd negative. Outputs unchanged on failure. */
int ae_execution_code_open(const struct ae_execution_profile *,struct ae_execution_sources *,uint8_t publisher[16]);
/* Requires an empty initialized output. Owns exact held code/data/image FDs on
 * success. Image FD is under the admitted code root, not an independent path.
 * The source data is not yet a noexec mount: clone/attach/check views before
 * passing to ae_execute_target. Closing FDs cannot release a persistent lease. */
int ae_execution_sources_open(const struct ae_execution_profile *,struct ae_execution_sources *);
void ae_execution_sources_clear(struct ae_execution_sources *);
#endif
