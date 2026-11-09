# Private audio Lua candidate

The audio rollout needs a maintained Lua runtime for WirePlumber's trusted policy
scripts. Lua 5.4 is at its final release. The candidate here is Lua 5.5.1 with four
subsequent upstream fixes and a distinct 5.5 ABI. The existing tested audio
and image profiles still use their original library. No normal service is changed.

[audio-lua-inputs.json](audio-lua-inputs.json) pins the official source tag,
archive bytes and complete pre/postimages for the six patched C files. The fixes
come from the linked upstream commits and retain Lua's MIT copyright/permission
notice. `prefix/usr/share/licenses/lua/COPYRIGHT` contains the original notice
from lua.h. The published patch also retains its adjacent
[Lua copyright notice](../patches/lua-5.5.1-COPYRIGHT). Aetheri's own C17 code and
test inputs remain separately MIT.

The builder executes explicit C17 compiler/linker commands for 32 library sources;
it does not run the source makefile, fetch dependencies or install on the host.
It rejects a changed archive, unsafe members and changed patch inputs. Output
must be new; failure preserves partial output. The SDK includes one shared
library, four public headers, pkg-config data and the original notice. A separate
CLI stays outside the runtime prefix for tests. There is no new daemon or policy
database.

Lua remains an internal dependency of the maintained policy manager. This does
not replace Aetheri's original shell/language or add an app scripting permission.

As an ordinary x86-64 Linux build user with the reviewed compiler already present:

```sh
python3 tools/build_audio_lua.py REVIEWED_ARCHIVE FRESH_BUILD
python3 tools/test_audio_lua_inputs.py
for case in nan zero close gc; do
  FRESH_BUILD/lua-test tests/audio_lua.lua "$case"
done
```

Use a separate fresh output and `--sanitizers` for ASan/UBSan. Record the actual
compiler/triple; a musl SDK is not an Aetheri glibc artifact. The three archive
controls run without a compiler, bus or installation. The Lua tests detect NaN
loop execution, negative-zero changes, coroutine-close errors and the GC signed
shift. Passing them is bounded source-regression evidence, not exhaustive Lua
security, performance, app-script admission or complete audio acceptance.

The [execution record](../brains/aetheri/journal/2026-11-09-audio-lua.md) preserves
actual Release/sanitizer positives and original-source negative controls on both
Alpine/musl and the separate enforcing Fedora/glibc lab. The latter uses the
independently reviewed [build-only tools](audio-build-tools.json); none belong to
the future audio runtime simply because a compiler needs them.

## Before rollout

The actual WirePlumber 0.5.19 meson source already supports Lua 5.5. Its currently
selected Fedora Lua-scripting module is linked to 5.4. The separate
[module candidate](audio-policy-lua.md) now builds against the new SDK and passes
all eleven upstream model tests with the original core. Recheck every resulting
ELF dependency, preserve complete notices
and exact source/runtime inventories, then repeat actual graph, PCM, GUI,
enforcing policy, source-loss and original-resource retirement tests. Only then
select a new audio/image profile. Keep the old reviewed prefixes and historical
receipts intact. Normal session/service/device ownership and the separate PCRE2
release review are still open.
