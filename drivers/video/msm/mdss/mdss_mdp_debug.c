/*
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/seq_file.h>

#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_mdp_debug.h"

#define BUF_DUMP_LAST_N 10

static struct debug_bus dbg_bus_8996[] = {

	/*
	 * sspp0  - 0x188
	 * sspp1  - 0x298
	 * dspp   - 0x348
	 * periph - 0x418
	 */

	/* Unpack 0 sspp 0*/
	{ 0x188, 50, 2 },
	{ 0x188, 60, 2 },
	{ 0x188, 54, 2 },
	{ 0x188, 64, 2 },
	{ 0x188, 70, 2 },
	{ 0x188, 85, 2 },
	/* Upack 0 sspp 1*/
	{ 0x298, 50, 2 },
	{ 0x298, 60, 2 },
	{ 0x298, 54, 2 },
	{ 0x298, 64, 2 },
	{ 0x298, 70, 2 },
	{ 0x298, 85, 2 },
	/* scheduler */
	{ 0x348, 130, 0 },
	{ 0x348, 130, 1 },
	{ 0x348, 130, 2 },
	{ 0x348, 130, 3 },
	{ 0x348, 130, 4 },
	{ 0x348, 130, 5 },

	/* qseed */
	{0x188, 6, 0},
	{0x188, 6, 1},
	{0x188, 26, 0},
	{0x188, 26, 1},
	{0x298, 6, 0},
	{0x298, 6, 1},
	{0x298, 26, 0},
	{0x298, 26, 1},

	/* scale */
	{0x188, 16, 0},
	{0x188, 16, 1},
	{0x188, 36, 0},
	{0x188, 36, 1},
	{0x298, 16, 0},
	{0x298, 16, 1},
	{0x298, 36, 0},
	{0x298, 36, 1},

	/* fetch sspp0 */

	/* vig 0 */
	{ 0x188, 0, 0 },
	{ 0x188, 0, 1 },
	{ 0x188, 0, 2 },
	{ 0x188, 0, 3 },
	{ 0x188, 0, 4 },
	{ 0x188, 0, 5 },
	{ 0x188, 0, 6 },
	{ 0x188, 0, 7 },

	{ 0x188, 1, 0 },
	{ 0x188, 1, 1 },
	{ 0x188, 1, 2 },
	{ 0x188, 1, 3 },
	{ 0x188, 1, 4 },
	{ 0x188, 1, 5 },
	{ 0x188, 1, 6 },
	{ 0x188, 1, 7 },

	{ 0x188, 2, 0 },
	{ 0x188, 2, 1 },
	{ 0x188, 2, 2 },
	{ 0x188, 2, 3 },
	{ 0x188, 2, 4 },
	{ 0x188, 2, 5 },
	{ 0x188, 2, 6 },
	{ 0x188, 2, 7 },

	{ 0x188, 4, 0 },
	{ 0x188, 4, 1 },
	{ 0x188, 4, 2 },
	{ 0x188, 4, 3 },
	{ 0x188, 4, 4 },
	{ 0x188, 4, 5 },
	{ 0x188, 4, 6 },
	{ 0x188, 4, 7 },

	{ 0x188, 5, 0 },
	{ 0x188, 5, 1 },
	{ 0x188, 5, 2 },
	{ 0x188, 5, 3 },
	{ 0x188, 5, 4 },
	{ 0x188, 5, 5 },
	{ 0x188, 5, 6 },
	{ 0x188, 5, 7 },

	/* vig 2 */
	{ 0x188, 20, 0 },
	{ 0x188, 20, 1 },
	{ 0x188, 20, 2 },
	{ 0x188, 20, 3 },
	{ 0x188, 20, 4 },
	{ 0x188, 20, 5 },
	{ 0x188, 20, 6 },
	{ 0x188, 20, 7 },

	{ 0x188, 21, 0 },
	{ 0x188, 21, 1 },
	{ 0x188, 21, 2 },
	{ 0x188, 21, 3 },
	{ 0x188, 21, 4 },
	{ 0x188, 21, 5 },
	{ 0x188, 21, 6 },
	{ 0x188, 21, 7 },

	{ 0x188, 22, 0 },
	{ 0x188, 22, 1 },
	{ 0x188, 22, 2 },
	{ 0x188, 22, 3 },
	{ 0x188, 22, 4 },
	{ 0x188, 22, 5 },
	{ 0x188, 22, 6 },
	{ 0x188, 22, 7 },

	{ 0x188, 24, 0 },
	{ 0x188, 24, 1 },
	{ 0x188, 24, 2 },
	{ 0x188, 24, 3 },
	{ 0x188, 24, 4 },
	{ 0x188, 24, 5 },
	{ 0x188, 24, 6 },
	{ 0x188, 24, 7 },

	{ 0x188, 25, 0 },
	{ 0x188, 25, 1 },
	{ 0x188, 25, 2 },
	{ 0x188, 25, 3 },
	{ 0x188, 25, 4 },
	{ 0x188, 25, 5 },
	{ 0x188, 25, 6 },
	{ 0x188, 25, 7 },

	/* rgb 0 */
	{ 0x188, 10, 0 },
	{ 0x188, 10, 1 },
	{ 0x188, 10, 2 },
	{ 0x188, 10, 3 },
	{ 0x188, 10, 4 },
	{ 0x188, 10, 5 },
	{ 0x188, 10, 6 },
	{ 0x188, 10, 7 },

	{ 0x188, 11, 0 },
	{ 0x188, 11, 1 },
	{ 0x188, 11, 2 },
	{ 0x188, 11, 3 },
	{ 0x188, 11, 4 },
	{ 0x188, 11, 5 },
	{ 0x188, 11, 6 },
	{ 0x188, 11, 7 },

	{ 0x188, 12, 0 },
	{ 0x188, 12, 1 },
	{ 0x188, 12, 2 },
	{ 0x188, 12, 3 },
	{ 0x188, 12, 4 },
	{ 0x188, 12, 5 },
	{ 0x188, 12, 6 },
	{ 0x188, 12, 7 },

	{ 0x188, 14, 0 },
	{ 0x188, 14, 1 },
	{ 0x188, 14, 2 },
	{ 0x188, 14, 3 },
	{ 0x188, 14, 4 },
	{ 0x188, 14, 5 },
	{ 0x188, 14, 6 },
	{ 0x188, 14, 7 },

	{ 0x188, 15, 0 },
	{ 0x188, 15, 1 },
	{ 0x188, 15, 2 },
	{ 0x188, 15, 3 },
	{ 0x188, 15, 4 },
	{ 0x188, 15, 5 },
	{ 0x188, 15, 6 },
	{ 0x188, 15, 7 },

	/* rgb 2 */
	{ 0x188, 30, 0 },
	{ 0x188, 30, 1 },
	{ 0x188, 30, 2 },
	{ 0x188, 30, 3 },
	{ 0x188, 30, 4 },
	{ 0x188, 30, 5 },
	{ 0x188, 30, 6 },
	{ 0x188, 30, 7 },

	{ 0x188, 31, 0 },
	{ 0x188, 31, 1 },
	{ 0x188, 31, 2 },
	{ 0x188, 31, 3 },
	{ 0x188, 31, 4 },
	{ 0x188, 31, 5 },
	{ 0x188, 31, 6 },
	{ 0x188, 31, 7 },

	{ 0x188, 32, 0 },
	{ 0x188, 32, 1 },
	{ 0x188, 32, 2 },
	{ 0x188, 32, 3 },
	{ 0x188, 32, 4 },
	{ 0x188, 32, 5 },
	{ 0x188, 32, 6 },
	{ 0x188, 32, 7 },

	{ 0x188, 34, 0 },
	{ 0x188, 34, 1 },
	{ 0x188, 34, 2 },
	{ 0x188, 34, 3 },
	{ 0x188, 34, 4 },
	{ 0x188, 34, 5 },
	{ 0x188, 34, 6 },
	{ 0x188, 34, 7 },

	{ 0x188, 35, 0 },
	{ 0x188, 35, 1 },
	{ 0x188, 35, 2 },
	{ 0x188, 35, 3 },
	{ 0x188, 35, 4 },
	{ 0x188, 35, 5 },
	{ 0x188, 35, 6 },
	{ 0x188, 35, 7 },

	/* dma 0 */
	{ 0x188, 40, 0 },
	{ 0x188, 40, 1 },
	{ 0x188, 40, 2 },
	{ 0x188, 40, 3 },
	{ 0x188, 40, 4 },
	{ 0x188, 40, 5 },
	{ 0x188, 40, 6 },
	{ 0x188, 40, 7 },

	{ 0x188, 41, 0 },
	{ 0x188, 41, 1 },
	{ 0x188, 41, 2 },
	{ 0x188, 41, 3 },
	{ 0x188, 41, 4 },
	{ 0x188, 41, 5 },
	{ 0x188, 41, 6 },
	{ 0x188, 41, 7 },

	{ 0x188, 42, 0 },
	{ 0x188, 42, 1 },
	{ 0x188, 42, 2 },
	{ 0x188, 42, 3 },
	{ 0x188, 42, 4 },
	{ 0x188, 42, 5 },
	{ 0x188, 42, 6 },
	{ 0x188, 42, 7 },

	{ 0x188, 44, 0 },
	{ 0x188, 44, 1 },
	{ 0x188, 44, 2 },
	{ 0x188, 44, 3 },
	{ 0x188, 44, 4 },
	{ 0x188, 44, 5 },
	{ 0x188, 44, 6 },
	{ 0x188, 44, 7 },

	{ 0x188, 45, 0 },
	{ 0x188, 45, 1 },
	{ 0x188, 45, 2 },
	{ 0x188, 45, 3 },
	{ 0x188, 45, 4 },
	{ 0x188, 45, 5 },
	{ 0x188, 45, 6 },
	{ 0x188, 45, 7 },

	/* cursor 0 */
	{ 0x188, 80, 0 },
	{ 0x188, 80, 1 },
	{ 0x188, 80, 2 },
	{ 0x188, 80, 3 },
	{ 0x188, 80, 4 },
	{ 0x188, 80, 5 },
	{ 0x188, 80, 6 },
	{ 0x188, 80, 7 },

	{ 0x188, 81, 0 },
	{ 0x188, 81, 1 },
	{ 0x188, 81, 2 },
	{ 0x188, 81, 3 },
	{ 0x188, 81, 4 },
	{ 0x188, 81, 5 },
	{ 0x188, 81, 6 },
	{ 0x188, 81, 7 },

	{ 0x188, 82, 0 },
	{ 0x188, 82, 1 },
	{ 0x188, 82, 2 },
	{ 0x188, 82, 3 },
	{ 0x188, 82, 4 },
	{ 0x188, 82, 5 },
	{ 0x188, 82, 6 },
	{ 0x188, 82, 7 },

	{ 0x188, 83, 0 },
	{ 0x188, 83, 1 },
	{ 0x188, 83, 2 },
	{ 0x188, 83, 3 },
	{ 0x188, 83, 4 },
	{ 0x188, 83, 5 },
	{ 0x188, 83, 6 },
	{ 0x188, 83, 7 },

	{ 0x188, 84, 0 },
	{ 0x188, 84, 1 },
	{ 0x188, 84, 2 },
	{ 0x188, 84, 3 },
	{ 0x188, 84, 4 },
	{ 0x188, 84, 5 },
	{ 0x188, 84, 6 },
	{ 0x188, 84, 7 },

	/* fetch sspp1 */
	/* vig 1 */
	{ 0x298, 0, 0 },
	{ 0x298, 0, 1 },
	{ 0x298, 0, 2 },
	{ 0x298, 0, 3 },
	{ 0x298, 0, 4 },
	{ 0x298, 0, 5 },
	{ 0x298, 0, 6 },
	{ 0x298, 0, 7 },

	{ 0x298, 1, 0 },
	{ 0x298, 1, 1 },
	{ 0x298, 1, 2 },
	{ 0x298, 1, 3 },
	{ 0x298, 1, 4 },
	{ 0x298, 1, 5 },
	{ 0x298, 1, 6 },
	{ 0x298, 1, 7 },

	{ 0x298, 2, 0 },
	{ 0x298, 2, 1 },
	{ 0x298, 2, 2 },
	{ 0x298, 2, 3 },
	{ 0x298, 2, 4 },
	{ 0x298, 2, 5 },
	{ 0x298, 2, 6 },
	{ 0x298, 2, 7 },

	{ 0x298, 4, 0 },
	{ 0x298, 4, 1 },
	{ 0x298, 4, 2 },
	{ 0x298, 4, 3 },
	{ 0x298, 4, 4 },
	{ 0x298, 4, 5 },
	{ 0x298, 4, 6 },
	{ 0x298, 4, 7 },

	{ 0x298, 5, 0 },
	{ 0x298, 5, 1 },
	{ 0x298, 5, 2 },
	{ 0x298, 5, 3 },
	{ 0x298, 5, 4 },
	{ 0x298, 5, 5 },
	{ 0x298, 5, 6 },
	{ 0x298, 5, 7 },

	/* vig 3 */
	{ 0x298, 20, 0 },
	{ 0x298, 20, 1 },
	{ 0x298, 20, 2 },
	{ 0x298, 20, 3 },
	{ 0x298, 20, 4 },
	{ 0x298, 20, 5 },
	{ 0x298, 20, 6 },
	{ 0x298, 20, 7 },

	{ 0x298, 21, 0 },
	{ 0x298, 21, 1 },
	{ 0x298, 21, 2 },
	{ 0x298, 21, 3 },
	{ 0x298, 21, 4 },
	{ 0x298, 21, 5 },
	{ 0x298, 21, 6 },
	{ 0x298, 21, 7 },

	{ 0x298, 22, 0 },
	{ 0x298, 22, 1 },
	{ 0x298, 22, 2 },
	{ 0x298, 22, 3 },
	{ 0x298, 22, 4 },
	{ 0x298, 22, 5 },
	{ 0x298, 22, 6 },
	{ 0x298, 22, 7 },

	{ 0x298, 24, 0 },
	{ 0x298, 24, 1 },
	{ 0x298, 24, 2 },
	{ 0x298, 24, 3 },
	{ 0x298, 24, 4 },
	{ 0x298, 24, 5 },
	{ 0x298, 24, 6 },
	{ 0x298, 24, 7 },

	{ 0x298, 25, 0 },
	{ 0x298, 25, 1 },
	{ 0x298, 25, 2 },
	{ 0x298, 25, 3 },
	{ 0x298, 25, 4 },
	{ 0x298, 25, 5 },
	{ 0x298, 25, 6 },
	{ 0x298, 25, 7 },

	/* rgb 1 */
	{ 0x298, 10, 0 },
	{ 0x298, 10, 1 },
	{ 0x298, 10, 2 },
	{ 0x298, 10, 3 },
	{ 0x298, 10, 4 },
	{ 0x298, 10, 5 },
	{ 0x298, 10, 6 },
	{ 0x298, 10, 7 },

	{ 0x298, 11, 0 },
	{ 0x298, 11, 1 },
	{ 0x298, 11, 2 },
	{ 0x298, 11, 3 },
	{ 0x298, 11, 4 },
	{ 0x298, 11, 5 },
	{ 0x298, 11, 6 },
	{ 0x298, 11, 7 },

	{ 0x298, 12, 0 },
	{ 0x298, 12, 1 },
	{ 0x298, 12, 2 },
	{ 0x298, 12, 3 },
	{ 0x298, 12, 4 },
	{ 0x298, 12, 5 },
	{ 0x298, 12, 6 },
	{ 0x298, 12, 7 },

	{ 0x298, 14, 0 },
	{ 0x298, 14, 1 },
	{ 0x298, 14, 2 },
	{ 0x298, 14, 3 },
	{ 0x298, 14, 4 },
	{ 0x298, 14, 5 },
	{ 0x298, 14, 6 },
	{ 0x298, 14, 7 },

	{ 0x298, 15, 0 },
	{ 0x298, 15, 1 },
	{ 0x298, 15, 2 },
	{ 0x298, 15, 3 },
	{ 0x298, 15, 4 },
	{ 0x298, 15, 5 },
	{ 0x298, 15, 6 },
	{ 0x298, 15, 7 },

	/* rgb 3 */
	{ 0x298, 30, 0 },
	{ 0x298, 30, 1 },
	{ 0x298, 30, 2 },
	{ 0x298, 30, 3 },
	{ 0x298, 30, 4 },
	{ 0x298, 30, 5 },
	{ 0x298, 30, 6 },
	{ 0x298, 30, 7 },

	{ 0x298, 31, 0 },
	{ 0x298, 31, 1 },
	{ 0x298, 31, 2 },
	{ 0x298, 31, 3 },
	{ 0x298, 31, 4 },
	{ 0x298, 31, 5 },
	{ 0x298, 31, 6 },
	{ 0x298, 31, 7 },

	{ 0x298, 32, 0 },
	{ 0x298, 32, 1 },
	{ 0x298, 32, 2 },
	{ 0x298, 32, 3 },
	{ 0x298, 32, 4 },
	{ 0x298, 32, 5 },
	{ 0x298, 32, 6 },
	{ 0x298, 32, 7 },

	{ 0x298, 34, 0 },
	{ 0x298, 34, 1 },
	{ 0x298, 34, 2 },
	{ 0x298, 34, 3 },
	{ 0x298, 34, 4 },
	{ 0x298, 34, 5 },
	{ 0x298, 34, 6 },
	{ 0x298, 34, 7 },

	{ 0x298, 35, 0 },
	{ 0x298, 35, 1 },
	{ 0x298, 35, 2 },
	{ 0x298, 35, 3 },
	{ 0x298, 35, 4 },
	{ 0x298, 35, 5 },
	{ 0x298, 35, 6 },
	{ 0x298, 35, 7 },

	/* dma 1 */
	{ 0x298, 40, 0 },
	{ 0x298, 40, 1 },
	{ 0x298, 40, 2 },
	{ 0x298, 40, 3 },
	{ 0x298, 40, 4 },
	{ 0x298, 40, 5 },
	{ 0x298, 40, 6 },
	{ 0x298, 40, 7 },

	{ 0x298, 41, 0 },
	{ 0x298, 41, 1 },
	{ 0x298, 41, 2 },
	{ 0x298, 41, 3 },
	{ 0x298, 41, 4 },
	{ 0x298, 41, 5 },
	{ 0x298, 41, 6 },
	{ 0x298, 41, 7 },

	{ 0x298, 42, 0 },
	{ 0x298, 42, 1 },
	{ 0x298, 42, 2 },
	{ 0x298, 42, 3 },
	{ 0x298, 42, 4 },
	{ 0x298, 42, 5 },
	{ 0x298, 42, 6 },
	{ 0x298, 42, 7 },

	{ 0x298, 44, 0 },
	{ 0x298, 44, 1 },
	{ 0x298, 44, 2 },
	{ 0x298, 44, 3 },
	{ 0x298, 44, 4 },
	{ 0x298, 44, 5 },
	{ 0x298, 44, 6 },
	{ 0x298, 44, 7 },

	{ 0x298, 45, 0 },
	{ 0x298, 45, 1 },
	{ 0x298, 45, 2 },
	{ 0x298, 45, 3 },
	{ 0x298, 45, 4 },
	{ 0x298, 45, 5 },
	{ 0x298, 45, 6 },
	{ 0x298, 45, 7 },

	/* cursor 1 */
	{ 0x298, 80, 0 },
	{ 0x298, 80, 1 },
	{ 0x298, 80, 2 },
	{ 0x298, 80, 3 },
	{ 0x298, 80, 4 },
	{ 0x298, 80, 5 },
	{ 0x298, 80, 6 },
	{ 0x298, 80, 7 },

	{ 0x298, 81, 0 },
	{ 0x298, 81, 1 },
	{ 0x298, 81, 2 },
	{ 0x298, 81, 3 },
	{ 0x298, 81, 4 },
	{ 0x298, 81, 5 },
	{ 0x298, 81, 6 },
	{ 0x298, 81, 7 },

	{ 0x298, 82, 0 },
	{ 0x298, 82, 1 },
	{ 0x298, 82, 2 },
	{ 0x298, 82, 3 },
	{ 0x298, 82, 4 },
	{ 0x298, 82, 5 },
	{ 0x298, 82, 6 },
	{ 0x298, 82, 7 },

	{ 0x298, 83, 0 },
	{ 0x298, 83, 1 },
	{ 0x298, 83, 2 },
	{ 0x298, 83, 3 },
	{ 0x298, 83, 4 },
	{ 0x298, 83, 5 },
	{ 0x298, 83, 6 },
	{ 0x298, 83, 7 },

	{ 0x298, 84, 0 },
	{ 0x298, 84, 1 },
	{ 0x298, 84, 2 },
	{ 0x298, 84, 3 },
	{ 0x298, 84, 4 },
	{ 0x298, 84, 5 },
	{ 0x298, 84, 6 },
	{ 0x298, 84, 7 },

	/* dspp */
	{ 0x348, 13, 0 },
	{ 0x348, 19, 0 },
	{ 0x348, 14, 0 },
	{ 0x348, 14, 1 },
	{ 0x348, 14, 3 },
	{ 0x348, 20, 0 },
	{ 0x348, 20, 1 },
	{ 0x348, 20, 3 },

	/* dither */
	{ 0x348, 18, 1 },
	{ 0x348, 24, 1 },

	/* ppb_0 */
	{ 0x348, 31, 0 },
	{ 0x348, 33, 0 },
	{ 0x348, 35, 0 },
	{ 0x348, 42, 0 },

	/* ppb_1 */
	{ 0x348, 32, 0 },
	{ 0x348, 34, 0 },
	{ 0x348, 36, 0 },
	{ 0x348, 43, 0 },

	/* lm_lut */
	{ 0x348, 109, 0 },
	{ 0x348, 105, 0 },
	{ 0x348, 103, 0 },
	{ 0x348, 101, 0 },
	{ 0x348,  99, 0 },

	/* tear-check */
	{ 0x418, 63, 0 },
	{ 0x418, 64, 0 },
	{ 0x418, 65, 0 },
	{ 0x418, 73, 0 },
	{ 0x418, 74, 0 },

	/* crossbar */
	{ 0x348, 0, 0},

	/* blend */
	/* LM0 */
	{ 0x348, 63, 0},
	{ 0x348, 63, 1},
	{ 0x348, 63, 2},
	{ 0x348, 63, 3},
	{ 0x348, 63, 4},
	{ 0x348, 63, 5},
	{ 0x348, 63, 6},
	{ 0x348, 63, 7},

	{ 0x348, 64, 0},
	{ 0x348, 64, 1},
	{ 0x348, 64, 2},
	{ 0x348, 64, 3},
	{ 0x348, 64, 4},
	{ 0x348, 64, 5},
	{ 0x348, 64, 6},
	{ 0x348, 64, 7},

	{ 0x348, 65, 0},
	{ 0x348, 65, 1},
	{ 0x348, 65, 2},
	{ 0x348, 65, 3},
	{ 0x348, 65, 4},
	{ 0x348, 65, 5},
	{ 0x348, 65, 6},
	{ 0x348, 65, 7},

	{ 0x348, 66, 0},
	{ 0x348, 66, 1},
	{ 0x348, 66, 2},
	{ 0x348, 66, 3},
	{ 0x348, 66, 4},
	{ 0x348, 66, 5},
	{ 0x348, 66, 6},
	{ 0x348, 66, 7},

	{ 0x348, 67, 0},
	{ 0x348, 67, 1},
	{ 0x348, 67, 2},
	{ 0x348, 67, 3},
	{ 0x348, 67, 4},
	{ 0x348, 67, 5},
	{ 0x348, 67, 6},
	{ 0x348, 67, 7},

	{ 0x348, 68, 0},
	{ 0x348, 68, 1},
	{ 0x348, 68, 2},
	{ 0x348, 68, 3},
	{ 0x348, 68, 4},
	{ 0x348, 68, 5},
	{ 0x348, 68, 6},
	{ 0x348, 68, 7},

	{ 0x348, 69, 0},
	{ 0x348, 69, 1},
	{ 0x348, 69, 2},
	{ 0x348, 69, 3},
	{ 0x348, 69, 4},
	{ 0x348, 69, 5},
	{ 0x348, 69, 6},
	{ 0x348, 69, 7},

	/* LM1 */
	{ 0x348, 70, 0},
	{ 0x348, 70, 1},
	{ 0x348, 70, 2},
	{ 0x348, 70, 3},
	{ 0x348, 70, 4},
	{ 0x348, 70, 5},
	{ 0x348, 70, 6},
	{ 0x348, 70, 7},

	{ 0x348, 71, 0},
	{ 0x348, 71, 1},
	{ 0x348, 71, 2},
	{ 0x348, 71, 3},
	{ 0x348, 71, 4},
	{ 0x348, 71, 5},
	{ 0x348, 71, 6},
	{ 0x348, 71, 7},

	{ 0x348, 72, 0},
	{ 0x348, 72, 1},
	{ 0x348, 72, 2},
	{ 0x348, 72, 3},
	{ 0x348, 72, 4},
	{ 0x348, 72, 5},
	{ 0x348, 72, 6},
	{ 0x348, 72, 7},

	{ 0x348, 73, 0},
	{ 0x348, 73, 1},
	{ 0x348, 73, 2},
	{ 0x348, 73, 3},
	{ 0x348, 73, 4},
	{ 0x348, 73, 5},
	{ 0x348, 73, 6},
	{ 0x348, 73, 7},

	{ 0x348, 74, 0},
	{ 0x348, 74, 1},
	{ 0x348, 74, 2},
	{ 0x348, 74, 3},
	{ 0x348, 74, 4},
	{ 0x348, 74, 5},
	{ 0x348, 74, 6},
	{ 0x348, 74, 7},

	{ 0x348, 75, 0},
	{ 0x348, 75, 1},
	{ 0x348, 75, 2},
	{ 0x348, 75, 3},
	{ 0x348, 75, 4},
	{ 0x348, 75, 5},
	{ 0x348, 75, 6},
	{ 0x348, 75, 7},

	{ 0x348, 76, 0},
	{ 0x348, 76, 1},
	{ 0x348, 76, 2},
	{ 0x348, 76, 3},
	{ 0x348, 76, 4},
	{ 0x348, 76, 5},
	{ 0x348, 76, 6},
	{ 0x348, 76, 7},

	/* LM2 */
	{ 0x348, 77, 0},
	{ 0x348, 77, 1},
	{ 0x348, 77, 2},
	{ 0x348, 77, 3},
	{ 0x348, 77, 4},
	{ 0x348, 77, 5},
	{ 0x348, 77, 6},
	{ 0x348, 77, 7},

	{ 0x348, 78, 0},
	{ 0x348, 78, 1},
	{ 0x348, 78, 2},
	{ 0x348, 78, 3},
	{ 0x348, 78, 4},
	{ 0x348, 78, 5},
	{ 0x348, 78, 6},
	{ 0x348, 78, 7},

	{ 0x348, 79, 0},
	{ 0x348, 79, 1},
	{ 0x348, 79, 2},
	{ 0x348, 79, 3},
	{ 0x348, 79, 4},
	{ 0x348, 79, 5},
	{ 0x348, 79, 6},
	{ 0x348, 79, 7},

	{ 0x348, 80, 0},
	{ 0x348, 80, 1},
	{ 0x348, 80, 2},
	{ 0x348, 80, 3},
	{ 0x348, 80, 4},
	{ 0x348, 80, 5},
	{ 0x348, 80, 6},
	{ 0x348, 80, 7},

	{ 0x348, 81, 0},
	{ 0x348, 81, 1},
	{ 0x348, 81, 2},
	{ 0x348, 81, 3},
	{ 0x348, 81, 4},
	{ 0x348, 81, 5},
	{ 0x348, 81, 6},
	{ 0x348, 81, 7},

	{ 0x348, 82, 0},
	{ 0x348, 82, 1},
	{ 0x348, 82, 2},
	{ 0x348, 82, 3},
	{ 0x348, 82, 4},
	{ 0x348, 82, 5},
	{ 0x348, 82, 6},
	{ 0x348, 82, 7},

	{ 0x348, 83, 0},
	{ 0x348, 83, 1},
	{ 0x348, 83, 2},
	{ 0x348, 83, 3},
	{ 0x348, 83, 4},
	{ 0x348, 83, 5},
	{ 0x348, 83, 6},
	{ 0x348, 83, 7},

	/* csc */
	{0x188, 7, 0},
	{0x188, 7, 1},
	{0x188, 27, 0},
	{0x188, 27, 1},
	{0x298, 7, 0},
	{0x298, 7, 1},
	{0x298, 27, 0},
	{0x298, 27, 1},

	/* pcc */
	{ 0x188, 3,  3},
	{ 0x188, 23, 3},
	{ 0x188, 13, 3},
	{ 0x188, 33, 3},
	{ 0x188, 43, 3},
	{ 0x298, 3,  3},
	{ 0x298, 23, 3},
	{ 0x298, 13, 3},
	{ 0x298, 33, 3},
	{ 0x298, 43, 3},

	/* spa */
	{ 0x188, 8,  0},
	{ 0x188, 28, 0},
	{ 0x298, 8,  0},
	{ 0x298, 28, 0},
	{ 0x348, 13, 0},
	{ 0x348, 19, 0},

	/* igc */
	{ 0x188, 9,  0},
	{ 0x188, 9,  1},
	{ 0x188, 9,  3},
	{ 0x188, 29, 0},
	{ 0x188, 29, 1},
	{ 0x188, 29, 3},
	{ 0x188, 17, 0},
	{ 0x188, 17, 1},
	{ 0x188, 17, 3},
	{ 0x188, 37, 0},
	{ 0x188, 37, 1},
	{ 0x188, 37, 3},
	{ 0x188, 46, 0},
	{ 0x188, 46, 1},
	{ 0x188, 46, 3},

	{ 0x298, 9,  0},
	{ 0x298, 9,  1},
	{ 0x298, 9,  3},
	{ 0x298, 29, 0},
	{ 0x298, 29, 1},
	{ 0x298, 29, 3},
	{ 0x298, 17, 0},
	{ 0x298, 17, 1},
	{ 0x298, 17, 3},
	{ 0x298, 37, 0},
	{ 0x298, 37, 1},
	{ 0x298, 37, 3},
	{ 0x298, 46, 0},
	{ 0x298, 46, 1},
	{ 0x298, 46, 3},

	{ 0x348, 14, 0},
	{ 0x348, 14, 1},
	{ 0x348, 14, 3},
	{ 0x348, 20, 0},
	{ 0x348, 20, 1},
	{ 0x348, 20, 3},

	{ 0x418, 60, 0},
};

