#include "catalogue/store.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static int commit_fault;
static int directory_sync_fault;
static int constructor_full;
int __real_fsync(int);
int __wrap_fsync(int descriptor)
{
    if (directory_sync_fault) { errno = EIO; return -1; }
    return __real_fsync(descriptor);
}
int __real_sqlite3_exec(sqlite3 *, const char *, int (*)(void *, int, char **, char **), void *, char **);
int __wrap_sqlite3_exec(sqlite3 *db, const char *sql, int (*callback)(void *, int, char **, char **), void *context, char **error)
{
    if (constructor_full && strcmp(sql, "BEGIN EXCLUSIVE") == 0) {
        constructor_full = 0;
        int result = __real_sqlite3_exec(db, "PRAGMA max_page_count=1", NULL, NULL, NULL);
        CHECK(result == SQLITE_OK);
    }
    if (strcmp(sql, "COMMIT") != 0 || commit_fault == 0) return __real_sqlite3_exec(db, sql, callback, context, error);
    if (commit_fault == 1) return SQLITE_IOERR;
    if (commit_fault == 3) _exit(76);
    int result = __real_sqlite3_exec(db, sql, callback, context, error);
    CHECK(result == SQLITE_OK);
    if (commit_fault == 4) _exit(77);
    return SQLITE_IOERR; /* Commit happened, acknowledgement was lost. */
}

static void id(uint8_t output[AE_ID_BYTES], unsigned char number)
{
    memset(output, 0, AE_ID_BYTES);
    output[6] = 0x40; output[8] = 0x80; output[15] = number;
}

static struct ae_catalogue_identity identity(enum ae_catalogue_kind kind)
{
    struct ae_catalogue_identity value = {.kind = kind};
    id(value.authority_epoch, 1);
    if (kind == AE_CATALOGUE_USER) id(value.owner, 2);
    return value;
}

struct fixture { char directory[PATH_MAX]; char db[PATH_MAX]; int root; };

