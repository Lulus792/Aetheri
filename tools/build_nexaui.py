"""Build the pinned NexaUI Settings probe privately; never install on the host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("work", type=Path, help="fresh unprivileged Linux directory")
    parser.add_argument("--sanitizers", action="store_true")
    parser.add_argument("--dbus-rpm", type=Path, required=True,
                        help="pinned Fedora SDK read as headers only; never installed")
    args = parser.parse_args()
    if sys.platform != "linux" or os.getuid() == 0:
        raise ValueError("use an unprivileged Linux build account")
    config = json.loads((ROOT / "spec/nexaui-inputs.json").read_text())
    archive = args.archive.absolute()
    if archive.is_symlink() or not archive.is_file() or archive.stat().st_size != config["archive"]["size"]:
        raise ValueError("unexpected source archive")
    if digest(archive) != config["archive"]["sha256"]:
        raise ValueError("source archive hash mismatch")
    dbus = config["dbus_sdk"]
    rpm = args.dbus_rpm.absolute()
    if rpm.is_symlink() or not rpm.is_file() or rpm.stat().st_size != dbus["size"] or digest(rpm) != dbus["sha256"]:
        raise ValueError("unexpected D-Bus SDK package")
    signature = subprocess.check_output(["rpmkeys", "--checksig", "--verbose", str(rpm)], text=True)
    if "key fingerprint: " + dbus["signer"] + ": OK" not in signature or "Payload SHA256 digest: OK" not in signature:
        raise ValueError("reviewed Fedora signer/digest not verified")
    nevra = subprocess.check_output(["rpm", "-q", "--qf", "%{NAME}-%{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH}", "dbus-libs"], text=True)
    if nevra != dbus["runtime_nevra"]:
        raise ValueError("reviewed D-Bus runtime changed")
    work = args.work.absolute()
    if work.is_symlink() or work.resolve() != work:
        raise ValueError("aliased build destination")
    work.mkdir(mode=0o700)
    source, snapshot, prefix = work / "source", work / "aetheri-model", work / "prefix"
    source.mkdir(mode=0o700)
    sdk = work / "dbus-sdk"
    headers = subprocess.check_output(["rpm", "-qpl", str(rpm)], text=True).splitlines()
    cpio = subprocess.check_output(["rpm2cpio", str(rpm)])
    header_hashes = {}
    for name in headers:
        if not name.endswith(".h") or not name.startswith(("/usr/include/dbus-1.0/dbus/", "/usr/lib64/dbus-1.0/include/dbus/")):
            continue
        relative = Path(name).relative_to("/usr")
        if ".." in relative.parts:
            raise ValueError("unexpected D-Bus header path")
        data = subprocess.check_output(["cpio", "-i", "--quiet", "--to-stdout", "." + name], input=cpio)
        if not data or len(data) > 1024**2:
            raise ValueError("unexpected D-Bus header size")
        destination = sdk / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        header_hashes[name] = hashlib.sha256(data).hexdigest()
    if len(header_hashes) != 18:
        raise ValueError("unexpected D-Bus header inventory")
    pc = sdk / "pkgconfig"
    pc.mkdir()
    (pc / "dbus-1.pc").write_text("Name: dbus\nDescription: pinned header-only SDK for late-bound native AT-SPI\nVersion: 1.16.2\n"
        + "Cflags: -I" + str(sdk / "include/dbus-1.0") + " -I" + str(sdk / "lib64/dbus-1.0/include") + "\nLibs:\n")
    env = {**os.environ, "PKG_CONFIG_PATH": str(pc)}
    with tarfile.open(archive, "r:") as stream:
        members = stream.getmembers()
        names = set()
        for member in members:
            name = Path(member.name)
            if name.is_absolute() or ".." in name.parts or member.name in names or not (member.isfile() or member.isdir()):
                raise ValueError("unexpected source member")
            if member.size > 16 * 1024**2:
                raise ValueError("unexpected member size")
            names.add(member.name)
        if len(members) != 1051 or sum(m.isfile() for m in members) != 945 or sum(m.size for m in members) > 64 * 1024**2:
            raise ValueError("unexpected source inventory")
        stream.extractall(source, filter="data")
    if digest(source / config["license"]["path"]) != config["license"]["sha256"]:
        raise ValueError("source license mismatch")
    models = {}
    for name in config["model"]:
        data = (ROOT / name).read_bytes()
        dest = snapshot / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        models[name] = hashlib.sha256(data).hexdigest()
    commands = []
    flags = "-fsanitize=address,undefined -fno-omit-frame-pointer" if args.sanitizers else ""
    common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + ("Debug" if args.sanitizers else "Release"),
              "-DCMAKE_C_FLAGS=" + flags, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
              "-DCMAKE_INSTALL_RPATH=" + str(prefix / "lib")]
    with (work / "build.log").open("xb") as log:
        def run(command):
            commands.append(command)
            subprocess.run(command, check=True, env=env, stdout=log, stderr=subprocess.STDOUT)
        build = work / "sdk-build"
        run(["cmake", "-S", str(source), "-B", str(build), *common,
             "-DCMAKE_INSTALL_PREFIX=" + str(prefix), "-DCMAKE_INSTALL_LIBDIR=lib",
             "-DBUILD_SHARED_LIBS=ON", "-DNEXAUI_BUILD_TESTS=OFF", "-DNEXAUI_BUILD_EXAMPLES=OFF",
             "-DNEXAUI_ENABLE_X11=OFF", "-DNEXAUI_BUILD_VULKAN=OFF",
             "-DNEXAUI_ENABLE_ATSPI=ON",
             "-DNEXAUI_SOURCE_REVISION=" + config["revision"]])
        run(["cmake", "--build", str(build), "-j2"])
        run(["cmake", "--install", str(build)])
        consumer = work / "consumer-build"
        run(["cmake", "-S", str(source / config["consumer"]), "-B", str(consumer), *common,
             "-DCMAKE_PREFIX_PATH=" + str(prefix), "-DAETHERI_SOURCE_DIR=" + str(snapshot)])
        run(["cmake", "--build", str(consumer), "-j2"])
    if "NEXAUI_HAVE_DBUS_SDK=1" not in (build / "compile_commands.json").read_text():
        raise ValueError("native AT-SPI was silently omitted")
    # The original MIT notice is not included by the SDK's standard install step.
    notice = prefix / "share/licenses/nexaui"
    notice.mkdir(parents=True)
    (notice / "LICENSE").write_bytes((source / "LICENSE").read_bytes())
    result = {"revision": config["revision"], "version": config["version"],
              "archive_sha256": digest(archive), "model": models, "commands": commands,
              "sanitizers": args.sanitizers, "atspi_compiled": True,
              "dbus_headers": header_hashes, "rpm_signature": signature,
              "artifacts": {str(p.relative_to(work)): digest(p) for p in
                  [consumer / "aetheri_model_view", consumer / "aetheri_model_view_test",
                   prefix / ("lib/libnexaui.so." + config["version"])]},
              "scope": config["adoption"]}
    (work / "build.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"result": "built", "work": str(work), **result}))


if __name__ == "__main__":
    main()
