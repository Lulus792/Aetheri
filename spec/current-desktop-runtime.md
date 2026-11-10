# Current desktop development runtime

The [current Release build](current-desktop-build.md) now has a finite runtime
inventory and a fresh-copy preparer. It supports explicit tests on the existing
development carrier. Normal installed app/session admission and a new OS image
remain separate work.

## Contents and footprint

[current-desktop-runtime-inputs.json](current-desktop-runtime-inputs.json) binds
200 regular files, 47 direct aliases and 49 attributed components. The regular
payload is 244,711,789 bytes, about 233.4 MiB. It contains:

- The nine current original programs: compositor, desktop, own Settings/Files/
  Terminal views, PTY starter, shell, modal editor and graphical text editor.
- One explicitly test-only Wayland client, for actual display/input evidence.
- The selected shared-library bodies, original source/notice material, three
  fonts and 54 matching libinput quirks. No compiler, development headers, static
  libraries, debug build, package scripts or new background daemon is included.

LLVM accounts for 147,082,848 bytes and Mesa Gallium for 55,510,872 bytes. They
provide the reviewed software graphics path; their presence is not evidence of
hardware acceleration. The remaining payload is 42,118,069 bytes. These are disk
sizes, not a measurement of resident RAM or the future full system footprint.
Mesa's actual X11-related transitive libraries remain attributed even though the
apps select Wayland; deleting them does not establish a valid smaller closure.

The profile derives from the independently verified SDK/app artifacts and actual
loader/process-map evidence. Each file identifies its provider, relative input,
literal destination, body hash, mode, role and component. Notice paths belong to
that same component. Aetheri's MIT code retains separate Unicode, Compose and
generated-protocol notices. The libglvnd source RPM is retained as original
source/notice data; its archive/spec is not executed. The ncurses notice has
installed-carrier RPM-header/body evidence, explicitly distinct from a newly
verified package signature. Exhaustive redistribution/source and security review
remain release work, including the existing PCRE2 review.

## Preparing and checking

Supply an independently selected JSON map of the exact required provider keys to
canonical source directories. Use a fresh output:

```sh
python3 tools/prepare_current_desktop_runtime.py prepare FRESH_RUNTIME \
  --sources REVIEWED_PROVIDER_MAP.json
python3 tools/prepare_current_desktop_runtime.py check FRESH_RUNTIME
```

Ordinary developer preparation stays ordinary-user. For the explicit disposable
carrier, a Root caller may select a new destination under protected parents and
add `--protected` to prepare/check. It must use a fresh Root-created copy. Merely
changing an existing user's file ownership cannot retire their held writers and
is not an admitted preparation.

The preparer opens all selected inputs with no-follow/nonblocking flags, rejects
nonregular/multiple-link/size conflicts, checks their complete bodies and retains
those FDs before creating output. Copying rechecks the consumed bytes. Existing
outputs reject; failures preserve partial trees without retry, repair or cleanup.
The checker rejects altered bodies/modes/aliases, missing notices and unexplained
entries, including empty directories. `runtime.json` is a consistency record;
it cannot authorize another profile or prove the original creation operation.
This is not a durable install/publication/recovery protocol or volume identity.

A protected check also requires Root ownership and no group/other write access
throughout the tree and its ancestors. Ordinary Linux symlinks have mode 0777;
their exact owners and closed targets are checked instead of treating those mode
bits as target write permission. The old all-regular Sound runtime keeps its
existing stricter shape and protection checks.

The lab applies the existing `bin_t`, `lib_t` and `usr_t` labels only to the newly
created files. It adds no MAC policy or permission. Original package/runtime,
normal build pins and user-owned producers remain unchanged.

## Actual display/input boundary

The explicit `current-desktop` mode in
`test_compositor_libinput.py` selects the protected profile and exact flat
library/quirk paths. It preserves the existing finite PAM/logind fixture,
original pidfds, service/scope invocations and end observations. Unknown dispatch
still retains the original keeper and handles; it does not authorize replay.

Loader inspection resolves all ten programs against the protected copy. Actual
DRM runs separately execute the current compositor and test client as UID1000,
with zero capabilities and NoNewPrivileges. Filesystem executable mappings must
belong to the selected runtime/program inventory, with one independently matched
carrier ELF-interpreter exception. Unselected native-library fallback and deleted
code mappings reject. This does not cover every anonymous/JIT mapping.

The existing ELF bytes and their original RUNPATH strings are preserved. A loader
listing is not proof of every future `dlopen` or app startup. The actual
compositor evidence below verifies its in-effect selected code. The carrier still
provides the kernel, PID1/logind/PAM, input rules, checked Wacom data and system
keymap/monitor metadata; this directory is not a complete isolated OS root.

Two current runs now pass actual bochs-drm 1280×800 output, PS/2 key and mouse
delivery, original process/session/unit/cgroup end and subsequent card
reacquisition. Each compositor has 25 checked executable-file mappings. Original
tty1 and input/udev baselines return. See the
[execution record](../brains/aetheri/journal/2026-11-10-current-runtime.md).

Next integrate the protected inventory with the existing full desktop/app owner
and repeat real graphical app workflows in that session. The headless current-app
tests and this KMS/input proof have different scopes. Normal admission, trusted
lock/resume, physical laptop/monitor acceptance and the agreed first-version USB
gate remain open.
