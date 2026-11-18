#ifndef AETHERI_UNLOCK_VIEW_CONTRACT_H
#define AETHERI_UNLOCK_VIEW_CONTRACT_H

enum { AE_UNLOCK_VIEW_WIDTH = 436, AE_UNLOCK_VIEW_HEIGHT = 356 };

/* Only on the view's inherited private broker channel. Single control bytes
   below the first printable password character identify complete packets. */
#define AE_UNLOCK_VIEW_MAGIC "AELV1"
enum {
    AE_UNLOCK_VIEW_MAGIC_SIZE = 5, AE_UNLOCK_VIEW_DENIED = 1, AE_UNLOCK_VIEW_ERROR = 2,
    AE_UNLOCK_VIEW_PAUSE = 3, AE_UNLOCK_VIEW_PAUSED = 4, AE_UNLOCK_VIEW_RESUME = 5,
};

#endif
