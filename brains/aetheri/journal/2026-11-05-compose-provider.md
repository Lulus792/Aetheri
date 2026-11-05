# Built-in native Compose provider

5 November 2026. Continued actual Aetheri input implementation after the separate
NexaUI handoff. Its user-confirmed maintenance rule remains in AGENTS/START/STATE;
this milestone sends no NexaUI chat message or SDK/consumer-pin update.

## Result and boundaries

Added original C17 `compositor/compose` and `compositor/text_input`, build-embedded
reviewed Compose data and an explicit development-session profile. The provider
uses real keyboard events and native v3; SDL and the existing terminal retain
uncommitted text separately, then admit complete committed UTF-8 once to the PTY.
No new daemon, thread, external IME/key-injection global, production policy, SQL,
wire contract or installed app capability is added. See [Compose](../../../spec/compose.md).

The alternatives and remaining external provider work are in DECISIONS. This is
a useful DE/EN composition path, not general CJK/IME, native assistive/hardware,
trusted login/lock/resume or normal admitted app/session acceptance. The complete
OS/apps, shell/editor/panes, persistent sessions and experiment desktop remain work.

## Inputs and ownership

Existing libxkbcommon 1.13.1, wayland-protocols 1.49 and wlroots 0.20.2 stay selected.
The exact Compose table and full COPYING from pinned Fedora
libX11-common-1.8.13-3.fc44 were rechecked against the signed cached RPM payload and
installed files. Record: `/home/aetheri/build/compose-provider-check-ibcplzhc/inputs-review.json`.
The generated data carries its separate upstream license; no X11 link or runtime
HOME/XCOMPOSEFILE lookup is introduced. The generator rejects altered, included,
malformed, oversized and symlink input before replacing output.

Routing retains exact original seat/client/surface/field; secret fields exclude
the provider. Ignored competing objects cannot inherit admission. Repeated enable
resets a field, while cursor-only commit preserves it. A zero-argument request
observer is needed because wlroots exposes enable-to-enable as ordinary commit;
it neither logs protocol payloads nor retains borrowed message arguments.

An added native case exposed a real pending-enable bug: enable before leave,
return to the surface, then commit could activate old state. The first fixture
waited for an ordinary key that the bug consumed and timed out; retained log
`pending-enable-before.log` is not the final assertion result. The direct native
case then failed the exact cross-focus assertion in `pending-enable-before-direct.log`,
with client/server logs under `session-release/compose-lifecycle-r2wb5msh`.
The fix binds pending admission to the current focus epoch and clears it on every
focus change. Fresh enable remains usable afterward. No assertion/deadline was
weakened to pass the implementation.

## Executed checks

Guest check root: `/home/aetheri/build/compose-provider-check-ibcplzhc`.

- Final Release: **21/21**, 62.39 s; final Debug ASan/UBSan: **21/21**, 90.85 s.
  Includes terminal model/selection/paste/preedit/PTY/native/clipboard/IME,
  four Compose groups, desktop model/native/supervisor/original owner with and
  without Compose, compositor desktop/windows/input/clipboard. Earlier full
  67.86/96.59 s runs predate the pending-enable guard; later final runs supersede
  them rather than assigning new evidence to old binaries. Third-party UI/text
  libraries retain their reviewed build profiles.
- Native provider: eight cases, six raw images per profile; real DE dead acute,
  Menu sequences and multi-scalar UTF-8 produce exact original-shell files.
  Escape, font/DE/contrast, same-keyboard keymap change, device replacement,
  foreign-client focus and close retain input/owner boundaries.
- Independent native lifecycle: seven cases per profile; repeated enable,
  cursor update, password/PIN/hidden/sensitive fields, competing inputs,
  resource replacement, foreign focus/unfocused enable and buffered old enable.
- `compose-boundary-negative-gcgv1qif` removes only the enable-to-enable boundary
  observer from a build-local copy; the unchanged native test rejects it. This
  control predates the added pending-enable guard and has its own source/log.
  `compose-disabled-rk4xpb7t` builds without the feature, passes native windows/
  input, emits no Compose library and refuses `--compose`. Duplicate/unknown/
  malformed CLI options are refused by the enabled build too.
- Host tests: 50 contract/migration cases, 24 runtime vectors and seven VM-input
  cases pass. The host development-contract run passes six portable cases and
  explicitly skips six Linux-only cases; the actual carrier desktop.supervisor
  group runs all twelve. Host skips are not native acceptance.

`local/compose-provider-2026-11-05/native-evidence-index.json` verifies **330**
copied files and **186** decompressed native image hashes. It includes final
native roots, both original-owner profiles, original failures and bounded input
review. Model/fixture names and loaded binary digests are retained in those
results, not inferred from a current branch name.

## Ordinary virtual DRM session

Final original token `4cdc587f2db44e4ead83a8fe53e29d18`, Debug with desktop+Compose.
Boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`, PID63480, original stdout stream
`0170c1a0461f472bab9877e89b85fe18`, session-1175.scope. The protected start/stop
records retain its exact invocation, binaries and profile. QMP stays protected
host-local AF_UNIX; actual display options remain SDL/gl-off/window-close-false.

Real QMP PS/2 events through libinput/logind/bochs-drm produce native preedit and
the exact `c3 a4` shell-file bytes. Twelve PNGs form six stable pairs; Escape
restores the original pixels exactly at 100% and 200% text size. Final original
compositor/desktop/terminal all exit status0, and the controller closes the same
attempt. The earlier pre-guard DRM token `f7ed52c27e3943c59edd26e44b7eff22` has
its distinct original evidence retained under `before-focus-guard`; its results
are not reassigned to the fixed provider.

These are virtual software-rendering/native-protocol checks, not physical
three-monitor/driver or Windows keyboard-forwarding/human assistive acceptance.
The final native German dead-key and final DRM 200% preedit images were visually
inspected. This is a provisional development view, not final product design.

## Preservation audit

The read-only final audit joins all **285** host/snapshot/guest source hashes,
**28** frozen SQL/XML hashes and **19** protected old fixture/catalogue/profile
hashes, with no SQLite sidecars. All **925** local Markdown file targets resolve.
The guest is enforcing on tty1 without a known live native test/app, running
Aetheri service or seat0 session. Native/DRM shared binary hashes agree, and all
twelve final console-file hashes verify. Record:
`local/compose-provider-2026-11-05/final-audit.json`.

Continue with the app-owned rich line/editor model and actual PTY/job-control
integration. Preserve the separate quarantined running/recover-handoff attempt;
this UI work cannot settle its old catalogue/lease obligations.
