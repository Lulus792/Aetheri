#include "catalogue/launch.h"
#include "catalogue/setup.h"
#include "catalogue/execution.h"
#include "catalogue/lifetime.h"
#include "catalogue/handoff.h"
#include "core/digest.h"
#include <errno.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"user launch line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int commit_fault,step_fault,step_number,crash_mode,ddl_fault,ddl_number;
#ifdef AETHERI_TEST_EXECUTION_SERVICE
void ae_test_execution_service(struct ae_catalogue *,const struct ae_execution_profile *,struct ae_launch_context *,const char *,const char *);
void ae_test_execution_wire(struct ae_catalogue *,const struct ae_execution_profile *,struct ae_launch_context *,const char *,const char *);
void ae_test_execution_commit_fault(int value) { commit_fault=value; }
#endif
int __real_sqlite3_exec(sqlite3 *,const char *,int (*)(void *,int,char **,char **),void *,char **);
int __wrap_sqlite3_exec(sqlite3 *db,const char *sql,int (*callback)(void *,int,char **,char **),void *context,char **error)
{
    if (ddl_fault && sqlite3_db_filename(db,"main") && sqlite3_db_filename(db,"main")[0] &&
        (strncmp(sql,"CREATE ",7)==0 || strcmp(sql,"PRAGMA user_version=3")==0 || strcmp(sql,"PRAGMA user_version=4")==0 || strcmp(sql,"PRAGMA user_version=5")==0 || strcmp(sql,"PRAGMA user_version=6")==0 || strncmp(sql,"DROP TRIGGER ",13)==0) && ++ddl_number==ddl_fault) {
        ddl_fault=0; return SQLITE_IOERR;
    }
    if (strcmp(sql,"COMMIT")==0 && (commit_fault || crash_mode)) {
        int mode=commit_fault; commit_fault=0;
        if (mode==2 || crash_mode==2) CHECK(__real_sqlite3_exec(db,sql,callback,context,error)==SQLITE_OK);
        if (crash_mode) _exit(70+crash_mode);
        return SQLITE_IOERR;
    }
    return __real_sqlite3_exec(db,sql,callback,context,error);
}
int __real_sqlite3_step(sqlite3_stmt *);
int __wrap_sqlite3_step(sqlite3_stmt *statement)
{
    if (step_fault && sqlite3_stmt_readonly(statement)==0 && ++step_number==step_fault) {
        step_fault=0; return SQLITE_IOERR;
    }
    return __real_sqlite3_step(statement);
}
static void id(uint8_t bytes[16],uint8_t last) { memset(bytes,0,16); bytes[6]=0x40; bytes[8]=0x80; bytes[15]=last; }
static sqlite3 *database(const char *directory)
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",directory)>0);
    sqlite3 *db=NULL; CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK);
    CHECK(sqlite3_exec(db,"PRAGMA foreign_keys=ON",NULL,NULL,NULL)==SQLITE_OK); return db;
}
static void edit(const char *directory,const char *sql)
{
    sqlite3 *db=database(directory); CHECK(sqlite3_exec(db,sql,NULL,NULL,NULL)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);
}
static int64_t scalar(const char *directory,const char *sql)
{
    sqlite3 *db=database(directory); sqlite3_stmt *statement=NULL;
    CHECK(sqlite3_prepare_v2(db,sql,-1,&statement,NULL)==SQLITE_OK && sqlite3_step(statement)==SQLITE_ROW);
    int64_t value=sqlite3_column_int64(statement,0);
    CHECK(sqlite3_finalize(statement)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK); return value;
}
static void database_digest(const char *directory,uint8_t digest[32])
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",directory)>0);
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW); struct stat s;
    CHECK(fd>=0 && fstat(fd,&s)==0 && s.st_size>0);
    CHECK(ae_sha256_fd(fd,(uint64_t)s.st_size,digest)==0 && close(fd)==0);
}
static void execution_profile(struct ae_execution_profile *p,const struct ae_user_launch *q,const struct ae_launch_context *c)
{
    *p=(struct ae_execution_profile){.install_revision=2,.install_gate_revision=3,.data_gate_revision=3};
    const struct ae_launch_request *r=&q->reservation;
    const uint8_t *source[]={r->system_epoch,c->system_runtime,r->broker_epoch,r->broker_runtime,r->boot,r->account,r->install,r->generation,
        r->data,q->storage,r->launch,r->lease,NULL,q->local.operation};
    uint8_t *target[]={p->system_epoch,p->system_runtime,p->broker_epoch,p->broker_runtime,p->boot,p->account,p->install,p->generation,
        p->data,p->storage,p->launch,p->lease,p->root_operation,p->user_operation};
    for (size_t n=0;n<14;++n) if (source[n]) memcpy(target[n],source[n],16);
    id(p->root_operation,41); memset(p->starter_digest,0x34,32); memset(p->allocation_digest,0x56,32);
    CHECK(ae_launch_fingerprint(r,p->request_digest)==0);
}
static void execution_codec(const struct ae_execution_profile *p,const char *expected)
{
    uint8_t bytes[AE_EXECUTION_BYTES+1],reference[AE_EXECUTION_BYTES+32],hash[32];
    CHECK(ae_execution_encode(p,bytes)==0 && ae_execution_digest(p,hash)==0);
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s.execution",expected)>0);
    FILE *file=fopen(path,"rb"); CHECK(file && fread(reference,1,sizeof reference,file)==sizeof reference && fclose(file)==0);
    CHECK(memcmp(bytes,reference,AE_EXECUTION_BYTES)==0 && memcmp(hash,reference+AE_EXECUTION_BYTES,32)==0);
    struct ae_execution_profile decoded,marker; memset(&marker,0xa5,sizeof marker);
    CHECK(ae_execution_decode(bytes,AE_EXECUTION_BYTES,&decoded)==0 && memcmp(&decoded,p,sizeof *p)==0);
    for (size_t n=0;n<=sizeof bytes;++n) if (n!=AE_EXECUTION_BYTES) {
        decoded=marker; CHECK(ae_execution_decode(bytes,n,&decoded)==-EBADMSG && memcmp(&decoded,&marker,sizeof marker)==0);
    }
    /* Every ID's version/variant, every revision's lower/upper bound and the
     * request hash are independent decode boundaries. Keep outputs unchanged. */
    for (size_t n=0;n<14;++n) for (size_t m=0;m<2;++m) {
        size_t offset=8+16*n+(m==0 ? 6U : 8U); uint8_t saved=bytes[offset]; bytes[offset]=0;
        decoded=marker; CHECK(ae_execution_decode(bytes,AE_EXECUTION_BYTES,&decoded)==-EBADMSG && memcmp(&decoded,&marker,sizeof marker)==0); bytes[offset]=saved;
    }
    for (size_t n=0;n<3;++n) {
        uint8_t saved[8]; memcpy(saved,bytes+232+8*n,8);
        for (int m=0;m<3;++m) {
            memset(bytes+232+8*n,m==0 ? 0 : m==1 ? 0x80 : 0xff,8); decoded=marker;
            CHECK(ae_execution_decode(bytes,AE_EXECUTION_BYTES,&decoded)==-EBADMSG && memcmp(&decoded,&marker,sizeof marker)==0);
        }
        memcpy(bytes+232+8*n,saved,8);
    }
    bytes[320]^=1; decoded=marker;
    CHECK(ae_execution_decode(bytes,AE_EXECUTION_BYTES,&decoded)==-EBADMSG && memcmp(&decoded,&marker,sizeof marker)==0); bytes[320]^=1;
    memcpy(bytes,"AETHSUP1",8); decoded=marker;
    CHECK(ae_execution_decode(bytes,AE_EXECUTION_BYTES,&decoded)==-EBADMSG && memcmp(&decoded,&marker,sizeof marker)==0);
    struct ae_setup_profile old;
    CHECK(ae_setup_decode(reference,AE_SETUP_BYTES,&old)==-EBADMSG);
    CHECK(ae_execution_decode(NULL,AE_EXECUTION_BYTES,&decoded)==-EINVAL && ae_execution_decode(reference,AE_EXECUTION_BYTES,NULL)==-EINVAL);
    CHECK(ae_execution_encode(NULL,bytes)==-EINVAL && ae_execution_encode(p,NULL)==-EINVAL && ae_execution_digest(p,NULL)==-EINVAL);
    struct ae_execution_profile invalid=*p; invalid.storage[6]=0; memset(bytes,0xa5,sizeof bytes);
    CHECK(ae_execution_encode(&invalid,bytes)==-EBADMSG);
    for (size_t n=0;n<sizeof bytes;++n) CHECK(bytes[n]==0xa5);
    struct ae_execution_evidence e={0}; id(e.invocation,66); memset(e.context_digest,2,32);
    CHECK(snprintf(path,sizeof path,"%s.diagnostics",expected)>0); file=fopen(path,"rb"); CHECK(file);
    for (int kind=1;kind<=4;++kind) {
        struct ae_execution_diagnostic d={.kind=(enum ae_execution_diagnostic_kind)kind,.phase=kind==2 ? 8U : 0U,.error=kind==2 ? 1U : kind==4 ? 5U : 0U};
        uint8_t vector[96],actual[32]; char reason[64];
        CHECK(fread(vector,1,sizeof vector,file)==sizeof vector && ae_execution_diagnostic_encode(p,&e,&d,actual,reason)==0);
        CHECK(memcmp(vector,actual,32)==0 && memcmp(vector+32,reason,64)==0);
    }
    CHECK(fgetc(file)==EOF && fclose(file)==0);
    const struct ae_execution_diagnostic bad[]={
        {0,0,0},{5,0,0},{AE_EXECUTION_DIAGNOSTIC_FAILURE,0,1},{AE_EXECUTION_DIAGNOSTIC_FAILURE,11,1},
        {AE_EXECUTION_DIAGNOSTIC_FAILURE,8,0},{AE_EXECUTION_DIAGNOSTIC_FAILURE,8,4096},
        {AE_EXECUTION_DIAGNOSTIC_UNKNOWN,1,0},{AE_EXECUTION_DIAGNOSTIC_MALFORMED,0,1},
        {AE_EXECUTION_DIAGNOSTIC_OBSERVER,1,1},{AE_EXECUTION_DIAGNOSTIC_OBSERVER,0,0}};
    for (size_t n=0;n<sizeof bad/sizeof bad[0];++n) {
        uint8_t actual[32],sentinel[32]; char reason[64],original[64]; memset(sentinel,0xa5,32); memcpy(actual,sentinel,32); memset(original,0xa5,64); memcpy(reason,original,64);
        CHECK(ae_execution_diagnostic_encode(p,&e,&bad[n],actual,reason)==-EINVAL && memcmp(actual,sentinel,32)==0 && memcmp(reason,original,64)==0);
    }
}
static void user_execution_test(struct ae_catalogue *root,struct ae_catalogue **user,const struct ae_catalogue_identity *identity,
    const struct ae_user_launch *q,struct ae_launch_context *context,const char *directory,const char *scenario)
{
    struct ae_execution_profile p; execution_profile(&p,q,context);
    struct ae_user_execution_result output,marker; memset(&marker,0xa5,sizeof marker); output=marker;
    CHECK(ae_catalogue_prepare_user_execution(root,&p,&output)==-EPERM && memcmp(&output,&marker,sizeof marker)==0);
    CHECK(ae_catalogue_prepare_user_execution(*user,&p,NULL)==-EINVAL);
    CHECK(ae_catalogue_prepare_user_execution(NULL,&p,&output)==-EINVAL);
    CHECK(ae_catalogue_prepare_user_execution(*user,NULL,&output)==-EINVAL);
    struct ae_execution_profile wrong=p;
    uint8_t *fields[]={wrong.system_epoch,wrong.broker_epoch,wrong.broker_runtime,wrong.boot,wrong.account,
        wrong.install,wrong.generation,wrong.data,wrong.storage,wrong.launch,wrong.lease,wrong.root_operation,wrong.user_operation};
    for (size_t n=0;n<sizeof fields/sizeof *fields;++n) {
        fields[n][15]^=0x80; output=marker;
        struct ae_launch_request alternate=q->reservation;
        const uint8_t *source[]={wrong.system_epoch,wrong.lease,wrong.launch,wrong.broker_epoch,wrong.broker_runtime,
            wrong.account,wrong.install,wrong.generation,wrong.data,wrong.boot};
        uint8_t *target[]={alternate.system_epoch,alternate.lease,alternate.launch,alternate.broker_epoch,alternate.broker_runtime,
            alternate.account,alternate.install,alternate.generation,alternate.data,alternate.boot};
        for (size_t m=0;m<10;++m) memcpy(target[m],source[m],16);
        CHECK(ae_launch_fingerprint(&alternate,wrong.request_digest)==0);
        CHECK(ae_catalogue_prepare_user_execution(*user,&wrong,&output)==-ESTALE && memcmp(&output,&marker,sizeof marker)==0);
        fields[n][15]^=0x80; memcpy(wrong.request_digest,p.request_digest,32);
    }
    uint8_t before[32],after[32]; database_digest(directory,before);
    for (int n=1;n<=4;++n) {
        step_fault=n; step_number=0; output=marker;
        CHECK(ae_catalogue_prepare_user_execution(*user,&p,&output)==-EIO && step_fault==0 && memcmp(&output,&marker,sizeof marker)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    }
    commit_fault=1; CHECK(ae_catalogue_prepare_user_execution(*user,&p,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
    database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    int lost=strcmp(scenario,"exec-user-prepare-lost")==0;
    int crash_before=strcmp(scenario,"exec-user-prepare-crash-before")==0;
    int crash_after=strcmp(scenario,"exec-user-prepare-crash-after")==0;
    if (crash_before || crash_after) {
        CHECK(ae_catalogue_close(user)==0); pid_t child=fork(); CHECK(child>=0);
        if (child==0) {
            struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(directory,identity,&fresh)==0);
            crash_mode=crash_before ? 1 : 2;
            (void)ae_catalogue_prepare_user_execution(fresh,&p,&output); _exit(90);
        }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(crash_before ? 71 : 72));
        CHECK(ae_catalogue_open(directory,identity,user)==0);
    }
    if (strncmp(scenario,"exec-user-prepare-overflow-",27)==0) {
        const char *table=scenario+27; char query[160];
        CHECK(strcmp(table,"launches")==0 || strcmp(table,"operations")==0 || strcmp(table,"authority_meta")==0);
        CHECK(snprintf(query,sizeof query,"UPDATE %s SET %s=9223372036854775807",table,
            strcmp(table,"authority_meta")==0 ? "catalogue_revision" : "revision")>0); edit(directory,query);
        database_digest(directory,before); output=marker;
        CHECK(ae_catalogue_prepare_user_execution(*user,&p,&output)==-EOVERFLOW && memcmp(&output,&marker,sizeof marker)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0); return;
    }
    if (lost) {
        commit_fault=2; output=marker;
        CHECK(ae_catalogue_prepare_user_execution(*user,&p,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
    }
    int64_t steps=scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=1");
    CHECK(steps==((lost || crash_after) ? 1 : 0));
    CHECK(ae_catalogue_prepare_user_execution(*user,&p,&output)==0 && output.submit_required==(steps==0));
    CHECK(strcmp(output.launch.state,"preparing")==0 && strcmp(output.launch.operation_state,"running")==0 &&
        strcmp(output.launch.readiness,"unknown")==0 && output.launch.root_link_present && !output.launch.pre_dispatch_withdrawable);
    CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=1 AND action='stage' AND subject_kind='data' AND state='executing'")==1);
    CHECK(scalar(directory,"SELECT reason_code='exec_preparing' FROM operations")==1);
    CHECK(ae_catalogue_authorize_user_execution(*user,&p)==-ESTALE);
    database_digest(directory,before); struct ae_user_execution_result original=output;
    for (int n=0;n<3;++n) {
        CHECK(ae_catalogue_prepare_user_execution(*user,&p,&output)==0 && !output.submit_required &&
            memcmp(&output.launch,&original.launch,sizeof output.launch)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    }
    wrong=p; wrong.starter_digest[0]^=1;
    CHECK(ae_catalogue_prepare_user_execution(*user,&wrong,&output)==-EEXIST);
    wrong=p; wrong.allocation_digest[0]^=1;
    CHECK(ae_catalogue_prepare_user_execution(*user,&wrong,&output)==-EEXIST);
    wrong=p; wrong.artifact[0]^=1;
    CHECK(ae_catalogue_prepare_user_execution(*user,&wrong,&output)==-EEXIST);
    wrong=p; wrong.system_runtime[15]^=1;
    CHECK(ae_catalogue_prepare_user_execution(*user,&wrong,&output)==-EEXIST);
    database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    if (strcmp(scenario,"exec-user-prepare-gate")==0) {
        edit(directory,"UPDATE admission_gates SET state='closed',revision=revision+1"); output=marker;
        CHECK(ae_catalogue_prepare_user_execution(*user,&p,&output)==-ESTALE && memcmp(&output,&marker,sizeof marker)==0);
        CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=1 AND state='executing'")==1);
    } else if (strcmp(scenario,"exec-user-prepare-restart")==0) {
        uint8_t replacement[16]; id(replacement,52);
        CHECK(ae_catalogue_begin_runtime(*user,p.boot,replacement)==0); output=marker;
        CHECK(ae_catalogue_prepare_user_execution(*user,&p,&output)==-ESTALE && memcmp(&output,&marker,sizeof marker)==0);
        struct ae_user_launch_snapshot current;
        CHECK(ae_catalogue_get_user_launch(*user,p.user_operation,&current)==0 && strcmp(current.state,"recovery_required")==0 && !current.pre_dispatch_withdrawable);
        CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=1 AND state='executing'")==1);
    }
    struct ae_dispatch_snapshot dispatch; struct ae_reservation_receipt receipt;
    CHECK(ae_catalogue_cancel_reservation(root,p.lease,context,p.broker_epoch,p.user_operation,1,123500,&dispatch)==0);
    CHECK(ae_catalogue_get_receipt(root,p.lease,context,&receipt)==0 && receipt.withdrawn);
    struct ae_user_launch_snapshot unsettled;
    CHECK(ae_catalogue_finish_withdrawn_operation(*user,p.user_operation,&receipt,123600,&unsettled)==-EAGAIN);
    CHECK(scalar(directory,"SELECT state='running' AND outcome IS NULL FROM operations")==1);
}
static void user_observe_test(struct ae_catalogue *root,struct ae_catalogue **user,const struct ae_catalogue_identity *identity,
    const struct ae_user_launch *q,struct ae_launch_context *context,const char *directory,const char *scenario)
{
    struct ae_execution_profile p; execution_profile(&p,q,context);
    struct ae_execution_evidence e; id(e.invocation,66); memset(e.context_digest,0x78,32);
    struct ae_user_launch_snapshot output,marker; memset(&marker,0xa5,sizeof marker); output=marker;
    CHECK(ae_catalogue_observe_user_execution(root,&p,AE_USER_EXECUTION_REGISTER,&e,&output)==-EPERM && memcmp(&output,&marker,sizeof marker)==0);
    CHECK(ae_catalogue_observe_user_execution(*user,&p,(enum ae_user_execution_stage)0,&e,&output)==-EINVAL);
    struct ae_user_execution_result prepared; CHECK(ae_catalogue_prepare_user_execution(*user,&p,&prepared)==0 && prepared.submit_required);
    CHECK(ae_catalogue_observe_user_execution(*user,&p,AE_USER_EXECUTION_READY,&e,&output)==-ENOENT);
    for (int stage=AE_USER_EXECUTION_REGISTER;stage<=AE_USER_EXECUTION_READY;++stage) {
        uint8_t before[32],after[32]; database_digest(directory,before);
        for (int n=1;n<=(stage==AE_USER_EXECUTION_REGISTER ? 5 : 4);++n) {
            step_fault=n; step_number=0; output=marker;
            CHECK(ae_catalogue_observe_user_execution(*user,&p,(enum ae_user_execution_stage)stage,&e,&output)==-EIO && step_fault==0 && memcmp(&output,&marker,sizeof marker)==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        }
        commit_fault=1; output=marker;
        CHECK(ae_catalogue_observe_user_execution(*user,&p,(enum ae_user_execution_stage)stage,&e,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        int selected=(stage==AE_USER_EXECUTION_REGISTER && strstr(scenario,"register")!=NULL) || (stage==AE_USER_EXECUTION_READY && strstr(scenario,"ready")!=NULL);
        if (selected && strstr(scenario,"crash")!=NULL) {
            CHECK(ae_catalogue_close(user)==0); pid_t child=fork(); CHECK(child>=0);
            int before_commit=strstr(scenario,"before")!=NULL;
            if (child==0) { struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(directory,identity,&fresh)==0); crash_mode=before_commit ? 1 : 2; (void)ae_catalogue_observe_user_execution(fresh,&p,(enum ae_user_execution_stage)stage,&e,&output); _exit(90); }
            int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(before_commit ? 71 : 72));
            CHECK(ae_catalogue_open(directory,identity,user)==0);
        } else if (selected && strstr(scenario,"lost")!=NULL) {
            commit_fault=2; output=marker;
            CHECK(ae_catalogue_observe_user_execution(*user,&p,(enum ae_user_execution_stage)stage,&e,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
        }
        if (stage==AE_USER_EXECUTION_REGISTER && strstr(scenario,"overflow-")!=NULL) {
            const char *table=strstr(scenario,"overflow-")+9; char sql[160];
            CHECK(strcmp(table,"launches")==0 || strcmp(table,"operations")==0 || strcmp(table,"authority_meta")==0);
            CHECK(snprintf(sql,sizeof sql,"UPDATE %s SET %s=9223372036854775807",table,strcmp(table,"authority_meta")==0 ? "catalogue_revision" : "revision")>0); edit(directory,sql);
            output=marker; database_digest(directory,before);
            CHECK(ae_catalogue_observe_user_execution(*user,&p,AE_USER_EXECUTION_REGISTER,&e,&output)==-EOVERFLOW && memcmp(&output,&marker,sizeof marker)==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0); return;
        }
        CHECK(ae_catalogue_observe_user_execution(*user,&p,(enum ae_user_execution_stage)stage,&e,&output)==0);
        CHECK(strcmp(output.state,stage==AE_USER_EXECUTION_REGISTER ? "starting" : "running")==0 && strcmp(output.readiness,stage==AE_USER_EXECUTION_REGISTER ? "unknown" : "ready")==0 && !output.pre_dispatch_withdrawable);
        database_digest(directory,before); struct ae_user_launch_snapshot original=output;
        for (int n=0;n<3;++n) {
            CHECK(ae_catalogue_observe_user_execution(*user,&p,(enum ae_user_execution_stage)stage,&e,&output)==0 && memcmp(&output,&original,sizeof output)==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        }
        struct ae_execution_evidence wrong=e; wrong.context_digest[0]^=1; output=marker;
        CHECK(ae_catalogue_observe_user_execution(*user,&p,(enum ae_user_execution_stage)stage,&wrong,&output)==-EEXIST && memcmp(&output,&marker,sizeof marker)==0);
        wrong=e; wrong.invocation[15]^=1;
        CHECK(ae_catalogue_observe_user_execution(*user,&p,(enum ae_user_execution_stage)stage,&wrong,&output)==-ESTALE);
        if (stage==AE_USER_EXECUTION_REGISTER && strstr(scenario,"restart")!=NULL) {
            uint8_t runtime[16]; id(runtime,52); CHECK(ae_catalogue_begin_runtime(*user,p.boot,runtime)==0);
            CHECK(ae_catalogue_observe_user_execution(*user,&p,AE_USER_EXECUTION_READY,&e,&output)==-ESTALE);
            CHECK(ae_catalogue_get_user_launch(*user,p.user_operation,&output)==0 && strcmp(output.state,"recovery_required")==0 && !output.pre_dispatch_withdrawable); return;
        }
        if (stage==AE_USER_EXECUTION_REGISTER && strstr(scenario,"gate")!=NULL) {
            edit(directory,"UPDATE admission_gates SET state='closed',revision=revision+1");
            CHECK(ae_catalogue_observe_user_execution(*user,&p,AE_USER_EXECUTION_READY,&e,&output)==-ESTALE); return;
        }
    }
    CHECK(ae_catalogue_observe_user_execution(*user,&p,AE_USER_EXECUTION_REGISTER,&e,&output)==0 && strcmp(output.state,"running")==0 && strcmp(output.readiness,"ready")==0);
    CHECK(scalar(directory,"SELECT state='running' AND outcome IS NULL FROM operations")==1);
    CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no BETWEEN 1 AND 3 AND state='applied'")==3);
}
static void root_observe_test(struct ae_catalogue **root,const struct ae_catalogue_identity *identity,const struct ae_execution_profile *p,
    struct ae_launch_context *c,const struct ae_execution_evidence *e,const char *directory,const char *scenario)
{
    uint8_t app_digest[32]; memset(app_digest,0x87,sizeof app_digest);
    struct ae_execution_result output,marker; memset(&marker,0xa5,sizeof marker); output=marker;
    if (strstr(scenario,"early-diagnostic")) {
        struct ae_execution_diagnostic d={.kind=AE_EXECUTION_DIAGNOSTIC_UNKNOWN};
        CHECK(ae_catalogue_execution_report(*root,p,c,e,&d,&output)==0); output=marker;
    }
    uint8_t before[32],after[32]; database_digest(directory,before);
    for (int n=1;n<=5;++n) {
        step_fault=n; step_number=0;
        CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==-EIO && step_fault==0 && memcmp(&output,&marker,sizeof marker)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    }
    commit_fault=1; CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
    database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    if (strstr(scenario,"closed-gate")) {
        edit(directory,"UPDATE admission_gates SET state='closed',revision=revision+1");
        CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==-EAGAIN); return;
    }
    if (strstr(scenario,"restart")) {
        uint8_t runtime[16]; id(runtime,6); CHECK(ae_catalogue_begin_runtime(*root,p->boot,runtime)==0);
        CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==-ESTALE);
        CHECK(scalar(directory,"SELECT state='uncertain' FROM launch_dispatches")==1 && scalar(directory,"SELECT state='running' FROM generation_leases")==1); return;
    }
    if (strstr(scenario,"overflow-")) {
        edit(directory,strstr(scenario,"dispatch") ? "UPDATE launch_dispatches SET revision=9223372036854775807" :
            strstr(scenario,"operation") ? "UPDATE operations SET revision=9223372036854775807" : "UPDATE authority_meta SET catalogue_revision=9223372036854775807");
        CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==-EOVERFLOW && memcmp(&output,&marker,sizeof marker)==0); return;
    }
    if (strstr(scenario,"crash")) {
        CHECK(ae_catalogue_close(root)==0); pid_t child=fork(); CHECK(child>=0); int first=strstr(scenario,"before")!=NULL;
        if (child==0) { struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(directory,identity,&fresh)==0); crash_mode=first ? 1 : 2; (void)ae_catalogue_observe_execution(fresh,p,c,e,app_digest,&output); _exit(90); }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(first ? 71 : 72));
        CHECK(ae_catalogue_open(directory,identity,root)==0);
    } else if (strstr(scenario,"lost")) {
        commit_fault=2; CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
    }
    CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==0 && !output.effect_required && strcmp(output.dispatch.state,"observed")==0);
    CHECK(scalar(directory,"SELECT state='running' FROM generation_leases")==1 && scalar(directory,"SELECT state='running' AND outcome IS NULL FROM operations")==1);
    CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=3 AND state='applied'")==1);
    database_digest(directory,before);
    struct ae_execution_profile wrong=*p; wrong.starter_digest[0]^=1; output=marker;
    CHECK(ae_catalogue_observe_execution(*root,&wrong,c,e,app_digest,&output)==-ESTALE && memcmp(&output,&marker,sizeof marker)==0);
    struct ae_execution_evidence bad=*e; bad.context_digest[0]^=1;
    CHECK(ae_catalogue_observe_execution(*root,p,c,&bad,app_digest,&output)==-ESTALE);
    bad=*e; bad.invocation[15]^=1; CHECK(ae_catalogue_observe_execution(*root,p,c,&bad,app_digest,&output)==-ESTALE);
    app_digest[0]^=1; CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==-EEXIST); app_digest[0]^=1;
    struct ae_launch_context replacement=*c; id(replacement.broker_runtime,52);
    CHECK(ae_catalogue_observe_execution(*root,p,&replacement,e,app_digest,&output)==-ESTALE);
    database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    if (strstr(scenario,"late-diagnostic")) {
        struct ae_execution_diagnostic d={.kind=AE_EXECUTION_DIAGNOSTIC_UNKNOWN};
        CHECK(ae_catalogue_execution_report(*root,p,c,e,&d,&output)==0 && !output.effect_required && strcmp(output.dispatch.state,"observed")==0);
        CHECK(scalar(directory,"SELECT reason_code='exec_channel_unknown' FROM operations")==1);
    }
    if (strstr(scenario,"early-diagnostic")) CHECK(scalar(directory,"SELECT reason_code='exec_channel_unknown' FROM operations")==1);
    if (strstr(scenario,"later-")) {
        edit(directory,"UPDATE admission_gates SET state='closed',revision=revision+1");
        edit(directory,strstr(scenario,"terminal") ? "UPDATE launch_dispatches SET state='terminal',revision=revision+1; UPDATE generation_leases SET state='released',revision=revision+1" :
            strstr(scenario,"uncertain") ? "UPDATE launch_dispatches SET state='uncertain',revision=revision+1" : "UPDATE launch_dispatches SET state='closing',revision=revision+1");
    }
    database_digest(directory,before);
    for (int n=0;n<3;++n) {
        CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_digest,&output)==0 && !output.effect_required);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    }
    CHECK(ae_catalogue_execution(*root,p,c,AE_EXECUTION_ARM,e,&output)<0);
    CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=5 AND state='applied'")==1);
}
static void completion_test(struct ae_catalogue **root,struct ae_catalogue **user,const struct ae_catalogue_identity *ri,const struct ae_catalogue_identity *ui,
    const struct ae_execution_profile *p,struct ae_launch_context *c,const struct ae_execution_evidence *e,const char *rd,const char *ud,const char *scenario)
{
    uint8_t app_hash[32]; memset(app_hash,0x87,sizeof app_hash);
    struct ae_execution_evidence ue=*e; memset(ue.context_digest,0x99,32);
    struct ae_user_launch_snapshot launch;
    CHECK(ae_catalogue_observe_user_execution(*user,p,AE_USER_EXECUTION_REGISTER,&ue,&launch)==0);
    if (!strstr(scenario,"missing-ready")) CHECK(ae_catalogue_observe_user_execution(*user,p,AE_USER_EXECUTION_READY,&ue,&launch)==0);
    struct ae_execution_diagnostic d={.kind=AE_EXECUTION_DIAGNOSTIC_UNKNOWN}; struct ae_execution_result observed;
    if (strstr(scenario,"bad-diagnostic")) d=(struct ae_execution_diagnostic){.kind=AE_EXECUTION_DIAGNOSTIC_FAILURE,.phase=8,.error=1};
    if (!strstr(scenario,"missing-diagnostic")) CHECK(ae_catalogue_execution_report(*root,p,c,e,&d,&observed)==0);
    CHECK(ae_catalogue_observe_execution(*root,p,c,e,app_hash,&observed)==0);
    struct ae_execution_completion receipt,marker; memset(&marker,0xa5,sizeof marker); receipt=marker;
    if (strstr(scenario,"bad-diagnostic") || strstr(scenario,"missing-diagnostic")) {
        CHECK(ae_catalogue_complete_execution(*root,p,c,e,app_hash,123700,&receipt)==-EAGAIN && memcmp(&receipt,&marker,sizeof marker)==0);
        if (strstr(scenario,"bad-diagnostic")) return;
        CHECK(ae_catalogue_execution_report(*root,p,c,e,&d,&observed)==0);
    }
    int64_t lease_revision=scalar(rd,"SELECT revision FROM generation_leases"),dispatch_revision=scalar(rd,"SELECT revision FROM launch_dispatches");
    for (int owner=0;owner<2;++owner) {
        const char *directory=owner ? ud : rd; uint8_t before[32],after[32]; database_digest(directory,before);
        struct ae_user_launch_snapshot local,sentinel; memset(&sentinel,0xa5,sizeof sentinel); local=sentinel;
        for (int n=1;n<=(owner ? 4 : 3);++n) {
            step_fault=n; step_number=0;
            int r=owner ? ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123800,&local) : ae_catalogue_complete_execution(*root,p,c,e,app_hash,123700,&receipt);
            CHECK(r==-EIO && step_fault==0 && (owner ? memcmp(&local,&sentinel,sizeof local)==0 : memcmp(&receipt,&marker,sizeof receipt)==0));
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        }
        commit_fault=1;
        CHECK((owner ? ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123800,&local) : ae_catalogue_complete_execution(*root,p,c,e,app_hash,123700,&receipt))==-EIO);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        const char *role=owner ? "user-" : "root-"; int selected=strstr(scenario,role)!=NULL;
        if (selected && strstr(scenario,"gate")) {
            edit(directory,"UPDATE admission_gates SET state='closed',revision=revision+1");
            CHECK((owner ? ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123800,&local) : ae_catalogue_complete_execution(*root,p,c,e,app_hash,123700,&receipt))<0); return;
        }
        if (selected && strstr(scenario,"restart")) {
            uint8_t runtime[16]; id(runtime,owner ? 52 : 6); CHECK(ae_catalogue_begin_runtime(owner ? *user : *root,p->boot,runtime)==0);
            CHECK((owner ? ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123800,&local) : ae_catalogue_complete_execution(*root,p,c,e,app_hash,123700,&receipt))==-ESTALE); return;
        }
        if (selected && strstr(scenario,"overflow")) {
            edit(directory,strstr(scenario,"operation") ? "UPDATE operations SET revision=9223372036854775807" : "UPDATE authority_meta SET catalogue_revision=9223372036854775807");
            CHECK((owner ? ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123800,&local) : ae_catalogue_complete_execution(*root,p,c,e,app_hash,123700,&receipt))==-EOVERFLOW); return;
        }
        if (selected && strstr(scenario,"crash")) {
            struct ae_catalogue **slot=owner ? user : root; CHECK(ae_catalogue_close(slot)==0); pid_t child=fork(); CHECK(child>=0); int early=strstr(scenario,"before")!=NULL;
            if (child==0) { struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(directory,owner ? ui : ri,&fresh)==0); crash_mode=early ? 1 : 2;
                if (owner) (void)ae_catalogue_finish_user_execution(fresh,p,&ue,app_hash,&receipt,123800,&local); else (void)ae_catalogue_complete_execution(fresh,p,c,e,app_hash,123700,&receipt); _exit(90); }
            int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(early ? 71 : 72)); CHECK(ae_catalogue_open(directory,owner ? ui : ri,slot)==0);
        } else if (selected && strstr(scenario,"lost")) {
            commit_fault=2;
            CHECK((owner ? ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123800,&local) : ae_catalogue_complete_execution(*root,p,c,e,app_hash,123700,&receipt))==-EIO);
            CHECK(owner ? memcmp(&local,&sentinel,sizeof local)==0 : memcmp(&receipt,&marker,sizeof receipt)==0);
        }
        CHECK((owner ? ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123800,&local) : ae_catalogue_complete_execution(*root,p,c,e,app_hash,123700,&receipt))==0);
        if (!owner) {
            CHECK(receipt.completed_unix_ms==123700 && ae_execution_completion_match(p,e,app_hash,&receipt)==0);
            if (strstr(scenario,"missing-ready")) {
                CHECK(ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123800,&local)==-ESTALE);
                CHECK(ae_catalogue_observe_user_execution(*user,p,AE_USER_EXECUTION_READY,&ue,&launch)==0);
            }
        } else CHECK(strcmp(local.state,"running")==0 && strcmp(local.readiness,"ready")==0 && strcmp(local.operation_state,"completed")==0 && !local.pre_dispatch_withdrawable);
        database_digest(directory,before);
        for (int n=0;n<3;++n) {
            struct ae_execution_completion repeat;
            CHECK((owner ? ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,123900,&local) : ae_catalogue_complete_execution(*root,p,c,e,app_hash,123900,&repeat))==0);
            if (!owner) CHECK(memcmp(&repeat,&receipt,sizeof repeat)==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        }
    }
    CHECK(scalar(rd,"SELECT revision FROM generation_leases")==lease_revision && scalar(rd,"SELECT revision FROM launch_dispatches")==dispatch_revision);
    CHECK(scalar(rd,"SELECT state='running' FROM generation_leases")==1 && scalar(rd,"SELECT state='observed' FROM launch_dispatches")==1);
    CHECK(scalar(rd,"SELECT state='completed' AND outcome='applied' AND reason_code='exec_channel_unknown' FROM operations")==1);
    CHECK(scalar(ud,"SELECT state='completed' AND outcome='applied' AND completed_unix_ms=123800 FROM operations")==1);
    struct ae_execution_completion wrong=receipt; wrong.revision++;
    CHECK(ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&wrong,123900,&launch)==-EEXIST);
    if (strstr(scenario,"later-terminal")) {
        edit(rd,"UPDATE admission_gates SET state='closed',revision=revision+1; UPDATE launch_dispatches SET state='terminal',revision=revision+1; UPDATE generation_leases SET state='released',revision=revision+1");
        edit(ud,"UPDATE admission_gates SET state='closed',revision=revision+1; UPDATE launches SET state='exited',revision=revision+1");
        uint8_t before[32],after[32]; database_digest(rd,before); CHECK(ae_catalogue_complete_execution(*root,p,c,e,app_hash,999999,&wrong)==0 && memcmp(&wrong,&receipt,sizeof receipt)==0); database_digest(rd,after); CHECK(memcmp(before,after,32)==0);
        database_digest(ud,before); CHECK(ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&receipt,999999,&launch)==0 && strcmp(launch.state,"exited")==0); database_digest(ud,after); CHECK(memcmp(before,after,32)==0);
    }
}
static int close_call(int stage,struct ae_catalogue *root,struct ae_catalogue *user,const struct ae_execution_profile *p,struct ae_launch_context *c,
    const struct ae_execution_evidence *e,const struct ae_execution_evidence *ue,const struct ae_execution_completion *started,
    const struct ae_run_receipt receipts[3],const uint8_t ended[32],struct ae_run_receipt *out,struct ae_user_launch_snapshot *launch)
{
    if (stage==0) return ae_catalogue_begin_run_close(root,p,c,e,started,124000,out);
    if (stage==1) return ae_catalogue_quiesce_user_run(user,p,ue,&receipts[0],124100,out);
    if (stage==2) return ae_catalogue_release_run(root,p,c,e,&receipts[1],ended,124200,out);
    return ae_catalogue_end_user_run(user,p,ue,&receipts[2],124300,launch);
}
static void lifetime_test(struct ae_catalogue **root,struct ae_catalogue **user,const struct ae_catalogue_identity *ri,const struct ae_catalogue_identity *ui,
    const struct ae_execution_profile *p,struct ae_launch_context *c,const struct ae_execution_evidence *e,const char *rd,const char *ud,const char *scenario)
{
    uint8_t app[32],ended[32]; memset(app,0x87,32); memset(ended,0x65,32);
    struct ae_execution_evidence ue=*e; memset(ue.context_digest,0x99,32);
    struct ae_execution_completion started; struct ae_user_launch_snapshot launch,launch_marker;
    memset(&launch_marker,0xa5,sizeof launch_marker);
    CHECK(ae_catalogue_complete_execution(*root,p,c,e,app,123900,&started)==0);
    struct ae_run_receipt unsupported,unsupported_marker; memset(&unsupported_marker,0xa5,sizeof unsupported_marker); unsupported=unsupported_marker;
    CHECK(ae_catalogue_begin_run_close(*root,p,c,e,&started,124000,&unsupported)==-EPROTONOSUPPORT && memcmp(&unsupported,&unsupported_marker,sizeof unsupported)==0);
    int64_t root_rev=scalar(rd,"SELECT revision FROM operations"),user_rev=scalar(ud,"SELECT revision FROM operations");
    for (int owner=0;owner<2;++owner) {
        struct ae_catalogue **slot=owner ? user : root; const char *directory=owner ? ud : rd;
        uint8_t before[32],after[32]; database_digest(directory,before);
        for (int fault=1;fault<=8;++fault) {
            ddl_number=0; ddl_fault=fault; CHECK(ae_catalogue_upgrade_lifetimes(*slot)==-EIO && ddl_fault==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0 && scalar(directory,"PRAGMA user_version")==2);
        }
        commit_fault=1; CHECK(ae_catalogue_upgrade_lifetimes(*slot)==-EIO && scalar(directory,"PRAGMA user_version")==2);
        commit_fault=2; CHECK(ae_catalogue_upgrade_lifetimes(*slot)==-EIO && scalar(directory,"PRAGMA user_version")==3);
        CHECK(ae_catalogue_upgrade_lifetimes(*slot)==0 && ae_catalogue_close(slot)==0 && ae_catalogue_open(directory,owner ? ui : ri,slot)==0);
        database_digest(directory,before); CHECK(ae_catalogue_upgrade_lifetimes(*slot)==0); database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    }
    struct ae_run_receipt receipts[3],out,marker; memset(receipts,0,sizeof receipts); memset(&marker,0xa5,sizeof marker);
    CHECK(ae_catalogue_get_run_receipt(*root,p,e,AE_RUN_ROOT_CLOSING,&out)==-ENOENT);
    for (int stage=0;stage<4;++stage) {
        int owner=stage%2; const char *directory=owner ? ud : rd; struct ae_catalogue **slot=owner ? user : root;
        if (stage==1 && strstr(scenario,"grants")) {
            edit(ud,"INSERT INTO resources(resource_id,owner_scope,kind,fixture_root_ref,locator,display_name,observed_boot_id,broker_incarnation,observed_device,observed_inode,observed_size,state) SELECT X'00000000000040008000000000000070',owner_scope_ref,'file',X'00000000000040008000000000000071','document','Document',current_boot_id,runtime_incarnation,1,1,1,'bound' FROM authority_meta; INSERT INTO grants SELECT X'00000000000040008000000000000072',X'00000000000040008000000000000070',launch_id,install_id,data_epoch_id,X'00000000000040008000000000000073','read','native','launch','pending',1 FROM launches");
            out=marker;
            CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==-EAGAIN && memcmp(&out,&marker,sizeof out)==0);
            edit(ud,"UPDATE grants SET state='active',revision=revision+1");
            CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==-EAGAIN);
            edit(ud,"UPDATE grants SET state='closing',revision=revision+1; UPDATE grants SET state='revoked',revision=revision+1");
            CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==-EAGAIN);
            CHECK(scalar(rd,"SELECT state='running' FROM generation_leases")==1 && scalar(ud,"SELECT state='running' FROM launches")==1);
            edit(ud,"DELETE FROM grants"); /* metadata-only fixture, no FD or real document */
        }
        uint8_t before[32],after[32]; database_digest(directory,before); out=marker; launch=launch_marker;
        int writes=stage==2 ? 4 : 3;
        for (int n=1;n<=writes;++n) {
            step_fault=n; step_number=0;
            CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==-EIO && step_fault==0);
            CHECK(stage==3 ? memcmp(&launch,&launch_marker,sizeof launch)==0 : memcmp(&out,&marker,sizeof out)==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        }
        commit_fault=1; CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==-EIO);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        char selector[20]; CHECK(snprintf(selector,sizeof selector,"step%d",stage)>0); int selected=strstr(scenario,selector)!=NULL;
        if (selected && strstr(scenario,"limit")) {
            edit(directory,strstr(scenario,"launch") ? (owner ? "UPDATE launches SET revision=9223372036854775807" : "UPDATE launch_dispatches SET revision=9223372036854775807") :
                strstr(scenario,"lease") ? "UPDATE generation_leases SET revision=9223372036854775807" : "UPDATE authority_meta SET catalogue_revision=9223372036854775807");
            CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==-EOVERFLOW); return;
        }
        if (selected && strstr(scenario,"replace")) {
            uint8_t runtime[16]; id(runtime,owner ? 52 : 6); CHECK(ae_catalogue_begin_runtime(*slot,p->boot,runtime)==0);
            CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)<0);
            CHECK(scalar(directory,"SELECT count(*) FROM run_closures WHERE state='uncertain'")==1); return;
        }
        if (stage==0 && strstr(scenario,"gates")) {
            edit(rd,"UPDATE admission_gates SET state='closed',revision=revision+1"); edit(ud,"UPDATE admission_gates SET state='closed',revision=revision+1");
        }
        if (selected && strstr(scenario,"crash")) {
            CHECK(ae_catalogue_close(slot)==0); pid_t child=fork(); CHECK(child>=0); int early=strstr(scenario,"before")!=NULL;
            if (child==0) { struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(directory,owner ? ui : ri,&fresh)==0); crash_mode=early ? 1 : 2;
                (void)close_call(stage,owner ? *root : fresh,owner ? fresh : *user,p,c,e,&ue,&started,receipts,ended,&out,&launch); _exit(90); }
            int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(early ? 71 : 72)); CHECK(ae_catalogue_open(directory,owner ? ui : ri,slot)==0);
        } else if (selected && strstr(scenario,"lost")) {
            commit_fault=2; CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==-EIO);
            CHECK(stage==3 ? memcmp(&launch,&launch_marker,sizeof launch)==0 : memcmp(&out,&marker,sizeof out)==0);
        }
        CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==0);
        if (stage<3) receipts[stage]=out;
        if (stage<2 && strstr(scenario,"lifetime-v4")) CHECK(ae_catalogue_upgrade_processes(*slot)==0 && scalar(directory,"SELECT state='closing' FROM run_closures")==1);
        database_digest(directory,before);
        CHECK(close_call(stage,*root,*user,p,c,e,&ue,&started,receipts,ended,&out,&launch)==0);
        if (stage<3) CHECK(memcmp(&out,&receipts[stage],sizeof out)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        CHECK(scalar(rd,"SELECT revision FROM operations")==root_rev && scalar(ud,"SELECT revision FROM operations")==user_rev);
        CHECK(scalar(rd,"SELECT state='running' FROM generation_leases")== (stage<2));
    }
    CHECK(strcmp(launch.state,"exited")==0 && strcmp(launch.readiness,"unavailable")==0);
    CHECK(scalar(rd,"SELECT state='terminal' FROM launch_dispatches")==1);
    CHECK(scalar(rd,"SELECT count(*) FROM operation_steps")==7 && scalar(ud,"SELECT count(*) FROM operation_steps")==5);
    struct ae_run_receipt changed=receipts[1]; changed.previous_digest[0]^=1;
    CHECK(ae_catalogue_release_run(*root,p,c,e,&changed,ended,999999,&out)==-ESTALE);
    ended[0]^=1; CHECK(ae_catalogue_release_run(*root,p,c,e,&receipts[1],ended,999999,&out)==-EEXIST); ended[0]^=1;
    changed=receipts[2]; changed.evidence_digest[0]^=1;
    CHECK(ae_catalogue_end_user_run(*user,p,&ue,&changed,999999,&launch)==-EEXIST);
    struct ae_execution_completion again;
    CHECK(ae_catalogue_complete_execution(*root,p,c,e,app,999999,&again)==0 && memcmp(&again,&started,sizeof again)==0);
    CHECK(ae_catalogue_finish_user_execution(*user,p,&ue,app,&started,999999,&launch)==0 && strcmp(launch.state,"exited")==0);
    CHECK(ae_catalogue_get_run_receipt(*root,p,e,AE_RUN_ROOT_RELEASED,&out)==0 && memcmp(&out,&receipts[2],sizeof out)==0);
    struct ae_execution_result original,untouched; memset(&untouched,0xa5,sizeof untouched); original=untouched;
    CHECK(ae_catalogue_execution(*root,p,c,AE_EXECUTION_ARM,e,&original)==-EAGAIN && memcmp(&original,&untouched,sizeof original)==0);
}
static struct ae_execution_process process_record(const struct ae_execution_profile *p,const struct ae_execution_evidence *e,enum ae_execution_process_role role)
{
    struct ae_execution_process v={.role=role,.pidfd_id=UINT64_MAX,.cgroup_id=(UINT64_C(1)<<63)+31,.pid=4242,.uid=role==AE_EXECUTION_STARTER ? 0U : 1000U};
    CHECK(ae_execution_digest(p,v.profile_digest)==0); memcpy(v.boot,p->boot,16); memcpy(v.invocation,e->invocation,16); memcpy(v.evidence_digest,e->context_digest,32);
    char hex[33]; const char *digits="0123456789abcdef";
    for (size_t n=0;n<16;++n) { hex[2*n]=digits[p->launch[n]>>4]; hex[2*n+1]=digits[p->launch[n]&15U]; } hex[32]=0;
    CHECK(snprintf(v.unit,sizeof v.unit,"aetheri-app-%s.service",hex)>0 && snprintf(v.cgroup,sizeof v.cgroup,"/system.slice/%s",v.unit)>0);
    strcpy(v.label,role==AE_EXECUTION_STARTER ? "system_u:system_r:aetheri_starter_t:s0" : "system_u:system_r:aetheri_exec_app_t:s0:c35,c36"); return v;
}
static void process_enrollment(struct ae_catalogue **slot,const struct ae_catalogue_identity *identity,const char *directory,
    const struct ae_execution_profile *p,const struct ae_execution_evidence *e,const struct ae_execution_process *process,int stage,const char *scenario)
{
    struct ae_execution_process_snapshot output,marker; memset(&marker,0xa5,sizeof marker); output=marker;
    uint8_t before[32],after[32]; database_digest(directory,before);
    for (int n=1;n<=2;++n) {
        step_fault=n; step_number=0; CHECK(ae_catalogue_enroll_execution_process(*slot,p,e,process,124000+stage,&output)==-EIO && step_fault==0);
        CHECK(memcmp(&output,&marker,sizeof marker)==0); database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    }
    commit_fault=1; CHECK(ae_catalogue_enroll_execution_process(*slot,p,e,process,124000+stage,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
    database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    char lost[32],crash[32]; CHECK(snprintf(lost,sizeof lost,"lost-%d",stage)>0 && snprintf(crash,sizeof crash,"crash-%d-",stage)>0);
    if (strstr(scenario,lost)) {
        commit_fault=2; CHECK(ae_catalogue_enroll_execution_process(*slot,p,e,process,124000+stage,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
    } else if (strstr(scenario,crash)) {
        CHECK(ae_catalogue_close(slot)==0); pid_t child=fork(); CHECK(child>=0); int early=strstr(scenario,"before")!=NULL;
        if (child==0) { struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(directory,identity,&fresh)==0); crash_mode=early ? 1 : 2;
            (void)ae_catalogue_enroll_execution_process(fresh,p,e,process,124000+stage,&output); _exit(90); }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(early ? 71 : 72)); CHECK(ae_catalogue_open(directory,identity,slot)==0);
    }
    CHECK(ae_catalogue_enroll_execution_process(*slot,p,e,process,124000+stage,&output)==0 && output.recorded_unix_ms==124000+stage && output.process.pidfd_id==UINT64_MAX);
    database_digest(directory,before);
    CHECK(ae_catalogue_enroll_execution_process(*slot,p,e,process,999999,&output)==0 && output.recorded_unix_ms==124000+stage);
    CHECK(ae_catalogue_get_execution_process(*slot,p,process->role,&output)==0 && output.recorded_unix_ms==124000+stage);
    database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    struct ae_execution_process changed=*process; changed.pidfd_id--;
    CHECK(ae_catalogue_enroll_execution_process(*slot,p,e,&changed,999999,&output)==-EEXIST);
    sqlite3 *db=database(directory);
    CHECK(sqlite3_exec(db,"UPDATE execution_processes SET recorded_unix_ms=9",NULL,NULL,NULL)==SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(db,"DELETE FROM execution_processes",NULL,NULL,NULL)==SQLITE_CONSTRAINT && sqlite3_close(db)==SQLITE_OK);
}
static struct ae_execution_handoff handoff_record(const struct ae_execution_process *app)
{
    struct ae_execution_handoff v={.process=*app,.pid_namespace=4026531836U,.cgroup_namespace=4026531835U,
        .events_inode=UINT64_MAX-5,.events_mount=UINT64_MAX-4,.events_mode=0100444};
    v.root=(struct ae_root_identity){.mount_id=UINT64_MAX-4,.inode=1,.device_minor=29,.mode=0040755,.filesystem_type=0x63677270};
    v.group=v.root; v.group.inode=app->cgroup_id; strcpy(v.custodian,"aetheri-p0-execution-root.service"); id(v.custodian_invocation,67); return v;
}
static void handoff_action(struct ae_catalogue **slot,const struct ae_catalogue_identity *ri,const char *directory,
    const struct ae_execution_profile *p,const struct ae_execution_handoff *original,enum ae_handoff_stage stage,const char *scenario)
{
    struct ae_handoff_result out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    struct ae_handoff_snapshot prior; int observed=ae_catalogue_get_handoff(*slot,p,&prior);
    CHECK(observed==0 || observed==-ENOENT); int64_t revision=observed==0 ? prior.revision+1 : 1;
    uint8_t before[32],after[32]; database_digest(directory,before);
    for (int n=1;n<=2;++n) {
        step_fault=n; step_number=0; CHECK(ae_catalogue_handoff(*slot,p,stage,original,125000+stage,&out)==-EIO && !step_fault && memcmp(&out,&marker,sizeof out)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    }
    commit_fault=1; CHECK(ae_catalogue_handoff(*slot,p,stage,original,125000+stage,&out)==-EIO && memcmp(&out,&marker,sizeof out)==0);
    database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    char lost[32],crash[32]; CHECK(snprintf(lost,sizeof lost,"lost-%d",stage)>0 && snprintf(crash,sizeof crash,"crash-%d-",stage)>0);
    int committed=0;
    if (strstr(scenario,lost)) {
        commit_fault=2; CHECK(ae_catalogue_handoff(*slot,p,stage,original,125000+stage,&out)==-EIO && memcmp(&out,&marker,sizeof out)==0); committed=1;
    } else if (strstr(scenario,crash)) {
        CHECK(ae_catalogue_close(slot)==0); pid_t child=fork(); CHECK(child>=0); int early=strstr(scenario,"before")!=NULL;
        if (!child) { struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(directory,ri,&fresh)==0); crash_mode=early ? 1 : 2;
            (void)ae_catalogue_handoff(fresh,p,stage,original,125000+stage,&out); _exit(90); }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(early ? 71 : 72));
        CHECK(ae_catalogue_open(directory,ri,slot)==0); committed=!early;
    }
    CHECK(ae_catalogue_handoff(*slot,p,stage,original,125000+stage,&out)==0);
    CHECK(out.transfer_required==(stage==AE_HANDOFF_PREPARE && !committed) && out.retire_required==(stage==AE_HANDOFF_RETIRE && !committed));
    CHECK(out.snapshot.revision==revision && out.snapshot.recorded_unix_ms==125001);
    database_digest(directory,before);
    CHECK(ae_catalogue_handoff(*slot,p,stage,original,999999,&out)==0 && !out.transfer_required && !out.retire_required && out.snapshot.revision==revision);
    database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
}
static int handoff_start(struct ae_catalogue **root,struct ae_catalogue *user,const struct ae_catalogue_identity *ri,const char *directory,
    const struct ae_execution_profile *p,const struct ae_execution_handoff *original,const char *scenario)
{
    struct ae_handoff_snapshot snapshot,marker; memset(&marker,0xa5,sizeof marker); snapshot=marker;
    CHECK(ae_catalogue_get_handoff(*root,p,&snapshot)==-EPROTONOSUPPORT && memcmp(&snapshot,&marker,sizeof marker)==0);
    CHECK(ae_catalogue_upgrade_handoffs(user)==-EPROTONOSUPPORT);
    uint8_t before[32],after[32]; database_digest(directory,before);
    for (int n=1;n<=6;++n) { ddl_fault=n; ddl_number=0; CHECK(ae_catalogue_upgrade_handoffs(*root)==-EIO && !ddl_fault);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0 && scalar(directory,"PRAGMA user_version")==4); }
    commit_fault=1; CHECK(ae_catalogue_upgrade_handoffs(*root)==-EIO && scalar(directory,"PRAGMA user_version")==4);
    commit_fault=2; CHECK(ae_catalogue_upgrade_handoffs(*root)==-EIO && scalar(directory,"PRAGMA user_version")==5);
    CHECK(ae_catalogue_close(root)==0 && ae_catalogue_open(directory,ri,root)==0);
    database_digest(directory,before); CHECK(ae_catalogue_upgrade_handoffs(*root)==0); database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    CHECK(ae_catalogue_get_handoff(*root,p,&snapshot)==-ENOENT && memcmp(&snapshot,&marker,sizeof marker)==0);
    struct ae_handoff_result result,untouched; memset(&untouched,0xa5,sizeof untouched); result=untouched;
    struct ae_execution_handoff wrong=*original; --wrong.process.pidfd_id;
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_PREPARE,&wrong,125001,&result)==-ESTALE && memcmp(&result,&untouched,sizeof result)==0);
    if (strstr(scenario,"overflow")) { edit(directory,"UPDATE authority_meta SET catalogue_revision=9223372036854775807");
        CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_PREPARE,original,125001,&result)==-EOVERFLOW); return 1; }
    handoff_action(root,ri,directory,p,original,AE_HANDOFF_PREPARE,scenario);
    ++wrong.root.inode;
    wrong.process=original->process;
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_PREPARE,&wrong,999999,&result)==-EEXIST);
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_OBSERVE,original,125002,&result)==-ESTALE);
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_RETIRE,original,125003,&result)==-EAGAIN);
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_RETIRED,original,125004,&result)==-EAGAIN);
    sqlite3 *db=database(directory);
    CHECK(sqlite3_exec(db,"UPDATE execution_handoffs SET witness_digest=zeroblob(32),revision=revision+1",NULL,NULL,NULL)==SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(db,"UPDATE execution_handoffs SET state='retired',revision=revision+1",NULL,NULL,NULL)==SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(db,"DELETE FROM execution_handoffs",NULL,NULL,NULL)==SQLITE_CONSTRAINT && sqlite3_close(db)==SQLITE_OK);
    if (strstr(scenario,"corrupt-handoff")) {
        uint8_t raw[AE_HANDOFF_MAX],hash[32]; size_t size=0;
        CHECK(ae_handoff_encode(original,raw,sizeof raw,&size)==0 && ae_sha256_bytes(raw,size,hash)==0 && ae_catalogue_close(root)==0);
        char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",directory)>0); int fd=open(path,O_RDWR|O_CLOEXEC|O_NOFOLLOW);
        struct stat st; CHECK(fd>=0 && fstat(fd,&st)==0 && st.st_size>32 && st.st_size<2*1024*1024);
        size=(size_t)st.st_size; uint8_t *bytes=malloc(size); CHECK(bytes && read(fd,bytes,size)==(ssize_t)size); size_t found=0,offset=0;
        for (size_t n=0;n+32<=size;++n) if (memcmp(bytes+n,hash,32)==0) { ++found; offset=n; }
        CHECK(found==1); bytes[offset]^=1U; CHECK(pwrite(fd,bytes+offset,1,(off_t)offset)==1 && fsync(fd)==0 && close(fd)==0); free(bytes);
        CHECK(ae_catalogue_open(directory,ri,root)==0); snapshot=marker;
        CHECK(ae_catalogue_get_handoff(*root,p,&snapshot)==-EUCLEAN && memcmp(&snapshot,&marker,sizeof snapshot)==0);
        result=untouched; CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_PREPARE,original,999999,&result)==-EUCLEAN && memcmp(&result,&untouched,sizeof result)==0); return 1;
    }
    if (strstr(scenario,"observe")) {
        uint8_t runtime[16]; id(runtime,6); CHECK(ae_catalogue_begin_runtime(*root,p->boot,runtime)==0);
        handoff_action(root,ri,directory,p,original,AE_HANDOFF_OBSERVE,scenario);
        id(runtime,7); CHECK(ae_catalogue_begin_runtime(*root,p->boot,runtime)==0);
        CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_OBSERVE,original,125010,&result)==0 && result.snapshot.revision==3 &&
            !result.transfer_required && !result.retire_required && memcmp(result.snapshot.observer_runtime,runtime,16)==0);
        CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_OBSERVE,original,999999,&result)==0 && result.snapshot.observed_unix_ms==125010 && result.snapshot.revision==3);
        CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_RETIRE,original,125011,&result)==-EAGAIN);
        CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_PREPARE,original,125011,&result)==-ESTALE);
        CHECK(scalar(directory,"SELECT state='uncertain' FROM launch_dispatches")==1 && scalar(directory,"SELECT state='running' FROM generation_leases")==1);
        CHECK(ae_catalogue_close(root)==0 && ae_catalogue_open(directory,ri,root)==0 && ae_catalogue_get_handoff(*root,p,&snapshot)==0 && snapshot.revision==3); return 1;
    }
    return 0;
}
static void handoff_finish(struct ae_catalogue **root,const struct ae_catalogue_identity *ri,const char *directory,
    const struct ae_execution_profile *p,const struct ae_execution_handoff *original,const char *scenario)
{
    /* Metadata-only mock removal evidence. Real PID1 absence is tested by the
     * separate component; no kernel/owner-recovery claim follows from this call. */
    handoff_action(root,ri,directory,p,original,AE_HANDOFF_RETIRE,scenario);
    handoff_action(root,ri,directory,p,original,AE_HANDOFF_RETIRED,scenario);
    struct ae_handoff_result result;
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_PREPARE,original,999999,&result)==0 && result.snapshot.state==AE_HANDOFF_FINISHED && !result.transfer_required && !result.retire_required);
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_OBSERVE,original,999999,&result)==-ESTALE);
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_RETIRE,original,999999,&result)==0 && !result.retire_required && result.snapshot.retiring_unix_ms==125003);
    sqlite3 *db=database(directory);
    CHECK(sqlite3_exec(db,"DELETE FROM execution_processes WHERE role=2",NULL,NULL,NULL)==SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(db,"DELETE FROM execution_handoffs",NULL,NULL,NULL)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);
}
/* Real SQLite owner transactions; process/handoff/end inputs below are synthetic
 * already-admitted fixtures, not evidence of an installed recovery coordinator. */
