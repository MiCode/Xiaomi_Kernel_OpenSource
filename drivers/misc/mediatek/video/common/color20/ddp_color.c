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
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#include <disp_helper.h>
#endif

#if defined(CONFIG_MTK_CLKMGR) || defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
#include <mach/mt_clkmgr.h>
#elif defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#include "ddp_clkmgr.h"
#endif

#include "ddp_reg.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_color.h"
#include "cmdq_def.h"

#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#define COLOR_SUPPORT_PARTIAL_UPDATE
#endif


/* global PQ param for kernel space */
static DISP_PQ_PARAM g_Color_Param[2] = {
	{
u4SHPGain:2,
u4SatGain:4,
u4PartialY:0,
u4HueAdj:{9, 9, 9, 9},
u4SatAdj:{0, 0, 0, 0},
u4Contrast:4,
u4Brightness:4,
u4Ccorr:0
	 },
	{
u4SHPGain:2,
u4SatGain:4,
u4PartialY:0,
u4HueAdj:{9, 9, 9, 9},
u4SatAdj:{0, 0, 0, 0},
u4Contrast:4,
u4Brightness:4,
u4Ccorr:1
	}
};

static DISP_PQ_PARAM g_Color_Cam_Param = {
u4SHPGain:0,
u4SatGain:4,
u4PartialY:0,
u4HueAdj:{9, 9, 9, 9},
u4SatAdj:{0, 0, 0, 0},
u4Contrast:4,
u4Brightness:4,
u4Ccorr:2
};

static DISP_PQ_PARAM g_Color_Gal_Param = {
u4SHPGain:2,
u4SatGain:4,
u4PartialY:0,
u4HueAdj:{9, 9, 9, 9},
u4SatAdj:{0, 0, 0, 0},
u4Contrast:4,
u4Brightness:4,
u4Ccorr:3
};

static DISP_PQ_DC_PARAM g_PQ_DC_Param = {
param:
	{
	 1, 1, 0, 0, 0, 0, 0, 0, 0, 0x0A,
	 0x30, 0x40, 0x06, 0x12, 40, 0x40, 0x80, 0x40, 0x40, 1,
	 0x80, 0x60, 0x80, 0x10, 0x34, 0x40, 0x40, 1, 0x80, 0xa,
	 0x19, 0x00, 0x20, 0, 0, 1, 2, 1, 80, 1}
};

static DISP_PQ_DS_PARAM g_PQ_DS_Param = {
param:
	{
	 1, -4, 1024, -4, 1024,
	 1, 400, 200, 1600, 800,
	 128, 8, 4, 12, 16,
	 8, 24, -8, -4, -12,
	 0, 0, 0}
};

static MDP_TDSHP_REG g_tdshp_reg = {
	TDS_GAIN_MID:0x10,
	TDS_GAIN_HIGH:0x20,
	TDS_COR_GAIN:0x10,
	TDS_COR_THR:0x4,
	TDS_COR_ZERO:0x2,
	TDS_GAIN:0x20,
	TDS_COR_VALUE:0x3
};

/* initialize index (because system default is 0, need fill with 0x80) */

static DISPLAY_PQ_T g_Color_Index = {
GLOBAL_SAT:
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},	/* 0~9 */

CONTRAST :
	{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},	/* 0~9 */

BRIGHTNESS :
	{0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400},	/* 0~9 */

PARTIAL_Y :
	{
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
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
	{
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
	 {0x0, 0x0, 0x400},
	 },
	},
#if defined(CONFIG_ARCH_MT6797)
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

LSP_EN:0
#endif
};

static DEFINE_MUTEX(g_color_reg_lock);
static DISPLAY_COLOR_REG_T g_color_reg;
static int g_color_reg_valid;

