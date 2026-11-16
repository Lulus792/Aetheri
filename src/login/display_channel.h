#ifndef AETHERI_LOGIN_DISPLAY_CHANNEL_H
#define AETHERI_LOGIN_DISPLAY_CHANNEL_H

/* One bounded descriptor offer on an inherited unnamed Unix seqpacket socket.
   The caller owns both descriptors. A send error requires retirement of this
   offer, and failure after receive must never cause adoption of a later packet. */
int ae_login_display_offer(int control, int stream);
/* Nonblocking. Only EAGAIN means that nothing has been received yet.
   Reject unexpected bytes, ancillary objects, or truncation and close all FDs
   received on error. On success *stream owns one CLOEXEC descriptor. */
int ae_login_display_take(int control, int *stream);
/* Validate a connected unnamed AF_UNIX socket of the requested type. */
int ae_login_display_channel_check(int fd, int type);

#endif
