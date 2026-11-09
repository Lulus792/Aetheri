# Confined audio under the user manager

This explicit audio-lab fixture joins the existing confined graph, routing
policy, native volume control and finite player to the real systemd user
manager. It checks original user-service actors through the native observer
and compares the complete synthetic PCM independently. Normal desktop audio,
physical devices and Settings integration remain separate work.

## Processes and permissions

The user manager owns the five role services. All run as UID/GID1000 with the
existing empty capability boundary and NoNewPrivileges. Roles retain the
[base MAC rules](audio-mac.md), four creator-specific memory types and actual
foreign connection/secret/TCP denials. The additional
`policy/audio-user-test.cil` permits the lab's existing unconfined user manager
to enter these test domains under its user role. Both temporary modules are
removed after the exact owned actors and resources retire.

The unconfined manager and same-domain administrative tools are trusted fixture
infrastructure. This transition grant must not become production app admission.
Normal service provenance, a protected manager/authority boundary, per-app
permissions and mutation/delegation exclusion still require enforcement.
The user profile requests role MAC and the existing capability/NNP boundary;
it does not request the system fixture's mount/device namespace layer.

A finite ordinary-user worker owns one original unique bus destination and
unit references. The root controller transfers only selected log/process FDs
over its inherited AF_UNIX SOCK_SEQPACKET socket. Received handles are close-on-
exec, bounded and closed after message construction/call retirement. Malformed
JSON, foreign ancillary data and truncated messages close every received FD.
This is private administrative transport, not an app API or permission grant.
Connection loss preserves unknown operations; it never proves unit/holder drain.

## Test session lifetime

Each case has an ordinary root-owned `.target` file under `/etc/systemd/user`.
systemd259 rejects transient target units, so the fixture writes a fresh exact
file, reloads the original manager and checks its actual FragmentPath. Content,
owner/mode and file object are retained for bounded removal. Unit files are
removed and the manager reloaded after the associated original services end.

Services require the already active test target. Graph/policy belong to it;
the policy is additionally ordered after and bound to the original graph.
Before target stop, both original source pins must be live with their exact
active main/invocation. One target stop must end both sources **before** any
independent unit/cgroup retirement. No second StopUnit is sent to mask missing
propagation. Acknowledgement is distinct from dead original pins, terminal
units/no jobs and held-cgroup ENODEV plus missing names.

These targets coordinate fixed tests; they do not authenticate a login or
prove an active local Wayland session. The current stop check follows the full
finite reference signal, rather than interrupting active playback. Lock,
VT/session revocation, suspend and unexpected restart retain their own gates.

Six finite native observers separately join original graph/policy handles,
current images/contexts and service invocations to the retained user manager.
Their latched loss is checked after source end. They do not release leases or
substitute for the controller's independent unit/cgroup/extra-FD observations.
Normal integration can use the library in an existing session owner; these
standalone observers are test executables.

## Run

Use only the registered enforcing audio lab, prepared exact SDKs and protected
root-selected source/binaries. The build directory contains the existing
`probe_audio_mac`, `aetheri-audio-check`, `aetheri-audio-control-probe` and
`probe_audio_actor` targets. The original P0 guest is refused.

```text
python tools/test_audio_user.py SDK POLICY CONTROL BUILD SOURCE FRESH_OUTPUT
```

The fixture creates four memory factories, then unity/half/muted graph runs.
There are19 role services, three session targets, six finite actor observers
and one finite bus worker. Complete PCM, original-peer control/player checks,
actual image/library maps and negative MAC controls remain required.

`tools/test_audio_user_controls.py` has seven Linux-only descriptor/channel
controls; it opens no bus and changes no unit/policy. A retained capture reader
can be explicitly closed after independently settled actors/effects. Changed
FD identity or unsettled state preserves it for inspection. This repairs a
test-abort resource path; closing an observer is never general drain.

The [execution record](../brains/aetheri/journal/2026-11-09-audio-user.md) preserves
successful runs, rejected setup/counterexample attempts, exact exports and the
remaining normal-session/device boundary. An interrupted fixture requires
inspection; this tooling is not production recovery, update or removal.
