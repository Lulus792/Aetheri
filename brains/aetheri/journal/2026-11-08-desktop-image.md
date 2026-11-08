# Desktop and apps in the own boot image

8 November 2026. The full OS goal remains active. The existing graphical stack
now runs in the independently assembled image, with persistent user documents
and normal shutdown. No subagents or CUA were used. The C/build/Linux and
dependency skills were applied. The NexaUI handoff stays closed.

Subsequent USB-runtime audit: this image's serial log explicitly reports missing
libbpf and disabled cgroup BPF features. The VT unit's configured DevicePolicy
is not evidence of an enforced device filter. Its capability configuration and
the actual GUI cap-zero/NNP evidence are separate. The USB desktop must add the
reviewed dynamic backend and a real device-access negative before acceptance.
See the [USB-root record](2026-11-08-usb-root.md). Earlier console-output failure
does not independently prove cgroup-device denial; the journal-output correction
only proves that the fixed service can start with those settings.

## Result and boundary

The [graphical profile](../../../spec/desktop-image.md) builds nine existing app
targets plus an original C17 desktop owner. It starts a normal logind session,
user manager and buses under a fixed UID/GID1000 account with NoNewPrivileges
and zero effective capabilities. Only the separate root oneshot changes VT.
The broker retains its vendor PrivateDevices sandbox. Device events, the bochs
module, selected rules and volatile16MiB journal supply the virtual-display path.

The768MiB system file contains a512MiB read-only root. The separate256MiB data
file backs `/var` and `/home`; ordinary documents belong to UID1000. The earlier
counter/diagnostic-poweroff services are removed from this profile. Its1426
pinned runtime files total258644882 bytes, with365 aliases and76 existing RPM
owners plus SDK provenance/licence files. Mesa/LLVM dominate the software-renderer
closure. NexaUI remains the reviewed0.1.23/97a4264; no toolkit update is implied.

Compilation and image creation run as UID1000 in a fresh root-provisioned private
2GiB workspace and256MiB temporary mount, after a3GiB available-memory check.
No global package, policy, carrier source/service or attached disk is changed.
The carrier remains Enforcing on its original boot. That is not production MAC
policy in the new image, which remains a development autostart fixture.

## Final canonical-source build

Buildv11 transfers exact Git index bytes, accepting working-copy differences
only when they are line endings. All477 bundled non-document inputs and the
image's native source/pin digests match the staged index. This avoids publishing
a different source encoding from the actual tested build; no unrelated working
files are rewritten or staged. Buildv10 is a successful retained intermediate.

Final token `dab2f78611674cc8a05339479d3efd8e`, helper1097622: terminal0,
123 independently verified export members before exact namespace retirement.
Source SHA256:
`4404959e4b27b11c09c7a50311bfc24c611ed52e4aca358e446798c953fd5c87`.
Export184887217 bytes, SHA256:
`45755418c750cdb9c4812272d521fa78631152e43107e95835691b5e8ef672bc`.
This build also runs the final native controller6/payload7 contracts, native
owner Release/ASan/UBSan and strict analyzer compilation, all successfully.

Final sequencev12 boots imagev11 twice, repeats actual app input/document reads
and normal unmount/poweroff, with native PNGs inspected again. Original QEMU
PIDs68112/68380 finish0 in28.781s/27.359s. Boot IDs are
`b9c2659a-e912-4819-b01a-763678625401` and
`e6e42b09-5629-4f16-9c91-403d329fcb9f`. Final data SHA256:
`c7735598b78dbe69ca89b629c574790ebe45d30e1914d9ae8f7946ea98b1fc73`.
The controller remains `fb0f98fb87be20914600a232321eb09898599ff5877d9e026e744b1c53727aa7`.

Independent inspectionv12, token `9a4e5244829946348496ec215c7051f1`, helper1099140,
finishes0. It repeats read-only fsck/debugfs, additionally requires a regular
inode, and verifies the exact14 bytes, UID/GID1000/mode0600/one link and unchanged
whole-image hash. Final native controller6/payload7 pass. Its13 evidence members
are verified before retirement; export3691 bytes, SHA256:
`244f1b366ef779e3aa986c37eb3a223e9ab8ae846304c6b0f990b285221d44cd`.

## Retained first functional build and regressions

First functional native buildv9, token `4136a11080a64df887740572641cf0d5`, helper1094396:
terminal0,122 independently checked export members, then exact namespace
retirement. Source SHA256:
`97e992a5f94c60121fbdc810a00d2da9c854e18ae30d347da6af81410e9d9f09`.
Export184886890 bytes, SHA256:
`05fe32d071c121ecfbc1c74e9141cbeb5bffd9a89aace45a3902c30e7f4c4570`.
The complete2067-inode root is owner-checked and passes e2fsck before packaging.
Its source archive retains the original working-copy bytes. The final canonical
build above replaces its mixed line endings without changing the app behavior.
The later host controller has separate sequence and inspection pins.

