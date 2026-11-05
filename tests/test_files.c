#define _GNU_SOURCE
#include "files/model.h"
#include "editor/file.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"files %d: %s errno=%d\n",__LINE__,#x,errno);exit(1); } } while (0)
void *__real_calloc(size_t n,size_t size);static bool deny;
void *__wrap_calloc(size_t n,size_t size) { return deny ? NULL : __real_calloc(n,size); }
static void put(int dir,const char *name,const char *text) { int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0);size_t n=strlen(text);CHECK(write(fd,text,n)==(ssize_t)n && !close(fd)); }
static void select_name(struct ae_files *f,const char *name) { for (size_t i=0;i<ae_files_count(f);++i) if (!strcmp(ae_files_entry(f,i)->name,name)) { CHECK(ae_files_select(f,i));return; }CHECK(false); }
static size_t descriptors(void) {
    DIR *d=opendir("/proc/self/fd");CHECK(d);size_t n=0;struct dirent *e;
    while ((e=readdir(d))) if (strcmp(e->d_name,".") && strcmp(e->d_name,"..")) ++n;
    CHECK(!closedir(d));return n;
}
static void names(void) {
    char out[AE_FILES_LABEL+1],saved[sizeof(out)];memset(out,0x53,sizeof(out));memcpy(saved,out,sizeof(out));
    CHECK(!ae_files_format_name(NULL,1,out) && !memcmp(out,saved,sizeof(out)));
    CHECK(!ae_files_format_name("ab\0cd",5,out) && !memcmp(out,saved,sizeof(out)));
    CHECK(!ae_files_format_name("x",AE_FILES_NAME+1,out) && !memcmp(out,saved,sizeof(out)));
    CHECK(!ae_files_format_name("x",1,NULL));
    CHECK(ae_files_format_name(NULL,0,out) && !*out);
    CHECK(ae_files_format_name("a\xe2\x80\xa8\xe2\x80\xa9z",8,out) && !strcmp(out,"a\\xe2\\x80\\xa8\\xe2\\x80\\xa9z"));
    CHECK(ae_files_format_name("\\xff",4,out) && !strcmp(out,"\\x5cxff"));
    CHECK(ae_files_format_name("\xff",1,out) && !strcmp(out,"\\xff"));
    CHECK(ae_files_format_name("ümlaut",7,out) && !strcmp(out,"ümlaut"));
    char worst[AE_FILES_NAME];memset(worst,0xff,sizeof(worst));
    CHECK(ae_files_format_name(worst,sizeof(worst),out) && strlen(out)==4*AE_FILES_NAME);
    for (size_t i=0;i<sizeof(worst);++i) CHECK(!memcmp(out+4*i,"\\xff",4));
}
static void ancestors(void) {
    size_t before=descriptors();char path[]="files-ancestors-XXXXXX";CHECK(mkdtemp(path));
    int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);
    CHECK(!mkdirat(root,"outer",0700));int outer=openat(root,"outer",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(outer>=0);
    CHECK(!mkdirat(outer,"inner",0700));int inner=openat(outer,"inner",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(inner>=0);
    put(inner,"keep.txt","held");put(inner,".hidden","hidden");close(inner);close(outer);
    enum ae_files_result r;struct ae_files *f=ae_files_create(root,&r);CHECK(f && r==AE_FILES_OK);
    CHECK(ae_files_enter(f)==AE_FILES_OK && ae_files_enter(f)==AE_FILES_OK && ae_files_depth(f)==2);
    CHECK(!strcmp(ae_files_ancestor_label(f,0),"Root") && !strcmp(ae_files_ancestor_label(f,1),"outer") && !strcmp(ae_files_ancestor_label(f,2),"inner"));
    CHECK(!*ae_files_ancestor_label(f,3) && !*ae_files_ancestor_label(NULL,0));
    CHECK(ae_files_filter(f,"keep",4) && ae_files_count(f)==1 && ae_files_total(f)==2);
    const struct ae_files_entry *snapshot=ae_files_snapshot_entry(f,0);
    CHECK(snapshot && !strcmp(snapshot->name,".hidden") && ae_files_snapshot_entry(f,1) && !ae_files_snapshot_entry(f,2) && !ae_files_snapshot_entry(NULL,0));
    int held=ae_files_directory(f);CHECK(held>=4);struct stat original,current;CHECK(!fstat(held,&original));close(held);
    size_t retained=descriptors();deny=true;
    CHECK(ae_files_ancestor(f,0)==AE_FILES_NOMEM && ae_files_depth(f)==2 && ae_files_count(f)==1 && !strcmp(ae_files_query(f),"keep") && ae_files_snapshot_entry(f,0)==snapshot);
    deny=false;CHECK(descriptors()==retained);
    CHECK(ae_files_ancestor(f,3)==AE_FILES_INVALID && ae_files_depth(f)==2 && ae_files_snapshot_entry(f,0)==snapshot);
    CHECK(ae_files_ancestor(f,2)==AE_FILES_OK && !*ae_files_query(f) && ae_files_snapshot_entry(f,0)==snapshot);
    /* Rename the held ancestor and put a different tree at its old pathname.
       Navigation must retain the admitted descriptor, not reopen that name. */
    CHECK(!renameat(root,"outer",root,"moved") && !mkdirat(root,"outer",0700));
    CHECK(ae_files_ancestor(f,1)==AE_FILES_OK && ae_files_depth(f)==1 && ae_files_count(f)==1 && !strcmp(ae_files_entry(f,0)->name,"inner"));
    CHECK(ae_files_enter(f)==AE_FILES_OK);held=ae_files_directory(f);CHECK(held>=4 && !fstat(held,&current));
    CHECK(current.st_dev==original.st_dev && current.st_ino==original.st_ino);close(held);
    CHECK(ae_files_ancestor(f,0)==AE_FILES_OK && ae_files_depth(f)==0 && ae_files_count(f)==2);
    CHECK(!*ae_files_ancestor_label(f,1));
    select_name(f,"outer");CHECK(ae_files_enter(f)==AE_FILES_OK && ae_files_count(f)==0);
    CHECK(ae_files_ancestor(f,0)==AE_FILES_OK && ae_files_count(f)==2);
    ae_files_destroy(f);close(root);CHECK(descriptors()==before);
}
int main(void) {
    CHECK(getuid()!=0);names();ancestors();char path[]="files-model-XXXXXX";CHECK(mkdtemp(path));CHECK(!chmod(path,0700));int root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);
    CHECK(!mkdirat(root,"docs",0700));int docs=openat(root,"docs",O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(docs>=0);put(docs,"note.txt","inside");close(docs);
    put(root,".hidden","secret");put(root,"alpha.txt","alpha");put(root,"bad\nname","control");put(root,"quotes ' $(noop)","literal");put(root,"ümlaut.txt","unicode");CHECK(!symlinkat("/",root,"escape") && !mkfifoat(root,"pipe",0600));
    enum ae_files_result r;struct ae_files *f=ae_files_create(root,&r);CHECK(f && r==AE_FILES_OK && ae_files_total(f)==8 && ae_files_count(f)==7 && ae_files_depth(f)==0);
    CHECK(ae_files_entry(f,0)->kind==AE_FILES_DIRECTORY);select_name(f,"bad\nname");CHECK(!strcmp(ae_files_entry(f,ae_files_selected(f))->label,"bad\\x0aname"));
    select_name(f,"ümlaut.txt");CHECK(!strcmp(ae_files_entry(f,ae_files_selected(f))->label,"ümlaut.txt"));
    CHECK(ae_files_up(f)==AE_FILES_ROOT);select_name(f,"escape");CHECK(ae_files_enter(f)==AE_FILES_UNSUPPORTED);int directory=-1,witness=-1;char leaf[256];CHECK(ae_files_open_selected(f,&directory,&witness,leaf)==AE_FILES_UNSUPPORTED && directory==-1 && witness==-1);
    CHECK(ae_files_hold_selected(f,&directory,&witness,leaf)==AE_FILES_UNSUPPORTED && directory==-1 && witness==-1);
    select_name(f,"pipe");CHECK(ae_files_open_selected(f,&directory,&witness,leaf)==AE_FILES_UNSUPPORTED);
    select_name(f,"docs");CHECK(ae_files_hold_selected(f,&directory,&witness,leaf)==AE_FILES_OK && directory>=4 && witness>=4 && !strcmp(leaf,"docs"));close(directory);close(witness);
    ae_files_hidden(f,true);CHECK(ae_files_count(f)==8 && ae_files_filter(f,"alpha",5) && ae_files_count(f)==1);CHECK(!ae_files_filter(f,"\xff",1) && ae_files_count(f)==1);
    CHECK(ae_files_open_selected(f,&directory,&witness,leaf)==AE_FILES_OK && directory>=4 && witness>=4 && !strcmp(leaf,"alpha.txt"));
    CHECK(!renameat(root,"alpha.txt",root,"old-alpha"));put(root,"alpha.txt","replacement");struct ae_editor_file *file=NULL;struct ae_document *document=NULL;
    CHECK(ae_editor_file_open_selected(directory,leaf,witness,&file,&document)==AE_EDITOR_FILE_CONFLICT && !file && !document);
    int other_dir=-1,other_file=-1;char other_leaf[256];CHECK(ae_files_open_selected(f,&other_dir,&other_file,other_leaf)==AE_FILES_CHANGED && other_dir==-1 && other_file==-1);
    CHECK(!renameat(root,"old-alpha",root,"alpha.txt"));CHECK(ae_editor_file_open_selected(directory,leaf,witness,&file,&document)==AE_EDITOR_FILE_OK);
    char bytes[5];CHECK(ae_document_copy(document,0,5,bytes,sizeof(bytes)) && !memcmp(bytes,"alpha",5));CHECK(ae_document_replace(document,0,5,"new",3)==AE_DOCUMENT_OK && ae_editor_file_save(file,document)==AE_EDITOR_FILE_OK);
    ae_editor_file_close(file);ae_document_destroy(document);close(directory);close(witness);
    CHECK(ae_files_filter(f,"",0));select_name(f,"docs");deny=true;CHECK(ae_files_enter(f)==AE_FILES_NOMEM && ae_files_depth(f)==0 && ae_files_count(f)==8);deny=false;
    CHECK(!renameat(root,"docs",root,"moved") && !mkdirat(root,"docs",0700));CHECK(ae_files_enter(f)==AE_FILES_CHANGED && ae_files_depth(f)==0);
    CHECK(!renameat(root,"docs",root,"new-docs") && !renameat(root,"moved",root,"docs"));CHECK(ae_files_enter(f)==AE_FILES_OK && ae_files_depth(f)==1 && ae_files_count(f)==1);
    CHECK(!renameat(root,"docs",root,"moved"));CHECK(ae_files_refresh(f)==AE_FILES_OK && !strcmp(ae_files_entry(f,0)->name,"note.txt"));CHECK(ae_files_up(f)==AE_FILES_OK && ae_files_depth(f)==0);
    size_t count=ae_files_count(f);deny=true;CHECK(ae_files_refresh(f)==AE_FILES_NOMEM && ae_files_count(f)==count);deny=false;
    for (unsigned i=0;i<=AE_FILES_ENTRIES;++i) { char name[32];snprintf(name,sizeof(name),"limit-%04u",i);put(root,name,""); }
    CHECK(ae_files_refresh(f)==AE_FILES_LIMIT && ae_files_count(f)==count);
    for (unsigned i=0;i<=AE_FILES_ENTRIES;++i) { char name[32];snprintf(name,sizeof(name),"limit-%04u",i);struct stat st;CHECK(!fstatat(root,name,&st,AT_SYMLINK_NOFOLLOW) && S_ISREG(st.st_mode) && st.st_uid==getuid() && st.st_nlink==1 && st.st_size==0 && !unlinkat(root,name,0)); }
    CHECK(ae_files_refresh(f)==AE_FILES_OK && ae_files_count(f)==count);ae_files_destroy(f);close(root);
    printf("Files model evidence: %s\n",path);puts("files: held navigation, lossless names, filter/hidden, stale selection, witnessed editor read/save and complete-list/failure atomicity passed");return 0;
}
