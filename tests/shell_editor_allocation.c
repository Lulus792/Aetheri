#include <errno.h>
#include <stddef.h>
/* Test-only normal shell producer: program allocation, first editor checkpoint,
   then fail exactly the second checkpoint (delete/cut/undo/redo). No preload,
   environment switch or fault hook is compiled into the production shell. */
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) {
    static unsigned calls;
    if (++calls==3) { errno=ENOMEM;return NULL; }
    return __real_malloc(size);
}
