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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <disp_drv_platform.h>
#if defined(CONFIG_MACH_MT6755) || defined(CONFIG_MACH_MT6797) || \
	defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#include <disp_helper.h>
#endif

#if defined(CONFIG_MTK_CLKMGR) || defined(CONFIG_MACH_MT6595) || \
	defined(CONFIG_MACH_MT6795)
#include <mach/mt_clkmgr.h>
#elif defined(CONFIG_MACH_MT6755) || defined(CONFIG_MACH_MT6797) || \
	defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#include "ddp_clkmgr.h"
#endif

#if defined(CONFIG_MACH_MT6799)
#include <primary_display.h>
#endif

#include "ddp_reg.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_color.h"
#include "cmdq_def.h"

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_MT6799)
#include "mt-plat/mtk_chip.h"
#endif

#if defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define COLOR_SUPPORT_PARTIAL_UPDATE
#endif

#define UNUSED(expr) (void)(expr)
/* global PQ param for kernel space */
static struct DISP_PQ_PARAM g_Color_Param[2] = {
	{
u4SHPGain:2,
u4SatGain:4,
u4PartialY:0,
u4HueAdj:{9, 9, 9, 9},
u4SatAdj:{0, 0, 0, 0},
u4Contrast:4,
u4Brightness:4,
u4Ccorr:0,
#if defined(COLOR_3_0)
u4ColorLUT:0
#endif
	 },
	{
u4SHPGain:2,
u4SatGain:4,
u4PartialY:0,
u4HueAdj:{9, 9, 9, 9},
u4SatAdj:{0, 0, 0, 0},
u4Contrast:4,
u4Brightness:4,
u4Ccorr:1,
#if defined(COLOR_3_0)
u4ColorLUT:0
#endif
	}
};

static struct DISP_PQ_PARAM g_Color_Cam_Param = {
u4SHPGain:0,
u4SatGain:4,
u4PartialY:0,
u4HueAdj:{9, 9, 9, 9},
u4SatAdj:{0, 0, 0, 0},
u4Contrast:4,
u4Brightness:4,
u4Ccorr:2,
#if defined(COLOR_3_0)
u4ColorLUT:0
#endif
};

static struct DISP_PQ_PARAM g_Color_Gal_Param = {
u4SHPGain:2,
u4SatGain:4,
u4PartialY:0,
u4HueAdj:{9, 9, 9, 9},
u4SatAdj:{0, 0, 0, 0},
u4Contrast:4,
u4Brightness:4,
u4Ccorr:3,
#if defined(COLOR_3_0)
u4ColorLUT:0
#endif
};

static struct DISP_PQ_DC_PARAM g_PQ_DC_Param = {
param:
	{
	 1, 1, 0, 0, 0, 0, 0, 0, 0, 0x0A,
	 0x30, 0x40, 0x06, 0x12, 40, 0x40, 0x80, 0x40, 0x40, 1,
	 0x80, 0x60, 0x80, 0x10, 0x34, 0x40, 0x40, 1, 0x80, 0xa,
	 0x19, 0x00, 0x20, 0, 0, 1, 2, 1, 80, 1}
};

static struct DISP_PQ_DS_PARAM g_PQ_DS_Param = {
param:
	{
	 1, -4, 1024, -4, 1024,
	 1, 400, 200, 1600, 800,
	 128, 8, 4, 12, 16,
	 8, 24, -8, -4, -12,
	 0, 0, 0}
};

static struct MDP_TDSHP_REG g_tdshp_reg = {
	TDS_GAIN_MID:0x10,
	TDS_GAIN_HIGH:0x20,
	TDS_COR_GAIN:0x10,
	TDS_COR_THR:0x4,
	TDS_COR_ZERO:0x2,
	TDS_GAIN:0x20,
	TDS_COR_VALUE:0x3
};

/* initialize index */
/* (because system default is 0, need fill with 0x80) */

static struct DISPLAY_PQ_T g_Color_Index = {
GLOBAL_SAT:	/* 0~9 */
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},

CONTRAST :	/* 0~9 */
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},

BRIGHTNESS :	/* 0~9 */
	{0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400},

PARTIAL_Y :
	{
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},


PURP_TONE_S :
{			/* hue 0~10 */
	{			/* 0 disable */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 1 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 2 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},
	{			/* 3 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 4 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 5 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 6 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 7 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 8 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 9 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 10 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 11 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 12 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 13 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 14 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 15 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 16 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 17 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 18 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	}
},
SKIN_TONE_S :
{
	{			/* 0 disable */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 1 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 2 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 3 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 4 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 5 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 6 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 7 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 8 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 9 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 10 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 11 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 12 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 13 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 14 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 15 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 16 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 17 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 18 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	}
},
GRASS_TONE_S :
{
	{			/* 0 disable */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 1 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 2 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 3 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 4 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 5 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 6 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 7 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 8 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 9 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 10 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 11 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 12 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 13 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 14 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 15 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 16 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 17 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	},

	{			/* 18 */
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
	}
},
SKY_TONE_S :
{			/* hue 0~10 */
	{			/* 0 disable */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 1 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 2 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},
	{			/* 3 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 4 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 5 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 6 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 7 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 8 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 9 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 10 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 11 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 12 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 13 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 14 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 15 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 16 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 17 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	},

	{			/* 18 */
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80}
	}
},

PURP_TONE_H :
{
	/* hue 0~2 */
	{0x80, 0x80, 0x80},	/* 3 */
	{0x80, 0x80, 0x80},	/* 4 */
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},	/* 3 */
	{0x80, 0x80, 0x80},	/* 4 */
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},	/* 4 */
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},	/* 3 */
	{0x80, 0x80, 0x80},	/* 4 */
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80}
},

SKIN_TONE_H :
{
	/* hue 3~16 */
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
},

GRASS_TONE_H :
{
/* hue 17~24 */
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
},

SKY_TONE_H :
{
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80}
},
CCORR_COEF : /* ccorr feature */
{
	{
		{0x400, 0x0, 0x0},
		{0x0, 0x400, 0x0},
		{0x0, 0x0, 0x400},
	},
	{
		{0x400, 0x0, 0x0},
		{0x0, 0x400, 0x0},
		{0x0, 0x0, 0x400},
	},
	{
		{0x400, 0x0, 0x0},
		{0x0, 0x400, 0x0},
		{0x0, 0x0, 0x400},
	},
	{
		{0x400, 0x0, 0x0},
		{0x0, 0x400, 0x0},
		{0x0, 0x0, 0x400}
	}
},
#if defined(COLOR_2_1)
S_GAIN_BY_Y :
{
	{0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80},

	{0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80},

	{0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80},

	{0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80},

	{0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80,
	 0x80, 0x80, 0x80, 0x80}
},

S_GAIN_BY_Y_EN:0,

LSP_EN:0,

LSP :
{0x0, 0x0, 0x7F, 0x7F, 0x7F, 0x0, 0x7F, 0x7F},
#endif
#if defined(COLOR_3_0)
COLOR_3D :
{
	{			/* 0 */
		/* Windows  1 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
		/* Windows  2 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
		/* Windows  3 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
	},
	{			/* 1 */
		/* Windows  1 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
		/* Windows  2 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
		/* Windows  3 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
	},
	{			/* 2 */
		/* Windows  1 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
		/* Windows  2 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
		/* Windows  3 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
	},
	{			/* 3 */
		/* Windows  1 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
		/* Windows  2 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
		/* Windows  3 */
		{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
		  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
		 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
	},
}
#endif
};

static DEFINE_MUTEX(g_color_reg_lock);
static struct DISPLAY_COLOR_REG g_color_reg;
static int g_color_reg_valid;

