"""Explicit protected current runtime for the private development supervisor.

The caller independently selects a fresh Root-prepared runtime and this source's
profile. Neither runtime.json nor these role projections grant app admission,
publisher trust, a volume identity or recovery/lease authority.
"""
import copy
from pathlib import Path

import prepare_current_desktop_runtime as current

KIND = 'aetheri-current-desktop-v1'
PROGRAMS = {
    'settings': 'aetheri-settings-nexaui',
    'files': 'aetheri-files-nexaui',
    'terminal': 'aetheri-terminal-nexaui',
}
SUPPORT = ('aetheri-compositor', 'aetheri-desktop', 'aetheri-shell',
           'aetheri-editor', 'aetheri-terminal-pty-starter', 'aetheri-text-editor')


class Runtime:
    def __init__(self, binding, profile, desktop, settings_ui, files_ui, terminal_ui):
        if (profile != 'release' or desktop is not True
                or (settings_ui, files_ui, terminal_ui) != ('nexaui-owned',)*3):
            raise ValueError('current runtime requires the complete Release owned desktop')
        if (not isinstance(binding, dict) or set(binding) != {'kind', 'root', 'profile_sha256'}
                or binding['kind'] != KIND or not isinstance(binding['root'], str)):
            raise ValueError('invalid independent current runtime binding')
        root = Path(binding['root'])
        if not root.is_absolute() or str(root) != binding['root'] or root.resolve() != root:
            raise ValueError('noncanonical current runtime selection')
        value = current.profile()  # Trusted source profile, never the candidate's record.
        if current.fingerprint(value) != binding['profile_sha256']:
            raise ValueError('different independently selected current runtime profile')
        self.root, self.value, self.binding = root, value, copy.deepcopy(binding)
        self.build = root/'usr/libexec/aetheri-development'
        self.ui = root/'usr'
        self.font = self.ui/'share/aetheri-fonts/LiberationMono-Regular.ttf'
        self.bold_font = self.ui/'share/aetheri-fonts/LiberationMono-Bold.ttf'
        self._entries = {e['destination']: e for e in value['files']}
        for name in (*PROGRAMS.values(), *SUPPORT):
            self._entry(self.build/name, 'program')
        for name in ('LiberationSans-Regular.ttf', self.font.name, self.bold_font.name):
            self._entry(self.ui/'share/aetheri-fonts'/name, 'font')
        self.recheck()

    def _entry(self, path, role):
        entry = self._entries.get('/'+path.relative_to(self.root).as_posix())
        if entry is None or entry['role'] != role:
            raise ValueError('missing current desktop role input: '+str(path))
        return entry

    def recheck(self):
        # Every new app attempt repeats the complete protected-tree/body check.
        # This observation is not an atomic exec pin or a sandbox grant.
        return current.check_protected(self.root, self.value)

    def selection(self, role):
        if role not in PROGRAMS:
            raise ValueError('unknown current desktop role')
        binary = self.build/PROGRAMS[role]
        view = dict(kind='aetheri-current-'+role+'-v1', binary=str(binary),
                    sha256=self._entry(binary, 'program')['sha256'])
        if role == 'terminal':
            starter = self.build/'aetheri-terminal-pty-starter'
            view['starter'] = dict(binary=str(starter), sha256=self._entry(starter, 'program')['sha256'])
        return dict(kind=KIND, runtime=copy.deepcopy(self.binding), role=role, presentation=view)

    def environment(self, original):
        # The finite PAM fixture supplies the remaining carrier/session context.
        # Do not inherit library/renderer overrides from a producer environment.
        prefixes = ('LD_', 'SDL_', 'MESA_', 'GALLIUM_', 'LIBGL_', '__EGL_', '__GLX_',
                    'EGL_', 'GL_', 'DRI_', 'GBM_', 'VK_', 'LIBINPUT_')
        env = {k:v for k,v in original.items() if not k.startswith(prefixes)}
        env.update(LD_LIBRARY_PATH=str(self.root/'usr/lib64'),
                   LIBINPUT_QUIRKS_DIR=str(self.ui/'share/libinput'),
                   __EGL_VENDOR_LIBRARY_FILENAMES=str(self.ui/'share/glvnd/egl_vendor.d/50_mesa.json'),
                   LIBGL_DRIVERS_PATH=str(self.root/'usr/lib64/dri'),
                   LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe', LP_NUM_THREADS='2')
        return env
