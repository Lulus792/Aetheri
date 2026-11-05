"""Prepare signed, private native accessibility test inputs; no RPM installation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path)
    parser.add_argument("work", type=Path, help="fresh private unprivileged test prefix")
    parser.add_argument("dbus_sdk", type=Path, help="checked NexaUI build's dbus-sdk directory")
    args = parser.parse_args()
    if sys.platform != "linux" or os.getuid() == 0:
        raise ValueError("use an unprivileged Linux build account")
    config = json.loads((ROOT / "spec/atspi-inputs.json").read_text())
    sdk = args.dbus_sdk.resolve()
    sdk_record = json.loads((sdk.parent / "build.json").read_text())
    pin = json.loads((ROOT / "spec/nexaui-inputs.json").read_text())
    if sdk_record["revision"] != pin["revision"] or len(sdk_record["dbus_headers"]) != 18:
        raise ValueError("unexpected original D-Bus header SDK")
    for name, expected in sdk_record["dbus_headers"].items():
        relative = Path(name).relative_to("/usr")
        if ".." in relative.parts or digest(sdk / relative) != expected:
            raise ValueError("original D-Bus header changed")
    records = []
    for package, expected in config["runtime"].items():
        actual = subprocess.check_output(["rpm", "-q", "--qf", "%{NAME}-%{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH}", package], text=True)
        if actual != expected:
            raise ValueError("reviewed test runtime changed: " + package)
    for entry in config["inputs"]:
        path = args.cache / entry["name"]
        if path.is_symlink() or path.stat().st_size != entry["size"] or digest(path) != entry["sha256"]:
            raise ValueError("unexpected test RPM")
        proof = subprocess.check_output(["rpmkeys", "--checksig", "--verbose", str(path)], text=True)
        if "key fingerprint: " + config["signer"] + ": OK" not in proof or "Payload SHA256 digest: OK" not in proof:
            raise ValueError("reviewed signer/digest not verified")
        records.append((path, proof))
    work = args.work.absolute()
    if work.is_symlink() or work.resolve() != work:
        raise ValueError("aliased test destination")
    work.mkdir(mode=0o700)
    selected = {
        "/usr/bin/dbus-daemon", "/usr/libexec/at-spi2-registryd", "/usr/lib64/libatspi.so.0.0.1",
        "/usr/lib64/libXtst.so.6.1.0", "/usr/lib64/libXi.so.6.1.0", "/usr/lib64/libXext.so.6.4.0"}
    files = {}
    for path, _ in records:
        names = subprocess.check_output(["rpm", "-qpl", str(path)], text=True).splitlines()
        cpio = subprocess.check_output(["rpm2cpio", str(path)])
        for name in names:
            header = name.endswith(".h") and name.startswith(("/usr/include/at-spi-2.0/", "/usr/include/glib-2.0/", "/usr/lib64/glib-2.0/include/"))
            notice = name.endswith("/COPYING") and name.startswith(("/usr/share/licenses/", "/usr/share/doc/libX"))
            if name not in selected and not header and not notice:
                continue
            relative = Path(name).relative_to("/")
            if ".." in relative.parts or name in files:
                raise ValueError("unexpected or duplicate test member")
            data = subprocess.check_output(["cpio", "-i", "--quiet", "--to-stdout", "." + name], input=cpio)
            if not data or len(data) > 4 * 1024**2:
                raise ValueError("unexpected test member size")
            dest = work / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
            dest.chmod(0o755 if name in selected else 0o644)
            files[name] = digest(dest)
    if not selected.issubset(files):
        raise ValueError("missing private native fixture")
    for name, target in (("libatspi.so.0", "libatspi.so.0.0.1"), ("libXtst.so.6", "libXtst.so.6.1.0"),
                         ("libXi.so.6", "libXi.so.6.1.0"), ("libXext.so.6", "libXext.so.6.4.0")):
        (work / "usr/lib64" / name).symlink_to(target)
    # libatspi is an independent test peer. Its X11 dependencies stay in this
    # private fixture, never the installed NexaUI package or app link interface.
    command = ["cc", "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
               str(ROOT / "tests/nexaui_settings_atspi.c"), "-o", str(work / "settings-atspi-client")]
    command += ["-I" + str(work / p) for p in ("usr/include/at-spi-2.0", "usr/include/glib-2.0", "usr/lib64/glib-2.0/include")]
    command += ["-I" + str(sdk / p) for p in ("include/dbus-1.0", "lib64/dbus-1.0/include")]
    command += [str(work / "usr/lib64/libatspi.so.0.0.1"), "-l:libgobject-2.0.so.0", "-l:libglib-2.0.so.0",
                "-Wl,-rpath," + str(work / "usr/lib64"), "-Wl,-rpath-link," + str(work / "usr/lib64")]
    with (work / "build.log").open("xb") as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    result = {"inputs": [{"name": p.name, "signature": proof} for p, proof in records],
              "files": files, "client_sha256": digest(work / "settings-atspi-client"),
              "source_sha256": digest(ROOT / "tests/nexaui_settings_atspi.c"), "command": command,
              "scope": "private independent native test environment; no system installation or normal session adoption"}
    (work / "inputs.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"result": "prepared", "work": str(work), "files": len(files), "client_sha256": result["client_sha256"]}))


if __name__ == "__main__":
    main()
