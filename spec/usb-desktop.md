# Single-medium USB development desktop

This joins the existing graphical image to the checked USB initrd and persistent
data on one medium. It is built as a NEW regular file by an ordinary Linux user;
the builder never opens a physical disk. Actual executed acceptance belongs in
STATE and the dated journal. Laptop graphics/input/firmware and physical acceptance
remain separate required work before the authorized flash.

## Layout and ownership

The selected geometry is the authorized Intenso's 31,457,280,000 bytes, independently
pinned in `spec/usb-desktop-inputs.json`. Both GPT copies describe the whole
medium; a small image with a backup table at the wrong physical end is not used.

| Region | Start | Size | Purpose |
| --- | --- | --- | --- |
| EFI partition | 1 MiB | 126 MiB | Unsigned fallback UKI, kernel and initrd |
| System partition | 128 MiB | 512 MiB | Original desktop/apps/runtime, mounted ro,noload |
| Data partition | 640 MiB | 30,785,142,784 bytes | Writable `/var` and home bind |
| Final reserved region | Last 1 MiB | 1 MiB | Alignment padding and backup GPT |

Fresh root/data identifiers derive from the selected build nonce and distinct
roles. Configuration originates in the builder and UKI/root files, never from
the candidate disk's own labels. UUIDs, diskseq and sysfs observations remain
fixture correlations, not durable volume/clone/account/publisher authentication.
The prototype has one fixed development account; normal login, production MAC/
app admission, LUKS and installer remain work.

## Checked sequence

The additive `AETHERI-USB2` profile binds root/data PARTUUIDs, filesystem UUIDs,
kernel, expected whole-device geometry and nonoverlapping extents. USB1 retains
its original fixed diagnostic geometry. Parsing is bounded/canonical, rejects
overflow, and changes the output only after complete validation. The USB2 runtime
observation binds every configured field as well as the original kernel facts.
Before handover, the initrd compares the candidate root's complete configuration
against its independent input. Matching names, filesystem metadata or the nonce
alone cannot select a different data configuration.

The original initrd checks run before read-only root mount, after mounting and
after udev stops. Standard systemd SwitchRoot preserves `/run` and the original
observation. In the real root, `--data` repeats root/device checks, requires the
original boot/diskseq/sysfs-parent observation, checks the separate data partition
and requires the same current parent disk before any writable data mount.
`--data-mounted` then verifies actual root read-only, `/var` and `/home` on the
expected read-write ext4 device, with nosuid/nodev. Only this success permits
the VT/desktop path. No second virtual disk or `/dev/vdb` assumption remains.

These helpers only open block devices read-only. systemd performs the selected
mounts. The old data-only diagnostic executable/config are retired only inside
the fresh build tree; no installed carrier file or production app is removed.
The independently reviewed libbpf runtime and actual device-denial startup
probes remain part of the graphical root. The kernel still uses the selected
development software-rendering/virtual-display profile until laptop integration.

## Host verification

The independent reader streams hashes and bounded GPT/FAT/PE structures instead
of loading a 31 GB file into memory. Windows test copies use explicit sparse-file
marking and SetEndOfFile, avoiding large CRT zero-fill operations; allocated-size
and zero-read checks run against regular test files. Sparse copying is never a
physical-media writer: a later flash must write and verify every intended byte.

The VM controller owns one fresh writable medium copy and its held file identity,
fresh NVRAM and the original QEMU handles. QMP stays protected host-local AF_UNIX,
with UUID admission on each connection. No network/shared folder/physical disk or
direct kernel injection is used. It requires ordered same-boot initrd/root/data
and device-filter observations, actual app input, a document across two cold
boots and normal unmount/poweroff. System/EFI/GPT ranges outside the data partition
must remain byte-identical after each boot. Failed/unknown copies are retained,
never silently replayed. Whole-medium RW access in this VM is not hardware
write protection; this is still a development profile.

Run `python tools/test_usb_desktop_contract.py` for isolated I/O/layout/result
guards and the native C initrd adversaries in Release/ASan/UBSan. Build with
`tools/build_usb_desktop.py` and explicitly run `tools/test_usb_desktop.py` against
its `media` directory. Existing RAM/root/UEFI/data/desktop tests remain separate.
No runtime acceptance follows from documentation or compilation alone.

The separate `prepare_usb_parent_negative.py` creates two new owned copies and
changes only selected GPT GUIDs/CRCs. The primary keeps the expected root but
loses the expected data PARTUUID; the external copy has the expected data and a
different root/disk GUID. Filesystem bytes remain unchanged. The explicit
`test_usb_parent_negative.py` must observe actual handover followed by data-parent
rejection before any data mount/GUI, normal poweroff and unchanged full bytes on
both media. Its isolated evaluator/GPT tests do not prove that actual VM result;
consult STATE for execution status. Incomplete preparations are never reused.
The pair preparation/controller require two full logical media plus 8 GiB free
headroom before new files. This does not reserve space against other host writers;
any later failure still preserves the incomplete attempt and forbids replay.

Primary references: systemd259
[SwitchRoot](https://raw.githubusercontent.com/systemd/systemd/v259/src/shared/switch-root.c),
Linux6.19 [block ioctls](https://raw.githubusercontent.com/torvalds/linux/v6.19/block/ioctl.c),
Microsoft [sparse-file marking](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_sparse),
[file length](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setendoffile)
and [actual allocation size](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getcompressedfilesizew).
