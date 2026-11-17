"""Offline HDA assembly checks with owned input and image trees."""

import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import desktop_hda_payload as hda


class DesktopHda(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.kernel = self.root / "kernel"
        self.image = self.root / "image"
        self.release = "test-release"
        self.modules = Path("usr/lib/modules") / self.release
        self.controller = "kernel/sound/controller.ko.xz"
        self.codec = "kernel/sound/codec.ko.xz"
        (self.image / self.modules).mkdir(parents=True)
        (self.image / hda.CONFIG.parent).mkdir(parents=True)
        for name in ("modules.order", "modules.builtin", "modules.builtin.modinfo"):
            (self.image / self.modules / name).touch()
        for name, contents in ((self.controller, b"controller"), (self.codec, b"codec")):
            path = self.kernel / self.modules / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(contents)
        profile = self.root / "inputs.json"
        profile.write_text(json.dumps({"format": 1, "kernel_release": self.release,
                                       "codec_modules": [self.entry(self.codec)]}))
        controller = self.entry(self.controller)
        del controller["path"]
        controller.update(destination=(self.modules / self.controller).as_posix(),
                          mode=0o644, provenance=None)
        self.addCleanup(patch.stopall)
        patch.object(hda, "PROFILE", profile).start()
        patch.object(hda.prepare_audio_hda, "profile",
                     return_value=({"kernel_release": self.release,
                                    "kernel_modules": [controller]}, "fixture")).start()

    def entry(self, name):
        data = (self.kernel / self.modules / name).read_bytes()
        return {"path": name, "package": "kernel-modules-fixture",
                "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}

    def test_includes_controller_configuration_and_separately_selected_codec(self):
        receipt = hda.install(self.image, self.kernel)
        self.assertEqual(receipt["module_count"], 2)
        self.assertEqual((self.image / hda.CONFIG).read_bytes(), b"snd_hda_intel\n")
        for name in (self.controller, self.codec):
            self.assertEqual((self.image / self.modules / name).read_bytes(),
                             (self.kernel / self.modules / name).read_bytes())
        with self.assertRaisesRegex(ValueError, "configuration already exists"):
            hda.install(self.image, self.kernel)

    def test_changed_codec_cannot_complete_layer(self):
        (self.kernel / self.modules / self.codec).write_bytes(b"other")
        with self.assertRaisesRegex(ValueError, "changed"):
            hda.install(self.image, self.kernel)
        self.assertFalse((self.image / hda.CONFIG).exists())

    def test_incomplete_kernel_base_refused_before_copy(self):
        (self.image / self.modules / "modules.order").unlink()
        with self.assertRaises(FileNotFoundError):
            hda.install(self.image, self.kernel)
        self.assertFalse((self.image / self.modules / self.controller).exists())


if __name__ == "__main__":
    unittest.main()
