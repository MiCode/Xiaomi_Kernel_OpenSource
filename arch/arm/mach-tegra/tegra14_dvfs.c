/*
 * arch/arm/mach-tegra/tegra14_dvfs.c
 *
 * Copyright (c) 2012-2013 NVIDIA Corporation. All rights reserved.
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

#include "clock.h"
#include "dvfs.h"
#include "board.h"
#include "tegra_cl_dvfs.h"
#include "tegra_core_sysfs_limits.h"

static bool tegra_dvfs_cpu_disabled;
static bool tegra_dvfs_core_disabled;

#define KHZ 1000
#define MHZ 1000000

/* FIXME: need tegra14 step */
#define VDD_SAFE_STEP			100

static int vdd_core_vmin_trips_table[MAX_THERMAL_LIMITS] = { 20, };
static int vdd_core_therm_floors_table[MAX_THERMAL_LIMITS] = { 900, };

static int vdd_cpu_vmax_trips_table[MAX_THERMAL_LIMITS] = { 62,   72, };
static int vdd_cpu_therm_caps_table[MAX_THERMAL_LIMITS] = { 1180, 1150, };

static struct tegra_cooling_device cpu_vmax_cdev = {
	.cdev_type = "cpu_hot",
};

static struct tegra_cooling_device cpu_vmin_cdev = {
	.cdev_type = "cpu_cold",
};

static struct tegra_cooling_device core_vmin_cdev = {
	.cdev_type = "core_cold",
};

static struct dvfs_rail tegra14_dvfs_rail_vdd_cpu = {
	.reg_id = "vdd_cpu",
	.max_millivolts = 1400,
	.min_millivolts = 800,
	.step = VDD_SAFE_STEP,
	.jmp_to_zero = true,
	.vmin_cdev = &cpu_vmin_cdev,
	.vmax_cdev = &cpu_vmax_cdev,
};

static struct dvfs_rail tegra14_dvfs_rail_vdd_core = {
	.reg_id = "vdd_core",
	.max_millivolts = 1400,
	.min_millivolts = 800,
	.step = VDD_SAFE_STEP,
	.vmin_cdev = &core_vmin_cdev,
};

static struct dvfs_rail *tegra14_dvfs_rails[] = {
	&tegra14_dvfs_rail_vdd_cpu,
	&tegra14_dvfs_rail_vdd_core,
};

/* default cvb alignment on Tegra14 - 10mV */
int __attribute__((weak)) tegra_get_cvb_alignment_uV(void)
{
	return 10000;
}

int __attribute__((weak)) tegra_get_core_cvb_alignment_uV(void)
{
	return  6250;
}

/* CPU DVFS tables */
static struct cpu_cvb_dvfs cpu_cvb_dvfs_table[] = {
	{
		.speedo_id = 0,
		.process_id = -1,
		.dfll_tune_data  = {
			.tune0		= 0x0041061F,
			.tune0_high_mv	= 0x0041001F,
			.tune1		= 0x00000007,
			.droop_rate_min = 1000000,
			.tune_high_min_millivolts = 900,
			.tune_high_margin_mv = 40,
			.min_millivolts = 800,
		},
		.max_mv = 1230,
		.freqs_mult = KHZ,
		.speedo_scale = 100,
		.voltage_scale = 1000,
		.cvb_table = {
			/*f       dfll: c0,     c1,   c2  pll:  c0,   c1,    c2 */
			{ 408000, { 2167974,  -118652,   2434}, {  812396,    0,    0} },
			{ 510000, { 2210727,  -119982,   2434}, {  824163,    0,    0} },
			{ 612000, { 2255116,  -121322,   2434}, {  837498,    0,    0} },
			{ 714000, { 2301137,  -122662,   2434}, {  852671,    0,    0} },
			{ 816000, { 2348792,  -124002,   2434}, {  869680,    0,    0} },
			{ 918000, { 2398075,  -125332,   2434}, {  888794,    0,    0} },
			{1020000, { 2448994,  -126672,   2434}, {  909476,    0,    0} },
			{1122000, { 2501546,  -128012,   2434}, {  931995,    0,    0} },
			{1224000, { 2555727,  -129342,   2434}, {  956619,    0,    0} },
			{1326000, { 2611544,  -130682,   2434}, {  982812,    0,    0} },
			{1428000, { 2668994,  -132022,   2434}, { 1010841,    0,    0} },
			{1530000, { 2728076,  -133362,   2434}, { 1040706,    0,    0} },
			{1632000, { 2788787,  -134692,   2434}, { 1072677,    0,    0} },
			{1734000, { 2851134,  -136032,   2434}, { 1106216,    0,    0} },
			{      0, {      0,      0,   0}, {      0,    0,    0} },
		},
		.vmin_trips_table = { 20 },
		.therm_floors_table = { 900 },
	},
	{
		.speedo_id = 1,
		.process_id = 1,
		.dfll_tune_data  = {
			.tune0		= 0x0041061F,
			.tune0_high_mv	= 0x0041001F,
			.tune1		= 0x00000007,
			.droop_rate_min = 1000000,
			.tune_high_min_millivolts = 900,
			.tune_high_margin_mv = 40,
			.min_millivolts = 800,
		},
		.max_mv = 1230,
		.freqs_mult = KHZ,
		.speedo_scale = 100,
		.voltage_scale = 1000,
		.cvb_table = {
			/*f       dfll: c0,     c1,   c2  pll:  c0,   c1,    c2 */
			{ 408000, { 2167974,  -118652,   2434}, {  812396,    0,    0} },
			{ 510000, { 2210727,  -119982,   2434}, {  824163,    0,    0} },
			{ 612000, { 2255116,  -121322,   2434}, {  837498,    0,    0} },
			{ 714000, { 2301137,  -122662,   2434}, {  852671,    0,    0} },
			{ 816000, { 2348792,  -124002,   2434}, {  869680,    0,    0} },
			{ 918000, { 2398075,  -125332,   2434}, {  888794,    0,    0} },
			{1020000, { 2448994,  -126672,   2434}, {  909476,    0,    0} },
			{1122000, { 2501546,  -128012,   2434}, {  931995,    0,    0} },
			{1224000, { 2555727,  -129342,   2434}, {  956619,    0,    0} },
			{1326000, { 2611544,  -130682,   2434}, {  982812,    0,    0} },
			{1428000, { 2668994,  -132022,   2434}, { 1010841,    0,    0} },
			{1530000, { 2728076,  -133362,   2434}, { 1040706,    0,    0} },
			{1632000, { 2788787,  -134692,   2434}, { 1072677,    0,    0} },
			{1734000, { 2851134,  -136032,   2434}, { 1106216,    0,    0} },
			{1836000, { 2915114,  -137372,   2434}, { 1141591,    0,    0} },
			{1938000, { 2980727,  -138712,   2434}, { 1178803,    0,    0} },
			{2014500, { 3030673,  -139702,   2434}, { 1207951,    0,    0} },
			{2116500, { 3099135,  -141042,   2434}, { 1248368,    0,    0} },
			{      0, {      0,      0,   0}, {      0,    0,    0} },
		},
		.vmin_trips_table = { 20 },
		.therm_floors_table = { 900 },
	},
};