int color_dbg_en = 1;
#define COLOR_ERR(fmt, arg...) pr_err("[COLOR] " fmt "\n", ##arg)
#define COLOR_DBG(fmt, arg...) \
	do { if (color_dbg_en) pr_debug("[COLOR] " fmt "\n", ##arg); } while (0)
#define COLOR_NLOG(fmt, arg...) pr_debug("[COLOR] " fmt "\n", ##arg)

static ddp_module_notify g_color_cb;

static DISPLAY_TDSHP_T g_TDSHP_Index;

static unsigned int g_split_en;
static unsigned int g_split_window_x = 0xFFFF0000;
static unsigned int g_split_window_y = 0xFFFF0000;

#if defined(CONFIG_ARCH_MT6797)
	static unsigned long g_color_window = 0x40185E57;
#else
	static unsigned long g_color_window = 0x40106051;
#endif

static unsigned long g_color0_dst_w;
static unsigned long g_color0_dst_h;
static unsigned long g_color1_dst_w;
static unsigned long g_color1_dst_h;

static MDP_COLOR_CAP mdp_color_cap;

#if defined(CONFIG_FPGA_EARLY_PORTING) || defined(DISP_COLOR_OFF)
static int g_color_bypass = 1;
#else
static int g_color_bypass;
#endif
static int g_tdshp_flag;	/* 0: normal, 1: tuning mode */
int ncs_tuning_mode = 0;
int tdshp_index_init = 0;

#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
#define TDSHP_PA_BASE   0x14009000
#define TDSHP1_PA_BASE  0x1400A000
static unsigned long g_tdshp1_va;
#elif defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#define TDSHP_PA_BASE   0x14009000
#else
#define TDSHP_PA_BASE   0x14006000
#endif

#ifdef DISP_MDP_COLOR_ON
#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#define MDP_COLOR_PA_BASE 0x1400A000
#else
#define MDP_COLOR_PA_BASE 0x14007000
#endif
static unsigned long g_mdp_color_va;
#endif

#if defined(CONFIG_ARCH_MT6797)
#define MDP_RSZ0_PA_BASE 0x14003000
static unsigned long g_mdp_rsz0_va;
#define MDP_RSZ1_PA_BASE 0x14004000
static unsigned long g_mdp_rsz1_va;
#define MDP_RSZ2_PA_BASE 0x14005000
static unsigned long g_mdp_rsz2_va;
#endif

static unsigned long g_tdshp_va;

void disp_color_set_window(unsigned int sat_upper, unsigned int sat_lower,
			   unsigned int hue_upper, unsigned int hue_lower)
{
	g_color_window = (sat_upper << 24) | (sat_lower << 16) | (hue_upper << 8) | (hue_lower);
}


/*
*g_Color_Param
*/

DISP_PQ_PARAM *get_Color_config(int id)
{
	return &g_Color_Param[id];
}

DISP_PQ_PARAM *get_Color_Cam_config(void)
{
	return &g_Color_Cam_Param;
}

DISP_PQ_PARAM *get_Color_Gal_config(void)
{
	return &g_Color_Gal_Param;
}

/*
*g_Color_Index
*/

DISPLAY_PQ_T *get_Color_index(void)
{
	return &g_Color_Index;
}


DISPLAY_TDSHP_T *get_TDSHP_index(void)
{
	return &g_TDSHP_Index;
}

static void _color_reg_set(void *__cmdq, unsigned long addr, unsigned int value)
{
	cmdqRecHandle cmdq = (cmdqRecHandle) __cmdq;

	DISP_REG_SET(cmdq, addr, value);
}

static void _color_reg_mask(void *__cmdq, unsigned long addr, unsigned int value, unsigned int mask)
{
	cmdqRecHandle cmdq = (cmdqRecHandle) __cmdq;

	DISP_REG_MASK(cmdq, addr, value, mask);
}

static void _color_reg_set_field(void *__cmdq, unsigned int field_mask, unsigned long addr,
				 unsigned int value)
{
	cmdqRecHandle cmdq = (cmdqRecHandle) __cmdq;

	DISP_REG_SET_FIELD(cmdq, field_mask, addr, value);
}

void DpEngine_COLORonInit(DISP_MODULE_ENUM module, void *__cmdq)
{
	/* pr_debug("===================init COLOR =======================\n"); */
	int offset = C0_OFFSET;
	void *cmdq = __cmdq;

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

#ifndef CONFIG_FPGA_EARLY_PORTING
	COLOR_DBG("DpEngine_COLORonInit(), en[%d],  x[0x%x], y[0x%x]\n", g_split_en,
		g_split_window_x, g_split_window_y);
	_color_reg_mask(cmdq, DISP_COLOR_DBG_CFG_MAIN + offset, g_split_en << 3, 0x00000008);
	_color_reg_set(cmdq, DISP_COLOR_WIN_X_MAIN + offset, g_split_window_x);
	_color_reg_set(cmdq, DISP_COLOR_WIN_Y_MAIN + offset, g_split_window_y);
#endif

	/* enable interrupt */
	_color_reg_mask(cmdq, DISP_COLOR_INTEN + offset, 0x00000007, 0x00000007);

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* Set 10bit->8bit Rounding */
	_color_reg_mask(cmdq, DISP_COLOR_OUT_SEL + offset, 0x333, 0x00000333);
#endif

#if defined(CONFIG_MTK_AAL_SUPPORT)
	/* c-boost ??? */
	_color_reg_set(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0xFF402280);
	_color_reg_set(cmdq, DISP_COLOR_C_BOOST_MAIN_2 + offset, 0x00000000);
#endif

}

void DpEngine_COLORonConfig(DISP_MODULE_ENUM module, void *__cmdq)
{
	int index = 0;
	unsigned int u4Temp = 0;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int offset = C0_OFFSET;
	DISP_PQ_PARAM *pq_param_p = &g_Color_Param[COLOR_ID_0];
	void *cmdq = __cmdq;
#if defined(CONFIG_ARCH_MT6797)
	int i, j, reg_index;
#endif

	if (DISP_MODULE_COLOR1 == module) {
		offset = C1_OFFSET;
		pq_param_p = &g_Color_Param[COLOR_ID_1];
	}

	if (pq_param_p->u4SatGain >= COLOR_TUNING_INDEX ||
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

	if (g_color_bypass == 0) {
#if defined(CONFIG_ARCH_MT6797)
		_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 21)
						| (g_Color_Index.LSP_EN << 20)
						| (g_Color_Index.S_GAIN_BY_Y_EN << 15) | (0 << 8)
						| (0 << 7), 0x003081FF);
#else
		_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (0 << 8) | (0 << 7)
						, 0x000001FF);	/* disable wide_gamut */
#endif
		_color_reg_mask(cmdq, DISP_COLOR_START + offset, 0x1, 0x3);	/* color start */
		/* enable R2Y/Y2R in Color Wrapper */
#if defined(CONFIG_ARCH_MT6797)
		/* RDMA & OVL will enable wide-gamut function*/
		/* disable rgb clipping function in CM1 to keep the wide-gamut range */
		_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset, 0x01, 0x03);
#else
		_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset, 0x01, 0x01);
#endif

#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
		_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0x01, 0x11);
#else
		_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0x11, 0x11); /* also set no rounding on Y2R */
#endif
	} else {
		_color_reg_set_field(cmdq, CFG_MAIN_FLD_COLOR_DBUF_EN, DISP_COLOR_CFG_MAIN + offset,
					 0x1);
		_color_reg_set_field(cmdq, START_FLD_DISP_COLOR_START, DISP_COLOR_START + offset,
					 0x1);
	}

	/* for partial Y contour issue */
#if defined(CONFIG_ARCH_MT6797)
	_color_reg_mask(cmdq, DISP_COLOR_LUMA_ADJ + offset, 0x40, 0x0000007F);
#else
	_color_reg_mask(cmdq, DISP_COLOR_LUMA_ADJ + offset, 0x0, 0x0000007F);
#endif

	/* config parameter from customer color_index.h */
	_color_reg_mask(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_1 + offset,
				(g_Color_Index.BRIGHTNESS[pq_param_p->u4Brightness] << 16) | g_Color_Index.
				CONTRAST[pq_param_p->u4Contrast], 0x07FF01FF);
	_color_reg_mask(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_2 + offset,
				g_Color_Index.GLOBAL_SAT[pq_param_p->u4SatGain]
				, 0x000001FF);

	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		_color_reg_mask(cmdq, DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index + offset,
				(g_Color_Index.PARTIAL_Y[pq_param_p->u4PartialY][2 * index] | g_Color_Index.
				PARTIAL_Y[pq_param_p->u4PartialY][2 * index + 1] << 16)
				, 0x00FF00FF);
	}

#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6755)
	_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0x80 << 16, 0x00FF0000);
#endif

#if !defined(CONFIG_ARCH_MT6797) /* set cboost_en = 0 for projects before 6755 */
	_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0 << 13, 0x00002000);
#endif

#if defined(CONFIG_ARCH_MT6797)
	_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN_2 + offset, 0x40 << 24, 0xFF000000);
