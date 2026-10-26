#include "linux/process.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <selinux/selinux.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/pidfd.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static unsigned int fault, identity_calls;
int __real_ioctl(int fd, unsigned long request, ...);
int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list arguments;
    va_start(arguments, request);
    struct pidfd_info *info = va_arg(arguments, struct pidfd_info *);
    va_end(arguments);
    CHECK(request == PIDFD_GET_INFO);
    if (fault == 2) { errno = ENOTTY; return -1; }
    int result = __real_ioctl(fd, request, info);
    if (result == 0) {
        ++identity_calls;
        if (fault == 1) info->mask &= ~PIDFD_INFO_CREDS;
        if (fault == 3) info->cgroupid ^= 1U;
        if (fault == 4 && identity_calls == 2) info->euid ^= 1U;
    }
    return result;
}

struct server {
    sd_bus *system;
    struct ae_peer retained;
    struct ae_process_context context;
    char unit[256];
    unsigned int calls;
};

static int descriptors(void)
{
    DIR *directory = opendir("/proc/self/fd");
    CHECK(directory != NULL);
    int count = 0;
    while (readdir(directory) != NULL) ++count;
    CHECK(closedir(directory) == 0);
    return count;
}

static int check_call(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    (void)error;
    struct server *state = userdata;
    CHECK(state->calls < 2);
    struct ae_peer peer = AE_PEER_INIT;
    CHECK(ae_peer_capture(message, &peer) == 0);
    struct ae_process_context actual = {0};
    int result = ae_process_observe(&peer, state->system, &actual);
    if (result != 0) fprintf(stderr, "observe: %s (%d)\n", strerror(-result), result);
    CHECK(result == 0 && actual.uid == geteuid() && strcmp(actual.unit, state->unit) == 0);
    CHECK(ae_process_verify(&peer, state->system, &actual) == 0);
    if (state->calls == 1) {
        CHECK(strcmp(actual.unit, state->context.unit) == 0 && memcmp(actual.boot, state->context.boot, sizeof actual.boot) == 0);
        CHECK(memcmp(actual.invocation, state->context.invocation, sizeof actual.invocation) != 0);
        CHECK(ae_process_verify(&peer, state->system, &state->context) == -ESTALE);
        CHECK(ae_peer_alive(&state->retained) == -ESRCH);
        ae_peer_clear(&state->retained);
        state->context = actual;
        state->retained = peer;
        ++state->calls;
        return sd_bus_reply_method_return(message, "");
    }
    printf("process: uid=%u unit=%s label=%s cgroup-id=%llu\n", (unsigned int)actual.uid, actual.unit, actual.label, (unsigned long long)actual.cgroup_id);
    fflush(stdout);
    for (unsigned int field = 0; field < 7; ++field) {
        struct ae_process_context wrong = actual;
        switch (field) {
            case 0: wrong.uid ^= 1U; break;
            case 1: wrong.boot[0] ^= 1U; break;
            case 2: wrong.invocation[0] ^= 1U; break;
            case 3: wrong.cgroup_id ^= 1U; break;
            case 4: wrong.unit[0] ^= 1; break;
            case 5: wrong.cgroup[0] ^= 1; break;
            default: wrong.label[0] ^= 1; break;
        }
        CHECK(ae_process_verify(&peer, state->system, &wrong) == -ESTALE);
    }
    struct ae_process_context untouched;
    memset(&untouched, 0xa5, sizeof untouched);
    struct ae_process_context sentinel = untouched;
    ++peer.connection_pid;
    CHECK(ae_process_observe(&peer, state->system, &untouched) == -EACCES);
    CHECK(memcmp(&untouched, &sentinel, sizeof sentinel) == 0);
    --peer.connection_pid;
    peer.connection_label[0] ^= 1;
    CHECK(ae_process_observe(&peer, state->system, &untouched) == -ESTALE);
    CHECK(memcmp(&untouched, &sentinel, sizeof sentinel) == 0);
    peer.connection_label[0] ^= 1;
    const int failures[] = {0, -ENODATA, -ENOTTY, -ESTALE, -EACCES};
    for (fault = 1; fault <= 4; ++fault) {
        identity_calls = 0;
        CHECK(ae_process_observe(&peer, state->system, &untouched) == failures[fault]);
        CHECK(memcmp(&untouched, &sentinel, sizeof sentinel) == 0);
    }
    fault = 0;
    int baseline = descriptors();
    for (unsigned int n = 0; n < 32; ++n) CHECK(ae_process_verify(&peer, state->system, &actual) == 0);
    CHECK(descriptors() == baseline);
    state->context = actual;
    state->retained = peer;
    ++state->calls;
    return sd_bus_reply_method_return(message, "");
}

