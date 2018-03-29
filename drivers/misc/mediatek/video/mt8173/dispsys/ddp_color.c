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
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>

#include "ddp_reg.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_color.h"
#include "cmdq_def.h"
#include "ddp_log.h"

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
	 0x30, 5, 0x06, 0x12, 40, 0x40, 0x80, 0x40, 0x40, 1,
	 0x80, 0x60, 0x80, 0x10, 0x34, 0x40, 0x40, 1, 0x80, 0xa,
	 0x19, 0x00, 0x20, 0, 0, 1, 2, 1, 80}
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
};
static DEFINE_MUTEX(g_color_reg_lock);
static DISPLAY_COLOR_REG_T g_color_reg;
static int g_color_reg_valid;

int color_dbg_en = 1;
#define COLOR_ERR(fmt, arg...) DDPERR("[COLOR] " fmt "\n", ##arg)
#define COLOR_DBG(fmt, arg...) \
	do { if (color_dbg_en) DDPDBG("[COLOR] " fmt "\n", ##arg); } while (0)


static ddp_module_notify g_color_cb;

static DISPLAY_TDSHP_T g_TDSHP_Index;

static unsigned int g_split_en;
static unsigned int g_split_window_x = 0xFFFF0000;
static unsigned int g_split_window_y = 0xFFFF0000;
static unsigned long g_color_window = 0x40106051;
static unsigned long g_color0_dst_w;
static unsigned long g_color0_dst_h;
static unsigned long g_color1_dst_w;
static unsigned long g_color1_dst_h;

static int g_color_bypass;
static int g_tdshp_flag;	/* 0: normal, 1: tuning mode */
int ncs_tuning_mode = 0;

static unsigned long g_tdshp_va;
static unsigned long g_tdshp1_va;

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

void DpEngine_COLORonInit(DISP_MODULE_ENUM module, void *__cmdq)
{
	/* printk("===================init COLOR =======================\n"); */
	int offset = C0_OFFSET;
	cmdqRecHandle cmdq = (cmdqRecHandle) __cmdq;

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

	if (g_color_bypass == 0) {
		DISP_REG_SET(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 29));	/* color enable */
		DISP_REG_SET(cmdq, DISP_COLOR_START + offset, 0x00000001);	/* color start */
	} else {
		DISP_REG_SET_FIELD(cmdq, CFG_MAIN_FLD_COLOR_DBUF_EN, DISP_COLOR_CFG_MAIN + offset,
				   0x1);
		DISP_REG_SET_FIELD(cmdq, START_FLD_DISP_COLOR_START, DISP_COLOR_START + offset,
				   0x1);
	}

	COLOR_DBG("DpEngine_COLORonInit(), en[%d],  x[0x%x], y[0x%x]\n", g_split_en,
		  g_split_window_x, g_split_window_y);
	DISP_REG_SET(cmdq, DISP_COLOR_DBG_CFG_MAIN + offset, (g_split_en << 3));
	DISP_REG_SET(cmdq, DISP_COLOR_WIN_X_MAIN + offset, g_split_window_x);
	DISP_REG_SET(cmdq, DISP_COLOR_WIN_Y_MAIN + offset, g_split_window_y);

	/* enable interrupt */
	DISP_REG_SET(cmdq, DISP_COLOR_INTEN + offset, 0x00000007);

	/* Set 10bit->8bit Rounding */
	DISP_REG_SET(cmdq, DISP_COLOR_OUT_SEL + offset, 0x333);

#if defined(CONFIG_MTK_AAL_SUPPORT)
	/* c-boost ??? */
	DISP_REG_SET(cmdq, DISP_COLOR_C_BOOST_MAIN + offset, 0xFF402280);
	DISP_REG_SET(cmdq, DISP_COLOR_C_BOOST_MAIN_2 + offset, 0x00000000);
#endif

}

static unsigned long color_get_TDSHP_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-mdp_tdshp0");
	if (node) {
		VA = (unsigned long)of_iomap(node, 0);
		pr_err("color_get_TDSHP_VA: TDSHP VA: 0x%lx\n", VA);
	} else {
		VA = 0;
		pr_err("color_get_TDSHP_VA: not found mediatek,mt8173-mdp_tdshp0\n");
	}

	return VA;
}