#endif

	/* Partial Saturation Function */

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_0 + offset,
		       (g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG1][0] | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG1][1] << 8 | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG1][2] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_1 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][1] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][2] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][3] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_2 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][5] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][6] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][7] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_3 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][1] | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][2] << 8 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->
				     u4SatAdj[GRASS_TONE]][SG1][3] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN1_4 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][5] | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG1][0] << 8 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG1][1] << 16 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG1][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_0 + offset,
		       (g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG2][0] | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG2][1] << 8 | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG2][2] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_1 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][1] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][2] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][3] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_2 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][5] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][6] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][7] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_3 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][1] | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][2] << 8 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->
				     u4SatAdj[GRASS_TONE]][SG2][3] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN2_4 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][5] | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG2][0] << 8 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG2][1] << 16 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG2][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_0 + offset,
		       (g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG3][0] | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG3][1] << 8 | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG3][2] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_1 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][1] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][2] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][3] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_2 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][5] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][6] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][7] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_3 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][1] | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][2] << 8 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->
				     u4SatAdj[GRASS_TONE]][SG3][3] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_GAIN3_4 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][5] | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG3][0] << 8 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG3][1] << 16 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG3][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_0 + offset,
		       (g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP1][0] | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP1][1] << 8 | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP1][2] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_1 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][1] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][2] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][3] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_2 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][5] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][6] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][7] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_3 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][1] | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][2] << 8 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->
				     u4SatAdj[GRASS_TONE]][SP1][3] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT1_4 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][5] | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP1][0] << 8 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP1][1] << 16 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP1][2] << 24));

	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_0 + offset,
		       (g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP2][0] | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP2][1] << 8 | g_Color_Index.
			PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP2][2] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_1 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][1] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][2] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][3] << 16 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_2 + offset,
		       (g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][5] | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][6] << 8 | g_Color_Index.
			SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][7] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][0] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_3 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][1] | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][2] << 8 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->
				     u4SatAdj[GRASS_TONE]][SP2][3] << 16 | g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][4] << 24));
	_color_reg_set(cmdq, DISP_COLOR_PART_SAT_POINT2_4 + offset,
		       (g_Color_Index.
			GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][5] | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP2][0] << 8 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP2][1] << 16 | g_Color_Index.
			SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP2][2] << 24));

	for (index = 0; index < 3; index++) {
		h_series[index + PURP_TONE_START] =
		    g_Color_Index.PURP_TONE_H[pq_param_p->u4HueAdj[PURP_TONE]][index];
	}

	for (index = 0; index < 8; index++) {
		h_series[index + SKIN_TONE_START] =
		    g_Color_Index.SKIN_TONE_H[pq_param_p->u4HueAdj[SKIN_TONE]][index];
	}

	for (index = 0; index < 6; index++) {
		h_series[index + GRASS_TONE_START] =
		    g_Color_Index.GRASS_TONE_H[pq_param_p->u4HueAdj[GRASS_TONE]][index];
	}

	for (index = 0; index < 3; index++) {
		h_series[index + SKY_TONE_START] =
		    g_Color_Index.SKY_TONE_H[pq_param_p->u4HueAdj[SKY_TONE]][index];
	}

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) +
		    (h_series[4 * index + 1] << 8) +
		    (h_series[4 * index + 2] << 16) + (h_series[4 * index + 3] << 24);
		_color_reg_set(cmdq, DISP_COLOR_LOCAL_HUE_CD_0 + offset + 4 * index, u4Temp);
	}

#if defined(CONFIG_ARCH_MT6797)
	/* S Gain By Y */
	u4Temp = 0;

	reg_index = 0;
	for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT; i++) {
		for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
			u4Temp = (g_Color_Index.S_GAIN_BY_Y[i][j]) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 1] << 8) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 2] << 16) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 3] << 24);

			_color_reg_set(cmdq, DISP_COLOR_S_GAIN_BY_Y0_0 + offset + reg_index, u4Temp);
			reg_index += 4;
		}
	}
	/* LSP */
	_color_reg_mask(cmdq, DISP_COLOR_LSP_1 + offset, (0x7F << 0) | (0x7F << 7) | (0x0 << 14) | (0x0 << 22)
				, 0x1FFFFFFF);
	_color_reg_mask(cmdq, DISP_COLOR_LSP_2 + offset, (0x7F << 0) | (0x7F << 8) | (0x0 << 16) | (0x7F << 23)
				, 0x3FFF7F7F);
#endif

	/* color window */

	_color_reg_set(cmdq, DISP_COLOR_TWO_D_WINDOW_1 + offset, g_color_window);
}

static void color_write_hw_reg(DISP_MODULE_ENUM module,
	const DISPLAY_COLOR_REG_T *color_reg, void *cmdq)
{
	int offset = C0_OFFSET;
	int index;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned int u4Temp = 0;
#if defined(CONFIG_ARCH_MT6797)
	int i, j, reg_index;
#endif

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

	if (g_color_bypass == 0) {
#if defined(CONFIG_ARCH_MT6797)
		_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 21)
						| (g_Color_Index.LSP_EN << 20)
						| (g_Color_Index.S_GAIN_BY_Y_EN << 15) | (0 << 8)
						| (0 << 7), 0x003081FF);
#else
		_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (0 << 8) | (0 << 7)
						, 0x000001FF);	/* enable wide_gamut */
#endif
		_color_reg_mask(cmdq, DISP_COLOR_START + offset, 0x1, 0x3);	/* color start */
		/* enable R2Y/Y2R in Color Wrapper */
#if defined(CONFIG_ARCH_MT6797)
		/* RDMA & OVL will enable wide-gamut function*/
		/* disable rgb clipping function in CM1 to keep the wide-gamut range */
		_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset, 0x01, 0x03);
#else
		_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset, 0x01, 0x01);
#endif

#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
		_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0x01, 0x11);
#else
		_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0x11, 0x11); /* also set no rounding on Y2R */
#endif

	} else {
		_color_reg_set_field(cmdq, CFG_MAIN_FLD_COLOR_DBUF_EN, DISP_COLOR_CFG_MAIN + offset,
					 0x1);
		_color_reg_set_field(cmdq, START_FLD_DISP_COLOR_START, DISP_COLOR_START + offset,
					 0x1);
	}

	/* for partial Y contour issue */
#if defined(CONFIG_ARCH_MT6797)
	_color_reg_mask(cmdq, DISP_COLOR_LUMA_ADJ + offset, 0x40, 0x0000007F);
#else
	_color_reg_mask(cmdq, DISP_COLOR_LUMA_ADJ + offset, 0x0, 0x0000007F);
#endif

	_color_reg_mask(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_1 + offset,
		(color_reg->BRIGHTNESS << 16) | color_reg->CONTRAST, 0x07FF01FF);
	_color_reg_mask(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_2 + offset,
		 color_reg->GLOBAL_SAT, 0x000001FF);


	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		_color_reg_mask(cmdq, DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index + offset,
			(color_reg->PARTIAL_Y[2 * index] |
			(color_reg->PARTIAL_Y[2 * index + 1] << 16)), 0x00FF00FF);
	}

#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6755)
	_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0x80 << 16, 0x00FF0000);
#endif

#if !defined(CONFIG_ARCH_MT6797) /* set cboost_en = 0 for projects before 6755 */
	_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0 << 13, 0x00002000);
#endif

#if defined(CONFIG_ARCH_MT6797)
	_color_reg_mask(cmdq, DISP_COLOR_C_BOOST_MAIN_2 + offset, 0x40 << 24, 0xFF000000);
#endif

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
		h_series[index + PURP_TONE_START] = color_reg->PURP_TONE_H[index];

	for (index = 0; index < 8; index++)
		h_series[index + SKIN_TONE_START] = color_reg->SKIN_TONE_H[index];

	for (index = 0; index < 6; index++)
		h_series[index + GRASS_TONE_START] = color_reg->GRASS_TONE_H[index];

	for (index = 0; index < 3; index++)
		h_series[index + SKY_TONE_START] = color_reg->SKY_TONE_H[index];

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) |
		    (h_series[4 * index + 1] << 8) |
		    (h_series[4 * index + 2] << 16) |
		    (h_series[4 * index + 3] << 24);
		_color_reg_set(cmdq, DISP_COLOR_LOCAL_HUE_CD_0 + offset + 4 * index, u4Temp);
	}

#if defined(CONFIG_ARCH_MT6797)
	/* S Gain By Y */
	u4Temp = 0;

	reg_index = 0;
	for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT; i++) {
		for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
			u4Temp = (g_Color_Index.S_GAIN_BY_Y[i][j]) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 1] << 8) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 2] << 16) +
				(g_Color_Index.S_GAIN_BY_Y[i][j + 3] << 24);

			_color_reg_set(cmdq, DISP_COLOR_S_GAIN_BY_Y0_0 + offset + reg_index, u4Temp);
			reg_index += 4;
		}
	}
	/* LSP */
	_color_reg_mask(cmdq, DISP_COLOR_LSP_1 + offset, (0x7F << 0) | (0x7F << 7) | (0x0 << 14) | (0x0 << 22)
					, 0x1FFFFFFF);
	_color_reg_mask(cmdq, DISP_COLOR_LSP_2 + offset, (0x7F << 0) | (0x7F << 8) | (0x50 << 16) | (0x7 << 23)
					, 0x3FFF7F7F);
