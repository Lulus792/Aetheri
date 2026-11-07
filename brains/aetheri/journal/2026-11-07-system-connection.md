# Reciprocal current Root connection

7 November 2026. Implemented in native C17 and updated in the normal protected
Fedora 44 Enforcing P0 bootstrap. The full Aetheri goal remains active/incomplete.

## Behavior

User retains the actual bus-authenticated Root ProcessFD, unique connection and
current active MainPID/unit/invocation/boot/cgroup context. Its independently
protected selected Root unit and authority epoch must agree. Discovery and
Broker1.Register/Check use request-correlated successful replies; event
incarnation and Root runtime remain distinct. Normal private catalogue,
reservation/receipt and no-exec setup calls now use this same binding instead
of resolving the well-known name independently for each call.

Root also checks its own selected active MainPID/unit at protected context use.
Its existing local ELF check supplies source admission without giving User
access to Root's executable. The controller adds one root-owned 0444 selected
unit slot and repeated publication flushes. Existing normal owner/epochs,
catalogue schema and public capability sets remain unchanged.

A live context/configuration/runtime/protocol error latches unavailability.
Restoring a configuration file is not renewed actor admission. Actual original
process exit/name loss permits dropping only the connection identity and later
fresh registration. No automatic request replay, old-run adoption, lease release
or holder drain follows. The dedicated outbound bus/event remain borrowed;
nested calls/close reject with EBUSY and post-fork use with ECHILD.

## Source and regression evidence

The official upstream stable Linux 6.19.10 pidfs source was retrieved directly
after browser retrieval failed. Its PIDFD_GET_INFO path provides credentials/
cgroup metadata without the namespace-opening ptrace check. SHA-256:
`49e14ed42427e9c5e5ee7c71dba91596e2d577cbddf4138e1153870782e00b3a`.
This source review is not a claim about all Fedora patches or other kernels;
actual cap-empty cross-UID service acceptance is independently tested below.

The first compile exposed the removed registration boolean's remaining use and
an obsolete callback; both were corrected. The initial focused component passed.
An expanded same-process/name release/reacquire regression then failed: a
synchronous owner query saw the final original owner while the loss signal was
still pending. The implementation now dispatches at most 64 queued messages
around the owner observation. Queue exhaustion rejects; no idle poller is added.
Callbacks retain in-use descriptors until the outer call leaves.

The final native component uses real private-bus replies, cookies, ProcessFDs,
name changes and process exits. Only domain/PID1 context are controlled in this
component; it is not the enforcing cross-UID proof. Cases cover repeated calls/
FD counts, event/runtime distinction, malformed discovery/registration/Check,
wrong peer UID/domain/selected unit, live context/selection quarantine, unknown
reply timeout without resubmission, explicit rejection without resubmission,
wrong destination, actual departure/replacement, same-name reacquisition,
unchanged error outputs, nested close/use and fork refusal.

Both complete native Release and Debug builds pass. Five selected groups pass
without skips in Release (3.62s) and ASan/UBSan (3.88s):
`authority.system_connection`, `authority.broker`, `authority.projection_wire`,
`linux.peer` and `linux.process`. Exact compiler commands retain C17/Werror,
Release O3/NDEBUG and Debug sanitizers. All 50 host contract/migration cases
(134.986s) and all 24 Linux runtime-vector cases pass.

A separate compiled producer deliberately omits queued-signal dispatch; the same
final component detects its ownership-loss failure. The mutation changes no
public source or native test. Its manual compile/link initially used ordinary
compiler dependency-output paths; the unchanged original commands restore those
metadata files, and the original test ELF remains byte-identical:
`ad16272385ffb366e095eaec98f8833e5c45c5d0fffaaf928a2e17d24c2dae2e`.
No production program is replaced by the mutant.

## Actual normal-service acceptance

The explicit controller updates the verified stopped normal bootstrap. The new
policy adds only reciprocal process getattr and proc directory/file metadata
reads. Explicit neverallow rules prohibit User ptrace and Root executable
read/open/map/execute. No capability, dependency, SQL/XML or raw app bus access
is added. The new authority ELF SHA-256 is
`df418f839cfd3414b0643fc1602550ee2f5cc488050b7e52531de13aa8b1d495`.

All seven actual controller stages pass: update, start, discovery/denials,
connection-check, broker-check, idle-check and stop. The reciprocal negative
selects an already owned historical Root unit while both original processes
remain live. User rejects it; restoring the original selection does not clear
User's quarantine. A fresh User succeeds while the original Root PID is kept.
The probe's protected stages are retained as CLOSED. An interrupted probe requires
manual reconciliation; no automatic slot restoration or unknown dispatch retry.

Separate broker checks retain the original Root during User replacement, then
retain the original User through Root restart. Root FD count drops 12 to 11 on
User departure and returns on binding. Both one-task services show zero CPU and
context-switch deltas during the 5.029s idle observation. Services are stopped.

The before/after collector compares all 51 original files and the one added
selected-Root slot. Only reviewed program/policy/controller/slot and authority
runtime changes are allowed. Owner/epochs, stable lock inodes and every
non-runtime table remain unchanged in both ordinary v2 catalogues.

## Preservation and remaining work

`local/counterpart-2026-11-07` retains source review, failed/preliminary/final logs,
exact JUnit cases, C17 commands, native binaries, deliberate counterexample and
service snapshots. All 40 exported evidence files are verified. Archive SHA-256:
`03d8e93c3d5f6a86fdf3e4eacb8e8f92cb9b07b9101ded51372d7147057d1884`.
Audit checks 405 source inputs, 28 frozen SQL/XML contracts, 19 protected old-runtime
hashes, four foreign files, exact historical CLOSED DRM, SDK 0.1.18 and SDL04.
No native/build actor, Aetheri service, protected sidecar or seat0 session remains;
the guest is Enforcing on tty1. Git history records publication.

This is reciprocal current protocol identity, not complete recovery-owner
admission. Independently admitted volume/clone continuity, loaded dependencies/
protected configuration, prior-owner and future-effect fences, complete holder
settlement and recovery coordination remain work. The old quarantined v6/User v4
executing attempt is not reopened, migrated, adopted, released or purged. Its
lost handles stay lost. Normal session, brokered documents, full removal,
updates/installer, remaining apps, experiment desktop and physical/human/final
UI acceptance remain incomplete.
