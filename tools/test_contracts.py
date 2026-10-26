"""Behavioral SQLite/migration and frozen-wire tests; synthetic metadata only."""
from pathlib import Path
import hashlib
import itertools
import os
import re
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import uuid
import xml.etree.ElementTree as ET

import catalogue_contracts as cc
import validate_contracts as v1

ROOT = cc.ROOT
SERIAL = itertools.count(1)
PHASES = ("backed_up","locked","tables_created","copied","old_dropped","renamed",
          "schema_installed","gates_backfilled","validated","version_set")
V1_HASHES = {
    "spec/schema/common-v1.sql": "5a7d370f2ccfffd0aaa5ba8368477be35c088dd9b3a9cf9b8b64e22772c6ee0b",
    "spec/schema/system-v1.sql": "0f32adbe053f75ba29908dee30c45d5720776cf9a95c02ab1d3bc51305f5c7e6",
    "spec/schema/user-v1.sql": "235b95b978cc2ce8535273cc8407b33daee5f822a6ef942965c95550b1d68526",
    "spec/api/management-v1.xml": "83eab1b87f720385bb9e9c1c59663f69e8b28dda5bff0e42cb6307bdd3ff32d0",
}


def uid():
    # Deterministic valid-format test identifiers, never a runtime ID generator.
    return uuid.UUID(int=next(SERIAL), version=4).bytes


def insert(db, table, **values):
    cols = ",".join(values)
    db.execute(f"INSERT INTO {table} ({cols}) VALUES ({','.join('?' for _ in values)})",
               tuple(values.values()))


def change(db, table, key, value, state, **extra):
    parts = ["state=?","revision=revision+1"] + [f"{name}=?" for name in extra]
    db.execute(f"UPDATE {table} SET {','.join(parts)} WHERE {key}=?",
               (state,*extra.values(),value))


def plan(db, f, kind="install", **extra):
    p = dict(plan_id=uid(),authority_epoch=f["epoch"],actor_ref=f["actor"],
             scope_ref=f["account"],kind=kind,component_id="aetheri.terminal",
             retention="none",intent_digest=b"H"*32,boot_id=f["boot"],
             expires_boottime_ns=300_000_000_000)
    if kind == "install":
        p["artifact_ref"] = b"A"*32
    p.update(extra)
    insert(db,"plans",**p)
    return p


def accept(db, p):
    op = uid()
    insert(db,"operations",operation_id=op,plan_id=p["plan_id"],actor_ref=p["actor_ref"],
           request_key=uid(),intent_digest=p["intent_digest"],state="accepted",
           phase="validate",accepted_unix_ms=1792972800000)
    return op


def rows(db):
    result = {}
    for (name,) in db.execute("SELECT name FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%'"):
        cols = [r[1] for r in db.execute(f'PRAGMA table_info("{name}")')]
        result[name] = (cols, sorted(db.execute(f'SELECT * FROM "{name}"').fetchall(),key=repr))
    return result


