# State

25 October 2026 — suspend confirmed and architecture alternatives prepared.

## Completed

- SecondBrain-style memory and English documentation maintained.
- Agreed foundations: C17, targeted Rust/adapters, existing development tools,
  glibc, retained technical backends and MIT; exact integration versions still open.
- Recorded original shell/language, custom editor/plugins/terminal sessions,
  browser goals, Physim future NexaUI migration and NexaCloud Desktop target.
- Component/file/process visibility and complete managed-state removal specified.
- F:/Aetheri is our workspace; other projects' active migrations/processes protected.
- Suspend/resume confirmed for laptop and later main-PC acceptance.
- Prepared [architecture alternatives](knowledge/architecture-options.md), comparing
  distribution-derived, controlled modular and deeper original userspace approaches,
  with compositor, service/session, update, sandbox, inventory and plugin tradeoffs.
- Recommendations remain proposals: modular original platform, wlroots-first
  compositor evaluation, selected systemd/session foundation, versioned read-only
  system deployments plus independent managed apps.

## Limits

Planning documentation only. No new runtime libraries selected/installed, VM created,
kernel built, app implemented, disk formatted or hardware accepted in this work.
Research reads live upstream docs, not an audited dependency graph or benchmark.

## Publication

Public planning repository: https://github.com/Lulus792/Aetheri.
The architecture discussion is explicitly a proposal; publishing it does not
approve its implementation choices. Historical publication evidence is retained
in the journal. No bootable OS release.

## Next

Discuss the proposed architecture with the user. Record selected boundaries before
pinning dependencies or running implementation spikes. Then define bounded acceptance
for package recovery, compositor/NexaUI interaction, VM boot and physical sleep.
