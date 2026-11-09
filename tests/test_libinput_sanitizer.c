/* Test-only sanitizer controls. Never part of a runtime projection. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libinput.h>
#include <lauxlib.h>

static int deny_open(const char *path, int flags, void *data)
{
    (void)path;
    (void)flags;
    (void)data;
    return -EPERM;
}

static void no_close(int fd, void *data)
{
    (void)fd;
    (void)data;
}

int main(int argc, char **argv)
{
    if (argc != 3)
        return 2;
    const struct libinput_interface interface = {deny_open, no_close};
    struct libinput *input = libinput_path_create_context(&interface, NULL);
    lua_State *lua = luaL_newstate();
    if (!input || !lua)
        return 3;
    luaL_checkversion(lua);
    lua_close(lua);
    libinput_unref(input);
    puts("SANITIZER_CONTEXT_READY");
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps)
        return 7;
    char line[4096];
    unsigned lines = 0;
    while (fgets(line, sizeof(line), maps)) {
        if (++lines > 2048)
            return 8;
        if (strstr(line, "libinput.so.") || strstr(line, "liblua-5.5.so") ||
            strstr(line, "libasan.so.") || strstr(line, "libubsan.so.") ||
            strstr(line, "libstdc++.so."))
            printf("SANITIZER_MAP %s", line);
    }
    if (ferror(maps) || fclose(maps))
        return 9;
    fflush(stdout);

    char *end = NULL;
    long value = strtol(argv[2], &end, 10);
    if (!end || *end || value < 0 || value > INT_MAX)
        return 4;
    if (strcmp(argv[1], "address") == 0) {
        /* Keep the pointer load opaque to object-size folding so this control
         * reaches ASan's heap check rather than an earlier UBSan size check.
         * Both sanitizers remain enabled. */
        unsigned char *volatile bytes = malloc(4);
        if (!bytes)
            return 5;
        memset(bytes, 0, 4);
        /* Runtime-selected out-of-bounds access must be caught by ASan. */
        volatile unsigned char observed = bytes[value];
        free(bytes);
        printf("ADDRESS_RESULT=%u\n", (unsigned int)observed);
    } else if (strcmp(argv[1], "undefined") == 0) {
        volatile int one = 1;
        /* Runtime-selected signed overflow must be caught by UBSan. */
        int result = (int)value + one;
        printf("UNDEFINED_RESULT=%d\n", result);
    } else {
        return 6;
    }
    return 0;
}
