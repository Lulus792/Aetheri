/* Controlled native counterexample; use only through test_sdl_wakeup.py. */
#define _POSIX_C_SOURCE 200809L
#include <SDL3/SDL.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"wake probe %d: %s SDL=%s\n",__LINE__,#x,SDL_GetError());exit(2);}}while(0)
static int pair[2];static Uint32 type;
static void *worker(void *unused) {
    (void)unused;char c;ssize_t n;
    do {n=read(pair[1],&c,1);}while(n<0 && errno==EINTR);CHECK(n==1 && c=='G');
    SDL_Event e={.type=type};CHECK(SDL_PushEvent(&e));c='A';
    do {n=write(pair[1],&c,1);}while(n<0 && errno==EINTR);CHECK(n==1);return NULL;
}
int main(void) {
    CHECK(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"wayland") && SDL_Init(SDL_INIT_VIDEO));
    CHECK(SDL_GetVersion()==SDL_VERSION);
    SDL_Window *window=SDL_CreateWindow("Aetheri wake regression",480,320,0);CHECK(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,"opengles2");CHECK(renderer);
    CHECK(SDL_SetRenderDrawColor(renderer,16,20,28,255) && SDL_RenderClear(renderer) && SDL_RenderPresent(renderer));
    struct wl_display *display=SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER,NULL);CHECK(display);
    CHECK(wl_display_roundtrip(display)>=0);SDL_Event e;
    while(SDL_PollEvent(&e)) {}
    CHECK(wl_display_roundtrip(display)>=0);while(SDL_PollEvent(&e)) {}
    type=SDL_RegisterEvents(1);CHECK(type);
    CHECK(!socketpair(AF_UNIX,SOCK_SEQPACKET,0,pair));
    char text[32];snprintf(text,sizeof(text),"%d",pair[0]);CHECK(!setenv("AETHERI_TEST_WAKE_FD",text,1));
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,worker,NULL));
    Uint64 start=SDL_GetTicksNS();bool received=SDL_WaitEventTimeout(&e,100);Uint64 elapsed=SDL_GetTicksNS()-start;
    CHECK(!pthread_join(thread,NULL));CHECK(!unsetenv("AETHERI_TEST_WAKE_FD"));
    int delivered=received && e.type==type,queued=0;
    while(SDL_PollEvent(&e)) if(e.type==type) queued++;
    printf("WAKE_PROBE received=%d delivered=%d queued_after=%d elapsed_ns=%llu\n",received,delivered,queued,(unsigned long long)elapsed);
    close(pair[0]);close(pair[1]);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    return delivered?0:1;
}
