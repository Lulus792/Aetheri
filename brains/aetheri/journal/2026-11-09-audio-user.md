# Confined per-user audio and test-session stop

9 November 2026. The previous goal turn made progress: `dba762a` published the
native original-user-service observer and actual fixed sleep-unit checks. This
turn joins that observer to actual confined graph/policy/control/player audio
under the user manager. Read the [fixture contract](../../../spec/audio-user.md).
Normal desktop/device/Settings admission remains work; the full OS goal is active.

Local inputs, original scripts, logs and receipts live under
`local/audio-user-2026-11-09`. Preserve failures and counterexamples separately.
The separate guest uses the existing eight-package runtime alignment; no new
dependency family or package is adopted. Original source/policy/services and
USB data are not targets of these tests.

## Implementation

The base audio fixture now injects its supervisor observations and expected
role/memory contexts. Its default remains PID1/system roles. Stop observation
can run after independently observed target propagation without sending another
stop request. The user fixture adds a finite UID1000 bus worker with a retained
unique manager destination and bounded inherited Unix FD channel. FDs close on
invalid/truncated messages and after the typed D-Bus call. No host TCP listener,
installed API, permanent watchdog or database is added.

Temporary `audio-user-test.cil` adds only the user-role/manager transitions and
user-broker log-file permissions on top of the existing role policy. The lab's
unconfined manager is trusted infrastructure; this is not an acceptable general
grant to production unconfined apps. Real role denials, cap-zero/NNP, selected
program/library maps and full PCM remain checked. Mount/device namespaces are
not requested by this separate user profile. Physical device admission remains
unimplemented here.

Each case uses a fresh root-owned ordinary session target, selected by actual
FragmentPath and retained file metadata/hash. Services require the active target;
source services belong to it and policy is bound/ordered after graph. Both
original source handles must be live immediately before stop and dead after
the one target stop. Native actor loss and independent unit/cgroup/extra-FD
retirement are separate. These are fixed coordination targets during an SSH
login, not local Wayland-session authentication or an in-flight audio-loss test.

## Retained attempts

- Offline profile v1 runs five native Linux channel/FD controls and compiles both
  modules in a copied policy store. All1,330 original store/config files and the
  loaded original policy remain unchanged. Its five-member export SHA256 is
  `131430ad13b8c74b887080567c41d102bbc9fc648ef7da223b8fcca9cad47a77`;
  original helper exits0 after independent verification/acknowledgement.
- User v1 passes all19 actual role services and six native observers, with full
  unity/half/muted PCM, before adding session targets. Independent export SHA256:
  `0996442b1ac33c63ceee9cf199f88a4c27a9987c2b7a11d0ea416324f6d93d25`.
- User v2 finishes its four memory factories, then rejects StartTransientUnit
  for a target with exact remote InvalidArgs. Keep its pending original start.
  Independent administrative inspection/removal retires only its temporary
  resources; it never replays that start or clears its historical uncertainty.
- V3 is mistakenly dispatched before that retirement process is terminal. Its
  foreign/pending-module guard refuses setup before creating a private root or
  role service. Wait for the original retirement's actual exit, then use a fresh
  attempt. The old dispatch/guard record remains failed.
- V4 uses ordinary protected target files and passes all cases. V5 additionally
  records live original sources immediately before stop, with19 role services,
  three target stops and six native observers. Its independently revalidated
  export SHA256 is
  `500e04eb826386f0c8cefbe275192c9271b5ab65ded8e8f750f7b070949cbc1c`.
  One early export read during a later owned test's temporary policy load is
  rejected by the global module-list check; export succeeds after that test's
  independently observed retirement. No runtime failure is renamed a pass.

The first proposed counterexample removes only PartOf. The actual check still
passes because Requisite also propagates an explicit stop. Preserve the failed
counterexample expectation; do not weaken the working dependencies. Nine role
services and both observers have already retired in that run. A separate
administrative receipt removes its two unused target files and temporary tree/
modules without rewriting the failed record.

Removing both Requisite and PartOf produces the intended failure: the target
is inactive while both original sources remain live. Original handles are kept
for explicit fixed-test retirement. That counterexample exposes another test
resource path: the aborted case still owns its capture FIFO reader, so unmount
fails while the controller remains alive. Its exit closes that bounded observer;
independent inspection then disposes only the exact temporary resources. Keep
the original negative detection and cleanup failure. This is not app recovery.

