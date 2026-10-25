"""Validate draft constraints in disposable in-memory SQLite databases; no services."""
from pathlib import Path
import re
import sqlite3
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
checks = 0


def uid():
    return uuid.uuid4().bytes


def insert(db, table, **values):
    columns = ",".join(values)
    marks = ",".join("?" for _ in values)
    db.execute(f"INSERT INTO {table} ({columns}) VALUES ({marks})", tuple(values.values()))


def require(condition, label):
    global checks
    if not condition:
        raise AssertionError(label)
    checks += 1


def rejects(db, label, action):
    global checks
    db.execute("SAVEPOINT negative_case")
    try:
        action()
    except sqlite3.IntegrityError:
        checks += 1
    else:
        raise AssertionError(f"constraint accepted: {label}")
    finally:
        db.execute("ROLLBACK TO negative_case")
        db.execute("RELEASE negative_case")


def catalogue(kind):
    db = sqlite3.connect(":memory:")
    for name in ("common", kind):
        db.executescript((ROOT / f"spec/schema/{name}-v1.sql").read_text(encoding="utf-8"))
    epoch, boot, owner = uid(), uid(), uid() if kind == "user" else None
    insert(db, "authority_meta", singleton=1, kind=kind, authority_epoch=epoch,
           current_boot_id=boot, owner_scope_ref=owner)
    require(db.execute("PRAGMA foreign_keys").fetchone()[0] == 1, "foreign keys enabled")
    require(db.execute("PRAGMA synchronous").fetchone()[0] == 3,"EXTRA sync configured")
    return db, epoch, boot, owner


def plan(db, epoch, boot, scope, actor, kind="install", **overrides):
    values = dict(plan_id=uid(), authority_epoch=epoch, actor_ref=actor,
                  scope_ref=scope, kind=kind, component_id="aetheri.terminal",
                  artifact_ref=b"A" * 32 if kind == "install" else None,
                  retention="none", intent_digest=uid() + uid(), boot_id=boot,
                  expires_boottime_ns=300_000_000_000)
    values.update(overrides)
    insert(db, "plans", **values)
    return values


def operation(db, p, **overrides):
    values = dict(operation_id=uid(), plan_id=p["plan_id"], actor_ref=p["actor_ref"],
                  request_key=uid(), intent_digest=p["intent_digest"],
                  state="accepted", phase="validate", accepted_unix_ms=1_792_886_400_000)
    values.update(overrides)
    insert(db, "operations", **values)
    return values


def check_common(db, epoch, boot, scope):
    actor = uid()
    p = plan(db, epoch, boot, scope, actor)
    o = operation(db, p)
    require(db.execute("SELECT state FROM plans WHERE plan_id=?", (p["plan_id"],)).fetchone()[0]
            == "accepted", "accept consumes plan")
    rejects(db, "plan cannot accept twice", lambda: operation(db, p))
    p2 = plan(db, epoch, boot, scope, actor)
    rejects(db, "request key cannot mean a second intent",
            lambda: operation(db, p2, request_key=o["request_key"]))
    rejects(db, "actor binding", lambda: operation(db, p2, actor_ref=uid()))
    rejects(db, "digest binding", lambda: operation(db, p2, intent_digest=b"X" * 32))
    rejects(db, "completed needs outcome", lambda: db.execute(
        "UPDATE operations SET state='completed' WHERE operation_id=?", (o["operation_id"],)))
    rejects(db, "unfinished cannot claim outcome", lambda: db.execute(
        "UPDATE operations SET outcome='applied' WHERE operation_id=?", (o["operation_id"],)))
    rejects(db, "immutable target", lambda: db.execute(
        "UPDATE plans SET install_ref=? WHERE plan_id=?", (uid(), p2["plan_id"])))
    stale = plan(db, uid(), boot, scope, actor)
    rejects(db, "old authority epoch", lambda: operation(db, stale))
    stale_boot = plan(db, epoch, uid(), scope, actor)
    rejects(db, "old boot preview", lambda: operation(db, stale_boot))
    rejects(db, "strict identifier type", lambda: db.execute(
        "UPDATE authority_meta SET authority_epoch='sixteen letters!'"))
    db.execute("UPDATE operations SET state='completed',outcome='applied',completed_unix_ms=1792886401000,revision=2 WHERE operation_id=?",
               (o["operation_id"],))
    require(db.execute("SELECT revision FROM operations WHERE operation_id=?",
                       (o["operation_id"],)).fetchone()[0] == 2, "valid completion")
    rejects(db,"completed operation is terminal",lambda: db.execute(
        "UPDATE operations SET state='running',outcome=NULL WHERE operation_id=?",(o["operation_id"],)))
    rejects(db,"embedded NUL component",lambda: plan(
        db,epoch,boot,scope,actor,component_id="aetheri.terminal\\0hidden"))
    link = dict(operation_id=o["operation_id"],link_no=0,remote_epoch=uid(),
                remote_plan_ref=uid(),remote_intent_digest=b"D"*32,remote_request_key=uid(),state="prepared")
    insert(db,"operation_links",**link)
    rejects(db,"delegation needs receipt",lambda: db.execute(
        "UPDATE operation_links SET state='accepted' WHERE operation_id=?",(o["operation_id"],)))
    rejects(db,"delegation cannot retarget",lambda: db.execute(
        "UPDATE operation_links SET remote_plan_ref=? WHERE operation_id=?",(uid(),o["operation_id"])))
    rejects(db,"accepted intent cannot retarget",lambda: db.execute(
        "UPDATE operations SET request_key=? WHERE operation_id=?",(uid(),o["operation_id"])))
    db.execute("UPDATE operation_links SET remote_operation_ref=?,state='accepted' WHERE operation_id=?",
               (uid(),o["operation_id"]))
    rejects(db,"remote receipt cannot retarget",lambda: db.execute(
        "UPDATE operation_links SET remote_operation_ref=? WHERE operation_id=?",(uid(),o["operation_id"])))


