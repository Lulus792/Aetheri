# Intel WLAN image inputs

The HP EliteBook 840 G4 reports PCI device `8086:24fd` for its Intel
8265/8275 adapter. The desktop now has an optional WLAN payload for the
existing Fedora 6.19.10 kernel. A scoped udev rule requests the kernel's
modalias for an unbound matching PCI device. The release rule keeps the
driver dormant on images running without that hardware.
[Linux PCI aliases](https://github.com/torvalds/linux/blob/v6.19/drivers/net/wireless/intel/iwlwifi/pcie/drv.c)
and [systemd driver discovery](https://github.com/systemd/systemd/blob/v259.10/rules.d/80-drivers.rules).

The six compressed module files match the installed carrier's RPM database
hashes and the selected kernel ABI. Each file reports the Fedora kernel
signer. The whole mirror downloads timed out. The assembly uses the verified
carrier module inputs and the original Intel API 36 files from
Linux Firmware commit `c822cbbb14ce5b8ee1f27346220640ac350bbf34`. Both
firmware files matched between tag `20260309` and its resolved commit.
The license also matched the carrier's distribution notice.
[Linux Firmware source](https://gitlab.com/kernel-firmware/linux-firmware/-/tree/c822cbbb14ce5b8ee1f27346220640ac350bbf34).

The stager verifies the Fedora release-key signature and the selected bytes
of the regulatory database RPM. It stages `regulatory.db`, its detached
signature and the original notice. The selected kernel requires a signed
regulatory database. Use the [image instructions](../../../spec/login-deployment.md#intel-pci-wlan-files)
and keep the receipt with the other image inputs.

## Offline verification

The ordinary Linux stager copies 13 files totaling 6,776,591 bytes. The
merged image module indexes resolve a reviewed `8086:24fd` alias to
`iwlwifi`. A different device alias is refused. The dependency plan
contains precisely `rfkill`, `cfg80211`, `libarc4`, `mac80211`, `iwlwifi`
and `iwlmvm`, with each dependency ordered before its consumer. `udevadm`
validates the scoped rule. A changed RPM is refused before staging writes,
and three local tests verify exact reuse and failure before image changes.
The combined desktop payload suite passes 18 tests. Seven previous input
checks pass on Windows with one Linux-specific case skipped. The native
regulatory extraction also passes after the shared RPM reader is given the
WLAN error description.

The first test tree lacked the production `/lib` link. The second test
assumed a fixed order for independent modules. The corrected test uses the
merged layout and checks the dependency order instead. The image stager
walks the same path in the development desktop builder before its existing
combined `depmod` step.

## Boot with virtual radios

The simulation module comes from the signed Fedora `kernel-modules-internal`
RPM for the same ABI. It keeps its `internal` module path and is reserved
for the VM. The first composition expected a `kernel` path. The next editor
tried to validate image-specific SELinux types against the carrier's running
policy. The corrected editor applies the base file contexts to the new
standard paths inside a private copy. It preserves the existing signed
login selection and the exact policy file.

Image `561ea4743319` passes `e2fsck -fn`. UEFI WHPX session `347ca597`
loads all six WLAN modules and the separate `mac80211_hwsim` module under
enforcing SELinux. The in-guest observer sees both `ieee80211` PHYs and
`/dev/rfkill`. The original signed login startup completes, then the guest
powers down normally at exit zero. The boot image hash stays consistent.
The VM exercises module loading on the real selected kernel with simulated
radios. Physical firmware upload, association, connectivity and the Settings
controls require their own tests. The first-version USB gate still applies.

The inputs, editor failures, staging results and VM trace are in
`local/login-review-2026-11-10/desktop-wlan-2026-11-18-evidence.tar.gz`
with SHA-256 `20b073c449a345064b1e3f2d2d82b8b7bd36f68f88334e0b656e8c119d90c639`.
