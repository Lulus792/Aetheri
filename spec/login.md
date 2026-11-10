# Login backend and conversation

Aetheri's post-login entry is [aetheri-session](session.md#entry-from-an-existing-local-login).
It consumes an existing local Wayland login. This document covers the next
boundary: the original greeter's conversation with a maintained PAM backend.
The C conversation model, bounded native stream adapter and synthetic wire tests
exist. Deployment admission, an installed authentication service and graphical
greeter do not yet exist.

## Backend direction and review

Use a separate maintained PAM backend with an original Aetheri greeter. The
current candidate is greetd, whose protocol permits an independent presentation
and whose worker performs PAM authentication/account/session setup. This keeps
password handling and privileged account switching out of our UI. An original
privileged PAM service would offer more implementation control but would also
make Aetheri responsible for that additional credential and session lifecycle.

[login-inputs.json](login-inputs.json) records the exact reviewed Fedora
greetd 0.10.3-6.fc44 binary/source RPMs, original source hashes, signer, licences
and pending admission. Both package signatures match the existing full Fedora 44
key. They were queried and unpacked as data; no package script or executable ran.
The binary's declared Rust dependency notice differs from the upstream source
lockfile; never use the latter as evidence of the packaged dependency versions.

The package notice lists bytes 1.11.1, the patched version for the reviewed
RUSTSEC-2026-0007, and Tokio 1.49.0, within the version range reported by
RUSTSEC-2026-0386. Reachability of its affected AsyncFd API has not been
established. A maintained rebuild/complete dependency review is required before
runtime admission. The separate pam-bindings advisory does not identify pam-sys
as the same crate. None of this is a general security clearance.

The backend remains **unadmitted**. No service, PAM profile, user, policy or binary
has been installed. The existing carrier authentication setup is unchanged.
Fedora's packaged PAM/keyring/namespace/profile and service scripts are inputs to
review, not an Aetheri provisioning recipe. SELinux policy and local/offline PAM
account policy still require their own integration and tests.

The source joins StartSession command elements and executes them through
`/bin/sh -c`. Our model exposes no caller command/environment arguments. It sends
only `/usr/libexec/aetheri/aetheri-session` and three fixed Aetheri/Wayland
environment labels. Future provisioning must disable `source_profile`, preserve
the independently admitted immutable executable and review the `/bin/sh` runtime
dependency. No arbitrary account name, password or UI label enters command text.

## Implemented conversation model

`src/login/protocol.h` supplies an opaque, single-owner C17 model. It shares the
existing strict scalar UTF-8 decoder and adds no JSON library, daemon, worker
thread, socket, authentication decision or filesystem operation. Creation owns
fixed bounded buffers; requests and replies allocate no additional memory.

| State | Meaning and permitted next action |
| --- | --- |
| IDLE | A fresh explicitly admitted connection or confirmed cancellation; accept one username |
| SENDING | One complete request is queued; transport may consume its original bytes once |
| WAITING | Local transmission finished; await one response, with no resend |
| PROMPT | Present the original visible/secret/info/error question; answer or cancel explicitly |
| AUTHENTICATED | Backend reported conversation success; request the fixed session or cancel |
| DENIED | Backend rejected create/answer; require acknowledged cancellation before another attempt |
| START_ACKNOWLEDGED | Backend accepted scheduling; this is not target exec or desktop readiness |
| UNKNOWN | Malformed/lost exchange or uncertain start/cancel; no commands or rearming |

Info/error messages require an explicit NULL acknowledgement. Visible/secret
prompts require a string, including a permitted empty string. Nothing guesses
that every question is a password or supplies answers automatically. Invalid or
oversized local input preserves the prior phase/prompt and cannot submit an old
accepted prefix. The caller retains responsibility for wiping its own input/UI
copies and rendering backend text as text rather than executable markup/control.

`ae_login_output` borrows the queued frame. `ae_login_sent` acknowledges only a
locally accepted prefix and immediately overwrites that prefix in owned memory.
It does not acknowledge authentication or start. A connection loss wipes the
model's transport/prompt buffers and fences the live exchange. A prior confirmed
start acknowledgement remains historical; a new connection cannot reconcile or
replay an uncertain start. Closing releases/wipes memory and never implies
cancellation, process stop, session drain or lease settlement. This is not
locked memory, guaranteed system-wide erasure or durable restart recovery.

## Wire and input budgets

The reviewed protocol uses a native-endian uint32 byte length followed by one
UTF-8 JSON object. This is a local same-architecture protocol; Aetheri targets
x86-64. Incoming headers and bodies may arrive in arbitrary fragments.

| Input | Limit |
| --- | --- |
| JSON frame body | 32 KiB; zero/oversized lengths reject before body admission |
| Username | 256 UTF-8 bytes, nonempty, no NUL/C0/DEL |
| Visible/secret response | 1,024 strict UTF-8 bytes, no NUL; empty differs from NULL |
| Decoded prompt/error text | 4,096 UTF-8 bytes |
| Outstanding operations | One; no pipelining or automatic retry |

