#include "catalogue/projection.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"projection line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int commit_fault;
int __real_sqlite3_exec(sqlite3 *,const char *,int (*)(void *,int,char **,char **),void *,char **);
int __wrap_sqlite3_exec(sqlite3 *db,const char *sql,int (*callback)(void *,int,char **,char **),void *context,char **error)
{
    if (commit_fault && strcmp(sql,"COMMIT")==0) {
        int mode=commit_fault; commit_fault=0;
        if (mode==2) CHECK(__real_sqlite3_exec(db,sql,callback,context,error)==SQLITE_OK);
        return SQLITE_IOERR;
    }
    return __real_sqlite3_exec(db,sql,callback,context,error);
}
static void id(uint8_t bytes[16],uint8_t last)
{
    memset(bytes,0,16); bytes[6]=0x40; bytes[8]=0x80; bytes[15]=last;
}
static void edit(const char *directory,const char *sql)
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",directory)>0);
    sqlite3 *db=NULL;
    CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK);
    CHECK(sqlite3_exec(db,"PRAGMA foreign_keys=ON",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(sqlite3_exec(db,sql,NULL,NULL,NULL)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);
}
static int64_t scalar(const char *directory,const char *sql)
{
    char path[4096]; CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",directory)>0);
    sqlite3 *db=NULL; sqlite3_stmt *statement=NULL;
    CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK);
    CHECK(sqlite3_prepare_v2(db,sql,-1,&statement,NULL)==SQLITE_OK && sqlite3_step(statement)==SQLITE_ROW);
    int64_t value=sqlite3_column_int64(statement,0);
    CHECK(sqlite3_step(statement)==SQLITE_DONE && sqlite3_finalize(statement)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);
    return value;
}
int main(int argc,char **argv)
{
    CHECK(argc==3);
    struct ae_catalogue_identity root_identity={.kind=AE_CATALOGUE_SYSTEM},user_identity={.kind=AE_CATALOGUE_USER};
    id(root_identity.authority_epoch,1); id(user_identity.authority_epoch,3); id(user_identity.owner,2);
    uint8_t boot[16],root_runtime[16],user_runtime[16],install[16],other[16];
    id(boot,4); id(root_runtime,5); id(user_runtime,6); id(install,10); id(other,7);
    struct ae_catalogue *root=NULL,*user=NULL;
    CHECK(ae_catalogue_create(argv[1],&root_identity,boot,&root)==0);
    CHECK(ae_catalogue_enroll_account(root,user_identity.owner,1000)==0);
    CHECK(ae_catalogue_enroll_account(root,other,1001)==0);
    CHECK(ae_catalogue_close(&root)==0);
    edit(argv[1],"INSERT INTO scopes VALUES(X'00000000000040008000000000000008','machine',NULL,'active',1);"
        "INSERT INTO components VALUES('aetheri.probe',X'00000000000040008000000000000009','app','Probe',0,1);"
        "INSERT INTO artifacts VALUES(zeroblob(32),'aetheri.probe','test','x86_64-linux-gnu','verified');"
        "INSERT INTO installs(install_id,component_id,scope_id,state) VALUES(X'0000000000004000800000000000000a','aetheri.probe',X'00000000000040008000000000000002','preparing'),"
        "(X'0000000000004000800000000000000b','aetheri.probe',X'00000000000040008000000000000007','preparing'),"
        "(X'0000000000004000800000000000000c','aetheri.probe',X'00000000000040008000000000000008','preparing');"
        "INSERT INTO generations SELECT X'0000000000004000800000000000000e',X'0000000000004000800000000000000a','aetheri.probe',zeroblob(32),'committed',0,1;"
        "INSERT INTO generations SELECT X'0000000000004000800000000000000f',X'0000000000004000800000000000000b','aetheri.probe',zeroblob(32),'committed',0,1;"
        "INSERT INTO generations SELECT X'00000000000040008000000000000010',X'0000000000004000800000000000000c','aetheri.probe',zeroblob(32),'committed',0,1;"
        "UPDATE installs SET state='active',active_generation_id=(SELECT generation_id FROM generations WHERE generations.install_id=installs.install_id),revision=2;");
    CHECK(ae_catalogue_open(argv[1],&root_identity,&root)==0);
    CHECK(ae_catalogue_begin_runtime(root,boot,root_runtime)==0);
    CHECK(ae_catalogue_create(argv[2],&user_identity,boot,&user)==0 && ae_catalogue_begin_runtime(user,boot,user_runtime)==0);
    struct ae_install_snapshot snapshot,marker;
    memset(&marker,0xa5,sizeof marker); snapshot=marker;
    int64_t root_revision=scalar(argv[1],"SELECT catalogue_revision FROM authority_meta");
    CHECK(ae_catalogue_resolve_app(root,user_identity.owner,1000,install,boot,root_runtime,&snapshot)==0);
    CHECK(strcmp(snapshot.component,"aetheri.probe")==0 && snapshot.publisher[15]==9 && snapshot.generation[15]==14);
    CHECK(snapshot.account[15]==2 && snapshot.scope[15]==2 && snapshot.install_revision==2 && snapshot.generation_revision==2);
    CHECK(snapshot.gate==AE_INSTALL_GATE_CLOSED && snapshot.gate_revision==2 && snapshot.catalogue_revision==root_revision);
    CHECK(scalar(argv[1],"SELECT catalogue_revision FROM authority_meta")==root_revision);
    struct ae_install_snapshot good=snapshot,machine;
    CHECK(ae_catalogue_resolve_app(root,user_identity.owner,1001,install,boot,root_runtime,&snapshot)<0);
    id(install,11); CHECK(ae_catalogue_resolve_app(root,user_identity.owner,1000,install,boot,root_runtime,&snapshot)==-ENOENT);
    id(install,12); CHECK(ae_catalogue_resolve_app(root,user_identity.owner,1000,install,boot,root_runtime,&machine)==0);
    CHECK(machine.scope[15]==8 && machine.account[15]==2);
    CHECK(ae_catalogue_resolve_app(root,other,1001,install,boot,root_runtime,&snapshot)==0 && snapshot.account[15]==7 && snapshot.scope[15]==8);
    id(install,10);
    CHECK(ae_catalogue_resolve_app(user,user_identity.owner,1000,install,boot,root_runtime,&snapshot)==-EPERM);
    CHECK(ae_catalogue_resolve_app(root,user_identity.owner,1000,install,boot,user_runtime,&snapshot)==-ESTALE);
    struct ae_projection_context context;
    memcpy(context.system_epoch,good.system_epoch,16); memcpy(context.system_runtime,root_runtime,16);
    memcpy(context.boot,boot,16); memcpy(context.user_runtime,user_runtime,16);
    int64_t revision=-777;
    CHECK(ae_catalogue_cache_install(root,&good,&context,&revision)==-EPERM && revision==-777);
    struct ae_install_snapshot changed=good;
    changed.account[15]=7;
    CHECK(ae_catalogue_cache_install(user,&changed,&context,&revision)==-ESTALE && revision==-777);
    struct ae_projection_context stale=context; stale.user_runtime[15]^=1;
    CHECK(ae_catalogue_cache_install(user,&good,&stale,&revision)==-ESTALE);
    stale=context; stale.system_runtime[15]^=1;
    CHECK(ae_catalogue_cache_install(user,&good,&stale,&revision)==-ESTALE);
    commit_fault=1;
    CHECK(ae_catalogue_cache_install(user,&good,&context,&revision)==-EIO && revision==-777);
    CHECK(scalar(argv[2],"SELECT count(*) FROM install_bindings")==0);
    commit_fault=2;
    CHECK(ae_catalogue_cache_install(user,&good,&context,&revision)==-EIO && revision==-777);
    CHECK(scalar(argv[2],"SELECT count(*) FROM install_bindings")==1);
    CHECK(ae_catalogue_close(&user)==0 && ae_catalogue_open(argv[2],&user_identity,&user)==0);
    CHECK(ae_catalogue_cache_install(user,&good,&context,&revision)==0 && revision==3);
    CHECK(ae_catalogue_cache_install(user,&good,&context,&revision)==0 && revision==3);
    CHECK(scalar(argv[2],"SELECT count(*) FROM data_epochs")==0 && scalar(argv[2],"SELECT count(*) FROM attachments")==0 &&
          scalar(argv[2],"SELECT count(*) FROM launches")==0 && scalar(argv[2],"SELECT count(*) FROM admission_gates")==0);
    for (int case_no=0;case_no<5;++case_no) {
        changed=good;
        if (case_no==0) ++changed.install_revision;
        if (case_no==1) --changed.install_revision;
        if (case_no==2) changed.publisher[15]^=1;
        if (case_no==3) strcpy(changed.component,"aetheri.other");
        if (case_no==4) changed.system_epoch[15]^=1;
        revision=-777;
        CHECK(ae_catalogue_cache_install(user,&changed,&context,&revision)==-ESTALE && revision==-777);
    }
    stale=context; stale.system_epoch[15]^=1; changed=good; memcpy(changed.system_epoch,stale.system_epoch,16);
    CHECK(ae_catalogue_cache_install(user,&changed,&stale,&revision)==-ESTALE);
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) _exit(ae_catalogue_cache_install(user,&good,&context,&revision)==-ECHILD &&
        ae_catalogue_resolve_app(root,user_identity.owner,1000,install,boot,root_runtime,&snapshot)==-ECHILD ? 0 : 1);
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(ae_catalogue_close(&user)==0);
    edit(argv[2],"UPDATE install_bindings SET state='revoked'");
    CHECK(ae_catalogue_open(argv[2],&user_identity,&user)==0 && ae_catalogue_cache_install(user,&good,&context,&revision)==-ESTALE);
    CHECK(ae_catalogue_close(&user)==0);
    edit(argv[2],"UPDATE authority_meta SET catalogue_revision=9223372036854775807");
    CHECK(ae_catalogue_open(argv[2],&user_identity,&user)==0);
    CHECK(ae_catalogue_cache_install(user,&machine,&context,&revision)==-EOVERFLOW && revision==-777);
    CHECK(scalar(argv[2],"SELECT count(*) FROM install_bindings")==1);
    CHECK(ae_catalogue_close(&root)==0);
    const char *mutations[]={"UPDATE artifacts SET store_state='staged'", "UPDATE scopes SET state='retired' WHERE kind='account' AND os_uid=1000",
        "UPDATE installs SET state='removed',active_generation_id=NULL WHERE scope_id=X'00000000000040008000000000000002'"};
    for (size_t n=0;n<sizeof mutations/sizeof mutations[0];++n) {
        edit(argv[1],mutations[n]);
        CHECK(ae_catalogue_open(argv[1],&root_identity,&root)==0); snapshot=marker;
        CHECK(ae_catalogue_resolve_app(root,user_identity.owner,1000,install,boot,root_runtime,&snapshot)<0 && memcmp(&snapshot,&marker,sizeof snapshot)==0);
        CHECK(ae_catalogue_close(&root)==0);
        if (n==0) edit(argv[1],"UPDATE artifacts SET store_state='verified'");
        if (n==1) edit(argv[1],"UPDATE scopes SET state='active' WHERE kind='account' AND os_uid=1000");
    }
    CHECK(ae_catalogue_close(&user)==0);
    puts("Install projection: scoped account/machine observation, separate user writer, lost commits, stale/revoked contexts, no data/admission and fork guards passed.");
    return 0;
}
