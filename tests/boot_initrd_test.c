/* Pure bounded metadata/file probes; the real USB/mount path is tested in a VM. */
#define main ae_initrd_entry
#include "../src/boot/initrd.c"
#undef main
#include <assert.h>

static const char good[]="AETHERI-USB1\n0123456789abcdef0123456789abcdef\n11111111-2222-3333-4444-555555555555\n66666666-7777-8888-9999-aaaaaaaaaaaa\n6.19.10-300.fc44.x86_64\n";
int main(void) {
    assert(getuid()==1000);
    char buffer[1024];struct profile p;
    memcpy(buffer,good,sizeof(good));assert(parse_profile(buffer,&p));
    assert(!p.desktop && p.disk_sectors==1048576 && p.root_sectors==524288);
    assert(!strcmp(p.nonce,"0123456789abcdef0123456789abcdef"));
    for(size_t i=0;i<sizeof(good)-1;++i){memcpy(buffer,good,sizeof(good));buffer[i]=0;assert(!parse_profile(buffer,&p));}
    memcpy(buffer,good,sizeof(good));buffer[12]='G';assert(!parse_profile(buffer,&p));
    snprintf(buffer,sizeof(buffer),"%sx\n",good);assert(!parse_profile(buffer,&p));
    assert(uuid_text("11111111-2222-3333-4444-555555555555"));
    assert(!uuid_text("111111112222-3333-4444-555555555555"));
    assert(!uuid_text("66666666-7777-8888-9999-AAAAAAAAAAAA"));
    dev_t d=0;assert(device_text("8:2\n",&d) && major(d)==8 && minor(d)==2);
    assert(device_text("0:0\n",&d) && d==0);
    const char *bad[]={"8:2","08:2\n","8:02\n","-1:2\n","+8:2\n","8:2\nx","8:2\n\n","8:2x\n","99999999999999999999999999999:2\n","8:9999999999999999999999999999\n"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i)assert(!device_text(bad[i],&d));
    assert(field("A=x\nDEVTYPE=usb_device\nB=y\n","DEVTYPE=usb_device"));
    assert(!field("A=DEVTYPE=usb_device\n","DEVTYPE=usb_device"));
    assert(!field("DEVTYPE=usb_device_extra\n","DEVTYPE=usb_device"));
    char path[]="/tmp/aetheri-initrd-XXXXXX";int fd=mkstemp(path);assert(fd>=0);
    unsigned char raw[2048]={0};raw[1080]=0x53;raw[1081]=0xef;
    for(size_t i=0,at=0;i<36;){if(p.filesystem[i]=='-'){++i;continue;}raw[1128+at++]=(unsigned char)(digit(p.filesystem[i])*16+digit(p.filesystem[i+1]));i+=2;}
    assert(write(fd,raw,sizeof(raw))==(ssize_t)sizeof(raw));
    assert(superblock(fd,p.filesystem));assert(!superblock(fd,"11111111-2222-3333-4444-555555555555"));
    raw[1080]=0;assert(pwrite(fd,raw,sizeof(raw),0)==(ssize_t)sizeof(raw));assert(!superblock(fd,p.filesystem));
    assert(!ftruncate(fd,0));assert(pwrite(fd,"ab\0cd",5,0)==5);assert(!read_text_at(AT_FDCWD,path,buffer,sizeof(buffer)));
    assert(!ftruncate(fd,0));assert(pwrite(fd,"bounded\n",8,0)==8);
    assert(read_text_at(AT_FDCWD,path,buffer,sizeof(buffer)) && !strcmp(buffer,"bounded\n"));
    assert(!read_text_at(AT_FDCWD,path,buffer,8));
    assert(!close(fd) && !unlink(path));
    struct observation o={.partition=makedev(8,2),.disk=makedev(8,0),.sequence=123,.parent=456};
    memcpy(o.boot,"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",37);
    assert(record(&p,&o,buffer,sizeof(buffer)));char original[1024];memcpy(original,buffer,strlen(buffer)+1);
    ++o.sequence;assert(record(&p,&o,buffer,sizeof(buffer)) && strcmp(original,buffer));
    assert(!record(&p,&o,buffer,8));
    const char desktop[]="AETHERI-USB2\n0123456789abcdef0123456789abcdef\n11111111-2222-3333-4444-555555555555\n66666666-7777-8888-9999-aaaaaaaaaaaa\n6.19.10-300.fc44.x86_64\n61440000\n1048576\nbbbbbbbb-cccc-dddd-eeee-ffffffffffff\n01234567-89ab-cdef-0123-456789abcdef\n1310720\n60127232\n";
    memcpy(buffer,desktop,sizeof(desktop));assert(parse_profile(buffer,&p) && p.desktop && p.disk_sectors==61440000 && p.root_sectors==1048576 && p.data_start==1310720);
    assert(record(&p,&o,original,sizeof(original)) && !strncmp(original,"AETHERI-USB-OBS2\n",sizeof("AETHERI-USB-OBS2\n")-1));
    struct profile changed=p;changed.data_filesystem[0]='1';assert(record(&changed,&o,buffer,sizeof(buffer)) && strcmp(buffer,original));
    changed=p;changed.data_partition[0]='1';assert(record(&changed,&o,buffer,sizeof(buffer)) && strcmp(buffer,original));
    changed=p;++changed.data_sectors;assert(record(&changed,&o,buffer,sizeof(buffer)) && strcmp(buffer,original));
    for(size_t i=0;i<sizeof(desktop)-1;++i){memcpy(buffer,desktop,sizeof(desktop));buffer[i]=0;assert(!parse_profile(buffer,&p));}
    uint64_t integer=0;assert(decimal("18446744073709551615",&integer) && integer==UINT64_MAX);
    const char *bad_decimal[]={"", "01", "-1", "+1", "1x", "18446744073709551616"};
    for(size_t i=0;i<sizeof(bad_decimal)/sizeof(bad_decimal[0]);++i)assert(!decimal(bad_decimal[i],&integer));
    const char *fields[]={"0", "67110912", "18446744073709551615", "61440001"};
    for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i){
        const char *at=strstr(desktop,"61440000");size_t prefix=(size_t)(at-desktop);
        memcpy(buffer,desktop,prefix);snprintf(buffer+prefix,sizeof(buffer)-prefix,"%s%s",fields[i],at+8);assert(!parse_profile(buffer,&p));
    }
    struct observation data=o;data.partition=makedev(8,3);assert(same_parent(&o,&data));
    ++data.sequence;assert(!same_parent(&o,&data));data=o;data.partition=makedev(9,3);data.disk=makedev(9,0);assert(!same_parent(&o,&data));
    data=o;data.partition=makedev(8,3);++data.parent;assert(!same_parent(&o,&data));
    data=o;data.partition=makedev(8,3);data.boot[0]='0';assert(!same_parent(&o,&data));
    assert(!same_parent(&o,&o));
    puts("Initrd: bounded profile/UUID/devnum, exact ext4 metadata, file bounds and original observation tests passed");
    return 0;
}
