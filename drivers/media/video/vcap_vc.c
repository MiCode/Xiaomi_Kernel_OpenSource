/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <mach/camera.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <mach/clk.h>
#include <linux/clk.h>

#include <media/vcap_v4l2.h>
#include <media/vcap_fmt.h>
#include "vcap_vc.h"

static unsigned debug;

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (debug >= level)					\
			printk(KERN_DEBUG "VC: " fmt, ## arg);		\
	} while (0)

struct v4l2_format_vc_ext vcap_vc_lut[] = {
		/* 1080p */
	{
		HAL_VCAP_YUV_1080p_60_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 148.5,
		32, 2200, 192, 2112, 4, 24, 0, 2, 0, 44, 0, 0, 0, 0,
		0, 0
	},
	{
		HAL_VCAP_YUV_1080p_60_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 148.5,
		1125, 2200, 192, 2112, 41, 1121, 0, 5, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_RGB_1080p_60_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 148.5,
		1125, 2200, 192, 2112, 41, 1121, 0, 5, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_24_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		1125, 2750, 192, 2112, 41, 1121, 0, 5, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_24_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		112, 2750, 192, 2112, 4, 110, 0, 2, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_24_RW, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		1125, 275, 19, 211, 41, 1121, 0, 5, 0, 16, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_60_RW, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 148.5,
		1125, 200, 22, 182, 41, 1121, 0, 5, 0, 16, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_50_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 148.5,
		1125, 2640, 192, 2112, 41, 1121, 0, 5, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_50_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 148.5,
		15, 2640, 192, 2112, 6, 13, 0, 5, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_25_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		1125, 2640, 192, 2112, 41, 1121, 0, 5, 0, 44, 0, 0,
		0, 0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_25_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		10, 2640, 192, 2112, 4, 8, 0, 2, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1080p_30_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		1125, 2200, 192, 2112, 41, 1121, 0, 5, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_RGB_1080p_25_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 74.25,
		1125, 2640, 192, 2112, 41, 1121, 0, 5, 0, 44, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_RGB_1080p_25_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 74.25,
		10, 2640, 192, 2112, 4, 8, 0, 2, 0, 44, 0, 0, 0,
		0, 0, 0
	},
		/* 1080i */
	{
		HAL_VCAP_YUV_1080i_60_FL, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		1125, 2200, 192, 2112, 20, 560, 0, 5, 0, 44, 583, 1123, 1100,
		1100, 563, 568
	},
	{
		HAL_VCAP_YUV_1080i_60_RH, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		18, 2200, 192, 2112, 3, 7, 0, 2, 0, 44, 11, 15, 1100,
		1100, 8, 10
	},
	{
		HAL_VCAP_YUV_1080i_60_RW, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		1125, 220, 19, 211, 20, 560, 0, 5, 0, 4, 583, 1123, 110,
		110, 563, 568
	},
	{
		HAL_VCAP_YUV_1080i_50_FL, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 72.00,
		1125, 2640, 192, 2112, 20, 560, 0, 5, 0, 44, 583, 1123, 1320,
		1320, 563, 568
	},
	{
		HAL_VCAP_YUV_1080i_50_RH, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 72.00,
		52, 2640, 192, 2112, 4, 24, 0, 2, 0, 44, 30, 50, 1320,
		1320, 26, 28},
	{
		HAL_VCAP_YUV_1080i_50_RW, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 72.00,
		1125, 264, 19, 211, 20, 560, 0, 5, 0, 4, 583, 1123, 110,
		110, 563, 568
	},
	{
		HAL_VCAP_RGB_1080i_50_FL, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 72.00,
		1125, 2640, 192, 2112, 20, 560, 0, 5, 0, 44, 583, 1123, 1320,
		1320, 563, 568
	},
	{
		HAL_VCAP_RGB_1080i_50_RH, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 72.00,
		52, 2640, 192, 2112, 4, 24, 0, 2, 0, 44, 30, 50, 1320,
		1320, 26, 28
	},
		/* 480i */
	{
		HAL_VCAP_YUV_480i_60_RH, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.027,
		20, 1716, 238, 1678, 3, 7, 0, 2, 0, 124, 14, 18, 820,
		820, 10, 12
	},
	{
		HAL_VCAP_YUV_480i_60_FL, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.027,
		525, 1716, 238, 1678, 18, 258, 0, 3, 0, 124, 281, 521, 858,
		858, 262, 265
	},
	{
		HAL_VCAP_YUV_480i_60_RW, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.027,
		525, 172, 24, 168, 18, 258, 0, 3, 0, 12, 281, 521, 86,
		86, 262, 265
	},
	{
		HAL_VCAP_YUV_2880_480i_60_FL, HAL_VCAP_MODE_INT,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS,
		HAL_VCAP_YUV, 54.000, 525, 3432, 476, 3356, 18, 258, 0, 3,
		0, 248, 281, 521, 1716, 1716, 262, 265
	},
	{
		HAL_VCAP_YUV_2880_480i_60_RH, HAL_VCAP_MODE_INT,
		HAL_VCAP_POLAR_NEG,	HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS,
		HAL_VCAP_YUV, 54.000, 32, 3432, 476, 3356, 4, 14, 0, 3, 0,
		248, 20, 30, 1716, 1716, 16, 19
	},
		/* 480p */
	{
		HAL_VCAP_YUV_480p_60_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.027,
		8, 858, 122, 842, 2, 5, 0, 1, 0, 62, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_RGB_480p_60_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 27.027,
		52, 858, 122, 842, 3, 50, 0, 2, 0, 62, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_RGB_480p_60_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 27.027,
		525, 858, 122, 842, 36, 516, 0, 6, 0, 62, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_480p_60_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.027,
		525, 858, 122, 842, 36, 516, 0, 6, 0, 62, 0, 0, 0, 0, 0, 0
	},
	{
		HAL_VCAP_YUV_480p_60_RW, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.027,
		525, 86, 12, 84, 36, 516, 0, 6, 0, 6, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_2880_480p_60_FL, HAL_VCAP_MODE_PRO,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS,
		HAL_VCAP_YUV, 108.000, 525, 3432, 488, 3368, 36, 516, 0, 6,
		0, 248, 0, 0, 0, 0, 0, 0
	},
	{
		HAL_VCAP_YUV_2880_480p_60_RH, HAL_VCAP_MODE_PRO,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS,
		HAL_VCAP_YUV, 108.000, 25, 3432, 488, 3368, 8, 22, 0, 6, 0,
		248, 0, 0, 0,	0, 0, 0
	},
		/* 720p */
	{
		HAL_VCAP_YUV_720p_60_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		750, 1650, 260, 1540, 25, 745, 0, 5, 0, 40, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_RGB_720p_60_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 74.25,
		750, 1650, 260, 1540, 25, 745, 0, 5, 0, 40, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_720p_60_RW, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		750, 165, 26, 154, 25, 745, 0, 5, 0, 4, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_720p_60_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		35, 1650, 260, 1540, 5, 32, 0, 3, 0, 40, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_720p_50_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		750, 1980, 260, 1540, 25, 745, 0, 5, 0, 40, 0, 0, 0,
		 0, 0, 0
	},
	{
		HAL_VCAP_YUV_720p_50_RW, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		750, 198, 26, 154, 25, 745, 0, 5, 0, 4, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_720p_50_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 74.25,
		6, 1980, 260, 1540, 2, 5, 0, 1, 0, 40, 0, 0, 0,
		0, 0, 0
	},
		/* 576p */
	{
		HAL_VCAP_YUV_576p_50_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.0,
		625, 864, 132, 852, 44, 620, 0, 5, 0, 64, 0, 0, 0,
		0, 0, 0},
	{
		HAL_VCAP_RGB_576p_50_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 27.0,
		625, 864, 132, 852, 44, 620, 0, 5, 0, 64, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_576p_50_RW, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.0,
		625, 86, 13, 85, 44, 620, 0, 5, 0, 6, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_576p_50_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.0,
		25, 864, 132, 852, 4, 23, 0, 3, 0, 64, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_1440_576p_50_RH, HAL_VCAP_MODE_PRO,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS,
		HAL_VCAP_YUV, 54.000, 25, 1728, 264, 1704, 6, 23, 0, 5, 0,
		128, 0, 0, 0,	0, 0, 0
	},
	{
		HAL_VCAP_YUV_2880_576p_50_FL, HAL_VCAP_MODE_PRO,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS,
		HAL_VCAP_YUV, 108.000, 625, 3456, 528, 3408, 44, 620, 0, 5,
		0, 256, 0, 0, 0, 0, 0, 0
	},
	{
		HAL_VCAP_YUV_2880_576p_50_RH, HAL_VCAP_MODE_PRO,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS,
		HAL_VCAP_YUV, 108.000, 25, 3456, 528, 3408, 6, 23, 0, 5, 0,
		256, 0, 0, 0, 0, 0, 0
	},
		/* 576i */
	{
		HAL_VCAP_YUV_576i_50_FL, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.0,
		625, 1728, 264, 1704, 22, 310, 0, 3, 0, 126, 335, 623, 864,
		864, 313, 316
	},
	{
		HAL_VCAP_YUV_576i_50_RW, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.0,
		625, 172, 26, 170, 22, 310, 0, 3, 0, 13, 335, 623, 86,
		86, 313, 316
	},
	{
		HAL_VCAP_YUV_576i_50_RH, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_NEG, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 27.0,
		29, 1728, 264, 1704, 3, 13, 0, 1, 0, 126, 16, 26, 864, 864,
		14, 15
	},
		/* XGA 1024x768 */
	{
		HAL_VCAP_YUV_XGA_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 63.5,
		798, 1328, 256, 1280, 27, 795, 0, 4, 0, 104, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_XGA_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 63.5,
		12, 1328, 256, 1280, 6, 10, 0, 4, 0, 104, 0, 0, 0, 0,
		0, 0
	},
	{
		HAL_VCAP_YUV_XGA_RB, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 63.5,
		12, 1216, 112, 1136, 6, 10, 0, 4, 0, 32, 0, 0, 0, 0,
		0, 0
	},
		/* SXGA 1280x1024 */
	{
		HAL_VCAP_YUV_SXGA_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 109.0,
		1063, 1712, 352, 1632, 36, 1060, 0, 7, 0, 136, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_RGB_SXGA_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 109.0,
		1063, 1712, 352, 1632, 36, 1060, 0, 7, 0, 136, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_SXGA_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 109.0,
		17, 1712, 352, 1632, 8, 15, 0, 7, 0, 136, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_SXGA_RB, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 109.0,
		17, 1440, 112, 1392, 8, 15, 0, 7, 0, 32, 0, 0, 0, 0,
		0, 0
	},
		/* UXGA 1600x1200 */
	{
		HAL_VCAP_YUV_UXGA_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 161.0,
		1245, 2160, 448, 2048, 42, 1242, 0, 4, 0, 168, 0,
		0, 0, 0, 0, 0
	},
	{
		HAL_VCAP_RGB_UXGA_FL, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_RGB, 161.0,
		1245, 2160, 448, 2048, 42, 1242, 0, 4, 0, 168, 0,
		0, 0, 0, 0, 0
	},
	{
		HAL_VCAP_YUV_UXGA_RH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 161.0,
		12, 2160, 448, 2048, 6, 10, 0, 4, 0, 168, 0, 0, 0,
		0, 0, 0
	},
	{
		HAL_VCAP_YUV_UXGA_RB, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_NEG,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_POS, HAL_VCAP_YUV, 161.0,
		12, 1808, 112, 1712, 6, 10, 0, 4, 0, 32, 0, 0, 0, 0,
		0, 0
	},
		/* test odd height */
	{
		HAL_VCAP_ODD_HEIGHT, HAL_VCAP_MODE_INT, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_NEG, HAL_VCAP_YUV, 148.5,
		65, 1728, 264, 1704, 5, 20, 0, 3, 0, 126, 25, 40, 864,
		864, 21, 24
	},
		/* test odd width RGB only */
	{
		HAL_VCAP_ODD_WIDTH, HAL_VCAP_MODE_PRO, HAL_VCAP_POLAR_POS,
		HAL_VCAP_POLAR_POS, HAL_VCAP_POLAR_NEG, HAL_VCAP_RGB, 148.5,
		52, 859, 122, 843, 3, 50, 0, 2, 0, 62, 0, 0, 0, 0, 0, 0
	},
};

