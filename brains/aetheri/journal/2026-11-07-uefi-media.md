# Standalone UEFI removable-media boot — 7 November 2026

The previous direct initramfs boot checkpoint is now packaged as an actual
GPT/FAT32 boot medium. UEFI finds `EFI/BOOT/BOOTX64.EFI` on an emulated removable
USB device and starts its embedded kernel/initramfs. The final QEMU commands have
no `-kernel`, `-initrd` or `-append`. Native systemd/diagnostic success and a
separate wrong embedded command-line rejection are observed, followed by kernel
poweroff and QEMU exit0. Read the [contract](../../../spec/uefi-probe.md).

This advances the laptop-alpha path; it does not deliver its desktop/system root,
production policy, installer, persistence, updates or hardware acceptance. The
image-only immediate-poweroff unit is never installed in the development carrier.

## Dependency and build review

The exact Fedora44 systemd-boot-unsigned259.10, systemd-ukify259.10 and
python3-pefile2024.8.26 RPMs come from the official release/update mirror. Their
whole-package digests, declared requirements/licences and selected payloads are
in [the pin](../../../spec/uefi-probe-inputs.json). Actual rpmkeys checks verify
the independently expected Fedora fingerprint
`36F612DCF27F7D1A48A835E4DBFCF71C6D9F90A6`, header signature and header/payload
digests. The selected stub/script/module payloads also match RPM file digests.
No installation scripts, key import or system package transaction occurs.

The same-family stub avoids changing the selected systemd runtime. Official
ukify handles PE layout/alignment; the extra pefile module is build-only in the
private prefix. The explicitly unsigned/non-measuring build supplies the kernel
release and does not exercise the script's broader optional cryptography/zstd
features. It does not constitute a full package-manager installation or support
for arbitrary ukify modes. The original systemd licence texts remain in the
initramfs; pefile's MIT text is preserved in its tools prefix. No third-party
binary is relicensed under Aetheri's MIT licence.

Review-v1 verifies/extracts the stub and retires with seven verified files.
Review-v2 verifies all three packages and exports their selected payloads, then
its post-inspection path incorrectly expects `/usr/lib/systemd/ukify`; the vendor
script is `/usr/bin/ukify`. That original terminal1/18-member export is retained.
The signed payload/source is independently inspected on the host, and the builder
uses the actual vendor path. No failed extraction is silently installed.