/* To enable debug log: */
/* # echo color_dbg:1 > /sys/kernel/debug/dispsys */
static unsigned int g_color_dbg_en;
#define COLOR_ERR(fmt, arg...) \
	pr_notice("[COLOR] %s: " fmt "\n", __func__, ##arg)
#define COLOR_DBG(fmt, arg...) \
	do { if (g_color_dbg_en) \
		pr_info("[COLOR] %s: " fmt "\n", __func__, ##arg); \
		} while (0)
#define COLOR_NLOG(fmt, arg...) \
	pr_debug("[COLOR] %s: " fmt "\n", __func__, ##arg)

static ddp_module_notify g_color_cb;

static struct DISPLAY_TDSHP_T g_TDSHP_Index;

static unsigned int g_split_en;
static unsigned int g_split_window_x_start;
static unsigned int g_split_window_y_start;
static unsigned int g_split_window_x_end = 0xFFFF;
static unsigned int g_split_window_y_end = 0xFFFF;

#if defined(COLOR_2_1)
	static unsigned long g_color_window = 0x40185E57;
#else
	static unsigned long g_color_window = 0x40106051;
#endif

static unsigned long g_color0_dst_w;
static unsigned long g_color0_dst_h;
static unsigned long g_color1_dst_w;
static unsigned long g_color1_dst_h;

#if defined(CONFIG_MACH_MT6799)
static unsigned int g_color_pos_x;
#define DISP_COLOR_POS_MAIN_OFFSET (0x484)
#define DISP_COLOR_POS_MAIN_POS_X_MASK (0x0000FFFF)
#define DISP_COLOR_POS_MAIN_POS_Y_MASK (0xFFFF0000)
#endif

static struct MDP_COLOR_CAP mdp_color_cap;

static int g_tdshp_flag;	/* 0: normal, 1: tuning mode */
int ncs_tuning_mode;
int tdshp_index_init;
static bool g_get_va_flag;

#if defined(DISP_COLOR_ON)
#define COLOR_MODE			(1)
#elif defined(MDP_COLOR_ON)
#define COLOR_MODE			(2)
#elif defined(DISP_MDP_COLOR_ON)
#define COLOR_MODE			(3)
#else
#define COLOR_MODE			(0)	/*color feature off */
#endif

#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
#define TDSHP_PA_BASE   0x14009000
#define TDSHP1_PA_BASE  0x1400A000
static unsigned long g_tdshp1_va;
#elif defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6739)
#define TDSHP_PA_BASE   0x14009000
#elif defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define TDSHP_PA_BASE   0x14007000
#elif defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761)
#define TDSHP_PA_BASE   0x1400A000
#else
#define TDSHP_PA_BASE   0x14006000
#endif

#if defined(NO_COLOR_SHARED)
#if defined(DISP_MDP_COLOR_ON) || defined(MDP_COLOR_ON)
#if defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799)
#define MDP_COLOR_PA_BASE 0x1400A000
#else
#define MDP_COLOR_PA_BASE 0x14007000
#endif
static unsigned long g_mdp_color_va;
#endif
#endif

#if defined(SUPPORT_ULTRA_RESOLUTION)
#define MDP_RSZ0_PA_BASE 0x14003000
static unsigned long g_mdp_rsz0_va;
#define MDP_RSZ1_PA_BASE 0x14004000
static unsigned long g_mdp_rsz1_va;
#define MDP_RSZ2_PA_BASE 0x14005000
static unsigned long g_mdp_rsz2_va;
#endif

#if defined(SUPPORT_HDR)
#if defined(HDR_IN_RDMA)
#define MDP_RDMA0_PA_BASE 0x14001000
static unsigned long g_mdp_rdma0_va;
#define MDP_HDR_OFFSET 0x00000800
#else
#define MDP_HDR_PA_BASE 0x1401c000
static unsigned long g_mdp_hdr_va;
#endif
#endif

#if defined(SUPPORT_MDP_AAL)
#include <linux/delay.h>
#if defined(CONFIG_MACH_MT6771) || defined(CONFIG_MACH_MT6779)
#define MDP_AAL0_PA_BASE 0x1401b000
#else
#define MDP_AAL0_PA_BASE 0x1401c000
#endif
#define DRE30_HIST_START         (1024)
#define DRE30_HIST_END           (4092)
static unsigned long g_mdp_aal0_va;
#endif

#define TRANSLATION(origin, shift) ((origin >= shift) ? (origin - shift) : 0)

static unsigned long g_tdshp_va;

#if defined(SUPPORT_WCG)
#define MDP_CCORR0_PA_BASE 0x14005000
static unsigned long g_mdp_ccorr0_va;
#endif

/* set cboost_en = 0 for projects before 6755 */
/* in order to resolve contour in some special color pattern */
#if defined(COLOR_2_1)
static bool g_config_color21 = true;
#else
static bool g_config_color21;
#endif
#if defined(COLOR_3_0)
static bool g_config_color30;
#endif

#if defined(CONFIG_MACH_MT6799)
#define COLOR_TOTAL_MODULE_NUM (2)
#define index_of_color(module) ((module == DISP_MODULE_COLOR0) ? 0 : 1)

#if defined(CONFIG_FPGA_EARLY_PORTING) || defined(DISP_COLOR_OFF)
static int g_color_bypass[COLOR_TOTAL_MODULE_NUM] = {1, 1};
#else
static int g_color_bypass[COLOR_TOTAL_MODULE_NUM];
#endif

static atomic_t g_color_is_clock_on[COLOR_TOTAL_MODULE_NUM] = { ATOMIC_INIT(0),
	ATOMIC_INIT(0)};
#else
#define COLOR_TOTAL_MODULE_NUM (1)
#define index_of_color(module) (0)

#if defined(CONFIG_FPGA_EARLY_PORTING) || defined(DISP_COLOR_OFF)
static int g_color_bypass[COLOR_TOTAL_MODULE_NUM] = {1};
#else
static int g_color_bypass[COLOR_TOTAL_MODULE_NUM];
#endif

static atomic_t g_color_is_clock_on[COLOR_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0)};
#endif

bool disp_color_reg_get(enum DISP_MODULE_ENUM module, unsigned long addr,
		unsigned int *value)
{
	if (COLOR_MODE == 2) {
		COLOR_DBG("color is not in DISP path");
		*value = 0;
		return true;
	}

	if (atomic_read(&g_color_is_clock_on[index_of_color(module)]) != 1) {
		COLOR_DBG("clock off .. skip reg set");
		return false;
	}

	*value = DISP_REG_GET(addr);
	return true;
}

void disp_color_set_window(unsigned int sat_upper, unsigned int sat_lower,
			   unsigned int hue_upper, unsigned int hue_lower)
{
	g_color_window = (sat_upper << 24) | (sat_lower << 16) |
					(hue_upper << 8) | (hue_lower);
}

static void ddp_color_cal_split_window(enum DISP_MODULE_ENUM module,
		unsigned int *p_split_window_x, unsigned int *p_split_window_y);

/* g_Color_Param */

struct DISP_PQ_PARAM *get_Color_config(int id)
{
	return &g_Color_Param[id];
}

struct DISP_PQ_PARAM *get_Color_Cam_config(void)
{
	return &g_Color_Cam_Param;
}

struct DISP_PQ_PARAM *get_Color_Gal_config(void)
{
	return &g_Color_Gal_Param;
}

/* g_Color_Index */

struct DISPLAY_PQ_T *get_Color_index(void)
{
	return &g_Color_Index;
}


struct DISPLAY_TDSHP_T *get_TDSHP_index(void)
{
	return &g_TDSHP_Index;
}

static void _color_reg_set(void *__cmdq, unsigned long addr,
		unsigned int value)
{
	struct cmdqRecStruct *cmdq = (struct cmdqRecStruct *) __cmdq;

	DISP_REG_SET(cmdq, addr, value);
}

static void _color_reg_mask(void *__cmdq, unsigned long addr,
		unsigned int value, unsigned int mask)
{
	struct cmdqRecStruct *cmdq = (struct cmdqRecStruct *) __cmdq;

	DISP_REG_MASK(cmdq, addr, value, mask);
}

static void _color_reg_set_field(void *__cmdq, unsigned int field_mask,
		unsigned long addr, unsigned int value)
{
	struct cmdqRecStruct *cmdq = (struct cmdqRecStruct *) __cmdq;

	DISP_REG_SET_FIELD(cmdq, field_mask, addr, value);
}

void DpEngine_COLORonInit(enum DISP_MODULE_ENUM module, void *__cmdq)
{
	/* pr_debug("================= init COLOR ====================="); */
	int offset = C0_OFFSET;
	void *cmdq = __cmdq;
#ifndef CONFIG_FPGA_EARLY_PORTING
	unsigned int split_window_x, split_window_y;
#endif

	offset = color_get_offset(module);

#ifndef CONFIG_FPGA_EARLY_PORTING
	ddp_color_cal_split_window(module, &split_window_x, &split_window_y);

	COLOR_DBG("module[%d], en[%d], x[0x%x], y[0x%x]", module, g_split_en,
		split_window_x, split_window_y);

	_color_reg_mask(cmdq, DISP_COLOR_DBG_CFG_MAIN + offset,
					g_split_en << 3, 0x00000008);
	_color_reg_set(cmdq, DISP_COLOR_WIN_X_MAIN + offset, split_window_x);
	_color_reg_set(cmdq, DISP_COLOR_WIN_Y_MAIN + offset, split_window_y);
#endif

	/* enable interrupt */
	_color_reg_mask(cmdq, DISP_COLOR_INTEN + offset, 0x00000007,
		0x00000007);

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* Set 10bit->8bit Rounding */
	_color_reg_mask(cmdq, DISP_COLOR_OUT_SEL + offset, 0x333,
		0x00000333);
#endif
}

void DpEngine_COLORonConfig(enum DISP_MODULE_ENUM module, void *__cmdq)
{
	int index = 0;
	unsigned int u4Temp = 0;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int offset = C0_OFFSET;
	struct DISP_PQ_PARAM *pq_param_p = &g_Color_Param[COLOR_ID_0];
	void *cmdq = __cmdq;
#if defined(COLOR_2_1) || defined(COLOR_3_0)
	int i, j, reg_index;
#if defined(COLOR_3_0)
	unsigned int pq_index;
#endif
#endif
	int wide_gamut_en = 0;

	if (is_color1_module(module)) {
		offset = C1_OFFSET;
		pq_param_p = &g_Color_Param[COLOR_ID_1];
	}

	if (pq_param_p->u4Brightness >= BRIGHTNESS_SIZE ||
	    pq_param_p->u4Contrast >= CONTRAST_SIZE ||
	    pq_param_p->u4SatGain >= GLOBAL_SAT_SIZE ||
	    pq_param_p->u4HueAdj[PURP_TONE] >= COLOR_TUNING_INDEX ||
	    pq_param_p->u4HueAdj[SKIN_TONE] >= COLOR_TUNING_INDEX ||
	    pq_param_p->u4HueAdj[GRASS_TONE] >= COLOR_TUNING_INDEX ||
	    pq_param_p->u4HueAdj[SKY_TONE] >= COLOR_TUNING_INDEX ||
	    pq_param_p->u4SatAdj[PURP_TONE] >= COLOR_TUNING_INDEX ||
	    pq_param_p->u4SatAdj[SKIN_TONE] >= COLOR_TUNING_INDEX ||
	    pq_param_p->u4SatAdj[GRASS_TONE] >= COLOR_TUNING_INDEX ||
	    pq_param_p->u4SatAdj[SKY_TONE] >= COLOR_TUNING_INDEX) {
		COLOR_ERR("[PQ][COLOR] Tuning index range error !\n");
		return;
	}
	/* COLOR_ERR("DpEngine_COLORonConfig(%d)", module); */

	if (g_color_bypass[index_of_color(module)] == 0) {
#if defined(COLOR_2_1)
		if (g_config_color21 == true) {
			_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset,
				(1 << 21)
				| (g_Color_Index.LSP_EN << 20)
				| (g_Color_Index.S_GAIN_BY_Y_EN << 15)
				| (wide_gamut_en << 8)
				| (0 << 7), 0x003081FF);
		}
#endif
		if (g_config_color21 == false) {
			/* disable wide_gamut */
			_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset
				, (0 << 8) | (0 << 7), 0x000001FF);
		}
		/* color start */
		_color_reg_mask(cmdq, DISP_COLOR_START + offset, 0x1, 0x3);
		/* enable R2Y/Y2R in Color Wrapper */
		if (g_config_color21 == true) {
			/* RDMA & OVL will enable wide-gamut function*/
			/* disable rgb clipping function in CM1 */
			/* to keep the wide-gamut range */
			_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset,
				0x01, 0x03);
		} else {
			_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset,
				0x01, 0x01);
		}
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
		_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0x01, 0x11);
#else
		/* also set no rounding on Y2R */
		_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0x11, 0x11);
#endif
	} else {
		_color_reg_set_field(cmdq, CFG_MAIN_FLD_COLOR_DBUF_EN,
			DISP_COLOR_CFG_MAIN + offset, 0x1);
		_color_reg_set_field(cmdq, START_FLD_DISP_COLOR_START,
			DISP_COLOR_START + offset, 0x1);
	}

	/* for partial Y contour issue */
	if (wide_gamut_en == 0)
		_color_reg_mask(cmdq, DISP_COLOR_LUMA_ADJ + offset,
			0x40, 0x0000007F);
	else if (wide_gamut_en == 1)
		_color_reg_mask(cmdq, DISP_COLOR_LUMA_ADJ + offset,
			0x0, 0x0000007F);

	/* config parameter from customer color_index.h */
	_color_reg_mask(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_1 + offset,
		(g_Color_Index.BRIGHTNESS[pq_param_p->u4Brightness] << 16) |
		g_Color_Index.CONTRAST[pq_param_p->u4Contrast], 0x07FF01FF);
	_color_reg_mask(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_2 + offset,
		g_Color_Index.GLOBAL_SAT[pq_param_p->u4SatGain], 0x000001FF);

	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		_color_reg_mask(cmdq,
		 DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index + offset,
		 (g_Color_Index.PARTIAL_Y[pq_param_p->u4PartialY][2 * index] |
		 g_Color_Index.PARTIAL_Y[pq_param_p->u4PartialY][2 * index + 1]
		 << 16), 0x00FF00FF);
	}

#if defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6755) || \
	defined(CONFIG_MACH_MT6799)
	_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0x80 << 16,
		0x00FF0000);
#endif

	if (g_config_color21 == false)
		_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0 << 13,
			0x00002000);
	else
		_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN_2 + offset,
			0x40 << 24,	0xFF000000);

	/* Partial Saturation Function */

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_0 + offset,
		(g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG1][0] |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG1][1] << 8 |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG1][2] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG1][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_1 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG1][1] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG1][2] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG1][3] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG1][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_2 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG1][5] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG1][6] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG1][7] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG1][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_3 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG1][1] |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG1][2] << 8 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG1][3] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG1][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_4 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG1][5] |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG1][0] << 8 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG1][1] << 16 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG1][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_0 + offset,
		(g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG2][0] |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG2][1] << 8 |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG2][2] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG2][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_1 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG2][1] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG2][2] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG2][3] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG2][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_2 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG2][5] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG2][6] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG2][7] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG2][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_3 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG2][1] |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG2][2] << 8 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG2][3] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG2][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_4 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG2][5] |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG2][0] << 8 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG2][1] << 16 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG2][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_0 + offset,
		(g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG3][0] |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG3][1] << 8 |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SG3][2] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG3][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_1 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG3][1] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG3][2] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG3][3] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG3][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_2 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG3][5] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG3][6] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SG3][7] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG3][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_3 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG3][1] |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG3][2] << 8 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG3][3] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG3][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_4 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SG3][5] |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG3][0] << 8 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG3][1] << 16 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SG3][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_0 + offset,
		(g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SP1][0] |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SP1][1] << 8 |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SP1][2] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP1][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_1 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP1][1] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP1][2] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP1][3] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP1][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_2 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP1][5] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP1][6] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP1][7] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP1][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_3 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP1][1] |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP1][2] << 8 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP1][3] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP1][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_4 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP1][5] |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SP1][0] << 8 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SP1][1] << 16 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SP1][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_0 + offset,
		(g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SP2][0] |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SP2][1] << 8 |
		g_Color_Index.PURP_TONE_S[pq_param_p->
			u4SatAdj[PURP_TONE]][SP2][2] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP2][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_1 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP2][1] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP2][2] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP2][3] << 16 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP2][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_2 + offset,
		(g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP2][5] |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP2][6] << 8 |
		g_Color_Index.SKIN_TONE_S[pq_param_p->
			u4SatAdj[SKIN_TONE]][SP2][7] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP2][0] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_3 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP2][1] |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP2][2] << 8 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP2][3] << 16 |
		g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP2][4] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_4 + offset,
		(g_Color_Index.GRASS_TONE_S[pq_param_p->
			u4SatAdj[GRASS_TONE]][SP2][5] |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SP2][0] << 8 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SP2][1] << 16 |
		g_Color_Index.SKY_TONE_S[pq_param_p->
			u4SatAdj[SKY_TONE]][SP2][2] << 24));

	for (index = 0; index < 3; index++) {
		h_series[index + PURP_TONE_START] =
			g_Color_Index.PURP_TONE_H[pq_param_p->
				u4HueAdj[PURP_TONE]][index];
	}

	for (index = 0; index < 8; index++) {
		h_series[index + SKIN_TONE_START] =
		    g_Color_Index.SKIN_TONE_H[pq_param_p->
				u4HueAdj[SKIN_TONE]][index];
	}

	for (index = 0; index < 6; index++) {
		h_series[index + GRASS_TONE_START] =
			g_Color_Index.GRASS_TONE_H[pq_param_p->
				u4HueAdj[GRASS_TONE]][index];
	}

	for (index = 0; index < 3; index++) {
		h_series[index + SKY_TONE_START] =
		    g_Color_Index.SKY_TONE_H[pq_param_p->
				u4HueAdj[SKY_TONE]][index];
	}

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) +
		    (h_series[4 * index + 1] << 8) +
		    (h_series[4 * index + 2] << 16) +
		    (h_series[4 * index + 3] << 24);
		_color_reg_set(cmdq, DISP_COLOR_LOCAL_HUE_CD_0 + offset +
			4 * index, u4Temp);
	}