#endif

	/* color window */
	_color_reg_set(cmdq, DISP_COLOR_TWO_D_WINDOW_1 + offset, g_color_window);
}

static void color_trigger_refresh(DISP_MODULE_ENUM module)
{
	if (g_color_cb != NULL)
		g_color_cb(module, DISP_PATH_EVENT_TRIGGER);
	else
		COLOR_ERR("ddp listener is NULL!!\n");
}

static void ddp_color_bypass_color(DISP_MODULE_ENUM module, int bypass, void *__cmdq)
{
	int offset = C0_OFFSET;
	void *cmdq = __cmdq;

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

	if (bypass)
		_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 7), 0x000000FF);	/* bypass all */
	else
		_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (0 << 7), 0x000000FF);	/* resume all */
}

static void ddp_color_set_window(DISP_PQ_WIN_PARAM *win_param, void *__cmdq)
{
	const int offset = C0_OFFSET;
	void *cmdq = __cmdq;

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		g_split_en = 1;
#ifdef LCM_PHYSICAL_ROTATION_180
		g_split_window_x = ((g_color0_dst_w - win_param->start_x) << 16) | (g_color0_dst_w - win_param->end_x);
		g_split_window_y = ((g_color0_dst_h - win_param->start_y) << 16) | (g_color0_dst_h - win_param->end_y);
		COLOR_DBG("ddp_color_set_window(), LCM_PHYSICAL_ROTATION_180\n");
#else
		g_split_window_x = (win_param->end_x << 16) | win_param->start_x;
		g_split_window_y = (win_param->end_y << 16) | win_param->start_y;
#endif
	} else {
		g_split_en = 0;
		g_split_window_x = 0xFFFF0000;
		g_split_window_y = 0xFFFF0000;
	}

	COLOR_DBG("ddp_color_set_window(), en[%d],  x[0x%x], y[0x%x]\n", g_split_en,
		  g_split_window_x, g_split_window_y);

	_color_reg_mask(cmdq, DISP_COLOR_DBG_CFG_MAIN + offset, (g_split_en << 3), 0x00000008);	/* split enable */
	_color_reg_set(cmdq, DISP_COLOR_WIN_X_MAIN + offset, g_split_window_x);
	_color_reg_set(cmdq, DISP_COLOR_WIN_Y_MAIN + offset, g_split_window_y);
}


static unsigned long color_get_TDSHP_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_tdshp0");
#else
	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_tdshp");
#endif
	VA = (unsigned long)of_iomap(node, 0);
	COLOR_DBG("TDSHP VA: 0x%lx\n", VA);

	return VA;
}

#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
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

#ifdef DISP_MDP_COLOR_ON
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

#if defined(CONFIG_ARCH_MT6797)
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

static unsigned int color_is_reg_addr_valid(unsigned long addr)
{
	unsigned int i = 0;

	if ((addr & 0x3) != 0) {
		COLOR_ERR("color_is_reg_addr_valid, addr is not 4-byte aligned!\n");
		return 0;
	}

	for (i = 0; i < DISP_REG_NUM; i++) {
		if ((addr >= dispsys_reg[i]) && (addr < (dispsys_reg[i] + 0x1000)) && (dispsys_reg[i] != 0)) {
			break;
		}
	}

	if (i < DISP_REG_NUM) {
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, ddp_get_reg_module_name(i));
		return 1;
	}

	/*Check if MDP color base address*/
#ifdef DISP_MDP_COLOR_ON
	if ((addr >= g_mdp_color_va) && (addr < (g_mdp_color_va + 0x1000))) {			/* MDP COLOR */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, "MDP_COLOR");
		return 2;
	}
#endif


	/*Check if MDP RSZ base address*/
#if defined(CONFIG_ARCH_MT6797)
		if ((addr >= g_mdp_rsz0_va) && (addr < (g_mdp_rsz0_va + 0x1000))) {			/* MDP RSZ0 */
			COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, "MDP_RSZ0");
			return 2;
		} else if ((addr >= g_mdp_rsz1_va) && (addr < (g_mdp_rsz1_va + 0x1000))) {	/* MDP RSZ1 */
			COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, "MDP_RSZ1");
			return 2;
		} else if ((addr >= g_mdp_rsz2_va) && (addr < (g_mdp_rsz2_va + 0x1000))) {	/* MDP RSZ2 */
			COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, "MDP_RSZ2");
			return 2;
		}
#endif

	/* check if TDSHP base address */
	if ((addr >= g_tdshp_va) && (addr < (g_tdshp_va + 0x1000))) {			/* TDSHP0 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, "TDSHP0");
		return 2;
	}
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
	else if ((addr >= g_tdshp1_va) && (addr < (g_tdshp1_va + 0x1000))) {	/* TDSHP1 */
		COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, "TDSHP1");
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

	/* check disp module */
	for (i = 0; i < DISP_REG_NUM; i++) {
		if ((addr >= ddp_reg_pa_base[i]) && (addr < (ddp_reg_pa_base[i] + 0x1000))) {
			COLOR_DBG("color_pa2va(), COLOR PA:0x%x, PABase[0x%x], VABase[0x%lx]\n",
				  addr, (unsigned int)ddp_reg_pa_base[i], dispsys_reg[i]);
			return dispsys_reg[i] + (addr - ddp_reg_pa_base[i]);
		}
	}

	/* TDSHP */
	if ((TDSHP_PA_BASE <= addr) && (addr < (TDSHP_PA_BASE + 0x1000))) {
		COLOR_DBG("color_pa2va(), TDSHP PA:0x%x, PABase[0x%x], VABase[0x%lx]\n", addr,
			  TDSHP_PA_BASE, g_tdshp_va);
		return g_tdshp_va + (addr - TDSHP_PA_BASE);
	}
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
	/* TDSHP1 */
	if ((TDSHP1_PA_BASE <= addr) && (addr < (TDSHP1_PA_BASE + 0x1000))) {
		COLOR_DBG("color_pa2va(), TDSHP1 PA:0x%x, PABase[0x%x], VABase[0x%lx]\n", addr,
			  TDSHP1_PA_BASE, g_tdshp1_va);
		return g_tdshp1_va + (addr - TDSHP1_PA_BASE);
	}
#endif

#ifdef DISP_MDP_COLOR_ON
	/* MDP_COLOR */
	if ((MDP_COLOR_PA_BASE <= addr) && (addr < (MDP_COLOR_PA_BASE + 0x1000))) {
		COLOR_DBG("color_pa2va(), MDP_COLOR PA:0x%x, PABase[0x%x], VABase[0x%lx]\n", addr,
			  MDP_COLOR_PA_BASE, g_mdp_color_va);
		return g_mdp_color_va + (addr - MDP_COLOR_PA_BASE);
	}
#endif

