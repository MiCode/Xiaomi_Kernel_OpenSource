/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef MSM_IOMMU_PRIV_H
#define MSM_IOMMU_PRIV_H

/**
 * struct msm_iommu_pt - Container for first level page table and its
 * attributes.
 * fl_table: Pointer to the first level page table.
 * redirect: Set to 1 if L2 redirect for page tables are enabled, 0 otherwise.
 */
struct msm_iommu_pt {
	unsigned long *fl_table;
	int redirect;
};

/**
 * struct msm_iommu_priv - Container for page table attributes and other
 * private iommu domain information.
 * attributes.
 * pt: Page table attribute structure
 * list_attached: List of devices (contexts) attached to this domain.
 * client_name: Name of the domain client.
 */
struct msm_iommu_priv {
	struct msm_iommu_pt pt;
	struct list_head list_attached;
	const char *client_name;
};

#endif
