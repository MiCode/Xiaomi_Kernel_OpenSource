/*
 * arch/arm/mach-tegra/tegra3_dvfs.c
 *
 * Copyright (C) 2010-2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/clk.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/time.h>

#include "clock.h"
#include "dvfs.h"
#include "fuse.h"
#include "board.h"
#include "tegra3_emc.h"
#include "tegra_core_sysfs_limits.h"

#define CPU_MILLIVOLTS {\
	750, 762, 775, 787, 800, 825, 837, 850, 862, 875, 887, 900, 912, 916, 925, 937, 950, 962, 975, 987, 1000, 1007, 1012, 1025, 1037, 1050, 1062, 1075, 1087, 1100, 1112, 1125, 1137, 1150, 1162, 1175, 1187, 1200, 1212, 1237};

static bool tegra_dvfs_cpu_disabled;
static bool tegra_dvfs_core_disabled;
static struct dvfs *cpu_dvfs;

static int cpu_millivolts[MAX_DVFS_FREQS] = CPU_MILLIVOLTS;

static int cpu_millivolts_aged[MAX_DVFS_FREQS];

static struct dvfs_data *cpu_dvfs_data;
static struct dvfs_data *cpu_0_dvfs_data;
static struct dvfs_data *core_dvfs_data;

static unsigned int cpu_cold_offs_mhz[MAX_DVFS_FREQS] = {
	 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,   50};

static int core_millivolts[MAX_DVFS_FREQS] = {
	950, 1000, 1050, 1100, 1150, 1200, 1250, 1300, 1350};

#define KHZ 1000
#define MHZ 1000000

/* VDD_CPU >= (VDD_CORE - cpu_below_core) */
/* VDD_CORE >= min_level(VDD_CPU), see tegra3_get_core_floor_mv() below */
#define VDD_CPU_BELOW_VDD_CORE		300
static int cpu_below_core = VDD_CPU_BELOW_VDD_CORE;

#define VDD_SAFE_STEP			100

static struct dvfs_rail tegra3_dvfs_rail_vdd_cpu = {
	.reg_id = "vdd_cpu",
	.max_millivolts = 1250,
	.min_millivolts = 725,
	.step = VDD_SAFE_STEP,
	.jmp_to_zero = true,
};

static struct dvfs_rail tegra3_dvfs_rail_vdd_core = {
	.reg_id = "vdd_core",
	.max_millivolts = 1350,
	.min_millivolts = 950,
	.step = VDD_SAFE_STEP,
};

static struct dvfs_rail *tegra3_dvfs_rails[] = {
	&tegra3_dvfs_rail_vdd_cpu,
	&tegra3_dvfs_rail_vdd_core,
};

#ifdef CONFIG_OF
static int of_cpu_millivolts[MAX_DVFS_FREQS];
static int of_core_millivolts[MAX_DVFS_FREQS];

static struct dvfs of_cpu_dvfs[MAX_DVFS_TABLES];
static struct dvfs of_cpu_0_dvfs[MAX_DVFS_TABLES];
static struct dvfs of_core_dvfs[MAX_DVFS_TABLES];

static struct dvfs_data of_cpu_data = {
	.tables = of_cpu_dvfs,
	.millivolts = of_cpu_millivolts,
	.rail = &tegra3_dvfs_rail_vdd_cpu,
};

static struct dvfs_data of_cpu_0_data = {
	.tables = of_cpu_0_dvfs,
	.millivolts = of_cpu_millivolts,
	.rail = &tegra3_dvfs_rail_vdd_cpu,
};

static struct dvfs_data of_core_data = {
	.tables = of_core_dvfs,
	.millivolts = of_core_millivolts,
	.rail = &tegra3_dvfs_rail_vdd_core,
};
#endif

static int tegra3_get_core_floor_mv(int cpu_mv)
{
	if (cpu_mv < 800)
		return  950;
	if (cpu_mv < 900)
		return 1000;
	if (cpu_mv < 1000)
		return 1100;
	if ((tegra_cpu_speedo_id() < 2) ||
	    (tegra_cpu_speedo_id() == 4) ||
	    (tegra_cpu_speedo_id() == 7) ||
	    (tegra_cpu_speedo_id() == 8))
		return 1200;
	if (cpu_mv < 1100)
		return 1200;
	if (cpu_mv <= 1250)
		return 1300;
	BUG();
}

/* vdd_core must be >= min_level as a function of vdd_cpu */
static int tegra3_dvfs_rel_vdd_cpu_vdd_core(struct dvfs_rail *vdd_cpu,
	struct dvfs_rail *vdd_core)
{
	int core_floor = max(vdd_cpu->new_millivolts, vdd_cpu->millivolts);
	core_floor = tegra3_get_core_floor_mv(core_floor);
	return max(vdd_core->new_millivolts, core_floor);
}

/* vdd_cpu must be >= (vdd_core - cpu_below_core) */
static int tegra3_dvfs_rel_vdd_core_vdd_cpu(struct dvfs_rail *vdd_core,
	struct dvfs_rail *vdd_cpu)
{
	int cpu_floor;

	if (vdd_cpu->new_millivolts == 0)
		return 0; /* If G CPU is off, core relations can be ignored */

	cpu_floor = max(vdd_core->new_millivolts, vdd_core->millivolts) -
		cpu_below_core;
	return max(vdd_cpu->new_millivolts, cpu_floor);
}

static struct dvfs_relationship tegra3_dvfs_relationships[] = {
	{
		.from = &tegra3_dvfs_rail_vdd_cpu,
		.to = &tegra3_dvfs_rail_vdd_core,
		.solve = tegra3_dvfs_rel_vdd_cpu_vdd_core,
		.solved_at_nominal = true,
	},
	{
		.from = &tegra3_dvfs_rail_vdd_core,
		.to = &tegra3_dvfs_rail_vdd_cpu,
		.solve = tegra3_dvfs_rel_vdd_core_vdd_cpu,
	},
};

#define CPU_DVFS(_clk_name, _speedo_id, _process_id, _mult, _freqs...)	\
	{								\
		.clk_name	= _clk_name,				\
		.speedo_id	= _speedo_id,				\
		.process_id	= _process_id,				\
		.freqs		= {_freqs},				\
		.freqs_mult	= _mult,				\
		.millivolts	= cpu_millivolts,			\
		.auto_dvfs	= true,					\
		.dvfs_rail	= &tegra3_dvfs_rail_vdd_cpu,		\
	}

static struct dvfs cpu_dvfs_table[] = {
	/* Cpu voltages (mV):         750, 762, 775, 787, 800, 825, 837, 850, 862, 875, 887,  900,  912,  916,  925,  937,  950,  962,  975,  987, 1000, 1007, 1012, 1025, 1037, 1050, 1062, 1075, 1087, 1100, 1112, 1125, 1137, 1150, 1162, 1175, 1187, 1200, 1212, 1237 */
	CPU_DVFS("cpu_g",  0, 0, MHZ,   1,   1,   1,   1,   1,   1,   1, 684, 684, 684, 684,  817,  817,  817,  817,  817,  817,  817, 1026, 1026, 1102, 1102, 1102, 1149, 1149, 1187, 1187, 1225, 1225, 1282, 1282, 1300),
	CPU_DVFS("cpu_g",  0, 1, MHZ,   1,   1,   1,   1,   1,   1,   1, 807, 807, 807, 807,  948,  948,  948,  948,  948,  948,  948, 1117, 1117, 1171, 1171, 1171, 1206, 1206, 1300),
	CPU_DVFS("cpu_g",  0, 2, MHZ,   1,   1,   1,   1,   1,   1,   1, 883, 883, 883, 883, 1039, 1039, 1039, 1039, 1039, 1039, 1039, 1178, 1178, 1206, 1206, 1206, 1300),
	CPU_DVFS("cpu_g",  0, 3, MHZ,   1,   1,   1,   1,   1,   1,   1, 931, 931, 931, 931, 1102, 1102, 1102, 1102, 1102, 1102, 1102, 1216, 1216, 1300, 1300, 1300),

