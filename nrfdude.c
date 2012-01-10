/* nrfdude.c: programmer for the nRF24LU1 series
 *
 * Copyright (C) 2012 Tristan Willy <tristan.willy at gmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb.h>
#include <errno.h>
#include <string.h>
#include "ihex.h"

#define DEVSTRNAME          "nRF24LU1+"
#define VENDOR_NORDIC       0x1915
#define PID_NRF24LU         0x0101
#define IN                  0x81
#define OUT                 0x01
#define TIMEOUT             2000


/* typedef because libusb has terrible names */
typedef libusb_device_handle * devp;

static void print_help(void){
    printf( "Usage: nrfdude [options]\n"
            "Options:\n"
            " -h                    : This message\n"
            " -r <file>             : Read device to <file>\n");
}


/* bulk transfer */
static int nrf_bulk(devp dev, unsigned char endpoint, void *data, int length){
    int trans, rc;

    rc = libusb_bulk_transfer(dev, endpoint, data, length, &trans, TIMEOUT);

    return rc;
}


/* locate the first byte in 's' that does not match 'c' */
const void *memnotchr(const void *s, int c, size_t n){
    const unsigned char *s1 = (const unsigned char *)s;
    const unsigned char *s2 = s1 + n;

    while(s1 != s2 && *s1 == (unsigned char)c){
        s1++;
    }

    if(s1 != s2){
        return (const void *)s1;
    } else {
        return NULL;
    }
}


/* execute one command */
int nrf_cmd(devp dev, void *cmd, int cmdlen, void *ret, int retlen){
    if(nrf_bulk(dev, OUT, cmd, cmdlen)){
        return -1;
    }
    if(nrf_bulk(dev, IN, ret, retlen)){
        return -2;
    }
    return 0;
}


/* dump all of device flash to fn */
int nrf_dump(devp dev, const char *fn){
    unsigned char cmd[2], data[64];
    FILE *fp = NULL;
    int ecode, block, sub_block;
    IHexRecord record;

    if((fp = fopen(fn, "w+")) == NULL){
        ecode = -1;
        goto err;
    }

    /* dump */
    for(block = 0; block < 0x200; block++){
        /* set address MSB */
        if((block % 0x100) == 0){
            cmd[0] = 0x06;
            cmd[1] = (unsigned char)(block / 256);
            if(nrf_cmd(dev, cmd, 2, data, 1)){
                ecode = -2;
                goto err;
            }
        }
        /* request the block */
        cmd[0] = 0x03;
        cmd[1] = (unsigned char)block;
        if(nrf_cmd(dev, cmd, 2, data, 64)){
            ecode = -2;
            goto err;
        }
        /* write two records for this block
         * records are only written if the block contains something not 0xFF
         */
        for(sub_block = 0; sub_block < 2; sub_block++){
            record.type = IHEX_TYPE_00;
            memcpy(record.data, &data[sub_block * 32], 32);
            record.dataLen = 32;
            record.address = block * 64 + sub_block * 32;
            if(memnotchr(record.data, 0xFF, 32) &&
                    Write_IHexRecord(&record, fp)){
                ecode = -3;
                goto err;
            }
        }
    }

    /* write EoF record */
    record.type = IHEX_TYPE_01;
    record.address = 0;
    record.dataLen = 0;
    if(Write_IHexRecord(&record, fp)){
        ecode = -3;
        goto err;
    }

    ecode = 0;
err:
    if(fp){
        fclose(fp);
    }
    return ecode;
}


const char *nrf_version_str(devp dev){
    static unsigned char verbin[2];
    static char verstr[4];
    static unsigned char vercmd = 0x01;

    if(nrf_cmd(dev, &vercmd, 1, verbin, sizeof(verbin))){
        return "?.?";
    } else {
        verstr[0] = verbin[0] + '0';
        verstr[1] = '.';
        verstr[2] = verbin[0] + '0';
        verstr[3] = '\0';
        return verstr;
    }
}


int main(int argc, char *argv[]){
    int c, exit_code, rc;
    libusb_context *usb = NULL;
    devp dev = NULL;
    char *rbin_fn = NULL;

    while((c = getopt(argc, argv, "hr:")) != -1){
        switch(c){
        case 'h':
            print_help();
            exit(1);
        case 'r':
            rbin_fn = optarg;
            break;
        default:
            printf("[!] Invalid switch: %c\n", c);
            exit(1);
        }
    }

    if(libusb_init(&usb)){
        printf("[!] Failed to init libusb.\n");
        exit(1);
    }

    /* Spamming stdout is NOT OK. */
    libusb_set_debug(usb, 0);

    if((dev = libusb_open_device_with_vid_pid(usb, VENDOR_NORDIC, PID_NRF24LU))
            == NULL){
        printf("[!] Failed to open %04X:%04X.\n", VENDOR_NORDIC, PID_NRF24LU);
        goto error;
    }

    /* reset, setup, and claim */
    if(libusb_reset_device(dev)){
        printf("[!] Failed to reset device.\n");
        goto error;
    }
    if(libusb_set_configuration(dev, 1) || libusb_claim_interface(dev, 0)){
        printf("[!] Failed to set and claim %s.\n", DEVSTRNAME);
        goto error;
    }

    printf("[*] %s version %s\n", DEVSTRNAME, nrf_version_str(dev));

    /* reading memory to file */
    if(rbin_fn){
        printf("[*] Dumping device to %s\n", rbin_fn);
        if((rc = nrf_dump(dev, rbin_fn))){
            printf("[!] Failed to dump: %d/%s\n", rc, strerror(errno));
        }
    }

    exit_code = 0;
error:
    if(dev){
        libusb_release_interface(dev, 0);
        libusb_close(dev);
    }
    if(usb){
        libusb_exit(usb);
    }
    return exit_code;
}

