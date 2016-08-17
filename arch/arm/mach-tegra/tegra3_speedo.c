/*
 * arch/arm/mach-tegra/tegra3_speedo.c
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/bug.h>			/* For BUG_ON.  */

#include <mach/iomap.h>
#include <mach/tegra_fuse.h>
#include <mach/hardware.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "fuse.h"

#define CORE_PROCESS_CORNERS_NUM	1
#define CPU_PROCESS_CORNERS_NUM	7

#define FUSE_SPEEDO_CALIB_0	0x114
#define FUSE_IDDQ_CALIB_0	0x118
#define FUSE_PACKAGE_INFO	0X1FC
#define FUSE_TEST_PROG_VER	0X128
#define FUSE_SPARE_BIT_58	0x32c
#define FUSE_SPARE_BIT_59	0x330
#define FUSE_SPARE_BIT_60	0x334
#define FUSE_SPARE_BIT_61	0x338
#define FUSE_SPARE_BIT_62	0x33c
#define FUSE_SPARE_BIT_63	0x340
#define FUSE_SPARE_BIT_64	0x344
#define FUSE_SPARE_BIT_65	0x348

#define G_SPEEDO_BIT_MINUS1	FUSE_SPARE_BIT_58
#define G_SPEEDO_BIT_MINUS1_R	FUSE_SPARE_BIT_59
#define G_SPEEDO_BIT_MINUS2	FUSE_SPARE_BIT_60
#define G_SPEEDO_BIT_MINUS2_R	FUSE_SPARE_BIT_61
#define LP_SPEEDO_BIT_MINUS1	FUSE_SPARE_BIT_62
#define LP_SPEEDO_BIT_MINUS1_R	FUSE_SPARE_BIT_63
#define LP_SPEEDO_BIT_MINUS2	FUSE_SPARE_BIT_64
#define LP_SPEEDO_BIT_MINUS2_R	FUSE_SPARE_BIT_65

/* Maximum speedo levels for each core process corner */
static const u32 core_process_speedos[][CORE_PROCESS_CORNERS_NUM] = {
/* proc_id 0 */
	{180}, /* [0]: soc_speedo_id 0: any A01 */

/* T30 family */
	{170}, /* [1]: soc_speedo_id 1: AP30 */
	{195}, /* [2]: soc_speedo_id 2: T30  */
	{180}, /* [3]: soc_speedo_id 2: T30S */

/* Characterization SKUs */
	{168}, /* [4]: soc_speedo_id 1: AP30 char */
	{192}, /* [5]: soc_speedo_id 2: T30  char */
	{180}, /* [6]: soc_speedo_id 2: T30S char */

/* T33 family */
	{170}, /* [7]: soc_speedo_id = 1 - AP33 */
	{195}, /* [8]: soc_speedo_id = 2 - T33  */
	{180}, /* [9]: soc_speedo_id = 2 - T33S/AP37 */

/* T30 'L' family */
	{180}, /* [10]: soc_speedo_id 1: T30L */
	{180}, /* [11]: soc_speedo_id 1: T30SL */

/* T30 Automotives */
	{185}, /* [12]: soc_speedo_id = 3 - Automotives */
	{185}, /* [13]: soc_speedo_id = 3 - Automotives */

/* T37 Family*/
	{210}, /* [14]: soc_speedo_id 2: T37 */
};

