#include "catalogue/store.h"
#include "core/digest.h"

#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "fixture journal line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static int commit_fault;
static int activation_fault;
int __real_sqlite3_exec(sqlite3 *, const char *, int (*)(void *,int,char **,char **), void *, char **);
int __wrap_sqlite3_exec(sqlite3 *db, const char *sql, int (*callback)(void *,int,char **,char **), void *context, char **message)
{
    if (commit_fault && strcmp(sql, "COMMIT") == 0) {
        int mode = commit_fault;
        commit_fault = 0;
        if (mode==3) _exit(73);
        if (mode == 2 || mode==4) CHECK(__real_sqlite3_exec(db, sql, callback, context, message) == SQLITE_OK);
        if (mode==4) _exit(73);
        return SQLITE_IOERR;
    }
    return __real_sqlite3_exec(db, sql, callback, context, message);
}
int __real_sqlite3_step(sqlite3_stmt *);
int __wrap_sqlite3_step(sqlite3_stmt *statement)
{
    if (activation_fault && strstr(sqlite3_sql(statement),"UPDATE installs SET state='active'")!=NULL) {
        int mode=activation_fault;
        activation_fault=0;
        if (mode==2) CHECK(__real_sqlite3_step(statement)==SQLITE_DONE);
        return SQLITE_IOERR;
    }
    return __real_sqlite3_step(statement);
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
static void mutate(const char *directory,const char *sql)
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",directory)>0);
    sqlite3 *db=NULL;
    CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK);
    CHECK(sqlite3_exec(db,"PRAGMA foreign_keys=ON",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(sqlite3_exec(db,sql,NULL,NULL,NULL)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);
}
static void activation_hash(struct ae_fixture_activation *activation)
{
    uint8_t bytes[72]; memcpy(bytes,"AETHACT1",8); memcpy(bytes+8,activation->publication_evidence,32);
    const int64_t numbers[]={activation->install_revision,activation->generation_revision,activation->gate_revision,activation->completed_unix_ms};
    for (size_t n=0;n<4;++n) {
        uint64_t value=(uint64_t)numbers[n];
        for (size_t b=0;b<8;++b) { bytes[40U+n*8U+7U-b]=(uint8_t)value; value>>=8; }
    }
    CHECK(ae_sha256_bytes(bytes,sizeof bytes,activation->expected_evidence)==0);
}

int main(int argc, char **argv)
{
    CHECK(argc == 5);
    uint8_t manifest[131072], expected[96];
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
    struct ae_fixture_activation premature={.install_revision=1,.generation_revision=2,.gate_revision=2,.completed_unix_ms=123457000};
    memcpy(premature.publication_evidence,publication,32); activation_hash(&premature);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&premature,other_boot,&progress)==-EINVAL);
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
        if (steps[n]!=AE_FIXTURE_READY) CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&premature,other_boot,&progress)==-EINVAL);
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
    struct ae_fixture_activation activation={.install_revision=1,.generation_revision=2,.gate_revision=2,.completed_unix_ms=123457000};
    memcpy(activation.publication_evidence,publication,32); activation_hash(&activation);
    struct ae_fixture_activation altered=activation;
    ++altered.install_revision; activation_hash(&altered);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&altered,other_boot,&progress)==-ESTALE);
    altered=activation; --altered.generation_revision; activation_hash(&altered);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&altered,other_boot,&progress)==-ESTALE);
    altered=activation; --altered.gate_revision; activation_hash(&altered);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&altered,other_boot,&progress)==-ESTALE);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,boot,&progress)==-ESTALE);
    mutate(argv[3],"UPDATE authority_meta SET runtime_incarnation=NULL");
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-ESTALE);
    id(incarnation,104);
    CHECK(ae_catalogue_begin_runtime(store,other_boot,incarnation)==0);
    activation.publication_evidence[0]^=1; activation_hash(&activation);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-ESTALE);
    activation.publication_evidence[0]^=1; activation_hash(&activation);
    mutate(argv[3],"UPDATE admission_gates SET state='open',revision=3");
    activation.gate_revision=3; activation_hash(&activation);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-EBUSY);
    mutate(argv[3],"UPDATE admission_gates SET state='closing',revision=4,closing_operation_id=(SELECT operation_id FROM operations)");
    activation.gate_revision=4; activation_hash(&activation);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-EBUSY);
    mutate(argv[3],"UPDATE admission_gates SET state='closed',revision=5");
    activation.gate_revision=5; activation_hash(&activation);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-EBUSY);
    mutate(argv[3],"UPDATE admission_gates SET revision=6,closing_operation_id=NULL");
    activation.gate_revision=6; activation_hash(&activation);
    CHECK(memcmp(activation.expected_evidence,expected+64,32)==0);
    mutate(argv[3],"INSERT INTO generation_leases SELECT X'00000000000040008000000000000028',generation_id,X'00000000000040008000000000000001',X'00000000000040008000000000000029','recovery',X'00000000000040008000000000000063','reserved',1 FROM generations");
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-EBUSY);
    mutate(argv[3],"UPDATE generation_leases SET state='released',revision=revision+1");
    mutate(argv[3],"INSERT INTO generations SELECT X'0000000000004000800000000000004d',install_id,component_id,artifact_id,'committed',0,1 FROM generations; INSERT INTO generation_edges SELECT X'0000000000004000800000000000004d',X'0000000000004000800000000000000f'");
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-EBUSY);
    mutate(argv[3],"DELETE FROM generation_edges; DELETE FROM generations WHERE generation_id=X'0000000000004000800000000000004d'");
    progress=marker; commit_fault=1;
    revision=scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-EIO);
    CHECK(memcmp(&progress,&marker,sizeof marker)==0 && scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision);
    for (int mode=1;mode<=2;++mode) {
        activation_fault=mode;
        CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-EIO);
        CHECK(memcmp(&progress,&marker,sizeof marker)==0 && scalar(argv[3],"SELECT count(*) FROM installs WHERE state='active'")==0);
        CHECK(scalar(argv[3],"SELECT count(*) FROM operation_steps WHERE step_no=2")==0);
    }
    if (strcmp(argv[4],"lost")==0) {
        commit_fault=2;
        CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==-EIO);
        CHECK(memcmp(&progress,&marker,sizeof marker)==0);
        CHECK(ae_catalogue_close(&store)==0);
    } else {
        CHECK(ae_catalogue_close(&store)==0);
        pid_t child=fork(); CHECK(child>=0);
        if (child==0) {
            CHECK(ae_catalogue_open(argv[3],&identity,&store)==0);
            commit_fault=strcmp(argv[4],"exit-before")==0 ? 3 : 4;
            (void)ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress);
            _exit(74);
        }
        int status=0; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==73);
    }
    CHECK(ae_catalogue_open(argv[3],&identity,&store)==0);
    CHECK(ae_catalogue_fixture_get(store,&request,manifest,size,&progress)==0);
    CHECK(progress.activation_present==(strcmp(argv[4],"exit-before")!=0));
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,other_boot,&progress)==0);
    CHECK(progress.activation_present && progress.completed_unix_ms==activation.completed_unix_ms && memcmp(progress.activation_evidence,expected+64,32)==0);
    revision=scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");
    uint8_t next_boot[16],next_runtime[16]; id(next_boot,101); id(next_runtime,102);
    CHECK(ae_catalogue_begin_runtime(store,next_boot,next_runtime)==0);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,next_boot,&progress)==0);
    CHECK(scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision+1);
    CHECK(ae_catalogue_fixture_publication(store,&request,manifest,size,AE_FIXTURE_READY,publication,&progress)==0 && progress.activation_present);
    altered=activation; ++altered.completed_unix_ms; activation_hash(&altered);
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&altered,next_boot,&progress)==-ESTALE);
    altered=activation; altered.expected_evidence[0]^=1;
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&altered,next_boot,&progress)==-EBADMSG);
    CHECK(scalar(argv[3],"SELECT count(*) FROM operations WHERE state='completed' AND outcome='applied' AND result_kind='install' AND result_ref=X'0000000000004000800000000000000e' AND completed_unix_ms=123457000")==1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM generations WHERE activated_once=1 AND revision=3")==1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM installs WHERE state='active' AND revision=2 AND active_generation_id=X'0000000000004000800000000000000f'")==1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM admission_gates WHERE state='closed' AND revision=6 AND closing_operation_id IS NULL")==1);
    CHECK(scalar(argv[3],"SELECT count(*) FROM generation_leases WHERE state<>'released'")==0);
    CHECK(scalar(argv[3],"SELECT count(*) FROM launch_dispatches")==0);
    mutate(argv[3],"UPDATE admission_gates SET state='open',revision=7");
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,next_boot,&progress)==0);
    CHECK(scalar(argv[3],"SELECT count(*) FROM admission_gates WHERE state='open' AND revision=7")==1);
    mutate(argv[3],"UPDATE admission_gates SET state='closed',revision=8");
    /* Completed receipts survive later lifecycle changes. Retrying the old
     * request must never switch back from a newer generation or undo removal. */
    mutate(argv[3],"INSERT INTO generations SELECT X'0000000000004000800000000000004d',install_id,component_id,artifact_id,'committed',0,1 FROM generations; UPDATE installs SET active_generation_id=X'0000000000004000800000000000004d',revision=revision+1");
    CHECK(ae_catalogue_fixture_get(store,&request,manifest,size,&progress)==0 && progress.activation_present && !progress.installation_active);
    revision=scalar(argv[3],"SELECT catalogue_revision FROM authority_meta");
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,next_boot,&progress)==0 && !progress.installation_active);
    CHECK(scalar(argv[3],"SELECT count(*) FROM installs WHERE active_generation_id=X'0000000000004000800000000000004d'")==1);
    mutate(argv[3],"UPDATE installs SET state='removed',active_generation_id=NULL,revision=revision+1; UPDATE generations SET publication_state='reclaimed',revision=revision+1; UPDATE artifacts SET store_state='reclaimed'");
    CHECK(ae_catalogue_fixture_activate(store,&request,manifest,size,&activation,next_boot,&progress)==0 && progress.activation_present && !progress.installation_active);
    CHECK(scalar(argv[3],"SELECT count(*) FROM installs WHERE state='removed'")==1 && scalar(argv[3],"SELECT catalogue_revision FROM authority_meta")==revision);
    CHECK(ae_catalogue_close(&store) == 0);
    puts("Native stage/publication/activation journal, closed admission, CAS/holder guards, SQL/process faults and cross-boot receipts passed; no launch.");
    return 0;
}
