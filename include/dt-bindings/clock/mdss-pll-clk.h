/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_PLL_CLK_H
#define __MDSS_PLL_CLK_H

/* DSI PLL clocks */
#define BYTE0_MUX_CLK             0
#define BYTE0_SRC_CLK             1
#define PIX0_MUX_CLK              2
#define PIX0_SRC_CLK              3
#define N2_DIV_0_CLK              4
#define POST_N1_DIV_0_CLK         5
#define VCO_CLK_0_CLK             6
#define SHADOW_BYTE0_SRC_CLK      7
#define SHADOW_PIX0_SRC_CLK       8
#define SHADOW_N2_DIV_0_CLK       9
#define SHADOW_POST_N1_DIV_0_CLK  10
#define SHADOW_VCO_CLK_0_CLK      11
#define BYTE1_MUX_CLK             12
#define BYTE1_SRC_CLK             13
#define PIX1_MUX_CLK              14
#define PIX1_SRC_CLK              15
#define N2_DIV_1_CLK              16
#define POST_N1_DIV_1_CLK         17
#define VCO_CLK_1_CLK             18
#define SHADOW_BYTE1_SRC_CLK      19
#define SHADOW_PIX1_SRC_CLK       20
#define SHADOW_N2_DIV_1_CLK       21
#define SHADOW_POST_N1_DIV_1_CLK  22
#define SHADOW_VCO_CLK_1_CLK      23

/* DP PLL clocks */
#define DP_VCO_CLK                  0
#define DP_LINK_2X_CLK_DIVSEL_FIVE  1
#define DP_VCO_DIVSEL_FOUR_CLK_SRC  2
#define DP_VCO_DIVSEL_TWO_CLK_SRC   3
#define DP_VCO_DIVIDED_CLK_SRC_MUX  4

#endif
