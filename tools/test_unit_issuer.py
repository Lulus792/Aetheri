"""Format and explicit v10->v11 migration, including lost replies and process death."""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import unittest

import catalogue_contracts as cc
import runtime_contracts as rc
import unit_issuer_contracts as codec
from test_contracts import ContractCase, rows

ROOT = Path(__file__).resolve().parents[1]


class IssuerCodec(unittest.TestCase):
    def test_vector_and_adversarial_frames(self):
        vector = json.loads((ROOT/'spec/vectors/unit-issuer-v1.json').read_text())
        raw = codec.encode(vector['input'])
        self.assertEqual(raw.hex(), vector['hex'])
        self.assertEqual(hashlib.sha256(raw).hexdigest(), vector['sha256'])
        self.assertEqual(codec.decode(raw), vector['input'])
        for n in range(len(raw)):
            with self.assertRaises(rc.InvalidContract): codec.decode(raw[:n])
        for bad in (raw+b'\0', b'Z'+raw[1:], raw[:168]+b'\xff'*4+raw[172:], raw[:24]+bytes(8)+raw[32:]):
            with self.assertRaises(rc.InvalidContract): codec.decode(bad)
        for key, bad in (('unit', '.service'), ('unit', '/bad.service'), ('unit', 'x'*248+'.service'),
                         ('label', 'é'), ('label', 'bad\0label'), ('cgroup', '/'), ('cgroup', '/a/../b'),
                         ('cgroup', '/a//b'), ('cgroup', '/a/'), ('pidfs_id', 0), ('pidfs_id', 2**64),
                         ('cgroup_id', True), ('boot', '00'*16), ('invocation', '00'*16)):
            value = deepcopy(vector['input']); value[key] = bad
            with self.assertRaises(rc.InvalidContract): codec.encode(value)
        value = deepcopy(vector['input'])
        value.update(unit='a'*247+'.service', cgroup='/'+'b'*1022, label='c'*511)
        maximum = codec.encode(value)
        self.assertEqual(len(maximum), 1969)
        self.assertEqual(codec.decode(maximum), value)


class IssuerMigrations(ContractCase):
    # Reuse populated system fixtures and preservation assertions, not the base suite.
    def source11(self, name):
        db = self.custody_source(name); cc.migrate_unit_custody(db, 'system'); return db

    def test_issuer_migration_each_boundary(self):
        db = self.source11('issuer11'); before, signature = rows(db), cc.signature(db)
        for phase in ['locked']+['statement_'+str(n) for n in range(len(cc.extension_v11('system')))]+['version_set', 'validated']:
            def fault(where):
                if where == phase: raise RuntimeError(where)
            with self.assertRaisesRegex(RuntimeError, phase): cc.migrate_unit_issuers(db, 'system', checkpoint=fault)
            cc.validate(db, 'system', 10)
            self.assertEqual(rows(db), before); self.assertEqual(cc.signature(db), signature)
        def lost(where):
            if where == 'committed': raise RuntimeError('unknown acknowledgement')
        with self.assertRaisesRegex(RuntimeError, 'unknown'): cc.migrate_unit_issuers(db, 'system', checkpoint=lost)
        cc.validate(db, 'system', 11); self.assert_preserved(db, before)
        self.assertFalse(cc.migrate_unit_issuers(db, 'system'))
        self.assertEqual(db.execute('SELECT count(*) FROM unit_issuers').fetchone()[0], 0)
        fresh = self.connect('fresh11'); cc.bootstrap(fresh, 'system', 11)
        self.assertEqual(cc.signature(db), cc.signature(fresh))
        with self.assertRaises(cc.ContractError): cc.source('user', 11)

    def test_issuer_process_death_disk_and_foreign_schema(self):
        code = '''import os, sqlite3, sys
import catalogue_contracts as cc
db=sqlite3.connect(sys.argv[1]); cc.configure(db)
def fault(phase):
    if phase==sys.argv[2]: os._exit(75)
cc.migrate_unit_issuers(db,'system',checkpoint=fault)
raise RuntimeError('checkpoint missed')
'''
        for phase in ('statement_0', 'version_set', 'committed'):
            name = 'issuer-exit-'+phase; db = self.source11(name); before = rows(db)
            path = db.execute('PRAGMA database_list').fetchone()[2]; db.close(); self.connections.remove(db)
            self.assertEqual(subprocess.run([sys.executable, '-c', code, path, phase], cwd=ROOT/'tools', timeout=20).returncode, 75)
            db = self.connect(name); cc.validate(db, 'system', 11 if phase=='committed' else 10)
            self.assert_preserved(db, before)
            self.assertEqual(cc.migrate_unit_issuers(db, 'system'), phase!='committed')
        db = self.source11('issuer-full'); db.execute('VACUUM'); before = rows(db)
        db.execute('PRAGMA max_page_count='+str(db.execute('PRAGMA page_count').fetchone()[0]))
        with self.assertRaises(sqlite3.OperationalError): cc.migrate_unit_issuers(db, 'system')
        cc.validate(db, 'system', 10); self.assertEqual(rows(db), before)
        db.execute('PRAGMA max_page_count=2147483646'); db.execute('CREATE TABLE foreign_issuer(value)'); db.commit(); before = rows(db)
        with self.assertRaises(cc.ContractError): cc.migrate_unit_issuers(db, 'system')
        self.assertEqual(rows(db), before)


if __name__ == '__main__':
    suite = unittest.TestSuite()
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(IssuerCodec))
    suite.addTests(IssuerMigrations(name) for name in ('test_issuer_migration_each_boundary', 'test_issuer_process_death_disk_and_foreign_schema'))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(not result.wasSuccessful())
