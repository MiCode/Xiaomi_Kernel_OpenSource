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
		.id = "IMGSYS_CG_IMG_TRAW2",
	},
	{
		.id = "IMGSYS_CG_IMG_TRAW3",
	},
	{
		.id = "IMGSYS_CG_IMG_DIP0",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE0",
	},
	{
		.id = "IMGSYS_CG_IMG_DIP1",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE1",
	},
	{
		.id = "DIP_CG_IMG_DIP",
	},
	{
		.id = "DIP_NR_DIP_NR",
	},
	{
		.id = "WPE_CG_WPE_WPE",
	},
  // TBD: ADL clock
	//{
	//	.id = "IMGSYS_CG_IMG_ADL",
	//},
};

#define MTK_IMGSYS_CLK_NUM	ARRAY_SIZE(imgsys_isp7_clks)

#endif /* _MTK_IMGSYS_PLAT_H_ */
