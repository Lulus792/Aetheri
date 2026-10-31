"""Development harness for canonical SQLite contracts, not a privileged OS service.

No command-line database mutator. Callers must stop writers/services, own the
connections, and provide a distinct empty backup. See spec/README.md.
"""
from pathlib import Path
import os
import re
import sqlite3

ROOT = Path(__file__).resolve().parents[1]
APPLICATION_IDS = {"system": 1096042579, "user": 1096042581}
CURRENT_VERSION = 2


class ContractError(RuntimeError):
    pass


def statements(text):
    buffer = ""
    for line in text.splitlines(keepends=True):
        buffer += line
        if sqlite3.complete_statement(buffer):
            sql = re.sub(r"(?m)^\s*--[^\n]*", "", buffer).strip()
            if sql:
                yield sql
            buffer = ""
    if re.sub(r"(?m)^\s*--[^\n]*", "", buffer).strip():
        raise ContractError("incomplete contract SQL")


def source(kind, version):
    if kind not in APPLICATION_IDS or version not in (1, 2, 3):
        raise ContractError("unsupported catalogue kind/version")
    if version==3:
        return source(kind,2)+extension_v3(kind)
    return [sql for name in ("common", kind)
            for sql in statements((ROOT / f"spec/schema/{name}-v{version}.sql").read_text(encoding="utf-8"))]


def extension_v3(kind):
    if kind not in APPLICATION_IDS:
        raise ContractError('unsupported catalogue kind')
    return [sql for name in ('common',kind)
            for sql in statements((ROOT/f'spec/schema/{name}-v3.sql').read_text(encoding='utf-8'))]


def configure(db):
    if db.in_transaction:
        raise ContractError("connection already has a transaction")
    db.execute("PRAGMA foreign_keys=ON")
    mode = db.execute("PRAGMA journal_mode=DELETE").fetchone()[0]
    if mode not in ("delete", "memory"):
        raise ContractError("rollback journal required")
    db.execute("PRAGMA synchronous=EXTRA")


def bootstrap(db, kind, version=CURRENT_VERSION):
    sql = source(kind, version)
    if db.execute("SELECT COUNT(*) FROM sqlite_schema").fetchone()[0]:
        raise ContractError("bootstrap requires an empty catalogue")
    configure(db)
    db.execute("BEGIN EXCLUSIVE")
    try:
        for statement in sql:
            if not statement.startswith("PRAGMA"):
                db.execute(statement)
        db.execute(f"PRAGMA application_id={APPLICATION_IDS[kind]}")
        db.execute(f"PRAGMA user_version={version}")
        db.commit()
    except BaseException:
        db.rollback()
        raise


_SQL_TOKENS = re.compile(r"'(?:''|[^'])*'|\"(?:\"\"|[^\"])*\"|--[^\n]*|/\*.*?\*/|[ \t\r\n\f\v]+|[^'\"/\- \t\r\n\f\v]+|.", re.S)


def canonical(sql):
    # Normalize only formatting/ordinary quoted identifiers. Text literals and
    # their escaped quotes/whitespace remain byte-significant, as in native C17.
    if not isinstance(sql, str) or "\0" in sql:
        raise ContractError("invalid schema SQL text")
    output, space = [], False
    for piece in _SQL_TOKENS.finditer(sql):
        token = piece[0]
        if token == "/" and sql.startswith("/*", piece.start()):
            raise ContractError("incomplete schema SQL comment")
        if token.startswith(("--", "/*")) or token[0] in " \t\r\n\f\v":
            space = True
            continue
        if token.startswith('"'):
            match = re.fullmatch(r'"([A-Za-z_][A-Za-z0-9_]*)"', token)
            if match is None:
                raise ContractError("unsupported quoted schema identifier")
            token = match[1]
        elif token == "'":
            raise ContractError("incomplete schema SQL literal")
        if space and output:
            output.append(" ")
        output.append(token)
        space = False
    return "".join(output)


def signature(db):
    return [(kind, name, table, canonical(sql)) for kind, name, table, sql in
            db.execute("SELECT type,name,tbl_name,sql FROM sqlite_schema "
                       "WHERE sql IS NOT NULL ORDER BY type,name")]


def validate(db, kind, version=CURRENT_VERSION, *, metadata=True):
    source(kind, version)  # Validate the requested supported pair before any write.
    if db.execute("PRAGMA application_id").fetchone()[0] != APPLICATION_IDS[kind]:
        raise ContractError("wrong application_id")
    if db.execute("PRAGMA user_version").fetchone()[0] != version:
        raise ContractError("unsupported catalogue version")
    if db.execute("PRAGMA foreign_keys").fetchone()[0] != 1:
        raise ContractError("foreign keys must be enabled")
    if db.execute("PRAGMA foreign_key_check").fetchall():
        raise ContractError("foreign-key corruption")
    if db.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
        raise ContractError("catalogue integrity failure")
    reference = sqlite3.connect(":memory:")
    try:
        bootstrap(reference, kind, version)
        if signature(db) != signature(reference):
            raise ContractError("catalogue schema differs from canonical contract")
    finally:
        reference.close()
    if metadata:
        rows = db.execute("SELECT singleton,kind FROM authority_meta").fetchall()
        if rows != [(1, kind)]:
            raise ContractError("missing or mismatched authority metadata")


def _main_file(db):
    return next(row[2] for row in db.execute("PRAGMA database_list") if row[1] == "main")


