// SPDX-License-Identifier: GPL-2.0

// DEFINE THIS TO USE SPECIAL SENSOR F/W WITH PATTERN I/Q DATA
#define USE_IQ_DEBUG	0


#if !USE_IQ_DEBUG

/* REGULAR FIRMWARE */

//Chirp Microsystems Firmware Header Generator

#include "ch101.h"
#include <linux/types.h>

char ch101_gpr_version[CH101_FW_VERS_SIZE];

//#define RAM_INIT_ADDRESS 1864
static uint16_t ram_init_addr;

//static uint8_t  RAM_INIT_WRITE_SIZE;
static uint8_t  ram_init_write_size;

uint16_t get_ch101_gpr_fw_ram_init_addr(void)
{
	return (uint16_t)ram_init_addr;
}

uint16_t get_ch101_gpr_fw_ram_init_size(void)
{
	return (uint16_t)ram_init_write_size;
}

void set_ch101_gpr_fw_ram_init_addr(int addr)
{
	ram_init_addr = addr;
}

void set_ch101_gpr_fw_ram_write_size(int size)
{
	ram_init_write_size = size;
}

unsigned char ram_ch101_gpr_init[CH101_INIT_RAM_MAX_SIZE] = {
/*0x06, 0x00, 0x00, 0x00, 0x00, 0xFA, 0x00, 0x00, 0x64, 0x00,*/
 /*  0x00, 0x0C, 0x00, 0x00, 0x01, 0x00,*/
};

unsigned char *get_ram_ch101_gpr_init_ptr(void)
{
	return &ram_ch101_gpr_init[0];
}

unsigned char ch101_gpr_fw[CH101_FW_SIZE + CH101_INIT_RAM_MAX_SIZE];

#else	 // USE_IQ_DEBUG

/* SPECIAL DEBUG FIRMWARE  - Puts out ascending */
 /*  number sequence instead of real I/Q data */

//Chirp Microsystems Firmware Header Generator

#include "ch101.h"

char ch101_gpr_version[CH101_FW_VERS_SIZE];

#define RAM_INIT_ADDRESS 1864

#define RAM_INIT_WRITE_SIZE   16

uint16_t get_ch101_gpr_fw_ram_init_addr(void)
{
	return (uint16_t)RAM_INIT_ADDRESS;
}

uint16_t get_ch101_gpr_fw_ram_init_size(void)
{
	return (uint16_t)RAM_INIT_WRITE_SIZE;
}

unsigned char ram_ch101_gpr_init[CH101_INIT_RAM_MAX_SIZE] = {
/*0x06, 0x00, 0x00, 0x00, 0x00, 0xFA, 0x00, 0x00, 0x64,*/
/* 0x00, 0x00, 0x0C, 0x00, 0x00, 0x01, 0x00,*/
};

unsigned char *get_ram_ch101_gpr_init_ptr(void)
{
	return &ram_ch101_gpr_init[0];
}

unsigned char ch101_gpr_fw[CH101_FW_SIZE + CH101_INIT_RAM_MAX_SIZE];

#endif  // USE_IQ_DEBUG
