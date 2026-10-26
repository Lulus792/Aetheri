#include "catalogue/store.h"
#include "catalogue/schema.h"

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

struct ae_catalogue {
    sqlite3 *db;
    struct ae_catalogue_identity identity;
    pid_t process;
    int root, lock;
    struct stat root_stat, database_stat, lock_stat;
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
