/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_VIDEO_CC_CROW_H
#define _DT_BINDINGS_CLK_QCOM_VIDEO_CC_CROW_H

/* VIDEO_CC clocks */
#define VIDEO_CC_PLL0						0
#define VIDEO_CC_AHB_CLK					1
#define VIDEO_CC_AHB_CLK_SRC					2
#define VIDEO_CC_MVS0_AXI_CLK					3
#define VIDEO_CC_MVS0_CLK					4
#define VIDEO_CC_MVS0_CLK_SRC					5
#define VIDEO_CC_MVS0_SHIFT_CLK					6
#define VIDEO_CC_MVS0C_CLK					7
#define VIDEO_CC_MVS0C_SHIFT_CLK				8
#define VIDEO_CC_MVSC_CTL_AXI_CLK				9
#define VIDEO_CC_SLEEP_CLK					10
#define VIDEO_CC_SLEEP_CLK_SRC					11
#define VIDEO_CC_TRIG_CLK					12
#define VIDEO_CC_VENUS_AHB_CLK					13
#define VIDEO_CC_XO_CLK						14
#define VIDEO_CC_XO_CLK_SRC					15

/* VIDEO_CC resets */
#define VCODEC_VIDEO_CC_INTERFACE_AHB_BCR			0
#define VCODEC_VIDEO_CC_INTERFACE_BCR				1
#define VCODEC_VIDEO_CC_MVS0_BCR				2
#define VCODEC_VIDEO_CC_MVS0C_BCR				3
#define VIDEO_CC_MVS0C_CLK_ARES					4

#endif
