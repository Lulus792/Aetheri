"""Control only the disposable P0 guest below vm/p0; no hardware disk arguments."""
import argparse
from contextlib import contextmanager
import csv
import ctypes
from functools import partial
import http.server
import json
import os
from pathlib import Path
import re
import select
import shutil
import socket
import subprocess
import sys
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
VM = ROOT / "vm/p0"
TOOLS = ROOT / "local/tools/qemu-11.1.0"
QEMU = TOOLS / "qemu-system-x86_64.exe"
IMAGE = TOOLS / "qemu-img.exe"
ARCHIVER = ROOT / "local/tools/7zip-26.05/7z.exe"
SSH_PORT, HTTP_PORT = 19222, 19446
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


def owned(path):
    path = Path(path)
    if path.is_symlink() or not path.resolve().is_relative_to(ROOT):
        raise ValueError("path outside workspace")
    return path


@contextmanager
def launch_guard():
    owned(VM).mkdir(parents=True, exist_ok=True)
    with owned(VM / "launch.lock").open("a+b") as lock:
        if lock.seek(0, os.SEEK_END) == 0:
            lock.write(b"\0")
            lock.flush()
        lock.seek(0)
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        try:
            yield
        finally:
            lock.seek(0)
            if os.name == "nt":
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def write_state(path, value):
    temporary = owned(path.with_suffix(path.suffix + ".tmp"))
    with temporary.open("x", encoding="utf-8", newline="\n") as output:
        json.dump(value, output, indent=2)
        output.flush()
        os.fsync(output.fileno())
    os.replace(temporary, owned(path))


def qmp_socket():
    path = owned(VM / "qmp.sock")
    if os.name != "nt":
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(3)
        try:
            sock.connect(str(path))
            return sock
        except BaseException:
            sock.close()
            raise
    # This Python Windows build does not expose AF_UNIX in socket.connect.
    # Winsock still supports it; only sockaddr marshalling needs this adapter.
    encoded = str(path).encode("utf-8")
    if len(encoded) >= 108:
        raise ValueError("local QMP socket path exceeds sockaddr_un bounds")
    class Address(ctypes.Structure):
        _fields_ = [("family", ctypes.c_ushort), ("path", ctypes.c_char * 108)]
    address = Address(1, encoded)  # AF_UNIX in Windows SDK afunix.h.
    api = ctypes.WinDLL("ws2_32")
    api.connect.argtypes = [ctypes.c_size_t, ctypes.c_void_p, ctypes.c_int]
    api.connect.restype = ctypes.c_int
    api.WSAGetLastError.restype = ctypes.c_int
    sock = socket.socket(1, socket.SOCK_STREAM)
    try:
        sock.setblocking(False)
        if api.connect(sock.fileno(), ctypes.byref(address), ctypes.sizeof(address)) != 0:
            error = api.WSAGetLastError()
            if error not in (10035, 10036, 10037):
                raise OSError(error, "local QMP connect failed")
            _, ready, failed = select.select([], [sock], [sock], 3)
            if not ready and not failed:
                raise TimeoutError("local QMP connect timed out")
            error = sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
            if error:
                raise OSError(error, "local QMP connect failed")
        sock.settimeout(5)
        return sock
    except BaseException:
        sock.close()
        raise


def qmp(command, arguments=None):
    with qmp_socket() as sock:
        sock.settimeout(5)
        stream = sock.makefile("rwb", buffering=0)
        greeting = json.loads(stream.readline())
        if "QMP" not in greeting:
            raise ValueError("not a QMP endpoint")
        for request in ("qmp_capabilities", command):
            message = {"execute": request}
            if request == command and arguments is not None:
                message["arguments"] = arguments
            stream.write(json.dumps(message).encode() + b"\r\n")
            while True:
                reply = json.loads(stream.readline())
                if "event" not in reply:
                    break
            if "error" in reply:
                raise ValueError(reply["error"])
        return reply["return"]


def pid_alive(pid):
    if os.name != "nt":
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.OpenProcess(0x1000, False, pid)
    if not handle:
        if ctypes.get_last_error() == 87:
            return False
        raise OSError("cannot inspect recorded VM process")
    try:
        code = ctypes.c_ulong()
        if not kernel.GetExitCodeProcess(handle, ctypes.byref(code)):
            raise OSError("cannot query recorded VM process")
        return code.value == 259
    finally:
        kernel.CloseHandle(handle)


def config():
    return json.loads(owned(VM / "config.json").read_text(encoding="utf-8"))


