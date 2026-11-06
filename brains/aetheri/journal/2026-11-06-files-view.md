# Own NexaUI Files view

6 November 2026. `aetheri-files-nexaui` is a separate original C17 presentation
over the existing Files model/controller. List/grid, held locations, filtering,
inline naming/rename, Cut/Paste move review, menus and information now run through
NexaUI's native Wayland software path. The original Terminal/editor still owns
document editing and save decisions. The SDL executable and development owner's
selection remain unchanged. See the [contract](../../../spec/files.md#opt-in-nexaui-view).

The reviewed SDK stays 0.1.18/e71ee30. Its host scaffold attribution and MIT notice
are retained. No new dependency family, SDK rebuild, catalogue or chat handoff is
needed. The Files process loads no SDL, X11, EGL or Vulkan; the separate Terminal
continues to use its reviewed SDL profile.

## Ownership and input

The host supplies one C17 operation worker and its original join to the shared
controller. Completion wakes the UI through a private pipe. Operation inputs stay
immutable until join; a failed start promises no effect. Close waits for work and
the original document. Failure does not fabricate a successful join or discard a
live child. This is ordinary-user development I/O, not installed authority.

Every ordinary native event is applied and drawn before the next event. Native
semantic actions use the same UI context. Only one ordinary widget intent can be
applied per frame. Snapshot replacement retires row scopes even when contents
match; selected commands, dialog close/reopen and draft changes retire their controls. IDs are
monotonic local observations, never object identity or grants.

The new hidden-location counterexample initially opened the next sidebar folder
after its queued target was hidden. Root children now bind to their raw snapshot
entry and ancestors to their held depth, rather than changing sidebar ordinals.
The regression fails against the original view and passes after this correction.

Minimum-size/200-percent modal testing also exposed a required selection whose
original row had scrolled out while naming. The blocked inline list now publishes
no selected member until its rows return; the underlying model selection remains.
Naming controls are contained, nonoverlapping and keyboard reachable. Move review
is height-bounded; information reserves its Close area and retains dialog ancestry.

The final review added a stronger worker-attempt counter with a positive fresh
confirmation control. A direct controller draft change initially let an older
queued confirmation prepare the new name. The view now retires its control scope
on any draft change, preserving field focus while dropping the old action. The
original counterexample fails before the fix and passes afterward; a new explicit
confirmation still reaches the worker adapter once. An earlier incomplete test
adapter omitted the mandatory join callback and could not count attempts; its
positive control exposed that fixture error before accepting the negative case.

Name/filter fields currently use append-only app-owned editing, whole-grapheme
Backspace and Ctrl+U. Complete untagged commit batches are atomic; invalid,
filtered, incomplete and tagged target/revision edits cannot apply a prefix.
Full caret/selection, editable-text transport and external IME remain work.

## Executed verification

All 60 affected app/desktop/compositor groups pass Release in 184.55 seconds and
ASan/UBSan in 358.79 seconds, without skips. Both native Files view samples record
zero CPU ticks over the bounded 1.2-second idle interval, without a new view frame.
This is not a hardware performance benchmark. The existing SDL Files suite retains
61 successful cases per profile, including its separate uncertainty/close fixtures.
After the final draft fix, the three directly affected view groups pass again in
Release (8.26s) and ASan/UBSan (19.17s). The collector checks that only the view and
its test source changed after the full runs; other sources and legacy tested
program hashes remain unchanged. Full-run and final-targeted evidence are retained
separately rather than attributing the earlier suite to later source bytes.

The pure view cases cover 128 regular layouts at each of two raster densities,
English/German, contrast, four text sizes, list/grid and minimum through 4K-sized
logical viewports. Additional tests cover stale snapshot/selection/dialog/sidebar
actions, complete-commit rejection, minimum-size naming/move controls, modal Tab
focus and information above Close. These are scene/semantic checks, not physical
monitor or independent assistive-client acceptance.

The native private-seat driver performs actual keyboard/pointer input, real
file/folder workers, original document edit/save with exact `hello` bytes,
rename/move retaining exact `seed` bytes, German 200-percent text, native resize
to 640×420, inline cancellation and information. It retains 21 images per profile.
It checks the actually mapped SDK against its pinned build metadata and font hash.
The idle measurement waits for actual client-side Escape key-up processing and
render completion; driver acknowledgement alone was insufficient. The original
CPU/frame budget was not relaxed. Prior failed fixture/idle observations remain
separate from final passes.

Local evidence is retained under `local/files-view-2026-11-06`; full guest results
are under `/home/aetheri/build/files-view-5p_1cp4u`. The collector verifies 363
host/snapshot/guest source inputs, unchanged 28 frozen SQL/XML and nineteen
protected hashes without SQLite sidecars. All four foreign files remain byte-exact
and unstaged. Its 70 final exports include 42 verified native images; fourteen output-0
PNGs provide local previews. Minimum-size inline naming/information and the compact
move review were visually inspected. Full glass/human acceptance is still open.

The enforcing carrier is idle on tty1, with no build actors, running Aetheri
service or seat0 session. The prior closed development record and original older
history remain unchanged. No new DRM token was started: the new executable has
private-seat evidence only. The collector initially expected the current closed
record in history; that record remains in `active.json` until the next start.
The corrected read-only check compares current active and the actual older
archived baseline separately; the original collector failure is retained.

## Next

Bind the new executable to a distinct explicit development-owner selection and
exercise its fresh/reused lifetime on actual virtual DRM. Add independent Files
AT-SPI and fault-state view cases before broader adoption. The old no-exec or
quarantined runtime must not be used for UI testing or inferred settled.

SDK 0.1.18 still has no public fullscreen-entry request. The opt-in view reports
F11 unavailable; Zoom is only maximize/restore and SDL retains fullscreen. Full
glass, complete input/accessibility, normal admitted sessions, production file
grants/recovery/removal and physical/hardware acceptance remain required. The
broader apps, image/installer, updates, suspend/multimonitor and persistent
resettable experiment desktop goal remains active.
