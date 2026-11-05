/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_EVA_CC_SUN_H
#define _DT_BINDINGS_CLK_QCOM_EVA_CC_SUN_H

/* EVA_CC clocks */
#define EVA_CC_AHB_CLK						0
#define EVA_CC_AHB_CLK_SRC					1
#define EVA_CC_MVS0_CLK						2
#define EVA_CC_MVS0_CLK_SRC					3
#define EVA_CC_MVS0_DIV_CLK_SRC					4
#define EVA_CC_MVS0_FREERUN_CLK					5
#define EVA_CC_MVS0_SHIFT_CLK					6
#define EVA_CC_MVS0C_CLK					7
#define EVA_CC_MVS0C_DIV2_DIV_CLK_SRC				8
#define EVA_CC_MVS0C_FREERUN_CLK				9
#define EVA_CC_MVS0C_SHIFT_CLK					10
#define EVA_CC_PLL0						11
#define EVA_CC_SLEEP_CLK					12
#define EVA_CC_SLEEP_CLK_SRC					13
#define EVA_CC_XO_CLK						14
#define EVA_CC_XO_CLK_SRC					15

/* EVA_CC power domains */
#define EVA_CC_MVS0_GDSC					0
#define EVA_CC_MVS0C_GDSC					1

/* EVA_CC resets */
#define EVA_CC_INTERFACE_BCR					0
#define EVA_CC_MVS0_BCR						1
#define EVA_CC_MVS0_FREERUN_CLK_ARES				2
#define EVA_CC_MVS0C_CLK_ARES					3
#define EVA_CC_MVS0C_BCR					4
#define EVA_CC_MVS0C_FREERUN_CLK_ARES				5

#define EVA_CVP_EVA_CC_INTERFACE_BCR				EVA_CC_INTERFACE_BCR
#define EVA_CVP_EVA_CC_MVS0_BCR					EVA_CC_MVS0_BCR
#define EVA_CVP_EVA_CC_MVS0C_BCR				EVA_CC_MVS0C_BCR

#endif
