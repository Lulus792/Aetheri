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
an independently expected ArtifactId and checks held-file content. The protected
administrative P0 path now constructs a fixed native install intent, journals the
operation and stages copied payloads. General preview/allocation, atomic store
publication and app/run admission remain pending.

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

### Native payload staging

[import.h](../src/core/import.h) adds two synchronous internal C17 operations:
`ae_fixture_stage_payload` prepares an empty 0700 root from an authority-selected
0555 synthetic source; `ae_fixture_sync_staged_payload` rechecks an independently
journal-bound existing root and repeats all file/directory flushes. Roots are
authority-selected FDs; neither operation publishes an artifact or updates a catalogue. Both require
an independently trusted ArtifactId, operation/root binding, protected parents/
mount namespace, caller-supplied MAC validation and exclusion of other writers.
All objects belong to the authority's effective UID; its umask preserves owner
directory access (normally 0077). Staging root's parent persistence is a caller duty.

The inventory contains exactly the declared files and their derived directories,
including no unexpected empty directories. Directory entry types are not trusted.
Each listed leaf is first pinned with O_PATH and classified; only a regular inode
with the exact length/mode, owner and single link is reopened through its held
proc FD. Contained source/stage lookups require openat2 no-symlink/no-mount flags,
including same-filesystem binds. Root observations require unique mount IDs;
held/named metadata observations detect ordinary substitution/drift. They do not
exclude a privileged mutator or authenticate a volume.

The stager keeps at most 32 source file FDs and bounded derived directory metadata,
not a recursive stack or FD per directory. A 64 KiB buffer copies at most 16 MiB
per file / 32 MiB total. Files are created exclusively at 0600; their writer FDs
close after read-only mode and flush. The final inventory/hash/flush pass reads
those protected copies. Children become 0555; the staging root remains 0700.
No hooks execute. Creation metadata comes from fixed rules and validated manifest
modes; no source data/mode/owner/xattr writes occur. Reads may update source atime.

Visible xattrs other than the host security.selinux label are rejected; no xattrs,
ACLs, capabilities or source labels are imported. An unprivileged list can omit
hidden names, so trusted fixture preparation must exclude those separately. MAC
type/category authorization remains a caller obligation; mode bits alone do not
isolate apps sharing a UID. This is not an arbitrary untrusted archive extractor.

On any error or lost reply, the operation must retain its intent and partial
root. A nonempty root blocks another fresh staging attempt. Resync verifies the
entire read-only inventory and content and repeats every required flush before
returning success, even if an earlier root flush failed. It does not adopt an
unbound candidate, finish a journal or prove physical power-loss durability.
The administrative P0 integration below now supplies these gates for a trusted
carrier fixture. Journaled atomic store publication is described below. General
dynamic allocation and recovery coordination remain next. Existing SQL/XML and
canonical byte formats are unchanged.

### Administrative P0 stage journal

[authority/fixture](../src/authority/fixture.h) connects the stager to the actual
enforcing, cap-zero system role. This is a fixed administrative fixture path, not
a public installer or authenticated arbitrary package upload. Root provisioning
copies the reviewed native development probe into an immutable source and creates
one empty private staging slot as carrier infrastructure. It reads the development
binary into one buffer for that copy/manifest; no untrusted client's hashes enroll code.
The probe uses the carrier's selected shared technical libraries; an empty managed
provider list does not make its ELF self-contained. Declared providers are currently
rejected by this P0 native acceptance path until their admission is implemented.

The source and slot live under the root-owned 0700 directory
`/var/lib/aetheri-p0/fixtures/<StageSlotId>/`. `source/` and its children have
aetheri_fixture_source_t; writable `staging/` has aetheri_staging_t. The containing
slot has aetheri_stage_parent_t, a narrow namespace-mutation boundary. Both payload types
deny executable mappings/execution to Aetheri processes. Apps cannot access the
stage. This StageSlotId identifies preallocated carrier infrastructure; it is not
the private-data StorageRef/DataEpoch/allocation-operation lifetime.

The protected root-owned 0444 `registry/fixture.manifest` contains canonical
AETHFIX1 bytes. `registry/fixture.conf` is an internal 328-byte AETHFIC1 record:

```text
magic[8]
ID plan, operation, actor, request_key, scope, install, generation, slot, publisher, boot
H artifact
u64 scope_revision, expires_boottime_ns, accepted_unix_ms
source_witness[36], staging_witness[36]
H expected_intent
```

The P0 accepted_unix_ms is the root-provisioned administrative request stamp,
not a measurement of the later SQLite COMMIT instant. Deadline enforcement uses
the trusted current boot and CLOCK_BOOTTIME instead of that wall-clock stamp.

Each witness is u64 filesystem type, eight opaque fstatfs ID bytes, u64 inode,
u64 birth seconds and u32 birth nanoseconds. It excludes ephemeral mount IDs.
Read-only fixture-observe obtains witnesses from newly provisioned objects;
the root controller checks their inodes against its independent setup receipt,
persists seal intent, publishes and flushes the exact configuration and retains it.
Normal staging requires exact existing witnesses; it never enrolls them by merely
observing candidates. This is not registered-volume or whole-filesystem clone
authentication. The P0 config fixes target OS UID 1000 and requires UUIDv4 IDs.

The [native catalogue API](../src/catalogue/store.h) constructs the existing
AETHINT1 projection: kind install, no existing install/generation/data/grant/action,
one exact scope revision read, and create effects for the future InstallId,
GenerationId and StageSlotId. It compares its digest to the independent protected
expected intent. One SQLite transaction accepts the plan/operation, preparing
metadata and planned stage step. Current kernel/catalogue boot and deadline are
checked for new acceptance; exact accepted replay retains the original IDs/intent
across boot/deadline boundaries and rechecks active scope/UID/revision.

Before payload IO, another transaction binds SHA-256(configuration) to the executing
step. A planned step requires an empty slot; existing bound work is rechecked/
resynced. Failures preserve data and can mark the step uncertain/operation blocked.
After copy/hash, exact labels, root witnesses, non-executable mappings and flushes
pass, the step becomes applied and the operation remains running/publish. Artifact
store state stays staged, install preparing, generation staged and result/outcome
unset. No lease, activation or completed installation is inferred from READY.

The root controller exposes fixture-prepare, fixture-check and fixture-rebind-check
only in the dedicated marked carrier. After a Release build with
AETHERI_LINUX_IPC=ON and AETHERI_POLICY_PROBES=ON, then update-owned,
fixture-prepare provisions/seals once; fixture-check stages or rechecks in separate
real system units. The substitution probe binds a same-filesystem identical source
copy inside only its unit namespace: native ESTALE and unchanged catalogue are
required before explicitly retiring that verified temporary copy. No live fixture,
private data, unknown tree or volume is cleaned up. Empty-bootstrap cleanup refuses
this prepared installation and the existing private allocation.

### Administrative P0 store publication

[core/publish](../src/core/publish.h) moves the independently bound payload into
a preallocated container, writes a create-only immutable manifest, then publishes
that whole container under `store/sha256/<ArtifactId>/`. Its only children are
`manifest` and `payload/`. Both moves use renameat2(RENAME_NOREPLACE); the original
payload/container inode and birth witnesses survive. An unknown existing target
is rejected, even when its bytes match. There is no copy, overwrite or weaker
rename fallback. All held parents and moved roots must have the same current
unique mount ID. Matching st_dev alone is insufficient.

The native system administrative path owns the journal and filesystem work.
`--fixture-store-observe` reads preallocated objects; the root controller compares
their inodes with its independent infrastructure receipt. It persists seal intent
before publishing root-owned 0444 `registry/fixture-store.conf` (registry type):

```text
"AETHSTO1"[8]
H stage_configuration_digest
store_anchor_witness[36], publication_container_witness[36]
```

This internal 112-byte configuration uses the same witness encoding as AETHFIC1;
both object witnesses must be present. Its SHA-256 is immutable step-1 evidence,
binding the original stage configuration and separate store/container objects.
The controller supplies carrier infrastructure, not a production dynamic allocator
or publisher/signature enrollment. Configuration, receipts and objects are retained
after uncertain results; empty-bootstrap cleanup refuses this installation.

`--fixture-store-publish` first requires an existing exact accepted request and
applied stage. It never accepts a new operation, refreshes its deadline or chooses
replacement IDs. The existing v2 operation_steps table receives step 1, action
publish, kind storage, ArtifactId subject. PLANNED reserves its configuration hash;
EXECUTING is committed before filesystem effects. Failed work can become uncertain
with the operation blocked/publish. Replays locate only the expected container and
payload in the original, nested or final positions. Visible names/content alone
never settle an uncertain operation. Full inventory/hash/identity checks and every
file/directory/old-parent/new-parent flush are repeated, including after a final
name became visible. Unexpected namespace combinations or missing final metadata
are rejected rather than repaired from an unbound candidate.

After the namespace moves, the service seals exact files/directories, revalidates
the whole object and flushes again. Code/data/manifest have separate read-only
SELinux types; directories are 0555 with aetheri_store_dir_t. The system role cannot
write, chmod, relabel from final types or remove the store entry. Apps have no
store access or execution permission yet. Native executable-mmap and owner chmod
probes must fail, so ordinary read-only modes alone cannot claim MAC sealing.
There are no remaining payload/manifest writer FDs at sealing.

The publication unit uses one ReadWritePaths ancestor for this owned P0 root,
with source explicitly ReadOnlyPaths. Separate writable leaf binds would prevent
the required cross-parent rename. UID 0, NoNewPrivileges, empty capabilities,
private mounts and MAC restrictions remain: this namespace choice grants no
registry/private-user writes to the system domain. Only trusted PID 1 mounts it.

Only after those checks does one SQLite transaction set the publish step applied,
artifact verified and generation committed. Install stays preparing with no active
generation; operation stays running/reconcile, reason activation_pending, with no
outcome/result/lease. Applied publication cannot be downgraded by this API; later
corruption must block admission. Lost COMMIT acknowledgement retains the original
binding and requires recheck. First activation is implemented below. General
recovery coordination, updates, launching, document grants, package signatures and physical power-loss acceptance remain
separate work. No public API capability or schema/wire version changes.

After a Release build/update-owned and successful fixture-check, run the root-only
controller actions fixture-store-prepare, then fixture-store-check. The latter
uses separate native units for publication and exact replay; it checks content,
labels, modes, original root inodes and unchanged operation/object metadata.
Once publication starts, use fixture-store-check for reconciliation; the older
fixture-check/rebind check addresses only the original staging location.

### First fixture activation and completion

First activation completes the original fixed P0 install intent. It does not
allocate another InstallId/GenerationId or mutate the accepted plan/effects.
Updates, rollback and a public activate-plan API remain separate work.

The root controller's fixture-activation-prepare starts/stops the native endpoints
to establish the actual catalogue boot/runtime and close bootstrap-open gates,
then reads the expected install/generation/gate revisions. It writes no business
rows. Seal intent precedes root-owned 0444 `registry/fixture-activation.conf`:

```text
"AETHACT1"[8]
H publication_configuration_digest
u64 install_revision, generation_revision, install_gate_revision, completed_unix_ms
```

This internal configuration is exactly 72 bytes. Its SHA-256 binds the prior
AETHSTO1 evidence, revisions and administrative completion stamp. That stamp is
not a measurement of SQLite COMMIT time. No new SQL/XML version or dependency.

System `--fixture-activate` requires an already applied publication. Before first
activation it repeats exact store object/hash/mode/label/flush and denial checks.
The single native catalogue writer reconstructs AETHACT1 and requires matching
evidence, active scope/UID/revision, current kernel/catalogue boot and a nonzero
runtime. It compares install/generation/gate revisions, requires a closed gate
without another operation's closing cause, and rejects live code leases,
dependency edges or dispatch records. Unsupported providers remain rejected by
the original fixture acceptance path.

One SQLite transaction adds step 2 (activate/generation, applied, configuration
hash), sets the original installation active with its exact generation, and
completes the original operation (outcome applied, result InstallId). The existing
trigger records activated_once and advances the generation revision; install,
operation and catalogue revisions advance in that same commit. No external
effect follows those writes, so there is no separately persisted executing phase
for this pure metadata step. Errors/process exit before commit leave no partial
activation. A lost acknowledgement after commit retains one original result.

The install gate stays closed. No app process, code lease, user data, grant,
integration or readiness is created. Source/configuration and carrier test
infrastructure remain explicitly tracked; this is not a general installer or
complete-removal implementation.

A completed retry authenticates the retained original request/configuration and
returns its historical receipt without reactivating, changing gates or rereading
code as launch authorization. It survives a later generation switch or removal
while those P0 registry/scope/receipt bindings are retained. installation_active
separately reports whether that generation is still the current catalogue pointer.
Historical publication/activation steps do not prove current file availability,
policy, data readiness or permission to launch. Fresh admission must establish
those facts again; receipt pruning/expiry and general recovery remain pending.

fixture-activation-check uses separate native units for first execution/retry,
checks the exact completion/generation/gate rows, unchanged published inode/mode/
ctime and an unchanged catalogue hash on retry. fixture-store-check continues to
verify the actual immutable object after activation. No public lifecycle capability
is enabled by either administrative path.

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

## Authenticated installation projection

An active installation in the system catalogue must become known to the user
authority before that authority can manage app data. Keep the two database owners
separate: the system service reads its own consistent snapshot and the user
service writes only its own binding. Neither opens the other owner's SQLite file.
This step implements that observation for the fixed trusted P0 fixture. It does
not create app data or admit a launch.

The new private [Catalogue1](api/catalogue-v1.xml) interface is version 1.0,
independent of public protocol 1.1. ResolveAppInstall requires the already
registered broker's current process and supervised invocation, expected system
epoch and broker incarnation. The account/UID comes from that registration,
never an app-supplied account or filesystem path. One read transaction requires
an active enrolled account, an active app installation in its account or an active
machine scope, a committed/previously activated current generation, verified
artifact metadata and an install gate. Verification here refers to catalogue
state; this query does not rehash code or authorize execution.

The reply contains system epoch/incarnation, boot, install, component, publisher,
recipient account, actual install scope, install revision, active generation,
artifact, generation revision, gate state/revision and catalogue revision. Keeping
recipient and install scope distinct supports a machine installation observed by
an eligible account. UUID references are 16 bytes, ArtifactId 32 bytes, component
an ASCII token of 1–128 bytes, revisions 1 through INT64_MAX and gate one of
open/closing/closed. The native codec rejects malformed/truncated/extra fields and
preserves output on failure. One fixed-endpoint call has a two-second timeout;
there is no automatic retry of an uncertain projection request.

The user writer checks source epoch/runtime/boot, recipient account and its own
current boot/runtime before inserting install_bindings and advancing its catalogue
revision in one transaction. An exact active binding is a no-write retry. Different
identity, epoch or install revision, including a newer revision, and a revoked
binding return ESTALE for explicit reconciliation. No silent rebinding or
reactivation. A lost COMMIT acknowledgement can leave the whole binding committed;
an exact retry finds it without another revision increment.

The persisted binding contains the last authenticated installation observation,
not a durable permission. Generation, artifact and gate observations do not become
user-owned admission authority. A restart or concurrent change can immediately
outdate a snapshot; future admission must ask the system authority again. No SQL
transaction spans IPC or service-manager calls. No DataEpoch, attachment, grant,
launch, access lease or admission gate is added by this path.

