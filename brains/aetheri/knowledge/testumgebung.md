# Test environment and hardware

25 October 2026. Separate observed host data from user-reported target hardware.

## Development host: observed

- Windows workstation: HP Z4 G4, Xeon W-2123, approximately 31.7 GiB RAM.
- Active hypervisor and running WSL 2 distribution Alpine-Dev.
- Alpine reports 3.24.2, kernel 6.18.33.2-microsoft-standard-WSL2.
- Clang/C compiler, CMake, Ninja and Python 3 found in WSL.
- First snapshot: F: 230.2 GiB free, C: 72.5 GiB, D: 0.5 GiB.
  Shared WSL storage is on D:. Free space inside its virtual filesystem does not
  guarantee host capacity.

QEMU was not found on the inspected PATHs. Standard inspected Windows locations
did not contain QEMU, VirtualBox or VMware Workstation; portable installations
elsewhere are not ruled out. No /dev/kvm was present in Alpine.
Some WMI virtualization flags were false despite the running hypervisor/WSL.
This does not prove disabled firmware virtualization.

## Target laptop: user-reported

| Item | Specification |
| --- | --- |
| Model | HP EliteBook 840 G4, dedicated test device |
| CPU | Intel Core i7-7600U, 2 cores/4 threads, 2.8 GHz, up to 3.9 GHz |
| GPU | Intel HD Graphics 620 integrated |
| RAM | 16 GB DDR4 |
| Platform | x86-64, UEFI |
| SATA HDD | WDC WD5000LPLX-60ZNTT1, 465.86 GiB |
| NVMe SSD | Intel SSDPEKKF256G7H, 238.56 GiB |
| WLAN | Intel Wireless 8265/8275, PCI 8086:24fd revision 78 |

No live device enumeration, boot or driver acceptance has been performed.
The iwlwifi documentation lists Intel 8265 support, but exact firmware and actual
WLAN operation need testing. Bluetooth identity and audio devices are still unknown.
No target disk is selected and the dedicated-device designation is not a disk-wipe command.

Required workflows: WLAN, Bluetooth, headphone audio, multiple monitors, DE/US input
and scaling. Up to three 1080p/1440p/4K monitors are requested. Connection paths, refresh rates
and mixed-scaling details remain open. Wired/Bluetooth keyboards and mice, and
3.5 mm/Bluetooth headphones, are required. Suspend/resume is now required, with
separate physical acceptance on each target profile. Hibernate remains unselected.

## Main-PC target: confirmed as the development host

On 26 October 2026 the user clarified that the current development workstation
is also the intended main-PC target. Fresh read-only Windows queries observed:

- Intel Xeon W-2123, four cores/eight logical processors.
- 34065440768 bytes reported physical memory (about 31.7 GiB; nominal 32 GB).
- NVIDIA GeForce RTX 2080 Ti, Windows driver 32.0.16.1088.
- Three active monitor records: UG25F and two HP E223i. Connection technology
  reports HDMI for UG25F and external DisplayPort for both HP monitors.
- Win32_VideoController reports 1920x1080 at 60 Hz; this is not an independent
  per-monitor mode/refresh/scaling inventory or proof of each cable/adapter path.
- HypervisorPresent=true and WinHvPlatform.dll exists. Some processor capability
  flags still report false. A subsequent read-only WHvGetCapability query returned
  S_OK (0), HypervisorPresent=true, written size 4 with an 8-byte output buffer.
  No partition was created and no virtual processor executed. This is a WHP API
  capability result, not proof of QEMU's WHPX execution or guest compatibility.

These observations identify a test target, not Linux acceptance. Monitor serials,
machine identifiers and full device-instance paths are excluded from public notes.
The connector-code interpretation follows the
[Microsoft enum](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmdt/ne-d3dkmdt-_d3dkmdt_video_output_technology).
Intel-laptop acceptance does not establish NVIDIA display, suspend or driver-update behavior.
Plan a separate target profile, including kernel/userspace driver and firmware
compatibility, module signing/trusted boot, monitor hotplug/scaling and updates.
The NVIDIA open-kernel-module project documents matching userspace components;
these modules alone are not the complete graphics stack.

The user also requires support after a future AM5 upgrade. Keep generic x86-64/
UEFI boot and CPU feature detection; do not make Intel-only instructions, model
names or hard-coded device paths prerequisites. Baseline builds must not silently
inherit the development CPU through -march=native. Optimized paths need runtime
feature checks and a baseline fallback. AMD/mainboard firmware, chipset, storage,
network/audio/USB and sleep behavior require a distinct eventual physical profile.
Exact CPU, board, RAM, firmware, peripherals and retained/replaced GPU are not
selected. This is a portability requirement, not an AM5 hardware acceptance claim.

Suspend means a low-power sleep state, usually retaining the working session in
RAM. Hibernate writes a session image to disk and is a separate feature. Exact
sleep support depends on firmware, kernel configuration and device drivers.

## Budget and proposed test path

The user allows up to 16 GB VM RAM. The initial approximately 32 GB disk discussion
was followed by allocation of F: for the project, now confirmed ready. Use F:/Aetheri
for our sources, builds, caches and VMs. Existing Physim/NexaCloud files, processes
and gradual migrations must not be touched. No cleanup, relocation, process stop,
formatting or repartitioning of their resources is authorized. Measure available
space and estimate large downloads/builds; volume allocation is not a disk-wipe request.

Use F: for new approved large artifacts. Do not move or expand shared WSL storage
without a concrete plan. QEMU with Windows WHPX is a candidate; acceleration must
be probed, not assumed. TCG is a functional fallback with different performance.
WSL builds do not test Aetheri's own kernel/boot path.