static unsigned long color_get_TDSHP1_VA(void)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-mdp_tdshp1");
	if (node) {
		VA = (unsigned long)of_iomap(node, 0);
		pr_err("color_get_TDSHP_VA: TDSHP1 VA: 0x%lx\n", VA);
	} else {
		VA = 0;
		pr_err("color_get_TDSHP_VA: not found mediatek,mt8173-mdp_tdshp1\n");
	}

	 return VA;
}

static unsigned int color_is_reg_addr_valid(unsigned long addr)
{
	unsigned int i = 0;

	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if ((addr >= ddp_regbase_va[i]) &&
			(addr < (ddp_regbase_va[i] + 0x1000))) {
			break;
		}
	}

	if (i < DISP_MODULE_NUM) {
		COLOR_DBG("addr valid, addr=0x%lx!\n", addr);
	} else {
		/* check if TDSHP base address */
		if ((addr >= g_tdshp_va) && (addr < (g_tdshp_va + 0x1000))) {
			COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, "TDSHP0");
		} else if ((addr >= g_tdshp1_va) && (addr < (g_tdshp1_va + 0x1000))) {
			COLOR_DBG("addr valid, addr=0x%lx, module=%s!\n", addr, "TDSHP1");
		} else {
			COLOR_ERR("invalid address! addr=0x%lx!\n", addr);
			return 0;
		}
		return 2;
	}
	return 1;
}

static inline unsigned long ddp_color_convert_pa2va(unsigned int pa)
{
	unsigned int i = 0;

	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if (ddp_regbase_pa[i] == (pa & (~0xfff)))
			return (ddp_regbase_va[i] + (pa & 0xfff));
	}

	if ((TDSHP_PA_BASE <= pa) &&
		(pa < (TDSHP_PA_BASE + 0x1000))) {
		pr_err("color_pa2va(), TDSHP PA:0x%x, PABase[0x%x], VABase[0x%lx]\n", pa, TDSHP_PA_BASE, g_tdshp_va);
		return g_tdshp_va + (pa - TDSHP_PA_BASE);
	}

	if ((TDSHP1_PA_BASE <= pa) && (pa < (TDSHP1_PA_BASE + 0x1000))) {
		pr_err("color_pa2va(), TDSHP1 PA:0x%x, PABase[0x%x], VABase[0x%lx]\n",
				pa, TDSHP1_PA_BASE, g_tdshp1_va);
		return g_tdshp1_va + (pa - TDSHP1_PA_BASE);
	}

	pr_err("color_pa2va(), NO FOUND VA!! PA:0x%x\n", pa);

	return 0;
}

static void _color_reg_mask(void *__cmdq, unsigned long addr, unsigned int value, unsigned int mask)
{
	cmdqRecHandle cmdq = (cmdqRecHandle)__cmdq;

	DISP_REG_MASK(cmdq, addr, value, mask);
}

