# First original initramfs boot — 7 November 2026

The next laptop-alpha delivery step now has an actual diskless boot result.
Aetheri's new ordinary-user builder assembles selected pinned kernel/userspace
files rather than cloning the development carrier. UEFI starts the Linux kernel,
systemd becomes PID1 and starts the original C17 diagnostic. The positive test
verifies the exact profile/command-line nonce, expected kernel/x86-64, UEFI,
RAM-backed root, API mounts and original service cgroup, then powers off through
the image-only systemd unit. Read the [contract](../../../spec/boot-probe.md).

This proves the direct QEMU kernel/initramfs path through UEFI. It does not yet
prove a standalone removable-media artifact, desktop, persistent installation,
production SELinux confinement, Secure Boot, updates or laptop hardware.
Normal systemd remains the selected supervisor; no original PID1 is substituted.

## Selected files and build

The explicit pin contains 17 runtime/kernel/config files and 21 licence files.
Every original payload SHA256 matches the relevant owning RPM file metadata,
including shared licence ownership and symlink targets. The audit initially
encountered RPM ghost `/boot` files without stored payload hashes, then correctly
selected the actual `/lib/modules` package payload. Empty symlink digests and
duplicate identical shared-licence metadata are distinguished from content.
The new input pins do not invent vendor signatures or original volume evidence.

Kernel 6.19.10, systemd259.10, glibc2.43 and libcrypto3.5.9 reuse the existing
carrier versions. The required early-mount closure adds the existing libmount/
libblkid2.41.3, libselinux3.11 and PCRE2 10.47 payloads explicitly. Nothing is
installed or updated in the carrier. Their exact paths, versions, sizes and
hashes are in the committed pin; a production security/source-distribution review
remains separate. No BPF feature is exercised or silently enabled.

The source build runs UID1000 in a root-provisioned private 768MiB work mount and
256MiB `/tmp`; the carrier remains Enforcing. Two new output directories use the
same original nonce and produce byte-identical kernel/initramfs/command-line
payloads. Fixed archive ownership/metadata require no builder root privileges.
The original C diagnostic compiles with all strict warnings enabled and rejects
ordinary unprivileged development-host execution. It has no reboot syscall;
only the image's unit can request poweroff, and that unit is never installed in
the carrier. No claim of cross-toolchain reproducibility follows.

Final build-v3 token `fd7b52ba434a4de286534afcba8e7c89`, root helper1042779,
host handle14254 terminal0. Source archive SHA256:
`187334646de4f2d1d3bd1bb4f46aa92fc450b0cd7903930b079db33f667b3a35`.
All 105 evidence members are independently host-verified before exact original
root-protected token/source/archive acknowledgement and namespace retirement.
Export SHA256:
`1951a26ae1a4171d5bad25e9aad16761c8e2dbdc7e0d11c34c9cff65a24e6154`
(74,807,020 bytes). Only the four verified boot inputs are extracted for QEMU.
The initramfs is 9,634,836 bytes, SHA256
`c0ebd483fb296cd7640f7a61edbf0b45c8d47e8991f38687e83430d8a4e67447`.
At that checkpoint all four source/pin/builder/runner inputs match its intent.

The final publication build normalizes the JSON pin to the repository's required
LF endings and includes the independent contract suite in its bound inputs.
Build-v4 token `5cf558a9f118430181d6b9e58b1ebfb2`, helper1043942/handle35320,
is terminal0. Its source archive SHA256 is
`0e735c2877023e79b5d30228ce762f51ac0abca1bf75dbc790dc7c1f763c54cd`.
All five current executable/build/pin/test inputs match this original intent.
Both native builds and all five contracts pass again, with 106 independently
verified evidence members before exact retirement. Final export SHA256:
`c78bd7f6c39738e516073a8af1f94c480c70232f343f13eb48782878283a1f67`
(74,807,250 bytes). Using the explicit original nonce, all three boot payloads
are byte-identical to the v3 image actually cold-booted below. Only manifest/build
provenance differs; the successful/negative boot evidence remains attributable
to those exact bytes. No extra boot is inferred from the repeated build.

## Actual cold boots

The unchanged extracted QEMU11.1.0 executable, all adjacent DLLs and UEFI code/
NVRAM template match their original extraction receipt. Each q35/TCG run has two
vCPUs, 768MiB, a new exclusive output and fresh NVRAM. No system/data disk,
network, shared directory, QMP or management listener is attached. Firmware code
is read-only; the private NVRAM is the only writable drive. The existing carrier
is independently confirmed live before the work and never restarted.

| Original attempt | Actual result |
| --- | --- |
| boot-v1 / PID59192 | Early mounts fail without the dlopen libmount closure; original child times out at120.141s and is retired through its retained handle |
| boot-v2 / PID65324 | Reaches systemd, then manager allocation fails without its mandatory executor; exact original child times out at120.156s and is retired |
| boot-v3 / PID61984 | PASS, QEMU exit0, 14.031s; boot `a967ae87-feb9-47e8-b786-696eb84bfef8` |
| boot-nonce-v3 / PID66672 | Deliberately changed command-line nonce yields exactly FAIL reason=nonce, no PASS; image unit powers off, QEMU exit0 in24.640s |
| boot-cold-v3 / PID63416 | Same image cold-boots again: PASS, exit0,13.843s; distinct boot `05db6d6e-9ba6-4891-99ee-2913c0f4a792` |

No timeout is treated as success, and no attempt directory is replayed. The
negative result is evidence that firmware/kernel startup or QEMU exit0 alone
cannot satisfy the image's original profile. Optional libbpf absence remains
visible in systemd's log, with no sandbox/BPF acceptance claim.

Five independent host tests cover actual newc parsing, changed boot payloads,
manifest disagreement, traversal/absolute names, duplicate files/directories,
foreign ownership, symbolic-link escapes, special devices, archive tails and
exclusive no-spawn/no-overwrite of a prior attempt. All pass. These are bounded
format/integrity checks, not publisher verification or production admission.

Evidence, independent manifests, logs, exact commands and retired dispatches live
under `local/boot-probe-2026-11-07`. Earlier build-v1/v2 exports remain verified
and retired; their missing-file startup evidence is retained rather than erased.

The final read-only carrier audit preserves all 445 normal source inputs, 31
frozen SQL/XML contracts, 19 quarantined hashes, 52 normal P0/controller files,
the original authority hash, prior SDK/SDL and CLOSED DRM history. The four
foreign planning/design files remain excluded. Neither live foreign NexaUI work
nor the prepared/unattached developer disk is restarted, attached or altered.

## Next

Produce a standalone UEFI removable-media artifact and boot it through firmware
without QEMU's direct kernel/initramfs options. Follow with read-only system-root
assembly, normal services and the existing desktop/session integration. Keep
updates/rollback, normal app/document lifecycle, laptop drivers/suspend and the
full resettable experiment desktop as explicit acceptance work. Responsive editor
loading/folder scans also remain unfinished; UI refinement is not a prerequisite
for the first real boot artifact. The full OS goal remains active/incomplete.