/* Maximum speedo levels for each CPU process corner */
static const u32 cpu_process_speedos[][CPU_PROCESS_CORNERS_NUM] = {
/* proc_id 0    1    2    3    4*/
	{306, 338, 360, 376, UINT_MAX}, /* [0]: cpu_speedo_id 0: any A01 */

/* T30 family */
	{295, 336, 358, 375, UINT_MAX}, /* [1]: cpu_speedo_id 1: AP30 */
	{325, 325, 358, 375, UINT_MAX}, /* [2]: cpu_speedo_id 2: T30  */
	{325, 325, 358, 375, UINT_MAX}, /* [3]: cpu_speedo_id 3: T30S */

/* Characterization SKUs */
	{292, 324, 348, 364, UINT_MAX}, /* [4]: cpu_speedo_id 1: AP30char */
	{324, 324, 348, 364, UINT_MAX}, /* [5]: cpu_speedo_id 2: T30char  */
	{324, 324, 348, 364, UINT_MAX}, /* [6]: cpu_speedo_id 3: T30Schar */

/* T33 family */
	{295, 336, 358, 375, UINT_MAX},      /* [7]: cpu_speedo_id: 4: AP33 */
	{358, 358, 358, 358, 397, UINT_MAX}, /* [8]: cpu_speedo_id: 5: T33  */
	{364, 364, 364, 364, 397, UINT_MAX}, /* [9]: cpu_speedo_id: 6/12: T33S/AP37 */

/* T30 'L' family */
	{295, 336, 358, 375, 391, UINT_MAX}, /* [10]: cpu_speedo_id 7: T30L  */
	{295, 336, 358, 375, 391, UINT_MAX}, /* [11]: cpu_speedo_id 8: T30SL */

/* T30 Automotives */
	/* threshold_index 12: cpu_speedo_id 9 & 10
	 * 0,1,2 values correspond to speedo_id  9/14
	 * 3,4,5 values correspond to speedo_id 10/15*/
	{300, 311, 360, 371, 381, 415, 431},
	{300, 311, 410, 431, UINT_MAX}, /* [13]: cpu_speedo_id 11: T30 auto */

/* T37 family */
	{358, 358, 358, 358, 397, UINT_MAX}, /* [14]: cpu_speedo_id 13: T37 */
};

/*
 * Common speedo_value array threshold index for both core_process_speedos and
 * cpu_process_speedos arrays. Make sure these two arrays are always in synch.
 */
static int threshold_index;

static int cpu_process_id;
static int core_process_id;
static int cpu_speedo_id;
static int soc_speedo_id;
static int package_id;
static int cpu_iddq_value;

/*
 * Only AP37 supports App Profile
 * This informs user space of support without exposing cpu id's
 */
static int enable_app_profiles;

static void fuse_speedo_calib(u32 *speedo_g, u32 *speedo_lp)
{
	u32 reg;
	int ate_ver, bit_minus1, bit_minus2;

	BUG_ON(!speedo_g || !speedo_lp);
	reg = tegra_fuse_readl(FUSE_SPEEDO_CALIB_0);

	/* Speedo LP = Lower 16-bits Multiplied by 4 */
	*speedo_lp = (reg & 0xFFFF) * 4;

	/* Speedo G = Upper 16-bits Multiplied by 4 */
	*speedo_g = ((reg >> 16) & 0xFFFF) * 4;

	if (tegra_fuse_get_revision(&ate_ver))
		return;
	pr_info("%s: ATE prog ver %d.%d\n", __func__, ate_ver/10, ate_ver%10);

	pr_debug("CPU speedo base value %u (0x%3x)\n", *speedo_g, *speedo_g);
	pr_debug("Core speedo base value %u (0x%3x)\n", *speedo_lp, *speedo_lp);

	if (ate_ver >= 26) {
		/* read lower 2 bits of LP speedo from spare fuses */
		bit_minus1 = tegra_fuse_readl(LP_SPEEDO_BIT_MINUS1) & 0x1;
		bit_minus1 |= tegra_fuse_readl(LP_SPEEDO_BIT_MINUS1_R) & 0x1;
		bit_minus2 = tegra_fuse_readl(LP_SPEEDO_BIT_MINUS2) & 0x1;
		bit_minus2 |= tegra_fuse_readl(LP_SPEEDO_BIT_MINUS2_R) & 0x1;
		*speedo_lp |= (bit_minus1 << 1) | bit_minus2;

		/* read lower 2 bits of G speedo from spare fuses */
		bit_minus1 = tegra_fuse_readl(G_SPEEDO_BIT_MINUS1) & 0x1;
		bit_minus1 |= tegra_fuse_readl(G_SPEEDO_BIT_MINUS1_R) & 0x1;
		bit_minus2 = tegra_fuse_readl(G_SPEEDO_BIT_MINUS2) & 0x1;
		bit_minus2 |= tegra_fuse_readl(G_SPEEDO_BIT_MINUS2_R) & 0x1;
		*speedo_g |= (bit_minus1 << 1) | bit_minus2;
	} else {
		/* set lower 2 bits for speedo ate-ver independent comparison */
		*speedo_lp |= 0x3;
		*speedo_g |= 0x3;
	}
}

