# Current authority ELF selection

7 November 2026. Native C17 and the normal protected v2 P0 bootstrap in the
Fedora 44 Enforcing disposable carrier. The whole Aetheri goal remains active
and incomplete; this is one prerequisite for current recovery-owner admission.

## Behavior and boundary

`ae_image_match_self` pins only the calling process with
`pidfd_open(getpid())`. It requires the expected real/effective/saved/filesystem
UIDs/GIDs through the held kernel ProcessFD, then repeats the existing current
label, procfs executable object, ELF/content and metadata observations. UID/GID
0 works without capabilities. The remote-peer API keeps its existing UID0
rejection and still requires a bus-authenticated peer; this self-only call never
creates a substitute remote Root pin.

The normal authority selects its source from the fixed independently provisioned
`system-authority` or `user-authority` file under the protected bootstrap root.
It checks the parent/role label, root ownership, 0555 mode, single link, absence
of file capabilities, bounded ELF and SHA-256 before comparing its running image.
The source path does not come from proc, a peer, a supplied digest or a candidate
header. Root installation is the trust boundary; a hash alone does not authorize
code. Named source replacement with identical bytes still cannot adopt another
object. The explicit offline `--recover-setup` mode selects its already protected
`setup-recovery` file; its original no-exec recovery protocol is unchanged and
was not redispatched in this turn.

Startup checks precede any catalogue open/create/runtime commit. Current-context
queries and User registration/check delegation reopen the same source and repeat
the self check. Failure latches the local image state until a new process start;
there is no automatic source adoption. Existing source/policy immutability,
trusted proc/PID namespaces and single-threaded use remain required. This checks
the ELF, not every loaded library/environment, service role, remote actor, future
effect, old-volume/clone continuity or all holders. Existing authenticated peer,
strict MainPID/unit/invocation/cgroup and catalogue checks remain separate.

The implementation uses the existing image/SHA APIs, with no new dependency,
permission, capability, SQL/XML, persistent image codec or public lifecycle
capability. It avoids extending Root's cross-UID executable access. The source
inventory scan from the preceding milestone is now also installed and observed
at actual normal Root/User startup with `metadata_only=1`.

## Executed checks

Both complete native builds passed. Five affected groups passed without skips:
`linux.image_self`, `linux.execute`, `linux.peer`, `linux.process` and
`authority.broker`. Release took 1.61s; Debug ASan/UBSan took 3.32s.
All 24 Linux runtime-vector/contract tests also pass with the unchanged codecs.

The new native component runs capability-empty UID/GID0 and UID/GID1000. Its
ten cases repeat the main check twelve times each: current object, byte-identical
foreign object, wrong digest, wrong UID and wrong GID. It checks unchanged output
on failures, descriptor counts, invalid arguments, actual EMFILE from a temporary
descriptor limit, and continued rejection of the remote UID0 form. The root
driver explicitly passes leak/halt sanitizer options to its child actors.

The first component attempt failed before the new API: capability-empty UID0
could not traverse `/home/aetheri`, whose actual mode is 0700 for UID1000. The
fixture moved to a root-owned searchable `/var/tmp` directory. No directory
permission, kernel policy, capability or assertion was weakened. The original
failure, executable and build logs remain in the evidence.

An explicit enforcing test runs a byte-identical foreign program object in
each actual authority domain. Its private read-only shadow registry has no
catalogue or writer lock, and cannot mutate normal owner state. With the actual
previous installed programs, both actors pass the absent image guard and fail
later at the missing catalogue input. With the candidate, both fail at the
selected-image check with ESTALE before attempting the catalogue. Owned unit
definitions are bound by inode/hash; exact failed unit/no-main/no-control/no-job/
empty-group checks precede removing those temporary definitions. Unknown effects
would preserve the fixture. This is an early-rejection infrastructure check,
never an app-drain or lease-release proof.

The reviewed `update-owned` controller updated the inspected stopped normal
bootstrap. Actual Root/User discovery, live XML/capability denials, broker checks,
User replacement and Root-only restart with User retained passed. Departure
reduced Root descriptors from 12 to 11. The final authority SHA-256 is
`329c9ff42b127b43c9b71564a953db7ecd874716d301264af13a54f28afa81a3`.

Before/after snapshots cover all 51 normal bootstrap files. Owner/epochs, both
v2 schemas, writer-lock inodes, and every table other than `authority_meta` are
unchanged. Only the selected program replacements, controller/broker slot and
catalogue runtime bytes are permitted differences. The policy hash is unchanged.
The cached normal bootstrap still named the preceding boot; ordinary startup
updates its boot/runtime/revision to the current kernel. An initial collector
incorrectly required boot equality. It now checks the immutable identity fields,
actual current boot, new runtime and increasing revision separately. Non-runtime
tables remain exact. No different-boot observation authorizes old-run release;
the unrelated quarantined executing fixture is unchanged.
Five-second idle observation reports zero CPU time and context-switch deltas
for both one-task services; it is not a general startup/input performance gate.
Both services are stopped afterward.

Evidence: `local/authority-image-2026-11-07`, guest
`/home/aetheri/build/authority-image-final` and
`/home/aetheri/build/authority-image-services`. Enforcing baseline fixture:
`/var/lib/aetheri-authority-image-test-50fe992ebf104cffbf683919760d27a8`;
candidate fixture:
`/var/lib/aetheri-authority-image-test-81689900b1a5488eaad2c748fd36d721`.
Their unit definitions are retired; closed source/log fixtures are retained as
explicit test evidence, not an installed application or background service.

The collector verifies 90 evidence files, exact JUnit case names, actual C17/
Release/ASan compiler flags, original/candidate executable hashes and both closed
actual-domain fixtures. Final tar SHA-256:
`53e0b3b450167a9f7aa7adaa08e2c7b8214973443f9ba2cc60393024b7c09d3b`.
Final audit: 401 host/guest source inputs, all 28 frozen SQL/XML and 19 protected
quarantine hashes, the four foreign files, exact historical CLOSED DRM record,
NexaUI 0.1.18 and SDL04 remain as expected. There are no own native actors,
Aetheri services, protected SQLite sidecars or seat0 session. Enforcing/tty1.
Git history records the publication; no whole-OS or complete recovery acceptance.

## Remaining work

Independent volume/clone continuity, complete current-counterpart recovery
admission, prior-owner/future-effect fencing and holder settlement remain work.
The old quarantined v6/User v4 run is not reopened, migrated, adopted, released
or purged. Its lost handles stay lost. Normal session/image, brokered documents,
complete app removal, updates/installer, accepted apps, experiment desktop and
hardware/human/final UI acceptance remain incomplete.