def running():
    try:
        actual = qmp("query-uuid")["UUID"]
    except (OSError, ValueError):
        return False
    if actual.lower() != config()["uuid"].lower():
        raise ValueError("QMP endpoint belongs to a different VM")
    return True


def free_ports(*ports):
    for port in ports:
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", port))


def private_path(path, directory=False):
    path = owned(path)
    if os.name != "nt":
        path.chmod(0o700 if directory else 0o600)
        return
    identity = subprocess.check_output(["whoami.exe", "/user", "/fo", "csv", "/nh"],
                                       text=True, creationflags=FLAGS)
    sid = next(csv.reader(identity.strip().splitlines()))[1]
    if re.fullmatch(r"S-[0-9-]+", sid) is None:
        raise ValueError("unexpected current user SID")
    rights = "(OI)(CI)F" if directory else "(F)"
    subprocess.run(["icacls.exe", str(path), "/inheritance:r", "/grant:r", f"*{sid}:{rights}"],
                   check=True, stdout=subprocess.DEVNULL, creationflags=FLAGS)


def prepare():
    owned(VM).mkdir(parents=True, exist_ok=True)
    private_path(VM, directory=True)
    if not (VM / "config.json").exists():
        if (VM / "system.qcow2").exists() or (VM / "nvram.fd").exists():
            raise ValueError("existing disk/firmware lacks its VM registration")
        write_state(VM / "config.json", {"uuid": str(uuid.uuid4()),
            "disk_serial": "AETHERI_P0_ONLY", "disk_bytes": 48 * 1024**3})
    key = owned(VM / "id_ed25519")
    if not key.exists():
        subprocess.run(["ssh-keygen.exe", "-q", "-t", "ed25519", "-N", "", "-C", "aetheri-p0",
                        "-f", str(key)], check=True, creationflags=FLAGS)
        private_path(key)
    public = owned(VM / "id_ed25519.pub").read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"ssh-ed25519 [A-Za-z0-9+/=]+ aetheri-p0", public):
        raise ValueError("unexpected public key format")
    http = owned(VM / "http")
    http.mkdir(exist_ok=True)
    template = (ROOT / "spec/p0-carrier.ks.in").read_text(encoding="utf-8")
    (http / "carrier.ks").write_text(template.replace("@SSH_PUBLIC_KEY@", public),
                                    encoding="utf-8", newline="\n")
    disk = owned(VM / "system.qcow2")
    if not disk.exists():
        subprocess.run([str(IMAGE), "create", "-f", "qcow2", str(disk), "48G"],
                       check=True, creationflags=FLAGS)
    info = json.loads(subprocess.check_output([str(IMAGE), "info", "--output=json", str(disk)],
                                             creationflags=FLAGS))
    if info["format"] != "qcow2" or info["virtual-size"] != config()["disk_bytes"] or "backing-filename" in info:
        raise ValueError("unexpected virtual disk")
    nvram = owned(VM / "nvram.fd")
    if not nvram.exists():
        shutil.copyfile(TOOLS / "share/edk2-i386-vars.fd", nvram)
    return disk, nvram


def command(install, offline=False):
    disk, nvram = VM / "system.qcow2", VM / "nvram.fd"
    args = [str(QEMU), "-name", "aetheri-p0", "-uuid", config()["uuid"],
            "-machine", "pc-q35-11.1,accel=whpx", "-cpu", "host,vmx=off,svm=off", "-smp", "2", "-m", "8192M",
            "-smbios", "type=1,product=Aetheri P0 disposable VM", "-no-user-config",
            "-drive", f"if=pflash,format=raw,readonly=on,file={TOOLS / 'share/edk2-x86_64-code.fd'}",
            "-drive", f"if=pflash,format=raw,file={nvram}",
            "-drive", f"if=none,id=p0disk,format=qcow2,file={disk}",
            "-device", "virtio-blk-pci,drive=p0disk,serial=AETHERI_P0_ONLY",
            "-netdev", f"user,id=net0,{'restrict=on,' if offline else ''}hostfwd=tcp:127.0.0.1:{SSH_PORT}-:22",
            "-device", "virtio-net-pci,netdev=net0", "-display", "none",
            "-serial", f"file:{VM / ('install-serial.log' if install else 'serial.log')}",
            "-qmp", f"unix:{VM / 'qmp.sock'},server=on,wait=off", "-no-reboot"]
    if install:
        kernel = VM / "installer/images/pxeboot/vmlinuz"
        if not kernel.exists():
            subprocess.run([str(ARCHIVER), "x", "-y", f"-o{VM / 'installer'}",
                str(ROOT / "downloads/Fedora-Server-netinst-x86_64-44-1.7.iso"), "images/pxeboot/*"],
                check=True, creationflags=FLAGS, stdout=subprocess.DEVNULL)
        args += ["-drive", f"file={ROOT / 'downloads/Fedora-Server-netinst-x86_64-44-1.7.iso'},media=cdrom,readonly=on,format=raw",
                 "-kernel", str(kernel), "-initrd", str(VM / "installer/images/pxeboot/initrd.img"),
                 "-append", f"inst.stage2=hd:LABEL=Fedora-S-dvd-x86_64-44 inst.ks=http://10.0.2.2:{HTTP_PORT}/carrier.ks ip=dhcp console=ttyS0,115200n8 inst.text inst.noninteractive"]
    return args