Security review on 7 November checks the upstream advisory listing and the
[recent nsresourced advisory](https://github.com/systemd/systemd/security/advisories/GHSA-xxx4-mr52-4hh5).
This proof has no nsresourced executable/socket, managed user-namespace allocator
or untrusted package admission. The builder processes only these pinned inputs
as UID1000 in a private work mount. That bounded exposure is not a complete CVE
audit or production-security approval. RPM OpenPGP signatures do **not** sign the
generated EFI application; it remains unsigned, without Secure Boot acceptance.

The already installed rpm6.0.1, cpio2.15, util-linux2.41.3, dosfstools4.2 and
mtools4.0.49 supply the extraction/partition/format/readback tools. Their actual
executable hashes and precise package releases are retained in each manifest.
All format targets are newly created regular files under an exclusive output;
no command accepts a device or mounts the generated filesystem.

## Final native construction and integrity checks

Final build-v3 binds 14 exact code/pin/previous-boot/package inputs. Token
`6ef6efc8db3841e98b138c438931b033`, root helper1049016 and handle82050 are
terminal0. Source archive SHA256:
`1528df6197446fc3cc868f09ff65bfeda3e0ec20592f7b3bc9026b5279c57a3f`.
Ordinary UID1000 construction runs in the enforcing carrier's separate bounded
768MiB work/256MiB temporary namespace. Two good outputs and one explicitly
wrong-nonce test input build in1.338s/1.395s/1.373s. Native changed-RPM rejection
precedes output creation; reusing an old output rejects and preserves it.

All seven independent media contracts and all five previous boot-image contracts
pass in Linux, as they do on Windows. They cover GPT checksums and independently
rechecksummed wrong geometry, FAT mirrors/cycles, the exact fallback file, PE
architecture/duplicates/overlap/resources, fixed no-direct-boot commands, original
attempt preservation, and precise service/result/poweroff acceptance. The
readonly parser invokes no formatter, mount or PE execution. It is integrity
evidence within the independently bound build, never publisher authentication.

An initial verifier assumption of GPT first-usable LBA34 is corrected to the
actual explicitly selected LBA2048. A bytearray UUID check in the corruption
fixture is normalized to bytes. Build-v2 successfully constructs all three
images, but its command-only unit test tries to copy host firmware from the
Linux source archive; that fixture now records/asserts the intended copy through
an isolated adapter. Actual firmware copies are still independently hash-checked
and exercised by the real Windows boot tests. No native test is skipped.

Final export SHA256:
`1c835d8c0cb85748af239cd8634f90b396b03d6b5ac654075de962e4dfdaaad5`
(281,967,179 bytes). All 203 members are independently verified before the
original protected token/source/archive acknowledgement and exact namespace
retirement. Earlier exports/failed test attribution remain retained.

Only three verified members per selected image are extracted for host testing.
The normal image is128MiB; its UKI is28,228,608 bytes. Both UKIs are byte-identical:
`5d6002e2b7f7baa9ef7376f0ad6e65d060197fc776333bfea2f2bb550ff89fd0`.
Normal raw SHA256:
`601444f8f9e0b9f4573b1a6cde05ed271afc1a89c6b085d787b6c3ff18a9be4a`.
FAT directory creation timestamps vary; no whole-raw-image reproducibility is
claimed. Disk/ESP GUIDs are deterministic fixture metadata, not volume/clone proof.

## Actual firmware boots

Each test verifies the original QEMU11.1.0/DLL/UEFI-code/NVRAM-template hashes,
uses a fresh private NVRAM and attaches only a read-only USB boot image. There is
no system/data disk, network, QMP or shared directory. q35/TCG uses two vCPUs and
768MiB. A complete native service result, kernel poweroff and original child
exit are required; marker text or QEMU exit0 alone cannot satisfy acceptance.

| Original attempt | Result |
| --- | --- |
| Initial boot-v1 / PID66796 / handle78889 | PASS, exit0,14.641s, boot `5864aefc-2dde-4b8e-b4e2-f6b7394a86a2` |
| Final boot-v3 / PID50672 / handle69600 | PASS, exit0,12.844s, boot `911c4c71-3954-421b-9c91-dbb18794f357` |
| Final boot-negative-v3 / PID25572 / handle66474 | Exactly nonce rejection, failed diagnostic service, no PASS, poweroff/exit0,19.656s |
| Final boot-cold-v3 / PID7808 / handle15555 | PASS again, exit0,12.860s, distinct boot `48b9dc57-0fcb-464e-bc5d-be001509d49c` |

All actors are terminal; no observation timeout is replayed as a new attempt.
The shared original-child observer is factored from the direct boot runner and
now additionally rejects foreign/duplicate PASS, a missing successful service,
missing poweroff and mixed failure output. The three previous direct-boot serial
records pass the stronger pure evaluation without claiming new VM executions.

The read-only audit preserves all445 normal guest source inputs,31 frozen SQL/XML,
19 quarantined hashes,52 normal P0/controller files, the original authority hash,
old SDK/SDL and CLOSED DRM history. Carrier boot, foreign work and the separately
prepared/unattached developer disk remain untouched. Four unrelated planning/
design files stay excluded; no NexaUI requirement or chat handoff is generated.

## Next

Independently assemble the read-only system root and connect the existing Aetheri
session/desktop to this actual boot path. Keep writable-data ownership, normal
app/document admission/recovery/removal, device services, laptop drivers/suspend,
installer, signed updates/rollback and the full resettable experiment desktop as
explicit remaining work. Full OS goal stays active/incomplete.
