/*
 * arch/arm/mach-tegra/tegra14_speedo.c
 *
 * Copyright (C) 2013 NVIDIA Corporation. All rights reserved.
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
#include <linux/bug.h>

#include <linux/tegra-soc.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/tegra-fuse.h>

#include "iomap.h"
#include "common.h"

#define CPU_PROCESS_CORNERS_NUM		2
#define CORE_PROCESS_CORNERS_NUM	2
#define CPU_IDDQ_BITS			13

#define TEGRA148_CPU_SPEEDO 2109
#define FUSE_CPU_IDDQ 0x118 /*FIXME: update T148 register*/
#define FUSE_CPU_SPEEDO_0 0x114
#define FUSE_CORE_SPEEDO_0 0x134
#define FUSE_SPARE_BIT_62_0 0x398
static int threshold_index;
static int cpu_process_id;
static int core_process_id;
static int cpu_speedo_id;
static int cpu_speedo_value;
static int soc_speedo_id;
static int package_id;
static int cpu_iddq_value;
static int core_speedo_value;

static int enable_app_profiles;

static const u32 cpu_process_speedos[][CPU_PROCESS_CORNERS_NUM] = {
/* proc_id  0,     1 */
	{2070,     UINT_MAX}, /* [0]: threshold_index 0 */
	{0,        UINT_MAX}, /* [1]: threshold_index 1 */
};

static const u32 core_process_speedos[][CORE_PROCESS_CORNERS_NUM] = {
/* proc_id  0,	1 */
	{1295,	UINT_MAX}, /* [0]: threshold_index 0 */
	{0,	UINT_MAX}, /* [1]: threshold_index 0 */
};

static void rev_sku_to_speedo_ids(int rev, int sku)
{

	switch (sku) {
	case 0x00: /* Eng */
	case 0x07:
		cpu_speedo_id = 0;
		soc_speedo_id = 0;
		threshold_index = 0;
		break;

	case 0x03:
	case 0x83:
		cpu_speedo_id = 1;
		soc_speedo_id = 1;
		threshold_index = 1;
		break;

	default:
		pr_err("Tegra14 Unknown SKU %d\n", sku);
		cpu_speedo_id = 0;
		soc_speedo_id = 0;
		threshold_index = 0;
		break;
	}
}

void tegra_init_speedo_data(void)
{
	int i;

	cpu_speedo_value = 1024 + tegra_fuse_readl(FUSE_CPU_SPEEDO_0);
	core_speedo_value = tegra_fuse_readl(FUSE_CORE_SPEEDO_0);

	rev_sku_to_speedo_ids(tegra_revision, tegra_get_sku_id());

	for (i = 0; i < CPU_PROCESS_CORNERS_NUM; i++) {
		if (cpu_speedo_value <
			cpu_process_speedos[threshold_index][i]) {
			break;
		}
	}
	cpu_process_id = i;

	cpu_iddq_value = 0;
	for (i = 0; i < CPU_IDDQ_BITS; i++) {
		cpu_iddq_value = (cpu_iddq_value << 1) +
		tegra_fuse_readl(FUSE_SPARE_BIT_62_0 - 4*i);
	}

	if (!cpu_iddq_value)
		cpu_iddq_value = tegra_fuse_readl(FUSE_CPU_IDDQ);


	for (i = 0; i < CORE_PROCESS_CORNERS_NUM; i++) {
		if (core_speedo_value <
			core_process_speedos[threshold_index][i]) {
			break;
		}
	}

	core_process_id = i;

	pr_info("Tegra14: CPU Speedo %d, Soc Speedo %d",
		cpu_speedo_value, core_speedo_value);
	pr_info("Tegra14: CPU Speedo ID %d, Soc Speedo ID %d",
		cpu_speedo_id, soc_speedo_id);
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

int tegra_core_speedo_value(void)
{
	return core_speedo_value;
}

/*
 * CPU and core nominal voltage levels as determined by chip SKU and speedo
 * (not final - can be lowered by dvfs tables and rail dependencies; the
 * latter is resolved by the dvfs code)
 */
int tegra_cpu_speedo_mv(void)
{
	/* Not applicable on Tegra148 */
	return -ENOSYS;
}

int tegra_core_speedo_mv(void)
{
	switch (soc_speedo_id) {
	case 0:
	case 1:
		return 1230;
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
