# P0 runtime contracts

26 October 2026. Byte formats below are implemented by the development reference
[runtime_contracts.py](../tools/runtime_contracts.py) and checked against
[frozen vectors](vectors/runtime-v1.json). They add no SQL tables or D-Bus methods.
The native C17 owner codec, bounded Linux file reader and bus-credential capture
are implemented and tested in the guest. App/run mapping, volume registration, app policy,
launch, grants and operation recovery remain integration work.

P0 handles trusted synthetic fixtures only. This is not a general package format,
signed public installation protocol, production consent UI or permission engine.
Keep the [accepted P0 scope](../brains/aetheri/knowledge/first-linux-prototype.md)
and [SQL/XML guide](README.md) as the surrounding contracts.

## Why these records exist

| Record | Purpose | What it cannot establish alone |
| --- | --- | --- |
| Fixture manifest | Bind an artifact to its exact immutable files, actions and providers | Publisher trust, a safe file reader or an installed sandbox |
| Private-root owner header | Bind one allocated root to machine, volume, account and data lifetime | Permission to adopt/delete a path or proof the volume is genuine |
| Plan intent | Bind an exact preview, actor, targets, read set and effects | Caller authorization or completion of filesystem/process effects |

The reference accepts dictionaries/JSON for development convenience and emits
bytes. Services must use bounded typed inputs and verified database projections.
Do not hash JSON text, native C structures, filenames alone or freshly supplied
client hashes. The C17 owner codec checks the fixed header only; C17 fixture/intent
codecs and a privileged importer are not implemented.

## Shared byte rules

- Each record starts with the exact eight-byte ASCII magic below. Its final digit
  is the format version. An incompatible change gets a new magic and new vectors.
- Integers are unsigned big-endian: u32 is four bytes, u64 eight. A u64 stored in
  SQLite is limited to 0..2^63-1; revisions start at 1. Booleans are not integers.
- S is u32 byte length followed by strict UTF-8 bytes, without a terminator.
  Text rejects C0/C1 controls, NUL and malformed Unicode. No implicit normalization.
- B is u32 byte length followed by raw bytes. ID is 16 nonzero bytes; H is 32 bytes.
  IDs remain identities, not authority. Runtime local allocation uses OS-CSPRNG
  UUIDv4; BootId/volume observations need not be UUIDv4. Test IDs are synthetic.
- Optional fixed fields are one byte 00 for absent or 01 followed by the full ID/H.
  Optional action is 00 or 01 followed by S. Empty and absent are not interchangeable.
- Lists start with u32 count. Record maximum is 131072 bytes. No padding, native
  alignment, host endianness, implicit terminator or trailing bytes are allowed.
- SHA-256 covers the entire canonical record, including magic. The artifact does
  not include its own digest. An intent excludes its digest and ApplyPlan retry key;
  the existing actor/request-key operation binding handles retries separately.
- JSON notation uses lowercase unhyphenated fixed hex for IDs/digests; null for
  absent fields. Duplicate/unknown/missing keys are errors, not defaults. Input
  maximum is 131072 bytes and nesting is limited to 16. JSON is not a wire format.

Component/action tokens use [a-z0-9._-]+ with maxima 128/64 bytes respectively.
P0 relative file paths use ASCII [a-zA-Z0-9._/-]+, at most 256 bytes and 64 per
segment. Empty, dot, dot-dot, absolute, backslash and repeated/trailing-slash paths
are rejected. These lexical checks do not replace Linux bounded path resolution.

## Fixture manifest v1

Exact order:

```text
"AETHFIX1"
S component, ID publisher, S version, S target
u32 file_count
  repeated: S path, u32 mode, u64 size, H sha256
u32 action_count
  repeated: S id, S executable, u32 argument_count, repeated S argument
u32 dependency_count
  repeated H provider_artifact
```

Target is x86_64-linux-gnu, describing the P0 ABI, not all future hardware features.
Version is nonempty, at most 128 bytes. There are 1..32 regular files, each at most
16 MiB and together at most 32 MiB. Mode is 0444 or 0555, with no setuid/setgid or
capability metadata. Files are sorted by path bytes. Duplicates and a file used
as an ancestor of another file are invalid. Directories are derived, not entries.
Reject links, device nodes, sockets, FIFOs, xattrs and unlisted payload entries.

