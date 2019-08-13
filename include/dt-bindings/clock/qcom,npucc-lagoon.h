/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_NPU_CC_LAGOON_H
#define _DT_BINDINGS_CLK_QCOM_NPU_CC_LAGOON_H

/* NPU_CC clocks */
#define NPU_CC_PLL0			0
#define NPU_CC_PLL0_OUT_EVEN		1
#define NPU_CC_PLL1			2
#define NPU_CC_PLL1_OUT_EVEN		3
#define NPU_CC_BTO_CORE_CLK		5
#define NPU_CC_BWMON_CLK		6
#define NPU_CC_CAL_HM0_CDC_CLK		7
#define NPU_CC_CAL_HM0_CLK		8
#define NPU_CC_CAL_HM0_CLK_SRC		9
#define NPU_CC_CAL_HM0_PERF_CNT_CLK	10
#define NPU_CC_CORE_CLK			11
#define NPU_CC_CORE_CLK_SRC		12
#define NPU_CC_DSP_AHBM_CLK		13
#define NPU_CC_DSP_AHBS_CLK		14
#define NPU_CC_DSP_AXI_CLK		15
#define NPU_CC_NOC_AHB_CLK		16
#define NPU_CC_NOC_AXI_CLK		17
#define NPU_CC_NOC_DMA_CLK		18
#define NPU_CC_RSC_XO_CLK		19
#define NPU_CC_S2P_CLK			20
#define NPU_CC_XO_CLK			21
#define NPU_CC_XO_CLK_SRC		22
#define NPU_DSP_CORE_CLK_SRC		23
#define NPU_Q6SS_PLL			24

/* NPU_CC resets */
#define NPU_CC_CAL_HM0_BCR		0
#define NPU_CC_CORE_BCR			1
#define NPU_CC_DSP_BCR			2

#endif