def start(install, retry=False, offline=False):
    with launch_guard():
        return start_locked(install, retry, offline)


def start_locked(install, retry=False, offline=False):
    if install and offline:
        raise ValueError("network installation requires network access")
    if running():
        raise ValueError("VM is already running")
    state_file = VM / "process.json"
    if state_file.exists():
        previous = json.loads(state_file.read_text())
        if "pid" not in previous or pid_alive(previous["pid"]):
            raise ValueError("previous start is unresolved/live; inspect before another start")
        if install and not retry:
            raise ValueError("repeat installation requires explicit --retry-install")
    if (VM / "qmp.sock").exists():
        raise ValueError("existing QMP socket needs process reconciliation; never remove it blindly")
    free_ports(SSH_PORT, *([HTTP_PORT] if install else []))
    prepare()
    server = None
    if install:
        handler = partial(http.server.SimpleHTTPRequestHandler, directory=str(VM / "http"))
        server = http.server.ThreadingHTTPServer(("127.0.0.1", HTTP_PORT), handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
    args = command(install, offline)
    (VM / "last-command.json").write_text(json.dumps(args, indent=2), encoding="utf-8")
    with (VM / ("install-qemu.log" if install else "qemu.log")).open("ab") as log:
        state = {"uuid": config()["uuid"], "install": install, "offline": offline,
                 "started_unix": time.time(), "state": "starting"}
        write_state(state_file, state)
        process = subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                   creationflags=FLAGS)
        state.update(pid=process.pid, state="spawned")
        write_state(state_file, state)
        print(json.dumps({"pid": process.pid, "install": install, "qmp": "local AF_UNIX"}), flush=True)
        try:
            result = process.wait()
        finally:
            if server:
                server.shutdown()
                server.server_close()
        state.update(state="exited", exit=result)
        write_state(state_file, state)
        print(json.dumps({"pid": process.pid, "exit": result}), flush=True)
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--retry-install", action="store_true", help="explicitly reinstall this disposable guest")
    parser.add_argument("--offline", action="store_true", help="restrict guest networking to host forwarding")
    parser.add_argument("action", choices=("install", "boot", "status", "ssh", "poweroff"))
    parser.add_argument("remote", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.action in ("install", "boot"):
        return start(args.action == "install", args.retry_install, args.offline)
    if args.action == "status":
        active = running()
        record = VM / "process.json"
        observed_pid = None
        unresolved = False
        if record.exists():
            previous = json.loads(record.read_text(encoding="utf-8"))
            unresolved = "pid" not in previous
            if not unresolved:
                observed_pid = pid_alive(previous["pid"])
        print(json.dumps({"qmp_verified": active, "recorded_process_alive": observed_pid,
                          "unresolved_start": unresolved,
                          "status": qmp("query-status") if active else None}))
        return 0
    if not running():
        raise ValueError("VM identity not verified via QMP")
    if args.action == "poweroff":
        print(json.dumps(qmp("system_powerdown")))
        return 0
    return subprocess.call(["ssh.exe", "-F", "NUL" if os.name == "nt" else "/dev/null", "-T",
        "-p", str(SSH_PORT), "-i", str(VM / "id_ed25519"),
        "-o", "IdentityAgent=none", "-o", "ForwardAgent=no", "-o", "ClearAllForwardings=yes",
        "-o", "IdentitiesOnly=yes", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
        "-o", "StrictHostKeyChecking=accept-new", "-o", f"UserKnownHostsFile={VM / 'known_hosts'}",
        "aetheri@127.0.0.1", *args.remote], creationflags=FLAGS,
        stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