static struct vbif_debug_bus vbif_dbg_bus_8996[] = {
	{0x214, 0x21c, 16, 2, 0x10}, /* arb clients */
	{0x214, 0x21c, 0, 14, 0x13}, /* xin blocks - axi side */
	{0x21c, 0x214, 0, 14, 0xc}, /* xin blocks - clock side */
};

static struct vbif_debug_bus nrt_vbif_dbg_bus_8996[] = {
	{0x214, 0x21c, 16, 1, 0x10}, /* arb clients */
	{0x214, 0x21c, 0, 12, 0x13}, /* xin blocks - axi side */
	{0x21c, 0x214, 0, 12, 0xc}, /* xin blocks - clock side */
};

void mdss_mdp_hw_rev_debug_caps_init(struct mdss_data_type *mdata)
{
	mdata->dbg_bus = NULL;
	mdata->dbg_bus_size = 0;

	switch (mdata->mdp_rev) {
	case MDSS_MDP_HW_REV_107:
	case MDSS_MDP_HW_REV_107_1:
	case MDSS_MDP_HW_REV_107_2:
		mdata->dbg_bus = dbg_bus_8996;
		mdata->dbg_bus_size = ARRAY_SIZE(dbg_bus_8996);
		mdata->vbif_dbg_bus = vbif_dbg_bus_8996;
		mdata->vbif_dbg_bus_size = ARRAY_SIZE(vbif_dbg_bus_8996);
		mdata->nrt_vbif_dbg_bus = nrt_vbif_dbg_bus_8996;
		mdata->nrt_vbif_dbg_bus_size =
			ARRAY_SIZE(nrt_vbif_dbg_bus_8996);
		break;
	default:
		break;
	}
}

