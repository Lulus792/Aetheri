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


def check_desktop_audio(check, process, obj) -> None:
    session = process("aetheri_session_t")
    graph = process("aetheri_audio_graph_t")
    policy = process("aetheri_audio_policy_t")
    tone = process("aetheri_audio_tone_t")
    socket = obj("aetheri_audio_socket_t")
    runtime = obj("aetheri_audio_runtime_t")
    state = obj("aetheri_audio_state_t")
    sound = obj("sound_device_t")
    for role, name in ((graph, "graph"), (policy, "policy"), (tone, "tone")):
        program = obj(f"aetheri_audio_{name}_exec_t")
        check(session, program, "file", "execute", True)
        check(session, program, "file", "execute_no_trans", False)
        check(session, role, "process2", "nnp_transition", True)
        check(role, program, "file", "entrypoint", True)
        check(role, obj("aetheri_audio_config_t"), "file", "read", True)
        check(role, obj("aetheri_audio_config_t"), "file", "write", False)
        check(role, obj("user_home_t"), "file", "read", False)
        check(role, obj("tmpfs_t"), "file", "map", False)
        check(role, role, "tcp_socket", "create", False)
        check(role, role, "capability", "sys_admin", False)
        for permission in ("open", "read", "write", "ioctl", "map"):
            check(role, sound, "chr_file", permission, role != tone)
        for source in ("settings", "files", "terminal", "greeter"):
            check(process(f"aetheri_{source}_t"), program, "file", "execute", False)
    check(session, runtime, "dir", "create", True)
    check(session, state, "dir", "rmdir", True)
    check(graph, state, "dir", "add_name", False)
    check(policy, state, "dir", "add_name", True)
    check(graph, runtime, "dir", "add_name", True)
    check(policy, runtime, "dir", "add_name", False)
    check(graph, socket, "sock_file", "create", True)
    check(policy, socket, "sock_file", "write", True)
    check(policy, graph, "unix_stream_socket", "connectto", True)
    check(tone, graph, "unix_stream_socket", "connectto", True)
    check(tone, graph, "process", "ptrace", False)
    check(tone, state, "file", "write", False)
    check(policy, obj("device_t"), "dir", "watch", True)
    check(policy, obj("device_t"), "dir", "open", True)
    check(graph, obj("device_t"), "dir", "watch", False)
    check(policy, obj("device_t"), "chr_file", "open", False)
    for name in ("settings", "files", "terminal", "greeter"):
        client = process(f"aetheri_{name}_t")
        check(client, socket, "sock_file", "write", False)
        check(client, graph, "unix_stream_socket", "connectto", False)
        for permission in ("open", "read", "write", "ioctl", "map"):
            check(client, sound, "chr_file", permission, False)


def check_desktop_sound(check, process, obj) -> None:
    control = process("aetheri_audio_control_t")
    executable = obj("aetheri_audio_control_exec_t")
    session = process("aetheri_session_t")
    graph = process("aetheri_audio_graph_t")
    startup = process("aetheri_login_startup_t")
    check(session, executable, "file", "execute", True)
    check(session, executable, "file", "execute_no_trans", False)
    check(session, control, "process2", "nnp_transition", True)
    check(control, executable, "file", "entrypoint", True)
    check(startup, executable, "file", "read", True)
    check(startup, executable, "file", "execute", False)
    check(startup, executable, "file", "write", False)
    check(control, session, "fd", "use", True)
    for access in ("read", "write", "getattr", "getopt"):
        check(control, session, "unix_stream_socket", access, True)
    check(control, session, "unix_stream_socket", "connectto", False)
    check(control, graph, "unix_stream_socket", "connectto", True)
    check(control, obj("systemd_logind_sessions_t"), "dir", "watch", True)
    check(control, obj("systemd_logind_sessions_t"), "file", "write", False)
    check(control, obj("aetheri_audio_graph_mem_t"), "file", "map", True)
    check(control, obj("aetheri_audio_state_t"), "file", "write", False)
    for access in ("open", "read", "write", "ioctl", "map"):
        check(control, obj("sound_device_t"), "chr_file", access, False)
    check(control, control, "tcp_socket", "create", False)
    for actor in ("settings", "files", "terminal", "greeter"):
        peer = process(f"aetheri_{actor}_t")
        check(peer, executable, "file", "execute", False)
        check(peer, control, "unix_stream_socket", "connectto", False)
        check(control, peer, "process", "ptrace", False)


def check_desktop_lock(check, process, obj) -> None:
    session = process("aetheri_session_t")
    worker = process("aetheri_login_worker_t")
    startup = process("aetheri_login_startup_t")
    view = process("aetheri_lock_view_t")
    entry = obj("aetheri_lock_view_exec_t")
    profile = obj("aetheri_unlock_pam_t")
    pixels = obj("aetheri_lock_pixels_t")
    check(session, entry, "file", "execute", True)
    check(session, entry, "file", "execute_no_trans", False)
    check(session, view, "process2", "nnp_transition", True)
    check(view, entry, "file", "entrypoint", True)
    check(startup, entry, "file", "read", True)
    check(startup, entry, "file", "execute", False)
    check(startup, profile, "file", "read", True)
    check(startup, obj("aetheri_greeter_assets_t"), "file", "read", True)
    check(worker, profile, "file", "read", True)
    check(worker, profile, "file", "write", False)
    check(session, profile, "file", "read", False)
    check(view, profile, "file", "read", False)
    for permission in ("read", "write", "getattr", "getopt"):
        check(view, session, "unix_stream_socket", permission, True)
        check(session, worker, "unix_stream_socket", permission, True)
    check(view, session, "unix_stream_socket", "connectto", False)
    check(session, worker, "unix_stream_socket", "connectto", False)
    check(view, worker, "unix_stream_socket", "read", False)
    check(worker, session, "fd", "use", True)
    keymap = obj("aetheri_session_shm_t")
    for permission in ("read", "getattr", "map"):
        check(view, keymap, "file", permission, True)
    for permission in ("open", "write"):
        check(view, keymap, "file", permission, False)
    check(view, obj("aetheri_session_memfd_t"), "file", "read", False)
    check(view, pixels, "file", "create", True)
    check(session, pixels, "file", "map", True)
    check(view, obj("shadow_t"), "file", "read", False)
    check(view, obj("dri_device_t"), "chr_file", "open", False)
    check(view, obj("event_device_t"), "chr_file", "open", False)
    check(view, view, "tcp_socket", "create", False)
    for name in ("settings", "files", "terminal"):
        app = process(f"aetheri_{name}_t")
        check(app, entry, "file", "execute", False)
        check(app, pixels, "file", "map", False)
        check(app, worker, "unix_stream_socket", "read", False)


