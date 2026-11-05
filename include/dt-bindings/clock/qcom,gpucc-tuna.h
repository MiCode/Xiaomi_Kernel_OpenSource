/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_GPU_CC_TUNA_H
#define _DT_BINDINGS_CLK_QCOM_GPU_CC_TUNA_H

/* GPU_CC clocks */
#define GPU_CC_PLL0						0
#define GPU_CC_PLL0_OUT_EVEN					1
#define GPU_CC_AHB_CLK						2
#define GPU_CC_CB_CLK						3
#define GPU_CC_CX_ACCU_SHIFT_CLK				4
#define GPU_CC_CX_GMU_CLK					5
#define GPU_CC_CXO_AON_CLK					6
#define GPU_CC_CXO_CLK						7
#define GPU_CC_DEMET_CLK					8
#define GPU_CC_DPM_CLK						9
#define GPU_CC_FF_CLK_SRC					10
#define GPU_CC_FREQ_MEASURE_CLK					11
#define GPU_CC_GMU_CLK_SRC					12
#define GPU_CC_GX_ACCU_SHIFT_CLK				13
#define GPU_CC_GX_GMU_CLK					14
#define GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK				15
#define GPU_CC_HUB_AON_CLK					16
#define GPU_CC_HUB_CLK_SRC					17
#define GPU_CC_HUB_CX_INT_CLK					18
#define GPU_CC_HUB_DIV_CLK_SRC					19
#define GPU_CC_MEMNOC_GFX_CLK					20
#define GPU_CC_RSCC_HUB_AON_CLK					21
#define GPU_CC_RSCC_XO_AON_CLK					22
#define GPU_CC_SLEEP_CLK					23

/* GPU_CC power domains */
#define GPU_CC_CX_GDSC						0
#define GPU_CC_CX_SMMU_GDSC					1
#define GPU_CC_CX_GMU_GDSC					2

/* GX_CLKCTL power domains */
#define GX_CLKCTL_GX_GDSC					0

/* GPU_CC resets */
#define GPU_CC_CB_BCR						0
#define GPU_CC_CX_BCR						1
#define GPU_CC_FAST_HUB_BCR					2
#define GPU_CC_FF_BCR						3
#define GPU_CC_GMU_BCR						4
#define GPU_CC_GX_BCR						5
#define GPU_CC_RBCPR_BCR					6
#define GPU_CC_XO_BCR						7

#endif
