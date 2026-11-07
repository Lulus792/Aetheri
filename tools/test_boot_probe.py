"""Cold-boot only a verified RAM-only image in a separate diskless QEMU.

No carrier controller, physical device, network, QMP or existing NVRAM is used.
The retained child handle owns timeout retirement; results preserve every attempt.
"""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "local/tools/qemu-11.1.0"


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inspect(image):
    manifest = json.loads((image / "manifest.json").read_text())
    if manifest["format"] != 1 or not re.fullmatch("[0-9a-f]{32}", manifest["nonce"]):
        raise ValueError("unsupported boot proof")
    if set(manifest["outputs"]) != {"vmlinuz", "initramfs.cpio.gz", "cmdline.txt"}:
        raise ValueError("unexpected boot inputs")
    for name, expected in manifest["outputs"].items():
        if (image / name).is_symlink() or sha(image / name) != expected:
            raise ValueError("changed boot input: " + name)
    # Independently parse the actual newc bytes, not the builder's source tree.
    packed = (image / "initramfs.cpio.gz").read_bytes()
    if len(packed) > 32 * 1024**2:
        raise ValueError("oversized boot proof")
    with gzip.GzipFile(fileobj=io.BytesIO(packed)) as stream:
        data = stream.read(64 * 1024**2 + 1)
    if len(data) > 64 * 1024**2:
        raise ValueError("oversized boot root")
    offset = 0
    files = {}
    seen = set()
    while True:
        header = data[offset:offset+110]
        if len(header) != 110 or header[:6] != b"070701":
            raise ValueError("invalid newc header")
        values = [int(header[i:i+8], 16) for i in range(6, 110, 8)]
        _, mode, uid, gid, _, mtime, size, _, _, _, _, namesize, _ = values
        offset += 110
        name = data[offset:offset+namesize]
        if not name.endswith(b"\0") or b"\0" in name[:-1]:
            raise ValueError("invalid archive name")
        name = name[:-1].decode("utf-8")
        offset = (offset+namesize+3) & ~3
        body = data[offset:offset+size]
        offset = (offset+size+3) & ~3
        if len(body) != size:
            raise ValueError("truncated archive")
        if name == "TRAILER!!!":
            if any(data[offset:]):
                raise ValueError("unexamined archive tail")
            break
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts or name in seen or uid or gid or mtime:
            raise ValueError("unexpected archive metadata")
        seen.add(name)
        kind = mode & 0o170000
        if kind == 0o100000:
            files[name] = {"sha256": hashlib.sha256(body).hexdigest(), "mode": oct(mode & 0o7777)}
        elif kind == 0o120000:
            target = body.decode()
            if PurePosixPath(target).is_absolute() or ".." in PurePosixPath(target).parts:
                raise ValueError("unexpected symbolic link")
            files[name] = {"symlink": target}
        elif kind != 0o040000:
            raise ValueError("unexpected archive object")
    root_profile = manifest.get("proof") in ("readonly-root", "writable-data")
    valid_init = "init" not in seen if root_profile else files.get("init") == {"symlink": "usr/lib/systemd/systemd"}
    if files != manifest["root"] or not valid_init:
        raise ValueError("archive does not match original root manifest")
    return manifest


def host_tools():
    receipt = json.loads((ROOT / "local/verification/host-tools.json").read_text())
    paths = [TOOLS / "qemu-system-x86_64.exe", TOOLS / "share/edk2-x86_64-code.fd", TOOLS / "share/edk2-i386-vars.fd", *TOOLS.glob("*.dll")]
    hashes = {}
    for path in paths:
        key = str(path.relative_to(ROOT / "local")).replace("\\", "/")
        # Original provisioning records workspace-relative paths.
        expected = receipt["files"].get(path.relative_to(ROOT).as_posix())
        actual = sha(path)
        if not expected or actual != expected:
            raise ValueError("host tool differs from original extraction: " + str(path))
        hashes[key] = actual
    return hashes


def run(image, output, expect_failure=False, timeout=120):
    manifest = inspect(image)
    hashes = host_tools()
    output.mkdir(mode=0o700)
    nvram = output / "nvram.fd"
    shutil.copyfile(TOOLS / "share/edk2-i386-vars.fd", nvram)
    commandline = (image / "cmdline.txt").read_text().strip()
    if expect_failure:
        replacement = ("0" if manifest["nonce"][0] != "0" else "1") + manifest["nonce"][1:]
        commandline = commandline.replace(manifest["nonce"], replacement)
    command = [str(TOOLS / "qemu-system-x86_64.exe"), "-name", "aetheri-diskless-boot-proof", "-machine", "pc-q35-11.1,accel=tcg", "-cpu", "max", "-smp", "2", "-m", "768M", "-no-user-config", "-nodefaults", "-display", "none", "-monitor", "none", "-nic", "none", "-no-reboot", "-drive", "if=pflash,format=raw,readonly=on,file=" + str(TOOLS / "share/edk2-x86_64-code.fd"), "-drive", "if=pflash,format=raw,file=" + str(nvram), "-kernel", str(image / "vmlinuz"), "-initrd", str(image / "initramfs.cpio.gz"), "-append", commandline, "-serial", "file:" + str(output / "serial.log")]
    observe(output, command, manifest, hashes, sha(image / "manifest.json"), expect_failure, timeout)