def legacy(db, kind):
    cc.bootstrap(db,kind,1)
    f = dict(epoch=uid(),boot=uid(),account=uid(),actor=uid(),publisher=uid(),
             install=uid(),data=uid(),generation=uid(),launch=uid(),system_epoch=uid())
    insert(db,"authority_meta",singleton=1,kind=kind,authority_epoch=f["epoch"],
           current_boot_id=f["boot"],owner_scope_ref=f["account"] if kind=="user" else None)
    p = plan(db,f)
    op = accept(db,p)
    insert(db,"plan_reads",plan_id=p["plan_id"],source_epoch=f["epoch"],
           subject_kind="component",subject_ref=b"aetheri.terminal",expected_revision=1)
    insert(db,"plan_effects",plan_id=p["plan_id"],effect_no=0,subject_kind="install",
           component_id="aetheri.terminal",subject_ref=f["install"],disposition="create",reason_code="fixture")
    insert(db,"operation_steps",operation_id=op,step_no=0,action="verify",
           subject_kind="install",subject_ref=f["install"],state="applied",evidence_digest=b"E"*32)
    insert(db,"operation_links",operation_id=op,link_no=0,remote_epoch=uid(),
           remote_plan_ref=uid(),remote_intent_digest=b"R"*32,remote_request_key=uid(),
           remote_operation_ref=uid(),state="accepted")
    db.execute("UPDATE operations SET state='completed',outcome='applied',completed_unix_ms=1792972801000 WHERE operation_id=?",(op,))
    if kind == "system":
        machine = uid()
        insert(db,"scopes",scope_id=machine,kind="machine",state="active")
        insert(db,"scopes",scope_id=f["account"],kind="account",os_uid=1000,state="active")
        runtime_i, runtime_g = uid(),uid()
        for component,artifact,inst,gen in [
                ("aetheri.terminal",b"A"*32,f["install"],f["generation"]),
                ("aetheri.runtime",b"R"*32,runtime_i,runtime_g)]:
            insert(db,"components",component_id=component,publisher_ref=f["publisher"],
                   kind="app" if component.endswith("terminal") else "runtime",display_name=component,protected=0)
            insert(db,"artifacts",artifact_id=artifact,component_id=component,version_label="0.1",
                   target="x86_64-linux-gnu",store_state="verified")
            insert(db,"installs",install_id=inst,component_id=component,scope_id=machine,state="preparing")
            insert(db,"generations",generation_id=gen,install_id=inst,component_id=component,
                   artifact_id=artifact,publication_state="committed")
            db.execute("UPDATE installs SET state='active',active_generation_id=? WHERE install_id=?",(gen,inst))
        insert(db,"generation_edges",consumer_id=f["generation"],provider_id=runtime_g)
        insert(db,"generation_leases",lease_id=uid(),generation_id=runtime_g,
               holder_authority=uid(),holder_ref=uid(),kind="recovery",boot_id=f["boot"],state="reserved")
    else:
        insert(db,"install_bindings",install_id=f["install"],component_id="aetheri.terminal",
               publisher_ref=f["publisher"],owner_scope=f["account"],system_epoch=f["system_epoch"],
               install_revision=1,state="active")
        insert(db,"data_epochs",data_epoch_id=f["data"],component_id="aetheri.terminal",
               publisher_ref=f["publisher"],owner_scope=f["account"],storage_ref=uid(),state="active",data_schema_version=1)
        insert(db,"attachments",install_id=f["install"],data_epoch_id=f["data"],
               component_id="aetheri.terminal",publisher_ref=f["publisher"],owner_scope=f["account"])
        p = plan(db,f,"launch",install_ref=f["install"],generation_ref=f["generation"],
                 data_epoch_ref=f["data"],action_id="run")
        op = accept(db,p)
        insert(db,"launches",launch_id=f["launch"],operation_id=op,install_id=f["install"],
               data_epoch_id=f["data"],generation_ref=f["generation"],lease_ref=uid(),
               system_epoch=f["system_epoch"],boot_id=f["boot"],session_ref="test",seat_ref="seat0",
               supervisor_unit="aetheri-legacy-fixture.service",state="requested",readiness="unknown")
        db.execute("UPDATE launches SET state='running',invocation_ref=? WHERE launch_id=?",(uid(),f["launch"]))
    db.commit()
    return f


