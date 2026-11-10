# DRM development session

The current Release build now has a separate
[protected runtime projection](current-desktop-runtime.md). Two explicit current
compositor/input runs pass actual KMS display, PS/2/Wayland delivery, original
retirement and card reacquisition with checked code mappings. The older carrier
dependency/build path below remains historical and available; current runtime
selection is explicit and does not promote normal source/build/service defaults.
The real desktop/app owner now has an explicit protected-runtime binding and an
actual one-display KMS app/save/shutdown workflow; read the linked runtime contract
and [execution record](../brains/aetheri/journal/2026-11-10-current-desktop-owner.md).
Normal service/device/session admission remains open.

## Native desktop owner and original-session observation

10 November 2026. The existing C17 owner in `src/boot/desktop.c` now uses
`src/linux/session.h`, a read-only self-session observer. An explicit native
owner passes the full protected-runtime KMS workflow and active-VT-loss test;
see the [execution record](../brains/aetheri/journal/2026-11-10-native-session-owner.md).
This does not promote the fixture to a normal login or build a new USB image.

The observer opens only on the main thread of an ordinary equal-real/effective-UID
process. It retains its self pidfd, mandatory kernel PID/credential/cgroup-ID
observations and exact original session/scope/cgroup strings. Opening requires
a local active Wayland `class=user` session on the independently expected seat
and UID; the scope must be a session scope. Trusted host proc/cgroup/logind views
and policy excluding context manipulation remain prerequisites. This self pin
must never substitute for a bus-authenticated caller's ProcessFD.

| Function | Contract |
| --- | --- |
| `ae_session_open` | Subscribe before observing; publish only a fully checked handle, leaving the initially null output unchanged on error |
| `ae_session_check` | Reobserve the original context even without a monitor event; retain the first observed loss permanently |
| `ae_session_poll` | Borrow the monitor FD/events and absolute monotonic microsecond deadline; a hint is not admission |
| `ae_session_close` | Release local observations on the creating process/thread; never stop a process or settle a lease |

Thread/fork misuse rejects without changing output or the live pointer. Context
drift, an inactive/foreign session and failed observation remain distinguishable.
Returning to the original VT cannot rearm a lost handle. A monitor query failure
also latches loss. Repeated observations bracket credentials/cgroup identity;
they do not atomically lock future context or device access. Closing is not
revocation, process drain or durable recovery.

The desktop owner requires UID/GID1000, observed zero effective/permitted/
inheritable/ambient capabilities and NoNewPrivileges. It checks its original
session before compositor/desktop effects, readiness and fresh app starts, and
on event-loop wakes. The existing poll loop combines the monitor hint/deadline
with signals, launch messages and child output. No observer thread, daemon or
new periodic scan is introduced. Session loss fences launches and enters the
existing Files-first ordered shutdown; document-close decisions remain intact.
Trusted lock, suspend/resume and normal account/app admission are still work.

Each start creates a fresh `aetheri-XXXXXX` directory beneath the selected
`XDG_RUNTIME_DIR`, with its own cache; an existing directory is never recovered
by name. Build-selected path constants retain image defaults and permit an
explicit private lab ELF to bind a protected runtime. They are not package trust
or caller authority. Files receives both selected document Terminal font paths.

Build the separate owner with `AETHERI_LINUX_IPC=ON` and
`AETHERI_DESKTOP_OWNER=ON`; both remain opt-in. It links the small
`aetheri_linux_session` library and libsystemd. `linux.session` uses controlled
logind metadata/monitor inputs with real self pidfd, kernel observations,
eventfd/poll and thread/fork behavior. UID0 returns the explicit skip code 77.
Native ordinary-user Release and ASan/UBSan/leak checks pass. Real logind/KMS
evidence is separate from those controlled metadata tests.

`tools/test_current_desktop_session.py` can explicitly select
`aetheri-native-desktop-owner-v1`. Root retains the stopped original PAM/logind
wrapper's pidfd/scope before its same-process exec into the independently hashed
native ELF. There is no resident Python desktop owner after exec. A distinct
`active-seat-loss` case changes the active VT and expects the native failure/loss
record, followed by original process/session/group end. The test controller is
Root-only disposable-carrier tooling, not product authentication or recovery.
Its user fixture is populated while parents are protected and handed over from
leaves upward; document verification reads with the ordinary account's rights.