static int cpu_millivolts[MAX_DVFS_FREQS];
static int cpu_dfll_millivolts[MAX_DVFS_FREQS];

static struct dvfs cpu_dvfs = {
	.clk_name	= "cpu_g",
	.millivolts	= cpu_millivolts,
	.dfll_millivolts = cpu_dfll_millivolts,
	.auto_dvfs	= true,
	.dvfs_rail	= &tegra14_dvfs_rail_vdd_cpu,
};

/* Core DVFS tables */
static const int core_millivolts[MAX_DVFS_FREQS] = {
	800, 850, 900, 950, 1000, 1050, 1100, 1150, 1200, 1230};

#define CORE_DVFS(_clk_name, _speedo_id, _process_id, _auto, _mult, _freqs...)	\
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= _process_id,			\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.dvfs_rail	= &tegra14_dvfs_rail_vdd_core,	\
	}

#define OVRRD_DVFS(_clk_name, _speedo_id, _process_id, _auto, _mult, _freqs...)	\
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= _process_id,			\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.can_override	= true,				\
		.dvfs_rail	= &tegra14_dvfs_rail_vdd_core,	\
	}


static struct dvfs core_dvfs_table[] = {
	/* Core voltages (mV):		             800,    850,    900,    950,   1000,   1050,   1100,   1150,   1200,   1230 */
	CORE_DVFS("cpu_lp",   0,  0, 1, KHZ,      249600, 364800, 460800, 556800, 633600, 691200, 729600, 768000, 768000, 768000),
	CORE_DVFS("cpu_lp",   0,  1, 1, KHZ,      307200, 393600, 489600, 556800, 652800, 729600, 768000, 768000, 768000, 768000),
	CORE_DVFS("cpu_lp",   1,  1, 1, KHZ,      307200, 393600, 489600, 556800, 652800, 729600, 768000, 806400, 806400, 806400),

#ifndef CONFIG_TEGRA_DUAL_CBUS
	CORE_DVFS("3d",       0,  0, 1, KHZ,      153600, 230400, 307200, 384000, 499200, 556800, 600000, 600000, 600000, 600000),
	CORE_DVFS("2d",       0,  0, 1, KHZ,      153600, 230400, 307200, 384000, 499200, 556800, 600000, 600000, 600000, 600000),
	CORE_DVFS("epp",      0,  0, 1, KHZ,      153600, 230400, 307200, 384000, 499200, 556800, 600000, 600000, 600000, 600000),
	CORE_DVFS("3d",       0,  1, 1, KHZ,      192000, 268800, 345600, 384000, 499200, 556800, 600000, 600000, 600000, 600000),
	CORE_DVFS("2d",       0,  1, 1, KHZ,      192000, 268800, 345600, 384000, 499200, 556800, 600000, 600000, 600000, 600000),
	CORE_DVFS("epp",      0,  1, 1, KHZ,      192000, 268800, 345600, 384000, 499200, 556800, 600000, 600000, 600000, 600000),
	CORE_DVFS("3d",       1,  1, 1, KHZ,      192000, 268800, 345600, 384000, 499200, 556800, 595200, 672000, 710400, 748800),
	CORE_DVFS("2d",       1,  1, 1, KHZ,      192000, 268800, 345600, 384000, 499200, 556800, 595200, 672000, 710400, 748800),
	CORE_DVFS("epp",      1,  1, 1, KHZ,      192000, 268800, 345600, 384000, 499200, 556800, 595200, 672000, 710400, 748800),
#endif
	CORE_DVFS("se",       0,  0, 1, KHZ,      115200, 192000, 249600, 326400, 364800, 441600, 480000, 518400, 518400, 518400),
	CORE_DVFS("vde",      0,  0, 1, KHZ,      115200, 192000, 249600, 326400, 364800, 441600, 480000, 518400, 518400, 518400),
	CORE_DVFS("se",       0,  1, 1, KHZ,      153600, 211200, 288000, 345600, 403200, 460800, 499200, 518400, 518400, 518400),
	CORE_DVFS("vde",      0,  1, 1, KHZ,      153600, 211200, 288000, 345600, 403200, 460800, 499200, 518400, 518400, 518400),
	CORE_DVFS("se",       1,  1, 1, KHZ,      153600, 211200, 288000, 345600, 403200, 460800, 499200, 537600, 537600, 537600),
	CORE_DVFS("vde",      1,  1, 1, KHZ,      153600, 211200, 288000, 345600, 403200, 460800, 499200, 537600, 537600, 537600),

