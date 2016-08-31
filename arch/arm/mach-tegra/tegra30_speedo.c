/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/bug.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/tegra-fuse.h>


#define CORE_PROCESS_CORNERS_NUM	1
#define CPU_PROCESS_CORNERS_NUM		7

#define FUSE_SPEEDO_CALIB_0	0x114
#define FUSE_IDDQ_CALIB_0	0x118
#define FUSE_PACKAGE_INFO	0X1FC
#define FUSE_TEST_PROG_VER	0X128

#define G_SPEEDO_BIT_MINUS1	58
#define G_SPEEDO_BIT_MINUS1_R	59
#define G_SPEEDO_BIT_MINUS2	60
#define G_SPEEDO_BIT_MINUS2_R	61
#define LP_SPEEDO_BIT_MINUS1	62
#define LP_SPEEDO_BIT_MINUS1_R	63
#define LP_SPEEDO_BIT_MINUS2	64
#define LP_SPEEDO_BIT_MINUS2_R	65

enum {
	THRESHOLD_INDEX_0,
	THRESHOLD_INDEX_1,
	THRESHOLD_INDEX_2,
	THRESHOLD_INDEX_3,
	THRESHOLD_INDEX_4,
	THRESHOLD_INDEX_5,
	THRESHOLD_INDEX_6,
	THRESHOLD_INDEX_7,
	THRESHOLD_INDEX_8,
	THRESHOLD_INDEX_9,
	THRESHOLD_INDEX_10,
	THRESHOLD_INDEX_11,
	THRESHOLD_INDEX_12,
	THRESHOLD_INDEX_13,
	THRESHOLD_INDEX_14,
	THRESHOLD_INDEX_COUNT,
};

static const u32 core_process_speedos[][CORE_PROCESS_CORNERS_NUM] = {
	{180},
	{170},
	{195},
	{180},
	{168},
	{192},
	{180},
	{170},
	{195},
	{180},
	{180},
	{180},
	{185},
	{185},
	{210},
};

static const u32 cpu_process_speedos[][CPU_PROCESS_CORNERS_NUM] = {
	{306, 338, 360, 376, UINT_MAX},
	{295, 336, 358, 375, UINT_MAX},
	{325, 325, 358, 375, UINT_MAX},
	{325, 325, 358, 375, UINT_MAX},
	{292, 324, 348, 364, UINT_MAX},
	{324, 324, 348, 364, UINT_MAX},
	{324, 324, 348, 364, UINT_MAX},
	{295, 336, 358, 375, UINT_MAX},
	{358, 358, 358, 358, 397, UINT_MAX},
	{364, 364, 364, 364, 397, UINT_MAX},
	{295, 336, 358, 375, 391, UINT_MAX},
	{295, 336, 358, 375, 391, UINT_MAX},
	{300, 311, 360, 371, 381, 415, 431},
	{300, 311, 410, 431, UINT_MAX},
	{358, 358, 358, 358, 397, UINT_MAX},
};

static int threshold_index;
static int cpu_iddq_value;

/*
 * Only AP37 supports App Profile
 * This informs user space of support without exposing cpu id's
 */
static int enable_app_profiles;

static void fuse_speedo_calib(u32 *speedo_g, u32 *speedo_lp)
{
	u32 reg;
	int ate_ver;
	int bit_minus1;
	int bit_minus2;

	reg = tegra_fuse_readl(FUSE_SPEEDO_CALIB_0);

	*speedo_lp = (reg & 0xFFFF) * 4;
	*speedo_g = ((reg >> 16) & 0xFFFF) * 4;

	ate_ver = tegra_fuse_readl(FUSE_TEST_PROG_VER);
	pr_info("%s: ATE prog ver %d.%d\n", __func__, ate_ver/10, ate_ver%10);

	if (ate_ver >= 26) {
		bit_minus1 = tegra_spare_fuse(LP_SPEEDO_BIT_MINUS1);
		bit_minus1 |= tegra_spare_fuse(LP_SPEEDO_BIT_MINUS1_R);
		bit_minus2 = tegra_spare_fuse(LP_SPEEDO_BIT_MINUS2);
		bit_minus2 |= tegra_spare_fuse(LP_SPEEDO_BIT_MINUS2_R);
		*speedo_lp |= (bit_minus1 << 1) | bit_minus2;

		bit_minus1 = tegra_spare_fuse(G_SPEEDO_BIT_MINUS1);
		bit_minus1 |= tegra_spare_fuse(G_SPEEDO_BIT_MINUS1_R);
		bit_minus2 = tegra_spare_fuse(G_SPEEDO_BIT_MINUS2);
		bit_minus2 |= tegra_spare_fuse(G_SPEEDO_BIT_MINUS2_R);
		*speedo_g |= (bit_minus1 << 1) | bit_minus2;
	} else {
		*speedo_lp |= 0x3;
		*speedo_g |= 0x3;
	}
}

