// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __VDSP_DVFS_SHARKL5PRO_H__
#define __VDSP_DVFS_SHARKL5PRO_H__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>
#include "sprd_dvfs_apsys.h"

enum {
	SHARKL5PRO_VOLT70 = 0,
	SHARKL5PRO_VOLT75,
	SHARKL5PRO_VOLT80,
};

enum {
	SHARKL5PRO_EDAP_DIV_0 = 0,
	SHARKL5PRO_EDAP_DIV_1,
	SHARKL5PRO_EDAP_DIV_2,
	SHARKL5PRO_EDAP_DIV_3,
};

enum {
	SHARKL5PRO_VDSP_CLK_INDEX_256M = 0,
	SHARKL5PRO_VDSP_CLK_INDEX_384M,
	SHARKL5PRO_VDSP_CLK_INDEX_512M,
	SHARKL5PRO_VDSP_CLK_INDEX_614M4,
	SHARKL5PRO_VDSP_CLK_INDEX_768M,
	SHARKL5PRO_VDSP_CLK_INDEX_936M,
};

enum {
	SHARKL5PRO_VDSP_CLK256M = 256000000,
	SHARKL5PRO_VDSP_CLK384M = 384000000,
	SHARKL5PRO_VDSP_CLK512M = 512000000,
	SHARKL5PRO_VDSP_CLK614M4 = 614400000,
	SHARKL5PRO_VDSP_CLK768M = 768000000,
	SHARKL5PRO_VDSP_CLK936M = 936000000,
};

struct vdsp_sharkl5pro_dvfs_map_cfg {
	u32 map_index;
	u32 volt_level;
	u32 clk_level;
	u32 clk_rate;
	u32 edap_div;
};

#endif /* __SPRD_DVFS_VDSP_H__ */
