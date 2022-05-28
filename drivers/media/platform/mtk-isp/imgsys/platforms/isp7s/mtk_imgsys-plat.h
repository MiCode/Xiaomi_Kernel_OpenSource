/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_PLAT_H_
#define _MTK_IMGSYS_PLAT_H_

#include <linux/clk.h>

struct clk_bulk_data imgsys_isp7_clks[] = {
	{
		.id = "IMGSYS_CG_IMG_TRAW0",
	},
	{
		.id = "IMGSYS_CG_IMG_TRAW1",
	},
	{
		.id = "IMGSYS_CG_IMG_VCORE_GALS",
	},
	{
		.id = "IMGSYS_CG_IMG_DIP0",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE0",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE1",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE2",
	},
	{
		.id = "IMGSYS_CG_IMG_ADL_TOP0",
	},
	{
		.id = "IMGSYS_CG_IMG_AVS",
	},
	{
		.id = "IMGSYS_CG_IMG_GALS",
	},
	{
		.id = "DIP_TOP_DIP_TOP",
	},
	{
		.id = "DIP_NR1_DIP1_LARB",
	},
	{
		.id = "DIP_NR1_DIP_NR1",
	},
	{
		.id = "DIP_NR2_DIP_NR",
	},
	{
		.id = "WPE1_CG_DIP1_WPE",
	},
	{
		.id = "WPE2_CG_DIP1_WPE",
	},
	{
		.id = "WPE3_CG_DIP1_WPE",
	},
	{
		.id = "TRAW_CG_DIP1_TRAW",
	},
	{
		.id = "IMGSYS_CG_IMG_IPE"
	},
	{
		.id = "ME_CG"
	},
	{
		.id = "MMG_CG"
	}
};

#define MTK_IMGSYS_CLK_NUM	ARRAY_SIZE(imgsys_isp7_clks)

#endif /* _MTK_IMGSYS_PLAT_H_ */
