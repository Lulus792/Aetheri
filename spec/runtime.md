# P0 runtime contracts

26 October 2026. Byte formats below are implemented by the development reference
[runtime_contracts.py](../tools/runtime_contracts.py) and checked against
[frozen vectors](vectors/runtime-v1.json). They add no SQL tables or D-Bus methods.
The native C17 owner codec and bounded Linux file reader are now implemented and
tested in the guest. Authority/credential mapping, volume registration, app policy,
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

All names below are implementation responsibilities, not running services.

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
No policy source, category allocator or credentials backend is installed yet.

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
cmake -S /home/aetheri/source -B /home/aetheri/build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAETHERI_SANITIZERS=ON -DAETHERI_PRIVILEGED_TESTS=ON
cmake --build /home/aetheri/build/debug --parallel 2
ctest --test-dir /home/aetheri/build/debug --output-on-failure
```

Repeat in a separate release build with CMAKE_BUILD_TYPE=Release and sanitizers
off. The privileged test option defaults off. In the disposable guest it adds
sudo/unshare for a private mount namespace, verifies separation from PID 1 before
any mount change, makes propagation private and uses only newly created test
directories. The test proves a real same-filesystem bind is reachable by ordinary
openat yet rejected with EXDEV by the bounded reader. No physical disk is mounted.
All successful fixture tests remove their own known files/directories.

Three native tests pass in Debug with ASan/UBSan and in Release. They exercise the
fixed owner vector, output/buffer bounds, actual reads, UID/mode mismatch, symlink,
FIFO, directory, hardlink, oversized file and mount containment. This is kernel/
codec evidence; it does not establish the future Aetheri SELinux app policy.

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
