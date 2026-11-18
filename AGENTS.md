# Working on Aetheri

Current signed WLAN desktop: read the last section of the 2026-11-18
desktop-iwd journal. Two cold boots agree on enforcing readback 6017642b4d44…5d7a.
Preserve the format-5 rows except the effective policy and use the reviewed
VM signer. The old selection fails in dadf504c. c8777115b7b6/50558f29 passes
two confined iwd cycles, original graphical PAM login, visible desktop,
normal logout and ACPI shutdown. Keep the VM-only observer and package
receipt scope. Next authenticated WLAN control, WPA/DHCP and Settings.
Preserve IPv6 accept_ra evidence and the USB gate.

Current WLAN confinement: read the SELinux section of the 2026-11-18
desktop-iwd journal and `spec/login-deployment.md`. Select `--desktop-wlan`
and apply the dedicated exec/config/runtime labels. Enforcing headless
37ec642aca02/d06ad76c passes two iwd lifetimes, radio interface discovery,
runtime cleanup and ACPI shutdown. Keep the typed watch, inherited journal
stream and exact SIOCGIFNAME ioctl. Preserve the IPv6 accept_ra write denial,
diagnostic failures and three rejected strict policy mutations. Next capture
the exact effective policy for signed desktop, then mediated WPA/DHCP and
Settings. USB gate applies.

Current WLAN service: read `spec/login-deployment.md#wireless-connection-manager-files`
and the start/stop section of the 2026-11-18 desktop-iwd journal. Stage the
unit and immutable main.conf with the existing iwd inventory. Keep explicit
activation, the fixed bus path and mode-0700 volatile runtime. Enforcing
1aa38f0e4bbc/3b9a9588 proves inactive boot, two distinct service/bus owners,
original process exits, marker/runtime removal and clean power-off. No
fixture DES preloader. Next dedicated policy, Settings control mediator and
independent DHCP evidence. USB gate applies.

Current WLAN association: read `spec/login-deployment.md#wireless-connection-manager-files`
and the 2026-11-18 desktop-iwd journal. The optional payload stages signed
iwd 3.12, libell 0.83, the two pinned DES modules and a root-only bus rule.
Keep the explicit /run system-bus address, combined depmod and the failed
DES/default-bus evidence. Enforcing d85a9a3548a0/1040603b verifies scan,
WPA-PSK, an IPv4 pool address, disconnect, ordinary UID denial and power-off.
The fixture DHCP marker is address-only compatibility evidence. Next verify
actual DHCP exchange and admit the production service and Settings mediator.
Physical HP testing and the first-version USB gate remain.

Current Intel WLAN inputs: read `spec/login-deployment.md#intel-pci-wlan-files`
and the 2026-11-18 desktop-wlan record. Stage the six pinned modules, two
API 36 blobs, notices, signed regulatory data and the scoped 8086:24fd rule.
Run combined depmod after staging. The merged tree resolves the sample PCI
alias, orders the six dependencies and rejects a changed regulatory RPM.
VM 561ea4743319/347ca597 loads the real WLAN modules and two hwsim PHYs
under enforcing SELinux. Signed login startup and ACPI power-off pass.
Keep hwsim and explicit module loads in VM fixtures. The physical test,
WLAN connection manager, Settings and the first-version USB gate remain.

Current signed suspend: read the installed-suspend-guard record and
`spec/session.md#suspend-and-resume-lock-coordination`. Keep guard phase
exit statuses and read-only desktop access to the enforcing flag, with
policy read and mutation denied. Signed ac97b7682ba1/TCG 87ef5424 reaches
deep S3, resumes, refuses a wrong password, unlocks the original desktop
and exits after normal logout and ACPI power-off. Retain the handshake
refusals 74ad32dc/e3ccb051 and the 226 policy decisions. The same signed
image repeats two S3/guard retire/reauth cycles in no-HDA TCG d30a0607.
The physical test and first-version USB gate remain. NexaUI maintenance only.

Current installed suspend guard: read `spec/session.md#suspend-and-resume-lock-coordination`
and the 2026-11-18 installed-suspend-guard record. Format 5 observes the
seven suspend inputs in both mount views. Preserve the original ten-source
handoff, signed stock systemd bytes and optional Sound row. The guard needs
the original active logind desktop and its ordered endpoint through the job.
Keep the observed seat and cgroup reads and the write and ptrace denials.
Preserve the first publication failure and the enforcing guard refusals.
Release af4f75f6419c passes nine tests and the authority fixture. Signed
c87cf60a8f17 passes 219 readback decisions and starts the desktop. TCG
7178be3f and quiet no-HDA WHPX b93fea3b still block the sleep job. Observe
the exact guard result before changing the denied cgroup ioctls. The USB
gate and NexaUI maintenance state apply.

Current suspend lock coordination: read spec/session.md#suspend-and-resume-lock-coordination
and the 2026-11-18 suspend-lock-coordination record. Keep separate sleep and
wake cover generations, the original dialog Wayland sync and old PAM-result
retirement. Release c1a694200ba6 passes eight selected tests and the separate
authority test. Carrier 420878c47851 observes an accepted original reply
queued before sleep preparation, then restores all three frames only after
a new successful attempt. The wake is simulated. Preserve the hold fixture
and capture failures. Next connect the privileged original session to a
required systemd suspend condition and verify actual signed S3 and aftermath.
The USB gate and NexaUI maintenance state apply.

Current signed desktop lock: read spec/login-deployment.md#signed-login-selection
and the 2026-11-18 signed-desktop-lock record. Format 4 retains the ten-source
handoff and observes the lock ELF, unlock PAM and shared font in both views.
Use the reviewed Compose table and the inherited original worker connection.
Keep received-only read/map for the wlroots keymap and separate lock pixels.
Final image 23bcbeebbda6/session c639ec7d passes actual PAM refusal, restores
the same desktop, logs out and powers off. The final effective policy passes
184 decisions and the composed root checks pass. Preserve the Compose and
keymap failures and the two signed-input refusals. Next coordinate suspend and
wake with the installed lock. The USB gate and NexaUI maintenance state apply.

Current unlock authority: read spec/session.md#privileged-password-verification,
spec/login-backend.md#privileged-unlock-handoff and the 2026-11-18 journal.
The optional backend passes FD 3 only to the exact authenticated desktop.
Keep the original pidfds, UID/GID/cgroup checks, finite verifier and shutdown
ordering. Final GUI `a070ee80f14d` uses real PAM with NoNewPrivileges 1 and
passes all three outputs and queued-result rejection. Backend 5b6373f85c77
passes 25 unit tests, one doc test and three compatibility checks. Actual
worker `reauth-worker-pu6_ndc5` retires the active verifier and desktop on
SIGHUP and closes PAM with zero worker exit. Five selected CTests, one
privileged test and 13 source-preparer tests pass. Preserve the earlier
fixture failures. Next sign the runtime and PAM profile, verify dedicated
policy, and boot the combined lock and sleep paths. The USB gate applies.

Current lock broker: read spec/session.md#desktop-owner-lock-broker and
the 2026-11-18 lock-broker record. Super+L starts cover locally. The original
compositor and dialog pidfds gate the native event loop, with one PAM
worker per covered attempt. Release passes 12 tests and sanitizers pass
four. Real PAM case `bb346b38dbb9` restores the same three displays and
retains cover on a simulated session loss. The carrier's privilege
counter `ee12beb0f6f2` confirms that the correct
password is refused with NoNewPrivileges. Preserve the isolated failures
and the signed-image guard. Next admit the original privileged PAM
authority, sign the runtime, and combine lock with suspend/wake. The USB
gate applies.

Current lock view: read spec/session.md#private-password-dialog and the
2026-11-18 lock-view record. Keep the same original parent, private Wayland
surface, full primary composition, matching presentation before focus and
neutral input before release. Workflow `e5f134d571c2` passes 11 Release and
three sanitizer tests. Actual PAM case `d0e16659dc55` verifies German masked input, denial, all three
original frames on restore and black cover after view loss. Preserve the
earlier undersized scanout evidence. Next signed session owner and broker,
PAM policy and suspend/wake. The USB gate applies.

Current managed lock: read spec/session.md#managed-compositor-lock and the
2026-11-18 managed-lock record. Keep the original parent pidfd, exact covered
generation, all-output presentation and neutral-input gate. Release aa018293d4
passes ten selected tests and memory 95df69f669 passes four. Headless VM case
b5b2e34b6d57 uses real PAM and restores all three exact original frames in
the same app, then refuses an old release. Preserve early focus/startup failures.
Next trusted password UI, installed broker and sleep/wake. USB gate applies.

Current unlock channel: read spec/session.md#one-worker-per-unlock-attempt
and the 2026-11-18 unlock-channel record. Keep the worker receive option
before spawn and broker receive option after its only send. Original sender
pidfds, random ticket, EOF, zero exit and cover generation all gate delivery.
Final Release/memory 6fd5a55a1ea3 and live exec/PAM 1a00753c08f6 pass.
Preserve the initial macro and abstract-autobind failures. Next compositor
generation/release and private trusted UI. USB gate applies.

Current unlock PAM: read spec/session.md#password-verification-for-unlock and
the 2026-11-18 unlock-pam record. Keep one hidden prompt, original UID/name,
the 512-byte limit and the auth-only profile. Both build profiles and analyzer
pass. Private case 646ae8304fba confirms correct password, refusal and expiry
with the exact frozen PAM library/module/helper. Preserve first SDK failures.
Next original-lock/attempt admission and the trusted prompt. USB gate applies.

Current compositor cover: read spec/session.md#compositor-cover and the
2026-11-18 desktop-lock record. Release f54319119195 passes seven compositor
tests. Three-output fixture holds the third presentation, rejects its stale
sequence, then captures every pixel covered. DRM bf56b0c437ac confirms black
QMP scanout and native keyboard denial in the original UID1000 session.
Original actors and unit retire normally. tty1 returns and enforcing stays active.
Preserve the nested case also reproduced on dceaf9e, linkage/timestamp build
failures and DRM frame/retirement fixture failures. Next private trusted
unlock UI/PAM and normal owner power path. USB gate applies.

Current suspend VM: read spec/login-deployment.md#suspend-runtime-files
and the 2026-11-18 desktop-suspend record. Keep the four exact Fedora 259.10
members, original signed login selection and native deactivation fences.
WHPX 3658ab4d enters S3 and reports a VP fault after wake. TCG 34ee4fad
passes one cycle and b0cbc1e5 passes two in the same login with the original
Settings and mediator, live volume/mute and normal logout/power-down.
Preserve the archive/label/focus failures. All four QEMUs are terminal.
Next trusted lock before sleep and physical device recovery. USB gate applies.

Current installed HDA Sound: read spec/audio-session.md#desktop-sound-mediator
and the 2026-11-18 installed HDA record. Signed VM 659ead1f authenticates
and confirms 40% to 39% and mute via native keyboard input. Original app
and mediator exit zero; logout, replacement greeter and power-off pass.
0b85af2f passes the same flow with zero outputs. Missing cac5a6eb and
changed 59d7e72d preserve the signature and fail with -2 and -74.
Preserve the full-F:, QMP scope, fixture credential and focus races as
evidence. All new QEMUs are terminal. Physical HDA, Bluetooth, WLAN,
suspend and the first-version USB gate remain.

Current signed Sound stage: read spec/login-deployment.md and the 2026-11-18
Sound image record. Build-selected v3 adds one protected mediator ELF to
the startup apps guard in both views. The ten-source handoff stays exact.
Both e2cd43820800 profiles pass selection and packaging. UEFI preflight
7a9f758ed52b captures 0444e00f…a8d2; all 139 decisions pass. The actual
1,007-byte signed stage 11bbfde9…9fa6 passes the native reader with the
compiled VM anchor and enrolled UID/GID999. Preserve the base conditional
strict-compile finding and first account rejection. The public stage is
archived locally. Next assemble and boot signed HDA, verify mediator
admission and teardown. USB gate remains.

Current live Sound mediator: read spec/audio-session.md#desktop-sound-mediator
and the 2026-11-17 live Sound record. Opt-in owner passes FD 3 to Settings,
and the channel and three original pins on FDs 3–6 to a finite audio
mediator. Release 96a8f8690897 passes four CTests. Native f3d0924ca9/session
165 joins the real Settings view and live graph with a software sink,
confirms mute and handles original routing loss. All fixture children exit
zero. Preserve first module, capture-mode and compositor-stop failures.
Next signed image selection, dedicated mediator policy and installed HDA.
USB gate remains.

Current native Sound entry: read spec/settings.md#private-sound-channel and
the 2026-11-17 native Settings entry record. The NexaUI executable admits
the direct launcher's unnamed seqpacket FD 3 and combines bounded audio
receives with the normal event wait. Release 05da0db34a11 passes six CTests
including a real Wayland window with controlled owner, original mute
intent/readback and navigation after peer retirement. Use the payload
environment with the original ctest executable. Next provision FD 3 in
the authenticated desktop owner, bind live graph authority and verify
signed HDA. USB gate remains.

Current Sound channel: read spec/settings.md#private-sound-channel and the
2026-11-17 Sound settings channel record. Version 1 has bounded native
snapshot/intent/result packets. The owner dispatcher consumes each request
once. The Settings adapter reuses model readback and unknown feedback.
5b6fdf4e0032 passes native Release and ASan/UBSan. Next admit and provision
original endpoints, join the live broker and verify the normal Sound page.
USB gate remains.

Current desktop tone: read the request-sized final buffer section in the
2026-11-17 login-image boot journal and spec/audio-session.md. The finite
client prepends 24,000 silent frames and pads only the bounded requested
turn. Final CMake ELF be895f91…5e2d in image 9ab6c4e3d2f7 passes exact PCM
in 73cd84eb and WHPX 54508ece. Preserve gaps in 502af9e3 and 299a9a77.
Five Release CTests, the sanitizer callback and all 88 HDA checks pass.
Next original-session Sound settings mediation. USB gate remains.

Current PCM tail: read the playback follow-up in the 2026-11-17 login-image
boot journal. The warm test 19bce8cd loses 320 left-channel samples in the
final partial period. Image b231a92fb6fd appends 704 zero frames and passes
all 48,000 audible samples per channel in ada4d8a7 and WHPX ac7f0a27. Keep
the failed label observation and WHPX startup counter ba1bfaa6. The 88
HDA host tests pass. Next use the actual requested buffer bounds in the
finite client and connect the original graph to Settings. USB gate remains.

Current desktop playback: read `spec/audio-session.md#finite-playback-client`
and the 2026-11-17 desktop HDA playback record. Release 87939b44a506 passes
four focused tests and the exact enforcing policy passes 101 decisions.
Signed WHPX f8acdc2d and two-vCPU 8a2a1208 join the original PipeWire and
drain, measure both channel tones and close normally. Instruction-clock
cadd2a22 does the same. Both clone captures have 1,024-frame gaps.
Keep the first ALSA failure and interrupted VM evidence. The recovered
carrier is enforcing; only the copied test-account fixture was refreshed.
Next settle the original output before playback, measure continuous PCM and
connect Settings. USB gate remains.

Current HDA discovery: read the 2026-11-17 discovery record and
`spec/login-deployment.md#pipewire-and-wireplumber-files`. The signed Fedora
259.10 rule is selected by the audio supplement. Image bbbdaf5a4579 session
62527d4e sees readable/writable virtual PCM and selects analog stereo and
line-out. 776d5a4a passes without HDA. Both cleanly return to the greeter.
Final image projection and 79 policy decisions pass. Preserve the first
archive-reader failure and unrelated frozen base pins. Next mediated graph
client, actual PCM playback and Settings. USB gate remains.

Current audio policy: read `spec/audio-session.md#selinux-and-sound-devices`
and the 2026-11-17 audio policy record. Offline optional roles and private
paths compile. Signed HDA VM 819df4d9e095/session b3f5a90b reaches both
original actors and ALSA inotify, then clean logout/replacement. All 78
readback decisions pass. Preserve first label and watch failures and failed
full-/run build. Next source exact systemd-udev 259.10 `78-sound-card.rules`,
verify the actual card, ACL/device/route/PCM and connect Settings. The 259.5
carrier rule is a different input. No USB release or NexaUI task.

Current audio lifecycle: read `spec/audio-session.md#desktop-session-owner` and
the 2026-11-17 owner record. The normal entry has an opt-in direct-child pair.
Keep fixed environments, ordered pidfd retirement, optional audio failure and
parent-death SIGTERM with SIGPIPE ignored only in the audio children. Install
the pinned ALSA prefix with the desktop files. Carrier 688ebdd1473e loads ALSA
and Lua 5.5, and passes orderly stop, routing loss and owner SIGKILL. Preserve
earlier absence and SIGPIPE evidence. Next dedicated audio policy and staged
enforcing desktop boot/real HDA, then Settings. No USB release or NexaUI task.

Current desktop audio: read `spec/login-deployment.md#pipewire-and-wireplumber-files`
and the 2026-11-17 desktop audio record. The public offline stager yields 254
files and six links. All 28 selected ELFs resolve in the HDA desktop root.
Image 76e4dde39a97 passes HDA/startup and private pair f42f2e9bb725 connects,
loads Lua 5.5 and ends cleanly. Preserve the first smoke assertion failure.
Next normal authenticated session devices, service policy and Settings.
The USB gate and NexaUI maintenance-only state apply.

Current desktop HDA: read spec/login-deployment.md and the HDA section
of the 2026-11-17 login-image-boot journal. The optional image payload
reuses eleven kernel modules and adds the checked Conexant codec.
Keep driver and config pins and regenerate the combined depmod indexes.
Derivative ab3a3c4a71ea verifies virtual node creation and Settings
launch, and the no-HDA desktop/power-off control passes. Three payload
and 85 HDA contract tests pass. Device roles and app policy are unchanged.
Next attached graph/routing runtime and Settings audio admission.
Physical HP playback, Bluetooth, WLAN, suspend and USB gate remain.

