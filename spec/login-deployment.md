# Graphical login deployment

The login layout is a staging input for an Aetheri system image. It gives the
backend, startup verifier, greeter, and desktop entry stable paths so the
deployment provider can bind them to independently selected release inputs.

| Path | Purpose |
| --- | --- |
| `/usr/libexec/aetheri/` | Backend, startup verifier, greeter, its display process, session entry, compositor and desktop |
| `/usr/share/aetheri/login/` | Signed release selection, backend configuration, greeter font and SELinux policy inputs |
| `/etc/pam.d/aetheri-login` | Local human authentication and session setup through PAM |
| `/etc/pam.d/aetheri-greeter` | Internal PAM session for the locked display account |
| `/etc/pam.d/aetheri-unlock` | Password and account checks for the active desktop's lock |
| `/etc/pam.d/other` | Deny authentication, account, password and session requests for unknown PAM services |
| `/etc/nsswitch.conf` | Local account and group lookup, with DNS available for host names |
| `/etc/ld.so.conf` | Search path for the image's reviewed desktop libraries |
| `/usr/lib/systemd/system/` | Backend service, ordered startup verifier and worker shutdown drop-in |
| `/usr/lib/sysusers.d/` and `/usr/lib/tmpfiles.d/` | Display account and per-boot directory recipes |
| `/usr/share/wayland-sessions/aetheri.desktop` | Entry from an existing local Wayland login |

