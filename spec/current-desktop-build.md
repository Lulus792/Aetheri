# Building the current private desktop

`tools/build_desktop_apps.py` builds the compositor, desktop and original apps
together against the separately reviewed SDKs. It is an ordinary-user Linux
development build, not an image builder, installer or app-admission service.

## Inputs

The [app SDK profile](app-sdk-inputs.json) selects original development files
from eight signed Fedora packages: OpenSSL 3.5.9 Crypto, SQLite 3.51.2 and
libselinux 3.11. The projection has 162 regular inputs, 10,391,401 bytes and five
aliases. Original notices are retained. The shared SDK preparer changes only
pkg-config path assignments. It does not install packages or run their scripts.
Use `prepare_desktop_sdk.py prepare FRESH_SDK --cache CACHE --profile apps`,
then the matching `check` command.

Only libcrypto is selected from OpenSSL; libssl is not part of this SDK.
libselinux's original `Requires.private` entries need the original libsepol and
PCRE2 pkg-config metadata. Those two packages provide metadata only. This is not
a static-linking SDK, a new libsepol runtime or a policy installation. The
separately reviewed shared runtime continues to provide PCRE2.

[Independent component pins](app-build-inputs.json) bind the complete installed
content/link inventories and original receipts of both wlroots, SDL/text and
NexaUI variants. A component cannot authorize changed files by rewriting its own
receipt. Inventory counts and byte limits derive from the previously verified
exports. These are developer content checks, not sealed-store or volume identity.
The memory SDL/text graph includes a 144,648,912-byte HarfBuzz library; stream
hashes and use that component's exact bounds rather than a generic small-file cap.

The builder also checks the common/session/SDL SDKs, build tools, Mesa runtime,
original GUI projection and separate audio SDK/policy/control inputs. Keep the
policy's Lua dependency distinct from the compositor's input dependency. Compose
uses the original private table and COPYING from the pinned libX11-common
package; this neither installs X11 nor enables an X11 backend.

## Running a build

Use a fresh, non-overlapping output on an ordinary x86-64 Linux account:

```sh
python3 tools/build_desktop_apps.py FRESH_WORK \
  --app-sdk APP_SDK --common-sdk COMMON --session-sdk SESSION \
  --sdl-sdk SDL --toolchain TOOLS --graphics-runtime MESA \
  --original-runtime ORIGINAL_GUI --wlroots WLR_BUILD \
  --ui SDL_TEXT_BUILD --nexaui NEXAUI_BUILD \
  --audio-prefix AUDIO --audio-policy-prefix POLICY \
  --audio-control-prefix CONTROL --compose-data COMPOSE_DIRECTORY
```

For ASan/UBSan, select the independently pinned memory variants of wlroots,
SDL/text and NexaUI and add `--sanitizers`. Leak detection stays enabled without
suppression. External prebuilt dependencies are not thereby instrumented.

CMake receives explicit include/library/package paths, including a typed
`NexaUI_DIR:PATH`. The original ctest executable uses the same selected loader
environment; the tool-only convenience wrapper intentionally selects a different
environment. The current build compiles all enabled targets and then runs 36
ordinary app/model tests. It records commands, logs, SDK checks and hashes of nine
main executables. Root catalogue/authority tests and graphical integration are
separate; successful compilation does not mean all registered tests passed.

## Private integration and remaining boundary

Existing development selection checks retain exact source/artifact/cache hashes.
Their directory-shape validation accepts the historical `nexaui-settings-…`
paths and the current `nexaui-desktop-{release,debug}-<12 hex digits>` layout.
Neither layout alone grants authority or selects a candidate. Normal defaults
and closed historical records remain separate from explicitly chosen test inputs.

The test-only desktop host binds its own source and reviewed font through the
actual CMake cache instead of assuming old carrier directories. It still uses
the real supervisor and app commands on a private headless seat, without PAM,
DRM provisioning, installed services or normal app admission.

Keep internal builds and disposable tests separate from the next USB-targeted
image. The agreed first-version gate remains in force. See the
[execution record](../brains/aetheri/journal/2026-11-10-current-desktop.md) for
actual results and the outstanding integration boundary.
