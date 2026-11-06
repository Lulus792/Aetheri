"""Test-only host adapters around the real development supervisor loop.

App commands, process/pipe ownership, packets and shutdown are unchanged.
Use an actual headless test compositor and report notify/diagnostics locally;
never request a PAM/DRM unit, touch the root controller or install a service.
"""
import os
import importlib.util
from pathlib import Path
import sys

def main():
    build, root, issuer = Path(sys.argv[1]), Path(sys.argv[2]), int(sys.argv[3])
    if os.getuid() != 1000:
        raise RuntimeError("explicit disposable-carrier test account required")
    path = Path(sys.argv[4]) if len(sys.argv)>4 else Path(__file__).resolve().parents[1]/"tools/development_session.py"
    spec = importlib.util.spec_from_file_location("development_session_under_test", path)
    session = importlib.util.module_from_spec(spec);spec.loader.exec_module(session)
    session.terminal_assets()
    session.USER_ROOT = root / "sessions"
    session.build_for = lambda profile: build
    session.notify = lambda message: print("FIXTURE_NOTIFY " + message.splitlines()[0], flush=True)
    session.emit_child_line = lambda role, line: print(role + ": " + line.decode(errors="replace"), flush=True)

    def compositor(build, shell_server, compose=False):
        captures = root / ("missing-captures" if len(sys.argv)>5 and sys.argv[5]=="early-error" else "captures")
        return ([str(build / "test_compositor_server"), "--headless", "aetheri-development",
                 str(captures), str(issuer), "--shell-fd", str(shell_server.fileno()), *(["--compose"] if compose else [])],
                (issuer, shell_server.fileno()))

    session.compositor_launch = compositor
    ui=os.environ.get("AETHERI_TEST_SETTINGS_UI","sdl")
    files_ui=os.environ.get("AETHERI_TEST_FILES_UI","sdl")
    if ui in ("nexaui", "nexaui-owned") and "AETHERI_TEST_SETTINGS_SOURCE" in os.environ:
        session.PROJECT=Path(os.environ["AETHERI_TEST_SETTINGS_SOURCE"])
    if files_ui == "nexaui-owned" and "AETHERI_TEST_FILES_SOURCE" in os.environ:
        session.PROJECT=Path(os.environ["AETHERI_TEST_FILES_SOURCE"])
    profile="debug" if "AETHERI_SANITIZERS:BOOL=ON" in (build/"CMakeCache.txt").read_text() else "release"
    selected=session.settings_selection(profile,ui)
    binding=session.settings_fingerprint(selected) if selected is not None else None
    files_profile=session.files_selection(profile,files_ui)
    files_binding=session.profile_fingerprint(files_profile) if files_profile is not None else None
    return session.supervise(profile, "fixture", desktop=True, compose=len(sys.argv)>5 and sys.argv[5]=="compose",
                             settings_ui=ui,settings_binding=binding,files_ui=files_ui,files_binding=files_binding)


if __name__ == "__main__":
    raise SystemExit(main())
