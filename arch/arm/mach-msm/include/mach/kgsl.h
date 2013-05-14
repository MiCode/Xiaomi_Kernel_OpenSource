/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ARCH_ARM_MACH_KGSL_H
#define _ARCH_ARM_MACH_KGSL_H

/* Clock flags to show which clocks should be controled by a given platform */
#define KGSL_CLK_SRC	0x00000001
#define KGSL_CLK_CORE	0x00000002
#define KGSL_CLK_IFACE	0x00000004
#define KGSL_CLK_MEM	0x00000008
#define KGSL_CLK_MEM_IFACE 0x00000010
#define KGSL_CLK_AXI	0x00000020
#define KGSL_CLK_ALT_MEM_IFACE 0x00000040

#define KGSL_MAX_PWRLEVELS 10

#define KGSL_CONVERT_TO_MBPS(val) \
	(val*1000*1000U)

#define KGSL_3D0_REG_MEMORY	"kgsl_3d0_reg_memory"
#define KGSL_3D0_SHADER_MEMORY	"kgsl_3d0_shader_memory"
#define KGSL_3D0_IRQ		"kgsl_3d0_irq"
#define KGSL_2D0_REG_MEMORY	"kgsl_2d0_reg_memory"
#define KGSL_2D0_IRQ		"kgsl_2d0_irq"
#define KGSL_2D1_REG_MEMORY	"kgsl_2d1_reg_memory"
#define KGSL_2D1_IRQ		"kgsl_2d1_irq"

#define ADRENO_CHIPID(_co, _ma, _mi, _pa) \
	((((_co) & 0xFF) << 24) | \
	 (((_ma) & 0xFF) << 16) | \
	 (((_mi) & 0xFF) << 8) | \
	 ((_pa) & 0xFF))

enum kgsl_iommu_context_id {
	KGSL_IOMMU_CONTEXT_USER = 0,
	KGSL_IOMMU_CONTEXT_PRIV = 1,
};

struct kgsl_iommu_ctx {
	const char *iommu_ctx_name;
	enum kgsl_iommu_context_id ctx_id;
};

/*
 * struct kgsl_device_iommu_data - Struct holding iommu context data obtained
 * from dtsi file
 * @iommu_ctxs:		Pointer to array of struct hoding context name and id
 * @iommu_ctx_count:	Number of contexts defined in the dtsi file
 * @iommu_halt_enable:	Indicated if smmu halt h/w feature is supported
 * @physstart:		Start of iommu registers physical address
 * @physend:		End of iommu registers physical address
 */
struct kgsl_device_iommu_data {
	const struct kgsl_iommu_ctx *iommu_ctxs;
	int iommu_ctx_count;
	int iommu_halt_enable;
	unsigned int physstart;
	unsigned int physend;
};

struct kgsl_pwrlevel {
	unsigned int gpu_freq;
	unsigned int bus_freq;
	unsigned int io_fraction;
};

struct kgsl_device_platform_data {
	struct kgsl_pwrlevel pwrlevel[KGSL_MAX_PWRLEVELS];
	int init_level;
	int num_levels;
	int (*set_grp_async)(void);
	unsigned int idle_timeout;
	bool strtstp_sleepwake;
	unsigned int clk_map;
	unsigned int idle_needed;
	unsigned int step_mul;
	struct msm_bus_scale_pdata *bus_scale_table;
	struct kgsl_device_iommu_data *iommu_data;
	int iommu_count;
	struct msm_dcvs_core_info *core_info;
	struct coresight_device *csdev;
	struct coresight_platform_data *coresight_pdata;
	unsigned int chipid;
};

#endif
