/* Linked only into the explicit native failure fixture, never the application. */
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>
#undef SDL_PushEvent
#undef SDL_WaitEvent
extern bool SDLCALL SDL_PushEvent(SDL_Event *event);
extern bool SDLCALL SDL_WaitEvent(SDL_Event *event);
bool SDLCALL ae_test_wait_event(SDL_Event *event) {
    const char *gate=getenv("AETHERI_TERMINAL_WAIT_FAILURE_GATE");
    if (gate && !access(gate,F_OK)) {
        /* Persistent, bounded-rate failure: returning to this wait must not
           replace synchronous owner cleanup with an endless error loop. */
        SDL_Delay(10);SDL_SetError("controlled persistent event-wait failure");return false;
    }
    return SDL_WaitEvent(event);
}
bool SDLCALL ae_test_push_event(SDL_Event *event) {
    static atomic_bool failed=false;
    const char *gate=getenv("AETHERI_TERMINAL_EVENT_FAILURE_GATE");
    if (event->type>=SDL_EVENT_USER && gate && !access(gate,F_OK) && !atomic_exchange(&failed,true)) {
        SDL_SetError("controlled terminal event-queue failure");return false;
    }
    return SDL_PushEvent(event);
}