static void rev_sku_to_speedo_ids(int rev, int sku)
{
	switch (rev) {
	case TEGRA_REVISION_A01:
		tegra_cpu_speedo_id = 0;
		tegra_soc_speedo_id = 0;
		threshold_index = THRESHOLD_INDEX_0;
		break;
	case TEGRA_REVISION_A02:
	case TEGRA_REVISION_A03:
		switch (sku) {
		case 0x87:
		case 0x82:
			tegra_cpu_speedo_id = 1;
			tegra_soc_speedo_id = 1;
			threshold_index = THRESHOLD_INDEX_1;
			break;
		case 0x81:
			switch (tegra_package_id) {
			case 1:
				tegra_cpu_speedo_id = 2;
				tegra_soc_speedo_id = 2;
				threshold_index = THRESHOLD_INDEX_2;
				break;
			case 2:
				tegra_cpu_speedo_id = 4;
				tegra_soc_speedo_id = 1;
				threshold_index = THRESHOLD_INDEX_7;
				break;
			default:
				pr_err("Tegra30: Unknown pkg %d\n", tegra_package_id);
				BUG();
				break;
			}
			break;
		case 0x80:
			switch (tegra_package_id) {
			case 1:
				tegra_cpu_speedo_id = 5;
				tegra_soc_speedo_id = 2;
				threshold_index = THRESHOLD_INDEX_8;
				break;
			case 2:
				tegra_cpu_speedo_id = 6;
				tegra_soc_speedo_id = 2;
				threshold_index = THRESHOLD_INDEX_9;
				break;
			default:
				pr_err("Tegra30: Unknown pkg %d\n", tegra_package_id);
				BUG();
				break;
			}
			break;
		case 0x83:
			switch (tegra_package_id) {
			case 1:
				tegra_cpu_speedo_id = 7;
				tegra_soc_speedo_id = 1;
				threshold_index = THRESHOLD_INDEX_10;
				break;
			case 2:
				tegra_cpu_speedo_id = 3;
				tegra_soc_speedo_id = 2;
				threshold_index = THRESHOLD_INDEX_3;
				break;
			default:
				pr_err("Tegra30: Unknown pkg %d\n", tegra_package_id);
				BUG();
				break;
			}
			break;
		case 0x8F:
			tegra_cpu_speedo_id = 8;
			tegra_soc_speedo_id = 1;
			threshold_index = THRESHOLD_INDEX_11;
			break;
		case 0xA0: /* T37 or A37 */
			switch (tegra_package_id) {
			case 1: /* MID => T37 */
				tegra_cpu_speedo_id = 13;
				tegra_soc_speedo_id = 2;
				threshold_index = THRESHOLD_INDEX_14;
				break;
			case 2: /* DSC => AP37 */
				tegra_cpu_speedo_id = 12;
				tegra_soc_speedo_id = 2;
				threshold_index = THRESHOLD_INDEX_9;
				enable_app_profiles = 1;
				break;
			default:
				pr_err("Tegra3 Rev-A02: Reserved pkg: %d\n",
						tegra_package_id);
				BUG();
				break;
			}
			break;
		case 0x08:
			tegra_cpu_speedo_id = 1;
			tegra_soc_speedo_id = 1;
			threshold_index = THRESHOLD_INDEX_4;
			break;
		case 0x02:
			tegra_cpu_speedo_id = 2;
			tegra_soc_speedo_id = 2;
			threshold_index = THRESHOLD_INDEX_5;
			break;
		case 0x04:
			tegra_cpu_speedo_id = 3;
			tegra_soc_speedo_id = 2;
			threshold_index = THRESHOLD_INDEX_6;
			break;
		case 0x91: /* T30AGS-Ax */
		case 0xb0: /* T30IQS-Ax */
		case 0xb1: /* T30MQS-Ax */
		case 0x90: /* T30AQS-Ax */
			tegra_soc_speedo_id = 3;
			threshold_index = THRESHOLD_INDEX_12;
			break;
		case 0x93: /* T30AG-Ax */
			tegra_cpu_speedo_id = 11;
			tegra_soc_speedo_id = 3;
			threshold_index = THRESHOLD_INDEX_13;
			break;
		case 0:
			switch (tegra_package_id) {
			case 1:
				tegra_cpu_speedo_id = 2;
				tegra_soc_speedo_id = 2;
				threshold_index = THRESHOLD_INDEX_2;
				break;
			case 2:
				tegra_cpu_speedo_id = 3;
				tegra_soc_speedo_id = 2;
				threshold_index = THRESHOLD_INDEX_3;
				break;
			default:
				pr_err("Tegra30: Unknown pkg %d\n", tegra_package_id);
				BUG();
				break;
			}
			break;
		default:
			pr_warn("Tegra30: Unknown SKU %d\n", sku);
			tegra_cpu_speedo_id = 0;
			tegra_soc_speedo_id = 0;
			threshold_index = THRESHOLD_INDEX_0;
			break;
		}
		break;
	default:
		pr_warn("Tegra30: Unknown chip rev %d\n", rev);
		tegra_cpu_speedo_id = 0;
		tegra_soc_speedo_id = 0;
		threshold_index = THRESHOLD_INDEX_0;
		break;
	}
}

