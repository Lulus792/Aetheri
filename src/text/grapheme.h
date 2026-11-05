#ifndef AETHERI_TEXT_GRAPHEME_H
#define AETHERI_TEXT_GRAPHEME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Immutable borrowed byte span, strict scalar UTF-8 (NUL is a valid scalar).
   Success advances offset and sets scalar; failure changes neither. */
bool ae_utf8_next(const char *text,size_t size,size_t *offset,uint32_t *scalar);
/* Unicode 18.0 default extended grapheme boundaries, including 0 and byte size.
   Empty text has only offset 0. No shaping, width, normalization or word policy.
   NULL output/capacity 0 measures. Valid but insufficient capacity returns false
   with the required count; malformed input returns false/count 0. Output bytes
   remain unchanged on failure. Input/output/count must not overlap. No allocation. */
bool ae_text_graphemes(const char *text,size_t size,size_t *output,size_t capacity,size_t *count);
#endif
