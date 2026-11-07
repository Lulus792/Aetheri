#define _GNU_SOURCE
#include "catalogue/recovery.h"
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"recovery inventory %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int armed,mode,used,allocations;static struct ae_catalogue *current;
static sqlite3 *writer;
int __real_sqlite3_exec(sqlite3 *,const char *,int (*)(void *,int,char **,char **),void *,char **);
int __wrap_sqlite3_exec(sqlite3 *db,const char *sql,int (*cb)(void *,int,char **,char **),void *data,char **error) {
    int hit=armed && !used && (((mode==1 || mode==2) && !strcmp(sql,"BEGIN")) || ((mode==3 || mode==4) && !strcmp(sql,"COMMIT")));
    if(hit) {used=1;if(mode==2 || mode==4) CHECK(__real_sqlite3_exec(db,sql,cb,data,error)==SQLITE_OK);return SQLITE_IOERR;}
    return __real_sqlite3_exec(db,sql,cb,data,error);
}
int __real_sqlite3_prepare_v2(sqlite3 *,const char *,int,sqlite3_stmt **,const char **);
int __wrap_sqlite3_prepare_v2(sqlite3 *db,const char *sql,int bytes,sqlite3_stmt **out,const char **tail) {
    if(armed && !used && strstr(sql,"SELECT o.operation_id")) {
        if(mode==5) {used=1;return SQLITE_NOMEM;}
        if(mode==10) {used=1;struct ae_recovery_inventory *nested=NULL;CHECK(ae_catalogue_recovery_inventory(current,65536,&nested)==-EBUSY && !nested);}
        if(mode==13) {
            used=1;
            CHECK(__real_sqlite3_exec(writer,"BEGIN IMMEDIATE; UPDATE operations SET reason_code='concurrent',revision=revision+1 WHERE state='accepted'",NULL,NULL,NULL)==SQLITE_OK);
            CHECK(__real_sqlite3_exec(writer,"COMMIT",NULL,NULL,NULL)==SQLITE_BUSY);
        }
    }
    return __real_sqlite3_prepare_v2(db,sql,bytes,out,tail);
}
int __real_sqlite3_step(sqlite3_stmt *);
int __wrap_sqlite3_step(sqlite3_stmt *q) {
    if(armed && mode==6 && !used && strstr(sqlite3_sql(q),"SELECT o.operation_id")) {used=1;return SQLITE_IOERR;}
    return __real_sqlite3_step(q);
}
void *__real_realloc(void *,size_t);
void *__wrap_realloc(void *p,size_t n) {
    if(armed && (mode==7 || mode==8) && ++allocations==(mode==7?1:2)) {used=1;return NULL;}
    return __real_realloc(p,n);
}
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size) {
    if(armed && mode==9 && !used) {used=1;return NULL;}
    return __real_calloc(n,size);
}
static void hex(const void *data,size_t n) {
    const unsigned char *p=data;if(!n) {putchar('-');return;}
    for(size_t i=0;i<n;++i) printf("%02x",p[i]);
}
static void id(const char *text,uint8_t out[16]) {
    CHECK(strlen(text)==32);
    for(unsigned i=0;i<16;++i) {char pair[3]={text[2*i],text[2*i+1],0},*end;unsigned long v=strtoul(pair,&end,16);CHECK(!*end && v<=255);out[i]=(uint8_t)v;}
}
static unsigned fds(void) {
    DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned n=0;struct dirent *p;
    while((p=readdir(d))) if(p->d_name[0]!='.') ++n;
    CHECK(!closedir(d));return n;
}
int main(int argc,char **argv) {
    CHECK(argc==8);struct ae_catalogue_identity expected={.kind=!strcmp(argv[2],"system")?AE_CATALOGUE_SYSTEM:AE_CATALOGUE_USER};
    id(argv[3],expected.authority_epoch);if(expected.kind==AE_CATALOGUE_USER) id(argv[4],expected.owner);
    char *end;unsigned long limit=strtoul(argv[5],&end,10);CHECK(!*end && limit<=AE_RECOVERY_MAX_RECORDS);
    mode=(int)strtol(argv[6],&end,10);CHECK(!*end && mode>=0 && mode<=13);
    int wanted=(int)strtol(argv[7],&end,10);CHECK(!*end);
    CHECK(ae_catalogue_open(argv[1],&expected,&current)==0);
    struct ae_catalogue_info before,after;CHECK(ae_catalogue_get_info(current,&before)==0);
    if(mode==11 || mode==12) {
        uint8_t runtime[16]={0x81,0,0,0,0,0,0x40,0,0x80};runtime[15]=(uint8_t)mode;
        CHECK(ae_catalogue_begin_runtime(current,before.boot_id,runtime)==0);
        CHECK(ae_catalogue_close(&current)==0);puts("STARTED");return 0;
    }
    if(mode==13) {
        char path[4096];CHECK(snprintf(path,sizeof path,"%s/catalogue.sqlite3",argv[1])>0);
        CHECK(sqlite3_open_v2(path,&writer,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK);
    }
    unsigned handles=fds();
    struct ae_recovery_inventory *v=NULL;armed=1;int result=ae_catalogue_recovery_inventory(current,(size_t)limit,&v);armed=0;
    CHECK(result==wanted && (mode==0 || used));
    if(mode==13) CHECK(__real_sqlite3_exec(writer,"ROLLBACK",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(fds()==handles);
    if(result) CHECK(!v);
    else {
        CHECK(v && !memcmp(&v->context,&before,sizeof(before)) && !memcmp(&v->identity,&expected,sizeof(expected)));
        printf("CONTEXT %u %" PRId64 " ",v->schema_version,v->context.revision);hex(v->context.authority_epoch,16);putchar(' ');hex(v->context.boot_id,16);putchar(' ');hex(v->context.runtime_incarnation,16);printf(" %zu\n",v->count);
        size_t counts[AE_RECOVERY_KIND_COUNT]={0};
        for(size_t i=0;i<v->count;++i) {
            struct ae_recovery_record *r=&v->records[i];CHECK(r->kind>0 && r->kind<AE_RECOVERY_KIND_COUNT);++counts[r->kind];
            if(i) CHECK(v->records[i-1].kind<r->kind || (v->records[i-1].kind==r->kind && memcmp(v->records[i-1].id,r->id,16)<0));
            printf("ITEM %u ",(unsigned)r->kind);hex(r->id,16);putchar(' ');if(r->has_related) hex(r->related,16);else putchar('-');
            printf(" %" PRId64 " %" PRId64 " ",r->revision,r->unfinished_steps);hex(r->state,strlen(r->state));putchar(' ');hex(r->detail,strlen(r->detail));putchar(' ');hex(r->phase,strlen(r->phase));putchar(' ');hex(r->reason,strlen(r->reason));putchar('\n');
        }
        CHECK(!memcmp(counts,v->by_kind,sizeof(counts)));ae_recovery_inventory_free(&v);CHECK(!v);ae_recovery_inventory_free(&v);
    }
    CHECK(ae_catalogue_get_info(current,&after)==0 && !memcmp(&before,&after,sizeof(before)));
    CHECK(ae_catalogue_recovery_inventory(current,0,&v)==-EINVAL && !v);
    struct ae_recovery_inventory *sentinel=(void *)(uintptr_t)1;
    CHECK(ae_catalogue_recovery_inventory(current,1,&sentinel)==-EINVAL && sentinel==(void *)(uintptr_t)1);
    if(writer) CHECK(sqlite3_close(writer)==SQLITE_OK);
    CHECK(ae_catalogue_close(&current)==0);return 0;
}
