/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "msm_smem_iface.h"

/**
 * mem_get_cpr_info() - Copy Core Power Reduction (CPR) driver specific
 *			data from Shared memory (SMEM).
 * @cpr_info - Pointer to CPR data. Memory to be allocated and freed by
 *             calling function.
 *
 * Copy CPR specific data from SMEM to cpr_info.
 */

void msm_smem_get_cpr_info(struct cpr_info_type *cpr_info)
{
	struct boot_info_for_apps *boot_info;
	struct cpr_info_type *temp_cpr_info;
	uint32_t smem_boot_info_size;

	boot_info = smem_get_entry(SMEM_BOOT_INFO_FOR_APPS,
					&smem_boot_info_size);
	BUG_ON(!boot_info);
	if (smem_boot_info_size < sizeof(struct boot_info_for_apps)) {
		pr_err("%s: Shared boot info data structure too small!\n",
			__func__);
		BUG();
	} else {
		pr_debug("%s: Shared boot info available.\n", __func__);
	}
	temp_cpr_info = (struct cpr_info_type *) &(boot_info->cpr_info);
	cpr_info->ring_osc = temp_cpr_info->ring_osc;
	cpr_info->turbo_quot = temp_cpr_info->turbo_quot;
	cpr_info->pvs_fuse = temp_cpr_info->pvs_fuse;
}
