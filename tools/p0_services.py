"""Provision/check the two real P0 authorities inside the disposable carrier.

Explicit root controller, never an app API or host installer. Owned state is kept
on failure. No foreign file/module replacement or forced process termination.
"""
import bz2
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import stat
import struct
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
    pattern = "aetheri-p0-" + record["token"].replace("-", "") + r"-(?:system(?:-(?:init|enroll|fixture-(?:observe|stage|activate|store-(?:observe|publish))))?|user(?:-(?:init|storage-(?:publish|verify)|data-(?:observe|enroll)))?|probe-(?:check|claim|broker|projection))-[0-9]+\.service"
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
    if record.get("projection_unit","-")!="-" and (record["projection_unit"] not in record["units"] or re.search(r"-probe-projection-[0-9]+\.service$",record["projection_unit"]) is None):
        raise RuntimeError("unrecognized protected projection unit")
    if "app_data" in record:
        data=record["app_data"]
        if set(data["owner"])!=set(runtime_contracts.OWNER_FIELDS.split()) or data["owner"]["account"]!=record["owner"]:
            raise RuntimeError("invalid app data owner")
        for value in [*data["owner"].values(),*(data[key] for key in ("install","publisher","system_epoch","user_epoch"))]:
            if not isinstance(value,str) or re.fullmatch("[0-9a-f]{32}",value) is None or uuid.UUID(hex=value).version!=4:
                raise RuntimeError("invalid app data identity")
        if any(type(data[key]) is not int or not 1<=data[key]<2**63 for key in ("install_revision","base_revision")) or data["schema_version"]!=0:
            raise RuntimeError("invalid app data revision")
    if "storage" in record:
        intent = record["storage"]
        if set(intent) != set(runtime_contracts.OWNER_FIELDS.split()) or intent["account"] != record["owner"]:
            raise RuntimeError("unrecognized allocation intent")
        for value in intent.values():
            if not isinstance(value, str) or re.fullmatch("[0-9a-f]{32}", value) is None or uuid.UUID(hex=value).version != 4:
                raise RuntimeError("invalid allocation identity")
    if "fixture" in record:
        fixture = record["fixture"]
        if fixture["scope"] != record["owner"]:
            raise RuntimeError("fixture scope changed")
        for key in FIXTURE_IDS:
            value = fixture[key]
            if not isinstance(value, str) or re.fullmatch("[0-9a-f]{32}", value) is None or uuid.UUID(hex=value).version != 4:
                raise RuntimeError("invalid fixture identity")
    return record


def module_bytes():
    path = Path("/var/lib/selinux/targeted/active/modules/400") / MODULE / "cil"
    data = path.read_bytes()
    return bz2.decompress(data) if data.startswith(b"BZh") else data


def verified(record, allow_new_probe=False, allow_unreceipted_storage=False, allow_unfinished_fixture=False, allow_new_projection=False, allow_unreceipted_app_data=False):
    verify_fixture(record, allow_unfinished_fixture)
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
    projection_probe=ROOT / "probe-projection"
    if allow_new_projection and "projection_probe_sha256" not in record:
        if projection_probe.exists() or projection_probe.is_symlink(): raise RuntimeError("unregistered projection probe exists")
    else:
        owned_file(projection_probe,0o555)
        if checksum(projection_probe)!=record["projection_probe_sha256"]: raise RuntimeError("projection probe changed")
    if "projection_unit" in record:
        owned_file(ROOT / "registry/projection.conf",0o444)
        if (ROOT / "registry/projection.conf").read_text()!="AETHERI-P0-PROJECTION-1\nunit="+record["projection_unit"]+"\n": raise RuntimeError("projection controller slot changed")
    verify_app_data(record,allow_unreceipted_app_data)


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
              "projection_probe_sha256": checksum(BUILD / "probe_projection"), "projection_unit":"-",
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
    publish(ROOT / "registry/projection.conf", b"AETHERI-P0-PROJECTION-1\nunit=-\n", "aetheri_registry_t")
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
    publish(ROOT / "probe-projection", (BUILD / "probe_projection").read_bytes(), "aetheri_user_exec_t", mode=0o555)
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


def app_data_paths(record):
    owner=record["app_data"]["owner"]
    anchor=ROOT/"users"/record["owner"]/"app-data"
    root=anchor/owner["storage"]
    registry=state_path(record,"user")/"app-allocations"
    return dict(anchor=anchor,root=root,registry=registry,allocation=registry/owner["storage"],
        owner=root/"owner",payload=root/"payload",owner_config=ROOT/"registry/app-data-owner.conf",config=ROOT/"registry/app-data.conf")


def app_data_config(data,witness):
    result=(b"AETHDTA1"+runtime_contracts.owner(data["owner"])
        +b"".join(bytes.fromhex(data[key]) for key in ("install","publisher","system_epoch","user_epoch"))
        +struct.pack(">QQQ",data["install_revision"],data["base_revision"],data["schema_version"])+witness)
    if len(result)!=368 or len(witness)!=168: raise ValueError("invalid app data configuration")
    return result


def verify_app_data(record,allow_unreceipted=False):
    if "app_data" not in record: return
    if record["app_data_state"] not in ("prepared","enrolled"): raise RuntimeError("interrupted app data preparation requires reconciliation")
    paths=app_data_paths(record)
    owner=runtime_contracts.owner(record["app_data"]["owner"])
    for key in ("anchor","root","registry","payload"):
        info=paths[key].lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid!=1000 or stat.S_IMODE(info.st_mode)!=0o700:
            raise RuntimeError("app data directory changed")
    for key,uid,mode,content in (("owner_config",0,0o444,owner),("owner",1000,0o600,owner),
            ("config",0,0o444,app_data_config(record["app_data"],bytes.fromhex(record["app_data_witness"])))):
        info=paths[key].lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_uid!=uid or stat.S_IMODE(info.st_mode)!=mode or paths[key].read_bytes()!=content:
            raise RuntimeError("protected app data file changed")
    entry=paths["allocation"]
    if "app_data_allocation_sha256" in record:
        info=entry.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_uid!=1000 or stat.S_IMODE(info.st_mode)!=0o600 or checksum(entry)!=record["app_data_allocation_sha256"]:
            raise RuntimeError("app allocation receipt changed")
    elif (entry.exists() or entry.is_symlink()) and not allow_unreceipted:
        raise RuntimeError("unreceipted app allocation requires explicit data-check reconciliation")


