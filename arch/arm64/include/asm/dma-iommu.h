#ifndef ASMARM_DMA_IOMMU_H
#define ASMARM_DMA_IOMMU_H

#ifdef __KERNEL__

#include <linux/err.h>
#include <linux/mm_types.h>
#include <linux/scatterlist.h>
#include <linux/dma-debug.h>
#include <linux/kmemcheck.h>
#include <linux/kref.h>
#include <linux/dma-mapping-fast.h>

struct dma_iommu_mapping {
	/* iommu specific data */
	struct iommu_domain	*domain;

	void			*bitmap;
	size_t			bits;
	dma_addr_t		base;

	spinlock_t		lock;
	struct kref		kref;

	struct dma_fast_smmu_mapping *fast;
};

#ifdef CONFIG_ARM64_DMA_USE_IOMMU

struct dma_iommu_mapping *
arm_iommu_create_mapping(struct bus_type *bus, dma_addr_t base, size_t size);

void arm_iommu_release_mapping(struct dma_iommu_mapping *mapping);

int arm_iommu_attach_device(struct device *dev,
					struct dma_iommu_mapping *mapping);
void arm_iommu_detach_device(struct device *dev);

#else  /* !CONFIG_ARM64_DMA_USE_IOMMU */

static inline struct dma_iommu_mapping *
arm_iommu_create_mapping(struct bus_type *bus, dma_addr_t base, size_t size)
{
	return ERR_PTR(-ENOMEM);
}

static inline void arm_iommu_release_mapping(struct dma_iommu_mapping *mapping)
{
}

static inline int arm_iommu_attach_device(struct device *dev,
			struct dma_iommu_mapping *mapping)
{
	return -ENODEV;
}

static inline void arm_iommu_detach_device(struct device *dev)
{
}

#endif	/* CONFIG_ARM64_DMA_USE_IOMMU */

#endif /* __KERNEL__ */
#endif