class ContractCase(unittest.TestCase):
    def setUp(self):
        out = (ROOT/"out").resolve()
        if out.parent != ROOT.resolve():
            raise RuntimeError("test output escapes workspace")
        out.mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(prefix="contracts-",dir=out)
        self.work = Path(self.tmp.name).resolve()
        self.assertEqual(self.work.parent,out)
        self.connections = []

    def tearDown(self):
        for db in self.connections:
            db.close()
        self.assertEqual(self.work.resolve().parent,(ROOT/"out").resolve())
        self.tmp.cleanup()

    def connect(self,name):
        db = sqlite3.connect(self.work/name)
        db.execute("PRAGMA foreign_keys=ON")
        self.connections.append(db)
        return db

    def migrated(self,kind):
        db = self.connect(kind+".db")
        f = legacy(db,kind)
        cc.migrate_v1_to_v2(db,kind,backup=self.connect(kind+"-backup.db"))
        return db,f

    def active(self,kind):
        db,f = self.migrated(kind)
        f["inc"] = uid()
        db.execute("UPDATE authority_meta SET runtime_incarnation=?",(f["inc"],))
        db.execute("UPDATE admission_gates SET state='open',revision=revision+1")
        db.commit()
        return db,f

    def denied(self,db,action):
        db.execute("SAVEPOINT denied")
        try:
            with self.assertRaises(sqlite3.IntegrityError):
                action()
        finally:
            db.execute("ROLLBACK TO denied")
            db.execute("RELEASE denied")

    def assert_preserved(self,db,before):
        for name,(cols,expected) in before.items():
            fields = ",".join(f'"{c}"' for c in cols)
            actual = sorted(db.execute(f'SELECT {fields} FROM "{name}"').fetchall(),key=repr)
            self.assertEqual(actual,expected,name)

    def grant(self,db,f,**extra):
        resource,grant = uid(),uid()
        r = dict(resource_id=resource,owner_scope=f["account"],kind="file",fixture_root_ref=uid(),
                 locator="document.txt",display_name="Document",observed_boot_id=f["boot"],
                 broker_incarnation=f["inc"],observed_device=1,observed_inode=42,
                 observed_size=64,state="bound")
        r.update(extra)
        insert(db,"resources",**r)
        insert(db,"grants",grant_id=grant,resource_id=resource,launch_id=f["launch"],
               install_id=f["install"],data_epoch_id=f["data"],issuer_ref=f["actor"],
               rights="read",mode="native",lifetime="launch",state="pending")
        change(db,"grants","grant_id",grant,"active")
        return resource,grant

    def access(self,db,f,grant,**extra):
        v = dict(access_id=uid(),grant_id=grant,launch_id=f["launch"],
                 broker_incarnation=f["inc"],request_key=uid(),mode="native",state="opening")
        v.update(extra)
        insert(db,"access_leases",**v)
        return v["access_id"]

    def dispatch(self,db,f):
        launch,lease,broker = uid(),uid(),uid()
        insert(db,"generation_leases",lease_id=lease,generation_id=f["generation"],
               holder_authority=broker,holder_ref=launch,kind="launch",boot_id=f["boot"],state="reserved")
        p = plan(db,f,"launch",install_ref=f["install"],generation_ref=f["generation"],
                 data_epoch_ref=f["data"],action_id="run")
        op = accept(db,p)
        fence = db.execute("SELECT revision FROM admission_gates WHERE subject_ref=?",(f["install"],)).fetchone()[0]
        insert(db,"launch_dispatches",launch_id=launch,lease_id=lease,root_operation_id=op,
               broker_epoch=broker,broker_incarnation=uid(),account_scope_ref=f["account"],
               install_ref=f["install"],generation_ref=f["generation"],data_epoch_ref=f["data"],
               boot_id=f["boot"],action_id="run",session_ref="test",seat_ref="seat0",
               install_gate_revision=fence,unit_name="aetheri-app-"+launch.hex()+".service",state="reserved")
        return launch,lease

    def test_v1_reference_is_frozen_and_still_passes(self):
        for name,digest in V1_HASHES.items():
            self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),digest)
        v1.check_system()
        v1.check_user()
        self.assertEqual(v1.check_xml(),14)

    def test_fresh_v2_and_populated_migration_match(self):
        for kind,count in (("system",16),("user",15)):
            with self.subTest(kind=kind):
                source = self.connect(kind+".db")
                legacy(source,kind)
                before = rows(source)
                self.assertTrue(all(data[1] for data in before.values()),"all legacy tables populated")
                backup = self.connect(kind+"-bak.db")
                self.assertTrue(cc.migrate_v1_to_v2(source,kind,backup=backup))
                self.assert_preserved(source,before)
                self.assert_preserved(backup,before)
                cc.validate(backup,kind,1)
                self.assertEqual(source.execute("PRAGMA user_version").fetchone()[0],2)
                self.assertIsNone(source.execute("SELECT runtime_incarnation FROM authority_meta").fetchone()[0])
                self.assertFalse(source.execute("SELECT 1 FROM admission_gates WHERE state<>'closed'").fetchall())
                self.assertEqual(source.execute("SELECT COUNT(*) FROM sqlite_schema WHERE type='table'").fetchone()[0],count)
                fresh = self.connect(kind+"-fresh.db")
                cc.bootstrap(fresh,kind)
                self.assertEqual(cc.signature(fresh),cc.signature(source))
                with self.assertRaises(cc.ContractError):
                    cc.validate(source,kind,1)
                empty_backup = self.connect(kind+"-unused.db")
                self.assertFalse(cc.migrate_v1_to_v2(source,kind,backup=empty_backup))
                self.assertEqual(empty_backup.execute("SELECT COUNT(*) FROM sqlite_schema").fetchone()[0],0)
                source.close()
                self.connections.remove(source)
                reopened = self.connect(kind+".db")
                cc.validate(reopened,kind)
                self.assert_preserved(reopened,before)

    def test_every_precommit_failure_rolls_back_and_can_retry(self):
        for kind in ("system","user"):
            for phase in PHASES:
                with self.subTest(kind=kind,phase=phase):
                    db = self.connect(kind+"-"+phase+".db")
                    legacy(db,kind)
                    before = rows(db)
                    def fail(current):
                        if current == phase:
                            raise RuntimeError("injected migration failure")
                    with self.assertRaisesRegex(RuntimeError,"injected"):
                        cc.migrate_v1_to_v2(db,kind,backup=self.connect(kind+"-"+phase+"-b.db"),checkpoint=fail)
                    self.assertFalse(db.in_transaction)
                    cc.validate(db,kind,1)
                    self.assert_preserved(db,before)
                    self.assertTrue(cc.migrate_v1_to_v2(db,kind,backup=self.connect(kind+"-"+phase+"-retry.db")))

    def test_process_exit_recovers_before_and_after_commit(self):
        for kind in ("system","user"):
            for phase in ("old_dropped","version_set","committed"):
                with self.subTest(kind=kind,phase=phase):
                    name = kind+"-"+phase+".db"
                    db = self.connect(name)
                    legacy(db,kind)
                    before = rows(db)
                    db.close()
                    self.connections.remove(db)
                    result = subprocess.run([sys.executable,str(Path(__file__).resolve()),"--crash",
                                             phase,str(self.work/name),str(self.work/(name+"-bak")),kind],
                                            capture_output=True,text=True,timeout=20)
                    self.assertEqual(result.returncode,73,result.stderr)
                    reopened = self.connect(name)
                    cc.validate(reopened,kind,2 if phase=="committed" else 1)
                    self.assert_preserved(reopened,before)

    def test_new_foreign_key_failure_reverts_whole_migration(self):
        db = self.connect("foreign.db")
        f = legacy(db,"user")
        before = rows(db)
        def corrupt(phase):
            if phase == "schema_installed":
                insert(db,"plan_reads",plan_id=uid(),source_epoch=f["epoch"],subject_kind="grant",
                       subject_ref=uid(),expected_revision=1)
        with self.assertRaisesRegex(cc.ContractError,"foreign keys"):
            cc.migrate_v1_to_v2(db,"user",backup=self.connect("foreign-bak.db"),checkpoint=corrupt)
        cc.validate(db,"user",1)
        self.assert_preserved(db,before)

    def test_writer_between_backup_and_lock_is_rejected(self):
        db = self.connect("writer.db")
        legacy(db,"user")
        def writer(phase):
            if phase == "backed_up":
                other = sqlite3.connect(self.work/"writer.db")
                other.execute("UPDATE authority_meta SET catalogue_revision=2")
                other.commit()
                other.close()
        with self.assertRaisesRegex(cc.ContractError,"writer changed"):
            cc.migrate_v1_to_v2(db,"user",backup=self.connect("writer-bak.db"),checkpoint=writer)
        cc.validate(db,"user",1)
        self.assertEqual(db.execute("SELECT catalogue_revision FROM authority_meta").fetchone()[0],2)

    def test_wrong_version_kind_drift_and_backup_fail_closed(self):
        for case in ("version","kind","drift","backup","alias","transaction","fk"):
            with self.subTest(case=case):
                db = self.connect(case+".db")
                f = legacy(db,"user")
                backup = self.connect(case+"-bak.db")
                kind = "user"
                if case=="version":
                    db.execute("PRAGMA user_version=99")
                elif case=="kind":
                    kind = "system"
                elif case=="drift":
                    db.execute("CREATE TABLE unexpected(x)")
                elif case=="backup":
                    backup.execute("CREATE TABLE occupied(x)")
                    backup.commit()
                elif case=="alias":
                    backup = db
                elif case=="transaction":
                    db.execute("BEGIN")
                elif case=="fk":
                    db.execute("PRAGMA foreign_keys=OFF")
                    insert(db,"plan_reads",plan_id=uid(),source_epoch=f["epoch"],subject_kind="data",
                           subject_ref=uid(),expected_revision=1)
                    db.commit()
                    db.execute("PRAGMA foreign_keys=ON")
                before = rows(db)
                with self.assertRaises(cc.ContractError):
                    cc.migrate_v1_to_v2(db,kind,backup=backup)
                self.assert_preserved(db,before)
                if db.in_transaction:
                    db.rollback()
                self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0],99 if case=="version" else 1)

    def test_readonly_source_is_not_recreated(self):
        db = self.connect("readonly.db")
        legacy(db,"user")
        before = rows(db)
        db.close()
        self.connections.remove(db)
        ro = sqlite3.connect((self.work/"readonly.db").as_uri()+"?mode=ro",uri=True)
        ro.execute("PRAGMA foreign_keys=ON")
        self.connections.append(ro)
        with self.assertRaises(sqlite3.OperationalError):
            cc.migrate_v1_to_v2(ro,"user",backup=self.connect("ro-bak.db"))
        cc.validate(ro,"user",1)
        self.assert_preserved(ro,before)

    def test_sqlite_page_limit_rolls_back_incomplete_schema(self):
        db = self.connect("page-limit.db")
        legacy(db,"user")
        before = rows(db)
        pages = db.execute("PRAGMA page_count").fetchone()[0]
        db.execute(f"PRAGMA max_page_count={pages}")
        with self.assertRaises(sqlite3.OperationalError) as error:
            cc.migrate_v1_to_v2(db,"user",backup=self.connect("page-limit-bak.db"))
        self.assertEqual(error.exception.sqlite_errorcode,sqlite3.SQLITE_FULL)
        cc.validate(db,"user",1)
        self.assert_preserved(db,before)

    def test_partial_two_catalogue_upgrade_does_not_pass_both_gates(self):
        system = self.connect("pair-system.db")
        user = self.connect("pair-user.db")
        legacy(system,"system")
        legacy(user,"user")
        cc.migrate_v1_to_v2(system,"system",backup=self.connect("pair-s-bak.db"))
        cc.validate(system,"system")
        with self.assertRaises(cc.ContractError):
            cc.validate(user,"user")
        cc.migrate_v1_to_v2(user,"user",backup=self.connect("pair-u-bak.db"))
        cc.validate(user,"user")

    def test_grant_access_and_revoke_require_settlement(self):
        db,f = self.active("user")
        _,grant = self.grant(db,f)
        access = self.access(db,f,grant)
        change(db,"access_leases","access_id",access,"open")
        p = plan(db,f,"revoke",install_ref=f["install"],data_epoch_ref=f["data"],grant_ref=grant)
        insert(db,"plan_reads",plan_id=p["plan_id"],source_epoch=f["epoch"],subject_kind="grant",
               subject_ref=grant,expected_revision=2)
        op = accept(db,p)
        self.denied(db,lambda:db.execute("DELETE FROM operations WHERE operation_id=?",(op,)))
        change(db,"grants","grant_id",grant,"closing")
        self.denied(db,lambda:self.access(db,f,grant))
        self.denied(db,lambda:change(db,"grants","grant_id",grant,"revoked"))
        change(db,"access_leases","access_id",access,"released")
        change(db,"grants","grant_id",grant,"revoked")
        db.execute("UPDATE operations SET state='completed',outcome='applied',result_kind='grant',"
                   "result_ref=?,completed_unix_ms=1792972801000 WHERE operation_id=?",(grant,op))
        self.denied(db,lambda:change(db,"grants","grant_id",grant,"active"))
        self.denied(db,lambda:change(db,"access_leases","access_id",access,"open"))
        db.commit()
        cc.validate(db,"user")

    def test_new_grant_binding_and_supported_mode_are_checked(self):
        db,f = self.active("user")
        resource,_ = self.grant(db,f)
        base = dict(resource_id=resource,launch_id=f["launch"],install_id=f["install"],
                    data_epoch_id=f["data"],issuer_ref=f["actor"],rights="read",mode="native",
                    lifetime="launch",state="pending")
        for bad in (dict(data_epoch_id=uid()),dict(install_id=uid()),dict(rights="write"),
                    dict(mode="direct"),dict(lifetime="installation")):
            values = dict(base,grant_id=uid(),**bad)
            self.denied(db,lambda values=values:insert(db,"grants",**values))

    def test_unresolved_obligations_cannot_be_pruned(self):
        db,f = self.active("user")
        _,grant = self.grant(db,f)
        access = self.access(db,f,grant)
        self.denied(db,lambda:db.execute("DELETE FROM access_leases WHERE access_id=?",(access,)))
        self.denied(db,lambda:db.execute("DELETE FROM grants WHERE grant_id=?",(grant,)))
        self.denied(db,lambda:db.execute("DELETE FROM launches WHERE launch_id=?",(f["launch"],)))
        root,rf = self.active("system")
        launch,lease = self.dispatch(root,rf)
        self.denied(root,lambda:root.execute("DELETE FROM launch_dispatches WHERE launch_id=?",(launch,)))
        self.denied(root,lambda:root.execute("DELETE FROM generation_leases WHERE lease_id=?",(lease,)))

    def test_terminal_legacy_targets_migrate_but_cannot_reopen(self):
        for kind in ("system","user"):
            db = self.connect("terminal-"+kind+".db")
            f = legacy(db,kind)
            if kind == "system":
                db.execute("UPDATE installs SET state='removed',active_generation_id=NULL WHERE install_id=?",(f["install"],))
                target = f["install"]
            else:
                db.execute("UPDATE launches SET state='exited' WHERE launch_id=?",(f["launch"],))
                db.execute("DELETE FROM attachments WHERE install_id=?",(f["install"],))
                db.execute("UPDATE data_epochs SET state='purged' WHERE data_epoch_id=?",(f["data"],))
                target = f["data"]
            db.commit()
            before = rows(db)
            cc.migrate_v1_to_v2(db,kind,backup=self.connect("terminal-"+kind+"-bak.db"))
            self.assert_preserved(db,before)
            self.denied(db,lambda:db.execute("UPDATE admission_gates SET state='open',revision=revision+1 WHERE subject_ref=?",(target,)))

    def test_scope_context_limits_and_duplicate_access_are_rejected(self):
        db,f = self.active("user")
        for extra in (dict(owner_scope=uid()),dict(broker_incarnation=uid()),
                      dict(observed_size=65537),dict(observed_size=-1),dict(locator="/outside"),
                      dict(locator="../outside"),dict(locator="a/../outside"),dict(locator="a//b"),dict(locator=".")):
            self.denied(db,lambda extra=extra:self.grant(db,f,**extra))
        resource,grant = self.grant(db,f)
        self.denied(db,lambda:self.access(db,f,grant,launch_id=uid()))
        self.denied(db,lambda:self.access(db,f,grant,broker_incarnation=uid()))
        self.denied(db,lambda:db.execute("UPDATE grants SET data_epoch_id=? WHERE grant_id=?",(uid(),grant)))
        self.denied(db,lambda:db.execute("UPDATE grants SET revision=1 WHERE grant_id=?",(grant,)))
        access = self.access(db,f,grant)
        key = db.execute("SELECT request_key FROM access_leases WHERE access_id=?",(access,)).fetchone()[0]
        self.denied(db,lambda:self.access(db,f,grant,request_key=key))
        change(db,"resources","resource_id",resource,"needs_reselect")
        self.denied(db,lambda:change(db,"resources","resource_id",resource,"bound"))
        self.denied(db,lambda:self.access(db,f,grant))

    def test_stale_revoke_plan_cannot_accept(self):
        db,f = self.active("user")
        _,grant = self.grant(db,f)
        p = plan(db,f,"revoke",install_ref=f["install"],data_epoch_ref=f["data"],grant_ref=grant)
        insert(db,"plan_reads",plan_id=p["plan_id"],source_epoch=f["epoch"],subject_kind="grant",
               subject_ref=grant,expected_revision=1)  # Active grant is revision 2.
        self.denied(db,lambda:accept(db,p))
        self.denied(db,lambda:plan(db,f,"revoke",install_ref=uid(),data_epoch_ref=f["data"],grant_ref=grant))

    def test_broker_restart_invalidates_resources_without_releasing_app(self):
        db,f = self.active("user")
        resource,grant = self.grant(db,f)
        access = self.access(db,f,grant)
        change(db,"access_leases","access_id",access,"open")
        db.execute("UPDATE authority_meta SET runtime_incarnation=?",(uid(),))
        self.assertEqual(db.execute("SELECT state FROM resources WHERE resource_id=?",(resource,)).fetchone()[0],"needs_reselect")
        self.assertEqual(db.execute("SELECT state FROM grants WHERE grant_id=?",(grant,)).fetchone()[0],"closing")
        self.assertEqual(db.execute("SELECT state FROM access_leases WHERE access_id=?",(access,)).fetchone()[0],"uncertain")
        self.assertEqual(db.execute("SELECT state FROM launches WHERE launch_id=?",(f["launch"],)).fetchone()[0],"running")
        self.denied(db,lambda:self.access(db,f,grant))
        self.denied(db,lambda:change(db,"grants","grant_id",grant,"expired"))

    def test_purge_waits_for_grants_and_never_retargets_new_data(self):
        db,f = self.active("user")
        _,grant = self.grant(db,f)
        access = self.access(db,f,grant)
        db.execute("UPDATE admission_gates SET state='closed',revision=revision+1 WHERE subject_ref=?",(f["data"],))
        db.execute("UPDATE launches SET state='exited' WHERE launch_id=?",(f["launch"],))
        db.execute("DELETE FROM attachments WHERE install_id=?",(f["install"],))
        self.denied(db,lambda:db.execute("UPDATE data_epochs SET state='purging' WHERE data_epoch_id=?",(f["data"],)))
        change(db,"access_leases","access_id",access,"released")
        change(db,"grants","grant_id",grant,"revoked")
        db.execute("UPDATE data_epochs SET state='purging' WHERE data_epoch_id=?",(f["data"],))
        i2,d2 = uid(),uid()
        insert(db,"install_bindings",install_id=i2,component_id="aetheri.terminal",publisher_ref=f["publisher"],
               owner_scope=f["account"],system_epoch=f["system_epoch"],install_revision=1,state="active")
        insert(db,"data_epochs",data_epoch_id=d2,component_id="aetheri.terminal",publisher_ref=f["publisher"],
               owner_scope=f["account"],storage_ref=uid(),state="active",data_schema_version=1)
        insert(db,"attachments",install_id=i2,data_epoch_id=d2,component_id="aetheri.terminal",
               publisher_ref=f["publisher"],owner_scope=f["account"])
        db.execute("UPDATE data_epochs SET state='purged' WHERE data_epoch_id=?",(f["data"],))
        self.assertEqual(db.execute("SELECT data_epoch_id FROM attachments WHERE install_id=?",(i2,)).fetchone()[0],d2)
        self.denied(db,lambda:db.execute("UPDATE admission_gates SET state='open',revision=revision+1 WHERE subject_ref=?",(f["data"],)))

    def test_root_dispatch_is_fenced_and_uncertain_is_not_redispatched(self):
        db,f = self.active("system")
        launch,lease = self.dispatch(db,f)
        change(db,"launch_dispatches","launch_id",launch,"dispatching",ready_data_gate_revision=1)
        change(db,"launch_dispatches","launch_id",launch,"uncertain")
        self.denied(db,lambda:change(db,"launch_dispatches","launch_id",launch,"dispatching"))
        self.denied(db,lambda:db.execute("UPDATE generation_leases SET state='released' WHERE lease_id=?",(lease,)))
        change(db,"launch_dispatches","launch_id",launch,"observed",invocation_ref=uid())
        self.denied(db,lambda:db.execute("UPDATE launch_dispatches SET invocation_ref=?,revision=revision+1 WHERE launch_id=?",(uid(),launch)))
        change(db,"launch_dispatches","launch_id",launch,"closing")
        change(db,"launch_dispatches","launch_id",launch,"terminal")
        db.execute("UPDATE generation_leases SET state='released' WHERE lease_id=?",(lease,))
        self.denied(db,lambda:db.execute("UPDATE generation_leases SET state='reserved' WHERE lease_id=?",(lease,)))
        self.denied(db,lambda:change(db,"launch_dispatches","launch_id",launch,"reserved"))

    def test_root_close_before_dispatch_blocks_new_start(self):
        db,f = self.active("system")
        launch,_ = self.dispatch(db,f)
        p = plan(db,f,"remove",install_ref=f["install"],retention="keep_data")
        op = accept(db,p)
        db.execute("UPDATE admission_gates SET state='closing',closing_operation_id=?,revision=revision+1 WHERE subject_ref=?",(op,f["install"]))
        self.denied(db,lambda:change(db,"launch_dispatches","launch_id",launch,"dispatching",ready_data_gate_revision=1))
        self.denied(db,lambda:self.dispatch(db,f))

    def test_root_restart_closes_gates_and_keeps_dispatch_obligation(self):
        db,f = self.active("system")
        launch,lease = self.dispatch(db,f)
        db.execute("UPDATE authority_meta SET runtime_incarnation=?",(uid(),))
        self.assertEqual(db.execute("SELECT state FROM launch_dispatches WHERE launch_id=?",(launch,)).fetchone()[0],"uncertain")
        self.assertFalse(db.execute("SELECT 1 FROM admission_gates WHERE state='open'").fetchall())
        self.denied(db,lambda:db.execute("UPDATE generation_leases SET state='released' WHERE lease_id=?",(lease,)))
        self.denied(db,lambda:plan(db,f,"revoke",install_ref=f["install"],data_epoch_ref=f["data"],grant_ref=uid()))


