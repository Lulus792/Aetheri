# NexaUI Settings integration probe

## Aetheri-owned Settings presentation

6 November 2026. `src/settings/nexaui_view.c` now provides an opt-in Aetheri
presentation of the unchanged Settings model. Its three sections expose observed
displays, local appearance/text size and local language. The sidebar, muted
surfaces, content hierarchy and grouped controls follow the accepted external
studio at `F:/Aetheri-UI-Konzepte/02-designwerkstatt/index.html`. Only implemented
sections appear; illustrative device/update controls are not copied as operations.
This is the opaque material fallback, not the complete glass design or a default
toolkit replacement. Light and high contrast are engineering variants; final
visual and human assistive acceptance remain open.

The original app model owns all choices and display IDs. A replaced view retains
that model; closing the process does not persist its preferences. Failed display
refresh preserves observations and disables selection. The view owns only layouts,
font/context resources, scroll/focus presentation and consumed-once refresh/window
requests. It never writes system settings or establishes permission. Labels are
cached and overlong runs wrap at grapheme boundaries while semantic names retain
their original bytes. At narrow widths navigation uses labeled semantic icon
buttons; content scrolls and follows keyboard focus. Page Up/Down and Home/End
also expose read-only help without changing a choice. No animation or backdrop
effect is required for an operation.

`nexaui_main.c` adapts the pinned upstream example's native event/observation
scaffold, with its MIT notice retained in `src/settings/NEXAUI-NOTICE.txt`.
It uses the same Wayland app ID, event-driven wait and signal pipe. Close and
observed maximize/restore are separate from supported requests; unavailable
minimize stays disabled. Diagnostics distinguish a changed model from `SETTLED`
frames so a new choice is not mistaken for already painted pixels. These are
private developer diagnostics, not installed authority receipts.

Build against the existing reviewed private prefix; no SDK upgrade is needed:

```sh
cmake -S . -B /path/to/aetheri-build \
  -DAETHERI_NEXAUI_PREFIX=/home/aetheri/deps/nexaui-settings-release-02/prefix
cmake --build /path/to/aetheri-build --target aetheri-settings-nexaui test_settings_view
ctest --test-dir /path/to/aetheri-build -R '^settings.view' --output-on-failure
```

This extends an already configured Settings/compositor development build with its
reviewed font. CMake requires the exact installed 0.1.18 package. Use the separate
Debug prefix for sanitizer builds. The new executable is not yet selected by the
development owner: `--settings-ui nexaui` still binds the earlier upstream consumer.
Keep that existing profile and SDL intact until a distinct owned-view binding and
DRM lifetime test have been implemented. Normal session accessibility, full input/
window chrome, real settings services and physical/performance acceptance remain work.

The checks are `settings.view` and `settings.view_hidpi` (layout/model/keyboard/contrast
at separate raster densities),
`settings.view_native` (native input/presentation/window requests) and optional
`settings.view_atspi` (independent C17/libatspi peer, private registry and visible
disabled-bridge negative). The latter reuses verified private headers/runtime,
compiles a separate test peer and changes no installed accessibility service.
See the [dated project record](../brains/aetheri/journal/2026-11-06-settings-view.md)
for executed results and limitations.

## Earlier pinned consumer

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

The original `settings.nexaui` test deliberately has an unavailable accessibility bus. It proves
compiled/late-loaded transport and usable ordinary interaction in that condition,
**not** successful native AT-SPI publication, a screenreader or accessible normal
session. The separate native AT-SPI test below now supplies actual registry/client
evidence. Before replacing Settings, exercise actual development-owner launch/
reuse/close/reopen on the DRM desktop. Compare
the full existing Settings behavior, finish the accepted layout/design, and
measure relevant resource/response budgets. Persistent settings and real device
services remain Aetheri work; the SDK view cannot create those backends.

## Independent native AT-SPI test

`settings.nexaui_atspi` uses a separate C17/libatspi client with no NexaUI or
Aetheri model linkage. A private real D-Bus daemon and AT-SPI registry provide
transport; the normal system bus, desktop configuration and installed services
are unchanged. The application remains the original pinned consumer.

