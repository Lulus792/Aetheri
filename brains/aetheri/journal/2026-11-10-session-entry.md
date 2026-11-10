# Desktop entry after a local login

10 November 2026. Aetheri now has an opt-in native `aetheri-session` entry that
consumes an existing local Wayland login. It reuses the C desktop owner and its
original-session observer, supports the actual account instead of hard-coded
UID1000, and can run without a systemd notify service. No USB build or flash.

## Boundary and implementation

The maintained login stack remains responsible for authentication and creation
of the session. Writing another privileged password handler would expand the
trusted surface without providing the still-needed greeter/PAM/policy review.
Keeping the fixed test account and notify service alone would not allow a normal
login-manager handoff. The selected entry therefore consumes an already-active,
local seat0 Wayland `class=user` scope and retains its original observations.
It never changes accounts, reads passwords or opens a privileged PAM session.

The entry rejects UID0 and unequal real/effective/saved UID or GID. A bounded
16 KiB NSS lookup must agree with the actual UID and primary GID. HOME/USER/LOGNAME
come from that account; `/run/user/UID` comes from the actual UID rather than
XDG_RUNTIME_DIR. Root-owned non-writable parent directories and the user's real
mode0700 runtime are checked before a fresh private desktop directory is created.
NSS and host runtime/session views remain trusted prerequisites. There is no
runtime-directory creation/adoption fallback or volume/clone authentication.

Default Files documents derive from `NSS_HOME/Documents`, with a 4 KiB output
bound. The entry does not create personal directories or grant file authority.
It sets NNP, then requires empty observed capability sets. It shares the fixed
owner's original session checks, child barriers/pins, launch channel, fonts and
Files-first document shutdown. An absent NOTIFY_SOCKET is valid only for this
entry; a supplied channel still needs successful notification. The original
image-owner profile retains its fixed account and required notification.

No extra dependency, process or thread is introduced by the entry. The existing
systemd/NSS implementation remains the selected host foundation. CMake builds
the two profiles from one source and emits the Wayland session descriptor with
literal `/usr/libexec/aetheri/aetheri-session` Exec/TryExec paths. These assets
are not installed; complete system packaging and normal app admission remain open.

## Executed checks

Use the original restricted audio lab only: UUID
`033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU 5548, boot
`8b915c0c-ab36-4cce-a8a3-e84e1c12627c`, SELinux enforcing 1.
The original vm/p0 quarantine stays stopped. Revalidate before further dispatch.

Final source `/var/tmp/a-session-396d0e6a2289`, work
`/home/aetheri/session-396d0e6a2289`: sixteen standalone compile/run commands
pass across Release/memory. Both entry profiles compile with strict warnings
and GCC analyzer; native session, owner and entry tests pass. ASan/UBSan/leak
detection remain enabled without suppression in memory runs. Controlled entry
tests use a different UID/GID 1201/1301, forged caller home/runtime hints, root and
unequal/saved identities, missing/wrong NSS results, bounds/NNP failures and
absent/successful/failed/empty notification channels. They do not authenticate.

Pinned CMake Release source `/var/tmp/a-session-cmake-20d104dc729b` and memory
source `/var/tmp/a-session-cmake-205022f56f76` each configure, build both owners
and two tests, and pass `linux.session` and `linux.session_entry`. All five
relevant compilation records and memory flags are independently checked, as is
the emitted descriptor. Preserve the first missing-API/header test compile and
the second test-adapter nonnull-attribute warning, with their original sources.
No failed artifact is reused as a passing build.

The deliberate test-only mutant reads XDG_RUNTIME_DIR instead of deriving the
original UID's runtime. It compiles and fails the original-runtime assertion,
exit 134. It is never installed; core dumps are disabled. The independent archive
audit initially expected the wrong CTest summary word order; inspecting the raw
two-test success corrected that audit assertion without rerunning any effect.

## Actual KMS and retirement

Private bound entry ELF SHA-256
`03cd5434fa471bd650f1f86a383b60c1e439a175ab45ea20981b688577b632cf`
is independently downloaded/hashed, then freshly Root-copied into the sealed
program selection. It binds the unchanged protected current runtime and separate
`/run/a-native-doc-ef3394875810` documents. Source `/run/a-ns-f8e93b3f3913`, output
`/root/aetheri-native-desktop-f8e93b3f39134255981da6654eac5763`.

The existing explicit PAM fixture establishes the ordinary UID1000 local session;
Root retains its stopped original wrapper/scope before same-process exec. The
fixture requests Type=exec and NotifyAccess=none; readiness now waits for the
native first-frame record as well as the service. It does not install or test
real password authentication. Native output confirms the actual fresh directory
below `/run/user/1000`, rather than the fixture's caller runtime hint.

Host controller 11603 and input driver 66023 both exit 0; controller reaches
`retired`. Seventy-four acknowledged native input chords open Files/editor, save
exact `hello alpha\n`, write `desktop` to `owner-proof` through the own shell and
open Settings. Viewed captures show the saved editor and four-window desktop.
Actual output is bochs-drm 1280×800/74.994 Hz, backend keyboard layout. The saved
original document closes through input during ordered shutdown; all five direct
children exit 0. Nine original actors and nine checked code-map records end,
the exact PAM fixture is removed, original tty1 returns and selected code is
unmapped. These are VM/development UI observations, not physical acceptance.

## Evidence and next work

Streaming audit verifies all 1,879 exported files and 1,740 submitted inputs,
current C/test/helper/CMake bodies, the bound ELF and actual maps. Native controls
pass 55 without skips; host controls pass 71/skip 16 (87 cases). Packages, original
udev 693/invocation and enforcing 1 remain unchanged; no fixture PAM/local seat or
selected runtime/program mapping remains.

Archive `local/full-desktop-build-2026-11-09/session-entry-evidence-v1.tar.gz`:
7,241,894 bytes, SHA-256
`75e003684028d6584c1a4c2008ac977f0f1dda9a6b81c6a996e322f26e2f27d4`.
Protected guest origin:
`/root/aetheri-session-entry-evidence-4cca960bc767487f9f43345d80d4a727`.
The existing 200-body runtime is independently retained and rechecked.

Next finish normal login-manager/authentication and account provisioning,
service/device integration and complete runtime packaging. Other users and an
actual display manager still need live integration evidence. Trusted lock,
suspend/resume, normal installed app lifecycle and the broader first-version
scope remain incomplete. USB gate stands; NexaUI handoff stays delivered.
