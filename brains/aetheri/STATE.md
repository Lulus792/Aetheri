# State

25 October 2026 — architecture selected; implementation planning remains.

## Confirmed

- Linux x86-64/UEFI, C17 primary, targeted Rust/adapters, existing development tools,
  glibc and MIT; suspend/resume required.
- Controlled modular system (B), original wlroots-based Wayland compositor,
  selected systemd/session/sleep foundation and SQLite component metadata.
- Versioned read-only system base plus separately managed apps/tools/shared runtimes,
  controlled private state and authoritative ownership/permission inventory.
- Original shell/language, custom terminal/editor/plugins, browser without clutter,
  Physim future NexaUI migration and NexaCloud Desktop target.
- Laptop first; later RTX 2080 Ti main PC with separate hardware acceptance.
- Use F:/Aetheri. Other projects' files, processes and active migrations remain protected.

## Completed

Maintained English project memory and comparison rationale. Recorded explicit
architecture approval and removed stale proposal-only status from current guidance.
The architecture discussion remains available to explain alternatives and consequences.

## Limits

No component installed, dependency release pinned, VM created, kernel built,
app implemented or hardware accepted in this work. SQLite transaction support does
not by itself implement cross-filesystem/service/boot recovery.
Versions, sandbox/update/boot mechanisms, filesystem layout, browser embedding,
plugin interfaces and shell syntax still need concrete specifications.

## Publication

Public planning repository: https://github.com/Lulus792/Aetheri.
Current decisions are architecture selections, not a bootable OS release or test result.

## Next

Prepare the detailed module/interface and dependency/build plan within the selected
architecture, then bounded lifecycle/recovery and compositor/NexaUI prototypes,
a coherent QEMU boot image, and physical suspend/display tests.
Discuss remaining product choices when concrete alternatives are ready.