void mdss_mdp_debug_mid(u32 mid)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	struct range_dump_node *xlog_node;
	struct mdss_debug_base *blk_base;
	char *addr;
	u32 len;

	list_for_each_entry(blk_base, &mdd->base_list, head) {
		list_for_each_entry(xlog_node, &blk_base->dump_list, head) {
			if (xlog_node->xin_id != mid)
				continue;

			len = get_dump_range(&xlog_node->offset,
				blk_base->max_offset);
			addr = blk_base->base + xlog_node->offset.start;
			pr_info("%s: mid:%d range_base=0x%p start=0x%x end=0x%x\n",
				xlog_node->range_name, mid, addr,
				xlog_node->offset.start, xlog_node->offset.end);

			/*
			 * Next instruction assumes that MDP clocks are ON
			 * because it is called from interrupt context
			 */
			mdss_dump_reg((const char *)xlog_node->range_name,
				MDSS_DBG_DUMP_IN_LOG, addr, len,
				&xlog_node->reg_dump, true);
		}
	}
}

static void __print_time(char *buf, u32 size, u64 ts)
{
	unsigned long rem_ns = do_div(ts, NSEC_PER_SEC);

	snprintf(buf, size, "%llu.%06lu", ts, rem_ns);
}

static void __print_buf(struct seq_file *s, struct mdss_mdp_data *buf,
		bool show_pipe)
{
	char tmpbuf[20];
	int i;
	const char const *buf_stat_stmap[] = {
		[MDP_BUF_STATE_UNUSED]  = "UNUSED ",
		[MDP_BUF_STATE_READY]   = "READY  ",
		[MDP_BUF_STATE_ACTIVE]  = "ACTIVE ",
		[MDP_BUF_STATE_CLEANUP] = "CLEANUP",
	};
	const char const *domain_stmap[] = {
		[MDSS_IOMMU_DOMAIN_UNSECURE]     = "mdp_unsecure",
		[MDSS_IOMMU_DOMAIN_ROT_UNSECURE] = "rot_unsecure",
		[MDSS_IOMMU_DOMAIN_SECURE]       = "mdp_secure",
		[MDSS_IOMMU_DOMAIN_ROT_SECURE]   = "rot_secure",
		[MDSS_IOMMU_MAX_DOMAIN]          = "undefined",
	};
	const char const *dma_data_dir_stmap[] = {
		[DMA_BIDIRECTIONAL] = "read/write",
		[DMA_TO_DEVICE]     = "read",
		[DMA_FROM_DEVICE]   = "read/write",
		[DMA_NONE]          = "????",
	};

	seq_puts(s, "\t");
	if (show_pipe && buf->last_pipe)
		seq_printf(s, "pnum=%d ", buf->last_pipe->num);

	seq_printf(s, "state=%s addr=%pa size=%lu ",
		buf->state < ARRAY_SIZE(buf_stat_stmap) &&
		buf_stat_stmap[buf->state] ? buf_stat_stmap[buf->state] : "?",
		&buf->p[0].addr, buf->p[0].len);

	__print_time(tmpbuf, sizeof(tmpbuf), buf->last_alloc);
	seq_printf(s, "alloc_time=%s ", tmpbuf);
	if (buf->state == MDP_BUF_STATE_UNUSED) {
		__print_time(tmpbuf, sizeof(tmpbuf), buf->last_freed);
		seq_printf(s, "freed_time=%s ", tmpbuf);
	} else {
		for (i = 0; i < buf->num_planes; i++) {
			seq_puts(s, "\n\t\t");
			seq_printf(s, "plane[%d] domain=%s ", i,
				domain_stmap[buf->p[i].domain]);
			seq_printf(s, "permission=%s ",
				dma_data_dir_stmap[buf->p[i].dir]);
		}
	}
	seq_puts(s, "\n");
}

