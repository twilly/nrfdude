#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <libusb.h>
#include <errno.h>
#include <string.h>
#include "nrfgo.h"
#include "nrfdude.h"
#include "ihex.h"

static int nrfgo_set_programming_mode(devp dev);
static int nrfgo_stop_programming_mode(devp dev);
static int nrfgo_set_to_main_flash(devp dev);
static int nrfgo_wait_for_ready(devp dev);



int nrfgo_program(devp dev, const char *fn){
    FILE *fp = NULL;
    int rc;
    int i;
    unsigned char cmd[IHEX_MAX_DATA_LEN/2+2];
    IHexRecord record;
    printf("Programming nRFgo\n");

    if((fp = fopen(fn,"r")) == NULL){
        return -1;
    }

    nrfgo_set_programming_mode(dev);
    nrfgo_wait_for_ready(dev);
    nrfgo_set_to_main_flash(dev);
    nrfgo_wait_for_ready(dev);
    nrfgo_erase_memory(dev);
    nrfgo_wait_for_ready(dev);

    while((rc = Read_IHexRecord(&record,fp)) == IHEX_OK && record.type != IHEX_TYPE_01){
        cmd[0] = 0x2c;
        cmd[1] = (unsigned char)record.dataLen;
        cmd[2] = *(((unsigned char*)(&(record.address))+1));
        cmd[3] = *(((unsigned char*)(&(record.address))+0));
        for(i=0;i<record.dataLen;i++) {
            cmd[i+4] = record.data[i];
        }
        for(i=0;i<record.dataLen+4;i++) {
            printf("0x%02x ",cmd[i]);
        }
        printf("\n");
        if(nrf_bulk(dev, 0x02, cmd, record.dataLen+4)){
            printf("Error writing flash");
            return -1;
        }
    }
    nrfgo_wait_for_ready(dev);

    nrfgo_stop_programming_mode(dev);
    return 0;
}

static int nrfgo_set_to_main_flash(devp dev){
    unsigned char cmd[2];
    cmd[0] = 0x15;
    cmd[1] = 0x00;
    if(nrf_bulk(dev, 0x02, cmd, sizeof(cmd))){
        return -1;
    }
    return 0;
}

static int nrfgo_set_programming_mode(devp dev){
    unsigned char cmd[2];
    cmd[0] = 0x11;
    cmd[1] = 0x00;
    if(nrf_bulk(dev, 0x02, cmd, sizeof(cmd))){
        return -1;
    }
    return 0;
}

static int nrfgo_stop_programming_mode(devp dev){
    unsigned char cmd[1];
    cmd[0] = 0x12;
    if(nrf_bulk(dev, 0x02, cmd, sizeof(cmd))){
        return -1;
    }
    return 0;
}

int nrfgo_set_led_number(devp dev, int number){
    unsigned char cmd[2];
    printf("Setting number on the led display\n");
    if(number > 9 || number < 0) {
        printf("Invalid number. Setting to 0.\n");
        number = 0;
    }
    else printf("Setting it to %d\n",number);
    cmd[0] = 0x06;
    cmd[1] = (unsigned char)number;
    if(nrf_bulk(dev, 0x02, cmd, sizeof(cmd))){
        return -1;
    }
    nrfgo_wait_for_ready(dev);
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

int nrfgo_erase_memory(devp dev){
    unsigned char cmd[1];
    cmd[0] = 0x13;
    if(nrf_bulk(dev, 0x02, cmd, sizeof(cmd))){
        return -1;
    }
    return 0;
}

static int nrfgo_wait_for_ready(devp dev){
    unsigned char cmd[1];
    unsigned char ret[1];
    int i;
    cmd[0] = 0x16;
    for(i=0;i<100;i++){
        nrfgo_cmd(dev, cmd, sizeof(cmd), ret, sizeof(ret));
        if(ret[0] == 0x00)
            break;
    }
    return 0;
}

