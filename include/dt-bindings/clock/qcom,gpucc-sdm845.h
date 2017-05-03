/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_MSM_GPU_CC_SDM845_H
#define _DT_BINDINGS_CLK_MSM_GPU_CC_SDM845_H

/* GPUCC clock registers */
#define GPU_CC_ACD_AHB_CLK					0
#define GPU_CC_ACD_CXO_CLK					1
#define GPU_CC_AHB_CLK						2
#define GPU_CC_CRC_AHB_CLK					3
#define GPU_CC_CX_APB_CLK					4
#define GPU_CC_CX_GMU_CLK					5
#define GPU_CC_CX_QDSS_AT_CLK					6
#define GPU_CC_CX_QDSS_TRIG_CLK					7
#define GPU_CC_CX_QDSS_TSCTR_CLK				8
#define GPU_CC_CX_SNOC_DVM_CLK					9
#define GPU_CC_CXO_AON_CLK					10
#define GPU_CC_CXO_CLK						11
#define GPU_CC_GX_CXO_CLK					12
#define GPU_CC_GX_GMU_CLK					13
#define GPU_CC_GX_QDSS_TSCTR_CLK				14
#define GPU_CC_GX_VSENSE_CLK					15
#define GPU_CC_PLL0_OUT_MAIN					16
#define GPU_CC_PLL0_OUT_ODD					17
#define GPU_CC_PLL0_OUT_TEST					18
#define GPU_CC_PLL1						19
#define GPU_CC_PLL1_OUT_EVEN					20
#define GPU_CC_PLL1_OUT_MAIN					21
#define GPU_CC_PLL1_OUT_ODD					22
#define GPU_CC_PLL1_OUT_TEST					23
#define GPU_CC_PLL_TEST_CLK					24
#define GPU_CC_RBCPR_AHB_CLK					25
#define GPU_CC_RBCPR_CLK					26
#define GPU_CC_RBCPR_CLK_SRC					27
#define GPU_CC_SLEEP_CLK					28
#define GPU_CC_GMU_CLK_SRC					29
#define GPU_CC_CX_GFX3D_CLK					30
#define GPU_CC_CX_GFX3D_SLV_CLK					31

/* GPUCC reset clock registers */
#define GPUCC_GPU_CC_ACD_BCR					0
#define GPUCC_GPU_CC_CX_BCR					1
#define GPUCC_GPU_CC_GFX3D_AON_BCR				2
#define GPUCC_GPU_CC_GMU_BCR					3
#define GPUCC_GPU_CC_GX_BCR					4
#define GPUCC_GPU_CC_RBCPR_BCR					5
#define GPUCC_GPU_CC_SPDM_BCR					6
#define GPUCC_GPU_CC_XO_BCR					7

/* GFX3D clock registers */
#define GPU_CC_PLL0						0
#define GPU_CC_PLL0_OUT_EVEN					1
#define GPU_CC_GX_GFX3D_CLK_SRC					2
#define GPU_CC_GX_GFX3D_CLK					3
#endif