static void __dump_pipe(struct seq_file *s, struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_data *buf;
	int format;
	int smps[4];
	int i;

	seq_printf(s, "\nSSPP #%d type=%s ndx=%x flags=0x%08x play_cnt=%u xin_id=%d\n",
			pipe->num, mdss_mdp_pipetype2str(pipe->type),
			pipe->ndx, pipe->flags, pipe->play_cnt, pipe->xin_id);
	seq_printf(s, "\tstage=%d alpha=0x%x transp=0x%x blend_op=%d\n",
			pipe->mixer_stage, pipe->alpha,
			pipe->transp, pipe->blend_op);

	format = pipe->src_fmt->format;
	seq_printf(s, "\tsrc w=%d h=%d format=%d (%s)\n",
			pipe->img_width, pipe->img_height, format,
			mdss_mdp_format2str(format));
	seq_printf(s, "\tsrc_rect x=%d y=%d w=%d h=%d H.dec=%d V.dec=%d\n",
			pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
			pipe->horz_deci, pipe->vert_deci);
	seq_printf(s, "\tdst_rect x=%d y=%d w=%d h=%d\n",
			pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

	smps[0] = bitmap_weight(pipe->smp_map[0].allocated,
			MAX_DRV_SUP_MMB_BLKS);
	smps[1] = bitmap_weight(pipe->smp_map[1].allocated,
			MAX_DRV_SUP_MMB_BLKS);
	smps[2] = bitmap_weight(pipe->smp_map[0].reserved,
			MAX_DRV_SUP_MMB_BLKS);
	smps[3] = bitmap_weight(pipe->smp_map[1].reserved,
			MAX_DRV_SUP_MMB_BLKS);

	seq_printf(s, "\tSMP allocated=[%d %d] reserved=[%d %d]\n",
			smps[0], smps[1], smps[2], smps[3]);

	seq_puts(s, "\tSupported formats = ");
	for (i = 0; i < BITS_TO_BYTES(MDP_IMGTYPE_LIMIT1); i++)
		seq_printf(s, "0x%02X ", pipe->supported_formats[i]);
	seq_puts(s, "\n");

	seq_puts(s, "Data:\n");

	list_for_each_entry(buf, &pipe->buf_queue, pipe_list)
		__print_buf(s, buf, false);
}

static void __dump_mixer(struct seq_file *s, struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *pipe;
	int i, cnt = 0;

	if (!mixer)
		return;

	seq_printf(s, "\n%s Mixer #%d  res=%dx%d roi[%d, %d, %d, %d] %s\n",
		mixer->type == MDSS_MDP_MIXER_TYPE_INTF ? "Intf" : "Writeback",
		mixer->num, mixer->width, mixer->height,
		mixer->roi.x, mixer->roi.y, mixer->roi.w, mixer->roi.h,
		mixer->cursor_enabled ? "w/cursor" : "");

	for (i = 0; i < ARRAY_SIZE(mixer->stage_pipe); i++) {
		pipe = mixer->stage_pipe[i];
		if (pipe) {
			__dump_pipe(s, pipe);
			cnt++;
		}
	}

	seq_printf(s, "\nTotal pipes=%d\n", cnt);
}

static void __dump_timings(struct seq_file *s, struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->panel_data)
		return;

	pinfo = &ctl->panel_data->panel_info;
	seq_printf(s, "Panel #%d %dx%dp%d\n",
			pinfo->pdest, pinfo->xres, pinfo->yres,
			mdss_panel_get_framerate(pinfo));
	seq_printf(s, "\tvbp=%d vfp=%d vpw=%d hbp=%d hfp=%d hpw=%d\n",
			pinfo->lcdc.v_back_porch,
			pinfo->lcdc.v_front_porch,
			pinfo->lcdc.v_pulse_width,
			pinfo->lcdc.h_back_porch,
			pinfo->lcdc.h_front_porch,
			pinfo->lcdc.h_pulse_width);

	if (pinfo->lcdc.border_bottom || pinfo->lcdc.border_top ||
			pinfo->lcdc.border_left ||
			pinfo->lcdc.border_right) {
		seq_printf(s, "\tborder (l,t,r,b):[%d,%d,%d,%d] off xy:%d,%d\n",
				pinfo->lcdc.border_left,
				pinfo->lcdc.border_top,
				pinfo->lcdc.border_right,
				pinfo->lcdc.border_bottom,
				ctl->border_x_off,
				ctl->border_y_off);
	}
}

