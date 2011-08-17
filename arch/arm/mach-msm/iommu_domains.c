/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <asm/sizes.h>
#include <asm/page.h>
#include <linux/init.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>

struct msm_iommu_domain {
	int domain_idx;
	int iova_pool_idx;
};

enum {
	GLOBAL_DOMAIN,
	VIDEO_DOMAIN,
	EMPTY_DOMAIN,
	MAX_DOMAINS
};

enum {
	GLOBAL_MEMORY_POOL,
	VIDEO_FIRMWARE_POOL,
	VIDEO_ALLOC_POOL,
};

struct {
	char *name;
	int  domain;
} msm_iommu_ctx_names[] = {
	/* Camera */
	{
		.name = "vpe_src",
		.domain = GLOBAL_DOMAIN,
	},
	/* Camera */
	{
		.name = "vpe_dst",
		.domain = GLOBAL_DOMAIN,
	},
	/* Camera */
	{
		.name = "vfe_imgwr",
		.domain = GLOBAL_DOMAIN,
	},
	/* Camera */
	{
		.name = "vfe_misc",
		.domain = GLOBAL_DOMAIN,
	},
	/* Camera */
	{
		.name =	"ijpeg_src",
		.domain = GLOBAL_DOMAIN,
	},
	/* Camera */
	{
		.name =	"ijpeg_dst",
		.domain = GLOBAL_DOMAIN,
	},
	/* Camera */
	{
		.name = "jpegd_src",
		.domain = GLOBAL_DOMAIN,
	},
	/* Camera */
	{
		.name = "jpegd_dst",
		.domain = GLOBAL_DOMAIN,
	},
	/* Display */
	{
		.name = "mdp_vg1",
		.domain = GLOBAL_DOMAIN,
	},
	/* Display */
	{
		.name = "mdp_vg2",
		.domain = GLOBAL_DOMAIN,
	},
	/* Display */
	{
		.name = "mdp_rgb1",
		.domain = GLOBAL_DOMAIN,
	},
	/* Display */
	{
		.name = "mdp_rgb2",
		.domain = GLOBAL_DOMAIN,
	},
	/* Rotator */
	{
		.name = "rot_src",
		.domain = GLOBAL_DOMAIN,
	},
	/* Rotator */
	{
		.name = "rot_dst",
		.domain = GLOBAL_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_a_mm1",
		.domain = VIDEO_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_b_mm2",
		.domain = VIDEO_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_a_stream",
		.domain = VIDEO_DOMAIN,
	},
};

static struct iommu_domain *msm_iommu_domains[MAX_DOMAINS];

static struct mem_pool msm_iommu_iova_pools[] = {
	[GLOBAL_MEMORY_POOL] = {
		.paddr	= SZ_4K,
		.size	= SZ_2G - SZ_4K,
	},
	/*
	 * The video hardware has several constraints:
	 * 1) The start address for firmware must be 128K aligned
	 * 2) The video firmware must exist at a lower address than
	 *	all other video allocations
	 * 3) Video allocations cannot be more than 256MB away from the
	 *	firmware
	 *
	 * Splitting the video pools makes sure that firmware will
	 * always be lower than regular allocations and the maximum
	 * size of 256MB will be enforced.
	 */
	[VIDEO_FIRMWARE_POOL] = {
		.paddr	= SZ_128K,
		.size	= SZ_16M - SZ_128K,
	},
	[VIDEO_ALLOC_POOL] = {
		.paddr	= SZ_16M,
		.size	= SZ_256M - SZ_16M - SZ_128K,
	}
};

