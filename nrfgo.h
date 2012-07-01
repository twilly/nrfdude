#ifndef INC_NRFGO_H
#define INC_NRFGO_H

#include "nrfdude.h"



int nrfgo_program(devp dev, const char *fn);
int nrfgo_set_led_number(devp dev, int number);
int nrfgo_cmd(devp dev, void *cmd, int cmdlen, void *ret, int retlen);

#endif /* INC_NRFGO_H */
