# P0 runtime contracts

26 October 2026. Byte formats below are implemented by the development reference
[runtime_contracts.py](../tools/runtime_contracts.py) and checked against
[frozen vectors](vectors/runtime-v1.json). They add no SQL tables or D-Bus methods.
The native C17 owner/fixture decoders, bounded Linux file reader and bus-credential capture
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
client hashes. The C17 owner codec checks the fixed header; the C17 fixture decoder
now validates canonical metadata bytes. The native SHA-256 verifier binds them to
an independently expected ArtifactId and checks held-file content. Native intent
encoding and a privileged importer are not implemented.

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
publication; runtime generation leases protect their removal. OpenSSL 3.5 LTS is
the accepted native hashing backend; signatures for general distribution remain
a separate release contract.

### Native fixture decoder

[fixture.h](../src/core/fixture.h) exposes bounded C17 views of canonical AETHFIX1
metadata. Text views borrow immutable input bytes and carry lengths; they are not
NUL-terminated C strings. The caller must retain the input until all views are no
longer used. Decoding allocates nothing and leaves output untouched on failure.
Unused slots are empty. It adds no filesystem lookup, service, schema or wire API.

The decoder validates exact field/count/byte bounds, strict UTF-8, the contract's
C0/C1 exclusion, target and nonzero publisher identity. File paths/actions and
dependency digests must already be in canonical order. It rejects duplicate files,
file ancestors (including a non-adjacent ancestor in sorted order), unsafe paths,
unsupported modes, payload-size overflow, undeclared/nonexecutable action targets
and trailing bytes. Actions reference a checked file index; arguments remain literal
length-delimited text. It never interprets them as shell code.

The test-only batch reader and Python driver compare native field projections
with the frozen vector and independently expected fields. Valid cases include
zero-byte files, empty arguments, Unicode boundaries, maximum counts/lengths and
the exact payload-size limit. Negative cases include invalid UTF-8/controls,
wrong order, aliasing paths, hostile counts and an oversized record. Every shorter
prefix of the frozen vector is rejected with unchanged output.

This is metadata validation only: supplied SHA-256 values are copied, not trusted
or checked against payload bytes. File inventory, held-source copy/hash, publisher
registry, immutable publication and catalogue admission remain importer duties.
The decoder itself does not hash. The verification functions below add that
separate check without changing the frozen manifest or SQL/XML contracts.

### Native SHA-256 verification

[digest.h](../src/core/digest.h) provides bounded byte and held-regular-FD hashing
using OpenSSL EVP. It selects SHA256 from provider=default in a private, synchronous
call-local library context; it accepts no algorithm, provider, configuration or
path from a client. It frees its own resources without changing or cleaning up the
process-wide default context. Context allocation, provider/fetch/init/update/final
and unload failures return errors; there is no original cryptographic fallback.
The 3.5 family and maintenance floor 3.5.9 are checked at build and runtime.

The FD function hashes offset zero through the exact expected length, at most
16 MiB, in 64 KiB chunks. It preserves the borrowed FD and its seek offset, retries
EINTR, handles short reads and rejects nonregular files, size mismatch, early EOF
and read failures. It compares inode/device, ownership, mode, links, size and
mtime/ctime before/after. Outputs remain unchanged on every error, including crypto
cleanup failure. These observations detect ordinary drift; they do not exclude a
writer or make a file immutable. Copy/hash into protected staging is still required.

`ae_fixture_verify_manifest` validates canonical bytes and checks their digest
against the caller's independently trusted ArtifactId before returning borrowed
views. `ae_fixture_verify_file` checks the length/digest of one decoded file using
its held FD. Neither function opens a pathname, authenticates a publisher, checks
the complete directory inventory or publishes/adopts an installation. Replacing
payload, manifest and a newly supplied expected digest together remains forbidden.
Public digests are compared as ordinary bytes; they are not secret authenticators.