static struct fixture make_fixture(const char *build)
{
    char parent[PATH_MAX];
    CHECK(realpath(build, parent) != NULL);
    struct fixture fixture;
    int count = snprintf(fixture.directory, sizeof fixture.directory, "%s/catalogue-XXXXXX", parent);
    CHECK(count > 0 && (size_t)count < sizeof fixture.directory);
    CHECK(mkdtemp(fixture.directory) != NULL);
    fixture.root = open(fixture.directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(fixture.root >= 0);
    count = snprintf(fixture.db, sizeof fixture.db, "%s/catalogue.sqlite3", fixture.directory);
    CHECK(count > 0 && (size_t)count < sizeof fixture.db);
    return fixture;
}

static void cleanup(struct fixture *fixture)
{
    const char *files[] = {"catalogue.sqlite3", "writer.lock", "catalogue.sqlite3-journal", "alias", "moved", "selflink", "catalogue.sqlite3-wal"};
    for (size_t n = 0; n < sizeof files / sizeof files[0]; ++n)
        CHECK(unlinkat(fixture->root, files[n], 0) == 0 || errno == ENOENT);
    CHECK(close(fixture->root) == 0);
    CHECK(rmdir(fixture->directory) == 0);
}

/* Trusted fixture mutation only while the native connection is CLOSED. */
static void edit(const struct fixture *fixture, const char *sql)
{
    sqlite3 *db = NULL;
    CHECK(sqlite3_open_v2(fixture->db, &db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL) == SQLITE_OK);
    char *error = NULL;
    int result = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (result != SQLITE_OK) fprintf(stderr, "fixture edit: %s\n", error);
    sqlite3_free(error);
    CHECK(result == SQLITE_OK);
    CHECK(sqlite3_close(db) == SQLITE_OK);
}

static int integer(const struct fixture *fixture, const char *sql)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *statement = NULL;
    CHECK(sqlite3_open_v2(fixture->db, &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    int result = sqlite3_column_int(statement, 0);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    CHECK(sqlite3_close(db) == SQLITE_OK);
    return result;
}

static void seed(const struct fixture *fixture, enum ae_catalogue_kind kind)
{
    if (kind == AE_CATALOGUE_SYSTEM) edit(fixture,
        "INSERT INTO scopes VALUES(x'00000000000040008000000000000010','machine',NULL,'active',1);"
        "INSERT INTO components VALUES('fixture',x'00000000000040008000000000000011','app','Fixture',0,1);"
        "INSERT INTO installs VALUES(x'00000000000040008000000000000012','fixture',x'00000000000040008000000000000010','preparing',NULL,1);");
    else edit(fixture,
        "INSERT INTO data_epochs VALUES(x'00000000000040008000000000000012','fixture',x'00000000000040008000000000000011',"
        "x'00000000000040008000000000000002',x'00000000000040008000000000000013','active',1,1);"
        "INSERT INTO resources VALUES(x'00000000000040008000000000000014',x'00000000000040008000000000000002','file',"
        "x'00000000000040008000000000000015','document','Document',x'00000000000040008000000000000003',"
        "x'00000000000040008000000000000004',1,1,4,'bound',1);");
}

static void basic(const char *build, enum ae_catalogue_kind kind)
{
    struct fixture fixture = make_fixture(build);
    struct ae_catalogue_identity expected = identity(kind);
    uint8_t boot[AE_ID_BYTES], runtime[AE_ID_BYTES]; id(boot, 3); id(runtime, 4);
    struct ae_catalogue *store = NULL;
    CHECK(ae_catalogue_open(fixture.directory, &expected, NULL) == -EINVAL);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == -ENOENT && store == NULL);
    CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == 0);
    struct ae_catalogue *unchanged = store;
    CHECK(ae_catalogue_open(fixture.directory, &expected, &unchanged) == -EWOULDBLOCK && unchanged == store);
    struct ae_catalogue_info info;
    CHECK(ae_catalogue_get_info(store, &info) == 0 && info.revision == 1);
    CHECK(info.runtime_incarnation[15] == 0 && memcmp(info.authority_epoch, expected.authority_epoch, AE_ID_BYTES) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        CHECK(ae_catalogue_get_info(store, &info) == -ECHILD);
        CHECK(ae_catalogue_close(&store) == -ECHILD);
        struct ae_catalogue *other = NULL;
        CHECK(ae_catalogue_open(fixture.directory, &expected, &other) == -EWOULDBLOCK && other == NULL);
        _exit(0);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(ae_catalogue_begin_runtime(store, boot, runtime) == 0);
    CHECK(ae_catalogue_begin_runtime(store, boot, runtime) == -EALREADY);
    CHECK(ae_catalogue_close(&store) == 0 && store == NULL);
    CHECK(ae_catalogue_close(&store) == 0);
    struct ae_catalogue_identity wrong = expected;
    wrong.authority_epoch[15] = 9;
    CHECK(ae_catalogue_open(fixture.directory, &wrong, &store) == -ESTALE && store == NULL);
    wrong = expected;
    if (kind == AE_CATALOGUE_USER) {
        wrong.owner[15] = 9;
        CHECK(ae_catalogue_open(fixture.directory, &wrong, &store) == -ESTALE && store == NULL);
    }
    seed(&fixture, kind);
    CHECK(integer(&fixture, "SELECT count(*) FROM admission_gates WHERE state='open'") == 1);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
    id(runtime, 5);
    CHECK(ae_catalogue_begin_runtime(store, boot, runtime) == 0);
    CHECK(ae_catalogue_get_info(store, &info) == 0 && info.revision == 3 && info.runtime_incarnation[15] == 5);
    CHECK(ae_catalogue_close(&store) == 0);
    CHECK(integer(&fixture, "SELECT count(*) FROM admission_gates WHERE state='closed' AND revision=2") == 1);
    if (kind == AE_CATALOGUE_USER) CHECK(integer(&fixture, "SELECT count(*) FROM resources WHERE state='needs_reselect' AND revision=2") == 1);
    CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == -EEXIST && store == NULL);
    cleanup(&fixture);
}

