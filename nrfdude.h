/* nrfdude.h
 *
 * Copyright (C) 2012 Tristan Willy <tristan.willy at gmail.com>
 * Copyright (C) 2012 Ruben Breimoen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
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
