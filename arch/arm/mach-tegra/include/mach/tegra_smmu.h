/*
 * arch/arm/mach-tegra/include/mach/tegra_smmu.h
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
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
#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
extern struct resource *tegra_smmu_window(int wnum);
extern int tegra_smmu_window_count(void);
#endif

#if  defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU_LINEAR)
static inline void tegra_iommu_map_linear(unsigned long start, size_t size)
{
}
#elif defined(CONFIG_TEGRA_IOMMU_SMMU)
extern void smmu_iommu_map_linear(unsigned long start, size_t size);
#define tegra_iommu_map_linear(start, size) smmu_iommu_map_linear(start, size);
#else
#define tegra_iommu_map_linear(start, size)
#endif

/* FIXME: Should be done in DMA API */
#define dma_map_linear_at(dev, start, size, dir) tegra_iommu_map_linear(start, size)

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
extern struct dma_iommu_mapping *tegra_smmu_get_map(struct device *dev,
						    u64 swgids);
#else
static inline struct dma_iommu_mapping *tegra_smmu_get_map(struct device *dev,
							   u64 swgids)
{
	return NULL;
}
#endif

u64 tegra_smmu_fixup_swgids(struct device *dev);
