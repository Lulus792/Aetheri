# Developer build and capture storage

7 November 2026. The carrier root is almost full. A separate fixed 8 GiB work
disk keeps future builds and captures away from protected prototype state.
It does not move current programs, SDKs, documents, catalogues or test history.
This is disposable development infrastructure, not Aetheri's production storage
allocator or the resettable experiment product.

## Current boundary

The host image is prepared and registered. Actual QEMU cold attachment and its
block-node/serial observations pass in a separate stopped-CPU probe. A real
64 MiB loop-device test in the enforcing carrier passes format, mount, normal-user
compilation/execution, remount and content retention. The fixed host image has
**not** been attached, formatted or mounted in the running carrier. That machine
still has a live foreign NexaUI test helper; do not reboot it to activate this disk.

The current q35 configuration has no hotplug-capable root port. The same-version
isolated QEMU probe rejects both direct root-bus virtio and existing IDE-bus
hotplug. No hotplug mutation was attempted on the running carrier.

## Ownership and lifetime

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

## Use after a safely coordinated next boot

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

## Verification

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
