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
#include <stdbool.h>
#include <unistd.h>
#include <libusb.h>
#include <errno.h>
#include <string.h>
#include "ihex.h"

#define VERSION_STRING      "0.1.0"
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


/* we should not overwrite the bootloader by default */
static bool protect_bootloader = true;


static void print_help(void){
    printf( "Usage: nrfdude [options]\n"
            "Options:\n"
            " -h                    : This message\n"
            " -r <file>             : Read from device to <file>\n"
            " -w <file>             : Write from <file> to device\n"
            " -x                    : Allow writing to 0x7800-0x7FFF"
                " (bootloader)\n");
}


/* address translation functions

 * The nRF24LU1+ has 32KiB of flash memory. It is divided into 64 pages with
 * each page holding 8 blocks of 64 bytes.
 *
 * Address types from least to most granular: page, block, addr
 *
 * Translation to a higher granularity address is exact. Translation to a lower
 * granularity address is inexact. Inexact translations are floor operations,
 * so addresses are rounded towards zero.
 */
unsigned block2addr(unsigned block){
    return block * 64;
}
unsigned addr2block(unsigned addr){
    return addr / 64;
}
unsigned page2block(unsigned page){
    return page * 8;
}
unsigned block2page(unsigned block){
    return block / 8;
}
unsigned page2addr(unsigned page){
    return block2addr(page2block(page));
}
unsigned addr2page(unsigned addr){
    return block2page(addr2block(addr));
}


/* bit vector manipulation */
void bitset(void *bv, int bit){
    unsigned char *bbv = (unsigned char *)bv;

    bbv[bit / 8] |= 1U << (bit % 8);
}


bool bitisset(void *bv, int bit){
    unsigned char *bbv = (unsigned char *)bv;

    return (bbv[bit / 8] & (1U << (bit % 8)));
}


/* check if an address is valid
 *
 * Valid valid conditions:
 *  1. Within 0x0000 - 0x7FFF
 *  2. Not protected:
 *      a. Bootloader @ 0x7800 - 0x7FFF
 *
 * Don't be too smart with this. The code only checks the first, last, and any
 * block-aligned address it tries to write.
 */
