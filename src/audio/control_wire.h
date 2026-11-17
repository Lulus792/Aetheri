#ifndef AETHERI_AUDIO_CONTROL_WIRE_H
#define AETHERI_AUDIO_CONTROL_WIRE_H
#include "audio/types.h"

enum ae_audio_message_kind {
    AE_AUDIO_MESSAGE_SNAPSHOT=1, AE_AUDIO_MESSAGE_COMMAND=2, AE_AUDIO_MESSAGE_RESULT=3
};
struct ae_audio_command {
    uint64_t revision;
    struct ae_audio_output_ref target;
    enum ae_audio_command_kind kind;
    double volume;
    bool mute;
};
struct ae_audio_message {
    enum ae_audio_message_kind kind;
    uint64_t request; /* Zero for snapshots, otherwise unique on this connection. */
    union {
        struct ae_audio_session_snapshot snapshot;
        struct ae_audio_command command;
        enum ae_audio_submit result;
    };
};

/* Check an inherited, unnamed endpoint created by the current launcher
 * parent, and mark it close-on-exec. The deployment admits the launcher. */
bool ae_audio_wire_admit_parent(int fd);

/* Version 1 on a connected, unnamed AF_UNIX SOCK_SEQPACKET pair selected by
 * the session owner. The caller owns the descriptors and admits each peer.
 * Only -EAGAIN is a would-block result. Receive preserves the output on
 * error. Protocol failure retires the channel at the caller. */
int ae_audio_wire_send(int fd,const struct ae_audio_message *);
int ae_audio_wire_receive(int fd,struct ae_audio_message *);

struct ae_audio_wire_requests {
    uint64_t last;
    bool retired;
};
typedef enum ae_audio_submit (*ae_audio_wire_submit)(void *,const struct ae_audio_command *);
/* Handle one intent from an independently admitted Settings child. The
 * callback checks the actual audio authority and applies at most one effect.
 * Sequence numbers are strictly increasing. A lost result retires this
 * channel even if the effect might be applied. Publish any resulting
 * snapshots only after the result has been sent. 0 means idle, 1 handled. */
int ae_audio_wire_dispatch(int fd,struct ae_audio_wire_requests *,
                           ae_audio_wire_submit,void *data);
#endif