There are 0..8 actions, sorted by unique action ID. Each executable names a listed
0555 file. Each action has 0..8 ordered arguments of at most 256 bytes; empty
arguments are allowed. Arguments are literal, never shell text. No package hooks,
arbitrary environment, mount directives or requested SELinux labels are accepted.
There are 0..8 unique provider artifact digests, sorted by digest bytes. Each must
resolve to an already trusted committed runtime; a digest is not a download URL.

ArtifactId is SHA-256(manifest bytes). Publisher identity/trust must come from the
protected fixture registry with an independently expected ArtifactId. A client
cannot change payload + manifest + expected digest together to enroll new code.
The reference verifies supplied bytes/inventory; it does not open filesystem paths.
The vector's bin/probe is deliberately text marked synthetic-not-an-executable,
not an ELF binary or runnable probe. A real build will have its own artifact ID.

Future Linux enrollment must verify regular files using held descriptors below a
protected root, reject unexpected links/mount transitions and hash the bytes
actually copied into trusted staging. Reopening the original path after hashing
would break that binding. Verified files are made immutable to app domains before
publication; runtime generation leases protect their removal. A cryptographic
library and signatures for general distribution remain a separate release choice.

## Private-root owner v1

Exactly 104 bytes:

```text
"AETHOWN1"
ID machine, ID volume, ID account, ID storage, ID data, ID allocation_operation
```

storage is the 16-byte StorageRef already stored as a BLOB in data_epochs;
P0 paths render it as lowercase unhyphenated hex. allocation_operation identifies
the durable trusted allocation. These values must all agree with expected ownership
and registered-root facts. An old D1 header cannot become D2 by renaming a folder.

volume is a registry identity, not just st_dev or an ext4 UUID copied from the
header. The future protected registry must bind it to the mounted guest filesystem
and held root, and detect unknown/duplicate/cloned registration before admission.
The reference's exact-byte check does not implement that registry or prove a mount.
UID mapping is checked separately against AccountId; UID reuse cannot adopt data.
The future protected allocation registry must retain this header binding for the
root's lifetime, even after the general operation receipt is pruned. No existing
SQL column is silently assumed to contain that registry or allocation operation.

Create the owner header through trusted allocation under protected anchors. Flush
header and required directories before publishing allocation success. Apps see
only payload and cannot edit the header, control database or its sidecars, even
when their numeric UID equals the broker's. SELinux policy must enforce this.
An unexpected type, mode, label, header, volume or root identity blocks destructive
work. A header hash is no substitute for the expected owner and protected context.

## Plan intent v1

Exact order, using the JSON names in the vectors:

```text
"AETHINT1"
ID plan, ID authority, ID actor, ID scope
S kind, S component
optional ID install, optional ID generation, optional ID data
optional H artifact, optional ID grant
S retention, optional S action, ID boot, u64 expires
u32 read_count
  repeated: ID epoch, S kind, B ref, u64 revision
u32 effect_count
  repeated: u32 number, S kind, S component, B ref, S disposition, S reason
```

Target/action/retention shapes match common-v2.sql. Read kinds are scope, component,
install, generation, data, policy, resource, grant and gate. Ref length is 1..128
bytes, preserving the existing SQL contract. A gate's ref must unambiguously encode
its kind and target; a concrete runtime policy must define the ref bytes before
using that subject. Reads sort by (raw epoch, kind bytes, raw ref), with no duplicate
key. Expected revisions are positive. Read count and effect count are each 0..128.

Effects have contiguous unique numbers 0..count-1 and sort numerically. Kind is
install/generation/data/grant/integration/storage, ref is 16 or 32 bytes, disposition
is create/activate/stop/remove/retain/revoke. Reason uses the 64-byte token rule.
Effect numbering is meaningful; swapping two effects changes the digest. Reordering
input arrays without changing their numbering does not change it.

The authority assembles this record from a frozen preview and revalidates required
read coverage and authorization before acceptance. The codec does not infer a
complete business read set. Changing any bound identity, revision, deadline or
effect creates a different digest. Never rewrite an accepted operation's target
to a new installation/data epoch. No fallback interprets unknown versions as v1.

## Process and principal mapping: Linux implementation contract

These roles define admission responsibilities. Protocol endpoints now execute;
the complete lifecycle and broker/app registration remain unimplemented.

