/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_MSM_GPU_660_H
#define _DT_BINDINGS_CLK_MSM_GPU_660_H

#define GFX3D_CLK_SRC						0
#define GPU_PLL0_PLL						1
#define GPU_PLL0_PLL_OUT_AUX					2
#define GPU_PLL0_PLL_OUT_AUX2					3
#define GPU_PLL0_PLL_OUT_EARLY					4
#define GPU_PLL0_PLL_OUT_MAIN					5
#define GPU_PLL0_PLL_OUT_TEST					6
#define GPU_PLL1_PLL						7
#define GPU_PLL1_PLL_OUT_AUX					8
#define GPU_PLL1_PLL_OUT_AUX2					9
#define GPU_PLL1_PLL_OUT_EARLY					10
#define GPU_PLL1_PLL_OUT_MAIN					11
#define GPU_PLL1_PLL_OUT_TEST					12
#define GPUCC_CXO_CLK						13
#define GPUCC_GFX3D_CLK						14
#define GPUCC_RBBMTIMER_CLK					15
#define GPUCC_RBCPR_CLK						16
#define RBBMTIMER_CLK_SRC					17
#define RBCPR_CLK_SRC						18

#define GPU_CX_GDSC						0
#define GPU_GX_GDSC						1

#define GPUCC_GPU_CX_BCR					0
#define GPUCC_GPU_GX_BCR					1
#define GPUCC_RBCPR_BCR						2
#define GPUCC_SPDM_BCR						3

#endif
