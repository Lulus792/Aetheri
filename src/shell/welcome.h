#ifndef AETHERI_SHELL_WELCOME_H
#define AETHERI_SHELL_WELCOME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { AE_WELCOME_FIELD=65,AE_WELCOME_BYTES=4096 };
/* A one-time kernel snapshot for display, never identity/admission evidence.
   Unknown fields stay explicit; no environment/NSS/network lookup is used. */
struct ae_shell_welcome {
    char system[AE_WELCOME_FIELD],release[AE_WELCOME_FIELD],machine[AE_WELCOME_FIELD],host[AE_WELCOME_FIELD];
    uint64_t uid,memory_bytes,uptime_seconds,cpus;
    bool kernel_known,memory_known,uptime_known,cpus_known;
};
void ae_shell_welcome_capture(struct ae_shell_welcome *);
/* Bounded ASCII content plus authored SGR color/reset escapes. Wraps fields at
   cols-1, uses a compact mark below 26 columns and side-by-side at >=72.
   Failure leaves output untouched and sets size=0. No writes to the terminal. */
bool ae_shell_welcome_format(const struct ae_shell_welcome *,unsigned cols,char *output,size_t capacity,size_t *size);
#endif