void DpEngine_COLORonConfig(DISP_MODULE_ENUM module, unsigned int srcWidth, unsigned int srcHeight, void *__cmdq)
{
	int index = 0;
	unsigned int u4Temp = 0;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int offset = C0_OFFSET;
	DISP_PQ_PARAM *pq_param_p = &g_Color_Param[COLOR_ID_0];
	cmdqRecHandle cmdq = (cmdqRecHandle) __cmdq;

	if (DISP_MODULE_COLOR1 == module) {
		offset = C1_OFFSET;
		pq_param_p = &g_Color_Param[COLOR_ID_1];
	}

	if ((pq_param_p->u4SatGain >= COLOR_TUNING_INDEX && pq_param_p->u4SatGain != 0xffff) ||
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


	DISP_REG_SET(cmdq, DISP_COLOR_INTERNAL_IP_WIDTH + offset, srcWidth);	/* wrapper width */
	DISP_REG_SET(cmdq, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, srcHeight);	/* wrapper height */


	if (g_color_bypass == 0) {
		/* enable R2Y/Y2R in Color Wrapper */
		DISP_REG_SET(cmdq, DISP_COLOR_CM1_EN + offset, 1);
		DISP_REG_SET(cmdq, DISP_COLOR_CM2_EN + offset, 1);
	}
	/* config parameter from customer color_index.h */
	DISP_REG_SET(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_1 + offset,
		     (g_Color_Index.BRIGHTNESS[pq_param_p->u4Brightness] << 16) | g_Color_Index.
		     CONTRAST[pq_param_p->u4Contrast]);

	if (pq_param_p->u4SatGain == 0xffff)
		DISP_REG_SET(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_2 + offset,
					 (0x200 << 16) | 0);
	else
	DISP_REG_SET(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_2 + offset,
		     (0x200 << 16) | g_Color_Index.GLOBAL_SAT[pq_param_p->u4SatGain]);


	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		DISP_REG_SET(cmdq, DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index + offset,
			(g_Color_Index.PARTIAL_Y[pq_param_p->u4PartialY][2 * index] |
			g_Color_Index.PARTIAL_Y[pq_param_p->u4PartialY][2 * index + 1] << 16));
	}


	/* Partial Saturation Function */

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_0 + offset,
		     (g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG1][0] |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG1][1] << 8 |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG1][2] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_1 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][1] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][2] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][3] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_2 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][5] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][6] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG1][7] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_3 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][1] |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][2] << 8 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][3] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_4 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG1][5] |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG1][0] << 8 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG1][1] << 16 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG1][2] << 24));

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_0 + offset,
		     (g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG2][0] |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG2][1] << 8 |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG2][2] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_1 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][1] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][2] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][3] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_2 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][5] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][6] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG2][7] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_3 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][1] |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][2] << 8 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][3] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_4 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG2][5] |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG2][0] << 8 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG2][1] << 16 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG2][2] << 24));

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_0 + offset,
		     (g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG3][0] |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG3][1] << 8 |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SG3][2] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_1 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][1] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][2] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][3] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_2 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][5] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][6] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SG3][7] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_3 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][1] |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][2] << 8 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][3] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_4 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SG3][5] |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG3][0] << 8 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG3][1] << 16 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SG3][2] << 24));

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_0 + offset,
		     (g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP1][0] |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP1][1] << 8 |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP1][2] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_1 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][1] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][2] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][3] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_2 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][5] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][6] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP1][7] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_3 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][1] |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][2] << 8 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][3] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_4 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP1][5] |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP1][0] << 8 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP1][1] << 16 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP1][2] << 24));

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_0 + offset,
		     (g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP2][0] |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP2][1] << 8 |
		      g_Color_Index.PURP_TONE_S[pq_param_p->u4SatAdj[PURP_TONE]][SP2][2] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_1 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][1] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][2] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][3] << 16 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_2 + offset,
		     (g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][5] |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][6] << 8 |
		      g_Color_Index.SKIN_TONE_S[pq_param_p->u4SatAdj[SKIN_TONE]][SP2][7] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_3 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][1] |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][2] << 8 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][3] << 16 |
		      g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_4 + offset,
		     (g_Color_Index.GRASS_TONE_S[pq_param_p->u4SatAdj[GRASS_TONE]][SP2][5] |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP2][0] << 8 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP2][1] << 16 |
		      g_Color_Index.SKY_TONE_S[pq_param_p->u4SatAdj[SKY_TONE]][SP2][2] << 24));

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
		DISP_REG_SET(cmdq, DISP_COLOR_LOCAL_HUE_CD_0 + offset + 4 * index, u4Temp);
	}

	/* color window */

	DISP_REG_SET(cmdq, DISP_COLOR_TWO_D_WINDOW_1 + offset, g_color_window);
}