void config_buffer(struct vcap_client_data *c_data,
			struct vcap_buffer *buf,
			void __iomem *y_addr,
			void __iomem *c_addr)
{
	if (c_data->vc_format.color_space == HAL_VCAP_RGB) {
		writel_relaxed(buf->paddr, y_addr);
	} else {
		int size = ((c_data->vc_format.hactive_end -
				c_data->vc_format.hactive_start) *
				(c_data->vc_format.vactive_end -
				c_data->vc_format.vactive_start));
		writel_relaxed(buf->paddr, y_addr);
		writel_relaxed(buf->paddr + size, c_addr);
	}
}

irqreturn_t vc_handler(struct vcap_dev *dev)
{
	uint32_t irq, timestamp;
	enum rdy_buf vc_buf_status, buf_ind;
	struct vcap_buffer *buf;
	struct vb2_buffer *vb = NULL;
	struct vcap_client_data *c_data;

	irq = readl_relaxed(VCAP_VC_INT_STATUS);

	dprintk(1, "%s: irq=0x%08x\n", __func__, irq);

	vc_buf_status = irq & VC_BUFFER_WRITTEN;

	dprintk(1, "Done buf status = %d\n", vc_buf_status);

	if (vc_buf_status == VC_NO_BUF) {
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		pr_err("VC IRQ shows some error\n");
		return IRQ_HANDLED;
	}

	if (dev->vc_client == NULL) {
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		pr_err("VC: There is no active vc client\n");
		return IRQ_HANDLED;
	}
	c_data = dev->vc_client;

	spin_lock(&dev->vc_client->cap_slock);
	if (list_empty(&dev->vc_client->vid_vc_action.active)) {
		/* Just leave we have no new queued buffers */
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		spin_unlock(&dev->vc_client->cap_slock);
		dprintk(1, "We have no more avilable buffers\n");
		return IRQ_HANDLED;
	}
	spin_unlock(&dev->vc_client->cap_slock);

	timestamp = readl_relaxed(VCAP_VC_TIMESTAMP);

	buf_ind = dev->vc_client->vid_vc_action.buf_ind;

	if (vc_buf_status == VC_BUF1N2) {
		/* There are 2 buffer ready */
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		return IRQ_HANDLED;
	} else if (buf_ind != vc_buf_status) {
		/* buffer is out of sync */
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		return IRQ_HANDLED;
	}

	if (buf_ind == VC_BUF1) {
		dprintk(1, "Got BUF1\n");
		vb = &dev->vc_client->vid_vc_action.buf1->vb;
		spin_lock(&dev->vc_client->cap_slock);
		if (list_empty(&dev->vc_client->vid_vc_action.active)) {
			spin_unlock(&dev->vc_client->cap_slock);
			writel_relaxed(irq, VCAP_VC_INT_CLEAR);
			return IRQ_HANDLED;
		}
		buf = list_entry(dev->vc_client->vid_vc_action.active.next,
				struct vcap_buffer, list);
		list_del(&buf->list);
		spin_unlock(&dev->vc_client->cap_slock);
		/* Config vc with this new buffer */
		config_buffer(c_data, buf, VCAP_VC_Y_ADDR_1,
				VCAP_VC_C_ADDR_1);

		vb->v4l2_buf.timestamp.tv_usec = timestamp;
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		dev->vc_client->vid_vc_action.buf1 = buf;
		dev->vc_client->vid_vc_action.buf_ind = VC_BUF2;
		irq = VC_BUF1;
	} else {
		dprintk(1, "Got BUF2\n");
		spin_lock(&dev->vc_client->cap_slock);
		vb = &dev->vc_client->vid_vc_action.buf2->vb;
		if (list_empty(&dev->vc_client->vid_vc_action.active)) {
			writel_relaxed(irq, VCAP_VC_INT_CLEAR);
			spin_unlock(&dev->vc_client->cap_slock);
			return IRQ_HANDLED;
		}
		buf = list_entry(dev->vc_client->vid_vc_action.active.next,
						 struct vcap_buffer, list);
		list_del(&buf->list);
		spin_unlock(&dev->vc_client->cap_slock);
		/* Config vc with this new buffer */
		config_buffer(c_data, buf, VCAP_VC_Y_ADDR_2,
				VCAP_VC_C_ADDR_2);

		vb->v4l2_buf.timestamp.tv_usec = timestamp;
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);

		dev->vc_client->vid_vc_action.buf2 = buf;
		dev->vc_client->vid_vc_action.buf_ind = VC_BUF1;
		irq = VC_BUF2;
	}
	writel_relaxed(irq, VCAP_VC_INT_CLEAR);

	return IRQ_HANDLED;
}

