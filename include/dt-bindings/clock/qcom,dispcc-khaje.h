/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_DISP_CC_KHAJE_H
#define _DT_BINDINGS_CLK_QCOM_DISP_CC_KHAJE_H

/* DISP_CC clocks */
#define DISP_CC_MDSS_AHB_CLK					0
#define DISP_CC_MDSS_AHB_CLK_SRC				1
#define DISP_CC_MDSS_BYTE0_CLK					2
#define DISP_CC_MDSS_BYTE0_CLK_SRC				3
#define DISP_CC_MDSS_BYTE0_DIV_CLK_SRC				4
#define DISP_CC_MDSS_BYTE0_INTF_CLK				5
#define DISP_CC_MDSS_ESC0_CLK					6
#define DISP_CC_MDSS_ESC0_CLK_SRC				7
#define DISP_CC_MDSS_MDP_CLK					8
#define DISP_CC_MDSS_MDP_CLK_SRC				9
#define DISP_CC_MDSS_MDP_LUT_CLK				10
#define DISP_CC_MDSS_NON_GDSC_AHB_CLK				11
#define DISP_CC_MDSS_PCLK0_CLK					12
#define DISP_CC_MDSS_PCLK0_CLK_SRC				13
#define DISP_CC_MDSS_ROT_CLK					14
#define DISP_CC_MDSS_ROT_CLK_SRC				15
#define DISP_CC_MDSS_RSCC_AHB_CLK				16
#define DISP_CC_MDSS_RSCC_VSYNC_CLK				17
#define DISP_CC_MDSS_VSYNC_CLK					18
#define DISP_CC_MDSS_VSYNC_CLK_SRC				19
#define DISP_CC_PLL0						20
#define DISP_CC_SLEEP_CLK					21
#define DISP_CC_XO_CLK						22

/* DISP_CC resets */
#define DISP_CC_MDSS_CORE_BCR					0
#define DISP_CC_MDSS_RSCC_BCR					1

#endif