Current release app assembly: read spec/login-deployment.md and the
fixed-desktop-apps section of the 2026-11-17 login-image-boot journal.
Format 2 stages six fixed ELFs and signs them in AETHLS02. Startup retains
separate core/apps guards in both views. Keep read-only app observation,
the ten-entry handoff shape and rejection of v1 records by the v2 reader.
Release 8b66d256141c passes 18 CTests. Signed f6d7bcad58f5 passes Settings
refresh, Terminal cat and Files-editor save/power-off. The original Files
VM was resumed after QMP timeout. Signed-byte-preserving missing and
mutated editor counters fail. Next installed hardware service integration
and Settings. Preserve counter and data evidence. USB/NexaUI gates apply.

Current installed power-off: read the PAM worker hangup section of the
2026-11-17 login-image-boot journal and spec/login-backend.md. Diagnostic
f2d08a75 pins and observes the original replacement worker, greeter and
display each exit from SIGHUP. No sender attribution. Keep the worker HUP
mask through PAM teardown, children's original mask at exec and the
infinite-stop/normal original worker fence. Backend ae38890e7a49 passes
24 Rust unit tests, one doc test, two compatibility build checks and the
source preparer passes 13 tests. Signed 9d2ac4bd4a92 passes three ACPI
power-offs: active session, just after logout and visible replacement
greeter. PAM close, service stop, ext4 unmount and kernel power-down
are witnessed. Preserve all interrupted attempts and data. Next exact
public fixed-app release assembly and first-version hardware work.
USB and NexaUI gates apply.

Current shell and document handoff: read the shell job selection section
in the 2026-11-17 login-image-boot journal and spec/files.md. The two
enforcing policy reads and 35 decisions pass. Signed 5c0b5e09cd37
executes pwd in the actual shell and shows / and a fresh prompt.
Original document FD 4 fails the SELinux exec transition ioctl check.
Files reopens the same pinned inode read-only before spawning. Keep
the editor's leaf identity checks and the policy boundary. Release
8b1290550c8f passes 18+7 CTests. Signed a2906a8c4658 passes create,
open, save, copy-only disk inspection, cold reopen and normal logout.
Next fixed shell utilities including cat and public app assembly.
Preserve failed images and original data. All new VMs are terminal.
The USB gate and NexaUI maintenance-only state apply.

Current fixed app roles: read the fixed app entrypoints section of
the 2026-11-17 login-image-boot journal and spec/session.md. Offline
74712fbbc0d2 and two boot reads produce effective 78e5a894…b82f.
Never widen generic bin_t. Signed 140c4d10fc7f with data 6ef800c75f25
passes visible Settings, F5, Escape and logout. The first data root
var_t fixture blocks PID1 and is retired. Archived Files lacks the
current bold-font argument; new a9cfe52b0b1d passes 15 tests and
signed 0f697ed804cd passes actual Files create, immediate display,
close and offline copied-journal verification. Preserve original
data bytes. Terminal starts its PTY shell and logs renderer ready
then exits 139. Its fault address is unobserved. Next Terminal
presentation/PTY input and document opening, then exact public app
release assembly. All new VMs are terminal. USB gate remains.

Current signed app launch: read the installed app execution boundary
in the 2026-11-17 login-image-boot journal. The three ordinary app
search launches reach original fork in signed b0bfb4471e8b and
f528ac353c7f. Final d3bc5e6f781b confirms execv EACCES for Terminal,
Files and Settings; 13 CTests pass. Offline loader resolves six
archived apps/helpers and the exact effective policy denies generic
bin_t execution while allowing selected compositor/desktop controls.
All seven VMs are terminal. Next exact app and helper admission
with dedicated role policy, then runtime, input and shutdown.
Retain the USB gate and original failure evidence.

Current signed desktop VM: read the shared-memory desktop section of
the 2026-11-17 login-image-boot journal and spec/session.md. Signed
image b0bfb4471e8b reaches visible user desktop after actual password
and Start, draws six dock/search frames with two release-tracked buffers,
then closes normally and displays replacement greeter in session 3.
Release d159781eb1fd passes 13 tests (841 source inputs); both original
VMs ended. Next verify installed apps, then broader first-version
requirements. The existing user app renderers and USB gate are separate.

Current installed desktop fault: read the desktop fault section of
the 2026-11-17 login-image-boot journal. Two signed diagnostic VMs
reproduce authenticated desktop status 139. The second shows
SEGV_ACCERR at an anonymous rw-p mapping and a return into exact
libgallium 26.2.5. Do not infer the earlier operation or a shader-cache
cause. Signed SDL software-window counter 9cbc88e7d55b reaches the
renderer but refuses absent window framebuffer support. The code
counter is reverted. Next explicit Wayland shm presentation with SDL
software surface, original buffer release, first frame and intentional
logout. All six VMs are terminal; preserve their evidence and USB gate.

Current installed direct-exec VM: read spec/login-backend.md and the
direct session execution section in the 2026-11-17 login-image-boot
journal. Backend d1a75296af9f passes 23 Rust unit/one doc test and
startup-only/default compile checks. Signed 7b1412821e82 reaches the
protected handoff and visible greeter twice. Actual synthetic auth
and Start open UID1000 Wayland session 2. User compositor reaches
KMS output, desktop logs SDL ready then exits 139 before owner ready,
PAM closes and greeter session 3 displays the account prompt. The
shader-cache warning is separate evidence. Next localize the desktop
fault in the exact installed profile, then test readiness and normal
logout. Both QEMU attempts are terminal. Preserve images/logs and
the USB gate.

Current standalone signed login VM: read `brains/aetheri/STATE.md` and
the 2026-11-17 login-image-boot journal. Final Release profile passes
13 CTests and package suite passes four. The original root/data checks
pass. After the tmpfiles correction, signed image e536a921bc7e starts
the backend and PAM opens/closes actual greeter session 1. VT8 and
worker source identify the missing /bin/sh exec despite source_profile
false. The selected greeter ELF exists. Startup selection occurs but
namespace observation ends after the backend exits. Next verify direct
exec for the installed backend, preserve worker/PAM ownership, then
re-sign and boot. Both VM cases are terminal. Keep their logs/screens,
the separate audit findings, the physical USB gate and NexaUI
maintenance-only state.

Current offline image policy: read spec/login-deployment.md and the
2026-11-17 login-offline-policy journal. Keep the three production CIL
sources and login file contexts together. The offline compiler uses a
fresh copy of the distribution store with explicit root and no reload.
Sign the enforcing kernel's serialized policy readback for that image.
Two cold UEFI boots of local/policy-boot-4904237e9396 returned the exact
policy readback 5e4e5370a275...407e. This differs from the installed
policy.35 digest. Continue assembling the VM-only login root with that
policy and the selected real units. The USB gate applies.

Current installed image provider: read spec/login-deployment.md and the
2026-11-17 login-image-provider journal. Explicit CMake image mode compiles
the same key/release into startup and greeter. Keep the kernel statmount
unique-ID request on 6.19. Use the offline ldconfig cache for the fixed
runtime directory. The two build profiles and six native mount cases pass;
the actual startup rejects the writable carrier in its intended SELinux
domain. Next compose a standalone VM boot with these artifacts, the final
policy and installed services. The USB gate applies.

Current signed desktop startup work: read spec/login-deployment.md and the
2026-11-17 login-startup-runtime journal. The installed helper observes its
signed unit and its actual executed image plus session/compositor/desktop
files in both views. Keep the original per-view handles, fixed ten-source
handoff and read-only startup policy. Native Release and memory fixtures
reject changed or lost desktop files and a byte-identical foreign startup
inode. The typed test with the selected libcrypto reaches the expected
policy rejection after runtime capture. Next boot the selected image with
the installed provider and actual units. The USB gate applies.

Current image account work: read spec/login-deployment.md and the 2026-11-17
login-image-accounts journal. The staged NSS file uses local account sources;
the supplement pins nologin. The composed verifier checks the locked greeter
against independent UID/GID expectations and reports the sole kernel group.
Pass --account-root in the normal signing flow. The exact 259.10 PAM files
pass named account gates in the fresh private image root. Next connect these
image inputs and the remaining installed platform gates to a booted graphical
login. Preserve the protected carrier account and the first-version USB gate.

Current composed PAM root work: read spec/login-deployment.md and the
2026-11-17 login-pam-root journal. The offline verifier checks the three
configuration files, 30 ELF objects, 18 library aliases and helper metadata
in the isolated final-layout fixture. Exact archived systemd-pam 259.10 loads
with all six selected modules, and the unknown-service default denies five
calls. Preserve the archived raw bytes, fixture failures and private nosuid
metadata testing. Next actual account/search inputs and installed platform
gates through a booted login. The USB gate applies.

Current packaged PAM work: read spec/login-deployment.md and the 2026-11-17
login-pam-runtime journal. Both installed adapters include the exact deny-only
fallback as their tenth protected source. Keep the signed selection format and
its two named PAM rows. The ordinary-user supplement stages 18 RPM-pinned
objects and eight aliases. Preserve the helper's 0755 staging mode and record
its 04755 image metadata for final image assembly. Carrier systemd-pam 259.5
differs from the selected 259.10. Next verify the composed target base, actual
loader/account inputs and installed platform gate. The USB gate applies.

Current packaged PAM sources: read the 2026-11-17 login-packaged-pam journal
and spec/login-deployment.md. Both installed adapters add the two literal
PAM profiles to the nine-entry handoff source set. Use O_PATH for ancestry;
the five-drop-in bound is unchanged. Case `01207743de7c` checks all nine and
negative PAM cases. Actual packaged PAM case `d257d0be87fe` uses bound entry
`6b2636f06b9c` with compiled runtime data paths and passes desktop/logout.
Preserve failed EGL case `34afa1ca0bd6` and exact fixture retirement. Next
integrate the protected target platform, PAM modules/default and live units.
The first-version USB gate applies.

Current login policy handoff: read the version-2 profile in `spec/login.md`
and the 2026-11-17 login-policy-handoff journal. Both installed adapters pin
the enforcing selinuxfs sequence. The startup digest comparison and the
graphical callback forwarding must retain the original sequence and context.
Preserve the failed graphical fixtures. Same-byte kernel reload rejects the
old handoff and fences the original reader. Next admit the signed PAM profiles,
remaining image platform inputs and full installed lifecycle. Keep the USB gate.

Current installed greeter adapter: read `spec/login-deployment.md` and the
2026-11-17 login-installed-greeter journal. Both installed adapters bind
`selection.bin` in the common handoff source set. The greeter checks actual
UID/GID/groups before runner entry and pins the signed font. VM
`fe0059739895` passes seven private cases per profile; startup
`da5c58a8732e` passes four per profile. Preserve failed build records.
Candidate policy `f99ec70ee8a5` is retired to baseline. Next select actual
platform gates and exercise the installed units through full login, desktop
and logout. Keep the first-version USB gate.

Current installed startup adapter: read `spec/login-deployment.md` and the
2026-11-17 login-installed-startup journal. The finite executable can select
`installed_release` and an explicit platform gate. Preserve the native
original-service and namespace guards. One view hook verifies a second
record against the original authenticated digest before source capture.
VM `e9f64834b8fa` verifies both private mount views and the loaded selinuxfs
policy in Release and memory. Candidate `a78021537a8a` is retired to baseline.
Next wire the installed greeter and platform gate, then exercise live units
and the original boot/seat/PAM lifecycle. Keep the first-version USB gate.

Current installed login selection: use `spec/login-deployment.md` and the
2026-11-17 login-installed-selection journal. Stage the raw signature with
`prepare_login_selection.py --signature` at the literal 0444 `selection.bin`
path. Native readers pin authenticated bytes in their original view. VM
`3c5e5fe9f62b` passes 11 checks in each profile, including byte-identical
replacement and permanent fencing after restoration. Preserve the failed
raw-label fixture. Candidate policy `6a313795bebf` is retired to exact baseline.
Next connect the actual providers. Startup must acquire separate protected
observations before and after its authenticated namespace transition. Keep
live policy/account/runtime/device admission and the first-version USB gate.

Current login release selection: read the signed-selection section in
`spec/login-deployment.md` and the 2026-11-17 login-release-selection journal.
The offline generator emits 691 canonical bytes from the reviewed staging
receipt, literal account IDs, selected systemd drop-in and effective policy.
The OpenSSL 3.5 verifier expects the 64-byte Ed25519 suffix and independently
selected public key and release identifier. The native and Python-to-C tests pass in
Release `42b8160c9343` and memory `7c3c491ecb4b`. Preserve the failed build
artifacts. Next wire the fixed-path providers, preserve original-instance
guards and verify against the effective image. The first-version USB gate applies.

Current fixed login layout: read `spec/login-deployment.md` and the
2026-11-17 deployment-layout journal. The offline stager requires 11 independently
selected artifacts and writes a 24-file tree plus receipt. Case `33b887fe4e0e`
verifies the isolated units/alias, sysusers/tmpfiles and all 17 file contexts.
Case `994e02d124b3` proves the proposed sticky scratch mounts and full PAM/desktop/
logout in the existing fixture profile. Candidate policy `022652a61780` was
retired to exact baseline. Preserve the VM records and USB gate. Next bind
normal providers to the fixed paths and full image runtime/effective policy.

Current session runtime cleanup: read spec/session.md and the 2026-11-17
session-runtime-cleanup journal. Retain parent/runtime/cache FDs from
creation and use empty-only removals after known children and output end.
Keep the error diagnostic independent of the user session exit status.
No recursive deletion, retry, or adoption after an observed mismatch.
Case 40866a4393b0 proves original runtime removal and preservation of
the live same-UID parent/sibling, plus PAM close and fresh greeter/stop.
The original leftover case stays preserved. Next normal installed provider
and deployment profile. USB gate remains.

Current confined graphical desktop: read the 2026-11-17 confined-graphical-
desktop journal and spec/login.md/spec/session.md. Case f5ed8d7d307e reaches
actual PAM/Start ACK and visible user desktop under the same split policy,
then original user end/PAM closure, fresh greeter and clean backend stop.
Keep private greeter /tmp and typed private /dev/shm roles. Final module
b3a2fdbf1b71 reproduces exact effective policy of the tested candidate,
passes 272 decisions and is retired to baseline. Preserve first case's
lost-pin boundary and the leftover empty user runtime/cache as evidence.
Next owned runtime cleanup and normal installed deployment. USB gate
and NexaUI maintenance-only state remain.

Current graphical policy: read spec/login.md and the 2026-11-17
graphical-login-policy journal. The separate compositor domain uses logind
devices and has no direct device open or backend credential path. Two real
confined graphical PAM authentications reach the Start action. Final
cd81c2dc1d7e retires the four original pins at zero and records greeter PAM
closure; it sends no Start. Keep typed private scratch, the received-only
read/write pixel FD and the no-ptrace/no-device-open checks. Final loaded
decisions and strict negative compiles pass. Preserve prior failures and the
original-pin loss case. Next same-policy graphical Start into a separate user
runtime and regular deployment. USB gate remains. NexaUI P04 maintained in
place; no new chat message.

Current greeter shutdown correction: read spec/login.md and the 2026-11-17
graphical-login-stop journal. Connection watch is local and retains existing
fences; command preparation and pump still revalidate admission. The old
binary fails the pending stop regression. Final idle 65600c933018 and full
graphical PAM/desktop/return 8904a560d0b0 pass zero exits, actual PAM closure
and baseline restoration in the administrative VM role. The diagnostic
reproduced ESTALE; the historical EAGAIN property source remains unobserved.
Preserve unsuccessful probe cases. Next dedicated graphical provider,
MAC/device policy and normal deployment. USB gate stays in force.

Current graphical login VM milestone: read spec/login.md and the 2026-11-17
graphical-login-desktop journal. Real protected handoff + graphical keyboard
input + private PAM reach Start ACK, the visible DRM user desktop, original
entry/worker exit and PAM close, then a fresh empty greeter. Final
58a9b0d7f26d uses an administrative unconfined_service_t test role. Backend
stop after the replacement frame leaves the greeter exit 1/EAGAIN, with its
compositor/worker and backend terminal at zero. Preserve both prior fixture
failures and this stop outcome. Separate service-job observation from a
confirmed local stop before changing the fence. Next stop sequencing and
dedicated graphical policy/normal deployment. No USB image.

Current graphical greeter executable: read spec/login.md and the 2026-11-17
login-greeter-entry journal. AETHERI_LOGIN_GREETER requires an explicit compiled
provider. The ordinary entry closes inherited FDs, selects its original seat,
checks the compositor image and pins its child before descriptor exec. The
socket hint does not admit the backend. Both build profiles pass ten selected
tests and reject with the deliberately denying provider. The VM retains the
enforcing baseline. Next actual graphical login with the protected greetd/PAM
deployment through visible desktop and original shutdown. USB gate and NexaUI
maintenance-only state apply.

Current native greeter UI checkpoint: read spec/login.md and the 2026-11-17
login-ui-host journal. Host takes the only private Wayland endpoint, gates
input on active greeter seat/VT, submitted frame and keyboard focus, and
drains native events before pumping requests. Actual greeter-seat fixtures
91305d9c9368/ba8e5bfce7ad show native UI, synthetic ACK and masked draft loss
without an answer. Keep dummy peer distinct from the protected backend.
Next compiled deployment, exact normal provider and full authenticated UI
transition. Preserve failures and the USB/NexaUI maintenance gates.

Current private greeter display checkpoint: read spec/login.md and the 2026-11-17
login-display journal. The compositor creates the only client stream after
DRM startup and passes it through a one-off unnamed seqpacket channel. The
receiver verifies the original launch pidfd, child ancestry, socket/process
labels and actual ELF. No listener, clipboard global, capture or desktop
shell in the login mode. Native 5725b9cfc293 passes real DRM/Wayland and
same-image verification separately. Retain failed artifacts and independent
VT restoration. Next native UI host with original greeter seat, focus/frame
and normal deployment admission. USB and NexaUI maintenance gates apply.

Current greeter connection/view checkpoint: read spec/login.md and the
2026-11-17 login-client-view journal. The owner waits only for a missing leaf
in its held original directory. Native 3f90cdbdeffe admits an idle connection,
14164e2cff55 permanently rejects reordered published sources. Both preserve
original worker/PAM closure. Example37 view/poll source is imported with license,
uses the existing 0.1.23 pin and passes five borrowed-view tests. Next bind it
to a graphical host on an independently admitted original display/input path
and advance the normal compiled provider. Keep the USB gate and NexaUI
catalogue maintenance only.

