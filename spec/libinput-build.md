# Private libinput candidate

The candidate builds libinput 1.32.0 against the reviewed Lua 5.5.1 SDK, keeping
mtdev, libwacom and optional Lua plugin support. Plugin autoload stays off.
Normal graphics, input, audio and image manifests remain unchanged.

[libinput-build-inputs.json](libinput-build-inputs.json) pins the official source
archive/tag and eleven signed build-input RPMs. The source archive's complete
Git tree also matches the independently retrieved upstream tag tree. These are
source/provenance checks, not publisher authentication from a checksum alone.
The [execution record](../brains/aetheri/journal/2026-11-09-libinput-build.md)
contains the source comparison, exact native result and remaining acceptance.

## Build inputs and scope

The ordinary-user builder checks complete existing GUI and Lua SDK snapshots,
then verifies all RPM hashes, sealed-snapshot signatures, source/license metadata
and absence of scriptlets before output creation. It extracts only selected
headers, pkg-config data, Ninja and Meson's Python sources. It installs no RPM,
changes no host udev files and performs no network request. Partial output is
retained and never adopted on retry.

The private SDK uses existing checked library providers. It excludes both old
libinput and Lua 5.4 to prevent accidental test shadowing. The generic `lua.pc`
advertises the actual 5.5.1 version and `liblua-5.5.so`; it never impersonates a
5.4 package or soname. libinput's unchanged Meson lookup accepts generic Lua
versions >=5.4. Official plugin documentation still names Lua 5.4, so this build
is an explicit compatibility candidate rather than a claim of upstream 5.5 support.

RPM `.pc` files describe `/usr`. A scoped wrapper asks pkgconf to `--define-prefix`
and rejects unrelocated header paths. GLib's required sysprof-capture headers/PC
metadata are build-only; no profiler library, agent or daemon is installed.
The existing PCRE2 provider is reused, without clearing its separate release review.

The builder uses upstream Meson, GNU C17, generic compiler targeting, stack/FORTIFY
protection and RELRO/NOW. It compiles the complete configured source and stages
installation under its own `DESTDIR`. The candidate shared object has soname
`libinput.so.10`, filename `libinput.so.10.13.0`, Lua 5.5 linkage and no RPATH/RUNPATH.
The original MIT notice and source data remain available with the build inputs.
Counterexample libraries and test executables must never enter a runtime projection.

```sh
python3 tools/test_libinput_inputs.py REVIEWED_SOURCE_ARCHIVE
python3 tools/build_libinput.py \
  REVIEWED_SOURCE_ARCHIVE REVIEWED_RPM_CACHE ORIGINAL_GUI_RUNTIME \
  REVIEWED_LUA55_SDK FRESH_BUILD
python3 tools/test_libinput_device_report.py
```

No root or device access is needed for this build. Record actual compiler/triple;
the checked execution uses the separate enforcing Fedora/glibc lab.

## Explicit virtual-device fixture

`tools/test_libinput_devices.py` is an administrative test for the separate audio
lab only. It refuses the original P0 carrier and requires the selected original
boot, a fresh protected output and a complete caller-selected file/hash inventory
of a completed root-owned, non-writable build. Links must resolve inside that
build. It never installs the library, enables networking or changes SELinux policy.

The upstream device runner must use `--build-type debugoptimized`. In 1.32.0,
`builddir_lookup()` deliberately returns false for Release, causing that runner
to assume installed rules/quirks under `/usr`. Generic deviceless tests do not
expose this prerequisite. Do not populate the host's `/usr` to accommodate it.
Instead build a fresh debugoptimized runner with the ordinary-user builder. Keep
the independently selected Release library in a separate `release` directory
inside its sealed input tree, with its original soname links. This is reuse of a
verified code artifact, not restoration of an old live runtime or its handles.

The protected selection JSON contains `boot`, `root`, `files` (relative regular
names mapped to `size`/`sha256`) and `library_directory: "release"`. It binds the
Release artifact independently of the debugoptimized build's own library. The
test environment selects the Release directory and private SDK; actual process
maps must show that library and the SDK's real Lua 5.5 file. The upstream runner
and udev helpers retain their build-directory profile. Their debug build is not
a replacement production input library.

```sh
python3 tools/test_libinput_devices.py \
  PROTECTED_SELECTION_JSON FRESH_PROTECTED_TEST_OUTPUT --group lua
```

Run one group at a time. The fixture records the original udev manager, input
objects/nodes, rules and module state before effects. It uses standard existing
SELinux types on its own helper/library/log files, root-only PID1 FD delivery,
one non-restarting service and `--jobs=1`; no TTY is supplied. A lost reply keeps
the pending intent and original controller/handles alive, without retry or cleanup.
In particular PID1 must be allowed to receive the log FD: an `admin_home_t` log
is rejected; the fixture labels its fresh log `var_log_t` before delivery.

