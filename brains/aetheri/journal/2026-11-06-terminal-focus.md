# Original-content reading focus in the Terminal

6 November 2026. Implemented and validated after the resize correction.
Full product, screenreader and hardware acceptance remain open.

The NexaUI Terminal now exposes focusable read-only output rows. A native
Component focus request selects that original row in an already active window;
it does not acquire another window's input focus. Ctrl+Shift+R clears reading
focus, or focuses the visible input row (the first visible row in scrollback).
The original shell/editor still receives ordinary input. A blue row outline and
short footer identify reading focus without adding another editor or caret.

Row semantic IDs now derive from the original content-incarnation token, with
a checked reserved-ID offset. Moving an unchanged row retains its ID and focus;
editing/replacing it retires the old node. Missing/disabled regions clear focus
at frame end. Exhaustion rejects instead of wrapping or reusing an ID. One
original model lifetime belongs to a view; replacing/reinitializing that model
requires a fresh view. These are local presentation identities, not authority.

Read focus is disabled during preedit, paste review and closing. No native text,
caret or selection mutation is enabled. The existing context and worker remain;
there is no new native window/thread, PTY input owner or permanent poll timer.
The pinned SDK rejects native focus requests while the Wayland window is not
already active. Native window acquisition needs a separately reviewed activation
path, rather than an unverified claim from an accepted raise request.

## Evidence

The view test checks focus/outline, unchanged model bytes, original-row movement,
retirement of a queued old target after editing, disabled focus and ID exhaustion.
A separately compiled positional-ID counterexample fails the precise movement
check while the unchanged control passes. Sources/libraries are untouched by that
private counterexample. Record: `/home/aetheri/build/terminal-focus-negative-ffdifdrj`.

The independent native client additionally sets row focus, checks actual prompt
caret 9, rejects focus while the window is inactive and retains a native Component
before its row is replaced. Subsequent stale focus is refused; an ordinary shell
command still writes exact `ready` bytes. The keyboard shortcut sends no PTY bytes.
The two focused groups pass Release 8.42s and ASan/UBSan 10.72s at
`/home/aetheri/build/terminal-focus-probe-ja_5d5zl`.

The first native test queried the Component interface after the row was already
defunct; it now retains that original interface before replacement. A second run
hits the earlier untraced FRAME/ACTIVE issue while deliberately querying the
wrong focus repeatedly. The negative now follows an independent inactive-window
observation and makes one immediate contradictory check. The positive acquisition
deadline is unchanged. This does not establish the exact old wire failure's cause.

The host also now routes before its final SDK prepare call: descriptor, write
interest and deadline are borrowed only after other bridge operations, following
the public API's invalidation contract. Only then is the readiness object
duplicated. This is a separate correctness improvement, not proof that the old
untraced failure was caused by that ordering.

The first whole-source Release suite at
`/home/aetheri/build/terminal-focus-final-hqsdgbf1` ends 73/74 in 305.94s. Its
native client fails a repeated ACTIVE query after focus return; sanitizer
execution does not start. That run lacks the direct failing wire reply, so no
SDK or host root cause is established from it. Originals remain unchanged.

New diagnostics compare a false public ACTIVE query with a direct D-Bus GetState
reply on the same native object/connection. The peer additionally makes a
bounded 128-query active-state stream. This is test-only use of the existing
reviewed libdbus family, not another product dependency or transport fallback.
An optional wait/push trace exists only in the existing failure-fixture binary.
The instrumented fixture and the ordinary producer each pass the new workflow;
the ordinary Release case takes 8.40s. A trace-pass does not explain the older
untraced failure. The reading footer also preserves nonfatal status/error text.

The whole-source app/UI suite before the final resize correction at
`/home/aetheri/build/terminal-focus-wire-final-xmxm32bn` passes all 74 groups
in Release (306.10s) and ASan/UBSan (543.28s), with no skips. Runner wall times
are 306.11s and 543.30s. The same original host handle was observed live, then
terminal with exit zero; it was not restarted after an observation timeout or
the user's intervening NexaUI status request.

