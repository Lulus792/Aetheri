# Sound GUI lab runtime

The explicit audio-linked Settings client has a separately reviewed runtime
projection in [settings-audio-lab-inputs.json](settings-audio-lab-inputs.json).
It contains 34 shared objects, two test programs, one font and 43 notice/source
files: 80 files, 30 components and 25,241,858 bytes. It supplies files for the
next [embedded host integration](audio-session.md#embedded-native-host-event-loop).
It does not install a normal app, start an audio service or admit a caller.

## Provenance and attribution

The program/library/font bytes come from the independently verified native v4
export, SHA-256 `e3b196bdc66119421ea7b007561f44c24f75890d4bc86f362d266878a8e13716`.
The native source inputs match revision `4c12de2`. Existing audio SDK profiles,
the reviewed NexaUI 0.1.23 source pin and wlroots build remain unchanged. Each
component has an origin and nonempty notice list; every notice is pinned just
like code. Notices belong to their original components, retaining their terms.

Installed RPM metadata and exact library hashes identify system providers.
Private audio objects match the existing SDK inventories. Missing installed
documentation for pixman, libinput, mtdev and libevdev is read from sealed RPM
snapshots after checking the reviewed Fedora signer, payload digest and matching
library bytes. No RPM is installed and no package script is executed. Other
available notices are read from the original installed/source files. NexaUI's
Unicode data notice accompanies its MIT license.

libdrm has copyright/license statements in individual source files rather than
one aggregate COPYING file. Retain its exact upstream source archive. The PCRE2
and zlib-ng notice sets also include their exact source archives, preserving
embedded notices; PCRE2's SLJIT notice is available separately. These archives
are data and are not built or executed. Source URLs and hashes are in the
manifest. This is an internal test projection, not a complete distribution
license/compliance audit or a claim of publisher authentication from hashes.

## Prepare and inspect

The caller must independently select the reviewed input snapshot. Its relative
names are the manifest's `source` fields; checksum equality cannot select or
admit an arbitrary candidate as trusted. The public tool performs no download,
package installation, ELF execution, policy change or service start.

```text
python tools/prepare_settings_audio_lab.py prepare spec/settings-audio-lab-inputs.json FRESH_TREE --inputs REVIEWED_INPUTS
python tools/prepare_settings_audio_lab.py check spec/settings-audio-lab-inputs.json FRESH_TREE
python tools/test_settings_audio_runtime.py
```

Prepare requires a fresh nonoverlapping output directory. It rejects aliased
parents and preserves failed partial copies; it never adopts or overwrites an
existing output. Check requires the exact files and ancestor directories,
content/size, regular single-link objects and, on Linux, modes. Attribution
coverage is validated before output creation, including equality between each
file's and component's declared origin. These checks require trusted parent
directories; they do not establish namespace race exclusion, durability, volume
authentication or production file ownership/admission.

The lab uses flat SONAME copies in `usr/lib64` and explicit library selection.
The actual lab system interpreter is checked against the pinned loader bytes.
Its normal `/lib64` to `/usr/lib64` spelling is accepted only for that exact
interpreter; other listed dependencies must resolve to the projected library
directory. A successful loader listing checks compatibility, not actual app,
source/session admission or later path stability.

## Executed boundary and next work

All nine projection tests pass on native enforcing Linux as UID1000. Windows
passes eight and explicitly skips the Linux mode/alias case. A private removed-
notice-guard variant fails the missing-attribution assertion; restored code
passes. The complete real 80-file projection passes in the separate audio lab.
Both actual ELF loader listings succeed. The actual GUI client exits 2 without
output when original input descriptors are absent. No audio test unit is started.
See the [execution record](../brains/aetheri/journal/2026-11-09-audio-runtime.md).

The subsequent [connected test record](../brains/aetheri/journal/2026-11-09-settings-audio-live.md)
tracks actual local-session/GUI joining and volume readback, plus failed attempts
and their separate retirement. Its updated native program fixes passive slider
rounding; third-party versions/notices stay unchanged. Do not infer the complete
joined workflow from the earlier projection/loader checks.

`tools/test_settings_audio_live.py` is an explicit root-only disposable-lab
driver. It independently checks its SDKs, native helpers and complete GUI
projection before provisioning. It connects original source handles and selected
image FDs through private PID1 transport, then exercises real headless Wayland
input and independent graph properties. A failed/unknown step retains the
controller and original handles for inspection. Do not rerun that attempt.
`tools/test_settings_audio_live_controls.py` exercises the driver parsers and
preflight without provisioning; it requires Linux imports. Normal
service/broker/device policy, holder/restart coordination, physical HP/Bluetooth
and release integration remain work.

There is also an explicit release review item: the existing PCRE2 provider is
10.47-1.fc44.1. The installed changelog ends at the January 2026 mass rebuild;
[upstream lists later advisories affecting 10.47](https://github.com/PCRE2Project/pcre2/security).
Exact Fedora backports/reachability have not been established. Review and update
the affected runtime/image pins before release; this projection makes no security
acceptance claim. Do not change the preserved original carrier in place to do so.
