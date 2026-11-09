# Native audio management

`aetheri_audio_control` is Aetheri's C17 backend for listing outputs, observing
the default output and changing default, volume or mute through libwireplumber.
It calls the two maintained API modules directly. Settings will not need to
launch or parse `wpctl`; that tool remains an independent test oracle.

This is an opt-in development library. Normal session/device mediation and
Settings integration are still separate work. The local API is declared in
[`control.h`](../src/audio/control.h), not an installed D-Bus capability.
The separate [user-session fixture](audio-session.md) now joins this adapter to
real PAM/logind, user-managed services and dynamic device ACLs in a diskless VM.

## Original listener binding

`ae_audio_control_open_original` uses a borrowed, independently admitted server
pidfd. Before sending PipeWire protocol bytes it joins the connected socket's
kernel-provided `SO_PEERPIDFD` to that original process. Mandatory
`PIDFD_GET_INFO` observations bracket direct 64-bit pidfs inode checks, with
current UID/GID, cgroup and liveness checks. There is no numeric-PID lookup,
replacement adoption or fallback to the same-UID-only connector. Failure is
reported as `DISCONNECTED` with `AE_AUDIO_ERROR_ENDPOINT` and no outputs.

The helper borrows the original only during open; its temporary process FDs
are closed on every outcome. The caller retains the original lifetime fence.
An already connected socket is not redirected when its filesystem name changes.
The original `open` entry remains an explicitly unadmitted development profile.
The private user-session control and playback clients now receive the parent's
original pin in one fixed inherited FD slot and use the strict connector.

Socket credentials describe the connection/listener creator, not every later
holder or message processor. Current image/configuration/unit admission, exec
drift, listener/accepted-FD delegation, ongoing revocation and production MAC
remain separate obligations. A listener inherited from a different creator is
rejected; socket activation needs a separately admitted activation/creator chain.
This does not authenticate the user manager or make a pidfd a bearer capability.
See the [peer record](../brains/aetheri/journal/2026-11-09-audio-peer.md) for tests
and current integration evidence.

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
callback only; copy them to retain. Close clears the handle, disconnects user
signals, cancels observation and disables the owned cache subscriptions. It waits
for one final same-connection barrier, bounded to five seconds, then disconnects
on a later idle turn. Disconnection must not happen inside the protocol read
callback that delivered the barrier. Final object destruction also waits for an
idle turn after retained callbacks finish. Closing from a change callback is
supported. Continue running the context until the supplied data-destroy callback
runs, then pop/release it. This is local observer retirement, not device/app
drain, cancellation or rollback of a submitted change.

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
before streaming and copies slices in the callback. It also uses PipeWire's
`EARLY_PROCESS` flag to fill available buffers early on its existing main loop;
it does not move this callback to a real-time thread. The 1024-frame graph clock,
sample tolerances and exact order requirements remain unchanged. Passing these
tests is not a guarantee against scheduling stalls or production audio glitches.

## Native controls on virtual HDA

The separate diskless image now runs a finite ordinary-user client of this API
against the actual udev/ACP-discovered output. The supervisor independently
inspects the card, output, profile and kernel mixer. It passes the exact selected
output name to the client; native readback must match that name, bound ID and
serial. The client observes volume and mute separately, closes from its change
callback and exits before playback. No control CLI writes volume or mute in this
variant. The remaining CLI calls only inspect device/profile/link evidence.

The three images select unity, cubic 50% volume or mute. Full host WAV captures
must contain all 96,000 ordered stereo frames at the expected gain within the
existing two-LSB tolerance. Mute requires at least 96,000 captured frames with
every sample zero; it does not claim a detectable signal start.

QEMU 11.1's codec advertises dB steps but applies its amplifier linearly. The
first hardware-mixer half-volume attempt therefore fails the ideal-gain check,
despite correct native readback and an actual kernel mixer change. Preserve
that capture. The native test image has a separate software-mixer rule matching
only `alsa_card.pci-0000_00_01.0`. The live card property must confirm it; the
emulated mixer stays enabled. The root fixture initializes its independently
checked master controls once to 74/unmuted before starting the ordinary server.
Software mixing alone leaves this cold-boot codec at zero/muted, as a rejected
intermediate test demonstrates. The supervisor verifies the unchanged hardware
baseline again after native changes. This is a fixture-specific workaround,
not a recommendation for every physical card.
The unchanged nested device/manual images retain hardware-mixer regression.

Build each case into a fresh directory on the ordinary Linux builder:

```sh
python3 tools/build_audio_control_hda.py /absolute/fresh/output 0123456789abcdef0123456789abcdef \
  /absolute/audio-prefix /absolute/hda-prefix /absolute/policy-prefix \
  /absolute/control-prefix --case half
```

Use `--case unity` and `--case muted` for the other images. On the prepared
Windows host, run the existing controller with a fresh result directory:

```sh
python tools/test_audio_hda.py --image /absolute/output/image --output local/native-half
```

The image retains `device/image` and `device/manual/image` as separate controls.
All guests have no NIC, disk, shared directory or host-speaker output. Native
controls on this virtual output do not establish HP/Bluetooth audibility, normal
session access, hardware hotplug, suspend or Settings integration. Read the
[executed record](../brains/aetheri/journal/2026-11-08-audio-native-hda.md).

## Inputs and build

Endpoint connection/original-peer validation precedes backend construction.
A rejected endpoint publishes DISCONNECTED (ENDPOINT for the strict variant)
and can close without a WirePlumber core. This path is tested with missing SPA
plugins under ASan/UBSan as well as Release. It does not make a valid endpoint
safe with an incomplete or unadmitted backend: full library/configuration
admission remains required. See the [role record](../brains/aetheri/journal/2026-11-09-audio-mac.md).

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