	CPU_DVFS("cpu_g",  1, 0, MHZ,   1,   1,   1,   1, 460, 460, 460, 550, 550, 550, 550,  680,  680,  680,  680,  680,  680,  680,  820,  820,  970,  970,  970, 1040, 1040, 1080, 1080, 1150, 1150, 1200, 1200, 1280, 1280, 1300),
	CPU_DVFS("cpu_g",  1, 1, MHZ,   1,   1,   1,   1, 480, 480, 480, 650, 650, 650, 650,  780,  780,  780,  780,  780,  780,  780,  990,  990, 1040, 1040, 1040, 1100, 1100, 1200, 1200, 1300),
	CPU_DVFS("cpu_g",  1, 2, MHZ,   1,   1,   1,   1, 520, 520, 520, 700, 700, 700, 700,  860,  860,  860,  860,  860,  860,  860, 1050, 1050, 1150, 1150, 1150, 1200, 1200, 1300),
	CPU_DVFS("cpu_g",  1, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1300),

	CPU_DVFS("cpu_g",  2, 1, MHZ,   1,   1,   1,   1, 480, 480, 480, 650, 650, 650, 650,  780,  780,  780,  780,  780,  780,  780,  990,  990, 1040, 1040, 1040, 1100, 1100, 1200, 1200, 1250, 1250, 1300, 1300, 1330, 1330, 1400),
	CPU_DVFS("cpu_g",  2, 2, MHZ,   1,   1,   1,   1, 520, 520, 520, 700, 700, 700, 700,  860,  860,  860,  860,  860,  860,  860, 1050, 1050, 1150, 1150, 1150, 1200, 1200, 1280, 1280, 1300, 1300, 1350, 1350, 1400),
	CPU_DVFS("cpu_g",  2, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1280, 1280, 1300, 1300, 1350, 1350, 1400),

	CPU_DVFS("cpu_g",  3, 1, MHZ,   1,   1,   1,   1, 480, 480, 480, 650, 650, 650, 650,  780,  780,  780,  780,  780,  780,  780,  990,  990, 1040, 1040, 1040, 1100, 1100, 1200, 1200, 1250, 1250, 1300, 1300, 1330, 1330, 1400),
	CPU_DVFS("cpu_g",  3, 2, MHZ,   1,   1,   1,   1, 520, 520, 520, 700, 700, 700, 700,  860,  860,  860,  860,  860,  860,  860, 1050, 1050, 1150, 1150, 1150, 1200, 1200, 1280, 1280, 1300, 1300, 1350, 1350, 1400),
	CPU_DVFS("cpu_g",  3, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1280, 1280, 1300, 1300, 1350, 1350, 1400),

	CPU_DVFS("cpu_g",  4, 0, MHZ,   1,   1,   1,   1, 460, 460, 460, 550, 550, 550, 550,  680,  680,  680,  680,  680,  680,  680,  820,  820,  970,  970,  970, 1040, 1040, 1080, 1080, 1150, 1150, 1200, 1200, 1240, 1240, 1280, 1280, 1320, 1320, 1360, 1360, 1500),
	CPU_DVFS("cpu_g",  4, 1, MHZ,   1,   1,   1,   1, 480, 480, 480, 650, 650, 650, 650,  780,  780,  780,  780,  780,  780,  780,  990,  990, 1040, 1040, 1040, 1100, 1100, 1200, 1200, 1250, 1250, 1300, 1300, 1330, 1330, 1360, 1360, 1400, 1400, 1500),
	CPU_DVFS("cpu_g",  4, 2, MHZ,   1,   1,   1,   1, 520, 520, 520, 700, 700, 700, 700,  860,  860,  860,  860,  860,  860,  860, 1050, 1050, 1150, 1150, 1150, 1200, 1200, 1280, 1280, 1300, 1300, 1340, 1340, 1380, 1380, 1500),
	CPU_DVFS("cpu_g",  4, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1280, 1280, 1330, 1330, 1370, 1370, 1400, 1400, 1500),

	CPU_DVFS("cpu_g",  5, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1280, 1280, 1330, 1330, 1370, 1370, 1400, 1400, 1470, 1470, 1500, 1500, 1500, 1500, 1540, 1540, 1700),
	CPU_DVFS("cpu_g",  5, 4, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  940,  940,  940,  940,  940,  940,  940, 1160, 1160, 1240, 1240, 1240, 1280, 1280, 1360, 1360, 1390, 1390, 1470, 1470, 1500, 1500, 1520, 1520, 1520, 1520, 1590, 1700),

	CPU_DVFS("cpu_g",  6, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1280, 1280, 1330, 1330, 1370, 1370, 1400, 1400, 1470, 1470, 1500, 1500, 1500, 1500, 1540, 1540, 1700),
	CPU_DVFS("cpu_g",  6, 4, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  940,  940,  940,  940,  940,  940,  940, 1160, 1160, 1240, 1240, 1240, 1280, 1280, 1360, 1360, 1390, 1390, 1470, 1470, 1500, 1500, 1520, 1520, 1520, 1520, 1590, 1700),

	CPU_DVFS("cpu_g",  7, 0, MHZ,   1,   1,   1,   1, 460, 460, 460, 550, 550, 550, 550,  680,  680,  680,  680,  680,  680,  680,  820,  820,  970,  970,  970, 1040, 1040, 1080, 1080, 1150, 1150, 1200, 1200, 1280, 1280, 1300),
	CPU_DVFS("cpu_g",  7, 1, MHZ,   1,   1,   1,   1, 480, 480, 480, 650, 650, 650, 650,  780,  780,  780,  780,  780,  780,  780,  990,  990, 1040, 1040, 1040, 1100, 1100, 1200, 1200, 1300),
	CPU_DVFS("cpu_g",  7, 2, MHZ,   1,   1,   1,   1, 520, 520, 520, 700, 700, 700, 700,  860,  860,  860,  860,  860,  860,  860, 1050, 1050, 1150, 1150, 1150, 1200, 1200, 1300),
	CPU_DVFS("cpu_g",  7, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1300),
	CPU_DVFS("cpu_g",  7, 4, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  940,  940,  940,  940,  940,  940,  940, 1160, 1160, 1300, 1300, 1300),