static void reject(const char *build, enum ae_catalogue_kind kind)
{
    const char *mutations[] = {
        "PRAGMA user_version=1", "PRAGMA user_version=99", "PRAGMA application_id=1",
        "DROP TRIGGER immutable_gate",
        "PRAGMA writable_schema=ON; UPDATE sqlite_schema SET sql=replace(sql,'immutable gate target','immutable  gate target') WHERE name='immutable_gate';",
        "UPDATE authority_meta SET authority_epoch=x'00000000000040008000000000000009'",
        "UPDATE authority_meta SET current_boot_id=zeroblob(16)",
        "DELETE FROM authority_meta",
        "PRAGMA foreign_keys=OFF; INSERT INTO plan_reads VALUES(x'00000000000040008000000000000009',"
        "x'00000000000040008000000000000001','component',x'66697874757265',1)",
        "PRAGMA ignore_check_constraints=ON; UPDATE authority_meta SET catalogue_revision=0"
    };
    for (size_t n = 0; n < sizeof mutations / sizeof mutations[0]; ++n) {
        struct fixture fixture = make_fixture(build);
        struct ae_catalogue_identity expected = identity(kind);
        uint8_t boot[AE_ID_BYTES]; id(boot, 3);
        struct ae_catalogue *store = NULL;
        CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == 0);
        CHECK(ae_catalogue_close(&store) == 0);
        edit(&fixture, mutations[n]);
        CHECK(ae_catalogue_open(fixture.directory, &expected, &store) < 0 && store == NULL);
        cleanup(&fixture);
    }
    struct fixture fixture = make_fixture(build);
    struct ae_catalogue_identity expected = identity(kind);
    uint8_t boot[AE_ID_BYTES]; id(boot, 3);
    struct ae_catalogue *store = NULL;
    CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == 0);
    CHECK(ae_catalogue_close(&store) == 0);
    CHECK(fchmod(fixture.root, 0755) == 0);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == -EACCES);
    CHECK(fchmod(fixture.root, 0700) == 0);
    CHECK(fchmodat(fixture.root, "catalogue.sqlite3", 0640, 0) == 0);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == -EACCES);
    CHECK(fchmodat(fixture.root, "catalogue.sqlite3", 0600, 0) == 0);
    CHECK(linkat(fixture.root, "catalogue.sqlite3", fixture.root, "alias", 0) == 0);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == -EACCES);
    CHECK(unlinkat(fixture.root, "alias", 0) == 0);
    CHECK(symlinkat("/etc/passwd", fixture.root, "catalogue.sqlite3-journal") == 0);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == -EACCES);
    CHECK(unlinkat(fixture.root, "catalogue.sqlite3-journal", 0) == 0);
    int fd = openat(fixture.root, "catalogue.sqlite3-wal", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == -ENOTSUP);
    CHECK(unlinkat(fixture.root, "catalogue.sqlite3-wal", 0) == 0);
    CHECK(symlinkat(".", fixture.root, "selflink") == 0);
    char alias[PATH_MAX];
    int count = snprintf(alias, sizeof alias, "%s/selflink", fixture.directory);
    CHECK(count > 0 && (size_t)count < sizeof alias);
    int alias_result = ae_catalogue_open(alias, &expected, &store);
    /* A final O_DIRECTORY|O_NOFOLLOW symlink is ENOTDIR on this kernel;
     * resolution may instead report ELOOP. Neither may become a valid alias. */
    CHECK((alias_result == -ELOOP || alias_result == -ENOTDIR) && store == NULL);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
    CHECK(renameat(fixture.root, "catalogue.sqlite3", fixture.root, "moved") == 0);
    fd = openat(fixture.root, "catalogue.sqlite3", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0 && close(fd) == 0);
    struct ae_catalogue_info untouched, snapshot;
    memset(&untouched, 0xa5, sizeof untouched); snapshot = untouched;
    CHECK(ae_catalogue_get_info(store, &untouched) == -ESTALE && memcmp(&untouched, &snapshot, sizeof untouched) == 0);
    CHECK(ae_catalogue_close(&store) == 0);
    CHECK(unlinkat(fixture.root, "catalogue.sqlite3", 0) == 0);
    CHECK(renameat(fixture.root, "moved", fixture.root, "catalogue.sqlite3") == 0);
    fd = openat(fixture.root, "catalogue.sqlite3", O_WRONLY | O_TRUNC | O_CLOEXEC);
    CHECK(fd >= 0 && write(fd, "damaged fixture", 15) == 15 && close(fd) == 0);
    CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == -EUCLEAN);
    cleanup(&fixture);
}