#if defined(CONFIG_ARCH_MT6797)
		/* MDP_COLOR */
		if ((MDP_RSZ0_PA_BASE <= addr) && (addr < (MDP_RSZ0_PA_BASE + 0x1000))) {
			COLOR_DBG("color_pa2va(), MDP_RSZ0 PA:0x%x, PABase[0x%x], VABase[0x%lx]\n", addr,
				  MDP_RSZ0_PA_BASE, g_mdp_rsz0_va);
			return g_mdp_rsz0_va + (addr - MDP_RSZ0_PA_BASE);
		} else if ((MDP_RSZ1_PA_BASE <= addr) && (addr < (MDP_RSZ1_PA_BASE + 0x1000))) {
			COLOR_DBG("color_pa2va(), MDP_RSZ1 PA:0x%x, PABase[0x%x], VABase[0x%lx]\n", addr,
				  MDP_RSZ1_PA_BASE, g_mdp_rsz1_va);
			return g_mdp_rsz1_va + (addr - MDP_RSZ1_PA_BASE);
		} else if ((MDP_RSZ2_PA_BASE <= addr) && (addr < (MDP_RSZ2_PA_BASE + 0x1000))) {
			COLOR_DBG("color_pa2va(), MDP_RSZ2 PA:0x%x, PABase[0x%x], VABase[0x%lx]\n", addr,
				  MDP_RSZ2_PA_BASE, g_mdp_rsz2_va);
			return g_mdp_rsz2_va + (addr - MDP_RSZ2_PA_BASE);
		}
#endif

	COLOR_ERR("color_pa2va(), NO FOUND VA!! PA:0x%x, PABase[0x%x], VABase[0x%lx]\n", addr,
		  (unsigned int)ddp_reg_pa_base[0], dispsys_reg[0]);

	return 0;
}

static unsigned int color_read_sw_reg(unsigned int reg_id)
{
	unsigned int ret = 0;

	if (reg_id >= SWREG_PQDS_DS_EN && reg_id <= SWREG_PQDS_GAIN_0) {
		ret = (unsigned int)g_PQ_DS_Param.param[reg_id - SWREG_PQDS_DS_EN];
		return ret;
	}
	if (reg_id >= SWREG_PQDC_BLACK_EFFECT_ENABLE && reg_id <= SWREG_PQDC_DC_ENABLE) {
		ret = (unsigned int)g_PQ_DC_Param.param[reg_id - SWREG_PQDC_BLACK_EFFECT_ENABLE];
		return ret;
	}

	switch (reg_id) {
	case SWREG_COLOR_BASE_ADDRESS:
		{
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795) || defined(CONFIG_ARCH_ELBRUS) \
	|| defined(CONFIG_ARCH_MT6757)
			ret = ddp_reg_pa_base[DISP_REG_COLOR0];
#else
			ret = ddp_reg_pa_base[DISP_REG_COLOR];
#endif
			break;
		}

	case SWREG_GAMMA_BASE_ADDRESS:
		{
#if defined(CONFIG_ARCH_ELBRUS) || defined(CONFIG_ARCH_MT6757)
			ret = ddp_reg_pa_base[DISP_REG_GAMMA0];
#else
			ret = ddp_reg_pa_base[DISP_REG_GAMMA];
#endif
			break;
		}

	case SWREG_AAL_BASE_ADDRESS:
		{
#if defined(CONFIG_ARCH_ELBRUS) || defined(CONFIG_ARCH_MT6757)
			ret = ddp_reg_pa_base[DISP_REG_AAL0];
#else
			ret = ddp_reg_pa_base[DISP_REG_AAL];
#endif
			break;
		}

#if defined(CCORR_SUPPORT)
	case SWREG_CCORR_BASE_ADDRESS:
		{
#if defined(CONFIG_ARCH_ELBRUS) || defined(CONFIG_ARCH_MT6757)
			ret = ddp_reg_pa_base[DISP_REG_CCORR0];
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
#ifdef DISP_MDP_COLOR_ON
	case SWREG_MDP_COLOR_BASE_ADDRESS:
		{
			ret = MDP_COLOR_PA_BASE;
			break;
		}
#endif
	case SWREG_COLOR_MODE:
		{
			ret = COLOR_MODE;
			break;
		}

#if defined(CONFIG_ARCH_MT6797)
	case SWREG_RSZ_BASE_ADDRESS:
		{
			ret = MDP_RSZ0_PA_BASE;
			break;
		}
#endif

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
	if (reg_id >= SWREG_PQDC_BLACK_EFFECT_ENABLE && reg_id <= SWREG_PQDC_DC_ENABLE) {
		g_PQ_DC_Param.param[reg_id - SWREG_PQDC_BLACK_EFFECT_ENABLE] = (int)value;
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


static int _color_clock_on(DISP_MODULE_ENUM module, void *cmq_handle)
{
#if defined(CONFIG_ARCH_MT6755)
	/* color is DCM , do nothing */
	return 0;
#endif
#ifdef ENABLE_CLK_MGR
#ifdef CONFIG_MTK_CLKMGR
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
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
	COLOR_DBG("color[0]_clock_on CG 0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#endif
#else
	ddp_clk_enable(DISP0_DISP_COLOR);
	COLOR_DBG("color[0]_clock_on CG 0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#endif
#endif

	return 0;
}

static int _color_clock_off(DISP_MODULE_ENUM module, void *cmq_handle)
{
#if defined(CONFIG_ARCH_MT6755)
	/* color is DCM , do nothing */
	return 0;
#endif
#ifdef ENABLE_CLK_MGR
#ifdef CONFIG_MTK_CLKMGR
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
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
	ddp_clk_disable(DISP0_DISP_COLOR);
#endif
#endif
	return 0;
}

static int _color_init(DISP_MODULE_ENUM module, void *cmq_handle)
{
	_color_clock_on(module, cmq_handle);

	g_tdshp_va = color_get_TDSHP_VA();
#ifdef DISP_MDP_COLOR_ON
	g_mdp_color_va = color_get_MDP_COLOR_VA();
#endif
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
	g_tdshp1_va = color_get_TDSHP1_VA();
#endif
#if defined(CONFIG_ARCH_MT6797)
	g_mdp_rsz0_va = color_get_MDP_RSZ0_VA();
	g_mdp_rsz1_va = color_get_MDP_RSZ1_VA();
	g_mdp_rsz2_va = color_get_MDP_RSZ2_VA();
#endif

	return 0;
}

static int _color_deinit(DISP_MODULE_ENUM module, void *cmq_handle)
{
	_color_clock_off(module, cmq_handle);
	return 0;
}

static int _color_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmq_handle)
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
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
#ifndef CONFIG_FOR_SOURCE_PQ
		offset = C1_OFFSET;
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset, pConfig->dst_w
				, 0x00003FFF);  /* wrapper width */
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, pConfig->dst_h
				, 0x00003FFF);  /* wrapper height */
		return 0;
#endif
#endif
	}
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset, pConfig->dst_w
				, 0x00003FFF);  /* wrapper width */
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, pConfig->dst_h
				, 0x00003FFF);  /* wrapper height */

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