[atspi-inputs.json](atspi-inputs.json) pins seven signed Fedora RPMs for this
optional test: AT-SPI 2.60.7 runtime/headers, D-Bus 1.16.2 daemon, matching
GLib 2.88.0 headers and three small X client libraries. The test peer/registry
need those X libraries even on Wayland; the NexaUI application does not load
libatspi, GLib or X11 through this test. This does not introduce an X server.
The existing system GLib/D-Bus runtimes stay selected. LGPL and the packages'
other original notices remain distinct from Aetheri's MIT code.

The [fixture preparer](../tools/prepare_atspi.py) verifies signatures, exact
package bytes, matching existing runtimes and the original D-Bus header SDK.
It extracts only selected headers, six executables/libraries and license notices
into a fresh private directory, records 344 files and compiles our independent
[client](../tests/nexaui_settings_atspi.c). It performs no RPM installation,
scriptlet, download or global library replacement. Generated records and exact
relative library links remain with the fixture; interrupted attempts are retained.

```sh
python3 tools/prepare_atspi.py /path/to/pinned-rpm-cache /path/to/new-atspi-fixture \
  /path/to/nexaui-private-build/dbus-sdk
cmake -S . -B /path/to/aetheri-build \
  -DAETHERI_ATSPI_TEST_PREFIX=/path/to/new-atspi-fixture
ctest --test-dir /path/to/aetheri-build -R '^settings.nexaui_atspi$' --output-on-failure
```

The existing NexaUI probe path must also be selected. Both cache options default
to empty: ordinary builds/tests start no registry or bus. The test's AF_UNIX bus
lives under its own 0700 runtime directory, authenticates the test UID and has
no service-activation directories. The parent keeps each original child handle,
waits for orderly positive exits and closes the test-owned registry/bus afterward.
Failure cleanup concerns only those test children, never production drain/leases.

The identical client first rejects a visible window with `NO_AT_BRIDGE=1` and
confirms that no settings action occurred. With the bridge enabled, it finds
the parent's actual held app through the private registry, checks native radio
roles/enabled/checked states, requests high contrast, German and 200% text and
observes applied model results. That PID comparison is test correlation only,
not an authority/admission mechanism.

The second window is on output one after the first retires. The harness follows
that actual placement, maximizes there, resizes it to 320x320 and lets the
independent client verify the native semantic size and change text to 150%.
It also selects the observed HEADLESS-1 entry, restores the actual 2560x1440
window, rechecks settings and closes through native AT-SPI. The parent's own
model logs, captured compositor pixels and original process exit independently
confirm effects. Selecting an observed display does not apply system configuration.

Release and the ASan/UBSan consumer pass; each retains three states/nine images.
Sanitizers halt on errors and log checks reject sanitizer/GObject diagnostics.
The independent client and packaged registry themselves are regular native
builds. See the [executed record](../brains/aetheri/journal/2026-11-05-nexaui-atspi.md)
for exact test boundaries and original fixture failure. This closes a scoped
private native-transport/action gate. Human screenreader tasks, focus-event/
announcement quality, complete accessible roles/text, physical input/scaling,
normal session bus provisioning and the accepted visual design remain work.

## Explicit development desktop selection

The existing [development owner](development-session.md) now accepts
`start --desktop --settings-ui nexaui`, optionally with `--compose`. It selects
only the reviewed per-profile build paths in the input pin and records the exact
candidate/model/metadata binding. Root checks it before dispatch, the original
ordinary-user supervisor checks that binding again, and each new Settings spawn
must still match. Reuse retains the original live process. A failed recheck
returns the existing launch-error reply without a new process or toolkit fallback;
closing/reopening uses a fresh app under the same retained selection.

`desktop.owner_nexaui` exercises actual search/dock input, one live instance,
local model changes, process/pipe retirement and fresh reopen alongside the
original Terminal/Files. Its drift case changes only a copied test pin, proving
rejection without modifying the shared SDK or repository. The default SDL owner
and its dirty-document/job/pipe shutdown tests remain separate regressions.
The controlled owner fixture is unprivileged/headless; actual PAM/DRM exercise
uses the explicit root-selected development profile. Neither supplies normal
authority admission, persistent settings or a complete UI migration.
