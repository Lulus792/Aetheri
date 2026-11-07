# Diskless UEFI boot proof

This builds Aetheri's first small initramfs and boots it with a separately
selected Linux kernel under UEFI. systemd is PID 1 and supervises an original
C17 diagnostic. It is the first boot integration step toward the laptop alpha.
It does not contain a desktop, installer, production SELinux policy, persistent
data or an update mechanism. The existing development carrier is not the image.

## What is in it

[boot-probe-inputs.json](boot-probe-inputs.json) pins 17 existing carrier files
(kernel/config, systemd/executor and their selected libraries) and 21 original
licence files. Each payload digest was checked against its owning RPM metadata.
The kernel is the already tested Fedora 6.19.10 build, systemd is 259.10, glibc
is 2.43 and libcrypto is the accepted 3.5.9. These are development input pins;
they are not a new production-base selection or a current security assessment.
RPM database comparisons establish correspondence to that trusted carrier's
metadata, not fresh repository-signature verification or volume provenance.

Only selected files are copied. There is no carrier disk clone, package manager,
network configuration, login service or inherited personal data. Library symlinks
are flattened to their pinned payload at the image's library name. `/lib64`
points to `/usr/lib64`; `/init` points to the selected systemd binary.

systemd's early mounts require dynamically loaded libmount and its libblkid,
libselinux and PCRE2 dependencies. Its manager also pins `systemd-executor` before
starting services. ELF `NEEDED`/`ldd` alone did not expose these requirements.
The actual failed initial boots and subsequent positive boot are retained.
Optional BPF support is absent and reported by systemd; this image does not test
production sandbox enforcement. Its settings do not change carrier policy.

Two small generated units start the boot diagnostic. It checks its root/systemd
parent, the exact image/kernel-command-line nonce, x86-64/kernel release, UEFI,
RAM root, proc/sys/run/cgroup-v2 mounts and its supervised service cgroup. It
prints one correlated result and exits. The diagnostic itself has no reboot,
mount or installation effect and rejects ordinary development-host invocation.
The image-only unit then requests immediate poweroff. Immediate poweroff is
appropriate only for this diskless proof, never the normal user shutdown design.

## Build and inspect

On the original x86-64 Linux carrier, as an ordinary build user in a separately
owned work directory:

```sh
python3 tools/build_boot_probe.py --output /private-work/boot-first --nonce 0123456789abcdef0123456789abcdef
```

Choose a new random 32-character lowercase hexadecimal nonce for each build
attempt. Repeating that exact nonce in a second **new** output directory allows
same-toolchain byte comparison. The nonce correlates a test; it is not a secret,
signature, authorization capability or durable machine identity.

The builder checks every pinned source before reserving its exclusive output,
compiles the diagnostic with strict C17 warnings, and emits:

| Output | Purpose |
| --- | --- |
| `vmlinuz` | Exact selected kernel payload |
| `initramfs.cpio.gz` | Ordered `newc` root archive, fixed times/ownership and deterministic gzip |
| `cmdline.txt` | Explicit console, init, target and original test correlation |
| `manifest.json` | Source/pin/output hashes, root objects, compiler command and tool versions |
| `root/`, `compiler.log` | Inspectable build inputs/results |

Existing output directories are never reused. Build failure preserves the
attempt. There is no root requirement, package installation, device formatting or
mount operation. The observed two builds produce identical boot payloads on the
same compiler/toolchain; independent-toolchain reproducibility remains untested.

## Cold-boot test

Export the four boot inputs only after independently verifying their archive
members and original build binding. On the current Windows development host:

```sh
python tools/test_boot_image_contract.py
python tools/test_boot_probe.py --image local/boot-image --output local/boot-attempt
python tools/test_boot_probe.py --image local/boot-image --output local/boot-wrong-nonce --expect-nonce-failure
```

The independent archive parser checks actual decompressed `newc` entries,
ownership/times, file/link metadata, bounded content and exact manifest agreement.
It rejects modified kernel/initramfs/command-line bytes, traversal, duplicate
entries, special devices and unexamined archive tails. Those checks are integrity
and format evidence, not publisher trust or boot authenticity.

The runner verifies the original extracted QEMU/DLL/firmware hashes and starts a
new q35/TCG VM with two CPUs and 768 MiB. Each attempt has a fresh copied NVRAM.
It attaches **no system/data disks, network, QMP, monitor or shared directories**.
Only the selected read-only firmware and private NVRAM pflash are drives. The
kernel/initramfs use QEMU's direct boot input through UEFI. This is not yet an
ESP/UKI, standalone disk/USB image, Secure Boot or removable-media test.

Each exclusive attempt retains command, tool hashes, process/terminal status,
serial output and elapsed time. Timeout retires only the original still-owned
QEMU child handle; an existing directory is never silently replayed. A successful
result requires QEMU exit 0 and exactly one nonce/kernel-correlated native PASS.
The negative case must produce exactly the expected nonce failure and no PASS;
exit 0 alone is never success. A second positive cold boot must have a different
kernel boot ID. No production app lifetime is settled by these tests.

## Next delivery boundary

The [standalone UEFI medium](uefi-probe.md) now packages these exact boot inputs
and has actual removable-media firmware boot evidence, without `-kernel`/`-initrd`.
Next add an independently built read-only system root, selected system services,
normal session/desktop integration and controlled writable data. Review exact
boot/update formats, dependency security and redistribution/source requirements
before a public binary release. Retaining licence texts alone does not complete
that review. Installer, rollback, laptop drivers, suspend/resume and the resettable
experiment desktop remain explicit work.

Sources: [Linux initramfs documentation](https://www.kernel.org/doc/html/latest/filesystems/ramfs-rootfs-initramfs.html),
[Linux EFI stub](https://www.kernel.org/doc/html/latest/admin-guide/efi-stub.html),
[systemd v259 early mount handling](https://github.com/systemd/systemd/blob/v259/src/shared/mount-util.c),
[manager executor pin](https://github.com/systemd/systemd/blob/v259/src/core/manager.c),
[unit action semantics](https://github.com/systemd/systemd/blob/v259/man/systemd.unit.xml).
The exact carrier payloads remain the pinned build inputs; v259 source links
explain the relevant mechanisms without asserting byte equality to vendor patches.
