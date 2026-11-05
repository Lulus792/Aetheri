# NexaUI Settings integration probe

5 November 2026. An opt-in external consumer now runs the original Settings model
through NexaUI on Aetheri's private compositor. The normal Settings executable
and development-session owner still use SDL. This probe is a step toward a
per-view replacement, not a completed migration or the accepted visual design.

## Selected boundary

[nexaui-inputs.json](nexaui-inputs.json) selects NexaUI 0.1.18 at immutable commit
`e71ee304f1e6c23fa562d9e4b11f7e8e12152079`. This candidate already has an external
C17 consumer for our actual `src/settings/model.c/.h`, with window-output
observations, software presentation, keyboard focus and narrow-layout tests.
The builder copies those two original model files into a retained snapshot;
neither SDK types nor a second settings model enter Aetheri's model contract.
The consumer view and host remain upstream MIT code in the private SDK tree.
No NexaUI worktree or SDK source is changed by this integration.

The shared SDK builds only Wayland and its own software renderer. X11, Vulkan,
SDL, FreeType and HarfBuzz are not dependencies of this candidate profile.
This is a dependency/functional probe, not a decision that software rendering
will meet the final compositor/app performance budget. Exact mapped libraries
are retained; no physical GPU or mixed-DPI acceptance follows.

Native AT-SPI is compiled. The builder reads eighteen headers from the pinned,
Fedora-signed `dbus-devel-1:1.16.2-1.fc44.x86_64` RPM as data and checks the existing
matching `dbus-libs` runtime. It installs no package, executes no scriptlet and
replaces no library. A private pkg-config file supplies only header paths.
NexaUI late-loads the existing system `libdbus-1.so.3`. A missing SDK/transport
is a build failure rather than silent loss of accessibility. The source pin,
Fedora signer, archive/package hashes and license declarations are explicit;
they are not a comprehensive vulnerability audit or a signed NexaUI release.
The SDK MIT notice and its installed Unicode notice remain in the private prefix;
retain the Fedora runtime's original licenses when distributing it.

## Build and run

Generate the archive from the exact committed object, not a dirty checkout.
On Windows, write Git's binary stdout to a file using Python or another binary
stream; do not use a text pipeline. Command-local `core.autocrlf=false` is required
to retain the pinned raw source bytes without changing repository configuration:

```text
git -c core.autocrlf=false archive --format=tar e71ee304f1e6c23fa562d9e4b11f7e8e12152079
```

Copy that archive and the exact pinned RPM to the disposable carrier. In a fresh
unprivileged Linux directory:

```sh
python3 tools/build_nexaui.py /path/to/nexaui-e71ee304f1e6c23fa562d9e4b11f7e8e12152079.tar \
  /path/to/new-private-build --dbus-rpm /path/to/dbus-devel-1.16.2-1.fc44.x86_64.rpm
```

Use `--sanitizers` for a separate Debug SDK and consumer. The builder performs no
download or global installation; partial directories remain for inspection.
It checks source size/hash/inventory, raw license bytes, RPM signature/runtime,
compiled transport and model/artifact hashes. `build.json`, compile commands,
licenses and logs remain beside each build. The source archive is approximately
38 MB and stays outside Git; this is build input, not an app installation size.

An existing Aetheri native Settings/compositor test build can opt in:

```sh
cmake -S . -B /path/to/aetheri-build \
  -DAETHERI_NEXAUI_SETTINGS_PROBE=/path/to/new-private-build
ctest --test-dir /path/to/aetheri-build -R '^settings.nexaui$' --output-on-failure
```

`AETHERI_SETTINGS` and `AETHERI_COMPOSITOR` must already be enabled and their
native test server built. The existing `AETHERI_SETTINGS_TEST_FONT` selects the
reviewed Liberation Sans bytes. The default probe path is empty, so normal
builds do not acquire a NexaUI dependency or replace the SDL application.

## Executed evidence

The test compiles/runs the original consumer's scene checks, including model
retention across view replacement. Native input goes through the existing
test-only inherited compositor socket and real Wayland delivery, not a NexaUI
event-injection API. It checks three real headless output observations, pointer
selection, keyboard traversal, German/high contrast/200% text, maximize, resize
to 320 by 320, restore, focus retention, presented pixels and orderly close.
Missing-display/font failures do not fall back to X11. A bounded idle observation
records frames and CPU ticks. Original handles own test cleanup only; a probe's
exit is never production app/lease/descendant settlement.

An executed negative exposed a compositor defect: without a private desktop
owner, output mode changes updated the observation but did not reconfigure an
existing maximized app. The standalone compositor now updates the selected
windows' bounds/dimensions and current pointer hit. With a desktop owner, the
existing dock/work-area arrangement path remains responsible.

Both Release and ASan/UBSan pass the native probe, with four captured states
and twelve images per profile. Five existing Settings/compositor groups also
pass per profile. Exact sources, original failures, binaries and current-state
audit are in the [dated record](../brains/aetheri/journal/2026-11-05-nexaui-settings.md).

The private test deliberately has an unavailable accessibility bus. It proves
compiled/late-loaded transport and usable ordinary interaction in that condition,
**not** successful native AT-SPI publication, a screenreader or accessible normal
session. Before replacing Settings, exercise actual development-owner launch/
reuse/close/reopen on the DRM desktop and a separate real AT-SPI client. Compare
the full existing Settings behavior, finish the accepted layout/design, and
measure relevant resource/response budgets. Persistent settings and real device
services remain Aetheri work; the SDK view cannot create those backends.