## Entry from an existing local login

The opt-in `aetheri-session` target reuses the same owner with
`AETHERI_DESKTOP_SESSION_ENTRY=1`. It is a post-login entry, intended for a login
manager to execute after establishing the local active seat0 Wayland user session.
It does not authenticate passwords, create accounts, change UID/GID or open PAM
sessions. SSH, a plain TTY login and caller-provided session environment variables
cannot replace the observer's required actual session. Login-manager selection,
authentication policy and a trusted greeter remain separate work.

The entry rejects root and unequal real/effective/saved user or group IDs. Bounded
NSS account lookup must return that exact UID and primary GID. It sets HOME, USER
and LOGNAME from that account and derives `/run/user/UID` from the actual UID,
ignoring the caller's runtime hint. `/run` and `/run/user` must be real Root-owned
directories without group/other write access; the runtime must be a real
mode0700 directory owned by that user. The trusted systemd runtime hierarchy and
NSS remain prerequisites, not volume authentication or hostile-writer exclusion.
There is no creation/adoption fallback for an absent runtime.

The default Files development root is `NSS_HOME/Documents`; bounded build-time
paths may still bind a private test ELF to separate documents. No document
directory is created and no document grant or app sandbox is implied. Account
and user-directory provisioning remain distribution responsibilities. The entry
sets NoNewPrivileges itself, then requires the same observed empty capability
sets and original-session checks as the fixed owner.

A directly executed session has no systemd notification socket. In that case,
first-frame readiness is recorded locally without requiring a notify service.
If a channel is supplied, its notification must still succeed; an empty or
failed channel is not silently ignored. The original fixed development-image
owner continues to require its notification acknowledgement and fixed account.

With `AETHERI_DESKTOP_OWNER=ON`, CMake builds both entry profiles and copies
`data/wayland-sessions/aetheri.desktop` into the build directory. Its literal
Exec/TryExec path is `/usr/libexec/aetheri/aetheri-session`. It is a packaging
artifact for the eventual complete system closure; this change installs no
display manager, service, PAM file, executable or desktop entry on the host.
The complete selected app/font/library closure must exist before distribution.
The fixed development app owner is still separate from production app admission.

`linux.session_entry` exercises account/runtime selection, root/unequal/saved-ID
rejection, bounded paths, NNP failure and absent/supplied/failed notifications
with controlled adapters. Both it and `linux.session` pass native ordinary-user
Release and memory CMake runs. An explicit Type=exec/NotifyAccess=none carrier
case executes a separately hashed entry in an existing PAM/logind fixture and
passes the full KMS workflow. It uses the actual `/run/user/1000` hierarchy and
does not test password authentication or a display manager. Read the
[execution record](../brains/aetheri/journal/2026-11-10-session-entry.md).

4 November 2026. The optional compositor can now use an actual KMS connector and
evdev input on the dedicated Fedora carrier. This extends the private
[headless/nested prototype](graphics.md); normal Aetheri session/app admission,
trusted login/lock, suspend/resume and the complete desktop remain work.

## Selected path

`--drm SOCKET CAPTURE_DIR_OR_- /dev/dri/cardN` selects one explicit card. It is
available only with `AETHERI_COMPOSITOR=ON` and `AETHERI_DRM_SESSION=ON`; both remain
off by default. The selected wlroots must advertise DRM, libinput and session
support. The separate input-test executable retains its headless-only profile.

The ordinary-user compositor requires `LIBSEAT_BACKEND=logind` and a local seat0
session. It opens that same session once, waits up to five monotonic seconds for
asynchronous activation, then requests the selected card through libseat/logind.
It never tries seatd, direct privileged device access, another card or another
display backend. Missing/inactive/foreign seat, failed setup and lost devices
fail closed. Root and unequal real/effective IDs are refused.

One multi-backend joins DRM and libinput. Pixman paints DRM dumb buffers; this
profile needs neither a wlroots GL/Vulkan renderer nor GBM/libliftoff. Actual
connector events supply up to three outputs, using their preferred modes within
64–3840 by 64–2160. No headless output is fabricated. The current development
policy places outputs left to right and ends the session on output loss. This
does not implement the final display configuration/hotplug policy.

