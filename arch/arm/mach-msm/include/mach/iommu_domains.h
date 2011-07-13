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

/*
 * Nothing in this file is to be used outside of the iommu wrappers.
 * Do NOT try and use anything here in a driver. Doing so is incorrect.
 */

/*
 * These subsytem ids are NOT for public use. Please check the iommu
 * wrapper header for the properly abstracted id to pass in.
 */

#if defined(CONFIG_MSM_IOMMU)
extern struct iommu_domain *msm_subsystem_get_domain(int subsys_id);

extern struct mem_pool *msm_subsystem_get_pool(int subsys_id);
#else
static inline struct iommu_domain
	*msm_subsystem_get_domain(int subsys_id) { return NULL; }

static inline struct mem_pool
	*msm_subsystem_get_pool(int subsys_id) { return NULL; }
#endif

#endif
