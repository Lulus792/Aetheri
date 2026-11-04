# Experiment environments

4 November 2026. Confirmed user requirement: provide an early, resettable place
for OS experiments and later AI-controlled work, and support running VMs in
Aetheri. The user selected a complete Aetheri desktop first, persistent changes
until explicit reset and individual file/network/device permission.
This is a product requirement and a reviewed technical direction, not an
implemented feature or current isolation acceptance.

## First experience

Open an independent Aetheri desktop, change settings, install tools, edit system
code and reboot its guest without changing the daily-use installation. Keep a
known baseline and offer an explicit reset. Later, run AI tools with terminal or
desktop control inside the same boundary. AI permission inside a guest grants
no authority over the host, its credentials or the VM controller.

The guest may have its own administrator account for kernel, service and OS
experiments. Its administrator remains untrusted by the host. A guest crash is
an expected recoverable event. Host-kernel/hypervisor/device bugs and physical
resource exhaustion prevent an absolute promise that the host can never fail.

## Recommended boundary

| Option | Appropriate work | Important limit |
| --- | --- | --- |
| App sandbox/namespaces | Ordinary app restrictions and fast lightweight tools | Shares the host kernel; guest-like root or kernel experiments require a separate boundary |
| Hardware-virtualized VM | Full desktop, guest root, kernel/service changes and untrusted AI tools | More RAM/disk/startup cost; hypervisor, device and control surfaces require confinement |
| Separate physical machine | Driver/firmware/hardware experiments | Needed when virtual devices cannot reproduce actual hardware; hardware recovery must be reviewed separately |

Recommend QEMU/KVM with a supported x86-64 machine type on native Linux, subject
to exact version/build/device/license/security review before adoption. Reuse an
existing maintained virtualization foundation; original Aetheri UI, lifecycle,
ownership and policy integration remain our work. Compare direct supervised QEMU
with a libvirt backend at implementation time: direct control is smaller but
requires us to implement all supervision/confinement; libvirt supplies mature
resource/device/security management with additional dependencies and services.
No backend or new service is installed by this plan.

Integrate the future original UI through per-user mediation and typed system
authority requests. The privileged boundary chooses an allowed VM profile and
protected image slots; apps and guest agents cannot submit arbitrary host paths,
QEMU arguments or management commands. Run each VM as a separately confined,
unprivileged supervised process. Give UI status from events and durable owner
records, with actual current invocation/process checks before mutations. Prefer
existing authorities and on-demand helpers; justify any additional resident
backend explicitly. Do not create speculative source trees, schemas or APIs yet.

Software emulation remains useful for compatibility tests. It must not silently
replace the reviewed isolation boundary for untrusted work: QEMU explicitly
excludes TCG from its supported guest-isolation security guarantees. The current
Windows development carrier is separate from this future Aetheri-host feature;
its existence proves neither nested virtualization nor experiment isolation.

## Confirmed behavior and recommended implementation

- No host disks, home directory, credentials, SSH agents, clipboard, drag/drop,
  USB/PCI/GPU passthrough or shared folders. Use a reviewed virtual display first;
  3D acceleration and remote-display listeners need their own acceptance.
- No network by default. Offer explicit restricted connectivity for downloads or
  remote AI. NAT alone does not isolate host/LAN services; enforce host, LAN and
  management-endpoint restrictions, including IPv6 and DNS. Local model services
  need a dedicated bounded gateway, not unrestricted host networking.
- Import selected files as copies. Export selected results through the host's
  own picker and destination approval. Treat exported files, patches and disk
  images as untrusted. No automatic merge into the OS, projects or documents.
- Retain changes until explicit reset, as confirmed by the user. Optional
  disposable sessions may follow later. Keep disk overlays separate from an
  immutable reviewed baseline. Include guest UEFI variables, virtual TPM state
  if used, extra writable disks and controller metadata in reset semantics.
- Bound host-side total memory (including VM overhead), CPU, process count,
  storage/log/overlay growth and disk IO. Reserve host capacity. The existing
  16 GiB VM allowance is a ceiling, not a default for every experiment. Initial
  desktop sizing must be measured on the 16 GiB laptop and current main PC.
- Authenticate local management through protected AF_UNIX endpoints. Guest
  networks must never reach QMP, host control APIs or privileged brokers.
- Give each environment explicit ownership of images, overlays, checkpoints,
  logs, credentials and helper services. Show actual disk use and all retained
  checkpoints; deletion previews list shared bases and independent exports.
  Remove only after exact process/unit/job/holder drain. Unknown effects retain
  intent and evidence for reconciliation.

Checkpoints are not backups and cannot undo network requests, cloud changes,
credential disclosure or writes to explicitly shared external storage. First
support disk-consistent reset while the VM is stopped; live RAM/device snapshots
are a separate feature with compatibility and secret-retention consequences.
Never use a monitor commit command to merge experiments into the protected base.

## Delivery and acceptance

1. Define the host controller, storage ownership, supervised VM process,
   resource limits and restart/reset journal. Exercise these with a trusted
   test guest before adding a desktop or untrusted AI workloads.
2. As soon as the original Aetheri image boots and the compositor/session/apps
   provide a useful desktop, deliver that complete desktop in the experiment
   VM. This is an early desktop acceptance gate, before daily-use deployment.
3. Add independently scoped AI control, reviewed network/file exchange and
   then general VM import/creation. Do not require all future apps to be finished
   before the first useful desktop experiment.

| Requirement | Required observable acceptance; none claimed yet |
| --- | --- |
| Complete desktop first | Aetheri boots to its own graphical session and apps inside a VM; normal window/input/reboot paths work |
| Host separation | Guest root cannot read/write host canary files, devices, control sockets or service endpoints; explicit exchange stays bounded |
| Broken guest recovery | Deliberately break guest boot/settings and reset to the selected baseline; the host stays usable |
| Resource protection | Guest memory/CPU/IO/disk pressure hits configured limits; host input and essential services remain responsive |
| Reliable reset | All declared writable state resets; unrelated environments, shared bases and exports remain intact |
| Interrupted operations | Host/controller/guest exit during create/start/reset/delete reconciles the same intent without double-start or mistaken deletion |
| AI confinement | AI tools can modify the guest while failing host-file, secret, local-network and controller-access attempts |
| General VMs | Reviewed supported guest formats boot with explicit resource/device profiles and the same ownership/removal behavior |

UX/name, detailed desktop controls and external-device
workflows remain user decisions. Exact dependencies, enforcement mechanisms,
budgets and image compatibility are engineering work under the agreed review
policy. App sandboxing remains required independently of VM isolation.

## Evidence

Inspected 4 November 2026. These primary sources support the mechanism/limits;
they are current upstream manuals, not evidence for a pinned Aetheri build:

- [QEMU security](https://www.qemu.org/docs/master/system/security.html): hardware
  virtualization/supported machines, TCG exclusion, untrusted guests and host
  resource overhead.
- [QEMU disk images](https://www.qemu.org/docs/master/system/images.html): writable
  overlays, snapshot state and device limitations; snapshot mode can still be
  explicitly committed and must not serve as the sole base-protection mechanism.
- [libvirt confinement](https://www.libvirt.org/kbase/qemu-passthrough-security.html):
  explicit host-resource modelling, SELinux/DAC/namespaces and risks of bypassing
  modelled configuration through arbitrary QEMU arguments.
