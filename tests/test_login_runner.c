#define _GNU_SOURCE
#define clock_gettime ae_test_clock
#include "../src/login/runner.c"
#undef clock_gettime

#define CHECK(x) do {if(!(x)){fprintf(stderr,"runner model line %d: %s\n",__LINE__,#x);abort();}}while(0)
static struct timespec clock_value={.tv_sec=100,.tv_nsec=0};
static bool clock_failure;
int ae_test_clock(clockid_t id,struct timespec *out){CHECK(id==CLOCK_MONOTONIC);if(clock_failure){errno=EIO;return -1;}*out=clock_value;return 0;}
static int gate_calls;
static int test_gate(void *unused){(void)unused;++gate_calls;return 0;}
static void original_selection(void) {
    struct selection original={.manager=":1.1",.path="/original",.pid=123,.invocation={1}};
    struct ae_image_process process={.pid=123};uint8_t other[16]={2},boot[16]={3};
    CHECK(!match_selected(NULL,":1.1","/original",&process,original.invocation,boot,4,&original));
    CHECK(match_selected(NULL,":1.1","/original",&process,other,boot,4,&original)==-ESTALE);
    CHECK(match_selected(NULL,":1.2","/original",&process,original.invocation,boot,4,&original)==-ESTALE);
    CHECK(match_selected(NULL,":1.1","/replacement",&process,original.invocation,boot,4,&original)==-ESTALE);
    process.pid=124;CHECK(match_selected(NULL,":1.1","/original",&process,original.invocation,boot,4,&original)==-ESTALE);
}
static void bounded_wait(void) {
    struct runner r={.deadline=100050000};
    CHECK(remaining(&r,25)==25);clock_value.tv_nsec=49000000;CHECK(remaining(&r,25)==1);
    clock_value.tv_nsec=50000000;CHECK(remaining(&r,25)==-ETIMEDOUT);
    clock_failure=true;CHECK(remaining(&r,25)==-EIO);clock_failure=false;
    uint64_t output=7;clock_value.tv_sec=-1;CHECK(clock_usec(&output)==-EOVERFLOW && output==7);
    clock_value=(struct timespec){.tv_sec=100};
}
static void preflight(void) {
    CHECK(ae_login_startup_run(NULL)==-EINVAL);
    struct ae_login_run_profile p={.unit="startup.service",.label="selected-label",.socket_prefix="/run/greetd-",.timeout_usec=1000000,
        .startup={.service={.unit="backend.service",.label="backend-label",.socket_label="socket-label"},.check=test_gate}};
    p.timeout_usec=15000001;CHECK(ae_login_startup_run(&p)==-EINVAL && !gate_calls);p.timeout_usec=1000000;
    p.socket_prefix="relative";CHECK(ae_login_startup_run(&p)==-EINVAL && !gate_calls);p.socket_prefix="/run/greetd-";
    p.startup.check=NULL;CHECK(ae_login_startup_run(&p)==-EINVAL && !gate_calls);p.startup.check=test_gate;
    CHECK(ae_login_startup_run(&p)==-EACCES && !gate_calls);
}
int main(void) {
    if(!getuid() || !geteuid())return 77;
    original_selection();bounded_wait();preflight();
    puts("Startup runner: original manager/path/PID/invocation, bounded wait and preflight rejection passed");return 0;
}
