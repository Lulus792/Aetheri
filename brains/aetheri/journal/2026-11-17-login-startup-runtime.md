# Signed startup and desktop program files

The installed startup adapter captures five signed sources in its own mount
view and in the original backend's view. They cover the startup executable and
unit, session entry, compositor and desktop shell. In both views it joins
the actual self executable to the independently selected held file using the
kernel pidfd and proc ELF observation. Each later gate checks the five files
in its current view. A detected loss fences the retained observation. The
ten common backend handoff sources and signed record format retain their
existing layout.

The Release build `92c2db86529d` and ASan/UBSan build `37ac096ba51b` use 834
inputs and pass 11 selected CTests each. Private fixed-path case
`33d7e1bd02c8` passes seven scenarios
in each build. An initial desktop program whose bytes differ from the
signed selection fails. A byte-identical startup executable on a different
inode fails before the runner. Removing the original desktop file fences
the observed runtime after the file is restored. Existing matching and
differing selection, wrong effective policy and platform denial cases pass.
The controller wraps the service-runner boundary in these cases.

The corrected SELinux candidate `c131c408056b` grants the startup domain
read, open and getattr on the three desktop executable types. Loaded
kernel decisions confirm those nine permissions and deny nine write or
execution permissions. Private Release service `50716c9b739e` runs under
the intended startup domain and uses the reviewed libcrypto 3.5.9 in
its mount view. It passes source and self-ELF capture, checks the actual
domain, then rejects the deliberately wrong signed effective policy as
expected. The private service ends inactive and its original fragment is
removed. This probe wraps the runner and creates no handoff or PAM session.

Preserve initial module build `8babca101bb6` with invalid CIL list syntax,
the missing host executable precheck `3eb4c19115e2`, and terminal probe
`b9b53d5b2adb` that selected the carrier's 3.5.5 libcrypto and failed the
minimum version check. The corrected test binds verified 3.5.9 only inside
the service's private view. The final image still needs the production
platform gates and a booted graphical login through the installed units.
The candidate policy was retired to the exact baseline with enforcing mode
and no remaining candidate tasks.
