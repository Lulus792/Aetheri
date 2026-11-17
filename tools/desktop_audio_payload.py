"""Stage the selected PipeWire and WirePlumber runtime in an offline image."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

import desktop_payload
import prepare_audio
import prepare_audio_hda
import prepare_audio_policy_candidate as routing


DAEMONS = {
    "usr/bin/pipewire": "/usr/libexec/aetheri/aetheri-pipewire",
    "usr/bin/wireplumber": "/usr/libexec/aetheri/aetheri-wireplumber",
}
CONFIG_ROOT = Path(__file__).resolve().parents[1] / "data/audio"
CONFIG_FILES = ("pipewire.conf", "client.conf", "wireplumber.conf.d/80-aetheri.conf")


def audio_manifest(graph: Path, policy: Path, hda: Path | None = None) -> dict:
    """Project only runtime files, logic, device data and notices from checked prefixes."""
    graph, policy = graph.absolute(), policy.absolute()
    prepare_audio.check(graph)
    routing.check(policy)
    graph_pins, graph_digest = prepare_audio.profile()
    policy_pins, policy_entries, policy_aliases = routing.profile()
    inputs = [
        (graph, (entry for entry in graph_pins["files"] if entry["role"] in ("runtime", "notice")),
         f"pipewire:{graph_digest}"),
        (policy, (entry for entry in policy_entries.values()
                  if entry.get("role", "runtime") in ("runtime", "notice", "device-data")),
         f"wireplumber:{policy_pins['id']}:{routing.lf_hash(routing.PROFILE)}"),
    ]
    hda_aliases = {}
    if hda is not None:
        hda = hda.absolute()
        prepare_audio_hda.check(hda)
        hda_pins, hda_digest = prepare_audio_hda.profile()
        inputs.append((hda, hda_pins["files"], f"alsa:{hda_digest}"))
        hda_aliases = hda_pins["symlinks"]
    selected = {}
    for prefix, rows, profile in inputs:
        for entry in rows:
            path = entry["path"]
            destination = DAEMONS.get(path, "/" + path)
            row = {
                "source": str(prefix / path),
                "destination": destination,
                **{key: entry[key] for key in ("size", "mode", "sha256")},
                "package": entry.get("package"),
                "provenance": None if entry.get("package") else profile,
            }
            if destination in selected:
                original = selected[destination]
                if any(original[key] != row[key] for key in ("size", "mode", "sha256")):
                    raise ValueError(f"conflicting audio inputs for {destination}")
            else:
                selected[destination] = row
    for name in CONFIG_FILES:
        source = CONFIG_ROOT / name
        body = source.read_bytes()
        destination = "/usr/share/aetheri/audio/" + name
        if destination in selected:
            raise ValueError(f"conflicting audio configuration for {destination}")
        selected[destination] = {
            "source": str(source), "destination": destination, "size": len(body),
            "mode": 0o644, "sha256": hashlib.sha256(body).hexdigest(),
            "package": None, "provenance": "Aetheri desktop audio configuration",
        }
    # The unversioned PipeWire .so alias belongs to the SDK. Runtime consumers
    # resolve the SONAME. Keep the routing aliases for their exact same files.
    aliases = {"/usr/lib64/libpipewire-0.3.so.0":
               graph_pins["symlinks"]["usr/lib64/libpipewire-0.3.so.0"]}
    for group in (policy_aliases, hda_aliases):
        for name, target in group.items():
            path = "/" + name
            if path in aliases and aliases[path] != target:
                raise ValueError(f"conflicting audio aliases for {path}")
            aliases[path] = target
    manifest = {"format": 1, "files": [selected[name] for name in sorted(selected)], "symlinks": aliases}
    desktop_payload.validate(manifest)
    if not set(DAEMONS.values()).issubset(selected):
        raise ValueError("incomplete audio daemon selection")
    return manifest


def install(tree: Path, graph: Path, policy: Path, hda: Path | None = None) -> dict:
    tree = tree.absolute()
    if tree == Path(tree.anchor) or tree.resolve(strict=True) != tree:
        raise ValueError("select an offline image tree")
    if any(tree == prefix.absolute() or tree.is_relative_to(prefix.absolute())
           or prefix.absolute().is_relative_to(tree) for prefix in (graph, policy, hda)
           if prefix is not None):
        raise ValueError("audio image tree overlaps an input prefix")
    manifest = audio_manifest(graph, policy, hda)
    result = desktop_payload.install(tree, manifest)
    return {
        **result,
        "files_by_path": {
            entry["destination"]: {key: entry[key] for key in ("size", "sha256", "mode", "package", "provenance")}
            for entry in manifest["files"]
        },
        "symlinks": manifest["symlinks"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-root", required=True, type=Path)
    parser.add_argument("--pipewire-prefix", required=True, type=Path)
    parser.add_argument("--wireplumber-prefix", required=True, type=Path)
    parser.add_argument("--hda-prefix", type=Path)
    args = parser.parse_args()
    try:
        result = install(args.image_root, args.pipewire_prefix, args.wireplumber_prefix, args.hda_prefix)
    except (OSError, ValueError, KeyError) as error:
        print(f"desktop audio: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
