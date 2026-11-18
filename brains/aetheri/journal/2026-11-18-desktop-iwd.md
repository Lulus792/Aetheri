# Wireless connection manager

The optional [image payload](../../../spec/login-deployment.md#wireless-connection-manager-files)
selects iwd 3.12 and libell 0.83 from Fedora 44. The Fedora RSA header
signatures and both RPM payload digests passed. The staged daemon needs
`libell.so.0`, `libgcc_s.so.1` and `libc.so.6`. The original license notices
and the SONAME alias come from the signed packages.
[Fedora iwd package](https://packages.fedoraproject.org/pkgs/iwd/iwd/fedora-44.html)
and [Fedora libell package](https://packages.fedoraproject.org/pkgs/libell/libell/fedora-44.html).

The first enforcing image `7cdb00ec2c06` boots both virtual radios but
iwd exits before acquiring the bus name. In session `3a68c773` it reports
`DES support not found`. iwd's startup self-check asks for DES and CBC
3DES before station setup. For this kernel, `des_generic` supplies both and
depends on `libdes`. Both compressed modules match the RPM inventory and
carry the Fedora kernel signature. The combined module index resolves
`crypto-des3_ede` through `libdes` followed by `des_generic`.

With those two modules, image `97d5579e862c` starts the exact iwd ELF and
loads its configuration. Session `7231ee74` then reports
`Failed to initialize D-Bus` after the system broker is ready. libell's
default uses `/var/run/dbus/system_bus_socket`. The Aetheri unit needs
to select the actual `/run/dbus/system_bus_socket` path explicitly.

The final standalone stager copies seven files totaling 1,664,880 bytes
and creates one alias. A same-length changed `libell` RPM and a changed
`des_generic` module are refused before writing to the target tree. The
composed root's dynamic loader resolves all three needed libraries. The
18 desktop payload tests and seven image contract tests pass. Staging
adds the root-only bus rule and leaves activation off.

## Virtual WLAN connection

Image `d85a9a3548a0` passes `e2fsck -fn` and keeps the original signed login
selection and SELinux policy bytes. WHPX session `1040603b` boots under
enforcing SELinux with no external NIC. The VM-only unit loads the generic
crypto module, sets the explicit bus address and starts the pinned daemon.
The AP and station use separate simulated PHYs and throwaway WPA-PSK profiles.

The root test client discovers the AP with `Station.Scan`, connects to the
same PSK network, and observes IPv4 address `192.0.2.10` on the selected
station. It disconnects and stops the AP. An ordinary UID 1000 client gets
`org.freedesktop.DBus.Error.AccessDenied` on the iwd object inventory call.
Signed login startup and guest ACPI power-off both succeed. The original
data image and the boot image hash remain intact.

The address check is labeled `DHCP` in the fixture. It reads the actual
interface address and does not observe the address exchange. iwd can also
assign the address during association through FILS, so this establishes IPv4
assignment only. A separate AP can verify the ordinary DHCP exchange.
Production service policy, credential storage, resolver integration, Settings
and physical HP acceptance remain open. The first-version USB gate applies.

The staging receipts, boot attempts and fixture sources are in
`local/login-review-2026-11-10/desktop-iwd-2026-11-18-evidence.tar.gz`
with SHA-256 `ff2fc5ddad91988d6d1ee47d0d778aea5e83e9a747c54b6317732c146f50b1df`.

## Explicit service start and stop

The stager now installs `aetheri-wlan.service` and `main.conf` with the
original seven files and one alias. The two added files match the source
bytes. The unit starts on an explicit administrative request and uses the
system bus name for readiness. It places iwd profiles in the mode-0700
runtime directory, which systemd removes on stop. The daemon is limited to
its network capabilities, selected socket families and `/dev/rfkill`.

WHPX session `3b9a9588` boots image `1aa38f0e4bbc` with the original signed
login selection and enforcing policy. Before the first request the unit is
inactive, its runtime directory is absent and the system bus reports no
iwd owner. The root test client requests two start and stop cycles using
the installed unit. Each start yields a distinct bus owner with the same PID
as the unit's iwd ELF. Each stop retires the original pidfd and removes the
bus name, runtime directory and synthetic marker. Signed login startup and
ACPI shutdown succeed. The kernel modules are staged and indexed. The test
starts the reviewed service without a fixture preloader for DES.

The test uses the current base SELinux policy and a privileged observer.
Dedicated WLAN confinement, an authenticated Settings mediator and DNS
remain open. This lifecycle test uses two virtual radios and stops at bus
and device discovery. The earlier WPA test remains the association proof.
A separate exchange with a DHCP server is still required.

The exact images and fixture results are identified in
`local/login-review-2026-11-10/desktop-wlan-service-2026-11-18-evidence.tar.gz`
with SHA-256 `2a108915408af9016dccaf4efac77044f1f6d446b15e116ead4a6f802054a14e`.

## Dedicated SELinux service role

The WLAN service selects `aetheri_wlan_t` and requires SELinux. The image
policy labels the fixed iwd executable, read-only configuration and private
runtime directory separately. The daemon can use the wireless device,
system bus and selected network sockets. The desktop app roles remain
outside its execution, private runtime and bus boundaries. The image
compiler accepts the role through `--desktop-wlan`.

The permissive-role diagnostic `5a81c40c` identifies the original iwd
process in the dedicated domain. The observer's direct control attempt
ends with `EACCES`. The old signed login selection rejects the changed
policy. The next strict run starts the unit but exits while loading the
configuration. An isolated boot removes the greeter's unrelated audit
traffic and exposes two original denials in `d9bd6e2e`. iwd maps
`main.conf` and creates a private `hotspot` directory. Both operations are
admitted only on their selected types.

Session `0f512ebe` verifies two strict service starts, bus-owner matches,
typed runtime directories, original exits and runtime removal. Its
headless target omitted logind and the host eventually quit the VM after
the ACPI power-off timeout. The service also exposed profile watch, network
sysctl search, its own keyring and socket denials. The next candidate admits
profile watches, read-only network sysctl access and the identified private
socket operations. `2d0e05bc` shows that interface discovery uses
`SIOCGIFNAME` on a UNIX datagram socket. The policy limits that socket's
extended ioctl access to request `0x8910`.
[Linux socket request numbers](https://github.com/torvalds/linux/blob/v6.19/include/uapi/linux/sockios.h).

Fedora's normal offline configuration has `expand-check=0`. Enabling it
for the full stack exposes existing audio and lock assertions against a
conditional distribution-wide file-map rule. The full compiler keeps the
distribution selection. A private strict compile of the base desktop and
WLAN role accepts the original policy and rejects three direct changes for
shadow reads, generic executable entry and Settings-to-iwd bus access.

Image `37ec642aca02` boots the complete candidate policy in enforcing mode.
Session `d06ad76c` starts from inactive, starts iwd twice under
`aetheri_wlan_t`, and matches each original daemon to the bus owner.
Each cycle produces new interface indexes for both virtual radios. Both
stops retire the original processes and remove the typed runtime. ACPI
power-off exits zero and the data overlay check passes. The isolated
boot target exercises the service without starting the graphical login.

The final policy keeps the write denial on general network sysctls. The
trace shows iwd attempting to write the per-interface IPv6 `accept_ra`
setting. IPv6 admission needs a separate path for that operation. A signed
desktop image with the new policy, WPA under this role, independent DHCP
and the Settings mediator remain open. The first-version USB gate applies.

The compiled policy, assertion checks and original boot attempts are in
`local/login-review-2026-11-10/desktop-wlan-policy-2026-11-18-evidence.tar.gz`
with SHA-256 `8f73e251e9b1e7df0b4dfa6402b9034e56b88e1e1592062daa778a51b1f5f36b`.

## Signed desktop boot with WLAN

Two cold UEFI boots, `760c28e1` and `6e52e743`, read exactly the same
3,731,274 bytes from enforcing selinuxfs. The effective SHA-256 is
`6017642b4d446a171b070ca689815f24434a1def5d962e9d8d467fe6dfdc5d7a`.
The staged `policy.35` is `2628a410426b…7b26`. The other format-5 selection
rows, release ID and account selection stay byte-for-byte equal to the
existing signed fixture. The normal encoder regenerates the record from
the reviewed stage, the verified drop-in and the new kernel readback. The
existing local VM Ed25519 key signs the record.

The control `60bf069267ae`/`dadf504c` keeps the earlier valid signature.
The installed startup verifier rejects its policy selection with `-74`.
Image `c8777115b7b6`/`50558f29` uses the updated signature, passes the
installed login checks and opens the authenticated desktop through PAM and
the graphical greeter. The rendered desktop, ordinary logout, PAM close,
replacement greeter and ACPI power-off pass.

In the positive boot, iwd starts inactive. The isolated VM observer
requests two administrative start and stop cycles before login. Each cycle
matches the bus owner to the original confined process, observes both new
virtual interfaces and confirms original process exit and runtime directory
removal. Both desktop attempts power off at exit zero. The original data
volume digest stays intact and both qcow2 overlays pass verification.
The package receipt covers the WLAN files separately from the signed login
selection. WLAN control from the user session, WPA and DHCP under the new
role, DNS and IPv6 still need verification. The USB gate applies.

The readbacks, boot traces, screenshots and composition receipts are in
`local/login-review-2026-11-10/desktop-wlan-signed-2026-11-18-evidence.tar.gz`
with SHA-256 `a2fe65de60627a19cb402724c58f27ae5de6936e7e1af7d224f087984af934e9`.

## WLAN control interface

The Settings contract is `org.aetheri.Wlan1`. The optional controller
accepts `GetState` and `SetEnabled` on the system bus. It uses the existing
non-augmented peer capture to obtain the sender's original ProcessFD and
checks live credentials, the exact Settings label, the original session
scope and active local seat0. A state change sends a fixed `StartUnit` or
`StopUnit` request to the pinned PID 1 bus peer. The reply acknowledges the
manager job. Unit readiness remains observable through `GetState`.

The controller service activates on demand and has a 30-second idle wait.
Its SELinux role grants start and stop on the dedicated WLAN unit label.
Settings can send the controller requests. Apps retain their direct iwd
denial, and the controller has no direct iwd permission in this selection.
The first controller applies an on/off operation at the unit boundary.

The Fedora C17 Release targets build with the selected SDK. The native
`wlan.caller` test uses the real process descriptor and credentials with
scripted logind and context observations. It checks the admitted scope,
UID and labels plus wrong PID, remote and inactive sessions, greeter,
wrong scope, another active session, replaced session and cgroup drift.
The unconfined standalone executable is refused. The payload without the
controller still selects nine files. The optional payload selects 13 files
and one alias, copies ELF `36d1ea410fe5…39ba` without an RPATH and refuses a
non-ELF before writing to the image tree.

The full offline policy compiles as `2859a121008b…eeca`. The strict desktop
WLAN compile refuses three changed inputs that try to grant generic unit
start, direct Settings unit start or controller access to iwd. The 45
desktop and login Python tests also pass. The controller still needs
the live enforcing bus and systemd execution check from an original
Settings process. The visual switch, WPA routes and DHCP remain open.

The Release build, staged payload and offline compiler checks are in
`local/login-review-2026-11-10/desktop-wlan-control-2026-11-18-evidence.tar.gz`
with SHA-256 `381d16fe398038b6a7f57a88e256b69a99fe960f4d9010c340c0e4858dfeecee`.

## Installed WLAN control from Settings

Image `e4cc9e8aba8d` boots with enforcing SELinux and opens the desktop
through graphical PAM login. The desktop search launches a test ELF
at the fixed Settings path and the first request activates the WLAN
controller. The controller returns `NotAuthorized` on `GetState`.
The first diagnostic run `6e3a6d89` reproduces the rejection without
the expected stderr trace. In `bd68ac63`, a diagnostic reply reports
successful peer capture. Both pidfd session and scope reads return
`-EACCES`. No manager call has run at that point.

The selected systemd cgroup helpers resolve the process path relative to
PID 1. The matching Fedora carrier labels `/proc/1/cgroup` as `init_t`,
`/run/systemd` as `init_var_run_t`, and the logind seat and session
directories separately. The revised rules grant filesystem observation
through those labels and prohibit file writes and PID 1 tracing.
[systemd 259.10 cgroup path resolution](https://github.com/systemd/systemd/blob/v259.10/src/basic/cgroup-util.c#L646-L705).

Two cold boots `7f400d38` and `cc643a7e` each read the same 3,810,236
enforcing policy bytes. The effective SHA-256 is
`e995f8d663e3f0843ca119cd0ed99a541de710f11f0025cdacd05185159e9b34`.
The installed policy digest is `79ae00124ad9…690f`. The canonical
format-5 VM record changes the policy and Settings probe rows. It
preserves the other rows and uses the existing VM-only signing key.
The final boot uses the original controller ELF `36d1ea410fe5…39ba`
at its staged mode.

Image `f54a34f2d3a9`, session `92901c25`, boots normally and launches the
Settings probe as UID 1000. It reads off, submits a start job and reads on.
While the unit is on, both direct requests, one to iwd and one to systemd,
return access denied. A fresh read still reports on. The probe submits
a stop job and observes off. Ordinary logout closes PAM and displays the
replacement greeter. ACPI power-off exits zero, the qcow2 overlay check
reports zero errors and the original disk digests agree.

The strict desktop WLAN policy compiles and rejects six altered inputs
covering generic unit start, direct Settings start, controller access to
iwd, session writes, PID 1 file writes and PID 1 tracing. All 45 desktop
and login Python tests pass. The live caller is a VM-only Settings
replacement. Live denial after switching away from the user's seat
remains open. The visible switch, network joining, DHCP/DNS and physical
WLAN also require verification. The first-version USB gate applies.

The failed and successful runs, policy, readback and probe are archived in
`local/login-review-2026-11-10/desktop-wlan-settings-2026-11-18-evidence.tar.gz`
with SHA-256 `c6fc4433b5880ce9e833ffe59a2dd7d1a1bef2b9b33bccbbe6adf33553ba7085`.

## Native Network page

The selected native Settings build adds the [Network page](../../../spec/settings.md#network-page)
using the existing switch style and Wi-Fi icon. It reads on arrival or
explicit refresh. A toggle queues one `SetEnabled` and waits for
`GetState` to observe the target. The window loop integrates the
asynchronous D-Bus connection with its existing wait sources. Subsequent
reads are bounded to one operation window. Unconfirmed changes require
fresh state before the switch is usable again.

The first signed native app image `15d7fe785ead`, VM `1708f000`, passes
off/on/off, F5, logout and ACPI power-off. The switch loses keyboard focus
while disabled for the pending change. The view now returns focus after a
successful observation when focus was lost on that switch. A deliberate
move to a different control cancels the return. The view test covers both
outcomes, pending and unknown states, and the disabled switch.

Final image `40ae87db5c27`, VM `156753ed`, selects the native Settings ELF
`c4ccd67113de…d69b`. The prior enforcing policy digest and controller are
reused. The Settings row is the only changed row between the two native
image selections. After graphical login, the desktop launches the usual
Settings window. Tab reaches Network and Wi-Fi, and Space turns the
service on. Once the switch regains focus, another Space turns it off.
Both readable states and switch semantics agree. F5 produces a fresh
off observation and focus returns again. Settings exits at zero. The
session closes normally, the replacement greeter appears, and ACPI
power-off ends the VM at zero. The data overlay check reports zero errors.

Five Settings CTests pass in each Release build, with the controller
option enabled and disabled. The unprivileged WLAN caller test and 45
desktop/login Python tests pass. View cases cover both languages, three
themes, text scales from 100% to 200%, narrow and wide layouts, and 1×
and 2× text rasterization. The VM logs retain the nonfatal denied
`ioctlcmd=0x542a` on `/proc/1/cgroup`. The image uses virtual radios.
Network discovery, joining, DHCP and DNS, a live inactive-session call,
and physical laptop WLAN remain open. The first-version USB gate applies.

The build receipts, tests, signed VM results and screenshots are in
`local/login-review-2026-11-10/desktop-wlan-network-view-2026-11-18-evidence.tar.gz`
with SHA-256 `e452b2e90b465e1eb8407d5958521838b953f94b16c337a793e563e67f885cb5`.

## Network discovery from Settings

Wlan1 1.1 adds an explicit scan request and a bounded snapshot of
display names, security, signal and connection state. The controller
uses the original confined iwd process and version 3.12 station and
network interfaces. Settings requests one scan and reads while iwd
reports scanning. It exposes a progress, failure, empty or populated
state and keeps the synchronized switch state. Names in the list carry
no connection authority.

The Release builds pass five Settings CTests with WLAN enabled and five
with it disabled. The unprivileged caller test and 45 desktop/login
Python tests pass. The optional package count stays at 13 files and one
alias. It selects controller `d90d033bb74c…6462` and Settings
`ad8e795ee2a5…0e2c`. Strict policy compilation rejects eight altered
inputs, including direct app-to-iwd, profile-file access and tracing iwd.

The full installed policy is `bbe900addf6e…7c13`. Two cold readbacks
`ab9db52b` and `891ce45f` agree on 3,810,308 enforcing bytes, SHA-256
`c396c6562cdb90c7185e1c690243cd041899407ef274f69b7edb5a0d86b9ec1c`.
The two native cases use the same format-5 selection bytes.

In `f5d41666`, the broker denies the initial AP fixture's direct
unconfined-service request. The VM-only fixture then uses the existing
controller role while the installed controller stays at its fixed path.
In `2d7c5905`, station discovery in the fixture fails and the boot trace
places iwd startup before the two virtual radios are ready. A VM-only
drop-in orders iwd after virtual radio discovery. Run `5d02ec21` starts
the AP successfully and the harness times out on the ANSI-decorated
systemd marker. The next run checks the service's completion marker.

Signed image `ee55c631fbbb`, VM `b75e7341`, displays exactly one
network after a keyboard scan. The native Settings frame shows
`Aetheri-Scan-Lab`, Password protected and `-30 dBm`. The scan button
regains focus. Turning Wi-Fi off clears the displayed list. A repeat
scan after restarting iwd in `fedc56f3` still returns one row, so that
run does not establish an empty radio environment.

In the cold boot of image `ccdac6c14bf7`, VM `ad993a64`, the AP unit is
absent from the boot dependencies. Settings starts the WLAN service
through the switch and scans two virtual stations. It displays No
networks found and returns focus to the scan control. Both accepted
VMs close Settings, log out normally, power off via ACPI at exit zero
and pass the qcow2 overlay check. The earlier cgroup ioctl denials stay
visible in the logs. Network joining, DHCP and DNS acceptance and testing
on the HP laptop remain open. The first-version USB gate applies.

The build receipts, scans, controls and screenshots are in
`local/login-review-2026-11-10/desktop-wlan-discovery-2026-11-18-evidence.tar.gz`
with SHA-256 `4e9599217e6dca373243240d6e7792fdf5160bef8f904accb42855648b33606e`.

## WLAN association from Settings

Wlan1 1.2 adds a two-minute, one-use selection for the original iwd network
object. Each snapshot is bound to the original Settings process and its
session cgroup, plus the original iwd process. The controller checks the
local session throughout the pending request. Its passphrase agent accepts
one request for the selected object from that same iwd connection. The
on-demand agent connection closes after the result or timeout.

The native Settings window displays a connection form for each selectable
open or personal network. A password stays masked in the field and its
semantic value stays empty. Escape returns focus to the original row. The
view regression exercises short input, cancellation, German at 200% text
size, and the explicit confirmation of an open network. Settings reads a
new snapshot after attempting an association, and the controller checks the
peer's `Network.Connected` property before reporting success.

Both Release profiles pass five Settings CTests. The unprivileged controller
caller check and 45 desktop and login Python tests pass. The installed
package still contains 13 files and one library alias.

Signed test `561ba918` stops when the harness misses the AP's completion
marker. The AP completed and signed startup succeeded. The corrected
harness reaches password entry and submits a join in `59db5e0d`. The broker
refuses the actual `RequestPassphrase` method between the two original bus
peers. Settings reports the failed operation and returns a fresh scan row.
The updated bus config admits the three descriptor-free methods on the
agent path for root senders. The existing enforcing SELinux policy and the
agent's live sender check still apply.

The final package selects controller `17a87246cc09…d5a7`, Settings
`dc6199599f7b…55c3` and broker config `487deb05fc5d…3afc`. Signed image
`5d555d013e6b`, VM `9268f373`, uses the known enforcing policy readback
`c396c6562cdb…ec1c`. With keyboard input from the virtual seat,
Settings discovers the protected fixture network, cancels one draft,
and submits a fresh masked password. It then displays the connected row.
Wi-Fi turns off, the original Settings process exits at zero, PAM closes
and the replacement greeter appears. ACPI powers the VM down at zero and
the data overlay passes its consistency check. This case requires no
external network. A second cold VM `a3c9b372` on the same signed image
rejects an incorrect password, returns a disconnected row in a new
snapshot, and connects after explicit reselection with the correct
password. It passes the same retirement and overlay checks.

Independent DHCP and DNS, an open-network live join,
and physical WLAN testing remain open. The first-version USB gate applies.
The staged iwd profile uses `NameResolvingService=none`. The resolver
therefore needs its own integration and acceptance case.

The build and VM records are in
`local/login-review-2026-11-10/desktop-wlan-join-2026-11-18-evidence.tar.gz`
with SHA-256 `bbdd87e4acecdf21726721f9094c107d85c73c03a64f7935026f702ef255561b`.

## Open WLAN and IPv4 configuration

A separate hostapd process owns the first simulated radio. A test process
linked to the selected libell serves IPv4 leases on that interface. The
installed iwd process uses the second radio. Only the VM fixture selects
the station interface explicitly. The image boots with no external NIC.
The production Settings ELF, controller and enforcing policy retain
their signed association inputs.

The carrier verifies the Fedora signature and payload digest for
hostapd 2.11, libnl 3.12, libell 0.83 and OpenSSL 3.5.9. The target
image's loader resolves both fixture executables. The first offline
editor stops while writing the receipt because its local path shadows
the source lookup. The resumed editor reuses the 29 verified parts
and passes the ext4 check.

Signed image `1eb8ab271b99`, VM `57c171bd`, opens the regular Settings
window, turns WLAN on, scans, selects the open network, and confirms
the connection without a password field. Hostapd reports the station's
MAC. The separate DHCP server receives DISCOVER, sends ACK through
Rapid Commit and records a lease for the same MAC. The independent
observer sees `192.0.2.80/24` on the station and that source address
selected for an off-subnet route. The harness originally waits for OFFER
and stops because its four-message expectation does not cover Rapid Commit.
The classical fixture disables Rapid Commit on the test server only.

Signed image `9feea5e21ad6`, VM `42da0e06`, repeats the full open-network
flow from the initially inactive WLAN service. The separate server records
DISCOVER, OFFER, REQUEST and ACK in order. The offered address and logged
lease match the station's actual `192.0.2.80/24`. The lease MAC matches the
hostapd association. A route lookup selects the leased source for an
off-subnet test address. After Wi-Fi is switched off, the station address
disappears and a fresh socket returns `ENETUNREACH` for that destination.
Settings exits normally, PAM closes, the greeter returns, and ACPI
power-off and the data overlay check pass. DHCP renewal and physical
acceptance remain open.

For DNS, the inspected Fedora 44 `systemd-resolved-259.10-1.fc44.x86_64`
RPM passes its Fedora signature and payload digest check. Its SHA-256 is
`a5d7893d4feb79adae49d39b179581df47b79910df82560bc6f43f701f1ab977`.
The daemon needs the matching systemd shared library and both OpenSSL
libraries. The package supplies the fixed `systemd-resolve` account with
UID and GID 193. Aetheri's local account lookup already selects
`hosts: files dns`. The next image needs the resolver's account, startup
ordering, a loopback stub configuration and the bounded iwd D-Bus calls
for per-link DNS. Its acceptance case needs a real upstream DNS query and
removal of the link's DNS state after Wi-Fi is turned off. [Fedora resolver
package](https://packages.fedoraproject.org/pkgs/systemd/systemd-resolved/).

The packet, route, boot and fixture records are in
`local/login-review-2026-11-10/desktop-wlan-dhcp-2026-11-18-evidence.tar.gz`
with SHA-256 `b3619050ad9b27ad86104ee2038a47dba8a59c47f2a7c9689c90cff1f15c7392`.

## Resolver and DNS lifetime

The optional image payload stages the signed Fedora 44 resolver 259.10 and
the matching OpenSSL 3.5.9 TLS library. The Linux staging check copies 21
files and two library aliases, reuses all 21 files on an exact retry, and
rejects a changed resolver RPM before image writes. The earlier WLAN-only
profile still stages its 13 files and one alias. The generated unit removes
the two vendor socket dependencies. The WLAN unit starts the resolver on
demand, and the ordinary libc path uses its loopback stub. The offline
loader resolves the daemon and its selected dependencies.
[Upstream service unit](https://github.com/systemd/systemd/blob/v259.10/units/systemd-resolved.service.in)

VM `dc812923` receives the upstream DNS address through DHCP. An independent
DNS peer answers the test name and the ordinary UID 1000 glibc client receives
`198.51.100.43`. Wi-Fi off removes the address and route. The first harness
times out waiting for DNS retirement. The next image projects the corrected
service unit and distinguishes the listener from its runtime files. Its
first launch script times out waiting for the editor, then a fresh launch
of the completed image (`eea57e3f`) confirms the TCP listener stops while
the mode-0644 stub file remains.

The diagnostic image records the effective systemd properties and filesystem
labels. VM `937fb7e5` observes the online resolver, then the Settings join
times out after hostapd records association. VM `2cc4985e` toggles Wi-Fi
without joining and observes `ActiveState=inactive`, `MainPID=0` and
`RuntimeDirectoryPreserve=no`. The same inode for the stub remains under
the tmpfs runtime directory with the Fedora `net_conf_t` label. Its serial
stream interleaves the Settings exit marker with an audit message, and the
harness times out at that check. Both diagnostic overlays pass consistency.

The exact enforcing policy denies PID 1 `unlink` on `net_conf_t` and allows
it on `systemd_resolved_var_run_t`. The optional resolver file contexts use
the existing runtime type for the two literal generated files. The normal
image compiler produces the same binary policy as before, `bbe900addf6e5…7c13`,
and the strict WLAN subset compiles. A strict compile of the combined Sound,
lock and WLAN profile reports two existing conditional `map` conflicts.
The same compile without the resolver context option reports the identical
pair. Ordinary sandboxed application network admission remains independent.
[Runtime cleanup in systemd 259.10](https://github.com/systemd/systemd/blob/v259.10/src/core/unit.c#L5783)

Image `e5a0816facb7`, VM `e0390a4a`, passes signed login selection and joins
the open network from Settings. A separate DHCP peer performs Discover,
Offer, Request and Acknowledge. The resolver uses that lease's DNS address,
and the UID 1000 glibc lookup receives the answer from the independent DNS
peer. When Wi-Fi turns off, the IPv4 address and route disappear, the TCP
stub refuses connections, and the runtime directory and stub configuration
are absent. The ordinary client no longer gets the test answer. Settings
exits at zero, the original PAM session closes, the greeter returns, and
ACPI power-off and the QCOW2 overlay check pass. The run uses two virtual
radios in one guest with no external NIC. DHCP renewal, IPv6, and physical
hardware acceptance remain open.

Twenty-eight Windows payload, image and login-preparation tests pass. The VM
agrees with the selected enforcing readback `c396c6562cdb…ec1c`. Its boot disk
uses the existing read-only base, a shared block patch and one signature
cluster. The compact test record is
`local/login-review-2026-11-10/desktop-wlan-dns-2026-11-18-evidence.tar.gz`
with SHA-256 `d7dca811f4ae19b8047ac5dacec29e6bbad75ed59cdc5db30bb22c3ee4baefa7`.
