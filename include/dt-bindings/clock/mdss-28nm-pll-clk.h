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

#ifndef __MDSS_28NM_PLL_CLK_H
#define __MDSS_28NM_PLL_CLK_H

/* DSI PLL clocks */
#define VCO_CLK_0		0
#define ANALOG_POSTDIV_0_CLK	1
#define INDIRECT_PATH_SRC_0_CLK	2
#define BYTECLK_SRC_MUX_0_CLK	3
#define BYTECLK_SRC_0_CLK	4
#define PCLK_SRC_0_CLK		5
#define VCO_CLK_1		6
#define ANALOG_POSTDIV_1_CLK	7
#define INDIRECT_PATH_SRC_1_CLK	8
#define BYTECLK_SRC_MUX_1_CLK	9
#define BYTECLK_SRC_1_CLK	10
#define PCLK_SRC_1_CLK		11

/* HDMI PLL clocks */
#define HDMI_VCO_CLK			0
#define HDMI_VCO_DIVIDED_1_CLK_SRC	1
#define HDMI_VCO_DIVIDED_TWO_CLK_SRC	2
#define HDMI_VCO_DIVIDED_FOUR_CLK_SRC	3
#define HDMI_VCO_DIVIDED_SIX_CLK_SRC	4
#define HDMI_PCLK_SRC_MUX		5
#define HDMI_PCLK_SRC			6
#endif
