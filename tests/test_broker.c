#include "authority/broker.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "broker line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

/* Cache ownership/lifetime tests only. Actual credentials/context are exercised
 * separately by the enforcing service integration and linux.process case. */
static struct ae_process_context observed;
static struct ae_broker *invalidate;
int __wrap_ae_process_observe_registration(const struct ae_peer *peer, sd_bus *bus, struct ae_process_context *out)
{
    (void)peer; (void)bus;
    *out = observed;
    return 0;
}
int __wrap_ae_process_verify(const struct ae_peer *peer, sd_bus *bus, const struct ae_process_context *expected)
{
    (void)peer; (void)bus;
    if (invalidate != NULL) { ae_broker_clear(invalidate); invalidate = NULL; }
    return ae_process_context_equal(expected, &observed) ? 0 : -ESTALE;
}

static struct ae_peer peer_for(pid_t child, sd_bus *owner)
{
    struct ae_peer peer = AE_PEER_INIT;
    peer.pidfd = pidfd_open(child, 0);
    CHECK(peer.pidfd >= 0);
    peer.connection_pid = child;
    peer.connection_uid = 1000;
    strcpy(peer.connection_label, "system_u:system_r:aetheri_user_t:s0");
    const char *name = NULL;
    sd_id128_t id;
    CHECK(sd_bus_get_unique_name(owner, &name) >= 0 && strlen(name) < sizeof peer.unique_name);
    strcpy(peer.unique_name, name);
    CHECK(sd_bus_get_bus_id(owner, &id) >= 0);
    memcpy(peer.bus_id, id.bytes, 16);
    return peer;
}

static void await_clear(sd_event *event, struct ae_broker *broker)
{
    for (unsigned int n = 0; broker->peer.pidfd >= 0 && n < 8; ++n) CHECK(sd_event_run(event, 2000000) > 0);
    CHECK(broker->peer.pidfd == -1 && broker->exit_source == NULL && broker->name_slot == NULL);
}

int main(void)
{
    alarm(20);
    CHECK(geteuid() == 1000);
    int pipefd[2];
    CHECK(pipe2(pipefd, O_CLOEXEC) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        close(pipefd[1]);
        char byte;
        ssize_t count;
        do { count = read(pipefd[0], &byte, 1); } while (count < 0 && errno == EINTR);
        _exit(count == 0 ? 0 : 1);
    }
    CHECK(close(pipefd[0]) == 0);
    sd_bus *bus = NULL, *owner = NULL;
    sd_event *event = NULL;
    CHECK(sd_event_new(&event) >= 0 && sd_bus_open_user(&bus) >= 0 && sd_bus_open_user(&owner) >= 0);
    CHECK(sd_bus_attach_event(bus, event, 0) >= 0);
    observed.uid = 1000; observed.boot[0] = 1; observed.invocation[0] = 2; observed.cgroup_id = 3;
    strcpy(observed.unit, "aetheri-fixture.service"); strcpy(observed.cgroup, "/system.slice/aetheri-fixture.service");
    strcpy(observed.label, "system_u:system_r:aetheri_user_t:s0");
    struct ae_broker_identity identity = {.account = {1}, .epoch = {2}, .runtime = {3}};
    struct ae_broker broker = AE_BROKER_INIT;
    struct ae_peer peer = peer_for(child, owner);
    int original = peer.pidfd;
    CHECK(ae_broker_bind(&broker, &peer, bus, event, &identity, observed.unit, observed.boot) == 0 && peer.pidfd == -1);
    for (unsigned int n = 0; n < 16; ++n) {
        peer = peer_for(child, owner);
        CHECK(ae_broker_bind(&broker, &peer, bus, event, &identity, observed.unit, observed.boot) == 0);
        CHECK(peer.pidfd >= 0 && broker.peer.pidfd == original);
        CHECK(ae_broker_verify(&broker, &peer, bus) == 0);
        ae_peer_clear(&peer);
    }
    peer = peer_for(child, owner);
    struct ae_broker_identity changed = identity; changed.runtime[0] ^= 1U;
    CHECK(ae_broker_bind(&broker, &peer, bus, event, &changed, observed.unit, observed.boot) == -EALREADY);
    CHECK(broker.peer.pidfd == original && memcmp(&broker.identity, &identity, sizeof identity) == 0);
    CHECK(ae_broker_bind(&broker, &peer, bus, event, &identity, "other.service", observed.boot) == -EACCES);
    peer.unique_name[0] = 'x';
    CHECK(ae_broker_verify(&broker, &peer, bus) == -EACCES);
    ae_peer_clear(&peer);
    struct ae_peer disconnected = peer_for(child, owner);
    sd_bus_flush_close_unref(owner); owner = NULL;
    await_clear(event, &broker);  /* Real daemon name-loss signal; child still alive. */
    CHECK(fcntl(original, F_GETFD) == -1 && errno == EBADF);
    CHECK(ae_broker_bind(&broker, &disconnected, bus, event, &identity, observed.unit, observed.boot) == -ENOTCONN);
    CHECK(broker.peer.pidfd == -1 && disconnected.pidfd >= 0);
    ae_peer_clear(&disconnected);
    CHECK(sd_bus_open_user(&owner) >= 0);
    peer = peer_for(child, owner);
    CHECK(ae_peer_alive(&peer) == 0);
    CHECK(ae_broker_bind(&broker, &peer, bus, event, &identity, observed.unit, observed.boot) == 0);
    peer = peer_for(child, owner);
    invalidate = &broker;
    CHECK(ae_broker_verify(&broker, &peer, bus) == -ESTALE && broker.peer.pidfd == -1);
    ae_peer_clear(&peer);
    peer = peer_for(child, owner);
    CHECK(ae_broker_bind(&broker, &peer, bus, event, &identity, observed.unit, observed.boot) == 0);
    CHECK(close(pipefd[1]) == 0);
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    await_clear(event, &broker);  /* Real pidfd event; bus owner remains connected. */
    ae_broker_clear(&broker);
    sd_bus_flush_close_unref(owner); sd_bus_flush_close_unref(bus); sd_event_unref(event);
    puts("broker cache: move/idempotence/conflict, reentrant invalidation, separate real name-loss/exit events passed (context mocked)");
    return 0;
}
