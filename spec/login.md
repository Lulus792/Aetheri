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

## Original system-service observation

`src/login/service.h` implements the service-context portion of the connection's
admission callback. It observes an already connected kernel Unix peer through a
trusted host system bus, proc/cgroup view and enforcing SELinux. It neither opens
the bus/socket from an environment address nor fabricates a D-Bus `ae_peer` from
socket credentials. The selected unit, UID/GID and two exact labels must come from
an independently admitted deployment selection.

The socket label and current raw process label are separate expectations. They
can differ: the administrative carrier fixture has a socket MLS range ending in
`s0-s0:c0.c1023`, while its current process label ends in `s0`. Both must match
their own full expected value. Matching only a type, UID or label prefix is not
supported. This fixture's administrative domain is not a proposed greeter policy.

Construction captures the original `SO_PEERPIDFD`, connection credentials and
socket label. Two complete observations bind the held process to current kernel
credentials/label/cgroup, the selected active system service, original nonzero
invocation, actual MainPID, absent ControlPID and absent job. The named cgroup
directory's inode must agree with the kernel process info and libsystemd path.
The trusted bus ID/boot and an unaugmented bus-authenticated PID1 process pin are
retained. No process handle is reconstructed from a supplied numeric PID.

Later checks repeat the observations against that original selection and latch
the first loss. The service handle exposes a borrowed original peer pin for
connection construction, owns its PID1/peer descriptors and bus reference, and
rejects another process/thread or callback reentry. Closing frees only local
observations; it never stops a service, releases a lease or proves session drain.
The synchronous read-only bus calls use the caller's configured timeout; there
are no subscriptions, automatic retries or new background workers in this module.

Build `aetheri_login_service` with both `AETHERI_LOGIN_PROTOCOL` and
`AETHERI_LINUX_IPC`. Its explicit `test_login_service` program is an inert
administrative fixture, never installed or registered as a default CTest. The
ordinary client passes against a real Root MainPID, rejects incorrect unit,
UID/GID and either label, and keeps terminal observations fenced. A Root child
with the same UID/domain/unit rejects as non-MainPID. Removing just that check
causes the targeted test failure. The memory-instrumented ordinary client also
passes against the uninstrumented Root fixture; Root executes no user-SDK RPATH.

This observer does **not** verify the executable currently running, immutable
configuration/unit sources, loaded-unit graph, deployment policy or the original
greeter/account/session context. Those checks must be composed before normal
login use. The inert fixture alone may use the service check as a connection
callback; it sends no username, credential or authentication request. See the
[execution record](../brains/aetheri/journal/2026-11-10-login-service.md).
## Protected deployment source observations

`login/sources.h` supplies the separate read-only file portion of login admission.
It does not decide which deployment is trusted. Its caller must independently
select each canonical parent/literal leaf, exact file label, kind, mode, size and
SHA-256. Do not derive that selection from a candidate's own manifest/header and
then call the candidate trusted. The existing maintained crypto/image verifier
is reused; this adds no language, crypto provider or resident process.

The bounded selection has at most eight entries. Parent paths are at most 1,023
bytes/depth16, leaves at most255 bytes and raw labels at most255 bytes. Absolute
canonical paths only: no empty components, trailing slash, `.` or `..`. Every
ancestor, including `/`, is observed through held directory FDs, must be UID0/
GID0, owner-searchable, without special mode bits or group/other write access.
Unique mount IDs and original directory objects are retained as observations;
symlinks/magic links are rejected. Ancestor mount boundaries may be selected
explicitly (for example a separate `/usr`); crossing a mount at the literal file
is forbidden, including same-filesystem bind mounts.

Files must be regular, UID0/GID0, exactly one hard link, with the selected raw
SELinux file label, no file capabilities and exact size/mode. Data is nonempty,
at most64KiB, mode0444 or0644; ELF is nonempty, at most16MiB, mode0555. ELF also
passes the existing x86-64/interpreter/segment/stack/SHA-256 verifier. Required
birth/mtime/ctime, device/inode and non-recycled mount observations bind the
original file. Duplicate underlying files cannot impersonate multiple sources.
Construction checks the complete selection again before publishing the handle.

Later checks repeat ancestor, held/named file, metadata, capability and exact
label observations. Content hashes are checked at construction; they are not
recomputed on every input pump. This requires independently protected immutable
deployment backing and exclusion of privileged filesystem/mount manipulation.
It is not an atomic multi-file snapshot or historical mutation detector. First
observed loss permanently fences that handle even if an original path/object is
restored. The original process/thread owns it; fork copies may only close locally.
Close frees local FDs/memory without changing files, services or session state.

