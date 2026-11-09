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
    parser.add_argument("--common-sdk", type=Path, help="checked private Wayland/XKB/FFI developer prefix")
    parser.add_argument("--sdl-sdk", type=Path, help="checked private EGL/GLES/Meson build prefix")
    parser.add_argument("--toolchain", type=Path, help="checked private CMake/C++ build tools; requires both SDK options")
    parser.add_argument("--font-runtime", type=Path, help="checked original GUI snapshot, used only for its font/notices")
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
    selected = (args.common_sdk, args.sdl_sdk, args.toolchain)
    if any(p is not None for p in selected) and not all(p is not None for p in selected):
        raise ValueError("private build requires common SDK, SDL SDK and toolchain together")
    private = None
    if all(p is not None for p in selected):
        import prepare_desktop_sdk as developer_sdk
        import prepare_desktop_toolchain as developer_tools
        common, sdl_sdk, toolchain = [developer_sdk.prefix_path(p) for p in selected]
        private = dict(common=dict(prefix=str(common), check=developer_sdk.check(common)),
                       sdl=dict(prefix=str(sdl_sdk), check=developer_sdk.check(sdl_sdk, 'sdl')),
                       toolchain=dict(prefix=str(toolchain), check=developer_tools.check(toolchain)))
        sdl_profile, _ = developer_sdk.profile('sdl')
        package = next(p for p in sdl_profile['packages'] if p['name'] == sdk_record['name'])
        if package['size'] != sdk_record['size'] or package['sha256'] != sdk_record['digest']:
            raise ValueError("different private EGL/GLES header origin")
    else:
        for package in config["graphics_sdk"]["runtime"]:
            evr = subprocess.check_output(["rpm", "-q", "--qf", "%{EPOCHNUM}:%{VERSION}-%{RELEASE}", package], text=True)
            if evr != config["graphics_sdk"]["runtime_evr"]:
                raise ValueError("reviewed graphics runtime changed")
    font = config["font"]
    font_path, font_license = Path(font["path"]), Path(font["license_path"])
    font_input = None
    if args.font_runtime is not None:
        import prepare_settings_audio_lab as gui
        font_profile = json.loads((ROOT / 'spec/settings-audio-lab-inputs.json').read_bytes())
        gui.check(args.font_runtime, font_profile)
        entry = next(e for e in font_profile['files'] if e['role'] == 'font')
        notice = next(e for e in font_profile['files'] if e['destination'] == font['license_path'])
        font_path = args.font_runtime / entry['destination'].lstrip('/')
        font_license = args.font_runtime / notice['destination'].lstrip('/')
        if entry['sha256'] != font['sha256']:
            raise ValueError("different private font origin")
        font_input = dict(path=str(font_path), license=str(font_license), sha256=entry['sha256'],
                          license_sha256=notice['sha256'], provenance=entry['provenance'])
    if hashlib.sha256(font_path.read_bytes()).hexdigest() != font["sha256"]:
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
    if private is not None:
        sdk = sdl_sdk / 'usr'
        pc = sdk / 'lib64/pkgconfig'
    else:
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
    install_rpath = str(prefix / 'lib')
    cmake_prefixes = [str(prefix), str(sdk)]
    if private is not None:
        env.pop('PKG_CONFIG_SYSROOT_DIR', None)
        paths = [prefix/'lib/pkgconfig', common/'usr/lib64/pkgconfig', common/'usr/share/pkgconfig', pc]
        env['PKG_CONFIG_PATH'] = ':'.join(map(str, paths))
        env['PKG_CONFIG_LIBDIR'] = env['PKG_CONFIG_PATH']
        env['PATH'] = ':'.join(map(str, [toolchain/'bin', common/'usr/bin', sdl_sdk/'usr/bin'])) + ':/usr/bin:/usr/sbin:/bin:/sbin'
        env['PYTHONPATH'] = str(sdl_sdk/'usr/lib/python3.14/site-packages')
        env['PYTHONDONTWRITEBYTECODE'] = '1'
        env['CC'], env['CXX'] = '/usr/bin/gcc', str(toolchain/'bin/c++')
        libraries = [prefix/'lib', common/'usr/lib64', sdl_sdk/'usr/lib64', toolchain/'usr/lib64']
        env['LD_LIBRARY_PATH'] = ':'.join(map(str, libraries))
        install_rpath = ';'.join(map(str, libraries))
        cmake_prefixes += [str(common/'usr'), str(toolchain/'usr')]
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
             "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_PREFIX_PATH=" + ";".join(cmake_prefixes),
             "-DCMAKE_INSTALL_RPATH=" + install_rpath,
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
        if private is not None:
            runtime_flag = '-Wl,-rpath,'+':'.join(map(str, libraries))
            options += ['-Dc_link_args='+json.dumps([runtime_flag]),
                        '-Dcpp_link_args='+json.dumps([runtime_flag])]
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
    shutil.copyfile(font_path, fonts / "LiberationSans-Regular.ttf")
    shutil.copyfile(font_license, fonts / "LICENSE")
    report = {"inputs": records, "patch": patch, "event_wakeup_patch": wakeup_patch,
              "wayland_wakeup_patch": wayland_wakeup_patch, "patch_tool": patch_tool,
              "graphics_sdk": sdk_record, "font": font, "commands": commands,
              "artifacts": {str(p.relative_to(prefix)): hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in sorted(prefix.rglob("*")) if p.is_file() and not p.is_symlink()}}
    if private is not None:
        report['private_dependencies'] = private
    if font_input is not None:
        report['font_input'] = font_input
    (work / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Private UI profile built at", prefix)


if __name__ == "__main__":
    main()
