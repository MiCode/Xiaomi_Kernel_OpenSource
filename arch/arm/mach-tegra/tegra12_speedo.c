/*
 * arch/arm/mach-tegra/tegra12_speedo.c
 *
 * Copyright (C) 2013-2014 NVIDIA Corporation. All rights reserved.
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

#include <linux/tegra-soc.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/tegra-fuse.h>

#include "iomap.h"
#include "common.h"

#define TEGRA124_CPU_SPEEDO 2271 /* FIXME: Get Correct Value */

#define CPU_PROCESS_CORNERS_NUM		2
#define GPU_PROCESS_CORNERS_NUM		2
#define CORE_PROCESS_CORNERS_NUM		2

#define FUSE_CPU_SPEEDO_0	0x114
#define FUSE_CPU_SPEEDO_1	0x12c
#define FUSE_CPU_SPEEDO_2	0x130
#define FUSE_SOC_SPEEDO_0	0x134
#define FUSE_SOC_SPEEDO_1	0x138
#define FUSE_SOC_SPEEDO_2	0x13c
#define FUSE_CPU_IDDQ		0x118
#define FUSE_SOC_IDDQ		0x140
#define FUSE_GPU_IDDQ		0x228
#define FUSE_FT_REV		0x128

static int threshold_index;
static int cpu_process_id;
static int core_process_id;
static int gpu_process_id;
static int cpu_speedo_id;
static int cpu_speedo_value;
static int soc_speedo_id;
static int gpu_speedo_id;
static int package_id;
static int cpu_iddq_value;
static int gpu_iddq_value;
static int soc_iddq_value;

static int cpu_speedo_0_value;
static int cpu_speedo_1_value;
static int soc_speedo_0_value;
static int soc_speedo_1_value;
static int soc_speedo_2_value;

static int gpu_speedo_value;

static int enable_app_profiles;

static const u32 cpu_process_speedos[][CPU_PROCESS_CORNERS_NUM] = {
/* proc_id  0,	   1 */
	{UINT_MAX, UINT_MAX}, /* [0]: threshold_index 0 */
	{0,	   UINT_MAX}, /* [1]: threshold_index 1 */
};

static const u32 gpu_process_speedos[][GPU_PROCESS_CORNERS_NUM] = {
/* proc_id  0,	   1 */
	{UINT_MAX, UINT_MAX}, /* [0]: threshold_index 0 */
	{0,	   UINT_MAX}, /* [1]: threshold_index 1 */
};

static const u32 core_process_speedos[][CORE_PROCESS_CORNERS_NUM] = {
/* proc_id  0,	1 */
	{2061,	UINT_MAX}, /* [0]: threshold_index 0 */
	{0,	UINT_MAX}, /* [1]: threshold_index 1 */
};

static void rev_sku_to_speedo_ids(int rev, int sku)
{
	int can_boost = (tegra_spare_fuse(60) && tegra_get_sku_override());

	switch (sku) {
	case 0x00: /* Engg sku */
	case 0x0F:
	case 0x83:
	case 0x23:
		cpu_speedo_id = sku == 0x83 ? 2 : 0;
		soc_speedo_id = 0;
		gpu_speedo_id = 0;
		threshold_index = 0;
		break;
	case 0x1F:
	case 0x87:
	case 0x27:
		cpu_speedo_id = sku == 0x87 ? 2 : 5;
		soc_speedo_id = 0;
		gpu_speedo_id = 1;
		threshold_index = 0;
		break;
	case 0x07:
		if (can_boost) {
			cpu_speedo_id = 3;
			soc_speedo_id = 1;
			gpu_speedo_id = 2;
			threshold_index = 1;
			break;
		}
		/* fall thru */
	case 0x81:
	case 0x21:
		cpu_speedo_id = 1;
		soc_speedo_id = 1;
		gpu_speedo_id = 1;
		threshold_index = 1;
		break;
	default:
		pr_warn("Tegra12: Unknown SKU %d\n", sku);
		cpu_speedo_id = 0;
		soc_speedo_id = 0;
		gpu_speedo_id = 0;
		threshold_index = 0;
		break;
	}
}

