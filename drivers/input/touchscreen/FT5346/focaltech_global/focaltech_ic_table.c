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

/*******************************************************************************
*
* File Name: Focaltech_ic_table.c
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

#include <linux/kernel.h>
#include <linux/string.h>
#include "focaltech_ic_table.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FOCALTECH_IC_TABLE_INFO  "File Version of  focaltech_ic_table.c:  V1.0.0 2016-03-22"

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

/*******************************************************************************
* Static variables
*******************************************************************************/

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/

/*******************************************************************************
* Static function prototypes
*******************************************************************************/


/*******************************************************************************
* functions body
*******************************************************************************/

unsigned int fts_ic_table_get_ic_code_from_ic_name(char *strIcName)
{

	if (strncmp(strIcName, "FT5X36", 6) == 0)
		return IC_FT5X36;
	if (strncmp(strIcName, "FT5X36i", 7) == 0)
		return IC_FT5X36i;
	if (strncmp(strIcName, "FT3X16", 6) == 0)
		return IC_FT3X16;
	if (strncmp(strIcName, "FT3X26", 6) == 0)
		return IC_FT3X26;
	if (strncmp(strIcName, "FT5X22", 6) == 0)
		return IC_FT5X46;
	if (strncmp(strIcName, "FT5X46", 6) == 0)
		return IC_FT5X46;
	if (strncmp(strIcName, "FT5X46i", 7) == 0)
		return IC_FT5X46i;
	if (strncmp(strIcName, "FT5526", 6) == 0)
		return IC_FT5526;
	if (strncmp(strIcName, "FT3X17", 6) == 0)
		return IC_FT3X17;
	if (strncmp(strIcName, "FT5436", 6) == 0)
		return IC_FT5436;
	if (strncmp(strIcName, "FT3X27", 6) == 0)
		return IC_FT3X27;
	if (strncmp(strIcName, "FT5526i", 7) == 0)
		return IC_FT5526I;
	if (strncmp(strIcName, "FT5416", 6) == 0)
		return IC_FT5416;
	if (strncmp(strIcName, "FT5426", 6) == 0)
		return IC_FT5426;
	if (strncmp(strIcName, "FT5435", 6) == 0)
		return IC_FT5435;
	if (strncmp(strIcName, "FT7681", 6) == 0)
		return IC_FT7681;
	if (strncmp(strIcName, "FT7661", 6) == 0)
		return IC_FT7661;
	if (strncmp(strIcName, "FT7511", 6) == 0)
		return IC_FT7511;
	if (strncmp(strIcName, "FT7421", 6) == 0)
		return IC_FT7421;
	if (strncmp(strIcName, "FT6X06", 6) == 0)
		return IC_FT6X06;
	if (strncmp(strIcName, "FT3X06", 6) == 0)
		return IC_FT3X06;
	if (strncmp(strIcName, "FT6X36", 6) == 0)
		return IC_FT6X36;
	if (strncmp(strIcName, "FT3X07", 6) == 0)
		return IC_FT3X07;
	if (strncmp(strIcName, "FT6416", 6) == 0)
		return IC_FT6416;
	if (strncmp(strIcName, "FT6336G/U", 9) == 0)
		return IC_FT6426;
	if (strncmp(strIcName, "FT6236U", 7) == 0)
		return IC_FT6236U;
	if (strncmp(strIcName, "FT6436U", 7) == 0)
		return IC_FT6436U;
	if (strncmp(strIcName, "FT3267", 6) == 0)
		return IC_FT3267;
	if (strncmp(strIcName, "FT3367", 6) == 0)
		return IC_FT3367;
	if (strncmp(strIcName, "FT7401", 6) == 0)
		return IC_FT7401;
	if (strncmp(strIcName, "FT3407U", 7) == 0)
		return IC_FT3407U;
	if (strncmp(strIcName, "FT5X16", 6) == 0)
		return IC_FT5X16;
	if (strncmp(strIcName, "FT5X12", 6) == 0)
		return IC_FT5X12;
	if (strncmp(strIcName, "FT5506", 6) == 0)
		return IC_FT5506;
	if (strncmp(strIcName, "FT5606", 6) == 0)
		return IC_FT5606;
	if (strncmp(strIcName, "FT5816", 6) == 0)
		return IC_FT5816;
	if (strncmp(strIcName, "FT5822", 6) == 0)
		return IC_FT5822;
	if (strncmp(strIcName, "FT5626", 6) == 0)
		return IC_FT5626;
	if (strncmp(strIcName, "FT5726", 6) == 0)
		return IC_FT5726;
	if (strncmp(strIcName, "FT5826B", 7) == 0)
		return IC_FT5826B;
	if (strncmp(strIcName, "FT3617", 6) == 0)
		return IC_FT3617;
	if (strncmp(strIcName, "FT3717", 6) == 0)
		return IC_FT3717;
	if (strncmp(strIcName, "FT7811", 6) == 0)
		return IC_FT7811;
	if (strncmp(strIcName, "FT5826S", 7) == 0)
		return IC_FT5826S;
	if (strncmp(strIcName, "FT5306", 6) == 0)
		return IC_FT5306;
	if (strncmp(strIcName, "FT5406", 6) == 0)
		return IC_FT5406;
	if (strncmp(strIcName, "FT8606", 6) == 0)
		return IC_FT8606;
	if (strncmp(strIcName, "FT8716", 6) == 0)
		return IC_FT8716;
	if (strncmp(strIcName, "FT3C47U", 7) == 0)
		return IC_FT3C47U;
	if (strncmp(strIcName, "FT8607", 6) == 0)
		return IC_FT8607;
	if (strncmp(strIcName, "FT8707", 6) == 0)
		return IC_FT8707;
	if (strncmp(strIcName, "FT8736", 6) == 0)
		return IC_FT8736;
	if (strncmp(strIcName, "FT3D47", 6) == 0)
		return IC_FT3D47;
	if (strncmp(strIcName, "FTE716", 6) == 0)
		return IC_FTE716;
	if (strncmp(strIcName, "FT5442", 6) == 0)
		return IC_FT5442;
	if (strncmp(strIcName, "FT3428U", 7) == 0)
		return IC_FT3428U;
	if (strncmp(strIcName, "FT8006", 6) == 0)
		return IC_FT8006;
	if (strncmp(strIcName, "FTE736", 6) == 0)
		return IC_FTE736;
	printk("%s.  can NOT get ic code.  ERROR  !!!  \n", __func__);
		return 0xff;
}

