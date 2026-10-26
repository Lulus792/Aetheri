"""Extract the pinned host tools locally. Does not execute MSI/NSIS installers."""
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import prepare_vm

ROOT = prepare_vm.ROOT
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def cabinet(msi, destination):
    dll = c.WinDLL("msi.dll")
    signatures = {
        "MsiOpenDatabaseW": [w.LPCWSTR, w.LPCWSTR, c.POINTER(w.UINT)],
        "MsiDatabaseOpenViewW": [w.UINT, w.LPCWSTR, c.POINTER(w.UINT)],
        "MsiViewExecute": [w.UINT, w.UINT],
        "MsiViewFetch": [w.UINT, c.POINTER(w.UINT)],
        "MsiRecordReadStream": [w.UINT, w.UINT, c.c_void_p, c.POINTER(w.DWORD)],
        "MsiCloseHandle": [w.UINT],
    }
    for name, arguments in signatures.items():
        function = getattr(dll, name)
        function.argtypes, function.restype = arguments, w.UINT

    def check(code):
        if code:
            raise OSError(code, "read-only MSI data access failed")

    database, view, record = w.UINT(), w.UINT(), w.UINT()
    try:
        # Null persistence mode is MSIDBOPEN_READONLY. No installation actions.
        check(dll.MsiOpenDatabaseW(str(msi), None, c.byref(database)))
        check(dll.MsiDatabaseOpenViewW(database,
              "SELECT `Data` FROM `_Streams` WHERE `Name`='product.cab'", c.byref(view)))
        check(dll.MsiViewExecute(view, 0))
        check(dll.MsiViewFetch(view, c.byref(record)))
        total = 0
        with destination.open("xb") as output:
            while True:
                buffer = c.create_string_buffer(65536)
                length = w.DWORD(len(buffer))
                check(dll.MsiRecordReadStream(record, 1, buffer, c.byref(length)))
                if not length.value:
                    break
                total += length.value
                if total > 32 * 1024**2:
                    raise ValueError("cabinet exceeds expected bootstrap bounds")
                output.write(buffer.raw[:length.value])
    finally:
        for handle in (record, view, database):
            if handle.value:
                dll.MsiCloseHandle(handle)


def publish_files(source, target, mapping=None):
    expected = mapping or {p.relative_to(source).as_posix(): p.relative_to(source).as_posix()
                           for p in source.rglob("*") if p.is_file()}
    hashes = {}
    for old, new in expected.items():
        original, final = source / old, target / new
        if original.is_symlink() or final.is_symlink() or not final.resolve().is_relative_to(ROOT):
            raise ValueError("tool path outside workspace")
        digest = sha(original)
        if final.exists():
            if sha(final) != digest:
                raise ValueError("existing tool differs: " + new)
        else:
            final.parent.mkdir(parents=True, exist_ok=True)
            # Exclusive destination; no overwrite of another extraction.
            with final.open("xb") as output, original.open("rb") as incoming:
                shutil.copyfileobj(incoming, output)
        hashes[final.relative_to(ROOT).as_posix()] = digest
    return hashes


def main():
    if os.name != "nt":
        raise ValueError("Windows host provisioning only")
    lock = json.loads((ROOT / "spec/vm-inputs.json").read_text(encoding="utf-8"))
    inputs = {r["name"]: r for r in lock["inputs"]}
    for name in ("7z2605-x64.msi", "qemu-w64-setup-20260811.exe"):
        prepare_vm.verify(ROOT / "downloads" / name, inputs[name])
    local = ROOT / "local"
    if local.is_symlink() or local.resolve() != local:
        raise ValueError("local tools must remain inside workspace")
    local.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tool-bootstrap-", dir=local) as temporary:
        temp = Path(temporary)
        if temp.resolve().parent != local.resolve():
            raise ValueError("temporary cleanup outside workspace")
        cab = temp / "7zip.cab"
        cabinet(ROOT / "downloads/7z2605-x64.msi", cab)
        seven = temp / "seven"
        seven.mkdir()
        mapping = {"_7z.exe": "7z.exe", "_7z.dll": "7z.dll",
                   "License.txt": "License.txt", "readme.txt": "readme.txt"}
        for member in mapping:
            subprocess.run([str(Path(os.environ["SystemRoot"]) / "System32/expand.exe"),
                            "-F:" + member, str(cab), str(seven)], check=True,
                           stdout=subprocess.DEVNULL, creationflags=FLAGS)
        receipt = publish_files(seven, local / "tools/7zip-26.05", mapping)
        qemu = temp / "qemu"
        archive = local / "tools/7zip-26.05/7z.exe"
        subprocess.run([str(archive), "x", "-y", "-o" + str(qemu),
            str(ROOT / "downloads/qemu-w64-setup-20260811.exe"),
            "qemu-system-x86_64.exe", "qemu-img.exe", "*.dll", "share/*", "keymaps/*",
            "COPYING", "COPYING.LIB", "VERSION"], check=True, stdout=subprocess.DEVNULL,
            creationflags=FLAGS)
        receipt.update(publish_files(qemu, local / "tools/qemu-11.1.0"))
        (local / "verification").mkdir(exist_ok=True)
        (local / "verification/host-tools.json").write_text(json.dumps({
            "source_digests": {name: inputs[name]["digest"] for name in
                               ("7z2605-x64.msi", "qemu-w64-setup-20260811.exe")},
            "files": receipt}, indent=2), encoding="utf-8")
        print(f"Verified {len(receipt)} extracted files; no installer executed.")
    subprocess.run([str(local / "tools/qemu-11.1.0/qemu-system-x86_64.exe"), "--version"],
                   check=True, creationflags=FLAGS)


if __name__ == "__main__":
    main()
