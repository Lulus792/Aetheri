# Native graphical Editor — 7 November 2026

The opt-in `aetheri-text-editor` now has an actual NexaUI Wayland window over the
original direct controller/document workspace. It renders live tabs, workspace,
line numbers, text/caret/selection, a held-location file picker and document-first
Save/Discard/Cancel review. Last-tab close leaves an empty window. Terminal's
modal editor and normal desktop/authority provisioning remain unchanged.
Read the [contract and limitations](../../../spec/graphical-editor.md).

The selected-file workspace extension requires the original held regular-file
witness both when reading a new document and when reusing its existing tab.
An additive read-only selection getter supports rendering during frozen close
review. Neither is a grant, volume proof or production admission API.

## Executed native boundary

The original Fedora44 carrier remains Enforcing, boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. Source/build/test artifacts live in a
separate bounded private mount namespace: 768 MiB work root and 256 MiB `/tmp`.
The latter avoids the already attributed foreign UID quota exhaustion; it does
not fix NexaUI's sparse-buffer failure behavior. No foreign files are reclaimed.
App/compositor/tests execute as UID1000, not root. SDK0.1.23/97a4264 and existing
reviewed graphics prefixes are reused; no dependency family/version changes.

A separate native configure-only fixture verifies the existing Settings consumer
still configures after moving the shared SDK check. Missing SDK, wrong exact
version and wrong source revision all reject at their intended guards. The
negative sources are explicit separate copies; installed SDKs/canonical inputs
remain unchanged. Token `da583fccc5c24f9d81d5d463fada303d`, all four cases pass;
51 evidence files are independently verified before original namespace retirement
(`verified-config-v1.json`). Export SHA256:
`fae2cead4d4671851256f96bf5d324ee24ab58e2cb69f5d535d2129d9068891f`.

Final dispatch token: `852f8a3dcfe94f37a2d52c1568e4b04d`, helper PID1009983.
Host handle73255 is terminal0. All 465 original source inputs are hashed in
`local/editor-window-2026-11-07/intent-v12.json`. Source archive SHA256:
`ac9344fe62b15cfdb254bf8b5c521b0175f2f19b36687b2cd60dc397476ea402`.

| Check | Release | ASan/UBSan |
| --- | --- | --- |
| Strict native configure/build | Pass | Pass |
| All nine editor CTest groups, no skips | 3.053 s | 18.832 s |
| Actual native Wayland workflows | 8.493 s | 22.157 s |
| Idle CPU ticks over 1.2 seconds; no redraw | 0 | 0 |
| Captures across three virtual outputs | 21 | 21 |

The native test checks actual file bytes after direct typing/newline, first Save,
selection replacement, both original undo groups and redo. It checks tab identity/
content retention, Cancel/Discard, empty last-tab view, Save from an unnamed close
review, original selected-file open and external-change rejection without replacing
the external bytes. F6 reaches the toolbar and returns to the same document.

Selected presentation cases are dark100%, German high-contrast200% and light150%.
The test-only compositor `Z 2 200` commits integer density on the real 3840x2160
virtual output; app diagnostics independently report density2.00. It is not an
installed display-setting API or fractional-scale implementation. Default virtual
outputs are density1; resolution alone was not treated as HiDPI proof.

Native maps check the exact installed NexaUI library hash/version/revision and
absence of SDL/Vulkan/EGL/X11 in this app. The read-only text snapshot is not
complete accessible editing. The capture/interaction checks do not claim physical
hardware, human assistive, minimum-window or full accepted-design acceptance.

All 297 evidence files (51,652,498 bytes including the manifest) are independently
host-hashed before the original root-protected token/source/archive acknowledgement.
Only then are the exact private mounts/control retired. Complete export SHA256:
`55953452faa2aa9509a4ec98501d6fbdab801c24949c1ea1b90951e641062218`
(3,569,603 compressed bytes). Local `verified-v12.json`, archive and dispatch log
retain the originals. Both app versions and their compositors exit normally.

Selected native images were inspected: first-save field label/path, document-first
close review with Cancel focus, black selection text on high-contrast yellow and
light text selection on the actual2x output. Layout is still a functional opaque
development presentation; final compact glass/icon/search composition remains work.

## Attributed earlier attempts

Each attempt reached terminal status and exported/verified its own evidence before
retirement; unknown dispatches were not restarted on an observation timeout.

| Attempt | Observation and resulting correction |
| --- | --- |
| v1 | Strict build rejected guessed overlay/snapshot API and misleading indentation; use pinned public signatures, retain warnings. |
| v2 | Both builds/nine editor groups pass; no graphical execution yet. |
| v3 | Test's long Wayland socket path prevented server startup; shorten fixture/socket names and assert AF_UNIX length. |
| v4 | Actual app SIGBUS before first log under exhausted shared `/tmp` quota; pinned source hardcodes that buffer location. |
| v5 | Private `/tmp` permits typing/save; Select All exposes mishandled successful-count query (`NX_ERROR_LIMIT`), corrected without relaxing selection checks. |
| v6 | Native undo returns the expected intermediate replacement group; test had incorrectly expected two groups undone in one action. |
| v7 | Both native baseline workflows pass. |
| v8 | Theme test wrongly required output0 to contain every window; compositor assigns successive windows to successive outputs. |
| v9 | Expanded native theme/F6/Save-close cases pass; no density claim. |
| v10 | New density observation disproves inferred1.5/2; existing outputs all report1.00. |
| v11 | Separate compositor configure lacks its reviewed prefix; select the existing profile-specific graphics prefix. |
| v12 | Actual test-only output2x commit, app density observation and both full native workflows pass. |

Inspection also found the old host loop would route remaining native events
directly into the toolkit after examining just one; direct document input could
be lost. It now renders/handles each original event in order before the next.
Native close observations are latched once. The SDK's missing consumable/cancellable
native close request is recorded in existing NexaUI catalogue W01; no replacement
chat handoff was created. Repeated compositor-close/management acceptance remains
open, as does the known allocator/backing issue already in P04.

## Preservation and next work

The read-only audit preserves 445 normal guest source inputs, 31 frozen SQL/XML,
19 quarantined runtime hashes, 52 normal P0 files/controller, old SDK0.1.18/SDL04
and exact prior CLOSED DRM history. No normal services/app/source/build is replaced.
Foreign design/planning edits are excluded. The prepared work disk remains separate;
no reboot, format, foreign-helper retirement or production policy change occurs.

Next connect responsive original file I/O and asynchronous clipboard cancellation,
then complete direct-editor IME/accessibility, search and native chrome/resize/
reflow/design acceptance before normal desktop integration. Save As/export and
durable unsaved/UNKNOWN recovery remain required. The full OS/apps/bootable image/
installer/hardware/experiment objective is active and incomplete; this app window
does not complete it or the P0 lifecycle/document/removal gates.