	CPU_DVFS("cpu_g",  8, 0, MHZ,   1,   1,   1,   1, 460, 460, 460, 550, 550, 550, 550,  680,  680,  680,  680,  680,  680,  680,  820,  820,  970,  970,  970, 1040, 1040, 1080, 1080, 1150, 1150, 1200, 1200, 1280, 1280, 1300),
	CPU_DVFS("cpu_g",  8, 1, MHZ,   1,   1,   1,   1, 480, 480, 480, 650, 650, 650, 650,  780,  780,  780,  780,  780,  780,  780,  990,  990, 1040, 1040, 1040, 1100, 1100, 1200, 1200, 1300),
	CPU_DVFS("cpu_g",  8, 2, MHZ,   1,   1,   1,   1, 520, 520, 520, 700, 700, 700, 700,  860,  860,  860,  860,  860,  860,  860, 1050, 1050, 1150, 1150, 1150, 1200, 1200, 1300),
	CPU_DVFS("cpu_g",  8, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1300),
	CPU_DVFS("cpu_g",  8, 4, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  940,  940,  940,  940,  940,  940,  940, 1160, 1160, 1300, 1300, 1300),

	CPU_DVFS("cpu_g",  9, -1, MHZ,  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900),
	CPU_DVFS("cpu_g", 10, -1, MHZ,  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,    1,    1,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900),
	CPU_DVFS("cpu_g", 11, -1, MHZ,  1,   1,   1,   1,   1,   1,   1, 600, 600, 600, 600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600,  600),
	CPU_DVFS("cpu_g", 14, -1, MHZ,  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,    1,    1,    1,    1,    1,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900),
	CPU_DVFS("cpu_g", 15, -1, MHZ,  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900,  900),

	CPU_DVFS("cpu_g", 12, 3, MHZ,   1,   475, 475, 475, 475, 620, 620, 620, 620, 760, 760, 760, 760,  760,  910,  910,  1000,  1000, 1000, 1000, 1150, 1150, 1150, 1150, 1150, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1400, 1500, 1500, 1500, 1500, 1500, 1500, 1700),
	CPU_DVFS("cpu_g", 12, 4, MHZ,   475, 475, 475, 475, 475, 620, 620, 620, 760, 760, 760, 760, 910,  910,  910, 1000,  1000,  1000, 1000, 1150, 1150, 1150, 1150, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1400, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1700),

	CPU_DVFS("cpu_g", 13, 3, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  910,  910,  910,  910,  910,  910,  910, 1150, 1150, 1230, 1230, 1230, 1280, 1280, 1330, 1330, 1370, 1370, 1400, 1400, 1470, 1470, 1500, 1500, 1500, 1500, 1540, 1540, 1700),
	CPU_DVFS("cpu_g", 13, 4, MHZ,   1,   1,   1,   1, 550, 550, 550, 770, 770, 770, 770,  940,  940,  940,  940,  940,  940,  940, 1160, 1160, 1240, 1240, 1240, 1280, 1280, 1360, 1360, 1390, 1390, 1470, 1470, 1500, 1500, 1520, 1520, 1520, 1520, 1590, 1700),

	/*
	 * "Safe entry" to be used when no match for chip speedo, process
	 *  corner is found (just to boot at low rate); must be the last one
	 */
	CPU_DVFS("cpu_g", -1, -1, MHZ, 1,   1, 216, 216, 300),
};

static struct dvfs cpu_0_dvfs_table[] = {
	/* Cpu voltages (mV):	      750, 762, 775, 787, 800, 825, 837, 850, 862,  875,  887,  900,  912,  916,  925,  937,  950,  962,  975,  987, 1000, 1007, 1012, 1025, 1037, 1050, 1062, 1075, 1087, 1100, 1112, 1125, 1137, 1150, 1162, 1175, 1187, 1200, 1212, 1237*/
	CPU_DVFS("cpu_0",  4, 0, MHZ,   1,   1,   1, 475, 475, 475, 475, 640, 640,  640,  760,  760,  760,  760,  860,  860,  860,  860,  860, 1000, 1000, 1000, 1000, 1100, 1100, 1100, 1100, 1200, 1200, 1200, 1200, 1200, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1500),
	CPU_DVFS("cpu_0",  4, 1, MHZ,   1, 475, 475, 475, 475, 640, 640, 640, 760,  760,  760,  860,  860,  860,  860,  860, 1000, 1000, 1000, 1100, 1100, 1100, 1100, 1100, 1200, 1200, 1200, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1400, 1400, 1400, 1500),
	CPU_DVFS("cpu_0",  4, 2, MHZ, 475, 475, 475, 640, 640, 640, 760, 760, 760,  860,  860,  860, 1000, 1000, 1000, 1000, 1000, 1100, 1100, 1200, 1200, 1200, 1200, 1200, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1400, 1400, 1500),
	CPU_DVFS("cpu_0",  4, 3, MHZ, 475, 475, 640, 640, 640, 760, 760, 860, 860,  860,  860, 1000, 1000, 1000, 1100, 1100, 1100, 1100, 1200, 1200, 1200, 1200, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1400, 1400, 1500),

	CPU_DVFS("cpu_0",  5, 3, MHZ, 475, 475, 620, 620, 620, 760, 760, 760, 910,  910,  910, 1000, 1000, 1000, 1000, 1000, 1000, 1150, 1150, 1150, 1150, 1150, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1400, 1500, 1500, 1500, 1500, 1500, 1600, 1600, 1600, 1600, 1700),
	CPU_DVFS("cpu_0",  5, 4, MHZ, 475, 620, 620, 620, 760, 760, 760, 910, 910, 1000, 1000, 1000, 1000, 1000, 1150, 1150, 1150, 1150, 1150, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1500, 1500, 1500, 1500, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1700),

	CPU_DVFS("cpu_0",  6, 3, MHZ, 475, 475, 620, 620, 620, 760, 760, 760, 910,  910,  910, 1000, 1000, 1000, 1000, 1000, 1000, 1150, 1150, 1150, 1150, 1150, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1400, 1500, 1500, 1500, 1500, 1500, 1600, 1600, 1600, 1600, 1700),
	CPU_DVFS("cpu_0",  6, 4, MHZ, 475, 620, 620, 620, 760, 760, 760, 910, 910, 1000, 1000, 1000, 1000, 1000, 1150, 1150, 1150, 1150, 1150, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1500, 1500, 1500, 1500, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1700),

	CPU_DVFS("cpu_0", 12, 3, MHZ, 475, 475, 620, 620, 620, 760, 760, 760, 910,  910,  910, 1000, 1000, 1000, 1000, 1000, 1000, 1150, 1150, 1150, 1150, 1150, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1400, 1500, 1500, 1500, 1500, 1500, 1600, 1600, 1600, 1600, 1700),
	CPU_DVFS("cpu_0", 12, 4, MHZ, 475, 620, 620, 620, 760, 760, 760, 910, 910, 1000, 1000, 1000, 1000, 1000, 1000, 1150, 1150, 1150, 1150, 1300, 1300, 1300, 1300, 1400, 1400, 1400, 1500, 1500, 1500, 1500, 1500, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1700),
};

#define CORE_DVFS(_clk_name, _speedo_id, _auto, _mult, _freqs...)	\
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= -1,				\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.dvfs_rail	= &tegra3_dvfs_rail_vdd_core,	\
	}

