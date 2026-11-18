# State

Updated 18 November 2026. Aetheri has a development desktop with a user-tested
HP USB boot and a separate signed Linux VM profile. The
[project scope](PROJECT.md) and [milestones](knowledge/etappen.md) define the
first version.

## Current work

The signed desktop joins an open or WPA-PSK Wi-Fi network from Settings.
VM `e0390a4a` confirms IPv4 DHCP, an ordinary user DNS lookup through the
DHCP-supplied server, and removal of the address, route and resolver
runtime after Wi-Fi off. Settings exit, logout and ACPI power-off pass.
Lease renewal, IPv6 and physical WLAN remain open.
[WLAN record](journal/2026-11-18-desktop-iwd.md#resolver-and-dns-lifetime)

## Verified milestones

| Area | Latest verified result |
| --- | --- |
| HP EliteBook 840 G4 | The existing USB image boots. The user verified file creation, editing, retention and language changes. The user also reported missing `cat` output and delayed Files refresh. [Hardware report](journal/2026-11-08-hardware-feedback.md) |
| Installed desktop and apps | The signed VM passes PAM login, Terminal `cat`, immediate Files creation and editor save, Settings refresh, logout and ACPI shutdown. [Login and app record](journal/2026-11-17-login-image-boot.md#fixed-desktop-apps-in-release-selection) |
| Sound | The signed VM uses a virtual HDA output and verifies volume and mute through Settings. Analog headphone output and Bluetooth audio need physical acceptance. [Sound record](journal/2026-11-18-desktop-sound-image.md#installed-hda-sound) |
| Suspend | The signed VM enters ACPI S3, resumes to the lock dialog and returns to the same desktop after fresh PAM authentication. Repeat suspend passes. Physical suspend remains open. [Suspend record](journal/2026-11-18-installed-suspend-guard.md#signed-suspend-and-fresh-unlock) |
| App ownership | Fixed-profile fixtures exercise the versioned catalogue and supervised app lifecycle. General install, brokered access, complete removal and recovery remain in development. [Runtime contracts](../../spec/runtime.md) |

Remaining first-version work also includes physical Bluetooth and display
testing, browser and project apps, installer and encryption, updates and
recovery, and a resettable experiment desktop. The
[release decision](DECISIONS.md#hold-the-next-usb-build-until-first-version-readiness--9-november-2026)
holds the next USB build and flash until the agreed scope is implemented and
verified. The [first laptop guide](../../spec/first-laptop-test.md) describes the
already tested image.

## Development and coordination

The P0 Fedora VM retains the protected prototype state. The separate audio lab
carries the current desktop and hardware test toolchain. Use the
[development storage guide](../../spec/development-storage.md) and
[cleanup record](journal/2026-11-18-storage-retention.md) when creating or
retiring test media.

The NexaUI handoff has been delivered. Update the existing catalogue when
requirements or evidence change. See the [coordination rule](knowledge/nexaui.md#maintenance-rule--confirmed-5-november-2026).

The [journal](journal/) contains the detailed test history. Older handoff
entries are available in the
[previous state snapshot](https://github.com/Lulus792/Aetheri/blob/3ca53b3568be75463f64c559049fb4348d091172/brains/aetheri/STATE.md).