static void recovery(const char *build, enum ae_catalogue_kind kind)
{
    {
        struct fixture fixture = make_fixture(build);
        struct ae_catalogue_identity expected = identity(kind);
        uint8_t boot[AE_ID_BYTES]; id(boot, 3);
        struct ae_catalogue *store = NULL;
        constructor_full = 1;
        CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == -ENOSPC && store == NULL);
        CHECK(integer(&fixture, "PRAGMA application_id") == 0);
        CHECK(integer(&fixture, "PRAGMA user_version") == 0);
        CHECK(integer(&fixture, "SELECT count(*) FROM sqlite_schema") == 0);
        CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == -EEXIST);
        CHECK(ae_catalogue_open(fixture.directory, &expected, &store) < 0 && store == NULL);
        cleanup(&fixture);
    }
    {
        struct fixture fixture = make_fixture(build);
        struct ae_catalogue_identity expected = identity(kind);
        uint8_t boot[AE_ID_BYTES]; id(boot, 3);
        struct ae_catalogue *store = NULL;
        directory_sync_fault = 1;
        CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == -EIO && store == NULL);
        directory_sync_fault = 0;
        /* Committed metadata is not erased after a failed directory flush. */
        CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == -EEXIST);
        CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
        CHECK(ae_catalogue_close(&store) == 0);
        cleanup(&fixture);
    }
    for (int fault = 1; fault <= 4; ++fault) {
        struct fixture fixture = make_fixture(build);
        struct ae_catalogue_identity expected = identity(kind);
        uint8_t boot[AE_ID_BYTES], runtime[AE_ID_BYTES]; id(boot, 3); id(runtime, 4);
        struct ae_catalogue *store = NULL;
        CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == 0);
        CHECK(ae_catalogue_begin_runtime(store, boot, runtime) == 0);
        CHECK(ae_catalogue_close(&store) == 0);
        seed(&fixture, kind);
        id(boot, 7); id(runtime, 5);
        if (fault <= 2) {
            CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
            commit_fault = fault;
            CHECK(ae_catalogue_begin_runtime(store, boot, runtime) == -EIO);
            commit_fault = 0;
            CHECK(ae_catalogue_close(&store) == 0);
        } else {
            pid_t child = fork();
            CHECK(child >= 0);
            if (child == 0) {
                CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
                commit_fault = fault;
                ae_catalogue_begin_runtime(store, boot, runtime);
                _exit(1);
            }
            int status = 0;
            CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == (fault == 3 ? 76 : 77));
        }
        CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
        struct ae_catalogue_info info;
        CHECK(ae_catalogue_get_info(store, &info) == 0);
        int committed = fault == 2 || fault == 4;
        CHECK(info.runtime_incarnation[15] == (committed ? 5 : 4));
        CHECK(info.boot_id[15] == (committed ? 7 : 3));
        CHECK(ae_catalogue_close(&store) == 0);
        CHECK(integer(&fixture, "SELECT count(*) FROM admission_gates WHERE state='closed'") == committed);
        cleanup(&fixture);
    }
    {
        struct fixture fixture = make_fixture(build);
        struct ae_catalogue_identity expected = identity(kind);
        uint8_t boot[AE_ID_BYTES], runtime[AE_ID_BYTES]; id(boot, 3); id(runtime, 4);
        struct ae_catalogue *store = NULL;
        CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == 0);
        CHECK(ae_catalogue_close(&store) == 0);
        edit(&fixture, "UPDATE authority_meta SET catalogue_revision=9223372036854775807");
        CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
        CHECK(ae_catalogue_begin_runtime(store, boot, runtime) == -EOVERFLOW);
        struct ae_catalogue_info info;
        CHECK(ae_catalogue_get_info(store, &info) == 0 && info.revision == INT64_MAX && info.runtime_incarnation[15] == 0);
        CHECK(ae_catalogue_close(&store) == 0);
        cleanup(&fixture);
    }
}

