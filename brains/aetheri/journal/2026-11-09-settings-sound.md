# Native Sound page and original-owner binding

## Result

The optional NexaUI Settings view now presents output choices, a thin volume
track and a right-aligned mute switch. It uses the accepted Sound study as its
reference and the current native opaque fallback. Native keyboard and semantic
radio/slider/switch roles, English/German, three themes/text sizes and narrow
scrolling remain available. No animation or background helper is added.

`audio/types.h` extracts the existing copied C value definitions without changing
their fields or enum values. The shared Settings model therefore needs no GLib
or audio runtime. It uses the existing UTF-8 decoder, validates observations and
assigns never-rebound window IDs to complete connection/serial/node refs.
Reordering retains identity; reuse and removal/reappearance do not.

UI actions create one intent and leave observed selection/volume/mute unchanged.
Dragging edits a draft, commits on release and cancels on pointer/focus loss.
Keyboard/value actions submit directly. Cached pending/unknown/stale/lost state
disables effects. Old observed commands cannot confirm a later request. Boosted
readback is displayed without automatic clamp writes. Mute preserves volume.

`settings/audio` pumps the original admitted session-control handle, checks before
consuming one intent, forwards exact revision/ref and records actual submit and
readback results. It never opens/selects a socket or replacement owner and never
replays an uncertain intent. This is a programmatic owner binding. The normal
executable does not yet create an admitted owner or wire its event notifications;
its real Sound page correctly reports unavailability, with no injected devices.

## Executed evidence

All builds/tests use ordinary UID1000 in fresh private RAM/mount namespaces of
the preserved enforcing carrier, boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. Existing reviewed NexaUI 0.1.23/97a4264,
SDL/text and wlroots prefixes are read-only. The private audio SDK export is
checked by archive/member hashes and complete reconstructed prefix inventories.
No package, normal service, loaded policy, frozen SQL/XML or USB image is changed.
Populated audio observations and binding endpoints are controlled test adapters,
not actual joined Wayland/logind/MAC/audio admission.

Evidence is retained in `local/settings-sound-2026-11-09`:

| Run | Result | Export SHA256 |
| --- | --- | --- |
| v1 | Existing matrix expects a content action on the new unavailable page; model/binding/owner groups pass; retire failure | `76c0f0b2ed794f51aeb2100fd2df7644135c189916c8497720fdf7d97f903cc2` |
| v2 | Six Release groups and renders pass; native script omits the new Sound stop in its expected Tab sequence; retire failure | `6ecae716c85618b3ad85592f1278320b5d2f1143eccd8ea8aef4fe478ef511a9` |
| v3 | Six groups plus real native window/navigation pass in Release and ASan/UBSan | `369f0f7642b94b37e06669febfe9ae9ba04e6be4f18b12631aa503fd42b40efe` |
| v4 | Add drag/release/cancel coverage; same full suite passes; omitted-serial counterexample detected and restored | `09f7f342a717b601c8f7e4a6646d29dbe440bc10d0d0a54a27e50724f2a0c0fb` |
| v5 | Keep ready controls at the selected raster density; add zero/unity/boost/max-finite readback; six groups pass both builds; counterexample detected/restored | `af24c94d37b9125565a20f45d9754de78c2da7a82a645e12b871e768da67fde9` |
| v6 | Fix feedback when an unsubmitted queued intent is discarded after observation failure; final six groups plus native cases pass both builds; counterexample detected/restored | `ce8ccb00367edf269de10e1beb9fb1df822a7cab002624fd8e296c1ed66daac3` |

Original helpers 1336041, 1336866, 1337699, 1339310, 1340949 and 1342634 retire after
independent full-export/member validation. Keep their exact protected tokens in
the local intent/receipt files; do not replay any historical run.

Final v6 groups are `settings.model`, `settings.sound`, `settings.audio_binding`,
`settings.view`, `settings.view_hidpi` and `audio.session_control`: Release 5.01 s,
ASan/UBSan 8.20 s. The real `settings.view_native` passes Release 13.81 s and
ASan/UBSan 19.49 s. Exact final source inputs match this executed archive. The private
counterexample removes serial equality from the model and fails the actual
held-row replacement assertion. Restore exact source and pass again.

Each view matrix checks 360 theme/language/text/viewport/page combinations at its
selected raster density. Additional ready-page input tests cover semantic
roles, keyboard volume, one release-only drag, cancelled movement, mute and
default intent/readback, no optimistic state, held-row replacement, pending/
lost refusal and large-text focus reveal. The selected-density correction keeps
those ready controls at 2x in the HiDPI case, not just the unavailable page.

Four final Release raw PPM captures are exported and viewed as equivalent PNG
previews. Their SHA256 values are:

| Capture | SHA256 |
| --- | --- |
| Dark ready | `a2673807b9e8f9d27e80b3458df36d12bdf2c62d7085c9cec6526d967b7667f1` |
| Light German200% | `9bb501f3bf6f882b1d2b24f033b3533823182e29c69a75ffb97db9228efed29a` |
| Light German200%, focused controls | `dd77168f3e3af1cbd69f751e41b05d7a1fa24cc24bcf49fe8645b19e8224da5f` |
| High-contrast muted | `2fa1cedebd02ddb919287d1dba530e45c31d598251017b21fbe0efcb7df16d8a` |

These are rendered test observations. The separate real native test operates
the normal app through the private compositor: Sound navigation/unavailability,
return to Displays, original chrome/input/scroll/window behavior and orderly
original-child retirement. It does not play audio. Screenreader tasks and a
joined live UI-to-HDA/physical acceptance are still pending.

## Remaining integration

The final read-only audit confirms all 445 original source inputs, 19 protected
hashes, 31 frozen SQL/XML and 52 normal guest files unchanged. The copied audio
types retain exactly the original definitions. All four foreign UI documents
retain their byte hashes. Loaded enforcing policy remains SHA256
`c836158721c0b314cea6e26ec8d18f71957b676180f049b3ca5a6957db9dceaf`.
Original QEMU 9520 stays live; the separate audio lab stays off. USB is not written.

Provide protected normal services/device ownership and the session-owner event
loop, then attach that original handle to the visible host. Keep normal public
capabilities closed until actual admission and loss/holder obligations pass.
Microphone/app mixer, complete device discovery, final materials/layout,
delegation/revocation/restart, physical HP/Bluetooth and release-image integration
remain work. Preserve original quarantine, foreign UI documentation and USB
documents. NexaUI handoff remains delivered; no new message or toolkit requirement
is needed for these already supported primitives.
