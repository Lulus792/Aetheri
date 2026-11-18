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
