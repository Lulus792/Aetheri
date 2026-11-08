# Explicit laptop boot path

The previous [video diagnosis](2026-11-08-laptop-image.md#video-narrows-the-physical-boot-failure)
and actual missing/one/duplicate-argument controls motivate a same-version
systemd-boot entry. The HP's actual failed command line remains unobserved.
The [new profile](../../../spec/boot-manager.md) preserves the root/data guards,
pins the additional139264-byte EFI payload from the existing signed RPM and
independently validates the entire managed ESP tree and exact full options line.
No production Secure Boot, hardware or daily-use acceptance follows.

The user explicitly authorizes a NEW flash once the remaining tests finish and
reports the stick reconnected. Read-only metadata and a held descriptor again
identify Intenso Speed Line, serial212300100050, USB/removable and31457280000 bytes.
The three previous Aetheri partition GUIDs/types/extents match. Only the ESP has
a Windows volume GUID; no drive letters or ext4 Windows access paths are present.
This supersedes the earlier instruction to preserve the stick pending permission;
it never authorizes replay of the old completed operation. The new source approval
is closed until full candidate acceptance. Windows replacement guards and the
actual unbuffered regular-file write/readback suite pass15 cases. No physical
write has occurred in this new operation yet.

## Build and preparation evidence

Managed-build-v1 uses fresh private mount/tmpfs state in the enforcing original
carrier, with the ordinary UID1000 builder. It keeps committed desktop/controller
inputs rather than folding unrelated uncommitted keyboard work into this image.
The complete new laptop base builds successfully, including the root-only failure
diagnostics. Preparation then fails closed because mtools creates an unreviewed
VFAT long-name entry for a mixed-case directory. The first full attempt is not
accepted. Its full export/verification and original helper retirement must finish
before reusing its complete base image. Original helper1166740/token
`c0131dee85bd469c9ac273bb72bc815e` retains ownership until that export completes.

The failed worker's broad diagnostic export also includes the complete root/data
intermediates. Before its private workspace fills, a read-only observation checks
the exact tmpfs source/target/mount491,2GiB limit and available memory. Only that
owned namespace's workspace limit is increased to3GiB to preserve the complete
export; no global/shared/carrier filesystem is resized. The host receipt records
the observed available2071633920 bytes. Never apply this to the original carrier
or guess that a missing reply authorizes another build.

A bounded read of the immutable completed UKI and manifest is saved separately
while the full exporter owns its tree. The UKI matches its manifest. It must still
join the same full export before assembly can accept the base. This bounded read
does not prove an absent complete disk image.

The correction uses uppercase8.3 directory/EFI destinations; only loader.conf
and aetheri.conf need reviewed VFAT names. Their single name entries, termination,
padding, checksum and exact allowed names are checked. The FAT hidden-sector value
also retains the known2048-sector placement. A separate prepare-v2 attempt refuses
insufficient memory BEFORE mounting or starting a worker. Its two exact empty,
root-owned0700 non-mount directories are inspected and removed without recursion.

Prepare-v3 uses a fresh512MiB private workspace and1GiB memory preflight; it does
not repeat the full base build. All7 native profile/FAT/corruption/source-binding
tests pass. The39 exported members are independently verified,59549273 bytes,
SHA256 `c9b67d8c11b6712b274d1f7b9f0dc96dae01ba84aaf368ab27155af159b6cf66`.
Original helper1169682/tokenbb9f662be9844d3ead1057854f84c789 receives its exact
verified acknowledgement and retires exit0. The same7 actual-byte tests pass on
Windows after extraction. The seven legacy GPT/FAT/UKI corruption/no-spawn cases
also pass on Windows against the unchanged previous RAM image.

The complete first attempt subsequently exports all2002 members; every member is
independently verified before acknowledgement and original helper retirement,
exit1 as expected for its failed preparation. Bundle811103485 bytes, SHA256
`f524888eceeab1eba3341784708008195e0afb3d5e93f603c08789ecb1e4aebc`.
The separately prepared UKI/manifest join those same verified members exactly.
The complete base is extracted into a new sparse regular file with its full
31457280000-byte hash checked, then independently inspected. A first assembly
fails before copying because it references a nonexistent image-I/O helper;
the empty image-managed-v1 output is preserved. The correction uses the existing
set_length primitive. A bounded real-file regression now checks the copied bytes,
zero overwrite, preserved source, fresh-output refusal and low-space refusal.
It mocks only profile admission/space for the4KiB I/O case, not file operations.

The first native run of that new I/O case correctly hits the8GiB headroom gate on
its512MiB test workspace; the bounded fixture is corrected to supply its explicit
test budget and also check budget rejection. Native review-v5 passes all8 cases,
4 independently verified exported members,2085 bytes, SHA256
`75404824712ff5c2c661e3879c5ada53d3d0936676756025748add3d2c20a68f`.
Original helper1171022/tokenf68d194f17c64828afdfd7a6ecf75c85 retires exit0.
Earlier review-v4 is retained as failed test setup, with verified export and exit1;
it never establishes acceptance. Full assembly completes in the fresh v2 output.
The complete non-ESP regions and both final source/candidate inspections pass.
The two-cold-boot no-UART desktop controller is then started with a fresh copy.

The replacement-flash suite also passes on Linux:15 cases, two Windows-only
skips. Its4 exported members are independently verified,2396 bytes, SHA256
`c8a1472e9b2be03f2cfcc34f7c06256a8b2b3f7aa18e53accb0a3408fbc4e7f1`;
original helper1170118/token59b170a6946a474b9fd418ab6bed900d retires exit0.
Actual Windows evidence remains15/15 with no skipped case. Read-only preservation
again verifies445 normal carrier inputs,31 frozen SQL/XML,19 protected hashes,
52 normal P0 files and CLOSED DRM history. The separate foreign UI note is
observed changing externally and left untouched; the other foreign hashes match.

Next independently assemble the complete candidate, prove all non-ESP bytes
unchanged, run two no-UART desktop boots with actual capture review, then inspect
the data/proof files read-only. Only after that may the exact new source be pinned
for the separately authorized fresh physical write/readback. Preserve all failed
attempts, original carrier state and foreign documentation. The full OS/apps,
normal lifecycle/removal, updates, browser, hardware and experiment desktop goal
remains active and incomplete; NexaUI handoff stays closed.

## Complete candidate and first desktop sequence

Assembly-v2 completes with original exit0. Its base full hash is
`9107ba286ab34ede59a9a46ee0c8867bc51041afb052938c805847eb27734d63`;
candidate full hash `4d8e9b00bf5594ed4037421d645667804eee71d1ea475081731da17b1a172da9`.
Candidate manifest SHA256 is
`c982ff77d465c877b1325be59260cd029c11dacf9959d33776be482ceba697d7`.
These are source/artifact observations, not physical flash or complete acceptance.

The first no-UART sequence reaches desktop, Terminal and the actual command/file
output. Those three captures are actually reviewed and acknowledged; its first
original QEMU62172 exits0 with guest shutdown. The second original QEMU69056 also
reaches the desktop. The root agent misses its240-second engineering capture
review window. The controller correctly fails, preserves the copy/history and
retires that original VM, exit1. The final desktop capture is subsequently viewed
but never retroactively accepted. Only the untouched original carrier9520 remains.
This is a host review failure, not a demonstrated OS boot failure. A new fresh
no-uart-v2 sequence starts from the unchanged pristine candidate, never reusing or
repairing the failed copy. Its complete two-boot/offline proof is still pending.

## Full VM and offline acceptance

The fresh no-uart-v2 sequence passes both complete boots with six actually viewed,
hash-bound captures. Original QEMU69572/UUIDae35fece-8a23-4979-82f1-dfb97e3ccc50
and QEMU56852/UUID879682bc-b6ab-4da0-abd0-e8f241983264 both exit0 with genuine
guest-shutdown events. Controller original exit0 is recorded separately; the
sequence SHA256 is `185445113ec5f1f132caf255468920f5ed7de8eab0ecce7a2e762de9298c7390`.
EFI/system/GPT ranges and the pristine source remain unchanged. The visible
document survives the second boot. No serial/parallel port, network, shared
directory, direct kernel injection or physical device is used.

Independent ordinary-user e2fsck/debugfs then verifies the clean unchanged data
image and all11 proof files. Actual boot IDs are
`4b693f9b-d2c5-4e9f-afd4-038449fa3862` and `245a5c59-64da-42a2-843e-7d71cfc083b1`.
UID/GID1000, zero capabilities/NoNewPrivileges, ro root, rw/nosuid/nodev data/home,
tty0-only consoles and all three required data/device units match. The14-byte
document is exact. Data SHA256 stays
`917b4975b88aae799cc9a16adf032d1982e0001e5c61e80c8831d8e2da5c88b5`.
The less-than-one-day future-time warning is retained; no clock acceptance.
All49 exported members are independently checked,7070 bytes, SHA256
`17919a61af0652cea1057cebd5315ada18932d3cbf96adcab0f29cac9a260bb3`.
Original helper1175319/tokene607e48a4b884561a35cffc7af9e6799 receives verified
acknowledgement and retires exit0. No original carrier reboot or normal update.

Local acceptance-v2.json independently joins original controller exits, captures,
sequence/source bindings and the read-only inspection. The exact new source is
now pinned in the replacement writer; all15 actual Windows cases pass again.
The user's renewed instruction authorizes one fresh physical replacement. Prepare
the new protected intent and retain its original elevated process; no physical
write has occurred at this checkpoint. Physical HP acceptance remains unproven.

## Authorized replacement flash completed

The fresh protected flash-intenso-v2 intent SHA256 is
`c844e2355c26a37b36d399697cf75ec03e69fafd675d2d30d38ff4b6f21b318a`.
Native Windows RunAs succeeds using the signed existing Python runtime. Local
RemoteSigned policy is respected; no execution-policy or firmware change occurs.
The parent retains the originally returned SafeHandle for writer43696 until
actual exit0. Its protected native-exit record and the original host exec19225
terminal result agree. The earlier completed format1 operation is not replayed.

The source is held read-only with write/delete sharing denied and freshly hashed.
The exact Intenso is reidentified, then its held device and current three-partition
layout are joined. The only mounted Windows volume is the exact ESP GUID; its
held disk extent is verified, then it is locked and dismounted before any write.
Repeated device/partition checks pass. Every31457280000 bytes, including zeros,
is written with aligned unbuffered WriteThrough I/O and flushed. Complete direct
readback matches the accepted source SHA256
`4d8e9b00bf5594ed4037421d645667804eee71d1ea475081731da17b1a172da9`.
Logged writing takes1077.72 seconds and readback/final context checks343.12 seconds.

Independent post-exit verification joins all242 complete chained records, exact
phase order/progress/total bytes, original process/intent/source bindings, held
device/layout and cleanup. No failed or cleanup_failed record exists. All writer
handles close before complete; original exit0 also confirms final log closure.
Journal SHA256 is
`72ad08e179d0b4123afcafca95580b64fe17f6c3ec84184d29efa131eaaa865b`.
The protected acceptance.json retains the joined physical result. This is full
write/readback evidence, not secure erase, cache power-loss assurance or successful
HP boot. The user can safely remove the completed stick and retest the HP; work
continues without waiting for that test. The image still uses its existing US map.
Later controller/keyboard changes are independently tested source, not another
reason to reflash this completed medium or discard subsequent user data.