def data_prepare(record):
    verified(record)
    if "app_data" in record: raise RuntimeError("app data already selected; preserve its identities")
    projection_check(record)
    stop(record)
    with sqlite3.connect(f"file:{state_path(record,'user')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        binding=db.execute("SELECT lower(hex(publisher_ref)),lower(hex(system_epoch)),install_revision FROM install_bindings WHERE install_id=? AND state='active'",(bytes.fromhex(record["fixture"]["install"]),)).fetchone()
        baseline=db.execute("SELECT catalogue_revision FROM authority_meta").fetchone()[0]
        if binding is None or db.execute("SELECT count(*) FROM data_epochs").fetchone()[0]!=0 or db.execute("SELECT count(*) FROM attachments").fetchone()[0]!=0:
            raise RuntimeError("data preparation requires first projected installation and no existing data")
    owner={key:uuid.uuid4().hex for key in runtime_contracts.OWNER_FIELDS.split()}
    owner.update(account=record["owner"],machine=record["storage"]["machine"],volume=record["storage"]["volume"])
    data=dict(owner=owner,install=record["fixture"]["install"],publisher=binding[0],system_epoch=binding[1],
        user_epoch=record["epochs"]["user"],install_revision=binding[2],base_revision=baseline,schema_version=0)
    candidate={**record,"app_data":data}
    paths=app_data_paths(candidate)
    if any(paths[key].exists() or paths[key].is_symlink() for key in ("anchor","registry","config","owner_config")):
        raise RuntimeError("unknown app data infrastructure exists; no adoption")
    record.update(app_data=data,app_data_state="preparing")
    save(record)
    directory(paths["registry"],"aetheri_allocation_t",uid=1000,mode=0o700)
    for key in ("anchor","root"): directory(paths[key],"aetheri_user_state_t",uid=1000,mode=0o700)
    directory(paths["payload"],"aetheri_app_data_t",uid=1000,mode=0o700,cats="c33,c34")
    encoded=runtime_contracts.owner(owner)
    publish(paths["owner_config"],encoded,"aetheri_registry_t")
    publish(paths["owner"],encoded,"aetheri_user_state_t",mode=0o600)
    os.chown(paths["owner"],1000,1000)
    for key in ("owner","owner_config","payload","root","anchor","registry"): flush_path(paths[key],key not in ("owner","owner_config"))
    for parent in (state_path(record,"user"),ROOT/"registry",ROOT/"users"/record["owner"]): flush_path(parent,True)
    unit=start_unit(record,"user",data="observe")
    output=run("journalctl","-u",unit,"--output=cat","--no-pager")
    witnesses=re.findall(r"^data-witness=([0-9a-f]{336})$",output,re.M)
    if len(witnesses)!=1: raise RuntimeError("native app data observation missing")
    witness=bytes.fromhex(witnesses[0])
    if witness[:112]!=b"AETHALC1"+encoded: raise RuntimeError("native app data owner differs")
    publish(paths["config"],app_data_config(data,witness),"aetheri_registry_t")
    flush_path(paths["config"]); flush_path(ROOT/"registry",True)
    record.update(app_data_state="prepared",app_data_witness=witnesses[0])
    save(record)
    print("Prepared distinct protected app data fixture; no native allocation or metadata enrollment yet.")


def data_check(record):
    verified(record,allow_unreceipted_app_data=True)
    stop(record)
    paths=app_data_paths(record)
    system_hash=checksum(state_path(record,"system")/"catalogue.sqlite3")
    unrelated=(checksum(storage_paths(record)["record"]),storage_paths(record)["record"].stat().st_ino)
    database=state_path(record,"user")/"catalogue.sqlite3"
    def rows():
        with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
            data=db.execute("SELECT lower(hex(data_epoch_id)),lower(hex(storage_ref)),state,data_schema_version,revision FROM data_epochs").fetchall()
            attachment=db.execute("SELECT lower(hex(install_id)),lower(hex(data_epoch_id)) FROM attachments").fetchall()
            gate=db.execute("SELECT state,revision,closing_operation_id FROM admission_gates").fetchall()
            other={name:db.execute('SELECT * FROM "'+name+'" ORDER BY rowid').fetchall() for name in ("operations","launches","grants","access_leases")}
            revision=db.execute("SELECT catalogue_revision FROM authority_meta").fetchone()[0]
        return data,attachment,gate,other,revision
    before=rows()
    start_unit(record,"user",data="enroll")
    first=rows()
    owner=record["app_data"]["owner"]
    expected=([(owner["data"],owner["storage"],"active",0,1)],[(record["app_data"]["install"],owner["data"])],[("closed",2,None)])
    if first[:3]!=expected or first[3]!=before[3] or first[4]!=before[4]+(0 if before[0] else 1): raise AssertionError("native app data enrollment differs")
    first_hash=checksum(database)
    allocation=(checksum(paths["allocation"]),paths["allocation"].stat().st_ino)
    if paths["allocation"].read_bytes()!=bytes.fromhex(record["app_data_witness"]): raise AssertionError("allocation differs from independent witness")
    start_unit(record,"user",data="enroll")
    if rows()!=first or checksum(database)!=first_hash or allocation!=(checksum(paths["allocation"]),paths["allocation"].stat().st_ino): raise AssertionError("data retry changed durable state")
    if checksum(state_path(record,"system")/"catalogue.sqlite3")!=system_hash or unrelated!=(checksum(storage_paths(record)["record"]),storage_paths(record)["record"].stat().st_ino): raise AssertionError("unrelated system/storage fixture changed")
    boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    initial=record.setdefault("app_data_first_boot",boot)
    record.update(app_data_state="enrolled",app_data_allocation_sha256=allocation[0])
    record["app_data_check"]=dict(native_separate_data=True,atomic_closed_gate=True,exact_retry=True,unrelated_storage_preserved=True,cross_boot_reopen=initial!=boot,app_dispatched=False)
    save(record)
    print(json.dumps(record["app_data_check"]))


FIXTURE_IDS = ("plan", "operation", "actor", "request_key", "scope", "install", "generation", "slot", "publisher", "boot")


def fixture_paths(record):
    parent = ROOT / "fixtures" / record["fixture"]["slot"]
    return dict(parent=parent, source=parent / "source", staging=parent / "staging",
                config=ROOT / "registry/fixture.conf", manifest=ROOT / "registry/fixture.manifest",
                container=parent / "publication", store=ROOT / "store/sha256",
                target=ROOT / "store/sha256" / record["fixture"]["artifact"], store_config=ROOT / "registry/fixture-store.conf",
                activation_config=ROOT / "registry/fixture-activation.conf")


def fixture_payload(record, paths):
    candidates = [paths["staging"]]
    if "fixture_publication" in record:
        candidates += [paths["container"] / "payload", paths["target"] / "payload"]
    existing = [path for path in candidates if path.exists() or path.is_symlink()]
    if len(existing) != 1 or existing[0].is_symlink():
        raise RuntimeError("ambiguous/missing fixture payload location")
    return existing[0]


def fixture_config(record, roots):
    fixture = record["fixture"]
    data = (b"AETHFIC1" + b"".join(bytes.fromhex(fixture[key]) for key in FIXTURE_IDS)
            + bytes.fromhex(fixture["artifact"]) + struct.pack(">QQQ", fixture["scope_revision"], fixture["expires"], fixture["accepted"])
            + roots + bytes.fromhex(fixture["intent_digest"]))
    if len(data) != 328:
        raise RuntimeError("fixture configuration shape")
    return data


def flush_path(path, directory=False):
    fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | (os.O_DIRECTORY if directory else 0))
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def verify_fixture(record, allow_unfinished=False):
    if "fixture" not in record:
        return
    state = record["fixture_state"]
    if state not in ("prepared", "stage-ready") and not (allow_unfinished and state in ("observing", "sealing")):
        raise RuntimeError("unfinished fixture provisioning needs explicit reconciliation")
    paths, fixture = fixture_paths(record), record["fixture"]
    payload = fixture_payload(record, paths)
    for path, mode in ((ROOT / "fixtures", 0o700), (paths["parent"], 0o700), (paths["source"], 0o555), (payload, None)):
        info = path.lstat()
        modes = (0o700, 0o555) if mode is None and "fixture_publication" in record else (0o700,) if mode is None else (mode,)
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != 0 or stat.S_IMODE(info.st_mode) not in modes:
            raise RuntimeError("fixture root ownership/mode changed")
    for key in ("source", "staging"):
        info = (payload if key == "staging" else paths[key]).stat()
        expected = fixture["objects"][key]
        if info.st_ino != expected[0] or (state in ("observing", "sealing") and info.st_ctime_ns != expected[1]):
            raise RuntimeError("preallocated fixture object changed")
    if state in ("observing", "sealing") and any(paths["staging"].iterdir()):
        raise RuntimeError("untracked content in preallocated fixture slot")
    actual = {path.relative_to(paths["source"]).as_posix() for path in paths["source"].rglob("*")}
    if actual != {"bin", "bin/probe", "share", "share/message"}:
        raise RuntimeError("fixture source inventory changed")
    for file in fixture["manifest"]["files"]:
        path = paths["source"] / file["path"]
        owned_file(path, file["mode"])
        if checksum(path) != file["sha256"]:
            raise RuntimeError("fixture source bytes changed")
    owned_file(paths["manifest"], 0o444)
    if checksum(paths["manifest"]) != fixture["artifact"]:
        raise RuntimeError("fixture manifest changed")
    owned_file(paths["config"], 0o444)
    acceptable = {fixture["config_sha256"]}
    if state == "sealing": acceptable.add(fixture["sealed_sha256"])
    if checksum(paths["config"]) not in acceptable:
        raise RuntimeError("protected fixture configuration changed")
    if "fixture_publication" in record:
        publication = record["fixture_publication"]
        if publication["state"] not in ("observing", "sealing", "prepared", "executing", "published"):
            raise RuntimeError("unfinished publication infrastructure needs reconciliation")
        containers = [p for p in (paths["container"], paths["target"]) if p.exists() or p.is_symlink()]
        if len(containers) != 1 or containers[0].is_symlink(): raise RuntimeError("ambiguous publication container")
        for key, path in (("store", paths["store"]), ("container", containers[0])):
            info = path.lstat()
            if not stat.S_ISDIR(info.st_mode) or info.st_uid != 0 or stat.S_IMODE(info.st_mode) not in ((0o700,) if key == "store" else (0o700, 0o555)) or info.st_ino != publication["objects"][key]:
                raise RuntimeError("preallocated publication object changed")
        if publication["state"] not in ("observing", "sealing"):
            owned_file(paths["store_config"], 0o444)
            if checksum(paths["store_config"]) != publication["config_sha256"]: raise RuntimeError("publication configuration changed")
    if "fixture_activation" in record:
        activation = record["fixture_activation"]
        if activation["state"] not in ("sealing", "prepared", "executing", "applied"): raise RuntimeError("unrecognized activation provisioning state")
        if paths["activation_config"].exists() or paths["activation_config"].is_symlink():
            owned_file(paths["activation_config"], 0o444)
            if checksum(paths["activation_config"]) != activation["config_sha256"]: raise RuntimeError("activation configuration changed")
        elif activation["state"] != "sealing": raise RuntimeError("activation configuration missing")


