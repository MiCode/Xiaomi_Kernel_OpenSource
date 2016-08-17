/*
 * arch/arm/mach-tegra/tegra11_speedo.c
 *
 * Copyright (C) 2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
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

#define CORE_PROCESS_CORNERS_NUM	2
#define CPU_PROCESS_CORNERS_NUM		2

#define FUSE_CPU_SPEEDO_0 0x114
#define FUSE_CPU_SPEEDO_1 0x12c
#define FUSE_CPU_IDDQ 0x118
#define FUSE_CORE_SPEEDO_0 0x134
#define FUSE_CORE_SPEEDO_1 0x138
#define FUSE_CORE_IDDQ 0x140
#define FUSE_FT_REV 0x128
#define FUSE_OPT_CPU23_DISABLE 0x26c
#define FUSE_OPT_CPU23_REENABLE 0x270

static int threshold_index;

static int cpu_process_id;
static int core_process_id;
static int cpu_speedo_id;
static int cpu_speedo_value;
static int soc_speedo_id;
static int core_speedo_value;
static int package_id;
static int cpu_iddq_value;

static int enable_app_profiles;

static const u32 core_process_speedos[][CORE_PROCESS_CORNERS_NUM] = {
/* proc_id  0,     1 */
	{1123,     UINT_MAX}, /* [0]: threshold_index 0 */
	{0,        UINT_MAX}, /* [1]: threshold_index 1 */
};

static const u32 cpu_process_speedos[][CPU_PROCESS_CORNERS_NUM] = {
/* proc_id  0,     1 */
	{1695,     UINT_MAX}, /* [0]: threshold_index 0 */
	{0,        UINT_MAX}, /* [1]: threshold_index 1 */
};

static void rev_sku_to_speedo_ids(int rev, int sku)
{
	bool a01 = false;
	cpu_speedo_id = 0;	/* For A01 rev, regardless of SKU */

	if (rev == TEGRA_REVISION_A01) {
		u32 a01p = tegra_fuse_readl(FUSE_OPT_CPU23_REENABLE) << 1;
		a01p |= tegra_fuse_readl(FUSE_OPT_CPU23_DISABLE);
		if (a01p == 0)
			a01 = true;
	}

	switch (sku) {
	case 0x00: /* Eng */
	case 0x10: /* Eng */
	case 0x05: /* T40S */
	case 0x06: /* AP40 */
	case 0x20: /* T40DC */
		if (!a01)
			cpu_speedo_id = 1;
		soc_speedo_id = 0;
		threshold_index = 0;
		break;

	case 0x03: /* T40X */
	case 0x04: /* T40T */
		if (!a01)
			cpu_speedo_id = 2;
		soc_speedo_id = 1;
		threshold_index = 1;
		break;

	case 0x08: /* AP40X */
		if (!a01)
			cpu_speedo_id = 3;
		soc_speedo_id = 1;
		threshold_index = 1;
		break;

	default:
		/* FIXME: replace with BUG() when all SKU's valid */
		pr_err("Tegra11 Unknown SKU %d\n", sku);
		cpu_speedo_id = 0;
		soc_speedo_id = 0;
		threshold_index = 0;
		break;
	}
}

void tegra_init_speedo_data(void)
{
	int i;
	u32 ft_rev, ft_rev_major, ft_rev_minor;

	cpu_speedo_value = 1024 + tegra_fuse_readl(FUSE_CPU_SPEEDO_1);
	core_speedo_value = tegra_fuse_readl(FUSE_CORE_SPEEDO_0);

	cpu_iddq_value = tegra_fuse_readl(FUSE_CPU_IDDQ);

	ft_rev = tegra_fuse_readl(FUSE_FT_REV);
	ft_rev_minor = ft_rev & 0x1f;
	ft_rev_major = (ft_rev >> 5) & 0x3f;

	if ((ft_rev_minor < 5) && (ft_rev_major == 0)) {
		/* Implement: cpu_iddq = max(1.3*fused_cpu_iddq, 2 Amps) */
		cpu_iddq_value *= 130;
		cpu_iddq_value = cpu_iddq_value > 200000 ?
			cpu_iddq_value : 200000;
		cpu_iddq_value /= 100;

		pr_warn("Tegra11: CPU IDDQ and speedo may be bogus");
	}

	rev_sku_to_speedo_ids(tegra_revision, tegra_sku_id);

	pr_info("Tegra11: CPU Speedo ID %d, Soc Speedo ID %d",
		cpu_speedo_id, soc_speedo_id);
	pr_info("Tegra11: CPU Speedo Value %d, Soc Speedo Value %d",
		cpu_speedo_value, core_speedo_value);

	for (i = 0; i < CPU_PROCESS_CORNERS_NUM; i++) {
		if (cpu_speedo_value <
		    cpu_process_speedos[threshold_index][i]) {
			break;
		}
	}
	cpu_process_id = i;

	for (i = 0; i < CORE_PROCESS_CORNERS_NUM; i++) {
		if (core_speedo_value <
		    core_process_speedos[threshold_index][i]) {
			break;
		}
	}
	core_process_id = i;
}

int tegra_cpu_process_id(void)
{
	return cpu_process_id;
}

int tegra_core_process_id(void)
{
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

int tegra_cpu_speedo_value(void)
{
	return cpu_speedo_value;
}

/*
 * CPU and core nominal voltage levels as determined by chip SKU and speedo
 * (not final - can be lowered by dvfs tables and rail dependencies; the
 * latter is resolved by the dvfs code)
 */
int tegra_cpu_speedo_mv(void)
{
	/* Not applicable on Tegra11 */
	return -ENOSYS;
}

int tegra_core_speedo_mv(void)
{
	switch (soc_speedo_id) {
	case 0:
		if (core_process_id == 1)
			return 1170;
	/* fall thru if core_process_id = 0 */
	case 1:
		if ((tegra_sku_id == 0x4) || (tegra_sku_id == 0x8))
			return 1390;
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
