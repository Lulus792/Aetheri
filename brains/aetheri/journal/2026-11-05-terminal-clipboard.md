# Native terminal clipboard and paste review

5 November 2026. Continued the active full Aetheri implementation goal after the
complete NexaUI documentation handoff. This checkpoint does not complete the OS.

Implemented `src/terminal/clipboard.{h,c}` on SDL's actual Wayland connection/queue
and toolkit-free `paste.{h,c}`. The existing worker observes readiness and deadline
only; native IO, ownership and validation stay on the UI thread. Source snapshots
survive selection edits and source replacement. Transfers have explicit byte/FD/
object/memory/IO limits, absolute deadlines and terminal cleanup. Focus loss drops
old offers; cancellation retains the unacknowledged-receive barrier. No additional
daemon, native reader, dependency family or installed authority/capability exists.

The app accepts only a fully validated <=4096-byte snapshot. Newlines/tabs require
an escaped, fully scrollable review with default Cancel, fresh confirmation and
current context rechecks. No partial rejected input or accepted-paste replay is
allowed. Close stops/joins readiness before destroying native borrowed resources,
then retains the original shell/PTY end obligations. Full IME, editor, panes,
plugins, durable sessions, accessibility and normal app admission remain work.

The twelve affected groups pass Release 54.84 s / ASan+UBSan 62.06 s. Native
clipboard has eleven cases per profile and six images; existing terminal has
17 cases/33 images, desktop 13/27, actual owner 7/6. All 144 raw hashes verify.
The original full logs/JUnit are under
`/home/aetheri/build/terminal-clipboard-check-h4qdq139`. Final clipboard repeats
after a file-content readiness fix pass 12.91/15.86 s. The pure test subsequently
compares defined fields/text instead of struct padding and passes both profiles.

Current native roots:

- Release: terminal-clipboard-6hp18be7, terminal-ui-rk8dt1cw,
  desktop-ui-fiv8box_, desktop-owner-3dt6_m10.
- Debug: terminal-clipboard-x9j9zs94, terminal-ui-6r7ywzld,
  desktop-ui-faywo95a, desktop-owner-ixbn8xkg.

All belong to `/home/aetheri/build/session-<profile>`. Copied evidence, hashes
and original logs/images are indexed in
`local/terminal-clipboard-2026-11-05/native-evidence-index.json` (244 copied files).
Other source/physical/human claims are not inferred from these fixtures.

A native mutation that renews receive deadlines on each byte fails after real
trickled IO at the unchanged total-deadline assertion (7.119 s). Original mutant
source/logs remain under clipboard-deadline-negative-80bke9wa. The first negative
attempt used a 111-byte native socket path and failed before compositor readiness;
it is not deadline evidence. A second attempt saw a created output file before
its bytes were written; the harness now waits for the exact expected contents.
Both originals are retained. The initial main test also rejected stale synthetic
event timestamps: the private driver now uses the real monotonic millisecond
clock, preserving the production fresh-confirmation guard.

Actual final Debug DRM/QMP token e8c394ed63c54e9c81dc4c6e2bbdea74 uses original
PID42474/invocation05776b91b6b749be8db305abeaf6d894. Both 100/200% native clipboard
reviews cancel back to exactly matching original pixels. Twelve console PNGs form
six stable pairs. Original compositor/desktop/terminal end status0; the controller
record is closed. Stdout stream d0dea03917bf4135914207433a9300c3 is bound to
session-827.scope. The same terminal binary is present in the independent native
result. Native DE/high-contrast and DRM 200% images were inspected for legibility,
controls and content bounds. No physical, Windows-host input, human assistive,
production login or final product-design acceptance follows.

The protected old running/recover-handoff attempt stays unchanged: nineteen
catalogue/static/profile hashes pass without SQLite sidecars. Frozen contracts,
source consistency, idle carrier state and local documentation links receive a
separate final audit before publication: 270 host/snapshot/guest source hashes,
28 frozen SQL/XML and 872 local file links pass. The guest is enforcing/tty1 with
no known native app/test process, running Aetheri service or seat0 session. See
`local/terminal-clipboard-2026-11-05/final-audit.json`. No other project or chat was modified
during this terminal work. Next: full text input/IME and original editor/pane
models, normal admitted sessions/apps, continued image/app/experiment delivery.
