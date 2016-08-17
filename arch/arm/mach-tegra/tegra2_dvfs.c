/*
 * arch/arm/mach-tegra/tegra2_dvfs.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>

#include "clock.h"
#include "dvfs.h"
#include "fuse.h"

#ifdef CONFIG_TEGRA_CORE_DVFS
static bool tegra_dvfs_core_disabled;
#else
static bool tegra_dvfs_core_disabled = true;
#endif
#ifdef CONFIG_TEGRA_CPU_DVFS
static bool tegra_dvfs_cpu_disabled;
#else
static bool tegra_dvfs_cpu_disabled = true;
#endif

static const int core_millivolts[MAX_DVFS_FREQS] =
	{950, 1000, 1100, 1200, 1225, 1275, 1300};
static const int cpu_millivolts[MAX_DVFS_FREQS] =
	{750, 775, 800, 825, 850, 875, 900, 925, 950, 975, 1000, 1025, 1050, 1100, 1125};

static const int cpu_speedo_nominal_millivolts[] =
/* spedo_id  0,    1,    2 */
	{ 1100, 1025, 1125 };

static const int core_speedo_nominal_millivolts[] =
/* spedo_id  0,    1,    2 */
	{ 1225, 1225, 1300 };

#define KHZ 1000
#define MHZ 1000000

static struct dvfs_rail tegra2_dvfs_rail_vdd_cpu = {
	.reg_id = "vdd_cpu",
	.max_millivolts = 1125,
	.min_millivolts = 750,
	.nominal_millivolts = 1125,
};

static struct dvfs_rail tegra2_dvfs_rail_vdd_core = {
	.reg_id = "vdd_core",
	.max_millivolts = 1300,
	.min_millivolts = 950,
	.nominal_millivolts = 1225,
	.step = 150, /* step vdd_core by 150 mV to allow vdd_aon to follow */
};

static struct dvfs_rail tegra2_dvfs_rail_vdd_aon = {
	.reg_id = "vdd_aon",
	.max_millivolts = 1300,
	.min_millivolts = 950,
	.nominal_millivolts = 1225,
#ifndef CONFIG_TEGRA_CORE_DVFS
	.disabled = true,
#endif
};

/* vdd_core and vdd_aon must be 120 mV higher than vdd_cpu */
static int tegra2_dvfs_rel_vdd_cpu_vdd_core(struct dvfs_rail *vdd_cpu,
	struct dvfs_rail *vdd_core)
{
	if (vdd_cpu->new_millivolts > vdd_cpu->millivolts &&
	    vdd_core->new_millivolts < vdd_cpu->new_millivolts + 120)
		return vdd_cpu->new_millivolts + 120;

	if (vdd_core->new_millivolts < vdd_cpu->millivolts + 120)
		return vdd_cpu->millivolts + 120;

	return vdd_core->new_millivolts;
}

/* vdd_aon must be within 170 mV of vdd_core */
static int tegra2_dvfs_rel_vdd_core_vdd_aon(struct dvfs_rail *vdd_core,
	struct dvfs_rail *vdd_aon)
{
	BUG_ON(abs(vdd_aon->millivolts - vdd_core->millivolts) >
		vdd_aon->step);
	return vdd_core->millivolts;
}

static struct dvfs_relationship tegra2_dvfs_relationships[] = {
	{
		/* vdd_core must be 120 mV higher than vdd_cpu */
		.from = &tegra2_dvfs_rail_vdd_cpu,
		.to = &tegra2_dvfs_rail_vdd_core,
		.solve = tegra2_dvfs_rel_vdd_cpu_vdd_core,
	},
	{
		/* vdd_aon must be 120 mV higher than vdd_cpu */
		.from = &tegra2_dvfs_rail_vdd_cpu,
		.to = &tegra2_dvfs_rail_vdd_aon,
		.solve = tegra2_dvfs_rel_vdd_cpu_vdd_core,
	},
	{
		/* vdd_aon must be within 170 mV of vdd_core */
		.from = &tegra2_dvfs_rail_vdd_core,
		.to = &tegra2_dvfs_rail_vdd_aon,
		.solve = tegra2_dvfs_rel_vdd_core_vdd_aon,
	},
};

