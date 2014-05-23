/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
 * unaligned_fl_table: Original address of memory for the page table.
 * fl_table is manually aligned (as per spec) but we need the original address
 * to free the table.
 * fl_table_shadow: This is "copy" of the fl_table with some differences.
 * It stores the same information as fl_table except that instead of storing
 * second level page table address + page table entry descriptor bits it
 * stores the second level page table address and the number of used second
 * level page tables entries. This is used to check whether we need to free
 * the second level page table which allows us to also free the second level
 * page table after doing a TLB invalidate which should catch bugs with
 * clients trying to unmap an address that is being used.
 * fl_table_shadow will use the lower 9 bits for the use count and the upper
 * bits for the second level page table address.
 * sl_table_shadow uses the same concept as fl_table_shadow but for LPAE 2nd
 * level page tables.
 */
#ifdef CONFIG_IOMMU_LPAE
struct msm_iommu_pt {
	u64 *fl_table;
	u64 **sl_table_shadow;
	int redirect;
	u64 *unaligned_fl_table;
};
#else
struct msm_iommu_pt {
	u32 *fl_table;
	int redirect;
	u32 *fl_table_shadow;
};
#endif
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