static void __dump_ctl(struct seq_file *s, struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_perf_params *perf;
	if (!mdss_mdp_ctl_is_power_on(ctl))
		return;

	seq_printf(s, "\n--[ Control path #%d - ", ctl->num);

	if (ctl->panel_data) {
		struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);

		seq_printf(s, "%s%s]--\n",
			sctl && sctl->panel_data ? "DUAL " : "",
			mdss_panel2str(ctl->panel_data->panel_info.type));
		__dump_timings(s, ctl);
		__dump_timings(s, sctl);
	} else {
		struct mdss_mdp_mixer *mixer;
		mixer = ctl->mixer_left;
		if (mixer) {
			seq_printf(s, "%s%d",
					(mixer->rotator_mode ? "rot" : "wb"),
					mixer->num);
		} else {
			seq_puts(s, "unknown");
		}
		seq_puts(s, "]--\n");
	}
	perf = &ctl->cur_perf;
	seq_printf(s, "MDP Clk=%u  Final BW=%llu\n",
			perf->mdp_clk_rate,
			perf->bw_ctl);
	seq_printf(s, "Play Count=%u  Underrun Count=%u\n",
			ctl->play_cnt, ctl->underrun_cnt);

	__dump_mixer(s, ctl->mixer_left);
	__dump_mixer(s, ctl->mixer_right);
}

