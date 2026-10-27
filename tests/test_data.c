#include "catalogue/data.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"data line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int fault,step_number,crash_mode;
int __real_sqlite3_exec(sqlite3 *,const char *,int (*)(void *,int,char **,char **),void *,char **);
int __wrap_sqlite3_exec(sqlite3 *db,const char *sql,int (*callback)(void *,int,char **,char **),void *context,char **error)
{
    if (strcmp(sql,"COMMIT")==0 && crash_mode) {
        if (crash_mode==2) CHECK(__real_sqlite3_exec(db,sql,callback,context,error)==SQLITE_OK);
        _exit(70+crash_mode);
    }
    if (strcmp(sql,"COMMIT")==0 && (fault==5 || fault==6)) {
        int mode=fault; fault=0;
        if (mode==6) CHECK(__real_sqlite3_exec(db,sql,callback,context,error)==SQLITE_OK);
        return SQLITE_IOERR;
    }
    return __real_sqlite3_exec(db,sql,callback,context,error);
}
int __real_sqlite3_step(sqlite3_stmt *);
int __wrap_sqlite3_step(sqlite3_stmt *statement)
{
    const char *sql=sqlite3_sql(statement);
    if (sql!=NULL && strncmp(sql,"WITH r(",7)==0 && strstr(sql,"SELECT (SELECT")==NULL && fault>=1 && fault<=4) {
        if (++step_number==fault) { fault=0; return SQLITE_IOERR; }
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
    FILE *input=fopen(argv[2],"rb"); CHECK(input!=NULL);
    uint8_t bytes[AE_DATA_CONFIG_BYTES+1]; CHECK(fread(bytes,1,sizeof bytes,input)==AE_DATA_CONFIG_BYTES && fclose(input)==0);
    struct ae_data_enrollment request,marker,decoded;
    memset(&marker,0xa5,sizeof marker);
    CHECK(ae_data_enrollment_decode(bytes,AE_DATA_CONFIG_BYTES,&request)==0);
    CHECK(request.install[15]==20 && request.owner.id[AE_ACCOUNT][15]==2 && request.allocation.root.inode==19);
    for (size_t size=0;size<=sizeof bytes;++size) if (size!=AE_DATA_CONFIG_BYTES) {
        decoded=marker; CHECK(ae_data_enrollment_decode(bytes,size,&decoded)<0 && memcmp(&decoded,&marker,sizeof marker)==0);
    }
    const size_t offsets[]={0,8,8+8+6,112+6,128+6,144+6,160+6,176,184,192,200};
    for (size_t n=0;n<sizeof offsets/sizeof offsets[0];++n) {
        size_t offset=offsets[n]; uint8_t saved=bytes[offset]; bytes[offset]=0xff;
        decoded=marker; CHECK(ae_data_enrollment_decode(bytes,AE_DATA_CONFIG_BYTES,&decoded)<0 && memcmp(&decoded,&marker,sizeof marker)==0); bytes[offset]=saved;
    }
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_USER};
    memcpy(identity.authority_epoch,request.user_epoch,16); memcpy(identity.owner,request.owner.id[AE_ACCOUNT],16);
    uint8_t boot[16],runtime[16]; id(boot,30); id(runtime,31);
    struct ae_catalogue *store=NULL;
    CHECK(ae_catalogue_create(argv[1],&identity,boot,&store)==0 && ae_catalogue_begin_runtime(store,boot,runtime)==0 && ae_catalogue_close(&store)==0);
    const char *binding_sql="INSERT INTO install_bindings VALUES(X'00000000000040008000000000000014','aetheri.probe',X'00000000000040008000000000000015',X'00000000000040008000000000000002',X'00000000000040008000000000000016',2,'active')";
    edit(argv[1],binding_sql);
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0);
    int64_t revision=-777;
    struct ae_data_enrollment wrong=request;
    wrong.base_revision=3;
    CHECK(ae_catalogue_enroll_data(store,&wrong,boot,&revision)==-ESTALE && revision==-777);
    wrong=request; wrong.owner.id[AE_ACCOUNT][15]=24; wrong.allocation.owner=wrong.owner;
    CHECK(ae_catalogue_enroll_data(store,&wrong,boot,&revision)==-ESTALE);
    wrong=request; wrong.user_epoch[15]=24;
    CHECK(ae_catalogue_enroll_data(store,&wrong,boot,&revision)==-ESTALE);
    wrong=request; wrong.system_epoch[15]=24;
    CHECK(ae_catalogue_enroll_data(store,&wrong,boot,&revision)==-ESTALE);
    wrong=request; wrong.install_revision++;
    CHECK(ae_catalogue_enroll_data(store,&wrong,boot,&revision)==-ESTALE);
    uint8_t other_boot[16]; id(other_boot,32);
    CHECK(ae_catalogue_enroll_data(store,&request,other_boot,&revision)==-ESTALE);
    for (int n=1;n<=5;++n) {
        fault=n; step_number=0;
        CHECK(ae_catalogue_enroll_data(store,&request,boot,&revision)==-EIO && revision==-777 && fault==0);
        CHECK(scalar(argv[1],"SELECT count(*) FROM data_epochs")==0 && scalar(argv[1],"SELECT count(*) FROM attachments")==0 && scalar(argv[1],"SELECT count(*) FROM admission_gates")==0);
    }
    fault=6;
    CHECK(ae_catalogue_enroll_data(store,&request,boot,&revision)==-EIO && revision==-777);
    CHECK(ae_catalogue_close(&store)==0 && ae_catalogue_open(argv[1],&identity,&store)==0);
    CHECK(ae_catalogue_enroll_data(store,&request,boot,&revision)==0 && revision==3);
    CHECK(scalar(argv[1],"SELECT state='closed' AND revision=2 AND closing_operation_id IS NULL FROM admission_gates")==1);
    CHECK(scalar(argv[1],"SELECT count(*) FROM attachments")==1 && scalar(argv[1],"SELECT count(*) FROM launches")==0);
    CHECK(scalar(argv[1],"SELECT count(*) FROM grants")==0 && scalar(argv[1],"SELECT count(*) FROM operations")==0);
    pid_t child=fork(); CHECK(child>=0);
    if (child==0) _exit(ae_catalogue_enroll_data(store,&request,boot,&revision)==-ECHILD ? 0 : 1);
    int status; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(ae_catalogue_close(&store)==0);
    edit(argv[1],"UPDATE admission_gates SET state='open',revision=revision+1; UPDATE authority_meta SET catalogue_revision=9");
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0);
    CHECK(ae_catalogue_enroll_data(store,&request,other_boot,&revision)==0 && revision==9);
    CHECK(scalar(argv[1],"SELECT state='open' AND revision=3 FROM admission_gates")==1);
    wrong=request; wrong.schema_version++;
    CHECK(ae_catalogue_enroll_data(store,&wrong,boot,&revision)==-ESTALE);
    wrong=request; wrong.owner.id[AE_STORAGE][15]=25; wrong.allocation.owner=wrong.owner;
    CHECK(ae_catalogue_enroll_data(store,&wrong,boot,&revision)==-ESTALE);
    CHECK(ae_catalogue_close(&store)==0);
    edit(argv[1],"DELETE FROM attachments; UPDATE admission_gates SET state='closed',revision=revision+1; UPDATE data_epochs SET state='retained'");
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0 && ae_catalogue_enroll_data(store,&request,boot,&revision)==-ESTALE && ae_catalogue_close(&store)==0);
    edit(argv[1],"UPDATE data_epochs SET state='purging'");
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0 && ae_catalogue_enroll_data(store,&request,boot,&revision)==-ESTALE && ae_catalogue_close(&store)==0);
    edit(argv[1],"UPDATE data_epochs SET state='purged'");
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0 && ae_catalogue_enroll_data(store,&request,boot,&revision)==-ESTALE && ae_catalogue_close(&store)==0);
    edit(argv[1],"DELETE FROM admission_gates; DELETE FROM data_epochs");
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0 && ae_catalogue_enroll_data(store,&request,boot,&revision)==-ESTALE && ae_catalogue_close(&store)==0);
    edit(argv[1],"UPDATE authority_meta SET catalogue_revision=9223372036854775807");
    wrong=request; wrong.base_revision=INT64_MAX;
    CHECK(ae_catalogue_open(argv[1],&identity,&store)==0 && ae_catalogue_enroll_data(store,&wrong,boot,&revision)==-EOVERFLOW && ae_catalogue_close(&store)==0);
    CHECK(ae_catalogue_create(argv[3],&identity,boot,&store)==0 && ae_catalogue_begin_runtime(store,boot,runtime)==0 && ae_catalogue_close(&store)==0);
    edit(argv[3],binding_sql);
    for (int mode=1;mode<=2;++mode) {
        child=fork(); CHECK(child>=0);
        if (child==0) {
            struct ae_catalogue *fresh=NULL;
            CHECK(ae_catalogue_open(argv[3],&identity,&fresh)==0);
            crash_mode=mode;
            (void)ae_catalogue_enroll_data(fresh,&request,boot,&revision);
            _exit(90);
        }
        CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==70+mode);
        CHECK(ae_catalogue_open(argv[3],&identity,&store)==0);
        CHECK(scalar(argv[3],"SELECT count(*) FROM data_epochs")==mode-1);
        CHECK(scalar(argv[3],"SELECT count(*) FROM attachments")==mode-1);
        CHECK(scalar(argv[3],"SELECT count(*) FROM admission_gates WHERE state='closed'")==mode-1);
        CHECK(ae_catalogue_close(&store)==0);
    }
    CHECK(ae_catalogue_open(argv[3],&identity,&store)==0 && ae_catalogue_enroll_data(store,&request,boot,&revision)==0 && revision==3 && ae_catalogue_close(&store)==0);
    puts("Initial data enrollment: independent config, atomic closed gate, SQL/commit faults, process exits, exact replay and no resurrection passed.");
    return 0;
}
