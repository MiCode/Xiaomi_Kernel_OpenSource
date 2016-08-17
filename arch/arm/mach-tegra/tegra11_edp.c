/*
 * arch/arm/mach-tegra/tegra11_edp.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/kobject.h>
#include <linux/err.h>

#include <mach/edp.h>

#include "clock.h"
#include "fuse.h"

#define CORE_MODULES_STATES 1
#define TEMPERATURE_RANGES 4
#define CAP_CLKS_NUM 2
#define	TOTAL_CAPS (CORE_EDP_PROFILES_NUM * CORE_MODULES_STATES *\
			TEMPERATURE_RANGES * CAP_CLKS_NUM)

struct core_edp_entry {
	int sku;
	int process_id;
	unsigned int cap_mA;
	int mult;
	unsigned long cap_scpu_on[CORE_EDP_PROFILES_NUM][
		CORE_MODULES_STATES][TEMPERATURE_RANGES][CAP_CLKS_NUM];
	unsigned long cap_scpu_off[CORE_EDP_PROFILES_NUM][
		CORE_MODULES_STATES][TEMPERATURE_RANGES][CAP_CLKS_NUM];
};

static int temperatures[] = { 50, 70, 90, 105 };

#ifdef CONFIG_TEGRA_DUAL_CBUS
static char *cap_clks_names[] = { "edp.emc", "edp.c2bus" };
#else
static char *cap_clks_names[] = { "edp.emc", "edp.cbus" };
#endif
static struct clk *cap_clks[CAP_CLKS_NUM];

static struct core_edp_entry core_edp_table[] = {
	/* SKU 3 */
	{
		.sku		= 0x3,		/* SKU = 3 */
		.process_id	= -1,		/* any process id */
		.cap_mA		= 6000,		/* 6A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 636 },
				 { 924, 612 },
				 { 924, 564 },
				 { 924, 384 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 636 },
				 { 792, 636 },
				 { 792, 636 },
				 { 792, 384 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 624, 672 },
				 { 624, 672 },
				 { 528, 672 },
				 { 528, 384 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 648 },
				 { 924, 636 },
				 { 924, 516 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 792, 672 },
				 { 792, 672 },
				 { 792, 516 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 792, 672 },
				 { 792, 672 },
				 { 792, 516 },
				}
			},
		},
	},
	{
		.sku		= 0x3,		/* SKU = 3 */
		.process_id	= -1,		/* any process id */
		.cap_mA		= 8000,		/* 8A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 588 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 588 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 588 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 588 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 588 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 588 },
				}
			},
		},
	},
	/* SKU 4 */
	{
		.sku		= 0x4,		/* SKU = 4 */
		.process_id	= -1,		/* any process id */
		.cap_mA		= 6000,		/* 6A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 636 },
				 { 924, 624 },
				 { 924, 588 },
				 { 924, 526 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 672 },
				 { 792, 636 },
				 { 792, 636 },
				 { 792, 576 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 672 },
				 { 624, 672 },
				 { 624, 672 },
				 { 624, 636 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 660 },
				 { 924, 636 },
				 { 924, 588 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 792, 672 },
				 { 792, 672 },
				 { 792, 648 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 792, 672 },
				 { 792, 672 },
				 { 792, 648 },
				}
			},
		},
	},
	{
		.sku		= 0x4,		/* SKU = 4 */
		.process_id	= -1,		/* any process id */
		.cap_mA		= 8000,		/* 8A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 648 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 648 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 648 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 648 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 672 },
				 { 924, 672 },
				 { 924, 672 },
				 { 924, 648 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 924, 828 },
				 { 924, 816 },
				 { 924, 804 },
				 { 924, 648 },
				}
			},
		},
	},
	/* SKU 5 */
	{
		.sku		= 0x5,		/* SKU = 5 */
		.process_id	= 0,		/* bin 0 */
		.cap_mA		= 4000,		/* 4A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 336 },
				 { 792, 336 },
				 { 792, 300 },
				 { 792, 240 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 624, 396 },
				 { 660, 372 },
				 { 660, 324 },
				 { 660, 288 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 408, 492 },
				 { 408, 396 },
				 { 408, 396 },
				 { 408, 396 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 432 },
				 { 792, 432 },
				 { 792, 396 },
				 { 792, 348 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 624, 492 },
				 { 660, 492 },
				 { 660, 444 },
				 { 660, 384 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 408, 516 },
				 { 408, 516 },
				 { 408, 516 },
				 { 408, 492 },
				}
			},
		},
	},
	{
		.sku		= 0x5,		/* SKU = 5 */
		.process_id	= 1,		/* bin 1 */
		.cap_mA		= 4000,		/* 4A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 348 },
				 { 792, 336 },
				 { 792, 300 },
				 { 792, 240 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 624, 420 },
				 { 660, 372 },
				 { 660, 324 },
				 { 660, 288 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 408, 528 },
				 { 408, 492 },
				 { 408, 420 },
				 { 408, 420 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 432 },
				 { 792, 432 },
				 { 792, 396 },
				 { 792, 348 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 624, 528 },
				 { 660, 492 },
				 { 660, 444 },
				 { 660, 384 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 408, 564 },
				 { 408, 564 },
				 { 408, 528 },
				 { 408, 528 },
				}
			},
		},
	},
	{
		.sku		= 0x5,		/* SKU = 5 */
		.process_id	= -1,		/* any process id */
		.cap_mA		= 6000,		/* 6A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 792, 516 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 660, 600 },
				 { 660, 564 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 660, 600 },
				 { 528, 600 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 624, 600 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 624, 600 },
				}
			},
		},
	},
	/* SKU 6 */
	{
		.sku		= 0x6,		/* SKU = 6 */
		.process_id	= 0,		/* bin 0 */
		.cap_mA		= 4000,		/* 4A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 348 },
				 { 792, 348 },
				 { 792, 312 },
				 { 792, 264 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 660, 372 },
				 { 660, 372 },
				 { 660, 336 },
				 { 660, 300 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 408, 492 },
				 { 408, 396 },
				 { 408, 396 },
				 { 408, 396 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 444 },
				 { 792, 444 },
				 { 792, 408 },
				 { 792, 372 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 660, 492 },
				 { 660, 492 },
				 { 660, 456 },
				 { 660, 408 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 408, 552 },
				 { 408, 516 },
				 { 408, 516 },
				 { 408, 516 },
				}
			},
		},
	},
	{
		.sku		= 0x6,		/* SKU = 6 */
		.process_id	= 1,		/* bin 1 */
		.cap_mA		= 4000,		/* 4A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 348 },
				 { 792, 348 },
				 { 792, 312 },
				 { 792, 264 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 660, 420 },
				 { 660, 372 },
				 { 660, 336 },
				 { 660, 300 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 408, 528 },
				 { 408, 492 },
				 { 408, 420 },
				 { 408, 420 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 444 },
				 { 792, 444 },
				 { 792, 408 },
				 { 792, 372 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 660, 492 },
				 { 660, 492 },
				 { 660, 456 },
				 { 660, 408 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 408, 564 },
				 { 408, 564 },
				 { 408, 564 },
				 { 408, 528 },
				}
			},
		},
	},
	{
		.sku		= 0x6,		/* SKU = 6 */
		.process_id	= -1,		/* any process id */
		.cap_mA		= 6000,		/* 6A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 792, 516 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 660, 600 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 660, 600 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				 { 792, 600 },
				}
			},
		},
	},
	/* SKU 8 */
	{
		.sku		= 0x8,		/* SKU = 8 */
		.process_id	= -1,		/* any process id */
		.cap_mA		= 6000,		/* 6A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 636 },
				 { 744, 636 },
				 { 744, 576 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 636 },
				 { 744, 636 },
				 { 744, 576 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 624, 672 },
				 { 624, 672 },
				 { 624, 636 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 672 },
				 { 744, 672 },
				 { 744, 648 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 672 },
				 { 744, 672 },
				 { 744, 648 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 672 },
				 { 744, 672 },
				 { 744, 648 },
				}
			},
		},
	},
	{
		.sku		= 0x8,		/* SKU = 8 */
		.process_id	= -1,		/* any process id */
		.cap_mA		= 8000,		/* 8A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_scpu_on	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 672 },
				 { 744, 672 },
				 { 744, 648 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 672 },
				 { 744, 672 },
				 { 744, 648 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 672 },
				 { 744, 672 },
				 { 744, 648 },
				}
			},
		},
		.cap_scpu_off	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 672 },
				 { 744, 672 },
				 { 744, 648 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 744, 672 },
				 { 744, 672 },
				 { 744, 672 },
				 { 744, 648 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 792, 828 },
				 { 792, 816 },
				 { 792, 804 },
				 { 792, 648 },
				}
			},
		},
	},
};

#ifdef CONFIG_TEGRA_EDP_LIMITS
#define LEAKAGE_CONSTS_IJK_COMMON					\
	{								\
		/* i = 0 */						\
		{ {   13919916,  -28721837,   7560552,  -570495, },	\
		  {  -39991855,   87294629, -22972570,  1734058, },	\
		  {   36869935,  -86826110,  22833611, -1723750, },	\
		  {  -10611796,   28192235,  -7407903,   559012, },	\
		},							\
		/* i = 1 */						\
		{ {  -37335213,   53397584, -16025243,  1341064, },	\
		  {  111121782, -160756323,  48421377, -4049609, },	\
		  { -107149149,  157911131, -47786861,  3994796, },	\
		  {   32802647,  -49872380,  15236453, -1268662, },	\
		},							\
		/* i = 2 */						\
		{ {    3315214,  -21010655,   7718286,  -789185, },	\
		  {   -4336249,   59786076, -22312653,  2313754, },	\
		  {   -3346058,  -54529998,  20777469, -2198700, },	\
		  {    4810027,   15417133,  -6086955,   665766, },	\
		},							\
		/* i = 3 */						\
		{ {    4681958,   -1470999,   -232691,    73384, },	\
		  {  -15445149,    5487248,    422447,  -201475, },	\
		  {   16983482,   -6716242,    -65917,   174128, },	\
		  {   -6293336,    2756799,   -140100,   -44673, },	\
		},							\
	}

#define LEAKAGE_PARAMS_COMMON_PART					\
	.temp_scaled        = 10,					\
	.dyn_scaled	    = 1000000,					\
	.dyn_consts_n	    = { 1410000, 2440000, 3450000, 4440000 },	\
	.consts_scaled	    = 1000000,					\
	.leakage_consts_n   = {	 400000,  650000,  850000, 1050000 },	\
	.ijk_scaled	    = 10000,					\
	.leakage_min	    = 30,					\
	.volt_temp_cap	    = { 70, 1300 },				\
	.leakage_consts_ijk = LEAKAGE_CONSTS_IJK_COMMON

static struct tegra_edp_cpu_leakage_params t11x_leakage_params[] = {
	{
		.cpu_speedo_id	    = 0, /* A01 CPU */
		.max_current_cap = { /* values are from tegra4 datasheet */
			{ .max_cur = 9000, .max_temp = 60,
				{ 1900000, 1900000, 1600000, 1600000 }
			},
			{ .max_cur = 9000, .max_temp = 75,
				{ 1900000, 1900000, 1530000, 1530000 }
			},
			{ .max_cur = 9000, .max_temp = 90,
				{ 1900000, 1900000, 1500000, 1500000 }
			},
			{ .max_cur = 12000, .max_temp = 90,
				{ 1900000, 1900000, 1700000, 1700000 }
			},
			{ .max_cur = 15000, .max_temp = 90,
				{ 1900000, 1900000, 1900000, 1900000 }
			},
		},
		LEAKAGE_PARAMS_COMMON_PART,
	},
	{
		.cpu_speedo_id	    = 1, /* A01P+ CPU */
		.safety_cap = { 1810500, 1810500, 1606500, 1606500 },
		.max_current_cap = { /* values are from tegra4 datasheet */
			{ .max_cur = 7500, .max_temp = 90,
				{ 1800000, 1700000, 1320000, 1320000 }
			},
			{ .max_cur = 7500, .max_temp = 75,
				{ 1800000, 1700000, 1420000, 1420000 }
			},
			{ .max_cur = 7500, .max_temp = 60,
				{ 1800000, 1800000, 1420000, 1420000 }
			},
			{ .max_cur = 7500, .max_temp = 45,
				{ 1800000, 1800000, 1530000, 1530000 }
			},
			{ .max_cur = 9000, .max_temp = 90,
				{ 1800000, 1800000, 1500000, 1500000 }
			},
			{ .max_cur = 9000, .max_temp = 75,
				{ 1800000, 1800000, 1530000, 1530000 }
			},
			{ .max_cur = 9000, .max_temp = 60,
				{ 1800000, 1800000, 1600000, 1600000 }
			},
			{ .max_cur = 12000, .max_temp = 45,
				{ 1800000, 1800000, 1600000, 1600000 }
			},
		},
		LEAKAGE_PARAMS_COMMON_PART,
	},
	{
		.cpu_speedo_id	    = 2, /* A01P+ fast CPU */
		.safety_cap = { 1912500, 1912500, 1912500, 1912500 },
		.max_current_cap = { /* values are from tegra4 datasheet */
			{ .max_cur = 9000, .max_temp = 90,
				{ 1900000, 1900000, 1500000, 1500000 }
			},
			{ .max_cur = 9000, .max_temp = 75,
				{ 1900000, 1900000, 1530000, 1530000 }
			},
			{ .max_cur = 9000, .max_temp = 60,
				{ 1900000, 1900000, 1600000, 1600000 }
			},
			{ .max_cur = 12000, .max_temp = 90,
				{ 1900000, 1900000, 1700000, 1700000 }
			},
			{ .max_cur = 15000, .max_temp = 90,
				{ 1900000, 1900000, 1900000, 1900000 }
			},
		},
		LEAKAGE_PARAMS_COMMON_PART,
	},
	{
		.cpu_speedo_id	    = 3,
		.safety_cap = { 1810500, 1810500, 1810500, 1810500 },
		.max_current_cap = { /* fixed values */
			{ .max_cur = 9000, .max_temp = 105,
				{ 1810500, 1810500, 1530000, 1530000 }
			},
			{ .max_cur = 9000, .max_temp = 90,
				{ 1810500, 1810500, 1606500, 1606500 }
			},
			{ .max_cur = 12000, .max_temp = 105,
				{ 1810500, 1810500, 1708500, 1708500 }
			},
			{ .max_cur = 15000, .max_temp = 105,
				{ 1810500, 1810500, 1810500, 1810500 }
			},
		},
		LEAKAGE_PARAMS_COMMON_PART,
	},
};

struct tegra_edp_cpu_leakage_params *tegra11x_get_leakage_params(int index,
							unsigned int *sz)
{
	BUG_ON(index >= ARRAY_SIZE(t11x_leakage_params));
	if (sz)
		*sz = ARRAY_SIZE(t11x_leakage_params);
	return &t11x_leakage_params[index];
}
#endif

static struct core_edp_entry *find_edp_entry(int sku, unsigned int regulator_mA)
{
	int i;
	int pid = tegra_core_process_id();

	if ((sku == 0x5) || (sku == 0x6)) {
		if (regulator_mA >= 8000)
			return NULL;		/* no edp limits above 8A */
	} else if ((sku == 0x3) || (sku == 0x4) || (sku == 0x8)) {
		if (regulator_mA >= 8000)
			regulator_mA = 8000;	/* apply 8A table above 8A */
	} else {
		return NULL;			/* no edp limits at all */
	}

	for (i = 0; i < ARRAY_SIZE(core_edp_table); i++) {
		struct core_edp_entry *entry = &core_edp_table[i];
		if ((entry->sku == sku) && (entry->cap_mA == regulator_mA) &&
		    ((entry->process_id == -1) || (entry->process_id == pid)))
			return entry;
	}
	return ERR_PTR(-ENOENT);
}

static unsigned long clip_cap_rate(struct clk *cap_clk, unsigned long rate)
{
	unsigned long floor, ceiling;
	struct clk *p = clk_get_parent(cap_clk);

	if (!p || !p->ops || !p->ops->shared_bus_update) {
		WARN(1, "%s: edp cap clk %s is not a shared bus user\n",
			__func__, cap_clk->name);
		return rate;
	}

	/*
	 * Clip cap rate to shared bus possible rates (going up via shared
	 * bus * ladder since bus clocks always rounds up with resolution of
	 * at least 2kHz)
	 */
	ceiling = clk_round_rate(p, clk_get_min_rate(p));
	do {
		floor = ceiling;
		ceiling = clk_round_rate(p, floor + 2000);
		if (IS_ERR_VALUE(ceiling)) {
			pr_err("%s: failed to clip %lu to %s possible rates\n",
			       __func__, rate, p->name);
			return rate;
		}
	} while ((floor < ceiling) && (ceiling <= rate));

	if (floor > rate)
		WARN(1, "%s: %s cap rate %lu is below %s floor %lu\n",
			__func__, cap_clk->name, rate, p->name, floor);
	return floor;
}

int __init tegra11x_select_core_edp_table(unsigned int regulator_mA,
					  struct tegra_core_edp_limits *limits)
{
	int i;
	int sku = tegra_sku_id;
	unsigned long *cap_rates;
	struct core_edp_entry *edp_entry;

	BUG_ON(ARRAY_SIZE(temperatures) != TEMPERATURE_RANGES);
	BUG_ON(ARRAY_SIZE(cap_clks_names) != CAP_CLKS_NUM);
	for (i = 0; i < CAP_CLKS_NUM; i++) {
		struct clk *c = tegra_get_clock_by_name(cap_clks_names[i]);
		if (!c) {
			pr_err("%s: failed to find edp cap clock %s\n",
			       __func__, cap_clks_names[i]);
			return -ENODEV;
		}
		cap_clks[i] = c;
	}

	edp_entry = find_edp_entry(sku, regulator_mA);
	if (!edp_entry) {
		pr_info("%s: no core edp table for sku %d, %d mA\n",
		       __func__, sku, regulator_mA);
		return -ENODATA;
	} else if (IS_ERR(edp_entry)) {
		WARN(1, "%s: missing core edp table for sku %d, %d mA\n",
		       __func__, sku, regulator_mA);
		return PTR_ERR(edp_entry);
	}

	limits->sku = sku;
	limits->cap_clocks = cap_clks;
	limits->cap_clocks_num = CAP_CLKS_NUM;
	limits->temperatures = temperatures;
	limits->temperature_ranges = TEMPERATURE_RANGES;
	limits->core_modules_states = CORE_MODULES_STATES;

	cap_rates = &edp_entry->cap_scpu_on[0][0][0][0];
	limits->cap_rates_scpu_on = cap_rates;
	for (i = 0; i < TOTAL_CAPS; i++, cap_rates++) {
		unsigned long rate = *cap_rates * edp_entry->mult;
		*cap_rates = clip_cap_rate(cap_clks[i % CAP_CLKS_NUM], rate);
	}

	cap_rates = &edp_entry->cap_scpu_off[0][0][0][0];
	limits->cap_rates_scpu_off = cap_rates;
	for (i = 0; i < TOTAL_CAPS; i++, cap_rates++) {
		unsigned long rate = *cap_rates * edp_entry->mult;
		*cap_rates = clip_cap_rate(cap_clks[i % CAP_CLKS_NUM], rate);
	}

	return 0;
}
