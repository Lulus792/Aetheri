# Installed login startup adapter

The [installed startup adapter](../../../src/login/installed_startup.c)
composes the fixed signed selection with the existing finite executable.
The build-selected source supplies the release key, release identifier and
mandatory platform gate. The adapter selects the fixed systemd units, backend
source order and handoff location. Both the platform gate and the selected
kernel policy must pass before the native flow can publish an observation.

The startup coordinator already holds the original backend and verifies its
namespace. It now accepts a one-time view hook between namespace entry and
source capture. The installed adapter uses it to open and retain a second
protected selection. The second selection must match the original signed
record digest. The original handle remains held until exit. The adapter
checks the policy through the live selinuxfs file, using signed size and
SHA-256 expectations. It performs the same policy check in both views and on
subsequent gate calls through publication.

Release `0f09d277759c` and ASan/UBSan `637e93d37fc7` each build the new adapter
and its native fixture, with 824 recorded source inputs per build. All eleven
selected CTests pass in each build. The coordinator model confirms the view
hook runs once before source capture, propagates failure, and stays uncalled
if entering the namespace fails. The existing finite entry checks also pass.

VM case `e9f64834b8fa` executes the real executable entry and adapter with a
test-only compiled key and platform callback. Its runner wrapper supplies
separate actual private mount views and the selected SELinux policy
`c93759a6…caf91f`. Both builds accept the same signed content in separate
files and refuse a different valid record under the same key and release.
They also refuse a signed policy digest that differs from the actual
kernel policy, and refuse when the platform callback fails. The real
systemd backend runner and handoff publication are exercised by the existing
independent fixtures. The new adapter test still needs a combined live login
using the installed units and the installed greeter provider.

The exact candidate module `a78021537a8a` was retired to the original policy
bytes. The VM remains enforcing, on VT1, with its original system paths.
