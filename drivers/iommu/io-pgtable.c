// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generic page table allocator for IOMMUs.
 *
 * Copyright (C) 2014 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#include <linux/bug.h>
#include <linux/io-pgtable.h>
#include <linux/kernel.h>
#include <linux/types.h>

static const struct io_pgtable_init_fns *
io_pgtable_init_table[IO_PGTABLE_NUM_FMTS] = {
#ifdef CONFIG_IOMMU_IO_PGTABLE_LPAE
	[ARM_32_LPAE_S1] = &io_pgtable_arm_32_lpae_s1_init_fns,
	[ARM_32_LPAE_S2] = &io_pgtable_arm_32_lpae_s2_init_fns,
	[ARM_64_LPAE_S1] = &io_pgtable_arm_64_lpae_s1_init_fns,
	[ARM_64_LPAE_S2] = &io_pgtable_arm_64_lpae_s2_init_fns,
	[ARM_MALI_LPAE] = &io_pgtable_arm_mali_lpae_init_fns,
#endif
#ifdef CONFIG_IOMMU_IO_PGTABLE_ARMV7S
	[ARM_V7S] = &io_pgtable_arm_v7s_init_fns,
#endif
};

struct io_pgtable_ops *alloc_io_pgtable_ops(enum io_pgtable_fmt fmt,
					    struct io_pgtable_cfg *cfg,
					    void *cookie)
{
	struct io_pgtable *iop;
	const struct io_pgtable_init_fns *fns;

	if (fmt >= IO_PGTABLE_NUM_FMTS)
		return NULL;

	fns = io_pgtable_init_table[fmt];
	if (!fns)
		return NULL;

	iop = fns->alloc(cfg, cookie);
	if (!iop)
		return NULL;

	iop->fmt	= fmt;
	iop->cookie	= cookie;
	iop->cfg	= *cfg;

	return &iop->ops;
}
EXPORT_SYMBOL_GPL(alloc_io_pgtable_ops);

/*
 * It is the IOMMU driver's responsibility to ensure that the page table
 * is no longer accessible to the walker by this point.
 */
void free_io_pgtable_ops(struct io_pgtable_ops *ops)
{
	struct io_pgtable *iop;

	if (!ops)
		return;

	iop = container_of(ops, struct io_pgtable, ops);
	io_pgtable_tlb_flush_all(iop);
	io_pgtable_init_table[iop->fmt]->free(iop);
}
EXPORT_SYMBOL_GPL(free_io_pgtable_ops);

void *io_pgtable_alloc_pages(struct io_pgtable_cfg *cfg, void *cookie,
			     int order, gfp_t gfp_mask)
{
	struct device *dev;
	struct page *p;

	if (!cfg)
		return NULL;

	if (cfg->iommu_pgtable_ops && cfg->iommu_pgtable_ops->alloc_pgtable)
		return cfg->iommu_pgtable_ops->alloc_pgtable(cookie, order,
							     gfp_mask);

	dev = cfg->iommu_dev;
	p =  alloc_pages_node(dev ? dev_to_node(dev) : NUMA_NO_NODE,
			      gfp_mask, order);
	if (!p)
		return NULL;
	return page_address(p);
}

void io_pgtable_free_pages(struct io_pgtable_cfg *cfg, void *cookie, void *virt,
			   int order)
{
	if (!cfg)
		return;

	if (cfg->iommu_pgtable_ops && cfg->iommu_pgtable_ops->free_pgtable)
		cfg->iommu_pgtable_ops->free_pgtable(cookie, virt, order);
	else
		free_pages((unsigned long)virt, order);
}