#if defined(COLOR_2_1)
	if (g_config_color21 == true) {
		/* S Gain By Y */
		u4Temp = 0;

		reg_index = 0;
		for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT; i++) {
			for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
			u4Temp = (g_Color_Index.S_GAIN_BY_Y[i][j]) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 1] << 8) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 2] << 16) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 3] << 24);

			_color_reg_set(cmdq, DISP_COLOR_S_GAIN_BY_Y0_0 +
				offset + reg_index, u4Temp);
			reg_index += 4;
			}
		}
		/* LSP */
		_color_reg_mask(cmdq, DISP_COLOR_LSP_1 + offset,
		 (g_Color_Index.LSP[3] << 0) | (g_Color_Index.LSP[2] << 7) |
		 (g_Color_Index.LSP[1] << 14) | (g_Color_Index.LSP[0] << 22),
		 0x1FFFFFFF);
		_color_reg_mask(cmdq, DISP_COLOR_LSP_2 + offset,
		 (g_Color_Index.LSP[7] << 0) | (g_Color_Index.LSP[6] << 8) |
		 (g_Color_Index.LSP[5] << 16) | (g_Color_Index.LSP[4] << 23),
		 0x3FFF7F7F);
	}
#endif

	/* color window */

	_color_reg_set(cmdq, DISP_COLOR_TWO_D_WINDOW_1 + offset,
		g_color_window);
#if defined(COLOR_3_0)
	if (g_config_color30 == true) {
		_color_reg_mask(cmdq, DISP_COLOR_CM_CONTROL + offset,
			0x0 |
			(0x3 << 1) |	/* enable window 1 */
			(0x3 << 4) |	/* enable window 2 */
			(0x3 << 7)		/* enable window 3 */
			, 0x1B7);

		pq_index = pq_param_p->u4ColorLUT;
		for (i = 0; i < WIN_TOTAL; i++) {
			reg_index = i * 4 * (LUT_TOTAL * 5);
			for (j = 0; j < LUT_TOTAL; j++) {
				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_0 +
				 offset + reg_index,
				 g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_L] |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_U] << 10) |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_POINT0] << 20));

				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_1 +
				 offset + reg_index,
				 g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_POINT1] |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_POINT2] << 10) |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_POINT3] << 20));

				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_2 +
				 offset + reg_index,
				 g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_POINT4] |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_SLOPE0] << 10) |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_SLOPE1] << 20));

				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_3 +
				 offset + reg_index,
				 g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_SLOPE2] |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_SLOPE3] << 8) |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_SLOPE4] << 16) |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_SLOPE5] << 24));

				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_4 +
				 offset + reg_index,
				 g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_WGT_LSLOPE] |
				 (g_Color_Index.COLOR_3D[pq_index]
				 [i][j*LUT_REG_TOTAL+REG_WGT_USLOPE] << 16));

				reg_index += (4 * 5);
			}
		}
	}
#endif
}

static void color_write_hw_reg(enum DISP_MODULE_ENUM module,
	const struct DISPLAY_COLOR_REG *color_reg, void *cmdq)
{
	int offset = C0_OFFSET;
	int index = 0;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned int u4Temp = 0;
#if defined(COLOR_2_1) || defined(COLOR_3_0)
	int i, j, reg_index;
#endif
	int wide_gamut_en = 0;

	offset = color_get_offset(module);


	if (g_color_bypass[index_of_color(module)] == 0) {
#if defined(COLOR_2_1)
		if (g_config_color21 == true) {
			_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset,
				(1 << 21)
				| (g_Color_Index.LSP_EN << 20)
				| (g_Color_Index.S_GAIN_BY_Y_EN << 15)
				| (wide_gamut_en << 8)
				| (0 << 7), 0x003081FF);
		}
#endif
		if (g_config_color21 == false) {
			/* enable wide_gamut */
			_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset,
				(0 << 8) | (0 << 7), 0x000001FF);
		}
		/* color start */
		_color_reg_mask(cmdq, DISP_COLOR_START + offset, 0x1, 0x3);
		/* enable R2Y/Y2R in Color Wrapper */
		if (g_config_color21 == true) {
			/* RDMA & OVL will enable wide-gamut function*/
			/* disable rgb clipping function in CM1 */
			/* to keep the wide-gamut range */
			_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset, 0x01,
				0x03);
		} else {
			_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset, 0x01,
				0x01);
		}

#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
		_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0x01, 0x11);
#else
		/* also set no rounding on Y2R */
		_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0x11, 0x11);
#endif
	} else {
		_color_reg_set_field(cmdq, CFG_MAIN_FLD_COLOR_DBUF_EN,
			DISP_COLOR_CFG_MAIN + offset, 0x1);
		_color_reg_set_field(cmdq, START_FLD_DISP_COLOR_START,
			DISP_COLOR_START + offset, 0x1);
	}

	/* for partial Y contour issue */
	if (wide_gamut_en == 0)
		_color_reg_mask(cmdq, DISP_COLOR_LUMA_ADJ + offset,
			0x40, 0x0000007F);
	else if (wide_gamut_en == 1)
		_color_reg_mask(cmdq, DISP_COLOR_LUMA_ADJ + offset,
			0x0, 0x0000007F);

	_color_reg_mask(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_1 + offset,
		(color_reg->BRIGHTNESS << 16) | color_reg->CONTRAST,
		0x07FF01FF);
	_color_reg_mask(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_2 + offset,
		 color_reg->GLOBAL_SAT, 0x000001FF);


	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
	_color_reg_mask(cmdq, DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index + offset,
		(color_reg->PARTIAL_Y[2 * index] |
		(color_reg->PARTIAL_Y[2 * index + 1] << 16)), 0x00FF00FF);
	}

#if defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6755) || \
	defined(CONFIG_MACH_MT6799)
	_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0x80 << 16,
		0x00FF0000);
