/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef _ARCH_IOMMU_DOMAINS_H
#define _ARCH_IOMMU_DOMAINS_H

enum {
	GLOBAL_DOMAIN,
	MAX_DOMAINS
};

enum {
	VIDEO_FIRMWARE_POOL,
	VIDEO_MAIN_POOL,
	VIDEO_MFC_POOL,
	GEN_POOL,
};


#if defined(CONFIG_MSM_IOMMU)

extern struct iommu_domain *msm_get_iommu_domain(int domain_num);

extern unsigned long msm_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					unsigned long align);

extern void msm_free_iova_address(unsigned long iova,
			unsigned int iommu_domain,
			unsigned int partition_no,
			unsigned long size);

extern unsigned long msm_subsystem_get_domain_no(int subsys_id);

extern unsigned long msm_subsystem_get_partition_no(int subsys_id);

extern int msm_use_iommu(void);

extern int msm_iommu_map_extra(struct iommu_domain *domain,
						unsigned long start_iova,
						unsigned long size,
						int cached);

#else
static inline struct iommu_domain
	*msm_get_iommu_domain(int subsys_id) { return NULL; }



static inline unsigned long msm_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					unsigned long align) { return 0; }

static inline void msm_free_iova_address(unsigned long iova,
			unsigned int iommu_domain,
			unsigned int partition_no,
			unsigned long size) { return; }

static inline unsigned long msm_subsystem_get_domain_no(int subsys_id)
{
	return 0xFFFFFFFF;
}

static inline unsigned long msm_subsystem_get_partition_no(int subsys_id)
{
	return 0xFFFFFFFF;
}

static inline int msm_use_iommu(void)
{
	return 0;
}

static inline int msm_iommu_map_extra(struct iommu_domain *domain,
						unsigned long start_iova,
						unsigned long size,
						int cached)
{
	return -ENODEV;
}
#endif

#endif
