/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_DISP_CC_RAVELIN_H
#define _DT_BINDINGS_CLK_QCOM_DISP_CC_RAVELIN_H

/* DISP_CC clocks */
#define DISP_CC_PLL0						0
#define DISP_CC_PLL1						1
#define DISP_CC_MDSS_AHB1_CLK					2
#define DISP_CC_MDSS_AHB_CLK					3
#define DISP_CC_MDSS_AHB_CLK_SRC				4
#define DISP_CC_MDSS_BYTE0_CLK					5
#define DISP_CC_MDSS_BYTE0_CLK_SRC				6
#define DISP_CC_MDSS_BYTE0_DIV_CLK_SRC				7
#define DISP_CC_MDSS_BYTE0_INTF_CLK				8
#define DISP_CC_MDSS_ESC0_CLK					9
#define DISP_CC_MDSS_ESC0_CLK_SRC				10
#define DISP_CC_MDSS_MDP1_CLK					11
#define DISP_CC_MDSS_MDP_CLK					12
#define DISP_CC_MDSS_MDP_CLK_SRC				13
#define DISP_CC_MDSS_MDP_LUT1_CLK				14
#define DISP_CC_MDSS_MDP_LUT_CLK				15
#define DISP_CC_MDSS_NON_GDSC_AHB_CLK				16
#define DISP_CC_MDSS_PCLK0_CLK					17
#define DISP_CC_MDSS_PCLK0_CLK_SRC				18
#define DISP_CC_MDSS_ROT1_CLK					19
#define DISP_CC_MDSS_ROT_CLK					20
#define DISP_CC_MDSS_ROT_CLK_SRC				21
#define DISP_CC_MDSS_RSCC_AHB_CLK				22
#define DISP_CC_MDSS_RSCC_VSYNC_CLK				23
#define DISP_CC_MDSS_VSYNC1_CLK					24
#define DISP_CC_MDSS_VSYNC_CLK					25
#define DISP_CC_MDSS_VSYNC_CLK_SRC				26
#define DISP_CC_SLEEP_CLK					27
#define DISP_CC_SLEEP_CLK_SRC					28
#define DISP_CC_XO_CLK						29
#define DISP_CC_XO_CLK_SRC					30

/* DISP_CC resets */
#define DISP_CC_MDSS_CORE_BCR					0
#define DISP_CC_MDSS_CORE_INT2_BCR				1
#define DISP_CC_MDSS_RSCC_BCR					2

#endif