static struct dvfs_rail *tegra2_dvfs_rails[] = {
	&tegra2_dvfs_rail_vdd_cpu,
	&tegra2_dvfs_rail_vdd_core,
	&tegra2_dvfs_rail_vdd_aon,
};

#define CPU_DVFS(_clk_name, _speedo_id, _process_id, _mult, _freqs...)	\
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= _process_id,			\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= cpu_millivolts,		\
		.auto_dvfs	= true,				\
		.dvfs_rail	= &tegra2_dvfs_rail_vdd_cpu,	\
	}

#define CORE_DVFS(_clk_name, _process_id, _auto, _mult, _freqs...)	\
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= -1,				\
		.process_id	= _process_id,				\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.dvfs_rail	= &tegra2_dvfs_rail_vdd_core,	\
	}

static struct dvfs dvfs_init[] = {
	/* Cpu voltages (mV):	   750, 775, 800, 825, 850, 875,  900,  925,  950,  975,  1000, 1025, 1050, 1100, 1125 */
	CPU_DVFS("cpu", 0, 0, MHZ, 314, 314, 314, 456, 456, 456,  608,  608,  608,  760,  817,  817,  912,  1000),
	CPU_DVFS("cpu", 0, 1, MHZ, 314, 314, 314, 456, 456, 456,  618,  618,  618,  770,  827,  827,  922,  1000),
	CPU_DVFS("cpu", 0, 2, MHZ, 494, 494, 494, 675, 675, 817,  817,  922,  922,  1000),
	CPU_DVFS("cpu", 0, 3, MHZ, 730, 760, 845, 845, 940, 1000),

	CPU_DVFS("cpu", 1, 0, MHZ, 380, 380, 503, 503, 655, 655,  798,  798,  902,  902,  960,  1000),
	CPU_DVFS("cpu", 1, 1, MHZ, 389, 389, 503, 503, 655, 760,  798,  798,  950,  950,  1000),
	CPU_DVFS("cpu", 1, 2, MHZ, 598, 598, 750, 750, 893, 893,  1000),
	CPU_DVFS("cpu", 1, 3, MHZ, 730, 760, 845, 845, 940, 1000),

	CPU_DVFS("cpu", 2, 0, MHZ,   0,   0, 503, 503, 655, 760,  798,  798,  950,  970,  1015,  1040,  1100, 1178, 1200),
	CPU_DVFS("cpu", 2, 1, MHZ,   0,   0, 503, 503, 655, 760,  798,  798,  950,  970,  1015,  1040,  1100, 1200),
	CPU_DVFS("cpu", 2, 2, MHZ,   0,   0, 680, 680, 769, 845,  902,  902,  1026, 1092, 1140,  1197,  1200),
	CPU_DVFS("cpu", 2, 3, MHZ,   0,   0, 845, 845, 940, 1000, 1045, 1045, 1130, 1160, 1200),

	/* Core voltages (mV):           950,    1000,   1100,   1200,   1225,   1275,   1300 */
	CORE_DVFS("emc",     -1, 1, KHZ, 57000,  333000, 380000, 666000, 666000, 666000, 760000),

	CORE_DVFS("sdmmc1",  -1, 1, KHZ, 44000,  52000,  52000,  52000,  52000,  52000,  52000),
	CORE_DVFS("sdmmc2",  -1, 1, KHZ, 44000,  52000,  52000,  52000,  52000,  52000,  52000),
	CORE_DVFS("sdmmc3",  -1, 1, KHZ, 44000,  52000,  52000,  52000,  52000,  52000,  52000),
	CORE_DVFS("sdmmc4",  -1, 1, KHZ, 44000,  52000,  52000,  52000,  52000,  52000,  52000),

