# SDL native wait and original Terminal readiness

The full Aetheri goal remains active and incomplete. This record distinguishes a
confirmed SDL queue/arm race from the earlier untraced native Text/GetState/Close
findings. Similar symptoms alone do not prove that every historical failure has
the same cause.

## Evidence and cause

The first diagnostic-only existing Terminal runs passed Release/ASan/UBSan in
`terminal-query-diagnostic-x6jsd7x4`; those positives did not repair anything.
A bounded sixteen-focus-transition workload then failed on the original Release
producer at cycle 12 in `terminal-query-workload-h9v8ifcl`, while its instrumented
profile passed. A traced sequential comparison also passed; observation can alter
the timing of a race.

The four-client private-bus workload in `terminal-query-monitored-1kuq5h3m`
captured a Release failure at cycle 15. Its original host reports a successful
worker `SDL_PushEvent` at trace order 180458, then no `SDL_WaitEvent` return until
order 180459, **2,012,865,444 ns later**. Exact private-bus sender/call/reply cookies
show four `GetState` replies about 2.025 seconds late and four further replies
about 1.023 seconds late. The replies exist; cleanup wakes the UI too late for
the independent clients. This is more specific than an absent output-group name.

The pinned SDL3-3.4.20 source checks an empty event queue before arming
`wakeup_window`. A worker can enqueue in that gap while no native wake is armed;
the UI then enters a native wait with the event already queued. The selected
Wayland backend sends a display sync only when a wake was armed. The application
uses the documented thread boundary: worker-side
[PushEvent](https://wiki.libsdl.org/SDL3/SDL_PushEvent), main-thread
[WaitEvent](https://wiki.libsdl.org/SDL3/SDL_WaitEvent), inspected 6 November 2026.
Versioned source from the verified archive supplies the concrete implementation;
the live API documentation is not proof of a particular release's internals.

## Controlled before/after

`sdl-wake-race-fcvwumvb/native-result-3.json` runs the same C17 probe with two
private SDL variants. Both have the same one-shot gate between the first empty
queue check and wake arming. A worker queues one real event there and acknowledges
it before the UI proceeds. Only the candidate adds the reviewed second queue check.

- Original: exit 1, wait returns false after 100,335,400 ns, and one event remains
  queued. The failure is the lost wake, not failed queue insertion.
- Candidate: exit 0, the event is delivered and no copy remains queued afterward.
  Its 73,300 ns sample is fixture evidence, not a general latency benchmark.

The initial probe setup failures are retained: the test compositor first lacked
its mandatory inherited input socket, then rejected a non-0700 capture directory.
Neither failure reached the probe or establishes an SDL defect. The first trace
wrapper build also retained a corrected misleading-indentation warning. The bus
monitor fixture initially expected a plaintext readiness message in JSON mode;
it now verifies an actual private `NameHasOwner` request before starting the app.

The permanent [probe](../../../tests/sdl_wakeup_probe.c),
[one-shot gate](../../../tests/sdl_wakeup_gate.h) and
[private runner](../../../tools/test_sdl_wakeup.py) retain this controlled boundary.
Gated libraries are test fixtures and must never become application profiles.

## Reviewed correction

[The patch](../../../patches/sdl3-3.4.20-event-wakeup.patch) rechecks the queue after
arming the native wake. A nonzero result clears the arm and returns the existing
queue result. There is no periodic polling, duplicate-event retry, daemon, new
dependency family, API or ABI change. Only the private Linux/Wayland profile is
being accepted; no Windows/macOS/hardware acceptance is inferred.

The SDL archive retains SHA-256
`f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`.
The reviewed patch SHA-256 is
`63d2149ed88ce43b0068f91eb52e7a535b2c619d78218347c406c3ea8b0e8c74`;
`SDL_events.c` preimage is
`ef6301f9416c6960c5f37fc9d8ed38a5624fbd246fa4da0ac02a8351bcb172d7`,
result is `ebc1dc6d10ee7e99d572f821651265324ef414d600b43b929a478329b4d52906`.
SDL remains 3.4.20 under its Zlib license; originals preserve third-party notices.

`build_ui.py` verifies the patch/preimage/result before writing the fresh source
and records the correction in its output metadata. Controls on the actual
upstream source reject changed patch, preimage or result before a source write.
The earlier optional-data-device patch remains independently checked.

An uninstrumented private candidate passed the actual original Terminal's
sixteen-focus/four-client workload in Release (9.36s) and ASan/UBSan (10.54s) at
`terminal-query-clean-sdl-beu6wzbl`. Its exact loaded library is checked through
the original process maps and SHA-256. No gated source is used by that candidate.

The first complete profile, `/home/aetheri/deps/ui-gles-release-03/prefix`,
rebuilt the same families and left 02 intact. Its initial Release run passed
79/80 groups: Settings still had a cached 02 font path, so its exact library
assertion correctly rejected the loaded 03 library. Updating that test argument
changed fifteen CTest commands; all fifteen then passed (154.15s), completing
Release coverage. The original results are retained at
`terminal-wakeup-profile-final-bywp6uvj` and `terminal-wakeup-profile-bound-7wv7azte`.

The latter ASan/UBSan run passed only 79/80 groups (543.83s). Files' real owned
Terminal/editor closure reported a **96-byte Wayland callback leak**, with
`Wayland_SendWakeupEvent` in the allocating stack. The original artifact is
`session-debug/files-ui-530agip7`. The old backend allocates `wl_display_sync`
callbacks without retaining pending ownership; closing before the callback
response leaves that allocation behind. Profile 03 is not accepted as a fix.

## Linux wake descriptor correction — continued 7 November

The separate [Linux backend patch](../../../patches/sdl3-3.4.20-wayland-wakeup-fd.patch)
replaces per-wakeup callbacks with one owned nonblocking, close-on-exec eventfd.
Both display read and backpressure waits include it. Waking cancels the prepared
Wayland read before returning to the queue; device deletion closes the descriptor.
Creation failure follows the device cleanup path. Other platforms keep their
existing backend; this work provides no acceptance for those backends.

The uninstrumented prototype compiled successfully; its library SHA-256 is
`cd78c04a6336f675acff9551c606ca119c9bdf89c62654b1ea39a49afeb18d66`.
A fresh full 04 profile has built successfully. No runtime acceptance follows
from this build alone.
The permanent controlled runner now applies the same backend patch to both gap
variants, preserving the queue-check counterexample, and adds repeated native
wakeup/descriptor-retirement plus injected eventfd-creation failure checks.
The permanent first execution at `sdl-wakeup-regression-20261107-01` proves the
controlled gap (original exit 1/queued 1, candidate exit 0/delivered 1) and sixteen
native lifetime cycles/512 events with unchanged descriptors. Its allocation
failure case initially expected the private backend error text. SDL's public
selector replaces that text with `wayland not available`; that fixture assertion
failed before the cleanup check, and its original output is retained.

The corrected probe independently witnesses entry into the injected eventfd
failure, requires SDL initialization failure and a nonempty public diagnostic,
then compares complete before/after FD snapshots after `SDL_Quit`. The full
permanent runner passes at `sdl-wakeup-regression-20261107-02`, including this
failure cleanup and both queue variants. This corrects a test expectation; no
dependency source or deadline changed between those two runs.

The ordinary uninstrumented 04 profile passes the full
80-group Release/ASan/UBSan application suite at
`terminal-wakeup-eventfd-final-3qienfs2`. It does not contain the one-shot gate.
The controller and both configured builds select 04, including the Settings font
argument. Release passes all 80 groups without skips (326.82s, including setup/
report overhead); ASan/UBSan passes all 80 without skips (544.24s). This includes
the previously leaking Files/editor closure, native Text/tabs/clipboard/IME and
the original desktop/app owner paths. The separate sixteen-focus/four-client
query workload also passes on the ordinary original apps in Release (8.43s)
and ASan/UBSan (10.74s), at `terminal-query-final-eventfd-xcvclvg7`. Their actual
maps contain the selected uninstrumented 04 SDL library. These are bounded
native test results, not a general latency benchmark or human/physical acceptance.

The diagnostic trace target is explicitly excluded from default builds. A late
diagnostic-only failure guard and runner docstring clarification were separately
synced; both trace targets compile with C17, and twelve ordinary executable
hashes remain byte-identical to those used by the passing suites. No app test
was replaced with a traced producer. The earlier failed query and leak artifacts
remain preserved; the specific demonstrated race is corrected, while attribution
of every earlier untraced native failure remains unproven.

The final read-only audit verifies 394 source inputs, 28 frozen SQL/XML contracts,
nineteen protected hashes with no SQLite sidecars, unchanged quarantined intent,
the exact historical CLOSED DRM record and unchanged NexaUI SDK artifacts. It
retains 727 exports and 216 verified native images. The two eight-shell idle
samples have zero frames and one CPU tick each over two seconds; read-only native
Terminal samples have zero frames/ticks. Terminal panes and Files captures were
visually inspected without claiming final design acceptance. No build/native/
private-registry actor, Aetheri service or seat0 session remains; the carrier is
Enforcing on tty1. Local audit/export inventories delimit this acceptance.

The private patch tool is Fedora `patch-2.8-4.fc44.x86_64` (GPL-3.0-or-later),
read as data from its pinned signed RPM, never installed or linked into Aetheri.
`build_ui.py` verifies its full Fedora signer fingerprint, payload, exact runtime
versions/artifacts, all source preimages and patch postimages. Dry-run/apply
commands and the tool license are retained. It is a build tool, not a new runtime
library. Pinned inputs and the exact signer are in `spec/ui-inputs.json`.
NexaUI remains 0.1.18/e71ee30; no SDK upgrade or new handoff is requested.

Local evidence/continuation is under `local/terminal-native-query-2026-11-06`.
Protected authority data, old unsettled runtime, frozen SQL/XML, historical closed
DRM records, normal app admission and production lifecycle remain separate.
