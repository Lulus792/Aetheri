"""Real private PTYs, terminal-generated signals and original process observations."""
import errno
import json
import os
from pathlib import Path
import pty
import re
import select
import signal
import subprocess
import sys
import tempfile
import termios
import time
import unittest

SHELL = str(Path(sys.argv.pop(1)).resolve())
FAILURE = str(Path(sys.argv.pop(1)).resolve())
BUILD = Path(SHELL).parent
ROOT = Path(tempfile.mkdtemp(prefix="shell-interactive-", dir=BUILD))


class Terminal:
    def __init__(self, name, binary=SHELL, args=("-i",)):
        self.root = ROOT / name
        self.root.mkdir()
        self.data = bytearray()
        self.cursor = 0
        self.status = None
        self.members = {}
        self.pid, self.master = pty.fork()
        if not self.pid:
            os.chdir(self.root)
            os.execv(binary, [binary, *args])
        os.set_blocking(self.master, False)
        self.pin = os.pidfd_open(self.pid)

    def enroll(self):
        # The original direct shell child is still unreaped here, anchoring its
        # session number. After reaping it, never rediscover by a stale SID.
        assert self.status is None
        for p in Path("/proc").iterdir():
            if p.name.isdecimal():
                fd = None
                try:
                    if os.getsid(int(p.name)) != self.pid:
                        continue
                    fd = os.pidfd_open(int(p.name))
                    if os.getsid(int(p.name)) != self.pid:
                        os.close(fd)
                        continue
                    key = (int(p.name), os.fstat(fd).st_ino)
                    if key in self.members:
                        os.close(fd)
                    else:
                        self.members[key] = fd
                except (ProcessLookupError, PermissionError, FileNotFoundError):
                    if fd is not None:
                        os.close(fd)
                    continue

    def pump(self, timeout=.05):
        if select.select([self.master], [], [], timeout)[0]:
            try:
                value = os.read(self.master, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                value = b""
            self.data.extend(value)
        if self.status is None:
            self.enroll()
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                self.status = os.waitstatus_to_exitcode(status)

    def expect(self, pattern, timeout=5):
        deadline = time.monotonic() + timeout
        while True:
            match = re.search(pattern, bytes(self.data[self.cursor:]))
            if match:
                self.cursor += match.end()
                return match
            if time.monotonic() >= deadline or self.status is not None:
                raise AssertionError((pattern, self.status, bytes(self.data[-6000:])))
            self.pump()

    def send(self, value):
        pending = memoryview(value.encode() if isinstance(value, str) else value)
        deadline = time.monotonic() + 5
        while pending:
            try:
                n = os.write(self.master, pending)
                assert n > 0
                pending = pending[n:]
            except BlockingIOError:
                assert time.monotonic() < deadline, "PTY input did not become writable"
                select.select([], [self.master], [], .02)

    def foreground(self):
        deadline = time.monotonic() + 3
        while os.tcgetpgrp(self.master) == self.pid:
            assert self.status is None and time.monotonic() < deadline, "original job did not receive foreground"
            self.pump(.01)
        return os.tcgetpgrp(self.master)

    def command(self, value):
        self.send(value + "\n")
        self.expect(rb"aetheri> ")

    def wait(self, expected, timeout=5):
        deadline = time.monotonic() + timeout
        while self.status is None and time.monotonic() < deadline:
            self.pump()
        if self.status != expected:
            raise AssertionError((expected, self.status, bytes(self.data[-6000:])))
        # Exit can precede consumption of the final stdout/LSan bytes. Reaping
        # the process is not an acknowledgement that its PTY stream was drained.
        while True:
            try:
                value = os.read(self.master, 65536)
            except BlockingIOError:
                break
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                break
            if not value:
                break
            self.data.extend(value)
            assert len(self.data) < 2**20, "fixture output exceeded its budget"

    def close(self):
        # Exact fresh private-PTY session only. Failure cleanup never selects a
        # production unit/run or another user's terminal; retain the transcript.
        if self.status is None:
            self.enroll()
        for fd in self.members.values():
            try:
                signal.pidfd_send_signal(fd, signal.SIGKILL)
            except ProcessLookupError:
                pass
            self.assert_ended(fd)
            os.close(fd)
        if self.status is None:
            _, status = os.waitpid(self.pid, 0)
            self.status = os.waitstatus_to_exitcode(status)
        os.close(self.master)
        os.close(self.pin)
        (self.root / "transcript.bin").write_bytes(self.data)
        (self.root / "result.json").write_text(json.dumps({"pid": self.pid, "exit": self.status}))
        assert b"AddressSanitizer" not in self.data and b"runtime error:" not in self.data, bytes(self.data[-4000:])

    @staticmethod
    def assert_ended(fd):
        assert select.select([fd], [], [], 2)[0], "exact owned PTY fixture did not end"


HELPER = r'''import json,os,signal,sys,termios,time
from pathlib import Path
mode,tag=sys.argv[1:3]
fd=os.open('/dev/tty',os.O_RDWR)
context={'pid':os.getpid(),'group':os.getpgrp(),'foreground':os.tcgetpgrp(fd),'session':os.getsid(0),'blocked':list(signal.pthread_sigmask(signal.SIG_BLOCK,[]))}
Path(tag+'.json').write_text(json.dumps(context))
print('READY '+tag,file=sys.stderr,flush=True)
if mode=='probe':pass
elif mode=='read':
 data=os.read(0,4096);print('DATA '+data.decode().strip(),flush=True)
elif mode=='timer':
 time.sleep(.5);print('TIMER_DONE',flush=True)
elif mode=='modes':
 changed=termios.tcgetattr(fd);changed[3]&=~(termios.ICANON|termios.ECHO);termios.tcsetattr(fd,termios.TCSANOW,changed)
 os.kill(os.getpid(),signal.SIGTSTP)
 assert termios.tcgetattr(fd)[3]&(termios.ICANON|termios.ECHO)==0
 print('JOB_MODES_RESTORED',flush=True)
elif mode=='ignore':
 signal.signal(signal.SIGHUP,signal.SIG_IGN);print('HUP_IGNORED',flush=True)
 while True:signal.pause()
elif mode=='foreign':
 child=os.fork()
 if child==0:
  os.setpgid(0,0)
  while True:signal.pause()
 os.setpgid(child,child);os.tcsetpgrp(fd,child)
 print('FOREIGN_SELECTED',file=sys.stderr,flush=True)
 time.sleep(.15)
else:
 while True:signal.pause()
'''


class InteractiveShell(unittest.TestCase):
    def setUp(self):
        self.t = Terminal(self._testMethodName)
        (self.t.root / "helper.py").write_text(HELPER)
        self.t.expect(rb"aetheri> ")

    def tearDown(self):
        self.t.close()

    def helper(self, mode, tag="job", pipeline=None):
        value = f"{sys.executable} helper.py {mode} {tag}"
        if pipeline:
            value += " | " + f"{sys.executable} helper.py {pipeline} other"
        self.t.send(value + "\n")
        if pipeline:
            seen = set()
            while seen != {tag, "other"}:
                match = self.t.expect(rb"READY ([a-z0-9]+)\r\n")
                seen.add(match.group(1).decode())
        else:
            self.t.expect(("READY " + tag).encode())

    def context(self, tag="job"):
        return json.loads((self.t.root / (tag + ".json")).read_text())

    def stop(self):
        self.t.send(b"\x1a")
        match = self.t.expect(rb"\[(\d+)\] stopped \d+")
        self.t.expect(rb"aetheri> ")
        return int(match.group(1))

    def test_foreground_context_and_prompt_interrupt(self):
        self.helper("probe")
        self.t.expect(rb"aetheri> ")
        value = self.context()
        self.assertEqual(value["group"], value["foreground"])
        self.assertEqual(value["session"], self.t.pid)
        self.assertNotEqual(value["group"], self.t.pid)
        self.assertEqual(value["blocked"], [])
        self.t.send("touch never-executed")
        self.t.send(b"\x03")
        self.t.expect(rb"aetheri> ")
        self.t.command("printf 'status=%s\\n' $?")
        self.assertIn(b"status=130", self.t.data)
        self.assertFalse((self.t.root / "never-executed").exists())
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_pipeline_group_receives_ctrl_c_and_both_originals_end(self):
        self.helper("block", pipeline="block")
        left, right = self.context(), self.context("other")
        self.assertEqual(left["group"], right["group"])
        self.assertEqual(left["group"], left["foreground"])
        pins = [os.pidfd_open(c["pid"]) for c in (left, right)]
        try:
            self.t.send(b"\x03")
            self.t.expect(rb"aetheri> ")
            for fd in pins:
                self.assertTrue(select.select([fd], [], [], 1)[0])
        finally:
            for fd in pins:
                os.close(fd)
        self.t.command("printf 'status=%s\\n' $?")
        self.assertIn(b"status=130", self.t.data)
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_stopped_job_exit_and_eof_refusal_then_fg(self):
        self.helper("block")
        job = self.stop()
        self.t.command("jobs")
        self.t.command("exit")
        self.assertIn(b"original jobs remain", self.t.data)
        self.t.send(b"\x04")
        self.t.expect(rb"before EOF")
        self.t.expect(rb"aetheri> ")
        self.t.send(f"fg {job}\n")
        self.assertEqual(self.t.foreground(), self.context()["group"])
        self.t.send(b"\x03")
        self.t.expect(rb"aetheri> ")
        self.t.command(f"fg {job}")
        self.assertIn(b"no such original job", self.t.data)
        self.t.send("exit 7\n")
        self.t.wait(7)

    def test_background_reader_stops_until_foreground_owns_terminal(self):
        self.helper("read")
        job = self.stop()
        self.t.send(f"bg {job}\n")
        self.t.expect(rb"continue requested")
        self.t.expect(rb"stopped")
        self.t.expect(rb"aetheri> ")
        self.t.send(f"fg {job}\n")
        self.t.send("payload\n")
        self.t.expect(rb"DATA payload")
        self.t.expect(rb"aetheri> ")
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_job_modes_saved_and_shell_modes_restored(self):
        self.helper("modes")
        self.t.expect(rb"stopped")
        self.t.expect(rb"aetheri> ")
        flags = termios.tcgetattr(self.t.master)[3]
        self.assertEqual(flags & (termios.ICANON | termios.ECHO), termios.ICANON | termios.ECHO)
        self.t.send("fg\n")
        self.t.expect(rb"JOB_MODES_RESTORED")
        self.t.expect(rb"aetheri> ")
        self.assertEqual(termios.tcgetattr(self.t.master)[3] & (termios.ICANON | termios.ECHO), termios.ICANON | termios.ECHO)
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_background_completion_wakes_idle_shell_and_retires_job(self):
        self.helper("timer")
        job = self.stop()
        self.t.send(f"bg {job}\n")
        self.t.expect(rb"done 0")
        self.t.expect(rb"aetheri> ")
        self.t.command(f"fg {job}")
        self.assertIn(b"no such original job", self.t.data)
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_hup_ignored_child_retains_shell_until_exact_child_ends(self):
        self.helper("ignore")
        self.t.expect(rb"HUP_IGNORED")
        child = os.pidfd_open(self.context()["pid"])
        try:
            signal.pidfd_send_signal(self.t.pin, signal.SIGHUP)
            self.t.pump(.15)
            self.assertIsNone(self.t.status)
            self.assertFalse(select.select([child], [], [], .1)[0])
            signal.pidfd_send_signal(child, signal.SIGTERM)
            self.t.wait(129)
        finally:
            os.close(child)

    def test_foreign_foreground_is_not_replaced_with_shell_prompt(self):
        self.helper("foreign")
        self.t.expect(rb"FOREIGN_SELECTED")
        self.t.wait(125)
        self.assertNotIn(b"aetheri> ", self.t.data[self.t.cursor:])
        self.assertIn(b"refusing foreground replacement", self.t.data)

    def test_fixed_job_budget_and_never_reused_ids(self):
        ids = []
        groups = {}
        for index in range(16):
            self.helper("block", "job" + str(index))
            job = self.stop()
            ids.append(job)
            groups[job] = self.context("job" + str(index))["group"]
        self.assertEqual(len(set(ids)), 16)
        self.t.command("touch refused")
        self.assertFalse((self.t.root / "refused").exists())
        self.assertIn(b"job budget", self.t.data)
        for job in ids:
            self.t.send(f"fg {job}\n")
            self.assertEqual(self.t.foreground(), groups[job])
            self.t.send(b"\x03")
            self.t.expect(rb"aetheri> ")
        self.helper("block", "fresh")
        self.assertGreater(self.stop(), max(ids))
        self.t.send("fg\n")
        self.assertEqual(self.t.foreground(), self.context("fresh")["group"])
        self.t.send(b"\x03")
        self.t.expect(rb"aetheri> ")
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_idle_prompt_sleeps_without_periodic_work(self):
        def ticks():
            fields = Path(f"/proc/{self.t.pid}/stat").read_text().rsplit(")", 1)[1].split()
            return int(fields[11]) + int(fields[12])
        first = ticks()
        time.sleep(.2)
        self.assertEqual(ticks(), first)
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_denied_terminal_handoff_has_no_file_effect(self):
        terminal = Terminal(self._testMethodName + "-failure", FAILURE, ("--tty",))
        try:
            terminal.wait(0)
            self.assertIn(b"terminal handoff failure", terminal.data)
        finally:
            terminal.close()
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_ctrl_c_cancels_blocked_redirection_before_exec(self):
        os.mkfifo(self.t.root / "input")
        self.t.send("cat < input\n")
        self.t.foreground()
        self.t.send(b"\x03")
        self.t.expect(rb"aetheri> ")
        self.t.command("printf 'status=%s\\n' $?")
        self.assertIn(b"status=130", self.t.data)
        self.t.send("exit 0\n")
        self.t.wait(0)

    def test_canonical_overflow_does_not_execute_a_truncated_prefix(self):
        prefix = "touch forbidden "
        for value in (prefix + "x" * 5000 + " |\n", prefix + "x" * (4094-len(prefix)) + "\r ignored |\n"):
            modes = termios.tcgetattr(self.t.master)
            modes[0] &= ~termios.ICRNL
            termios.tcsetattr(self.t.master, termios.TCSANOW, modes)
            self.t.send(value)
            self.t.expect(rb"aetheri> ")
            self.assertFalse((self.t.root / "forbidden").exists())
            self.assertIn(b"interactive line budget", self.t.data)
        self.t.send("exit 0\n")
        self.t.wait(0)


if __name__ == "__main__":
    assert os.getuid() != 0
    print("Private PTY evidence:", ROOT, flush=True)
    unittest.main(verbosity=2)