static struct msm_iommu_domain msm_iommu_subsystems[] = {
	[JPEGD_SUBSYS_ID]	= {
		.domain_idx	= GLOBAL_DOMAIN,
		.iova_pool_idx	= GLOBAL_MEMORY_POOL,
	},
	[VPE_SUBSYS_ID]		= {
		.domain_idx	= GLOBAL_DOMAIN,
		.iova_pool_idx	= GLOBAL_MEMORY_POOL,
	},
	[MDP0_SUBSYS_ID]	= {
		.domain_idx	= GLOBAL_DOMAIN,
		.iova_pool_idx	= GLOBAL_MEMORY_POOL,
	},
	[MDP1_SUBSYS_ID]	= {
		.domain_idx	= GLOBAL_DOMAIN,
		.iova_pool_idx	= GLOBAL_MEMORY_POOL,
	},
	[ROT_SUBSYS_ID]		= {
		.domain_idx	= GLOBAL_DOMAIN,
		.iova_pool_idx	= GLOBAL_MEMORY_POOL,
	},
	[IJPEG_SUBSYS_ID]	= {
		.domain_idx	= GLOBAL_DOMAIN,
		.iova_pool_idx	= GLOBAL_MEMORY_POOL,
	},
	[VFE_SUBSYS_ID]		= {
		.domain_idx	= GLOBAL_DOMAIN,
		.iova_pool_idx	= GLOBAL_MEMORY_POOL,
	},
	[VCODEC_A_SUBSYS_ID]	= {
		.domain_idx	= VIDEO_DOMAIN,
		.iova_pool_idx	= VIDEO_ALLOC_POOL,
	},
	[VCODEC_B_SUBSYS_ID]	= {
		.domain_idx	= VIDEO_DOMAIN,
		.iova_pool_idx	= VIDEO_ALLOC_POOL,
	},
	[VIDEO_FWARE_ID]	= {
		.domain_idx	= VIDEO_DOMAIN,
		.iova_pool_idx	= VIDEO_FIRMWARE_POOL,
	}
};

struct iommu_domain *msm_subsystem_get_domain(int subsys_id)
{
	int id = msm_iommu_subsystems[subsys_id].domain_idx;

	return msm_iommu_domains[id];
}

struct mem_pool *msm_subsystem_get_pool(int subsys_id)
{
	int id = msm_iommu_subsystems[subsys_id].iova_pool_idx;

	return &msm_iommu_iova_pools[id];
}

static int __init msm_subsystem_iommu_init(void)
{
	int i;

	for (i = 0; i < (ARRAY_SIZE(msm_iommu_domains) - 1); i++)
		msm_iommu_domains[i] = iommu_domain_alloc();

	for (i = 0; i < ARRAY_SIZE(msm_iommu_iova_pools); i++) {
		mutex_init(&msm_iommu_iova_pools[i].pool_mutex);
		msm_iommu_iova_pools[i].gpool = gen_pool_create(PAGE_SHIFT, -1);

		if (!msm_iommu_iova_pools[i].gpool) {
			pr_err("%s: could not allocate iova pool. iommu"
				" programming will not work with iova space"
				" %d\n", __func__, i);
			continue;
		}

		if (gen_pool_add(msm_iommu_iova_pools[i].gpool,
				msm_iommu_iova_pools[i].paddr,
				msm_iommu_iova_pools[i].size,
				-1)) {
			pr_err("%s: could not add memory to iova pool. iommu"
				" programming will not work with iova space"
				" %d\n", __func__, i);
			gen_pool_destroy(msm_iommu_iova_pools[i].gpool);
			msm_iommu_iova_pools[i].gpool = NULL;
			continue;
		}
	}

	for (i = 0; i < ARRAY_SIZE(msm_iommu_ctx_names); i++) {
		int domain_idx;
		struct device *ctx = msm_iommu_get_ctx(
						msm_iommu_ctx_names[i].name);

		if (!ctx)
			continue;

		domain_idx = msm_iommu_ctx_names[i].domain;

		if (!msm_iommu_domains[domain_idx])
			continue;

		if (iommu_attach_device(msm_iommu_domains[domain_idx], ctx)) {
			pr_err("%s: could not attach domain %d to context %s."
				" iommu programming will not occur.\n",
				__func__, domain_idx,
				msm_iommu_ctx_names[i].name);
			msm_iommu_subsystems[i].domain_idx = EMPTY_DOMAIN;
			continue;
		}
	}

	return 0;
}
device_initcall(msm_subsystem_iommu_init);
