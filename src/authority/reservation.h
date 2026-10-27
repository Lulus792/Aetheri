#ifndef AETHERI_AUTHORITY_RESERVATION_H
#define AETHERI_AUTHORITY_RESERVATION_H
#include "catalogue/reservation.h"
#include <systemd/sd-bus.h>
/* Bounded wire shape only. Authentication/session policy belongs to the caller. */
int ae_reservation_read(sd_bus_message *message,struct ae_launch_request *output);
int ae_reservation_write(sd_bus_message *message,const struct ae_launch_request *request);
int ae_receipt_read(sd_bus_message *message,struct ae_reservation_receipt *output);
int ae_receipt_write(sd_bus_message *message,const struct ae_reservation_receipt *receipt);
#endif