static int __dump_mdp(struct seq_file *s, struct mdss_data_type *mdata)
{
	struct mdss_mdp_ctl *ctl;
	int i, ignore_ndx = -1;

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		/* ignore slave ctl in split display case */
		if (ctl->num == ignore_ndx)
			continue;
		if (ctl->mixer_right && (ctl->mixer_right->ctl != ctl))
			ignore_ndx = ctl->mixer_right->ctl->num;
		__dump_ctl(s, ctl);
	}
	return 0;
}

#define DUMP_CHUNK 256
#define DUMP_SIZE SZ_32K
void mdss_mdp_dump(struct mdss_data_type *mdata)
{
	struct seq_file s = {
		.size = DUMP_SIZE - 1,
	};
	int i;

	s.buf = kzalloc(DUMP_SIZE, GFP_KERNEL);
	if (!s.buf)
		return;

	__dump_mdp(&s, mdata);
	seq_puts(&s, "\n");

	pr_info("MDP DUMP\n------------------------\n");
	for (i = 0; i < s.count; i += DUMP_CHUNK) {
		if ((s.count - i) > DUMP_CHUNK) {
			char c = s.buf[i + DUMP_CHUNK];
			s.buf[i + DUMP_CHUNK] = 0;
			pr_cont("%s", s.buf + i);
			s.buf[i + DUMP_CHUNK] = c;
		} else {
			s.buf[s.count] = 0;
			pr_cont("%s", s.buf + i);
		}
	}

	kfree(s.buf);
}

