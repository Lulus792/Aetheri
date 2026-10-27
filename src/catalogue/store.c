#include "catalogue/store.h"
#include "catalogue/schema.h"
#include "core/root.h"
#include "core/fixture.h"
#include "core/digest.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/openat2.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>

struct ae_catalogue {
    sqlite3 *db;
    struct ae_catalogue_identity identity;
    pid_t process;
    int root, lock;
    struct stat root_stat, database_stat, lock_stat;
    struct ae_root_identity root_identity;
    char directory[PATH_MAX], path[PATH_MAX];
};

static int nonzero(const uint8_t *id)
{
    unsigned int value = 0;
    for (size_t n = 0; n < AE_ID_BYTES; ++n) value |= id[n];
    return value != 0;
}

static int valid_identity(const struct ae_catalogue_identity *identity)
{
    return identity != NULL && nonzero(identity->authority_epoch) &&
        ((identity->kind == AE_CATALOGUE_SYSTEM && !nonzero(identity->owner)) ||
         (identity->kind == AE_CATALOGUE_USER && nonzero(identity->owner)));
}

static int failure(int code)
{
    switch (code & 0xff) {
    case SQLITE_OK: return 0;
    case SQLITE_NOMEM: return -ENOMEM;
    case SQLITE_FULL: return -ENOSPC;
    case SQLITE_BUSY: case SQLITE_LOCKED: return -EBUSY;
    case SQLITE_READONLY: return -EROFS;
    case SQLITE_CORRUPT: case SQLITE_NOTADB: return -EUCLEAN;
    case SQLITE_CONSTRAINT: return -EINVAL;
    default: return -EIO;
    }
}

static int exec_sql(sqlite3 *db, const char *sql)
{
    return failure(sqlite3_exec(db, sql, NULL, NULL, NULL));
}

static int scalar_int(sqlite3 *db, const char *sql, sqlite3_int64 *output)
{
    sqlite3_stmt *statement = NULL;
    int code = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
    int result = failure(code);
    if (result == 0) {
        code = sqlite3_step(statement);
        if (code != SQLITE_ROW) result = code == SQLITE_DONE ? -EUCLEAN : failure(code);
        else if (sqlite3_column_type(statement, 0) != SQLITE_INTEGER) result = -EUCLEAN;
        else {
            *output = sqlite3_column_int64(statement, 0);
            code = sqlite3_step(statement);
            if (code != SQLITE_DONE) result = code == SQLITE_ROW ? -EUCLEAN : failure(code);
        }
    }
    int final = failure(sqlite3_finalize(statement));
    return result != 0 ? result : final;
}

static int scalar_text(sqlite3 *db, const char *sql, const char *expected)
{
    sqlite3_stmt *statement = NULL;
    int result = failure(sqlite3_prepare_v2(db, sql, -1, &statement, NULL));
    if (result == 0) {
        int code = sqlite3_step(statement);
        if (code != SQLITE_ROW) result = code == SQLITE_DONE ? -EUCLEAN : failure(code);
        else if (sqlite3_column_type(statement, 0) != SQLITE_TEXT ||
                 sqlite3_column_bytes(statement, 0) != (int)strlen(expected) ||
                 memcmp(sqlite3_column_text(statement, 0), expected, strlen(expected)) != 0) result = -EUCLEAN;
        else if ((code = sqlite3_step(statement)) != SQLITE_DONE) result = code == SQLITE_ROW ? -EUCLEAN : failure(code);
    }
    int final = failure(sqlite3_finalize(statement));
    return result != 0 ? result : final;
}

static int configure(sqlite3 *db)
{
    sqlite3_limit(db, SQLITE_LIMIT_LENGTH, 1048576);
    sqlite3_limit(db, SQLITE_LIMIT_SQL_LENGTH, 1048576);
    sqlite3_limit(db, SQLITE_LIMIT_ATTACHED, 0);
    const int settings[][2] = {
        {SQLITE_DBCONFIG_ENABLE_FKEY, 1}, {SQLITE_DBCONFIG_DEFENSIVE, 1},
        {SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0}, {SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 0},
        {SQLITE_DBCONFIG_DQS_DDL, 0}, {SQLITE_DBCONFIG_DQS_DML, 0}
    };
    for (size_t n = 0; n < sizeof settings / sizeof settings[0]; ++n) {
        int actual = -1;
        int result = failure(sqlite3_db_config(db, settings[n][0], settings[n][1], &actual));
        if (result != 0) return result;
        if (actual != settings[n][1]) return -ENOTSUP;
    }
    int result = failure(sqlite3_extended_result_codes(db, 1));
    if (result == 0) result = failure(sqlite3_busy_timeout(db, 0));
    if (result == 0) result = exec_sql(db, "PRAGMA synchronous=EXTRA; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-512; PRAGMA mmap_size=0;");
    return result;
}

static int application_id(enum ae_catalogue_kind kind)
{
    return kind == AE_CATALOGUE_SYSTEM ? 1096042579 : 1096042581;
}

static int build_schema(sqlite3 *db, enum ae_catalogue_kind kind)
{
    const char *const *sql = kind == AE_CATALOGUE_SYSTEM ? ae_schema_system : ae_schema_user;
    int result = 0;
    for (size_t n = 0; sql[n] != NULL && result == 0; ++n) result = exec_sql(db, sql[n]);
    if (result == 0) result = exec_sql(db, kind == AE_CATALOGUE_SYSTEM ?
        "PRAGMA application_id=1096042579; PRAGMA user_version=2;" :
        "PRAGMA application_id=1096042581; PRAGMA user_version=2;");
    return result;
}

static int identifier(unsigned char c, int first)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
           (!first && c >= '0' && c <= '9');
}

/* Normalize formatting outside literals only. ALTER TABLE can quote identifiers.
 * Whitespace inside string data is significant and must never be collapsed.
 */
static char *canonical(const unsigned char *sql, int length)
{
    if (sql == NULL || length < 0 || length > 1048576 || memchr(sql, 0, (size_t)length) != NULL) return NULL;
    char *output = malloc((size_t)length + 1U);
    if (output == NULL) return NULL;
    size_t used = 0;
    int space = 0;
    for (int n = 0; n < length;) {
        unsigned char c = sql[n];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v') { space = 1; ++n; continue; }
        if (c == '-' && n + 1 < length && sql[n + 1] == '-') {
            while (n < length && sql[n] != '\n') ++n;
            space = 1; continue;
        }
        if (c == '/' && n + 1 < length && sql[n + 1] == '*') {
            n += 2;
            while (n + 1 < length && !(sql[n] == '*' && sql[n + 1] == '/')) ++n;
            if (n + 1 >= length) { free(output); return NULL; }
            n += 2; space = 1; continue;
        }
        if (space && used != 0) output[used++] = ' ';
        space = 0;
        if (c == '"') {
            int start = ++n;
            while (n < length && identifier(sql[n], n == start)) ++n;
            if (n == start || n >= length || sql[n] != '"') { free(output); return NULL; }
            while (start < n) output[used++] = (char)sql[start++];
            ++n;
        } else if (c == '\'') {
            output[used++] = (char)sql[n++];
            int closed = 0;
            while (n < length) {
                c = sql[n++];
                output[used++] = (char)c;
                if (c == '\'') {
                    if (n < length && sql[n] == '\'') output[used++] = (char)sql[n++];
                    else { closed = 1; break; }
                }
            }
            if (!closed) { free(output); return NULL; }
        } else output[used++] = (char)sql[n++];
    }
    output[used] = '\0';
    return output;
}