def check_system():
    db, epoch, boot, _ = catalogue("system")
    scope, publisher, i1, i2 = uid(), uid(), uid(), uid()
    insert(db, "scopes", scope_id=scope, kind="machine", state="active")
    check_common(db, epoch, boot, scope)
    for name in ("aetheri.terminal", "aetheri.runtime"):
        insert(db, "components", component_id=name, publisher_ref=publisher,
               kind="app" if name.endswith("terminal") else "runtime",
               display_name=name, protected=0)
    for artifact, component in ((b"A"*32, "aetheri.terminal"), (b"B"*32, "aetheri.terminal"),
                                (b"R"*32, "aetheri.runtime")):
        insert(db, "artifacts", artifact_id=artifact, component_id=component,
               version_label="0.1", target="x86_64-linux-gnu", store_state="verified")
    insert(db, "installs", install_id=i1, component_id="aetheri.terminal",
           scope_id=scope, state="preparing")
    rejects(db, "duplicate scoped enrollment", lambda: insert(
        db, "installs", install_id=i2, component_id="aetheri.terminal", scope_id=scope, state="preparing"))
    g1, g2, g3 = uid(), uid(), uid()
    for gen, artifact in ((g1,b"A"*32),(g2,b"B"*32),(g3,b"A"*32)):
        insert(db, "generations", generation_id=gen, install_id=i1,
               component_id="aetheri.terminal", artifact_id=artifact, publication_state="committed")
    def activate(gen):
        db.execute("UPDATE installs SET state='active',active_generation_id=?,revision=revision+1 WHERE install_id=?",
                   (gen,i1))
    activate(g1)
    activate(g2)
    rejects(db, "old activation cannot be revived", lambda: activate(g1))
    require(db.execute("SELECT active_generation_id FROM installs WHERE install_id=?",
                       (i1,)).fetchone()[0] == g2, "failed activation leaves pointer intact")
    activate(g3)
    require(db.execute("SELECT artifact_id FROM generations WHERE generation_id=?",
                       (g3,)).fetchone()[0] == b"A"*32, "rollback is fresh generation, old artifact")
    rejects(db, "artifact cannot change component", lambda: insert(
        db, "generations", generation_id=uid(), install_id=i1, component_id="aetheri.terminal",
        artifact_id=b"R"*32, publication_state="committed"))
    rejects(db, "missing artifact", lambda: insert(
        db, "generations", generation_id=uid(), install_id=i1, component_id="aetheri.terminal",
        artifact_id=b"Z"*32, publication_state="committed"))
    rejects(db, "active generation cannot be reclaimed", lambda: db.execute(
        "UPDATE generations SET publication_state='reclaimed' WHERE generation_id=?", (g3,)))
    lease = uid()
    insert(db, "generation_leases", lease_id=lease, generation_id=g1,
           holder_authority=epoch, holder_ref=uid(), kind="launch", boot_id=boot, state="reserved")
    rejects(db, "lease pins retired generation", lambda: db.execute(
        "UPDATE generations SET publication_state='reclaiming' WHERE generation_id=?", (g1,)))
    db.execute("UPDATE generation_leases SET state='released' WHERE lease_id=?", (lease,))
    insert(db, "generation_edges", consumer_id=g3, provider_id=g1)
    rejects(db, "consumer pins provider", lambda: db.execute(
        "UPDATE generations SET publication_state='reclaimed' WHERE generation_id=?", (g1,)))
    db.execute("DELETE FROM generation_edges WHERE consumer_id=?", (g3,))
    db.execute("UPDATE generations SET publication_state='reclaimed' WHERE generation_id=?", (g1,))
    rejects(db, "new lease cannot pin reclaimed code", lambda: insert(
        db, "generation_leases", lease_id=uid(), generation_id=g1, holder_authority=epoch,
        holder_ref=uid(), kind="launch", boot_id=boot, state="reserved"))
    rejects(db, "artifact still used by another generation", lambda: db.execute(
        "UPDATE artifacts SET store_state='reclaimed' WHERE artifact_id=?", (b"A"*32,)))
    db.execute("UPDATE installs SET state='removed',active_generation_id=NULL WHERE install_id=?", (i1,))
    insert(db, "installs", install_id=i2, component_id="aetheri.terminal", scope_id=scope, state="preparing")
    rejects(db, "generation belongs to old installation", lambda: db.execute(
        "UPDATE installs SET state='active',active_generation_id=? WHERE install_id=?", (g2,i2)))
    rejects(db,"removed install cannot revive",lambda: db.execute(
        "UPDATE installs SET state='preparing' WHERE install_id=?",(i1,)))
    integrity(db)


