"""Stage the selected PipeWire and WirePlumber runtime in an offline image."""

import argparse
import json
from pathlib import Path
import sys

import desktop_payload
import prepare_audio
import prepare_audio_policy_candidate as routing


DAEMONS = {
    "usr/bin/pipewire": "/usr/libexec/aetheri/aetheri-pipewire",
    "usr/bin/wireplumber": "/usr/libexec/aetheri/aetheri-wireplumber",
}


def audio_manifest(graph: Path, policy: Path) -> dict:
    """Project only runtime files, logic, device data and notices from checked prefixes."""
    graph, policy = graph.absolute(), policy.absolute()
    prepare_audio.check(graph)
    routing.check(policy)
    graph_pins, graph_digest = prepare_audio.profile()
    policy_pins, policy_entries, policy_aliases = routing.profile()
    selected = {}
    for prefix, rows, profile in (
        (graph, (entry for entry in graph_pins["files"] if entry["role"] in ("runtime", "notice")),
         f"pipewire:{graph_digest}"),
        (policy, (entry for entry in policy_entries.values()
                  if entry.get("role", "runtime") in ("runtime", "notice", "device-data")),
         f"wireplumber:{policy_pins['id']}:{routing.lf_hash(routing.PROFILE)}"),
    ):
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
    # The unversioned PipeWire .so alias belongs to the SDK. Runtime consumers
    # resolve the SONAME. Keep the routing aliases for their exact same files.
    aliases = {"/usr/lib64/libpipewire-0.3.so.0":
               graph_pins["symlinks"]["usr/lib64/libpipewire-0.3.so.0"]}
    for name, target in policy_aliases.items():
        path = "/" + name
        if path in aliases and aliases[path] != target:
            raise ValueError(f"conflicting audio aliases for {path}")
        aliases[path] = target
    manifest = {"format": 1, "files": [selected[name] for name in sorted(selected)], "symlinks": aliases}
    desktop_payload.validate(manifest)
    if not set(DAEMONS.values()).issubset(selected):
        raise ValueError("incomplete audio daemon selection")
    return manifest


def install(tree: Path, graph: Path, policy: Path) -> dict:
    tree = tree.absolute()
    if tree == Path(tree.anchor) or tree.resolve(strict=True) != tree:
        raise ValueError("select an offline image tree")
    if any(tree == prefix.absolute() or tree.is_relative_to(prefix.absolute())
           or prefix.absolute().is_relative_to(tree) for prefix in (graph, policy)):
        raise ValueError("audio image tree overlaps an input prefix")
    manifest = audio_manifest(graph, policy)
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
    args = parser.parse_args()
    try:
        result = install(args.image_root, args.pipewire_prefix, args.wireplumber_prefix)
    except (OSError, ValueError, KeyError) as error:
        print(f"desktop audio: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