#ifdef CONFIG_DEBUG_FS
static void __dump_buf_data(struct seq_file *s, struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_data *buf;
	int i = 0;

	seq_printf(s, "List of buffers for fb%d\n", mfd->index);

	mutex_lock(&mdp5_data->list_lock);
	if (!list_empty(&mdp5_data->bufs_used)) {
		seq_puts(s, " Buffers used:\n");
		list_for_each_entry(buf, &mdp5_data->bufs_used, buf_list)
			__print_buf(s, buf, true);
	}

	if (!list_empty(&mdp5_data->bufs_freelist)) {
		seq_puts(s, " Buffers in free list:\n");
		list_for_each_entry(buf, &mdp5_data->bufs_freelist, buf_list)
			__print_buf(s, buf, true);
	}

	if (!list_empty(&mdp5_data->bufs_pool)) {
		seq_printf(s, " Last %d buffers used:\n", BUF_DUMP_LAST_N);

		list_for_each_entry_reverse(buf, &mdp5_data->bufs_pool,
				buf_list) {
			if (buf->last_freed == 0 || i == BUF_DUMP_LAST_N)
				break;
			__print_buf(s, buf, true);
			i++;
		}
	}
	mutex_unlock(&mdp5_data->list_lock);
}

static int __dump_buffers(struct seq_file *s, struct mdss_data_type *mdata)
{
	struct mdss_mdp_ctl *ctl;
	int i, ignore_ndx = -1;

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		/* ignore slave ctl in split display case */
		if (ctl->num == ignore_ndx)
			continue;
		if (ctl->mixer_right && (ctl->mixer_right->ctl != ctl))
			ignore_ndx = ctl->mixer_right->ctl->num;

		if (ctl->mfd)
			__dump_buf_data(s, ctl->mfd);
	}
	return 0;
}

