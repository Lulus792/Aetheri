# Laptop image integration — 8 November 2026

This work extends the single-medium development desktop, not normal login,
production app admission or an installer. The authorized Intenso is still
untouched. NexaUI handoff remains closed; catalogue maintenance only.

## Native graphics and driver work

The owner selects `auto` through the existing logind/libseat session. Reviewed
wlroots 0.20.2 GPU enumeration retains primary ordering with a spare slot, refuses
saturation/overrides and closes unselected devices. Constructor failures can
consume the selected object; cleanup compares a pre-captured integer address
against live session members, never a dangling pointer. Heap/FD adversaries and
editor save-work pass Release and ASan/UBSan in final drm-v3. The minimal profile
also revealed a missing Threads target; CMake now discovers it for testing.

Final drm-v3 token `4e1d132629e944c2a11d8299d3db8982`, helper 1126770, exports 15
verified members/16899 bytes, SHA256
`8a21b331ef8e2444b4eb4b51fd98ba84a34235f8c2f408c79a53242605227801`.
The failed first configure and intermediate review are retained separately.

The new inventory adds 18 same-ABI6.19.10-300.fc44 modules, Kaby Lake DMC 1.04 and
its unchanged Intel licence:20 files / 2,488,128 bytes. Only bochs/i915 are explicitly
loaded; other drivers use modaliases. GuC/HuC remain excluded. Installed payloads
match the carrier RPMDB, not a fresh downloaded-RPM signature check. Rendering
remains software. Physical Intel/input/display/suspend acceptance remains open.

## First candidate and discovered console failure

Laptop-build-v1 token `e901a3d68d9347249b222d8dfe8cd223`, helper 1131339, finishes
successfully under the private ordinary-user builder. All 195 export members are
independently verified before exact helper retirement. Export 358,278,129 bytes,
SHA256 `4bd978f160369da83406a2e1c783a05288e6de4102b96ce9c3a75f44af9f8ee1`.
The full 31,457,280,000-byte raw image hash is
`0c5d8bdc61ac10254450bb2d9c32aaef7fe1df61086069689258cb16f47769b1`.

Sequence-laptop-v3 passes two cold boots with actual app input, device-denial
checks, retained document and normal unmount/S5. Original QEMU 62520/3540 exit 0;
boot IDs `57300566-22a1-495a-b0a2-a0223a68c78e` and
`97f1caaa-8c92-4934-9f8a-866cf47439d9`. Data hash
`dd3d3c819f2239bcf79b2fccc71a20c8eb7c789dad4cc3154b01529cecd964bd`.
The packed read-only data copy is retained; this candidate is superseded below.

The SAME candidate fails its separate no-UART test before desktop input:
QEMU 23312 emits an actual guest-shutdown event and exits0 during the initial
wait. The connection reset is preserved as failure, never acceptance. A fresh
read-only-backed qcow2 diagnostic captures the same early shutdown: QEMU 53568,
14.703s, exit 0, original base hash unchanged. Native captures show kernel output
on the virtual display, then no PID1 output before shutdown. They were actually
viewed; the failed copy/overlay are retained and never replayed.

The dual-console command selected ttyS0 last. Linux documents how console order
controls `/dev/console` and how unavailable devices affect that selection.
The working hypothesis is the unavailable serial console path, not a proven
general kernel defect. The next build selects ONLY `console=tty0` for the laptop;
the old serial-only diagnostic remains unchanged. Desktop output uses the
existing bounded volatile journal and `/dev/console`. A new native image and
actual no-UART boots must validate this correction; compilation is insufficient.

## Host image I/O and capacity

Sequential reads of sparse 31 GB images repeatedly coincided with F: reaching
ENOSPC, although allocated-range queries still reported about 347 MB. The cause
of the additional capacity consumption is unknown. No foreign files/processes
were removed or stopped. Incomplete copies/preparations are preserved.

For local fixed NTFS sparse regular files only, image_io now obtains bounded
kernel allocation ranges. Actual data is read; confirmed holes contribute every
logical zero byte to SHA256. File identity/size/times must remain unchanged.
Unsupported sparse files fail closed; ordinary/Linux files retain ordinary I/O.
This is not a concurrent snapshot or physical-device writer. Physical flashing
must still write/read ALL bytes, including zeros.

An independent full readback of the first candidate matches its native export
in 68.515 s with exactly unchanged free space. Windows tests cover actual sparse
windows/hash/copy,300 disjoint extents/pagination and bounds; USB/UEFI regressions
pass. Native io-review-v1 token `4bf216d288bb4e78af54d8e7894666b3`, helper 1141646,
passes affected contract suites:13 verified members/4720 bytes, SHA256
`e521b83ef5f1a2363a6884a38b2cfa573a76cf306970445d6c9c3aa7273155f9`.
Windows-only cases are explicitly skipped in that Linux run.