The corresponding native artifacts are `terminal-atspi-q4ppeoon` (Release) and
`terminal-atspi-39g0ejwx` (ASan/UBSan), under their corresponding session build.
Each runs thirteen independent peers, including expected rejection of the live
wrong-focus assertion and disabled bridge; all eleven positive peers exit zero.
The bounded 128-query ACTIVE stream also passes. Both actual native samples
record zero CPU ticks/new frames across two idle seconds. This does not explain
the earlier uncaptured getter failure or establish full human reading behavior.

That audit joins 375 host/snapshot/guest source inputs and 262 exported records,
including 90 native images, unchanged 28 frozen SQL/XML contracts and nineteen
protected hashes without sidecars. It includes the separately compiled identity
counterexample, optional trace-pass and original native failures
`terminal-atspi-924kzkoc`, `terminal-atspi-gqsfa2i8` and
`terminal-atspi-8jvfmot9`. The enforcing carrier is idle on tty1 without build/
private-registry actors, running Aetheri service or seat0 session. Existing
closed DRM history, old pending runtime and foreign files are unchanged; no new
DRM run. SDK 0.1.18/library hashes and production contracts remain unchanged.

The real Release captures were inspected: the blue outline surrounds the original
Unicode output row, the short reading footer is visible, and later ordinary
typing produces the expected command and returned prompt with no retained stale
outline. The second capture shows the German ordinary footer after reconnection.
The chosen font still has a missing-glyph box for CJK despite exact accessible
text; font coverage and accepted compact/glass chrome remain open.

## Resize review and current validation

Final review found that `ae_terminal_resize` can preserve a content token while
changing the projected row. The first focus implementation therefore did not
retire all targets promised by the specification. The view now advances its
checked semantic-ID range above every previously emitted ID whenever model
dimensions change. Returning to an earlier size cannot revive an earlier node.
Ordinary unchanged row movement retains its current identity. Checked arithmetic
still rejects exhaustion rather than wrapping; the original model stays untouched.

The view regression resizes narrower and back, rejects each previous target and
checks that neither the old focus nor its outline returns. At
`/home/aetheri/build/terminal-focus-resize-negative-1d_rd5im`, the unchanged control
passes and separately compiled positional-ID and fixed-resize-offset mutants
both exit one at their expected movement/outline assertions. The earlier
`terminal-focus-resize-negative-tgb6aa12` wrapper expected a later ID assertion,
but its resize mutant had already correctly failed the earlier no-outline check;
the original logs are preserved. Only that diagnostic expectation changed.

The final whole-source suite at
`/home/aetheri/build/terminal-focus-resize-final-fgjjjb29` passes all 74 app/UI
groups in Release (324.26s) and ASan/UBSan (530.46s), without skips. Runner wall
times are 324.27s and 530.48s. The same original run was polled through terminal
exit zero, not restarted on an observation timeout. Earlier complete-suite
results remain attributed to their original source.

The final audit joins all 375 host/snapshot/guest inputs and 292 exported
records, including 90 native images, both current counterexamples and the
previous complete suite and original failed runs. Final native cases are
`terminal-atspi-1b4gdyf3` and `terminal-atspi-isqykrn8`, each with thirteen peers
and the two intended negatives. Each observes zero CPU ticks/new frames across
two idle seconds. Both SDK libraries retain their pinned hashes. Nineteen
protected hashes, 28 frozen contracts, foreign files and exact closed DRM
history remain unchanged without sidecars. The enforcing guest is idle on tty1
without build/private-registry actors, running Aetheri unit or seat0 session;
the old pending runtime is untouched.

Final Release focused-row and sanitizer post-reconnect German captures were
also inspected: the outline/footer and original shell output remain correct.
The previously noted font, native-query diagnosis, activation, complete assistive
and final design/hardware limits remain open.

The existing NexaUI catalogue receives a scoped focus evidence delta only;
all 103 IDs/thirteen surface groups and the closed handoff are retained.
Local records: `local/terminal-focus-2026-11-06/`. Complete human/Orca reading,
selection/text geometry/events, native activation, IME/review semantics and final
design/physical acceptance remain open. SDK/default/policy/SQL/public lifecycle
contracts and the quarantined old runtime remain unchanged.
