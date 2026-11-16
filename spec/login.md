# Login backend and conversation

Aetheri's post-login entry is [aetheri-session](session.md#entry-from-an-existing-local-login).
It consumes an existing local Wayland login. This document covers the next
boundary: the original greeter's conversation with a maintained PAM backend.
The C conversation model, native stream and protected startup components exist.
Private PAM authentication now reaches the actual graphical desktop and completes
session shutdown. A candidate SELinux domain confines the finite startup helper.
A locked dedicated greeter account and internal PAM profile now have actual
carrier session evidence, including a split SELinux profile. Normal deployment
admission and the original graphical greeter remain work.

## Candidate login roles

The candidate policy assigns separate SELinux domains to the protocol backend,
its privileged PAM worker and the greeter. The finite startup publisher remains
in its own domain. PID1 starts the selected backend. The backend's self-exec
enters the worker domain, and the worker's child drops its Unix credentials
before the selected executable enters the greeter domain. Fedora's authlogin
policy requires explicit membership in `can_read_shadow_passwords`. Only the
worker has it, and it has read-only shadow access. Its audit netlink access
allows reporting PAM events. The backend and worker retain the TTY operations
needed by the pinned backend's VT handling. The greeter has no direct TTY
rights or account-changing capabilities.

The carrier supplement allows the backend's startup notice and the worker's
PAM log messages to reach the existing early-boot `kernel_t` datagram sockets
that PID1 and journald hold on this Fedora carrier. Package only the policy
selected for the normal deployment and independently verify its socket and
process labels, device bindings, writable runtime paths and unit settings.
The test log type supports the inherited `OpenFile` diagnostic descriptor.
The probe writes only synthetic access results and lifecycle markers.

The finite startup publisher can open the selected backend executable and
configuration for verification, connect through the dedicated login socket type,
and read the loaded policy for the compiled provider's digest check. It cannot
execute the backend, modify those sources or create, relabel or unlink the
socket. The general `var_run_t` pathname socket grant belongs to the older
administrative carrier supplement. The split deployment denies that grant.

The explicit `probe_login_roles` build target exercises synthetic file and
socket access. The read-only `tools/check_login_policy.py` checks selected
access decisions against an independently supplied loaded-policy digest. The
native test starts an actual internal greeter PAM session on seat0/VT8 with an
inert greeter. It observes the three labels and the greeter's exact UID, GID,
groups and empty capability sets. Stopping the original backend leads to the
original greeter and worker's normal exits, PAM close and the inactive session
scope. On the split carrier, a separate finite publisher now writes the
protected `AETHLGH1` record
while the original backend and internal greeter are live. The controller verifies
its boot, invocation and pidfd object before the original PAM shutdown. The
compiled carrier provider compares the loaded policy with its selected digest.
The confined greeter now consumes the record using the same ordered five
sources as the publisher. It verifies its own identity and ancestry, the
original live backend, and the published bytes before handing the socket to
the connection model. The native test observes `AE_LOGIN_IDLE` after those
checks. A second real publisher uses a different source order and the greeter
rejects its record with `ESTALE`. The test greeter waits for the original PAM
worker's shutdown signal after rejecting the record.
After the same admission step, the split carrier now verifies a real PAM
password conversation and account check with a synthetic credential. The
worker can start the distribution's `unix_chkpwd` only in its existing
`chkpwd_t` domain. It exchanges password input and results over the PAM
helper's pipes. The backend and greeter cannot execute the helper, and the
login roles cannot execute the password update program. The carrier supplement
labels a sealed descriptor for the synthetic test password. The greeter reads
it after the protected connection and PAM prompt checks and clears its copy
after submission. An empty answer fails, the selected password succeeds, and
the authentication test waits for acknowledged cancellation. The eventual
graphical host keeps this admission step ahead of input and authentication.
See the [native record](../brains/aetheri/journal/2026-11-16-login-roles.md).

### User-session handoff under the split policy

The explicit Start fixture consumes the sealed test input through the original
open file description. It sends the fixed session command after the controller
pins the authenticated worker and publishes a create-only test marker. A Start
acknowledgement records scheduling. The controller separately observes the
selected entry object, process credentials, SELinux domain and active logind
scope. After the session closes, the replacement greeter admits its own
connection and observes the already consumed input without starting PAM again.

