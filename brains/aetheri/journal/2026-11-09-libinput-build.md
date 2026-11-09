# libinput 1.32 / Lua 5.5 build — 9 November 2026

## Decision and source evidence

The previous GUI loader check identifies the retained Lua 5.4 dependency in
libinput 1.31.3. Review the [build contract](../../../spec/libinput-build.md).
Lua implements optional input-event filters, not the ordinary input processing
core. The selected wlroots 0.20.2 source contains no `libinput_plugin_*` call;
both libinput versions default autoload off. Nevertheless, keep plugin, mtdev
and tablet support while reviewing an upgrade rather than dropping functionality
solely to remove a library.

The current listed stable upstream tag is 1.32.0, dated 17 September 2026. Compare
both official 1.31.3 and 1.32.0 archives as data. Both use the generic Lua dependency
lookup with version>=5.4, while the official plugin guide names 5.4. Choose a
private 1.32.0/Lua 5.5 candidate and require actual compatibility tests. No source
or soname is relabelled as Lua 5.4 and no normal provider is replaced.

Official archive SHA256:
`7dd6c1ca964c86eb6810ccd6639cda634e68bdca43bf3afec39e94e942fac8d4`,
1,193,963 bytes, 525 regular source files. Tag commit
`5cf833b572f5a33663888da321dce0ba1abd82a2`. Independent reconstruction of every
Git blob/tree matches upstream tag tree
`43b027c997068e990f016c49a4c802c1f8b5de13`. The official archive downloads succeed
even though several browser GitLab release/raw pages are unavailable. A Gitiles
mirror supplies the independent tag metadata; no unverified mirror archive is used.

Primary references: [plugin documentation](https://wayland.freedesktop.org/libinput/doc/latest/lua-plugins.html),
[upstream tag source](https://gitlab.freedesktop.org/libinput/libinput/-/tree/1.32.0),
[tag mirror](https://chromium.googlesource.com/external/gitlab.freedesktop.org/libinput/libinput/+/refs/tags/upstream/1.32.0),
[Gentoo's dated advisory](https://security.gentoo.org/glsa/202608-08).
The advisory's fixed threshold includes the old 1.31.3 baseline; this is not a
claim that it lacks those fixes or an exhaustive current security clearance.

## Private SDK and actual build

The separate registered audio lab remains UUID
`033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU 43164/controller 10491,
boot `32baadd1-a444-4aa8-8c32-56eb3ce22c21`, enforcing with restricted networking.
The original P0 carrier is stopped and untouched. Existing reviewed GCC 16.2.1
and glibc 2.43 toolchain is reused; no system package is installed.

Eleven Fedora-signed RPM snapshots provide only build headers, PC metadata,
Meson and Ninja. Complete source/license metadata, dependencies, empty script
lists and full signer/payload proofs are retained. Sysprof capture metadata is
needed by GLib's private PC requirements, not a new runtime profiler. The private
SDK copies independently checked existing library providers and the earlier
patched glibc Lua 5.5 SDK. Old libinput/Lua 5.4 cannot shadow the new tests there.

Fresh root-controlled mount namespaces/tmpfs run compiler/tests as UID/GID1000,
empty groups and NNP. All subprocesses are waited for before complete protected
exports; namespace exit retires scratch mounts. This is build scratch, not a
durable package publication or app recovery protocol.

Local evidence is `local/libinput-review-2026-11-09`:

- v1 correctly finds Lua 5.5 but cannot resolve GLib's sysprof-capture PC entry.
  Its failed output is retained; a separately signed SDK input snapshot adds only
  the missing build metadata/headers.
- v2 configures but compile finds `/usr` headers because pkgconf did not relocate
  the RPM prefixes by default. v3 adds explicit `--define-prefix` and rejects
  ambient/unrelocated include flags. No header dependency or tablet feature is
  disabled to make the build pass.
- v3 completes the full configured build, generic deviceless and utility/version
  tests and private `DESTDIR` staging. It remains separate historical evidence.
- v4 additionally tests matching-version bytecode admission and its real negative
  source counterexample. Original host controller 44928 exits0. Protected output:
  `/root/aetheri-libinput-build-3465b16f0db6481da0ae98a8a0958691`.

The v4 complete 2,465-file export SHA256 is
`3d61b64d9c4e4f122b3f70ff18230483b71c0a247022077793302c28641672b0`.
Every file length/hash and submitted public source/profile matches independently.
The final verifier streams the compressed archive once; repeated backward gzip
lookups are unnecessarily slow. Initial postprocessors also incorrectly expect
an explicit `#undef AUTOLOAD_PLUGINS`; Meson omits an unset key. Correct checks
observe no define and the actual introspected option false. No native result is
changed to repair those verification mistakes.

Candidate library: 637,664 bytes, SHA256
`38dc7da06bb33703b2843ca67731c0485e562a2e1aac6463d55489069f730b07`.
It links Lua 5.5, has no RPATH/RUNPATH, and retains all 309 versioned exported API
symbols from the selected 1.31.3 ELF. Full ABI and input behavior remain unproven.

## Executed tests and important limit

The verbose generic deviceless runner reports 44 pass, 0 skip/fail. Utilities
report 64 pass, 0 skip/fail, and the library-version test passes. Five host archive
controls reject changed bytes, traversal, an unexpected link and a wrong root.
These do not include upstream's full Lua suite: its `no_device` registration is
different from `deviceless` and may create uinput devices. Do not turn the generic
Meson success into a claim of complete plugin or physical-device acceptance.

The own C17 probe loads text containing an actual `_VERSION == 'Lua 5.5'` check,
logs its acceptance and observes zero device opens. Matching Lua 5.5 bytecode is
generated by the selected runtime and rejected by the plugin loader's text-only
mode. The unmodified upstream negative vector is precompiled 5.4, so rejection
of that vector alone could merely reflect a bytecode-version mismatch.

A separate copied source changes exactly `luaL_loadfilex(..., "t")` to unrestricted
mode and rebuilds its own library. The same probe now reports
`MATCHING_BYTECODE_ACCEPTED` and exits 1. That counter library is test-only, never
staged/installed or selected as the candidate. Both outcomes are independently
checked in the export. This establishes one guard, not complete sandbox safety.

## Next

Run the full upstream Lua/input cases in a new root-only disposable virtual-device
fixture with protected udev/rule/device/process lifetimes and no normal input
admission. Add sanitizer and actual compositor/input regressions before replacing
the old runtime library/notice or claiming a maintained whole-desktop Lua closure.
The PCRE2 review, normal service/session/device ownership, physical hardware and
the remainder of the OS/app/installer/experiment goal stay open.

Preserve all failed archives and exact completed receipts. The lab remains live
under its original controller for the next tests. Normal manifests, original
carrier, USB user documents and foreign planning/UI files remain untouched.
NexaUI handoff stays closed; the full goal remains active.

Independent same-boot inspection finds no executable/working directory under any
of the four build scratch roots, and enforcing remains1. All four foreign file
hashes match the initial snapshot. This is administrative build-process evidence,
not production app/descendant/cgroup/lease settlement.
