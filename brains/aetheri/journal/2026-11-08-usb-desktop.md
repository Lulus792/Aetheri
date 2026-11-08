# Desktop and persistent data on one USB medium

8 November 2026. The full OS goal stays active. C/build/Linux/dependency skills
were used; no subagents, CUA, physical writes, package transactions or normal
authority changes. NexaUI handoff stays closed. Read the
[contract](../../../spec/usb-desktop.md) and [geometry](../../../spec/usb-desktop-inputs.json).

## Implemented boundary

The new regular image has the authorized Intenso's full 31,457,280,000-byte size.
EFI and the 512 MiB read-only system precede a 30,785,142,784-byte writable data
partition. Both GPT copies describe the entire medium. The bounded USB2 profile
and runtime observation bind every root/data field. The initrd compares the
candidate root's full configuration before standard systemd SwitchRoot.

The real-root helper repeats original boot/diskseq/sysfs-parent observations and
requires a common current removable USB parent before `/var` mounts. A second
check inspects actual read-only root, read-write ext4 data/home and nosuid/nodev.
These are fixture correlations, not durable volume/clone/publisher authentication
or production account/app admission. Windows regular-image copies use explicit
sparse marking/SetEndOfFile and bounded streaming, with actual allocation/zero-read
regressions. Physical flashing must instead write and read back every byte.

## Executed evidence

Evidence directory: `local/usb-desktop-2026-11-08`. Final build-v3 token
`9ef1e11427a74a2a86272b7d1a89ba2e`, helper 1118666, runs as UID 1000 in private
2 GiB work/256 MiB temporary mounts. Strict C17/analyzer, Release/ASan/UBSan
adversaries and 31 native image contract groups pass. All 190 export members
were independently verified, acknowledged and exactly retired. Source archive:
`091e61b7d32c547148260b0900c126329a1f7bf1831c5be35d2f61c9328d1877`.
Export 355756035 bytes:
`1111eb87aaec6ba6574f9785160525befd3e52ff5f5c5c34c711e251bd7c96ea`.

Sequence-v3 owns fresh NVRAM, the SAME held writable USB copy, original QEMU
handles and protected host-local AF_UNIX QMP with per-connection UUID checks.
No network/shared directory/physical device/direct kernel injection is used.

| Boot | Original PID | Seconds | Kernel boot ID |
| --- | --- | --- | --- |
| 1 | 41360 | 39.907 | 35899441-709e-4193-8e70-2945f2460012 |
| 2 | 67644 | 34.516 | 4f36d915-0f71-4f72-b0fa-49434c2ae083 |

Both pass ordered initrd/handover/root/data and actual device baseline/denial
checks. Keyboard input opens/closes Terminal, Files and Settings. Boot 1 writes
the document; boot 2 visibly reads it. Native desktop and second-boot document
screenshots were inspected. Normal ACPI/logind/systemd shutdown unmounts home/data
before kernel S5 and original QEMU exit 0. EFI/system/GPT ranges stay unchanged.
Final data SHA-256:
`f2db5d1e64569b981d0a219a8ba59865f8ce68b0ae1d994b64756f482a069850`.

Independent forensics-v1 token `e066ee4add2c44ec8e73e6b3134c2523`, helper 1123589,
reconstructs only that hash-bound partition as a private sparse regular file.
Ordinary-user e2fsck -f -n exits 0; debugfs verifies the exact 14-byte document,
UID/GID 1000, mode 0600 and one link. The complete data hash stays unchanged.
Fsck's less-than-a-day future-time warning is retained, not clock acceptance.
All nine export members retire after verification; 2839-byte export:
`9a3f96b1461f50d237d40e7a74845ec8df0c7e80d8beaf85d1a13bbd1cc7ced4`.

Separate native review-v1 passes 34 groups, including three GPT/wrong-parent
evaluator guards and coloured shutdown handling. Its nine verified members retire;
export SHA-256 `a8c6c14b47263f1639b93ac6e5749491fd6ef6eac4cc822bd4294efecefae9fc`.
All 500 bundled non-document inputs match except two separately reviewed host-only
SGR parser/test changes. Native image inputs match without exceptions; ten captures
retain their hashes. The read-only audit preserves 445 normal inputs, 31 frozen
SQL/XML, 19 quarantine hashes, 52 P0 files, original authority/CLOSED DRM history
and foreign work. QMP confirms the original carrier still running.

## Preserved failures and next work

Build-v1 fails at the independent reader's buffer-protocol use; bounded slicing
and a regression precede fresh builds. Intermediate sequence-v2 reaches the GUI
and poweroff but its controller rejects coloured unmount messages. Its failed
state remains intact. SGR normalization and later full-profile native hardening
both precede fresh build/sequence-v3; historical v2 is not final acceptance.

Two wrong-parent preparations stop with host read ENOSPC after GUID/CRC changes
only in their new copies: v1 primary and v2 external. Neither launches a VM and
both remain preserved. A complete later read of v1 succeeds. F: was independently
observed with zero free bytes, then about 94 GB free without agent cleanup; no
foreign files or processes were changed. The added preflight requires both full
logical copies plus 8 GiB headroom. It is an observation, not a reservation against
other writers. Native review-v2 token `3299b8c020a6421c9622706b55220610`, helper
1123872, passes 35 groups including all four parent-test guards. Nine members retire
after verification; 3618-byte export
`85a72ec1cc16ab7e531203d0f804d8bbcc90b6173243c021268bb08cc4e32a4f`.
Fresh v3 preparation/actual negative is still in progress here. Require actual
rejection and both unchanged-media checks before acceptance. Actual wrong-data-
filesystem testing also remains.

Next add laptop Intel DRM/input/firmware, automatic DRM selection and a console/
logging path tested without a UART. The current bochs/serial profile is not
physical-ready. Intenso G: remains authorized when ready: reidentify it, fully
write/read back, and continue without waiting for the user's hardware test.
No physical write yet. Normal login/MAC/lifecycle/removal, browser, installer/
LUKS, updates/rollback, suspend and the experiment desktop remain required.