#endif

	if (g_config_color21 == false)
		_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0 << 13,
			0x00002000);
	else
		_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN_2 + offset,
			0x40 << 24,	0xFF000000);

	/* Partial Saturation Function */

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_0 + offset,
		(color_reg->PURP_TONE_S[SG1][0]) |
		(color_reg->PURP_TONE_S[SG1][1] << 8) |
		(color_reg->PURP_TONE_S[SG1][2] << 16) |
		(color_reg->SKIN_TONE_S[SG1][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_1 + offset,
		(color_reg->SKIN_TONE_S[SG1][1]) |
		(color_reg->SKIN_TONE_S[SG1][2] << 8) |
		(color_reg->SKIN_TONE_S[SG1][3] << 16) |
		(color_reg->SKIN_TONE_S[SG1][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_2 + offset,
		(color_reg->SKIN_TONE_S[SG1][5]) |
		(color_reg->SKIN_TONE_S[SG1][6] << 8) |
		(color_reg->SKIN_TONE_S[SG1][7] << 16) |
		(color_reg->GRASS_TONE_S[SG1][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_3 + offset,
		(color_reg->GRASS_TONE_S[SG1][1]) |
		(color_reg->GRASS_TONE_S[SG1][2] << 8) |
		(color_reg->GRASS_TONE_S[SG1][3] << 16) |
		(color_reg->GRASS_TONE_S[SG1][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_4 + offset,
		(color_reg->GRASS_TONE_S[SG1][5]) |
		(color_reg->SKY_TONE_S[SG1][0] << 8) |
		(color_reg->SKY_TONE_S[SG1][1] << 16) |
		(color_reg->SKY_TONE_S[SG1][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_0 + offset,
		(color_reg->PURP_TONE_S[SG2][0]) |
		(color_reg->PURP_TONE_S[SG2][1] << 8) |
		(color_reg->PURP_TONE_S[SG2][2] << 16) |
		(color_reg->SKIN_TONE_S[SG2][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_1 + offset,
		(color_reg->SKIN_TONE_S[SG2][1]) |
		(color_reg->SKIN_TONE_S[SG2][2] << 8) |
		(color_reg->SKIN_TONE_S[SG2][3] << 16) |
		(color_reg->SKIN_TONE_S[SG2][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_2 + offset,
		(color_reg->SKIN_TONE_S[SG2][5]) |
		(color_reg->SKIN_TONE_S[SG2][6] << 8) |
		(color_reg->SKIN_TONE_S[SG2][7] << 16) |
		(color_reg->GRASS_TONE_S[SG2][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_3 + offset,
		(color_reg->GRASS_TONE_S[SG2][1]) |
		(color_reg->GRASS_TONE_S[SG2][2] << 8) |
		(color_reg->GRASS_TONE_S[SG2][3] << 16) |
		(color_reg->GRASS_TONE_S[SG2][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_4 + offset,
		(color_reg->GRASS_TONE_S[SG2][5]) |
		(color_reg->SKY_TONE_S[SG2][0] << 8) |
		(color_reg->SKY_TONE_S[SG2][1] << 16) |
		(color_reg->SKY_TONE_S[SG2][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_0 + offset,
		(color_reg->PURP_TONE_S[SG3][0]) |
		(color_reg->PURP_TONE_S[SG3][1] << 8) |
		(color_reg->PURP_TONE_S[SG3][2] << 16) |
		(color_reg->SKIN_TONE_S[SG3][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_1 + offset,
		(color_reg->SKIN_TONE_S[SG3][1]) |
		(color_reg->SKIN_TONE_S[SG3][2] << 8) |
		(color_reg->SKIN_TONE_S[SG3][3] << 16) |
		(color_reg->SKIN_TONE_S[SG3][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_2 + offset,
		(color_reg->SKIN_TONE_S[SG3][5]) |
		(color_reg->SKIN_TONE_S[SG3][6] << 8) |
		(color_reg->SKIN_TONE_S[SG3][7] << 16) |
		(color_reg->GRASS_TONE_S[SG3][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_3 + offset,
		(color_reg->GRASS_TONE_S[SG3][1]) |
		(color_reg->GRASS_TONE_S[SG3][2] << 8) |
		(color_reg->GRASS_TONE_S[SG3][3] << 16) |
		(color_reg->GRASS_TONE_S[SG3][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_4 + offset,
		(color_reg->GRASS_TONE_S[SG3][5]) |
		(color_reg->SKY_TONE_S[SG3][0] << 8) |
		(color_reg->SKY_TONE_S[SG3][1] << 16) |
		(color_reg->SKY_TONE_S[SG3][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_0 + offset,
		(color_reg->PURP_TONE_S[SP1][0]) |
		(color_reg->PURP_TONE_S[SP1][1] << 8) |
		(color_reg->PURP_TONE_S[SP1][2] << 16) |
		(color_reg->SKIN_TONE_S[SP1][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_1 + offset,
		(color_reg->SKIN_TONE_S[SP1][1]) |
		(color_reg->SKIN_TONE_S[SP1][2] << 8) |
		(color_reg->SKIN_TONE_S[SP1][3] << 16) |
		(color_reg->SKIN_TONE_S[SP1][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_2 + offset,
		(color_reg->SKIN_TONE_S[SP1][5]) |
		(color_reg->SKIN_TONE_S[SP1][6] << 8) |
		(color_reg->SKIN_TONE_S[SP1][7] << 16) |
		(color_reg->GRASS_TONE_S[SP1][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_3 + offset,
		(color_reg->GRASS_TONE_S[SP1][1]) |
		(color_reg->GRASS_TONE_S[SP1][2] << 8) |
		(color_reg->GRASS_TONE_S[SP1][3] << 16) |
		(color_reg->GRASS_TONE_S[SP1][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_4 + offset,
		(color_reg->GRASS_TONE_S[SP1][5]) |
		(color_reg->SKY_TONE_S[SP1][0] << 8) |
		(color_reg->SKY_TONE_S[SP1][1] << 16) |
		(color_reg->SKY_TONE_S[SP1][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_0 + offset,
		(color_reg->PURP_TONE_S[SP2][0]) |
		(color_reg->PURP_TONE_S[SP2][1] << 8) |
		(color_reg->PURP_TONE_S[SP2][2] << 16) |
		(color_reg->SKIN_TONE_S[SP2][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_1 + offset,
		(color_reg->SKIN_TONE_S[SP2][1]) |
		(color_reg->SKIN_TONE_S[SP2][2] << 8) |
		(color_reg->SKIN_TONE_S[SP2][3] << 16) |
		(color_reg->SKIN_TONE_S[SP2][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_2 + offset,
		(color_reg->SKIN_TONE_S[SP2][5]) |
		(color_reg->SKIN_TONE_S[SP2][6] << 8) |
		(color_reg->SKIN_TONE_S[SP2][7] << 16) |
		(color_reg->GRASS_TONE_S[SP2][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_3 + offset,
		(color_reg->GRASS_TONE_S[SP2][1]) |
		(color_reg->GRASS_TONE_S[SP2][2] << 8) |
		(color_reg->GRASS_TONE_S[SP2][3] << 16) |
		(color_reg->GRASS_TONE_S[SP2][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_4 + offset,
		(color_reg->GRASS_TONE_S[SP2][5]) |
		(color_reg->SKY_TONE_S[SP2][0] << 8) |
		(color_reg->SKY_TONE_S[SP2][1] << 16) |
		(color_reg->SKY_TONE_S[SP2][2] << 24));

	for (index = 0; index < 3; index++)
		h_series[index + PURP_TONE_START] =
			color_reg->PURP_TONE_H[index];

	for (index = 0; index < 8; index++)
		h_series[index + SKIN_TONE_START] =
			color_reg->SKIN_TONE_H[index];

	for (index = 0; index < 6; index++)
		h_series[index + GRASS_TONE_START] =
			color_reg->GRASS_TONE_H[index];

	for (index = 0; index < 3; index++)
		h_series[index + SKY_TONE_START] =
			color_reg->SKY_TONE_H[index];

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) |
		    (h_series[4 * index + 1] << 8) |
		    (h_series[4 * index + 2] << 16) |
		    (h_series[4 * index + 3] << 24);
		_color_reg_set(cmdq, DISP_COLOR_LOCAL_HUE_CD_0 +
			offset + 4 * index, u4Temp);
	}

#if defined(COLOR_2_1)
	if (g_config_color21 == true) {
		/* S Gain By Y */
		u4Temp = 0;

		reg_index = 0;
		for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT; i++) {
			for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
				u4Temp = (g_Color_Index.S_GAIN_BY_Y[i][j]) +
				 (g_Color_Index.S_GAIN_BY_Y[i][j + 1] << 8) +
				 (g_Color_Index.S_GAIN_BY_Y[i][j + 2] << 16) +
				 (g_Color_Index.S_GAIN_BY_Y[i][j + 3] << 24);

				_color_reg_set(cmdq,
				 DISP_COLOR_S_GAIN_BY_Y0_0 + offset +
				 reg_index, u4Temp);
				reg_index += 4;
			}
		}
		/* LSP */
		_color_reg_mask(cmdq, DISP_COLOR_LSP_1 + offset,
		 (g_Color_Index.LSP[3] << 0) | (g_Color_Index.LSP[2] << 7) |
		 (g_Color_Index.LSP[1] << 14) | (g_Color_Index.LSP[0] << 22),
		 0x1FFFFFFF);
		_color_reg_mask(cmdq, DISP_COLOR_LSP_2 + offset,
		 (g_Color_Index.LSP[7] << 0) | (g_Color_Index.LSP[6] << 8) |
		 (g_Color_Index.LSP[5] << 16) | (g_Color_Index.LSP[4] << 23),
		 0x3FFF7F7F);
	}
#endif
	/* color window */
	_color_reg_set(cmdq, DISP_COLOR_TWO_D_WINDOW_1 + offset,
		g_color_window);
#if defined(COLOR_3_0)
	if (g_config_color30 == true) {
		_color_reg_mask(cmdq, DISP_COLOR_CM_CONTROL + offset,
			0x0 |
			(0x3 << 1) |	/* enable window 1 */
			(0x3 << 4) |	/* enable window 2 */
			(0x3 << 7)		/* enable window 3 */
			, 0x1B7);

		for (i = 0; i < WIN_TOTAL; i++) {
			reg_index = i * 4 * (LUT_TOTAL * 5);
			for (j = 0; j < LUT_TOTAL; j++) {
				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_0 +
				 offset + reg_index, (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_L]) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_U] << 10) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_POINT0] << 20));

				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_1 +
				  offset + reg_index, (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_POINT1]) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_POINT2] << 10) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_POINT3] << 20));

				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_2 +
				  offset + reg_index, (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_POINT4]) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_SLOPE0] << 10) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_SLOPE1] << 20));

				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_3 +
				  offset + reg_index, (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_SLOPE2]) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_SLOPE3] << 8) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_SLOPE4] << 16) |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_SLOPE5] << 24));

				_color_reg_set(cmdq, DISP_COLOR_CM_W1_HUE_4 +
				  offset + reg_index, color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_WGT_LSLOPE] |
				 (color_reg->COLOR_3D[i]
				 [j*LUT_REG_TOTAL+REG_WGT_USLOPE] << 16));

				reg_index += (4 * 5);
			}
		}
	}
#endif
}

static void color_trigger_refresh(enum DISP_MODULE_ENUM module)
{
	if (g_color_cb != NULL)
		g_color_cb(module, DISP_PATH_EVENT_TRIGGER);
	else
		COLOR_ERR("ddp listener is NULL!!\n");
}

static void ddp_color_bypass_color(enum DISP_MODULE_ENUM module, int bypass,
		void *__cmdq)
{
	int offset = C0_OFFSET;
	void *cmdq = __cmdq;

	g_color_bypass[index_of_color(module)] = bypass;

	offset = color_get_offset(module);

	if (bypass)
		_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 7),
			0x000000FF);	/* bypass all */
	else
		_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (0 << 7),
			0x000000FF);	/* resume all */
}

static void ddp_color_set_window(enum DISP_MODULE_ENUM module,
		struct DISP_PQ_WIN_PARAM *win_param, void *__cmdq)
{
	int offset = C0_OFFSET;
	void *cmdq = __cmdq;
	unsigned int split_window_x, split_window_y;

	offset = color_get_offset(module);

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		g_split_en = 1;
		g_split_window_x_start = win_param->start_x;
		g_split_window_y_start = win_param->start_y;
		g_split_window_x_end = win_param->end_x;
		g_split_window_y_end = win_param->end_y;
	} else {
		g_split_en = 0;
		g_split_window_x_start = 0x0000;
		g_split_window_y_start = 0x0000;
		g_split_window_x_end = 0xFFFF;
		g_split_window_y_end = 0xFFFF;
	}

	COLOR_DBG("input: module[%d], en[%d], x[0x%x], y[0x%x]",
	 module, g_split_en, ((win_param->end_x << 16) | win_param->start_x),
	 ((win_param->end_y << 16) | win_param->start_y));


	ddp_color_cal_split_window(module, &split_window_x, &split_window_y);

	COLOR_DBG("output: x[0x%x], y[0x%x]", split_window_x, split_window_y);

	_color_reg_mask(cmdq, DISP_COLOR_DBG_CFG_MAIN + offset,
		(g_split_en << 3), 0x00000008); /* split enable */
	_color_reg_set(cmdq, DISP_COLOR_WIN_X_MAIN + offset, split_window_x);
	_color_reg_set(cmdq, DISP_COLOR_WIN_Y_MAIN + offset, split_window_y);
}

static void ddp_color_cal_split_window(enum DISP_MODULE_ENUM module,
		unsigned int *p_split_window_x, unsigned int *p_split_window_y)
{
	unsigned int split_window_x = 0xFFFF0000;
	unsigned int split_window_y = 0xFFFF0000;
#if defined(CONFIG_MACH_MT6799)
	int pipeStatus = primary_display_get_pipe_status();
#endif

	/* save to global, can be applied on following PQ param updating. */
	if (g_color0_dst_w == 0 || g_color0_dst_h == 0) {
		COLOR_DBG
		 ("g_color0_dst_w/h not init, return default settings\n");
	} else if (g_split_en) {
#ifdef LCM_PHYSICAL_ROTATION_180
#if defined(CONFIG_MACH_MT6799)
		if (pipeStatus == SINGLE_PIPE) {
			split_window_x =
			 ((g_color0_dst_w - g_split_window_x_start) << 16)
			 | (g_color0_dst_w - g_split_window_x_end);
		} else if (pipeStatus == DUAL_PIPE) {
			if (module == DISP_MODULE_COLOR0) {
				split_window_x = ((g_color0_dst_w -
					g_split_window_x_start) << 16) |
					(g_color0_dst_w - g_split_window_x_end);
			} else if (is_color1_module(module)) {
				split_window_x = (TRANSLATION((
				 g_color0_dst_w - g_split_window_x_start),
				 g_color0_dst_w) << 16) | TRANSLATION((
				 g_color0_dst_w - g_split_window_x_end),
				 g_color0_dst_w);
			}
		}

		COLOR_DBG("pipeStatus[%d]", pipeStatus);
#else
		split_window_x =
		 ((g_color0_dst_w - g_split_window_x_start) << 16) |
		 (g_color0_dst_w - g_split_window_x_end);
#endif /* #if defined(CONFIG_MACH_MT6799) */
		split_window_y =
		 ((g_color0_dst_h - g_split_window_y_start) << 16) |
		 (g_color0_dst_h - g_split_window_y_end);
		COLOR_DBG("LCM_PHYSICAL_ROTATION_180");
#else
		split_window_y =
		 (g_split_window_y_end << 16) | g_split_window_y_start;
#ifdef LCM_PHYSICAL_ROTATION_270
#if defined(CONFIG_MACH_MT6799)
#else
		split_window_x = ((g_split_window_y_end) << 16) |
				g_split_window_y_start;
		split_window_y =
		 ((g_color0_dst_h - g_split_window_x_start) << 16) |
		 (g_color0_dst_h - g_split_window_x_end);
		COLOR_DBG("LCM_PHYSICAL_ROTATION_270");
#endif /* #if defined(CONFIG_MACH_MT6799) */
#else
#if defined(CONFIG_MACH_MT6799)
		if (pipeStatus == SINGLE_PIPE) {
			split_window_x = (g_split_window_x_end << 16) |
					g_split_window_x_start;
		} else if (pipeStatus == DUAL_PIPE) {
			if (module == DISP_MODULE_COLOR0) {
				split_window_x = (g_split_window_x_end << 16) |
							g_split_window_x_start;
			} else if (is_color1_module(module)) {
				split_window_x = (TRANSLATION(
				 g_split_window_x_end, g_color0_dst_w)
				 << 16) | TRANSLATION(
				 g_split_window_x_start, g_color0_dst_w);
			}
		}
#else
		split_window_x = (g_split_window_x_end << 16) |
			g_split_window_x_start;
#endif /* #if defined(CONFIG_MACH_MT6799) */
#endif /* #ifdef LCM_PHYSICAL_ROTATION_270 */
#endif /* #ifdef LCM_PHYSICAL_ROTATION_180 */
	}

	*p_split_window_x = split_window_x;
	*p_split_window_y = split_window_y;
}

static unsigned long color_get_TDSHP_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761)
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_tdshp0");
#else
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_tdshp");
#endif
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("TDSHP VA: 0x%lx\n", VA);

	return VA;
}

#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
static unsigned long color_get_TDSHP1_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_tdshp1");
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("TDSHP1 VA: 0x%lx\n", VA);

	return VA;
}
#endif

#if defined(NO_COLOR_SHARED)
#if defined(DISP_MDP_COLOR_ON) || defined(MDP_COLOR_ON)
static unsigned long color_get_MDP_COLOR_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_color");
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("MDP_COLOR VA: 0x%lx\n", VA);

	return VA;
}
#endif
#endif

#if defined(SUPPORT_ULTRA_RESOLUTION)
static unsigned long color_get_MDP_RSZ0_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_rsz0");
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("MDP_RSZ0 VA: 0x%lx\n", VA);

	return VA;
}

static unsigned long color_get_MDP_RSZ1_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_rsz1");
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("MDP_RSZ1 VA: 0x%lx\n", VA);

	return VA;
}

static unsigned long color_get_MDP_RSZ2_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_rsz2");
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("MDP_RSZ2 VA: 0x%lx\n", VA);

	return VA;
}
#endif

#if defined(SUPPORT_HDR)
#if defined(HDR_IN_RDMA)
static unsigned long color_get_MDP_RDMA0_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_rdma0");
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("MDP_RDMA0 VA: 0x%lx\n", VA);

	return VA;
}
#else
static unsigned long color_get_MDP_HDR_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_hdr0");
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("MDP_HDR VA: 0x%lx\n", VA);

	return VA;
}
#endif
#endif

#if defined(SUPPORT_MDP_AAL)
static unsigned long color_get_MDP_AAL0_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

#if defined(CONFIG_MACH_MT6771)
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_aal");
#else
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_aal0");
#endif
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("MDP_AAL0 VA: 0x%lx\n", VA);

	return VA;
}

static inline void dre_sram_read(unsigned int addr, unsigned int *value)
{
	unsigned int reg_value;
	unsigned int polling_time = 0;
	const unsigned int POLL_SLEEP_TIME_US = 10;
	const unsigned int MAX_POLL_TIME_US = 1000;

	DISP_REG_SET(NULL, g_mdp_aal0_va + 0xD4, addr);

	do {
		reg_value = DISP_REG_GET(g_mdp_aal0_va + 0xC8);

		if ((reg_value & (0x1 << 17)) == (0x1 << 17))
			break;

		udelay(POLL_SLEEP_TIME_US);
		polling_time += POLL_SLEEP_TIME_US;
	} while (polling_time < MAX_POLL_TIME_US);

	*value = DISP_REG_GET(g_mdp_aal0_va + 0xD8);
}

static void dump_dre_blk_histogram(void)
{
	int i;
	unsigned int value;

	for (i = DRE30_HIST_START; i < DRE30_HIST_END; i += 4) {
		dre_sram_read(i, &value);
		COLOR_NLOG("Hist add[%d], value[0x%08x]", i, value);
	}
}
#endif

#if defined(SUPPORT_WCG)
static unsigned long color_get_MDP_CCORR0_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_ccorr0");
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("MDP_CCORR0 VA: 0x%lx\n", VA);

	return VA;
}
#endif

