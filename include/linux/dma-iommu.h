/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014-2015 ARM Ltd.
 */
#ifndef __DMA_IOMMU_H
#define __DMA_IOMMU_H

#include <linux/errno.h>
#include <linux/types.h>

#ifdef CONFIG_IOMMU_DMA
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/msi.h>

struct iova_domain;

/* Domain management interface for IOMMU drivers */
int iommu_get_dma_cookie(struct iommu_domain *domain);
int iommu_get_msi_cookie(struct iommu_domain *domain, dma_addr_t base);
void iommu_put_dma_cookie(struct iommu_domain *domain);

int dma_info_to_prot(enum dma_data_direction dir, bool coherent,
		     unsigned long attrs);
size_t iommu_dma_prepare_map_sg(struct device *dev, struct iova_domain *iovad,
				struct scatterlist *sg, int nents);
int iommu_dma_finalise_sg(struct device *dev, struct scatterlist *sg,
		int nents, dma_addr_t dma_addr);
void iommu_dma_invalidate_sg(struct scatterlist *sg, int nents);
int iommu_dma_mmap(struct device *dev, struct vm_area_struct *vma,
		   void *cpu_addr, dma_addr_t dma_addr, size_t size,
		   unsigned long attrs);
int iommu_dma_get_sgtable(struct device *dev, struct sg_table *sgt,
			  void *cpu_addr, dma_addr_t dma_addr, size_t size,
			  unsigned long attrs);

/* Setup call for arch DMA mapping code */
void iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size);

/* The DMA API isn't _quite_ the whole story, though... */
/*
 * iommu_dma_prepare_msi() - Map the MSI page in the IOMMU device
 *
 * The MSI page will be stored in @desc.
 *
 * Return: 0 on success otherwise an error describing the failure.
 */
int iommu_dma_prepare_msi(struct msi_desc *desc, phys_addr_t msi_addr);

/* Update the MSI message if required. */
void iommu_dma_compose_msi_msg(struct msi_desc *desc,
			       struct msi_msg *msg);

void iommu_dma_get_resv_regions(struct device *dev, struct list_head *list);

int iommu_dma_reserve_iova(struct device *dev, dma_addr_t base,
			   u64 size);

int iommu_dma_enable_best_fit_algo(struct device *dev);

#ifdef CONFIG_DMA_CONFIGURE_ALIGNMENT
int iommu_dma_configure_alignment(struct device *dev, bool force_no_align);
#else /* CONFIG_DMA_CONFIGURE_ALIGNMENT */
static inline int iommu_dma_configure_alignment(struct device *dev,
						bool force_no_align)
{
	return -ENOTSUPP;
}
#endif

#else /* CONFIG_IOMMU_DMA */

struct iommu_domain;
struct msi_desc;
struct msi_msg;
struct device;

static inline void iommu_setup_dma_ops(struct device *dev, u64 dma_base,
		u64 size)
{
}

static inline int iommu_get_dma_cookie(struct iommu_domain *domain)
{
	return -ENODEV;
}

static inline int iommu_get_msi_cookie(struct iommu_domain *domain, dma_addr_t base)
{
	return -ENODEV;
}

static inline void iommu_put_dma_cookie(struct iommu_domain *domain)
{
}

static inline int iommu_dma_prepare_msi(struct msi_desc *desc,
					phys_addr_t msi_addr)
{
	return 0;
}

static inline void iommu_dma_compose_msi_msg(struct msi_desc *desc,
					     struct msi_msg *msg)
{
}

static inline void iommu_dma_get_resv_regions(struct device *dev, struct list_head *list)
{
}

static inline int iommu_dma_reserve_iova(struct device *dev, dma_addr_t base,
					 u64 size)
{
	return -ENODEV;
}

static inline int iommu_dma_enable_best_fit_algo(struct device *dev)
{
	return -ENODEV;
}

static inline int iommu_dma_configure_alignment(struct device *dev,
						bool force_no_align)
{
	return -ENODEV;
}

#endif	/* CONFIG_IOMMU_DMA */
#endif	/* __DMA_IOMMU_H */
