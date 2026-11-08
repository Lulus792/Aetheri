# Laptop USB flash

This Windows host tool writes the already authorized Intenso Speed Line. It is
a fixed development procedure, not the Aetheri installer or a general disk tool.
Original Aetheri code is MIT; it uses the existing Python runtime and Windows APIs.

## Inputs and target

The source is the pristine laptop-v2 image, not a VM's modified medium. Its
31,457,280,000-byte SHA256 is pinned in usb_flash.py:
`06aad2b908ce008d37744c5171d8d10444d81c31bd7692c86f6dc01754337d20`.
The [laptop record](../brains/aetheri/journal/2026-11-08-laptop-image.md) links two
actual no-UART boots, six reviewed captures and independent filesystem/proof-file
inspection. This is development-image evidence, not physical hardware acceptance.

The target must match the independently selected Intenso model, serial212300100050,
USB bridge identity, complete size and512-byte logical sectors. Duplicate serials,
boot/system disks, read-only/offline devices and other buses/geometries fail.
The first-flash profile requires the inspected single original partition on G:.
Disk numbers are freshly discovered, never used alone as identity.

The later replacement profile is separate. On8 November the user explicitly
authorizes reflashing after the remaining tests and reconnects the same stick.
Its previous Aetheri layout has three independently fixed partition GUID/type/
offset/size bindings. Only the ESP has a Windows volume GUID; the two ext4
partitions must have no Windows access paths. No partition may be boot/system
or have an unexpected drive letter. Replacement locks/dismounts the exact held
ESP volume by GUID and joins its disk extent; it does not assume G: still exists.
The volume handle omits the GUID path's final backslash, following
[Microsoft's volume naming rules](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-volume).
It retains read/write sharing and requires a successful
[volume lock](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_lock_volume)
before dismount; busy/system/page-file volumes cannot be forced through this gate.
The source approval stays closed until complete-image acceptance. A new format2
intent binds the accepted replacement hash, original source hash and this renewed
authorization. The old completed format1 attempt must never be replayed.

The tool joins that observation to the held device's storage number, bounded
device descriptor (vendor/product/serial/USB/removable) and geometry/length.
The held G: volume must have exactly the independently expected extent on that
disk. These are controlled-host device observations, not cryptographic clone
authentication or a production StorageRef.

The reviewed checkout, current operator and Windows administrator are trusted.
The protected intent and held source prevent accidental drift/replay; they are
not a privilege boundary against a malicious checkout owner or administrator.
Device descriptors/partition GUIDs are not cryptographic clone authentication.
No credentials or secret boot arguments are recorded. A new intended image needs
its own accepted source pin and fresh operation; flags cannot select arbitrary
devices, images, mount paths or old completed attempts.

## Procedure

The prepared intent binds the source, script/image-reader hashes and exact USB
unique ID. The elevated tool requires administrator access before any attempt or
device open. It creates an exclusive attempt marker, retains one asset-wide host
lock and refuses an existing attempt. Unknown work is never automatically replayed.

The source is held read-only with write/delete sharing denied and its complete
hash is checked again. The tool then opens the physical device and volume,
verifies their binding, locks G: and dismounts it while retaining the lock. It
does not force a busy volume or change any internal disk. Device/partition context
is checked again immediately before the first write.

Every byte is written, including zeros. The sparse source reader may synthesize
kernel-confirmed zero holes; the physical target never uses sparse/zero-skipping
copying. The native buffer is64KiB-aligned and transfers1MiB blocks using
NO_BUFFERING and WRITE_THROUGH. A source digest during writing, explicit flush,
complete direct readback and repeated original-device checks must all pass.
Short/failed I/O is never retried or called success. Hardware cache behaviour and
power-loss durability are not certified by a successful readback.

An append-only, fsynced journal records dispatch before writing and progress at
256MiB boundaries. Image hashes and record-chain hashes have different fields.
Ordinary readers can inspect this log without the earlier JSON rename conflict.
The bounded reader validates complete records and flags an incomplete tail.
The final complete record follows verification and handle cleanup. Acceptance also
requires exit0 from the originally launched process; a historical log alone is
not live process, device or future-write authority.

Failure preserves the intent, marker and log. Cleanup attempts every owned handle
even if another close fails. No automatic repair, format, replay, reboot or driver
installation occurs. The user has already approved this target: Windows elevation
is an OS access step, not another request for authorization to erase the stick.
Credentials, if required, belong only in Windows' native dialog.

## Commands and tests

`python tools/usb_flash.py --inspect` queries metadata only. The observed held
descriptor matches Intenso/Speed Line/PMAP, serial212300100050, USB/removable,
31,457,280,000 bytes and the original G: extent.
Use `--inspect --replacement-layout` for the current three-partition metadata;
the old first-flash inspection deliberately refuses that different layout.

Writing requires `--write --intent PATH --intent-sha256 HASH` through the prepared
administrator launch. `--status --intent PATH` only reads the bounded journal;
it deliberately does not claim the original process has exited.

Run `python tools/test_usb_flash.py`. Fifteen Windows cases include actual
aligned unbuffered write/flush/full-readback on a new regular test file, overwriting
nonzero contents with zeros, malformed/wrong/ambiguous identities, extent/device
joins, short/failed/corrupt transfers, journal integrity/live reading and the
non-administrator guard, exact previous three-partition layout and closed/new
image-approval checks. No unit test opens/writes a physical device. Applicable
pure tests also run on Linux; the two Windows-specific cases are explicitly skipped.
Physical locking/writing/readback remains a separate executed boundary in STATE.

The first authorized physical attempt now passes on 8 November 2026: all
31,457,280,000 bytes are written and directly reread, the full SHA256 matches the
pinned source, cleanup succeeds and the originally held process exits0. The
[executed record](../brains/aetheri/journal/2026-11-08-laptop-image.md#physical-flash-completed)
retains the exact receipt bindings. This is not a physical laptop boot, secure
erase or permission to replay the completed operation after user files change.

Primary references: Microsoft [direct disk access](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew),
[unbuffered I/O](https://learn.microsoft.com/en-us/windows/win32/fileio/file-buffering),
[volume lock](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_lock_volume),
[volume extents](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-ioctl_volume_get_volume_disk_extents),
[device number](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-ioctl_storage_get_device_number)
and [device descriptor](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-storage_device_descriptor).
