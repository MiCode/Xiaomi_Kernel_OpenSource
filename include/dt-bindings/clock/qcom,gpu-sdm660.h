/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017, 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_MSM_GPU_660_H
#define _DT_BINDINGS_CLK_MSM_GPU_660_H

#define GPU_PLL0_PLL						0
#define GPU_PLL0_PLL_OUT_AUX					1
#define GPU_PLL0_PLL_OUT_AUX2					2
#define GPU_PLL0_PLL_OUT_EARLY					3
#define GPU_PLL0_PLL_OUT_MAIN					4
#define GPU_PLL0_PLL_OUT_TEST					5
#define GPU_PLL1_PLL						6
#define GPU_PLL1_PLL_OUT_AUX					7
#define GPU_PLL1_PLL_OUT_AUX2					8
#define GPU_PLL1_PLL_OUT_EARLY					9
#define GPU_PLL1_PLL_OUT_MAIN					10
#define GPU_PLL1_PLL_OUT_TEST					11
#define GFX3D_CLK_SRC						12
#define GPUCC_GFX3D_CLK						13
#define GPUCC_RBBMTIMER_CLK					14
#define RBBMTIMER_CLK_SRC					15
#define GPUCC_CXO_CLK						16

/* RBCPR GPUCC clocks */
#define RBCPR_CLK_SRC						0
#define GPUCC_RBCPR_CLK						1

#define GPU_CX_GDSC						0
#define GPU_GX_GDSC						1

#define GPUCC_GPU_CX_BCR					0
#define GPUCC_GPU_GX_BCR					1
#define GPUCC_RBCPR_BCR						2
#define GPUCC_SPDM_BCR						3

#endif