	CORE_DVFS("msenc",    0,  0, 1, KHZ,       68000, 102000, 136000, 204000, 204000, 204000, 384000, 408000, 408000, 408000),
	CORE_DVFS("msenc",    0,  1, 1, KHZ,       81600, 136000, 204000, 204000, 204000, 204000, 408000, 408000, 408000, 408000),
	CORE_DVFS("msenc",    1,  1, 1, KHZ,       81600, 136000, 204000, 204000, 204000, 204000, 408000, 408000, 408000, 408000),

	CORE_DVFS("tsec",     0,  0, 1, KHZ,      136000, 204000, 204000, 408000, 408000, 408000, 408000, 408000, 408000, 408000),
	CORE_DVFS("tsec",     0,  1, 1, KHZ,      136000, 204000, 384000, 408000, 408000, 408000, 408000, 408000, 408000, 408000),
	CORE_DVFS("tsec",     1,  1, 1, KHZ,      136000, 204000, 384000, 408000, 408000, 408000, 408000, 408000, 408000, 408000),

	CORE_DVFS("host1x",   0,  0, 1, KHZ,      102000, 102000, 204000, 204000, 204000, 204000, 384000, 408000, 408000, 408000),
	CORE_DVFS("host1x",   0,  1, 1, KHZ,      136000, 136000, 204000, 204000, 204000, 384000, 384000, 408000, 408000, 408000),
	CORE_DVFS("host1x",   1,  1, 1, KHZ,      136000, 136000, 204000, 204000, 204000, 384000, 384000, 408000, 408000, 408000),

	CORE_DVFS("vi",      -1, -1, 1, KHZ,      136000, 204000, 204000, 408000, 408000, 408000, 408000, 408000, 408000, 408000),
	CORE_DVFS("isp",     -1, -1, 1, KHZ,      136000, 204000, 204000, 408000, 408000, 408000, 408000, 408000, 408000, 408000),

	CORE_DVFS("sbus",     0,  0, 1, KHZ,      102000, 102000, 102000, 102000, 204000, 384000, 384000, 408000, 408000, 408000),
	CORE_DVFS("sbus",     0,  1, 1, KHZ,      136000, 136000, 204000, 204000, 204000, 384000, 384000, 408000, 408000, 408000),
	CORE_DVFS("sbus",     1,  1, 1, KHZ,      136000, 136000, 204000, 204000, 204000, 384000, 384000, 408000, 408000, 408000),

	CORE_DVFS("emc",     -1, -1, 1, KHZ,           1,      1,      1,      1,      1,      1,1066000,1066000,1066000,1066000),

#ifdef CONFIG_TEGRA_DUAL_CBUS
	CORE_DVFS("c3bus",    0,  0, 1, KHZ,      115200, 192000, 249600, 326400, 364800, 441600, 480000, 518400, 518400, 518400),
	CORE_DVFS("c3bus",    0,  1, 1, KHZ,      153600, 211200, 288000, 345600, 403200, 460800, 499200, 518400, 518400, 518400),
	CORE_DVFS("c3bus",    1,  1, 1, KHZ,      153600, 211200, 288000, 345600, 403200, 460800, 499200, 537600, 537600, 537600),
#else
	CORE_DVFS("cbus",     0,  0, 1, KHZ,      115200, 192000, 249600, 326400, 364800, 441600, 480000, 518400, 518400, 518400),
	CORE_DVFS("cbus",     0,  1, 1, KHZ,      153600, 211200, 288000, 345600, 403200, 460800, 499200, 518400, 518400, 518400),
	CORE_DVFS("cbus",     1,  1, 1, KHZ,      153600, 211200, 288000, 345600, 403200, 460800, 499200, 537600, 537600, 537600),
#endif
	/* Core voltages (mV):		             800,    850,    900,    950,   1000,   1050,   1100,   1150,   1200,   1230 */
	/* Clock limits for I/O peripherals */
	CORE_DVFS("cilab",  -1, -1, 1, KHZ,       102000, 102000, 102000, 102000, 102000, 114700, 136000, 136000, 136000, 136000),
	CORE_DVFS("cilcd",  -1, -1, 1, KHZ,       102000, 102000, 102000, 102000, 102000, 114700, 136000, 136000, 136000, 136000),
	CORE_DVFS("cile",   -1, -1, 1, KHZ,       102000, 102000, 102000, 102000, 102000, 114700, 136000, 136000, 136000, 136000),
	CORE_DVFS("dsia",  -1, -1, 1, KHZ,        252000, 431000, 431000, 431000, 431000, 431000, 431000, 431000, 431000, 431000),
	CORE_DVFS("dsib",  -1, -1, 1, KHZ,        252000, 431000, 431000, 431000, 431000, 431000, 431000, 431000, 431000, 431000),
	CORE_DVFS("hdmi",   -1, -1, 1, KHZ,       111300, 111300, 111300, 135600, 173200, 212300, 270000, 270000, 270000, 270000),

