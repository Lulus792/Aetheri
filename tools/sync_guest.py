"""Copy an explicit source snapshot through SSH; no writable host-folder sharing."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile

import run_vm as vm


def main():
    if not vm.running():
        raise ValueError("guest identity is not verified")
    # Include only repository source, contracts and development tests, never vm,
    # local, downloads, private keys, build outputs or another project.
    paths = [vm.ROOT / "CMakeLists.txt"]
    for prefix in ("src", "tests", "spec", "tools", "policy"):
        paths.extend(path for path in (vm.ROOT / prefix).rglob("*")
                     if path.is_file() and path.suffix in (".c", ".h", ".py", ".sql", ".xml", ".json", ".cil"))
    payload = io.BytesIO()
    inventory = {}
    with tarfile.open(fileobj=payload, mode="w") as archive:
        for path in sorted(paths):
            if path.is_symlink() or not path.resolve().is_relative_to(vm.ROOT):
                raise ValueError("source path outside workspace")
            data = path.read_bytes()
            name = path.relative_to(vm.ROOT).as_posix()
            if len(data) > 1024**2:
                raise ValueError("unexpected source size")
            inventory[name] = hashlib.sha256(data).hexdigest()
            info = tarfile.TarInfo(name)
            info.size, info.mode, info.mtime = len(data), 0o644, 0
            archive.addfile(info, io.BytesIO(data))
    command = [sys.executable, "-X", "utf8", str(vm.ROOT / "tools/run_vm.py"), "ssh"]
    subprocess.run(command + ["mkdir -p /home/aetheri/source"], check=True,
                   stdin=subprocess.DEVNULL, creationflags=vm.FLAGS)
    # The archive has deterministic timestamps; extraction uses guest time so
    # changed bytes cannot look older than already-built Ninja object files.
    result = subprocess.run(command + ["tar -xmf - -C /home/aetheri/source"],
                            input=payload.getvalue(), capture_output=True, creationflags=vm.FLAGS)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace"))
    (vm.VM / "source-snapshot.json").write_text(json.dumps({
        "archive_sha256": hashlib.sha256(payload.getvalue()).hexdigest(),
        "files": inventory}, indent=2), encoding="utf-8")
    print(f"Copied {len(inventory)} source files into the disposable guest.")


if __name__ == "__main__":
    main()
