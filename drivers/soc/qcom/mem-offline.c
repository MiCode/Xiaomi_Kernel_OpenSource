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
#include <linux/memblock.h>
#include <linux/mmzone.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>

#define AOP_MSG_ADDR_MASK		0xffffffff
#define AOP_MSG_ADDR_HIGH_SHIFT		32
#define MAX_LEN				96

static unsigned long start_section_nr, end_section_nr;
static struct kobject *kobj;
static unsigned int offline_granule, sections_per_block;
#define MODULE_CLASS_NAME	"mem-offline"
#define BUF_LEN			100

struct section_stat {
	unsigned long success_count;
	unsigned long fail_count;
	unsigned long avg_time;
	unsigned long best_time;
	unsigned long worst_time;
	unsigned long total_time;
	unsigned long last_recorded_time;
};

enum memory_states {
	MEMORY_ONLINE,
	MEMORY_OFFLINE,
	MAX_STATE,
};

static struct mem_offline_mailbox {
	struct mbox_client cl;
	struct mbox_chan *mbox;
} mailbox;

static struct section_stat *mem_info;

void record_stat(unsigned long sec, ktime_t delay, int mode)
{
	unsigned int total_sec = end_section_nr - start_section_nr + 1;
	unsigned int blk_nr = (sec - start_section_nr + mode * total_sec) /
				sections_per_block;

	if (sec > end_section_nr)
		return;

	if (delay < mem_info[blk_nr].best_time || !mem_info[blk_nr].best_time)
		mem_info[blk_nr].best_time = delay;

	if (delay > mem_info[blk_nr].worst_time)
		mem_info[blk_nr].worst_time = delay;

	++mem_info[blk_nr].success_count;
	if (mem_info[blk_nr].fail_count)
		--mem_info[blk_nr].fail_count;

	mem_info[blk_nr].total_time += delay;

	mem_info[blk_nr].avg_time =
		mem_info[blk_nr].total_time / mem_info[blk_nr].success_count;

	mem_info[blk_nr].last_recorded_time = delay;
}

static int aop_send_msg(unsigned long addr, bool online)
{
	struct qmp_pkt pkt;
	char mbox_msg[MAX_LEN];
	unsigned long addr_low, addr_high;

	addr_low = addr & AOP_MSG_ADDR_MASK;
	addr_high = (addr >> AOP_MSG_ADDR_HIGH_SHIFT) & AOP_MSG_ADDR_MASK;

	snprintf(mbox_msg, MAX_LEN,
		 "{class: ddr, event: pasr, addr_hi: 0x%08lx, addr_lo: 0x%08lx, refresh: %s}",
		 addr_high, addr_low, online ? "on" : "off");

	pkt.size = MAX_LEN;
	pkt.data = mbox_msg;
	return mbox_send_message(mailbox.mbox, &pkt);
}