Native tests pass Release and ASan/UBSan. They actually reject/close transferred
FDs without leaks, reject malformed packets, prevent target execution on failed
pidfd capture and close unrelated inherited FDs at target exec. The owner compiles
with strict C17 warnings-as-errors and `-fanalyzer`, without warning suppression.
Seven payload and six controller cases pass on Windows and Linux. The52 earlier
host VM-tool cases pass after the unchanged AF_UNIX adapter extraction; live
original-carrier status still verifies. Five boot/five root/seven UEFI/six data
regression groups pass on Windows in their recorded scopes. Initial root/UEFI
CLI invocations without the required image argument failed before tests; the
properly supplied original images pass. This is not a newly run full lifecycle suite.

Two retained sequences each exercise two actual cold boots of imagev9. The second
sequencev11 uses WHPX/q35, two CPUs,1536MiB RAM, standard VGA/PS2, fresh NVRAM,
read-only system media and one new retained data copy. There is no network, shared
folder or physical disk. Local AF_UNIX QMP checks the private VM UUID before each
once-only journaled input. Actual keyboard input opens/closes Terminal, Files
and Settings. First boot writes `aetheri-image\n`; the second visibly reads it.
Inspected native1280x800 PNGs show the desktop, terminal, file and Settings views.
Native owner exit/EOF acknowledgement precedes subsequent app use. Both runs
close the desktop, accept the ACPI power button and complete normal systemd
unmount/poweroff, original QEMU exit0 and kernel S5.

- Sequencev11 boot IDs: `77e09d77-9441-4e59-b8d4-860cb35d7866` and
  `c32be916-f89e-4729-adb5-a8a1fe54fd45`.
- Original QEMU PIDs46800/52872; final elapsed28.594s/24.531s, both terminal0.
- Sequencev11 controller SHA256:
  `fb0f98fb87be20914600a232321eb09898599ff5877d9e026e744b1c53727aa7`.
- Sequencev11 data SHA256:
  `32094f04993c12ccc3398ac9c031f15158507bc682bfb36d63c9e04c6348d3e2`.

Independent native inspectionv11, token `4f9d4a29c8c2477e9eb219ee45223307`,
helper1095891: terminal0,13 verified export members before retirement. Read-only
e2fsck/debugfs requires a clean filesystem, exact14 document bytes, UID/GID1000,
mode0600 and one link; the whole input hash remains unchanged. Native payload7
and controller6 cases pass again with the final controller source. Export SHA256:
`8288de9774343b5695edc7dfaa313b00768b3972a432b46c545bf206f5b3a123`.
The earlier accepted sequencev10 independently retains the original13-byte
unterminated document and its own successful read-only inspection; it is not
overwritten. All receipts/images/logs remain under
`local/desktop-image-2026-11-08/`, outside Git binary payloads.

## Preserved failures and limits

Earlier build attempts lack an enabled shell target/protocol source or trigger
an analyzer diagnostic for discarded dup2 return values. The final code retains
and checks each descriptor result; a controlled compiler comparison verifies the
correction without suppressing analysis. Earlier actual boots expose distinct
integration omissions: broker stdout inside PrivateDevices, inactive VT, restricted
VT-service console stdout, missing replay of existing input devices and absent
glibc C.UTF-8 data. Each correction preserves the relevant device/seat checks.
Sequencev9 reaches all app views but its controller expects nonexistent Settings
READY/CLOSED strings; the corrected controller uses its real SETTLED frame and
owner exit acknowledgement. Those attempts are failed/unknown and retained,
never restarted with their original data or mistaken for accepted sequences.

The13-byte experiment also exposes a shell presentation limitation: a following
line-editor prompt clears unterminated command output. The final text fixture
has a newline so its cold-boot read is independently visible. That limitation
remains a separate shell issue, not a storage loss or a reason to alter NexaUI.

The protected-state audit preserves445 normal carrier sources,31 frozen SQL/XML,
19 quarantine hashes,52 P0 files, original authority/CLOSED history, old SDK and
foreign host planning/design files. No original-carrier reboot, foreign-process
termination, quota change, global installation or physical-device write occurs.
The new fixed L3 process owner is not production launch admission, lease closure,
SELinux isolation, complete holder drain/recovery/removal or a normal login.

## Next boundary and USB permission

The user confirms the Intenso Speed Line on G:,31,457,280,000 bytes, and explicitly
authorizes flashing once the laptop-test image is ready. Reidentify the physical
device immediately before writing; no repeat question or wait for the user's
later hardware test is needed. Internal disks are outside that permission.
No USB write has yet occurred. The current virtio/bochs test profile requires
early USB-root modules/device selection, laptop DRM/input/firmware and a
single-medium writable-data layout before it can be that test image. Test the
removable profile in the VM first. Continue development after flashing.

Full OS/apps/browser, normal lifecycle/removal, installer/LUKS, updates/rollback,
physical hardware/suspend and the persistent resettable experiment desktop remain
required. This checkpoint does not complete the full goal.
