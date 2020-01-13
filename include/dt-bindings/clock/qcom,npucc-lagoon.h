/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_NPU_CC_LAGOON_H
#define _DT_BINDINGS_CLK_QCOM_NPU_CC_LAGOON_H

/* NPU_CC clocks */
#define NPU_CC_ATB_CLK						0
#define NPU_CC_BTO_CORE_CLK					1
#define NPU_CC_BWMON_CLK					2
#define NPU_CC_CAL_HM0_CDC_CLK					3
#define NPU_CC_CAL_HM0_CLK					4
#define NPU_CC_CAL_HM0_CLK_SRC					5
#define NPU_CC_CAL_HM0_PERF_CNT_CLK				6
#define NPU_CC_CORE_CLK					7
#define NPU_CC_CORE_CLK_SRC					8
#define NPU_CC_DSP_AHBM_CLK					9
#define NPU_CC_DSP_AHBS_CLK					10
#define NPU_CC_DSP_AXI_CLK					11
#define NPU_CC_NOC_AHB_CLK					12
#define NPU_CC_NOC_AXI_CLK					13
#define NPU_CC_NOC_DMA_CLK					14
#define NPU_CC_PLL0						15
#define NPU_CC_PLL1						16
#define NPU_CC_RSC_XO_CLK					17
#define NPU_CC_S2P_CLK						18
#define NPU_CC_XO_CLK						19
#define NPU_CC_XO_CLK_SRC					20
#define NPU_DSP_CORE_CLK_SRC					21
#define NPU_Q6SS_PLL						22

/* NPU_CC resets */
#define NPU_CC_CAL_HM0_BCR		0
#define NPU_CC_CORE_BCR			1
#define NPU_CC_DSP_BCR			2

#endif
