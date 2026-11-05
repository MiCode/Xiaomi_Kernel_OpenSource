/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_VIDEO_CC_SUN_H
#define _DT_BINDINGS_CLK_QCOM_VIDEO_CC_SUN_H

/* VIDEO_CC clocks */
#define VIDEO_CC_AHB_CLK					0
#define VIDEO_CC_AHB_CLK_SRC					1
#define VIDEO_CC_MVS0_CLK					2
#define VIDEO_CC_MVS0_CLK_SRC					3
#define VIDEO_CC_MVS0_DIV_CLK_SRC				4
#define VIDEO_CC_MVS0_FREERUN_CLK				5
#define VIDEO_CC_MVS0_SHIFT_CLK					6
#define VIDEO_CC_MVS0C_CLK					7
#define VIDEO_CC_MVS0C_DIV2_DIV_CLK_SRC				8
#define VIDEO_CC_MVS0C_FREERUN_CLK				9
#define VIDEO_CC_MVS0C_SHIFT_CLK				10
#define VIDEO_CC_PLL0						11
#define VIDEO_CC_SLEEP_CLK					12
#define VIDEO_CC_SLEEP_CLK_SRC					13
#define VIDEO_CC_XO_CLK						14
#define VIDEO_CC_XO_CLK_SRC					15

/* VIDEO_CC power domains */
#define VIDEO_CC_MVS0_GDSC					0
#define VIDEO_CC_MVS0C_GDSC					1

/* VIDEO_CC resets */
#define VIDEO_CC_INTERFACE_BCR					0
#define VIDEO_CC_MVS0_BCR					1
#define VIDEO_CC_MVS0_FREERUN_CLK_ARES				2
#define VIDEO_CC_MVS0C_CLK_ARES					3
#define VIDEO_CC_MVS0C_BCR					4
#define VIDEO_CC_MVS0C_FREERUN_CLK_ARES				5
#define VIDEO_CC_XO_CLK_ARES					6

#define IRIS_VCODEC_VIDEO_CC_INTERFACE_BCR			VIDEO_CC_INTERFACE_BCR
#define IRIS_VCODEC_VIDEO_CC_MVS0_BCR				VIDEO_CC_MVS0_BCR
#define IRIS_VCODEC_VIDEO_CC_MVS0C_BCR				VIDEO_CC_MVS0C_BCR

#endif
