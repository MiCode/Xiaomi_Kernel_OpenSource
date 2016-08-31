/*
 * arch/arm/mach-tegra/tegra14_edp.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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
#include "common.h"

#define CORE_MODULES_STATES 1
#define TEMPERATURE_RANGES 5
#define CAP_CLKS_NUM 2
#define	TOTAL_CAPS (CORE_EDP_PROFILES_NUM * CORE_MODULES_STATES *\
			TEMPERATURE_RANGES * CAP_CLKS_NUM)

struct core_edp_entry {
	int sku;
	unsigned int cap_mA;
	int mult;
	unsigned long cap_cpu[CORE_EDP_PROFILES_NUM][
		CORE_MODULES_STATES][TEMPERATURE_RANGES][CAP_CLKS_NUM];
};

static int temperatures[] = { 50, 70, 80, 90, 100 };

#ifdef CONFIG_TEGRA_DUAL_CBUS
static char *cap_clks_names[] = { "edp.emc", "edp.c2bus" };
#else
static char *cap_clks_names[] = { "edp.emc", "edp.cbus" };
#endif
static struct clk *cap_clks[CAP_CLKS_NUM];

/* FIXME: Populate with correct values as per final EDP tables.
 * Currently contains *safe* values
 */
static struct core_edp_entry core_edp_table[] = {
	{
		.sku		= 0x7,
		.cap_mA		= 3000,		/* 3A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_cpu	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 655 },
				 { 921, 596 },
				 { 921, 596 },
				 { 921, 596 },
				 { 921, 557 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 655 },
				 { 921, 596 },
				 { 921, 596 },
				 { 921, 596 },
				 { 788, 596 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 655 },
				 { 788, 655 },
				 { 921, 596 },
				 { 921, 596 },
				 { 788, 596 },
				}
			},
		},
	},
	{
		.sku		= 0x3,
		.cap_mA		= 3000,		/* 3A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_cpu	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 672 },
				 { 921, 596 },
				 { 921, 596 },
				 { 921, 596 },
				 { 921, 557 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 672 },
				 { 788, 672 },
				 { 921, 596 },
				 { 921, 596 },
				 { 788, 596 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 672 },
				 { 788, 672 },
				 { 921, 596 },
				 { 921, 596 },
				 { 788, 596 },
				}
			},
		},
	},
	{
		.sku		= 0x3,		/* SL460 */
		.cap_mA		= 3500,		/* 3.5A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_cpu	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 711 },
				 { 921, 711 },
				 { 921, 711 },
				 { 921, 672 },
				 { 921, 672 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 788, 749 },
				 { 921, 711 },
				 { 921, 711 },
				 { 921, 672 },
				 { 921, 672 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 788, 749 },
				 { 921, 711 },
				 { 921, 711 },
				 { 921, 672 },
				 { 921, 672 },
				}
			},
		},
	},
	{
		.sku		= 0x3,		/* SL460 */
		.cap_mA		= 4000,		/* 4A cap */
		.mult		= 1000000,	/* MHZ */
		.cap_cpu	= {
			/* favor emc */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 749 },
				 { 921, 749 },
				 { 921, 711 },
				 { 921, 672 },
				 { 921, 672 },
				},
			},
			/* balanced profile */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 749 },
				 { 921, 749 },
				 { 921, 711 },
				 { 921, 672 },
				 { 921, 672 },
				},
			},
			/* favor gpu */
			{	/* core modules power state 0 (all ON) */
				{{ 921, 749 },
				 { 921, 749 },
				 { 921, 711 },
				 { 921, 672 },
				 { 921, 672 },
				}
			},
		},
	}
};

