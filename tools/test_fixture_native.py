"""Check native canonical decoding against independent frozen bytes and field views.

No filesystem importer, hash-library verification, trust enrollment or app launch.
The batch archive is test input only; decoded metadata is compared field by field.
"""
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

import runtime_contracts as reference


ROOT = Path(__file__).resolve().parents[1]


def word(value):
    return struct.pack(">I", value)


def string(value):
    data = value if isinstance(value, bytes) else value.encode("utf-8")
    return word(len(data)) + data


def prefix(value):
    return (b"AETHFIX1" + string(value["component"]) + bytes.fromhex(value["publisher"])
            + string(value["version"]) + string(value["target"]))


def unchecked(value):
    """Deliberately retain invalid values/order for negative wire fixtures."""
    data = prefix(value) + word(len(value["files"]))
    for file in value["files"]:
        data += string(file["path"]) + struct.pack(">IQ", file["mode"], file["size"]) + bytes.fromhex(file["sha256"])
    data += word(len(value["actions"]))
    for action in value["actions"]:
        data += string(action["id"]) + string(action["executable"]) + word(len(action["arguments"]))
        data += b"".join(string(arg) for arg in action["arguments"])
    return data + word(len(value["dependencies"])) + b"".join(bytes.fromhex(dep) for dep in value["dependencies"])


def expected(value):
    def h(text):
        return text.encode("utf-8").hex()
    files = sorted(value["files"], key=lambda file: file["path"])
    actions = sorted(value["actions"], key=lambda action: action["id"])
    lines = ["component " + h(value["component"]), "publisher " + value["publisher"],
             "version " + h(value["version"]), "target " + h(value["target"]), "files " + str(len(files))]
    lines += [f"f {h(file['path'])} {file['mode']} {file['size']} {file['sha256']}" for file in files]
    lines.append("actions " + str(len(actions)))
    for action in actions:
        index = next(n for n, file in enumerate(files) if file["path"] == action["executable"])
        lines.append(f"a {h(action['id'])} {index} {len(action['arguments'])}")
        lines += ["arg " + h(arg) for arg in action["arguments"]]
    lines.append("dependencies " + str(len(value["dependencies"])))
    lines += ["d " + dep for dep in sorted(value["dependencies"])]
    return "\n".join(lines + ["END"]) + "\n"