static struct dvfs core_dvfs_table[] = {
	/* Core voltages (mV):		    950,   1000,   1050,   1100,   1150,    1200,    1250,    1300,    1350 */
	/* Clock limits for internal blocks, PLLs */
	CORE_DVFS("cpu_lp", 0, 1, KHZ,        1, 294000, 342000, 427000, 475000,  500000,  500000,  500000,  500000),
	CORE_DVFS("cpu_lp", 1, 1, KHZ,   204000, 294000, 342000, 427000, 475000,  500000,  500000,  500000,  500000),
	CORE_DVFS("cpu_lp", 2, 1, KHZ,   204000, 295000, 370000, 428000, 475000,  513000,  579000,  620000,  620000),
	CORE_DVFS("cpu_lp", 3, 1, KHZ,        1,      1,      1,      1,      1,       1,  450000,  450000,  450000),

	CORE_DVFS("emc",    0, 1, KHZ,        1, 266500, 266500, 266500, 266500,  533000,  533000,  533000,  533000),
	CORE_DVFS("emc",    1, 1, KHZ,   102000, 408000, 408000, 408000, 408000,  667000,  667000,  667000,  667000),
	CORE_DVFS("emc",    2, 1, KHZ,   102000, 450000, 450000, 450000, 450000,  667000,  667000,  800000,  900000),
	CORE_DVFS("emc",    3, 1, KHZ,        1,      1,      1,      1,      1,       1,  625000,  625000,  625000),

	CORE_DVFS("sbus",   0, 1, KHZ,        1, 136000, 164000, 191000, 216000,  216000,  216000,  216000,  216000),
	CORE_DVFS("sbus",   1, 1, KHZ,   102000, 205000, 205000, 227000, 227000,  267000,  267000,  267000,  267000),
	CORE_DVFS("sbus",   2, 1, KHZ,   102000, 205000, 205000, 227000, 227000,  267000,  334000,  334000,  334000),
	CORE_DVFS("sbus",   3, 1, KHZ,        1,      1,      1,      1,      1,       1,  334000,  334000,  334000),

	CORE_DVFS("vi",     0, 1, KHZ,        1, 216000, 285000, 300000, 300000,  300000,  300000,  300000,  300000),
	CORE_DVFS("vi",     1, 1, KHZ,        1, 216000, 267000, 300000, 371000,  409000,  409000,  409000,  409000),
	CORE_DVFS("vi",     2, 1, KHZ,        1, 219000, 267000, 300000, 371000,  409000,  425000,  425000,  425000),
	CORE_DVFS("vi",     3, 1, KHZ,        1,      1,      1,      1,      1,       1,  470000,  470000,  470000),

