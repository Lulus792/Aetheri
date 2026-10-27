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
    pattern = "aetheri-p0-" + record["token"].replace("-", "") + r"-(?:system(?:-(?:init|enroll|recover-setup|fixture-(?:exec-)?(?:observe|stage|activate|store-(?:observe|publish))))?|user(?:-(?:init|storage-(?:publish|verify)|data-(?:exec-)?(?:observe|enroll)))?|probe-(?:check|claim|broker|projection))-[0-9]+\.service"
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
    for data_selection in (record,record.get("exec_fixture",{})):
        if "app_data" not in data_selection: continue
        data=data_selection["app_data"]
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
    for selected_fixture in (record,record.get("exec_fixture",{})):
        if "fixture" not in selected_fixture: continue
        fixture=selected_fixture["fixture"]
        if fixture["scope"] != record["owner"]:
            raise RuntimeError("fixture scope changed")
        for key in FIXTURE_IDS:
            value = fixture[key]
            if not isinstance(value, str) or re.fullmatch("[0-9a-f]{32}", value) is None or uuid.UUID(hex=value).version != 4:
                raise RuntimeError("invalid fixture identity")
    if "fixture" in record.get("exec_fixture",{}):
        if "fixture_publication" not in record: raise RuntimeError("execution fixture has no enrolled shared store")
        old,new=record["fixture"],record["exec_fixture"]["fixture"]
        keys=("plan","operation","request_key","install","generation","slot")
        if {old[k] for k in keys}&{new[k] for k in keys} or old["artifact"]==new["artifact"]:
            raise RuntimeError("execution fixture aliases an original immutable identity")
    for selected in (record,record.get("exec_fixture",{})):
        if "app_data" in selected and (selected["app_data"]["install"]!=selected["fixture"]["install"] or selected["app_data"]["publisher"]!=selected["fixture"]["publisher"]):
            raise RuntimeError("app data belongs to another selected installation")
    if "app_data" in record.get("exec_fixture",{}):
        fresh=record["exec_fixture"]["app_data"]["owner"]
        keys=("storage","data","allocation_operation")
        for other in (record["storage"],record["app_data"]["owner"]):
            if {fresh[k] for k in keys}&{other[k] for k in keys}: raise RuntimeError("execution data aliases existing private ownership")
    if "setup" in record:
        setup=record["setup"]
        for key in ("launch","lease","root_operation","user_operation"):
            if re.fullmatch("[0-9a-f]{32}",setup[key]) is None or uuid.UUID(hex=setup[key]).version!=4: raise RuntimeError("invalid setup identity")
        if setup["unit"]!="aetheri-app-"+setup["launch"]+".service": raise RuntimeError("foreign setup unit")
        if setup["state"] not in ("prepared","starting","active","failed","drained","withdrawn"): raise RuntimeError("unknown setup state")
    return record


def module_bytes():
    path = Path("/var/lib/selinux/targeted/active/modules/400") / MODULE / "cil"
    data = path.read_bytes()
    return bz2.decompress(data) if data.startswith(b"BZh") else data


def verified(record, allow_new_probe=False, allow_unreceipted_storage=False, allow_unfinished_fixture=False, allow_new_projection=False, allow_unreceipted_app_data=False):
    verify_fixture(record, allow_unfinished_fixture)
    if "exec_fixture" in record: verify_fixture(record,allow_unfinished_fixture,profile="exec")
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
    if "exec_fixture" in record: verify_app_data(record,allow_unreceipted_app_data,profile="exec")
    if "setup_sha256" in record:
        owned_file(ROOT/"setup-helper",0o555)
        if checksum(ROOT/"setup-helper")!=record["setup_sha256"]: raise RuntimeError("setup helper changed")
    if "setup_recovery_sha256" in record:
        owned_file(ROOT/"setup-recovery",0o555)
        if checksum(ROOT/"setup-recovery")!=record["setup_recovery_sha256"]: raise RuntimeError("setup recovery image changed")
    for name in ("admission","reservation","launch","setup"):
        if name+"_config_sha256" in record:
            path=ROOT/"registry"/(name+".conf")
            owned_file(path,0o444)
            if checksum(path)!=record[name+"_config_sha256"]: raise RuntimeError("protected "+name+" configuration changed")


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


def app_data_paths(record,profile="original"):
    selection=fixture_selection(record,profile)
    owner=selection["app_data"]["owner"]
    prefix="exec-" if profile=="exec" else ""
    anchor=ROOT/"users"/record["owner"]/"app-data"
    root=anchor/owner["storage"]
    registry=state_path(record,"user")/"app-allocations"
    return dict(anchor=anchor,root=root,registry=registry,allocation=registry/owner["storage"],
        owner=root/"owner",payload=root/"payload",owner_config=ROOT/"registry"/(prefix+"app-data-owner.conf"),config=ROOT/"registry"/(prefix+"app-data.conf"))


def app_data_config(data,witness):
    result=(b"AETHDTA1"+runtime_contracts.owner(data["owner"])
        +b"".join(bytes.fromhex(data[key]) for key in ("install","publisher","system_epoch","user_epoch"))
        +struct.pack(">QQQ",data["install_revision"],data["base_revision"],data["schema_version"])+witness)
    if len(result)!=368 or len(witness)!=168: raise ValueError("invalid app data configuration")
    return result


def verify_app_data(record,allow_unreceipted=False,profile="original"):
    selection=fixture_selection(record,profile)
    if "app_data" not in selection: return
    if selection["app_data_state"] not in ("prepared","enrolled"): raise RuntimeError("interrupted app data preparation requires reconciliation")
    paths=app_data_paths(record,profile)
    owner=runtime_contracts.owner(selection["app_data"]["owner"])
    for key in ("anchor","root","registry","payload"):
        info=paths[key].lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid!=1000 or info.st_gid!=1000 or stat.S_IMODE(info.st_mode)!=0o700:
            raise RuntimeError("app data directory changed")
    for key,uid,mode,content in (("owner_config",0,0o444,owner),("owner",1000,0o600,owner),
            ("config",0,0o444,app_data_config(selection["app_data"],bytes.fromhex(selection["app_data_witness"])))):
        info=paths[key].lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_uid!=uid or stat.S_IMODE(info.st_mode)!=mode or paths[key].read_bytes()!=content:
            raise RuntimeError("protected app data file changed")
    expected_context="system_u:object_r:aetheri_app_data_t:s0:"+("c35,c36" if profile=="exec" else "c33,c34")
    if os.getxattr(paths["payload"],"security.selinux").rstrip(b"\0")!=expected_context.encode(): raise RuntimeError("app data security category changed")
    entry=paths["allocation"]
    if "app_data_allocation_sha256" in selection:
        info=entry.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_uid!=1000 or info.st_gid!=1000 or stat.S_IMODE(info.st_mode)!=0o600 or checksum(entry)!=selection["app_data_allocation_sha256"]:
            raise RuntimeError("app allocation receipt changed")
    elif (entry.exists() or entry.is_symlink()) and not allow_unreceipted:
        raise RuntimeError("unreceipted app allocation requires explicit data-check reconciliation")


