#include "core/unit_issuer.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"issuer codec line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(int argc,char **argv)
{
    CHECK(argc==2);
    FILE *f=fopen(argv[1],"rb"); CHECK(f);
    uint8_t raw[AE_UNIT_ISSUER_MAX+1],copy[AE_UNIT_ISSUER_MAX+1];
    size_t n=fread(raw,1,sizeof raw,f); CHECK(!ferror(f) && !fclose(f) && n<=AE_UNIT_ISSUER_MAX);
    struct ae_unit_issuer value,marker; memset(&marker,0xa5,sizeof marker);
    CHECK(!ae_unit_issuer_decode(raw,n,&value));
    CHECK(value.pidfs_id==UINT64_MAX && value.cgroup_id==UINT64_C(9223372036854775839));
    CHECK(value.image.mtime_seconds==-1 && value.image.ctime_seconds==INT64_MAX);
    size_t used=99; CHECK(!ae_unit_issuer_encode(&value,copy,sizeof copy,&used) && used==n && !memcmp(raw,copy,n));
    for (size_t k=0;k<n;++k) {
        struct ae_unit_issuer got=marker;
        CHECK(ae_unit_issuer_decode(raw,k,&got)==-EBADMSG && !memcmp(&got,&marker,sizeof got));
        memset(copy,0xa5,sizeof copy); used=99;
        CHECK(ae_unit_issuer_encode(&value,copy,k,&used)==-ENOSPC && used==99);
        for (size_t j=0;j<sizeof copy;++j) CHECK(copy[j]==0xa5);
    }
    memcpy(copy,raw,n); copy[n]=0;
    struct ae_unit_issuer got=marker;
    CHECK(ae_unit_issuer_decode(copy,n+1,&got)==-EBADMSG && !memcmp(&got,&marker,sizeof got));
    memcpy(copy,raw,n); memset(copy+168,0xff,4); got=marker;
    CHECK(ae_unit_issuer_decode(copy,n,&got)==-EBADMSG && !memcmp(&got,&marker,sizeof got));
    const char *bad_groups[]={"/","/a//b","/a/.","/a/../b","/a/"};
    for (size_t k=0;k<sizeof bad_groups/sizeof bad_groups[0];++k) {
        struct ae_unit_issuer bad=value; strcpy(bad.cgroup,bad_groups[k]); memset(copy,0xa5,sizeof copy); used=99;
        CHECK(ae_unit_issuer_encode(&bad,copy,sizeof copy,&used)==-EBADMSG && used==99 && copy[0]==0xa5);
    }
    memset(value.unit,'a',247); strcpy(value.unit+247,".service");
    value.cgroup[0]='/'; memset(value.cgroup+1,'b',1022); value.cgroup[1023]=0;
    memset(value.label,'c',511); value.label[511]=0;
    CHECK(!ae_unit_issuer_encode(&value,copy,sizeof copy,&used) && used==1969);
    CHECK(!ae_unit_issuer_decode(copy,used,&got) && !strcmp(got.cgroup,value.cgroup));
    value.label[0]=(char)0x80; used=99;
    CHECK(ae_unit_issuer_encode(&value,copy,sizeof copy,&used)==-EBADMSG && used==99);
    puts("AETHUIS1 independent vector, exact boundaries and untouched error outputs verified");
    return 0;
}
