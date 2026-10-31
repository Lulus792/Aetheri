#include "catalogue/store.h"
#include "catalogue/schema.h"
#include "catalogue/projection.h"
#include "catalogue/data.h"
#include "catalogue/reservation.h"
#include "catalogue/launch.h"
#include "catalogue/setup.h"
#include "catalogue/execution.h"
#include "catalogue/lifetime.h"
#include "catalogue/handoff.h"
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

static int extend_schema_v3(sqlite3 *db,enum ae_catalogue_kind kind)
{
    const char *const *sql=kind==AE_CATALOGUE_SYSTEM ? ae_schema_system_v3 : ae_schema_user_v3;
    int r=0;
    for (size_t n=0;r==0 && sql[n]!=NULL;++n) r=exec_sql(db,sql[n]);
    return r==0 ? exec_sql(db,"PRAGMA user_version=3") : r;
}
static int extend_schema_v4(sqlite3 *db,enum ae_catalogue_kind kind)
{
    const char *const *sql=kind==AE_CATALOGUE_SYSTEM ? ae_schema_system_v4 : ae_schema_user_v4;
    int r=0;
    for (size_t n=0;r==0 && sql[n]!=NULL;++n) r=exec_sql(db,sql[n]);
    return r==0 ? exec_sql(db,"PRAGMA user_version=4") : r;
}
static int build_schema(sqlite3 *db, enum ae_catalogue_kind kind,int version)
{
    const char *const *sql = kind == AE_CATALOGUE_SYSTEM ? ae_schema_system : ae_schema_user;
    int result = 0;
    for (size_t n = 0; sql[n] != NULL && result == 0; ++n) result = exec_sql(db, sql[n]);
    if (result == 0) result = exec_sql(db, kind == AE_CATALOGUE_SYSTEM ?
        "PRAGMA application_id=1096042579; PRAGMA user_version=2;" :
        "PRAGMA application_id=1096042581; PRAGMA user_version=2;");
    if (result==0 && version>=3) result=extend_schema_v3(db,kind);
    if (result==0 && version>=4) result=extend_schema_v4(db,kind);
    if (result==0 && version>=5) {
        if (kind!=AE_CATALOGUE_SYSTEM) return -EPROTONOSUPPORT;
        for (size_t n=0;result==0 && ae_schema_system_v5[n];++n) result=exec_sql(db,ae_schema_system_v5[n]);
        if (result==0) result=exec_sql(db,"PRAGMA user_version=5");
    }
    if (result==0 && version==6) {
        for (size_t n=0;result==0 && ae_schema_system_v6[n];++n) result=exec_sql(db,ae_schema_system_v6[n]);
        if (result==0) result=exec_sql(db,"PRAGMA user_version=6");
    }
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

static int compare_schema(sqlite3 *db, enum ae_catalogue_kind kind,int version)
{
    sqlite3 *reference = NULL;
    sqlite3_stmt *left = NULL, *right = NULL;
    int result = failure(sqlite3_open_v2(":memory:", &reference, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL));
    if (result == 0) result = configure(reference);
    if (result == 0) result = build_schema(reference, kind,version);
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
    if (result == 0 && value != 2 && value != 3 && value != 4 && !((value==5 || value==6) && store->identity.kind==AE_CATALOGUE_SYSTEM)) result = -EPROTONOSUPPORT;
    if (result == 0) result = compare_schema(store->db, store->identity.kind,(int)value);
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
    if (result == 0) result = build_schema(store->db, identity->kind,2);
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
    /* A new user broker must reconcile accepted runs; a previous process state
     * or a reserved root lease is not evidence of a live/current application.
     * Keep the original tuple, readiness and remote receipt for recovery. */
    if (result == 0 && store->identity.kind == AE_CATALOGUE_USER) result = exec_sql(store->db,
        "UPDATE launches SET state='recovery_required',revision=revision+1 "
        "WHERE state NOT IN ('exited','start_failed','recovery_required')");
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

int ae_catalogue_upgrade_lifetimes(struct ae_catalogue *store)
{
    int r=still_named(store);
    if (r==0) r=exec_sql(store->db,"BEGIN EXCLUSIVE");
    if (r!=0) return r;
    r=validate(store);
    sqlite3_int64 version=0;
    if (r==0) r=scalar_int(store->db,"PRAGMA user_version",&version);
    if (r==0 && version==2) r=extend_schema_v3(store->db,store->identity.kind);
    if (r==0) r=validate(store);
    return fixture_commit(store,r);
}
int ae_catalogue_upgrade_processes(struct ae_catalogue *store)
{
    int r=still_named(store);
    if (r==0) r=exec_sql(store->db,"BEGIN EXCLUSIVE");
    if (r!=0) return r;
    r=validate(store); sqlite3_int64 version=0;
    if (r==0) r=scalar_int(store->db,"PRAGMA user_version",&version);
    if (r==0 && version!=3 && version!=4) r=-EPROTONOSUPPORT;
    if (r==0 && version==3) r=extend_schema_v4(store->db,store->identity.kind);
    if (r==0) r=validate(store);
    return fixture_commit(store,r);
}
int ae_catalogue_upgrade_handoffs(struct ae_catalogue *store)
{
    int r=still_named(store);
    if (r==0 && store->identity.kind!=AE_CATALOGUE_SYSTEM) r=-EPROTONOSUPPORT;
    if (r==0) r=exec_sql(store->db,"BEGIN EXCLUSIVE");
    if (r!=0) return r;
    r=validate(store); sqlite3_int64 version=0;
    if (r==0) r=scalar_int(store->db,"PRAGMA user_version",&version);
    if (r==0 && version!=4 && version!=5) r=-EPROTONOSUPPORT;
    if (r==0 && version==4) {
        for (size_t n=0;r==0 && ae_schema_system_v5[n];++n) r=exec_sql(store->db,ae_schema_system_v5[n]);
        if (r==0) r=exec_sql(store->db,"PRAGMA user_version=5");
    }
    if (r==0) r=validate(store);
    return fixture_commit(store,r);
}

int ae_catalogue_upgrade_recovery(struct ae_catalogue *store)
{
    int r=still_named(store);
    if (r==0 && store->identity.kind!=AE_CATALOGUE_SYSTEM) r=-EPROTONOSUPPORT;
    if (r==0) r=exec_sql(store->db,"BEGIN EXCLUSIVE");
    if (r!=0) return r;
    r=validate(store); sqlite3_int64 version=0;
    if (r==0) r=scalar_int(store->db,"PRAGMA user_version",&version);
    if (r==0 && version!=5 && version!=6) r=-EPROTONOSUPPORT;
    if (r==0 && version==5) {
        for (size_t n=0;r==0 && ae_schema_system_v6[n];++n) r=exec_sql(store->db,ae_schema_system_v6[n]);
        if (r==0) r=exec_sql(store->db,"PRAGMA user_version=6");
    }
    if (r==0) r=validate(store);
    return fixture_commit(store,r);
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

static int projection_id(const uint8_t id[16])
{
    return id!=NULL && (id[6]>>4)==4 && (id[8]&0xc0)==0x80;
}

int ae_install_snapshot_valid(const struct ae_install_snapshot *snapshot)
{
    if (snapshot==NULL) return -EINVAL;
    const uint8_t *ids[]={snapshot->system_epoch,snapshot->system_runtime,snapshot->boot,snapshot->install,
        snapshot->publisher,snapshot->account,snapshot->scope,snapshot->generation};
    for (size_t n=0;n<sizeof ids/sizeof ids[0];++n) if (!projection_id(ids[n])) return -EBADMSG;
    size_t size=strnlen(snapshot->component,sizeof snapshot->component);
    if (size==0 || size>=sizeof snapshot->component) return -EBADMSG;
    for (size_t n=0;n<size;++n) {
        unsigned char c=(unsigned char)snapshot->component[n];
        if (!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-')) return -EBADMSG;
    }
    if (snapshot->install_revision<1 || snapshot->generation_revision<1 || snapshot->gate_revision<1 ||
        snapshot->catalogue_revision<1 || snapshot->gate<AE_INSTALL_GATE_OPEN || snapshot->gate>AE_INSTALL_GATE_CLOSED) return -EBADMSG;
    return 0;
}

static int projection_blob(sqlite3_stmt *statement,int column,uint8_t *output,int size)
{
    if (sqlite3_column_type(statement,column)!=SQLITE_BLOB || sqlite3_column_bytes(statement,column)!=size) return -EUCLEAN;
    memcpy(output,sqlite3_column_blob(statement,column),(size_t)size);
    return 0;
}

int ae_catalogue_resolve_app(struct ae_catalogue *store,const uint8_t account[16],uint32_t os_uid,
                             const uint8_t install[16],const uint8_t boot[16],const uint8_t runtime[16],
                             struct ae_install_snapshot *output)
{
    if (output==NULL || !projection_id(account) || !projection_id(install) || !projection_id(boot) ||
        !projection_id(runtime) || os_uid==UINT32_MAX) return -EINVAL;
    int result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_SYSTEM) result=-EPERM;
    if (result==0) result=exec_sql(store->db,"BEGIN");
    if (result!=0) return result;
    struct ae_catalogue_info info;
    int64_t account_revision;
    result=read_info(store,&info);
    if (result==0 && (memcmp(info.boot_id,boot,16)!=0 || memcmp(info.runtime_incarnation,runtime,16)!=0)) result=-ESTALE;
    if (result==0) result=account_row(store,account,os_uid,&account_revision);
    sqlite3_stmt *statement=NULL;
    if (result==0) result=failure(sqlite3_prepare_v2(store->db,
        "SELECT c.component_id,c.publisher_ref,i.scope_id,i.revision,g.generation_id,g.artifact_id,g.revision,f.state,f.revision "
        "FROM installs i JOIN scopes s ON s.scope_id=i.scope_id JOIN components c ON c.component_id=i.component_id "
        "JOIN generations g ON g.generation_id=i.active_generation_id AND g.install_id=i.install_id AND g.component_id=i.component_id "
        "JOIN artifacts a ON a.artifact_id=g.artifact_id AND a.component_id=i.component_id "
        "JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=i.install_id "
        "WHERE i.install_id=?1 AND i.state='active' AND c.kind='app' AND s.state='active' "
        "AND (s.kind='machine' OR (s.kind='account' AND s.scope_id=?2)) "
        "AND g.publication_state='committed' AND g.activated_once=1 AND a.store_state='verified'",-1,&statement,NULL));
    if (result==0) result=failure(sqlite3_bind_blob(statement,1,install,16,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,2,account,16,SQLITE_TRANSIENT));
    struct ae_install_snapshot snapshot={0};
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -ENOENT : failure(code);
        else {
            int length=sqlite3_column_bytes(statement,0);
            if (sqlite3_column_type(statement,0)!=SQLITE_TEXT || length<1 || length>128 ||
                memchr(sqlite3_column_text(statement,0),0,(size_t)length)!=NULL) result=-EUCLEAN;
            else memcpy(snapshot.component,sqlite3_column_text(statement,0),(size_t)length);
            if (result==0) result=projection_blob(statement,1,snapshot.publisher,16);
            if (result==0) result=projection_blob(statement,2,snapshot.scope,16);
            if (result==0) result=projection_blob(statement,4,snapshot.generation,16);
            if (result==0) result=projection_blob(statement,5,snapshot.artifact,32);
            snapshot.install_revision=sqlite3_column_int64(statement,3);
            snapshot.generation_revision=sqlite3_column_int64(statement,6);
            snapshot.gate_revision=sqlite3_column_int64(statement,8);
            const unsigned char *gate=sqlite3_column_text(statement,7);
            if (gate==NULL) result=-EUCLEAN;
            else if (strcmp((const char *)gate,"open")==0) snapshot.gate=AE_INSTALL_GATE_OPEN;
            else if (strcmp((const char *)gate,"closing")==0) snapshot.gate=AE_INSTALL_GATE_CLOSING;
            else if (strcmp((const char *)gate,"closed")==0) snapshot.gate=AE_INSTALL_GATE_CLOSED;
            else result=-EUCLEAN;
            if (result==0 && sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN;
        }
    }
    int final=failure(sqlite3_finalize(statement));
    if (result==0) result=final;
    if (result==0) {
        memcpy(snapshot.system_epoch,info.authority_epoch,16); memcpy(snapshot.system_runtime,info.runtime_incarnation,16);
        memcpy(snapshot.boot,info.boot_id,16); memcpy(snapshot.install,install,16); memcpy(snapshot.account,account,16);
        snapshot.catalogue_revision=info.revision;
        result=ae_install_snapshot_valid(&snapshot);
    }
    result=fixture_commit(store,result);
    if (result==0) *output=snapshot;
    return result;
}

int ae_catalogue_cache_install(struct ae_catalogue *store,const struct ae_install_snapshot *snapshot,
                               const struct ae_projection_context *context,int64_t *local_revision)
{
    if (local_revision==NULL || context==NULL || !projection_id(context->system_epoch) ||
        !projection_id(context->system_runtime) || !projection_id(context->boot) || !projection_id(context->user_runtime)) return -EINVAL;
    int result=ae_install_snapshot_valid(snapshot);
    if (result==0) result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_USER) result=-EPERM;
    if (result==0 && (memcmp(snapshot->account,store->identity.owner,16)!=0 || memcmp(snapshot->system_epoch,context->system_epoch,16)!=0 ||
        memcmp(snapshot->system_runtime,context->system_runtime,16)!=0 || memcmp(snapshot->boot,context->boot,16)!=0)) result=-ESTALE;
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE");
    if (result!=0) return result;
    struct ae_catalogue_info info;
    result=read_info(store,&info);
    if (result==0 && (memcmp(info.boot_id,context->boot,16)!=0 || memcmp(info.runtime_incarnation,context->user_runtime,16)!=0)) result=-ESTALE;
    sqlite3_stmt *statement=NULL;
    if (result==0) result=failure(sqlite3_prepare_v2(store->db,
        "SELECT component_id=?2 AND publisher_ref=?3 AND owner_scope=?4 AND system_epoch=?5 AND install_revision=?6 AND state='active' FROM install_bindings WHERE install_id=?1",-1,&statement,NULL));
    const void *blobs[]={snapshot->install,snapshot->publisher,snapshot->account,snapshot->system_epoch};
    const int indices[]={1,3,4,5};
    for (size_t n=0;result==0 && n<4;++n) result=failure(sqlite3_bind_blob(statement,indices[n],blobs[n],16,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_text(statement,2,snapshot->component,-1,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_int64(statement,6,snapshot->install_revision));
    int absent=0;
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code==SQLITE_DONE) absent=1;
        else if (code!=SQLITE_ROW) result=failure(code);
        else if (sqlite3_column_int(statement,0)!=1) result=-ESTALE;
        else if (sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN;
    }
    int final=failure(sqlite3_finalize(statement));
    if (result==0) result=final;
    if (result==0 && absent) {
        if (info.revision==INT64_MAX) result=-EOVERFLOW;
        statement=NULL;
        if (result==0) result=failure(sqlite3_prepare_v2(store->db,
            "INSERT INTO install_bindings VALUES(?1,?2,?3,?4,?5,?6,'active')",-1,&statement,NULL));
        for (size_t n=0;result==0 && n<4;++n) result=failure(sqlite3_bind_blob(statement,indices[n],blobs[n],16,SQLITE_TRANSIENT));
        if (result==0) result=failure(sqlite3_bind_text(statement,2,snapshot->component,-1,SQLITE_TRANSIENT));
        if (result==0) result=failure(sqlite3_bind_int64(statement,6,snapshot->install_revision));
        if (result==0) {
            int code=sqlite3_step(statement);
            result=code==SQLITE_DONE ? 0 : failure(code);
        }
        final=failure(sqlite3_finalize(statement));
        if (result==0) result=final;
        if (result==0) result=exec_sql(store->db,"UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1");
        if (result==0) ++info.revision;
    }
    result=fixture_commit(store,result);
    if (result==0) *local_revision=info.revision;
    return result;
}

#define DATA_INPUT "WITH r(i,p,a,s,d,se,ue,ir,base,version) AS (VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)) "
static int data_statement(struct ae_catalogue *store,const struct ae_data_enrollment *request,const char *sql,sqlite3_stmt **output)
{
    sqlite3_stmt *statement=NULL;
    int result=failure(sqlite3_prepare_v2(store->db,sql,-1,&statement,NULL));
    const void *ids[]={request->install,request->publisher,request->owner.id[AE_ACCOUNT],request->owner.id[AE_STORAGE],
        request->owner.id[AE_DATA],request->system_epoch,request->user_epoch};
    for (int n=0;result==0 && n<7;++n) result=failure(sqlite3_bind_blob(statement,n+1,ids[n],16,SQLITE_TRANSIENT));
    const int64_t numbers[]={request->install_revision,request->base_revision,request->schema_version};
    for (int n=0;result==0 && n<3;++n) result=failure(sqlite3_bind_int64(statement,n+8,numbers[n]));
    if (result!=0) { sqlite3_finalize(statement); return result; }
    *output=statement;
    return 0;
}

int ae_catalogue_enroll_data(struct ae_catalogue *store,const struct ae_data_enrollment *request,
                             const uint8_t current_boot[16],int64_t *revision)
{
    if (revision==NULL || !projection_id(current_boot)) return -EINVAL;
    int result=ae_data_enrollment_valid(request);
    if (result==0) result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_USER) result=-EPERM;
    if (result==0 && (memcmp(request->owner.id[AE_ACCOUNT],store->identity.owner,16)!=0 ||
        memcmp(request->user_epoch,store->identity.authority_epoch,16)!=0)) result=-ESTALE;
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE");
    if (result!=0) return result;
    struct ae_catalogue_info info;
    result=read_info(store,&info);
    sqlite3_stmt *statement=NULL;
    if (result==0) result=data_statement(store,request,DATA_INPUT
        "SELECT (SELECT count(*) FROM install_bindings b WHERE b.install_id=r.i AND b.publisher_ref=r.p AND b.owner_scope=r.a AND b.system_epoch=r.se AND b.install_revision=r.ir AND b.state='active'),"
        "(SELECT count(*) FROM data_epochs d WHERE d.data_epoch_id=r.d OR d.storage_ref=r.s),"
        "(SELECT count(*) FROM attachments a WHERE a.install_id=r.i OR a.data_epoch_id=r.d),"
        "(SELECT count(*) FROM data_epochs d JOIN attachments a USING(data_epoch_id,component_id,publisher_ref,owner_scope) "
        "JOIN install_bindings b USING(install_id,component_id,publisher_ref,owner_scope) "
        "JOIN admission_gates g ON g.subject_kind='data' AND g.subject_ref=d.data_epoch_id "
        "WHERE d.data_epoch_id=r.d AND d.storage_ref=r.s AND d.publisher_ref=r.p AND d.owner_scope=r.a AND d.state='active' "
        "AND d.data_schema_version=r.version AND a.install_id=r.i) FROM r",&statement);
    int existing=0;
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EUCLEAN : failure(code);
        else if (sqlite3_column_int(statement,0)!=1) result=-ESTALE;
        else {
            int data=sqlite3_column_int(statement,1),attachments=sqlite3_column_int(statement,2),exact=sqlite3_column_int(statement,3);
            if (data==1 && attachments==1 && exact==1) existing=1;
            else if (data!=0 || attachments!=0) result=-ESTALE;
            if (result==0 && sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN;
        }
    }
    int final=failure(sqlite3_finalize(statement));
    if (result==0) result=final;
    if (result==0 && !existing && (info.revision!=request->base_revision || memcmp(info.boot_id,current_boot,16)!=0 ||
        !projection_id(info.runtime_incarnation))) result=-ESTALE;
    if (result==0 && !existing && info.revision==INT64_MAX) result=-EOVERFLOW;
    const char *statements[]={
        DATA_INPUT "INSERT INTO data_epochs(data_epoch_id,component_id,publisher_ref,owner_scope,storage_ref,state,data_schema_version) SELECT r.d,b.component_id,r.p,r.a,r.s,'active',r.version FROM r JOIN install_bindings b ON b.install_id=r.i",
        DATA_INPUT "UPDATE admission_gates SET state='closed',revision=revision+1 WHERE subject_kind='data' AND subject_ref=(SELECT d FROM r)",
        DATA_INPUT "INSERT INTO attachments SELECT r.i,r.d,b.component_id,r.p,r.a FROM r JOIN install_bindings b ON b.install_id=r.i",
        DATA_INPUT "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1"
    };
    for (size_t n=0;result==0 && !existing && n<4;++n) {
        statement=NULL;
        result=data_statement(store,request,statements[n],&statement);
        if (result==0) {
            int code=sqlite3_step(statement);
            result=code==SQLITE_DONE ? 0 : failure(code);
            if (result==0 && sqlite3_changes(store->db)!=1) result=-EUCLEAN;
        }
        final=failure(sqlite3_finalize(statement));
        if (result==0) result=final;
    }
    result=fixture_commit(store,result);
    if (result==0) *revision=info.revision+(existing ? 0 : 1);
    return result;
}
#undef DATA_INPUT

static int reservation_token(const char *text,size_t capacity,int action)
{
    size_t length=strnlen(text,capacity);
    if (length==0 || length>=capacity) return 0;
    for (size_t n=0;n<length;++n) {
        unsigned char c=(unsigned char)text[n];
        if (action ? !((c>='a' && c<='z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-') : c<0x21 || c>0x7e) return 0;
    }
    return 1;
}
int ae_launch_request_valid(const struct ae_launch_request *request)
{
    if (request==NULL || request->install_revision<1) return -EINVAL;
    const uint8_t *ids[]={request->system_epoch,request->lease,request->launch,request->broker_epoch,request->broker_runtime,
        request->account,request->install,request->generation,request->data,request->boot};
    for (size_t n=0;n<10;++n) if (!projection_id(ids[n])) return -EBADMSG;
    if (!reservation_token(request->action,sizeof request->action,1) || !reservation_token(request->session,sizeof request->session,0) ||
        !reservation_token(request->seat,sizeof request->seat,0)) return -EBADMSG;
    return 0;
}
static int reservation_context(struct ae_catalogue *store,const struct ae_launch_context *context)
{
    if (context==NULL || context->uid==UINT32_MAX) return -EINVAL;
    const uint8_t *ids[]={context->system_runtime,context->broker_epoch,context->broker_runtime,context->account,context->boot};
    for (size_t n=0;n<5;++n) if (!projection_id(ids[n])) return -EINVAL;
    int result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_SYSTEM) result=-EPERM;
    struct ae_catalogue_info info;
    if (result==0) result=read_info(store,&info);
    if (result==0 && (memcmp(info.boot_id,context->boot,16)!=0 || memcmp(info.runtime_incarnation,context->system_runtime,16)!=0)) result=-ESTALE;
    int64_t revision;
    if (result==0) result=account_row(store,context->account,context->uid,&revision);
    return result;
}
static int reservation_bind(sqlite3_stmt *statement,const char *name,const void *bytes,int size)
{
    int index=sqlite3_bind_parameter_index(statement,name);
    return index==0 ? -EINVAL : failure(sqlite3_bind_blob(statement,index,bytes,size,SQLITE_TRANSIENT));
}
static int dispatch_row(struct ae_catalogue *store,const uint8_t lease[16],const struct ae_launch_context *context,
                        struct ae_dispatch_snapshot *output)
{
    sqlite3_stmt *statement=NULL;
    int result=failure(sqlite3_prepare_v2(store->db,
        "SELECT root_operation_id,launch_id,invocation_ref,state,unit_name,revision,install_gate_revision FROM launch_dispatches "
        "WHERE lease_id=:lease AND account_scope_ref=:account AND broker_epoch=:broker",-1,&statement,NULL));
    if (result==0) result=reservation_bind(statement,":lease",lease,16);
    if (result==0) result=reservation_bind(statement,":account",context->account,16);
    if (result==0) result=reservation_bind(statement,":broker",context->broker_epoch,16);
    struct ae_dispatch_snapshot snapshot={0};
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -ENOENT : failure(code);
        else {
            result=projection_blob(statement,0,snapshot.operation,16);
            if (result==0) result=projection_blob(statement,1,snapshot.launch,16);
            if (result==0 && sqlite3_column_type(statement,2)!=SQLITE_NULL) {
                snapshot.invocation_present=1; result=projection_blob(statement,2,snapshot.invocation,16);
            }
            for (int n=0;result==0 && n<2;++n) {
                int column=3+n,bytes=sqlite3_column_bytes(statement,column);
                char *target=n==0 ? snapshot.state : snapshot.unit;
                size_t capacity=n==0 ? sizeof snapshot.state : sizeof snapshot.unit;
                if (sqlite3_column_type(statement,column)!=SQLITE_TEXT || bytes<1 || (size_t)bytes>=capacity ||
                    memchr(sqlite3_column_text(statement,column),0,(size_t)bytes)!=NULL) result=-EUCLEAN;
                else memcpy(target,sqlite3_column_text(statement,column),(size_t)bytes);
            }
            snapshot.revision=sqlite3_column_int64(statement,5); snapshot.install_gate_revision=sqlite3_column_int64(statement,6);
            if (result==0 && (snapshot.revision<1 || snapshot.install_gate_revision<1 || sqlite3_step(statement)!=SQLITE_DONE)) result=-EUCLEAN;
        }
    }
    int final=failure(sqlite3_finalize(statement));
    if (result==0) result=final;
    if (result==0) *output=snapshot;
    return result;
}
int ae_catalogue_get_dispatch(struct ae_catalogue *store,const uint8_t lease[16],const struct ae_launch_context *context,struct ae_dispatch_snapshot *output)
{
    if (!projection_id(lease) || output==NULL) return -EINVAL;
    int result=reservation_context(store,context);
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN");
    if (result!=0) return result;
    struct ae_dispatch_snapshot snapshot;
    result=reservation_context(store,context);
    if (result==0) result=dispatch_row(store,lease,context,&snapshot);
    result=fixture_commit(store,result);
    if (result==0) *output=snapshot;
    return result;
}

struct reservation_parameters {
    const struct ae_launch_request *request;
    const struct ae_reservation_ids *proposed;
    const struct ae_install_snapshot *install;
    const uint8_t *fingerprint,*intent;
    int64_t scope_revision;
};
#define RESERVATION_INPUT "WITH r(se,lease,launch,be,br,account,i,g,d,boot,ir,action,session,seat,plan,op,stamp,expires,component,fp,intent,gate,sr,unit) AS (VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,'aetheri-app-'||lower(hex(?3))||'.service')) "
static int reservation_statement(struct ae_catalogue *store,const struct reservation_parameters *p,const char *sql,sqlite3_stmt **output)
{
    sqlite3_stmt *statement=NULL;
    int result=failure(sqlite3_prepare_v2(store->db,sql,-1,&statement,NULL));
    const struct ae_launch_request *r=p->request;
    const void *ids[]={r->system_epoch,r->lease,r->launch,r->broker_epoch,r->broker_runtime,r->account,r->install,r->generation,r->data,r->boot};
    for (int n=0;result==0 && n<10;++n) result=failure(sqlite3_bind_blob(statement,n+1,ids[n],16,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_int64(statement,11,r->install_revision));
    const char *texts[]={r->action,r->session,r->seat};
    for (int n=0;result==0 && n<3;++n) result=failure(sqlite3_bind_text(statement,n+12,texts[n],-1,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,15,p->proposed->plan,16,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,16,p->proposed->operation,16,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_int64(statement,17,p->proposed->accepted_unix_ms));
    if (result==0) result=failure(sqlite3_bind_int64(statement,18,p->proposed->expires_boottime_ns));
    if (result==0) result=failure(sqlite3_bind_text(statement,19,p->install->component,-1,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,20,p->fingerprint,32,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,21,p->intent,32,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_int64(statement,22,p->install->gate_revision));
    if (result==0) result=failure(sqlite3_bind_int64(statement,23,p->scope_revision));
    if (result!=0) { sqlite3_finalize(statement); return result; }
    *output=statement; return 0;
}
static int reservation_execute(struct ae_catalogue *store,const struct reservation_parameters *p,const char *sql)
{
    sqlite3_stmt *statement=NULL;
    int result=reservation_statement(store,p,sql,&statement);
    if (result==0) {
        int code=sqlite3_step(statement);
        result=code==SQLITE_DONE ? 0 : (code==SQLITE_CONSTRAINT_UNIQUE || code==SQLITE_CONSTRAINT_PRIMARYKEY) ? -EEXIST : failure(code);
        if (result==0 && sqlite3_changes(store->db)!=1) result=-EUCLEAN;
    }
    int final=failure(sqlite3_finalize(statement));
    return result==0 ? final : result;
}
int ae_launch_fingerprint(const struct ae_launch_request *r,uint8_t digest[32])
{
    if (digest==NULL) return -EINVAL;
    int result=ae_launch_request_valid(r);
    if (result!=0) return result;
    struct fixture_encoder e={0}; fixture_bytes(&e,"AETHRSV1",8);
    const uint8_t *ids[]={r->system_epoch,r->lease,r->launch,r->broker_epoch,r->broker_runtime,r->account,r->install,r->generation,r->data,r->boot};
    for (size_t n=0;n<10;++n) fixture_bytes(&e,ids[n],16);
    fixture_number(&e,(uint64_t)r->install_revision,8);
    fixture_token(&e,r->action); fixture_token(&e,r->session); fixture_token(&e,r->seat);
    return e.error!=0 ? e.error : ae_sha256_bytes(e.bytes,e.used,digest);
}
static int reservation_intent(const struct reservation_parameters *p,uint8_t digest[32])
{
    const struct ae_launch_request *r=p->request;
    struct fixture_encoder e={0}; fixture_bytes(&e,"AETHINT1",8);
    fixture_bytes(&e,p->proposed->plan,16); fixture_bytes(&e,r->system_epoch,16); fixture_bytes(&e,r->broker_epoch,16); fixture_bytes(&e,r->account,16);
    fixture_token(&e,"launch"); fixture_token(&e,p->install->component);
    const uint8_t *ids[]={r->install,r->generation,r->data};
    for (size_t n=0;n<3;++n) { fixture_number(&e,1,1); fixture_bytes(&e,ids[n],16); }
    fixture_number(&e,0,1); fixture_number(&e,0,1); fixture_token(&e,"none"); fixture_number(&e,1,1); fixture_token(&e,r->action);
    fixture_bytes(&e,r->boot,16); fixture_number(&e,(uint64_t)p->proposed->expires_boottime_ns,8); fixture_number(&e,3,4);
    const char *kinds[]={"gate","install","scope"}; const uint8_t *refs[]={r->install,r->install,r->account};
    const int64_t revisions[]={p->install->gate_revision,r->install_revision,p->scope_revision};
    for (size_t n=0;n<3;++n) {
        fixture_bytes(&e,r->system_epoch,16); fixture_token(&e,kinds[n]); fixture_text(&e,refs[n],16); fixture_number(&e,(uint64_t)revisions[n],8);
    }
    fixture_number(&e,1,4); fixture_number(&e,0,4); fixture_token(&e,"generation"); fixture_token(&e,p->install->component);
    fixture_text(&e,r->generation,16); fixture_token(&e,"retain"); fixture_token(&e,"launch_reservation");
    return e.error!=0 ? e.error : ae_sha256_bytes(e.bytes,e.used,digest);
}

int ae_catalogue_reserve_launch(struct ae_catalogue *store,const struct ae_launch_request *r,
    const struct ae_launch_context *context,const struct ae_reservation_ids *proposed,struct ae_dispatch_snapshot *output)
{
    if (output==NULL || proposed==NULL || !projection_id(proposed->plan) || !projection_id(proposed->operation) ||
        proposed->accepted_unix_ms<0 || proposed->expires_boottime_ns<0) return -EINVAL;
    int result=ae_launch_request_valid(r);
    if (result==0) result=reservation_context(store,context);
    if (result==0 && (memcmp(r->system_epoch,store->identity.authority_epoch,16)!=0 || memcmp(r->broker_epoch,context->broker_epoch,16)!=0 ||
        memcmp(r->broker_runtime,context->broker_runtime,16)!=0 || memcmp(r->account,context->account,16)!=0 || memcmp(r->boot,context->boot,16)!=0)) result=-ESTALE;
    if (result!=0) return result;
    uint8_t fingerprint[32],intent[32]={0};
    result=ae_launch_fingerprint(r,fingerprint);
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE");
    if (result!=0) return result;
    result=reservation_context(store,context);
    struct ae_install_snapshot install={0}; int64_t scope_revision=0;
    struct reservation_parameters p={r,proposed,&install,fingerprint,intent,0};
    sqlite3_stmt *statement=NULL;
    if (result==0) result=reservation_statement(store,&p,RESERVATION_INPUT
        "SELECT count(*) FROM operations o,r WHERE (o.actor_ref=r.be AND o.request_key=r.lease) OR o.operation_id IN "
        "(SELECT root_operation_id FROM launch_dispatches WHERE lease_id=r.lease OR launch_id=r.launch)",&statement);
    int exists=0;
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EUCLEAN : failure(code);
        else { exists=sqlite3_column_int(statement,0); if (exists>1) result=-EEXIST; }
    }
    int final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
    if (result==0 && exists) {
        statement=NULL;
        result=reservation_statement(store,&p,RESERVATION_INPUT
            "SELECT d.state='reserved' AND l.state='reserved' FROM r JOIN launch_dispatches d ON d.lease_id=r.lease AND d.launch_id=r.launch "
            "AND d.account_scope_ref=r.account AND d.broker_epoch=r.be AND d.broker_incarnation=r.br AND d.install_ref=r.i AND d.generation_ref=r.g "
            "AND d.data_epoch_ref=r.d AND d.boot_id=r.boot AND d.action_id=r.action AND d.session_ref=r.session AND d.seat_ref=r.seat "
            "JOIN generation_leases l ON l.lease_id=d.lease_id AND l.holder_authority=r.be AND l.holder_ref=r.launch AND l.generation_id=r.g AND l.kind='launch' "
            "JOIN operations o ON o.operation_id=d.root_operation_id AND o.actor_ref=r.be AND o.request_key=r.lease "
            "JOIN plans p ON p.plan_id=o.plan_id AND p.authority_epoch=r.se AND p.kind='launch' AND p.install_ref=r.i AND p.generation_ref=r.g AND p.data_epoch_ref=r.d AND p.action_id=r.action "
            "JOIN operation_steps s ON s.operation_id=o.operation_id AND s.step_no=0 AND s.action='verify' AND s.subject_kind='launch' AND s.subject_ref=r.launch AND s.state='applied' AND s.evidence_digest=r.fp "
            "JOIN plan_reads t ON t.plan_id=p.plan_id AND t.source_epoch=r.se AND t.subject_kind='install' AND t.subject_ref=r.i AND t.expected_revision=r.ir",&statement);
        if (result==0) {
            int code=sqlite3_step(statement);
            if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EEXIST : failure(code);
            else if (sqlite3_column_int(statement,0)!=1) result=-EAGAIN;
            else if (sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN;
        }
        final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
    }
    if (result==0 && !exists) {
        struct ae_catalogue_info info;
        result=read_info(store,&info);
        if (result==0 && info.revision==INT64_MAX) result=-EOVERFLOW;
        struct timespec now;
        if (result==0 && clock_gettime(CLOCK_BOOTTIME,&now)!=0) result=-errno;
        if (result==0 && (now.tv_sec<0 || now.tv_sec>INT64_MAX/1000000000LL ||
            (uint64_t)proposed->expires_boottime_ns<=(uint64_t)now.tv_sec*1000000000ULL+(uint64_t)now.tv_nsec)) result=-ETIMEDOUT;
        if (result==0) result=account_row(store,r->account,context->uid,&scope_revision);
        p.scope_revision=scope_revision;
        statement=NULL;
        if (result==0) result=reservation_statement(store,&p,RESERVATION_INPUT
            "SELECT c.component_id,g.artifact_id,g.revision,f.revision FROM r JOIN installs i ON i.install_id=r.i AND i.state='active' AND i.revision=r.ir AND i.active_generation_id=r.g "
            "JOIN scopes s ON s.scope_id=i.scope_id AND s.state='active' AND (s.kind='machine' OR s.scope_id=r.account) "
            "JOIN components c ON c.component_id=i.component_id AND c.kind='app' "
            "JOIN generations g ON g.generation_id=r.g AND g.install_id=r.i AND g.publication_state='committed' AND g.activated_once=1 "
            "JOIN artifacts a ON a.artifact_id=g.artifact_id AND a.store_state='verified' "
            "JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=r.i AND f.state='open' AND f.closing_operation_id IS NULL",&statement);
        if (result==0) {
            int code=sqlite3_step(statement);
            if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -ESTALE : failure(code);
            else {
                int length=sqlite3_column_bytes(statement,0);
                if (length<1 || length>128 || memchr(sqlite3_column_text(statement,0),0,(size_t)length)!=NULL) result=-EUCLEAN;
                else memcpy(install.component,sqlite3_column_text(statement,0),(size_t)length);
                install.gate_revision=sqlite3_column_int64(statement,3);
                if (result==0 && sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN;
            }
        }
        final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
        if (result==0) result=reservation_intent(&p,intent);
        const char *sql[]={
            RESERVATION_INPUT "INSERT INTO plans(plan_id,authority_epoch,actor_ref,scope_ref,kind,component_id,install_ref,generation_ref,data_epoch_ref,retention,action_id,intent_digest,boot_id,expires_boottime_ns) SELECT plan,se,be,account,'launch',component,i,g,d,'none',action,intent,boot,expires FROM r",
            RESERVATION_INPUT "INSERT INTO plan_reads SELECT plan,se,'gate',i,gate FROM r",
            RESERVATION_INPUT "INSERT INTO plan_reads SELECT plan,se,'install',i,ir FROM r",
            RESERVATION_INPUT "INSERT INTO plan_reads SELECT plan,se,'scope',account,sr FROM r",
            RESERVATION_INPUT "INSERT INTO plan_effects SELECT plan,0,'generation',component,g,'retain','launch_reservation' FROM r",
            RESERVATION_INPUT "INSERT INTO operations(operation_id,plan_id,actor_ref,request_key,intent_digest,state,phase,reason_code,accepted_unix_ms) SELECT op,plan,be,lease,intent,'running','stage','code_reserved',stamp FROM r",
            RESERVATION_INPUT "INSERT INTO operation_steps SELECT op,0,'verify','launch',launch,'applied',fp FROM r",
            RESERVATION_INPUT "INSERT INTO generation_leases(lease_id,generation_id,holder_authority,holder_ref,kind,boot_id,state) SELECT lease,g,be,launch,'launch',boot,'reserved' FROM r",
            RESERVATION_INPUT "INSERT INTO launch_dispatches(launch_id,lease_id,root_operation_id,broker_epoch,broker_incarnation,account_scope_ref,install_ref,generation_ref,data_epoch_ref,boot_id,action_id,session_ref,seat_ref,install_gate_revision,unit_name,state) SELECT launch,lease,op,be,br,account,i,g,d,boot,action,session,seat,gate,unit,'reserved' FROM r",
            RESERVATION_INPUT "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1"
        };
        for (size_t n=0;result==0 && n<sizeof sql/sizeof sql[0];++n) result=reservation_execute(store,&p,sql[n]);
    }
    struct ae_dispatch_snapshot snapshot;
    if (result==0) result=dispatch_row(store,r->lease,context,&snapshot);
    result=fixture_commit(store,result);
    if (result==0) *output=snapshot;
    return result;
}
#undef RESERVATION_INPUT

int ae_catalogue_get_receipt(struct ae_catalogue *store,const uint8_t lease[16],const struct ae_launch_context *context,struct ae_reservation_receipt *output)
{
    if (!projection_id(lease) || output==NULL) return -EINVAL;
    int result=reservation_context(store,context);
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN");
    if (result!=0) return result;
    struct ae_reservation_receipt receipt={0};
    result=reservation_context(store,context);
    if (result==0) result=dispatch_row(store,lease,context,&receipt.dispatch);
    sqlite3_stmt *statement=NULL;
    if (result==0) result=failure(sqlite3_prepare_v2(store->db,
        "SELECT o.plan_id,o.intent_digest,s.evidence_digest,d.broker_incarnation,d.close_cause_epoch,d.close_cause_operation_ref,"
        "d.state='terminal' AND l.state='released' AND o.state='completed' AND o.outcome='cancelled' AND o.result_kind='launch' AND o.result_ref=d.launch_id AND d.invocation_ref IS NULL AND d.ready_data_gate_revision IS NULL "
        "FROM launch_dispatches d JOIN operations o ON o.operation_id=d.root_operation_id JOIN generation_leases l USING(lease_id) "
        "JOIN operation_steps s ON s.operation_id=o.operation_id AND s.step_no=0 AND s.action='verify' AND s.subject_kind='launch' AND s.subject_ref=d.launch_id AND s.state='applied' "
        "WHERE d.lease_id=:lease",-1,&statement,NULL));
    if (result==0) result=reservation_bind(statement,":lease",lease,16);
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EUCLEAN : failure(code);
        else {
            result=projection_blob(statement,0,receipt.plan,16);
            if (result==0) result=projection_blob(statement,1,receipt.intent,32);
            if (result==0) result=projection_blob(statement,2,receipt.fingerprint,32);
            if (result==0) result=projection_blob(statement,3,receipt.broker_runtime,16);
            receipt.cause_present=sqlite3_column_type(statement,4)!=SQLITE_NULL;
            if (result==0 && receipt.cause_present) result=projection_blob(statement,4,receipt.cause_epoch,16);
            if (result==0 && receipt.cause_present) result=projection_blob(statement,5,receipt.cause,16);
            receipt.withdrawn=sqlite3_column_int(statement,6)==1;
            if (result==0 && (sqlite3_step(statement)!=SQLITE_DONE || (receipt.withdrawn && !receipt.cause_present))) result=-EUCLEAN;
        }
    }
    int final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
    if (result==0) {
        memcpy(receipt.system_epoch,store->identity.authority_epoch,16); memcpy(receipt.system_runtime,context->system_runtime,16);
        memcpy(receipt.boot,context->boot,16); memcpy(receipt.lease,lease,16);
    }
    result=fixture_commit(store,result);
    if (result==0) *output=receipt;
    return result;
}

int ae_catalogue_cancel_reservation(struct ae_catalogue *store,const uint8_t lease[16],const struct ae_launch_context *context,
    const uint8_t cause_epoch[16],const uint8_t cause[16],int64_t expected_revision,int64_t completed_unix_ms,struct ae_dispatch_snapshot *output)
{
    if (output==NULL || !projection_id(lease) || !projection_id(cause_epoch) || !projection_id(cause) || expected_revision<1 || completed_unix_ms<0) return -EINVAL;
    int result=reservation_context(store,context);
    if (result==0 && memcmp(cause_epoch,context->broker_epoch,16)!=0) result=-EACCES;
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE");
    if (result!=0) return result;
    result=reservation_context(store,context);
    struct ae_dispatch_snapshot snapshot;
    if (result==0) result=dispatch_row(store,lease,context,&snapshot);
    int terminal=0;
    sqlite3_stmt *statement=NULL;
    if (result==0) result=failure(sqlite3_prepare_v2(store->db,
        "SELECT (d.state='terminal' AND l.state='released' AND o.state='completed' AND o.outcome='cancelled' AND d.close_cause_epoch=:epoch AND d.close_cause_operation_ref=:cause),"
        "(d.state='reserved' AND l.state='reserved' AND o.state='running' AND d.boot_id=:boot "
        "AND d.invocation_ref IS NULL AND d.ready_data_gate_revision IS NULL AND d.close_cause_epoch IS NULL "
        "AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=d.root_operation_id AND step_no>0)),l.revision,o.revision "
        "FROM launch_dispatches d JOIN generation_leases l USING(lease_id) JOIN operations o ON o.operation_id=d.root_operation_id WHERE d.lease_id=:lease",-1,&statement,NULL));
    const char *names[]={":epoch",":cause",":boot",":lease"};
    const void *values[]={cause_epoch,cause,context->boot,lease};
    for (size_t n=0;result==0 && n<4;++n) result=reservation_bind(statement,names[n],values[n],16);
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EUCLEAN : failure(code);
        else {
            terminal=sqlite3_column_int(statement,0)==1;
            if (!terminal && strcmp(snapshot.state,"terminal")==0) result=-EEXIST;
            else if (!terminal && sqlite3_column_int(statement,1)!=1) result=-EAGAIN;
            else if (!terminal && snapshot.revision!=expected_revision) result=-ESTALE;
            else if (!terminal && (snapshot.revision==INT64_MAX || sqlite3_column_int64(statement,2)==INT64_MAX || sqlite3_column_int64(statement,3)==INT64_MAX)) result=-EOVERFLOW;
            if (result==0 && sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN;
        }
    }
    int final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
    struct ae_catalogue_info info;
    if (result==0) result=read_info(store,&info);
    if (result==0 && !terminal && info.revision==INT64_MAX) result=-EOVERFLOW;
    const char *sql[]={
        "UPDATE launch_dispatches SET state='terminal',close_cause_epoch=:epoch,close_cause_operation_ref=:cause,revision=revision+1 WHERE lease_id=:lease",
        "UPDATE generation_leases SET state='released',revision=revision+1 WHERE lease_id=:lease",
        "UPDATE operations SET state='completed',phase='reconcile',reason_code='reservation_cancelled',outcome='cancelled',completed_unix_ms=:stamp,result_kind='launch',result_ref=:launch,revision=revision+1 WHERE operation_id=:operation",
        "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1"
    };
    for (size_t n=0;result==0 && !terminal && n<4;++n) {
        statement=NULL;
        result=failure(sqlite3_prepare_v2(store->db,sql[n],-1,&statement,NULL));
        const char *keys[]={":lease",":epoch",":cause",":launch",":operation"};
        const void *ids[]={lease,cause_epoch,cause,snapshot.launch,snapshot.operation};
        for (size_t m=0;result==0 && m<5;++m) if (sqlite3_bind_parameter_index(statement,keys[m])!=0) result=reservation_bind(statement,keys[m],ids[m],16);
        int index=result==0 ? sqlite3_bind_parameter_index(statement,":stamp") : 0;
        if (index!=0) result=failure(sqlite3_bind_int64(statement,index,completed_unix_ms));
        if (result==0) {
            int code=sqlite3_step(statement); result=code==SQLITE_DONE ? 0 : failure(code);
            if (result==0 && sqlite3_changes(store->db)!=1) result=-EUCLEAN;
        }
        final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
    }
    if (result==0) result=dispatch_row(store,lease,context,&snapshot);
    result=fixture_commit(store,result);
    if (result==0) *output=snapshot;
    return result;
}

int ae_catalogue_admit_install(struct ae_catalogue *store,const uint8_t install[16],const uint8_t generation[16],
    int64_t install_revision,int64_t gate_revision,const uint8_t boot[16],const uint8_t runtime[16],int64_t *output)
{
    if (!projection_id(install) || !projection_id(generation) || !projection_id(boot) || !projection_id(runtime) || output==NULL || install_revision<1 || gate_revision<1) return -EINVAL;
    int result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_SYSTEM) result=-EPERM;
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE");
    if (result!=0) return result;
    struct ae_catalogue_info info;
    result=read_info(store,&info);
    if (result==0 && (memcmp(info.boot_id,boot,16)!=0 || memcmp(info.runtime_incarnation,runtime,16)!=0)) result=-ESTALE;
    sqlite3_stmt *statement=NULL;
    if (result==0) result=failure(sqlite3_prepare_v2(store->db,
        "SELECT f.state,f.revision FROM installs i JOIN generations g ON g.generation_id=i.active_generation_id "
        "JOIN artifacts a ON a.artifact_id=g.artifact_id JOIN components c ON c.component_id=i.component_id "
        "JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=i.install_id "
        "WHERE i.install_id=:i AND i.active_generation_id=:g AND i.state='active' AND i.revision=:revision AND c.kind='app' "
        "AND g.publication_state='committed' AND g.activated_once=1 AND a.store_state='verified' AND f.closing_operation_id IS NULL "
        "AND NOT EXISTS (SELECT 1 FROM generation_leases WHERE generation_id=:g AND state<>'released') "
        "AND NOT EXISTS (SELECT 1 FROM launch_dispatches WHERE install_ref=:i AND state<>'terminal')",-1,&statement,NULL));
    if (result==0) result=reservation_bind(statement,":i",install,16);
    if (result==0) result=reservation_bind(statement,":g",generation,16);
    if (result==0) result=failure(sqlite3_bind_int64(statement,sqlite3_bind_parameter_index(statement,":revision"),install_revision));
    int existing=0;
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -ESTALE : failure(code);
        else {
            const char *state=(const char *)sqlite3_column_text(statement,0);
            int64_t revision=sqlite3_column_int64(statement,1);
            if (state==NULL) result=-EUCLEAN;
            else if (strcmp(state,"open")==0 && gate_revision<INT64_MAX && revision==gate_revision+1) existing=1;
            else if (strcmp(state,"closed")!=0 || revision!=gate_revision) result=-ESTALE;
            if (result==0 && sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN;
        }
    }
    int final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
    if (result==0 && !existing && (gate_revision==INT64_MAX || info.revision==INT64_MAX)) result=-EOVERFLOW;
    if (result==0 && !existing) {
        statement=NULL;
        result=failure(sqlite3_prepare_v2(store->db,"UPDATE admission_gates SET state='open',revision=revision+1 WHERE subject_kind='install' AND subject_ref=:i",-1,&statement,NULL));
        if (result==0) result=reservation_bind(statement,":i",install,16);
        if (result==0) { int code=sqlite3_step(statement); result=code==SQLITE_DONE ? 0 : failure(code); }
        final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
        if (result==0) result=exec_sql(store->db,"UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1");
    }
    result=fixture_commit(store,result);
    if (result==0) *output=gate_revision+1;
    return result;
}

#define UL_INPUT "WITH u(plan,op,actor,retry,ue,br,se,sr,boot,account,i,g,d,launch,lease,publisher,storage,component,action,session,seat,ir,dr,dgate,igate,stamp,expires,fp,intent,rplan,rop,rintent) AS (VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24,?25,?26,?27,?28,?29,?30,?31,?32)) "
struct user_launch_parameters { const struct ae_user_launch *q; const uint8_t *fingerprint,*intent; const struct ae_reservation_receipt *receipt; };
static int user_launch_inputs(struct ae_catalogue *store,const struct ae_user_launch *q)
{
    if (q==NULL || q->data_revision<1 || q->data_gate_revision<1 || q->local.accepted_unix_ms<0 || q->local.expires_boottime_ns<0) return -EINVAL;
    int result=ae_launch_request_valid(&q->reservation);
    if (result==0) result=ae_install_snapshot_valid(&q->source);
    const uint8_t *ids[]={q->local.plan,q->local.operation,q->actor,q->request_key,q->storage};
    for (size_t n=0;result==0 && n<5;++n) if (!projection_id(ids[n])) result=-EINVAL;
    if (result==0) result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_USER) result=-EPERM;
    const struct ae_launch_request *r=&q->reservation;
    if (result==0 && (memcmp(r->broker_epoch,store->identity.authority_epoch,16)!=0 || memcmp(r->account,store->identity.owner,16)!=0 ||
        memcmp(r->system_epoch,q->source.system_epoch,16)!=0 || memcmp(r->boot,q->source.boot,16)!=0 ||
        memcmp(r->account,q->source.account,16)!=0 || memcmp(r->install,q->source.install,16)!=0 || memcmp(r->generation,q->source.generation,16)!=0 ||
        r->install_revision!=q->source.install_revision || memcmp(r->system_epoch,r->broker_epoch,16)==0)) result=-ESTALE;
    return result;
}
static int user_launch_statement(struct ae_catalogue *store,const struct user_launch_parameters *p,const char *sql,sqlite3_stmt **output)
{
    const struct ae_user_launch *q=p->q; const struct ae_launch_request *r=&q->reservation;
    uint8_t zero[32]={0};
    const void *ids[]={q->local.plan,q->local.operation,q->actor,q->request_key,r->broker_epoch,r->broker_runtime,r->system_epoch,q->source.system_runtime,
        r->boot,r->account,r->install,r->generation,r->data,r->launch,r->lease,q->source.publisher,q->storage};
    sqlite3_stmt *statement=NULL; int result=failure(sqlite3_prepare_v2(store->db,sql,-1,&statement,NULL));
    for (int n=0;result==0 && n<17;++n) result=failure(sqlite3_bind_blob(statement,n+1,ids[n],16,SQLITE_TRANSIENT));
    const char *texts[]={q->source.component,r->action,r->session,r->seat};
    for (int n=0;result==0 && n<4;++n) result=failure(sqlite3_bind_text(statement,n+18,texts[n],-1,SQLITE_TRANSIENT));
    const int64_t numbers[]={r->install_revision,q->data_revision,q->data_gate_revision,q->source.gate_revision,q->local.accepted_unix_ms,q->local.expires_boottime_ns};
    for (int n=0;result==0 && n<6;++n) result=failure(sqlite3_bind_int64(statement,n+22,numbers[n]));
    if (result==0) result=failure(sqlite3_bind_blob(statement,28,p->fingerprint,32,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,29,p->intent,32,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,30,p->receipt ? p->receipt->plan : zero,16,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,31,p->receipt ? p->receipt->dispatch.operation : zero,16,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,32,p->receipt ? p->receipt->intent : zero,32,SQLITE_TRANSIENT));
    if (result!=0) { sqlite3_finalize(statement); return result; }
    *output=statement; return 0;
}
static int user_launch_execute(struct ae_catalogue *store,const struct user_launch_parameters *p,const char *sql)
{
    sqlite3_stmt *statement=NULL; int result=user_launch_statement(store,p,sql,&statement);
    if (result==0) {
        int code=sqlite3_step(statement);
        result=code==SQLITE_DONE ? 0 : (code==SQLITE_CONSTRAINT_UNIQUE || code==SQLITE_CONSTRAINT_PRIMARYKEY) ? -EEXIST : failure(code);
        if (result==0 && sqlite3_changes(store->db)!=1) result=-EUCLEAN;
    }
    int final=failure(sqlite3_finalize(statement)); return result==0 ? final : result;
}
static int user_launch_count(struct ae_catalogue *store,const struct user_launch_parameters *p,const char *sql,int64_t *output)
{
    sqlite3_stmt *statement=NULL; int result=user_launch_statement(store,p,sql,&statement);
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EUCLEAN : failure(code);
        else { *output=sqlite3_column_int64(statement,0); if (sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN; }
    }
    int final=failure(sqlite3_finalize(statement)); return result==0 ? final : result;
}
static int user_launch_row(struct ae_catalogue *store,const uint8_t operation[16],struct ae_user_launch_snapshot *output)
{
    sqlite3_stmt *statement=NULL;
    int result=failure(sqlite3_prepare_v2(store->db,
        "SELECT l.launch_id,l.lease_ref,l.state,l.revision,l.readiness,o.state,o.revision,x.remote_operation_ref,s.evidence_digest,"
        "(SELECT expected_revision FROM plan_reads WHERE plan_id=p.plan_id AND source_epoch=p.authority_epoch AND subject_kind='gate' AND subject_ref=l.data_epoch_id),"
        "(SELECT expected_revision FROM plan_reads WHERE plan_id=p.plan_id AND source_epoch=l.system_epoch AND subject_kind='gate' AND subject_ref=l.install_id),l.system_epoch,"
        "l.invocation_ref IS NULL AND l.readiness='unknown' AND l.state IN('requested','preparing','recovery_required') AND o.state<>'completed' "
        "AND NOT EXISTS(SELECT 1 FROM grants WHERE launch_id=l.launch_id) AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=o.operation_id AND step_no>0) "
        "FROM launches l JOIN operations o USING(operation_id) JOIN plans p USING(plan_id) "
        "JOIN operation_steps s ON s.operation_id=o.operation_id AND s.step_no=0 AND s.subject_kind='launch' AND s.subject_ref=l.launch_id "
        "LEFT JOIN operation_links x ON x.operation_id=o.operation_id AND x.link_no=0 WHERE o.operation_id=:op",-1,&statement,NULL));
    if (result==0) result=reservation_bind(statement,":op",operation,16);
    struct ae_user_launch_snapshot snapshot={0};
    if (result==0) {
        int code=sqlite3_step(statement);
        if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -ENOENT : failure(code);
        else {
            memcpy(snapshot.operation,operation,16);
            result=projection_blob(statement,0,snapshot.launch,16);
            if (result==0) result=projection_blob(statement,1,snapshot.lease,16);
            snapshot.revision=sqlite3_column_int64(statement,3); snapshot.operation_revision=sqlite3_column_int64(statement,6);
            const int columns[]={2,4,5}; char *targets[]={snapshot.state,snapshot.readiness,snapshot.operation_state};
            const size_t capacities[]={sizeof snapshot.state,sizeof snapshot.readiness,sizeof snapshot.operation_state};
            for (size_t n=0;result==0 && n<3;++n) {
                int size=sqlite3_column_bytes(statement,columns[n]);
                if (sqlite3_column_type(statement,columns[n])!=SQLITE_TEXT || size<1 || (size_t)size>=capacities[n] || memchr(sqlite3_column_text(statement,columns[n]),0,(size_t)size)!=NULL) result=-EUCLEAN;
                else memcpy(targets[n],sqlite3_column_text(statement,columns[n]),(size_t)size);
            }
            snapshot.root_link_present=sqlite3_column_type(statement,7)!=SQLITE_NULL;
            if (result==0 && snapshot.root_link_present) result=projection_blob(statement,7,snapshot.root_operation,16);
            if (result==0) result=projection_blob(statement,8,snapshot.fingerprint,32);
            snapshot.data_gate_revision=sqlite3_column_int64(statement,9); snapshot.install_gate_revision=sqlite3_column_int64(statement,10);
            if (result==0) result=projection_blob(statement,11,snapshot.system_epoch,16);
            snapshot.pre_dispatch_withdrawable=sqlite3_column_int(statement,12)==1;
            if (result==0 && (snapshot.revision<1 || snapshot.operation_revision<1 || snapshot.data_gate_revision<1 || snapshot.install_gate_revision<1 || sqlite3_step(statement)!=SQLITE_DONE)) result=-EUCLEAN;
        }
    }
    int final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
    if (result==0) *output=snapshot;
    return result;
}
int ae_catalogue_get_user_launch(struct ae_catalogue *store,const uint8_t operation[16],struct ae_user_launch_snapshot *output)
{
    if (!projection_id(operation) || output==NULL) return -EINVAL;
    int result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_USER) result=-EPERM;
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN"); if (result!=0) return result;
    struct ae_user_launch_snapshot snapshot;
    result=user_launch_row(store,operation,&snapshot); result=fixture_commit(store,result);
    if (result==0) *output=snapshot;
    return result;
}
static int user_launch_intent(const struct ae_user_launch *q,uint8_t digest[32])
{
    const struct ae_launch_request *r=&q->reservation;
    struct fixture_encoder e={0}; fixture_bytes(&e,"AETHINT1",8); fixture_bytes(&e,q->local.plan,16);
    fixture_bytes(&e,r->broker_epoch,16); fixture_bytes(&e,q->actor,16); fixture_bytes(&e,r->account,16);
    fixture_token(&e,"launch"); fixture_token(&e,q->source.component);
    const uint8_t *refs[]={r->install,r->generation,r->data};
    for (size_t n=0;n<3;++n) { fixture_number(&e,1,1); fixture_bytes(&e,refs[n],16); }
    fixture_number(&e,0,1); fixture_number(&e,0,1); fixture_token(&e,"none"); fixture_number(&e,1,1); fixture_token(&e,r->action);
    fixture_bytes(&e,r->boot,16); fixture_number(&e,(uint64_t)q->local.expires_boottime_ns,8); fixture_number(&e,4,4);
    struct read { const uint8_t *epoch,*ref; const char *kind; int64_t revision; } reads[]={
        {r->system_epoch,r->install,"gate",q->source.gate_revision},{r->system_epoch,r->install,"install",r->install_revision},
        {r->broker_epoch,r->data,"data",q->data_revision},{r->broker_epoch,r->data,"gate",q->data_gate_revision}};
    for (size_t n=0;n<4;++n) for (size_t m=n+1;m<4;++m) {
        int order=memcmp(reads[n].epoch,reads[m].epoch,16);
        if (order==0) order=strcmp(reads[n].kind,reads[m].kind);
        if (order==0) order=memcmp(reads[n].ref,reads[m].ref,16);
        if (order>0) { struct read temporary=reads[n]; reads[n]=reads[m]; reads[m]=temporary; }
    }
    for (size_t n=0;n<4;++n) { fixture_bytes(&e,reads[n].epoch,16); fixture_token(&e,reads[n].kind); fixture_text(&e,reads[n].ref,16); fixture_number(&e,(uint64_t)reads[n].revision,8); }
    fixture_number(&e,1,4); fixture_number(&e,0,4); fixture_token(&e,"data"); fixture_token(&e,q->source.component);
    fixture_text(&e,r->data,16); fixture_token(&e,"retain"); fixture_token(&e,"launch_data_reservation");
    return e.error ? e.error : ae_sha256_bytes(e.bytes,e.used,digest);
}
#define UL_LIVE "SELECT count(*) FROM u JOIN authority_meta m ON m.singleton=1 AND m.authority_epoch=u.ue AND m.owner_scope_ref=u.account AND m.current_boot_id=u.boot AND m.runtime_incarnation=u.br JOIN install_bindings b ON b.install_id=u.i AND b.component_id=u.component AND b.publisher_ref=u.publisher AND b.owner_scope=u.account AND b.system_epoch=u.se AND b.install_revision=u.ir AND b.state='active' JOIN attachments a ON a.install_id=u.i AND a.data_epoch_id=u.d JOIN data_epochs d ON d.data_epoch_id=u.d AND d.component_id=u.component AND d.publisher_ref=u.publisher AND d.owner_scope=u.account AND d.storage_ref=u.storage AND d.state='active' AND d.revision=u.dr JOIN admission_gates f ON f.subject_kind='data' AND f.subject_ref=u.d "
int ae_catalogue_admit_launch_data(struct ae_catalogue *store,const struct ae_user_launch *q,int64_t *output)
{
    if (output==NULL) return -EINVAL;
    int result=user_launch_inputs(store,q);
    if (result!=0) return result;
    if (q->source.gate!=AE_INSTALL_GATE_OPEN) return -ESTALE;
    if (q->data_gate_revision==INT64_MAX) return -EOVERFLOW;
    uint8_t zero[32]={0}; struct user_launch_parameters p={q,zero,zero,NULL};
    result=exec_sql(store->db,"BEGIN IMMEDIATE"); if (result!=0) return result;
    int64_t count=0;
    result=user_launch_count(store,&p,UL_INPUT UL_LIVE "WHERE f.state='open' AND f.revision=u.dgate+1 AND f.closing_operation_id IS NULL",&count);
    if (result==0 && count==0) {
        result=user_launch_count(store,&p,UL_INPUT UL_LIVE "WHERE f.state='closed' AND f.revision=u.dgate AND f.closing_operation_id IS NULL "
            "AND NOT EXISTS(SELECT 1 FROM launches WHERE data_epoch_id=u.d AND state NOT IN('exited','start_failed')) "
            "AND NOT EXISTS(SELECT 1 FROM grants WHERE data_epoch_id=u.d AND state NOT IN('revoked','expired'))",&count);
        struct ae_catalogue_info info;
        if (result==0 && count!=1) result=-ESTALE;
        if (result==0) result=read_info(store,&info);
        if (result==0 && (info.revision==INT64_MAX || q->data_gate_revision==INT64_MAX)) result=-EOVERFLOW;
        if (result==0) result=user_launch_execute(store,&p,UL_INPUT "UPDATE admission_gates SET state='open',revision=revision+1 WHERE subject_kind='data' AND subject_ref=(SELECT d FROM u)");
        if (result==0) result=user_launch_execute(store,&p,UL_INPUT "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1");
    }
    result=fixture_commit(store,result);
    if (result==0) *output=q->data_gate_revision+1;
    return result;
}
static int user_launch_match(struct ae_catalogue *store,const struct user_launch_parameters *p,struct ae_user_launch_snapshot *output)
{
    int64_t count=0;
    int result=user_launch_count(store,p,UL_INPUT
        "SELECT count(*) FROM u JOIN operations o ON o.operation_id=u.op AND o.plan_id=u.plan AND o.actor_ref=u.actor AND o.request_key=u.retry AND o.intent_digest=u.intent AND o.accepted_unix_ms=u.stamp "
        "JOIN plans p ON p.plan_id=u.plan AND p.authority_epoch=u.ue AND p.scope_ref=u.account AND p.kind='launch' AND p.component_id=u.component AND p.install_ref=u.i AND p.generation_ref=u.g AND p.data_epoch_ref=u.d AND p.action_id=u.action AND p.boot_id=u.boot AND p.expires_boottime_ns=u.expires "
        "JOIN launches l ON l.operation_id=u.op AND l.launch_id=u.launch AND l.install_id=u.i AND l.generation_ref=u.g AND l.data_epoch_id=u.d AND l.lease_ref=u.lease AND l.system_epoch=u.se AND l.boot_id=u.boot AND l.session_ref=u.session AND l.seat_ref=u.seat "
        "JOIN operation_steps s ON s.operation_id=u.op AND s.step_no=0 AND s.subject_kind='launch' AND s.subject_ref=u.launch AND s.evidence_digest=u.fp",&count);
    if (result==0 && count!=1) result=-EEXIST;
    if (result==0) result=user_launch_row(store,p->q->local.operation,output);
    return result;
}
int ae_catalogue_accept_launch(struct ae_catalogue *store,const struct ae_user_launch *q,struct ae_user_launch_snapshot *output)
{
    if (output==NULL) return -EINVAL;
    int result=user_launch_inputs(store,q);
    if (result!=0) return result;
    uint8_t fingerprint[32],intent[32];
    result=ae_launch_fingerprint(&q->reservation,fingerprint);
    if (result==0) result=user_launch_intent(q,intent);
    if (result!=0) return result;
    struct user_launch_parameters p={q,fingerprint,intent,NULL};
    result=exec_sql(store->db,"BEGIN IMMEDIATE"); if (result!=0) return result;
    int64_t exists=0;
    result=user_launch_count(store,&p,UL_INPUT "SELECT count(*) FROM operations o,u WHERE o.operation_id=u.op OR o.plan_id=u.plan OR (o.actor_ref=u.actor AND o.request_key=u.retry) OR o.operation_id IN (SELECT operation_id FROM launches WHERE launch_id=u.launch OR lease_ref=u.lease)",&exists);
    if (result==0 && exists>1) result=-EEXIST;
    if (result==0 && !exists) {
        int64_t valid=0;
        result=user_launch_count(store,&p,UL_INPUT UL_LIVE "WHERE f.state='open' AND f.revision=u.dgate AND f.closing_operation_id IS NULL",&valid);
        if (result==0 && (valid!=1 || q->source.gate!=AE_INSTALL_GATE_OPEN)) result=-ESTALE;
        struct timespec now;
        if (result==0 && clock_gettime(CLOCK_BOOTTIME,&now)!=0) result=-errno;
        if (result==0 && (now.tv_sec<0 || now.tv_sec>INT64_MAX/1000000000LL || (uint64_t)q->local.expires_boottime_ns<=(uint64_t)now.tv_sec*1000000000ULL+(uint64_t)now.tv_nsec)) result=-ETIMEDOUT;
        struct ae_catalogue_info info;
        if (result==0) result=read_info(store,&info);
        if (result==0 && info.revision==INT64_MAX) result=-EOVERFLOW;
        const char *sql[]={
            UL_INPUT "INSERT INTO plans(plan_id,authority_epoch,actor_ref,scope_ref,kind,component_id,install_ref,generation_ref,data_epoch_ref,retention,action_id,intent_digest,boot_id,expires_boottime_ns) SELECT plan,ue,actor,account,'launch',component,i,g,d,'none',action,intent,boot,expires FROM u",
            UL_INPUT "INSERT INTO plan_reads SELECT plan,se,'gate',i,igate FROM u",
            UL_INPUT "INSERT INTO plan_reads SELECT plan,se,'install',i,ir FROM u",
            UL_INPUT "INSERT INTO plan_reads SELECT plan,ue,'data',d,dr FROM u",
            UL_INPUT "INSERT INTO plan_reads SELECT plan,ue,'gate',d,dgate FROM u",
            UL_INPUT "INSERT INTO plan_effects SELECT plan,0,'data',component,d,'retain','launch_data_reservation' FROM u",
            UL_INPUT "INSERT INTO operations(operation_id,plan_id,actor_ref,request_key,intent_digest,state,phase,reason_code,accepted_unix_ms) SELECT op,plan,actor,retry,intent,'running','stage','reserving_code',stamp FROM u",
            UL_INPUT "INSERT INTO launches(launch_id,operation_id,install_id,data_epoch_id,generation_ref,lease_ref,system_epoch,boot_id,session_ref,seat_ref,supervisor_unit,state,readiness) SELECT launch,op,i,d,g,lease,se,boot,session,seat,'aetheri-app-'||lower(hex(launch))||'.service','requested','unknown' FROM u",
            UL_INPUT "INSERT INTO operation_steps SELECT op,0,'verify','launch',launch,'executing',fp FROM u",
            UL_INPUT "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1"
        };
        for (size_t n=0;result==0 && n<sizeof sql/sizeof sql[0];++n) result=user_launch_execute(store,&p,sql[n]);
    }
    struct ae_user_launch_snapshot snapshot;
    if (result==0) result=user_launch_match(store,&p,&snapshot);
    result=fixture_commit(store,result);
    if (result==0) *output=snapshot;
    return result;
}

static int user_receipt_match(const struct ae_user_launch *q,const struct ae_reservation_receipt *receipt,const uint8_t fingerprint[32])
{
    if (receipt==NULL || memchr(receipt->dispatch.state,0,sizeof receipt->dispatch.state)==NULL || !projection_id(receipt->plan) || !projection_id(receipt->dispatch.operation) || !projection_id(receipt->system_runtime) ||
        !projection_id(receipt->boot) || receipt->dispatch.revision<1 || receipt->dispatch.install_gate_revision!=q->source.gate_revision) return -EINVAL;
    if (memcmp(receipt->system_epoch,q->reservation.system_epoch,16)!=0 || memcmp(receipt->fingerprint,fingerprint,32)!=0 ||
        memcmp(receipt->lease,q->reservation.lease,16)!=0 || memcmp(receipt->dispatch.launch,q->reservation.launch,16)!=0 ||
        memcmp(receipt->broker_runtime,q->reservation.broker_runtime,16)!=0) return -ESTALE;
    return 0;
}
#define UL_LINK_MATCH "SELECT count(*) FROM operation_links x,u WHERE x.operation_id=u.op AND x.link_no=0 AND x.remote_epoch=u.se AND x.remote_plan_ref=u.rplan AND x.remote_intent_digest=u.rintent AND x.remote_request_key=u.lease AND x.remote_operation_ref=u.rop "
#define UL_LINK_INSERT UL_INPUT "INSERT INTO operation_links SELECT op,0,se,rplan,rintent,lease,rop,'accepted' FROM u"
static int user_link_state(struct ae_catalogue *store,const struct user_launch_parameters *p,int64_t *present)
{
    int64_t count=0;
    int result=user_launch_count(store,p,UL_INPUT "SELECT count(*) FROM operation_links x,u WHERE x.operation_id=u.op AND x.link_no=0",present);
    if (result==0 && *present) result=user_launch_count(store,p,UL_INPUT UL_LINK_MATCH,&count);
    if (result==0 && *present && count!=1) result=-EEXIST;
    return result;
}
static int user_revision_room(struct ae_catalogue *store,const struct ae_user_launch_snapshot *snapshot)
{
    struct ae_catalogue_info info; int result=read_info(store,&info);
    if (result==0 && (info.revision==INT64_MAX || snapshot->revision==INT64_MAX || snapshot->operation_revision==INT64_MAX)) result=-EOVERFLOW;
    return result;
}
int ae_catalogue_ack_launch(struct ae_catalogue *store,const struct ae_user_launch *q,const struct ae_reservation_receipt *receipt,struct ae_user_launch_snapshot *output)
{
    if (output==NULL) return -EINVAL;
    int result=user_launch_inputs(store,q); uint8_t fingerprint[32],intent[32];
    if (result==0) result=ae_launch_fingerprint(&q->reservation,fingerprint);
    if (result==0) result=user_launch_intent(q,intent);
    if (result==0) result=user_receipt_match(q,receipt,fingerprint);
    if (result!=0) return result;
    struct user_launch_parameters p={q,fingerprint,intent,receipt};
    result=exec_sql(store->db,"BEGIN IMMEDIATE"); if (result!=0) return result;
    struct ae_user_launch_snapshot snapshot;
    result=user_launch_match(store,&p,&snapshot);
    int64_t present=0;
    if (result==0) result=user_link_state(store,&p,&present);
    if (result==0 && !present) {
        if (memcmp(receipt->system_runtime,q->source.system_runtime,16)!=0 || memcmp(receipt->boot,q->reservation.boot,16)!=0 ||
            strcmp(receipt->dispatch.state,"reserved")!=0 || receipt->withdrawn || receipt->dispatch.revision!=1 ||
            strcmp(snapshot.state,"requested")!=0 || strcmp(snapshot.readiness,"unknown")!=0) result=-ESTALE;
        int64_t valid=0;
        if (result==0) result=user_launch_count(store,&p,UL_INPUT UL_LIVE "WHERE f.state='open' AND f.revision=u.dgate AND f.closing_operation_id IS NULL",&valid);
        if (result==0 && valid!=1) result=-ESTALE;
        if (result==0) result=user_revision_room(store,&snapshot);
        const char *sql[]={UL_LINK_INSERT,
            UL_INPUT "UPDATE operation_steps SET state='applied' WHERE operation_id=(SELECT op FROM u) AND step_no=0 AND state='executing'",
            UL_INPUT "UPDATE launches SET state='preparing',revision=revision+1 WHERE operation_id=(SELECT op FROM u)",
            UL_INPUT "UPDATE operations SET reason_code='code_reserved',revision=revision+1 WHERE operation_id=(SELECT op FROM u)",
            UL_INPUT "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1"};
        for (size_t n=0;result==0 && n<5;++n) result=user_launch_execute(store,&p,sql[n]);
    }
    if (result==0) result=user_launch_row(store,q->local.operation,&snapshot);
    result=fixture_commit(store,result); if (result==0) *output=snapshot;
    return result;
}
#undef UL_LINK_MATCH
#undef UL_LINK_INSERT
#undef UL_LIVE
#undef UL_INPUT

int ae_catalogue_finish_withdrawn_operation(struct ae_catalogue *store,const uint8_t operation[16],const struct ae_reservation_receipt *r,
    int64_t stamp,struct ae_user_launch_snapshot *output)
{
    if (output==NULL || r==NULL || memchr(r->dispatch.state,0,sizeof r->dispatch.state)==NULL || !projection_id(operation) || !projection_id(r->plan) || !projection_id(r->dispatch.operation) || stamp<0) return -EINVAL;
    int result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_USER) result=-EPERM;
    if (result==0 && (!r->withdrawn || !r->cause_present || strcmp(r->dispatch.state,"terminal")!=0 || r->dispatch.invocation_present ||
        memcmp(r->cause_epoch,store->identity.authority_epoch,16)!=0 || memcmp(r->cause,operation,16)!=0)) result=-EACCES;
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE"); if (result!=0) return result;
    struct ae_user_launch_snapshot snapshot;
    result=user_launch_row(store,operation,&snapshot);
    if (result==0 && (memcmp(snapshot.fingerprint,r->fingerprint,32)!=0 || memcmp(snapshot.lease,r->lease,16)!=0 ||
        memcmp(snapshot.launch,r->dispatch.launch,16)!=0 || memcmp(snapshot.system_epoch,r->system_epoch,16)!=0 ||
        snapshot.install_gate_revision!=r->dispatch.install_gate_revision)) result=-ESTALE;
    int terminal=result==0 && strcmp(snapshot.operation_state,"completed")==0;
    const char *sql[]={
        "SELECT count(*) FROM operation_links WHERE operation_id=:op AND link_no=0 AND NOT(remote_epoch IS :epoch AND remote_plan_ref IS :plan AND remote_intent_digest IS :intent AND remote_request_key IS :lease AND remote_operation_ref IS :root)",
        "SELECT count(*) FROM launches l JOIN operations o USING(operation_id) WHERE l.operation_id=:op AND ((o.state='completed' AND o.outcome='cancelled' AND o.result_kind='launch' AND o.result_ref=:launch AND l.state='start_failed' AND l.readiness='unavailable' AND EXISTS(SELECT 1 FROM operation_links WHERE operation_id=:op AND link_no=0 AND state='completed')) OR (o.state<>'completed' AND l.state IN('requested','preparing','recovery_required') AND l.readiness='unknown' AND l.invocation_ref IS NULL AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=:op AND step_no>0) AND NOT EXISTS(SELECT 1 FROM grants WHERE launch_id=:launch)))",
        "INSERT INTO operation_links SELECT :op,0,:epoch,:plan,:intent,:lease,:root,'accepted' WHERE NOT EXISTS(SELECT 1 FROM operation_links WHERE operation_id=:op AND link_no=0)",
        "UPDATE operation_steps SET state='applied' WHERE operation_id=:op AND step_no=0",
        "INSERT INTO operation_steps VALUES(:op,1,'release','launch',:launch,'applied',:fp)",
        "UPDATE launches SET state='start_failed',readiness='unavailable',revision=revision+1 WHERE operation_id=:op",
        "UPDATE operation_links SET state='completed' WHERE operation_id=:op AND link_no=0",
        "UPDATE operations SET state='completed',phase='reconcile',reason_code='cancelled_before_dispatch',outcome='cancelled',completed_unix_ms=:stamp,result_kind='launch',result_ref=:launch,revision=revision+1 WHERE operation_id=:op",
        "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1"};
    for (size_t n=0;result==0 && n<9;++n) {
        if (terminal && n>=2) break;
        if (n==2) result=user_revision_room(store,&snapshot);
        sqlite3_stmt *statement=NULL;
        if (result==0) result=failure(sqlite3_prepare_v2(store->db,sql[n],-1,&statement,NULL));
        const char *names[]={":op",":epoch",":plan",":intent",":lease",":root",":launch",":fp"};
        const void *ids[]={operation,r->system_epoch,r->plan,r->intent,r->lease,r->dispatch.operation,r->dispatch.launch,r->fingerprint};
        for (size_t m=0;result==0 && m<8;++m) if (sqlite3_bind_parameter_index(statement,names[m])) result=reservation_bind(statement,names[m],ids[m],m==3 || m==7 ? 32 : 16);
        int index=result==0 ? sqlite3_bind_parameter_index(statement,":stamp") : 0;
        if (index) result=failure(sqlite3_bind_int64(statement,index,stamp));
        if (result==0) {
            int code=sqlite3_step(statement);
            if (n<2) { if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EUCLEAN : failure(code); else if (sqlite3_column_int(statement,0)!=(n==0 ? 0 : 1)) result=n==0 ? -EEXIST : -EAGAIN; }
            else { result=code==SQLITE_DONE ? 0 : failure(code); if (result==0 && n!=2 && sqlite3_changes(store->db)!=1) result=-EUCLEAN; }
        }
        int final=failure(sqlite3_finalize(statement)); if (result==0) result=final;
    }
    if (result==0) result=user_launch_row(store,operation,&snapshot);
    result=fixture_commit(store,result); if (result==0) *output=snapshot;
    return result;
}

