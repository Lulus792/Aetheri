# Persistent documents in the own boot image

8 November 2026. The full Aetheri objective remains active and incomplete.
The previous goal interaction reported status without changing implementation;
this continuation implements and actually tests the next data/image boundary.
No subagents or CUA were used. C/build/Linux skill entry points and selected
knowledge were read. The NexaUI handoff stays closed.

## Result

The original readonly-root builder now accepts a separate explicit development
profile. `tools/build_data_probe.py` adds the selected existing udev/mount runtime,
fixed test-account files and standard mount/service units. It creates only a new
regular-file data image. Its own source, C source, payload pins, complete root
inventory and tool/configuration hashes are in the final manifest.

One new ext4 volume backs `/var` and the `/home` bind. The UID/GID1000 document
directory is0700 and its document0600. `/tmp` is a fresh64MiB tmpfs and the
private volatile runtime directory belongs to the test user. The actual service
has zero effective capabilities. Its canonical document advances across cold
boots; the root finisher checks the current record, syncs and unmounts home,
var and tmp before the image-only poweroff action. Original system-root checks
still require a real EROFS observation. See [contract](../../../spec/data-probe.md).

The builder reuses the original RAM boot inputs, existing e2fsprogs1.47.3 and
GCC16.2.1 toolchain. The13 newly selected executable/licence payloads match
existing RPM file metadata and `spec/data-probe-inputs.json`; they add existing
systemd-udev259.10 and util-linux2.41.3 payloads, not another service framework.
No download, installer script, package transaction or kernel/version change occurs.

## Actual native build and tests

Sources and the already verified UEFI packages were copied into a root-provisioned
private768MiB work tmpfs, with a separate256MiB `/tmp`, under a fresh private
mount namespace. Memory preflight required over2GiB available. Compilation,
image construction and record tests run as UID1000, not root; the existing
carrier remains Enforcing and on its original boot. No normal source/build is
replaced. These carrier facts do not establish MAC policy in the new image.

- Strict C17 warnings-as-errors plus `-fanalyzer` compile the native data probe.
- Native record adversaries pass ordinary and ASan/UBSan profiles. Actual files
  exercise missing/canonical records, zero/negative/noncanonical/oversized counts,
  integer-overflow input, trailing/missing bytes, wrong mode, hardlinks and symlink.
- Six Python controller/result groups pass natively and on Windows. They reject
  missing/mixed/foreign markers, wrong counter/boot, missing user/clean-unmount
  evidence, replacement/resizing and uncertain second-dispatch replay. Native
  Linux also exercises renamed/replaced/symlinked held-file paths; Windows uses
  its actual open-file replacement restriction. No Windows symlink result is claimed.
- The five older boot contracts pass in the native build. Windows also passes
  five root cases and seven UEFI cases. Updated mixed-DATA rejection in the old
  result classifier is tested afterward on Windows. The unchanged frozen SQL/XML
  are audited, not falsely described as a newly run full lifecycle suite.

Final build token `165b046fa17545df824af6bf1211ae17`, helper PID1056927,
host handle49385: terminal0. Its182 evidence members were independently streamed
and checked before the original export ACK and exact namespace retirement.

- Source archive SHA256:
  `f96abbd566622f0776ede6bc53064d4edb960c108f1a64655b2c62cf19dd570d`.
- Export SHA256:
  `9678a6e19d6ed8e98ebf61c13e5ecc7b52dd2be87d13b9a8092b9f9b9fbc4fb4`;
  141629861 bytes.
- Original inputs, receipts, logs and extracted artifacts are under
  `local/data-probe-2026-11-07/`. The directory date reflects the initial work
  before local midnight. These large/raw development artifacts are not Git payloads.

## Cold boots and independent disk evidence

`tools/test_data_probe.py` makes a new private owned copy of the selected initial
data file. Its original open handle remains held across the two actual QEMU
children. Held/named device/inode/size are checked before/after each dispatch.
Each child uses fresh NVRAM, the exact readonly system image and one writable
data file. No direct kernel/initrd/append, network, QMP, shared directory or
physical device is supplied. Previously extracted QEMU/firmware/DLL hashes are
checked against the original receipt. An installer source hash is not a valid
Windows code-signature claim.

