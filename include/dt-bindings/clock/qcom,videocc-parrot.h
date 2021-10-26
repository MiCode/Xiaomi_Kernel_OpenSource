/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_VIDEO_CC_PARROT_H
#define _DT_BINDINGS_CLK_QCOM_VIDEO_CC_PARROT_H

/* VIDEO_CC clocks */
#define VIDEO_PLL0				0
#define VIDEO_CC_IRIS_AHB_CLK			1
#define VIDEO_CC_IRIS_CLK_SRC			2
#define VIDEO_CC_MVS0_AXI_CLK			3
#define VIDEO_CC_MVS0_CORE_CLK			4
#define VIDEO_CC_MVSC_CORE_CLK			5
#define VIDEO_CC_MVSC_CTL_AXI_CLK		6
#define VIDEO_CC_SLEEP_CLK			7
#define VIDEO_CC_SLEEP_CLK_SRC			8
#define VIDEO_CC_TRIG_CLK			9
#define VIDEO_CC_VENUS_AHB_CLK			10
#define VIDEO_CC_XO_CLK				11
#define VIDEO_CC_XO_CLK_SRC			12

/* VIDEO_CC resets */
#define VCODEC_VIDEO_CC_INTERFACE_AHB_BCR	0
#define VCODEC_VIDEO_CC_INTERFACE_BCR		1
#define VCODEC_VIDEO_CC_MVS0_BCR		2
#define VCODEC_VIDEO_CC_MVSC_BCR		3

#endif