void tegra_init_speedo_data(void)
{
	int i;

	if (!tegra_platform_is_silicon()) {
		cpu_process_id  =  0;
		core_process_id =  0;
		gpu_process_id  = 0;
		cpu_speedo_id   = 0;
		soc_speedo_id   = 0;
		gpu_speedo_id   = 0;
		package_id = -1;
		cpu_speedo_value = 1777;
		cpu_speedo_0_value = 0;
		cpu_speedo_1_value = 0;
		soc_speedo_0_value = 0;
		soc_speedo_1_value = 0;
		soc_speedo_2_value = 0;
		soc_iddq_value = 0;
		gpu_iddq_value = 0;
		return;
	}

	cpu_speedo_0_value = tegra_fuse_readl(FUSE_CPU_SPEEDO_0);
	cpu_speedo_1_value = tegra_fuse_readl(FUSE_CPU_SPEEDO_1);

	/* GPU Speedo is stored in CPU_SPEEDO_2 */
	gpu_speedo_value = tegra_fuse_readl(FUSE_CPU_SPEEDO_2);

	soc_speedo_0_value = tegra_fuse_readl(FUSE_SOC_SPEEDO_0);
	soc_speedo_1_value = tegra_fuse_readl(FUSE_SOC_SPEEDO_1);
	soc_speedo_2_value = tegra_fuse_readl(FUSE_SOC_SPEEDO_2);

	cpu_iddq_value = tegra_fuse_readl(FUSE_CPU_IDDQ);
	soc_iddq_value = tegra_fuse_readl(FUSE_SOC_IDDQ);
	gpu_iddq_value = tegra_fuse_readl(FUSE_GPU_IDDQ);

	/* cpu_speedo_value = TEGRA124_CPU_SPEEDO; */
	cpu_speedo_value = cpu_speedo_0_value;

	if (cpu_speedo_value == 0) {
		cpu_speedo_value = 1900;
		pr_warn("Tegra12: Warning: Speedo value not fused. PLEASE FIX!!!!!!!!!!!\n");
		pr_warn("Tegra12: Warning: PLEASE USE BOARD WITH FUSED SPEEDO VALUE !!!!\n");
	}


	rev_sku_to_speedo_ids(tegra_revision, tegra_get_sku_id());

	for (i = 0; i < GPU_PROCESS_CORNERS_NUM; i++) {
		if (gpu_speedo_value <
			gpu_process_speedos[threshold_index][i]) {
			break;
		}
	}
	gpu_process_id = i;

	for (i = 0; i < CPU_PROCESS_CORNERS_NUM; i++) {
                if (cpu_speedo_value <
                        cpu_process_speedos[threshold_index][i]) {
                        break;
                }
        }
	cpu_process_id = i;

	for (i = 0; i < CORE_PROCESS_CORNERS_NUM; i++) {
                if (soc_speedo_0_value <
                        core_process_speedos[threshold_index][i]) {
                        break;
                }
        }
	core_process_id = i;

	pr_info("Tegra12: CPU Speedo ID %d, Soc Speedo ID %d, Gpu Speedo ID %d\n",
		cpu_speedo_id, soc_speedo_id, gpu_speedo_id);
	pr_info("Tegra12: CPU Process ID %d,Soc Process ID %d,Gpu Process ID %d\n",
		 cpu_process_id, core_process_id, gpu_process_id);
}

int tegra_cpu_process_id(void)
{
	return cpu_process_id;
}

int tegra_core_process_id(void)
{
	return core_process_id;
}

int tegra_gpu_process_id(void)
{
	return gpu_process_id;
}

int tegra_cpu_speedo_id(void)
{
	return cpu_speedo_id;
}

int tegra_soc_speedo_id(void)
{
	return soc_speedo_id;
}

int tegra_gpu_speedo_id(void)
{
	return gpu_speedo_id;
}

int tegra_package_id(void)
{
	return package_id;
}

int tegra_cpu_speedo_value(void)
{
	return cpu_speedo_value;
}

int tegra_cpu_speedo_0_value(void)
{
	return cpu_speedo_0_value;
}

int tegra_cpu_speedo_1_value(void)
{
	return cpu_speedo_1_value;
}

int tegra_gpu_speedo_value(void)
{
	return gpu_speedo_value;
}

int tegra_soc_speedo_0_value(void)
{
	return soc_speedo_0_value;
}

int tegra_soc_speedo_1_value(void)
{
	return soc_speedo_1_value;
}

int tegra_soc_speedo_2_value(void)
{
	return soc_speedo_2_value;
}
/*
 * CPU and core nominal voltage levels as determined by chip SKU and speedo
 * (not final - can be lowered by dvfs tables and rail dependencies; the
 * latter is resolved by the dvfs code)
 */
int tegra_cpu_speedo_mv(void)
{
	/* Not applicable on Tegra12 */
	return -ENOSYS;
}

int tegra_core_speedo_mv(void)
{
	switch (soc_speedo_id) {
	case 0:
		return 1150;
	case 1:
		return 1150;
	default:
		BUG();
	}
}

int tegra_get_cpu_iddq_value(void)
{
	return cpu_iddq_value;
}

int tegra_get_soc_iddq_value(void)
{
	return soc_iddq_value;
}

int tegra_get_gpu_iddq_value(void)
{
	return gpu_iddq_value;
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
