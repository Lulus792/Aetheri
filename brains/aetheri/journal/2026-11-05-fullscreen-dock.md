# Fullscreen dock and current-scene pointer routing

5 November 2026. The ordinary development desktop now hides its compact dock
under a foreground fullscreen app, while keeping explicitly opened search
reachable. This corrects the [Files milestone's](2026-11-05-files-view.md) retained
overlap image. It is not secure locking, production admission or a complete
fullscreen workspace/mixed-DPI/hardware contract.

## Result and ownership

The compositor derives coverage per output from reverse scene order, enabled/
mapped windows, current coordinates, effective xdg geometry and committed
fullscreen state. It requires the selected output to be fully covered. A normal
foreground window reveals the dock there; moving it away or reactivating the
original fullscreen app hides it. A window's original home and a global keyboard
owner are insufficient. Surface-local geometry offsets are already handled by
the selected wlroots xdg scene child; the window root is the geometry origin.
Bounds use widened arithmetic. No toolkit/library version or protocol changed.

Only compact scene visibility changes. The original layer remains mapped and
keeps its role/reserved work area; expanded search is visible and retains its
actual exclusive keyboard ownership. Hooks cover commit/map/unmap, raise/focus,
arrangement/output-size changes and interactive moves. No new thread, service,
polling timer, account authority or lifecycle acknowledgement is added.

A scene change rechecks the actual target and local coordinates at the stationary
cursor. Only a changed target/coordinates produces motion/frame notifications;
keyboard ownership is unchanged. Held buttons and move/resize grabs defer it until
their original sequence ends. No pointer device means no synthesized pointer
notification. The existing release path then routes a fresh press to the current
scene. This avoids both invisible-target clicks and a repaint/motion loop.

## Executed verification

Evidence: `local/fullscreen-dock-2026-11-05`; guest final root
`/home/aetheri/build/fullscreen-dock-check-5ajd5mmb`. Commands:

```text
python tools/run_vm.py status
python tools/sync_guest.py
python local/fullscreen-dock-2026-11-05/run.py
python local/fullscreen-dock-2026-11-05/finalize.py
python tools/development_session.py start --desktop --compose
python local/fullscreen-dock-2026-11-05/exercise_drm.py <evidence-directory>
python local/fullscreen-dock-2026-11-05/audit.py
python local/fullscreen-dock-2026-11-05/collect.py
git diff --check
```

Two original negatives are retained. The unchanged original compositor binaries
fail the new bottom-pixel assertion: dock RGB52/64/90 remains instead of app
RGB41/99/196. A visibility-only correction passes that image but fails a new
stationary-pointer press after hiding; no fresh pointer movement is sent. The
subsequent source recheck corrects it. These are distinct executed failures,
not claimed successful runs. Their reports and both raw-image hashes survive.

All **46/46 affected groups** pass Release (**126.56s**) and ASan+UBSan
(**181.89s**). Final review then adds only the pointer-device-presence guard in
`input.c`. Exact snapshots prove that sole source delta; final compositor,
native-window/input and desktop/owner modes pass **6/6 groups per profile**
(**41.92s Release, 58.54s Debug**). Only the compositor and test-server binary
hashes change. The earlier full-suite binary and final binaries remain distinct
in the reports. Debug compile commands verify both sanitizers on changed units.

Each final private desktop fixture has **25 cases and 48 images** across three
actual headless outputs. The unchanged sixteen-capture limit is retained.
Independent clients exercise fullscreen pixels, exclusive search/real keyboard,
other outputs, resized selected output, normal-window activation, a real drag
across output boundaries without client repaint, fullscreen unmap during search,
stationary target/local-coordinate changes and original held press/release.
Existing private-global denials, bounds, stale IDs and idle/no-configure-loop
checks remain. These are virtual/software protocol results, not physical GPUs.

Actual final-binary DRM session `0641483bd71e4cbba5f5cdfc9e907a5b` has eighteen
stable PNGs. A sampled old dock pixel now belongs to opaque fullscreen Files.
The actual SDL search producer responds while compact visibility is disabled;
Settings can open above fullscreen, and returning to the original Files process
hides the dock again. Native restore and fullscreen close return to the normal
desktop. A new Files lifetime edits/saves exactly `Aetheri fullscreen` in the
retained document. Three actual actor ELF hashes match tested binaries. The
original four direct roles end status0 and the selected controller closes0.

Audit binds **340** host/snapshot/guest sources. **28** frozen SQL/XML and
**19** old-runtime hashes are unchanged, without SQLite sidecars. The enforcing
carrier is idle on tty1 without known native actors, an Aetheri unit or seat0
session. The old running/recover-handoff intent remains unresolved. Bounded native
export retains **112 files**, **96 verified positive images** and two original
negative images; no complete workspace is recursively duplicated.

## Remaining work

The current fixed-scale free-window presentation does not add fullscreen spaces,
animation, general dock autohide, secure lock or mixed-DPI/hotplug acceptance.
The accepted visual baseline, glass, full accessibility/input, further apps,
normal grants/recovery/removal, system image/installer/hardware and the resettable
experiment desktop remain required. Next, assess a concrete NexaUI Settings
consumer in the Fedora carrier without replacing the working view before its
actual gates pass. The delivered NexaUI catalogue remains the existing handoff;
no new chat text is pending. Keep the full project goal active.