	CORE_DVFS("pll_m",   0,  0, 1, KHZ,       408000, 667000, 800000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c",   0,  0, 1, KHZ,       408000, 667000, 800000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c2",  0,  0, 1, KHZ,       408000, 667000, 800000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c3",  0,  0, 1, KHZ,       408000, 667000, 800000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_m",   0,  1, 1, KHZ,       408000, 667000, 800000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c",   0,  1, 1, KHZ,       408000, 667000, 800000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c2",  0,  1, 1, KHZ,       408000, 667000, 800000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c3",  0,  1, 1, KHZ,       408000, 667000, 800000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_m",   1,  1, 1, KHZ,       533000, 667000, 933000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c",   1,  1, 1, KHZ,       533000, 667000, 933000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c2",  1,  1, 1, KHZ,       533000, 667000, 933000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),
	CORE_DVFS("pll_c3",  1,  1, 1, KHZ,       533000, 667000, 933000,1066000,1066000,1066000,1066000,1066000,1066000,1066000),

	/* Core voltages (mV):» »                    800,    850,    900,    950,   1000,   1050,   1100,   1150,   1200,   1230 */
	/* Clock limits for IO Peripherals */
	CORE_DVFS("sbc1",   -1, -1, 1, KHZ,        33000,  33000,  33000,  30000,  30000,  50000,  50000,  50000,  50000,  50000),
	CORE_DVFS("sbc2",   -1, -1, 1, KHZ,        33000,  33000,  33000,  30000,  30000,  50000,  50000,  50000,  50000,  50000),
	CORE_DVFS("sbc3",   -1, -1, 1, KHZ,        33000,  33000,  33000,  30000,  30000,  50000,  50000,  50000,  50000,  50000),

	OVRRD_DVFS("sdmmc1", -1, -1, 1, KHZ,       51000,  51000,  51000,  51000,  51000,  51000, 136000, 136000, 136000, 204000),
	OVRRD_DVFS("sdmmc3", -1, -1, 1, KHZ,       51000,  51000,  51000,  51000,  51000,  51000, 136000, 136000, 136000, 204000),
	OVRRD_DVFS("sdmmc4", -1, -1, 1, KHZ,       51000,  51000,  51000,  51000,  51000,  51000, 136000, 136000, 136000, 192000),

	/*
	 * The clock rate for the display controllers that determines the
	 * necessary core voltage depends on a divider that is internal
	 * to the display block. Disable auto-dvfs on the display clocks,
	 * and let the display driver call tegra_dvfs_set_rate manually
	 */
	CORE_DVFS("disp1",  -1, -1, 0, KHZ,        74250, 165000, 165000, 165000, 165000, 165000, 165000, 165000, 165000, 165000),
	CORE_DVFS("disp2",  -1, -1, 0, KHZ,        74250, 165000, 165000, 165000, 165000, 165000, 165000, 165000, 165000, 165000),
};

/* C2BUS dvfs tables */
static struct core_cvb_dvfs c2bus_cvb_dvfs_table[] = {
	{
		.speedo_id =  0,
		.process_id = 0,
		.freqs_mult = KHZ,
		.speedo_scale = 100,
		.voltage_scale = 1000,
		.cvb_table = {
			/*f       dfll  pll:   c0,     c1,   c2 */
			{ 153600, {  }, {  800000,      0,   0}, },
			{ 230400, {  }, {  850000,      0,   0}, },
			{ 307200, {  }, {  900000,      0,   0}, },
			{ 384000, {  }, {  950000,      0,   0}, },
			{ 499200, {  }, { 1000000,      0,   0}, },
			{ 556800, {  }, { 1050000,      0,   0}, },
			{ 600000, {  }, { 1100000,      0,   0}, },
			{ 600000, {  }, { 1150000,      0,   0}, },
			{ 600000, {  }, { 1200000,      0,   0}, },
			{ 600000, {  }, { 1230000,      0,   0}, },
			{      0, {  }, {       0,      0,   0}, },
		},
	},
	{
		.speedo_id =  0,
		.process_id = 1,
		.freqs_mult = KHZ,
		.speedo_scale = 100,
		.voltage_scale = 1000,
		.cvb_table = {
			/*f       dfll  pll:   c0,     c1,   c2 */
			{ 192000, {  }, {  800000,      0,   0}, },
			{ 268800, {  }, {  850000,      0,   0}, },
			{ 345600, {  }, {  900000,      0,   0}, },
			{ 384000, {  }, {  950000,      0,   0}, },
			{ 499200, {  }, { 1000000,      0,   0}, },
			{ 556800, {  }, { 1050000,      0,   0}, },
			{ 600000, {  }, { 1100000,      0,   0}, },
			{ 600000, {  }, { 1150000,      0,   0}, },
			{ 600000, {  }, { 1200000,      0,   0}, },
			{ 600000, {  }, { 1230000,      0,   0}, },
			{      0, {  }, {       0,      0,   0}, },
		},
	},
	{
		.speedo_id =  1,
		.process_id = 1,
		.freqs_mult = KHZ,
		.speedo_scale = 100,
		.voltage_scale = 1000,
		.cvb_table = {
			/*f       dfll  pll:   c0,     c1,   c2 */
			{ 192000, {  }, {  800000,      0,   0}, },
			{ 268800, {  }, {  850000,      0,   0}, },
			{ 345600, {  }, {  900000,      0,   0}, },
			{ 384000, {  }, {  950000,      0,   0}, },
			{ 499200, {  }, { 1000000,      0,   0}, },
			{ 556800, {  }, { 1050000,      0,   0}, },
			{ 595200, {  }, { 1100000,      0,   0}, },
			{ 672000, {  }, { 1150000,      0,   0}, },
			{ 710400, {  }, { 1200000,      0,   0}, },
			{ 748800, {  }, { 1230000,      0,   0}, },
			{      0, {  }, {      0,       0,   0}, },
		},
	},
};

