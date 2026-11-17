# Session-owned runtime cleanup

The original confined login case `f5ed8d7d307e` exposed an empty
`aetheri-dNUZ4h/cache` under a shared logind user runtime after logout.
The owner created both directories and had no removal path. Other
sessions for the same account kept the outer logind runtime available.

The owner now creates its runtime through its original parent FD and
retains close-on-exec references to the runtime and cache. It checks
the live names through the original parent references and requests
empty-directory removal. Cleanup also runs on setup failures after an
original directory was retained. The original child and output lifecycle
controls normal teardown. Cleanup errors appear separately from the
desktop result.

The native regression exercises successful cleanup with a second runtime
under the same parent, nonempty-cache refusal, consumed cleanup
references, a replaced cache symlink and a replaced runtime name. The
tests use real Linux directories and preserve the foreign targets.
Cleanup uses no recursive removal. Name validation and removal are
separate syscalls, so this profile assumes cooperating same-UID writers.

Release `f0a82c0f3394` and memory `3d1b4bb31305` each build both desktop
entry profiles and pass nine selected login/session CTests plus
`login.ui_poll`. The original desktop-owner fork/barrier and descriptor
regressions pass in both profiles under `50c1cd269af3`. Bound Release
entry `73bfe576d2f2` passes the GCC analyzer and the independent 200-file
runtime inventory. Its staged copy `765138f786ed` has no build RPATH
or RUNPATH. The first optimized build `580ff13c670c` rejected an
unproven formatting bound. The final source copies exactly the checked
pathname length and cache suffix.

Confined VM case `40866a4393b0` uses that staged executable and the
prior tested effective policy. Real PAM and keyboard input reach the
visible desktop. After it ends, the original entry and PAM worker exit
zero, the user session closes and the runtime no longer resolves.
The controller witnesses the same outer user-runtime directory and
same-UID control sibling before and after cleanup. The independent
sibling's canary remains intact. The empty replacement greeter appears
and stops with the original backend. The controller restores the
original enforcing state and terminal seat. The normal installed
deployment and any policy for retiring populated application caches
remain separate work.