static void accounts(const char *build, enum ae_catalogue_kind kind)
{
    struct fixture fixture = make_fixture(build);
    struct ae_catalogue_identity expected = identity(kind);
    uint8_t boot[AE_ID_BYTES], first[AE_ID_BYTES], second[AE_ID_BYTES], zero[AE_ID_BYTES] = {0};
    id(boot, 3); id(first, 10); id(second, 11);
    struct ae_catalogue *store = NULL;
    CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == 0);
    int64_t revision = 99;
    CHECK(ae_catalogue_enroll_account(store, zero, 1000) == -EINVAL);
    CHECK(ae_catalogue_enroll_account(store, first, UINT32_MAX) == -EINVAL);
    CHECK(ae_catalogue_check_account(store, first, 1000, NULL) == -EINVAL);
    if (kind == AE_CATALOGUE_USER) {
        CHECK(ae_catalogue_enroll_account(store, first, 1000) == -EACCES);
        CHECK(ae_catalogue_check_account(store, first, 1000, &revision) == -EACCES && revision == 99);
    } else {
        CHECK(ae_catalogue_check_account(store, first, 1000, &revision) == -ENOENT && revision == 99);
        CHECK(ae_catalogue_enroll_account(store, first, 1000) == 0);
        CHECK(ae_catalogue_check_account(store, first, 1000, &revision) == 0 && revision == 1);
        for (int n = 0; n < 8; ++n) CHECK(ae_catalogue_enroll_account(store, first, 1000) == 0);
        CHECK(ae_catalogue_enroll_account(store, second, 1000) == -EEXIST);
        CHECK(ae_catalogue_enroll_account(store, first, 1001) == -ESTALE);
        revision = 99;
        CHECK(ae_catalogue_check_account(store, first, 1001, &revision) == -ESTALE && revision == 99);
        struct ae_catalogue_info info;
        CHECK(ae_catalogue_get_info(store, &info) == 0 && info.revision == 2);
        pid_t child = fork();
        CHECK(child >= 0);
        if (child == 0) {
            CHECK(ae_catalogue_enroll_account(store, second, 1001) == -ECHILD);
            CHECK(ae_catalogue_check_account(store, first, 1000, &revision) == -ECHILD);
            _exit(0);
        }
        int status;
        CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        CHECK(ae_catalogue_close(&store) == 0);
        edit(&fixture, "UPDATE scopes SET state='retired',revision=revision+1");
        CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
        CHECK(ae_catalogue_enroll_account(store, first, 1000) == -ESTALE);
        CHECK(ae_catalogue_check_account(store, first, 1000, &revision) == -ESTALE && revision == 99);
        CHECK(ae_catalogue_enroll_account(store, second, 1000) == 0);
        CHECK(ae_catalogue_check_account(store, second, 1000, &revision) == 0 && revision == 1);
        uint8_t high[AE_ID_BYTES]; id(high, 12);
        CHECK(ae_catalogue_enroll_account(store, high, 4294967294U) == 0);
        CHECK(ae_catalogue_check_account(store, high, 4294967294U, &revision) == 0 && revision == 1);
        CHECK(ae_catalogue_close(&store) == 0);
        CHECK(integer(&fixture, "SELECT count(*) FROM scopes WHERE state='retired' AND os_uid=1000 AND revision=2") == 1);
        edit(&fixture, "UPDATE authority_meta SET catalogue_revision=9223372036854775807");
        CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
        CHECK(ae_catalogue_enroll_account(store, second, 1000) == 0); /* No revision needed for an exact retry. */
        CHECK(ae_catalogue_enroll_account(store, boot, 1002) == -EOVERFLOW);
    }
    CHECK(ae_catalogue_close(&store) == 0);
    cleanup(&fixture);
    if (kind != AE_CATALOGUE_SYSTEM) return;
    for (int fault = 1; fault <= 4; ++fault) {
        fixture = make_fixture(build);
        CHECK(ae_catalogue_create(fixture.directory, &expected, boot, &store) == 0);
        CHECK(ae_catalogue_close(&store) == 0);
        if (fault <= 2) {
            CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
            commit_fault = fault;
            CHECK(ae_catalogue_enroll_account(store, first, 1000) == -EIO);
            commit_fault = 0;
            CHECK(ae_catalogue_close(&store) == 0);
        } else {
            pid_t child = fork();
            CHECK(child >= 0);
            if (child == 0) {
                CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
                commit_fault = fault;
                ae_catalogue_enroll_account(store, first, 1000);
                _exit(1);
            }
            int status;
            CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == (fault == 3 ? 76 : 77));
        }
        CHECK(ae_catalogue_open(fixture.directory, &expected, &store) == 0);
        struct ae_catalogue_info info;
        CHECK(ae_catalogue_get_info(store, &info) == 0 && info.revision == (fault == 2 || fault == 4 ? 2 : 1));
        CHECK(ae_catalogue_check_account(store, first, 1000, &revision) == (fault == 2 || fault == 4 ? 0 : -ENOENT));
        CHECK(ae_catalogue_enroll_account(store, first, 1000) == 0);
        CHECK(ae_catalogue_get_info(store, &info) == 0 && info.revision == 2);
        CHECK(ae_catalogue_close(&store) == 0);
        CHECK(integer(&fixture, "SELECT count(*) FROM scopes") == 1);
        cleanup(&fixture);
    }
}

