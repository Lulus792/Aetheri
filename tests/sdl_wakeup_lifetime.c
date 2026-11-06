/* Private Linux SDL wake-descriptor lifetime/failure probe; never installed. */
#define _GNU_SOURCE
#ifdef AETHERI_WAKE_FAULT_SHIM
#include <errno.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>
int eventfd(unsigned int initial, int flags)
{
    if (getenv("AETHERI_TEST_EVENTFD_FAILURE")) {
        if (setenv("AETHERI_TEST_EVENTFD_SEEN", "1", 1) != 0) return -1;
        errno = EMFILE;
        return -1;
    }
    return (int)syscall(SYS_eventfd2, initial, flags);
}
#else
#include <SDL3/SDL.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "wake lifetime %d: %s SDL=%s\n", __LINE__, #x, SDL_GetError()); \
    exit(2); } } while (0)
enum { FD_BOUND = 256, ROUNDS = 16, EVENTS = 32 };
struct snapshot { int present[FD_BOUND]; char target[FD_BOUND][256]; };
static Uint32 event_type;

static void snapshot(struct snapshot *result)
{
    memset(result, 0, sizeof(*result));
    DIR *directory = opendir("/proc/self/fd");
    CHECK(directory != NULL);
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        char *end;
        long number = strtol(entry->d_name, &end, 10);
        CHECK(*end == '\0' && number >= 0 && number < FD_BOUND);
        if (number == dirfd(directory)) continue;
        ssize_t count = readlinkat(dirfd(directory), entry->d_name,
                                  result->target[number], sizeof(result->target[number]) - 1);
        CHECK(count > 0 && (size_t)count < sizeof(result->target[number]) - 1);
        result->present[number] = 1;
    }
    CHECK(closedir(directory) == 0);
}

static void *producer(void *unused)
{
    (void)unused;
    SDL_Delay(2); /* Exercise an armed native wait, separately from the gap gate. */
    for (int i = 0; i < EVENTS; ++i) {
        SDL_Event event = {.type = event_type};
        CHECK(SDL_PushEvent(&event));
    }
    return NULL;
}

int main(void)
{
    struct snapshot before, active, after;
    snapshot(&before);
    CHECK(SDL_GetVersion() == SDL_VERSION);
    CHECK(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland"));
    if (getenv("AETHERI_TEST_EVENTFD_FAILURE")) {
        CHECK(unsetenv("AETHERI_TEST_EVENTFD_SEEN") == 0);
        CHECK(!SDL_Init(SDL_INIT_VIDEO));
        /* SDL's public video selector replaces the backend diagnostic with
           "wayland not available". Prove the injected boundary separately. */
        CHECK(getenv("AETHERI_TEST_EVENTFD_SEEN") != NULL &&
              strcmp(getenv("AETHERI_TEST_EVENTFD_SEEN"), "1") == 0);
        CHECK(SDL_GetError()[0] != '\0');
        SDL_Quit();
        snapshot(&after);
        CHECK(memcmp(&before, &after, sizeof(before)) == 0);
        puts("WAKE_LIFETIME failure_cleanup=1 descriptors_unchanged=1");
        return 0;
    }
    for (int round = 0; round < ROUNDS; ++round) {
        CHECK(SDL_Init(SDL_INIT_VIDEO));
        SDL_Window *window = SDL_CreateWindow("Aetheri wake lifetime", 64, 64, SDL_WINDOW_HIDDEN);
        CHECK(window != NULL);
        snapshot(&active);
        int wakes = 0;
        for (int fd = 0; fd < FD_BOUND; ++fd) {
            if (!active.present[fd] || before.present[fd] ||
                strcmp(active.target[fd], "anon_inode:[eventfd]") != 0) continue;
            CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
            CHECK((fcntl(fd, F_GETFL) & O_NONBLOCK) != 0);
            ++wakes;
        }
        CHECK(wakes == 1);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {}
        event_type = SDL_RegisterEvents(1);
        CHECK(event_type != 0);
        pthread_t worker;
        CHECK(pthread_create(&worker, NULL, producer, NULL) == 0);
        int received = 0;
        Uint64 deadline = SDL_GetTicks() + 1000;
        while (received != EVENTS && SDL_GetTicks() < deadline) {
            if (SDL_WaitEventTimeout(&event, 100) && event.type == event_type) ++received;
        }
        CHECK(pthread_join(worker, NULL) == 0);
        CHECK(received == EVENTS);
        SDL_DestroyWindow(window);
        SDL_Quit();
        snapshot(&after);
        CHECK(memcmp(&before, &after, sizeof(before)) == 0);
    }
    printf("WAKE_LIFETIME rounds=%d delivered=%d descriptors_unchanged=1\n", ROUNDS, ROUNDS * EVENTS);
    return 0;
}
#endif