def check_user():
    db, epoch, boot, owner = catalogue("user")
    check_common(db, epoch, boot, owner)
    publisher, system_epoch, actor = uid(), uid(), uid()
    i1, i2, d1, d2, gen = uid(), uid(), uid(), uid(), uid()
    def binding(i, **overrides):
        v = dict(install_id=i,component_id="aetheri.terminal",publisher_ref=publisher,
                 owner_scope=owner,system_epoch=system_epoch,install_revision=1,state="active")
        v.update(overrides)
        insert(db,"install_bindings",**v)
    def data(d, **overrides):
        v = dict(data_epoch_id=d,component_id="aetheri.terminal",publisher_ref=publisher,
                 owner_scope=owner,storage_ref=uid(),state="active",data_schema_version=1)
        v.update(overrides)
        insert(db,"data_epochs",**v)
    def attach(i,d,**overrides):
        v = dict(install_id=i,data_epoch_id=d,component_id="aetheri.terminal",
                 publisher_ref=publisher,owner_scope=owner)
        v.update(overrides)
        insert(db,"attachments",**v)
    binding(i1)
    data(d1)
    rejects(db,"wrong account data",lambda: data(uid(),owner_scope=uid()))
    rejects(db,"wrong account binding",lambda: binding(uid(),owner_scope=uid()))
    rejects(db,"wrong publisher attachment",lambda: attach(i1,d1,publisher_ref=uid()))
    rejects(db,"wrong component attachment",lambda: attach(i1,d1,component_id="other.app"))
    attach(i1,d1)
    rejects(db,"attached data cannot purge",lambda: db.execute(
        "UPDATE data_epochs SET state='purging' WHERE data_epoch_id=?",(d1,)))
    p = plan(db,epoch,boot,owner,actor,kind="launch",install_ref=i1,
             generation_ref=gen,data_epoch_ref=d1,action_id="open")
    o = operation(db,p)
    launch = uid()
    insert(db,"launches",launch_id=launch,operation_id=o["operation_id"],install_id=i1,
           data_epoch_id=d1,generation_ref=gen,lease_ref=uid(),system_epoch=system_epoch,
           boot_id=boot,session_ref="session-fixture",seat_ref="seat-fixture",
           supervisor_unit="aetheri-fixture.service",state="requested",readiness="unknown")
    rejects(db,"unreconciled run prevents detach",lambda: db.execute(
        "DELETE FROM attachments WHERE install_id=?",(i1,)))
    db.execute("UPDATE launches SET state='recovery_required' WHERE launch_id=?",(launch,))
    rejects(db,"unknown run still prevents detach",lambda: db.execute(
        "DELETE FROM attachments WHERE install_id=?",(i1,)))
    db.execute("UPDATE launches SET state='exited',revision=revision+1 WHERE launch_id=?",(launch,))
    db.execute("DELETE FROM attachments WHERE install_id=?",(i1,))
    db.execute("UPDATE install_bindings SET state='revoked' WHERE install_id=?",(i1,))
    db.execute("UPDATE data_epochs SET state='purging' WHERE data_epoch_id=?",(d1,))
    binding(i2)
    data(d2)
    attach(i2,d2)
    rejects(db,"purging data cannot reattach",lambda: attach(i1,d1))
    db.execute("UPDATE data_epochs SET state='purged' WHERE data_epoch_id=?",(d1,))
    rejects(db,"purged data cannot resurrect",lambda: db.execute(
        "UPDATE data_epochs SET state='active' WHERE data_epoch_id=?",(d1,)))
    require(db.execute("SELECT data_epoch_id FROM attachments WHERE install_id=?",
                       (i2,)).fetchone()[0] == d2,"old purge leaves new install/data unchanged")
    rejects(db,"ended run cannot restart under old ID",lambda: db.execute(
        "UPDATE launches SET state='running' WHERE launch_id=?",(launch,)))
    db.execute("DELETE FROM attachments WHERE install_id=?",(i2,))
    db.execute("UPDATE data_epochs SET state='retained' WHERE data_epoch_id=?",(d2,))
    rejects(db,"retained data requires explicit activation",lambda: attach(i2,d2))
    db.execute("UPDATE data_epochs SET state='active' WHERE data_epoch_id=?",(d2,))
    attach(i2,d2)
    require(db.execute("SELECT COUNT(*) FROM attachments").fetchone()[0] == 1,"explicit compatible reattach")
    integrity(db)


