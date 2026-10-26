"""Provision/check the two real P0 authorities inside the disposable carrier.

Explicit root controller, never an app API or host installer. Owned state is kept
on failure. No foreign file/module replacement or forced process termination.
"""
import bz2
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import stat
import subprocess
import sys
import time
import uuid
import xml.etree.ElementTree as ET
import runtime_contracts

ROOT = Path("/var/lib/aetheri-p0")
SOURCE = Path(__file__).resolve().parents[1]
BUILD = Path("/home/aetheri/build/release")
MODULE = "aetheri_p0_services"
CONFIGS = {"system": Path("/etc/dbus-1/system.d/90-aetheri-p0.conf"),
           "user": Path("/etc/dbus-1/session.d/90-aetheri-p0.conf")}


def run(*command, timeout=120):
    return subprocess.run(command, check=True, capture_output=True, text=True, timeout=timeout).stdout.strip()


def user_run(*command):
    return run("sudo", "-n", "-u", "aetheri", "--", "env", "XDG_RUNTIME_DIR=/run/user/1000", *command)


def bus(role, *arguments):
    return (run("busctl", "--system", *arguments) if role == "system" else user_run("busctl", "--user", *arguments))


def checksum(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def owned_file(path, mode):
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_uid != 0 or stat.S_IMODE(info.st_mode) != mode:
        raise RuntimeError("untrusted owned file metadata")


def save(record):
    path = ROOT / "controller.json"
    temporary = ROOT / "controller.next"
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as output:
        json.dump(record, output, indent=2)
        output.flush()
        os.fsync(output.fileno())
    os.replace(temporary, path)
    fd = os.open(ROOT, os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def load():
    if ROOT.is_symlink() or ROOT.resolve() != ROOT or ROOT.stat().st_uid != 0 or stat.S_IMODE(ROOT.stat().st_mode) != 0o755:
        raise RuntimeError("untrusted controller root")
    path = ROOT / "controller.json"
    if path.is_symlink() or not path.is_file() or path.stat().st_nlink != 1 or path.stat().st_uid != 0 or stat.S_IMODE(path.stat().st_mode) != 0o600:
        raise RuntimeError("untrusted controller record")
    return validate_record(json.loads(path.read_text()))


def validate_record(record):
    if str(uuid.UUID(record["token"])) != record["token"]:
        raise RuntimeError("invalid owned token")
    for value in [record["owner"], *record["epochs"].values()]:
        if not isinstance(value, str) or re.fullmatch("[0-9a-f]{32}", value) is None or uuid.UUID(hex=value).version != 4:
            raise RuntimeError("invalid fixed registry identity")
    pattern = "aetheri-p0-" + record["token"].replace("-", "") + r"-(?:system(?:-(?:init|enroll))?|user(?:-(?:init|storage-(?:publish|verify)))?|probe-(?:check|claim|broker))-[0-9]+\.service"
    if any(not isinstance(unit, str) or re.fullmatch(pattern, unit) is None for unit in record["units"]):
        raise RuntimeError("foreign unit in owned record")
    if record["pending"] is not None and record["pending"] not in record["units"]:
        raise RuntimeError("unrecognized pending dispatch")
    if any(role not in ("system", "user") or unit not in record["units"] for role, unit in record.get("active_units", {}).items()):
        raise RuntimeError("unrecognized active authority")
    if any(re.search("-" + role + r"-[0-9]+\.service$", unit) is None for role, unit in record.get("active_units", {}).items()):
        raise RuntimeError("administrative/probe unit cannot be an active authority")
    if record.get("broker_unit") is not None and (record["broker_unit"] not in record["units"] or re.search(r"-user-[0-9]+\.service$", record["broker_unit"]) is None):
        raise RuntimeError("unrecognized protected broker unit")
    if "storage" in record:
        intent = record["storage"]
        if set(intent) != set(runtime_contracts.OWNER_FIELDS.split()) or intent["account"] != record["owner"]:
            raise RuntimeError("unrecognized allocation intent")
        for value in intent.values():
            if not isinstance(value, str) or re.fullmatch("[0-9a-f]{32}", value) is None or uuid.UUID(hex=value).version != 4:
                raise RuntimeError("invalid allocation identity")
    return record


def module_bytes():
    path = Path("/var/lib/selinux/targeted/active/modules/400") / MODULE / "cil"
    data = path.read_bytes()
    return bz2.decompress(data) if data.startswith(b"BZh") else data


def verified(record, allow_new_probe=False, allow_unreceipted_storage=False):
    if hashlib.sha256(module_bytes()).hexdigest() != record["policy_sha256"]:
        raise RuntimeError("installed module differs from owned policy")
    owned_file(ROOT / (MODULE + ".cil"), 0o644)
    if checksum(ROOT / (MODULE + ".cil")) != record["policy_sha256"]:
        raise RuntimeError("local owned policy changed")
    for role, target in CONFIGS.items():
        owned_file(target, 0o644)
        if checksum(target) != record["config_sha256"][role]:
            raise RuntimeError("bus policy differs from owned input")
    for role in ("system", "user"):
        owned_file(ROOT / (role + "-authority"), 0o555)
        if checksum(ROOT / (role + "-authority")) != record["authority_sha256"]:
            raise RuntimeError("authority executable changed")
        uid = 0 if role == "system" else 1000
        registry = ROOT / "registry" / ("system.conf" if role == "system" else "user-1000.conf")
        owned_file(registry, 0o444)
        expected = f"AETHERI-P0-AUTHORITY-1\nkind={role}\nepoch={record['epochs'][role]}\nowner={record['owner'] if uid else '-'}\nuid={uid}\n"
        if registry.read_bytes() != expected.encode():
            raise RuntimeError("owned registry changed")
    owned_file(ROOT / "probe-service", 0o555)
    if checksum(ROOT / "probe-service") != record["probe_sha256"]:
        raise RuntimeError("probe executable changed")
    mount = ROOT / "user-bus"
    owned_file(mount, 0o444)
    if mount.stat().st_size != 0:
        raise RuntimeError("untrusted bus mount placeholder")
    owned_file(ROOT / "registry/broker.conf", 0o444)
    if (ROOT / "registry/broker.conf").read_text() != "AETHERI-P0-BROKER-1\nunit=" + (record.get("broker_unit") or "-") + "\n":
        raise RuntimeError("protected broker slot changed")
    probe = ROOT / "probe-broker"
    if allow_new_probe and "broker_probe_sha256" not in record:
        if probe.exists() or probe.is_symlink():
            raise RuntimeError("unregistered broker probe exists")
    else:
        owned_file(probe, 0o555)
        if checksum(probe) != record["broker_probe_sha256"]:
            raise RuntimeError("broker probe changed")
    if "storage" in record:
        verify_storage(record, allow_unreceipted_storage)


def label(path, type_name, cats=""):
    run("chcon", "system_u:object_r:" + type_name + ":s0" + (":" + cats if cats else ""), str(path))


def directory(path, type_name, uid=0, mode=0o755, cats=""):
    path.mkdir(mode=mode)
    os.chown(path, uid, uid)
    label(path, type_name, cats)


def publish(path, data, type_name, mode=0o444):
    with path.open("xb") as output:
        output.write(data)
    path.chmod(mode)
    label(path, type_name)


def prepare():
    if ROOT.exists() or ROOT.is_symlink() or MODULE in run("semodule", "-l").split() or any(p.exists() or p.is_symlink() for p in CONFIGS.values()):
        raise RuntimeError("existing P0 state/config/module needs reconciliation")
    for role, name in (("system", "org.aetheri.System1"), ("user", "org.aetheri.User1")):
        if bus(role, "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", "s", name) != "b false":
            raise RuntimeError("authority name already owned")
    ROOT.mkdir(mode=0o755)
    record = {"token": str(uuid.uuid4()), "owner": uuid.uuid4().hex,
              "epochs": {role: uuid.uuid4().hex for role in ("system", "user")},
              "policy_sha256": checksum(SOURCE / "policy/p0-services.cil"),
              "authority_sha256": checksum(BUILD / "aetheri-authority"),
              "probe_sha256": checksum(BUILD / "probe_service"),
              "broker_probe_sha256": checksum(BUILD / "probe_broker"),
              "config_sha256": {role: checksum(SOURCE / ("policy/org.aetheri." + ("System1" if role == "system" else "User1") + ".conf")) for role in CONFIGS},
              "units": [], "pending": None, "state": "preparing"}
    save(record)
    cil = ROOT / (MODULE + ".cil")
    cil.write_bytes((SOURCE / "policy/p0-services.cil").read_bytes())
    cil.chmod(0o644)
    run("semodule", "-X", "400", "-i", str(cil))
    label(ROOT, "aetheri_root_t")
    directory(ROOT / "registry", "aetheri_registry_t")
    publish(ROOT / "registry/broker.conf", b"AETHERI-P0-BROKER-1\nunit=-\n", "aetheri_registry_t")
    directory(ROOT / "system", "aetheri_system_state_t", mode=0o700)
    directory(ROOT / "users", "aetheri_root_t")
    directory(ROOT / "users" / record["owner"], "aetheri_root_t")
    directory(ROOT / "users" / record["owner"] / "control", "aetheri_user_state_t", uid=1000, mode=0o700)
    directory(ROOT / "probe-data", "aetheri_app_data_t", uid=1000, mode=0o700, cats="c31,c32")
    publish(ROOT / "user-bus", b"", "aetheri_bus_mount_t")
    for role in ("system", "user"):
        publish(ROOT / (role + "-authority"), (BUILD / "aetheri-authority").read_bytes(), "aetheri_" + role + "_exec_t", mode=0o555)
        uid = 0 if role == "system" else 1000
        data = f"AETHERI-P0-AUTHORITY-1\nkind={role}\nepoch={record['epochs'][role]}\nowner={record['owner'] if uid else '-'}\nuid={uid}\n"
        publish(ROOT / "registry" / ("system.conf" if role == "system" else "user-1000.conf"), data.encode(), "aetheri_registry_t")
        source = SOURCE / ("policy/org.aetheri." + ("System1" if role == "system" else "User1") + ".conf")
        with CONFIGS[role].open("xb") as output:
            output.write(source.read_bytes())
        CONFIGS[role].chmod(0o644)
        run("restorecon", str(CONFIGS[role]))
    publish(ROOT / "probe-service", (BUILD / "probe_service").read_bytes(), "aetheri_app_exec_t", mode=0o555)
    publish(ROOT / "probe-broker", (BUILD / "probe_broker").read_bytes(), "aetheri_user_exec_t", mode=0o555)
    for role in ("system", "user"):
        bus(role, "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ReloadConfig")
    record["state"] = "prepared"
    save(record)
    print("Prepared owned P0 policy/registry/executables; no catalogues or services started.")


def state_path(record, role):
    return ROOT / "system" if role == "system" else ROOT / "users" / record["owner"] / "control"


def storage_paths(record):
    anchor = ROOT / "users" / record["owner"] / "data"
    root = anchor / record["storage"]["storage"]
    registry = state_path(record, "user") / "allocations"
    return {"anchor": anchor, "root": root, "registry": registry, "payload": root / "payload",
            "owner": root / "owner", "document": root / "payload/document",
            "config": ROOT / "registry/allocation.conf", "record": registry / record["storage"]["storage"]}


def verify_storage(record, allow_unreceipted=False):
    if record.get("storage_state") not in ("prepared", "published"):
        raise RuntimeError("interrupted storage preparation needs reconciliation")
    paths = storage_paths(record)
    for key in ("anchor", "root", "registry", "payload"):
        info = paths[key].lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != 1000 or stat.S_IMODE(info.st_mode) != 0o700:
            raise RuntimeError("private fixture directory changed")
    for key in ("anchor", "root"):
        info = paths[key].stat()
        if (info.st_ino, info.st_ctime_ns) != (record["storage_" + key + "_inode"], record["storage_" + key + "_ctime_ns"]):
            raise RuntimeError("private fixture object replaced or metadata changed")
    expected = runtime_contracts.owner(record["storage"])
    for key, uid, mode, contents in (("config", 0, 0o444, expected), ("owner", 1000, 0o600, expected), ("document", 1000, 0o600, b"hello\n")):
        info = paths[key].lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_uid != uid or stat.S_IMODE(info.st_mode) != mode or paths[key].read_bytes() != contents:
            raise RuntimeError("private fixture file changed")
    entry = paths["record"]
    if "storage_record_sha256" in record:
        info = entry.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_uid != 1000 or stat.S_IMODE(info.st_mode) != 0o600 or info.st_size != 168 or checksum(entry) != record["storage_record_sha256"]:
            raise RuntimeError("immutable allocation record changed")
    elif (entry.exists() or entry.is_symlink()) and not allow_unreceipted:
        raise RuntimeError("unreceipted allocation needs explicit storage-check reconciliation")


def storage_prepare(record):
    verified(record)
    stop(record)
    if "storage" in record:
        raise RuntimeError("storage already present; preserve its allocation")
    if not (state_path(record, "user") / "catalogue.sqlite3").is_file():
        raise RuntimeError("initialize the owner catalogue before storage preparation")
    with sqlite3.connect("file:" + str(state_path(record, "system") / "catalogue.sqlite3") + "?mode=ro", uri=True) as db:
        eligible = db.execute("SELECT kind,os_uid,state FROM scopes WHERE scope_id=?", (bytes.fromhex(record["owner"]),)).fetchall()
    db.close()
    if eligible != [("account", 1000, "active")]:
        raise RuntimeError("fixed account is not eligible for storage preparation")
    intent = {key: uuid.uuid4().hex for key in runtime_contracts.OWNER_FIELDS.split()}
    intent["account"] = record["owner"]
    candidate = {**record, "storage": intent}
    paths = storage_paths(candidate)
    if any(paths[key].exists() or paths[key].is_symlink() for key in ("anchor", "registry", "config")):
        raise RuntimeError("unregistered storage path exists; no adoption")
    record.update(storage=intent, storage_state="preparing")
    save(record)
    directory(paths["registry"], "aetheri_allocation_t", uid=1000, mode=0o700)
    for key in ("anchor", "root"):
        directory(paths[key], "aetheri_user_state_t", uid=1000, mode=0o700)
    directory(paths["payload"], "aetheri_app_data_t", uid=1000, mode=0o700, cats="c31,c32")
    expected = runtime_contracts.owner(intent)
    publish(paths["config"], expected, "aetheri_registry_t")
    publish(paths["owner"], expected, "aetheri_user_state_t", mode=0o600)
    publish(paths["document"], b"hello\n", "aetheri_app_data_t", mode=0o600)
    os.chown(paths["owner"], 1000, 1000); os.chown(paths["document"], 1000, 1000)
    label(paths["document"], "aetheri_app_data_t", "c31,c32")
    for path in [paths[key] for key in ("config", "owner", "document", "payload", "root", "anchor", "registry")] + [state_path(record, "user"), ROOT / "users" / record["owner"], ROOT / "registry"]:
        fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    for key in ("anchor", "root"):
        info = paths[key].stat()
        record["storage_" + key + "_inode"], record["storage_" + key + "_ctime_ns"] = info.st_ino, info.st_ctime_ns
    record["storage_state"] = "prepared"
    save(record)
    print("Prepared protected fixed storage intent/root/payload; no durable allocation record or app installed.")


def storage_check(record):
    verified(record, allow_unreceipted_storage=True)
    stop(record)
    paths = storage_paths(record)
    before = (checksum(paths["record"]), paths["record"].stat().st_ino) if paths["record"].exists() else None
    start_unit(record, "user", storage="publish")
    first = (checksum(paths["record"]), paths["record"].stat().st_ino)
    if before is not None and before != first:
        raise AssertionError("existing allocation changed on repeat publication")
    start_unit(record, "user", storage="publish")
    if first != (checksum(paths["record"]), paths["record"].stat().st_ino):
        raise AssertionError("allocation retry rewrote the committed object")
    start_unit(record, "user", storage="verify")
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    first_boot = record.setdefault("storage_first_boot", boot)
    if "storage_record_inode" in record and record["storage_record_inode"] != first[1]:
        raise AssertionError("committed allocation record inode changed")
    record.update(storage_record_sha256=first[0], storage_record_inode=first[1], storage_state="published")
    record["storage_check"] = {"durable_record": True, "exact_retry_preserved": True, "separate_invocation_reopen": True, "cross_boot_reopen": first_boot != boot, "native_sha256": True}
    save(record)
    verify_storage(record)
    print(json.dumps(record["storage_check"]))


def unit_has_job(unit):
    jobs = json.loads(bus("system", "--json=short", "call", "org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "ListJobs"))
    return jobs["type"] != "a(usssoo)" or any(job[1] == unit for job in jobs["data"][0])


def publish_broker_slot(record, unit):
    temporary = ROOT / "registry/broker.next"
    publish(temporary, ("AETHERI-P0-BROKER-1\nunit=" + unit + "\n").encode(), "aetheri_registry_t")
    os.replace(temporary, ROOT / "registry/broker.conf")
    record["broker_unit"] = unit


def start_unit(record, role, init=False, probe=None, claims=None, enroll=False, storage=None):
    if enroll and (role != "system" or init or probe):
        raise ValueError("account enrollment requires only the system administrative role")
    if storage is not None and (storage not in ("publish", "verify") or role != "user" or init or probe or enroll):
        raise ValueError("storage administration requires only the user role")
    suffix = ("probe-" + probe) if probe else role + ("-init" if init else "-enroll" if enroll else "-storage-" + storage if storage else "")
    if record["pending"]:
        raise RuntimeError("previous dispatch is unacknowledged")
    unit = "aetheri-p0-" + record["token"].replace("-", "") + "-" + suffix + "-" + str(len(record["units"])) + ".service"
    if role == "user" and not init and probe is None and storage is None:
        publish_broker_slot(record, unit)
    if unit not in record["units"]:
        record["units"].append(unit)
    record["pending"] = unit
    save(record)
    properties = {"User": "aetheri" if role == "user" or probe else "root",
                  "Group": "aetheri" if role == "user" or probe else "root",
                  "NoNewPrivileges": "yes", "CapabilityBoundingSet": "", "UMask": "0077",
                  "PrivateMounts": "yes", "ProtectSystem": "strict", "ProtectHome": "yes",
                  "ProtectControlGroups": "yes", "RestrictNamespaces": "yes", "RestrictSUIDSGID": "no", "SystemCallArchitectures": "native",
                  "KillMode": "control-group", "SendSIGKILL": "no", "TimeoutStopSec": "5s", "TimeoutStartSec": "15s",
                  "WorkingDirectory": str(ROOT), "MemoryMax": "64M", "TasksMax": "16"}
    if probe:
        properties.update(SELinuxContext="system_u:system_r:aetheri_app_t:s0:c31,c32", ReadWritePaths=str(ROOT / "probe-data"))
        command = [str(ROOT / "probe-service"), probe, record["owner"]]
        if "storage" in record:
            command.append(record["storage"]["storage"])
        if probe == "broker":
            properties["SELinuxContext"] = "system_u:system_r:aetheri_user_t:s0"
            properties.pop("ReadWritePaths")
            command = [str(ROOT / "probe-broker"), *claims]
    else:
        properties.update(SELinuxContext="system_u:system_r:aetheri_" + role + "_t:s0", ReadWritePaths=str(state_path(record, role)))
        command = [str(ROOT / (role + "-authority")), role] + (["--init"] if init else ["--enroll-account"] if enroll else ["--storage-" + storage] if storage else [])
    if role == "user" or probe:
        properties.update(BindsTo="user@1000.service", After="user@1000.service",
                          BindReadOnlyPaths="/run/user/1000/bus:" + str(ROOT / "user-bus"))
    arguments = ["systemd-run", "--unit=" + unit, "--service-type=" + ("exec" if init or probe or enroll or storage else "notify")]
    if init or probe or enroll or storage:
        arguments.append("--wait")
    arguments.extend("--property=" + key + "=" + value for key, value in properties.items())
    try:
        run(*arguments, *command, timeout=40)
    except subprocess.CalledProcessError as error:
        facts = dict(line.split("=", 1) for line in run("systemctl", "show", unit, "--property=ActiveState,ExecMainStatus,MainPID,ControlGroup").splitlines())
        if facts.get("ActiveState") == "failed" and facts.get("MainPID") == "0" and not facts.get("ControlGroup") and not unit_has_job(unit):
            record["pending"] = None
            record["last_failure"] = {"unit": unit, "facts": facts, "stderr": error.stderr}
            save(record)
        raise
    record["pending"] = None
    save(record)
    return unit


def protocol(role):
    name = "org.aetheri.System1" if role == "system" else "org.aetheri.User1"
    value = json.loads(bus(role, "--json=short", "call", name, "/org/aetheri/Management1", "org.aetheri.Management1", "GetProtocolInfo"))
    if value["type"] != "uuasayay" or value["data"][:3] != [1, 1, []]:
        raise AssertionError("protocol/capabilities disagree")
    return bytes(value["data"][3]).hex(), bytes(value["data"][4]).hex()


def start(record):
    if record["pending"]:
        raise RuntimeError("unacknowledged dispatch requires reconciliation")
    verified(record)
    active = record.setdefault("active_units", {})
    for role in ("system", "user"):
        name = "org.aetheri.System1" if role == "system" else "org.aetheri.User1"
        owned = bus(role, "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", "s", name)
        if owned == "b true":
            if role not in active:
                raise RuntimeError("existing authority is not registered to this controller")
            pid = run("systemctl", "show", active[role], "--property=MainPID", "--value")
            actual = bus(role, "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetConnectionUnixProcessID", "s", name)
            if pid == "0" or actual != "u " + pid or protocol(role)[0] != record["epochs"][role]:
                raise RuntimeError("authority identity is unresolved; no second dispatch")
            continue
        if role in active:
            facts = dict(line.split("=", 1) for line in run("systemctl", "show", active[role], "--property=ActiveState,MainPID,ControlGroup").splitlines())
            if facts.get("ActiveState") not in ("inactive", "failed") or facts.get("MainPID") != "0" or facts.get("ControlGroup") or unit_has_job(active[role]):
                raise RuntimeError("old authority still live/unknown; no second dispatch")
        if not (state_path(record, role) / "catalogue.sqlite3").exists():
            start_unit(record, role, init=True)
        active[role] = start_unit(record, role)
        save(record)
        epoch, event = protocol(role)
        if epoch != record["epochs"][role] or uuid.UUID(hex=event).version != 4:
            raise AssertionError("protocol identity mismatch")
    record["state"] = "running"
    save(record)
    print("Both confined P0 authority endpoints are running.")


def check(record):
    verified(record)
    report = {}
    for role in ("system", "user"):
        epoch, event = protocol(role)
        with sqlite3.connect("file:" + str(state_path(record, role) / "catalogue.sqlite3") + "?mode=ro", uri=True) as db:
            actual = db.execute("SELECT hex(authority_epoch),hex(current_boot_id),hex(runtime_incarnation),catalogue_revision FROM authority_meta").fetchone()
            if role == "system" and db.execute("SELECT kind,os_uid,state,revision FROM scopes WHERE scope_id=?", (bytes.fromhex(record["owner"]),)).fetchall() != [("account", 1000, "active", 1)]:
                raise AssertionError("protected account enrollment differs from its native bootstrap")
        db.close()
        if epoch != record["epochs"][role] or epoch.upper() != actual[0] or event.upper() == actual[2]:
            raise AssertionError("durable/protocol/runtime identity mismatch")
        if actual[1].lower() != Path("/proc/sys/kernel/random/boot_id").read_text().strip().replace("-", ""):
            raise AssertionError("catalogue is not bound to the real kernel boot")
        paths = ["/org/aetheri/Management1", "/org/aetheri/Runtime1", "/org/aetheri/Broker1"] if role == "system" else ["/org/aetheri/Management1", "/org/aetheri/Documents1"]
        name = "org.aetheri.System1" if role == "system" else "org.aetheri.User1"
        for path in paths:
            live = ET.fromstring(json.loads(bus(role, "--json=short", "call", name, path, "org.freedesktop.DBus.Introspectable", "Introspect"))["data"][0])
            expected_file = {"Management1": "management-v1.xml", "Runtime1": "runtime-v1.xml", "Documents1": "documents-v1.xml", "Broker1": "broker-v1.xml"}[path.rsplit("/", 1)[1]]
            expected = ET.parse(SOURCE / "spec/api" / expected_file).getroot()
            for interface in expected.findall("interface"):
                if interface.attrib["name"] == "org.aetheri.Subscription1" or (role == "system" and interface.attrib["name"] == "org.aetheri.User1"):
                    continue
                actual_interface = live.find("./interface[@name='" + interface.attrib["name"] + "']")
                if actual_interface is None:
                    raise AssertionError("missing live interface")
                for method in interface.findall("method"):
                    actual_method = actual_interface.find("./method[@name='" + method.attrib["name"] + "']")
                    shape = lambda value: [(a.get("direction", "in"), a.attrib["type"], a.attrib["name"]) for a in value.findall("arg")]
                    if actual_method is None or shape(actual_method) != shape(method):
                        raise AssertionError("live method differs from frozen XML")
        unit = record["active_units"][role]
        facts = dict(line.split("=", 1) for line in run("systemctl", "show", unit, "--property=ActiveState,MainPID,MemoryCurrent,TasksCurrent,CPUUsageNSec").splitlines())
        pid = int(facts["MainPID"])
        actual_pid = bus(role, "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetConnectionUnixProcessID", "s", name)
        if facts["ActiveState"] != "active" or pid <= 0 or actual_pid != "u " + str(pid):
            raise AssertionError("bus owner is not the supervised authority")
        process = Path("/proc") / str(pid)
        if (process / "attr/current").read_text().rstrip("\x00\n") != "system_u:system_r:aetheri_" + role + "_t:s0":
            raise AssertionError("authority domain mismatch")
        status = dict(line.split(":", 1) for line in (process / "status").read_text().splitlines() if ":" in line)
        if status["NoNewPrivs"].strip() != "1" or any(int(status[key].strip(), 16) for key in ("CapEff", "CapPrm", "CapInh", "CapBnd", "CapAmb")):
            raise AssertionError("authority privileges mismatch")
        report[role] = {"epoch": epoch, "events": event, "runtime": actual[2].lower(), "boot": actual[1].lower(),
                        "revision": actual[3], "observed": facts}
    user_run("python3", "-c", "from pathlib import Path; p=Path('" + str(state_path(record, "user") / "catalogue.sqlite3") + "'); assert p.open('rb').read(16)==b'SQLite format 3\\0'")
    if "storage_record_sha256" in record:
        user_run("python3", "-c", "from pathlib import Path; p=Path('" + str(storage_paths(record)["record"]) + "'); assert p.open('rb').read(8)==b'AETHALC1'")
    start_unit(record, "user", probe="check")
    claims = [record["epochs"]["system"], record["owner"], record["epochs"]["user"], report["user"]["runtime"]]
    user_run(str(BUILD / "probe_broker"), *claims)
    start_unit(record, "user", probe="broker", claims=claims)
    record["report"] = report
    save(record)
    print(json.dumps({"confined_endpoints": 2, "live_xml": True, "real_boot": True, "app_public_ipc_and_denials": True}))


def restart_check(record):
    check(record)
    before = record["report"]
    stop(record)
    if bus("user", "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", "s", "org.aetheri.User1") != "b false":
        raise RuntimeError("user name still owned after stop")
    start_unit(record, "user", probe="claim")
    start(record)
    check(record)
    after = record["report"]
    for role in ("system", "user"):
        if any(after[role][key] != before[role][key] for key in ("epoch", "boot")) or any(after[role][key] == before[role][key] for key in ("runtime", "events")) or after[role]["revision"] <= before[role]["revision"]:
            raise AssertionError("restart identity/revision contract failed")
    record["restart_check"] = {"before": before, "after": after, "unowned_name_claim_denied": True}
    save(record)
    print("Restart preserves epochs/boot, replaces runtime/events, advances revisions; unowned name claim denied.")


def stop_unit(unit):
    facts = dict(line.split("=", 1) for line in run("systemctl", "show", unit, "--property=ActiveState,MainPID,ControlGroup,LoadState").splitlines())
    if facts.get("LoadState") != "not-found":
        run("systemctl", "stop", unit, timeout=20)
    facts = dict(line.split("=", 1) for line in run("systemctl", "show", unit, "--property=ActiveState,MainPID,ControlGroup").splitlines())
    if facts.get("ActiveState") not in ("inactive", "failed") or facts.get("MainPID") != "0" or facts.get("ControlGroup"):
        raise RuntimeError("owned unit is not drained; preserve state")
    if unit_has_job(unit):
        raise RuntimeError("owned unit still has a supervisor job; preserve state")


def stop(record):
    if record["pending"]:
        raise RuntimeError("unacknowledged dispatch requires reconciliation")
    for unit in record["units"]:
        stop_unit(unit)
    record["state"] = "stopped"
    save(record)
    print("Owned authority/probe units stopped without SIGKILL escalation.")


def bootstrap_scopes(db, owner):
    # Only the fixed bootstrap account may be removed with this empty carrier.
    # Retired, rebound, revised or additional accounts require explicit handling.
    rows = db.execute("SELECT hex(scope_id),kind,os_uid,state,revision FROM scopes").fetchall()
    return rows in ([], [(owner.upper(), "account", 1000, "active", 1)])


def cleanup(record):
    """Remove only an empty, inspected development bootstrap, never app data."""
    if record["pending"]:
        raise RuntimeError("unacknowledged dispatch requires reconciliation")
    verified(record)
    stop(record)
    if "storage" in record:
        raise RuntimeError("allocated private data is not an empty bootstrap; use a reviewed storage retirement protocol")
    for role, name in (("system", "org.aetheri.System1"), ("user", "org.aetheri.User1")):
        if bus(role, "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", "s", name) != "b false":
            raise RuntimeError("authority name still owned; preserve installation")
    directories = {ROOT: (0, 0o755), ROOT / "registry": (0, 0o755), ROOT / "system": (0, 0o700),
                   ROOT / "users": (0, 0o755), ROOT / "users" / record["owner"]: (0, 0o755),
                   state_path(record, "user"): (1000, 0o700), ROOT / "probe-data": (1000, 0o700)}
    files = {ROOT / "controller.json": (0, 0o600), ROOT / (MODULE + ".cil"): (0, 0o644),
             ROOT / "system-authority": (0, 0o555), ROOT / "user-authority": (0, 0o555),
             ROOT / "probe-service": (0, 0o555), ROOT / "user-bus": (0, 0o444),
             ROOT / "registry/system.conf": (0, 0o444), ROOT / "registry/user-1000.conf": (0, 0o444)}
    files[ROOT / "registry/broker.conf"] = (0, 0o444)
    files[ROOT / "probe-broker"] = (0, 0o555)
    for role in ("system", "user"):
        directory = state_path(record, role)
        uid = 0 if role == "system" else 1000
        dbpath = directory / "catalogue.sqlite3"
        if dbpath.exists():
            files.update({dbpath: (uid, 0o600), directory / "writer.lock": (uid, 0o600)})
    # No following links, recursive shell deletion, unrecognized scratch, journals
    # or sidecars. Any residue means inspection, even after all units have stopped.
    def visit(directory):
        for path in directory.iterdir():
            info = path.lstat()
            if stat.S_ISDIR(info.st_mode):
                if path not in directories:
                    raise RuntimeError("unexpected directory; preserve installation")
                visit(path)
            elif not stat.S_ISREG(info.st_mode) or path not in files or info.st_nlink != 1:
                raise RuntimeError("unexpected file/link/sidecar; preserve installation")
    visit(ROOT)
    for path, (uid, mode) in {**directories, **files}.items():
        info = path.lstat()
        if info.st_uid != uid or stat.S_IMODE(info.st_mode) != mode:
            raise RuntimeError("unexpected owned entry metadata; preserve installation")
    for role in ("system", "user"):
        path = state_path(record, role) / "catalogue.sqlite3"
        if path not in files:
            continue
        with sqlite3.connect("file:" + str(path) + "?mode=ro", uri=True) as db:
            db.execute("PRAGMA trusted_schema=OFF")
            epoch, owner = db.execute("SELECT hex(authority_epoch),hex(owner_scope_ref) FROM authority_meta").fetchone()
            if epoch.lower() != record["epochs"][role] or owner.lower() != (record["owner"] if role == "user" else ""):
                raise RuntimeError("catalogue identity mismatch; preserve installation")
            if db.execute("PRAGMA quick_check").fetchall() != [("ok",)]:
                raise RuntimeError("catalogue requires inspection")
            tables = db.execute("SELECT name FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%'").fetchall()
            for (table,) in tables:
                if role == "system" and table == "scopes":
                    if not bootstrap_scopes(db, record["owner"]):
                        raise RuntimeError("non-bootstrap accounts exist; preserve installation")
                    continue
                quoted = '"' + table.replace('"', '""') + '"'
                if table != "authority_meta" and db.execute("SELECT COUNT(*) FROM " + quoted).fetchone()[0] != 0:
                    raise RuntimeError("bootstrap contains managed records; cleanup refuses app removal")
        db.close()
    for role, path in CONFIGS.items():
        path.unlink()
        bus(role, "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ReloadConfig")
    for unit in record["units"]:
        if run("systemctl", "show", unit, "--property=LoadState", "--value") != "not-found":
            run("systemctl", "reset-failed", unit)
    # Retain the root-owned receipt and CIL if module removal fails. After this
    # administrative step there is no automatic interrupted-cleanup recovery.
    retained = {ROOT / "controller.json", ROOT / (MODULE + ".cil")}
    for path in files.keys() - retained:
        path.unlink()
    run("semodule", "-X", "400", "-r", MODULE)
    for path in retained:
        path.unlink()
    for path in sorted(directories, key=lambda item: len(item.parts), reverse=True):
        path.rmdir()
    if ROOT.exists() or MODULE in run("semodule", "-l").split():
        raise RuntimeError("cleanup did not finish")
    print("Removed the enumerated empty P0 bootstrap, bus policies and owned SELinux module.")


def update_owned(record):
    verified(record, allow_new_probe=True)
    stop(record)
    record["state"] = "updating-owned-inputs"
    save(record)
    cil = ROOT / (MODULE + ".cil")
    new_policy = (SOURCE / "policy/p0-services.cil").read_bytes()
    cil.write_bytes(new_policy)
    if hashlib.sha256(new_policy).hexdigest() != record["policy_sha256"]:
        run("semodule", "-X", "400", "-i", str(cil))
    record["policy_sha256"] = checksum(cil)
    for role in ("system", "user"):
        target = ROOT / (role + "-authority")
        temporary = ROOT / (role + "-authority.next")
        publish(temporary, (BUILD / "aetheri-authority").read_bytes(), "aetheri_" + role + "_exec_t", mode=0o555)
        os.replace(temporary, target)
    temporary = ROOT / "probe-service.next"
    publish(temporary, (BUILD / "probe_service").read_bytes(), "aetheri_app_exec_t", mode=0o555)
    os.replace(temporary, ROOT / "probe-service")
    temporary = ROOT / "probe-broker.next"
    publish(temporary, (BUILD / "probe_broker").read_bytes(), "aetheri_user_exec_t", mode=0o555)
    os.replace(temporary, ROOT / "probe-broker")
    for role, target in CONFIGS.items():
        source = SOURCE / ("policy/org.aetheri." + ("System1" if role == "system" else "User1") + ".conf")
        if checksum(source) != record["config_sha256"][role]:
            temporary = target.with_suffix(".conf.next")
            with temporary.open("xb") as output:
                output.write(source.read_bytes())
            temporary.chmod(0o644)
            run("restorecon", str(temporary))
            os.replace(temporary, target)
            record["config_sha256"][role] = checksum(target)
            bus(role, "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ReloadConfig")
    record.update(authority_sha256=checksum(BUILD / "aetheri-authority"), probe_sha256=checksum(BUILD / "probe_service"),
                  broker_probe_sha256=checksum(BUILD / "probe_broker"), state="prepared")
    save(record)
    print("Updated verified stopped owned policy/executables; registry and catalogues preserved.")


def enroll_account(record):
    verified(record)
    stop(record)
    start_unit(record, "system", enroll=True)
    record["state"] = "prepared"
    save(record)
    print("Native fixed account enrollment completed; no service or app was started.")


def account_check(record):
    verified(record)
    stop(record)
    path = state_path(record, "system") / "catalogue.sqlite3"
    def snapshot():
        with sqlite3.connect("file:" + str(path) + "?mode=ro", uri=True) as db:
            rows = db.execute("SELECT hex(scope_id),kind,os_uid,state,revision FROM scopes").fetchall()
            revision = db.execute("SELECT catalogue_revision FROM authority_meta WHERE singleton=1").fetchone()[0]
        db.close()
        return rows, revision, checksum(path)
    before = snapshot()
    expected = [(record["owner"].upper(), "account", 1000, "active", 1)]
    if before[0] not in ([], expected):
        raise RuntimeError("existing account records require reconciliation")
    denied_missing = False
    if not before[0]:
        try:
            unit = start_unit(record, "system")
        except subprocess.CalledProcessError:
            failed = record["last_failure"]
            journal = run("journalctl", "-u", failed["unit"], "--no-pager", "--output=cat")
            if failed["facts"].get("ExecMainStatus") != "5" or "Authority startup/runtime failed: -2" not in journal:
                raise AssertionError("missing account test failed for another reason")
            if snapshot() != before or bus("system", "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", "s", "org.aetheri.System1") != "b false":
                raise AssertionError("unregistered account changed startup state or acquired the authority name")
            denied_missing = True
        else:
            stop_unit(unit)
            raise AssertionError("authority served without its account enrollment")
    start_unit(record, "system", enroll=True)
    first = snapshot()
    start_unit(record, "system", enroll=True)
    second = snapshot()
    if first[0] != expected or first[1] != before[1] + (0 if before[0] else 1) or first != second:
        raise AssertionError("native enrollment changed an identity or repeated a committed write")
    record["account_check"] = {"durable_account": True, "exact_retry_preserves_database": True,
                               "missing_account_blocks_start": denied_missing if not before[0] else None}
    record["state"] = "prepared"
    save(record)
    print(json.dumps(record["account_check"]))


def lifecycle_check(record):
    """Explicitly retire/recreate an empty development bootstrap, not an app."""
    start(record)  # A headless SSH logout may have stopped the prior user bus.
    restart_check(record)
    before = record["restart_check"]
    unexpected = ROOT / "unexplained-fixture"
    with unexpected.open("xb") as output:
        output.write(b"This development test must survive the refused cleanup.\n")
    try:
        cleanup(record)
    except RuntimeError as error:
        if str(error) != "unexpected file/link/sidecar; preserve installation":
            raise
    else:
        raise AssertionError("cleanup accepted an unexplained entry")
    if not unexpected.is_file():
        raise AssertionError("refused cleanup removed unexplained entry")
    verified(record)
    unexpected.unlink()
    cleanup(record)
    prepare()
    replacement = load()
    if replacement["owner"] == record["owner"] or any(replacement["epochs"][role] == record["epochs"][role] for role in ("system", "user")):
        raise AssertionError("recreated registry reused an old durable identity")
    start(replacement)
    check(replacement)
    print(json.dumps({"restart": before, "unknown_entry_preserved": True, "empty_bootstrap_removed": True,
                      "fresh_registry": True, "after_recreation": replacement["report"]}))


def broker_check(record):
    start(record)
    check(record)
    first = record["report"]
    root_pid = first["system"]["observed"]["MainPID"]
    def root_fds():
        return len(list((Path("/proc") / root_pid / "fd").iterdir()))
    bound = root_fds()
    stop_unit(record["active_units"]["user"])
    protocol("system")  # Read-side liveness barrier; no launch/holder settlement.
    dropped = root_fds()
    if dropped != bound - 1:
        raise AssertionError("departed broker did not release exactly its retained process pin")
    start(record)
    check(record)
    second = record["report"]
    if second["system"]["observed"]["MainPID"] != root_pid or root_fds() != bound or second["user"]["runtime"] == first["user"]["runtime"]:
        raise AssertionError("new broker failed to replace only its own binding")
    user_pid = second["user"]["observed"]["MainPID"]
    stop_unit(record["active_units"]["system"])
    start(record)
    check(record)
    third = record["report"]
    if third["user"]["observed"]["MainPID"] != user_pid or third["user"]["runtime"] != second["user"]["runtime"] or third["system"]["runtime"] == second["system"]["runtime"]:
        raise AssertionError("root restart did not re-register the existing broker")
    record["broker_check"] = {"fds_bound": bound, "fds_after_exit": dropped, "user_replaced": True, "root_restarted_with_user_retained": True}
    save(record)
    print(json.dumps(record["broker_check"]))


def idle_check(record):
    start(record)
    check(record)
    def sample(role):
        unit = record["active_units"][role]
        facts = dict(line.split("=", 1) for line in run("systemctl", "show", unit, "--property=MainPID,MemoryCurrent,CPUUsageNSec,TasksCurrent").splitlines())
        process = Path("/proc") / facts["MainPID"]
        status = dict(line.split(":", 1) for line in (process / "status").read_text().splitlines() if ":" in line)
        memory = dict(line.split(":", 1) for line in (process / "smaps_rollup").read_text().splitlines() if ":" in line)
        return {"pid": int(facts["MainPID"]), "cpu_ns": int(facts["CPUUsageNSec"]),
                "cgroup_bytes": int(facts["MemoryCurrent"]), "tasks": int(facts["TasksCurrent"]),
                "rss_kib": int(memory["Rss"].split()[0]), "pss_kib": int(memory["Pss"].split()[0]),
                "voluntary_switches": int(status["voluntary_ctxt_switches"]),
                "involuntary_switches": int(status["nonvoluntary_ctxt_switches"])}
    before = {role: sample(role) for role in ("system", "user")}
    started = time.monotonic()
    time.sleep(5)  # One explicit measurement, never a resident polling loop.
    after = {role: sample(role) for role in ("system", "user")}
    elapsed = time.monotonic() - started
    result = {}
    for role in ("system", "user"):
        if before[role]["pid"] != after[role]["pid"]:
            raise AssertionError("authority changed during idle observation")
        result[role] = {"before": before[role], "after": after[role],
                        "delta": {key: after[role][key] - before[role][key] for key in ("cpu_ns", "voluntary_switches", "involuntary_switches")}}
    record["idle_observation"] = {"elapsed_seconds": elapsed, "services": result}
    save(record)
    print(json.dumps(record["idle_observation"]))


def main():
    if sys.platform != "linux" or os.geteuid() != 0 or not Path("/sys/firmware/efi").is_dir() or Path("/sys/class/dmi/id/product_name").read_text().strip() != "Aetheri P0 disposable VM" or not Path("/etc/aetheri-p0-carrier").is_file() or run("getenforce") != "Enforcing":
        raise RuntimeError("requires the enforcing disposable carrier root controller")
    import pwd
    account = pwd.getpwnam("aetheri")
    if account.pw_uid != 1000 or account.pw_gid != 1000:
        raise RuntimeError("unexpected test account")
    # One controller for this dedicated carrier. Lock the stable root-owned marker;
    # no lock-file deletion can let a second controller bypass an uncertain start.
    import fcntl
    marker = os.open("/etc/aetheri-p0-carrier", os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
    try:
        info = os.fstat(marker)
        if not stat.S_ISREG(info.st_mode) or info.st_uid != 0 or info.st_mode & 0o022:
            raise RuntimeError("untrusted carrier marker")
        fcntl.flock(marker, fcntl.LOCK_EX | fcntl.LOCK_NB)
        action = sys.argv[1]
        if action == "prepare":
            prepare()
            return
        record = load()
        if action == "start": start(record)
        elif action == "check": check(record)
        elif action == "restart-check": restart_check(record)
        elif action == "stop": stop(record)
        elif action == "cleanup": cleanup(record)
        elif action == "lifecycle-check": lifecycle_check(record)
        elif action == "idle-check": idle_check(record)
        elif action == "broker-check": broker_check(record)
        elif action == "update-owned": update_owned(record)
        elif action == "enroll-account": enroll_account(record)
        elif action == "account-check": account_check(record)
        elif action == "storage-prepare": storage_prepare(record)
        elif action == "storage-check": storage_check(record)
        else: raise RuntimeError("unsupported controller action")
    finally:
        os.close(marker)


if __name__ == "__main__":
    main()
