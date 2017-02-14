/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_MSM_DISP_CC_SDM845_H
#define _DT_BINDINGS_CLK_MSM_DISP_CC_SDM845_H

#define DISP_CC_DEBUG_CLK					0
#define DISP_CC_MDSS_AHB_CLK					1
#define DISP_CC_MDSS_AXI_CLK					2
#define DISP_CC_MDSS_BYTE0_CLK					3
#define DISP_CC_MDSS_BYTE0_CLK_SRC				4
#define DISP_CC_MDSS_BYTE0_INTF_CLK				5
#define DISP_CC_MDSS_BYTE1_CLK					6
#define DISP_CC_MDSS_BYTE1_CLK_SRC				7
#define DISP_CC_MDSS_BYTE1_INTF_CLK				8
#define DISP_CC_MDSS_DP_AUX_CLK					9
#define DISP_CC_MDSS_DP_AUX_CLK_SRC				10
#define DISP_CC_MDSS_DP_CRYPTO_CLK				11
#define DISP_CC_MDSS_DP_CRYPTO_CLK_SRC				12
#define DISP_CC_MDSS_DP_LINK_CLK				13
#define DISP_CC_MDSS_DP_LINK_CLK_SRC				14
#define DISP_CC_MDSS_DP_LINK_INTF_CLK				15
#define DISP_CC_MDSS_DP_PIXEL1_CLK				16
#define DISP_CC_MDSS_DP_PIXEL1_CLK_SRC				17
#define DISP_CC_MDSS_DP_PIXEL_CLK				18
#define DISP_CC_MDSS_DP_PIXEL_CLK_SRC				19
#define DISP_CC_MDSS_ESC0_CLK					20
#define DISP_CC_MDSS_ESC0_CLK_SRC				21
#define DISP_CC_MDSS_ESC1_CLK					22
#define DISP_CC_MDSS_ESC1_CLK_SRC				23
#define DISP_CC_MDSS_MDP_CLK					24
#define DISP_CC_MDSS_MDP_CLK_SRC				25
#define DISP_CC_MDSS_MDP_LUT_CLK				26
#define DISP_CC_MDSS_PCLK0_CLK					27
#define DISP_CC_MDSS_PCLK0_CLK_SRC				28
#define DISP_CC_MDSS_PCLK1_CLK					29
#define DISP_CC_MDSS_PCLK1_CLK_SRC				30
#define DISP_CC_MDSS_QDSS_AT_CLK				31
#define DISP_CC_MDSS_QDSS_TSCTR_DIV8_CLK			32
#define DISP_CC_MDSS_ROT_CLK					33
#define DISP_CC_MDSS_ROT_CLK_SRC				34
#define DISP_CC_MDSS_RSCC_AHB_CLK				35
#define DISP_CC_MDSS_RSCC_VSYNC_CLK				36
#define DISP_CC_MDSS_SPDM_DEBUG_CLK				37
#define DISP_CC_MDSS_SPDM_DP_CRYPTO_CLK				38
#define DISP_CC_MDSS_SPDM_DP_PIXEL1_CLK				39
#define DISP_CC_MDSS_SPDM_DP_PIXEL_CLK				40
#define DISP_CC_MDSS_SPDM_MDP_CLK				41
#define DISP_CC_MDSS_SPDM_PCLK0_CLK				42
#define DISP_CC_MDSS_SPDM_PCLK1_CLK				43
#define DISP_CC_MDSS_SPDM_ROT_CLK				44
#define DISP_CC_MDSS_VSYNC_CLK					45
#define DISP_CC_MDSS_VSYNC_CLK_SRC				46
#define DISP_CC_PLL0						47
#define DISP_CC_PLL0_OUT_EVEN					48
#define DISP_CC_PLL0_OUT_MAIN					49
#define DISP_CC_PLL0_OUT_ODD					50
#define DISP_CC_PLL0_OUT_TEST					51

#define DISP_CC_DISP_CC_MDSS_CORE_BCR				0
#define DISP_CC_DISP_CC_MDSS_GCC_CLOCKS_BCR			1
#define DISP_CC_DISP_CC_MDSS_RSCC_BCR				2
#define DISP_CC_DISP_CC_MDSS_SPDM_BCR				3

#endif
