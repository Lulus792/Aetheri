# Shared original terminal session

6 November 2026. The actual SDL Terminal now uses an internal C17 session for
its bounded input queue, original-master flush/output feed, PTY/grid resize and
close-intake fence. This prepares presentation migration while retaining the
actual shell/editor. No NexaUI terminal executable, SDK upgrade, public API or
default replacement is claimed.

## Ownership

The host still owns the initialized model allocation, trusted shell/editor source,
pre-worker PTY creation, readiness handoff, original child/pidfd wait and final
stream/descriptor cleanup. The session holds those original PTY fields and
borrows the model on the owner thread. It is never copied or replaced live.
View destruction cannot settle a process or stream obligation.

Whole-event intake either stages all bytes or preserves the queue, selection
and scrollback. Its counter means admission, not delivery/execution. Flush retains
the exact suffix after partial writes/EINTR/EAGAIN/error; EOF and child exit stay
independent. Close fences intake before the same one-shot hangup, preserving
pending bytes and original handles. The user editor close still asks the actual
foreground editor to Save/Drop/Back. Original composition, paste-review/context,
clipboard ownership/deadlines and SDL worker behavior remain in their owners.

## Checks and original findings

New native C checks use an actual nonblocking stream for exact queue/data bounds,
controlled EINTR/partial/EAGAIN/EPIPE/zero-write paths and the original Aetheri
shell in a real PTY for command output, resize and retained close ownership.
Assertions stay active under Release NDEBUG. The initial unmodified test failed
because FIONREAD on the carrier's AF_UNIX stream counts the original partially
consumed buffer. A separate original-system-call diagnostic reports four 4,096-byte
reads, FIONREAD 20,000 and MSG_PEEK 3,616. The fixture now checks the actual
remaining stream without changing the product budget. Original logs remain.

The focused test passes Release and ASan/UBSan at
`/home/aetheri/build/terminal-session-uagc5o6m`.
Four separately compiled Release counterexamples reject a one-byte capacity
change, a corrupted partial-write suffix, a removed closing fence and an
increased read budget at their exact expected checks; the original control
passes. Sources, compiler commands, program hashes and logs are retained under
`/home/aetheri/build/terminal-session-counterexamples-fmuplrp0`.

All 65 final app/UI groups pass Release (244.75s) and ASan/UBSan (416.26s), without
skips, at `/home/aetheri/build/terminal-session-full-4sx3exx_`. This includes actual
PTY/shell/jobs, modal editor, clipboard/review/IME, Files create/rename/move and
direct-editor ownership, both NexaUI views, development-owner startup/reuse/ordered
retirement and compositor/Compose cases. No source/test changes follow that run.

The evidence collector initially expected complete JSON in JUnit system-out;
CTest truncates several old success records at 1,024 bytes. It now reads those
same completed cases from their original full LastTest.log, retaining both forms.
No test is rerun or inferred from the truncation. Local execution records:
`local/terminal-session-2026-11-06/`. The separate final audit records source,
native evidence and the unchanged protected-state boundary.

The collector verifies 369 host/snapshot/guest inputs, 392 exports/264 native
images, unchanged 28 frozen SQL/XML and nineteen protected hashes with no database
sidecars. The normal terminal contains the shared-session symbols but neither
the event-failure nor write-fault hooks. The session archive has no SDL/TTF/NexaUI
dependency. Native shell output, composition, German 200-percent paste review
and the original Files/editor desktop images were inspected. This is regression
evidence for the existing opaque view, not final design/physical acceptance.
All build/private-registry actors ended; the enforcing carrier is idle on tty1
without an Aetheri service or seat0 session. Closed DRM records and foreign files
remain unchanged; no new DRM session or NexaUI catalogue/handoff was created.

## Remaining work

Connect the existing NexaUI cell grid to this real session and preserve native
input, composition/review and current app/editor behavior through that migration.
The SDK's examples are useful references, not replacement applications. Aetheri's
accepted visual design, assistive/physical acceptance, panes/plugins, image/PDF
viewing and the full OS/apps/image/installer/update/experiment-desktop objective
remain work. The old quarantined lifecycle runtime remains unresolved.
