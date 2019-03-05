/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_GPU_CC_SM6150_H
#define _DT_BINDINGS_CLK_QCOM_GPU_CC_SM6150_H

/* Hardware clocks */
#define CRC_DIV_PLL0_OUT_AUX2			0
#define CRC_DIV_PLL1_OUT_AUX2			1

/* GPUCC clock registers */
#define GPU_CC_PLL0_OUT_AUX2			2
#define GPU_CC_PLL1_OUT_AUX2			3
#define GPU_CC_CRC_AHB_CLK			4
#define GPU_CC_CX_APB_CLK			5
#define GPU_CC_CX_GFX3D_CLK			6
#define GPU_CC_CX_GFX3D_SLV_CLK			7
#define GPU_CC_CX_GMU_CLK			8
#define GPU_CC_CX_SNOC_DVM_CLK			9
#define GPU_CC_CXO_AON_CLK			10
#define GPU_CC_CXO_CLK				11
#define GPU_CC_GMU_CLK_SRC			12
#define GPU_CC_SLEEP_CLK			13
#define GPU_CC_GX_GMU_CLK			14
#define GPU_CC_GX_CXO_CLK			15
#define GPU_CC_GX_GFX3D_CLK			16
#define GPU_CC_GX_GFX3D_CLK_SRC			17
#define GPU_CC_AHB_CLK				18
#define GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK		19

#endif