static void run_recovery_test(struct ae_catalogue **root,struct ae_catalogue *user,const struct ae_catalogue_identity *ri,
    const struct ae_execution_profile *p,const struct ae_launch_context *original,const struct ae_execution_evidence *e,
    const struct ae_execution_evidence *ue,const struct ae_execution_completion *started,const struct ae_execution_handoff *h,
    const char *rd,const char *ud,const char *scenario)
{
    CHECK(ae_catalogue_upgrade_handoffs(*root)==0 && ae_catalogue_upgrade_recovery(user)==-EPROTONOSUPPORT);
    uint8_t before[32],after[32]; database_digest(rd,before);
    for (int n=1;n<=7;++n) {
        ddl_fault=n; ddl_number=0;
        CHECK(ae_catalogue_upgrade_recovery(*root)==-EIO && !ddl_fault && scalar(rd,"PRAGMA user_version")==5);
        database_digest(rd,after); CHECK(memcmp(before,after,32)==0);
    }
    commit_fault=2; CHECK(ae_catalogue_upgrade_recovery(*root)==-EIO && scalar(rd,"PRAGMA user_version")==6);
    CHECK(ae_catalogue_close(root)==0 && ae_catalogue_open(rd,ri,root)==0);
    database_digest(rd,before); CHECK(ae_catalogue_upgrade_recovery(*root)==0); database_digest(rd,after); CHECK(memcmp(before,after,32)==0);
    struct ae_handoff_result handoff;
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_PREPARE,h,124150,&handoff)==0 && handoff.transfer_required);
    struct ae_run_receipt begin,quiet,released,out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    if (strstr(scenario,"existing-close")) CHECK(ae_catalogue_begin_run_close(*root,p,original,e,started,124180,&begin)==0);
    uint8_t runtime[16]; id(runtime,6); CHECK(ae_catalogue_begin_runtime(*root,p->boot,runtime)==0);
    struct ae_launch_context c=*original; memcpy(c.system_runtime,runtime,16);
    CHECK(ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,124200,&out)==-ESTALE && memcmp(&out,&marker,sizeof out)==0);
    if (strstr(scenario,"unobserved")) return;
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_OBSERVE,h,124190,&handoff)==0);
    CHECK(ae_catalogue_begin_run_close(*root,p,original,e,started,124200,&out)==-ESTALE);
    CHECK(ae_catalogue_begin_run_close(*root,p,&c,e,started,124200,&out)==-ESTALE);
    struct ae_launch_context wrong=c; id(wrong.broker_runtime,52);
    CHECK(ae_catalogue_recover_begin_run_close(*root,p,&wrong,e,started,124200,&out)==-ESTALE);
    wrong=c; id(wrong.boot,8); CHECK(ae_catalogue_recover_begin_run_close(*root,p,&wrong,e,started,124200,&out)==-ESTALE);
    if (strstr(scenario,"wrong-user") || strstr(scenario,"wrong-boot")) return;
    if (strstr(scenario,"missing-start")) {
        edit(rd,"DELETE FROM operation_steps WHERE step_no=6");
        CHECK(ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,124200,&out)==-ESTALE);
        CHECK(scalar(rd,"SELECT count(*) FROM run_recovery_owners")==0); return;
    }
    if (strstr(scenario,"overflow-owner")) {
        edit(rd,"UPDATE authority_meta SET catalogue_revision=9223372036854775807");
        CHECK(ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,124200,&out)==-EOVERFLOW);
        CHECK(scalar(rd,"SELECT count(*) FROM run_recovery_owners")==0); return;
    }
    int64_t root_revision=scalar(rd,"SELECT revision FROM operations"),user_revision=scalar(ud,"SELECT revision FROM operations");
    uint8_t end[32]; memset(end,0x65,32);
    for (int stage=0;stage<2;++stage) {
        if (stage) {
            CHECK(ae_catalogue_quiesce_user_run(user,p,ue,&begin,124210,&quiet)==0);
            if (strstr(scenario,"twice")) {
                id(runtime,7); CHECK(ae_catalogue_begin_runtime(*root,p->boot,runtime)==0); memcpy(c.system_runtime,runtime,16);
                CHECK(ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,124220,&out)==-ESTALE);
                CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_OBSERVE,h,124215,&handoff)==0);
                CHECK(ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,124220,&out)==-ENOENT);
                CHECK(ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,124216,&out)==0 && memcmp(&out,&begin,sizeof out)==0);
                CHECK(scalar(rd,"SELECT count(*) FROM run_recovery_owners")==2);
            }
            if (strstr(scenario,"overflow-release")) {
                edit(rd,"UPDATE generation_leases SET revision=9223372036854775807");
                CHECK(ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,124220,&out)==-EOVERFLOW);
                CHECK(scalar(rd,"SELECT state='running' FROM generation_leases")==1); return;
            }
        }
        database_digest(rd,before); out=marker;
        int writes=stage ? 4 : strstr(scenario,"existing-close") ? 2 : 5;
        for (int n=1;n<=writes;++n) {
            step_fault=n; step_number=0;
            int r=stage ? ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,124220,&out) :
                ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,124200,&out);
            CHECK(r==-EIO && !step_fault && memcmp(&out,&marker,sizeof out)==0);
            database_digest(rd,after); CHECK(memcmp(before,after,32)==0);
        }
        commit_fault=1;
        CHECK((stage ? ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,124220,&out) :
            ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,124200,&out))==-EIO);
        database_digest(rd,after); CHECK(memcmp(before,after,32)==0);
        int selected=strstr(scenario,stage ? "release" : "begin")!=NULL;
        if (selected && strstr(scenario,"crash")) {
            CHECK(ae_catalogue_close(root)==0); pid_t child=fork(); CHECK(child>=0); int early=strstr(scenario,"before")!=NULL;
            if (!child) { struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(rd,ri,&fresh)==0); crash_mode=early ? 1 : 2;
                if (stage) (void)ae_catalogue_recover_release_run(fresh,p,&c,e,&quiet,end,124220,&out);
                else (void)ae_catalogue_recover_begin_run_close(fresh,p,&c,e,started,124200,&out);
                _exit(90);
            }
            int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(early ? 71 : 72));
            CHECK(ae_catalogue_open(rd,ri,root)==0);
        } else if (selected && strstr(scenario,"lost")) {
            commit_fault=2;
            CHECK((stage ? ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,124220,&out) :
                ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,124200,&out))==-EIO && memcmp(&out,&marker,sizeof out)==0);
        }
        CHECK((stage ? ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,124220,&out) :
            ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,124200,&out))==0);
        if (stage) released=out; else begin=out;
        database_digest(rd,before);
        CHECK((stage ? ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,999999,&out) :
            ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,999999,&out))==0);
        database_digest(rd,after); CHECK(memcmp(before,after,32)==0);
        CHECK(ae_catalogue_get_recovered_run_receipt(*root,p,&c,e,stage ? AE_RUN_ROOT_RELEASED : AE_RUN_ROOT_CLOSING,&out)==0);
        CHECK(memcmp(&out,stage ? &released : &begin,sizeof out)==0);
        CHECK(scalar(rd,"SELECT revision FROM operations")==root_revision && scalar(ud,"SELECT revision FROM operations")==user_revision);
        CHECK(scalar(rd,"SELECT state='running' FROM generation_leases")==!stage);
    }
    struct ae_user_launch_snapshot launch;
    CHECK(ae_catalogue_end_user_run(user,p,ue,&released,124230,&launch)==0 && strcmp(launch.state,"exited")==0);
    CHECK(scalar(rd,"SELECT count(*) FROM admission_gates WHERE state='open'")==0);
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_RETIRE,h,124240,&handoff)==0 && handoff.retire_required);
    CHECK(ae_catalogue_handoff(*root,p,AE_HANDOFF_RETIRED,h,124241,&handoff)==0);
    database_digest(rd,before);
    CHECK(ae_catalogue_recover_begin_run_close(*root,p,&c,e,started,999999,&out)==0 && memcmp(&out,&begin,sizeof out)==0);
    CHECK(ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,999999,&out)==0 && memcmp(&out,&released,sizeof out)==0);
    database_digest(rd,after); CHECK(memcmp(before,after,32)==0);
    end[0]^=1; CHECK(ae_catalogue_recover_release_run(*root,p,&c,e,&quiet,end,999999,&out)==-EEXIST);
    CHECK(ae_catalogue_get_run_receipt(*root,p,e,AE_RUN_ROOT_RELEASED,&out)<0);
    CHECK(ae_catalogue_close(root)==0 && ae_catalogue_open(rd,ri,root)==0);
}
static void process_journal_test(struct ae_catalogue **root,struct ae_catalogue **user,const struct ae_catalogue_identity *ri,
    const struct ae_execution_profile *p,struct ae_launch_context *context,const struct ae_execution_evidence *e,const char *rd,const char *scenario)
{
    struct ae_catalogue_identity ui={.kind=AE_CATALOGUE_USER}; memcpy(ui.authority_epoch,p->broker_epoch,16); memcpy(ui.owner,p->account,16);
    size_t length=strlen(rd); char ud[4096]; CHECK(length>6 && strcmp(rd+length-6,"system")==0 && length<sizeof ud); memcpy(ud,rd,length-6); strcpy(ud+length-6,"user");
    struct ae_execution_process_snapshot out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    CHECK(ae_catalogue_get_execution_process(*root,p,AE_EXECUTION_STARTER,&out)==-EPROTONOSUPPORT && memcmp(&out,&marker,sizeof out)==0);
    CHECK(ae_catalogue_upgrade_processes(*root)==-EPROTONOSUPPORT);
    for (int owner=0;owner<2;++owner) {
        struct ae_catalogue **slot=owner ? user : root; const char *directory=owner ? ud : rd;
        CHECK(ae_catalogue_upgrade_lifetimes(*slot)==0); uint8_t before[32],after[32]; database_digest(directory,before);
        for (int n=1;n<=6;++n) { ddl_fault=n; ddl_number=0; CHECK(ae_catalogue_upgrade_processes(*slot)==-EIO && ddl_fault==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0 && scalar(directory,"PRAGMA user_version")==3); }
        commit_fault=1; CHECK(ae_catalogue_upgrade_processes(*slot)==-EIO && scalar(directory,"PRAGMA user_version")==3);
        commit_fault=2; CHECK(ae_catalogue_upgrade_processes(*slot)==-EIO && scalar(directory,"PRAGMA user_version")==4);
        CHECK(ae_catalogue_close(slot)==0 && ae_catalogue_open(directory,owner ? &ui : ri,slot)==0);
        database_digest(directory,before); CHECK(ae_catalogue_upgrade_processes(*slot)==0); database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
    }
    struct ae_user_execution_result accepted; struct ae_execution_result execution,untouched; memset(&untouched,0xa5,sizeof untouched);
    CHECK(ae_catalogue_prepare_user_execution(*user,p,&accepted)==0 && accepted.submit_required);
    CHECK(ae_catalogue_execution(*root,p,context,AE_EXECUTION_REQUEST,NULL,&execution)==0 && execution.effect_required);
    CHECK(ae_catalogue_execution(*root,p,context,AE_EXECUTION_BIND,e,&execution)==0);
    CHECK(ae_catalogue_execution(*root,p,context,AE_EXECUTION_PREPARE,e,&execution)==0); execution=untouched;
    CHECK(ae_catalogue_execution(*root,p,context,AE_EXECUTION_ARM,e,&execution)==-EAGAIN && memcmp(&execution,&untouched,sizeof execution)==0);
    struct ae_execution_process starter=process_record(p,e,AE_EXECUTION_STARTER),app=process_record(p,e,AE_EXECUTION_APP);
    if (strstr(scenario,"overflow-root")) { edit(rd,"UPDATE authority_meta SET catalogue_revision=9223372036854775807");
        CHECK(ae_catalogue_enroll_execution_process(*root,p,e,&starter,124000,&out)==-EOVERFLOW && memcmp(&out,&marker,sizeof out)==0); return; }
    process_enrollment(root,ri,rd,p,e,&starter,0,scenario);
    CHECK(ae_catalogue_execution(*root,p,context,AE_EXECUTION_ARM,e,&execution)==0 && execution.effect_required);
    uint8_t app_hash[32]; memset(app_hash,0x87,32);
    CHECK(ae_catalogue_observe_execution(*root,p,context,e,app_hash,&execution)==-EAGAIN);
    struct ae_execution_evidence ue=*e; memset(ue.context_digest,0x99,32);
    struct ae_user_launch_snapshot launch;
    CHECK(ae_catalogue_observe_user_execution(*user,p,AE_USER_EXECUTION_REGISTER,&ue,&launch)==0);
    CHECK(ae_catalogue_observe_user_execution(*user,p,AE_USER_EXECUTION_READY,&ue,&launch)==-EAGAIN);
    struct ae_execution_process user_app=process_record(p,&ue,AE_EXECUTION_APP);
    if (strstr(scenario,"overflow-user")) { edit(ud,"UPDATE authority_meta SET catalogue_revision=9223372036854775807");
        CHECK(ae_catalogue_enroll_execution_process(*user,p,&ue,&user_app,124001,&out)==-EOVERFLOW); return; }
    process_enrollment(user,&ui,ud,p,&ue,&user_app,1,scenario);
    struct ae_execution_process wrong=app; wrong.pidfd_id--;
    CHECK(ae_catalogue_enroll_execution_process(*root,p,e,&wrong,124002,&out)==-ESTALE);
    process_enrollment(root,ri,rd,p,e,&app,2,scenario);
    struct ae_execution_handoff handoff=handoff_record(&app); int with_handoff=strncmp(scenario,"exec-handoff-",13)==0;
    int with_transport=strncmp(scenario,"exec-bridge-store-",18)==0;
    if (with_transport) {
#ifdef AETHERI_TEST_EXECUTION_SERVICE
        void ae_test_handoff_transport_begin(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_execution_handoff *,const char *);
        ae_test_handoff_transport_begin(*root,p,&handoff,scenario);
#else
        CHECK(0 && "handoff transport requires AETHERI_LINUX_IPC");
#endif
    } else if (strncmp(scenario,"exec-bridge-",12)==0) {
#ifdef AETHERI_TEST_EXECUTION_SERVICE
        void ae_test_handoff_service(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_execution_handoff *,const char *);
        ae_test_handoff_service(*root,p,&handoff,scenario);
        CHECK(scalar(rd,"SELECT state='running' FROM generation_leases")==1 && scalar(rd,"SELECT state='uncertain' FROM launch_dispatches")==1); return;
#else
        CHECK(0 && "handoff bridge requires AETHERI_LINUX_IPC");
#endif
    }
    if (with_handoff && handoff_start(root,*user,ri,rd,p,&handoff,scenario)) return;
    CHECK(ae_catalogue_observe_user_execution(*user,p,AE_USER_EXECUTION_READY,&ue,&launch)==0);
    if (strstr(scenario,"replace")) {
        int is_user=strstr(scenario,"user-replace")!=NULL; struct ae_catalogue **slot=is_user ? user : root; const char *directory=is_user ? ud : rd;
        uint8_t runtime[16]; id(runtime,is_user ? 52 : 6); CHECK(ae_catalogue_begin_runtime(*slot,p->boot,runtime)==0);
        CHECK(ae_catalogue_get_execution_process(*slot,p,AE_EXECUTION_APP,&out)==0 && out.process.pidfd_id==UINT64_MAX);
        CHECK(ae_catalogue_enroll_execution_process(*slot,p,is_user ? &ue : e,is_user ? &user_app : &app,999999,&out)==-ESTALE);
        CHECK(scalar(rd,"SELECT state='running' FROM generation_leases")==1);
        CHECK(scalar(directory,is_user ? "SELECT state='recovery_required' FROM launches" : "SELECT state='uncertain' FROM launch_dispatches")==1);
        CHECK(ae_catalogue_close(slot)==0 && ae_catalogue_open(directory,is_user ? &ui : ri,slot)==0 && ae_catalogue_get_execution_process(*slot,p,AE_EXECUTION_APP,&out)==0); return;
    }
    if (strstr(scenario,"corrupt")) {
        uint8_t raw[AE_EXECUTION_PROCESS_MAX],hash[32]; size_t size=0; CHECK(ae_execution_process_encode(&app,raw,sizeof raw,&size)==0 && ae_sha256_bytes(raw,size,hash)==0);
        CHECK(ae_catalogue_close(root)==0); char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",rd)>0);
        int fd=open(path,O_RDWR|O_CLOEXEC|O_NOFOLLOW); struct stat st; CHECK(fd>=0 && fstat(fd,&st)==0 && st.st_size>32 && st.st_size<2*1024*1024);
        size=(size_t)st.st_size; uint8_t *bytes=malloc(size); CHECK(bytes && read(fd,bytes,size)==(ssize_t)size); size_t found=0,offset=0;
        for (size_t n=0;n+32<=size;++n) if (memcmp(bytes+n,hash,32)==0) { ++found; offset=n; }
        CHECK(found==1); bytes[offset]^=1U; CHECK(pwrite(fd,bytes+offset,1,(off_t)offset)==1 && fsync(fd)==0 && close(fd)==0); free(bytes);
        CHECK(ae_catalogue_open(rd,ri,root)==0); out=marker; CHECK(ae_catalogue_get_execution_process(*root,p,AE_EXECUTION_APP,&out)==-EUCLEAN && memcmp(&out,&marker,sizeof out)==0);
        CHECK(ae_catalogue_observe_execution(*root,p,context,e,app_hash,&execution)==-EUCLEAN); return;
    }
    struct ae_execution_diagnostic unknown={.kind=AE_EXECUTION_DIAGNOSTIC_UNKNOWN};
    CHECK(ae_catalogue_execution_report(*root,p,context,e,&unknown,&execution)==0);
    CHECK(ae_catalogue_observe_execution(*root,p,context,e,app_hash,&execution)==0);
    struct ae_execution_completion completed;
    CHECK(ae_catalogue_complete_execution(*root,p,context,e,app_hash,124100,&completed)==0);
    CHECK(ae_catalogue_finish_user_execution(*user,p,&ue,app_hash,&completed,124101,&launch)==0);
    if (strncmp(scenario,"exec-recover-",13)==0) {
        run_recovery_test(root,*user,ri,p,context,e,&ue,&completed,&handoff,rd,ud,scenario); return;
    }
    if (strstr(scenario,"gates")) { edit(rd,"UPDATE admission_gates SET state='closed',revision=revision+1"); edit(ud,"UPDATE admission_gates SET state='closed',revision=revision+1"); }
    struct ae_run_receipt closing,quiet,released; uint8_t ended[32]; memset(ended,0x65,32);
    CHECK(ae_catalogue_begin_run_close(*root,p,context,e,&completed,124200,&closing)==0);
    CHECK(ae_catalogue_quiesce_user_run(*user,p,&ue,&closing,124201,&quiet)==0);
    CHECK(ae_catalogue_release_run(*root,p,context,e,&quiet,ended,124202,&released)==0);
    CHECK(ae_catalogue_end_user_run(*user,p,&ue,&released,124203,&launch)==0);
    if (with_transport) {
#ifdef AETHERI_TEST_EXECUTION_SERVICE
        void ae_test_handoff_transport_finish(struct ae_catalogue *,const struct ae_execution_profile *,const char *);
        ae_test_handoff_transport_finish(*root,p,scenario);
#endif
        CHECK(scalar(rd,"SELECT state='released' FROM generation_leases")==1 && scalar(rd,"SELECT state='terminal' FROM launch_dispatches")==1); return;
    }
    if (with_handoff) handoff_finish(root,ri,rd,p,&handoff,scenario);
    CHECK(ae_catalogue_enroll_execution_process(*root,p,e,&app,999999,&out)==0 && out.recorded_unix_ms==124002);
    CHECK(ae_catalogue_enroll_execution_process(*user,p,&ue,&user_app,999999,&out)==0 && out.recorded_unix_ms==124001);
    CHECK(ae_catalogue_close(root)==0 && ae_catalogue_open(rd,ri,root)==0 && ae_catalogue_get_execution_process(*root,p,AE_EXECUTION_STARTER,&out)==0);
    CHECK(scalar(rd,"SELECT state='released' FROM generation_leases")==1 && scalar(ud,"SELECT state='exited' FROM launches")==1);
}
static void execution_test(struct ae_catalogue **root,struct ae_catalogue **user_slot,const struct ae_catalogue_identity *identity,
    const struct ae_user_launch *q,struct ae_launch_context *c,const char *directory,const char *expected,const char *scenario)
{
    struct ae_catalogue *user=*user_slot;
    struct ae_execution_profile p; execution_profile(&p,q,c); execution_codec(&p,expected);
    struct ae_execution_evidence e={0}; id(e.invocation,66); memset(e.context_digest,2,32);
    struct ae_execution_result result,marker; memset(&marker,0xa5,sizeof marker); result=marker;
    if (strncmp(scenario,"exec-process-",13)==0 || strncmp(scenario,"exec-handoff-",13)==0 || strncmp(scenario,"exec-bridge-",12)==0 ||
        strncmp(scenario,"exec-recover-",13)==0) { process_journal_test(root,user_slot,identity,&p,c,&e,directory,scenario); return; }
    CHECK(ae_catalogue_authorize_user_execution(user,&p)==0 && ae_catalogue_authorize_user_execution(*root,&p)==-EPERM);
    struct ae_execution_profile wrong=p; wrong.storage[15]++;
    CHECK(ae_catalogue_authorize_user_execution(user,&wrong)==-ESTALE);
    wrong=p; wrong.data_gate_revision++;
    CHECK(ae_catalogue_authorize_user_execution(user,&wrong)==-ESTALE);
    CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_BIND,&e,&result)==-ENOENT && memcmp(&result,&marker,sizeof marker)==0);
    CHECK(ae_catalogue_execution(*root,&p,c,(enum ae_execution_stage)0,&e,&result)==-EINVAL);
    CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_BIND,NULL,&result)==-EINVAL);
    int lost=strcmp(scenario,"exec-lost")==0;
    if (strncmp(scenario,"exec-completion-",16)==0 || strncmp(scenario,"exec-lifetime-",14)==0) { struct ae_user_execution_result prepared; CHECK(ae_catalogue_prepare_user_execution(user,&p,&prepared)==0 && prepared.submit_required); }
    if (strncmp(scenario,"exec-observation-",17)==0) {
        uint8_t app_hash[32]={0};
        CHECK(ae_catalogue_observe_execution(user,&p,c,&e,app_hash,&result)==-EPERM);
        CHECK(ae_catalogue_observe_execution(*root,&p,c,NULL,app_hash,&result)==-EINVAL);
        CHECK(ae_catalogue_observe_execution(*root,&p,c,&e,NULL,&result)==-EINVAL);
    }
    for (int stage=AE_EXECUTION_REQUEST;stage<=AE_EXECUTION_ARM;++stage) {
        if (strncmp(scenario,"exec-observation-",17)==0) {
            struct ae_execution_result untouched=marker; uint8_t app_hash[32]={0};
            CHECK(ae_catalogue_observe_execution(*root,&p,c,&e,app_hash,&untouched)==-ESTALE && memcmp(&untouched,&marker,sizeof marker)==0);
        }
        /* All mutating statements roll back atomically; both COMMIT failure
         * directions leave the outward result untouched. */
        int count=stage==AE_EXECUTION_PREPARE ? 4 : stage==AE_EXECUTION_ARM ? 5 : 3;
        uint8_t before[32],after[32]; database_digest(directory,before);
        for (int n=1;n<=count;++n) {
            step_fault=n; step_number=0; result=marker;
            CHECK(ae_catalogue_execution(*root,&p,c,(enum ae_execution_stage)stage,&e,&result)==-EIO && step_fault==0 && memcmp(&result,&marker,sizeof marker)==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        }
        commit_fault=1; result=marker;
        CHECK(ae_catalogue_execution(*root,&p,c,(enum ae_execution_stage)stage,&e,&result)==-EIO && memcmp(&result,&marker,sizeof marker)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        if (strstr(scenario,"crash")!=NULL && ((strstr(scenario,"request")!=NULL && stage==AE_EXECUTION_REQUEST) || (strstr(scenario,"arm")!=NULL && stage==AE_EXECUTION_ARM))) {
            CHECK(ae_catalogue_close(root)==0);
            pid_t child=fork(); CHECK(child>=0);
            if (child==0) {
                struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(directory,identity,&fresh)==0);
                crash_mode=strstr(scenario,"before") ? 1 : 2;
                (void)ae_catalogue_execution(fresh,&p,c,(enum ae_execution_stage)stage,&e,&result); _exit(90);
            }
            int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(strstr(scenario,"before") ? 71 : 72));
            CHECK(ae_catalogue_open(directory,identity,root)==0);
            int committed=strstr(scenario,"after")!=NULL;
            CHECK(scalar(directory,stage==AE_EXECUTION_REQUEST ? "SELECT count(*) FROM operation_steps WHERE step_no=1" : "SELECT count(*) FROM operation_steps WHERE step_no=3")==committed);
            CHECK(ae_catalogue_execution(*root,&p,c,(enum ae_execution_stage)stage,&e,&result)==0);
            CHECK(result.effect_required==!committed);
        } else if (lost) {
            commit_fault=2; result=marker;
            CHECK(ae_catalogue_execution(*root,&p,c,(enum ae_execution_stage)stage,&e,&result)==-EIO && memcmp(&result,&marker,sizeof marker)==0);
        } else {
            CHECK(ae_catalogue_execution(*root,&p,c,(enum ae_execution_stage)stage,&e,&result)==0);
            CHECK(result.effect_required==(stage==AE_EXECUTION_REQUEST || stage==AE_EXECUTION_ARM));
        }
        int64_t revision=scalar(directory,"SELECT catalogue_revision FROM authority_meta");
        for (int previous=AE_EXECUTION_REQUEST;previous<=stage;++previous) {
            CHECK(ae_catalogue_execution(*root,&p,c,(enum ae_execution_stage)previous,&e,&result)==0 && !result.effect_required);
            CHECK(scalar(directory,"SELECT catalogue_revision FROM authority_meta")==revision);
        }
        CHECK(strcmp(result.dispatch.state,stage==AE_EXECUTION_ARM ? "dispatching" : "reserved")==0);
        CHECK(result.dispatch.invocation_present==(stage==AE_EXECUTION_ARM));
        if (stage==AE_EXECUTION_ARM) CHECK(memcmp(result.dispatch.invocation,e.invocation,16)==0);
        struct ae_dispatch_snapshot snapshot;
        CHECK(ae_catalogue_cancel_reservation(*root,p.lease,c,p.broker_epoch,p.user_operation,1,123500,&snapshot)==-EAGAIN);
        /* Even a caller passing the execution hash to the old no-exec API
         * cannot turn this different journal into a no-exec drain proof. */
        struct ae_setup_profile old={.install_revision=2,.install_gate_revision=3,.data_gate_revision=3};
        memcpy(&old,&p,offsetof(struct ae_setup_profile,artifact)); memcpy(old.artifact,p.artifact,32); memcpy(old.image_digest,p.starter_digest,32);
        uint8_t digest[32]; CHECK(ae_execution_digest(&p,digest)==0);
        CHECK(ae_catalogue_begin_setup(*root,&old,digest,c,&snapshot)==-EEXIST);
        CHECK(ae_catalogue_withdraw_setup(*root,&old,digest,e.invocation,e.context_digest,c,123500,&snapshot)==-EEXIST);
        wrong=p; wrong.allocation_digest[0]^=1; result=marker;
        CHECK(ae_catalogue_execution(*root,&wrong,c,AE_EXECUTION_REQUEST,&e,&result)==-EEXIST && memcmp(&result,&marker,sizeof marker)==0);
        wrong=p; wrong.starter_digest[0]^=1;
        CHECK(ae_catalogue_execution(*root,&wrong,c,AE_EXECUTION_REQUEST,&e,&result)==-EEXIST);
        if (stage>=AE_EXECUTION_BIND) {
            struct ae_execution_evidence bad=e; bad.invocation[15]++;
            CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_BIND,&bad,&result)==-EEXIST);
            bad=e; bad.context_digest[0]^=1;
            CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_PREPARE,&bad,&result)==-EEXIST);
        }
        if (stage==AE_EXECUTION_REQUEST) CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_ARM,&e,&result)==-ENOENT);
        if (stage==AE_EXECUTION_BIND) CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_ARM,&e,&result)==-EAGAIN);
        char restart[48]; CHECK(snprintf(restart,sizeof restart,"exec-restart-%d",stage)>0);
        if (strcmp(scenario,restart)==0) {
            id(c->system_runtime,6); CHECK(ae_catalogue_begin_runtime(*root,p.boot,c->system_runtime)==0); result=marker;
            CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_ARM,&e,&result)==-ESTALE && memcmp(&result,&marker,sizeof marker)==0);
            CHECK(ae_catalogue_get_dispatch(*root,p.lease,c,&snapshot)==0 && strcmp(snapshot.state,"uncertain")==0);
            CHECK(scalar(directory,"SELECT state<>'released' FROM generation_leases")==1);
            wrong=p; memcpy(wrong.system_runtime,c->system_runtime,16);
            CHECK(ae_catalogue_execution(*root,&wrong,c,AE_EXECUTION_REQUEST,&e,&result)==-EAGAIN); return;
        }
        if (stage==AE_EXECUTION_PREPARE && strcmp(scenario,"exec-user-restart")==0) {
            uint8_t replacement[16]; id(replacement,52);
            CHECK(ae_catalogue_begin_runtime(user,p.boot,replacement)==0);
            CHECK(ae_catalogue_authorize_user_execution(user,&p)==-ESTALE);
            memcpy(c->broker_runtime,replacement,16); result=marker;
            CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_ARM,&e,&result)==-ESTALE && memcmp(&result,&marker,sizeof marker)==0);
            CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=3")==0); return;
        }
        if (stage==AE_EXECUTION_PREPARE && strcmp(scenario,"exec-closed-gate")==0) {
            edit(directory,"UPDATE admission_gates SET state='closed',revision=revision+1"); result=marker;
            CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_ARM,&e,&result)==-ESTALE && memcmp(&result,&marker,sizeof marker)==0);
            CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=3")==0); return;
        }
        if (stage==AE_EXECUTION_PREPARE && strstr(scenario,"overflow")) {
            edit(directory,strstr(scenario,"lease") ? "UPDATE generation_leases SET revision=9223372036854775807" :
                strstr(scenario,"dispatch") ? "UPDATE launch_dispatches SET revision=9223372036854775807" :
                strstr(scenario,"operation") ? "UPDATE operations SET revision=9223372036854775807" :
                "UPDATE authority_meta SET catalogue_revision=9223372036854775807"); result=marker;
            CHECK(ae_catalogue_execution(*root,&p,c,AE_EXECUTION_ARM,&e,&result)==-EOVERFLOW && memcmp(&result,&marker,sizeof marker)==0);
            CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=3")==0); return;
        }
    }
    CHECK(scalar(directory,"SELECT state='running' AND outcome IS NULL FROM operations")==1);
    CHECK(scalar(directory,"SELECT state='running' FROM generation_leases")==1);
    CHECK(scalar(directory,"SELECT ready_data_gate_revision=3 FROM launch_dispatches")==1);
    if (strncmp(scenario,"exec-completion-",16)==0 || strncmp(scenario,"exec-lifetime-",14)==0) {
        struct ae_catalogue_identity ui={.kind=AE_CATALOGUE_USER}; memcpy(ui.authority_epoch,p.broker_epoch,16); memcpy(ui.owner,p.account,16);
        size_t length=strlen(directory); char ud[4096]; CHECK(length>6 && strcmp(directory+length-6,"system")==0 && length<sizeof ud); memcpy(ud,directory,length-6); strcpy(ud+length-6,"user");
        int lifetime=strncmp(scenario,"exec-lifetime-",14)==0;
        completion_test(root,user_slot,identity,&ui,&p,c,&e,directory,ud,lifetime ? "exec-completion-fresh" : scenario);
        if (lifetime) lifetime_test(root,user_slot,identity,&ui,&p,c,&e,directory,ud,scenario);
        return;
    }
    if (strncmp(scenario,"exec-observation-",17)==0) {
        root_observe_test(root,identity,&p,c,&e,directory,scenario); return;
    }
    if (strstr(scenario,"exec-diagnostic-")) {
        struct ae_execution_diagnostic d={.kind=AE_EXECUTION_DIAGNOSTIC_FAILURE,.phase=8,.error=1};
        if (strstr(scenario,"unknown")) d=(struct ae_execution_diagnostic){.kind=AE_EXECUTION_DIAGNOSTIC_UNKNOWN};
        if (strstr(scenario,"malformed")) d=(struct ae_execution_diagnostic){.kind=AE_EXECUTION_DIAGNOSTIC_MALFORMED};
        if (strstr(scenario,"observer")) d=(struct ae_execution_diagnostic){.kind=AE_EXECUTION_DIAGNOSTIC_OBSERVER,.error=5};
        struct ae_execution_result receipt,sentinel; memset(&sentinel,0xa5,sizeof sentinel); receipt=sentinel;
        uint8_t before[32],after[32]; database_digest(directory,before);
        for (int n=1;n<=3;++n) {
            step_fault=n; step_number=0;
            CHECK(ae_catalogue_execution_report(*root,&p,c,&e,&d,&receipt)==-EIO && step_fault==0 && memcmp(&receipt,&sentinel,sizeof receipt)==0);
            database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        }
        commit_fault=1; CHECK(ae_catalogue_execution_report(*root,&p,c,&e,&d,&receipt)==-EIO && memcmp(&receipt,&sentinel,sizeof receipt)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        commit_fault=2; CHECK(ae_catalogue_execution_report(*root,&p,c,&e,&d,&receipt)==-EIO && memcmp(&receipt,&sentinel,sizeof receipt)==0);
        CHECK(ae_catalogue_execution_report(*root,&p,c,&e,&d,&receipt)==0 && !receipt.effect_required && strcmp(receipt.dispatch.state,"dispatching")==0);
        database_digest(directory,before);
        CHECK(ae_catalogue_execution_report(*root,&p,c,&e,&d,&receipt)==0 && !receipt.effect_required);
        struct ae_execution_diagnostic different={.kind=d.kind==AE_EXECUTION_DIAGNOSTIC_UNKNOWN ? AE_EXECUTION_DIAGNOSTIC_MALFORMED : AE_EXECUTION_DIAGNOSTIC_UNKNOWN};
        receipt=sentinel;
        CHECK(ae_catalogue_execution_report(*root,&p,c,&e,&different,&receipt)==-EEXIST && memcmp(&receipt,&sentinel,sizeof receipt)==0);
        database_digest(directory,after); CHECK(memcmp(before,after,32)==0);
        CHECK(scalar(directory,"SELECT state='running' AND outcome IS NULL AND reason_code LIKE 'exec_%' FROM operations")==1);
        CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=4 AND action='verify' AND subject_kind='integration' AND state='applied'")==1);
        id(c->system_runtime,6); CHECK(ae_catalogue_begin_runtime(*root,p.boot,c->system_runtime)==0);
        CHECK(ae_catalogue_execution_report(*root,&p,c,&e,&d,&receipt)==-ESTALE);
        CHECK(scalar(directory,"SELECT state='uncertain' FROM launch_dispatches")==1 && scalar(directory,"SELECT state='running' FROM generation_leases")==1);
        CHECK(scalar(directory,"SELECT reason_code LIKE 'exec_%' FROM operations")==1 && scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no=4 AND state='applied'")==1);
    }
}
int main(int argc,char **argv)
{
    if (argc==3 && strcmp(argv[1],"handoff-codec")==0) {
        uint8_t data[2981],raw[AE_HANDOFF_MAX],hash[32]; FILE *file=fopen(argv[2],"rb"); CHECK(file);
        size_t total=fread(data,1,sizeof data,file); CHECK(!ferror(file) && feof(file) && fclose(file)==0 && total>=388+374+32);
        size_t size=0; for (size_t n=0;n<4;++n) size=(size<<8)|data[384+n]; CHECK(size<=AE_HANDOFF_MAX && total==388+size+32);
        struct ae_execution_profile p; CHECK(ae_execution_decode(data,384,&p)==0);
        struct ae_execution_handoff v,marker; memset(&marker,0xa5,sizeof marker); size_t written=0;
        CHECK(ae_handoff_decode(data+388,size,&v)==0 && v.process.pidfd_id==UINT64_MAX && v.root.mount_id==UINT64_MAX-4 && v.group.inode==(UINT64_C(1)<<63)+31);
        CHECK(ae_handoff_encode(&v,raw,sizeof raw,&written)==0 && written==size && memcmp(raw,data+388,size)==0 && ae_sha256_bytes(raw,size,hash)==0 && memcmp(hash,data+388+size,32)==0);
        CHECK(ae_handoff_match(&p,&v)==0);
        for (size_t n=0;n<size;++n) { v=marker; CHECK(ae_handoff_decode(data+388,n,&v)==-EBADMSG && memcmp(&v,&marker,sizeof v)==0); }
        v=marker; CHECK(ae_handoff_decode(data+388,size+1,&v)==-EBADMSG && memcmp(&v,&marker,sizeof v)==0);
        const size_t offsets[]={0,24,26,97,184,257};
        for (size_t n=0;n<6;++n) { memcpy(raw,data+388,size); raw[offsets[n]]=255; v=marker;
            CHECK(ae_handoff_decode(raw,size,&v)==-EBADMSG && memcmp(&v,&marker,sizeof v)==0); }
        CHECK(ae_handoff_decode(data+388,size,&v)==0); memset(raw,0xa5,sizeof raw); written=999;
        CHECK(ae_handoff_encode(&v,raw,size-1,&written)==-ENOBUFS && written==999); for (size_t n=0;n<sizeof raw;++n) CHECK(raw[n]==0xa5);
        const int64_t times[]={-1,INT64_MIN,INT64_MAX};
        for (size_t n=0;n<3;++n) { v.group.birth_seconds=times[n]; struct ae_execution_handoff decoded;
            CHECK(ae_handoff_encode(&v,raw,sizeof raw,&written)==0 && ae_handoff_decode(raw,written,&decoded)==0 && decoded.group.birth_seconds==times[n]); }
        v.process.uid=0; CHECK(ae_handoff_match(&p,&v)==-ESTALE); return 0;
    }
    if (argc==3 && strcmp(argv[1],"process-codec")==0) {
        uint8_t data[2469],raw[AE_EXECUTION_PROCESS_MAX],hash[32]; FILE *file=fopen(argv[2],"rb"); CHECK(file);
        size_t total=fread(data,1,sizeof data,file); CHECK(!ferror(file) && feof(file) && fclose(file)==0 && total>=388+149+32);
        size_t size=0; for (size_t n=0;n<4;++n) size=(size<<8)|data[384+n]; CHECK(size<=AE_EXECUTION_PROCESS_MAX && total==388+size+32);
        struct ae_execution_profile p; CHECK(ae_execution_decode(data,384,&p)==0);
        struct ae_execution_process v,marker; memset(&marker,0xa5,sizeof marker); size_t written=0;
        CHECK(ae_execution_process_decode(data+388,size,&v)==0 && v.pidfd_id==UINT64_MAX && v.cgroup_id==(UINT64_C(1)<<63)+31);
        CHECK(ae_execution_process_encode(&v,raw,sizeof raw,&written)==0 && written==size && memcmp(raw,data+388,size)==0 && ae_sha256_bytes(raw,size,hash)==0 && memcmp(hash,data+388+size,32)==0);
        struct ae_execution_evidence evidence={0}; memcpy(evidence.invocation,v.invocation,16); memcpy(evidence.context_digest,v.evidence_digest,32);
        CHECK(ae_execution_process_match(&p,&evidence,&v)==0);
        for (size_t n=0;n<size;++n) { v=marker; CHECK(ae_execution_process_decode(data+388,n,&v)==-EBADMSG && memcmp(&v,&marker,sizeof v)==0); }
        v=marker; CHECK(ae_execution_process_decode(data+388,size+1,&v)==-EBADMSG && memcmp(&v,&marker,sizeof v)==0);
        const size_t offsets[]={0,40,41,44,140};
        for (size_t n=0;n<5;++n) { memcpy(raw,data+388,size); raw[offsets[n]]=0xff; v=marker; CHECK(ae_execution_process_decode(raw,size,&v)==-EBADMSG && memcmp(&v,&marker,sizeof v)==0); }
        CHECK(ae_execution_process_decode(data+388,size,&v)==0); memset(raw,0xa5,sizeof raw); written=999;
        CHECK(ae_execution_process_encode(&v,raw,size-1,&written)==-ENOBUFS && written==999); for (size_t n=0;n<sizeof raw;++n) CHECK(raw[n]==0xa5);
        v.uid=1; CHECK(ae_execution_process_match(&p,&evidence,&v)==-ESTALE); v.uid=0; v.boot[0]^=1;
        CHECK(ae_execution_process_match(&p,&evidence,&v)==-ESTALE); return 0;
    }
    if (argc==3 && strcmp(argv[1],"run-codec")==0) {
        uint8_t data[920],bytes[137],hash[32]; FILE *file=fopen(argv[2],"rb"); CHECK(file && fread(data,1,sizeof data,file)==sizeof data && fgetc(file)==EOF && fclose(file)==0);
        struct ae_execution_profile p; CHECK(ae_execution_decode(data,384,&p)==0);
        struct ae_execution_evidence e={0}; id(e.invocation,66); memset(e.context_digest,2,32);
        for (int kind=1;kind<=3;++kind) {
            const uint8_t *raw=data+384+(size_t)(kind-1)*168;
            struct ae_run_receipt value,marker; memset(&marker,0xa5,sizeof marker);
            CHECK(ae_run_receipt_decode(raw,136,&value)==0 && ae_run_receipt_match(&p,&e,(enum ae_run_receipt_kind)kind,&value)==0);
            CHECK(ae_run_receipt_encode(&value,bytes)==0 && memcmp(raw,bytes,136)==0 && ae_run_receipt_digest(&value,hash)==0 && memcmp(hash,raw+136,32)==0);
            for (size_t n=0;n<136;++n) { value=marker; CHECK(ae_run_receipt_decode(raw,n,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0); }
            memcpy(bytes,raw,136); bytes[136]=0; value=marker; CHECK(ae_run_receipt_decode(bytes,137,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0);
            const size_t offsets[]={0,120,128};
            for (size_t n=0;n<3;++n) { memcpy(bytes,raw,136); bytes[offsets[n]]=0xff; value=marker; CHECK(ae_run_receipt_decode(bytes,136,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0); }
            memcpy(bytes,raw,136); memset(bytes+40,0,16); CHECK(ae_run_receipt_decode(bytes,136,&value)==-EBADMSG);
            memcpy(bytes,raw,136); memset(bytes+120,0,8); CHECK(ae_run_receipt_decode(bytes,136,&value)==-EBADMSG);
        }
        struct ae_run_end end={.cgroup_id=(UINT64_C(1)<<63)+17,.pid=1234,.group_removed=1}; memcpy(end.boot,p.boot,16); memcpy(end.invocation,e.invocation,16);
        CHECK(ae_run_end_digest(&p,&e,&end,hash)==0 && memcmp(hash,data+888,32)==0);
        memset(hash,0xa5,32); end.pid=0; CHECK(ae_run_end_digest(&p,&e,&end,hash)==-EBADMSG && hash[0]==0xa5); end.pid=1234;
        end.boot[0]^=1; CHECK(ae_run_end_digest(&p,&e,&end,hash)==-ESTALE && hash[0]==0xa5); return 0;
    }
    if (argc==3 && strcmp(argv[1],"completion-codec")==0) {
        uint8_t bytes[AE_EXECUTION_COMPLETION_BYTES+1],encoded[AE_EXECUTION_COMPLETION_BYTES]; FILE *file=fopen(argv[2],"rb");
        CHECK(file && fread(bytes,1,sizeof bytes,file)==AE_EXECUTION_COMPLETION_BYTES && fclose(file)==0);
        struct ae_execution_completion value,marker; memset(&marker,0xa5,sizeof marker);
        CHECK(ae_execution_completion_decode(bytes,AE_EXECUTION_COMPLETION_BYTES,&value)==0 && value.revision==17 && value.completed_unix_ms==INT64_C(1793400000123));
        CHECK(ae_execution_completion_encode(&value,encoded)==0 && memcmp(bytes,encoded,sizeof encoded)==0);
        for (size_t n=0;n<AE_EXECUTION_COMPLETION_BYTES;++n) { value=marker; CHECK(ae_execution_completion_decode(bytes,n,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0); }
        value=marker; CHECK(ae_execution_completion_decode(bytes,sizeof bytes,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0);
        const size_t offsets[]={0,14,184,192};
        for (size_t n=0;n<sizeof offsets/sizeof *offsets;++n) { uint8_t saved=bytes[offsets[n]]; bytes[offsets[n]]=0xff; value=marker; CHECK(ae_execution_completion_decode(bytes,AE_EXECUTION_COMPLETION_BYTES,&value)==-EBADMSG && memcmp(&value,&marker,sizeof value)==0); bytes[offsets[n]]=saved; }
        return 0;
    }
    CHECK(argc==5);
    struct ae_catalogue_identity ri={.kind=AE_CATALOGUE_SYSTEM},ui={.kind=AE_CATALOGUE_USER};
    id(ri.authority_epoch,1); id(ui.authority_epoch,50); id(ui.owner,2);
    struct ae_user_launch q={.local={.accepted_unix_ms=123456,.expires_boottime_ns=INT64_MAX},.data_revision=1,.data_gate_revision=2};
    struct ae_launch_request *r=&q.reservation;
    id(r->system_epoch,1); id(r->lease,32); id(r->launch,31); id(r->broker_epoch,50); id(r->broker_runtime,51);
    id(r->account,2); id(r->install,10); id(r->generation,14); id(r->data,12); id(r->boot,4);
    int execution=strncmp(argv[4],"exec-",5)==0;
    r->install_revision=2; strcpy(r->action,execution ? "run" : "main"); strcpy(r->session,"p0"); strcpy(r->seat,"none");
    id(q.local.plan,60); id(q.local.operation,61); id(q.actor,62); id(q.request_key,63); id(q.storage,13);
    struct ae_launch_context rc={.uid=1000}; id(rc.system_runtime,5); memcpy(rc.broker_epoch,r->broker_epoch,16);
    memcpy(rc.broker_runtime,r->broker_runtime,16); memcpy(rc.account,r->account,16); memcpy(rc.boot,r->boot,16);
    struct ae_catalogue *root=NULL,*user=NULL;
    CHECK(ae_catalogue_create(argv[1],&ri,r->boot,&root)==0 && ae_catalogue_enroll_account(root,r->account,1000)==0 && ae_catalogue_close(&root)==0);
    edit(argv[1],"INSERT INTO components VALUES('aetheri.probe',X'00000000000040008000000000000009','app','Probe',0,1); INSERT INTO artifacts VALUES(zeroblob(32),'aetheri.probe','test','x86_64-linux-gnu','verified'); INSERT INTO installs(install_id,component_id,scope_id,state) VALUES(X'0000000000004000800000000000000a','aetheri.probe',X'00000000000040008000000000000002','preparing'); INSERT INTO generations VALUES(X'0000000000004000800000000000000e',X'0000000000004000800000000000000a','aetheri.probe',zeroblob(32),'committed',0,1); UPDATE installs SET state='active',active_generation_id=X'0000000000004000800000000000000e',revision=2;");
    CHECK(ae_catalogue_open(argv[1],&ri,&root)==0 && ae_catalogue_begin_runtime(root,r->boot,rc.system_runtime)==0);
    int64_t gate;
    CHECK(ae_catalogue_admit_install(root,r->install,r->generation,2,2,r->boot,rc.system_runtime,&gate)==0);
    CHECK(ae_catalogue_resolve_app(root,r->account,1000,r->install,r->boot,rc.system_runtime,&q.source)==0);
    CHECK(ae_catalogue_create(argv[2],&ui,r->boot,&user)==0 && ae_catalogue_close(&user)==0);
    edit(argv[2],"INSERT INTO install_bindings VALUES(X'0000000000004000800000000000000a','aetheri.probe',X'00000000000040008000000000000009',X'00000000000040008000000000000002',X'00000000000040008000000000000001',2,'active'); INSERT INTO data_epochs(data_epoch_id,component_id,publisher_ref,owner_scope,storage_ref,state,data_schema_version) VALUES(X'0000000000004000800000000000000c','aetheri.probe',X'00000000000040008000000000000009',X'00000000000040008000000000000002',X'0000000000004000800000000000000d','active',0); INSERT INTO attachments SELECT install_id,data_epoch_id,b.component_id,b.publisher_ref,b.owner_scope FROM install_bindings b,data_epochs;");
    CHECK(ae_catalogue_open(argv[2],&ui,&user)==0 && ae_catalogue_begin_runtime(user,r->boot,r->broker_runtime)==0);
    struct ae_user_launch_snapshot output,marker; memset(&marker,0xa5,sizeof marker); output=marker;
    CHECK(ae_catalogue_accept_launch(user,&q,&output)==-ESTALE && memcmp(&output,&marker,sizeof marker)==0);
    for (int n=1;n<=2;++n) {
        step_fault=n; step_number=0; gate=-777;
        CHECK(ae_catalogue_admit_launch_data(user,&q,&gate)==-EIO && step_fault==0 && gate==-777);
        CHECK(scalar(argv[2],"SELECT state='closed' AND revision=2 FROM admission_gates")==1);
    }
    commit_fault=1; gate=-777;
    CHECK(ae_catalogue_admit_launch_data(user,&q,&gate)==-EIO && gate==-777);
    commit_fault=2;
    CHECK(ae_catalogue_admit_launch_data(user,&q,&gate)==-EIO && gate==-777);
    CHECK(ae_catalogue_admit_launch_data(user,&q,&gate)==0 && gate==3 && ae_catalogue_admit_launch_data(user,&q,&gate)==0);
    q.data_gate_revision=3;
    int setup=strncmp(argv[4],"setup",5)==0;
    int recovery=strcmp(argv[4],"recovery")==0 || strcmp(argv[4],"lost-root-reply")==0;
    int lost_reply=strcmp(argv[4],"lost-root-reply")==0;
    if (strcmp(argv[4],"faults")==0) {
        for (int n=1;n<=10;++n) {
            step_fault=n; step_number=0; output=marker;
            CHECK(ae_catalogue_accept_launch(user,&q,&output)==-EIO && step_fault==0 && memcmp(&output,&marker,sizeof marker)==0);
            CHECK(scalar(argv[2],"SELECT count(*) FROM operations")==0 && scalar(argv[2],"SELECT count(*) FROM launches")==0);
        }
        commit_fault=1; CHECK(ae_catalogue_accept_launch(user,&q,&output)==-EIO);
        commit_fault=2; CHECK(ae_catalogue_accept_launch(user,&q,&output)==-EIO);
    } else if (!recovery && !setup && !execution) {
        CHECK(ae_catalogue_close(&user)==0);
        pid_t child=fork(); CHECK(child>=0);
        if (child==0) {
            struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(argv[2],&ui,&fresh)==0);
            crash_mode=strcmp(argv[4],"exit-before")==0 ? 1 : 2;
            (void)ae_catalogue_accept_launch(fresh,&q,&output); _exit(90);
        }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(strcmp(argv[4],"exit-before")==0 ? 71 : 72));
        CHECK(ae_catalogue_open(argv[2],&ui,&user)==0);
        CHECK(scalar(argv[2],"SELECT count(*) FROM launches")== (strcmp(argv[4],"exit-before")==0 ? 0 : 1));
    }
    CHECK(ae_catalogue_accept_launch(user,&q,&output)==0 && strcmp(output.state,"requested")==0 && !output.root_link_present);
    CHECK(scalar(argv[1],"SELECT count(*) FROM generation_leases")==0);
    CHECK(ae_catalogue_get_user_launch(user,q.local.operation,&output)==0 && output.lease[15]==32);
    struct ae_user_launch changed=q; changed.reservation.session[0]='x';
    CHECK(ae_catalogue_accept_launch(user,&changed,&output)==-EEXIST);
    sqlite3 *db=database(argv[2]); sqlite3_stmt *statement=NULL;
    CHECK(sqlite3_prepare_v2(db,"SELECT intent_digest FROM operations",-1,&statement,NULL)==SQLITE_OK && sqlite3_step(statement)==SQLITE_ROW);
    FILE *expected=fopen(argv[3],"rb"); uint8_t digest[32]; CHECK(expected!=NULL && fread(digest,1,32,expected)==32 && fclose(expected)==0);
    CHECK(sqlite3_column_bytes(statement,0)==32 && memcmp(digest,sqlite3_column_blob(statement,0),32)==0 && sqlite3_finalize(statement)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);
    struct ae_reservation_ids proposed={.accepted_unix_ms=123480,.expires_boottime_ns=INT64_MAX}; id(proposed.plan,40); id(proposed.operation,41);
    struct ae_dispatch_snapshot dispatch;
    CHECK(ae_catalogue_reserve_launch(root,r,&rc,&proposed,&dispatch)==0);
    struct ae_reservation_receipt receipt;
    CHECK(ae_catalogue_get_receipt(root,r->lease,&rc,&receipt)==0 && !receipt.withdrawn && receipt.plan[15]==40);
    if (!lost_reply) {
        for (int n=1;n<=5;++n) {
            step_fault=n; step_number=0; output=marker;
            CHECK(ae_catalogue_ack_launch(user,&q,&receipt,&output)==-EIO && step_fault==0 && memcmp(&output,&marker,sizeof marker)==0);
            CHECK(scalar(argv[2],"SELECT count(*) FROM operation_links")==0);
        }
        commit_fault=2; CHECK(ae_catalogue_ack_launch(user,&q,&receipt,&output)==-EIO);
        CHECK(ae_catalogue_ack_launch(user,&q,&receipt,&output)==0 && strcmp(output.state,"preparing")==0 && output.revision==2 && output.root_operation[15]==41);
        struct ae_reservation_receipt wrong=receipt; wrong.plan[15]++;
        CHECK(ae_catalogue_ack_launch(user,&q,&wrong,&output)==-EEXIST);
    }
    if (recovery) {
        uint8_t replacement[16]; id(replacement,52);
        CHECK(ae_catalogue_begin_runtime(user,r->boot,replacement)==0);
        CHECK(ae_catalogue_get_user_launch(user,q.local.operation,&output)==0 && strcmp(output.state,"recovery_required")==0);
        CHECK(scalar(argv[1],"SELECT state='reserved' FROM generation_leases")==1);
        CHECK(ae_catalogue_accept_launch(user,&q,&output)==0 && strcmp(output.state,"recovery_required")==0);
        memcpy(rc.broker_runtime,replacement,16);
    }
    if (execution) {
        if (strncmp(argv[4],"exec-user-observe-",18)==0) {
            user_observe_test(root,&user,&ui,&q,&rc,argv[2],argv[4]);
        } else if (strncmp(argv[4],"exec-user-prepare-",18)==0) {
            user_execution_test(root,&user,&ui,&q,&rc,argv[2],argv[4]);
        } else
#ifdef AETHERI_TEST_EXECUTION_SERVICE
        if (strncmp(argv[4],"exec-wire-",10)==0) {
            struct ae_execution_profile p; execution_profile(&p,&q,&rc);
            ae_test_execution_wire(root,&p,&rc,argv[1],argv[4]);
        } else if (strncmp(argv[4],"exec-service-",13)==0) {
            struct ae_execution_profile p; execution_profile(&p,&q,&rc);
            ae_test_execution_service(root,&p,&rc,argv[1],argv[4]);
        } else
#endif
            execution_test(&root,&user,&ri,&q,&rc,argv[1],argv[3],argv[4]);
        CHECK(ae_catalogue_close(&root)==0 && ae_catalogue_close(&user)==0);
        puts("Execution journal: distinct purpose, exact run/fences, atomic intent/binding/preparation/arm, no replayed effects and retained obligations passed."); return 0;
    }
    if (setup) {
        struct ae_setup_profile p={.install_revision=2,.install_gate_revision=3,.data_gate_revision=3};
        memcpy(p.system_epoch,r->system_epoch,16); memcpy(p.system_runtime,rc.system_runtime,16); memcpy(p.broker_epoch,r->broker_epoch,16);
        memcpy(p.broker_runtime,r->broker_runtime,16); memcpy(p.boot,r->boot,16); memcpy(p.account,r->account,16); memcpy(p.install,r->install,16);
        memcpy(p.generation,r->generation,16); memcpy(p.data,r->data,16); memcpy(p.storage,q.storage,16); memcpy(p.launch,r->launch,16);
        memcpy(p.lease,r->lease,16); memcpy(p.root_operation,proposed.operation,16); memcpy(p.user_operation,q.local.operation,16);
        uint8_t hash[32]={1},evidence[32]={2},invocation[16]; id(invocation,66);
        struct ae_dispatch_snapshot before; memset(&before,0xa5,sizeof before);
        CHECK(ae_catalogue_authorize_user_setup(user,&p)==0);
        struct ae_setup_profile wrong_profile=p; wrong_profile.storage[15]++;
        CHECK(ae_catalogue_authorize_user_setup(user,&wrong_profile)==-ESTALE);
        CHECK(ae_catalogue_register_setup(root,&p,hash,invocation,evidence,&dispatch)==-ENOENT);
        for (int stage=0;stage<4;++stage) {
            if (stage==2 && strcmp(argv[4],"setup-terminal")==0) continue;
            if (stage==3 && (strcmp(argv[4],"setup-dispatch-overflow")==0 || strcmp(argv[4],"setup-lease-overflow")==0)) {
                edit(argv[1],strcmp(argv[4],"setup-dispatch-overflow")==0 ? "UPDATE launch_dispatches SET revision=9223372036854775807" :
                    "UPDATE generation_leases SET revision=9223372036854775807");
                dispatch=before;
                CHECK(ae_catalogue_withdraw_setup(root,&p,hash,invocation,evidence,&rc,123500,&dispatch)==-EOVERFLOW);
                CHECK(memcmp(&dispatch,&before,sizeof before)==0 && scalar(argv[1],"SELECT state='reserved' FROM generation_leases")==1);
                CHECK(scalar(argv[1],"SELECT count(*) FROM operation_steps WHERE step_no=3")==0);
                CHECK(ae_catalogue_close(&root)==0 && ae_catalogue_close(&user)==0); return 0;
            }
            int count=stage==0 || stage==1 ? 3 : stage==2 ? 4 : 7;
            for (int n=1;n<=count;++n) {
                step_fault=n; step_number=0; dispatch=before;
                int result=stage==0 ? ae_catalogue_begin_setup(root,&p,hash,&rc,&dispatch) : stage==1 ?
                    (strcmp(argv[4],"setup-terminal")==0 ? ae_catalogue_record_setup_exit(root,&p,hash,invocation,evidence,&dispatch) : ae_catalogue_register_setup(root,&p,hash,invocation,evidence,&dispatch)) :
                    stage==2 ? ae_catalogue_prepared_setup(root,&p,hash,invocation,evidence,&dispatch) : ae_catalogue_withdraw_setup(root,&p,hash,invocation,evidence,&rc,123500,&dispatch);
                CHECK(result==-EIO && step_fault==0 && memcmp(&dispatch,&before,sizeof before)==0);
            }
            for (int repeat=0;repeat<3;++repeat) {
                commit_fault=repeat==0 ? 2 : 0;
                int result=stage==0 ? ae_catalogue_begin_setup(root,&p,hash,&rc,&dispatch) : stage==1 ?
                    (strcmp(argv[4],"setup-terminal")==0 ? ae_catalogue_record_setup_exit(root,&p,hash,invocation,evidence,&dispatch) : ae_catalogue_register_setup(root,&p,hash,invocation,evidence,&dispatch)) :
                    stage==2 ? ae_catalogue_prepared_setup(root,&p,hash,invocation,evidence,&dispatch) : ae_catalogue_withdraw_setup(root,&p,hash,invocation,evidence,&rc,123500+repeat,&dispatch);
                CHECK(result==(repeat==0 ? -EIO : 0));
            }
            if (stage<3) CHECK(ae_catalogue_cancel_reservation(root,r->lease,&rc,r->broker_epoch,q.local.operation,1,123500,&dispatch)==-EAGAIN);
            if (stage==1 && strcmp(argv[4],"setup-terminal")==0) CHECK(scalar(argv[1],"SELECT reason_code='setup_exit_observed' FROM operations")==1);
            uint8_t bad_hash[32]; memcpy(bad_hash,hash,32); bad_hash[0]^=1;
            CHECK(ae_catalogue_begin_setup(root,&p,bad_hash,&rc,&dispatch)==-EEXIST);
            if (stage>=1) {
                uint8_t bad_invocation[16]; memcpy(bad_invocation,invocation,16); bad_invocation[15]++;
                CHECK(ae_catalogue_register_setup(root,&p,hash,bad_invocation,evidence,&dispatch)==-EEXIST);
                uint8_t bad_evidence[32]; memcpy(bad_evidence,evidence,32); bad_evidence[0]^=1;
                CHECK(ae_catalogue_prepared_setup(root,&p,hash,invocation,bad_evidence,&dispatch)==-EEXIST);
            }
            if (stage==2 && strcmp(argv[4],"setup-restart")==0) {
                id(rc.system_runtime,6); CHECK(ae_catalogue_begin_runtime(root,r->boot,rc.system_runtime)==0);
                CHECK(ae_catalogue_withdraw_setup(root,&p,hash,invocation,evidence,&rc,123500,&dispatch)==-ESTALE);
                CHECK(ae_catalogue_get_dispatch(root,r->lease,&rc,&dispatch)==0 && strcmp(dispatch.state,"uncertain")==0);
                CHECK(scalar(argv[1],"SELECT state='reserved' FROM generation_leases")==1);
                CHECK(ae_catalogue_close(&root)==0 && ae_catalogue_close(&user)==0); return 0;
            }
        }
    }
    CHECK(ae_catalogue_cancel_reservation(root,r->lease,&rc,r->broker_epoch,q.local.operation,1,123500,&dispatch)==0);
    CHECK(ae_catalogue_get_receipt(root,r->lease,&rc,&receipt)==0 && receipt.withdrawn && receipt.cause[15]==61);
    struct ae_reservation_receipt wrong=receipt; wrong.cause[15]++;
    CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&wrong,123600,&output)==-EACCES);
    wrong=receipt; wrong.fingerprint[0]^=1;
    CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&wrong,123600,&output)==-ESTALE);
    edit(argv[2],"UPDATE launches SET readiness='ready'");
    CHECK(ae_catalogue_get_user_launch(user,q.local.operation,&output)==0 && !output.pre_dispatch_withdrawable);
    CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&receipt,123600,&output)==-EAGAIN);
    edit(argv[2],"UPDATE launches SET readiness='unknown',invocation_ref=X'0000000000004000800000000000005a'");
    CHECK(ae_catalogue_get_user_launch(user,q.local.operation,&output)==0 && !output.pre_dispatch_withdrawable);
    CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&receipt,123600,&output)==-EAGAIN);
    edit(argv[2],"UPDATE launches SET invocation_ref=NULL; INSERT INTO operation_steps SELECT operation_id,1,'verify','launch',launch_id,'executing',zeroblob(32) FROM launches");
    CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&receipt,123600,&output)==-EAGAIN);
    edit(argv[2],"DELETE FROM operation_steps WHERE step_no=1");
    CHECK(ae_catalogue_get_user_launch(user,q.local.operation,&output)==0 && output.pre_dispatch_withdrawable);
    for (int n=1;n<=7;++n) {
        step_fault=n; step_number=0; output=marker;
        CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&receipt,123600,&output)==-EIO && step_fault==0 && memcmp(&output,&marker,sizeof marker)==0);
        CHECK(scalar(argv[2],recovery ? "SELECT state='recovery_required' FROM launches" : "SELECT state='preparing' FROM launches")==1);
    }
    commit_fault=2;
    CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&receipt,123600,&output)==-EIO);
    CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&receipt,123601,&output)==0 && strcmp(output.state,"start_failed")==0 && output.revision==(recovery && !lost_reply ? 4 : 3));
    CHECK(ae_catalogue_accept_launch(user,&q,&output)==0 && strcmp(output.state,"start_failed")==0);
    CHECK(ae_catalogue_ack_launch(user,&q,&receipt,&output)==0 && strcmp(output.state,"start_failed")==0);
    CHECK(scalar(argv[2],recovery ? "SELECT state='closed' AND revision=4 FROM admission_gates" : "SELECT state='open' AND revision=3 FROM admission_gates")==1);
    CHECK(scalar(argv[2],"SELECT completed_unix_ms=123600 AND outcome='cancelled' FROM operations")==1);
    CHECK(ae_catalogue_finish_withdrawn_operation(user,q.local.operation,&receipt,123700,&output)==0);
    CHECK(ae_catalogue_close(&root)==0 && ae_catalogue_close(&user)==0);
    puts("User launch: canonical intent, durable pre-RPC correlation, root receipt, all SQL/commit faults and terminal no-dispatch cancellation passed.");
    return 0;
}