The existing seat/focus, native keymaps, pointer routing, move/resize and clipboard
code handles backend devices. No input-injection global is exposed by the normal
server. Devices are closed through their owning backends/session. Unexpected
libseat session destruction invalidates the retained pointer and terminates the
server; cleanup must not destroy that freed session a second time.
Failed input enrollment also marks startup failed: a pre-run display termination
alone must not be lost when the main display loop is subsequently entered.

Deactivation currently exits with failure rather than retaining a session with
unimplemented lock/resume policy. This is intentional provisional behavior, not
suspend/resume acceptance. After losing DRM master, wlroots may also report that
it could not disable the old CRTC; actual process/session exit and a new successful
acquisition must be checked separately.

## Dependency review and build

[session-inputs.json](session-inputs.json) adds eleven exact signed Fedora RPMs:

| Input | Version | Reason |
| --- | --- | --- |
| libinput + headers | 1.31.3-1.fc44 | Native evdev keyboard/pointer processing |
| libseat + headers | 0.9.3-1.fc44 | logind-mediated seat/device access |
| libdisplay-info + headers | 0.3.0-1.fc44 | wlroots DRM monitor information |
| libwacom + data | 2.19.0-1.fc44 | Fedora libinput runtime dependency |
| mtdev | 1.1.6-12.fc44 | Fedora libinput runtime dependency |
| hwdata + build metadata | 0.412-1.fc44 | wlroots monitor manufacturer lookup |

libudev 259 and systemd-pam 259.10-1.fc44 already exist in the carrier. Ten packages
were added and hwdata 0.405 replaced; the reviewed offline transaction adds about
3 MiB net. All eleven RPM signatures match full Fedora key
`36F612DCF27F7D1A48A835E4DBFCF71C6D9F90A6`. Hash/size checks, signature checks,
package scripts and the exact offline transaction were inspected first. No seatd
service/package was installed. No video/input group membership, device chmod,
new capability, SELinux permission or host/WSL setting was introduced.

These are reviewed carrier inputs, not a universal security or physical-driver
acceptance. MIT applies to the original code; libwacom retains HPND and hwdata
retains its packaged GPL-2.0-or-later terms. Keep the complete third-party notices
and applicable redistribution/source obligations when producing system images.

On the host, `prepare_vm.py verify --profile session` checks the cached RPMs.
The existing graphics source archive and its two unchanged private patches are
still pinned in [graphics-inputs.json](graphics-inputs.json). Build a fresh private
prefix as the ordinary guest account:

```sh
python3 tools/build_graphics.py SOURCE_CACHE NEW_PREFIX_WORK --session
# Add --sanitizers for the separately instrumented wlroots profile.
```

This profile checks the exact pkg-config dependency versions and records the
options/dependencies/library hash. Backends are DRM/libinput, session enabled,
renderers/optional allocators empty, libliftoff/Xwayland/color management disabled,
wrap downloads disabled. The previous headless profile remains available.

Configure Aetheri with both selected SDK prefixes in `CMAKE_PREFIX_PATH`, so CMake
regeneration retains them as well as the initial configure:

```sh
cmake -S . -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAETHERI_COMPOSITOR=ON -DAETHERI_DRM_SESSION=ON -DAETHERI_SETTINGS=ON \
  '-DCMAKE_PREFIX_PATH=WLR_PREFIX;UI_PREFIX' \
  -DAETHERI_SETTINGS_TEST_FONT=UI_PREFIX/share/aetheri-fonts/LiberationSans-Regular.ttf
cmake --build BUILD -j2
ctest --test-dir BUILD --output-on-failure -R '^(settings[.]|compositor[.])'
```

`compositor.session_arguments` checks five invalid backend/card requests without
contacting logind or acquiring any device. Missing/wrong seat-backend requests
must terminate without a Wayland socket or READY. The normal protocol/input/
clipboard/nested/Settings tests remain necessary; argument checks do not prove KMS.

## Explicit carrier session

The current QEMU already supplies standard VGA (bochs-drm), a PS/2 keyboard and
relative PS/2 mouse. `-display none` hides a host viewer; it does not remove those
guest devices. This bring-up therefore needs no reboot, new virtual GPU, disk
mutation or unauthenticated display/QMP listener.