static void _color_get_VA(void)
{
	/* check if va address initialized*/
	if (g_get_va_flag == false) {
		g_tdshp_va = color_get_TDSHP_VA();
#if defined(NO_COLOR_SHARED)
#if defined(DISP_MDP_COLOR_ON) || defined(MDP_COLOR_ON)
		g_mdp_color_va = color_get_MDP_COLOR_VA();
#endif
#endif
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
		g_tdshp1_va = color_get_TDSHP1_VA();
#endif
#if defined(SUPPORT_ULTRA_RESOLUTION)
		g_mdp_rsz0_va = color_get_MDP_RSZ0_VA();
		g_mdp_rsz1_va = color_get_MDP_RSZ1_VA();
		g_mdp_rsz2_va = color_get_MDP_RSZ2_VA();
#endif

#if defined(SUPPORT_HDR)
#if defined(HDR_IN_RDMA)
		g_mdp_rdma0_va = color_get_MDP_RDMA0_VA();
#else
		g_mdp_hdr_va = color_get_MDP_HDR_VA();
#endif
#endif

#if defined(SUPPORT_MDP_AAL)
		g_mdp_aal0_va = color_get_MDP_AAL0_VA();
#endif

#if defined(SUPPORT_WCG)
		g_mdp_ccorr0_va = color_get_MDP_CCORR0_VA();
#endif

		g_get_va_flag = true;
	}
}


static unsigned int color_is_reg_addr_valid(unsigned long addr)
{
	unsigned int i = 0;

	if (addr == 0) {
		COLOR_ERR("addr is NULL\n");
		return 0;
	}

	if ((addr & 0x3) != 0) {
		COLOR_ERR("addr is not 4-byte aligned!");
		return 0;
	}
#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	i = is_reg_addr_valid(1, addr);
	if (i) {
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n",
			addr, ddp_get_module_name(i));
		return 1;
	}
#else

	for (i = 0; i < DISP_REG_NUM; i++) {
		if ((addr >= dispsys_reg[i]) &&
			(addr < (dispsys_reg[i] + 0x1000)) &&
			(dispsys_reg[i] != 0))
			break;
	}

	if (i < DISP_REG_NUM) {
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			ddp_get_reg_module_name(i));
		return 1;
	}
#endif

	/*Check if MDP color base address*/
#if defined(NO_COLOR_SHARED)
#if defined(DISP_MDP_COLOR_ON) || defined(MDP_COLOR_ON)
	if ((addr >= g_mdp_color_va) && (addr < (g_mdp_color_va + 0x1000))) {
		/* MDP COLOR */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"MDP_COLOR");
		return 2;
	}
#endif
#endif

	/*Check if MDP RSZ base address*/
#if defined(SUPPORT_ULTRA_RESOLUTION)
	if ((addr >= g_mdp_rsz0_va) && (addr < (g_mdp_rsz0_va + 0x1000))) {
		/* MDP RSZ0 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"MDP_RSZ0");
		return 2;
	} else if ((addr >= g_mdp_rsz1_va) &&
				(addr < (g_mdp_rsz1_va + 0x1000))) {
		/* MDP RSZ1 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"MDP_RSZ1");
		return 2;
	} else if ((addr >= g_mdp_rsz2_va) &&
				(addr < (g_mdp_rsz2_va + 0x1000))) {
		/* MDP RSZ2 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"MDP_RSZ2");
		return 2;
	}
#endif

	/*Check if MDP HDR base address*/
#if defined(SUPPORT_HDR)
#if defined(HDR_IN_RDMA)
	if ((addr >= g_mdp_rdma0_va + MDP_HDR_OFFSET) &&
		(addr < (g_mdp_rdma0_va + 0x1000))) {
		/* MDP RDMA0 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"MDP_RDMA0");
		return 2;
	}
#else
	if ((addr >= g_mdp_hdr_va) &&
		(addr < (g_mdp_hdr_va + 0x1000))) {
		/* MDP HDR */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"MDP_HDR");
		return 2;
	}
#endif
#endif

	/*Check if MDP AAL base address*/
