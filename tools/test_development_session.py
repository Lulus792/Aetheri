"""Development-control contract guards; never dispatch a service or touch a VM."""
import copy
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import development_session as session


class DevelopmentContracts(unittest.TestCase):
    def setUp(self):
        self.carrier = "0a174bc7-f17b-4799-9331-145ba3a5800c"
        self.record = dict(format=1, carrier=self.carrier, profile="release", token="a" * 32,
                           unit="aetheri-development-" + "a" * 32 + ".service", phase="running",
                           boot="7e6602b8-721b-4aa1-8b31-33cbb81d5c8c", invocation="b" * 32, pid=100)
        self.props = dict(InvocationID="b" * 32, ExecMainPID="100", MainPID="100",
                          RemainAfterExit="yes", Type="notify", User="aetheri")

    def test_foreign_versions_carriers_units_and_path_tokens_are_rejected(self):
        self.assertIs(session.validate(self.record, self.carrier), self.record)
        changes = [dict(format=2), dict(carrier="foreign"), dict(profile="other"),
                   dict(token="../foreign"), dict(token="A" * 32), dict(unit="foreign.service"),
                   dict(phase="complete"), dict(boot="broken"), dict(invocation="not-an-invocation")]
        for change in changes:
            with self.subTest(change=change), self.assertRaises((RuntimeError, ValueError)):
                session.validate({**self.record, **change}, self.carrier)

    def test_pending_effect_never_permits_another_start_or_stop(self):
        for phase in ("preparing", "starting", "stopping", "exited"):
            record = {**self.record, "phase": phase}
            with patch.object(session, "run", side_effect=AssertionError("effect replay")), \
                    patch.object(session, "save", side_effect=AssertionError("state replacement")):
                with self.assertRaisesRegex(RuntimeError, "existing development intent"):
                    session.start(record, self.carrier, "debug")
                with self.assertRaisesRegex(RuntimeError, "manual reconciliation"):
                    session.stop(record)

    def test_closed_retry_is_historical_and_does_not_observe_a_later_unit(self):
        record = {**self.record, "phase": "closed"}; before = copy.deepcopy(record)
        with patch.object(session, "properties", side_effect=AssertionError("later unit")), \
                patch.object(session, "run", side_effect=AssertionError("effect")), \
                patch.object(session, "save", side_effect=AssertionError("history rewrite")):
            self.assertEqual(session.stop(record), before)
            self.assertEqual(session.observation(record), {"kind": "historical_closed"})
        self.assertEqual(record, before)

    def test_replaced_unit_process_or_configuration_cannot_be_stopped(self):
        changes = [dict(InvocationID="c" * 32), dict(ExecMainPID="101"), dict(MainPID="101"),
                   dict(RemainAfterExit="no"), dict(Type="simple"), dict(User="root")]
        with patch.object(session.Path, "read_text", return_value=self.record["boot"]):
            session.require_original(self.record, self.props)
            for change in changes:
                with self.subTest(change=change), patch.object(session, "properties", return_value={**self.props, **change}), \
                        patch.object(session, "run", side_effect=AssertionError("foreign effect")), \
                        patch.object(session, "save", side_effect=AssertionError("foreign mutation")):
                    with self.assertRaises(RuntimeError): session.stop(self.record)

    def test_boot_replacement_requires_reconciliation_before_any_effect(self):
        with patch.object(session.Path, "read_text", return_value="11111111-1111-4111-8111-111111111111"), \
                patch.object(session, "properties", return_value=self.props), \
                patch.object(session, "run", side_effect=AssertionError("cross-boot effect")), \
                patch.object(session, "save", side_effect=AssertionError("cross-boot mutation")):
            with self.assertRaisesRegex(RuntimeError, "different boot"): session.stop(self.record)
            self.assertEqual(session.observation(self.record)["kind"], "recovery_required")


