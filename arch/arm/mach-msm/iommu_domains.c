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
#include <linux/vmalloc.h>
#include <asm/sizes.h>
#include <asm/page.h>
#include <linux/init.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/socinfo.h>

/* dummy 4k for overmapping */
char iommu_dummy[2*PAGE_SIZE-4];

struct msm_iommu_domain {
	/* iommu domain to map in */
	struct iommu_domain *domain;
	/* total number of allocations from this domain */
	atomic_t allocation_cnt;
	/* number of iova pools */
	int npools;
	/*
	 * array of gen_pools for allocating iovas.
	 * behavior is undefined if these overlap
	 */
	struct mem_pool *iova_pools;

};


struct {
	char *name;
	int  domain;
} msm_iommu_ctx_names[] = {
};


static struct msm_iommu_domain msm_iommu_domains[] = {
};

int msm_iommu_map_extra(struct iommu_domain *domain,
				unsigned long start_iova,
				unsigned long size,
				int cached)
{
	int i, ret = 0;
	struct scatterlist *sglist;
	unsigned int nrpages = PFN_ALIGN(size) >> PAGE_SHIFT;
	struct page *dummy_page = phys_to_page(
					PFN_ALIGN(virt_to_phys(iommu_dummy)));

	sglist = vmalloc(sizeof(*sglist) * nrpages);
	if (!sglist) {
		ret = -ENOMEM;
		goto err1;
	}

	sg_init_table(sglist, nrpages);

	for (i = 0; i < nrpages; i++)
		sg_set_page(&sglist[i], dummy_page, PAGE_SIZE, 0);

	ret = iommu_map_range(domain, start_iova, sglist, size, cached);
	if (ret) {
		pr_err("%s: could not map extra %lx in domain %p\n",
			__func__, start_iova, domain);
	}

	vfree(sglist);
err1:
	return ret;
}


struct iommu_domain *msm_get_iommu_domain(int domain_num)
{
	if (domain_num >= 0 && domain_num < MAX_DOMAINS)
		return msm_iommu_domains[domain_num].domain;
	else
		return NULL;
}

static unsigned long subsystem_to_domain_tbl[] = {
	VIDEO_DOMAIN,
	VIDEO_DOMAIN,
	CAMERA_DOMAIN,
	DISPLAY_DOMAIN,
	ROTATOR_DOMAIN,
	0xFFFFFFFF
};

unsigned long msm_subsystem_get_domain_no(int subsys_id)
{
	if (subsys_id > INVALID_SUBSYS_ID && subsys_id <= MAX_SUBSYSTEM_ID &&
	    subsys_id < ARRAY_SIZE(subsystem_to_domain_tbl))
		return subsystem_to_domain_tbl[subsys_id];
	else
		return subsystem_to_domain_tbl[MAX_SUBSYSTEM_ID];
}

unsigned long msm_subsystem_get_partition_no(int subsys_id)
{
	switch (subsys_id) {
	case MSM_SUBSYSTEM_VIDEO_FWARE:
		return VIDEO_FIRMWARE_POOL;
	case MSM_SUBSYSTEM_VIDEO:
		return VIDEO_MAIN_POOL;
	case MSM_SUBSYSTEM_CAMERA:
	case MSM_SUBSYSTEM_DISPLAY:
	case MSM_SUBSYSTEM_ROTATOR:
		return GEN_POOL;
	default:
		return 0xFFFFFFFF;
	}
}

unsigned long msm_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					unsigned long align)
{
	struct mem_pool *pool;
	unsigned long iova;

	if (iommu_domain >= MAX_DOMAINS)
		return 0;

	if (partition_no >= msm_iommu_domains[iommu_domain].npools)
		return 0;

	pool = &msm_iommu_domains[iommu_domain].iova_pools[partition_no];

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

	if (iommu_domain >= MAX_DOMAINS) {
		WARN(1, "Invalid domain %d\n", iommu_domain);
		return;
	}

	if (partition_no >= msm_iommu_domains[iommu_domain].npools) {
		WARN(1, "Invalid partition %d for domain %d\n",
			partition_no, iommu_domain);
		return;
	}

	pool = &msm_iommu_domains[iommu_domain].iova_pools[partition_no];

	if (!pool)
		return;

	pool->free += size;
	gen_pool_free(pool->gpool, iova, size);
}

int msm_use_iommu()
{
	/* Kill use of the iommu by these clients for now. */
	return 0;
}

static int __init msm_subsystem_iommu_init(void)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(msm_iommu_domains); i++) {
		msm_iommu_domains[i].domain = iommu_domain_alloc(0);
		if (!msm_iommu_domains[i].domain)
			continue;

		for (j = 0; j < msm_iommu_domains[i].npools; j++) {
			struct mem_pool *pool = &msm_iommu_domains[i].
							iova_pools[j];
			mutex_init(&pool->pool_mutex);
			pool->gpool = gen_pool_create(PAGE_SHIFT, -1);

			if (!pool->gpool) {
				pr_err("%s: domain %d: could not allocate iova"
					" pool. iommu programming will not work"
					" with iova space %d\n", __func__,
					i, j);
				continue;
			}

			if (gen_pool_add(pool->gpool, pool->paddr, pool->size,
						-1)) {
				pr_err("%s: domain %d: could not add memory to"
					" iova pool. iommu programming will not"
					" work with iova space %d\n", __func__,
					i, j);
				gen_pool_destroy(pool->gpool);
				pool->gpool = NULL;
				continue;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(msm_iommu_ctx_names); i++) {
		int domain_idx;
		struct device *ctx = msm_iommu_get_ctx(
						msm_iommu_ctx_names[i].name);

		if (!ctx)
			continue;

		domain_idx = msm_iommu_ctx_names[i].domain;

		if (!msm_iommu_domains[domain_idx].domain)
			continue;

		if (iommu_attach_device(msm_iommu_domains[domain_idx].domain,
					ctx)) {
			WARN(1, "%s: could not attach domain %d to context %s."
				" iommu programming will not occur.\n",
				__func__, domain_idx,
				msm_iommu_ctx_names[i].name);
			continue;
		}
	}

	return 0;
}
device_initcall(msm_subsystem_iommu_init);