#if defined(SUPPORT_MDP_AAL)
	if ((addr >= g_mdp_aal0_va) &&
		(addr < (g_mdp_aal0_va + 0x1000))) {
		/* MDP AAL0 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"MDP_AAL0");
		if (addr >= g_mdp_aal0_va + 0xFF0)
			dump_dre_blk_histogram();
		return 2;
	}
#endif

#if defined(SUPPORT_WCG)
	/*Check if MDP CCORR base address*/
	if ((addr >= g_mdp_ccorr0_va) &&
		(addr < (g_mdp_ccorr0_va + 0x1000))) {
		/* MDP CCORR0 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"MDP_CCORR0");
		return 2;
	}
#endif

	/* check if TDSHP base address */
	if ((addr >= g_tdshp_va) && (addr < (g_tdshp_va + 0x1000))) {
		/* TDSHP0 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"TDSHP0");
		return 2;
	}
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
	else if ((addr >= g_tdshp1_va) && (addr < (g_tdshp1_va + 0x1000))) {
		/* TDSHP1 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr,
			"TDSHP1");
		return 2;
	}
#endif
	else {
		COLOR_ERR("invalid address! addr=0x%lx!\n", addr);
		return 0;
	}
}

static unsigned long color_pa2va(unsigned int addr)
{
	unsigned int i = 0;
	/* check base is not zero */
	if ((addr & 0xFFFF0000) == 0) {
		COLOR_ERR("invalid address! addr=0x%x!\n", addr);
		return 0;
	}
	_color_get_VA();

	/* check disp module */
#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	i = is_reg_addr_valid(0, addr);
	if (i) {
		COLOR_DBG("COLOR PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, (unsigned int)ddp_get_module_pa(i),
			ddp_get_module_va(i));
		return ddp_get_module_va(i) + (addr - ddp_get_module_pa(i));
	}
#else
	for (i = 0; i < DISP_REG_NUM; i++) {
		if ((addr >= ddp_reg_pa_base[i]) &&
			(addr < (ddp_reg_pa_base[i] + 0x1000))) {
		COLOR_DBG("COLOR PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, (unsigned int)ddp_reg_pa_base[i],
			dispsys_reg[i]);
		return dispsys_reg[i] + (addr - ddp_reg_pa_base[i]);
		}
	}
#endif

	/* TDSHP */
	if ((addr >= TDSHP_PA_BASE) && (addr < (TDSHP_PA_BASE + 0x1000))) {
		COLOR_DBG("TDSHP PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, TDSHP_PA_BASE, g_tdshp_va);
		return g_tdshp_va + (addr - TDSHP_PA_BASE);
	}
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
	/* TDSHP1 */
	if ((addr >= TDSHP1_PA_BASE) && (addr < (TDSHP1_PA_BASE + 0x1000))) {
		COLOR_DBG("TDSHP1 PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, TDSHP1_PA_BASE, g_tdshp1_va);
		return g_tdshp1_va + (addr - TDSHP1_PA_BASE);
	}
#endif

#if defined(NO_COLOR_SHARED)
#if defined(DISP_MDP_COLOR_ON) || defined(MDP_COLOR_ON)
	/* MDP_COLOR */
	if ((addr >= MDP_COLOR_PA_BASE) &&
		(addr < (MDP_COLOR_PA_BASE + 0x1000))) {
		COLOR_DBG("MDP_COLOR PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, MDP_COLOR_PA_BASE, g_mdp_color_va);
		return g_mdp_color_va + (addr - MDP_COLOR_PA_BASE);
	}
#endif
#endif

#if defined(SUPPORT_ULTRA_RESOLUTION)
	/* MDP_RSZ */
	if ((addr >= MDP_RSZ0_PA_BASE) &&
		(addr < (MDP_RSZ0_PA_BASE + 0x1000))) {
		COLOR_DBG("MDP_RSZ0 PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, MDP_RSZ0_PA_BASE, g_mdp_rsz0_va);
		return g_mdp_rsz0_va + (addr - MDP_RSZ0_PA_BASE);
	} else if ((addr >= MDP_RSZ1_PA_BASE) &&
		(addr < (MDP_RSZ1_PA_BASE + 0x1000))) {
		COLOR_DBG("MDP_RSZ1 PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, MDP_RSZ1_PA_BASE, g_mdp_rsz1_va);
		return g_mdp_rsz1_va + (addr - MDP_RSZ1_PA_BASE);
	} else if ((addr >= MDP_RSZ2_PA_BASE) &&
				(addr < (MDP_RSZ2_PA_BASE + 0x1000))) {
		COLOR_DBG("MDP_RSZ2 PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, MDP_RSZ2_PA_BASE, g_mdp_rsz2_va);
		return g_mdp_rsz2_va + (addr - MDP_RSZ2_PA_BASE);
	}
#endif

#if defined(SUPPORT_HDR)
	/* MDP_HDR */
#if defined(HDR_IN_RDMA)
	if ((addr >= MDP_RDMA0_PA_BASE + MDP_HDR_OFFSET) &&
		(addr < (MDP_RDMA0_PA_BASE + 0x1000))) {
		COLOR_DBG("MDP_RDMA0 PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, MDP_RDMA0_PA_BASE, g_mdp_rdma0_va);
		return g_mdp_rdma0_va + (addr - MDP_RDMA0_PA_BASE);
	}
#else
	if ((addr >= MDP_HDR_PA_BASE) &&
		(addr < (MDP_HDR_PA_BASE + 0x1000))) {
		COLOR_DBG("MDP_HDR PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, MDP_HDR_PA_BASE, g_mdp_hdr_va);
		return g_mdp_hdr_va + (addr - MDP_HDR_PA_BASE);
	}
#endif
#endif

#if defined(SUPPORT_MDP_AAL)
	/* MDP_AAL */
	if ((addr >= MDP_AAL0_PA_BASE) &&
		(addr < (MDP_AAL0_PA_BASE + 0x1000))) {
		COLOR_DBG("MDP_AAL0 PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, MDP_AAL0_PA_BASE, g_mdp_aal0_va);
		return g_mdp_aal0_va + (addr - MDP_AAL0_PA_BASE);
	}
#endif

#if defined(SUPPORT_WCG)
	/* MDP_CCORR */
	if ((addr >= MDP_CCORR0_PA_BASE) &&
		(addr < (MDP_CCORR0_PA_BASE + 0x1000))) {
		COLOR_DBG("MDP_CCORR0 PA:0x%x, PABase:0x%x, VABase:0x%lx",
			addr, MDP_CCORR0_PA_BASE, g_mdp_ccorr0_va);
		return g_mdp_ccorr0_va + (addr - MDP_CCORR0_PA_BASE);
	}
#endif

	COLOR_ERR("NO FOUND VA!! PA:0x%x", addr);

return 0;
}
#if defined(CONFIG_MACH_MT6757)
static unsigned int color_get_chip_ver(void)
{
	if (g_config_color30 == true)
		return MIRAVISION_HW_P_VERSION;
	else
		return MIRAVISION_HW_VERSION;
}
#endif
static unsigned int color_read_sw_reg(unsigned int reg_id)
{
	unsigned int ret = 0;

	if (reg_id >= SWREG_PQDS_DS_EN && reg_id <= SWREG_PQDS_GAIN_0) {
		ret = (unsigned int)g_PQ_DS_Param.
			param[reg_id - SWREG_PQDS_DS_EN];
		return ret;
	}
	if (reg_id >= SWREG_PQDC_BLACK_EFFECT_ENABLE &&
		reg_id <= SWREG_PQDC_DC_ENABLE) {
		ret = (unsigned int)g_PQ_DC_Param.
				param[reg_id - SWREG_PQDC_BLACK_EFFECT_ENABLE];
		return ret;
	}

	switch (reg_id) {
	case SWREG_COLOR_BASE_ADDRESS:
		{
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795) || \
	defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799)
			ret = ddp_reg_pa_base[DISP_REG_COLOR0];
#elif defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
			ret = ddp_get_module_pa(DISP_MODULE_COLOR0);
#else
			ret = ddp_reg_pa_base[DISP_REG_COLOR];
#endif
			break;
		}

	case SWREG_GAMMA_BASE_ADDRESS:
		{
#if defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_MT6799)
			ret = ddp_reg_pa_base[DISP_REG_GAMMA0];
#elif defined(CONFIG_MACH_MT6759)
			ret = ddp_get_module_pa(DISP_MODULE_GAMMA);
#elif defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
			ret = ddp_get_module_pa(DISP_MODULE_GAMMA0);

#else
			ret = ddp_reg_pa_base[DISP_REG_GAMMA];
#endif
			break;
		}

	case SWREG_AAL_BASE_ADDRESS:
		{
#if defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799)
			ret = ddp_reg_pa_base[DISP_REG_AAL0];
#elif defined(CONFIG_MACH_MT6759)
			ret = ddp_get_module_pa(DISP_MODULE_AAL);
#elif defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
			ret = ddp_get_module_pa(DISP_MODULE_AAL0);
#else
			ret = ddp_reg_pa_base[DISP_REG_AAL];
#endif
			break;
		}

#if defined(CCORR_SUPPORT)
	case SWREG_CCORR_BASE_ADDRESS:
		{
#if defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799)
			ret = ddp_reg_pa_base[DISP_REG_CCORR0];
#elif defined(CONFIG_MACH_MT6759)
			ret = ddp_get_module_pa(DISP_MODULE_CCORR);
#elif defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
			ret = ddp_get_module_pa(DISP_MODULE_CCORR0);

#else
			ret = ddp_reg_pa_base[DISP_REG_CCORR];
#endif
			break;
		}
#endif

	case SWREG_TDSHP_BASE_ADDRESS:
		{
			ret = TDSHP_PA_BASE;
			break;
		}
#if defined(DISP_MDP_COLOR_ON) || defined(MDP_COLOR_ON)
	case SWREG_MDP_COLOR_BASE_ADDRESS:
		{
#if defined(NO_COLOR_SHARED)
			ret = MDP_COLOR_PA_BASE;
#else
			ret = ddp_get_module_pa(DISP_MODULE_COLOR0);
#endif
			break;
		}
#endif
	case SWREG_COLOR_MODE:
		{
			ret = COLOR_MODE;
			break;
		}

	case SWREG_RSZ_BASE_ADDRESS:
		{
#if defined(SUPPORT_ULTRA_RESOLUTION)
			ret = MDP_RSZ0_PA_BASE;
#endif
			break;
		}

	case SWREG_MDP_RDMA_BASE_ADDRESS:
		{
#if defined(SUPPORT_HDR)
#if defined(HDR_IN_RDMA)
			ret = MDP_RDMA0_PA_BASE;
#endif
#endif
			break;
		}

	case SWREG_MDP_AAL_BASE_ADDRESS:
		{
#if defined(SUPPORT_AAL)
			ret = MDP_AAL0_PA_BASE;
#endif
			break;
		}

	case SWREG_MDP_HDR_BASE_ADDRESS:
		{
#if defined(SUPPORT_HDR)
#if !defined(HDR_IN_RDMA)
			ret = MDP_HDR_PA_BASE;
#endif
#endif
			break;
		}

	case SWREG_TDSHP_TUNING_MODE:
		{
			ret = (unsigned int)g_tdshp_flag;
			break;
		}

	case SWREG_MIRAVISION_VERSION:
		{
			ret = MIRAVISION_VERSION;
			break;
		}

	case SWREG_SW_VERSION_VIDEO_DC:
		{
			ret = SW_VERSION_VIDEO_DC;
			break;
		}

	case SWREG_SW_VERSION_AAL:
		{
			ret = SW_VERSION_AAL;
			break;
		}

	default:
		break;

	}

	return ret;
}

static void color_write_sw_reg(unsigned int reg_id, unsigned int value)
{
	if (reg_id >= SWREG_PQDC_BLACK_EFFECT_ENABLE &&
		reg_id <= SWREG_PQDC_DC_ENABLE) {
		g_PQ_DC_Param.param[reg_id - SWREG_PQDC_BLACK_EFFECT_ENABLE] =
			(int)value;
		return;
	}

	if (reg_id >= SWREG_PQDS_DS_EN && reg_id <= SWREG_PQDS_GAIN_0) {
		g_PQ_DS_Param.param[reg_id - SWREG_PQDS_DS_EN] = (int)value;
		return;
	}

	switch (reg_id) {
	case SWREG_TDSHP_TUNING_MODE:
		{
			g_tdshp_flag = (int)value;
			break;
		}
	case SWREG_MDP_COLOR_CAPTURE_EN:
		{
			mdp_color_cap.en = value;
			break;
		}
	case SWREG_MDP_COLOR_CAPTURE_POS_X:
		{
			mdp_color_cap.pos_x = value;
			break;
		}
	case SWREG_MDP_COLOR_CAPTURE_POS_Y:
		{
			mdp_color_cap.pos_y = value;
			break;
		}
	case SWREG_TDSHP_GAIN_MID:
		{
			g_tdshp_reg.TDS_GAIN_MID = value;
			break;
		}
	case SWREG_TDSHP_GAIN_HIGH:
		{
			g_tdshp_reg.TDS_GAIN_HIGH = value;
			break;
		}
	case SWREG_TDSHP_COR_GAIN:
		{
			g_tdshp_reg.TDS_COR_GAIN = value;
			break;
		}
	case SWREG_TDSHP_COR_THR:
		{
			g_tdshp_reg.TDS_COR_THR = value;
			break;
		}
	case SWREG_TDSHP_COR_ZERO:
		{
			g_tdshp_reg.TDS_COR_ZERO = value;
			break;
		}
	case SWREG_TDSHP_GAIN:
		{
			g_tdshp_reg.TDS_GAIN = value;
			break;
		}
	case SWREG_TDSHP_COR_VALUE:
		{
			g_tdshp_reg.TDS_COR_VALUE = value;
			break;
		}

	default:
		break;

	}
}


static int _color_clock_on(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
	atomic_set(&g_color_is_clock_on[index_of_color(module)], 1);

#if defined(CONFIG_MACH_MT6755)
	/* color is DCM , do nothing */
	return 0;
#endif

#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
	return 0;
#else

#ifdef ENABLE_CLK_MGR
#ifdef CONFIG_MTK_CLKMGR
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
	if (module == DISP_MODULE_COLOR0) {
		enable_clock(MT_CG_DISP0_DISP_COLOR0, "DDP");
		COLOR_DBG("color[0]_clock_on CG 0x%x\n",
			  DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	} else {
		enable_clock(MT_CG_DISP0_DISP_COLOR1, "DDP");
		COLOR_DBG("color[1]_clock_on CG 0x%x\n",
			  DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	}
#else
	enable_clock(MT_CG_DISP0_DISP_COLOR, "DDP");
	COLOR_DBG("color[0]_clock_on CG 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#endif
#else
#if defined(CONFIG_MACH_MT6799)
	if (module == DISP_MODULE_COLOR0)
		ddp_clk_enable(DISP0_DISP_COLOR0);
	else if (is_color1_module(module))
		ddp_clk_enable(DISP0_DISP_COLOR1);
#else
	ddp_clk_enable(DISP0_DISP_COLOR);
#endif
	COLOR_DBG("color[0]_clock_on CG 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#endif
#endif

	return 0;
#endif

}

static int _color_clock_off(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
	atomic_set(&g_color_is_clock_on[index_of_color(module)], 0);

#if defined(CONFIG_MACH_MT6755)
	/* color is DCM , do nothing */
	return 0;
#endif

#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
	return 0;
#else


#ifdef ENABLE_CLK_MGR
#ifdef CONFIG_MTK_CLKMGR
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
	if (module == DISP_MODULE_COLOR0) {
		COLOR_DBG("color[0]_clock_off\n");
		disable_clock(MT_CG_DISP0_DISP_COLOR0, "DDP");
	} else {
		COLOR_DBG("color[1]_clock_off\n");
		disable_clock(MT_CG_DISP0_DISP_COLOR1, "DDP");
	}
#else
	COLOR_DBG("color[0]_clock_off\n");
	disable_clock(MT_CG_DISP0_DISP_COLOR, "DDP");
#endif
#else
#if defined(CONFIG_MACH_MT6799)
	if (module == DISP_MODULE_COLOR0)
		ddp_clk_disable(DISP0_DISP_COLOR0);
	else if (is_color1_module(module))
		ddp_clk_disable(DISP0_DISP_COLOR1);
#else
		ddp_clk_disable(DISP0_DISP_COLOR);
#endif

#endif
#endif

	return 0;
#endif
}

static int _color_init(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
#if !defined(CONFIG_MACH_MT6759) && !defined(CONFIG_MACH_MT6739)
	_color_clock_on(module, cmq_handle);
#endif

	_color_get_VA();

#if defined(CONFIG_MACH_MT6757)
	if (mt_get_chip_hw_ver() >= 0xCB00)
		g_config_color30 = true;
	else
		g_config_color21 = false;
#elif defined(CONFIG_MACH_MT6799)
	if (mt_get_chip_sw_ver() == 0x0001) { /* E2 */
		g_config_color30 = true;
	}
#elif defined(COLOR_3_0)
	g_config_color30 = true;
#endif

	return 0;
}

static int _color_deinit(enum DISP_MODULE_ENUM module, void *cmq_handle)
{
#if !defined(CONFIG_MACH_MT6759) && !defined(CONFIG_MACH_MT6739)
	_color_clock_off(module, cmq_handle);
#endif
	return 0;
}

static int _color_config(enum DISP_MODULE_ENUM module,
		struct disp_ddp_path_config *pConfig, void *cmq_handle)
{
	int offset = C0_OFFSET;
	void *cmdq = cmq_handle;

	if (!pConfig->dst_dirty)
		return 0;

	if (module == DISP_MODULE_COLOR0) {
		g_color0_dst_w = pConfig->dst_w;
		g_color0_dst_h = pConfig->dst_h;
	} else {
		g_color1_dst_w = pConfig->dst_w;
		g_color1_dst_h = pConfig->dst_h;
		offset = C1_OFFSET;
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
#ifndef CONFIG_FOR_SOURCE_PQ
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset,
			pConfig->dst_w, 0x00003FFF);  /* wrapper width */
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset,
			pConfig->dst_h, 0x00003FFF);  /* wrapper height */
		return 0;
#endif
#endif
	}

	_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset,
		pConfig->dst_w, 0x00003FFF);  /* wrapper width */
	_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset,
		pConfig->dst_h, 0x00003FFF);  /* wrapper height */

#ifdef DISP_PLATFORM_HAS_SHADOW_REG
	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
		if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
			/* full shadow mode*/
			_color_reg_set(cmdq, DISP_COLOR_SHADOW_CTRL, 0);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 1) {
			/* force commit */
			_color_reg_set(cmdq, DISP_COLOR_SHADOW_CTRL, 2);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 2) {
			/* bypass shadow */
			_color_reg_set(cmdq, DISP_COLOR_SHADOW_CTRL, 1);
		}
	}
#endif

#ifdef DISP_PLATFORM_HAS_SHADOW_REG
	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
		if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
			/* full shadow mode*/
			_color_reg_set(cmdq, DISP_COLOR_SHADOW_CTRL, 0);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 1) {
			/* force commit */
			_color_reg_set(cmdq, DISP_COLOR_SHADOW_CTRL, 2);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 2) {
			/* bypass shadow */
			_color_reg_set(cmdq, DISP_COLOR_SHADOW_CTRL, 1);
		}
	}
#endif

	return 0;
}

static int _color_start(enum DISP_MODULE_ENUM module, void *cmdq)
{
	if (is_color1_module(module)) {
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
#ifndef CONFIG_FOR_SOURCE_PQ
		/* set bypass to COLOR1 */
		{
			const int offset = C1_OFFSET;
			/* disable R2Y/Y2R in Color Wrapper */
			_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset,
				0, 0x1);
			_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset,
				0, 0x1);
			_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN +
				offset, (1 << 7), 0xFF);  /* all bypass */
			_color_reg_mask(cmdq, DISP_COLOR_START + offset,
				0x00000003,	0x3);	/* color start */
		}
		return 0;
#endif
#endif
	}
	DpEngine_COLORonInit(module, cmdq);

	mutex_lock(&g_color_reg_lock);
	if (g_color_reg_valid) {
		color_write_hw_reg(module, &g_color_reg, cmdq);
		mutex_unlock(&g_color_reg_lock);
	} else {
		mutex_unlock(&g_color_reg_lock);
		DpEngine_COLORonConfig(module, cmdq);
	}

	return 0;
}

static int _color_set_listener(enum DISP_MODULE_ENUM module,
		ddp_module_notify notify)
{
	g_color_cb = notify;
	return 0;
}

#if defined(COLOR_SUPPORT_PARTIAL_UPDATE)
static int _color_partial_update(enum DISP_MODULE_ENUM module,
		void *arg, void *cmdq)
{
	struct disp_rect *roi = (struct disp_rect *) arg;
	int width = roi->width;
	int height = roi->height;
	int offset = C0_OFFSET;

	if (module == DISP_MODULE_COLOR0) {
		g_color0_dst_w = width;
		g_color0_dst_h = height;
	} else {
		g_color1_dst_w = width;
		g_color1_dst_h = height;
		offset = C1_OFFSET;
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
#ifndef CONFIG_FOR_SOURCE_PQ
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset,
			width, 0x00003FFF);  /* wrapper width */
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset,
			height, 0x00003FFF);  /* wrapper height */

		return 0;
#endif
#endif
	}
	_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset, width,
		0x00003FFF);  /* wrapper width */
	_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, height,
		0x00003FFF);  /* wrapper height */

	return 0;
}