`ae_login_sources_image` copies a selected captured ELF observation after checking
the entire selection. It exports no FD. File verification does not prove which
ELF a Root peer currently executes, which configuration it actually loaded,
loaded DSOs/PAM modules, active systemd unit/policy, or original greeter identity.
Those remain separate required admission checks alongside `login/service.h` and
before credential transport. No source checker is installed as a sufficient
`connection_source.check` callback. Root-owned test data/configuration is not a
production login deployment or PAM configuration.

The opt-in `aetheri_login_sources` target requires `AETHERI_LOGIN_PROTOCOL` and
`AETHERI_LINUX_IPC`. `test_login_sources` is an explicit ordinary-user integration
fixture, never installed and not an automatic privileged CTest. It takes an
independently prepared protected directory, expected backend SHA-256/size, an
ordinary-user control directory and `normal`, `reject` or `change` test mode.
The test backend file is observed as data, never executed. See the
[native execution record](../brains/aetheri/journal/2026-11-10-login-sources.md)
for concrete inputs, tests, counterexample and remaining admission work.

## Loaded login unit

`login/loaded.h` joins the retained socket service to the protected fragment,
backend ELF, backend configuration and ordered drop-ins. The caller selects
distinct entries in the existing protected source set. Both underlying guards
must outlive the loaded-unit handle. The service observer binds the original
PID1 bus connection's unique name and uses it for all unit reads. The loaded
properties are read inside that observer, with the original peer, invocation,
cgroup and PID1 verified around the read. The source set is checked again after
the call.

The selected non-transient unit must be loaded with the exact primary fragment,
an empty `SourcePath`, the exact ordered `DropInPaths`, and `NeedDaemonReload`
false. It must use `Type=exec`. The single `ExecStartEx` record must contain the
protected executable and exactly `PROGRAM --config CONFIG`. The checker admits
no command flags or condition, pre-start, post-start, reload or stop commands.
It verifies the command record's PID and original real and monotonic start
timestamps. The backend and configuration arguments use a restricted literal
path alphabet to exclude systemd variable or specifier syntax. A changed or
unavailable observation fences the loaded handle. Close releases local memory.

These reads describe the properties that PID1 currently holds. The complete
login path also needs evidence of the current backend executable, the settings
greetd actually consumed, effective policy and the original greeter. The current
carrier's ordinary client gets `EACCES` opening the fixture Root process's
`/proc/PID/exe`. Linux applies a ptrace access check to that path. The fixture
controller independently identified the running executable as Root before
retirement. See the [execution record](../brains/aetheri/journal/2026-11-15-login-loaded.md).

## Running backend executable

`ae_login_loaded_match_image` serves a trusted administrative observer with
UID/GID 0 and existing host `/proc` access. It takes an existing loaded-unit
handle. The expected ELF comes from the same source entry selected as `PROGRAM`.
The current executable is opened in the observer's trusted host PID namespace
while the service holds the original kernel socket peer pin. No numeric PID
from a caller enters the check. The kernel PID, credentials, cgroup, label and
peer liveness are checked around repeated references to the process executable.
The ELF verifier checks ownership, mode, capabilities, format and SHA-256.
Object identity and metadata also have to agree, including for byte-identical
files. The service and source checks complete before returning the observation.

Only administrative callers can request this operation. The ordinary greeter
will need a way to consume a protected observation tied to its original backend
instance. The current result is local to the administrative process and carries
no authorization for a future process. A same-PID image change after observation,
the configuration read by greetd, effective policy and original greeter identity
remain separate admission concerns. The existing public bus-peer image check
continues to reject UID/GID 0. See the
[native record](../brains/aetheri/journal/2026-11-15-login-current-image.md).

## Configuration read by the backend

`ae_login_loaded_check_config` reads the [greetd startup notice](login-backend.md#optional-startup-notice)
from the original PID1 connection and unit object. It compares the complete
`StatusText` with the original invocation's lowercase hex ID and the digest of
the selected protected configuration. `NotifyAccess` must be `main` around
the read. The deployment must hold the reviewed sender policy throughout the
invocation and independently trust the current executable and its environment.
The source and process checks complete before a result is accepted.

Until the first match, an empty status returns `-EAGAIN`. The caller owns the
startup deadline and can repeat the read on the same handle. Once requested,
plain loaded and executable checks also wait for the notice. A wrong nonempty
status, wrong sender policy, or loss after a match fences the handle. A later
matching status cannot reopen it.

The native tests include the actual pinned greetd with an inert unprivileged
test greeter and unit-private PAM files. The ordinary reader accepts its
configuration and the administrative observer verifies its ELF while the same
service is live. A production deployment still needs the protected handoff to
the original greeter, effective PAM/SELinux policy and user-session admission.
See the [native record](../brains/aetheri/journal/2026-11-15-login-config-reader.md).
