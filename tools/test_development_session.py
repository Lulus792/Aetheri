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
    def test_native_key_tap_requires_valid_chord_and_verified_carrier(self):
        import run_vm as host
        with patch.object(host,"running") as live,patch.object(host,"qmp") as dispatch:
            for chord in ((),('ctrl','ctrl'),('a','b','c','d','e'),('',),(None,)):
                with self.subTest(chord=chord),self.assertRaises(ValueError):host.tap_keys(*chord)
            live.assert_not_called();dispatch.assert_not_called()
            live.return_value=False
            with self.assertRaises(RuntimeError):host.tap_keys('ctrl','s')
            dispatch.assert_not_called()

    def test_unknown_native_key_dispatch_never_replays_a_press(self):
        import run_vm as host
        with patch.object(host,"running",return_value=True),patch.object(host,"qmp",side_effect=TimeoutError('lost acknowledgement')) as dispatch:
            with self.assertRaises(TimeoutError):host.tap_keys('ctrl','shift','s')
        self.assertEqual(dispatch.call_count,1)
        command,args=dispatch.call_args.args;self.assertEqual(command,'input-send-event')
        held=set();pressed=set()
        for event in args['events']:
            code=event['data']['key']['data']
            if event['data']['down']:
                self.assertNotIn(code,held);held.add(code);pressed.add(code)
            else:self.assertIn(code,held);held.remove(code)
        self.assertEqual(pressed,{'ctrl','shift','s'});self.assertFalse(held)

    def setUp(self):
        self.carrier = "0a174bc7-f17b-4799-9331-145ba3a5800c"
        self.record = dict(format=1, carrier=self.carrier, profile="release", token="a" * 32,
                           unit="aetheri-development-" + "a" * 32 + ".service", phase="running",
                           boot="7e6602b8-721b-4aa1-8b31-33cbb81d5c8c", invocation="b" * 32, pid=100)
        self.props = dict(InvocationID="b" * 32, ExecMainPID="100", MainPID="100",
                          RemainAfterExit="yes", Type="notify", User="aetheri")

    def test_foreign_versions_carriers_units_and_path_tokens_are_rejected(self):
        self.assertIs(session.validate(self.record, self.carrier), self.record)
        self.assertNotIn("launch_protocol", self.record)  # Historical v1 is not rewritten.
        self.assertNotIn("compose", self.record)
        current = {**self.record, "desktop": True, "launch_protocol": 2}
        self.assertIs(session.validate(current, self.carrier), current)
        new = {**self.record, "desktop": True, "launch_protocol": 3}
        self.assertIs(session.validate(new, self.carrier), new)
        self.assertEqual(current["launch_protocol"],2)
        changes = [dict(format=2), dict(carrier="foreign"), dict(profile="other"),
                   dict(token="../foreign"), dict(token="A" * 32), dict(unit="foreign.service"),
                   dict(phase="complete"), dict(boot="broken"), dict(invocation="not-an-invocation"),
                   dict(desktop=1), dict(desktop="true"), dict(launch_protocol=2),
                   dict(launch_protocol=True), dict(launch_protocol="2"),
                   dict(desktop=True, launch_protocol=0), dict(desktop=True, launch_protocol=4)]
        changes.extend((dict(compose=1),dict(compose="true"),dict(compose=None)))
        for change in changes:
            with self.subTest(change=change), self.assertRaises((RuntimeError, ValueError)):
                session.validate({**self.record, **change}, self.carrier)

    def test_compose_is_an_explicit_original_compositor_profile(self):
        record={**self.record,'compose':True}
        self.assertIs(session.validate(record,self.carrier),record)
        sock,peer=socket.socketpair()
        with sock,peer:
            # The test borrows one real descriptor; command construction owns none.
            args,fds=session.compositor_launch(Path('/selected-build'),sock,True)
            self.assertEqual(args[-3:],['--shell-fd',str(sock.fileno()),'--compose'])
            self.assertEqual(fds,(sock.fileno(),))
            self.assertIn('--drm',args)
            self.assertNotIn('--compose',session.compositor_launch(Path('/selected-build'),sock)[0])

    def test_nexaui_binding_is_explicit_and_closed_history_does_not_select_current_inputs(self):
        selected = dict(revision="a"*40, version="0.1.18", build="/home/aetheri/deps/nexaui-settings-release-02",
                        artifacts={"consumer-build/aetheri_model_view":"1"*64,"prefix/lib/libnexaui.so.0.1.18":"2"*64},
                        model={"src/settings/model.c":"3"*64,"src/settings/model.h":"4"*64},
                        metadata_sha256="5"*64, pin_sha256="6"*64)
        record = {**self.record, "desktop":True, "settings_ui":"nexaui", "settings_profile":selected,
                  "settings_binding":session.settings_fingerprint(selected)}
        self.assertIs(session.validate(record,self.carrier),record)
        for change in ({"settings_binding":"0"*64},{"desktop":False},{"settings_ui":"sdl"},
                       {"settings_profile":{**selected,"build":"/home/aetheri/deps/../foreign"}},
                       {"settings_profile":{**selected,"artifacts":{"../foreign":"7"*64}}}):
            with self.subTest(change=change),self.assertRaises(RuntimeError):
                session.validate({**record,**change},self.carrier)
        before=copy.deepcopy(record);record["phase"]="closed";before["phase"]="closed"
        with patch.object(session,"settings_selection",side_effect=AssertionError("current candidate read")), \
             patch.object(session,"run",side_effect=AssertionError("historical effect")):
            self.assertEqual(session.stop(record),before)
        self.assertEqual(record,before)

    def test_nexaui_cannot_start_without_original_binding_or_desktop(self):
        selected={"test":"controlled profile"}
        with patch.object(session.os,"getuid",return_value=1000,create=True),patch.object(session.os,"geteuid",return_value=1000,create=True), \
             patch.object(session,"settings_selection",return_value=selected), \
             patch.object(session.subprocess,"Popen",side_effect=AssertionError("premature process")):
            with self.assertRaisesRegex(RuntimeError,"original Settings selection"):
                session.supervise("release","a"*32,desktop=True,settings_ui="nexaui",settings_binding="0"*64)
            with self.assertRaisesRegex(RuntimeError,"selected desktop"):
                session.supervise("release","a"*32,settings_ui="nexaui")
        with patch.object(session,"free_seat",side_effect=AssertionError("unapproved seat effect")):
            with self.assertRaisesRegex(RuntimeError,"selected desktop"):
                session.start(None,self.carrier,"release",settings_ui="nexaui")

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
    def test_desktop_owner_keeps_one_app_and_orders_shutdown(self):
        with tempfile.TemporaryDirectory(prefix="desktop-owner-") as directory:
            root=Path(directory);build=root/"build";build.mkdir();token="e"*32
            base=root/token;base.mkdir(mode=0o700)
            for name in ("runtime","cache","documents"):(base/name).mkdir(mode=0o700)
            marker=root/"app-live";desktop_live=root/"desktop-live";started=root/"started";order=root/"order"
            common="from pathlib import Path\nimport os,signal,socket,sys,time\n"
            append="with Path("+repr(str(order))+").open('a') as f:f.write(ROLE+'\\n')\n"
            programs={
                "aetheri-settings":common+"ROLE='settings'\nmarker=Path("+repr(str(marker))+")\nmarker.write_text('active')\nwith Path("+repr(str(started))+").open('a') as f:f.write(str(os.getpid())+'\\n')\ndef stop(*args):\n marker.unlink()\n "+append.replace("\n","\n ").rstrip()+"\n sys.exit(0)\nsignal.signal(signal.SIGTERM,stop)\nprint('READY synthetic',flush=True)\nwhile True:time.sleep(1)\n",
                "aetheri-compositor":common+"ROLE='compositor'\ndef stop(*args):\n if Path("+repr(str(desktop_live))+").exists():sys.exit(9)\n "+append.replace("\n","\n ").rstrip()+"\n sys.exit(0)\nsignal.signal(signal.SIGTERM,stop)\nprint('READY synthetic',flush=True)\nwhile True:time.sleep(1)\n",
                "aetheri-desktop":common+"ROLE='desktop'\nmarker=Path("+repr(str(desktop_live))+")\nmarker.write_text('active')\ndef stop(*args):\n if Path("+repr(str(marker))+").exists():sys.exit(7)\n marker.unlink()\n "+append.replace("\n","\n ").rstrip()+"\n sys.exit(0)\nsignal.signal(signal.SIGTERM,stop)\ns=socket.socket(fileno=int(sys.argv[sys.argv.index('--launcher-fd')+1]))\nprint('READY synthetic',flush=True)\nprint('LAYOUT 1',flush=True)\ns.sendall(b'L3S')\nassert s.recv(8)==b'A3S'\ndeadline=time.monotonic()+5\nwhile not Path("+repr(str(marker))+").exists():\n assert time.monotonic()<deadline\n time.sleep(.01)\ns.sendall(b'L3S')\nassert s.recv(8)==b'A3S'\ns.sendall(b'Q30')\nwhile True:time.sleep(1)\n"}
            programs["aetheri-settings"]=programs["aetheri-settings"].replace("marker.write_text('active')\n","",1).replace(
                "signal.signal(signal.SIGTERM,stop)\n","signal.signal(signal.SIGTERM,stop)\nmarker.write_text('active')\n",1)
            for name,code in programs.items():
                path=build/name;path.write_text("#!/usr/bin/python3\n"+code);path.chmod(0o700)
            notify=socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM);notify.bind(str(root/"notify"));notify.settimeout(5)
            code="import sys\nfrom pathlib import Path\nsys.path.insert(0,"+repr(str(Path(__file__).parent))+")\nimport development_session as s\ns.USER_ROOT=Path("+repr(str(root))+")\ns.build_for=lambda p:Path("+repr(str(build))+")\nsys.exit(s.supervise('release',"+repr(token)+",True))\n"
            proc=None
            try:
                proc=subprocess.Popen([sys.executable,"-c",code],env={**os.environ,"NOTIFY_SOCKET":str(root/"notify")},
                                      stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
                self.assertIn(b"READY=1",notify.recv(4096))
                output,_=proc.communicate(timeout=8)
                self.assertEqual(proc.returncode,0,output.decode(errors="replace"))
                self.assertEqual(len(started.read_text().splitlines()),1)
                self.assertEqual(order.read_text().splitlines(),["settings","desktop","compositor"])
                self.assertFalse(marker.exists());self.assertFalse(desktop_live.exists())
            finally:
                notify.close()
                if proc and proc.poll() is None:proc.terminate();proc.communicate(timeout=8)

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
