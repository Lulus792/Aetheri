"""The current Settings consumer needs its own pure model dependencies."""
import json
from pathlib import Path
import tempfile
import unittest

import build_nexaui as build


class Snapshot(unittest.TestCase):
    def test_current_closure_keeps_original_model_keys(self):
        config = json.loads((build.ROOT/'spec/nexaui-inputs.json').read_bytes())
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory)/'snapshot'
            model, support = build.snapshot_models(config, build.ROOT, destination)
            self.assertEqual(set(model), {'src/settings/model.c', 'src/settings/model.h'})
            self.assertIn('src/audio/types.h', support)
            self.assertIn('src/text/grapheme_data.h', support)
            self.assertIn('src/text/UNICODE-LICENSE.txt', support)
            self.assertEqual(set(model)|set(support), {p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file()})
            for name, digest in {**model, **support}.items():
                self.assertEqual(build.digest(destination/name), digest)
                self.assertEqual((destination/name).read_bytes(), (build.ROOT/name).read_bytes())

    def test_missing_dependency_has_no_partial_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'model.c').write_bytes(b'model')
            with self.assertRaises(FileNotFoundError):
                build.snapshot_models(dict(model=['model.c'], model_support=['missing.h']), root, root/'snapshot')
            self.assertFalse((root/'snapshot').exists())

    def test_duplicate_or_escaping_source_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for config in (dict(model=['a'], model_support=['a']), dict(model=['../escape'], model_support=[])):
                with self.assertRaises(ValueError):
                    build.snapshot_models(config, root, root/'snapshot')
            self.assertFalse((root/'snapshot').exists())


if __name__ == '__main__':
    unittest.main()
