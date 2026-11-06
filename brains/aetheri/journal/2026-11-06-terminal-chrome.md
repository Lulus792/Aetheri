# Compact Terminal window controls

6 November 2026. Implemented and validated in the dedicated enforcing carrier.
Final visual, physical and normal-session acceptance remain open.

The own NexaUI Terminal uses the accepted studio's left-hand window controls,
quiet title and no tab strip for its single session. The native opaque fallback
has a 32-logical-pixel header, growing to 38 at 200-percent terminal text. Its
22-by-26 button targets leave space around the 12-pixel dots. The studio's final
28-pixel/glass material is still a visual integration target; this checkpoint
does not claim an exact material or physical display match.

Close retains the original shell/editor Save/Drop/Back, hangup and child/stream
waiting rules. Zoom requests maximize or restore according to observed SDL
flags; Ctrl+Shift+M follows the same adapter without PTY input. Later native
state, not the request return value, changes the accessible Zoom/Restore name.
The adapter refuses zoom while closing, composing, reviewing paste or fullscreen.
Fullscreen remains separate. Minimize is visibly and semantically disabled:
the current compositor advertises maximize/fullscreen only and has no complete
hide/restore lifecycle. No unsupported minimize request or hidden background
session is invented. The default SDL presenter and all production APIs stay.

Pointer activation requires release over the originally pressed control.
Mismatched releases and a body-origin selection cannot activate another button.
Escape consumes its own release and cancels the pending button; focus, pointer
leave and geometry/state changes retire pending presses. The existing SDL native
hit-test keeps these button targets out of caption/edge gestures. No extra native
window, connection, worker or permanent timer is created.

Chrome title text uses a smaller logical size through the original borrowed
font/label cache. Cache keys now include point size, so another size cannot change
an existing label's pixels. The original cell grid, model, PTY, selection,
clipboard, IME and document controller remain the content owners.

## Evidence

The view checks eight text/density/locale/contrast/editor cases, geometry and
disabled actions, stale queued zoom refusal and Close delivery. A separate view
renders the same body label without chrome; the pixels must match despite the
other label size. Independent libatspi clients additionally invoke Zoom/Restore,
observe actual state, reject unavailable Minimize and retain text/focus/reconnect/
Close checks. Native input tests exercise pointer zoom, keyboard restore,
mismatched/escaped/body-origin releases, unchanged input counters and actual
left-button Close with original shell end.

The first focused Release run at
`/home/aetheri/build/terminal-chrome-probe-55lwevmf` ends 2/3. The newly opened
Close test window maps to output 1, while the test clicks output 0: the app records
focus loss rather than a button click. The compositor's actual MAP record and
round-robin placement establish the fixture error. The driver now uses that
observed home output with the existing known fixture layout. All test actors
are dead afterward; the original failed logs remain unchanged.

The corrected three-group run at
`/home/aetheri/build/terminal-chrome-probe-mjf_lo8b` passes Release 29.29s and
ASan/UBSan 36.13s, without skips. Runner wall times are 29.31s and 36.14s.
The subsequent body-label reference strengthens the view test and is included
in the final source validation below.

Separate compiled counterexamples at
`/home/aetheri/build/terminal-chrome-negative-m_1dtt1r` preserve the actual SDK
and shared build sources. Removing the label point-size key fails the exact
body-pixel comparison; the unchanged view control passes. Removing original-
button release matching closes the native shell without PTY input. The driver
rejects this run, and its original app log independently records closing and
clean status-129 hangup. The earlier `terminal-chrome-negative-51mh9zpp` wrapper
expected the failure at an earlier assertion; actual asynchronous closure is
observed at the later body-origin selection check. Those originals remain.

Final full app/UI validation at
`/home/aetheri/build/terminal-chrome-final-_rtszmuz` passes all 74 groups in
Release (325.80s) and ASan/UBSan (543.66s), without skips. Runner wall times are
325.82s/543.68s. The same original run was observed through terminal exit zero;
no restart followed an observation timeout. Native assistive cases
`terminal-atspi-i9cwsfiu` and `terminal-atspi-3qqhr940` each run sixteen peers,
including the two intended negatives and actual Zoom/Restore/unavailable
Minimize. Both samples have zero idle CPU ticks/frames across two seconds.
The full native terminal workflow also has zero new idle frames, with 0.01s
Release CPU and 0s sanitizer CPU in its two-second sample. These are scoped
software measurements, not physical performance acceptance.

Final review found the diagnostic link had retained a shared linker dependency-
file output. The original command and before/after metadata are preserved. A
normal link regenerates that metadata with a byte-identical tested executable
and unchanged object/static-library hashes; the helper now redirects future
diagnostic dependency output privately. No product source or SDK byte changed,
so the same complete-suite results still apply. The repeated read-only audit
checks this reconciliation as well as the actual current product hash.

That final audit joins 375 host/snapshot/guest inputs and 253 exported records,
including 96 native images, both counterexamples and original failed runs.
Nineteen protected hashes, 28 frozen SQL/XML contracts, foreign files and exact
closed DRM history remain unchanged without sidecars. No new DRM run. The
enforcing carrier is idle on tty1 without build/private-registry actors, running
Aetheri unit or seat0 session; the old pending runtime stays untouched.

The real Release normal, zoomed and German high-contrast 200-percent captures
were inspected. The small title, left dots, separator and enlarged title fit
their header; original shell output/selection is intact. This is the opaque
fallback, not a final studio/glass match. The existing NexaUI catalogue receives
only scoped evidence, preserving all 103 IDs/thirteen groups and closed handoff.

Local evidence is under `local/terminal-chrome-2026-11-06/`. Startup mark/system information, real tabs/
panes, compositor minimize/fullscreen integration, final glass/visual/human/
physical acceptance and normal app admission remain work. The older untraced
native-state query remains unresolved; this is not a repair claim for it.
Preserve the old quarantined runtime and foreign files. The full Aetheri goal
remains active and incomplete.
