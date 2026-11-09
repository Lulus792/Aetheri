# Private desktop build tools

The disposable audio carrier now has a separate CMake/C++ tool prefix for
rebuilding the current Aetheri SDK and apps. It does not install packages or
change the carrier's libraries. This is build preparation; the complete current
SDK, desktop and bootable image have not been rebuilt with it yet.

The [input profile](desktop-toolchain-inputs.json) selects nine signed Fedora
RPMs: CMake/data4.3.0, Ninja1.13.2, gcc-c++/libstdc++-devel/libstdc++16.2.1,
jsoncpp1.9.6, rhash1.4.5 and libuv1.52.1. CMake4.3.0 retains the existing
development baseline; this is not a claim that it is the latest patch release.
The last three libraries satisfy CMake's actual loader requirements. They are
developer-tool dependencies, not new Aetheri app/runtime dependencies.

The prefix contains2,607 original regular files (93,659,654 bytes), six additional
license files, four wrappers, one profile receipt and six explicit aliases.
CMake Modules/Templates and C++ headers account for most file names. CPack,
ccmake, manuals, editor integrations, static C++ libraries, multilib headers and
unneeded compiler helpers are omitted. No background service is added.

## Preparation and reuse

Run as an ordinary Linux build account with the already reviewed carrier
GCC16.2.1, binutils and glibc development environment:

```sh
python3 tools/test_desktop_toolchain.py
python3 tools/prepare_desktop_toolchain.py prepare FRESH_PREFIX \
  --cache REVIEWED_RPM_CACHE --notices REVIEWED_NOTICE_CACHE
python3 tools/prepare_desktop_toolchain.py check PREFIX
python3 tools/test_desktop_toolchain_native.py PREFIX FRESH_PROOF_DIRECTORY
```

The notice cache supplies the six exact `source` names in the profile. Five GCC
license/runtime-exception texts come from the independently checked same-source
libgcc16.2.1 inventory. The main CMake license comes from its exact upstream tag;
the selected signed RPM retains its bundled-component notices. External license
conditions remain separate from Aetheri's MIT license.

Preparation hashes each RPM into a sealed memfd. Signature, metadata, scriptlet
inspection and rpm2cpio share that same immutable descriptor. All selected bodies
and notices validate before the first output creation. Archive links are never
extracted. A selected zero-body hardlink obtains bytes only from the original
inode/device group with matching class/UID/GID/link count and expected digest;
each destination becomes an independent regular file. Only the listed soname
aliases and two development aliases are created.

Existing or partially created output is never repaired, adopted or deleted.
Preparation/check is developer tooling, not an untrusted package importer,
privileged installation operation, durable operation journal or security boundary.

Use `PREFIX/bin` in the build's PATH and `PREFIX/bin/c++` as CXX. The wrappers
select the private standard-library headers/runtime. They retain native GCC
collect2, startup objects and linker plugin through explicit original search
directories; LTO/plugin support is not disabled to make a probe pass. Compiler
arguments and native tool/SDK inputs still require review. This is not a hermetic
or cross-compilation environment.

The carrier's original libstdc++16.0.1 file and both existing sanitizer runtimes
are checked and remain unchanged. This preserves earlier instrumented fixtures.
The new16.2.1 runtime is private to these build processes. Do not update the
global package or export this prefix as a product runtime.

## Executed checks and remaining work

Ten archive/profile controls pass on Windows and in the enforcing Linux carrier.
Removing the hardlink-header comparison makes its precise counter test fail.
The native Release proof configures/builds with GCC16.2.1, CMake4.3.0 and Ninja,
then actually runs three tests: C17, C++23 vectors/exceptions/threads/filesystem
values, and a C executable loading a C++ shared adapter. Installed C/C++ probes
also run. Actual C++ maps select the private libstdc++. Seven loader listings
and seven original native helper hashes are recorded;51 distinct runtime ELF
files are rechecked during evidence export. These are compiler-tool checks,
not app, sandbox, performance or complete SDK acceptance.

The [execution record](../brains/aetheri/journal/2026-11-09-desktop-toolchain.md)
retains exact attempts, sources and the fully verified2,837-file export. Package
inventory, udev invocation and enforcing policy remain unchanged; no selected
tool/proof code remains mapped after the synchronous tests end.

Next assemble genuine pinned graphics/UI/system development headers and libraries,
rebuild the current SDK/apps, and run their normal native/GUI regressions. New
build paths can change ELF bytes and RPATHs; existing artifact pins must not be
silently reused. Complete-image ELF closure and normal device/service ownership
remain separate work. The user's first-version USB gate stays closed: no next
USB-targeted image build or flash follows from this tool proof.

Primary version/license references: [Fedora CMake4.3.0](https://packages.fedoraproject.org/pkgs/cmake/cmake/fedora-44.html),
[CMake4.3 release notes](https://cmake.org/cmake/help/v4.3/release/4.3.html),
[upstream license](https://raw.githubusercontent.com/Kitware/CMake/v4.3.0/LICENSE.rst),
[Fedora jsoncpp](https://packages.fedoraproject.org/pkgs/jsoncpp/jsoncpp/fedora-44.html),
[Fedora rhash](https://packages.fedoraproject.org/pkgs/rhash/rhash/fedora-44.html)
and [Fedora libuv](https://packages.fedoraproject.org/pkgs/libuv/libuv/).
Exact libuv1.52.1-2 signed package metadata/requirements are in the local review;
the Fedora44 release page still describes1.51.0. No exhaustive current security
clearance is inferred from the package pages or successful tests.
