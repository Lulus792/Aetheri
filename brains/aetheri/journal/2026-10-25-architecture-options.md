# Architecture alternatives — 25 October 2026

User confirmed suspend/resume and requested a detailed comparison/recommendation.
Updated requirements, decisions and open questions without approving an architecture
on the user's behalf.

Prepared one consolidated comparison with three overall approaches and component
alternatives: display/compositor, init/session, deployments/updates, ownership/
sandboxing, metadata, plugins/shell and coordinated lock/suspend/resume.
Recommendations and specific new dependency candidates are explicitly provisional.

Inspected official Wayland/wlroots/libweston, systemd/logind/sleep, s6, RAUC,
Flatpak, kernel, SQLite and NVIDIA sources. Re-read the relevant NexaUI Linux
dispatcher/build definition without modifying that project or its active work.

No installation, runtime prototype, hardware operation or new dependency adoption.
Document checks passed with exit 0: 29 UTF-8 text/metadata files without BOM/NUL,
41 local Markdown links, valid six-field brain metadata/project root and balanced
architecture code fences. Git diff --check passed. No runtime tests are implied.
Next: explain alternatives, capture the user's architecture choices, then plan spikes.

## Subsequent user approval

The user approved the recommendations and explicitly chose B, wlroots, systemd
and SQLite. Updated current decisions, project, instructions, comparison status,
questions and README. Preserved alternatives as rationale. Versioned base/managed-app
separation is part of the approved recommendation; exact implementation choices
and releases remain open. No dependencies installed or code implemented.

Approval-update validation passed: 29 UTF-8 files, 41 local Markdown links,
valid brain metadata/root, balanced code fences and consistent selected component
names in the current entry documents. Git diff --check passed; no runtime tests.
