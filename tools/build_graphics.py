"""Build the reviewed private wlroots profile in a new unprivileged Linux workspace."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile

from prepare_vm import ROOT, verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path, help="directory containing the pinned source archive")
    parser.add_argument("work", type=Path, help="new private directory; never reused or overwritten")
    parser.add_argument("--sanitizers", action="store_true")
    args = parser.parse_args()
    if sys.platform != "linux" or os.getuid() == 0:
        raise ValueError("use an unprivileged Linux build account")
    config = json.loads((ROOT / "spec/graphics-inputs.json").read_text())
    record = next(r for r in config["inputs"] if r["name"] == "wlroots-0.20.2.tar.gz")
    archive = args.cache.resolve() / record["name"]
    verify(archive, record)
    patch = config["pointer_enter_patch"]
    patch_path = ROOT / patch["path"]
    if hashlib.sha256(patch_path.read_bytes()).hexdigest() != patch["sha256"]:
        raise ValueError("reviewed patch bytes changed")
    # Fresh workspace only. Existing builds, partial attempts and evidence remain intact.
    work = args.work.absolute()
    work.mkdir(mode=0o700)
    source = work / "source"
    source.mkdir(mode=0o700)
    with tarfile.open(archive) as tar:
        for entry in tar.getmembers():
            parts = Path(entry.name).parts
            if not parts or parts[0] != "wlroots-0.20.2" or ".." in parts or not (entry.isfile() or entry.isdir()) or entry.size > 1024**2:
                raise ValueError("unexpected upstream archive member")
        tar.extractall(source, filter="data")
    tree = source / "wlroots-0.20.2"
    pointer = tree / "backend/wayland/pointer.c"
    original = pointer.read_bytes()
    if hashlib.sha256(original).hexdigest() != patch["original_sha256"]:
        raise ValueError("pointer source preimage changed")
    original = original.replace(b'#include "backend/wayland.h"\n', b'#include "backend/wayland.h"\n#include "util/time.h"\n', 1)
    insertion = b'''
	/* An enter carries a position even when no motion follows before a click. */
	if (pointer != NULL && output->wlr_output.width > 0 && output->wlr_output.height > 0) {
		struct wlr_pointer_motion_absolute_event event = {
			.pointer = &pointer->wlr_pointer,
			.time_msec = (uint32_t)get_current_time_msec(),
			.x = wl_fixed_to_double(sx) / output->wlr_output.width,
			.y = wl_fixed_to_double(sy) / output->wlr_output.height,
		};
		wl_signal_emit_mutable(&pointer->wlr_pointer.events.motion_absolute, &event);
	}
'''
    original = original.replace(b'\tupdate_wl_output_cursor(output);\n', b'\tupdate_wl_output_cursor(output);\n' + insertion, 1)
    if hashlib.sha256(original).hexdigest() != patch["patched_sha256"]:
        raise ValueError("reviewed patch result changed")
    pointer.write_bytes(original)
    build, prefix = work / "build", work / "prefix"
    options = ["--libdir=lib", "--wrap-mode=nodownload", "-Dauto_features=disabled", "-Dexamples=false",
               "-Dbackends=[]", "-Drenderers=[]", "-Dallocators=[]", "-Dsession=disabled",
               "-Dxwayland=disabled", "-Dcolor-management=disabled"]
    options += ["--buildtype=debugoptimized", "-Db_sanitize=address,undefined"] if args.sanitizers else ["--buildtype=release"]
    commands = [["meson", "setup", str(build), str(tree), "--prefix=" + str(prefix), *options],
                ["meson", "compile", "-C", str(build), "-j2"], ["meson", "install", "-C", str(build)]]
    with (work / "build.log").open("xb") as log:
        for command in commands:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    license_dir = prefix / "share/licenses/wlroots-0.20.2"
    license_dir.mkdir(parents=True)
    with (license_dir / "LICENSE").open("xb") as output:
        output.write((tree / "LICENSE").read_bytes())
    report = {"archive_sha256": record["digest"], "patch": patch, "commands": commands,
              "library_sha256": hashlib.sha256((prefix / "lib/libwlroots-0.20.so").read_bytes()).hexdigest()}
    (work / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Reviewed private wlroots built at", prefix)


if __name__ == "__main__":
    main()
