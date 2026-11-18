# Working on Aetheri

For software development, read `D:/Skills/skills/AGENTS.md` and the relevant
specialist skills. Start with [project requirements](brains/aetheri/PROJECT.md),
[current state](brains/aetheri/STATE.md), [decisions](brains/aetheri/DECISIONS.md)
and [open questions](brains/aetheri/QUESTIONS.md). Read the relevant contract and
latest dated test record before changing a subsystem.

- Write project documentation in English. Conversation can remain German.
- Keep the approved modular Linux x86-64/UEFI architecture and dependency
  boundary. Record new dependency selections, pins, licenses and the checks that
  justify them. See the [architecture decisions](brains/aetheri/DECISIONS.md)
  and [contracts](spec/README.md).
- Keep versioned compatibility contracts and distinguish observed results from
  planned behavior. Preserve user work, quarantined state and original evidence
  for pending or uncertain operations. The subsystem spec defines its ownership,
  lifetime and recovery guarantees.
- The next USB-targeted build and flash require the agreed first version to be
  implemented and verified. Preserve the existing stick and its test documents.
  Continue ordinary development and VM verification.
  [USB decision](brains/aetheri/DECISIONS.md#hold-the-next-usb-build-until-first-version-readiness--9-november-2026)
- Reuse selected read-only VM bases and small overlays. When a run ends, keep the
  evidence needed to verify the result and retire unreferenced generated disk
  copies. Follow the [storage guide](spec/development-storage.md).
- Maintain the existing NexaUI catalogue when requirements change. The handoff
  has been delivered. Follow the [coordination rule](brains/aetheri/knowledge/nexaui.md#maintenance-rule--confirmed-5-november-2026).

Use `STATE.md` for the current handoff, the dated journal for test results and the
subsystem spec for its contract. Update the existing entry when a result changes.
The [previous handoff](https://github.com/Lulus792/Aetheri/blob/3ca53b3568be75463f64c559049fb4348d091172/AGENTS.md)
contains the original checkpoint history.
