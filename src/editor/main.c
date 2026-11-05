#define _GNU_SOURCE
#include "file.h"
#include "view.h"
#include "text/grapheme.h"
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
enum { COMMAND_BYTES=4096, PENDING_EXPORTS=8 };
enum interaction { EDIT, COMMAND, CLOSE, DISCARD, SEARCH };
struct app {
    struct ae_document *document;struct ae_modal *modal;struct ae_editor_view *view;
    struct ae_editor_file *file,*pending[PENDING_EXPORTS];size_t pending_count;
    int tty,signals;pid_t group,session;struct termios modes,raw;sigset_t old_mask;bool masked,raw_set,alternate,quit,help,compact,dd,command_bad,quit_after_save,direct;
    enum interaction interaction;char title[COMMAND_BYTES+1],command[COMMAND_BYTES+1],message[256];size_t command_size;
    char utf8[4],sequence[32];size_t utf_size,utf_need,seq_size;bool seq_bad;uint64_t deadline;int result;unsigned help_offset;
    size_t search_origin,last_query_size;int search_direction,last_direction;
    char last_query[AE_DOCUMENT_QUERY_BYTES+1];bool search_found,search_dirty;
};
static uint64_t milliseconds(void) { struct timespec t;if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u; }
static void notice(struct app *a,const char *text) { (void)snprintf(a->message,sizeof(a->message),"%s",text); }
static bool uncertain(const struct app *a) { return a->pending_count || ae_editor_file_uncertain(a->file); }
static bool owns(const struct app *a) { return tcgetsid(a->tty)==a->session && tcgetpgrp(a->tty)==a->group; }
/* Independent /dev/tty OFD: never set O_NONBLOCK on a command/shell descriptor. */
static bool write_tty(struct app *a,const char *bytes,size_t size,bool cleanup) {
    size_t at=0;uint64_t end=milliseconds()+5000;
    while (at<size) {
        ssize_t n=write(a->tty,bytes+at,size-at);if (n>0) { at+=(size_t)n;continue; }
        if (n<0 && errno==EINTR) continue;
        if (n>=0 || (errno!=EAGAIN && errno!=EWOULDBLOCK) || milliseconds()>=end) return false;
        struct pollfd fds[2]={{a->tty,POLLOUT,0},{a->signals,POLLIN,0}};
        int r=poll(fds,cleanup ? 1u : 2u,100);if (r<0 && errno!=EINTR) return false;
        if (!cleanup && fds[1].revents) { errno=EINTR;return false; }
        if (fds[0].revents&(POLLHUP|POLLERR|POLLNVAL)) return false;
    }
    return true;
}
static void screen_leave(struct app *a) {
    if (a->tty>=0 && owns(a)) {
        if (a->alternate) (void)write_tty(a,"\033[0m\033[?25h\033[?1049l",sizeof("\033[0m\033[?25h\033[?1049l")-1,true);
        if (a->raw_set) (void)tcsetattr(a->tty,TCSANOW,&a->modes);
    }
    a->alternate=a->raw_set=false;
}
static bool screen_enter(struct app *a) {
    if (!owns(a) || tcsetattr(a->tty,TCSANOW,&a->raw)) return false;
    a->raw_set=true;a->alternate=true;
    return write_tty(a,"\033[?1049h",sizeof("\033[?1049h")-1,false);
}
static bool terminal_open(struct app *a) {
    a->group=getpgrp();a->session=getsid(0);
    if (a->group!=getpid() || tcgetsid(0)!=a->session || tcgetpgrp(0)!=a->group || tcgetattr(0,&a->modes) || !(a->modes.c_lflag&ICANON) || !(a->modes.c_lflag&ISIG)) return false;
    a->tty=open("/dev/tty",O_RDWR|O_CLOEXEC|O_NONBLOCK);
    if (a->tty<0 || !owns(a)) return false;
    sigset_t mask;sigemptyset(&mask);static const int signals[]={SIGINT,SIGQUIT,SIGHUP,SIGTERM,SIGWINCH,SIGTSTP,SIGCONT,SIGTTIN,SIGTTOU};
    for (size_t i=0;i<sizeof(signals)/sizeof(*signals);++i) sigaddset(&mask,signals[i]);
    if (sigprocmask(SIG_BLOCK,&mask,&a->old_mask)) return false;
    a->masked=true;a->signals=signalfd(-1,&mask,SFD_CLOEXEC|SFD_NONBLOCK);if (a->signals<0) return false;
    a->raw=a->modes;cfmakeraw(&a->raw);return screen_enter(a);
}
static void terminal_close(struct app *a) {
    screen_leave(a);if (a->signals>=0) (void)close(a->signals);if (a->tty>=0) (void)close(a->tty);
    if (a->masked) (void)sigprocmask(SIG_SETMASK,&a->old_mask,NULL);
}
static enum ae_editor_file_result bind_path(const char *path,bool creation,struct ae_editor_file **file,struct ae_document **document) {
    size_t length=strlen(path);if (!length || length>COMMAND_BYTES || path[length-1]=='/') return AE_EDITOR_FILE_INVALID;
    char parent[COMMAND_BYTES+1];memcpy(parent,path,length+1);char *slash=strrchr(parent,'/');const char *leaf=path;int fd;
    if (slash) { leaf=path+(slash-parent)+1;if (slash==parent) slash[1]=0;else *slash=0;fd=open(parent,O_RDONLY|O_DIRECTORY|O_CLOEXEC); }
    else fd=open(".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if (fd<0) return AE_EDITOR_FILE_IO;
    enum ae_editor_file_result r=creation ? ae_editor_file_new(fd,leaf,file,document) : ae_editor_file_open(fd,leaf,file,document);(void)close(fd);return r;
}
static const char *file_error(enum ae_editor_file_result r) {
    switch (r) {
    case AE_EDITOR_FILE_OK:return "Saved";
    case AE_EDITOR_FILE_CONFLICT:return "File changed or name exists; export to a new path";
    case AE_EDITOR_FILE_UNKNOWN:return "SAVE UNKNOWN: no retry; :export NEW-PATH or close choices";
    case AE_EDITOR_FILE_UNSUPPORTED:return "Unsupported file, permissions or metadata; target not replaced";
    case AE_EDITOR_FILE_NOMEM:return "Not enough memory; target not replaced";
    case AE_EDITOR_FILE_INVALID:return "Invalid file/path";
    case AE_EDITOR_FILE_IO:return "I/O failed before replacement; document retained";
    }
    return "File operation failed";
}
static bool save(struct app *a) {
    ae_document_history_break(a->document);
    if (uncertain(a)) { notice(a,file_error(AE_EDITOR_FILE_UNKNOWN));return false; }
    if (!a->file) { a->interaction=COMMAND;memcpy(a->command,"saveas ",8);a->command_size=7;notice(a,"Choose a new path; existing names are never overwritten");return false; }
    enum ae_editor_file_result r=ae_editor_file_save(a->file,a->document);notice(a,file_error(r));return r==AE_EDITOR_FILE_OK;
}
static void save_close(struct app *a) {
    if (save(a)) a->quit=true;
    else a->quit_after_save=!a->file && a->interaction==COMMAND;
}
static bool copy_to(struct app *a,const char *path,bool adopt) {
    ae_document_history_break(a->document);
    if ((adopt && uncertain(a)) || a->pending_count==PENDING_EXPORTS) { notice(a,"Unsettled saves retained; no new save binding allowed");return false; }
    struct ae_editor_file *file=NULL;struct ae_document *copy=NULL;
    enum ae_editor_file_result r=bind_path(path,true,&file,&copy);
    if (r==AE_EDITOR_FILE_OK) {
        struct ae_document_info info;struct ae_document_slice text;enum ae_document_result result;
        if (!ae_document_info(a->document,&info) || !ae_document_slice(a->document,0,info.size,&text)) r=AE_EDITOR_FILE_INVALID;
        else {
            char *bytes=info.size ? malloc(info.size) : NULL;
            if (info.size && !bytes) r=AE_EDITOR_FILE_NOMEM;
            else if (!ae_document_copy(a->document,0,info.size,bytes,info.size)) r=AE_EDITOR_FILE_INVALID;
            else {
                struct ae_document *replacement=ae_document_create(bytes,info.size,&result);
                if (!replacement) r=result==AE_DOCUMENT_NOMEM ? AE_EDITOR_FILE_NOMEM : AE_EDITOR_FILE_UNSUPPORTED;
                else { ae_document_destroy(copy);copy=replacement;r=ae_editor_file_save(file,copy); }
            }
            free(bytes);
        }
    }
    if (r==AE_EDITOR_FILE_OK && adopt) {
        struct ae_document_info info;if (!ae_document_info(a->document,&info) || !ae_document_mark_clean(a->document,info.revision)) r=AE_EDITOR_FILE_UNKNOWN;
        else { ae_editor_file_close(a->file);a->file=file;file=NULL;(void)snprintf(a->title,sizeof(a->title),"%s",path); }
    }
    if (r==AE_EDITOR_FILE_UNKNOWN && file) { a->pending[a->pending_count++]=file;file=NULL; }
    ae_editor_file_close(file);ae_document_destroy(copy);
    notice(a,r==AE_EDITOR_FILE_OK && !adopt ? "Copy saved; original document/save state retained" : file_error(r));return r==AE_EDITOR_FILE_OK;
}
static void close_request(struct app *a) {
    ae_document_history_break(a->document);
    struct ae_document_info info;if (!ae_document_info(a->document,&info)) return;
    if (!info.dirty && !uncertain(a)) a->quit=true;else { a->interaction=CLOSE;a->help=false; }
}
static void command_start(struct app *a,const char *prefix,enum interaction mode) {
    ae_document_history_break(a->document);
    a->interaction=mode;a->command_bad=false;a->command_size=strlen(prefix);memcpy(a->command,prefix,a->command_size+1);a->dd=false;a->help=false;
}
static void command_run(struct app *a) {
    if (a->command_bad) { notice(a,"Rejected command; Esc cancels or Ctrl+U clears");return; }
    a->interaction=EDIT;
    if (!strcmp(a->command,"w")) (void)save(a);
    else if (!strcmp(a->command,"wq")) save_close(a);
    else if (!strcmp(a->command,"q")) close_request(a);
    else if (!strcmp(a->command,"q!")) command_start(a,"",DISCARD);
    else if (!strcmp(a->command,"help")) { a->help=true;a->help_offset=0; }
    else if (!strncmp(a->command,"saveas ",7)) { if (copy_to(a,a->command+7,true) && a->quit_after_save) a->quit=true; }
    else if (!strncmp(a->command,"export ",7)) (void)copy_to(a,a->command+7,false);
    else notice(a,"Unknown command; :help lists commands");
}
static void edit_result(struct app *a,enum ae_document_result r) {
    if (r==AE_DOCUMENT_OK) notice(a,"");
    else notice(a,r==AE_DOCUMENT_NOMEM ? "Not enough memory; edit not applied" : r==AE_DOCUMENT_LIMIT ? "Document/line limit; edit not applied" : "Edit not supported in this mode");
}
static void action(struct app *a,enum ae_modal_action command) { a->dd=false;edit_result(a,ae_modal_action(a->modal,command)); }
static void history(struct app *a,int direction) {
    bool changed=false;a->dd=false;enum ae_document_result r=ae_modal_history(a->modal,direction,&changed);
    if (r!=AE_DOCUMENT_OK) edit_result(a,r);
    else notice(a,changed ? direction<0 ? "Undone" : "Redone" : direction<0 ? "No earlier edit" : "No later edit");
}
static void transfer(struct app *a,enum ae_modal_transfer command) {
    a->dd=false;struct ae_modal_selection selection;struct ae_modal_register_info reg;
    if (!ae_modal_selection(a->modal,&selection)) return;
    enum ae_document_result r=ae_modal_transfer(a->modal,command);
    if (r!=AE_DOCUMENT_OK) { edit_result(a,r);return; }
    if (selection.active && selection.begin==selection.end) notice(a,"Empty selection; editor register retained");
    else if (ae_modal_register_info(a->modal,&reg) && !reg.size) notice(a,"Editor register is empty");
    else notice(a,command==AE_MODAL_YANK ? "Copied in editor" : command==AE_MODAL_CUT ? "Cut in editor" : command==AE_MODAL_CHANGE ? "Replace selection; Esc returns to Normal" : "Pasted from editor register");
}
static void search_preview(struct app *a) {
    if (!a->search_dirty) return;
    a->search_dirty=false;a->search_found=false;size_t position=0;
    enum ae_document_result r=AE_DOCUMENT_INVALID;
    if (!a->command_bad && a->command_size) r=ae_document_find(a->document,a->command,a->command_size,a->search_origin,a->search_direction,true,&a->search_found,&position);
    if (r!=AE_DOCUMENT_OK) a->search_found=false;
    if (!ae_modal_seek(a->modal,a->search_found ? position : a->search_origin)) { a->quit=true;a->result=1; }
}
static void search_start(struct app *a,int direction) {
    if (ae_modal_mode(a->modal)!=AE_MODAL_NORMAL && ae_modal_action(a->modal,AE_MODAL_ESCAPE)!=AE_DOCUMENT_OK) return;
    struct ae_document_info info;if (!ae_document_info(a->document,&info)) return;
    command_start(a,"",SEARCH);a->search_origin=info.cursor;a->search_direction=direction;a->search_dirty=true;
}
static void search_repeat(struct app *a,bool opposite) {
    if (ae_modal_mode(a->modal)!=AE_MODAL_NORMAL && ae_modal_action(a->modal,AE_MODAL_ESCAPE)!=AE_DOCUMENT_OK) return;
    if (!a->last_query_size) { notice(a,"No accepted search");return; }
    struct ae_document_info info;if (!ae_document_info(a->document,&info)) return;
    bool found=false;size_t position=0;int direction=opposite ? -a->last_direction : a->last_direction;
    enum ae_document_result r=ae_document_find(a->document,a->last_query,a->last_query_size,info.cursor,direction,false,&found,&position);
    if (r==AE_DOCUMENT_OK && found && ae_modal_seek(a->modal,position)) notice(a,"Match; n next, N reverse");
    else notice(a,"No further match; search does not wrap");
}
static bool text_entry(const struct app *a) { return a->interaction==COMMAND || a->interaction==DISCARD || a->interaction==SEARCH; }
static void reject_entry(struct app *a) { if (text_entry(a)) { a->command_bad=true;a->search_dirty=true; } }
static void escape(struct app *a) {
    if (a->interaction==SEARCH && !ae_modal_seek(a->modal,a->search_origin)) { a->quit=true;a->result=1; }
    notice(a,"");
    a->quit_after_save=false;
    a->command_size=0;a->command[0]=0;a->command_bad=false;a->dd=false;a->help=false;
    if (a->interaction==EDIT) action(a,AE_MODAL_ESCAPE);else a->interaction=EDIT;
}
static void text(struct app *a,const char *bytes,size_t size) {
    if (text_entry(a)) {
        if (a->command_bad) return;
        size_t limit=a->interaction==SEARCH ? AE_DOCUMENT_QUERY_BYTES : COMMAND_BYTES;
        if (size>limit-a->command_size) { reject_entry(a);notice(a,"Input too long; Esc or Ctrl+U");return; }
        memcpy(a->command+a->command_size,bytes,size);a->command_size+=size;a->command[a->command_size]=0;a->search_dirty=true;return;
    }
    if (a->interaction==CLOSE) {
        if (size!=1) return;
        if (*bytes=='s' && !uncertain(a)) { a->interaction=EDIT;save_close(a); }
        else if (*bytes=='e' && uncertain(a)) command_start(a,"export ",COMMAND);
        else if (*bytes=='d') command_start(a,"",DISCARD);
        return;
    }
    if (a->help) { if (size==1 && *bytes=='j' && a->help_offset<128) ++a->help_offset;else if (size==1 && *bytes=='k' && a->help_offset) --a->help_offset;return; }
    if (a->compact) return;
    if (ae_modal_mode(a->modal)==AE_MODAL_INSERT) { edit_result(a,ae_modal_text(a->modal,bytes,size));return; }
    if (size!=1) { a->dd=false;return; }
    char c=*bytes;bool dd=a->dd;a->dd=false;
    switch (c) {
    case 'v':case 'V':if (!ae_modal_select(a->modal,c=='V')) notice(a,"Selection not available");else notice(a,"");break;
    case 'y':transfer(a,AE_MODAL_YANK);break;
    case 'c':transfer(a,AE_MODAL_CHANGE);break;
    case 'p':transfer(a,AE_MODAL_PASTE_AFTER);break;case 'P':transfer(a,AE_MODAL_PASTE_BEFORE);break;
    case '/':search_start(a,1);break;case '?':search_start(a,-1);break;
    case 'n':search_repeat(a,false);break;case 'N':search_repeat(a,true);break;
    case 'u':history(a,-1);break;
    case ':':command_start(a,"",COMMAND);break;
    case 'h':action(a,AE_MODAL_LEFT);break;case 'j':action(a,AE_MODAL_DOWN);break;
    case 'k':action(a,AE_MODAL_UP);break;case 'l':action(a,AE_MODAL_RIGHT);break;
    case '0':action(a,AE_MODAL_HOME);break;case '$':action(a,AE_MODAL_END);break;
    case 'i':action(a,AE_MODAL_ENTER_INSERT);break;case 'a':action(a,AE_MODAL_APPEND);break;
    case 'I':action(a,AE_MODAL_LINE_INSERT);break;case 'A':action(a,AE_MODAL_LINE_APPEND);break;
    case 'o':action(a,AE_MODAL_OPEN_AFTER);break;case 'O':action(a,AE_MODAL_OPEN_BEFORE);break;
    case 'x':action(a,AE_MODAL_DELETE);break;case 'd':if (ae_modal_mode(a->modal)==AE_MODAL_VISUAL || ae_modal_mode(a->modal)==AE_MODAL_LINES) transfer(a,AE_MODAL_CUT);else if (dd) action(a,AE_MODAL_DELETE_LINE);else a->dd=true;break;
    default:break;
    }
}
static bool sequence(struct app *a) {
    const char *s=a->sequence;
    if (a->interaction!=EDIT || a->help || a->compact) return true;
    enum ae_modal_action act;
    if (!strcmp(s,"[A") || !strcmp(s,"OA")) act=AE_MODAL_UP;
    else if (!strcmp(s,"[B") || !strcmp(s,"OB")) act=AE_MODAL_DOWN;
    else if (!strcmp(s,"[C") || !strcmp(s,"OC")) act=AE_MODAL_RIGHT;
    else if (!strcmp(s,"[D") || !strcmp(s,"OD")) act=AE_MODAL_LEFT;
    else if (!strcmp(s,"[H") || !strcmp(s,"OH") || !strcmp(s,"[1~") || !strcmp(s,"[7~")) act=AE_MODAL_HOME;
    else if (!strcmp(s,"[F") || !strcmp(s,"OF") || !strcmp(s,"[4~") || !strcmp(s,"[8~")) act=AE_MODAL_END;
    else if (!strcmp(s,"[3~")) act=AE_MODAL_DELETE;
    else return false;
    action(a,act);return true;
}
static void input(struct app *a,unsigned char b) {
    if (a->compact && b!=27 && b!=3 && b!=7 && b!=17) { a->seq_size=a->utf_size=a->utf_need=0;return; }
    if (b==27 && a->utf_need) { a->utf_need=a->utf_size=0;reject_entry(a); }
    if (a->seq_size) {
        if (a->seq_size==1) {
            if (b!='[' && b!='O') { a->seq_size=0;escape(a);input(a,b);return; }
            a->sequence[0]=(char)b;a->seq_size=2;return;
        }
        size_t at=a->seq_size-1;if (at+1<sizeof(a->sequence)) { a->sequence[at]=(char)b;a->sequence[at+1]=0; }else a->seq_bad=true;
        if (a->seq_size<sizeof(a->sequence)) ++a->seq_size;
        if (b>=0x40 && b<=0x7e) { if (a->seq_bad || !sequence(a)) notice(a,"Unsupported key sequence");a->seq_size=0;a->seq_bad=false; }
        return;
    }
    if (a->utf_need) {
        if ((b&0xc0)!=0x80) { a->utf_need=a->utf_size=0;reject_entry(a);notice(a,"Invalid UTF-8 input rejected");return; }
        a->utf8[a->utf_size++]=(char)b;
        if (a->utf_size==a->utf_need) {
            size_t at=0;uint32_t c;if (!ae_utf8_next(a->utf8,a->utf_size,&at,&c) || (c>=0x7f && c<=0x9f)) { reject_entry(a);notice(a,"Invalid/control input rejected"); }
            else text(a,a->utf8,a->utf_size);
            a->utf_need=a->utf_size=0;
        }
        return;
    }
    if (b==27) { a->seq_size=1;a->deadline=milliseconds()+120;return; }
    if (b>=128) { if (b>=0xc2 && b<=0xf4) { a->utf8[0]=(char)b;a->utf_size=1;a->utf_need=b<0xe0 ? 2u : b<0xf0 ? 3u : 4u; }else { notice(a,"Invalid UTF-8 input rejected");reject_entry(a); }return; }
    if (b==3 || b==7) { escape(a);return; }
    if (a->interaction==EDIT && !a->help && (b==30 || b==31 || (b==18 && ae_modal_mode(a->modal)==AE_MODAL_NORMAL))) { history(a,b==31 ? -1 : 1);return; }
    if (b==19 && a->interaction!=DISCARD) { if (a->interaction==SEARCH) escape(a);(void)save(a);return; }
    if (b==17 && a->interaction!=DISCARD) { if (a->interaction==SEARCH) escape(a);close_request(a);return; }
    if (b=='\r' || b=='\n') {
        if (a->interaction==SEARCH) {
            search_preview(a);
            if (!a->command_bad && a->search_found) {
                memcpy(a->last_query,a->command,a->command_size+1);a->last_query_size=a->command_size;a->last_direction=a->search_direction;
                a->interaction=EDIT;notice(a,"Match; n next, N reverse");
            }
        }
        else if (a->interaction==COMMAND) command_run(a);
        else if (a->interaction==DISCARD) { if (!a->command_bad && !strcmp(a->command,"DISCARD")) { a->quit=true;a->result=uncertain(a) ? 2 : 0; }else notice(a,"Type DISCARD exactly; Esc cancels"); }
        else if (a->interaction==EDIT && !a->help && !a->compact && ae_modal_mode(a->modal)==AE_MODAL_INSERT) action(a,AE_MODAL_NEWLINE);
        return;
    }
    if (b==21 && text_entry(a)) { a->command_size=0;a->command[0]=0;a->command_bad=false;a->search_dirty=true;return; }
    if (b==127 || b==8) {
        if (text_entry(a)) { if (a->command_size && !a->command_bad) { size_t points[COMMAND_BYTES+1],count;if (ae_text_graphemes(a->command,a->command_size,points,COMMAND_BYTES+1,&count)) { a->command_size=points[count-2];a->command[a->command_size]=0;a->search_dirty=true; } } }
        else if (a->interaction==EDIT && !a->help && !a->compact) action(a,AE_MODAL_BACKSPACE);
        return;
    }
    if (b=='\t' && a->interaction==EDIT && ae_modal_mode(a->modal)==AE_MODAL_INSERT) { text(a,"\t",1);return; }
    if (b<32) { reject_entry(a);notice(a,"Unsupported control; Esc cancels");return; }
    char c=(char)b;text(a,&c,1);
}
static bool draw(struct app *a) {
    errno=0;
    struct winsize w;if (ioctl(a->tty,TIOCGWINSZ,&w)) return false;
    if (a->interaction==SEARCH) search_preview(a);
    const char *dialog=a->interaction==CLOSE ? uncertain(a) ? "[e] Copy [d] Drop [Esc] Back" : "[s] Save [d] Drop [Esc] Back" : a->interaction==DISCARD ? "Type DISCARD; Esc cancels" : a->command_bad ? "REJECTED: Esc / Ctrl+U" : NULL;
    if (a->interaction==SEARCH && !a->command_bad) dialog=!a->command_size ? "Search: type text; Esc back" : a->search_found ? "Match: Enter choose; Esc back" : "No match; Esc back; Ctrl+U";
    struct ae_editor_view_state state={.title=*a->title ? a->title : NULL,.message=a->message,.command=text_entry(a) ? a->command : NULL,.dialog=dialog,.uncertain=uncertain(a),.help=a->help,.help_offset=a->help_offset,.command_prefix=a->interaction==SEARCH ? a->search_direction>0 ? '/' : '?' : ':',.highlight_size=a->interaction==SEARCH && !a->command_bad && a->search_found ? a->command_size : 0};
    struct ae_editor_frame frame;if (!ae_editor_view_render(a->view,a->modal,&state,w.ws_col,w.ws_row,&frame)) { errno=EINVAL;return false; }
    if (a->help && a->help_offset>frame.help_max) a->help_offset=frame.help_max;
    a->compact=frame.compact;return write_tty(a,frame.bytes,frame.size,false);
}
static void emergency(struct app *a) {
    struct ae_document_info info;if (!ae_document_info(a->document,&info) || (!info.dirty && !uncertain(a))) return;
    unsigned char random[12];char name[64]=".aetheri-editor-recovery-";static const char hex[]="0123456789abcdef";
    if (getrandom(random,sizeof(random),0)!=(ssize_t)sizeof(random)) { dprintf(2,"aetheri-editor: emergency copy failed; original save state unresolved\n");return; }
    size_t begin=strlen(name);for (size_t i=0;i<sizeof(random);++i) { name[begin+2*i]=hex[random[i]>>4];name[begin+2*i+1]=hex[random[i]&15]; }name[begin+2*sizeof(random)]=0;
    if (copy_to(a,name,false)) dprintf(2,"aetheri-editor: emergency copy retained in current directory: %s\n",name);
    else dprintf(2,"aetheri-editor: emergency copy failed; document/save state unresolved\n");
}
static void suspend_app(struct app *a) {
    if (a->direct) { notice(a,"Document window has no shell job control");return; }
    ae_document_history_break(a->document);
    screen_leave(a);a->seq_size=a->utf_size=a->utf_need=0;
    do { if (raise(SIGSTOP)) { emergency(a);a->result=1;a->quit=true;return; } } while (!owns(a));
    if (!screen_enter(a)) { emergency(a);a->result=1;a->quit=true; }
}
static bool signals(struct app *a) {
    struct signalfd_siginfo s;ssize_t n;
    while ((n=read(a->signals,&s,sizeof(s)))==(ssize_t)sizeof(s)) {
        /* A decided exit owns the remaining lifetime. Drain queued signals
           without another copy, cancellation, stop or exit-result rewrite. */
        if (a->quit) continue;
        if (s.ssi_signo==SIGHUP || s.ssi_signo==SIGTERM) { emergency(a);a->result=128+(int)s.ssi_signo;a->quit=true; }
        else if (s.ssi_signo==SIGINT || s.ssi_signo==SIGQUIT) escape(a);
        else if (s.ssi_signo==SIGTSTP) suspend_app(a);
        else if (s.ssi_signo==SIGTTIN || s.ssi_signo==SIGTTOU) { a->result=1;a->quit=true; }
    }
    return n<0 && (errno==EAGAIN || errno==EINTR);
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--help")) { puts("Usage: aetheri-editor [--new] [PATH]\nDevelopment handoff: --selected-fds LEAF (directory FD 3, file witness FD 4).\nCtrl+S save; Ctrl+Q close; Esc Normal; :help keyboard guide.\nLocal development profile; no document grants or durable crash recovery.");return 0; }
    bool selected=argc==3 && !strcmp(argv[1],"--selected-fds");
    if (!selected && (argc>3 || (argc==3 && strcmp(argv[1],"--new")) || (argc==2 && !strcmp(argv[1],"--new")))) { fputs("Usage: aetheri-editor [--new] [PATH]\n",stderr);return 2; }
    if (!setlocale(LC_CTYPE,"C.UTF-8")) return 1;
    struct app a={.tty=-1,.signals=-1,.direct=selected};enum ae_editor_file_result opened=AE_EDITOR_FILE_OK;
    const char *path=argc==3 ? argv[2] : argc==2 ? argv[1] : NULL;
    if (selected) { opened=ae_editor_file_open_selected(3,path,4,&a.file,&a.document);int first=close(3),second=close(4);if ((first || second) && opened==AE_EDITOR_FILE_OK) opened=AE_EDITOR_FILE_IO;if (opened==AE_EDITOR_FILE_OK) (void)snprintf(a.title,sizeof(a.title),"%s",path); }
    else if (path) { opened=bind_path(path,argc==3,&a.file,&a.document);if (opened==AE_EDITOR_FILE_OK) (void)snprintf(a.title,sizeof(a.title),"%s",path); }
    else { enum ae_document_result r;a.document=ae_document_create(NULL,0,&r); }
    if (opened!=AE_EDITOR_FILE_OK || !a.document) { fprintf(stderr,"aetheri-editor: %s\n",file_error(opened==AE_EDITOR_FILE_OK ? AE_EDITOR_FILE_NOMEM : opened));ae_editor_file_close(a.file);ae_document_destroy(a.document);return 1; }
    a.modal=ae_modal_create(a.document);a.view=ae_editor_view_create();
    if (!a.modal || !a.view || !terminal_open(&a)) { fputs("aetheri-editor: requires an owned foreground canonical terminal\n",stderr);a.result=1;goto done; }
    while (!a.quit) {
        if (!owns(&a)) { emergency(&a);a.result=1;break; }
        if (!draw(&a) && errno!=EINTR) { emergency(&a);a.result=1;break; }
        struct pollfd fds[2]={{a.tty,POLLIN,0},{a.signals,POLLIN,0}};int timeout=-1;
        if (a.seq_size) { uint64_t now=milliseconds();timeout=now>=a.deadline ? 0 : (int)(a.deadline-now); }
        int r=poll(fds,2,timeout);if (r<0) { if (errno==EINTR) continue;emergency(&a);a.result=1;break; }
        if (!r && a.seq_size) { bool lone=a.seq_size==1;a.seq_size=0;a.seq_bad=false;if (lone) escape(&a);else notice(&a,"Incomplete key sequence discarded");continue; }
        if (fds[1].revents && !signals(&a)) { emergency(&a);a.result=1;break; }
        if (a.quit) break;
        if (fds[0].revents&(POLLHUP|POLLERR|POLLNVAL)) { emergency(&a);a.result=1;break; }
        if (fds[0].revents&POLLIN) {
            for (unsigned batch=0;batch<128 && !a.quit;++batch) {
                /* Drain a bounded burst but never read beyond a completed quit:
                   following bytes still belong to the shell or next program. */
                unsigned char b;ssize_t n=read(a.tty,&b,1);
                if (n==1) { if (b==26 && !a.seq_size && !a.utf_need && (a.interaction==EDIT || a.interaction==SEARCH)) { if (a.interaction==SEARCH) escape(&a);suspend_app(&a); }else input(&a,b); }
                else if (!n || (errno!=EAGAIN && errno!=EINTR)) { emergency(&a);a.result=1;a.quit=true;break; }
                else break;
            }
        }
    }
done:
    terminal_close(&a);for (size_t i=0;i<a.pending_count;++i) ae_editor_file_close(a.pending[i]);
    ae_editor_file_close(a.file);ae_editor_view_destroy(a.view);ae_modal_destroy(a.modal);ae_document_destroy(a.document);return a.result;
}
