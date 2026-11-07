# Fresh reservation admission after a local source fence

7 November 2026. Native C17 and normal P0 update in the existing Fedora 44
Enforcing carrier. The full Aetheri goal remains active and incomplete.

## Implemented boundary

Three more native source paths now use the retained SYSTEM latch: opening a
closed install reservation gate, creating a new canonical reservation/lease/
dispatch, and writing the first no-exec setup intent. Checks occur after source
observations and before mutation; reservations additionally recheck after their
final install/source read. Refusal preserves output markers and journal/gate
state, rather than closing a durable gate or inventing a release.

Exact historical gate/reservation/setup acknowledgments remain observations of
their original records. Lost successful commits retain those original IDs and
times. Conflicting, terminal and uncertain tuples keep their earlier rejection
rules. No response becomes permission to start a unit or issue a helper reply.

Existing helper registration, independently admitted fixed-image early-exit
observations and proven no-exec withdrawal remain available while fenced.
Their actual peer/unit/image/purpose/drain proof is still the caller's duty.
In particular, a source latch never makes the no-exec proof valid for an
executing starter. No schema, migration, XML, public capability or policy changes.

## Native evidence

Both complete builds pass with the existing strict C17 flags. Twelve new cases
pass in Release (37.37s) and ASan/UBSan (35.10s), without skips: gate-before/
observe/existing/lost, reserve-before/observe/existing/lost and setup-before/
observe/existing/lost. Late-read hooks set the actual local latch during real
SQLite observations. Lost-commit hooks commit the original transaction, fence
the owner and return an I/O error. Tests inspect actual gate/operation/lease/
step rows, revisions, times and unchanged outputs; they never restore a backup.

Historical retries are checked with deliberately unused replacement proposals;
new tuples still reject. Failed fresh reservations leave all seven affected
tables empty. Existing reserved cancellation survives an unknown successful
commit and preserves its original completion time. Existing setup observation
and withdrawal preserve the original terminal record. Caller evidence in these
native cases is synthetic, not actual process/supervisor/drain admission.

Three isolated producers omit respectively gate opening, the late reservation
check and first-setup admission checks. Original tests reject each at its
intended assertion. Production archive/test ELF remain unchanged; all mutated
sources, compiler/link commands and failure transcripts are retained separately.

All six selected regression groups pass without skips in Release (258.94s) and
ASan/UBSan (248.42s): native launch/reservation/recovery inventory, the existing
handoff fence and both execution source/wire fence groups. Full bounded JUnit
retains every new case with no truncation or sanitizer diagnostic. All 50
contract/migration and all 24 independent runtime/vector tests pass; the host
suite takes 167.482s. No timeout, policy or verification relaxation was used.

## Normal prototype and remaining work

The reviewed seven-stage update/start/check/connection-check/broker-check/idle/
stop sequence passes. Both normal services are stopped. All 52 files are compared;
owner/epochs, stable lock inodes, policy and non-runtime owner tables are preserved.
Actual connection negatives, User replacement and Root restart with User retained
are covered by that controller run. Installed authority SHA-256:
`2b2ff6e6d6d7b1e65d2930db0c8eb79244e58c2f4b49e79c791813faa25a3c25`.

The final read-only audit matches all 406 source inputs, 28 frozen SQL/XML
contracts, 19 protected old-runtime hashes, four foreign files, exact historical
CLOSED DRM record, pinned SDK 0.1.18 and private SDL04. No own build/native actor,
Aetheri service, protected sidecar or seat0 session remains; Enforcing/tty1.
The old uncertain dispatch/running lease/observed handoff is unchanged, never
opened through SQLite or represented as recovered.

The verified local archive contains 50 files (11,376,640 bytes), SHA-256
`4a73333c66014b197f1e0c682327291525f619c1de596ed635b67b42a6aa3869`.
It retains exact compiler/binary identities, complete logs/JUnit, all three
deliberately defective sources/failures and normal-service before/after evidence.

Next join no-exec helper reply transport and supervisor activation to the source
barrier, while retaining original helper pins and admitted observation/drain
cleanup. The present native historical acknowledgment alone authorizes neither.
Admitted owner/volume continuity, old/intervening sources, external/delegated
holders and durable per-run recovery remain separate work. Preserve the old
v6/User-v4 lost-handle quarantine without adopting, releasing or redispatching it.

Evidence workspace: `local/admission-fence-2026-11-07`. No SDK pin/default,
dependency, new target-execution fixture or repeated NexaUI handoff is introduced.
