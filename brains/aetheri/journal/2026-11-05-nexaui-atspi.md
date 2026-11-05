# Independent native Settings accessibility

5 November 2026. The pinned NexaUI Settings consumer now passes an independent
C17/libatspi interaction test on the Aetheri compositor in the Fedora/glibc
carrier. This extends the [first probe](2026-11-05-nexaui-settings.md); it does
not replace the SDL app, finish the visual design or establish human assistive
or normal session acceptance.

## Boundary and implementation

[atspi-inputs.json](../../../spec/atspi-inputs.json) pins seven Fedora-signed
packages. [prepare_atspi.py](../../../tools/prepare_atspi.py) checks their bytes,
signatures, matching existing GLib/D-Bus runtimes and the original checked D-Bus
header SDK. It reads selected headers, six native executables/libraries and
notices as data into a fresh private prefix, without package installation,
scriptlets, system configuration or global library replacement. Its 344-file
inventory, original notices, compile command and native client hash are retained.
The X libraries belong to the independent AT-SPI peer/registry, not the NexaUI
application. No X server or new normal desktop service is introduced.

[Our client](../../../tests/nexaui_settings_atspi.c) links libatspi and GLib,
with no NexaUI or Aetheri model linkage. It queries native names, radio roles,
enabled/checked state and semantic size, then issues native actions. The parent
retains the original application process and independently observes its model
logs, compositor output and orderly exit. PID matching is private correlation,
never production authentication, process admission or a grant.

[The harness](../../../tools/test_nexaui_atspi.py) creates an authenticated
AF_UNIX D-Bus daemon under its 0700 runtime directory, a real AT-SPI registry,
the existing compositor test server and the unchanged pinned consumer. No
activation directories or system bus are used. Original child handles own
cleanup; registry/bus termination is test retirement, not app/lease drain.
The opt-in CMake cache path `AETHERI_ATSPI_TEST_PREFIX` registers
`settings.nexaui_atspi` only with the existing selected NexaUI probe. Defaults
start no extra service and leave normal Settings selection unchanged.

## Executed verification

Evidence: `local/nexaui-atspi-2026-11-05`. Prepared final fixture:
`/home/aetheri/deps/atspi-settings-03`. The full selected check is
`/home/aetheri/build/nexaui-atspi-check-iexflmdn`; the final strict diagnostic
check is `/home/aetheri/build/nexaui-atspi-final-qe823mx_`.

The same client first fails as required on a real visible `NO_AT_BRIDGE=1`
window, reporting `target app was not registered`; the model remains default.
Its normal connection finds the actual second window through the private
registry and changes high contrast, German and 200% text. The app's diagnostic
model independently confirms each result. The client reads checked native radio
states after those changes. A native maximize and 320x320 output resize retain
them; it reads the new semantic window size, changes text to 150% and selects
the actual HEADLESS-1 observation. After restoring 2560x1440 it verifies state
again and closes through native AT-SPI. Selecting a display changes only the
in-memory model selection; it does not apply an output setting.

An initial harness assumed the second window would reuse output zero. Our
compositor correctly allocated output one, and the pixel assertion failed on
the unused output. Original `/home/aetheri/build/nexaui-atspi-9i04ayd7` logs are
retained. The corrected harness follows actual MAP placement and verifies its
2560x1440 selected output; no compositor/SDK change, deadline extension or
weakened accessibility action followed from this fixture error.

Release and the ASan/UBSan SDK/consumer each pass all four selected groups:
`settings.model`, `settings.native`, `settings.nexaui`, `settings.nexaui_atspi`
(13.86s / 27.47s total). The final harness explicitly halts on sanitizer errors
and rejects sanitizer/GObject diagnostics; its targeted native test passes
again in **7.71s Release / 9.58s Debug**. The independent libatspi peer and the
packaged daemon/registry are regular native builds; they are not claimed to be
instrumented. The client compiles with C17 and strict warnings-as-errors.

Each final native case retains three captured states/nine images; all eighteen
positive images and both independent loaded-library maps are exported and
checked. App, client, bus and registry logs have no sanitizer/runtime/GObject
critical diagnostics. The client maps the pinned libatspi and no NexaUI;
the app maps its original NexaUI and no libatspi/X11/SDL/Vulkan test dependency.
The selected SDK/app binary hashes remain unchanged from the prior milestone.

Final audit binds **347** host/snapshot/guest sources, **28** unchanged frozen
SQL/XML and **19** unchanged protected old-runtime hashes, without sidecars.
The enforcing carrier is idle on tty1 without candidate/test daemon processes,
known native app/Aetheri unit or seat0 session. Bounded export verifies **58**
files. The old running/recover-handoff remains unresolved and untouched.

## Next

The development supervisor already owns app start/reuse/retirement separately
from views. Its current Settings selection is fixed to SDL. The NexaUI candidate
uses the matching `org.aetheri.Settings` app ID and supports the owner's font/
cache/diagnostic argument shape, but emits FRAME/MODEL rather than SDL's
READY/LAYOUT. An explicit, bound development profile must preserve the original
owner/channel/one-live-app and shutdown rules; an app's spawn acknowledgement
must not become a readiness claim. Recheck the selected candidate at start and
in the unprivileged supervisor, retain historical profile identity and test
launch/reuse/close/reopen through the actual desktop/DRM path before adoption.
Do not silently replace the working SDL view or inject a second renderer.

Human screenreader tasks, focus-event/announcement quality, complete accessible
text/roles, real session bus provisioning, final design and physical/scaling/
performance acceptance remain. Other apps, normal grants/recovery/removal,
device backends, OS image/installer/hardware and the persistent experiment desktop
remain required. The full goal stays active. The existing NexaUI catalogue now
records this scoped A02/A10 evidence without changing IDs or requesting a new
chat handoff. See [the probe guide](../../../spec/nexaui.md).
