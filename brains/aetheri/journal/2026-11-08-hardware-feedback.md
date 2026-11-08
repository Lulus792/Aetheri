# HP laptop retest and desktop regressions

## User-reported hardware result

On 8 November the user reports completing the renewed USB test on the HP
EliteBook 840 G4. The desktop starts; creating and editing files works. Files and
their contents survive the test's later start. They appear in Files, but an
external change is not displayed until an action such as creating a file inside
Files refreshes the view. The language change works. This reports interface
language, not separate proof of a German keyboard map.

`cat` shows no output and returns to a new prompt. The exact command and file
bytes are not supplied. A source review identifies a concrete matching defect:
the next editable shell prompt starts with CR/erase-line. Output without a final
LF is erased. Reproduce that case using a real file, shell, PTY and terminal
model, and check after the next prompt/redraw. Do not claim it proves the exact
contents or cause of the user's command without additional evidence.

WLAN, Bluetooth and audio were absent from this image as expected. This result
does not establish support for them, suspend, external screens, Secure Boot,
installation or the normal admitted app/session system. No hardware log or
read-only stick extraction accompanies the report. Treat it as user-reported
boot, desktop, file editing/persistence and language evidence.

The completed renewed flash is the explicit-boot-manager v2 source SHA256
`4d8e9b00bf5594ed4037421d645667804eee71d1ea475081731da17b1a172da9`.
Its write/readback receipt remains immutable, including its then-correct
`physical_boot_accepted=false`. Record this later report separately; never
rewrite historical evidence. The stick now contains the user's test documents.
No new flash is authorized by this report.

## Scoped correction

Preserve unterminated foreground output before the next editable prompt with a
tty-only newline. It also gives an already terminated command a blank separator.
Noninteractive output and redirected child bytes remain exact. This is a
reversible development presentation choice, not final shell design approval.

Use one nonblocking, close-on-exec inotify instance per Files model. Subscribe
to the current held directory before scanning. Events invalidate a bounded
snapshot; names, cookies and event counts are never filesystem authority or a
mutation journal. Overflow and stale watch events also request a rescan.
Navigation switches the watch only after a successful scan. A failed scan
retains the previous view/watch. Refresh retains the selected exact
name/device/inode/kind when still present, plus the current filter.

SDL reuses its existing signal wake thread; NexaUI combines its original wake
pipe and inotify FD through epoll. Neither introduces a daemon or periodic
directory scan. Coalesce changes and defer refreshing while a pointer press,
menu/dialog, worker or uncertain operation owns the view. A bounded read leaves
remaining notifications readable. Close original threads before destroying the
model/notification FD. These are ordinary-user development observations, not a
document grant, security boundary or production reconciliation protocol.

Linux documents that notifications may coalesce or overflow and that watching
directory children has hard-link, mmap, network-filesystem and mount limitations.
Manual refresh remains necessary as a fallback for changes outside this local
ext4 notification profile. No general recursive/file-watching service follows.
Source: [Linux inotify manual](https://man7.org/linux/man-pages/man7/inotify.7.html),
reviewed 8 November 2026.

## Executed native evidence

All new native work runs as UID1000 inside fresh private tmpfs/mount namespaces
of the already running enforcing carrier, boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. No package, policy, original carrier
source/build/service or physical medium is replaced. The existing reviewed
SDL/text/wlroots and NexaUI0.1.23/97a4264 prefixes are used read-only.
Each export is checked by SHA256 and every member hash before acknowledging
that exact original helper's retirement. Original host process exits are observed.

Evidence is retained under `local/hardware-feedback-2026-11-08`:

- v1 exports247 members, helper1188014/token
  `c5e941cbbbb34869806669314f881248`, then exits1. The initial shell separator
  was placed outside editor mode, causing the existing mode guard to reject it.
  Move it into readline after entering the original edit mode; do not weaken
  the guard. The Files model/notification tests already pass in this attempt.
- v2 exports460 members, helper1189530/token
  `785e78990e29498f9173a1afba1a599a`, then exits1. All17 selected model/shell/PTY
  groups and SDL Files pass; the added NexaUI test incorrectly requests a
  nonexistent frame field. Use its actual snapshot revision. The old prompt
  countercheck already fails on the missing unterminated text.
- v3 passes17 model/shell/PTY groups plus3 native Files groups in each of Release
  and ASan/UBSan. Helper1191187/token `2a3abfe06ce948ccba2a6c953fb2b4f5`,
  1112-member exportSHA256
  `b6eaa5e2d162b9d4f9bc144c53a0005dab68c52cfb0176b4b376b5f75f9f0d60`,
  original exit0. Two exported SDL/NexaUI native initial captures are actually
  viewed; their respective raw SHA256 values are
  `60f79f38d2011f9d53333ebc6e71ffa089d1f36ea678fe2ababeac64157af4ce` and
  `799b7225d23c037703265f1580e681639a84b7f00a5c445b4e1a9c3a5e1df2bd`.
- v4 independently substitutes the Git0fe41d0 readline or SDL Files view in the
  private build. The new real-PTY test detects lost text under the old prompt;
  the new idle native Files test detects the old view's missing update. These
  are precise negative results, not crashes or acceptance of an old full image.
  The final PTY test now drains/reaps/closes its original child before making
  the deliberate negative assertion. Restored Files and terminal tests pass;
  terminal also passes ASan/UBSan. Helper1194220/token
  `9247eea6fdeb446faa6bfdf0e1eea247`, 359-member exportSHA256
  `562c9be1c9effd436f70c9c94bc0e1cd44f8796e568c046c9e531d07457949bb`,
  original exit0. A separate read-only process enumeration observes no surviving
  executable from the retired v1/v2 namespaces.
- Final v5 includes the original native press/cancel/focus state in the NexaUI
  refresh fence. Its native test waits for app-side press acknowledgement and
  deferred invalidation, proves the old snapshot stays until release, then
  observes the new file without another action. All20 groups pass in Release
  and20 in ASan/UBSan, including both native views and real uncertain-operation
  fixtures. Helper1196125/token `f99ec9bab63d4247b64f7f84b7be708b`,
  1109-member exportSHA256
  `ff4a910dcf14af6c43bb60fadacc08b150fb8b5d8b85e58928f9545e1caee026`,
  original exit0. All549 normalized bound input files match the checkout before
  this final evidence-documentation update; native sources are unchanged.

The notification model case uses a real external writer process, checks idle
FD readiness, modified size, filter and selected-object retention, modal
deferral, renamed held directory versus a replacement at its old path, a burst
larger than one bounded read and FD cleanup. No recursive watching, durable
filesystem identity, overflow recovery journal or production holder settlement
is claimed. The original-code controls are independent regression evidence;
the user's exact cat invocation/file bytes remain unknown.

The final protected-state audit retains445 source inputs,19 protected hashes,
31 frozen SQL/XML files,52 normal P0 inputs, the original authority hash and
CLOSED DRM history. Foreign tracked project notes remain byte-identical. The
UI baseline note changes independently in its own chat and is neither edited
nor staged here. The verified physical flash receipt and user test data stay
untouched. These corrections still need a later independently accepted image;
they are not retroactive changes to the user's completed hardware test.
