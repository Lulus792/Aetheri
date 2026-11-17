"""Stage the pinned HDA kernel modules in an offline desktop image tree."""

import argparse
import hashlib
import json
from pathlib import Path
import stat
import sys

import desktop_payload
import prepare_audio_hda


PROFILE = Path(__file__).resolve().parents[1] / "spec/desktop-hda-inputs.json"
CONFIG = Path("etc/modules-load.d/aetheri-hda.conf")
CONTROLLER = b"snd_hda_intel\n"


def selection(kernel_root: Path) -> tuple[str, dict, str]:
    """Use the module directory layout below an independently chosen kernel root."""
    kernel_root = kernel_root.absolute()
    if kernel_root.resolve(strict=True) != kernel_root or not kernel_root.is_dir():
        raise ValueError("expected a canonical kernel input root")
    base, _ = prepare_audio_hda.profile()
    raw = PROFILE.read_bytes()
    extra = json.loads(raw)
    release = base["kernel_release"]
    if extra["format"] != 1 or extra["kernel_release"] != release:
        raise ValueError("desktop HDA inputs target a different kernel")
    directory = f"usr/lib/modules/{release}"
    records = [dict(entry) for entry in base["kernel_modules"]]
    for codec in extra["codec_modules"]:
        records.append({
            "destination": f"{directory}/{codec['path']}", "size": codec["size"],
            "sha256": codec["sha256"], "package": codec["package"],
            "mode": 0o644, "provenance": None,
        })
    files = []
    for entry in records:
        name = entry["destination"]
        if not name.startswith(f"{directory}/kernel/sound/") or not name.endswith(".ko.xz"):
            raise ValueError("unexpected desktop HDA module destination")
        source = kernel_root / name
        files.append({**entry, "source": str(source), "destination": "/" + name})
    manifest = {"format": 1, "files": files, "symlinks": {}}
    desktop_payload.validate(manifest)
    return release, manifest, hashlib.sha256(raw.replace(b"\r\n", b"\n")).hexdigest()


def install(tree: Path, kernel_root: Path) -> dict:
    tree = tree.absolute()
    release, manifest, supplemental_hash = selection(kernel_root)
    if tree == Path(tree.anchor) or tree.resolve(strict=True) != tree:
        raise ValueError("select an offline image tree")
    directory = tree / "usr/lib/modules" / release
    if directory.resolve(strict=True) != directory or any(
        not stat.S_ISREG((directory / name).lstat().st_mode)
        for name in ("modules.order", "modules.builtin", "modules.builtin.modinfo")
    ):
        raise ValueError("image is missing the selected kernel module inventory")
    config = tree / CONFIG
    # Check the image-owned /etc path explicitly. The payload copier only accepts /usr paths.
    for parent in (tree / "etc", config.parent):
        if not stat.S_ISDIR(parent.lstat().st_mode):
            raise ValueError("HDA configuration parent is not a directory")
    if config.exists() or config.is_symlink():
        raise ValueError("HDA controller configuration already exists")
    installed = desktop_payload.install(tree, manifest)
    with config.open("xb") as stream:
        stream.write(CONTROLLER)
    config.chmod(0o644)
    return {
        "kernel_release": release, "module_count": installed["files"],
        "module_bytes": installed["bytes"], "supplemental_profile_sha256": supplemental_hash,
        "modules": {entry["destination"]: entry["sha256"] for entry in manifest["files"]},
        "configuration": {"/" + CONFIG.as_posix(): hashlib.sha256(CONTROLLER).hexdigest()},
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-root", required=True, type=Path)
    parser.add_argument("--kernel-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = install(args.image_root, args.kernel_root)
    except (OSError, ValueError) as error:
        print(f"desktop HDA: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
