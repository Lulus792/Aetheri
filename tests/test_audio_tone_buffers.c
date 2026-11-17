#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <pipewire/pipewire.h>

static struct pw_buffer *dequeue(struct pw_stream *);
static int queue(struct pw_stream *,struct pw_buffer *);
static int flush(struct pw_stream *,bool);

#define pw_stream_dequeue_buffer dequeue
#define pw_stream_queue_buffer queue
#define pw_stream_flush flush
#define main ae_tone_entry
#define AETHERI_AUDIO_DESKTOP_TONE
#include "../src/audio/check.c"
#undef main
#undef pw_stream_dequeue_buffer
#undef pw_stream_queue_buffer
#undef pw_stream_flush

static struct pw_buffer buffer;
static unsigned queues,drains;

static struct pw_buffer *dequeue(struct pw_stream *stream) {
    assert(!stream);
    return &buffer;
}

static int queue(struct pw_stream *stream,struct pw_buffer *value) {
    assert(!stream && value==&buffer);
    ++queues;
    return 0;
}

static int flush(struct pw_stream *stream,bool drain) {
    assert(!stream && drain && queues==1);
    ++drains;
    return 0;
}

static void turn(uint64_t requested,uint32_t capacity,uint32_t remaining,
                 uint32_t expected_signal,uint32_t expected_queued) {
    float prepared[CHANNELS*25];
    for(size_t i=0;i<sizeof(prepared)/sizeof(*prepared);++i) prepared[i]=(float)i+1.0f;
    float output[CHANNELS*(TURN_FRAMES+2)];
    for(size_t i=0;i<sizeof(output)/sizeof(*output);++i) output[i]=-1.0f;

    struct spa_chunk chunk={.offset=16,.flags=SPA_CHUNK_FLAG_CORRUPTED};
    struct spa_data data={.data=output,.chunk=&chunk,.maxsize=capacity*STRIDE};
    struct spa_buffer storage={.n_datas=1,.datas=&data};
    buffer=(struct pw_buffer){.buffer=&storage,.requested=requested};
    struct check state={.prepared=prepared,.total=2+remaining,.sent=2,.formatted=true};
    queues=0;
    drains=0;

    process(&state);

    bool last=expected_signal==remaining;
    assert(!state.failed && state.sent==2+expected_signal);
    assert(queues==1 && drains==(unsigned)last && state.draining==last);
    assert(buffer.size==expected_queued && chunk.size==expected_queued*STRIDE);
    assert(chunk.offset==0 && chunk.stride==STRIDE && chunk.flags==0);
    for(size_t i=0;i<(size_t)expected_signal*CHANNELS;++i)
        assert(output[i]==prepared[2*CHANNELS+i]);
    for(size_t i=(size_t)expected_signal*CHANNELS;i<(size_t)expected_queued*CHANNELS;++i)
        assert(output[i]==0.0f);
    for(size_t i=(size_t)expected_queued*CHANNELS;i<sizeof(output)/sizeof(*output);++i)
        assert(output[i]==-1.0f);
}

int main(void) {
    turn(8,16,3,3,8);
    turn(UINT64_MAX,4,2,2,4);
    turn(UINT64_MAX,TURN_FRAMES+1,2,2,TURN_FRAMES);
    turn(0,16,3,3,3);
    turn(8,16,8,8,8);
    turn(4,16,8,4,4);
    puts("desktop tone turns: requested tail, capacity, bound, no suggestion, exact and nonfinal passed");
    return 0;
}