def evaluate(serial, manifest, exit_code, timed_out=False, expect_failure=False):
    if manifest.get("proof") == "writable-data":
        return evaluate_data(serial, manifest, exit_code, timed_out, expect_failure)
    is_root = manifest.get("proof") == "readonly-root"
    prefix = "AETHERI_ROOT" if is_root else "AETHERI_BOOT"
    service = "aetheri-root.service" if is_root else "aetheri-boot.service"
    filesystem = "ext4_ro=1 write_erofs=1" if is_root else "ram_root=1"
    positive = re.findall(r"^"+prefix+r"_PASS nonce=" + manifest["nonce"] + r" kernel=" + re.escape(manifest["kernel_release"]) + r" arch=x86_64 pid1=systemd uefi=1 "+filesystem+r" cgroup2=1 boot=([0-9a-f-]{36})\s*$", serial, re.MULTILINE)
    all_positive = re.findall(r"^AETHERI_(?:BOOT|ROOT|DATA)_PASS .*?$", serial, re.MULTILINE)
    failures = re.findall(r"^"+prefix+r"_FAIL reason=(\S+)\s*$", serial, re.MULTILINE)
    all_failures = re.findall(r"^AETHERI_(?:BOOT|ROOT|DATA)_FAIL reason=\S+\s*$", serial, re.MULTILINE)
    powered_off = "reboot: Power down" in serial
    if expect_failure:
        result = not all_positive and len(all_failures) == 1 and failures == ["nonce"] and service+": Failed with result 'exit-code'." in serial
    else:
        result = len(positive) == len(all_positive) == 1 and not all_failures and service+": Deactivated successfully." in serial
    return bool(exit_code == 0 and not timed_out and powered_off and result), positive, failures


def evaluate_data(serial, manifest, exit_code, timed_out=False, expect_failure=False):
    """A root preflight alone cannot satisfy a persistent user-data boot."""
    nonce = re.escape(manifest['nonce'])
    identifier = r"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"
    roots = re.findall(r"^AETHERI_ROOT_PASS nonce="+nonce+r" kernel="+re.escape(manifest['kernel_release'])+r" arch=x86_64 pid1=systemd uefi=1 ext4_ro=1 write_erofs=1 cgroup2=1 boot="+identifier+r"\s*$", serial, re.MULTILINE)
    users = re.findall(r"^AETHERI_DATA_USER nonce="+nonce+r" counter=([1-9][0-9]{0,5}) user=1000 cap_zero=1 tmp=1 runtime=1 boot="+identifier+r"\s*$", serial, re.MULTILINE)
    results = re.findall(r"^AETHERI_DATA_PASS nonce="+nonce+r" counter=([1-9][0-9]{0,5}) user=1000 cap_zero=1 persisted=1 unmounted=1 boot="+identifier+r"\s*$", serial, re.MULTILINE)
    checks = re.findall(r"^AETHERI_DATA_CHECK nonce="+nonce+r"\s*$", serial, re.MULTILINE)
    positives = re.findall(r"^AETHERI_\w+_PASS .*?$", serial, re.MULTILINE)
    failures = re.findall(r"^AETHERI_\w+_FAIL reason=(\S+)\s*$", serial, re.MULTILINE)
    all_users = re.findall(r"^AETHERI_DATA_USER .*?$", serial, re.MULTILINE)
    if expect_failure:
        result = expect_failure == 'volume-context' and len(roots) == len(positives) == 1 and not checks and not all_users and failures == ['volume-context'] and "AETHERI_DATA_FAIL reason=volume-context" in serial and "aetheri-data-check.service: Failed with result 'exit-code'." in serial
    else:
        result = len(roots) == len(users) == len(results) == len(checks) == len(all_users) == 1 and len(positives) == 2 and not failures and users == results and roots[0] == results[0][1] and 'aetheri-data-finish.service: Deactivated successfully.' in serial and 'aetheri-data-user.service: Deactivated successfully.' in serial
    accepted = exit_code == 0 and not timed_out and 'reboot: Power down' in serial and result
    return bool(accepted), roots, failures


def observe(output, command, manifest, hashes, manifest_digest, expect_failure=False, timeout=120):
    """Observe one original owned test child; callers supply admitted fixed inputs."""
    state = dict(state="starting", command=command, manifest_sha256=manifest_digest, host_tools=hashes, expected_failure=expect_failure, observer_sha256=sha(Path(__file__)))

    def record():
        (output / "attempt.json").write_text(json.dumps(state, indent=2) + "\n")

    record()
    started = time.monotonic()
    with (output / "qemu.log").open("xb") as log:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log, stderr=log, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        state.update(state="running", pid=process.pid)
        record()
        print(json.dumps({"pid": process.pid, "attempt": str(output)}), flush=True)
        try:
            while process.poll() is None:
                if time.monotonic()-started >= timeout:
                    state["timeout"] = True
                    process.terminate()  # Only this still-owned child handle.
                    process.wait(timeout=10)
                    break
                time.sleep(0.2)
        finally:
            state.update(state="terminal" if process.poll() is not None else "unresolved", exit=process.poll(), seconds=time.monotonic()-started)
            record()
    serial = (output / "serial.log").read_text(errors="replace")
    accepted, positive, failures = evaluate(serial, manifest, state["exit"], state.get("timeout", False), expect_failure)
    state.update(accepted=accepted, serial_sha256=sha(output / "serial.log"), boot_ids=positive, failures=failures)
    record()
    print(json.dumps({k: state[k] for k in ("state", "exit", "seconds", "accepted", "boot_ids", "failures")}), flush=True)
    if not accepted:
        raise RuntimeError("boot proof failed; inspect the original preserved attempt")
    return state


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expect-nonce-failure", action="store_true")
    args = parser.parse_args()
    run(args.image.resolve(), args.output.absolute(), args.expect_nonce_failure)