static int color_ioctl(enum DISP_MODULE_ENUM module, void *handle,
		enum DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_color_partial_update(module, params, handle);
		ret = 0;
	}

	return ret;
}
#endif

static int _color_io(enum DISP_MODULE_ENUM module, unsigned int msg,
		unsigned long arg, void *cmdq)
{
	/* legacy chip use driver .cmd to call _color_io */
	/* After mt6763 directly call ioctl_function from ddp_manager */
	return disp_color_ioctl(module, msg, arg, cmdq);
}

int disp_color_ioctl(enum DISP_MODULE_ENUM module, unsigned int msg,
		unsigned long arg, void *cmdq)
{

	int ret = 0;
	int value = 0;
	struct DISP_PQ_PARAM *pq_param;
	struct DISPLAY_PQ_T *pq_index;
	struct DISPLAY_TDSHP_T *tdshp_index;

	COLOR_DBG("ioctl_function: module %d, msg %x", module, msg);
	/* COLOR_ERR("_color_io: GET_PQPARAM %lx", DISP_IOCTL_GET_PQPARAM); */
	/* COLOR_ERR("_color_io: SET_PQPARAM %lx", DISP_IOCTL_SET_PQPARAM); */
	/* COLOR_ERR("_color_io: READ_REG %lx", DISP_IOCTL_READ_REG); */
	/* COLOR_ERR("_color_io: WRITE_REG %lx", DISP_IOCTL_WRITE_REG); */

	switch (msg) {
	case DISP_IOCTL_SET_PQPARAM:
		/* case DISP_IOCTL_SET_C0_PQPARAM: */

		pq_param = get_Color_config(COLOR_ID_0);
		if (copy_from_user(pq_param, (void *)arg,
			sizeof(struct DISP_PQ_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_SET_PQPARAM Copy from user fail");

			return -EFAULT;
		}

		if (ncs_tuning_mode == 0) {
			/* normal mode */
			DpEngine_COLORonInit(module, cmdq);
			DpEngine_COLORonConfig(module, cmdq);

			color_trigger_refresh(module);

			COLOR_DBG("SET_PQ_PARAM(0)");
		} else {
			/* ncs_tuning_mode = 0; */
			COLOR_DBG
			 ("SET_PQ_PARAM(0), bypassed by ncs_tuning_mode = 1");
		}

		break;

	case DISP_IOCTL_GET_PQPARAM:
		/* case DISP_IOCTL_GET_C0_PQPARAM: */

		pq_param = get_Color_config(COLOR_ID_0);
		if (copy_to_user((void *)arg, pq_param,
			sizeof(struct DISP_PQ_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_GET_PQPARAM Copy to user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_PQINDEX:
		COLOR_DBG("DISP_IOCTL_SET_PQINDEX!");

		pq_index = get_Color_index();
		if (copy_from_user(pq_index, (void *)arg,
			sizeof(struct DISPLAY_PQ_T))) {
			COLOR_ERR
			 ("DISP_IOCTL_SET_PQINDEX Copy from user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQINDEX:

		pq_index = get_Color_index();
		if (copy_to_user((void *)arg, pq_index,
			sizeof(struct DISPLAY_PQ_T))) {
			COLOR_ERR
			 ("DISP_IOCTL_GET_PQPARAM Copy to user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_COLOR_REG:
		COLOR_DBG("DISP_IOCTL_SET_COLOR_REG");

		mutex_lock(&g_color_reg_lock);
		if (copy_from_user(&g_color_reg, (void *)arg,
			sizeof(struct DISPLAY_COLOR_REG))) {
			mutex_unlock(&g_color_reg_lock);
			COLOR_ERR
			 ("DISP_IOCTL_SET_COLOR_REG Copy from user fail");
			return -EFAULT;
		}

		color_write_hw_reg(module, &g_color_reg, cmdq);
		g_color_reg_valid = 1;
		mutex_unlock(&g_color_reg_lock);

		color_trigger_refresh(module);

		break;

	case DISP_IOCTL_SET_TDSHPINDEX:

		COLOR_DBG("DISP_IOCTL_SET_TDSHPINDEX!");

		tdshp_index = get_TDSHP_index();
		if (copy_from_user(tdshp_index, (void *)arg,
			sizeof(struct DISPLAY_TDSHP_T))) {
			COLOR_ERR
			 ("DISP_IOCTL_SET_TDSHPINDEX Copy from user fail");
			return -EFAULT;
		}
		tdshp_index_init = 1;
		break;

	case DISP_IOCTL_GET_TDSHPINDEX:

		if (tdshp_index_init == 0) {
			COLOR_ERR
			 ("DISP_IOCTL_GET_TDSHPINDEX TDSHPINDEX not init");
			return -EFAULT;
		}
		tdshp_index = get_TDSHP_index();
		if (copy_to_user((void *)arg, tdshp_index,
			sizeof(struct DISPLAY_TDSHP_T))) {
			COLOR_ERR
			 ("DISP_IOCTL_GET_TDSHPINDEX Copy to user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_PQ_CAM_PARAM:

		pq_param = get_Color_Cam_config();
		if (copy_from_user(pq_param, (void *)arg,
			sizeof(struct DISP_PQ_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_SET_PQ_CAM_PARAM Copy from user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQ_CAM_PARAM:

		pq_param = get_Color_Cam_config();
		if (copy_to_user((void *)arg, pq_param,
			sizeof(struct DISP_PQ_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_GET_PQ_CAM_PARAM Copy to user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_PQ_GAL_PARAM:

		pq_param = get_Color_Gal_config();
		if (copy_from_user(pq_param, (void *)arg,
			sizeof(struct DISP_PQ_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_SET_PQ_GAL_PARAM Copy from user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQ_GAL_PARAM:

		pq_param = get_Color_Gal_config();
		if (copy_to_user((void *)arg, pq_param,
			sizeof(struct DISP_PQ_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_GET_PQ_GAL_PARAM Copy to user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_MUTEX_CONTROL:
		if (copy_from_user(&value, (void *)arg, sizeof(int))) {
			COLOR_ERR
			 ("DISP_IOCTL_MUTEX_CONTROL Copy from user fail");
			return -EFAULT;
		}

		if (value == 1) {
			ncs_tuning_mode = 1;
			COLOR_DBG("ncs_tuning_mode = 1");
		} else if (value == 2) {

			ncs_tuning_mode = 0;
			COLOR_DBG("ncs_tuning_mode = 0");
			color_trigger_refresh(module);
		} else {
			COLOR_ERR("DISP_IOCTL_MUTEX_CONTROL invalid control");
			return -EFAULT;
		}

		COLOR_DBG("DISP_IOCTL_MUTEX_CONTROL done: %d", value);

		break;

	case DISP_IOCTL_READ_REG:
	{
		struct DISP_READ_REG rParams;
		unsigned long va;
		unsigned int pa;

		if (copy_from_user(&rParams, (void *)arg,
			sizeof(struct DISP_READ_REG))) {
			COLOR_ERR
			 ("DISP_IOCTL_READ_REG, copy_from_user fail");
			return -EFAULT;
		}

		pa = (unsigned int)rParams.reg;
#if defined(CONFIG_MACH_MT6799)
		/*handle capture coordinate of dual pipe tool tuning*/
		COLOR_ERR
		 ("DISP_IOCTL_READ_REG, g_color_pos_x[%x], g_color0_dst_w[%lx]"
		 , g_color_pos_x, g_color0_dst_w);

		if (primary_display_get_pipe_status() == DUAL_PIPE &&
			g_color_pos_x > g_color0_dst_w &&
			(pa & 0xFFFFF000) == ddp_reg_pa_base[DISP_REG_COLOR0])
			pa =
			(pa & 0x00000FFF) | ddp_reg_pa_base[DISP_REG_COLOR1];
#endif
		va = color_pa2va(pa);

		if (color_is_reg_addr_valid(va) == 0) {
			COLOR_ERR("reg read, addr invalid, pa:0x%x(va:0x%lx)",
				pa, va);
			return -EFAULT;
		}

		rParams.val = (DISP_REG_GET(va)) & rParams.mask;

		COLOR_NLOG("read pa:0x%x(va:0x%lx) = 0x%x (0x%x)", pa, va,
			rParams.val, rParams.mask);

		if (copy_to_user((void *)arg, &rParams,
			sizeof(struct DISP_READ_REG))) {
			COLOR_ERR("DISP_IOCTL_READ_REG, copy_to_user fail");
			return -EFAULT;
		}
		break;
	}

	case DISP_IOCTL_WRITE_REG:
	{
		struct DISP_WRITE_REG wParams;
		unsigned int ret;
		unsigned long va;
		unsigned int pa;

		if (copy_from_user(&wParams, (void *)arg,
			sizeof(struct DISP_WRITE_REG))) {
			COLOR_ERR
			 ("DISP_IOCTL_WRITE_REG, copy_from_user fail");
			return -EFAULT;
		}

		pa = (unsigned int)wParams.reg;
#if defined(CONFIG_MACH_MT6799)
	{
		bool isCaptureCmd = (pa == (DISP_COLOR_POS_MAIN_OFFSET |
		 ddp_reg_pa_base[DISP_REG_COLOR0]) || pa ==
		 (DISP_COLOR_POS_MAIN_OFFSET |
		 ddp_reg_pa_base[DISP_REG_COLOR1])) ? true : false;

		/*keep capture x-coordinate for dual pipe tool tuning*/
		if (isCaptureCmd) {
			g_color_pos_x =
				(wParams.val & DISP_COLOR_POS_MAIN_POS_X_MASK);
			COLOR_ERR
			 ("DISP_IOCTL_WRITE_REG, g_color_pos_x=%x",
			 g_color_pos_x);
			COLOR_ERR
			 ("DISP_IOCTL_WRITE_REG, wParams.val=%x",
			 g_color_pos_x);

		}

		if (module == DISP_MODULE_COLOR1) {
			/*handle capture coordinate of dual pipe tool tuning*/
			if (primary_display_get_pipe_status() == DUAL_PIPE &&
				isCaptureCmd) {
				/* it's pipe0 x_pos */
				/* force pipe1 to write a over-bound x_pos */
				if (g_color_pos_x <= g_color0_dst_w)
					wParams.val |= 0xFFFF;
				else
					wParams.val = ((wParams.val &
					DISP_COLOR_POS_MAIN_POS_Y_MASK) |
					(g_color_pos_x - g_color0_dst_w));
			}

			if ((pa & 0xFFFFF000) ==
				ddp_reg_pa_base[DISP_REG_COLOR0])
				pa = (pa & 0x00000FFF) |
					ddp_reg_pa_base[DISP_REG_COLOR1];
			else if ((pa & 0xFFFFF000) ==
				ddp_reg_pa_base[DISP_REG_CCORR0])
				pa = (pa & 0x00000FFF) |
					ddp_reg_pa_base[DISP_REG_CCORR1];
			else if ((pa & 0xFFFFF000) ==
				ddp_reg_pa_base[DISP_REG_AAL0])
				pa = (pa & 0x00000FFF) |
					ddp_reg_pa_base[DISP_REG_AAL1];
			else if ((pa & 0xFFFFF000) ==
				ddp_reg_pa_base[DISP_REG_GAMMA0])
				pa = (pa & 0x00000FFF) |
					ddp_reg_pa_base[DISP_REG_GAMMA1];
			else {
			 COLOR_DBG
			 ("DISP_IOCTL_WRITE_REG,not disp dual pipe PQ module");
				break;
			}
		}
	}
#endif
		va = color_pa2va(pa);

		ret = color_is_reg_addr_valid(va);
		if (ret == 0) {
			COLOR_ERR("reg write, addr invalid, pa:0x%x(va:0x%lx)",
				pa, va);
			return -EFAULT;
		}

		/* if TDSHP, write PA directly */
		if (ret == 2) {
			if (cmdq == NULL) {
				mt_reg_sync_writel((unsigned int)(INREG32(va) &
					~(wParams.mask)) | (wParams.val),
					(unsigned long *)(va));
			} else {
				cmdqRecWrite(cmdq, pa,
					wParams.val, wParams.mask);
			/*cmdqRecWrite(cmdq,TDSHP_PA_BASE+(wParams.reg - */
			/*g_tdshp_va),wParams.val, wParams.mask);*/
			}
		} else {
			_color_reg_mask(cmdq, va, wParams.val, wParams.mask);
		}

		COLOR_NLOG("write pa:0x%x(va:0x%lx) = 0x%x (0x%x)", pa, va,
			wParams.val, wParams.mask);

		break;

}

	case DISP_IOCTL_READ_SW_REG:
	{
		struct DISP_READ_REG rParams;

		if (copy_from_user(&rParams, (void *)arg,
			sizeof(struct DISP_READ_REG))) {
			COLOR_ERR
			 ("DISP_IOCTL_READ_SW_REG, copy_from_user fail");
			return -EFAULT;
		}
		if (rParams.reg > DISP_COLOR_SWREG_END
			|| rParams.reg < DISP_COLOR_SWREG_START) {
			COLOR_ERR
			("sw reg read, invalid, min=0x%x, max=0x%x, addr=0x%x",
			DISP_COLOR_SWREG_START, DISP_COLOR_SWREG_END,
			rParams.reg);
			return -EFAULT;
		}

		rParams.val = color_read_sw_reg(rParams.reg);

		COLOR_NLOG("read sw reg 0x%x = 0x%x", rParams.reg,
			rParams.val);

		if (copy_to_user((void *)arg, &rParams,
			sizeof(struct DISP_READ_REG))) {
			COLOR_ERR
			 ("DISP_IOCTL_READ_SW_REG, copy_to_user fail");
			return -EFAULT;
		}
		break;
	}

	case DISP_IOCTL_WRITE_SW_REG:
	{
		struct DISP_WRITE_REG wParams;

		if (copy_from_user(&wParams, (void *)arg,
			sizeof(struct DISP_WRITE_REG))) {
			COLOR_ERR
			 ("DISP_IOCTL_WRITE_SW_REG, copy_from_user fail");
			return -EFAULT;
		}


		if (wParams.reg > DISP_COLOR_SWREG_END
			|| wParams.reg < DISP_COLOR_SWREG_START) {
			COLOR_ERR
			 ("sw reg write, invalid,min=0x%x,max=0x%x, addr=0x%x",
			 DISP_COLOR_SWREG_START, DISP_COLOR_SWREG_END,
			 wParams.reg);
			return -EFAULT;
		}

		color_write_sw_reg(wParams.reg, wParams.val);

		COLOR_NLOG("write sw reg  0x%x = 0x%x", wParams.reg,
			wParams.val);

		break;

	}

	case DISP_IOCTL_PQ_SET_BYPASS_COLOR:
		if (copy_from_user(&value, (void *)arg, sizeof(int))) {
			COLOR_ERR
			("DISP_IOCTL_PQ_SET_BYPASS_COLOR Copy from user fail");
			return -EFAULT;
		}

		ddp_color_bypass_color(module, value, cmdq);
		color_trigger_refresh(module);

		break;

	case DISP_IOCTL_PQ_SET_WINDOW:
	{
		struct DISP_PQ_WIN_PARAM win_param;

		if (copy_from_user(&win_param, (void *)arg,
			sizeof(struct DISP_PQ_WIN_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_PQ_SET_WINDOW Copy from user fail");
			return -EFAULT;
		}

		COLOR_DBG
		 ("DISP_IOCTL_PQ_SET_WINDOW, module=%d, en=%d, x=0x%x, y=0x%x",
		 module, g_split_en, ((g_split_window_x_end << 16) |
		 g_split_window_x_start), ((g_split_window_y_end << 16) |
		 g_split_window_y_start));

		ddp_color_set_window(module, &win_param, cmdq);
		color_trigger_refresh(module);

		break;
	}

	case DISP_IOCTL_PQ_GET_TDSHP_FLAG:
		if (copy_to_user((void *)arg, &g_tdshp_flag, sizeof(int))) {
			COLOR_ERR
			 ("DISP_IOCTL_PQ_GET_TDSHP_FLAG Copy to user fail");
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_PQ_SET_TDSHP_FLAG:
		if (copy_from_user(&g_tdshp_flag, (void *)arg, sizeof(int))) {
			COLOR_ERR
			 ("DISP_IOCTL_PQ_SET_TDSHP_FLAG Copy from user fail");
			return -EFAULT;
		}
		break;


	case DISP_IOCTL_PQ_GET_DC_PARAM:
		if (copy_to_user((void *)arg, &g_PQ_DC_Param,
			sizeof(struct DISP_PQ_DC_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_PQ_GET_DC_PARAM Copy to user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_PQ_SET_DC_PARAM:
		if (copy_from_user(&g_PQ_DC_Param, (void *)arg,
			sizeof(struct DISP_PQ_DC_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_PQ_SET_DC_PARAM Copy from user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_PQ_GET_DS_PARAM:
		if (copy_to_user((void *)arg, &g_PQ_DS_Param,
			sizeof(struct DISP_PQ_DS_PARAM))) {
			COLOR_ERR
			 ("DISP_IOCTL_PQ_GET_DS_PARAM Copy to user fail");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_PQ_GET_MDP_TDSHP_REG:
		if (copy_to_user((void *)arg, &g_tdshp_reg,
			sizeof(struct MDP_TDSHP_REG))) {
			COLOR_ERR
			 ("DISP_IOCTL_PQ_GET_MDP_TDSHP_REG Copy to user fail");
			return -EFAULT;
		}

	break;

#if defined(DISP_MDP_COLOR_ON) || defined(MDP_COLOR_ON)
	case DISP_IOCTL_PQ_GET_MDP_COLOR_CAP:
		if (copy_to_user((void *)arg, &mdp_color_cap,
			sizeof(struct MDP_COLOR_CAP))) {
			COLOR_ERR
			 ("DISP_IOCTL_PQ_GET_MDP_COLOR_CAP Copy to user fail");
			return -EFAULT;
		}

	break;
#endif

	default:
	{
		COLOR_DBG("ioctl not supported failed\n");
		return -EFAULT;
	}
	}

	return ret;
}

#ifdef CONFIG_FOR_SOURCE_PQ
void set_color_bypass(enum DISP_MODULE_ENUM module, int bypass,
		void *cmdq_handle)
{
	int offset = C0_OFFSET;

#ifdef DISP_COLOR_OFF
	COLOR_NLOG("DISP_COLOR_OFF, Color bypassed...\n");
	return;
#endif

	g_color_bypass[index_of_color(module)] = bypass;

	offset = color_get_offset(module);

	/* DISP_REG_SET(NULL, DISP_COLOR_INTERNAL_IP_WIDTH + offset, */
	/* srcWidth); //wrapper width */
	/* DISP_REG_SET(NULL, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, */
	/* srcHeight); //wrapper height */

	if (bypass) {
		/* disable R2Y/Y2R in Color Wrapper */
		_color_reg_mask(cmdq_handle, DISP_COLOR_CM1_EN + offset, 0,
			0x1);
		_color_reg_mask(cmdq_handle, DISP_COLOR_CM2_EN + offset, 0,
			0x1);

		_color_reg_mask(cmdq_handle, DISP_COLOR_CFG_MAIN + offset,
			 (1 << 7), 0x000000FF);	/* bypass all */
		_color_reg_mask(cmdq_handle, DISP_COLOR_START + offset,
			0x00000003, 0x3);	/* color start */
	} else {
		_color_reg_mask(cmdq_handle, DISP_COLOR_CFG_MAIN + offset,
			 (0 << 7), 0x000000FF);	/* bypass all */
		_color_reg_mask(cmdq_handle, DISP_COLOR_START + offset,
			 0x00000001, 0x3);	/* color start */

		/* enable R2Y/Y2R in Color Wrapper */
		if (g_config_color21 == true) {
			/* RDMA & OVL will enable wide-gamut function*/
			/* disable rgb clipping function in CM1 */
			/* to keep the wide-gamut range */
			_color_reg_mask(cmdq_handle, DISP_COLOR_CM1_EN +
				  offset, 0x01, 0x03);
		} else {
			_color_reg_mask(cmdq_handle, DISP_COLOR_CM1_EN +
				  offset, 0x01, 0x01);
		}
		/* also set no rounding on Y2R */
		_color_reg_mask(cmdq_handle, DISP_COLOR_CM2_EN + offset, 0x11,
			0x11);
	}

}
#endif

static int _color_bypass(enum DISP_MODULE_ENUM module, int bypass)
{
	int offset = C0_OFFSET;

#ifdef DISP_COLOR_OFF
	COLOR_NLOG("DISP_COLOR_OFF, Color bypassed...\n");
	UNUSED(module);
	UNUSED(bypass);
	UNUSED(offset);
	return -1;
#else

	g_color_bypass[index_of_color(module)] = bypass;

	offset = color_get_offset(module);

	/* _color_reg_set(NULL, DISP_COLOR_INTERNAL_IP_WIDTH + offset, */
		/* srcWidth); //wrapper width */
	/* wrapper height */
	/* _color_reg_set(NULL, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, */
		/* srcHeight); //wrapper height */


	if (bypass) {
		/* disable R2Y/Y2R in Color Wrapper */
		_color_reg_mask(NULL, DISP_COLOR_CM1_EN + offset, 0, 0x1);
		_color_reg_mask(NULL, DISP_COLOR_CM2_EN + offset, 0, 0x1);

		_color_reg_mask(NULL, DISP_COLOR_CFG_MAIN + offset, (1 << 7),
			0x000000FF);	/* bypass all */
		_color_reg_mask(NULL, DISP_COLOR_START + offset, 0x00000003,
			0x3);	/* color start */
	} else {
		_color_reg_mask(NULL, DISP_COLOR_CFG_MAIN + offset, (0 << 7),
			0x000000FF);	/* resume all */
		_color_reg_mask(NULL, DISP_COLOR_START + offset, 0x00000001,
			0x3);	/* color start */

		/* enable R2Y/Y2R in Color Wrapper */
		if (g_config_color21 == true) {
			/* RDMA & OVL will enable wide-gamut function*/
			/* disable rgb clipping function in CM1 */
			/* to keep the wide-gamut range */
			_color_reg_mask(NULL, DISP_COLOR_CM1_EN + offset, 0x01,
				0x03);
		} else {
			_color_reg_mask(NULL, DISP_COLOR_CM1_EN + offset, 0x01,
				0x01);
		}

#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795)
		/* also set no rounding on Y2R */
		_color_reg_mask(NULL, DISP_COLOR_CM2_EN + offset, 0x01, 0x11);
#else
		/* also set no rounding on Y2R */
		_color_reg_mask(NULL, DISP_COLOR_CM2_EN + offset, 0x11, 0x11);
#endif
	}

	return 0;
#endif
}

static int _color_build_cmdq(enum DISP_MODULE_ENUM module,
		void *cmdq_trigger_handle, enum CMDQ_STATE state)
{
	int ret = 0;

	if (cmdq_trigger_handle == NULL) {
		COLOR_ERR("cmdq_trigger_handle is NULL\n");
		return -1;
	}

	/* only get COLOR HIST on primary display */
	if ((module == DISP_MODULE_COLOR0) &&
		(state == CMDQ_AFTER_STREAM_EOF)) {
#if defined(CONFIG_MACH_MT6595) || defined(CONFIG_MACH_MT6795) || \
	defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799)
		ret = cmdqRecReadToDataRegister(cmdq_trigger_handle,
			ddp_reg_pa_base[DISP_REG_COLOR0] +
			(DISP_COLOR_TWO_D_W1_RESULT - DISPSYS_COLOR0_BASE),
				CMDQ_DATA_REG_PQ_COLOR);
#elif defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) ||  \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
		ret = cmdqRecReadToDataRegister(cmdq_trigger_handle,
			ddp_get_module_pa(DISP_MODULE_COLOR0) +
			(DISP_COLOR_TWO_D_W1_RESULT - DISPSYS_COLOR0_BASE),
			CMDQ_DATA_REG_PQ_COLOR);

#else
		ret = cmdqRecReadToDataRegister(cmdq_trigger_handle,
			ddp_reg_pa_base[DISP_REG_COLOR] +
			(DISP_COLOR_TWO_D_W1_RESULT - DISPSYS_COLOR0_BASE),
			CMDQ_DATA_REG_PQ_COLOR);
#endif
	}

	return ret;
}

void disp_color_dbg_log_level(unsigned int debug_level)
{
	g_color_dbg_en = debug_level;
}

struct DDP_MODULE_DRIVER ddp_driver_color = {
	.init = _color_init,
	.deinit = _color_deinit,
	.config = _color_config,
	.start = _color_start,
	.trigger = NULL,
	.stop = NULL,
	.reset = NULL,
	.power_on = _color_clock_on,
	.power_off = _color_clock_off,
	.is_idle = NULL,
	.is_busy = NULL,
	.dump_info = NULL,
	.bypass = _color_bypass,
	.build_cmdq = _color_build_cmdq,
	.set_lcm_utils = NULL,
	.set_listener = _color_set_listener,
	.cmd = _color_io,
#if defined(COLOR_SUPPORT_PARTIAL_UPDATE)
	.ioctl = color_ioctl,
#endif
};
