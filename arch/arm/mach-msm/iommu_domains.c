/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <mach/msm_subsystem_map.h>
#include <linux/memory_alloc.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <asm/sizes.h>
#include <asm/page.h>
#include <linux/init.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/socinfo.h>

/* dummy 64K for overmapping */
char iommu_dummy[2*SZ_64K-4];

struct msm_iommu_domain_state {
	struct msm_iommu_domain *domains;
	int ndomains;
};

static struct msm_iommu_domain_state domain_state;

int msm_iommu_map_extra(struct iommu_domain *domain,
				unsigned long start_iova,
				unsigned long size,
				unsigned long page_size,
				int cached)
{
	int i, ret_value = 0;
	unsigned long order = get_order(page_size);
	unsigned long aligned_size = ALIGN(size, page_size);
	unsigned long nrpages = aligned_size >> (PAGE_SHIFT + order);
	unsigned long phy_addr = ALIGN(virt_to_phys(iommu_dummy), page_size);
	unsigned long temp_iova = start_iova;

	for (i = 0; i < nrpages; i++) {
		int ret = iommu_map(domain, temp_iova, phy_addr, order, cached);
		if (ret) {
			pr_err("%s: could not map %lx in domain %p, error: %d\n",
				__func__, start_iova, domain, ret);
			ret_value = -EAGAIN;
			goto out;
		}
		temp_iova += page_size;
	}
	return ret_value;
out:
	for (; i > 0; --i) {
		temp_iova -= page_size;
		iommu_unmap(domain, start_iova, order);
	}
	return ret_value;
}

void msm_iommu_unmap_extra(struct iommu_domain *domain,
				unsigned long start_iova,
				unsigned long size,
				unsigned long page_size)
{
	int i;
	unsigned long order = get_order(page_size);
	unsigned long aligned_size = ALIGN(size, page_size);
	unsigned long nrpages =  aligned_size >> (PAGE_SHIFT + order);
	unsigned long temp_iova = start_iova;

	for (i = 0; i < nrpages; ++i) {
		iommu_unmap(domain, temp_iova, order);
		temp_iova += page_size;
	}
}

static int msm_iommu_map_iova_phys(struct iommu_domain *domain,
				unsigned long iova,
				unsigned long phys,
				unsigned long size,
				int cached)
{
	int ret;
	struct scatterlist *sglist;

	sglist = vmalloc(sizeof(*sglist));
	if (!sglist) {
		ret = -ENOMEM;
		goto err1;
	}

	sg_init_table(sglist, 1);
	sglist->length = size;
	sglist->offset = 0;
	sglist->dma_address = phys;

	ret = iommu_map_range(domain, iova, sglist, size, cached);
	if (ret) {
		pr_err("%s: could not map extra %lx in domain %p\n",
			__func__, iova, domain);
	}

	vfree(sglist);
err1:
	return ret;

}

int msm_iommu_map_contig_buffer(unsigned long phys,
				unsigned int domain_no,
				unsigned int partition_no,
				unsigned long size,
				unsigned long align,
				unsigned long cached,
				unsigned long *iova_val)
{
	unsigned long iova;
	int ret;

	if (size & (align - 1))
		return -EINVAL;

	iova = msm_allocate_iova_address(domain_no, partition_no, size, align);

	if (!iova)
		return -ENOMEM;

	ret = msm_iommu_map_iova_phys(msm_get_iommu_domain(domain_no), iova,
					phys, size, cached);

	if (ret)
		msm_free_iova_address(iova, domain_no, partition_no, size);
	else
		*iova_val = iova;

	return ret;
}

void msm_iommu_unmap_contig_buffer(unsigned long iova,
					unsigned int domain_no,
					unsigned int partition_no,
					unsigned long size)
{
	iommu_unmap_range(msm_get_iommu_domain(domain_no), iova, size);
	msm_free_iova_address(iova, domain_no, partition_no, size);
}

