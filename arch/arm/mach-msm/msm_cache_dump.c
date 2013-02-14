/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/memory_alloc.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <mach/scm.h>
#include <mach/msm_cache_dump.h>
#include <mach/memory.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memory_dump.h>

#define L2_DUMP_OFFSET 0x14

static unsigned long msm_cache_dump_addr;

/*
 * These should not actually be dereferenced. There's no
 * need for a virtual mapping, but the physical address is
 * necessary.
 */
static struct l1_cache_dump *l1_dump;
static struct l2_cache_dump *l2_dump;
static int use_imem_dump_offset;

static int msm_cache_dump_panic(struct notifier_block *this,
				unsigned long event, void *ptr)
{
#ifdef CONFIG_MSM_CACHE_DUMP_ON_PANIC
	/*
	 * Clear the bootloader magic so the dumps aren't overwritten
	 */
	if (use_imem_dump_offset)
		__raw_writel(0, MSM_IMEM_BASE + L2_DUMP_OFFSET);

	scm_call_atomic1(L1C_SERVICE_ID, CACHE_BUFFER_DUMP_COMMAND_ID, 2);
	scm_call_atomic1(L1C_SERVICE_ID, CACHE_BUFFER_DUMP_COMMAND_ID, 1);
#endif
	return 0;
}

static struct notifier_block msm_cache_dump_blk = {
	.notifier_call  = msm_cache_dump_panic,
	/*
	 * higher priority to ensure this runs before another panic handler
	 * flushes the caches.
	 */
	.priority = 1,
};

static int msm_cache_dump_probe(struct platform_device *pdev)
{
	struct msm_cache_dump_platform_data *d = pdev->dev.platform_data;
	struct msm_client_dump l1_dump_entry, l2_dump_entry;
	int ret;
	struct {
		unsigned long buf;
		unsigned long size;
	} l1_cache_data;
	void *temp;
	u32 l1_size, l2_size;
	unsigned long total_size;

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,l1-dump-size", &l1_size);
		if (ret)
			return ret;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,l2-dump-size", &l2_size);
		if (ret)
			return ret;

		use_imem_dump_offset = of_property_read_bool(pdev->dev.of_node,
						   "qcom,use-imem-dump-offset");
	} else {
		l1_size = d->l1_size;
		l2_size = d->l2_size;

		/* Non-DT targets assume the IMEM dump offset shall be used */
		use_imem_dump_offset = 1;
	};

	total_size = l1_size + l2_size;
	msm_cache_dump_addr = allocate_contiguous_ebi_nomap(total_size, SZ_4K);

	if (!msm_cache_dump_addr) {
		pr_err("%s: Could not get memory for cache dumping\n",
			__func__);
		return -ENOMEM;
	}

	temp = ioremap(msm_cache_dump_addr, total_size);
	memset(temp, 0xFF, total_size);
	iounmap(temp);

	l1_cache_data.buf = msm_cache_dump_addr;
	l1_cache_data.size = l1_size;

	ret = scm_call(L1C_SERVICE_ID, L1C_BUFFER_SET_COMMAND_ID,
			&l1_cache_data, sizeof(l1_cache_data), NULL, 0);

	if (ret)
		pr_err("%s: could not register L1 buffer ret = %d.\n",
			__func__, ret);

	l1_dump = (struct l1_cache_dump *)msm_cache_dump_addr;
	l2_dump = (struct l2_cache_dump *)(msm_cache_dump_addr + l1_size);

#if defined(CONFIG_MSM_CACHE_DUMP_ON_PANIC)
	l1_cache_data.buf = msm_cache_dump_addr + l1_size;
	l1_cache_data.size = l2_size;

	ret = scm_call(L1C_SERVICE_ID, L2C_BUFFER_SET_COMMAND_ID,
			&l1_cache_data, sizeof(l1_cache_data), NULL, 0);

	if (ret)
		pr_err("%s: could not register L2 buffer ret = %d.\n",
			__func__, ret);
#endif

	if (use_imem_dump_offset)
		__raw_writel(msm_cache_dump_addr + l1_size,
			MSM_IMEM_BASE + L2_DUMP_OFFSET);
	else {
		l1_dump_entry.id = MSM_L1_CACHE;
		l1_dump_entry.start_addr = msm_cache_dump_addr;
		l1_dump_entry.end_addr = l1_dump_entry.start_addr + l1_size - 1;

		l2_dump_entry.id = MSM_L2_CACHE;
		l2_dump_entry.start_addr = msm_cache_dump_addr + l1_size;
		l2_dump_entry.end_addr = l2_dump_entry.start_addr + l2_size - 1;

		ret = msm_dump_table_register(&l1_dump_entry);
		if (ret)
			pr_err("Could not register L1 dump area: %d\n", ret);

		ret = msm_dump_table_register(&l2_dump_entry);
		if (ret)
			pr_err("Could not register L2 dump area: %d\n", ret);
	}

	atomic_notifier_chain_register(&panic_notifier_list,
						&msm_cache_dump_blk);
	return 0;
}

static int msm_cache_dump_remove(struct platform_device *pdev)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
					&msm_cache_dump_blk);
	return 0;
}

static struct of_device_id cache_dump_match_table[] = {
	{	.compatible = "qcom,cache_dump",	},
	{}
};
EXPORT_COMPAT("qcom,cache_dump");

static struct platform_driver msm_cache_dump_driver = {
	.remove		= __devexit_p(msm_cache_dump_remove),
	.driver         = {
		.name = "msm_cache_dump",
		.owner = THIS_MODULE,
		.of_match_table = cache_dump_match_table,
	},
};

static int __init msm_cache_dump_init(void)
{
	return platform_driver_probe(&msm_cache_dump_driver,
					msm_cache_dump_probe);
}

static void __exit msm_cache_dump_exit(void)
{
	platform_driver_unregister(&msm_cache_dump_driver);
}
late_initcall(msm_cache_dump_init);
module_exit(msm_cache_dump_exit)
