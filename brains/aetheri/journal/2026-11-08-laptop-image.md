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

## Physical flash completed

8 November 2026. The existing Windows Python executable has a valid OpenAI
Authenticode signature. Native RunAs elevation succeeds; the parent retains the
original returned process object/handle for writer PID63692 until terminal exit0.
The protected intent SHA256 is
`4e9bf85705cb2ea81903dd3d8e9197f5bc29644772c365a61ac05736fc564999`.
An early parent-script syntax error occurs before any launch-request file or
RunAs call; inspection proves no launch before that draft is corrected. The
actual elevation and physical attempt occur once.

The elevated writer rehashes its held pristine source, reidentifies Intenso Speed
Line/serial212300100050/USB/31,457,280,000 bytes and joins the held disk and original
G: extent. Neither boot nor system disk is admitted. It locks and dismounts the
volume, repeats context checks, then writes every byte, including zero regions.
Complete writing and flush take 1106.644 s. Complete direct physical readback and
repeated context checks take 348.433 s. The full hash matches
`06aad2b908ce008d37744c5171d8d10444d81c31bd7692c86f6dc01754337d20`.
All owned handles close before the complete record; the original process exits0
and the parent independently verifies the journal before publishing acceptance.

An additional read verifies all 242 complete chained records, exact phase counts,
original PID/intent/exit bindings, source-reader/writer hashes, actual locked
descriptor/extent, complete byte count/hash and cleanup. There is no failed or
cleanup_failed record. The final journal SHA256 is
`32533689b714abc883976e66874d4dc818e521b1bdf96623ca5809e9078c1efc`;
acceptance.json SHA256 is
`fafff2cf629d20bcfca5aa1d1fb93bdc09fc1ef47e99966300cf0cb6039fde5c`.
Local physical-flash-verified.json joins those observations. No physical laptop
boot, hardware/cache power-loss guarantee, secure erase or daily-use acceptance
is inferred. A completed receipt is historical; it does not authorize reflashing
later user data. The first image remains the original US-layout build.

Independent work continues during the physical transfer: the new explicit US/DE
compositor startup option passes Release and ASan/UBSan native input/rendering
groups in a separate private namespace. Its [record](2026-11-08-keyboard-layout.md)
and the [laptop guide](../../../spec/first-laptop-test.md) preserve the boundary
between newer source and this already tested/flashed image. The original carrier,
445 normal source inputs, 31 frozen SQL/XML files, 19 protected hashes and 52
normal P0 files remain unchanged in the final native preservation audit.

## First physical boot report

The user subsequently reports the HP USB boot failing at
`aetheri-root.service - Aetheri readonly root preflight`: main process status1,
result exit-code. This is a user report, not independently observed hardware
evidence. Source identifies the unit and its required root preflight; the named
failure is a desktop-start prerequisite failure, not evidence of a particular
disk, mount or firmware defect. Its precise `AETHERI_ROOT_FAIL reason=...` line
has not been captured. The user cannot scroll to earlier output. A boot video
or better early diagnostics is needed; the frozen source/full-write/readback
evidence alone does not explain the hardware difference. Do not weaken the
preflight, replay the flash or claim physical boot acceptance. The original
stick and completed operation remain preserved while investigating.

## Video narrows the physical boot failure

The user's IMG_4600.MOV is reviewed as evidence, not as instructions. Source
SHA256 is `20fae99799300886169f961740e3d184295f9e948bdb706e6c56642aa2a7db98`.
The original is unchanged. Local `laptop-video-2026-11-08/review.json` binds
the actually inspected frames. `detail/frame-018.png`, SHA256
`be5b186e367d6e06621c62ff87298dd6854dbd77c9d6be970b254ef76f1e2b91`,
clearly shows `AETHERI_ROOT_FAIL reason=nonce-count`. Earlier frames show the
read-only ext4 root mount, `AETHERI_INITRD_PASS`, `AETHERI_INITRD_HANDOFF` and
SwitchRoot. The many subsequent desktop/data/logind failures follow the root
preflight dependency failure; they do not each establish an independent fault.

