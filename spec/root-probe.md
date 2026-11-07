# Read-only system root proof

The [standalone UEFI medium](uefi-probe.md) now has a separate ext4 system root.
One 512 MiB GPT image contains the EFI partition and a 256 MiB Linux root
partition. The selected kernel mounts that partition read-only and starts
systemd from it. The original C17 diagnostic runs from the real root, confirms
its mount/device context and requires a write attempt to fail with `EROFS`.

This first system-base assembly contains selected runtime files and one
diagnostic. It does not yet include the Aetheri desktop, a writable user-data
volume, normal app admission, production SELinux policy or an installer.

## Boot profile

The exact already selected kernel configuration has ext4, virtio PCI/block and
devtmpfs built in. Its hash is checked before construction. That permits the
kernel's existing root-mount path: a tiny initramfs has a marker and `dev/`, with
**no `/init` program**. The kernel selects the root by its explicit PARTUUID,
mounts ext4 with `ro,rootflags=noload`, then executes the root's systemd binary.
No replacement PID1 or hand-written early-userspace mount/switch-root service is
introduced. This is not evidence of an initrd userspace handover.

This bootstrap choice is specific to these builtin drivers. USB storage is a
module in the selected configuration. Laptop USB-root support, firmware loading,
LUKS unlock and more complex storage still require an appropriate early-userspace
profile; they are not implemented by this shortcut. The current test attaches
the combined image as a read-only virtio block device and lets UEFI discover its
normal fallback EFI application. No direct QEMU kernel/initramfs/append input,
network, shared directory or other system/data disk is supplied.

`noload` matters: a nominally read-only ext4 mount can otherwise replay its
journal. The prototype uses only a clean filesystem that passed `e2fsck -fn`,
suppresses replay and also exposes the complete image read-only at the VM layer.
These are test-image protections, not signed boot, a production integrity tree
or protection against every privileged in-guest action.

## Assembly and ownership

On the original x86-64 carrier, as an ordinary user in a fresh private workspace:

```sh
python3 tools/build_root_probe.py --boot-image /private-work/original-boot --packages /private-work/rpms --output /private-work/system-first
```

The inputs are the four original RAM-boot artifacts and the same three signed
RPMs from the UEFI recipe. The builder reconstructs only their admitted bounded
root archive, removes the obsolete RAM diagnostic/unit/profile and builds the
root diagnostic with the existing C17 toolchain and static analysis. It separately
compiles the normal RAM profile and requires its ELF bytes to remain identical
to the original tested binary. The compile-time root mode changes no installed
authority or public interface.

The root contains systemd/executor, the already selected libraries and licence
texts, basic root account files, mount-point directories and the explicit root
diagnostic/units. `root_inventory` records file hashes and symbolic links. There
is no copied distribution desktop, package manager, inherited personal data or
unexplained background service. The diagnostic unit powers off this test image
immediately; it is not the normal desktop/session shutdown policy.

`mkfs.ext4 -d` populates a **new regular file**, never a supplied device. The
builder captures `/etc/mke2fs.conf` into its private output, explicitly uses that
copy and records its digest. It records the exact e2fsprogs/compiler executable
hashes and package releases. debugfs normalizes each selected inode's UID/GID to
root and then independently reports every expected inode; missing reports or
non-root ownership reject the result. A read-only forced filesystem check must
pass before packaging. No filesystem is mounted on the carrier or Windows host.
The filesystem's standard `lost+found` directory is reserved for repair.

The complete GPT image has:

| Region | Purpose |
| --- | --- |
| EFI partition, 126 MiB at1 MiB | Existing FAT32 layout and fallback UKI |
| Root partition, 256 MiB at128 MiB | Selected ext4 system files; GPT read-only attribute |
| Remaining space | Reserved test-image space and backup GPT; no extra filesystem |

The builder copies only the verified ESP volume, not an old protective MBR or
obsolete backup GPT. Sparse zero ranges remain holes in its private output.
Disk/partition/filesystem GUIDs and nonce are explicit fixture correlations;
they do not authenticate a cloned volume or replace deployment/publisher trust.
Creation timestamps are not normalized, and whole-image bit reproducibility is
not claimed. A separately bound exact digest identifies each built artifact.

## Native acceptance

```sh
python tools/test_root_probe_contract.py local/system-image
python tools/test_uefi_probe.py --image local/system-image --output local/system-attempt
```

The independent media reader checks both GPT copies, the second partition type,
extent/read-only attribute, ext4 magic/UUID and exact filesystem bytes, in
addition to the existing FAT/PE/kernel/initramfs checks. It does not claim to
replace a full filesystem checker or authenticate a candidate's own manifest.

After actual kernel mounting, the C17 diagnostic requires:

- The expected nonce/kernel, UEFI, systemd parent and original service context.
- An ext4 `/` with the kernel's read-only filesystem flag.
- The actual mounted device's PARTUUID matching the unique kernel root argument.
- A create attempt at the fixed absent root path failing specifically with
  `EROFS`, rather than merely a permission error.
- The expected proc/sys/run/cgroup-v2 mounts and a fresh kernel boot ID.

A RAM-root PASS cannot satisfy this profile. Acceptance also requires a successful
root service, kernel poweroff and QEMU exit0. The observer rejects foreign or
mixed result markers. A separate `--wrong-nonce` builder fixture must boot far
enough to reject its changed root profile, fail its service and power off without
PASS. It has its own output/VM attempt. Cold retries always use new NVRAM.

Read-only `/` does not make every pathname immutable: systemd supplies volatile
API filesystems and a transient machine-id bind mount. This prototype has no
persistent account identity, user-data ownership journal, production MAC policy,
trusted update/rollback or complete launch/removal semantics. The enforcing
development carrier and its protected old state remain a different environment.

Next add a deliberate writable runtime/data layout and integrate the existing
session/desktop into this booted system base. Driver/firmware coverage, normal
security/lifecycle, hardware/suspend, installation, signed updates/rollback and
the full experiment desktop remain acceptance work.

References: [kernel initramfs/root fallback](https://www.kernel.org/doc/html/latest/filesystems/ramfs-rootfs-initramfs.html),
[kernel command-line parameters](https://docs.kernel.org/admin-guide/kernel-parameters.html),
[ext4 mount options](https://docs.kernel.org/admin-guide/ext4.html),
[mke2fs](https://man7.org/linux/man-pages/man8/mke2fs.8.html).