Nineteen inactive, manifest-bound factory boot images were sparsified under
exclusive writer handles. Only spans already read and proved all-zero were
deallocated. Full before/after logical hashes, sizes and inode identities match;
no file was deleted/truncated. Data disks, carrier disk, quarantine, physical and
foreign files were excluded. The first reclaims 507,510,784 bytes; the batch restores
about 9.3 GB free. Exact per-file evidence remains in reclaim-first.json and
reclaim-images-v1.json. These are allocation changes, never content deletion.

The no-UART controller retains8 GiB headroom by default. A 4 GiB floor is allowed
only for a kernel-confirmed local sparse source with at most512 MiB possible
data; its measured allocation and free space are journaled. This bounds the new
copy while preserving substantial headroom. It does not reserve against foreign
writers or authorize a failed replay.

## Earlier failures and remaining boundary at that point

The corrected image now actually reaches desktop and Terminal without any UART.
In no-serial-laptop-v2, QEMU 63728 completes the first boot and a genuine guest
shutdown with exit0. QEMU 66180 reaches desktop/Terminal on boot2, but a parallel
ordinary JSON reader interferes with Windows replacement of sequence.json.
WinError5 leaves both the old state and its complete temporary candidate. This
is a FAILED test-tool run, not a completed two-boot/persistence acceptance.

Error recording itself then fails on the retained temporary name before the
old handler reaches termination. The status watcher is stopped; the exact test
QEMU is inspected by full command/UUID and a held process handle with observed
creation `2026-11-08T07:31:00.1825109Z`, then administratively terminated. Both
retirement records and all medium/NVRAM/captures/old+temporary state survive.
Never replay/adopt that medium or call its termination graceful shutdown/drain.
The protected original carrier is not involved.

The Windows conflict is reproduced with an ordinary held reader. Merely opening
with delete sharing does NOT make os.replace succeed on this host; that attempted
fix is rejected. The selected image_state protocol instead serializes cooperating
readers and the publisher through one stable byte guard (msvcrt on Windows,
flock on Linux), then retains create-only/fsynced temporary publication. Never
replace the guard inode, use ordinary live readers or infer authority from it.
A bounded busy wait fails closed; a leftover temporary file is preserved.
Controller failure cleanup now uses the original process handle despite recording
errors, closes transport independently and retains unknown exits as unknown.

Six actual Windows cases pass, including a separate-process writer with a held
reader/Unicode path, original conflict, bounds, unknown temporary preservation
and failed recording/retirement. Native io-review-v3 passes its applicable cases
(the ordinary Windows-reader reproduction is explicitly skipped): token
`5ce0e978fe664718a6ac2c14cdb70fdb`, helper 1143696,8 members/3241 bytes, SHA256
`fa1a806927bf56ac115d93b0c5932239ded3bd269308b7bc2cb15f986f3c10d9`.
A separate no-serial-laptop-v3 starts from a NEW copy of the unchanged corrected
image. Full acceptance and independent data inspection remain pending.

The actual Windows fault probe also passes: state-retirement-v2 uses a separate
paused QEMU, checked firmware, no data disk/network/guest execution and one local
authenticated QMP connection. An ordinary held reader blocks acknowledgement
publication after the real query-status reply. Despite three further recording
errors, the original process exits1 through its held handle and the transport
closes. Published pending state AND complete acknowledged temporary state remain.
Its UUID is `db064b06-40ef-43f6-a740-323e42e286f4`; numeric PID66180 was reused
from the earlier unrelated failed VM, explicitly illustrating why PID alone is
not identity. This is expected fault-probe success, never OS boot/drain acceptance.
The first fault-probe setup fails before an accepted QMP session because machine
none/max lacks the expected APIC setup; its artifacts remain separate.
Runtime: CPython3.12.14, AMD64, Windows11 build26200. One temporary unit-test
directory left by an initial CRLF assertion failure is retired only after checking
the exact three owned files/content and absent process; no recursive/foreign
cleanup occurs. The corrected test always joins its own child before cleanup.

The corrected laptop-build-v2 completes successfully. Token
`0bdcc4f22aaf435f8664c6405387a67b`, helper 1142086; all 195 members are
independently verified before acknowledgement and exact retirement. Export
358,277,993 bytes, SHA256
`a138b8b951e67eaa85ac3921dc215e854c331762d94e5b5ac3197bc870313c23`.
It selects tty0 only; actual image extraction and no-UART acceptance remain work.
Current QMP/headroom/console/corruption/parent guards also pass a separate native
review (17 groups): token `80b5cae538134f1b8caf4bdf061e6eb1`, helper 1143584,
7 members / 2863 bytes, SHA256
`3bd7d497c4b28eccd8e9968b873b81890ddf529e2fb1a66d8a929950746b3a0c`.
Only the shutdown-wait phase accepts either EOF or Windows connection reset as
transport termination; original exit0 AND the actual guest-shutdown event are
still mandatory. An early reset remains a failed attempt.