void fts_ic_table_get_ic_name_from_ic_code(unsigned int ucIcCode, char *strIcName)
{
	if (NULL == strIcName)
		return;
		sprintf(strIcName, "%s", "NA");/*if can't find IC , set 'NA'*/
	if (ucIcCode == IC_FT5X36)
		sprintf(strIcName, "%s", "FT5X36");
	if (ucIcCode == IC_FT5X36i)
		sprintf(strIcName, "%s",  "FT5X36i");
	if (ucIcCode == IC_FT3X16)
		sprintf(strIcName, "%s",  "FT3X16");
	if (ucIcCode == IC_FT3X26)
		sprintf(strIcName, "%s",  "FT3X26");
	if (ucIcCode == IC_FT5X46)
		sprintf(strIcName, "%s",  "FT5X46");
	if (ucIcCode == IC_FT5X46i)
		sprintf(strIcName, "%s",  "FT5X46i");
	if (ucIcCode == IC_FT5526)
		sprintf(strIcName, "%s",  "FT5526");
	if (ucIcCode == IC_FT3X17)
		sprintf(strIcName, "%s",  "FT3X17");
	if (ucIcCode == IC_FT5436)
		sprintf(strIcName, "%s",  "FT5436");
	if (ucIcCode == IC_FT3X27)
		sprintf(strIcName, "%s",  "FT3X27");
	if (ucIcCode == IC_FT5526I)
		sprintf(strIcName, "%s",  "FT5526i");
	if (ucIcCode == IC_FT5416)
		sprintf(strIcName, "%s",  "FT5416");
	if (ucIcCode == IC_FT5426)
		sprintf(strIcName, "%s",  "FT5426");
	if (ucIcCode == IC_FT5435)
		sprintf(strIcName, "%s",  "FT5435");
	if (ucIcCode == IC_FT7681)
		sprintf(strIcName, "%s",  "FT7681");
	if (ucIcCode == IC_FT7661)
		sprintf(strIcName, "%s",  "FT7661");
	if (ucIcCode == IC_FT7511)
		sprintf(strIcName, "%s",  "FT7511");
	if (ucIcCode == IC_FT7421)
		sprintf(strIcName, "%s",  "FT7421");
	if (ucIcCode == IC_FT6X06)
		sprintf(strIcName, "%s",  "FT6X06");
	if (ucIcCode == IC_FT3X06)
		sprintf(strIcName, "%s",  "FT3X06");
	if (ucIcCode == IC_FT6X36)
		sprintf(strIcName, "%s",  "FT6X36");
	if (ucIcCode == IC_FT3X07)
		sprintf(strIcName, "%s",  "FT3X07");
	if (ucIcCode == IC_FT6416)
		sprintf(strIcName, "%s",  "FT6416");
	if (ucIcCode == IC_FT6426)
		sprintf(strIcName, "%s",  "FT6336G/U");
	if (ucIcCode == IC_FT6236U)
		sprintf(strIcName, "%s",  "FT6236U");
	if (ucIcCode == IC_FT6436U)
		sprintf(strIcName, "%s",  "FT6436U");
	if (ucIcCode == IC_FT3267)
		sprintf(strIcName, "%s",  "FT3267");
	if (ucIcCode == IC_FT3367)
		sprintf(strIcName, "%s",  "FT3367");
	if (ucIcCode == IC_FT7401)
		sprintf(strIcName, "%s",  "FT7401");
	if (ucIcCode == IC_FT3407U)
		sprintf(strIcName, "%s",  "FT3407U");
	if (ucIcCode == IC_FT5X16)
		sprintf(strIcName, "%s",  "FT5X16");
	if (ucIcCode == IC_FT5X12)
		sprintf(strIcName, "%s",  "FT5X12");
	if (ucIcCode == IC_FT5506)
		sprintf(strIcName, "%s",  "FT5506");
	if (ucIcCode == IC_FT5606)
		sprintf(strIcName, "%s",  "FT5606");
	if (ucIcCode == IC_FT5816)
		sprintf(strIcName, "%s",  "FT5816");
	if (ucIcCode == IC_FT5822)
		sprintf(strIcName, "%s",  "FT5822");
	if (ucIcCode == IC_FT5626)
		sprintf(strIcName, "%s",  "FT5626");
	if (ucIcCode == IC_FT5726)
		sprintf(strIcName, "%s",  "FT5726");
	if (ucIcCode == IC_FT5826B)
		sprintf(strIcName, "%s",  "FT5826B");
	if (ucIcCode == IC_FT3617)
		sprintf(strIcName, "%s",  "FT3617");
	if (ucIcCode == IC_FT3717)
		sprintf(strIcName, "%s",  "FT3717");
	if (ucIcCode == IC_FT7811)
		sprintf(strIcName, "%s",  "FT7811");
	if (ucIcCode == IC_FT5826S)
		sprintf(strIcName, "%s",  "FT5826S");
	if (ucIcCode == IC_FT5306)
		sprintf(strIcName, "%s",  "FT5306");
	if (ucIcCode == IC_FT5406)
		sprintf(strIcName, "%s",  "FT5406");
	if (ucIcCode == IC_FT8606)
		sprintf(strIcName, "%s",  "FT8606");
	if (ucIcCode == IC_FT8716)
		sprintf(strIcName, "%s",  "FT8716");
	if (ucIcCode == IC_FT3C47U)
		sprintf(strIcName, "%s",  "FT3C47U");
	if (ucIcCode == IC_FT8607)
		sprintf(strIcName, "%s",  "FT8607");
	if (ucIcCode == IC_FT8707)
		sprintf(strIcName, "%s",  "FT8707");
	if (ucIcCode == IC_FT8736)
		sprintf(strIcName, "%s",  "FT8736");
	if (ucIcCode == IC_FT3D47)
		sprintf(strIcName, "%s",  "FT3D47");
	if (ucIcCode == IC_FTE716)
		sprintf(strIcName, "%s",  "FTE716");
	if (ucIcCode == IC_FT5442)
		sprintf(strIcName, "%s",  "FT5442");
	if (ucIcCode == IC_FT3428U)
		sprintf(strIcName, "%s",  "FT3428U");
	if (ucIcCode == IC_FT8006
		sprintf(strIcName, "%s",  "FT8006");
	if (ucIcCode == IC_FTE736)
		sprintf(strIcName, "%s",  "FTE736");
		return ;
}

unsigned int fts_ic_table_get_ic_code_from_chip_id(unsigned char chip_id, unsigned char chip_id2)
{
	unsigned int ic_code = 0;
	switch (chip_id) {
	case 0x55:
		ic_code = IC_FT5306;
		break;
	case 0x08:
		ic_code = IC_FT5606;
		break;
	case 0x0a:
		ic_code = IC_FT5X16;
		break;
	case 0x06:
		ic_code = IC_FT6X06;
		break;
	case 0x36:
		ic_code = IC_FT6X36;
		break;
	case 0x14:
		ic_code = IC_FT5X36;
		break;
	case 0x13:
		ic_code = IC_FT3X16;
		break;
	case 0x12:
		ic_code = IC_FT5X36i;
		break;
	case 0x11:
		ic_code = IC_FT5X36i;
		break;
	case 0x54:
		ic_code = IC_FT5X46;
		break;
	case 0x58:
		ic_code = IC_FT5822;
		break;
	case 0x86:
		if (0x06 == chip_id2)
			ic_code = IC_FT8606;
		else if (0x07 == chip_id2)
			ic_code = IC_FT8607;
		break;
	case 0x87:
		if (0x07 == chip_id2)
			ic_code = IC_FT8707;
		else if (0x16 == chip_id2)
			ic_code = IC_FT8716;
		else if (0x36 == chip_id2)
			ic_code = IC_FT8736;
		break;
	case 0xE7:
		if (0x16 == chip_id2)
			ic_code = IC_FTE716;
		else if (0x36 == chip_id2)
			ic_code = IC_FTE736;
		break;
	default:
			ic_code = 0;
		break;
	}
	return ic_code;
}
unsigned int fts_ic_table_get_chip_id_from_ic_code(unsigned int ic_code, unsigned char *chip_id, unsigned char *chip_id2)
{
	unsigned char uc_chip_id = 0;
	unsigned char uc_chip_id2 = 0;
	switch (ic_code>>4) {
	case (IC_FT5306>>4):
		uc_chip_id = 0x55;
		break;
	case (IC_FT5606>>4):
		uc_chip_id = 0x08;
		break;
	case (IC_FT5X16>>4):
		uc_chip_id = 0x0a;
		break;
	case (IC_FT6X06>>4):
		uc_chip_id = 0x06;
		break;
	case (IC_FT6X36>>4):
		uc_chip_id = 0x36;
		break;
	case (IC_FT5X36>>4):
		switch (ic_code) {
		case (IC_FT5X36):
			uc_chip_id = 0x14;
			break;
		case (IC_FT3X26):
			uc_chip_id = 0x13;
			break;
		case (IC_FT3X16):
			uc_chip_id = 0x12;
			break;
		case (IC_FT5X36i):
			uc_chip_id = 0x11;
			break;
		}
		break;
	case (IC_FT5X46>>4):
		uc_chip_id = 0x54;
		break;
	case (IC_FT5822>>4):
		uc_chip_id = 0x58;
		break;
	case (IC_FT8606>>4):
		uc_chip_id = 0x86;
		uc_chip_id2 = 0x06;
		break;
	case (IC_FT8607>>4):
		uc_chip_id = 0x86;
		uc_chip_id2 = 0x07;
		break;
	case (IC_FT8707>>4):
		uc_chip_id = 0x87;
		uc_chip_id2 = 0x07;
		break;
	case (IC_FT8716>>4):
		uc_chip_id = 0x87;
		uc_chip_id2 = 0x16;
		break;
	case (IC_FT8736>>4):
		uc_chip_id = 0x87;
		uc_chip_id2 = 0x36;
		break;
	case (IC_FTE716>>4):
		uc_chip_id = 0xE7;
		uc_chip_id2 = 0x16;
		break;
	case (IC_FTE736>>4):
		uc_chip_id = 0xE7;
		uc_chip_id2 = 0x36;
		break;
	default:
		uc_chip_id = 0;
		break;
	}
	*chip_id = uc_chip_id;
	*chip_id2 = uc_chip_id2;
	return 0;
}
int fts_ic_table_need_chip_id2(unsigned int chip_id)
{
	int b_need_id2 = -1;
	switch (chip_id) {
	case 0x86:
	case 0x87:
	case 0xE7:
		b_need_id2 = 0;
		printk("b_need_id2 = %d, chip_id = 0x%02x\n", b_need_id2, chip_id);
		break;
	default:
		b_need_id2 = -1;
		printk("b_need_id2 = %d, chip_id = 0x%02x\n", b_need_id2, chip_id);
		break;
	}
	return b_need_id2;
}