int main(int argc, char **argv)
{
    if (argc == 6 && strcmp(argv[1], "open") == 0) {
        struct ae_catalogue_identity expected = identity(strcmp(argv[3], "system") == 0 ? AE_CATALOGUE_SYSTEM : AE_CATALOGUE_USER);
        CHECK(ae_id_from_hex(argv[4], strlen(argv[4]), expected.authority_epoch) == 0);
        if (expected.kind == AE_CATALOGUE_USER) CHECK(ae_id_from_hex(argv[5], strlen(argv[5]), expected.owner) == 0);
        struct ae_catalogue *store = NULL;
        CHECK(ae_catalogue_open(argv[2], &expected, &store) == 0);
        CHECK(ae_catalogue_close(&store) == 0);
        return 0;
    }
    CHECK(argc == 3);
    for (int kind = AE_CATALOGUE_SYSTEM; kind <= AE_CATALOGUE_USER; ++kind) {
        if (strcmp(argv[1], "basic") == 0) basic(argv[2], (enum ae_catalogue_kind)kind);
        else if (strcmp(argv[1], "reject") == 0) reject(argv[2], (enum ae_catalogue_kind)kind);
        else if (strcmp(argv[1], "accounts") == 0) accounts(argv[2], (enum ae_catalogue_kind)kind);
        else { CHECK(strcmp(argv[1], "recovery") == 0); recovery(argv[2], (enum ae_catalogue_kind)kind); }
    }
    puts("catalogue: both owner kinds passed");
    return 0;
}
