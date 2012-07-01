#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <libusb.h>
#include <errno.h>
#include <string.h>
#include "nrfgo.h"
#include "nrfdude.h"

int nrfgo_program(devp dev, const char *fn){
    unsigned char cmd[2];
    printf("Programming nRFgo\n");
    cmd[0] = 0x06;
    cmd[1] = 0x04;
    if(nrf_bulk(dev, 0x02, cmd, sizeof(cmd))){
        return -1;
    }
    return 0;
}

int nrfgo_set_led_number(devp dev, int number){
    unsigned char cmd[2];
    if(number > 9) number = 9;
    else if(number < 0) number = 0;
    printf("Setting number on led:\n");
    cmd[0] = 0x06;
    cmd[1] = (unsigned char)number;
    if(nrf_bulk(dev, 0x02, cmd, sizeof(cmd))){
        return -1;
    }
    return 0;
}

int nrfgo_cmd(devp dev, void *cmd, int cmdlen, void *ret, int retlen){
    if(nrf_bulk(dev, 0x02, cmd, cmdlen)){
        return -1;
    }
    if(nrf_bulk(dev, 0x81, ret, retlen)){
        return -2;
    }
    return 0;
}

