# Login handoff bound to live SELinux policy

Both installed adapters hold the enforcing kernel's selinuxfs status page.
The status reader verifies the filesystem and the versioned status prefix,
requires a stable even sequence, and fences any observed change. The
startup adapter also checks the signed digest of the loaded policy between
two observations of that sequence. The greeter maps the status page and
keeps the full policy read unavailable to its domain.
[Kernel selinuxfs source](https://raw.githubusercontent.com/torvalds/linux/master/security/selinux/selinuxfs.c)
[SELinux status implementation](https://raw.githubusercontent.com/SELinuxProject/selinux/main/libselinux/src/sestatus.c)

The [version-2 handoff](../../../spec/login.md#protected-observation-handoff)
uses the same protected file dimensions as version 1, a distinct magic and
a context digest that includes the observed status sequence. The loaded
guard checks its policy observation before and after the original
peer/source checks. The installed greeter requires version 2 and the
same kernel sequence. The explicit custom profile can still use version 1.

Final Release `5c3a8a1c9bed` and ASan/UBSan `60ef840e808a` pass all eleven
selected CTests with 828 recorded inputs each. Loaded candidate policy
`81f25eededb9` passes 233 core and 277 graphical decisions, including the
read-only mapping grant and the greeter's full-policy read denial.
Real publisher/ordinary-reader cases `4e58e8ef7a78` and `4463820c49d0`
exercise version 2 in the final Release and memory builds. A kernel reload
retains the exact effective policy bytes, enforcing mode, protected file
identity and record bytes. The original reader fences and a new bound reader
rejects the published record. An unbound reader rejects version 2.
Compatibility case `db599f715d8d` uses the preceding otherwise compatible
Release build to publish and consume version 1 and prove a bound reader
rejects it. Earlier version-2 evidence `6dd5846455c6` and `ad221942fe36`
remains separate.

The first graphical cases `3210f755b462` and `42153f83e4cb` stopped before
publication. Diagnostic case `874d0dc15ee2` failed to build its test wrapper.
After correcting the test include, `dce445cf6f9b` shows successful greeter
loaded-unit observation followed by `ECHILD` from the policy bind. The
graphical host had changed the callback context to its own structure. It
now forwards the policy callback with the original provider context, like
the existing host admission callback. The failed fixtures remain available.

Release graphical case `ea7849275d4b` uses the real typed startup and
greeter processes and publishes `AETHLGH2`. The keyboard reaches the actual
PAM prompt, Start acknowledgement and visible user desktop. Original entry
and worker end, PAM closes, the original runtime retires, and the replacement
greeter returns and stops at zero. These are private carrier deployment
providers and the selected scratch mount profile. The installed-image
account, PAM and runtime gates still need their own integration.

The candidate module was retired to the original carrier policy, enforcing
mode and VT1. Continue with the signed PAM profiles and the remaining live
platform gates before a full fixed-path image boot. The first-version USB
gate applies.
