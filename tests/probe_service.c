#include "core/owner.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <selinux/selinux.h>
#include <stdio.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#define CHECK(x, status) do { if (!(x)) { fprintf(stderr, "probe line %d\n", __LINE__); return status; } } while (0)

static int denied_file(const char *path)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) { close(fd); return 0; }
    return errno == EACCES;
}

int main(int argc, char **argv)
{
    CHECK((argc == 3 || argc == 4) && (strcmp(argv[1], "check") == 0 || strcmp(argv[1], "claim") == 0), 10);
    uint8_t owner[AE_ID_BYTES];
    CHECK(ae_id_from_hex(argv[2], strlen(argv[2]), owner) == 0 && geteuid() == 1000, 11);
    CHECK(prctl(PR_GET_NO_NEW_PRIVS, 0L, 0L, 0L, 0L) == 1, 28);
    struct __user_cap_header_struct header = {.version = _LINUX_CAPABILITY_VERSION_3};
    struct __user_cap_data_struct caps[2] = {{0}};
    CHECK(syscall(SYS_capget, &header, caps) == 0, 29);
    for (size_t n = 0; n < 2; ++n)
        CHECK(caps[n].effective == 0 && caps[n].permitted == 0 && caps[n].inheritable == 0, 30);
    char *context = NULL;
    CHECK(getcon_raw(&context) == 0 && strcmp(context, "system_u:system_r:aetheri_app_t:s0:c31,c32") == 0, 31);
    freecon(context);
    sd_bus *bus = NULL;
    CHECK(sd_bus_new(&bus) >= 0, 12);
    CHECK(sd_bus_set_address(bus, "unix:path=/var/lib/aetheri-p0/user-bus") >= 0, 13);
    CHECK(sd_bus_set_bus_client(bus, 1) >= 0 && sd_bus_set_method_call_timeout(bus, 2000000) >= 0, 14);
    CHECK(sd_bus_start(bus) >= 0, 15);
    if (strcmp(argv[1], "claim") == 0) {
        int result = sd_bus_request_name(bus, "org.aetheri.User1", 0);
        CHECK(result == -EACCES || result == -EPERM, 16);
    } else {
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = NULL;
        CHECK(sd_bus_call_method(bus, "org.aetheri.User1", "/org/aetheri/Management1", "org.aetheri.Management1",
                                 "GetProtocolInfo", &error, &reply, "") >= 0, 17);
        uint32_t major = 0, minor = 0;
        CHECK(sd_bus_message_has_signature(reply, "uuasayay") && sd_bus_message_read(reply, "uu", &major, &minor) > 0, 18);
        CHECK(major == 1 && minor == 1 && sd_bus_message_enter_container(reply, 'a', "s") > 0, 19);
        const char *capability = NULL;
        CHECK(sd_bus_message_read(reply, "s", &capability) == 0 && sd_bus_message_exit_container(reply) > 0, 20);
        const void *bytes = NULL;
        size_t size = 0;
        CHECK(sd_bus_message_read_array(reply, 'y', &bytes, &size) > 0 && size == 16, 21);
        CHECK(sd_bus_message_read_array(reply, 'y', &bytes, &size) > 0 && size == 16, 22);
        sd_bus_message_unref(reply);
        reply = NULL;
        CHECK(sd_bus_call_method(bus, "org.aetheri.User1", "/org/aetheri/Management1", "org.aetheri.Management1",
                                 "ListComponents", &error, &reply, "us", 1U, "") < 0, 32);
        CHECK(sd_bus_error_has_name(&error, "org.aetheri.Error1.UnsupportedCapability"), 33);
        sd_bus_error_free(&error);
        sd_bus_message_unref(reply);
        reply = NULL;
        CHECK(sd_bus_call_method(bus, "org.aetheri.User1", "/org/aetheri/Management1", "org.aetheri.Management1",
                                 "GetProtocolInfo", &error, &reply, "s", "invalid signature") < 0, 34);
        CHECK(sd_bus_error_has_name(&error, SD_BUS_ERROR_INVALID_ARGS), 35);
        sd_bus_error_free(&error);
        sd_bus_message_unref(reply);
        reply = NULL;
        int unexpected_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        CHECK(unexpected_fd >= 0, 39);
        CHECK(sd_bus_call_method(bus, "org.aetheri.User1", "/org/aetheri/Management1", "org.aetheri.Management1",
                                 "GetProtocolInfo", &error, &reply, "h", unexpected_fd) < 0, 40);
        CHECK(sd_bus_error_has_name(&error, SD_BUS_ERROR_ACCESS_DENIED), 41);
        CHECK(close(unexpected_fd) == 0, 42);
        sd_bus_error_free(&error);
        sd_bus_message_unref(reply);
        reply = NULL;
        CHECK(sd_bus_call_method(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
                                 "GetUnit", &error, &reply, "s", "basic.target") < 0, 23);
        CHECK(sd_bus_error_has_name(&error, SD_BUS_ERROR_ACCESS_DENIED), 24);
        sd_bus_error_free(&error);
        sd_bus_message_unref(reply);
        char private_path[256];
        snprintf(private_path, sizeof private_path, "/var/lib/aetheri-p0/users/%s/control/catalogue.sqlite3", argv[2]);
        CHECK(denied_file(private_path) && denied_file("/var/lib/aetheri-p0/registry/user-1000.conf"), 25);
        if (argc == 4) {
            uint8_t storage[AE_ID_BYTES];
            CHECK(ae_id_from_hex(argv[3], strlen(argv[3]), storage) == 0, 39);
            snprintf(private_path, sizeof private_path, "/var/lib/aetheri-p0/users/%s/control/allocations/%s", argv[2], argv[3]);
            CHECK(denied_file(private_path), 40);
            snprintf(private_path, sizeof private_path, "/var/lib/aetheri-p0/users/%s/data/%s/owner", argv[2], argv[3]);
            CHECK(denied_file(private_path) && denied_file("/var/lib/aetheri-p0/registry/allocation.conf"), 41);
        }
        int scratch = open("/var/lib/aetheri-p0/probe-data/scratch", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        CHECK(scratch >= 0 && write(scratch, "P0", 2) == 2 && lseek(scratch, 0, SEEK_SET) == 0, 36);
        char data[2];
        CHECK(read(scratch, data, sizeof data) == 2 && memcmp(data, "P0", 2) == 0, 37);
        CHECK(close(scratch) == 0 && unlink("/var/lib/aetheri-p0/probe-data/scratch") == 0, 38);
        sd_bus *system = NULL;
        CHECK(sd_bus_new(&system) >= 0 && sd_bus_set_address(system, "unix:path=/run/dbus/system_bus_socket") >= 0 && sd_bus_set_bus_client(system, 1) >= 0, 26);
        CHECK(sd_bus_start(system) < 0, 27);
        sd_bus_close_unref(system);
    }
    sd_bus_flush_close_unref(bus);
    return 0;
}
