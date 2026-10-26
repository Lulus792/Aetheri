#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define ROOT "/var/lib/aetheri-p0-policy-test"
#define CHECK(x, code) do { if (!(x)) return (code); } while (0)

static int denied(const char *path)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) { close(fd); return 0; }
    return errno == EACCES;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2 && (strcmp(argv[1], "a") == 0 || strcmp(argv[1], "b") == 0), 10);
    int first = strcmp(argv[1], "a") == 0;
    CHECK(getuid() == 1000 && geteuid() == 1000, 11);
    CHECK(prctl(PR_GET_NO_NEW_PRIVS, 0L, 0L, 0L, 0L) == 1, 12);
    struct __user_cap_header_struct header = {.version = _LINUX_CAPABILITY_VERSION_3, .pid = 0};
    struct __user_cap_data_struct capabilities[2] = {{0}};
    CHECK(syscall(SYS_capget, &header, capabilities) == 0, 32);
    for (size_t n = 0; n < 2; ++n) {
        CHECK(capabilities[n].effective == 0 && capabilities[n].permitted == 0 && capabilities[n].inheritable == 0, 33);
    }
    FILE *label_file = fopen("/proc/self/attr/current", "r");
    char label[128] = {0};
    CHECK(label_file != NULL, 13);
    CHECK(fgets(label, sizeof label, label_file) != NULL, 14);
    CHECK(fclose(label_file) == 0, 15);
    label[strcspn(label, "\n")] = '\0';
    const char *expected = first ? "system_u:system_r:aetheri_p0_app_t:s0:c21,c22" :
                                   "system_u:system_r:aetheri_p0_app_t:s0:c23,c24";
    CHECK(strcmp(label, expected) == 0, 16);
    const char *own = first ? ROOT "/a/own" : ROOT "/b/own";
    const char *sibling = first ? ROOT "/b/own" : ROOT "/a/own";
    int fd = open(own, O_RDWR | O_CLOEXEC);
    CHECK(fd >= 0, 17);
    char bytes[4];
    CHECK(read(fd, bytes, sizeof bytes) == (ssize_t)sizeof bytes, 18);
    CHECK(memcmp(bytes, first ? "owna" : "ownb", 4) == 0, 19);
    CHECK(lseek(fd, 0, SEEK_END) >= 0 && write(fd, "+", 1) == 1, 20);
    CHECK(close(fd) == 0, 21);
    CHECK(denied(sibling), 22);
    CHECK(denied(ROOT "/broker.db"), 23);
    CHECK(denied(ROOT "/unselected-document"), 24);
    const char *created = first ? ROOT "/a/created" : ROOT "/b/created";
    fd = open(created, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    CHECK(fd >= 0 && write(fd, "created", 7) == 7, 25);
    CHECK(close(fd) == 0, 26);
    pid_t child = fork();
    CHECK(child >= 0, 27);
    if (child == 0) {
        execl("/usr/bin/sudo", "sudo", "-n", "/usr/bin/true", (char *)NULL);
        _exit(errno == EACCES ? 0 : 1);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, 28);
    const char *report = first ? ROOT "/a/result" : ROOT "/b/result";
    FILE *output = fopen(report, "w");
    CHECK(output != NULL, 29);
    CHECK(fprintf(output, "uid=1000\nlabel=%s\nown=read-write-create\nsibling=denied\nbroker=denied\ndocument=denied\nsudo=denied\nnnp=1\ncapabilities=empty\n", label) > 0, 30);
    CHECK(fclose(output) == 0, 31);
    return 0;
}
