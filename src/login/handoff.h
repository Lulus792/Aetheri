#ifndef AETHERI_LOGIN_HANDOFF_H
#define AETHERI_LOGIN_HANDOFF_H
#include "login/loaded.h"

struct ae_login_handoff;
/* Administrative one-time publication after current ELF and consumed config
   verification. Caller independently admits the Root-owned 0555/0755 directory,
   its absolute path, protected ancestors and mount topology, and exact file MAC
   label. Create an anonymous file, label it, make it0444, flush, link without
   replacement and flush the directory. Existing entries return EEXIST, never
   adoption. Any uncertain link/flush/close preserves the possible named file.
   Caller must retain that outcome and never automatically repeat or clean up.
   This ephemeral record is tied to one boot/service/process/source observation. */
int ae_login_handoff_publish(struct ae_login_loaded *,int directory_fd,
                             const char *absolute_directory,const char *leaf,const char *label);
/* Ordinary client reads an independently selected protected path and label.
   Retain original directory/file FDs and verify the record against the complete
   live loaded/source selection. Borrow loaded until handoff close. Output starts
   NULL and is unchanged on failure. No raw-record or caller-PID admission. */
int ae_login_handoff_open(struct ae_login_loaded *,int directory_fd,
                          const char *absolute_directory,const char *leaf,const char *label,
                          struct ae_login_handoff **out);
/* Suitable as one component of connection_source.check. Rechecks the named/
   retained file, original parent and live instance. First loss fences. Caller
   still admits the original greeter and effective policy and excludes privileged
   exec/source/namespace mutation after the administrative observation. */
int ae_login_handoff_check(void *);
int ae_login_handoff_close(struct ae_login_handoff **);
#endif
