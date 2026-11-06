/* Linked only into the explicit native failure fixture, never the application. */
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
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
    bool trace=getenv("AETHERI_TERMINAL_ACCESS_TRACE")!=NULL;
    if(trace) fprintf(stderr,"TRACE_WAIT begin %llu\n",(unsigned long long)SDL_GetTicksNS());
    bool okay=SDL_WaitEvent(event);
    if(trace) fprintf(stderr,"TRACE_WAIT end %llu okay=%d type=%u\n",(unsigned long long)SDL_GetTicksNS(),okay,okay?event->type:0u);
    return okay;
}
bool SDLCALL ae_test_push_event(SDL_Event *event) {
    static atomic_bool failed=false;
    const char *gate=getenv("AETHERI_TERMINAL_EVENT_FAILURE_GATE");
    if (event->type>=SDL_EVENT_USER && gate && !access(gate,F_OK) && !atomic_exchange(&failed,true)) {
        SDL_SetError("controlled terminal event-queue failure");return false;
    }
    bool okay=SDL_PushEvent(event);
    if(getenv("AETHERI_TERMINAL_ACCESS_TRACE")) fprintf(stderr,"TRACE_PUSH %llu okay=%d type=%u\n",(unsigned long long)SDL_GetTicksNS(),okay,event->type);
    return okay;
}
