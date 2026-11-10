# Graphical development image

This profile assembles the existing Aetheri compositor, desktop, Settings, Files,
Terminal, shell and editors into a separate boot image. It extends the
[read-only root](root-probe.md) and [persistent data](data-probe.md) proofs rather
than copying the development carrier. The test account starts automatically;
this is not the production login or app lifecycle service.

## Layout

| Location | Purpose |
| --- | --- |
| 768MiB GPT system image | UEFI fallback executable, kernel and read-only 512MiB ext4 root |
| Separate 256MiB ext4 image | Writable `/var`, with `home/` bound at `/home` |
| `/home/aetheri/Documents` | Ordinary documents owned by the fixed UID/GID1000 account |
| `/run/user/1000` | Normal systemd user runtime and user bus |
| `/run/user/1000/aetheri-XXXXXX` | Fresh desktop socket/cache directory; never recovered by name |
| `/tmp` | Bounded 64MiB volatile filesystem |
| `/run/log/journal` | Volatile diagnostic journal, limited to 16MiB |
| `/usr/libexec/aetheri` | Fixed development application executables |
| `/usr/lib/aetheri/runtime` | Relocated, pinned SDK runtime libraries |

The old data counter service and immediate diagnostic poweroff are absent from
this profile. User documents do not become prerequisites for boot. The small
data volume is a test fixture, not the proposed capacity for an everyday system.
The 1GiB combined virtual capacity fits the offered USB medium; that does not
establish a final USB layout or physical-device compatibility.

## Startup responsibilities

The existing native root and data checks run before writable mounts. Selected
udev rules and module metadata supply the virtual display/device path. Stock
systemd-logind, the system bus, user manager and user bus keep their separate
roles. The system broker retains its vendor device sandbox and writes to the
journal instead of trying to open a console inside its private `/dev`.

A fixed root oneshot selects VT8 with CAP_SYS_TTY_CONFIG in its capability bound.
Its unit configures a `/dev/tty0` device allowlist. The first image lacked libbpf
and logged disabled cgroup BPF features; its earlier configured filter has no
retroactive enforcement acceptance. The subsequent
[device-filter profile](device-filter.md) includes the reviewed backend and
tests actual baseline/denied block opens in separate cap-zero oneshots before
the VT service. Two new cold boots exercise that path; see the
[record](../brains/aetheri/journal/2026-11-08-device-filter.md). The USB desktop
must retain and repeat it. GUI capability/NNP and session evidence remain separate.
The graphical service starts through its explicit development
PAM profile on that console. Its UID/GID1000 process requires NoNewPrivileges,
zero effective capabilities and an active local seat0 Wayland session. Dedicated
PAM files configure these root-selected units only; the fallback PAM profile
denies access. This does not implement password authentication or a login screen.

The original C17 desktop owner holds actual pidfds for its direct children.
A child waits behind a one-way start barrier until its parent has captured that
pin. Failed capture cannot execute the target. Private inherited Wayland and
bounded launch sockets connect the compositor, desktop and fixed app roles.
Unexpected descriptor transfers are closed and rejected. Shutdown retains the
original child observations and drains their output; it does not settle any
production catalogue run, lease or delegated holder.

Current source also retains and rechecks the original active session through the
[native self-session observer](session.md#native-desktop-owner-and-original-session-observation).
The existing event loop fences new starts after observed session loss and keeps
the Files-first document-close/shutdown protocol. Release and memory builds and
a separate protected-runtime KMS fixture test this source; the previously built
USB image does not acquire these changes retroactively. No new USB-targeted
build or flash is authorized until the agreed first-version gate is satisfied.

Desktop readiness requires compositor readiness, the desktop's first layout,
the still-active original session and a successful systemd notification. The
normal compositor active-seat checks remain in force. Software rendering is
selected for this virtual-display proof; physical GPU acceptance remains separate.

## Build and input inventory

`tools/build_desktop_probe.py` runs as an ordinary x86-64 Linux user. It compiles
the application targets and desktop owner, populates a new image tree and formats
only new regular image files. It does not install packages, mount the image or
modify carrier services. The builder requires the explicitly selected existing
wlroots, SDL/text and NexaUI SDK prefixes; it is not a standalone SDK bootstrap.

`spec/desktop-image-inputs.json` records individual source/destination paths,
sizes, modes, SHA-256, RPM ownership or SDK provenance, relative library aliases
and selected licence files. `tools/desktop_payload.py` bounds and validates the
inventory before copying, refuses conflicting existing objects and preserves a
failed partial copy. Normalizing the old builder's flat library copies requires
independent old and new pins; matching names alone cannot authorize replacement.

This development inventory is not publisher trust, a current security clearance,
or approval to redistribute a complete production distribution. Preserve
third-party licences and complete the applicable source-distribution obligations
before publishing binary releases. Original Aetheri code remains MIT.

The selected closure includes the existing C.UTF-8 glibc locale and selected
XKB/Compose data; libraries alone cannot provide the terminal's required locale.
Eighteen existing coreutils commands make the development shell useful for
ordinary file tasks without adding a resident service. This proof's largest
runtime inputs are Mesa/LLVM for software rendering; the 512MiB root leaves
room for that closure instead of silently overflowing the old 256MiB partition.

## Explicit VM test

`tools/test_desktop_image.py` accepts this profile only. Every sequence owns a
new data copy, fresh firmware variables and the original QEMU child handle.
Control uses host-local AF_UNIX QMP inside a private directory. The same
connection checks the expected VM UUID before each command. No network, shared
folder, physical disk or direct kernel/initramfs injection is supplied.

Each input is recorded pending before dispatch and is sent once. An unknown
reply or failed boot preserves the sequence and prevents an automatic retry.
A successful sequence must use the real virtual keyboard to launch the fixed
apps, write and reread an ordinary document across two cold boots, close the
desktop and complete normal guest poweroff with the data filesystem unmounted.
PNG captures and independent read-only filesystem inspection supply separate
visual and persistence evidence. A screenshot alone is not app or storage
acceptance. See STATE and the executed journal for the actual passing boundary.

Native desktop-owner tests exercise descriptor rejection/closure, failed pidfd
capture without target execution and unrelated-FD isolation. Runtime payload and
controller tests check their separate bounds. These do not establish production
app admission, document portals, SELinux isolation, recovery or removal.

On the prepared Linux builder, run the Python boundary tests with
`python3 tools/test_desktop_payload.py` and
`python3 tools/test_desktop_image_contract.py`. Compile the native owner test into
a fresh private output directory and run it as the fixed UID1000 test account:

```sh
cc -std=c17 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Wpedantic \
  -Wconversion -Wshadow -Werror tests/boot_desktop_test.c \
  -lsystemd -o /path/to/private-output/desktop-test
/path/to/private-output/desktop-test
```

The same test also runs with `-fsanitize=address,undefined
-fno-omit-frame-pointer`. Image construction uses
`python3 tools/build_desktop_probe.py --boot-image ORIGINAL_RAM_INPUT
--packages VERIFIED_UKI_RPMS --output NEW_PRIVATE_OUTPUT`. The Windows VM test is
`python tools/test_desktop_image.py --image EXPORTED_IMAGE --output NEW_SEQUENCE`.
These paths are explicit development inputs, not device or installation targets.

## Remaining work

The image has no production MAC policy, normal authority/SDK provisioning,
installer, LUKS unlock, signed boot/update/rollback path or browser. Laptop USB
root drivers, firmware, physical display/input, wireless/audio/Bluetooth and
suspend require their own evidence. Fixed development PAM and loader paths must
not be promoted to production authentication or app admission. No physical USB
write or formatting is part of the build or VM test.