static int mem_event_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	struct memory_notify *mn = arg;
	unsigned long start, end, sec_nr;
	static ktime_t cur;
	ktime_t delay = 0;
	phys_addr_t start_addr, end_addr;
	unsigned int idx = end_section_nr - start_section_nr + 1;

	start = SECTION_ALIGN_DOWN(mn->start_pfn);
	end = SECTION_ALIGN_UP(mn->start_pfn + mn->nr_pages);

	if ((start != mn->start_pfn) || (end != mn->start_pfn + mn->nr_pages)) {
		WARN("mem-offline: %s pfn not aligned to section\n", __func__);
		pr_err("mem-offline: start pfn = %lu end pfn = %lu\n",
			mn->start_pfn, mn->start_pfn + mn->nr_pages);
		return -EINVAL;
	}

	start_addr = __pfn_to_phys(start);
	end_addr = __pfn_to_phys(end);
	sec_nr = pfn_to_section_nr(start);

	if (sec_nr > end_section_nr || sec_nr < start_section_nr) {
		if (action == MEM_ONLINE || action == MEM_OFFLINE)
			pr_info("mem-offline: %s mem%d, but not our block. Not performing any action\n",
				action == MEM_ONLINE ? "Onlined" : "Offlined",
				sec_nr);
		return NOTIFY_OK;
	}
	switch (action) {
	case MEM_GOING_ONLINE:
		pr_debug("mem-offline: MEM_GOING_ONLINE : start = 0x%lx end = 0x%lx",
				start_addr, end_addr);
		++mem_info[(sec_nr - start_section_nr + MEMORY_ONLINE *
			   idx) / sections_per_block].fail_count;
		cur = ktime_get();

		if (aop_send_msg(__pfn_to_phys(start), true))
			pr_err("PASR: AOP online request addr:0x%llx failed\n",
			       __pfn_to_phys(start));

		break;
	case MEM_ONLINE:
		pr_info("mem-offline: Onlined memory block mem%lu\n", sec_nr);
		delay = ktime_ms_delta(ktime_get(), cur);
		record_stat(sec_nr, delay, MEMORY_ONLINE);
		cur = 0;
		break;
	case MEM_GOING_OFFLINE:
		pr_debug("mem-offline: MEM_GOING_OFFLINE : start = 0x%lx end = 0x%lx",
				start_addr, end_addr);
		++mem_info[(sec_nr - start_section_nr + MEMORY_OFFLINE *
			   idx) / sections_per_block].fail_count;
		cur = ktime_get();
		break;
	case MEM_OFFLINE:
		pr_info("mem-offline: Offlined memory block mem%lu\n", sec_nr);

		if (aop_send_msg(__pfn_to_phys(start), false))
			pr_err("PASR: AOP offline request addr:0x%llx failed\n",
			       __pfn_to_phys(start));

		delay = ktime_ms_delta(ktime_get(), cur);
		record_stat(sec_nr, delay, MEMORY_OFFLINE);
		cur = 0;
		break;
	case MEM_CANCEL_ONLINE:
		pr_info("mem-offline: MEM_CANCEL_ONLINE: start = 0x%lx end = 0x%lx",
				start_addr, end_addr);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static int mem_online_remaining_blocks(void)
{
	unsigned long memblock_end_pfn = __phys_to_pfn(memblock_end_of_DRAM());
	unsigned long ram_end_pfn = __phys_to_pfn(bootloader_memory_limit - 1);
	unsigned long block_size, memblock, pfn;
	unsigned int nid;
	phys_addr_t phys_addr;
	int fail = 0;

	block_size = memory_block_size_bytes();
	sections_per_block = block_size / MIN_MEMORY_BLOCK_SIZE;

	start_section_nr = pfn_to_section_nr(memblock_end_pfn);
	end_section_nr = pfn_to_section_nr(ram_end_pfn);

	if (start_section_nr >= end_section_nr) {
		pr_info("mem-offline: System booted with no zone movable memory blocks. Cannot perform memory offlining\n");
		return -EINVAL;
	}
	for (memblock = start_section_nr; memblock <= end_section_nr;
			memblock += sections_per_block) {
		pfn = section_nr_to_pfn(memblock);
		phys_addr = __pfn_to_phys(pfn);

		if (phys_addr & (((PAGES_PER_SECTION * sections_per_block)
					<< PAGE_SHIFT) - 1)) {
			fail = 1;
			pr_warn("mem-offline: PFN of mem%lu block not aligned to section start. Not adding this memory block\n",
								memblock);
			continue;
		}
		nid = memory_add_physaddr_to_nid(phys_addr);
		if (add_memory(nid, phys_addr,
				 MIN_MEMORY_BLOCK_SIZE * sections_per_block)) {
			pr_warn("mem-offline: Adding memory block mem%lu failed\n",
								memblock);
			fail = 1;
		}
	}
	return fail;
}

static ssize_t show_mem_offline_granule(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, BUF_LEN, "%lu\n", (unsigned long)offline_granule *
									SZ_1M);
}