def main():
    executable = Path(sys.argv[1]).resolve(strict=True)
    build = Path(sys.argv[2]).resolve(strict=True)
    if executable.parent != build:
        raise ValueError("test executable must belong to its build directory")
    frozen = next(row for row in json.loads((ROOT / "spec/vectors/runtime-v1.json").read_text()) if row["kind"] == "fixture")
    base = frozen["input"]
    cases = []
    def valid(value):
        encoded = reference.fixture(value)
        cases.append((True, encoded, expected(value)))
    def invalid(value):
        cases.append((False, value if isinstance(value, bytes) else unchecked(value), "REJECT\n"))
    def changed(key, value):
        item = copy.deepcopy(base)
        item[key] = value
        return item
    assert reference.fixture(base).hex() == frozen["hex"]
    valid(base)
    minimal = copy.deepcopy(base)
    minimal.update(files=[dict(path="zero", mode=0o444, size=0, sha256="00" * 32)], actions=[], dependencies=[])
    valid(minimal)
    unicode = changed("version", "é e\u0301 🚀 \uffff")
    unicode["actions"][0]["arguments"] = ["", "space and 'quotes'", '"back\\slash"', "最大", "🚀"]
    valid(unicode)
    valid(changed("version", "\u00a0\u07ff\u0800\ud7ff\ue000\uffff\U00010000\U0010ffff"))
    action_boundary = copy.deepcopy(base)
    action_boundary["actions"][0].update(id="a" * 64, arguments=[])
    valid(action_boundary)
    limits = copy.deepcopy(base)
    limits.update(component="x" * 128, version="🚀" * 32,
                  files=[dict(path=f"bin/{n:02d}", mode=0o555, size=1024**2, sha256=f"{n:064x}") for n in range(32)],
                  actions=[dict(id=f"a{n}", executable=f"bin/{31-n:02d}", arguments=["x" * 256] * 8) for n in range(8)],
                  dependencies=[f"{n:064x}" for n in range(8)])
    valid(limits)
    longest = copy.deepcopy(minimal)
    longest["files"][0]["path"] = "/".join(["a" * 64, "b" * 64, "c" * 64, "d" * 61])
    valid(longest)
    pair = copy.deepcopy(base)
    for file in pair["files"]:
        file["size"] = 16 * 1024**2
    valid(pair)
    # The encoder may accept unsorted input; the native wire decoder must not.
    invalid(changed("files", list(reversed(base["files"]))))
    invalid(changed("files", base["files"] * 2))
    invalid(changed("files", []))
    invalid(changed("files", [dict(path=f"f{n:02d}", mode=0o444, size=0, sha256="00" * 32) for n in range(33)]))
    ancestor = [dict(path=path, mode=0o444, size=0, sha256="00" * 32) for path in ("a", "a-keep", "a/child")]
    invalid(changed("files", ancestor))
    for path in ("", "/root", ".", "..", "a/../b", "a/./b", "a//b", "a/", "a\\b", "a\0b", "é", "a" * 65, "a/" * 128 + "b"):
        value = copy.deepcopy(minimal); value["files"][0]["path"] = path; invalid(value)
    for mode in (0, 0o644, 0o755, 0o4555, 0xffffffff):
        value = copy.deepcopy(base); value["files"][0]["mode"] = mode; invalid(value)
    for size in (16 * 1024**2 + 1, 2**63, 2**64 - 1):
        value = copy.deepcopy(base); value["files"][0]["size"] = size; invalid(value)
    over_total = copy.deepcopy(pair)
    over_total["files"].append(dict(path="z", mode=0o444, size=1, sha256="00" * 32)); invalid(over_total)
    for key, values in (("component", ("", "A", "a/b", "x" * 129)),
                        ("version", ("", "x" * 129)), ("target", ("", "aarch64-linux-gnu", "x86_64-linux-musl")),
                        ("publisher", ("00" * 16,))):
        for value in values:
            invalid(changed(key, value))
    for bad in (b"\0", b"\x1f", b"\x7f", b"\xc2\x80", b"\xc2\x9f", b"\xc0\xaf", b"\xc1\xbf", b"\x80", b"\xe0\x80\xa0", b"\xed\xa0\x80", b"\xf4\x90\x80\x80", b"\xf5\x80\x80\x80", b"\xe2\x82"):
        invalid(changed("version", bad))
        value = copy.deepcopy(base); value["actions"][0]["arguments"] = [bad]; invalid(value)
    for key, value in (("id", ""), ("id", "X"), ("id", "x" * 65), ("executable", "missing"), ("executable", "share/message"),
                       ("arguments", ["x"] * 9), ("arguments", ["x" * 257])):
        item = copy.deepcopy(base); item["actions"][0][key] = value; invalid(item)
    invalid(changed("actions", base["actions"] * 2))
    invalid(changed("actions", [dict(id=f"a{n}", executable="bin/probe", arguments=[]) for n in range(9)]))
    invalid(changed("actions", [dict(id=id, executable="bin/probe", arguments=[]) for id in ("z", "a")]))
    for dependencies in (["11" * 32, "00" * 32], ["00" * 32] * 2, [f"{n:064x}" for n in range(9)]):
        invalid(changed("dependencies", dependencies))
    data = bytes.fromhex(frozen["hex"])
    invalid(data + b"\0"); invalid(b"AETHFIX2" + data[8:]); invalid(b"x" * (reference.MAX_RECORD + 1))
    # Hostile lengths/counts are rejected before allocation or payload iteration.
    invalid(data[:8] + word(0xffffffff) + data[12:])
    head = prefix(base)
    invalid(head + word(0xffffffff) + data[len(head) + 4:])
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=build) as temporary:
        directory = Path(temporary)
        if directory.resolve().parent != build:
            raise ValueError("temporary case archive outside build")
        archive = directory / "cases.bin"
        archive.write_bytes(word(len(cases)) + b"".join(word(int(accepted)) + word(len(payload)) + payload for accepted, payload, _ in cases))
        result = subprocess.run([str(executable), str(archive)], check=True, capture_output=True, text=True, timeout=30)
        if result.stdout != "".join(output for _, _, output in cases):
            raise AssertionError("decoded metadata differs from independent expected fields")
        if result.stderr:
            raise AssertionError("unexpected native diagnostics: " + result.stderr)
    print(f"native fixture: {sum(accepted for accepted, _, _ in cases)} valid field projections, {sum(not accepted for accepted, _, _ in cases)} malformed cases and every frozen-vector truncation passed")


if __name__ == "__main__":
    main()
