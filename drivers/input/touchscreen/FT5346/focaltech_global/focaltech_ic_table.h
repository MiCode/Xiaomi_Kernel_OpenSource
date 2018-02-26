/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __FOCALTECH_IC_TABLE_H__
#define __FOCALTECH_IC_TABLE_H__
/*******************************************************************************
*
* File Name: focaltech_ic_table.h
*
*  Author: Xu YF & ZR, Software Department, FocalTech
*
* Created: 2016-03-17
*
* Modify:
*
* Abstract:
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/

/*-----------------------------------------------------------
IC corresponding code, each of the IC code is 8 bit, high 4 bit on behalf of the same series, low 4 bit on behalf of the specific IC
-----------------------------------------------------------*/
enum IC_Type {
	IC_FT5X36 = 0x10,
	IC_FT5X36i = 0x11,
	IC_FT3X16 = 0x12,
	IC_FT3X26 = 0x13,

	IC_FT5X46 = 0x21,
	IC_FT5X46i = 0x22,
	IC_FT5526 = 0x23,
	IC_FT3X17 = 0x24,
	IC_FT5436 = 0x25,
	IC_FT3X27 = 0x26,
	IC_FT5526I = 0x27,
	IC_FT5416 = 0x28,
	IC_FT5426 = 0x29,
	IC_FT5435 = 0x2A,
	IC_FT7681 = 0x2B,
	IC_FT7661 = 0x2C,
	IC_FT7511 = 0x2D,
	IC_FT7421 = 0x2E,
	IC_FT6X06 = 0x30,
	IC_FT3X06 = 0x31,
	IC_FT6X36 = 0x40,
	IC_FT3X07 = 0x41,
	IC_FT6416 = 0x42,
	IC_FT6426 = 0x43,
	IC_FT7401 = 0x44,
	IC_FT3407U = 0x45,
	IC_FT6236U = 0x46,
	IC_FT6436U = 0x47,
	IC_FT3267 = 0x48,
	IC_FT3367 = 0x49,
	IC_FT5X16 = 0x50,
	IC_FT5X12 = 0x51,
	IC_FT5506 = 0x60,
	IC_FT5606 = 0x61,
	IC_FT5816 = 0x62,
	IC_FT5822 = 0x70,
	IC_FT5626 = 0x71,
	IC_FT5726 = 0x72,
	IC_FT5826B = 0x73,
	IC_FT3617 = 0x74,
	IC_FT3717 = 0x75,
	IC_FT7811 = 0x76,
	IC_FT5826S = 0x77,
	IC_FT5306  = 0x80,
	IC_FT5406  = 0x81,
	IC_FT8606  = 0x90,
	IC_FT8716  = 0xA0,
	IC_FT3C47U  = 0xB0,
	IC_FT8607  = 0xC0,
	IC_FT8707  = 0xD0,
	IC_FT8736  = 0xE0,
	IC_FT3D47  = 0xF0,
	IC_FTE716  = 0x100,
	IC_FT5442  = 0x110,
	IC_FT3428U = 0x120,
	IC_FT8006 = 0x130,
	IC_FTE736  = 0x140
};



extern unsigned int fts_ic_table_get_ic_code_from_ic_name(char *strIcName);
extern void fts_ic_table_get_ic_name_from_ic_code(unsigned int ic_code, char *strIcName);

extern unsigned int fts_ic_table_get_ic_code_from_chip_id(unsigned char chip_id, unsigned char chip_id2);
extern unsigned int fts_ic_table_get_chip_id_from_ic_code(unsigned int ic_code, unsigned char *chip_id, unsigned char *chip_id2);
extern int fts_ic_table_need_chip_id2(unsigned int chip_id);

#endif
