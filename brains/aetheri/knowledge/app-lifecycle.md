# Application ownership and complete removal

User requirement confirmed 25 October 2026. This is a product/architecture draft,
not implemented behavior. It must influence packaging and runtime isolation from
the beginning, not be added after apps can scatter state.

## Product contract

Aetheri must show what an installation added, why it is present, how much space
it occupies, what it starts, which permissions it has and what removal will affect.
The component view must connect packages, files, state, plugins, services, processes
and dependencies in one understandable model.

Offer two explicit actions:

- Remove the application while retaining selected app data for later reuse.
- Remove the application and its managed data/integrations completely, with a
  concrete preview and optional deliberate retention of selected data.

The exact labels/defaults are a later UI choice. Do not silently preserve an
AppData-like directory after claiming complete removal.

The accepted [module/interface direction](modules-and-interfaces.md) defines
responsibility scopes. The recommended [identity/lifecycle contract](identity-and-lifecycle.md)
refines installation generations, data epochs, operation retries and recovery for
these requirements.

## Ownership model to design

A manifest of installed files is necessary but not sufficient: it cannot observe
arbitrary future writes by unrestricted native programs. To support the guarantee,
combine package ownership with controlled app-private storage, mediated access to
external locations, and explicit registration APIs.

Every managed object needs a stable owning identity, role and lifecycle:

| Object | Examples and expected accounting |
| --- | --- |
| Installed package | Executable, libraries, resources, localization and license records |
| Per-user app state | Settings, databases, history, caches, logs and temporary files |
| Shared app state | App-specific service data or machine configuration, with admin scope |
| Extensions | Editor/browser/terminal plugins, plugin state and owned dependencies |
| Credentials | App-owned keyring entries/tokens; do not display secret values |
| Integrations | Launchers, file associations, protocol handlers, notifications and autostart |
| Background work | Services, agents, scheduled jobs, indexers, IPC endpoints and registrations |
| Permissions | Files/devices/network grants and access-broker records |
| Development tools | SDKs, toolchains, package caches and projects with separate ownership |
| Recovery artifacts | Retained package versions, rollback snapshots and backups |

Include direct and transitive dependencies, install reason, source/version, actual
locations, activation conditions and privileges. A UI installation card and process
view should use the same inventory, not separate manually maintained explanations.

Do not permit unreviewed package scripts/plugins to write anywhere as root while
promising fully tracked removal. Restricted app writes and a privileged broker are
candidate architecture, not a selected mechanism. Foreign/manual unmanaged programs
must be visibly identified as outside the managed guarantee until imported or contained.

## Preview and removal behavior

The preview should identify app code, private state, extensions, credentials,
registrations and service effects, then show each of these distinctly:

- Will be removed in the chosen scope.
- Retained because another component/user still needs it.
- Retained because the user explicitly chose to keep it.
- User-created/exported documents that are outside automatic app-state removal.
- Backup/rollback copies, remote data or other storage not removed by this action.
- Unknown/unmanaged state or failures that prevent a complete result.

Installed files and private caches are different from a user's Physim experiment,
exported PDF, downloaded document or source repository. Exported/independent documents
must not disappear just because the creating/viewing app is removed. If the user
wants those deleted as well, make their selection and consequences explicit.

Shared dependencies can only be removed when no installed component or retained
recovery policy requires them. Explain why any remain and offer removal when unused.
A package with an explicit user install reason must not be mistaken for an orphan
merely because the app that once used it is gone.

Revoke app-owned integrations and stop owned background work without affecting other
apps. Cover the selected user's state versus machine-wide removal versus other users.
Admin actions must not silently erase another user's documents or private state.

## Reliability and safety properties

Plan and revalidate target identities before committing changes. Reject escaping
paths, symlink substitutions and unexpected ownership changes. Use package/store
handles and scoped operations rather than blindly expanding manifest paths.

Removal must handle running apps, concurrent updates, interruption and restart.
Journal/recover the operation, prevent services from recreating removed state, and
report partial outcomes honestly. Never display complete success while known owned
residue remains. A small maintenance/audit record has its own retention and removal
policy; it must not preserve deleted secrets or pretend to be removed app data.

Show logical size and potentially reclaimable storage carefully. Shared extents,
hardlinks and retained snapshots mean deleting a visible directory may not release
the same physical bytes. Do not imply forensic secure erasure or deletion of copies
in backups/remote storage when only the live installation was purged.

## Acceptance cases for later implementation

1. Install, launch, change settings, create caches/logs/credentials and a plugin;
   preview the ownership graph; complete removal leaves no selected app-private
   state, registration or owned background task.
2. Remove while explicitly preserving settings; show their location, size and
   install-independent cleanup action. Reinstall uses only deliberately retained state.
3. Two apps share a library; removing one preserves the working other app. Removing
   the final consumer permits cleanup subject to explicit install/recovery reasons.
4. Preserve independent documents, Physim projects and user-selected exports.
5. Multiple users and machine-wide services observe their scope and permissions.
6. App crash, full disk, interrupted uninstall, concurrent update and restart
   produce recoverable and truthful results.
7. Malicious/changed paths or symlinks do not delete unrelated files.
8. Plugin disable/remove/purge follows its own permissions, state and process ownership.
9. Unmanaged content and remote/backup copies remain visibly outside the result.
10. Test storage reclaimed versus retained recovery/shared storage without double counting.

## Proposed storage enforcement

The [filesystem/permissions proposal](filesystem-and-permissions.md) maps these
requirements to immutable code, exact private storage roots, protected ownership
headers, external document grants and mandatory same-UID isolation.
The SELinux/ext4/LUKS2 recommendations are new proposals, not tested integrations.
Removing a directory name alone cannot establish complete managed-state removal.

## Relation to other systems

Flatpak distinguishes package removal and deletion of app data, showing that these
are separate lifecycle decisions. It is a reference only; Aetheri has not selected
Flatpak or its storage design. See [command reference](https://docs.flatpak.org/en/latest/flatpak-command-reference.html).
