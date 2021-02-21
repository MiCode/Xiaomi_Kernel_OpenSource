// SPDX-License-Identifier: GPL-2.0
//Chirp Microsystems Firmware Header Generator

#include "ch101.h"

char *ch101_gpr_sr_open_version;

#define RAM_INIT_ADDRESS 1660
#define RAM_INIT_WRITE_SIZE   18

unsigned short get_ch101_gpr_sr_open_fw_ram_init_addr(void)
{
	return (unsigned short)RAM_INIT_ADDRESS;
}

unsigned short get_ch101_gpr_sr_open_fw_ram_init_size(void)
{
	return (unsigned short)RAM_INIT_WRITE_SIZE;
}

unsigned char ram_ch101_gpr_sr_open_init[RAM_INIT_WRITE_SIZE] = {
	/* 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFA, 0x00,*/
	/* 0x00, 0x64, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x01, 0x00,*/
};

unsigned char *get_ram_ch101_gpr_sr_open_init_ptr(void)
{
	return &ram_ch101_gpr_sr_open_init[0];
}

unsigned char ch101_gpr_sr_open_fw[CH101_FW_SIZE + 32];

