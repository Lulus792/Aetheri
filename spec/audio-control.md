# Native audio management

`aetheri_audio_control` is Aetheri's C17 backend for listing outputs, observing
the default output and changing default, volume or mute through libwireplumber.
It calls the two maintained API modules directly. Settings will not need to
launch or parse `wpctl`; that tool remains an independent test oracle.

This is an opt-in development library. Normal session/device mediation and
Settings integration are still separate work. The local API is declared in
[`control.h`](../src/audio/control.h), not an installed D-Bus capability.

## Event loop and lifetime

The caller creates a GLib main context, pushes it as the creating thread's
default, and keeps it pushed while iterating that context. The backend creates
no helper process or polling thread. Upstream libraries retain their own
implementation and thread behavior; this is not a single-thread guarantee for
the whole process.

WirePlumber's nested asynchronous operations capture the thread-default context.
Providing only `wp_core_new(context, ...)` is insufficient. Open rejects a
context that is not installed as the calling thread's default. Calls from other
threads are rejected. Change notifications are coalesced through one idle source;
there is no periodic device scan. Startup and command observation have five-second
deadlines.

The caller owns the returned handle. Callback snapshots are borrowed for the
callback only; copy them to retain. Close clears the handle, disconnects signals,
destroys owned sources and cancels asynchronous observation. Pending callbacks
retain the backend until they finish. Continue running the context until the
supplied data-destroy callback runs, then pop/release it. Closing cannot undo a
submitted backend change or prove its cancellation.

## Connection and output references

Each instance opens one explicit absolute AF_UNIX endpoint and gives that same
connected FD to WirePlumber. The finite playback diagnostic uses the shared
connection helper too. Neither lets `PIPEWIRE_REMOTE` replace an explicit target.
Missing sockets fail; there is no fallback or reconnect. FDs are nonblocking and
close-on-exec. Same-UID peer credentials are a sanity check, not service/process
admission. The caller still admits the server, path, configuration and namespace.

The current implementation requires the reviewed libwireplumber 0.5.19, GLib
2.88.0 and remote PipeWire 1.6.11. A fresh random connection value plus the
observed object serial and ID scope each output reference. These are live backend
observations, not durable device/component identity, permissions or bearer tokens.

Snapshots own up to 64 copied outputs, with bounded UTF-8 names/descriptions.
They distinguish connecting, ready, failed and disconnected states. Ready means
the output collection is installed; a default selection or mixer value can still
arrive later. Invalid metadata or count overflow fails the collection instead
of returning a misleading partial list. Error codes identify the failed boundary.

Every mutation requires the latest snapshot revision and an exact output
reference. A pending refresh or stale revision/ref is rejected before submission.
Only one command is pending at a time. Default names containing characters that
the pinned module does not safely interpolate into JSON are not submitted; the
same applies to duplicate output names. Displaying such a name remains possible.

## Changes and their meaning

Volume uses WirePlumber's cubic scale and is limited to `[0,1]` for writes.
It sets all channels equally and preserves mute. Mute changes preserve volume.
Observed readback may report an existing boosted volume; that does not authorize
another boost. Per-channel balance and richer profile/route controls remain work.

`SUBMITTED` only means the maintained API accepted the local request. The backend
waits for its server barrier and a matching effective-state observation before
reporting `OBSERVED`. This is not persistence, device drain, physical audibility
or proof that no other client wrote the same value. Missing target, timeout or
connection failure leaves the affected command `UNKNOWN`; it is never replayed.
Timeout fails the instance. An explicit new observer is not recovery of its old
command history. Normal durable authority/permission integration remains work.

Default readback describes effective selection for new streams. The finite
diagnostic deliberately sets DONT_RECONNECT. Switching defaults while that
inactive stream already exists leaves extra links in the observed private graph.
The native selection tests therefore run before creating it; the new stream must
still have exactly its two expected links and complete samples. Moving existing
streams is not accepted by this milestone.

The final native suite covers unity, 50% and mute in Release and ASan/UBSan,
including six actual paused-server timeouts and original-handle retirement
after injected cleanup failures. It retains the earlier rejected capture with
an inserted silent quantum. The finite producer now prepares its bounded signal
before streaming and copies slices in the callback. The 1024-frame graph clock,
sample tolerances and exact order requirements remain unchanged. Passing these
tests is not a guarantee against scheduling stalls or production audio glitches.

## Inputs and build

The [control SDK inventory](audio-control-inputs.json) adds exact signed
WirePlumber-devel 0.5.19 and glib2-devel 2.88.0 headers: 351 files, 2,825,653 bytes.
It also copies one existing pinned GLib runtime object, 1,469,656 bytes, for
explicit linking/loading. This is a private build prefix, not a new runtime
family or a standalone redistributable SDK. It requires the reviewed
[policy prefix](audio-policy.md), including its WirePlumber/GLib notices.
Package licensing remains MIT and LGPL-2.1-or-later respectively; Aetheri's MIT
license does not replace either. Header comments/notices remain intact.

Preparation verifies signatures and payloads against the same sealed RPM objects
used for bounded, named regular-file extraction. It executes no installer or
package scripts. Runtime copy hashes are existing-object evidence, not fresh RPM
signature checks. Prefix checks reject changed, missing or unexplained objects.

```sh
python3 tools/prepare_audio_control.py prepare /absolute/fresh/control-prefix \
  --cache /absolute/audio-rpm-cache
cmake -S . -B /absolute/fresh/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAETHERI_AUDIO=ON -DAETHERI_AUDIO_CONTROL=ON \
  -DAETHERI_AUDIO_PREFIX=/absolute/audio-prefix \
  -DAETHERI_AUDIO_POLICY_PREFIX=/absolute/policy-prefix \
  -DAETHERI_AUDIO_CONTROL_PREFIX=/absolute/control-prefix
cmake --build /absolute/fresh/build --target aetheri-audio-check aetheri-audio-control-test
python3 tools/test_audio_control.py /absolute/fresh/build/aetheri-audio-control-test \
  /absolute/fresh/build/aetheri-audio-check /absolute/audio-prefix \
  /absolute/policy-prefix /absolute/control-prefix /absolute/fresh/test-parent
```

Use the existing dedicated ordinary Linux builder. The test parent must exist;
each run makes new private graphs, sockets and state. The driver protocol is
test-only. Normal builds keep both audio options off. Read the
[executed record](../brains/aetheri/journal/2026-11-08-audio-control.md) for current
results, rejected inputs and the hardware-regression boundary.

References: [WirePlumber 0.5.19 control source](https://github.com/PipeWire/wireplumber/blob/0.5.19/src/tools/wpctl.c),
[mixer API](https://github.com/PipeWire/wireplumber/blob/0.5.19/modules/module-mixer-api.c),
[default API](https://github.com/PipeWire/wireplumber/blob/0.5.19/modules/module-default-nodes-api.c),
[core implementation](https://github.com/PipeWire/wireplumber/blob/0.5.19/lib/wp/core.c)
and [GLib thread-default context](https://docs.gtk.org/glib/method.MainContext.push_thread_default.html).