Current confined desktop checkpoint: read spec/login.md and the 2026-11-17
login-desktop-policy journal. Final Release ff65b1671440/36ea39e113cc visibly
start the desktop after real PAM and retire the original entry/worker/PAM
scope. The second verifies native Super+Space/files, a settled drawn frame
and virtual DRM/input descriptors. Only the split desktop-shell carrier role
gets LLVMpipe execmem. Preserve failed fixtures and independent VT
retirements. Next normal protected provider and original graphical greeter,
then managed app admission. USB gate and NexaUI catalogue maintenance apply.

Current split user-session checkpoint: read spec/login.md and the 2026-11-16
login-roles journal. 05f87c3c78b6 verifies the original selected unprivileged
entry, active Wayland seat0/VT8, typed runtime and compositor exec. Compositor
startup stops at the actual udev monitor denial. Both attempts are terminal
with PAM close; preserve the private input holders and failed fixtures. An ACK
and worker exit 0 do not prove a successful desktop. Next constrained udev and
logind-brokered DRM/input, full visual readiness and original shutdown. Keep
the USB gate and NexaUI catalogue maintenance only.

Current split authentication checkpoint: the actual confined PAM worker passes
empty-answer AUTH_ERR, synthetic authentication/account success and acknowledged
cancel in f2e5e69a75f2. The distro unix_chkpwd transition and worker pipes are
checked. The sealed synthetic input type belongs to the carrier supplement.
Keep first helper-denial fixture and original test outputs. All single-threaded
startup constructor cases now precede the first pthread. Release/memory nine
tests each, 159 loaded decisions and strict 426 modules with nine negative cases
pass. Temporary policy retired. Read spec/login.md and the login-roles journal.
Next admit the fixed user command, PAM session and desktop under split policy.

Current confined greeter checkpoint: matched live source/worker/backend and
handoff checks create an idle connection in aab9fa4a3505. The real reordered
publisher record is rejected before admission in 8784cfa4b31d. Both retain
original PAM close and greeter/worker exit0. Preserve the early source denial
and mismatch/post-publication ESRCH evidence. The Release suite passes nine.
The memory suite's first startup model line137 failure stays unexplained and
its exact binary recheck passes nine. Loaded 146 decisions and strict 426 inputs
with seven forbidden additions pass. Candidate retired to exact baseline.
Read spec/login.md and the login-roles journal. Next admit human PAM and the
trusted graphical/session path under the selected policy, with the USB gate.

Current split publisher checkpoint: the finite startup service publishes a
protected AETHLGH1 for the original confined backend in 867a85c110ea and exits.
The original internal greeter/PAM worker also close normally. The split policy
uses typed socket access and read-only backend/config/policy observation. The
older administrative supplement alone carries the generic runtime socket rule.
Strict 426-module compilation and five negative inputs pass, as do 130 loaded
decisions. Preserve the EACCES and concurrent EBUSY evidence and the retired
module receipts. Read spec/login.md and the login-roles journal. Next align
ordered source selections and test the original confined greeter's actual
handoff consumption and initial connection, then normal credentials and UI.

Current login role checkpoint: read spec/login.md's candidate login roles and
the 2026-11-16 login-roles journal. The separate backend, PAM worker and UID992
greeter pass actual enforcing SELinux access and internal PAM open/close.
Independent strict compilation checks all 426 module inputs and three rejected
counterexamples. Keep the Fedora shadow-reader attribute worker-only and the
early-boot kernel_t datagram supplement carrier-only. Release/memory nine tests
each, GCC analyzer and ordinary instrumented probe pass. The host reboot ended
PID16628, the qcow2 check passed, and new VM boot 342ed7a6 observed fresh role and
PAM cases with the final probe. Exact module retirement restores the baseline.
Next integrate the finite publisher and protected greeter connection in the split
policy, then verify normal PAM authentication and trusted graphical handoff.
The USB gate and existing NexaUI catalogue maintenance continue to apply.

Current extra-group checkpoint: read spec/login.md and the follow-up in the
2026-11-16 login-account journal. Actual Release/memory greeters with private
NSS groups 992/60000 reject expected 992, keep the failed guard fenced, then pass
a fresh guard against the same held live kernel peer and literal control set.
No credential/Start request or global account change. Both original worker ends
and PAM closes pass; new memory desktop 83d69fec21ca also completes normal login/
shutdown. Audit 35e92aa3eab1 checks both 784-input profiles and 200 runtime files.
The memory suite's original startup-model failure stays unexplained: unchanged
binary rerun passes 9; diagnostic 128 runs do not reproduce. Never claim a thread
cause or a production fix. Preserve failed4471's partial-marker fixture and
original memory output. Candidate module d00 is retired to the exact baseline.
Next dedicated backend/worker/greeter MAC/device admission and normal provider.
NexaUI snapshot 014449f is SDK 0.1.33, while Aetheri remains 0.1.23. Review/reuse its
existing Example37 login view/connection glue for the graphical greeter; imported
protocol/connection/grapheme files are unchanged from7d5fa47. This is not a pin
upgrade or trusted-host admission. Example39 now supplies an initial original
Physim model seam, not full app migration. Catalogue maintained; no new chat message.

Current continuation: read spec/login.md's dedicated greeter account section,
spec/login-backend.md's original-session-worker shutdown and the 2026-11-16
login-account journal. Locked carrier UID/GID992 aetheri-greeter is reserved;
never delete/recreate/reuse it implicitly. Native supplementary groups are exact
independently selected copied expectations. Final backend /opt/aetheri-worker-
test-b8d0e86e07bb (SHA ceff41c3…68a61e7) adds worker-owned signal waiting, normal
PAM-worker shutdown, abnormal/lost-current-worker reaper fences and nonzero
worker-error propagation. Keep the
original Context/reaper lock and KillMode=process/SendSIGKILL=no/infinite stop.
Five full desktop flows span three candidates; final memory b4b8ecb620ef and
preceding a835 actual abnormal-worker refusal pass. Do not claim every case used
the latest backend. Three nine-CTest C profiles/784 inputs each, final 21 Rust
unit/one doc test, no-feature check and audit a52adef6ccc5 pass. Candidate modules
1be/78c/942 are retired with exact baseline restored. Preserve abnormal5a3136
inputs/socket, failed43b/410/98 holders/fragments and earlier failures/quarantine.
The98 case queried exit information too early; fresh-pin administrative
retirement never restores its controller's lost original pins. Require the EXIT
mask with bounded observation on the original pidfd; readiness alone is insufficient.
Next dedicated backend/worker/greeter
MAC/device admission, then normal provider and graphical greeter. USB gate and
NexaUI maintenance-only state remain.

Current startup confinement: read `spec/login.md#candidate-startup-confinement`
and the 2026-11-16 login-startup-policy journal. The finite publisher passes both
actual desktop/PAM shutdown flows as aetheri_login_startup_t, with dedicated
runtime/handoff labels. The normal backend attribute stays empty; the separate
carrier-test supplement is never production admission. Retain five capabilities
but no backend process ptrace or manager mutation; kernel read-mode observation
uses file-read permission. Eight real probe denials and seven loaded decisions
pass, plus three security-SID mutation denials after strict assertion review.
That final source compiles to the identical complete tested policy. Two eight-
CTest builds/779 inputs and the separately corrected current probe/analyzer pass.
All six new backend/helper flows and probe services are terminal; preserve four
new failed holders/fragments and the earlier test/export/comparison failures.
Both candidate-module runs were retired; exact pre-test policy bytes restored,
enforcing=1/tty1, original RPM/runtime intact. Next dedicated backend/worker/
greeter domains and account/PAM/device integration, then normal compiled provider
and original graphical greeter. No installed production policy/service or USB
image. Keep foreign UI memory, USB gate and NexaUI maintenance-only state.

Current finite startup executable: read `spec/login.md#finite-startup-executable`
and the 2026-11-15 login-runner journal. aetheri-login-startup requires an explicit
build-time deployment C provider; no default/runtime profile/plugin/CLI selector.
It drops extra inherited FDs, uses the literal host bus, binds its original self
service MainPID, freezes backend manager/path/PID/invocation and compares the real
kernel peer before coordinator entry/publication. Final separate-unit Release/
memory greeter desktop flows pass original entry/worker/PAM/scope end; helpers
are Release only and exit before the user desktop. Eight CTests/all776 inputs,
analyzer/invocation counter, configure guards and outside-supervisor refusals pass.
All five new cases are terminal. Preserve the two connection failures, the third
visual-timeout holder/new-pin administrative retirement and older quarantine.
The greeter-owned0755 socket requires DAC_OVERRIDE in addition to the previous
four capabilities. No socket mode/policy loosening; the unconfined carrier role
and enforcing-only provider are NOT production admission. Next effective dedicated
startup/backend/greeter/account/PAM/MAC/device policy and normal provider, then
original graphical greeter/account integration. Keep foreign UI memory, USB gate
and NexaUI maintenance-only state. No installed service or USB image is implied.

Current native startup work: read `spec/login.md#native-startup-coordination` and
the 2026-11-15 login-startup-coordinator journal. login/startup.h composes the
original kernel socket service with PIDFD_GET_MNT_NAMESPACE/setns, same-view
source/loaded/directory observations, mandatory external policy gate and one
publication attempt. Constructor failure requires process exit; never restore,
retry, adopt or clean a possible record. Only config-notice absence is pending.
Final Release/memory CMake profiles pass six CTests/all770 inputs, analyzer and
namespace-omission counter; final actual authenticated desktop cases use the
native coordinator without nsenter and finish original entry/worker/PAM/scope
retirement. All five new cases are terminal; preserve earlier failed holders,
failed pending-config/analyzer/counter preparations and old quarantine. The
enforcing-only fixture callback is NOT deployment admission. Next finite installed
startup executable/selected deployment inputs/service ordering and effective
dedicated greeter/account/PAM/MAC/device policy, then original graphical greeter.
No backend ExecStartPost shortcut around active/no-ControlPID/no-job observation.
Keep foreign UI memory, USB gate and NexaUI maintenance-only state.

Current authenticated desktop work: read
`spec/login.md#authenticated-desktop-fixture`, `spec/session.md` and the
2026-11-15 login-desktop journal. Actual private synthetic authentication now
reaches the selected entry, seat0/VT8 Wayland user scope and visually inspected
KMS desktop. Release/memory greeters pass original entry/worker end, actual PAM
close and inactive scope before backend stop; the replacement does not replay
the consumed credential OFD. Both desktop runtimes are Release. Final CMake
profiles pass five tests each/all767 inputs; all200 protected runtime files,
analyzer and logging-omission detection pass. All four live attempts are terminal.
Preserve both new failed holders and the explicit new-pin administrative
retirement; it is not recovery of original pins. Entry console diagnostics now
use existing libsystemd after stdio checks/extra-FD cleanup. Next protected
installed startup coordination, effective dedicated greeter/account/PAM/MAC/
device policy and original graphical greeter, then normal app/account transitions.
Keep the quarantine, foreign UI memory, USB gate and NexaUI maintenance-only state.

Current successful-authentication work: read
`spec/login.md#successful-authentication-fixture`, `spec/session.md` and the
2026-11-15 login-authentication journal. Actual private pam_unix auth/account
accepts a fresh synthetic secret after rejecting empty input; confirmed Cancel
ends the original worker. No Start, PAM open_session or desktop login is proven.
The test uses PID1 OpenFile/sealed input plus a private `/etc` projection on guest
tmpfs, with no host password/PAM change. Keep failed memory holders separate.
The session entry now closes extra inherited FDs before its own observers; native
fork/failure/counter tests and Release/memory owner regressions pass. Next verify
the full selected entry/runtime at the fixed path, then actual PAM/logind/VT and
KMS desktop transitions. Keep ACK distinct from exec/readiness, existing policy
requirements, old quarantine, USB gate and NexaUI maintenance-only state.

Current login-worker cancellation: read
`spec/login-backend.md#configuring-worker-cancellation` and the 2026-11-15
login-worker journal. Optional integration now serializes original direct-child
observation with the sole reaper, retains the configuring slot through failure,
and acknowledges Cancel only after original worker end. No socket-error/ECHILD
shortcut, repeated send or C UNKNOWN weakening. Native old-backend regression
fails; corrected Release/memory greeters pass actual prompt/deny/retry, with an
independent original-worker pin and explicit LSan. Keep the reaper lock before
waitpid and enrollment before initiation; do not reuse this proof after Start.
Next actual successful authentication/user-session transitions plus protected
startup and effective account/PAM/SELinux/logind/device policy. USB gate applies.

Current original greeter work: read `spec/login.md#original-greeter-process`
and the 2026-11-15 login-greeter journal. The self-only C observer joins its
kernel-derived live Root-parent ancestry to the original backend, exact self
ELF/account/labels/cgroups and absent self capabilities. Actual greetd-launched
Release/memory greeters consume the same-namespace protected handoff and exercise
real pam_unix prompt/cancel and empty-answer AUTH_ERR. Failed-worker Cancel stays
UNKNOWN; do not weaken the fence or infer worker drain. Next fix that backend
retry path with original-worker lifetime evidence, then effective deployment
policy and successful authentication/graphical session. The observer alone does
not admit worker ELF, supplementary groups, greeter class/seat or policy. All13
fixtures are terminal; preserve failures, old quarantine and the USB gate.

Current protected login handoff work: read `spec/login.md#protected-observation-handoff`
and the 2026-11-15 login-handoff journal. Root publishes one create-only AETHLGH1
observation after actual ELF/config checks. The ordinary reader compares original
boot/invocation/pidfd/cgroup/image/source and manager/process context while the
same service is live. Keep directory/path/ancestor and exact MAC admission with
the caller. Preserve possible named files after uncertain publication and never
adopt an EEXIST entry. Release/memory clients and the targeted omission mutant
are verified. Next wire publication/consumption to the original greeter and
effective policy, then actual credential and graphical session transitions.

Current consumed login config work: read `spec/login.md#configuration-read-by-the-backend`
and the 2026-11-15 login-config-reader journal. C now requires the exact PID1
invocation/digest and NotifyAccess=main. Once requested, all checks require the
notice. Only initial absence is pending. Release/memory and the actual protected
greetd with an inert UID1000 greeter passed. Explicit `User=root` in this VM
profile dropped CAP_SETUID. The tested system-default Root identity preserves
the bounded capability set. Next protect the handoff to the original greeter
and finish policy and real authentication/session tests. The USB gate applies.

Current login startup configuration work: read `spec/login-backend.md` and the
2026-11-15 login-startup-notice journal. The optional pinned greetd build emits
`AETHERI_CONFIG_V1` through `NotifyAccess=main` from the same bytes its parser
used. Native systemd status and input rejection passed. Next join that status
to the exact original live PID1 service invocation and expected source digest,
then integrate the protected executable observation and original greeter before
real PAM/local session tests. The first-version USB gate still applies.

Current login executable work: read `spec/login.md#running-backend-executable`
and the 2026-11-15 login-current-image journal. The administrative observer joins
the original socket peer's actual ELF to the loaded unit and selected source.
Keep this privileged operation local, with no caller-selected PID or fabricated
bus peer. The native same-bytes different-object case is rejected and fenced.
Next bind a protected observation to the original greeter and actual consumed
config/policy, then verify PAM and session transitions. The USB release gate applies.

Current loaded login unit work: read `spec/login.md#loaded-login-unit` and the
2026-11-15 login-loaded journal. The protected fragment, backend/config paths
and ordered drop-ins now join the original active socket service and exact
loaded ExecStartEx record. PID1 reads use its original unique bus name. The
ordinary client got EACCES opening the inert Root peer's `/proc/PID/exe` in both
native profiles. Establish current-image and loaded-config/policy evidence before
credential transport, then admit the original greeter and exercise PAM/session.

Current login source work: read `spec/login.md#protected-deployment-source-observations`
and the login-sources journal. `login/sources.h` verifies a separately selected
Root-owned source set and retains original ancestry and file identities. Keep
exact file labels/digests, leaf mount checks and permanent fencing after observed
loss. Join this with current executable, loaded configuration/policy and original
greeter admission before credential transport. Verify actual PAM/session transitions
after that integration. The existing first-version USB gate still governs release.

Current continuation: read `brains/aetheri/STATE.md`, spec/login.md and the
2026-11-10 login-service journal. The new Unix-peer service observer passes actual
ordinary-client/Root MainPID, same-unit Root-child rejection, memory-client and
targeted mutant tests. Socket/current raw process labels are separately exact;
the first ESTALE was that distinction, not a proven permission blocker. Retain
both peer/PID1 FDs and original invocation/cgroup/context; never fabricate a bus
ae_peer from socket data or infer current ELF/config/greeter admission from this.
Next protected source/loaded-policy and greeter/initial-connection admission, then
real PAM/session tests. Fixtures are terminal; preserve their failures/evidence.
The C17 connected-stream adapter also passes
native analyzer/Release/memory and all three CMake login tests. It requires the
independently selected original kernel peer/current credentials and a mandatory
external daemon/config/supervisor/policy/greeter check; the test callback is not
deployment admission. Preserve receive-first bounded I/O, ancillary FD cleanup,
owner/reentry checks and permanent UNKNOWN fences; no reconnect or inferred drain.
Next implement real protected admission/initial connection, then actual PAM and
greeter/session transitions. For the built backend read spec/login-backend.md and
the 2026-11-10 login-backend-build journal/pins.
The greetd candidate now builds offline with the pinned34-crate graph/Tokio1.53.3;
upstream tests and source-preparer regressions pass. It is not an admitted daemon,
Unix peer or PAM/login service. Advance protected original daemon/config/socket
admission and bounded C transport, then actual PAM/greeter/session transitions.
Keep build-only Rust/Cargo/LLVM dependencies separate from the daemon footprint.
The optional C17 model passes
login.protocol/login.wire in Release/memory with no backend execution. It fixes
command/env, bounds the worker's escaped answer and never replays unknown start.
The exact greetd candidate is source-reviewed and rebuilt but runtime-unadmitted;
inspect the recorded connection/PAM/policy gates before use.
Advance maintained backend/runtime and protected connection admission, then real
PAM/greeter/session transitions. For prior evidence read the 2026-11-10
session-entry/native-session-owner journals and spec/session.md. The opt-in aetheri-session
entry consumes an existing local Wayland login, derives actual NSS account and
systemd runtime, and passes full Type=exec KMS workflow without notify. It is not
password authentication or installed display-manager/app admission. The C owner
also passes full KMS app/save/shutdown and active-VT-loss tests. The Linux self-
session observer rechecks original context without events and permanently latches
loss; it adds no process/thread. Run linux.session, linux.session_entry and owner tests in the
ordinary enforcing carrier. Advance normal login/account/service/device integration and
full system runtime closure; this private fixture is not login/app admission.
Preserve failed attempts and the old quarantine. The user's 9 November gate
forbids the next USB-targeted image build/flash until the agreed first version is
implemented and verified. Older next-flash notes below do not override that gate.
NexaUI handoff remains delivered; catalogue maintenance only.