int vc_start_capture(struct vcap_client_data *c_data)
{
	return 0;
}

int vc_hw_kick_off(struct vcap_client_data *c_data)
{
	struct vcap_action *vid_vc_action = &c_data->vid_vc_action;
	struct vcap_dev *dev;
	unsigned long flags = 0;
	int rc, counter = 0;
	struct vcap_buffer *buf;

	dev = c_data->dev;
	vid_vc_action->buf_ind = VC_BUF1;
	dprintk(2, "Start Kickoff\n");

	if (dev->vc_client == NULL) {
		pr_err("No active vc client\n");
		return -ENODEV;
	}
	spin_lock_irqsave(&dev->vc_client->cap_slock, flags);
	if (list_empty(&dev->vc_client->vid_vc_action.active)) {
		spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);
		pr_err("%s: VC We have no more avilable buffers\n",
				__func__);
		return -EINVAL;
	}

	list_for_each_entry(buf, &vid_vc_action->active, list)
		counter++;

	if (counter < 2) {
		/* not enough buffers have been queued */
		spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);
		return -EINVAL;
	}

	vid_vc_action->buf1 = list_entry(vid_vc_action->active.next,
			struct vcap_buffer, list);
	list_del(&vid_vc_action->buf1->list);

	vid_vc_action->buf2 = list_entry(vid_vc_action->active.next,
			struct vcap_buffer, list);
	list_del(&vid_vc_action->buf2->list);

	spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);

	config_buffer(c_data, vid_vc_action->buf1, VCAP_VC_Y_ADDR_1,
			VCAP_VC_C_ADDR_1);
	config_buffer(c_data, vid_vc_action->buf2, VCAP_VC_Y_ADDR_2,
			VCAP_VC_C_ADDR_2);

	rc = readl_relaxed(VCAP_VC_CTRL);
	writel_relaxed(rc | 0x1, VCAP_VC_CTRL);

	writel_relaxed(0x6, VCAP_VC_INT_MASK);

	enable_irq(dev->vcapirq->start);
	return 0;
}

