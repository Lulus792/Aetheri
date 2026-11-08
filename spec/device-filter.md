# Device filtering in the development image

The first graphical image configured a restricted VT service but omitted
systemd's dynamically loaded libbpf backend. Its serial explicitly reported
disabled cgroup BPF features. That configuration was not accepted as an enforced
device filter. The new image build includes the backend and two startup probes
before opening the desktop. Actual executed results belong in the dated journal;
building these units alone is not acceptance.

## Dependency and scope

`spec/device-filter-inputs.json` selects Fedora44 libbpf
`2:1.6.3-2.fc44.x86_64`, SONAME1. The signed RPM is read as data; only its pinned
DSO is extracted to a fresh image tree. No package scripts, package transaction
or carrier library replacement runs. Full Fedora44 signing fingerprint, RPM
digest, individual payload digest and licence digest are pinned. The original
1426-file desktop runtime inventory remains independent.

The11 May Fedora patch updates a bpf_stream_vprintk declaration for kernel7.0;
it is not represented as a security fix. The current Fedora44 packaging branch
still selects1.6.3-2 at this review. Upstream already has newer releases, including
1.8.0; they are not required for this existing systemd259 device-controller API.
The selected DSO directly needs libelf, zlib and libc; libelf adds zstd, all already
in the pinned image. The pin's `needed` list records that runtime closure, not
only ELF DT_NEEDED entries. Independent readelf inspection confirms SONAME1
and the three direct entries.
The RPM itself omits licence files, so the matching upstream BSD-2-Clause text
is separately pinned and included. Original Aetheri code remains MIT.

Review date:8 November2026. Upstream's advisory page displays no published
advisories, but that is not a vulnerability clearance. Open reports include
CVE-2025-29481 about object initialization and issue960 about crafted BPF ELF
CO-RE relocation, reported against1.7.0. Exact affected/fixed status for the
selected Fedora DSO is not established by those reports. This adoption is limited
to the separate development image's trusted PID1-generated device rules. The
reviewed systemd259 path constructs instructions itself and invokes BPF_PROG_LOAD;
it does not accept user-supplied ELF/CO-RE objects for this device controller.
Do not expose arbitrary object loading, BPF delegation or claim production
security review from this scoped test. Broader image security/update review is
still required before daily-use installation.

## Actual access probe

Two root-owned oneshots run the same C17 executable with no effective/permitted/
inheritable capabilities and NoNewPrivileges. Both require actual PID1/systemd,
UEFI, read-only ext4 root, cgroup2 and the image's independently generated root
partition identifier. The selected block node must refer to the actual root
device, be root-owned and readable by its owner.

1. `--baseline`, with DevicePolicy=auto and no explicit allowlist, opens that
   root partition read-only, joins its actual device number and512MiB extent,
   and closes it without reading/writing filesystem content.
2. `--denied`, with DevicePolicy=closed and an explicit null-device allowance,
   requires the same open to fail with EPERM. Both modes must successfully open
   the actual `/dev/null` character device read-write. No device write occurs.

This baseline prevents a missing device, ordinary DAC denial or read-only media
from being mistaken for device-filter enforcement. The denied service has no
PrivateDevices namespace hiding the node. The separate broker keeps its vendor
PrivateDevices sandbox; that mechanism is not the subject of this probe.

Both probes must finish before the existing restricted VT service and desktop
start. Native results include the configured nonce and actual boot ID. The host
controller requires exactly the ordered baseline/denial pair from the same boot,
rejects mixed/duplicate/failure records and the missing-BPF warning, then still
requires real app input, persistent documents and normal shutdown. Recheck after
shutdown so later output cannot silently invalidate the observations.

This verifies this fixed pair of services in an owned VM. It does not establish
all app permissions, production MAC policy, caller admission, device revocation,
hotplug or physical-hardware acceptance. The USB desktop must carry this runtime
and repeat the relevant test in its own integrated image.

## References

- systemd259 [device controller](https://raw.githubusercontent.com/systemd/systemd/v259/src/core/bpf-devices.c),
  [instruction loader](https://raw.githubusercontent.com/systemd/systemd/v259/src/shared/bpf-program.c)
  and [dynamic backend](https://raw.githubusercontent.com/systemd/systemd/v259/src/shared/bpf-dlopen.c).
- [Fedora44 package](https://packages.fedoraproject.org/pkgs/libbpf/libbpf/)
  and [packaging source](https://src.fedoraproject.org/rpms/libbpf/raw/f44/f/libbpf.spec).
  Signed RPM metadata and payload bytes were independently read in the carrier.
- [Upstream releases](https://github.com/libbpf/libbpf/releases),
  [advisory page](https://github.com/libbpf/libbpf/security/advisories),
  [CVE report898](https://github.com/libbpf/libbpf/issues/898),
  [CO-RE report960](https://github.com/libbpf/libbpf/issues/960) and
  [BSD licence](https://raw.githubusercontent.com/libbpf/libbpf/v1.6.3/LICENSE.BSD-2-Clause).
