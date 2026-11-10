#ifndef AETHERI_FILES_LAUNCH_H
#define AETHERI_FILES_LAUNCH_H
#include <stdbool.h>
#include <sys/types.h>
struct ae_files_child { pid_t pid;int pin,status;bool ended; };
/* Explicit local development profile, one direct Terminal child at a time.
   Fixed independent absolute program/font/cache inputs; literal leaf + directory/
   regular-file witness copied to 3/4, all other extra FDs closed. No shell text,
   authority admission or descendant drain. Preserve a live/unknown child slot. */
int ae_files_launch(struct ae_files_child *child,const char *terminal,const char *editor,
    const char *font,const char *cache,int directory,int witness,const char *leaf,bool diagnostics);
/* Additive local profile: optional independently selected absolute bold font for
   the Terminal view. NULL preserves the original command. No path discovery or
   shell expansion; the original directory/file handoff and child custody apply. */
int ae_files_launch_profile(struct ae_files_child *child,const char *terminal,const char *editor,
    const char *font,const char *bold_font,const char *cache,int directory,int witness,const char *leaf,bool diagnostics);
/* Reap only the original direct child. Missing pidfd does not permit another
   launch; waitpid still owns the unreaped direct child. Never signals a numeric
   PID. Close requires observed direct-child end, not app/holder/lease drain. */
int ae_files_child_observe(struct ae_files_child *child);
int ae_files_child_close(struct ae_files_child *child);
#endif
