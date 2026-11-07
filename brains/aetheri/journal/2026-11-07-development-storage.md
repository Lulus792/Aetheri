# Separate developer disk preparation — 7 November 2026

## Reason and scope

After the NexaUI promotion the carrier root had only 77,533,184 user-available
bytes. The user authorizes supported technical recommendations and F: capacity;
the initial 32 GB discussion is not a permanent whole-project ceiling. A separately
registered 8 GiB developer disk avoids resizing the existing root or moving protected
objects. It uses existing reviewed QEMU 11.1.0 and ext4 tools, not a new library family.

`tools/development_storage.py` implements fixed host creation, original-object
registration, future cold-boot arguments and guarded explicit guest dispatch.
`tools/development_storage_guest.py` implements the one-attempt blank-disk format
journal and original-filesystem mount. The [contract](../../../spec/development-storage.md)
records paths, permissions, interrupted acknowledgements and current limits.

## Executed evidence

Local evidence is retained under `local/build-storage-2026-11-07/`.

- QMP/guest inventory records the original running VM, root disk, mount topology
  and tools: Fedora 44 e2fsprogs 1.47.3-4, util-linux 2.41.3-12 and systemd 259.10-1.
- A separate 128 MiB stopped-CPU QEMU q35 probe with no guest disk rejects IDE and
  direct root PCIe hotplug. No live-carrier hotplug was attempted.
- The fixed 8 GiB qcow2 is created once and retains its original host file ID.
  `qemu-img check` succeeds; its entire virtual map is zero/non-data. Initial host
  allocation is 197,120 bytes. Full profile and image observations are retained.
- A separate stopped-CPU QEMU probe actually opens the new disk with the future
  boot arguments. Its `query-block` node, qcow2/read-write/image binding and QOM
  serial match. The probe quits normally; it executes no guest code.
- Fourteen host storage tests pass, including unknown creation/SSH replies,
  original-image replacement, backing/encryption/snapshot rejection, insufficient
  capacity, wrong VM/serial, missing attachment and installer exclusion.
- The VM-tools regression suite passes 52 tests. Its original launch lock stays
  exclusive while a separate storage lock can be acquired. A pre-existing
  system-slot fixture omission initially attempted a real nonexistent `/var/lib`
  file; the test now mocks and asserts that external publication explicitly.
  No production authority behavior was changed.
- Two explicit enforcing guest cases pass. A new 64 MiB loop fixture exercises
  complete zero checking, ext4 creation, mount, UID 1000 compile/execute using its
  own TMPDIR, persisted bytes across unmount/remount and negative guards. The
  unmodified actual selector separately rejects a missing serial, regular file
  and the carrier root. These are loop-path evidence, not actual new-virtio
  enrollment or a production allocation/volume proof.

The first native test correctly hit mke2fs's in-use guard while our scanner still
held an exclusive block claim. The handoff now preserves the same block object
through a nonexclusive FD and lets mke2fs obtain its own claim; no `-F` bypass was
added. The next compiler probe exposed the already exhausted shared `/tmp` quota;
its TMPDIR now stays on the new private fixture. Failed logs remain distinct from
the final passing evidence. All original test mounts/loops are retired normally.

The initial host preparation attempted the lifetime VM launch lock and was refused
before creating an image. Separate resource locking now permits creation of an
unattached disk without weakening that existing launch lock.

## Preserved state and next work

The running carrier still contains foreign NexaUI helper PID 955243/session 10987;
it was observed live, not inferred from a lock file. No shutdown/reboot, existing
disk modification, new guest disk enrollment, normal source/build replacement or
quota/reserve/policy change was performed. The current normal DRM record remains
CLOSED; protected prototype history is retained.

The final read-only audit verifies the unchanged 445 guest source inputs, 31
frozen SQL/XML contracts, 19 quarantined object hashes, 52 normal P0 files,
normal authority image, old SDK/SDL inputs and current closed DRM record. Only
the two explicitly changed host orchestration/test files are excluded from the
old host source comparison; new tool inputs have separate tested hashes. All
own loop fixtures are absent, SELinux is Enforcing and the original boot remains
unchanged. Root user-available space is still 77,533,184 bytes. The actual current
`initialize-guest` request is rejected by the host before SSH because the new
disk is unattached. All 1,602 local documentation links and Python compilation
checks pass. Four foreign planning/design edits remain outside this change.

Next: coordinate an actually idle carrier boundary, preserve/audit original
prototype state, cold boot with the fixed disk, execute the explicit guest
enrollment, verify actual virtio admission/free space and use it for fresh builds
and captures. Add automatic mount only after that original registration exists.
Do not wait on or terminate the other project's helper without authorization.
Other OS work may proceed independently while that restart is deferred. Full OS,
apps, production admission/recovery/removal, image/installer, physical hardware and
the full experiment environment remain incomplete.