The capture reader now has explicit owned FD/object state and an explicit
retirement method. Pending effects, unsettled actors and changed descriptor
identity refuse closure. Offline profile v2 initially omits the newly imported
base-fixture module; preserve its failed source/export as a fixture dependency
error. V3 includes it and passes seven native Linux controls plus both offline
policy compilations. Its five-member export SHA256 is
`b26a48764d0d28d35438b34ef3dc6f10566a6336e9fc44509302ea96fa2daaa7`,
with actual original helper exit0 and unchanged original store/kernel policy.

## Final changed-source evidence

Corrected negative v3 removes both session relations and detects the inactive
target with both original sources still live. Its original nine role units,
three target files and worker retire through explicit fixed-test cleanup. The
two native observers time out with exit1 while sources deliberately stay live;
those are retained negative observations, not successful launch results. The
capture reader is independently observed as FD7/object60:952 at the original
unity FIFO, then explicitly retired before successful unmount. Final state is
`expected-negative-retired`, never `passed-retired`.

User v6 passes the current capture/ownership code,19 role services, three targets,
six native observers and the original worker's exit0. The17 current bound source/
profile/probe hashes and complete PCM revalidate on the host. Its export SHA256:
`d995579211ffbf69f95038ea0652e55e6f67aac1a5ffc4b122a2f55f32c90f13`.

| Case | Captured stereo frames | Complete reference | Maximum sample error |
| --- | ---: | ---: | ---: |
| Unity | 49,152 | 48,000; start1024 | 3.7005065955941774e-9 |
| Half | 49,152 | 48,000; gain0.125 | 4.6256332444927217e-10 |
| Muted | 49,152 | Producer drains48,000 | All samples zero |

All raw PCM bytes match the earlier system/user captures. No frame, tolerance,
header or failed record is modified to pass. The final default system-scope
regression also passes19 role services, all memory/negative controls and the
same complete PCM. Its44-file independent export hashes to
`1ff4ac201f954e492396e1df2af1264f43dc5da079e504db4c818a7c673026bb`.
All eight aligned runtime packages still pass rpm verification without output.

Before shutdown, all10 run directories are exported, including failed dispatches,
negative expectations and their separate administrative receipts. The complete
archive hashes to
`90f44802708a115294a79fd34774fb40fa5ad7346fd306ae2f2ba25f9e23e451`.
Final host verification joins the current source hashes, seven executed native
controls, positive exports and the actual corrected negative. Independent live
inspection finds no private mount roots, target files, role processes or either
temporary module; SELinux remains enforcing.

Original lab QEMU70188, UUID `033ac49e-68bb-47a3-9c9c-26a812dc8300`, exits0 after
shutdown. Guest boot was `fb03f108-8440-46b1-81a5-fdac9b607fb6`. The final read-only
original-carrier audit confirms445 source inputs,19 protected hashes,31 frozen
SQL/XML contracts and52 normal P0 files, authority/CLOSED/quarantine and all four
foreign worktree files unchanged. The original guest is not restarted or
reconfigured. USB contents remain untouched.

## Sources and remaining boundary

systemd v259 [execution properties](https://raw.githubusercontent.com/systemd/systemd/v259/src/core/dbus-execute.c),
[target implementation](https://raw.githubusercontent.com/systemd/systemd/v259/src/core/target.c),
[transient-unit check](https://raw.githubusercontent.com/systemd/systemd/v259/src/core/dbus-manager.c)
and [unit dependencies](https://raw.githubusercontent.com/systemd/systemd/v259/man/systemd.unit.xml)
support the exact signatures, ordinary target requirement and dependency
semantics. Runtime results and own original-object observations remain separate
from those source statements. No broad security assurance is inferred.

Protected production unit/configuration/dependency provenance, manager/app role
separation, normal session/login/revocation/holder coordination, Settings and
physical HP jack/Bluetooth remain work. Existing default-clock timing limitations
remain open. No normal service/profile, public capability, frozen SQL/XML or
physical medium is changed by this administrative fixture.