static int mdss_debugfs_dump_show(struct seq_file *s, void *v)
{
	struct mdss_data_type *mdata = (struct mdss_data_type *)s->private;

	return __dump_mdp(s, mdata);
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdss_debugfs_dump);

static int mdss_debugfs_buffers_show(struct seq_file *s, void *v)
{
	struct mdss_data_type *mdata = (struct mdss_data_type *)s->private;

	return __dump_buffers(s, mdata);
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdss_debugfs_buffers);

static int __danger_safe_signal_status(struct seq_file *s, bool danger_status)
{
	struct mdss_data_type *mdata = (struct mdss_data_type *)s->private;
	u32 status;
	int i, j;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	if (danger_status) {
		seq_puts(s, "\nDanger signal status:\n");
		status = readl_relaxed(mdata->mdp_base +
			MDSS_MDP_DANGER_STATUS);
	} else {
		seq_puts(s, "\nSafe signal status:\n");
		status = readl_relaxed(mdata->mdp_base +
			MDSS_MDP_SAFE_STATUS);
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	seq_printf(s, "MDP     :  0x%lx\n",
		DANGER_SAFE_STATUS(status, MDP_DANGER_SAFE_BIT_OFFSET));

	for (i = 0, j = VIG_DANGER_SAFE_BIT_OFFSET; i < mdata->nvig_pipes;
			i++, j += 2)
		seq_printf(s, "VIG%d    :  0x%lx  \t", i,
			DANGER_SAFE_STATUS(status, j));
	seq_puts(s, "\n");

	for (i = 0, j = RGB_DANGER_SAFE_BIT_OFFSET; i < mdata->nrgb_pipes;
			i++, j += 2)
		seq_printf(s, "RGB%d    :  0x%lx  \t", i,
			DANGER_SAFE_STATUS(status, j));
	seq_puts(s, "\n");
	for (i = 0, j = DMA_DANGER_SAFE_BIT_OFFSET; i < mdata->ndma_pipes;
			i++, j += 2)
		seq_printf(s, "DMA%d    :  0x%lx  \t", i,
			DANGER_SAFE_STATUS(status, j));
	seq_puts(s, "\n");

	for (i = 0, j = CURSOR_DANGER_SAFE_BIT_OFFSET; i < mdata->ncursor_pipes;
			i++, j += 2)
		seq_printf(s, "CURSOR%d :  0x%lx  \t", i,
			DANGER_SAFE_STATUS(status, j));
	seq_puts(s, "\n");

	return 0;
}

static int mdss_debugfs_danger_stats_show(struct seq_file *s, void *v)
{
	return __danger_safe_signal_status(s, true);
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdss_debugfs_danger_stats);

static int mdss_debugfs_safe_stats_show(struct seq_file *s, void *v)
{
	return __danger_safe_signal_status(s, false);
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdss_debugfs_safe_stats);

static void __stats_ctl_dump(struct mdss_mdp_ctl *ctl, struct seq_file *s)
{
	if (!ctl->ref_cnt)
		return;

	if (ctl->intf_num) {
		seq_printf(s, "intf%d: play: %08u \t",
				ctl->intf_num, ctl->play_cnt);
		seq_printf(s, "vsync: %08u \tunderrun: %08u\n",
				ctl->vsync_cnt, ctl->underrun_cnt);
		if (ctl->mfd) {
			seq_printf(s, "user_bl: %08u \tmod_bl: %08u\n",
				ctl->mfd->bl_level, ctl->mfd->bl_level_scaled);
		}
	} else {
		seq_printf(s, "wb: \tmode=%x \tplay: %08u\n",
				ctl->opmode, ctl->play_cnt);
	}
}

static int mdss_debugfs_stats_show(struct seq_file *s, void *v)
{
	struct mdss_data_type *mdata = (struct mdss_data_type *)s->private;
	struct mdss_mdp_pipe *pipe;
	int i;

	seq_puts(s, "\nmdp:\n");

	for (i = 0; i < mdata->nctl; i++)
		__stats_ctl_dump(mdata->ctl_off + i, s);
	seq_puts(s, "\n");

	for (i = 0; i < mdata->nvig_pipes; i++) {
		pipe = mdata->vig_pipes + i;
		seq_printf(s, "VIG%d :   %08u\t", i, pipe->play_cnt);
	}
	seq_puts(s, "\n");

	for (i = 0; i < mdata->nrgb_pipes; i++) {
		pipe = mdata->rgb_pipes + i;
		seq_printf(s, "RGB%d :   %08u\t", i, pipe->play_cnt);
	}
	seq_puts(s, "\n");

	for (i = 0; i < mdata->ndma_pipes; i++) {
		pipe = mdata->dma_pipes + i;
		seq_printf(s, "DMA%d :   %08u\t", i, pipe->play_cnt);
	}
	seq_puts(s, "\n");

	return 0;
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdss_debugfs_stats);

int mdss_mdp_debugfs_init(struct mdss_data_type *mdata)
{
	struct mdss_debug_data *mdd;

	if (!mdata)
		return -ENODEV;

	mdd = mdata->debug_inf.debug_data;
	if (!mdd)
		return -ENOENT;

	debugfs_create_file("dump", 0644, mdd->root, mdata,
			&mdss_debugfs_dump_fops);
	debugfs_create_file("buffers", 0644, mdd->root, mdata,
			&mdss_debugfs_buffers_fops);
	debugfs_create_file("stat", 0644, mdd->root, mdata,
			&mdss_debugfs_stats_fops);
	debugfs_create_file("danger_stat", 0644, mdd->root, mdata,
			&mdss_debugfs_danger_stats_fops);
	debugfs_create_file("safe_stat", 0644, mdd->root, mdata,
			&mdss_debugfs_safe_stats_fops);
	debugfs_create_bool("serialize_wait4pp", 0644, mdd->root,
		(u32 *)&mdata->serialize_wait4pp);
	debugfs_create_bool("wait4autorefresh", 0644, mdd->root,
		(u32 *)&mdata->wait4autorefresh);
	debugfs_create_bool("enable_gate", 0644, mdd->root,
		(u32 *)&mdata->enable_gate);

	debugfs_create_u32("color0", 0644, mdd->bordercolor,
		(u32 *)&mdata->bcolor0);
	debugfs_create_u32("color1", 0644, mdd->bordercolor,
		(u32 *)&mdata->bcolor1);
	debugfs_create_u32("color2", 0644, mdd->bordercolor,
		(u32 *)&mdata->bcolor2);
	debugfs_create_u32("ad_debugen", 0644, mdd->postproc,
		(u32 *)&mdata->ad_debugen);

	return 0;
}
#endif