Latest USB desktop boundary: read `spec/usb-desktop.md` and the 2026-11-08 USB
desktop journal/STATE. Full-size single-medium two-boot/persistence and independent
ext4 checks pass. USB2's OBS2 binds the complete root/data profile before handover;
real-root data checks require the original boot/device and same current parent.
USB1 stays fixed. Current bochs/serial evidence is not laptop acceptance. Actual
wrong-parent rejection now passes with new qcow2 overlays over prepared v3 bases;
raw bases AND guest-visible contents stay unchanged after exact original VM exit.
Validate both backing chains/sizes and offline integrity/compare; never repair or
commit live/unknown overlays. Retain failed raw fixtures and never replay them.
Native image inputs are fenced; two
host-only SGR parser/test updates were separately reviewed. Sparse regular-image
helpers are never physical writers; flash must write and verify zeros too. Next
finish wrong-data-filesystem testing, add laptop DRM/input/firmware and test without serial before the
already authorized Intenso G: flash. Reidentify, do not ask again, and continue
without waiting for the later user test. No flash yet; NexaUI handoff stays closed.

Root v9 now adds immutable unit_allocations: native prepare precedes filesystem
effects; only fresh confirmed prepare permits one O_TMPFILE/create-only-link
primary fragment. Recipe AND original directory/parent placement bind before
creation. Unknown commit/link/flush preserves pending intent and possible files,
never auto-recreates/adopts/cleans up. Complete selection is witnessed once before
REQUEST/ARM. The Root adapter requires that original witnessed selection/placement.
Read spec/unit-profiles.md; run catalogue.units, authority.unit_custody and the
cross-language/contract tests. Explicit quiesced upgrade_units is Root v8->v9 only;
ordinary create stays v2 and User v9 is unsupported. Preserve frozen v1-v8 SQL/XML.
Recovery inventory includes preparing AND witnessed unit obligations; no custody
retirement/purge or general volume/clone/owner-replacement admission exists.
The old quarantined executing fixture and normal installed v2 inputs stay closed.

authority/unit.h joins fresh native execution REQUEST to exclusive unit
custody. Read spec/runtime.md's Root request section. Constructor moves only an
unused unit against independent source/graph/service and starter-ELF witnesses;
read-only REQUEST-state queries never grant replay. Keep the Root catalogue hold
through Subscribe/Ref/Start, completed jobs and unknown releases. Original SYSTEM
cleanup context plus independently admitted original end/holders governs retire;
source fencing does not require renewed broker start delegation. Abandon drops
only local observation/hold. Keep its bus separate from the coordinator's bus,
and the adapter alive through callbacks. Run explicit authority.unit_custody in
the enforcing carrier. This is inert UID0 administrative/native-journal evidence,
not a normal installed registry/provisioner, production end/volume/recovery admission,
normal launch or lease settlement. Preserve the old execution quarantine.

Local Files creation now uses files/create.h: an immutable literal name/held
parent, preparation without effects, one worker attempt and cached retry results.
Existing entries are conflicts. Unknown mkdir/save/flush acknowledgements retain
possible objects/stages and block new creation for that window; close only frees
memory/FDs. No automatic retry/rollback/cleanup or durable restart reconciliation.
Read spec/files.md and run files.create/files.native. The alternate test_files_fault
binary is test-only; never install its lost-mkdir-acknowledgement hook.
Files rename.h supplies same-folder rename and explicit cross-folder move with
independent selected source/destination parents, O_PATH source and immutable names.
Directory moves reject observed self/descendant targets through bounded repeated
ancestry walks and flush their own original metadata FD plus both parents. Kernel
RENAME_NOREPLACE protects the destination, not source-inode compare-and-swap.
Recheck source/parent, retain unknown outcomes, never auto-retry/reverse/clean up.
Preview invalidation requires cancel/refresh; own document end precedes admission.
Unknown creation/rename blocks both; unknown rename also blocks new document starts.
The separate test_files_fault now includes a lost-rename-reply hook as well.
Window-local Cut/Paste holds an independent selection; clearing it never settles
an unknown move. Menus own their press/release through close. No cross-filesystem
copy/delete fallback exists. Read spec/files.md; run files.rename/files.move/files.native.
No durable restart recovery,
hostile-writer exclusion, normal grant or managed removal follows.

For software development, first read `D:/Skills/skills/AGENTS.md`, select relevant
specialist skills and read their required entry points and knowledge.
Start at `brains/aetheri/START.md`, then PROJECT, STATE, DECISIONS and QUESTIONS.

- NexaUI coordination: the requirement catalogue and initial chat message are
  delivered. Maintain `D:/NexaUI/NEXT_AETHERI_UI.md` in place when new requirements
  or evidence warrant a change; otherwise leave it unchanged. Do not generate or
  repeat a chat/start message or ask the user to paste one unless newly requested.
  After compaction, carry forward **handoff delivered; catalogue maintenance only;
  no pending chat message** and resume the actual task in STATE. Historical handoff
  records are not new assignments. See brains/aetheri/knowledge/nexaui.md.
  Treat routine NexaUI checks as background catalogue maintenance, not a recurring
  deliverable or next step for the user. With no relevant change, omit a NexaUI
  update from the reply. With a relevant change, a brief delta and catalogue link
  suffice; do not repeat the catalogue overview or implementation request.
- Files is an opt-in ordinary-user development view; read spec/files.md.
  Keep the C17 directory model separate from SDL, complete bounded snapshots and
  held parent navigation, raw names separate from display labels, and rejected
  filter intents unable to activate a retained prefix. A selected file's held
  O_PATH witness passes literally through a separately owned Terminal/PTY to the
  editor and is compared before reading. No shell interpolation or symlink
  following. Preserve the original direct child across pending/unknown results;
  UI close retains the editor's dirty-content choice and Files waits for Terminal
  end. These FDs/observations are not document grants, sandbox/volume identity,
  descendant drain or durable recovery. Run files.model/launch/native plus affected
  editor/terminal/shell/desktop regressions. Fixed development desktop launch now
  exists; normal app admission remains work.
- Document now owns bounded delta undo/redo: read spec/editor.md. Keep monotonic
  revisions distinct from logical saved-state identity. New successful edits
  discard redo; failed admission/allocation must preserve it. Preallocate before
  mutation; raw inverse grapheme splits are private original-history only. Reuse
  retained capacity on traversal and evict complete entries within both budgets.
  Modal/UI history never acknowledges or rearms an uncertain save. Run
  editor.history/document/modal/file/native plus provider groups. Signal exit
  now drains remaining queued managed signals without repeating effects after
  its first decision; retain the test-only fsync gate, not a production hook.
- Modal Visual/Lines selection and its bounded private register are app-owned.
  Read spec/editor.md: inclusive grapheme endpoints, original line endings, final
  line removal, linewise-change empty line and paste separator rules. Stage copy/
  assembly/document/index/history growth before mutation; failures preserve the
  old register, selection, text and redo. replace_line_start admits original line
  starts and records that result cursor in the same undo entry. Wipe replaced
  register bytes; never imply platform clipboard or file/save authority. Run
  editor.selection plus document/modal/history/view/native/provider groups.
- Literal document search is app-owned: read spec/editor.md. ae_document_find is
  read-only, bounded, case-sensitive and admits whole-grapheme endpoints only.
  Preview/cancel preserves original cursor and prior accepted query; rejected
  input cannot accept an old prefix. n/N recompute after edits, without wrapping.
  No search settles UNKNOWN or changes text/revision/clean/history/file state.
  Keep minimum-size query hints, inverse match and query focus. Run editor.search,
  view/native/provider plus affected groups; accessible text/richer search remain.
  The old 200-percent no-match image contains zerr, not zero; read the native-key
  input record before reopening it as a renderer/search defect. Use verified
  tap_keys and actual target/text/frame gates; QMP success or stable pairs alone
  do not establish intended text or visual acceptance. No timer cause is proven.
- AETHERI_EDITOR now builds the separate aetheri-editor development executable.
  Read spec/editor.md: main joins existing document/modal/file libraries to the
  pure bounded view. UNKNOWN is independent of clean state; q/CtrlQ must not
  silently close it, save-as cannot rearm it, export is a separate create-only
  copy and discard requires literal DISCARD. Preserve extra uncertain exports.
  Own TTY OFD/foreground checks, alternate screen/modes, bounded UTF-8/key/command
  rejection and no reads beyond quit are required. Below 30x8 refuse hidden
  editing/commands. Stop restores modes; background resume stops again. Emergency
  signal copies are best effort, not durable recovery. Run editor/native plus
  shell/terminal/provider groups. Normal grants, search/panes/accessibility
  and final UI remain work; preserve the quarantined runtime.
- editor/file.h now supplies a separate local development load/save adapter.
  Read spec/editor.md: repeated original byte/metadata checks are not atomic CAS
  or a production document grant. Preserve metadata and pre-rename target bytes;
  uncertain rename/flush/close fences later saves. Uncertainty is independent of
  document dirty state, including an already clean submitted document. Never
  rollback/retry or delete a potential artifact after an unknown outcome. Run
  editor.file plus document/modal/grapheme tests; the application has its own
  view/command layer. Durable brokered/restart reconciliation remains work.
- Shell history search now has its own bounded query/preview: preserve original
  draft/cursor/undo until accept; Enter accepts only, later Enter submits. No-match
  and rejected input cannot reuse an old result. Run editor/search/native groups.
  New src/editor document/modal are pure model libraries, not an editor executable
  or file/save authority. Read spec/editor.md: UTF-8/ending/gap/line/grapheme and
  allocation/revision contracts. mark_clean is only local generation state.
  The app owns its separate view/commands; full reconciliation remains work.
  Keep installed profiles/capabilities and quarantined old intent unchanged.
- The graphical terminal now selects the own `-i --edit` single-line profile.
  Read `spec/line-editor.md`: toolkit-free editor/history, original terminal/OFD
  ownership and whole-draft invalid/overflow rejection. Preserve plain `-i` and
  original child input/modes. Unicode18 generated data uses separate Unicode-3.0
  licensing; reproduce with tools/generate_graphemes.py downloads --check. Run
  text.graphemes, shell groups and relevant terminal/desktop/Compose integration.
  Bounded undo/redo and literal command/path completion now exist. Preserve undo
  grouping/branch/budget and clear/reset/history lifetimes; no undo may revive
  blocked input. Completion only proposes literal quoted tokens, with no exec,
  cwd/env mutation or content reads. Keep budgets/cancellation and real parser
  round trips; filesystem hints never authorize later operations. Run shell.completion
  plus editor/native/terminal/provider groups. No programmable completion/daemon.
  Grapheme segmentation is not shaping; advanced completion/richer search, modal editor app,
  panes and persistent sessions remain work. Preserve the quarantined old runtime.
- Optional built-in Compose is documented in `spec/compose.md`: build and runtime
  opt-ins, reviewed embedded data/license, no external input-method/virtual-keyboard
  global. Retain original field/client/focus admission, including pending enable
  invalidation across leave/re-enter. Run the compose model/input/provider/lifecycle
  checks and relevant desktop tests. This is not external/CJK IME, trusted lock or
  normal app/session admission. Keep source-data licensing separate from our MIT.
- Follow the user's agreed foundations: Linux x86-64/UEFI, C17 primary, targeted
  Rust and small C++ adapters where justified, existing open development tools,
  glibc and the discussed technical-library boundary. Exact dependency versions
  and substantial new families need evidence, rationale and compatibility/security
  review. While temporarily unavailable, the user authorizes well-supported
  recommended technical choices; document them and defer personally important
  product/UI/destructive choices. Do not repeatedly ask noncritical questions.
- Approved architecture: controlled modular system (B), original wlroots-based
  Wayland compositor, selected systemd/session/sleep components, SQLite metadata,
  versioned read-only system base and separately managed apps/tools with enforced
  ownership boundaries. Do not reopen these choices without material new evidence.
  Exact releases and remaining sandbox/update/browser integrations need review;
  architectural selection is not implementation or runtime acceptance.
- Approved module direction: privileged system authority plus per-user mediation,
  compositor/UI separation, typed local D-Bus/sd-bus control, owner-scoped SQLite
  writers and recoverable operations. The identity/lifecycle model is accepted.
  The initial SQL/XML management contracts and limits are accepted in
  brains/aetheri/knowledge/schema-and-api.md. Run tools/validate_contracts.py for
  isolated constraints; acceptance does not imply installed APIs.
- Filesystem/ownership/enforcement direction is accepted in
  brains/aetheri/knowledge/filesystem-and-permissions.md, including ext4, LUKS2 and
  SELinux plus namespaces as foundations. Exact releases, policy and runtime
  acceptance are pending. Do not create the Linux tree or change policy on Windows.
- Launch/document/revocation/removal protocol and P0 scope are accepted in
  brains/aetheri/knowledge/launch-access-and-removal.md and
  brains/aetheri/knowledge/first-linux-prototype.md.
- Versioned v2 SQL, additive P0 XML and development migration/tests are implemented;
  read spec/README.md and run tools/test_contracts.py. Keep v1 contracts unchanged.
  This contract portion does not implement Linux lifecycle or document transport. Native
  edits/bulk/direct/saved access remain later work. Dedicated carrier/tool setup now
  exists; read STATE for the exact executed boundary.
- P0 fixture/owner/intent byte references and frozen vectors are in spec/runtime.md;
  run tools/test_runtime_contracts.py. VM input pins/cache tooling are in
  spec/vm-inputs.json and tools/prepare_vm.py; run tools/test_vm_inputs.py.
  QEMU's checked Windows installer has an expired Authenticode certificate; the
  reviewed provision_windows.py reads MSI/NSIS archives as data without installer
  execution. Do not equate source hashes with valid Windows codesignatures.
  Native core/fixture now decodes canonical AETHFIX1 into borrowed bounded text
  views. Retain immutable input while using them; never treat views as C strings.
  It validates metadata, not content hashes, publisher trust or installation.
  OpenSSL 3.5 LTS/libcrypto is accepted and reviewed at Fedora 1:3.5.9-1.fc44;
  exact RPMs/hashes are in spec/vm-inputs.json. CMake/runtime reject other families
  and versions below 3.5.9. core/digest uses fixed EVP SHA-256 in a private context;
  fixture verification does not replace publisher trust, inventory or staged import.
  core/import now checks complete trusted-fixture inventories, copies/hashes private
  staged payloads and repeats required flushes. A protected system-domain P0
  administrative path now binds a preallocated carrier slot and native install
  intent/journal before payload writes; it reopens after a guest boot. Read
  spec/runtime.md's administrative stage journal and store publication. The trusted
  fixture now publishes a manifest/payload container through no-replace same-mount
  renames, immutable AETHSTO1 object evidence, full repeated flushes and final MAC
  sealing. Publication alone leaves install preparing/running-reconcile; the
  separate AETHACT1 first-activation path now atomically completes the original
  install operation, with its gate closed and no lease/run. Completed receipts
  are historical: never rearm/restore a generation, change later gates or undo
  removal on retry. Read current STATE/runtime contracts. General dynamic allocation,
  updates, caller admission and complete recovery coordination remain pending. After moving
  the payload use fixture-store-check, not the original stage/rebind checks.
  The slot ID is not a private-data StorageRef. Never adopt a candidate root from
  matching content/name; retain independent configuration/operation bindings.
- Native C17 ownership/storage primitives and tests exist under src/core and tests.
  core/root requires STATX_MNT_ID_UNIQUE and joins held/named mount/inode/filesystem
  observations. These are runtime facts, never durable volume/clone authentication.
  storage_ref derives its leaf from StorageRef and binds an independently expected
  owner/root under a trusted anchor; caller still owes account/volume/MAC admission.
  Returned file FDs are internal bounded-read obligations, not native app transport.
  Closing a storage reference never revokes an already returned FD.
  core/allocation adds immutable AETHALC1 object witnesses (168 bytes, frozen vector).
  Birth time is mandatory for these records, optional for generic root snapshots.
  Capture requires independently authorized, flushed allocation setup; never adopt
  a candidate tree from its own header. Publishing uses O_TMPFILE/create-only link/
  fsync; unknown acknowledgements preserve records. This is not volume/clone
  authentication or a completed allocation/purge journal. The optional protected
  storage fixture is now retained in the guest; empty-bootstrap cleanup must refuse
  it until a reviewed storage-retirement protocol exists. Use storage-check for
  native same-intent reconciliation, never mint replacement IDs after a lost reply.
  They are not an installed authority, public grant API or Aetheri app sandbox.
  Dedicated guest runs Fedora enforcing policy. A temporary Aetheri app-domain/MCS
  fixture probe is tested and removes its module afterward; production authority/
  broker/app domains and category allocation remain unimplemented.
  QMP must stay host-local AF_UNIX under the protected VM directory: guest slirp
  networking can reach a host loopback TCP listener through 10.0.2.2. Never restore
  unauthenticated TCP QMP. Use run_vm.py status/live process evidence before retries.
- Native src/linux/peer captures bus-authenticated connection credentials and a
  process pin. This is not complete app/run authorization or current-domain proof.
  Keep required ProcessFD/provenance checks; do not replace them with UID/PID paths.
- Native src/linux/process joins that held ProcessFD to current UID/label, an
  active system service invocation and cgroup-v2 identity. Read the process proof
  in spec/runtime.md. Trusted host system bus/proc/cgroup views are prerequisites.
  Its self-pin API is only for the calling process, never a substitute for a
  missing caller ProcessFD. Snapshots are not full app/run admission or target-exec
  evidence. The root endpoint checks its own supervised context on discovery;
  complete app registration, mutation fences and production policy still remain.