#define SETUP_INPUT "WITH p(se,sr,be,br,boot,account,i,g,d,storage,launch,lease,op,uop,ir,igate,dgate,hash,inv,evidence,stamp,artifact) AS (VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22)) "
static int setup_statement(struct ae_catalogue *store,const struct ae_setup_profile *p,const uint8_t digest[32],
    const uint8_t invocation[16],const uint8_t evidence[32],int64_t stamp,const char *sql,int64_t *value)
{
    const void *ids[]={p->system_epoch,p->system_runtime,p->broker_epoch,p->broker_runtime,p->boot,p->account,p->install,p->generation,
        p->data,p->storage,p->launch,p->lease,p->root_operation,p->user_operation};
    sqlite3_stmt *statement=NULL; int result=failure(sqlite3_prepare_v2(store->db,sql,-1,&statement,NULL));
    for (int n=0;result==0 && n<14;++n) result=failure(sqlite3_bind_blob(statement,n+1,ids[n],16,SQLITE_TRANSIENT));
    const int64_t numbers[]={p->install_revision,p->install_gate_revision,p->data_gate_revision};
    for (int n=0;result==0 && n<3;++n) result=failure(sqlite3_bind_int64(statement,n+15,numbers[n]));
    uint8_t zero[32]={0};
    if (result==0) result=failure(sqlite3_bind_blob(statement,18,digest,32,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,19,invocation ? invocation : zero,16,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(statement,20,evidence ? evidence : zero,32,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_int64(statement,21,stamp));
    if (result==0) result=failure(sqlite3_bind_blob(statement,22,p->artifact,32,SQLITE_TRANSIENT));
    if (result==0) {
        int code=sqlite3_step(statement);
        if (value!=NULL) {
            if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EUCLEAN : failure(code);
            else { *value=sqlite3_column_int64(statement,0); if (sqlite3_step(statement)!=SQLITE_DONE) result=-EUCLEAN; }
        } else { result=code==SQLITE_DONE ? 0 : failure(code); if (result==0 && sqlite3_changes(store->db)!=1) result=-EUCLEAN; }
    }
    int final=failure(sqlite3_finalize(statement)); return result==0 ? final : result;
}
#define SETUP_TUPLE "FROM p JOIN authority_meta m ON m.singleton=1 AND m.authority_epoch=p.se AND m.runtime_incarnation=p.sr AND m.current_boot_id=p.boot " \
    "JOIN launch_dispatches d ON d.lease_id=p.lease AND d.launch_id=p.launch AND d.root_operation_id=p.op AND d.broker_epoch=p.be AND d.broker_incarnation=p.br AND d.account_scope_ref=p.account " \
    "AND d.install_ref=p.i AND d.generation_ref=p.g AND d.data_epoch_ref=p.d AND d.boot_id=p.boot AND d.install_gate_revision=p.igate " \
    "JOIN operations o ON o.operation_id=p.op JOIN plans a ON a.plan_id=o.plan_id AND a.authority_epoch=p.se AND a.scope_ref=p.account " \
    "JOIN plan_reads r ON r.plan_id=a.plan_id AND r.source_epoch=p.se AND r.subject_kind='install' AND r.subject_ref=p.i AND r.expected_revision=p.ir " \
    "JOIN generation_leases l ON l.lease_id=p.lease JOIN generations gen ON gen.generation_id=p.g AND gen.artifact_id=p.artifact "
#define SETUP_ONE "SELECT count(*) " SETUP_TUPLE "JOIN operation_steps s ON s.operation_id=p.op AND s.step_no=1 AND s.action='stage' AND s.subject_kind='launch' AND s.subject_ref=p.launch AND s.evidence_digest=p.hash "
#define SETUP_TWO "SELECT count(*) " SETUP_TUPLE "JOIN operation_steps s ON s.operation_id=p.op AND s.step_no=2 AND s.action='verify' AND s.subject_kind='integration' AND s.subject_ref=p.inv AND s.evidence_digest=p.evidence "
static int setup_action(struct ae_catalogue *store,const struct ae_setup_profile *p,const uint8_t digest[32],const uint8_t invocation[16],
    const uint8_t evidence[32],const struct ae_launch_context *context,int64_t stamp,int action,struct ae_dispatch_snapshot *output)
{
    int exited=action==4;
    if (exited) action=1;
    if (p==NULL || digest==NULL || output==NULL || stamp<0 || p->install_revision<1 || p->install_gate_revision<1 || p->data_gate_revision<1) return -EINVAL;
    const uint8_t *ids[]={p->system_epoch,p->system_runtime,p->broker_epoch,p->broker_runtime,p->boot,p->account,p->install,p->generation,p->data,p->storage,p->launch,p->lease,p->root_operation,p->user_operation};
    for (size_t n=0;n<14;++n) if (!projection_id(ids[n])) return -EINVAL;
    if (action>0 && (invocation==NULL || evidence==NULL || !nonzero(invocation))) return -EINVAL;
    int result=reservation_context(store,context);
    if (result==0 && (memcmp(p->system_epoch,store->identity.authority_epoch,16)!=0 || memcmp(p->system_runtime,context->system_runtime,16)!=0 ||
        memcmp(p->boot,context->boot,16)!=0 || memcmp(p->broker_epoch,context->broker_epoch,16)!=0 || memcmp(p->account,context->account,16)!=0 ||
        (action==0 && memcmp(p->broker_runtime,context->broker_runtime,16)!=0))) result=-ESTALE;
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE"); if (result!=0) return result;
    int64_t match=0,one=0,two=0,steps=0,terminal=0;
#define SQ(sql,out) setup_statement(store,p,digest,invocation,evidence,stamp,SETUP_INPUT sql,out)
    result=SQ("SELECT count(*) " SETUP_TUPLE,&match);
    if (result==0 && match!=1) result=-ESTALE;
    if (result==0) result=SQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=1",&one);
    if (result==0 && one) {
        result=SQ(SETUP_ONE,&match);
        if (result==0 && match!=1) result=-EEXIST;
    }
    if (result==0 && action>0 && !one) result=-ENOENT;
    if (result==0 && action>0) {
        result=SQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=2",&two);
        if (result==0 && two) { result=SQ(SETUP_TWO,&match); if (result==0 && match!=1) result=-EEXIST; }
        if (result==0 && action>1 && !two) result=-ENOENT;
    }
    if (result==0 && action==3) {
        result=SQ("SELECT count(*) " SETUP_TUPLE "WHERE d.state='terminal' AND l.state='released' AND o.state='completed' AND o.outcome='cancelled' AND o.result_ref=p.launch AND d.close_cause_epoch=p.be AND d.close_cause_operation_ref=p.uop "
            "AND EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no=3 AND action='release' AND subject_kind='launch' AND subject_ref=p.launch AND state='applied' AND evidence_digest=p.hash)",&terminal);
    }
    int mutation=result==0 && ((action==0 && !one) || (action==1 && !two) || action==2 || (action==3 && !terminal));
    if (result==0 && mutation) {
        result=SQ("SELECT count(*) " SETUP_TUPLE "WHERE d.state='reserved' AND l.state='reserved' AND o.state='running' AND d.invocation_ref IS NULL AND d.ready_data_gate_revision IS NULL AND d.close_cause_epoch IS NULL",&match);
        if (result==0 && match!=1) result=-EAGAIN;
        if (result==0) result=SQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no>0",&steps);
        if (result==0 && steps!=(action==0 ? 0 : action==1 ? 1 : 2)) result=-EAGAIN;
        if (result==0 && action==0) {
            result=SQ("SELECT count(*) FROM installs i,admission_gates f,p WHERE i.install_id=p.i AND i.state='active' AND i.active_generation_id=p.g AND i.revision=p.ir AND f.subject_kind='install' AND f.subject_ref=p.i AND f.state='open' AND f.closing_operation_id IS NULL AND f.revision=p.igate",&match);
            if (result==0 && match!=1) result=-ESTALE;
        }
        if (result==0 && action==2) {
            result=SQ(SETUP_ONE "WHERE s.state='applied'",&match);
            if (result==0 && match==1) mutation=0;
        }
        struct ae_catalogue_info info;
        if (result==0 && mutation) result=read_info(store,&info);
        if (result==0 && mutation) {
            if (info.revision==INT64_MAX) result=-EOVERFLOW;
            if (result==0) result=SQ("SELECT revision FROM operations,p WHERE operation_id=p.op",&match);
            if (result==0 && match==INT64_MAX) result=-EOVERFLOW;
            if (result==0 && action==3) result=SQ("SELECT max(d.revision,l.revision) " SETUP_TUPLE,&match);
            if (result==0 && action==3 && match==INT64_MAX) result=-EOVERFLOW;
        }
        if (result==0 && mutation && action==0) result=SQ("INSERT INTO operation_steps SELECT op,1,'stage','launch',launch,'executing',hash FROM p",NULL);
        if (result==0 && mutation && action==1) result=SQ("INSERT INTO operation_steps SELECT op,2,'verify','integration',inv,'executing',evidence FROM p",NULL);
        if (result==0 && mutation && action>=2) result=SQ("UPDATE operation_steps SET state='applied' WHERE operation_id=(SELECT op FROM p) AND step_no=1",NULL);
        if (result==0 && mutation && action>=2) result=SQ("UPDATE operation_steps SET state='applied' WHERE operation_id=(SELECT op FROM p) AND step_no=2",NULL);
        if (result==0 && mutation && action==3) {
            const char *sql[]={
                SETUP_INPUT "INSERT INTO operation_steps SELECT op,3,'release','launch',launch,'applied',hash FROM p",
                SETUP_INPUT "UPDATE launch_dispatches SET state='terminal',close_cause_epoch=(SELECT be FROM p),close_cause_operation_ref=(SELECT uop FROM p),revision=revision+1 WHERE lease_id=(SELECT lease FROM p)",
                SETUP_INPUT "UPDATE generation_leases SET state='released',revision=revision+1 WHERE lease_id=(SELECT lease FROM p)"};
            for (size_t n=0;result==0 && n<3;++n) result=setup_statement(store,p,digest,invocation,evidence,stamp,sql[n],NULL);
        }
        const char *update=action==3 ?
            SETUP_INPUT "UPDATE operations SET state='completed',phase='reconcile',reason_code='setup_drained_without_exec',outcome='cancelled',completed_unix_ms=(SELECT stamp FROM p),result_kind='launch',result_ref=(SELECT launch FROM p),revision=revision+1 WHERE operation_id=(SELECT op FROM p)" : action==0 ?
            SETUP_INPUT "UPDATE operations SET reason_code='setup_requested',revision=revision+1 WHERE operation_id=(SELECT op FROM p)" : action==1 ?
            (exited ? SETUP_INPUT "UPDATE operations SET reason_code='setup_exit_observed',revision=revision+1 WHERE operation_id=(SELECT op FROM p)" :
             SETUP_INPUT "UPDATE operations SET reason_code='setup_bound',revision=revision+1 WHERE operation_id=(SELECT op FROM p)") :
            SETUP_INPUT "UPDATE operations SET reason_code='setup_prepared',revision=revision+1 WHERE operation_id=(SELECT op FROM p)";
        if (result==0 && mutation) result=setup_statement(store,p,digest,invocation,evidence,stamp,update,NULL);
        if (result==0 && mutation) result=SQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    struct ae_dispatch_snapshot snapshot;
    if (result==0) result=dispatch_row(store,p->lease,context,&snapshot);
    result=fixture_commit(store,result); if (result==0) *output=snapshot;
#undef SQ
    return result;
}
int ae_catalogue_authorize_user_setup(struct ae_catalogue *store,const struct ae_setup_profile *p)
{
    if (p==NULL) return -EINVAL;
    int result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_USER) result=-EPERM;
    if (result!=0) return result;
    uint8_t zero[32]={0}; int64_t match=0;
    result=setup_statement(store,p,zero,NULL,NULL,0,SETUP_INPUT
        "SELECT count(*) FROM p JOIN authority_meta m ON m.singleton=1 AND m.authority_epoch=p.be AND m.owner_scope_ref=p.account AND m.current_boot_id=p.boot AND m.runtime_incarnation=p.br "
        "JOIN launches l ON l.operation_id=p.uop AND l.launch_id=p.launch AND l.lease_ref=p.lease AND l.system_epoch=p.se AND l.install_id=p.i AND l.generation_ref=p.g AND l.data_epoch_id=p.d "
        "JOIN operations o ON o.operation_id=p.uop AND o.state='running' JOIN operation_links x ON x.operation_id=p.uop AND x.link_no=0 AND x.state='accepted' AND x.remote_epoch=p.se AND x.remote_operation_ref=p.op "
        "JOIN data_epochs d ON d.data_epoch_id=p.d AND d.storage_ref=p.storage AND d.state='active' JOIN attachments a ON a.install_id=p.i AND a.data_epoch_id=p.d "
        "JOIN admission_gates f ON f.subject_kind='data' AND f.subject_ref=p.d AND f.state='open' AND f.closing_operation_id IS NULL AND f.revision=p.dgate "
        "WHERE l.state='preparing' AND l.readiness='unknown' AND l.invocation_ref IS NULL "
        "AND EXISTS(SELECT 1 FROM plan_reads WHERE plan_id=o.plan_id AND source_epoch=p.se AND subject_kind='gate' AND subject_ref=p.i AND expected_revision=p.igate) "
        "AND EXISTS(SELECT 1 FROM plan_reads WHERE plan_id=o.plan_id AND source_epoch=p.be AND subject_kind='gate' AND subject_ref=p.d AND expected_revision=p.dgate) "
        "AND EXISTS(SELECT 1 FROM plan_reads WHERE plan_id=o.plan_id AND source_epoch=p.be AND subject_kind='data' AND subject_ref=p.d AND expected_revision=d.revision) "
        "AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.uop AND step_no>0) AND NOT EXISTS(SELECT 1 FROM grants WHERE launch_id=p.launch)",&match);
    return result==0 && match!=1 ? -ESTALE : result;
}
#undef SETUP_INPUT
#undef SETUP_ONE
#undef SETUP_TWO
#undef SETUP_TUPLE
static struct ae_launch_context setup_context(const struct ae_setup_profile *p)
{
    struct ae_launch_context c={.uid=1000};
    memcpy(c.system_runtime,p->system_runtime,16); memcpy(c.broker_epoch,p->broker_epoch,16); memcpy(c.broker_runtime,p->broker_runtime,16);
    memcpy(c.account,p->account,16); memcpy(c.boot,p->boot,16); return c;
}
int ae_catalogue_begin_setup(struct ae_catalogue *s,const struct ae_setup_profile *p,const uint8_t h[32],const struct ae_launch_context *c,struct ae_dispatch_snapshot *o)
{ return setup_action(s,p,h,NULL,NULL,c,0,0,o); }
int ae_catalogue_register_setup(struct ae_catalogue *s,const struct ae_setup_profile *p,const uint8_t h[32],const uint8_t i[16],const uint8_t e[32],struct ae_dispatch_snapshot *o)
{ if (p==NULL) return -EINVAL; struct ae_launch_context c=setup_context(p); return setup_action(s,p,h,i,e,&c,0,1,o); }
int ae_catalogue_record_setup_exit(struct ae_catalogue *s,const struct ae_setup_profile *p,const uint8_t h[32],const uint8_t i[16],const uint8_t e[32],struct ae_dispatch_snapshot *o)
{ if (p==NULL) return -EINVAL; struct ae_launch_context c=setup_context(p); return setup_action(s,p,h,i,e,&c,0,4,o); }
int ae_catalogue_prepared_setup(struct ae_catalogue *s,const struct ae_setup_profile *p,const uint8_t h[32],const uint8_t i[16],const uint8_t e[32],struct ae_dispatch_snapshot *o)
{ if (p==NULL) return -EINVAL; struct ae_launch_context c=setup_context(p); return setup_action(s,p,h,i,e,&c,0,2,o); }
int ae_catalogue_withdraw_setup(struct ae_catalogue *s,const struct ae_setup_profile *p,const uint8_t h[32],const uint8_t i[16],const uint8_t e[32],const struct ae_launch_context *c,int64_t t,struct ae_dispatch_snapshot *o)
{ return setup_action(s,p,h,i,e,c,t,3,o); }

#define EXEC_INPUT "WITH p(se,sr,be,br,boot,account,i,g,d,storage,launch,lease,op,uop,ir,igate,dgate,hash,inv,evidence,artifact,fp,rhash,reason) AS (VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24)) "
static int execution_statement_full(struct ae_catalogue *store,const struct ae_execution_profile *p,const uint8_t hash[32],
    const struct ae_execution_evidence *e,const uint8_t report[32],const char *reason,const int64_t *stamp,const char *sql,int64_t *value)
{
    const uint8_t *ids[]={p->system_epoch,p->system_runtime,p->broker_epoch,p->broker_runtime,p->boot,p->account,p->install,p->generation,
        p->data,p->storage,p->launch,p->lease,p->root_operation,p->user_operation};
    sqlite3_stmt *s=NULL; int result=failure(sqlite3_prepare_v2(store->db,sql,-1,&s,NULL));
    for (int n=0;result==0 && n<14;++n) result=failure(sqlite3_bind_blob(s,n+1,ids[n],16,SQLITE_TRANSIENT));
    const int64_t numbers[]={p->install_revision,p->install_gate_revision,p->data_gate_revision};
    for (int n=0;result==0 && n<3;++n) result=failure(sqlite3_bind_int64(s,n+15,numbers[n]));
    uint8_t zero[32]={0};
    const uint8_t *digests[]={hash,e ? e->invocation : zero,e ? e->context_digest : zero,p->artifact,p->request_digest};
    for (int n=0;result==0 && n<5;++n) result=failure(sqlite3_bind_blob(s,n+18,digests[n],n==1 ? 16 : 32,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_blob(s,23,report ? report : zero,32,SQLITE_TRANSIENT));
    if (result==0) result=failure(sqlite3_bind_text(s,24,reason ? reason : "",-1,SQLITE_TRANSIENT));
    if (result==0 && stamp!=NULL && sqlite3_bind_parameter_count(s)>=25) result=failure(sqlite3_bind_int64(s,25,*stamp));
    if (result==0) {
        int code=sqlite3_step(s);
        if (value!=NULL) {
            if (code!=SQLITE_ROW) result=code==SQLITE_DONE ? -EUCLEAN : failure(code);
            else if (sqlite3_column_type(s,0)!=SQLITE_INTEGER) result=-EUCLEAN;
            else { *value=sqlite3_column_int64(s,0); if (sqlite3_step(s)!=SQLITE_DONE) result=-EUCLEAN; }
        } else { result=code==SQLITE_DONE ? 0 : failure(code); if (result==0 && sqlite3_changes(store->db)!=1) result=-EUCLEAN; }
    }
    int final=failure(sqlite3_finalize(s)); return result==0 ? final : result;
}
static int execution_statement_extra(struct ae_catalogue *store,const struct ae_execution_profile *p,const uint8_t hash[32],
    const struct ae_execution_evidence *e,const uint8_t report[32],const char *reason,const char *sql,int64_t *value)
{ return execution_statement_full(store,p,hash,e,report,reason,NULL,sql,value); }
static int execution_statement(struct ae_catalogue *store,const struct ae_execution_profile *p,const uint8_t hash[32],
    const struct ae_execution_evidence *e,const char *sql,int64_t *value)
{ return execution_statement_extra(store,p,hash,e,NULL,NULL,sql,value); }
#define EXEC_TUPLE "FROM p JOIN authority_meta m ON m.singleton=1 AND m.authority_epoch=p.se AND m.runtime_incarnation=p.sr AND m.current_boot_id=p.boot " \
    "JOIN launch_dispatches d ON d.lease_id=p.lease AND d.launch_id=p.launch AND d.root_operation_id=p.op AND d.broker_epoch=p.be AND d.broker_incarnation=p.br AND d.account_scope_ref=p.account " \
    "AND d.install_ref=p.i AND d.generation_ref=p.g AND d.data_epoch_ref=p.d AND d.boot_id=p.boot AND d.install_gate_revision=p.igate AND d.action_id='run' AND d.session_ref='p0' AND d.seat_ref='none' " \
    "JOIN operations o ON o.operation_id=p.op JOIN plans a ON a.plan_id=o.plan_id AND a.authority_epoch=p.se AND a.scope_ref=p.account " \
    "JOIN plan_reads r ON r.plan_id=a.plan_id AND r.source_epoch=p.se AND r.subject_kind='install' AND r.subject_ref=p.i AND r.expected_revision=p.ir " \
    "JOIN generation_leases l ON l.lease_id=p.lease JOIN generations gen ON gen.generation_id=p.g AND gen.artifact_id=p.artifact " \
    "JOIN operation_steps z ON z.operation_id=p.op AND z.step_no=0 AND z.action='verify' AND z.subject_kind='launch' AND z.subject_ref=p.launch AND z.state='applied' AND z.evidence_digest=p.fp "
static int process_catalogue_row(struct ae_catalogue *,const struct ae_execution_profile *,enum ae_execution_process_role,struct ae_execution_process_snapshot *);
static int require_execution_process(struct ae_catalogue *s,const struct ae_execution_profile *p,const uint8_t hash[32],const struct ae_execution_evidence *e,int role)
{
    (void)hash;
    sqlite3_int64 version=0; int r=scalar_int(s->db,"PRAGMA user_version",&version);
    struct ae_execution_process_snapshot snapshot;
    if (r==0 && version!=2 && version!=3 && version!=4 && !((version==5 || version==6) && s->identity.kind==AE_CATALOGUE_SYSTEM)) r=-EPROTONOSUPPORT;
    if (r==0 && (version==4 || version==5 || version==6)) {
        r=process_catalogue_row(s,p,(enum ae_execution_process_role)role,&snapshot);
        if (r==0) r=ae_execution_process_match(p,e,&snapshot.process);
    }
    return r==-ENOENT ? -EAGAIN : r;
}
static int user_execution_begin(struct ae_catalogue *store,const struct ae_execution_profile *p,uint8_t hash[32],int write)
{
    int result=ae_execution_digest(p,hash);
    if (result==0) result=still_named(store);
    if (result==0 && store->identity.kind!=AE_CATALOGUE_USER) result=-EPERM;
    return result==0 ? exec_sql(store->db,write ? "BEGIN IMMEDIATE" : "BEGIN") : result;
}
static int user_execution_fence(struct ae_catalogue *store,const struct ae_execution_profile *p,const uint8_t hash[32],const struct ae_execution_evidence *e)
{
    int64_t match=0;
    int result=execution_statement(store,p,hash,e,EXEC_INPUT
        "SELECT count(*) FROM p JOIN authority_meta m ON m.singleton=1 AND m.authority_epoch=p.be AND m.owner_scope_ref=p.account AND m.current_boot_id=p.boot AND m.runtime_incarnation=p.br "
        "JOIN launches l ON l.operation_id=p.uop AND l.launch_id=p.launch AND l.lease_ref=p.lease AND l.system_epoch=p.se AND l.install_id=p.i AND l.generation_ref=p.g AND l.data_epoch_id=p.d AND l.boot_id=p.boot AND l.session_ref='p0' AND l.seat_ref='none' "
        "JOIN operations o ON o.operation_id=p.uop AND o.state='running' JOIN plans a ON a.plan_id=o.plan_id AND a.action_id='run' "
        "JOIN operation_links x ON x.operation_id=p.uop AND x.link_no=0 AND x.state='accepted' AND x.remote_epoch=p.se AND x.remote_operation_ref=p.op "
        "JOIN operation_steps z ON z.operation_id=p.uop AND z.step_no=0 AND z.action='verify' AND z.subject_kind='launch' AND z.subject_ref=p.launch AND z.state='applied' AND z.evidence_digest=p.fp "
        "JOIN data_epochs d ON d.data_epoch_id=p.d AND d.storage_ref=p.storage AND d.state='active' JOIN attachments t ON t.install_id=p.i AND t.data_epoch_id=p.d "
        "JOIN install_bindings b ON b.install_id=p.i AND b.state='active' AND b.system_epoch=p.se AND b.install_revision=p.ir "
        "JOIN admission_gates f ON f.subject_kind='data' AND f.subject_ref=p.d AND f.state='open' AND f.closing_operation_id IS NULL AND f.revision=p.dgate "
        "WHERE ((l.state='preparing' AND l.readiness='unknown' AND l.invocation_ref IS NULL) OR (p.inv<>zeroblob(16) AND l.invocation_ref=p.inv AND ((l.state='starting' AND l.readiness='unknown') OR (l.state='running' AND l.readiness='ready')))) "
        "AND EXISTS(SELECT 1 FROM plan_reads WHERE plan_id=a.plan_id AND source_epoch=p.se AND subject_kind='gate' AND subject_ref=p.i AND expected_revision=p.igate) "
        "AND EXISTS(SELECT 1 FROM plan_reads WHERE plan_id=a.plan_id AND source_epoch=p.se AND subject_kind='install' AND subject_ref=p.i AND expected_revision=p.ir) "
        "AND EXISTS(SELECT 1 FROM plan_reads WHERE plan_id=a.plan_id AND source_epoch=p.be AND subject_kind='gate' AND subject_ref=p.d AND expected_revision=p.dgate) "
        "AND EXISTS(SELECT 1 FROM plan_reads WHERE plan_id=a.plan_id AND source_epoch=p.be AND subject_kind='data' AND subject_ref=p.d AND expected_revision=d.revision) "
        "AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.uop AND step_no>3) AND NOT EXISTS(SELECT 1 FROM grants WHERE launch_id=p.launch)",&match);
    return result==0 && match!=1 ? -ESTALE : result;
}
int ae_catalogue_authorize_user_execution(struct ae_catalogue *store,const struct ae_execution_profile *p)
{
    uint8_t hash[32]; int result=user_execution_begin(store,p,hash,0);
    if (result!=0) return result;
    result=user_execution_fence(store,p,hash,NULL);
    int64_t steps=0;
    if (result==0) result=execution_statement(store,p,hash,NULL,EXEC_INPUT
        "SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no>0",&steps);
    if (result==0 && steps!=0) result=-ESTALE;
    return fixture_commit(store,result);
}
int ae_catalogue_prepare_user_execution(struct ae_catalogue *store,const struct ae_execution_profile *p,struct ae_user_execution_result *output)
{
    if (output==NULL) return -EINVAL;
    uint8_t hash[32]; int result=user_execution_begin(store,p,hash,1);
    if (result!=0) return result;
    result=user_execution_fence(store,p,hash,NULL);
    int64_t present=0,match=0;
#define UQ(sql,out) execution_statement(store,p,hash,NULL,EXEC_INPUT sql,out)
    if (result==0) result=UQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no>1",&match);
    if (result==0 && match) result=-ESTALE;
    if (result==0) result=UQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no=1",&present);
    if (result==0 && present) {
        result=UQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no=1 AND action='stage' AND subject_kind='data' AND subject_ref=p.d AND state='executing' AND evidence_digest=p.hash",&match);
        if (result==0 && match!=1) result=-EEXIST;
    }
    if (result==0 && !present) {
        result=UQ("SELECT max(m.catalogue_revision,o.revision,l.revision) FROM p JOIN authority_meta m ON m.singleton=1 JOIN operations o ON o.operation_id=p.uop JOIN launches l ON l.operation_id=p.uop",&match);
        if (result==0 && match==INT64_MAX) result=-EOVERFLOW;
        if (result==0) result=UQ("INSERT INTO operation_steps SELECT uop,1,'stage','data',d,'executing',hash FROM p",NULL);
        if (result==0) result=UQ("UPDATE launches SET revision=revision+1 WHERE operation_id=(SELECT uop FROM p)",NULL);
        if (result==0) result=UQ("UPDATE operations SET reason_code='exec_preparing',revision=revision+1 WHERE operation_id=(SELECT uop FROM p)",NULL);
        if (result==0) result=UQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    struct ae_user_execution_result receipt={0};
    if (result==0) result=user_launch_row(store,p->user_operation,&receipt.launch);
    receipt.submit_required=!present;
    result=fixture_commit(store,result); if (result==0) *output=receipt;
#undef UQ
    return result;
}
int ae_catalogue_observe_user_execution(struct ae_catalogue *store,const struct ae_execution_profile *p,
    enum ae_user_execution_stage stage,const struct ae_execution_evidence *e,struct ae_user_launch_snapshot *output)
{
    if (output==NULL || e==NULL || !nonzero(e->invocation) || stage<AE_USER_EXECUTION_REGISTER || stage>AE_USER_EXECUTION_READY) return -EINVAL;
    uint8_t hash[32]; int r=user_execution_begin(store,p,hash,1); if (r!=0) return r;
    r=user_execution_fence(store,p,hash,e);
    int64_t match=0,two=0,three=0;
#define AQ(sql,out) execution_statement(store,p,hash,e,EXEC_INPUT sql,out)
    if (r==0) r=AQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no=1 AND action='stage' AND subject_kind='data' AND subject_ref=p.d AND evidence_digest=p.hash AND state IN ('executing','applied')",&match);
    if (r==0 && match!=1) r=-ESTALE;
    if (r==0) r=AQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no=2",&two);
    if (r==0) r=AQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no=3",&three);
    if (r==0 && two) {
        r=AQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no=2 AND action='verify' AND subject_kind='integration' AND subject_ref=p.inv AND state='applied' AND evidence_digest=p.evidence",&match);
        if (r==0 && match!=1) r=-EEXIST;
    }
    if (r==0 && three) {
        r=AQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no=3 AND action='activate' AND subject_kind='launch' AND subject_ref=p.launch AND state='applied' AND evidence_digest=p.evidence",&match);
        if (r==0 && (match!=1 || !two)) r=-EEXIST;
    }
    if (r==0 && stage==AE_USER_EXECUTION_READY && !two) r=-ENOENT;
    int mutation=r==0 && (stage==AE_USER_EXECUTION_REGISTER ? !two : !three);
    if (r==0) {
        r=AQ("SELECT count(*) FROM launches l,operation_steps s,p WHERE l.operation_id=p.uop AND s.operation_id=p.uop AND s.step_no=1 AND ((l.state='preparing' AND l.invocation_ref IS NULL AND l.readiness='unknown' AND s.state='executing' AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.uop AND step_no>1)) OR (l.invocation_ref=p.inv AND s.state='applied' AND ((l.state='starting' AND l.readiness='unknown' AND EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.uop AND step_no=2) AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.uop AND step_no=3)) OR (l.state='running' AND l.readiness='ready' AND EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.uop AND step_no=3)))))",&match);
        if (r==0 && match!=1) r=-ESTALE;
    }
    if (r==0 && mutation) {
        r=AQ("SELECT max(m.catalogue_revision,o.revision,l.revision) FROM p JOIN authority_meta m ON m.singleton=1 JOIN operations o ON o.operation_id=p.uop JOIN launches l ON l.operation_id=p.uop",&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0 && stage==AE_USER_EXECUTION_READY) r=require_execution_process(store,p,hash,e,2);
        if (r==0 && stage==AE_USER_EXECUTION_REGISTER) {
            r=AQ("INSERT INTO operation_steps SELECT uop,2,'verify','integration',inv,'applied',evidence FROM p",NULL);
            if (r==0) r=AQ("UPDATE operation_steps SET state='applied' WHERE operation_id=(SELECT uop FROM p) AND step_no=1",NULL);
            if (r==0) r=AQ("UPDATE launches SET state='starting',invocation_ref=(SELECT inv FROM p),revision=revision+1 WHERE operation_id=(SELECT uop FROM p)",NULL);
        } else if (r==0) {
            r=AQ("INSERT INTO operation_steps SELECT uop,3,'activate','launch',launch,'applied',evidence FROM p",NULL);
            if (r==0) r=AQ("UPDATE launches SET state='running',readiness='ready',revision=revision+1 WHERE operation_id=(SELECT uop FROM p)",NULL);
        }
        const char *update=stage==AE_USER_EXECUTION_REGISTER ? EXEC_INPUT "UPDATE operations SET reason_code='app_registered',revision=revision+1 WHERE operation_id=(SELECT uop FROM p)" :
            EXEC_INPUT "UPDATE operations SET reason_code='app_ready',revision=revision+1 WHERE operation_id=(SELECT uop FROM p)";
        if (r==0) r=execution_statement(store,p,hash,e,update,NULL);
        if (r==0) r=AQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    struct ae_user_launch_snapshot receipt;
    if (r==0) r=user_launch_row(store,p->user_operation,&receipt);
    r=fixture_commit(store,r); if (r==0) *output=receipt;
#undef AQ
    return r;
}
int ae_catalogue_execution(struct ae_catalogue *store,const struct ae_execution_profile *p,const struct ae_launch_context *context,
    enum ae_execution_stage stage,const struct ae_execution_evidence *e,struct ae_execution_result *output)
{
    if (output==NULL || stage<AE_EXECUTION_REQUEST || stage>AE_EXECUTION_ARM ||
        (stage!=AE_EXECUTION_REQUEST && (e==NULL || !nonzero(e->invocation)))) return -EINVAL;
    uint8_t hash[32]; int result=ae_execution_digest(p,hash); if (result!=0) return result;
    result=reservation_context(store,context);
    if (result==0 && (memcmp(p->system_epoch,store->identity.authority_epoch,16)!=0 || memcmp(p->system_runtime,context->system_runtime,16)!=0 ||
        memcmp(p->boot,context->boot,16)!=0 || memcmp(p->broker_epoch,context->broker_epoch,16)!=0 ||
        memcmp(p->broker_runtime,context->broker_runtime,16)!=0 || memcmp(p->account,context->account,16)!=0)) result=-ESTALE;
    if (result!=0) return result;
    result=exec_sql(store->db,"BEGIN IMMEDIATE"); if (result!=0) return result;
    int64_t match=0,one=0,two=0,three=0,steps=0,prepared=0;
#define EQ(sql,out) execution_statement(store,p,hash,e,EXEC_INPUT sql,out)
    result=EQ("SELECT count(*) " EXEC_TUPLE,&match);
    if (result==0 && match!=1) result=-ESTALE;
    if (result==0) result=EQ("SELECT count(*) " EXEC_TUPLE "WHERE o.state='running' AND d.state IN ('reserved','dispatching') AND d.close_cause_epoch IS NULL",&match);
    if (result==0 && match!=1) result=-EAGAIN;
    if (result==0) result=EQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=1",&one);
    if (result==0 && one) {
        result=EQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=1 AND action='stage' AND subject_kind='generation' AND subject_ref=p.g AND evidence_digest=p.hash AND state IN ('executing','applied')",&match);
        if (result==0 && match!=1) result=-EEXIST;
    }
    if (result==0 && stage>AE_EXECUTION_REQUEST && !one) result=-ENOENT;
    if (result==0) result=EQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=2",&two);
    if (result==0 && stage>AE_EXECUTION_REQUEST && two) {
        result=EQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=2 AND action='verify' AND subject_kind='integration' AND subject_ref=p.inv AND evidence_digest=p.evidence AND state IN ('executing','applied')",&match);
        if (result==0 && match!=1) result=-EEXIST;
    }
    if (result==0 && stage>AE_EXECUTION_BIND && !two) result=-ENOENT;
    if (result==0) result=EQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=3",&three);
    if (result==0) result=EQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no IN (1,2) AND state='applied'",&prepared);
    if (result==0) result=EQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no>0",&steps);
    if (result==0 && steps!=one+two+three) result=-EAGAIN;
    if (result==0 && three) {
        result=EQ("SELECT count(*) " EXEC_TUPLE "JOIN operation_steps s ON s.operation_id=p.op AND s.step_no=3 AND s.action='activate' AND s.subject_kind='launch' AND s.subject_ref=p.launch AND s.evidence_digest=p.hash AND s.state='executing' "
            "WHERE d.state='dispatching' AND l.state='running' AND d.invocation_ref=(SELECT subject_ref FROM operation_steps WHERE operation_id=p.op AND step_no=2) AND d.ready_data_gate_revision=p.dgate",&match);
        if (result==0 && (match!=1 || prepared!=2)) result=-EAGAIN;
    }
    int mutation=result==0 && ((stage==AE_EXECUTION_REQUEST && !one) || (stage==AE_EXECUTION_BIND && !two) ||
        (stage==AE_EXECUTION_PREPARE && prepared!=2) || (stage==AE_EXECUTION_ARM && !three));
    if (result==0 && mutation) {
        result=EQ("SELECT count(*) " EXEC_TUPLE "WHERE d.state='reserved' AND l.state='reserved' AND d.invocation_ref IS NULL AND d.ready_data_gate_revision IS NULL",&match);
        if (result==0 && match!=1) result=-EAGAIN;
        if (result==0 && (steps!=(stage==AE_EXECUTION_REQUEST ? 0 : stage==AE_EXECUTION_BIND ? 1 : 2) ||
            (stage==AE_EXECUTION_ARM && prepared!=2) || (stage==AE_EXECUTION_PREPARE && prepared!=0))) result=-EAGAIN;
        if (result==0 && (stage==AE_EXECUTION_REQUEST || stage==AE_EXECUTION_ARM)) {
            result=EQ("SELECT count(*) FROM installs i,admission_gates f,p WHERE i.install_id=p.i AND i.state='active' AND i.active_generation_id=p.g AND i.revision=p.ir AND f.subject_kind='install' AND f.subject_ref=p.i AND f.state='open' AND f.closing_operation_id IS NULL AND f.revision=p.igate",&match);
            if (result==0 && match!=1) result=-ESTALE;
        }
        struct ae_catalogue_info info;
        if (result==0) result=read_info(store,&info);
        if (result==0 && info.revision==INT64_MAX) result=-EOVERFLOW;
        if (result==0) result=EQ("SELECT max(o.revision,d.revision,l.revision) " EXEC_TUPLE,&match);
        if (result==0 && match==INT64_MAX) result=-EOVERFLOW;
        if (result==0 && stage==AE_EXECUTION_ARM) result=require_execution_process(store,p,hash,e,1);
        if (result==0 && stage==AE_EXECUTION_REQUEST) result=EQ("INSERT INTO operation_steps SELECT op,1,'stage','generation',g,'executing',hash FROM p",NULL);
        if (result==0 && stage==AE_EXECUTION_BIND) result=EQ("INSERT INTO operation_steps SELECT op,2,'verify','integration',inv,'executing',evidence FROM p",NULL);
        if (result==0 && stage==AE_EXECUTION_PREPARE) {
            result=EQ("UPDATE operation_steps SET state='applied' WHERE operation_id=(SELECT op FROM p) AND step_no=1",NULL);
            if (result==0) result=EQ("UPDATE operation_steps SET state='applied' WHERE operation_id=(SELECT op FROM p) AND step_no=2",NULL);
        }
        if (result==0 && stage==AE_EXECUTION_ARM) {
            result=EQ("INSERT INTO operation_steps SELECT op,3,'activate','launch',launch,'executing',hash FROM p",NULL);
            if (result==0) result=EQ("UPDATE launch_dispatches SET state='dispatching',invocation_ref=(SELECT inv FROM p),ready_data_gate_revision=(SELECT dgate FROM p),revision=revision+1 WHERE lease_id=(SELECT lease FROM p)",NULL);
            if (result==0) result=EQ("UPDATE generation_leases SET state='running',revision=revision+1 WHERE lease_id=(SELECT lease FROM p)",NULL);
        }
        const char *update=stage==AE_EXECUTION_REQUEST ? EXEC_INPUT "UPDATE operations SET reason_code='exec_requested',revision=revision+1 WHERE operation_id=(SELECT op FROM p)" :
            stage==AE_EXECUTION_BIND ? EXEC_INPUT "UPDATE operations SET reason_code='exec_bound',revision=revision+1 WHERE operation_id=(SELECT op FROM p)" :
            stage==AE_EXECUTION_PREPARE ? EXEC_INPUT "UPDATE operations SET reason_code='exec_prepared',revision=revision+1 WHERE operation_id=(SELECT op FROM p)" :
            EXEC_INPUT "UPDATE operations SET reason_code='exec_armed',revision=revision+1 WHERE operation_id=(SELECT op FROM p)";
        if (result==0) result=execution_statement(store,p,hash,e,update,NULL);
        if (result==0) result=EQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    struct ae_execution_result receipt={0};
    if (result==0) result=dispatch_row(store,p->lease,context,&receipt.dispatch);
    receipt.effect_required=mutation && (stage==AE_EXECUTION_REQUEST || stage==AE_EXECUTION_ARM);
    result=fixture_commit(store,result); if (result==0) *output=receipt;
#undef EQ
    return result;
}
int ae_catalogue_execution_report(struct ae_catalogue *store,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,const struct ae_execution_diagnostic *diagnostic,struct ae_execution_result *output)
{
    if (output==NULL) return -EINVAL;
    uint8_t hash[32],report[32]; char reason[64];
    int r=ae_execution_diagnostic_encode(p,e,diagnostic,report,reason);
    if (r==0) r=ae_execution_digest(p,hash);
    if (r==0) r=reservation_context(store,c);
    if (r==0 && (memcmp(p->system_epoch,store->identity.authority_epoch,16)!=0 || memcmp(p->system_runtime,c->system_runtime,16)!=0 ||
        memcmp(p->boot,c->boot,16)!=0 || memcmp(p->broker_epoch,c->broker_epoch,16)!=0 ||
        memcmp(p->broker_runtime,c->broker_runtime,16)!=0 || memcmp(p->account,c->account,16)!=0)) r=-ESTALE;
    if (r!=0) return r;
    r=exec_sql(store->db,"BEGIN IMMEDIATE"); if (r!=0) return r;
    int64_t match=0,present=0,two=0;
#define DQ(sql,out) execution_statement_extra(store,p,hash,e,report,reason,EXEC_INPUT sql,out)
    r=DQ("SELECT count(*) " EXEC_TUPLE "JOIN operation_steps s ON s.operation_id=p.op AND s.step_no=1 AND s.action='stage' AND s.subject_kind='generation' AND s.subject_ref=p.g AND s.evidence_digest=p.hash AND s.state IN ('executing','applied')",&match);
    if (r==0 && match!=1) r=-ESTALE;
    if (r==0) r=DQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=2",&two);
    if (r==0 && two) {
        r=DQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=2 AND action='verify' AND subject_kind='integration' AND subject_ref=p.inv AND evidence_digest=p.evidence AND state IN ('executing','applied')",&match);
        if (r==0 && match!=1) r=-EEXIST;
    }
    if (r==0 && !two) {
        r=DQ("SELECT count(*) " EXEC_TUPLE "WHERE d.state='reserved' AND d.invocation_ref IS NULL AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no=3)",&match);
        if (r==0 && match!=1) r=-EAGAIN;
    }
    if (r==0) r=DQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=4",&present);
    if (r==0 && present) {
        r=DQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=4 AND action='verify' AND subject_kind='integration' AND subject_ref=p.inv AND state='applied' AND evidence_digest=p.rhash",&match);
        if (r==0 && match!=1) r=-EEXIST;
    }
    if (r==0 && !present) {
        r=DQ("SELECT count(*) " EXEC_TUPLE "WHERE o.state='running' AND o.outcome IS NULL AND d.state IN ('reserved','dispatching','observed') AND l.state IN ('reserved','running') AND d.close_cause_epoch IS NULL AND (d.invocation_ref IS NULL OR d.invocation_ref=p.inv)",&match);
        if (r==0 && match!=1) r=-EAGAIN;
        if (r==0) r=DQ("SELECT max(m.catalogue_revision,o.revision) " EXEC_TUPLE,&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0) r=DQ("INSERT INTO operation_steps SELECT op,4,'verify','integration',inv,'applied',rhash FROM p",NULL);
        if (r==0) r=DQ("UPDATE operations SET reason_code=(SELECT reason FROM p),revision=revision+1 WHERE operation_id=(SELECT op FROM p)",NULL);
        if (r==0) r=DQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    struct ae_execution_result result={0};
    if (r==0) r=dispatch_row(store,p->lease,c,&result.dispatch);
    r=fixture_commit(store,r); if (r==0) *output=result;
#undef DQ
    return r;
}
int ae_catalogue_observe_execution(struct ae_catalogue *store,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,const uint8_t app_digest[32],struct ae_execution_result *output)
{
    if (output==NULL || e==NULL || !nonzero(e->invocation) || app_digest==NULL) return -EINVAL;
    uint8_t hash[32]; int r=ae_execution_digest(p,hash);
    if (r==0) r=reservation_context(store,c);
    if (r==0 && (memcmp(p->system_epoch,store->identity.authority_epoch,16)!=0 || memcmp(p->system_runtime,c->system_runtime,16)!=0 ||
        memcmp(p->boot,c->boot,16)!=0 || memcmp(p->broker_epoch,c->broker_epoch,16)!=0 ||
        memcmp(p->broker_runtime,c->broker_runtime,16)!=0 || memcmp(p->account,c->account,16)!=0)) r=-ESTALE;
    if (r!=0) return r;
    r=exec_sql(store->db,"BEGIN IMMEDIATE"); if (r!=0) return r;
    int64_t match=0,present=0;
#define OQ(sql,out) execution_statement_extra(store,p,hash,e,app_digest,NULL,EXEC_INPUT sql,out)
    r=OQ("SELECT count(*) " EXEC_TUPLE "JOIN operation_steps s ON s.operation_id=p.op AND s.step_no=1 AND s.action='stage' AND s.subject_kind='generation' AND s.subject_ref=p.g AND s.state='applied' AND s.evidence_digest=p.hash "
        "JOIN operation_steps b ON b.operation_id=p.op AND b.step_no=2 AND b.action='verify' AND b.subject_kind='integration' AND b.subject_ref=p.inv AND b.state='applied' AND b.evidence_digest=p.evidence "
        "JOIN operation_steps arm ON arm.operation_id=p.op AND arm.step_no=3 AND arm.action='activate' AND arm.subject_kind='launch' AND arm.subject_ref=p.launch AND arm.state IN ('executing','applied') AND arm.evidence_digest=p.hash "
        "WHERE d.invocation_ref=p.inv AND d.ready_data_gate_revision=p.dgate",&match);
    if (r==0 && match!=1) r=-ESTALE;
    if (r==0) r=OQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=5",&present);
    if (r==0 && present) {
        r=OQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=5 AND action='verify' AND subject_kind='launch' AND subject_ref=p.launch AND state='applied' AND evidence_digest=p.rhash",&match);
        if (r==0 && match!=1) r=-EEXIST;
        if (r==0) r=OQ("SELECT count(*) " EXEC_TUPLE "JOIN operation_steps arm ON arm.operation_id=p.op AND arm.step_no=3 AND arm.state='applied' WHERE d.state IN ('observed','closing','terminal','uncertain') AND (l.state='running' OR (l.state='released' AND d.state='terminal'))",&match);
        if (r==0 && match!=1) r=-EAGAIN;
    }
    if (r==0 && !present) {
        r=OQ("SELECT count(*) " EXEC_TUPLE "JOIN installs i ON i.install_id=p.i AND i.state='active' AND i.active_generation_id=p.g AND i.revision=p.ir "
            "JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=p.i AND f.state='open' AND f.closing_operation_id IS NULL AND f.revision=p.igate "
            "JOIN operation_steps arm ON arm.operation_id=p.op AND arm.step_no=3 AND arm.state='executing' "
            "WHERE o.state='running' AND o.outcome IS NULL AND d.state='dispatching' AND d.close_cause_epoch IS NULL AND l.state='running' "
            "AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no>4) "
            "AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no=4 AND (action<>'verify' OR subject_kind<>'integration' OR subject_ref<>p.inv OR state<>'applied' OR evidence_digest IS NULL))",&match);
        if (r==0 && match!=1) r=-EAGAIN;
        if (r==0) r=OQ("SELECT max(m.catalogue_revision,o.revision,d.revision) " EXEC_TUPLE,&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0) r=require_execution_process(store,p,hash,e,2);
        if (r==0) r=OQ("INSERT INTO operation_steps SELECT op,5,'verify','launch',launch,'applied',rhash FROM p",NULL);
        if (r==0) r=OQ("UPDATE operation_steps SET state='applied' WHERE operation_id=(SELECT op FROM p) AND step_no=3",NULL);
        if (r==0) r=OQ("UPDATE launch_dispatches SET state='observed',revision=revision+1 WHERE lease_id=(SELECT lease FROM p)",NULL);
        if (r==0) r=OQ("UPDATE operations SET reason_code=CASE WHEN EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=(SELECT op FROM p) AND step_no=4) THEN reason_code ELSE 'exec_observed' END,revision=revision+1 WHERE operation_id=(SELECT op FROM p)",NULL);
        if (r==0) r=OQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    struct ae_execution_result result={0};
    if (r==0) r=dispatch_row(store,p->lease,c,&result.dispatch);
    r=fixture_commit(store,r); if (r==0) *output=result;
#undef OQ
    return r;
}
int ae_catalogue_complete_execution(struct ae_catalogue *store,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,const uint8_t app_digest[32],int64_t stamp,struct ae_execution_completion *output)
{
    if (output==NULL || e==NULL || !nonzero(e->invocation) || app_digest==NULL || stamp<0) return -EINVAL;
    uint8_t hash[32],diagnostic[32],bytes[AE_EXECUTION_COMPLETION_BYTES],receipt_hash[32]; char reason[64];
    struct ae_execution_diagnostic unknown={.kind=AE_EXECUTION_DIAGNOSTIC_UNKNOWN};
    int r=ae_execution_digest(p,hash);
    if (r==0) r=ae_execution_diagnostic_encode(p,e,&unknown,diagnostic,reason);
    if (r==0) r=reservation_context(store,c);
    if (r==0 && (memcmp(p->system_epoch,store->identity.authority_epoch,16)!=0 || memcmp(p->system_runtime,c->system_runtime,16)!=0 ||
        memcmp(p->boot,c->boot,16)!=0 || memcmp(p->broker_epoch,c->broker_epoch,16)!=0 || memcmp(p->broker_runtime,c->broker_runtime,16)!=0 || memcmp(p->account,c->account,16)!=0)) r=-ESTALE;
    if (r!=0) return r;
    r=exec_sql(store->db,"BEGIN IMMEDIATE"); if (r!=0) return r;
    int64_t match=0,present=0; const uint8_t *record_hash=app_digest;
#define CQ(sql,out) execution_statement_full(store,p,hash,e,record_hash,reason,&stamp,EXEC_INPUT sql,out)
    r=CQ("SELECT count(*) " EXEC_TUPLE "JOIN operation_steps req ON req.operation_id=p.op AND req.step_no=1 AND req.action='stage' AND req.subject_kind='generation' AND req.subject_ref=p.g AND req.state='applied' AND req.evidence_digest=p.hash "
        "JOIN operation_steps pin ON pin.operation_id=p.op AND pin.step_no=2 AND pin.action='verify' AND pin.subject_kind='integration' AND pin.subject_ref=p.inv AND pin.state='applied' AND pin.evidence_digest=p.evidence "
        "JOIN operation_steps arm ON arm.operation_id=p.op AND arm.step_no=3 AND arm.action='activate' AND arm.subject_kind='launch' AND arm.subject_ref=p.launch AND arm.state='applied' AND arm.evidence_digest=p.hash "
        "JOIN operation_steps obs ON obs.operation_id=p.op AND obs.step_no=5 AND obs.action='verify' AND obs.subject_kind='launch' AND obs.subject_ref=p.launch AND obs.state='applied' AND obs.evidence_digest=p.rhash "
        "WHERE d.invocation_ref=p.inv AND d.ready_data_gate_revision=p.dgate",&match);
    if (r==0 && match!=1) r=-ESTALE;
    if (r==0) r=CQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=6",&present);
    record_hash=diagnostic;
    if (r==0) r=CQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=4 AND action='verify' AND subject_kind='integration' AND subject_ref=p.inv AND state='applied' AND evidence_digest=p.rhash",&match);
    if (r==0 && match!=1) r=-EAGAIN;
    struct ae_execution_completion result={0};
    const uint8_t *sources[]={p->system_epoch,p->system_runtime,p->boot,p->root_operation,p->launch,p->lease,e->invocation};
    uint8_t *targets[]={result.system_epoch,result.system_runtime,result.boot,result.operation,result.launch,result.lease,result.invocation};
    for (size_t n=0;n<7;++n) memcpy(targets[n],sources[n],16);
    memcpy(result.profile_digest,hash,32); memcpy(result.observation_digest,app_digest,32);
    if (r==0) r=CQ("SELECT o.revision " EXEC_TUPLE,&result.revision);
    if (r==0 && present) r=CQ("SELECT o.completed_unix_ms " EXEC_TUPLE,&result.completed_unix_ms);
    if (r==0 && !present) {
        r=CQ("SELECT count(*) " EXEC_TUPLE "JOIN installs i ON i.install_id=p.i AND i.state='active' AND i.active_generation_id=p.g AND i.revision=p.ir "
            "JOIN admission_gates f ON f.subject_kind='install' AND f.subject_ref=p.i AND f.state='open' AND f.closing_operation_id IS NULL AND f.revision=p.igate "
            "WHERE d.state='observed' AND d.close_cause_epoch IS NULL AND l.state='running' AND o.state='running' AND o.outcome IS NULL AND o.accepted_unix_ms<=?25 AND o.reason_code=p.reason "
            "AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no>5)",&match);
        if (r==0 && match!=1) r=-EAGAIN;
        if (r==0) r=CQ("SELECT max(m.catalogue_revision,o.revision) " EXEC_TUPLE,&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0) { ++result.revision; result.completed_unix_ms=stamp; }
    }
    if (r==0) r=ae_execution_completion_encode(&result,bytes);
    if (r==0) r=ae_sha256_bytes(bytes,sizeof bytes,receipt_hash);
    record_hash=receipt_hash;
    if (r==0 && present) {
        r=CQ("SELECT count(*) " EXEC_TUPLE "JOIN operation_steps done ON done.operation_id=p.op AND done.step_no=6 AND done.action='reconcile' AND done.subject_kind='launch' AND done.subject_ref=p.launch AND done.state='applied' AND done.evidence_digest=p.rhash "
            "WHERE o.state='completed' AND o.outcome='applied' AND o.result_kind='launch' AND o.result_ref=p.launch AND d.state IN ('observed','closing','terminal','uncertain') AND (l.state='running' OR (l.state='released' AND d.state='terminal'))",&match);
        if (r==0 && match!=1) r=-EEXIST;
    }
    if (r==0 && !present) {
        r=CQ("INSERT INTO operation_steps SELECT op,6,'reconcile','launch',launch,'applied',rhash FROM p",NULL);
        if (r==0) r=CQ("UPDATE operations SET state='completed',phase='reconcile',outcome='applied',completed_unix_ms=?25,result_kind='launch',result_ref=(SELECT launch FROM p),revision=revision+1 WHERE operation_id=(SELECT op FROM p)",NULL);
        if (r==0) r=CQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    r=fixture_commit(store,r); if (r==0) *output=result;
#undef CQ
    return r;
}

int ae_catalogue_finish_user_execution(struct ae_catalogue *store,const struct ae_execution_profile *p,const struct ae_execution_evidence *e,
    const uint8_t app_digest[32],const struct ae_execution_completion *receipt,int64_t stamp,struct ae_user_launch_snapshot *output)
{
    if (output==NULL || stamp<0) return -EINVAL;
    int r=ae_execution_completion_match(p,e,app_digest,receipt);
    uint8_t hash[32],bytes[AE_EXECUTION_COMPLETION_BYTES],receipt_hash[32];
    if (r==0) r=ae_execution_completion_encode(receipt,bytes);
    if (r==0) r=ae_sha256_bytes(bytes,sizeof bytes,receipt_hash);
    if (r!=0) return r;
    r=user_execution_begin(store,p,hash,1); if (r!=0) return r;
    int64_t match=0,present=0;
#define FQ(sql,out) execution_statement_full(store,p,hash,e,receipt_hash,NULL,&stamp,EXEC_INPUT sql,out)
#define USER_RESULT_TUPLE "FROM p JOIN authority_meta m ON m.singleton=1 AND m.authority_epoch=p.be AND m.owner_scope_ref=p.account AND m.runtime_incarnation=p.br AND m.current_boot_id=p.boot " \
    "JOIN launches l ON l.operation_id=p.uop AND l.launch_id=p.launch AND l.lease_ref=p.lease AND l.system_epoch=p.se AND l.boot_id=p.boot AND l.install_id=p.i AND l.generation_ref=p.g AND l.data_epoch_id=p.d AND l.invocation_ref=p.inv AND l.session_ref='p0' AND l.seat_ref='none' " \
    "JOIN operations o ON o.operation_id=p.uop JOIN plans plan ON plan.plan_id=o.plan_id AND plan.authority_epoch=p.be AND plan.scope_ref=p.account AND plan.kind='launch' AND plan.action_id='run' " \
    "JOIN operation_links x ON x.operation_id=p.uop AND x.link_no=0 AND x.remote_epoch=p.se AND x.remote_operation_ref=p.op " \
    "JOIN operation_steps intent ON intent.operation_id=p.uop AND intent.step_no=0 AND intent.action='verify' AND intent.subject_kind='launch' AND intent.subject_ref=p.launch AND intent.state='applied' AND intent.evidence_digest=p.fp " \
    "JOIN operation_steps prep ON prep.operation_id=p.uop AND prep.step_no=1 AND prep.action='stage' AND prep.subject_kind='data' AND prep.subject_ref=p.d AND prep.state='applied' AND prep.evidence_digest=p.hash " \
    "JOIN operation_steps reg ON reg.operation_id=p.uop AND reg.step_no=2 AND reg.action='verify' AND reg.subject_kind='integration' AND reg.subject_ref=p.inv AND reg.state='applied' AND reg.evidence_digest=p.evidence " \
    "JOIN operation_steps ready ON ready.operation_id=p.uop AND ready.step_no=3 AND ready.action='activate' AND ready.subject_kind='launch' AND ready.subject_ref=p.launch AND ready.state='applied' AND ready.evidence_digest=p.evidence "
    r=FQ("SELECT count(*) " USER_RESULT_TUPLE,&match);
    if (r==0 && match!=1) r=-ESTALE;
    if (r==0) r=FQ("SELECT count(*) FROM operation_steps,p WHERE operation_id=p.uop AND step_no=4",&present);
    if (r==0 && present) {
        r=FQ("SELECT count(*) " USER_RESULT_TUPLE "JOIN operation_steps done ON done.operation_id=p.uop AND done.step_no=4 AND done.action='reconcile' AND done.subject_kind='launch' AND done.subject_ref=p.launch AND done.state='applied' AND done.evidence_digest=p.rhash "
            "WHERE o.state='completed' AND o.outcome='applied' AND o.result_kind='launch' AND o.result_ref=p.launch AND x.state='completed'",&match);
        if (r==0 && match!=1) r=-EEXIST;
    }
    if (r==0 && !present) {
        r=user_execution_fence(store,p,hash,e);
        if (r==0) r=FQ("SELECT count(*) " USER_RESULT_TUPLE "WHERE l.state='running' AND l.readiness='ready' AND x.state='accepted' AND o.accepted_unix_ms<=?25",&match);
        if (r==0 && match!=1) r=-EAGAIN;
        if (r==0) r=FQ("SELECT max(m.catalogue_revision,o.revision) " USER_RESULT_TUPLE,&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0) r=FQ("INSERT INTO operation_steps SELECT uop,4,'reconcile','launch',launch,'applied',rhash FROM p",NULL);
        if (r==0) r=FQ("UPDATE operation_links SET state='completed' WHERE operation_id=(SELECT uop FROM p) AND link_no=0",NULL);
        if (r==0) r=FQ("UPDATE operations SET state='completed',phase='reconcile',reason_code='launch_started',outcome='applied',completed_unix_ms=?25,result_kind='launch',result_ref=(SELECT launch FROM p),revision=revision+1 WHERE operation_id=(SELECT uop FROM p)",NULL);
        if (r==0) r=FQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    struct ae_user_launch_snapshot result;
    if (r==0) r=user_launch_row(store,p->user_operation,&result);
    r=fixture_commit(store,r); if (r==0) *output=result;
#undef USER_RESULT_TUPLE
#undef FQ
    return r;
}
static int process_catalogue_begin(struct ae_catalogue *s,const struct ae_execution_profile *p,int write)
{
    uint8_t raw[AE_EXECUTION_BYTES]; int r=ae_execution_encode(p,raw);
    if (r==0) r=still_named(s);
    if (r==0) r=exec_sql(s->db,write ? "BEGIN IMMEDIATE" : "BEGIN");
    if (r!=0) return r;
    sqlite3_int64 version=0; r=scalar_int(s->db,"PRAGMA user_version",&version);
    if (r==0 && version!=4 && !((version==5 || version==6) && s->identity.kind==AE_CATALOGUE_SYSTEM)) r=-EPROTONOSUPPORT;
    struct ae_catalogue_info info;
    if (r==0) r=read_info(s,&info);
    int system=s->identity.kind==AE_CATALOGUE_SYSTEM;
    if (r==0 && (memcmp(info.authority_epoch,system ? p->system_epoch : p->broker_epoch,16)!=0 ||
        (!system && memcmp(s->identity.owner,p->account,16)!=0) || memcmp(info.boot_id,p->boot,16)!=0 ||
        (write==1 && memcmp(info.runtime_incarnation,system ? p->system_runtime : p->broker_runtime,16)!=0))) r=-ESTALE;
    return r==0 ? 0 : fixture_commit(s,r);
}
static int process_catalogue_row(struct ae_catalogue *s,const struct ae_execution_profile *p,enum ae_execution_process_role role,struct ae_execution_process_snapshot *output)
{
    sqlite3_stmt *q=NULL; int r=failure(sqlite3_prepare_v2(s->db,
        "SELECT profile,profile_digest,invocation_ref,context_digest,process_identity,witness,witness_digest,recorded_unix_ms,start_operation_id FROM execution_processes WHERE launch_id=? AND role=?",-1,&q,NULL));
    if (r==0) r=failure(sqlite3_bind_blob(q,1,p->launch,16,SQLITE_TRANSIENT));
    if (r==0) r=failure(sqlite3_bind_int(q,2,(int)role));
    if (r==0) { int code=sqlite3_step(q); if (code!=SQLITE_ROW) r=code==SQLITE_DONE ? -ENOENT : failure(code); }
    struct ae_execution_process_snapshot result={0};
    uint8_t profile[384],expected[384],hash[32],value[32],operation[16];
    const uint8_t *raw=NULL; int bytes=0;
    if (r==0) r=projection_blob(q,0,profile,384);
    if (r==0) r=ae_execution_encode(p,expected);
    if (r==0 && memcmp(profile,expected,384)!=0) r=-ESTALE;
    if (r==0) r=ae_sha256_bytes(profile,384,hash);
    if (r==0) r=projection_blob(q,1,value,32);
    if (r==0 && memcmp(value,hash,32)!=0) r=-EUCLEAN;
    if (r==0) {
        bytes=sqlite3_column_bytes(q,5); raw=sqlite3_column_blob(q,5);
        if (sqlite3_column_type(q,5)!=SQLITE_BLOB || bytes<149 || bytes>AE_EXECUTION_PROCESS_MAX) r=-EUCLEAN;
    }
    if (r==0) r=ae_execution_process_decode(raw,(size_t)bytes,&result.process);
    if (r==0) r=ae_sha256_bytes(raw,(size_t)bytes,hash);
    if (r==0) r=projection_blob(q,6,value,32);
    if (r==0 && memcmp(value,hash,32)!=0) r=-EUCLEAN;
    struct ae_execution_evidence evidence={0};
    if (r==0) r=projection_blob(q,2,evidence.invocation,16);
    if (r==0) r=projection_blob(q,3,evidence.context_digest,32);
    if (r==0) r=ae_execution_process_match(p,&evidence,&result.process);
    if (r==0) r=projection_blob(q,4,value,32);
    if (r==0 && (result.process.role!=role || memcmp(value,raw+44,32)!=0)) r=-EUCLEAN;
    if (r==0) r=projection_blob(q,8,operation,16);
    if (r==0 && memcmp(operation,s->identity.kind==AE_CATALOGUE_SYSTEM ? p->root_operation : p->user_operation,16)!=0) r=-EUCLEAN;
    if (r==0 && (sqlite3_column_type(q,7)!=SQLITE_INTEGER || sqlite3_column_int64(q,7)<0)) r=-EUCLEAN;
    if (r==0) { result.recorded_unix_ms=sqlite3_column_int64(q,7); int code=sqlite3_step(q); if (code!=SQLITE_DONE) r=code==SQLITE_ROW ? -EUCLEAN : failure(code); }
    int final=failure(sqlite3_finalize(q)); if (r==0) r=final;
    if (r==0) *output=result;
    return r;
}
int ae_catalogue_get_execution_process(struct ae_catalogue *s,const struct ae_execution_profile *p,enum ae_execution_process_role role,struct ae_execution_process_snapshot *output)
{
    if (!output || (role!=AE_EXECUTION_STARTER && role!=AE_EXECUTION_APP)) return -EINVAL;
    int r=process_catalogue_begin(s,p,0); if (r!=0) return r;
    struct ae_execution_process_snapshot result;
    r=process_catalogue_row(s,p,role,&result); r=fixture_commit(s,r);
    if (r==0) *output=result;
    return r;
}
int ae_catalogue_enroll_execution_process(struct ae_catalogue *s,const struct ae_execution_profile *p,
    const struct ae_execution_evidence *e,const struct ae_execution_process *process,int64_t stamp,struct ae_execution_process_snapshot *output)
{
    if (!output || stamp<0) return -EINVAL;
    int r=ae_execution_process_match(p,e,process); if (r!=0) return r;
    uint8_t raw[AE_EXECUTION_PROCESS_MAX],profile[384],hash[32],digest[32]; size_t size=0;
    r=ae_execution_process_encode(process,raw,sizeof raw,&size);
    if (r==0) r=ae_execution_encode(p,profile);
    if (r==0) r=ae_execution_digest(p,hash);
    if (r==0) r=ae_sha256_bytes(raw,size,digest);
    if (r!=0) return r;
    r=process_catalogue_begin(s,p,1); if (r!=0) return r;
    struct ae_execution_process_snapshot result;
    r=process_catalogue_row(s,p,process->role,&result);
    if (r==0) {
        uint8_t original[AE_EXECUTION_PROCESS_MAX]; size_t length=0; r=ae_execution_process_encode(&result.process,original,sizeof original,&length);
        if (r==0 && (length!=size || memcmp(raw,original,size)!=0)) r=-EEXIST;
    } else if (r==-ESTALE) r=-EEXIST;
    else if (r==-ENOENT) {
        int64_t match=0;
        if (s->identity.kind==AE_CATALOGUE_SYSTEM) {
            r=execution_statement(s,p,hash,e,EXEC_INPUT "SELECT count(*) " EXEC_TUPLE
                "JOIN scopes account ON account.scope_id=p.account AND account.kind='account' AND account.state='active' AND account.os_uid=1000 "
                "JOIN operation_steps bound ON bound.operation_id=p.op AND bound.step_no=2 AND bound.action='verify' AND bound.subject_kind='integration' AND bound.subject_ref=p.inv AND bound.evidence_digest=p.evidence "
                "JOIN operation_steps requested ON requested.operation_id=p.op AND requested.step_no=1 AND requested.action='stage' AND requested.subject_kind='generation' AND requested.subject_ref=p.g AND requested.evidence_digest=p.hash "
                "WHERE o.state='running' AND d.close_cause_epoch IS NULL",&match);
            if (r==0 && match==1) r=execution_statement(s,p,hash,e,process->role==AE_EXECUTION_STARTER ?
                EXEC_INPUT "SELECT count(*) " EXEC_TUPLE "WHERE d.state='reserved' AND d.invocation_ref IS NULL AND l.state='reserved' AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no>=3)" :
                EXEC_INPUT "SELECT count(*) " EXEC_TUPLE "WHERE d.state='dispatching' AND d.invocation_ref=p.inv AND l.state='running' AND EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no=3 AND action='activate' AND subject_kind='launch' AND subject_ref=p.launch AND evidence_digest=p.hash AND state='executing')",&match);
            if (r==0 && match==1 && process->role==AE_EXECUTION_APP) {
                struct ae_execution_process_snapshot starter;
                r=process_catalogue_row(s,p,AE_EXECUTION_STARTER,&starter);
                if (r==0 && (process->pidfd_id!=starter.process.pidfd_id || process->pid!=starter.process.pid || process->cgroup_id!=starter.process.cgroup_id || strcmp(process->cgroup,starter.process.cgroup)!=0)) r=-ESTALE;
            }
        } else {
            r=process->role==AE_EXECUTION_APP ? user_execution_fence(s,p,hash,e) : -EPERM;
            if (r==0) match=1;
        }
        if (r==0 && match!=1) r=-ESTALE;
        struct ae_catalogue_info info;
        if (r==0) r=read_info(s,&info);
        if (r==0 && info.revision==INT64_MAX) r=-EOVERFLOW;
        sqlite3_stmt *q=NULL;
        if (r==0) r=failure(sqlite3_prepare_v2(s->db,"INSERT INTO execution_processes VALUES(?,?,?,?,?,?,?,?,?,?,?)",-1,&q,NULL));
        const uint8_t *op=s->identity.kind==AE_CATALOGUE_SYSTEM ? p->root_operation : p->user_operation;
        const void *blobs[]={p->launch,op,profile,hash,e->invocation,e->context_digest,raw+44,raw,digest};
        const int columns[]={1,3,4,5,6,7,8,9,10},lengths[]={16,16,384,32,16,32,32,(int)size,32};
        for (size_t n=0;r==0 && n<9;++n) r=failure(sqlite3_bind_blob(q,columns[n],blobs[n],lengths[n],SQLITE_TRANSIENT));
        if (r==0) r=failure(sqlite3_bind_int(q,2,(int)process->role));
        if (r==0) r=failure(sqlite3_bind_int64(q,11,stamp));
        if (r==0) { int code=sqlite3_step(q); r=code==SQLITE_DONE ? 0 : failure(code); if (r==0 && sqlite3_changes(s->db)!=1) r=-EUCLEAN; }
        int final=failure(sqlite3_finalize(q)); if (r==0) r=final;
        if (r==0) r=execution_statement(s,p,hash,e,EXEC_INPUT "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
        if (r==0) r=process_catalogue_row(s,p,process->role,&result);
    }
    r=fixture_commit(s,r); if (r==0) *output=result;
    return r;
}
static int handoff_begin(struct ae_catalogue *s,const struct ae_execution_profile *p,int write)
{
    if (!s || s->identity.kind!=AE_CATALOGUE_SYSTEM) return -EPERM;
    int r=process_catalogue_begin(s,p,write); if (r!=0) return r;
    sqlite3_int64 version=0; r=scalar_int(s->db,"PRAGMA user_version",&version);
    if (r==0 && version!=5 && version!=6) r=-EPROTONOSUPPORT;
    return r==0 ? 0 : fixture_commit(s,r);
}
static int handoff_row(struct ae_catalogue *s,const struct ae_execution_profile *p,struct ae_handoff_snapshot *output)
{
    sqlite3_stmt *q=NULL; int r=failure(sqlite3_prepare_v2(s->db,
        "SELECT profile,profile_digest,process_digest,witness,witness_digest,state,original_runtime,boot_id,recorded_unix_ms,"
        "observer_runtime,observed_unix_ms,retirement_runtime,retiring_unix_ms,retired_runtime,retired_unix_ms,revision FROM execution_handoffs WHERE launch_id=?",-1,&q,NULL));
    if (r==0) r=failure(sqlite3_bind_blob(q,1,p->launch,16,SQLITE_TRANSIENT));
    if (r==0) { int code=sqlite3_step(q); if (code!=SQLITE_ROW) r=code==SQLITE_DONE ? -ENOENT : failure(code); }
    struct ae_handoff_snapshot result={0}; uint8_t profile[384],expected[384],hash[32],value[32];
    if (r==0) r=projection_blob(q,0,profile,384);
    if (r==0) r=ae_execution_encode(p,expected);
    if (r==0 && memcmp(profile,expected,384)!=0) r=-ESTALE;
    if (r==0) r=ae_sha256_bytes(profile,384,hash);
    if (r==0) r=projection_blob(q,1,value,32);
    if (r==0 && memcmp(hash,value,32)!=0) r=-EUCLEAN;
    if (r==0) {
        int size=sqlite3_column_bytes(q,3); const uint8_t *raw=sqlite3_column_blob(q,3);
        if (sqlite3_column_type(q,3)!=SQLITE_BLOB || size<374 || size>AE_HANDOFF_MAX) r=-EUCLEAN;
        if (r==0 && ae_handoff_decode(raw,(size_t)size,&result.original)!=0) r=-EUCLEAN;
        if (r==0) r=ae_sha256_bytes(raw,(size_t)size,hash);
    }
    if (r==0) r=projection_blob(q,4,value,32);
    if (r==0 && memcmp(hash,value,32)!=0) r=-EUCLEAN;
    if (r==0) r=ae_handoff_match(p,&result.original);
    struct ae_execution_process_snapshot process;
    if (r==0) r=process_catalogue_row(s,p,AE_EXECUTION_APP,&process);
    uint8_t a[AE_EXECUTION_PROCESS_MAX],b[AE_EXECUTION_PROCESS_MAX]; size_t as=0,bs=0;
    if (r==0) r=ae_execution_process_encode(&process.process,a,sizeof a,&as);
    if (r==0) r=ae_execution_process_encode(&result.original.process,b,sizeof b,&bs);
    if (r==0 && (as!=bs || memcmp(a,b,as)!=0)) r=-EUCLEAN;
    if (r==0) r=ae_sha256_bytes(a,as,hash);
    if (r==0) r=projection_blob(q,2,value,32);
    if (r==0 && memcmp(hash,value,32)!=0) r=-EUCLEAN;
    if (r==0) {
        const char *state=(const char *)sqlite3_column_text(q,5);
        const char *states[]={"pending","observed","retiring","retired"};
        for (int n=0;state && n<4;++n) if (sqlite3_column_type(q,5)==SQLITE_TEXT &&
            sqlite3_column_bytes(q,5)==(int)strlen(states[n]) && memcmp(state,states[n],strlen(states[n]))==0) result.state=(enum ae_handoff_state)(n+1);
        if (!result.state) r=-EUCLEAN;
    }
    if (r==0) r=projection_blob(q,6,value,16);
    if (r==0 && memcmp(value,p->system_runtime,16)!=0) r=-EUCLEAN;
    if (r==0) r=projection_blob(q,7,value,16);
    if (r==0 && memcmp(value,p->boot,16)!=0) r=-EUCLEAN;
    if (r==0 && (sqlite3_column_type(q,8)!=SQLITE_INTEGER || sqlite3_column_int64(q,8)<0 ||
        sqlite3_column_type(q,15)!=SQLITE_INTEGER || sqlite3_column_int64(q,15)<1)) r=-EUCLEAN;
    if (r==0) { result.recorded_unix_ms=sqlite3_column_int64(q,8); result.revision=sqlite3_column_int64(q,15); }
    uint8_t *ids[]={result.observer_runtime,result.retirement_runtime,result.retired_runtime};
    int64_t *times[]={&result.observed_unix_ms,&result.retiring_unix_ms,&result.retired_unix_ms};
    int present[3]={0};
    for (int n=0;r==0 && n<3;++n) {
        int column=9+2*n; present[n]=sqlite3_column_type(q,column)!=SQLITE_NULL;
        if (!present[n]) { if (sqlite3_column_type(q,column+1)!=SQLITE_NULL) r=-EUCLEAN; continue; }
        r=projection_blob(q,column,ids[n],16);
        if (r==0 && ((ids[n][6]>>4)!=4 || (ids[n][8]&0xc0)!=0x80 ||
            sqlite3_column_type(q,column+1)!=SQLITE_INTEGER || sqlite3_column_int64(q,column+1)<0)) r=-EUCLEAN;
        if (r==0) *times[n]=sqlite3_column_int64(q,column+1);
    }
    if (r==0 && ((result.state==AE_HANDOFF_PENDING && result.revision!=1) ||
        (result.state!=AE_HANDOFF_PENDING && result.revision<2) || (result.state==AE_HANDOFF_FINISHED && result.revision<3) ||
        (present[0] && memcmp(result.observer_runtime,p->system_runtime,16)==0) ||
        (result.state==AE_HANDOFF_PENDING && (present[0] || present[1] || present[2])) ||
        (result.state==AE_HANDOFF_OBSERVED && (!present[0] || present[1] || present[2])) ||
        (result.state==AE_HANDOFF_RETIRING && (!present[1] || present[2])) ||
        (result.state==AE_HANDOFF_FINISHED && (!present[1] || !present[2])))) r=-EUCLEAN;
    if (r==0 && sqlite3_step(q)!=SQLITE_DONE) r=-EUCLEAN;
    int final=failure(sqlite3_finalize(q)); if (r==0) r=final;
    if (r==0) *output=result;
    return r;
}
int ae_catalogue_get_handoff(struct ae_catalogue *s,const struct ae_execution_profile *p,struct ae_handoff_snapshot *output)
{
    if (!output) return -EINVAL;
    int r=handoff_begin(s,p,0); if (r!=0) return r;
    struct ae_handoff_snapshot result; r=handoff_row(s,p,&result);
    r=fixture_commit(s,r); if (r==0) *output=result; return r;
}
#define HANDOFF_INPUT "WITH h(launch,profile,pdigest,process,witness,digest,original,boot,stamp,current) AS (VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)) "
struct handoff_values { uint8_t profile[384],phash[32],process[32],raw[AE_HANDOFF_MAX],digest[32],runtime[16]; size_t size; int64_t stamp; };
static int handoff_statement(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct handoff_values *v,const char *sql,int64_t *count)
{
    sqlite3_stmt *q=NULL; int r=failure(sqlite3_prepare_v2(s->db,sql,-1,&q,NULL));
    const void *blobs[]={p->launch,v->profile,v->phash,v->process,v->raw,v->digest,p->system_runtime,p->boot,v->runtime};
    const int columns[]={1,2,3,4,5,6,7,8,10},sizes[]={16,384,32,32,(int)v->size,32,16,16,16};
    for (size_t n=0;r==0 && n<9;++n) r=failure(sqlite3_bind_blob(q,columns[n],blobs[n],sizes[n],SQLITE_TRANSIENT));
    if (r==0) r=failure(sqlite3_bind_int64(q,9,v->stamp));
    if (r==0) {
        int code=sqlite3_step(q);
        if (count) {
            if (code!=SQLITE_ROW) r=code==SQLITE_DONE ? -EUCLEAN : failure(code);
            else if (sqlite3_column_type(q,0)!=SQLITE_INTEGER) r=-EUCLEAN;
            else { *count=sqlite3_column_int64(q,0); if (sqlite3_step(q)!=SQLITE_DONE) r=-EUCLEAN; }
        } else { r=code==SQLITE_DONE ? 0 : failure(code); if (r==0 && sqlite3_changes(s->db)!=1) r=-EUCLEAN; }
    }
    int final=failure(sqlite3_finalize(q)); return r==0 ? final : r;
}
#define HQ(sql,out) handoff_statement(s,p,&v,HANDOFF_INPUT sql,out)
int ae_catalogue_handoff(struct ae_catalogue *s,const struct ae_execution_profile *p,enum ae_handoff_stage stage,
    const struct ae_execution_handoff *original,int64_t stamp,struct ae_handoff_result *output)
{
    if (!output || !original || stamp<0 || stage<AE_HANDOFF_PREPARE || stage>AE_HANDOFF_RETIRED) return -EINVAL;
    struct handoff_values v={.stamp=stamp}; int r=ae_handoff_match(p,original);
    if (r==0) r=ae_handoff_encode(original,v.raw,sizeof v.raw,&v.size);
    if (r==0) r=ae_sha256_bytes(v.raw,v.size,v.digest);
    if (r==0) r=ae_execution_encode(p,v.profile);
    if (r==0) r=ae_sha256_bytes(v.profile,sizeof v.profile,v.phash);
    uint8_t proc[AE_EXECUTION_PROCESS_MAX]; size_t size=0;
    if (r==0) r=ae_execution_process_encode(&original->process,proc,sizeof proc,&size);
    if (r==0) r=ae_sha256_bytes(proc,size,v.process);
    if (r==0) r=handoff_begin(s,p,stage==AE_HANDOFF_PREPARE ? 1 : 2);
    if (r!=0) return r;
    struct ae_catalogue_info info; r=read_info(s,&info); if (r==0) memcpy(v.runtime,info.runtime_incarnation,16);
    struct ae_handoff_result result={0}; int fresh=0;
    if (r==0) r=handoff_row(s,p,&result.snapshot);
    if (r==-ENOENT && stage==AE_HANDOFF_PREPARE) {
        struct ae_execution_process_snapshot process; r=process_catalogue_row(s,p,AE_EXECUTION_APP,&process);
        uint8_t bytes[AE_EXECUTION_PROCESS_MAX]; size_t length=0;
        if (r==0) r=ae_execution_process_encode(&process.process,bytes,sizeof bytes,&length);
        if (r==0 && (length!=size || memcmp(bytes,proc,size)!=0)) r=-ESTALE;
        int64_t admitted=0;
        if (r==0) r=HQ("SELECT count(*) FROM h JOIN launch_dispatches d ON d.launch_id=h.launch JOIN generation_leases l ON l.lease_id=d.lease_id "
            "WHERE d.state IN ('dispatching','observed') AND l.state='running' AND NOT EXISTS (SELECT 1 FROM run_closures c WHERE c.launch_id=h.launch)",&admitted);
        if (r==0 && !admitted) r=-EAGAIN;
        if (r==0) { fresh=1; result.transfer_required=1; }
    } else if (r==0) {
        uint8_t bytes[AE_HANDOFF_MAX]; size_t length=0; r=ae_handoff_encode(&result.snapshot.original,bytes,sizeof bytes,&length);
        if (r==0 && (length!=v.size || memcmp(bytes,v.raw,v.size)!=0)) r=-EEXIST;
        if (r==0 && stage==AE_HANDOFF_OBSERVE) {
            if (memcmp(info.runtime_incarnation,p->system_runtime,16)==0) r=-ESTALE;
            else if (result.snapshot.state>AE_HANDOFF_OBSERVED) r=-EALREADY;
            else if (memcmp(info.runtime_incarnation,result.snapshot.observer_runtime,16)!=0) fresh=1;
        } else if (r==0 && stage==AE_HANDOFF_RETIRE && result.snapshot.state<AE_HANDOFF_RETIRING) { fresh=1; result.retire_required=1; }
        else if (r==0 && stage==AE_HANDOFF_RETIRED) {
            if (result.snapshot.state<AE_HANDOFF_RETIRING) r=-EAGAIN;
            else if (result.snapshot.state==AE_HANDOFF_RETIRING) fresh=1;
        }
    }
    if (r==0 && fresh && (info.revision==INT64_MAX || (stage!=AE_HANDOFF_PREPARE && result.snapshot.revision==INT64_MAX))) r=-EOVERFLOW;
    if (r==0 && fresh && stage>=AE_HANDOFF_RETIRE) {
        int64_t settled=0;
        r=HQ("SELECT count(*) FROM h JOIN launch_dispatches d ON d.launch_id=h.launch JOIN generation_leases l ON l.lease_id=d.lease_id "
            "JOIN run_closures c ON c.launch_id=h.launch WHERE d.state='terminal' AND l.state='released' AND c.state='released' AND c.profile=h.profile",&settled);
        if (r==0 && !settled) r=-EAGAIN;
    }
    if (r==0 && fresh && stage==AE_HANDOFF_PREPARE)
        r=HQ("INSERT INTO execution_handoffs SELECT launch,2,profile,pdigest,process,witness,digest,'pending',original,boot,stamp,NULL,NULL,NULL,NULL,NULL,NULL,1 FROM h",NULL);
    else if (r==0 && fresh && stage==AE_HANDOFF_OBSERVE)
        r=HQ("UPDATE execution_handoffs SET state='observed',observer_runtime=(SELECT current FROM h),observed_unix_ms=(SELECT stamp FROM h),revision=revision+1 WHERE launch_id=(SELECT launch FROM h)",NULL);
    else if (r==0 && fresh && stage==AE_HANDOFF_RETIRE)
        r=HQ("UPDATE execution_handoffs SET state='retiring',retirement_runtime=(SELECT current FROM h),retiring_unix_ms=(SELECT stamp FROM h),revision=revision+1 WHERE launch_id=(SELECT launch FROM h)",NULL);
    else if (r==0 && fresh && stage==AE_HANDOFF_RETIRED)
        r=HQ("UPDATE execution_handoffs SET state='retired',retired_runtime=(SELECT current FROM h),retired_unix_ms=(SELECT stamp FROM h),revision=revision+1 WHERE launch_id=(SELECT launch FROM h)",NULL);
    if (r==0 && fresh) r=HQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    if (r==0 && fresh) r=handoff_row(s,p,&result.snapshot);
    r=fixture_commit(s,r); if (r==0) *output=result; return r;
}
#undef HQ
#undef HANDOFF_INPUT
#undef EXEC_INPUT
#undef EXEC_TUPLE

/* Separate v3 lifetime records: never extend/change a completed start operation. */
#define FB(value, length) ((struct fixture_value){.bytes=(value), .size=(length)})
#define FI(value) ((struct fixture_value){.size=-1, .number=(value)})
#define RUN_INPUT "WITH p(profile,hash,op,uop,launch,lease,i,g,d,se,sr,be,br,account,boot,inv,ctx,start,beg,quiet,ended,rel,stamp,srev,stime,obs,fp,artifact,ir,igate,dgate) AS (VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24,?25,?26,?27,?28,?29,?30,?31)) "
struct run_values {
    struct fixture_value v[31];
    uint8_t profile[AE_EXECUTION_BYTES],hash[32],start[32],beg[AE_RUN_RECEIPT_BYTES],quiet[AE_RUN_RECEIPT_BYTES],rel[AE_RUN_RECEIPT_BYTES],end[32],obs[32];
};
static int run_inputs(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_execution_evidence *e,int64_t stamp,struct run_values *v)
{
    if (!e || !nonzero(e->invocation) || stamp<0 || !v) return -EINVAL;
    memset(v,0,sizeof *v);
    int r=ae_execution_encode(p,v->profile);
    if (r==0) r=ae_sha256_bytes(v->profile,sizeof v->profile,v->hash);
    if (r==0) r=still_named(s);
    if (r!=0) return r;
    const struct fixture_value values[]={FB(v->profile,384),FB(v->hash,32),FB(p->root_operation,16),FB(p->user_operation,16),FB(p->launch,16),FB(p->lease,16),
        FB(p->install,16),FB(p->generation,16),FB(p->data,16),FB(p->system_epoch,16),FB(p->system_runtime,16),FB(p->broker_epoch,16),FB(p->broker_runtime,16),
        FB(p->account,16),FB(p->boot,16),FB(e->invocation,16),FB(e->context_digest,32),FB(v->start,32),FB(v->beg,136),FB(v->quiet,136),FB(v->end,32),FB(v->rel,136),
        FI(stamp),FI(0),FI(0),FB(v->obs,32),FB(p->request_digest,32),FB(p->artifact,32),FI(p->install_revision),FI(p->install_gate_revision),FI(p->data_gate_revision)};
    memcpy(v->v,values,sizeof values); return 0;
}
static int run_statement(struct ae_catalogue *s,const struct run_values *v,const char *sql,int64_t *number,void *blob,int bytes)
{
    sqlite3_stmt *q=NULL; int r=failure(sqlite3_prepare_v2(s->db,sql,-1,&q,NULL));
    for (int n=0;r==0 && n<31;++n) {
        const struct fixture_value *a=&v->v[n];
        r=failure(a->size<0 ? sqlite3_bind_int64(q,n+1,a->number) : sqlite3_bind_blob(q,n+1,a->bytes,a->size,SQLITE_TRANSIENT));
    }
    if (r==0) {
        int code=sqlite3_step(q);
        if (number || blob) {
            if (code!=SQLITE_ROW) r=code==SQLITE_DONE ? -ENOENT : failure(code);
            else {
                if (number) { if (sqlite3_column_type(q,0)!=SQLITE_INTEGER) r=-EUCLEAN; else *number=sqlite3_column_int64(q,0); }
                else r=projection_blob(q,0,blob,bytes);
                if (r==0 && sqlite3_step(q)!=SQLITE_DONE) r=-EUCLEAN;
            }
        } else { r=code==SQLITE_DONE ? 0 : failure(code); if (r==0 && sqlite3_changes(s->db)!=1) r=-EUCLEAN; }
    }
    int final=failure(sqlite3_finalize(q)); return r==0 ? final : r;
}
#define RQ(sql,out) run_statement(s,&v,RUN_INPUT sql,out,NULL,0)
#define RB(sql,out,bytes) run_statement(s,&v,RUN_INPUT sql,NULL,out,bytes)
#define RUN_ROOT "FROM p JOIN authority_meta m ON m.singleton=1 AND m.kind='system' AND m.authority_epoch=p.se AND m.runtime_incarnation=p.sr AND m.current_boot_id=p.boot " \
    "JOIN launch_dispatches l ON l.launch_id=p.launch AND l.lease_id=p.lease AND l.root_operation_id=p.op AND l.broker_epoch=p.be AND l.broker_incarnation=p.br AND l.account_scope_ref=p.account AND l.install_ref=p.i AND l.generation_ref=p.g AND l.data_epoch_ref=p.d AND l.boot_id=p.boot AND l.invocation_ref=p.inv AND l.install_gate_revision=p.igate AND l.ready_data_gate_revision=p.dgate AND l.action_id='run' AND l.session_ref='p0' AND l.seat_ref='none' " \
    "JOIN operations o ON o.operation_id=p.op AND o.state='completed' AND o.outcome='applied' AND o.result_kind='launch' AND o.result_ref=p.launch " \
    "JOIN plans plan ON plan.plan_id=o.plan_id AND plan.kind='launch' AND plan.authority_epoch=p.se AND plan.scope_ref=p.account AND plan.action_id='run' " \
    "JOIN generation_leases lease ON lease.lease_id=p.lease JOIN generations g ON g.generation_id=p.g AND g.artifact_id=p.artifact " \
    "JOIN operation_steps intent ON intent.operation_id=p.op AND intent.step_no=0 AND intent.action='verify' AND intent.subject_kind='launch' AND intent.subject_ref=p.launch AND intent.state='applied' AND intent.evidence_digest=p.fp " \
    "JOIN operation_steps prep ON prep.operation_id=p.op AND prep.step_no=1 AND prep.action='stage' AND prep.subject_kind='generation' AND prep.subject_ref=p.g AND prep.state='applied' AND prep.evidence_digest=p.hash " \
    "JOIN operation_steps pin ON pin.operation_id=p.op AND pin.step_no=2 AND pin.action='verify' AND pin.subject_kind='integration' AND pin.subject_ref=p.inv AND pin.state='applied' AND pin.evidence_digest=p.ctx " \
    "JOIN operation_steps arm ON arm.operation_id=p.op AND arm.step_no=3 AND arm.action='activate' AND arm.subject_kind='launch' AND arm.subject_ref=p.launch AND arm.state='applied' AND arm.evidence_digest=p.hash " \
    "JOIN operation_steps observed ON observed.operation_id=p.op AND observed.step_no=5 AND observed.action='verify' AND observed.subject_kind='launch' AND observed.subject_ref=p.launch AND observed.state='applied' " \
    "JOIN operation_steps done ON done.operation_id=p.op AND done.step_no=6 AND done.action='reconcile' AND done.subject_kind='launch' AND done.subject_ref=p.launch AND done.state='applied' "
#define RUN_USER "FROM p JOIN authority_meta m ON m.singleton=1 AND m.kind='user' AND m.authority_epoch=p.be AND m.owner_scope_ref=p.account AND m.runtime_incarnation=p.br AND m.current_boot_id=p.boot " \
    "JOIN launches l ON l.launch_id=p.launch AND l.operation_id=p.uop AND l.lease_ref=p.lease AND l.system_epoch=p.se AND l.boot_id=p.boot AND l.install_id=p.i AND l.generation_ref=p.g AND l.data_epoch_id=p.d AND l.invocation_ref=p.inv AND l.session_ref='p0' AND l.seat_ref='none' " \
    "JOIN operations o ON o.operation_id=p.uop AND o.state='completed' AND o.outcome='applied' AND o.result_kind='launch' AND o.result_ref=p.launch " \
    "JOIN plans plan ON plan.plan_id=o.plan_id AND plan.kind='launch' AND plan.authority_epoch=p.be AND plan.scope_ref=p.account AND plan.action_id='run' " \
    "JOIN operation_links link ON link.operation_id=p.uop AND link.link_no=0 AND link.state='completed' AND link.remote_epoch=p.se AND link.remote_operation_ref=p.op " \
    "JOIN operation_steps intent ON intent.operation_id=p.uop AND intent.step_no=0 AND intent.action='verify' AND intent.subject_kind='launch' AND intent.subject_ref=p.launch AND intent.state='applied' AND intent.evidence_digest=p.fp " \
    "JOIN operation_steps prep ON prep.operation_id=p.uop AND prep.step_no=1 AND prep.action='stage' AND prep.subject_kind='data' AND prep.subject_ref=p.d AND prep.state='applied' AND prep.evidence_digest=p.hash " \
    "JOIN operation_steps pin ON pin.operation_id=p.uop AND pin.step_no=2 AND pin.action='verify' AND pin.subject_kind='integration' AND pin.subject_ref=p.inv AND pin.state='applied' AND pin.evidence_digest=p.ctx " \
    "JOIN operation_steps ready ON ready.operation_id=p.uop AND ready.step_no=3 AND ready.action='activate' AND ready.subject_kind='launch' AND ready.subject_ref=p.launch AND ready.state='applied' AND ready.evidence_digest=p.ctx " \
    "JOIN operation_steps done ON done.operation_id=p.uop AND done.step_no=4 AND done.action='reconcile' AND done.subject_kind='launch' AND done.subject_ref=p.launch AND done.state='applied' "
#define RUN_ROW "JOIN run_closures c ON c.launch_id=p.launch AND c.profile=p.profile AND c.profile_digest=p.hash AND c.invocation_ref=p.inv AND c.context_digest=p.ctx "
static int run_begin(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_execution_evidence *e,int64_t stamp,struct run_values *v,int write)
{
    int r=run_inputs(s,p,e,stamp,v);
    if (r==0) r=exec_sql(s->db,write ? "BEGIN IMMEDIATE" : "BEGIN");
    if (r!=0) return r;
    sqlite3_int64 version=0; r=scalar_int(s->db,"PRAGMA user_version",&version);
    if (r==0 && version!=3 && version!=4 && !((version==5 || version==6) && s->identity.kind==AE_CATALOGUE_SYSTEM)) r=-EPROTONOSUPPORT;
    if (r!=0) return fixture_commit(s,r);
    return 0;
}
static int run_existing(struct ae_catalogue *s,const struct run_values *v,int *exists)
{
    int64_t count=0; int r=run_statement(s,v,RUN_INPUT "SELECT count(*) FROM run_closures,p WHERE launch_id=p.launch",&count,NULL,0);
    if (r==0 && count) {
        const char *base=s->identity.kind==AE_CATALOGUE_SYSTEM ? RUN_INPUT "SELECT count(*) " RUN_ROOT RUN_ROW "WHERE c.start_operation_id=p.op AND c.start_result_digest=done.evidence_digest" :
            RUN_INPUT "SELECT count(*) " RUN_USER RUN_ROW "WHERE c.start_operation_id=p.uop AND c.start_result_digest=done.evidence_digest";
        r=run_statement(s,v,base,&count,NULL,0);
        if (r==0 && count!=1) r=-EEXIST;
        if (r==0) *exists=1;
    } else if (r==0) *exists=0;
    return r;
}
static int run_receipt_row(struct ae_catalogue *s,const struct run_values *v,enum ae_run_receipt_kind kind,struct ae_run_receipt *output)
{
    const char *sql=kind==AE_RUN_ROOT_CLOSING ? RUN_INPUT "SELECT begin_receipt FROM run_closures,p WHERE launch_id=p.launch" :
        kind==AE_RUN_USER_QUIET ? RUN_INPUT "SELECT quiet_receipt FROM run_closures,p WHERE launch_id=p.launch AND quiet_receipt IS NOT NULL" :
        kind==AE_RUN_ROOT_RELEASED ? RUN_INPUT "SELECT release_receipt FROM run_closures,p WHERE launch_id=p.launch AND state='released'" : NULL;
    if (!sql) return -EINVAL;
    uint8_t raw[AE_RUN_RECEIPT_BYTES]; int r=run_statement(s,v,sql,NULL,raw,sizeof raw);
    if (r==0) r=ae_run_receipt_decode(raw,sizeof raw,output);
    return r;
}
static struct ae_run_receipt run_receipt(const struct run_values *v,const struct ae_execution_evidence *e,enum ae_run_receipt_kind kind,const uint8_t previous[32],const uint8_t evidence[32],int64_t revision,int64_t stamp)
{
    struct ae_run_receipt a={.kind=kind,.revision=revision,.unix_ms=stamp};
    memcpy(a.profile_digest,v->hash,32); memcpy(a.invocation,e->invocation,16); memcpy(a.previous_digest,previous,32); memcpy(a.evidence_digest,evidence,32); return a;
}
static int run_root_context(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *c)
{
    int r=reservation_context(s,c);
    if (r==0 && (!p || memcmp(p->system_epoch,s->identity.authority_epoch,16)!=0 || memcmp(p->system_runtime,c->system_runtime,16)!=0 || memcmp(p->boot,c->boot,16)!=0 ||
        memcmp(p->broker_epoch,c->broker_epoch,16)!=0 || memcmp(p->broker_runtime,c->broker_runtime,16)!=0 || memcmp(p->account,c->account,16)!=0)) r=-ESTALE;
    return r;
}
static int run_recovery_context(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *c)
{
    int r=reservation_context(s,c);
    if (r==0 && (!p || c->uid!=1000 || memcmp(p->system_epoch,s->identity.authority_epoch,16)!=0 ||
        memcmp(p->system_runtime,c->system_runtime,16)==0 || memcmp(p->boot,c->boot,16)!=0 ||
        memcmp(p->broker_epoch,c->broker_epoch,16)!=0 || memcmp(p->broker_runtime,c->broker_runtime,16)!=0 ||
        memcmp(p->account,c->account,16)!=0)) r=-ESTALE;
    return r;
}
/* Called inside the owner transaction. Only the SQL current-runtime input is
 * replaced: profile bytes/hash and all original launch/process fields stay fixed. */
static int run_recovery_current(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,struct run_values *v)
{
    sqlite3_int64 version=0; int r=scalar_int(s->db,"PRAGMA user_version",&version);
    if (r==0 && version!=6) r=-EPROTONOSUPPORT;
    if (r==0) r=run_recovery_context(s,p,c);
    struct ae_handoff_snapshot h;
    if (r==0) r=handoff_row(s,p,&h);
    if (r==0 && (h.state<AE_HANDOFF_OBSERVED || memcmp(h.observer_runtime,c->system_runtime,16)!=0)) r=-ESTALE;
    if (r==0) r=ae_execution_process_match(p,e,&h.original.process);
    if (r==0) v->v[10]=FB(c->system_runtime,16);
    return r;
}
static int run_recovery_owner(struct ae_catalogue *s,struct run_values *v,int create)
{
    int64_t count=0; int r=run_statement(s,v,RUN_INPUT "SELECT count(*) FROM run_recovery_owners r,p WHERE r.launch_id=p.launch AND r.runtime=p.sr",&count,NULL,0);
    if (r==0 && count) {
        r=run_statement(s,v,RUN_INPUT "SELECT count(*) FROM p JOIN run_recovery_owners r ON r.launch_id=p.launch AND r.runtime=p.sr "
            "JOIN execution_handoffs h ON h.launch_id=r.launch_id WHERE r.profile_digest=p.hash AND r.profile_digest=h.profile_digest "
            "AND r.witness_digest=h.witness_digest AND r.handoff_revision<=h.revision AND h.observer_runtime=p.sr",&count,NULL,0);
        return r==0 && count!=1 ? -EUCLEAN : r;
    }
    if (r!=0 || !create) return r!=0 ? r : -ENOENT;
    r=run_statement(s,v,RUN_INPUT "SELECT catalogue_revision FROM authority_meta WHERE singleton=1",&count,NULL,0);
    if (r==0 && count==INT64_MAX) r=-EOVERFLOW;
    if (r==0) r=run_statement(s,v,RUN_INPUT "INSERT INTO run_recovery_owners SELECT p.launch,p.sr,p.hash,h.witness_digest,h.revision,p.stamp "
        "FROM p JOIN execution_handoffs h ON h.launch_id=p.launch",NULL,NULL,0);
    if (r==0) r=run_statement(s,v,RUN_INPUT "UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL,NULL,0);
    return r;
}
int ae_catalogue_get_run_receipt(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_execution_evidence *e,enum ae_run_receipt_kind kind,struct ae_run_receipt *output)
{
    if (!output) return -EINVAL;
    struct run_values v; int r=run_begin(s,p,e,0,&v,0); if (r!=0) return r;
    int exists=0; r=run_existing(s,&v,&exists); if (r==0 && !exists) r=-ENOENT;
    struct ae_run_receipt result;
    if (r==0) r=run_receipt_row(s,&v,kind,&result);
    if (r==0) r=ae_run_receipt_match(p,e,kind,&result);
    r=fixture_commit(s,r); if (r==0) *output=result;
    return r;
}
static int begin_run_close(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *context,
    const struct ae_execution_evidence *e,const struct ae_execution_completion *started,int64_t stamp,struct ae_run_receipt *output,int recovery)
{
    if (!output || !started) return -EINVAL;
    int r=ae_execution_completion_match(p,e,started->observation_digest,started);
    if (r==0) r=recovery ? run_recovery_context(s,p,context) : run_root_context(s,p,context);
    if (r!=0) return r;
    struct run_values v; r=run_begin(s,p,e,stamp,&v,1); if (r!=0) return r;
    if (recovery) r=run_recovery_current(s,p,context,e,&v);
    uint8_t raw[AE_EXECUTION_COMPLETION_BYTES]; if (r==0) r=ae_execution_completion_encode(started,raw);
    if (r==0) r=ae_sha256_bytes(raw,sizeof raw,v.start);
    memcpy(v.obs,started->observation_digest,32); v.v[23]=FI(started->revision); v.v[24]=FI(started->completed_unix_ms);
    int exists=0; int64_t match=0;
    if (r==0) r=RQ("SELECT count(*) " RUN_ROOT "WHERE done.evidence_digest=p.start AND observed.evidence_digest=p.obs AND o.revision=p.srev AND o.completed_unix_ms=p.stime",&match);
    if (r==0 && match!=1) r=-ESTALE;
    if (r==0) r=run_existing(s,&v,&exists);
    if (r==0 && recovery) {
        int64_t released=0;
        r=RQ("SELECT count(*) FROM run_closures,p WHERE launch_id=p.launch AND state='released'",&released);
        /* Historical completion cannot enroll another recovery owner. */
        if (r==0) r=run_recovery_owner(s,&v,!released);
    }
    struct ae_run_receipt result=run_receipt(&v,e,AE_RUN_ROOT_CLOSING,v.start,e->context_digest,1,stamp);
    if (r==0 && exists) r=run_receipt_row(s,&v,AE_RUN_ROOT_CLOSING,&result);
    if (r==0 && !exists) {
        const char *fresh=recovery ? RUN_INPUT "SELECT count(*) " RUN_ROOT "WHERE l.state='uncertain' AND lease.state='running' AND l.close_cause_epoch IS NULL AND o.completed_unix_ms<=p.stamp AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no>6)" :
            RUN_INPUT "SELECT count(*) " RUN_ROOT "WHERE l.state='observed' AND lease.state='running' AND l.close_cause_epoch IS NULL AND o.completed_unix_ms<=p.stamp AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.op AND step_no>6)";
        r=run_statement(s,&v,fresh,&match,NULL,0);
        if (r==0 && match!=1) r=-EAGAIN;
        if (r==0) r=RQ("SELECT max(m.catalogue_revision,l.revision) " RUN_ROOT,&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0) r=ae_run_receipt_encode(&result,v.beg);
        if (r==0) r=RQ("INSERT INTO run_closures(launch_id,start_operation_id,profile,profile_digest,invocation_ref,context_digest,start_result_digest,state,begin_receipt,began_unix_ms) SELECT launch,op,profile,hash,inv,ctx,start,'closing',beg,stamp FROM p",NULL);
        if (r==0) r=RQ("UPDATE launch_dispatches SET state='closing',revision=revision+1 WHERE launch_id=(SELECT launch FROM p)",NULL);
        if (r==0) r=RQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    r=fixture_commit(s,r); if (r==0) *output=result;
    return r;
}
int ae_catalogue_begin_run_close(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,const struct ae_execution_completion *started,int64_t stamp,struct ae_run_receipt *out)
{ return begin_run_close(s,p,c,e,started,stamp,out,0); }
int ae_catalogue_recover_begin_run_close(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,const struct ae_execution_completion *started,int64_t stamp,struct ae_run_receipt *out)
{ return begin_run_close(s,p,c,e,started,stamp,out,1); }
int ae_catalogue_quiesce_user_run(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_execution_evidence *e,
    const struct ae_run_receipt *begin,int64_t stamp,struct ae_run_receipt *output)
{
    if (!output) return -EINVAL;
    int r=ae_run_receipt_match(p,e,AE_RUN_ROOT_CLOSING,begin); if (r!=0) return r;
    struct run_values v; r=run_begin(s,p,e,stamp,&v,1); if (r!=0) return r;
    if (s->identity.kind!=AE_CATALOGUE_USER) r=-EPERM;
    memcpy(v.start,begin->previous_digest,32); uint8_t hash[32]={0};
    if (r==0) r=ae_run_receipt_encode(begin,v.beg);
    if (r==0) r=ae_sha256_bytes(v.beg,sizeof v.beg,hash);
    int64_t match=0; int exists=0;
    if (r==0) r=RQ("SELECT count(*) " RUN_USER "WHERE done.evidence_digest=p.start",&match);
    if (r==0 && match!=1) r=-ESTALE;
    if (r==0) r=run_existing(s,&v,&exists);
    struct ae_run_receipt result=run_receipt(&v,e,AE_RUN_USER_QUIET,hash,e->context_digest,1,stamp);
    if (r==0 && exists) {
        r=RQ("SELECT count(*) FROM run_closures,p WHERE launch_id=p.launch AND begin_receipt=p.beg AND state IN ('closing','released')",&match);
        if (r==0 && match!=1) r=-EEXIST;
        if (r==0) r=run_receipt_row(s,&v,AE_RUN_USER_QUIET,&result);
    }
    if (r==0 && !exists) {
        r=RQ("SELECT count(*) " RUN_USER "WHERE l.state='running' AND l.readiness='ready' AND o.completed_unix_ms<=p.stamp AND NOT EXISTS(SELECT 1 FROM grants WHERE launch_id=p.launch) AND NOT EXISTS(SELECT 1 FROM access_leases WHERE launch_id=p.launch) AND NOT EXISTS(SELECT 1 FROM operation_steps WHERE operation_id=p.uop AND step_no>4)",&match);
        if (r==0 && match!=1) r=-EAGAIN;
        if (r==0) r=RQ("SELECT max(m.catalogue_revision,l.revision) " RUN_USER,&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0) r=ae_run_receipt_encode(&result,v.quiet);
        if (r==0) r=RQ("INSERT INTO run_closures(launch_id,start_operation_id,profile,profile_digest,invocation_ref,context_digest,start_result_digest,state,begin_receipt,quiet_receipt,began_unix_ms) SELECT launch,uop,profile,hash,inv,ctx,start,'closing',beg,quiet,stamp FROM p",NULL);
        if (r==0) r=RQ("UPDATE launches SET state='stopping',readiness='unavailable',revision=revision+1 WHERE launch_id=(SELECT launch FROM p)",NULL);
        if (r==0) r=RQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    r=fixture_commit(s,r); if (r==0) *output=result;
    return r;
}
static int release_run(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *context,
    const struct ae_execution_evidence *e,const struct ae_run_receipt *quiet,const uint8_t end_digest[32],int64_t stamp,struct ae_run_receipt *output,int recovery)
{
    if (!output || !end_digest) return -EINVAL;
    int r=ae_run_receipt_match(p,e,AE_RUN_USER_QUIET,quiet);
    if (r==0) r=recovery ? run_recovery_context(s,p,context) : run_root_context(s,p,context);
    if (r!=0) return r;
    struct run_values v; r=run_begin(s,p,e,stamp,&v,1); if (r!=0) return r;
    if (recovery) r=run_recovery_current(s,p,context,e,&v);
    if (r==0 && recovery) r=run_recovery_owner(s,&v,0);
    int exists=0; if (r==0) r=run_existing(s,&v,&exists); if (r==0 && !exists) r=-ENOENT;
    struct ae_run_receipt begin,result; uint8_t hash[32]; int64_t match=0,revision=0,released=0;
    if (r==0) r=run_receipt_row(s,&v,AE_RUN_ROOT_CLOSING,&begin);
    if (r==0) r=ae_run_receipt_digest(&begin,hash);
    if (r==0 && memcmp(hash,quiet->previous_digest,32)!=0) r=-ESTALE;
    if (r==0) r=ae_run_receipt_encode(quiet,v.quiet);
    memcpy(v.end,end_digest,32);
    if (r==0) r=RQ("SELECT count(*) FROM run_closures,p WHERE launch_id=p.launch AND state='released'",&released);
    if (r==0 && released) {
        r=RQ("SELECT count(*) " RUN_ROOT RUN_ROW "WHERE c.state='released' AND c.quiet_receipt=p.quiet AND c.end_digest=p.ended AND l.state='terminal' AND lease.state='released'",&match);
        if (r==0 && match!=1) r=-EEXIST;
        if (r==0) r=run_receipt_row(s,&v,AE_RUN_ROOT_RELEASED,&result);
    }
    if (r==0 && !released) {
        const char *pending=recovery ? RUN_INPUT "SELECT count(*) " RUN_ROOT RUN_ROW "WHERE c.state IN ('closing','uncertain') AND c.quiet_receipt IS NULL AND c.began_unix_ms<=p.stamp AND l.state IN ('closing','uncertain') AND lease.state='running'" :
            RUN_INPUT "SELECT count(*) " RUN_ROOT RUN_ROW "WHERE c.state='closing' AND c.quiet_receipt IS NULL AND c.began_unix_ms<=p.stamp AND l.state='closing' AND lease.state='running'";
        r=run_statement(s,&v,pending,&match,NULL,0);
        if (r==0 && match!=1) r=-EAGAIN;
        if (r==0) r=RQ("SELECT max(m.catalogue_revision,l.revision,lease.revision,c.revision) " RUN_ROOT RUN_ROW,&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0) r=RQ("SELECT c.revision " RUN_ROOT RUN_ROW,&revision);
        if (r==0) r=ae_run_receipt_digest(quiet,hash);
        result=run_receipt(&v,e,AE_RUN_ROOT_RELEASED,hash,end_digest,revision<INT64_MAX ? revision+1 : 0,stamp);
        if (r==0) r=ae_run_receipt_encode(&result,v.rel);
        if (r==0) r=RQ("UPDATE run_closures SET state='released',quiet_receipt=(SELECT quiet FROM p),end_digest=(SELECT ended FROM p),release_receipt=(SELECT rel FROM p),ended_unix_ms=(SELECT stamp FROM p),revision=revision+1 WHERE launch_id=(SELECT launch FROM p)",NULL);
        if (r==0) r=RQ("UPDATE launch_dispatches SET state='terminal',revision=revision+1 WHERE launch_id=(SELECT launch FROM p)",NULL);
        if (r==0) r=RQ("UPDATE generation_leases SET state='released',revision=revision+1 WHERE lease_id=(SELECT lease FROM p)",NULL);
        if (r==0) r=RQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    r=fixture_commit(s,r); if (r==0) *output=result;
    return r;
}
int ae_catalogue_release_run(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,const struct ae_run_receipt *quiet,const uint8_t hash[32],int64_t stamp,struct ae_run_receipt *out)
{ return release_run(s,p,c,e,quiet,hash,stamp,out,0); }
int ae_catalogue_recover_release_run(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,const struct ae_run_receipt *quiet,const uint8_t hash[32],int64_t stamp,struct ae_run_receipt *out)
{ return release_run(s,p,c,e,quiet,hash,stamp,out,1); }
int ae_catalogue_get_recovered_run_receipt(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_launch_context *c,
    const struct ae_execution_evidence *e,enum ae_run_receipt_kind kind,struct ae_run_receipt *out)
{
    if (!out) return -EINVAL;
    int r=run_recovery_context(s,p,c); if (r!=0) return r;
    struct run_values v; r=run_begin(s,p,e,0,&v,0); if (r!=0) return r;
    r=run_recovery_current(s,p,c,e,&v);
    if (r==0) r=run_recovery_owner(s,&v,0);
    int exists=0; if (r==0) r=run_existing(s,&v,&exists); if (r==0 && !exists) r=-ENOENT;
    struct ae_run_receipt result;
    if (r==0) r=run_receipt_row(s,&v,kind,&result);
    if (r==0) r=ae_run_receipt_match(p,e,kind,&result);
    r=fixture_commit(s,r); if (r==0) *out=result; return r;
}
int ae_catalogue_end_user_run(struct ae_catalogue *s,const struct ae_execution_profile *p,const struct ae_execution_evidence *e,
    const struct ae_run_receipt *released,int64_t stamp,struct ae_user_launch_snapshot *output)
{
    if (!output) return -EINVAL;
    int r=ae_run_receipt_match(p,e,AE_RUN_ROOT_RELEASED,released); if (r!=0) return r;
    struct run_values v; r=run_begin(s,p,e,stamp,&v,1); if (r!=0) return r;
    if (s->identity.kind!=AE_CATALOGUE_USER) r=-EPERM;
    int exists=0; if (r==0) r=run_existing(s,&v,&exists); if (r==0 && !exists) r=-ENOENT;
    struct ae_run_receipt quiet; uint8_t hash[32]; int64_t match=0,done=0;
    if (r==0) r=run_receipt_row(s,&v,AE_RUN_USER_QUIET,&quiet);
    if (r==0) r=ae_run_receipt_digest(&quiet,hash);
    if (r==0 && memcmp(released->previous_digest,hash,32)!=0) r=-ESTALE;
    if (r==0) r=ae_run_receipt_encode(released,v.rel);
    memcpy(v.end,released->evidence_digest,32);
    if (r==0) r=RQ("SELECT count(*) FROM run_closures,p WHERE launch_id=p.launch AND state='released'",&done);
    if (r==0 && done) {
        r=RQ("SELECT count(*) " RUN_USER RUN_ROW "WHERE c.release_receipt=p.rel AND l.state='exited'",&match);
        if (r==0 && match!=1) r=-EEXIST;
    }
    if (r==0 && !done) {
        r=RQ("SELECT count(*) " RUN_USER RUN_ROW "WHERE c.state='closing' AND c.began_unix_ms<=p.stamp AND l.state='stopping' AND l.readiness='unavailable' AND NOT EXISTS(SELECT 1 FROM grants WHERE launch_id=p.launch) AND NOT EXISTS(SELECT 1 FROM access_leases WHERE launch_id=p.launch)",&match);
        if (r==0 && match!=1) r=-EAGAIN;
        if (r==0) r=RQ("SELECT max(m.catalogue_revision,l.revision,c.revision) " RUN_USER RUN_ROW,&match);
        if (r==0 && match==INT64_MAX) r=-EOVERFLOW;
        if (r==0) r=RQ("UPDATE run_closures SET state='released',end_digest=(SELECT ended FROM p),release_receipt=(SELECT rel FROM p),ended_unix_ms=(SELECT stamp FROM p),revision=revision+1 WHERE launch_id=(SELECT launch FROM p)",NULL);
        if (r==0) r=RQ("UPDATE launches SET state='exited',readiness='unavailable',revision=revision+1 WHERE launch_id=(SELECT launch FROM p)",NULL);
        if (r==0) r=RQ("UPDATE authority_meta SET catalogue_revision=catalogue_revision+1 WHERE singleton=1",NULL);
    }
    struct ae_user_launch_snapshot result;
    if (r==0) r=user_launch_row(s,p->user_operation,&result);
    r=fixture_commit(s,r); if (r==0) *output=result;
    return r;
}
#undef RUN_INPUT
#undef RUN_ROOT
#undef RUN_USER
#undef RUN_ROW
#undef RQ
#undef RB
#undef FB
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