WIRE = {
    "Documents1": {
        "GetGrant": ("ay","ay ay as s t ay"),
        "OpenRead": ("ay t ay ay ay","ay t s"),
        "ReadRange": ("ay ay t u","ay b"),
        "CloseAccess": ("ay ay","s t"),
        "PrepareRevokeGrant": ("ay t","ay ay ay t ay u")},
    "Runtime1": {
        "ReserveLaunch": ("ay ay ay ay ay ay ay ay ay ay t s s s","ay t t"),
        "DispatchLaunch": ("ay ay t t ay","ay s t"),
        "CloseLaunch": ("ay ay ay ay t","s t"),
        "GetDispatch": ("ay","ay s s ay t t")}
}


def parse_type(sig):
    def one(i):
        if i >= len(sig):
            raise ValueError("incomplete type")
        if sig[i] in "ybnqiuxtdhsogv":
            return i+1
        if sig[i] == "a":
            return one(i+1)
        if sig[i] == "(":
            j = one(i+1)
            while j < len(sig) and sig[j] != ")":
                j = one(j)
            if j < len(sig):
                return j+1
        raise ValueError("invalid type")
    if one(0) != len(sig):
        raise ValueError("argument needs one complete type")


def wire_shape(root):
    result = {}
    for interface in root.findall("interface"):
        name = interface.attrib["name"]
        if name in result:
            raise ValueError("duplicate interface")
        result[name] = {}
        for method in interface.findall("method"):
            n = method.attrib["name"]
            if n in result[name]:
                raise ValueError("duplicate method")
            groups = {"in":[],"out":[]}
            names = set()
            for arg in method.findall("arg"):
                if arg.attrib["name"] in names:
                    raise ValueError("duplicate argument")
                names.add(arg.attrib["name"])
                parse_type(arg.attrib["type"])
                direction = arg.attrib["direction"]
                if direction not in groups:
                    raise ValueError("invalid direction")
                groups[direction].append(arg.attrib["type"])
            result[name][n] = tuple(" ".join(groups[x]) for x in ("in","out"))
    return result