- Native src/catalogue now creates/validates v2 and commits startup context using
  build-embedded canonical SQL. Read spec/README.md#native-catalogue-boundary.
  No production registry/MAC/authority or native v1 migration is implied. Keep
  writer.lock stable, never close independent DB-inode FDs during SQLite locks,
  and never infer rollback from a lost commit/flush acknowledgement.
  System-owner account enrollment/check now uses existing scopes rows. Exact
  active retries are idempotent; never rebind/reactivate an old scope or adopt its
  private state on UID reuse. Only explicit initialization/admin enrollment may
  insert the fixed test account; normal startup and broker use require it already.
- tools/test_linux_policy.py is an explicit root-only disposable-carrier test,
  never a default host test or production installer. It must stay enforcing,
  preserve uncertain dispatches and refuse foreign fixtures/modules. Fixed test
  categories are not a production category allocator or complete A3 acceptance.
- src/authority implements two confined prototype protocol endpoints, generated
  from accepted XML. Read spec/runtime.md#confined-protocol-services. Public discovery
  and private P0 Broker1 enrollment/check now work; no lifecycle capabilities are
  advertised. Read the broker enrollment section in spec/runtime.md. Bootstrap is
  one protected UID-1000 account/unit, not general account/launch admission. Losing
  a name/process drops identity only; never infer app drain or release leases.
  tools/p0_services.py explicitly
  provisions/checks/stops the dedicated guest bootstrap. lifecycle-check removes
  and recreates only an inspected empty bootstrap, never a production app install.
  Preserve unknown/pending dispatches, processes, jobs and unexplained entries.
  Its admin update/cleanup interruption needs manual reconciliation; do not claim
  production power-loss recovery. User roles follow the login manager, no lingering.
- src/authority/broker owns the retained peer pin and narrow name/exit subscriptions.
  Registration may observe a known Type=notify service while activating; every
  use still verifies active state. Caller-supplied IDs cannot select the unit.
  broker-check proves departure FD cleanup, user replacement and root-only restart.
  Cache tests mock context but use real name-loss/pidfd events; separate enforcing
  integration rejects unconfined and same-domain/wrong-unit impersonators.
- Use English for project documentation. Conversation can remain German.
- The opt-in C17 graphical terminal is in src/terminal; read spec/terminal.md and
  run terminal.model/pty/native with the reviewed UI profile. Fork its original
  shell before SDL/Mesa/worker creation. Retain independent child/pidfd and stream
  observations; neither EOF nor a close request proves descendant/cgroup/lease
  drain. Failed event posts must not abruptly abandon ownership. TERM=dumb is
  intentional; do not claim complete VT/Unicode/editor/IME/a11y or persistence.
  Its test-only event-failure executable is not a normal app dependency. The
  carrier Mono font has explicit font/OFL hashes and limited installed-RPM
  evidence, not a new independently verified RPM/distribution package. Normal
  development owner/dock integration now has independent Settings/Terminal/Files
  slots.
  Normal admitted app/session launch remains separate. Keep app/model
  state toolkit-independent for later NexaUI migration and preserve old fixtures.
- Private Catalogue1 1.1 / Development1 1.4 support a bounded authenticated install
  observation and fixed-fixture refresh. Public capabilities remain empty. A user
  binding is not admission, storage ownership or current generation authority.
  projection-check uses a root-selected short-lived user-domain helper with actual
  process/unit/invocation/cgroup checks; clear its protected slot after use. Keep
  the retained private allocation fixture separate from future app DataEpochs.
- The first app DataEpoch/StorageRef is now enrolled through a separate protected
  P0 fixture, not adopted from the old storage fixture. Read catalogue/data.h and
  spec/runtime.md's initial data enrollment. AETHDTA1 freezes both authority epochs,
  exact binding/baseline and independent AETHALC1 object witness. data-check repeats
  filesystem verification/publication then atomic native data+attachment+closed
  gate enrollment. Retired/detached/conflicting data is never reactivated; a live
  exact retry never changes later gate state. Root setup remains preallocated test
  infrastructure, not a general allocation operation journal. Preserve both fixtures.
- Distinguish confirmed requirements, architecture proposals, implementation and
  test evidence. No runtime function exists merely because it is documented.
- Native catalogue/reservation.h and Runtime1 now reserve/query/withdraw code.
  DispatchLaunch remains unsupported and public capabilities empty. The P0 wire
  profile is only the protected original I/G/D, main/p0/none. Never treat a lease
  receipt or gate observation as data readiness or process execution. Only a
  still-reserved tuple can be atomically withdrawn; uncertain/observed/dispatching
  state requires reconciliation, never inferred drain. Development1 is now 1.4.
  Explicit system --admit-fixture rechecks the current sealed store using a
  boot/original-catalogue-baseline-bound AETHADM1 before opening reservation
  metadata. Ordinary startup stays closed; stale admission config must fail.
  reservation-check/new-check/continue/replay-check preserve protected intent and
  terminal history. These are fixed P0 tests, not normal user launch acceptance.
- Native catalogue/launch.h now journals user acceptance before root RPC, binds
  actual root receipts (Catalogue1 minor 1.1), and reconciles proven withdrawal
  after broker replacement. User startup marks unsettled runs recovery_required.
  A current same-authority/account broker may withdraw an old still-reserved
  tuple without rebinding it; root uncertainty/readiness/exec must never infer
  release. launch-check/cancel/reopen-check preserve fixed correlations/history.
  Private data gate admission is not view readiness; no actual app dispatch or
  public launch capability exists yet. Keep SQL v1/v2 and Runtime1 XML unchanged.
- Native linux/view.h now clones exact code/data FDs into private mount views.
  It is for fresh trusted single-threaded setup only, not a permanent authority.
  Entry can change namespace on later failure; exit rather than fall back. Move
  attempts are not retryable after uncertainty. Closing view/namespace handles is
  not drain or lease release. Real kernel tests run as administrative unconfined_t
  in the enforcing guest; the separate supervised no-exec setup path below now
  exercises setup_t. App policy and target readiness remain pending. Preserve the
  existing source/MAC/content admission requirements.
- src/launcher and private Setup1 1.0 connect view setup to the original Root
  journal. The fixed helper cannot execute app code. Root pins actual ProcessFD /
  unit invocation and separately observes terminal unit/cgroup/job drain before
  releasing this no-exec attempt. Read spec/runtime.md's supervised setup section.
  Root's ordinary reservation withdrawal rejects additional setup steps. Early
  failed helpers have a distinct supervisor-only proof, never a fabricated peer;
  it depends on the unchanged protected image/unit and original Root runtime/boot.
  Offline --recover-setup keeps that runtime under the exclusive catalogue lock.
  Do not reuse this proof for a target-executing starter or uncertain dispatch.
  Preserve old main tuples and the immutable run/c31,c32 fixture; actual target
  exec needs a new compatible fixture for c33,c34 data. Public capabilities remain
  empty. No production app launch or complete removal yet.
- linux/execute.h and image.h now provide isolated one-way execution and current
  image observation. They are not wired into the installed helper/authority.
  Read spec/runtime.md's execution section. EOF/exit 125 is never exec/readiness
  evidence; never extend the no-exec withdrawal proof to an executing starter.
  test_execute_policy.py installs only its separate temporary test domains and
  retires them after exact fixture and independent unit/cgroup/job drain checks.
  Unknown dispatch preserves pending state. Test-only SYS_PTRACE/KILL belong to
  the parent harness; the exec boundary accepts neither. Actual app admission,
  observation, recovery and drain integration require separate evidence.
- The separate aetheri.p0.exec fixture is now published/activated, gate closed.
  Read spec/runtime.md's separate fixture/action section. Fixed original/exec
  administrative registry selections share the existing store/catalogue but have
  separate immutable inputs, slots, identities and receipts. The controller's
  exec_fixture subtree preserves the original. Normal broker/data/no-exec paths
  still select the original; no implicit main-to-run or DataEpoch rebinding.
  core/fixture's action resolver returns owned literal argv/file metadata only.
  Private Development1 1.4 now adds RefreshExecFixtureInstall. The new fixture has
  its own projected binding/DataEpoch/allocation; both gates remain closed. Native
  data enrollment checks the exact fixed raw MAC context, UID/GID and selected
  install/publisher. Exec data uses c35,c36; original data keeps c33,c34. Never
  reinterpret an allocation-operation ID as policy identity. Read spec/runtime.md's
  scoped execution-fixture data section. Existing owner bytes/SQL stay unchanged.
  Actual executing dispatch/registration/readiness/drain remains pending.
- catalogue/execution.h now defines the distinct AETHEXE1 native execution
  profile and owner request/bind/prepare/arm journal. Read spec/runtime.md's
  durable execution section; run catalogue.execution. Fresh-only effect_required
  is zero on every retry/lost acknowledgement. Its stage/generation step cannot
  use old no-exec stage/launch withdrawal. This is not a live starter/DispatchLaunch
  implementation or image/readiness/drain proof. Do not install/dispatch it as one.
- linux/exec_channel.h now provides the process-owned bounded error reader and
  checked anonymous writer. Only the issuer keeps the reader; future live Root
  must bind it to the exact journal/held starter before exporting a writer reply.
  Incoming-FD method-call denials remain. sd-bus append duplicates, read borrows:
  close all issuer/message copies after transfer and seal the starter's writer.
  UNKNOWN EOF is never execution/readiness/drain; unregister terminal watches.
  The peer/execute tests use it, but no live Execution interface exists yet.
  Run explicit test_execute_policy.py with Release; Debug LeakSanitizer needs a
  same-domain ptrace permission deliberately absent from that test policy. Keep
  ASan/UBSan in the normal native suite; do not weaken policy for instrumentation.
- authority/execution.h now joins native journal/pin/channel/event handling in a
  Root-only coordinator. Its compiled effect adapters run after fresh commits;
  unknown request/release/bind never permits duplicate effects/writer issuance.
  Re-observe original context; observed drift stays quarantined. Sample channel
  before arm. sd-event terminal watches stop; close never frees a lease or stops
  the process. Callback-time close is EBUSY. Run authority.execution_service;
  its process/PID1/effect adapters are controlled fixtures, not live admission.
  AETHEXD1 diagnostics persist immutable step 4 + bounded reason with no outcome/
  lease change. Retry only acknowledgement. Read spec/runtime.md's coordinator
  section. This library has no installed wire/provisioning/starter policy yet;
  do not claim runtime/target readiness or use no-exec drain to settle it.
- authority/execution_profile.h now binds protected config/publication/manifest/
  ELF and private owner/allocation/context into held sources. Read spec/runtime.md's
  protected execution-profile section. The 0700 store parent is not a sealed 0555
  package. Source checks are read-only, not launch/view/volume/runtime admission.
  execution.conf/execution-starter and aetheri_starter_exec_t remain reserved and
  unprovisioned; positive starter admission still needs actual policy/image tests.
  The optional --live-read-only probe uses synthetic build-local request IDs and
  keeps gates closed. Never install it as a real dispatch profile. Shared pure
  fixture config codecs now live in fixture_profile.c. core/import's check_payload
  has no fsync; preserve publication sync/durability behavior.
- launcher/execution.h now supplies the native executing-starter client. It pins
  actual Root discovery/process/unit/invocation/context and verifies same-owner
  correlated replies plus exact binding/revision before accepting writer/release.
  Registration/prepared each run at most once; no owner replacement or retries.
  peer_capture_reply requires the retained sent unique-destination request and
  matching bus/cookies; capture outputs must start empty. Execution1 1.0 generates
  a separate client header only, not default installed method tables. Read the
  authenticated-client section in spec/runtime.md. Explicit test_execution_client.py
  uses a Release producer in the actual Root domain; optional Debug client stays
  outside it. Temporary pipe/bus-write grants retire only after exact owned drain;
  never grant the bus read access to Root's reader or ptrace for instrumentation.
  The synthetic producer has no catalogue/views/target exec. Actual starter,
  protected Root unit/profile provisioning, producer admission/ARM handlers and
  target readiness/drain remain work. Keep ordinary installed inputs unchanged.
- MIT applies to original Aetheri code/documentation. Preserve third-party licenses.
- catalogue/execution.h now persists immutable AETHPRC1 process observations in
  explicit v4 execution_processes. Read spec/runtime.md's durable enrollment;
  run catalogue.processes and contract/migration tests. Freeze v1/v2/v3 SQL/wire;
  normal create stays v2 and open validates exact v2/v3/v4 (Root also v5) without migrating.
  upgrade_processes is an explicit quiesced-owner v3-to-v4 action under the same
  retained lock; no existing rows/revisions/gates change or missing evidence is
  backfilled. Root starter precedes ARM, Root app retains that exact kernel
  identity after ARM, User app follows registration and precedes READY. V4 fresh
  ARM/Root observation/User READY require their validated original record.
  Enrollment needs the original runtime; exact retries keep time/bytes. The
  historical same-authority/account/boot getter permits a replacement runtime
  without adopting a run, restoring readiness or releasing a lease. Current
  process/image/data/holder admission and an actual recovery transaction remain
  separate. End/lease settlement precedes evidence pruning; no full purge exists.
  Actual confined tests enroll all three observations, but production restart
  coordination, already-dead/lost-handle recovery and normal provisioning remain
  work. Ordinary installed catalogues stay v2 and public capabilities empty.
- linux/process.h now captures/reidentifies AETHPID1 (boot + 64-bit pidfs inode)
  from an authenticated live ProcessFD and independently admitted current context.
  Read spec/runtime.md's observer-replacement section; run linux.process_identity
  and linux.process in the explicit carrier suite. Mandatory PIDFD_GET_INFO checks
  bracket direct fstat on the accepted x86-64 pidfs kernel; no numeric-PID fallback.
  Do not widen policy for libsystemd's optional self-pidfd/fstatfs/handle probes.
  Fresh bus names can identify the same original process, never authorize a new
  owner/run or recover an old connection/image/lease/cgroup handle. Persist original
  identity with independently protected run/context before relying on recovery;
  never adopt candidate-supplied evidence. The actual replacement-observer test
  keeps its native context envelope private/test-only. Confined Root/User probe
  identities now have separate v4 enrollment above; restart settlement remains work.
- catalogue/lifetime.h now journals successful original-run closure separately
  in explicit v3 run_closures. Read spec/README.md and spec/runtime.md's separate
  durable run closure section; run catalogue.lifetime. Keep frozen v1/v2 SQL and
  wire unchanged. Native create stays v2, open validates exact v2/v3/v4 (Root also v5) without
  migrating; upgrade_lifetimes is an explicit quiesced-owner action under the
  retained writer lock. It adds no rows/admission/revisions to existing tables.
  Never restore a backup after unknown commit. A completed start operation,
  revision/time/result/steps and AETHLCR1 receipt remain immutable on close.
  Root closing -> authenticated User quiet -> independently admitted original
  end/Root terminal+lease released -> authenticated User exited. Digests/frames
  are metadata, never process/quiescence or bearer proof. Get receipts are
  historical, not fresh observations. Original runtime/owner replacement requires
  separate recovery; unfinished v3 closures become uncertain. The P0 User path
  requires no grant/access rows. Actual fixed test provisioning excludes future
  dispatch/restart/migration/delegation/FD-store/export holders; general holder
  settlement is unimplemented. Root rechecks original broker just before mutation.
  New methods stay only in explicit test vtables; normal installed catalogues
  remain v2 and capabilities empty. Do not use old no-exec withdrawal to close
  an executing attempt, or treat an isolated successful-run close as full removal.
- linux/lifetime.h now provides a Root-owned read-only original-process/unit/
  recursive-cgroup end observer. Read spec/runtime.md's lifetime section; run
  linux.lifetime in the explicit privileged carrier suite. Borrow the caller's
  original unit reference and trusted system bus; never Unref/stop/release from
  observer close. Same held objects/mounts, invocation, dead pin, terminal unit/
  no job and empty/deactivated original group are required. st_nlink remains 2
  after removal on this carrier: require missing name AND held event-reader
  ENODEV, never missing-name/link-count/deleted-path inference alone. Other
  kernel behavior fails closed. Restart/namespace/lookup drift quarantines.
  Actual root-only PID1 tests use a private component bus; property/lookup faults
  are controlled (v2 rejects actual rename). The connected enforcing test captures
  after actual Root/User execution confirmation and authenticates ObserveEnd only
  in its test interface. Archive native_end as historical snapshot; the separate
  v3 path above now settles only the successful fixed run. General future-dispatch
  fencing, external/delegated holder settlement, recovery and removal remain work.
  Never use the old no-exec withdrawal proof to settle an executing attempt.
- catalogue/execution.h now separately completes Root/User start results through
  ae_catalogue_complete_execution / ae_catalogue_finish_user_execution. Read
  spec/runtime.md's launch-result completion section; run catalogue.execution_completion.
  AETHLCR1 is a 200-byte immutable receipt, never a lease/drain capability. Original
  Root replies, actual app/User/current-image/private-data admission remain caller
  obligations. Root requires observed execution plus the exact settled UNKNOWN
  diagnostic; UNKNOWN alone is no exec proof. Preserve its reason. Root step6 and
  User step4 acknowledge applied launch results without changing live dispatch,
  launch, gates or lease revisions. Exact receipt/time retries preserve later
  state; owner replacement needs separate recovery. Test-only FinishLaunch/Complete
  and launch-completion.bin are not a normal SDK or production close/removal API.
  Archive completed start results AND the still-running lease/run obligations.
- catalogue/execution.h now has ae_catalogue_observe_execution: original Root
  profile/starter binding plus independently authenticated app/User evidence
  records step5 and observed dispatch, retaining the running lease/operation.
  Read spec/runtime.md's Root observation section; run catalogue.execution_observation.
  A digest is not process/IPC proof. Historical exact retries preserve later
  closing/terminal/uncertain state and gates; never rearm or infer lease release.
  Preserve the bounded step4 diagnostic reason in either ordering; dispatch state/
  step5 separately retain actual observation. The explicit
  live fixture adds PinApp/ConfirmApp only in test interfaces; no installed API/
  capability/policy change. Keep actual original pins/unique connections and User
  current-image/private-data checks. Executing durable drain/recovery remain
  pending; administrative failed-test retirement preserves both
  unsettled catalogues and is never a no-exec release or production recovery.