static int c2bus_millivolts[MAX_DVFS_FREQS];
static struct dvfs c2bus_dvfs_table[] = {
	{ .clk_name = "3d", },
	{ .clk_name = "2d", },
	{ .clk_name = "epp", },
	{ .clk_name = "c2bus", },	/* must be the last */
};

int tegra_dvfs_disable_core_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra14_dvfs_rail_vdd_core);
	else
		tegra_dvfs_rail_enable(&tegra14_dvfs_rail_vdd_core);

	return 0;
}

int tegra_dvfs_disable_cpu_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra14_dvfs_rail_vdd_cpu);
	else
		tegra_dvfs_rail_enable(&tegra14_dvfs_rail_vdd_cpu);

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

static bool __init can_update_max_rate(struct clk *c, struct dvfs *d)
{
	/* Don't update manual dvfs clocks */
	if (!d->auto_dvfs)
		return false;

	/*
	 * Don't update EMC shared bus, since EMC dvfs is board dependent: max
	 * rate and EMC scaling frequencies are determined by tegra BCT (flashed
	 * together with the image) and board specific EMC DFS table; we will
	 * check the scaling ladder against nominal core voltage when the table
	 * is loaded (and if on particular board the table is not loaded, EMC
	 * scaling is disabled).
	 */
	if (c->ops->shared_bus_update && (c->flags & PERIPH_EMC_ENB))
		return false;

	/*
	 * Don't update shared cbus, and don't propagate common cbus dvfs
	 * limit down to shared users, but set maximum rate for each user
	 * equal to the respective client limit.
	 */
	if (c->ops->shared_bus_update && (c->flags & PERIPH_ON_CBUS)) {
		struct clk *user;
		unsigned long rate;

		list_for_each_entry(
			user, &c->shared_bus_list, u.shared_bus_user.node) {
			if (user->u.shared_bus_user.client) {
				rate = user->u.shared_bus_user.client->max_rate;
				user->max_rate = rate;
				user->u.shared_bus_user.rate = rate;
			}
		}
		return false;
	}

	/* Other, than EMC and cbus, auto-dvfs clocks can be updated */
	return true;
}

static void __init init_dvfs_one(struct dvfs *d, int max_freq_index)
{
	int ret;
	struct clk *c = tegra_get_clock_by_name(d->clk_name);

	if (!c) {
		pr_debug("tegra14_dvfs: no clock found for %s\n",
			d->clk_name);
		return;
	}

	/* Update max rate for auto-dvfs clocks, with shared bus exceptions */
	if (can_update_max_rate(c, d)) {
		BUG_ON(!d->freqs[max_freq_index]);
		tegra_init_max_rate(
			c, d->freqs[max_freq_index] * d->freqs_mult);
	}
	d->max_millivolts = d->dvfs_rail->nominal_millivolts;

	ret = tegra_enable_dvfs_on_clk(c, d);
	if (ret)
		pr_err("tegra14_dvfs: failed to enable dvfs on %s\n", c->name);
}

static bool __init match_dvfs_one(const char *name,
	int dvfs_speedo_id, int dvfs_process_id,
	int speedo_id, int process_id)
{
	if ((dvfs_process_id != -1 && dvfs_process_id != process_id) ||
		(dvfs_speedo_id != -1 && dvfs_speedo_id != speedo_id)) {
		pr_debug("tegra14_dvfs: rejected %s speedo %d, process %d\n",
			 name, dvfs_speedo_id, dvfs_process_id);
		return false;
	}
	return true;
}

/* cvb_mv = ((c2 * speedo / s_scale + c1) * speedo / s_scale + c0) / v_scale */
static inline int get_cvb_voltage(int speedo, int s_scale,
				  struct cvb_dvfs_parameters *cvb)
{
	/* apply only speedo scale: output mv = cvb_mv * v_scale */
	int mv;
	mv = DIV_ROUND_CLOSEST(cvb->c2 * speedo, s_scale);
	mv = DIV_ROUND_CLOSEST((mv + cvb->c1) * speedo, s_scale) + cvb->c0;
	return mv;
}

static inline int round_cvb_voltage(int mv, int v_scale)
{
	/* combined: apply voltage scale and round to cvb alignment step */
	int cvb_align_step_uv = tegra_get_cvb_alignment_uV();

	return DIV_ROUND_UP(mv * 1000, v_scale * cvb_align_step_uv) *
		cvb_align_step_uv / 1000;
}

static inline int round_core_cvb_voltage(int mv, int v_scale)
{
	/* combined: apply voltage scale and round to cvb alignment step */
	int cvb_align_step_uv = tegra_get_core_cvb_alignment_uV();

	return DIV_ROUND_UP(mv * 1000, v_scale * cvb_align_step_uv) *
		cvb_align_step_uv / 1000;
}

static int __init set_cpu_dvfs_data(
	struct cpu_cvb_dvfs *d, struct dvfs *cpu_dvfs, int *max_freq_index)
{
	int i, j, mv, dfll_mv, min_dfll_mv;
	unsigned long fmax_at_vmin = 0;
	unsigned long fmax_pll_mode = 0;
	unsigned long fmin_use_dfll = 0;
	struct cvb_dvfs_table *table = NULL;
	int speedo = tegra_cpu_speedo_value();

	min_dfll_mv = d->dfll_tune_data.min_millivolts;
	min_dfll_mv =  round_cvb_voltage(min_dfll_mv * 1000, 1000);
	d->max_mv = round_cvb_voltage(d->max_mv * 1000, 1000);
	BUG_ON(min_dfll_mv < tegra14_dvfs_rail_vdd_cpu.min_millivolts);

