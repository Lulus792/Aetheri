#define _GNU_SOURCE
#include "readline.h"
#include "completion.h"
#include "text/grapheme.h"
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <wchar.h>
static uint64_t milliseconds(void) {
    struct timespec t;if (clock_gettime(CLOCK_MONOTONIC,&t) || t.tv_sec<0) return 0;
    return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;
}
/* Match the current Aetheri cell profile. Complex/unrepresentable clusters have
   an explicit ASCII fallback; segmentation is not a shaping/emoji-width claim. */
static size_t cluster(const char *text,size_t size,char out[256],unsigned *width,unsigned maximum) {
    size_t at=0;unsigned cells=0,marks=0;bool fallback=size>=256;
    while (at<size) {
        uint32_t c;if (!ae_utf8_next(text,size,&at,&c)) return 0;
        int w=wcwidth((wchar_t)c);
        if (w<0 || (!w && (!cells || ++marks>3)) || c==0x200d || (c>=0xfe00 && c<=0xfe0f)) fallback=true;
        if (w>0) { cells+=(unsigned)w;marks=0; }
    }
    if (!fallback && cells && cells<256) {
        if (cells>maximum) { out[0]='?';*width=1;return 1; }
        memcpy(out,text,size);*width=cells;return size;
    }
    uint32_t first;at=0;if (!ae_utf8_next(text,size,&at,&first)) return 0;
    int n=snprintf(out,256,"[U+%04X%s]",(unsigned)first,at<size ? "+..." : "");
    if (n<=0 || n>=256) return 0;
    if ((unsigned)n>maximum) { out[0]='?';*width=1;return 1; }
    *width=(unsigned)n;return (size_t)n;
}
static int draw_span(struct ae_shell_terminal *t,const char *text,size_t cursor,const size_t *offsets,size_t count,
        const char *prompt,bool submit,bool blocked) {
    struct winsize window;
    if (ioctl(t->fd,TIOCGWINSZ,&window) || window.ws_col<16 || window.ws_col>4096) return -1;
    size_t prompt_size=strlen(prompt);if (prompt_size>12) return -1;
    unsigned available=(unsigned)window.ws_col-(unsigned)prompt_size-3u,start_width=0,total=0,cursor_cells=0;
    size_t start=0,end=count-1;unsigned maximum=submit ? 256u : available;
    for (size_t i=0;i+1<count;++i) {
        char bytes[256];unsigned width;size_t n=cluster(text+offsets[i],offsets[i+1]-offsets[i],bytes,&width,maximum);if (!n) return -1;
        if (offsets[i]<cursor) cursor_cells+=width;
        total+=width;
    }
    if (!submit && cursor_cells>=available) {
        unsigned target=cursor_cells-available/2;
        while (start+1<count) {
            char bytes[256];unsigned width;if (!cluster(text+offsets[start],offsets[start+1]-offsets[start],bytes,&width,maximum)) return -1;
            if (start_width+width>target) break;
            start_width+=width;++start;
        }
        /* The midpoint can lie inside a wide indivisible cluster. Keep the
           actual caret visible, even if that means skipping that whole unit. */
        while (cursor_cells-start_width>available && start+1<count) {
            char bytes[256];unsigned width;
            if (!cluster(text+offsets[start],offsets[start+1]-offsets[start],bytes,&width,maximum)) return -1;
            start_width+=width;++start;
        }
    }
    char output[(AE_LINE_BYTES+AE_LINE_SEARCH_BYTES+32)*8+256];size_t used=0;unsigned painted=0,prefix=(unsigned)prompt_size+(start ? 1u : 0u);
    memcpy(output,"\r\033[K",4);memcpy(output+4,prompt,prompt_size);used=4+prompt_size;
    if (!submit && start) output[used++]='<';
    for (size_t i=start;i<end;++i) {
        char bytes[256];unsigned width;size_t n=cluster(text+offsets[i],offsets[i+1]-offsets[i],bytes,&width,maximum);
        if (!n) return -1;
        if (!submit && width>available-painted) break;
        if (n>sizeof(output)-used-64) return -1;
        memcpy(output+used,bytes,n);used+=n;painted+=width;
    }
    if (submit) output[used++]='\n';
    else {
        if (start_width+painted<total) output[used++]='>';
        if (blocked) { const char warning[]=" [rejected: Ctrl+U/C]";size_t left=window.ws_col-1u-(prefix+painted+1u);if (left>sizeof(warning)-1) left=sizeof(warning)-1;memcpy(output+used,warning,left);used+=left; }
        int n=snprintf(output+used,sizeof(output)-used,"\r\033[%uC",prefix+cursor_cells-start_width);
        if (n<0 || (size_t)n>=sizeof(output)-used) return -1;
        used+=(size_t)n;
    }
    return ae_shell_jobs_write(t,output,used);
}
static int draw_search(struct ae_shell_terminal *t,struct ae_line_editor *e,bool blocked) {
    struct ae_line_search_view view;if (!ae_line_search_view(e,&view)) return -1;
    char text[AE_LINE_BYTES+AE_LINE_SEARCH_BYTES+32];size_t used=view.query_size;
    memcpy(text,view.query,used);memcpy(text+used," | ",3);used+=3;
    const char *match=view.found ? view.match : "(no match)";size_t size=view.found ? view.match_size : 10;
    memcpy(text+used,match,size);used+=size;
    size_t offsets[sizeof(text)+1],count;
    if (!ae_text_graphemes(text,used,offsets,sizeof(offsets)/sizeof(*offsets),&count)) return -1;
    return draw_span(t,text,view.query_size,offsets,count,"find> ",false,blocked);
}
static int draw(struct ae_shell_terminal *t,struct ae_line_editor *e,bool submit,bool blocked) {
    if (ae_line_search_active(e) && !submit) return draw_search(t,e,blocked);
    size_t size,cursor,count;const char *text=ae_line_text(e,&size,&cursor);(void)size;
    const size_t *offsets=ae_line_boundaries(e,&count);
    return draw_span(t,text,cursor,offsets,count,"aetheri> ",submit,blocked);
}
static bool completion_cancelled(void *context) {
    struct ae_shell_terminal *t=context;struct pollfd event={t->signals,POLLIN,0};
    return ae_shell_jobs_check(t)!=0 || poll(&event,1,0)!=0;
}
static int complete(struct ae_shell_terminal *t,struct ae_line_editor *e,bool *rejected) {
    size_t size,cursor;const char *text=ae_line_text(e,&size,&cursor);
    struct ae_shell_completion suggestions;
    enum ae_completion_status status=ae_shell_complete(text,size,cursor,getenv("PATH"),completion_cancelled,t,&suggestions);
    if (status==AE_COMPLETION_CANCELLED) return 0; /* Original signal read owns cancellation. */
    const char *message=NULL;
    if (status==AE_COMPLETION_READY && suggestions.replacement_size) {
        if (!ae_line_replace(e,suggestions.begin,suggestions.end,suggestions.replacement,suggestions.replacement_size)) {
            message="completion did not fit or could not be recorded";*rejected=true;
        }
        else return 0;
    } else if (status==AE_COMPLETION_READY) {
        int result=ae_shell_jobs_write(t,"\n",1);if (result) return result;
        for (size_t i=0;i<suggestions.count;++i) {
            const char *value=suggestions.values+suggestions.items[i].offset;char quoted[AE_LINE_BYTES+1];size_t used;
            if (!ae_shell_quote(value,strlen(value),quoted,sizeof(quoted),&used)) { message="completion display limit reached";break; }
            result=ae_shell_jobs_write(t,quoted,used);if (result) return result;
            result=ae_shell_jobs_write(t,"\n",1);if (result) return result;
        }
        if (!message) return 0;
    } else message=status==AE_COMPLETION_NONE ? "no completion matches" : status==AE_COMPLETION_LIMIT ? "completion limit reached; narrow the prefix" :
        status==AE_COMPLETION_IO ? "completion directory unavailable" : "completion needs a literal token at the cursor";
    char output[128];int n=snprintf(output,sizeof(output),"\n%s\n",message);
    return n<0 || (size_t)n>=sizeof(output) ? -1 : ae_shell_jobs_write(t,output,(size_t)n);
}
static bool sequence(struct ae_line_editor *e,const char *s) {
    if (ae_line_search_active(e)) {
        if (!strcmp(s,"[A") || !strcmp(s,"OA")) return ae_line_search_next(e,-1);
        if (!strcmp(s,"[B") || !strcmp(s,"OB")) return ae_line_search_next(e,1);
        return !strcmp(s,"[D") || !strcmp(s,"OD") || !strcmp(s,"[C") || !strcmp(s,"OC") ||
            !strcmp(s,"[H") || !strcmp(s,"OH") || !strcmp(s,"[F") || !strcmp(s,"OF") ||
            !strcmp(s,"[1~") || !strcmp(s,"[7~") || !strcmp(s,"[4~") || !strcmp(s,"[8~") || !strcmp(s,"[3~");
    }
    if (!strcmp(s,"[D") || !strcmp(s,"OD")) return ae_line_edit(e,AE_LINE_LEFT);
    if (!strcmp(s,"[C") || !strcmp(s,"OC")) return ae_line_edit(e,AE_LINE_RIGHT);
    if (!strcmp(s,"[A") || !strcmp(s,"OA")) return ae_line_history(e,-1);
    if (!strcmp(s,"[B") || !strcmp(s,"OB")) return ae_line_history(e,1);
    if (!strcmp(s,"[H") || !strcmp(s,"OH") || !strcmp(s,"[1~") || !strcmp(s,"[7~")) return ae_line_edit(e,AE_LINE_HOME);
    if (!strcmp(s,"[F") || !strcmp(s,"OF") || !strcmp(s,"[4~") || !strcmp(s,"[8~")) return ae_line_edit(e,AE_LINE_END);
    return !strcmp(s,"[3~") && ae_line_edit(e,AE_LINE_DELETE);
}
static bool flush(struct ae_line_editor *e,char *pending,size_t *size) {
    bool result=ae_line_search_active(e) ? ae_line_search_insert(e,pending,*size) : ae_line_insert(e,pending,*size);
    *size=0;return result;
}
ssize_t ae_shell_readline(struct ae_shell_terminal *t,struct ae_line_editor *e,char *out,size_t capacity) {
    if (!e || !out || capacity<AE_LINE_BYTES+1u || ae_shell_jobs_edit_mode(t,true)) return -1;
    ae_line_clear(e);ssize_t result=-1;char seq[32],utf8[4],pending[256];size_t seq_size=0,utf_size=0,utf_need=0,pending_size=0;
    uint64_t deadline=0;bool blocked=false;unsigned burst=0;
    int shown=draw(t,e,false,false);if (shown) { result=shown==-2 ? -2 : -1;goto done; }
    for (;;) {
        int timeout=-1;
        if (seq_size) { uint64_t now=milliseconds();if (!now) goto done;timeout=now>=deadline ? 0 : (int)(deadline-now); }
        unsigned char b;ssize_t n=ae_shell_jobs_read_timeout(t,(char *)&b,1,timeout);
        if (n==-4) {
            bool incomplete=seq_size>1;seq_size=0;
            if (!incomplete && ae_line_search_active(e)) {
                ae_line_search_cancel(e);shown=draw(t,e,false,blocked);
                if (shown) { result=shown==-2 ? -2 : -1;goto done; }
            }
            if (incomplete) {
                blocked=true;shown=draw(t,e,false,blocked);
                if (shown) { result=shown==-2 ? -2 : -1;goto done; }
            }
            continue;
        }
        if (n==-3 || n==-5) { if (pending_size && !flush(e,pending,&pending_size)) blocked=true;shown=draw(t,e,false,blocked);if (shown) { result=shown==-2 ? -2 : -1;goto done; }continue; }
        if (n==-2) { result=-2;goto done; }
        if (n<=0 || t->shutdown_signal) goto done;
        if (!blocked && pending_size && (b<32 || b==127)) blocked=!flush(e,pending,&pending_size);
        if (b==21) {
            if (blocked || seq_size || utf_need) { ae_line_clear(e);blocked=false; }
            else if (ae_line_search_active(e)) blocked=!ae_line_search_clear_query(e);
            else blocked=!ae_line_edit(e,AE_LINE_KILL_START);
            seq_size=utf_size=utf_need=pending_size=0;
        }
        else if (blocked) { if (b=='\n' || b=='\r') { const char message[]="\ninput rejected; Ctrl+U or Ctrl+C resets the whole draft\n";if (ae_shell_jobs_write(t,message,sizeof(message)-1)) goto done; } }
        else if (seq_size) {
            if (seq_size==sizeof(seq)-1) { blocked=true;seq_size=0; }
            else {
                seq[seq_size++]=(char)b;seq[seq_size]=0;
                if (seq_size==2 && b!='[' && b!='O') { blocked=true;seq_size=0; }
                else if (seq_size>2 && ((b>='A' && b<='Z') || (b>='a' && b<='z') || b=='~')) {
                    blocked=!sequence(e,seq+1);seq_size=0;
                }
            }
        } else if (b==27) {
            if (utf_need) blocked=true;
            else { seq[0]=27;seq[1]=0;seq_size=1;uint64_t now=milliseconds();if (!now || now>UINT64_MAX-120u) goto done;deadline=now+120u; }
        } else if (b>=0x80 || utf_need) {
            if (!utf_need) { utf_need=b>=0xc2 && b<=0xdf ? 2u : b>=0xe0 && b<=0xef ? 3u : b>=0xf0 && b<=0xf4 ? 4u : 0u;utf_size=0; }
            if (!utf_need || utf_size>=sizeof(utf8) || (utf_size && (b<0x80 || b>0xbf))) blocked=true;
            else { utf8[utf_size++]=(char)b;if (utf_size==utf_need) {
                if (utf_size>sizeof(pending)-pending_size) blocked=!flush(e,pending,&pending_size);
                if (!blocked) { memcpy(pending+pending_size,utf8,utf_size);pending_size+=utf_size; }
                utf_size=utf_need=0;
            } }
        } else if (b==18) {
            blocked=!(ae_line_search_active(e) ? ae_line_search_next(e,-1) : ae_line_search_begin(e));
        } else if (ae_line_search_active(e) && b<32) {
            if (b==7 || b==4) ae_line_search_cancel(e);
            else if (b=='\n' || b=='\r') (void)ae_line_search_accept(e);
            else if (b==8) blocked=!ae_line_search_backspace(e);
            else if (b==19 || b==14 || b==16) blocked=!ae_line_search_next(e,b==16 ? -1 : 1);
            else if (b!=1 && b!=2 && b!=5 && b!=6 && b!=9 && b!=12 && b!=30 && b!=31) blocked=true;
        } else if (ae_line_search_active(e) && b==127) {
            blocked=!ae_line_search_backspace(e);
        } else if (b=='\n' || b=='\r') {
            size_t size,cursor;const char *text=ae_line_text(e,&size,&cursor);(void)cursor;
            shown=draw(t,e,true,false);if (shown) { result=shown==-2 ? -2 : -1;goto done; }
            memcpy(out,text,size);out[size]='\n';(void)ae_line_remember(e);result=(ssize_t)(size+1);goto done;
        } else if (b==4) {
            size_t size,cursor;(void)ae_line_text(e,&size,&cursor);
            if (!size) { result=0;goto done; }
            blocked=!ae_line_edit(e,AE_LINE_DELETE);
        } else if (b==1) (void)ae_line_edit(e,AE_LINE_HOME);
        else if (b==2) (void)ae_line_edit(e,AE_LINE_LEFT);
        else if (b==5) (void)ae_line_edit(e,AE_LINE_END);
        else if (b==6) (void)ae_line_edit(e,AE_LINE_RIGHT);
        else if (b==8 || b==127) blocked=!ae_line_edit(e,AE_LINE_BACKSPACE);
        else if (b==11) blocked=!ae_line_edit(e,AE_LINE_KILL_END);
        else if (b==23) blocked=!ae_line_edit(e,AE_LINE_WORD_BACK);
        else if (b==25) blocked=!ae_line_edit(e,AE_LINE_YANK);
        else if (b==14 || b==16) blocked=!ae_line_history(e,b==14 ? 1 : -1);
        else if (b==31 || b==30) blocked=!ae_line_undo(e,b==31 ? -1 : 1);
        else if (b==9) { shown=complete(t,e,&blocked);if (shown) { result=shown==-2 ? -2 : -1;goto done; } }
        else if (b!=12) {
            if (b<32) blocked=true;
            else {
                if (pending_size==sizeof(pending)) blocked=!flush(e,pending,&pending_size);
                if (!blocked) pending[pending_size++]=(char)b;
            }
        }
        struct pollfd readiness={t->edit_fd,POLLIN,0};int available=poll(&readiness,1,0);
        if (available<0) goto done;
        if (++burst>=64 || available==0 || !(readiness.revents&POLLIN)) {
            if (pending_size && !flush(e,pending,&pending_size)) blocked=true;
            burst=0;shown=draw(t,e,false,blocked);if (shown) { result=shown==-2 ? -2 : -1;goto done; }
        }
    }
done:
    if (result==-2) t->interrupted=false;
    if (result==-2 && (ae_shell_jobs_check(t) || tcflush(t->edit_fd,TCIFLUSH))) result=-1;
    if (result==-1 && !t->shutdown_signal) { t->failed=true;t->shutdown_signal=SIGHUP; }
    if (ae_shell_jobs_edit_mode(t,false)) return -1;
    return result;
}