class ApiContracts(unittest.TestCase):
    def test_frozen_extension_signatures_and_limits(self):
        for short,expected in WIRE.items():
            file = "documents" if short=="Documents1" else "runtime"
            root = ET.parse(ROOT/f"spec/api/{file}-v1.xml").getroot()
            self.assertEqual(wire_shape(root),{"org.aetheri."+short:expected})
            interface = root.find("interface")
            annotations = {a.attrib["name"]:a.attrib["value"] for a in interface.findall("annotation")}
            self.assertEqual(annotations["org.aetheri.Contract.Schema"],"2")
            self.assertEqual(annotations["org.aetheri.Contract.Version"],"1.1")
            self.assertEqual(annotations["org.aetheri.Contract.Visibility"],"broker-only" if file=="runtime" else "caller-scoped")
            for arg in root.findall(".//arg"):
                limits = {a.attrib["name"].split(".")[-1]:a.attrib["value"] for a in arg.findall("annotation")}
                self.assertEqual(len(limits),len(arg.findall("annotation")),"duplicate argument annotation")
                if arg.attrib["type"]=="ay" and arg.attrib["name"]!="data":
                    self.assertEqual(limits["Bytes"],"32" if arg.attrib["name"]=="intent_digest" else "16")
                if arg.attrib["name"]=="maximum_bytes":
                    self.assertEqual((limits["Minimum"],limits["Maximum"]),("1","16384"))
                if arg.attrib["name"]=="data":
                    self.assertEqual(limits["Maximum"],"16384")
                if arg.attrib["name"]=="size":
                    self.assertEqual(limits["Maximum"],"65536")
            self.assertFalse(any(a.attrib["name"] in ("uid","pid","executable","mount_path","security_label")
                                 for a in root.findall(".//arg[@direction='in']")))

    def test_malformed_wire_controls_are_detected(self):
        for sig in ("","a","()","ayx","r","(ay"):
            with self.subTest(sig=sig),self.assertRaises(ValueError):
                parse_type(sig)
        for change_xml in (
                lambda r:r.find(".//arg").set("type","a"),
                lambda r:r.find(".//arg").set("direction","sideways"),
                lambda r:r.find("interface").append(ET.fromstring('<method name="GetGrant"/>'))):
            root = ET.parse(ROOT/"spec/api/documents-v1.xml").getroot()
            change_xml(root)
            with self.assertRaises(ValueError):
                wire_shape(root)


def crash_worker():
    phase,source,backup,kind = sys.argv[2:]
    base = (ROOT/"out").resolve()
    for value in (source,backup):
        if not Path(value).resolve().is_relative_to(base):
            raise RuntimeError("crash fixture escapes test output")
    db = sqlite3.connect(source)
    db.execute("PRAGMA foreign_keys=ON")
    target = sqlite3.connect(backup)
    def die(current):
        if current == phase:
            os._exit(73)
    cc.migrate_v1_to_v2(db,kind,backup=target,checkpoint=die)
    raise RuntimeError("crash phase not reached")


if __name__ == "__main__":
    if len(sys.argv)>1 and sys.argv[1]=="--crash":
        crash_worker()
    else:
        print(f"Python {sys.version.split()[0]}; SQLite {sqlite3.sqlite_version}",flush=True)
        unittest.main(verbosity=2)
