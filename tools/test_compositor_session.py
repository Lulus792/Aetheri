"""Check that the optional DRM session never silently selects another backend."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary, build = (Path(p).resolve() for p in sys.argv[1:])
    if os.getuid() == 0:
        raise RuntimeError("run session argument checks as an ordinary user")
    root = Path(tempfile.mkdtemp(prefix="session-arguments-", dir=build))
    runtime = root / "runtime"
    runtime.mkdir(mode=0o700)
    env = {**os.environ, "XDG_RUNTIME_DIR": str(runtime)}
    env.pop("LIBSEAT_BACKEND", None)
    # None of these cases is allowed to contact logind or acquire a device.
    cases = [("missing-backend", "/dev/dri/card0", None, 1),
             ("foreign-backend", "/dev/dri/card0", "seatd", 1),
             ("malformed-card", "/dev/dri/card0/../card1", "logind", 2),
             ("render-node", "/dev/dri/renderD128", "logind", 2),
             ("missing-card-index", "/dev/dri/card", "logind", 2)]
    results = []
    try:
        for name, card, backend, expected in cases:
            selected = {**env}
            if backend is not None:
                selected["LIBSEAT_BACKEND"] = backend
            result = subprocess.run([str(binary), "--drm", "session-negative", "-", card],
                                    env=selected, stdin=subprocess.DEVNULL,
                                    capture_output=True, timeout=10)
            (root / (name + ".log")).write_bytes(result.stdout + result.stderr)
            assert result.returncode == expected, (name, result.returncode, result.stderr)
            assert b"READY " not in result.stdout, name
            assert not list(runtime.iterdir()), name
            results.append({"case": name, "exit": result.returncode})
        (root / "result.json").write_text(json.dumps(results, indent=2) + "\n")
    finally:
        print("Session argument evidence:", root)


if __name__ == "__main__":
    main()
