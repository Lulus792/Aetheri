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
```

No root or device access is needed for this build. Record actual compiler/triple;
the checked execution uses the separate enforcing Fedora/glibc lab.

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
Next run the upstream Lua/input suites with controlled virtual devices, sanitizer
checks and actual compositor/input integration in a fresh disposable fixture.
Preserve original udev rules, devices, processes and unknown effects. Only after
those gates should a separately attributed runtime replace the old libinput/Lua
payload. Normal device/session admission and physical HP tests remain separate.
