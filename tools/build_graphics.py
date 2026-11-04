"""Build the reviewed private wlroots profile in a new unprivileged Linux workspace."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import tarfile

from prepare_vm import ROOT, verify


def apply_reviewed_patch(tree, record):
    """Apply only exact pinned unified hunks; no shell, fuzzy context or offsets."""
    raw = (ROOT / record["path"]).read_bytes()
    if hashlib.sha256(raw).hexdigest() != record["sha256"]:
        raise ValueError("reviewed integration patch changed")
    lines = raw.splitlines(keepends=True)
    cursor = 0
    for name, hashes in record["files"].items():
        if ".." in Path(name).parts or Path(name).is_absolute():
            raise ValueError("unexpected integration path")
        if lines[cursor:cursor+2] != [b"--- a/" + name.encode() + b"\n", b"+++ b/" + name.encode() + b"\n"]:
            raise ValueError("unexpected integration file header")
        cursor += 2
        path = tree / name
        original = path.read_bytes()
        if hashlib.sha256(original).hexdigest() != hashes["original_sha256"]:
            raise ValueError("integration preimage changed: " + name)
        source, output, pos = original.splitlines(keepends=True), [], 0
        while cursor < len(lines) and lines[cursor].startswith(b"@@ "):
            match = re.fullmatch(rb"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@\n", lines[cursor])
            if not match:
                raise ValueError("unexpected integration hunk")
            old_start, new_start = int(match[1]), int(match[3])
            old_count, new_count = int(match[2] or b"1"), int(match[4] or b"1")
            if old_start < pos + 1 or old_start > len(source) + 1:
                raise ValueError("overlapping integration hunks")
            output.extend(source[pos:old_start-1]); pos = old_start-1
            if len(output) != new_start-1:
                raise ValueError("integration result offset changed")
            consumed = produced = 0
            cursor += 1
            while cursor < len(lines) and lines[cursor][:1] in (b" ", b"+", b"-") and not lines[cursor].startswith(b"--- a/"):
                line = lines[cursor]
                if line[:1] in (b" ", b"-"):
                    if pos >= len(source) or source[pos] != line[1:]:
                        raise ValueError("integration context mismatch")
                    pos += 1; consumed += 1
                if line[:1] in (b" ", b"+"):
                    output.append(line[1:]); produced += 1
                cursor += 1
            if (consumed, produced) != (old_count, new_count):
                raise ValueError("integration hunk length mismatch")
        output.extend(source[pos:])
        result = b"".join(output)
        if hashlib.sha256(result).hexdigest() != hashes["patched_sha256"]:
            raise ValueError("integration result changed: " + name)
        path.write_bytes(result)
    if cursor != len(lines):
        raise ValueError("unconsumed integration patch data")


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
    apply_reviewed_patch(tree, config["clipboard_patch"])
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
    report = {"archive_sha256": record["digest"], "patch": patch, "clipboard_patch": config["clipboard_patch"], "commands": commands,
              "library_sha256": hashlib.sha256((prefix / "lib/libwlroots-0.20.so").read_bytes()).hexdigest()}
    (work / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Reviewed private wlroots built at", prefix)


if __name__ == "__main__":
    main()
