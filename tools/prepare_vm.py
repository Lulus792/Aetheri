"""Download/verify pinned P0 inputs only. Never install, execute or start a VM."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import urllib.request


ROOT = Path(__file__).resolve().parents[1]


def verify(path, record):
    if path.is_symlink() or not path.is_file() or path.stat().st_size != record["size"]:
        raise ValueError("missing/changed input size or type: " + record["name"])
    digest = hashlib.new(record["algorithm"])
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    if digest.hexdigest() != record["digest"]:
        raise ValueError("input digest mismatch: " + record["name"])
    return digest.hexdigest()


def fetch(cache, record):
    name = record["name"]
    if re.fullmatch(r"[a-zA-Z0-9._-]+", name) is None or name in (".", ".."):
        raise ValueError("input filename")
    target = cache / name
    if target.exists() or target.is_symlink():
        verify(target, record)
        return "already verified"
    part = cache / (name + ".part")
    # Exclusive creation: never overwrite a failed/interrupted download or an alias.
    with part.open("xb") as output:
        with urllib.request.urlopen(record["url"], timeout=30) as response:
            total = 0
            while chunk := response.read(1024 * 1024):
                total += len(chunk)
                if total > record["size"]:
                    raise ValueError("download larger than pinned size")
                output.write(chunk)
            output.flush()
            os.fsync(output.fileno())
    verify(part, record)
    # Publish without overwriting a concurrently created name on either platform.
    os.link(part, target)
    part.unlink()
    return "downloaded and verified"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("fetch", "verify"))
    args = parser.parse_args()
    config = json.loads((ROOT / "spec/vm-inputs.json").read_text(encoding="utf-8"))
    if config["format"] != 1:
        raise ValueError("unknown input lock format")
    cache = ROOT / "downloads"
    if cache.is_symlink() or (cache.exists() and cache.resolve() != ROOT / "downloads"):
        raise ValueError("cache must remain inside workspace")
    cache.mkdir(exist_ok=True)
    for record in config["inputs"]:
        result = fetch(cache, record) if args.action == "fetch" else verify(cache / record["name"], record)
        print(record["name"] + ": " + result, flush=True)


if __name__ == "__main__":
    main()