The new private [Development1](api/development-v1.xml) version 1.0 provides only
RefreshFixtureInstall(), with no caller-selected install, data or path. It reads
the original root-protected fixture selection and verifies the returned publisher,
artifact and generation before caching. This is a P0 test entry point, not a public
installer or a production app API. Public capabilities remain empty and lifecycle
methods remain UnsupportedCapability.

Root selects one unique transient helper unit in protected registry/projection.conf.
That reviewed helper runs as UID 1000 in aetheri_user_t, with NoNewPrivileges,
empty Linux capabilities and no writable path exception. The user authority pins
its actual bus peer and checks UID/context, current active system unit, invocation,
cgroup identity and boot. It repeats process verification and protected unit
selection before the local transaction. The controller drains the helper and
replaces its slot with '-' after success. Unknown or uncertain work is preserved.
The helper exits; no resident process or polling loop is introduced.

The enforcing policy permits user-domain self IPC, process metadata and read-only
systemd/cgroup status observations. It adds no manager mutation, capabilities or
app-domain system-bus access. Actual integration first established that UID 0
cannot use this carrier's user-bus authentication. The fixed UID-1000 helper uses
the existing transport; it does not relax bus authentication. Subsequent denials
identified self-domain messaging and systemd's status check, each addressed with
the corresponding narrow permission. Same UID or SELinux domain alone is never
sufficient application authorization.

Native catalogue tests exercise account/machine scope, wrong owner/UID/context,
inactive/unverified state, exact retry/reopen, failed/lost commit acknowledgement,
revision overflow, fork guards and conflicts requiring reconciliation. Machine
scope is a controlled database fixture, not an installed machine-wide runtime app.
Wire tests use actual sealed sd-bus message bodies for independent malformed-field
cases and a mocked call boundary for source-context/timeout checks. The separate
enforcing VM probe exercises real system-to-user IPC and each owner's live writer.
Its reports check the exact binding, unchanged system database, no data/gate
effects, stable retry bytes and denied ordinary/same-domain wrong-unit callers.
broker-check exercises a replacement user service and a root-only restart with
the user service retained; projection-check then observes the new system runtime.

Executed results on 27 October 2026: native complete suites 25/25 Debug ASan/UBSan
(99.02 s) and Release (72.47 s), then a final Debug build and six focused identity/
projection/config checks (6.21 s). Host migration/wire 28/28 (40.352 s), 25
orchestration plus seven cache guards, 14 frozen-vector tests and 374 isolated
constraints pass. Actual projection and endpoint/denial checks pass with SELinux
Enforcing, NNP and empty capabilities. After orderly shutdown, QEMU exit 0 and
verified PID absence, the same disk boots offline; projection reports
cross_boot_reopen=true and system_runtime_changed=true. Exact binding/retry bytes,
sealed-store/current activation checks and the separate retained allocation pass
again. Owned units are stopped. No physical power-loss/hardware acceptance.

Next: a fresh app DataEpoch and recoverable allocation journal, followed by run
admission, supervised dispatch and target-exec observation. Preserve the independent
retained private-storage fixture; never silently attach it to this installation.
Public binding reconciliation, production recovery and full removal remain work.

## Initial private-data enrollment

The fixed installed app now has a private DataEpoch and StorageRef. These are
fresh identities, with a fresh AllocationId and protected root, independent of the
retained allocation/read test fixture. Both remain tracked. Carrier machine/volume
references remain explicit P0 test identities, not authenticated production
volume registration.

The root controller first records the new identities and original metadata
baseline, refuses existing paths and provisions a distinct app-data anchor,
StorageRef root, owner header, empty payload and allocation registry. This remains
trusted preallocated P0 infrastructure. A confined native user-domain observation
captures its held/named inode, birth and filesystem witnesses. Root freezes those
bytes in protected configuration before native allocation publication. Unknown
or interrupted infrastructure is preserved for reconciliation, never adopted or
automatically cleaned up. This is not a general dynamic allocation journal.

The new paths inside the dedicated carrier are:

- users/AccountId/app-data/StorageRef: broker-owned root and owner header; only
  its payload/ child is the intended future app data view.
- users/AccountId/control/app-allocations/StorageRef: immutable allocation witness,
  separate from the old storage-test registry.
- registry/app-data-owner.conf and registry/app-data.conf: protected owner
  selection and complete independent enrollment evidence. They are retained test
  infrastructure, excluded from Git with the VM disk and controller receipts.

The internal AETHDTA1 configuration is exactly 368 bytes, all integers big-endian:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | AETHDTA1 |
| 8 | 104 | Expected AETHOWN1 machine/volume/account/storage/data/allocation tuple |
| 112 | 16 each | InstallId, publisher, system epoch, user epoch |
| 176 | 8 each | Expected install revision, initial user catalogue revision, data schema version |
| 200 | 168 | Independently expected AETHALC1 owner and persistent object witness |

The decoder requires UUIDv4 references, distinct ownership IDs, exact matching
owners in both encodings, positive signed-SQL revisions and a nonnegative schema
version. It preserves output on malformed bounds/bytes. app-data-owner.conf is
the independently selected owner used for the first native observation;
app-data.conf is the frozen complete enrollment configuration. Both are root-owned
0444 registry files; neither is an app-controlled request or public API.

The native --data-enroll administrative mode checks the original protected owner,
current root/header/payload/registry labels and account metadata. It uses no-follow
openat2, rejects payload mount transitions, checks 0700 account ownership, compares
the independently expected allocation witness, repeats required flushes and
publishes/opens the immutable create-only AETHALC1 record. Only then does its owner
SQLite writer enroll the data. No transaction spans filesystem effects. A lost
publication acknowledgement leaves a named same-intent allocation for explicit
data-check reconciliation; no replacement IDs or guessed allocation are allowed.

The native [catalogue API](../src/catalogue/data.h) checks both authority epochs,
account, exact active installation binding, publisher/revision and metadata schema.
For first insertion it additionally requires the original protected catalogue
baseline, current kernel boot and an initialized runtime. This conservative P0
baseline rejects intervening changes rather than silently refreshing intent.
One transaction inserts the active data row, closes its automatically created
data gate and attaches it to the original install. The insertion trigger's open
gate never commits: only the final closed gate at revision 2 becomes visible.
Catalogue revision advances once. No install gate, run, launch lease, grant or
operation changes, and the app gains no execution or document access.

An exact currently attached retry writes no metadata and preserves later gate
changes. A different schema, identity, attachment or binding, and detached,
retained or purged data return an error. Missing data after an advanced original
baseline cannot be recreated by an old request. This API returns current exact
enrollment, not a historical operation receipt; future prune/retirement must keep
the appropriate tombstones and configuration retirement. Generic filesystem
allocation, public data adoption and full recovery remain separate work. Do not
mislabel this all-SQL enrollment as a completed install or launch operation.

The controller uses systemd Type=exec one-shots in aetheri_user_t, NNP and empty
capabilities. They hold the normal catalogue writer lock, cannot register as the
live user broker and add no resident service. The payload uses fixed P0 categories
c33,c34; the unrelated storage probe retains c31,c32. These are test categories,
not a general allocator. The existing app probe with the same Unix UID cannot
open the new owner, payload, allocation record or protected configuration. Normal
public IPC still works; raw-system-bus and private-database denials remain.

Native tests use independently encoded Python configuration bytes and controlled
unconfined database fixtures. They cover all configuration truncations/extra byte,
identity/integer/owner errors, stale binding/baseline/boot, statement failures at
each mutation, failed/lost COMMIT acknowledgement, fresh-process exits before/after
real COMMIT, inherited-connection rejection and reopen/retry. Detachment/retention,
later gate opening and metadata pruning are controlled test states; production
retirement and removal APIs are not implemented by those tests.

The separate enforcing data-check exercises actual allocation and metadata with
the configured objects, exact retries and unchanged system catalogue/unrelated
storage witness. It asserts the closed data gate and absence of app dispatch.
Next: real run/code reservation, user readiness and supervised target-exec
evidence using this data identity. Admission must recheck current system authority;
the old cached install observation and a data attachment are insufficient.

Executed on 27 October 2026: complete native suites pass 26/26 Debug ASan/UBSan
(83.90 s) and Release (76.33 s). Final additional owner/epoch/retirement/overflow
cases then pass focused data repeats (4.13/3.02 s). Host migration/wire tests
28/28 (36.901 s), orchestration 26/26, cache guards 7/7, runtime vectors 14/14
and isolated constraints 374 assertions pass. Actual enforcing data enrollment,
exact repeat, projection, normal endpoint/app-denial and previous fixture checks
pass. Orderly shutdown yields QEMU exit 0 and verified PID absence; the same disk
boots offline and data-check reports cross_boot_reopen=true with the same data
identity, closed gate, unchanged old storage and no dispatch.

## Private code reservation and withdrawal

The original Runtime1 ReserveLaunch, GetDispatch and limited CloseLaunch now
execute in the system service. Only the currently registered broker's held
process, UID/context, active unit, invocation and cgroup can use them. Runtime1
XML remains unchanged at 1.1. The capability runtime.dispatch.v1 remains absent:
DispatchLaunch, normal user launch acceptance, data readiness and target dispatch
are not implemented by this step.

The wire profile is deliberately the protected original P0 I/G/D with action main,
headless session p0 and seat none. These constants are a test authorization profile,
not logind session/seat authorization or arbitrary app action selection. Root checks
the protected data/account/authority references, never opens the user database and
does not infer data readiness from those references. The registered broker identity
still supplies actual account/UID and current runtime; caller UUIDs are references.

One native owner transaction records an internal accepted canonical AETHINT1 launch
plan, relevant scope/install/gate reads, a generation-retention effect, a running
root operation, verified request step, reserved generation lease and reserved
dispatch. Root plan/operation IDs and timestamps are generated for new acceptance;
exact retry finds the original records regardless of new proposed IDs. The unit
name derives only from LaunchId. No executable/path/environment/policy argument
can select a process, and no supervisor call is made here.

AETHRSV1 is an internal request fingerprint: eight-byte tag, ten wire-order 16-byte
IDs, expected install revision as big-endian u64, then action/session/seat as
big-endian u32 byte length plus bytes. The ten IDs are system epoch, lease, launch,
broker epoch/incarnation, account, install, generation, data and boot. Its SHA-256
is immutable step evidence. Canonical AETHINT1 separately binds the root plan and
read/effect set. Positive signed-SQL revisions, UUIDv4 references and bounded
tokens are required. The independent Python encoder checks both digests.

New acceptance requires current root boot/runtime, active eligible account/UID,
correct account or machine install scope, exact active I/G and revision, verified
artifact metadata and open unowned gate. Reuse with changed context/identity is a
conflict; a lost COMMIT acknowledgement never permits another lease or root ID.
An existing reserved receipt can be acknowledged after its gate closes, without
opening it again. That receipt is historical acceptance, not current dispatch
permission. Root restart marks unfinished dispatch uncertain and keeps its holder;
retry/withdrawal cannot turn that obligation into a fresh reservation or release.

GetDispatch is scoped to the current eligible account and broker authority epoch.
A replacement broker incarnation can inspect its authority's old records for
recovery; it cannot rebind the immutable reservation tuple. Replies contain derived
unit name, state/revisions and optional actual invocation. Empty invocation means
there is no target observation; it is never interpreted as a successful exec.

CloseLaunch currently supports only withdrawal while dispatch and lease are both
reserved, the original boot still matches and no ready-data
or invocation evidence exists. One transaction freezes the caller authority/cause,
sets dispatch terminal, releases the lease and completes the root operation with
outcome cancelled and result LaunchId. Repeated identical cause returns that state
without rewriting the original completion timestamp. Different cause conflicts;
dispatching, observed or uncertain requires reconciliation. A current authenticated
replacement broker of the same authority/account may withdraw the old reserved
tuple without rebinding its original incarnation. This is not stop/drain
support for a process, and it never guesses from a missing bus name or timeout.

The explicit system --admit-fixture startup profile is test administration.
Root-protected registry/admission.conf is AETHADM1, exactly 96 bytes: tag, system
epoch (16), boot (16), original pre-start catalogue revision (u64), I (16), G (16),
install revision and expected closed-gate revision (u64 each), all numbers
big-endian. Native startup commits its new context, then requires exactly that
baseline increment, current boot/runtime and original fixture. It checks current
manifest/content/inventory, final labels/modes and independent store/container/
payload witnesses, with repeated flushes and no sealing mutation, before opening
only metadata reservation admission. It refuses holders/nonterminal dispatches.
Ordinary startup never uses this profile and closes prior open gates. Replaying
the old profile after runtime/catalogue advancement fails rather than refreshing
its intent. App executable mappings remain denied throughout.

The controller stores its fixed P0 test LaunchId/LeaseId/cause before IPC in
registry/reservation.conf: AETHRSF1 plus three 16-byte UUIDs (56 bytes). These
root-owned 0444 registry files and controller receipts stay inside the ignored VM,
not Git. Development1's additive minor 1.1 TestFixtureReservation uses the existing
root-selected, short-lived user-domain controller. It reserves twice, observes
reserved state, withdraws twice and observes terminal state. The driver reconstructs
canonical bytes from actual committed rows and verifies the released lease,
cancelled operation, frozen cause, unchanged user database and absent app unit.
This is a fixed system-reservation test with durable protected correlations; it
does not claim the normal user plan/run/reservation pipeline exists yet.

reservation-continue can retain a prepared intent after an observed terminal failed
startup only if no matching dispatch or held lease was accepted. It refreshes an
explicit admission attempt and preserves original launch/lease/cause. Unknown or
accepted work blocks automatic continuation. reservation-new-check requires the
previous tuple terminal/released and retains its historical receipt before making
new test identities. reservation-replay-check deliberately rejects the stale
profile and verifies closed admission with unchanged business history.

Native tests exercise canonical bytes, every reservation SQL mutation failure,
failed/lost COMMIT acknowledgement, fresh-process exits before/after real COMMIT,
ID retention, conflicts, scoped lookup, gate closure after acceptance, withdrawal
mutation/COMMIT faults, terminal retry and restart uncertainty with a held lease.
Independent sd-bus bodies cover all truncations, array lengths, integer/token
bounds, extra fields and output preservation. Those are separate from actual
enforcing broker RPC and wrong-unit/app denial evidence.

Next after user acceptance below: data readiness, code/data view preparation,
serialized DispatchLaunch and supervisor/target-exec evidence. Recovery of potentially
dispatched work and real stop/drain must precede any corresponding lease release.
General session policy, production volume/allocation trust and the original full
desktop/apps goal remain open.

Executed on 27 October 2026: native complete suites 28/28 Debug ASan/UBSan
(104.93 s) and Release (82.47 s); host migration/wire 28/28 (45.848 s),
orchestration 27/27, cache guards 7/7, frozen runtime vectors 14/14 and isolated
constraints 374 assertions pass. Actual enforcing broker reservation/retry/status/
withdrawal confirms the canonical digest independently, unchanged user DB and no
app unit. Ordinary and same-domain wrong-unit callers are rejected by all three
implemented Runtime1 handlers and the protected trigger. Stale-profile replay
fails closed. Orderly QEMU exit 0 and verified PID absence precede offline boot of
the same disk; replay then reports cross_boot_reopen=true and unchanged terminal
business history. Normal endpoint/app denials and original store/activation/data/
storage checks pass again. Owned units are stopped; no physical power-loss,
production launch readiness or actual Aetheri app execution is claimed.

