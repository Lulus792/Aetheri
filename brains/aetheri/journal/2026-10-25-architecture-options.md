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

## Module and interface discussion

Prepared a single consolidated module/interface draft rather than stub code trees
or a separate daemon per logical feature. Covered trusted owners, process lifetimes,
private versus machine metadata, typed control versus bulk/display transport,
operation revisions/idempotency, crash recovery and representative user workflows.

Read API/database skill knowledge and official D-Bus/systemd/portal/SQLite/wlroots/
Linux descriptor references. No code, schema, dependency or hardware operation.
New detailed recommendations remain open for discussion; approved core architecture
is not reopened. Existing projects and migrations remain untouched.

Module-plan document checks passed: 30 UTF-8 files without BOM/NUL, 50 local
Markdown links, balanced code fences and valid brain metadata/project root.
Git diff --check passed. No runtime/API/security tests are implied by this check.

## Module approval and identity/lifecycle discussion

User accepted module/interface recommendations and requested identities/lifetimes.
Recorded acceptance separately from the new detailed contract. Prepared a single
identity/lifecycle document with logical/content/installation/generation/data/run/
operation identities, admission/leases, revision epochs, retries/expiry, cancellation,
restore and cleanup scenarios. No schema or runtime state was created.

Read RFC UUID, Linux process-descriptor, systemd and SQLite primary references.
The user also reports F: fully clear; only queried capacity, approximately 232.4 GiB.
All work remains under F:/Aetheri; no unrelated files or processes touched.

Identity-draft document validation passed: 31 UTF-8 files without BOM/NUL,
60 local Markdown links, balanced fences, valid brain metadata/project root and
all seven principal identity definitions present. Git diff --check passed.
This validates documents, not lifecycle execution, security or fault recovery.

## Identity approval and concrete SQLite/API discussion

The user accepted the identity/lifecycle recommendations. Updated current memory
to record that acceptance separately from the new schema/API recommendations.

Prepared three proposed SQL files, one D-Bus introspection XML and one standard-
library validator, explained in a single knowledge document. Eighteen distinct
table designs cover owner identity, exact package/activation relationships, private
data, launch admission and durable/delegated operation correlation. Fourteen methods
define typed previews, acceptance, status, launch and authorized event subscriptions.
Budgets and concrete signatures are proposals; nothing is installed or exposed.

Read official SQLite/D-Bus and Linux clock documentation. Chose DELETE + EXTRA
as the proposed first durability configuration after checking rollback-journal
last-commit behavior; in-memory fixtures cannot verify power-loss durability.

Executed validator with Python 3.12.14 / SQLite 3.53.1, exit 0:
374 assertions, including selected SQL rejection/acceptance cases and structural
XML checks for 14 methods; foreign_key_check and integrity_check pass for both
disposable catalogues. Most assertions are structural, not end-to-end scenarios.
No live D-Bus, filesystem publication/deletion, security, race, migration, VM or
hardware acceptance. Canonical encoding/test vectors, authenticated principals
and storage/admission/recovery integrations remain gates before implementation.

Document validation passed: 37 UTF-8 files without BOM/NUL, 77 local Markdown
links, balanced fences and valid six-field brain metadata/project root. Git diff
checks passed. New SQL/XML/Python sources use explicit LF rules for the Linux target.
Existing projects, migration processes and other drives remain untouched.
