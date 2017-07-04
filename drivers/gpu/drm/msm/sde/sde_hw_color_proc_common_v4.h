/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _SDE_HW_COLOR_PROC_COMMON_V4_H_
#define _SDE_HW_COLOR_PROC_COMMON_V4_H_

#define GAMUT_TABLE_SEL_OFF 0x4
#define GAMUT_SCALEA_OFFSET_OFF 0x10
#define GAMUT_SCALEB_OFFSET_OFF 0x50
#define GAMUT_LOWER_COLOR_OFF 0xc
#define GAMUT_UPPER_COLOR_OFF 0x8
#define GAMUT_TABLE0_SEL BIT(12)
#define GAMUT_MAP_EN BIT(1)
#define GAMUT_EN BIT(0)
#define GAMUT_MODE_13B_OFF 640
#define GAMUT_MODE_5_OFF 1248

enum {
	gamut_mode_17 = 0,
	gamut_mode_5,
	gamut_mode_13a,
	gamut_mode_13b,
};

#define GC_C0_OFF 0x4
#define GC_C0_INDEX_OFF 0x8
#define GC_8B_ROUND_EN BIT(1)
#define GC_EN BIT(0)
#define GC_TBL_NUM 3
#define GC_LUT_SWAP_OFF 0x1c

#define IGC_TBL_NUM 3
#define IGC_DITHER_OFF 0x7e0
#define IGC_OPMODE_OFF 0x0
#define IGC_C0_OFF 0x0
#define IGC_DATA_MASK (BIT(12) - 1)
#define IGC_DSPP_SEL_MASK_MAX (BIT(4) - 1)
#define IGC_DSPP_SEL_MASK(n) \
	((IGC_DSPP_SEL_MASK_MAX & ~(1 << (n))) << 28)
#define IGC_INDEX_UPDATE BIT(25)
#define IGC_EN BIT(0)
#define IGC_DIS 0
#define IGC_DITHER_DATA_MASK (BIT(4) - 1)

#define PCC_NUM_PLANES 3
#define PCC_NUM_COEFF 11
#define PCC_EN BIT(0)
#define PCC_DIS 0
#define PCC_C_OFF 0x4

#endif /* _SDE_HW_COLOR_PROC_COMMON_V4_H_ */
