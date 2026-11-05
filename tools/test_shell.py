"""Actual experimental shell processes/files/pipes in owned temporary directories."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest


SHELL = str(Path(sys.argv.pop(1)).resolve())


def quote(value):
    return "'" + str(value).replace("'", "'\\''") + "'"


class ShellExecution(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="aetheri-shell-")
        self.root = Path(self.tmp.name)
        self.env = {**os.environ, "HOME": str(self.root), "PATH": "/usr/bin:/bin"}
        self.env.pop("AETHERI_MISSING", None)

    def tearDown(self):
        self.tmp.cleanup()

    def run_shell(self, source, *, data=None, env=None, args=None, pass_fds=(), before_exec=None):
        p = subprocess.Popen([SHELL, *(args or ["-c", source])], cwd=self.root,
                             env=env or self.env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, pass_fds=pass_fds, start_new_session=True,
                             preexec_fn=before_exec)
        try:
            stdout, stderr = p.communicate(data, timeout=8)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.communicate()
            self.fail("shell/process pipeline exceeded its bounded test deadline")
        self.assertNotIn(b"AddressSanitizer", stderr)
        self.assertNotIn(b"runtime error:", stderr)
        return p.returncode, stdout, stderr

    def test_arguments_quotes_unicode_and_nonrecursive_environment(self):
        helper = self.root / "args.py"
        helper.write_text("import json,sys;print(json.dumps(sys.argv[1:],ensure_ascii=False))\n")
        env = {**self.env, "AETHERI_VALUE": "a b | touch unexpected;"}
        code, out, err = self.run_shell(f"{quote(sys.executable)} {quote(helper)} \"$AETHERI_VALUE\" '' Grüße a\\ b", env=env)
        self.assertEqual((code, err), (0, b""))
        self.assertEqual(json.loads(out), [env["AETHERI_VALUE"], "", "Grüße", "a b"])
        self.assertFalse((self.root / "unexpected").exists())

    def test_concurrent_pipe_larger_than_kernel_buffer(self):
        helper = self.root / "produce.py"
        helper.write_text("import sys;sys.stdout.buffer.write(b'x'*300000)\n")
        code, out, err = self.run_shell(f"{quote(sys.executable)} {quote(helper)} | /usr/bin/wc -c")
        self.assertEqual(code, 0, err)
        self.assertEqual(out.strip(), b"300000")

    def test_files_append_input_and_stderr_are_real(self):
        code, out, err = self.run_shell("printf '%s' first > note\nprintf '%s' second >> note\ncat < note\nls /aetheri-definitely-missing 2> errors\n")
        self.assertNotEqual(code, 0)
        self.assertEqual(out, b"firstsecond")
        self.assertEqual(err, b"")
        self.assertTrue((self.root / "errors").read_bytes())

    def test_redirection_replaces_pipe_end_without_hanging(self):
        code, out, err = self.run_shell("printf payload > note | cat")
        self.assertEqual((code, out, err), (0, b"", b""))
        self.assertEqual((self.root / "note").read_bytes(), b"payload")

    def test_parent_state_and_last_status(self):
        target = self.root / "Grüße with space"
        target.mkdir()
        source = f"export AETHERI_VALUE 'a b'\ncd {quote(target)}\npwd\nprintf '%s' \"$PWD\"\n/usr/bin/false\nprintf '%s' $?\nexit 17\ntouch forbidden"
        code, out, err = self.run_shell(source)
        self.assertEqual((code, err), (17, b""))
        self.assertEqual(out.decode(), str(target) + "\n" + str(target) + "1")
        self.assertFalse((target / "forbidden").exists())

    def test_whole_pipeline_rejected_before_files_or_process_effects(self):
        for source in ("touch forbidden |", "pwd > forbidden > other", "pwd > forbidden | cd /", "touch forbidden && pwd", "touch forbidden; pwd", "pwd > forbidden | ''"):
            with self.subTest(source=source):
                code, _, _ = self.run_shell(source)
                self.assertEqual(code, 2)
                self.assertFalse((self.root / "forbidden").exists())
                self.assertFalse((self.root / "other").exists())

    def test_missing_expansion_is_error_and_empty_is_valid(self):
        code, _, _ = self.run_shell("touch forbidden $AETHERI_MISSING")
        self.assertEqual(code, 2)
        self.assertFalse((self.root / "forbidden").exists())
        code, out, _ = self.run_shell("export AETHERI_EMPTY ''\nprintf '[%s]' $AETHERI_EMPTY")
        self.assertEqual((code, out), (0, b"[]"))
        code, _, _ = self.run_shell("export AETHERI_VALUE yes\nunset AETHERI_VALUE\nprintf '%s' $AETHERI_VALUE")
        self.assertEqual(code, 2)

    def test_lookup_has_no_implicit_shell_fallback(self):
        bad = self.root / "bad"
        bad.write_text("touch forbidden\n")
        bad.chmod(0o700)
        for path in (quote(bad), "bad"):
            code, _, _ = self.run_shell(path, env={**self.env, "PATH": str(self.root)})
            self.assertEqual(code, 126)
            self.assertFalse((self.root / "forbidden").exists())
        good = self.root / "good"
        good.write_text(f"#!{sys.executable}\nprint('explicit interpreter')\n")
        good.chmod(0o700)
        code, out, _ = self.run_shell("good", env={**self.env, "PATH": str(self.root)})
        self.assertEqual((code, out), (0, b"explicit interpreter\n"))

    def test_not_found_denied_and_last_pipeline_status(self):
        for command, expected in (("aetheri-command-that-does-not-exist", 127), ("./missing", 127), ("/usr/bin/false | /usr/bin/true", 0), ("/usr/bin/true | /usr/bin/false", 1), ("exit 256", 2)):
            with self.subTest(command=command):
                self.assertEqual(self.run_shell(command)[0], expected)
        bad = self.root / "denied"
        bad.write_text("cannot execute")
        bad.chmod(0o600)
        self.assertEqual(self.run_shell("./denied")[0], 126)

    def test_inherited_extra_descriptor_is_not_exported(self):
        fd = os.open(self.root / "private", os.O_RDWR | os.O_CREAT, 0o600)
        try:
            helper = self.root / "fd.py"
            helper.write_text("import os,sys\ntry:os.fstat(int(sys.argv[1]));sys.exit(5)\nexcept OSError:sys.exit(0)\n")
            code, _, err = self.run_shell(f"{quote(sys.executable)} {quote(helper)} {fd}", pass_fds=(fd,))
            self.assertEqual(code, 0, err)
        finally:
            os.close(fd)

    def test_script_stdin_and_crlf_without_losing_program_input(self):
        self.assertEqual(self.run_shell("cat", data=b"native input")[1], b"native input")
        script = self.root / "script.ae"
        script.write_bytes(b"# comment\r\nprintf done\r\n")
        self.assertEqual(self.run_shell("", args=["--file", str(script)])[1], b"done")
        self.assertEqual(self.run_shell("", args=["--stdin"], data=b"printf script\n")[1], b"script")

    def test_oversized_or_nul_line_never_executes_partial_input(self):
        for data in (b"touch forbidden " + b"a" * 16384, b"touch forbidden\0\n"):
            code, _, _ = self.run_shell("", args=["--stdin"], data=data)
            self.assertEqual(code, 2)
            self.assertFalse((self.root / "forbidden").exists())

    def test_expansion_budget_rejects_whole_command(self):
        code, _, _ = self.run_shell("touch forbidden $AETHERI_VALUE$AETHERI_VALUE", env={**self.env, "AETHERI_VALUE": "x" * 9000})
        self.assertEqual(code, 2)
        self.assertFalse((self.root / "forbidden").exists())

    def test_signal_status_and_short_pipe_do_not_hang(self):
        helper = self.root / "terminate.py"
        helper.write_text("import os,signal;os.kill(os.getpid(),signal.SIGTERM)\n")
        self.assertEqual(self.run_shell(f"{quote(sys.executable)} {quote(helper)}")[0], 143)
        self.assertEqual(self.run_shell("yes | head -c 2")[:2], (0, b"y\n"))

    def test_inherited_sigchld_ignore_does_not_lose_original_status(self):
        code, out, err = self.run_shell("printf observed", before_exec=lambda: signal.signal(signal.SIGCHLD, signal.SIG_IGN))
        self.assertEqual((code, out, err), (0, b"observed", b""))

    def test_terminal_stdin_is_explicitly_not_interactive_yet(self):
        import pty
        master, slave = pty.openpty()
        try:
            p = subprocess.run([SHELL, "--stdin"], stdin=slave, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, timeout=5, start_new_session=True)
            self.assertEqual(p.returncode, 2)
            self.assertIn(b"noninteractive", p.stderr)
        finally:
            os.close(master)
            os.close(slave)


if __name__ == "__main__":
    assert os.getuid() != 0, "run only as ordinary disposable-carrier user"
    unittest.main(verbosity=2)