static const sd_bus_vtable methods[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Check", "", "", check_call, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static int client(const char *destination)
{
    sd_bus *bus = NULL, *system = NULL;
    CHECK(sd_bus_open_user(&bus) >= 0);
    CHECK(sd_bus_open_system(&system) >= 0 && sd_bus_set_method_call_timeout(system, 2000000) >= 0);
    int baseline = descriptors();
    for (unsigned int n = 0; n < 8; ++n) {
        struct ae_process_context self;
        sd_id128_t invocation;
        CHECK(ae_process_observe_self(system, &self) == 0);
        CHECK(sd_id128_get_invocation(&invocation) == 0 && memcmp(self.invocation, invocation.bytes, sizeof self.invocation) == 0);
    }
    CHECK(descriptors() == baseline);
    sd_bus_flush_close_unref(system);
    CHECK(sd_bus_set_method_call_timeout(bus, 15000000) >= 0);
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int result = sd_bus_call_method(bus, destination, "/org/aetheri/TestProcess", "org.aetheri.TestProcess1", "Check", &error, NULL, "");
    if (result < 0) fprintf(stderr, "client: %s\n", error.message == NULL ? strerror(-result) : error.message);
    sd_bus_error_free(&error);
    sd_bus_flush_close_unref(bus);
    return result < 0 ? 1 : 0;
}

static void carrier_guard(void)
{
    int marker = open("/etc/aetheri-p0-carrier", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    struct stat info;
    CHECK(marker >= 0 && fstat(marker, &info) == 0 && S_ISREG(info.st_mode) && info.st_uid == 0 &&
          info.st_nlink == 1 && (info.st_mode & 0022) == 0);
    CHECK(close(marker) == 0 && security_getenforce() == 1);
    CHECK(stat("/sys/firmware/efi", &info) == 0 && S_ISDIR(info.st_mode));
    FILE *product = fopen("/sys/class/dmi/id/product_name", "r");
    char name[128] = {0};
    CHECK(product != NULL && fgets(name, sizeof name, product) != NULL && fclose(product) == 0);
    CHECK(strcmp(name, "Aetheri P0 disposable VM\n") == 0);
}

int main(int argc, char **argv)
{
    alarm(25);
    if (argc == 3 && strcmp(argv[1], "--client") == 0) return client(argv[2]);
    CHECK(argc == 1 && geteuid() == 1000 && argv[0][0] == '/');
    carrier_guard();
    struct server state = {.retained = AE_PEER_INIT};
    sd_id128_t nonce;
    CHECK(sd_id128_randomize(&nonce) == 0);
    char digits[SD_ID128_STRING_MAX];
    sd_id128_to_string(nonce, digits);
    snprintf(state.unit, sizeof state.unit, "aetheri-process-test-%s.service", digits);
    char unit_option[280];
    snprintf(unit_option, sizeof unit_option, "--unit=%s", state.unit);
    printf("supervised fixture: %s\n", state.unit);
    fflush(stdout);
    sd_bus *bus = NULL;
    CHECK(sd_bus_open_user(&bus) >= 0 && sd_bus_open_system(&state.system) >= 0);
    CHECK(sd_bus_set_method_call_timeout(state.system, 2000000) >= 0);
    CHECK(sd_bus_add_object_vtable(bus, NULL, "/org/aetheri/TestProcess", "org.aetheri.TestProcess1", methods, &state) >= 0);
    const char *unique = NULL;
    CHECK(sd_bus_get_unique_name(bus, &unique) >= 0);
    char *context = NULL;
    CHECK(getcon_raw(&context) == 0);
    char context_option[600];
    int count = snprintf(context_option, sizeof context_option, "--property=SELinuxContext=%s", context);
    CHECK(count > 0 && (size_t)count < sizeof context_option);
    freecon(context);
    for (unsigned int run = 0; run < 2; ++run) {
        pid_t runner = fork();
        CHECK(runner >= 0);
        if (runner == 0) {
            execlp("sudo", "sudo", "-n", "systemd-run", unit_option, "--wait", "--collect", "--service-type=exec", "--uid=1000",
               "--property=NoNewPrivileges=yes", "--property=CapabilityBoundingSet=", "--property=SendSIGKILL=no",
               "--property=RestrictNamespaces=yes", "--property=TimeoutStopSec=5s", "--property=RuntimeMaxSec=20s",
               "--setenv=XDG_RUNTIME_DIR=/run/user/1000", context_option, "--", "/usr/bin/env", argv[0], "--client", unique, (char *)NULL);
            _exit(127);
        }
        while (state.calls <= run) {
            int result = sd_bus_process(bus, NULL);
            CHECK(result >= 0);
            if (result == 0) CHECK(sd_bus_wait(bus, UINT64_MAX) >= 0);
        }
        CHECK(sd_bus_flush(bus) >= 0);
        int status = 0;
        CHECK(waitpid(runner, &status, 0) == runner && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    CHECK(ae_process_verify(&state.retained, state.system, &state.context) == -ESRCH);
    ae_peer_clear(&state.retained);
    sd_bus_flush_close_unref(state.system);
    sd_bus_flush_close_unref(bus);
    puts("process: real bus ProcessFD/current UID+label, invocation/cgroup, seven stale bindings, same-name restart, missing/changed observations, exit and FD balance passed");
    return 0;
}
