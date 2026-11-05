# Built-in Compose and dead-key input

The optional C17 provider connects actual compositor keyboard events to native
Wayland text-input-v3 preedit and commit events. The terminal receives those
through SDL3 and retains uncommitted text outside PTY content. This supplies an
early German/English Compose/dead-key path without a separate input daemon.
It is not a general external IME, trusted lock input path or completed normal
session/app-admission implementation.

## Build and use

In the reviewed Linux [graphics/session environment](session.md), add
`-DAETHERI_COMPOSE=ON` to the existing compositor build. It requires
`AETHERI_COMPOSITOR=ON` and defaults off. The feature uses the already reviewed
libxkbcommon 1.13.1 and wayland-protocols 1.49; it adds standard Compose data,
not another rendering, widget or input-library family. Other build options and
dependency pins remain in [graphics inputs](graphics-inputs.json).

Enable it explicitly for a new [development session](development-session.md):

```sh
python tools/development_session.py start --desktop --compose
```

`--profile debug` selects the existing ASan/UBSan build. Direct compositor use
accepts `--compose` after its fixed arguments, in either order with the optional
`--shell-fd FD`. Duplicate/unknown options and a Compose request on a feature-off
binary are refused. The development record binds the chosen boolean profile;
historical records without that field remain unchanged and mean disabled.
Start selection does not update or restart an existing session.

Menu is a provisional Compose key only while this provider handles the current
text field. For example, Menu, double quote, a commits `ä`; Menu, apostrophe, j
commits the two-scalar string `j́`. With an actual German keymap, dead acute then
e commits `é`. Escape cancels a draft. Application function keys can remain
usable while composing; the terminal's F6–F8 update its language, contrast and
text size. Final shortcuts and layout settings remain product/UI work.

## Ownership and protocol boundaries

| Module | Responsibility |
| --- | --- |
| [compose.h](../src/compositor/compose.h), [compose.c](../src/compositor/compose.c) | One owner-thread Compose state, bounded UTF-8 preview/result and borrowed result lifetimes; no protocol or keyboard effects |
| [text_input.c](../src/compositor/text_input.c) | Original seat/client/surface/field admission, serial and enable boundaries, native preedit/commit and reset |
| [input.c](../src/compositor/input.c) | Actual keyboard/keymap/modifier delivery and consumed press/release suppression |
| [embed_compose.py](../tools/embed_compose.py) | Check exact build inputs, reject includes/invalid bytes and embed standard data while retaining its license |
| [terminal](terminal.md) | Own preedit view, complete committed-text validation and exactly-once bounded PTY queue admission |

The provider handles at most 64 live text-input resources. Excess or foreign-seat
creation refuses that client, not the compositor. Only a freshly committed enable
on the currently focused surface can select an input. A competing enable is
ignored, even if the accepted field is secret; the other object needs a fresh
enable after the original is disabled. A rejected or old enabled object cannot
inherit input when focus returns or the previous object disappears.

Password/PIN purposes and hidden/sensitive hints exclude this provider. Ordinary
keys continue through the native keyboard path; exclusion is not authentication
or a guarantee that a client protects its own secrets. No surrounding text is
forwarded to an external service or logged. The existing wlroots protocol state
still receives the client's requests.

A repeated enable also starts another field epoch within the same surface.
wlroots 0.20.2 otherwise exposes that as an ordinary commit. A narrow owner-thread
Wayland request observer therefore records only whether an original resource
received a zero-argument enable/disable request. It reads no text arguments and
prints no protocol messages. The boundary takes effect on commit. An ordinary
candidate-rectangle update preserves composition. The observer is removed before
its owner is freed.

An enable must originate in the same current focus epoch as its later commit.
Focus changes discard pending enable admission as well as the active field.
Committing a request buffered before leave/re-enter cannot make it fresh; a new
enable is required. This has an actual independent-client counterexample before
the guard, not just a modelled test.

Focus, original keyboard removal/replacement, keymap changes, control/alt/logo
commands and field disable/replacement reset the draft. The same resource's
commit counter must increase; wrap/reuse retires it rather than admitting another
transaction. The draft/result is bounded to 1024 UTF-8 bytes and 32 sequence keys.
Returned results reject malformed UTF-8, controls and bidi-formatting characters.
Protocol byte cursors, app scalar offsets and terminal cells remain distinct.
No external input-method or virtual-keyboard global is introduced.

## Standard data and licenses

The [input manifest](graphics-inputs.json) records exact sizes and SHA-256 values
for the reviewed `en_US.UTF-8/Compose` and complete upstream COPYING from the
already pinned Fedora `libX11-common-1.8.13-3.fc44.noarch.rpm` in
[UI inputs](ui-inputs.json). The package carries MIT AND X11 terms. This data does
not link the compositor to X11. The protocol XML also has an exact CMake hash
check before scanner generation.

The generator validates data and license before creating output, rejects symlinks,
incorrect sizes/hashes, invalid UTF-8, NUL and include directives, and bounds
reads. It emits a byte initializer and a separate `compose-libX11-COPYING` in
the build's generated directory. Runtime uses the embedded bytes with explicit
`C.UTF-8`; it does not select `$HOME/.XCompose`, `XCOMPOSEFILE` or arbitrary files.
The source RPM cache is not a runtime requirement. Binary/image packaging must
retain the exact upstream license; the original-code MIT license does not replace
it. No production image/install packaging is claimed here.

## Verification and remaining work

[Model tests](../tests/test_compose.c) use the actual embedded table, including
ignored modifiers, reset/cancel and multi-scalar output. [Input tests](../tools/test_embed_compose.py)
check generated bytes/license and reject changed, included, malformed, oversized
and symlink inputs before replacing output.

The [native provider test](../tools/test_compose_provider.py) uses actual device
events, production provider code, native v3, SDL, the original PTY and shell file
effects. The [independent native client](../tests/test_compositor_client.c) and
[lifecycle test](../tools/test_compose_lifecycle.py) exercise repeated enable,
cursor-only commits, all four secret cases, competing objects, resource replacement
and other-client focus. The [supervised desktop test](../tools/test_development_desktop.py)
also has an explicit Compose profile. None uses the older test-only fixed-text
producer as the provider under test.

Executed results, source identities, original failures, negative controls and
virtual DRM checks belong to the dated [handoff](../brains/aetheri/journal/2026-11-05-compose-provider.md).
External/CJK IMEs, authenticated external provider lifecycle, broader script/font
coverage, human assistive acceptance, physical devices, trusted lock/resume and
normal app/session mediation remain separate required work. This does not finish
the shell's rich editor/panes/persistence or the complete OS.

Primary contracts: [Compose API](https://xkbcommon.org/doc/current/group__compose.html),
the exact installed wlroots 0.20.2 `types/wlr_text_input_v3.c` and pinned protocol
XML, plus [Wayland 1.26.0 request-observer ordering](https://github.com/wayland-mirror/wayland/blob/1.26.0/src/wayland-server.c).
Generic current documentation does not select a newer libxkbcommon release.