static int compare_schema(sqlite3 *db, enum ae_catalogue_kind kind)
{
    sqlite3 *reference = NULL;
    sqlite3_stmt *left = NULL, *right = NULL;
    int result = failure(sqlite3_open_v2(":memory:", &reference, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL));
    if (result == 0) result = configure(reference);
    if (result == 0) result = build_schema(reference, kind);
    const char *query = "SELECT type,name,tbl_name,sql FROM sqlite_schema WHERE sql IS NOT NULL ORDER BY type,name";
    if (result == 0) result = failure(sqlite3_prepare_v2(db, query, -1, &left, NULL));
    if (result == 0) result = failure(sqlite3_prepare_v2(reference, query, -1, &right, NULL));
    while (result == 0) {
        int a = sqlite3_step(left), b = sqlite3_step(right);
        if (a == SQLITE_DONE && b == SQLITE_DONE) break;
        if (a != SQLITE_ROW || b != SQLITE_ROW) { result = -EUCLEAN; break; }
        for (int column = 0; column < 3; ++column) {
            int length = sqlite3_column_bytes(left, column);
            const unsigned char *a_text = sqlite3_column_text(left, column);
            const unsigned char *b_text = sqlite3_column_text(right, column);
            if (length != sqlite3_column_bytes(right, column) ||
                sqlite3_column_type(left, column) != SQLITE_TEXT ||
                a_text == NULL || b_text == NULL || memcmp(a_text, b_text, (size_t)length) != 0) result = -EUCLEAN;
        }
        char *a_sql = canonical(sqlite3_column_text(left, 3), sqlite3_column_bytes(left, 3));
        char *b_sql = canonical(sqlite3_column_text(right, 3), sqlite3_column_bytes(right, 3));
        if (a_sql == NULL || b_sql == NULL || strcmp(a_sql, b_sql) != 0) result = -EUCLEAN;
        free(a_sql); free(b_sql);
    }
    int a = failure(sqlite3_finalize(left)), b = failure(sqlite3_finalize(right));
    if (result == 0) result = a != 0 ? a : b;
    sqlite3_close(reference);
    return result;
}

static int read_id(sqlite3_stmt *statement, int column, uint8_t output[AE_ID_BYTES])
{
    if (sqlite3_column_type(statement, column) != SQLITE_BLOB || sqlite3_column_bytes(statement, column) != AE_ID_BYTES) return -EUCLEAN;
    memcpy(output, sqlite3_column_blob(statement, column), AE_ID_BYTES);
    return nonzero(output) ? 0 : -EUCLEAN;
}

static int read_info(struct ae_catalogue *store, struct ae_catalogue_info *output)
{
    sqlite3_stmt *statement = NULL;
    const char *sql = "SELECT kind,authority_epoch,current_boot_id,owner_scope_ref,runtime_incarnation,catalogue_revision FROM authority_meta WHERE singleton=1";
    int result = failure(sqlite3_prepare_v2(store->db, sql, -1, &statement, NULL));
    struct ae_catalogue_info temporary = {0};
    if (result == 0) {
        int step = sqlite3_step(statement);
        if (step != SQLITE_ROW) result = step == SQLITE_DONE ? -EUCLEAN : failure(step);
        else {
            const char *kind = store->identity.kind == AE_CATALOGUE_SYSTEM ? "system" : "user";
            const unsigned char *actual = sqlite3_column_text(statement, 0);
            if (actual == NULL || sqlite3_column_bytes(statement, 0) != (int)strlen(kind) || memcmp(actual, kind, strlen(kind)) != 0) result = -ESTALE;
            if (result == 0) result = read_id(statement, 1, temporary.authority_epoch);
            if (result == 0 && memcmp(temporary.authority_epoch, store->identity.authority_epoch, AE_ID_BYTES) != 0) result = -ESTALE;
            if (result == 0) result = read_id(statement, 2, temporary.boot_id);
            if (result == 0 && store->identity.kind == AE_CATALOGUE_SYSTEM && sqlite3_column_type(statement, 3) != SQLITE_NULL) result = -ESTALE;
            if (result == 0 && store->identity.kind == AE_CATALOGUE_USER) {
                uint8_t owner[AE_ID_BYTES];
                result = read_id(statement, 3, owner);
                if (result == 0 && memcmp(owner, store->identity.owner, AE_ID_BYTES) != 0) result = -ESTALE;
            }
            if (result == 0 && sqlite3_column_type(statement, 4) != SQLITE_NULL) result = read_id(statement, 4, temporary.runtime_incarnation);
            if (result == 0) {
                temporary.revision = sqlite3_column_int64(statement, 5);
                if (sqlite3_column_type(statement, 5) != SQLITE_INTEGER || temporary.revision < 1) result = -EUCLEAN;
            }
            if (result == 0 && sqlite3_step(statement) != SQLITE_DONE) result = -EUCLEAN;
        }
    }
    int final = failure(sqlite3_finalize(statement));
    if (result == 0) result = final;
    if (result == 0) *output = temporary;
    return result;
}

static int validate(struct ae_catalogue *store)
{
    sqlite3_int64 value = 0;
    int result = scalar_int(store->db, "PRAGMA application_id", &value);
    if (result == 0 && value != application_id(store->identity.kind)) result = -ESTALE;
    if (result == 0) result = scalar_int(store->db, "PRAGMA user_version", &value);
    if (result == 0 && value != 2) result = -EPROTONOSUPPORT;
    if (result == 0) result = compare_schema(store->db, store->identity.kind);
    sqlite3_stmt *check = NULL;
    if (result == 0) result = failure(sqlite3_prepare_v2(store->db, "PRAGMA foreign_key_check", -1, &check, NULL));
    if (result == 0) {
        int code = sqlite3_step(check);
        if (code != SQLITE_DONE) result = code == SQLITE_ROW ? -EUCLEAN : failure(code);
    }
    int final = failure(sqlite3_finalize(check));
    if (result == 0) result = final;
    if (result == 0) result = scalar_text(store->db, "PRAGMA integrity_check", "ok");
    struct ae_catalogue_info info;
    if (result == 0) result = read_info(store, &info);
    return result;
}

static int safe_file(const struct stat *value)
{
    return S_ISREG(value->st_mode) && value->st_uid == geteuid() && value->st_nlink == 1 &&
           (value->st_mode & 07777) == 0600;
}

