#include "catalogue/reservation.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"reservation line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int commit_fault,step_fault,step_number,crash_mode,cancel_fault;
int __real_sqlite3_exec(sqlite3 *,const char *,int (*)(void *,int,char **,char **),void *,char **);
int __wrap_sqlite3_exec(sqlite3 *db,const char *sql,int (*callback)(void *,int,char **,char **),void *context,char **error)
{
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
    const char *sql=sqlite3_sql(statement);
    if (cancel_fault && sqlite3_stmt_readonly(statement)==0 && strncmp(sql,"UPDATE ",7)==0 && ++step_number==cancel_fault) {
        cancel_fault=0; return SQLITE_IOERR;
    }
    if (step_fault && sqlite3_stmt_readonly(statement)==0 && strncmp(sql,"WITH r(se,lease",15)==0 && ++step_number==step_fault) {
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
