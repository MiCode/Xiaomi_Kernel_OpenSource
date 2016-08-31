/*
 * arch/arm/mach-tegra/include/mach/tegra_smmu.h
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __ARCH_ARM_MACH_TEGRA_SMMU_H
#define __ARCH_ARM_MACH_TEGRA_SMMU_H

int tegra_smmu_save(void);
int tegra_smmu_restore(void);

#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
extern struct resource *tegra_smmu_window(int wnum);
extern int tegra_smmu_window_count(void);
#endif

struct iommu_linear_map {
	dma_addr_t start;
	size_t size;
};

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
/*
 * ASID[0] for the system default
 * ASID[1] for PPCS, which has SDMMC
 * ASID[2] for PPCS1
 * ASID[3][4] open for drivers, first come, first served.
 */
enum {
	SYSTEM_DEFAULT,
	SYSTEM_PROTECTED,
	PPCS1_ASID,
	SYSTEM_DC,
	SYSTEM_DCB,
	SYSTEM_GK20A,
	SDMMC1A_ASID,
	SDMMC2A_ASID,
	SDMMC3A_ASID,
	SDMMC4A_ASID,
	NUM_ASIDS,
};

/* Maximum number of iommu address spaces in the system */
#define TEGRA_IOMMU_NUM_ASIDS NUM_ASIDS
extern struct dma_iommu_mapping *tegra_smmu_get_map(struct device *dev,
						    u64 swgids);
void tegra_smmu_unmap_misc_device(struct device *dev);
void tegra_smmu_map_misc_device(struct device *dev);
int tegra_smmu_get_asid(struct device *dev);
#else
#define TEGRA_IOMMU_NUM_ASIDS 1

static inline struct dma_iommu_mapping *tegra_smmu_get_map(struct device *dev,
							   u64 swgids)
{
	return NULL;
}

static inline void tegra_smmu_unmap_misc_device(struct device *dev)
{
}

static inline void tegra_smmu_map_misc_device(struct device *dev)
{
}

static inline int tegra_smmu_get_asid(struct device *dev)
{
	return 0;
}
#endif


extern u64 tegra_smmu_fixup_swgids(struct device *dev,
				   struct iommu_linear_map **map);

#endif /* __ARCH_ARM_MACH_TEGRA_SMMU_H */
