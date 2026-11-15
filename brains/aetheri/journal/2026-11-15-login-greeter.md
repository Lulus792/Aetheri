# Original greeter and real PAM conversation — 15 November 2026

`login/greeter.h` now supplies the self-process observer used with the original
socket service, protected sources and loaded/handoff guards. It retains its own
kernel pin and the immediate parent derived from that self observation. The
live Root parent's parent must be the original Root backend. Each use checks
original ancestry/cgroups, all UID/GID values, exact raw labels, absent self
capabilities and the actual selected greeter ELF. Loss permanently fences.
There is no caller-selected PID, numeric reopening of a socket peer, new
background process/thread, process control or inferred session authority.

The observer does not inspect Root-worker ELF, supplementary groups, greeter
session class or seat/device ownership. Independently admitted account, loaded
code, backend/PAM/SELinux policy and trusted host namespace views remain required.
The carrier's existing UID1000 test account is not a production greeter account.
See [the contract](../../../spec/login.md#original-greeter-process).

## Executed boundary

The same enforcing carrier stayed on boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, QEMU PID16628. Final Release source
`/var/tmp/a-login-cmake-81ccd1648d19` and ASan/UBSan source
`/var/tmp/a-login-cmake-5feaff8a87ab` build the actual libraries and explicit
greeter target. Both pass the three existing login CTests without skips.
An independent audit matches all766 submitted files in each source tree and
five currently selected code/build inputs, and checks actual sanitizer flags.

Final live fixtures are `/var/tmp/a-greeter-live-7b0d94d76a19` (Release) and
`/var/tmp/a-greeter-live-d695085f2907` (memory). The pinned actual greetd starts
the ordinary C test greeter through its real Root PAM worker. Independent Root
observations join the greeter's actual executable, UID/GID/capabilities and
parent to that worker's actual backend executable and original MainPID. Fresh
same-image/label/account child construction is rejected using newly created
source and bus guards, rather than merely an inherited-handle owner error.
Wrong UID, self label, worker label and selected executable are rejected.
Wrong-thread use and fork-inherited use/close also exercise ownership.

The administrative publisher checks the current backend/config and creates the
protected296-byte observation without replacement. It uses the held original
backend mount namespace, independently compared with the greeter namespace,
while retaining a live authenticated original socket pin. The actual greeter
consumes the observation and composes both guards into the native connection.
It starts a real `pam_unix` secret prompt and receives an explicit Cancel ACK.
It then creates another prompt, sends an empty answer for the existing locked
account and receives an authentication error (`pam_authenticate: AUTH_ERR`).
No real password was used or logged, and no user session was started. Each final
case makes two create requests and one empty response. Earlier host summaries
that say `credential_messages=1` describe an inaccurate summary counter, not a
wire capture; retained final summaries report these separate counts.

The next Cancel gets an error from the pinned backend after the authentication
worker exits. The client correctly fences UNKNOWN and rejects another Begin.
Its model was not weakened to label that error a confirmed cancellation.
The backend's `Context::cancel` removes its configuring slot and calls
`Session::cancel`, which still sends to the ended worker socket. Normal retry
handling needs a reviewed backend change and original-worker end evidence.
A Cancel ACK in the earlier live-prompt test is protocol acknowledgement,
not a claim of worker/session/descendant drain.

After the protocol tests the controller changes the original greeter's mode.
The observation fences and stays fenced after restoration. All owned handles
are closed. The memory greeter explicitly runs the recoverable LSan check
before its DONE marker and before controller shutdown; zero leaks are reported.
ASan/UBSan produce no diagnostic. The fixture holds the greeter until exact
original service retirement, avoiding an automatic replacement during the test.

Static analysis passes the actual `greeter.c`. A separate ordinary test binary
removes only its actual self-image comparison. Actual greetd fixture
`/var/tmp/a-greeter-live-7be976c895e9` then fails the wrong-program assertion
at line157, before READY. The Root backend and publisher remain the good code.
The counter source/build is `/var/tmp/a-greeter-counter-b4aebf1e15b4`; it is not
installed. Windows contract validation passes374 assertions/14 API methods.

## Preserved failed evidence and causes

- The first compile lacked `_GNU_SOURCE`; the missing gettid/strnlen declarations
  were corrected. The analyzer later identified a possible uninitialized-info
  path through an opaque syscall error. All observation structures now start
  zeroed and invalid fields still reject; analyzer passes without suppression.
- greetd supplies the greeter's PAM environment. Its service loader environment
  was not inherited. The test command now explicitly selects the protected
  reviewed crypto library after account change. Memory SDK DSOs are selected
  only for the ordinary test greeter, never Root backend or publisher.
- Actual in-service SO_PEERSEC is the exact `unconfined_service_t:s0` context;
  earlier external test clients observed the same type with a full MLS range.
  These are separate exact expectations. No prefix/range normalization or
  relaxation of the production service observer was added.
- A host-namespace publication did not match the private service namespace's
  source fingerprint. Publication now uses the held original namespace. The
  source/mount checks remain intact.
- The first protocol fixture tried Cancel while IDLE. The model correctly
  rejected it. The real prompt/cancel sequence replaces that incorrect test.
- Initial PAM-deny tests exposed the failed-worker cancellation error. Early
  `pam_unix` tests then produced AUTHINFO_UNAVAIL: carrier `/etc/shadow` has
  mode000 and the bounded Root fixture lacked DAC read/search capability.
  Only the disposable Root backend/worker profile adds CAP_DAC_READ_SEARCH
  (effective set0xe5 instead of0xe1); actual AUTH_ERR is then observed. Greeter
  capabilities remain empty. No shadow permissions, account, global PAM file or
  production policy were changed. Production shadow access still requires its
  reviewed MAC/capability policy.
- The first final audit compared different RPM query formats. The corrected
  query matches the preserved baseline; runtime tests were not replayed for it.

All13 live fixtures, including failed and counter cases, are terminal. Original
unit fragments are archived, cgroups empty/removed, sockets absent and no selected
runtime mappings remain. The final independent audit confirms unchanged RPM
inventory, no global fixture PAM files, enforcing1 and active tty1. The four
foreign UI-memory files retain their original hashes. Receipts and primary
kernel-source copies remain under `local/login-review-2026-11-10`, using
`greeter-` prefixes; audit3 is the final audit.

Next fix the backend's failed-conversation cancellation/retry with real worker
lifetime evidence, then complete protected startup coordination, dedicated
greeter/account/effective PAM/SELinux/logind policy and successful authentication
and graphical user-session transitions. This is explicit development integration,
not an installed graphical login or production admission. Preserve the original
quarantine, full OS scope, first-version USB gate and NexaUI maintenance-only state.