| Role | Identity and authority |
| --- | --- |
| System authority | Privileged installation/dispatch writer in its dedicated SELinux domain |
| User broker | Registered account + durable authority epoch + current incarnation, distinct confined domain |
| Fixture controller | Trusted administrative test principal outside the app boundary; no production consent claim |
| Probe/app | Authenticated connection mapped to exactly one eligible LaunchId and its account/install/data tuple |

The system authority registers the actual supervised broker invocation. An app
claiming its UID, bus name, AccountId or incarnation does not register itself.
System and user bus access policy and service method checks both enforce scope.
Do not replace this with sd_bus_query_sender_privilege's same-UID shortcut.

Require bus-authenticated UID, LinuxSecurityLabel and a process-pinning ProcessFD,
bound to the message's unique sender on that bus incarnation. A later pidfd_open
of an unpinned numeric PID is not equivalent. Missing provenance/capability fails
closed; whether the selected guest bus supplies these credentials is an A0 gate.
Do not silently accept /proc-augmented identity as bus-authenticated evidence.

Resolve process membership using the held process reference, trusted supervisor
invocation and cgroup observation, checking that the original process remains alive
before/after observations. Exact UID, app domain/category, BootId, unit/invocation,
account and recorded launch must agree. Cache only under bus incarnation/unique
sender/process/run context; drop on disconnect, exit, restart or mismatch. A bus
connection inherited by children remains the originating run's authority, never
another run. Cross-domain socket/FD transfer and ptrace/process-memory attacks must
be denied; a shared UID must not bypass these boundaries.

The SELinux profile must distinguish authority, broker, controller and app domains
and separate the two same-UID test data identities, with reviewed category
allocation/reuse. Deny apps access to databases/journals, headers, sibling payload,
unselected documents, raw system/user supervisor control and arbitrary broker
registration. App payload remains writable in its allowed root; code stays read-only.
Namespaces restrict visibility; they do not replace mandatory access enforcement.
The temporary fixture policy and connection-credential library below test only
parts of this boundary. No production policy/category allocator or complete
app/run credentials backend is installed yet.

P0 is headless with one protected test-account/session context, not an interactive
logind seat or arbitrary client-supplied session. Its exact registered context is
a runtime policy prerequisite. A later desktop launch must bind verified logind
session/seat instead. Mutable context cannot be changed underneath an accepted
plan; its relevant policy/read-set revision must invalidate a stale preview.

## Launch evidence and recovery

The accepted reserve/ready/dispatch protocol still applies. Record exact system
and user fences before requesting a derived systemd unit. No shell command or
caller-selected executable/environment is accepted. Initial app setup must have
verified mounts, file descriptors, UID, label, capabilities and isolation before
any target code can run. The trusted launcher is transient, not a resident monitor.

A systemd job reply, Type=exec for the launcher, or EOF on a close-on-exec pipe
alone is insufficient evidence of target exec: a launcher can exit without exec.
Before implementing observed, choose and test target-exec evidence against that
counterexample. Candidate: authority-controlled exec-event observation of the
paused launcher with held process/executable identities, followed by verified
target-domain observation and release. The guest SELinux/ptrace interaction must
be proven. This backend is a review gate, not an implemented tracing subsystem.
Keep dispatch uncertain instead of inventing an observed target after a timeout.

On each authority restart, validate catalogue + actual owner + enforcement, commit
a fresh runtime incarnation with admission closed, then reconcile. Old broker
exit can settle its native descriptors; it does not establish app/child exit.

| Durable observation | Required recovery behavior |
| --- | --- |
| Reserved, never dispatched | Resolve cancellation/readiness under current fences; do not infer from elapsed time |
| Dispatching/uncertain | Reconcile recorded unit/invocation and outstanding supervisor request; never blindly start again |
| Running app after broker exit | Retain code/data obligations and deny stale native access; reconcile app separately |
| Closing grant | Deny new reads; settle admitted bounded work before terminal completion |
| Interrupted private purge | Verify original header/root and repeat only recorded remaining steps; preserve I2/D2 |
| Visible rename with failed directory flush | Repeat required durability work before marking the step applied |
| Missing/conflicting proof | Keep blocked/recovering and report a reason; no silent adoption or successful purge |

