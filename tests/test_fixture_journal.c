#include "catalogue/store.h"

#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "fixture journal line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static int commit_fault;
int __real_sqlite3_exec(sqlite3 *, const char *, int (*)(void *,int,char **,char **), void *, char **);
int __wrap_sqlite3_exec(sqlite3 *db, const char *sql, int (*callback)(void *,int,char **,char **), void *context, char **message)
{
    if (commit_fault && strcmp(sql, "COMMIT") == 0) {
        int mode = commit_fault;
        commit_fault = 0;
        if (mode == 2) CHECK(__real_sqlite3_exec(db, sql, callback, context, message) == SQLITE_OK);
        return SQLITE_IOERR;
    }
    return __real_sqlite3_exec(db, sql, callback, context, message);
}
static void id(uint8_t bytes[16], uint8_t last)
{
    memset(bytes, 0, 16); bytes[6] = 0x40; bytes[8] = 0x80; bytes[15] = last;
}
static int64_t scalar(const char *directory, const char *sql)
{
    char path[4096];
    CHECK(snprintf(path, sizeof path, "%s/catalogue.sqlite3", directory) > 0);
    sqlite3 *db = NULL;
    CHECK(sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    sqlite3_stmt *statement = NULL;
    CHECK(sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    int64_t value = sqlite3_column_int64(statement, 0);
    CHECK(sqlite3_step(statement) == SQLITE_DONE && sqlite3_finalize(statement) == SQLITE_OK);
    CHECK(sqlite3_close(db) == SQLITE_OK);
    return value;
}

int main(int argc, char **argv)
{
    CHECK(argc == 4);
    uint8_t manifest[131072], expected[64];
    FILE *input = fopen(argv[1], "rb"); CHECK(input != NULL);
    size_t size = fread(manifest, 1, sizeof manifest, input);
    CHECK(!ferror(input) && fclose(input) == 0);
    input = fopen(argv[2], "rb"); CHECK(input != NULL);
    CHECK(fread(expected, 1, sizeof expected, input) == sizeof expected && fgetc(input) == EOF && fclose(input) == 0);
    struct ae_catalogue_identity identity = {.kind = AE_CATALOGUE_SYSTEM}; id(identity.authority_epoch, 1);
    uint8_t boot[16]; id(boot, 2);
    struct ae_catalogue *store = NULL;
    CHECK(ae_catalogue_create(argv[3], &identity, boot, &store) == 0);
    struct ae_fixture_request request = {.scope_revision=1, .expires_boottime_ns=8000000000000000000LL,
        .accepted_unix_ms=123456789, .os_uid=1000};
    id(request.plan,10); id(request.operation,11); id(request.actor,12); id(request.request_key,13);
    id(request.scope,3); id(request.install,14); id(request.generation,15); id(request.slot,16);
    id(request.publisher,1); id(request.boot,2);
    const uint8_t artifact[32] = {0x9b,0x76,0x99,0xb7,0x84,0xff,0xa7,0x8e,0xe9,0xda,0x6f,0xe1,0xec,0x5d,0x7e,0xd4,
        0x97,0x7e,0x04,0x90,0xae,0x66,0x1b,0x66,0xaa,0x8f,0x52,0xcb,0x6c,0x99,0x8a,0xfb};
    memcpy(request.artifact, artifact, 32); memcpy(request.expected_intent, expected, 32);
    CHECK(ae_catalogue_enroll_account(store, request.scope, 1000) == 0);
    struct ae_fixture_progress progress, marker;
    memset(&marker, 0xa5, sizeof marker); progress = marker;
    CHECK(ae_catalogue_fixture_get(store,&request,manifest,size,&progress)==-ENOENT);
    CHECK(memcmp(&progress,&marker,sizeof marker)==0 && scalar(argv[3],"SELECT count(*) FROM operations")==0);
    uint8_t other_boot[16]; id(other_boot,99);
    CHECK(ae_catalogue_fixture_accept(store,&request,manifest,size,other_boot,&progress) == -ESTALE);
    CHECK(memcmp(&progress,&marker,sizeof marker) == 0);
    struct ae_fixture_request changed = request;
    changed.os_uid = 1001;
    CHECK(ae_catalogue_fixture_accept(store,&changed,manifest,size,boot,&progress) == -ESTALE);
    changed = request; changed.expires_boottime_ns = 0; memcpy(changed.expected_intent,expected+32,32);
    CHECK(ae_catalogue_fixture_accept(store,&changed,manifest,size,boot,&progress) == -ESTALE);
    CHECK(scalar(argv[3],"SELECT count(*) FROM operations") == 0);
    commit_fault = 1;
    CHECK(ae_catalogue_fixture_accept(store,&request,manifest,size,boot,&progress) == -EIO);
    CHECK(memcmp(&progress,&marker,sizeof marker) == 0 && scalar(argv[3],"SELECT count(*) FROM operations") == 0);
    commit_fault = 2;
    CHECK(ae_catalogue_fixture_accept(store,&request,manifest,size,boot,&progress) == -EIO);
    CHECK(memcmp(&progress,&marker,sizeof marker) == 0 && scalar(argv[3],"SELECT count(*) FROM operations") == 1);
    CHECK(ae_catalogue_close(&store) == 0 && ae_catalogue_open(argv[3],&identity,&store) == 0);
    CHECK(ae_catalogue_fixture_accept(store,&request,manifest,size,boot,&progress) == 0);
    CHECK(progress.step == AE_FIXTURE_PLANNED && !progress.evidence_present && progress.revision == 1);
    CHECK(memcmp(progress.intent,expected,32) == 0);
    int64_t revision = scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");
    CHECK(ae_catalogue_fixture_accept(store,&request,manifest,size,boot,&progress) == 0);
    CHECK(scalar(argv[3],"SELECT catalogue_revision FROM authority_meta") == revision);
    changed = request; id(changed.operation,77);
    CHECK(ae_catalogue_fixture_accept(store,&changed,manifest,size,boot,&progress) == -ESTALE);
    changed = request; id(changed.request_key,78);
    CHECK(ae_catalogue_fixture_accept(store,&changed,manifest,size,boot,&progress) == -ESTALE);
    changed = request; ++changed.accepted_unix_ms;
    CHECK(ae_catalogue_fixture_accept(store,&changed,manifest,size,boot,&progress) == -ESTALE);
    uint8_t evidence[32]; memset(evidence,0x42,sizeof evidence);
    CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,AE_FIXTURE_PLANNED,evidence,&progress)==-EINVAL);
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_READY,evidence,&progress) == -EINVAL);
    progress = marker; commit_fault = 2;
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_EXECUTING,evidence,&progress) == -EIO);
    CHECK(memcmp(&progress,&marker,sizeof marker) == 0);
    CHECK(ae_catalogue_fixture_accept(store,&request,manifest,size,boot,&progress) == 0);
    CHECK(progress.step == AE_FIXTURE_EXECUTING && progress.evidence_present && memcmp(progress.evidence,evidence,32) == 0);
    evidence[0] ^= 1;
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_READY,evidence,&progress) == -ESTALE);
    evidence[0] ^= 1;
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_READY,evidence,&progress) == 0);
    CHECK(progress.step == AE_FIXTURE_READY && progress.revision == 3);
    revision = scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_READY,evidence,&progress) == 0);
    CHECK(scalar(argv[3],"SELECT catalogue_revision FROM authority_meta") == revision);
    CHECK(scalar(argv[3],"SELECT count(*) FROM operations WHERE state='running' AND phase='publish' AND outcome IS NULL") == 1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM artifacts WHERE store_state='staged'") == 1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM installs WHERE state='preparing' AND active_generation_id IS NULL") == 1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM generations WHERE publication_state='staged' AND activated_once=0") == 1);
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_EXECUTING,evidence,&progress) == -EINVAL);
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_UNCERTAIN,evidence,&progress) == 0);
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_EXECUTING,evidence,&progress) == 0);
    CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_READY,evidence,&progress) == 0);
    uint8_t incarnation[16]; id(incarnation,100);
    CHECK(ae_catalogue_begin_runtime(store,other_boot,incarnation) == 0);
    CHECK(ae_catalogue_fixture_accept(store,&request,manifest,size,other_boot,&progress) == 0 && progress.step == AE_FIXTURE_READY);
    uint8_t publication[32]; memset(publication,0x43,sizeof publication);
    CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,AE_FIXTURE_EXECUTING,publication,&progress)==-EINVAL);
    CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,AE_FIXTURE_READY,publication,&progress)==-EINVAL);
    const enum ae_fixture_step steps[]={AE_FIXTURE_PLANNED,AE_FIXTURE_EXECUTING,AE_FIXTURE_UNCERTAIN,AE_FIXTURE_EXECUTING,AE_FIXTURE_READY};
    for (size_t n=0;n<sizeof steps/sizeof steps[0];++n) {
        progress=marker; commit_fault=1;
        revision=scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");
        CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,steps[n],publication,&progress)==-EIO);
        CHECK(memcmp(&progress,&marker,sizeof marker)==0 && scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision);
        commit_fault=2;
        CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,steps[n],publication,&progress)==-EIO);
        CHECK(memcmp(&progress,&marker,sizeof marker)==0 && scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision+1);
        CHECK(ae_catalogue_close(&store)==0 && ae_catalogue_open(argv[3],&identity,&store)==0);
        CHECK(ae_catalogue_fixture_get(store,&request,manifest,size,&progress)==0 && progress.publication_present && progress.publication_step==steps[n]);
        CHECK(memcmp(progress.publication_evidence,publication,32)==0 && memcmp(progress.evidence,evidence,32)==0);
        CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,steps[n],publication,&progress)==0);
        CHECK(scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision+1);
        publication[0]^=1;
        CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,steps[n],publication,&progress)==-ESTALE);
        publication[0]^=1;
        CHECK(ae_catalogue_fixture_transition(store,&request,manifest,size,AE_FIXTURE_READY,evidence,&progress)==-EBUSY);
    }
    CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,AE_FIXTURE_UNCERTAIN,publication,&progress)==-EINVAL);
    CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,AE_FIXTURE_EXECUTING,publication,&progress)==-EINVAL);
    CHECK(scalar(argv[3],"SELECT count(*) FROM operations WHERE state='running' AND phase='reconcile' AND reason_code='activation_pending' AND outcome IS NULL AND result_kind IS NULL")==1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM artifacts WHERE store_state='verified'")==1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM generations WHERE publication_state='committed' AND activated_once=0")==1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM installs WHERE state='preparing' AND active_generation_id IS NULL")==1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM generation_leases")==0);
    CHECK(scalar(argv[3],"SELECT count(*) FROM operation_steps WHERE step_no=1 AND action='publish' AND subject_kind='storage' AND length(subject_ref)=32 AND state='applied'")==1);
    changed=request; changed.scope_revision=2; progress=marker;
    CHECK(ae_catalogue_fixture_get(store,&changed,manifest,size,&progress)<0 && memcmp(&progress,&marker,sizeof marker)==0);
    CHECK(ae_catalogue_close(&store) == 0);
    puts("Native stage/publication journal, independent intent, lost commits, immutable bindings and cross-boot replay passed; no activation or lease.");
    return 0;
}
