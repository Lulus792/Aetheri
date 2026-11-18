"""Checks for offline WLAN staging and refusal before image mutation."""

from contextlib import chdir
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import desktop_wlan_payload as wlan
from desktop_iwd_payload import (
    DAEMON, RESOLVER_ACCOUNTS, RESOLVER_DAEMON, RESOLVER_SOCKET_UNITS, remove_resolver_socket_activation,
    resolver_accounts, reviewed_resolver_daemon,
)


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


class DesktopResolverAccounts(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.image = Path(temporary.name)
        self.etc = self.image / "etc"
        self.etc.mkdir()
        (self.etc / "passwd").write_bytes(b"root:x:0:0:root:/:/usr/sbin/nologin\n")
        (self.etc / "group").write_bytes(b"root:x:0:\n")

    def test_preflight_selects_locked_identity_and_is_idempotent(self):
        updates = resolver_accounts(self.image)
        self.assertEqual({path.name for path, _, _ in updates}, RESOLVER_ACCOUNTS.keys())
        self.assertFalse((self.etc / "resolv.conf").exists())
        for path, record, create in updates:
            with path.open("xb" if create else "ab") as out:
                out.write(record + b"\n")
        self.assertEqual(resolver_accounts(self.image), [])

    def test_foreign_primary_group_is_refused_before_changes(self):
        group = self.etc / "group"
        original = group.read_bytes() + b"other:x:0193:\n"
        group.write_bytes(original)
        with self.assertRaisesRegex(ValueError, "conflicting resolver account or ID"):
            resolver_accounts(self.image)
        self.assertEqual(group.read_bytes(), original)
        self.assertFalse((self.etc / "shadow").exists())

    def test_foreign_resolver_configuration_is_refused(self):
        conf = self.etc / "resolv.conf"
        conf.write_bytes(b"nameserver 192.0.2.1\n")
        with self.assertRaisesRegex(ValueError, "conflicting offline resolver configuration"):
            resolver_accounts(self.image)
        self.assertEqual(conf.read_bytes(), b"nameserver 192.0.2.1\n")

    def test_resolver_service_projects_without_socket_dependencies(self):
        source = self.image / "packaged.service"
        service = b"[Service]\nType=notify-reload\n"
        account = b"User=systemd-resolve\n\n[Install]\nWantedBy=sysinit.target\n"
        original = service + b"Sockets=" + RESOLVER_SOCKET_UNITS + b"\n" + account
        original += b"Also=" + RESOLVER_SOCKET_UNITS + b"\n"
        source.write_bytes(original)
        entry = {"source": str(source), "package": "reviewed-resolver.rpm"}
        remove_resolver_socket_activation(entry, self.image)
        self.assertEqual(source.read_bytes(), original)
        self.assertEqual(Path(entry["source"]).read_bytes(), service + account)
        self.assertNotIn("package", entry)
        self.assertIn("reviewed-resolver.rpm", entry["provenance"])

    def test_resolver_daemon_selects_exact_bytes_from_relative_package_directory(self):
        payload = b"patched daemon"
        profile = {"resolver_daemon": {
            "filename": RESOLVER_DAEMON, "destination": DAEMON, "mode": 0o755,
            "size": len(payload), "sha256": hashlib.sha256(payload).hexdigest(),
        }}
        packages = self.image / "packages"
        packages.mkdir()
        daemon = packages / RESOLVER_DAEMON
        daemon.write_bytes(payload)
        with chdir(self.image.resolve()):
            selected = reviewed_resolver_daemon(profile, Path("packages"))
            self.assertEqual(Path(selected["source"]), daemon.resolve())
            self.assertEqual(selected["destination"], DAEMON)
            daemon.write_bytes(b"altered daemon")
            with self.assertRaisesRegex(ValueError, "different WLAN input"):
                reviewed_resolver_daemon(profile, Path("packages"))


if __name__ == "__main__":
    unittest.main()