static void rev_sku_to_speedo_ids(int rev, int sku)
{
	switch (rev) {
	case TEGRA_REVISION_A01: /* any A01 */
		cpu_speedo_id = 0;
		soc_speedo_id = 0;
		threshold_index = 0;
		break;

	case TEGRA_REVISION_A02:
	case TEGRA_REVISION_A03:
		switch (sku) {
		case 0x87: /* AP30 */
		case 0x82: /* T30V */
			cpu_speedo_id = 1;
			soc_speedo_id = 1;
			threshold_index = 1;
			break;

		case 0x81: /* T30 */
			switch (package_id) {
			case 1: /* MID => T30 */
				cpu_speedo_id = 2;
				soc_speedo_id = 2;
				threshold_index = 2;
				break;
			case 2: /* DSC => AP33 */
				cpu_speedo_id = 4;
				soc_speedo_id = 1;
				threshold_index = 7;
				break;
			default:
				pr_err("Tegra3 Rev-A02: Reserved pkg: %d\n",
				       package_id);
				BUG();
				break;
			}
			break;

		case 0x80: /* T33 or T33S */
			switch (package_id) {
			case 1: /* MID => T33 */
				cpu_speedo_id = 5;
				soc_speedo_id = 2;
				threshold_index = 8;
				break;
			case 2: /* DSC => T33S */
				cpu_speedo_id = 6;
				soc_speedo_id = 2;
				threshold_index = 9;
				break;
			default:
				pr_err("Tegra3 Rev-A02: Reserved pkg: %d\n",
				       package_id);
				BUG();
				break;
			}
			break;

		case 0x83: /* T30L or T30S */
			switch (package_id) {
			case 1: /* MID => T30L */
				cpu_speedo_id = 7;
				soc_speedo_id = 1;
				threshold_index = 10;
				break;
			case 2: /* DSC => T30S */
				cpu_speedo_id = 3;
				soc_speedo_id = 2;
				threshold_index = 3;
				break;
			default:
				pr_err("Tegra3 Rev-A02: Reserved pkg: %d\n",
				       package_id);
				BUG();
				break;
			}
			break;

		case 0x8F: /* T30SL */
			cpu_speedo_id = 8;
			soc_speedo_id = 1;
			threshold_index = 11;
			break;

		case 0xA0: /* T37 or A37 */
			switch (package_id) {
			case 1: /* MID => T37 */
				cpu_speedo_id = 13;
				soc_speedo_id = 2;
				threshold_index = 14;
				break;
			case 2: /* DSC => AP37 */
				cpu_speedo_id = 12;
				soc_speedo_id = 2;
				threshold_index = 9;
				enable_app_profiles = 1;
				break;
			default:
				pr_err("Tegra3 Rev-A02: Reserved pkg: %d\n",
						package_id);
				BUG();
				break;
			}
			break;

/* Characterization SKUs */
		case 0x08: /* AP30 char */
			cpu_speedo_id = 1;
			soc_speedo_id = 1;
			threshold_index = 4;
			break;
		case 0x02: /* T30 char */
			cpu_speedo_id = 2;
			soc_speedo_id = 2;
			threshold_index = 5;
			break;
		case 0x04: /* T30S char */
			cpu_speedo_id = 3;
			soc_speedo_id = 2;
			threshold_index = 6;
			break;

		case 0x91: /* T30AGS-Ax */
		case 0xb0: /* T30IQS-Ax */
		case 0xb1: /* T30MQS-Ax */
		case 0x90: /* T30AQS-Ax */
			soc_speedo_id = 3;
			threshold_index = 12;
			break;
		case 0x93: /* T30AG-Ax */
			cpu_speedo_id = 11;
			soc_speedo_id = 3;
			threshold_index = 13;
			break;
		case 0:    /* ENG - check package_id */
			pr_info("Tegra3 ENG SKU: Checking package_id\n");
			switch (package_id) {
			case 1: /* MID => assume T30 */
				cpu_speedo_id = 2;
				soc_speedo_id = 2;
				threshold_index = 2;
				break;
			case 2: /* DSC => assume T30S */
				cpu_speedo_id = 3;
				soc_speedo_id = 2;
				threshold_index = 3;
				break;
			default:
				pr_err("Tegra3 Rev-A02: Reserved pkg: %d\n",
				       package_id);
				BUG();
				break;
			}
			break;

		default:
			/* FIXME: replace with BUG() when all SKU's valid */
			pr_err("Tegra3 Rev-A02: Unknown SKU %d\n", sku);
			cpu_speedo_id = 0;
			soc_speedo_id = 0;
			threshold_index = 0;
			break;
		}
		break;
	default:
		BUG();
		break;
	}
}

