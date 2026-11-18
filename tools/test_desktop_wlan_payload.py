"""Checks for offline WLAN staging and refusal before image mutation."""

import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import desktop_wlan_payload as wlan


class DesktopWlan(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name).resolve()
        self.image = root / "image"
        self.kernel = root / "kernel"
        self.firmware = root / "firmware"
        self.firmware.mkdir()
        self.regdb = root / "regdb.rpm"
        self.profile_path = root / "wlan.json"
        self.profile = json.loads(wlan.PROFILE.read_bytes())
        modules = self.image / wlan.MODULES[1:]
        modules.mkdir(parents=True)
        for name in ("modules.order", "modules.builtin", "modules.builtin.modinfo"):
            (modules / name).write_bytes(b"")
        self.bodies = {}
        for kind in ("kernel_modules", "firmware", "regdb_files"):
            for entry in self.profile[kind]:
                name = entry["destination"]
                body = name.encode()
                entry.update(size=len(body), sha256=hashlib.sha256(body).hexdigest())
                self.bodies[name] = body
                if kind == "regdb_files":
                    continue
                source = self.kernel / name[1:] if kind == "kernel_modules" else self.firmware / Path(name).name
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(body)
        self.save_profile()

    def save_profile(self):
        self.profile_path.write_text(json.dumps(self.profile), encoding="utf-8")

    def install(self):
        with patch.object(wlan, "PROFILE", self.profile_path), patch.object(
            wlan, "regulatory_files", return_value={name: self.bodies[name] for name in wlan.REGDB_FILES}
        ):
            return wlan.install(self.image, self.kernel, self.firmware, self.regdb)

    def test_pinned_files_and_pci_scoped_discovery(self):
        receipt = self.install()
        self.assertEqual(receipt["pci_device"], "8086:24fd")
        self.assertEqual(receipt["inventory"]["copied"], len(self.bodies) + 1)
        for name, expected in self.bodies.items():
            self.assertEqual((self.image / name[1:]).read_bytes(), expected)
        rule = (self.image / wlan.RULE_PATH[1:]).read_bytes()
        self.assertEqual(rule, wlan.RULE)
        self.assertIn(b'ATTR{device}=="0x24fd"', rule)
        self.assertFalse((self.image / "etc/modules-load.d").exists())
        self.assertEqual(self.install()["inventory"]["reused"], len(self.bodies) + 1)

    def test_changed_firmware_leaves_image_unmodified(self):
        (self.firmware / "iwlwifi-8265-36.ucode").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "different WLAN input"):
            self.install()
        self.assertFalse((self.image / wlan.MODULES[1:] / "kernel").exists())
        self.assertFalse((self.image / "usr/lib/firmware").exists())

    def test_missing_regulatory_signature_leaves_image_unmodified(self):
        self.profile["regdb_files"] = [entry for entry in self.profile["regdb_files"]
                                       if not entry["destination"].endswith(".p7s")]
        self.save_profile()
        with self.assertRaisesRegex(ValueError, "unreviewed desktop WLAN profile"):
            self.install()
        self.assertFalse((self.image / wlan.RULE_PATH[1:]).exists())


if __name__ == "__main__":
    unittest.main()