- catalogue/execution.h now separately journals User REGISTER and READY through
  ae_catalogue_observe_user_execution. Read spec/runtime.md's User-mediated app
  registration section; run catalogue.user_readiness. Original process/Root/
  image/private-data evidence remains the caller's obligation. REGISTER binds
  actual invocation/context; READY requires the same actor/evidence. Exact retries
  preserve later state; restart/gate drift rejects. Neither completes operations
  or releases Root's lease. The explicit live fixture exercises actual cap-zero
  User process/current-image checks and rejects premature/foreign-connection READY.
  AETHIMG1 is a 112-byte bounded observation codec, never provenance by itself;
  AETHOBS1/AETHAPP1 are test-only Root-observation/context envelopes. The temporary
  User policy allows exact proc/executable reads, no ptrace/capability or code-store
  traversal. Normal SDK/session provisioning, Root settlement and executing drain/
  recovery remain pending. Public capabilities and installed profiles stay closed.
- authority/execution_admission.h now connects the original registered User peer
  to native Root request/arm. Read spec/runtime.md's connected-owner section;
  run authority.execution_admission and the explicit live fixture. It verifies
  endpoint/profile equality, retains an independent original peer pin before any
  request, and rechecks active broker identity/unit/context before ARM. Unknown
  requests cannot repeat; replacement cannot inherit delegation. Caller still
  owes protected User code/data and mutation/revocation coordination. Split code/
  private-data readers retain combined starter checks; no User SQLite is read by
  Root. The live fixture now uses an actual UID1000 system-scope User service and
  its own native catalogue/preparation, not a normal login session. Its temporary
  bus name must map to the existing User service type; inbound-FD denials remain.
  Bootstrap acknowledgement requires exact independent slot/context/new-file
  evidence; native SELinux xattrs may have one terminal NUL, frozen afterward.
  Archive BOTH unresolved owner catalogues after exact original process/unit/
  cgroup/job test drain. This is not production readiness, release or removal.
- catalogue/execution.h now has ae_catalogue_prepare_user_execution. Read the User
  preparation section in spec/runtime.md; run catalogue.user_execution. It binds
  complete AETHEXE1 in User step1 before a possible Root execution request. Only
  fresh successful commit returns submit_required=1; retries/lost acknowledgements
  never authorize replay. Old User withdrawal refuses the step; startup retains
  it as recovery_required. Current Root/broker/still-reserved receipt provenance,
  private allocation/MAC and mutation exclusion remain caller obligations. This
  is not current owner RPC, filesystem admission, ARM/readiness/drain or a release
  protocol. No SQL/XML/installed capability changed; preserve the normal inputs.
- launcher/preparation.h and aetheri-execution-starter now join protected self/
  source checking, the authenticated client, private views and one-way exec. Read
  spec/runtime.md's separate starter section. AETHVWS1 (184 bytes, frozen vector)
  binds four independently recorded scaffold objects to the entire AETHEXE1;
  never adopt a recreated/matching-name directory or reuse the no-exec scaffold
  proof. Source checks happen inside the private namespace; held/named mounts and
  image are checked again after release. Failure must exit, with no retry/fallback.
  The fixed P0 Root unit is aetheri-p0-execution-root.service; target UID/GID1000,
  c35,c36, manifest literals and three fixed environment entries are fixture-only.
  No normal starter/profile/scaffold record/policy is installed. Flow adapters
  are controlled; actual confined positive admission, Root provisioning/live
  request/data mediation and executing target readiness/drain still remain.
- An explicit tools/test_execution_runtime.py now joins actual native Root journal/
  PID1 request, unchanged starter/private views and UID1000 executing target in
  the enforcing disposable carrier. Read spec/runtime.md's isolated live test
  section. It uses a separate privately bound root and controlled broker/data
  fixture admission, never normal installed inputs. Temporary aetheri_exec_app_t
  preserves old app-domain denials; compiled target SID changes the profile-bound
  starter digest. Root/starter get no SYS_PTRACE/KILL; the administrative
  driver independently checks the running ELF and terminates the exact app pidfd. Fresh
  post-exec credentials join the original held process/invocation/cgroup. Unknown
  starts preserve intent/fixture/policy. Exact process/unit/cgroup/job drain and
  full owned-input/tree checks precede test retirement; archive native unresolved
  lease/operation history, never use no-exec withdrawal or infer release from EOF.
  No production admission/readiness/recovery/removal or public capability follows.
- authority/execution_wire.h now supplies the separate journal-backed Execution1
  producer component. Read spec/runtime.md's endpoint section. It retains one
  original prepared call until fresh native ARM, consumes before send and checks
  one four-second deadline before and after commit. Expiry/send uncertainty never
  releases leases or proves drain. Run authority.execution_endpoint: real isolated
  owner/bus/ProcessFD tests use controlled actor/PID1 adapters on an owned private
  broker37 socket/config. No global bus/policy change or dbus-run-session dependency.
  The launcher uses system scope for its own PID1 subscription only. This library
  is not connected to the installed authority; actual Root/starter provisioning,
  live supervisor request/current broker-data admission, target readiness and
  independent executing drain/recovery remain pending. Keep normal inputs closed.
- linux/lifetime now exports/restores four private original observation handles.
  Read spec/runtime.md's retained-observation section. The native checkpoint is
  not a portable persistent record or journal adoption. Independently protect
  the original expectation before transport; never adopt incoming names/objects.
  Restore uses the exact retained root across PrivateMounts, rechecks original
  PID/cgroup namespaces and keeps ordinary capture's named-mount quarantine.
  linux.lifetime_handoff tests actual PID1 retention after observer exit and app/
  group disappearance, with capability-empty administrative unconfined_t services.
  Its four-entry store uses FDPOLL=0 and explicit retirement; a barrier is not set
  acceptance. The independent test controller retains the original unit. No normal
  Root FD store, production unit retention or Root/User recovery is implemented;
  the separate journal below does not enable them. The app FD store stays forbidden.
  Preserve uncertainty.
- catalogue/handoff.h adds canonical AETHHND1 and an explicit Root-only v5
  journal linked to the original v4 app witness. Read spec/runtime.md's durable
  handoff section. User remains v4; normal create remains v2, open never migrates,
  and frozen v1/v2/v3/v4 SQL/wires stay unchanged. Existing-intent retries/lost
  successful commits never permit another transfer/retirement effect. Current
  observer incarnations cannot replace the origin, adopt a run, restore readiness
  or release leases; retired retries never rearm. Actual descriptor-set/absence
  and protected custodian admission remain caller obligations. The real component
  uses the canonical frame with explicit synthetic digests/UID0; it is not a fixed
  UID1000 execution-profile admission. No installed Root producer/receiver,
  production unit retention, partial-set or early-starter recovery is implied.
  Run catalogue.handoffs and the existing linux.lifetime_handoff component test.
- linux.lifetime_handoff_retention separately tests a protected process-free
  target retaining original custodian/app metadata after all original probe
  clients exit and the controller closes its host-bus connection. The controller
  uses zero RefUnit calls; actual restoration/end/store retirement precedes exact
  target retirement and all-three-unit GC. Read spec/runtime.md's original-unit
  retention section. This is trusted administrative carrier configuration, not
  production Root provisioning/reload exclusion or v5 recovery integration.
  Preserve uncertain target/file/job state; do not infer drain from target stop.
  Normal inputs, policy, closed gates and public capabilities remain unchanged.
- authority/handoff.h now joins actual lifetime export to the enrolled v4 app
  witness before v5 PREPARE, and protected SQL expectations to restore/OBSERVE.
  Read spec/runtime.md's native bridge section. Only a fresh confirmed intent
  returns transfer FDs; existing/lost-successful-commit retries return none.
  Incoming FDs stay caller-owned; exact supervisor inventory/transport and
  original-unit retention remain caller obligations. Current self admission must
  already be independently established; construct only after the unit is active.
  authority.handoff uses real SQL with controlled kernel adapters. The confined
  default test exercises actual UID1000 PREPARE in Root v5/User v4 but submits no
  transfer. Its default retirement proof is ONLY for that unsubmitted case,
  after native release/local-copy fencing and zero-capacity/empty store checks.
  The temporary Root nsfs getattr grant adds no capability/namespace entry/open.
  Preserve the failed live fixture's original pending/held history in protected
  guest quarantine; infrastructure retirement did not settle it. No normal
  installed receiver, partial-set or full owner recovery is implied.
- The same handoff module now has explicit P0 SEND/RECEIVE/RETIRE with a four-slot
  preserved main-only PID1 store. Read spec/runtime.md's confined-transfer section.
  Only fresh confirmed intents permit notifications; unknown/partial effects
  never allow automatic repeat. Completed retirement never touches a later store.
  Labels route only; complete inventory plus actual SQL-bound object validation
  remain mandatory. The opt-in test_execution_runtime.py --handoff proves actual
  confined Root replacement after the original run is already closed/released.
  Its process-free target retains original unit history; startup selects its role
  from protected catalogue history, not from incoming FDs. The receiver exposes
  no executing/broker handlers and uses a fresh runtime without rewriting original
  receipts. Native retirement follows actual local-copy closure/fencing and store
  absence. A pidfs getattr failure was repaired with the same retained intent/FDs;
  no app/operation identities were regenerated. Temporary PID1 fd use/barrier write
  and Root pidfs metadata permissions add no pipe read, capability or ptrace.
  Active-run/cross-owner recovery, general allocation, partial-set integration,
  early starter/error-channel recovery and production retention still remain.
- catalogue/lifetime.h now has explicit Root-only v6 close-only recovery functions.
  Read spec/runtime.md's Root replacement closure section. Immutable current-owner
  rows require same boot, original successful start/app witness, current handoff
  OBSERVE and still-original User identity. Caller must retain that original
  User's independent process/unit/invocation expectation before failure; current
  unit/domain or broker UUID alone is insufficient. Original APIs remain strict.
  A further Root replacement must OBSERVE and BEGIN again; never rewrite AETHEXE1,
  rearm a run/gate or infer fresh end/holder proof from SQL. Native tests use
  controlled evidence, not an installed active-run coordinator. Normal inputs
  stay v2; User remains v4. Actual live Root recovery, replaced-User coordination
  and early-start recovery still need separate integration/evidence.
- The opt-in --recover-active test now captures immutable AETHUSR1 original-User
  metadata under system/recovery-user.bin before Root transfer. Read spec/runtime.md's
  active-test section. Its first actual attempt 0f04cd9ae92d4500a42177e3e14de76b
  reached current Root OBSERVE while App/User remained active, then failed at a
  combined package/process-image check. Root UID0 without capabilities cannot
  dereference UID1000 proc/exe; the source now keeps current image verification
  in the reidentified User. That source correction is NOT installed in the held
  failed fixture. App/User later expired; no actual recovery closure passed.
  On 4 November QEMU was found dead; offline carrier reboot confirms those four
  volatile handles and /run units/target are gone. Preserve the exact persistent
  fixture/controller/module and old evidence. Its Root v6 still has uncertain dispatch, running lease, observed
  handoff revision 2, no closure and no recovery-owner row. Evidence is under the
  guest build's active-recovery-failure-0f04cd9ae92d4500a42177e3e14de76b.
  Never reconstruct old handles from IDs, retire this fixture or rerun
  from fresh app/operation IDs to obtain a passing test. Original User metadata
  is not a live ProcessFD or proof of its complete drain. Next implement separate
  replaced-User admission/quiescence and separate cross-boot/lost-handle recovery,
  preserving v6's strict original-User boundary. Normal v2 inputs remain closed. Native
  Release/Debug metadata checks ran only as administrative unconfined observers;
  they do not prove live User reidentification or the remaining RPC sequence.
- Existing projects are read-only references unless a scoped change is requested.
  Physim's future NexaUI migration is a requirement, not an order to modify it here.
- Native catalogue/lifetime.h now adds explicit v7 paired-owner close-only APIs
  and AETHRCP1 (232 bytes). Read spec/runtime.md's paired-owner section. Root v6
  and User v4 upgrade independently; create stays v2 and open never migrates.
  Original v3/v6 runtime/User checks and frozen SQL/XML stay unchanged. Both
  owners separately enroll the exact current pair after independent original/all
  prior User drain, current actor/image, holder and future-effect admission.
  Digests never prove those facts. Root can forward historical release to a new
  User without changing the lease. Old uncertain receipts never rearm; another
  actor replacement needs new admission. Run catalogue.owner_recovery and host
  migration/vector tests. No installed pair wire, cross-boot/lost-handle or
  post-retirement Root-replacement recovery is implied. Preserve the old failed
  fixture; do not migrate it to turn native metadata into runtime proof.
- linux/process now has optional observe_main/verify_main/observe_self_main APIs:
  active actual MainPID, no ControlPID/Unit.Job, with the original retained peer
  and repeated UID/label/unit/invocation/cgroup checks. Read spec/runtime.md's
  current-service-main section. Generic/activating registration stays unchanged.
  Root discovery and active broker use require strict main checks; cache tests
  mock this explicitly, while linux.process tests actual same-unit subordinate
  rejection and typed property faults. No numeric MainPID reopen/ExecMainPID
  history/remote self-pin substitution, new capability or policy permission.
  Normal v2 owned inputs were updated and enforcing check/broker-check passed;
  services are stopped/gates closed. Old isolated v6/User v4 fixtures stay intact.
  A service-main snapshot is not current image, future-effect/holder fencing or
  old volume/clone admission. Retained host UUID/PID/command files have no original
  independent disk-object witness; never invent it retroactively.
- catalogue/interruption.h now adds explicit v8 cross-boot maintenance after v7:
  immutable AETHINT1 bindings (248 bytes) and four distinct 352-byte receipts.
  Read spec/runtime.md's cross-boot section; run catalogue.interruptions and
  contract/migration/vector tests. Normal create stays v2; open never migrates.
  SQL v1-v7/XML and original/same-boot checks stay unchanged. Original successful
  start/app evidence is mandatory; early/unfinished starts are separate work.
  Further owner/boot replacement requires fresh independently admitted bindings
  at BOTH owners; old stages/results never change or rearm effects. Hashes and
  boot IDs are not continuity/clone/current-actor/holder/future-effect proof.
  Tests use controlled admission with real SQLite; no installed wire/coordinator
  or real retained-fixture settlement follows. Keep its v6/User v4 unchanged.
- linux/boot.h now provides read-only AETHBOT1 current-kernel/proc/self PID/cgroup/
  time namespace observation (48 bytes). Read spec/runtime.md's boot section; run
  linux.boot and the explicitly privileged linux.boot_namespaces carrier checks.
  Independent trusted host scope remains mandatory; self IDs never prove initial
  scope. No PID1 namespace reads/numeric-PID opens/ptrace or new policy permission.
  Different boot is not chronology, machine/volume/clone provenance, original end,
  holder drain or release authority. The build-local administrative inspect-boot
  checks original protected metadata/store without beginning a runtime; it does
  not change normal load checks or settle the retained v6 fixture. Cross-boot
  interruption/closure, current actors and future-effect fencing remain work.
  Inspection requires independent quiescence/fencing and existing regular lock/DB
  with no journal/WAL/SHM; the ordinary SQLite connection is read-write and must
  not perform incidental hot-journal recovery. The explicit --inspection Python
  runner tests the preserved fixture plus private shadow refusals, never dispatch.
- Keep a small number of coherent modules/documents. Explain every major directory,
  dependency, resident service and generated artifact.
- Design ownership and complete app removal from the beginning; read
  `brains/aetheri/knowledge/app-lifecycle.md`. Include plugins, state, credentials,
  registrations, services and multi-user/shared dependencies.
- Avoid unnecessary resident processes and polling. Measure wakeups, memory,
  responsiveness, startup and disk use. Process/file count alone is insufficient.
- VM RAM may use up to 16 GB. F: is available for Aetheri, but use F:/Aetheri only.
  Existing Physim/NexaCloud directories, ongoing migrations and processes are
  protected: do not move, delete, clean, stop or otherwise interfere with them.
  Do not format/repartition the volume or treat it as disposable workspace.
  Do not expand shared WSL storage on D: casually.
- UI remains a user decision. Later prepare multiple alternatives with keyboard,
  focus, scaling, three-monitor, accessibility and error-state behavior.
- User now prefers working UI foundations such as SDL3 instead of waiting for
  NexaUI completion. Read brains/aetheri/knowledge/nexaui.md. Review exact SDL3/
  text/widget dependencies before adoption; the first exact development profile
  and runtime limits are now in spec/settings.md.
  Keep app models/commands/persistence toolkit-independent and wlroots compositor
  separate. NexaUI can be adopted per app when its required scope is ready. The
  authorized local NEXT_AETHERI_UI.md note is a handoff, not permission to alter
  NexaUI implementation, other consumer pins or send another chat a message.
- Early experiment environments are now required: complete Aetheri desktop first,
  persistent changes until explicit reset, and individual personal-file/network/
  device permission (4 November 2026). Read knowledge/experiment-environments.md
  under brains/aetheri. Plan hardware-virtualized isolation for guest root/kernel
  and later AI work, resource limits and complete VM-state ownership/removal.
  The existing Windows carrier is not this feature; no absolute host-crash or
  escape guarantee, TCG security fallback or implicit host-management exposure.
- No invented commands, screenshots, measurements or hardware acceptance.
- The optional private C17 compositor now exists; read spec/graphics.md and its
  separate graphics-inputs.json. Native input and explicit nested output are tested;
  its wlroots pointer-enter patch and fresh build helper are pinned there. Default
  builds stay graphics-OFF. These tests are not physical/trusted-session or app-
  lifecycle acceptance. The input-FD driver is a separate test executable. Keep its
  private socket/captures and unprivileged test children separate from the retained
  old execution intent. Exact wlroots/Wayland dependency changes require review.
- Optional AETHERI_SETTINGS now builds the native development view in src/settings;
  read spec/settings.md and spec/ui-inputs.json. Keep its model toolkit-independent.
  The selected SDL Wayland profile uses OpenGL ES; the initial no-renderer profile
  could not create a framebuffer. Private builds pin SDL/text sources and one
  optional-data-device null guard. Do not advertise clipboard merely to avoid that
  client bug. The VM uses explicitly selected llvmpipe; hardware/AT-SPI/normal app
  admission and settings backends remain work. Supply an explicit owned 0700
  development shader-cache directory; it is not a StorageRef or disk quota.
  Preserve unpatched close-time core dumps and the same-binary negative control.
  Source/headers and guest graphics RPMs are reviewed separately; do not install
  broad GL development packages, change shared host/WSL settings or infer physical
  GPU acceptance. SDK/Mesa Release libraries are not ASan-instrumented by the
  current app Debug check. The default Settings option stays OFF.