void tegra_init_speedo_data(void)
{
	u32 cpu_speedo_val, core_speedo_val;
	int iv;
	int fuse_sku = tegra_sku_id;
	int sku_override = tegra_get_sku_override();
	int new_sku = fuse_sku;

	/* Package info: 4 bits - 0,3:reserved 1:MID 2:DSC */
	package_id = tegra_fuse_readl(FUSE_PACKAGE_INFO) & 0x0F;

	/* Arrays must be of equal size - each index corresponds to a SKU */
	BUG_ON(ARRAY_SIZE(cpu_process_speedos) !=
	       ARRAY_SIZE(core_process_speedos));

	cpu_iddq_value = tegra_fuse_readl(FUSE_IDDQ_CALIB_0);
	cpu_iddq_value = ((cpu_iddq_value >> 5) & 0x3ff) * 8;

	/* SKU Overrides
	* T33	=> T30, T30L
	* T33S	=> T30S, T30SL
	* T30	=> T30L
	* T30S	=> T30SL
	* AP33	=> AP30
	*/
	switch (sku_override) {
	case 1:
		/* Base sku override */
		if (fuse_sku == 0x80) {
			if (package_id == 1) {
				/* T33 to T30 */
				pr_info("%s: SKU OR: T33->T30\n", __func__);
				new_sku = 0x81;
			} else if (package_id == 2) {
				/* T33S->T30S */
				pr_info("%s: SKU OR: T33S->T30S\n", __func__);
				new_sku = 0x83;
			}
		} else if (fuse_sku == 0x81) {
			if (package_id == 2) {
				/* AP33->AP30 */
				pr_info("%s: SKU OR: AP33->AP30\n", __func__);
				new_sku = 0x87;
			}
		}
		break;
	case 2:
		/* L sku override */
		if (fuse_sku == 0x80) {
			if (package_id == 1) {
				/* T33->T30L */
				pr_info("%s: SKU OR: T33->T30L\n", __func__);
				new_sku = 0x83;
			} else if (package_id == 2) {
				/* T33S->T33SL */
				pr_info("%s: SKU OR: T33S->T30SL\n", __func__);
				new_sku = 0x8f;
			}
		} else if (fuse_sku == 0x81) {
			if (package_id == 1) {
				pr_info("%s: SKU OR: T30->T30L\n", __func__);
				/* T30->T30L */
				new_sku = 0x83;
			}
		} else if (fuse_sku == 0x83) {
			if (package_id == 2) {
				pr_info("%s: SKU OR: T30S->T30SL\n", __func__);
				/* T30S to T30SL */
				new_sku = 0x8f;
			}
		}
		break;
	default:
		/* no override */
		break;
	}

	rev_sku_to_speedo_ids(tegra_revision, new_sku);
	BUG_ON(threshold_index >= ARRAY_SIZE(cpu_process_speedos));

	fuse_speedo_calib(&cpu_speedo_val, &core_speedo_val);
	pr_debug("%s CPU speedo value %u\n", __func__, cpu_speedo_val);
	pr_debug("%s Core speedo value %u\n", __func__, core_speedo_val);

	for (iv = 0; iv < CPU_PROCESS_CORNERS_NUM; iv++) {
		if (cpu_speedo_val <
		    cpu_process_speedos[threshold_index][iv]) {
			break;
		}
	}
	cpu_process_id = iv -1;

	if (cpu_process_id == -1) {
		pr_err("****************************************************");
		pr_err("****************************************************");
		pr_err("* tegra3_speedo: CPU speedo value %3d out of range *",
		       cpu_speedo_val);
		pr_err("****************************************************");
		pr_err("****************************************************");

		cpu_process_id = INVALID_PROCESS_ID;
		cpu_speedo_id = 1;
	}

	for (iv = 0; iv < CORE_PROCESS_CORNERS_NUM; iv++) {
		if (core_speedo_val <
		    core_process_speedos[threshold_index][iv]) {
			break;
		}
	}
	core_process_id = iv -1;

	if (core_process_id == -1) {
		pr_err("****************************************************");
		pr_err("****************************************************");
		pr_err("* tegra3_speedo: CORE speedo value %3d out of range *",
		       core_speedo_val);
		pr_err("****************************************************");
		pr_err("****************************************************");

		core_process_id = INVALID_PROCESS_ID;
		soc_speedo_id = 1;
	}
	if (threshold_index == 12 && cpu_process_id != INVALID_PROCESS_ID) {
		if (cpu_process_id <= 2) {
			switch(fuse_sku) {
			case 0xb0:
			case 0xb1:
				cpu_speedo_id = 9;
				break;
			case 0x90:
			case 0x91:
				cpu_speedo_id = 14;
			default:
				break;
			}
		} else if (cpu_process_id >= 3 && cpu_process_id < 6) {
			switch(fuse_sku) {
			case 0xb0:
			case 0xb1:
				cpu_speedo_id = 10;
				break;
			case 0x90:
			case 0x91:
				cpu_speedo_id = 15;
			default:
				break;
			}
		}
	}
	pr_info("Tegra3: CPU Speedo ID %d, Soc Speedo ID %d",
		 cpu_speedo_id, soc_speedo_id);
}

