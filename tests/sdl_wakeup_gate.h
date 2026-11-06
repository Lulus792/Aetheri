/* Injected only into private counterexample SDKs, never application profiles. */
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
static void aetheri_test_wake_gate(void) {
    static int used;
    const char *value=getenv("AETHERI_TEST_WAKE_FD");
    if(used || !value) return;
    used=1;int fd=atoi(value);char c='G';ssize_t n;
    do {n=write(fd,&c,1);}while(n<0 && errno==EINTR);
    if(n!=1) abort();
    do {n=read(fd,&c,1);}while(n<0 && errno==EINTR);
    if(n!=1 || c!='A') abort();
}
