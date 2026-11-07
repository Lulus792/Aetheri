# Persistent user-data boot proof

The [read-only system root](root-probe.md) now has a separate, writable ext4
development volume. A fixed UID/GID1000 service writes one document, the volume
is cleanly unmounted, and a second cold VM boot reads and advances that document.
The system image stays read-only throughout both boots.

This is the next image-integration step toward the laptop alpha. It is not a
login session, app sandbox, installer, production volume enrollment or recovery
protocol. The original development carrier and installed authorities are unchanged.

## Layout and ownership

| Path | Backing and purpose |
| --- | --- |
| `/` | Existing 512MiB GPT/UEFI image with a 256MiB read-only ext4 root |
| `/var` | New, separate 256MiB writable ext4 fixture volume |
| `/home` | Bind mount of that volume's `home/`; no second fixed-capacity home partition |
| `/home/aetheri/Documents` | UID/GID1000, mode0700; the single persistence document is mode0600 |
| `/tmp` | Fresh 64MiB bounded tmpfs, mode1777, nosuid/nodev |
| `/run/aetheri-data-user` | systemd-owned volatile RuntimeDirectory, UID/GID1000, mode0700 |

Only the needed directories are created. This small fixture does not allocate
future catalogues, private app DataEpochs or service state. Its ordinary document
must not be adopted as managed app storage. Fixed passwd/group entries provide
the test identity; their UID does not establish a durable Aetheri AccountId.

## Build and device admission

`tools/build_data_probe.py` extends the existing ordinary-user root builder.
It verifies the 13 executable/licence payloads in `data-probe-inputs.json`, using
the selected carrier's existing systemd-udev and util-linux families. Existing
package licences remain in the image. No package scripts, host device formatting,
mounts or installed-service changes are performed by the builder.

The new filesystem has an independently configured expected UUID and a fixed
20-byte virtio serial. Before any writable mount, the native root preflight
checks `/dev/vdb`'s block type, exact size, writable flag, ext4 magic, UUID and
actual kernel sysfs serial. UUID/serial are fixture observations, not clone,
publisher or durable-volume authentication. Do not select a production device by
these fields alone. Virtio's GET_ID limit is explicit in the
[QEMU11.1 header](https://raw.githubusercontent.com/qemu/qemu/v11.1.0/include/standard-headers/linux/virtio_blk.h).

The image includes the existing udev daemon plus one original block/systemd rule,
a trigger/settle service and standard `.mount` units. This deliberately narrow
rule set is not complete hardware discovery or module loading. See systemd259's
[mount contract](https://raw.githubusercontent.com/systemd/systemd/v259/man/systemd.mount.xml)
and [execution/directory contract](https://raw.githubusercontent.com/systemd/systemd/v259/man/systemd.exec.xml).

## Write and shutdown sequence

1. The original root diagnostic verifies actual ext4/read-only/EROFS behavior.
2. Device events settle; the separate volume check runs before `var.mount`.
3. `/var`, the `/home` bind and the bounded `/tmp` mount become available.
4. The supervised user service runs with NoNewPrivileges, an empty capability
   bounding set and umask0077. Native checks require actual UID/GID1000, zero
   effective capabilities and the expected volatile/durable directory context.
5. `Documents/boot-persistence.txt` contains one canonical line:
   `AETHERI-DATA1 <nonce> <counter> <boot-id>`. First creation is create-only;
   subsequent writes use a separate exclusive temporary file, file fsync,
   original-name/inode check, rename and parent-directory fsync. The temporary
   file is preserved after an uncertain write. Same-boot replay, noncanonical or
   oversized counts, changed owners/modes and unexpected links are rejected.
6. A separate root finisher rechecks the selected device and mounted filesystems,
   reads this boot's record, calls syncfs and unmounts `/home`, `/var`, then `/tmp`.
   Only a successful finisher allows the image-only immediate poweroff action.

The immediate poweroff remains diagnostic-only. Failure after a writable mount
does not claim a clean shutdown or permit an automatic next attempt. This is
clean-shutdown persistence evidence, not power-loss recovery or complete desktop
logout/suspend behavior.

This is a single-writer fixture. The pre-rename inode check is not an atomic
compare-and-swap and does not establish exclusion against another same-UID writer.
Normal broker/document concurrency and mutation fencing remain separate work.

## Execute the isolated proof

Build in the disposable Linux environment as the ordinary build user, using the
original four-file RAM boot input and already checked UEFI package directory:

```sh
python3 tools/build_data_probe.py --boot-image ORIGINAL_RAM_INPUT --packages CHECKED_PACKAGES --output NEW_IMAGE
```

On the Windows development host, with the previously verified QEMU tools:

```powershell
python tools/test_data_probe.py --image NEW_IMAGE --output NEW_SEQUENCE
python tools/test_data_probe.py --image NEW_IMAGE --output NEW_NEGATIVE --negative-volume
python tools/test_data_probe_contract.py
```

Use the configured Python runtime if `python` is unavailable. Each sequence owns
a new data copy and retains its original open file handle across both VM boots.
Held/named object identity and size are checked before/after dispatch. Every boot
gets new NVRAM; only the read-only system image and this owned writable file are
attached. No direct kernel/initrd/append, network, QMP, shared folder or physical
device is supplied. The same-process file observation is not restart admission.

Before dispatch, `sequence.json` preserves a pending fence. A failure or unknown
result stops the sequence; reusing its output directory is refused. Success
requires matching root/user/final markers, actual service completion, clean
unmount, poweroff and original-child exit0. Root PASS alone cannot satisfy it.
Counters1/2 must have distinct boot IDs. The negative case changes only a fresh
test copy's UUID, requires rejection before mounting and verifies unchanged bytes.
Independent read-only debugfs/e2fsck checks supply the separate on-disk evidence.

Remaining work includes the existing desktop/session in this image, production
identity/MAC/volume admission, module/firmware and physical laptop drivers, LUKS,
normal shutdown/suspend, updates/rollback, installer and complete app lifecycle.