int tegra_cpu_process_id(void)
{
	/* FIXME: remove when ready to deprecate invalid process-id boards */
	if (cpu_process_id == INVALID_PROCESS_ID)
		return 0;
	else
		return cpu_process_id;
}

int tegra_core_process_id(void)
{
	/* FIXME: remove when ready to deprecate invalid process-id boards */
	if (core_process_id == INVALID_PROCESS_ID)
		return 0;
	else
		return core_process_id;
}

int tegra_cpu_speedo_id(void)
{
	return cpu_speedo_id;
}

int tegra_soc_speedo_id(void)
{
	return soc_speedo_id;
}

int tegra_package_id(void)
{
	return package_id;
}

/*
 * CPU and core nominal voltage levels as determined by chip SKU and speedo
 * (not final - can be lowered by dvfs tables and rail dependencies; the
 * latter is resolved by the dvfs code)
 */
static const int cpu_speedo_nominal_millivolts[] =
/* speedo_id 0,    1,    2,    3,    4,    5,    6,    7,    8,   9,  10,  11,   12,    13,  14,  15 */
	{ 1125, 1150, 1150, 1150, 1237, 1237, 1237, 1150, 1150, 1007, 916, 850, 1237, 1237, 950, 900};

int tegra_cpu_speedo_mv(void)
{
	return cpu_speedo_nominal_millivolts[cpu_speedo_id];
}

int tegra_core_speedo_mv(void)
{
	switch (soc_speedo_id) {
	case 0:
		return 1200;
	case 1:
		if ((cpu_speedo_id != 7) && (cpu_speedo_id != 8))
			return 1200;
		/* fall thru for T30L or T30SL */
	case 2:
		if (cpu_speedo_id != 13)
			return 1300;
		/* T37 */
		return 1350;
	case 3:
		return 1250;
	default:
		BUG();
	}
}

int tegra_get_cpu_iddq_value()
{
	return cpu_iddq_value;
}

static int get_enable_app_profiles(char *val, const struct kernel_param *kp)
{
	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_profiles_ops = {
	.get = get_enable_app_profiles,
};

module_param_cb(tegra_enable_app_profiles,
	&tegra_profiles_ops, &enable_app_profiles, 0444);