def integrity(db):
    db.commit()  # Includes deferred foreign keys at the actual transaction boundary.
    require(not db.execute("PRAGMA foreign_key_check").fetchall(),"foreign key integrity")
    require(db.execute("PRAGMA integrity_check").fetchone()[0] == "ok","database integrity")
    db.close()


def check_xml():
    root = ET.parse(ROOT / "spec/api/management-v1.xml").getroot()
    names = set()
    methods = 0

    def signature(sig):
        # Deliberately supports only the basic/array/struct subset used by this draft.
        def one(pos):
            if pos >= len(sig):
                raise AssertionError(f"incomplete signature: {sig}")
            token = sig[pos]
            if token in "ybnqiuxtdhsogv":
                return pos + 1
            if token == "a":
                return one(pos + 1)
            if token == "(":
                pos = one(pos + 1)
                while pos < len(sig) and sig[pos] != ")":
                    pos = one(pos)
                if pos < len(sig):
                    return pos + 1
            raise AssertionError(f"unsupported or malformed signature: {sig}")
        require(one(0) == len(sig),"one complete argument type")

    for interface in root.findall("interface"):
        name = interface.attrib["name"]
        require(name not in names and re.fullmatch(r"(?:[A-Za-z_][A-Za-z0-9_]*\.)+[A-Za-z_][A-Za-z0-9_]*",name),
                "unique valid interface name")
        names.add(name)
        members = set()
        for member in interface:
            key = (member.tag,member.attrib["name"])
            require(key not in members,"unique member")
            members.add(key)
            args = set()
            for arg in member.findall("arg"):
                require(arg.attrib["name"] not in args,"unique argument")
                args.add(arg.attrib["name"])
                require(member.tag == "signal" or arg.attrib["direction"] in ("in","out"),"argument direction")
                signature(arg.attrib["type"])
            methods += member.tag == "method"
    require(methods == 14,"expected core method inventory")
    return methods


if __name__ == "__main__":
    if sqlite3.sqlite_version_info < (3,37,0):
        raise SystemExit("SQLite >= 3.37.0 required for STRICT tables")
    check_system()
    check_user()
    methods = check_xml()
    print(f"PASS: {checks} assertions; {methods} API methods; SQLite {sqlite3.sqlite_version}.")
    print("In-memory constraint and XML checks only; no D-Bus, filesystem, migration or hardware acceptance.")
