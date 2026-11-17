#ifndef AETHERI_SETTINGS_SOUND_CHANNEL_H
#define AETHERI_SETTINGS_SOUND_CHANNEL_H
#include <stdbool.h>
#include <stdint.h>

struct ae_settings;
struct ae_settings_sound_channel {
    int fd;
    uint64_t request,pending;
    bool retired;
};

/* Validate a connected unnamed seqpacket endpoint created by the current
 * launcher parent, and mark it close-on-exec. Policy and launch selection
 * still admit the actual parent before an installed handoff. */
bool ae_settings_sound_channel_admit(int fd);
/* Initialize one lifetime with an already admitted endpoint. The caller
 * retains the FD and closes it after retirement. Use the owning UI thread. */
void ae_settings_sound_channel_init(struct ae_settings_sound_channel *,int fd);
/* Receive one snapshot or one matching result. Returns 1 after an update,
 * 0 when idle or negative errno on loss. Protocol loss fences the model. */
int ae_settings_sound_channel_receive(struct ae_settings_sound_channel *,struct ae_settings *);
/* Consume one explicit intent. 1 means the request was queued on the
 * socket, 0 means no intent or a locally rejected busy send. No replay. */
int ae_settings_sound_channel_send(struct ae_settings_sound_channel *,struct ae_settings *);
#endif
