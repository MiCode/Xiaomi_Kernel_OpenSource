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

#ifndef _DT_BINDINGS_CLK_MSM_GPU_FALCON_H
#define _DT_BINDINGS_CLK_MSM_GPU_FALCON_H

/* Clocks */
#define GPU_PLL0_PLL			0
#define GPU_PLL1_PLL			1
#define GFX3D_CLK_SRC			2
#define RBBMTIMER_CLK_SRC		3
#define RBCPR_CLK_SRC			4
#define GPUCC_CXO_CLK			5
#define GPUCC_GFX3D_CLK			6
#define GPUCC_RBBMTIMER_CLK		7
#define GPUCC_RBCPR_CLK			8

/* Block Reset */
#define GPU_CC_GPU_GX_BCR		0
#define GPU_CC_GPU_CX_BCR		1
#define GPU_CC_RBCPR_BCR		2
#define GPU_CC_SPDM_BCR			3

/* GDSC */
#define GPU_GX_GDSC			0
#define GPU_CX_GDSC			1

#endif