	/*
	 * Use CVB table to fill in CPU dvfs frequencies and voltages. Each
	 * CVB entry specifies CPU frequency and CVB coefficients to calculate
	 * the respective voltage when either DFLL or PLL is used as CPU clock
	 * source.
	 *
	 * Minimum voltage limit is applied only to DFLL source. For PLL source
	 * voltage can go as low as table specifies. Maximum voltage limit is
	 * applied to both sources, but differently: directly clip voltage for
	 * DFLL, and limit maximum frequency for PLL.
	 */
	for (i = 0, j = 0; i < MAX_DVFS_FREQS; i++) {
		table = &d->cvb_table[i];
		if (!table->freq)
			break;

		dfll_mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_dfll_param);
		dfll_mv = round_cvb_voltage(dfll_mv, d->voltage_scale);

		mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_pll_param);
		mv = round_cvb_voltage(mv, d->voltage_scale);

		/*
		 * Check maximum frequency at minimum voltage for dfll source;
		 * round down unless all table entries are above Vmin, then use
		 * the 1st entry as is.
		 */
		dfll_mv = max(dfll_mv, min_dfll_mv);
		if (dfll_mv > min_dfll_mv) {
			if (!j)
				fmax_at_vmin = table->freq;
			if (!fmax_at_vmin)
				fmax_at_vmin = cpu_dvfs->freqs[j - 1];
		}

		/* Clip maximum frequency at maximum voltage for pll source */
		if (mv > d->max_mv) {
			if (!j)
				break;	/* 1st entry already above Vmax */
			if (!fmax_pll_mode)
				fmax_pll_mode = cpu_dvfs->freqs[j - 1];
		}

		/* Minimum rate with pll source voltage above dfll Vmin */
		if ((mv >= min_dfll_mv) && (!fmin_use_dfll))
			fmin_use_dfll = table->freq;

		/* fill in dvfs tables */
		cpu_dvfs->freqs[j] = table->freq;
		cpu_dfll_millivolts[j] = min(dfll_mv, d->max_mv);
		cpu_millivolts[j] = mv;
		j++;

		/*
		 * "Round-up" frequency list cut-off (keep first entry that
		 *  exceeds max voltage - the voltage limit will be enforced
		 *  anyway, so when requested this frequency dfll will settle
		 *  at whatever high frequency it can on the particular chip)
		 */
		if (dfll_mv > d->max_mv)
			break;
	}
	/* Table must not be empty, must have at least one entry above Vmin */
	if (!i || !j || !fmax_at_vmin) {
		pr_err("tegra14_dvfs: invalid cpu dvfs table\n");
		return -ENOENT;
	}

	/* In the dfll operating range dfll voltage at any rate should be
	   better (below) than pll voltage */
	if (!fmin_use_dfll || (fmin_use_dfll > fmax_at_vmin)) {
		WARN(1, "tegra14_dvfs: pll voltage is below dfll in the dfll"
			" operating range\n");
		fmin_use_dfll = fmax_at_vmin;
	}

	/* dvfs tables are successfully populated - fill in the rest */
	cpu_dvfs->speedo_id = d->speedo_id;
	cpu_dvfs->process_id = d->process_id;
	cpu_dvfs->freqs_mult = d->freqs_mult;
	cpu_dvfs->dvfs_rail->nominal_millivolts = min(d->max_mv,
		max(cpu_millivolts[j - 1], cpu_dfll_millivolts[j - 1]));
	*max_freq_index = j - 1;

	cpu_dvfs->dfll_data = d->dfll_tune_data;
	cpu_dvfs->dfll_data.max_rate_boost = fmax_pll_mode ?
		(cpu_dvfs->freqs[j - 1] - fmax_pll_mode) * d->freqs_mult : 0;
	cpu_dvfs->dfll_data.out_rate_min = fmax_at_vmin * d->freqs_mult;
	cpu_dvfs->dfll_data.use_dfll_rate_min = fmin_use_dfll * d->freqs_mult;
	cpu_dvfs->dfll_data.min_millivolts = min_dfll_mv;
	return 0;
}

static int __init set_c2bus_dvfs_data(
	struct core_cvb_dvfs *d, struct dvfs *c2bus_dvfs, int *max_freq_index)
{
	int i, j, mv, min_mv, max_mv;
	struct cvb_dvfs_table *table = NULL;
	int speedo = 0; /* FIXME: tegra_core_speedo_value(); */

	min_mv = round_core_cvb_voltage(core_millivolts[0] * 1000, 1000);
	max_mv = tegra14_dvfs_rail_vdd_core.nominal_millivolts;

	/*
	 * Use CVB table to fill in c2bus dvfs frequencies and voltages. Each
	 * CVB entry specifies c2bus frequency and CVB coefficients to calculate
	 * the respective voltage.
	 */
	for (i = 0, j = 0; i < MAX_DVFS_FREQS; i++) {
		table = &d->cvb_table[i];
		if (!table->freq)
			break;

		mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_pll_param);
		mv = round_core_cvb_voltage(mv, d->voltage_scale);

		if (mv > max_mv)
			break;

		/* fill in c2bus dvfs tables */
		mv = max(mv, min_mv);
		if (!j || (mv > c2bus_millivolts[j - 1])) {
			c2bus_millivolts[j] = mv;
			c2bus_dvfs->freqs[j] = table->freq;
			j++;
		} else {
			c2bus_dvfs->freqs[j - 1] = table->freq;
		}
	}
	/* Table must not be empty, must have at least one entry above Vmin */
	if (!i || !j) {
		pr_err("tegra14_dvfs: invalid c2bus dvfs table\n");
		return -ENOENT;
	}

	/* dvfs tables are successfully populated - fill in the c2bus dvfs */
	c2bus_dvfs->speedo_id = d->speedo_id;
	c2bus_dvfs->process_id = d->process_id;
	c2bus_dvfs->freqs_mult = d->freqs_mult;
	c2bus_dvfs->millivolts = c2bus_millivolts;
	c2bus_dvfs->dvfs_rail = &tegra14_dvfs_rail_vdd_core;
	c2bus_dvfs->auto_dvfs = 1;

	*max_freq_index = j - 1;
	return 0;
}

