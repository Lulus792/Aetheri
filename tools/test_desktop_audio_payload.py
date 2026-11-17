"""Check the offline desktop audio projection and its existing-file boundary."""

import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import desktop_audio_payload as audio


class DesktopAudio(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.graph, self.policy, self.image = (self.root / name for name in ("graph", "policy", "image"))
        self.image.mkdir()
        graph = [
            self.file(self.graph, "usr/bin/pipewire", "runtime"),
            self.file(self.graph, "usr/lib64/libpipewire-0.3.so.0.1611.0", "runtime"),
            self.file(self.graph, "usr/include/pipewire.h", "headers"),
            self.file(self.graph, "usr/bin/pw-dump", "test-tool"),
        ]
        self.routes = {row["path"]: row for row in (
            self.file(self.policy, "usr/bin/wireplumber", "runtime"),
            self.file(self.policy, "usr/share/alsa/alsa.conf", "device-data"),
            self.file(self.policy, "usr/share/licenses/wireplumber/COPYING", "notice"),
            self.file(self.policy, "usr/bin/wpctl", "test"),
        )}
        graph_aliases = {"usr/lib64/libpipewire-0.3.so.0": "libpipewire-0.3.so.0.1611.0"}
        for module, name, value in (
            (audio.prepare_audio, "check", None),
            (audio.routing, "check", None),
            (audio.prepare_audio, "profile", ({"files": graph, "symlinks": graph_aliases}, "graph")),
            (audio.routing, "profile", ({"id": "policy"}, self.routes, {})),
        ):
            mock = patch.object(module, name, return_value=value)
            mock.start()
            self.addCleanup(mock.stop)

    @staticmethod
    def file(root, path, role):
        dest = root / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        body = path.encode()
        dest.write_bytes(body)
        dest.chmod(0o644)
        return {"path": path, "role": role, "mode": 0o644, "size": len(body),
                "sha256": hashlib.sha256(body).hexdigest()}

    def test_projects_daemons_and_data_without_sdk_or_test_files(self):
        manifest = audio.audio_manifest(self.graph, self.policy)
        self.assertEqual({entry["destination"] for entry in manifest["files"]},
                         {*audio.DAEMONS.values(), "/usr/lib64/libpipewire-0.3.so.0.1611.0",
                          "/usr/share/alsa/alsa.conf", "/usr/share/licenses/wireplumber/COPYING",
                          *("/usr/share/aetheri/audio/" + name for name in audio.CONFIG_FILES)})
        self.assertEqual(set(manifest["symlinks"]), {"/usr/lib64/libpipewire-0.3.so.0"})

    def test_conflicting_audio_inputs_are_refused_before_image_effects(self):
        path = "usr/lib64/libpipewire-0.3.so.0.1611.0"
        self.routes[path] = self.file(self.policy, path, "runtime")
        self.routes[path]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "conflicting audio inputs"):
            audio.install(self.image, self.graph, self.policy)
        self.assertEqual(list(self.image.iterdir()), [])

    def test_existing_image_file_is_preserved_on_mismatch(self):
        daemon = self.image / audio.DAEMONS["usr/bin/pipewire"][1:]
        daemon.parent.mkdir(parents=True)
        daemon.write_bytes(b"foreign")
        with self.assertRaisesRegex(ValueError, "conflicting existing runtime"):
            audio.install(self.image, self.graph, self.policy)
        self.assertEqual(daemon.read_bytes(), b"foreign")

    def test_hda_projection_adds_alsa_plugin_and_soname(self):
        prefix = self.root / "hda"
        plugin = self.file(prefix, "usr/lib64/spa-0.2/alsa/libspa-alsa.so", "runtime")
        asound = self.file(prefix, "usr/lib64/libasound.so.2.0.0", "runtime")
        alias = "usr/lib64/libasound.so.2"
        pins = {"files": [plugin, asound], "symlinks": {alias: "libasound.so.2.0.0"}}
        with patch.object(audio.prepare_audio_hda, "check") as check, \
             patch.object(audio.prepare_audio_hda, "profile", return_value=(pins, "hda")):
            result = audio.audio_manifest(self.graph, self.policy, prefix)
        check.assert_called_once_with(prefix)
        self.assertEqual(result["symlinks"]["/" + alias], pins["symlinks"][alias])
        staged = {entry["destination"]: entry["sha256"] for entry in result["files"]}
        self.assertEqual(staged["/" + plugin["path"]], plugin["sha256"])


if __name__ == "__main__":
    unittest.main()