- The private compositor now exposes native clipboard selection. Read the
  clipboard section of spec/graphics.md and the additional pinned wlroots patch.
  Use the actual request client/current seat focus and current focus serial floor;
  no UID/source-name inference. Retained offers require current focus at receive.
  Replacement keyboards retain the current window and start fresh focus. The
  private getter/reject symbols require the new ABI before socket publication;
  they are not library provenance. Reject unstarted drags without leaking a
  source-less drag or destroying the active clipboard source. Clipboard uses no
  history/payload cache/host bridge; it is not app admission, consent, FD revocation,
  trusted lock or completed DnD. Preserve the old-ABI/receive-guard negative controls.
- The user confirmed the current workstation is the main-PC target and requires
  future AM5 portability. Read brains/aetheri/knowledge/testumgebung.md.
  Keep the x86-64/UEFI baseline vendor-neutral; no unconditional -march=native.
- Review selected files before public publication. Keep credentials, personal
  machine paths, local state, downloaded/build trees and VM images out of Git.
- No laptop installation disk is selected. Verify exact device identity and
  recovery steps before any destructive hardware test.
- The private desktop connection now exists in compositor/desktop.c; read
  spec/desktop.md. Only the independently inherited original stream connection
  can discover/bind layer-shell/control; no UID/title/app-ID or numeric-PID proof.
  Keep view state, compositor focus policy and fixed-app supervisor launches
  separate. Neither private global exists without --shell-fd. The current native
  protocol consumer stays diagnostic; a separate SDL development consumer and
  fixed two-app owner channel now exist in spec/desktop-ui.md. No production
  session/lock/app authority is implemented. Unexpected shell loss
  fails this development session; retire acknowledges only orderly departure,
  never lifecycle/lease settlement. Preserve old intents and normal empty caps.
- AETHERI_DESKTOP is opt-in and requires the compositor. Read spec/desktop-ui.md.
  Keep models/commands toolkit-independent, SDL custom roles/high-density scaling
  in the adapter and fixed app paths/process handles in the development owner.
  Initial configure precedes EGL allocation; retain strict logical geometry and
  correct premultiplied clear colors. The selected carrier needs one bounded
  follow-up repaint after resize. Idle is event-driven. Both channel directions
  reject ancillary FDs/truncation; no unknown request is replayed. The private
  three-byte v3 channel uses L3S/L3T/L3F, A3/E3/X3 plus S/T/F, and Q30. App/window
  selection is tagged; keep per-app pending state and fixed owner-selected paths.
  User Q30 fences new launches and waits for Files (including its document) ->
  Terminal -> Settings -> desktop ->
  compositor, including actual process AND output-pipe end before the next role.
  Preserve this order on exception too; post-spawn pipe errors are not no-effect
  replies. Close unused parent client copies when stopping before desktop startup;
  a late compositor READY cannot create the first UI after the stop fence. Closed
  format1 records without launch_protocol stay historical v1;
  historical protocol2 remains readable without rebinding; new desktop records
  bind protocol3, seven binaries and Mono font/license hashes. The session-local
  documents root is independently prepared. Search may still activate existing
  windows during closing, but cannot launch new ones.
  desktop.owner runs the real loop with test-only host adapters and independent
  pidfds; its /proc writer retention and force cleanup are never app admission.
  Journal stdout can use a PAM session scope: bind current boot/original process
  and captured stream for evidence, never adopt historical PID/log identity. A3 is
  only an original child observation, never app READY/admission or lease release.
  App-ID grouping grants no privilege. SDL/SDK/Mesa instrumentation limits and
  unresolved native a11y/text/physical/session gates remain explicit.
- Window desired maximize/fullscreen modes are compositor-owned, separate from
  client requested/committed state. Super+Up/Down requires actual app focus and
  refuses shell focus, pointer interactions and fullscreen; consume both arrows.
  Initial v4+ bounds are advisory and exclude dock/margins, not client clipping.
  Reconfigure against the current selected output, never only the home index.
  Read spec/desktop-ui.md's later work-area evidence; physical/general off-screen
  recovery and fullscreen dock policy remain open.
- AETHERI_SHELL is opt-in; read spec/shell.md before extending its experimental
  profile0. Parsed programs own arena strings and cannot be copied without pointer
  rebasing. Keep scalar expansion nonrecursive and reject whole invalid pipelines
  before effects. Retain the all-forks start barrier, direct original-child waits,
  stdio-only child FD export and explicit execve (no execvp shell fallback).
  Parent builtins require a singleton without redirection. This is not a graphical
  terminal, app admission, descendant drain or a sandbox; -i is scoped below.
  Final language/product choices and normal provisioning remain open.
- Shell -i now supplies private canonical N_TTY/group/SID checks and ephemeral
  jobs through jobs.h. Retain original direct children and monotonic IDs, terminal
  handoff before barrier release, default child actions and mask unblocking before
  any blocking redirection. Continue requests are not running observations; a new
  STOP may coalesce with CONT. Refuse foreign foreground takeover. Exit/EOF refuses
  open jobs; HUP/TERM signals still-waitable original direct children and waits
  without force-kill. Do not infer descendant/app/lease drain. Final persistence/
  raw editing and normal terminal app integration remain open. Reject saturated raw canonical records
  before CR/delimiter trimming; no lossless kernel-editor claim. PTY test cleanup
  enrolls original pidfds while the private leader is unreaped; never rediscover
  a session from a stale numeric SID. Read spec/shell.md's interactive evidence.

- AETHERI_TERMINAL supplies a separate graphical PTY owner; read spec/terminal.md.
  Native selection retains every row's content-incarnation token through history
  movement. Edits/erase/wrap changes, eviction, resize/reset/screen replacement
  cannot retarget it. Tokens are process-local observations, never security or
  durable identity. Copy sizing precedes writes; preserve wide/combining UTF-8,
  real soft-wrap joins and atomic undersized-buffer failure. Ctrl+Shift+arrows/A
  and selection Escape never enter the PTY. Accepted input clears selection;
  focus loss cancels drag. UI handles stay outside the cell/selection model.
  UTF-8 export is not native clipboard transport. The selected SDL Wayland read
  path is synchronous without a complete-transfer byte limit; do not call its
  main-thread-only API from the readiness worker or add unbounded UI paste.
  The bounded clipboard adapter now borrows SDL's actual display/surface/queue;
  keep one owner reader/dispatcher and the existing readiness worker. Preserve
  fixed source/offer/send/memory budgets, nonreused epoll cookies, total deadlines,
  immutable in-flight exports and the one-unacknowledged-receive sync barrier.
  Old offers are dropped on native focus loss. Run terminal.clipboard: real peers,
  stalled/trickled IO, blocked exports/replacement and close must stay bounded.
  Toolkit-free paste validates all <=4096 bytes before queueing; multiline/tab
  review starts on Cancel and requires fresh input after review-frame submission.
  Recheck original focus/shell/foreground/screen state; no retry after acceptance.
  This remains TERM=dumb, not bracketed paste/full VT/IME or app admission.
  Add no clipboard daemon/host bridge implicitly. Closing FDs, EOF and protocol
  sync never prove remote revocation, process drain or lease settlement.
  Native tests retain exact pixel/byte and stale-row mutation evidence; virtual
  input and software rendering do not establish hardware/a11y acceptance.
- terminal/composition now retains <=1024 bytes of preedit separately from PTY
  cells/history. Scalar offsets are not bytes/cells/graphemes. Preserve complete
  validation, bounded horizontal viewing and current candidate coordinates.
  Hold editing/confirmation keys while composing; Escape/blur/context/end/close
  clear the draft. Commit is validated single-line input with no speculative
  PTY bytes or replay. Stop native text input during blur/end/close/paste review.
  SDL's pinned Wayland ClearComposition can succeed as a no-op; retain actual
  Stop/Start transitions and queued-event fencing, never infer provider/serial
  admission from that local timestamp. Run terminal.composition and terminal.ime
  plus affected terminal/desktop regressions. The controlled native v3 producer
  is linked only into the test server and explicitly enabled by inherited-socket
  I on; it is not installed compositor/provider IME integration. Keep normal
  production globals/services/policy unchanged until that separate route is reviewed.

- The own NexaUI Terminal now has original-session tabs/splits. Read
  spec/terminal.md#original-session-tabs-and-split-panes and the multiplex journal.
  workspace is a bounded value tree, never a PTY/model owner. Keep live sessions
  in stable slots, one worker and a rotating shared 16 KiB turn. Prepare spawner
  sources/environment before threads; pidfd_spawn's unprivileged bootstrap is
  distinct from the privileged execution starter. A post-spawn error with a pin
  retains original child/stream duties; never retry or infer descendant drain.
  Named pane IDs must remain disjoint across equal-token models, hide/resize and
  slot reuse. Focus transfer cancels old paste/preedit and fences queued commits.
  Owner v2 adds exact bootstrap/source binding without rewriting v1/closed history;
  Files' document editor receives no multiplexer helper. Run terminal.workspace,
  terminal.spawn, terminal.panes_view, terminal.multiplex and affected owner/input
  regressions. Persistence, normal admission and complete assistive use remain work.
- catalogue/recovery.h now provides one bounded read-only owner metadata snapshot
  on the retained SQLite connection. Read spec/runtime.md's recovery-inventory
  section and run catalogue.recovery_inventory. It never authorizes replay,
  holder drain, gate reopening or old-volume/process adoption; empty counts are
  not service readiness. Preserve historical uncertain closure rows and separate
  terminal interruption semantics. Source startup scans before protocol
  publication, but the protected installed bootstrap is not updated by that build.
- linux/image.h now has a self-only image matcher supporting capability-empty
  UID/GID0. Keep remote peer UID0 rejection; never turn self pins into remote
  authority credentials. Normal P0 selects the independently protected role ELF
  before catalogue access and at context/User-delegation use, latching failure.
  The separate offline setup-recovery image selection remains explicit. Read
  spec/runtime.md's current-authority-executable section; run linux.image_self
  and affected execution/peer/process/broker tests. The explicit actual-domain
  shadow test has no catalogue and cannot adopt normal state. The normal v2
  bootstrap is now updated/checked enforcing, including the earlier startup
  inventory. Policy and non-runtime owner tables stay unchanged. This does not
  prove loaded-dependency/remote-owner/continuity/future-effect/holder admission
  or settle the old quarantine. Preserve its lost-handle boundary.

- authority/system_connection.h now binds normal User to the protected current
  Root unit/epoch using actual correlated successful replies, an original
  ProcessFD and repeated active MainPID/unit/invocation/cgroup/boot checks. Read
  spec/runtime.md's reciprocal-current-Root section; run
  authority.system_connection and the explicit controller connection-check.
  Management event incarnation is not Root runtime. Private normal User Root
  RPCs use the same unique destination. Bounded pending-message dispatch retains
  name-loss evidence through same-name reacquisition; nested use/close is EBUSY.
  Live drift quarantines, actual departure drops identity only; never replay a
  sent call or infer old-run adoption/drain/lease release. The normal v2 bootstrap
  now includes the protected selected-Root slot and minimal reciprocal proc
  metadata policy, with explicit User ptrace/Root-ELF denials. No SQL/XML/public
  capability change. Interrupted controller probe/update needs manual review.
  Continuity, prior-owner/future-effect/holder recovery remain separate work;
  preserve the old lost-handle quarantine and all unsettled owner journals.

- catalogue/effect.h now supplies a process-owned, one-way local execution latch
  shared by coordinators on the retained SYSTEM store. Read spec/runtime.md's
  local-execution-source-fence section; run authority.execution_fence and
  authority.execution_fence_wire plus affected execution/catalogue tests. Fresh
  stages and post-commit adapter/transport permits reject when fenced; historical
  acknowledgments never regain effect_required. Balance all entered brackets;
  close/runtime replacement is EBUSY while active. Actual Root shutdown fences
  before event-loop exit. This is memory-local, not a durable/restart/volume or
  global holder fence. Entered calls/sends may still finish or return unknown;
  active==0 never proves external drain or lease release. Other effect families,
  supervisor/old-owner/export/delegated sources require separate coordination.
  No SQL/XML/policy/public capability change. Keep the old lost-handle quarantine
  untouched; no normal executing launch or complete recovery follows.

- authority/handoff now shares that local source fence for raw observer export
  and every fresh four-slot FDSTORE notification. Root-custodian permits match
  current epoch/boot/runtime without inventing a historical User delegation.
  Hold brackets across source effects; recheck after PREPARE and before each
  send. An entered last send/barrier may still finish; partial/unknown sets stay
  pending and are never filled or replayed. Receive and independently authorized
  retirement use cleanup brackets allowed after fencing, with all original
  exact-object/terminal/holder prerequisites intact. Read spec/runtime.md's
  original-observer-export section; run authority.handoff_fence and affected
  catalogue/handoff/execution-fence regressions. These new component scenarios
  use real SQL/local FDs with controlled lifetime/PID1 adapters, not joined live
  app/supervisor acceptance. No installed public launch/recovery or old-volume
  provenance follows; preserve the existing quarantine and frozen contracts.

- The same retained SYSTEM latch now rejects fresh install reservation-gate
  opening, new reservation intent/lease/dispatch and first no-exec setup intent.
  Check after admission reads and before mutation. Preserve exact historical
  acknowledgments and existing independently admitted observation/withdrawal;
  none authorizes a new unit/start/helper reply. Read spec/runtime.md's fresh-
  reservation-and-no-exec-intent section; run catalogue.admission_fence and
  affected reservation/launch/source-fence tests. The new twelve cases use real
  native metadata with synthetic caller evidence, not live supervisor/drain
  admission. Keep all original cleanup proofs and frozen SQL/XML; the latch
  remains local and does not resolve old-owner/volume/holder continuity.

- authority/setup methods now bracket the current owner for the whole method,
  reject reentrant use/clear, and source-permit Prepare/Register/Prepared replies
  after native work and again before send. Entered uncertain sends are handled;
  never generate a replacement error reply or drop original pins/journals.
  Existing admitted observations and proven no-exec withdrawal remain cleanup
  while fenced. Read spec/runtime.md's no-exec-setup-reply section; run
  authority.setup_fence plus setup-supervisor/launch/admission/transport regressions.
  Its private-bus/process pins are real, actor labels/PID1 context controlled;
  no live MAC/view/drain acceptance follows. Clear now returns EBUSY while active.
  No SQL/XML/policy/public capability changes; preserve old quarantine and all
  remaining supervisor/volume/old-owner/holder obligations.

- linux/start and authority/start now submit/observe one original asynchronous
  PID1 StartUnit/fail after the coordinator's fresh REQUEST commit. The compiled
  caller still owes immutable unit/dependency-graph/exclusive allocation admission,
  its own RefUnit/Subscribe on a dedicated host bus, and current app/data context.
  Authenticate original reply and JobRemoved; never adopt matching-unit jobs
  after a lost reply. Keep the catalogue cleanup hold for pending/quarantined
  work; ordinary close is EBUSY, explicit abandon is local only. Known job result
  is neither target exec/readiness nor process/unit/holder drain or lease release.
  Terminal watches stop and historical results survive later Manager replacement.
  Read spec/runtime.md's original-supervisor section; run authority.supervisor_start
  and explicit linux.start_pid1. The first controls PID1 role on a real private
  bus; the second uses actual PID1 with an owned process-free target, not a new
  executing app fixture. No installed source/profile/capability or SQL/XML change.
  Keep the old quarantine; normal provisioning/restart/volume/holder recovery remains.

- linux/unit now owns an exclusive moved bus, original PID1 pin, witnessed source/
  drop-in FDs and one-shot Subscribe/RefUnit/UnrefUnit custody. Read runtime.md's
  protected-unit section. Its bounded P0 graph has one executable starter and
  non-executing infrastructure; only literal perpetual root/system slices can
  omit files. Current file observations are not durable volume/allocation trust.
  Caller still supplies independent original allocation/global mutation admission
  and journals custody intent. Drive its borrowed bus only; never mix independent
  Ref/Subscribe/Start owners or the older job-only adapter on that connection.
  Fresh config checks fence on source drift; original end/holder admission is
  mandatory before retirement. Unknown acknowledgements never retry. Ordinary
  close rejects unsettled custody; abandon closes without flushing and cannot
  cancel work, prove drain or free a lease. Run explicit linux.unit_custody and
  linux.start_pid1. Actual PID1/admin true starts and controlled lost replies do
  not establish normal aetheri_system_t provisioning, app target readiness or
  executing drain/recovery. Preserve all old fixtures/quarantine and installed
  inputs; live registry/coordinator/end integration is still pending.

- Root v10 now journals original unit connection origins and ordered per-node
  Subscribe/Ref/Start/Unref/Unsubscribe intent/acknowledgement history. Read
  spec/unit-profiles.md#root-v10-connection-custody-journal. The Root adapter
  requires v10 and compiled hooks; isolated Linux owners can omit them only for
  their explicit tests. Only fresh confirmed intent permits an RPC; unknown
  replies/acknowledgements quarantine use. Start confirmation is an authenticated
  job reply, never target exec or drain. AETHUBS1 is connection metadata, not
  issuer process/supervisor/image or volume proof. Full sequences stay visible
  and immutable; there is no custody retirement/recovery transaction yet.
  Explicit upgrade_unit_custody migrates only quiesced Root9->10; normal create/
  installed owners stayv2, User9/10 unsupported, published v1-v9 SQL/wire frozen.
  Run catalogue.unit_custody and explicit authority.unit_custody. Preserve the
  old executing quarantine and all original histories; no replay/adoption/lease
  release follows from this journal or local connection closure.
  First-quarantine notification also fences fresh Root execution effects; this
  does not settle metadata or prohibit independently admitted historical cleanup.
  Once published, keep v10 SQL and unit-origin-v1 unchanged; future schema changes
  need a new explicit version, never an edit to a published development contract.