def migrate_v2_to_v3(db,kind,*,checkpoint=None):
    """Explicit quiesced-owner additive migration; never an automatic opener.

    Existing tables/rows/metadata are untouched. DDL and version commit together.
    A failed acknowledgement is queried, never rolled back by copying an old file.
    This is development tooling; native owners retain their independent lock.
    """
    if db.in_transaction: raise ContractError('dedicated idle connection required')
    version=db.execute('PRAGMA user_version').fetchone()[0]
    if version==3:
        validate(db,kind,3); return False
    configure(db)
    def mark(phase):
        if checkpoint: checkpoint(phase)
    try:
        db.execute('BEGIN EXCLUSIVE')
        validate(db,kind,2); mark('locked')
        for n,statement in enumerate(extension_v3(kind)):
            db.execute(statement); mark('statement_'+str(n))
        db.execute('PRAGMA user_version=3'); mark('version_set')
        validate(db,kind,3); mark('validated')
        db.commit(); mark('committed')
    except BaseException:
        if db.in_transaction: db.rollback()
        raise
    return True


def migrate_v1_to_v2(db, kind, *, backup, checkpoint=None):
    """Atomic metadata migration with a pre-migration SQLite backup.

    Requires externally quiesced services/writers. Checkpoints support tests.
    Returns False for an already valid v2 catalogue, True after commit.
    Backup is never automatically restored over a source.
    """
    if db.in_transaction or backup.in_transaction:
        raise ContractError("dedicated idle connections required")
    version = db.execute("PRAGMA user_version").fetchone()[0]
    if version == CURRENT_VERSION:
        validate(db, kind)
        return False
    validate(db, kind, 1)
    if db.execute("PRAGMA journal_mode").fetchone()[0] not in ("delete", "memory"):
        raise ContractError("migration requires a quiesced rollback-journal catalogue")
    if db is backup:
        raise ContractError("backup must be distinct")
    a, b = _main_file(db), _main_file(backup)
    if a and b and os.path.samefile(a, b):
        raise ContractError("backup aliases source")
    if backup.execute("SELECT COUNT(*) FROM sqlite_schema").fetchone()[0]:
        raise ContractError("backup must be empty")
    configure(backup)
    before = db.execute("PRAGMA data_version").fetchone()[0]
    db.backup(backup)
    validate(backup, kind, 1)

    def mark(phase):
        if checkpoint:
            checkpoint(phase)

    mark("backed_up")
    contract = source(kind, 2)
    tables, other = [], []
    for sql in contract:
        match = re.match(r"CREATE TABLE ([a-z_][a-z0-9_]*)\b", sql)
        if match:
            tables.append((match.group(1), sql))
        elif sql.startswith(("CREATE INDEX", "CREATE UNIQUE INDEX", "CREATE TRIGGER")):
            other.append(sql)
        elif not sql.startswith("PRAGMA"):
            raise ContractError("unrecognized schema statement")
    old_tables = [row[0] for row in db.execute(
        "SELECT name FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name")]

    db.execute("PRAGMA foreign_keys=OFF")  # Must be outside the migration transaction.
    db.execute("PRAGMA synchronous=EXTRA")
    try:
        db.execute("BEGIN EXCLUSIVE")
        if before != db.execute("PRAGMA data_version").fetchone()[0]:
            raise ContractError("writer changed source after backup")
        # Recheck schema after taking the lock; validate needs FK ON, so compare directly.
        if signature(db) != signature(backup):
            raise ContractError("source changed after backup")
        mark("locked")
        for (name,) in db.execute("SELECT name FROM sqlite_schema WHERE type='trigger'").fetchall():
            db.execute(f'DROP TRIGGER "{name}"')
        for name, sql in tables:
            db.execute(sql.replace(f"CREATE TABLE {name}", f"CREATE TABLE _v2_{name}", 1))
        mark("tables_created")
        for name in old_tables:
            columns = [row[1] for row in db.execute(f'PRAGMA table_info("{name}")')]
            names = ",".join(f'"{c}"' for c in columns)
            db.execute(f'INSERT INTO "_v2_{name}" ({names}) SELECT {names} FROM "{name}"')
        mark("copied")
        for name in old_tables:
            db.execute(f'DROP TABLE "{name}"')
        mark("old_dropped")
        for name, _ in tables:
            db.execute(f'ALTER TABLE "_v2_{name}" RENAME TO "{name}"')
        mark("renamed")
        for sql in other:
            db.execute(sql)
        mark("schema_installed")
        target, key, category = (("installs", "install_id", "install") if kind == "system"
                                 else ("data_epochs", "data_epoch_id", "data"))
        db.execute(f"INSERT INTO admission_gates(subject_kind,subject_ref,state) "
                   f"SELECT ?,{key},'closed' FROM {target}", (category,))
        mark("gates_backfilled")
        if db.execute("PRAGMA foreign_key_check").fetchall():
            raise ContractError("migration produced invalid foreign keys")
        if db.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ContractError("migration produced invalid catalogue")
        mark("validated")
        db.execute("PRAGMA user_version=2")
        mark("version_set")
        db.commit()
    except BaseException:
        if db.in_transaction:
            db.rollback()
        raise
    finally:
        db.execute("PRAGMA foreign_keys=ON")
    mark("committed")
    validate(db, kind, 2)
    return True