def check_desktop_suspend(check, process, obj) -> None:
    guard = process("aetheri_suspend_guard_t")
    session = process("aetheri_session_t")
    manager = process("init_t")
    startup = process("aetheri_login_startup_t")
    entry = obj("aetheri_suspend_guard_exec_t")
    check(manager, guard, "process2", "nnp_transition", True)
    check(guard, entry, "file", "entrypoint", True)
    check(guard, session, "unix_stream_socket", "connectto", True)
    check(session, guard, "unix_stream_socket", "connectto", False)
    for source, target in ((guard, session), (session, guard)):
        check(source, target, "fd", "use", True)
    check(guard, session, "file", "read", True)
    seat_state = obj("systemd_logind_var_run_t")
    for access in ("getattr", "open", "read"):
        check(guard, seat_state, "file", access, True)
    check(guard, seat_state, "file", "write", False)
    check(guard, manager, "dir", "search", True)
    check(guard, manager, "file", "read", True)
    check(guard, manager, "file", "write", False)
    check(guard, manager, "process", "ptrace", False)
    check(guard, session, "process", "ptrace", False)
    check(guard, session, "process", "sigkill", False)
    check(guard, manager, "unix_dgram_socket", "sendto", True)
    check(guard, obj("shadow_t"), "file", "read", False)
    check(guard, obj("sysfs_t"), "file", "write", False)
    check(guard, guard, "tcp_socket", "create", False)
    for name in ("aetheri_suspend_guard_exec_t", "systemd_sleep_exec_t", "power_unit_file_t"):
        file = obj(name)
        check(startup, file, "file", "read", True)
        check(startup, file, "file", "write", False)
    for name in ("settings", "files", "terminal", "lock_view"):
        role = process(f"aetheri_{name}_t")
        check(role, guard, "unix_stream_socket", "connectto", False)
        check(role, entry, "file", "execute", False)


def inspect(expected: str, graphical: bool = False, applications: bool = False,
            desktop_audio: bool = False, desktop_sound: bool = False, desktop_lock: bool = False,
            desktop_suspend: bool = False) -> dict:
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
    if applications:
        helper = obj("aetheri_terminal_helper_exec_t")
        documents = obj("aetheri_documents_t")
        for name in ("settings", "files", "terminal", "terminal_helper"):
            entry = obj(f"aetheri_{name}_exec_t")
            for permission in ("read", "open", "getattr"):
                check(publisher, entry, "file", permission, True)
            for permission in ("execute", "execute_no_trans", "write", "create", "unlink"):
                check(publisher, entry, "file", permission, False)
        for name in ("settings", "files", "terminal"):
            app = process(f"aetheri_{name}_t")
            entry = obj(f"aetheri_{name}_exec_t")
            check(session, entry, "file", "execute", True)
            check(session, entry, "file", "execute_no_trans", False)
            check(session, app, "process2", "nnp_transition", True)
            check(app, entry, "file", "entrypoint", True)
            check(app, session, "unix_stream_socket", "connectto", True)
            check(app, obj("bin_t"), "file", "execute", False)
            check(app, app, "process", "execmem", False)
            check(app, obj("dri_device_t"), "chr_file", "open", False)
            check(app, app, "process", "getpgid", name == "terminal")
            check(app, documents, "file", "read", name != "settings")
            check(app, helper, "file", "execute_no_trans", name == "terminal")
        check(process("aetheri_files_t"), obj("aetheri_terminal_exec_t"), "file", "execute", True)
        check(process("aetheri_settings_t"), obj("aetheri_terminal_exec_t"), "file", "execute", False)
        check(process("aetheri_files_t"), obj("aetheri_settings_exec_t"), "file", "execute", False)
    if desktop_audio:
        check_desktop_audio(check, process, obj)
    if desktop_sound:
        check_desktop_sound(check, process, obj)
    if desktop_lock:
        check_desktop_lock(check, process, obj)
    if desktop_suspend:
        check_desktop_suspend(check, process, obj)
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
    parser.add_argument("--applications", action="store_true", help="also check the installed desktop app roles")
    parser.add_argument("--desktop-audio", action="store_true", help="also check the desktop audio roles")
    parser.add_argument("--desktop-sound", action="store_true", help="also check the desktop Sound mediator")
    parser.add_argument("--desktop-lock", action="store_true", help="also check the installed lock roles")
    parser.add_argument("--desktop-suspend", action="store_true", help="also check the installed suspend guard")
    args = parser.parse_args()
    try:
        record = inspect(args.expected_policy_sha256, args.graphical, args.applications,
                         args.desktop_audio, args.desktop_sound, args.desktop_lock, args.desktop_suspend)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"login policy: {error}", file=sys.stderr)
        return 1
    print(json.dumps(record, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