	CORE_DVFS("vde",    0, 1, KHZ,        1, 228000, 275000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("mpe",    0, 1, KHZ,        1, 234000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("2d",     0, 1, KHZ,        1, 267000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("epp",    0, 1, KHZ,        1, 267000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("3d",     0, 1, KHZ,        1, 234000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("3d2",    0, 1, KHZ,        1, 234000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("se",     0, 1, KHZ,        1, 267000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),

	CORE_DVFS("vde",    1, 1, KHZ,   200000, 228000, 275000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("mpe",    1, 1, KHZ,   200000, 234000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("2d",     1, 1, KHZ,   200000, 267000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("epp",    1, 1, KHZ,   200000, 267000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("3d",     1, 1, KHZ,   200000, 234000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("3d2",    1, 1, KHZ,   200000, 234000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("se",     1, 1, KHZ,   200000, 267000, 285000, 332000, 380000,  416000,  416000,  416000,  416000),

	CORE_DVFS("vde",    2, 1, KHZ,   200000, 247000, 304000, 352000, 400000,  437000,  484000,  520000,  600000),
	CORE_DVFS("mpe",    2, 1, KHZ,   200000, 247000, 304000, 361000, 408000,  446000,  484000,  520000,  600000),
	CORE_DVFS("2d",     2, 1, KHZ,   200000, 267000, 304000, 361000, 408000,  446000,  484000,  520000,  600000),
	CORE_DVFS("epp",    2, 1, KHZ,   200000, 267000, 304000, 361000, 408000,  446000,  484000,  520000,  600000),
	CORE_DVFS("3d",     2, 1, KHZ,   200000, 247000, 304000, 361000, 408000,  446000,  484000,  520000,  600000),
	CORE_DVFS("3d2",    2, 1, KHZ,   200000, 247000, 304000, 361000, 408000,  446000,  484000,  520000,  600000),
	CORE_DVFS("se",     2, 1, KHZ,   200000, 267000, 304000, 361000, 408000,  446000,  484000,  520000,  600000),

	CORE_DVFS("vde",    3, 1, KHZ,        1,      1,      1,      1,      1,       1,  484000,  484000,  484000),
	CORE_DVFS("mpe",    3, 1, KHZ,        1,      1,      1,      1,      1,       1,  484000,  484000,  484000),
	CORE_DVFS("2d",     3, 1, KHZ,        1,      1,      1,      1,      1,       1,  484000,  484000,  484000),
	CORE_DVFS("epp",    3, 1, KHZ,        1,      1,      1,      1,      1,       1,  484000,  484000,  484000),
	CORE_DVFS("3d",     3, 1, KHZ,        1,      1,      1,      1,      1,       1,  484000,  484000,  484000),
	CORE_DVFS("3d2",    3, 1, KHZ,        1,      1,      1,      1,      1,       1,  484000,  484000,  484000),
	CORE_DVFS("se",     3, 1, KHZ,        1,      1,      1,      1,      1,       1,  625000,  625000,  625000),

	CORE_DVFS("host1x", 0, 1, KHZ,        1, 152000, 188000, 222000, 254000,  267000,  267000,  267000,  267000),
	CORE_DVFS("host1x", 1, 1, KHZ,   100000, 152000, 188000, 222000, 254000,  267000,  267000,  267000,  267000),
	CORE_DVFS("host1x", 2, 1, KHZ,   100000, 152000, 188000, 222000, 254000,  267000,  267000,  267000,  300000),
	CORE_DVFS("host1x", 3, 1, KHZ,        1,      1,      1,      1,      1,       1,  242000,  242000,  242000),

	CORE_DVFS("cbus",   0, 1, KHZ,        1, 228000, 275000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("cbus",   1, 1, KHZ,   200000, 228000, 275000, 332000, 380000,  416000,  416000,  416000,  416000),
	CORE_DVFS("cbus",   2, 1, KHZ,   200000, 247000, 304000, 352000, 400000,  437000,  484000,  520000,  600000),
	CORE_DVFS("cbus",   3, 1, KHZ,        1,      1,      1,      1,      1,       1,  484000,  484000,  484000),

	CORE_DVFS("pll_c",  -1, 1, KHZ,  533000, 667000, 667000, 800000, 800000, 1066000, 1066000, 1066000, 1200000),

	/*
	 * PLLM dvfs is common across all speedo IDs with one special exception
	 * for T30 and T33, rev A02+, provided PLLM usage is restricted. Both
	 * common and restricted table are included, and table selection is
	 * handled by is_pllm_dvfs() below.
	 */
	CORE_DVFS("pll_m",  -1, 1, KHZ,  533000, 667000, 667000, 800000, 800000, 1066000, 1066000, 1066000, 1066000),
#ifdef CONFIG_TEGRA_PLLM_RESTRICTED
	CORE_DVFS("pll_m",   2, 1, KHZ,  533000, 900000, 900000, 900000, 900000, 1066000, 1066000, 1066000, 1066000),
#endif
	/* Core voltages (mV):		    950,   1000,   1050,   1100,   1150,   1200,    1250,     1300,    1350 */
	/* Clock limits for I/O peripherals */
	CORE_DVFS("mipi",   0, 1, KHZ,        1,      1,      1,      1,      1,      1,       1,        1,       1),
	CORE_DVFS("mipi",   1, 1, KHZ,        1,      1,      1,      1,      1,  60000,   60000,    60000,   60000),
	CORE_DVFS("mipi",   2, 1, KHZ,        1,      1,      1,      1,      1,  60000,   60000,    60000,   60000),
	CORE_DVFS("mipi",   3, 1, KHZ,        1,      1,      1,      1,      1,      1,       1,        1,       1),

	CORE_DVFS("fuse_burn", -1, 1, KHZ,    1,      1,      1,      1,  26000,  26000,   26000,    26000,   26000),
	CORE_DVFS("sdmmc1", -1, 1, KHZ,  104000, 104000, 104000, 104000, 104000, 208000,  208000,   208000,  208000),
	CORE_DVFS("sdmmc3", -1, 1, KHZ,  104000, 104000, 104000, 104000, 104000, 208000,  208000,   208000,  208000),
	CORE_DVFS("sdmmc4", -1, 1, KHZ,   51000, 102000, 102000, 102000, 102000, 102000,  102000,   102000,  102000),
	CORE_DVFS("ndflash", -1, 1, KHZ, 120000, 120000, 120000, 120000, 200000, 200000,  200000,   200000,  200000),

	CORE_DVFS("nor",    0, 1, KHZ,        1, 115000, 130000, 130000, 133000, 133000,  133000,   133000,  133000),
	CORE_DVFS("nor",    1, 1, KHZ,   102000, 115000, 130000, 130000, 133000, 133000,  133000,   133000,  133000),
	CORE_DVFS("nor",    2, 1, KHZ,   102000, 115000, 130000, 130000, 133000, 133000,  133000,   133000,  133000),
	CORE_DVFS("nor",    3, 1, KHZ,        1,      1,      1,      1,      1,      1,  108000,   108000,  108000),

	CORE_DVFS("sbc1",  -1, 1, KHZ,    36000,  52000,  60000,  60000,  60000, 100000,  100000,   100000,  100000),
	CORE_DVFS("sbc2",  -1, 1, KHZ,    36000,  52000,  60000,  60000,  60000, 100000,  100000,   100000,  100000),
	CORE_DVFS("sbc3",  -1, 1, KHZ,    36000,  52000,  60000,  60000,  60000, 100000,  100000,   100000,  100000),
	CORE_DVFS("sbc4",  -1, 1, KHZ,    36000,  52000,  60000,  60000,  60000, 100000,  100000,   100000,  100000),
	CORE_DVFS("sbc5",  -1, 1, KHZ,    36000,  52000,  60000,  60000,  60000, 100000,  100000,   100000,  100000),
	CORE_DVFS("sbc6",  -1, 1, KHZ,    36000,  52000,  60000,  60000,  60000, 100000,  100000,   100000,  100000),

	CORE_DVFS("sata",  -1, 1, KHZ,        1, 216000, 216000, 216000, 216000, 216000,  216000,   216000,  216000),
	CORE_DVFS("sata_oob", -1, 1, KHZ,     1, 216000, 216000, 216000, 216000, 216000,  216000,   216000,  216000),

	CORE_DVFS("tvo",   -1, 1, KHZ,        1,      1, 297000, 297000, 297000, 297000,  297000,   297000,  297000),
	CORE_DVFS("cve",   -1, 1, KHZ,        1,      1, 297000, 297000, 297000, 297000,  297000,   297000,  297000),
	CORE_DVFS("dsia",  -1, 1, KHZ,   432500, 432500, 432500, 432500, 432500, 432500,  432500,   432500,  432500),
	CORE_DVFS("dsib",  -1, 1, KHZ,   432500, 432500, 432500, 432500, 432500, 432500,  432500,   432500,  432500),

	/*
	 * The clock rate for the display controllers that determines the
	 * necessary core voltage depends on a divider that is internal
	 * to the display block.  Disable auto-dvfs on the display clocks,
	 * and let the display driver call tegra_dvfs_set_rate manually
	 */
	CORE_DVFS("disp1",  0, 0, KHZ,        1, 120000, 120000, 120000, 120000, 190000,  190000,   190000,  190000),
	CORE_DVFS("disp1",  1, 0, KHZ,   155000, 155000, 268000, 268000, 268000, 268000,  268000,   268000,  268000),
	CORE_DVFS("disp1",  2, 0, KHZ,   155000, 155000, 268000, 268000, 268000, 268000,  268000,   268000,  268000),
	CORE_DVFS("disp1",  3, 0, KHZ,        1, 120000, 120000, 120000, 120000, 190000,  190000,   190000,  190000),

	CORE_DVFS("disp2",  0, 0, KHZ,        1, 120000, 120000, 120000, 120000, 190000,  190000,   190000,  190000),
	CORE_DVFS("disp2",  1, 0, KHZ,   155000, 155000, 268000, 268000, 268000, 268000,  268000,   268000,  268000),
	CORE_DVFS("disp2",  2, 0, KHZ,   155000, 155000, 268000, 268000, 268000, 268000,  268000,   268000,  268000),
	CORE_DVFS("disp2",  3, 0, KHZ,        1, 120000, 120000, 120000, 120000, 190000,  190000,   190000,  190000),

	CORE_DVFS("pwm",   -1, 1, KHZ,   204000, 408000, 408000, 408000, 408000, 408000,  408000,   408000,  408000),
};

/* CPU alternative DVFS table for cold zone */
static unsigned long cpu_cold_freqs[MAX_DVFS_FREQS];

/* CPU alternative DVFS table for single G CPU core 0 */
static unsigned long *cpu_0_freqs;

static struct dvfs_data tegra30_cpu_data = {
	.tables = cpu_dvfs_table,
	.num_tables = ARRAY_SIZE(cpu_dvfs_table),
	.millivolts = cpu_millivolts,
	.num_voltages = ARRAY_SIZE(cpu_millivolts),
	.rail = &tegra3_dvfs_rail_vdd_cpu,
};

static struct dvfs_data tegra30_cpu_0_data = {
	.tables = cpu_0_dvfs_table,
	.num_tables = ARRAY_SIZE(cpu_0_dvfs_table),
	.millivolts = cpu_millivolts,
	.num_voltages = ARRAY_SIZE(cpu_millivolts),
	.rail = &tegra3_dvfs_rail_vdd_cpu,
};

static struct dvfs_data tegra30_core_data = {
	.tables = core_dvfs_table,
	.num_tables = ARRAY_SIZE(core_dvfs_table),
	.millivolts = core_millivolts,
	.num_voltages = ARRAY_SIZE(core_millivolts),
	.rail = &tegra3_dvfs_rail_vdd_core,
};

int tegra_dvfs_disable_core_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra3_dvfs_rail_vdd_core);
	else
		tegra_dvfs_rail_enable(&tegra3_dvfs_rail_vdd_core);

	return 0;
}

int tegra_dvfs_disable_cpu_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra3_dvfs_rail_vdd_cpu);
	else
		tegra_dvfs_rail_enable(&tegra3_dvfs_rail_vdd_cpu);

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

static bool __init is_pllm_dvfs(struct clk *c, struct dvfs *d)
{
#ifdef CONFIG_TEGRA_PLLM_RESTRICTED
	/* Do not apply common PLLM dvfs table on T30, T33, T37 rev A02+ and
	   do not apply restricted PLLM dvfs table for other SKUs/revs */
	int cpu = tegra_cpu_speedo_id();
	if (((cpu == 2) || (cpu == 5) || (cpu == 13)) ==
	    (d->speedo_id == -1))
		return false;
#endif
	/* Check if PLLM boot frequency can be applied to clock tree at
	   minimum voltage. If yes, no need to enable dvfs on PLLM */
	if (clk_get_rate_all_locked(c) <= d->freqs[0] * d->freqs_mult)
		return false;

	return true;
}

static void __init init_dvfs_one(struct dvfs *d, int nominal_mv_index)
{
	int ret;
	struct clk *c = tegra_get_clock_by_name(d->clk_name);

	if (!c) {
		pr_debug("tegra3_dvfs: no clock found for %s\n",
			d->clk_name);
		return;
	}

	/*
	 * Update max rate for auto-dvfs clocks, except EMC.
	 * EMC is a special case, since EMC dvfs is board dependent: max rate
	 * and EMC scaling frequencies are determined by tegra BCT (flashed
	 * together with the image) and board specific EMC DFS table; we will
	 * check the scaling ladder against nominal core voltage when the table
	 * is loaded (and if on particular board the table is not loaded, EMC
	 * scaling is disabled).
	 */
	if (!(c->flags & PERIPH_EMC_ENB) && d->auto_dvfs) {
		BUG_ON(!d->freqs[nominal_mv_index]);
		tegra_init_max_rate(
			c, d->freqs[nominal_mv_index] * d->freqs_mult);
	}
	d->max_millivolts = d->dvfs_rail->nominal_millivolts;

	/*
	 * Check if we may skip enabling dvfs on PLLM. PLLM is a special case,
	 * since its frequency never exceeds boot rate, and configuration with
	 * restricted PLLM usage is possible.
	 */
	if (!(c->flags & PLLM) || is_pllm_dvfs(c, d)) {
		ret = tegra_enable_dvfs_on_clk(c, d);
		if (ret)
			pr_err("tegra3_dvfs: failed to enable dvfs on %s\n",
				c->name);
	}
}

static void __init init_dvfs_cold(struct dvfs *d, int nominal_mv_index)
{
	int i;
	unsigned long offs;

	BUG_ON((nominal_mv_index == 0) || (nominal_mv_index > d->num_freqs));

	for (i = 0; i < d->num_freqs; i++) {
		offs = cpu_cold_offs_mhz[i] * MHZ;
		if (i > nominal_mv_index)
			cpu_cold_freqs[i] = cpu_cold_freqs[i - 1];
		else if (d->freqs[i] > offs)
			cpu_cold_freqs[i] = d->freqs[i] - offs;
		else {
			cpu_cold_freqs[i] = d->freqs[i];
			if (d->freqs[i] > 1 * MHZ)
				pr_warn(
				"%s: offset %lu too high for dvfs limit %lu\n",
				__func__, offs, d->freqs[i]);
		}

		if (i)
			BUG_ON(cpu_cold_freqs[i] < cpu_cold_freqs[i - 1]);
	}
}

static bool __init match_dvfs_one(struct dvfs *d, int speedo_id, int process_id)
{
	if ((d->process_id != -1 && d->process_id != process_id) ||
		(d->speedo_id != -1 && d->speedo_id != speedo_id)) {
		pr_debug("tegra3_dvfs: rejected %s speedo %d,"
			" process %d\n", d->clk_name, d->speedo_id,
			d->process_id);
		return false;
	}
	return true;
}

static void __init init_cpu_0_dvfs(struct dvfs *cpud)
{
	int i;
	struct dvfs *d = NULL;

	/* Init single G CPU core 0 dvfs if this particular SKU/bin has it.
	   Max rates in multi-core and single-core tables must be the same */
	for (i = 0; i < cpu_0_dvfs_data->num_tables; i++) {
		if (match_dvfs_one(&cpu_0_dvfs_data->tables[i],
				   cpud->speedo_id, cpud->process_id)) {
			d = &cpu_0_dvfs_data->tables[i];
			break;
		}
	}

	if (d) {
		for (i = 0; i < cpud->num_freqs; i++) {
			d->freqs[i] *= d->freqs_mult;
			if (d->freqs[i] == 0) {
				BUG_ON(i == 0);
				d->freqs[i] = d->freqs[i - 1];
			}
		}
		BUG_ON(cpud->freqs[cpud->num_freqs - 1] !=
		       d->freqs[cpud->num_freqs - 1]);
		cpu_0_freqs = d->freqs;
	}
}

static int __init get_cpu_nominal_mv_index(
	int speedo_id, int process_id, struct dvfs **cpu_dvfs)
{
	int i, j, mv;
	struct dvfs *d = NULL;
	struct clk *c;

	/*
	 * Find maximum cpu voltage that satisfies cpu_to_core dependency for
	 * nominal core voltage ("solve from cpu to core at nominal"). Clip
	 * result to the nominal cpu level for the chips with this speedo_id.
	 */
	mv = tegra3_dvfs_rail_vdd_core.nominal_millivolts;
	for (i = 0; i < cpu_dvfs_data->num_voltages; i++) {
		if ((cpu_dvfs_data->millivolts[i] == 0) ||
		    tegra3_get_core_floor_mv(cpu_dvfs_data->millivolts[i]) > mv)
			break;
	}
	BUG_ON(i == 0);
	mv = cpu_dvfs_data->millivolts[i - 1];
	BUG_ON(mv < tegra3_dvfs_rail_vdd_cpu.min_millivolts);
	mv = min(mv, tegra_cpu_speedo_mv());

	/*
	 * Find matching cpu dvfs entry, and use it to determine index to the
	 * final nominal voltage, that satisfies the following requirements:
	 * - allows CPU to run at minimum of the maximum rates specified in
	 *   the dvfs entry and clock tree
	 * - does not violate cpu_to_core dependency as determined above
	 */
	for (i = 0, j = 0; j < cpu_dvfs_data->num_tables; j++) {
		d = &cpu_dvfs_data->tables[j];
		if (match_dvfs_one(d, speedo_id, process_id)) {
			c = tegra_get_clock_by_name(d->clk_name);
			BUG_ON(!c);

			for (; i < MAX_DVFS_FREQS; i++) {
				if ((d->freqs[i] == 0) ||
				    (cpu_dvfs_data->millivolts[i] == 0) ||
				    (mv < cpu_dvfs_data->millivolts[i]))
					break;

				if (c->max_rate <= d->freqs[i]*d->freqs_mult) {
					i++;
					break;
				}
			}
			break;
		}
	}

	BUG_ON(i == 0);
	if (j == (cpu_dvfs_data->num_tables - 1))
		pr_err("tegra3_dvfs: WARNING!!!\n"
		       "tegra3_dvfs: no cpu dvfs table found for chip speedo_id"
		       " %d and process_id %d: set CPU rate limit at %lu\n"
		       "tegra3_dvfs: WARNING!!!\n",
		       speedo_id, process_id, d->freqs[i-1] * d->freqs_mult);

	*cpu_dvfs = d;
	return (i - 1);
}

static int __init get_core_nominal_mv_index(int speedo_id)
{
	int i;
	int mv = tegra_core_speedo_mv();
	int core_edp_voltage = get_core_edp();

	/*
	 * Start with nominal level for the chips with this speedo_id. Then,
	 * make sure core nominal voltage is below edp limit for the board
	 * (if edp limit is set).
	 */
	if (!core_edp_voltage)
		core_edp_voltage = 1200;	/* default 1.2V EDP limit */

	mv = min(mv, core_edp_voltage);

	/* Round nominal level down to the nearest core scaling step */
	for (i = 0; i < core_dvfs_data->num_voltages; i++) {
		if ((core_dvfs_data->millivolts[i] == 0) ||
				(mv < core_dvfs_data->millivolts[i]))
			break;
	}

	if (i == 0) {
		pr_err("tegra3_dvfs: unable to adjust core dvfs table to"
		       " nominal voltage %d\n", mv);
		return -ENOSYS;
	}
	return (i - 1);
}

static void tegra_adjust_cpu_mvs(int mvs)
{
	int i;

	BUG_ON(ARRAY_SIZE(cpu_millivolts) != ARRAY_SIZE(cpu_millivolts_aged));

	for (i = 0; i < cpu_dvfs_data->num_voltages; i++)
		cpu_dvfs_data->millivolts[i] = cpu_millivolts_aged[i] - mvs;
}

/**
 * Adjust VDD_CPU to offset aging.
 * 25mV for 1st year
 * 12mV for 2nd and 3rd year
 * 0mV for 4th year onwards
 */
void tegra_dvfs_age_cpu(int cur_linear_age)
{
	int chip_linear_age;
	int chip_life;
	chip_linear_age = tegra_get_age();
	chip_life = cur_linear_age - chip_linear_age;

	/*For T37 and AP37*/
	if (tegra_cpu_speedo_id() == 12 || tegra_cpu_speedo_id() == 13) {
		if (chip_linear_age <= 0) {
			return;
		} else if (chip_life <= 12) {
			tegra_adjust_cpu_mvs(25);
		} else if (chip_life <= 36) {
			tegra_adjust_cpu_mvs(13);
		}
	}
}

#ifdef CONFIG_OF
static int __init of_read_dvfs_properties(struct device_node *np,
					struct dvfs *d, struct dvfs_data *data)
{
	bool auto_dvfs = !of_property_read_bool(np, "nvidia,manual-dvfs");
	const char *clk_name;
	int speedo_id;
	int process_id;

	if (of_property_read_string(np, "nvidia,clock-name", &clk_name))
		return -ENODATA;

	if (of_property_read_u32(np, "nvidia,speedo-id", &speedo_id))
		speedo_id = -1;

	if (of_property_read_u32(np, "nvidia,process-id", &process_id))
		process_id = -1;

	d->clk_name = clk_name;
	d->speedo_id = speedo_id;
	d->process_id = process_id;
	d->freqs_mult = KHZ;
	d->dvfs_rail = data->rail;
	d->millivolts = data->millivolts;
	d->auto_dvfs = auto_dvfs;

	return 0;
}

static int __init of_read_dvfs_data(struct device_node *np,
						struct dvfs_data *data)
{
	struct device_node *next = NULL;
	struct property *prop;
	int ret = 0;
	int count = 0;
	const __be32 *p;
	u32 u;

	if (!data->tables || !data->millivolts)
		return -ENODEV;

	of_property_for_each_u32(np, "nvidia,voltage-table", prop, p, u) {
		if (count == MAX_DVFS_FREQS)
			return -EOVERFLOW;
		data->millivolts[count] = u;
		count++;
	}

	if (!count)
		return -ENODATA;

	data->num_voltages = count;

	while (data->num_tables < MAX_DVFS_TABLES) {
		struct dvfs *d = NULL;
		int i = 0;

		next = of_get_next_child(np, next);
		if (!next)
			break;

		d = &data->tables[data->num_tables];
		ret = of_read_dvfs_properties(next, d, data);
		if (ret) {
			pr_warn("Failed to read DVFS table properties\n");
			return ret;
		}

		prop = of_find_property(next, "nvidia,frequencies", NULL);
		if (!prop) {
			pr_debug("Frequencies property not found in DT\n");
			return -ENODATA;
		}

		p = of_prop_next_u32(prop, NULL, &u);
		while (p && i < data->num_voltages) {
			d->freqs[i] = u;
			p = of_prop_next_u32(prop, p, &u);
			i++;
		}

		d->num_freqs = i;
		data->num_tables++;
	};

	return ret;
}

static int __init of_read_cpu_g_dvfs_data(struct device_node *np)
{
	int ret;
	int i;
	struct dvfs *d;
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int cpu_process_id = tegra_cpu_process_id();

	ret = of_read_dvfs_data(np, &of_cpu_data);
	if (ret) {
		pr_debug("Failed to read CPU DVFS tables from DT\n");
		return ret;
	}

	for (i = 0; i < of_cpu_data.num_tables; i++) {
		d = &of_cpu_data.tables[i];
		if (match_dvfs_one(d, cpu_speedo_id, cpu_process_id))
			break;
	}

	if (i >= of_cpu_data.num_tables)
		return -ENODATA;

	return 0;
}

static int __init of_read_cpu_0_dvfs_data(struct device_node *np)
{
	return of_read_dvfs_data(np, &of_cpu_0_data);
}

static int __init of_read_core_dvfs_data(struct device_node *np)
{
	return of_read_dvfs_data(np, &of_core_data);
}

static const __initconst struct of_device_id dvfs_match[] = {
	{ .compatible = "nvidia,tegra30-cpu-dvfs",
				.data = of_read_cpu_g_dvfs_data, },
	{ .compatible = "nvidia,tegra30-cpu0-dvfs",
				.data = of_read_cpu_0_dvfs_data, },
	{ .compatible = "nvidia,tegra30-core-dvfs",
				.data = of_read_core_dvfs_data, },
	{}
};
#endif /* CONFIG_OF */

void __init tegra3_init_dvfs(void)
{
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int soc_speedo_id = tegra_soc_speedo_id();
	int cpu_process_id = tegra_cpu_process_id();
	int core_process_id = tegra_core_process_id();

	int i;
	int core_nominal_mv_index;
	int cpu_nominal_mv_index;

#ifndef CONFIG_TEGRA_CORE_DVFS
	tegra_dvfs_core_disabled = true;
#endif
#ifndef CONFIG_TEGRA_CPU_DVFS
	tegra_dvfs_cpu_disabled = true;
#endif

#ifdef CONFIG_OF
	if (!of_tegra_dvfs_init(dvfs_match) && of_cpu_data.num_tables > 0 &&
				of_core_data.num_tables > 0) {
		cpu_dvfs_data = &of_cpu_data;
		cpu_0_dvfs_data = &of_cpu_0_data;
		core_dvfs_data = &of_core_data;
	} else {
#endif
		cpu_dvfs_data = &tegra30_cpu_data;
		cpu_0_dvfs_data = &tegra30_cpu_0_data;
		core_dvfs_data = &tegra30_core_data;
#ifdef CONFIG_OF
	}
#endif

	for (i = 0; i < cpu_dvfs_data->num_voltages; i++)
		cpu_millivolts_aged[i] = cpu_dvfs_data->millivolts[i];

	/*
	 * Find nominal voltages for core (1st) and cpu rails before rail
	 * init. Nominal voltage index in the scaling ladder will also be
	 * used to determine max dvfs frequency for the respective domains.
	 */
	core_nominal_mv_index = get_core_nominal_mv_index(soc_speedo_id);
	if (core_nominal_mv_index < 0) {
		tegra3_dvfs_rail_vdd_core.disabled = true;
		tegra_dvfs_core_disabled = true;
		core_nominal_mv_index = 0;
	}
	tegra3_dvfs_rail_vdd_core.nominal_millivolts =
		core_dvfs_data->millivolts[core_nominal_mv_index];

	cpu_nominal_mv_index = get_cpu_nominal_mv_index(
		cpu_speedo_id, cpu_process_id, &cpu_dvfs);

	BUG_ON((cpu_nominal_mv_index < 0) || (!cpu_dvfs));
	tegra3_dvfs_rail_vdd_cpu.nominal_millivolts =
		cpu_dvfs->millivolts[cpu_nominal_mv_index];

	/* Init rail structures and dependencies */
	tegra_dvfs_init_rails(tegra3_dvfs_rails, ARRAY_SIZE(tegra3_dvfs_rails));
	tegra_dvfs_add_relationships(tegra3_dvfs_relationships,
		ARRAY_SIZE(tegra3_dvfs_relationships));

	/* Search core dvfs table for speedo/process matching entries and
	   initialize dvfs-ed clocks */
	for (i = 0; i <  core_dvfs_data->num_tables; i++) {
		struct dvfs *d = &core_dvfs_data->tables[i];
		if (!match_dvfs_one(d, soc_speedo_id, core_process_id))
			continue;
		init_dvfs_one(d, core_nominal_mv_index);
	}

	/* Initialize matching cpu dvfs entry already found when nominal
	   voltage was determined */
	init_dvfs_one(cpu_dvfs, cpu_nominal_mv_index);

	/* Initialize alternative cold zone and single core tables */
	init_dvfs_cold(cpu_dvfs, cpu_nominal_mv_index);
	init_cpu_0_dvfs(cpu_dvfs);

	/* Finally disable dvfs on rails if necessary */
	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra3_dvfs_rail_vdd_core);
	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra3_dvfs_rail_vdd_cpu);

	pr_info("tegra dvfs: VDD_CPU nominal %dmV, scaling %s\n",
		tegra3_dvfs_rail_vdd_cpu.nominal_millivolts,
		tegra_dvfs_cpu_disabled ? "disabled" : "enabled");
	pr_info("tegra dvfs: VDD_CORE nominal %dmV, scaling %s\n",
		tegra3_dvfs_rail_vdd_core.nominal_millivolts,
		tegra_dvfs_core_disabled ? "disabled" : "enabled");
}

int tegra_cpu_dvfs_alter(int edp_thermal_index, const cpumask_t *cpus,
			  bool before_clk_update, int cpu_event)
{
	bool cpu_warm = !!edp_thermal_index;
	unsigned int n = cpumask_weight(cpus);
	unsigned long *alt_freqs = cpu_warm ?
		(n > 1 ? NULL : cpu_0_freqs) : cpu_cold_freqs;

	if (cpu_event || (cpu_warm == before_clk_update)) {
		int ret = tegra_dvfs_alt_freqs_set(cpu_dvfs, alt_freqs);
		if (ret) {
			pr_err("tegra dvfs: failed to set alternative dvfs on "
			       "%u %s CPUs\n", n, cpu_warm ? "warm" : "cold");
			return ret;
		}
	}
	return 0;
}

int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{
	int ret = 0;

	if (tegra_emc_get_dram_type() != DRAM_TYPE_DDR3)
		return ret;

	if (((&tegra3_dvfs_rail_vdd_core == rail) &&
	     (rail->nominal_millivolts > TEGRA_EMC_BRIDGE_MVOLTS_MIN)) ||
	    ((&tegra3_dvfs_rail_vdd_cpu == rail) &&
	     (tegra3_get_core_floor_mv(rail->nominal_millivolts) >
	      TEGRA_EMC_BRIDGE_MVOLTS_MIN))) {
		struct clk *bridge = tegra_get_clock_by_name("bridge.emc");
		BUG_ON(!bridge);

		ret = tegra_clk_prepare_enable(bridge);
		pr_info("%s: %s: %s bridge.emc\n", __func__,
			rail->reg_id, ret ? "failed to enable" : "enabled");
	}
	return ret;
}

int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{
	if (tegra_emc_get_dram_type() != DRAM_TYPE_DDR3)
		return 0;

	if (((&tegra3_dvfs_rail_vdd_core == rail) &&
	     (rail->nominal_millivolts > TEGRA_EMC_BRIDGE_MVOLTS_MIN)) ||
	    ((&tegra3_dvfs_rail_vdd_cpu == rail) &&
	     (tegra3_get_core_floor_mv(rail->nominal_millivolts) >
	      TEGRA_EMC_BRIDGE_MVOLTS_MIN))) {
		struct clk *bridge = tegra_get_clock_by_name("bridge.emc");
		BUG_ON(!bridge);

		tegra_clk_disable_unprepare(bridge);
		pr_info("%s: %s: disabled bridge.emc\n",
			__func__, rail->reg_id);
	}
	return 0;
}

/* Core voltage and bus cap object and tables */
static struct kobject *cap_kobj;

/* Arranged in order required for enabling/lowering the cap */
static struct core_dvfs_cap_table tegra3_core_cap_table[] = {
	{ .cap_name = "cap.cbus" },
	{ .cap_name = "cap.sclk" },
	{ .cap_name = "cap.emc" },
};

static struct core_bus_limit_table tegra3_bus_cap_table[] = {
	{ .limit_clk_name = "cap.profile.cbus",
	  .refcnt_attr = {.attr = {.name = "cbus_cap_state", .mode = 0644} },
	  .level_attr  = {.attr = {.name = "cbus_cap_level", .mode = 0644} },
	},
};

static int __init tegra3_dvfs_init_core_cap(void)
{
	int ret;

	cap_kobj = kobject_create_and_add("tegra_cap", kernel_kobj);
	if (!cap_kobj) {
		pr_err("tegra3_dvfs: failed to create sysfs cap object\n");
		return 0;
	}

	ret = tegra_init_shared_bus_cap(
		tegra3_bus_cap_table, ARRAY_SIZE(tegra3_bus_cap_table),
		cap_kobj);
	if (ret) {
		pr_err("tegra3_dvfs: failed to init bus cap interface (%d)\n",
		       ret);
		kobject_del(cap_kobj);
		return 0;
	}

	ret = tegra_init_core_cap(
		tegra3_core_cap_table, ARRAY_SIZE(tegra3_core_cap_table),
		core_millivolts, ARRAY_SIZE(core_millivolts), cap_kobj);

	if (ret) {
		pr_err("tegra3_dvfs: failed to init voltage cap interface (%d)\n",
		       ret);
		kobject_del(cap_kobj);
		return 0;
	}
	pr_info("tegra dvfs: tegra sysfs cap interface is initialized\n");

	return 0;
}
late_initcall(tegra3_dvfs_init_core_cap);