Final sequence-v3, host handle44208 terminal0:

| Attempt | Original PID | Exit / seconds | Result |
| --- | --- | --- | --- |
| boot1 | 57768 | 0 /18.282 | counter1, boot247fbf68-c5bd-4b27-895e-81fdce21df66 |
| boot2 | 64692 | 0 /16.235 | counter2, bootc5feb356-7bde-45e7-9be8-638e6045dadd |
| separate wrong UUID | 67228 | 0 /22.891 | volume-context rejection; no writable mount/user write |

The negative controller handle88393 is terminal0. Its newly corrupted fixture
hash remains unchanged before/after the VM:
`aa29ca1d931905709eb9e50d371c9a4031af56bfa66652dcd95b0bf1f08261ff`.
The original unmodified initial data source remains
`d179226def5a386032e637bf3545e2cc4208e7dced8d112e3743b6167a49996d`.

The final twice-booted data file is
`88ef9881fbf1ce81b202fd8bc3a27a2dd1ed19039ee1b9c3c8125926aab09954`.
A separately bound private native read-only inspection runs e2fsck-fn and debugfs
as the ordinary user, verifies exact counter2/current-boot bytes, UID/GID1000,
mode0600 and one link, then verifies the whole-file hash is unchanged. The
filesystem check exits0. Its token519f6e6402604c82b382a41929aafe24/helper1057266
is terminal0 and retired after all9 evidence members are checked. The2868-byte
export SHA256 is
`a2f9baadf292f0081754aaae208f91b505ac56bf6d4b03daded4333ca62b45bb`.

The read-only fsck log also notes last-mount/write times slightly ahead of the
carrier's clock (less than a day). No repair is performed; content/structure
checks pass and bytes stay unchanged. This proof uses boot IDs/counters and
does not establish trusted or synchronized clocks across the two VMs.

All three data boots originally used observer
`95a7b73383ef2d3c14c3b01f72dedd8ab64d3e31ff9aa9d9948cd8d8de3e69d3`.
Afterward, old ROOT/BOOT classification additionally rejects DATA markers. The
current evaluator98859b3c33249e051d403d607483c8da602fb2b44203a6c67effe94f27b0b53f
re-evaluates these three data logs and the three retained original root logs,
checking original manifest/serial hashes and terminal receipts. All6 pass.
`revalidation.json` explicitly records historical evaluation, not6 new boots.
The actual C/builder/pins match the image manifest; root/EFI bytes remain unchanged.

## Earlier failure and boundaries

The first25-byte and second21-byte virtio serials were both rejected at the
pre-mount volume check. Original attempts64140/29916 exit0 by the diagnostic
failure action; their controller results are failures and pending data sequences
remain preserved. They were not replayed. The kernel driver and selected QEMU
header establish the20-byte GET_ID limit; the final exactly20-byte serial passes.
The builder now explicitly checks that length. One Windows command-shape assertion
also initially used Unix path spelling; it was corrected to native Path rendering.

This is one trusted development writer. A pre-rename inode check is not atomic
concurrent-writer exclusion. Fixed UID/UUID/serial facts do not provide AccountId,
clone or volume authentication. No old app StorageRef/DataEpoch is adopted.
There is no normal login, production SELinux image policy, app isolation or full
power-loss/UNKNOWN recovery. A post-mount failure preserves the attempt rather
than claiming a clean shutdown or automatically starting again. The minimal
udev image currently logs unavailable libkmod and default journal facilities;
complete hardware rules/module loading and normal session logging remain work.

The read-only preservation audit confirms445 normal guest source inputs,
31 frozen SQL/XML,19 quarantine hashes,52 normal P0 files, the unchanged normal
authority hash and original CLOSED DRM history. Carrier QMP remains host-local
AF_UNIX, the developer disk remains untouched, and foreign project work is not
stopped/rebooted/modified. Four pre-existing host changes remain unstaged.

Next join the existing graphical desktop/session to this base. Laptop USB-root
modules/firmware and physical GPU/input, WLAN/Bluetooth/audio, suspend, normal
app/document admission/recovery/removal, installer/LUKS/updates/rollback, browser,
remaining apps and the complete resettable experiment desktop stay required.