Graceful drain has no automatic SIGKILL escalation. Both catalogues and actual
process/descriptor obligations must settle before private removal succeeds.
Kernel/manager state, filesystem operations and SQLite are separate transaction
domains. Event-driven reconciliation replaces periodic blind polling; deadlines
end waits but never manufacture completion evidence.

## Checks and remaining gates

```text
python tools/test_runtime_contracts.py
python tools/test_contracts.py
```

Vectors were constructed independently from the reference encoder. Tests cover
byte identity, each record's binding, unsafe/duplicate inputs, changed same-size
payload, mismatched owner and old-install versus new-install intent. These are
host tests, not Linux permission, file-resolution or service recovery tests.
The exact fixture enrollment registry, gate/policy read refs, guest principal
mapping, SELinux policy and exec evidence must be implemented before live exposure.

## Native core and building

[owner.h](../src/core/owner.h) defines the 104-byte header codec without hashing
native structs or allocating memory. Failed decoding leaves its output unchanged.
[storage.h](../src/core/storage.h) opens only bounded regular files below a borrowed,
already trusted directory descriptor. It returns an owned read-only/CLOEXEC FD to
the trusted caller, not to an app. It requires openat2 containment/no-symlink/
no-mount-crossing flags with no weaker fallback. Owner reads also check UID,
writable mode bits, full length and all expected identity fields.

The caller still owes root/volume registration, MAC protection against same-UID
writers, admission and lifetime coordination. These functions do not implement
application authorization, a public document API or durable removal.

Run in the dedicated Linux guest with the recorded GCC/CMake/Ninja toolchain:

```text
cmake -S /home/aetheri/source -B /home/aetheri/build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAETHERI_SANITIZERS=ON -DAETHERI_PRIVILEGED_TESTS=ON -DAETHERI_LINUX_IPC=ON
cmake --build /home/aetheri/build/debug --parallel 2
XDG_RUNTIME_DIR=/run/user/1000 ctest --test-dir /home/aetheri/build/debug --output-on-failure
```

Repeat in a separate release build with CMAKE_BUILD_TYPE=Release and sanitizers
off. The privileged test option defaults off. In the disposable guest it adds
sudo/unshare for a private mount namespace, verifies separation from PID 1 before
any mount change, makes propagation private and uses only newly created test
directories. The test proves a real same-filesystem bind is reachable by ordinary
openat yet rejected with EXDEV by the bounded reader. No physical disk is mounted.
All successful fixture tests remove their own known files/directories.

The three core-focused tests pass in Debug with ASan/UBSan and Release. They exercise the
fixed owner vector, output/buffer bounds, actual reads, UID/mode mismatch, symlink,
FIFO, directory, hardlink, oversized file and mount containment. This is kernel/
codec evidence; it does not establish the future Aetheri SELinux app policy.