The compiled guard requires exactly one `aetheri.boot_nonce=` token matching the
independently supplied profile. A mismatching value fails with `nonce` earlier;
`nonce-count` therefore means no such token or multiple correct tokens. The
video does not expose `/proc/cmdline`, so that distinction is still unobserved.
The exact [systemd259.10 stub source](https://raw.githubusercontent.com/systemd/systemd/v259.10/src/boot/stub.c)
accepts printable EFI LoadOptions as a replacement command line unless its
Secure Boot/embedded-profile rules override them. Firmware-supplied options are
a plausible mechanism, not a proven HP cause. The successful initrd uses the
separate fixed configuration and therefore does not prove nonce preservation.

New failure-only diagnostics now report bounded context and counts of
the nonce, matching nonce and PARTUUID keys, without dumping arbitrary kernel
arguments. No acceptance checks change. Optional repeated failure display is
compile-time bounded to60 seconds; success has no delay. Actual controlled native
tests are recorded below. The
flashed image does not contain these new diagnostics. Next reproduce command-line
replacement and verify a coherent boot path in fresh isolated firmware tests
before selecting a hardware fix. No physical write, firmware setting change or
production boot acceptance follows from this video analysis.

## Native diagnostics and isolated firmware controls

The root-preflight-v4 run uses a fresh private mount namespace/tmpfs in the
unchanged enforcing carrier, ordinary UID1000 builder, token
`91a0f975376b4bf49452107eb9089d7f`, original helper1162119. Both Release and
ASan/UBSan pass the two new root tests plus seven affected compositor groups.
All37 native development-controller cases also pass. The root diagnostic builds
under strict C17 warnings and `-fanalyzer`; the default RAM-only binary remains
byte-identical, SHA256
`964e694005c73d8a3bd685c53b74cee24e7fc85c35182e21596945d868fe666e`.
All294 exported members are independently checked, bundle2552756 bytes, SHA256
`ad1ccd5d1f32d30ca8071979c411b4d0889a1db80c06c4a88a02ed3501e69347`.
Verified acknowledgement precedes exact helper retirement, exit0. This is native
controlled evidence, not a new full desktop image or installed service.

Four separate direct-UKI QEMU experiments retain original child handles, fresh
private NVRAM and no disks/network/shared directories. They use the unchanged
previous RAM-only UKI, SHA256
`5d6002e2b7f7baa9ef7376f0ad6e65d060197fc776333bfea2f2bb550ff89fd0`.
The first deliberately incomplete argument set also omits the required target;
PID1 therefore freezes without its default/rescue target. It is preserved as a
failed test, host-retired through its original child after the90-second timeout
(PID33612/exit1), never accepted as a nonce test. Corrected controls retain all
required target/console parameters and vary only nonce presence/count:

- Missing nonce: PID61996, exactly `nonce-count`, no positive marker.
- One correct nonce: PID56776, exactly one original boot PASS, no failure.
- Two correct nonces: PID61604, exactly `nonce-count`, no positive marker.

All three corrected controls have original exit0, kernel Power down and unchanged
source hashes. Kernel command-line logs confirm the supplied options replace the
embedded UKI line. Local `root-preflight-2026-11-08/load-options-controls.json`
joins commands, original terminal observations and serial hashes. These explicit
`-kernel UKI -append` controls are not standalone disk or physical HP acceptance.

The same cached signed Fedora RPM is also extracted as data in a new ordinary-
user private namespace. Whole RPM SHA256 remains
`ef6e716e8ab4cc71b4174b6b4be4ef4643db669bfc4bb43c225825be2fede792`;
its expected Fedora signer is checked before extracting systemd-bootx64.efi.
The selected manager is139264 bytes, SHA256
`1dfce189a5a281bfa0bfc0e6b0ada38674893bfdb4f4d378b58a5c29d79a7821`.
Its package is systemd-boot-unsigned259.10-1.fc44, the already selected family and
version; its recorded mixed LGPL/MIT/GPL licensing remains third-party licensing.
This is not a signed EFI binary, Secure Boot approval or production advisory review.

The experiment creates a new128MiB GPT/FAT image, manager at the fallback path,
unchanged RAM UKI at `/EFI/Aetheri/system.efi` and one explicit full-options entry.
Editing/automatic entries/firmware entries/random-seed persistence are disabled
in this test config. All four copied files are read back exactly. No package is
installed and no firmware/device or normal carrier file is modified. The native
export's45 members are independently verified,56262895 bytes, SHA256
`cde51395ce31cf3a14aeef20bf818dcfe7e3346aca22028162588ed11467773e`;
original helper1163449/token30ce3d7cff934d6186f689f555084dc6 retires exit0.

The extracted image SHA256 is
`6324000b66c0fdca684b44461cbc7469cb50b71e1fed228ed225763fc062bdaa`.
An actual standalone removable-USB QEMU boot (PID57684, no kernel/initrd/append
injection, read-only regular medium, fresh NVRAM, no network/shared/physical
devices) passes the original RAM probe and powers off normally, original exit0.
Boot ID is `e2d7ac61-94ef-4d31-85b3-58da9adc6fad`; the medium hash is unchanged.
The exact [manager source](https://raw.githubusercontent.com/systemd/systemd/v259.10/src/boot/boot.c)
sets the child image's LoadOptions from the selected entry. This supports the
next full laptop-image candidate; that integration and HP acceptance remain work.
It does not prove what the HP firmware supplied to the failed original boot.

The final read-only carrier audit again preserves445 normal source inputs,
31 frozen SQL/XML files,19 protected hashes,52 normal P0 files and CLOSED DRM
history. The three existing foreign tracked-document hashes remain unchanged;
the separate foreign UI design note has a newly observed external hash and is
left untouched. No original carrier reboot, physical write or normal source/
service promotion occurs. The OS goal remains active and incomplete.
