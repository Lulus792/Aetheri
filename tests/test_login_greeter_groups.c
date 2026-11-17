#define _GNU_SOURCE
#define getgroups ae_test_getgroups
#include "../src/login/greeter.c"
#undef getgroups
#include <stdio.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"greeter groups line %d: %s\n",__LINE__,#x);abort();}}while(0)
static gid_t actual[AE_LOGIN_GREETER_GROUPS_MAX];
static int count,group_error,calls;
int ae_test_getgroups(int size,gid_t *out) {
    CHECK(size==AE_LOGIN_GREETER_GROUPS_MAX);++calls;
    if(group_error){errno=group_error;return -1;}
    if(count)memcpy(out,actual,(size_t)count*sizeof(gid_t));
    return count;
}
int main(void) {
    gid_t expected[AE_LOGIN_GREETER_GROUPS_MAX]={992};
    CHECK(!ae_login_greeter_check_groups(NULL,0));
    count=1;actual[0]=992;CHECK(!ae_login_greeter_check_groups(expected,1));
    actual[0]=10;CHECK(ae_login_greeter_check_groups(expected,1)==-EACCES);
    count=2;actual[0]=992;actual[1]=10;CHECK(ae_login_greeter_check_groups(expected,1)==-EACCES);
    expected[0]=10;expected[1]=992;CHECK(!ae_login_greeter_check_groups(expected,2));
    actual[0]=10;actual[1]=10;CHECK(ae_login_greeter_check_groups(expected,2)==-EACCES);
    count=AE_LOGIN_GREETER_GROUPS_MAX;
    for(size_t i=0;i<AE_LOGIN_GREETER_GROUPS_MAX;++i){actual[i]=(gid_t)(AE_LOGIN_GREETER_GROUPS_MAX-i);expected[i]=(gid_t)(i+1);}
    CHECK(!ae_login_greeter_check_groups(expected,AE_LOGIN_GREETER_GROUPS_MAX));
    group_error=EINVAL;CHECK(ae_login_greeter_check_groups(expected,AE_LOGIN_GREETER_GROUPS_MAX)==-EINVAL);
    group_error=EIO;CHECK(ae_login_greeter_check_groups(expected,AE_LOGIN_GREETER_GROUPS_MAX)==-EIO);
    struct ae_login_greeter_expect e={.uid=992,.gid=992,.label="selected",.worker_label="selected"};
    struct ae_login_greeter *out=NULL;struct ae_login_service *s=(void *)&e;struct ae_login_sources *sources=(void *)&e;
    e.group_count=AE_LOGIN_GREETER_GROUPS_MAX+1;CHECK(ae_login_greeter_create(s,sources,&e,&out)==-EINVAL && !out);
    e.group_count=1;CHECK(ae_login_greeter_create(s,sources,&e,&out)==-EINVAL && !out);
    gid_t invalid[]={0,992};e.groups=invalid;CHECK(ae_login_greeter_create(s,sources,&e,&out)==-EINVAL && !out);
    invalid[0]=(gid_t)-1;CHECK(ae_login_greeter_create(s,sources,&e,&out)==-EINVAL && !out);
    invalid[0]=992;e.group_count=2;CHECK(ae_login_greeter_create(s,sources,&e,&out)==-EINVAL && !out);
    CHECK(calls==9);
    puts("Greeter supplementary groups: exact sets, extra/missing/duplicate rejection, bound and errors passed");return 0;
}
