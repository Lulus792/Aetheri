# NexaUI suitability

## Maintenance rule — confirmed 5 November 2026

| Item | Status | Next action |
| --- | --- | --- |
| [Requirement catalogue](D:/NexaUI/NEXT_AETHERI_UI.md) | Delivered; maintained in place | Add new needs or correct existing rows when evidence warrants it; preserve IDs. |
| Initial NexaUI chat/start message | Delivered; closed | None. Prepare another message only on a new explicit user request. |
| Routine SDK review | Allowed alongside Aetheri work | Record meaningful changes in the catalogue; if nothing changed, leave it unchanged. |

After compaction, carry forward **handoff delivered; catalogue maintenance only;
no pending chat message**. Do not recreate the list, repeat its overview, ask the
user to paste the old message or treat the historical records below as an open
assignment. Continue the actual Aetheri task from STATE. SDK implementation and
consumer-pin changes are separate work, not implied by catalogue maintenance.

Routine checks are background maintenance, not a recurring user deliverable.
No relevant change means no catalogue edit and no NexaUI status paragraph in the
reply. When requirements change, update the relevant rows/integration cases;
if reporting the change helps, give only the concrete delta and catalogue link.
Do not append the old overview, implementation request or a request to paste it.
Context summaries should retain the closed handoff status and the actual current
Aetheri task; the historical request must not become the next task after resuming.

## Current decision — 4 November 2026

6 November startup-output update: the own Terminal now displays the accepted
mark/system-information layout as actual original-shell PTY output on the same
SDK. Kernel capture, bounded formatting and startup lifetime belong to Aetheri;
no toolkit-side information model or new host connection is needed. Native text
checks inspect the displayed fields independently. See the
[startup record](../journal/2026-11-06-terminal-startup.md). The SDK/pin/defaults
and delivered handoff remain unchanged; this creates no new SDK requirement.
The initial full instrumented run nevertheless fails a later native Text query;
further original-producer GetState/Close failures remain unresolved. Private
timing/bus-observer passes are diagnostic evidence, not SDK/host fixes. Retain
these under the existing A07/Q04 acceptance work, without upgrading the pin or
repeating the delivered handoff.

6 November compact Terminal chrome update: the own presenter reuses existing
SDK shapes, label layouts and semantic buttons on its original SDL host. No SDK
window or extra thread is introduced. Original-press cancellation and observed
Zoom/Restore retain the native host's effects; disabled Minimize requires future
compositor hide/restore work. Mixed-size label cache entries remain separate.
See the [chrome record](../journal/2026-11-06-terminal-chrome.md) for actual tests
and scoped visual limits. The pin/defaults and closed handoff stay unchanged.

6 November original-content reading-focus update: the own Terminal now uses
native read-only row focus and Ctrl+Shift+R on the unchanged SDK. Content-token
semantic IDs preserve unchanged movement and retire edited/replaced targets;
model-size changes advance the ID range rather than reviving old projections.
No shortcut bytes enter the PTY and the original model remains unchanged.
View/native tests and a positional-ID counterexample supply scoped evidence;
all 74 app/UI groups pass Release and ASan/UBSan. See the
[focus record](../journal/2026-11-06-terminal-focus.md), including retained
uncaptured native-state failures. Full human reading, native activation and
final design/hardware gates stay open. No SDK/pin/default or handoff change.

6 November native Terminal transport update: the existing Linux accessibility-
host API now publishes our actual read-only row snapshots from the SDL window.
The UI thread retains bridge calls, while the original worker polls duplicated
readiness objects. Independent native text/focus/reconnect/Close and wrong-focus/
disabled negatives pass; the complete 74-group app/UI suite passes Release and
ASan/UBSan. See the [record](../journal/2026-11-06-terminal-access.md), including
preserved initial failures and scoped acceptance. No SDK upgrade, default change
or new handoff is implied. Human reading and complete assistive/design/hardware
acceptance remain open.