	CORE_DVFS("ndflash", -1, 1, KHZ, 130000, 150000, 158000, 164000, 164000, 164000, 164000),
	CORE_DVFS("nor",     -1, 1, KHZ, 0,      92000,  92000,  92000,  92000,  92000,  92000),
	CORE_DVFS("ide",     -1, 1, KHZ, 0,      0,      100000, 100000, 100000, 100000, 100000),
	CORE_DVFS("mipi",    -1, 1, KHZ, 0,      40000,  40000,  40000,  40000,  60000,  60000),
	CORE_DVFS("usbd",    -1, 1, KHZ, 0,      0,      480000, 480000, 480000, 480000, 480000),
	CORE_DVFS("usb2",    -1, 1, KHZ, 0,      0,      480000, 480000, 480000, 480000, 480000),
	CORE_DVFS("usb3",    -1, 1, KHZ, 0,      0,      480000, 480000, 480000, 480000, 480000),
	CORE_DVFS("pcie",    -1, 1, KHZ, 0,      0,      0,      250000, 250000, 250000, 250000),
	CORE_DVFS("dsi",     -1, 1, KHZ, 100000, 100000, 100000, 500000, 500000, 500000, 500000),
	CORE_DVFS("tvo",     -1, 1, KHZ, 0,      0,      0,      250000, 250000, 250000, 250000),

	/*
	 * The clock rate for the display controllers that determines the
	 * necessary core voltage depends on a divider that is internal
	 * to the display block.  Disable auto-dvfs on the display clocks,
	 * and let the display driver call tegra_dvfs_set_rate manually
	 */
	CORE_DVFS("disp1",   -1, 0, KHZ, 158000, 158000, 190000, 190000, 190000, 190000, 190000),
	CORE_DVFS("disp2",   -1, 0, KHZ, 158000, 158000, 190000, 190000, 190000, 190000, 190000),
	CORE_DVFS("hdmi",    -1, 0, KHZ, 0,      0,      0,      148500, 148500, 148500, 148500),

	/*
	 * Clocks below depend on the core process id. Define per process_id
	 * tables for SCLK/VDE/3D clocks (maximum rate for these clocks is
	 * increased depending on tegra2 sku). Use the worst case value for
	 * other clocks for now.
	 */
	CORE_DVFS("host1x",  -1, 1, KHZ, 104500, 133000, 166000, 166000, 166000, 166000, 166000),
	CORE_DVFS("epp",     -1, 1, KHZ, 133000, 171000, 247000, 300000, 300000, 300000, 300000),
	CORE_DVFS("2d",      -1, 1, KHZ, 133000, 171000, 247000, 300000, 300000, 300000, 300000),

	CORE_DVFS("3d",       0, 1, KHZ, 114000, 161500, 247000, 304000, 304000, 333500, 333500),
	CORE_DVFS("3d",       1, 1, KHZ, 161500, 209000, 285000, 333500, 333500, 361000, 361000),
	CORE_DVFS("3d",       2, 1, KHZ, 218500, 256500, 323000, 380000, 380000, 400000, 400000),
	CORE_DVFS("3d",       3, 1, KHZ, 247000, 285000, 351500, 400000, 400000, 400000, 400000),

	CORE_DVFS("mpe",      0, 1, KHZ, 104500, 152000, 228000, 300000, 300000, 300000, 300000),
	CORE_DVFS("mpe",      1, 1, KHZ, 142500, 190000, 275500, 300000, 300000, 300000, 300000),
	CORE_DVFS("mpe",      2, 1, KHZ, 190000, 237500, 300000, 300000, 300000, 300000, 300000),
	CORE_DVFS("mpe",      3, 1, KHZ, 228000, 266000, 300000, 300000, 300000, 300000, 300000),

	CORE_DVFS("vi",      -1, 1, KHZ, 85000,  100000, 150000, 150000, 150000, 150000, 150000),

	CORE_DVFS("sclk",     0, 1, KHZ, 95000,  133000, 190000, 222500, 240000, 247000, 262000),
	CORE_DVFS("sclk",     1, 1, KHZ, 123500, 159500, 207000, 240000, 240000, 264000, 277500),
	CORE_DVFS("sclk",     2, 1, KHZ, 152000, 180500, 229500, 260000, 260000, 285000, 300000),
	CORE_DVFS("sclk",     3, 1, KHZ, 171000, 218500, 256500, 292500, 292500, 300000, 300000),