def fixture_prepare(record):
    if "fixture" not in record:
        verified(record)
        stop(record)
        for path in (ROOT / "fixtures", ROOT / "registry/fixture.conf", ROOT / "registry/fixture.manifest", ROOT / "registry/fixture.next"):
            if path.exists() or path.is_symlink(): raise RuntimeError("unowned fixture preparation path exists")
        current_boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip().replace("-", "")
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            boot = db.execute("SELECT lower(hex(current_boot_id)) FROM authority_meta").fetchone()[0]
        if boot != current_boot:
            start(record)
            stop(record)
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            scope = db.execute("SELECT revision FROM scopes WHERE scope_id=? AND kind='account' AND os_uid=1000 AND state='active'", (bytes.fromhex(record["owner"]),)).fetchone()
        if scope is None: raise RuntimeError("eligible fixture account missing")
        fixture = {key: uuid.uuid4().hex for key in FIXTURE_IDS}
        fixture.update(scope=record["owner"], boot=current_boot, scope_revision=scope[0],
                       expires=time.clock_gettime_ns(time.CLOCK_BOOTTIME) + 3600 * 10**9, accepted=time.time_ns() // 10**6)
        binary = (BUILD / "probe_service").read_bytes()
        if not binary.startswith(b"\x7fELF") or not 1 <= len(binary) <= 16 * 1024**2:
            raise RuntimeError("fixture probe is not a bounded native ELF")
        manifest = dict(component="aetheri.p0.probe", publisher=fixture["publisher"], version="p0-native", target="x86_64-linux-gnu",
                        files=[dict(path="bin/probe", mode=0o555, size=len(binary), sha256=hashlib.sha256(binary).hexdigest()),
                               dict(path="share/message", mode=0o444, size=6, sha256=hashlib.sha256(b"hello\n").hexdigest())],
                        actions=[dict(id="run", executable="bin/probe", arguments=["check", record["owner"]])], dependencies=[])
        encoded = runtime_contracts.fixture(manifest)
        fixture.update(manifest=manifest, artifact=hashlib.sha256(encoded).hexdigest())
        intent = dict(plan=fixture["plan"], authority=record["epochs"]["system"], actor=fixture["actor"], scope=fixture["scope"],
                      kind="install", component=manifest["component"], install=None, generation=None, data=None,
                      artifact=fixture["artifact"], grant=None, retention="none", action=None, boot=current_boot, expires=fixture["expires"],
                      reads=[dict(epoch=record["epochs"]["system"], kind="scope", ref=fixture["scope"], revision=scope[0])],
                      effects=[dict(number=n, kind=kind, component=manifest["component"], ref=fixture[key], disposition="create", reason="fixture_prepare")
                               for n, (kind, key) in enumerate((("install", "install"), ("generation", "generation"), ("storage", "slot")))])
        fixture["intent_digest"] = hashlib.sha256(runtime_contracts.intent(intent)).hexdigest()
        record.update(fixture=fixture, fixture_state="preparing")
        save(record)  # Administrative carrier-infrastructure intent before effects.
        paths = fixture_paths(record)
        directory(ROOT / "fixtures", "aetheri_root_t", mode=0o700)
        directory(paths["parent"], "aetheri_stage_parent_t", mode=0o700)
        directory(paths["source"], "aetheri_fixture_source_t", mode=0o700)
        for name in ("bin", "share"): directory(paths["source"] / name, "aetheri_fixture_source_t", mode=0o700)
        publish(paths["source"] / "bin/probe", binary, "aetheri_fixture_source_t", mode=0o555)
        publish(paths["source"] / "share/message", b"hello\n", "aetheri_fixture_source_t", mode=0o444)
        for name in ("bin", "share"): (paths["source"] / name).chmod(0o555)
        paths["source"].chmod(0o555)
        directory(paths["staging"], "aetheri_staging_t", mode=0o700)
        publish(paths["manifest"], encoded, "aetheri_registry_t")
        publish(paths["config"], fixture_config(record, bytes(72)), "aetheri_registry_t")
        for path in (paths["config"], paths["manifest"], paths["source"] / "bin/probe", paths["source"] / "share/message"):
            flush_path(path)
        for path in (paths["source"] / "bin", paths["source"] / "share", paths["source"], paths["staging"], paths["parent"], ROOT / "fixtures", ROOT / "registry", ROOT):
            flush_path(path, True)
        fixture["objects"] = {key: [paths[key].stat().st_ino, paths[key].stat().st_ctime_ns] for key in ("source", "staging")}
        fixture["config_sha256"] = checksum(paths["config"])
        record["fixture_state"] = "observing"
        save(record)
    verified(record, allow_unfinished_fixture=True)
    stop(record)
    fixture, paths = record["fixture"], fixture_paths(record)
    if record["fixture_state"] == "observing":
        unit = start_unit(record, "system", fixture="observe")
        output = run("journalctl", "-u", unit, "--output=cat", "--no-pager")
        values = re.findall(r"^fixture-roots ([0-9a-f]{144})$", output, re.M)
        if len(values) != 1: raise RuntimeError("native root observation missing/ambiguous")
        roots = bytes.fromhex(values[0])
        for offset, key in ((0, "source"), (36, "staging")):
            if struct.unpack_from(">Q", roots, offset + 16)[0] != fixture["objects"][key][0]:
                raise RuntimeError("native observation differs from independently provisioned inode")
        fixture["roots"] = values[0]
        fixture["sealed_sha256"] = hashlib.sha256(fixture_config(record, roots)).hexdigest()
        record["fixture_state"] = "sealing"
        save(record)
    if record["fixture_state"] == "sealing":
        data = fixture_config(record, bytes.fromhex(fixture["roots"]))
        temporary = ROOT / "registry/fixture.next"
        if checksum(paths["config"]) != fixture["sealed_sha256"]:
            if not temporary.exists(): publish(temporary, data, "aetheri_registry_t")
            owned_file(temporary, 0o444)
            if temporary.read_bytes() != data: raise RuntimeError("uncertain fixture seal needs reconciliation")
            flush_path(temporary)
            os.replace(temporary, paths["config"])
        flush_path(paths["config"])
        flush_path(ROOT / "registry", True)
        fixture["config_sha256"] = fixture["sealed_sha256"]
        record["fixture_state"] = "prepared"
        save(record)
    verify_fixture(record)
    print("Protected native ELF fixture/source/slot configuration is sealed; this provisioning action accepts no operation.")


def fixture_check(record):
    verified(record)
    stop(record)
    fixture, paths = record["fixture"], fixture_paths(record)
    def journal():
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            return db.execute("SELECT o.state,o.phase,o.outcome,o.revision,s.state,lower(hex(s.evidence_digest)),a.store_state,i.state,g.publication_state FROM operations o JOIN operation_steps s USING(operation_id) JOIN artifacts a ON a.artifact_id=? JOIN installs i ON i.install_id=? JOIN generations g ON g.generation_id=? WHERE o.operation_id=?",
                              tuple(bytes.fromhex(fixture[key]) for key in ("artifact", "install", "generation", "operation"))).fetchone()
    start_unit(record, "system", fixture="stage")
    first = journal()
    expected = ("running", "publish", None, first[3] if first else None, "applied", fixture["config_sha256"], "staged", "preparing", "staged")
    if first != expected or first[3] < 3: raise AssertionError(("fixture journal did not retain pending publication", first))
    snapshot = {}
    for file in fixture["manifest"]["files"]:
        path = paths["staging"] / file["path"]
        owned_file(path, file["mode"])
        if checksum(path) != file["sha256"]: raise AssertionError("native staged content differs")
        snapshot[file["path"]] = path.stat().st_ino
    start_unit(record, "system", fixture="stage")
    if journal() != first or any((paths["staging"] / name).stat().st_ino != inode for name, inode in snapshot.items()):
        raise AssertionError("replay rewrote a staged object/journal")
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    first_boot = record.setdefault("fixture_first_boot", boot)
    record.update(fixture_state="stage-ready", fixture_check=dict(native_elf=True, native_journal=True,
                  exact_replay=True, cross_boot_reopen=first_boot != boot, stage_nonexecutable=True,
                  operation_completed=False, artifact_published=False))
    save(record)
    print(json.dumps(record["fixture_check"]))


def fixture_store_prepare(record):
    verified(record)
    stop(record)
    paths = fixture_paths(record)
    if "fixture_publication" not in record:
        if record["fixture_state"] != "stage-ready": raise RuntimeError("stage verification is required first")
        for path in (ROOT / "store", paths["container"], paths["store_config"]):
            if path.exists() or path.is_symlink(): raise RuntimeError("unowned publication infrastructure exists")
        record["fixture_publication"] = dict(state="preparing")
        save(record)  # Root-provisioned carrier infrastructure, not dynamic allocation.
        directory(ROOT / "store", "aetheri_root_t", mode=0o700)
        directory(paths["store"], "aetheri_store_parent_t", mode=0o700)
        directory(paths["container"], "aetheri_publication_t", mode=0o700)
        for path in (paths["container"], paths["parent"], paths["store"], ROOT / "store", ROOT): flush_path(path, True)
        record["fixture_publication"].update(state="observing", objects={key: paths[key].stat().st_ino for key in ("store", "container")})
        save(record)
    publication = record["fixture_publication"]
    if publication["state"] == "observing":
        unit = start_unit(record, "system", fixture="store-observe")
        output = run("journalctl", "-u", unit, "--output=cat", "--no-pager")
        values = re.findall(r"^fixture-store-roots ([0-9a-f]{144})$", output, re.M)
        if len(values) != 1: raise RuntimeError("publication object observation missing/ambiguous")
        roots = bytes.fromhex(values[0])
        for offset, key in ((0, "store"), (36, "container")):
            if struct.unpack_from(">Q", roots, offset + 16)[0] != publication["objects"][key]: raise RuntimeError("publication observation differs from provisioned inode")
        data = b"AETHSTO1" + bytes.fromhex(record["fixture"]["config_sha256"]) + roots
        publication.update(state="sealing", config=data.hex(), config_sha256=hashlib.sha256(data).hexdigest())
        save(record)
    if publication["state"] == "sealing":
        data = bytes.fromhex(publication["config"])
        if not paths["store_config"].exists(): publish(paths["store_config"], data, "aetheri_registry_t")
        owned_file(paths["store_config"], 0o444)
        if paths["store_config"].read_bytes() != data: raise RuntimeError("uncertain publication configuration differs")
        flush_path(paths["store_config"])
        flush_path(ROOT / "registry", True)
        publication["state"] = "prepared"
        save(record)
    verified(record)
    print("Publication container and store witness configuration sealed; no payload moved or publication step accepted.")


def fixture_store_check(record):
    verified(record)
    stop(record)
    fixture, publication = record["fixture"], record["fixture_publication"]
    if publication["state"] not in ("prepared", "executing", "published"): raise RuntimeError("publication infrastructure not sealed")
    publication["state"] = "executing" if publication["state"] != "published" else "published"
    save(record)  # Permit known alternate locations after an interrupted native dispatch.
    def journal():
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            return db.execute("SELECT o.state,o.phase,o.outcome,o.revision,s.state,lower(hex(s.evidence_digest)),a.store_state,i.state,g.publication_state,g.activated_once,i.active_generation_id,o.reason_code FROM operations o JOIN operation_steps s USING(operation_id) JOIN artifacts a ON a.artifact_id=? JOIN installs i ON i.install_id=? JOIN generations g ON g.generation_id=? WHERE o.operation_id=? AND s.step_no=1",
                              tuple(bytes.fromhex(fixture[key]) for key in ("artifact", "install", "generation", "operation"))).fetchone()
    start_unit(record, "system", fixture="store-publish")
    first = journal()
    activated = first is not None and first[0] == "completed"
    expected = ("completed" if activated else "running", "reconcile", "applied" if activated else None, first[3] if first else None,
                "applied", publication["config_sha256"], "verified", "active" if activated else "preparing", "committed",
                1 if activated else 0, bytes.fromhex(fixture["generation"]) if activated else None, "" if activated else "activation_pending")
    if first != expected: raise AssertionError(("publication journal differs", first))
    if activated:
        activation = record["fixture_activation"]
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            receipt = db.execute("SELECT lower(hex(evidence_digest)) FROM operation_steps WHERE operation_id=? AND step_no=2 AND action='activate' AND subject_kind='generation' AND subject_ref=? AND state='applied'",
                                 (bytes.fromhex(fixture["operation"]), bytes.fromhex(fixture["generation"]))).fetchall()
        if receipt != [(activation["config_sha256"],)]: raise AssertionError("active publication has no matching activation receipt")
    paths = fixture_paths(record)
    container, payload = paths["target"], paths["target"] / "payload"
    if container.stat().st_ino != publication["objects"]["container"] or payload.stat().st_ino != fixture["objects"]["staging"][0]: raise AssertionError("publication copied/replaced an expected root")
    expected_paths = {".", "manifest", "payload", "payload/bin", "payload/share"} | {"payload/" + f["path"] for f in fixture["manifest"]["files"]}
    if {"."} | {p.relative_to(container).as_posix() for p in container.rglob("*")} != expected_paths: raise AssertionError("published inventory differs")
    snapshot = {p.relative_to(container).as_posix(): (p.stat().st_ino, p.stat().st_mode, p.stat().st_ctime_ns) for p in [container, *container.rglob("*")]}
    for path in (container, payload, payload / "bin", payload / "share"):
        if stat.S_IMODE(path.stat().st_mode) != 0o555 or os.getxattr(path, "security.selinux").rstrip(b"\0") != b"system_u:object_r:aetheri_store_dir_t:s0": raise AssertionError("store directory is not sealed")
    owned_file(container / "manifest", 0o444)
    if checksum(container / "manifest") != fixture["artifact"]: raise AssertionError("published manifest differs")
    for file in fixture["manifest"]["files"]:
        path = payload / file["path"]
        owned_file(path, file["mode"])
        expected_type = "aetheri_store_code_t" if file["mode"] == 0o555 else "aetheri_store_data_t"
        if checksum(path) != file["sha256"] or os.getxattr(path, "security.selinux").rstrip(b"\0") != ("system_u:object_r:" + expected_type + ":s0").encode(): raise AssertionError("published content/label differs")
    if os.getxattr(container / "manifest", "security.selinux").rstrip(b"\0") != b"system_u:object_r:aetheri_store_manifest_t:s0": raise AssertionError("manifest not sealed")
    start_unit(record, "system", fixture="store-publish")
    if journal() != first or snapshot != {p.relative_to(container).as_posix(): (p.stat().st_ino, p.stat().st_mode, p.stat().st_ctime_ns) for p in [container, *container.rglob("*")]}: raise AssertionError("publication retry rewrote objects/metadata")
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    first_boot = publication.setdefault("first_boot", boot)
    publication.update(state="published", check=dict(native_journal=True, original_root_inodes=True, sealed_types=True,
                       exact_replay=True, cross_boot_reopen=first_boot != boot, store_setattr_denied=True, activated=activated, operation_completed=activated))
    save(record)
    verified(record)
    print(json.dumps(publication["check"]))


def fixture_activation_prepare(record):
    verified(record)
    stop(record)
    paths = fixture_paths(record)
    if "fixture_activation" not in record:
        if record["fixture_publication"]["state"] != "published": raise RuntimeError("verified publication is required first")
        if paths["activation_config"].exists() or paths["activation_config"].is_symlink(): raise RuntimeError("unowned activation configuration exists")
        # Reconcile the actual boot/runtime and close bootstrap-open gates through
        # native normal startup, not Python business-row writes. No app is run.
        start(record)
        stop(record)
        fixture = record["fixture"]
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            revisions = db.execute("SELECT i.revision,g.revision,f.revision,f.state,f.closing_operation_id,o.state,o.phase FROM installs i JOIN generations g ON g.generation_id=? JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=i.install_id JOIN operations o ON o.operation_id=? WHERE i.install_id=? AND i.state='preparing' AND i.active_generation_id IS NULL AND g.activated_once=0 AND g.publication_state='committed'",
                                   tuple(bytes.fromhex(fixture[key]) for key in ("generation", "operation", "install"))).fetchone()
        if revisions is None or revisions[3:] != ("closed", None, "running", "reconcile"): raise RuntimeError("installation cannot accept first activation")
        stamp = time.time_ns() // 10**6
        data = b"AETHACT1" + bytes.fromhex(record["fixture_publication"]["config_sha256"]) + struct.pack(">QQQQ", *revisions[:3], stamp)
        record["fixture_activation"] = dict(state="sealing", config=data.hex(), config_sha256=hashlib.sha256(data).hexdigest(),
                                             stamp=stamp, revisions=list(revisions[:3]))
        save(record)  # Immutable administrative evidence before config publication.
    activation = record["fixture_activation"]
    if activation["state"] == "sealing":
        data = bytes.fromhex(activation["config"])
        if not paths["activation_config"].exists(): publish(paths["activation_config"], data, "aetheri_registry_t")
        owned_file(paths["activation_config"], 0o444)
        if paths["activation_config"].read_bytes() != data: raise RuntimeError("uncertain activation configuration differs")
        flush_path(paths["activation_config"])
        flush_path(ROOT / "registry", True)
        activation["state"] = "prepared"
        save(record)
    verified(record)
    print("First-activation evidence retained; provisioning itself performs no activation or launch.")


def fixture_activation_check(record):
    verified(record)
    stop(record)
    fixture, activation = record["fixture"], record["fixture_activation"]
    if activation["state"] not in ("prepared", "executing", "applied"): raise RuntimeError("activation evidence is not sealed")
    activation["state"] = "executing" if activation["state"] != "applied" else "applied"
    save(record)
    paths = fixture_paths(record)
    container = paths["target"]
    def snapshot():
        return {p.relative_to(container).as_posix(): (p.stat().st_ino,p.stat().st_mode,p.stat().st_ctime_ns) for p in [container,*container.rglob("*")]}
    before = snapshot()
    def journal():
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            receipt = db.execute("SELECT o.state,o.outcome,o.result_kind,lower(hex(o.result_ref)),o.completed_unix_ms,o.revision,s.state,lower(hex(s.evidence_digest)),i.state,lower(hex(i.active_generation_id)),i.revision,g.activated_once,g.revision,f.state,f.revision,f.closing_operation_id FROM operations o JOIN operation_steps s USING(operation_id) JOIN installs i ON i.install_id=? JOIN generations g ON g.generation_id=? JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=i.install_id WHERE o.operation_id=? AND s.step_no=2",
                                 tuple(bytes.fromhex(fixture[key]) for key in ("install", "generation", "operation"))).fetchone()
            live = db.execute("SELECT (SELECT count(*) FROM generation_leases WHERE state<>'released')+(SELECT count(*) FROM launch_dispatches)").fetchone()[0]
        if live: raise AssertionError("activation created/admitted launch obligations")
        return receipt
    start_unit(record, "system", fixture="activate")
    first = journal()
    install_revision, generation_revision, gate_revision = activation["revisions"]
    expected = ("completed", "applied", "install", fixture["install"], activation["stamp"], first[5] if first else None,
                "applied", activation["config_sha256"], "active", fixture["generation"], install_revision+1,
                1, generation_revision+1, "closed", gate_revision, None)
    if first != expected or snapshot() != before: raise AssertionError(("activation metadata/content differs", first))
    database_hash = checksum(state_path(record, "system") / "catalogue.sqlite3")
    start_unit(record, "system", fixture="activate")
    if journal() != first or snapshot() != before or checksum(state_path(record, "system") / "catalogue.sqlite3") != database_hash: raise AssertionError("completed activation retry rewrote the receipt/object")
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    first_boot = activation.setdefault("first_boot", boot)
    activation.update(state="applied", check=dict(native_transaction=True, exact_completed_replay=True, publication_unchanged=True,
                       gate_closed=True, cross_boot_reopen=first_boot != boot, app_dispatched=False))
    record["fixture_publication"]["check"].update(activated=True, operation_completed=True)
    save(record)
    verified(record)
    print(json.dumps(activation["check"]))


def fixture_rebind_check(record):
    verified(record)
    stop(record)
    paths, fixture = fixture_paths(record), record["fixture"]
    clone = paths["parent"] / "copied-source"
    if clone.exists() or clone.is_symlink() or record.get("fixture_rebind", {}).get("state") not in (None, "complete"):
        raise RuntimeError("existing substitution probe needs explicit reconciliation")
    record["fixture_rebind"] = dict(state="preparing")
    save(record)
    directory(clone, "aetheri_fixture_source_t", mode=0o700)
    for name in ("bin", "share"): directory(clone / name, "aetheri_fixture_source_t", mode=0o700)
    for file in fixture["manifest"]["files"]:
        data = (paths["source"] / file["path"]).read_bytes()
        if hashlib.sha256(data).hexdigest() != file["sha256"]: raise RuntimeError("original fixture changed")
        publish(clone / file["path"], data, "aetheri_fixture_source_t", mode=file["mode"])
    for name in ("bin", "share"): (clone / name).chmod(0o555)
    clone.chmod(0o555)
    original, copied = paths["source"].stat(), clone.stat()
    if original.st_dev != copied.st_dev or original.st_ino == copied.st_ino:
        raise AssertionError("substitution probe did not create a distinct same-filesystem root")
    snapshot = {path.relative_to(clone).as_posix(): path.lstat().st_ino for path in [clone, *clone.rglob("*")]}
    record["fixture_rebind"].update(state="prepared", inodes=snapshot)
    save(record)
    database = state_path(record, "system") / "catalogue.sqlite3"
    before = checksum(database)
    try:
        start_unit(record, "system", fixture="stage", fixture_source_override=clone)
    except subprocess.CalledProcessError:
        failure = record.get("last_failure", {})
        unit = failure.get("unit")
        if unit is None or failure["facts"].get("ExecMainStatus") != "4":
            raise RuntimeError("substitution failed before the native administrative check")
        output = run("journalctl", "-u", unit, "--output=cat", "--no-pager")
        if f"Authority administrative action failed: {-errno.ESTALE}" not in output:
            raise RuntimeError("substitution was not rejected by native object identity")
    else:
        raise AssertionError("native fixture accepted a copied root")
    if record["pending"] or checksum(database) != before:
        raise AssertionError("rejected root changed the journal or left an unknown dispatch")
    stop(record)  # Verify all owned units/jobs/cgroups have drained before cleanup.
    if snapshot != {path.relative_to(clone).as_posix(): path.lstat().st_ino for path in [clone, *clone.rglob("*")]}:
        raise RuntimeError("owned substitution tree changed; preserve it")
    if set(snapshot) != {".", "bin", "bin/probe", "share", "share/message"}:
        raise RuntimeError("unexpected substitution inventory")
    for file in fixture["manifest"]["files"]:
        path = clone / file["path"]
        owned_file(path, file["mode"])
        if checksum(path) != file["sha256"]: raise RuntimeError("substitution bytes changed; preserve it")
    for path in (clone / "bin", clone / "share", clone):
        info = path.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != 0 or stat.S_IMODE(info.st_mode) != 0o555:
            raise RuntimeError("owned substitution directory changed; preserve it")
    # Explicit verified probe retirement only, never real installation/data roots.
    for path in (clone / "bin", clone / "share", clone): path.chmod(0o700)
    for file in fixture["manifest"]["files"]: (clone / file["path"]).unlink()
    for path in (clone / "bin", clone / "share", clone): path.rmdir()
    flush_path(paths["parent"], True)
    record["fixture_rebind"].update(state="complete", identical_bytes=True, distinct_inode=True,
                                   native_estale=True, journal_unchanged=True, probe_removed=True)
    save(record)
    verify_fixture(record)
    print(json.dumps({key: value for key, value in record["fixture_rebind"].items() if key != "inodes"}))


def unit_has_job(unit):
    jobs = json.loads(bus("system", "--json=short", "call", "org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "ListJobs"))
    return jobs["type"] != "a(usssoo)" or any(job[1] == unit for job in jobs["data"][0])


def publish_broker_slot(record, unit):
    temporary = ROOT / "registry/broker.next"
    publish(temporary, ("AETHERI-P0-BROKER-1\nunit=" + unit + "\n").encode(), "aetheri_registry_t")
    os.replace(temporary, ROOT / "registry/broker.conf")
    record["broker_unit"] = unit


def publish_projection_slot(record,unit):
    pattern="aetheri-p0-"+record["token"].replace("-","")+r"-probe-projection-[0-9]+\.service"
    if unit!="-" and re.fullmatch(pattern,unit) is None:
        raise ValueError("projection slot requires an owned projection unit")
    temporary=ROOT / "registry/projection.next"
    publish(temporary,("AETHERI-P0-PROJECTION-1\nunit="+unit+"\n").encode(),"aetheri_registry_t")
    flush_path(temporary)
    os.replace(temporary,ROOT / "registry/projection.conf")
    flush_path(ROOT / "registry",True)
    record["projection_unit"]=unit


def start_unit(record, role, init=False, probe=None, claims=None, enroll=False, storage=None, fixture=None, fixture_source_override=None, data=None):
    if data is not None and (data not in ("observe","enroll") or role!="user" or init or probe or enroll or storage or fixture):
        raise ValueError("data administration requires only the user administrative role")
    if probe=="projection" and (role!="user" or init or enroll or storage or fixture):
        raise ValueError("projection controller requires only the user probe role")
    if fixture is not None and (fixture not in ("observe", "stage", "store-observe", "store-publish", "activate") or role != "system" or init or probe or enroll or storage):
        raise ValueError("fixture administration requires only the system role")
    if fixture_source_override is not None and (fixture != "stage" or fixture_source_override != fixture_paths(record)["parent"] / "copied-source"):
        raise ValueError("only the owned fixed fixture substitution probe may bind a source")
    if enroll and (role != "system" or init or probe):
        raise ValueError("account enrollment requires only the system administrative role")
    if storage is not None and (storage not in ("publish", "verify") or role != "user" or init or probe or enroll):
        raise ValueError("storage administration requires only the user role")
    suffix = ("probe-" + probe) if probe else role + ("-init" if init else "-enroll" if enroll else "-fixture-" + fixture if fixture else "-storage-" + storage if storage else "-data-"+data if data else "")
    if record["pending"]:
        raise RuntimeError("previous dispatch is unacknowledged")
    unit = "aetheri-p0-" + record["token"].replace("-", "") + "-" + suffix + "-" + str(len(record["units"])) + ".service"
    if role == "user" and not init and probe is None and storage is None and data is None:
        publish_broker_slot(record, unit)
    if probe=="projection":
        publish_projection_slot(record,unit)
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
            if "app_data" in record: command.append(record["app_data"]["owner"]["storage"])
        if probe == "broker":
            properties["SELinuxContext"] = "system_u:system_r:aetheri_user_t:s0"
            properties.pop("ReadWritePaths")
            command = [str(ROOT / "probe-broker"), *claims]
        if probe == "projection":
            properties["SELinuxContext"]="system_u:system_r:aetheri_user_t:s0"
            properties.pop("ReadWritePaths")
            command=[str(ROOT / "probe-projection")]
    else:
        properties.update(SELinuxContext="system_u:system_r:aetheri_" + role + "_t:s0", ReadWritePaths=str(state_path(record, role)))
        command = [str(ROOT / (role + "-authority")), role] + (["--init"] if init else ["--enroll-account"] if enroll else ["--fixture-" + fixture] if fixture else ["--storage-" + storage] if storage else ["--data-"+data] if data else [])
        if fixture in ("store-observe", "store-publish", "activate"):
            properties["ReadWritePaths"] = str(ROOT)
            properties["ReadOnlyPaths"] = str(fixture_paths(record)["source"])
        elif fixture:
            properties["ReadWritePaths"] += " " + str(fixture_paths(record)["staging"])
        if fixture_source_override is not None:
            properties["BindReadOnlyPaths"] = str(fixture_source_override) + ":" + str(fixture_paths(record)["source"])
    if role == "user" or probe:
        properties.update(BindsTo="user@1000.service", After="user@1000.service",
                          BindReadOnlyPaths="/run/user/1000/bus:" + str(ROOT / "user-bus"))
    arguments = ["systemd-run", "--unit=" + unit, "--service-type=" + ("exec" if init or probe or enroll or storage or fixture or data else "notify")]
    if init or probe or enroll or storage or fixture or data:
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
        paths = ["/org/aetheri/Management1", "/org/aetheri/Runtime1", "/org/aetheri/Broker1", "/org/aetheri/Catalogue1"] if role == "system" else ["/org/aetheri/Management1", "/org/aetheri/Documents1", "/org/aetheri/Development1"]
        name = "org.aetheri.System1" if role == "system" else "org.aetheri.User1"
        for path in paths:
            live = ET.fromstring(json.loads(bus(role, "--json=short", "call", name, path, "org.freedesktop.DBus.Introspectable", "Introspect"))["data"][0])
            expected_file = {"Management1": "management-v1.xml", "Runtime1": "runtime-v1.xml", "Documents1": "documents-v1.xml", "Broker1": "broker-v1.xml", "Catalogue1": "catalogue-v1.xml", "Development1": "development-v1.xml"}[path.rsplit("/", 1)[1]]
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
    user_run(str(BUILD / "probe_broker"), *claims, "--direct-user-bus")
    start_unit(record, "user", probe="broker", claims=claims)
    record["report"] = report
    save(record)
    print(json.dumps({"confined_endpoints": 2, "live_xml": True, "real_boot": True, "app_public_ipc_and_denials": True}))


def projection_check(record):
    verified(record)
    start(record)
    fixture=record["fixture"]
    user_database=state_path(record,"user") / "catalogue.sqlite3"
    def state():
        with sqlite3.connect(f"file:{user_database}?mode=ro",uri=True) as db:
            binding=db.execute("SELECT component_id,lower(hex(publisher_ref)),lower(hex(owner_scope)),lower(hex(system_epoch)),install_revision,state FROM install_bindings WHERE install_id=?",(bytes.fromhex(fixture["install"]),)).fetchall()
            revision=db.execute("SELECT catalogue_revision FROM authority_meta").fetchone()[0]
            other={name:db.execute('SELECT * FROM "'+name+'" ORDER BY rowid').fetchall() for name in ("data_epochs","attachments","launches","grants","access_leases","admission_gates")}
        return binding,revision,other
    before=state()
    system_database=state_path(record,"system") / "catalogue.sqlite3"
    system_hash=checksum(system_database)
    with sqlite3.connect(f"file:{system_database}?mode=ro",uri=True) as db:
        root_revision=db.execute("SELECT revision FROM installs WHERE install_id=? AND state='active'",(bytes.fromhex(fixture["install"]),)).fetchone()[0]
        system_runtime=db.execute("SELECT lower(hex(runtime_incarnation)) FROM authority_meta").fetchone()[0]
    def refresh():
        unit=start_unit(record,"user",probe="projection")
        output=run("journalctl","-u",unit,"--output=cat","--no-pager")
        replies=re.findall(r"^projection ([0-9a-f]{32}) ([0-9]+) ([0-9]+)$",output,re.M)
        if len(replies)!=1 or replies[0][0]!=fixture["install"] or int(replies[0][1])!=root_revision: raise AssertionError("native projection reply differs")
        stop_unit(unit)
        publish_projection_slot(record,"-")
        save(record)
        return int(replies[0][2])
    local_revision=refresh()
    first=state()
    expected=[(fixture["manifest"]["component"],fixture["publisher"],record["owner"],record["epochs"]["system"],root_revision,"active")]
    if first[0]!=expected or first[1]!=local_revision or first[1]!=before[1]+(0 if before[0] else 1) or first[2]!=before[2]: raise AssertionError("user binding/projection effects differ")
    first_hash=checksum(user_database)
    if refresh()!=local_revision or state()!=first or checksum(user_database)!=first_hash or checksum(system_database)!=system_hash: raise AssertionError("projection retry rewrote owner data")
    try:
        user_run("busctl","--user","call","org.aetheri.User1","/org/aetheri/Development1","org.aetheri.Development1","RefreshFixtureInstall")
    except subprocess.CalledProcessError as error:
        if "Protected P0 controller unit required" not in error.stderr: raise
    else: raise AssertionError("same-UID process invoked the protected development trigger")
    if checksum(user_database)!=first_hash or checksum(system_database)!=system_hash: raise AssertionError("rejected projection caller changed a catalogue")
    boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    initial=record.setdefault("projection_first_context",dict(boot=boot,system_runtime=system_runtime))
    record["projection_check"]=dict(native_rpc=True,separate_owner_writer=True,exact_retry=True,protected_controller=True,
        no_data_or_gate_effects=True,cross_boot_reopen=initial["boot"]!=boot,system_runtime_changed=initial["system_runtime"]!=system_runtime)
    save(record)
    print(json.dumps(record["projection_check"]))


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
    if "app_data" in record:
        raise RuntimeError("enrolled app data is not an empty bootstrap; preserve its allocation and metadata")
    if record["pending"]:
        raise RuntimeError("unacknowledged dispatch requires reconciliation")
    verified(record)
    stop(record)
    if "storage" in record:
        raise RuntimeError("allocated private data is not an empty bootstrap; use a reviewed storage retirement protocol")
    if "fixture" in record:
        raise RuntimeError("prepared installation is not an empty bootstrap; preserve its journal and staging")
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
    files[ROOT / "probe-projection"] = (0, 0o555)
    if "projection_unit" in record: files[ROOT / "registry/projection.conf"]=(0,0o444)
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
    verified(record, allow_new_probe=True, allow_unfinished_fixture=True, allow_new_projection=True)
    stop(record)
    record["state"] = "updating-owned-inputs"
    save(record)
    cil = ROOT / (MODULE + ".cil")
    new_policy = (SOURCE / "policy/p0-services.cil").read_bytes()
    cil.write_bytes(new_policy)
    if hashlib.sha256(new_policy).hexdigest() != record["policy_sha256"]:
        run("semodule", "-X", "400", "-i", str(cil))
    record["policy_sha256"] = checksum(cil)
    if "fixture" in record:
        parent = fixture_paths(record)["parent"]
        expected_inode = record.setdefault("fixture_parent_inode", parent.stat().st_ino)
        save(record)  # Same protected slot, explicit type upgrade; never another root.
        if parent.lstat().st_ino != expected_inode or parent.is_symlink(): raise RuntimeError("fixture parent changed during policy upgrade")
        label(parent, "aetheri_stage_parent_t")
        flush_path(parent, True)
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
    temporary=ROOT / "probe-projection.next"
    publish(temporary,(BUILD / "probe_projection").read_bytes(),"aetheri_user_exec_t",mode=0o555)
    os.replace(temporary,ROOT / "probe-projection")
    if "projection_unit" not in record:
        if (ROOT / "registry/projection.conf").exists() or (ROOT / "registry/projection.conf").is_symlink(): raise RuntimeError("unregistered projection slot exists")
        publish_projection_slot(record,"-")
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
                  broker_probe_sha256=checksum(BUILD / "probe_broker"),projection_probe_sha256=checksum(BUILD / "probe_projection"), state="prepared")
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
        elif action == "projection-check": projection_check(record)
        elif action == "update-owned": update_owned(record)
        elif action == "enroll-account": enroll_account(record)
        elif action == "account-check": account_check(record)
        elif action == "storage-prepare": storage_prepare(record)
        elif action == "storage-check": storage_check(record)
        elif action == "data-prepare": data_prepare(record)
        elif action == "data-check": data_check(record)
        elif action == "fixture-prepare": fixture_prepare(record)
        elif action == "fixture-check": fixture_check(record)
        elif action == "fixture-store-prepare": fixture_store_prepare(record)
        elif action == "fixture-store-check": fixture_store_check(record)
        elif action == "fixture-activation-prepare": fixture_activation_prepare(record)
        elif action == "fixture-activation-check": fixture_activation_check(record)
        elif action == "fixture-rebind-check": fixture_rebind_check(record)
        else: raise RuntimeError("unsupported controller action")
    finally:
        os.close(marker)


if __name__ == "__main__":
    main()
