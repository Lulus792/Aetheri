# Developer build and capture storage

## Where development storage goes

| Path | Purpose | Lifetime |
| --- | --- | --- |
| `vm/p0/` | Fedora carrier with protected prototype state and its build toolchain | Keep the registered disk and its state |
| `vm/audio-lab/` | Fedora carrier for current desktop and device testing | Keep the working disk and its state |
| `local/` | Pinned tools, active test media and archived test evidence | Keep active inputs and one verified copy of each historical artifact |
| `downloads/` | Verified installers and package inputs | Keep the versions used to reproduce an active environment |
| `out/` and Windows temporary directory | Build output and test scratch files | Retire generated copies after the case ends and its evidence is retained |

The two Fedora disks belong to the development environment. See the
[current inventory and cleanup record](../brains/aetheri/journal/2026-11-18-storage-retention.md#archive-and-scratch-cleanup)
for their measured sizes and the tracked project size. A virtual disk's capacity,
its allocated size on Windows and its free space inside the guest describe three
different quantities.

## Keep and retire test artifacts

Keep one selected, reusable boot base and seed data disk per active test
profile. Build VM cases from the read-only base with changed block extents
and a small writable data overlay. A virtual USB image can report a large
capacity even when it occupies little space. Use allocated bytes and
volume free space when planning the next build.

The desktop and writable-data test controllers use sparse copies for private
raw data disks. They verify the copied image before attaching it to the guest.

Keep the current checkpoint in [project state](../brains/aetheri/STATE.md) and
the source revision, input pins, image hash and observed results in one dated
journal entry. Link from that entry to the local receipts, compressed logs,
required screenshots and fixture sources. Put the reusable contract in the
subsystem spec.

Reuse the same source archive when later cases have identical inputs. Pass a
large payload separately to its launch script and record its hash. For a
historical build file that also exists in a verified archive, record the archive,
member and SHA-256 before retiring the extracted copy. Keep the archive and its
receipt so the file can be restored.

After the VM exits, check the backing paths of retained QCOW2 and VMDK files.
Keep full images that serve as active inputs, recovery copies or irreplaceable
evidence. Verify a replacement before removing a duplicate. Keep working VM
disks, pinned inputs, hardware test data and the current USB recovery image.
Review both the output directory and temporary scratch after each test batch.
Keep the input pins and the small verification receipt together with the case so
the next run can reuse the base without copying a full image.

When a historical raw regular-file image must remain byte-for-byte available,
NTFS can release space occupied by verified zero ranges. Use an exclusive file
handle on a completed case and verify the full digest before and after.
This is a host file operation. Physical media, active VM disks and active
overlays require their own handling.
[NTFS sparse file control](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_sparse)
and [zero range control](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_zero_data).

## Separate carrier work disk

7 November 2026. The carrier root is almost full. A separate fixed 8 GiB work
disk keeps future builds and captures away from protected prototype state.
It does not move current programs, SDKs, documents, catalogues or test history.
This is disposable development infrastructure, not Aetheri's production storage
allocator or the resettable experiment product.

### Enrollment checkpoint

The host image is prepared and registered. Actual QEMU cold attachment and its
block-node/serial observations pass in a separate stopped-CPU probe. A real
64 MiB loop-device test in the enforcing carrier passes format, mount, normal-user
compilation, execution, remount and content retention. Enrollment of the fixed
disk was pending at the 7 November checkpoint. The original run left the
carrier online for a separate NexaUI test helper.

The current q35 configuration has no hotplug-capable root port. The same-version
isolated QEMU probe rejects both direct root-bus virtio and existing IDE-bus
hotplug. No hotplug mutation was attempted on the running carrier.

### Ownership and lifetime

`vm/p0/development-storage/registration.json` independently binds the existing
VM UUID, random operation token, fixed serial, filesystem UUID, 8 GiB capacity
and original host image file ID. `work.qcow2` has no backing image, external data
file, snapshots or encryption. The image is sparse; 8 GiB is its virtual capacity,
not preallocated host space. Creation requires 9 GiB host free space. Continue to
monitor host free space as the guest writes to it.

The private `development-storage.lock` serializes image creation separately from
the VM's lifetime `launch.lock`. The launch lock remains exclusive. Creation
records `creating` before invoking qemu-img exactly once. A missing acknowledgement
or replaced object refuses recreation and boot attachment. Never adopt another
image from its name, header or matching content. These file IDs and private
directory assumptions guard accidental replacement by a trusted host operator;
they do not authenticate cloned volumes or hostile administrators.

Normal future VM boots add the registered image as a separately named virtio
block device. A registered work disk blocks carrier reinstallation, rather than
exposing it to the installer. There is no automatic detach/delete/reformat action.
The runner does not hotplug, reboot or format it.

Guest enrollment selects only that exact virtio serial under the independently
selected VM UUID. It checks the held block object, exact size, sysfs serial,
absence of partitions/holders and separation from the running root disk. It
requires an entirely zero-filled device before journaling the sole format attempt.
Root state is retained under `/var/lib/aetheri-development-storage`. `formatting`
after interruption requires reconciliation; no command retries mkfs or adopts
an existing filesystem as a successful enrollment. mke2fs retains its own in-use
guard; no force option is supplied.

The new ext4 filesystem uses the host-selected UUID and no reserved developer
blocks. That does not change the carrier root's reserve. Mount checks require
the exact device/UUID and `rw,nosuid,nodev`, with execution enabled for builds.
`/mnt/aetheri/work` is UID/GID1000 mode0700; mount root remains root-owned0755.
Directory metadata and state are flushed before acknowledging readiness. SELinux
stays enforcing with existing labels; no new policy rule or domain is installed.

### Use after a safely coordinated next boot

From the Windows repository, with the existing Python runtime:

```text
python tools/development_storage.py prepare
python tools/development_storage.py status
```

Preparation is complete on this host. Once the running guest's other work has
finished, inspect original sessions/processes/jobs and the protected prototype
state before an ordinary shutdown and boot. The next `run_vm.py boot` includes
the disk. Preserve the desired `--offline`/`--console` options.

Then explicitly enroll the new disk once:

```text
python tools/development_storage.py initialize-guest
```

The host verifies its actual QMP node, image and serial before sending the
repository's fixed setup code/profile over the existing protected SSH connection.
The guest repeats VM/device checks. No writable host directory is shared.
Initialization keeps its root registration and returns the actual work path and
available space. An unknown SSH result is not permission to initialize again.

On later boots, explicitly mount the original enrolled filesystem:

```text
python tools/development_storage.py mount-guest
```

This uses the original protected guest registration and checks equality with the
host profile. It never formats. It accepts an exact existing mount and refuses
wrong UUIDs/devices, unknown format state and nonempty uncovered mountpoints.
Automatic boot mounting, actual fixed-disk enrollment and moving future build/
capture consumers onto it remain pending. Do not change old normal build paths
or historical records to make them appear migrated.

### Verification

Host tests use temporary files and mocked image/SSH effects:

```text
python -m unittest discover -s tools -p test_development_storage.py -v
python -m unittest discover -s tools -p test_vm_tools.py -v
```

The explicit `test_development_storage_guest.py` requires root, the enforcing
disposable P0 identity, a private mount namespace and
`AETHERI_TEST_DEVELOPMENT_STORAGE=1`. It creates only a new 64 MiB `/run` loop
fixture, runs the production format/mount path with a test-only loop selector,
then independently exercises the real selector's missing-device, regular-file
and root-disk rejection. It is not a default host test or a production installer.
It does not prove the still-unattached virtio disk's eventual guest admission.
See the [executed record](../brains/aetheri/journal/2026-11-07-development-storage.md).
