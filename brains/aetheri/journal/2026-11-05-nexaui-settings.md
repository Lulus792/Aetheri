# NexaUI Settings on the Aetheri compositor

5 November 2026. A first original-model Settings consumer is now exercised in
the Fedora 44/glibc carrier. It is a separate opt-in probe; the working SDL view,
normal development owner and default app selection remain unchanged.

## Implemented

- [Input manifest](../../../spec/nexaui-inputs.json): exact NexaUI 0.1.18 commit
  `e71ee304f1e6c23fa562d9e4b11f7e8e12152079`, reproducible raw source archive,
  MIT notice and the matching signed D-Bus header-only RPM.
- [Private builder](../../../tools/build_nexaui.py): fresh unprivileged builds,
  source/license/signature/runtime checks, original model snapshot, retained
  commands/artifact hashes and no download/global package installation.
- [Native test](../../../tools/test_nexaui_settings.py) and opt-in
  `AETHERI_NEXAUI_SETTINGS_PROBE` / `settings.nexaui`: original upstream view test,
  actual compositor pointer/keyboard, output observations, model changes,
  narrow resize/restore, software pixels, idle and close/failure paths.
- Standalone compositor output changes now configure the selected initialized
  windows even when no private desktop owner exists. The previous dock/work-area
  path remains in charge when it does exist; the new branch also rechecks the
  current pointer hit after scene changes.

The SDK builds with C17, shared Wayland/software presentation and AT-SPI enabled.
The current carrier has Wayland 1.26.0, wayland-protocols 1.49, xkbcommon 1.13.1
and existing dbus-libs 1:1.16.2-1.fc44. The eighteen D-Bus headers are extracted
as data from the exact signed Fedora development RPM. There are no new installed
packages/scriptlets, SDL/X11/Vulkan links, authority RPCs or settings persistence.
This selects a functional pilot, not final renderer/performance acceptance.

## Executed results

Host evidence: `local/nexaui-settings-2026-11-05`. Guest initial positive suite:
`/home/aetheri/build/nexaui-pilot-check-7yc6u1l5`; registered probe:
`/home/aetheri/build/nexaui-registered-kamp5oa0`; final native pointer extension:
`/home/aetheri/build/nexaui-pointer-369f8m5w`.

The exact source archive has SHA-256
`15fc2fbd41223af85612c45e7b490985e92bed481075594b7edb866f2d75027a`.
Its original model hashes match the current Aetheri model. SDK/view/model/test
compile commands all select C17; the Debug commands include ASan and UBSan.
Release prefix: `/home/aetheri/deps/nexaui-settings-release-02`; Debug prefix:
`/home/aetheri/deps/nexaui-settings-debug-01`. Model/artifact/loaded-library
evidence remains separate for each profile.

The first export inherited Windows CRLF conversion and failed the frozen raw
license hash before compilation. An incomplete archive-size update then failed
the size guard before a new workspace was created. Both are input/tooling
failures, not SDK defects. The original archive and incomplete first workspace
remain. Command-local `core.autocrlf=false`, raw hash and correct size fix the
export without changing Git configuration or SDK source.

The first native run `/home/aetheri/build/nexaui-settings-yihsasff` used the
published `d19fd73` compositor. It passed actual input/model checks but failed
waiting for the 320x320 view: the output observation changed while the app still
submitted 1920x1080. Its source path returned early through a missing desktop.
That original log/maps/scene evidence is retained as the negative for the narrow
production correction; no deadline/assertion was weakened.

After correction, all five groups pass in both profiles: `settings.model`,
`settings.native`, `compositor.desktop`, `compositor.wayland`, `compositor.input`.
The registered NexaUI test passes in both profiles, then passes again after
adding actual pointer checks: **3.70s Release / 11.51s ASan+UBSan**. Each final
probe retains **four states / twelve images** across three headless outputs.
Keyboard changes the original theme/language/text model; native pointer selects
Light and Dark. Maximized output resize to 320x320 and restore retains German,
high contrast, 200% text and keyboard focus; traversal reaches the observed
display buttons and Close. Escape/unmap, missing display and missing font exit
as expected. Both bounded 1.2-second idle samples have zero additional CPU ticks
and no new frames. This is not a general performance benchmark.

Audit verifies **343** host/snapshot/guest source inputs, **28** frozen SQL/XML,
the unchanged **19** protected old-runtime hashes and no SQLite sidecars.
The enforcing guest is idle on tty1 with no candidate actors, known native apps,
running Aetheri unit or seat0 session. The old running/recover-handoff remains
unsettled. Bounded export verifies **54 files**, including all 24 final images,
compile commands, binaries' hashes, JUnit, logs and loaded maps.

## Next boundary

The test deliberately makes the AT-SPI bus unavailable. It establishes compiled/
late-loaded transport and ordinary interaction under that condition, not native
assistive publication or human screenreader acceptance. Add a real independent
AT-SPI client test and a separately selected development-owner/DRM launch/reuse/
close/reopen profile before replacing Settings. Preserve the original model,
actor ownership and existing behavior; complete the accepted visual design and
measure the relevant resource budgets. No normal app or privileged lifecycle
capability is opened. See [probe contract](../../../spec/nexaui.md).

Other app migrations, normal session/grants/recovery/removal, real settings
backends, full UI/input/accessibility, installable image/installer/hardware and
the persistent resettable experiment desktop remain required. The full goal is
active. The NexaUI handoff is delivered; no new chat message is pending.