static void color_write_hw_reg(DISP_MODULE_ENUM module,
	const DISPLAY_COLOR_REG_T *color_reg, void *cmdq)
{
	int offset = C0_OFFSET;
	int index;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned int u4Temp = 0;

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

	if (g_color_bypass == 0) {
		DISP_REG_SET(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 8));	/* enable wide_gamut */
		DISP_REG_SET(cmdq, DISP_COLOR_START + offset, 0x00000001);	/* color start */
		/* enable R2Y/Y2R in Color Wrapper */
		DISP_REG_SET(cmdq, DISP_COLOR_CM1_EN + offset, 0x01);
		DISP_REG_SET(cmdq, DISP_COLOR_CM2_EN + offset, 0x11); /* also set no rounding on Y2R */
	} else {
		DISP_REG_SET_FIELD(cmdq, CFG_MAIN_FLD_COLOR_DBUF_EN, DISP_COLOR_CFG_MAIN + offset,
					 0x1);
		DISP_REG_SET_FIELD(cmdq, START_FLD_DISP_COLOR_START, DISP_COLOR_START + offset,
					 0x1);
	}

	/* for partial Y contour issue */
	DISP_REG_MASK(cmdq, DISP_COLOR_LUMA_ADJ + offset, 0x0, 0x0000007F);


	DISP_REG_SET(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_1 + offset,
		(color_reg->BRIGHTNESS << 16) | color_reg->CONTRAST);
	if (color_reg->GLOBAL_SAT == 0xffff)
		DISP_REG_SET(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_2 + offset,
				(0x200 << 16) | 0);
	else
	DISP_REG_SET(cmdq, DISP_COLOR_G_PIC_ADJ_MAIN_2 + offset,
		(0x200 << 16) | color_reg->GLOBAL_SAT);


	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		DISP_REG_SET(cmdq, DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index + offset,
			(color_reg->PARTIAL_Y[2 * index] |
			(color_reg->PARTIAL_Y[2 * index + 1] << 16)));
	}


	/* Partial Saturation Function */

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_0 + offset,
		(color_reg->PURP_TONE_S[SG1][0]) |
		(color_reg->PURP_TONE_S[SG1][1] << 8) |
		(color_reg->PURP_TONE_S[SG1][2] << 16) |
		(color_reg->SKIN_TONE_S[SG1][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_1 + offset,
		(color_reg->SKIN_TONE_S[SG1][1]) |
		(color_reg->SKIN_TONE_S[SG1][2] << 8) |
		(color_reg->SKIN_TONE_S[SG1][3] << 16) |
		(color_reg->SKIN_TONE_S[SG1][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_2 + offset,
		(color_reg->SKIN_TONE_S[SG1][5]) |
		(color_reg->SKIN_TONE_S[SG1][6] << 8) |
		(color_reg->SKIN_TONE_S[SG1][7] << 16) |
		(color_reg->GRASS_TONE_S[SG1][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_3 + offset,
		(color_reg->GRASS_TONE_S[SG1][1]) |
		(color_reg->GRASS_TONE_S[SG1][2] << 8) |
		(color_reg->GRASS_TONE_S[SG1][3] << 16) |
		(color_reg->GRASS_TONE_S[SG1][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN1_4 + offset,
		(color_reg->GRASS_TONE_S[SG1][5]) |
		(color_reg->SKY_TONE_S[SG1][0] << 8) |
		(color_reg->SKY_TONE_S[SG1][1] << 16) |
		(color_reg->SKY_TONE_S[SG1][2] << 24));

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_0 + offset,
		(color_reg->PURP_TONE_S[SG2][0]) |
		(color_reg->PURP_TONE_S[SG2][1] << 8) |
		(color_reg->PURP_TONE_S[SG2][2] << 16) |
		(color_reg->SKIN_TONE_S[SG2][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_1 + offset,
		(color_reg->SKIN_TONE_S[SG2][1]) |
		(color_reg->SKIN_TONE_S[SG2][2] << 8) |
		(color_reg->SKIN_TONE_S[SG2][3] << 16) |
		(color_reg->SKIN_TONE_S[SG2][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_2 + offset,
		(color_reg->SKIN_TONE_S[SG2][5]) |
		(color_reg->SKIN_TONE_S[SG2][6] << 8) |
		(color_reg->SKIN_TONE_S[SG2][7] << 16) |
		(color_reg->GRASS_TONE_S[SG2][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_3 + offset,
		(color_reg->GRASS_TONE_S[SG2][1]) |
		(color_reg->GRASS_TONE_S[SG2][2] << 8) |
		(color_reg->GRASS_TONE_S[SG2][3] << 16) |
		(color_reg->GRASS_TONE_S[SG2][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN2_4 + offset,
		(color_reg->GRASS_TONE_S[SG2][5]) |
		(color_reg->SKY_TONE_S[SG2][0] << 8) |
		(color_reg->SKY_TONE_S[SG2][1] << 16) |
		(color_reg->SKY_TONE_S[SG2][2] << 24));

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_0 + offset,
		(color_reg->PURP_TONE_S[SG3][0]) |
		(color_reg->PURP_TONE_S[SG3][1] << 8) |
		(color_reg->PURP_TONE_S[SG3][2] << 16) |
		(color_reg->SKIN_TONE_S[SG3][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_1 + offset,
		(color_reg->SKIN_TONE_S[SG3][1]) |
		(color_reg->SKIN_TONE_S[SG3][2] << 8) |
		(color_reg->SKIN_TONE_S[SG3][3] << 16) |
		(color_reg->SKIN_TONE_S[SG3][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_2 + offset,
		(color_reg->SKIN_TONE_S[SG3][5]) |
		(color_reg->SKIN_TONE_S[SG3][6] << 8) |
		(color_reg->SKIN_TONE_S[SG3][7] << 16) |
		(color_reg->GRASS_TONE_S[SG3][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_3 + offset,
		(color_reg->GRASS_TONE_S[SG3][1]) |
		(color_reg->GRASS_TONE_S[SG3][2] << 8) |
		(color_reg->GRASS_TONE_S[SG3][3] << 16) |
		(color_reg->GRASS_TONE_S[SG3][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_GAIN3_4 + offset,
		(color_reg->GRASS_TONE_S[SG3][5]) |
		(color_reg->SKY_TONE_S[SG3][0] << 8) |
		(color_reg->SKY_TONE_S[SG3][1] << 16) |
		(color_reg->SKY_TONE_S[SG3][2] << 24));

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_0 + offset,
		(color_reg->PURP_TONE_S[SP1][0]) |
		(color_reg->PURP_TONE_S[SP1][1] << 8) |
		(color_reg->PURP_TONE_S[SP1][2] << 16) |
		(color_reg->SKIN_TONE_S[SP1][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_1 + offset,
		(color_reg->SKIN_TONE_S[SP1][1]) |
		(color_reg->SKIN_TONE_S[SP1][2] << 8) |
		(color_reg->SKIN_TONE_S[SP1][3] << 16) |
		(color_reg->SKIN_TONE_S[SP1][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_2 + offset,
		(color_reg->SKIN_TONE_S[SP1][5]) |
		(color_reg->SKIN_TONE_S[SP1][6] << 8) |
		(color_reg->SKIN_TONE_S[SP1][7] << 16) |
		(color_reg->GRASS_TONE_S[SP1][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_3 + offset,
		(color_reg->GRASS_TONE_S[SP1][1]) |
		(color_reg->GRASS_TONE_S[SP1][2] << 8) |
		(color_reg->GRASS_TONE_S[SP1][3] << 16) |
		(color_reg->GRASS_TONE_S[SP1][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT1_4 + offset,
		(color_reg->GRASS_TONE_S[SP1][5]) |
		(color_reg->SKY_TONE_S[SP1][0] << 8) |
		(color_reg->SKY_TONE_S[SP1][1] << 16) |
		(color_reg->SKY_TONE_S[SP1][2] << 24));

	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_0 + offset,
		(color_reg->PURP_TONE_S[SP2][0]) |
		(color_reg->PURP_TONE_S[SP2][1] << 8) |
		(color_reg->PURP_TONE_S[SP2][2] << 16) |
		(color_reg->SKIN_TONE_S[SP2][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_1 + offset,
		(color_reg->SKIN_TONE_S[SP2][1]) |
		(color_reg->SKIN_TONE_S[SP2][2] << 8) |
		(color_reg->SKIN_TONE_S[SP2][3] << 16) |
		(color_reg->SKIN_TONE_S[SP2][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_2 + offset,
		(color_reg->SKIN_TONE_S[SP2][5]) |
		(color_reg->SKIN_TONE_S[SP2][6] << 8) |
		(color_reg->SKIN_TONE_S[SP2][7] << 16) |
		(color_reg->GRASS_TONE_S[SP2][0] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_3 + offset,
		(color_reg->GRASS_TONE_S[SP2][1]) |
		(color_reg->GRASS_TONE_S[SP2][2] << 8) |
		(color_reg->GRASS_TONE_S[SP2][3] << 16) |
		(color_reg->GRASS_TONE_S[SP2][4] << 24));
	DISP_REG_SET(cmdq, DISP_COLOR_PART_SAT_POINT2_4 + offset,
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
		DISP_REG_SET(cmdq, DISP_COLOR_LOCAL_HUE_CD_0 + offset + 4 * index, u4Temp);
	}


	/* color window */
	DISP_REG_SET(cmdq, DISP_COLOR_TWO_D_WINDOW_1 + offset, g_color_window);
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
	cmdqRecHandle cmdq = (cmdqRecHandle) __cmdq;

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

	if (bypass)
		DISP_REG_MASK(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 7), 0x00000080);	/* bypass all */
	else
		DISP_REG_MASK(cmdq, DISP_COLOR_CFG_MAIN + offset, (0 << 7), 0x00000080);	/* resume all */
}

static void ddp_color_set_window(DISP_PQ_WIN_PARAM *win_param, void *__cmdq)
{
	const int offset = C0_OFFSET;
	cmdqRecHandle cmdq = (cmdqRecHandle) __cmdq;

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		g_split_en = 1;
		g_split_window_x = (win_param->end_x << 16) | win_param->start_x;
		g_split_window_y = (win_param->end_y << 16) | win_param->start_y;

	} else {
		g_split_en = 0;
		g_split_window_x = 0xFFFF0000;
		g_split_window_y = 0xFFFF0000;
	}

	COLOR_DBG("ddp_color_set_window(), en[%d],  x[0x%x], y[0x%x]\n", g_split_en,
		  g_split_window_x, g_split_window_y);

	DISP_REG_MASK(cmdq, DISP_COLOR_DBG_CFG_MAIN + offset, (g_split_en << 3), 0x00000008);	/* split enable */
	DISP_REG_SET(cmdq, DISP_COLOR_WIN_X_MAIN + offset, g_split_window_x);
	DISP_REG_SET(cmdq, DISP_COLOR_WIN_Y_MAIN + offset, g_split_window_y);
}

#if 0
static unsigned int color_read_sw_reg(unsigned int reg_id)
{
	unsigned int ret = 0;

	if (reg_id >= SWREG_PQDC_BLACK_EFFECT_ENABLE && reg_id <= SWREG_PQDC_PROTECT_REGION_WEIGHT) {
		ret = (unsigned int)g_PQ_DC_Param.param[reg_id - SWREG_PQDC_BLACK_EFFECT_ENABLE];
		return ret;
	}

	switch (reg_id) {
	case SWREG_COLOR_BASE_ADDRESS:
		{
			ret = DISPSYS_COLOR0_BASE;
			break;
		}

	case SWREG_TDSHP_TUNING_MODE:
		{
			ret = (unsigned int)g_tdshp_flag;
			break;
		}

	default:
		break;

	}

	return ret;
}

static void color_write_sw_reg(unsigned int reg_id, unsigned int value)
{
	if (reg_id >= SWREG_PQDC_BLACK_EFFECT_ENABLE && reg_id <= SWREG_PQDC_PROTECT_REGION_WEIGHT) {
		g_PQ_DC_Param.param[reg_id - SWREG_PQDC_BLACK_EFFECT_ENABLE] = (int)value;
		return;
	}

	switch (reg_id) {
	case SWREG_TDSHP_TUNING_MODE:
		{
			g_tdshp_flag = (int)value;
			break;
		}

	default:
		break;

	}
}
#endif

static int _color_clock_on(DISP_MODULE_ENUM module, void *cmq_handle)
{
	if (module == DISP_MODULE_COLOR0) {
		ddp_module_clock_enable(MM_CLK_DISP_COLOR0, true);
		COLOR_DBG("color[0]_clock_on CG 0x%x\n",
			  DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	} else {
		ddp_module_clock_enable(MM_CLK_DISP_COLOR1, true);
		COLOR_DBG("color[1]_clock_on CG 0x%x\n",
			  DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	}



	return 0;
}

static int _color_clock_off(DISP_MODULE_ENUM module, void *cmq_handle)
{

	if (module == DISP_MODULE_COLOR0) {
		COLOR_DBG("color[0]_clock_off\n");
		ddp_module_clock_enable(MM_CLK_DISP_COLOR0, false);
	} else {
		COLOR_DBG("color[1]_clock_off\n");
		ddp_module_clock_enable(MM_CLK_DISP_COLOR1, false);
	}

	return 0;
}

static int _color_init(DISP_MODULE_ENUM module, void *cmq_handle)
{
	_color_clock_on(module, cmq_handle);
	g_tdshp_va = color_get_TDSHP_VA();
	g_tdshp1_va = color_get_TDSHP1_VA();
	return 0;
}

static int _color_deinit(DISP_MODULE_ENUM module, void *cmq_handle)
{
	_color_clock_off(module, cmq_handle);
	return 0;
}

static int _color_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmq_handle)
{
	if (!pConfig->dst_dirty)
		return 0;

	if (module == DISP_MODULE_COLOR0) {
		g_color0_dst_w = pConfig->dst_w;
		g_color0_dst_h = pConfig->dst_h;
	} else {
		g_color1_dst_w = pConfig->dst_w;
		g_color1_dst_h = pConfig->dst_h;

		/* set bypass to COLOR1 */
		{
			const int offset = C1_OFFSET;
			cmdqRecHandle cmdq = (cmdqRecHandle) cmq_handle;

			/* disable R2Y/Y2R in Color Wrapper */
			DISP_REG_SET(cmdq, DISP_COLOR_CM1_EN + offset, 0);
			DISP_REG_SET(cmdq, DISP_COLOR_CM2_EN + offset, 0);

			DISP_REG_SET(cmdq, DISP_COLOR_CFG_MAIN + offset, (1 << 7));	/* all bypass */
			DISP_REG_SET(cmdq, DISP_COLOR_START + offset, 0x00000003);	/* color start */
		}

		return 0;
	}

	DpEngine_COLORonInit(module, cmq_handle);
	if (g_color_reg_valid) {
		COLOR_DBG("color_config : color_write_hw_reg\n");
		mutex_lock(&g_color_reg_lock);
		DISP_REG_SET(cmq_handle, DISP_COLOR_INTERNAL_IP_WIDTH, pConfig->dst_w);
		DISP_REG_SET(cmq_handle, DISP_COLOR_INTERNAL_IP_HEIGHT, pConfig->dst_h);
		color_write_hw_reg(DISP_MODULE_COLOR0, &g_color_reg, cmq_handle);
		mutex_unlock(&g_color_reg_lock);
	} else {
		COLOR_DBG("color_config : DpEngine_COLORonConfig\n");
		DpEngine_COLORonConfig(module, pConfig->dst_w, pConfig->dst_h, cmq_handle);
	}

#ifdef CONFIG_FPGA_EARLY_PORTING
	ddp_color_bypass_color(module, 1, NULL);
#endif

	return 0;
}

static int _color_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
	g_color_cb = notify;
	return 0;
}

static int _color_io(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *cmdq)
{
	int ret = 0;
	int value = 0;
	DISP_PQ_PARAM *pq_param;
	DISPLAY_PQ_T *pq_index;
	DISPLAY_TDSHP_T *tdshp_index;

	COLOR_DBG("_color_io: msg %x\n", msg);

	switch (msg) {
	case DISP_IOCTL_SET_PQPARAM:
		/* case DISP_IOCTL_SET_C0_PQPARAM: */

		pq_param = get_Color_config(COLOR_ID_0);
		if (copy_from_user(pq_param, (void *)arg, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_SET_PQPARAM Copy from user failed\n");

			return -EFAULT;
		}

		if (ncs_tuning_mode == 0) {	/* normal mode */
			DpEngine_COLORonInit(DISP_MODULE_COLOR0, cmdq);
			DpEngine_COLORonConfig(DISP_MODULE_COLOR0, g_color0_dst_w, g_color0_dst_h,
					       cmdq);

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

	case DISP_IOCTL_SET_C1_PQPARAM:

		pq_param = get_Color_config(COLOR_ID_1);
		if (copy_from_user(pq_param, (void *)arg, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_SET_PQPARAM Copy from user failed\n");

			return -EFAULT;
		}

		if (ncs_tuning_mode == 0) {	/* normal mode */
			DpEngine_COLORonInit(DISP_MODULE_COLOR1, cmdq);
			DpEngine_COLORonConfig(DISP_MODULE_COLOR1, g_color1_dst_w, g_color1_dst_h,
					       cmdq);

			color_trigger_refresh(DISP_MODULE_COLOR1);

			COLOR_DBG("SET_PQ_PARAM(1)\n");
		} else {
			/* ncs_tuning_mode = 0; */
			COLOR_DBG("SET_PQ_PARAM(1), bypassed by ncs_tuning_mode = 1\n");
		}

		break;

	case DISP_IOCTL_GET_C1_PQPARAM:

		pq_param = get_Color_config(COLOR_ID_1);
		if (copy_to_user((void *)arg, pq_param, sizeof(DISP_PQ_PARAM))) {
			COLOR_ERR("DISP_IOCTL_GET_PQPARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_PQINDEX:

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

		tdshp_index = get_TDSHP_index();
		if (copy_from_user(tdshp_index, (void *)arg, sizeof(DISPLAY_TDSHP_T))) {
			COLOR_ERR("DISP_IOCTL_SET_TDSHPINDEX Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_TDSHPINDEX:

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
			unsigned long addr_va = 0;

			if (copy_from_user(&rParams, (void *)arg, sizeof(DISP_READ_REG))) {
				COLOR_ERR("DISP_IOCTL_READ_REG, copy_from_user failed\n");
				return -EFAULT;
			}

			addr_va = ddp_color_convert_pa2va(rParams.reg);

			if (addr_va == 0) {
				COLOR_ERR
				    ("reg read, addr invalid, addr min=0x%d, max=0x%lx, addr=0x%u\n",
				     DISPSYS_REG_ADDR_MIN, DISPSYS_REG_ADDR_MAX, rParams.reg);
				return -EFAULT;
			}

			rParams.val = (DISP_REG_GET(addr_va)) & rParams.mask;

			COLOR_DBG("read 0x%x = 0x%x (0x%x)\n", rParams.reg, rParams.val,
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
		va = ddp_color_convert_pa2va(pa);
		ret = color_is_reg_addr_valid(va);
		if (ret == 0) {
			COLOR_ERR("reg write, addr invalid, pa:0x%x(va:0x%lx)\n", pa, va);
			return -EFAULT;
		}

		/* if TDSHP, write PA directly */
		if (ret == 2) {
			if (cmdq == NULL)
				mt_reg_sync_writel((unsigned int)(INREG32(va) & ~(wParams.mask)) | (wParams.val),
						(volatile unsigned long*)(va));
			else
				cmdqRecWrite(cmdq, pa, wParams.val, wParams.mask);
		} else {
			_color_reg_mask(cmdq, va, wParams.val, wParams.mask);
		}

		COLOR_ERR("write pa:0x%x(va:0x%lx) = 0x%x (0x%x)\n", pa, va, wParams.val, wParams.mask);

			break;

		}

	case DISP_IOCTL_READ_SW_REG:
		{
#if 1
			COLOR_ERR("ioctl DISP_IOCTL_READ_SW_REG need inprove, return directly\n");
			break;
#else
			DISP_READ_REG rParams;
			unsigned int rValue;

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

			rValue = color_read_sw_reg(rParams.reg);

			COLOR_DBG("read sw reg 0x%x = 0x%x\n", rParams.reg, rValue);

			if (copy_to_user(rParams.val, &rValue, sizeof(unsigned int))) {
				COLOR_ERR("DISP_IOCTL_READ_SW_REG, copy_to_user failed\n");
				return -EFAULT;
			}
			break;
#endif
		}

	case DISP_IOCTL_WRITE_SW_REG:
		{
#if 1
			COLOR_ERR("ioctl DISP_IOCTL_WRITE_SW_REG need inprove, return directly\n");
			break;
#else
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

			COLOR_DBG("write sw reg  0x%x = 0x%x\n", wParams.reg, wParams.val);

			break;
#endif
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

	default:
		{
			COLOR_DBG("ioctl not supported failed\n");
			return -EFAULT;
		}
	}

	return ret;
}

static int _color_bypass(DISP_MODULE_ENUM module, int bypass)
{
	int offset = C0_OFFSET;

	g_color_bypass = bypass;

	if (DISP_MODULE_COLOR1 == module)
		offset = C1_OFFSET;

	/* DISP_REG_SET(NULL, DISP_COLOR_INTERNAL_IP_WIDTH + offset, srcWidth);  //wrapper width */
	/* DISP_REG_SET(NULL, DISP_COLOR_INTERNAL_IP_HEIGHT + offset, srcHeight); //wrapper height */


	if (bypass) {
		/* disable R2Y/Y2R in Color Wrapper */
		DISP_REG_SET(NULL, DISP_COLOR_CM1_EN + offset, 0);
		DISP_REG_SET(NULL, DISP_COLOR_CM2_EN + offset, 0);

		DISP_REG_SET(NULL, DISP_COLOR_CFG_MAIN + offset, (1 << 7));	/* all bypass */
		DISP_REG_SET(NULL, DISP_COLOR_START + offset, 0x00000003);	/* color start */
	} else {
		DISP_REG_SET(NULL, DISP_COLOR_CFG_MAIN + offset, (1 << 29));	/* color enable */
		DISP_REG_SET(NULL, DISP_COLOR_START + offset, 0x00000001);	/* color start */

		/* enable R2Y/Y2R in Color Wrapper */
		DISP_REG_SET(NULL, DISP_COLOR_CM1_EN + offset, 1);
		DISP_REG_SET(NULL, DISP_COLOR_CM2_EN + offset, 1);
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
		ret =
		    cmdqRecReadToDataRegister(cmdq_trigger_handle,
					      ddp_addr_convert_va2pa(DISP_COLOR_TWO_D_W1_RESULT),
					      CMDQ_DATA_REG_PQ_COLOR);
	}

	return ret;
}

DDP_MODULE_DRIVER ddp_driver_color = {
	.init = _color_init,
	.deinit = _color_deinit,
	.config = _color_config,
	.start = NULL,
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
};
