#include "catalogue/launch.h"
#include "catalogue/setup.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"user launch line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int commit_fault,step_fault,step_number,crash_mode;
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
int main(int argc,char **argv)
{
    CHECK(argc==5);
    struct ae_catalogue_identity ri={.kind=AE_CATALOGUE_SYSTEM},ui={.kind=AE_CATALOGUE_USER};
    id(ri.authority_epoch,1); id(ui.authority_epoch,50); id(ui.owner,2);
    struct ae_user_launch q={.local={.accepted_unix_ms=123456,.expires_boottime_ns=INT64_MAX},.data_revision=1,.data_gate_revision=2};
    struct ae_launch_request *r=&q.reservation;
    id(r->system_epoch,1); id(r->lease,32); id(r->launch,31); id(r->broker_epoch,50); id(r->broker_runtime,51);
    id(r->account,2); id(r->install,10); id(r->generation,14); id(r->data,12); id(r->boot,4);
    r->install_revision=2; strcpy(r->action,"main"); strcpy(r->session,"p0"); strcpy(r->seat,"none");
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
    } else if (!recovery && !setup) {
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