A full Chromium source build was outside the original 32 GB discussion: its
official Linux instructions call for at least 100 GB free. The subsequent F:
allocation permits planning a larger budget, subject to actual free capacity and
the protected ongoing migrations. A pinned prebuilt engine or a
later agreed build service/space increase is a separate choice, not an excuse to
exceed the budget silently.

Test components first, then boot an image with serial diagnostics, verify persistence
and service failures, exercise graphical apps, and test installation/update recovery
on disposable VM disks. Only then progress to live USB and the actual laptop.
VM success does not establish laptop GPU, WLAN, audio, monitor or power behavior.

Evidence: first-round PowerShell/WSL observations; user hardware report;
[QEMU](https://www.qemu.org/docs/master/system/introduction.html),
[iwlwifi](https://wireless.docs.kernel.org/en/latest/en/users/drivers/iwlwifi.html),
[Chromium build requirements](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md).

## Latest storage clarification

The user subsequently reports F: completely clear. A read-only capacity query
showed approximately 232.4 GiB free. This supersedes earlier capacity snapshots
for planning only. Continue to use F:/Aetheri and do not format/repartition the
volume or operate on unrelated directories. No cleanup was performed here.

## P0 proposal and refreshed observation — 26 October 2026

Read-only host query: qemu-system-x86_64 and qemu-img not found on PATH;
WSL lists Alpine-Dev; F: approximately 232.4 GiB free. QEMU may exist elsewhere;
WHPX enablement/availability was not inspected. No tool/distro/VM installed.

The [P0 proposal](first-linux-prototype.md) recommends a dedicated x86-64 UEFI
QEMU VM: initially 2 vCPUs, 8 GiB RAM and 48 GiB sparse capacity, within the
16 GiB RAM ceiling. Fedora Server 44 is a proposed glibc/systemd/SELinux development
carrier, not Aetheri's final base. Pin/verify its image and actually prove enforcing
policy inside the VM. Shared WSL remains unchanged.

No personal data, raw hardware disk or writable host-folder sharing in this fixture
environment. Sparse maximum capacity is not actual usage; measure growth.
Use existing ignored vm/download/build locations, with native WHPX verification
or explicitly recorded TCG functional execution. No native-performance conclusion
follows from TCG timings.

## P0 input preparation — 26 October 2026

[vm-inputs.json](../../../spec/vm-inputs.json) pins QEMU 11.1.0 Windows build
2026-08-11 and Fedora Server netinst x86_64 44-1.7, plus initial VM resources.
The net installer is 1228384256 bytes and permits an explicit minimal ext4 guest
installation; it needs network package acquisition. Guest package versions and
licenses must be recorded/reviewed before development-library adoption. The image
does not freeze later repository contents and is not the final Aetheri base.

[prepare_vm.py](../../../tools/prepare_vm.py) can fetch or verify these exact inputs.
It creates files only under ignored downloads, verifies size and digest, refuses
to overwrite a partial/corrupt/existing name and never executes them. Interrupted
partials remain evidence; it does not silently resume or overwrite them. Cache
checks are development tooling, not a privileged hostile-file importer.

QEMU's upstream download page links Stefan Weil's Windows builds. Downloaded
206615928-byte installer matches the provider's SHA-512 and reports product
version 11.1.0. Windows Authenticode reports a certificate outside its validity
period; it is not a valid Windows signature. The inspected upstream NSIS script
requests administrator privileges and includes machine registry/menu entries.
Do not execute it as if it were a portable extraction. Reviewed extraction or a
separately verified Windows tool source remains an integration task; neither
disabling signature checks nor installing a broad new tool family is implied.

Fedora's signed checksum file was verified with the existing development GPG tool
in a project-local ignored keyring. VALIDSIG fingerprint matched the official
Fedora 44 fingerprint 36F612DCF27F7D1A48A835E4DBFCF71C6D9F90A6. Imported keys were
not granted generic trust; no personal keyring was changed. Media byte verification
also passed: the complete 1228384256-byte net installer matches SHA-256
ae20c06bea746913cadea7d80463e13f4bf55bee4df2918111c921c674b70283. The acquisition
finished with exit 0. This verifies installation media, not installed guest state.
The project-local GPG agent was stopped after verification; no extra verification
agent is intentionally left resident.

No QEMU binary/installer was executed, Windows feature toggled, VM disk created,
guest installed or Linux policy loaded. Host DLL presence is not a WHPX test.
Before A0: establish usable verified QEMU + UEFI firmware, probe actual acceleration,
create only the disposable 48 GiB guest disk, install ext4 and record guest versions,
then prove systemd, cgroup v2 and SELinux enforcing. Synthetic data only; no host
folder passthrough or raw physical disk. Separate test reports must distinguish
carrier baseline from Aetheri's own resource use.

Sources inspected for preparation:

- [QEMU Windows download links](https://www.qemu.org/download/#windows) and
  [provider history](https://qemu.weilnetz.de/w64/): source of the pinned build,
  version and checksum. The published newer source release is not assumed to be
  the version inside this Windows build.
- [QEMU v11.1.0 NSIS source](https://github.com/qemu/qemu/blob/v11.1.0/qemu.nsi):
  installer behavior, not a byte-for-byte binary reproducibility claim.
- [Fedora Server](https://fedoraproject.org/server/download/) and
  [Fedora signing fingerprints](https://fedoraproject.org/security/): exact media
  checksum workflow and independent full-fingerprint comparison.
- [WHvGetCapability](https://learn.microsoft.com/en-us/virtualization/api/hypervisor-platform/funcs/whvgetcapability):
  read-only host API query, separate from partition/virtual-processor execution.