static int same_file(const struct stat *a, const struct stat *b)
{
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

static int open_relative(int root, const char *name, uint64_t flags)
{
    struct open_how how = {.flags = flags | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK,
        .mode = (flags & O_CREAT) ? 0600 : 0,
        .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_XDEV};
    int descriptor;
    do { descriptor = (int)syscall(SYS_openat2, root, name, &how, sizeof how); } while (descriptor < 0 && errno == EINTR);
    return descriptor < 0 ? -errno : descriptor;
}

static int still_named(struct ae_catalogue *store)
{
    if (store == NULL) return -EINVAL;
    if (store->process != getpid()) return -ECHILD;
    int binding = ae_root_check_path(store->root, store->directory, &store->root_identity);
    if (binding != 0) return binding;
    struct stat root, db, lock;
    if (lstat(store->directory, &root) != 0 || fstatat(store->root, "catalogue.sqlite3", &db, AT_SYMLINK_NOFOLLOW) != 0 ||
        fstatat(store->root, "writer.lock", &lock, AT_SYMLINK_NOFOLLOW) != 0) return -ESTALE;
    if (!S_ISDIR(root.st_mode) || root.st_uid != geteuid() || (root.st_mode & 07777) != 0700 ||
        !safe_file(&db) || !safe_file(&lock) || !same_file(&root, &store->root_stat) ||
        !same_file(&db, &store->database_stat) || !same_file(&lock, &store->lock_stat)) return -ESTALE;
    int moved = 1;
    int result = failure(sqlite3_file_control(store->db, "main", SQLITE_FCNTL_HAS_MOVED, &moved));
    return result != 0 ? result : (moved ? -ESTALE : 0);
}

static void destroy(struct ae_catalogue *store)
{
    if (store == NULL) return;
    sqlite3_close_v2(store->db);
    if (store->lock >= 0) close(store->lock);
    if (store->root >= 0) close(store->root);
    free(store);
}

static int connect_store(const char *directory, const struct ae_catalogue_identity *identity,
                          int create, struct ae_catalogue **output)
{
    if (!valid_identity(identity) || directory == NULL || output == NULL || directory[0] != '/' || strlen(directory) >= PATH_MAX - 32U) return -EINVAL;
    struct ae_catalogue *store = calloc(1, sizeof *store);
    if (store == NULL) return -ENOMEM;
    store->root = store->lock = -1;
    store->identity = *identity;
    store->process = getpid();
    memcpy(store->directory, directory, strlen(directory) + 1U);
    int count = snprintf(store->path, sizeof store->path, "%s/catalogue.sqlite3", directory);
    int result = count < 0 || (size_t)count >= sizeof store->path ? -EINVAL : 0;
    struct open_how how = {.flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW,
                          .resolve = RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS};
    if (result == 0) {
        store->root = (int)syscall(SYS_openat2, AT_FDCWD, directory, &how, sizeof how);
        if (store->root < 0) result = -errno;
    }
    if (result == 0 && fstat(store->root, &store->root_stat) != 0) result = -errno;
    if (result == 0) result = ae_root_observe(store->root, &store->root_identity);
    if (result == 0 && (store->root_stat.st_uid != geteuid() || (store->root_stat.st_mode & 07777) != 0700)) result = -EACCES;
    if (result == 0) {
        struct stat existing;
        if (fstatat(store->root, "writer.lock", &existing, AT_SYMLINK_NOFOLLOW) == 0) {
            if (!safe_file(&existing)) result = -EACCES;
        } else if (errno != ENOENT) result = -errno;
    }
    if (result == 0) {
        store->lock = open_relative(store->root, "writer.lock", O_RDWR | O_CREAT);
        if (store->lock < 0) result = store->lock;
    }
    if (result == 0 && !create) {
        struct stat existing;
        if (fstatat(store->root, "catalogue.sqlite3", &existing, AT_SYMLINK_NOFOLLOW) != 0) result = -errno;
        else if (!safe_file(&existing)) result = -EACCES;
    }
    if (result == 0 && fstat(store->lock, &store->lock_stat) != 0) result = -errno;
    if (result == 0 && !safe_file(&store->lock_stat)) result = -EACCES;
    if (result == 0 && flock(store->lock, LOCK_EX | LOCK_NB) != 0) result = -errno;
    const char *sidecars[] = {"catalogue.sqlite3-journal", "catalogue.sqlite3-wal", "catalogue.sqlite3-shm"};
    for (size_t n = 0; result == 0 && n < 3; ++n) {
        struct stat status;
        if (fstatat(store->root, sidecars[n], &status, AT_SYMLINK_NOFOLLOW) == 0) {
            if (!safe_file(&status) || status.st_dev != store->root_stat.st_dev) result = -EACCES;
            else if (n != 0) result = -ENOTSUP;
        } else if (errno != ENOENT) result = -errno;
    }
    if (result == 0) {
        int fd = open_relative(store->root, "catalogue.sqlite3", O_RDWR | (create ? O_CREAT | O_EXCL : 0));
        if (fd < 0) result = fd;
        else {
            if (fstat(fd, &store->database_stat) != 0) result = -errno;
            else if (!safe_file(&store->database_stat)) result = -EACCES;
            /* Close BEFORE SQLite takes POSIX locks. Never independently open/
             * close this inode while its SQLite connection is alive. */
            close(fd);
        }
    }
    if (result == 0) result = failure(sqlite3_open_v2(store->path, &store->db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_NOFOLLOW, "unix"));
    if (result == 0 && sqlite3_db_readonly(store->db, "main") != 0) result = -EROFS;
    if (result == 0) result = configure(store->db);
    if (result == 0) result = scalar_text(store->db, "PRAGMA journal_mode", "delete");
    if (result == 0) result = still_named(store);
    if (result != 0) { destroy(store); return result; }
    *output = store;
    return 0;
}

int ae_catalogue_create(const char *directory, const struct ae_catalogue_identity *identity,
                        const uint8_t boot_id[AE_ID_BYTES], struct ae_catalogue **output)
{
    if (boot_id == NULL || !nonzero(boot_id) || output == NULL) return -EINVAL;
    struct ae_catalogue *store = NULL;
    int result = connect_store(directory, identity, 1, &store);
    if (result != 0) return result;
    result = exec_sql(store->db, "BEGIN EXCLUSIVE");
    if (result == 0) result = build_schema(store->db, identity->kind);
    sqlite3_stmt *statement = NULL;
    if (result == 0) result = failure(sqlite3_prepare_v2(store->db,
        "INSERT INTO authority_meta(singleton,kind,authority_epoch,current_boot_id,owner_scope_ref) VALUES(1,?,?,?,?)", -1, &statement, NULL));
    if (result == 0) result = failure(sqlite3_bind_text(statement, 1, identity->kind == AE_CATALOGUE_SYSTEM ? "system" : "user", -1, SQLITE_STATIC));
    if (result == 0) result = failure(sqlite3_bind_blob(statement, 2, identity->authority_epoch, AE_ID_BYTES, SQLITE_TRANSIENT));
    if (result == 0) result = failure(sqlite3_bind_blob(statement, 3, boot_id, AE_ID_BYTES, SQLITE_TRANSIENT));
    if (result == 0 && identity->kind == AE_CATALOGUE_USER) result = failure(sqlite3_bind_blob(statement, 4, identity->owner, AE_ID_BYTES, SQLITE_TRANSIENT));
    if (result == 0) result = failure(sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(store->db));
    int final = failure(sqlite3_finalize(statement));
    if (result == 0) result = final;
    if (result == 0) result = validate(store);
    if (result == 0) result = exec_sql(store->db, "COMMIT");
    if (result == 0 && fsync(store->root) != 0) result = -errno;
    if (result != 0) { exec_sql(store->db, "ROLLBACK"); destroy(store); return result; }
    *output = store;
    return 0;
}

int ae_catalogue_open(const char *directory, const struct ae_catalogue_identity *identity,
                      struct ae_catalogue **output)
{
    if (output == NULL) return -EINVAL;
    struct ae_catalogue *store = NULL;
    int result = connect_store(directory, identity, 0, &store);
    if (result == 0) result = exec_sql(store->db, "BEGIN");
    if (result == 0) result = validate(store);
    if (result == 0) result = exec_sql(store->db, "COMMIT");
    if (result != 0) { if (store != NULL) exec_sql(store->db, "ROLLBACK"); destroy(store); return result; }
    *output = store;
    return 0;
}

int ae_catalogue_get_info(struct ae_catalogue *store, struct ae_catalogue_info *output)
{
    if (output == NULL) return -EINVAL;
    int result = still_named(store);
    if (result == 0) result = read_info(store, output);
    return result;
}

int ae_catalogue_begin_runtime(struct ae_catalogue *store, const uint8_t boot_id[AE_ID_BYTES],
                               const uint8_t incarnation[AE_ID_BYTES])
{
    if (boot_id == NULL || incarnation == NULL || !nonzero(boot_id) || !nonzero(incarnation)) return -EINVAL;
    int result = still_named(store);
    if (result == 0) result = exec_sql(store->db, "BEGIN IMMEDIATE");
    struct ae_catalogue_info info;
    if (result == 0) result = read_info(store, &info);
    if (result == 0 && memcmp(info.runtime_incarnation, incarnation, AE_ID_BYTES) == 0) result = -EALREADY;
    if (result == 0 && info.revision == INT64_MAX) result = -EOVERFLOW;
    sqlite3_stmt *statement = NULL;
    if (result == 0) result = failure(sqlite3_prepare_v2(store->db,
        "UPDATE authority_meta SET current_boot_id=?,runtime_incarnation=?,catalogue_revision=catalogue_revision+1 WHERE singleton=1", -1, &statement, NULL));
    if (result == 0) result = failure(sqlite3_bind_blob(statement, 1, boot_id, AE_ID_BYTES, SQLITE_TRANSIENT));
    if (result == 0) result = failure(sqlite3_bind_blob(statement, 2, incarnation, AE_ID_BYTES, SQLITE_TRANSIENT));
    if (result == 0) result = failure(sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(store->db));
    int final = failure(sqlite3_finalize(statement));
    if (result == 0) result = final;
    if (result == 0 && sqlite3_changes(store->db) != 1) result = -EUCLEAN;
    if (result == 0) result = exec_sql(store->db, "COMMIT");
    if (result != 0 && store != NULL && store->process == getpid()) exec_sql(store->db, "ROLLBACK");
    return result;
}

static int account_row(struct ae_catalogue *store, const uint8_t account[AE_ID_BYTES],
                       uint32_t uid, int64_t *revision)
{
    sqlite3_stmt *statement = NULL;
    int result = failure(sqlite3_prepare_v2(store->db,
        "SELECT kind,os_uid,state,revision FROM scopes WHERE scope_id=?", -1, &statement, NULL));
    if (result == 0) result = failure(sqlite3_bind_blob(statement, 1, account, AE_ID_BYTES, SQLITE_TRANSIENT));
    int64_t found = 0;
    if (result == 0) {
        int code = sqlite3_step(statement);
        if (code == SQLITE_DONE) result = -ENOENT;
        else if (code != SQLITE_ROW) result = failure(code);
        else if (sqlite3_column_type(statement, 0) != SQLITE_TEXT || sqlite3_column_bytes(statement, 0) != 7 ||
                 memcmp(sqlite3_column_text(statement, 0), "account", 7) != 0 ||
                 sqlite3_column_type(statement, 1) != SQLITE_INTEGER || sqlite3_column_int64(statement, 1) != (sqlite3_int64)uid ||
                 sqlite3_column_type(statement, 2) != SQLITE_TEXT || sqlite3_column_bytes(statement, 2) != 6 ||
                 memcmp(sqlite3_column_text(statement, 2), "active", 6) != 0) result = -ESTALE;
        else if (sqlite3_column_type(statement, 3) != SQLITE_INTEGER || (found = sqlite3_column_int64(statement, 3)) < 1) result = -EUCLEAN;
        else if ((code = sqlite3_step(statement)) != SQLITE_DONE) result = code == SQLITE_ROW ? -EUCLEAN : failure(code);
    }
    int final = failure(sqlite3_finalize(statement));
    if (result == 0) result = final;
    if (result == 0) *revision = found;
    return result;
}

static int account_store(struct ae_catalogue *store, const uint8_t account[AE_ID_BYTES], uint32_t uid)
{
    if (account == NULL || !nonzero(account) || uid == UINT32_MAX) return -EINVAL;
    int result = still_named(store);
    if (result == 0 && store->identity.kind != AE_CATALOGUE_SYSTEM) result = -EACCES;
    return result;
}

int ae_catalogue_check_account(struct ae_catalogue *store, const uint8_t account[AE_ID_BYTES],
                               uint32_t uid, int64_t *revision)
{
    if (revision == NULL) return -EINVAL;
    int result = account_store(store, account, uid);
    if (result == 0) result = account_row(store, account, uid, revision);
    return result;
}

int ae_catalogue_enroll_account(struct ae_catalogue *store, const uint8_t account[AE_ID_BYTES], uint32_t uid)
{
    int result = account_store(store, account, uid);
    if (result != 0) return result;
    result = exec_sql(store->db, "BEGIN IMMEDIATE");
    if (result != 0) return result;
    int64_t revision;
    result = account_row(store, account, uid, &revision);
    if (result == -ENOENT) {
        struct ae_catalogue_info info;
        result = read_info(store, &info);
        if (result == 0 && info.revision == INT64_MAX) result = -EOVERFLOW;
        sqlite3_stmt *statement = NULL;
        if (result == 0) result = failure(sqlite3_prepare_v2(store->db,
            "INSERT INTO scopes(scope_id,kind,os_uid,state) VALUES(?,'account',?,'active')", -1, &statement, NULL));
        if (result == 0) result = failure(sqlite3_bind_blob(statement, 1, account, AE_ID_BYTES, SQLITE_TRANSIENT));
        if (result == 0) result = failure(sqlite3_bind_int64(statement, 2, (sqlite3_int64)uid));
        if (result == 0) {
            int code = sqlite3_step(statement);
            result = code == SQLITE_DONE ? 0 : (code == SQLITE_CONSTRAINT_UNIQUE ? -EEXIST : failure(code));
        }
        int final = failure(sqlite3_finalize(statement));
        if (result == 0) result = final;
        if (result == 0) result = exec_sql(store->db,
            "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1");
        if (result == 0 && sqlite3_changes(store->db) != 1) result = -EUCLEAN;
    }
    if (result == 0) result = exec_sql(store->db, "COMMIT");
    if (result != 0) (void)exec_sql(store->db, "ROLLBACK");
    return result;
}

/* All SQL remains private to the owning catalogue connection. */
struct fixture_value { const void *bytes; int size, text; int64_t number; };
#define FB(value, length) ((struct fixture_value){.bytes=(value), .size=(length)})
#define FT(value) ((struct fixture_value){.bytes=(value).data, .size=(int)(value).size, .text=1})
#define FI(value) ((struct fixture_value){.size=-1, .number=(value)})
#define FIXTURE_INPUT "WITH input(plan,op,actor,retry,scope,install,generation,slot,artifact,publisher,boot,expires,accepted,scope_revision,os_uid,digest,component,version,target,authority) AS (VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20)) "

static int fixture_statement(struct ae_catalogue *store, const char *sql,
                              const struct fixture_value values[20], sqlite3_stmt **output)
{
    sqlite3_stmt *statement = NULL;
    int result = failure(sqlite3_prepare_v2(store->db, sql, -1, &statement, NULL));
    for (int n = 0; result == 0 && n < 20; ++n) {
        const struct fixture_value *value = &values[n];
        int code = value->size < 0 ? sqlite3_bind_int64(statement, n + 1, value->number) :
            value->text ? sqlite3_bind_text(statement, n + 1, value->bytes, value->size, SQLITE_TRANSIENT) :
                          sqlite3_bind_blob(statement, n + 1, value->bytes, value->size, SQLITE_TRANSIENT);
        result = failure(code);
    }
    if (result != 0) { sqlite3_finalize(statement); return result; }
    *output = statement;
    return 0;
}

static int fixture_execute(struct ae_catalogue *store, const char *sql, const struct fixture_value values[20])
{
    sqlite3_stmt *statement = NULL;
    int result = fixture_statement(store, sql, values, &statement);
    if (result == 0) {
        int code = sqlite3_step(statement);
        result = code == SQLITE_DONE ? 0 :
            (code == SQLITE_CONSTRAINT_UNIQUE || code == SQLITE_CONSTRAINT_PRIMARYKEY) ? -EEXIST : failure(code);
    }
    int final = failure(sqlite3_finalize(statement));
    return result != 0 ? result : final;
}

static int fixture_count(struct ae_catalogue *store, const char *sql, const struct fixture_value values[20], int64_t *count)
{
    sqlite3_stmt *statement = NULL;
    int result = fixture_statement(store, sql, values, &statement);
    int64_t found = 0;
    if (result == 0) {
        int code = sqlite3_step(statement);
        if (code != SQLITE_ROW) result = code == SQLITE_DONE ? -EUCLEAN : failure(code);
        else if (sqlite3_column_type(statement, 0) != SQLITE_INTEGER) result = -EUCLEAN;
        else {
            found = sqlite3_column_int64(statement, 0);
            code = sqlite3_step(statement);
            if (code != SQLITE_DONE) result = code == SQLITE_ROW ? -EUCLEAN : failure(code);
        }
    }
    int final = failure(sqlite3_finalize(statement));
    if (result == 0) result = final;
    if (result == 0) *count = found;
    return result;
}

struct fixture_encoder { uint8_t bytes[2048]; size_t used; int error; };
static void fixture_bytes(struct fixture_encoder *encoder, const void *bytes, size_t size)
{
    if (size > sizeof encoder->bytes - encoder->used) { encoder->error = -E2BIG; return; }
    memcpy(encoder->bytes + encoder->used, bytes, size); encoder->used += size;
}
static void fixture_number(struct fixture_encoder *encoder, uint64_t value, size_t width)
{
    uint8_t bytes[8];
    for (size_t n = 0; n < width; ++n) { bytes[width - n - 1U] = (uint8_t)value; value >>= 8; }
    fixture_bytes(encoder, bytes, width);
}
static void fixture_text(struct fixture_encoder *encoder, const void *bytes, size_t size)
{
    fixture_number(encoder, (uint64_t)size, 4); fixture_bytes(encoder, bytes, size);
}
static void fixture_token(struct fixture_encoder *encoder, const char *text)
{
    fixture_text(encoder, text, strlen(text));
}

static int fixture_inputs(struct ae_catalogue *store, const struct ae_fixture_request *request,
                           const uint8_t *manifest, size_t size, struct ae_fixture *fixture,
                           uint8_t digest[32], struct fixture_value values[20])
{
    if (request == NULL || manifest == NULL || request->scope_revision < 1 ||
        request->expires_boottime_ns < 0 || request->accepted_unix_ms < 0 || request->os_uid == UINT32_MAX) return -EINVAL;
    const uint8_t *ids[] = {request->plan, request->operation, request->actor, request->request_key, request->scope,
        request->install, request->generation, request->slot, request->publisher, request->boot};
    for (size_t n = 0; n < sizeof ids / sizeof ids[0]; ++n) if (!nonzero(ids[n])) return -EINVAL;
    int result = still_named(store);
    if (result == 0 && store->identity.kind != AE_CATALOGUE_SYSTEM) result = -EPERM;
    if (result == 0) result = ae_fixture_verify_manifest(manifest, size, request->artifact, fixture);
    if (result == 0 && memcmp(fixture->publisher, request->publisher, 16) != 0) result = -ESTALE;
    if (result == 0 && fixture->dependency_count != 0) result = -ENOTSUP;
    if (result != 0) return result;
    struct fixture_encoder encoder = {0};
    fixture_bytes(&encoder, "AETHINT1", 8);
    fixture_bytes(&encoder, request->plan, 16); fixture_bytes(&encoder, store->identity.authority_epoch, 16);
    fixture_bytes(&encoder, request->actor, 16); fixture_bytes(&encoder, request->scope, 16);
    fixture_token(&encoder, "install"); fixture_text(&encoder, fixture->component.data, fixture->component.size);
    fixture_number(&encoder, 0, 1); fixture_number(&encoder, 0, 1); fixture_number(&encoder, 0, 1);
    fixture_number(&encoder, 1, 1); fixture_bytes(&encoder, request->artifact, 32); fixture_number(&encoder, 0, 1);
    fixture_token(&encoder, "none"); fixture_number(&encoder, 0, 1);
    fixture_bytes(&encoder, request->boot, 16); fixture_number(&encoder, (uint64_t)request->expires_boottime_ns, 8);
    fixture_number(&encoder, 1, 4); fixture_bytes(&encoder, store->identity.authority_epoch, 16);
    fixture_token(&encoder, "scope"); fixture_text(&encoder, request->scope, 16);
    fixture_number(&encoder, (uint64_t)request->scope_revision, 8); fixture_number(&encoder, 3, 4);
    const char *kinds[] = {"install", "generation", "storage"};
    const uint8_t *targets[] = {request->install, request->generation, request->slot};
    for (size_t n = 0; n < 3; ++n) {
        fixture_number(&encoder, n, 4); fixture_token(&encoder, kinds[n]);
        fixture_text(&encoder, fixture->component.data, fixture->component.size);
        fixture_text(&encoder, targets[n], 16); fixture_token(&encoder, "create"); fixture_token(&encoder, "fixture_prepare");
    }
    result = encoder.error;
    if (result == 0) result = ae_sha256_bytes(encoder.bytes, encoder.used, digest);
    if (result == 0 && memcmp(digest, request->expected_intent, 32) != 0) result = -EBADMSG;
    if (result != 0) return result;
    const struct fixture_value prepared[20] = {
        FB(request->plan,16), FB(request->operation,16), FB(request->actor,16), FB(request->request_key,16),
        FB(request->scope,16), FB(request->install,16), FB(request->generation,16), FB(request->slot,16),
        FB(request->artifact,32), FB(request->publisher,16), FB(request->boot,16), FI(request->expires_boottime_ns),
        FI(request->accepted_unix_ms), FI(request->scope_revision), FI(request->os_uid), FB(digest,32),
        FT(fixture->component), FT(fixture->version), FT(fixture->target), FB(store->identity.authority_epoch,16)
    };
    memcpy(values, prepared, sizeof prepared);
    return 0;
}

static int fixture_scope(struct ae_catalogue *store, const struct ae_fixture_request *request)
{
    int64_t revision;
    int result = account_row(store, request->scope, request->os_uid, &revision);
    if (result == 0 && revision != request->scope_revision) result = -ESTALE;
    return result;
}

static int fixture_state(sqlite3_stmt *statement, int column, enum ae_fixture_step *step)
{
    const unsigned char *state=sqlite3_column_text(statement,column);
    if (state==NULL) return -EUCLEAN;
    if (strcmp((const char *)state,"planned")==0) *step=AE_FIXTURE_PLANNED;
    else if (strcmp((const char *)state,"executing")==0) *step=AE_FIXTURE_EXECUTING;
    else if (strcmp((const char *)state,"applied")==0) *step=AE_FIXTURE_READY;
    else if (strcmp((const char *)state,"uncertain")==0) *step=AE_FIXTURE_UNCERTAIN;
    else return -EUCLEAN;
    return 0;
}

static int fixture_row(struct ae_catalogue *store, const struct fixture_value values[20], struct ae_fixture_progress *output)
{
    int64_t count;
    int result = fixture_count(store, FIXTURE_INPUT "SELECT count(*) FROM operations o,input r WHERE o.operation_id=r.op OR o.plan_id=r.plan OR (o.actor_ref=r.actor AND o.request_key=r.retry)", values, &count);
    if (result != 0 || count == 0) return result != 0 ? result : -ENOENT;
    if (count != 1) return -EEXIST;
    sqlite3_stmt *statement = NULL;
    result = fixture_statement(store, FIXTURE_INPUT
        "SELECT s.state,s.evidence_digest,o.revision,o.phase,o.state,t.state,t.evidence_digest,a.store_state,g.publication_state,u.operation_id,u.evidence_digest,o.completed_unix_ms,"
        "i.state='active' AND i.active_generation_id=r.generation AND g.publication_state='committed' AND a.store_state='verified' FROM input r "
        "JOIN operations o ON o.operation_id=r.op AND o.plan_id=r.plan AND o.actor_ref=r.actor AND o.request_key=r.retry AND o.intent_digest=r.digest AND o.accepted_unix_ms=r.accepted "
        "JOIN plans p ON p.plan_id=r.plan AND p.authority_epoch=r.authority AND p.actor_ref=r.actor AND p.scope_ref=r.scope AND p.kind='install' AND p.component_id=r.component AND p.artifact_ref=r.artifact AND p.intent_digest=r.digest AND p.boot_id=r.boot AND p.expires_boottime_ns=r.expires AND p.state='accepted' AND p.retention='none' "
        "JOIN components c ON c.component_id=r.component AND c.publisher_ref=r.publisher AND c.kind='app' AND c.protected=0 "
        "JOIN artifacts a ON a.artifact_id=r.artifact AND a.component_id=r.component AND a.version_label=r.version AND a.target=r.target "
        "JOIN installs i ON i.install_id=r.install AND i.component_id=r.component AND i.scope_id=r.scope "
        "JOIN generations g ON g.generation_id=r.generation AND g.install_id=r.install AND g.component_id=r.component AND g.artifact_id=r.artifact "
        "JOIN operation_steps s ON s.operation_id=r.op AND s.step_no=0 AND s.action='stage' AND s.subject_kind='storage' AND s.subject_ref=r.slot "
        "LEFT JOIN operation_steps t ON t.operation_id=r.op AND t.step_no=1 AND t.action='publish' AND t.subject_kind='storage' AND t.subject_ref=r.artifact "
        "LEFT JOIN operation_steps u ON u.operation_id=r.op AND u.step_no=2 AND u.action='activate' AND u.subject_kind='generation' AND u.subject_ref=r.generation AND u.state='applied' "
        "WHERE ((u.operation_id IS NULL AND o.state IN ('accepted','running','blocked','recovering') AND o.outcome IS NULL AND o.result_kind IS NULL AND i.state='preparing' AND i.active_generation_id IS NULL AND g.activated_once=0) "
        "OR (u.operation_id IS NOT NULL AND o.state='completed' AND o.outcome='applied' AND o.result_kind='install' AND o.result_ref=r.install AND g.activated_once=1)) "
        "AND p.install_ref IS NULL AND p.generation_ref IS NULL AND p.data_epoch_ref IS NULL AND p.grant_ref IS NULL AND p.action_id IS NULL",
        values, &statement);
    struct ae_fixture_progress progress = {0};
    if (result == 0) {
        int code = sqlite3_step(statement);
        if (code != SQLITE_ROW) result = code == SQLITE_DONE ? -ESTALE : failure(code);
        else {
            result=fixture_state(statement,0,&progress.step);
            if (result==0 && sqlite3_column_type(statement,5)!=SQLITE_NULL) {
                progress.publication_present=1;
                result=fixture_state(statement,5,&progress.publication_step);
                if (result==0 && (progress.step!=AE_FIXTURE_READY || sqlite3_column_type(statement,6)!=SQLITE_BLOB ||
                    sqlite3_column_bytes(statement,6)!=32)) result=-EUCLEAN;
                if (result==0) memcpy(progress.publication_evidence,sqlite3_column_blob(statement,6),32);
            }
            int published=progress.publication_present && progress.publication_step==AE_FIXTURE_READY;
            if (result==0 && sqlite3_column_type(statement,9)!=SQLITE_NULL) {
                if (!published || sqlite3_column_type(statement,10)!=SQLITE_BLOB || sqlite3_column_bytes(statement,10)!=32 ||
                    sqlite3_column_type(statement,11)!=SQLITE_INTEGER || sqlite3_column_int64(statement,11)<0) result=-EUCLEAN;
                else {
                    progress.activation_present=1;
                    memcpy(progress.activation_evidence,sqlite3_column_blob(statement,10),32);
                    progress.completed_unix_ms=sqlite3_column_int64(statement,11);
                }
            }
            const unsigned char *phase=sqlite3_column_text(statement,3), *artifact_state=sqlite3_column_text(statement,7),
                *generation_state=sqlite3_column_text(statement,8);
            const char *expected_phase=published ? "reconcile" : progress.step==AE_FIXTURE_PLANNED ? "validate" : progress.step==AE_FIXTURE_READY ? "publish" : "stage";
            if (result==0 && (phase==NULL || artifact_state==NULL || generation_state==NULL ||
                strcmp((const char *)phase,expected_phase)!=0 ||
                (!progress.activation_present && (strcmp((const char *)artifact_state,published ? "verified" : "staged")!=0 ||
                strcmp((const char *)generation_state,published ? "committed" : "staged")!=0)))) result=-EUCLEAN;
            if (result==0) progress.installation_active=sqlite3_column_int(statement,12)==1;
            if (result == 0 && sqlite3_column_type(statement, 1) != SQLITE_NULL) {
                if (sqlite3_column_type(statement, 1) != SQLITE_BLOB || sqlite3_column_bytes(statement, 1) != 32) result = -EUCLEAN;
                else { memcpy(progress.evidence, sqlite3_column_blob(statement, 1), 32); progress.evidence_present = 1; }
            }
            if (result == 0 && progress.evidence_present != (progress.step != AE_FIXTURE_PLANNED)) result = -EUCLEAN;
            progress.revision = sqlite3_column_int64(statement, 2);
            if (progress.revision < 1) result = -EUCLEAN;
            if (result == 0 && sqlite3_step(statement) != SQLITE_DONE) result = -EUCLEAN;
        }
    }
    int final = failure(sqlite3_finalize(statement));
    if (result == 0) result = final;
    const char *checks[] = {
        FIXTURE_INPUT "SELECT count(*) FROM plan_reads p,input r WHERE p.plan_id=r.plan",
        FIXTURE_INPUT "SELECT count(*) FROM plan_reads p,input r WHERE p.plan_id=r.plan AND p.source_epoch=r.authority AND p.subject_kind='scope' AND p.subject_ref=r.scope AND p.expected_revision=r.scope_revision",
        FIXTURE_INPUT "SELECT count(*) FROM plan_effects e,input r WHERE e.plan_id=r.plan",
        FIXTURE_INPUT "SELECT count(*) FROM plan_effects e,input r WHERE e.plan_id=r.plan AND e.component_id=r.component AND e.disposition='create' AND e.reason_code='fixture_prepare' AND ((e.effect_no=0 AND e.subject_kind='install' AND e.subject_ref=r.install) OR (e.effect_no=1 AND e.subject_kind='generation' AND e.subject_ref=r.generation) OR (e.effect_no=2 AND e.subject_kind='storage' AND e.subject_ref=r.slot))",
        FIXTURE_INPUT "SELECT count(*) FROM operation_steps s,input r WHERE s.operation_id=r.op"
    };
    const int64_t expected[] = {1,1,3,3,progress.activation_present ? 3 : progress.publication_present ? 2 : 1};
    for (size_t n = 0; result == 0 && n < sizeof checks / sizeof checks[0]; ++n) {
        result = fixture_count(store, checks[n], values, &count);
        if (result == 0 && count != expected[n]) result = -ESTALE;
    }
    if (result == 0) { memcpy(progress.intent, values[15].bytes, 32); *output = progress; }
    return result;
}

static int fixture_commit(struct ae_catalogue *store, int result)
{
    if (result == 0) result = exec_sql(store->db, "COMMIT");
    if (result != 0 && !sqlite3_get_autocommit(store->db)) (void)exec_sql(store->db, "ROLLBACK");
    return result;
}

int ae_catalogue_fixture_accept(struct ae_catalogue *store, const struct ae_fixture_request *request,
                                const uint8_t *manifest, size_t size, const uint8_t current_boot[16],
                                struct ae_fixture_progress *output)
{
    if (output == NULL || current_boot == NULL || !nonzero(current_boot)) return -EINVAL;
    struct ae_fixture fixture;
    uint8_t digest[32];
    struct fixture_value values[20];
    int result = fixture_inputs(store, request, manifest, size, &fixture, digest, values);
    if (result != 0) return result;
    result = exec_sql(store->db, "BEGIN IMMEDIATE");
    if (result != 0) return result;
    result = fixture_scope(store, request);
    struct ae_fixture_progress progress;
    int absent = 0;
    if (result == 0) { result = fixture_row(store, values, &progress); absent = result == -ENOENT; }
    if (absent) {
        struct ae_catalogue_info info;
        struct timespec now;
        result = read_info(store, &info);
        if (result == 0 && (memcmp(info.boot_id, request->boot, 16) != 0 ||
            memcmp(current_boot, request->boot, 16) != 0 || info.revision == INT64_MAX)) result = -ESTALE;
        if (result == 0 && clock_gettime(CLOCK_BOOTTIME, &now) != 0) result = -errno;
        if (result == 0 && (now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000LL ||
            now.tv_sec > (INT64_MAX - now.tv_nsec) / 1000000000LL ||
            (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec >= request->expires_boottime_ns)) result = -ESTALE;
        int64_t occupied = 0;
        if (result == 0) result = fixture_count(store, FIXTURE_INPUT "SELECT count(*) FROM operation_steps s,input r WHERE s.action='stage' AND s.subject_kind='storage' AND s.subject_ref=r.slot", values, &occupied);
        if (result == 0 && occupied != 0) result = -EEXIST;
        const char *inserts[] = {
            FIXTURE_INPUT "INSERT INTO plans(plan_id,authority_epoch,actor_ref,scope_ref,kind,component_id,artifact_ref,retention,intent_digest,boot_id,expires_boottime_ns) SELECT plan,authority,actor,scope,'install',component,artifact,'none',digest,boot,expires FROM input",
            FIXTURE_INPUT "INSERT INTO plan_reads SELECT plan,authority,'scope',scope,scope_revision FROM input",
            FIXTURE_INPUT "INSERT INTO plan_effects SELECT plan,0,'install',component,install,'create','fixture_prepare' FROM input UNION ALL SELECT plan,1,'generation',component,generation,'create','fixture_prepare' FROM input UNION ALL SELECT plan,2,'storage',component,slot,'create','fixture_prepare' FROM input",
            FIXTURE_INPUT "INSERT INTO components SELECT component,publisher,'app',component,0,1 FROM input WHERE NOT EXISTS (SELECT 1 FROM components c WHERE c.component_id=input.component)",
            FIXTURE_INPUT "INSERT INTO artifacts SELECT artifact,component,version,target,'staged' FROM input",
            FIXTURE_INPUT "INSERT INTO installs(install_id,component_id,scope_id,state) SELECT install,component,scope,'preparing' FROM input",
            FIXTURE_INPUT "INSERT INTO generations(generation_id,install_id,component_id,artifact_id,publication_state) SELECT generation,install,component,artifact,'staged' FROM input",
            FIXTURE_INPUT "INSERT INTO operations(operation_id,plan_id,actor_ref,request_key,intent_digest,state,phase,accepted_unix_ms) SELECT op,plan,actor,retry,digest,'accepted','validate',accepted FROM input",
            FIXTURE_INPUT "INSERT INTO operation_steps(operation_id,step_no,action,subject_kind,subject_ref,state) SELECT op,0,'stage','storage',slot,'planned' FROM input"
        };
        for (size_t n = 0; result == 0 && n < sizeof inserts / sizeof inserts[0]; ++n) result = fixture_execute(store, inserts[n], values);
        if (result == 0) result = fixture_row(store, values, &progress);
        if (result == 0) result = exec_sql(store->db, "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1");
    }
    result = fixture_commit(store, result);
    if (result == 0) *output = progress;
    return result;
}

int ae_catalogue_fixture_transition(struct ae_catalogue *store, const struct ae_fixture_request *request,
                                    const uint8_t *manifest, size_t size, enum ae_fixture_step step,
                                    const uint8_t evidence[32], struct ae_fixture_progress *output)
{
    if (output == NULL || evidence == NULL || step < AE_FIXTURE_EXECUTING || step > AE_FIXTURE_UNCERTAIN) return -EINVAL;
    struct ae_fixture fixture;
    uint8_t digest[32];
    struct fixture_value values[20];
    int result = fixture_inputs(store, request, manifest, size, &fixture, digest, values);
    if (result != 0) return result;
    result = exec_sql(store->db, "BEGIN IMMEDIATE");
    if (result != 0) return result;
    result = fixture_scope(store, request);
    struct ae_fixture_progress progress;
    if (result == 0) result = fixture_row(store, values, &progress);
    if (result == 0 && progress.publication_present) result = -EBUSY;
    if (result == 0 && progress.evidence_present && memcmp(progress.evidence, evidence, 32) != 0) result = -ESTALE;
    if (result == 0 && ((progress.step == AE_FIXTURE_PLANNED && step != AE_FIXTURE_EXECUTING) ||
        (progress.step == AE_FIXTURE_READY && step == AE_FIXTURE_EXECUTING))) result = -EINVAL;
    if (result == 0 && progress.step != step) {
        struct ae_catalogue_info info;
        result = read_info(store, &info);
        if (result == 0 && (info.revision == INT64_MAX || progress.revision == INT64_MAX)) result = -EOVERFLOW;
        sqlite3_stmt *statement = NULL;
        const char *state = step == AE_FIXTURE_EXECUTING ? "executing" : step == AE_FIXTURE_READY ? "applied" : "uncertain";
        if (result == 0) result = failure(sqlite3_prepare_v2(store->db, "UPDATE operation_steps SET state=?,evidence_digest=? WHERE operation_id=? AND step_no=0", -1, &statement, NULL));
        if (result == 0) result = failure(sqlite3_bind_text(statement, 1, state, -1, SQLITE_STATIC));
        if (result == 0) result = failure(sqlite3_bind_blob(statement, 2, evidence, 32, SQLITE_TRANSIENT));
        if (result == 0) result = failure(sqlite3_bind_blob(statement, 3, request->operation, 16, SQLITE_TRANSIENT));
        if (result == 0) result = failure(sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(store->db));
        int final = failure(sqlite3_finalize(statement));
        if (result == 0) result = final;
        if (result == 0) result = fixture_execute(store, step == AE_FIXTURE_READY ?
            FIXTURE_INPUT "UPDATE operations SET state='running',phase='publish',reason_code='',revision=revision+1 WHERE operation_id=(SELECT op FROM input)" : step == AE_FIXTURE_UNCERTAIN ?
            FIXTURE_INPUT "UPDATE operations SET state='blocked',phase='stage',reason_code='fixture_stage_uncertain',revision=revision+1 WHERE operation_id=(SELECT op FROM input)" :
            FIXTURE_INPUT "UPDATE operations SET state='running',phase='stage',reason_code='',revision=revision+1 WHERE operation_id=(SELECT op FROM input)", values);
        if (result == 0) result = exec_sql(store->db, "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1");
        if (result == 0) result = fixture_row(store, values, &progress);
    }
    result = fixture_commit(store, result);
    if (result == 0) *output = progress;
    return result;
}

int ae_catalogue_fixture_get(struct ae_catalogue *store, const struct ae_fixture_request *request,
                             const uint8_t *manifest,size_t size,struct ae_fixture_progress *output)
{
    if (output==NULL) return -EINVAL;
    struct ae_fixture fixture;
    uint8_t digest[32];
    struct fixture_value values[20];
    int result=fixture_inputs(store,request,manifest,size,&fixture,digest,values);
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN");
    if (result!=0) return result;
    struct ae_fixture_progress progress;
    result=fixture_scope(store,request);
    if (result==0) result=fixture_row(store,values,&progress);
    result=fixture_commit(store,result);
    if (result==0) *output=progress;
    return result;
}

int ae_catalogue_fixture_publication(struct ae_catalogue *store,const struct ae_fixture_request *request,
                                     const uint8_t *manifest,size_t size,enum ae_fixture_step step,
                                     const uint8_t evidence[32],struct ae_fixture_progress *output)
{
    if (output==NULL || evidence==NULL || step<AE_FIXTURE_PLANNED || step>AE_FIXTURE_UNCERTAIN) return -EINVAL;
    struct ae_fixture fixture;
    uint8_t digest[32];
    struct fixture_value values[20];
    int result=fixture_inputs(store,request,manifest,size,&fixture,digest,values);
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE");
    if (result!=0) return result;
    struct ae_fixture_progress progress;
    result=fixture_scope(store,request);
    if (result==0) result=fixture_row(store,values,&progress);
    if (result==0 && progress.step!=AE_FIXTURE_READY) result=-EINVAL;
    if (result==0 && progress.publication_present && memcmp(progress.publication_evidence,evidence,32)!=0) result=-ESTALE;
    if (result==0 && ((!progress.publication_present && step!=AE_FIXTURE_PLANNED) ||
        (progress.publication_present && progress.publication_step!=step &&
            (step==AE_FIXTURE_PLANNED || progress.publication_step==AE_FIXTURE_READY ||
             (progress.publication_step==AE_FIXTURE_PLANNED && step!=AE_FIXTURE_EXECUTING))))) result=-EINVAL;
    int change=result==0 && (!progress.publication_present || progress.publication_step!=step);
    if (change) {
        struct ae_catalogue_info info;
        result=read_info(store,&info);
        if (result==0 && (info.revision==INT64_MAX || progress.revision==INT64_MAX)) result=-EOVERFLOW;
        if (result==0 && !progress.publication_present) result=fixture_execute(store,FIXTURE_INPUT
            "INSERT INTO operation_steps(operation_id,step_no,action,subject_kind,subject_ref,state) SELECT op,1,'publish','storage',artifact,'planned' FROM input",values);
        sqlite3_stmt *statement=NULL;
        const char *state=step==AE_FIXTURE_PLANNED ? "planned" : step==AE_FIXTURE_EXECUTING ? "executing" : step==AE_FIXTURE_READY ? "applied" : "uncertain";
        if (result==0) result=failure(sqlite3_prepare_v2(store->db,"UPDATE operation_steps SET state=?,evidence_digest=? WHERE operation_id=? AND step_no=1",-1,&statement,NULL));
        if (result==0) result=failure(sqlite3_bind_text(statement,1,state,-1,SQLITE_STATIC));
        if (result==0) result=failure(sqlite3_bind_blob(statement,2,evidence,32,SQLITE_TRANSIENT));
        if (result==0) result=failure(sqlite3_bind_blob(statement,3,request->operation,16,SQLITE_TRANSIENT));
        if (result==0) result=failure(sqlite3_step(statement)==SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(store->db));
        int final=failure(sqlite3_finalize(statement));
        if (result==0) result=final;
        if (result==0 && step==AE_FIXTURE_READY) result=fixture_execute(store,FIXTURE_INPUT
            "UPDATE artifacts SET store_state='verified' WHERE artifact_id=(SELECT artifact FROM input)",values);
        if (result==0 && step==AE_FIXTURE_READY) result=fixture_execute(store,FIXTURE_INPUT
            "UPDATE generations SET publication_state='committed',revision=revision+1 WHERE generation_id=(SELECT generation FROM input)",values);
        if (result==0) result=fixture_execute(store,step==AE_FIXTURE_READY ? FIXTURE_INPUT
            "UPDATE operations SET state='running',phase='reconcile',reason_code='activation_pending',revision=revision+1 WHERE operation_id=(SELECT op FROM input)" : step==AE_FIXTURE_UNCERTAIN ? FIXTURE_INPUT
            "UPDATE operations SET state='blocked',phase='publish',reason_code='fixture_publish_uncertain',revision=revision+1 WHERE operation_id=(SELECT op FROM input)" : FIXTURE_INPUT
            "UPDATE operations SET state='running',phase='publish',reason_code='',revision=revision+1 WHERE operation_id=(SELECT op FROM input)",values);
        if (result==0) result=exec_sql(store->db,"UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1");
        if (result==0) result=fixture_row(store,values,&progress);
    }
    result=fixture_commit(store,result);
    if (result==0) *output=progress;
    return result;
}

#define ACTIVATION_INPUT FIXTURE_INPUT ", activation(install_revision,generation_revision,gate_revision,stamp,evidence) AS (VALUES(?21,?22,?23,?24,?25)) "

static int activation_statement(struct ae_catalogue *store,const char *sql,const struct fixture_value values[20],
                                  const struct ae_fixture_activation *activation,sqlite3_stmt **output)
{
    sqlite3_stmt *statement=NULL;
    int result=fixture_statement(store,sql,values,&statement);
    if (result==0) result=failure(sqlite3_bind_int64(statement,21,activation->install_revision));
    if (result==0) result=failure(sqlite3_bind_int64(statement,22,activation->generation_revision));
    if (result==0) result=failure(sqlite3_bind_int64(statement,23,activation->gate_revision));
    if (result==0) result=failure(sqlite3_bind_int64(statement,24,activation->completed_unix_ms));
    if (result==0) result=failure(sqlite3_bind_blob(statement,25,activation->expected_evidence,32,SQLITE_TRANSIENT));
    if (result!=0) { sqlite3_finalize(statement); return result; }
    *output=statement;
    return 0;
}

static int activation_guard(struct ae_catalogue *store,const struct fixture_value values[20],
                              const struct ae_fixture_activation *activation)
{
    sqlite3_stmt *statement=NULL;
    int result=activation_statement(store,ACTIVATION_INPUT
        "SELECT i.revision=x.install_revision,g.revision=x.generation_revision,f.revision=x.gate_revision,f.state='closed' AND f.closing_operation_id IS NULL,"
        "(SELECT count(*) FROM generation_leases l WHERE l.generation_id=r.generation AND l.state<>'released'),"
        "(SELECT count(*) FROM generation_edges e WHERE e.consumer_id=r.generation OR e.provider_id=r.generation),"
        "(SELECT count(*) FROM launch_dispatches d WHERE d.generation_ref=r.generation) "
        "FROM input r,activation x JOIN installs i ON i.install_id=r.install JOIN generations g ON g.generation_id=r.generation "
        "JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=r.install",values,activation,&statement);
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -ESTALE : failure(code);
        for (int column=0;result==0 && column<7;++column) {
            if (sqlite3_column_type(statement,column)!=SQLITE_INTEGER) result=-EUCLEAN;
            else if (sqlite3_column_int64(statement,column)!=(column<4 ? 1 : 0)) result=column<3 ? -ESTALE : -EBUSY;
        }
        if (result==0 && sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN;
    }
    int final=failure(sqlite3_finalize(statement));
    return result!=0 ? result : final;
}

int ae_catalogue_fixture_activate(struct ae_catalogue *store,const struct ae_fixture_request *request,
                                  const uint8_t *manifest,size_t size,const struct ae_fixture_activation *activation,
                                  const uint8_t current_boot[16],struct ae_fixture_progress *output)
{
    if (output==NULL || current_boot==NULL || !nonzero(current_boot) || activation==NULL ||
        activation->install_revision<1 || activation->generation_revision<1 || activation->gate_revision<1 ||
        activation->completed_unix_ms<0) return -EINVAL;
    if (activation->install_revision==INT64_MAX || activation->generation_revision==INT64_MAX) return -EOVERFLOW;
    struct fixture_encoder encoder={0};
    fixture_bytes(&encoder,"AETHACT1",8); fixture_bytes(&encoder,activation->publication_evidence,32);
    fixture_number(&encoder,(uint64_t)activation->install_revision,8);
    fixture_number(&encoder,(uint64_t)activation->generation_revision,8);
    fixture_number(&encoder,(uint64_t)activation->gate_revision,8);
    fixture_number(&encoder,(uint64_t)activation->completed_unix_ms,8);
    uint8_t evidence[32];
    int result=encoder.error;
    if (result==0) result=ae_sha256_bytes(encoder.bytes,encoder.used,evidence);
    if (result==0 && memcmp(evidence,activation->expected_evidence,32)!=0) result=-EBADMSG;
    struct ae_fixture fixture;
    uint8_t digest[32];
    struct fixture_value values[20];
    if (result==0) result=fixture_inputs(store,request,manifest,size,&fixture,digest,values);
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE");
    if (result!=0) return result;
    struct ae_fixture_progress progress;
    result=fixture_scope(store,request);
    if (result==0) result=fixture_row(store,values,&progress);
    if (result==0 && (!progress.publication_present || progress.publication_step!=AE_FIXTURE_READY)) result=-EINVAL;
    if (result==0 && memcmp(progress.publication_evidence,activation->publication_evidence,32)!=0) result=-ESTALE;
    if (result==0 && progress.activation_present && (memcmp(progress.activation_evidence,evidence,32)!=0 ||
        progress.completed_unix_ms!=activation->completed_unix_ms)) result=-ESTALE;
    if (result==0 && !progress.activation_present) {
        struct ae_catalogue_info info;
        result=read_info(store,&info);
        if (result==0 && (memcmp(info.boot_id,current_boot,16)!=0 || !nonzero(info.runtime_incarnation))) result=-ESTALE;
        if (result==0 && (info.revision==INT64_MAX || progress.revision==INT64_MAX)) result=-EOVERFLOW;
        if (result==0) result=activation_guard(store,values,activation);
        const char *statements[]={
            ACTIVATION_INPUT "INSERT INTO operation_steps(operation_id,step_no,action,subject_kind,subject_ref,state,evidence_digest) SELECT r.op,2,'activate','generation',r.generation,'applied',x.evidence FROM input r,activation x",
            ACTIVATION_INPUT "UPDATE installs SET state='active',active_generation_id=(SELECT generation FROM input),revision=revision+1 WHERE install_id=(SELECT install FROM input)",
            ACTIVATION_INPUT "UPDATE operations SET state='completed',phase='reconcile',reason_code='',outcome='applied',completed_unix_ms=(SELECT stamp FROM activation),result_kind='install',result_ref=(SELECT install FROM input),revision=revision+1 WHERE operation_id=(SELECT op FROM input)"
        };
        for (size_t n=0;result==0 && n<sizeof statements/sizeof statements[0];++n) {
            sqlite3_stmt *statement=NULL;
            result=activation_statement(store,statements[n],values,activation,&statement);
            if (result==0) {
                int code=sqlite3_step(statement);
                result=code==SQLITE_DONE ? 0 : failure(code);
                if (result==0 && sqlite3_changes(store->db)!=1) result=-EUCLEAN;
            }
            int final=failure(sqlite3_finalize(statement));
            if (result==0) result=final;
        }
        if (result==0) result=exec_sql(store->db,"UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1");
        if (result==0) result=fixture_row(store,values,&progress);
    }
    result=fixture_commit(store,result);
    if (result==0) *output=progress;
    return result;
}

#undef ACTIVATION_INPUT
#undef FIXTURE_INPUT
#undef FB
#undef FT
#undef FI

int ae_catalogue_close(struct ae_catalogue **output)
{
    if (output == NULL) return -EINVAL;
    struct ae_catalogue *store = *output;
    if (store == NULL) return 0;
    if (store->process != getpid()) return -ECHILD;
    int result = failure(sqlite3_close(store->db));
    if (result != 0) return result;
    store->db = NULL;
    destroy(store);
    *output = NULL;
    return 0;
}