static int __init init_c2bus_cvb_dvfs(int soc_speedo_id, int core_process_id)
{
	int i, ret;
	int max_freq_index = 0;
	int n = ARRAY_SIZE(c2bus_cvb_dvfs_table);
	int m = ARRAY_SIZE(c2bus_dvfs_table);
	struct dvfs *bus_dvfs = &c2bus_dvfs_table[m - 1];

	/*
	 * Setup bus (last) entry in c2bus legacy dvfs table from cvb data;
	 * determine maximum frequency index (cvb may devine for c2bus modules
	 * number of frequencies, and voltage levels different from other core
	 * dvfs modules). Error when c2bus legacy dvfs table can not be
	 * constructed must never happen.
	 */
	for (ret = 0, i = 0; i < n; i++) {
		struct core_cvb_dvfs *d = &c2bus_cvb_dvfs_table[i];
		if (match_dvfs_one("c2bus cvb", d->speedo_id, d->process_id,
				   soc_speedo_id, core_process_id)) {
			ret = set_c2bus_dvfs_data(d, bus_dvfs, &max_freq_index);
			break;
		}
	}
	BUG_ON((i == n) || ret);

	/*
	 * Copy bus dvfs entry across all entries in c2bus legacy devfs table,
	 * and bind each entry to clock
	 */
	for (i = 0; i < m; i++) {
		struct dvfs *d = &c2bus_dvfs_table[i];
		if (d != bus_dvfs) {
			const char *name = d->clk_name;
			*d = *bus_dvfs;
			d->clk_name = name;
		}
#ifdef CONFIG_TEGRA_DUAL_CBUS
		init_dvfs_one(d, max_freq_index);
#endif
	}
	return 0;
}

static int __init get_core_nominal_mv_index(int speedo_id)
{
	int i;
	int mv = tegra_core_speedo_mv();
	int core_edp_limit = get_core_edp();

	/*
	 * Start with nominal level for the chips with this speedo_id. Then,
	 * make sure core nominal voltage is below edp limit for the board
	 * (if edp limit is set).
	 */
	if (core_edp_limit)
		mv = min(mv, core_edp_limit);
	mv = round_core_cvb_voltage(mv * 1000, 1000);

	/* Round nominal level down to the nearest core scaling step */
	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		if ((core_millivolts[i] == 0) || (mv < core_millivolts[i]))
			break;
	}

	if (i == 0) {
		pr_err("tegra14_dvfs: unable to adjust core dvfs table to"
		       " nominal voltage %d\n", mv);
		return -ENOSYS;
	}
	return i - 1;
}

int tegra_cpu_dvfs_alter(int edp_thermal_index, const cpumask_t *cpus,
			 bool before_clk_update, int cpu_event)
{
	/* empty definition for tegra14 */
	return 0;
}

