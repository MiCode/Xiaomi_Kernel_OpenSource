/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <mach/kgsl.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/msm_dcvs.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "board-8064.h"

#ifdef CONFIG_MSM_DCVS
static struct msm_dcvs_freq_entry grp3d_freq[] = {
	{0, 900, 0, 0, 0},
	{0, 950, 0, 0, 0},
	{0, 950, 0, 0, 0},
	{0, 1200, 1, 100, 100},
};

static struct msm_dcvs_core_info grp3d_core_info = {
	.freq_tbl		= &grp3d_freq[0],
	.num_cores		= 1,
	.sensors		= (int[]){0},
	.thermal_poll_ms	= 60000,
	.core_param		= {
		.core_type	= MSM_DCVS_CORE_TYPE_GPU,
	},
	.algo_param		= {
		.disable_pc_threshold	= 0,
		.em_win_size_min_us	= 100000,
		.em_win_size_max_us	= 300000,
		.em_max_util_pct	= 97,
		.group_id		= 0,
		.max_freq_chg_time_us	= 100000,
		.slack_mode_dynamic	= 0,
		.slack_time_min_us	= 39000,
		.slack_time_max_us	= 39000,
		.ss_win_size_min_us	= 1000000,
		.ss_win_size_max_us	= 1000000,
		.ss_util_pct		= 95,
		.ss_no_corr_below_freq	= 0,
	},

	.energy_coeffs		= {
		.leakage_coeff_a	= -17720,
		.leakage_coeff_b	= 37,
		.leakage_coeff_c	= 3329,
		.leakage_coeff_d	= -277,

		.active_coeff_a		= 2492,
		.active_coeff_b		= 0,
		.active_coeff_c		= 0
	},

	.power_param		= {
		.current_temp	= 25,
		.num_freq	= ARRAY_SIZE(grp3d_freq),
	}
};
#endif /* CONFIG_MSM_DCVS */

#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors grp3d_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors grp3d_low_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(1000),
	},
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(1000),
	},
};

static struct msm_bus_vectors grp3d_nominal_low_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(2000),
	},
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(2000),
	},
};

static struct msm_bus_vectors grp3d_nominal_high_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(2656),
	},
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(2656),
	},
};

static struct msm_bus_vectors grp3d_max_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(4264),
	},
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(4264),
	},
};

static struct msm_bus_paths grp3d_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(grp3d_init_vectors),
		grp3d_init_vectors,
	},
	{
		ARRAY_SIZE(grp3d_low_vectors),
		grp3d_low_vectors,
	},
	{
		ARRAY_SIZE(grp3d_nominal_low_vectors),
		grp3d_nominal_low_vectors,
	},
	{
		ARRAY_SIZE(grp3d_nominal_high_vectors),
		grp3d_nominal_high_vectors,
	},
	{
		ARRAY_SIZE(grp3d_max_vectors),
		grp3d_max_vectors,
	},
};

static struct msm_bus_scale_pdata grp3d_bus_scale_pdata = {
	grp3d_bus_scale_usecases,
	ARRAY_SIZE(grp3d_bus_scale_usecases),
	.name = "grp3d",
};
#endif

static struct resource kgsl_3d0_resources[] = {
	{
		.name = KGSL_3D0_REG_MEMORY,
		.start = 0x04300000, /* GFX3D address */
		.end = 0x0430ffff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = KGSL_3D0_SHADER_MEMORY,
		.start = 0x04310000, /* Shader Mem Address */
		.end = 0x0431ffff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = KGSL_3D0_IRQ,
		.start = GFX3D_IRQ,
		.end = GFX3D_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct kgsl_iommu_ctx kgsl_3d0_iommu0_ctxs[] = {
	{ "gfx3d_user", 0 },
	{ "gfx3d_priv", 1 },
};

static const struct kgsl_iommu_ctx kgsl_3d0_iommu1_ctxs[] = {
	{ "gfx3d1_user", 0 },
	{ "gfx3d1_priv", 1 },
};

static struct kgsl_device_iommu_data kgsl_3d0_iommu_data[] = {
	{
		.iommu_ctxs = kgsl_3d0_iommu0_ctxs,
		.iommu_ctx_count = ARRAY_SIZE(kgsl_3d0_iommu0_ctxs),
		.physstart = 0x07C00000,
		.physend = 0x07C00000 + SZ_1M - 1,
	},
	{
		.iommu_ctxs = kgsl_3d0_iommu1_ctxs,
		.iommu_ctx_count = ARRAY_SIZE(kgsl_3d0_iommu1_ctxs),
		.physstart = 0x07D00000,
		.physend = 0x07D00000 + SZ_1M - 1,
	},
};

static struct kgsl_device_platform_data kgsl_3d0_pdata = {
	.pwrlevel = {
		{
			.gpu_freq = 400000000,
			.bus_freq = 4,
			.io_fraction = 0,
		},
		{
			.gpu_freq = 320000000,
			.bus_freq = 3,
			.io_fraction = 33,
		},
		{
			.gpu_freq = 200000000,
			.bus_freq = 2,
			.io_fraction = 100,
		},
		{
			.gpu_freq = 128000000,
			.bus_freq = 1,
			.io_fraction = 100,
		},
		{
			.gpu_freq = 27000000,
			.bus_freq = 0,
		},
	},
	.init_level = 1,
	.num_levels = 5,
	.set_grp_async = NULL,
	.idle_timeout = HZ/10,
	.strtstp_sleepwake = true,
	.clk_map = KGSL_CLK_CORE | KGSL_CLK_IFACE | KGSL_CLK_MEM_IFACE,
#ifdef CONFIG_MSM_BUS_SCALING
	.bus_scale_table = &grp3d_bus_scale_pdata,
#endif
	.iommu_data = kgsl_3d0_iommu_data,
	.iommu_count = ARRAY_SIZE(kgsl_3d0_iommu_data),
#ifdef CONFIG_MSM_DCVS
	.core_info = &grp3d_core_info,
#endif
};

struct platform_device device_kgsl_3d0 = {
	.name = "kgsl-3d0",
	.id = 0,
	.num_resources = ARRAY_SIZE(kgsl_3d0_resources),
	.resource = kgsl_3d0_resources,
	.dev = {
		.platform_data = &kgsl_3d0_pdata,
	},
};

void __init apq8064_init_gpu(void)
{
	unsigned int version = socinfo_get_version();

	if (cpu_is_apq8064ab())
		kgsl_3d0_pdata.pwrlevel[0].gpu_freq = 450000000;
	if (SOCINFO_VERSION_MAJOR(version) == 2) {
		kgsl_3d0_pdata.chipid = ADRENO_CHIPID(3, 2, 0, 2);
	} else {
		/* The bootloader has started returning 1.2 for chips that
		   are either 1.1 or 1.2. To handle that and default any
		   future revisions to this path, check for minor version >=1 */
		if ((SOCINFO_VERSION_MAJOR(version) == 1) &&
				(SOCINFO_VERSION_MINOR(version) >= 1))
			kgsl_3d0_pdata.chipid = ADRENO_CHIPID(3, 2, 0, 1);
		else
			kgsl_3d0_pdata.chipid = ADRENO_CHIPID(3, 2, 0, 0);
	}

	platform_device_register(&device_kgsl_3d0);
}
