"""Build the pinned private Wayland/OpenGL ES/text profile, without global installation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile

from prepare_vm import ROOT, verify


def prepare_patch_tool(cache, destination, config):
    """Read one signed/pinned build-tool payload; do not install an RPM."""
    selected = config["build_tools"]["patch"]
    package = next(r for r in config["inputs"] if r["name"] == selected["input"])
    rpm = cache / package["name"]
    verify(rpm, package)
    signature = subprocess.check_output(["rpmkeys", "--checksig", "--verbose", str(rpm)], text=True)
    fingerprints = re.findall(r"key fingerprint: ([0-9a-fA-F]{40}): OK", signature)
    if [value.upper() for value in fingerprints] != [selected["signer"].upper()] or "Payload SHA256 digest: OK" not in signature:
        raise ValueError("private patch tool signer/digest not verified")
    nevra = subprocess.check_output(["rpm", "-qp", "--qf", "%{NAME}-%{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH}", str(rpm)], text=True)
    if nevra != selected["nevra"]:
        raise ValueError("private patch tool version changed")
    for name, expected in selected["runtime"].items():
        actual = subprocess.check_output(["rpm", "-q", "--qf", "%{NAME}-%{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH}", name], text=True)
        if actual != expected:
            raise ValueError("patch tool runtime changed: " + name)
    for name, expected in selected["runtime_artifacts"].items():
        if hashlib.sha256(Path(name).read_bytes()).hexdigest() != expected:
            raise ValueError("patch tool runtime artifact changed: " + name)
    archive = subprocess.check_output(["rpm2cpio", str(rpm)])
    payload = subprocess.check_output(["cpio", "-i", "--quiet", "--to-stdout", "." + selected["binary"]], input=archive)
    if len(payload) != selected["binary_size"] or hashlib.sha256(payload).hexdigest() != selected["binary_sha256"]:
        raise ValueError("private patch tool payload changed")
    destination.mkdir(mode=0o700)
    binary = destination / "patch"
    binary.write_bytes(payload)
    binary.chmod(0o700)
    licenses = {}
    for name in selected["licenses"]:
        data = subprocess.check_output(["cpio", "-i", "--quiet", "--to-stdout", "." + name], input=archive)
        if not data or len(data) > 128 * 1024:
            raise ValueError("unexpected patch tool license")
        (destination / Path(name).name).write_bytes(data)
        licenses[name] = hashlib.sha256(data).hexdigest()
    return {"binary": str(binary), "binary_sha256": selected["binary_sha256"],
            "package": package, "signature": signature, "licenses": licenses}


def apply_wayland_wakeup_patch(sdl_source, record, tool, log, repository=ROOT):
    patch = repository / record["path"]
    if hashlib.sha256(patch.read_bytes()).hexdigest() != record["sha256"]:
        raise ValueError("reviewed Wayland wakeup patch changed")
    binary = Path(tool["binary"])
    if hashlib.sha256(binary.read_bytes()).hexdigest() != tool["binary_sha256"]:
        raise ValueError("private patch tool changed")
    for name, hashes in record["sources"].items():
        if hashlib.sha256((sdl_source / name).read_bytes()).hexdigest() != hashes["original_sha256"]:
            raise ValueError("Wayland wakeup preimage changed: " + name)
    observations = []
    for dry_run in (True, False):
        command = [str(binary), "--batch", "--forward", "--fuzz=0", "--no-backup-if-mismatch", "-p1", "-i", str(patch)]
        if dry_run:
            command.append("--dry-run")
        done = subprocess.run(command, cwd=sdl_source, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=30)
        observations.append({"command": command, "code": done.returncode, "stdout": done.stdout, "stderr": done.stderr})
        log.write_text(json.dumps(observations, indent=2) + "\n")
        if done.returncode:
            raise ValueError("Wayland wakeup patch refused; retain private source and " + str(log))
    for name, hashes in record["sources"].items():
        if hashlib.sha256((sdl_source / name).read_bytes()).hexdigest() != hashes["patched_sha256"]:
            raise ValueError("Wayland wakeup result changed: " + name)


def apply_event_wakeup_patch(sdl_source, record, repository=ROOT):
    """Apply only the reviewed SDL preimage/result in a fresh private source tree."""
    if hashlib.sha256((repository / record["path"]).read_bytes()).hexdigest() != record["sha256"]:
        raise ValueError("reviewed SDL wakeup patch changed")
    events = sdl_source / record["source"]
    original = events.read_bytes()
    if hashlib.sha256(original).hexdigest() != record["original_sha256"]:
        raise ValueError("SDL wakeup patch preimage changed")
    before = b'        SDL_SetAtomicPointer(&_this->wakeup_window, wakeup_window);\n        status = _this->WaitEventTimeout(_this, loop_timeoutNS);'
    after = b'''        SDL_SetAtomicPointer(&_this->wakeup_window, wakeup_window);
        // A producer may have queued an event before the wakeup window was armed.
        // Recheck after arming so that event cannot be stranded in a native wait.
        status = SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
        if (status != 0) {
            SDL_SetAtomicPointer(&_this->wakeup_window, NULL);
            return status;
        }
        status = _this->WaitEventTimeout(_this, loop_timeoutNS);'''
    if original.count(before) != 1:
        raise ValueError("SDL wakeup patch anchor changed")
    patched = original.replace(before, after, 1)
    if hashlib.sha256(patched).hexdigest() != record["patched_sha256"]:
        raise ValueError("reviewed SDL wakeup patch result changed")
    events.write_bytes(patched)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path)
    parser.add_argument("work", type=Path, help="fresh workspace; partial attempts are preserved")
    parser.add_argument("--sanitizers", action="store_true")
    args = parser.parse_args()
    if sys.platform != "linux" or os.getuid() == 0:
        raise ValueError("use an unprivileged Linux build account")
    config = json.loads((ROOT / "spec/ui-inputs.json").read_text())
    records = [r for r in config["inputs"] if "archive_root" in r]
    for record in records:
        verify(args.cache / record["name"], record)
    patch = config["optional_data_device_patch"]
    if hashlib.sha256((ROOT / patch["path"]).read_bytes()).hexdigest() != patch["sha256"]:
        raise ValueError("reviewed SDL patch changed")
    sdk_record = next(r for r in config["inputs"] if r["name"] == config["graphics_sdk"]["headers"])
    sdk_rpm = args.cache / sdk_record["name"]
    verify(sdk_rpm, sdk_record)
    for package in config["graphics_sdk"]["runtime"]:
        evr = subprocess.check_output(["rpm", "-q", "--qf", "%{EPOCHNUM}:%{VERSION}-%{RELEASE}", package], text=True)
        if evr != config["graphics_sdk"]["runtime_evr"]:
            raise ValueError("reviewed graphics runtime changed")
    font = config["font"]
    if hashlib.sha256(Path(font["path"]).read_bytes()).hexdigest() != font["sha256"]:
        raise ValueError("reviewed font bytes changed")
    work = args.work.absolute()
    work.mkdir(mode=0o700)
    source, prefix = work / "source", work / "prefix"
    source.mkdir(mode=0o700)
    for record in records:
        with tarfile.open(args.cache / record["name"]) as archive:
            members = archive.getmembers()
            if len(members) > 10000 or sum(m.size for m in members) > 200 * 1024**2:
                raise ValueError("unexpected archive size")
            for m in members:
                parts = Path(m.name).parts
                if not parts or parts[0] != record["archive_root"] or ".." in parts or m.size > 8 * 1024**2:
                    raise ValueError("unexpected archive member")
                if not (m.isfile() or m.isdir() or m.issym()):
                    raise ValueError("unexpected archive type")
            # Internal symlinks in the pinned archives are validated by the data filter.
            archive.extractall(source, filter="data")
    events = source / "SDL3-3.4.20" / patch["source"]
    original = events.read_bytes()
    if hashlib.sha256(original).hexdigest() != patch["original_sha256"]:
        raise ValueError("SDL patch preimage changed")
    original = original.replace(
        b'    // The spec says that data offers are no longer valid when keyboard focus is lost.\n    if (seat->data_device->selection_offer) {',
        b'    // The spec says that data offers are no longer valid when keyboard focus is lost.\n    if (seat->data_device && seat->data_device->selection_offer) {', 1)
    if hashlib.sha256(original).hexdigest() != patch["patched_sha256"]:
        raise ValueError("reviewed SDL patch result changed")
    events.write_bytes(original)
    wakeup_patch = config["event_wakeup_patch"]
    apply_event_wakeup_patch(source / "SDL3-3.4.20", wakeup_patch)
    patch_tool = prepare_patch_tool(args.cache, work / "patch-tool", config)
    wayland_wakeup_patch = config["wayland_wakeup_patch"]
    apply_wayland_wakeup_patch(source / "SDL3-3.4.20", wayland_wakeup_patch, patch_tool,
                              work / "wayland-wakeup-patch.json")
    # Read only reviewed EGL/GLES2/KHR headers from a pinned RPM; never install
    # the broad GLX/X11 development package or execute package scriptlets.
    sdk = work / "graphics-sdk"
    headers = subprocess.check_output(["rpm", "-qpl", str(sdk_rpm)], text=True).splitlines()
    cpio = subprocess.check_output(["rpm2cpio", str(sdk_rpm)])
    for name in headers:
        if not name.endswith(".h") or not any(name.startswith("/usr/include/" + d + "/") for d in ("EGL", "GLES2", "KHR")):
            continue
        relative = Path(name).relative_to("/usr/include")
        if ".." in relative.parts:
            raise ValueError("unexpected header path")
        data = subprocess.check_output(["cpio", "-i", "--quiet", "--to-stdout", "." + name], input=cpio)
        if not data or len(data) > 2 * 1024**2:
            raise ValueError("unexpected header bytes")
        dest = sdk / "include" / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
    pc = sdk / "lib/pkgconfig"
    pc.mkdir(parents=True)
    for name, library, version in (("egl", "EGL", "1.5"), ("glesv2", "GLESv2", "3.2")):
        soname = "lib" + library + (".so.1" if library == "EGL" else ".so.2")
        target = Path("/usr/lib64") / soname
        if not target.is_file():
            raise ValueError("reviewed graphics runtime missing")
        (sdk / "lib" / ("lib" + library + ".so")).symlink_to(target)
        (pc / (name + ".pc")).write_text(
            f"prefix={sdk}\nlibdir=${{prefix}}/lib\nincludedir=${{prefix}}/include\n"
            f"Name: {name}\nDescription: private reviewed headers and installed guest runtime\nVersion: {version}\n"
            f"Libs: -L${{libdir}} -l{library}\nCflags: -I${{includedir}}\n")
    env = os.environ.copy()
    env["PKG_CONFIG_PATH"] = str(prefix / "lib/pkgconfig") + ":" + str(pc)
    env["LD_LIBRARY_PATH"] = str(prefix / "lib")
    build_type = "RelWithDebInfo" if args.sanitizers else "Release"
    flags = "-fsanitize=address,undefined -fno-omit-frame-pointer" if args.sanitizers else ""
    commands = []

    def run(command, log):
        commands.append(command)
        subprocess.run(command, check=True, env=env, stdout=log, stderr=subprocess.STDOUT)

    def cmake(name, root, options, log):
        build = work / (name + "-build")
        run(["cmake", "-G", "Ninja", "-S", str(source / root), "-B", str(build),
             "-DCMAKE_BUILD_TYPE=" + build_type, "-DCMAKE_INSTALL_PREFIX=" + str(prefix),
             "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_PREFIX_PATH=" + str(prefix) + ";" + str(sdk),
             "-DCMAKE_INSTALL_RPATH=" + str(prefix / "lib"),
             "-DCMAKE_C_FLAGS=" + flags, "-DCMAKE_CXX_FLAGS=" + flags,
             "-DBUILD_SHARED_LIBS=ON", *options], log)
        run(["cmake", "--build", str(build), "-j2"], log)
        run(["cmake", "--install", str(build)], log)

    with (work / "build.log").open("xb") as log:
        cmake("freetype", "freetype-VER-2-15-0", ["-DFT_DISABLE_" + d + "=ON" for d in
              ("ZLIB", "BZIP2", "PNG", "HARFBUZZ", "BROTLI", "HVF")], log)
        hb_build = work / "harfbuzz-build"
        options = ["--libdir=lib", "--wrap-mode=nodownload", "-Dauto_features=disabled",
                   "-Dfreetype=enabled", "-Draster=disabled", "-Dvector=disabled", "-Dgpu=disabled",
                   "-Dsubset=disabled", "-Dutilities=disabled", "-Dtests=disabled", "-Ddocs=disabled",
                   "-Ddefault_library=shared"]
        options += ["--buildtype=debugoptimized", "-Db_sanitize=address,undefined"] if args.sanitizers else ["--buildtype=release"]
        run(["meson", "setup", str(hb_build), str(source / "harfbuzz-15.0.0"), "--prefix=" + str(prefix), *options], log)
        run(["meson", "compile", "-C", str(hb_build), "-j2"], log)
        run(["meson", "install", "-C", str(hb_build)], log)
        sdl_off = ("AUDIO", "GPU", "CAMERA", "JOYSTICK", "HAPTIC", "HIDAPI", "SENSOR", "X11",
                   "OPENGL", "VULKAN", "KMSDRM", "OFFSCREEN", "DUMMYVIDEO",
                   "WAYLAND_LIBDECOR", "TEST_LIBRARY", "TESTS", "EXAMPLES", "STATIC")
        cmake("sdl", "SDL3-3.4.20", ["-DSDL_" + d + "=OFF" for d in sdl_off] +
              ["-DSDL_SHARED=ON", "-DSDL_WAYLAND=ON", "-DSDL_WAYLAND_SHARED=OFF", "-DSDL_OPENGLES=ON", "-DSDL_RENDER=ON"], log)
        cmake("sdl-ttf", "SDL3_ttf-3.2.2", ["-DSDLTTF_VENDORED=OFF", "-DSDLTTF_HARFBUZZ=ON",
              "-DSDLTTF_PLUTOSVG=OFF", "-DSDLTTF_SAMPLES=OFF", "-DSDLTTF_STRICT=ON"], log)
    licenses = {
        "SDL3-3.4.20": ["LICENSE.txt"], "SDL3_ttf-3.2.2": ["LICENSE.txt"],
        "freetype-VER-2-15-0": ["docs/FTL.TXT", "LICENSE.TXT"],
        "harfbuzz-15.0.0": ["COPYING"],
    }
    for root, files in licenses.items():
        dest = prefix / "share/licenses" / root
        dest.mkdir(parents=True)
        for file in files:
            shutil.copyfile(source / root / file, dest / Path(file).name)
    fonts = prefix / "share/aetheri-fonts"
    fonts.mkdir(parents=True)
    shutil.copyfile(font["path"], fonts / "LiberationSans-Regular.ttf")
    shutil.copyfile(font["license_path"], fonts / "LICENSE")
    report = {"inputs": records, "patch": patch, "event_wakeup_patch": wakeup_patch,
              "wayland_wakeup_patch": wayland_wakeup_patch, "patch_tool": patch_tool,
              "graphics_sdk": sdk_record, "font": font, "commands": commands,
              "artifacts": {str(p.relative_to(prefix)): hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in sorted(prefix.rglob("*")) if p.is_file() and not p.is_symlink()}}
    (work / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Private UI profile built at", prefix)


if __name__ == "__main__":
    main()