void vc_stop_capture(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	int rc;

	rc = readl_relaxed(VCAP_VC_CTRL);
	writel_relaxed(rc & ~(0x1), VCAP_VC_CTRL);

	disable_irq(c_data->dev->vcapirq->start);
}

int config_vc_format(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev;
	unsigned int rc;
	int timeout;
	struct v4l2_format_vc_ext *vc_format = &c_data->vc_format;
	dev = c_data->dev;

	/* restart VC */
	writel_relaxed(0x00000001, VCAP_SW_RESET_REQ);
	timeout = 10000;
	while (1) {
		rc = (readl_relaxed(VCAP_SW_RESET_STATUS) & 0x1);
		if (!rc)
			break;
		timeout--;
		if (timeout == 0) {
			pr_err("VC is not resetting properly\n");
			return -EINVAL;
		}
	}
	writel_relaxed(0x00000000, VCAP_SW_RESET_REQ);

	writel_relaxed(0x00000102, VCAP_VC_NPL_CTRL);
	rc = readl_relaxed(VCAP_VC_NPL_CTRL);
	rc = readl_relaxed(VCAP_VC_NPL_CTRL);
	writel_relaxed(0x00000002, VCAP_VC_NPL_CTRL);

	dprintk(2, "%s: Starting VC configuration\n", __func__);
	writel_relaxed(0x00000002, VCAP_VC_NPL_CTRL);
	writel_relaxed(0x00000004 | vc_format->color_space << 1, VCAP_VC_CTRL);

	writel_relaxed(vc_format->h_polar << 4 |
			vc_format->v_polar << 0, VCAP_VC_POLARITY);

	writel_relaxed(vc_format->h_polar << 4 |
			vc_format->v_polar << 0, VCAP_VC_POLARITY);
	writel_relaxed(((vc_format->htotal << 16) | vc_format->vtotal),
			VCAP_VC_V_H_TOTAL);
	writel_relaxed(((vc_format->hactive_end << 16) |
			vc_format->hactive_start), VCAP_VC_H_ACTIVE);

	writel_relaxed(((vc_format->vactive_end << 16) |
			vc_format->vactive_start), VCAP_VC_V_ACTIVE);
	writel_relaxed(((vc_format->f2_vactive_end << 16) |
			vc_format->f2_vactive_start), VCAP_VC_V_ACTIVE_F2);
	writel_relaxed(((vc_format->vsync_end << 16) | vc_format->vsync_start),
			VCAP_VC_VSYNC_VPOS);
	writel_relaxed(((vc_format->f2_vsync_v_end << 16) |
			vc_format->f2_vsync_v_start), VCAP_VC_VSYNC_F2_VPOS);
	writel_relaxed(((vc_format->hsync_end << 16) |
			vc_format->hsync_start), VCAP_VC_HSYNC_HPOS);
	writel_relaxed(((vc_format->f2_vsync_h_end << 16) |
			vc_format->f2_vsync_h_start), VCAP_VC_VSYNC_F2_HPOS);
	writel_relaxed(0x000033FF, VCAP_VC_BUF_CTRL);

	rc = vc_format->hactive_end - vc_format->hactive_start;
	if (vc_format->color_space)
		rc *= 3;

	writel_relaxed(rc, VCAP_VC_Y_STRIDE);
	writel_relaxed(rc, VCAP_VC_C_STRIDE);

	writel_relaxed(0x00010033 , VCAP_OFFSET(0x0898));
	writel_relaxed(0x00010fff , VCAP_OFFSET(0x089c));
	writel_relaxed(0x0a418820, VCAP_VC_IN_CTRL1);
	writel_relaxed(0x16a4a0e6, VCAP_VC_IN_CTRL2);
	writel_relaxed(0x2307b9ac, VCAP_VC_IN_CTRL3);
	writel_relaxed(0x2f6ad272, VCAP_VC_IN_CTRL4);
	writel_relaxed(0x00006b38, VCAP_VC_IN_CTRL5);

	dprintk(2, "%s: Done VC configuration\n", __func__);

	return 0;
}

int detect_vc(struct vcap_dev *dev)
{
	int result;
	result = readl_relaxed(VCAP_HARDWARE_VERSION_REG);
	dprintk(1, "Hardware version: %08x\n", result);
	if (result != VCAP_HARDWARE_VERSION)
		return -ENODEV;
	return 0;
}

int deinit_vc(void)
{
	return 0;
}
