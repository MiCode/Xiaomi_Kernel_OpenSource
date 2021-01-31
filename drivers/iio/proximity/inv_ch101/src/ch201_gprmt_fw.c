// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ch201.h"
#include "ch201_gprmt.h"

char ch201_gprmt_version[CH201_FW_VERS_SIZE] = "gprmt_gprmt-201_v9";

const char *ch201_gprmt_gitsha1 = "7d06f03f0db7165f2e42305143416f7973dedf01";

//#define CH201_RAM_INIT_ADDRESS 2392
static uint16_t ram_init_addr;
static uint8_t  ram_init_write_size;

uint16_t get_ch201_gprmt_fw_ram_init_addr(void)
{
	return (uint16_t)ram_init_addr;
}
uint16_t get_ch201_gprmt_fw_ram_init_size(void)
{
	return (uint16_t)ram_init_write_size;
}

unsigned char ram_ch201_gprmt_init[CH201_INIT_RAM_MAX_SIZE] = {
/* 0x88, 0x13, 0xD0, 0x07, 0x20, 0x03, 0x90, 0x01, 0xFA, 0x00, 0xAF,*/
/*  0x00, 0x06, 0x00, 0x00, 0x00,*/
/*0x00, 0xFA, 0x00, 0x00, 0x64, 0x00, 0x00, 0x0C, 0x00,*/
/*  0x00, 0x01, 0x00, */};

void set_ch201_gpr_fw_ram_init_addr(int addr)
{
	ram_init_addr = addr;
}

void set_ch201_gpr_fw_ram_write_size(int size)
{
	ram_init_write_size = size;
}

unsigned char *get_ram_ch201_gprmt_init_ptr(void)
{
	return &ram_ch201_gprmt_init[0];
}

unsigned char ch201_gprmt_fw[CH201_FW_SIZE + CH201_INIT_RAM_MAX_SIZE];

