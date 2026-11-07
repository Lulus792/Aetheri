# Native graphical Editor clipboard — 7 November 2026

The graphical Editor now receives clipboard text without blocking its event
loop. It uses the already pinned NexaUI 0.1.23 public Wayland asynchronous read API;
no SDK patch, new library family/version, thread or service is introduced.
The [contract](../../../spec/graphical-editor.md) describes exact input ownership
and remaining normal-session/accessibility/recovery boundaries.

## Original input and lifetime

Each active request retains its native token and original document ID, canvas and
direct-input token (non-reused identity, interaction, revision, caret, anchor and
selection state). Native events/focus frames are drained before inspecting its
completion. A wake is advisory. Drift rejects before take, including a gesture
that leaves the caret at the same EOF. Explicit tab/file/review/focus/close and
Escape paths cancel the request; a second Paste separately replaces it.

Receive is bounded to 8 MiB with a five-second overall timeout and sensitive-buffer
handling. Retire only the owned token, not a filename widget's unrelated read.
Completed bytes have an explicit length and are wiped/released with the SDK's
public release operation. Nonempty text goes through the original direct paste
method as one history group. Empty/failing data preserves the document; UTF-8/NUL
and line/document limits still apply. EOF/completion is never proof that an
external producer sent its entire intended value. No partial timed-out data is
inserted. Cut deletes only after successful native Copy; a failed model deletion
keeps the document open instead of terminating the host.

## Actual native evidence

The same Fedora44 Enforcing carrier remains at boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. Original source/build/tests live in separate
768 MiB work/256 MiB temporary private mounts. App, compositor and independent
libwayland producer/receiver execute as UID1000. No normal source/build/app/unit
or authority is replaced; no reboot, disk format or foreign-helper retirement.

Main run token `fb632fd6b81d4cc7b5f9bf0e7e91dc2a`, helper PID1015355,
host handle46748 terminal0. All 466 inputs are hashed in local `intent-v2.json`;
source archive SHA256:
`cc72f344b4ce8052e71aa3a2b36a8db614632c61cd7f7b3055923f723737af6d`.

| Check | Release | ASan/UBSan |
| --- | --- | --- |
| All nine editor groups, no skips | 3.010 s | 18.319 s |
| Existing native graphical Editor workflows | 8.475 s | 20.579 s |
| Fourteen independent clipboard workflows | 9.600 s | 24.075 s |
| Actual live-partial receive deadline | 5.132 s | 5.004 s |
| Idle CPU ticks over 1.2 s; no redraw | 0 | 0 |

The fourteen cases prove Unicode/ZWJ source bytes survive actual Paste/Save and
one Undo/Redo, an independent receiver reads the real copied text, Cut and
own-source Paste work, unchanged-EOF gestures consume correlation, repeated
Paste retires only its old pipe, and cursor/F6/Escape/new-tab/picker/type/focus
changes close original held readers without replay. The sender's actual EPIPE
confirms reader retirement. A live sender continues partial writes during the
overall timeout; malformed UTF-8/NUL and an overlong resulting line preserve
the original saved bytes. These are clipboard observations, not app lifecycle
or lease-settlement proofs.

The first native attempt completed those initial clipboard interactions but its
idle check began before finite pointer/key-release focus frames finished (3 ticks,
four frames). The fixture now waits for 150 ms of actual quiet before measuring
the same 1.2-second budget (at most 2 ticks and no redraw); continuous repaint
would fail that bounded precondition. The budget/deadline was not enlarged.
Named Save now waits for the original operation result before checking file bytes,
including cases whose file already contained the expected bytes. Additional
same-EOF/repeated-request cases pass. The failed original v1 remains attributed
in its independently verified export, never adopted as a passing run.

All 331 main evidence files are host-hashed before the original root-protected
token/source/archive acknowledgement, then exact private mounts/control retire.
Main archive SHA256:
`5f192cee0833973f5e96ac14c918b95b5755eeeb64148e47b4e3908eff81f80f`
(3,780,716 compressed bytes).

A subsequent capture-only expansion reruns the clipboard suite, without repeating
unchanged backend/window groups. The app's C source is identical. Token
`6a3812a017e34e0ca470856bc544f586`, helper PID1017250, host35040 terminal0.
Both 14-case workflows pass (11.026 s/23.578 s); deadlines 5.036 s/5.034 s,
idle 0 ticks/no redraw. Source archive SHA256:
`45c8f6ec45609a4e319cf703a4f50abf007b14d95e6889785287191b6579837c`.
All 197 additional evidence files are verified before original acknowledgement/
retirement. Visual archive SHA256:
`f76ce8ed3e7c09df612951108eacc49873b348c107cfcf4ceb76fe10749a775f`
(3,597,749 compressed bytes). Local evidence resides in
`local/editor-clipboard-2026-11-07`, including immutable source/intent/export/
verification/dispatch records and native pending captures.

Final host-loop review found that testing the generic dirty flag after clipboard
progress could spin when a dirty drawable is too small to paint. Progress now
explicitly reports an actual receive transition; only that transition requests
another draw before the native wait. This was an inspection finding, not a
claimed executed tiny-window reproduction. Minimum-window acceptance remains open.
The final candidate reruns both native suites in each profile: window workflows
9.054 s/26.326 s, fourteen clipboard cases 11.937 s/23.823 s, deadlines 5.013 s/
5.078 s and idle 0 ticks/no redraw. Unchanged backend groups are not repeated.
Final token `89616fef147749f1a608c929662b6bcf`, helper PID1022267,
host 10034 terminal0; all 259 files are independently verified before original
acknowledgement/retirement. Final executable/test sources match the captured input
manifest. Source archive SHA256:
`336779cd9549049e5e4944e5f4bde7bf5c99d3ba1a5ae2ae31990ce91c08458a`.
Final export SHA256:
`7ebe7a5f9a058cfe313df4336a41bd573cca3a8ae2d8fb3be3d785c755377196`
(3,750,612 compressed bytes). The final receiving image is also inspected.

The actual pending-status image was inspected: document text/caret remain visible
and the footer names the receive. The reviewed mono font displays missing emoji
glyphs as placeholders while saved Unicode/ZWJ bytes remain exact. Full glyph
coverage/font fallback is still an open presentation requirement; the screenshot
does not establish final design or complete native accessible editing.

## Preservation and next work

Read-only audit preserves 445 normal guest source inputs, 31 frozen SQL/XML,
19 quarantined runtime hashes, 52 normal P0 files/controller, old SDK 0.1.18/SDL04
and the original CLOSED DRM history. Four foreign planning/design files remain
excluded. The prepared work disk is unchanged. No new NexaUI catalogue requirement
or chat handoff is created for this bounded consumer adoption.

File reads/saves/enumeration still execute synchronously: next move them behind
responsive original-operation ownership without letting worker completion target
a replaced document/review. Full IME/editable accessibility/search/chrome/reflow/
design, Save As/export/uncertain and abnormal-close recovery remain required.
Normal app/session admission, bootable image/installer, laptop hardware and the
complete experiment product remain unfinished. The original full goal stays active.
