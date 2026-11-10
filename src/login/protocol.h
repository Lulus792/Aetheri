#ifndef AETHERI_LOGIN_PROTOCOL_H
#define AETHERI_LOGIN_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

#define AE_LOGIN_WIRE_MAX 32768u
#define AE_LOGIN_TEXT_MAX 4096u
#define AE_LOGIN_RESPONSE_MAX 1024u
struct ae_login;
enum ae_login_phase {
    AE_LOGIN_IDLE,AE_LOGIN_SENDING,AE_LOGIN_WAITING,AE_LOGIN_PROMPT,
    AE_LOGIN_AUTHENTICATED,AE_LOGIN_DENIED,AE_LOGIN_START_ACKNOWLEDGED,AE_LOGIN_UNKNOWN
};
enum ae_login_prompt {AE_LOGIN_NO_PROMPT,AE_LOGIN_VISIBLE,AE_LOGIN_SECRET,AE_LOGIN_INFO,AE_LOGIN_ERROR};
enum ae_login_operation {AE_LOGIN_NONE,AE_LOGIN_CREATE,AE_LOGIN_ANSWER,AE_LOGIN_CANCEL,AE_LOGIN_START};
struct ae_login_status {
    enum ae_login_phase phase;
    enum ae_login_prompt prompt;
    enum ae_login_operation operation;
    const char *text;
    size_t text_size;
    int authentication_error;
};
/* Single-owner toolkit/transport-free greetd 0.10 conversation model. No sockets,
   authentication, account/session authority or process effects. The caller owes
   independent original backend/connection/greeter admission before transport.
   *out starts NULL. No allocation after creation; close wipes owned buffers.
   Source buffers/borrowed output/status text must not overlap this object. */
int ae_login_create(struct ae_login **out);
void ae_login_close(struct ae_login **);
/* Commands validate complete input before changing phase/prompt. No automatic
   answers/retries. Username <=256 UTF-8 bytes without controls; response <=1024,
   strict scalar UTF-8 without NUL. Empty secret/visible responses are distinct
   from NULL acknowledgement of an Info/Error prompt. The response budget also
   keeps worst-case JSON escaping below the backend worker's 10 KiB datagram.
   Caller wipes its own copy. */
int ae_login_begin(struct ae_login *,const char *username,size_t size);
int ae_login_answer(struct ae_login *,const char *response,size_t size);
int ae_login_cancel(struct ae_login *);
/* Fixed literal session command/environment only, never caller command text.
   Success response acknowledges scheduling, not exec, readiness or app admission. */
int ae_login_start(struct ae_login *);
int ae_login_status(const struct ae_login *,struct ae_login_status *out);
/* Borrow queued bytes until sent/interrupted/close. sent acknowledges only local
   transport acceptance and immediately wipes that prefix; it is not backend ACK.
   Never retain borrowed secret bytes or log frame/input contents. */
int ae_login_output(const struct ae_login *,const uint8_t **bytes,size_t *size);
int ae_login_sent(struct ae_login *,size_t size);
/* Incremental native-endian u32 framing, bounded strict response JSON. Returns
   0 incomplete, 1 one complete response, negative errno on rejection. Malformed,
   oversized, trailing/unsolicited responses fence UNKNOWN and wipe transport/text.
   Invalid API state/arguments preserve the previous state. One request at a time. */
int ae_login_receive(struct ae_login *,const uint8_t *bytes,size_t size);
/* Lost transport/observation cannot rearm or imply cancel/launch failure. A prior
   START_ACKNOWLEDGED remains historical; it never proves the session executed.
   No reconnect, automatic cancellation, rollback, process stop or lease release. */
void ae_login_interrupted(struct ae_login *);
#endif
