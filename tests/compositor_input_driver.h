#ifndef AETHERI_TEST_COMPOSITOR_INPUT_DRIVER_H
#define AETHERI_TEST_COMPOSITOR_INPUT_DRIVER_H
struct server;
struct input_driver;
struct input_driver *input_driver_start(struct server *server, int fd);
void input_driver_close(struct input_driver *driver);
#endif