The [native test](../tests/test_digest.c) and
[independent driver](../tools/test_digest_native.py) cover empty/abc/million-a known
answers, frozen manifest/payload values, binary data, 64 KiB chunk boundaries and
the exact 16 MiB bound. They reject changed canonical metadata against its original
ArtifactId and same-size altered payload. Fault tests exercise crypto setup/final/
cleanup errors, interrupted/short/failed reads, stat errors and observed drift,
with unchanged outputs/offsets and retained borrowed FDs. Invalid OpenSSL config/
module locations do not alter these fixed private-context results on this carrier.

The fixed administrative allocation check also verifies its independently known
hello-newline digest under the real enforcing, cap-zero user authority domain.
This is a synthetic integration proof, not a public grant or package importer.
The [dependency review](../brains/aetheri/SOURCES.md#openssl-35-lts-adoption--26-october-2026)
records exact RPM provenance, license, footprint and test evidence.

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
The independent P0 allocation record below now retains this header binding outside
the general operation receipt. Full allocation-journal and volume admission remain
future work. No existing SQL column is silently assumed to contain that registry
or allocation operation.

Create the owner header through trusted allocation under protected anchors. Flush
header and required directories before publishing allocation success. Apps see
only payload and cannot edit the header, control database or its sidecars, even
when their numeric UID equals the broker's. SELinux policy must enforce this.
An unexpected type, mode, label, header, volume or root identity blocks destructive
work. A header hash is no substitute for the expected owner and protected context.

### Retained root and storage observations

[root.h](../src/core/root.h) now observes a held directory using required statx
fields and STATX_MNT_ID_UNIQUE, with filesystem type/ID from fstatfs. It repeats
mount/inode/device/UID/GID/mode observations and rejects disagreement. A missing
unique-mount field fails even when a legacy recycled mount ID exists; no weaker
device/inode-only fallback. These structs are local runtime observations, not a
serialized format or persistent volume identity. Filesystem IDs alone do not
authenticate a volume or detect a replacement/cloned whole machine.

The native catalogue now records this identity on open/create. Before ordinary
uses, the retained directory and a no-symlink reopening of the protected absolute
path must agree. This catches a same-device/same-inode bind mount substituted at
that name. Only directory FDs are reopened/closed here: independent database-inode
FDs must still not be closed while SQLite POSIX locks are active. Parent paths and
mount topology require trusted writers/quiescence; observations are not a lock
against a concurrent privileged namespace mutator.

[storage.h](../src/core/storage.h) also exposes an opaque internal storage_ref.
Its constructor borrows a trusted anchor and independently expected owner/root
identity, derives the sole 32-hex leaf from StorageRef and opens it with openat2
beneath/no-symlink/no-mount-crossing resolution. It owns duplicate CLOEXEC anchor/
root FDs and copies the expected bindings; caller FDs may close afterward.
The root/header must be the expected account UID at modes 0700/0600. Caller-supplied
values cannot establish their own trusted allocation: protected account/volume/
allocation provenance and enforcing labels remain the owning authority's duties.

Each internal bounded-file open checks the retained anchor/root and named leaf,
then the exact owner header, before/after opening below payload/. Payload must be
account-owned 0700; the regular, singly linked bounded file must be account-owned.
The P0 ASCII path/64-KiB limit remains. Root owner/control metadata is outside the
payload namespace. Errors close transient file FDs. Fork children cannot use or
close an inherited storage_ref. The reference adds no resident process or polling.

An FD returned internally remains a read-worker obligation until explicitly closed.
Closing a storage_ref cannot revoke that FD; the tests deliberately prove it stays
readable. P0 native document transport must return bounded bytes and close its
internal FD, with the admitted read tracked through completion. This does not add
the deferred direct-descriptor grant mode or implement a public document method.

Native tests use actual private temporary roots and payloads, independent expected
test allocations, copied-header replacement, all six owner-field mismatches,
permissions/UID changes, symlink escape, oversize, fork and 64 open/read/close cycles.
A test-only header change at post-open validation rejects the result and balances
FDs. Required-mask/unsupported-statx and changing directory metadata are injected
separately; those are fault tests, not fabricated kernel-feature support.

Two opt-in private mount-namespace tests bind a directory over its own name.
Actual inode/device remain equal and unique mount IDs differ. The private storage
reference returns EXDEV; the catalogue rejects reads/startup writes with ESTALE and
preserves its database state. After unmounting the owned test bind, the original
binding works again. These tests do not establish durable volume registration,
general app MAC policy, clone detection, purge or public read/revoke acceptance.

### Immutable allocation witness v1

[allocation.h](../src/core/allocation.h) implements the independent object witness,
with [frozen bytes](vectors/allocation-v1.json) checked by both C and Python. It is
exactly 168 bytes, in this order:

```text
"AETHALC1"
104-byte AETHOWN1 owner record
u64 filesystem_type, 8 opaque filesystem-ID bytes
u64 anchor_inode, u64 anchor_birth_seconds, u32 anchor_birth_nanoseconds
u64 root_inode, u64 root_birth_seconds, u32 root_birth_nanoseconds
```

Integers are big-endian and at most 2^63-1; inodes/filesystem type are positive,
anchor/root inodes differ, seconds are nonnegative and nanoseconds are below 10^9.
The filesystem ID is an opaque Linux x86-64 observation, not an ext4 UUID or a
genuine-volume certificate. No mount ID, device number or native struct padding
is serialized. Runtime mount instances change across boots/service namespaces.
Birth time is optional for generic root observation and mandatory for this record.

The registry uses one immutable record named by the 32-hex StorageRef, under a
0700 directory owned by the user authority. The record is a singly linked 0600
regular file. Capture requires independently authorized/flushed allocation setup,
matching held/named root, owner header and filesystem witnesses. It is not an API
for adopting arbitrary existing data. The separate registry retains the allocation
operation binding for the data lifetime, independently of ordinary receipt pruning.

Publication writes an unnamed O_TMPFILE inode in the protected registry, sets its
mode, writes the complete record and fsyncs it. A create-only link then publishes
the exact already-open FD; fsync of the registry acknowledges the durable name.
The documented /proc/self/fd route links only this process's owned FD with no new
capability. It assumes the trusted kernel proc view, not a client-selected PID/path.
Unsupported filesystem/O_TMPFILE behavior fails; there is no named-temp fallback.
Before link, an interrupted unnamed inode disappears on descriptor close. After
link, a failed/lost directory-flush or close acknowledgement preserves the visible record.

Exact same-intent retries compare all canonical bytes and flush the directory;
they do not rewrite the existing inode. A different owner/object at the same key
fails. Reopening reads only existing metadata, checks the independently expected
six owner IDs, anchor/root inode and birth time, and filesystem witnesses, then
constructs a current storage_ref. A copied header on another directory cannot
replace the recorded root. Unknown/missing/mismatched records never trigger repair.

This does not create directories, authorize an account/volume, declare a data epoch
ready, settle a catalogue operation, authenticate a whole cloned filesystem or
implement purge. The owning authority must journal intent before filesystem work
and reconcile uncertain effects before publishing app admission. A filesystem
matching its own supplied witness is not independent volume provenance.

Native tests cover frozen bytes/all truncations, invalid ranges, unavailable birth
time, immutable retries, two concurrent identical publishers, file/link metadata,
all six owner mismatches, copied-root rejection and reopening after closing FDs.
Injected file-fsync failure/process exit before link leaves no named record;
lost directory-fsync acknowledgement/process exit after link leaves exactly one
record. An injected close error after successful publication also reports failure
while preserving the record. Reconciliation retains that identity. These are process/IO fault
observations, not physical power-loss or complete allocation-journal acceptance.

### Confined allocation fixture

The root-only controller now offers storage-prepare and storage-check for an
optional fixed test allocation. Preparation requires a stopped initialized owner
and active fixed account, refuses preexisting paths, durably records the exact
intent first, and creates/flushes the protected root/header/payload/configuration.
Interrupted setup stays preparing and requires inspection; it is not the production
allocation/recovery engine. It performs no direct SQL writes or app installation.

| Prototype path | Purpose |
| --- | --- |
| registry/allocation.conf | Root-owned 0444 AETHOWN1 fixed test intent |
| users/account/control/allocations | User-owned 0700 aetheri_allocation_t private witness registry |
| users/account/data/storage/owner | User-owned 0600 broker-protected owner header |
| users/account/data/storage/payload/document | User-owned 0600 synthetic hello file, app-data type at fixed c31,c32 |

Data parents/root are user-owned 0700 broker-state type; apps receive no metadata
traversal permission. This fixture uses synthetic machine/volume/data/operation
identities, not registered production volumes or admitted DataEpoch/install rows.
The registry adds one 168-byte record; it does not start a resident service.

Native user --storage-publish/--storage-verify actions run as supervised one-shots
under aetheri_user_t, NoNewPrivileges and empty capabilities. They take the existing
owner catalogue writer lock, validate protected fixed configuration/labels, publish
or read the immutable witness, reconstruct the current reference and read the
bounded hello bytes internally. They accept no caller paths/IDs and expose no new
D-Bus method/capability. Read-only verification never publishes a missing record.

Policy allows only the broker to create/link its private witness type and read
app-data fixtures; app metadata access has explicit neverallow rules. Actual
same-UID controls outside the app domain read the record; the confined app with
known StorageRef cannot open record, owner header or configuration. Existing
negative IPC/capability tests still pass. Administrative storage units cannot be
selected as live broker units or active protocol authorities.

Actual native publication, repeat without inode/hash changes and a separate unit's
reopen pass while offline/enforcing. A graceful guest reboot ended QEMU because
the carrier uses -no-reboot. After verifying process exit 0, the same disk booted
offline with a new kernel BootId. storage-check then reopened the original witness
with unchanged hash/inode and reported cross_boot_reopen=true. This is a real
cross-boot observation, not a physical power-loss or complete service-recovery claim.

The optional allocation makes this bootstrap nonempty. cleanup/lifecycle-check must
preserve it and refuse empty-bootstrap deletion until a reviewed storage-retirement
protocol is implemented. Unknown record publication is reconciled only by explicit
storage-check against the same protected intent; no fresh IDs are substituted.
The actual empty-bootstrap cleanup refusal was exercised; native storage-check
afterward reopened the unchanged private witness/payload successfully.

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
the complete lifecycle and app registration remain unimplemented. The final section
records the protected P0 broker enrollment now implemented.

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
Runtime1 returns NotAuthorized to unregistered callers; authenticated active
brokers still receive UnsupportedCapability. Private Broker1 enrollment now works. No
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

Next remain complete app principal mapping and general account eligibility, protected volume/read
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

Next: connect protected broker enrollment and process evidence to app/run records
and current catalogue fences, then fixture import and reserve/ready/dispatch with target-exec
observation. No lifecycle capability is enabled by this observer alone.

## Protected P0 broker enrollment

[broker-v1.xml](api/broker-v1.xml) defines private interface 1.0 on System1:
Register(expected_system_epoch, account_scope_id, broker_epoch, broker_incarnation)
and Check(broker_incarnation), each returning the current system incarnation.
Every argument is exactly 16 bytes, with UUIDv4 checked by the native receiver.
This adds no public lifecycle capability, SQL table or change to the existing
public wire 1.1/frozen v1 contracts.

The fixed eligible test account is UID 1000. Existing protected registry files
bind account and durable user/system epochs. A new root-owned 0444 registry file,
registry/broker.conf, selects its expected supervised unit:

```text
AETHERI-P0-BROKER-1
unit=<controller-derived aetheri-p0-...-user-...service name>
```

The initial unit value is `-`, which cannot enroll. The root controller publishes
a selected slot atomically before its recorded user-service dispatch. Neither
method accepts a client-selected UID, PID, security label, unit, executable or
path. Native identity reads may inspect the other role's root-owned registry;
they never open the other owner's SQLite catalogue. The durable account extension
below now backs this fixed account with a system scopes row. General account
eligibility, multiple users and registered volumes remain future work.

Register captures authenticated connection credentials/ProcessFD, checks the
protected account/epochs, requires the exact aetheri_user_t context and UID, and
joins the current process to that selected unit, invocation, boot and cgroup.
The known user service may still be activating while its Type=notify startup
registers. This stage binds identity only. After the acknowledgement, the user
service sends readiness. Check and Runtime1 authentication always use the normal
observer requiring active state; registration does not bypass that gate.

The registered incarnation is declared by the trusted broker after committing its
own catalogue runtime. Root binds it to the observed connection/invocation; it
does not become a second reader/writer of private user SQLite state. Every use
rechecks current root metadata, enforcing domain, protected enrollment and current
process context. Matching IDs from another caller are insufficient. Runtime1
still returns UnsupportedCapability even for a verified broker; launch/dispatch
effects and their catalogue fences are not implemented.

[broker.c](../src/authority/broker.c) owns one retained process FD and two targeted
subscriptions: its pidfd exit and the bus daemon's NameOwnerChanged for exactly
that unique sender. It subscribes before checking NameHasOwner, covering a
connection that vanished before the subscription. Binding and use recheck state
after nested bus calls. Same-binding enrollment is idempotent; a different live
binding conflicts instead of replacing its FD/context. Connection loss and process
exit invalidate identity and release the watcher/pin. This never settles app
holders, releases generation/data leases or proves that an app cgroup drained.

The user service now owns a second connection, to the fixed system bus. Policy
permits its bus-daemon/System1 traffic and narrow root observations of broker
process metadata. It gains no raw system-manager permission. App domains still
cannot reach the system bus. The user listens only for System1 ownership changes;
there is no global name monitor, polling loop or additional resident process.
After root restart it re-enrolls on demand. One explicit NotAuthorized response
can trigger one idempotent enrollment/check retry, covering notification ordering;
timeouts do not trigger an immediate retry or service redispatch.

The controller manages the additional slot and root-owned probe-broker executable
(0555, user entry type). The probe makes only public discovery and required
negative calls. An explicit update-owned may add this new probe after proving its
path absent in the earlier receipt; ordinary actions require the full inventory.
Unknown or changed files are never silently adopted. Administrative interrupted
update/cleanup still needs inspection. No production migration guarantee is added.

Executed in the enforcing offline carrier:

- Actual system/user enrollment and active Check work with unchanged empty Linux
  capability sets. The user endpoint's public discovery exercises that Check.
- A same-UID unconfined process and a same-domain wrong unit both have working
  public transport, but copied real IDs cannot Register, Check or call Runtime1.
  Existing app-domain raw-system-bus/private-state denials remain.
- Ending the broker reduces root descriptors from 12 to 11. Starting its next
  incarnation restores that one pin with a changed user runtime. Restarting only
  root preserves the live user PID/runtime and establishes a new registration.
- Separate cache tests inject context results, then exercise actual name loss
  while the process stays alive and actual pidfd exit while the name stays owned.
  They cover FD transfer, idempotence, live conflict, reentrant invalidation and
  a name already gone before subscription. They are not substitutes for the
  separate real kernel/enforcing integration.
- Full restart, unowned-name denial, unknown-entry preservation and removal/
  recreation of the inspected empty bootstrap pass with the new inventory.

Native suites pass 10/10 Debug ASan/UBSan and Release (30.75/19.18 s); the final
name-loss race checks pass again in their focused cache case. Host contracts pass
27/27 in 33.642 s and orchestration guards 18/18 in 0.164 s. A test-inventory
format mismatch (spaced argument types versus a concatenated wire signature) was
corrected before that passing run; no wire format was weakened.

A 5.024 s idle sample records one task each, zero reported CPU/context-switch
deltas, PSS 2260/2297 KiB, RSS 6748/6656 KiB and MemoryCurrent 1904640/1662976 bytes
(system/user). This remains a short metadata-service sample with different memory
accounting scopes. Owned services were stopped; no wakeup/workload/hardware or
finished-OS budget is claimed.

Next: native fixture enrollment, verified storage/volume/read references,
app/run records and reserve/ready/dispatch/exec evidence. The complete OS/apps goal
remains; broker enrollment alone does not launch an app or grant document access.

## Durable P0 account enrollment

The system catalogue now stores the fixed test account in the existing scopes
table: protected AccountId, kind account, UID 1000, active state and revision 1.
No schema/wire version, public method or resident process is added. The native
enrollment API accepts trusted administrative identity/UID inputs, never app RPC
arguments. The service obtains both from its root-owned registry.

Enrollment takes a write transaction before inspecting existing state. A missing
scope is inserted with the catalogue revision increment in the same commit.
An exact active retry commits no data changes. A differently bound or retired
scope fails; another active account already using that UID fails. A fresh account
may use a retired account's old UID without adopting or changing the old scope.
The API never retires, reactivates or rebinds accounts, edits credentials, changes
Linux users, attaches private data or grants access to an app.

System --init creates and enrolls the fixed bootstrap account. For an existing
older empty development catalogue, the explicit stopped system --enroll-account
administrative mode opens its original identity and enrolls the protected account.
The writer lock excludes a concurrent service. No supplied account, UID or path
argument is accepted. Normal startup never silently repairs a missing/retired
scope: it requires an already matching active row before changing runtime or
claiming its bus name. Register and broker-use checks also require that row.
The returned account revision is an observation, not a future admission fence.

The root controller's enroll-account action stops/drains owned services before
that one-shot native command. account-check tests the missing-row startup denial
when applicable, explicit enrollment and exact retry, inspecting real database
rows, revision and file hash. Its administrative service is a distinct recorded
unit, never eligible as a running authority or broker. The cleanup inventory may
remove only zero scopes or the exact original active bootstrap row at revision 1;
other, retired, changed or additional accounts require reconciliation.

Executed in the offline enforcing carrier: the previous empty bootstrap refused
normal startup with ENOENT, no owned name and unchanged database bytes. Native
enrollment then succeeded; its retry preserved rows, revision and file hash.
Broker departure/replacement and root-only restart passed again (12 to 11 held
FDs on departure). Full restart, unknown-file preservation and empty-bootstrap
cleanup/recreation passed with the account row created by normal initialization.

Native account tests cover idempotence, live UID collision, wrong UID/owner/fork,
retired scope rejection, a fresh identity after UID reuse and UID 4294967294.
Revision overflow leaves state unchanged. Injected failed/lost commit acknowledgements
and process exits before/after commit reopen with account and revision both absent
or both committed; retry retains one exact account. These are SQLite/process-exit
observations, not physical power-loss or production account-lifecycle acceptance.
All eleven native tests pass in Debug ASan/UBSan (26.96 s) and Release (22.61 s).
Twenty host orchestration guards pass (0.179 s). The first integration attempt
after enrollment exposed a controller receipt grammar missing system-enroll units;
the grammar and active-role boundary were corrected with a roundtrip regression.

SDK/dependency/policy versions are unchanged. General account eligibility,
Linux-account retirement coordination, machine/volume registration, fixture
publication and full app/run admission remain separate implementation work.
