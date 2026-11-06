"""Controlled SDL queue/arm race on a private Wayland compositor; no installation.

Both private SDL variants contain the same Linux backend and one-shot test gate.
Only the candidate has the queue/arm correction. Never use either gated library
as an app profile.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import socket
import subprocess
import sys
import tarfile
import time

from build_ui import (apply_event_wakeup_patch, apply_wayland_wakeup_patch,
                      prepare_patch_tool)
from prepare_vm import ROOT, verify


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path)
    parser.add_argument("reference_ui", type=Path, help="reviewed complete build_ui workspace")
    parser.add_argument("work", type=Path, help="fresh private workspace; failures are retained")
    parser.add_argument("compositor", type=Path, help="original test_compositor_server")
    args = parser.parse_args()
    if sys.platform != "linux" or os.getuid() == 0:
        raise ValueError("use an unprivileged Linux test account")
    config = json.loads((ROOT / "spec/ui-inputs.json").read_text())
    upstream = next(r for r in config["inputs"] if r["name"] == "SDL3-3.4.20.tar.gz")
    archive = args.cache.absolute() / upstream["name"]
    verify(archive, upstream)
    reference = args.reference_ui.absolute()
    metadata = json.loads((reference / "result.json").read_text())
    if upstream not in metadata["inputs"] or metadata["patch"] != config["optional_data_device_patch"]:
        raise ValueError("reviewed reference SDL inputs changed")
    for name, expected in metadata["artifacts"].items():
        if digest(reference / "prefix" / name) != expected:
            raise ValueError("reference UI artifact changed: " + name)
    work = args.work.absolute()
    if any(c in str(work) for c in ('"', '\n', '\r')):
        raise ValueError("unsupported fixture include path")
    work.mkdir(mode=0o700)
    source = work / "source"
    source.mkdir(mode=0o700)
    with tarfile.open(archive) as tar:
        tar.extractall(source, filter="data")  # Complete archive was verified above.
    sdl = source / upstream["archive_root"]
    optional = config["optional_data_device_patch"]
    wayland = sdl / optional["source"]
    if digest(wayland) != optional["original_sha256"]:
        raise ValueError("optional-data-device preimage changed")
    data = wayland.read_bytes().replace(b'    if (seat->data_device->selection_offer) {',
                                     b'    if (seat->data_device && seat->data_device->selection_offer) {', 1)
    if hashlib.sha256(data).hexdigest() != optional["patched_sha256"]:
        raise ValueError("optional-data-device result changed")
    wayland.write_bytes(data)
    # Keep the same Linux backend in both variants: only the queue/arm
    # recheck differs in this controlled counterexample.
    tool = prepare_patch_tool(args.cache.absolute(), work / "patch-tool", config)
    apply_wayland_wakeup_patch(sdl, config["wayland_wakeup_patch"], tool,
                              work / "wayland-wakeup-patch.json")
    events = sdl / "src/events/SDL_events.c"
    original = events.read_bytes()
    apply_event_wakeup_patch(sdl, config["event_wakeup_patch"])
    candidate = events.read_bytes()
    gate = work / "wake-gate.h"
    shutil.copyfile(ROOT / "tests/sdl_wakeup_gate.h", gate)

    def instrument(data):
        before = b"static int SDL_WaitEventTimeout_Device("
        anchor = b"        // No events found in the queue, call WaitEventTimeout to wait for an event.\n"
        if data.count(before) != 1 or data.count(anchor) != 1:
            raise ValueError("controlled gate site changed")
        data = data.replace(before, ('#include "' + str(gate) + '"\n').encode() + before, 1)
        return data.replace(anchor, anchor + b"        aetheri_test_wake_gate();\n", 1)

    commands = []

    def run(command, name):
        commands.append(command)
        with (work / name).open("xb") as log:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)

    command = next(list(c) for c in metadata["commands"] if c[0] == "cmake" and "-S" in c
                   and c[c.index("-S") + 1].endswith("/SDL3-3.4.20"))
    command[command.index("-S") + 1] = str(sdl)
    command[command.index("-B") + 1] = str(work / "build")
    command = ["-DCMAKE_INSTALL_PREFIX=" + str(work / "unused-prefix")
               if c.startswith("-DCMAKE_INSTALL_PREFIX=") else c for c in command]
    events.write_bytes(instrument(original))
    run(command, "configure.log")
    libraries = {}
    for label, data in (("original", original), ("candidate", candidate)):
        fixture = instrument(data)
        events.write_bytes(fixture)
        (work / (label + "-events.c")).write_bytes(fixture)
        run(["cmake", "--build", str(work / "build"), "-j2"], label + "-build.log")
        built = [p for p in (work / "build").glob("libSDL3.so.*") if p.is_file() and not p.is_symlink()]
        if len(built) != 1:
            raise ValueError("unexpected SDL shared library set")
        prefix = work / label
        prefix.mkdir()
        libraries[label] = prefix / "libSDL3.so.0"
        shutil.copyfile(built[0], libraries[label])
    flags = subprocess.check_output(["pkg-config", "--cflags", "--libs", "wayland-client"], text=True).split()
    probe = work / "probe"
    run(["cc", "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
         str(ROOT / "tests/sdl_wakeup_probe.c"), "-I" + str(reference / "prefix/include"),
         "-L" + str(reference / "prefix/lib"), "-Wl,-rpath," + str(reference / "prefix/lib"),
         "-lSDL3", "-pthread", *flags, "-o", str(probe)], "probe-build.log")
    lifetime = work / "lifetime"
    run(["cc", "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
         str(ROOT / "tests/sdl_wakeup_lifetime.c"), "-I" + str(reference / "prefix/include"),
         "-L" + str(reference / "prefix/lib"), "-Wl,-rpath," + str(reference / "prefix/lib"),
         "-lSDL3", "-pthread", "-o", str(lifetime)], "lifetime-build.log")
    fault = work / "eventfd-fault.so"
    run(["cc", "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
         "-DAETHERI_WAKE_FAULT_SHIM", "-fPIC", "-shared",
         str(ROOT / "tests/sdl_wakeup_lifetime.c"), "-o", str(fault)], "fault-build.log")
    results = {}
    cases = [(name, library, probe, {}) for name, library in libraries.items()]
    cases += [("lifetime", libraries["candidate"], lifetime, {}),
              ("failure", libraries["candidate"], lifetime,
               {"LD_PRELOAD": str(fault), "AETHERI_TEST_EVENTFD_FAILURE": "1"})]
    for label, library, program, extra in cases:
        case = work / (label + "-run")
        case.mkdir()
        for name in ("r", "cache", "captures"):
            (case / name).mkdir(mode=0o700)
        env = {**os.environ, "XDG_RUNTIME_DIR": str(case / "r"), "WAYLAND_DISPLAY": "wake",
               "MESA_SHADER_CACHE_DIR": str(case / "cache"), "MESA_SHADER_CACHE_MAX_SIZE": "32M",
               "LIBGL_ALWAYS_SOFTWARE": "1", "LP_NUM_THREADS": "2", "GALLIUM_DRIVER": "llvmpipe", "NO_AT_BRIDGE": "1"}
        for name in ("DISPLAY", "WAYLAND_SOCKET", "LD_PRELOAD", "AETHERI_TEST_WAKE_FD",
                     "SDL_DYNAMIC_API", "SDL_VIDEO_DRIVER", "SDL_RENDER_DRIVER", "AETHERI_TEST_EVENTFD_FAILURE"):
            env.pop(name, None)
        left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        with (case / "server.log").open("xb") as log:
            server = subprocess.Popen([str(args.compositor.absolute()), "--headless", "wake", str(case / "captures"), str(right.fileno())],
                                      env=env, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, pass_fds=(right.fileno(),))
            right.close()
            pin = os.pidfd_open(server.pid)
            try:
                end = time.monotonic() + 8
                while "READY " not in (case / "server.log").read_text():
                    if server.poll() is not None or time.monotonic() >= end:
                        raise ValueError("private compositor startup failed")
                    time.sleep(.01)
                with (case / "probe.log").open("xb") as output:
                    done = subprocess.run([str(program)], env={**env, "LD_LIBRARY_PATH": str(library.parent), **extra},
                                          stdout=output, stderr=subprocess.STDOUT, timeout=8)
                results[label] = {"code": done.returncode, "output": (case / "probe.log").read_text(), "sdl_sha256": digest(library)}
            finally:
                server.terminate()
                status = server.wait(timeout=8)
                ended = bool(select.select([pin], [], [], 0)[0])
                os.close(pin)
                left.close()
                if status != 0 or not ended:
                    raise ValueError("original compositor did not end cleanly")
    result = {"result": "observed", "variants": results, "commands": commands, "probe_sha256": digest(probe),
              "probe_source_sha256": digest(ROOT / "tests/sdl_wakeup_probe.c"),
              "runner_sha256": digest(Path(__file__)), "compositor_sha256": digest(args.compositor.absolute()),
              "reference_metadata_sha256": digest(reference / "result.json"), "archive": upstream,
              "gate_sha256": digest(gate), "patch": config["event_wakeup_patch"],
              "shared_backend_patch": config["wayland_wakeup_patch"], "patch_tool": tool,
              "lifetime_sha256": digest(lifetime), "fault_sha256": digest(fault),
              "lifetime_source_sha256": digest(ROOT / "tests/sdl_wakeup_lifetime.c"),
              "scope": "controlled native gate only; gated libraries are never app profiles"}
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    if results["original"]["code"] != 1 or "queued_after=1" not in results["original"]["output"]:
        raise ValueError("original SDL did not demonstrate the lost wake")
    if results["candidate"]["code"] != 0 or "delivered=1" not in results["candidate"]["output"]:
        raise ValueError("candidate did not deliver the queued event")
    if results["lifetime"]["code"] != 0 or "descriptors_unchanged=1" not in results["lifetime"]["output"]:
        raise ValueError("wake descriptor lifetime failed")
    if results["failure"]["code"] != 0 or "failure_cleanup=1" not in results["failure"]["output"]:
        raise ValueError("wake descriptor allocation failure cleanup failed")
    result["result"] = "passed"
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"result": "passed", "artifacts": str(work)}))


if __name__ == "__main__":
    main()