The backend takes VT8 and starts its own greeter. Explicit image activation
can select it as `display-manager.service`. It pulls in the finite startup
verifier, which runs once after the original backend starts and publishes the
protected observation it verified. The [startup contract](login.md#finite-startup-executable)
defines the observation, ordering, and original process checks. The worker
shutdown drop-in preserves the backend's original worker and PAM close path.

The human PAM profile checks the local password and account, excludes UID 0
and `aetheri-greeter` from password authentication, and opens a logind session.
An expired password requires a separate account-management workflow. Image
assembly must enroll and independently verify the locked greeter account and
its groups. The carrier's choice of numeric UID/GID is local to that VM.

Both installed adapters include all three PAM files in the common protected
source set. The signed selection supplies the two named profiles. The installed
adapter supplies a fixed digest for the standalone image's deny-only fallback.
The startup helper checks the files in the original backend's mount view
before publishing the handoff. The greeter checks the same named and held files
before credential transport and keeps them in its ongoing admission checks.
The image platform gate also covers PAM's loader, module files, account data and
configuration search paths. Linux-PAM loads the default service alongside a
named profile. [Linux-PAM configuration loader](https://raw.githubusercontent.com/linux-pam/linux-pam/v1.7.2/libpam/pam_handlers.c)

The [session entry](session.md) can supply the runtime's graphics and input
data paths when building against a protected runtime projection. The
packaged human PAM profile contains the account, authentication and
session steps. Image integration selects and verifies the actual runtime.

Root owns `/run/aetheri/login` and its create-only handoff. The greeter owns
the mode-0700 render directory `/run/aetheri/login-greeter`. The backend and
its descendants share their own `/tmp` and `/dev/shm` mounts backed by distinct
Root-owned mode-1733 directories under `/run/aetheri`. SELinux uses distinct
types for greeter and user objects. `ProtectSystem=full` makes the system paths
read-only in that namespace and leaves the user desktop's home available.
Image assembly applies the chosen file contexts and must verify that creation
at boot keeps those contexts, including on both scratch directories.

## Stage selected inputs

With the installed startup, greeter and desktop profiles configured, use
`cmake --build BUILD --target aetheri-login-image-artifacts` to build the
native executables. The installed desktop starts the compositor with Compose
enabled, so select the [reviewed Compose inputs](compose.md#standard-data-and-licenses).
Use the compositor ELF for both `aetheri-compositor` and `aetheri-login-display`.
Build the selected greetd backend separately.

```sh
python tools/prepare_login_deployment.py --inputs login-inputs.json --output login-stage
```

Use a fresh destination with an existing parent. The JSON object has `format`
set to `2` and an `artifacts` object with exactly these keys.

- `aetheri-login-backend`, `aetheri-login-startup`, `aetheri-login-greeter`,
  `aetheri-login-display`, `aetheri-session`, `aetheri-compositor`, `aetheri-desktop`
- `aetheri-settings-nexaui`, `aetheri-files-nexaui`, `aetheri-terminal-nexaui`,
  `aetheri-terminal-pty-starter`, `aetheri-shell`, `aetheri-editor`
- `greeter-font`, `greeter-font-license`, `backend-license`, `backend-dependency-notices`

Each value contains `path`, `size` in bytes, and `sha256` as 64 lowercase hex
digits. Relative paths resolve against the manifest directory. Select and hash
the built artifacts independently of the stager. It checks the selected size,
digest and basic file format before creating the destination. `root/` holds
the package tree. `receipt.json` lists the target paths, file sizes, intended
POSIX modes in decimal, and SHA-256 digests in path order. A partial write has
no completion receipt until all files are written. Use a fresh tree for a new
attempt.

Use staging format `3` for the Sound mediator without desktop lock. Add
`aetheri-audio-desktop-control` to the artifact list. Use format `4` for
desktop lock and add `aetheri-lock-view`. In format `4`, include
`aetheri-audio-desktop-control` when the selected desktop starts the mediator.
The stager takes the unlock PAM profile and lock file contexts from this
source tree. Build the backend with `aetheri-session-unlock` and the native session
with `AETHERI_DESKTOP_SESSION_LOCK=ON` and
`AETHERI_DESKTOP_UNLOCK_AUTHORITY=ON` for that image.

Use format `5` to require a covered session before installed suspend. Build
the session with `AETHERI_DESKTOP_SESSION_SUSPEND=ON`, add
`aetheri-suspend-guard` to the artifact list and supply `--suspend-stage`.
Sound remains optional in this format.

## Image integration

### Intel HDA kernel modules

Stage the HDA layer in the composed desktop root before generating the
combined kernel module indexes and applying the image file contexts.

```sh
python3 tools/desktop_hda_payload.py --image-root "$IMAGE_ROOT" --kernel-root "$KERNEL_ROOT"
depmod -C /dev/null -b "$IMAGE_ROOT" -a 6.19.10-300.fc44.x86_64
```

`KERNEL_ROOT` supplies the selected package layout under
`usr/lib/modules/6.19.10-300.fc44.x86_64`. Use the kernel that matches
the image and the paths and digests from the reviewed
[HDA module selection](audio-hda-inputs.json) and
[Conexant supplement](desktop-hda-inputs.json). The developer desktop builder
also accepts `--hda-modules-root` and regenerates the indexes in the resulting
image.

The existing boot-time module loader reads `aetheri-hda.conf`. The Intel
controller uses the resulting indexes for codec selection. The image keeps
sound nodes under standard kernel and udev ownership. PipeWire, WirePlumber,
and the Settings sound control connection have their own admission work.

### Intel PCI WLAN files

The [WLAN inventory](desktop-wlan-inputs.json) selects the `iwlwifi` and
`iwlmvm` modules and their four local dependencies for the existing Fedora
kernel. The PCI rule requests the device's reported alias when an unbound
Intel `8086:24fd` device is added. The kernel chooses the matching radio
configuration and requests its firmware. The inventory includes the two
API 36 firmware files referenced by the applicable Intel configurations
and their original license notice. [Linux Intel configuration](https://github.com/torvalds/linux/blob/v6.19/drivers/net/wireless/intel/iwlwifi/cfg/8000.c)

Use the already selected kernel tree for `--kernel-root`. Put the two
uncompressed `.ucode` files and `LICENCE.iwlwifi_firmware` from the recorded
commit into `--firmware-dir`. Supply the exact Fedora `wireless-regdb` RPM.
The stager verifies the RPM signature before selecting `regulatory.db`,
its kernel signature and its license notice. Keep the database and its
signature together so the kernel can enforce regulatory constraints.
[Fedora regulatory database package](https://packages.fedoraproject.org/pkgs/wireless-regdb/wireless-regdb/fedora-44.html)

```sh
python3 tools/desktop_wlan_payload.py --image-root "$IMAGE_ROOT" \
  --kernel-root "$KERNEL_ROOT" --firmware-dir "$WLAN_FIRMWARE" \
  --regdb-rpm "$REGDB_RPM"
depmod -C /dev/null -b "$IMAGE_ROOT" -a 6.19.10-300.fc44.x86_64
```

The development desktop builder accepts the same three inputs through
`--wlan-inputs` and regenerates the combined indexes after staging. Keep
the WLAN receipt with the image inputs. Settings controls and physical HP
acceptance have their own runtime tests.

### Wireless connection manager files

The [iwd inventory](desktop-iwd-inputs.json) pairs Fedora iwd 3.12 with
libell 0.83. iwd controls scanning, association and WPA authentication. Its
optional network configuration can obtain addresses through DHCP. The
stager checks the Fedora signatures and payload hashes before selecting
the daemon, the library, its SONAME alias and the original license notices.
The iwd startup self-check also requires the pinned generic DES module and
its `libdes` dependency from the selected kernel build.
[Fedora iwd package](https://packages.fedoraproject.org/pkgs/iwd/iwd/fedora-44.html),
[Fedora libell package](https://packages.fedoraproject.org/pkgs/libell/libell/fedora-44.html),
[iwd configuration](https://kernel.googlesource.com/pub/scm/network/wireless/iwd/+/master/src/iwd.config.rst).

```sh
python3 tools/desktop_iwd_payload.py --image-root "$IMAGE_ROOT" \
  --kernel-root "$KERNEL_ROOT" --packages "$IWD_RPMS"
depmod -C /dev/null -b "$IMAGE_ROOT" -a 6.19.10-300.fc44.x86_64
```

The development desktop builder accepts the same inputs through
`--iwd-inputs` and regenerates the combined indexes. The payload adds a
system-bus rule that permits root to own and call `net.connman.iwd` and
denies direct application calls. The runtime unit sets
`DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket` for libell.
Aetheri puts the socket on `/run` and the library defaults to `/var/run`.

In the development image, an explicit administrative request starts
`aetheri-wlan.service`. Installing the unit leaves it inactive at boot.
systemd waits for the `net.connman.iwd` bus name on start. The daemon reads
`/usr/share/aetheri/wlan/main.conf` and keeps profiles in the root-owned
mode-0700 `/run/aetheri-wlan`. systemd removes that directory on stop.
[systemd service readiness](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html),
[systemd runtime directories](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html#RuntimeDirectory=).

Select `--desktop-wlan` when compiling the image policy and apply the WLAN
file contexts before boot. The unit requires SELinux and selects
`aetheri_wlan_t` for the labeled iwd executable. The role uses the system
bus, the wireless device, the required network sockets and its own runtime
directory. The immutable configuration and runtime have separate labels.
The desktop app roles cannot execute the daemon, inspect its private state
or send directly to it. Keep the iwd receipt with the image inputs.

This profile enables IP configuration and leaves DNS integration and
persistent credentials pending.

### WLAN control for Settings

Build `aetheri-wlan-control` with `AETHERI_LINUX_IPC=ON` and
`AETHERI_WLAN_CONTROL=ON`. Select it with `--control-executable` in the iwd
stager or `--wlan-control` in the development desktop builder. Use
`--desktop-wlan-control` with `--desktop-wlan` for the image policy and
apply the controller and unit file contexts. The control service starts on
a system-bus request and can exit after an idle interval.

The [Wlan1 interface](api/wlan-v1.xml) exposes two methods. `GetState`
reports the iwd unit state as `off`, `starting`, `on`, `stopping`, `failed`
or `unavailable`. `SetEnabled` accepts a boolean and returns once systemd
accepts a job for the fixed WLAN unit. Settings reads `GetState` for the
resulting state. Association and IP configuration have their own state and
will use separate operations.

Both methods require the bus-authenticated original Settings process in
the active local seat0 Wayland user session. The controller rechecks the
live process, SELinux context, session and cgroup before forwarding a
change. Systemd enforces the controller's start and stop permissions on the
WLAN unit label. `NotAuthorized` means the caller check failed. `Unavailable`
means the backend could not be admitted. `OutcomeUnknown` requires a state
read before another change request because the original job might have
reached systemd. The request contains no caller identity or unit name.

The session and scope lookups use systemd's read-only cgroup view of
Settings and PID 1. The controller reads logind's session and seat
records under `/run/systemd` to verify the active local owner. It has no
permission to modify the PID 1 or logind records or trace PID 1.

The [signed desktop test](../brains/aetheri/journal/2026-11-18-desktop-iwd.md#installed-wlan-control-from-settings)
uses a probe ELF in the normal Settings launch. It confirms the unit
cycle and the direct-call denials under enforcing SELinux. The native
Settings switch and network association are separate acceptance cases.

### PipeWire and WirePlumber files

Stage the runtime in the offline desktop root before applying file contexts.
Supply the checked prefixes from [PipeWire preparation](audio.md) and the
[WirePlumber Lua 5.5 candidate](audio-policy-candidate.md). For HDA, also
prepare the [ALSA library and plug-in prefix](audio-hardware.md) with
`tools/prepare_audio_hda.py prepare`. Verify the `systemd-udev` RPM against
the [sound rule pin](audio-udev-inputs.json), including the Fedora signature
and package SHA-256. Extract only the selected rule into the build staging
area. The stager checks the rule's length and digest again.

```sh
rpm2cpio "$UDEV_RPM" | cpio --extract --quiet --to-stdout \
  './usr/lib/udev/rules.d/78-sound-card.rules' > "$SOUND_RULE"
```

```sh
python3 tools/desktop_audio_payload.py --image-root "$IMAGE_ROOT" \
  --pipewire-prefix "$PIPEWIRE_PREFIX" --wireplumber-prefix "$WIREPLUMBER_PREFIX" \
  --hda-prefix "$ALSA_PREFIX" --sound-rule "$SOUND_RULE" \
  > desktop-audio-receipt.json
ldconfig -r "$IMAGE_ROOT"
```

The daemons are `aetheri-pipewire` and `aetheri-wireplumber` under
`/usr/libexec/aetheri`. Shared libraries, modules, routing scripts, ALSA profiles
and license files keep their package-relative locations. The payload omits SDK
headers, the unversioned PipeWire SDK link and test commands. The stager reuses
matching installed files and refuses conflicting objects. Keep the receipt with
the build records. After a copy failure, inspect the partial tree and start the
next build from a fresh image root.

`/usr/share/aetheri/audio` contains the graph and client configuration and a
WirePlumber fragment. The fragment uses the pinned ACP card profiles to expose
HDA routes. The udev rule marks cards initialized after their control nodes
are processed. The graph socket is for trusted session roles. Desktop apps
need their own permission before connecting. [PipeWire configuration](https://pipewire.pages.freedesktop.org/pipewire/page_man_pipewire_conf_5.html)
[WirePlumber lookup rules](https://pipewire.pages.freedesktop.org/wireplumber/daemon/locations.html).

The [session integration](audio-session.md#desktop-session-owner) is a build
opt-in. Prepare the matching policy with `--desktop-audio` and use the
effective policy from that image in the signed selection. For the
[Settings mediator](audio-session.md#desktop-sound-mediator), enable
`AETHERI_DESKTOP_SESSION_SOUND` in the image build and select
`aetheri-audio-desktop-control` as the extra executable in a format-3
login artifact manifest. Install it as Root-owned mode 0555 at
`/usr/libexec/aetheri/aetheri-audio-desktop-control`.

### Suspend runtime files

Use [the suspend payload](desktop-suspend-inputs.json) for the pinned
systemd inputs in a suspend-enabled image.
It contains the three systemd units and the helper that requests sleep from the
kernel. The stager reads the same signed systemd-udev 259.10 RPM used for the
sound rule. It verifies the package signature and digest, then checks the four
members before writing them to a fresh tree. The optional base check verifies
the existing PID 1, logind, shared systemd library and LGPL notice against the
desktop image's pins.

```sh
python3 tools/prepare_desktop_suspend.py --rpm "$UDEV_RPM" \
  --base-root "$IMAGE_ROOT" --output "$SUSPEND_STAGE"
```

Apply the modes, root ownership and SELinux contexts in `inputs.json` when
assembling a VM-only suspend baseline. In format `5`, pass the staged payload
to the login deployment stager. It independently verifies the four stock
file digests and sets the selected helper's final mode to 0555. The three
stock units keep mode 0644 and use `power_unit_file_t`. The helper uses
`systemd_sleep_exec_t` so the distribution policy assigns its own domain.
The stager also supplies the Aetheri guard service and the drop-in for
`systemd-suspend.service`, both mode 0444. Keep the separate payload receipt
and verify the final image bytes.

The drop-in orders the original sleep job after the guard and binds it to
the guard's lifetime. The guard reports systemd readiness only after the
original session confirms that its outputs are covered and its password
dialog is paused. See [the session protocol](session.md#suspend-and-resume-lock-coordination)
and the [upstream sleep unit](https://github.com/systemd/systemd/blob/v259.10/units/systemd-suspend.service.in).

### Compile the image policy

```sh
python tools/prepare_login_policy.py --base "$POLICY_BASE" --output login-policy
```

Select a stable copy of the distribution's `etc/selinux` and
`var/lib/selinux/targeted/active` trees. The compiler builds the login policy
inside a fresh copy of that store and stages the
policy and login file contexts under `login-policy/root/etc/selinux`.
Pass `--desktop-audio` when building an image with `AETHERI_DESKTOP_SESSION_AUDIO=ON`.
Add `--desktop-sound` for the image with the Settings mediator.
Add `--desktop-lock` for the image with the installed password dialog.
Add `--desktop-suspend` alongside `--desktop-lock` for format `5`.
Add `--desktop-wlan` for the WLAN service.
The receipt identifies the inputs and compiled policy. The compiler store
stays beside that layer. [SELinux module compiler](https://github.com/SELinuxProject/selinux/tree/3.11/policycoreutils/semodule)

Apply the file contexts to the image while assembling it. Boot the selected
kernel with that policy in an isolated VM and capture the bytes of
`/sys/fs/selinux/policy` while the kernel is enforcing. Review that boot
observation and use it as `--effective-policy` during selection signing.
The compiled policy on disk and the kernel's serialized readback can have
different digests even when the byte counts agree.

### Installed image provider

Build the startup executable and greeter with the same selected Ed25519
public key and release ID. Add these options to the normal Linux build
configuration with the reviewed dependencies.

```sh
cmake -S . -B build-login \
  -DAETHERI_LINUX_IPC=ON -DAETHERI_LOGIN_PROTOCOL=ON \
  -DAETHERI_LOGIN_STARTUP=ON -DAETHERI_LOGIN_VIEW=ON -DAETHERI_LOGIN_GREETER=ON \
  -DAETHERI_LOGIN_IMAGE=ON -DAETHERI_NEXAUI_PREFIX="$NEXAUI_PREFIX" \
  -DAETHERI_LOGIN_IMAGE_PUBLIC_KEY="$LOGIN_PUBLIC_KEY" \
  -DAETHERI_LOGIN_IMAGE_RELEASE="$LOGIN_RELEASE_ID"
cmake --build build-login --target aetheri-login-startup aetheri-login-greeter
```

Supply 64 lowercase hexadecimal digits for each anchor. Both programs get
the same generated header. Image mode and explicit custom provider sources
are mutually exclusive. The fixed image provider checks the program's
expected SELinux domain and no-new-privileges before invoking the installed
adapter. It verifies a read-only ext4 superblock, read-only current mounts,
and the selected system and runtime directories. A service bind mount may
project a directory at its original image path. A different subtree or a
read-only bind onto a writable superblock fails. Linux provides the mount
identity and superblock attributes through `statmount`.
[Linux mount UAPI](https://github.com/torvalds/linux/blob/v6.19/include/uapi/linux/mount.h)

After placing the pinned libraries, generate the loader cache within the
offline assembly using the selected glibc's `ldconfig -r "$IMAGE_ROOT"`.
The installed provider uses the image cache and selects
`/usr/share/libinput` for input quirks. It does not pass a library path in
the greeter's child environment. Review the cache entries and retain the
protected root and mount layout from boot. Boot and image assembly still
own provenance, the account and PAM inputs, runtime library closure and
the effective device policy.

### PAM runtime supplement

```sh
python tools/prepare_login_pam_runtime.py --output login-pam-stage
```

Run this as an ordinary Linux build user with the exact source files listed
in [the PAM runtime inventory](login-pam-runtime-inputs.json) and `readelf`
available. It stages three additional PAM modules, the password helper,
the locked account's `nologin` shell, and their additional libraries and
notices. It verifies hashes during copying
and checks the staged ELF references against the supplement and the selected
desktop and boot inventories. The receipt records the required base paths
and the final image metadata for the password helper. The staged helper
has mode 0755. The image assembler still needs to apply and verify the final
Root ownership, mode 04755 and SELinux context on the assembled image only.

Inspect the assembled root after applying final ownership, modes and labels.
Use a stable offline root and a reader that can see its SELinux file contexts.

```sh
python tools/verify_login_pam_root.py --root /mnt/aetheri-image
```

The verifier checks the three PAM configuration files and follows the selected
module files' ELF dependencies through exact aliases to pinned file bytes in
the image. It checks the loader and the password helper's path, mode and label.
Pass `--desktop-lock` to verify the unlock profile's exact bytes, mode and
label and include its PAM module requirements.
The JSON result lists the checked objects and references. Image boot tests still
need to exercise loader configuration, account data, the installed platform
gates and full PAM session creation. The
[composed PAM check](../brains/aetheri/journal/2026-11-17-login-pam-root.md)
loads the selected modules with the image's own libraries and checks the
default service inside a private test root.

### Local account data

During assembly of a fresh image, select the greeter UID and GID against its
base account inventory. Preview and apply the staged sysusers recipe to that
root. Verify the result against the independently selected IDs before signing.
Keep an already enrolled greeter reserved across updates. [systemd-sysusers](https://www.freedesktop.org/software/systemd/man/259/systemd-sysusers.html)

```sh
systemd-sysusers --root="$IMAGE_ROOT" --dry-run - < "$IMAGE_ROOT/usr/lib/sysusers.d/aetheri-login.conf"
systemd-sysusers --root="$IMAGE_ROOT" - < "$IMAGE_ROOT/usr/lib/sysusers.d/aetheri-login.conf"
systemd-tmpfiles --root="$IMAGE_ROOT" --create - < "$IMAGE_ROOT/usr/lib/tmpfiles.d/aetheri-login.conf"
python tools/verify_login_accounts.py --root "$IMAGE_ROOT" \
  --greeter-uid "$LOGIN_UID" --greeter-gid "$LOGIN_GID"
```

The verifier checks the local NSS policy, the exact enrolled identity and
fully locked password and account, and rejects shared IDs and extra group
memberships. It checks the Root-owned empty home and pinned `nologin` shell.
For this recipe, `initgroups` sets the primary GID as the sole kernel
supplementary group. Pass that GID once as `--group` when preparing the
normal signed selection. The tool reports IDs and groups only.

### Signed login selection

The startup verifier and greeter need the same independently selected
deployment inputs. The selection can be signed after both executables are
built, so the greeter's own digest can be included. Build the eventual
providers with an explicitly selected Ed25519 public key and a nonzero,
unique 32-byte identifier for the selected image release. Keep the release
signing key on the signing workstation. The providers' trusted image
placement must establish the public key and release identifier before they
read a selection.

Prepare an unsigned record from an already reviewed staging receipt. Supply
the actual distribution-wide systemd service drop-in and a separately
reviewed snapshot of the effective kernel policy bytes as ordinary files.
Use the image's reserved greeter UID/GID and exact supplementary groups, one
`--group` argument per group in ascending order. With `--account-root`,
selection preparation verifies the offline account against those expectations.

```sh
python tools/prepare_login_selection.py --stage login-stage \
  --account-root "$IMAGE_ROOT" --group "$LOGIN_GID" \
  --release "$LOGIN_RELEASE_ID" --greeter-uid "$LOGIN_UID" --greeter-gid "$LOGIN_GID" \
  --systemd-dropin selected-timeout-abort.conf --effective-policy selected-policy.bin \
  --output login.body
```

On the signing workstation, review the selected inputs and sign the exact
`login.body` bytes with plain Ed25519. For the OpenSSL CLI, use
`openssl pkeyutl -sign -rawin -inkey login-release.pem -in login.body -out login.sig`.
Run the preparation command again with the same selection inputs, adding
`--signature login.sig` and setting the output to
`login-stage/root/usr/share/aetheri/login/selection.bin`. It rebuilds the body
and appends exactly 64 raw signature bytes. It creates the output once and
sets mode 0444. During image assembly use Root ownership and the supplied
SELinux file context. Signing and verification cover the final bytes
even if two preparation runs use different inputs.
[OpenSSL Ed25519 signing and verification](https://docs.openssl.org/3.5/man7/EVP_SIGNATURE-ED25519/)

The 971-byte version-2 record uses big-endian integers. It contains the
eight-byte magic `AETHLS02`, the 32-byte release identifier, the 32-bit UID
and GID, the one-byte supplementary group count, two zero reserved bytes,
and sixteen 32-bit group slots, padded with zeros. Each file row contains a
32-bit nonzero size and its 32-byte SHA-256 digest. The Ed25519 signature
covers the whole 907-byte body. File rows have this fixed order.

1. Backend unit, startup unit, distribution service drop-in, worker shutdown drop-in
2. Backend executable, backend configuration, greeter executable, greeter display executable
3. Greeter font, human PAM profile, greeter PAM profile
4. Session entry, compositor, desktop, startup executable, effective kernel policy
5. Settings, Files, Terminal, PTY starter, shell, editor

Use a new release identifier when producing the version-2 image and rebuild
both entry points against its selected anchor. The version-2 reader requires
all 22 records, so updating an older test image requires staging and signing
the complete selection.

Version 3 uses `AETHLS03` and appends one required, bounded row for
`aetheri-audio-desktop-control` after the editor. The signature covers
all 943 body bytes and the complete signed record is 1,007 bytes. The
format-3 staging receipt selects the matching encoder. A sound-enabled
image compiles the version-3 expectation into both login entry points.
The startup helper retains the mediator in its original executable
observations in both mount views. The common backend handoff continues
to contain ten sources and includes the exact authenticated selection
size and digest.

Version 4 uses `AETHLS04`, a 1,015-byte body and a 1,079-byte signed record.
It follows the same order through the editor, then carries Sound, the lock
dialog and the unlock PAM profile. The Sound row is 36 zero bytes when the
mediator is absent. The dialog and profile are required, and the profile is
limited to 65,536 bytes. Both login entry points require the format and Sound
presence selected at build time. The dialog uses the existing signed greeter
font at its fixed image path.

Version 5 uses `AETHLS05`, a 1,267-byte body and a 1,331-byte signed record.
It appends seven required rows after the unlock PAM profile, in this order.

1. Suspend guard ELF and guard service
2. Aetheri drop-in for `systemd-suspend.service`
3. Stock `sleep.target`, `suspend.target` and `systemd-suspend.service`
4. Stock `systemd-sleep` ELF

The startup helper holds a separate set of observations for these files in
each mount view. The staged guard ELF and sleep helper have mode 0555. The
two login entry points compile in the selection version and Sound presence.
The original backend handoff still contains ten source observations.

The native verifier checks the exact record size, release identifier,
signature, account IDs, group ordering, reserved fields, and the size bounds
of each role. A valid record supplies expectations for the subsequent live
checks. Reusing a release identifier would allow multiple selections signed
with that identifier. Boot/update policy still owns rollback between images.
Both login executables require an explicit deployment provider.

The [fixed-path reader](../src/login/installed.h) authenticates the record and
retains a protected source observation of those authenticated bytes. It
checks Root ownership, mode 0444, a single link, the exact SELinux file label,
and the original path and file identities. It projects shared expectations
for the backend, loaded configuration, ordered drop-ins and greeter, plus
the separate display executable. It reports the signed account and policy
expectations for the providers' live comparisons. When a source check
observes a replacement, later reads of that observation stay fenced.
[Linux path resolution](https://man7.org/linux/man-pages/man2/openat2.2.html)

The startup helper enters the original backend's mount namespace. Each
view needs its own source observation against the same signed release
selection. Source observations retain mount identities from their original
view.

### Startup executable

The startup executable can now select the installed adapter from its existing
build-time deployment source. Set `ae_login_run_profile.installed_release`
to the compiled public key and release identifier, and provide
`startup.check` and its context for the image's platform gate. The adapter
sets the fixed unit, selected source order and handoff location. The platform
gate still admits the protected installation, host namespace, account, PAM,
libraries and devices.

The adapter holds the first signed selection through the last original-view
check. The native startup coordinator enters the original backend's namespace
and verifies it before calling the one-time view hook. That hook opens a
second protected selection and requires the same authenticated record digest.
It keeps the second selection pinned during backend source capture, readiness
and publication.

The installed startup path observes eleven additional signed files in each
view. These include its unit fragment, its executable, the session executable,
the compositor, the desktop shell, and the six fixed application executables.
It checks the actual running startup
executable against the same held file object, including when it enters the
backend view. Subsequent checks revalidate the retained files in the current
view. An observed loss fences the attempt. The ten-entry backend handoff
fingerprint retains its existing form. SELinux grants the startup domain
read access to the selected desktop and app executable types while prohibiting
execution and mutation.

For a lock-enabled image, the startup helper also retains the lock dialog,
unlock PAM profile and shared font in each view. SELinux assigns the dialog
its own process type on exec. The dialog consumes the display and credential
sockets inherited from the desktop. It maps the compositor's received,
read-only Wayland keymap and writes its own typed pixel buffers. The original
privileged PAM worker reads the unlock profile and owns the authority socket
used by the desktop. The handoff still contains the same ten sources.

The adapter compares the signed policy size and SHA-256 with the actual
policy read from selinuxfs in each gate invocation. The loaded service checks,
future user-session transitions, actual DSO loading and image admission still
require their respective runtime proofs.

The [startup adapter verification](../brains/aetheri/journal/2026-11-17-login-installed-startup.md)
covers the native entry, real private mount views and effective policy checks.

### Greeter executable

The greeter also selects its installed adapter from its build-time provider.
Set `ae_login_greeter_profile.installed_release` to the compiled trust anchor
and supply the mandatory `backend.check` platform callback and its context.
The deployment can supply reviewed library and input data directories for
the display child.

The adapter derives its UID, GID, complete supplementary group set and
display image from the signed selection. Actual process IDs and groups
must agree before it enters the native greeter runner. The runner keeps
its existing original local session, peer, process and display checks.
The font comes from its original protected file at
`/usr/share/aetheri/login/greeter.ttf`. The resident font copy must match
the signed digest. The original selection and font file are checked on
subsequent host gate calls. An observed replacement fences the attempt.
The ordinary greeter continues to rely on the platform callback for live
policy and protected host admission.

Both installed adapters include `selection.bin` in the common source set
used by the protected handoff. That binds the handoff to the signed selection
observed in the backend mount view, including its release, account, policy
and asset rows.

The installed adapters use the `AETHLGH2` handoff profile. The startup helper
pins the enforcing kernel's status sequence and brackets its checks of the
signed policy digest with that original sequence. The ordinary greeter maps
the read-only status page and uses its own observed sequence to verify the
handoff. Policy reload or an enforcing-mode transition changes that sequence.
An existing attempt stays fenced after a change. The custom provider profile
can select the original handoff contract. The two formats are selected
explicitly and use separate context hash domains.
[Linux selinuxfs](https://raw.githubusercontent.com/torvalds/linux/master/security/selinux/selinuxfs.c)

The greeter has read-only access to the status page. The full loaded policy
read and digest comparison remain in the Root startup domain. The platform
gate still admits the protected boot and host kernel view, account, PAM,
runtime and devices. See the
[policy handoff verification](../brains/aetheri/journal/2026-11-17-login-policy-handoff.md).

See the [installed greeter check](../brains/aetheri/journal/2026-11-17-login-installed-greeter.md)
for the native entry and protected-font tests.

### Installed system

The stager copies the selected files and produces the receipt. Image assembly
must supply the linked libraries, PAM modules, full desktop runtime, protected
placement and ownership, and the verified distribution and Aetheri SELinux
policy. The normal compiled providers also need to bind the original loaded
unit, fixed source bytes, executable identities, enrolled account, session and
effective access rules. Select the final executable digests independently of
what the live greeter and backend report. The receipt is an inventory for that
work. The carrier-only policy supplement stays outside the package.

The [deployment verification record](../brains/aetheri/journal/2026-11-17-login-deployment-layout.md)
covers the isolated systemd layout and a separate full graphical VM test for
the scratch namespace. The [installed selection verification](../brains/aetheri/journal/2026-11-17-login-installed-selection.md)
covers the reader at the exact fixed path in a private mount namespace.
A login from the final fixed paths still requires the selected platform
gates and full image integration.