## Durable user launch acceptance and withdrawal

27 October 2026. Internal [launch.h](../src/catalogue/launch.h) now uses the existing
user v2 tables. No SQL version, dependency, resident process or policy permission
is added. Public PrepareLaunch/ApplyPlan and DispatchLaunch remain unsupported;
GetProtocolInfo still reports 1.1 with no capabilities.

Before accepting a new fixed-fixture start, the user writer independently reopens
the private allocation, verifies the protected witness, ownership, payload mode
and MAC, and obtains a fresh authenticated system installation snapshot. Only
then may it open the exact closed data gate. This is metadata admission; it does
not prepare a namespace, map code, give an app storage access or declare readiness.

Acceptance atomically stores a canonical AETHINT1 plan, four reads (system install/
gate and user data/gate), a data-retention effect, running local operation, requested
LaunchId, proposed LeaseId and executing step 0 with the complete AETHRSV1 request
fingerprint. These are committed **before ReserveLaunch RPC**. The local actor/key,
plan/op, tuple, timestamps and fingerprint are immutable correlation. Changed
reuse conflicts; lost replies retain the original identities and obligation.

Catalogue1's additive minor 1.1 GetReservationReceipt is current account/broker-
authority scoped, with the same actual broker authentication as GetDispatch.
Its typed response includes current system epoch/runtime/boot, original root
plan/operation/intent digest, request fingerprint, original broker incarnation,
LaunchId/LeaseId, dispatch state/revisions, and optional paired close cause.
The withdrawn flag requires terminal dispatch, released lease, cancelled completed
root operation with result LaunchId, and no invocation or ready-data evidence.
These fields are read in one transaction. They are observations from an authenticated
source, not bearer permissions or a replacement for supervisor evidence.

The user acknowledges that actual root plan/op/digest in operation_links; it never
invents the remote plan before RPC. Step 0 becomes applied and the local run becomes
preparing with readiness unknown. A user runtime replacement marks unsettled runs
recovery_required in the startup transaction; existing restart rules close open
data gates. Original tuples, receipts and root leases survive. The replacement
does not resend or rebind the old request.

Withdrawal is deliberately limited to a root tuple still reserved without any
dispatch/readiness observation. The current eligible replacement broker can close
that exact lease using the user epoch/local operation as cause. Once the root
receipt proves withdrawal, the user transaction records the actual remote receipt
(including after a lost root reply), release step, start_failed/unavailable run,
completed link and cancelled local operation. It checks the stored fingerprint,
system epoch, LaunchId/LeaseId, install fence and exact cause. Local invocation,
readiness, grants or additional setup effects block this narrow reconciliation.
The owner snapshot checks those local conditions before any CloseLaunch RPC as
well as inside final reconciliation; a local setup effect must not release code
first and merely fail the later user commit.
It never changes the data gate. Terminal retries preserve completion time/revisions.
Root restart uncertainty remains a held obligation, not proof of withdrawal.

Development1 minor 1.2 adds protected PrepareFixtureLaunch/CancelFixtureLaunch
triggers using the existing selected short-lived controller unit. Root-owned 0444
registry/launch.conf is AETHLNF1, exactly 136 bytes: tag (8), local plan/op/actor/
retry key/LaunchId/LeaseId (six 16-byte UUIDv4 values), then data revision, expected
closed data-gate revision, accepted Unix milliseconds and expiry boot nanoseconds
(four big-endian u64 values within signed SQLite range). The driver persists all
correlations before IPC. No arbitrary app/action/path/session is accepted.

Guest controller launch-check prepares once, restarts only the user broker, verifies
the original root lease remains reserved, then cancels and repeats cancellation
with unchanged database hashes. launch-cancel reconciles that existing protected
attempt without creating IDs or reserving again; unknown supervisor dispatch still
blocks it, and root uncertainty refuses release. launch-reopen-check verifies
terminal business history and canonical plans after normal/cross-boot reopening.
launch-new-check requires the previous user/root attempt terminal/released,
preserves its protected identities in history, then provisions a distinct attempt.
The data enrollment regression preserves a later closed gate revision instead of
assuming the original revision 2. Both unrelated data and publication fixtures stay
owned and retained. Real code/data views, readiness, target exec and drain remain next.

## Native private code/data views

27 October 2026. [linux/view.h](../src/linux/view.h) implements the mount portion
of trusted setup. It is not yet invoked by the installed authorities: no lifecycle
capability, policy permission, launch readiness or actual app dispatch is added.
The installed service pipeline still stops at the pre-dispatch journal above.

Only a fresh, trusted, single-threaded setup process enters this API. It creates
a new mount namespace and makes propagation recursively private; later failure
requires that process to exit, never reuse a permanent authority or fall back to
an unisolated launch. Source/target handles are opened and independently admitted
inside that namespace. Catalogue intent and code/data holders must already exist.

The helper clones the exact held source directory with open_tree, without recursive
child mounts. It compares the full source identity before/after and the cloned
backing object, while requiring a distinct unique mount ID for the new view.
No source path is reinterpreted. CODE adds readonly/nodev/nosuid/nosymfollow and
requires executable mounts; DATA adds noexec/nodev/nosuid/nosymfollow and requires
writability. Existing noexec/readonly restrictions are never cleared. Runtime
library/loader views need their own explicit contract; this strict P0 code/data
profile does not silently permit symlinks needed by ordinary runtime libraries.

Attachment accepts only the corresponding app/data slot under a held protected
setup root. The exact empty root-owned 0700 target must match its expected identity
on the parent's mount; links, replaced targets, foreign mount crossings, source
aliases and unexpected contents fail. move_mount uses held source and target FDs.
Before the move syscall, the local view becomes uncertain. Failure, including a
lost success acknowledgement, cannot be repeated with the same attachment object.
Post-move checks require the expected cloned mount at the named slot before marking
attached. The caller still excludes concurrent privileged topology writers.

All view/namespace objects are process scoped and hold CLOEXEC descriptors; fork
or namespace changes reject later checks. Closing an object releases only its
local handles. It does not remove attached mounts, drain an application or release
a durable lease. The setup/app process and any descendant/FD/namespace holder must
actually disappear before its durable obligation can be reconciled. The supervised
no-exec integration below journals these effects and binds the setup invocation
separately from target dispatch/readiness.

Both authorities' withdrawal paths must account for journaled setup effects,
including missing replies. Simple root withdrawal now rejects additional setup
steps. Only the separate setup reconciliation may release this no-exec attempt.

[Native tests](../tests/test_view.c) run only inside the dedicated Linux VM.
The independent Python parent observes its untouched target directories while
the child mounts remain live. Real kernel checks reject code writes, data exec
and executable mappings, and device opens; symlinks are refused and foreign nested
mounts are omitted. These tests use administrative unconfined_t under global
SELinux Enforcing; the future launcher's MAC domain is not exercised here.
Fault cases include namespace/private-propagation failure, clone/attribute failure,
move failure before and after a real move, stale source/target identities, existing
restrictions, wrong slots, unexpected entries, fork/namespace misuse and descriptor
leaks. Closing all library objects demonstrably leaves the attached view present
until explicit test unmount/process exit. Failed fixtures are retained for inspection.

