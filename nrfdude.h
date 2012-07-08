#ifndef INC_NRFDUDE_H
#define INC_NRFDUDE_H

#include <libusb.h>

#define VERSION_STRING      "0.1.1-beta"
#define DEVSTRNAME          "nRF24LU1+"
#define VENDOR_NORDIC       0x1915
#define PID_NRF24LU         0x0101
#define PID_NRFGO           0x001a
#define FLASH_SIZE          32768
#define IN                  0x81
#define OUT                 0x01
#define TIMEOUT             2000
#define BOOTLOADER_VECTOR   0x7800U


/* typedef because libusb has terrible names */
typedef libusb_device_handle * devp;

int nrf_bulk(devp dev, unsigned char endpoint, void *data, int length);

#endif /* INC_NRFDUDE_H */
