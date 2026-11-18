#ifndef AETHERI_COMPOSITOR_LOCK_WIRE_H
#define AETHERI_COMPOSITOR_LOCK_WIRE_H

#include <stdint.h>
#include <string.h>

enum ae_lock_action {
    AE_LOCK_COVER = 1, AE_LOCK_RELEASE = 2,
    AE_LOCK_COVERED = 3, AE_LOCK_RELEASED = 4, AE_LOCK_REJECTED = 5,
    AE_LOCK_REQUEST = 6,
    AE_LOCK_SLEEP_COVER = 7, AE_LOCK_SLEEP_COVERED = 8,
    AE_LOCK_WAKE_COVER = 9, AE_LOCK_WAKE_COVERED = 10,
};
enum { AE_LOCK_WIRE_SIZE = 14 };

static inline void ae_lock_wire_write(unsigned char packet[AE_LOCK_WIRE_SIZE],
        enum ae_lock_action action, uint64_t generation) {
    memcpy(packet, "AELK2", 5);
    packet[5] = (unsigned char)action;
    for (unsigned i = 0; i < 8; ++i)
        packet[6 + i] = (unsigned char)(generation >> ((7 - i) * 8));
}

static inline uint64_t ae_lock_wire_generation(const unsigned char packet[AE_LOCK_WIRE_SIZE]) {
    uint64_t generation = 0;
    for (unsigned i = 0; i < 8; ++i) generation = (generation << 8) | packet[6 + i];
    return generation;
}

#endif