The [native catalogue boundary](README.md#native-catalogue-boundary) now adds
canonical v2 creation/opening and startup transactions. Its SQLite SDK is the
reviewed carrier dependency. Build/test additionally needs Python 3.11+ for SQL
embedding, without adding a runtime Python interpreter requirement to services.
Four catalogue CTest cases bring the native suite to eight; see the guide for
failure, process-exit and populated-migration evidence and remaining caller duties.

## Native bus-credential capture

[peer.h](../src/linux/peer.h) captures the unique sender, bus incarnation,
bus-authenticated connection UID/PID/SELinux label and an owned CLOEXEC ProcessFD
from a received method call. It requires all four credential fields, omits
SD_BUS_CREDS_AUGMENT and rejects augmented provenance. Missing ProcessFD does not
fall back to pidfd_open of a later numeric PID. Failed capture preserves the
output. A successful capture must be cleared; it does not stop/signal the sender.
The authority must supply its trusted local bus connection. This helper does not
authenticate an arbitrary bus address/server supplied by an app.

The held process descriptor supports zero-time exit observation. This pin identifies
the original connection process, not an executable, a new request's worker or an
Aetheri run. A label captured when the connection opened is not a proof of its
current domain. Supervisor invocation/cgroup, current label, registry, policy,
admission and connection-lifetime invalidation remain mandatory caller work.
No same-UID authorization shortcut, public lifecycle capability or resident
credential-monitor process is introduced.

Enable the optional Linux integration with AETHERI_LINUX_IPC=ON. It links the
selected dynamic libsystemd; pkg-config checks API floor 259, while vm-inputs.json
records the reviewed carrier package release. Run its test in the test account's
active user-bus session, for example:

```text
cmake -S /home/aetheri/source -B /home/aetheri/build/debug -DAETHERI_LINUX_IPC=ON
cmake --build /home/aetheri/build/debug --parallel 2
XDG_RUNTIME_DIR=/run/user/1000 ctest --test-dir /home/aetheri/build/debug --output-on-failure
```

The new test uses a temporary unique-name object on the existing user bus, with
separate server/client processes and no installed bus name or policy change.
It checks the real sender against the known child UID/PID/label and bus ID,
128 repeated captures without FD growth, FD close/exit behavior and invalid
message/output preservation. Test-only linker wrapping supplies full-but-augmented
and incomplete credentials for rejection cases; the positive case uses real
sd-bus queries. These processes run outside the app boundary. Four native tests
pass in Debug ASan/UBSan and Release after an offline reboot; this is not A3
confinement or complete principal authorization.

## Disposable SELinux fixture boundary

[p0-fixture.cil](../policy/p0-fixture.cil) is an original, narrowly scoped test
module for the selected enforcing Fedora carrier. One app domain is constrained
by MCS; two synthetic data roots use the same type/UID but disjoint fixed
category pairs. Broker-state and unselected-document fixtures have separate
types with no app read/write allowance. Compile-time neverallow rules also guard
those types, privilege capabilities, domain changes and controller/supervisor
ptrace. This is not the production broker/system policy or a category allocator.

The [C17 probe](../tests/probe_policy.c) runs through a transient systemd service
as UID 1000. The controller sets the exact SELinux context, empty capability
bounding set, NoNewPrivileges, a private mount namespace and read-only system
with its own data exception. It disables automatic SIGKILL escalation. Each probe
verifies its actual UID, kernel-provided label, empty effective/permitted/
inheritable capabilities and NoNewPrivileges. It reads/appends its own fixture,
creates a file with the exact inherited data label, and requires EACCES for
sibling data, synthetic broker state, unselected document and sudo execution.
Outside-app controls prove that this same UID can read the negative fixtures
and can use the carrier's administrative sudo route. Enforcing audit records
confirm MCS search and type read/execute denials; this is not a DAC-only test.

Run explicitly in the disposable carrier, after reviewing the module/controller:

```text
cmake -S /home/aetheri/source -B /home/aetheri/build/release -DAETHERI_POLICY_PROBES=ON
cmake --build /home/aetheri/build/release --parallel 2
sudo python3 /home/aetheri/source/tools/test_linux_policy.py
```

Building the optional probe does not install policy. The root-only controller
checks the carrier marker/firmware/product, enforcing mode, fixed fixture account
and absence of an existing module/root. It never changes enforcing mode or
relabels another project. It copies/hash-records its probe/policy, creates only
the fixed disposable /var/lib/aetheri-p0-policy-test tree, tests both identities,
checks settled units/jobs and removes its enumerated files and exact module.
The JSON result/attempt records stay under the guest build directory.

Failure preserves the fixture for inspection. The explicit
--recover-owned-fixture path refuses unacknowledged dispatch, live/unknown unit
state, altered module bytes or unexplained entries. It compares the installed
priority-400 CIL with its owned input before removing that module. These checks
depend on the selected Fedora policy-store layout and are test tooling, not the
future Aetheri recovery/removal implementation.

Both probes pass in the offline enforcing guest, including capability checks;
the module/root are absent afterward. Initial setup failures exposed two missing
supervisor permissions (mounton of the new data directory and execute of the
new entry type). Both stopped before target exec and were corrected narrowly.
No app mount/privilege permission or permissive mode was added.

This supplies a part of A3. Live broker/runtime bus authorization, ptrace/FD-transfer
tests, protected SQLite writers, registry/category lifecycle, selected grants,
authority-controlled target-exec evidence and full A2–A9 remain pending.

Primary references inspected 26 October 2026:

- [D-Bus credentials](https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-get-connection-credentials):
  ProcessFD, numeric PID limitations and LinuxSecurityLabel.
- [sd-bus sender credentials](https://www.freedesktop.org/software/systemd/man/latest/sd_bus_query_sender_creds.html)
  and [credential fields](https://www.freedesktop.org/software/systemd/man/latest/sd_bus_creds_get_pid.html):
  available observations, augmentation and executable-path limitations.
- [openat2](https://man7.org/linux/man-pages/man2/openat2.2.html): containment,
  symlinks and mount crossings, including bind mounts.
- [systemd execution](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html):
  setup and sandbox primitives; settings still require actual enforcement tests.

Record layouts, limits and recovery policy are original Aetheri design, not claims
that these references provide a finished Aetheri service. The augmented-mask manual
URL was unavailable; no claim relies on its unavailable contents.

## Confined protocol services

`src/authority/main.c` builds one C17 executable with system/user roles. Both now
execute in the enforcing disposable carrier, each with its own catalogue writer.
This is the first service boundary, not a working app manager: GetProtocolInfo
returns wire 1.1, its durable authority epoch, a fresh event incarnation and an
empty capability list. All other public methods return UnsupportedCapability;
Runtime1 returns NotAuthorized until broker registration is implemented. No
subscription object, launch, grant, revocation or app-removal capability exists.

`tools/embed_api.py` generates ignored native vtables from the accepted XML at
build time. Interface roles are explicit. Names, directions and argument types
are compared with live method introspection; custom XML annotations are not
reproduced as dynamic vtable annotations. No frozen XML or v1 SQL was changed.

Startup requires the expected effective UID, enforcing SELinux type,
NoNewPrivileges and empty effective/permitted/inheritable capabilities. A fixed
root-owned registry binds each role to its durable authority/account identities.
The native store verifies these against v2 metadata. The actual kernel boot and
fresh runtime incarnation are committed before acquiring the bus name and sending
systemd readiness. Each handler captures required bus-authenticated ProcessFD and
connection credentials; the user role also requires its account UID. It releases
the pin after these read-only/unsupported calls. This is not sufficient authority
for future mutating methods: registered app/run, current-domain, invocation,
revision and volume evidence must be added before those methods are enabled.

The explicit root-only `tools/p0_services.py` controller installs only the dedicated
guest bootstrap below. It uses the existing carrier tools and reviewed SDKs; no
new library family or package transaction was needed. Building with
`AETHERI_LINUX_IPC=ON` creates the authority. Add `AETHERI_POLICY_PROBES=ON` for the
confined C17 service client. Neither build option installs policy or starts services.

| Guest path | Owner and purpose |
| --- | --- |
| /var/lib/aetheri-p0 | Root-owned 0755 bootstrap, separate from the proposed production layout |
| registry/system.conf, registry/user-1000.conf | Root-owned 0444 fixed identities; apps cannot read them |
| system | Root-owned 0700 system catalogue and stable writer.lock |
| users/&lt;account UUID&gt;/control | UID 1000, 0700 user catalogue; MAC prevents same-UID app access |
| system-authority, user-authority, probe-service | Root-owned 0555 copies with separate entry types and recorded hashes |
| user-bus | Root-owned empty 0444 mount placeholder; only user/probe namespaces bind the actual session socket here |
| probe-data | UID 1000, 0700 synthetic c31,c32 data; the client creates, reads and removes a scratch file |
| controller.json, aetheri_p0_services.cil | Root-only operation receipt and root-owned policy input |
| /etc/dbus-1/{system.d,session.d}/90-aetheri-p0.conf | Exact root-owned bus-name associations and FD-call restrictions |

The registry is a bounded ASCII bootstrap format, not a general registry API:

```text
AETHERI-P0-AUTHORITY-1
kind=system
epoch=<32 lowercase hexadecimal UUIDv4 digits>
owner=-
uid=0
```

For the user, kind is user, owner is its 32-digit UUIDv4 and uid is 1000. Order,
line endings and final newline are exact; complete files must be smaller than
256 bytes. The daemon accepts no client-controlled path or identity. This does
not register a production volume, firmware identity or arbitrary user account.

The service CIL gives the two authorities separate state types. A confined app
can send public messages to User1, but cannot read authority state/registry,
acquire its name, send raw System1 messages or use authority FDs/ptrace rights.
Compile-time neverallow rules complement actual negative tests; ptrace and all
descriptor-transfer paths still need their own complete P0 acceptance. The bus
policies deny FD-bearing method calls to authority owners. They leave bus credential
replies intact, so the required bus-sourced ProcessFD remains usable. No permissive
mode, blanket generated policy or fallback to numeric PID authorization is used.

Transient system services set private mount namespaces, ProtectSystem=strict,
ProtectHome=yes, their own writable state exception, empty capability bounds,
namespace restrictions and no automatic SIGKILL escalation. RestrictSUIDSGID is
explicitly disabled: its selected seccomp implementation blocks openat2, which
the native reader requires. NoNewPrivileges, empty capabilities, readonly mounts
and MAC remain required. No weaker path-resolution fallback was introduced.
Binding into ProtectHome=tmpfs initially failed because init_t could not create
the destination file with user_tmp_t. The final profile uses the precreated,
separately typed root-owned mount placeholder instead of broader init permissions.

User/probe units follow the existing user@1000.service through BindsTo/After.
The headless SSH login supplies that session; lingering is not enabled. Logout
can stop the user role and replace the session socket. A later explicit start
checks actual bus/supervisor ownership before starting a fresh role. The last
controller state alone never proves that an endpoint is still alive.

Run inside the dedicated guest after reviewing the controller and policy:

```text
sudo python3 /home/aetheri/source/tools/p0_services.py prepare
sudo python3 /home/aetheri/source/tools/p0_services.py start
sudo python3 /home/aetheri/source/tools/p0_services.py check
sudo python3 /home/aetheri/source/tools/p0_services.py restart-check
sudo python3 /home/aetheri/source/tools/p0_services.py idle-check
sudo python3 /home/aetheri/source/tools/p0_services.py stop
```

Use prepare only when no owned bootstrap/module/bus file exists. After reviewed
source changes and a Release rebuild, update-owned verifies the existing bytes,
stops its recorded units and replaces only its policy/binaries/bus inputs. No
catalogue is overwritten or implicitly migrated. Actions serialize on the stable
root-owned carrier marker. Receipts are written with exclusive temporary creation,
fsync, rename and parent flush; a pending dispatch survives an observation timeout.

`cleanup` is administrative removal of an **empty development bootstrap**. It
requires no pending dispatch, drained recorded units and jobs, unowned authority
names, exact expected entries/permissions and empty non-metadata tables. Symlinks,
sidecars, unknown files or managed rows cause refusal. It enumerates/unlinks only
known files; no recursive shell deletion or physical-storage operation is used.
Update/cleanup are not a production transactional installer: interruption between
policy/file changes requires manual reconciliation. No automatic repair, power-loss
guarantee or general app-removal acceptance is claimed.

`lifecycle-check` explicitly restarts, checks an unowned-name takeover denial,
adds an unknown file and requires cleanup to preserve it, then removes that exact
test file. It removes the inspected empty bootstrap and prepares a fresh one,
requiring new account/authority identities. Its final check succeeds on both new
roles. This action intentionally replaces only empty prototype metadata.

Executed 26 October 2026 in the offline carrier: both roles and live method shapes,
real boot binding, same-UID app discovery/own scratch access and the specified
negative calls all pass. The C17 client verifies its actual domain, UID, empty
capabilities and NoNewPrivileges. Bad argument signatures, unsupported methods and
unexpected FDs return their expected errors. Restart and empty-bootstrap lifecycle
checks pass. Native Debug ASan/UBSan and Release suites pass 8/8 (20.25/19.86 s);
host orchestration/cleanup guards pass 17/17 (0.198 s).

A single 5.023 s idle sample observed one task per authority, zero reported CPU
and voluntary/involuntary context-switch deltas, PSS 2184/2147 KiB, RSS 6532/6340
KiB and cgroup MemoryCurrent 1634304/1650688 bytes (system/user). Memory measures
have different accounting scopes; context switches are not a precise wakeup count.
This is not a long-term performance, startup/latency or finished-OS budget. The
controller is a short-lived development tool, not another resident service.

Next remain actual broker registration/principal mapping, protected volume/read
refs, fixture import, launch supervision/target-exec, access transport and recovery.
No app can yet be launched or removed through these public endpoints.

## Live process and invocation proof

[process.h](../src/linux/process.h) adds a C17 observer for the existing
bus-authenticated process pin. It neither accepts a caller-selected executable nor
opens a new process descriptor from the caller's numeric PID. A separate self API
pins only the calling process, so an authority can corroborate its own context.
It must never replace a missing bus-sourced caller ProcessFD.

Each observation requires all of the following:

- PIDFD_GET_INFO supplies PID, credentials and cgroup ID, with its returned mask
  checked. PID and thread-group leader must match the captured connection. Real,
  effective, saved and filesystem UIDs must still equal its connection UID.
- Current SELinux attributes, read under the held live reference, match the
  connection label. Current attributes are separate observations, not relabeled
  bus-authenticated credentials.
- The trusted host system bus resolves GetUnitByPIDFD to an actual system service
  and nonempty invocation ID. Unit identity, active state and InvocationID agree;
  ControlGroup comes from the Service interface, not the generic Unit interface.
- The pidfd-based cgroup path agrees with that service's path. The opened directory
  must be on cgroup v2; openat2 rejects symlinks, escaping and nested mount crossings.
  Its inode must agree with the kernel's cgroup ID.

The observer repeats the complete context and checks liveness before returning.
Mismatch, unsupported features or missing evidence fail closed and preserve the
output. Verify compares boot, UID, label, unit, invocation, cgroup path and ID with
a trusted expected record. That record must come from supervision/registration,
never an app's claimed tuple. The snapshot is not a lock against future changes,
an exec observation, registry enrollment or a catalogue admission fence. Policy
must prevent untrusted credential/domain/cgroup changes and cross-run FD/ptrace
attacks. Revalidate at admission; do not cache this as permanent authority.

This first backend requires a system-managed .service with exact cgroup membership,
the host PID namespace and trusted host proc/cgroup/system-bus views. It does not
generalize a user@ parent, a .scope, a child delegated cgroup or an arbitrary
container into an Aetheri app identity. The selected carrier supplies the required
glibc sys/pidfd.h ABI and kernel ioctl fields. Unsupported SDK/runtime combinations
must fail, without a weaker numeric-PID fallback. No library family or installed
release changed for this implementation.

The system endpoint now corroborates its own active context and compares the
observed boot/invocation with its actual boot and systemd-provided invocation.
Missing evidence returns NeedsRecovery. The user endpoint does not gain raw
system-manager access. Narrow root-domain policy additions allow read-side manager
messaging/status and cgroup directory/filesystem observations; no tracing, cgroup
writes or Linux capabilities were added. Actual enforcing USER_AVC and filesystem
getattr denials identified the missing status/fstatfs permissions before correction.

The optional linux.process CTest requires AETHERI_LINUX_IPC and
AETHERI_PRIVILEGED_TESTS. Before requesting any system unit it verifies the fixed
carrier product/marker, UEFI, enforcing mode and UID 1000. It starts a unique,
bounded transient test service through the carrier's existing sudo route. The
fixture uses the developer's unconfined context, empty capability bound and
NoNewPrivileges. A standard env executable supplies the entry into that context
before executing the home-directory test binary; it is a development fixture,
not the trusted Aetheri launcher or target-exec proof. No blanket permission for
init_t to execute user-home files was added. Completed fixtures use --collect;
failure diagnostics remain in CTest/journal and unknown/live state is not an
instruction to force-kill or redispatch.

Real bus calls exercise the kernel/systemd positive path, seven wrong expected
bindings, stale PID/label, unchanged error output and 32 repeated peer observations
without FD growth. Eight repeated self observations also balance descriptors.
After actual termination and a second start using the same unit name, a fresh
invocation is observed and the old expected context is rejected. Test-only ioctl
wrapping supplies missing credential-mask, unsupported ioctl, wrong-cgroup and
mid-observation credential changes; positive evidence comes from the real kernel.

Native full suites pass 9/9 in Debug ASan/UBSan and Release (32.56/27.86 s during
overlapping policy work). Final focused process checks pass again (0.72/0.60 s).
The updated enforcing endpoint discovery, existing app denials and actual restart
checks also pass. A 5.030 s idle sample records one task each, no reported
CPU/context-switch growth, PSS 2184/2267 KiB and MemoryCurrent 1630208/1658880 bytes
(system/user). This remains a short observation, not a full performance budget.
Owned services were stopped afterward; prototype data/policy stay in the guest.

Next: connect this evidence to protected broker/app/run registration and current
catalogue fences, then fixture import and reserve/ready/dispatch with target-exec
observation. No lifecycle capability is enabled by this observer alone.
