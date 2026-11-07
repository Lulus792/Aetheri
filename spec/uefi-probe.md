# Standalone UEFI boot medium

This packages the [original initramfs boot proof](boot-probe.md) into a real
128 MiB GPT/FAT32 image. Firmware discovers `EFI/BOOT/BOOTX64.EFI` through the
removable-media path. That EFI application contains the kernel, initramfs and
command line; QEMU no longer receives separate boot payloads.

The image starts systemd and the bounded original diagnostic, then powers off.
It has no desktop, persistent user data, installer or production security policy.
This is the next delivery step toward the laptop alpha, not its completion.

## Components and trust

[uefi-probe-inputs.json](uefi-probe-inputs.json) records three exact signed RPMs:

| Component | Version | Role |
| --- | --- | --- |
| systemd-boot-unsigned | 259.10-1.fc44 | The x86-64 EFI stub executed by firmware |
| systemd-ukify | 259.10-1.fc44 | Official development tool that assembles the EFI file |
| python3-pefile | 2024.8.26-7.fc44 | Build-only PE handling required by ukify |

The systemd version matches the existing prototype. `ukify` is preferred over a
custom PE assembler because it handles section layout/alignment and offers a
future signing path. pefile is used only by the private build tool; Python and
these development scripts are not added to the boot runtime. The explicit
unsigned/non-measuring build supplies the kernel release, so ukify's optional
cryptography/zstandard code paths are not needed. This is a bounded extracted-tool
workflow, not a package-manager installation satisfying every broader RPM feature.

The builder checks whole-package size/SHA256, verifies each RPM against the
existing Fedora44 trusted key and checks selected extracted payloads against
their pinned hashes. It does not execute install scripts or copy unselected EFI
boot managers. Package metadata, selected licences and exact payload hashes are
inspectable. Existing systemd licence texts remain inside the original initramfs;
pefile's MIT licence is retained in the private tools prefix.

**The RPM signatures are valid; the generated EFI application is unsigned.**
These are different signatures. There is no Secure Boot, TPM/PCR validation,
publisher-admission or production-security acceptance from this proof. A dated
dependency review covers the exact signatures/licences, same-version compatibility
and the rootless/trusted-input build boundary. Production advisory/source-release
review remains required. No host firmware setting is changed.

## Build

On the original x86-64 Linux development carrier, as an ordinary user in a new
private work directory, supply the four independently verified original boot
inputs and the three cached RPMs named in the pin:

```sh
python3 tools/build_uefi_probe.py --boot-image /private-work/boot-inputs --packages /private-work/rpms --output /private-work/media-first
```

The builder reserves a new output, extracts only pinned files into its private
`tools/` prefix, supplies an explicit empty ukify configuration and builds
`BOOTX64.EFI`. It uses already available sfdisk, dosfstools and mtools to create
`boot.raw`. Their actual executable hashes and owning package versions are
recorded. No build command accepts a device, modifies partition tables on a host
disk, mounts a filesystem, installs a service or updates firmware.

| Artifact | Contents |
| --- | --- |
| `BOOTX64.EFI` | AMD64 EFI application with `.linux`, `.initrd`, `.cmdline`, `.osrel` and `.uname` resources |
| `boot.raw` | Protective MBR, primary/backup GPT, one 126 MiB FAT32 ESP starting at1 MiB, and the fallback EFI file |
| `manifest.json` | Exact input/source/tool/output hashes, partition identities and embedded resource hashes |
| `logs/`, `tools/`, cached RPM copies | Signature, extraction, formatting and readback evidence |

The fixed disk and ESP GUIDs derive deterministically from the explicit test
nonce; these are image metadata, never durable volume or clone authentication.
The builder rereads the fallback file through mtools and compares it with the
original EFI file. Two builds must reproduce the UKI byte for byte. FAT directory
creation timestamps are not normalized, so whole-image bit reproducibility is
not claimed; each complete image has its own recorded digest.

## Independent verification and actual firmware boot

```sh
python tools/test_boot_image_contract.py
python tools/test_uefi_probe_contract.py local/uefi-image
python tools/test_uefi_probe.py --image local/uefi-image --output local/uefi-attempt
```

`uefi_probe_image.py` independently reads GPT headers/arrays and their checksums,
the mirrored FATs and bounded file chains, the exact `EFI/BOOT/BOOTX64.EFI` path,
and PE headers/section bounds/resources. It invokes no formatter, filesystem
mount, PE loader or external helper. Corruption checks include independently
rechecksummed wrong partition geometry, FAT disagreement/cycles, duplicate/
overlapping PE sections and a foreign EFI architecture. A self-consistent manifest
does not establish publisher trust; the original build/export binding is separate.

The host runner verifies the original extracted QEMU/DLL/firmware hashes and
attaches only the selected **read-only** raw image as removable USB storage,
read-only firmware and a new private NVRAM copy. There are no `-kernel`, `-initrd`
or `-append` arguments, system/data disks, shared folders, network or QMP endpoint.
It never reuses an attempt directory or implicitly replays an uncertain process.
Timeout retirement uses only the original still-owned child handle.

Acceptance requires a correlated native PASS, successful diagnostic-service exit,
kernel poweroff and QEMU exit0. Mere firmware output, a kernel banner, a PASS from
another profile or exit0 alone cannot satisfy it. The same observer also evaluates
the older direct boot proof, whose original serial records remain historical.

A separate deliberately wrong embedded command line must yield exactly the
expected nonce rejection and failed service, no PASS, followed by poweroff. It is
built from an explicitly changed test input; the normal builder has no fault hook.
Run it only with `--expect-nonce-failure` and a separate owned attempt directory.
Repeated cold boots use fresh NVRAM and must report different kernel boot IDs.

## Next

Assemble an independently built read-only system root and integrate the existing
Aetheri session/desktop into that image. Then test normal writable data, drivers
and device settings on the laptop. Installer, encrypted storage, signed updates,
rollback, production app/document lifecycle, suspend/resume and the full
resettable experiment desktop remain separate acceptance work. The boot medium
does not implement them by containing a working kernel.

References: [UAPI UKI specification](https://uapi-group.org/specifications/specs/unified_kernel_image/),
[systemd-stub v259](https://github.com/systemd/systemd/blob/v259/man/systemd-stub.xml),
[ukify v259](https://github.com/systemd/systemd/blob/v259/man/ukify.xml),
[Fedora pefile package](https://packages.fedoraproject.org/pkgs/python-pefile/python3-pefile/fedora-44.html).
Exact vendor payloads and hashes are in the pin; upstream documentation does not
assert byte equivalence to vendor patches.