	CORE_DVFS("vde",      0, 1, KHZ, 95000,  123500, 209000, 275500, 275500, 300000, 300000),
	CORE_DVFS("vde",      1, 1, KHZ, 123500, 152000, 237500, 300000, 300000, 300000, 300000),
	CORE_DVFS("vde",      2, 1, KHZ, 152000, 209000, 285000, 300000, 300000, 300000, 300000),
	CORE_DVFS("vde",      3, 1, KHZ, 171000, 218500, 300000, 300000, 300000, 300000, 300000),
	/* What is this? */
	CORE_DVFS("NVRM_DEVID_CLK_SRC", -1, 1, MHZ, 480, 600, 800, 1067, 1067, 1067, 1067),
};

int tegra_dvfs_disable_core_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra2_dvfs_rail_vdd_core);
	else
		tegra_dvfs_rail_enable(&tegra2_dvfs_rail_vdd_core);

	return 0;
}

int tegra_dvfs_disable_cpu_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra2_dvfs_rail_vdd_cpu);
	else
		tegra_dvfs_rail_enable(&tegra2_dvfs_rail_vdd_cpu);

	return 0;
}

int tegra_dvfs_disable_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops tegra_dvfs_disable_core_ops = {
	.set = tegra_dvfs_disable_core_set,
	.get = tegra_dvfs_disable_get,
};

static struct kernel_param_ops tegra_dvfs_disable_cpu_ops = {
	.set = tegra_dvfs_disable_cpu_set,
	.get = tegra_dvfs_disable_get,
};

module_param_cb(disable_core, &tegra_dvfs_disable_core_ops,
	&tegra_dvfs_core_disabled, 0644);
module_param_cb(disable_cpu, &tegra_dvfs_disable_cpu_ops,
	&tegra_dvfs_cpu_disabled, 0644);

void __init tegra2_init_dvfs(void)
{
	int i;
	struct clk *c;
	struct dvfs *d;
	int process_id;
	int ret;
	int cpu_process_id = tegra_cpu_process_id();
	int core_process_id = tegra_core_process_id();
	int speedo_id = tegra_soc_speedo_id();

	BUG_ON(speedo_id >= ARRAY_SIZE(cpu_speedo_nominal_millivolts));
	tegra2_dvfs_rail_vdd_cpu.nominal_millivolts =
		cpu_speedo_nominal_millivolts[speedo_id];
	BUG_ON(speedo_id >= ARRAY_SIZE(core_speedo_nominal_millivolts));
	tegra2_dvfs_rail_vdd_core.nominal_millivolts =
		core_speedo_nominal_millivolts[speedo_id];
	tegra2_dvfs_rail_vdd_aon.nominal_millivolts =
		core_speedo_nominal_millivolts[speedo_id];

	tegra_dvfs_init_rails(tegra2_dvfs_rails, ARRAY_SIZE(tegra2_dvfs_rails));
	tegra_dvfs_add_relationships(tegra2_dvfs_relationships,
		ARRAY_SIZE(tegra2_dvfs_relationships));
	/*
	 * VDD_CORE must always be at least 50 mV higher than VDD_CPU
	 * Fill out cpu_core_millivolts based on cpu_millivolts
	 */
	for (i = 0; i < ARRAY_SIZE(dvfs_init); i++) {
		d = &dvfs_init[i];

		process_id = strcmp(d->clk_name, "cpu") ?
			core_process_id : cpu_process_id;
		if ((d->process_id != -1 && d->process_id != process_id) ||
		    (d->speedo_id != -1 && d->speedo_id != speedo_id)) {
			pr_debug("tegra_dvfs: rejected %s speedo %d,"
				" process %d\n", d->clk_name, d->speedo_id,
				d->process_id);
			continue;
		}

		c = tegra_get_clock_by_name(d->clk_name);

		if (!c) {
			pr_debug("tegra_dvfs: no clock found for %s\n",
				d->clk_name);
			continue;
		}

		ret = tegra_enable_dvfs_on_clk(c, d);
		if (ret)
			pr_err("tegra_dvfs: failed to enable dvfs on %s\n",
				c->name);
	}

	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra2_dvfs_rail_vdd_core);

	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra2_dvfs_rail_vdd_cpu);
}