The first candidate's separate data-corruption negative now passes. It changes
only 16 primary-superblock UUID bytes in a NEW copy (one byte value differs),
leaving the deliberately stale checksum. QEMU 45600 exits0 in 16.453 s after root
handover and exact `filesystem` rejection before `/var`/GUI. Boot ID
`13d5ca69-add3-452d-997a-2edb9d69cc3b`; serial SHA256
`0c0cfc8f10d9d2d3a4fe02501492af0edf69310f6ada5f5c12a4bd8328af4e95`.
Full mutated-medium and original-source hashes remain unchanged afterward.
Native corruption review token `be4071bce2d64418a51c6d446ff05923`, helper 1141925,
passes 3 corruption plus 4 old parent guards; 5 members/2224 bytes, export SHA256
`08f9251b991725c384f2db81cd0f0d32ab1123ae08526eb7b74eb31076cde0b0`.
This evidence belongs to the first candidate; later console changes are separate.

An access-only Windows probe reidentifies disk4 as Intenso Speed Line,
serial 212300100050, USB, 31,457,280,000 bytes, neither boot nor system disk. Its
PnP/bridge UniqueId retains 22010500000313&0. Opening that physical device with
read/write access returns WinError 5 (Access denied) in this unelevated process.
No lock, dismount, write or flush is attempted. User authorization is sufficient;
the remaining physical-write issue is actual OS access, not missing permission
from the user. Do not bypass it or claim a flashed stick. Keep developing.

Complete the corrected candidate's native build, real no-UART capture reviews,
two-boot persistence and independent e2fsck/debugfs proof-file inspection. The
separate unexpected-data-filesystem negative is also in progress. Then repeat
source/preservation audits, publish only owned files and prepare the authorized
full physical write/readback when OS access permits. No hardware/runtime acceptance follows yet.

References: [Linux console selection](https://www.kernel.org/doc/html/latest/admin-guide/serial-console.html),
[profile and pinned components](../../../spec/laptop-probe.md),
[single-medium evidence](2026-11-08-usb-desktop.md).

## Final no-UART acceptance and prepared physical flash

The fresh no-serial-laptop-v3 completes two cold boots on the same writable copy.
Original QEMU43788 (UUID8df9494e-b5a6-47f0-975b-3c4225934ef5) and QEMU55440
(UUIDfe4f6fbd-5644-4600-8f5b-011ffc8bd3e9) each exit0 with a genuine guest=true,
guest-shutdown event. Six actual desktop/Terminal/document PNGs are reviewed
and hash-bound separately. Two kernel boot IDs are
`2eb4ccb1-70bc-4130-84a3-5304b056bcc8` and
`ee21bb4f-aa17-47dc-8802-2d64fc90fb0f`. The document survives both boots.
EFI/system/GPT ranges and the pristine source remain unchanged. The controller
retains observed_requires_offline_verification; it is not rewritten as accepted.

Separate native forensics-v3, token17753957f1f0496682eb2db0d9306695/helper1143802,
independently exports49 verified members (7084 bytes), SHA256
`5e7b1e5b31c106e45d892034b622559645081f35910bcc4ca03e93ed78a41134`,
then acknowledges and exactly retires with exit0. Actual e2fsck-f-n succeeds;
debugfs verifies all11 proof files, exact14-byte document, regular0600/UID1000/
GID1000/link1 metadata, both boot IDs, all UID/GID fields1000, CapEff0/NNP1,
root ext4ro, separate rw/nosuid/nodev var/home, tty0-only consoles and all three
successful data/device-check services. Inspection does not change data SHA256
`fa0b332c5f9ea5fa3ca434df2ec35e7b55f363e8187c0829d377233653ed5317`.
The filesystem future-time warning under one day is retained; this does not
accept clocks. laptop-v3-acceptance.json joins those observations and a fresh
complete pristine source hash. It does not accept physical hardware or writing.

The only flash source is image-laptop-v2/boot.raw,31,457,280,000 bytes, SHA256
`06aad2b908ce008d37744c5171d8d10444d81c31bd7692c86f6dc01754337d20`.
Never flash a modified VM medium. The [fixed writer](../../../spec/usb-flash.md)
requires exact independently selected Intenso identity/geometry, held device and
G: extent, fresh source hash, locked/dismounted volume, complete aligned direct
write including zeros, flush and complete direct readback. Create-only intent/
attempt, asset-wide lock and hash-chained append-only diagnostics prevent blind
replay; original launched exit0 is also mandatory. No physical write yet.

All13 actual Windows tests pass, including unbuffered regular-file transfers,
failed/short/corrupt I/O, identity/extent negatives and nonadministrator refusal.
Final native io-review-v6, token7ce4828e7fdc4404955554d8b896730c/helper1146836,
passes applicable flash/state/QMP/laptop/corruption/parent cases. Three explicitly
Windows-only cases are skipped there and have actual Windows evidence instead.
All9 exported members are independently verified (3822 bytes), SHA256
`938b3d060c61c95ec6e2a8ad86e4ec35f6c70a5d9d38bf5fc169f27328d62883`;
acknowledgement/exact retirement exit0. No installed carrier service or app
capability changes. Windows elevation is the remaining physical access boundary;
credentials belong only in its native dialog. The user's erase authorization is
already recorded. Continue independent development after the flash without
waiting for the later laptop test.