Primary API references inspected for this slice:
[open_tree](https://man7.org/linux/man-pages/man2/open_tree.2.html),
[mount_setattr](https://man7.org/linux/man-pages/man2/mount_setattr.2.html),
[move_mount](https://man7.org/linux/man-pages/man2/move_mount.2.html).
Their detached-mount/FD/attribute semantics are distinct from Aetheri's own ownership
and recovery rules. Runtime tests use the existing kernel/glibc pins; no new library.

## Supervised no-exec setup

The short-lived [aetheri-setup](../src/launcher/main.c) prepares code/data views for
one protected P0 attempt. It has no app-exec, shell, plugin, arbitrary-argument or
UID-changing path. It does not run the package's action or establish app readiness.
The root-only development controller still submits its fixed systemd unit; there
is no production starter or public launch capability. No new dependency or schema
is introduced. Private Setup1 is 1.0; Development1 is additive 1.3. Frozen Runtime1
and all SQL definitions remain unchanged.

The existing User operation/run holds DataEpoch D before Root setup is requested.
The user writer checks the exact attachment, allocation, current runtime, open
data gate and original read fences. The authenticated broker asks Root to commit
step 1 (stage launch L, executing, profile SHA-256) before the external unit request.
Root's simple reservation withdrawal now refuses any additional setup step.

The selected helper checks UID 0, Enforcing, NNP, exact setup_t:s0 and the two
effective/permitted capabilities SYS_ADMIN and DAC_READ_SEARCH. Allowed inherited
bits left by PID1 are dropped and checked again before registration. It hashes its
own protected image against the profile. RegisterSetup authenticates the actual
bus ProcessFD, UID/label, original boot and derived aetheri-app-L.service. Repeated
live observations bind its actual InvocationID, cgroup path/inode and unique bus
sender. Root holds that peer and a PID1 unit reference before committing step 2
(verify integration invocation, executing, canonical context digest). A failed or
lost SQL acknowledgement keeps this frame for exact reconciliation.

The helper then enters a fresh private mount namespace. It verifies the published
container witness, canonical manifest/ArtifactId and listed file hashes/modes/types,
plus the protected allocation/owner header and private payload directory. The
publication's writer exclusion remains a precondition; this is not a fresh general
tree inventory or dynamic adoption. Held sources are cloned and attached through
linux/view. Only after both views pass does ReportPrepared mark steps 1/2 applied.
The following READY=1 means *setup* is ready. The target dispatch remains reserved,
with NULL target InvocationRef and ready-data fence; user readiness stays unknown.

The helper links pure data/setup codecs, core/Linux libraries, libcrypto, libsystemd
and libselinux, without SQLite or a catalogue writer. It waits once on a signal FD
with a 30-second upper bound, then exits. There is no polling service or permanent
setup daemon. Timeout and connection loss do not prove drain. This first helper
retains its two privileges for trusted setup only; executing untrusted code with
them is impossible in this implementation. Privilege dropping and app exec require
a separate implementation and evidence contract.

WithdrawSetup requires the current registered broker of the same authority/account.
For a registered helper, Root first observes its held ProcessFD dead. Independently,
the retained same-invocation unit must be inactive/failed, MainPID 0, with empty
ControlGroup and no Job. The original invocation must still match. Root then
applies setup steps, writes step 3 (release L, profile digest), terminalizes the
dispatch, releases the code lease and completes the original operation cancelled
with reason setup_drained_without_exec. User finishes its original local operation
from the exact authenticated Root receipt. A replaced User broker may do this;
its closed data gate does not reopen. Exact terminal retries preserve database
bytes, revisions and completion timestamps. Root restart/boot uncertainty remains
held and cannot use this same-runtime shortcut.

### Failure before registration

A separate supervisor-only proof handles the fixed helper exiting with status 5
before a ProcessFD was registered. It never invents a held peer. Root checks the
unchanged protected helper digest, exact derived unit, Type=notify, root user/group,
mandatory SELinuxContext tuple, NNP, private mounts, exact capability bound,
Restart=no/NRestarts=0, empty auxiliary commands and exactly one fixed executable
and argv entry. It requires actual exited-PID/timestamps/status plus the independent
same-invocation drain above. Its evidence has a distinct AETHSTE1 tag. It records
setup_exit_observed before ordinary setup withdrawal. This proof depends on trusted
root provisioning preventing helper/unit replacement throughout the attempt; it
cannot authorize release after a target-executing starter, unknown job or Root
runtime change.

The offline system --recover-setup administrative action supports the same proof
under the exclusive catalogue writer lock without beginning a new runtime. It is
for this development fixture only. The controller independently stops the old
authorities, preserves the failed helper/profile, runs the confined cap-zero native
writer, verifies its durable terminal receipt and only then replaces owned inputs.
It never patches SQL, repeats setup dispatch or claims a live peer was observed.
Unknown state or contradictory observations preserve all obligations and files.

### Protected formats and policy

AETHSUP1 is exactly 320 bytes: tag 8; fourteen UUIDv4 values (Root epoch/runtime,
User epoch/original runtime, boot, account, install, generation, data, storage,
launch, code lease, Root operation, User operation); three positive signed-range
u64 revisions (install/install gate/data gate); ArtifactId 32; helper image SHA-256
32. Integers use big endian. Registry setup.conf is root-owned 0444, no links.
Decoded bytes grant no authority by themselves.

The live AETHSPC1 evidence is tag 8, profile hash 32, boot 16, invocation 16,
cgroup inode u64, UID u32, then unit/label/cgroup strings each prefixed by u16 byte
length. AETHSTE1 is tag 8, profile hash 32, invocation 16, four u64 ExecStart
timestamps (start real/monotonic, exit real/monotonic), exited PID/code/status u32.
All integers are big endian. These are private development formats, not public
stable token or authentication formats.

setup_t is separate from the cap-zero authority/app attribute. It may read selected
publication/registry/owner metadata, use mount namespace FDs, change private mount
topology, mount onto protected setup slots and notify PID1. It cannot execute/map
package code or app-data files. Its trusted owner-header code shares the existing
user_state_t read permission; MAC alone does not distinguish that header from a
user catalogue. It has no public path/SQL interface and opens no user catalogue.
Production type separation remains work. The system authority gains PID1's system
start permission because RefUnit uses that authorization class. This is
broader than status inspection and belongs to the trusted Root boundary; apps and
User receive none of it. Aetheri exposes no arbitrary unit-name management method.

Only the exact known empty root-owned scaffold directories are removed after
native terminal evidence; unknown entries/replaced objects are preserved. The
private payload, allocation and histories remain. Complete app removal, document
transport, target exec and production recovery are still pending. The old fixture
declares action run and c31,c32; enrolled app data uses c33,c34. Old immutable main
reservations cannot be reinterpreted into a successful real app start. A new,
compatible fixture/generation and target-execution contract are needed next.

## Native one-way execution and image observation

The internal [image](../src/linux/image.h) and [execute](../src/linux/execute.h)
primitives are separate from the installed no-exec helper. Only the dedicated
owned-child tests call execution today. No SQL/XML version, public capability,
package enrollment or resident service is added. The earlier helper's release
proof still applies only to that immutable no-exec implementation.

The trusted caller must already admit the immutable code/data views, account,
label, literal arguments, environment and exact stdio endpoints. It must journal
dispatch before any external execution request. These primitives do not perform
that journal or admission, build a complete root filesystem sandbox, enroll a
publisher, resolve shared runtimes or establish app readiness. They use the
existing carrier's trusted glibc loader/libraries. Writer exclusion and protection
against hostile ptrace/concurrent credential or image changes are prerequisites.

### Image profile

ae_image_verify borrows a readable regular FD and independently expected SHA-256.
It requires root UID/GID, mode 0555, one link, no file-capability xattr, birth time
and bounded size (64 bytes through 16 MiB). A limited native little-endian x86-64
ELF profile rejects foreign/script formats, malformed header/program-table bounds,
writable executable load segments and missing/executable GNU_STACK. An optional
single interpreter must be exactly /lib64/ld-linux-x86-64.so.2. Static ELF is also
allowed. This is a P0 execution profile, not a complete ELF verifier or promise
that every accepted file will load successfully.

Metadata before/after hashing must agree. The private in-memory image witness
contains device/inode, birth time, size/ownership/mode, change/modify times and
content hash. It is not serialized, a bearer token or a replacement for protected
manifest/artifact enrollment. Mount IDs may differ between the original and its
private clone; backing object identity must agree. Failed calls preserve output.

### Irreversible execution boundary

ae_execute_target accepts a fresh single-threaded trusted root process only,
global Enforcing and NoNewPrivileges already enabled. Code must be readonly,
nodev/nosuid and executable; private owner data must be writable, nodev/nosuid/
noexec and 0700. The bounded argv/environment allowlist never inherits environ
or invokes a shell. Stdio must be approved pipe/socket/null endpoints, not regular
privileged files. The caller is responsible for their exact identity and direction.

It rechecks the held image, selects and reads back the mandatory pending SELinux
exec label, and marks every FD from 3 upward CLOEXEC with close_range/UNSHARE.
Only approved stdio survives exec. Core dumps are disabled. Required setup
capabilities are SETUID, SETGID and SETPCAP; only SYS_ADMIN and DAC_READ_SEARCH
may additionally be present. It clears inheritable/ambient rights, locks root and
keep-caps behavior, empties the capability bounding set, clears supplementary
groups and sets every real/effective/saved UID/GID to the selected non-root owner.
After verifying empty capability sets it enters the held data directory as that
owner, resets signal dispositions/mask and calls execveat on the held image FD.
There is no return to privileged work, shell, alternate interpreter or fallback.

Any failure exits 125, even after successful irreversible effects with a lost
acknowledgement. The sole nonblocking CLOEXEC error-pipe writer can emit one
16-byte record: AETHXER1, phase u32, positive errno u32, integers big endian.
Phases 1..10 are profile, image, context, descriptors, capabilities, groups, GID,
UID, signals and exec call. Syscall errno is saved immediately before reporting.
SIGPIPE is blocked for the reporting write; a missing reader cannot turn failure
into continued work. A full/broken/empty channel provides no success evidence.
EOF plus exit 125 cannot distinguish a pre-exec exit from a real program returning
125. Never use it to declare readiness, release a lease or repeat dispatch.

### Current executable observation

ae_image_match_process takes an already authenticated/owned held ProcessFD. It
does not open a new pidfd from a caller-supplied PID. Kernel PID/TGID, all four
UID/GID fields, nonzero cgroup identity and liveness must agree. Through a checked
host procfs directory for that kernel PID it checks the raw MAC label and opens
the current exe magic link. This deliberately opens the kernel's backing image,
not the pathname returned by readlink. Its profile, SHA-256 and object witness
must match; a second image open, credential/cgroup/label check and liveness check
bound the observation. It is a snapshot, not an atomic history or guarantee that
the process remains alive/unchanged afterward. Unit/invocation/run admission and
the eventual ready-data fence remain separate integration requirements.

### Executed test boundary

linux.execute runs 23 owned-fixture scenarios: malformed image/profile checks,
real exec and exact-image observation, wrong identity/image, credential/descriptor/
signal faults, failures after bounding/securebits changes, precise ENOEXEC despite
later errno changes, pre-exec/payload exit 125, and an absent error reader before
fork. The manifest limit of eight literal arguments plus argv[0] executes intact;
a tenth argv entry fails with E2BIG. Malformed ELF bytes are verified as data,
never passed to kernel binfmt.
The inert payload checks all IDs, groups/capability sets, locked securebits, NNP,
core limit, label, exact environment, signal reset, FD inventory and private data
write/read/unlink. It also attempts a real executable mmap from noexec data.
The Debug harness uses ASan/UBSan. exec_payload deliberately does not: instrumentation
could add its own initial FDs and invalidate the inherited-FD inventory assertion.

The explicit MAC check is separate from CTest:

```text
python tools/run_vm.py ssh 'sudo python3 /home/aetheri/source/tools/test_execute_policy.py /home/aetheri/build/release/test_execute /home/aetheri/build/release/exec_payload'
```

It accepts only the registered enforcing UEFI development guest. Its temporary
p0-execute.cil defines separate test types; it changes no original authority,
setup or app domain. Five supervised cases execute across the real transition to
aetheri_exec_app_test_t:s0:c33,c34. The parent harness alone has SYS_PTRACE for
cross-UID observation and KILL for signalling its child; both are removed before
the execution primitive. An additional same-UID observer with empty capabilities
verifies the real executable without SYS_PTRACE. This is not a deployed User API.

The trusted test starter explicitly retains ambient SETUID because PID1's v259
seccomp setup otherwise drops it despite the bounding ceiling. The primitive
clears ambient/inheritable before the owner change. NNP and nosuid remain enabled;
the policy allows only the exact test source/target transition at both gates.
The target receives no setup capabilities and cannot fork, change execution
context, write code or execute/map its data files.

The controller records each fresh unit request before submission. Unknown dispatch
preserves pending state and forbids cleanup/retry. Known termination still needs
independent inactive/failed, MainPID 0, empty cgroup and no manager job. Only exact
known inventories/ownership/modes/hashes are removed; changed objects remain.
The module is removed only after all owned units drain and its installed hash
agrees. Exact drained failed transient units are reset, preserving journal/audit
evidence. A known failed/drained test can be retired with --cleanup after inspection;
an unacknowledged request cannot. No retained private data, catalogue history or
installed no-exec helper is modified by this test.

The compatible fixture described below now exists. The next integration needs
durable dispatch before execution, exact retained process/invocation observation,
actual app registration/readiness and richer drain/recovery than no-exec withdrawal.
Passing this primitive's tests does not satisfy those gates.

## Separate execution fixture and declared actions

The retained aetheri.p0.probe package is immutable and expects c31,c32; its old
main reservations remain historical. A separate aetheri.p0.exec installation now
uses a sealed copy of the inert exec payload. It accepts a manifest-relative
argv[0] and checks its actual context against a trusted starter-supplied label,
so it can test the planned c33,c34 execution profile. It is a development fixture,
not a desktop application or a registered run. Its action is run, executable
bin/probe, with no additional arguments. It also includes share/message.

The new fixture has fresh install/generation/slot/artifact and operation identities.
It follows the same native intent, stage, no-replace publication and first-activation
transactions, in the existing owner catalogue and shared content-addressed store.
It does not copy the old catalogue, relabel the old package, reinterpret old requests
or attach the old private data to a different installation. Its gate stays closed;
the existing live broker admission still selects the original protected profile.
Public launch and DispatchLaunch remain unsupported.

ae_fixture_registry accepts only two compiled administrative selections, original
and exec. Each selects fixed protected config/manifest/publication/activation paths.
The latter use registry/exec-fixture.conf, exec-fixture.manifest,
exec-fixture-store.conf and exec-fixture-activation.conf. The existing binary formats
and integrity checks are unchanged. The matching exec-admission.conf path is
reserved for later integration; no such file or admission is created here. Arbitrary
registry paths and profile claims from apps are not accepted. The store remains
store/sha256/ArtifactId; each preallocated fixtures/slot belongs to its own intent.

The root-only controller keeps the second fixture's receipts under exec_fixture
in its existing record. Shared identities, unit inventory, pending-dispatch guard
and stable carrier lock remain common. Every verification checks both fixture
trees. The controller rejects reused original plan/operation/request/install/
generation/slot identities and artifacts. It reuses the independently enrolled
store root and never recreates it for a second package. Existing cleanup refuses
either prepared installation. This is still preallocated P0 infrastructure, not
general package selection, dynamic allocation or a production update protocol.

Explicit guest controller actions (each takes the existing record/lock):

```text
python3 tools/p0_services.py exec-fixture-prepare
python3 tools/p0_services.py exec-fixture-check
python3 tools/p0_services.py exec-fixture-store-prepare
python3 tools/p0_services.py exec-fixture-store-check
python3 tools/p0_services.py exec-fixture-activation-prepare
python3 tools/p0_services.py exec-fixture-activation-check
```

Run as root only in the registered guest, with reviewed built inputs and stopped
owned authorities. System-domain administrative units use --exec-fixture-observe,
--exec-fixture-stage, --exec-fixture-store-observe, --exec-fixture-store-publish or
--exec-fixture-activate. These exit without becoming a protocol endpoint. Unknown
dispatch still preserves its original unit and pending request; no retry is inferred.
Completed package receipts are never refreshed into a new intent on retry.

ae_fixture_resolve_action is an internal metadata helper for future start admission.
It verifies immutable manifest bytes against an independently expected artifact,
finds the exact declared action and returns owned executable/argv/file-size/hash
metadata. argv[0] is the manifest path, followed by at most eight literal arguments;
empty arguments and UTF-8 text remain intact. No shell expansion, PATH search or
main-to-run fallback occurs. Undeclared actions return ENOENT; invalid action tokens
return EINVAL; failed calls preserve output. This helper does not authenticate a
publisher, open code, admit a run or prove execution. Its output still needs the
Linux image/view checks and the durable Root/User launch contract.

The new installation now has its own projected binding and private data as
described below. Next connect real run dispatch, process registration/image
evidence, readiness and drain. No executing starter can use no-exec withdrawal.

## Scoped execution-fixture projection and data

Private Development1 1.4 adds only RefreshExecFixtureInstall, no inputs and the
same aytt result as RefreshFixtureInstall. Existing methods keep their original
selection and signatures. Both use the same protected controller-unit admission,
actual bus ProcessFD/unit/invocation/boot checks, authenticated Root projection
and owner-scoped user writer. The caller cannot supply an install/path/profile.
An ordinary same-UID process, same-domain wrong unit or app-domain process cannot
use the new trigger. Public versions/capabilities, Setup1, Runtime1 and SQL remain
unchanged. The new user binding is a cached observation, not launch admission.

The two compiled registry selections now also contain owner/data config paths and
an exact raw data-object label. The original keeps app-data-owner.conf,
app-data.conf and c33,c34. Exec uses exec-app-data-owner.conf,
exec-app-data.conf and c35,c36. Both use aetheri_app_data_t at s0 with system_u and
object_r; complete raw label equality, UID/GID and 0700 are checked before allocation
publication or SQL enrollment. The independently protected selected fixture must
agree on account, install and publisher. Type-only or caller-supplied labels do
not authorize another category. These are fixed test profiles, not a dynamic MCS
allocator or a user-selectable policy. Earlier isolated execution tests at c33,c34
remain separate evidence; a real start of this second installation must use its
own c35,c36 data/process profile.

The new storage lifetime has fresh StorageRef, DataEpoch and allocation-operation
IDs. AETHOWN1/AETHALC1/AETHDTA1 and all existing encodings are unchanged. Machine,
volume and account remain intentionally shared. The ownership header's final ID
is the allocation operation; it is not a MAC-policy ID. The controller rejects
aliasing an existing private storage/data/allocation-operation tuple or attaching
the selected data to another fixture's install/publisher.

Shared user app-data and control/app-allocations parents must already be enrolled;
they are verified and reused, never adopted/recreated for the new leaf. Each
app-data/StorageRef contains its own owner header and private payload, and each
control/app-allocations/StorageRef stores its own immutable allocation record.
The controller tracks these under exec_fixture beside its package receipts.
First-data preparation now checks absence for the selected component/publisher/
account and installation, rather than demanding an empty entire user catalogue.
Retained/detached matching data still needs a separate protocol.

The native writer already scopes its transaction to the exact install/data/storage
tuple. The second enrollment reuses it: commit data, attachment and closed data
gate atomically, with the protected original baseline for a first insert. Exact
retry never refreshes identities, gate revisions, later state or catalogue bytes.
The check now observes only selected rows while independently retaining all
unrelated data, attachments, gates, bindings and launch/grant/operation rows.
No user SQL is read by Root and no Python controller writes business rows.

First-time preparation in the registered enforcing guest:

```text
sudo python3 tools/p0_services.py exec-projection-check
sudo python3 tools/p0_services.py exec-data-prepare
sudo python3 tools/p0_services.py exec-data-check
```

After preparation, repeat the checks, not prepare with replacement IDs. The
administrative native user actions --exec-data-observe and --exec-data-enroll
use the fixed exec registry selection and existing cap-zero/NNP user domain.
They do not become a broker, open a gate or execute an app. No new policy grant,
dependency or resident process is required.

Actual negative evidence: while all owned services were stopped and before first
enrollment, Root changed only the independently known new empty payload to c37,c38.
The native confined cap-zero user writer returned EACCES with no allocation record
or catalogue change. After independently observed unit/job/process drain and
checking the same payload inode, Root restored c35,c36. Normal enrollment and exact
retries then passed. This proves rejection at this metadata boundary; it does not
claim complete launched-app view isolation or production allocation recovery.

## Durable execution intent and arm

The internal [execution profile](../src/catalogue/execution.h) and owner APIs now
separate a potentially executing starter from the no-exec setup helper. They are
tested native catalogue code, not an installed starter or implemented DispatchLaunch
endpoint. No executable process, pipe, D-Bus message or systemd request is created
by these transactions. The public capability list remains empty.

AETHEXE1 is exactly 384 bytes. Integers use unsigned big-endian representation,
restricted to 1..INT64_MAX; IDs use the existing UUIDv4 representation. The codec
rejects another purpose, extra bytes, invalid IDs/revisions and a request digest
that does not match the exact run/p0/none reservation. Its whole canonical byte
sequence is hashed internally, rather than accepting a caller's profile hash.

| Offset | Bytes | Binding |
| --- | --- | --- |
| 0 | 8 | AETHEXE1 purpose/version |
| 8 | 224 | System epoch/runtime, broker epoch/runtime, boot, account, I/G/D, storage, launch, lease, Root operation, User operation |
| 232 | 24 | Install, install-gate and data-gate revisions |
| 256 | 32 | Artifact digest |
| 288 | 32 | Independently admitted trusted starter image digest |
| 320 | 32 | Canonical AETHRSV1 request digest |
| 352 | 32 | Independently verified immutable allocation-record digest |

These bindings are not capabilities. The future live authority still owes
protected profile provisioning, current authenticated broker/Root provenance,
verified manifest/action/code/data/allocation/raw MAC checks and a held process
plus independently checked supervisor invocation. A hash supplied by an app is
not such evidence. User authorization reads only its own catalogue: exact accepted
run/Root receipt, current user runtime, active binding/data/attachment, storage,
unchanged plan reads and open data gate, with no readiness or additional effects.
Root never reads that database. The live User must mediate revocation and supply
its current fence at arm; the Root SQL API cannot independently discover a change
in private User state.

The Root operation uses existing v2 journal vocabulary:

| Call | Durable change | External effect allowed after fresh successful commit |
| --- | --- | --- |
| REQUEST | Step 1: stage/generation/G, executing, whole-profile hash | Request the fixed supervised starter |
| BIND | Step 2: verify/integration/actual invocation, executing, observed-context digest | None |
| PREPARE | Steps 1 and 2 applied; dispatch/lease remain reserved | None |
| ARM | Step 3: activate/launch/L, executing; dispatching with frozen invocation/data fence; lease running | Release that same prepared starter to exec |

Step 1 deliberately targets generation rather than the old setup journal's launch.
Even passing the execution hash to the old no-exec withdrawal API cannot match
this journal. Ordinary pre-dispatch cancellation also rejects any of these extra
steps. An arm requires both preparation records, the original current broker/boot/
Root runtime, exact active I/G and unchanged open install gate. Root restart leaves
the dispatch uncertain and lease held; broker replacement cannot rebind the run.

The result's effect_required flag is one only for a freshly committed REQUEST or
ARM. Exact retries return zero and preserve revisions, including after a commit
that succeeded but whose acknowledgement was reported lost. Errors leave output
untouched. The caller may send the external effect only for the fresh result;
it must retain uncertainty after an error or lost outward reply. This chooses
at-most-once initiation with explicit reconciliation over silently repeating a
possibly successful request. It does not promise eventual start or exactly-once
execution. There is no release/completion API in this slice: an arm is neither
actual image observation nor app readiness, and it never permits reclaiming code.

`catalogue.execution` exercises independent Python canonical bytes/hash, all
mutating-statement failures with byte-identical database rollback, both commit
acknowledgement outcomes, real owner-process exits before/after REQUEST and ARM
commit, replay of earlier stages, changed evidence, closed gates, overflow, Root
restart at every stage, User replacement and rejection by no-exec withdrawal.
These isolated owner-store tests do not exercise a live executing starter. The
next integration must bind an authenticated error channel and actual target image,
then readiness and independently proven drain; EOF is still unknown.

## Issuer-owned execution error channel

[exec_channel](../src/linux/exec_channel.h) now creates an anonymous in-memory
pipe in the supervising process, which retains its reader and exports only the
NONBLOCK/CLOEXEC writer. The one-way execution boundary independently checks the
writer's anonymous pipe filesystem, UID/GID 0, mode and flags. Named FIFOs are
rejected. No filesystem path, persistent file, new service or polling process is
created for a channel. The caller owns one reader/object; constructor failures
publish neither endpoint and close their partial resources.

For live integration, Root must bind that instance to the admitted profile,
journal and original held starter ProcessFD/invocation before exporting the
writer. Use a reply to the authenticated selected starter; keep all inbound-FD
method-call denials in place. The primitive does not authenticate or register a
starter by itself. A future starter must authenticate the current Root reply and
its operation/runtime/profile, then validate and duplicate the borrowed writer
before freeing the message. The issuer must close its local writer after appending
it, including on send failure. The transport's temporary copy and the starter's
message copy also need to disappear. Only the trusted starter keeps a writer;
exec closes it and the target must never receive it. An unknown transfer result
does not permit a fresh channel or repeated arm.

This follows sd-bus v259's ownership contract: appending an h duplicates the
descriptor; reading an h borrows it from the message. The actual guest test uses
259.10 and a test interface on the existing user bus. The server pins the exact
child's bus ProcessFD/name/UID/label before issuing the writer, and the client uses
the independently supplied exact unique destination. The client duplicates the
message FD, frees the reply, writes two fragments and closes it. The issuer's
retained reader then receives the precise failure. This is actual transport/
descriptor evidence, not an installed Root execution interface or its MAC policy.

The existing AETHXER1 record remains exactly 16 bytes: eight-byte tag, phase
u32 and positive errno u32, big endian. Supported phases are PROFILE through CALL;
the numeric error slot accepts 1..4095. Each read call performs at most two
nonblocking reads, retaining at most 17 bytes in total, including one overflow byte.

| Reader state | Evidence and required treatment |
| --- | --- |
| PENDING | Writer still open, including a complete valid frame without EOF; no success/failure completion inferred |
| UNKNOWN | Empty EOF; no reported failure, no proof of execution/readiness/drain |
| FAILURE | Exactly one valid frame followed by EOF; preserve the admitted starter's phase/errno |
| MALFORMED | Invalid, truncated or extra bytes; retain uncertainty, never reinterpret/reset as a new attempt |

Terminal reports are stable and perform no further reads. Remove their event
watch so unread malformed data cannot cause repeated wakeups. I/O errors leave
the output unchanged and retain an already consumed prefix. No call blocks for
EOF, consumes an unbounded stream, stops a process or releases a code/data lease.
Closing a channel only closes this owned reader; external supervisor/process/
image/readiness/drain obligations remain separate. Each channel is process-owned;
inherited copies cannot use its API after fork.

The ordinary native execution tests now use this reader for the actual one-way
child, all reported fault phases and both exit-125 cases. An empty EOF is also
observed while the executed payload is alive; actual executable observation is
still required independently. linux.exec_channel additionally tests fragmented/
duplicate/truncated frames, retained writer copies, named FIFO rejection, FD
identity reuse, creator/flag changes, resource and I/O failures, inherited owner
rejection and descriptor balance. linux.peer tests the actual sd-bus writer reply.

The separate enforcing-domain probe passes with Release binaries. An initial
Debug run failed in LeakSanitizer: the audit record denied its same-domain ptrace
in aetheri_exec_setup_test_t. The known failed invocation was independently
drained and retired using --cleanup before the Release probe. No permission was
added to accommodate instrumentation. Keep ASan/UBSan in the normal native suite;
use Release for this explicit policy probe. Its c33,c34 test data/domain are
separate from the enrolled exec fixture's c35,c36. The new channel is not yet
wired into live authority registration/arm, app readiness or drain.

## Native execution coordinator and durable diagnostics

[authority/execution](../src/authority/execution.h) now connects the owner journal,
original starter ProcessFD/invocation and issuer-owned error channel. It is a
native Root-only library, not another resident service. It is not yet wired into
the installed authority's methods or a new starter binary. The live adapter still
owes protected profile/image/unit provisioning, fresh caller capture, current
broker/data admission, an authenticated writer reply and actual supervisor control.
The reserved starter context is system_u:system_r:aetheri_starter_t:s0; no policy
definition or permission for it is installed by this work.

The coordinator borrows the existing catalogue/system bus/event loop and copies
one admitted AETHEXE1 profile. REQUEST commits before its compiled supervisor
adapter runs. A retry or lost commit acknowledgement cannot invoke that adapter
again. An unknown supervisor reply does not prohibit the genuine original starter
from registering; it never permits another request. BIND requires the selected
UID/domain/boot/derived unit, observes the actual process via PID1, retains a
duplicate of the authenticated ProcessFD, references the unit and repeats the
observation before journaling. AETHEXC1 hashes the profile and observed boot,
invocation, cgroup identity, UID, unit, context and cgroup path. Only a successfully
bound original starter gets the newly created channel writer. An ambiguous bind
keeps its pins/reader and never issues a replacement writer.

PREPARE rechecks that same connection and kernel context. ARM additionally samples
the nonblocking channel immediately, so queued EOF/failure cannot be missed merely
because its event callback has not run. Any already consumed prefix also blocks
arm, even while the parser awaits EOF. The pristine query only describes observed
state; it is not a replacement for sampling the kernel. Context drift stays quarantined even if
later observations match again. Only a fresh ARM commit invokes the compiled
release adapter; lost acknowledgement or callback timeout never repeats it.
Neither adapter runs inside SQLite. Destroying the coordinator during one of
these callbacks returns EBUSY, protecting its borrowed/profile lifetime.

The event watch is disabled on terminal data or observation error. Its handler
retains the error rather than terminating the event loop, and the owned reader
closes only after its event-source reference is removed. Close releases local
pins/references, never a persistent lease, operation or process. Actual drain
still requires independent process, invocation, cgroup and job evidence.

Terminal observations now have a native owner transaction. Step 4 is
verify/integration/actual invocation, applied, with the AETHEXD1 digest. The same
commit records a canonical bounded operation reason and increments owner/operation
revisions. It changes no dispatch state, readiness, outcome or lease state.
The original REQUEST/profile must match. An existing BIND must match its context;
a missing BIND is allowed only for the same reserved original request with no
arm, so an ambiguous binding commit can retain diagnostic evidence safely.

| Kind | Phase/error fields | Canonical operation reason |
| --- | --- | --- |
| 1: unknown | 0 / 0 | exec_channel_unknown |
| 2: reported failure | phase 1..10 / errno 1..4095 | exec_failure_pN_eM |
| 3: malformed | 0 / 0 | exec_channel_malformed |
| 4: observer failure | 0 / errno 1..4095 | exec_observer_eM |

AETHEXD1 hashes exactly 100 bytes: its eight-byte tag, 32-byte whole-profile hash,
16-byte actual invocation, 32-byte observed-context hash, then kind/phase/error as
three big-endian u32 values. Exact retry is read-only/idempotent; a changed report
conflicts. Root restart retains the stored reason/evidence and the unsettled
lease; an old-runtime reporter cannot write again. A diagnostic acknowledgement
may be retried explicitly without any process effect. Storage/acknowledgement
failure remains visible separately from the observed starter/observer result.

Status stage flags describe this coordinator's acknowledgements, not app execution.
A lost commit may advance the persisted dispatch while a local flag remains false.
Request/release/context error details are currently local status; their persistent
intent/binding/arm and held lease remain the recovery obligation. Only terminal
channel/observer diagnostics use step 4 here. This is not complete control-plane
error history or production recovery.

`authority.execution_service` exercises real SQLite transactions, owned-child
ProcessFDs, pipes and sd-event dispatch with controlled process/PID1 adapters.
The external request/release adapters deliberately simulate supervisor effects;
this is not a live unit/domain admission test. Cases include repeated/unknown
effects, wrong peers/contexts, context drift, Root/broker replacement, EOF before
event dispatch, commit losses, diagnostic retry and non-release on close/error.
Even unread malformed data produces no further event dispatch after disable.
The actual child remains alive after coordinator close until the harness ends it.
The separate native diagnostic tests use independent Python bytes/hash/reasons,
all write/commit faults and restart checks. Live wire adapters, selected starter,
actual target registration/readiness and independently proven drain remain next.

The peer transport fixture now waits for actual terminal channel evidence under
a single five-second monotonic deadline. A sender's exit is not synchronous
retirement of every temporary transport FD copy, and a bounded parser need not
finish in one call. This changes the test's observation timing, not the reader's
unknown/pending semantics or any production admission timeout.

## Protected execution profile and source opening

`authority/execution_profile.h` now implements read-only filesystem checks for
the fixed executing P0 fixture. The root anchor is `/var/lib/aetheri-p0`; callers
cannot select registry paths, action, UID or categories. This is a starter
prerequisite, not launch admission.

`ae_execution_profile_load` reads canonical AETHEXE1 from reserved
`registry/execution.conf`. Root/registry directories require exact ownership,
mode and raw SELinux context; the file is root:root, 0444, singly linked and
exactly 384 bytes. `ae_execution_starter_open` similarly reserves
`execution-starter`, requiring root:root 0555, full `aetheri_starter_exec_t:s0`
object context and an ELF matching the profile's starter digest. Neither file
nor that starter type is installed. Tests reject absence and a binary labeled
for the different setup purpose; positive starter admission remains work.

`ae_execution_sources_open` uses only the compiled EXEC registry selection:

- Decode protected AETHFIC1; match account/I/G/artifact and hash the complete config
  against AETHSTO1. Compare independent store-parent/container filesystem, inode
  and birth witnesses. The shared store parent is root:root 0700
  `aetheri_store_parent_t:s0`; sealed containers/code directories are 0555
  `aetheri_store_dir_t:s0`.
- Verify the sealed manifest/artifact, fixed `aetheri.p0.exec` component and
  selected publisher. Resolve the exact declared `run` action into owned literal
  arguments, including empty ones. No `main` alias, shell or PATH fallback.
- Check the complete inventory, file contents and repeated metadata through new
  `ae_fixture_check_payload`. This reuses import validation without writes, chmod
  or fsync; publication still repeats its required flushes. Normal read-atime
  updates remain possible. Require each declared file and directory prefix's
  complete raw label, UID/GID and exact mode. Hold and verify the resolved ELF
  under that same code root.
- Match selected AETHDTA1's account/I/D/storage, both authority epochs, publisher
  and install revision. Require its AETHALC1 digest, actual allocation-registry
  bytes, independently protected owner config, actual owner header and anchor/
  root object witnesses to agree. The payload requires UID/GID 1000, 0700 and
  exactly `system_u:object_r:aetheri_app_data_t:s0:c35,c36`.

Success returns three distinct owned CLOEXEC FDs: code root, private payload and
image. Errors leave output unchanged. Clear only closes local resources; it
cannot settle operations, revoke access or release leases. Config/code/private
metadata resolution forbids links, magic links and mount crossings under held
roots. Only the selected payload leaf allows a supervisor-provided writable bind.
Trusted namespace topology and policy excluding future mutation/relabel/rename
remain caller obligations. These observations do not enroll publishers/volumes,
prove active I/G/D or current broker admission. The caller still owes current
catalogue/User fences, private-view creation/checks and final held-image recheck
at one-way exec. Source data is not yet a noexec mount. No gates/SQL/processes
are changed by the loader.

`fixture_profile.c` shares the existing compiled registry selections and unchanged
pure AETHFIC1/AETHSTO1/AETHACT1 decoders, avoiding administrative staging/publication
effects in a future starter. No byte/schema/wire version, dependency family or
resident service changed; installed protected binaries are unchanged.

`authority.execution_profile` exercises 35 owned enforcing-guest fixtures with
real openat2/statx/inventory/image and file-context checks. Only the fixed initial
root pathname is mapped to scratch space; child resolution/MAC are not mocked.
Repeated successes/failures verify unchanged outputs, FD balance and tree
metadata/content. Cases cover wrong generation/publication/component/action/ELF,
extra entries, ownership/labels/categories, links, data headers and allocation
record/witness/digest. Import tests also assert zero file/directory syncs on the
read-only path.

The optional existing-installation observation is:

```sh
sudo python3 tools/test_execution_profile.py --live-read-only \
    /home/aetheri/build/release/test_execution_profile /home/aetheri/build/release
```

It locks the carrier, verifies protected inputs, requires closed gates and no
unsettled dispatch/lease, and writes only a build-local observation profile with
synthetic request IDs. Eight checks open the actual retained code/private data
without root mapping and preserve catalogues/protected inputs. This is Root
administrative read-side evidence, not admission in the selected starter domain.
Live authenticated replies/wire/supervisor adapters, starter provisioning/policy,
target registration/readiness and independent drain remain work.

## Authenticated executing-starter client

`launcher/execution.[ch]` now implements the starter side of private
[Execution1 1.0](api/execution-v1.xml). The installed authority does not expose
that interface yet. The actual executing starter binary, protected Root unit/
profile selection and production handlers/adapters/policy still need integration.
The existing setup-only helper remains unchanged and cannot execute app code.

The caller is a fresh trusted single-threaded UID/GID-0 starter with the trusted
host system bus, independently protected AETHEXE1 and selected current Root unit.
It cannot take those values from an app. Constructor discovery resolves System1
once through the trusted bus, addresses that unique owner and retains its actual
ProcessFD. It requires UID 0, complete `system_u:system_r:aetheri_system_t:s0`,
the selected active service unit/invocation/cgroup/boot, protocol major 1/minor at
least 1, and the profile's Root epoch/runtime. Discovery capabilities are bounded
to 16 strings of at most 128 bytes. No subsequent call follows a replacement owner.

`ae_peer_capture_reply` first correlates an actual return to its retained sent
request: same bus connection, unique destination/sender, nonzero matching call/
reply cookies, reply-expected method call and METHOD_RETURN. It preserves the
method-call capture's bus-authenticated UID/PID/label/ProcessFD requirements and
rejects missing/augmented credentials. It does not consume the body. Both capture
APIs now reject an already occupied output rather than overwrite a held pin.
Correlation alone does not establish service purpose or current process context.

The client then rechecks the original Root pin and current supervisor context on
each exchange. Peer/bus/name/PID/UID/full label must remain the same. Observed
errors are retained; constructor failure leaves no client, and each operational
method can be attempted only once. Close frees only local resources, not any
dispatch, process or persistent lease.
This is a per-client guard, not durable duplicate prevention. Never reconstruct
a client to retry an unknown outcome; issuer journal/profile checks remain
authoritative across client objects/processes and Root replacement.

Both methods send the exact 16-byte lease and 32-byte whole-profile digest.
`RegisterStarter` returns Root epoch/runtime/boot, bound lease, root operation,
bound profile digest, dispatch revision and one anonymous error writer. The
client compares all fields, requires a signed-SQL-range positive revision,
validates Root:Root ownership, pipe identity/mode and NONBLOCK/CLOEXEC flags,
duplicates the borrowed writer before unrefing its message, and returns sole
local ownership to its caller. The client object keeps no writer copy; closing
it does not close a transferred writer. Root keeps the reader.

After independently verified mount/image preparation, `ReportPrepared` sends
the same input tuple. The future producer must retain that original request and
reply only after fresh durable ARM. Its receipt repeats the same six fields and
has a revision strictly newer than the bound revision. No FD is returned. The
client accepts only the exact signature/fields/current original Root context.
Registration waits at most two seconds and prepared release five seconds.
Timeout, malformed/error reply or lost connection never permits retry or exec.
An authenticated release is still not app execution, registration or readiness.

`embed_api.py --execution-client` generates names/signatures for client and test
producer from XML into the ignored build header. It does not add Execution1 to
the default authority method tables. Existing SQL/public/private contracts stay
unchanged; no new dependency family or resident process is introduced.

The explicit `test_execution_client.py` probe uses a protected Release test
producer in the actual existing Root domain under a recorded notify service,
NNP, empty capabilities and a one-task limit. It serves synthetic replies only:
no catalogue, launcher, views, target exec or app registration. An optional Debug
client runs outside that domain with ASan/UBSan; no LeakSanitizer ptrace grant is
added to the producer. Its 28 cases verify actual process/unit/invocation/cgroup,
binding fields, revisions, unsuitable descriptors, errors/timeouts, non-repeated
effects, writer ownership/FD balance, fork ownership, non-Root denial and original
Root departure. They prove consumer-side authentication and protocol handling,
not the future producer's journal/admission implementation.

The probe temporarily permits Root's anonymous pipes and the actual bus daemon's
write access needed to receive the writer through SCM_RIGHTS. It grants the bus
no read access to Root's reader. An initial run failed because that daemon write
permission was missing; an actual system_dbusd_t-to-aetheri_system_t fifo_file
write AVC identified the cause. Failed owned fixtures were independently drained
and retired before the corrected run. No permissive mode or broad policy grant
was used. The temporary module/files are removed only after original-process,
unit/cgroup/job drain and owned-input checks. Unknown dispatch preserves state.

Run explicitly in the registered carrier, with normal System1 unowned:

```sh
sudo python3 tools/test_execution_client.py /home/aetheri/build/release/probe_execution_client
sudo python3 tools/test_execution_client.py /home/aetheri/build/release/probe_execution_client /home/aetheri/build/debug/probe_execution_client
```

## Journal-backed execution endpoint

`authority/execution_wire.[ch]` now supplies the native producer component for
private Execution1 1.0. It owns one execution coordinator, the method slots and
at most one deferred prepared request for one independently admitted P0 profile.
The caller supplies a compiled request adapter and borrows its owner catalogue,
trusted bus and event loop. The component acquires no bus name and provisions no
unit, image, policy or profile. It is not linked into the installed authority.
Current broker/private-data admission and protected source admission remain
caller obligations; no public launch capability is advertised.

Both handlers require an actual reply-expected call on the selected bus, exact
generated signatures and the fixed lease/profile digest. Fresh authenticated
peer capture feeds the coordinator's original starter/process/unit/context checks.
Registration issues its writer only after a successful native BIND. The return
uses the actual journal revision and full binding, and closes the issuer's local
writer after the outgoing message has taken its own copy. Root retains only the
reader. A construction/send error after BIND is retained; the handler returns
handled without asking sd-bus to attempt a second automatic response.

Successful PREPARE retains a reference to the original method call and returns
handled without replying. Only a fresh native ARM can consume that call and send
its advanced-revision release. The producer owns this release callback; callers
cannot replace it with an arbitrary transport effect. The pending call is consumed
before reply allocation/send, including a send whose acknowledgement is lost.
No duplicate or late release is attempted.

One monotonic four-second timer bounds the local prepared wait inside the client's
five-second timeout. ARM samples fresh time before its transaction and again in
the release callback after commit. This also covers a slow commit crossing the
deadline. Expiration consumes the request, detaches the timer and attempts one
NeedsRecovery error. It never rolls back an ARM, completes the operation, frees
its lease or proves drain. A post-commit deadline/send failure can therefore leave
a dispatching/running obligation with no confirmed release. Preserve and reconcile
that uncertainty. Endpoint close detaches local resources only and refuses during
its active callbacks; it does not stop the starter or settle durable state.

`authority.execution_endpoint` exercises the producer with real isolated owner
catalogues, kernel process references, anonymous pipe/descriptor transport and
correlated deferred replies. Actor labels and PID1 context/ref observations use
explicit controlled adapters; these tests do not prove actual starter-domain or
installed-service admission. The owned child stays alive after endpoint close,
and the test verifies that its lease remains held. Cases cover wrong peer/tuple,
lost BIND/PREPARE/ARM commits, pre-send and post-send errors, timer and sampled
deadline expiry, context drift, broker replacement and Root runtime replacement.

The test harness starts the carrier's existing dbus-broker 37 on one private
socket under its mode-0700 scratch directory. Its configuration allows only Root
and contains no activation entries or global policy includes. Socket activation
passes a single inherited FD with the launcher's actual LISTEN_PID. The launcher
uses system scope for its own normal PID1 subscription, while tested calls use
only the private session-type bus. The owned launcher/broker group is terminated
afterward. No system-bus rules, protected service inputs, gates or carrier
catalogues are changed. No dbus-run-session package is required.

Actual protected Root/starter provisioning, live request/supervisor adapters,
target exec/registration/readiness and independent executing drain/recovery remain
integration work. The older non-executing helper/proofs remain separate.

## Separate executing starter and witnessed mount scaffold

`aetheri-execution-starter` is a distinct executable built from
`launcher/starter.c`, with `launcher/preparation.[ch]` supplying its read-only
self/source checks and private views. It accepts no arguments or caller-selected
paths, UID, action, environment, categories or Root unit. The compiled P0 Root
unit is `aetheri-p0-execution-root.service`; actual protected provisioning of that
unit/profile/image remains work. Neither this binary nor its missing starter
domain is installed by the normal controller or the component tests.

Entry requires real/effective UID/GID0, one thread, Enforcing, NNP and exactly
SYS_ADMIN, DAC_READ_SEARCH, SETUID, SETGID and SETPCAP effective/permitted, with
only those allowed inheritable bits. It clears inherited/ambient bits and
rechecks capabilities. The complete current label must be
`system_u:system_r:aetheri_starter_t:s0`. It loads the protected AETHEXE1 and
compares its actual current ELF object/digest to the independently protected
starter before making RPCs. No capability/domain is added by this entry check.

The flow opens only the host-local system bus, negotiates descriptors before
start, and uses the existing original-Root authenticated client. Registration
must succeed before namespace/view setup. Views are checked before ReportPrepared;
its original correlated release must succeed before a second target recheck and
the one-way execute boundary. The starter closes its client/bus locally after
that exchange; departure is not target drain. There is no retry, shell expansion,
PATH lookup, interpreter fallback or pre-exec READY notification.

The fixed inert EXEC fixture receives manifest-owned literal argv and exactly
LANG=C.UTF-8, its c35,c36 expected-context value and AETHERI_SCENARIO=p0. No host
environment, HOME, PATH, loader, bus or supervisor-notify variables are inherited.
The target is UID/GID1000 in `aetheri_exec_app_t:s0:c35,c36`. This separate
executing-fixture domain preserves the old non-executing app domain's denials.
General application
environment/account/category policy remains separate work. Existing one-way exec
seals descriptors and drops groups/capabilities/credentials; the borrowed target
handles remain alive through that call. A pre-exec failure after writer issuance
uses the same bounded AETHXER1 PROFILE/error frame and exit125. Invalid internal
report phase/errno normalizes to PROFILE/EINVAL. No writer means no report; neither
that absence nor exit125 proves no target execution or permits lease release.

The scaffold is a temporary runtime workspace, distinct from private DataEpoch/
StorageRef allocations. A protected `registry/execution-views.conf` contains the
184-byte AETHVWS1 record:

| Offset | Field |
| --- | --- |
| 0 | Eight bytes AETHVWS1 |
| 8 | SHA-256 of the complete canonical AETHEXE1, 32 bytes |
| 40 | Independently recorded launches parent object, 36 bytes |
| 76 | Exact LaunchId workspace object, 36 bytes |
| 112 | Empty app attachment directory object, 36 bytes |
| 148 | Empty data attachment directory object, 36 bytes |

Each object is filesystem type u64, filesystem ID eight raw bytes, inode u64,
birth seconds u64 and birth nanoseconds u32, all integers big-endian. Type/inode/
seconds fit signed SQL range, inode is positive and nanoseconds below 1e9.
These are bounded object witnesses, not volume/clone authentication or authority.
The frozen [vector](vectors/scaffold-v1.json) and reference `execution_views`
encoder cover this separate format without changing older vectors/SQL/XML.

Root must independently create/admit/flush and journal these exact objects before
dispatch, then publish the protected profile-bound record. That provisioning/
operation journal is not implemented here. Never capture/adopt a candidate from
its matching name or its own record during recovery. The reader requires the
Root-owned fixed anchor/registry, a single-link 0444 registry file and 0700 Root-
owned aetheri_setup_root_t directories under launches/LaunchId. Symlink and child
mount crossings are rejected while joining the four independently witnessed
objects; directory FDs stay held through attachment. Binding to the full profile
also distinguishes Root/boot/run/image replacement. Existing no-exec scaffolds
and their records cannot substitute for AETHVWS1.

After entering a fresh private mount namespace, preparation reopens the already
reviewed EXEC source inventory/private allocation there. It clones/attaches exact
readonly executable code and writable noexec private data, then opens the image
through that attached code view. The image must match the original admitted
backing object/digest. Rechecks join held and named app/data mounts, exact data
context and image; a replacement mount cannot select another tree. Closing view
objects only closes handles. Failure can already have changed namespace/mounts:
exit the process, never reuse it or attempt fallback/rollback. The caller still
owes current broker/data fences and exclusion of content/topology mutation.

Native profile tests now also exercise actual private mounts with independently
recorded scaffold witnesses, including recreated directories, metadata/record
errors, nonempty slots, fork ownership, unchanged parent namespace and a later
replacement mount. Separate starter flow tests inject component faults to verify
ordering, original release gating, exact argv/environment and actual error frames.
Those flow adapters do not authenticate a live starter/Root or execute a target.
The separate explicit integration below now exercises real confined starter/Root
admission, supervisor request and target image observation with isolated inputs.
Normal provisioning/current broker-data admission, production registration/
readiness and executing recovery remain required. The old setup helper has no
execution path and remains unchanged; its no-exec withdrawal proof never applies
to this new executable.

## Isolated live Root/starter/execution test

`tools/test_execution_runtime.py` is an explicit root-only test on the registered
disposable enforcing UEFI carrier. It is outside CTest/default installation. Build
`probe_execution_runtime`, `aetheri-execution-starter`, `exec_payload` and
`test_execution_profile` with AETHERI_POLICY_PROBES enabled, then run:

```sh
sudo -n python3 tools/test_execution_runtime.py \
  /home/aetheri/build/release/probe_execution_runtime \
  /home/aetheri/build/release/aetheri-execution-starter \
  /home/aetheri/build/release/exec_payload \
  /home/aetheri/build/release/test_execution_profile
```

Release binaries keep instrumentation permissions outside the confined services.
No package/dependency family was added. The inert test payload now links the
already selected libsystemd for its fresh post-exec connection; the immutable
normal package is not rebuilt or replaced by this test.

The controller holds the carrier lock, verifies normal protected inputs and
requires normal gates closed, no pending request, no unsettled normal dispatch/
lease and no System1 owner. It refuses existing test roots, modules, configuration
or unit slots. Its separate `/var/lib/aetheri-p0-execution-runtime-test` contains
separate system/User catalogues, random authority/runtime epochs, actual boot ID,
trusted package, independent private allocation and profile-bound scaffold. The
Root, User and starter units privately bind that root at the fixed anchor; normal
protected files/catalogues are never selected. Package/allocation/scaffold setup
remains controlled administrative input. Actual broker registration and User
projection/enrollment/data admission/preparation now run in separate confined
processes; see the connected-owner boundary below. The fixed UID1000 User is a
test system-scope service, not production login-manager/session provisioning.

| Part | Actual evidence |
| --- | --- |
| Root | Confined cap-zero/NNP process, native catalogue/runtime/reservation and Execution1 endpoint under the fixed Root unit |
| User | Actual UID/GID1000 broker, its own native catalogue/runtime, authenticated Root replies, independent private-allocation checks and durable execution preparation |
| REQUEST | Fresh native commit calls real PID1 StartUnit for the journal-selected unit; caller supplies no unit/path |
| BIND/PREPARE/ARM | Original starter ProcessFD, current service/invocation/cgroup/context, actual writer reply and retained original prepared release |
| Starter | Unmodified production starter binary, protected current ELF/profile, private witnessed code/data mounts and one-way credential/descriptor drop |
| Target | Fresh UID/GID1000 `aetheri_exec_app_t:s0:c35,c36` connection, same still-held process pin and original invocation/cgroup, actual running ELF matching manifest Artifact |
| Retirement | Administrative exact app pidfd termination/death, original User/Root pidfd death and independent unit/cgroup/job drain, then full owned-tree/static-input checks |

The private P0ExecutionTest1, P0ExecutionBrokerTest1 and P0ExecutionUserTest1 methods
are compiled only in `tests/probe_execution_runtime.c`. They are not installed API
contracts, general registration or public capabilities. The subsequent fixed-fixture
User registration/readiness handshake is described below. The target's fresh
connection supplies actual current bus credentials after exec; no old starter
UID/label snapshot is rewritten. Root retains its original starter pin and checks
the new connection against that process and unit invocation. A correlated Root
reply forwards the actual app ProcessFD to the administrative driver. That driver
rechecks original Root identity, tuple and current app context, then performs the
cross-UID current-image check and pidfd termination. Neither Root nor starter
receives SYS_PTRACE or KILL. The resulting process termination is test retirement,
not a production removal/recovery protocol.

Temporary `policy/p0-execution-runtime.cil` defines the exact five-capability
starter and a separate MCS-constrained executing-fixture app domain. The old
`aetheri_app_t` deliberately denies store execution and Root messaging; weakening
it would merge incompatible fixtures. Changing the compiled target label also
changes the starter digest bound by AETHEXE1. The new domain cannot execute/map
private data, inherit authority descriptors, read owner metadata or add privileges.
The target uses the fixed User Register/Ready handshake and Root Observe; its
attempted privileged broker Resolve is rejected. Root, User and starter query a dedicated test unit-file type,
with Root start access on that type. User can observe Root's current process
metadata for authenticated reply checks; neither receives SYS_PTRACE/KILL. The daemon
can write the issued Root pipe for descriptor transfer, never read its reader.
The test adds no grants to the normal app domain. Enforcing remains enabled.

Controller acknowledgement is saved before each external start. A timeout/lost
reply preserves the fixture, policy and pending intent; rerunning never redispatches.
Successful retirement requires exact owned inputs and independently observed
process/unit/cgroup/job drain. Unknown files or changed objects stop cleanup.
`--retire` only retries an already passed, non-pending test; it cannot settle an
uncertain execution. Interrupted administrative teardown requires inspection.
Object tree witnesses and policy/config/unit digests are local test evidence,
not clone authentication or a complete power-loss journal.

Both native catalogues retain their original run histories after the app exits.
The later launch-result completion boundary below now acknowledges both start
operations, while Root's running lease remains. REQUEST/BIND/PREPARE/ARM,
diagnostic/observation/completion and User's preparation/registration/readiness/
acknowledgement steps are archived with `historical-test-catalogue.sqlite3`,
`historical-user-catalogue.sqlite3` and `report.json` in a protected
`live-execution-<token>` directory beside the driver binary. Channel UNKNOWN/EOF
does not establish execution or drain; only the independent image observation
establishes what this test actually executed. No no-exec withdrawal/release is used.

Actual evidence covers this one fixed inert action/account on the pinned carrier.
Normal Root/User provisioning, general admission and mutation fences, dynamic
category/scaffold allocation, production target registration/readiness, executing
recovery/lease settlement and complete removal remain work. Both normal install/
data gates remain closed; DispatchLaunch/public capabilities stay unchanged.

## User execution preparation journal

`ae_catalogue_prepare_user_execution` in `catalogue/execution.h` is the User-owner
intent boundary before the first executing Root request. A read-only data check
does not record whether that request might subsequently have been sent. This
transaction binds the complete canonical AETHEXE1 to the already accepted User
operation, so a lost reply/restart cannot erase that obligation.

The caller first authenticates current Root/broker provenance and a still-reserved
original Root receipt, and independently verifies private allocation/payload MAC
and object identity. Those are caller obligations; this function does not read
Root's catalogue or observe a process, filesystem or IPC reply. The trusted User
writer must still exclude concurrent owner mutation around those observations.
Profile shape/digest is not proof of their provenance.

Inside one BEGIN IMMEDIATE transaction it checks current User epoch/runtime/boot/
account, exact accepted LaunchId/lease/install/generation/DataEpoch, run/p0/none,
original Root link and request fingerprint, active binding/attached data and the
original install/data read revisions. The data gate must remain open at the exact
revision. Readiness is unknown, invocation absent and the operation still running.
Grants or additional journal steps reject preparation.

| Record | Fresh mutation |
| --- | --- |
| operation_steps | Step1 stage/data/exact DataEpoch/executing, evidence is the full AETHEXE1 digest |
| launches | Increment revision; keep preparing/unknown with no invocation |
| operations | Set exec_preparing and increment revision; leave outcome absent |
| authority_meta | Increment catalogue revision |

There is no filesystem, bus or supervisor effect in the transaction. It reuses
the current v2 schema and changes no frozen SQL/XML/profile bytes. Overflow of
any incremented revision rejects before mutation. Output includes the current
User launch snapshot and `submit_required`, one only after a successful fresh
commit. On error the complete output remains untouched. A lost COMMIT reply can
leave the step durable without returning permission to submit.

An exact retry under the same current fences returns unchanged history/revisions
and submit_required=0. It never permits another external submission or converts
an unknown result into rollback. A different starter/allocation/Artifact/Root
runtime changes the bound profile and conflicts with the existing step. Owner
runtime/gate/data drift fails instead of refreshing old admission. The existing
read-only `ae_catalogue_authorize_user_execution` remains a pre-preparation check;
it rejects a prepared operation rather than interpreting the step as fresh rights.

The new step makes the existing User snapshot's pre_dispatch_withdrawable false
and blocks `ae_catalogue_finish_withdrawn_operation`, even if Root can separately
prove its own reservation was withdrawn. User startup preserves the step and
marks the unsettled launch recovery_required. A future executing reconciliation
must join both owners' evidence; neither a historical Root receipt nor absent
process may bypass this User obligation. No release/settlement API is added here.

This is durable intent and retention evidence, not a cross-owner ARM capability,
private-view readiness, target registration, app readiness or process drain.
It does not freeze another owner, prevent a privileged database edit or install
a User wire method. Actual current Root/User admission, mutation fencing and
recovery coordination remain next. Normal installed launch methods/gates and
public capabilities are unchanged.

`catalogue.user_execution` uses actual native owner catalogues and nine isolated
scenarios: fresh, lost reply, process exit before/after COMMIT, User restart, gate
closure and each of the three revision overflows. It also checks all thirteen
metadata-bound identity substitutions with valid recomputed request fingerprints,
all four mutating-statement failures, precommit rollback, exact retry byte/revision
stability, full-profile conflict, wrong-owner/invalid input, and rejection of the
old User withdrawal path. These are metadata/transaction tests, not live User
process admission or power-loss testing. Existing execution/kernel tests retain
their separately documented scopes.

## Connected User and Root execution admission

The isolated live test now replaces the synthetic broker context with an actual
registered UID1000 `aetheri_user_t` process under the independently selected
`aetheri-p0-execution-user.service`. Both owners use their own native catalogue;
Root never opens User SQLite. The separate wrong-unit service runs the same
protected binary/domain/UID and is rejected by actual broker registration.

Root loads the protected profile, verifies current sealed code and starter image,
then opens its install reservation gate. `ae_execution_code_open` checks code,
manifest, inventory and ELF without touching private User storage. In the actual
cap-zero Root unit, `ae_execution_data_open` is denied by the private owner boundary.
The User process is denied code-store opening and separately succeeds in checking
its exact allocation, owner, UID/GID/mode and raw MAC context. A substituted
publisher is rejected with unchanged outputs. Combined starter source opening
retains both checks; the five-capability starter still performs private setup.
Returned FDs are internal held sources, not public app/document transport.

The User bootstrap creates its catalogue with O_EXCL, begins its protected runtime,
registers on Root's actual bus connection and reaches notify readiness. Root's
existing broker module holds the bus-authenticated ProcessFD and original service/
invocation/cgroup, with name/exit subscriptions. Registration may see activating;
every later use requires active. User pins actual Root replies to their sent unique
destination/cookies, Root UID/domain/unit/invocation/boot and original process pin.
There is no owner-replacement fallback. Neither process acquires tracing privileges.

On the administrative Begin request, User fetches an authenticated current Root
installation projection, caches it natively, verifies/enrolls the independently
allocated data, opens its data gate and journals launch acceptance before Root
reservation. It binds the authenticated actual Root receipt, rechecks allocation/
MAC, and commits `ae_catalogue_prepare_user_execution`. Only its fresh submit flag
permits the original Dispatch request with the exact lease/profile digest.
The private envelope is fixed-fixture control, not a new Runtime1 contract.

`authority/execution_admission.[ch]` is a reusable Root-only process-owned adapter
around the endpoint and registered broker. Construction checks complete profile
equality with the endpoint. REQUEST re-observes the current registered connection,
matches account/User epoch/runtime/boot, derives context from that binding and
retains its own original ProcessFD before calling native REQUEST. Once attempted,
an unknown result cannot trigger another request. Closing the local method-capture
FD does not close that independent pin. ARM re-verifies the original connection
against the still-current broker before native arm/release. Departure or replacement
never inherits delegation. Callback-time close is EBUSY; fork use is ECHILD. Close
only releases local resources, never a lease or a process obligation.

Root trusts the independently protected User code/unit to perform its own journal
and allocation checks; the profile hash alone proves neither. The original User
request delegates only that fixed launch/profile/data fence. This test has no
concurrent owner mutation or revocation; general closure/stop/recovery coordination
still needs its protocol. Ordinary installed APIs and public capabilities remain
unchanged, and normal gates remain closed.

Actual enforcing negative controls reject a same-domain wrong unit, a new bus
connection from the original User process, and the same-UID app's privileged
Resolve attempt. Successful execution still requires original starter binding,
prepared release, private mounts, credential/FD drop, fresh app credentials and
independent running-ELF observation. The User and Root operations remain running;
neither app exit nor channel EOF settles them.

The controller journals each original start and preserves unknown dispatches.
User's fresh catalogue/lock are acknowledged only in the independently inspected
control slot, with exact expected new entries, object ownership/MAC and empty native
startup context checked against the protected profile. Other changes reject.
Native inherited SELinux xattrs may contain one terminal NUL; only the exact
expected label with or without that terminator is accepted, and the observed bytes
remain frozen afterward. It is not self-header adoption or a general allocator.
After exact app/User/Root process and unit/cgroup/job drain, archive both unresolved
catalogues, verify the whole owned tree and retire only the temporary fixture.

`authority.execution_admission` checks controlled endpoint/broker ordering with real
pidfd lifetime, replacement/lost-request guards, unchanged error output, callback-
time close and fork ownership. Its actor/endpoint adapters are explicit mocks; the
separate enforcing integration supplies actual identity/unit/policy evidence.
No normal service/profile or production app registration/readiness/recovery is
installed by either test. The next section records the subsequent fixed-fixture
app handshake. Dynamic allocation, session admission, executing drain/lease
settlement, native documents and complete removal remain required.

## User-mediated application registration and readiness

The isolated live test now includes an actual post-exec app handshake with the
actual User owner. It distinguishes a verified registered process from that
process's explicit readiness report. Neither state completes the launch operation
or releases Root's generation lease. Readiness here is a protocol declaration
from this inert fixture, not a UI responsiveness or application-health measure.
Normal service provisioning and public capabilities remain unchanged.

Root's test-only AppSource method accepts only the original registered User
connection. It rechecks the original native dispatch and invocation, reopens the
protected code profile/manifest/ELF, and checks the active exact generation,
Artifact and install gate/revision. The authenticated reply carries AETHOBS1:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 8 | AETHOBS1 |
| 8 | 32 | SHA-256 of the complete canonical AETHEXE1 profile |
| 40 | 16 | Actual Root dispatch invocation |
| 56 | 112 | AETHIMG1 expected executable observation |

This 168-byte test envelope is not an installed XML API or a bearer capability.
User authenticates the original Root reply/request cookies, unique connection,
ProcessFD, service/invocation/boot and context before consuming it. Root never
reads User SQLite or private allocation. A matching hash alone grants no access.

### Executable observation bytes

`linux/image.h` adds a padding-free AETHIMG1 codec. Its 112 bytes are:

| Offset | Encoding | Field |
| --- | --- | --- |
| 0 | 8 literal bytes | AETHIMG1 |
| 8, 16 | u64, u64 | Inode, file size |
| 24, 28 | u32, u32 | Device major/minor |
| 32, 36, 40 | u32, u32, u32 | UID, GID, complete mode |
| 44, 52 | i64, u32 | Birth seconds/nanoseconds |
| 56, 64 | i64, u32 | Modification seconds/nanoseconds |
| 68, 76 | i64, u32 | Change seconds/nanoseconds |
| 80 | 32 bytes | Content SHA-256 |

All integers are big endian; signed times use two's complement. Inode is a
nonzero full-width u64, size is 64 bytes through 16 MiB, ownership is root:root,
mode is regular 0555, birth seconds are nonnegative and nanoseconds are below
one billion. Modification/change seconds can be negative. Decode rejects trailing
or short input and invalid shape; both directions preserve outputs on error.
`spec/vectors/image-v1.json` freezes an independent Python/C round trip including
a negative modification time. These bytes describe an already checked file;
they do not authenticate a volume, publisher or process by themselves.

### Actual User observation

Test-only Register and Ready take no caller-selected IDs, paths, units or labels.
User captures the actual app connection's ProcessFD and requires UID1000 and the
exact compiled MCS context. `ae_process_observe` joins current process credentials
to the derived original app unit, current boot, actual invocation and cgroup.
User then authenticates Root's AppSource reply, checks the complete profile and
invocation, and uses `ae_image_match_process` on the live app. It rechecks its own
private allocation/owner/raw MAC and the current process context before the
native journal transaction. The original connection/process pin is independently
retained before registration commits, including an unknown commit outcome.

The temporary User policy permits the required app metadata, executable-file and
proc-link reads plus app/User messages. Direct code-store traversal remains
denied by DAC. No execute/map, SYS_PTRACE, KILL or Linux capability is added to
User. Linux documents `/proc/PID/exe` access under
[PTRACE_MODE_READ_FSCREDS](https://man7.org/linux/man-pages/man5/proc_pid_exe.5.html);
the pinned enforcing carrier proves this same-UID, exact-domain read path with
User's permitted/effective/inheritable/bounding/ambient capability sets empty.
It is not evidence for arbitrary processes, UIDs or hardware.

The test's AETHAPP1 evidence digest covers its literal tag, authenticated complete
Root envelope, bus GUID, actual boot/invocation/cgroup ID/PID and bounded
length-prefixed unique name/unit/label/cgroup. It binds observations for this
transaction, not durable PID authority. A second connection cannot inherit the
first registration, even from the same actual process. Every Register/Ready use
rechecks the original active actor and current source/data fences; no owner-
replacement fallback or replay after restart is implemented.

### Native User journal

`ae_catalogue_observe_user_execution` reuses v2; no frozen SQL/XML/profile changes
are needed. Caller-supplied evidence still requires all independent process,
Root, filesystem and current-owner observations above. Inside BEGIN IMMEDIATE,
the transaction checks the current User epoch/runtime/account/boot, exact accepted
launch and original Root link, full profile-bound preparation, active binding,
attached data, open exact data gate and original plan read revisions.

| Transition | Atomic changes |
| --- | --- |
| REGISTER | Step2 verify/integration/actual invocation/applied with context digest; step1 preparation becomes applied; launch becomes starting/unknown with that invocation; operation reason app_registered; increment launch, operation and catalogue revisions |
| READY | Require that exact registration; step3 activate/launch/applied with the same digest; launch becomes running/ready; operation reason app_ready; increment the same three revisions |

READY before registration rejects. Exact retries preserve file bytes/revisions;
REGISTER after READY returns the existing ready snapshot without downgrading it.
Changed invocation/evidence, closed or revised gates, owner restart and conflicting
steps reject. Revision overflow rejects before mutation. Error/unknown COMMIT
acknowledgement leaves caller outputs unchanged. Old preparation/withdrawal paths
cannot reinterpret registration as a new submission or a no-exec release proof.
The current fixed P0 fence rejects existing grants; future grant integration must
review retry semantics instead of silently weakening this boundary.

`catalogue.user_readiness` runs twelve actual native transaction scenarios:
fresh, lost REGISTER/READY replies, child exits before/after both commits, restart,
gate closure and overflow of each incremented revision. It injects every five
REGISTER/four READY write failure and commit boundaries, checks unchanged error
outputs, byte-stable retries, invocation/evidence conflicts, wrong owner/stage
and REGISTER after READY. These are transaction/restart tests, not power-loss or
general runtime admission evidence.

The live payload first attempts READY before REGISTER, then registers. Another
connection's READY is rejected; the original connection reports READY and repeats
both methods successfully. The controller checks the archived User running/ready
invocation equals Root's actual dispatch invocation and User steps0..3 are applied.
Registration/READY alone leave both operations running. The later completion
boundary below now acknowledges those start results while Root retains its
running lease after independent exact test drain. Histories are archived before retiring
the owned fixture. The payload's fixed test bus-name calls are not an authenticated
general-purpose app SDK. Normal session admission, executing recovery/drain,
documents and complete removal remain implementation
work; no public launch capability is enabled.

## Root execution observation journal

`ae_catalogue_observe_execution` records actual target-execution evidence after
ARM. It does not interpret a successful StartUnit reply, helper readiness, error
channel EOF or User READY alone as execution. The caller must join its original
held starter process to the current app connection/unit/invocation/cgroup and
the original authenticated User owner's independently verified image/registration
evidence. An app digest without that provenance grants nothing. Root still never
opens User SQLite or private storage.

The native Root transaction requires the full original profile, owner/runtime/
boot/account, exact launch/reservation tuple and original request fingerprint.
It verifies applied REQUEST/PREPARE binding steps1/2, the original starter
invocation/context digest and ARM step3. A fresh observation additionally requires
dispatching, the running generation lease, original invocation/data-gate fence,
active exact installation/generation/revision and the open original install gate.
Unknown later steps or a malformed optional diagnostic step4 reject mutation.

| Record | Atomic fresh mutation |
| --- | --- |
| operation_steps | Step5 verify/launch/exact LaunchId/applied with app-observation digest |
| operation_steps | ARM step3 becomes applied |
| launch_dispatches | dispatching becomes observed; increment revision |
| operations | Preserve an existing step4 diagnostic reason, otherwise set exec_observed; increment revision and retain running/no outcome |
| authority_meta | Increment catalogue revision |

No generation-lease state or revision changes, no process/IPC effect occurs and
effect_required is always zero. Overflow of any incremented revision rejects
before mutation. Error/unknown COMMIT acknowledgement leaves caller output
unchanged. The five writes and both commit boundaries are tested independently.

An exact existing step5 retry returns the current historical dispatch, including
later closing/terminal/uncertain state, without changing file bytes/revisions or
reopening a gate. A later terminal/released tuple is only a receipt: this API
does not create that drain result. Different app evidence conflicts. Original
profile/starter context/owner identity must still match; owner restart requires
separate recovery. The old REQUEST/ARM/no-exec paths cannot reuse this journal.
Actual closure and operation-result acknowledgement remain separate work.

A late terminal channel diagnostic can now record immutable step4 while the
dispatch is observed. Its bounded diagnostic reason remains in the operation;
step5 and the dispatch state separately retain actual execution observation.
EOF remains UNKNOWN; recording it before or after a real execution
observation neither establishes nor undoes that observation. It never releases
the generation lease or completes an operation.

### Connected fixed-fixture confirmation

The explicit live test adds PinApp and ConfirmApp only to its compiled test
interfaces. After User registration, the app's fresh original connection calls
Root PinApp. Root checks current UID/MCS and independently joins it to its held
original starter process, boot, actual service invocation/unit and cgroup. This
does not yet acknowledge execution or release the administrative observer.

After User has rechecked its original app/image/private-data context and committed
READY, that original User connection sends the bounded AETHACK1 test envelope:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 8 | AETHACK1 |
| 8 | 32 | Complete canonical AETHEXE1 SHA-256 |
| 40 | 16 | Actual invocation |
| 56 | 16 | App connection's actual bus GUID |
| 72 | 2 | Big-endian unique-name byte length |
| 74 | 1..255 | Exact app unique name |
| 74 + name length | 32 | User's original AETHAPP1 observation digest |

The test message is 106 + name length bytes, at most361. Root authenticates the
original currently registered User process/unit/connection, compares profile,
invocation, bus GUID and unique name to its own app pin, re-verifies that live
app context and original starter liveness, then hashes the complete envelope
for native step5. Its independent producer of the existing AETHEXC1 starter
context binding must match the original native BIND record. User authenticates
the original Root reply and exact operation/observed state. No incoming app FD,
asserted PID, arbitrary unit/path or User database is accepted as authority.

The app repeats REGISTER/READY through the original connection. AppSource can
return the same admitted image after Root's observed transition; it still checks
current code/gates. ConfirmApp exact retries preserve native history. A deliberate
wrong bus GUID is rejected as NeedsRecovery before any step5 mutation. The test
payload finally calls Observe on its same Root connection; only then can the
administrative driver observe/terminate the exact app and retire the owned fixture.
This final test-driver handoff is not a production readiness or drain API.

This observation boundary requires Root observed plus steps0..5/applied and User
running/ready for that invocation, without completing either operation. The later
completion boundary below adds the result acknowledgements. The lease/run closure
obligation survives exact administrative test drain/retirement. No normal service, policy, dependency,
SQL/XML version or public capability changes.

`catalogue.execution_observation` covers fourteen scenarios: fresh/lost reply,
actual child exits before/after COMMIT, closed admission, Root restart, each of
three revision overflows, early/late channel diagnostics and historical retries after
simulated closing/terminal/uncertain states. It also checks missing ARM, wrong
owner/input/profile/invocation/starter evidence/broker runtime, changed app digest,
every write fault, output preservation and byte-stable retries. The simulated
later states are receipt tests, not execution-drain evidence. Normal owner/session
provisioning, executing recovery/drain/lease settlement,
documents/revocation and complete removal remain work.

## Launch-result completion without run or lease release

The start operation and the running app have separate lifetimes. Root now
acknowledges the known start postcondition with an immutable AETHLCR1 receipt.
User independently acknowledges that receipt against its original registered,
ready app. Both start operations complete applied with result_kind=launch and
the exact LaunchId. The Root dispatch remains observed, its generation lease
remains running, and the User launch remains running/ready. This neither stops
the app nor permits code/data removal. Closure/recovery/drain remain separate.

`ae_catalogue_complete_execution` requires the original Root owner/runtime/boot/
account, full profile and original applied request/bind/prepare/ARM/observation
steps. Fresh completion requires observed execution, the running lease and exact
current active install/generation/gate. The original applied step4 must contain
the canonical closed UNKNOWN channel diagnostic. This proves channel settlement
only; step5 and the caller's independently rechecked original app/User evidence
prove the actual start. Failure/malformed/observer diagnostics cannot become
successful start receipts. The bounded diagnostic reason remains intact.

| Root record | Atomic changes |
| --- | --- |
| operation_steps | Step6 reconcile/launch/exact LaunchId/applied with receipt SHA-256 |
| operations | completed/applied/reconcile, original diagnostic reason, result launch/LaunchId and completion time; increment revision |
| authority_meta | Increment catalogue revision |

No dispatch, generation lease or gate mutation occurs. Before committing, Root
encodes the resulting operation revision and completion time into the receipt.
An exact retry reconstructs the original receipt and checks its immutable step6
digest; a different supplied current time never changes it. Later closing,
terminal/released or uncertain state is preserved as history, never reactivated.
Wrong profile/context/evidence and original-owner replacement reject. Current
runtime-only acknowledgement is not cross-restart recovery. No external effect
occurs in SQLite, and unknown commit acknowledgement preserves caller outputs.

### AETHLCR1 receipt bytes

The canonical receipt is exactly 200 bytes, all integers big endian:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 8 | AETHLCR1 |
| 8, 24, 40 | 16 each | System epoch, original Root runtime, boot |
| 56, 72, 88 | 16 each | Root operation, LaunchId, LeaseId |
| 104 | 16 | Actual invocation |
| 120 | 32 | Complete AETHEXE1 SHA-256 |
| 152 | 32 | Original Root step5 observation digest |
| 184 | 8 | Completed Root operation revision, 1..2^63-1 |
| 192 | 8 | Original completion Unix milliseconds, 0..2^63-1 |

The first six IDs are v4 UUIDs; the supervisor invocation is a nonzero 128-bit
value. Digests are byte values, not provenance. Codec errors preserve outputs;
short/trailing input, invalid IDs and integer bounds reject. The independent
Python encoder and native C codec use the frozen
`spec/vectors/execution-completion-v1.json` vector. `ae_execution_completion_match`
checks the exact profile/Root identity/operation/run/lease/invocation and expected
observation digest. It cannot authenticate the sender of a copied receipt.

User must authenticate the reply to its original unique-destination Root request,
including cookies and actual Root ProcessFD/domain/unit/invocation/boot, and
derive the observation digest from its own original AETHACK1 confirmation. Root
never reads User SQLite or private storage. User never treats an arbitrary packet
or root-looking name as completion authority.

### User acknowledgement

`ae_catalogue_finish_user_execution` first validates the authenticated receipt's
complete binding. Inside its own transaction, the original User epoch/runtime/
account/boot, exact launch/root link/profile preparation, registered invocation/
context and ready evidence must agree. Fresh completion rechecks current active
binding/attached data, open original data gate and original plan-read revisions;
the launch must be running/ready with no later work or grants in this P0 profile.

| User record | Atomic changes |
| --- | --- |
| operation_steps | Step4 reconcile/launch/exact LaunchId/applied with the same Root receipt SHA-256 |
| operation_links | Original Root link becomes completed |
| operations | completed/applied/reconcile, launch_started, result launch/LaunchId and own completion time; increment revision |
| authority_meta | Increment catalogue revision |

Launch/data/gate state and revisions do not change. Root can already be completed
while User acknowledgement is lost: retain that original obligation and resolve
the same receipt, without another start. Exact User retries preserve original
time/revisions and later exited/closed history. A changed receipt conflicts.
Original User replacement requires separate recovery, never rebinding the run.
Both owners reject revision overflow before mutation and leave outputs unchanged
on every error/unknown commit. Completion timestamps must not precede that owner's
acceptance; wall-clock values are not drain or access authorization.

### Executed fixed-fixture path and limits

The explicit live test adds private FinishLaunch and app Complete methods only
to its test vtables. After registration/READY and original Root observation,
Complete rechecks the original app/image/private data, obtains the authenticated
Root receipt and commits the User acknowledgement. Repeated Complete returns
the same result. It does not call Register/Ready again after completion or turn
their handshake into permission to restart. Root still rechecks the original
User broker and held app/starter context before replying.

The controller independently reconstructs AETHLCR1 from the archived Root database
using the Python reference. Its SHA-256 must equal Root step6 and User step4,
with the original link completed and both results bound to the same LaunchId.
The archive adds launch-completion.bin (200 bytes) and completion_sha256 in the
report; these are historical test evidence, never package/app capabilities.
Root steps0..6 and User steps0..4 are applied, Root's lease stays running and User
stays running/ready until independent administrative test drain. Fixture retirement
still leaves that native lease/run closure obligation untouched.

`catalogue.execution_completion` covers nineteen native scenarios across both
owners: fresh, lost replies, actual child exits before/after each commit, gate/
owner replacement, revision overflows, missing/conflicting diagnostics, missing
User readiness and exact retries after simulated later terminal/exited history.
All three Root/four User writes and commit boundaries are faulted, with rollback,
unchanged error output, stable original receipts/times and held-lease/revision
checks. Simulated later states are receipt tests, not live drain evidence. The
separate enforcing test supplies actual peer/process/image/IPC evidence. Normal
session provisioning, general app SDK, executing recovery/drain/lease settlement,
native documents/revocation, complete removal and desktop/image/hardware remain
work. No frozen SQL/XML, normal profile/policy or public capability changes.

## Original-process lifetime observation

`linux/lifetime.h` adds a Root-owned, single-threaded, read-only observer. Capture
starts from an already admitted live service leader's actual ProcessFD and current
process context. The caller keeps its existing original supervisor unit reference
and trusted host system bus alive. The observer duplicates the process pin, holds
the exact cgroup-v2 root/group and opens that group's original cgroup.events.
It verifies the live context again before returning. Neither caller-supplied
paths nor a later same-name process can select the observed objects.

`ae_lifetime_observe_end` requires all of the following, then repeats the unit
and filesystem observations:

| Observation | Required current evidence |
| --- | --- |
| Original process | Held ProcessFD reports dead |
| Original service | Exact original unit object/name and InvocationID |
| Terminal supervisor state | inactive or failed; MainPID zero; original ExecMainPID; Job `(0,"/")` |
| Unit group selection | Empty ControlGroup or the original exact group path |
| Held filesystem objects | Same non-recycled mount identities, inode/filesystem and root metadata |
| Original recursive group | Still named as the held object with populated=0, or missing name plus original event reader deactivated/ENODEV |

The absent-name branch does not use link counts: the pinned carrier still reports
st_nlink=2 after the original group is removed. It also does not parse a procfs
`(deleted)` pathname. The already-held kernfs reader supplies an independent
deactivation observation. A missing name while that reader still works, or a
different object under the old name, rejects and permanently quarantines the
handle. This branch is checked against the pinned carrier; other kernel behavior
fails closed and needs separate acceptance. The
[kernel cgroup-v2 documentation](https://docs.kernel.org/admin-guide/cgroup-v2.html)
defines populated across the subtree and the constraints on group removal.
[Upstream kernfs read handling](https://github.com/torvalds/linux/blob/master/fs/kernfs/file.c)
explains the deactivated-reader error; that source is background evidence, not
a claim to have reviewed the exact Fedora kernel patch set.

Unsettled state returns EAGAIN. Substitution latches ESTALE, inherited-handle use
after fork returns ECHILD and callback-time close returns EBUSY. Errors leave
the output unchanged. Closing only releases this object's local handles; it
does not UnrefUnit, stop a service, free a lease, retire data or revoke handles.
The native module creates no thread, daemon or polling loop.

The privileged `linux.lifetime` test uses an isolated root-only component bus
and actual host PID1/process/cgroup facts in the disposable enforcing carrier.
It covers successful/failed units, an inactive job-free leader with only a live
nested child, actual same-name restart and mount-namespace replacement, fork,
busy close and FD balance. Controlled unit-property/lookup faults reject jobs,
main PIDs, invocation changes, missing names and wrong objects; these faults are
not actual cgroup rename tests (v2 rejects the attempted rename). Its private bus
reuses the existing test launcher and edits no installed bus policy. Failure
preserves its inputs/logs and printed service identities for inspection.

The explicit live execution fixture captures this observer from Root's actual
post-exec app pin while the coordinator still holds the original unit reference.
After the administrative driver signals its independently verified app pidfd,
the test-only ObserveEnd reply is authenticated back to original Root and compared
with the original app PID/cgroup/invocation/boot. PID1 property signals plus a
bounded test-only requery cover asynchronous cgroup removal. The controller
archives this snapshot as native_end, independently checks unit/job/group drain
for fixture retirement, and still requires the native lease running and User
launch running/ready in the historical databases. Only this temporary test policy
adds read/open/getattr for Root's cgroup.events; no write or kill permission.

This is one current observation, not a permanent drain token or restart recovery.
Future dispatch/migration must be excluded and external/delegated holders settled
before it can contribute to full app drain. Durable executing run closure, lease
settlement, recovery coordination and User acknowledgement remain separate work.
The old no-exec withdrawal proof is unchanged. No normal profile, frozen SQL/XML,
installed lifecycle capability or production launch/removal acceptance is added.
