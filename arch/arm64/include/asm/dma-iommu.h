/* SPDX-License-Identifier: GPL-2.0 */
#ifndef ASMARM_DMA_IOMMU_H
#define ASMARM_DMA_IOMMU_H

#ifdef __KERNEL__

#include <linux/err.h>
#include <linux/mm_types.h>
#include <linux/scatterlist.h>
#include <linux/dma-debug.h>
#include <linux/kref.h>
#include <linux/dma-mapping-fast.h>

struct dma_iommu_mapping {
	/* iommu specific data */
	struct iommu_domain	*domain;
	bool			init;
	struct kref		kref;
	const struct dma_map_ops *ops;

	/* Protects bitmap */
	spinlock_t		lock;
	void			*bitmap;
	size_t			bits;
	dma_addr_t		base;

	struct dma_fast_smmu_mapping *fast;
};

#ifdef CONFIG_ARM64_DMA_USE_IOMMU

struct dma_iommu_mapping *
__depr_arm_iommu_create_mapping(struct bus_type *bus, dma_addr_t base,
				size_t size);

void __depr_arm_iommu_release_mapping(struct dma_iommu_mapping *mapping);

int __depr_arm_iommu_attach_device(struct device *dev,
					struct dma_iommu_mapping *mapping);
void __depr_arm_iommu_detach_device(struct device *dev);

void arm_iommu_put_dma_cookie(struct iommu_domain *domain);
#else  /* !CONFIG_ARM64_DMA_USE_IOMMU */

static inline struct dma_iommu_mapping *
__depr_arm_iommu_create_mapping(struct bus_type *bus, dma_addr_t base,
				size_t size)
{
	return NULL;
}

static inline void
__depr_arm_iommu_release_mapping(struct dma_iommu_mapping *mapping)
{
}

static inline int __depr_arm_iommu_attach_device(struct device *dev,
			struct dma_iommu_mapping *mapping)
{
	return -ENODEV;
}

static inline void __depr_arm_iommu_detach_device(struct device *dev)
{
}

static inline void arm_iommu_put_dma_cookie(struct iommu_domain *domain) {}
#endif	/* CONFIG_ARM64_DMA_USE_IOMMU */

#endif /* __KERNEL__ */
#endif
