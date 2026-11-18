#ifndef AETHERI_UNLOCK_VIEW_CONTRACT_H
#define AETHERI_UNLOCK_VIEW_CONTRACT_H

enum { AE_UNLOCK_VIEW_WIDTH = 436, AE_UNLOCK_VIEW_HEIGHT = 356 };

/* Only on the view's inherited private broker channel. The header is
   followed by password bytes on submission or by one status byte on reply. */
#define AE_UNLOCK_VIEW_MAGIC "AELV1"
enum { AE_UNLOCK_VIEW_MAGIC_SIZE = 5, AE_UNLOCK_VIEW_DENIED = 1, AE_UNLOCK_VIEW_ERROR = 2 };

#endif