def data_prepare(record,profile="original"):
    selection=fixture_selection(record,profile)
    verified(record)
    if profile=="exec" and record.get("app_data_state")!="enrolled": raise RuntimeError("shared app-data parents have not been enrolled")
    if "app_data" in selection: raise RuntimeError("app data already selected; preserve its identities")
    projection_check(record,profile)
    stop(record)
    with sqlite3.connect(f"file:{state_path(record,'user')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        binding=db.execute("SELECT lower(hex(publisher_ref)),lower(hex(system_epoch)),install_revision FROM install_bindings WHERE install_id=? AND state='active'",(bytes.fromhex(selection["fixture"]["install"]),)).fetchone()
        baseline=db.execute("SELECT catalogue_revision FROM authority_meta").fetchone()[0]
        if binding is None or db.execute("SELECT count(*) FROM data_epochs WHERE component_id=? AND publisher_ref=? AND owner_scope=?",(selection["fixture"]["manifest"]["component"],bytes.fromhex(binding[0]),bytes.fromhex(record["owner"]))).fetchone()[0]!=0 or db.execute("SELECT count(*) FROM attachments WHERE install_id=?",(bytes.fromhex(selection["fixture"]["install"]),)).fetchone()[0]!=0:
            raise RuntimeError("data preparation requires projected installation and no existing data for its component")
    owner={key:uuid.uuid4().hex for key in runtime_contracts.OWNER_FIELDS.split()}
    owner.update(account=record["owner"],machine=record["storage"]["machine"],volume=record["storage"]["volume"])
    data=dict(owner=owner,install=selection["fixture"]["install"],publisher=binding[0],system_epoch=binding[1],
        user_epoch=record["epochs"]["user"],install_revision=binding[2],base_revision=baseline,schema_version=0)
    candidate={**record,"app_data":data} if profile=="original" else {**record,"exec_fixture":{**selection,"app_data":data}}
    paths=app_data_paths(candidate,profile)
    if any(paths[key].exists() or paths[key].is_symlink() for key in (("anchor","registry","root","config","owner_config") if profile=="original" else ("root","config","owner_config"))):
        raise RuntimeError("unknown app data infrastructure exists; no adoption")
    selection.update(app_data=data,app_data_state="preparing")
    save(record)
    if profile=="original":
        directory(paths["registry"],"aetheri_allocation_t",uid=1000,mode=0o700)
        directory(paths["anchor"],"aetheri_user_state_t",uid=1000,mode=0o700)
    directory(paths["root"],"aetheri_user_state_t",uid=1000,mode=0o700)
    directory(paths["payload"],"aetheri_app_data_t",uid=1000,mode=0o700,cats="c35,c36" if profile=="exec" else "c33,c34")
    encoded=runtime_contracts.owner(owner)
    publish(paths["owner_config"],encoded,"aetheri_registry_t")
    publish(paths["owner"],encoded,"aetheri_user_state_t",mode=0o600)
    os.chown(paths["owner"],1000,1000)
    for key in ("owner","owner_config","payload","root","anchor","registry"): flush_path(paths[key],key not in ("owner","owner_config"))
    for parent in (state_path(record,"user"),ROOT/"registry",ROOT/"users"/record["owner"]): flush_path(parent,True)
    unit=start_unit(record,"user",data="observe",data_profile=profile)
    output=run("journalctl","-u",unit,"--output=cat","--no-pager")
    witnesses=re.findall(r"^data-witness=([0-9a-f]{336})$",output,re.M)
    if len(witnesses)!=1: raise RuntimeError("native app data observation missing")
    witness=bytes.fromhex(witnesses[0])
    if witness[:112]!=b"AETHALC1"+encoded: raise RuntimeError("native app data owner differs")
    publish(paths["config"],app_data_config(data,witness),"aetheri_registry_t")
    flush_path(paths["config"]); flush_path(ROOT/"registry",True)
    selection.update(app_data_state="prepared",app_data_witness=witnesses[0])
    save(record)
    print("Prepared distinct protected app data fixture; no native allocation or metadata enrollment yet.")


def data_check(record,profile="original"):
    selection=fixture_selection(record,profile)
    verified(record,allow_unreceipted_app_data=True)
    stop(record)
    paths=app_data_paths(record,profile)
    system_hash=checksum(state_path(record,"system")/"catalogue.sqlite3")
    unrelated=(checksum(storage_paths(record)["record"]),storage_paths(record)["record"].stat().st_ino)
    database=state_path(record,"user")/"catalogue.sqlite3"
    owner=selection["app_data"]["owner"]
    data_id=bytes.fromhex(owner["data"]); install_id=bytes.fromhex(selection["app_data"]["install"])
    def rows():
        with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
            data=db.execute("SELECT lower(hex(data_epoch_id)),lower(hex(storage_ref)),state,data_schema_version,revision FROM data_epochs WHERE data_epoch_id=?",(data_id,)).fetchall()
            attachment=db.execute("SELECT lower(hex(install_id)),lower(hex(data_epoch_id)) FROM attachments WHERE install_id=?",(install_id,)).fetchall()
            gate=db.execute("SELECT state,revision,closing_operation_id FROM admission_gates WHERE subject_kind='data' AND subject_ref=?",(data_id,)).fetchall()
            other={name:db.execute('SELECT * FROM "'+name+'" ORDER BY rowid').fetchall() for name in ("operations","launches","grants","access_leases","install_bindings")}
            for name,column,value in (("data_epochs","data_epoch_id",data_id),("attachments","install_id",install_id),("admission_gates","subject_ref",data_id)):
                other["unrelated_"+name]=db.execute('SELECT * FROM "'+name+'" WHERE '+column+'<>? ORDER BY rowid',(value,)).fetchall()
            revision=db.execute("SELECT catalogue_revision FROM authority_meta").fetchone()[0]
        return data,attachment,gate,other,revision
    before=rows()
    start_unit(record,"user",data="enroll",data_profile=profile)
    first=rows()
    owner=selection["app_data"]["owner"]
    prior_gate=before[2] if before[0] else [("closed",2,None)]
    if len(prior_gate)!=1 or prior_gate[0][0]!="closed" or prior_gate[0][1]<2 or prior_gate[0][2] is not None:
        raise AssertionError("data enrollment retry requires a closed unowned gate")
    expected=([(owner["data"],owner["storage"],"active",0,1)],[(selection["app_data"]["install"],owner["data"])],prior_gate)
    if first[:3]!=expected or first[3]!=before[3] or first[4]!=before[4]+(0 if before[0] else 1): raise AssertionError("native app data enrollment differs")
    first_hash=checksum(database)
    allocation=(checksum(paths["allocation"]),paths["allocation"].stat().st_ino)
    if paths["allocation"].read_bytes()!=bytes.fromhex(selection["app_data_witness"]): raise AssertionError("allocation differs from independent witness")
    start_unit(record,"user",data="enroll",data_profile=profile)
    if rows()!=first or checksum(database)!=first_hash or allocation!=(checksum(paths["allocation"]),paths["allocation"].stat().st_ino): raise AssertionError("data retry changed durable state")
    if checksum(state_path(record,"system")/"catalogue.sqlite3")!=system_hash or unrelated!=(checksum(storage_paths(record)["record"]),storage_paths(record)["record"].stat().st_ino): raise AssertionError("unrelated system/storage fixture changed")
    boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    initial=selection.setdefault("app_data_first_boot",boot)
    selection.update(app_data_state="enrolled",app_data_allocation_sha256=allocation[0])
    selection["app_data_check"]=dict(native_separate_data=True,atomic_closed_gate=True,exact_retry=True,unrelated_storage_preserved=True,cross_boot_reopen=initial!=boot,app_dispatched=False)
    save(record)
    print(json.dumps(selection["app_data_check"]))


FIXTURE_IDS = ("plan", "operation", "actor", "request_key", "scope", "install", "generation", "slot", "publisher", "boot")


def fixture_selection(record,profile):
    if profile=="original": return record
    if profile=="exec": return record.get("exec_fixture",{})
    raise ValueError("unknown protected fixture selection")


def fixture_paths(record,profile="original"):
    selection=fixture_selection(record,profile)
    parent = ROOT / "fixtures" / selection["fixture"]["slot"]
    prefix="exec-" if profile=="exec" else ""
    return dict(parent=parent, source=parent / "source", staging=parent / "staging",
                config=ROOT / "registry" / (prefix+"fixture.conf"), manifest=ROOT / "registry" / (prefix+"fixture.manifest"),
                container=parent / "publication", store=ROOT / "store/sha256",
                target=ROOT / "store/sha256" / selection["fixture"]["artifact"], store_config=ROOT / "registry" / (prefix+"fixture-store.conf"),
                activation_config=ROOT / "registry" / (prefix+"fixture-activation.conf"))


def fixture_payload(record, paths,profile="original"):
    selection=fixture_selection(record,profile)
    candidates = [paths["staging"]]
    if "fixture_publication" in selection:
        candidates += [paths["container"] / "payload", paths["target"] / "payload"]
    existing = [path for path in candidates if path.exists() or path.is_symlink()]
    if len(existing) != 1 or existing[0].is_symlink():
        raise RuntimeError("ambiguous/missing fixture payload location")
    return existing[0]


def fixture_config(record, roots,profile="original"):
    selection=fixture_selection(record,profile)
    fixture = selection["fixture"]
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


def verify_fixture(record, allow_unfinished=False,profile="original"):
    selection=fixture_selection(record,profile)
    if "fixture" not in selection:
        return
    state = selection["fixture_state"]
    if state not in ("prepared", "stage-ready") and not (allow_unfinished and state in ("observing", "sealing")):
        raise RuntimeError("unfinished fixture provisioning needs explicit reconciliation")
    paths, fixture = fixture_paths(record,profile), selection["fixture"]
    payload = fixture_payload(record, paths,profile)
    for path, mode in ((ROOT / "fixtures", 0o700), (paths["parent"], 0o700), (paths["source"], 0o555), (payload, None)):
        info = path.lstat()
        modes = (0o700, 0o555) if mode is None and "fixture_publication" in selection else (0o700,) if mode is None else (mode,)
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
    if "fixture_publication" in selection:
        publication = selection["fixture_publication"]
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
    if "fixture_activation" in selection:
        activation = selection["fixture_activation"]
        if activation["state"] not in ("sealing", "prepared", "executing", "applied"): raise RuntimeError("unrecognized activation provisioning state")
        if paths["activation_config"].exists() or paths["activation_config"].is_symlink():
            owned_file(paths["activation_config"], 0o444)
            if checksum(paths["activation_config"]) != activation["config_sha256"]: raise RuntimeError("activation configuration changed")
        elif activation["state"] != "sealing": raise RuntimeError("activation configuration missing")


def fixture_prepare(record,profile="original"):
    selection=fixture_selection(record,profile)
    if profile=="exec" and "exec_fixture" not in record:
        verified(record); stop(record)
        if "fixture_publication" not in record: raise RuntimeError("original shared store is not enrolled")
        record["exec_fixture"]={}; selection=record["exec_fixture"]
    prefix="exec-" if profile=="exec" else ""
    if "fixture" not in selection:
        verified(record)
        stop(record)
        for path in ([ROOT / "fixtures"] if profile=="original" else []) + [ROOT / "registry" / (prefix+name) for name in ("fixture.conf","fixture.manifest","fixture.next")]:
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
        binary = (BUILD / ("exec_payload" if profile=="exec" else "probe_service")).read_bytes()
        if not binary.startswith(b"\x7fELF") or not 1 <= len(binary) <= 16 * 1024**2:
            raise RuntimeError("fixture probe is not a bounded native ELF")
        manifest = dict(component="aetheri.p0.exec" if profile=="exec" else "aetheri.p0.probe", publisher=fixture["publisher"], version="p0-exec" if profile=="exec" else "p0-native", target="x86_64-linux-gnu",
                        files=[dict(path="bin/probe", mode=0o555, size=len(binary), sha256=hashlib.sha256(binary).hexdigest()),
                               dict(path="share/message", mode=0o444, size=6, sha256=hashlib.sha256(b"hello\n").hexdigest())],
                        actions=[dict(id="run", executable="bin/probe", arguments=[] if profile=="exec" else ["check", record["owner"]])], dependencies=[])
        encoded = runtime_contracts.fixture(manifest)
        fixture.update(manifest=manifest, artifact=hashlib.sha256(encoded).hexdigest())
        intent = dict(plan=fixture["plan"], authority=record["epochs"]["system"], actor=fixture["actor"], scope=fixture["scope"],
                      kind="install", component=manifest["component"], install=None, generation=None, data=None,
                      artifact=fixture["artifact"], grant=None, retention="none", action=None, boot=current_boot, expires=fixture["expires"],
                      reads=[dict(epoch=record["epochs"]["system"], kind="scope", ref=fixture["scope"], revision=scope[0])],
                      effects=[dict(number=n, kind=kind, component=manifest["component"], ref=fixture[key], disposition="create", reason="fixture_prepare")
                               for n, (kind, key) in enumerate((("install", "install"), ("generation", "generation"), ("storage", "slot")))])
        fixture["intent_digest"] = hashlib.sha256(runtime_contracts.intent(intent)).hexdigest()
        selection.update(fixture=fixture, fixture_state="preparing")
        save(record)  # Administrative carrier-infrastructure intent before effects.
        paths = fixture_paths(record,profile)
        if profile=="original": directory(ROOT / "fixtures", "aetheri_root_t", mode=0o700)
        directory(paths["parent"], "aetheri_stage_parent_t", mode=0o700)
        directory(paths["source"], "aetheri_fixture_source_t", mode=0o700)
        for name in ("bin", "share"): directory(paths["source"] / name, "aetheri_fixture_source_t", mode=0o700)
        publish(paths["source"] / "bin/probe", binary, "aetheri_fixture_source_t", mode=0o555)
        publish(paths["source"] / "share/message", b"hello\n", "aetheri_fixture_source_t", mode=0o444)
        for name in ("bin", "share"): (paths["source"] / name).chmod(0o555)
        paths["source"].chmod(0o555)
        directory(paths["staging"], "aetheri_staging_t", mode=0o700)
        publish(paths["manifest"], encoded, "aetheri_registry_t")
        publish(paths["config"], fixture_config(record, bytes(72),profile), "aetheri_registry_t")
        for path in (paths["config"], paths["manifest"], paths["source"] / "bin/probe", paths["source"] / "share/message"):
            flush_path(path)
        for path in (paths["source"] / "bin", paths["source"] / "share", paths["source"], paths["staging"], paths["parent"], ROOT / "fixtures", ROOT / "registry", ROOT):
            flush_path(path, True)
        fixture["objects"] = {key: [paths[key].stat().st_ino, paths[key].stat().st_ctime_ns] for key in ("source", "staging")}
        fixture["config_sha256"] = checksum(paths["config"])
        selection["fixture_state"] = "observing"
        save(record)
    verified(record, allow_unfinished_fixture=True)
    stop(record)
    fixture, paths = selection["fixture"], fixture_paths(record,profile)
    if selection["fixture_state"] == "observing":
        unit = start_unit(record, "system", fixture="observe",fixture_profile=profile)
        output = run("journalctl", "-u", unit, "--output=cat", "--no-pager")
        values = re.findall(r"^fixture-roots ([0-9a-f]{144})$", output, re.M)
        if len(values) != 1: raise RuntimeError("native root observation missing/ambiguous")
        roots = bytes.fromhex(values[0])
        for offset, key in ((0, "source"), (36, "staging")):
            if struct.unpack_from(">Q", roots, offset + 16)[0] != fixture["objects"][key][0]:
                raise RuntimeError("native observation differs from independently provisioned inode")
        fixture["roots"] = values[0]
        fixture["sealed_sha256"] = hashlib.sha256(fixture_config(record, roots,profile)).hexdigest()
        selection["fixture_state"] = "sealing"
        save(record)
    if selection["fixture_state"] == "sealing":
        data = fixture_config(record, bytes.fromhex(fixture["roots"]),profile)
        temporary = ROOT / "registry" / (prefix+"fixture.next")
        if checksum(paths["config"]) != fixture["sealed_sha256"]:
            if not temporary.exists(): publish(temporary, data, "aetheri_registry_t")
            owned_file(temporary, 0o444)
            if temporary.read_bytes() != data: raise RuntimeError("uncertain fixture seal needs reconciliation")
            flush_path(temporary)
            os.replace(temporary, paths["config"])
        flush_path(paths["config"])
        flush_path(ROOT / "registry", True)
        fixture["config_sha256"] = fixture["sealed_sha256"]
        selection["fixture_state"] = "prepared"
        save(record)
    verify_fixture(record,profile=profile)
    print("Protected native ELF fixture/source/slot configuration is sealed; this provisioning action accepts no operation.")


def fixture_check(record,profile="original"):
    selection=fixture_selection(record,profile)
    verified(record)
    stop(record)
    fixture, paths = selection["fixture"], fixture_paths(record,profile)
    def journal():
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            return db.execute("SELECT o.state,o.phase,o.outcome,o.revision,s.state,lower(hex(s.evidence_digest)),a.store_state,i.state,g.publication_state FROM operations o JOIN operation_steps s USING(operation_id) JOIN artifacts a ON a.artifact_id=? JOIN installs i ON i.install_id=? JOIN generations g ON g.generation_id=? WHERE o.operation_id=?",
                              tuple(bytes.fromhex(fixture[key]) for key in ("artifact", "install", "generation", "operation"))).fetchone()
    start_unit(record, "system", fixture="stage",fixture_profile=profile)
    first = journal()
    expected = ("running", "publish", None, first[3] if first else None, "applied", fixture["config_sha256"], "staged", "preparing", "staged")
    if first != expected or first[3] < 3: raise AssertionError(("fixture journal did not retain pending publication", first))
    snapshot = {}
    for file in fixture["manifest"]["files"]:
        path = paths["staging"] / file["path"]
        owned_file(path, file["mode"])
        if checksum(path) != file["sha256"]: raise AssertionError("native staged content differs")
        snapshot[file["path"]] = path.stat().st_ino
    start_unit(record, "system", fixture="stage",fixture_profile=profile)
    if journal() != first or any((paths["staging"] / name).stat().st_ino != inode for name, inode in snapshot.items()):
        raise AssertionError("replay rewrote a staged object/journal")
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    first_boot = selection.setdefault("fixture_first_boot", boot)
    selection.update(fixture_state="stage-ready", fixture_check=dict(native_elf=True, native_journal=True,
                  exact_replay=True, cross_boot_reopen=first_boot != boot, stage_nonexecutable=True,
                  operation_completed=False, artifact_published=False))
    save(record)
    print(json.dumps(selection["fixture_check"]))


def fixture_store_prepare(record,profile="original"):
    selection=fixture_selection(record,profile)
    verified(record)
    stop(record)
    if profile=="exec" and "fixture_publication" not in record: raise RuntimeError("original shared store is not enrolled")
    paths = fixture_paths(record,profile)
    if "fixture_publication" not in selection:
        if selection["fixture_state"] != "stage-ready": raise RuntimeError("stage verification is required first")
        for path in ([ROOT / "store"] if profile=="original" else []) + [paths["container"],paths["store_config"]]:
            if path.exists() or path.is_symlink(): raise RuntimeError("unowned publication infrastructure exists")
        selection["fixture_publication"] = dict(state="preparing")
        save(record)  # Root-provisioned carrier infrastructure, not dynamic allocation.
        if profile=="original":
            directory(ROOT / "store", "aetheri_root_t", mode=0o700)
            directory(paths["store"], "aetheri_store_parent_t", mode=0o700)
        directory(paths["container"], "aetheri_publication_t", mode=0o700)
        for path in (paths["container"], paths["parent"], paths["store"], ROOT / "store", ROOT): flush_path(path, True)
        selection["fixture_publication"].update(state="observing", objects={key: paths[key].stat().st_ino for key in ("store", "container")})
        save(record)
    publication = selection["fixture_publication"]
    if publication["state"] == "observing":
        unit = start_unit(record, "system", fixture="store-observe",fixture_profile=profile)
        output = run("journalctl", "-u", unit, "--output=cat", "--no-pager")
        values = re.findall(r"^fixture-store-roots ([0-9a-f]{144})$", output, re.M)
        if len(values) != 1: raise RuntimeError("publication object observation missing/ambiguous")
        roots = bytes.fromhex(values[0])
        for offset, key in ((0, "store"), (36, "container")):
            if struct.unpack_from(">Q", roots, offset + 16)[0] != publication["objects"][key]: raise RuntimeError("publication observation differs from provisioned inode")
        data = b"AETHSTO1" + bytes.fromhex(selection["fixture"]["config_sha256"]) + roots
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


def fixture_store_check(record,profile="original"):
    selection=fixture_selection(record,profile)
    verified(record)
    stop(record)
    fixture, publication = selection["fixture"], selection["fixture_publication"]
    if publication["state"] not in ("prepared", "executing", "published"): raise RuntimeError("publication infrastructure not sealed")
    publication["state"] = "executing" if publication["state"] != "published" else "published"
    save(record)  # Permit known alternate locations after an interrupted native dispatch.
    def journal():
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            return db.execute("SELECT o.state,o.phase,o.outcome,o.revision,s.state,lower(hex(s.evidence_digest)),a.store_state,i.state,g.publication_state,g.activated_once,i.active_generation_id,o.reason_code FROM operations o JOIN operation_steps s USING(operation_id) JOIN artifacts a ON a.artifact_id=? JOIN installs i ON i.install_id=? JOIN generations g ON g.generation_id=? WHERE o.operation_id=? AND s.step_no=1",
                              tuple(bytes.fromhex(fixture[key]) for key in ("artifact", "install", "generation", "operation"))).fetchone()
    start_unit(record, "system", fixture="store-publish",fixture_profile=profile)
    first = journal()
    activated = first is not None and first[0] == "completed"
    expected = ("completed" if activated else "running", "reconcile", "applied" if activated else None, first[3] if first else None,
                "applied", publication["config_sha256"], "verified", "active" if activated else "preparing", "committed",
                1 if activated else 0, bytes.fromhex(fixture["generation"]) if activated else None, "" if activated else "activation_pending")
    if first != expected: raise AssertionError(("publication journal differs", first))
    if activated:
        activation = selection["fixture_activation"]
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            receipt = db.execute("SELECT lower(hex(evidence_digest)) FROM operation_steps WHERE operation_id=? AND step_no=2 AND action='activate' AND subject_kind='generation' AND subject_ref=? AND state='applied'",
                                 (bytes.fromhex(fixture["operation"]), bytes.fromhex(fixture["generation"]))).fetchall()
        if receipt != [(activation["config_sha256"],)]: raise AssertionError("active publication has no matching activation receipt")
    paths = fixture_paths(record,profile)
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
    start_unit(record, "system", fixture="store-publish",fixture_profile=profile)
    if journal() != first or snapshot != {p.relative_to(container).as_posix(): (p.stat().st_ino, p.stat().st_mode, p.stat().st_ctime_ns) for p in [container, *container.rglob("*")]}: raise AssertionError("publication retry rewrote objects/metadata")
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    first_boot = publication.setdefault("first_boot", boot)
    publication.update(state="published", check=dict(native_journal=True, original_root_inodes=True, sealed_types=True,
                       exact_replay=True, cross_boot_reopen=first_boot != boot, store_setattr_denied=True, activated=activated, operation_completed=activated))
    save(record)
    verified(record)
    print(json.dumps(publication["check"]))


def fixture_activation_prepare(record,profile="original"):
    selection=fixture_selection(record,profile)
    verified(record)
    stop(record)
    paths = fixture_paths(record,profile)
    if "fixture_activation" not in selection:
        if selection["fixture_publication"]["state"] != "published": raise RuntimeError("verified publication is required first")
        if paths["activation_config"].exists() or paths["activation_config"].is_symlink(): raise RuntimeError("unowned activation configuration exists")
        # Reconcile the actual boot/runtime and close bootstrap-open gates through
        # native normal startup, not Python business-row writes. No app is run.
        start(record)
        stop(record)
        fixture = selection["fixture"]
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            revisions = db.execute("SELECT i.revision,g.revision,f.revision,f.state,f.closing_operation_id,o.state,o.phase FROM installs i JOIN generations g ON g.generation_id=? JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=i.install_id JOIN operations o ON o.operation_id=? WHERE i.install_id=? AND i.state='preparing' AND i.active_generation_id IS NULL AND g.activated_once=0 AND g.publication_state='committed'",
                                   tuple(bytes.fromhex(fixture[key]) for key in ("generation", "operation", "install"))).fetchone()
        if revisions is None or revisions[3:] != ("closed", None, "running", "reconcile"): raise RuntimeError("installation cannot accept first activation")
        stamp = time.time_ns() // 10**6
        data = b"AETHACT1" + bytes.fromhex(selection["fixture_publication"]["config_sha256"]) + struct.pack(">QQQQ", *revisions[:3], stamp)
        selection["fixture_activation"] = dict(state="sealing", config=data.hex(), config_sha256=hashlib.sha256(data).hexdigest(),
                                             stamp=stamp, revisions=list(revisions[:3]))
        save(record)  # Immutable administrative evidence before config publication.
    activation = selection["fixture_activation"]
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


def fixture_activation_check(record,profile="original"):
    selection=fixture_selection(record,profile)
    verified(record)
    stop(record)
    fixture, activation = selection["fixture"], selection["fixture_activation"]
    if activation["state"] not in ("prepared", "executing", "applied"): raise RuntimeError("activation evidence is not sealed")
    activation["state"] = "executing" if activation["state"] != "applied" else "applied"
    save(record)
    paths = fixture_paths(record,profile)
    container = paths["target"]
    def snapshot():
        return {p.relative_to(container).as_posix(): (p.stat().st_ino,p.stat().st_mode,p.stat().st_ctime_ns) for p in [container,*container.rglob("*")]}
    before = snapshot()
    def journal():
        with sqlite3.connect(f"file:{state_path(record, 'system') / 'catalogue.sqlite3'}?mode=ro", uri=True) as db:
            receipt = db.execute("SELECT o.state,o.outcome,o.result_kind,lower(hex(o.result_ref)),o.completed_unix_ms,o.revision,s.state,lower(hex(s.evidence_digest)),i.state,lower(hex(i.active_generation_id)),i.revision,g.activated_once,g.revision,f.state,f.revision,f.closing_operation_id FROM operations o JOIN operation_steps s USING(operation_id) JOIN installs i ON i.install_id=? JOIN generations g ON g.generation_id=? JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=i.install_id WHERE o.operation_id=? AND s.step_no=2",
                                 tuple(bytes.fromhex(fixture[key]) for key in ("install", "generation", "operation"))).fetchone()
            live = db.execute("SELECT (SELECT count(*) FROM generation_leases WHERE state<>'released')+(SELECT count(*) FROM launch_dispatches WHERE state<>'terminal')").fetchone()[0]
        if live: raise AssertionError("activation created/admitted launch obligations")
        return receipt
    with sqlite3.connect(f"file:{state_path(record,'system')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        previous_gate=db.execute("SELECT state,revision,closing_operation_id FROM admission_gates WHERE subject_kind='install' AND subject_ref=?",(bytes.fromhex(fixture["install"]),)).fetchone()
    if previous_gate[0]!="closed" or previous_gate[2] is not None or previous_gate[1]<activation["revisions"][2]: raise RuntimeError("activation receipt check requires current closed admission")
    start_unit(record, "system", fixture="activate",fixture_profile=profile)
    first = journal()
    install_revision, generation_revision, gate_revision = activation["revisions"]
    expected = ("completed", "applied", "install", fixture["install"], activation["stamp"], first[5] if first else None,
                "applied", activation["config_sha256"], "active", fixture["generation"], install_revision+1,
                1, generation_revision+1, "closed", previous_gate[1], None)
    if first != expected or snapshot() != before: raise AssertionError(("activation metadata/content differs", first))
    database_hash = checksum(state_path(record, "system") / "catalogue.sqlite3")
    start_unit(record, "system", fixture="activate",fixture_profile=profile)
    if journal() != first or snapshot() != before or checksum(state_path(record, "system") / "catalogue.sqlite3") != database_hash: raise AssertionError("completed activation retry rewrote the receipt/object")
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    first_boot = activation.setdefault("first_boot", boot)
    activation.update(state="applied", check=dict(native_transaction=True, exact_completed_replay=True, publication_unchanged=True,
                       gate_closed=True, cross_boot_reopen=first_boot != boot, app_dispatched=False))
    selection["fixture_publication"]["check"].update(activated=True, operation_completed=True)
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


def start_unit(record, role, init=False, probe=None, claims=None, enroll=False, storage=None, fixture=None, fixture_source_override=None,fixture_profile="original", data=None, admit=False, reservation_probe=False, launch_probe=None, recover_setup=False,data_profile="original"):
    fixture_selection(record,data_profile)
    if data_profile!="original" and data is None: raise ValueError("execution data profile requires a data administrative action")
    fixture_selection(record,fixture_profile)
    if fixture_profile!="original" and fixture is None: raise ValueError("execution profile requires a fixture administrative action")
    if recover_setup and (role!="system" or init or probe or enroll or storage or fixture or data or admit or reservation_probe or launch_probe):
        raise ValueError("setup recovery requires only the offline system administrative role")
    if launch_probe is not None and (launch_probe not in ("prepare","cancel","setup-prepare","setup-cancel","setup-busy","exec-refresh") or role!="user" or probe!="projection" or reservation_probe):
        raise ValueError("launch probe requires the selected fixed user controller")
    if reservation_probe and (role!="user" or probe!="projection"): raise ValueError("reservation probe requires selected user controller")
    if admit and (role!="system" or init or probe or enroll or storage or fixture or data):
        raise ValueError("fixture admission requires only the normal system service")
    if data is not None and (data not in ("observe","enroll") or role!="user" or init or probe or enroll or storage or fixture):
        raise ValueError("data administration requires only the user administrative role")
    if probe=="projection" and (role!="user" or init or enroll or storage or fixture):
        raise ValueError("projection controller requires only the user probe role")
    if fixture is not None and (fixture not in ("observe", "stage", "store-observe", "store-publish", "activate") or role != "system" or init or probe or enroll or storage):
        raise ValueError("fixture administration requires only the system role")
    if fixture_source_override is not None and (fixture != "stage" or fixture_source_override != fixture_paths(record,fixture_profile)["parent"] / "copied-source"):
        raise ValueError("only the owned fixed fixture substitution probe may bind a source")
    if enroll and (role != "system" or init or probe):
        raise ValueError("account enrollment requires only the system administrative role")
    if storage is not None and (storage not in ("publish", "verify") or role != "user" or init or probe or enroll):
        raise ValueError("storage administration requires only the user role")
    suffix = ("probe-" + probe) if probe else role + ("-recover-setup" if recover_setup else "-init" if init else "-enroll" if enroll else "-fixture-" + ("exec-" if fixture_profile=="exec" else "") + fixture if fixture else "-storage-" + storage if storage else "-data-"+("exec-" if data_profile=="exec" else "")+data if data else "")
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
            if reservation_probe: command.append("--reserve")
            if launch_probe: command.append("--"+launch_probe if launch_probe=="exec-refresh" else "--"+launch_probe if launch_probe.startswith("setup-") else "--launch-"+launch_probe)
    else:
        properties.update(SELinuxContext="system_u:system_r:aetheri_" + role + "_t:s0", ReadWritePaths=str(state_path(record, role)))
        command = [str(ROOT / (role + "-authority")), role] + (["--init"] if init else ["--enroll-account"] if enroll else [("--exec-fixture-" if fixture_profile=="exec" else "--fixture-") + fixture] if fixture else ["--storage-" + storage] if storage else [("--exec-data-" if data_profile=="exec" else "--data-")+data] if data else ["--admit-fixture"] if admit else [])
        if recover_setup: command=[str(ROOT/"setup-recovery"),"system","--recover-setup"]
        if fixture in ("store-observe", "store-publish", "activate"):
            properties["ReadWritePaths"] = str(ROOT)
            properties["ReadOnlyPaths"] = str(fixture_paths(record,fixture_profile)["source"])
        elif fixture:
            properties["ReadWritePaths"] += " " + str(fixture_paths(record,fixture_profile)["staging"])
        if fixture_source_override is not None:
            properties["BindReadOnlyPaths"] = str(fixture_source_override) + ":" + str(fixture_paths(record,fixture_profile)["source"])
    if role == "user" or probe:
        properties.update(BindsTo="user@1000.service", After="user@1000.service",
                          BindReadOnlyPaths="/run/user/1000/bus:" + str(ROOT / "user-bus"))
    arguments = ["systemd-run", "--unit=" + unit, "--service-type=" + ("exec" if init or probe or enroll or storage or fixture or data or recover_setup else "notify")]
    if init or probe or enroll or storage or fixture or data or recover_setup:
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
        paths = ["/org/aetheri/Management1", "/org/aetheri/Runtime1", "/org/aetheri/Broker1", "/org/aetheri/Catalogue1","/org/aetheri/Setup1"] if role == "system" else ["/org/aetheri/Management1", "/org/aetheri/Documents1", "/org/aetheri/Development1"]
        name = "org.aetheri.System1" if role == "system" else "org.aetheri.User1"
        for path in paths:
            live = ET.fromstring(json.loads(bus(role, "--json=short", "call", name, path, "org.freedesktop.DBus.Introspectable", "Introspect"))["data"][0])
            expected_file = {"Management1": "management-v1.xml", "Runtime1": "runtime-v1.xml", "Documents1": "documents-v1.xml", "Broker1": "broker-v1.xml", "Catalogue1": "catalogue-v1.xml", "Development1": "development-v1.xml","Setup1":"setup-v1.xml"}[path.rsplit("/", 1)[1]]
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


def projection_check(record,profile="original"):
    selection=fixture_selection(record,profile)
    verified(record)
    start(record)
    fixture=selection["fixture"]
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
        unit=start_unit(record,"user",probe="projection",launch_probe="exec-refresh" if profile=="exec" else None)
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
        user_run("busctl","--user","call","org.aetheri.User1","/org/aetheri/Development1","org.aetheri.Development1","RefreshExecFixtureInstall" if profile=="exec" else "RefreshFixtureInstall")
    except subprocess.CalledProcessError as error:
        if "Protected P0 controller unit required" not in error.stderr: raise
    else: raise AssertionError("same-UID process invoked the protected development trigger")
    if checksum(user_database)!=first_hash or checksum(system_database)!=system_hash: raise AssertionError("rejected projection caller changed a catalogue")
    boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    initial=selection.setdefault("projection_first_context",dict(boot=boot,system_runtime=system_runtime))
    selection["projection_check"]=dict(native_rpc=True,separate_owner_writer=True,exact_retry=True,protected_controller=True,
        no_data_or_gate_effects=True,cross_boot_reopen=initial["boot"]!=boot,system_runtime_changed=initial["system_runtime"]!=system_runtime)
    save(record)
    print(json.dumps(selection["projection_check"]))


def verify_reservation_intent(database,operation):
    with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
        db.row_factory=sqlite3.Row
        row=db.execute("SELECT p.*,o.intent_digest AS accepted_digest FROM plans p JOIN operations o USING(plan_id) WHERE o.operation_id=?",(bytes.fromhex(operation),)).fetchone()
        if row is None: raise AssertionError("reservation plan missing")
        plan={key:row[column].hex() if row[column] is not None else None for key,column in
            (("plan","plan_id"),("authority","authority_epoch"),("actor","actor_ref"),("scope","scope_ref"),("install","install_ref"),
             ("generation","generation_ref"),("data","data_epoch_ref"),("artifact","artifact_ref"),("grant","grant_ref"),("boot","boot_id"))}
        plan.update(kind=row["kind"],component=row["component_id"],retention=row["retention"],action=row["action_id"],expires=row["expires_boottime_ns"])
        plan["reads"]=[dict(epoch=r[0].hex(),kind=r[1],ref=r[2].hex(),revision=r[3]) for r in db.execute("SELECT source_epoch,subject_kind,subject_ref,expected_revision FROM plan_reads WHERE plan_id=?",(row["plan_id"],))]
        plan["effects"]=[dict(number=r[0],kind=r[1],component=r[2],ref=r[3].hex(),disposition=r[4],reason=r[5]) for r in db.execute("SELECT effect_no,subject_kind,component_id,subject_ref,disposition,reason_code FROM plan_effects WHERE plan_id=?",(row["plan_id"],))]
        digest=hashlib.sha256(runtime_contracts.intent(plan)).digest()
        if digest!=row["intent_digest"] or digest!=row["accepted_digest"]: raise AssertionError("canonical native reservation plan differs")


def reservation_check(record,resume=False,fresh=False):
    verified(record)
    stop(record)
    if "reservation_check" in record:
        if not fresh: raise RuntimeError("reservation already exercised; preserve its receipt for replay checks")
        with sqlite3.connect(f"file:{state_path(record,'system')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
            old=record["reservation_intent"]
            observed=db.execute("SELECT d.state,l.state,lower(hex(d.close_cause_operation_ref)) FROM launch_dispatches d JOIN generation_leases l USING(lease_id) WHERE d.lease_id=? AND d.launch_id=?",tuple(bytes.fromhex(old[key]) for key in ("lease","launch"))).fetchone()
            if observed!=("terminal","released",old["cause"]): raise RuntimeError("previous reservation is not terminal/released")
        record.setdefault("reservation_history",[]).append(dict(intent=old,operation=record["reservation_operation"],check=record["reservation_check"]))
        record.pop("reservation_check")
    paths={name:ROOT/"registry"/(name+".conf") for name in ("admission","reservation")}
    if not resume and not fresh and any(path.exists() or path.is_symlink() for path in paths.values()): raise RuntimeError("unregistered reservation configuration exists")
    if resume and record.get("reservation_state")!="preparing": raise RuntimeError("no prepared reservation to resume")
    fixture=record["fixture"]
    database=state_path(record,"system")/"catalogue.sqlite3"
    with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
        baseline=db.execute("SELECT catalogue_revision FROM authority_meta").fetchone()[0]
        ir=db.execute("SELECT revision FROM installs WHERE install_id=? AND state='active'",(bytes.fromhex(fixture["install"]),)).fetchone()[0]
        gate=db.execute("SELECT state,revision,closing_operation_id FROM admission_gates WHERE subject_kind='install' AND subject_ref=?",(bytes.fromhex(fixture["install"]),)).fetchone()
        if gate[0] not in ("open","closed") or gate[2] is not None or db.execute("SELECT count(*) FROM generation_leases WHERE state<>'released'").fetchone()[0]: raise RuntimeError("unreconciled admission state")
        if resume and db.execute("SELECT count(*) FROM launch_dispatches WHERE lease_id=? OR launch_id=?",tuple(bytes.fromhex(record["reservation_intent"][key]) for key in ("lease","launch"))).fetchone()[0]: raise RuntimeError("accepted reservation requires reconciliation; no automatic repeat")
    boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    admission=(b"AETHADM1"+bytes.fromhex(record["epochs"]["system"])+uuid.UUID(boot).bytes+struct.pack(">Q",baseline)
        +bytes.fromhex(fixture["install"])+bytes.fromhex(fixture["generation"])+struct.pack(">QQ",ir,gate[1]+(gate[0]=="open")))
    ids=record["reservation_intent"] if resume else {key:uuid.uuid4().hex for key in ("launch","lease","cause")}
    reservation=b"AETHRSF1"+b"".join(bytes.fromhex(ids[key]) for key in ("launch","lease","cause"))
    record.update(reservation_intent=ids,reservation_state="preparing")
    save(record)
    for name,content in (("admission",admission),("reservation",reservation)):
        if resume and name=="reservation":
            if paths[name].read_bytes()!=content: raise RuntimeError("prepared reservation identity changed")
        elif resume or fresh:
            temporary=ROOT/"registry"/(name+".next")
            publish(temporary,content,"aetheri_registry_t"); flush_path(temporary); os.replace(temporary,paths[name])
        else: publish(paths[name],content,"aetheri_registry_t")
        flush_path(paths[name])
        record[name+"_config_sha256"]=checksum(paths[name])
    flush_path(ROOT/"registry",True); save(record)
    unit=start_unit(record,"system",admit=True)
    record.setdefault("active_units",{})["system"]=unit; save(record)
    start(record)
    user_hash=checksum(state_path(record,"user")/"catalogue.sqlite3")
    unit=start_unit(record,"user",probe="projection",reservation_probe=True)
    output=run("journalctl","-u",unit,"--output=cat","--no-pager")
    replies=re.findall(r"^reservation ([0-9a-f]{32}) ([0-9]+) ([0-9]+)$",output,re.M)
    if len(replies)!=1 or int(replies[0][2])!=2: raise AssertionError("reservation receipt missing")
    stop_unit(unit); publish_projection_slot(record,"-"); save(record)
    with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
        actual=db.execute("SELECT lower(hex(root_operation_id)),state,revision,install_gate_revision,invocation_ref,ready_data_gate_revision,lower(hex(close_cause_epoch)),lower(hex(close_cause_operation_ref)) FROM launch_dispatches WHERE lease_id=? AND launch_id=?",tuple(bytes.fromhex(ids[key]) for key in ("lease","launch"))).fetchone()
        if actual!=(replies[0][0],"terminal",2,int(replies[0][1]),None,None,record["epochs"]["user"],ids["cause"]): raise AssertionError("dispatch withdrawal differs")
        if db.execute("SELECT state,revision FROM generation_leases WHERE lease_id=?",(bytes.fromhex(ids["lease"]),)).fetchone()!=("released",2): raise AssertionError("code lease not released")
        if db.execute("SELECT state,outcome,result_kind,lower(hex(result_ref)) FROM operations WHERE operation_id=?",(bytes.fromhex(replies[0][0]),)).fetchone()!=("completed","cancelled","launch",ids["launch"]): raise AssertionError("root operation not cancelled")
    if checksum(state_path(record,"user")/"catalogue.sqlite3")!=user_hash: raise AssertionError("root reservation changed user metadata")
    verify_reservation_intent(database,replies[0][0])
    if run("systemctl","show","aetheri-app-"+ids["launch"]+".service","--property=LoadState","--value")!="not-found": raise AssertionError("reservation created an app unit")
    record["reservation_check"]=dict(native_rpc=True,canonical_intent=True,exact_retry=True,terminal_withdrawal=True,code_lease_released=True,user_data_unchanged=True,app_dispatched=False)
    record.update(reservation_state="withdrawn",reservation_operation=replies[0][0],reservation_first_boot=boot)
    save(record)
    print(json.dumps(record["reservation_check"]))


def launch_probe(record,action):
    unit=start_unit(record,"user",probe="projection",launch_probe=action)
    output=run("journalctl","-u",unit,"--output=cat","--no-pager")
    replies=re.findall(r"^launch ([0-9a-f]{32}) ([0-9]+) ([0-9]+)$",output,re.M)
    stop_unit(unit); publish_projection_slot(record,"-"); save(record)
    if action=="setup-busy":
        if not re.search(r"^setup-busy$",output,re.M): raise AssertionError("live setup was not protected")
        return None
    if len(replies)!=1 or replies[0][0]!=record["launch_intent"]["operation"]: raise AssertionError("local launch receipt missing")
    return tuple(int(value) for value in replies[0][1:])


def launch_business(record):
    result={}
    for role,tables in (("system",("operations","operation_steps","generation_leases","launch_dispatches")),
                        ("user",("plans","plan_reads","plan_effects","operations","operation_steps","operation_links","launches"))):
        with sqlite3.connect(f"file:{state_path(record,role)/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
            result[role]={name:db.execute('SELECT * FROM "'+name+'" ORDER BY rowid').fetchall() for name in tables}
    return result


def verify_launch_terminal(record):
    ids=record["launch_intent"]
    with sqlite3.connect(f"file:{state_path(record,'system')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        dispatch=db.execute("SELECT state,revision,invocation_ref,ready_data_gate_revision,lower(hex(close_cause_epoch)),lower(hex(close_cause_operation_ref)),lower(hex(root_operation_id)) FROM launch_dispatches WHERE launch_id=? AND lease_id=?",
            tuple(bytes.fromhex(ids[k]) for k in ("launch","lease"))).fetchone()
        lease=db.execute("SELECT state,revision FROM generation_leases WHERE lease_id=?",(bytes.fromhex(ids["lease"]),)).fetchone()
    if dispatch is None or dispatch[:6]!=("terminal",2,None,None,record["epochs"]["user"],ids["operation"]) or lease!=("released",2):
        raise AssertionError("root withdrawal is not proven")
    with sqlite3.connect(f"file:{state_path(record,'user')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        local=db.execute("SELECT l.state,l.readiness,l.invocation_ref,o.state,o.outcome,o.result_kind,lower(hex(o.result_ref)),x.state,lower(hex(x.remote_operation_ref)) FROM launches l JOIN operations o USING(operation_id) JOIN operation_links x USING(operation_id) WHERE l.operation_id=? AND x.link_no=0",(bytes.fromhex(ids["operation"]),)).fetchone()
        gate=db.execute("SELECT state,closing_operation_id FROM admission_gates WHERE subject_kind='data' AND subject_ref=?",(bytes.fromhex(record["app_data"]["owner"]["data"]),)).fetchone()
    if local!=("start_failed","unavailable",None,"completed","cancelled","launch",ids["launch"],"completed",dispatch[6]) or gate!=("closed",None):
        raise AssertionError("user journal not reconciled or data gate reopened")
    verify_reservation_intent(state_path(record,"system")/"catalogue.sqlite3",dispatch[6])
    verify_reservation_intent(state_path(record,"user")/"catalogue.sqlite3",ids["operation"])
    unit="aetheri-app-"+ids["launch"]+".service"
    if run("systemctl","show",unit,"--property=LoadState","--value")!="not-found":
        if record.get("setup",{}).get("launch")!=ids["launch"]: raise AssertionError("journal-only test dispatched an app")
        facts=dict(line.split("=",1) for line in run("systemctl","show",unit,"--property=ActiveState,MainPID,ControlGroup").splitlines())
        if facts.get("ActiveState") not in ("inactive","failed") or facts.get("MainPID")!="0" or facts.get("ControlGroup") or unit_has_job(unit):
            raise AssertionError("known setup unit is not independently drained")


def launch_check(record,fresh=False,prepare_only=False):
    verified(record); stop(record)
    previous=None
    if "launch_intent" in record:
        if not fresh or record.get("launch_state")!="withdrawn": raise RuntimeError("a launch attempt already exists; inspect/reconcile it without replacing IDs")
        verify_launch_terminal(record)
        previous={key:record[key] for key in ("launch_intent","launch_state","launch_first_boot","launch_config_sha256","launch_check","launch_reopen_check") if key in record}
    path=ROOT/"registry/launch.conf"
    if (path.exists() or path.is_symlink()) and previous is None: raise RuntimeError("unregistered launch configuration exists")
    fixture=record["fixture"]; database=state_path(record,"system")/"catalogue.sqlite3"
    with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
        baseline=db.execute("SELECT catalogue_revision FROM authority_meta").fetchone()[0]
        ir=db.execute("SELECT revision FROM installs WHERE install_id=? AND state='active'",(bytes.fromhex(fixture["install"]),)).fetchone()[0]
        gate=db.execute("SELECT state,revision,closing_operation_id FROM admission_gates WHERE subject_kind='install' AND subject_ref=?",(bytes.fromhex(fixture["install"]),)).fetchone()
        if gate[0] not in ("open","closed") or gate[2] is not None or db.execute("SELECT count(*) FROM generation_leases WHERE state<>'released'").fetchone()[0]:
            raise RuntimeError("unreconciled root obligations; no new launch")
    with sqlite3.connect(f"file:{state_path(record,'user')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        if db.execute("SELECT count(*) FROM launches WHERE state NOT IN('exited','start_failed')").fetchone()[0]: raise RuntimeError("unreconciled user launches")
    boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    admission=(b"AETHADM1"+bytes.fromhex(record["epochs"]["system"])+uuid.UUID(boot).bytes+struct.pack(">Q",baseline)
        +bytes.fromhex(fixture["install"])+bytes.fromhex(fixture["generation"])+struct.pack(">QQ",ir,gate[1]+(gate[0]=="open")))
    target=ROOT/"registry/admission.conf"; temporary=ROOT/"registry/admission.next"
    if "admission_config_sha256" not in record or checksum(target)!=record["admission_config_sha256"]: raise RuntimeError("unowned admission profile")
    publish(temporary,admission,"aetheri_registry_t"); flush_path(temporary); os.replace(temporary,target); flush_path(target)
    record["admission_config_sha256"]=checksum(target); save(record)
    record.setdefault("active_units",{})["system"]=start_unit(record,"system",admit=True); save(record)
    start(record)
    user_database=state_path(record,"user")/"catalogue.sqlite3"
    with sqlite3.connect(f"file:{user_database}?mode=ro",uri=True) as db:
        dr=db.execute("SELECT revision FROM data_epochs WHERE data_epoch_id=? AND state='active'",(bytes.fromhex(record["app_data"]["owner"]["data"]),)).fetchone()[0]
        data_gate=db.execute("SELECT state,revision,closing_operation_id FROM admission_gates WHERE subject_kind='data'").fetchone()
        old_runtime=db.execute("SELECT runtime_incarnation FROM authority_meta").fetchone()[0]
    if data_gate[0]!="closed" or data_gate[2] is not None: raise RuntimeError("data gate is not initially closed")
    ids={key:uuid.uuid4().hex for key in ("plan","operation","actor","retry","launch","lease")}
    stamp=time.time_ns()//1000000; expires=time.clock_gettime_ns(time.CLOCK_BOOTTIME)+60000000000
    config=b"AETHLNF1"+b"".join(bytes.fromhex(ids[key]) for key in ("plan","operation","actor","retry","launch","lease"))+struct.pack(">QQQQ",dr,data_gate[1],stamp,expires)
    if previous is not None: record.setdefault("launch_history",[]).append(previous)
    record.update(launch_intent=ids,launch_state="preparing",launch_first_boot=boot)
    for key in ("launch_check","launch_reopen_check"): record.pop(key,None)
    save(record)
    temporary=path.with_suffix(".next") if previous is not None else path
    publish(temporary,config,"aetheri_registry_t"); flush_path(temporary)
    if previous is not None: os.replace(temporary,path); flush_path(path)
    flush_path(path.parent,True)
    record["launch_config_sha256"]=checksum(path); save(record)
    if launch_probe(record,"prepare")!=(2,1): raise AssertionError("launch preparation revisions differ")
    with sqlite3.connect(f"file:{user_database}?mode=ro",uri=True) as db:
        row=db.execute("SELECT l.state,l.readiness,s.state,x.state FROM launches l JOIN operation_steps s USING(operation_id) JOIN operation_links x USING(operation_id) WHERE l.operation_id=? AND s.step_no=0 AND x.link_no=0",(bytes.fromhex(ids["operation"]),)).fetchone()
    if row!=("preparing","unknown","applied","accepted"): raise AssertionError("missing durable user/root acceptance")
    record["launch_state"]="reserved"; save(record)
    if prepare_only: return
    stop_unit(record["active_units"]["user"])
    start(record)  # Root stays live; only the authenticated user incarnation changes.
    with sqlite3.connect(f"file:{user_database}?mode=ro",uri=True) as db:
        row=db.execute("SELECT state,readiness FROM launches WHERE operation_id=?",(bytes.fromhex(ids["operation"]),)).fetchone()
        new_runtime=db.execute("SELECT runtime_incarnation FROM authority_meta").fetchone()[0]
    with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
        held=db.execute("SELECT d.state,l.state,d.broker_incarnation FROM launch_dispatches d JOIN generation_leases l USING(lease_id) WHERE d.lease_id=?",(bytes.fromhex(ids["lease"]),)).fetchone()
    if row!=("recovery_required","unknown") or old_runtime==new_runtime or held!=("reserved","reserved",old_runtime):
        raise AssertionError("broker replacement rebound or released the old reservation")
    if launch_probe(record,"cancel")!=(4,2): raise AssertionError("withdrawal revisions differ")
    verify_launch_terminal(record)
    hashes=[checksum(state_path(record,role)/"catalogue.sqlite3") for role in ("system","user")]
    if launch_probe(record,"cancel")!=(4,2) or hashes!=[checksum(state_path(record,role)/"catalogue.sqlite3") for role in ("system","user")]:
        raise AssertionError("terminal retry changed either authority")
    record["launch_check"]=dict(user_journal=True,canonical_intents=True,authenticated_root_receipt=True,broker_replacement=True,
        old_lease_held_until_explicit_withdrawal=True,terminal_retry=True,app_dispatched=False,data_ready=False)
    record["launch_state"]="withdrawn"; save(record); print(json.dumps(record["launch_check"]))


def launch_reopen_check(record):
    verified(record)
    if record.get("launch_state")!="withdrawn": raise RuntimeError("launch not reconciled; inspect preserved obligations")
    before=launch_business(record); start(record); verify_launch_terminal(record)
    if before!=launch_business(record): raise AssertionError("normal restart changed terminal launch history")
    record["launch_reopen_check"]=dict(terminal_history_preserved=True,cross_boot_reopen=Path("/proc/sys/kernel/random/boot_id").read_text().strip()!=record["launch_first_boot"])
    save(record); print(json.dumps(record["launch_reopen_check"]))


def setup_cleanup_layout(record):
    setup=record["setup"]; base=ROOT/"launches"/setup["launch"]
    if setup["state"]!="withdrawn": raise RuntimeError("setup still holds its roots")
    setup["layout_state"]="removing"; save(record)
    for name in ("app","data",""):
        path=base/name
        if not path.exists() and not path.is_symlink(): continue
        info=path.lstat()
        if path.is_symlink() or not stat.S_ISDIR(info.st_mode) or info.st_uid!=0 or stat.S_IMODE(info.st_mode)!=0o700 or info.st_ino!=setup["objects"][name or "root"]:
            raise RuntimeError("setup scaffold changed; preserve it")
        if list(path.iterdir()): raise RuntimeError("unexplained setup scaffold entry; preserve it")
        path.rmdir()  # Only these exact known empty directories, never recursive removal.
    setup["layout_state"]="removed"; save(record)


def setup_reconcile(record):
    verified(record)
    setup=record["setup"]
    if setup["state"]=="starting": raise RuntimeError("unknown setup dispatch; inspect its live supervisor request first")
    stop_unit(setup["unit"])
    if setup["state"]!="withdrawn":
        setup["state"]="drained"; save(record)
        if "user" in record.get("active_units",{}): stop_unit(record["active_units"]["user"])
        start(record)
        revisions=launch_probe(record,"setup-cancel")
        if revisions[1]!=2: raise AssertionError("setup root withdrawal missing")
        setup["state"]="withdrawn"; record["launch_state"]="withdrawn"; save(record)
    verify_launch_terminal(record)
    hashes=[checksum(state_path(record,role)/"catalogue.sqlite3") for role in ("system","user")]
    launch_probe(record,"setup-cancel")
    if hashes!=[checksum(state_path(record,role)/"catalogue.sqlite3") for role in ("system","user")]: raise AssertionError("setup withdrawal retry changed a catalogue")
    setup_cleanup_layout(record)
    checks=setup.get("checks",{})
    record["setup_check"]=dict(supervised_views=checks.get("views",False),kernel_peer_bound=checks.get("peer",False),
        live_withdrawal_denied=checks.get("live_denied",False),broker_replacement=checks.get("replacement",False),
        observed_drain=True,terminal_retry=True,scaffold_removed=True,target_executed=False)
    save(record); print(json.dumps(record["setup_check"]))


def setup_check(record):
    verified(record)
    old=record.get("setup")
    if old is not None:
        if old["state"]!="withdrawn": raise RuntimeError("reconcile the previous setup first; never replace its intent")
        record.setdefault("setup_history",[]).append(dict(old)); save(record)
    path=ROOT/"registry/setup.conf"
    if old is None and (path.exists() or path.is_symlink()): raise RuntimeError("unregistered setup profile exists")
    launch_check(record,fresh="launch_intent" in record,prepare_only=True)
    ids=record["launch_intent"]; fixture=record["fixture"]; data=record["app_data"]
    with sqlite3.connect(f"file:{state_path(record,'system')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        row=db.execute("SELECT d.root_operation_id,d.install_gate_revision,d.broker_incarnation,m.runtime_incarnation,d.boot_id FROM launch_dispatches d,authority_meta m WHERE d.lease_id=?",(bytes.fromhex(ids["lease"]),)).fetchone()
    with sqlite3.connect(f"file:{state_path(record,'user')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        gate=db.execute("SELECT revision FROM admission_gates WHERE subject_kind='data' AND state='open'").fetchone()[0]
    values=[record["epochs"]["system"],row[3].hex(),record["epochs"]["user"],row[2].hex(),row[4].hex(),record["owner"],
        fixture["install"],fixture["generation"],data["owner"]["data"],data["owner"]["storage"],ids["launch"],ids["lease"],row[0].hex(),ids["operation"]]
    content=b"AETHSUP1"+b"".join(bytes.fromhex(v) for v in values)+struct.pack(">QQQ",data["install_revision"],row[1],gate)+bytes.fromhex(fixture["artifact"])+bytes.fromhex(record["setup_sha256"])
    if len(content)!=320: raise AssertionError("setup profile size")
    setup=dict(launch=ids["launch"],lease=ids["lease"],root_operation=row[0].hex(),user_operation=ids["operation"],unit="aetheri-app-"+ids["launch"]+".service",state="prepared")
    record["setup"]=setup; save(record)
    temporary=path.with_suffix(".next") if old is not None else path
    publish(temporary,content,"aetheri_registry_t"); flush_path(temporary)
    if old is not None: os.replace(temporary,path); flush_path(path)
    record["setup_config_sha256"]=checksum(path); flush_path(path.parent,True); save(record)
    launch_probe(record,"setup-prepare")
    parent=ROOT/"launches"
    if "launches_inode" not in record:
        if parent.exists() or parent.is_symlink(): raise RuntimeError("unregistered launch scaffold parent")
        directory(parent,"aetheri_setup_root_t",mode=0o700); record["launches_inode"]=parent.stat().st_ino; save(record)
    if parent.is_symlink() or parent.stat().st_ino!=record["launches_inode"]: raise RuntimeError("launch scaffold parent replaced")
    base=parent/setup["launch"]
    setup["layout_state"]="creating"; save(record)
    directory(base,"aetheri_setup_root_t",mode=0o700)
    for slot in ("app","data"): directory(base/slot,"aetheri_setup_root_t",mode=0o700)
    setup["objects"]={slot:(base/slot).stat().st_ino for slot in ("app","data")}; setup["objects"]["root"]=base.stat().st_ino
    setup["layout_state"]="prepared"; save(record)
    if run("systemctl","show",setup["unit"],"--property=LoadState","--value")!="not-found": raise RuntimeError("setup unit already exists; no dispatch")
    properties={"User":"root","Group":"root","SELinuxContext":"system_u:system_r:aetheri_setup_t:s0","NoNewPrivileges":"yes",
        "CapabilityBoundingSet":"CAP_SYS_ADMIN CAP_DAC_READ_SEARCH","RestrictNamespaces":"mnt","PrivateMounts":"yes","PrivateNetwork":"yes",
        "PrivateDevices":"yes","ProtectSystem":"strict","ProtectHome":"yes","ProtectControlGroups":"yes","UMask":"0077",
        "ReadWritePaths":str(app_data_paths(record)["payload"]),"MemoryMax":"64M","TasksMax":"1","KillMode":"control-group","SendSIGKILL":"no",
        "TimeoutStartSec":"15s","TimeoutStopSec":"5s","RuntimeMaxSec":"35s","BindsTo":record["active_units"]["system"],"After":record["active_units"]["system"]}
    setup["state"]="starting"; save(record)
    try: run("systemd-run","--unit="+setup["unit"],"--service-type=notify",*("--property="+k+"="+v for k,v in properties.items()),str(ROOT/"setup-helper"),timeout=40)
    except subprocess.CalledProcessError:
        facts=dict(line.split("=",1) for line in run("systemctl","show",setup["unit"],"--property=MainPID,ControlGroup").splitlines())
        if facts=={"MainPID":"0","ControlGroup":""} and not unit_has_job(setup["unit"]): setup["state"]="failed"; save(record)
        raise
    setup["state"]="active"; save(record)
    pid=int(run("systemctl","show",setup["unit"],"--property=MainPID","--value"))
    raw_label=Path(f"/proc/{pid}/attr/current").read_bytes()
    label="system_u:system_r:aetheri_setup_t:s0"
    if raw_label not in (label.encode(),label.encode()+b"\0"): raise AssertionError("actual setup label differs")
    invocation=run("systemctl","show",setup["unit"],"--property=InvocationID","--value")
    group=run("systemctl","show",setup["unit"],"--property=ControlGroup","--value")
    cg=(Path("/sys/fs/cgroup")/group.lstrip("/")).stat().st_ino
    evidence=b"AETHSPC1"+hashlib.sha256(content).digest()+row[4]+bytes.fromhex(invocation)+struct.pack(">QI",cg,0)
    for value in (setup["unit"],label,group): raw=value.encode(); evidence+=struct.pack(">H",len(raw))+raw
    with sqlite3.connect(f"file:{state_path(record,'system')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        steps=db.execute("SELECT step_no,state,subject_ref,evidence_digest FROM operation_steps WHERE operation_id=? AND step_no>0 ORDER BY step_no",(row[0],)).fetchall()
    setup["observation"]={"label":label,"invocation":invocation,"cgroup":group,"cgroup_inode":cg,"expected_evidence":hashlib.sha256(evidence).hexdigest()}; save(record)
    if label!="system_u:system_r:aetheri_setup_t:s0" or steps!=[(1,"applied",bytes.fromhex(ids["launch"]),hashlib.sha256(content).digest()),(2,"applied",bytes.fromhex(invocation),hashlib.sha256(evidence).digest())]:
        raise AssertionError("actual setup context/journal differs")
    setup["checks"]={"peer":True}; save(record)
    mounts={line.split()[4]:set(line.split()[5].split(",")) for line in Path(f"/proc/{pid}/mountinfo").read_text().splitlines()}
    for slot,flags in (("app",{"ro","nodev","nosuid","nosymfollow"}),("data",{"rw","nodev","nosuid","noexec","nosymfollow"})):
        if not flags<=mounts.get(str(base/slot),set()): raise AssertionError("actual setup view flags differ")
        if list((base/slot).iterdir()): raise AssertionError("setup view propagated to controller")
    setup["checks"]["views"]=True; save(record)
    launch_probe(record,"setup-busy")
    setup["checks"]["live_denied"]=True; save(record)
    stop_unit(record["active_units"]["user"]); start(record)
    launch_probe(record,"setup-busy")
    setup["checks"]["replacement"]=True; save(record)
    setup_reconcile(record)


def launch_cancel(record):
    """Reconcile the existing fixed attempt only; never create/resend a reserve."""
    verified(record)
    if "launch_intent" not in record or "launch_config_sha256" not in record:
        raise RuntimeError("no protected correlated launch to cancel")
    if record["pending"]: raise RuntimeError("unknown dispatch; inspect before cancellation")
    if "user" in record.get("active_units",{}): stop_unit(record["active_units"]["user"])
    start(record)
    # A restarted root marks outstanding dispatch uncertain; the native method
    # then refuses release. Starting an endpoint never proves the old work drained.
    revisions=launch_probe(record,"cancel")
    if revisions[1]!=2: raise AssertionError("root withdrawal not terminal")
    verify_launch_terminal(record)
    record["launch_state"]="withdrawn"; save(record)
    print("Existing fixed launch withdrawn; original identities and data preserved.")


def reservation_replay_check(record):
    verified(record); stop(record)
    database=state_path(record,"system")/"catalogue.sqlite3"
    def business():
        with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
            return {name:db.execute('SELECT * FROM "'+name+'" ORDER BY rowid').fetchall() for name in ("operations","operation_steps","generation_leases","launch_dispatches")}
    before=business()
    try: start_unit(record,"system",admit=True)
    except subprocess.CalledProcessError:
        failed=record["last_failure"]["unit"]
        output=run("journalctl","-u",failed,"--output=cat","--no-pager")
        if "Authority startup/runtime failed: -116" not in output: raise RuntimeError("stale startup did not fail with ESTALE")
    else: raise AssertionError("old admission configuration reopened the gate")
    with sqlite3.connect(f"file:{database}?mode=ro",uri=True) as db:
        gate=db.execute("SELECT state,closing_operation_id FROM admission_gates WHERE subject_kind='install' AND subject_ref=?",(bytes.fromhex(record["fixture"]["install"]),)).fetchone()
    if gate!=("closed",None) or business()!=before: raise AssertionError("stale admission changed reservation history")
    boot=Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    record["reservation_replay_check"]=dict(stale_profile_denied=True,gate_closed=True,terminal_history_preserved=True,cross_boot_reopen=boot!=record["reservation_first_boot"])
    save(record); print(json.dumps(record["reservation_replay_check"]))


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
    if "setup" in record: stop_unit(record["setup"]["unit"])
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
    if "app_data" in record or "app_data" in record.get("exec_fixture",{}):
        raise RuntimeError("enrolled app data is not an empty bootstrap; preserve its allocation and metadata")
    if record["pending"]:
        raise RuntimeError("unacknowledged dispatch requires reconciliation")
    verified(record)
    stop(record)
    if "storage" in record:
        raise RuntimeError("allocated private data is not an empty bootstrap; use a reviewed storage retirement protocol")
    if "fixture" in record or "exec_fixture" in record:
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


def root_setup_withdrawn(record):
    setup=record["setup"]
    with sqlite3.connect(f"file:{state_path(record,'system')/'catalogue.sqlite3'}?mode=ro",uri=True) as db:
        rows=db.execute("SELECT d.state,l.state,o.outcome,o.reason_code,d.invocation_ref,d.ready_data_gate_revision,d.close_cause_operation_ref,s.evidence_digest "
            "FROM launch_dispatches d JOIN generation_leases l USING(lease_id) JOIN operations o ON o.operation_id=d.root_operation_id "
            "JOIN operation_steps s ON s.operation_id=o.operation_id AND s.step_no=3 AND s.action='release' AND s.state='applied' "
            "WHERE d.lease_id=? AND d.launch_id=? AND d.root_operation_id=?",tuple(bytes.fromhex(setup[k]) for k in ("lease","launch","root_operation"))).fetchall()
    return rows==[("terminal","released","cancelled","setup_drained_without_exec",None,None,bytes.fromhex(setup["user_operation"]),bytes.fromhex(record["setup_config_sha256"]))]


def refresh_owned_policy(record):
    current=ROOT/(MODULE+".cil"); content=(SOURCE/"policy/p0-services.cil").read_bytes()
    if hashlib.sha256(content).hexdigest()==record["policy_sha256"]: return
    directory_path=ROOT/"policy-refresh"
    directory(directory_path,"aetheri_root_t",mode=0o700)
    # semodule uses the input basename as the module name. Preserve it even for
    # staged policy replacement; a renamed .cil installs a different module.
    temporary=directory_path/(MODULE+".cil")
    publish(temporary,content,"aetheri_root_t",mode=0o644); flush_path(temporary)
    run("semodule","-X","400","-i",str(temporary))
    os.replace(temporary,current); flush_path(current.parent,True)
    directory_path.rmdir()
    record["policy_sha256"]=checksum(current); save(record)


def recover_unbound_setup(record):
    """Explicit owner recovery, never repeat the failed setup dispatch or edit SQL.

    The native writer verifies the retained failed PID1 invocation and the exact
    protected no-exec image before releasing anything. It runs without replacing
    the old Root runtime, under the catalogue's exclusive writer lock.
    """
    verified(record)
    setup=record["setup"]
    if setup["state"] not in ("failed","drained"): raise RuntimeError("only a known failed unbound setup can use offline recovery")
    if not setup.get("recovery_root_withdrawn"):
        stop(record); refresh_owned_policy(record)
        target=ROOT/"setup-recovery"
        if "setup_recovery_sha256" not in record:
            if target.exists() or target.is_symlink(): raise RuntimeError("unregistered recovery image")
            publish(target,(BUILD/"aetheri-authority").read_bytes(),"aetheri_system_exec_t",mode=0o555)
            record["setup_recovery_sha256"]=checksum(target); save(record)
        elif checksum(BUILD/"aetheri-authority")!=record["setup_recovery_sha256"]:
            # Old administrative invocations are independently stopped above;
            # the failed setup image/profile and original Root runtime stay fixed.
            temporary=ROOT/"setup-recovery.next"
            publish(temporary,(BUILD/"aetheri-authority").read_bytes(),"aetheri_system_exec_t",mode=0o555)
            os.replace(temporary,target)
            record.setdefault("setup_recovery_images",[]).append(record["setup_recovery_sha256"])
            record["setup_recovery_sha256"]=checksum(target); save(record)
        unit=start_unit(record,"system",recover_setup=True)
        stop_unit(unit)
        if not root_setup_withdrawn(record): raise AssertionError("native terminal setup receipt missing")
        setup["recovery_root_withdrawn"]=True; save(record)
    if not root_setup_withdrawn(record): raise AssertionError("recorded recovery differs from native receipt")
    if "setup_recovery_sha256" in record:
        target=ROOT/"setup-recovery"; owned_file(target,0o555)
        if checksum(target)!=record["setup_recovery_sha256"]: raise RuntimeError("recovery image changed")
        target.unlink(); record.pop("setup_recovery_sha256"); save(record)
    update_owned(record); start(record)
    launch_probe(record,"cancel"); verify_launch_terminal(record)
    setup["state"]="withdrawn"; record["launch_state"]="withdrawn"; save(record)
    setup_cleanup_layout(record)
    record["setup_recovery_check"]=dict(supervisor_only=True,held_peer=False,target_executed=False,native_receipt=True,private_data_preserved=True)
    save(record); print(json.dumps(record["setup_recovery_check"]))


def update_owned(record):
    verified(record, allow_new_probe=True, allow_unfinished_fixture=True, allow_new_projection=True)
    if "setup" in record and record["setup"]["state"]!="withdrawn" and not (record["setup"].get("recovery_root_withdrawn") and root_setup_withdrawn(record)):
        raise RuntimeError("reconcile the existing setup before replacing its authority/image")
    if "setup_sha256" not in record and ((ROOT/"setup-helper").exists() or (ROOT/"setup-helper").is_symlink()): raise RuntimeError("unregistered setup helper exists")
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
    temporary=ROOT/"setup-helper.next"
    publish(temporary,(BUILD/"aetheri-setup").read_bytes(),"aetheri_setup_exec_t",mode=0o555)
    os.replace(temporary,ROOT/"setup-helper")
    record["setup_sha256"]=checksum(ROOT/"setup-helper")
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
        elif action == "exec-projection-check": projection_check(record,profile="exec")
        elif action == "exec-data-prepare": data_prepare(record,profile="exec")
        elif action == "exec-data-check": data_check(record,profile="exec")
        elif action == "launch-check": launch_check(record)
        elif action == "launch-new-check": launch_check(record,fresh=True)
        elif action == "setup-check": setup_check(record)
        elif action == "setup-reconcile": setup_reconcile(record)
        elif action == "setup-recover-unbound": recover_unbound_setup(record)
        elif action == "launch-reopen-check": launch_reopen_check(record)
        elif action == "launch-cancel": launch_cancel(record)
        elif action == "reservation-check": reservation_check(record)
        elif action == "reservation-continue": reservation_check(record,resume=True)
        elif action == "reservation-new-check": reservation_check(record,fresh=True)
        elif action == "reservation-replay-check": reservation_replay_check(record)
        elif action == "update-owned": update_owned(record)
        elif action == "enroll-account": enroll_account(record)
        elif action == "account-check": account_check(record)
        elif action == "storage-prepare": storage_prepare(record)
        elif action == "storage-check": storage_check(record)
        elif action == "data-prepare": data_prepare(record)
        elif action == "data-check": data_check(record)
        elif action in ("exec-fixture-prepare","exec-fixture-check","exec-fixture-store-prepare","exec-fixture-store-check","exec-fixture-activation-prepare","exec-fixture-activation-check"):
            actions={"exec-fixture-prepare":fixture_prepare,"exec-fixture-check":fixture_check,
                     "exec-fixture-store-prepare":fixture_store_prepare,"exec-fixture-store-check":fixture_store_check,
                     "exec-fixture-activation-prepare":fixture_activation_prepare,"exec-fixture-activation-check":fixture_activation_check}
            actions[action](record,profile="exec")
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