The answer budget keeps worst-case escaping below the reviewed backend worker's
10 KiB internal datagram, including its envelope. The external frame budget
alone would not establish this compatibility. The parser admits exact known
response fields/types, any field order, JSON escapes and valid surrogate pairs.
It rejects duplicate decoded keys, unknown fields/types, invalid UTF-8/surrogates,
NUL, truncation on interruption and extra response bytes. Unexpected input during
another live phase also fences the conversation; an invalid API call without
received bytes leaves the state intact. Strict unknown-field rejection requires
an explicit compatibility review when the backend protocol changes.

## Build, evidence and remaining work

Enable `AETHERI_LOGIN_PROTOCOL=ON`, then build `aetheri_login_protocol` and
`test_login_protocol`. The library links only the existing `aetheri_text`.
Run `ctest --test-dir BUILD -R '^login[.](protocol|wire)$' --output-on-failure`
as the ordinary native account. The wire case explicitly skips UID0/non-Linux.
Its Python peer independently decodes the native requests and supplies fragmented
Unicode JSON responses over real pipes, with synthetic inputs only.

Release and ASan/UBSan/leak runs pass both cases. Controlled tests cover invalid
responses, complete local input rejection, frame/text limits, all shorter reply
prefixes, uncertainty, explicit cancellation, fixed command and owned-buffer
wiping. A premature-start mutant compiles and fails the authentication gate test.
These are protocol/model tests, not PAM, daemon, trusted greeter, hardware or login
acceptance. Read the [execution record](../brains/aetheri/journal/2026-11-10-login-protocol.md).

The separate [backend candidate](login-backend.md) now builds offline with pinned
reviewed inputs and passes upstream tests. It remains runtime-unadmitted. Next
establish the deployment's protected original daemon/configuration/greeter
admission around the native adapter below.
Then exercise real PAM rejection/cancellation and the greeter-to-user session
transition, build the original graphical greeter and complete service/account/
SELinux/system packaging. Public app lifecycle capabilities and the old runtime
quarantine remain unchanged. This work does not satisfy the first-version USB gate.

## Native connected-stream adapter

`src/login/connection.h` joins the conversation to an already connected local
stream. It does not find a daemon by environment/path/PID, connect, authenticate
the user or install a service. The caller must independently admit the original
daemon image/configuration, supervised invocation, policy and greeter context.
A mandatory compiled check retains that responsibility at each admission point;
the test callback is not a production verifier.

Construction compares the socket's kernel `SO_PEERPIDFD` with the independently
selected live daemon pin. It requires pidfs and `PIDFD_GET_INFO`, matching process
identity, original connection UID/GID and current real/effective/saved/filesystem
UID/GID. The two kernel pins must identify the same object; same UID or numeric
PID alone is insufficient. There is no numeric-PID reopening or weaker fallback.
Original daemon identity and admission are rechecked before requests, watches and
I/O, and after receiving bytes before applying a reply. These checks are bounded
observations, not locks against future exec, credential or supervisor changes.

This matters for the reviewed greetd implementation: the Root listener creates
`/run/greetd-PID.sock` and then changes its owner to the greeter account. Socket
ownership is not the server process's identity. `SO_PEERCRED` describes credentials
at connection/listener creation; it does not establish the current process or
authenticate every later writer holding a copied endpoint. Full deployment must
prevent unauthorized endpoint transfer/ptrace and admit the original service.

On successful construction the adapter consumes the caller's socket and owns
CLOEXEC descriptors and a fresh conversation. Failure preserves the caller's
socket and null output. The caller must provide exclusive stream ownership and
keep the admission context alive. Every active operation belongs to the creating
process/thread; a fork child may only release its local copy. Callback reentry
and callback-time close reject without freeing the live object.

Each pump attempts one nonblocking receive of at most 4,096 bytes and at most one
send. Receive precedes send so queued unsolicited/early input fences before a
new request leaves. Partial transmissions wipe only accepted prefixes; EAGAIN
and EINTR do not resend them. `MSG_NOSIGNAL` avoids process-wide SIGPIPE changes.
All ancillary input rejects, with every delivered rights/pidfd descriptor closed,
including a truncated control message. No frame or credential contents are logged.

The adapter returns borrowed socket/pidfd descriptors for event waits. It creates
no thread, timer, blocking wait or automatic retry. The UI owns deadlines and may
explicitly interrupt. Once the stream, original process or admission is lost,
commands remain fenced even if a check later succeeds. A prior start ACK remains
historical; EOF, close, interruption or peer exit never proves target execution,
session failure, descendant drain or cancellation. Closing releases local memory
and descriptors without issuing cancel, reconnect, process control or lease effects.

`AETHERI_LOGIN_PROTOCOL=ON` builds `aetheri_login_connection`. Run
`ctest --test-dir BUILD -R '^login[.](protocol|wire|connection)$' --output-on-failure`
as an ordinary native account. The connection case includes real kernel pins,
fragmented synthetic conversations, partial/would-block I/O, unsolicited input,
original peer exit with another endpoint still open, complete/truncated 253-FD
messages, owner-thread/fork restrictions and mandatory-check loss. Scoped test
faults are confined to the test translation unit. Release/memory builds and the
independent protocol peer pass; removing the original-pin comparison makes the
same-UID wrong-process test fail. See the
[execution record](../brains/aetheri/journal/2026-11-10-login-connection.md).
