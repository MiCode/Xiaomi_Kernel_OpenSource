/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_QCOM_VIDEO_CC_SM6150_H
#define _DT_BINDINGS_CLK_QCOM_VIDEO_CC_SM6150_H

/* Hardware clocks*/
#define CHIP_SLEEP_CLK						0

/* VIDEOCC clock registers */
#define VIDEO_PLL0_OUT_MAIN					1
#define VIDEO_CC_APB_CLK					2
#define VIDEO_CC_SLEEP_CLK					3
#define VIDEO_CC_SLEEP_CLK_SRC					4
#define VIDEO_CC_VCODEC0_AXI_CLK				5
#define VIDEO_CC_VCODEC0_CORE_CLK				6
#define VIDEO_CC_VENUS_AHB_CLK					7
#define VIDEO_CC_VENUS_CLK_SRC					8
#define VIDEO_CC_VENUS_CTL_AXI_CLK				9
#define VIDEO_CC_VENUS_CTL_CORE_CLK				10
#define VIDEO_CC_XO_CLK						11
#define VIDEO_CC_XO_CLK_SRC					12

#endif
