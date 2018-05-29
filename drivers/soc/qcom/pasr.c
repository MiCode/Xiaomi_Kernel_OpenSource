/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/memory.h>
#include <linux/module.h>
#include <soc/qcom/rpm-smd.h>

struct memory_refresh_request {
	u64 start;	/* Lower bit signifies action
			 * 0 - disable self-refresh
			 * 1 - enable self-refresh
			 * upper bits are for base address
			 */
	size_t size;	/* size of memory region */
};
#define RPM_DDR_REQ 0x726464

static void mem_region_refresh_control(unsigned long pfn,
		unsigned long nr_pages, bool enable)
{
	struct memory_refresh_request mem_req;
	struct msm_rpm_kvp rpm_kvp;
	int ret;

	mem_req.start = enable;
	mem_req.start |= pfn << PAGE_SHIFT;
	mem_req.size = nr_pages * PAGE_SIZE;

	rpm_kvp.key = RPM_DDR_REQ;
	rpm_kvp.data = (void *)&mem_req;
	rpm_kvp.length = sizeof(mem_req);

	ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET, RPM_DDR_REQ, 0,
					&rpm_kvp, 1);
	if (ret)
		pr_err("PASR: Failed to send rpm message\n");
}

static int pasr_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	struct memory_notify *mn = arg;
	unsigned long start, end;

	start = SECTION_ALIGN_DOWN(mn->start_pfn);
	end = SECTION_ALIGN_UP(mn->start_pfn + mn->nr_pages);

	if ((start != mn->start_pfn) || (end != mn->start_pfn + mn->nr_pages)) {
		pr_err("PASR: %s pfn not aligned to section\n", __func__);
		pr_err("PASR: start pfn = %lu end pfn = %lu\n",
			mn->start_pfn, mn->start_pfn + mn->nr_pages);
		return -EINVAL;
	}

	switch (action) {
	case MEM_GOING_ONLINE:
		pr_debug("PASR: MEM_GOING_ONLINE : start = %lx end = %lx",
			mn->start_pfn << PAGE_SHIFT,
			(mn->start_pfn + mn->nr_pages) << PAGE_SHIFT);
		mem_region_refresh_control(mn->start_pfn, mn->nr_pages, true);
		break;
	case MEM_OFFLINE:
		pr_debug("PASR: MEM_OFFLINE: start = %lx end = %lx",
			mn->start_pfn << PAGE_SHIFT,
			(mn->start_pfn + mn->nr_pages) << PAGE_SHIFT);
		mem_region_refresh_control(mn->start_pfn, mn->nr_pages, false);
		break;
	case MEM_CANCEL_ONLINE:
		pr_debug("PASR: MEM_CANCEL_ONLINE: start = %lx end = %lx",
			mn->start_pfn << PAGE_SHIFT,
			(mn->start_pfn + mn->nr_pages) << PAGE_SHIFT);
		mem_region_refresh_control(mn->start_pfn, mn->nr_pages, false);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int __init pasr_module_init(void)
{
	return hotplug_memory_notifier(pasr_callback, 0);
}
late_initcall(pasr_module_init);
