#ifndef AETHERI_COMPOSITOR_TEXT_INPUT_H
#define AETHERI_COMPOSITOR_TEXT_INPUT_H
#include <stdbool.h>
#include <stdint.h>
struct server;struct wlr_seat;struct wlr_keyboard;struct ae_text_input;
#ifdef AETHERI_COMPOSE
struct ae_text_input *ae_text_input_create(struct server *server,struct wlr_seat *seat);
void ae_text_input_destroy(struct ae_text_input *input);
bool ae_text_input_key(struct ae_text_input *input,struct wlr_keyboard *keyboard,uint32_t keycode);
void ae_text_input_modifiers(struct ae_text_input *input,struct wlr_keyboard *keyboard);
void ae_text_input_forget_keyboard(struct ae_text_input *input,struct wlr_keyboard *keyboard);
#else
static inline struct ae_text_input *ae_text_input_create(struct server *s,struct wlr_seat *seat) { (void)s;(void)seat;return 0; }
static inline void ae_text_input_destroy(struct ae_text_input *t) { (void)t; }
static inline bool ae_text_input_key(struct ae_text_input *t,struct wlr_keyboard *k,uint32_t code) { (void)t;(void)k;(void)code;return false; }
static inline void ae_text_input_modifiers(struct ae_text_input *t,struct wlr_keyboard *k) { (void)t;(void)k; }
static inline void ae_text_input_forget_keyboard(struct ae_text_input *t,struct wlr_keyboard *k) { (void)t;(void)k; }
#endif
#endif
