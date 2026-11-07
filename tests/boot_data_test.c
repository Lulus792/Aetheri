/* Native record-format/filesystem adversaries for the isolated boot fixture. */
#define main aetheri_data_probe_main
#include "../src/boot/data.c"
#undef main
#include <assert.h>

static void candidate(int parent,const char *bytes,mode_t mode) {
    int fd=openat(parent,"boot-persistence.txt",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,mode);
    assert(fd>=0);
    assert(write_all(fd,bytes,strlen(bytes)));
    assert(fchmod(fd,mode)==0);
    assert(close(fd)==0);
}

int main(void) {
    assert(getuid()==1000 && getgid()==1000);
    char path[]="/tmp/aetheri-data-test-XXXXXX";
    assert(mkdtemp(path));
    int parent=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    assert(parent>=0);
    const char *good="AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 1 12345678-1234-1234-1234-123456789abc\n";
    struct record r={0};struct stat s={0};bool missing=false;
    assert(record_read(parent,&r,&s,&missing) && missing);
    candidate(parent,good,0600);
    assert(record_read(parent,&r,&s,&missing) && !missing && r.count==1);
    assert(!strcmp(r.nonce,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    assert(linkat(parent,"boot-persistence.txt",parent,"other",0)==0);
    assert(!record_read(parent,&r,&s,&missing));
    assert(unlinkat(parent,"other",0)==0);
    assert(unlinkat(parent,"boot-persistence.txt",0)==0);
    const char *invalid[]={
        "AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 0 12345678-1234-1234-1234-123456789abc\n",
        "AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa -1 12345678-1234-1234-1234-123456789abc\n",
        "AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 01 12345678-1234-1234-1234-123456789abc\n",
        "AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 1000000 12345678-1234-1234-1234-123456789abc\n",
        "AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 9999999999999999999999999999999999999 12345678-1234-1234-1234-123456789abc\n",
        "AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 1 12345678-1234-1234-1234-123456789abc",
        "AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 1 12345678-1234-1234-1234-123456789abc\nextra",
        "AETHERI-DATA1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 1 short\n",
        "AETHERI-DATA1 short 1 12345678-1234-1234-1234-123456789abc\n",
    };
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        candidate(parent,invalid[i],0600);
        assert(!record_read(parent,&r,&s,&missing) && !missing);
        assert(unlinkat(parent,"boot-persistence.txt",0)==0);
    }
    candidate(parent,good,0644);
    assert(!record_read(parent,&r,&s,&missing));
    assert(unlinkat(parent,"boot-persistence.txt",0)==0);
    assert(symlinkat("foreign",parent,"boot-persistence.txt")==0);
    assert(!record_read(parent,&r,&s,&missing));
    assert(unlinkat(parent,"boot-persistence.txt",0)==0);
    assert(close(parent)==0 && rmdir(path)==0);
    puts("Aetheri data record: canonical/missing/range/overflow/mode/link/foreign cases passed");
    return 0;
}
