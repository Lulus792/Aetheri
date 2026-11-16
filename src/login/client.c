#define _GNU_SOURCE
#include "login/client.h"
#include "login/handoff.h"
#include "core/root.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct ae_login_client {
    const struct ae_login_client_expect *expected;
    struct ae_login_service *service;
    struct ae_login_sources *sources;
    struct ae_login_greeter *greeter;
    struct ae_login_loaded *loaded;
    struct ae_login_handoff *handoff;
    struct ae_login_connection *connection;
    struct ae_root_identity parent;
    int socket, directory, error;
    pid_t owner;
    bool busy;
};

static int enter(const struct ae_login_client *client)
{
    if (!client) return -EINVAL;
    if (getpid() != client->owner) return -ECHILD;
    if (gettid() != client->owner) return -EXDEV;
    return client->busy ? -EBUSY : 0;
}

static int check_host(const struct ae_login_client_expect *expected)
{
    int result = expected->check(expected->userdata);
    return result > 0 ? -EACCES : result;
}

static int check_original(struct ae_login_client *client)
{
    int result = check_host(client->expected);
    if (!result) result = ae_login_greeter_check(client->greeter);
    if (!result) result = ae_root_check_path(client->directory,
        client->expected->directory, &client->parent);
    return result;
}

static int check_connection(void *data)
{
    struct ae_login_client *client = data;
    bool was_busy = client->busy;
    client->busy = true;
    int result = check_original(client);
    if (!result) result = ae_login_handoff_check(client->handoff);
    client->busy = was_busy;
    return result;
}

int ae_login_client_create(int *socket_fd, sd_bus *bus,
    const struct ae_login_client_expect *expected, struct ae_login_client **out)
{
    if (!out || *out || !socket_fd || *socket_fd < 0 || !bus || !expected ||
        !expected->check || !expected->directory || expected->directory[0] != '/' ||
        strnlen(expected->directory, AE_LOGIN_SOURCE_PATH_MAX) == AE_LOGIN_SOURCE_PATH_MAX ||
        !expected->leaf || !*expected->leaf || strnlen(expected->leaf, 256) == 256 ||
        strchr(expected->leaf, '/') || !strcmp(expected->leaf, ".") || !strcmp(expected->leaf, "..") ||
        !expected->label || !*expected->label ||
        strnlen(expected->label, AE_LOGIN_SOURCE_LABEL_MAX) == AE_LOGIN_SOURCE_LABEL_MAX ||
        expected->service.uid || expected->service.gid) return -EINVAL;
    if (gettid() != getpid()) return -EXDEV;
    int result = check_host(expected);
    if (result) return result;
    struct ae_login_client *client = calloc(1, sizeof *client);
    if (!client) return -ENOMEM;
    client->expected = expected;
    client->owner = getpid();
    client->socket = client->directory = -1;
    client->directory = open(expected->directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (client->directory < 0) result = -errno;
    if (!result) result = ae_root_observe(client->directory, &client->parent);
    if (!result && (client->parent.uid || client->parent.gid ||
        (client->parent.mode != (S_IFDIR | 0755) && client->parent.mode != (S_IFDIR | 0555))))
        result = -EACCES;
    if (!result) result = ae_root_check_path(client->directory, expected->directory, &client->parent);
    if (!result) result = ae_login_sources_create(expected->sources, expected->source_count, &client->sources);
    if (!result) result = ae_login_service_create(*socket_fd, bus, &expected->service, &client->service);
    if (!result) result = ae_login_greeter_create(client->service, client->sources, &expected->greeter, &client->greeter);
    if (!result) result = ae_login_loaded_create(client->service, client->sources,
        expected->fragment, expected->program, expected->config, expected->dropins,
        expected->dropin_count, &client->loaded);
    if (!result) result = check_original(client);
    if (!result) {
        client->socket = fcntl(*socket_fd, F_DUPFD_CLOEXEC, 3);
        if (client->socket < 0) result = -errno;
    }
    if (result) {
        ae_login_client_close(&client);
        return result;
    }
    close(*socket_fd);
    *socket_fd = -1;
    *out = client;
    return 0;
}

int ae_login_client_admit(struct ae_login_client *client, struct ae_login_connection **out)
{
    if (!out || *out) return -EINVAL;
    int result = enter(client);
    if (result) return result;
    if (client->error) return client->error;
    if (client->connection) return -EALREADY;
    client->busy = true;
    const struct ae_login_client_expect *expected = client->expected;
    result = check_original(client);
    if (!result) result = ae_login_loaded_check(client->loaded);
    if (!result) {
        struct stat observed;
        if (fstatat(client->directory, expected->leaf, &observed, AT_SYMLINK_NOFOLLOW)) {
            result = -errno;
            if (result == -ENOENT) {
                result = check_original(client);
                if (!result) {
                    client->busy = false;
                    return -EAGAIN;
                }
            }
        }
    }
    if (!result) result = ae_login_handoff_open(client->loaded, client->directory,
        expected->directory, expected->leaf, expected->label, &client->handoff);
    struct ae_login_connection_source source = {
        .uid = expected->service.uid, .gid = expected->service.gid,
        .check = check_connection, .userdata = client,
    };
    if (!result) result = ae_login_service_peer(client->service, &source.peer_pidfd);
    if (!result) result = ae_login_connection_create(&client->connection, &client->socket, &source);
    client->busy = false;
    if (result) {
        client->error = result == -EAGAIN ? -ESTALE : result;
        return client->error;
    }
    *out = client->connection;
    return 0;
}

int ae_login_client_close(struct ae_login_client **value)
{
    if (!value) return -EINVAL;
    struct ae_login_client *client = *value;
    if (!client) return 0;
    int result = enter(client);
    if (result && result != -ECHILD) return result;
    result = ae_login_connection_close(&client->connection);
    if (result) return result;
    ae_login_handoff_close(&client->handoff);
    ae_login_loaded_close(&client->loaded);
    ae_login_greeter_close(&client->greeter);
    ae_login_service_close(&client->service);
    ae_login_sources_close(&client->sources);
    if (client->directory >= 0) close(client->directory);
    if (client->socket >= 0) close(client->socket);
    free(client);
    *value = NULL;
    return 0;
}