The authenticated worker can exec the separately labeled session entry into
`aetheri_session_t`. The entry uses the ordinary UID and GID with zero effective,
permitted, inheritable and ambient capabilities and with NoNewPrivileges set.
The selected core program type executes in the same domain. The desktop entry
reads logind's session metadata and the PID1 cgroup path used by libsystemd. It
creates a private labeled runtime directory under the user's systemd runtime
directory. The candidate denies shadow reads, signals to the login worker and
changes to the published login handoff. The early-boot journal stream allowance
belongs to the disposable carrier supplement.

The split carrier reaches an active local Wayland user session and runs the
selected compositor executable. The compositor stops at udev monitor creation
under the candidate policy. The test records PAM close, inactive user scope and
the replacement greeter's normal shutdown. Device access and desktop readiness
remain open under this policy. See the
[handoff record](../brains/aetheri/journal/2026-11-16-login-roles.md#confined-user-session-handoff).

## Dedicated greeter account and supplementary groups

[The sysusers recipe](../data/sysusers.d/aetheri-login.conf) reserves
`aetheri-greeter` with a locked/expired normal login, no login shell and
`/var/empty` as its home. Its numeric UID/GID is selected and independently
validated at enrollment; 992 is the tested carrier allocation, not a portable
fixed product UID. Existing accounts must not be deleted/recreated, rebound or
accepted merely because their name matches. Validate the actual account, group
memberships, protected home and shell separately. The recipe is not automatically
installed by CMake.

`ae_login_greeter_expect` now includes a separately selected supplementary-group
set. It contains at most 16 strictly increasing, unique, nonzero GIDs; an empty
set is valid. The constructor copies it. Every identity check obtains the
calling greeter's actual groups, sorts the bounded local copy and requires exact
equality. Missing, extra or duplicated actual groups fail. Primary GID,
capabilities, original ancestry/cgroups, labels and executable remain separately
checked. Observed loss permanently fences the observer. Do not derive the
expected set from the candidate process or current NSS result.

[The internal PAM profile](../data/pam.d/aetheri-greeter) admits only the named
display-manager identity, denies password changes and opens a real
`class=greeter type=wayland desktop=aetheri` session. `pam_loginuid` precedes
`pam_systemd`. The name gate deliberately permits this internal service despite
the account's expired normal-login state; it does not unlock the account.
Independent protected account/backend admission is mandatory. Never use this
profile for authenticating human users, whose password/account checks belong to
the separate selected PAM service.

Actual tests use a unit-private PAM/account view, a fresh synthetic user secret
and the locked carrier greeter identity. They observe UID/GID/groups, zero
capabilities, audit loginuid, logind class/seat/VT/scope, runtime-directory
ownership and the original worker/child pins. A regular user desktop follows
successful authentication. Normal worker exit, PAM close and inactive scope are
checked independently. Model tests cover wrong/extra/missing/duplicate groups;
the actual admitted client also rejects an incorrect empty expectation.

The test-only `groups` mode now exercises a real extra membership. The original
backend's initgroups reads a unit-private group file adding unused fixture
GID 60000; the global group/account files remain unchanged. Its original greeter
actually has groups 992/60000 and zero capabilities. The independent 992-only
expectation rejects with EACCES and permanently fences that service observer.
A separate fresh observer must select the same still-held live kernel peer
before the literal 992/60000 positive control can succeed. It never rearms the
failed guard. No password, conversation begin, publication or user-session
Start is performed. Both Release and memory clients pass with normal original
greeter/worker exit, PAM close and inactive scope. The full normal memory login
also passes after the test-log FD refactor. These remain explicit administrative
carrier fixtures, not an installed greeter or normal deployment admission.

See [backend session closure](login-backend.md#original-session-worker-shutdown)
for shutdown and retained-failure rules. These account/session fixtures do not
establish normal account enrollment, production MAC/device policy, general
application admission or a finished graphical login screen.

## Candidate startup confinement

[login-startup.cil](../policy/login-startup.cil) defines the short-lived
publisher's candidate SELinux domain. It is a separately reviewed policy input,
not an automatically installed policy or a replacement for the deployment gate.

| Type | Responsibility |
| --- | --- |
| `aetheri_login_startup_t` | Finite Root startup process |
| `aetheri_login_startup_exec_t` | Protected executable selected by PID1 |
| `aetheri_login_runtime_t` | Root-owned publication directory |
| `aetheri_login_handoff_t` | Create-only, read-only observation record |

The helper can read its selected program/configuration and ancestors, inspect
the original peer and namespaces, query the host system bus and publish inside
the dedicated runtime directory. The five bounded capabilities remain those
required by the existing runner. `CAP_SYS_PTRACE` permits the kernel's privileged
read-mode inspection; the SELinux policy separately forbids process `ptrace`
against its selected backend domain. It grants no manager start/stop/reload or
enable/disable permission. PID1's unit `service:status` permission is required
in addition to bus messaging and the older system status permission.

No shadow-file or personal-home access, TCP/UDP socket creation, policy mutation,
handoff unlink or generic runtime-file write is granted. Compile-time
`neverallow` assertions cover the sensitive account/home/network/policy and
service-mutation/peer-ptrace boundaries. The existing native guards still select
the particular unit, invocation, process, objects and publication directory;
SELinux type permission alone does not select an authorized instance.

`aetheri_login_observed_backend` is deliberately empty in this candidate input.
A deployment must select its independently admitted dedicated backend type.
[login-startup-carrier-test.cil](../policy/login-startup-carrier-test.cil) fills
it with the existing administrative carrier domain **only for disposable tests**.
Never include that supplement in a normal deployment. Backend/worker/greeter
confinement, effective account/PAM/device policy and the normal compiled provider
remain separate admission requirements. The test provider's enforcing-only
callback does not establish them.

The explicit `probe_login_startup_policy` CMake target is excluded from default
builds, CTest and installation. A Root controller selects its ordinary-built
bytes, stages them under protected ownership and supplies synthetic targets.
The same probe bytes in an administrative control service can open those files
and create the sockets. With the same Root identity and capability bound in the
startup domain, six forbidden file opens and two socket creations fail, while
create-only publication succeeds. No private network, filesystem read-only view
or address-family filter explains those probe denials. No personal password,
network connection, policy write or process signal is attempted.

`AE_LOGIN_TEST_CONFINED_STARTUP` selects the dedicated handoff label in the
existing test greeter only. It does not change the normal greeter contract or
introduce a runtime label selector. Actual separate-unit Release and memory-client
tests use that label and the confined publisher, reach the visible desktop and
complete original entry/worker/PAM/scope shutdown. Both Root helpers and desktop
runtimes are Release; only the second ordinary greeter is memory-instrumented.
After all typed processes and attempts are terminal, the test module is removed
and the complete loaded policy is compared with its pre-test bytes. Failed
fixtures and their evidence remain preserved. See the
[native record](../brains/aetheri/journal/2026-11-16-login-startup-policy.md).

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

## Protected observation handoff

`login/handoff.h` transfers an administrative observation to an ordinary client
through an independently selected protected directory. The caller admits that
directory, its absolute path, ancestors and mount topology. The directory is
UID0/GID0, mode0755 or0555. The reader requires a regular UID0/GID0 file with
one hard link, mode0444, the exact selected raw SELinux label and no file
capabilities. Its mount must match the held directory. Both retained and named
objects and the original directory path are checked on use.

Publication captures the current executed ELF and consumed configuration on the
same loaded-service guard. It writes an anonymous file, sets its MAC label and
mode, flushes it, repeats the administrative observation and links the file
without replacement. Directory flush completes publication. An existing name
returns `EEXIST`. A failed link/flush/close can leave a named file, so the caller
retains that possibility for explicit reconciliation. The current adapter has
local observation and publication semantics. Installed startup coordination
and restart reconciliation remain part of the login integration.

The reader obtains the expected ELF, configuration digest and source selection
from its own admitted loaded guard. It reconstructs the complete observation
against the original live socket peer and compares all bytes. A protected file
from a different boot, invocation, process, image, source selection or manager
context fails. The handle holds its original FDs and fences on observed drift.
Replaced bytes or restored rights cannot reopen a fenced handle. It belongs
to the original process/thread, with callback-time use/close returning `EBUSY`.
Close releases its local FDs and memory. The loaded guard must outlive it.

`AETHLGH1` is a 296-byte canonical observation. All integers are unsigned
big-endian. The existing `AETHIMG1` image encoding retains its signed timestamp
convention.

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 8 | `AETHLGH1` |
| 8 | 16 | Boot ID |
| 24 | 16 | Original service invocation ID |
| 40 | 8 | Held peer pidfd device |
| 48 | 8 | Held peer pidfd inode |
| 56 | 8 | Kernel cgroup ID |
| 64 | 8 | Peer PID |
| 72 | 112 | `AETHIMG1` current executable observation |
| 184 | 32 | Consumed configuration SHA-256 |
| 216 | 32 | Ordered protected source observation SHA-256 |
| 248 | 8 | Original command monotonic start timestamp |
| 256 | 8 | Original command realtime start timestamp |
| 264 | 32 | Original manager/process context SHA-256 |

The context hash covers `AETHLGC1`, the bus authentication server ID, UID and GID
as u64, raw process label plus NUL, and the original unique PID1 bus name plus
NUL. The source hash covers `AETHLGS1` and the u64 source count. Each ordered
entry contributes NUL-terminated parent path, leaf and label, then its expected
digest and ordered u64 values for kind, mode, size, ancestor depth, file
mount ID, inode, device major/minor, UID/GID, mode, link count, size and birth,
modification and change times as seconds/nanoseconds. Each original ancestor
including `/` contributes u64 mount ID, inode, device major/minor, UID/GID,
mode, filesystem type, birth-present flag, birth seconds/nanoseconds and eight
filesystem-ID bytes. Signed values use their two's-complement u64 encoding.
The source count, path sizes and depth retain the existing bounded profile.

The protection of the selected file and parent supplies authenticity. The hash
values identify the captured context. The administrative observation remains
local to its boot and process lifetimes. The complete deployment must exclude
privileged executable/source/namespace mutation after observation and admit the
original greeter and effective PAM/SELinux policy. The native integration
currently exercises a short-lived administrative publisher and UID1000 clients
with the actual greetd and its inert test greeter. See the
[handoff record](../brains/aetheri/journal/2026-11-15-login-handoff.md).

## Original greeter process

`login/greeter.h` observes the calling greeter on its process leader's thread.
The caller supplies the independently selected ordinary account, exact raw
greeter and PAM-worker process labels, original socket-service observer and
protected greeter ELF source entry. The observer borrows both source/service
handles until close. It creates a self pidfd and opens only the immediate
parent reported by that held self's kernel `PIDFD_GET_INFO` observation. The
self relationship brackets that opening, and the retained live parent must
itself be a direct child of the original Root backend MainPID. No PID supplied
by a caller, environment or socket is reopened to manufacture this ancestry.

Each check repeats the actual self ELF, all real/effective/saved/filesystem
UID/GID values, exact process labels, live pins, original parent relation and
the original self/parent cgroup IDs. Self must have no effective, permitted or
inheritable capabilities. Backend and PAM worker must have UID/GID0. PAM can
move a session into its own scope, so the observer does not require the three
processes to share a cgroup. No new process/thread, signal or supervisor action
is added. Owner/thread/reentry rules and a permanent first-loss fence apply;
closing an observation only releases local FDs and memory.

This is the self-process part of admission. A deployment must separately admit
the dedicated account and supplementary groups, PAM/backend configuration,
effective SELinux policy, trusted namespace/proc views, loaded libraries and
protection against hostile exec/ptrace/injection. The ordinary observer does
not open the Root worker's executable, prove its PAM session class or establish
display/seat ownership. The reviewed backend and independently enforced policy
must give the observed ancestry its intended role. Tests use the carrier's
existing UID1000 account and administrative domain, not production greeter
confinement. Compose this check with the loaded configuration, protected
handoff and remaining policy/device checks before credential transport.

The explicit `test_login_greeter` target runs from the real pinned greetd
through its Root PAM worker. Its Root-created test profile is not a production
profile format. The test joins self checks and the protected handoff to the
native connection, exercises a real `pam_unix` secret prompt and acknowledged
cancellation, then submits an empty answer for the existing locked account.
Authentication is denied. With the original backend, a subsequent cancellation
error fences the client UNKNOWN; it cannot silently start another attempt.
The optional [worker integration](login-backend.md#configuring-worker-cancellation)
now confirms the original configuring worker's end before cancellation succeeds.
The test's `retry` mode exercises the acknowledged cancellation after denial
and two further explicit prompt/cancel attempts. An administrative observer
independently pins the actual user-authentication worker during the first secret
prompt and checks that it exited before the Cancel ACK. No C protocol fence was
weakened. The default fixture retains its old UNKNOWN check for the original
candidate. Successful password/session transitions and the graphical greeter
remain work.

The publisher and reader must use the same independently admitted source mount
topology. A private service namespace changes the source fingerprint, even when
the underlying files have the same bytes. The native fixture enters the held
original backend mount namespace for administrative publication, checks it
against the actual greeter namespace and retains the original peer pin around
that operation. This is disposable test coordination, not an installed startup
coordinator or automatic namespace-selection API. Likewise greetd starts the
greeter with its PAM environment; the service's loader environment is not an
implicit greeter environment. The test command explicitly selects its protected
crypto library after the account change. Memory-only test DSOs are selected only
for the ordinary test greeter, never the privileged backend/publisher.

## Successful authentication fixture

The explicit C test greeter's `authenticate` mode exercises real `pam_unix`
authentication and account checks, followed by confirmed cancellation. It first
submits an empty answer and requires rejection, then supplies a fresh synthetic
password and requires AUTHENTICATED. The account keeps its actual NSS UID/GID;
its host password is unchanged. This response precedes user PAM open_session,
logind registration and target Start. None of those later transitions is implied.

The administrative fixture generates a random test password and a yescrypt
setting with the carrier's existing libxcrypt. It supplies the plaintext only
through a Root-owned sealed anonymous memory FD opened read-only by PID1 and
inherited as fd3. The test checks regular-file ownership/mode, zero links, size,
access flags and seals, closes the FD and wipes its local answer after queuing.
No password enters argv, environment, profile JSON or test output. This is an
explicit test input provider, not a production greeter credential-input API.

A Root-only holder on guest `/run` tmpfs contains a private `/etc` projection:
selected unchanged public NSS/loader files, the original SELinux configuration,
the fixture PAM directory and a mode000 test Shadow file. Only that unit sees
the projection. Auth/account use pam_unix; session/password remain deny-only.
The controller checks the actual backend's bound Shadow and inherited secret
FD against its independently created original objects. Global Shadow bytes are
compared before/after, and the secret/hash are checked absent from the journal.
The plaintext FD closes after exact unit retirement. The original named test
hash is removed only after held/named identity and terminal-unit checks. Failed
preparations remain separate protected evidence. This supplies no general
allocation/removal protocol, volume authentication or system-wide erasure/swap
guarantee. No personal password or new library/package was used.

Release and ASan/UBSan/explicit-LSan clients pass this actual backend flow.
The [session entry](session.md#entry-from-an-existing-local-login) separately
rejects inherited extra descriptors before creating its own channels. Complete
PAM/logind/VT transitions, protected startup, dedicated greeter/account/MAC policy,
the selected runtime closure and graphical login remain required before normal
user authentication. See the [native record](../brains/aetheri/journal/2026-11-15-login-authentication.md).

## Authenticated desktop fixture

The explicit C greeter's `start` mode extends the synthetic-authentication test
to the fixed session command. It consumes the original inherited credential
OFD once, waits for the controller's protected start marker, submits Start,
observes its scheduling acknowledgement and exits normally. A replacement
fixture greeter sees EOF and waits without submitting another authentication.
This one-use input is test coordination, not a production credential provider
or durable replay defence. The immutable command/environment and UNKNOWN fences
are unchanged. A separate Root-created append-mode test FD supplies
greeter diagnostics after the backend connects standard streams to the VT;
it is not a trusted audit-log API.

Only the selected unit sees the private `/etc` projection and a read-only
`/usr/libexec` projection containing `/usr/libexec/aetheri/aetheri-session`.
The global executable path remains absent. Its private user PAM service uses
actual pam_unix authentication/account checks, pam_systemd, pam_unix session
open/close and a final pam_env entry with fixed protected runtime paths. The
runtime selects its library, EGL vendor, DRI driver and libinput data paths;
these data inputs are needed in addition to its executable/library files. No
user environment file is read. Password changes stay denied; the greeter PAM
service remains an explicit permit-only fixture, without greeter logind policy.

VT8 is independently checked free before dispatch. The disposable unit retains
the carrier devices and adds only CAP_SYS_TTY_CONFIG to its prior bounded Root
capabilities for the backend's actual VT handling. Enforcing SELinux stays on;
no carrier device mode or existing SELinux policy/label is changed. The existing
administrative service domain and UID1000 test account are not production policy.

The controller retains the original user PAM worker's pidfd before Start and
separately observes the actual executed entry, ordinary credentials, absent
capabilities, active local Wayland user session, original seat/VT/scope and
selected compositor/desktop images. Actual first-frame diagnostics and a viewed
QMP screenshot establish the tested display. The scheduling ACK alone proves
none of these. After visual observation, the held entry pin receives SIGTERM;
its own ordered shutdown precedes original worker end, PAM close and inactive
session scope. Only then is the original backend stopped and tty1 restored.

Release and ASan/UBSan/explicit-LSan greeters pass this complete private flow.
Both use the same independently hashed Release session entry and runtime;
no Root process loads user-writable sanitizer libraries. See the
[native record](../brains/aetheri/journal/2026-11-15-login-desktop.md). The startup
publisher/controller is still administrative test tooling. Dedicated greeter
account/UI/confinement, complete effective deployment/device policy, installed
startup coordination, normal app admission, lock/resume and the full system
runtime closure remain open. This does not satisfy the first-version USB gate.

## Native startup coordination

`login/startup.h` composes the existing service, protected-source, loaded-image/
configuration and handoff adapters into one finite native startup context.
It adds no daemon, thread, new library family or authentication request. The
caller independently selects the connected socket, Root system service, exact
sources, publication directory and mandatory compiled deployment-policy check.
The supplied socket is borrowed during construction without protocol I/O.

Creation requires a fresh single-threaded Root process with equal zero real,
effective and saved UID/GID values. It captures the actual socket service, asks
the original peer pidfd for `PIDFD_GET_MNT_NAMESPACE`, verifies nsfs/type and
retains that namespace FD. It enters only that selected mount namespace with
`setns(..., CLONE_NEWNS)`. It never accepts a PID, reopens a supplied PID or
selects `/proc/PEER/ns/mnt` by pathname. Source and loaded guards, and the held
publication directory, are then constructed in this view. Both the peer's
current namespace and `/proc/self/ns/mnt` must still match the retained original
namespace object before success and on later checks.

Namespace entry is a process effect even if a subsequent check fails. After
any constructor failure the startup caller must exit. Closing a successful
context never restores an earlier namespace. There is no fallback, reconnect,
automatic reconstruction or namespace selection through a configuration path.
Trusted host proc/PID/cgroup/system-bus views and exclusion of privileged
namespace/exec/source manipulation remain prerequisites. The caller also owns
the required namespace/proc permissions and bounded system-bus call timeout.

| Function | Result and lifetime |
| --- | --- |
| `ae_login_startup_create` | Own the original service/source/loaded/directory/namespace observations; leave an initially null output unchanged on failure, then caller exits |
| `ae_login_startup_check` | Recheck policy, original namespaces/directory, consumed configuration and current backend ELF; only an initially absent original config notice is pending |
| `ae_login_startup_publish` | Consume the one publication attempt, repeat readiness, perform create-only handoff publication and recheck namespace/policy afterward |
| `ae_login_startup_close` | Release local observations; neither restore a namespace nor remove a possible record or stop the backend |

An external gate's EAGAIN is a loss, not permission to wait as though only the
config notice were absent. Observed losses latch permanently. After a publication
attempt, any failure preserves possible named output and forbids another attempt,
including failure before a file effect. A successful attempt cannot be repeated
either. Thread/fork/reentry rules retain the existing creating-context boundary.
No raw record, matching filename or new context acknowledges an old outcome.

The mandatory external gate must admit the actual deployment: account/PAM/MAC/
device policy, trusted views, libraries/runtime and the selected destination with
protected ancestors. The local directory check supplies original Root-owned
mode0555/0755 identity, not that complete admission. The gate is called before
and after namespace entry and around readiness/publication. It must remain valid
for the context's lifetime and cannot change process/thread/namespace ownership.

`test_login_startup` provides controlled service/policy adapters with actual nsfs
FDs, threads and fork; it is an ordinary-user CTest. The explicit administrative
`test_login_loaded ... startup ...` fixture exercises the real kernel peer,
namespace entry and same-view publication with the actual backend and greeter.
Its test-only policy callback is not deployment admission. Release and memory
greeters consume the resulting record and complete the authenticated desktop
workflow; the privileged publisher remains the selected Release ELF. See the
[native record](../brains/aetheri/journal/2026-11-15-login-startup-coordinator.md).

An installed finite startup executable, selected deployment metadata, service
ordering and effective dedicated greeter/account/PAM/SELinux/device policy remain
work. The existing backend observer requires an active original MainPID with no
ControlPID/job; adding an unchecked ExecStartPost path to that unit does not meet
this contract. This library does not install/enable a service or promote the
carrier's administrative test profile to a product configuration.

## Finite startup executable

`aetheri-login-startup` now supplies the finite executable around the coordinator.
Build it explicitly with AETHERI_LOGIN_STARTUP, AETHERI_LOGIN_PROTOCOL and
AETHERI_LINUX_IPC enabled, and AETHERI_LOGIN_DEPLOYMENT_SOURCE set to an absolute
C source implementing `ae_login_deployment_select` from `login/runner.h`.
There is no default provider or runtime profile/plugin loader. The selected
provider supplies immutable service/source/destination expectations and the
mandatory effective deployment-policy gate. Compiling a carrier fixture provider
does not admit it for normal credentials. Packaging must independently verify
the provider, resulting ELF, runtime and effective policy.

The executable accepts no arguments. It requires open stdio and closes extra
inherited descriptors before invoking the provider. Failed cleanup ends startup.
The runner requires Root real/effective/saved user and group IDs and main-thread
entry; the coordinator also checks the actual single-threaded setup. It opens
the literal host system-bus socket, without using DBUS_SYSTEM_BUS_ADDRESS, then
captures its own actual bus credentials and original service context. Its
compiled startup unit/label must match, and its retained self pin must be that
unit's active MainPID with no ControlPID or pending job. An initial activation
wait rechecks the original self context; it cannot select a replacement.

Before waiting for the backend socket, the runner freezes the original manager
unique name, unit path, MainPID and nonzero invocation. The numeric PID constructs
only the reviewed socket pathname, never a process pin or admission result.
While waiting for initial socket appearance it rechecks that selection and its
own context. It makes one nonblocking connection, then composes the actual kernel
socket-service observer with the frozen manager/path/PID/invocation comparison
before namespace entry. No stream protocol data or credential request is sent.
There is no reconnect after a connection attempt. The existing coordinator then
owns same-view source/config/image checks and the single publication attempt.

One monotonic budget of100ms..15s bounds initial readiness waits; each synchronous
system-bus call has a separate one-second bound. Kernel file operations are not
preempted by this budget. The unit template also bounds process lifetime. Every
return from the runner requires immediate caller exit: its mount namespace may
have changed. Cleanup never restores a view, removes possible output or controls
another service. Local journal diagnostics distinguish selection, completion and
the failing phase. They are not readiness or lifecycle capabilities.

The [systemd template](../data/systemd/aetheri-login-startup.service.in) uses a
separate Type=exec service ordered after the backend. The greeter still waits for
and verifies the protected observation; an active startup unit alone is not
publication readiness. The helper exits after publication and does not remain
as a background process. Restart stays disabled. The template is not installed,
enabled or assigned an implicit MAC role by CMake; its context placeholder must
be selected together with the admitted deployment.

The tested capability set has distinct purposes: SYS_ADMIN/SYS_CHROOT for the
selected mount-namespace entry, SYS_PTRACE for kernel-gated inspection of the
privileged peer, DAC_READ_SEARCH for protected reads and AT_EMPTY_PATH linking,
and DAC_OVERRIDE for connecting to the greeter-owned pathname socket without
changing its modes/ownership. These are substantial Root privileges. Effective
dedicated MAC/device/account/PAM policy remains mandatory; the administrative
carrier domain and enforcing-only fixture gate are not that policy. The helper
does not gain the backend's setuid, setgid, kill or VT capabilities.

The final ordinary Release/memory profiles each pass eight selected CTests,
including frozen-selection/deadline/preflight and actual fork/descriptor-entry
tests. An invocation-comparison omission is detected. Two actual separate-unit
flows with compiled carrier providers reach the visible desktop and original
PAM/session shutdown. Both privileged helpers are Release binaries; only the
second greeter is memory-instrumented. Native execution outside the selected
startup supervisor is rejected without changing its runtime tree. See the
[native record](../brains/aetheri/journal/2026-11-15-login-runner.md). Normal
deployment inputs, effective confinement, a dedicated greeter account/UI and
full installed-system packaging remain open. No USB release is implied.
