#include "linux/issuer.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ae_issuer {
    pid_t owner;
    int busy;
    struct ae_peer peer;
    struct ae_unit_origin origin;
    struct ae_process_context context;
    struct ae_process_identity identity;
    struct ae_unit_issuer snapshot;
};
static int owner(struct ae_issuer *s) { return !s ? -EINVAL : s->owner==getpid() ? 0 : -ECHILD; }
static int origin_match(const struct ae_unit_origin *a,const struct ae_unit_origin *b)
{
    uint8_t x[AE_UNIT_ORIGIN_BYTES],y[AE_UNIT_ORIGIN_BYTES];
    int r=ae_unit_origin_encode(a,x); if (!r) r=ae_unit_origin_encode(b,y);
    return !r && memcmp(x,y,sizeof x) ? -ESTALE : r;
}
static int connection(struct ae_issuer *s,sd_bus *bus,const struct ae_unit_origin *origin)
{
    int r=origin_match(&s->origin,origin); struct ae_peer actual=AE_PEER_INIT;
    if (!r) r=ae_peer_capture_self(bus,&actual);
    if (!r && (actual.connection_uid || actual.connection_pid!=s->owner ||
        memcmp(actual.bus_id,origin->bus,16) || strcmp(actual.unique_name,origin->issuer) ||
        strcmp(actual.connection_label,s->context.label))) r=-ESTALE;
    if (!r) r=ae_process_reidentify(&s->identity,&actual,bus,&s->context);
    ae_peer_clear(&actual); return r;
}
static int verify(struct ae_issuer *s,sd_bus *bus,const struct ae_unit_origin *origin)
{
    if (getuid() || geteuid() || getgid() || getegid()) return -EPERM;
    int r=connection(s,bus,origin);
    if (!r) r=ae_process_verify_main(&s->peer,bus,&s->context);
    if (!r) r=ae_process_reidentify(&s->identity,&s->peer,bus,&s->context);
    struct ae_image image;
    if (!r) r=ae_image_match_self(0,0,&s->snapshot.image,&image);
    if (!r) r=ae_process_verify_main(&s->peer,bus,&s->context);
    if (!r) r=connection(s,bus,origin);
    return r;
}
int ae_issuer_open(sd_bus *bus,const struct ae_unit_origin *origin,const char *unit,const char *label,
    const struct ae_image *image,struct ae_issuer **out)
{
    if (!bus || !origin || !unit || !label || !image || !out || *out || !unit[0] ||
        strnlen(unit,256)==256 || !label[0] || strnlen(label,512)==512) return -EINVAL;
    if (getuid() || geteuid() || getgid() || getegid()) return -EPERM;
    uint8_t raw[AE_UNIT_ORIGIN_BYTES]; int r=ae_unit_origin_encode(origin,raw); if (r) return r;
    struct ae_issuer *s=calloc(1,sizeof *s); if (!s) return -ENOMEM;
    s->owner=getpid(); s->peer=(struct ae_peer)AE_PEER_INIT; s->origin=*origin;
    s->snapshot.image=*image;
    char selected_unit[256],selected_label[512]; strcpy(selected_unit,unit); strcpy(selected_label,label);
    r=ae_peer_capture_self(bus,&s->peer);
    if (!r && (s->peer.connection_uid || memcmp(s->peer.bus_id,origin->bus,16) || strcmp(s->peer.unique_name,origin->issuer))) r=-ESTALE;
    if (!r) r=ae_process_observe_main(&s->peer,bus,&s->context);
    if (!r && (s->context.uid || strcmp(s->context.unit,selected_unit) || strcmp(s->context.label,selected_label) || memcmp(s->context.boot,origin->boot,16))) r=-EACCES;
    if (!r) r=ae_process_identify(&s->peer,bus,&s->context,&s->identity);
    if (!r) {
        memcpy(s->snapshot.boot,s->identity.boot,16); memcpy(s->snapshot.invocation,s->context.invocation,16);
        s->snapshot.pidfs_id=s->identity.pidfd_id; s->snapshot.cgroup_id=s->context.cgroup_id;
        strcpy(s->snapshot.unit,s->context.unit); strcpy(s->snapshot.cgroup,s->context.cgroup); strcpy(s->snapshot.label,s->context.label);
        r=verify(s,bus,origin);
    }
    uint8_t encoded[AE_UNIT_ISSUER_MAX]; size_t used=0;
    if (!r) r=ae_unit_issuer_encode(&s->snapshot,encoded,sizeof encoded,&used);
    if (r) { ae_peer_clear(&s->peer); free(s); return r; }
    *out=s; return 0;
}
int ae_issuer_verify(struct ae_issuer *s,sd_bus *bus,const struct ae_unit_origin *origin)
{
    int r=owner(s); if (!r && (!bus || !origin)) r=-EINVAL; if (!r && s->busy) r=-EBUSY;
    if (r) return r;
    s->busy=1; r=verify(s,bus,origin); s->busy=0; return r;
}
int ae_issuer_snapshot(struct ae_issuer *s,struct ae_unit_issuer *out)
{
    int r=owner(s); if (!r && !out) r=-EINVAL;
    if (!r) *out=s->snapshot;
    return r;
}
int ae_issuer_close(struct ae_issuer **out)
{
    if (!out) return -EINVAL;
    struct ae_issuer *s=*out; if (!s) return 0;
    int r=owner(s); if (!r && s->busy) r=-EBUSY; if (r) return r;
    ae_peer_clear(&s->peer); free(s); *out=NULL; return 0;
}