After observed main-process end, require the original test cgroup empty or
already removed through its held event FD, then retire the same referenced unit
and verify GC. Settle udev, compare complete baseline input/rule/quirk state and
reject remaining helper/library mappings. Only an original, unused module loaded
by this attempt may be unloaded. Remove only its own held, unchanged and empty
rules directory. Unknown outcomes and aborted-test artifacts remain for inspection.
This is a disposable administrative fixture, not production device/session
admission or durable recovery.

Reports retain upstream pass, not-applicable, fail and skip counts separately.
`upstream_passed` requires a consistent positive summary and native exit0;
`coverage_complete` additionally requires every selected case to pass. The host
report controls reject missing/inconsistent summaries and native failures. A
successful upstream exit never turns skipped or inapplicable cases into passes.

### Complete device data

Touchpad tests require a separately selected `wacom` input (`root`, `files`),
checked like the code tree under a fresh `/run/a-wacom-HEX` directory. The helper
`prepare_libinput_data.load()` checks the already pinned libwacom-data 2.19 RPM's
sealed signature/payload, exact origin and absence of scripts. It materializes
all 829 independently pinned database/layout files from the existing desktop
profile: 1,174,091 bytes, retaining HPND attribution. Five newc hardlink entries
carry their bytes in another member of the same RPM-header inode/device class.
Resolve that exact class and still require each destination's independent size/
digest, then create separate regular copies. Never create filesystem links or
infer a hardlink class from matching bytes alone. Pure controls cover those cases.

The selected library's [upstream constructor](https://raw.githubusercontent.com/linuxwacom/libwacom/libwacom-2.19.0/libwacom/libwacom-database.c)
checks `XDG_CONFIG_HOME/libwacom` before its system directories. The fixture sets
that environment only for its own service, to the sealed data tree's `usr/share`.
It refuses unselected `/etc/libwacom` or `/usr/share/libwacom` fallback data and
checks the data tree again at end. No global data directory or package is installed.
`--test LITERAL_NAME` permits a focused upstream regression before repeating the
full group; it never changes the selected library. The prior missing-data failure
and its original result remain evidence, not a library-source defect to patch.

### Separate memory instrumentation

The ordinary builder also accepts `--build-type debugoptimized --sanitizers` in
a fresh output. Its separate Lua SDK is the previously checked patched 5.5 ASan/
UBSan artifact; Release pins remain strict. Existing carrier sanitizer runtimes
and their libstdc++ dependency are version/hash/RPM-verified before and after the
build. They are compiler tooling, not new production dependencies or app code.

Both C libraries must actually reference ASan/UBSan instrumentation. Positive
controls initialize and release actual libinput/Lua contexts and record their
process maps, including the selected sanitizer runtimes. Two test-only controls
must fail precisely for heap-buffer overflow and signed overflow. Leak checking
and halt-on-error stay enabled; no suppression is added. Test controls and
instrumented libraries never enter a normal runtime projection. Executed evidence
and remaining coverage belong in the device record; build options alone prove
neither memory safety nor complete integration.

The instrumented test runner uses one pinned test-only patch: linker-provided
registration sections are declared as unknown-bound arrays rather than single
objects. GCC's object-size check otherwise stops registration before any case
runs. The patch checks complete pre/postimages and leaves library sources and
all sanitizer checks enabled. The address control loads its pointer through
volatile storage so ASan can demonstrate its heap check separately from UBSan's
earlier object-size diagnostic. Neither change hides a library error.

## Tests and next acceptance

The upstream generic deviceless run executes44 cases, utility run 64 cases, and
library-version check passes. These are not the full Lua plugin suite: upstream's
Lua tests registered as `no_device` may create virtual devices and are excluded
by the narrower `--filter-deviceless` selection.

The separate C17 probe accepts a real text plugin reporting Lua 5.5 and observes
zero device-open calls. It creates matching Lua 5.5 bytecode and requires text-only
rejection. This matters because upstream's embedded bytecode vector is Lua 5.4:
version mismatch alone could otherwise make a missing guard appear correct.
A separate deliberately weakened source/build accepts matching bytecode and
fails the same probe precisely. This is one focused guard test, not complete
Lua sandbox or hardware acceptance.

The new library retains all 309 versioned exported API symbols of the selected
1.31.3 library. That observation does not prove all ABI/layout or input behavior.
The separate [device record](../brains/aetheri/journal/2026-11-09-libinput-devices.md)
tracks executed upstream Lua/input cases against the selected Release artifact.
Sanitizer checks and actual compositor/input integration remain separate gates.
Preserve original udev rules, devices, processes and unknown effects. Only after
those gates should a separately attributed runtime replace the old libinput/Lua
payload. Normal device/session admission and physical HP tests remain separate.
