# Original issuer identity — 7 November 2026

This change connects the original Root bus connection to the actual issuing
process and its independently selected executable. A unique bus name, current
UID, service name or executable digest on its own is insufficient. The existing
unit allocator and RPC journal remain separate obligations.

## Implementation boundary

The Root-only schema v11 adds immutable `unit_issuers` rows. Native
enrollment must precede REQUEST; a later exact retry acknowledges the same
observation without replacing its timestamp or permitting another effect.
Explicit v10-to-v11 migration adds no observations to old allocations. A legacy
custody without an issuer stays visible but cannot acquire new effect steps.
Normal creation remains v2 and opening never migrates. Published SQL/XML v1-v10
are unchanged. No wire interface, public capability or normal service changed.

`linux/issuer` retains the calling Root's bus-authenticated ProcessFD. It checks
the exact expected main service, current invocation, cgroup and raw security
label, then reidentifies the original pidfs object and verifies the independently
selected original Root ELF. The same-service child negative matters: membership
in a service's cgroup must not substitute for its actual main process. The
existing remote process-image API still rejects UID0; only the existing self
image API is used for the calling Root.

`AETHUIS1` serializes these bounded observations, including the existing unchanged
`AETHIMG1` image format. It carries no descriptor, numeric PID or bearer right.
The pure image codec has moved to core; Linux retains file/process observation.
The recovery inventory retains issuer retirement as unsettled. This record does
not implement owner transfer, bus-loss recovery, holder drain or lease release.

## Test evidence so far

- The independent Python issuer codec and explicit migration tests pass 3/3,
  including every DDL boundary, lost commit acknowledgement, process termination,
  page exhaustion and a foreign schema.
- The unchanged host contract suite passes 55/55 in 186.814 seconds; the frozen
  runtime suite passes 25/25.
- Candidate v4 builds both profiles. Its native issuer codec, populated native/
  Python interop and eight catalogue cases pass in Release and ASan/UBSan:
  106.015 and 88.985 seconds. The legacy case proves no backfill/new effect.
- Candidate v6's separately supervised normal driver passes in 20.171 seconds.
  Actual issuer capture, wrong role/label/image, inherited connection, same-unit
  child and descriptor cleanup are exercised before work begins. Its native
  reserved lease and operation histories remain retained.

Final source v9 has 444 inputs and builds both profiles. All five groups pass
Release and ASan/UBSan in 438.085 and 469.514 seconds, including all 32 drivers
per profile (385.38 and 407.40 seconds). All eleven affected regressions pass
without skips in 394.448 and 446.280 seconds. The checked export contains 541
files and 2,907,189 bytes, including actual commands/artifact hashes, all four
JUnit suites and both retained owner histories. This accepts the scoped native
prototype; normal confined provisioning and lifecycle recovery remain open.

The read-only preservation audit checks all 444 source inputs, 30 published
SQL/XML hashes, 19 old-quarantine hashes, 52 normal files/controller and the
unchanged CLOSED DRM/SDK0.1.18/SDL04/vendor inputs. All 64 final administrative
driver units/images are retired. Foreign planning/UI changes are preserved and
excluded from this implementation.

## Preserved fixture failures

V3 stopped at `-Wshadow` in the new legacy test; the local sentinel was renamed
without changing warnings. V4's first supervised driver failed before exec at
systemd's STDOUT setup. Test output now uses the journal rather than asking PID1
to create a log in a home directory. No policy permission was added.

V5 completed the native normal case, then its observer found `ENODEV` on the
original cgroup events reader. PID1 had already deactivated the empty cgroup
while the successful RemainAfterExit unit was active/exited. The test now accepts
that exact pair of held-reader ENODEV and missing original name; absence alone
is still insufficient.

V7 completed eight drivers, then the ninth failed in the pre-work barrier. The
parent had created the final gate name before writing its byte. A child could
open it in that interval and correctly reject its empty size. Both the native
start observation and Python gate now use anonymous inodes, complete writes,
fsync and create-only publication. The permanent two-process regression pauses
before publication and checks that no name is visible. It passes the corrected
helper and rejects the unchanged v7 publisher (exit 0 versus 1).

The 32-driver aggregate CTest budget was adjusted before the full attempt for
the added independent PID1/image/retirement work. Each original driver work
observation keeps its 40-second deadline. No failed assertion or individual
deadline was relaxed. The first failed dispatches were preserved rather than
restarted. Three known terminal administrative drivers subsequently received
inspected operator cleanup: their exact original invocation, source/image
bytes/inodes, zero Main/ControlPID, no job and absent cgroup were checked.
V4/V7 had no catalogue yet; V5's two native catalogues remain byte-identical and
its lease stays reserved. One cleanup assertion incorrectly expected Stop to
clear a failed marker; its original intent was retained, Stop was not replayed,
and only the two inspected failed markers were reset before removing their test
configurations/images. Logs, source bindings, original terminal observations
and native histories remain. This is not process reidentification or production
custody retirement.

All development-driver supervision is UID0 administrative test infrastructure
in the enforcing disposable carrier. It does not establish production confined
Root admission, Aetheri app execution, removal or general recovery. The older
executing quarantine is untouched.
