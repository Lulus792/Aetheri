# Original socket peer and system-service instance

10 November 2026. The new C17 service observer passes real ordinary-client/Root-
service cases, memory instrumentation and a targeted wrong-MainPID counterexample.
It is the service portion of login admission, not executable/configuration/policy
verification or an installed PAM/greeter/session implementation.

## Scope and implementation

Previous goal turn made progress and pushed4bee90c. The same restricted audio lab
UUID033ac49e-68bb-47a3-9c9c-26a812dc8300, QEMU5548 and
boot8b915c0c-ab36-4cce-a8a3-e84e1c12627c remain original, enforcing1. Original p0,
catalogues and quarantined execution remain closed. No USB-targeted build/flash.

`src/login/service.{h,c}` captures the Unix socket's kernel process pin, never a
fabricated bus peer or pidfd_open of an app-supplied PID. Independent expected unit,
UID/GID, current process label and connection socket label constrain admission.
Trusted system-bus credentials capture PID1 without augmentation; both PID1 and
peer pins remain held. Retaining PID1's actual FD, not only a stat record, keeps
that original observation alive. Bus/boot identity, current kernel IDs/label,
GetUnitByPIDFD unit/invocation, active state, current MainPID/no ControlPID/no job,
and exact cgroup path/inode agreement are checked twice. Later loss permanently
fences the handle. Ordinary connection construction consumes its borrowed peer
pin while the service handle remains independently alive through callbacks.

Only libsystemd/libselinux and existing connection/model foundations are used.
No new daemon/thread/timer, API/XML/SQL contract or privileged state writer is
introduced. The module's synchronous RPC cost/timeout belongs to the caller;
normal UI scheduling and complete deployment/source/greeter admission remain work.

## Failed observations and correction

The first Root fixture's positive create failed. A stale errno initially suggested
a permission failure; that interpretation was corrected after the actual return
was ESTALE. No permission, policy or credential check was weakened. Diagnostics
showed the socket context `system_u:system_r:unconfined_service_t:s0-s0:c0.c1023`
and current raw process context `system_u:system_r:unconfined_service_t:s0`.
Changing a single expected label could not satisfy both different facts. The
final API requires two independent exact expectations. Neither ignores MLS bits
nor substitutes UID/type/prefix matching. Normal cross-UID Root process-info
queries do work in this carrier; there is no demonstrated permission blocker.

Preserve live fixture1's defined120-second runtime expiry (failed/timeout) and
fixtures2–5's inspected explicit inert quit/held kernel-peer exit. Fixtures6–12
end normally. The early controller preparation collision never dispatched.
Fixture9 repeated the positive case because its wrapper did not actually switch
arguments; only fixture10 is accepted as the normal child-negative evidence.
The administrative test domain and world-connectable inert test socket are not
production greeter policy or deployment defaults. No PAM/account/profile is
installed, no real credential is supplied, no SELinux module or global package
is changed. Root fixture programs have no build RPATH, cleared environment,
empty capabilities/NNP and a protected system with one explicit output tree.

## Final native results

Release source `/var/tmp/a-login-cmake-2175fb289576` and memory source
`/var/tmp/a-login-cmake-9a9198c87a44` configure/build the actual service library and
explicit test executable; both pass the existing three login CTests without skips.
All six selected translation units carry actual memory instrumentation in the
memory build. Root server is an independently copied/hash-bound0555 program, not
the mutable user build output; readelf proves no RPATH/RUNPATH before UID0 use.

Fixture8: `aetheri-login-service-test-590a9175b440`, ordinary client exit0. Incorrect
unit/current label/socket label/UID/GID reject without consuming the caller FD;
wrong thread rejects, original terminal loss stays fenced. Service-only callback
composition with the C connection succeeds and closes locally without an auth
request or stopping the Root server.

Fixture10: `aetheri-login-service-test-0e122a2773a2`, child client exit0. The socket
creator is a Root child in the same unit/domain, while the original parent stays
MainPID. Constructor rejects EACCES. The inert child and its original waiting
parent then exit, with no normal application/session recovery inference.

Fixture11: `aetheri-login-service-test-dbbd0fcc5195`, same Root Release server with
the actual memory-built ordinary client, exit0, ASan/UBSan/leak enabled. Root never
executes the user-owned memory target or loads its SDK library prefix.

Counter source `/var/tmp/a-login-cmake-63563a642267` removes only the MainPID
comparison. Fixture12: `aetheri-login-service-test-0eaf885e4bf9` runs the unchanged
good Root child server and defective ordinary client. It admits the wrong actor,
so the exact `rejected` assertion fails, exit1 as expected; the client has already
sent the inert quit before reporting failure. Root parent/child end normally.
The counterexample is not installed or used as an admission implementation.

## Independent evidence and continuation

Export `/root/aetheri-login-service-evidence-75f0f214e8df49df9700db136fa9f952`
is8,071,586bytes, SHA256
`53f4e56c7ea535aa050e10222c7deb1a6fa5ba234447ff10f9c957016f7d98e6`.
Host `local/login-review-2026-11-10/service-audit-v1.json` verifies all2,432 regular
files including manifest, all1,502 final submitted source inputs, current source
bodies, exact mutant, actual memory command/client and the12 terminal fixtures.
The first retains its failed timeout; all later Root units end successfully.
No selected fixture/build code is mapped. Original RPM/udev693/invocation,
enforcing1 and tty1 are unchanged; no fixture PAM/local-seat session remains.

Next verify protected executable/configuration/unit sources and loaded policy,
bind the real initial connection and original greeter context, then exercise
actual PAM rejection/cancel and user-session transition. This does not establish
current executable identity merely from a service name or complete first-version
readiness. Preserve the four foreign UI-memory files, full OS goal, old quarantine
and NexaUI maintenance-only gate.
