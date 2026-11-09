# Private Mesa/DRI runtime — 10 November 2026

Continue the full current desktop rebuild after the SDL/text SDK. The preceding
status answer did not advance implementation. This turn authenticates, prepares
and exercises the actual renderer. No USB build/flash, P0 restart, normal runtime
promotion or NexaUI handoff occurs.

## Inputs and first failure

The existing UI/graphics pins provide 16 signed binary RPMs. Header queries are
retained under `/root/aetheri-mesa-review-ac75b8aa1194462692a68e88478dd60b` and
`/root/aetheri-expat-review-97e1618827534455ad82793811b4ac4e`.
The additional original signed Mesa source RPM is retained at
`/root/aetheri-mesa-source-review-ab4ae75b39cb4d38ae3bb7dbb418c735`.
It is source/notice data only: SOURCEPACKAGE is one and i686 is not a target ABI.
The profile also reuses the already reviewed 2.4.134 libdrm source archive.

The first controller exits one at preparation after its 14 controls pass.
Source `/run/a-mesa-747266ae5349`; no output runtime directory was created.
The new reader's 4-KiB trailer cap rejected legitimate rpm2cpio zero padding.
An independent read of all 16 original packages observes 10-KiB output blocks;
tails range from 348 to 9,880 bytes, all zero. The separate diagnostic is retained
at `/root/aetheri-mesa-tail-e29d3279af57416e9f3230be4b27226f`.
Correct the separate reader to a maximum 10,239 zero bytes and retain the short-
read/nonzero/excess rejection tests. Do not change older SDK readers' bounds.

The original sealed-input cache is reused as input data after fresh checks;
failed outputs are never adopted. All earlier logs/source remain unchanged.

## Successful preparation and rendering

Original host controller **43654 exits zero** for native controls, preparation
and actual draw. Source `/run/a-mesa-c5d3c88f8995`;
runtime `/home/aetheri/deps/mesa-runtime-c5d3c88f8995`;
initial proof `/home/aetheri/deps/mesa-proof-c5d3c88f8995`.
The 52 original regular inputs total 277,258,995 bytes; there are 86 original
aliases and a separate profile receipt. No package is installed. This is private
development storage, including a 69,191,486-byte source RPM, not final image size.

The final public probe adds an actual no-draw counter. Controller exits zero:
source `/run/a-mesa-counter-7926c3a6615d`;
work `/home/aetheri/deps/mesa-counter-proof-7926c3a6615d`.
Four commands have exits `0,0,0,1`: compile, real draw, counter compile, expected
counter rejection. The rejection is specifically the pixel-count assertion.

Actual output:

```text
EGL 1.5 Mesa Project
GL OpenGL ES 3.2 Mesa 26.2.5
RENDERER llvmpipe (LLVM 22.1.8, 256 bits)
PIXELS red=1352 blue=2744 total=4096
GRAPHICS_NATIVE_OK
```

The test compiles/links shaders, uploads vertices, draws, reads all 4,096 pixels,
checks inside/outside samples and destroys/releases its context/surface/thread.
Its 40 actual filesystem mappings bind the selected renderer, dispatch,
Wayland/FFI, private C++ and explicit native base. Both shader outputs must be
exact red/blue/opaque; merely loading libraries fails the counter. This is
surfaceless software rendering, not a Wayland/DRM/physical display test.

## Independent verification

Host and native have 14 graphics controls; the final host combined graphics,
common/SDL SDK and toolchain regression suite has **35 passing tests**.
All 400 exported regular files and 238 submitted source inputs match. Every
one of the 52 runtime bodies, all 86 aliases, 18 original package/notice inputs
and all 40 mapped files are independently checked. The current public preparer,
controls and final probe match their actual native source bytes.

Export: `/root/aetheri-mesa-evidence-b01946deea6b482c84856a722716392f/export.tar.gz`.
Host: `local/full-desktop-build-2026-11-09/mesa-evidence-v1.tar.gz`.
Size **212,184,251 bytes**;
SHA-256 **ec2138f1c4b7b2a5ad96591bc8917cbc3f8be22c429b87a29b94d959b5d2c1ec**.
Receipt: `mesa-evidence-verified-v1.json` in that local directory. The archive
also retains duplicate process-map library evidence; its size is not image size.

An independent same-boot audit verifies unchanged RPM inventory, enforcing=1 and
udev PID 693/invocation `1f3734ab799e47ab91ea4d27abbac917`. No selected runtime or
probe is mapped after original controller completion. This is not a general
cgroup/lease drain proof. The restricted audio lab remains the original UUID
033ac49e-68bb-47a3-9c9c-26a812dc8300, QEMU 5548, boot
8b915c0c-ab36-4cce-a8a3-e84e1c12627c; revalidate before future effects.

## Continue

Build the genuine current wlroots SDK, including matching generated libinput
1.32 metadata/headers, then rebuild the current compositor/apps and run normal
GUI/native regressions with this actual renderer. Keep old installed pins and
the quarantined execution fixture separate. The full first-version gate, physical
hardware acceptance, normal app/device ownership, browser, updates/recovery and
experiment desktop remain incomplete. The overall OS goal stays active.