6 November Terminal owner update: `--terminal-ui nexaui-owned` now selects the
tested presenter for both normal Terminal and Files' direct-editor windows.
The separate original-owner profile retains exact source/SDK/font bindings,
fresh-role drift rejection, original live reuse and historical closed semantics.
Ten affected groups pass Release and ASan/UBSan; a separate Root-bound virtual
DRM session verifies actual document save, normal shell output and orderly end.
See the [owner record](../journal/2026-11-06-terminal-owner.md). Default SDL,
SDK 0.1.18, native assistive/design/performance boundaries and production
admission remain unchanged. The delivered handoff still needs no new message.

6 November Terminal presenter update: an explicit own terminal binary now paints
the actual shared session through the pinned SDK's cell-grid/text/raster APIs.
Its original SDL Wayland/event/clipboard/IME/process host remains. This is a
bounded CPU RGBA upload path, not a second toolkit window or substitute shell.
Complete per-row read-only snapshots cover dense Unicode grids without silent
per-object truncation. Native host accessibility transport, accepted final
materials/chrome, default-owner adoption and physical/performance acceptance
remain separate work. See the [view record](../journal/2026-11-06-terminal-view.md).
The SDK version/defaults and delivered handoff status remain unchanged.

6 November Files native-text update: the custom field editor is replaced by one
SDK text field per view input, with history disabled and complete Aetheri candidate
admission before publication/actions. Independent native Text/EditableText tests
cover Unicode reads, caret/selection and range/whole edits, stale handles and
retained over-limit rejection. See the [text record](../journal/2026-11-06-files-text.md).
This supersedes the earlier custom-field implementation below. The pinned field
descriptor lacks a description member, so localized rejection context is on the
immediate named group. Human/geometry/event/IME/clipboard acceptance stays open;
no SDK pin or default changes.
The existing catalogue now records this scoped evidence and the pinned
description/style limitation under existing IDs; no new handoff text is due.

6 November Files chrome update: own Files now follows the tested Settings
original-press move/resize and Escape ownership rules, with title double-click
maximize/restore and fresh modal-admission checks. Native geometry/pixel/device
tests exercise the actual Aetheri compositor. See the
[chrome record](../journal/2026-11-06-files-chrome.md). The pinned SDK still has
no fullscreen-entry API; this does not change the SDK/defaults or complete
human/hardware/session acceptance.

6 November Files editing update: the own name/filter fields now retain bounded
app-owned grapheme selections with keyboard and mouse editing. NexaUI supplies
text layout, hit testing, selection geometry and clipped caret viewing. Native
EditableText, external IME and an additional toolkit text/undo model are not
introduced. Complete replacement admission and explicit rejection clearing stay
with Aetheri. See the [editing record](../journal/2026-11-06-files-editing.md).

6 November Files accessibility/fault update: the own view now passes an
independent C17/libatspi client with a real private registry, including actual
selection/actions, invalid state, stale rejection and localized dialog/focus.
Separate native tests hold/lose real mkdir/rename acknowledgements, retain the
effects and verify a read-only inspection plus original-worker close waiting.
The final affected seven groups pass in both Release and ASan/UBSan. See the
[executed record](../journal/2026-11-06-files-access.md). Complete editable text,
IME, human/normal-session and hardware acceptance remain separate. No SDK pin,
default, catalogue or handoff change follows.

