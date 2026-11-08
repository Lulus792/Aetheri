# USB root and initrd handover

This development profile tests the missing early USB-storage path before joining
it to the graphical image. It keeps systemd as PID1 in both the initrd and the
real root. The existing read-only system filesystem is reused byte for byte;
the new UKI supplies selected modules, explicit initrd units and an Aetheri C17
preflight. It is not the image to flash onto the physical laptop-test stick yet.

## Components and sequence

1. UEFI loads the unsigned fallback UKI from the emulated USB medium. The kernel
   executes the initrd's `/init`, which selects the existing systemd executable.
2. The selected kmod tool loads `usb_storage` and `uas`. The pinned kernel already
   provides the USB controllers, SCSI disk path and ext4. No kernel rebuild or
   package installation is performed.
3. A small udev rule probes block metadata and publishes the expected PARTUUID
   alias. The mount graph waits for that device; it does not guess a disk letter.
4. `aetheri-initrd-device.service` opens the expected partition read-only. The C
   preflight joins its actual block-device number to kernel sysfs metadata,
   requires exactly one matching partition, checks USB ancestry and removable
   status, and verifies the independently expected filesystem UUID and geometry.
   It records this boot's device numbers, diskseq and held sysfs-parent inode
   observation in a fresh root-owned0700 runtime directory.
5. Only this successful preflight allows `sysroot.mount`, with ext4 `ro,noload`.
   A second preflight repeats device checks, matches the original observation,
   requires the actual mounted read-only device and checks the real root profile.
6. The initrd isolates its switch target so the original udev process stops.
   Another read-only check precedes the standard systemctl/PID1 SwitchRoot call.
   systemd handles the root transition and API filesystems; Aetheri does not
   replace that mechanism with a new PID1 or a shell-script framework.
7. The original real-root diagnostic runs after handover. It requires actual
   UEFI/systemd, the expected partition, ext4/read-only state and a failed write
   with EROFS. Its existing image-only action then powers off this disposable VM.

The immediate poweroff is diagnostic behavior, not the eventual desktop shutdown
path. The graphical profile already has separate normal shutdown evidence.

The successful mount check enters the inactive state. Its `OnSuccess=` with
`OnSuccessJobMode=isolate` lets PID1 queue the switch target directly. That target
retains the original mount/device preflight and orders handover after udev stops.
The final read-only check runs in the switch service before its root RPC.

## Inputs and effects

`spec/usb-root-inputs.json` pins the original RAM/root manifest digests and each
new runtime/module payload against existing RPM file metadata. The builder also
independently inspects both original artifacts before producing output. Kernel
release and the original root partition/ESP must agree. Original filesystem
bytes and inventory are retained; only the selected ESP/UKI changes.

The inventory explicitly includes the libkmod DSO loaded by udev. On this carrier
the kmod command embeds its own module code, so its `ldd` list alone does not
describe udev's dynamically loaded backend. Imported block properties are tested
in a separate udev rule after the import has completed.

`tools/build_usb_root_probe.py` runs as an ordinary x86-64 Linux user, writing a
fresh regular image file and initrd tree. It does not mount or format a host
device, load a module into the development carrier, install packages, or alter
the original images. Third-party licence files remain included. These pins
establish development input consistency, not current vulnerability clearance or
complete public binary-distribution compliance.

This first fixture deliberately requires a512MiB whole device, its256MiB root
as partition2 at128MiB, and the independently configured identifiers. That bound
is not the capacity of the authorized physical USB stick. Joining the graphical
root, persistent data partition and actual stick geometry is a subsequent
required integration step, followed by another removable-media VM test.

## Identity limits

Diskseq is a current kernel disk observation, including when queried through a
partition. The boot ID, device numbers and sysfs inode are also runtime facts.
They do not authenticate a disk across reboot, a clone or the publisher. The
runtime observation is not a grant, lease, durable volume enrollment or portable
capability. No file descriptor is exported to an app. Repeated checks narrow
lookup/replacement errors; they are not a complete hot-unplug/recovery protocol.

The preflight rejects a regular file, wrong size/offset/identifier, multiple
matching partitions, a non-removable or non-USB transport, and mismatched ext4
metadata before the root mount. It preserves failed/partial runtime state rather
than replacing it. A changed original observation or mounted root prevents
handover. It never mounts a data filesystem read-write.

## Verification

`tests/boot_initrd_test.c` exercises bounded profile/UUID/device-number parsing,
overflow, exact superblock metadata, truncated/embedded-NUL reads and observation
differences as an ordinary user. Run Release and ASan/UBSan separately.
`python tools/test_usb_root_contract.py` checks payload/result/command boundaries
without starting a VM. Existing RAM/root/UEFI contracts remain separate.

`tools/test_usb_root_probe.py` explicitly boots this profile in a new private VM
with fresh firmware variables and read-only owned media. It has no network,
shared folder, physical disk, QMP or direct kernel/initramfs injection. Positive
acceptance requires ordered, same-boot device/check/handover and real-root results,
the original process exit0 and actual kernel poweroff. A virtio-only negative
must fail the transport check before mounting. A separately built wrong-filesystem
profile must fail that specific preflight instead. Exit0 alone never passes.
Failed/unknown output directories are retained and never reused automatically.

Actual executed evidence belongs in STATE and the dated journal after the tests,
not in this description of the intended checks. No physical-media, laptop GPU,
firmware/input, persistent desktop-data or production security acceptance follows
merely from compiling this profile.

## Primary references

- systemd259 [boot sequence](https://raw.githubusercontent.com/systemd/systemd/v259/man/bootup.xml)
  describes initrd PID1 and handover.
- Its [switch-root implementation](https://raw.githubusercontent.com/systemd/systemd/v259/src/shared/switch-root.c)
  handles the API mounts and root transition; the
  [systemctl client](https://raw.githubusercontent.com/systemd/systemd/v259/src/systemctl/systemctl-switch-root.c)
  requires a local initrd and calls PID1's SwitchRoot method.
- Linux6.19 [block ioctl implementation](https://raw.githubusercontent.com/torvalds/linux/v6.19/block/ioctl.c)
  returns the parent gendisk's diskseq for BLKGETDISKSEQ. This is not a durable
  volume identifier.