void __init tegra14x_init_dvfs(void)
{
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int cpu_process_id = tegra_cpu_process_id();
	int soc_speedo_id = tegra_soc_speedo_id();
	int core_process_id = tegra_core_process_id();

	int i, ret;
	int core_nominal_mv_index;
	int cpu_max_freq_index = 0;

#ifndef CONFIG_TEGRA_CORE_DVFS
	tegra_dvfs_core_disabled = true;
#endif
#ifndef CONFIG_TEGRA_CPU_DVFS
	tegra_dvfs_cpu_disabled = true;
#endif
	/* Setup rail bins */
	tegra14_dvfs_rail_vdd_cpu.stats.bin_uV = tegra_get_cvb_alignment_uV();
	tegra14_dvfs_rail_vdd_core.stats.bin_uV =
		tegra_get_core_cvb_alignment_uV();

	/* Align dvfs voltages */
	for (i = 0; (i < MAX_DVFS_FREQS) && (core_millivolts[i] != 0); i++) {
		((int *)core_millivolts)[i] =
			round_core_cvb_voltage(core_millivolts[i] * 1000, 1000);
	}

	/*
	 * Find nominal voltages for core (1st) and cpu rails before rail
	 * init. Nominal voltage index in core scaling ladder can also be
	 * used to determine max dvfs frequencies for all core clocks. In
	 * case of error disable core scaling and set index to 0, so that
	 * core clocks would not exceed rates allowed at minimum voltage.
	 */
	core_nominal_mv_index = get_core_nominal_mv_index(soc_speedo_id);
	if (core_nominal_mv_index < 0) {
		tegra14_dvfs_rail_vdd_core.disabled = true;
		tegra_dvfs_core_disabled = true;
		core_nominal_mv_index = 0;
	}
	tegra14_dvfs_rail_vdd_core.nominal_millivolts =
			core_millivolts[core_nominal_mv_index];

	/*
	 * Setup cpu dvfs and dfll tables from cvb data, determine nominal
	 * voltage for cpu rail, and cpu maximum frequency. Note that entire
	 * frequency range is guaranteed only when dfll is used as cpu clock
	 * source. Reaching maximum frequency with pll as cpu clock source
	 * may not be possible within nominal voltage range (dvfs mechanism
	 * would automatically fail frequency request in this case, so that
	 * voltage limit is not violated). Error when cpu dvfs table can not
	 * be constructed must never happen.
	 */
	for (ret = 0, i = 0; i <  ARRAY_SIZE(cpu_cvb_dvfs_table); i++) {
		struct cpu_cvb_dvfs *d = &cpu_cvb_dvfs_table[i];
		if (match_dvfs_one("cpu cvb", d->speedo_id, d->process_id,
				   cpu_speedo_id, cpu_process_id)) {
			ret = set_cpu_dvfs_data(
				d, &cpu_dvfs, &cpu_max_freq_index);
			break;
		}
	}
	BUG_ON((i == ARRAY_SIZE(cpu_cvb_dvfs_table)) || ret);

	/* Init thermal limits */
	tegra_dvfs_rail_init_vmax_thermal_profile(
		vdd_cpu_vmax_trips_table, vdd_cpu_therm_caps_table,
		&tegra14_dvfs_rail_vdd_cpu, &cpu_dvfs.dfll_data);
	tegra_dvfs_rail_init_vmin_thermal_profile(
		cpu_cvb_dvfs_table[i].vmin_trips_table,
		cpu_cvb_dvfs_table[i].therm_floors_table,
		&tegra14_dvfs_rail_vdd_cpu, &cpu_dvfs.dfll_data);
	tegra_dvfs_rail_init_vmin_thermal_profile(vdd_core_vmin_trips_table,
		vdd_core_therm_floors_table, &tegra14_dvfs_rail_vdd_core, NULL);

	/* Init rail structures and dependencies */
	tegra_dvfs_init_rails(tegra14_dvfs_rails,
		ARRAY_SIZE(tegra14_dvfs_rails));

	/* Init c2bus modules (a subset of core dvfs) that have cvb data */
	init_c2bus_cvb_dvfs(soc_speedo_id, core_process_id);

	/* Search core dvfs table for speedo/process matching entries and
	   initialize dvfs-ed clocks */
	for (i = 0; i <  ARRAY_SIZE(core_dvfs_table); i++) {
		struct dvfs *d = &core_dvfs_table[i];
		if (!match_dvfs_one(d->clk_name, d->speedo_id, d->process_id,
				    soc_speedo_id, core_process_id))
			continue;
		init_dvfs_one(d, core_nominal_mv_index);
	}

	/* Initialize matching cpu dvfs entry already found when nominal
	   voltage was determined */
	init_dvfs_one(&cpu_dvfs, cpu_max_freq_index);

	/* Finally disable dvfs on rails if necessary */
	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra14_dvfs_rail_vdd_core);
	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra14_dvfs_rail_vdd_cpu);

	pr_info("tegra dvfs: VDD_CPU nominal %dmV, scaling %s\n",
		tegra14_dvfs_rail_vdd_cpu.nominal_millivolts,
		tegra_dvfs_cpu_disabled ? "disabled" : "enabled");
	pr_info("tegra dvfs: VDD_CORE nominal %dmV, scaling %s\n",
		tegra14_dvfs_rail_vdd_core.nominal_millivolts,
		tegra_dvfs_core_disabled ? "disabled" : "enabled");
}

int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{
	return 0;
}

int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{
	return 0;
}

/* Core voltage and bus cap object and tables */
static struct kobject *cap_kobj;

static struct core_dvfs_cap_table tegra14_core_cap_table[] = {
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ .cap_name = "cap.c2bus" },
	{ .cap_name = "cap.c3bus" },
#else
	{ .cap_name = "cap.cbus" },
#endif
	{ .cap_name = "cap.sclk" },
	{ .cap_name = "cap.emc" },
	{ .cap_name = "cap.host1x" },
	{ .cap_name = "cap.msenc" },
};

/*
 * Keep sys file names the same for dual and single cbus configurations to
 * avoid changes in user space GPU capping interface.
 */
static struct core_bus_limit_table tegra14_bus_cap_table[] = {
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ .limit_clk_name = "cap.profile.c2bus",
	  .refcnt_attr = {.attr = {.name = "cbus_cap_state", .mode = 0644} },
	  .level_attr  = {.attr = {.name = "cbus_cap_level", .mode = 0644} },
	},
#else
	{ .limit_clk_name = "cap.profile.cbus",
	  .refcnt_attr = {.attr = {.name = "cbus_cap_state", .mode = 0644} },
	  .level_attr  = {.attr = {.name = "cbus_cap_level", .mode = 0644} },
	},
#endif
};

static int __init tegra14_dvfs_init_core_cap(void)
{
	int ret;

	cap_kobj = kobject_create_and_add("tegra_cap", kernel_kobj);
	if (!cap_kobj) {
		pr_err("tegra14_dvfs: failed to create sysfs cap object\n");
		return 0;
	}

	ret = tegra_init_shared_bus_cap(
		tegra14_bus_cap_table, ARRAY_SIZE(tegra14_bus_cap_table),
		cap_kobj);
	if (ret) {
		pr_err("tegra14_dvfs: failed to init bus cap interface (%d)\n",
		       ret);
		kobject_del(cap_kobj);
		return 0;
	}

	ret = tegra_init_core_cap(
		tegra14_core_cap_table, ARRAY_SIZE(tegra14_core_cap_table),
		core_millivolts, ARRAY_SIZE(core_millivolts), cap_kobj);

	if (ret) {
		pr_err("tegra14_dvfs: failed to init core cap interface (%d)\n",
		       ret);
		kobject_del(cap_kobj);
		return 0;
	}
	pr_info("tegra dvfs: tegra sysfs cap interface is initialized\n");

	return 0;
}
late_initcall(tegra14_dvfs_init_core_cap);