static ssize_t show_mem_perf_stats(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{

	unsigned int blk_start = start_section_nr / sections_per_block;
	unsigned int blk_end = end_section_nr / sections_per_block;
	unsigned int idx = blk_end - blk_start + 1;
	unsigned int char_count = 0;
	unsigned int i, j;

	for (j = 0; j < MAX_STATE; j++) {
		char_count += snprintf(buf + char_count,  BUF_LEN,
			"\n\t%s\n\t\t\t", j == 0 ? "ONLINE" : "OFFLINE");
		for (i = blk_start; i <= blk_end; i++)
			char_count += snprintf(buf + char_count, BUF_LEN,
							"%s%d\t\t", "mem", i);
		char_count += snprintf(buf + char_count, BUF_LEN, "\n");
		char_count += snprintf(buf + char_count, BUF_LEN,
							"\tLast recd time:\t");
		for (i = 0; i <= blk_end - blk_start; i++)
			char_count += snprintf(buf + char_count, BUF_LEN,
			     "%lums\t\t", mem_info[i+j*idx].last_recorded_time);
		char_count += snprintf(buf + char_count, BUF_LEN, "\n");
		char_count += snprintf(buf + char_count, BUF_LEN,
							"\tAvg time:\t");
		for (i = 0; i <= blk_end - blk_start; i++)
			char_count += snprintf(buf + char_count, BUF_LEN,
				"%lums\t\t", mem_info[i+j*idx].avg_time);
		char_count += snprintf(buf + char_count, BUF_LEN, "\n");
		char_count += snprintf(buf + char_count, BUF_LEN,
							"\tBest time:\t");
		for (i = 0; i <= blk_end - blk_start; i++)
			char_count += snprintf(buf + char_count, BUF_LEN,
				"%lums\t\t", mem_info[i+j*idx].best_time);
		char_count += snprintf(buf + char_count, BUF_LEN, "\n");
		char_count += snprintf(buf + char_count, BUF_LEN,
							"\tWorst time:\t");
		for (i = 0; i <= blk_end - blk_start; i++)
			char_count += snprintf(buf + char_count, BUF_LEN,
				"%lums\t\t", mem_info[i+j*idx].worst_time);
		char_count += snprintf(buf + char_count, BUF_LEN, "\n");
		char_count += snprintf(buf + char_count, BUF_LEN,
							"\tSuccess count:\t");
		for (i = 0; i <= blk_end - blk_start; i++)
			char_count += snprintf(buf + char_count, BUF_LEN,
				"%lu\t\t", mem_info[i+j*idx].success_count);
		char_count += snprintf(buf + char_count, BUF_LEN, "\n");
		char_count += snprintf(buf + char_count, BUF_LEN,
							"\tFail count:\t");
		for (i = 0; i <= blk_end - blk_start; i++)
			char_count += snprintf(buf + char_count, BUF_LEN,
				"%lu\t\t", mem_info[i+j*idx].fail_count);
		char_count += snprintf(buf + char_count, BUF_LEN, "\n");
	}
	return char_count;
}

static struct kobj_attribute perf_stats_attr =
		__ATTR(perf_stats, 0444, show_mem_perf_stats, NULL);

static struct kobj_attribute offline_granule_attr =
		__ATTR(offline_granule, 0444, show_mem_offline_granule, NULL);

static struct attribute *mem_root_attrs[] = {
		&perf_stats_attr.attr,
		&offline_granule_attr.attr,
		NULL,
};

static struct attribute_group mem_attr_group = {
	.attrs = mem_root_attrs,
};

static int mem_sysfs_init(void)
{
	unsigned int total_blks = (end_section_nr - start_section_nr + 1) /
							sections_per_block;

	if (start_section_nr == end_section_nr)
		return -EINVAL;

	kobj = kobject_create_and_add(MODULE_CLASS_NAME, kernel_kobj);
	if (!kobj)
		return -ENOMEM;

	if (sysfs_create_group(kobj, &mem_attr_group))
		kobject_put(kobj);

	mem_info = kzalloc(sizeof(*mem_info) * total_blks * MAX_STATE,
								GFP_KERNEL);
	if (!mem_info)
		return -ENOMEM;

	return 0;
}

static int mem_parse_dt(struct platform_device *pdev)
{
	const unsigned int *val;
	struct device_node *node = pdev->dev.of_node;

	val = of_get_property(node, "granule", NULL);
	if (!val) {
		pr_err("mem-offine: granule property not found in DT\n");
		return -EINVAL;
	}
	if (!*val) {
		pr_err("mem-offine: invalid granule property\n");
		return -EINVAL;
	}
	offline_granule = be32_to_cpup(val);
	if (!offline_granule && !(offline_granule & (offline_granule - 1)) &&
			offline_granule * SZ_1M < MIN_MEMORY_BLOCK_SIZE) {
		pr_err("mem-offine: invalid granule property\n");
		return -EINVAL;
	}

	mailbox.cl.dev = &pdev->dev;
	mailbox.cl.tx_block = true;
	mailbox.cl.tx_tout = 1000;
	mailbox.cl.knows_txdone = false;

	mailbox.mbox = mbox_request_channel(&mailbox.cl, 0);
	if (IS_ERR(mailbox.mbox)) {
		pr_err("mem-offline: failed to get mailbox channel %pK %d\n",
			mailbox.mbox, PTR_ERR(mailbox.mbox));
		return PTR_ERR(mailbox.mbox);
	}

	return 0;
}

static struct notifier_block hotplug_memory_callback_nb = {
	.notifier_call = mem_event_callback,
	.priority = 0,
};

static int mem_offline_driver_probe(struct platform_device *pdev)
{
	int ret;

	if (mem_parse_dt(pdev))
		return -ENODEV;

	ret = mem_online_remaining_blocks();
	if (ret < 0)
		return -ENODEV;

	if (ret > 0)
		pr_err("mem-offline: !!ERROR!! Auto onlining some memory blocks failed. System could run with less RAM\n");

	if (mem_sysfs_init())
		return -ENODEV;

	if (register_hotmemory_notifier(&hotplug_memory_callback_nb)) {
		pr_err("mem-offline: Registering memory hotplug notifier failed\n");
		return -ENODEV;
	}
	pr_info("mem-offline: Added memory blocks ranging from mem%lu - mem%lu\n",
			start_section_nr, end_section_nr);
	return 0;
}

static const struct of_device_id mem_offline_match_table[] = {
	{.compatible = "qcom,mem-offline"},
	{}
};

static struct platform_driver mem_offline_driver = {
	.probe = mem_offline_driver_probe,
	.driver = {
		.name = "mem_offline",
		.of_match_table = mem_offline_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init mem_module_init(void)
{
	return platform_driver_register(&mem_offline_driver);
}

subsys_initcall(mem_module_init);
