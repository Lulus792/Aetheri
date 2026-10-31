"""Build-time SQL embedding. No Python dependency in the installed authorities."""
import json
from pathlib import Path
import sys

from catalogue_contracts import source,extension_v3,extension_v4,extension_v5


def main():
    destination = Path(sys.argv[1])
    lines = ['/* Generated frozen v2 plus explicit versioned additions. Do not edit. */',
             '#include "catalogue/schema.h"']
    for kind in ("system", "user"):
        statements = [sql for sql in source(kind, 2) if not sql.startswith("PRAGMA")]
        lines.append(f"const char *const ae_schema_{kind}[] = {{")
        for sql in statements:
            sql.encode("ascii")  # Review escaping before accepting non-ASCII SQL sources.
            if len(sql) > 4095:
                raise ValueError("SQL statement exceeds the reviewed C17 literal bound")
            lines.append("    " + json.dumps(sql) + ",")
        lines.extend(["    0", "};"])
        lines.append(f"const char *const ae_schema_{kind}_v3[] = {{")
        for sql in extension_v3(kind):
            sql.encode('ascii')
            if len(sql)>4095: raise ValueError('v3 SQL exceeds C17 literal bound')
            lines.append('    '+json.dumps(sql)+',')
        lines.extend(['    0','};'])
        lines.append(f"const char *const ae_schema_{kind}_v4[] = {{")
        for sql in extension_v4(kind):
            sql.encode('ascii')
            if len(sql)>4095: raise ValueError('v4 SQL exceeds C17 literal bound')
            lines.append('    '+json.dumps(sql)+',')
        lines.extend(['    0','};'])
    lines.append('const char *const ae_schema_system_v5[] = {')
    for sql in extension_v5('system'):
        sql.encode('ascii')
        if len(sql)>4095: raise ValueError('v5 SQL exceeds C17 literal bound')
        lines.append('    '+json.dumps(sql)+',')
    lines.extend(['    0','};'])
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
