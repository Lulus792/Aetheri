# Read-only ext4 system-root boot — 7 November 2026

Aetheri's combined UEFI image now starts systemd and the original diagnostic
from its independent ext4 system root. Native observation confirms ext4,
ST_RDONLY, the actual mounted device's selected PARTUUID and a create attempt
failing specifically with EROFS. A RAM-root result cannot satisfy this profile.
Read the [contract](../../../spec/root-probe.md).

The full OS goal remains active: this image has selected userspace and one
diagnostic, not the desktop, writable user data, production policy, installation,
updates or hardware acceptance. No protected old runtime is adopted or settled.

## Chosen boot and construction boundary

The original kernel configuration confirms ext4, virtio PCI/block and devtmpfs
built in. Its exact recorded hash is checked. A tiny archive has no /init; the
kernel's existing root mounting selects PARTUUID and ro,noload, then executes
systemd from ext4. This proves a kernel root-mount path, not a userspace initrd
handover. USB storage is modular in this kernel; laptop USB-root, LUKS unlock and
complex storage still require an appropriate early-userspace profile.

One new 512MiB regular GPT file has the existing126MiB ESP and a separate256MiB
Linux x86-64 root partition with its read-only attribute. The builder copies the
ESP volume rather than stale MBR/backup GPT bytes and leaves zero ranges sparse.
The rest is reserved test-image space, not an additional filesystem. Formatter
commands accept only this private new output; nothing is mounted on the carrier
or Windows host, and no physical/old carrier disk is attached to the test VM.

Only admitted original runtime/licence files populate the root. Obsolete RAM
diagnostic/units/profile are removed. The new C17 compile-time root mode checks
the actual filesystem/device/write outcome; its normal mode must compile to
the exact original RAM diagnostic ELF bytes. Both builds enforce that comparison.
Strict warnings and GCC's static analyzer pass. No full positive ASan/UBSan boot
or general memory-safety proof is inferred from those checks.

The builder runs UID1000 in a separate private namespace on the Enforcing
carrier. Existing e2fsprogs1.47.3-4.fc44 populates a new ext4 file. debugfs sets
and independently reports UID/GID0 for all72 selected inode paths; forced
read-only e2fsck passes afterward. The private mke2fs configuration copy is used
explicitly under C locale, with SHA256
`c998d64f9b58c2235186f8ee75ba602d1fdb2991d94695fd687ab0e8096b6727`.
Exact compiler/e2fsprogs hashes/releases, root file/link inventory and filesystem
digest are retained in the manifest. Existing licences are preserved.

The new root has no production SELinux policy. The carrier's Enforcing result
does not certify it. API filesystems and systemd's transient machine-id bind
remain volatile/writable; root filesystem read-only is not a claim that every
path or privileged operation is immutable. Fixture GUIDs/UUIDs/nonce do not
authenticate volume clones, publishers, DataEpochs or accounts.

## Native artifacts and tests

Initial build-v1 token `7ab370e7ce6d4db6be39dfcc944337ef`, helper1050092,
handle18208 is terminal0,269 members verified/retired. Its two builds pass; the
first actual virtio root boot passes in16.329s (PID58008, handle95900), boot
`56ad6ca9-c79e-4e77-b2d8-790b6cd67f61`. The first export's random-access gzip
verification is slow on the large image members. An additional read-only
single-stream verifier checks the original root-protected token/source/export
binding and all members before extracting selected inputs; it does not issue
an extra acknowledgement. The original controller later acknowledges and
retires its exact namespace. No build is replayed because an observation is slow.

Build-v2 adds complete root inventory/tool evidence, the matching UKI OS title
and profile regressions. Token `fdabf499a55f44bb85697d5b2717139b`, helper1052191,
handle89306 is terminal0 with271 verified members. Actual boot-v2 (PID8832,
handle11338) passes in16.656s, boot `320f1037-c300-40cf-9a4e-7ce6fd4b2685`.
Its explicitly bounded kernel/configuration scope remains unchanged.

Final build-v3 captures/uses the exact formatter configuration and C locale.
Token `4812035b26f8493ca8aae452adaadc37`, helper1052388, handle65546 is
terminal0. All18 original code/config-pin/prior-boot/package inputs are bound.
Source archive SHA256:
`59606e3f6ac31021b5c77b01f8e39f207070d736fd4138e894f0efcad73ec7ab`.
Positive and negative builds pass in9.365s/6.512s. Five root contracts and five
prior boot-image contracts pass in the native Linux fixture; Windows also passes
the five root cases and all seven older UEFI cases. Exact extents/identities,
filesystem UUID/content, actual fallback resources, fixed no-direct-boot command,
and rejection of RAM/foreign/mixed root results are covered.

All273 members are independently streamed/hashed before the unchanged protected
token/source/archive acknowledgement and namespace retirement. Final archive
SHA256 `8bb95568e5d71cefc720d2ccbe2618a8f8629af32fb3c0389a9fb690cf5f10ce`
(280,464,298 bytes). The optimized verification checks every member name, digest
and complete set; it does not reduce coverage or change retirement semantics.

Final normal raw image SHA256:
`87cd13f86f45faede502310ebbdc4ed9edbd09069be26f4b308a372d3d85aa10`.
Its root filesystem SHA256:
`945cb121f7d057b72a3b992ad4045ec515470866193bbb5b1c21de0cb607a15f`.
Filesystem/directory creation timestamps remain variable; whole-image
reproducibility is not claimed. Only verified selected members are extracted
for the host tests; the build/runtime evidence stays in
`local/root-probe-2026-11-07`.

## Final real cold boots

Each test verifies original QEMU/DLL/UEFI artifacts, copies fresh NVRAM and
attaches only its selected read-only combined image as virtio block storage.
There is no direct kernel/initramfs/append injection, network, QMP, shared folder
or additional system/data drive. q35/TCG uses two vCPUs/768MiB. The literal
configured command is the drive inventory; the launch diagnostic's zero-disk
count concerns additional drives, not the selected combined boot/root artifact.

| Original final attempt | Actual result |
| --- | --- |
| boot-v3, PID66072 | PASS, service success/kernel poweroff/exit0 in14.453s; boot `04cbd100-f886-4524-9998-ebb156a6fc3e` |
| boot-negative-v3, PID65484 | Wrong root-profile nonce rejects, no PASS, service failure/poweroff/exit0 in18.047s |
| boot-cold-v3, PID65352 | PASS again in14.844s; distinct boot `4945dcbe-9efe-49f5-9f62-733398bef053` |

The final matrix's original host handle3221 is terminal0. All older build/boot
handles are terminal too. Unknown or slow observations never permit dispatch
replay. An EROFS result, service outcome and poweroff are all required; a PASS
from the older RAM profile or a process exit alone cannot settle this proof.

The read-only audit preserves445 normal guest inputs,31 frozen SQL/XML,19 old
quarantine hashes,52 normal P0/controller files, the authority image, old SDK/SDL
and original CLOSED DRM history. Normal capability advertisements stay empty.
No developer-disk attachment, foreign work retirement or physical installation
occurs. The four unrelated planning/design files remain excluded from publication.

Next integrate deliberate writable runtime/data mounts and the existing Aetheri
desktop/session. Preserve normal app/document admission/recovery/removal, driver/
firmware, hardware/suspend, installer/encryption, signed updates/rollback and the
complete resettable experiment desktop as remaining requirements.
