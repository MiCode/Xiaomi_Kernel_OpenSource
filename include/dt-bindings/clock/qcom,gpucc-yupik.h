/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_GPU_CC_YUPIK_H
#define _DT_BINDINGS_CLK_QCOM_GPU_CC_YUPIK_H

/* GPU_CC clocks */
#define GPU_CC_PLL0						0
#define GPU_CC_PLL1						1
#define GPU_CC_ACD_AHB_CLK					2
#define GPU_CC_ACD_CXO_CLK					3
#define GPU_CC_AHB_CLK						4
#define GPU_CC_CB_CLK						5
#define GPU_CC_CRC_AHB_CLK					6
#define GPU_CC_CX_APB_CLK					7
#define GPU_CC_CX_GFX3D_CLK					8
#define GPU_CC_CX_GFX3D_SLV_CLK					9
#define GPU_CC_CX_GMU_CLK					10
#define GPU_CC_CX_SNOC_DVM_CLK					11
#define GPU_CC_CXO_AON_CLK					12
#define GPU_CC_CXO_CLK						13
#define GPU_CC_FREQ_MEASURE_CLK					14
#define GPU_CC_GMU_CLK_SRC					15
#define GPU_CC_GX_CXO_CLK					16
#define GPU_CC_GX_GFX3D_CLK					17
#define GPU_CC_GX_GFX3D_CLK_SRC					18
#define GPU_CC_GX_GMU_CLK					19
#define GPU_CC_GX_VSENSE_CLK					20
#define GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK				21
#define GPU_CC_HUB_AHB_DIV_CLK_SRC				22
#define GPU_CC_HUB_AON_CLK					23
#define GPU_CC_HUB_CLK_SRC					24
#define GPU_CC_HUB_CX_INT_CLK					25
#define GPU_CC_HUB_CX_INT_DIV_CLK_SRC				26
#define GPU_CC_MND1X_0_GFX3D_CLK				27
#define GPU_CC_MND1X_1_GFX3D_CLK				28
#define GPU_CC_RBCPR_AHB_CLK					29
#define GPU_CC_RBCPR_CLK					30
#define GPU_CC_RBCPR_CLK_SRC					31
#define GPU_CC_SLEEP_CLK					32

/* GPU_CC power domains */
#define GPU_CC_CX_GDSC						0
#define GPU_CC_GX_GDSC						1

#endif