static int _color_start(DISP_MODULE_ENUM module, void *cmdq)
{
	if (module == DISP_MODULE_COLOR1) {
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
#ifndef CONFIG_FOR_SOURCE_PQ
		/* set bypass to COLOR1 */
		{
			const int offset = C1_OFFSET;
			/* disable R2Y/Y2R in Color Wrapper */
			_color_reg_mask(cmdq, DISP_COLOR_CM1_EN + offset, 0, 0x1);
			_color_reg_mask(cmdq, DISP_COLOR_CM2_EN + offset, 0, 0x1);

			_color_reg_mask(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 7), 0xFF);  /* all bypass */
			_color_reg_mask(cmdq, DISP_COLOR_START + offset, 0x00000003, 0x3);	/* color start */
		}
		return 0;
#endif
#endif
	}
	DpEngine_COLORonInit(module, cmdq);

	mutex_lock(&g_color_reg_lock);
	if (g_color_reg_valid) {
		color_write_hw_reg(DISP_MODULE_COLOR0, &g_color_reg, cmdq);
		mutex_unlock(&g_color_reg_lock);
	} else {
		mutex_unlock(&g_color_reg_lock);
		DpEngine_COLORonConfig(module, cmdq);
	}

	return 0;
}

static int _color_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
	g_color_cb = notify;
	return 0;
}

#if defined(COLOR_SUPPORT_PARTIAL_UPDATE)
static int _color_partial_update(DISP_MODULE_ENUM module, void *arg, void *cmdq)
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
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
#ifndef CONFIG_FOR_SOURCE_PQ
		offset = C1_OFFSET;
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset, width
			, 0x00003FFF);  /* wrapper width */
		_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, height
			, 0x00003FFF);  /* wrapper height */

		return 0;
#endif
#endif
	}

	_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset, width
		, 0x00003FFF);  /* wrapper width */
	_color_reg_mask(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, height
		, 0x00003FFF);  /* wrapper height */

	return 0;
}

static int color_ioctl(DISP_MODULE_ENUM module, void *handle,
		DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_color_partial_update(module, params, handle);
		ret = 0;
	}

	return ret;
}
#endif

