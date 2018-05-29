/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/arm-smmu-errata.h>

static struct page *guard_pages[VMID_LAST];
static DEFINE_MUTEX(guard_page_lock);

struct page *arm_smmu_errata_get_guard_page(int vmid)
{
	struct page *page;
	int ret;
	int source_vm = VMID_HLOS;
	int dest_vm = vmid;
	int dest_perm = PERM_READ | PERM_WRITE | PERM_EXEC;
	size_t size = ARM_SMMU_MIN_IOVA_ALIGN;

	mutex_lock(&guard_page_lock);
	page = guard_pages[vmid];
	if (page)
		goto out;

	page = alloc_pages(GFP_KERNEL, get_order(size));
	if (!page)
		goto out;

	if (vmid != VMID_HLOS) {
		ret = hyp_assign_phys(page_to_phys(page), PAGE_ALIGN(size),
				&source_vm, 1,
				&dest_vm, &dest_perm, 1);
		if (ret && (ret != -EIO)) {
			__free_pages(page, get_order(size));
			page = NULL;
		}
	}
	guard_pages[vmid] = page;
out:
	mutex_unlock(&guard_page_lock);
	return page;
}