- Root unit construction now requires explicit Root v11 original issuer
  enrollment before REQUEST. Read spec/unit-profiles.md#root-v11-original-issuer-identity.
  linux/issuer retains an actual bus-authenticated own ProcessFD and checks the
  independently selected main service/label, invocation/cgroup, pidfs identity
  and original self ELF. Same-unit children, wrong image/label/role and inherited
  connections reject. Local self-image pinning never replaces the required bus
  ProcessFD; remote UID0 image admission remains forbidden. AETHUIS1 and getters
  are historical metadata, never current proof, recovered handles or new effects.
  Fresh effects and independently admitted original cleanup repeat issuer checks;
  close only drops local handles. Explicit quiesced upgrade_unit_issuers adds no
  old evidence or gate/runtime/revision changes. Normal create/installed owners
  stay v2; open validates exact Root2–11/User2–4,7–8 without migrating. Preserve
  published v1-v10 SQL/XML and unit-issuer-v1 once published. Run core.unit_issuer,
  catalogue.unit_issuers, tools/test_unit_issuer.py and explicit enforcing-carrier
  authority.unit_custody / authority.issuer_barriers. Their UID0 administrative
  drivers are not production confined Root provisioning. All owner histories and
  reserved leases remain; operator cleanup of known terminal test configurations
  never settles native obligations. No bus-loss/owner/volume/clone recovery,
  external holder settlement or durable issuer/custody retirement is implied.
  Preserve the old executing quarantine, normal inputs and SDK/UI defaults.

- The NexaUI consumer pin now selects 0.1.23/97a4264 through
  spec/nexaui-inputs.json. Read the 2026-11-07 NexaUI upgrade journal and STATE:
  private Release/ASan app evidence is separate from shared-source/build
  promotion. Preserve old 0.1.18/CLOSED inputs and normal P0/quarantine. Never
  weaken owner/artifact checks for test permissions, or remove foreign /tmp
  data to resolve quota failures. The delivered chat handoff remains closed.

- The normal development source and session-release/session-debug builds now use
  the reviewed NexaUI 0.1.23 inputs. Read the 2026-11-07 development-promotion record.
  New Root starts persist tmpfs-v1 and verify the original unit option; supervisor
  preflights a separate 1 GiB temporary filesystem. Preserve older records/history.
  A source .aetheri-promotion.pending fences new starts during a partial refresh;
  never clear unexplained pending state. Native owner fixtures can use an explicit
  canonical UID1000/mode0700 artifact root, with binary bindings unchanged. Export
  private tmpfs evidence before retiring it. Carrier root capacity is limited;
  do not delete foreign caches/data or protected lifecycle fixtures to make room.

- A separate 8 GiB developer qcow2 and original-object registration are prepared
  on the host. Read spec/development-storage.md and the 2026-11-07 journal/STATE.
  Future normal cold boots attach it; installer boots refuse it. Host QEMU probes
  and enforcing guest loop tests pass, but actual fixed-disk guest enrollment is
  pending. A live foreign NexaUI helper defers reboot; never terminate that work.
  Host create/guest format have distinct pending records and no automatic replay.
  initialize-guest requires the actual original QMP block/image/serial; mount-guest
  never formats. Keep old build paths, root reserve, policies and protected state.

- editor/direct.h now supplies the separate graphical Editor's toolkit-free
  non-modal input controller over the original document kernel. Read
  spec/graphical-editor.md; run editor.direct with existing editor regressions.
  Input tokens are process-local correlations, not native clipboard/grant proof.
  Host focus/tab/file changes must cancel original input without losing selection.
  The native tab/save/close coordinator and opt-in window below preserve the
  separate modal Terminal editor and normal launch paths.

- editor/workspace.h now owns bounded graphical-editor documents/direct inputs/
  local file adapters and original close/destination correlations. Read the
  current graphical-editor contract and 2026-11-07 workspace journal; run
  editor.workspace and affected editor regressions. First saves are create-only;
  UNKNOWN retains the original adapter/name and blocks save/discard completion,
  even for clean text. Normal UI shutdown must resolve tabs before destruction.
  The opt-in native window below uses that backend; complete Save As/export/
  recovery, responsive I/O and normal grants remain unfinished.

- The opt-in aetheri-text-editor now has a native NexaUI0.1.23 window; read
  spec/graphical-editor.md and the 2026-11-07 editor-window journal/STATE.
  Selected private Wayland workflows and nine editor groups pass Release/ASan.
  Each original native event passes through the direct controller/focus frame;
  never route the unexamined remainder directly to the toolkit. The local picker
  passes the original held file witness for both new reads and duplicate reuse.
  Rendering may query selection while original close review blocks input handles.
  Save I/O now has the explicit original-worker integration below; loading and
  folder enumeration still need responsive original operation ownership.
  Clipboard/focus cancellation now has the separate native integration below.
  Text snapshots are read-only, not a
  complete accessible editor. Search, IME, native chrome/reflow/design, Save As/
  recovery and normal app admission remain work. Sticky SDK native close is
  latched once; repeated native-close/management acceptance is unproven (W01).
  tests/compositor_input_driver.c's Z command commits test-only integer output
  density, absent from installed compositor input; no fractional-scale claim.
  Preserve normal sources/builds/authority/quarantine, protected VM and foreign
  work. Use private /tmp backing for native tests; never remove foreign quota data
  or treat tmpfs as the known SDK sparse-mmap failure fix. Goal remains incomplete.

- The graphical Editor now uses the pinned public Wayland clipboard asynchronous
  begin/take/cancel/release API. Read the updated graphical-editor contract and
  2026-11-07 editor-clipboard journal. Original document/direct token/canvas/focus
  admission precedes take, after all queued native events and focus frames; a wake
  is advisory. Retire only the original app-owned request, never a field's other
  request. Empty results do not delete selection; native EOF does not prove full
  source intent. Use the SDK wiping release with the exact returned length.
  Fourteen independent native workflows pass Release/ASan with original pipe
  cancellation, real file bytes and overall timeout evidence. There is no new
  thread/service for clipboard, installed permission or normal desktop acceptance.
  Save work below adds a bounded on-demand worker; loading/scanning, final design/
  accessibility and the full OS goal remain pending.

- The graphical Editor now prepares one original save job through workspace.h.
  Read spec/graphical-editor.md and the 2026-11-07 editor-save-work journal/STATE.
  Document snapshots freeze the captured saved-history boundary; actual OK may
  acknowledge that state after newer edits, preserving dirty state/newer typing.
  Ack retries never restore later clean markers. Worker owns only its private
  document/file adapter; never read the live workspace/document from it. First
  saves borrow fixed incumbent adapters for checks; their close is blocked.
  The trusted host MUST join the original worker before finish/free; done/wake
  alone is not termination or save success. No started-write cancellation/replay.
  Finish binds only the retained original tab, including UNKNOWN even for clean
  text. Destruction refuses active work. A later save reply never closes a newer
  or cancelled review. All ten native editor groups and actual window/clipboard/
  controlled save workflows pass Release/ASan. test_editor_save_pause contains
  the separate test-only fsync pause/fault, not the normal app. Its UNKNOWN actor
  is pidfd-administratively retired only after join, not production close/recovery.
  TSan compiler probe fails because its runtime is absent; do not claim TSan or
  change global packages/policy. Loading/scanning and full OS acceptance remain
  work; preserve normal inputs, protected history and foreign planning/design.

- tools/build_boot_probe.py now builds a separate ordinary-user diskless
  initramfs from spec/boot-probe-inputs.json. Read spec/boot-probe.md and the
  2026-11-07 boot journal/STATE. systemd remains PID1; the native probe has no
  reboot or installation effect. Its generated image-only unit requests immediate
  poweroff, never suitable for the normal host/user shutdown path. Preserve vendor
  licence files; these development pins are not a production security/base or
  source-distribution approval. tools/test_boot_image_contract.py checks actual
  archive metadata and no-overwrite; tools/test_boot_probe.py uses separate
  diskless/no-network/no-QMP q35/TCG with original extracted tool hashes and fresh
  private NVRAM. Retain original handles/attempt logs; exit0 alone is not PASS.
  Two positive cold boots and a real wrong-nonce rejection pass. Do not call this
  a USB/disk image, Secure Boot, production policy, desktop or hardware acceptance.
  Next test normal removable-media firmware boot, then root/session integration.
  Never attach/format host or carrier disks to make that proof easier.

- tools/build_uefi_probe.py now makes the standalone GPT/FAT32 probe with a
  pinned systemd259.10 stub/ukify and build-only pefile. Read spec/uefi-probe.md
  and the 2026-11-07 UEFI journal/STATE. Verify exact RPM signatures/payloads and
  preserve licences; do not install these private extracted tools globally.
  Formatting accepts only the newly created regular boot.raw under a new output,
  never a device. UKIs reproduce byte for byte; FAT directory timestamps do not.
  tools/uefi_probe_image.py independently checks GPT/FAT/PE; run the seven media
  contracts and five older boot-image contracts. tools/test_uefi_probe.py boots
  only the read-only USB artifact plus fresh NVRAM, no direct kernel/initrd/append,
  system/data disks, network, QMP or shared directory. The shared original-child
  observer now requires exact native result, service outcome and kernel poweroff.
  Two final positive cold boots and an actual wrong embedded nonce rejection pass.
  Valid RPM signatures are not an EFI signature; no Secure Boot/production policy
  or hardware acceptance follows. This is still the immediate-poweroff diagnostic
  image, never a normal user desktop. Next independently assemble a read-only
  system root and integrate the existing session/desktop, without adopting old
  quarantined runtime state or treating image GUIDs as durable volume proof.

- tools/build_root_probe.py now adds a readonly ext4 root in the combined 512MiB
  GPT file. Read spec/root-probe.md and the 2026-11-07 root journal/STATE. It runs
  only as an ordinary Linux builder, formats only new regular files and never
  mounts or installs them. Capture the mke2fs configuration/tool hashes; require
  all 72 selected inode ownership reports and e2fsck acceptance. The C17 root
  mode confirms actual ext4/ST_RDONLY, mounted device PARTUUID and create EROFS;
  the normal RAM mode must preserve its original ELF bytes. Run five root tests
  plus affected old boot/media regressions. Kernel builtin virtio/ext4 performs
  root mounting with no /init; do not claim initrd userspace handover, laptop USB
  root, LUKS, production policy, writable data or desktop acceptance. The new image
  has no production MAC policy; carrier Enforcing evidence is separate. Immediate
  poweroff units stay image-only. Root GUIDs/UUIDs/nonces are fixture correlations,
  never clone authentication or catalogue/data admission. Preserve normal sources,
  old quarantine/CLOSED records, unattached developer disk and foreign work.
  Next writable runtime/data and existing desktop/session integration; full goal
  remains active and incomplete.

- tools/build_data_probe.py now adds a separate persistent-document image profile;
  read spec/data-probe.md and the 2026-11-08 journal/STATE. Native UID1000 writes
  survive two cold boots with a readonly root; root syncs and unmounts the data
  before diagnostic poweroff. Six controller contracts and native record
  Release/ASan/UBSan adversaries pass. UUID/20-byte virtio serial and a retained
  host file handle are bounded fixture observations, never volume/clone/account
  admission. The original data source is immutable; every sequence owns a NEW
  copy and NVRAM. Pending/unknown attempts preserve files and forbid replay.
  Normal app DataEpochs, catalogues and quarantined attempts are unrelated. The
  image has no production MAC policy/login/hardware or power-loss acceptance.
  Minimal udev rules lack full module/device integration; fixed test-account
  paths are not the final account/session design. Independent read-only fsck/
  debugfs verifies actual bytes and ownership after unmount. Next integrate the
  existing desktop/session in this base; full OS/apps/installer/hardware and
  experiment goals remain active. Preserve the original carrier and foreign work.

- tools/build_desktop_probe.py now assembles the existing desktop/apps into a
  separate768MiB system/512MiB readonly-root profile plus256MiB data. Read
  spec/desktop-image.md and the2026-11-08 desktop journal/STATE. Fixed development
  PAM autostart is not production login/MAC/app admission. Keep GUI UID1000,
  NoNewPrivileges/cap-zero and real active-seat/input checks. The bounded root
  VT oneshot uses only SYS_TTY_CONFIG; broker PrivateDevices stays enabled.
  Runtime inventory includes locale/input/module/licence data, not libraries
  alone. Require bounded pinned copy validation and exact fixed image geometry.
  Native owner uses pidfds and a pre-exec barrier; keep unknown attempts/data.
  Accepted two-cold-boot sequences prove real app input, persistent documents
  and normal unmount/poweroff. Separate read-only fsck/debugfs confirms exact
  bytes/UID/mode/link count without changing input. Settings uses SETTLED and
  original-owner exit acknowledgement, not invented READY/CLOSED strings.
  Normal lifecycle/catalogue capabilities and protected carrier stay unchanged.
  The virtio/bochs image is not laptop/USB acceptance. Next early USB-root/device
  selection, module/firmware and single-medium data integration. User explicitly
  authorizes flashing the identified Intenso Speed Line G: when ready; reidentify
  the physical device before writing but do not ask again or wait for their later
  hardware test. Internal disks are not authorized. No USB write yet. Full goal
  remains active; preserve foreign work and the closed NexaUI handoff.

- tools/build_usb_root_probe.py and src/boot/initrd.c now prove early USB root
  and standard systemd initrd handover. Read spec/usb-root-probe.md, STATE and
  the2026-11-08 USB-root journal. Two actual cold boots and wrong-transport/UUID
  pre-mount negatives pass. The fixed512MiB diagnostic is not the physical stick
  desktop. Keep original root bytes and independent input-manifest pins; selected
  libkmod is a udev dlopen dependency, not covered by kmod CLI ldd. Import block
  properties in a rule before matching them. OnSuccess requires an inactive
  oneshot; PID1 OnSuccessJobMode=isolate stops udev before checked SwitchRoot.
  Current boot/devnums/sysfs/diskseq facts are not durable volume/clone trust.
  The prior graphical image lacks libbpf and explicitly disables cgroup BPF;
  its configured DevicePolicy is not accepted as enforced. Add reviewed runtime
  and actual allowed/denied device-open evidence before USB desktop acceptance.
  Next join graphical root/data on one removable medium with independently
  checked common parent/actual stick geometry and laptop DRM/input/firmware.
  VM-test that profile before the already authorized flash. Reidentify Intenso G:
  without asking again; continue work after flashing, never write internal disks.
  Preserve all failed attempts, original carrier/normal inputs/quarantine/CLOSED
  and foreign files. Full goal remains active; NexaUI handoff stays closed.

- tools/device_filter_image.py now adds signed Fedora44 libbpf2:1.6.3-2 and its
  pinned matching BSD licence as data to new graphical images. Read
  spec/device-filter.md and the2026-11-08 device-filter journal/STATE. Carrier
  library stays unchanged; known object-parser reports are not claimed cleared.
  This is trusted PID1-generated instructions, not caller ELF/BPF loading.
  src/boot/device_filter.c checks baseline read-only root block open versus EPERM
  under DevicePolicy=closed, with actual null access in both cap-zero/NNP probes.
  Both oneshots gate VT/desktop startup. The root PARTUUID alias needs a separate
  blkid-import then match rule; old data rules only tag. Two cold boots also pass
  GUI/document/normal unmount-poweroff. This proves fixed probe policy, not all
  app/MAC/device policy or retroactive old-image acceptance. Keep PrivateDevices
  and original runtime pins. Controller requires exact ordered same-boot results
  and rejects mixed/failed/disabled-backend output. Independent ordinary-user
  fsck/debugfs verifies14 document bytes, ownership/mode/link and unchanged hash.
  Preserve failed/unknown attempts, original carrier and foreign work. Next
  single-medium USB root/data, independently checked parent/physical geometry and
  laptop DRM/input/firmware; repeat relevant tests before the authorized G: flash.
  Continue development afterward without waiting for hardware tests. No flash yet.

- The single-medium USB desktop now passes actual persistent two-boot and
  wrong-parent-overlay tests; read current STATE instead of the older next-step
  paragraphs above. The separate laptop integration adds automatic session GPU
  selection and same-kernel Intel/input/DMC files. Native DRM tests pass Release/
  ASan/UBSan. Its first dual-console candidate passes serial boots but FAILS
  without a UART; never flash that candidate or replay failed media. The new
  tty0-only build reaches desktop/Terminal; full two-boot acceptance is repeated
  after a Windows reader/replacement failure in test recording. All live image
  status reads must use image_state.read and keep its stable guard inode. Failed
  saves cannot skip original-handle retirement; unknown exits/publications stay
  unknown. Read spec/laptop-probe.md and the dated laptop
  journal for actual evidence. Native compiled tests are not hardware acceptance.
  Local NTFS sparse image reads may use proven allocation ranges while hashing
  every logical zero byte; this is never a physical-device verification shortcut.
  The authorized Intenso still requires complete physical write/readback.
  Access-only CreateFile returns WinError5 in this unelevated process; no write
  occurred. This is OS access, not missing user authorization. Keep developing;
  preserve the carrier, all failed artifacts, foreign work and closed NexaUI handoff.

- Current laptop status supersedes the older pending paragraphs: tty0-only
  no-UART v3 now passes both boots, six capture reviews and independent clean
  data/proof-file inspection. Read STATE and spec/usb-flash.md. The fixed Windows
  writer has13 actual Windows cases and native applicable review; it must retain
  exact Intenso/source binding, full zero writes/direct readback and original
  process exit0. No physical write yet. Windows elevation is required, not a new
  erase authorization. Never replay uncertain attempts or weaken volume locks.

- The first Intenso flash is now complete and independently verified: full
  31,457,280,000-byte direct write/readback, expected image SHA256, cleanup and
  original held process exit0. Read current STATE and spec/first-laptop-test.md;
  do not repeat the completed attempt or automatically replace later user data.
  Physical laptop boot remains untested; keep developing without waiting for it.
  The subsequent compositor --keyboard-layout us|de startup option passes seven
  native groups in Release/ASan/UBSan, but is not in this first USB image or a
  normal Settings/runtime/persistent layout selector. The image still uses US.
  Preserve the original carrier/quarantine and foreign work; NexaUI handoff stays
  closed. Full OS, hardware, lifecycle, installer and experiment goals remain work.