struct iommu_domain *msm_get_iommu_domain(int domain_num)
{
	if (domain_num >= 0 && domain_num < domain_state.ndomains)
		return domain_state.domains[domain_num].domain;
	else
		return NULL;
}

unsigned long msm_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					unsigned long align)
{
	struct mem_pool *pool;
	unsigned long iova;

	if (iommu_domain >= domain_state.ndomains)
		return 0;

	if (partition_no >= domain_state.domains[iommu_domain].npools)
		return 0;

	pool = &domain_state.domains[iommu_domain].iova_pools[partition_no];

	if (!pool->gpool)
		return 0;

	iova = gen_pool_alloc_aligned(pool->gpool, size, ilog2(align));
	if (iova)
		pool->free -= size;

	return iova;
}

void msm_free_iova_address(unsigned long iova,
			   unsigned int iommu_domain,
			   unsigned int partition_no,
			   unsigned long size)
{
	struct mem_pool *pool;

	if (iommu_domain >= domain_state.ndomains) {
		WARN(1, "Invalid domain %d\n", iommu_domain);
		return;
	}

	if (partition_no >= domain_state.domains[iommu_domain].npools) {
		WARN(1, "Invalid partition %d for domain %d\n",
			partition_no, iommu_domain);
		return;
	}

	pool = &domain_state.domains[iommu_domain].iova_pools[partition_no];

	if (!pool)
		return;

	pool->free += size;
	gen_pool_free(pool->gpool, iova, size);
}

int msm_use_iommu()
{
	/*
	 * If there are no domains, don't bother trying to use the iommu
	 */
	return domain_state.ndomains && iommu_found();
}

static int __init iommu_domain_probe(struct platform_device *pdev)
{
	struct iommu_domains_pdata *p  = pdev->dev.platform_data;
	int i, j;

	if (!p)
		return -ENODEV;

	domain_state.domains = p->domains;
	domain_state.ndomains = p->ndomains;

	for (i = 0; i < domain_state.ndomains; i++) {
		domain_state.domains[i].domain = iommu_domain_alloc(
							p->domain_alloc_flags);
		if (!domain_state.domains[i].domain)
			continue;

		for (j = 0; j < domain_state.domains[i].npools; j++) {
			struct mem_pool *pool = &domain_state.domains[i].
							iova_pools[j];
			mutex_init(&pool->pool_mutex);
			if (pool->size) {
				pool->gpool = gen_pool_create(PAGE_SHIFT, -1);

				if (!pool->gpool) {
					pr_err("%s: could not allocate pool\n",
						__func__);
					pr_err("%s: domain %d iova space %d\n",
						__func__, i, j);
					continue;
				}

				if (gen_pool_add(pool->gpool, pool->paddr,
						pool->size, -1)) {
					pr_err("%s: could not add memory\n",
						__func__);
					pr_err("%s: domain %d pool %d\n",
						__func__, i, j);
					gen_pool_destroy(pool->gpool);
					pool->gpool = NULL;
					continue;
				}
			} else {
				pool->gpool = NULL;
			}
		}
	}

	for (i = 0; i < p->nnames; i++) {
		int domain_idx;
		struct device *ctx = msm_iommu_get_ctx(
						p->domain_names[i].name);

		if (!ctx)
			continue;

		domain_idx = p->domain_names[i].domain;

		if (!domain_state.domains[domain_idx].domain)
			continue;

		if (iommu_attach_device(domain_state.domains[domain_idx].domain,
					ctx)) {
			WARN(1, "%s: could not attach domain %d to context %s."
				" iommu programming will not occur.\n",
				__func__, domain_idx,
				p->domain_names[i].name);
			continue;
		}
	}

	return 0;
}

static struct platform_driver iommu_domain_driver = {
	.driver         = {
		.name = "iommu_domains",
		.owner = THIS_MODULE
	},
};

static int __init msm_subsystem_iommu_init(void)
{
	return platform_driver_probe(&iommu_domain_driver, iommu_domain_probe);
}
device_initcall(msm_subsystem_iommu_init);