#ifdef CONFIG_TEGRA_EDP_LIMITS
#define LEAKAGE_CONSTS_IJK_COMMON					\
	{								\
		/* i = 0 */					        \
		{ {    564982,  -1353469,    309283,    -20862, },	\
		  {  -1866916,   4500931,  -1026666,     68669, },	\
		  {   1965934,  -4869757,   1107682,    -73554, },	\
		  {   -637854,   1715497,   -388916,     25621, },	\
		},						        \
		/* i = 1 */					        \
		{ {  -7341396,   7706464,  -1729662,    114105, },	\
		  {  24249928, -25178676,   5651247,   -370322, },	\
		  { -26109261,  26794485,  -6018513,    392722, },	\
		  {   9127986,  -9288224,   2091707,   -135487, },	\
		},						        \
		/* i = 2 */					        \
		{ {   9830061,  -9444047,   2035950,   -132842, },	\
		  { -31837469,  30412491,  -6571344,    428173, },	\
		  {  33645736, -31974811,   6933186,   -452148, },	\
		  { -11561204,  11000249,  -2395570,    156270, },	\
		},						        \
		/* i = 3 */					        \
		{ {  -2848862,   2437747,   -500201,     31386, }, 	\
		  {   9160903,  -7785587,   1605160,   -100724, },	\
		  {  -9619266,   8124245,  -1686232,    106069, },	\
		  {   3291191,  -2777151,    581139,    -36610, },	\
		}, 							\
	}

#define LEAKAGE_PARAMS_COMMON_PART					\
	.temp_scaled        = 10,					\
	.dyn_scaled         = 1000000,					\
	.dyn_consts_n       = {  376000,  638000,  916000, 1203000 },	\
	.consts_scaled      = 1000000,					\
	.leakage_consts_n   = {  489500,  730600,  867600, 1000000 },	\
	.ijk_scaled         = 1000,					\
	.leakage_min        = 30,					\
	.volt_temp_cap	    = { 80, 1200 },				\
	.leakage_consts_ijk = LEAKAGE_CONSTS_IJK_COMMON

static struct tegra_edp_cpu_leakage_params t14x_leakage_params[] = {
	{
		.cpu_speedo_id      = 0, /* A01 CPU */
		LEAKAGE_PARAMS_COMMON_PART,
	},
	{
		.cpu_speedo_id      = 1, /* SKU 0x3 CPU */
		LEAKAGE_PARAMS_COMMON_PART,
	},
};

struct tegra_edp_cpu_leakage_params *tegra14x_get_leakage_params(int index,
							unsigned int *sz)
{
	BUG_ON(index >= ARRAY_SIZE(t14x_leakage_params));
	if (sz)
		*sz = ARRAY_SIZE(t14x_leakage_params);
	return &t14x_leakage_params[index];
}
#endif

static struct core_edp_entry *find_edp_entry(int sku, unsigned int regulator_mA)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(core_edp_table); i++) {
		struct core_edp_entry *entry = &core_edp_table[i];
		if ((entry->sku == sku) && (entry->cap_mA == regulator_mA))
			return entry;
	}
	return NULL;
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

int __init tegra14x_select_core_edp_table(unsigned int regulator_mA,
					  struct tegra_core_edp_limits *limits)
{
	int i;
	int sku;
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

	sku = tegra_get_sku_id();
	if (sku == 0x0)
		sku = 0x7;

	if ((sku == 0x7) && (regulator_mA >= 3500)) {
		pr_info("%s: no core edp capping for sku %d, %d mA\n",
		       __func__, sku, regulator_mA);
		return -ENODATA;
	}

	edp_entry = find_edp_entry(sku, regulator_mA);
	if (!edp_entry) {
		pr_info("%s: no core edp table for sku %d, %d mA\n",
		       __func__, sku, regulator_mA);
		return -ENODATA;
	}

	limits->sku = sku;
	limits->cap_clocks = cap_clks;
	limits->cap_clocks_num = CAP_CLKS_NUM;
	limits->temperatures = temperatures;
	limits->temperature_ranges = TEMPERATURE_RANGES;
	limits->core_modules_states = CORE_MODULES_STATES;

	cap_rates = &edp_entry->cap_cpu[0][0][0][0];
	limits->cap_rates_scpu_on = cap_rates;
	limits->cap_rates_scpu_off = cap_rates;
	for (i = 0; i < TOTAL_CAPS; i++, cap_rates++) {
		unsigned long rate = *cap_rates * edp_entry->mult;
		*cap_rates = clip_cap_rate(cap_clks[i % CAP_CLKS_NUM], rate);
	}

	return 0;
}
