# Native Terminal pane access

7 November 2026. This joins the original multipane Terminal to independent
native assistive tasks. It adds localized pane descriptions, not another shell,
terminal model, window, toolkit or service. Human screenreader, physical input,
final design, persistent sessions and production app admission remain open.

## Behavior

Each visible shell retains its own original process, PTY, model and working
directory. Its existing native header action selects that same shell. The
associated output group now describes whether it is the selected pane, in
English or German. Other panes explain how to select their header. The host
chooses these strings from its actual selected session; the view copies them
through the existing NexaUI 0.1.18 semantic API. The optional private descriptor
is bounded to 4096 bytes and does not own app/session state.

Selected pane and native window focus are separate. An explicit AT-SPI button
action can select a pane while its window is inactive. That does not activate
the window or admit a reading-focus request. A row in another pane cannot take
reading focus until that pane is selected. An open command menu blocks pane
activation. None of these actions submits PTY bytes by itself.

Hiding a tab retains its original shells but retires its native text objects.
Restoring the tab publishes fresh objects for those same models. A retained old
object cannot focus or read the replacement view. Previously copied client text
is outside the provider's control; the test separately requires the original
server destination/path to return `org.a11y.atspi.Error.Defunct`.

## Native task and controls

`terminal.panes_atspi` runs the ordinary original Terminal against a private
Aetheri test compositor and accessibility registry. Its C17 libatspi client has
no NexaUI/model linkage. It uses standard grouping/button/Text/Component queries
and actions, plus a direct old-path D-Bus read for retirement evidence.

The task:

- Creates two visible original shells in separate working directories and reads
  exact output from both; inactive row focus is refused.
- Selects each pane through its native header, focuses its actual output, clears
  reading focus with the existing shortcut and verifies no shortcut/action bytes
  entered the PTY.
- Executes the same proof filename in both directories after native selection,
  proving input reached the intended original shell.
- Checks modal refusal and explicit background selection separately from refused
  reading focus while the actual window remains inactive.
- Holds an old native row across tab hiding/restoration, changes the original
  model afterward and reads the fresh text through the new view. The old path
  must still be defunct; any local cached text may only equal its original copy.
- Checks English/German descriptions, high contrast and larger text, then
  observes ordered end of all original owned shell pins.

Existing semantic/model tests also check copied descriptions without changing
the disjoint row IDs or content/resize/hidden-view retirement contract.

## Original observations and test corrections

The first ordinary producer already passed native pane activation, text/focus,
modal and lifetime tasks. The initial four affected groups passed Release
(14.75s) and ASan/UBSan (19.86s) at `terminal-pane-native-focused-2l6ghup4`, with
all twelve original executable hashes unchanged. These were consumer tests,
not an SDK repair.

The stronger description task then failed on the unchanged producer at
`terminal-panes-atspi-_wiqx87t`: the native group's Description was empty. Its
exact client diagnostic is preserved. The implementation now supplies that
missing metadata; no SDK/dependency update is required.

Earlier fixture failures are retained rather than attributed to the SDK:

- `qk9rzfr6` sampled an input counter before command admission/output completed.
  Counters belong to each shell; the fixture now waits for exact input bytes
  and actual output/prompt movement for the selected original session.
- `yv6o80yu` incorrectly expected every background action to be refused. The
  existing action contract and actual source distinguish button invocation from
  keyboard focus. The final task verifies the real window stays inactive.
- `65fzuqr_`, `xyfxs_1j` and `ykcz8sh3` exposed invalid assumptions about retired
  client proxies: a failed text getter can return an allocated empty string, and
  the proxy may no longer provide its original application routing. The final
  peer retains its own original D-Bus connection reference and copied name/path
  before retirement, then requires the actual old-path error. This does not
  claim provider control over text already copied by a client.

## Acceptance checkpoint

The new source passes all four focused groups in Release (15.58s), followed by
the other 26 affected Terminal/desktop-owner/direct-editor groups (151.26s), at
`terminal-pane-description-meeca0ik`. ASan/UBSan passes its four focused groups
(21.39s) and 25/26 other groups (243.68s); the first `terminal.multiplex` attempt
fails its no-extra-focus assertion. Its original artifacts are retained.

That failure's native wire trace has no new keyboard-focus transition during
the internal switch. The clipboard keyboard callback reports focus before the
queued SDL focus event is handled. The old fixture could therefore record its
baseline too early. Evaluation of the real failing log prefix demonstrates that
the old readiness condition accepts it, while the corrected condition waits for
the original SDL focus observation. The peer's initial mapping can itself move
focus, so the fixture records its baseline before starting that peer. An initial
correction that recorded it after mapping timed out; that artifact is retained
at `terminal-pane-focus-barrier-n95lwnnb`.

The corrected multiplexer task passes Release (7.51s) and ASan/UBSan (12.60s) at
`terminal-pane-focus-barrier-settled-uelgikd0`. It preserves the original strict
no-extra-focus, closed-transfer and unchanged-pane assertions and the same
deadlines. Together the runs cover all 30 distinct affected groups in both
profiles; they are not described as a new complete 81-group suite run.

A separate compiled C17 negative producer removes only the new description
values while retaining the current view API and SDK. The actual native task
rejects its empty Description at `terminal-pane-description-negative-wolezi8l`.
Its source, compiler/link commands and executable are retained; it is not called
the original published binary. The ordinary producer is unchanged by that test.

The selected SDK remains 0.1.18/e71ee30 and the SDL profile remains private 04.
No SQL/XML contract, policy, Root/User authority or quarantined runtime is
changed. The final read-only audit verifies 394 source inputs, 28 frozen SQL/XML
contracts, nineteen protected hashes with no SQLite sidecars, unchanged old
quarantined intent and exact historical CLOSED DRM state. Both eight-shell idle
samples have zero frames and at most one CPU tick over two seconds. SDK artifacts
and the complete SDL 04 profile retain their exact hashes. No own build/native/
registry actor, Aetheri service or seat0 session remains; the carrier is Enforcing
on tty1. The four independent project/design edits are preserved.

Local continuation/evidence is under `local/terminal-pane-access-2026-11-07`,
including 530 retained exports and 198 verified native images. The ordinary
two-pane view and larger/high-contrast native reading-focus capture were visually
inspected. These are bounded native development results, not final design,
human screenreader, physical hardware or production admission acceptance.
