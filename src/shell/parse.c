#include "parse.h"
#include <stdbool.h>
#include <string.h>
struct lexer {
    const char *source; size_t length, at, words;
    ae_shell_lookup lookup; void *context; struct ae_shell_program *out;
    enum ae_shell_error error;
};
enum token { END, WORD, PIPE, INPUT, OUTPUT, APPEND, ERROR_OUTPUT, ERROR_APPEND, BAD };
static bool initial(unsigned char c) { return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_'; }
static bool following(unsigned char c) { return initial(c) || (c>='0' && c<='9'); }
int ae_shell_name(const char *name) {
    if (!name || !initial((unsigned char)*name)) return 0;
    for (++name;*name;++name) if (!following((unsigned char)*name)) return 0;
    return 1;
}
static bool put(struct lexer *l,char c) {
    if (l->out->used==AE_SHELL_BYTES) { l->error=AE_SHELL_LIMIT; return false; }
    l->out->bytes[l->out->used++]=c; return true;
}
static bool variable(struct lexer *l) {
    ++l->at; bool braced=l->at<l->length && l->source[l->at]=='{';
    if (braced) ++l->at;
    char name[256]; size_t n=0;
    if (l->at<l->length && l->source[l->at]=='?' && !braced) name[n++]=l->source[l->at++];
    else {
        if (l->at==l->length || !initial((unsigned char)l->source[l->at])) return false;
        while (l->at<l->length && following((unsigned char)l->source[l->at])) {
            if (n==sizeof(name)-1) { l->error=AE_SHELL_LIMIT; return false; }
            name[n++]=l->source[l->at++];
        }
    }
    if (braced && (l->at==l->length || l->source[l->at++]!='}')) return false;
    name[n]=0; const char *value=l->lookup ? l->lookup(l->context,name) : NULL;
    if (!value) { l->error=AE_SHELL_UNSET; return false; }
    for (;*value;++value) if (!put(l,*value)) return false;
    return true;
}
static enum token token(struct lexer *l,char **value) {
    while (l->at<l->length && (l->source[l->at]==' ' || l->source[l->at]=='\t')) ++l->at;
    if (l->at==l->length || l->source[l->at]=='#') return END;
    char c=l->source[l->at];
    if (c=='|') { ++l->at; return PIPE; }
    if (c=='<') { ++l->at; return INPUT; }
    bool err=c=='2' && l->at+1<l->length && l->source[l->at+1]=='>';
    if (c=='>' || err) {
        l->at+=err ? 2u : 1u;
        bool append=l->at<l->length && l->source[l->at]=='>';
        if (append) ++l->at;
        return err ? (append ? ERROR_APPEND : ERROR_OUTPUT) : (append ? APPEND : OUTPUT);
    }
    if (++l->words>AE_SHELL_WORDS) { l->error=AE_SHELL_LIMIT; return BAD; }
    size_t start=l->out->used; char quote=0; bool seen=false;
    while (l->at<l->length) {
        c=l->source[l->at];
        if (!quote && (c==' ' || c=='\t' || c=='|' || c=='<' || c=='>')) break;
        if (!quote && (c==';' || c=='&' || c=='(' || c==')' || c=='`')) return BAD;
        if (c=='\'' || c=='"') {
            if (!quote) { quote=c; seen=true; ++l->at; continue; }
            if (quote==c) { quote=0; ++l->at; continue; }
        }
        if (c=='\\' && quote!='\'') {
            ++l->at;
            if (l->at==l->length) return BAD;
            c=l->source[l->at];
            if (quote=='"' && c!='"' && c!='\\' && c!='$' && !put(l,'\\')) return BAD;
        } else if (c=='$' && quote!='\'') {
            if (!variable(l)) return BAD;
            seen=true; continue;
        }
        if (!put(l,c)) return BAD;
        seen=true; ++l->at;
    }
    if (quote || !seen || !put(l,0)) return BAD;
    *value=l->out->bytes+start; return WORD;
}
enum ae_shell_error ae_shell_parse(const char *source,size_t length,ae_shell_lookup lookup,
        void *context,struct ae_shell_program *out,size_t *offset) {
    if (!out || !offset) return AE_SHELL_SYNTAX;
    memset(out,0,sizeof(*out)); *offset=0;
    if ((!source && length) || length>=AE_SHELL_BYTES) return length>=AE_SHELL_BYTES ? AE_SHELL_LIMIT : AE_SHELL_SYNTAX;
    for (size_t i=0;i<length;++i) if ((unsigned char)source[i]<0x20 && source[i]!='\t') { *offset=i; return AE_SHELL_SYNTAX; }
    struct lexer l={.source=source,.length=length,.lookup=lookup,.context=context,.out=out,.error=AE_SHELL_SYNTAX};
    struct ae_shell_command *command=&out->commands[0]; bool need=false;
    for (;;) {
        char *value=NULL; enum token t=token(&l,&value);
        if (t==BAD) break;
        if (t==END) {
            if (need || (!command->argc && out->used)) break;
            if (command->argc && !*command->argv[0]) break;
            out->count=command->argc ? (size_t)(command-out->commands)+1 : 0;
            return AE_SHELL_OK;
        }
        if (t==WORD) {
            if (command->argc==AE_SHELL_ARGS) { l.error=AE_SHELL_LIMIT; break; }
            command->argv[command->argc++]=value; need=false; continue;
        }
        if (t==PIPE) {
            if (!command->argc || !*command->argv[0]) break;
            if ((size_t)(command-out->commands)+1==AE_SHELL_COMMANDS) { l.error=AE_SHELL_LIMIT; break; }
            ++command; need=true; continue;
        }
        unsigned fd=t==INPUT ? 0u : t==ERROR_OUTPUT || t==ERROR_APPEND ? 2u : 1u;
        if (command->files[fd].mode!=AE_SHELL_NO_REDIRECT || token(&l,&value)!=WORD) break;
        command->files[fd]=(struct ae_shell_file){t==INPUT ? AE_SHELL_READ : t==APPEND || t==ERROR_APPEND ? AE_SHELL_APPEND : AE_SHELL_WRITE,value};
    }
    *offset=l.at; memset(out,0,sizeof(*out)); return l.error;
}
