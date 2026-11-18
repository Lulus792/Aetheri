"""Offline regressions for the image login layer."""
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import tomllib
import unittest

import prepare_login_deployment as deployment
import prepare_login_selection as selection


class LoginDeploymentTests(unittest.TestCase):
    verifier = None
    def setUp(self):
        work = tempfile.TemporaryDirectory()
        self.addCleanup(work.cleanup)
        self.root = Path(work.name)
        self.manifest = self.root / "selected.json"
        self.selection = {"format": 2, "artifacts": {}}
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
        fallback = next(entry for entry in selected["files"] if entry["path"] == "/etc/pam.d/other")
        self.assertEqual(fallback["mode"], 0o444)
        self.assertEqual((tree / "etc/pam.d/other").read_bytes(),
                         b"auth required pam_deny.so\naccount required pam_deny.so\n"
                         b"password required pam_deny.so\nsession required pam_deny.so\n")
        nss = (tree / "etc/nsswitch.conf").read_bytes()
        self.assertEqual(nss.splitlines()[1:],
                         [b"passwd: files", b"group: files", b"shadow: files", b"gshadow: files",
                          b"initgroups: files", b"hosts: files dns"])
        self.assertEqual((tree / "etc/ld.so.conf").read_bytes(), b"/usr/lib/aetheri/runtime\n")
        conf = tomllib.loads((tree / deployment.DATA / "greetd.toml").read_text())
        self.assertIs(conf["general"]["source_profile"], False)
        self.assertEqual(conf["general"]["service"], "aetheri-login")
        self.assertEqual(conf["default_session"]["service"], "aetheri-greeter")
        self.assertEqual(conf["default_session"]["command"],
                         "/usr/libexec/aetheri/aetheri-login-greeter")
        units = tree / deployment.UNITS
        backend = (units / "aetheri-login.service").read_text().splitlines()
        self.assertIn("Wants=aetheri-login-startup.service", backend)
        self.assertIn("Requires=systemd-logind.service systemd-tmpfiles-setup.service", backend)
        self.assertEqual([line for line in backend if line.startswith("AssertPathIsDirectory=")], [
            "AssertPathIsDirectory=/run/aetheri/login",
            "AssertPathIsDirectory=/run/aetheri/login-greeter",
            "AssertPathIsDirectory=/run/aetheri/login-tmp",
            "AssertPathIsDirectory=/run/aetheri/login-shm",
        ])
        self.assertIn("SELinuxContext=system_u:system_r:aetheri_login_startup_t:s0",
                      (units / "aetheri-login-startup.service").read_text())
        self.assertFalse((units / "display-manager.service").exists())
        self.assertEqual(sorted(p.name for p in (tree / deployment.DATA / "policy").iterdir()),
                         ["login-apps.cil", "login-graphical.cil", "login-startup.cil", "login.cil", "login.fc"])
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
        del self.selection["artifacts"]["aetheri-files-nexaui"]
        self.save()
        with self.assertRaisesRegex(ValueError, "complete format-2"):
            deployment.stage(self.manifest, self.root / "out")
        self.assertFalse((self.root / "out").exists())

    def test_sound_profile_requires_the_reviewed_executable(self):
        self.selection["format"] = deployment.SOUND_FORMAT
        stage = self.root / "sound"
        self.save()
        with self.assertRaisesRegex(ValueError, "complete format-3"):
            deployment.stage(self.manifest, stage)
        name = deployment.SOUND_EXECUTABLE
        original = (self.root / "aetheri-editor.input").read_bytes()[:64] + name.encode()
        self.select(name, original)
        self.save()
        receipt = deployment.stage(self.manifest, stage)
        self.assertEqual(receipt["format"], 3)
        dropin, policy = self.root / "distribution", self.root / "policy"
        dropin.write_bytes(b"distribution service\n")
        policy.write_bytes(b"\x01\x02")
        args = (stage, dropin, policy, "2a" + "00" * 31, 992, 993, [993, 60000])
        body = selection.payload(*args)
        self.assertEqual((body[:8], len(body)), (b"AETHLS03", 943))
        self.assertEqual(int.from_bytes(body[-36:-32], "big"), len(original))
        self.assertEqual(body[-32:].hex(), deployment.digest(original))
        if self.verifier:
            encoded, signed = self.root / "body", self.root / "signed"
            encoded.write_bytes(body)
            subprocess.run([self.verifier, encoded, signed], check=True, timeout=15)
            self.assertEqual((signed.read_bytes()[:len(body)], signed.stat().st_size), (body, 1007))
        candidate = stage / "root" / deployment.LIBEXEC / name
        candidate.chmod(0o755)
        candidate.write_bytes(original[:-1] + b"!")
        with self.assertRaisesRegex(ValueError, f"staged file differs.*{name}"):
            selection.payload(*args)
        candidate.unlink()
        with self.assertRaises(FileNotFoundError):
            selection.payload(*args)

    def test_release_selection_checks_the_actual_staged_bytes(self):
        self.save()
        stage = self.root / "stage"
        deployment.stage(self.manifest, stage)
        dropin, policy = self.root / "distribution", self.root / "policy"
        dropin.write_bytes(b"distribution service\n")
        policy.write_bytes(b"\x01\x02")
        release = "2a" + "00" * 31
        args = (stage, dropin, policy, release, 992, 993, [993, 60000])
        result = selection.payload(*args)
        self.assertEqual(len(result), 907)
        self.assertEqual(result[:8], b"AETHLS02")
        for index, name in enumerate(deployment.DESKTOP_EXECUTABLES, start=16):
            with self.subTest(name=name):
                candidate = stage / "root" / deployment.LIBEXEC / name
                original = candidate.read_bytes()
                offset = 115 + 36 * index
                self.assertEqual(int.from_bytes(result[offset:offset + 4], "big"), len(original))
                self.assertEqual(result[offset + 4:offset + 36].hex(), deployment.digest(original))
                candidate.chmod(0o755)
                candidate.write_bytes(original[:-1] + b"!")
                with self.assertRaisesRegex(ValueError, f"staged file differs.*{name}"):
                    selection.payload(*args)
                candidate.write_bytes(original)
                candidate.chmod(0o555)
        signature = self.root / "signature"
        signature.write_bytes(bytes(63))
        destination = stage / "root" / deployment.DATA / "selection.bin"
        command = [sys.executable, selection.__file__, "--stage", str(stage),
                   "--systemd-dropin", str(dropin), "--effective-policy", str(policy),
                   "--release", release, "--greeter-uid", "992", "--greeter-gid", "993",
                   "--group", "993", "--group", "60000", "--signature", str(signature),
                   "--output", str(destination)]
        failed = subprocess.run(command, capture_output=True, timeout=15)
        self.assertEqual(failed.returncode, 1, failed.stderr)
        self.assertFalse(destination.exists())
        if self.verifier:
            encoded = self.root / "selection"
            encoded.write_bytes(result)
            signed = self.root / "signed"
            subprocess.run([self.verifier, encoded, signed], check=True, timeout=15)
            record = signed.read_bytes()
            self.assertEqual(record[:len(result)], result)
            signature.write_bytes(record[len(result):])
        else:
            signature.write_bytes(bytes(64))
        subprocess.run(command, check=True, timeout=15)
        self.assertEqual(destination.read_bytes(), result + signature.read_bytes())
        if os.name != "nt":
            self.assertEqual(stat.S_IMODE(destination.stat().st_mode), 0o444)
        failed = subprocess.run(command, capture_output=True, timeout=15)
        self.assertEqual(failed.returncode, 1, failed.stderr)

        with self.assertRaisesRegex(ValueError, "strictly increasing"):
            selection.payload(*args[:-1], [60000, 993])

    def test_lock_profile_signs_view_and_pam_with_optional_sound(self):
        self.selection["format"] = deployment.LOCK_FORMAT
        self.save()
        with self.assertRaisesRegex(ValueError, "complete format-4"):
            deployment.stage(self.manifest, self.root / "missing-view")
        self.assertFalse((self.root / "missing-view").exists())
        header = (self.root / "aetheri-editor.input").read_bytes()[:64]
        self.select(deployment.LOCK_EXECUTABLE, header + deployment.LOCK_EXECUTABLE.encode())
        dropin, policy = self.root / "distribution", self.root / "policy"
        dropin.write_bytes(b"distribution service\n")
        policy.write_bytes(b"\x01\x02")
        for with_sound in (False, True):
            with self.subTest(with_sound=with_sound):
                if with_sound:
                    self.select(deployment.SOUND_EXECUTABLE, header + deployment.SOUND_EXECUTABLE.encode())
                self.save()
                stage = self.root / f"lock-{with_sound}"
                receipt = deployment.stage(self.manifest, stage)
                self.assertEqual(receipt["format"], deployment.LOCK_FORMAT)
                args = (stage, dropin, policy, "2a" + "00" * 31, 992, 993, [993, 60000])
                body = selection.payload(*args)
                self.assertEqual((body[:8], len(body)), (b"AETHLS04", 1015))
                if with_sound:
                    self.assertEqual(body[911:943].hex(), deployment.digest(header + deployment.SOUND_EXECUTABLE.encode()))
                else:
                    self.assertEqual(body[907:943], bytes(36))
                    orphan = stage / "root" / deployment.LIBEXEC / deployment.SOUND_EXECUTABLE
                    orphan.write_bytes(header)
                    with self.assertRaisesRegex(ValueError, "no reviewed receipt"):
                        selection.payload(*args)
                    orphan.unlink()
                if self.verifier:
                    encoded, signed = stage / "body", stage / "signed"
                    encoded.write_bytes(body)
                    subprocess.run([self.verifier, encoded, signed], check=True, timeout=15)
                    self.assertEqual((signed.read_bytes()[:len(body)], signed.stat().st_size), (body, 1079))
                for name, offset in ((f"{deployment.LIBEXEC}/{deployment.LOCK_EXECUTABLE}", 943),
                                     (deployment.LOCK_PAM, 979)):
                    path = stage / "root" / name
                    original = path.read_bytes()
                    self.assertEqual(int.from_bytes(body[offset:offset+4], "big"), len(original))
                    self.assertEqual(body[offset+4:offset+36].hex(), deployment.digest(original))
                    path.chmod(0o755)
                    path.write_bytes(original[:-1] + b"!")
                    with self.assertRaisesRegex(ValueError, f"staged file differs.*{path.name}"):
                        selection.payload(*args)
                    path.write_bytes(original)
        candidate = stage / "root" / deployment.LIBEXEC / "aetheri-login-greeter"
        candidate.chmod(0o755)
        original = candidate.read_bytes()
        candidate.write_bytes(original[:-1] + b"P")
        with self.assertRaisesRegex(ValueError, "differs from reviewed receipt"):
            selection.payload(*args)


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "--verify-selection":
        LoginDeploymentTests.verifier = sys.argv[2]
        del sys.argv[1:3]
    unittest.main()