6 November Files view/owner update: a separate `aetheri-files-nexaui` now uses the
reviewed 0.1.18 SDK and the shared Files controller. Its own list/grid, held
locations, inline naming, move review, menus and information work through native
Wayland/software input and the original file/editor operations. Deferred actions
retain snapshot/dialog/location identity. Minimum-size modal keyboard and
200-percent text cases supplement the ordinary layout matrix. See the
[Files contract](../../../spec/files.md#opt-in-nexaui-view). A distinct explicit
`--files-ui nexaui-owned` profile now has original-owner, private-seat and actual
virtual DRM evidence, including unsaved-editor close waiting and fresh Files.
Settings selection is independent; SDL stays the default. See the
[owner record](../journal/2026-11-06-files-owner.md). At that checkpoint independent
Files AT-SPI was still pending; the later update above supplies its bounded
native evidence. Complete text/IME, fullscreen entry, full glass and normal
session/hardware acceptance remain separate gates.

6 November owned-view/owner update: Aetheri's own C17 Settings presentation now
uses the reviewed 0.1.18 SDK through an explicit `--settings-ui nexaui-owned`
profile. Separate pages, opaque themes, narrow keyboard reading and native
independent AT-SPI checks retain the original model. Native and actual DRM owner
tests prove image/library binding, reuse, close/fresh lifetime, independent
Terminal/Files coexistence and ordered end. Own model input is independent of
the unused SDK example's model; the retained owner still refuses changed inputs.
The original upstream option and SDL default remain. Full glass, human/normal-
session/physical/performance acceptance and real system services remain open.
See the [view record](../journal/2026-11-06-settings-view.md) and
[owner/DRM record](../journal/2026-11-06-owned-settings-owner.md).

5 November development-owner update: the pinned external Settings consumer is now
an explicit `--settings-ui nexaui` development profile with root/original-user/
fresh-spawn binding checks. Native dock/search and actual QMP/PS2/DRM tests prove
reuse, fresh lifetime, original Terminal/Files coexistence, local large-text
preferences and ordered five-role retirement. Private-copy input drift rejects
a new spawn without fallback. The default remains SDL; developer hashes are not
sealed-code admission, and normal bus/final design/production integration remain.
See [executed record](../journal/2026-11-05-nexaui-owner.md).

5 November native assistive-transport update: the unchanged pinned consumer now
passes a separate C17/libatspi peer and real private registry on the Aetheri
compositor. Native radio state/actions, localized large text, semantic resize,
display selection and Close are checked against independent original-model and
pixel/process observations in Release and ASan/UBSan. The disabled-bridge negative
fails correctly. This advances the prior unavailable-bus boundary; it does not
establish human screenreader/focus-event or normal production session acceptance.
The subsequent owner update above records its separate development/DRM result.
See [executed record](../journal/2026-11-05-nexaui-atspi.md). Defaults remain SDL.

5 November Aetheri-side pilot: the pinned NexaUI `e71ee30` original-model Settings
consumer now builds and runs on the Fedora/glibc carrier and actual Aetheri
private compositor, in Release and ASan/UBSan. Its software Wayland path has no
SDL/X11/Vulkan dependency. Native pointer/keyboard, three output observations,
German/high contrast/200%, narrow resize/restore and view lifetime checks pass.
That first test bus is deliberately unavailable; the subsequent separate native
AT-SPI test above supplies scoped action evidence; the later owner entry records
its separate opt-in development/DRM result.
The working SDL app remains selected. See [probe contract](../../../spec/nexaui.md) and
[record](../journal/2026-11-05-nexaui-settings.md). This advances the previous
recommendation to an executed consumer probe, not global SDK/application adoption.

5 November incremental consumer update: the existing catalogue's integration
cases now reference our [own line-editor contract](../../../spec/line-editor.md),
including app-owned grapheme offsets/history/draft and whole-draft rejection.
The same case now includes bounded undo/redo and literal completion ownership,
with no automatic execution on suggestion selection or history mutation on undo.
It also records reversible history search and the new app-owned document/modal
core: borrowed gap spans/content revisions are not file permissions or durable
save evidence. Aetheri's actual terminal editor now owns local file I/O and
save/close/create-only export choices. The existing case now also preserves
uncertainty independently of clean content, additional export holders, explicit
discard, focus return and minimum-viewport edit refusal. These are consumer
contracts for a future view migration, not a toolkit-adoption or document-grant claim.
Document delta undo/redo now also belongs to Aetheri: the existing case forwards
edit/history commands, preserves saved logical-state versus monotonic revision,
and refuses a competing widget undo stack or any history-based save acknowledgement.
This reuses the existing T/I/B/P IDs; all 103 IDs and thirteen surface groups
remain. It adds no SDK acceptance claim, consumer-pin change or new chat text.
The same case now retains Aetheri-owned literal document search: reversible
whole-grapheme preview, original cursor/prior-query restoration, fresh matching
after edits and no text/command/save effects from accepting a search result.
It now also preserves Visual character/whole-line selection and a bounded private
editor register: borrowed range/ending-marker painting, original anchor/endings,
copy/cut/change/paste commands and failure/redo atomicity remain app-owned. This
is an incremental existing-case update, not another handoff or SDK adoption claim.

The user prefers a practical foundation such as SDL3 for early Aetheri UI, with
later NexaUI migration where useful. Missing NexaUI features must not hold up the
OS or applications. NexaUI may still be used per application when the necessary
scope is ready; full toolkit completion is not an adoption prerequisite.

Use SDL3 as the preferred platform foundation for early application surfaces.
The first exact dependency/build/license profile and native execution evidence
are now recorded in [Settings](../../../spec/settings.md): SDL3/SDL_ttf plus a private
FreeType/HarfBuzz stack, OpenGL ES and explicit Mesa software rendering in the VM.
The attempted renderer-free SDL Wayland path was unsuitable. This is a measured
development integration, not a proven speed gain over NexaUI or physical-driver
acceptance.
The compositor remains our wlroots-based Wayland server. SDL3 handles client
windows, input and graphics integration; it is not a widget toolkit, compositor,
terminal emulator, shell, browser engine or complete accessibility solution.

Keep each app's data, commands, service clients, validation and persistence free
of SDL/NexaUI types. Put toolkit handles and event/render translation in its UI
module. Do not build a speculative universal widget abstraction or duplicate a
large toolkit in anticipation of migration. A later migration replaces views
incrementally while retaining those application contracts and data formats.

The first [SDL3 desktop consumer](../../../spec/desktop-ui.md) now follows this
boundary: its C17 query/window model has no toolkit types, while the adapter owns
custom Wayland roles, painting and input translation. A fixed development owner
handles actual app processes independently. This is working SDL evidence, not a
completed NexaUI adoption gate or permission to modify its SDK/consumer pins.

Text layout/shaping, IME, editing, accessibility semantics and keyboard focus
remain explicit work. An SDL-only rectangle/text demo does not satisfy them.
Choose the needed text/widget foundations by evidence; preserve reduced motion,
scaling and unsupported-capability behavior. Prefer event-driven idle behavior
and measure CPU/wakeups, frame response and actual memory on the target session.
The user's final visual design remains a separate decision.

### Historical SDK reconciliation — 5 November 2026

The full requirements review updated the existing
[NexaUI catalogue](D:/NexaUI/NEXT_AETHERI_UI.md), retaining all **103 stable IDs
and 13 surface groups** rather than creating another backlog. At that time a read-only
main query returned `d4c9aab9354feb4c9e045b960f4985468bec3253`; its 34-file change
inventory from the previous inspected candidate, selected deltas, public contracts,
selected implementation paths, requirement map and adoption report were reviewed. No SDK build/runtime
test, checkout, consumer pin, publication or other-chat message was performed.

The review recognized multi-window owner-thread Wayland waiting, total-deadline
clipboard sends, initial configure bounds, field-selection restoration and the
Settings in-memory uncertain-operation example. These were implementation/source
evidence with scoped original reports, not full Aetheri acceptance. The inspected
SDK map had all 103 IDs; its application-ownership table omitted the thirteenth
installer/first-run/recovery surface. That was a table gap, not a missing D11 ID.

That review added a category index, explicit composition-lifetime cases and
eight delivery packets to the catalogue. It recommended an installed C17 Settings
consumer on the Aetheri development compositor before private shell and dense-text
integration, preserving app models and process ownership. Those packets belong
to the SDK/adoption backlog; this record does not request another user handoff.
Keep working SDL views until each replacement passes.
Final designs, persistence/product decisions and physical/human acceptance stay
open. The initial start message was delivered; maintain the catalogue from here.

### Earlier cell-grid handoff reconciliation — 5 November 2026

The user's complete-list request was reconciled against a new read-only main
query: `88b4a5633b9b603a34fba103173e2266bc577adc`. The committed cell-grid and
asynchronous Wayland clipboard headers, copied example, requirement map and
adoption assessment were read with `git show`. The earlier inspection below is
historical; no SDK build or Aetheri migration was performed.

The local [complete catalogue](D:/NexaUI/NEXT_AETHERI_UI.md) retains **103 stable
requirements and 13 app/system surface groups**. Its latest reconciliation now
recognizes existing visible-cell drawing/hit tests, copied read-only accessible
text snapshots and a token-based bounded async clipboard reader. These are
reusable foundations, not completed terminal/editor, PTY, IME or human assistive
acceptance. The ordinary-window clipboard API does not establish a borrowed-host
input/clipboard contract. Current SDK source also guards external reads by real
keyboard focus; its reported missing-new-offer focus-cycle case remains open.

Added concrete acceptance for immutable copy exports, independent native peers,
slow-transfer deadlines, byte/FD budgets, source replacement, app-owned paste
validation/review and cancellation without partial command dispatch. Updated the
delivery packets and copyable chat request to reuse current SDK work. Keep each
app model, privileged authority, content engine and process/session ownership
outside the UI toolkit; adopt an independently accepted presentation scope first.
The offline no-AT-SPI profile is not an accessible normal desktop profile.

This is a documentation/source review. Existing NexaUI/consumer changes and the
in-progress Aetheri terminal work were preserved. No other-chat message was sent.
See the [handoff record](../journal/2026-11-05-nexaui-complete-handoff.md).

### Earlier source inspection and SDK handoff — 5 November 2026

A fresh read-only `git ls-remote origin refs/heads/main` selected
`88aeef97f0cd9b90f335e8520cae65570994ba09`. Relevant committed headers,
examples, the requirement map and adoption assessment were read with `git show`.
The local primary checkout remains at
`5f7680f7d00a950f759d17d98520405901f9419d` with existing work; it was not
checked out, built, reset or adopted.

This advances the earlier `120384b` inspection. Public installed Vulkan and
Wayland software views, installed Settings, application-descriptor polling,
a correlated short-lived Settings worker, DE/EN example strings and borrowed
Wayland host presentation/polling now exist. The source map includes all 103
IDs, including D11. Do not repeat those implementations or request the earlier
102-to-103 ID repair again. The preliminary statuses still require individual
acceptance review against the exact candidate and each concrete workflow.

The user-requested full handoff is
[D:/NexaUI/NEXT_AETHERI_UI.md](D:/NexaUI/NEXT_AETHERI_UI.md): **103 stable IDs
and 13 app/system surface groups**, with priorities, source-status boundaries,
observable acceptance, concrete integration cases, delivery packets and a
copyable implementation request. It covers every currently agreed Aetheri UI,
including installer/recovery and the persistent full-desktop experiment VM.
New product choices extend that baseline without renumbering existing IDs.

First reuse the public package/renderer/wakeup work and close remaining Settings
and service-integration gates. Then exercise the actual Aetheri compositor and
private shell adapter, including input/semantic/AT-SPI composition. The borrowed
host example submits an ordinary surface on private Weston; it does not prove
private-role admission, dock/search, three outputs, physical presentation or
normal session authorization. Dense terminal/editor, Files, content-engine and
VM interfaces retain their own acceptance cases. Aetheri's separate fixed
Terminal/Settings development-owner contracts must survive a view migration.

This update is a source/document review and planning handoff, not fresh SDK
build/runtime/hardware acceptance. Original reports keep their stated candidate
and environment. No SDK code, consumer pin, existing edits, other-chat message
or publication was changed. Engines, process supervision, app models and
privileged backends retain their separate responsibilities; working SDL3 views
continue until an individually accepted NexaUI scope is available.

### Primary references inspected

- [SDL3 overview](https://wiki.libsdl.org/SDL3/FrontPage): platform/input/graphics
  scope, C API and zlib licensing; text/font helpers are separate libraries.
- [SDL3 Wayland notes](https://wiki.libsdl.org/SDL3/README-wayland): compositor
  capability boundaries, configure/initial-buffer handling and scaling limits.
- [NexaUI earlier pinned README](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/README.md),
  [build profile](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/CMakeLists.txt),
  [adoption assessment](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/docs/quality/aetheri-adoption-assessment.md),
  [requirement map](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/docs/quality/aetheri-requirement-map.md)
  and [Settings example](https://github.com/Lulus792/NexaUI/blob/120384bcb9d80f9b5a958bab337fbfecbb30ca7a/examples/18_settings/README.md).

## Earlier inspection — 25 October 2026

Inspected 25 October 2026. This is a source/document inventory, not a fresh build,
benchmark, hardware test or security acceptance.

## Evidence

NexaUI is an original C17 toolkit with layout, widgets, text/font handling, themes,
input and accessibility semantics. It has software rendering and Linux Vulkan code.
Its Linux dispatcher creates Wayland or X11 client windows. The inspected build
requires X11, Wayland and xkbcommon development files; Vulkan is a separate option.

This makes it a candidate for Settings, Files, a monitor, editor chrome and other
original app UI. It is not a compositor, browser engine, terminal emulator or shell.

The README describes early version 0.1.0 and remaining text, IME, accessibility,
API and hardware acceptance. Some local documents describe older feature states.
The working checkout has local/untracked changes; do not infer complete coverage
or a releasable commit from one document. Inspected revisions are in [sources](../SOURCES.md).

## Relation to the user's answer

The user accepts NexaUI if suitable and would consider another toolkit for a
significant demonstrated benefit. OS-specific code is not automatically faster.
A matching theme and a thin Aetheri integration layer may meet the product need
without replacing layout, text, widgets and accessibility.

Recommendation: first measure an agreed NexaUI commit in the intended userspace.
Exercise a text field, list, dialog, clipboard, focus, scaling and semantics.
Measure idle CPU/wakeups, input-to-frame latency, frame timing, allocation behavior
and memory on the target laptop. Test CPU and GPU paths separately.

Wayland-only Aetheri would need a deliberate review of the current X11 build
requirement. Supporting our own compositor needs server work beyond the client toolkit.
No change has been made to NexaUI or its other consumers.

The user now explicitly intends Physim to migrate to NexaUI when ready. Record
text/editor, rendering, accessibility and app-specific requirements as migration
gates. Until that separate work is implemented and tested, Physim's SDL3/OpenGL
needs still apply. Do not use the future migration as evidence those dependencies
are already gone.

Latest inspected sources: [public host contract](https://github.com/Lulus792/NexaUI/blob/88aeef97f0cd9b90f335e8520cae65570994ba09/include/nexaui/wayland_host.h),
[adoption assessment](https://github.com/Lulus792/NexaUI/blob/88aeef97f0cd9b90f335e8520cae65570994ba09/docs/quality/aetheri-adoption-assessment.md)
and [requirement map](https://github.com/Lulus792/NexaUI/blob/88aeef97f0cd9b90f335e8520cae65570994ba09/docs/quality/aetheri-requirement-map.md).