@unittest.skipUnless(sys.platform == "linux", "requires Linux file/notification/process primitives")
class DevelopmentFiles(unittest.TestCase):
    def test_interrupted_temporary_write_preserves_both_files(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(session, "CONTROL", Path(directory)):
            root = Path(directory); (root / "active.json").write_bytes(b"original")
            (root / "active.next").write_bytes(b"uncertain")
            with self.assertRaises(FileExistsError): session.save({"phase": "starting"})
            self.assertEqual((root / "active.json").read_bytes(), b"original")
            self.assertEqual((root / "active.next").read_bytes(), b"uncertain")

    def test_lost_directory_flush_does_not_restore_an_old_record(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(session, "CONTROL", Path(directory)):
            root = Path(directory); (root / "active.json").write_bytes(b"original")
            with patch.object(session.os, "fsync", side_effect=[None, OSError("unknown flush acknowledgement")]):
                with self.assertRaises(OSError): session.save({"phase": "starting"})
            self.assertEqual(json.loads((root / "active.json").read_text()), {"phase": "starting"})


@unittest.skipUnless(sys.platform == "linux" and os.getuid() == 1000, "requires the unprivileged carrier account")
class SupervisorProcesses(unittest.TestCase):
    def fixture(self, app_exit, compositor_exit, layout=True):
        with tempfile.TemporaryDirectory(prefix="aetheri-supervisor-") as directory:
            root = Path(directory); build = root / "build"; build.mkdir()
            token = "d" * 32; base = root / token; base.mkdir(mode=0o700)
            for name in ("runtime", "cache"): (base / name).mkdir(mode=0o700)
            programs = {
                "aetheri-settings": "import time\nprint('READY test-only',flush=True)\n"+("print('LAYOUT 1',flush=True)\n" if layout else "")+"time.sleep(.1)\nraise SystemExit("+str(app_exit)+")\n",
                "aetheri-compositor": "import signal,time,sys\ndef stop(*args):\n for _ in range(40):print('DRAIN '+('x'*500),flush=True)\n print('TAIL_SENTINEL',flush=True)\n sys.exit("+str(compositor_exit)+")\nsignal.signal(signal.SIGTERM,stop)\nprint('READY test-only',flush=True)\nwhile True:time.sleep(1)\n"}
            for name, code in programs.items():
                path = build / name; path.write_text("#!/usr/bin/python3\n" + code); path.chmod(0o700)
            notify = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM); notify.bind(str(root / "notify")); notify.settimeout(5)
            code = "import sys\nfrom pathlib import Path\nsys.path.insert(0,"+repr(str(Path(__file__).parent))+ ")\nimport development_session as s\ns.USER_ROOT=Path("+repr(str(root))+")\ns.build_for=lambda profile:Path("+repr(str(build))+")\nsys.exit(s.supervise('release',"+repr(token)+"))\n"
            try:
                proc = subprocess.Popen([sys.executable, "-c", code], env={**os.environ, "NOTIFY_SOCKET": str(root / "notify")},
                                        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if layout: self.assertIn(b"READY=1", notify.recv(4096))
                output, _ = proc.communicate(timeout=8)
                if not layout:
                    notify.settimeout(.05)
                    with self.assertRaises(socket.timeout): notify.recv(4096)
                self.assertEqual(proc.returncode, 1, output.decode(errors="replace"))
                self.assertIn(b"TAIL_SENTINEL", output)
                self.assertIn(b"SESSION_END", output)
                self.assertNotIn(b"SUPERVISOR_ERROR", output)
            finally:
                notify.close()

    def test_unsolicited_app_failure_ends_the_supervised_session(self):
        self.fixture(app_exit=7, compositor_exit=0)

    def test_planned_compositor_stop_cannot_hide_its_cleanup_failure(self):
        self.fixture(app_exit=0, compositor_exit=7)

    def test_ready_without_a_submitted_frame_is_not_a_ready_session(self):
        self.fixture(app_exit=0, compositor_exit=0, layout=False)


if __name__ == "__main__":
    unittest.main(verbosity=2)