/*
 * CPU and core nominal voltage levels as determined by chip SKU and speedo
 * (not final - can be lowered by dvfs tables and rail dependencies; the
 * latter is resolved by the dvfs code)
 */
static const int cpu_speedo_nominal_millivolts[] =
/* speedo_id 0,    1,    2,    3,    4,    5,    6,    7,    8,   9,  10,  11,   12,    13,  14,  15 */
	{ 1125, 1150, 1150, 1150, 1237, 1237, 1237, 1150, 1150, 1007, 916, 850, 1237, 1237, 950, 900};

void tegra30_init_speedo_data(void)
{
	u32 cpu_speedo_val;
	u32 core_speedo_val;
	int i;
	int fuse_sku, new_sku;

	fuse_sku = tegra_get_sku_id();
	new_sku = fuse_sku;

	BUILD_BUG_ON(ARRAY_SIZE(cpu_process_speedos) !=
			THRESHOLD_INDEX_COUNT);
	BUILD_BUG_ON(ARRAY_SIZE(core_process_speedos) !=
			THRESHOLD_INDEX_COUNT);

	tegra_package_id = tegra_fuse_readl(FUSE_PACKAGE_INFO) & 0x0F;

	cpu_iddq_value = tegra_fuse_readl(FUSE_IDDQ_CALIB_0);
	cpu_iddq_value = ((cpu_iddq_value >> 5) & 0x3ff) * 8;

	/* SKU Overrides
	* T33	=> T30, T30L
	* T33S	=> T30S, T30SL
	* T30	=> T30L
	* T30S	=> T30SL
	* AP33	=> AP30
	*/
	switch (tegra_sku_override) {
	case 1:
		/* Base sku override */
		if (fuse_sku == 0x80) {
			if (tegra_package_id == 1) {
				/* T33 to T30 */
				pr_info("%s: SKU OR: T33->T30\n", __func__);
				new_sku = 0x81;
			} else if (tegra_package_id == 2) {
				/* T33S->T30S */
				pr_info("%s: SKU OR: T33S->T30S\n", __func__);
				new_sku = 0x83;
			}
		} else if (fuse_sku == 0x81) {
			if (tegra_package_id == 2) {
				/* AP33->AP30 */
				pr_info("%s: SKU OR: AP33->AP30\n", __func__);
				new_sku = 0x87;
			}
		}
		break;
	case 2:
		/* L sku override */
		if (fuse_sku == 0x80) {
			if (tegra_package_id == 1) {
				/* T33->T30L */
				pr_info("%s: SKU OR: T33->T30L\n", __func__);
				new_sku = 0x83;
			} else if (tegra_package_id == 2) {
				/* T33S->T33SL */
				pr_info("%s: SKU OR: T33S->T30SL\n", __func__);
				new_sku = 0x8f;
			}
		} else if (fuse_sku == 0x81) {
			if (tegra_package_id == 1) {
				pr_info("%s: SKU OR: T30->T30L\n", __func__);
				/* T30->T30L */
				new_sku = 0x83;
			}
		} else if (fuse_sku == 0x83) {
			if (tegra_package_id == 2) {
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
	fuse_speedo_calib(&cpu_speedo_val, &core_speedo_val);
	pr_debug("%s CPU speedo value %u\n", __func__, cpu_speedo_val);
	pr_debug("%s Core speedo value %u\n", __func__, core_speedo_val);

	for (i = 0; i < CPU_PROCESS_CORNERS_NUM; i++) {
		if (cpu_speedo_val < cpu_process_speedos[threshold_index][i])
			break;
	}
	tegra_cpu_process_id = i - 1;

	if (tegra_cpu_process_id == -1) {
		pr_warn("Tegra30: CPU speedo value %3d out of range",
		       cpu_speedo_val);
		tegra_cpu_process_id = 0;
		tegra_cpu_speedo_id = 1;
	} else {
		if (threshold_index == 12) {
			if (tegra_cpu_process_id <= 2) {
				switch(fuse_sku) {
				case 0xb0:
				case 0xb1:
					tegra_cpu_speedo_id = 9;
					break;
				case 0x90:
				case 0x91:
					tegra_cpu_speedo_id = 14;
				default:
					break;
				}
			} else if (tegra_cpu_process_id >= 3 && tegra_cpu_process_id < 6) {
				switch(fuse_sku) {
				case 0xb0:
				case 0xb1:
					tegra_cpu_speedo_id = 10;
					break;
				case 0x90:
				case 0x91:
					tegra_cpu_speedo_id = 15;
				default:
					break;
				}
			}
		}
	}

	for (i = 0; i < CORE_PROCESS_CORNERS_NUM; i++) {
		if (core_speedo_val < core_process_speedos[threshold_index][i])
			break;
	}
	tegra_core_process_id = i - 1;

	if (tegra_core_process_id == -1) {
		pr_warn("Tegra30: CORE speedo value %3d out of range",
		       core_speedo_val);
		tegra_core_process_id = 0;
		tegra_soc_speedo_id = 1;
	}

	pr_info("Tegra30: CPU Speedo ID %d, Soc Speedo ID %d",
		tegra_cpu_speedo_id, tegra_soc_speedo_id);

	tegra_cpu_speedo_mv =
		cpu_speedo_nominal_millivolts[tegra_cpu_speedo_id];
	
	switch (tegra_soc_speedo_id) {
	case 0:
		tegra_core_speedo_mv = 1200;
		break;
	case 1:
		if ((tegra_cpu_speedo_id != 7) &&
		    (tegra_cpu_speedo_id != 8)) {
			tegra_core_speedo_mv = 1200;
			break;
		}
		/* fall thru for T30L or T30SL */
	case 2:
		if (tegra_cpu_speedo_id != 13)
			tegra_core_speedo_mv = 1300;
		else /* T37 */
			tegra_core_speedo_mv = 1350;
		break;
	case 3:
		tegra_core_speedo_mv = 1250;
		break;
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
