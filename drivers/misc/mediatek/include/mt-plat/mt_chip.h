/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT_CHIP_H__
#define __MT_CHIP_H__

enum chip_sw_ver {
	CHIP_SW_VER_01 = 0x0000,
	CHIP_SW_VER_02 = 0x0001,
	CHIP_SW_VER_03 = 0x0002,
	CHIP_SW_VER_04 = 0x0003,
};

enum chip_info_id {
	CHIP_INFO_NONE = 0,
	CHIP_INFO_HW_CODE,
	CHIP_INFO_HW_SUBCODE,
	CHIP_INFO_HW_VER,
	CHIP_INFO_SW_VER,

	CHIP_INFO_REG_HW_CODE,
	CHIP_INFO_REG_HW_SUBCODE,
	CHIP_INFO_REG_HW_VER,
	CHIP_INFO_REG_SW_VER,

	CHIP_INFO_FUNCTION_CODE,
	CHIP_INFO_DATE_CODE,
	CHIP_INFO_PROJECT_CODE,
	CHIP_INFO_FAB_CODE,
	CHIP_INFO_WAFER_BIG_VER,

	CHIP_INFO_MAX,
	CHIP_INFO_ALL,
};


extern unsigned int mt_get_chip_id(void);
extern unsigned int mt_get_chip_hw_code(void);
extern unsigned int mt_get_chip_hw_subcode(void);
extern unsigned int mt_get_chip_hw_ver(void);
extern unsigned int mt_get_chip_sw_ver(void);
extern unsigned int mt_get_chip_info(unsigned int id);

#define get_chip_code mt_get_chip_hw_code

#endif
