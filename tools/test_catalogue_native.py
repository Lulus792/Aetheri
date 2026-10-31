"""Cross-language check: native opening of populated Python-migrated catalogues."""
import hashlib
from contextlib import closing
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

import catalogue_contracts as cc
from test_contracts import legacy, rows


def main():
    if sys.platform != "linux":
        raise RuntimeError("native catalogue checks require the Linux build")
    binary = Path(sys.argv[1]).resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="catalogue-interop-", dir=binary.parent) as temporary:
        work = Path(temporary)
        if work.resolve().parent != binary.parent:
            raise RuntimeError("test cleanup escaped build directory")
        for kind in ("system", "user"):
            directory = work / kind
            directory.mkdir(mode=0o700)
            path = directory / "catalogue.sqlite3"
            with closing(sqlite3.connect(path)) as db, closing(sqlite3.connect(work / (kind + "-backup.sqlite3"))) as backup:
                context = legacy(db, kind)
                cc.migrate_v1_to_v2(db, kind, backup=backup)
                expected = rows(db)
            os.chmod(path, 0o600)
            for version in ((2,3,4,5,6) if kind == 'system' else (2,3,4)):
                if version > 2:
                    with closing(sqlite3.connect(path)) as db:
                        getattr(cc,f'migrate_v{version-1}_to_v{version}')(db,kind)
                        actual=rows(db)
                        if any(actual[name]!=value for name,value in expected.items()):
                            raise AssertionError('additive migration changed original rows')
                        expected=actual
                before = hashlib.sha256(path.read_bytes()).digest()
                subprocess.run([str(binary), "open", str(directory), kind, context["epoch"].hex(),
                                context["account"].hex() if kind == "user" else "-"], check=True, timeout=15)
                if hashlib.sha256(path.read_bytes()).digest() != before:
                    raise AssertionError("native validation changed a healthy migrated database")
                with closing(sqlite3.connect(path)) as reopened:
                    cc.configure(reopened)
                    cc.validate(reopened, kind,version)
                    if rows(reopened) != expected:
                        raise AssertionError("populated migration rows changed after native opening")
    print("Populated Root v2–v6/User v2–v4 accepted by native C17; all rows and file bytes preserved.")


if __name__ == "__main__":
    main()
