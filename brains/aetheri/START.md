# Start here

Aetheri is a planned Linux-based personal desktop OS with original applications.
Updated 5 November 2026 for the experimental command shell, SDL desktop/fixed app owner, native clipboard,
Settings/nested input and the separate admission/interruption work. Read STATE before
relying on retained-handle evidence.
The separate [DRM development session](../../spec/session.md) now has actual
virtual-display/input evidence; normal session and lock/resume admission remain open.
The [private desktop connection](../../spec/desktop.md) has native role/focus/
foreign-client tests. The [SDL3 desktop](../../spec/desktop-ui.md) now provides
dock/search, window selection and separate fixed Settings/Terminal/Files launches in the explicit carrier mode.
For repeatable start/status/stop and the native VM window, see the
[development session guide](../../spec/development-session.md).
The [own shell profile](../../spec/shell.md) now runs scripts and explicit interactive
terminal/jobs in the carrier. The opt-in [graphical terminal](../../spec/terminal.md)
now adds a real PTY, native SDL3 input/output, stable text selection and bounded
native clipboard/paste review and an owned SDL preedit view. An optional
[built-in Compose provider](../../spec/compose.md) now has real native and virtual
DRM keyboard-to-shell evidence. The [own line editor](../../spec/line-editor.md)
now provides whole-grapheme editing, bounded in-memory history/undo and literal
Tab completion and history search in the actual terminal, with native PTY and
virtual DRM evidence. The [modal editor](../../spec/editor.md) now has an opt-in
executable with local load/edit/save, keyboard view and explicit save/close/export
choices, now with bounded document undo/redo and saved-state return.
Literal document search now provides reversible forward/backward previews.
Character/whole-line selection now uses an app-owned copied register.
The opt-in [Files development app](../../spec/files.md) now browses held directories,
creates empty files/folders without replacing existing entries, and opens a
selected text file in a separately owned Terminal/editor. It now has
its own fixed development dock/search slot; normal admission and brokered file
access remain separate work.
Ordinary brokered access, durable recovery, block/mouse selection, richer search and
full accessible text remain work.
External IMEs, advanced completion/richer search, panes, persistent
sessions and normal admitted app/session integration remain work.

**NexaUI: handoff delivered; catalogue maintenance only; no pending chat message.**
Add meaningful changes to the [existing catalogue](D:/NexaUI/NEXT_AETHERI_UI.md).
After compaction, follow the [maintenance rule](knowledge/nexaui.md#maintenance-rule--confirmed-5-november-2026)
and resume actual Aetheri work from STATE.

1. [Project](PROJECT.md): product scope and agreed foundations.
2. [State](STATE.md): actual work and publication status.
3. [Decisions](DECISIONS.md): confirmed choices and superseded assumptions.
4. [Architecture](knowledge/architecture-options.md): selected boundaries and alternatives,
   then [module/interfaces](knowledge/modules-and-interfaces.md),
   [identity/lifecycle](knowledge/identity-and-lifecycle.md),
   [schema/API](knowledge/schema-and-api.md),
   [storage/permissions](knowledge/filesystem-and-permissions.md),
   [launch/access protocol](knowledge/launch-access-and-removal.md),
   [first Linux prototype](knowledge/first-linux-prototype.md),
   [versioned contract implementation](../../spec/README.md),
   [P0 runtime contracts](../../spec/runtime.md) and
   [application lifecycle](knowledge/app-lifecycle.md).
5. [Questions](QUESTIONS.md): remaining architecture/product details.
6. [Technology discussion](knowledge/technology-options.md), [NexaUI](knowledge/nexaui.md),
   [private compositor](../../spec/graphics.md), [native Settings](../../spec/settings.md),
   [testing](knowledge/testumgebung.md), [experiment environments](knowledge/experiment-environments.md)
   and [milestones](knowledge/etappen.md).

[Sources](SOURCES.md) identifies original evidence. Inbox holds unprocessed notes,
knowledge holds scoped findings/proposals, journal holds handoffs and archive history.
Open the brains workspace and select aetheri in SecondBrain. Metadata/local links
are checked; no SecondBrain GUI acceptance is claimed.