The executed administrative fixture used a previously unoccupied VT and a
temporary system service: User/Group=aetheri, PAMName=login, TTYPath=/dev/tty2,
StandardInput=tty, journal output, TTYReset=yes, NoNewPrivileges=yes, empty
CapabilityBoundingSet/AmbientCapabilities and Restart=no. The first successful
manual fixture used tty2. A conflict with its getty alone does not solve automatic
activation when switching to tty2 after starting the service: the newly requested
getty can stop the conflicting compositor. That counterexample is retained.
Final native fixtures instead select the independently inspected free tty8,
outside this carrier's checked NAutoVTs=6/ReserveVT=6 range, with TTYPath, XDG_VTNR
and the getty conflict all matching tty8. No automatic-VT configuration was changed.
The existing tty1 and serial login are retained. Verify no other seat0 session
before selecting a console; never displace a user's existing desktop.

PAM creates the ordinary UID1000 carrier session; the selected environment declares
wayland/user/seat0/VT2. An explicit post-PAM `env XDG_RUNTIME_DIR=PRIVATE_RUNTIME`
keeps the development socket in an existing owned 0700 directory. Captures and
the Settings shader cache have separate owned 0700 directories. The Settings
fixture uses its own temporary background PAM service; direct init_t execution
from the home directory was correctly denied and was not bypassed with a policy
grant. Both processes run in the carrier's ordinary unconfined user domain with
zero capabilities. This is not the future confined Aetheri app/session policy.

On an already logged-in, active local test console, the ordinary-user command is:

```sh
LIBSEAT_BACKEND=logind XDG_RUNTIME_DIR=PRIVATE_RUNTIME \
  BUILD/aetheri-compositor --drm aetheri-development CAPTURES /dev/dri/card0
# Launch Settings in this explicit development runtime using spec/settings.md.
```

Transient units may be garbage-collected promptly after exit. Blank properties
on an unloaded unit are not an original exit-status observation. The final
development probe records exact live invocations/processes, then checks original
PID1 success events and the app's orderly-close output, actual process absence and
session removal. A journal receipt is historical, not a production drain proof.

The carrier's optional pam_lastlog2 module reports an unresolved pam_syslog symbol
under the systemd PAM service. Required PAM session setup succeeds and logind
reports the actual seat/VT/leader. Preserve that carrier warning; it is not proof
of a complete or accepted login manager, and no PAM configuration was weakened.
PAM scope migration also means application output is indexed under the actual
process/session, not necessarily the original service unit. Keep exact invocation,
leader and process identity when collecting journals.

## Native evidence and remaining work

The first actual Release session displayed Settings on Virtual-1 at 1280×800,
74.994 Hz. QMP keyboard/mouse input traversed actual PS/2, Linux/libinput, Wayland
and SDL. Tab/Enter selected Appearance; a native pointer click selected Light;
keyboard navigation selected 200% text and German. Original QMP PNGs were visually
inspected. A one-second idle observation recorded unchanged process CPU ticks
and compositor commits (28 before/after), not a whole-session performance budget.
Ctrl+Q closed Settings with status 0. Switching to tty1 terminated the provisional
compositor with status 1, removed its seat session and left both processes absent.

Mouse down/up must use separate QMP input batches: PS/2 synchronization can
coalesce both changes in one batch into no click. Keyboard and screenshot evidence
must reflect actual device behavior, not an assumed absolute pointer coordinate.
QMP remains only host-local AF_UNIX below the protected VM directory.

Original inactive-VT/getty-race attempts, the denied direct Settings service and
initial test setup mistakes remain in private evidence. The Settings test needs
the font asset from the reviewed SDK prefix because it also checks actual mapped
library provenance; a system-font path is not that provenance.

Final profile tests and the fresh Debug DRM reacquisition are recorded in STATE.
No frozen SQL/XML, installed authority capability or protected old execution
catalogue changes. These development sessions cannot settle that old intent.
The [supervised development controller and native console](development-session.md)
now provide repeatable start/status/stop with their own narrow test boundary.
Next: desktop shell, normal session/app admission, trusted lock/resume, device settings,
original boot image/installer and separate physical hardware acceptance.
