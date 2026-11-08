# First laptop test

Use the verified Intenso Speed Line on the spare HP EliteBook 840 G4. This first
image starts the Aetheri development desktop directly from USB. It has persistent
documents, Terminal, Files and Settings. It does not install Aetheri on the laptop.

Wait for the flash result before unplugging the stick. A running write or an
incomplete readback is not a ready image. Windows may offer to format unfamiliar
Linux partitions afterward: cancel that offer. Use Windows' safe-removal action
for the Intenso before disconnecting it.

## Start from USB

1. Shut down the laptop, connect its charger and insert the Intenso directly into
   a USB port. Start with the built-in screen and keyboard; a wired mouse is useful
   if the touchpad does not work yet.
2. Turn it on and open the one-time boot menu. HP documents F9 for its business
   notebook family; some firmware first shows an Esc startup menu. Choose the
   **UEFI** entry for the Intenso or USB drive. Exact labels depend on the firmware.
3. Wait for the Aetheri desktop. Photograph any error before restarting if it
   stops, goes black or switches itself off. Record whether the failure happened
   before or after a Linux boot message appeared.

The current EFI executable is unsigned. Firmware enforcing Secure Boot can reject
it before Linux starts. This image needs Secure Boot disabled on the test laptop
or a separate trusted signing setup; the latter is not implemented. Record the
original firmware setting before any temporary change. Do not erase firmware keys,
enable legacy/CSM boot or update firmware as part of this test. If the laptop has
an encrypted installed OS, have its recovery method available before changing its
boot-security settings. No change to the development PC's settings is required.

## Try the desktop

Open Terminal from the dock, or press Super+Space, type `terminal` and press Enter.
Super means the Windows-logo key. Use Super+Up to maximize an app and Super+Down
to restore it. Search also finds Files and Settings.

This image uses the **US keyboard layout**. German and English interface labels
are separate from that layout: F6 changes supported app labels, not the keyboard
mapping. On a German physical keyboard, Y/Z and punctuation can therefore differ
from the printed keys. A normal layout selector remains work.

In Terminal, enter one command per line:

```text
pwd
printf 'hello from aetheri\n' > laptop-note.txt
cat laptop-note.txt
```

The starting folder is `/home/aetheri/Documents`. Open Files and check that
`laptop-note.txt` appears. Opening a text file there uses the separate terminal
editor. Save or cancel that editor's close prompt before shutting down.

Terminal supports Ctrl+Shift+T for a new tab, Ctrl+Shift+D for a right split and
Ctrl+Shift+E for a split below. Alt+arrows select a nearby pane; Alt+Shift+arrows
resize a split. Each pane has its own shell. Close additional panes with
Ctrl+Shift+W. These sessions are not restored after reboot.

Settings can show display observations and change its own appearance and labels.
It does not yet apply system display modes, configure WLAN/Bluetooth/audio or
persist a system theme. Test basic pointer motion, typing, window movement and
whether text remains readable at the laptop's actual resolution. External
displays, suspend, audio and wireless need separate implementation and tests.

## Shut down, then check persistence

Finish document save prompts and foreground jobs first. Close Terminal with
Ctrl+Shift+Q. Open desktop search with Super+Space, then use Ctrl+Q to close the
development desktop in order. That action closes the desktop; it is not itself
a power-off command.

Briefly press the laptop's power button to request normal system shutdown. The
corresponding virtual ACPI shutdown path passes the image tests; the physical
button still needs testing. Wait until the laptop is off before removing USB.
If it fails to shut down, record that separately. A forced power cut is not a
successful shutdown or filesystem test.

Boot the same stick again and run:

```text
cat laptop-note.txt
```

The expected line is `hello from aetheri`. Do not reflash between the two starts:
reflashing replaces the writable data as well as the system.

## Capture a useful report

Report the last visible stage, the screen resolution if known, keyboard/pointer
behavior, whether the document survived, and shutdown behavior. Include any exact
error or photograph. A successful boot is useful evidence; it does not establish
all hardware support or everyday-system readiness.

For a local text report, choose an unused folder name, then run these commands
in Terminal. If `laptop-test-1` already exists, use a new name in every line.

```text
mkdir laptop-test-1
uname -a > laptop-test-1/kernel.txt
date -u > laptop-test-1/time.txt
cat /proc/sys/kernel/random/boot_id > laptop-test-1/boot-id.txt
cat /proc/self/status > laptop-test-1/process.txt
cat /proc/mounts > laptop-test-1/mounts.txt
cat /sys/class/tty/console/active > laptop-test-1/console.txt
ls /sys/class/drm > laptop-test-1/display-nodes.txt
systemctl show aetheri-graphics.service aetheri-usb-data-ready.service aetheri-device-baseline.service aetheri-device-denied.service --property=Id,ActiveState,Result,ExecMainStatus > laptop-test-1/services.txt
```

These are ordinary files on the stick. The current diagnostic journal is volatile;
rebooting loses it. Keep an on-screen error before restarting. There is no network
upload or automatic hardware report, and Windows does not normally read the ext4
data partition. Keep the stick intact for later read-only collection.

## Evidence and scope

The exact image passes two no-serial-port VM boots, document persistence, normal
guest shutdown and independent filesystem inspection. The [laptop profile](laptop-probe.md)
and [executed record](../brains/aetheri/journal/2026-11-08-laptop-image.md) identify
the bytes and remaining limits. [Flash status](../brains/aetheri/STATE.md) must
separately prove the physical write and full readback.

This development account autostarts without a login screen. The root is read-only
and documents live on the USB data partition, but production app admission,
encryption and sandboxing are unfinished. Use test documents. The installer,
browser, updates, normal accounts and complete hardware support remain work.

HP's [business-PC UEFI guide](https://h10032.www1.hp.com/ctg/Manual/c03857419.pdf)
documents the F9 family convention and Secure Boot behavior; it is an older
family reference, not proof of this laptop's particular firmware screens.
