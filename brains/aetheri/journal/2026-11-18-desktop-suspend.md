# Desktop suspend in the installed VM

The installed Sound image contains logind and the power-switch udev rule.
It lacks `sleep.target`, `suspend.target`, `systemd-suspend.service` and
`systemd-sleep`. The pinned Linux 6.19.10 kernel includes suspend and ACPI
sleep support. Fedora's signed `systemd-udev-259.10-1.fc44.x86_64` package
contains all four files. The development carrier runs systemd 259.5.

## Staging and image

The offline stager reuses the pinned Fedora RPM and checks each selected
member's bytes and mode. Its receipt records the source package, license,
ownership and file contexts for image assembly. The exact selected
systemd, logind, shared library and LGPL notice match the existing base
manifests. An RPM changed by one byte and an image projection with a
changed PID 1 each fail before creation of their output tree.

The first archive parse refused an unrelated slice name containing a
literal `\x2d` escape. The reader now supports an exact member selection.
The parser still bounds the entire archive and validates selected names
and multiplicity. The native suite passes all eight input tests. On
Windows, seven input tests pass and the Linux memfd test is skipped. The
two desktop payload suites pass 15 tests.

The first private image composition exposed a readback limitation in
`debugfs ea_list`. It printed the longer helper context's length. The
corrected assembly reads each context directly and compares its full
bytes. The four changes were applied in a private mount namespace to a
copy of the signed Sound image's root. The copy passes `e2fsck -fn`.
Its root SHA-256 is
`1d406b91d857262fc68f2d43aff42cc519b517a81a48cb79403674668a4de164`.
The VM media SHA-256 is
`ecdc81b5b277ef5e86ee0f97b285b4137f5d330aa1a60498fc64306900a41276`.
The login selection retains SHA-256
`11bbfde9798e4c45a122966e59fe76ce49b11e70e74436c616ef9f2deff69fa6`.

## VM observations

WHPX case `3658ab4d` reaches the enrolled user desktop. An ordinary virtual
sleep key reaches logind, which starts suspend and freezes the original
user scope. Linux reports `PM: suspend entry (deep)`, and QMP reports
`suspended`. The original QMP endpoint accepts the wake request. WHPX
logs `Unexpected VP exit code 4` and the kernel resume observation times
out. The controller retires the original QEMU at exit zero and checks
the boot media hash again.

Software-emulation case `5f64fee7` reaches the same desktop with Settings
open. Focus moves past Sound before the page opens. This case ends before
the sleep key. The next controller waits for a changed, settled focus before
taking action.

TCG case `34ee4fad` reaches ACPI S3 through the same sleep key. Its original
QMP connection observes `suspended`, requests wake and observes `running`.
The guest serial output records the kernel's deep suspend entry and exit.
Session owner PID 254 keeps Settings PID 267 and the Sound mediator PID 268.
After wake, a keyboard action changes the live output from 39% to 38%.
Settings also confirms mute. Both child processes retire at zero when the
window closes. Normal logout closes PAM and shows a replacement greeter,
then an ACPI power button request powers down the guest. The media hash
matches its composition receipt. [QEMU wake contract](https://github.com/qemu/qemu/blob/v11.1.0/qapi/machine.json#L365)

TCG case `b0cbc1e5` repeats S3 twice in one login with the original session
owner PID 255, Settings PID 268 and mediator PID 269. The kernel and QMP
confirm both transitions. Settings confirms 38% after the first wake and
mute after the second. Both children close normally. Logout, PAM closure,
replacement greeter, power-down and media readback all pass. All four
QEMU cases are terminal.

The tests cover a single virtual display and virtual Intel HDA. The VM
returns to the active desktop. Release acceptance requires a trusted
lock before sleep, authentication after wake and device recovery on the
HP laptop and workstation. The VM payload stays exclusive to test images.

The composition receipts, controllers, serial traces and screenshots are
retained in `local/login-review-2026-11-10/desktop-suspend-2026-11-18-evidence.tar.gz`
with SHA-256
`485ea491ac8e61a5a079a7690f318725758431de6ae258eef02badc1a092e403`.
