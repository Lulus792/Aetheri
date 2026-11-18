#include "unlock_channel.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3 || !*argv[1] || strspn(argv[1], "0123456789") != strlen(argv[1])) return 2;
    errno = 0;
    long fd = strtol(argv[1], NULL, 10);
    if (errno || fd < 3 || fd > INT_MAX) return 2;
    int status = ae_unlock_worker_once((int)fd, argv[2]);
    close((int)fd);
    return status ? 1 : 0;
}
