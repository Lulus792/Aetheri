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

## Later main-PC target: user-reported

Four CPU cores/eight threads and an RTX 2080 Ti. CPU model, RAM and connectors
are not yet reported; do not assume it is the development host. Intel-laptop
acceptance does not establish NVIDIA display, suspend or driver-update behavior.
Plan a separate target profile, including kernel/userspace driver and firmware
compatibility, module signing/trusted boot, monitor hotplug/scaling and updates.
The NVIDIA open-kernel-module project documents matching userspace components;
these modules alone are not the complete graphics stack.

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
