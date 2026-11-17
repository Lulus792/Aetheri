#!/usr/bin/env python3
"""Read-only decisions for the candidate login-role policy on an explicit carrier.

Does not load policy, change enforcement, start a service or grant admission.
The caller selects an independent exact expected *loaded* policy digest.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import re
import sys


class Decision(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in
                ("allowed", "decided", "auditallow", "auditdeny", "seqno", "flags")]


def inspect(expected: str, graphical: bool = False) -> dict:
    if not re.fullmatch(r"[0-9a-f]{64}", expected):
        raise ValueError("expected-policy-sha256 must be 64 lowercase hex digits")
    policy = Path("/sys/fs/selinux/policy")
    before = policy.read_bytes()
    if hashlib.sha256(before).hexdigest() != expected:
        raise RuntimeError("loaded policy differs from the independently selected digest")
    lib = ctypes.CDLL("/usr/lib64/libselinux.so.1", use_errno=True)
    lib.security_getenforce.restype = ctypes.c_int
    if lib.security_getenforce() != 1:
        raise RuntimeError("SELinux must remain enforcing")
    lib.security_get_initial_context_raw.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)]
    lib.freecon.argtypes = [ctypes.c_void_p]
    initial = ctypes.c_char_p()
    if lib.security_get_initial_context_raw(b"security", ctypes.byref(initial)):
        raise RuntimeError("cannot observe the initial security context")
    try:
        if initial.value != b"system_u:object_r:security_t:s0":
            raise RuntimeError("unexpected initial security context")
    finally:
        lib.freecon(initial)
    lib.string_to_security_class.argtypes = [ctypes.c_char_p]
    lib.string_to_security_class.restype = ctypes.c_uint16
    lib.string_to_av_perm.argtypes = [ctypes.c_uint16, ctypes.c_char_p]
    lib.string_to_av_perm.restype = ctypes.c_uint32
    lib.security_compute_av_flags_raw.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                                ctypes.c_uint16, ctypes.c_uint32,
                                                ctypes.POINTER(Decision)]
    rows = []

    def process(name: str) -> str:
        return f"system_u:system_r:{name}:s0"

    def obj(name: str) -> str:
        return f"system_u:object_r:{name}:s0"

    def check(source: str, target: str, klass: str, permission: str, wanted: bool) -> None:
        cls = lib.string_to_security_class(klass.encode())
        mask = lib.string_to_av_perm(cls, permission.encode()) if cls else 0
        if not cls or not mask:
            raise RuntimeError(f"unknown policy class/permission: {klass}/{permission}")
        d = Decision()
        if lib.security_compute_av_flags_raw(source.encode(), target.encode(), cls, mask, ctypes.byref(d)):
            raise RuntimeError(f"decision query failed: errno={ctypes.get_errno()}")
        allowed = bool(d.allowed & mask)
        if d.flags & 1 or allowed != wanted:
            raise RuntimeError(f"unexpected decision {source} -> {target} {klass}/{permission}: "
                               f"allowed={allowed}, permissive={bool(d.flags & 1)}")
        rows.append(dict(source=source, target=target, klass=klass, permission=permission,
                         allowed=allowed, seqno=d.seqno))

    backend, worker, greeter = (process(t) for t in
                               ("aetheri_login_backend_t", "aetheri_login_worker_t", "aetheri_greeter_t"))
    for role in (backend, worker, greeter):
        check(role, obj("shadow_t"), "file", "read", role == worker)
        for permission in ("write", "create", "unlink"):
            check(role, obj("shadow_t"), "file", permission, False)
        check(role, obj("user_home_dir_t"), "dir", "search", False)
        for permission in ("read", "open", "write", "create"):
            check(role, obj("user_home_t"), "file", permission, False)
        for klass in ("tcp_socket", "udp_socket"):
            check(role, role, klass, "create", False)
        for permission in ("load_policy", "setenforce", "setbool"):
            check(role, obj("security_t"), "security", permission, False)
        for permission in ("start", "stop", "reload", "enable", "disable"):
            check(role, obj("systemd_unit_file_t"), "service", permission, False)
        for permission in ("sys_admin", "sys_ptrace", "audit_control"):
            check(role, role, "capability", permission, False)
        for permission in ("setuid", "setgid", "audit_write"):
            check(role, role, "capability", permission, role == worker)
        for permission in ("read", "write", "ioctl"):
            check(role, obj("tty_device_t"), "chr_file", permission, role != greeter)
        check(role, role, "netlink_audit_socket", "create", role == worker)
        check(role, role, "netlink_audit_socket", "nlmsg_relay", role == worker)
        check(role, role, "netlink_audit_socket", "nlmsg_write", False)
        for target in ("aetheri_login_backend_exec_t", "aetheri_greeter_exec_t", "aetheri_login_config_t"):
            check(role, obj(target), "file", "write", False)
    check(backend, worker, "process", "transition", True)
    check(worker, greeter, "process", "transition", True)
    password_helper = process("chkpwd_t")
    check(worker, password_helper, "process", "transition", True)
    check(worker, password_helper, "process2", "nnp_transition", True)
    for role in (backend, worker, greeter):
        check(role, obj("chkpwd_exec_t"), "file", "execute", role == worker)
        check(role, obj("chkpwd_exec_t"), "file", "execute_no_trans", False)
        check(role, obj("updpwd_exec_t"), "file", "execute", False)
    for permission in ("read", "write"):
        check(password_helper, worker, "fifo_file", permission, True)
    for target in (worker, greeter):
        check(process("init_t"), target, "process", "transition", False)
    check(greeter, backend, "process", "transition", False)
    check(greeter, obj("aetheri_login_backend_exec_t"), "file", "execute", False)
    check(greeter, obj("bin_t"), "file", "execute", False)
    publisher = process("aetheri_login_startup_t")
    for target in ("aetheri_login_backend_exec_t", "aetheri_login_config_t", "aetheri_greeter_exec_t"):
        for permission in ("read", "open", "getattr"):
            check(publisher, obj(target), "file", permission, True)
        check(publisher, obj(target), "file", "write", False)
    check(publisher, obj("aetheri_login_backend_exec_t"), "file", "execute", False)
    for permission in ("write", "getattr"):
        check(publisher, obj("aetheri_login_socket_t"), "sock_file", permission, True)
    for permission in ("create", "setattr", "unlink"):
        check(publisher, obj("aetheri_login_socket_t"), "sock_file", permission, False)
    check(publisher, obj("var_run_t"), "sock_file", "write", False)
    check(publisher, backend, "unix_stream_socket", "connectto", True)
    check(publisher, backend, "process", "ptrace", False)
    check(publisher, obj("security_t"), "security", "read_policy", True)
    for role in (publisher, greeter):
        check(role, obj("security_t"), "file", "map", True)
        check(role, obj("security_t"), "file", "write", False)
    check(greeter, obj("security_t"), "security", "read_policy", False)
    for permission in ("load_policy", "setenforce", "setbool"):
        check(publisher, obj("security_t"), "security", permission, False)
    check(greeter, backend, "unix_stream_socket", "connectto", True)
    check(greeter, obj("aetheri_login_handoff_t"), "file", "read", True)
    for permission in ("write", "create", "unlink"):
        check(greeter, obj("aetheri_login_handoff_t"), "file", permission, False)
    check(greeter, obj("systemd_unit_file_t"), "service", "status", True)
    for peer in (backend, worker):
        for permission in ("ptrace", "signal", "sigkill"):
            check(greeter, peer, "process", permission, False)
    session = process("aetheri_session_t")
    entry = obj("aetheri_session_exec_t")
    core = obj("aetheri_session_core_exec_t")
    check(worker, session, "process", "transition", True)
    check(worker, session, "process2", "nnp_transition", True)
    check(worker, entry, "file", "execute", True)
    check(worker, entry, "file", "execute_no_trans", False)
    check(session, entry, "file", "entrypoint", True)
    check(session, core, "file", "execute_no_trans", True)
    for role in (backend, greeter, publisher):
        check(role, session, "process", "transition", False)
        check(role, entry, "file", "execute", False)
    for role in (backend, worker, greeter, session):
        check(role, entry, "file", "write", False)
        check(role, core, "file", "write", False)
    check(session, obj("aetheri_session_runtime_t"), "dir", "create", True)
    check(session, process("init_t"), "file", "read", True)
    check(session, process("init_t"), "process", "ptrace", False)
    check(session, obj("bin_t"), "file", "execute", False)
    check(session, obj("shadow_t"), "file", "read", False)
    check(session, obj("aetheri_login_handoff_t"), "file", "write", False)
    check(session, session, "capability", "sys_admin", False)
    for peer in (backend, worker):
        check(session, peer, "process", "signal", False)
    shell = process("aetheri_desktop_shell_t")
    shell_exec = obj("aetheri_desktop_shell_exec_t")
    check(session, shell, "process", "transition", True)
    check(session, shell, "process2", "nnp_transition", True)
    check(session, shell_exec, "file", "execute_no_trans", False)
    check(shell, shell_exec, "file", "entrypoint", True)
    check(session, session, "process", "execmem", False)
    check(shell, session, "process", "signal", False)
    check(shell, obj("shadow_t"), "file", "read", False)
    for device in ("dri_device_t", "event_device_t"):
        check(session, obj(device), "chr_file", "read", True)
        check(session, obj(device), "chr_file", "open", False)
        for role in (backend, worker, greeter, shell):
            check(role, obj(device), "chr_file", "read", False)
            check(role, obj(device), "chr_file", "open", False)
    for target in ("udev_var_run_t", "sysfs_t"):
        check(session, obj(target), "file", "read", True)
        check(session, obj(target), "file", "write", False)
    check(session, session, "netlink_kobject_uevent_socket", "read", True)
    check(session, session, "netlink_kobject_uevent_socket", "write", False)
    check(shell, shell, "netlink_kobject_uevent_socket", "create", False)
    for role in (session, shell):
        check(role, obj("aetheri_session_shm_t"), "file", "create", True)
        check(role, obj("aetheri_session_memfd_t"), "file", "write", True)
        check(role, obj("tmpfs_t"), "file", "write", False)
    if graphical:
        display = process("aetheri_greeter_display_t")
        display_exec = obj("aetheri_greeter_display_exec_t")
        check(greeter, display, "process", "transition", True)
        check(greeter, display, "process2", "nnp_transition", True)
        check(greeter, display, "process", "ptrace", False)
        check(display, greeter, "process", "ptrace", False)
        check(greeter, display_exec, "file", "execute_no_trans", False)
        check(display, display_exec, "file", "entrypoint", True)
        check(display, process("systemd_logind_t"), "fd", "use", True)
        for device in ("dri_device_t", "event_device_t"):
            check(display, obj(device), "chr_file", "read", True)
            check(display, obj(device), "chr_file", "open", False)
        check(display, backend, "unix_stream_socket", "connectto", False)
        check(display, obj("aetheri_login_socket_t"), "sock_file", "write", False)
        check(display, obj("aetheri_login_handoff_t"), "file", "read", False)
        check(display, obj("shadow_t"), "file", "read", False)
        check(display, obj("user_home_t"), "file", "read", False)
        check(display, display, "process", "execmem", False)
        for klass in ("tcp_socket", "udp_socket"):
            check(display, display, klass, "create", False)
        for role in (display, greeter):
            check(role, obj("tmpfs_t"), "file", "write", False)
            check(role, obj("aetheri_greeter_shm_dir_t"), "dir", "add_name", True)
        check(greeter, obj("aetheri_greeter_shm_t"), "file", "write", True)
        check(display, obj("aetheri_greeter_shm_t"), "file", "write", True)
        for permission in ("open", "create", "unlink"):
            check(display, obj("aetheri_greeter_shm_t"), "file", permission, False)
        check(display, obj("aetheri_greeter_keymap_t"), "file", "write", True)
        check(greeter, obj("aetheri_greeter_keymap_t"), "file", "write", False)
        shared = obj("aetheri_login_shm_dir_t")
        for role in (display, greeter, session, shell):
            check(role, shared, "dir", "add_name", True)
            check(role, shared, "dir", "read", False)
        for role in (display, greeter):
            check(role, obj("aetheri_session_shm_t"), "file", "read", False)
        for role in (session, shell):
            for name in ("aetheri_greeter_shm_t", "aetheri_greeter_keymap_t"):
                check(role, obj(name), "file", "read", False)
    if len({row["seqno"] for row in rows}) != 1:
        raise RuntimeError("policy sequence changed during observation")
    if lib.security_getenforce() != 1 or policy.read_bytes() != before:
        raise RuntimeError("policy/enforcement changed during observation")
    return dict(loaded_policy_sha256=expected, enforcing=True, read_only=True,
                deployment_admission=False, checks=rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-policy-sha256", required=True)
    parser.add_argument("--graphical", action="store_true", help="also check the graphical greeter roles")
    args = parser.parse_args()
    try:
        record = inspect(args.expected_policy_sha256, args.graphical)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"login policy: {error}", file=sys.stderr)
        return 1
    print(json.dumps(record, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
