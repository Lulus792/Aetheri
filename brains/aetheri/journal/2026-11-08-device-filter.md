# Actual development-image device filtering

8 November2026. The full OS goal remains active. C/build/Linux/dependency skills
were used; no subagents, CUA, physical writes or carrier package/policy changes.
NexaUI handoff stays closed. Read the [contract/review](../../../spec/device-filter.md).

## Result and dependency

The new graphical image corrects the earlier missing-libbpf finding. Separate
root/cap-zero/NoNewPrivileges oneshots prove that the actual root block partition
opens read-only in the baseline and returns EPERM with DevicePolicy=closed.
Both open the real null device read-write without writing data. The existing VT
service then succeeds. This proves the fixed probe policy, not all app/MAC/device
policy or retroactive old-image acceptance. Broker PrivateDevices stays enabled.

Signed Fedora44 libbpf2:1.6.3-2 supplies453248 bytes, DSO SHA-256
`0bfb4f513c68aa686622acb7d2c3e01c3afc4ac1750bdc377d29e4083453f886`.
RPM signature/header/payload digests verify against the full Fedora44 fingerprint;
signed archive SHA-256 is
`fa02ef95e23708061adb6b59fc69741a27c119c8f43dafa477c5b55600fbd9b9`.
Independent RPM extraction matches the metadata. Readelf confirms SONAME1 and
direct libelf/zlib/libc, with zstd through libelf, already in the original runtime.
The package omits licence files; matching upstream BSD-2-Clause text is separately
pinned and included. No package scripts/transactions run; carrier1.6.3-1 is unchanged.

The Fedora patch is kernel7.0 declaration compatibility, not a claimed security
fix. Current Fedora packaging, newer upstream releases and open parser reports
898/960 were reviewed. Exact CVE clearance is not established. This adoption is
trusted PID1-generated instructions, not arbitrary ELF/BPF loading or production
security clearance. The [pins](../../../spec/device-filter-inputs.json) and review
record the boundary and sources.

## Build and actual boots

Evidence: `local/device-filter-2026-11-08`. Build-v2 token
`57330fb8099f428e9c13bd9e7252e9eb`, helper1110690, uses actual UID1000 in a private
2GiB work tmpfs/256MiB tmp. Nine apps and native owner/probe compile; strict C17
warnings-as-errors/analyzer passes. Owner adversaries pass Release/ASan/UBSan;
seven payload and seven controller groups pass natively. Source archive SHA-256:
`18c080e8372c3b205075754b158b4fe87a1bd80cc220ccfd01da85b9db64cbab`.
All130 export members were independently checked before acknowledgement and exact
retirement. Export185317215 bytes SHA-256:
`cf96c8efc715b68c39cb0ffade7f7d44cf08c7cb4ecbb65a4bfc400e724af51d`.
Original1426 payloads/365 aliases retain their independent inventory; the added
DSO/alias/licence have separate provenance.

Sequence-v2 uses fresh NVRAM, original handles, read-only system image, one new
held data copy and protected host-local AF_UNIX QMP with per-connection UUID checks.
No network/shared directory/physical device/direct kernel injection is used.

| Boot | Original PID | Seconds | Kernel boot ID |
| --- | --- | --- | --- |
|1|63192|27.515|c3769fd7-439a-4b75-a781-bc158070711a|
|2|45832|23.843|4f566ef9-3bc1-4495-a935-00ad879b53da|

Both boots pass ordered baseline/denied observations, then actual UID1000 active-seat
desktop input. Terminal, Files and Settings open/close. Boot1 writes the document;
boot2 visibly reads it. Terminal-document and Settings screenshots were inspected.
Original desktop ownership closes; normal ACPI/logind/systemd unmounts data before
kernel S5 and original QEMU exit0. Final controller rechecks the exact same-boot
device pair and absence of failures/disabled-BPF warnings. Exit0 alone never passes.
Final data SHA-256 is
`5729b1a9ef8429f0bf50dc56a805f0189b20c4cccdbfce79204dc492f622a31b`.

Independent inspection-v2 token `bec6a2b5b6bd4eae92862d7bd3b9898b`, helper1111870,
runs ordinary-user e2fsck -f -n and debugfs. Actual14 bytes `aetheri-image` plus
newline, regular inode UID/GID1000/mode0600/one link and clean filesystem pass;
data hash remains unchanged. Native seven controller/seven payload cases pass.
Probe Release/ASan/UBSan ordinary-user guards reject before effects. All27 members
are verified before retirement; export29369 bytes SHA-256:
`c7cc4fee46887075a8ce0479f5a62bf08752bc91048e5eb62f5b304ca1799b5c`.
Final source review matches all489 bundled non-document inputs, without postbuild
code exceptions. Host25 affected contract groups pass;965 local Markdown targets
exist. Read-only preservation audit confirms445 normal source inputs,31 frozen
contracts,19 quarantine hashes and52 P0 files unchanged, original authority digest
`dd111cfb9fdd632fed3b86c22df22ec31e946fcf22236ea283171e85a94c2ae9`
and original CLOSED DRM history retained. QMP confirms the original carrier still
running. All new VM/build/inspection actors are terminal/retired; four foreign
host files remain outside this change.

## Preserved failures and next boundary

Build-v1 succeeds but VM57644 rejects `block-context`: the old data-only udev
rules tag devices without publishing PARTUUID aliases. Its180-second controller
limit terminates only that original test VM; failed/unknown data remains retained.
A targeted alias after a separate blkid import fixes the cause in fresh build-v2.
The controller now notices device failures immediately. The first inspection
harness separately uses incorrect Windows absolute archive member names and fails
before inspection. Its verified diagnostic/export remains retained; fresh v2 uses
relative names. Neither failure is silently reused or claimed as filesystem success.

Next join this graphical root/data with the proven USB initrd, independently
checking common parent/actual geometry and explicitly adapting fixed extents.
Add laptop Intel DRM/input/firmware and test the complete removable profile first.
The user authorizes the identified Intenso Speed Line G:31457280000B when ready;
reidentify before writing, do not ask again or wait for their later test, and
continue development after flash. Internal disks are not authorized. No physical
USB write yet. Full OS/apps/browser/lifecycle/removal/installer/LUKS/updates/
suspend/hardware and resettable experiment desktop remain required.
