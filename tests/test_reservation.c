#include "catalogue/reservation.h"
#include "catalogue/setup.h"
#include "catalogue/effect.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"reservation line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int commit_fault,step_fault,step_number,crash_mode,cancel_fault;
static struct ae_catalogue *fence_store;
static const char *fence_query;
static int fence_commit;
static void fence(void)
{
    struct ae_execution_fence_status status;
    CHECK(fence_store && ae_catalogue_fence_execution(fence_store,&status)==0 && status.fenced && !status.active);
}
int __real_sqlite3_exec(sqlite3 *,const char *,int (*)(void *,int,char **,char **),void *,char **);
int __wrap_sqlite3_exec(sqlite3 *db,const char *sql,int (*callback)(void *,int,char **,char **),void *context,char **error)
{
    if (strcmp(sql,"COMMIT")==0 && (commit_fault || crash_mode)) {
        int mode=commit_fault; commit_fault=0;
        if (mode==2 || crash_mode==2) {
            CHECK(__real_sqlite3_exec(db,sql,callback,context,error)==SQLITE_OK);
            if (fence_commit) { fence_commit=0; fence(); }
        }
        if (crash_mode) _exit(70+crash_mode);
        return SQLITE_IOERR;
    }
    return __real_sqlite3_exec(db,sql,callback,context,error);
}
int __real_sqlite3_step(sqlite3_stmt *);
int __wrap_sqlite3_step(sqlite3_stmt *statement)
{
    const char *sql=sqlite3_sql(statement);
    if (cancel_fault && sqlite3_stmt_readonly(statement)==0 && strncmp(sql,"UPDATE ",7)==0 && ++step_number==cancel_fault) {
        cancel_fault=0; return SQLITE_IOERR;
    }
    if (step_fault && sqlite3_stmt_readonly(statement)==0 && strncmp(sql,"WITH r(se,lease",15)==0 && ++step_number==step_fault) {
        step_fault=0; return SQLITE_IOERR;
    }
    int r=__real_sqlite3_step(statement);
    if (fence_query && sqlite3_stmt_readonly(statement) && r==SQLITE_ROW && strstr(sql,fence_query)) {
        fence_query=NULL; fence();
    }
    return r;
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
static void admission_fence(struct ae_catalogue *store,const struct ae_launch_request *request,
    const struct ae_launch_context *context,const struct ae_reservation_ids *ids,const char *directory,const char *mode)
{
    fence_store=store;
    struct ae_dispatch_snapshot out,marker; memset(&marker,0xa5,sizeof marker); out=marker;
    struct ae_catalogue_info before,after;
    int64_t gate=-777;
    if (strcmp(mode,"gate-before")==0 || strcmp(mode,"gate-observe")==0) {
        CHECK(ae_catalogue_get_info(store,&before)==0);
        if (strcmp(mode,"gate-before")==0) fence(); else fence_query="SELECT f.state,f.revision FROM installs";
        CHECK(ae_catalogue_admit_install(store,request->install,request->generation,2,2,request->boot,context->system_runtime,&gate)==-ESHUTDOWN && gate==-777 && !fence_query);
        CHECK(scalar(directory,"SELECT state='closed' AND revision=2 FROM admission_gates")==1);
        CHECK(ae_catalogue_get_info(store,&after)==0 && after.revision==before.revision);
        CHECK(scalar(directory,"SELECT count(*) FROM generation_leases")==0);
        goto done;
    }
    int gate_lost=strcmp(mode,"gate-lost")==0;
    if (gate_lost) { commit_fault=2; fence_commit=1; }
    int result=ae_catalogue_admit_install(store,request->install,request->generation,2,2,request->boot,context->system_runtime,&gate);
    CHECK(gate_lost ? result==-EIO && gate==-777 && !fence_commit : result==0 && gate==3);
    if (gate_lost || strcmp(mode,"gate-existing")==0) {
        if (!gate_lost) fence();
        CHECK(ae_catalogue_get_info(store,&before)==0);
        CHECK(ae_catalogue_admit_install(store,request->install,request->generation,2,2,request->boot,context->system_runtime,&gate)==0 && gate==3);
        CHECK(ae_catalogue_get_info(store,&after)==0 && before.revision==after.revision);
        CHECK(ae_catalogue_reserve_launch(store,request,context,ids,&out)==-ESHUTDOWN && memcmp(&out,&marker,sizeof out)==0);
        CHECK(scalar(directory,"SELECT count(*) FROM plans")==0);
        goto done;
    }
    if (strcmp(mode,"reserve-before")==0 || strcmp(mode,"reserve-observe")==0) {
        CHECK(ae_catalogue_get_info(store,&before)==0);
        if (strcmp(mode,"reserve-before")==0) fence(); else fence_query="SELECT c.component_id,g.artifact_id,g.revision,f.revision";
        CHECK(ae_catalogue_reserve_launch(store,request,context,ids,&out)==-ESHUTDOWN && memcmp(&out,&marker,sizeof out)==0 && !fence_query);
        CHECK(ae_catalogue_get_info(store,&after)==0 && before.revision==after.revision);
        const char *tables[]={"plans","plan_reads","plan_effects","operations","operation_steps","generation_leases","launch_dispatches"};
        for (size_t n=0;n<sizeof tables/sizeof tables[0];++n) { char sql[96]; CHECK(snprintf(sql,sizeof sql,"SELECT count(*) FROM %s",tables[n])>0); CHECK(scalar(directory,sql)==0); }
        CHECK(scalar(directory,"SELECT state='open' AND revision=3 FROM admission_gates")==1);
        goto done;
    }
    int reserve_lost=strcmp(mode,"reserve-lost")==0;
    if (reserve_lost) { commit_fault=2; fence_commit=1; }
    result=ae_catalogue_reserve_launch(store,request,context,ids,&out);
    CHECK(reserve_lost ? result==-EIO && memcmp(&out,&marker,sizeof out)==0 && !fence_commit : result==0);
    if (reserve_lost || strcmp(mode,"reserve-existing")==0) {
        if (!reserve_lost) fence();
        CHECK(ae_catalogue_get_info(store,&before)==0);
        struct ae_reservation_ids unused=*ids; id(unused.plan,44); id(unused.operation,45); unused.accepted_unix_ms=999; unused.expires_boottime_ns=0;
        CHECK(ae_catalogue_reserve_launch(store,request,context,&unused,&out)==0 && out.operation[15]==41 && out.revision==1);
        CHECK(ae_catalogue_get_info(store,&after)==0 && before.revision==after.revision);
        CHECK(scalar(directory,"SELECT accepted_unix_ms=123456 AND state='running' FROM operations")==1);
        struct ae_launch_request changed=*request; strcpy(changed.session,"changed"); out=marker;
        CHECK(ae_catalogue_reserve_launch(store,&changed,context,&unused,&out)==-EEXIST && memcmp(&out,&marker,sizeof out)==0);
        changed=*request; id(changed.lease,33); id(changed.launch,34);
        CHECK(ae_catalogue_reserve_launch(store,&changed,context,&unused,&out)==-ESHUTDOWN && memcmp(&out,&marker,sizeof out)==0);
        uint8_t cause[16]; id(cause,60);
        commit_fault=2;
        CHECK(ae_catalogue_cancel_reservation(store,request->lease,context,request->broker_epoch,cause,1,999,&out)==-EIO && memcmp(&out,&marker,sizeof out)==0);
        CHECK(ae_catalogue_cancel_reservation(store,request->lease,context,request->broker_epoch,cause,1,1000,&out)==0 && strcmp(out.state,"terminal")==0);
        struct ae_reservation_receipt receipt;
        CHECK(ae_catalogue_get_receipt(store,request->lease,context,&receipt)==0 && receipt.withdrawn && receipt.cause[15]==60);
        CHECK(scalar(directory,"SELECT state='released' FROM generation_leases")==1 && scalar(directory,"SELECT completed_unix_ms=999 FROM operations")==1);
        CHECK(ae_catalogue_reserve_launch(store,request,context,ids,&out)==-EAGAIN);
        goto done;
    }
    struct ae_setup_profile p={.install_revision=2,.install_gate_revision=3,.data_gate_revision=3};
    memcpy(p.system_epoch,request->system_epoch,16); memcpy(p.system_runtime,context->system_runtime,16);
    memcpy(p.broker_epoch,request->broker_epoch,16); memcpy(p.broker_runtime,request->broker_runtime,16);
    memcpy(p.boot,request->boot,16); memcpy(p.account,request->account,16); memcpy(p.install,request->install,16);
    memcpy(p.generation,request->generation,16); memcpy(p.data,request->data,16); id(p.storage,13);
    memcpy(p.launch,request->launch,16); memcpy(p.lease,request->lease,16); memcpy(p.root_operation,ids->operation,16); id(p.user_operation,61);
    uint8_t hash[32]={1},evidence[32]={2},invocation[16]; id(invocation,66);
    if (strcmp(mode,"setup-before")==0 || strcmp(mode,"setup-observe")==0) {
        CHECK(ae_catalogue_get_info(store,&before)==0);
        if (strcmp(mode,"setup-before")==0) fence(); else fence_query="SELECT count(*) FROM operation_steps,p WHERE operation_id=p.op AND step_no=1";
        out=marker;
        CHECK(ae_catalogue_begin_setup(store,&p,hash,context,&out)==-ESHUTDOWN && memcmp(&out,&marker,sizeof out)==0 && !fence_query);
        CHECK(ae_catalogue_get_info(store,&after)==0 && before.revision==after.revision);
        CHECK(scalar(directory,"SELECT count(*) FROM operation_steps WHERE step_no>0")==0 && scalar(directory,"SELECT state='reserved' FROM generation_leases")==1);
        uint8_t cause[16]; id(cause,61);
        CHECK(ae_catalogue_cancel_reservation(store,request->lease,context,request->broker_epoch,cause,1,1000,&out)==0);
        goto done;
    }
    CHECK(strcmp(mode,"setup-existing")==0 || strcmp(mode,"setup-lost")==0);
    int setup_lost=strcmp(mode,"setup-lost")==0;
    if (setup_lost) { commit_fault=2; fence_commit=1; }
    out=marker; result=ae_catalogue_begin_setup(store,&p,hash,context,&out);
    CHECK(setup_lost ? result==-EIO && memcmp(&out,&marker,sizeof out)==0 && !fence_commit : result==0);
    if (!setup_lost) fence();
    CHECK(ae_catalogue_get_info(store,&before)==0);
    CHECK(ae_catalogue_begin_setup(store,&p,hash,context,&out)==0);
    CHECK(ae_catalogue_get_info(store,&after)==0 && before.revision==after.revision);
    uint8_t bad[32]; memcpy(bad,hash,32); bad[0]^=1;
    CHECK(ae_catalogue_begin_setup(store,&p,bad,context,&out)==-EEXIST);
    /* Synthetic native metadata only: actual peer/no-exec/drain admission remains
     * the authority's obligation. Fencing cannot fabricate that proof. */
    CHECK((setup_lost ? ae_catalogue_record_setup_exit(store,&p,hash,invocation,evidence,&out) :
        ae_catalogue_register_setup(store,&p,hash,invocation,evidence,&out))==0);
    if (!setup_lost) CHECK(ae_catalogue_prepared_setup(store,&p,hash,invocation,evidence,&out)==0);
    CHECK(ae_catalogue_withdraw_setup(store,&p,hash,invocation,evidence,context,1000,&out)==0 && strcmp(out.state,"terminal")==0);
    CHECK(ae_catalogue_withdraw_setup(store,&p,hash,invocation,evidence,context,2000,&out)==0);
    CHECK(scalar(directory,"SELECT completed_unix_ms=1000 FROM operations")==1 && scalar(directory,"SELECT state='released' FROM generation_leases")==1);
done:
    CHECK(!fence_query && !fence_commit);
    struct ae_execution_fence_status status;
    CHECK(ae_catalogue_execution_fence_status(store,&status)==0 && status.fenced && !status.active);
    printf("Admission fence: %s; actual native metadata, immutable history and permitted proven cleanup; no process/supervisor admission.\n",mode);
    fence_store=NULL;
}
int main(int argc,char **argv)
{
    CHECK(argc==4);
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM}; id(identity.authority_epoch,1);
    struct ae_launch_request r={0};
    id(r.system_epoch,1); id(r.lease,32); id(r.launch,31); id(r.broker_epoch,50); id(r.broker_runtime,51);
    id(r.account,2); id(r.install,10); id(r.generation,14); id(r.data,12); id(r.boot,4);
    r.install_revision=2; strcpy(r.action,"main"); strcpy(r.session,"p0"); strcpy(r.seat,"none");
    struct ae_launch_context context={.uid=1000}; id(context.system_runtime,5);
    memcpy(context.broker_epoch,r.broker_epoch,16); memcpy(context.broker_runtime,r.broker_runtime,16);
    memcpy(context.account,r.account,16); memcpy(context.boot,r.boot,16);
    struct ae_reservation_ids proposed={.accepted_unix_ms=123456,.expires_boottime_ns=INT64_MAX}; id(proposed.plan,40); id(proposed.operation,41);
    struct ae_catalogue *store=NULL;
    CHECK(ae_catalogue_create(argv[1],&identity,r.boot,&store)==0 && ae_catalogue_enroll_account(store,r.account,1000)==0 && ae_catalogue_close(&store)==0);
    edit(argv[1],"INSERT INTO components VALUES('aetheri.probe',X'00000000000040008000000000000009','app','Probe',0,1);"
        "INSERT INTO artifacts VALUES(zeroblob(32),'aetheri.probe','test','x86_64-linux-gnu','verified');"
        "INSERT INTO installs(install_id,component_id,scope_id,state) VALUES(X'0000000000004000800000000000000a','aetheri.probe',X'00000000000040008000000000000002','preparing');"
        "INSERT INTO generations VALUES(X'0000000000004000800000000000000e',X'0000000000004000800000000000000a','aetheri.probe',zeroblob(32),'committed',0,1);"
        "UPDATE installs SET state='active',active_generation_id=X'0000000000004000800000000000000e',revision=2;");
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0 && ae_catalogue_begin_runtime(store,r.boot,context.system_runtime)==0);
    struct ae_dispatch_snapshot output,marker; memset(&marker,0xa5,sizeof marker); output=marker;
    CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==-ESTALE && memcmp(&output,&marker,sizeof marker)==0);
    CHECK(scalar(argv[1],"SELECT count(*) FROM operations")==0);
    if (strncmp(argv[3],"fence-",6)==0) {
        admission_fence(store,&r,&context,&proposed,argv[1],argv[3]+6);
        CHECK(ae_catalogue_close(&store)==0); return 0;
    }
    int64_t gate=-777;
    commit_fault=1;
    CHECK(ae_catalogue_admit_install(store,r.install,r.generation,2,2,r.boot,context.system_runtime,&gate)==-EIO && gate==-777);
    CHECK(scalar(argv[1],"SELECT state='closed' AND revision=2 FROM admission_gates")==1);
    CHECK(ae_catalogue_admit_install(store,r.install,r.generation,2,2,r.boot,context.system_runtime,&gate)==0 && gate==3);
    CHECK(ae_catalogue_admit_install(store,r.install,r.generation,2,2,r.boot,context.system_runtime,&gate)==0 && gate==3);
    if (strcmp(argv[3],"faults")==0) {
        for (int n=1;n<=10;++n) {
            step_fault=n; step_number=0; output=marker;
            CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==-EIO && step_fault==0 && memcmp(&output,&marker,sizeof marker)==0);
            CHECK(scalar(argv[1],"SELECT count(*) FROM operations")==0 && scalar(argv[1],"SELECT count(*) FROM generation_leases")==0 && scalar(argv[1],"SELECT count(*) FROM plans")==0);
        }
        commit_fault=1;
        CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==-EIO);
        commit_fault=2;
        CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==-EIO && memcmp(&output,&marker,sizeof marker)==0);
    } else {
        CHECK(ae_catalogue_close(&store)==0);
        pid_t child=fork(); CHECK(child>=0);
        if (child==0) {
            struct ae_catalogue *fresh=NULL; CHECK(ae_catalogue_open(argv[1],&identity,&fresh)==0);
            crash_mode=strcmp(argv[3],"exit-before")==0 ? 1 : 2;
            (void)ae_catalogue_reserve_launch(fresh,&r,&context,&proposed,&output); _exit(90);
        }
        int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==(strcmp(argv[3],"exit-before")==0 ? 71 : 72));
        CHECK(ae_catalogue_open(argv[1],&identity,&store)==0);
        CHECK(scalar(argv[1],"SELECT count(*) FROM launch_dispatches")== (strcmp(argv[3],"exit-before")==0 ? 0 : 1));
    }
    CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==0 && output.operation[15]==41 && output.revision==1 && output.install_gate_revision==3);
    CHECK(strcmp(output.state,"reserved")==0 && strcmp(output.unit,"aetheri-app-0000000000004000800000000000001f.service")==0 && !output.invocation_present);
    CHECK(ae_catalogue_close(&store)==0);
    FILE *expected=fopen(argv[2],"rb"); uint8_t expected_digest[64]; CHECK(expected!=NULL && fread(expected_digest,1,64,expected)==64 && fclose(expected)==0);
    sqlite3 *db=database(argv[1]); sqlite3_stmt *statement=NULL;
    CHECK(sqlite3_prepare_v2(db,"SELECT intent_digest,evidence_digest FROM operations JOIN operation_steps USING(operation_id) WHERE step_no=0",-1,&statement,NULL)==SQLITE_OK && sqlite3_step(statement)==SQLITE_ROW);
    CHECK(sqlite3_column_bytes(statement,0)==32 && sqlite3_column_bytes(statement,1)==32 && memcmp(sqlite3_column_blob(statement,0),expected_digest,32)==0 && memcmp(sqlite3_column_blob(statement,1),expected_digest+32,32)==0);
    CHECK(sqlite3_finalize(statement)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0);
    proposed.plan[15]=42; proposed.operation[15]=43; proposed.accepted_unix_ms=999; proposed.expires_boottime_ns=0;
    CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==0 && output.operation[15]==41);
    CHECK(scalar(argv[1],"SELECT count(*) FROM generation_leases")==1 && scalar(argv[1],"SELECT catalogue_revision FROM authority_meta")==5);
    struct ae_launch_request changed=r;
    strcpy(changed.session,"different"); output=marker;
    CHECK(ae_catalogue_reserve_launch(store,&changed,&context,&proposed,&output)==-EEXIST && memcmp(&output,&marker,sizeof marker)==0);
    changed=r; changed.install_revision++;
    CHECK(ae_catalogue_reserve_launch(store,&changed,&context,&proposed,&output)==-EEXIST);
    struct ae_launch_context wrong=context; wrong.uid=1001;
    CHECK(ae_catalogue_get_dispatch(store,r.lease,&wrong,&output)<0);
    wrong=context; wrong.broker_epoch[15]++;
    CHECK(ae_catalogue_get_dispatch(store,r.lease,&wrong,&output)==-ENOENT);
    CHECK(ae_catalogue_get_dispatch(store,r.lease,&context,&output)==0);
    uint8_t cause[16]; id(cause,60);
    CHECK(ae_catalogue_cancel_reservation(store,r.lease,&context,r.broker_epoch,cause,2,999,&output)==-ESTALE);
    CHECK(ae_catalogue_close(&store)==0);
    edit(argv[1],"UPDATE admission_gates SET state='closed',revision=revision+1; UPDATE authority_meta SET catalogue_revision=catalogue_revision+1");
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0);
    CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==0 && output.install_gate_revision==3);
    CHECK(scalar(argv[1],"SELECT state='closed' AND revision=4 FROM admission_gates")==1);
    CHECK(ae_catalogue_admit_install(store,r.install,r.generation,2,4,r.boot,context.system_runtime,&gate)==-ESTALE);
    for (int n=1;n<=4;++n) {
        cancel_fault=n; step_number=0; output=marker;
        CHECK(ae_catalogue_cancel_reservation(store,r.lease,&context,r.broker_epoch,cause,1,999,&output)==-EIO && cancel_fault==0 && memcmp(&output,&marker,sizeof marker)==0);
        CHECK(scalar(argv[1],"SELECT state='reserved' FROM generation_leases")==1 && scalar(argv[1],"SELECT state='reserved' FROM launch_dispatches")==1);
    }
    commit_fault=2;
    CHECK(ae_catalogue_cancel_reservation(store,r.lease,&context,r.broker_epoch,cause,1,999,&output)==-EIO);
    CHECK(ae_catalogue_cancel_reservation(store,r.lease,&context,r.broker_epoch,cause,1,1000,&output)==0 && strcmp(output.state,"terminal")==0 && output.revision==2);
    CHECK(scalar(argv[1],"SELECT completed_unix_ms=999 AND outcome='cancelled' FROM operations")==1);
    CHECK(scalar(argv[1],"SELECT state='released' AND revision=2 FROM generation_leases")==1);
    CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==-EAGAIN);
    cause[15]++;
    CHECK(ae_catalogue_cancel_reservation(store,r.lease,&context,r.broker_epoch,cause,2,1001,&output)==-EEXIST);
    CHECK(ae_catalogue_admit_install(store,r.install,r.generation,2,4,r.boot,context.system_runtime,&gate)==0 && gate==5);
    id(r.lease,33); id(r.launch,34); id(proposed.plan,44); id(proposed.operation,45); proposed.expires_boottime_ns=INT64_MAX;
    CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==0);
    id(context.system_runtime,6);
    CHECK(ae_catalogue_begin_runtime(store,r.boot,context.system_runtime)==0);
    output=marker;
    CHECK(ae_catalogue_reserve_launch(store,&r,&context,&proposed,&output)==-EAGAIN && memcmp(&output,&marker,sizeof marker)==0);
    CHECK(ae_catalogue_cancel_reservation(store,r.lease,&context,r.broker_epoch,cause,2,1002,&output)==-EAGAIN);
    CHECK(ae_catalogue_get_dispatch(store,r.lease,&context,&output)==0 && strcmp(output.state,"uncertain")==0);
    CHECK(scalar(argv[1],"SELECT count(*) FROM generation_leases WHERE state<>'released'")==1);
    CHECK(ae_catalogue_close(&store)==0);
    puts("Reservation: canonical intent, atomic lease/dispatch, all statement/commit faults, stable IDs, scoped reads and exact withdrawal passed.");
    return 0;
}
