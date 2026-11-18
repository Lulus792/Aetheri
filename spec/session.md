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

The bound session entry can compile `AETHERI_DESKTOP_SHARE` alongside its
selected library path. It takes libinput quirks and EGL vendor data from
that directory and Mesa drivers from the selected library directory's
`dri` subdirectory. The bound runtime supplies these child-process paths.
The standard PAM profile can stay focused on authentication and login.
[libglvnd vendor discovery](https://github.com/NVIDIA/libglvnd/blob/master/src/EGL/icd_enumeration.md)

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

Before account/session lookup the entry closes every inherited descriptor above
stderr with `close_range(..., CLOSE_RANGE_UNSHARE)`. It accepts no activation,
document or credential FD from its parent. Standard input/output/error remain;
new session observers and desktop channels are opened afterward. Failure ends
the entry before any child starts. The fixed development owner's invocation is
unchanged. This local descriptor cleanup is not document revocation or session/
app/lease drain. Native fork tests check actual closure, preserved parent and
stdio descriptors, and a failed syscall; an omitted-cleanup counterexample fails.

All three standard descriptors must already be open. Missing stdio rejects
startup, preventing a later diagnostic socket from occupying a standard slot.
After extra-FD cleanup, an entry attached to a console opens a local
`sd_journal_stream_fd("aetheri-session", LOG_INFO, 0)` and duplicates it onto
stdout/stderr. The process owns these standard-stream duplicates until exit;
the auxiliary descriptor closes before account/session work or child creation.
A failed stream connection preserves the original console. A failed duplicate
or close ends startup. Existing non-console output is preserved. This uses the
existing libsystemd and adds no resident logger process or thread. Native fork
tests cover console redirection, missing-journal fallback, non-console output,
auxiliary closure and missing stdio; an omitted-stdout-duplication counterexample
fails. This diagnostic stream is not a trusted readiness or lifecycle API.

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

In the [confined graphical login VM](../brains/aetheri/journal/2026-11-17-confined-graphical-desktop.md),
the real greeter starts the selected entry through the authenticated PAM
worker. The entry creates its UID-owned runtime independently of the
greeter's scratch. The private POSIX shared-memory mount uses distinct
file labels for the two sides of login. The test observes the original
entry and worker end, user PAM closure and return to a fresh greeter.

### Session-owned runtime cleanup

The owner creates its runtime relative to the selected parent directory
and retains close-on-exec descriptors for the parent, runtime and cache.
After its tracked children end and their output closes, it compares each
name with its retained directory identity. It removes the original cache
and then the original runtime with empty-directory operations. On setup
failure it also retires any directories it already holds.

The owner logs cleanup separately from the session result. A missing,
busy, replaced or nonempty directory stops that attempt. Only the two
owned names are considered. The identity checks and removals are separate
system calls, with cooperating writers under the same UID assumed.
The [cleanup verification](../brains/aetheri/journal/2026-11-17-session-runtime-cleanup.md)
checks that the just-ended runtime is gone while the same user's original
runtime parent and an independent sibling remain available.

`linux.session_entry` exercises account/runtime selection, root/unequal/saved-ID
rejection, bounded paths, NNP failure and absent/supplied/failed notifications
with controlled adapters. Both it and `linux.session` pass native ordinary-user
Release and memory CMake runs. An explicit Type=exec/NotifyAccess=none carrier
case executes a separately hashed entry in an existing PAM/logind fixture and
passes the full KMS workflow. It uses the actual `/run/user/1000` hierarchy and
does not test password authentication or a display manager. Read the
[execution record](../brains/aetheri/journal/2026-11-10-session-entry.md).

The separate [authenticated session test](login.md#authenticated-desktop-fixture)
now executes the current entry through the pinned backend's actual PAM worker,
with an independently selected protected runtime and local VT8/logind session.
Both Release and memory-instrumented test greeters reach a visually inspected
desktop and actual PAM close. The selected entry and desktop runtime are Release
code in both cases. This does not install a display manager, establish production
account/device/MAC policy or admit normal app lifecycle operations.

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

The [suspend VM profile](login-deployment.md#suspend-runtime-files) preserves
these checks. It exercises kernel sleep and return to the original desktop.
Release suspend requires every active output to present a covered frame and
ordinary applications to lose input focus before sleep. After waking, a
trusted unlock can restore application input and content to that session.
Loss of the lock client keeps the display covered. Laptop and workstation
hardware must each pass device recovery and unlock. [Wayland session-lock protocol](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/1.49/staging/ext-session-lock/ext-session-lock-v1.xml)

### Compositor cover

The compositor can take `--lock-fd FD` in a controlled headless or DRM
session. The caller supplies one end of an unnamed Unix seqpacket socket.
The first packet must be the five bytes `AELK1` with no descriptors.
The compositor clears ordinary seat focus, cancels any active move or
resize, hides the cursor and disables the desktop and application
scene branch. Applications keep running behind the cover.

`AELK1 COVERED` acknowledges the original request. The compositor first
requires a successful buffer commit and its matching positive presentation
report on every enrolled active output. An older commit sequence cannot
meet this condition. After sending the response, the compositor closes the
channel. A failed response or lost channel leaves an active cover in place.
The current entry supports one transition per compositor lifetime.

The installed desktop login path still needs a trusted lock UI and an unlock
verifier bound to the session. Keep ordinary input and output covered until
that verifier accepts the fresh attempt. Connect user and hardware sleep
requests after those paths are exercised together. The
[cover record](../brains/aetheri/journal/2026-11-18-desktop-lock.md) documents
headless and virtual DRM evidence.

### Managed compositor lock

The optional `--session-lock-fd FD` entry uses a different private channel.
The compositor requires an unnamed socket from its same-user parent,
pins that process and accepts packets only with its kernel sender
descriptor. The controller waits for compositor readiness before sending.
The original process and channel must stay live during a release.

Managed packets carry the five bytes `AELK2`, a one-byte operation and an
eight-byte unsigned generation in network byte order. The generation starts
at zero and increases for every cover. The channel belongs to this
compositor instance.

| Operation | Code | Meaning |
| --- | --- | --- |
| `COVER` | 1 | Request a cover while open, using the last generation |
| `COVERED` | 3 | Notify the controller of the new generation after all output presentations |
| `RELEASE` | 2 | Apply the controller's verified result for the current covered generation |
| `RELEASED` | 4 | Confirm acceptance of that release and schedule the ordinary scene |
| `REJECTED` | 5 | Refuse the request and echo its generation |
| `REQUEST` | 6 | Notify the controller that physical Super+L began the indicated new cover |

The controller authenticates separately before it sends a release. The
compositor also waits for all current outputs to be covered and every
tracked keyboard key and pointer button to be up. Then it restores ordinary
focus and the existing scene. If the saved window disappears while
covered, focus is selected from the remaining mapped windows. Wrong
generations leave the cover intact. Peer loss or malformed input closes
the channel and discards any pending release.

The [managed lock record](../brains/aetheri/journal/2026-11-18-managed-lock.md)
includes a fixture that consumes real PAM outcomes and restores the exact
original scene. The signed desktop still needs the installed owner and
the sleep and wake paths.

### Desktop owner lock broker

Physical Super+L starts a cover immediately when the managed compositor
has a private view. The compositor tells the broker the new generation
and separately reports its completed cover. A broken controller
connection leaves the cover active with the dialog hidden.

The candidate `AETHERI_DESKTOP_SESSION_LOCK` build wires the broker
into the session owner's poll loop. The owner creates the private
compositor, display and credential pairs, launches the original dialog
and pins both children. It selects the account from the authenticated
UID. The broker recognizes compositor notifications and password
submissions by their original kernel sender pidfds. Before applying
any result, it rechecks both children and the original login context.
App launches from the desktop shell require the broker's open state
when this option is enabled.

The broker accepts one attempt for a fully covered generation and
starts one finite worker for it. It wipes its bounded receive buffer
after handoff. A wrong password returns the generic refusal. An
unavailable worker returns the generic retry status. A verified result
requests release for the same generation. The compositor still waits
for neutral input. Loss of the session, protocol or actor closes the
channels and discards outstanding outcomes.

This candidate exercises the unprivileged broker and real PAM in
the development VM. In the installed path, the owner and all its
children inherit `NoNewPrivileges`, so executing a setuid helper preserves
the existing UID. Release integration needs password
verification under a separately admitted authority with access to
the protected local account verifier. The candidate is limited to
development builds pending those protected runtime inputs. The
[broker record](../brains/aetheri/journal/2026-11-18-lock-broker.md)
tracks the VM and rights boundary. [Linux kernel privilege flag](https://docs.kernel.org/userspace-api/no_new_privs.html)

### Private password dialog

Build the optional `aetheri-lock-view` executable with
`AETHERI_LOCK_VIEW=ON` and `AETHERI_COMPOSITOR=ON`. The same original
parent supplies `--lock-view-fd FD` to the compositor and the other
end to the dialog. The compositor checks the stream against the
original managed lock's parent pidfd. The dialog takes the display FD,
credential FD, font path, local account and `en` or `de` in that order.
The current candidate expects both unnamed endpoints from its same-user
parent and uses the inherited display stream exclusively.

The compositor gives that client one surface and limits its Wayland
globals to compositor, shared memory and seat. First, all enrolled
outputs present the black cover. Then the compositor renders the dialog
and its black surroundings together on the first output. The other
outputs remain black. The dialog receives keyboard focus only after
a matching successful commit and positive presentation for its own
frame and after neutral input.
Ordinary surfaces and pointer input stay fenced. Loss of the private
client or surface closes the managed control channel and keeps an
active cover in place.

SDL draws the clock, local account, password field and action into
release-tracked shared memory. Password bytes stay in the bounded
input buffer and credential socket. The rendered draft shows a bounded
mask. Tab moves between controls, Enter submits, Backspace removes a
character and Escape clears the draft. Submission and focus loss
also clear it.

The private credential channel uses one seqpacket per submission.
`AELV1` precedes exactly the nonempty password bytes.
The controller identifies the original dialog's kernel sender and
associates the attempt with the current lock generation. A five-byte
`AELV1` prefix followed by one byte returns a generic retry status.
Value 1 means denied and 2 means unavailable. A successful PAM
result uses the managed lock's release, which still waits for physical
input neutrality. The retry status carries no account details.

The selected font can be passed as `AETHERI_LOCK_VIEW_TEST_FONT` for
the native compositor test. The [password dialog
record](../brains/aetheri/journal/2026-11-18-lock-view.md) covers visible
states and real PAM in the carrier. The installed desktop still needs
the signed dialog and broker, production policy and trusted power
coordination. Pointer interaction, scalable layout and accessibility
services remain open for the dialog.

### Password verification for unlock

The password check in `src/login/unlock_pam.c` takes the account name and UID
selected by the trusted session owner. The local account database must resolve
both to the same account before and after verification. The conversation accepts
one hidden password prompt during authentication. The worker discards PAM text
and rejects additional prompts, changes to the PAM account and expired
credentials. Passwords are bounded to 512 bytes to match the selected
`pam_unix` helper.

The build option `AETHERI_UNLOCK_PAM=ON` compiles the verifier and its
conversation test. Select `AETHERI_UNLOCK_PAM_INCLUDE_DIR` and
`AETHERI_UNLOCK_PAM_LIBRARY` from the reviewed SDK and runtime. The
[SDK record](login-unlock-pam-inputs.json) identifies the three development
headers. The existing runtime contains the PAM library, modules and helper.
The candidate `data/pam.d/aetheri-unlock` service checks password and account
status and denies session creation and password changes. Its final deployment
needs protected profile admission alongside the verifier. [Linux-PAM
conversation contract](https://github.com/linux-pam/linux-pam/blob/v1.7.2/doc/man/pam_conv.3.xml)

Only the owner of the protected original lock may act on a successful check.
Bind the result to one live, outstanding attempt for that lock and session.
Loss of the seat, worker, credential channel or lock instance keeps the cover
in place. The trusted UI and worker own and wipe their own password copies.
The [unlock verification record](../brains/aetheri/journal/2026-11-18-unlock-pam.md)
covers the standalone verifier and its private Linux-PAM test.

### One worker per unlock attempt

The broker creates a fresh socket pair before starting the selected
`aetheri-unlock-worker` binary. The child receives only the worker socket and
the chosen account name. It runs as the unprivileged session user, checks its
original parent, disables process dumps and sets close-on-exec on the socket.
The broker supplies the password only after spawning the worker. The worker
clears the received password when PAM returns. The broker also clears its
own copy after submitting the packet.

Each request carries a fresh random ticket and at most 512 password bytes.
The result contains the ticket and a single outcome. Each endpoint checks the
kernel-supplied process descriptor for the packet against its original peer
identity. The receiving broker also requires orderly end of the channel and
normal zero exit of the original child. Keep the original child unreaped
until the attempt consumes the outcome. The broker then owns its retirement.

The broker supplies the current covered generation when consuming the result.
A lost or changed generation invalidates the attempt permanently, and each
outcome is returned at most once. The caller must confirm the actual active
session and original cover before applying a display change. The installed
desktop still needs the protected broker and password UI connected to the
managed compositor path. [Kernel Unix socket credentials](https://github.com/torvalds/linux/blob/v6.19/net/unix/af_unix.c)

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

## Installed desktop presentation

The login image builds the desktop shell with SDL software drawing into
Wayland shared-memory buffers. SDL still owns the custom-role windows, input
and format-aware text drawing. The desktop selects advertised ARGB8888
and presents its premultiplied pixels on the existing layer surfaces. The
development profile retains OpenGL ES.

Each view has up to two buffers. The client reserves storage on the private
`/dev/shm` before drawing, attaches a completed buffer, and waits for its
`wl_buffer.release` before reuse. Resize can replace an idle buffer while
the compositor retains the other. If both are busy, the next release wakes
the pending redraw. Geometry is limited to 7680 by 4320 pixels per buffer.
Teardown destroys the client objects without reusing their storage.

The signed VM test verifies the visible dock, keyboard-driven search
resize, deliberate logout, PAM close and replacement greeter. The VM
uses one virtual display. The test covers the desktop shell's renderer.
The signed application's own renderer and hardware paths have separate
acceptance work. See the
[installed VM record](../brains/aetheri/journal/2026-11-17-login-image-boot.md#wayland-shared-memory-desktop).

### Fixed desktop app entrypoints

The installed policy assigns separate process and executable types to
Settings, Files and Terminal. The session owner can start the three fixed
entrypoints through domain transitions with `NoNewPrivileges` in effect.
Files can also start the fixed Terminal entrypoint for its document flow.
Only the Terminal domain receives execution rights for the packaged PTY
starter, shell and editor files. The roles use the private compositor
connection and separate shared-memory and document labels.

For this policy, verify the current loaded decisions with
`tools/check_login_policy.py --applications --expected-policy-sha256 SHA256`.
The three roles have no generic executable, graphics device, or
executable anonymous-memory permission. Settings has no document access.
The role policy assumes labelled installed objects. App ownership, updates
and removal continue to use their separate authority contracts.

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
