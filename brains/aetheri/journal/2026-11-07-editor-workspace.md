# Graphical Editor document workspace — 7 November 2026

The graphical Editor now has a native C17 coordinator for original document
tabs, direct input, local file saving and explicit close review. This continues
the separate non-modal editor requirement; it does not modify the modal Terminal
editor or provide a graphical window yet. The [contract](../../../spec/graphical-editor.md)
and [header](../../../src/editor/workspace.h) define ownership and the remaining
UI/permission/recovery boundary.

## Behavior and ownership

Up to eight tabs own their document, direct controller and optional local file
adapter. Process-local non-reused direct identities select original tabs; they
are not durable or native-grant IDs. Opening an unchanged existing source selects
the original tab rather than discarding unsaved content. The additive read-only
file-match method checks its original held parent/name and baseline; drift fails.
It is local duplicate selection, not volume authentication or an atomic CAS.

Untitled first saves are create-only. Before an eventual picker, a token captures
the original document revision, input context and close-review serial. A delayed
result after edit/focus/review changes cannot create or overwrite a different
document. Known failures keep the document untitled. UNKNOWN retains the exact
file/name/recovery context. Named saves use the original file adapter and its
existing metadata, original-content and durability checks.

Close review freezes editing/switching and names the exact document/revision and
non-reused review serial. Cancel keeps it; Discard never deletes a file; Save
requires actual success. A completed explicit first save can resolve the original
review without writing again. Unknown save outcomes block completion even for
clean text. Destruction is mechanical process-memory/FD retirement, not successful
UI close or file recovery. Complete export/recovery and the native window remain
next work. No new dependencies, permanent process, public API or policy is installed.

## Executed evidence

`local/editor-workspace-2026-11-07/` retains the 460-input source archive,
hashes and original token `d8b89aac5ddb4da89f97571bce994c9e`. A separate private
768 MiB tmpfs supplies source/builds/TMPDIR/cache. Native tests run as UID1000 in
the enforcing Fedora carrier; normal sources/builds are not replaced.

Both fresh builds pass all nine editor groups without skips: workspace, direct,
document, modal, history, search, selection, file and view. JUnit names/counts and
zero failure/error/skipped elements are independently checked. The recorded test
times total 3.565 s Release and 18.531 s ASan/UBSan. Actual Ninja compile flags
confirm workspace instrumentation in Debug. No failed native candidate was retried.

The workspace case exercises eight workflows: bounded tab creation/identity and
empty state, duplicate-source reuse and independent tabs, actual create-only saves,
close/save/cancel/discard correlation, external-save conflicts, unknown outcomes
even for clean text, allocation/ENOSPC failures and stale picker results. Tests
use real local files and a post-rename fsync error; committed bytes are observed
without treating that observation as permission to repeat or settle UNKNOWN.
Fixture documents, including the uncertain result, remain in the evidence export.

The host checks the complete archive and all 186 member hashes (43,455,723 bytes)
while the original private namespace remains alive. A separate protected original
token/source/archive acknowledgement then permits its normal retirement; the
original producer reports terminal success. Archive SHA-256:
`03fab6d3069c68025bac593e5498d1ae6997bd1223c55398febcdd932714f49c`.
Source archive SHA-256:
`eb82983fb737ae96049629d209897260d9c577a1ca00201da943c2d1703ffdfc`.

The final read-only audit preserves all 445 normal guest source inputs, 31 frozen
SQL/XML contracts, 19 quarantined-object hashes, 52 normal P0 files, original
authority image, SDK/SDL inputs and closed DRM history. Both original private
fixture paths are absent after acknowledged retirement. SELinux and boot are
unchanged; root user-available space is 77,479,936 bytes. The foreign NexaUI helper
remains live. Current CMake/file/workspace/test bytes match the tested archive;
all 1,617 local documentation links pass. Four foreign planning/design edits
remain outside this change.

These are native backend/local file tests. There is no claim of rendered tabs,
actual file-picker admission, human assistive acceptance, asynchronous save
hosting, production recovery or a bootable laptop image. The full OS goal remains
active/incomplete; native graphical presentation is the next Editor step.
