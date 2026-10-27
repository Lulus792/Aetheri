"""Host orchestration guards. No guest, installer or physical disk is executed."""
import json
import sqlite3
from pathlib import Path
import tempfile
import subprocess
import unittest
from unittest.mock import patch

import provision_windows as provision
import run_vm as vm
import test_linux_policy as policy
import p0_services as services


class VmTools(unittest.TestCase):
    def test_projection_controller_is_separate_from_broker_and_cannot_widen_slot(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": [], "projection_unit": "-"}
        with patch.object(services,"save"), patch.object(services,"run",return_value="") as command, \
             patch.object(services,"publish_projection_slot") as slot, patch.object(services,"publish_broker_slot") as broker:
            unit=services.start_unit(record,"user",probe="projection")
            slot.assert_called_once_with(record,unit)
            broker.assert_not_called()
            self.assertIn("--property=SELinuxContext=system_u:system_r:aetheri_user_t:s0",command.call_args.args)
            self.assertIn("--property=CapabilityBoundingSet=",command.call_args.args)
            self.assertFalse(any(value.startswith("--property=ReadWritePaths=") for value in command.call_args.args))
            self.assertEqual(command.call_args.args[-1],str(services.ROOT/"probe-projection"))
        record["projection_unit"]=unit
        self.assertEqual(services.validate_record(json.loads(json.dumps(record))),record)
        record["active_units"]={"user":unit}
        with self.assertRaisesRegex(RuntimeError,"cannot be an active authority"): services.validate_record(record)
        record.pop("active_units")
        with patch.object(services,"publish",side_effect=AssertionError("mutation")):
            for wrong in (unit.replace("-probe-projection-","-user-"),unit.replace("000000000001","000000000002"),"../unit"):
                with self.assertRaises(ValueError): services.publish_projection_slot(record,wrong)
        with patch.object(services,"publish"), patch.object(services,"flush_path"), patch.object(services.os,"replace"):
            services.publish_projection_slot(record,"-")
        self.assertEqual(record["projection_unit"],"-")
        with patch.object(services,"save",side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError): services.start_unit(record,"system",probe="projection")

    def test_publication_uses_one_writable_mount_ancestor_and_administrative_identity(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": [], "fixture": {key: "00000000000040008000000000000002" for key in services.FIXTURE_IDS}}
        record["fixture"]["artifact"] = "ab" * 32
        for action in ("store-observe", "store-publish", "activate"):
            with patch.object(services, "save"), patch.object(services, "run", return_value="") as command:
                unit = services.start_unit(record, "system", fixture=action)
                arguments = command.call_args.args
                self.assertEqual([a for a in arguments if a.startswith("--property=ReadWritePaths=")], ["--property=ReadWritePaths=" + str(services.ROOT)])
                self.assertIn("--property=ReadOnlyPaths=" + str(services.fixture_paths(record)["source"]), arguments)
                self.assertIn("--property=CapabilityBoundingSet=", arguments)
                self.assertEqual(arguments[-2:], ("system", "--fixture-" + action))
            self.assertEqual(services.validate_record(json.loads(json.dumps(record))), record)
            record["active_units"] = {"system": unit}
            with self.assertRaisesRegex(RuntimeError, "cannot be an active authority"): services.validate_record(record)
            record.pop("active_units")
        with patch.object(services, "save", side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError): services.start_unit(record, "user", fixture="store-publish")

    def test_publication_payload_locations_must_be_unique(self):
        slot, target = self.root / "slot", self.root / "store/object"
        slot.mkdir()
        target.mkdir(parents=True)
        staging = slot / "staging"
        staging.mkdir()
        paths = dict(staging=staging, container=slot / "publication", target=target)
        self.assertEqual(services.fixture_payload({}, paths), staging)
        publication = {"fixture_publication": {}}
        staging.rename(target / "payload")
        self.assertEqual(services.fixture_payload(publication, paths), target / "payload")
        staging.mkdir()
        with self.assertRaisesRegex(RuntimeError, "ambiguous/missing"): services.fixture_payload(publication, paths)

    def test_storage_administration_cannot_become_a_live_broker(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": []}
        with patch.object(services, "save"), patch.object(services, "run", return_value="") as command, patch.object(services, "publish_broker_slot") as slot:
            unit = services.start_unit(record, "user", storage="publish")
            self.assertIn("--service-type=exec", command.call_args.args)
            self.assertEqual(command.call_args.args[-2:], ("user", "--storage-publish"))
            slot.assert_not_called()
        self.assertEqual(services.validate_record(json.loads(json.dumps(record))), record)
        record["active_units"] = {"user": unit}
        with self.assertRaisesRegex(RuntimeError, "cannot be an active authority"):
            services.validate_record(record)
        with patch.object(services, "save", side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError):
                services.start_unit(record, "system", storage="publish")

    def test_allocated_private_data_is_preserved_by_empty_bootstrap_cleanup(self):
        with patch.object(services, "verified"), patch.object(services, "stop"), patch.object(services, "bus", side_effect=AssertionError("cleanup mutation")):
            with self.assertRaisesRegex(RuntimeError, "not an empty bootstrap"):
                services.cleanup({"pending": None, "storage": {}})

    def test_account_enrollment_receipt_roundtrip_and_role_boundary(self):
        record = {"token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002",
                  "epochs": {"system": "00000000000040008000000000000003", "user": "00000000000040008000000000000004"},
                  "pending": None, "units": []}
        with patch.object(services, "save"), patch.object(services, "run", return_value="") as command:
            unit = services.start_unit(record, "system", enroll=True)
            self.assertIn("--service-type=exec", command.call_args.args)
            self.assertEqual(command.call_args.args[-2:], ("system", "--enroll-account"))
        self.assertEqual(services.validate_record(json.loads(json.dumps(record))), record)
        record["active_units"] = {"system": unit}
        with self.assertRaisesRegex(RuntimeError, "cannot be an active authority"):
            services.validate_record(record)
        record.pop("active_units")
        record["units"] = [unit.replace("-system-enroll-", "-user-enroll-")]
        with self.assertRaisesRegex(RuntimeError, "foreign unit"):
            services.validate_record(record)
        with patch.object(services, "save", side_effect=AssertionError("mutation")):
            with self.assertRaises(ValueError):
                services.start_unit(record, "user", enroll=True)

    def test_empty_bootstrap_cleanup_preserves_other_account_lifetimes(self):
        owner = "00000000000040008000000000000002"
        with sqlite3.connect(":memory:") as db:
            db.execute("CREATE TABLE scopes(scope_id BLOB,kind TEXT,os_uid INTEGER,state TEXT,revision INTEGER)")
            self.assertTrue(services.bootstrap_scopes(db, owner))
            db.execute("INSERT INTO scopes VALUES(?,'account',1000,'active',1)", (bytes.fromhex(owner),))
            self.assertTrue(services.bootstrap_scopes(db, owner))
            for column, value in (("scope_id", bytes(16)), ("kind", "machine"), ("os_uid", 1001), ("state", "retired"), ("revision", 2)):
                db.execute("SAVEPOINT changed_account")
                db.execute("UPDATE scopes SET " + column + "=?", (value,))
                self.assertFalse(services.bootstrap_scopes(db, owner))
                db.execute("ROLLBACK TO changed_account")
                db.execute("RELEASE changed_account")
            db.execute("INSERT INTO scopes VALUES(zeroblob(16),'account',1001,'active',1)")
            self.assertFalse(services.bootstrap_scopes(db, owner))

    def test_authority_pending_dispatch_blocks_start_stop_and_cleanup(self):
        record = {"pending": "unacknowledged.service"}
        with patch.object(services, "run", side_effect=AssertionError("supervisor mutation")), \
             patch.object(services, "verified", side_effect=AssertionError("queried installation")):
            for action in (services.start, services.stop, services.cleanup):
                with self.assertRaisesRegex(RuntimeError, "unacknowledged dispatch"):
                    action(record)

    def test_authority_dispatch_timeout_preserves_pending_identity(self):
        record = {"pending": None, "units": [], "token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002"}
        with patch.object(services, "save"), patch.object(services, "publish_broker_slot"), patch.object(services, "run", side_effect=subprocess.TimeoutExpired("systemd-run", 40)) as command:
            with self.assertRaises(subprocess.TimeoutExpired):
                services.start_unit(record, "user")
            self.assertEqual(record["pending"], record["units"][0])
            self.assertEqual(command.call_count, 1)

    def test_authority_stop_never_accepts_a_remaining_process(self):
        observations = ["ActiveState=active\nMainPID=123\nControlGroup=/owned\nLoadState=loaded", "", "ActiveState=failed\nMainPID=123\nControlGroup=/owned"]
        with patch.object(services, "run", side_effect=observations), patch.object(services, "save") as saved:
            with self.assertRaisesRegex(RuntimeError, "not drained"):
                services.stop({"pending": None, "units": ["owned.service"]})
            saved.assert_not_called()

    def test_authority_existing_name_with_wrong_pid_is_not_adopted(self):
        record = {"pending": None, "active_units": {"system": "owned.service"}}
        with patch.object(services, "verified"), patch.object(services, "bus", side_effect=["b true", "u 456"]), \
             patch.object(services, "run", return_value="123"), patch.object(services, "start_unit") as dispatch:
            with self.assertRaisesRegex(RuntimeError, "identity is unresolved"):
                services.start(record)
            dispatch.assert_not_called()

    def test_authority_stop_preserves_a_pending_supervisor_job(self):
        observations = ["ActiveState=inactive\nMainPID=0\nControlGroup=\nLoadState=loaded", "", "ActiveState=inactive\nMainPID=0\nControlGroup="]
        jobs = {"type": "a(usssoo)", "data": [[[1, "owned.service", "start", "waiting", "/job/1", "/unit/1"]]]}
        with patch.object(services, "run", side_effect=observations), patch.object(services, "bus", return_value=json.dumps(jobs)), patch.object(services, "save") as saved:
            with self.assertRaisesRegex(RuntimeError, "supervisor job"):
                services.stop({"pending": None, "units": ["owned.service"]})
            saved.assert_not_called()

    def test_authority_failed_dispatch_with_a_job_stays_pending(self):
        record = {"pending": None, "units": [], "token": "00000000-0000-4000-8000-000000000001", "owner": "00000000000040008000000000000002"}
        failed = subprocess.CalledProcessError(1, "systemd-run", stderr="failed")
        with patch.object(services, "save"), patch.object(services, "run", side_effect=[failed, "ActiveState=failed\nMainPID=0\nControlGroup="]), \
             patch.object(services, "unit_has_job", return_value=True), patch.object(services, "publish_broker_slot"):
            with self.assertRaises(subprocess.CalledProcessError):
                services.start_unit(record, "user")
            self.assertEqual(record["pending"], record["units"][0])

    def test_authority_stopped_unit_with_pending_job_is_not_replaced(self):
        record = {"pending": None, "active_units": {"system": "owned.service"}}
        with patch.object(services, "verified"), patch.object(services, "bus", return_value="b false"), \
             patch.object(services, "run", return_value="ActiveState=inactive\nMainPID=0\nControlGroup="), \
             patch.object(services, "unit_has_job", return_value=True), patch.object(services, "start_unit") as dispatched:
            with self.assertRaisesRegex(RuntimeError, "still live/unknown"):
                services.start(record)
            dispatched.assert_not_called()

    def test_policy_cleanup_never_infers_exit_from_unacknowledged_dispatch(self):
        with patch.object(policy, "run", side_effect=AssertionError("queried or mutated supervisor")) as invoked:
            with self.assertRaisesRegex(RuntimeError, "unacknowledged dispatch"):
                policy.cleanup_owned({"dispatch_in_progress": "uncertain.service"})
            invoked.assert_not_called()

    def setUp(self):
        output = vm.ROOT / "out"
        output.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="vm-tools-", dir=output)
        self.root = Path(self.temp.name)
        if self.root.resolve().parent != output.resolve():
            raise ValueError("test cleanup outside workspace")
        self.addCleanup(self.temp.cleanup)
        self.guest = self.root / "vm/p0"
        self.guest.mkdir(parents=True)
        for name, value in (("ROOT", self.root), ("VM", self.guest), ("TOOLS", self.root / "tools")):
            guard = patch.object(vm, name, value)
            guard.start()
            self.addCleanup(guard.stop)
        self.registration = {"uuid": "00000000-0000-4000-8000-000000000001",
                             "disk_bytes": 48 * 1024**3, "disk_serial": "AETHERI_P0_ONLY"}
        (self.guest / "config.json").write_text(json.dumps(self.registration), encoding="utf-8")

    def test_wrong_qmp_vm_is_not_accepted(self):
        with patch.object(vm, "qmp", return_value={"UUID": "ffffffff-ffff-4fff-8fff-ffffffffffff"}):
            with self.assertRaises(ValueError):
                vm.running()

    def test_correct_qmp_identity_and_observation_timeout(self):
        with patch.object(vm, "qmp", return_value={"UUID": self.registration["uuid"]}):
            self.assertTrue(vm.running())
        with patch.object(vm, "qmp", side_effect=TimeoutError):
            self.assertFalse(vm.running())  # Does not prove process exit; start checks PID too.

    def test_live_or_unresolved_start_never_dispatches_again(self):
        for record in ({"pid": 12345}, {"state": "starting"}):
            (self.guest / "process.json").write_text(json.dumps(record), encoding="utf-8")
            with patch.object(vm, "running", return_value=False), \
                 patch.object(vm, "pid_alive", return_value=True), \
                 patch.object(vm, "prepare", side_effect=AssertionError("mutated guest")):
                with self.assertRaises(ValueError):
                    vm.start(False)

    def test_running_guest_is_not_reinstalled(self):
        with patch.object(vm, "running", return_value=True), \
             patch.object(vm, "prepare", side_effect=AssertionError("mutated guest")):
            with self.assertRaises(ValueError):
                vm.start(True, retry=True)

    def test_repeat_install_needs_explicit_flag_even_after_process_exit(self):
        (self.guest / "process.json").write_text('{"pid":12345}', encoding="utf-8")
        with patch.object(vm, "running", return_value=False), \
             patch.object(vm, "pid_alive", return_value=False), \
             patch.object(vm, "prepare", side_effect=AssertionError("mutated guest")):
            with self.assertRaises(ValueError):
                vm.start(True)

    def test_unregistered_disk_is_not_adopted(self):
        (self.guest / "config.json").unlink()
        disk = self.guest / "system.qcow2"
        disk.write_bytes(b"existing data")
        with self.assertRaises(ValueError):
            vm.prepare()
        self.assertEqual(disk.read_bytes(), b"existing data")

    def test_firmware_and_disk_are_local_and_network_can_be_restricted(self):
        command = vm.command(False, offline=True)
        self.assertIn("host,vmx=off,svm=off", command)
        self.assertIn("8192M", command)
        net = command[command.index("-netdev") + 1]
        self.assertIn("restrict=on", net)
        self.assertIn("127.0.0.1", net)
        self.assertNotIn("-fsdev", command)
        self.assertNotIn("-virtfs", command)
        drives = [command[i + 1] for i, item in enumerate(command) if item == "-drive"]
        self.assertEqual(len(drives), 3)
        self.assertTrue(all(str(self.root) in drive for drive in drives))
        self.assertTrue(any("readonly=on" in drive and "pflash" in drive for drive in drives))
        self.assertTrue(any("format=qcow2" in drive for drive in drives))
        monitor = command[command.index("-qmp") + 1]
        self.assertTrue(monitor.startswith("unix:"))
        self.assertNotIn("tcp:", monitor)

    def test_offline_install_is_rejected_before_mutation(self):
        with patch.object(vm, "prepare", side_effect=AssertionError("mutated guest")):
            with self.assertRaises(ValueError):
                vm.start(True, offline=True)

    def test_atomic_state_preserves_previous_file_when_temp_already_exists(self):
        path = self.guest / "process.json"
        path.write_text("previous", encoding="utf-8")
        path.with_suffix(".json.tmp").write_text("interrupted", encoding="utf-8")
        with self.assertRaises(FileExistsError):
            vm.write_state(path, {"pid": 1})
        self.assertEqual(path.read_text(), "previous")

    def test_provision_reuses_equal_bytes_and_rejects_changed_existing_tool(self):
        source, target = self.root / "source", self.root / "target"
        source.mkdir()
        target.mkdir()
        (source / "tool").write_bytes(b"fixture")
        with patch.object(provision, "ROOT", self.root):
            receipt = provision.publish_files(source, target)
            self.assertEqual(provision.publish_files(source, target), receipt)
            (target / "tool").write_bytes(b"changed")
            with self.assertRaises(ValueError):
                provision.publish_files(source, target)
            self.assertEqual((target / "tool").read_bytes(), b"changed")


if __name__ == "__main__":
    unittest.main(verbosity=2)