static int _color_io(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *cmdq)
{
	int ret = 0;
	int value = 0;
	DISP_PQ_PARAM *pq_param;
	DISPLAY_PQ_T *pq_index;
	DISPLAY_TDSHP_T *tdshp_index;

	COLOR_DBG("_color_io: msg %x", msg);
	/* COLOR_ERR("_color_io: GET_PQPARAM %lx\n", DISP_IOCTL_GET_PQPARAM); */
	/* COLOR_ERR("_color_io: SET_PQPARAM %lx\n", DISP_IOCTL_SET_PQPARAM); */
	/* COLOR_ERR("_color_io: READ_REG %lx\n", DISP_IOCTL_READ_REG); */
	/* COLOR_ERR("_color_io: WRITE_REG %lx\n", DISP_IOCTL_WRITE_REG); */

	switch (msg) {
	case DISP_IOCTL_SET_PQPARAM:
		/* case DISP_IOCTL_SET_C0_PQPARAM: */

		pq_param = get_Color_config(COLOR_ID_0);
		if (copy_from_user(pq_param, (void *)arg, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_SET_PQPARAM Copy from user failed\n");

			return -EFAULT;
		}

		if (ncs_tuning_mode == 0) {
			/* normal mode */
			DpEngine_COLORonInit(DISP_MODULE_COLOR0, cmdq);
			DpEngine_COLORonConfig(DISP_MODULE_COLOR0, cmdq);

			color_trigger_refresh(DISP_MODULE_COLOR0);

			COLOR_DBG("SET_PQ_PARAM(0)\n");
		} else {
			/* ncs_tuning_mode = 0; */
			COLOR_DBG("SET_PQ_PARAM(0), bypassed by ncs_tuning_mode = 1\n");
		}

		break;

	case DISP_IOCTL_GET_PQPARAM:
		/* case DISP_IOCTL_GET_C0_PQPARAM: */

		pq_param = get_Color_config(COLOR_ID_0);
		if (copy_to_user((void *)arg, pq_param, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_GET_PQPARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_PQINDEX:
		COLOR_DBG("DISP_IOCTL_SET_PQINDEX!\n");

		pq_index = get_Color_index();
		if (copy_from_user(pq_index, (void *)arg, sizeof(DISPLAY_PQ_T))) {
			COLOR_ERR("DISP_IOCTL_SET_PQINDEX Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQINDEX:

		pq_index = get_Color_index();
		if (copy_to_user((void *)arg, pq_index, sizeof(DISPLAY_PQ_T))) {
			COLOR_ERR("DISP_IOCTL_GET_PQPARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_COLOR_REG:
		COLOR_DBG("DISP_IOCTL_SET_COLOR_REG\n");

		mutex_lock(&g_color_reg_lock);
		if (copy_from_user(&g_color_reg, (void *)arg, sizeof(DISPLAY_COLOR_REG_T))) {
			mutex_unlock(&g_color_reg_lock);
			COLOR_ERR("DISP_IOCTL_SET_COLOR_REG Copy from user failed\n");
			return -EFAULT;
		}

		color_write_hw_reg(DISP_MODULE_COLOR0, &g_color_reg, cmdq);
		g_color_reg_valid = 1;
		mutex_unlock(&g_color_reg_lock);

		color_trigger_refresh(DISP_MODULE_COLOR0);

		break;

	case DISP_IOCTL_SET_TDSHPINDEX:

		COLOR_DBG("DISP_IOCTL_SET_TDSHPINDEX!\n");

		tdshp_index = get_TDSHP_index();
		if (copy_from_user(tdshp_index, (void *)arg, sizeof(DISPLAY_TDSHP_T))) {
			COLOR_ERR("DISP_IOCTL_SET_TDSHPINDEX Copy from user failed\n");
			return -EFAULT;
		}
		tdshp_index_init = 1;
		break;

	case DISP_IOCTL_GET_TDSHPINDEX:

		if (tdshp_index_init == 0) {
			COLOR_ERR("DISP_IOCTL_GET_TDSHPINDEX TDSHPINDEX not init\n");
			return -EFAULT;
		}
		tdshp_index = get_TDSHP_index();
		if (copy_to_user((void *)arg, tdshp_index, sizeof(DISPLAY_TDSHP_T))) {
			COLOR_ERR("DISP_IOCTL_GET_TDSHPINDEX Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_PQ_CAM_PARAM:

		pq_param = get_Color_Cam_config();
		if (copy_from_user(pq_param, (void *)arg, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_SET_PQ_CAM_PARAM Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQ_CAM_PARAM:

		pq_param = get_Color_Cam_config();
		if (copy_to_user((void *)arg, pq_param, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_GET_PQ_CAM_PARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_PQ_GAL_PARAM:

		pq_param = get_Color_Gal_config();
		if (copy_from_user(pq_param, (void *)arg, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_SET_PQ_GAL_PARAM Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQ_GAL_PARAM:

		pq_param = get_Color_Gal_config();
		if (copy_to_user((void *)arg, pq_param, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_GET_PQ_GAL_PARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_MUTEX_CONTROL:
		if (copy_from_user(&value, (void *)arg, sizeof(int))) {
			COLOR_ERR("DISP_IOCTL_MUTEX_CONTROL Copy from user failed\n");
			return -EFAULT;
		}

		if (value == 1) {
			ncs_tuning_mode = 1;
			COLOR_DBG("ncs_tuning_mode = 1\n");
		} else if (value == 2) {

			ncs_tuning_mode = 0;
			COLOR_DBG("ncs_tuning_mode = 0\n");
			color_trigger_refresh(DISP_MODULE_COLOR0);
		} else {
			COLOR_ERR("DISP_IOCTL_MUTEX_CONTROL invalid control\n");
			return -EFAULT;
		}

		COLOR_DBG("DISP_IOCTL_MUTEX_CONTROL done: %d\n", value);

		break;

	case DISP_IOCTL_READ_REG:
		{
			DISP_READ_REG rParams;
			unsigned long va;
			unsigned int pa;

			if (copy_from_user(&rParams, (void *)arg, sizeof(DISP_READ_REG))) {
				COLOR_ERR("DISP_IOCTL_READ_REG, copy_from_user failed\n");
				return -EFAULT;
			}

			pa = (unsigned int)rParams.reg;
			va = color_pa2va(pa);

			if (0 == color_is_reg_addr_valid(va)) {
				COLOR_ERR("reg read, addr invalid, pa:0x%x(va:0x%lx)\n", pa, va);
				return -EFAULT;
			}

			rParams.val = (DISP_REG_GET(va)) & rParams.mask;

			COLOR_NLOG("read pa:0x%x(va:0x%lx) = 0x%x (0x%x)\n", pa, va, rParams.val,
				   rParams.mask);

			if (copy_to_user((void *)arg, &rParams, sizeof(DISP_READ_REG))) {
				COLOR_ERR("DISP_IOCTL_READ_REG, copy_to_user failed\n");
				return -EFAULT;
			}
			break;
		}

	case DISP_IOCTL_WRITE_REG:
		{
			DISP_WRITE_REG wParams;
			unsigned int ret;
			unsigned long va;
			unsigned int pa;

			if (copy_from_user(&wParams, (void *)arg, sizeof(DISP_WRITE_REG))) {
				COLOR_ERR("DISP_IOCTL_WRITE_REG, copy_from_user failed\n");
				return -EFAULT;
			}

			pa = (unsigned int)wParams.reg;
			va = color_pa2va(pa);

			ret = color_is_reg_addr_valid(va);
			if (ret == 0) {
				COLOR_ERR("reg write, addr invalid, pa:0x%x(va:0x%lx)\n", pa, va);
				return -EFAULT;
			}

			/* if TDSHP, write PA directly */
			if (ret == 2) {
				if (cmdq == NULL) {
					mt_reg_sync_writel((unsigned int)(INREG32(va) &
									  ~(wParams.
									    mask)) | (wParams.val),
							   (unsigned long *)(va));
				} else {
					/* cmdqRecWrite(cmdq, TDSHP_PA_BASE + (wParams.reg - g_tdshp_va),
					wParams.val, wParams.mask); */
					cmdqRecWrite(cmdq, pa, wParams.val, wParams.mask);
				}
			} else {
				_color_reg_mask(cmdq, va, wParams.val, wParams.mask);
			}

			COLOR_NLOG("write pa:0x%x(va:0x%lx) = 0x%x (0x%x)\n", pa, va, wParams.val,
				   wParams.mask);

			break;

		}

	case DISP_IOCTL_READ_SW_REG:
		{
			DISP_READ_REG rParams;

			if (copy_from_user(&rParams, (void *)arg, sizeof(DISP_READ_REG))) {
				COLOR_ERR("DISP_IOCTL_READ_SW_REG, copy_from_user failed\n");
				return -EFAULT;
			}
			if (rParams.reg > DISP_COLOR_SWREG_END
			    || rParams.reg < DISP_COLOR_SWREG_START) {
				COLOR_ERR
				    ("sw reg read, addr invalid, addr min=0x%x, max=0x%x, addr=0x%x\n",
				     DISP_COLOR_SWREG_START, DISP_COLOR_SWREG_END, rParams.reg);
				return -EFAULT;
			}

			rParams.val = color_read_sw_reg(rParams.reg);

			COLOR_NLOG("read sw reg 0x%x = 0x%x\n", rParams.reg, rParams.val);

			if (copy_to_user((void *)arg, &rParams, sizeof(DISP_READ_REG))) {
				COLOR_ERR("DISP_IOCTL_READ_SW_REG, copy_to_user failed\n");
				return -EFAULT;
			}
			break;
		}

	case DISP_IOCTL_WRITE_SW_REG:
		{
			DISP_WRITE_REG wParams;

			if (copy_from_user(&wParams, (void *)arg, sizeof(DISP_WRITE_REG))) {
				COLOR_ERR("DISP_IOCTL_WRITE_SW_REG, copy_from_user failed\n");
				return -EFAULT;
			}


			if (wParams.reg > DISP_COLOR_SWREG_END
			    || wParams.reg < DISP_COLOR_SWREG_START) {
				COLOR_ERR
				    ("sw reg write, addr invalid, addr min=0x%x, max=0x%x, addr=0x%x\n",
				     DISP_COLOR_SWREG_START, DISP_COLOR_SWREG_END, wParams.reg);
				return -EFAULT;
			}

			color_write_sw_reg(wParams.reg, wParams.val);

			COLOR_NLOG("write sw reg  0x%x = 0x%x\n", wParams.reg, wParams.val);

			break;

		}

	case DISP_IOCTL_PQ_SET_BYPASS_COLOR:
		if (copy_from_user(&value, (void *)arg, sizeof(int))) {
			COLOR_ERR("DISP_IOCTL_PQ_SET_BYPASS_COLOR Copy from user failed\n");
			return -EFAULT;
		}

		ddp_color_bypass_color(DISP_MODULE_COLOR0, value, cmdq);
		color_trigger_refresh(DISP_MODULE_COLOR0);

		break;

	case DISP_IOCTL_PQ_SET_WINDOW:
		{
			DISP_PQ_WIN_PARAM win_param;

			if (copy_from_user(&win_param, (void *)arg, sizeof(DISP_PQ_WIN_PARAM))) {
				COLOR_ERR("DISP_IOCTL_PQ_SET_WINDOW Copy from user failed\n");
				return -EFAULT;
			}

			COLOR_DBG
			    ("DISP_IOCTL_PQ_SET_WINDOW, before set... en[%d], x[0x%x], y[0x%x]\n",
			     g_split_en, g_split_window_x, g_split_window_y);
			ddp_color_set_window(&win_param, cmdq);
			color_trigger_refresh(DISP_MODULE_COLOR0);

			break;
		}

	case DISP_IOCTL_PQ_GET_TDSHP_FLAG:
		if (copy_to_user((void *)arg, &g_tdshp_flag, sizeof(int))) {
			COLOR_ERR("DISP_IOCTL_PQ_GET_TDSHP_FLAG Copy to user failed\n");
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_PQ_SET_TDSHP_FLAG:
		if (copy_from_user(&g_tdshp_flag, (void *)arg, sizeof(int))) {
			COLOR_ERR("DISP_IOCTL_PQ_SET_TDSHP_FLAG Copy from user failed\n");
			return -EFAULT;
		}
		break;


	case DISP_IOCTL_PQ_GET_DC_PARAM:
		if (copy_to_user((void *)arg, &g_PQ_DC_Param, sizeof(DISP_PQ_DC_PARAM))) {
			COLOR_ERR("DISP_IOCTL_PQ_GET_DC_PARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_PQ_SET_DC_PARAM:
		if (copy_from_user(&g_PQ_DC_Param, (void *)arg, sizeof(DISP_PQ_DC_PARAM))) {
			COLOR_ERR("DISP_IOCTL_PQ_SET_DC_PARAM Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_PQ_GET_DS_PARAM:
			if (copy_to_user((void *)arg, &g_PQ_DS_Param, sizeof(DISP_PQ_DS_PARAM))) {
				COLOR_ERR("DISP_IOCTL_PQ_GET_DS_PARAM Copy to user failed\n");
				return -EFAULT;
			}

		break;

	case DISP_IOCTL_PQ_GET_MDP_TDSHP_REG:
			if (copy_to_user((void *)arg, &g_tdshp_reg, sizeof(MDP_TDSHP_REG))) {
				COLOR_ERR("DISP_IOCTL_PQ_GET_MDP_TDSHP_REG Copy to user failed\n");
				return -EFAULT;
			}

		break;

#ifdef DISP_MDP_COLOR_ON
	case DISP_IOCTL_PQ_GET_MDP_COLOR_CAP:
			if (copy_to_user((void *)arg, &mdp_color_cap, sizeof(MDP_COLOR_CAP))) {
				COLOR_ERR("DISP_IOCTL_PQ_GET_MDP_COLOR_CAP Copy to user failed\n");
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
void set_color_bypass(DISP_MODULE_ENUM module, int bypass, void *cmdq_handle)
{
	int offset = C0_OFFSET;

#ifdef DISP_COLOR_OFF
	COLOR_NLOG("set_color_bypass: DISP_COLOR_OFF, Color bypassed...\n");
	return;
#endif

	g_color_bypass = bypass;

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

	/* DISP_REG_SET(NULL, DISP_COLOR_INTERNAL_IP_WIDTH + offset, srcWidth);  //wrapper width */
	/* DISP_REG_SET(NULL, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, srcHeight); //wrapper height */


	if (bypass) {
		/* disable R2Y/Y2R in Color Wrapper */
		_color_reg_mask(cmdq_handle, DISP_COLOR_CM1_EN + offset, 0, 0x1);
		_color_reg_mask(cmdq_handle, DISP_COLOR_CM2_EN + offset, 0, 0x1);

		_color_reg_mask(cmdq_handle, DISP_COLOR_CFG_MAIN + offset, (1 << 7), 0x000000FF);	/* bypass all */
		_color_reg_mask(cmdq_handle, DISP_COLOR_START + offset, 0x00000003, 0x3);	/* color start */
	} else {
		_color_reg_mask(cmdq_handle, DISP_COLOR_CFG_MAIN + offset, (0 << 7), 0x000000FF);	/* bypass all */
		_color_reg_mask(cmdq_handle, DISP_COLOR_START + offset, 0x00000001, 0x3);	/* color start */

		/* enable R2Y/Y2R in Color Wrapper */
#if defined(CONFIG_ARCH_MT6797)
		/* RDMA & OVL will enable wide-gamut function*/
		/* disable rgb clipping function in CM1 to keep the wide-gamut range */
		_color_reg_mask(cmdq_handle, DISP_COLOR_CM1_EN + offset, 0x01, 0x03);
#else
		_color_reg_mask(cmdq_handle, DISP_COLOR_CM1_EN + offset, 0x01, 0x01);
#endif
		/* also set no rounding on Y2R */
		_color_reg_mask(cmdq_handle, DISP_COLOR_CM2_EN + offset, 0x11, 0x11);

}
#endif

static int _color_bypass(DISP_MODULE_ENUM module, int bypass)
{
	int offset = C0_OFFSET;

#ifdef DISP_COLOR_OFF
	COLOR_NLOG("_color_bypass: DISP_COLOR_OFF, Color bypassed...\n");
	return -1;
#endif

	g_color_bypass = bypass;

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

	/* _color_reg_set(NULL, DISP_COLOR_INTERNAL_IP_WIDTH + offset, srcWidth);  //wrapper width */
	/* _color_reg_set(NULL, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, srcHeight); //wrapper height */


	if (bypass) {
		/* disable R2Y/Y2R in Color Wrapper */
		_color_reg_mask(NULL, DISP_COLOR_CM1_EN + offset, 0, 0x1);
		_color_reg_mask(NULL, DISP_COLOR_CM2_EN + offset, 0, 0x1);

		_color_reg_mask(NULL, DISP_COLOR_CFG_MAIN + offset, (1 << 7), 0x000000FF);	/* bypass all */
		_color_reg_mask(NULL, DISP_COLOR_START + offset, 0x00000003, 0x3);	/* color start */
	} else {
		_color_reg_mask(NULL, DISP_COLOR_CFG_MAIN + offset, (0 << 7), 0x000000FF);	/* resume all */
		_color_reg_mask(NULL, DISP_COLOR_START + offset, 0x00000001, 0x3);	/* color start */

		/* enable R2Y/Y2R in Color Wrapper */
#if defined(CONFIG_ARCH_MT6797)
		/* RDMA & OVL will enable wide-gamut function*/
		/* disable rgb clipping function in CM1 to keep the wide-gamut range */
		_color_reg_mask(NULL, DISP_COLOR_CM1_EN + offset, 0x01, 0x03);
#else
		_color_reg_mask(NULL, DISP_COLOR_CM1_EN + offset, 0x01, 0x01);
#endif

#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795)
		_color_reg_mask(NULL, DISP_COLOR_CM2_EN + offset, 0x01, 0x11);	/* also set no rounding on Y2R */
#else
		_color_reg_mask(NULL, DISP_COLOR_CM2_EN + offset, 0x11, 0x11);	/* also set no rounding on Y2R */
#endif
	}

	return 0;
}

static int _color_build_cmdq(DISP_MODULE_ENUM module, void *cmdq_trigger_handle, CMDQ_STATE state)
{
	int ret = 0;

	if (cmdq_trigger_handle == NULL) {
		COLOR_ERR("cmdq_trigger_handle is NULL\n");
		return -1;
	}

	/* only get COLOR HIST on primary display */
	if ((module == DISP_MODULE_COLOR0) && (state == CMDQ_AFTER_STREAM_EOF)) {
#if defined(CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6795) || defined(CONFIG_ARCH_ELBRUS) \
	|| defined(CONFIG_ARCH_MT6757)
		ret =
		    cmdqRecReadToDataRegister(cmdq_trigger_handle,
					      ddp_reg_pa_base[DISP_REG_COLOR0] +
					      (DISP_COLOR_TWO_D_W1_RESULT - DISPSYS_COLOR0_BASE),
					      CMDQ_DATA_REG_PQ_COLOR);
#else
		ret =
		    cmdqRecReadToDataRegister(cmdq_trigger_handle,
					      ddp_reg_pa_base[DISP_REG_COLOR] +
					      (DISP_COLOR_TWO_D_W1_RESULT - DISPSYS_COLOR0_BASE),
					      CMDQ_DATA_REG_PQ_COLOR);
#endif
	}

	return ret;
}

DDP_MODULE_DRIVER ddp_driver_color = {
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
