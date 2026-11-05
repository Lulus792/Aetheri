#ifndef AETHERI_TEST_TEXT_INPUT_H
#define AETHERI_TEST_TEXT_INPUT_H
#include <stdbool.h>
struct server;
struct ae_test_text_input;
struct ae_test_text_input *ae_test_text_input_create(struct server *server);
bool ae_test_text_input_command(struct ae_test_text_input *fixture,unsigned command);
void ae_test_text_input_destroy(struct ae_test_text_input *fixture);
#endif
