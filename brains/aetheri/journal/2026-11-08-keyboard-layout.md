# Explicit startup keyboard layout

8 November 2026. The first laptop image still uses US. Native development now
adds `--keyboard-layout us|de` to the original compositor's headless and DRM
profiles, as a prerequisite for a normal layout selector. It does not replace
the image being flashed or change an installed service.

The parser stores a validated literal for the lifetime of the server. Input
setup compiles and installs that map for each new keyboard; failure stops setup
without substituting another layout. Default startup retains a backend's map
and uses US only when none exists. Nested mode retains its parent map and rejects
the override. No dependency, privilege, wire contract or input global changes.

The separate native client observes actual keys from the inherited test driver:
Y/Z, punctuation, German umlauts and sharp S, Shift, AltGr and replacement devices.
An explicit US choice also overrides a backend-provided German map. Invalid,
missing, repeated and nested selections fail before a server socket is created.

Final keyboard-v3 passes all seven affected groups in Release (7.77 s) and
Debug ASan/UBSan (12.23 s): DRM selection, session arguments, Wayland rendering,
input, layout selection, clipboard and nested presentation. Both JUnit reports
contain seven tests and zero failures. Token
`52768fe611984e9382f766419c2dc3b5`, helper 1153367; all 279 export members are
independently verified before acknowledgement and exact retirement with exit0.
The 2,366,510-byte export SHA256 is
`180e2e0e7b54e9307c3c8043661deff10b7feaea01e19dacda06a378f05b3104`.

Earlier attempts remain recorded. The first fails with SIGBUS before readiness.
A separate diagnostic build records a Pixman write fault inside a deleted
wlroots shared-memory mapping. The carrier's existing `/dev/shm` has user quotas
and substantial retained use. A later read-only Q_GETQUOTA observation returns
UID1000's 3,245,398-KiB hard/soft limit and 3,186,679,808 bytes in use; that later
snapshot does not measure quota headroom at the original fault. A fresh 512-MiB
shared-memory mount in the test's private namespace removes that failure without
changing or cleaning the original mount.

The next failure is distinct: diagnostic stage/errno36 proves the generated
Wayland socket path is too long. The final workspace uses a shorter independently
chosen token path. No product branch, timeout or permission is weakened to pass.
The diagnostic instrumentation exists only in the private evidence copy. Every
failed and diagnostic helper is exported, verified and retired; the old carrier
and retained data are not rebooted, deleted or adopted.

The [first-test guide](../../../spec/first-laptop-test.md) describes only the
actual USB image, including its US mapping and unsigned EFI executable. Normal
Settings controls, runtime switching, persistence and physical keyboard/DRM
acceptance remain work. NexaUI's existing catalogue already covers layout UI;
there is no new handoff message. The full OS objective remains incomplete.
