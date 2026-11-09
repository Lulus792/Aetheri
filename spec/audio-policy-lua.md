# WirePlumber Lua module candidate

The private candidate rebuilds WirePlumber 0.5.19's Lua scripting module against
the [reviewed Lua 5.5.1 SDK](audio-lua.md). Its daemon, core library, other modules
and policy scripts remain the existing RPM bytes. This is a compatibility build;
normal audio and image profiles still select the original runtime.

[audio-policy-lua-inputs.json](audio-policy-lua-inputs.json) binds the official
source archive, six selected Lua SDK files and the resource compiler. Existing
SDK/control/policy validators check the complete original prefixes before and
after the build. New output is create-only; failed work is retained.

The explicit C recipe follows `modules/meson.build` and
`modules/module-lua-scripting/wplua/meson.build`: eleven original C files plus
two generated resources. It builds neither a replacement core nor the whole
WirePlumber distribution. The original resource XML and `glib-compile-resources`
generate both C resources, with manual registration for wplua as upstream specifies.
No source patch or new system package installation is involved.

This deliberately uses a narrow compiler recipe instead of running the full
upstream Meson build. The latter also builds the core and needs additional GLib
development-package dependencies unrelated to this module. The reviewed signed
GLib development RPM supplies the 48,640-byte resource compiler as data; its
package scripts are not run. That tool is build-only. The recipe retains generic
x86-64 code, GNU C17, PIC, stack protection, FORTIFY, RELRO/NOW and undefined-symbol
rejection. It emits no RPATH or RUNPATH. No Meson/Ninja runtime is added.

With independently prepared, verified inputs, as an ordinary Linux build user:

```sh
python3 tools/test_audio_policy_lua_inputs.py REVIEWED_WIREPLUMBER_ARCHIVE
python3 tools/build_audio_policy_lua.py \
  REVIEWED_WIREPLUMBER_ARCHIVE ORIGINAL_PIPEWIRE_SDK ORIGINAL_CONTROL_SDK \
  ORIGINAL_POLICY_PREFIX REVIEWED_LUA55_SDK REVIEWED_RESOURCE_COMPILER FRESH_OUTPUT
```

`FRESH_OUTPUT/libwireplumber-module-lua-scripting.so` is the only runtime candidate
produced. Objects, the full original source including its MIT notice, generated
resources, compiler commands, test executable and logs remain build evidence.
Do not distribute that module alone without its original attribution. A future
runtime projection must retain WirePlumber and Lua notices and identify rebuilt
bytes separately from unchanged RPM payloads.

The builder runs all eleven upstream wplua model tests against these same objects,
new Lua and original WirePlumber core. It checks ELF linkage to Lua 5.5, rejects
5.4/path overrides, and checks actual dependency resolution and unresolved symbols.
The host-only archive controls reject changed bytes, traversal and unreviewed
links. Three exact upstream aliases are omitted rather than extracted or followed.
Upstream agent instruction files are source data, not build instructions.

The [native record](../brains/aetheri/journal/2026-11-09-audio-policy-lua.md)
contains the actual Fedora build, eleven passed model cases and complete checked
export. The separately identified [runtime candidate](audio-policy-candidate.md)
now also has actual graph/PCM/native-control evidence. GUI, confined policy and
physical-device acceptance remain open. Repeat those tests before changing normal
manifests. Do not make the old
RPM-only prefix validator accept a rebuilt module under its old provenance.
