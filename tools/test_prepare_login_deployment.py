"""Offline regressions for the image login layer."""
import json
import os
from pathlib import Path
import stat
import tempfile
import tomllib
import unittest

import prepare_login_deployment as deployment


class LoginDeploymentTests(unittest.TestCase):
    def setUp(self):
        work = tempfile.TemporaryDirectory()
        self.addCleanup(work.cleanup)
        self.root = Path(work.name)
        self.manifest = self.root / "selected.json"
        self.selection = {"format": 1, "artifacts": {}}
        for name in deployment.ARTIFACTS:
            if name in deployment.EXECUTABLES:
                elf = bytearray(64)
                elf[:7] = b"\x7fELF\x02\x01\x01"
                elf[16:20] = b"\x03\x00\x3e\x00"
                body = bytes(elf) + name.encode()
            elif name == "greeter-font":
                body = b"\x00\x01\x00\x00fixture"
            else:
                body = b"fixture notice\n"
            self.select(name, body)

    def select(self, role, body):
        name = role + ".input"
        (self.root / name).write_bytes(body)
        self.selection["artifacts"][role] = {
            "path": name, "size": len(body), "sha256": deployment.digest(body)}

    def save(self):
        self.manifest.write_text(json.dumps(self.selection), encoding="utf-8")

    def test_fixed_layout_is_reproducible(self):
        self.save()
        first, second = self.root / "first", self.root / "second"
        selected = deployment.stage(self.manifest, first)
        self.assertEqual(selected, deployment.stage(self.manifest, second))
        self.assertEqual((first / "receipt.json").read_bytes(), (second / "receipt.json").read_bytes())
        tree = first / "root"
        self.assertEqual({entry["path"] for entry in selected["files"]}, {
            "/" + p.relative_to(tree).as_posix() for p in tree.rglob("*") if p.is_file()})
        for entry in selected["files"]:
            target = tree / entry["path"][1:]
            self.assertEqual(entry["sha256"], deployment.digest(target.read_bytes()))
            if os.name != "nt":
                self.assertEqual(entry["mode"], stat.S_IMODE(target.stat().st_mode))
        conf = tomllib.loads((tree / deployment.DATA / "greetd.toml").read_text())
        self.assertIs(conf["general"]["source_profile"], False)
        self.assertEqual(conf["general"]["service"], "aetheri-login")
        self.assertEqual(conf["default_session"]["service"], "aetheri-greeter")
        self.assertEqual(conf["default_session"]["command"],
                         "/usr/libexec/aetheri/aetheri-login-greeter")
        units = tree / deployment.UNITS
        self.assertIn("Wants=aetheri-login-startup.service", (units / "aetheri-login.service").read_text())
        self.assertIn("SELinuxContext=system_u:system_r:aetheri_login_startup_t:s0",
                      (units / "aetheri-login-startup.service").read_text())
        self.assertFalse((units / "display-manager.service").exists())
        self.assertEqual(sorted(p.name for p in (tree / deployment.DATA / "policy").iterdir()),
                         ["login-graphical.cil", "login-startup.cil", "login.cil", "login.fc"])
        with self.assertRaises(FileExistsError):
            deployment.stage(self.manifest, first)

    def test_wrong_artifact_keeps_output_absent(self):
        for name, corrupt, message in [
            ("aetheri-login-backend", b"changed bytes", "differs from its selection"),
            ("aetheri-login-greeter", b"not an executable", "x86-64 ELF"),
            ("greeter-font", b"not a font", "TrueType"),
        ]:
            with self.subTest(name=name):
                original = (self.root / (name + ".input")).read_bytes()
                if name == "aetheri-login-backend":
                    (self.root / (name + ".input")).write_bytes(corrupt)
                else:
                    self.select(name, corrupt)
                self.save()
                dest = self.root / name
                with self.assertRaisesRegex(ValueError, message):
                    deployment.stage(self.manifest, dest)
                self.assertFalse(dest.exists())
                self.select(name, original)

    def test_rejects_ambiguous_manifest(self):
        self.manifest.write_text('{"format": 1, "format": 1, "artifacts": {}}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "duplicate manifest field"):
            deployment.stage(self.manifest, self.root / "out")
        del self.selection["artifacts"]["aetheri-session"]
        self.save()
        with self.assertRaisesRegex(ValueError, "complete format-1"):
            deployment.stage(self.manifest, self.root / "out")
        self.assertFalse((self.root / "out").exists())


if __name__ == "__main__":
    unittest.main()
