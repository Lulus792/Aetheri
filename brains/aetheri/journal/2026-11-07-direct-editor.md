# Direct entry for the graphical Editor — 7 November 2026

## Purpose and boundary

The user distinguishes the standard non-modal graphical Editor from the modal
editor inside Terminal. Its first native input controller now uses the same
original document/history kernel without routing ordinary typing through Vim-like
modes. Tabs, close review, file pickers and the actual Editor window remain next
integration work. The [contract](../../../spec/graphical-editor.md) and
[header](../../../src/editor/direct.h) define ownership and failure behavior.
There is no new toolkit, dependency family, process, installed API or normal
application admission. The existing modal editor and launch paths are unchanged.

## Implementation

The C17 `aetheri_direct` library adds grapheme movement and half-open selection,
logical-column vertical movement, direct insertion/deletion, line-ending-aware
newlines, undo/redo and correlated paste. It borrows one original document under
one owner thread. All edits retain the document kernel's limits and failure
atomicity. File saving remains with the existing file adapter and original caller.

Process-local identities use a non-wrapping atomic counter, not object addresses.
Every accepted gesture advances an input sequence. Delayed paste must match the
original controller, sequence, document revision, cursor and selection. One paste
is a distinct undo group; replay fails. Input cancellation preserves selection
and vertical goal. The eventual window/clipboard host must call cancellation on
its own context changes; the token does not implement native transfer admission.

## Executed evidence

`local/direct-editor-2026-11-07/` retains the 456-input source archive, exact input
hashes, original dispatch token `e99d8368e2fa4275ae8ccb705d8bec71`, build outputs,
logs and JUnit evidence. A fresh private 768 MiB tmpfs mount namespace supplied
sources, builds, TMPDIR and cache. Both profiles ran as UID1000 in the enforcing
Fedora carrier, without replacing normal sources/builds or filling its root.

Release and Debug ASan/UBSan builds pass all eight selected editor groups without
skips: direct, document, modal, history, search, selection, file and view. JUnit
case names/counts and absence of failure/error/skipped elements are checked.
The recorded group times total 3.023 s Release and 19.728 s Debug. Ninja's actual
direct-object compile flags confirm sanitizer instrumentation in Debug and its
absence in Release. The first complete source builds and tests pass; there was
no failed native candidate to retry.

The direct case exercises seven workflows, including whole combining/ZWJ emoji
and CRLF boundaries, reversed selection collapse, replacement undo, return to a
saved state, short/empty-line vertical goal, stale/duplicate/foreign-controller
paste, owner address reuse and allocation/validation failure preserving input.
Seven existing groups provide affected regression evidence, including local file
save failure paths. These are native controller tests, not graphical-window,
human assistive, full save/close or normal document-grant acceptance.

All 160 exported evidence files (42,130,829 bytes) are individually SHA-256 checked
against the root export manifest. The wrapper completes the export before retiring
the private volume; host verification follows its terminal reply. A later fixture
that requires independent verification before retirement must retain its original
volume until that acknowledgement. The source archive SHA-256 is
`018ccbf9be9da782edffd0d2d5cd2f1e7ac6b1f0cc1cf66f45ad442d671d8ee7`.

The final read-only audit confirms 445 unchanged normal guest source inputs,
31 frozen SQL/XML, 19 old-quarantine hashes, 52 normal P0 files, original authority
image, SDK/SDL inputs and closed DRM history. It finds no remaining owned build
fixture; SELinux and the boot are unchanged, with 77,533,184 root user-available
bytes. The foreign helper remains live. Current CMake/direct/test bytes match
the tested archive, and all 1,610 local documentation links pass. Four foreign
planning/design edits remain excluded.

## Next work

Use this controller in the real graphical Editor with original document-tab and
file lifetimes, the approved unsaved-document review and then the native view.
Do not claim a working graphical app merely from this library. The dedicated
developer disk remains prepared but unattached; the live foreign NexaUI helper
still prevents an uncoordinated reboot. Other OS work can continue independently.
Keep the full OS/apps/image/installer/hardware/experiment goal active/incomplete.
