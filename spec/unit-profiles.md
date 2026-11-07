# Durable unit allocation and selection

This internal Root contract records the intent to create a launch unit before
filesystem effects and binds its original witnessed selection before execution
REQUEST. It is separate from app data, mount-view allocations and the later
systemd reference/subscription lifetime. It does not install a normal launcher,
authorize an app or complete recovery/removal.

## Owner journal

[Root v9](schema/system-v9.sql) adds `unit_allocations` only. Normal creation stays
v2; opening never upgrades. Explicitly upgrade a quiesced Root v8 through
`migrate_units` or `ae_catalogue_upgrade_units` under the same retained writer
lock. Local effect holds reject upgrade. User v9 is unsupported. Migration adds
no rows, changes no original table/gate/runtime/revision and never backfills old
executions. A lost acknowledgement inspects the actual same database; never
restore a backup over an unknown commit. Frozen v1-v8 SQL/XML remain unchanged.

Each row has a separate UUIDv4 unit-allocation ID, original root operation/lease,
full canonical AETHEXE1 and its digest, primary fragment SHA-256, original
directory/parent placement digest and
preparation timestamp. Selection digest and witness timestamp are initially NULL.
This ID is neither StorageRef/DataEpoch nor the profile's private-view allocation
digest, policy label or MCS category identity.

`ae_catalogue_prepare_unit` checks the original current owner/broker/tuple, open
install gate, source fence and absent REQUEST. Only a fresh confirmed commit
returns `create_required=1`. Exact retry returns zero even after a lost commit;
it never grants another creation attempt. Changed IDs/recipe/profile conflict.
Placement is immutable too; equal fragment bytes do not authorize another parent.
The creation adapter checks current source admission again after that commit.

`ae_catalogue_witness_unit` publishes one independently obtained selection digest
before REQUEST. Only preparing -> witnessed is allowed. Exact historical retry
retains the original digest/time, gates and later execution state; it cannot
rearm anything. Raw SQL cannot mutate the identity/recipe or an existing witness,
delete these histories, or insert a new executing REQUEST/ARM without the original
witness under the original Root runtime/boot. Fresh native REQUEST/ARM also check
the witness. Old v8 executions are not retroactively enrolled by migration.

`ae_catalogue_get_unit` reads historical metadata under the current Root owner of
the same authority epoch, including an earlier runtime/boot. This is no adoption
of its process, unit, bus connection or FD and no new effect permission. The Root
adapter still requires current original context, absent REQUEST, matching fragment
recipe and exact selection before moving its unused unit owner.

Both preparing and witnessed records remain visible in the bounded recovery
inventory. Witnessed means the selected inputs were recorded; it does not mean
references, processes, operations or generation leases have retired. There is no
v9 unit-custody retirement/purge protocol. A separate admitted protocol must settle
those obligations before future pruning can be implemented.

## Native primary-fragment creation

`ae_root_unit_create` copies the selected profile/context/allocation, protected
parent/path and bounded literal fragment before compiled admission callouts.
It holds the original catalogue through the operation, checks the selected
recipe, and records preparation before any file effect. Only the fresh result
can enter creation. It repeats current source/gate and original parent checks.

The fragment is written to O_TMPFILE, receives the fixed raw systemd-unit-file
label and mode0444, and is fully synchronized. Create-only link publishes the
launch-derived name without replacement, followed by another file flush and
parent flush. A read-only named FD must match the held writer's original object,
birth/metadata/digest and protected parent. Success returns that original FD and
witness; it does not publish the complete graph/image selection or start/reload
systemd. The caller still admits dependencies, global defaults, image/data/MAC,
configuration mutation exclusion and the actual original volume.

Any error closes local handles without deleting a possible named file. The
original allocation remains pending. A retry cannot recreate, relink, rewrite
or adopt a matching candidate. No fallback, rollback, automatic reconciliation
or daemon reload is provided. Closing an unlinked temporary FD is only local
kernel-resource cleanup; it cannot retract an already linked publication.
The test family's `/run` unit files are volatile. Durable intent is not a promise
that a runtime file survives reboot; a new boot cannot recreate or adopt this
old allocation. Original volume/owner admission and custody recovery stay separate.

## Canonical selection digests

All integers below are big endian. Text is UTF-8/native path bytes without NUL,
prefixed by a four-byte byte count. Descriptor numbers, struct padding, optional
statx fields and access times are excluded. Mandatory object/parent birth data
prevents a matching name/content alone from standing in for the original object.

`AETHUNP1` hashes tag, boot, four-byte node count and ordered nodes. Each node
contains four-byte kind, name and fifteen one-byte edge masks in `unit.h` order.
Built-in root/system slices end there. Other nodes add directory, parent, file
witness, four-byte drop-in count and ordered drop-ins (directory, leaf, parent,
file). Services add executable, argc/argv, envc/environment, label, capability and
ambient masks, four boolean words and umask. Unused fields are excluded.

Parent fields are mount ID, inode, device major/minor, UID/GID/mode, filesystem
type/ID and birth seconds/nanoseconds (8,8,4,4,4,4,4,8,8,8,4 bytes). File fields
are mount ID/inode, device major/minor/mode/UID/GID/link count, size, mtime/ctime/
birth seconds+nanoseconds and SHA-256 (8,8, six4,8, three12,32). Birth is required.
Signed time values use two's-complement 64-bit encoding.

`AETHUIM1` hashes tag, image directory/leaf/raw label, parent and canonical
112-byte AETHIMG1. `AETHUNT1` hashes the 104 bytes tag + AETHEXE1 digest +
AETHUNP1 digest + AETHUIM1 digest. The independent [Python reference](../tools/unit_contracts.py),
[frozen vector](vectors/unit-selection-v1.json) and native checks bind the format.
Native profile encoding is bounded to 262,144 bytes; image selection to 8,192 bytes.
`AETHUPL1` hashes tag + intended directory + original parent before allocation.
The Root constructor matches that original placement as well as full selection.

These are current-boot input fingerprints, never publisher trust, durable
volume/clone authentication, process/loaded-image evidence or bearer capabilities.
The complete independent selection and filesystem admission remain prerequisites.
Recovery must not authorize a candidate from its own matching header/content.
