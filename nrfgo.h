/* nrfgo.h
 *
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

#ifndef INC_NRFGO_H
#define INC_NRFGO_H

#include "nrfdude.h"



int nrfgo_program(devp dev, const char *fn);
int nrfgo_set_led_number(devp dev, int number);
int nrfgo_cmd(devp dev, void *cmd, int cmdlen, void *ret, int retlen);
int nrfgo_erase_memory(devp dev);
int nrfgo_reset_device(devp dev);

#endif /* INC_NRFGO_H */
