#ifndef INC_NRFGO_H
#define INC_NRFGO_H

#include "nrfdude.h"



int nrfgo_program(devp dev, const char *fn);
int nrfgo_set_led_number(devp dev, int number);
int nrfgo_cmd(devp dev, void *cmd, int cmdlen, void *ret, int retlen);
int nrfgo_erase_memory(devp dev);

#endif /* INC_NRFGO_H */