static bool addr_valid(unsigned addr){
    if((protect_bootloader && addr < BOOTLOADER_VECTOR) ||
            (!protect_bootloader && addr < 0x8000U)){
        return true;
    } else {
        return false;
    }
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


/* write one page to flash */
int nrf_write_page(devp dev, int page, void *data){
    unsigned char *b = (unsigned char *)data;
    unsigned char cmd[2], ret;
    int block;

    if(page < 0 || page > 63){
        /* invalid page */
        return -1;
    }

    /* send the flash-write command */
    cmd[0] = 0x02;
    cmd[1] = (unsigned char)page;
    if(nrf_cmd(dev, cmd, sizeof(cmd), &ret, sizeof(ret)) || ret){
        return -1;
    }

    /* send a page of memory, one block at a time */
    for(block = 0; block < 8; block++){
        if(nrf_cmd(dev, &b[block * 64], 64, &ret, sizeof(ret)) || ret){
            return -2;
        }
    }

    return 0;
}


/* compare/verify that a block on the device is identical to the one in data
 * returns result of memcpy() been device and data
 */
int nrf_compare_block(devp dev, int block, void *data){
    unsigned char cmd[2], devblock[64];

    /* request the block */
    cmd[0] = 0x03;
    cmd[1] = (unsigned char)block;
    if(nrf_cmd(dev, cmd, 2, devblock, 64)){
        return -1;
    }

    return memcmp(data, devblock, 64);
}



/* write fn to device */
int nrf_program(devp dev, const char *fn){
    unsigned char cmd[2], data[64], *flash_copy, dirty_bv[64];
    FILE *fp = NULL;
    int ecode, block, page, rc;
    unsigned int first_addr, last_addr;
    IHexRecord record;

    /* read the entire ROM into memory
     *
     * Why? Because Nordic doesn't have decent software. Their flash write
     * command erases the page first, instead of letting me decide if the page
     * should be erased first.
     *
     * Byte-level writing is offered by using a read-modify-write operation.
     * It would be smarter to operate on a per-page basis, but Intel HEX files
     * are not guaranteed to have sequential addressing. Since Nordic screwed
     * up and wild IHX files are adhoc, let's work with the whole 32k flash
     * memory at once.
     */
    if((flash_copy = malloc(FLASH_SIZE)) == NULL){
        ecode = -4;
        goto err;
    }
    printf("[*] Reading device.\n");
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
        if(nrf_cmd(dev, cmd, 2, &flash_copy[block2addr(block)], 64)){
            ecode = -2;
            goto err;
        }
    }

    /* overwrite in-memory with IHX contents */
    if((fp = fopen(fn, "r")) == NULL){
        ecode = -1;
        goto err;
    }
    memset(dirty_bv, 0, sizeof(dirty_bv));
    while((rc = Read_IHexRecord(&record, fp)) == IHEX_OK &&
            record.type != IHEX_TYPE_01){
        if(record.type != IHEX_TYPE_00){
            /* we cannot process segment or linear ihex files */
            printf("[!] IHX file contains segment or linear addressing.\n");
            ecode = -5;
            goto err;
        }
        /* first and last byte that is touched by this record */
        first_addr = record.address;
        last_addr = record.address + record.dataLen - 1;
        if(!addr_valid(first_addr) || !addr_valid(last_addr)){
            printf("[!] IHX record touches invalid or protected bytes: "
                    "0x%04X - 0x%04X\n", first_addr, last_addr);
            ecode = -6;
            goto err;
        }
        /* only write if the ihx record changes memory */
        if(memcmp(&flash_copy[first_addr], record.data, record.dataLen)){
            /* copy record into memory */
            memcpy(&flash_copy[first_addr], record.data, record.dataLen);
            /* mark dirty blocks */
            for(block = addr2block(first_addr); block <= addr2block(last_addr);
                    block++){
                /* check the block since we might have some *really* long
                 * record
                 */
                if(!addr_valid(block2addr(block))){
                    printf("[!] IHX record touches invalid or protected bytes:"
                            " 0x%04X\n", block2addr(block));
                    ecode = -6;
                    goto err;
                }
                bitset(dirty_bv, block);
            }
        }
    }
    if(rc != IHEX_OK && rc != IHEX_ERROR_EOF){
        /* we had an error that isn't EOF */
        ecode = -4;
        goto err;
    }

    /* write to flash */
    printf("[*] Writing device");
    fflush(stdout);
    for(page = 0; page < 64; page++){
        /* Each page is 8 blocks, which conviently maps to our dirty bit vector
         * on byte boundries.
         */
        if(dirty_bv[page]){
            printf(".");
            fflush(stdout);
            if(nrf_write_page(dev, page, &flash_copy[page2addr(page)])){
                printf("\n[!] Write failed.\n");
                ecode = -7;
                goto err;
            }
        }
    }
    printf("\n");

    /* verify */
    printf("[*] Verifying device");
    fflush(stdout);
    for(block = 0; block < 512; block++){
        if(bitisset(dirty_bv, block)){
            if(nrf_compare_block(dev, block, &flash_copy[block2addr(block)])){
                printf("\n[!] Block %d failed.\n", block);
                ecode = -8;
                goto err;
            } else {
                printf(".");
                fflush(stdout);
            }
        }
    }
    printf("\n");

    ecode = 0;
err:
    if(flash_copy){
        free(flash_copy);
    }
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
    char *r_fn = NULL, *w_fn = NULL;

    printf("nrfdude v%s, "
            "(C)2012 Tristan Willy <tristan dot willy@gmail.com>\n",
            VERSION_STRING);

    while((c = getopt(argc, argv, "hxr:w:")) != -1){
        switch(c){
        case 'h':
            print_help();
            exit(1);
        case 'r':
            r_fn = optarg;
            break;
        case 'w':
            w_fn = optarg;
            break;
        case 'x':
            protect_bootloader = false;
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
    if(r_fn){
        printf("[*] Dumping device to %s\n", r_fn);
        if((rc = nrf_dump(dev, r_fn))){
            printf("[!] Failed to dump: %d/%s\n", rc, strerror(errno));
        }
    }

    /* writing file to device */
    if(w_fn){
        printf("[*] Programming device with %s\n", w_fn);
        if((rc = nrf_program(dev, w_fn))){
            printf("[!] Failed to program: %d/%s\n", rc, strerror(errno));
        }
    }

    printf("[*] Done.\n");
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

