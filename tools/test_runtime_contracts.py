"""Independent frozen byte vectors and adversarial P0-format input tests."""
from copy import deepcopy
from pathlib import Path
import unittest

import runtime_contracts as rc


ROOT = Path(__file__).resolve().parents[1]
VECTORS = rc.load_json((ROOT / "spec/vectors/runtime-v1.json").read_bytes())


class RuntimeContracts(unittest.TestCase):
    def test_process_identity_vector_and_bounds(self):
        vector=rc.load_json((ROOT/'spec/vectors/process-identity-v1.json').read_bytes())
        data=rc.process_identity(vector['input'])
        self.assertEqual(data.hex(),vector['hex']); self.assertEqual(rc.digest(data),vector['sha256'])
        self.assertEqual(rc.read_process_identity(data),vector['input'])
        for key,value in (('boot','00'*16),('boot','ff'),('pidfd_id',0),('pidfd_id',2**64),('pidfd_id',True)):
            row=deepcopy(vector['input']); row[key]=value; self.rejects(rc.process_identity,row)
        for bad in (data[:n] for n in range(32)): self.rejects(rc.read_process_identity,bad)
        for bad in (data+b'\0',b'Z'+data[1:],data[:8]+b'\0'*16+data[24:],data[:24]+b'\0'*8): self.rejects(rc.read_process_identity,bad)

    def test_run_closure_vectors_and_bounds(self):
        vectors=rc.load_json((ROOT/'spec/vectors/run-closure-v1.json').read_bytes())
        for vector in vectors['receipts']+[vectors['end']]:
            encoder=rc.run_receipt if vector['kind']=='receipt' else rc.run_end
            data=encoder(vector['input'])
            self.assertEqual(data.hex(),vector['hex']); self.assertEqual(rc.digest(data),vector['sha256'])
            if vector['kind']=='receipt': self.assertEqual(rc.read_run_receipt(data),vector['input'])
        for key,value in (('kind','other'),('revision',0),('revision',2**63),('unix_ms',-1),('unix_ms',True),('invocation','00'*16),('profile_digest','ff')):
            row=deepcopy(vectors['receipts'][0]['input']); row[key]=value; self.rejects(rc.run_receipt,row)
        for key,value in (('pid',0),('pid',2**31),('cgroup_id',0),('cgroup_id',2**64),('group_removed',1),('unit_failed',0),('boot','00'*16)):
            row=deepcopy(vectors['end']['input']); row[key]=value; self.rejects(rc.run_end,row)

    def test_execution_completion_vector(self):
        value=rc.load_json((ROOT/'spec/vectors/execution-completion-v1.json').read_bytes())
        encoded=rc.execution_completion(value['input'])
        self.assertEqual(len(encoded),200); self.assertEqual(encoded.hex(),value['hex']); self.assertEqual(rc.digest(encoded),value['sha256'])
        for key,bad in (('revision',0),('revision',2**63),('completed_unix_ms',-1),('invocation','00'*16),('system_runtime','00'*16),('observation_digest','00')):
            wrong=deepcopy(value['input']); wrong[key]=bad; self.rejects(rc.execution_completion,wrong)
    def test_image_observation_vector(self):
        v=rc.load_json((ROOT/'spec/vectors/image-v1.json').read_bytes())
        encoded=rc.image_record(v['input'])
        self.assertEqual(len(encoded),112); self.assertEqual(encoded.hex(),v['hex']); self.assertEqual(rc.digest(encoded),v['sha256'])
        for key,value in (('inode',0),('size',63),('size',16*1024*1024+1),('uid',1000),('gid',1000),('mode',0o555),
                          ('birth_seconds',-1),('birth_ns',1000000000),('mtime_ns',-1),('ctime_ns',1000000000),('digest','00'),('inode',True)):
            bad=deepcopy(v['input']); bad[key]=value; self.rejects(rc.image_record,bad)
    def example(self, kind):
        return deepcopy(next(v["input"] for v in VECTORS if v["kind"] == kind))

    def rejects(self, encoder, value):
        with self.assertRaises(rc.InvalidContract):
            encoder(value)

    def test_frozen_vectors(self):
        self.assertEqual({v["kind"] for v in VECTORS}, {"fixture", "owner", "intent"})
        for vector in VECTORS:
            with self.subTest(kind=vector["kind"]):
                actual = rc.ENCODERS[vector["kind"]](vector["input"])
                self.assertEqual(actual.hex(), vector["hex"])
                self.assertEqual(rc.digest(actual), vector["sha256"])

    def test_frozen_execution_scaffold(self):
        vector=rc.load_json((ROOT/"spec/vectors/scaffold-v1.json").read_bytes())
        encoded=rc.execution_views(vector["input"])
        self.assertEqual(len(encoded),184)
        self.assertEqual(encoded.hex(),vector["hex"])
        self.assertEqual(rc.digest(encoded),vector["sha256"])
        for change in (lambda x:x.update(future=1),lambda x:x.update(profile_sha256="00"),
                       lambda x:x["objects"].pop(),lambda x:x["objects"].append(x["objects"][0]),
                       lambda x:x["objects"][0].update(inode=0),lambda x:x["objects"][1].update(birth_nanoseconds=1000000000),
                       lambda x:x["objects"][2].update(filesystem_id="GG"*8),lambda x:x["objects"][3].update(birth_seconds=True)):
            value=deepcopy(vector["input"]); change(value); self.rejects(rc.execution_views,value)

    def test_input_order_does_not_change_identity(self):
        for kind, key in (("fixture", "files"), ("intent", "reads"), ("intent", "effects")):
            a = self.example(kind)
            b = dict(reversed(list(deepcopy(a).items())))
            b[key].reverse()
            self.assertEqual(rc.ENCODERS[kind](a), rc.ENCODERS[kind](b))

    def test_unknown_and_missing_fields(self):
        for kind, encoder in rc.ENCODERS.items():
            value = self.example(kind)
            value["future"] = True
            self.rejects(encoder, value)
            del value["future"]
            del value[next(iter(value))]
            self.rejects(encoder, value)

    def test_identifier_and_integer_representations(self):
        for bad in ("0" * 32, "1" * 31, "a" * 31 + "G", "AA" * 16, "1-" * 16):
            value = self.example("owner")
            value["data"] = bad
            self.rejects(rc.owner, value)
        for bad in (True, -1, 2**63, 1.5, "1"):
            value = self.example("intent")
            value["expires"] = bad
            self.rejects(rc.intent, value)

    def test_payload_bytes_and_inventory(self):
        value = self.example("fixture")
        payload = {"bin/probe": b"synthetic-not-an-executable\n", "share/message": b"hello\n"}
        artifact = rc.digest(rc.fixture(value))
        self.assertTrue(rc.verify_payload(value, artifact, payload))
        for changed in ({**payload, "share/message": b"jello\n"},
                        {**payload, "extra": b""}, {"bin/probe": payload["bin/probe"]}):
            self.rejects(lambda p: rc.verify_payload(value, artifact, p), changed)
        changed_manifest = deepcopy(value)
        changed_manifest["files"][1]["sha256"] = rc.digest(b"jello\n")
        with self.assertRaises(rc.InvalidContract):
            rc.verify_payload(changed_manifest, artifact, {**payload, "share/message": b"jello\n"})

    def test_unsafe_paths_and_file_ancestors(self):
        for path in ("/bin/probe", "../probe", "a/../b", "a//b", "a/./b", "a/", "C:/probe",
                     "a\\b", "a\x00b", "a\nb", "a/" + "b" * 65):
            value = self.example("fixture")
            value["files"][0]["path"] = path
            self.rejects(rc.fixture, value)
        value = self.example("fixture")
        template = value["files"][1]
        value["actions"] = []
        value["files"] = [{**template, "path": p} for p in ("a", "a-x", "a/b")]
        self.rejects(rc.fixture, value)

    def test_duplicate_or_unsupported_manifest_entries(self):
        for change in (lambda v: v["files"].append(v["files"][0]),
                       lambda v: v["actions"].append(v["actions"][0]),
                       lambda v: v.update(dependencies=["11" * 32] * 2),
                       lambda v: v.update(target="aarch64-linux-gnu"),
                       lambda v: v["files"][0].update(mode=0o4755),
                       lambda v: v["files"][0].update(symlink="outside"),
                       lambda v: v["actions"][0].update(executable="share/message"),
                       lambda v: v["actions"][0].update(arguments=["\x1b[31m"])):
            value = self.example("fixture")
            change(value)
            self.rejects(rc.fixture, value)

    def test_manifest_bounds(self):
        for change in (lambda v: v.update(files=v["files"] * 17),
                       lambda v: v["files"][0].update(size=16777217),
                       lambda v: v["actions"][0].update(arguments=[""] * 9),
                       lambda v: v.update(version="x" * 129)):
            value = self.example("fixture")
            change(value)
            self.rejects(rc.fixture, value)

    def test_owner_exact_binding_and_length(self):
        expected = self.example("owner")
        data = rc.owner(expected)
        self.assertEqual(len(data), 104)
        self.assertTrue(rc.check_owner(data, expected))
        for key in rc.OWNER_FIELDS.split():
            changed = {**expected, key: "ff" * 16}
            with self.assertRaises(rc.InvalidContract):
                rc.check_owner(data, changed)
        for invalid in (data[:-1], data + b"\0", b"AETHOWN2" + data[8:]):
            with self.assertRaises(rc.InvalidContract):
                rc.check_owner(invalid, expected)

    def test_each_bound_intent_field_changes_digest(self):
        value = self.example("intent")
        before = rc.digest(rc.intent(value))
        changes = {"plan": "11" * 16, "authority": "12" * 16, "actor": "13" * 16,
                   "scope": "14" * 16, "component": "aetheri.other", "install": "15" * 16,
                   "data": "16" * 16, "retention": "keep_data", "boot": "17" * 16,
                   "expires": value["expires"] + 1}
        for key, changed in changes.items():
            self.assertNotEqual(before, rc.digest(rc.intent({**value, key: changed})))
        for key in ("revision", "ref", "epoch"):
            changed = deepcopy(value)
            changed["reads"][0][key] = 2 if key == "revision" else "18" * 16
            self.assertNotEqual(before, rc.digest(rc.intent(changed)))
        changed = deepcopy(value)
        changed["effects"][0]["reason"] = "different"
        self.assertNotEqual(before, rc.digest(rc.intent(changed)))
        changed = deepcopy(value)
        changed["effects"][0]["number"], changed["effects"][1]["number"] = 1, 0
        self.assertNotEqual(before, rc.digest(rc.intent(changed)))

    def test_reinstall_is_a_different_target(self):
        old = self.example("intent")
        new = {**old, "install": "ab" * 16, "data": "cd" * 16}
        self.assertNotEqual(rc.digest(rc.intent(old)), rc.digest(rc.intent(new)))

    def test_other_intent_kinds_and_optional_bindings(self):
        base = self.example("intent")
        cases = [
            {**base, "kind": "install", "install": None, "data": None,
             "artifact": "11" * 32, "retention": "none"},
            {**base, "kind": "activate", "artifact": "11" * 32, "retention": "none"},
            {**base, "kind": "purge"},
            {**base, "kind": "launch", "generation": "11" * 16,
             "action": "run", "retention": "none"},
            {**base, "kind": "revoke", "grant": "12" * 16, "retention": "none"},
        ]
        digests = {rc.digest(rc.intent(value)) for value in cases}
        self.assertEqual(len(digests), len(cases))
        for value, key, changed in ((cases[0], "artifact", "22" * 32),
                                    (cases[3], "generation", "23" * 16),
                                    (cases[3], "action", "other"),
                                    (cases[4], "grant", "24" * 16)):
            self.assertNotEqual(rc.digest(rc.intent(value)),
                                rc.digest(rc.intent({**value, key: changed})))

    def test_intent_shapes_and_duplicate_reads(self):
        for change in (lambda v: v.update(kind="unknown"), lambda v: v.update(kind=[]),
                       lambda v: v.update(retention="none"), lambda v: v.update(action="run"),
                       lambda v: v.update(grant="11" * 16), lambda v: v.update(install=None),
                       lambda v: v["reads"].append(v["reads"][0]),
                       lambda v: v["reads"][0].update(revision=0),
                       lambda v: v["effects"][0].update(number=5),
                       lambda v: v["effects"][1].update(number=0),
                       lambda v: v["effects"][0].update(disposition="erase-everything")):
            value = self.example("intent")
            change(value)
            self.rejects(rc.intent, value)

    def test_json_duplicate_keys_constants_encoding_and_depth(self):
        for data in (b'{"a":1,"a":2}', b'{"a":{"b":1,"b":2}}', b'NaN', b'Infinity',
                     b'\xff', b'1' * 5000, b'[' * 2000 + b'0' + b']' * 2000,
                     b' ' * (rc.MAX_RECORD + 1)):
            self.rejects(rc.load_json, data)
        value = self.example("fixture")
        value["version"] = "\ud800"
        self.rejects(rc.fixture, value)


if __name__ == "__main__":
    unittest.main(verbosity=2)
